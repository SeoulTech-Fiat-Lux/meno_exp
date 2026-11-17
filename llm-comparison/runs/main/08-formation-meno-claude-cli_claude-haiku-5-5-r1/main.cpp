#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kPlayerY = 490.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr int kLivesStart = 3;

struct Shot {
    meno::Vec2f pos;
    meno::Vec2f vel;
};

struct Bonus {
    bool active = false;
    float x = 0.f;
    float dir = 1.f;
};

struct Battle {
    int score = 0;
    int wave = 1;
    int lives = kLivesStart;
    std::array<bool, 32> enemyAlive{};
    float formationX = 0.f;
    float formationY = 0.f;
    float formationDir = 1.f;
    double waveTime = 0.0;
    double nextShotAt = 1.0;
    double animTimer = 0.0;
    int animPhase = 0;
    std::array<bool, 96> pieces{};
    bool playerShotActive = false;
    meno::Vec2f playerShot{};
    std::vector<Shot> enemyShots;
    float playerX = 460.f;
    double playTime = 0.0;
    double nextBonusAt = 20.0;
    int bonusSide = 0;
    Bonus bonus;
    double pauseLeft = 0.0;
    std::mt19937 rng{std::random_device{}()};
};

struct Sfx {
    meno::Sound shoot;
    meno::Sound hit;
    meno::Sound lose;
    meno::Sound bonusSfx;
};

struct Art {
    const meno::Texture& enemies;
    const meno::Texture& player;
    const meno::Texture& bonus;
    const meno::Font& font;
};

enum class Flow { Continue, Over };

meno::Vec2f enemyPos(const Battle& b, int i) {
    const int row = i / 8;
    const int col = i % 8;
    return meno::Vec2f{296.f + 48.f * static_cast<float>(col) + b.formationX,
                       80.f + 40.f * static_cast<float>(row) + b.formationY};
}

meno::Rectf pieceRect(int p) {
    const int barrier = p / 24;
    const int local = p % 24;
    const float left = 192.f + 192.f * static_cast<float>(barrier) - 24.f +
                       8.f * static_cast<float>(local % 6);
    const float top = 400.f + 8.f * static_cast<float>(local / 6);
    return meno::Rectf{meno::Vec2f{left, top}, meno::Vec2f{8.f, 8.f}};
}

bool circleHits(meno::Vec2f c, float r, const meno::Rectf& rect) {
    const float nx = std::clamp(c.x, rect.left(), rect.right());
    const float ny = std::clamp(c.y, rect.top(), rect.bottom());
    const float dx = c.x - nx;
    const float dy = c.y - ny;
    return dx * dx + dy * dy <= r * r;
}

int aliveCount(const Battle& b) {
    int n = 0;
    for (bool a : b.enemyAlive) {
        n += a ? 1 : 0;
    }
    return n;
}

void setupWave(Battle& b) {
    b.enemyAlive.fill(true);
    b.formationX = 0.f;
    b.formationY = std::min(16.f * static_cast<float>(b.wave - 1), 64.f);
    b.formationDir = 1.f;
    b.waveTime = 0.0;
    b.nextShotAt = 1.0;
    b.pieces.fill(true);
    b.playerShotActive = false;
    b.enemyShots.clear();
    b.bonus.active = false;
}

void resetAll(Battle& b) {
    b.score = 0;
    b.wave = 1;
    b.lives = kLivesStart;
    b.playerX = 460.f;
    b.playTime = 0.0;
    b.nextBonusAt = 20.0;
    b.bonusSide = 0;
    b.pauseLeft = 0.0;
    b.animTimer = 0.0;
    b.animPhase = 0;
    setupWave(b);
}

void fireEnemyShot(Battle& b) {
    if (b.enemyShots.size() >= 3) {
        return;
    }
    const float playerCx = b.playerX + 20.f;
    int bestCol = -1;
    float bestDist = 0.f;
    for (int c = 0; c < 8; ++c) {
        bool any = false;
        for (int r = 0; r < 4; ++r) {
            any = any || b.enemyAlive[r * 8 + c];
        }
        if (!any) {
            continue;
        }
        const float cx = 296.f + 48.f * static_cast<float>(c) + b.formationX + 16.f;
        const float d = std::abs(cx - playerCx);
        if (bestCol < 0 || d < bestDist) {
            bestCol = c;
            bestDist = d;
        }
    }
    if (bestCol < 0) {
        return;
    }
    int idx = -1;
    for (int r = 3; r >= 0 && idx < 0; --r) {
        if (b.enemyAlive[r * 8 + bestCol]) {
            idx = r * 8 + bestCol;
        }
    }
    const meno::Vec2f origin = enemyPos(b, idx) + meno::Vec2f{16.f, 24.f};
    const meno::Vec2f target{playerCx, kPlayerY + 12.f};
    const meno::Vec2f d = target - origin;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len <= 0.f) {
        return;
    }
    b.enemyShots.push_back(Shot{origin, d * (240.f / len)});
}

Flow stepPlay(Battle& b, const meno::Input& input, Sfx& sfx, double dtD) {
    if (b.pauseLeft > 0.0) {
        b.pauseLeft -= dtD;
        return Flow::Continue;
    }
    const float dt = static_cast<float>(dtD);

    // 플레이어 이동과 사격
    const auto held = [&](meno::Key key) { return input.isDown(key) || input.wasPressed(key); };
    if (held(meno::Key::Left) || held(meno::Key::A)) {
        b.playerX -= 300.f * dt;
    }
    if (held(meno::Key::Right) || held(meno::Key::D)) {
        b.playerX += 300.f * dt;
    }
    b.playerX = std::clamp(b.playerX, 16.f, 904.f);
    if (input.wasPressed(meno::Key::Space) && !b.playerShotActive) {
        b.playerShotActive = true;
        b.playerShot = meno::Vec2f{b.playerX + 18.f, kPlayerY - 12.f};
        sfx.shoot.play();
    }

    // 대열 이동과 가장자리 반전
    const int aliveN = aliveCount(b);
    const float speed = 40.f + 4.f * static_cast<float>(32 - aliveN);
    b.formationX += b.formationDir * speed * dt;
    float minLeft = 1e9f;
    float maxRight = -1e9f;
    for (int i = 0; i < 32; ++i) {
        if (b.enemyAlive[i]) {
            const float x = enemyPos(b, i).x;
            minLeft = std::min(minLeft, x);
            maxRight = std::max(maxRight, x + kEnemyW);
        }
    }
    if ((b.formationDir > 0.f && maxRight >= 944.f) || (b.formationDir < 0.f && minLeft <= 16.f)) {
        b.formationY += 16.f;
        b.formationDir = -b.formationDir;
    }

    // 적 그림 애니메이션(0.5초마다 번갈아 씀)
    b.animTimer += dtD;
    if (b.animTimer >= 0.5) {
        b.animTimer -= 0.5;
        b.animPhase ^= 1;
    }

    // 적 사격과 적 탄 이동
    b.waveTime += dtD;
    if (b.waveTime >= b.nextShotAt) {
        b.nextShotAt += 1.0;
        fireEnemyShot(b);
    }
    for (Shot& s : b.enemyShots) {
        s.pos += s.vel * dt;
    }

    // 적 탄과 플레이어·방어벽 판정
    bool lifeLost = false;
    const meno::Rectf playerRect{meno::Vec2f{b.playerX, kPlayerY}, meno::Vec2f{kPlayerW, kPlayerH}};
    std::erase_if(b.enemyShots, [&](const Shot& s) {
        if (s.pos.x < -10.f || s.pos.x > 970.f || s.pos.y < -10.f || s.pos.y > 550.f) {
            return true;
        }
        if (circleHits(s.pos, 3.f, playerRect)) {
            lifeLost = true;
            return true;
        }
        for (int p = 0; p < 96; ++p) {
            if (b.pieces[p] && circleHits(s.pos, 3.f, pieceRect(p))) {
                b.pieces[p] = false;
                return true;
            }
        }
        return false;
    });
    if (lifeLost) {
        --b.lives;
        sfx.lose.play();
        b.enemyShots.clear();
        b.playerShotActive = false;
        if (b.lives <= 0) {
            return Flow::Over;
        }
        b.pauseLeft = 1.0;
        return Flow::Continue;
    }

    // 적이 방어벽 조각에 겹치면 그 조각을 없앰
    for (int i = 0; i < 32; ++i) {
        if (!b.enemyAlive[i]) {
            continue;
        }
        const meno::Rectf e{enemyPos(b, i), meno::Vec2f{kEnemyW, kEnemyH}};
        for (int p = 0; p < 96; ++p) {
            if (b.pieces[p] && e.intersects(pieceRect(p))) {
                b.pieces[p] = false;
            }
        }
    }

    // 보너스 비행체
    b.playTime += dtD;
    if (b.playTime >= b.nextBonusAt) {
        b.nextBonusAt += 20.0;
        b.bonus.active = true;
        if (b.bonusSide == 0) {
            b.bonus.x = -48.f;
            b.bonus.dir = 1.f;
        } else {
            b.bonus.x = 960.f;
            b.bonus.dir = -1.f;
        }
        b.bonusSide ^= 1;
    }
    if (b.bonus.active) {
        b.bonus.x += b.bonus.dir * 120.f * dt;
        if ((b.bonus.dir > 0.f && b.bonus.x >= 960.f) ||
            (b.bonus.dir < 0.f && b.bonus.x + 48.f <= 0.f)) {
            b.bonus.active = false;
        }
    }

    // 플레이어 탄 이동과 판정
    if (b.playerShotActive) {
        b.playerShot.y -= 600.f * dt;
        if (b.playerShot.y + 12.f < 0.f) {
            b.playerShotActive = false;
        } else {
            const meno::Rectf shot{b.playerShot, meno::Vec2f{4.f, 12.f}};
            bool consumed = false;
            for (int i = 0; i < 32 && !consumed; ++i) {
                if (b.enemyAlive[i] &&
                    shot.intersects(meno::Rectf{enemyPos(b, i), meno::Vec2f{kEnemyW, kEnemyH}})) {
                    b.enemyAlive[i] = false;
                    const int row = i / 8;
                    b.score += row == 0 ? 30 : (row == 1 ? 20 : 10);
                    sfx.hit.play();
                    consumed = true;
                }
            }
            if (!consumed && b.bonus.active &&
                shot.intersects(meno::Rectf{meno::Vec2f{b.bonus.x, 50.f}, meno::Vec2f{48.f, 20.f}})) {
                static constexpr int kBonusPts[3] = {50, 100, 150};
                std::uniform_int_distribution<int> pick(0, 2);
                b.score += kBonusPts[pick(b.rng)];
                b.bonus.active = false;
                sfx.bonusSfx.play();
                consumed = true;
            }
            for (int p = 0; p < 96 && !consumed; ++p) {
                if (b.pieces[p] && shot.intersects(pieceRect(p))) {
                    b.pieces[p] = false;
                    consumed = true;
                }
            }
            if (consumed) {
                b.playerShotActive = false;
            }
        }
    }

    // 적이 y 490에 닿으면 게임 오버
    for (int i = 0; i < 32; ++i) {
        if (b.enemyAlive[i] && enemyPos(b, i).y >= kPlayerY) {
            return Flow::Over;
        }
    }

    // 적을 모두 없애면 다음 웨이브
    if (aliveCount(b) == 0) {
        ++b.wave;
        setupWave(b);
    }
    return Flow::Continue;
}

void drawLabel(meno::Renderer& r, const meno::Font& font, const std::string& text,
               meno::Vec2f pos, int size, bool centered) {
    meno::TextParams params{};
    params.characterSize = size;
    params.color = meno::colors::White;
    if (centered) {
        params.origin = r.measureTextBounds(font, text, params).center();
    }
    r.drawText(font, text, pos, params);
}

void drawBattle(const Battle& b, const Art& art, meno::Renderer& r) {
    for (int p = 0; p < 96; ++p) {
        if (b.pieces[p]) {
            r.drawRect(pieceRect(p), {.fill = meno::Color::fromRgb(0x4CC26B)});
        }
    }
    for (int i = 0; i < 32; ++i) {
        if (!b.enemyAlive[i]) {
            continue;
        }
        const int row = i / 8;
        const int cell = (row == 0 ? 0 : (row == 1 ? 2 : 4)) + b.animPhase;
        r.draw(art.enemies, {.position = enemyPos(b, i), .source = meno::Recti{cell * 32, 0, 32, 24}});
    }
    if (b.bonus.active) {
        r.draw(art.bonus, {.position = meno::Vec2f{b.bonus.x, 50.f}});
    }
    r.draw(art.player, {.position = meno::Vec2f{b.playerX, kPlayerY}});
    for (const Shot& s : b.enemyShots) {
        r.drawCircle(s.pos, 3.f, {.fill = meno::Color::fromRgb(0xFF6B6B)});
    }
    if (b.playerShotActive) {
        r.drawRect(meno::Rectf{b.playerShot, meno::Vec2f{4.f, 12.f}}, {.fill = meno::colors::White});
    }
    drawLabel(r, art.font, "SCORE " + std::to_string(b.score), meno::Vec2f{16.f, 8.f}, 16, false);
    drawLabel(r, art.font, "WAVE " + std::to_string(b.wave), meno::Vec2f{400.f, 8.f}, 16, false);
    drawLabel(r, art.font, "LIVES " + std::to_string(b.lives), meno::Vec2f{800.f, 8.f}, 16, false);
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "STAR FORMATION",
                     .clearColor = meno::Color::fromRgb(0x000000)}};

    const auto fontRes = game.resources().loadFont("assets/fonts/game.ttf");
    const auto enemiesRes = game.resources().loadTexture("assets/textures/fm_enemies.png");
    const auto playerRes = game.resources().loadTexture("assets/textures/fm_player.png");
    const auto bonusRes = game.resources().loadTexture("assets/textures/fm_bonus.png");
    const auto shootBuf = game.resources().loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuf = game.resources().loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuf = game.resources().loadSoundBuffer("assets/sfx/lose.wav");
    const auto bonusBuf = game.resources().loadSoundBuffer("assets/sfx/bonus.wav");
    std::optional<meno::Music> music = meno::Music::openFromFile("assets/music/bgm_formation.ogg");

    std::vector<const char*> missing;
    const auto check = [&](bool ok, const char* path) {
        if (!ok) {
            missing.push_back(path);
        }
    };
    check(fontRes != nullptr, "assets/fonts/game.ttf");
    check(enemiesRes != nullptr, "assets/textures/fm_enemies.png");
    check(playerRes != nullptr, "assets/textures/fm_player.png");
    check(bonusRes != nullptr, "assets/textures/fm_bonus.png");
    check(shootBuf != nullptr, "assets/sfx/shoot.wav");
    check(hitBuf != nullptr, "assets/sfx/hit.wav");
    check(loseBuf != nullptr, "assets/sfx/lose.wav");
    check(bonusBuf != nullptr, "assets/sfx/bonus.wav");
    check(music.has_value(), "assets/music/bgm_formation.ogg");
    if (!missing.empty()) {
        for (const char* path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    if (music) {
        music->setLooping(true);
        music->setVolume(0.4f);
    }

    Sfx sfx{meno::Sound{shootBuf}, meno::Sound{hitBuf}, meno::Sound{loseBuf}, meno::Sound{bonusBuf}};
    const Art art{*enemiesRes, *playerRes, *bonusRes, *fontRes};
    const meno::Font& font = *fontRes;

    Battle battle;
    resetAll(battle);
    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // 타이틀: Enter 또는 2초 뒤 플레이 시작
    double titleTime = 0.0;
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
    title.setDrawHandler([&](meno::Renderer& r) {
        drawLabel(r, font, "STAR FORMATION", meno::Vec2f{480.f, 180.f}, 48, true);
        drawLabel(r, font, "PRESS ENTER", meno::Vec2f{480.f, 300.f}, 32, true);
    });

    // 플레이: 들어올 때마다 상태 초기화, 음악 재생
    play.setEnterHandler([&] {
        resetAll(battle);
        if (music) {
            music->play();
        }
    });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (stepPlay(battle, input, sfx, dt) == Flow::Over) {
            (void)scenes.changeScene(result.id());
        }
    });
    play.setExitHandler([&] {
        if (music) {
            music->stop();
        }
    });
    play.setDrawHandler([&](meno::Renderer& r) { drawBattle(battle, art, r); });

    // 결과: R로 다시 시작, Esc로 종료
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
    result.setDrawHandler([&](meno::Renderer& r) {
        drawLabel(r, font, "GAME OVER", meno::Vec2f{480.f, 160.f}, 48, true);
        drawLabel(r, font, "SCORE " + std::to_string(battle.score), meno::Vec2f{480.f, 250.f}, 32, true);
        drawLabel(r, font, "WAVE " + std::to_string(battle.wave), meno::Vec2f{480.f, 300.f}, 32, true);
        drawLabel(r, font, "R: RESTART  ESC: QUIT", meno::Vec2f{480.f, 420.f}, 16, true);
    });

    (void)scenes.changeScene(title.id());
    game.run();
}
