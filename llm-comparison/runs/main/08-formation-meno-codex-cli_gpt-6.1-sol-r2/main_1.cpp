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

struct Enemy {
    meno::Vec2f position;
    int row;
    int column;
    bool alive = true;
};

struct Brick {
    meno::Rectf box;
    bool alive = true;
};

struct EnemyBullet {
    meno::Vec2f position;
    meno::Vec2f velocity;
    bool alive = true;
};

bool circleHitsRect(meno::Vec2f center, float radius, const meno::Rectf& rect) {
    const float x = std::clamp(center.x, rect.position.x,
                               rect.position.x + rect.size.x);
    const float y = std::clamp(center.y, rect.position.y,
                               rect.position.y + rect.size.y);
    const float dx = center.x - x;
    const float dy = center.y - y;
    return dx * dx + dy * dy <= radius * radius;
}

} // namespace

int main() {
    meno::Game game{{
        .application = {.fixedTimeStep = 1.0 / 60.0, .framerateLimit = 60},
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

    bool failed = false;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(bool(font), "assets/fonts/game.ttf");
    require(bool(enemyTexture), "assets/textures/fm_enemies.png");
    require(bool(playerTexture), "assets/textures/fm_player.png");
    require(bool(bonusTexture), "assets/textures/fm_bonus.png");
    require(bool(shootBuffer), "assets/sfx/shoot.wav");
    require(bool(hitBuffer), "assets/sfx/hit.wav");
    require(bool(loseBuffer), "assets/sfx/lose.wav");
    require(bool(bonusBuffer), "assets/sfx/bonus.wav");
    require(bool(music), "assets/music/bgm_formation.ogg");
    if (failed) {
        return 1;
    }

    meno::Sound shootSound{shootBuffer};
    meno::Sound hitSound{hitBuffer};
    meno::Sound loseSound{loseBuffer};
    meno::Sound bonusSound{bonusBuffer};
    music->setVolume(0.4f);
    music->setLooping(true);

    enum class Screen { Title, Play, Result };
    Screen screen = Screen::Title;

    int score = 0;
    int wave = 1;
    int lives = 3;
    float playerX = 460.f;
    float direction = 1.f;
    double titleTime = 0.0;
    double animationTime = 0.0;
    double shotTime = 0.0;
    double bonusTime = 0.0;
    double freezeTime = 0.0;
    bool nextBonusFromLeft = true;
    bool bonusAlive = false;
    float bonusX = -48.f;
    float bonusDirection = 1.f;
    bool playerBulletAlive = false;
    meno::Rectf playerBullet{{0.f, 0.f}, {4.f, 12.f}};

    std::vector<Enemy> enemies;
    std::vector<Brick> bricks;
    std::vector<EnemyBullet> enemyBullets;
    std::mt19937 random{std::random_device{}()};
    std::uniform_int_distribution<int> bonusPrize{1, 3};

    const auto remainingEnemies = [&]() {
        return static_cast<int>(std::count_if(
            enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; }));
    };

    const auto clearBullets = [&]() {
        playerBulletAlive = false;
        enemyBullets.clear();
    };

    const auto startWave = [&]() {
        enemies.clear();
        bricks.clear();
        clearBullets();
        bonusAlive = false;
        direction = 1.f;
        animationTime = 0.0;
        shotTime = 0.0;
        const float offset = 16.f * static_cast<float>(std::min(wave - 1, 4));
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 8; ++c) {
                enemies.push_back({
                    {296.f + 48.f * c, 80.f + 40.f * r + offset}, r, c, true
                });
            }
        }
        for (float center : {192.f, 384.f, 576.f, 768.f}) {
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 6; ++c) {
                    bricks.push_back({
                        {{center - 24.f + 8.f * c, 400.f + 8.f * r}, {8.f, 8.f}},
                        true
                    });
                }
            }
        }
    };

    const auto startGame = [&]() {
        score = 0;
        wave = 1;
        lives = 3;
        playerX = 460.f;
        freezeTime = 0.0;
        bonusTime = 0.0;
        nextBonusFromLeft = true;
        shootSound.stop();
        hitSound.stop();
        loseSound.stop();
        bonusSound.stop();
        startWave();
        screen = Screen::Play;
        music->stop();
        music->play();
    };

    const auto finishGame = [&]() {
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
        float movement = 0.f;
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) {
            movement -= 1.f;
        }
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) {
            movement += 1.f;
        }
        playerX = std::clamp(playerX + movement * 300.f * dt, 16.f, 904.f);

        if (input.wasPressed(meno::Key::Space) && !playerBulletAlive) {
            playerBulletAlive = true;
            playerBullet.position = {playerX + 18.f, 478.f};
            shootSound.play();
        }

        animationTime += fixedDeltaTime;
        const int aliveCount = remainingEnemies();
        const float speed = 40.f + 4.f * static_cast<float>(32 - aliveCount);
        float left = std::numeric_limits<float>::max();
        float right = std::numeric_limits<float>::lowest();
        for (const auto& enemy : enemies) {
            if (enemy.alive) {
                left = std::min(left, enemy.position.x);
                right = std::max(right, enemy.position.x + 32.f);
            }
        }

        float displacement = direction * speed * dt;
        bool descend = false;
        if (direction > 0.f && right + displacement >= 944.f) {
            displacement = 944.f - right;
            descend = true;
        } else if (direction < 0.f && left + displacement <= 16.f) {
            displacement = 16.f - left;
            descend = true;
        }
        for (auto& enemy : enemies) {
            if (!enemy.alive) {
                continue;
            }
            enemy.position.x += displacement;
            if (descend) {
                enemy.position.y += 16.f;
            }
            if (enemy.position.y + 24.f >= 490.f) {
                finishGame();
                return;
            }
            const meno::Rectf box{enemy.position, {32.f, 24.f}};
            for (auto& brick : bricks) {
                if (brick.alive && box.intersects(brick.box)) {
                    brick.alive = false;
                }
            }
        }
        if (descend) {
            direction = -direction;
        }

        bonusTime += fixedDeltaTime;
        while (bonusTime >= 20.0) {
            bonusTime -= 20.0;
            bonusAlive = true;
            bonusDirection = nextBonusFromLeft ? 1.f : -1.f;
            bonusX = nextBonusFromLeft ? -48.f : 960.f;
            nextBonusFromLeft = !nextBonusFromLeft;
        }
        if (bonusAlive) {
            bonusX += bonusDirection * 120.f * dt;
            if ((bonusDirection > 0.f && bonusX >= 960.f) ||
                (bonusDirection < 0.f && bonusX + 48.f <= 0.f)) {
                bonusAlive = false;
            }
        }

        shotTime += fixedDeltaTime;
        while (shotTime >= 1.0) {
            shotTime -= 1.0;
            if (enemyBullets.size() >= 3) {
                continue;
            }

            const Enemy* shooter = nullptr;
            float bestDistance = std::numeric_limits<float>::max();
            for (int c = 0; c < 8; ++c) {
                const Enemy* bottom = nullptr;
                for (const auto& enemy : enemies) {
                    if (enemy.alive && enemy.column == c &&
                        (!bottom || enemy.row > bottom->row)) {
                        bottom = &enemy;
                    }
                }
                if (!bottom) {
                    continue;
                }
                const float distance =
                    std::abs(bottom->position.x + 16.f - (playerX + 20.f));
                if (distance < bestDistance) {
                    bestDistance = distance;
                    shooter = bottom;
                }
            }
            if (shooter) {
                const meno::Vec2f origin{
                    shooter->position.x + 16.f, shooter->position.y + 24.f
                };
                const meno::Vec2f aim{playerX + 20.f - origin.x, 502.f - origin.y};
                const float length = std::sqrt(aim.x * aim.x + aim.y * aim.y);
                const meno::Vec2f velocity = length > 0.f
                    ? aim * (240.f / length)
                    : meno::Vec2f{0.f, 240.f};
                enemyBullets.push_back({origin, velocity, true});
            }
        }

        // Small projectile steps preserve the first obstruction, including 8px bricks.
        constexpr int projectileSteps = 5;
        const float projectileDt = dt / static_cast<float>(projectileSteps);
        const meno::Rectf playerBox{{playerX, 490.f}, {40.f, 24.f}};

        for (int step = 0; step < projectileSteps; ++step) {
            if (playerBulletAlive) {
                playerBullet.position.y -= 600.f * projectileDt;
                for (auto& brick : bricks) {
                    if (brick.alive && playerBullet.intersects(brick.box)) {
                        brick.alive = false;
                        playerBulletAlive = false;
                        break;
                    }
                }
                if (playerBulletAlive) {
                    for (auto& enemy : enemies) {
                        if (enemy.alive &&
                            playerBullet.intersects(
                                meno::Rectf{enemy.position, {32.f, 24.f}})) {
                            enemy.alive = false;
                            playerBulletAlive = false;
                            score += enemy.row == 0 ? 30 : enemy.row == 1 ? 20 : 10;
                            hitSound.play();
                            break;
                        }
                    }
                }
                if (playerBulletAlive && bonusAlive &&
                    playerBullet.intersects(
                        meno::Rectf{{bonusX, 50.f}, {48.f, 20.f}})) {
                    playerBulletAlive = false;
                    bonusAlive = false;
                    score += 50 * bonusPrize(random);
                    bonusSound.play();
                }
                if (playerBullet.position.y + 12.f <= 0.f) {
                    playerBulletAlive = false;
                }
            }

            if (remainingEnemies() == 0) {
                ++wave;
                startWave();
                return;
            }

            for (auto& bullet : enemyBullets) {
                if (!bullet.alive) {
                    continue;
                }
                bullet.position += bullet.velocity * projectileDt;
                for (auto& brick : bricks) {
                    if (brick.alive && circleHitsRect(bullet.position, 3.f, brick.box)) {
                        brick.alive = false;
                        bullet.alive = false;
                        break;
                    }
                }
                if (bullet.alive && circleHitsRect(bullet.position, 3.f, playerBox)) {
                    --lives;
                    loseSound.play();
                    clearBullets();
                    if (lives == 0) {
                        finishGame();
                    } else {
                        freezeTime = 1.0;
                    }
                    return;
                }
                if (bullet.position.y - 3.f > 540.f ||
                    bullet.position.y + 3.f < 0.f ||
                    bullet.position.x + 3.f < 0.f ||
                    bullet.position.x - 3.f > 960.f) {
                    bullet.alive = false;
                }
            }
        }
        std::erase_if(enemyBullets, [](const EnemyBullet& bullet) {
            return !bullet.alive;
        });
    });

    const auto centeredText = [&](meno::Renderer& renderer, const std::string& text,
                                  float y, unsigned int size) {
        meno::TextParams params{
            .characterSize = size,
            .color = meno::colors::White
        };
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {480.f, y}, params);
    };

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        if (screen == Screen::Title) {
            centeredText(renderer, "STAR FORMATION", 220.f, 48);
            centeredText(renderer, "PRESS ENTER", 310.f, 32);
            return;
        }
        if (screen == Screen::Result) {
            centeredText(renderer, "GAME OVER", 170.f, 48);
            centeredText(renderer, "SCORE " + std::to_string(score), 250.f, 32);
            centeredText(renderer, "WAVE " + std::to_string(wave), 300.f, 32);
            centeredText(renderer, "R: RESTART  ESC: QUIT", 380.f, 16);
            return;
        }

        for (const auto& brick : bricks) {
            if (brick.alive) {
                renderer.drawRect(brick.box, {
                    .fill = meno::Color::fromRgb(0x4CC26B)
                });
            }
        }

        const int animationFrame =
            static_cast<int>(std::fmod(animationTime, 1.0) >= 0.5);
        for (const auto& enemy : enemies) {
            if (!enemy.alive) {
                continue;
            }
            const int baseFrame = enemy.row == 0 ? 0 : enemy.row == 1 ? 2 : 4;
            renderer.draw(*enemyTexture, {
                .position = enemy.position,
                .source = meno::Recti{32 * (baseFrame + animationFrame), 0, 32, 24}
            });
        }
        if (bonusAlive) {
            renderer.draw(*bonusTexture, {.position = {bonusX, 50.f}});
        }
        renderer.draw(*playerTexture, {.position = {playerX, 490.f}});

        if (playerBulletAlive) {
            renderer.drawRect(playerBullet, {.fill = meno::colors::White});
        }
        for (const auto& bullet : enemyBullets) {
            if (bullet.alive) {
                renderer.drawCircle(bullet.position, 3.f, {
                    .fill = meno::Color::fromRgb(0xFF6B6B)
                });
            }
        }

        renderer.drawText(*font, "SCORE " + std::to_string(score), {16.f, 12.f},
                          {.characterSize = 16, .color = meno::colors::White});
        renderer.drawText(*font, "WAVE " + std::to_string(wave), {432.f, 12.f},
                          {.characterSize = 16, .color = meno::colors::White});
        renderer.drawText(*font, "LIVES " + std::to_string(lives), {800.f, 12.f},
                          {.characterSize = 16, .color = meno::colors::White});
    });

    scene.setExitHandler([&]() { music->stop(); });
    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}
