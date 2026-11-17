```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kScreenW = 960.f;
constexpr float kScreenH = 540.f;
constexpr int kCols = 8;
constexpr int kRows = 4;
constexpr int kBunkers = 4;
constexpr int kPieceCols = 6;
constexpr int kPieceRows = 4;
constexpr float kBunkerY = 400.f;
constexpr float kBunkerCenter[kBunkers] = {192.f, 384.f, 576.f, 768.f};
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerY = 490.f;
constexpr float kPlayerSpeed = 300.f;
constexpr float kPlayerBulletW = 4.f;
constexpr float kPlayerBulletH = 12.f;
constexpr float kPlayerBulletSpeed = 600.f;
constexpr float kEnemyBulletSpeed = 240.f;
constexpr float kEnemyBulletRadius = 3.f;
constexpr std::size_t kMaxEnemyShots = 3;
constexpr float kBonusY = 50.f;
constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusSpeed = 120.f;
constexpr double kBonusPeriod = 20.0;
constexpr double kFreezeTime = 1.0;
constexpr double kTitleTime = 2.0;
constexpr double kAnimPeriod = 0.5;
constexpr double kFirePeriod = 1.0;
constexpr int kBonusScores[3] = {50, 100, 150};

struct Shot {
    meno::Vec2f pos; // 중심 좌표
    meno::Vec2f vel;
};

struct Bonus {
    float x;
    float dir;
};

struct PlayState {
    std::array<std::array<bool, kCols>, kRows> alive{};
    std::array<std::array<std::array<bool, kPieceCols>, kPieceRows>, kBunkers> piece{};
    float formationX = 296.f;
    float formationY = 80.f;
    int formationDir = 1;
    double animTime = 0.0;
    double fireTimer = 0.0;
    double bonusTimer = 0.0;
    double freezeTimer = 0.0;
    int bonusCount = 0;
    std::vector<Shot> shots;
    std::optional<meno::Vec2f> playerBullet; // 왼쪽 위 좌표
    std::optional<Bonus> bonus;
    float playerX = 460.f;
    int score = 0;
    int lives = 3;
    int wave = 1;
};

meno::Rectf makeRect(float x, float y, float w, float h) {
    return meno::Rectf{{x, y}, {w, h}};
}

float enemyX(const PlayState& s, int c) {
    return s.formationX + 48.f * static_cast<float>(c);
}

float enemyY(const PlayState& s, int r) {
    return s.formationY + 40.f * static_cast<float>(r);
}

meno::Rectf enemyRect(const PlayState& s, int c, int r) {
    return makeRect(enemyX(s, c), enemyY(s, r), kEnemyW, kEnemyH);
}

meno::Rectf pieceRect(int b, int r, int c) {
    return makeRect(kBunkerCenter[b] - 24.f + 8.f * static_cast<float>(c),
                    kBunkerY + 8.f * static_cast<float>(r), 8.f, 8.f);
}

int rowPoints(int r) {
    return r == 0 ? 30 : (r == 1 ? 20 : 10);
}

int rowCellBase(int r) {
    return r == 0 ? 0 : (r == 1 ? 2 : 4);
}

int aliveCount(const PlayState& s) {
    int n = 0;
    for (const auto& row : s.alive) {
        for (bool a : row) {
            n += a ? 1 : 0;
        }
    }
    return n;
}

bool enemyOverlaps(const PlayState& s, const meno::Rectf& box) {
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (s.alive[r][c] && box.intersects(enemyRect(s, c, r))) {
                return true;
            }
        }
    }
    return false;
}

bool circleHitsRect(meno::Vec2f c, float radius, const meno::Rectf& box) {
    const float nx = std::clamp(c.x, box.position.x, box.position.x + box.size.x);
    const float ny = std::clamp(c.y, box.position.y, box.position.y + box.size.y);
    const float dx = c.x - nx;
    const float dy = c.y - ny;
    return dx * dx + dy * dy <= radius * radius;
}

// 방어벽 조각 하나를 찾아 지우고 true를 돌려줍니다.
template <class Hit>
bool removeFirstPiece(PlayState& s, Hit hit) {
    for (int b = 0; b < kBunkers; ++b) {
        for (int r = 0; r < kPieceRows; ++r) {
            for (int c = 0; c < kPieceCols; ++c) {
                if (s.piece[b][r][c] && hit(pieceRect(b, r, c))) {
                    s.piece[b][r][c] = false;
                    return true;
                }
            }
        }
    }
    return false;
}

void startWave(PlayState& s) {
    for (auto& row : s.alive) {
        row.fill(true);
    }
    for (auto& bunker : s.piece) {
        for (auto& row : bunker) {
            row.fill(true);
        }
    }
    s.formationX = 296.f;
    s.formationY = 80.f + std::min(64.f, 16.f * static_cast<float>(s.wave - 1));
    s.formationDir = 1;
    s.fireTimer = 0.0;
    s.shots.clear();
    s.playerBullet.reset();
    s.bonus.reset();
}

void resetGame(PlayState& s) {
    s = PlayState{};
    startWave(s);
}

// 남은 열 중 플레이어 중심과 x 거리가 가장 가까운 열의 맨 아래 적이 쏩니다.
void fireEnemyShot(PlayState& s, meno::Vec2f target) {
    int bestCol = -1;
    int bestRow = -1;
    float bestDist = 0.f;
    for (int c = 0; c < kCols; ++c) {
        int row = -1;
        for (int r = kRows - 1; r >= 0; --r) {
            if (s.alive[r][c]) {
                row = r;
                break;
            }
        }
        if (row < 0) {
            continue;
        }
        const float dist = std::abs(enemyX(s, c) + kEnemyW / 2.f - target.x);
        if (bestCol < 0 || dist < bestDist) {
            bestCol = c;
            bestRow = row;
            bestDist = dist;
        }
    }
    if (bestCol < 0) {
        return;
    }
    const meno::Vec2f start{enemyX(s, bestCol) + kEnemyW / 2.f, enemyY(s, bestRow) + kEnemyH};
    const float dx = target.x - start.x;
    const float dy = target.y - start.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.f) {
        return;
    }
    const float k = kEnemyBulletSpeed / len;
    s.shots.push_back(Shot{start, meno::Vec2f{dx * k, dy * k}});
}

void drawLabel(meno::Renderer& r, const meno::Font& font, const std::string& text, float x,
               float y, int size, bool centered) {
    meno::TextParams params{};
    params.characterSize = size;
    params.color = meno::colors::White;
    if (centered) {
        params.origin = r.measureTextBounds(font, text, params).center();
    }
    r.drawText(font, text, {x, y}, params);
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "STAR FORMATION",
                     .clearColor = meno::Color::fromRgb(0x000000)}};
    meno::ResourceManager& resources = game.resources();

    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto enemyTex = resources.loadTexture("assets/textures/fm_enemies.png");
    const auto playerTex = resources.loadTexture("assets/textures/fm_player.png");
    const auto bonusTex = resources.loadTexture("assets/textures/fm_bonus.png");
    const auto shootBuf = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuf = resources.loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuf = resources.loadSoundBuffer("assets/sfx/lose.wav");
    const auto bonusBuf = resources.loadSoundBuffer("assets/sfx/bonus.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_formation.ogg");

    bool ok = true;
    const auto require = [&ok](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Missing file: " << path << '\n';
            ok = false;
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(enemyTex != nullptr, "assets/textures/fm_enemies.png");
    require(playerTex != nullptr, "assets/textures/fm_player.png");
    require(bonusTex != nullptr, "assets/textures/fm_bonus.png");
    require(shootBuf != nullptr, "assets/sfx/shoot.wav");
    require(hitBuf != nullptr, "assets/sfx/hit.wav");
    require(loseBuf != nullptr, "assets/sfx/lose.wav");
    require(bonusBuf != nullptr, "assets/sfx/bonus.wav");
    require(bgm.has_value(), "assets/music/bgm_formation.ogg");
    if (!ok) {
        return 1;
    }

    meno::Sound shootSound{shootBuf};
    meno::Sound hitSound{hitBuf};
    meno::Sound loseSound{loseBuf};
    meno::Sound bonusSound{bonusBuf};
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    PlayState world;
    resetGame(world);
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> bonusPick{0, 2};
    double titleTimer = 0.0;

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& titleScene = scenes.createScene();
    meno::Scene& playScene = scenes.createScene();
    meno::Scene& resultScene = scenes.createScene();

    // 타이틀: Enter 또는 2초 뒤 플레이로 넘어갑니다.
    titleScene.setEnterHandler([&] { titleTimer = 0.0; });
    titleScene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleTime) {
            (void)scenes.changeScene(playScene.id());
        }
    });
    titleScene.setDrawHandler([&](meno::Renderer& r) {
        drawLabel(r, *font, "STAR FORMATION", kScreenW / 2.f, 190.f, 48, true);
        drawLabel(r, *font, "PRESS ENTER", kScreenW / 2.f, 300.f, 32, true);
    });

    // 플레이
    playScene.setEnterHandler([&] {
        resetGame(world);
        bgm->play();
    });
    playScene.setExitHandler([&] { bgm->stop(); });
    playScene.setUpdateHandler([&](double dtD) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        // 목숨을 잃은 뒤 1초 동안은 모든 것을 멈춥니다.
        if (world.freezeTimer > 0.0) {
            world.freezeTimer -= dtD;
            return;
        }
        const auto dt = static_cast<float>(dtD);
        const bool firePressed = input.wasPressed(meno::Key::Space);

        // 보너스 비행체: 20초마다 왼쪽과 오른쪽에서 번갈아 출발합니다.
        world.bonusTimer += dtD;
        if (world.bonusTimer >= kBonusPeriod) {
            world.bonusTimer -= kBonusPeriod;
            const bool fromLeft = world.bonusCount % 2 == 0;
            ++world.bonusCount;
            world.bonus = fromLeft ? Bonus{-kBonusW, 1.f} : Bonus{kScreenW, -1.f};
        }
        if (world.bonus) {
            world.bonus->x += world.bonus->dir * kBonusSpeed * dt;
            const bool gone = world.bonus->dir > 0.f ? world.bonus->x >= kScreenW
                                                     : world.bonus->x <= -kBonusW;
            if (gone) {
                world.bonus.reset();
            }
        }

        // 플레이어 이동
        float move = 0.f;
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) {
            move -= 1.f;
        }
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) {
            move += 1.f;
        }
        world.playerX = std::clamp(world.playerX + move * kPlayerSpeed * dt, 16.f,
                                   kScreenW - 16.f - kPlayerW);

        // 플레이어 발사: 총알은 동시에 하나만 있습니다.
        if (firePressed && !world.playerBullet) {
            world.playerBullet = meno::Vec2f{world.playerX + kPlayerW / 2.f - kPlayerBulletW / 2.f,
                                             kPlayerY - kPlayerBulletH};
            shootSound.play();
        }

        // 적 대열 이동: 남은 적이 적을수록 빨라집니다.
        world.animTime += dtD;
        const int remaining = aliveCount(world);
        if (remaining > 0) {
            const float speed = 40.f + 4.f * static_cast<float>(kRows * kCols - remaining);
            world.formationX += static_cast<float>(world.formationDir) * speed * dt;
            float leftEdge = kScreenW;
            float rightEdge = 0.f;
            for (int r = 0; r < kRows; ++r) {
                for (int c = 0; c < kCols; ++c) {
                    if (world.alive[r][c]) {
                        leftEdge = std::min(leftEdge, enemyX(world, c));
                        rightEdge = std::max(rightEdge, enemyX(world, c) + kEnemyW);
                    }
                }
            }
            if (world.formationDir > 0 && rightEdge >= 944.f) {
                world.formationDir = -1;
                world.formationY += 16.f;
            } else if (world.formationDir < 0 && leftEdge <= 16.f) {
                world.formationDir = 1;
                world.formationY += 16.f;
            }
        }

        // 적 사격: 1초마다 한 발, 적 탄은 동시에 3개까지
        world.fireTimer += dtD;
        if (world.fireTimer >= kFirePeriod) {
            world.fireTimer -= kFirePeriod;
            if (world.shots.size() < kMaxEnemyShots) {
                fireEnemyShot(world, meno::Vec2f{world.playerX + kPlayerW / 2.f,
                                                 kPlayerY + kPlayerH / 2.f});
            }
        }

        // 플레이어 총알: 방어벽, 적, 보너스 순서로 판정합니다.
        if (world.playerBullet) {
            meno::Vec2f p = *world.playerBullet;
            p.y -= kPlayerBulletSpeed * dt;
            const meno::Rectf box = makeRect(p.x, p.y, kPlayerBulletW, kPlayerBulletH);
            bool done = p.y + kPlayerBulletH < 0.f;
            if (!done) {
                done = removeFirstPiece(world, [&](const meno::Rectf& piece) {
                    return box.intersects(piece);
                });
            }
            for (int r = 0; r < kRows && !done; ++r) {
                for (int c = 0; c < kCols && !done; ++c) {
                    if (world.alive[r][c] && box.intersects(enemyRect(world, c, r))) {
                        world.alive[r][c] = false;
                        world.score += rowPoints(r);
                        hitSound.play();
                        done = true;
                    }
                }
            }
            if (!done && world.bonus &&
                box.intersects(makeRect(world.bonus->x, kBonusY, kBonusW, kBonusH))) {
                world.score += kBonusScores[bonusPick(rng)];
                bonusSound.play();
                world.bonus.reset();
                done = true;
            }
            if (done) {
                world.playerBullet.reset();
            } else {
                world.playerBullet = p;
            }
        }

        // 적 탄: 방어벽 조각과 플레이어를 판정합니다.
        const meno::Rectf playerBox = makeRect(world.playerX, kPlayerY, kPlayerW, kPlayerH);
        bool lifeLost = false;
        for (std::size_t i = 0; i < world.shots.size();) {
            world.shots[i].pos += world.shots[i].vel * dt;
            const meno::Vec2f p = world.shots[i].pos;
            bool gone = p.y > kScreenH + 10.f || p.x < -10.f || p.x > kScreenW + 10.f;
            if (!gone) {
                gone = removeFirstPiece(world, [&](const meno::Rectf& piece) {
                    return circleHitsRect(p, kEnemyBulletRadius, piece);
                });
            }
            if (!gone && circleHitsRect(p, kEnemyBulletRadius, playerBox)) {
                lifeLost = true;
                gone = true;
            }
            if (gone) {
                world.shots.erase(world.shots.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
        if (lifeLost) {
            world.lives -= 1;
            loseSound.play();
            world.shots.clear();
            world.playerBullet.reset();
            if (world.lives <= 0) {
                (void)scenes.changeScene(resultScene.id());
                return;
            }
            world.freezeTimer = kFreezeTime;
            return;
        }

        // 적이 방어벽 조각에 겹치면 그 조각을 지웁니다.
        for (int b = 0; b < kBunkers; ++b) {
            for (int r = 0; r < kPieceRows; ++r) {
                for (int c = 0; c < kPieceCols; ++c) {
                    if (world.piece[b][r][c] && enemyOverlaps(world, pieceRect(b, r, c))) {
                        world.piece[b][r][c] = false;
                    }
                }
            }
        }

        // 적이 바닥(y 490)에 닿으면 결과 화면으로 갑니다.
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                if (world.alive[r][c] && enemyY(world, r) + kEnemyH >= kPlayerY) {
                    (void)scenes.changeScene(resultScene.id());
                    return;
                }
            }
        }

        // 웨이브 클리어: 다음 웨이브를 시작합니다.
        if (aliveCount(world) == 0) {
            world.wave += 1;
            startWave(world);
        }
    });
    playScene.setDrawHandler([&](meno::Renderer& r) {
        for (int b = 0; b < kBunkers; ++b) {
            for (int row = 0; row < kPieceRows; ++row) {
                for (int c = 0; c < kPieceCols; ++c) {
                    if (world.piece[b][row][c]) {
                        r.drawRect(pieceRect(b, row, c),
                                   {.fill = meno::Color::fromRgb(0x4CC26B)});
                    }
                }
            }
        }

        const int toggle = static_cast<int>(world.animTime / kAnimPeriod) % 2;
        for (int row = 0; row < kRows; ++row) {
            for (int c = 0; c < kCols; ++c) {
                if (world.alive[row][c]) {
                    const int cell = rowCellBase(row) + toggle;
                    r.draw(*enemyTex, {.position = {enemyX(world, c), enemyY(world, row)},
                                       .source = meno::Recti{cell * 32, 0, 32, 24}});
                }
            }
        }

        if (world.bonus) {
            r.draw(*bonusTex, {.position = {world.bonus->x, kBonusY}});
        }
        r.draw(*playerTex, {.position = {world.playerX, kPlayerY}});

        if (world.playerBullet) {
            r.drawRect(makeRect(world.playerBullet->x, world.playerBullet->y, kPlayerBulletW,
                                kPlayerBulletH),
                       {.fill = meno::colors::White});
        }
        for (const Shot& shot : world.shots) {
            r.drawCircle(shot.pos, kEnemyBulletRadius, {.fill = meno::Color::fromRgb(0xFF6B6B)});
        }

        drawLabel(r, *font, "SCORE " + std::to_string(world.score), 16.f, 12.f, 16, false);
        drawLabel(r, *font, "WAVE " + std::to_string(world.wave), 400.f, 12.f, 16, false);
        drawLabel(r, *font, "LIVES " + std::to_string(world.lives), 800.f, 12.f, 16, false);
    });

    // 결과
    resultScene.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(playScene.id());
        }
    });
    resultScene.setDrawHandler([&](meno::Renderer& r) {
        drawLabel(r, *font, "GAME OVER", kScreenW / 2.f, 160.f, 48, true);
        drawLabel(r, *font, "SCORE " + std::to_string(world.score), kScreenW / 2.f, 250.f, 32,
                  true);
        drawLabel(r, *font, "WAVE " + std::to_string(world.wave), kScreenW / 2.f, 300.f, 32,
                  true);
        drawLabel(r, *font, "R: RESTART  ESC: QUIT", kScreenW / 2.f, 440.f, 16, true);
    });

    (void)scenes.changeScene(titleScene.id());
    game.run();
}
```