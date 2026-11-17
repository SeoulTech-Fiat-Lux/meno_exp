```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
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
constexpr float kEnemyStepX = 48.f;
constexpr float kEnemyStepY = 40.f;

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

constexpr int kShieldCount = 4;
constexpr int kShieldCols = 6;
constexpr int kShieldRows = 4;
constexpr float kPiece = 8.f;
constexpr float kShieldTop = 400.f;
constexpr std::array<float, kShieldCount> kShieldCenters{192.f, 384.f, 576.f, 768.f};

constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusY = 50.f;
constexpr float kBonusSpeed = 120.f;

struct Enemy {
    int col = 0;
    int row = 0;
};

struct EnemyBullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
};

struct Bonus {
    float x = 0.f;
    float dir = 1.f;
};

bool circleRect(meno::Vec2f c, float r, const meno::Rectf& rect) {
    const float cx = std::clamp(c.x, rect.left(), rect.right());
    const float cy = std::clamp(c.y, rect.top(), rect.bottom());
    const float dx = c.x - cx;
    const float dy = c.y - cy;
    return dx * dx + dy * dy < r * r;
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "STAR FORMATION",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& resources = game.resources();

    const char* fontPath = "assets/fonts/game.ttf";
    const char* enemiesPath = "assets/textures/fm_enemies.png";
    const char* playerPath = "assets/textures/fm_player.png";
    const char* bonusPath = "assets/textures/fm_bonus.png";
    const char* shootPath = "assets/sfx/shoot.wav";
    const char* hitPath = "assets/sfx/hit.wav";
    const char* losePath = "assets/sfx/lose.wav";
    const char* bonusSfxPath = "assets/sfx/bonus.wav";
    const char* musicPath = "assets/music/bgm_formation.ogg";

    const auto font = resources.loadFont(fontPath);
    const auto enemiesTex = resources.loadTexture(enemiesPath);
    const auto playerTex = resources.loadTexture(playerPath);
    const auto bonusTex = resources.loadTexture(bonusPath);
    const auto shootBuf = resources.loadSoundBuffer(shootPath);
    const auto hitBuf = resources.loadSoundBuffer(hitPath);
    const auto loseBuf = resources.loadSoundBuffer(losePath);
    const auto bonusBuf = resources.loadSoundBuffer(bonusSfxPath);
    std::optional<meno::Music> music = meno::Music::openFromFile(musicPath);

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, fontPath);
    require(enemiesTex != nullptr, enemiesPath);
    require(playerTex != nullptr, playerPath);
    require(bonusTex != nullptr, bonusPath);
    require(shootBuf != nullptr, shootPath);
    require(hitBuf != nullptr, hitPath);
    require(loseBuf != nullptr, losePath);
    require(bonusBuf != nullptr, bonusSfxPath);
    require(music.has_value(), musicPath);
    if (failed) {
        return 1;
    }

    music->setLooping(true);
    music->setVolume(0.4f);

    meno::Sound shootSound{shootBuf};
    meno::Sound hitSound{hitBuf};
    meno::Sound loseSound{loseBuf};
    meno::Sound bonusSound{bonusBuf};

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> bonusScoreDist{1, 3};

    // ---- 게임 상태 ----
    int score = 0;
    int wave = 1;
    int lives = 3;
    double playTime = 0.0;
    double bonusNext = 20.0;
    bool bonusFromLeft = true;
    std::optional<Bonus> bonus;
    double freeze = 0.0;
    float playerX = (kWidth - kPlayerW) / 2.f;
    std::optional<meno::Vec2f> playerBullet;
    std::vector<EnemyBullet> enemyBullets;
    std::vector<Enemy> enemies;
    meno::Vec2f formation{296.f, 80.f};
    float formationDir = 1.f;
    double waveTime = 0.0;
    double nextShot = 1.0;
    double animTimer = 0.0;
    int animFrame = 0;
    std::array<std::array<std::array<bool, kShieldCols>, kShieldRows>, kShieldCount> shields{};

    double titleTime = 0.0;

    const auto enemyPos = [&](const Enemy& e) {
        return meno::Vec2f{formation.x + kEnemyStepX * static_cast<float>(e.col),
                           formation.y + kEnemyStepY * static_cast<float>(e.row)};
    };
    const auto enemyRect = [&](const Enemy& e) {
        return meno::Rectf{enemyPos(e), {kEnemyW, kEnemyH}};
    };
    const auto pieceRect = [](int s, int r, int c) {
        const float left = kShieldCenters[static_cast<std::size_t>(s)] -
                           kPiece * static_cast<float>(kShieldCols) / 2.f;
        return meno::Rectf{{left + kPiece * static_cast<float>(c),
                            kShieldTop + kPiece * static_cast<float>(r)},
                           {kPiece, kPiece}};
    };
    const auto playerRect = [&] {
        return meno::Rectf{{playerX, kPlayerY}, {kPlayerW, kPlayerH}};
    };

    const auto setupWave = [&] {
        enemies.clear();
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                enemies.push_back({c, r});
            }
        }
        const float drop = std::min(16.f * static_cast<float>(wave - 1), 64.f);
        formation = {296.f, 80.f + drop};
        formationDir = 1.f;
        for (auto& shield : shields) {
            for (auto& row : shield) {
                row.fill(true);
            }
        }
        playerBullet.reset();
        enemyBullets.clear();
        bonus.reset();
        waveTime = 0.0;
        nextShot = 1.0;
        animTimer = 0.0;
        animFrame = 0;
    };

    const auto resetGame = [&] {
        score = 0;
        wave = 1;
        lives = 3;
        playTime = 0.0;
        bonusNext = 20.0;
        bonusFromLeft = true;
        freeze = 0.0;
        playerX = (kWidth - kPlayerW) / 2.f;
        setupWave();
    };

    // ---- 글자 ----
    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text, float y,
                                  unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = meno::colors::White};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {kWidth / 2.f, y}, params);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---- 타이틀 ----
    title.setEnterHandler([&] { titleTime = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTime += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawCentered(renderer, "STAR FORMATION", 220.f, 48);
        drawCentered(renderer, "PRESS ENTER", 320.f, 32);
    });

    // ---- 플레이 ----
    play.setEnterHandler([&] {
        resetGame();
        music->play();
    });
    play.setExitHandler([&] { music->stop(); });

    play.setUpdateHandler([&](double fixedDt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDt);

        if (freeze > 0.0) {
            freeze -= fixedDt;
            return;
        }

        playTime += fixedDt;
        waveTime += fixedDt;

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

        // 사격
        if (input.wasPressed(meno::Key::Space) && !playerBullet) {
            playerBullet = meno::Vec2f{playerX + (kPlayerW - kBulletW) / 2.f, kPlayerY - kBulletH};
            shootSound.play();
        }

        // 대열 이동
        if (!enemies.empty()) {
            const float speed = 40.f + 4.f * static_cast<float>(32 - static_cast<int>(enemies.size()));
            formation.x += formationDir * speed * dt;
            int minCol = kCols;
            int maxCol = -1;
            for (const Enemy& e : enemies) {
                minCol = std::min(minCol, e.col);
                maxCol = std::max(maxCol, e.col);
            }
            const float left = formation.x + kEnemyStepX * static_cast<float>(minCol);
            const float right = formation.x + kEnemyStepX * static_cast<float>(maxCol) + kEnemyW;
            if (formationDir > 0.f && right >= 944.f) {
                formation.x -= right - 944.f;
                formation.y += 16.f;
                formationDir = -1.f;
            } else if (formationDir < 0.f && left <= 16.f) {
                formation.x += 16.f - left;
                formation.y += 16.f;
                formationDir = 1.f;
            }
        }

        // 애니메이션
        animTimer += fixedDt;
        while (animTimer >= 0.5) {
            animTimer -= 0.5;
            animFrame = 1 - animFrame;
        }

        // 적 사격
        if (waveTime >= nextShot) {
            nextShot += 1.0;
            if (!enemies.empty() && enemyBullets.size() < kMaxEnemyBullets) {
                const float playerCx = playerX + kPlayerW / 2.f;
                const float playerCy = kPlayerY + kPlayerH / 2.f;
                int bestCol = -1;
                float bestDist = 0.f;
                for (int c = 0; c < kCols; ++c) {
                    const bool has = std::any_of(enemies.begin(), enemies.end(),
                                                 [c](const Enemy& e) { return e.col == c; });
                    if (!has) {
                        continue;
                    }
                    const float cx = formation.x + kEnemyStepX * static_cast<float>(c) + kEnemyW / 2.f;
                    const float d = std::abs(cx - playerCx);
                    if (bestCol < 0 || d < bestDist) {
                        bestCol = c;
                        bestDist = d;
                    }
                }
                const Enemy* shooter = nullptr;
                for (const Enemy& e : enemies) {
                    if (e.col == bestCol && (shooter == nullptr || e.row > shooter->row)) {
                        shooter = &e;
                    }
                }
                if (shooter != nullptr) {
                    const meno::Vec2f p = enemyPos(*shooter);
                    const meno::Vec2f from{p.x + kEnemyW / 2.f, p.y + kEnemyH};
                    meno::Vec2f dir{playerCx - from.x, playerCy - from.y};
                    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len > 0.0001f) {
                        dir = dir * (1.f / len);
                    } else {
                        dir = {0.f, 1.f};
                    }
                    enemyBullets.push_back({from, dir * kEnemyBulletSpeed});
                }
            }
        }

        // 보너스 비행체
        if (playTime >= bonusNext) {
            bonusNext += 20.0;
            if (bonusFromLeft) {
                bonus = Bonus{-kBonusW, 1.f};
            } else {
                bonus = Bonus{kWidth, -1.f};
            }
            bonusFromLeft = !bonusFromLeft;
        }
        if (bonus) {
            bonus->x += bonus->dir * kBonusSpeed * dt;
            if ((bonus->dir > 0.f && bonus->x >= kWidth) ||
                (bonus->dir < 0.f && bonus->x + kBonusW <= 0.f)) {
                bonus.reset();
            }
        }

        // 플레이어 총알
        if (playerBullet) {
            playerBullet->y -= kBulletSpeed * dt;
            const meno::Rectf br{*playerBullet, {kBulletW, kBulletH}};
            bool gone = false;
            // 방어벽 (아래 조각부터)
            for (int r = kShieldRows - 1; r >= 0 && !gone; --r) {
                for (int s = 0; s < kShieldCount && !gone; ++s) {
                    for (int c = 0; c < kShieldCols && !gone; ++c) {
                        auto& piece = shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(r)]
                                             [static_cast<std::size_t>(c)];
                        if (piece && br.intersects(pieceRect(s, r, c))) {
                            piece = false;
                            gone = true;
                        }
                    }
                }
            }
            // 적
            if (!gone) {
                for (auto it = enemies.begin(); it != enemies.end(); ++it) {
                    if (br.intersects(enemyRect(*it))) {
                        score += it->row == 0 ? 30 : (it->row == 1 ? 20 : 10);
                        enemies.erase(it);
                        hitSound.play();
                        gone = true;
                        break;
                    }
                }
            }
            // 보너스
            if (!gone && bonus) {
                const meno::Rectf bonusRect{{bonus->x, kBonusY}, {kBonusW, kBonusH}};
                if (br.intersects(bonusRect)) {
                    score += 50 * bonusScoreDist(rng);
                    bonus.reset();
                    bonusSound.play();
                    gone = true;
                }
            }
            if (!gone && br.bottom() < 0.f) {
                gone = true;
            }
            if (gone) {
                playerBullet.reset();
            }
        }

        // 적 탄
        bool playerHit = false;
        const meno::Rectf pr = playerRect();
        std::erase_if(enemyBullets, [&](EnemyBullet& b) {
            if (playerHit) {
                return false;
            }
            b.pos += b.vel * dt;
            for (int r = 0; r < kShieldRows; ++r) {
                for (int s = 0; s < kShieldCount; ++s) {
                    for (int c = 0; c < kShieldCols; ++c) {
                        auto& piece = shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(r)]
                                             [static_cast<std::size_t>(c)];
                        if (piece && circleRect(b.pos, kEnemyBulletRadius, pieceRect(s, r, c))) {
                            piece = false;
                            return true;
                        }
                    }
                }
            }
            if (circleRect(b.pos, kEnemyBulletRadius, pr)) {
                playerHit = true;
                return true;
            }
            const float m = kEnemyBulletRadius;
            return b.pos.y > kHeight + m || b.pos.y < -m || b.pos.x < -m || b.pos.x > kWidth + m;
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
            freeze = 1.0;
        }

        // 적과 방어벽
        for (const Enemy& e : enemies) {
            const meno::Rectf er = enemyRect(e);
            for (int s = 0; s < kShieldCount; ++s) {
                for (int r = 0; r < kShieldRows; ++r) {
                    for (int c = 0; c < kShieldCols; ++c) {
                        auto& piece = shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(r)]
                                             [static_cast<std::size_t>(c)];
                        if (piece && er.intersects(pieceRect(s, r, c))) {
                            piece = false;
                        }
                    }
                }
            }
        }

        // 적이 바닥에 닿음
        for (const Enemy& e : enemies) {
            if (enemyRect(e).bottom() >= kPlayerY) {
                (void)scenes.changeScene(result.id());
                return;
            }
        }

        // 웨이브 클리어
        if (enemies.empty()) {
            ++wave;
            setupWave();
        }
    });

    play.setDrawHandler([&](meno::Renderer& renderer) {
        const meno::Color shieldColor = meno::Color::fromRgb(0x4CC26B);
        // 방어벽
        for (int s = 0; s < kShieldCount; ++s) {
            for (int r = 0; r < kShieldRows; ++r) {
                for (int c = 0; c < kShieldCols; ++c) {
                    if (shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(r)]
                               [static_cast<std::size_t>(c)]) {
                        renderer.drawRect(pieceRect(s, r, c), {.fill = shieldColor});
                    }
                }
            }
        }
        // 적
        for (const Enemy& e : enemies) {
            const int base = e.row == 0 ? 0 : (e.row == 1 ? 2 : 4);
            const int cell = base + animFrame;
            renderer.draw(*enemiesTex,
                          {.position = enemyPos(e),
                           .source = meno::Recti{cell * 32, 0, 32, 24}});
        }
        // 보너스
        if (bonus) {
            renderer.draw(*bonusTex, {.position = {bonus->x, kBonusY}});
        }
        // 플레이어
        renderer.draw(*playerTex, {.position = {playerX, kPlayerY}});
        // 탄
        if (playerBullet) {
            renderer.drawRect({*playerBullet, {kBulletW, kBulletH}}, {.fill = meno::colors::White});
        }
        const meno::Color enemyBulletColor = meno::Color::fromRgb(0xFF6B6B);
        for (const EnemyBullet& b : enemyBullets) {
            renderer.drawCircle(b.pos, kEnemyBulletRadius, {.fill = enemyBulletColor});
        }
        // 점수
        const meno::TextParams hud{.characterSize = 32, .color = meno::colors::White};
        renderer.drawText(*font, "SCORE " + std::to_string(score), {16.f, 4.f}, hud);
        {
            const std::string text = "WAVE " + std::to_string(wave);
            meno::TextParams p = hud;
            const meno::Rectf bounds = renderer.measureTextBounds(*font, text, p);
            p.origin = {bounds.position.x + bounds.size.x / 2.f, 0.f};
            renderer.drawText(*font, text, {kWidth / 2.f, 4.f}, p);
        }
        {
            const std::string text = "LIVES " + std::to_string(lives);
            meno::TextParams p = hud;
            const meno::Rectf bounds = renderer.measureTextBounds(*font, text, p);
            p.origin = {bounds.position.x + bounds.size.x, 0.f};
            renderer.drawText(*font, text, {kWidth - 16.f, 4.f}, p);
        }
    });

    // ---- 결과 ----
    result.setEnterHandler([&] { music->stop(); });
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
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawCentered(renderer, "GAME OVER", 170.f, 48);
        drawCentered(renderer, "SCORE " + std::to_string(score), 250.f, 32);
        drawCentered(renderer, "WAVE " + std::to_string(wave), 300.f, 32);
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 380.f, 32);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```