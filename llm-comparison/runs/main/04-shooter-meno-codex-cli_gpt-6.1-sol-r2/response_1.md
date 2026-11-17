```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
using meno::Vec2f;

constexpr float WorldW = 1920.f;
constexpr float WorldH = 1080.f;
constexpr float ViewW = 960.f;
constexpr float ViewH = 540.f;

enum class Screen { Title, Play, Result };

struct Bullet {
    Vec2f position;
    Vec2f velocity;
    float life = 1.4f;
    bool alive = true;
};

struct Enemy {
    Vec2f position;
    bool alive = true;
};

struct Segment {
    Vec2f a;
    Vec2f b;
    float length;
};

float lengthSquared(Vec2f v) {
    return v.x * v.x + v.y * v.y;
}

Vec2f normalized(Vec2f v) {
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.00001f ? v * (1.f / length) : Vec2f{0.f, 0.f};
}

std::string timeText(double seconds) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << seconds;
    return stream.str();
}

// 선분이 원에 처음 들어가는 위치를 0~1로 반환합니다.
float intersectionTime(Vec2f start, Vec2f end, Vec2f center, float radius) {
    const Vec2f offset = start - center;
    const Vec2f direction = end - start;
    const float c = lengthSquared(offset) - radius * radius;
    if (c <= 0.f) {
        return 0.f;
    }

    const float a = lengthSquared(direction);
    if (a <= 0.000001f) {
        return std::numeric_limits<float>::infinity();
    }

    const float b = offset.x * direction.x + offset.y * direction.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.f) {
        return std::numeric_limits<float>::infinity();
    }

    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.f && t <= 1.f
               ? t
               : std::numeric_limits<float>::infinity();
}

class SoundPool {
public:
    SoundPool(std::shared_ptr<const meno::SoundBuffer> buffer, std::size_t count) {
        sounds_.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
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
} // namespace

int main() {
    try {
        meno::Game game{{
            .application = {.framerateLimit = 60},
            .windowSize = {960, 540},
            .title = "ARENA",
            .clearColor = meno::colors::Black
        }};

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
        const auto require = [&](bool success, const char* path) {
            if (!success) {
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

        SoundPool shootSounds{shootBuffer, 10};
        SoundPool hitSounds{hitBuffer, 16};
        SoundPool loseSounds{loseBuffer, 3};

        music->setVolume(0.4f);
        music->setLooping(true);

        const meno::Color background = meno::Color::fromRgb(0x14181F);
        const meno::Color gridColor = meno::Color::fromRgb(0x222A35);
        const meno::Color borderColor = meno::Color::fromRgb(0x5C6B80);

        const meno::Camera2D hud{
            .center = {ViewW * 0.5f, ViewH * 0.5f},
            .size = {ViewW, ViewH}
        };
        meno::Camera2D camera = hud;

        Screen screen = Screen::Title;
        double titleTime = 0.0;
        double elapsed = 0.0;
        double spawnCountdown = 0.5;
        double shotCountdown = 0.0;
        float invincibility = 0.f;
        bool paused = false;
        int hp = 3;
        int score = 0;
        Vec2f player{WorldW * 0.5f, WorldH * 0.5f};
        Vec2f aim{1.f, 0.f};
        float rotation = 0.f;

        std::vector<Bullet> bullets;
        std::vector<Enemy> enemies;
        bullets.reserve(16);
        enemies.reserve(200);

        std::mt19937 random{std::random_device{}()};

        const auto updateCamera = [&] {
            camera.center = {
                std::clamp(player.x, ViewW * 0.5f, WorldW - ViewW * 0.5f),
                std::clamp(player.y, ViewH * 0.5f, WorldH - ViewH * 0.5f)
            };
            game.renderer().setCamera(camera);
        };

        const auto startPlay = [&] {
            bullets.clear();
            enemies.clear();
            shootSounds.stop();
            hitSounds.stop();
            loseSounds.stop();
            player = {WorldW * 0.5f, WorldH * 0.5f};
            aim = {1.f, 0.f};
            rotation = 0.f;
            hp = 3;
            score = 0;
            elapsed = 0.0;
            spawnCountdown = 0.5;
            shotCountdown = 0.0;
            invincibility = 0.f;
            paused = false;
            screen = Screen::Play;
            updateCamera();
            music->stop();
            music->play();
        };

        const auto spawnEnemy = [&] {
            // 확장한 시야의 테두리 중 월드 안에 있는 구간만 표본으로 씁니다.
            const float left = camera.center.x - ViewW * 0.5f - 64.f;
            const float right = camera.center.x + ViewW * 0.5f + 64.f;
            const float top = camera.center.y - ViewH * 0.5f - 64.f;
            const float bottom = camera.center.y + ViewH * 0.5f + 64.f;

            std::vector<Segment> segments;
            segments.reserve(4);
            const auto horizontal = [&](float y) {
                const float a = std::max(0.f, left);
                const float b = std::min(WorldW, right);
                if (y >= 0.f && y <= WorldH && b > a) {
                    segments.push_back({{a, y}, {b, y}, b - a});
                }
            };
            const auto vertical = [&](float x) {
                const float a = std::max(0.f, top);
                const float b = std::min(WorldH, bottom);
                if (x >= 0.f && x <= WorldW && b > a) {
                    segments.push_back({{x, a}, {x, b}, b - a});
                }
            };
            horizontal(top);
            horizontal(bottom);
            vertical(left);
            vertical(right);

            float total = 0.f;
            for (const auto& segment : segments) {
                total += segment.length;
            }
            if (total <= 0.f) {
                return;
            }

            float distance = std::uniform_real_distribution<float>{0.f, total}(random);
            for (std::size_t i = 0; i < segments.size(); ++i) {
                const auto& segment = segments[i];
                if (distance <= segment.length || i + 1 == segments.size()) {
                    const float t = std::clamp(distance / segment.length, 0.f, 1.f);
                    enemies.push_back({segment.a + (segment.b - segment.a) * t});
                    return;
                }
                distance -= segment.length;
            }
        };

        const auto centeredText = [&](meno::Renderer& renderer,
                                      const std::string& text,
                                      float y, unsigned int size) {
            meno::TextParams params{
                .characterSize = size,
                .color = meno::colors::White
            };
            params.origin = renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {ViewW * 0.5f, y}, params);
        };

        auto& scene = game.scenes().createScene();
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
                return;
            }
            if (paused) {
                return;
            }

            const float dt = static_cast<float>(fixedDeltaTime);
            elapsed += fixedDeltaTime;
            invincibility = std::max(0.f, invincibility - dt);

            const auto active = [&](meno::Key key) {
                return input.isDown(key) || input.wasPressed(key);
            };
            Vec2f movement{0.f, 0.f};
            if (active(meno::Key::A) || active(meno::Key::Left)) {
                movement.x -= 1.f;
            }
            if (active(meno::Key::D) || active(meno::Key::Right)) {
                movement.x += 1.f;
            }
            if (active(meno::Key::W) || active(meno::Key::Up)) {
                movement.y -= 1.f;
            }
            if (active(meno::Key::S) || active(meno::Key::Down)) {
                movement.y += 1.f;
            }
            player += normalized(movement) * (240.f * dt);
            player.x = std::clamp(player.x, 16.f, WorldW - 16.f);
            player.y = std::clamp(player.y, 16.f, WorldH - 16.f);
            updateCamera();

            const auto framebuffer = game.renderer().framebufferSize();
            if (framebuffer.x > 0 && framebuffer.y > 0) {
                const Vec2f mouse{input.mousePosition()};
                const Vec2f target = meno::screenToWorld(camera, framebuffer, mouse);
                const Vec2f direction = target - player;
                if (lengthSquared(direction) > 0.000001f) {
                    aim = normalized(direction);
                    rotation = std::atan2(aim.y, aim.x) * 180.f / 3.14159265358979323846f;
                }
            }

            shotCountdown = std::max(0.0, shotCountdown - fixedDeltaTime);
            if ((input.isDown(meno::MouseButton::Left) ||
                 input.wasPressed(meno::MouseButton::Left)) &&
                shotCountdown <= 0.0000001) {
                bullets.push_back({player, aim * 600.f});
                shootSounds.play();
                shotCountdown = 0.15;
            }

            spawnCountdown -= fixedDeltaTime;
            while (spawnCountdown <= 0.0000001) {
                if (enemies.size() < 200) {
                    spawnEnemy();
                }
                const double interval =
                    std::max(0.3, 1.0 - std::floor(elapsed / 10.0) * 0.1);
                spawnCountdown += interval;
            }

            for (auto& enemy : enemies) {
                enemy.position += normalized(player - enemy.position) * (100.f * dt);
            }

            for (auto& bullet : bullets) {
                const Vec2f previous = bullet.position;
                bullet.position += bullet.velocity * dt;
                bullet.life -= dt;

                Enemy* target = nullptr;
                float firstTime = std::numeric_limits<float>::infinity();
                for (auto& enemy : enemies) {
                    if (!enemy.alive) {
                        continue;
                    }
                    const float t =
                        intersectionTime(previous, bullet.position, enemy.position, 18.f);
                    if (t < firstTime) {
                        firstTime = t;
                        target = &enemy;
                    }
                }

                if (target != nullptr) {
                    target->alive = false;
                    bullet.alive = false;
                    score += 10;
                    hitSounds.play();
                }

                if (bullet.life <= 0.000001f ||
                    bullet.position.x < 0.f || bullet.position.x > WorldW ||
                    bullet.position.y < 0.f || bullet.position.y > WorldH) {
                    bullet.alive = false;
                }
            }

            if (invincibility <= 0.f) {
                for (auto& enemy : enemies) {
                    if (enemy.alive &&
                        lengthSquared(enemy.position - player) < 28.f * 28.f) {
                        enemy.alive = false;
                        --hp;
                        invincibility = 1.f;
                        loseSounds.play();
                        break;
                    }
                }
            }

            std::erase_if(bullets, [](const Bullet& bullet) { return !bullet.alive; });
            std::erase_if(enemies, [](const Enemy& enemy) { return !enemy.alive; });

            if (hp <= 0) {
                screen = Screen::Result;
                paused = false;
                music->stop();
            }
        });

        scene.setDrawHandler([&](meno::Renderer& renderer) {
            if (screen != Screen::Play) {
                renderer.setCamera(hud);
                renderer.drawRect({{0.f, 0.f}, {ViewW, ViewH}},
                                  {.fill = background});

                if (screen == Screen::Title) {
                    centeredText(renderer, "ARENA", 218.f, 48);
                    centeredText(renderer, "PRESS ENTER", 306.f, 32);
                } else {
                    centeredText(renderer, "GAME OVER", 164.f, 48);
                    centeredText(renderer, "SCORE " + std::to_string(score), 244.f, 32);
                    centeredText(renderer, "TIME " + timeText(elapsed), 292.f, 32);
                    centeredText(renderer, "R: RESTART  ESC: QUIT", 380.f, 16);
                }
                return;
            }

            renderer.setCamera(camera);
            renderer.drawRect({{0.f, 0.f}, {WorldW, WorldH}},
                              {.fill = background});

            for (float x = 120.f; x < WorldW; x += 120.f) {
                renderer.drawLine({x, 0.f}, {x, WorldH},
                                  {.color = gridColor, .thickness = 1.f});
            }
            for (float y = 120.f; y < WorldH; y += 120.f) {
                renderer.drawLine({0.f, y}, {WorldW, y},
                                  {.color = gridColor, .thickness = 1.f});
            }
            renderer.drawRect(
                {{0.f, 0.f}, {WorldW, WorldH}},
                {.fill = meno::colors::Transparent,
                 .outline = borderColor,
                 .outlineThickness = -2.f});

            const float left = camera.center.x - ViewW * 0.5f;
            const float top = camera.center.y - ViewH * 0.5f;
            const auto visible = [&](Vec2f p, float radius) {
                return p.x + radius >= left && p.x - radius <= left + ViewW &&
                       p.y + radius >= top && p.y - radius <= top + ViewH;
            };

            for (const auto& enemy : enemies) {
                if (visible(enemy.position, 24.f)) {
                    renderer.draw(*enemyTexture,
                                  {.position = enemy.position,
                                   .origin = {16.f, 16.f}});
                }
            }
            for (const auto& bullet : bullets) {
                if (visible(bullet.position, 6.f)) {
                    renderer.draw(*bulletTexture,
                                  {.position = bullet.position,
                                   .origin = {4.f, 4.f}});
                }
            }

            const bool dim =
                invincibility > 0.f &&
                static_cast<int>(invincibility * 12.f) % 2 == 0;
            renderer.draw(
                *playerTexture,
                {.position = player,
                 .origin = {16.f, 16.f},
                 .rotation = rotation,
                 .tint = dim ? meno::colors::White.withAlpha(80)
                             : meno::colors::White});

            renderer.setCamera(hud);
            renderer.drawText(
                *font,
                "HP " + std::to_string(hp) +
                    "  SCORE " + std::to_string(score) +
                    "  TIME " + timeText(elapsed),
                {16.f, 16.f},
                {.characterSize = 16, .color = meno::colors::White});

            if (paused) {
                centeredText(renderer, "PAUSED", ViewH * 0.5f, 48);
            }
        });

        game.renderer().setCamera(hud);
        (void)game.scenes().changeScene(scene.id());
        game.run();
        music->stop();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
```