```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float Width = 960.f;
constexpr float Height = 540.f;

struct Enemy {
    int column = 0;
    int row = 0;
    bool alive = true;
};

struct EnemyBullet {
    meno::Vec2f position;
    meno::Vec2f velocity;
};

struct PlayerBullet {
    meno::Vec2f position;
    bool active = false;
};

struct Bonus {
    float x = -48.f;
    float direction = 1.f;
    bool active = false;
};

bool circleHitsRect(meno::Vec2f center, float radius, const meno::Rectf& box) {
    const float x = std::clamp(center.x, box.position.x, box.position.x + box.size.x);
    const float y = std::clamp(center.y, box.position.y, box.position.y + box.size.y);
    const float dx = center.x - x;
    const float dy = center.y - y;
    return dx * dx + dy * dy <= radius * radius;
}

int runGame() {
    meno::Game game{{
        .application = {.framerateLimit = 60},
        .windowSize = {960, 540},
        .title = "STAR FORMATION",
        .clearColor = meno::Color::fromRgb(0x000000)
    }};

    auto& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto enemyTexture = resources.loadTexture("assets/textures/fm_enemies.png");
    const auto playerTexture = resources.loadTexture("assets/textures/fm_player.png");
    const auto bonusTexture = resources.loadTexture("assets/textures/fm_bonus.png");
    const auto shootBuffer = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuffer = resources.loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuffer = resources.loadSoundBuffer("assets/sfx/lose.wav");
    const auto bonusBuffer = resources.loadSoundBuffer("assets/sfx/bonus.wav");
    auto music = meno::Music::openFromFile("assets/music/bgm_formation.ogg");

    bool missing = false;
    const auto require = [&](bool valid, const char* path) {
        if (!valid) {
            std::cerr << "Failed to load: " << path << '\n';
            missing = true;
        }
    };

    require(static_cast<bool>(font), "assets/fonts/game.ttf");
    require(static_cast<bool>(enemyTexture), "assets/textures/fm_enemies.png");
    require(static_cast<bool>(playerTexture), "assets/textures/fm_player.png");
    require(static_cast<bool>(bonusTexture), "assets/textures/fm_bonus.png");
    require(static_cast<bool>(shootBuffer), "assets/sfx/shoot.wav");
    require(static_cast<bool>(hitBuffer), "assets/sfx/hit.wav");
    require(static_cast<bool>(loseBuffer), "assets/sfx/lose.wav");
    require(static_cast<bool>(bonusBuffer), "assets/sfx/bonus.wav");
    require(static_cast<bool>(music), "assets/music/bgm_formation.ogg");
    if (missing) {
        return 1;
    }

    meno::Sound shootSound{shootBuffer};
    meno::Sound hitSound{hitBuffer};
    meno::Sound loseSound{loseBuffer};
    meno::Sound bonusSound{bonusBuffer};
    music->setVolume(0.4f);
    music->setLooping(true);

    const meno::Camera2D camera{
        .center = {Width / 2.f, Height / 2.f},
        .size = {Width, Height}
    };
    game.renderer().setCamera(camera);

    enum class Screen { Title, Play, Result };
    Screen screen = Screen::Title;

    std::array<Enemy, 32> enemies{};
    std::vector<meno::Rectf> wallPieces;
    std::vector<EnemyBullet> enemyBullets;
    enemyBullets.reserve(3);
    wallPieces.reserve(96);

    PlayerBullet playerBullet;
    Bonus bonus;
    meno::Vec2f player{460.f, 490.f};

    int score = 0;
    int wave = 1;
    int lives = 3;
    int remaining = 32;

    float formationX = 0.f;
    float formationY = 0.f;
    float formationDirection = 1.f;

    double titleTime = 0.0;
    double waveTime = 0.0;
    double shotTimer = 1.0;
    double bonusTimer = 20.0;
    double freezeTime = 0.0;
    bool nextBonusFromLeft = true;

    std::mt19937 random{std::random_device{}()};
    std::uniform_int_distribution<int> bonusReward{1, 3};

    const auto enemyBox = [&](const Enemy& enemy) {
        return meno::Rectf{
            {296.f + 48.f * static_cast<float>(enemy.column) + formationX,
             80.f + 40.f * static_cast<float>(enemy.row) + formationY},
            {32.f, 24.f}
        };
    };

    const auto playerBox = [&] {
        return meno::Rectf{player, {40.f, 24.f}};
    };

    const auto bulletBox = [&] {
        return meno::Rectf{playerBullet.position, {4.f, 12.f}};
    };

    const auto fillWalls = [&] {
        wallPieces.clear();
        for (int wall = 0; wall < 4; ++wall) {
            const float left = 192.f * static_cast<float>(wall + 1) - 24.f;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 6; ++column) {
                    wallPieces.push_back({
                        {left + 8.f * static_cast<float>(column),
                         400.f + 8.f * static_cast<float>(row)},
                        {8.f, 8.f}
                    });
                }
            }
        }
    };

    const auto startWave = [&] {
        int index = 0;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 8; ++column) {
                enemies[index++] = Enemy{column, row, true};
            }
        }
        remaining = 32;
        formationX = 0.f;
        formationY = 16.f * static_cast<float>(std::min(wave - 1, 4));
        formationDirection = 1.f;
        waveTime = 0.0;
        shotTimer = 1.0;
        playerBullet = {};
        enemyBullets.clear();
        bonus = {};
        fillWalls();
    };

    const auto startGame = [&] {
        shootSound.stop();
        hitSound.stop();
        loseSound.stop();
        bonusSound.stop();
        music->stop();

        score = 0;
        wave = 1;
        lives = 3;
        player = {460.f, 490.f};
        freezeTime = 0.0;
        bonusTimer = 20.0;
        nextBonusFromLeft = true;
        startWave();
        screen = Screen::Play;
        music->play();
    };

    const auto finishGame = [&] {
        screen = Screen::Result;
        music->stop();
    };

    const auto loseLife = [&] {
        --lives;
        loseSound.play();
        playerBullet = {};
        enemyBullets.clear();
        if (lives <= 0) {
            finishGame();
        } else {
            freezeTime = 1.0;
        }
    };

    const auto fireEnemyBullet = [&] {
        if (enemyBullets.size() >= 3) {
            return;
        }

        const meno::Vec2f target{player.x + 20.f, player.y + 12.f};
        const Enemy* shooter = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        // Ascending columns resolve equal distances in favor of the left column.
        for (int column = 0; column < 8; ++column) {
            const Enemy* bottom = nullptr;
            for (const Enemy& enemy : enemies) {
                if (enemy.alive && enemy.column == column &&
                    (!bottom || enemy.row > bottom->row)) {
                    bottom = &enemy;
                }
            }
            if (!bottom) {
                continue;
            }
            const auto box = enemyBox(*bottom);
            const float distance = std::abs(box.position.x + 16.f - target.x);
            if (distance < bestDistance) {
                bestDistance = distance;
                shooter = bottom;
            }
        }

        if (shooter) {
            const auto box = enemyBox(*shooter);
            const meno::Vec2f origin{box.position.x + 16.f, box.position.y + 24.f};
            const meno::Vec2f delta = target - origin;
            const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            const meno::Vec2f velocity =
                length > 0.f ? delta * (240.f / length) : meno::Vec2f{0.f, 240.f};
            enemyBullets.push_back({origin, velocity});
        }
    };

    // Small simulation steps also keep diagonal enemy shots from skipping wall pieces.
    const auto simulate = [&](float dt, float movement) {
        player.x = std::clamp(player.x + movement * 300.f * dt, 16.f, 904.f);

        waveTime += static_cast<double>(dt);
        const float speed = 40.f + 4.f * static_cast<float>(32 - remaining);
        formationX += formationDirection * speed * dt;

        float left = Width;
        float right = 0.f;
        for (const Enemy& enemy : enemies) {
            if (enemy.alive) {
                const auto box = enemyBox(enemy);
                left = std::min(left, box.position.x);
                right = std::max(right, box.position.x + box.size.x);
            }
        }

        if (formationDirection > 0.f && right >= 944.f) {
            formationX -= right - 944.f;
            formationY += 16.f;
            formationDirection = -1.f;
        } else if (formationDirection < 0.f && left <= 16.f) {
            formationX += 16.f - left;
            formationY += 16.f;
            formationDirection = 1.f;
        }

        for (const Enemy& enemy : enemies) {
            if (enemy.alive && enemyBox(enemy).position.y + 24.f >= 490.f) {
                finishGame();
                return;
            }
        }

        std::erase_if(wallPieces, [&](const meno::Rectf& piece) {
            for (const Enemy& enemy : enemies) {
                if (enemy.alive && enemyBox(enemy).intersects(piece)) {
                    return true;
                }
            }
            return false;
        });

        shotTimer -= static_cast<double>(dt);
        if (shotTimer <= 1e-7) {
            shotTimer += 1.0;
            fireEnemyBullet();
        }

        bonusTimer -= static_cast<double>(dt);
        if (bonusTimer <= 1e-7) {
            bonusTimer += 20.0;
            bonus.active = true;
            bonus.x = nextBonusFromLeft ? -48.f : 960.f;
            bonus.direction = nextBonusFromLeft ? 1.f : -1.f;
            nextBonusFromLeft = !nextBonusFromLeft;
        }

        if (bonus.active) {
            bonus.x += bonus.direction * 120.f * dt;
            if ((bonus.direction > 0.f && bonus.x >= Width) ||
                (bonus.direction < 0.f && bonus.x + 48.f <= 0.f)) {
                bonus.active = false;
            }
        }

        if (playerBullet.active) {
            playerBullet.position.y -= 600.f * dt;

            for (auto it = wallPieces.begin(); it != wallPieces.end(); ++it) {
                if (bulletBox().intersects(*it)) {
                    wallPieces.erase(it);
                    playerBullet.active = false;
                    break;
                }
            }

            if (playerBullet.active) {
                // Lower rows are encountered first by an upward-moving shot.
                for (int i = 31; i >= 0; --i) {
                    Enemy& enemy = enemies[static_cast<std::size_t>(i)];
                    if (enemy.alive && bulletBox().intersects(enemyBox(enemy))) {
                        enemy.alive = false;
                        --remaining;
                        score += enemy.row == 0 ? 30 : (enemy.row == 1 ? 20 : 10);
                        playerBullet.active = false;
                        hitSound.play();
                        break;
                    }
                }
            }

            if (playerBullet.active && bonus.active &&
                bulletBox().intersects({{bonus.x, 50.f}, {48.f, 20.f}})) {
                playerBullet.active = false;
                bonus.active = false;
                score += 50 * bonusReward(random);
                bonusSound.play();
            }

            if (playerBullet.active && playerBullet.position.y + 12.f <= 0.f) {
                playerBullet.active = false;
            }
        }

        for (std::size_t i = 0; i < enemyBullets.size();) {
            auto& bullet = enemyBullets[i];
            bullet.position += bullet.velocity * dt;

            bool destroyed = false;
            for (auto it = wallPieces.begin(); it != wallPieces.end(); ++it) {
                if (circleHitsRect(bullet.position, 3.f, *it)) {
                    wallPieces.erase(it);
                    destroyed = true;
                    break;
                }
            }

            if (!destroyed && circleHitsRect(bullet.position, 3.f, playerBox())) {
                loseLife();
                return; // loseLife invalidates all bullet references.
            }

            if (!destroyed) {
                destroyed = bullet.position.x < -3.f || bullet.position.x > Width + 3.f ||
                            bullet.position.y < -3.f || bullet.position.y > Height + 3.f;
            }

            if (destroyed) {
                enemyBullets.erase(enemyBullets.begin() +
                                   static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }

        if (remaining == 0) {
            ++wave;
            startWave();
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
                startGame();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (input.wasPressed(meno::Key::R)) {
                startGame();
            }
            return;
        }

        double activeTime = fixedDeltaTime;
        if (freezeTime > 0.0) {
            const double frozen = std::min(freezeTime, activeTime);
            freezeTime -= frozen;
            activeTime -= frozen;
            if (activeTime <= 1e-9) {
                return;
            }
        }

        float movement = 0.f;
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) {
            movement -= 1.f;
        }
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) {
            movement += 1.f;
        }

        if (input.wasPressed(meno::Key::Space) && !playerBullet.active) {
            playerBullet.position = {player.x + 18.f, player.y - 12.f};
            playerBullet.active = true;
            shootSound.play();
        }

        const int steps = std::max(1, static_cast<int>(std::ceil(activeTime * 240.0)));
        const float dt = static_cast<float>(activeTime / static_cast<double>(steps));
        for (int i = 0; i < steps; ++i) {
            simulate(dt, movement);
            if (screen != Screen::Play || freezeTime > 0.0) {
                break;
            }
        }
    });

    const auto centeredText = [&](meno::Renderer& renderer, const std::string& text,
                                  float y, unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = meno::colors::White};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {480.f, y}, params);
    };

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);

        if (screen == Screen::Title) {
            centeredText(renderer, "STAR FORMATION", 220.f, 48);
            centeredText(renderer, "PRESS ENTER", 310.f, 32);
            return;
        }

        if (screen == Screen::Result) {
            centeredText(renderer, "GAME OVER", 170.f, 48);
            centeredText(renderer, "SCORE " + std::to_string(score), 250.f, 32);
            centeredText(renderer, "WAVE " + std::to_string(wave), 300.f, 32);
            centeredText(renderer, "R: RESTART  ESC: QUIT", 390.f, 32);
            return;
        }

        for (const auto& piece : wallPieces) {
            renderer.drawRect(piece, {.fill = meno::Color::fromRgb(0x4CC26B)});
        }

        const int animationFrame = static_cast<int>(waveTime / 0.5) % 2;
        for (const Enemy& enemy : enemies) {
            if (!enemy.alive) {
                continue;
            }
            const int baseFrame = enemy.row == 0 ? 0 : (enemy.row == 1 ? 2 : 4);
            renderer.draw(*enemyTexture, {
                .position = enemyBox(enemy).position,
                .source = meno::Recti{(baseFrame + animationFrame) * 32, 0, 32, 24}
            });
        }

        if (bonus.active) {
            renderer.draw(*bonusTexture, {.position = {bonus.x, 50.f}});
        }

        renderer.draw(*playerTexture, {.position = player});

        if (playerBullet.active) {
            renderer.drawRect(bulletBox(), {.fill = meno::colors::White});
        }
        for (const auto& bullet : enemyBullets) {
            renderer.drawCircle(bullet.position, 3.f,
                                {.fill = meno::Color::fromRgb(0xFF6B6B)});
        }

        renderer.drawText(*font, "SCORE " + std::to_string(score), {16.f, 8.f},
                          {.characterSize = 32, .color = meno::colors::White});
        centeredText(renderer, "WAVE " + std::to_string(wave), 24.f, 32);

        const std::string livesText = "LIVES " + std::to_string(lives);
        meno::TextParams params{.characterSize = 32, .color = meno::colors::White};
        const auto bounds = renderer.measureTextBounds(*font, livesText, params);
        params.origin = {bounds.position.x + bounds.size.x, bounds.position.y};
        renderer.drawText(*font, livesText, {944.f, 8.f}, params);
    });

    scene.setExitHandler([&] { music->stop(); });
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