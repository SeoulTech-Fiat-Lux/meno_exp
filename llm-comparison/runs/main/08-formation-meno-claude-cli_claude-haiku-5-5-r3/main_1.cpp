#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float kWidth = 960.f;
constexpr float kHeight = 540.f;
constexpr float kEdgeMargin = 16.f;
constexpr float kEdgeLeft = 16.f;
constexpr float kEdgeRight = 944.f;

constexpr int kCols = 8;
constexpr int kRows = 4;
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr float kFormBaseX = 296.f;
constexpr float kFormBaseY = 80.f;
constexpr float kColStep = 48.f;
constexpr float kRowStep = 40.f;
constexpr float kFormDrop = 16.f;

constexpr float kPlayerY = 490.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerSpeed = 300.f;
constexpr float kPlayerShotSpeed = 600.f;
constexpr float kEnemyShotSpeed = 240.f;
constexpr std::size_t kEnemyShotCap = 3;

constexpr float kBonusY = 50.f;
constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusSpeed = 120.f;

constexpr int kWalls = 4;
constexpr int kWallRows = 4;
constexpr int kWallCols = 6;
constexpr float kWallTop = 400.f;
constexpr float kPiece = 8.f;
constexpr std::array<float, kWalls> kWallCenters{192.f, 384.f, 576.f, 768.f};

enum class Screen { Title, Play, Result };

struct Assets {
    std::shared_ptr<const meno::Font> font;
    std::shared_ptr<const meno::Texture> enemies;
    std::shared_ptr<const meno::Texture> player;
    std::shared_ptr<const meno::Texture> bonus;
    std::optional<meno::Sound> shoot;
    std::optional<meno::Sound> hit;
    std::optional<meno::Sound> lose;
    std::optional<meno::Sound> bonusSfx;
    std::optional<meno::Music> bgm;
};

struct EnemyShot {
    meno::Vec2f pos;
    meno::Vec2f vel;
};

struct Bonus {
    float x;
    float vx;
};

struct State {
    Screen screen = Screen::Title;
    double titleTimer = 0.0;
    int score = 0;
    int wave = 1;
    int lives = 3;

    float playerX = 460.f;

    std::array<std::array<bool, kRows>, kCols> enemies{};
    float formX = 0.f;
    float formY = 0.f;
    float formDir = 1.f;
    double animTimer = 0.0;
    int animPhase = 0;
    double shootTimer = 1.0;

    double freezeTimer = 0.0;

    double bonusTimer = 20.0;
    bool bonusFromLeft = true;
    std::optional<Bonus> bonus;

    std::array<std::array<std::array<bool, kWallCols>, kWallRows>, kWalls> walls{};

    std::optional<meno::Rectf> playerShot;
    std::vector<EnemyShot> enemyShots;

    std::mt19937 rng{std::random_device{}()};
};

float enemyX(const State& s, int c) {
    return kFormBaseX + kColStep * static_cast<float>(c) + s.formX;
}

float enemyY(const State& s, int r) {
    return kFormBaseY + kRowStep * static_cast<float>(r) + s.formY;
}

meno::Rectf enemyBox(const State& s, int c, int r) {
    return meno::Rectf{{enemyX(s, c), enemyY(s, r)}, {kEnemyW, kEnemyH}};
}

int countEnemies(const State& s) {
    int n = 0;
    for (const auto& col : s.enemies) {
        for (bool alive : col) {
            n += alive ? 1 : 0;
        }
    }
    return n;
}

int rowPoints(int r) {
    return r == 0 ? 30 : (r == 1 ? 20 : 10);
}

int rowGroup(int r) {
    return std::min(r, 2);
}

meno::Rectf pieceBox(int w, int row, int col) {
    return meno::Rectf{{kWallCenters[w] - 24.f + kPiece * static_cast<float>(col),
                        kWallTop + kPiece * static_cast<float>(row)},
                       {kPiece, kPiece}};
}

meno::Rectf playerBox(const State& s) {
    return meno::Rectf{{s.playerX, kPlayerY}, {kPlayerW, kPlayerH}};
}

meno::Rectf bonusBox(const Bonus& b) {
    return meno::Rectf{{b.x, kBonusY}, {kBonusW, kBonusH}};
}

// 방어벽 조각 하나와 겹치면 그 조각을 없애고 true를 돌려줍니다.
bool hitWall(State& s, const meno::Rectf& box) {
    for (int w = 0; w < kWalls; ++w) {
        for (int row = 0; row < kWallRows; ++row) {
            for (int col = 0; col < kWallCols; ++col) {
                bool& piece = s.walls[w][row][col];
                if (piece && box.intersects(pieceBox(w, row, col))) {
                    piece = false;
                    return true;
                }
            }
        }
    }
    return false;
}

// 적이 겹친 방어벽 조각은 모두 없앱니다.
void enemiesEatWalls(State& s) {
    for (int c = 0; c < kCols; ++c) {
        for (int r = 0; r < kRows; ++r) {
            if (!s.enemies[c][r]) {
                continue;
            }
            const meno::Rectf box = enemyBox(s, c, r);
            for (int w = 0; w < kWalls; ++w) {
                for (int row = 0; row < kWallRows; ++row) {
                    for (int col = 0; col < kWallCols; ++col) {
                        bool& piece = s.walls[w][row][col];
                        if (piece && box.intersects(pieceBox(w, row, col))) {
                            piece = false;
                        }
                    }
                }
            }
        }
    }
}

bool hitEnemy(State& s, Assets& a, const meno::Rectf& box) {
    for (int c = 0; c < kCols; ++c) {
        for (int r = 0; r < kRows; ++r) {
            if (s.enemies[c][r] && box.intersects(enemyBox(s, c, r))) {
                s.enemies[c][r] = false;
                s.score += rowPoints(r);
                if (a.hit) {
                    a.hit->play();
                }
                return true;
            }
        }
    }
    return false;
}

bool hitBonus(State& s, Assets& a, const meno::Rectf& box) {
    if (!s.bonus || !box.intersects(bonusBox(*s.bonus))) {
        return false;
    }
    const std::array<int, 3> points{50, 100, 150};
    std::uniform_int_distribution<int> pick(0, 2);
    s.score += points[pick(s.rng)];
    s.bonus.reset();
    if (a.bonusSfx) {
        a.bonusSfx->play();
    }
    return true;
}

void spawnWave(State& s) {
    for (auto& col : s.enemies) {
        col.fill(true);
    }
    s.formX = 0.f;
    s.formY = std::min(kFormDrop * static_cast<float>(s.wave - 1), 64.f);
    s.formDir = 1.f;
    for (auto& wall : s.walls) {
        for (auto& row : wall) {
            row.fill(true);
        }
    }
    s.shootTimer = 1.0;
    s.enemyShots.clear();
    s.playerShot.reset();
    s.bonus.reset();
}

void resetGame(State& s) {
    s.wave = 1;
    s.score = 0;
    s.lives = 3;
    s.playerX = (kWidth - kPlayerW) / 2.f;
    s.animTimer = 0.0;
    s.animPhase = 0;
    s.freezeTimer = 0.0;
    s.bonusTimer = 20.0;
    s.bonusFromLeft = true;
    spawnWave(s);
}

void startPlay(State& s, Assets& a) {
    resetGame(s);
    s.screen = Screen::Play;
    if (a.bgm) {
        a.bgm->play();
    }
}

void enterResult(State& s, Assets& a) {
    s.screen = Screen::Result;
    if (a.bgm) {
        a.bgm->stop();
    }
}

// 남은 적 중 플레이어와 x 거리가 가장 가까운 열의 맨 아래 적이 쏩니다.
void fireEnemyShot(State& s) {
    if (s.enemyShots.size() >= kEnemyShotCap) {
        return;
    }
    const float playerCenterX = s.playerX + kPlayerW / 2.f;

    int bestCol = -1;
    int bestRow = -1;
    float bestDist = 1e9f;
    for (int c = 0; c < kCols; ++c) {
        int bottom = -1;
        for (int r = 0; r < kRows; ++r) {
            if (s.enemies[c][r]) {
                bottom = r;
            }
        }
        if (bottom < 0) {
            continue;
        }
        const float dist = std::fabs(enemyX(s, c) + kEnemyW / 2.f - playerCenterX);
        if (dist < bestDist) {
            bestDist = dist;
            bestCol = c;
            bestRow = bottom;
        }
    }
    if (bestCol < 0) {
        return;
    }

    const meno::Vec2f origin{enemyX(s, bestCol) + kEnemyW / 2.f,
                             enemyY(s, bestRow) + kEnemyH};
    const meno::Vec2f target{playerCenterX, kPlayerY + kPlayerH / 2.f};
    const meno::Vec2f delta = target - origin;
    const float length = std::hypot(delta.x, delta.y);
    if (length <= 0.f) {
        return;
    }
    s.enemyShots.push_back(EnemyShot{origin, delta * (kEnemyShotSpeed / length)});
}

void updatePlay(State& s, Assets& a, const meno::Input& input, float dt) {
    // 피격 후 1초 동안은 모든 것이 멈춥니다.
    if (s.freezeTimer > 0.0) {
        s.freezeTimer -= dt;
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
    s.playerX = std::clamp(s.playerX + move * kPlayerSpeed * dt, kEdgeMargin,
                           kWidth - kEdgeMargin - kPlayerW);

    // 플레이어 총알: 발사와 이동, 충돌
    if (!s.playerShot && input.wasPressed(meno::Key::Space)) {
        s.playerShot = meno::Rectf{{s.playerX + 18.f, kPlayerY - 12.f}, {4.f, 12.f}};
        if (a.shoot) {
            a.shoot->play();
        }
    }
    if (s.playerShot) {
        meno::Rectf& shot = *s.playerShot;
        shot.position.y -= kPlayerShotSpeed * dt;
        bool gone = shot.position.y + shot.size.y < 0.f;
        gone = gone || hitWall(s, shot);
        gone = gone || hitEnemy(s, a, shot);
        gone = gone || hitBonus(s, a, shot);
        if (gone) {
            s.playerShot.reset();
        }
    }

    // 적을 모두 없앴으면 다음 웨이브
    if (countEnemies(s) == 0) {
        s.wave += 1;
        spawnWave(s);
        return;
    }

    // 적 탄 이동과 충돌
    for (EnemyShot& shot : s.enemyShots) {
        shot.pos += shot.vel * dt;
    }
    bool playerHit = false;
    const meno::Rectf player = playerBox(s);
    std::erase_if(s.enemyShots, [&](const EnemyShot& shot) {
        const meno::Rectf box{{shot.pos.x - 3.f, shot.pos.y - 3.f}, {6.f, 6.f}};
        if (box.intersects(player)) {
            playerHit = true;
            return true;
        }
        const bool outside = shot.pos.x < -10.f || shot.pos.x > kWidth + 10.f ||
                             shot.pos.y < -10.f || shot.pos.y > kHeight + 10.f;
        return outside || hitWall(s, box);
    });

    if (playerHit) {
        s.lives -= 1;
        if (a.lose) {
            a.lose->play();
        }
        s.enemyShots.clear();
        s.playerShot.reset();
        if (s.lives <= 0) {
            enterResult(s, a);
        } else {
            s.freezeTimer = 1.0;
        }
        return;
    }

    // 대열 이동과 가장자리에서의 방향 전환
    const int alive = countEnemies(s);
    const float speed = 40.f + 4.f * static_cast<float>(kCols * kRows - alive);
    s.formX += s.formDir * speed * dt;

    float minLeft = 1e9f;
    float maxRight = -1e9f;
    for (int c = 0; c < kCols; ++c) {
        for (int r = 0; r < kRows; ++r) {
            if (s.enemies[c][r]) {
                minLeft = std::min(minLeft, enemyX(s, c));
                maxRight = std::max(maxRight, enemyX(s, c) + kEnemyW);
            }
        }
    }
    if (s.formDir > 0.f && maxRight >= kEdgeRight) {
        s.formDir = -1.f;
        s.formY += kFormDrop;
    } else if (s.formDir < 0.f && minLeft <= kEdgeLeft) {
        s.formDir = 1.f;
        s.formY += kFormDrop;
    }

    // 적이 y 490에 닿으면 게임 오버
    for (int c = 0; c < kCols; ++c) {
        for (int r = 0; r < kRows; ++r) {
            if (s.enemies[c][r] && enemyY(s, r) >= kPlayerY) {
                enterResult(s, a);
                return;
            }
        }
    }

    // 적 그림 애니메이션 (0.5초마다 전환)
    s.animTimer += dt;
    if (s.animTimer >= 0.5) {
        s.animTimer -= 0.5;
        s.animPhase ^= 1;
    }

    enemiesEatWalls(s);

    // 1초 뒤부터 1초마다 한 발씩 쏩니다.
    s.shootTimer -= dt;
    if (s.shootTimer <= 0.0) {
        s.shootTimer += 1.0;
        fireEnemyShot(s);
    }

    // 보너스 비행체: 20초마다 번갈아 한쪽 화면 밖에서 출발
    s.bonusTimer -= dt;
    if (s.bonusTimer <= 0.0) {
        s.bonusTimer += 20.0;
        s.bonus = s.bonusFromLeft ? Bonus{-48.f, kBonusSpeed} : Bonus{kWidth, -kBonusSpeed};
        s.bonusFromLeft = !s.bonusFromLeft;
    }
    if (s.bonus) {
        s.bonus->x += s.bonus->vx * dt;
        const bool leftToRight = s.bonus->vx > 0.f && s.bonus->x >= kWidth;
        const bool rightToLeft = s.bonus->vx < 0.f && s.bonus->x <= -kBonusW;
        if (leftToRight || rightToLeft) {
            s.bonus.reset();
        }
    }
}

void drawCentered(meno::Renderer& r, const meno::Font& font, const std::string& text, float y,
                  unsigned int size) {
    meno::TextParams params{};
    params.characterSize = size;
    params.color = meno::colors::White;
    params.origin = r.measureTextBounds(font, text, params).center();
    r.drawText(font, text, {kWidth / 2.f, y}, params);
}

void drawHud(meno::Renderer& r, const meno::Font& font, const State& s) {
    r.drawText(font, "SCORE " + std::to_string(s.score), {16.f, 8.f},
               {.characterSize = 16, .color = meno::colors::White});
    r.drawText(font, "WAVE " + std::to_string(s.wave), {400.f, 8.f},
               {.characterSize = 16, .color = meno::colors::White});
    r.drawText(font, "LIVES " + std::to_string(s.lives), {800.f, 8.f},
               {.characterSize = 16, .color = meno::colors::White});
}

void drawPlay(const State& s, const Assets& a, meno::Renderer& r) {
    // 그리는 순서: 방어벽 → 적 → 보너스 → 플레이어 → 탄 → 점수
    const meno::Color wallColor = meno::Color::fromRgb(0x4CC26B);
    for (int w = 0; w < kWalls; ++w) {
        for (int row = 0; row < kWallRows; ++row) {
            for (int col = 0; col < kWallCols; ++col) {
                if (s.walls[w][row][col]) {
                    r.drawRect(pieceBox(w, row, col), {.fill = wallColor});
                }
            }
        }
    }

    if (a.enemies) {
        for (int c = 0; c < kCols; ++c) {
            for (int row = 0; row < kRows; ++row) {
                if (!s.enemies[c][row]) {
                    continue;
                }
                const int cell = rowGroup(row) * 2 + s.animPhase;
                r.draw(*a.enemies, {.position = {enemyX(s, c), enemyY(s, row)},
                                    .source = meno::Recti{cell * 32, 0, 32, 24}});
            }
        }
    }

    if (s.bonus && a.bonus) {
        r.draw(*a.bonus, {.position = {s.bonus->x, kBonusY}});
    }

    if (a.player) {
        r.draw(*a.player, {.position = {s.playerX, kPlayerY}});
    }

    if (s.playerShot) {
        r.drawRect(*s.playerShot, {.fill = meno::colors::White});
    }
    for (const EnemyShot& shot : s.enemyShots) {
        r.drawCircle(shot.pos, 3.f, {.fill = meno::Color::fromRgb(0xFF6B6B)});
    }

    if (a.font) {
        drawHud(r, *a.font, s);
    }
}

bool loadAssets(Assets& a, meno::ResourceManager& res) {
    bool ok = true;
    const auto check = [&ok](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            ok = false;
        }
    };

    a.font = res.loadFont("assets/fonts/game.ttf");
    check(a.font != nullptr, "assets/fonts/game.ttf");

    a.enemies = res.loadTexture("assets/textures/fm_enemies.png");
    check(a.enemies != nullptr, "assets/textures/fm_enemies.png");
    a.player = res.loadTexture("assets/textures/fm_player.png");
    check(a.player != nullptr, "assets/textures/fm_player.png");
    a.bonus = res.loadTexture("assets/textures/fm_bonus.png");
    check(a.bonus != nullptr, "assets/textures/fm_bonus.png");

    const auto loadSfx = [&](std::optional<meno::Sound>& sound, const char* path) {
        const auto buffer = res.loadSoundBuffer(path);
        check(buffer != nullptr, path);
        if (buffer) {
            sound.emplace(buffer);
        }
    };
    loadSfx(a.shoot, "assets/sfx/shoot.wav");
    loadSfx(a.hit, "assets/sfx/hit.wav");
    loadSfx(a.lose, "assets/sfx/lose.wav");
    loadSfx(a.bonusSfx, "assets/sfx/bonus.wav");

    if (auto music = meno::Music::openFromFile("assets/music/bgm_formation.ogg")) {
        a.bgm.emplace(std::move(*music));
        a.bgm->setLooping(true);
        a.bgm->setVolume(0.4f);
    } else {
        check(false, "assets/music/bgm_formation.ogg");
    }

    return ok;
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "STAR FORMATION",
                     .clearColor = meno::colors::Black}};

    Assets assets;
    if (!loadAssets(assets, game.resources())) {
        return 1;
    }

    State state;
    meno::Scene& scene = game.scenes().createScene();

    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDeltaTime);
        switch (state.screen) {
            case Screen::Title:
                state.titleTimer += fixedDeltaTime;
                if (input.wasPressed(meno::Key::Enter) || state.titleTimer >= 2.0) {
                    startPlay(state, assets);
                }
                break;
            case Screen::Play:
                updatePlay(state, assets, input, dt);
                break;
            case Screen::Result:
                if (input.wasPressed(meno::Key::R)) {
                    startPlay(state, assets);
                }
                break;
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        if (!assets.font) {
            return;
        }
        const meno::Font& font = *assets.font;
        switch (state.screen) {
            case Screen::Title:
                drawCentered(renderer, font, "STAR FORMATION", 200.f, 48);
                drawCentered(renderer, font, "PRESS ENTER", 300.f, 32);
                break;
            case Screen::Play:
                drawPlay(state, assets, renderer);
                break;
            case Screen::Result:
                drawCentered(renderer, font, "GAME OVER", 200.f, 48);
                drawCentered(renderer, font, "SCORE " + std::to_string(state.score), 260.f, 32);
                drawCentered(renderer, font, "WAVE " + std::to_string(state.wave), 300.f, 32);
                drawCentered(renderer, font, "R: RESTART  ESC: QUIT", 380.f, 16);
                break;
        }
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
