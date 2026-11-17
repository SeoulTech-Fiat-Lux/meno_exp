```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

using meno::Vec2f;

constexpr float TargetY = 476.f;
constexpr float MaximumRadius = 48.f;

constexpr std::array<float, 9> TargetX{
    160.f, 260.f, 360.f, 600.f, 700.f, 800.f,
    60.f, 480.f, 900.f
};

float distance(Vec2f a, Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

bool advance(Vec2f& position, Vec2f destination, float step) {
    const float remaining = distance(position, destination);
    if (remaining <= step) {
        position = destination;
        return true;
    }
    position += (destination - position) * (step / remaining);
    return false;
}

struct Interceptor {
    Vec2f start;
    Vec2f position;
    Vec2f destination;
};

struct Missile {
    Vec2f start;
    Vec2f position;
    int target;
    bool arrived = false;
    bool dead = false;
};

struct Explosion {
    Vec2f position;
    double age = 0.0;

    float radius() const {
        return MaximumRadius *
               static_cast<float>(std::clamp(
                   age <= 0.5 ? age / 0.5 : (1.0 - age) / 0.5,
                   0.0, 1.0));
    }
};

class SoundPool {
public:
    explicit SoundPool(std::shared_ptr<const meno::SoundBuffer> buffer) {
        sounds_.reserve(32);
        for (int i = 0; i < 32; ++i) {
            sounds_.emplace_back(buffer);
        }
    }

    void play() {
        sounds_[next_].play();
        next_ = (next_ + 1) % sounds_.size();
    }

    void stop() {
        for (auto& sound : sounds_) {
            sound.stop();
        }
        next_ = 0;
    }

private:
    std::vector<meno::Sound> sounds_;
    std::size_t next_ = 0;
};

int runGame() {
    meno::Game game{{
        .application = {.framerateLimit = 60},
        .windowSize = {960, 540},
        .title = "SKY SHIELD",
        .clearColor = meno::colors::Black
    }};

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

    bool valid = true;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            valid = false;
        }
    };

    require(static_cast<bool>(font), "assets/fonts/game.ttf");
    require(static_cast<bool>(cityTexture), "assets/textures/md_city.png");
    require(static_cast<bool>(baseTexture), "assets/textures/md_base.png");
    require(static_cast<bool>(crosshairTexture),
            "assets/textures/md_crosshair.png");
    require(static_cast<bool>(launchBuffer), "assets/sfx/launch.wav");
    require(static_cast<bool>(explodeBuffer), "assets/sfx/explode.wav");
    require(static_cast<bool>(loseBuffer), "assets/sfx/lose.wav");
    require(static_cast<bool>(music), "assets/music/bgm_missile.ogg");
    if (!valid) {
        return 1;
    }

    SoundPool launches{launchBuffer};
    SoundPool explosionsAudio{explodeBuffer};
    SoundPool losses{loseBuffer};

    music->setVolume(0.4f);
    music->setLooping(true);

    const meno::Camera2D camera{
        .center = {480.f, 270.f},
        .size = {960.f, 540.f}
    };
    game.renderer().setCamera(camera);

    enum class Screen { Title, Play, Result };
    Screen screen = Screen::Title;

    std::array<bool, 9> alive{};
    std::array<int, 3> ammunition{};
    std::vector<Interceptor> interceptors;
    std::vector<Missile> missiles;
    std::vector<Explosion> explosions;

    const auto seed = std::random_device{}();
    std::mt19937 random{seed};

    int score = 0;
    int wave = 1;
    int spawned = 0;
    double titleTime = 0.0;
    double spawnTimer = 1.0;
    double clearTimer = 0.0;
    bool waveClear = false;
    bool dying = false;
    Vec2f mouse{480.f, 270.f};
    bool mouseInGame = true;

    const auto cityCount = [&] {
        return static_cast<int>(
            std::count(alive.begin(), alive.begin() + 6, true));
    };

    const auto beginWave = [&] {
        spawned = 0;
        spawnTimer = 1.0;
        waveClear = false;
        clearTimer = 0.0;
        for (int i = 0; i < 3; ++i) {
            alive[6 + i] = true;
            ammunition[i] = 10;
        }
    };

    const auto restart = [&] {
        launches.stop();
        explosionsAudio.stop();
        losses.stop();
        music->stop();
        random.seed(seed);
        alive.fill(true);
        ammunition.fill(10);
        interceptors.clear();
        missiles.clear();
        explosions.clear();
        score = 0;
        wave = 1;
        dying = false;
        titleTime = 0.0;
        beginWave();
        screen = Screen::Play;
        music->play();
    };

    const auto explode = [&](Vec2f position) {
        explosions.push_back({position, 0.0});
        explosionsAudio.play();
    };

    const auto showResult = [&] {
        screen = Screen::Result;
        music->stop();
    };

    const auto hitByExplosion = [&](Vec2f point) {
        for (const Explosion& explosion : explosions) {
            const float radius = explosion.radius();
            if (distance(point, explosion.position) <= radius) {
                return true;
            }
        }
        return false;
    };

    meno::Scene& scene = game.scenes().createScene();

    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        const Vec2f pixelMouse{input.mousePosition()};
        mouse = game.renderer().screenToWorld(pixelMouse);
        mouseInGame = meno::viewportPixels(
            camera, game.renderer().framebufferSize()).contains(pixelMouse);

        if (screen == Screen::Title) {
            titleTime += fixedDeltaTime;
            if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
                restart();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (input.wasPressed(meno::Key::R)) {
                restart();
            }
            return;
        }

        if (waveClear) {
            clearTimer += fixedDeltaTime;
            if (clearTimer >= 3.0) {
                ++wave;
                beginWave();
            }
            return;
        }

        const float dt = static_cast<float>(fixedDeltaTime);
        for (Explosion& explosion : explosions) {
            explosion.age += fixedDeltaTime;
        }
        std::erase_if(explosions, [](const Explosion& explosion) {
            return explosion.age >= 1.0;
        });

        if (dying) {
            if (explosions.empty()) {
                showResult();
            }
            return;
        }

        if (input.wasPressed(meno::MouseButton::Left) &&
            mouseInGame && mouse.y < 470.f) {
            int chosen = -1;
            float nearest = std::numeric_limits<float>::max();

            // Ascending order and strict comparison resolve ties to the left.
            for (int i = 0; i < 3; ++i) {
                if (!alive[6 + i] || ammunition[i] == 0) {
                    continue;
                }
                const float separation = std::abs(mouse.x - TargetX[6 + i]);
                if (separation < nearest) {
                    nearest = separation;
                    chosen = i;
                }
            }

            if (chosen != -1) {
                const Vec2f start{TargetX[6 + chosen], TargetY};
                interceptors.push_back({start, start, mouse});
                --ammunition[chosen];
                launches.play();
            }
        }

        spawnTimer -= fixedDeltaTime;
        const int waveMissiles = 10 + 2 * (wave - 1);
        while (spawned < waveMissiles && spawnTimer <= 0.0) {
            std::array<int, 9> choices{};
            int count = 0;
            for (int i = 0; i < 9; ++i) {
                if (alive[i]) {
                    choices[count++] = i;
                }
            }

            if (count == 0) {
                break;
            }

            const int target = choices[
                std::uniform_int_distribution<int>{0, count - 1}(random)];
            const Vec2f start{
                std::uniform_real_distribution<float>{0.f, 960.f}(random),
                0.f
            };
            missiles.push_back({start, start, target});
            ++spawned;
            spawnTimer += 1.2;
        }

        std::erase_if(interceptors, [&](Interceptor& interceptor) {
            if (advance(interceptor.position, interceptor.destination, 600.f * dt)) {
                explode(interceptor.destination);
                return true;
            }
            return false;
        });

        const float missileSpeed = 70.f + 10.f * static_cast<float>(wave - 1);
        for (Missile& missile : missiles) {
            missile.arrived = advance(
                missile.position,
                {TargetX[missile.target], TargetY},
                missileSpeed * dt);
        }

        // Mark before creating the chained explosion: a missile scores once.
        const auto resolveChains = [&] {
            bool changed;
            do {
                changed = false;
                for (Missile& missile : missiles) {
                    if (!missile.dead && hitByExplosion(missile.position)) {
                        missile.dead = true;
                        score += 25;
                        explode(missile.position);
                        changed = true;
                    }
                }
            } while (changed);
        };

        resolveChains();

        for (Missile& missile : missiles) {
            if (missile.dead || !missile.arrived) {
                continue;
            }

            missile.dead = true;
            explode(missile.position);
            if (alive[missile.target]) {
                alive[missile.target] = false;
                losses.play();
            }
        }

        // Impact explosions use the same interception and chaining rules.
        resolveChains();
        std::erase_if(missiles, [](const Missile& missile) {
            return missile.dead;
        });

        if (cityCount() == 0) {
            dying = true;
            // No further projectile events after the last city is lost.
            missiles.clear();
            interceptors.clear();
            if (explosions.empty()) {
                showResult();
            }
            return;
        }

        if (spawned == waveMissiles && missiles.empty() &&
            interceptors.empty() && explosions.empty()) {
            for (int i = 0; i < 3; ++i) {
                if (alive[6 + i]) {
                    score += ammunition[i] * 5;
                }
            }
            score += cityCount() * 100;
            waveClear = true;
            clearTimer = 0.0;
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);

        const auto text = [&](const std::string& value, Vec2f position,
                              unsigned int size) {
            renderer.drawText(*font, value, position,
                              {.characterSize = size,
                               .color = meno::colors::White});
        };

        const auto centered = [&](const std::string& value, Vec2f position,
                                  unsigned int size) {
            meno::TextParams params{
                .characterSize = size,
                .color = meno::colors::White
            };
            params.origin =
                renderer.measureTextBounds(*font, value, params).center();
            renderer.drawText(*font, value, position, params);
        };

        if (screen == Screen::Title) {
            centered("SKY SHIELD", {480.f, 220.f}, 48);
            centered("PRESS ENTER", {480.f, 310.f}, 32);
            return;
        }

        if (screen == Screen::Result) {
            centered("GAME OVER", {480.f, 160.f}, 48);
            centered("SCORE " + std::to_string(score), {480.f, 240.f}, 32);
            centered("WAVE " + std::to_string(wave), {480.f, 290.f}, 32);
            centered("R: RESTART  ESC: QUIT", {480.f, 380.f}, 32);
            return;
        }

        renderer.drawRect({{0.f, 500.f}, {960.f, 40.f}},
                          {.fill = meno::Color::fromRgb(0xB07A2A)});

        for (int i = 0; i < 9; ++i) {
            if (!alive[i]) {
                continue;
            }
            renderer.draw(i < 6 ? *cityTexture : *baseTexture,
                          {.position = {TargetX[i] - 24.f, TargetY}});
            if (i >= 6) {
                centered(std::to_string(ammunition[i - 6]),
                         {TargetX[i], 519.f}, 16);
            }
        }

        for (const Missile& missile : missiles) {
            renderer.drawLine(
                missile.start, missile.position,
                {.color = meno::Color::fromRgb(0xFF4D4D), .thickness = 2.f});
            renderer.drawRect(
                {{missile.position.x - 2.f, missile.position.y - 2.f},
                 {4.f, 4.f}},
                {.fill = meno::colors::White});
        }

        for (const Interceptor& interceptor : interceptors) {
            renderer.drawLine(
                interceptor.start, interceptor.position,
                {.color = meno::Color::fromRgb(0x4FC3F7), .thickness = 2.f});
        }

        for (const Explosion& explosion : explosions) {
            const float radius = explosion.radius();
            if (radius > 0.f) {
                const bool white =
                    static_cast<int>(std::floor(explosion.age * 10.0)) % 2 == 0;
                renderer.drawCircle(
                    explosion.position, radius,
                    {.fill = white ? meno::colors::White
                                   : meno::Color::fromRgb(0xFFD54A)});
            }
        }

        text("SCORE " + std::to_string(score), {16.f, 12.f}, 32);
        text("WAVE " + std::to_string(wave), {720.f, 12.f}, 32);

        if (waveClear) {
            centered("WAVE " + std::to_string(wave) + " CLEAR",
                     {480.f, 250.f}, 32);
        }

        if (mouseInGame) {
            renderer.draw(*crosshairTexture,
                          {.position = mouse, .origin = {12.f, 12.f}});
        }
    });

    scene.setExitHandler([&] {
        music->stop();
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}

} // namespace

int main() {
    try {
        return runGame();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```