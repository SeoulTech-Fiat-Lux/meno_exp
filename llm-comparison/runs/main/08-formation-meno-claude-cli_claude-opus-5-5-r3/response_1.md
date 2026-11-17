```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWidth = 960.f;
constexpr float kHeight = 540.f;

constexpr int kCols = 8;
constexpr int kRows = 4;
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;

constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerY = 490.f;
constexpr float kPlayerSpeed = 300.f;

constexpr float kBulletW = 4.f;
constexpr float kBulletH = 12.f;
constexpr float kBulletSpeed = 600.f;

constexpr float kEnemyBulletRadius = 3.f;
constexpr float kEnemyBulletSpeed = 240.f;
constexpr std::size_t kMaxEnemyBullets = 3;

constexpr float kPieceSize = 8.f;
constexpr float kShieldTop = 400.f;

constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusY = 50.f;
constexpr float kBonusSpeed = 120.f;

struct Enemy {
    int col = 0;
    int row = 0;
    meno::Vec2f pos;
};

struct EnemyBullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
};

struct Bonus {
    meno::Vec2f pos;
    float vx = 0.f;
};

bool circleRect(meno::Vec2f c, float r, const meno::Rectf& rect) {
    const float cx = std::clamp(c.x, rect.left(), rect.right());
    const float cy = std::clamp(c.y, rect.top(), rect.bottom());
    const float dx = c.x - cx;
    const float dy = c.y - cy;
    return dx * dx + dy * dy < r * r;
}

int enemyPoints(int row) {
    if (row == 0) {
        return 30;
    }
    if (row == 1) {
        return 20;
    }
    return 10;
}

int enemyBaseCell(int row) {
    if (row == 0) {
        return 0;
    }
    if (row == 1) {
        return 2;
    }
    return 4;
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "STAR FORMATION",
                     .clearColor = meno::colors::Black}};

    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto enemiesTex = resources.loadTexture("assets/textures/fm_enemies.png");
    const auto playerTex = resources.loadTexture("assets/textures/fm_player.png");
    const auto bonusTex = resources.loadTexture("assets/textures/fm_bonus.png");
    const auto shootBuf = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuf = resources.loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuf = resources.loadSoundBuffer("assets/sfx/lose.wav");
    const auto bonusBuf = resources.loadSoundBuffer("assets/sfx/bonus.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_formation.ogg");

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(enemiesTex != nullptr, "assets/textures/fm_enemies.png");
    require(playerTex != nullptr, "assets/textures/fm_player.png");
    require(bonusTex != nullptr, "assets/textures/fm_bonus.png");
    require(shootBuf != nullptr, "assets/sfx/shoot.wav");
    require(hitBuf != nullptr, "assets/sfx/hit.wav");
    require(loseBuf != nullptr, "assets/sfx/lose.wav");
    require(bonusBuf != nullptr, "assets/sfx/bonus.wav");
    require(bgm.has_value(), "assets/music/bgm_formation.ogg");
    if (failed) {
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    meno::Sound shootSound{shootBuf};
    std::vector<meno::Sound> hitSounds;
    for (int i = 0; i < 3; ++i) {
        hitSounds.emplace_back(hitBuf);
    }
    std::size_t nextHit = 0;
    meno::Sound loseSound{loseBuf};
    meno::Sound bonusSound{bonusBuf};

    std::mt19937 rng{std::random_device{}()};

    // ---- 게임 상태 ----
    int score = 0;
    int wave = 1;
    int lives = 3;
    float playerX = kWidth / 2.f - kPlayerW / 2.f;
    std::optional<meno::Vec2f> playerBullet;
    std::vector<EnemyBullet> enemyBullets;
    std::vector<Enemy> enemies;
    std::vector<meno::Rectf> pieces;
    std::optional<Bonus> bonus;
    float formationDir = 1.f;
    double animTimer = 0.0;
    int animFrame = 0;
    double fireTimer = 1.0;
    double bonusTimer = 20.0;
    bool bonusFromLeft = true;
    double freezeTimer = 0.0;
    double titleTimer = 0.0;

    const auto startWave = [&] {
        enemies.clear();
        const float offset = std::min(16.f * static_cast<float>(wave - 1), 64.f);
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                enemies.push_back(
                    {c, r,
                     {296.f + 48.f * static_cast<float>(c), 80.f + 40.f * static_cast<float>(r) + offset}});
            }
        }
        formationDir = 1.f;
        pieces.clear();
        for (const float center : {192.f, 384.f, 576.f, 768.f}) {
            const float left = center - 24.f;
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 6; ++px) {
                    pieces.push_back(meno::Rectf{
                        {left + kPieceSize * static_cast<float>(px), kShieldTop + kPieceSize * static_cast<float>(py)},
                        {kPieceSize, kPieceSize}});
                }
            }
        }
        playerBullet.reset();
        enemyBullets.clear();
        bonus.reset();
        fireTimer = 1.0;
        animTimer = 0.0;
        animFrame = 0;
    };

    const auto resetGame = [&] {
        score = 0;
        wave = 1;
        lives = 3;
        playerX = kWidth / 2.f - kPlayerW / 2.f;
        bonusTimer = 20.0;
        bonusFromLeft = true;
        freezeTimer = 0.0;
        startWave();
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text, meno::Vec2f pos,
                                  unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = meno::colors::White};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, pos, params);
    };

    const auto escapeCheck = [&] {
        if (game.window().input().wasPressed(meno::Key::Escape)) {
            game.stop();
            return true;
        }
        return false;
    };

    // ---- 타이틀 ----
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        if (escapeCheck()) {
            return;
        }
        titleTimer += dt;
        if (game.window().input().wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawCentered(renderer, "STAR FORMATION", {kWidth / 2.f, 220.f}, 48);
        drawCentered(renderer, "PRESS ENTER", {kWidth / 2.f, 320.f}, 32);
    });

    // ---- 플레이 ----
    play.setEnterHandler([&] {
        resetGame();
        bgm->stop();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });

    play.setUpdateHandler([&](double fixedDeltaTime) {
        if (escapeCheck()) {
            return;
        }
        const meno::Input& input = game.window().input();
        const float dt = static_cast<float>(fixedDeltaTime);

        if (freezeTimer > 0.0) {
            freezeTimer -= fixedDeltaTime;
            return;
        }

        // 플레이어 이동
        float move = 0.f;
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) {
            move -= 1.f;
        }
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) {
            move += 1.f;
        }
        playerX += move * kPlayerSpeed * dt;
        playerX = std::clamp(playerX, 16.f, kWidth - 16.f - kPlayerW);
        const meno::Rectf playerRect{{playerX, kPlayerY}, {kPlayerW, kPlayerH}};
        const meno::Vec2f playerCenter = playerRect.center();

        // 사격
        if (input.wasPressed(meno::Key::Space) && !playerBullet) {
            playerBullet = meno::Vec2f{playerX + kPlayerW / 2.f - kBulletW / 2.f, kPlayerY - kBulletH};
            shootSound.play();
        }

        // 애니메이션
        animTimer += fixedDeltaTime;
        while (animTimer >= 0.5) {
            animTimer -= 0.5;
            animFrame ^= 1;
        }

        // 보너스 비행체 생성
        bonusTimer -= fixedDeltaTime;
        if (bonusTimer <= 0.0) {
            bonusTimer += 20.0;
            if (bonusFromLeft) {
                bonus = Bonus{{-kBonusW, kBonusY}, kBonusSpeed};
            } else {
                bonus = Bonus{{kWidth, kBonusY}, -kBonusSpeed};
            }
            bonusFromLeft = !bonusFromLeft;
        }

        // 적 사격
        fireTimer -= fixedDeltaTime;
        if (fireTimer <= 0.0) {
            fireTimer += 1.0;
            if (!enemies.empty() && enemyBullets.size() < kMaxEnemyBullets) {
                int bestCol = -1;
                float bestDist = 0.f;
                for (int c = 0; c < kCols; ++c) {
                    const auto it = std::find_if(enemies.begin(), enemies.end(),
                                                 [c](const Enemy& e) { return e.col == c; });
                    if (it == enemies.end()) {
                        continue;
                    }
                    const float d = std::abs(it->pos.x + kEnemyW / 2.f - playerCenter.x);
                    if (bestCol < 0 || d < bestDist) {
                        bestCol = c;
                        bestDist = d;
                    }
                }
                const Enemy* shooter = nullptr;
                for (const Enemy& e : enemies) {
                    if (e.col == bestCol && (shooter == nullptr || e.pos.y > shooter->pos.y)) {
                        shooter = &e;
                    }
                }
                if (shooter != nullptr) {
                    const meno::Vec2f origin{shooter->pos.x + kEnemyW / 2.f, shooter->pos.y + kEnemyH};
                    const meno::Vec2f diff = playerCenter - origin;
                    const float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                    meno::Vec2f vel{0.f, kEnemyBulletSpeed};
                    if (len > 0.0001f) {
                        vel = diff * (kEnemyBulletSpeed / len);
                    }
                    enemyBullets.push_back({origin, vel});
                }
            }
        }

        // 대열 이동
        if (!enemies.empty()) {
            const float speed = 40.f + 4.f * static_cast<float>(32 - static_cast<int>(enemies.size()));
            const float dx = formationDir * speed * dt;
            for (Enemy& e : enemies) {
                e.pos.x += dx;
            }
            float minX = enemies.front().pos.x;
            float maxX = enemies.front().pos.x + kEnemyW;
            for (const Enemy& e : enemies) {
                minX = std::min(minX, e.pos.x);
                maxX = std::max(maxX, e.pos.x + kEnemyW);
            }
            float shift = 0.f;
            bool bounce = false;
            if (formationDir > 0.f && maxX >= 944.f) {
                shift = 944.f - maxX;
                bounce = true;
            } else if (formationDir < 0.f && minX <= 16.f) {
                shift = 16.f - minX;
                bounce = true;
            }
            if (bounce) {
                for (Enemy& e : enemies) {
                    e.pos.x += shift;
                    e.pos.y += 16.f;
                }
                formationDir = -formationDir;
            }
        }

        // 적과 방어벽 겹침
        for (const Enemy& e : enemies) {
            const meno::Rectf er{e.pos, {kEnemyW, kEnemyH}};
            std::erase_if(pieces, [&](const meno::Rectf& p) { return p.intersects(er); });
        }

        // 적이 바닥선에 닿음
        for (const Enemy& e : enemies) {
            if (e.pos.y + kEnemyH >= kPlayerY) {
                (void)scenes.changeScene(result.id());
                return;
            }
        }

        // 플레이어 총알
        if (playerBullet) {
            playerBullet->y -= kBulletSpeed * dt;
            const meno::Rectf br{*playerBullet, {kBulletW, kBulletH}};
            bool gone = false;
            if (br.bottom() < 0.f) {
                gone = true;
            }
            if (!gone) {
                const auto it = std::find_if(pieces.begin(), pieces.end(),
                                             [&](const meno::Rectf& p) { return p.intersects(br); });
                if (it != pieces.end()) {
                    pieces.erase(it);
                    gone = true;
                }
            }
            if (!gone) {
                const auto it = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& e) {
                    return meno::Rectf{e.pos, {kEnemyW, kEnemyH}}.intersects(br);
                });
                if (it != enemies.end()) {
                    score += enemyPoints(it->row);
                    enemies.erase(it);
                    hitSounds[nextHit].play();
                    nextHit = (nextHit + 1) % hitSounds.size();
                    gone = true;
                }
            }
            if (!gone && bonus) {
                if (meno::Rectf{bonus->pos, {kBonusW, kBonusH}}.intersects(br)) {
                    std::uniform_int_distribution<int> pick{1, 3};
                    score += 50 * pick(rng);
                    bonus.reset();
                    bonusSound.play();
                    gone = true;
                }
            }
            if (gone) {
                playerBullet.reset();
            }
        }

        // 보너스 이동
        if (bonus) {
            bonus->pos.x += bonus->vx * dt;
            if ((bonus->vx > 0.f && bonus->pos.x >= kWidth) ||
                (bonus->vx < 0.f && bonus->pos.x + kBonusW <= 0.f)) {
                bonus.reset();
            }
        }

        // 적 탄
        bool playerHit = false;
        std::erase_if(enemyBullets, [&](EnemyBullet& b) {
            if (playerHit) {
                return false;
            }
            b.pos += b.vel * dt;
            if (b.pos.y - kEnemyBulletRadius > kHeight || b.pos.x < -10.f || b.pos.x > kWidth + 10.f) {
                return true;
            }
            const auto it = std::find_if(pieces.begin(), pieces.end(), [&](const meno::Rectf& p) {
                return circleRect(b.pos, kEnemyBulletRadius, p);
            });
            if (it != pieces.end()) {
                pieces.erase(it);
                return true;
            }
            if (circleRect(b.pos, kEnemyBulletRadius, playerRect)) {
                playerHit = true;
                return true;
            }
            return false;
        });

        if (playerHit) {
            --lives;
            loseSound.play();
            enemyBullets.clear();
            playerBullet.reset();
            if (lives <= 0) {
                lives = 0;
                (void)scenes.changeScene(result.id());
                return;
            }
            freezeTimer = 1.0;
            return;
        }

        // 웨이브 클리어
        if (enemies.empty()) {
            ++wave;
            startWave();
        }
    });

    play.setDrawHandler([&](meno::Renderer& renderer) {
        const meno::Color shieldColor = meno::Color::fromRgb(0x4CC26B);
        for (const meno::Rectf& p : pieces) {
            renderer.drawRect(p, {.fill = shieldColor});
        }
        for (const Enemy& e : enemies) {
            const int cell = enemyBaseCell(e.row) + animFrame;
            renderer.draw(*enemiesTex,
                          {.position = e.pos, .source = meno::Recti{cell * 32, 0, 32, 24}});
        }
        if (bonus) {
            renderer.draw(*bonusTex, {.position = bonus->pos});
        }
        renderer.draw(*playerTex, {.position = {playerX, kPlayerY}});
        if (playerBullet) {
            renderer.drawRect({*playerBullet, {kBulletW, kBulletH}}, {.fill = meno::colors::White});
        }
        const meno::Color enemyBulletColor = meno::Color::fromRgb(0xFF6B6B);
        for (const EnemyBullet& b : enemyBullets) {
            renderer.drawCircle(b.pos, kEnemyBulletRadius, {.fill = enemyBulletColor});
        }
        drawCentered(renderer, "SCORE " + std::to_string(score), {140.f, 24.f}, 16);
        drawCentered(renderer, "WAVE " + std::to_string(wave), {kWidth / 2.f, 24.f}, 16);
        drawCentered(renderer, "LIVES " + std::to_string(lives), {820.f, 24.f}, 16);
    });

    // ---- 결과 ----
    result.setEnterHandler([&] { bgm->stop(); });
    result.setUpdateHandler([&](double) {
        if (escapeCheck()) {
            return;
        }
        if (game.window().input().wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawCentered(renderer, "GAME OVER", {kWidth / 2.f, 180.f}, 48);
        drawCentered(renderer, "SCORE " + std::to_string(score), {kWidth / 2.f, 260.f}, 32);
        drawCentered(renderer, "WAVE " + std::to_string(wave), {kWidth / 2.f, 310.f}, 32);
        drawCentered(renderer, "R: RESTART  ESC: QUIT", {kWidth / 2.f, 390.f}, 16);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```