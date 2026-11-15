// 스프라이트 N개를 그리는 프레임 시간을 네 경로로 잰다 (SeoulTech-Fiat-Lux/Meno#30).
//
//   sfml-batched   SFML을 직접, 배칭: 사각형 N개를 sf::VertexArray 하나에 모아 draw를 한 번만 부름
//                  (배칭을 했을 때의 하한선. sfml과의 차이가 "배칭으로 얻을 수 있는 최대 이득"이다)
//   sfml           SFML을 직접: sf::Sprite N개에 위치·회전을 넣고 draw
//   meno-renderer  meno Game 루프 + draw 핸들러에서 Renderer::draw를 N번
//   meno-object    meno Game 루프 + GameObject N개 (사용자가 실제로 쓰는 경로)
//
// 사용: meno_bench --target <대상> --count N --warmup W --frames F --out <csv> [--seed S] [--loop run|direct]
//       [--screenshot <png>]  (sfml 대상만: 마지막 프레임을 저장해 두 SFML 대상이 같은 그림인지 확인한다)
// 출력: 머리글 frame,frame_ms와 F줄. 표준 오류에 frames·updates 요약 한 줄.
//
// 공정하게 만드는 규칙 (benchmark/README.md의 "실행 파일 약속"):
// - 네 대상은 같은 텍스처, 창 크기, 시드로 정한 초기 상태를 쓰고, 프레임마다 같은 이동 함수 step()을 한 번 부른다.
// - meno의 Application은 고정 시간 간격으로 update()를 부르므로, fixedTimeStep을 아주 작게 하고
//   maxUpdatesPerFrame = 1로 두어 프레임마다 정확히 한 번 갱신되게 한다. 이동은 meno의 dt가 아니라 kDt를 쓴다.
// - 프레임 시간은 프레임마다 같은 지점에서 찍은 시각의 간격이다(SFML은 display() 뒤, meno는 draw 핸들러).
//
// --loop direct: Game::run() 대신 그것이 하는 일을 같은 순서의 공개 API 호출로 직접 한다. Tracy 구간을
// 호출 사이에 넣기 위한 것이다(Meno 코드를 고치지 않고 단계별 비용을 보려고). 측정 수치는 --loop run으로 낸다.
#include <meno/Meno.hpp>

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#ifdef BENCH_ENABLE_TRACY
#include <tracy/Tracy.hpp>
#define BENCH_ZONE(name) ZoneScopedN(name)
#define BENCH_FRAME() FrameMark
#else
#define BENCH_ZONE(name)
#define BENCH_FRAME()
#endif

namespace {

constexpr unsigned kWidth = 1280;
constexpr unsigned kHeight = 720;
constexpr unsigned kTexture = 32;
constexpr float kHalf = kTexture / 2.f;
constexpr float kDt = 1.f / 60.f;  // 이동 계산의 고정 dt. 실제 프레임 시간과 무관하게 같은 일을 한다.

struct Options {
    std::string target;
    int count = 1000;
    int warmup = 120;
    int frames = 1000;
    std::string out;
    std::uint32_t seed = 1;
    bool directLoop = false;
    std::string screenshot;
};

// --- 공통 상태와 이동 --------------------------------------------------------

struct Body {
    float x, y, vx, vy, angle, spin;
};

std::vector<Body> makeBodies(int count, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> px(kHalf, kWidth - kHalf), py(kHalf, kHeight - kHalf);
    std::uniform_real_distribution<float> speed(-200.f, 200.f), angle(0.f, 360.f), spin(-180.f, 180.f);
    std::vector<Body> bodies(static_cast<std::size_t>(count));
    for (auto& b : bodies) {
        b = {px(rng), py(rng), speed(rng), speed(rng), angle(rng), spin(rng)};
    }
    return bodies;
}

// 네 대상이 똑같이 부르는 이동: 등속 이동, 창 경계에서 튕김, 일정한 회전.
void step(std::vector<Body>& bodies) {
    BENCH_ZONE("bench.move");
    for (auto& b : bodies) {
        b.x += b.vx * kDt;
        b.y += b.vy * kDt;
        if (b.x < kHalf || b.x > kWidth - kHalf) {
            b.vx = -b.vx;
            b.x = std::clamp(b.x, kHalf, kWidth - kHalf);
        }
        if (b.y < kHalf || b.y > kHeight - kHalf) {
            b.vy = -b.vy;
            b.y = std::clamp(b.y, kHalf, kHeight - kHalf);
        }
        b.angle = std::fmod(b.angle + b.spin * kDt + 360.f, 360.f);
    }
}

// 두 라이브러리가 같은 픽셀로 텍스처를 만든다: 테두리가 밝은 사각형.
std::vector<std::uint8_t> texturePixels() {
    std::vector<std::uint8_t> rgba(kTexture * kTexture * 4);
    for (unsigned y = 0; y < kTexture; ++y) {
        for (unsigned x = 0; x < kTexture; ++x) {
            const bool edge = x < 2 || y < 2 || x >= kTexture - 2 || y >= kTexture - 2;
            std::uint8_t* p = &rgba[(y * kTexture + x) * 4];
            p[0] = edge ? 255 : static_cast<std::uint8_t>(40 + x * 6);
            p[1] = edge ? 255 : static_cast<std::uint8_t>(80 + y * 5);
            p[2] = 200;
            p[3] = 255;
        }
    }
    return rgba;
}

// --- 프레임 시간 기록 ---------------------------------------------------------

class FrameRecorder {
public:
    explicit FrameRecorder(const Options& o) : warmup_(o.warmup), frames_(o.frames) {
        times_.reserve(static_cast<std::size_t>(frames_));
    }

    // 프레임마다 같은 지점에서 한 번 부른다. 기록이 끝나면 true를 돌려준다.
    bool tick() {
        const auto now = std::chrono::steady_clock::now();
        if (started_) {
            if (intervals_++ >= warmup_) {
                times_.push_back(std::chrono::duration<double, std::milli>(now - previous_).count());
            }
        }
        started_ = true;
        previous_ = now;
        return done();
    }

    [[nodiscard]] bool done() const { return static_cast<int>(times_.size()) >= frames_; }

    void countUpdate() { ++updates_; }

    bool write(const std::string& path) const {
        std::ofstream out(path);
        out << "frame,frame_ms\n";
        for (std::size_t i = 0; i < times_.size(); ++i) {
            out << i << ',' << times_[i] << '\n';
        }
        // 갱신이 프레임마다 정확히 한 번이었는지 확인할 수 있게 남긴다(공정성 점검).
        std::fprintf(stderr, "frames=%d updates=%d recorded=%zu\n", intervals_ + 1, updates_, times_.size());
        return static_cast<bool>(out);
    }

private:
    int warmup_, frames_;
    int intervals_ = 0, updates_ = 0;
    bool started_ = false;
    std::chrono::steady_clock::time_point previous_{};
    std::vector<double> times_;
};

// --- 대상: SFML 직접 ----------------------------------------------------------

void saveScreenshot(const sf::RenderWindow& window, const std::string& path) {
    if (path.empty()) return;
    sf::Texture shot(window.getSize());
    shot.update(window);
    if (!shot.copyToImage().saveToFile(path)) std::fprintf(stderr, "화면을 저장하지 못했다: %s\n", path.c_str());
}

int runSfml(std::vector<Body>& bodies, FrameRecorder& rec, const std::string& screenshot) {
    sf::RenderWindow window(sf::VideoMode({kWidth, kHeight}), "meno_bench sfml");
    window.setFramerateLimit(0);
    window.setVerticalSyncEnabled(false);

    sf::Texture texture({kTexture, kTexture});
    texture.update(texturePixels().data());
    std::vector<sf::Sprite> sprites(bodies.size(), sf::Sprite(texture));
    for (auto& s : sprites) {
        s.setOrigin({kHalf, kHalf});
    }

    while (window.isOpen() && !rec.done()) {
        BENCH_ZONE("frame");
        {
            BENCH_ZONE("app.events");
            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();
            }
        }
        {
            BENCH_ZONE("app.update");
            step(bodies);
            rec.countUpdate();
            {
                BENCH_ZONE("bench.apply");  // 한 블록에 구간은 하나만 열 수 있다(Tracy 매크로가 같은 변수 이름을 씀)
                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    sprites[i].setPosition({bodies[i].x, bodies[i].y});
                    sprites[i].setRotation(sf::degrees(bodies[i].angle));
                }
            }
        }
        {
            BENCH_ZONE("renderer.begin");
            window.clear();
        }
        {
            BENCH_ZONE("scene.draw");
            for (const auto& s : sprites) window.draw(s);
        }
        {
            BENCH_ZONE("window.display");
            window.display();
        }
        BENCH_FRAME();
        if (rec.tick()) saveScreenshot(window, screenshot);  // display() 뒤라 화면에 보인 마지막 프레임이다
    }
    return rec.done() ? 0 : 1;
}

// 배칭: 스프라이트마다 draw를 부르는 대신, 회전한 사각형 N개의 꼭짓점(삼각형 2개 = 6개)을 CPU에서 계산해
// VertexArray 하나에 넣고 draw를 한 번 부른다. 텍스처가 하나라 상태 전환이 없다. sf::Sprite와 같은 그림이 나온다
// (기준점은 텍스처 중심, 회전은 시계 방향 도 단위).
int runSfmlBatched(std::vector<Body>& bodies, FrameRecorder& rec, const std::string& screenshot) {
    sf::RenderWindow window(sf::VideoMode({kWidth, kHeight}), "meno_bench sfml-batched");
    window.setFramerateLimit(0);
    window.setVerticalSyncEnabled(false);

    sf::Texture texture({kTexture, kTexture});
    texture.update(texturePixels().data());
    sf::VertexArray vertices(sf::PrimitiveType::Triangles, bodies.size() * 6);
    sf::RenderStates states;
    states.texture = &texture;

    constexpr float t = static_cast<float>(kTexture);
    const sf::Vector2f corner[4] = {{-kHalf, -kHalf}, {kHalf, -kHalf}, {kHalf, kHalf}, {-kHalf, kHalf}};
    const sf::Vector2f uv[4] = {{0.f, 0.f}, {t, 0.f}, {t, t}, {0.f, t}};
    constexpr int order[6] = {0, 1, 2, 0, 2, 3};

    while (window.isOpen() && !rec.done()) {
        BENCH_ZONE("frame");
        {
            BENCH_ZONE("app.events");
            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();
            }
        }
        {
            BENCH_ZONE("app.update");
            step(bodies);
            rec.countUpdate();
            {
                BENCH_ZONE("bench.apply");
                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    const float radians = bodies[i].angle * 3.14159265358979f / 180.f;
                    const float c = std::cos(radians), sn = std::sin(radians);
                    for (int k = 0; k < 6; ++k) {
                        const sf::Vector2f p = corner[order[k]];
                        sf::Vertex& v = vertices[i * 6 + static_cast<std::size_t>(k)];
                        v.position = {bodies[i].x + p.x * c - p.y * sn, bodies[i].y + p.x * sn + p.y * c};
                        v.texCoords = uv[order[k]];
                    }
                }
            }
        }
        {
            BENCH_ZONE("renderer.begin");
            window.clear();
        }
        {
            BENCH_ZONE("scene.draw");
            window.draw(vertices, states);
        }
        {
            BENCH_ZONE("window.display");
            window.display();
        }
        BENCH_FRAME();
        if (rec.tick()) saveScreenshot(window, screenshot);
    }
    return rec.done() ? 0 : 1;
}

// --- 대상: meno ---------------------------------------------------------------

meno::GameConfig menoConfig(const char* title) {
    // 프레임마다 정확히 한 번 update가 돌게 한다(위 설명). framerateLimit 0 = 제한 없음.
    return {.application = {.fixedTimeStep = 1e-6, .maxUpdatesPerFrame = 1, .framerateLimit = 0},
            .windowSize = {kWidth, kHeight},
            .title = title};
}

// Game::run()이 프레임마다 하는 일(Application::run의 processEvents → update → render, Game::render)을
// 같은 순서의 공개 API 호출로 한다. 차이는 Clock과 meno::Time 갱신이 없다는 것뿐이고, 둘 다 meno 내부
// 동작에는 쓰이지 않는다(Time은 Application만 쓰고, 읽는 것은 사용자 코드뿐이다).
void runDirectLoop(meno::Game& game, FrameRecorder& rec) {
    meno::Renderer& renderer = game.renderer();
    while (!rec.done()) {
        BENCH_ZONE("frame");
        {
            BENCH_ZONE("app.events");
            game.window().pollEvents();
            if (!game.window().isOpen()) break;
        }
        {
            BENCH_ZONE("app.update");
            game.scenes().update(kDt);
        }
        {
            BENCH_ZONE("renderer.begin");
            renderer.beginFrame(meno::colors::Black);
        }
        {
            BENCH_ZONE("scene.draw");
            const std::optional<meno::Camera2D> camera = renderer.camera();
            game.scenes().draw(renderer);
            if (camera) renderer.setCamera(*camera);
            else renderer.resetCamera();
        }
        {
            BENCH_ZONE("window.display");
            renderer.endFrame();
        }
        BENCH_FRAME();
    }
    (void)game.scenes().shutdown();
}

int runMeno(const Options& o, std::vector<Body>& bodies, FrameRecorder& rec, bool objects) {
    meno::Game game{menoConfig(objects ? "meno_bench meno-object" : "meno_bench meno-renderer")};
    const auto pixels = texturePixels();
    auto made = meno::Texture::fromPixels({kTexture, kTexture}, pixels.data());
    if (!made) {
        std::fprintf(stderr, "텍스처를 만들지 못했다\n");
        return 1;
    }
    const auto texture = std::make_shared<const meno::Texture>(std::move(*made));

    meno::Scene& scene = game.scenes().createScene();
    std::vector<meno::GameObject*> objectsList;
    if (objects) {
        objectsList.reserve(bodies.size());
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            meno::GameObject& obj = scene.createGameObject();
            obj.sprite().setTexture(texture);  // 텍스처 하나를 공유한다(Texture&&로 넘기면 객체마다 따로 생김)
            obj.sprite().origin = {kHalf, kHalf};
            objectsList.push_back(&obj);
        }
    }

    scene.setUpdateHandler([&](double) {
        step(bodies);
        rec.countUpdate();
        if (objects) {
            BENCH_ZONE("bench.apply");
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                meno::Transform& t = objectsList[i]->transform();
                t.position = {bodies[i].x, bodies[i].y};
                t.rotation = bodies[i].angle;
            }
        }
    });
    scene.setDrawHandler([&](meno::Renderer& renderer) {
        if (!objects) {
            for (const auto& b : bodies) {
                renderer.draw(*texture, meno::SpriteParams{.position = {b.x, b.y}, .origin = {kHalf, kHalf},
                                                           .rotation = b.angle});
            }
        }
        // draw 핸들러는 프레임마다 한 번, display() 직전에 불린다. 연속한 두 호출의 간격이 한 프레임이다.
        if (rec.tick()) game.stop();
    });
    if (!game.scenes().changeScene(scene.id())) {
        std::fprintf(stderr, "장면을 바꾸지 못했다\n");
        return 1;
    }

    if (o.directLoop) runDirectLoop(game, rec);
    else game.run();
    return rec.done() ? 0 : 1;
}

// --- 인자 ---------------------------------------------------------------------

std::optional<Options> parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string_view key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--target") o.target = value;
        else if (key == "--count") o.count = std::atoi(value.c_str());
        else if (key == "--warmup") o.warmup = std::atoi(value.c_str());
        else if (key == "--frames") o.frames = std::atoi(value.c_str());
        else if (key == "--out") o.out = value;
        else if (key == "--seed") o.seed = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (key == "--loop") o.directLoop = value == "direct";
        else if (key == "--screenshot") o.screenshot = value;
        else return std::nullopt;
    }
    const bool known = o.target == "sfml-batched" || o.target == "sfml" || o.target == "meno-renderer" ||
                       o.target == "meno-object";
    if (!known || o.count < 1 || o.warmup < 0 || o.frames < 1 || o.out.empty()) return std::nullopt;
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = parse(argc, argv);
    if (!options) {
        std::fprintf(stderr, "사용: meno_bench --target sfml-batched|sfml|meno-renderer|meno-object --count N --warmup W "
                             "--frames F --out <csv> [--seed S] [--loop run|direct]\n");
        return 2;
    }
    const Options& o = *options;
    auto bodies = makeBodies(o.count, o.seed);
    FrameRecorder rec(o);

    int code = 0;
    if (o.target == "sfml-batched") code = runSfmlBatched(bodies, rec, o.screenshot);
    else if (o.target == "sfml") code = runSfml(bodies, rec, o.screenshot);
    else code = runMeno(o, bodies, rec, o.target == "meno-object");

    if (!rec.write(o.out)) {
        std::fprintf(stderr, "출력 파일을 쓰지 못했다: %s\n", o.out.c_str());
        return 1;
    }
    return code;
}
