```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Vec = meno::Vec2f;

constexpr float WorldWidth = 1920.f;
constexpr float WorldHeight = 1080.f;

float lengthSquared(Vec v) {
    return v.x * v.x + v.y * v.y;
}

Vec normalized(Vec v) {
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.00001f ? v * (1.f / length) : Vec{0.f, 0.f};
}

bool overlaps(Vec a, Vec b, float radius) {
    return lengthSquared(a - b) < radius * radius;
}

bool segmentHits(Vec start, Vec end, Vec center, float radius) {
    const Vec segment = end - start;
    const float squared = lengthSquared(segment);
    const Vec offset = center - start;
    const float t = squared > 0.f
                        ? std::clamp((offset.x * segment.x + offset.y * segment.y) / squared,
                                     0.f, 1.f)
                        : 0.f;
    return overlaps(start + segment * t, center, radius);
}

std::string timeString(double seconds) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << seconds;
    return stream.str();
}

struct Bullet {
    Vec position;
    Vec previous;
    Vec velocity;
    double age = 0.0;
    bool dead = false;
};

struct Enemy {
    Vec position;
    bool dead = false;
};

struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;

    void play() {
        if (!sounds.empty()) {
            sounds[next].play();
            next = (next + 1) % sounds.size();
        }
    }

    void reset() {
        for (auto& sound : sounds) {
            sound.stop();
        }
        next = 0;
    }
};
} // namespace

int main() {
    try {
        meno::Game game{{.application = {.framerateLimit = 60},
                         .windowSize = {960, 540},
                         .title = "ARENA",
                         .clearColor = meno::colors::Black}};

        auto& resources = game.resources();
        const auto font = resources.loadFont("assets/fonts/game.ttf");
        const auto playerTexture = resources.loadTexture("assets/textures/player.png");
        const auto enemyTexture = resources.loadTexture("assets/textures/enemy.png");
        const auto bulletTexture = resources.loadTexture("assets/textures/bullet.png");
        const auto shootBuffer = resources.loadSoundBuffer("assets/sfx/shoot.wav");
        const auto hitBuffer = resources.loadSoundBuffer("assets/sfx/hit.wav");
        const auto loseBuffer = resources.loadSoundBuffer("assets/sfx/lose.wav");
        auto music = meno::Music::openFromFile("assets/music/bgm_shooter.ogg");

        bool missing = false;
        const auto require = [&](bool loaded, const char* path) {
            if (!loaded) {
                std::cerr << "Failed to load: " << path << '\n';
                missing = true;
            }
        };
        require(static_cast<bool>(font), "assets/fonts/game.ttf");
        require(static_cast<bool>(playerTexture), "assets/textures/player.png");
        require(static_cast<bool>(enemyTexture), "assets/textures/enemy.png");
        require(static_cast<bool>(bulletTexture), "assets/textures/bullet.png");
        require(static_cast<bool>(shootBuffer), "assets/sfx/shoot.wav");
        require(static_cast<bool>(hitBuffer), "assets/sfx/hit.wav");
        require(static_cast<bool>(loseBuffer), "assets/sfx/lose.wav");
        require(music.has_value(), "assets/music/bgm_shooter.ogg");
        if (missing) {
            return 1;
        }

        SoundPool shoots;
        SoundPool hits;
        SoundPool losses;
        for (int i = 0; i < 12; ++i) {
            shoots.sounds.emplace_back(shootBuffer);
            hits.sounds.emplace_back(hitBuffer);
        }
        for (int i = 0; i < 3; ++i) {
            losses.sounds.emplace_back(loseBuffer);
        }
        music->setVolume(0.4f);
        music->setLooping(true);

        const auto background = meno::Color::fromRgb(0x14181F);
        const auto gridColor = meno::Color::fromRgb(0x222A35);
        const auto boundaryColor = meno::Color::fromRgb(0x5C6B80);
        const meno::Camera2D hud{.center = {480.f, 270.f}, .size = {960.f, 540.f}};
        meno::Camera2D camera = hud;
        game.renderer().setCamera(hud);

        enum class Screen { Title, Play, Result };
        Screen screen = Screen::Title;
        Vec player{960.f, 540.f};
        Vec aim{1.f, 0.f};
        float playerRotation = 0.f;
        int hp = 3;
        int score = 0;
        double titleTime = 0.0;
        double elapsed = 0.0;
        double invulnerability = 0.0;
        double shotCooldown = 0.0;
        double spawnCountdown = 0.5;
        bool paused = false;

        std::vector<Bullet> bullets;
        std::vector<Enemy> enemies;
        bullets.reserve(10);
        enemies.reserve(200);
        const auto seed = std::random_device{}();
        std::mt19937 random(seed);

        const auto updateCamera = [&] {
            camera.center = {
                std::clamp(player.x, 480.f, WorldWidth - 480.f),
                std::clamp(player.y, 270.f, WorldHeight - 270.f)};
            game.renderer().setCamera(camera);
        };

        const auto startPlay = [&] {
            player = {960.f, 540.f};
            aim = {1.f, 0.f};
            playerRotation = 0.f;
            hp = 3;
            score = 0;
            titleTime = 0.0;
            elapsed = 0.0;
            invulnerability = 0.0;
            shotCooldown = 0.0;
            spawnCountdown = 0.5;
            paused = false;
            bullets.clear();
            enemies.clear();
            random.seed(seed);
            shoots.reset();
            hits.reset();
            losses.reset();
            camera = hud;
            updateCamera();
            music->stop();
            music->play();
            screen = Screen::Play;
        };

        // Sample only the portions of the expanded view perimeter inside the world.
        const auto spawnEnemy = [&] {
            struct Edge {
                Vec start;
                Vec end;
                float length;
            };
            std::array<Edge, 4> edges{};
            std::size_t count = 0;
            float total = 0.f;
            const auto add = [&](Vec a, Vec b) {
                const float length = std::sqrt(lengthSquared(b - a));
                if (length > 0.f) {
                    edges[count++] = {a, b, length};
                    total += length;
                }
            };

            const float left = camera.center.x - 480.f - 64.f;
            const float right = camera.center.x + 480.f + 64.f;
            const float top = camera.center.y - 270.f - 64.f;
            const float bottom = camera.center.y + 270.f + 64.f;
            const float x0 = std::max(left, 0.f);
            const float x1 = std::min(right, WorldWidth);
            const float y0 = std::max(top, 0.f);
            const float y1 = std::min(bottom, WorldHeight);

            if (top >= 0.f && top <= WorldHeight && x1 > x0)
                add({x0, top}, {x1, top});
            if (bottom >= 0.f && bottom <= WorldHeight && x1 > x0)
                add({x0, bottom}, {x1, bottom});
            if (left >= 0.f && left <= WorldWidth && y1 > y0)
                add({left, y0}, {left, y1});
            if (right >= 0.f && right <= WorldWidth && y1 > y0)
                add({right, y0}, {right, y1});

            if (count == 0 || total <= 0.f) {
                return;
            }
            float distance = std::uniform_real_distribution<float>(0.f, total)(random);
            for (std::size_t i = 0; i < count; ++i) {
                const auto& edge = edges[i];
                if (distance <= edge.length || i + 1 == count) {
                    const float t = std::clamp(distance / edge.length, 0.f, 1.f);
                    enemies.push_back({edge.start + (edge.end - edge.start) * t, false});
                    return;
                }
                distance -= edge.length;
            }
        };

        meno::Scene& scene = game.scenes().createScene();
        scene.setUpdateHandler([&](double fixedDeltaTime) {
            const auto& input = game.window().input();
            if (input.wasPressed(meno::Key::Escape)) {
                game.stop();
                return;
            }

            if (screen == Screen::Title) {
                titleTime += fixedDeltaTime;
                if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
                    startPlay();
                }
                return;
            }
            if (screen == Screen::Result) {
                if (input.wasPressed(meno::Key::R)) {
                    startPlay();
                }
                return;
            }

            if (input.wasPressed(meno::Key::P)) {
                paused = !paused;
                if (paused) {
                    music->pause();
                } else {
                    music->play();
                }
            }
            if (paused) {
                return;
            }

            const float dt = static_cast<float>(fixedDeltaTime);
            elapsed += fixedDeltaTime;
            invulnerability = std::max(0.0, invulnerability - fixedDeltaTime);
            shotCooldown = std::max(0.0, shotCooldown - fixedDeltaTime);

            const auto active = [&](meno::Key key) {
                return input.isDown(key) || input.wasPressed(key);
            };
            Vec movement{0.f, 0.f};
            if (active(meno::Key::A) || active(meno::Key::Left)) movement.x -= 1.f;
            if (active(meno::Key::D) || active(meno::Key::Right)) movement.x += 1.f;
            if (active(meno::Key::W) || active(meno::Key::Up)) movement.y -= 1.f;
            if (active(meno::Key::S) || active(meno::Key::Down)) movement.y += 1.f;
            player += normalized(movement) * (240.f * dt);
            player.x = std::clamp(player.x, 16.f, WorldWidth - 16.f);
            player.y = std::clamp(player.y, 16.f, WorldHeight - 16.f);
            updateCamera();

            const Vec mouse{input.mousePosition()};
            const Vec target = game.renderer().screenToWorld(mouse);
            if (lengthSquared(target - player) > 0.000001f) {
                aim = normalized(target - player);
            }
            playerRotation = std::atan2(aim.y, aim.x) *
                             (180.f / std::numbers::pi_v<float>);

            for (auto& bullet : bullets) {
                bullet.previous = bullet.position;
                bullet.position += bullet.velocity * dt;
                bullet.age += fixedDeltaTime;
                bullet.dead = bullet.age >= 1.4 ||
                              bullet.position.x < 0.f || bullet.position.x > WorldWidth ||
                              bullet.position.y < 0.f || bullet.position.y > WorldHeight;
            }
            std::erase_if(bullets, [](const Bullet& bullet) { return bullet.dead; });

            if ((input.isDown(meno::MouseButton::Left) ||
                 input.wasPressed(meno::MouseButton::Left)) &&
                shotCooldown <= 0.0000001 && bullets.size() < 10) {
                bullets.push_back({player, player, aim * 600.f, 0.0, false});
                shotCooldown = 0.15;
                shoots.play();
            }

            spawnCountdown -= fixedDeltaTime;
            while (spawnCountdown <= 0.0000001) {
                if (enemies.size() < 200) {
                    spawnEnemy();
                }
                const double interval =
                    std::max(0.3, 1.0 - 0.1 * std::floor(elapsed / 10.0));
                spawnCountdown += interval;
            }

            for (auto& enemy : enemies) {
                const Vec delta = player - enemy.position;
                const float distance = std::sqrt(lengthSquared(delta));
                if (distance > 0.f) {
                    enemy.position += delta * (std::min(100.f * dt, distance) / distance);
                }
            }

            for (auto& bullet : bullets) {
                for (auto& enemy : enemies) {
                    if (!enemy.dead &&
                        segmentHits(bullet.previous, bullet.position, enemy.position, 18.f)) {
                        bullet.dead = true;
                        enemy.dead = true;
                        score += 10;
                        hits.play();
                        break;
                    }
                }
            }

            if (invulnerability <= 0.0) {
                for (auto& enemy : enemies) {
                    if (!enemy.dead && overlaps(player, enemy.position, 28.f)) {
                        enemy.dead = true;
                        --hp;
                        invulnerability = 1.0;
                        losses.play();
                        break;
                    }
                }
            }

            std::erase_if(bullets, [](const Bullet& bullet) { return bullet.dead; });
            std::erase_if(enemies, [](const Enemy& enemy) { return enemy.dead; });

            if (hp <= 0) {
                screen = Screen::Result;
                music->stop();
                game.renderer().setCamera(hud);
            }
        });

        const auto centeredText = [&](meno::Renderer& renderer, const std::string& text,
                                      float y, unsigned int size) {
            meno::TextParams params{.characterSize = size,
                                    .color = meno::colors::White};
            params.origin = renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {480.f, y}, params);
        };

        scene.setDrawHandler([&](meno::Renderer& renderer) {
            if (screen != Screen::Play) {
                renderer.setCamera(hud);
                renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}}, {.fill = background});
                if (screen == Screen::Title) {
                    centeredText(renderer, "ARENA", 220.f, 48);
                    centeredText(renderer, "PRESS ENTER", 310.f, 32);
                } else {
                    centeredText(renderer, "GAME OVER", 170.f, 48);
                    centeredText(renderer, "SCORE " + std::to_string(score), 250.f, 32);
                    centeredText(renderer, "TIME " + timeString(elapsed), 300.f, 32);
                    centeredText(renderer, "R: RESTART  ESC: QUIT", 390.f, 32);
                }
                return;
            }

            renderer.setCamera(camera);
            renderer.drawRect({{0.f, 0.f}, {WorldWidth, WorldHeight}},
                              {.fill = background});
            for (float x = 120.f; x < WorldWidth; x += 120.f) {
                renderer.drawLine({x, 0.f}, {x, WorldHeight},
                                  {.color = gridColor, .thickness = 1.f});
            }
            for (float y = 120.f; y < WorldHeight; y += 120.f) {
                renderer.drawLine({0.f, y}, {WorldWidth, y},
                                  {.color = gridColor, .thickness = 1.f});
            }
            renderer.drawRect({{0.f, 0.f}, {WorldWidth, WorldHeight}},
                              {.fill = meno::colors::Transparent,
                               .outline = boundaryColor,
                               .outlineThickness = -2.f});

            for (const auto& bullet : bullets) {
                renderer.draw(*bulletTexture,
                              {.position = bullet.position, .origin = {4.f, 4.f}});
            }
            for (const auto& enemy : enemies) {
                renderer.draw(*enemyTexture,
                              {.position = enemy.position, .origin = {16.f, 16.f}});
            }

            const bool dim = invulnerability > 0.0 &&
                             static_cast<int>(invulnerability * 12.0) % 2 == 0;
            renderer.draw(*playerTexture,
                          {.position = player,
                           .origin = {16.f, 16.f},
                           .rotation = playerRotation,
                           .tint = meno::colors::White.withAlpha(dim ? 80 : 255)});

            renderer.setCamera(hud);
            const std::string status = "HP " + std::to_string(hp) +
                                       "  SCORE " + std::to_string(score) +
                                       "  TIME " + timeString(elapsed);
            renderer.drawText(*font, status, {16.f, 16.f},
                              {.characterSize = 16, .color = meno::colors::White});
            if (paused) {
                centeredText(renderer, "PAUSED", 270.f, 48);
            }
        });

        scene.setExitHandler([&] { music->stop(); });
        (void)game.scenes().changeScene(scene.id());
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```