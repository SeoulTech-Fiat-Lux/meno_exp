```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWidth = 960.f;
constexpr float kHeight = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kTargetTopY = 476.f; // 텍스처 높이 24, 아랫변 500
constexpr float kInterceptorSpeed = 600.f;
constexpr float kMaxRadius = 48.f;
constexpr int kAmmoPerBase = 10;

const std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
const std::array<float, 3> kBaseX{60.f, 480.f, 900.f};

struct Interceptor {
    meno::Vec2f start;
    meno::Vec2f pos;
    meno::Vec2f target;
};

struct Missile {
    meno::Vec2f start;
    meno::Vec2f pos;
    meno::Vec2f target;
    int targetIndex = 0; // 0~5 도시, 6~8 발사대
    float speed = 70.f;
};

struct Explosion {
    meno::Vec2f center;
    double age = 0.0;
};

float explosionRadius(double age) {
    if (age < 0.0) {
        return 0.f;
    }
    if (age < 0.5) {
        return static_cast<float>(age / 0.5) * kMaxRadius;
    }
    if (age < 1.0) {
        return static_cast<float>((1.0 - age) / 0.5) * kMaxRadius;
    }
    return 0.f;
}

float distance(meno::Vec2f a, meno::Vec2f b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

// pos를 target 쪽으로 step만큼 옮깁니다. 닿으면 true를 돌려줍니다.
bool advance(meno::Vec2f& pos, meno::Vec2f target, float step) {
    const float dx = target.x - pos.x;
    const float dy = target.y - pos.y;
    const float d = std::hypot(dx, dy);
    if (d <= step || d <= 0.0001f) {
        pos = target;
        return true;
    }
    pos += meno::Vec2f{dx, dy} * (step / d);
    return false;
}

class SoundPool {
public:
    void init(const std::shared_ptr<const meno::SoundBuffer>& buffer, int count) {
        sounds_.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            sounds_.emplace_back(buffer);
        }
    }
    void play() {
        if (sounds_.empty()) {
            return;
        }
        sounds_[next_].play();
        next_ = (next_ + 1) % sounds_.size();
    }

private:
    std::vector<meno::Sound> sounds_;
    std::size_t next_ = 0;
};

enum class Phase { Playing, Clearing, GameOver };

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SKY SHIELD",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& resources = game.resources();

    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto cityTex = resources.loadTexture("assets/textures/md_city.png");
    const auto baseTex = resources.loadTexture("assets/textures/md_base.png");
    const auto crossTex = resources.loadTexture("assets/textures/md_crosshair.png");
    const auto launchBuf = resources.loadSoundBuffer("assets/sfx/launch.wav");
    const auto explodeBuf = resources.loadSoundBuffer("assets/sfx/explode.wav");
    const auto loseBuf = resources.loadSoundBuffer("assets/sfx/lose.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_missile.ogg");

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(launchBuf != nullptr, "assets/sfx/launch.wav");
    require(explodeBuf != nullptr, "assets/sfx/explode.wav");
    require(loseBuf != nullptr, "assets/sfx/lose.wav");
    require(bgm.has_value(), "assets/music/bgm_missile.ogg");
    require(cityTex != nullptr, "assets/textures/md_city.png");
    require(baseTex != nullptr, "assets/textures/md_base.png");
    require(crossTex != nullptr, "assets/textures/md_crosshair.png");
    if (failed) {
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    SoundPool launchSfx;
    SoundPool explodeSfx;
    SoundPool loseSfx;
    launchSfx.init(launchBuf, 6);
    explodeSfx.init(explodeBuf, 12);
    loseSfx.init(loseBuf, 4);

    // 16:9 레터박스 카메라. 띠는 clearColor(검은색)로 채워집니다.
    const meno::Camera2D camera{.center = {kWidth / 2.f, kHeight / 2.f}, .size = {kWidth, kHeight}};
    game.renderer().setCamera(camera);

    const meno::Color groundColor = meno::Color::fromRgb(0xB07A2A);
    const meno::Color missileColor = meno::Color::fromRgb(0xFF4D4D);
    const meno::Color interceptorColor = meno::Color::fromRgb(0x4FC3F7);
    const meno::Color explosionAlt = meno::Color::fromRgb(0xFFD54A);
    const meno::Color white = meno::Color::fromRgb(0xFFFFFF);

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    std::array<bool, 6> cityAlive{};
    std::array<bool, 3> baseAlive{};
    std::array<int, 3> ammo{};
    std::vector<Interceptor> interceptors;
    std::vector<Missile> missiles;
    std::vector<Explosion> explosions;
    int score = 0;
    int wave = 1;
    int spawned = 0;
    double waveTime = 0.0;
    double clearTimer = 0.0;
    Phase phase = Phase::Playing;
    double titleTimer = 0.0;

    const auto missileCount = [&] { return 10 + 2 * (wave - 1); };
    const auto missileSpeed = [&] { return 70.f + 10.f * static_cast<float>(wave - 1); };
    const auto citiesLeft = [&] {
        return static_cast<int>(std::count(cityAlive.begin(), cityAlive.end(), true));
    };
    const auto targetPoint = [&](int index) {
        const float x = index < 6 ? kCityX[static_cast<std::size_t>(index)]
                                  : kBaseX[static_cast<std::size_t>(index - 6)];
        return meno::Vec2f{x, kTargetTopY};
    };

    const auto startWave = [&] {
        baseAlive.fill(true);
        ammo.fill(kAmmoPerBase);
        spawned = 0;
        waveTime = 0.0;
        phase = Phase::Playing;
    };

    const auto resetGame = [&] {
        cityAlive.fill(true);
        interceptors.clear();
        missiles.clear();
        explosions.clear();
        score = 0;
        wave = 1;
        clearTimer = 0.0;
        startWave();
    };

    const auto addExplosion = [&](meno::Vec2f center) {
        explosions.push_back({center, 0.0});
        explodeSfx.play();
    };

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text,
                                  meno::Vec2f pos, unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = white};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, pos, params);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---------------- 타이틀 ----------------
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawCentered(renderer, "SKY SHIELD", {kWidth / 2.f, 220.f}, 48);
        drawCentered(renderer, "PRESS ENTER", {kWidth / 2.f, 310.f}, 32);
    });

    // ---------------- 플레이 ----------------
    play.setEnterHandler([&] {
        resetGame();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDeltaTime);

        // 요격탄 발사
        if (phase == Phase::Playing && input.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse{input.mousePosition()};
            const meno::Vec2f click = game.renderer().screenToWorld(mouse);
            if (click.y < 470.f && click.y >= 0.f && click.x >= 0.f && click.x <= kWidth) {
                int best = -1;
                float bestDist = 0.f;
                for (int i = 0; i < 3; ++i) {
                    const auto idx = static_cast<std::size_t>(i);
                    if (!baseAlive[idx] || ammo[idx] <= 0) {
                        continue;
                    }
                    const float d = std::abs(kBaseX[idx] - click.x);
                    if (best < 0 || d < bestDist) {
                        best = i;
                        bestDist = d;
                    }
                }
                if (best >= 0) {
                    const auto idx = static_cast<std::size_t>(best);
                    --ammo[idx];
                    const meno::Vec2f start{kBaseX[idx], kTargetTopY};
                    interceptors.push_back({start, start, click});
                    launchSfx.play();
                }
            }
        }

        // 미사일 출발
        if (phase == Phase::Playing) {
            waveTime += fixedDeltaTime;
            while (citiesLeft() > 0 && spawned < missileCount() &&
                   waveTime >= 1.0 + 1.2 * static_cast<double>(spawned)) {
                std::vector<int> candidates;
                for (int i = 0; i < 6; ++i) {
                    if (cityAlive[static_cast<std::size_t>(i)]) {
                        candidates.push_back(i);
                    }
                }
                for (int i = 0; i < 3; ++i) {
                    if (baseAlive[static_cast<std::size_t>(i)]) {
                        candidates.push_back(6 + i);
                    }
                }
                std::uniform_real_distribution<float> randomX{0.f, kWidth};
                std::uniform_int_distribution<std::size_t> pick{0, candidates.size() - 1};
                const int target = candidates[pick(rng)];
                const meno::Vec2f start{randomX(rng), 0.f};
                missiles.push_back({start, start, targetPoint(target), target, missileSpeed()});
                ++spawned;
            }
        }

        // 폭발 시간 진행
        for (Explosion& e : explosions) {
            e.age += fixedDeltaTime;
        }
        std::erase_if(explosions, [](const Explosion& e) { return e.age >= 1.0; });

        // 요격탄 이동
        std::vector<meno::Vec2f> pending;
        std::erase_if(interceptors, [&](Interceptor& it) {
            if (advance(it.pos, it.target, kInterceptorSpeed * dt)) {
                pending.push_back(it.target);
                return true;
            }
            return false;
        });
        for (const meno::Vec2f p : pending) {
            addExplosion(p);
        }
        pending.clear();

        // 미사일 이동과 목표 도달
        std::erase_if(missiles, [&](Missile& m) {
            if (!advance(m.pos, m.target, m.speed * dt)) {
                return false;
            }
            pending.push_back(m.target);
            if (m.targetIndex < 6) {
                auto& alive = cityAlive[static_cast<std::size_t>(m.targetIndex)];
                if (alive) {
                    alive = false;
                    loseSfx.play();
                }
            } else {
                auto& alive = baseAlive[static_cast<std::size_t>(m.targetIndex - 6)];
                if (alive) {
                    alive = false;
                    loseSfx.play();
                }
            }
            return true;
        });
        for (const meno::Vec2f p : pending) {
            addExplosion(p);
        }
        pending.clear();

        // 폭발에 걸린 미사일 (미사일은 지워지므로 한 번만 셈)
        std::erase_if(missiles, [&](const Missile& m) {
            for (const Explosion& e : explosions) {
                const float r = explosionRadius(e.age);
                if (r > 0.f && distance(m.pos, e.center) <= r) {
                    score += 25;
                    pending.push_back(m.pos);
                    return true;
                }
            }
            return false;
        });
        for (const meno::Vec2f p : pending) {
            addExplosion(p);
        }

        // 게임 오버
        if (citiesLeft() == 0 && phase != Phase::GameOver) {
            phase = Phase::GameOver;
        }
        if (phase == Phase::GameOver) {
            if (explosions.empty()) {
                (void)scenes.changeScene(result.id());
            }
            return;
        }

        // 웨이브 정산
        if (phase == Phase::Playing) {
            if (spawned >= missileCount() && missiles.empty() && interceptors.empty() &&
                explosions.empty()) {
                for (std::size_t i = 0; i < 3; ++i) {
                    if (baseAlive[i]) {
                        score += ammo[i] * 5;
                    }
                }
                score += citiesLeft() * 100;
                phase = Phase::Clearing;
                clearTimer = 3.0;
            }
        } else if (phase == Phase::Clearing) {
            clearTimer -= fixedDeltaTime;
            if (clearTimer <= 0.0) {
                ++wave;
                startWave();
            }
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, kGroundY}, {kWidth, 40.f}}, {.fill = groundColor});

        for (std::size_t i = 0; i < 6; ++i) {
            if (cityAlive[i]) {
                renderer.draw(*cityTex, {.position = {kCityX[i] - 24.f, kTargetTopY}});
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if (baseAlive[i]) {
                renderer.draw(*baseTex, {.position = {kBaseX[i] - 24.f, kTargetTopY}});
                drawCentered(renderer, std::to_string(ammo[i]), {kBaseX[i], 520.f}, 16);
            }
        }

        for (const Missile& m : missiles) {
            renderer.drawLine(m.start, m.pos, {.color = missileColor, .thickness = 2.f});
            renderer.drawRect({{m.pos.x - 2.f, m.pos.y - 2.f}, {4.f, 4.f}}, {.fill = white});
        }
        for (const Interceptor& it : interceptors) {
            renderer.drawLine(it.start, it.pos, {.color = interceptorColor, .thickness = 2.f});
        }
        for (const Explosion& e : explosions) {
            const float r = explosionRadius(e.age);
            if (r <= 0.f) {
                continue;
            }
            const bool useWhite = static_cast<int>(e.age / 0.1) % 2 == 0;
            renderer.drawCircle(e.center, r,
                                {.fill = useWhite ? white : explosionAlt, .origin = {r, r}});
        }

        renderer.drawText(*font, "SCORE " + std::to_string(score), {16.f, 12.f},
                          {.characterSize = 32, .color = white});
        {
            const std::string waveText = "WAVE " + std::to_string(wave);
            meno::TextParams params{.characterSize = 32, .color = white};
            const meno::Rectf bounds = renderer.measureTextBounds(*font, waveText, params);
            renderer.drawText(*font, waveText, {kWidth - 16.f - bounds.size.x - bounds.position.x, 12.f},
                              params);
        }
        if (phase == Phase::Clearing) {
            drawCentered(renderer, "WAVE " + std::to_string(wave) + " CLEAR",
                         {kWidth / 2.f, kHeight / 2.f - 40.f}, 48);
        }

        const meno::Vec2f mouse{game.window().input().mousePosition()};
        const meno::Vec2f aim = renderer.screenToWorld(mouse);
        renderer.draw(*crossTex, {.position = {aim.x - 12.f, aim.y - 12.f}});
    });

    // ---------------- 결과 ----------------
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawCentered(renderer, "GAME OVER", {kWidth / 2.f, 170.f}, 48);
        drawCentered(renderer, "SCORE " + std::to_string(score), {kWidth / 2.f, 250.f}, 32);
        drawCentered(renderer, "WAVE " + std::to_string(wave), {kWidth / 2.f, 300.f}, 32);
        drawCentered(renderer, "R: RESTART  ESC: QUIT", {kWidth / 2.f, 380.f}, 32);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```

참고: 폭발 원은 `drawCircle`의 원점이 중심이라는 설명이 있지만, 혹시 원점이 왼쪽 위 기준으로 동작하는 경우에 대비해 `origin = {r, r}`을 지정했습니다. 만약 빌드 후 폭발이 클릭 지점에서 반지름만큼 어긋난다면 그 `.origin = {r, r}` 부분을 지우면 됩니다.