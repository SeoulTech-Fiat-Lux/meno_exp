```cpp
#include <meno/Meno.hpp>

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

constexpr float kGroundY = 500.f;
constexpr float kGroundH = 40.f;
constexpr float kObjW = 48.f;
constexpr float kObjH = 24.f;
constexpr std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr std::array<float, 3> kBaseX{60.f, 480.f, 900.f};

struct Missile {
    meno::Vec2f start;
    meno::Vec2f pos;
    meno::Vec2f target;
    float speed = 70.f;
    int targetIndex = 0; // 0~5 도시, 6~8 발사대
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

float explosionRadius(double age) {
    if (age < 0.0) {
        return 0.f;
    }
    if (age <= 0.5) {
        return static_cast<float>(age / 0.5 * 48.0);
    }
    if (age < 1.0) {
        return static_cast<float>((1.0 - (age - 0.5) / 0.5) * 48.0);
    }
    return 0.f;
}

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

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SKY SHIELD",
                     .clearColor = meno::colors::Black}};

    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto launchBuf = resources.loadSoundBuffer("assets/sfx/launch.wav");
    const auto explodeBuf = resources.loadSoundBuffer("assets/sfx/explode.wav");
    const auto loseBuf = resources.loadSoundBuffer("assets/sfx/lose.wav");
    const auto cityTex = resources.loadTexture("assets/textures/md_city.png");
    const auto baseTex = resources.loadTexture("assets/textures/md_base.png");
    const auto crossTex = resources.loadTexture("assets/textures/md_crosshair.png");
    std::optional<meno::Music> music = meno::Music::openFromFile("assets/music/bgm_missile.ogg");

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
    require(music.has_value(), "assets/music/bgm_missile.ogg");
    require(cityTex != nullptr, "assets/textures/md_city.png");
    require(baseTex != nullptr, "assets/textures/md_base.png");
    require(crossTex != nullptr, "assets/textures/md_crosshair.png");
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    music->setLooping(true);
    music->setVolume(0.4f);

    SoundPool launchSfx;
    SoundPool explodeSfx;
    SoundPool loseSfx;
    for (int i = 0; i < 8; ++i) {
        launchSfx.sounds.emplace_back(launchBuf);
        explodeSfx.sounds.emplace_back(explodeBuf);
        loseSfx.sounds.emplace_back(loseBuf);
    }

    // 16:9 레터박스 카메라
    const meno::Camera2D camera{.center = {480.f, 270.f}, .size = {960.f, 540.f}};
    game.renderer().setCamera(camera);

    // 게임 상태
    std::mt19937 rng{std::random_device{}()};
    int score = 0;
    int wave = 1;
    std::array<bool, 6> cityAlive{};
    std::array<bool, 3> baseAlive{};
    std::array<int, 3> ammo{};
    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
    int spawned = 0;
    double spawnTimer = 0.0;
    double clearTimer = 0.0;
    bool inClear = false;
    double titleTimer = 0.0;

    const auto waveTotal = [&] { return 10 + 2 * (wave - 1); };
    const auto waveSpeed = [&] { return 70.f + 10.f * static_cast<float>(wave - 1); };
    const auto allCitiesDead = [&] {
        for (bool alive : cityAlive) {
            if (alive) {
                return false;
            }
        }
        return true;
    };
    const auto startWave = [&] {
        baseAlive.fill(true);
        ammo.fill(10);
        spawned = 0;
        spawnTimer = 1.0;
        clearTimer = 0.0;
        inClear = false;
        missiles.clear();
        interceptors.clear();
    };
    const auto newGame = [&] {
        score = 0;
        wave = 1;
        cityAlive.fill(true);
        explosions.clear();
        startWave();
    };
    const auto addExplosion = [&](meno::Vec2f at) {
        explosions.push_back({at, 0.0});
        explodeSfx.play();
    };
    const auto targetPoint = [](int index) {
        const float x = index < 6 ? kCityX[static_cast<std::size_t>(index)]
                                  : kBaseX[static_cast<std::size_t>(index - 6)];
        return meno::Vec2f{x, kGroundY - kObjH};
    };

    // 그리기 도우미
    const auto drawTextAt = [&](meno::Renderer& r, const std::string& s, meno::Vec2f pos, int size,
                                bool centered) {
        meno::TextParams params{};
        params.characterSize = static_cast<decltype(params.characterSize)>(size);
        params.color = meno::colors::White;
        if (centered) {
            params.origin = r.measureTextBounds(*font, s, params).center();
        }
        r.drawText(*font, s, pos, params);
    };
    const auto drawCrosshair = [&](meno::Renderer& r) {
        const meno::Vec2f mouse{game.window().input().mousePosition()};
        const meno::Vec2f p = r.screenToWorld(mouse);
        r.draw(*crossTex, {.position = {p.x - 12.f, p.y - 12.f}});
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // 타이틀
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += fixedDeltaTime;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        drawTextAt(r, "SKY SHIELD", {480.f, 220.f}, 48, true);
        drawTextAt(r, "PRESS ENTER", {480.f, 320.f}, 32, true);
        drawCrosshair(r);
    });

    // 플레이
    play.setEnterHandler([&] {
        newGame();
        music->play();
    });
    play.setExitHandler([&] { music->stop(); });
    play.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDeltaTime);
        const bool over = allCitiesDead();

        // 웨이브 정산 표시 중
        if (inClear) {
            clearTimer -= fixedDeltaTime;
            if (clearTimer <= 0.0) {
                ++wave;
                startWave();
            }
        }

        // 요격탄 발사
        if (!inClear && input.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse{input.mousePosition()};
            const meno::Vec2f p = game.renderer().screenToWorld(mouse);
            if (p.y < 470.f && p.y >= 0.f && p.x >= 0.f && p.x <= 960.f) {
                int best = -1;
                float bestDist = 0.f;
                for (int i = 0; i < 3; ++i) {
                    const auto ui = static_cast<std::size_t>(i);
                    if (!baseAlive[ui] || ammo[ui] <= 0) {
                        continue;
                    }
                    const float d = std::abs(kBaseX[ui] - p.x);
                    if (best < 0 || d < bestDist) {
                        best = i;
                        bestDist = d;
                    }
                }
                if (best >= 0) {
                    const auto ub = static_cast<std::size_t>(best);
                    --ammo[ub];
                    const meno::Vec2f start{kBaseX[ub], kGroundY - kObjH};
                    interceptors.push_back({start, start, p});
                    launchSfx.play();
                }
            }
        }

        // 미사일 출발
        if (!inClear && !over && spawned < waveTotal()) {
            spawnTimer -= fixedDeltaTime;
            while (spawnTimer <= 0.0 && spawned < waveTotal()) {
                spawnTimer += 1.2;
                std::vector<int> targets;
                for (int i = 0; i < 6; ++i) {
                    if (cityAlive[static_cast<std::size_t>(i)]) {
                        targets.push_back(i);
                    }
                }
                for (int i = 0; i < 3; ++i) {
                    if (baseAlive[static_cast<std::size_t>(i)]) {
                        targets.push_back(6 + i);
                    }
                }
                if (targets.empty()) {
                    break;
                }
                std::uniform_real_distribution<float> randX{0.f, 960.f};
                std::uniform_int_distribution<std::size_t> pick{0, targets.size() - 1};
                const int t = targets[pick(rng)];
                const meno::Vec2f start{randX(rng), 0.f};
                missiles.push_back({start, start, targetPoint(t), waveSpeed(), t});
                ++spawned;
            }
        }

        // 요격탄 이동
        for (std::size_t i = 0; i < interceptors.size();) {
            Interceptor& it = interceptors[i];
            const float dx = it.target.x - it.pos.x;
            const float dy = it.target.y - it.pos.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float step = 600.f * dt;
            if (d <= step) {
                const meno::Vec2f at = it.target;
                interceptors.erase(interceptors.begin() + static_cast<std::ptrdiff_t>(i));
                addExplosion(at);
            } else {
                it.pos.x += dx / d * step;
                it.pos.y += dy / d * step;
                ++i;
            }
        }

        // 미사일 이동
        for (std::size_t i = 0; i < missiles.size();) {
            Missile& m = missiles[i];
            const float dx = m.target.x - m.pos.x;
            const float dy = m.target.y - m.pos.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float step = m.speed * dt;
            if (d <= step) {
                const meno::Vec2f at = m.target;
                const int t = m.targetIndex;
                missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(i));
                addExplosion(at);
                bool& alive = t < 6 ? cityAlive[static_cast<std::size_t>(t)]
                                    : baseAlive[static_cast<std::size_t>(t - 6)];
                if (alive) {
                    alive = false;
                    loseSfx.play();
                }
            } else {
                m.pos.x += dx / d * step;
                m.pos.y += dy / d * step;
                ++i;
            }
        }

        // 폭발 시간 진행
        for (Explosion& e : explosions) {
            e.age += fixedDeltaTime;
        }
        std::erase_if(explosions, [](const Explosion& e) { return e.age >= 1.0; });

        // 미사일과 폭발 판정 (연쇄)
        for (std::size_t i = 0; i < missiles.size();) {
            const meno::Vec2f head = missiles[i].pos;
            bool hit = false;
            const std::size_t count = explosions.size();
            for (std::size_t j = 0; j < count; ++j) {
                const float r = explosionRadius(explosions[j].age);
                if (r <= 0.f) {
                    continue;
                }
                const float dx = head.x - explosions[j].center.x;
                const float dy = head.y - explosions[j].center.y;
                if (dx * dx + dy * dy < r * r) {
                    hit = true;
                    break;
                }
            }
            if (hit) {
                missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(i));
                score += 25;
                addExplosion(head);
            } else {
                ++i;
            }
        }

        // 종료와 정산
        if (allCitiesDead()) {
            if (explosions.empty() && interceptors.empty()) {
                (void)scenes.changeScene(result.id());
            }
        } else if (!inClear && spawned >= waveTotal() && missiles.empty() &&
                   interceptors.empty() && explosions.empty()) {
            for (std::size_t b = 0; b < 3; ++b) {
                if (baseAlive[b]) {
                    score += ammo[b] * 5;
                }
            }
            for (bool alive : cityAlive) {
                if (alive) {
                    score += 100;
                }
            }
            inClear = true;
            clearTimer = 3.0;
        }
    });
    play.setDrawHandler([&](meno::Renderer& r) {
        r.drawRect({{0.f, kGroundY}, {960.f, kGroundH}}, {.fill = meno::Color::fromRgb(0xB07A2A)});
        for (std::size_t i = 0; i < 6; ++i) {
            if (cityAlive[i]) {
                r.draw(*cityTex, {.position = {kCityX[i] - kObjW / 2.f, kGroundY - kObjH}});
            }
        }
        for (std::size_t i = 0; i < 3; ++i) {
            if (baseAlive[i]) {
                r.draw(*baseTex, {.position = {kBaseX[i] - kObjW / 2.f, kGroundY - kObjH}});
                drawTextAt(r, std::to_string(ammo[i]), {kBaseX[i], kGroundY + 20.f}, 16, true);
            }
        }
        const meno::Color red = meno::Color::fromRgb(0xFF4D4D);
        for (const Missile& m : missiles) {
            r.drawLine(m.start, m.pos, {.color = red, .thickness = 2.f});
            r.drawRect({{m.pos.x - 2.f, m.pos.y - 2.f}, {4.f, 4.f}}, {.fill = meno::colors::White});
        }
        const meno::Color blue = meno::Color::fromRgb(0x4FC3F7);
        for (const Interceptor& it : interceptors) {
            r.drawLine(it.start, it.pos, {.color = blue, .thickness = 2.f});
        }
        const meno::Color yellow = meno::Color::fromRgb(0xFFD54A);
        for (const Explosion& e : explosions) {
            const float radius = explosionRadius(e.age);
            if (radius <= 0.f) {
                continue;
            }
            const bool alt = (static_cast<int>(e.age / 0.1) % 2) == 1;
            r.drawCircle(e.center, radius, {.fill = alt ? yellow : meno::colors::White,
                                            .origin = {radius, radius}});
        }
        drawTextAt(r, "SCORE " + std::to_string(score), {16.f, 16.f}, 16, false);
        const std::string waveText = "WAVE " + std::to_string(wave);
        drawTextAt(r, waveText, {880.f, 24.f}, 16, true);
        if (inClear) {
            drawTextAt(r, "WAVE " + std::to_string(wave) + " CLEAR", {480.f, 240.f}, 32, true);
        }
        drawCrosshair(r);
    });

    // 결과
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
        drawTextAt(r, "GAME OVER", {480.f, 180.f}, 48, true);
        drawTextAt(r, "SCORE " + std::to_string(score), {480.f, 260.f}, 32, true);
        drawTextAt(r, "WAVE " + std::to_string(wave), {480.f, 310.f}, 32, true);
        drawTextAt(r, "R: RESTART  ESC: QUIT", {480.f, 400.f}, 16, true);
        drawCrosshair(r);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```