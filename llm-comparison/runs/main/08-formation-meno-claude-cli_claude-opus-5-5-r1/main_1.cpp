#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
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

constexpr float kShotRadius = 3.f;
constexpr float kShotSpeed = 240.f;
constexpr std::size_t kMaxShots = 3;

constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusY = 50.f;
constexpr float kBonusSpeed = 120.f;
constexpr double kBonusInterval = 20.0;

struct Enemy {
    meno::Vec2f pos;
    int col = 0;
    int row = 0;
};

struct EnemyShot {
    meno::Vec2f pos;
    meno::Vec2f vel;
};

struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;
    void play() {
        if (sounds.empty()) {
            return;
        }
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

bool circleHitsRect(meno::Vec2f c, float r, const meno::Rectf& rect) {
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
                     .clearColor = meno::Color::fromRgb(0x000000)}};
    meno::ResourceManager& res = game.resources();

    std::vector<std::string> missing;

    const auto font = res.loadFont("assets/fonts/game.ttf");
    if (!font) missing.emplace_back("assets/fonts/game.ttf");
    const auto enemyTex = res.loadTexture("assets/textures/fm_enemies.png");
    if (!enemyTex) missing.emplace_back("assets/textures/fm_enemies.png");
    const auto playerTex = res.loadTexture("assets/textures/fm_player.png");
    if (!playerTex) missing.emplace_back("assets/textures/fm_player.png");
    const auto bonusTex = res.loadTexture("assets/textures/fm_bonus.png");
    if (!bonusTex) missing.emplace_back("assets/textures/fm_bonus.png");

    const auto shootBuf = res.loadSoundBuffer("assets/sfx/shoot.wav");
    if (!shootBuf) missing.emplace_back("assets/sfx/shoot.wav");
    const auto hitBuf = res.loadSoundBuffer("assets/sfx/hit.wav");
    if (!hitBuf) missing.emplace_back("assets/sfx/hit.wav");
    const auto loseBuf = res.loadSoundBuffer("assets/sfx/lose.wav");
    if (!loseBuf) missing.emplace_back("assets/sfx/lose.wav");
    const auto bonusBuf = res.loadSoundBuffer("assets/sfx/bonus.wav");
    if (!bonusBuf) missing.emplace_back("assets/sfx/bonus.wav");

    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_formation.ogg");
    if (!bgm) missing.emplace_back("assets/music/bgm_formation.ogg");

    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    SoundPool shootSfx, hitSfx, loseSfx, bonusSfx;
    for (int i = 0; i < 4; ++i) {
        shootSfx.sounds.emplace_back(shootBuf);
        hitSfx.sounds.emplace_back(hitBuf);
        loseSfx.sounds.emplace_back(loseBuf);
        bonusSfx.sounds.emplace_back(bonusBuf);
    }

    const meno::Color white = meno::Color::fromRgb(0xFFFFFF);
    const meno::Color shieldColor = meno::Color::fromRgb(0x4CC26B);
    const meno::Color shotColor = meno::Color::fromRgb(0xFF6B6B);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> bonusRoll{1, 3};

    // ---- 게임 상태 ----
    std::vector<Enemy> enemies;
    std::vector<meno::Rectf> pieces;
    std::vector<EnemyShot> shots;
    std::optional<meno::Rectf> bullet;
    std::optional<meno::Vec2f> bonusPos;
    float bonusDir = 1.f;
    bool bonusNextFromLeft = true;
    double bonusTimer = kBonusInterval;

    float playerX = kWidth / 2.f - kPlayerW / 2.f;
    int score = 0;
    int lives = 3;
    int wave = 1;
    float formationDir = 1.f;
    double shotTimer = 1.0;
    double animTimer = 0.0;
    int animFrame = 0;
    double pauseTimer = 0.0;
    double titleTime = 0.0;

    const auto buildShields = [&] {
        pieces.clear();
        for (const float cx : {192.f, 384.f, 576.f, 768.f}) {
            const float left = cx - 24.f;
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 6; ++c) {
                    pieces.push_back(meno::Rectf{
                        {left + 8.f * static_cast<float>(c), 400.f + 8.f * static_cast<float>(r)},
                        {8.f, 8.f}});
                }
            }
        }
    };

    const auto startWave = [&](int w) {
        wave = w;
        const float offset = std::min(16.f * static_cast<float>(w - 1), 64.f);
        enemies.clear();
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                enemies.push_back(Enemy{{296.f + 48.f * static_cast<float>(c),
                                         80.f + 40.f * static_cast<float>(r) + offset},
                                        c, r});
            }
        }
        formationDir = 1.f;
        buildShields();
        shots.clear();
        bullet.reset();
        bonusPos.reset();
        shotTimer = 1.0;
        animTimer = 0.0;
        animFrame = 0;
        pauseTimer = 0.0;
    };

    const auto resetGame = [&] {
        score = 0;
        lives = 3;
        playerX = kWidth / 2.f - kPlayerW / 2.f;
        bonusTimer = kBonusInterval;
        bonusNextFromLeft = true;
        startWave(1);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text, float y,
                                  unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = white};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {kWidth / 2.f, y}, params);
    };

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
        drawCentered(renderer, "PRESS ENTER", 320.f, 16);
    });

    // ---- 플레이 ----
    play.setEnterHandler([&] {
        resetGame();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });

    play.setUpdateHandler([&](double fixedDt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDt);

        if (pauseTimer > 0.0) {
            pauseTimer -= fixedDt;
            return;
        }

        // 플레이어 이동
        float move = 0.f;
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) move -= 1.f;
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) move += 1.f;
        playerX += move * kPlayerSpeed * dt;
        playerX = std::clamp(playerX, 16.f, kWidth - 16.f - kPlayerW);
        const meno::Rectf playerRect{{playerX, kPlayerY}, {kPlayerW, kPlayerH}};
        const meno::Vec2f playerCenter{playerX + kPlayerW / 2.f, kPlayerY + kPlayerH / 2.f};

        // 사격
        if (input.wasPressed(meno::Key::Space) && !bullet) {
            bullet = meno::Rectf{{playerCenter.x - kBulletW / 2.f, kPlayerY - kBulletH},
                                 {kBulletW, kBulletH}};
            shootSfx.play();
        }

        // 애니메이션
        animTimer += fixedDt;
        while (animTimer >= 0.5) {
            animTimer -= 0.5;
            animFrame ^= 1;
        }

        // 대열 이동
        if (!enemies.empty()) {
            const float speed = 40.f + 4.f * static_cast<float>(32 - static_cast<int>(enemies.size()));
            const float dx = formationDir * speed * dt;
            for (Enemy& e : enemies) e.pos.x += dx;
            float minX = enemies.front().pos.x;
            float maxR = enemies.front().pos.x + kEnemyW;
            for (const Enemy& e : enemies) {
                minX = std::min(minX, e.pos.x);
                maxR = std::max(maxR, e.pos.x + kEnemyW);
            }
            if (formationDir > 0.f && maxR >= 944.f) {
                const float shift = 944.f - maxR;
                for (Enemy& e : enemies) {
                    e.pos.x += shift;
                    e.pos.y += 16.f;
                }
                formationDir = -1.f;
            } else if (formationDir < 0.f && minX <= 16.f) {
                const float shift = 16.f - minX;
                for (Enemy& e : enemies) {
                    e.pos.x += shift;
                    e.pos.y += 16.f;
                }
                formationDir = 1.f;
            }
        }

        // 적과 방어벽 겹침
        for (const Enemy& e : enemies) {
            const meno::Rectf er{e.pos, {kEnemyW, kEnemyH}};
            std::erase_if(pieces, [&](const meno::Rectf& p) { return p.intersects(er); });
        }

        // 적이 바닥에 닿음
        for (const Enemy& e : enemies) {
            if (e.pos.y + kEnemyH >= kPlayerY) {
                (void)scenes.changeScene(result.id());
                return;
            }
        }

        // 적 사격
        shotTimer -= fixedDt;
        while (shotTimer <= 0.0) {
            shotTimer += 1.0;
            if (enemies.empty() || shots.size() >= kMaxShots) {
                continue;
            }
            int bestCol = -1;
            float bestDist = 0.f;
            for (int c = 0; c < kCols; ++c) {
                const Enemy* any = nullptr;
                for (const Enemy& e : enemies) {
                    if (e.col == c) {
                        any = &e;
                        break;
                    }
                }
                if (any == nullptr) continue;
                const float d = std::abs(any->pos.x + kEnemyW / 2.f - playerCenter.x);
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
                const meno::Vec2f from{shooter->pos.x + kEnemyW / 2.f, shooter->pos.y + kEnemyH};
                const meno::Vec2f diff = playerCenter - from;
                const float len = std::sqrt(diff.x * diff.x + diff.y * diff.y);
                meno::Vec2f vel{0.f, kShotSpeed};
                if (len > 0.001f) {
                    vel = diff * (kShotSpeed / len);
                }
                shots.push_back(EnemyShot{from, vel});
            }
        }

        // 보너스 비행체
        bonusTimer -= fixedDt;
        while (bonusTimer <= 0.0) {
            bonusTimer += kBonusInterval;
            if (bonusNextFromLeft) {
                bonusPos = meno::Vec2f{-kBonusW, kBonusY};
                bonusDir = 1.f;
            } else {
                bonusPos = meno::Vec2f{kWidth, kBonusY};
                bonusDir = -1.f;
            }
            bonusNextFromLeft = !bonusNextFromLeft;
        }
        if (bonusPos) {
            bonusPos->x += bonusDir * kBonusSpeed * dt;
            if ((bonusDir > 0.f && bonusPos->x >= kWidth) ||
                (bonusDir < 0.f && bonusPos->x + kBonusW <= 0.f)) {
                bonusPos.reset();
            }
        }

        // 플레이어 총알
        if (bullet) {
            const float prevTop = bullet->position.y;
            bullet->position.y -= kBulletSpeed * dt;
            // 이동 전후를 모두 덮는 영역으로 방어벽을 검사합니다.
            const meno::Rectf swept{{bullet->position.x, bullet->position.y},
                                    {kBulletW, (prevTop + kBulletH) - bullet->position.y}};
            int hitPiece = -1;
            for (std::size_t i = 0; i < pieces.size(); ++i) {
                if (pieces[i].intersects(swept) &&
                    (hitPiece < 0 || pieces[i].position.y > pieces[static_cast<std::size_t>(hitPiece)].position.y)) {
                    hitPiece = static_cast<int>(i);
                }
            }
            if (hitPiece >= 0) {
                pieces.erase(pieces.begin() + hitPiece);
                bullet.reset();
            }
        }
        if (bullet) {
            const meno::Rectf b = *bullet;
            for (auto it = enemies.begin(); it != enemies.end(); ++it) {
                if (meno::Rectf{it->pos, {kEnemyW, kEnemyH}}.intersects(b)) {
                    score += it->row == 0 ? 30 : (it->row == 1 ? 20 : 10);
                    enemies.erase(it);
                    bullet.reset();
                    hitSfx.play();
                    break;
                }
            }
        }
        if (bullet && bonusPos) {
            if (meno::Rectf{*bonusPos, {kBonusW, kBonusH}}.intersects(*bullet)) {
                score += 50 * bonusRoll(rng);
                bonusPos.reset();
                bullet.reset();
                bonusSfx.play();
            }
        }
        if (bullet && bullet->position.y + kBulletH < 0.f) {
            bullet.reset();
        }

        // 적 탄
        bool playerHit = false;
        for (EnemyShot& s : shots) s.pos += s.vel * dt;
        std::erase_if(shots, [&](const EnemyShot& s) {
            for (auto it = pieces.begin(); it != pieces.end(); ++it) {
                if (circleHitsRect(s.pos, kShotRadius, *it)) {
                    pieces.erase(it);
                    return true;
                }
            }
            if (circleHitsRect(s.pos, kShotRadius, playerRect)) {
                playerHit = true;
                return true;
            }
            return s.pos.y - kShotRadius > kHeight || s.pos.x + kShotRadius < 0.f ||
                   s.pos.x - kShotRadius > kWidth || s.pos.y + kShotRadius < 0.f;
        });

        if (playerHit) {
            --lives;
            loseSfx.play();
            shots.clear();
            bullet.reset();
            if (lives <= 0) {
                lives = 0;
                (void)scenes.changeScene(result.id());
                return;
            }
            pauseTimer = 1.0;
            return;
        }

        // 웨이브 클리어
        if (enemies.empty()) {
            startWave(wave + 1);
        }
    });

    play.setDrawHandler([&](meno::Renderer& renderer) {
        for (const meno::Rectf& p : pieces) {
            renderer.drawRect(p, {.fill = shieldColor});
        }
        for (const Enemy& e : enemies) {
            const int base = e.row == 0 ? 0 : (e.row == 1 ? 2 : 4);
            const int idx = base + animFrame;
            renderer.draw(*enemyTex, {.position = e.pos,
                                      .source = meno::Recti{idx * 32, 0, 32, 24}});
        }
        if (bonusPos) {
            renderer.draw(*bonusTex, {.position = *bonusPos});
        }
        renderer.draw(*playerTex, {.position = {playerX, kPlayerY}});
        if (bullet) {
            renderer.drawRect(*bullet, {.fill = white});
        }
        for (const EnemyShot& s : shots) {
            renderer.drawCircle(s.pos, kShotRadius, {.fill = shotColor});
        }

        const meno::TextParams hud{.characterSize = 16, .color = white};
        renderer.drawText(*font, "SCORE " + std::to_string(score), {16.f, 16.f}, hud);
        {
            const std::string text = "WAVE " + std::to_string(wave);
            meno::TextParams p = hud;
            const meno::Rectf b = renderer.measureTextBounds(*font, text, p);
            p.origin = {b.position.x + b.size.x / 2.f, 0.f};
            renderer.drawText(*font, text, {kWidth / 2.f, 16.f}, p);
        }
        {
            const std::string text = "LIVES " + std::to_string(lives);
            meno::TextParams p = hud;
            const meno::Rectf b = renderer.measureTextBounds(*font, text, p);
            p.origin = {b.position.x + b.size.x, 0.f};
            renderer.drawText(*font, text, {kWidth - 16.f, 16.f}, p);
        }
    });

    // ---- 결과 ----
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
        drawCentered(renderer, "GAME OVER", 180.f, 48);
        drawCentered(renderer, "SCORE " + std::to_string(score), 260.f, 32);
        drawCentered(renderer, "WAVE " + std::to_string(wave), 310.f, 32);
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 390.f, 16);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
