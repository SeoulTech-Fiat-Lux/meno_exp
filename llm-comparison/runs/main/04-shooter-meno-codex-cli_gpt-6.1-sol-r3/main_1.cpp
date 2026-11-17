#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr float worldWidth = 1920.f;
constexpr float worldHeight = 1080.f;
constexpr float pi = 3.14159265358979323846f;

struct Bullet {
    meno::Vec2f position;
    meno::Vec2f velocity;
    double age = 0.0;
    bool dead = false;
};

struct Enemy {
    meno::Vec2f position;
    bool dead = false;
};

enum class Screen { Title, Play, Result };

float lengthSquared(meno::Vec2f v) {
    return v.x * v.x + v.y * v.y;
}

meno::Vec2f normalized(meno::Vec2f v) {
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.00001f ? v * (1.f / length) : meno::Vec2f{0.f, 0.f};
}

bool overlaps(meno::Vec2f a, meno::Vec2f b, float radiusSum) {
    return lengthSquared(a - b) < radiusSum * radiusSum;
}

std::string decimalTime(double seconds) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(1) << seconds;
    return text.str();
}
} // namespace

int main() {
    try {
        meno::Game game{
            {.application = {.framerateLimit = 60},
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
        require(static_cast<bool>(music), "assets/music/bgm_shooter.ogg");
        if (missing) {
            return 1;
        }

        music->setLooping(true);
        music->setVolume(0.4f);

        std::vector<meno::Sound> shootSounds;
        std::vector<meno::Sound> hitSounds;
        std::vector<meno::Sound> loseSounds;
        shootSounds.reserve(12);
        hitSounds.reserve(16);
        loseSounds.reserve(3);
        for (int i = 0; i < 12; ++i) {
            shootSounds.emplace_back(shootBuffer);
        }
        for (int i = 0; i < 16; ++i) {
            hitSounds.emplace_back(hitBuffer);
        }
        for (int i = 0; i < 3; ++i) {
            loseSounds.emplace_back(loseBuffer);
        }
        std::size_t nextShoot = 0;
        std::size_t nextHit = 0;
        std::size_t nextLose = 0;

        const auto playSound = [](std::vector<meno::Sound>& sounds,
                                  std::size_t& next) {
            sounds[next].play();
            next = (next + 1) % sounds.size();
        };

        const meno::Color background = meno::Color::fromRgb(0x14181F);
        const meno::Color gridColor = meno::Color::fromRgb(0x222A35);
        const meno::Color boundaryColor = meno::Color::fromRgb(0x5C6B80);
        const meno::Camera2D hud{
            .center = {480.f, 270.f}, .size = {960.f, 540.f}};
        meno::Camera2D camera{
            .center = {960.f, 540.f}, .size = {960.f, 540.f}};

        Screen screen = Screen::Title;
        meno::Vec2f player{960.f, 540.f};
        meno::Vec2f aim{1.f, 0.f};
        float playerAngle = 0.f;
        int hp = 3;
        int score = 0;
        bool paused = false;
        double titleTime = 0.0;
        double elapsed = 0.0;
        double invincible = 0.0;
        double shootCooldown = 0.0;
        double spawnCountdown = 0.5;

        std::vector<Bullet> bullets;
        std::vector<Enemy> enemies;
        bullets.reserve(16);
        enemies.reserve(200);

        const auto randomSeed = std::random_device{}();
        std::mt19937 random{randomSeed};

        const auto updateCamera = [&] {
            camera.center = {
                std::clamp(player.x, 480.f, worldWidth - 480.f),
                std::clamp(player.y, 270.f, worldHeight - 270.f)};
            game.renderer().setCamera(camera);
        };

        const auto restart = [&] {
            player = {960.f, 540.f};
            aim = {1.f, 0.f};
            playerAngle = 0.f;
            hp = 3;
            score = 0;
            paused = false;
            titleTime = 0.0;
            elapsed = 0.0;
            invincible = 0.0;
            shootCooldown = 0.0;
            spawnCountdown = 0.5;
            bullets.clear();
            enemies.clear();
            random.seed(randomSeed);
            for (auto& sound : shootSounds) {
                sound.stop();
            }
            for (auto& sound : hitSounds) {
                sound.stop();
            }
            for (auto& sound : loseSounds) {
                sound.stop();
            }
            nextShoot = nextHit = nextLose = 0;
            screen = Screen::Play;
            updateCamera();
            music->stop();
            music->play();
        };

        // Only portions of the expanded view's original perimeter that lie
        // inside the world are eligible; clipping does not create new edges.
        const auto spawnEnemy = [&] {
            struct Segment {
                meno::Vec2f start;
                meno::Vec2f end;
                float length;
            };
            std::array<Segment, 4> segments{};
            std::size_t count = 0;
            float total = 0.f;

            const auto add = [&](meno::Vec2f a, meno::Vec2f b) {
                const float length = std::sqrt(lengthSquared(b - a));
                if (length > 0.f) {
                    segments[count++] = {a, b, length};
                    total += length;
                }
            };

            const float left = camera.center.x - 480.f - 64.f;
            const float right = camera.center.x + 480.f + 64.f;
            const float top = camera.center.y - 270.f - 64.f;
            const float bottom = camera.center.y + 270.f + 64.f;
            const float minX = std::max(0.f, left);
            const float maxX = std::min(worldWidth, right);
            const float minY = std::max(0.f, top);
            const float maxY = std::min(worldHeight, bottom);

            if (minY < maxY) {
                if (left >= 0.f && left <= worldWidth) {
                    add({left, minY}, {left, maxY});
                }
                if (right >= 0.f && right <= worldWidth) {
                    add({right, minY}, {right, maxY});
                }
            }
            if (minX < maxX) {
                if (top >= 0.f && top <= worldHeight) {
                    add({minX, top}, {maxX, top});
                }
                if (bottom >= 0.f && bottom <= worldHeight) {
                    add({minX, bottom}, {maxX, bottom});
                }
            }

            if (total <= 0.f) {
                return;
            }
            float sample = std::uniform_real_distribution<float>(0.f, total)(random);
            for (std::size_t i = 0; i < count; ++i) {
                const auto& segment = segments[i];
                if (sample <= segment.length || i + 1 == count) {
                    const float t = std::clamp(sample / segment.length, 0.f, 1.f);
                    enemies.push_back(
                        {segment.start + (segment.end - segment.start) * t});
                    return;
                }
                sample -= segment.length;
            }
        };

        const auto centeredText = [&](meno::Renderer& renderer,
                                      const std::string& text, float y,
                                      unsigned int size) {
            meno::TextParams params{
                .characterSize = size, .color = meno::colors::White};
            params.origin = renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {480.f, y}, params);
        };

        meno::Scene& scene = game.scenes().createScene();
        game.renderer().setCamera(hud);

        scene.setUpdateHandler([&](double dt) {
            const auto& input = game.window().input();
            if (input.wasPressed(meno::Key::Escape)) {
                game.stop();
                return;
            }

            if (screen == Screen::Title) {
                titleTime += dt;
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

            const float step = static_cast<float>(dt);
            elapsed += dt;
            invincible = std::max(0.0, invincible - dt);

            meno::Vec2f movement{0.f, 0.f};
            if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left)) {
                movement.x -= 1.f;
            }
            if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right)) {
                movement.x += 1.f;
            }
            if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up)) {
                movement.y -= 1.f;
            }
            if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down)) {
                movement.y += 1.f;
            }
            player += normalized(movement) * (240.f * step);
            player.x = std::clamp(player.x, 16.f, worldWidth - 16.f);
            player.y = std::clamp(player.y, 16.f, worldHeight - 16.f);
            updateCamera();

            const meno::Vec2f mouse{input.mousePosition()};
            const meno::Vec2f target = game.renderer().screenToWorld(mouse);
            const meno::Vec2f difference = target - player;
            if (lengthSquared(difference) > 0.000001f) {
                aim = normalized(difference);
                playerAngle = std::atan2(aim.y, aim.x) * 180.f / pi;
            }

            for (auto& bullet : bullets) {
                bullet.position += bullet.velocity * step;
                bullet.age += dt;
                bullet.dead = bullet.age >= 1.4 ||
                              bullet.position.x < 0.f ||
                              bullet.position.x > worldWidth ||
                              bullet.position.y < 0.f ||
                              bullet.position.y > worldHeight;
            }
            std::erase_if(bullets, [](const Bullet& b) { return b.dead; });

            shootCooldown -= dt;
            const bool firing = input.isDown(meno::MouseButton::Left) ||
                                input.wasPressed(meno::MouseButton::Left);
            if (firing && shootCooldown <= 0.000000001) {
                bullets.push_back({player, aim * 600.f});
                playSound(shootSounds, nextShoot);
                shootCooldown = std::max(0.0, shootCooldown) + 0.15;
            } else if (!firing) {
                shootCooldown = std::max(0.0, shootCooldown);
            }

            spawnCountdown -= dt;
            if (spawnCountdown <= 0.000000001) {
                if (enemies.size() < 200) {
                    spawnEnemy();
                }
                const double interval =
                    std::max(0.3, 1.0 - 0.1 * std::floor(elapsed / 10.0));
                spawnCountdown += interval;
            }

            for (auto& enemy : enemies) {
                enemy.position += normalized(player - enemy.position) * (100.f * step);
            }

            for (auto& bullet : bullets) {
                for (auto& enemy : enemies) {
                    if (!enemy.dead &&
                        overlaps(bullet.position, enemy.position, 18.f)) {
                        bullet.dead = true;
                        enemy.dead = true;
                        score += 10;
                        playSound(hitSounds, nextHit);
                        break;
                    }
                }
            }

            if (invincible <= 0.0) {
                for (auto& enemy : enemies) {
                    if (!enemy.dead && overlaps(player, enemy.position, 28.f)) {
                        enemy.dead = true;
                        --hp;
                        invincible = 1.0;
                        playSound(loseSounds, nextLose);
                        break;
                    }
                }
            }

            std::erase_if(bullets, [](const Bullet& b) { return b.dead; });
            std::erase_if(enemies, [](const Enemy& e) { return e.dead; });

            if (hp <= 0) {
                screen = Screen::Result;
                music->stop();
                game.renderer().setCamera(hud);
            }
        });

        scene.setDrawHandler([&](meno::Renderer& renderer) {
            if (screen != Screen::Play) {
                renderer.setCamera(hud);
                renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                                  {.fill = background});
                if (screen == Screen::Title) {
                    centeredText(renderer, "ARENA", 220.f, 48);
                    centeredText(renderer, "PRESS ENTER", 310.f, 32);
                } else {
                    centeredText(renderer, "GAME OVER", 160.f, 48);
                    centeredText(renderer, "SCORE " + std::to_string(score), 240.f, 32);
                    centeredText(renderer, "TIME " + decimalTime(elapsed), 290.f, 32);
                    centeredText(renderer, "R: RESTART  ESC: QUIT", 380.f, 16);
                }
                return;
            }

            renderer.setCamera(camera);
            renderer.drawRect({{0.f, 0.f}, {worldWidth, worldHeight}},
                              {.fill = background});
            for (float x = 120.f; x < worldWidth; x += 120.f) {
                renderer.drawLine({x, 0.f}, {x, worldHeight},
                                  {.color = gridColor, .thickness = 1.f});
            }
            for (float y = 120.f; y < worldHeight; y += 120.f) {
                renderer.drawLine({0.f, y}, {worldWidth, y},
                                  {.color = gridColor, .thickness = 1.f});
            }
            renderer.drawRect({{0.f, 0.f}, {worldWidth, worldHeight}},
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

            const bool dim = invincible > 0.0 &&
                             static_cast<int>(invincible * 12.0) % 2 == 0;
            renderer.draw(*playerTexture,
                          {.position = player,
                           .origin = {16.f, 16.f},
                           .rotation = playerAngle,
                           .tint = dim ? meno::colors::White.withAlpha(96)
                                       : meno::colors::White});

            renderer.setCamera(hud);
            const std::string status =
                "HP " + std::to_string(hp) +
                "  SCORE " + std::to_string(score) +
                "  TIME " + decimalTime(elapsed);
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
