```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kCityY = 476.f;      // 도시·발사대 윗변 (높이 24, 아랫변 y 500)
constexpr float kFireLimitY = 470.f; // 이 값보다 아래(y가 큰 쪽) 클릭은 무시
constexpr float kInterceptorSpeed = 600.f;
constexpr float kExplosionLife = 1.f;
constexpr float kExplosionMaxRadius = 48.f;
constexpr int kAmmoPerBase = 10;
constexpr double kWaveClearTime = 3.0;

struct City {
    float x = 0.f;
    bool alive = true;
};

struct Base {
    float x = 0.f;
    bool alive = true;
    int ammo = kAmmoPerBase;
};

struct Missile {
    meno::Vec2f start{};
    meno::Vec2f head{};
    meno::Vec2f target{};
    float speed = 0.f;
    int targetCity = -1;
    int targetBase = -1;
};

struct Interceptor {
    meno::Vec2f origin{};
    meno::Vec2f head{};
    meno::Vec2f target{};
};

struct Explosion {
    meno::Vec2f center{};
    float age = 0.f;

    // 0~0.5초 동안 0에서 48px로 커지고, 다음 0.5초 동안 0으로 줄어든 뒤 사라집니다.
    float radius() const {
        if (age >= kExplosionLife) {
            return 0.f;
        }
        constexpr float half = kExplosionLife / 2.f;
        const float t = age <= half ? age / half : (kExplosionLife - age) / half;
        return kExplosionMaxRadius * t;
    }
};

enum class Phase { Active, Clear };

struct Target {
    meno::Vec2f point{};
    int city = -1;
    int base = -1;
};

struct World {
    std::vector<City> cities;
    std::vector<Base> bases;
    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
    int score = 0;
    int wave = 1;
    int spawned = 0;
    int total = 0;
    double waveTime = 0.0;
    double clearTimer = 0.0;
    Phase phase = Phase::Active;
    bool gameOver = false;
    std::mt19937 rng{std::random_device{}()};
};

float distance(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

void playNext(std::vector<meno::Sound>& pool, std::size_t& next) {
    pool[next].play();
    next = (next + 1) % pool.size();
}

void startWave(World& w) {
    w.phase = Phase::Active;
    w.waveTime = 0.0;
    w.clearTimer = 0.0;
    w.spawned = 0;
    w.total = 10 + 2 * (w.wave - 1);
    for (Base& b : w.bases) {
        b.alive = true;
        b.ammo = kAmmoPerBase;
    }
}

void resetWorld(World& w) {
    const float cityX[] = {160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
    const float baseX[] = {60.f, 480.f, 900.f};
    w.cities.clear();
    for (float x : cityX) {
        w.cities.push_back(City{x, true});
    }
    w.bases.clear();
    for (float x : baseX) {
        w.bases.push_back(Base{x, true, kAmmoPerBase});
    }
    w.missiles.clear();
    w.interceptors.clear();
    w.explosions.clear();
    w.score = 0;
    w.wave = 1;
    w.gameOver = false;
    startWave(w);
}

void spawnMissile(World& w) {
    std::vector<Target> targets;
    for (int i = 0; i < static_cast<int>(w.cities.size()); ++i) {
        if (w.cities[static_cast<std::size_t>(i)].alive) {
            const meno::Vec2f p{w.cities[static_cast<std::size_t>(i)].x, kCityY};
            targets.push_back(Target{p, i, -1});
        }
    }
    for (int i = 0; i < static_cast<int>(w.bases.size()); ++i) {
        if (w.bases[static_cast<std::size_t>(i)].alive) {
            const meno::Vec2f p{w.bases[static_cast<std::size_t>(i)].x, kCityY};
            targets.push_back(Target{p, -1, i});
        }
    }
    if (targets.empty()) {
        return;
    }
    std::uniform_int_distribution<std::size_t> pick(0, targets.size() - 1);
    std::uniform_real_distribution<float> startX(0.f, 960.f);
    const Target& t = targets[pick(w.rng)];

    Missile m;
    m.start = meno::Vec2f{startX(w.rng), 0.f};
    m.head = m.start;
    m.target = t.point;
    m.speed = 70.f + 10.f * static_cast<float>(w.wave - 1);
    m.targetCity = t.city;
    m.targetBase = t.base;
    w.missiles.push_back(m);
    ++w.spawned;
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SKY SHIELD",
                     .clearColor = meno::colors::Black}};
    const meno::Camera2D worldCamera{.center = {480.f, 270.f}, .size = {960.f, 540.f}};

    // 에셋을 모두 읽고, 실패한 경로를 표준 에러에 알립니다.
    meno::ResourceManager& res = game.resources();
    std::vector<std::string> missing;
    const auto require = [&missing](auto asset, const char* path) {
        if (!asset) {
            missing.emplace_back(path);
        }
        return asset;
    };
    const auto font = require(res.loadFont("assets/fonts/game.ttf"), "assets/fonts/game.ttf");
    const auto cityTex =
        require(res.loadTexture("assets/textures/md_city.png"), "assets/textures/md_city.png");
    const auto baseTex =
        require(res.loadTexture("assets/textures/md_base.png"), "assets/textures/md_base.png");
    const auto crosshairTex = require(res.loadTexture("assets/textures/md_crosshair.png"),
                                      "assets/textures/md_crosshair.png");
    const auto launchBuf = require(res.loadSoundBuffer("assets/sfx/launch.wav"),
                                   "assets/sfx/launch.wav");
    const auto explodeBuf = require(res.loadSoundBuffer("assets/sfx/explode.wav"),
                                    "assets/sfx/explode.wav");
    const auto loseBuf = require(res.loadSoundBuffer("assets/sfx/lose.wav"),
                                 "assets/sfx/lose.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_missile.ogg");
    if (!bgm) {
        missing.emplace_back("assets/music/bgm_missile.ogg");
    }
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing asset: " << path << '\n';
        }
        return 1;
    }
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    // 같은 효과음이 겹칠 수 있도록 Sound를 여러 개 둡니다.
    std::vector<meno::Sound> launchSounds;
    std::vector<meno::Sound> explodeSounds;
    std::vector<meno::Sound> loseSounds;
    for (int i = 0; i < 4; ++i) {
        launchSounds.emplace_back(launchBuf);
    }
    for (int i = 0; i < 6; ++i) {
        explodeSounds.emplace_back(explodeBuf);
    }
    for (int i = 0; i < 2; ++i) {
        loseSounds.emplace_back(loseBuf);
    }
    std::size_t launchNext = 0;
    std::size_t explodeNext = 0;
    std::size_t loseNext = 0;

    World w;
    double titleTimer = 0.0;
    meno::Vec2f mouseWorld{0.f, 0.f};

    const auto centerText = [&](meno::Renderer& r, const std::string& text, meno::Vec2f pos,
                                meno::TextParams params) {
        params.origin = r.measureTextBounds(*font, text, params).center();
        r.drawText(*font, text, pos, params);
    };

    const auto addExplosion = [&](meno::Vec2f center) {
        w.explosions.push_back(Explosion{center, 0.f});
        playNext(explodeSounds, explodeNext);
    };

    const auto fire = [&](meno::Vec2f target) {
        if (target.y >= kFireLimitY) {
            return;
        }
        int best = -1;
        float bestDist = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(w.bases.size()); ++i) {
            const Base& b = w.bases[static_cast<std::size_t>(i)];
            if (!b.alive || b.ammo <= 0) {
                continue;
            }
            const float d = std::abs(b.x - target.x);
            if (d < bestDist) { // 같은 거리면 먼저 나온(왼쪽) 발사대를 택합니다.
                bestDist = d;
                best = i;
            }
        }
        if (best < 0) {
            return;
        }
        Base& b = w.bases[static_cast<std::size_t>(best)];
        --b.ammo;
        const meno::Vec2f origin{b.x, kCityY};
        w.interceptors.push_back(Interceptor{origin, origin, target});
        playNext(launchSounds, launchNext);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& titleScene = scenes.createScene();
    meno::Scene& playScene = scenes.createScene();
    meno::Scene& resultScene = scenes.createScene();

    // 타이틀: Enter를 누르거나 2초가 지나면 플레이를 시작합니다.
    titleScene.setEnterHandler([&] { titleTimer = 0.0; });
    titleScene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(playScene.id());
        }
    });
    titleScene.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(worldCamera);
        centerText(r, "SKY SHIELD", {480.f, 200.f},
                   {.characterSize = 48, .color = meno::colors::White});
        centerText(r, "PRESS ENTER", {480.f, 300.f},
                   {.characterSize = 32, .color = meno::colors::White});
    });

    // 플레이: 들어올 때마다 모든 상태를 초기화합니다.
    playScene.setEnterHandler([&] {
        resetWorld(w);
        bgm->stop();
        bgm->play();
    });
    playScene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float fdt = static_cast<float>(dt);
        mouseWorld = meno::screenToWorld(worldCamera, game.renderer().framebufferSize(),
                                         meno::Vec2f{input.mousePosition()});

        if (w.phase == Phase::Clear) {
            w.clearTimer += dt;
            if (w.clearTimer >= kWaveClearTime) {
                ++w.wave;
                startWave(w);
            }
            return;
        }

        // 도시가 모두 부서지면 게임 오버입니다. 이후 새 미사일은 내보내지 않습니다.
        const bool anyCity = std::any_of(w.cities.begin(), w.cities.end(),
                                         [](const City& c) { return c.alive; });
        if (!anyCity) {
            w.gameOver = true;
        }

        w.waveTime += dt;
        if (!w.gameOver && w.spawned < w.total &&
            w.waveTime >= 1.0 + 1.2 * static_cast<double>(w.spawned)) {
            spawnMissile(w);
        }

        if (input.wasPressed(meno::MouseButton::Left)) {
            fire(mouseWorld);
        }

        // 요격탄 이동. 목표에 닿으면 폭발을 만듭니다.
        for (std::size_t i = 0; i < w.interceptors.size();) {
            Interceptor& it = w.interceptors[i];
            const float dist = distance(it.head, it.target);
            const float step = kInterceptorSpeed * fdt;
            if (dist <= step) {
                const meno::Vec2f at = it.target;
                w.interceptors.erase(w.interceptors.begin() + static_cast<std::ptrdiff_t>(i));
                addExplosion(at);
            } else {
                it.head += (it.target - it.head) * (step / dist);
                ++i;
            }
        }

        // 적 미사일 이동. 목표에 닿으면 목표를 부수고 폭발을 만듭니다.
        for (std::size_t i = 0; i < w.missiles.size();) {
            Missile& m = w.missiles[i];
            const float dist = distance(m.head, m.target);
            const float step = m.speed * fdt;
            if (dist <= step) {
                const meno::Vec2f at = m.target;
                m.head = m.target;
                bool destroyed = false;
                if (m.targetCity >= 0 &&
                    w.cities[static_cast<std::size_t>(m.targetCity)].alive) {
                    w.cities[static_cast<std::size_t>(m.targetCity)].alive = false;
                    destroyed = true;
                } else if (m.targetBase >= 0 &&
                           w.bases[static_cast<std::size_t>(m.targetBase)].alive) {
                    w.bases[static_cast<std::size_t>(m.targetBase)].alive = false;
                    destroyed = true;
                }
                if (destroyed) {
                    playNext(loseSounds, loseNext);
                }
                w.missiles.erase(w.missiles.begin() + static_cast<std::ptrdiff_t>(i));
                addExplosion(at);
            } else {
                m.head += (m.target - m.head) * (step / dist);
                ++i;
            }
        }

        // 폭발 나이 갱신.
        for (Explosion& e : w.explosions) {
            e.age += fdt;
        }

        // 연쇄: 폭발 원 안에 들어온 미사일을 없애고 25점. 새 폭발도 같은 규칙을 따릅니다.
        for (std::size_t e = 0; e < w.explosions.size(); ++e) {
            const meno::Vec2f center = w.explosions[e].center;
            const float radius = w.explosions[e].radius();
            for (std::size_t i = 0; i < w.missiles.size();) {
                if (distance(w.missiles[i].head, center) <= radius) {
                    const meno::Vec2f at = w.missiles[i].head;
                    w.missiles.erase(w.missiles.begin() + static_cast<std::ptrdiff_t>(i));
                    w.score += 25;
                    addExplosion(at);
                } else {
                    ++i;
                }
            }
        }
        std::erase_if(w.explosions,
                      [](const Explosion& e) { return e.age >= kExplosionLife; });

        if (w.gameOver) {
            if (w.missiles.empty() && w.interceptors.empty() && w.explosions.empty()) {
                (void)scenes.changeScene(resultScene.id());
            }
            return;
        }

        // 웨이브 정산: 모든 미사일이 출발하고 화면이 비면 끝납니다.
        if (w.spawned == w.total && w.missiles.empty() && w.interceptors.empty() &&
            w.explosions.empty()) {
            int ammo = 0;
            for (const Base& b : w.bases) {
                if (b.alive) {
                    ammo += b.ammo;
                }
            }
            int cities = 0;
            for (const City& c : w.cities) {
                if (c.alive) {
                    ++cities;
                }
            }
            w.score += 5 * ammo + 100 * cities;
            w.phase = Phase::Clear;
            w.clearTimer = 0.0;
        }
    });
    playScene.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(worldCamera);

        r.drawRect({{0.f, 500.f}, {960.f, 40.f}}, {.fill = meno::Color::fromRgb(0xB07A2A)});

        for (const City& c : w.cities) {
            if (c.alive) {
                r.draw(*cityTex, {.position = {c.x - 24.f, kCityY}});
            }
        }
        for (const Base& b : w.bases) {
            if (!b.alive) {
                continue;
            }
            r.draw(*baseTex, {.position = {b.x - 24.f, kCityY}});
            centerText(r, std::to_string(b.ammo), {b.x, 520.f},
                       {.characterSize = 16, .color = meno::colors::White});
        }

        for (const Interceptor& it : w.interceptors) {
            r.drawLine(it.origin, it.head, {.color = meno::Color::fromRgb(0x4FC3F7), .thickness = 2.f});
        }
        for (const Missile& m : w.missiles) {
            r.drawLine(m.start, m.head, {.color = meno::Color::fromRgb(0xFF4D4D), .thickness = 2.f});
            r.drawRect({m.head - meno::Vec2f{2.f, 2.f}, {4.f, 4.f}},
                       {.fill = meno::colors::White});
        }

        for (const Explosion& e : w.explosions) {
            const float radius = e.radius();
            if (radius <= 0.f) {
                continue;
            }
            const bool white = static_cast<int>(e.age / 0.1f) % 2 == 0;
            r.drawCircle(e.center, radius,
                         {.fill = white ? meno::colors::White : meno::Color::fromRgb(0xFFD54A)});
        }

        r.drawText(*font, "SCORE " + std::to_string(w.score), {16.f, 12.f},
                   {.characterSize = 16, .color = meno::colors::White});
        centerText(r, "WAVE " + std::to_string(w.wave), {480.f, 20.f},
                   {.characterSize = 16, .color = meno::colors::White});

        if (w.phase == Phase::Clear) {
            centerText(r, "WAVE " + std::to_string(w.wave) + " CLEAR", {480.f, 240.f},
                       {.characterSize = 32, .color = meno::colors::White});
        }

        r.draw(*crosshairTex, {.position = {mouseWorld.x - 12.f, mouseWorld.y - 12.f}});
    });

    // 결과: GAME OVER와 최종 점수를 보여 줍니다. R은 플레이를 다시 시작합니다.
    resultScene.setEnterHandler([&] { bgm->pause(); });
    resultScene.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(playScene.id());
        }
    });
    resultScene.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(worldCamera);
        centerText(r, "GAME OVER", {480.f, 180.f},
                   {.characterSize = 48, .color = meno::colors::White});
        centerText(r, "SCORE " + std::to_string(w.score), {480.f, 260.f},
                   {.characterSize = 32, .color = meno::colors::White});
        centerText(r, "WAVE " + std::to_string(w.wave), {480.f, 310.f},
                   {.characterSize = 32, .color = meno::colors::White});
        centerText(r, "R: RESTART  ESC: QUIT", {480.f, 480.f},
                   {.characterSize = 16, .color = meno::colors::White});
    });

    (void)scenes.changeScene(titleScene.id());
    game.run();
}
```