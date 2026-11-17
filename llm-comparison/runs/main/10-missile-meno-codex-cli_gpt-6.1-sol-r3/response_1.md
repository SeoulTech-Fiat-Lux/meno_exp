```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
using Vec = meno::Vec2f;

struct Target {
    Vec position;
    bool alive = true;
    int ammunition = 10;
};

struct Missile {
    Vec origin;
    Vec position;
    Vec destination;
    int target = 0;
};

struct Interceptor {
    Vec origin;
    Vec position;
    Vec destination;
};

struct Explosion {
    Vec position;
    double age = 0.0;
};

enum class Screen { Title, Play, Result };

float distance(Vec a, Vec b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool advance(Vec& position, Vec destination, float step) {
    const float remaining = distance(position, destination);
    if (remaining <= step) {
        position = destination;
        return true;
    }
    position += (destination - position) * (step / remaining);
    return false;
}

float radius(const Explosion& explosion) {
    return static_cast<float>(
        96.0 * std::max(0.0, std::min(explosion.age, 1.0 - explosion.age)));
}
} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SKY SHIELD",
                     .clearColor = meno::colors::Black}};

    auto& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto cityTexture = resources.loadTexture("assets/textures/md_city.png");
    const auto baseTexture = resources.loadTexture("assets/textures/md_base.png");
    const auto crosshairTexture =
        resources.loadTexture("assets/textures/md_crosshair.png");
    const auto launchBuffer = resources.loadSoundBuffer("assets/sfx/launch.wav");
    const auto explodeBuffer = resources.loadSoundBuffer("assets/sfx/explode.wav");
    const auto loseBuffer = resources.loadSoundBuffer("assets/sfx/lose.wav");
    auto music = meno::Music::openFromFile("assets/music/bgm_missile.ogg");

    bool missing = false;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            missing = true;
        }
    };
    require(static_cast<bool>(font), "assets/fonts/game.ttf");
    require(static_cast<bool>(cityTexture), "assets/textures/md_city.png");
    require(static_cast<bool>(baseTexture), "assets/textures/md_base.png");
    require(static_cast<bool>(crosshairTexture), "assets/textures/md_crosshair.png");
    require(static_cast<bool>(launchBuffer), "assets/sfx/launch.wav");
    require(static_cast<bool>(explodeBuffer), "assets/sfx/explode.wav");
    require(static_cast<bool>(loseBuffer), "assets/sfx/lose.wav");
    require(music.has_value(), "assets/music/bgm_missile.ogg");
    if (missing) {
        return 1;
    }

    std::vector<meno::Sound> launchSounds;
    std::vector<meno::Sound> explodeSounds;
    std::vector<meno::Sound> loseSounds;
    for (int i = 0; i < 32; ++i) {
        launchSounds.emplace_back(launchBuffer);
        explodeSounds.emplace_back(explodeBuffer);
        loseSounds.emplace_back(loseBuffer);
    }
    std::size_t nextLaunch = 0;
    std::size_t nextExplosion = 0;
    std::size_t nextLoss = 0;
    const auto playSound = [](std::vector<meno::Sound>& sounds, std::size_t& next) {
        sounds[next].play();
        next = (next + 1) % sounds.size();
    };

    music->setLooping(true);
    music->setVolume(0.4f);

    const meno::Camera2D camera{
        .center = {480.f, 270.f}, .size = {960.f, 540.f}};
    game.renderer().setCamera(camera);

    std::array<Target, 9> targets{};
    const std::array<float, 9> targetX{
        160.f, 260.f, 360.f, 600.f, 700.f, 800.f, 60.f, 480.f, 900.f};

    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
    std::mt19937 random{std::random_device{}()};
    std::uniform_real_distribution<float> randomX{0.f, 960.f};

    Screen screen = Screen::Title;
    double titleTime = 0.0;
    double waveTime = 0.0;
    double nextSpawnTime = 1.0;
    double clearTime = 0.0;
    int score = 0;
    int wave = 1;
    int spawned = 0;
    bool clearing = false;
    bool dying = false;
    Vec mouse{480.f, 270.f};
    bool mouseInGame = false;

    const auto cityCount = [&] {
        int count = 0;
        for (int i = 0; i < 6; ++i) {
            if (targets[i].alive) {
                ++count;
            }
        }
        return count;
    };

    const auto beginWave = [&] {
        waveTime = 0.0;
        nextSpawnTime = 1.0;
        spawned = 0;
        clearing = false;
        clearTime = 0.0;
        for (int i = 6; i < 9; ++i) {
            targets[i].alive = true;
            targets[i].ammunition = 10;
        }
    };

    const auto reset = [&] {
        missiles.clear();
        interceptors.clear();
        explosions.clear();
        score = 0;
        wave = 1;
        dying = false;
        for (std::size_t i = 0; i < targets.size(); ++i) {
            targets[i] = Target{{targetX[i], 476.f}, true, 10};
        }
        for (auto& sound : launchSounds) {
            sound.stop();
        }
        for (auto& sound : explodeSounds) {
            sound.stop();
        }
        for (auto& sound : loseSounds) {
            sound.stop();
        }
        nextLaunch = nextExplosion = nextLoss = 0;
        beginWave();
        screen = Screen::Play;
        music->stop();
        music->play();
    };

    const auto addExplosion = [&](Vec position) {
        explosions.push_back({position, 0.0});
        playSound(explodeSounds, nextExplosion);
    };

    const auto finish = [&] {
        screen = Screen::Result;
        music->stop();
    };

    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const auto& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        const Vec pixelMouse{input.mousePosition()};
        mouse = game.renderer().screenToWorld(pixelMouse);
        mouseInGame =
            meno::viewportPixels(camera, game.renderer().framebufferSize())
                .contains(pixelMouse);

        if (screen == Screen::Title) {
            titleTime += fixedDeltaTime;
            if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
                reset();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (input.wasPressed(meno::Key::R)) {
                reset();
            }
            return;
        }

        if (clearing) {
            clearTime += fixedDeltaTime;
            if (clearTime >= 3.0) {
                ++wave;
                beginWave();
            }
            return;
        }

        for (auto& explosion : explosions) {
            explosion.age += fixedDeltaTime;
        }
        std::erase_if(explosions, [](const Explosion& explosion) {
            return explosion.age >= 1.0;
        });

        if (dying) {
            if (explosions.empty()) {
                finish();
            }
            return;
        }

        if (input.wasPressed(meno::MouseButton::Left) &&
            mouseInGame && mouse.y >= 0.f && mouse.y < 470.f) {
            int best = -1;
            float bestDistance = 0.f;
            for (int i = 6; i < 9; ++i) {
                if (!targets[i].alive || targets[i].ammunition <= 0) {
                    continue;
                }
                const float d = std::abs(mouse.x - targets[i].position.x);
                if (best == -1 || d < bestDistance) {
                    best = i;
                    bestDistance = d;
                }
            }
            if (best != -1) {
                --targets[best].ammunition;
                const Vec start = targets[best].position;
                interceptors.push_back({start, start, mouse});
                playSound(launchSounds, nextLaunch);
            }
        }

        waveTime += fixedDeltaTime;
        const int missileCount = 10 + 2 * (wave - 1);
        while (spawned < missileCount && waveTime >= nextSpawnTime) {
            std::array<int, 9> available{};
            int count = 0;
            for (int i = 0; i < 9; ++i) {
                if (targets[i].alive) {
                    available[count++] = i;
                }
            }
            if (count == 0) {
                break;
            }
            std::uniform_int_distribution<int> choose{0, count - 1};
            const int selected = available[choose(random)];
            const Vec start{randomX(random), 0.f};
            missiles.push_back(
                {start, start, targets[selected].position, selected});
            ++spawned;
            nextSpawnTime += 1.2;
        }

        const float dt = static_cast<float>(fixedDeltaTime);
        std::erase_if(interceptors, [&](Interceptor& interceptor) {
            if (advance(interceptor.position, interceptor.destination, 600.f * dt)) {
                addExplosion(interceptor.destination);
                return true;
            }
            return false;
        });

        const float missileSpeed = 70.f + 10.f * static_cast<float>(wave - 1);
        std::erase_if(missiles, [&](Missile& missile) {
            const bool arrived =
                advance(missile.position, missile.destination, missileSpeed * dt);

            bool intercepted = false;
            for (const auto& explosion : explosions) {
                const float r = radius(explosion);
                if (r > 0.f && distance(missile.position, explosion.position) <= r) {
                    intercepted = true;
                    break;
                }
            }
            if (intercepted) {
                score += 25;
                addExplosion(missile.position);
                return true;
            }

            if (arrived) {
                addExplosion(missile.destination);
                Target& target = targets[missile.target];
                if (target.alive) {
                    target.alive = false;
                    playSound(loseSounds, nextLoss);
                }
                return true;
            }
            return false;
        });

        if (cityCount() == 0) {
            dying = true;
            missiles.clear();
            interceptors.clear();
            if (explosions.empty()) {
                finish();
            }
            return;
        }

        if (spawned == missileCount && missiles.empty() &&
            interceptors.empty() && explosions.empty()) {
            for (int i = 6; i < 9; ++i) {
                if (targets[i].alive) {
                    score += targets[i].ammunition * 5;
                }
            }
            score += cityCount() * 100;
            clearing = true;
            clearTime = 0.0;
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);

        const auto text = [&](const std::string& value, Vec position,
                              unsigned int size, bool centered = false) {
            meno::TextParams params{
                .characterSize = size, .color = meno::colors::White};
            if (centered) {
                params.origin =
                    renderer.measureTextBounds(*font, value, params).center();
            }
            renderer.drawText(*font, value, position, params);
        };

        if (screen == Screen::Title) {
            text("SKY SHIELD", {480.f, 210.f}, 48, true);
            text("PRESS ENTER", {480.f, 300.f}, 32, true);
            return;
        }

        if (screen == Screen::Result) {
            text("GAME OVER", {480.f, 170.f}, 48, true);
            text("SCORE " + std::to_string(score), {480.f, 250.f}, 32, true);
            text("WAVE " + std::to_string(wave), {480.f, 300.f}, 32, true);
            text("R: RESTART  ESC: QUIT", {480.f, 390.f}, 32, true);
            return;
        }

        renderer.drawRect({{0.f, 500.f}, {960.f, 40.f}},
                          {.fill = meno::Color::fromRgb(0xB07A2A)});

        for (int i = 0; i < 9; ++i) {
            if (!targets[i].alive) {
                continue;
            }
            const auto& texture = i < 6 ? cityTexture : baseTexture;
            renderer.draw(*texture,
                          {.position = targets[i].position,
                           .origin = {24.f, 0.f}});
            if (i >= 6) {
                text(std::to_string(targets[i].ammunition),
                     {targets[i].position.x, 520.f}, 16, true);
            }
        }

        for (const auto& missile : missiles) {
            renderer.drawLine(missile.origin, missile.position,
                              {.color = meno::Color::fromRgb(0xFF4D4D),
                               .thickness = 2.f});
            renderer.drawRect(
                {{missile.position.x - 2.f, missile.position.y - 2.f}, {4.f, 4.f}},
                {.fill = meno::colors::White});
        }

        for (const auto& interceptor : interceptors) {
            renderer.drawLine(interceptor.origin, interceptor.position,
                              {.color = meno::Color::fromRgb(0x4FC3F7),
                               .thickness = 2.f});
        }

        for (const auto& explosion : explosions) {
            const float r = radius(explosion);
            if (r <= 0.f) {
                continue;
            }
            const bool white = static_cast<int>(explosion.age / 0.1) % 2 == 0;
            renderer.drawCircle(
                explosion.position, r,
                {.fill = white ? meno::colors::White
                               : meno::Color::fromRgb(0xFFD54A)});
        }

        text("SCORE " + std::to_string(score), {16.f, 12.f}, 32);
        text("WAVE " + std::to_string(wave), {736.f, 12.f}, 32);

        if (clearing) {
            text("WAVE " + std::to_string(wave) + " CLEAR",
                 {480.f, 250.f}, 32, true);
        }

        if (mouseInGame) {
            renderer.draw(*crosshairTexture,
                          {.position = mouse, .origin = {12.f, 12.f}});
        }
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    music->stop();
    return 0;
}
```