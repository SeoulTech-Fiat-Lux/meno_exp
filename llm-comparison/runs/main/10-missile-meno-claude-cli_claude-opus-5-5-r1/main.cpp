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
constexpr float kTargetY = 476.f; // 도시·발사대 위쪽 가운데의 y
constexpr float kClickLimitY = 470.f;
constexpr float kInterceptorSpeed = 600.f;
constexpr float kExplosionMax = 48.f;
constexpr double kExplosionHalf = 0.5;
constexpr double kExplosionLife = 1.0;

constexpr std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr std::array<float, 3> kBaseX{60.f, 480.f, 900.f};

struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;
    void play() {
        if (sounds.empty()) {
            return;
        }
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

struct Missile {
    meno::Vec2f start;
    meno::Vec2f pos;
    meno::Vec2f target;
    int targetIndex = 0; // 0~5 도시, 6~8 발사대
    float speed = 70.f;
};

struct Interceptor {
    meno::Vec2f start;
    meno::Vec2f pos;
    meno::Vec2f target;
};

struct Explosion {
    meno::Vec2f center;
    double age = 0.0;
};

struct State {
    std::array<bool, 6> cityAlive{};
    std::array<bool, 3> baseAlive{};
    std::array<int, 3> ammo{};
    int score = 0;
    int wave = 1;
    int missilesTotal = 0;
    int missilesSpawned = 0;
    double spawnTimer = 0.0;
    double clearTimer = 0.0;
    bool clearing = false;
    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
};

float length(meno::Vec2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float explosionRadius(double age) {
    if (age < kExplosionHalf) {
        return static_cast<float>(age / kExplosionHalf) * kExplosionMax;
    }
    const double t = (age - kExplosionHalf) / kExplosionHalf;
    return std::max(0.f, static_cast<float>(1.0 - t) * kExplosionMax);
}

int citiesLeft(const State& s) {
    return static_cast<int>(std::count(s.cityAlive.begin(), s.cityAlive.end(), true));
}

void startWave(State& s) {
    s.baseAlive.fill(true);
    s.ammo.fill(10);
    s.missilesTotal = 10 + 2 * (s.wave - 1);
    s.missilesSpawned = 0;
    s.spawnTimer = 1.0;
    s.clearing = false;
    s.clearTimer = 0.0;
    s.missiles.clear();
    s.interceptors.clear();
    s.explosions.clear();
}

void resetGame(State& s) {
    s.cityAlive.fill(true);
    s.score = 0;
    s.wave = 1;
    startWave(s);
}

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

    std::vector<std::string> missing;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            missing.emplace_back(path);
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
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    SoundPool launchSfx;
    SoundPool explodeSfx;
    SoundPool loseSfx;
    for (int i = 0; i < 4; ++i) {
        launchSfx.sounds.emplace_back(launchBuf);
        loseSfx.sounds.emplace_back(loseBuf);
    }
    for (int i = 0; i < 8; ++i) {
        explodeSfx.sounds.emplace_back(explodeBuf);
    }

    // 960×540 게임 화면을 16:9 레터박스로 비춥니다.
    const meno::Camera2D camera{.center = {kWidth / 2.f, kHeight / 2.f}, .size = {kWidth, kHeight}};
    game.renderer().setCamera(camera);

    std::mt19937 rng{std::random_device{}()};
    State state;
    resetGame(state);
    double titleTimer = 0.0;

    const meno::Color groundColor = meno::Color::fromRgb(0xB07A2A);
    const meno::Color missileColor = meno::Color::fromRgb(0xFF4D4D);
    const meno::Color interceptorColor = meno::Color::fromRgb(0x4FC3F7);
    const meno::Color explosionAlt = meno::Color::fromRgb(0xFFD54A);

    const auto drawCentered = [&](meno::Renderer& r, const std::string& text, meno::Vec2f pos,
                                  unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = meno::colors::White};
        params.origin = r.measureTextBounds(*font, text, params).center();
        r.drawText(*font, text, pos, params);
    };

    const auto mouseWorld = [&]() {
        const meno::Vec2f mouse{game.window().input().mousePosition()};
        return game.renderer().screenToWorld(mouse);
    };

    const auto addExplosion = [&](meno::Vec2f at) {
        state.explosions.push_back({at, 0.0});
        explodeSfx.play();
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---------- 타이틀 ----------
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
    title.setDrawHandler([&](meno::Renderer& r) {
        drawCentered(r, "SKY SHIELD", {kWidth / 2.f, 220.f}, 48);
        drawCentered(r, "PRESS ENTER", {kWidth / 2.f, 300.f}, 32);
    });

    // ---------- 플레이 ----------
    play.setEnterHandler([&] {
        resetGame(state);
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double fixedDt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDt);
        const bool gameOver = citiesLeft(state) == 0;

        // 요격탄 발사
        if (input.wasPressed(meno::MouseButton::Left) && !state.clearing && !gameOver) {
            const meno::Vec2f p = mouseWorld();
            if (p.y < kClickLimitY && p.y >= 0.f && p.x >= 0.f && p.x <= kWidth) {
                int best = -1;
                float bestDist = 0.f;
                for (int i = 0; i < 3; ++i) {
                    if (!state.baseAlive[static_cast<std::size_t>(i)] ||
                        state.ammo[static_cast<std::size_t>(i)] <= 0) {
                        continue;
                    }
                    const float d = std::abs(kBaseX[static_cast<std::size_t>(i)] - p.x);
                    if (best < 0 || d < bestDist) {
                        best = i;
                        bestDist = d;
                    }
                }
                if (best >= 0) {
                    const auto b = static_cast<std::size_t>(best);
                    --state.ammo[b];
                    const meno::Vec2f start{kBaseX[b], kTargetY};
                    state.interceptors.push_back({start, start, p});
                    launchSfx.play();
                }
            }
        }

        // 적 미사일 출발
        if (!state.clearing && !gameOver && state.missilesSpawned < state.missilesTotal) {
            state.spawnTimer -= fixedDt;
            while (state.spawnTimer <= 0.0 && state.missilesSpawned < state.missilesTotal) {
                std::vector<int> targets;
                for (int i = 0; i < 6; ++i) {
                    if (state.cityAlive[static_cast<std::size_t>(i)]) {
                        targets.push_back(i);
                    }
                }
                for (int i = 0; i < 3; ++i) {
                    if (state.baseAlive[static_cast<std::size_t>(i)]) {
                        targets.push_back(6 + i);
                    }
                }
                if (targets.empty()) {
                    break;
                }
                std::uniform_real_distribution<float> randomX{0.f, kWidth};
                std::uniform_int_distribution<std::size_t> pick{0, targets.size() - 1};
                const int t = targets[pick(rng)];
                const float tx = t < 6 ? kCityX[static_cast<std::size_t>(t)]
                                       : kBaseX[static_cast<std::size_t>(t - 6)];
                const meno::Vec2f start{randomX(rng), 0.f};
                state.missiles.push_back({start, start, {tx, kTargetY}, t,
                                          70.f + 10.f * static_cast<float>(state.wave - 1)});
                ++state.missilesSpawned;
                state.spawnTimer += 1.2;
            }
        }

        // 요격탄 이동
        for (auto it = state.interceptors.begin(); it != state.interceptors.end();) {
            const meno::Vec2f delta = it->target - it->pos;
            const float dist = length(delta);
            const float step = kInterceptorSpeed * dt;
            if (dist <= step) {
                const meno::Vec2f at = it->target;
                it = state.interceptors.erase(it);
                addExplosion(at);
            } else {
                it->pos += delta * (step / dist);
                ++it;
            }
        }

        // 적 미사일 이동
        for (auto it = state.missiles.begin(); it != state.missiles.end();) {
            const meno::Vec2f delta = it->target - it->pos;
            const float dist = length(delta);
            const float step = it->speed * dt;
            if (dist <= step) {
                const meno::Vec2f at = it->target;
                const int t = it->targetIndex;
                it = state.missiles.erase(it);
                addExplosion(at);
                bool destroyed = false;
                if (t < 6) {
                    auto& alive = state.cityAlive[static_cast<std::size_t>(t)];
                    destroyed = alive;
                    alive = false;
                } else {
                    auto& alive = state.baseAlive[static_cast<std::size_t>(t - 6)];
                    destroyed = alive;
                    alive = false;
                }
                if (destroyed) {
                    loseSfx.play();
                }
            } else {
                it->pos += delta * (step / dist);
                ++it;
            }
        }

        // 폭발 진행
        for (Explosion& e : state.explosions) {
            e.age += fixedDt;
        }
        std::erase_if(state.explosions,
                      [](const Explosion& e) { return e.age >= kExplosionLife; });

        // 폭발에 닿은 미사일 (연쇄)
        std::vector<meno::Vec2f> newBlasts;
        std::erase_if(state.missiles, [&](const Missile& m) {
            for (const Explosion& e : state.explosions) {
                const float r = explosionRadius(e.age);
                if (r > 0.f && length(m.pos - e.center) < r) {
                    newBlasts.push_back(m.pos);
                    return true;
                }
            }
            return false;
        });
        for (const meno::Vec2f at : newBlasts) {
            state.score += 25;
            addExplosion(at);
        }

        // 웨이브 진행
        if (citiesLeft(state) == 0) {
            if (state.explosions.empty()) {
                (void)scenes.changeScene(result.id());
            }
            return;
        }
        if (state.clearing) {
            state.clearTimer -= fixedDt;
            if (state.clearTimer <= 0.0) {
                ++state.wave;
                startWave(state);
            }
        } else if (state.missilesSpawned >= state.missilesTotal && state.missiles.empty() &&
                   state.interceptors.empty() && state.explosions.empty()) {
            int bonus = 0;
            for (std::size_t i = 0; i < 3; ++i) {
                if (state.baseAlive[i]) {
                    bonus += state.ammo[i] * 5;
                }
            }
            bonus += citiesLeft(state) * 100;
            state.score += bonus;
            state.clearing = true;
            state.clearTimer = 3.0;
        }
    });
    play.setDrawHandler([&](meno::Renderer& r) {
        r.drawRect({{0.f, kGroundY}, {kWidth, 40.f}}, {.fill = groundColor});

        for (std::size_t i = 0; i < 6; ++i) {
            if (state.cityAlive[i]) {
                r.draw(*cityTex, {.position = {kCityX[i] - 24.f, kGroundY - 24.f}});
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if (state.baseAlive[i]) {
                r.draw(*baseTex, {.position = {kBaseX[i] - 24.f, kGroundY - 24.f}});
                drawCentered(r, std::to_string(state.ammo[i]), {kBaseX[i], 520.f}, 16);
            }
        }

        for (const Missile& m : state.missiles) {
            r.drawLine(m.start, m.pos, {.color = missileColor, .thickness = 2.f});
            r.drawRect({{m.pos.x - 2.f, m.pos.y - 2.f}, {4.f, 4.f}}, {.fill = meno::colors::White});
        }
        for (const Interceptor& in : state.interceptors) {
            r.drawLine(in.start, in.pos, {.color = interceptorColor, .thickness = 2.f});
        }
        for (const Explosion& e : state.explosions) {
            const float radius = explosionRadius(e.age);
            if (radius <= 0.f) {
                continue;
            }
            const bool alt = static_cast<int>(e.age / 0.1) % 2 == 1;
            r.drawCircle(e.center, radius, {.fill = alt ? explosionAlt : meno::colors::White});
        }

        const meno::TextParams hud{.characterSize = 32, .color = meno::colors::White};
        r.drawText(*font, "SCORE " + std::to_string(state.score), {16.f, 12.f}, hud);
        {
            const std::string waveText = "WAVE " + std::to_string(state.wave);
            const meno::Rectf b = r.measureTextBounds(*font, waveText, hud);
            r.drawText(*font, waveText, {kWidth - 16.f - b.size.x - b.position.x, 12.f}, hud);
        }
        if (state.clearing) {
            drawCentered(r, "WAVE " + std::to_string(state.wave) + " CLEAR",
                         {kWidth / 2.f, kHeight / 2.f - 40.f}, 48);
        }

        const meno::Vec2f m = mouseWorld();
        r.draw(*crossTex, {.position = {m.x - 12.f, m.y - 12.f}});
    });

    // ---------- 결과 ----------
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
    result.setDrawHandler([&](meno::Renderer& r) {
        drawCentered(r, "GAME OVER", {kWidth / 2.f, 170.f}, 48);
        drawCentered(r, "SCORE " + std::to_string(state.score), {kWidth / 2.f, 250.f}, 32);
        drawCentered(r, "WAVE " + std::to_string(state.wave), {kWidth / 2.f, 300.f}, 32);
        drawCentered(r, "R: RESTART  ESC: QUIT", {kWidth / 2.f, 380.f}, 32);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
