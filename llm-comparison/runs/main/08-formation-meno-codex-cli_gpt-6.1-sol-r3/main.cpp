#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

struct Enemy {
    meno::Rectf box;
    int row = 0;
    int column = 0;
    bool alive = true;
};

struct EnemyBullet {
    meno::Vec2f position;
    meno::Vec2f velocity;
    bool alive = true;
};

struct Bonus {
    meno::Rectf box;
    float velocity = 120.f;
};

bool circleHitsRect(meno::Vec2f center, float radius, const meno::Rectf& box) {
    const float x = std::clamp(center.x, box.position.x,
                               box.position.x + box.size.x);
    const float y = std::clamp(center.y, box.position.y,
                               box.position.y + box.size.y);
    const float dx = center.x - x;
    const float dy = center.y - y;
    return dx * dx + dy * dy <= radius * radius;
}

int runGame() {
    meno::Game game{{
        .application = {.fixedTimeStep = 1.0 / 60.0, .framerateLimit = 60},
        .windowSize = {960, 540},
        .title = "STAR FORMATION",
        .clearColor = meno::Color::fromRgb(0x000000)
    }};

    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    const auto enemyTexture =
        game.resources().loadTexture("assets/textures/fm_enemies.png");
    const auto playerTexture =
        game.resources().loadTexture("assets/textures/fm_player.png");
    const auto bonusTexture =
        game.resources().loadTexture("assets/textures/fm_bonus.png");
    const auto shootBuffer =
        game.resources().loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuffer =
        game.resources().loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuffer =
        game.resources().loadSoundBuffer("assets/sfx/lose.wav");
    const auto bonusBuffer =
        game.resources().loadSoundBuffer("assets/sfx/bonus.wav");
    auto music = meno::Music::openFromFile("assets/music/bgm_formation.ogg");

    bool missing = false;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
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
    require(music.has_value(), "assets/music/bgm_formation.ogg");
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
        .center = {480.f, 270.f},
        .size = {960.f, 540.f}
    };
    game.renderer().setCamera(camera);

    enum class Screen { Title, Play, Result };
    Screen screen = Screen::Title;
    double titleTime = 0.0;
    double animationTime = 0.0;
    double fireTime = 0.0;
    double bonusTime = 0.0;
    double freezeTime = 0.0;
    int score = 0;
    int wave = 1;
    int lives = 3;
    int direction = 1;
    bool nextBonusFromLeft = true;

    meno::Rectf player{{460.f, 490.f}, {40.f, 24.f}};
    std::array<Enemy, 32> enemies{};
    std::vector<meno::Rectf> walls;
    std::optional<meno::Rectf> playerBullet;
    std::vector<EnemyBullet> enemyBullets;
    std::optional<Bonus> bonus;
    walls.reserve(96);
    enemyBullets.reserve(3);

    std::mt19937 random{std::random_device{}()};
    std::uniform_int_distribution<int> bonusAward{1, 3};

    const auto remainingEnemies = [&] {
        return static_cast<int>(std::count_if(
            enemies.begin(), enemies.end(),
            [](const Enemy& enemy) { return enemy.alive; }));
    };

    const auto setupWave = [&] {
        direction = 1;
        fireTime = 0.0;
        animationTime = 0.0;
        playerBullet.reset();
        enemyBullets.clear();
        bonus.reset();

        const float offset = 16.f * static_cast<float>(std::min(wave - 1, 4));
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 8; ++column) {
                enemies[row * 8 + column] = Enemy{
                    {{296.f + 48.f * column, 80.f + 40.f * row + offset},
                     {32.f, 24.f}},
                    row, column, true
                };
            }
        }

        walls.clear();
        for (const float center : {192.f, 384.f, 576.f, 768.f}) {
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 6; ++column) {
                    walls.push_back({
                        {center - 24.f + 8.f * column, 400.f + 8.f * row},
                        {8.f, 8.f}
                    });
                }
            }
        }
    };

    const auto startGame = [&] {
        score = 0;
        wave = 1;
        lives = 3;
        bonusTime = 0.0;
        freezeTime = 0.0;
        nextBonusFromLeft = true;
        player.position = {460.f, 490.f};
        setupWave();
        shootSound.stop();
        hitSound.stop();
        loseSound.stop();
        bonusSound.stop();
        music->stop();
        music->play();
        screen = Screen::Play;
    };

    const auto gameOver = [&] {
        screen = Screen::Result;
        playerBullet.reset();
        enemyBullets.clear();
        music->stop();
    };

    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
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

        if (freezeTime > 0.0) {
            freezeTime = std::max(0.0, freezeTime - fixedDeltaTime);
            return;
        }

        const float dt = static_cast<float>(fixedDeltaTime);
        animationTime += fixedDeltaTime;

        float movement = 0.f;
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) {
            movement -= 1.f;
        }
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) {
            movement += 1.f;
        }
        player.position.x =
            std::clamp(player.position.x + movement * 300.f * dt, 16.f, 904.f);

        if (input.wasPressed(meno::Key::Space) && !playerBullet) {
            playerBullet = meno::Rectf{
                {player.position.x + 18.f, player.position.y - 12.f},
                {4.f, 12.f}
            };
            shootSound.play();
        }

        float left = 960.f;
        float right = 0.f;
        for (const Enemy& enemy : enemies) {
            if (enemy.alive) {
                left = std::min(left, enemy.box.position.x);
                right = std::max(right, enemy.box.position.x + 32.f);
            }
        }

        const float speed = 40.f + 4.f * (32 - remainingEnemies());
        float step = direction * speed * dt;
        bool turn = false;
        if (direction > 0 && right + step >= 944.f) {
            step = 944.f - right;
            turn = true;
        } else if (direction < 0 && left + step <= 16.f) {
            step = 16.f - left;
            turn = true;
        }

        for (Enemy& enemy : enemies) {
            if (!enemy.alive) {
                continue;
            }
            enemy.box.position.x += step;
            if (turn) {
                enemy.box.position.y += 16.f;
            }
            if (enemy.box.position.y + enemy.box.size.y >= 490.f) {
                gameOver();
                return;
            }
        }
        if (turn) {
            direction = -direction;
        }

        std::erase_if(walls, [&](const meno::Rectf& piece) {
            return std::any_of(enemies.begin(), enemies.end(),
                               [&](const Enemy& enemy) {
                return enemy.alive && enemy.box.intersects(piece);
            });
        });

        bonusTime += fixedDeltaTime;
        if (bonusTime >= 20.0) {
            bonusTime -= 20.0;
            bonus = Bonus{
                {{nextBonusFromLeft ? -48.f : 960.f, 50.f}, {48.f, 20.f}},
                nextBonusFromLeft ? 120.f : -120.f
            };
            nextBonusFromLeft = !nextBonusFromLeft;
        }
        if (bonus) {
            bonus->box.position.x += bonus->velocity * dt;
            if ((bonus->velocity > 0.f && bonus->box.position.x >= 960.f) ||
                (bonus->velocity < 0.f && bonus->box.position.x + 48.f <= 0.f)) {
                bonus.reset();
            }
        }

        if (playerBullet) {
            playerBullet->position.y -= 600.f * dt;

            for (auto it = walls.begin(); it != walls.end(); ++it) {
                if (playerBullet->intersects(*it)) {
                    walls.erase(it);
                    playerBullet.reset();
                    break;
                }
            }

            if (playerBullet) {
                // Check lower rows first: they are encountered first by an upward shot.
                for (auto it = enemies.rbegin(); it != enemies.rend(); ++it) {
                    if (it->alive && playerBullet->intersects(it->box)) {
                        it->alive = false;
                        score += it->row == 0 ? 30 : it->row == 1 ? 20 : 10;
                        playerBullet.reset();
                        hitSound.play();
                        break;
                    }
                }
            }

            if (playerBullet && bonus && playerBullet->intersects(bonus->box)) {
                score += bonusAward(random) * 50;
                playerBullet.reset();
                bonus.reset();
                bonusSound.play();
            }
            if (playerBullet &&
                playerBullet->position.y + playerBullet->size.y <= 0.f) {
                playerBullet.reset();
            }
        }

        if (remainingEnemies() == 0) {
            playerBullet.reset();
            enemyBullets.clear();
            bonus.reset();
            ++wave;
            setupWave();
            return;
        }

        bool playerWasHit = false;
        for (EnemyBullet& bullet : enemyBullets) {
            bullet.position += bullet.velocity * dt;
            for (auto it = walls.begin(); it != walls.end(); ++it) {
                if (circleHitsRect(bullet.position, 3.f, *it)) {
                    walls.erase(it);
                    bullet.alive = false;
                    break;
                }
            }
            if (!bullet.alive) {
                continue;
            }
            if (circleHitsRect(bullet.position, 3.f, player)) {
                playerWasHit = true;
                break;
            }
            if (bullet.position.x < -3.f || bullet.position.x > 963.f ||
                bullet.position.y < -3.f || bullet.position.y > 543.f) {
                bullet.alive = false;
            }
        }

        if (playerWasHit) {
            --lives;
            loseSound.play();
            playerBullet.reset();
            enemyBullets.clear();
            if (lives == 0) {
                gameOver();
            } else {
                freezeTime = 1.0;
            }
            return;
        }

        std::erase_if(enemyBullets, [](const EnemyBullet& bullet) {
            return !bullet.alive;
        });

        fireTime += fixedDeltaTime;
        if (fireTime >= 1.0) {
            fireTime -= 1.0;
            if (enemyBullets.size() < 3) {
                const Enemy* shooter = nullptr;
                float closest = 1.e9f;
                const float targetX = player.position.x + 20.f;

                // Ascending columns preserve the left-column tie breaker.
                for (int column = 0; column < 8; ++column) {
                    const Enemy* bottom = nullptr;
                    for (int row = 3; row >= 0; --row) {
                        const Enemy& enemy = enemies[row * 8 + column];
                        if (enemy.alive) {
                            bottom = &enemy;
                            break;
                        }
                    }
                    if (bottom) {
                        const float distance =
                            std::abs(bottom->box.position.x + 16.f - targetX);
                        if (distance < closest) {
                            closest = distance;
                            shooter = bottom;
                        }
                    }
                }

                if (shooter) {
                    const meno::Vec2f origin{
                        shooter->box.position.x + 16.f,
                        shooter->box.position.y + 24.f
                    };
                    const meno::Vec2f target{
                        player.position.x + 20.f, player.position.y + 12.f
                    };
                    const meno::Vec2f delta = target - origin;
                    const float length =
                        std::sqrt(delta.x * delta.x + delta.y * delta.y);
                    enemyBullets.push_back({
                        origin, delta * (240.f / length), true
                    });
                }
            }
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        const auto centeredText = [&](const std::string& text, float y,
                                      unsigned int size) {
            meno::TextParams params{
                .characterSize = size,
                .color = meno::colors::White
            };
            params.origin =
                renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {480.f, y}, params);
        };

        if (screen == Screen::Title) {
            centeredText("STAR FORMATION", 220.f, 48);
            centeredText("PRESS ENTER", 320.f, 32);
            return;
        }

        if (screen == Screen::Result) {
            centeredText("GAME OVER", 160.f, 48);
            centeredText("SCORE " + std::to_string(score), 240.f, 32);
            centeredText("WAVE " + std::to_string(wave), 292.f, 32);
            centeredText("R: RESTART  ESC: QUIT", 380.f, 32);
            return;
        }

        for (const meno::Rectf& piece : walls) {
            renderer.drawRect(piece, {.fill = meno::Color::fromRgb(0x4CC26B)});
        }

        const int frame = static_cast<int>(animationTime / 0.5) % 2;
        for (const Enemy& enemy : enemies) {
            if (!enemy.alive) {
                continue;
            }
            const int baseFrame = enemy.row == 0 ? 0 : enemy.row == 1 ? 2 : 4;
            renderer.draw(*enemyTexture, {
                .position = enemy.box.position,
                .source = meno::Recti{(baseFrame + frame) * 32, 0, 32, 24}
            });
        }

        if (bonus) {
            renderer.draw(*bonusTexture, {.position = bonus->box.position});
        }
        renderer.draw(*playerTexture, {.position = player.position});

        if (playerBullet) {
            renderer.drawRect(*playerBullet, {.fill = meno::colors::White});
        }
        for (const EnemyBullet& bullet : enemyBullets) {
            renderer.drawCircle(bullet.position, 3.f,
                                {.fill = meno::Color::fromRgb(0xFF6B6B)});
        }

        const meno::TextParams hud{
            .characterSize = 16,
            .color = meno::colors::White
        };
        renderer.drawText(*font, "SCORE " + std::to_string(score),
                          {16.f, 12.f}, hud);
        renderer.drawText(*font, "WAVE " + std::to_string(wave),
                          {432.f, 12.f}, hud);
        renderer.drawText(*font, "LIVES " + std::to_string(lives),
                          {816.f, 12.f}, hud);
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
