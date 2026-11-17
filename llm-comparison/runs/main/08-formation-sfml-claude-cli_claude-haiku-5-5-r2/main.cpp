#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// 상수
// ---------------------------------------------------------------------------
constexpr float kStep = 1.f / 60.f; // 고정 시뮬레이션 간격(초)
constexpr float kScreenW = 960.f;
constexpr float kScreenH = 540.f;

constexpr int   kCols = 8;
constexpr int   kRows = 4;
constexpr int   kEnemyCount = kCols * kRows;
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr float kFormX0 = 296.f;
constexpr float kFormY0 = 80.f;
constexpr float kColGap = 48.f;
constexpr float kRowGap = 40.f;
constexpr float kFormLeftEdge = 16.f;
constexpr float kFormRightEdge = 944.f;
constexpr float kFormDrop = 16.f;
constexpr float kFormDropMax = 64.f;
constexpr float kEnemyBottomLimit = 490.f;
constexpr float kEnemyShotInterval = 1.f;
constexpr int   kMaxEnemyBullets = 3;

constexpr float kPlayerY = 490.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerSpeed = 300.f;
constexpr float kPlayerMinX = 16.f;
constexpr float kPlayerMaxX = kScreenW - 16.f - kPlayerW;

constexpr float kPBulletW = 4.f;
constexpr float kPBulletH = 12.f;
constexpr float kPBulletSpeed = 600.f;

constexpr float kEBulletR = 3.f;
constexpr float kEBulletSpeed = 240.f;

constexpr float kBarrierY = 400.f;
constexpr float kPieceSize = 8.f;
constexpr int   kPieceCols = 6;
constexpr int   kPieceRows = 4;
constexpr float kBarrierCenters[] = {192.f, 384.f, 576.f, 768.f};

constexpr float kBonusY = 50.f;
constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusSpeed = 120.f;
constexpr float kBonusInterval = 20.f;
constexpr int   kBonusPoints[] = {50, 100, 150};

constexpr float kFreezeTime = 1.f;
constexpr float kTitleTime = 2.f;
constexpr int   kStartLives = 3;

// ---------------------------------------------------------------------------
// 상태 타입
// ---------------------------------------------------------------------------
enum class Screen { Title, Play, Result };
enum class Align { Left, Center, Right };

struct Piece {
    float x = 0.f;
    float y = 0.f;
    bool alive = true;
};

struct EnemyBullet {
    sf::Vector2f pos{};
    sf::Vector2f vel{};
};

struct Bonus {
    float x = 0.f;
    float dir = 1.f;
};

struct Input {
    bool left = false;
    bool right = false;
    bool fire = false;
    bool enter = false;
    bool restart = false;
};

struct Game {
    Screen screen = Screen::Title;
    float titleTimer = 0.f;

    int score = 0;
    int lives = kStartLives;
    int wave = 1;

    float playerX = 460.f;
    bool playerBulletActive = false;
    sf::Vector2f playerBullet{0.f, 0.f};

    std::array<bool, kEnemyCount> alive{};
    float formX = 0.f;
    float formY = 0.f;
    int formDir = 1;
    float shotTimer = kEnemyShotInterval;
    float animTimer = 0.f;
    int animPhase = 0;

    std::vector<EnemyBullet> enemyBullets;
    std::vector<Piece> pieces;
    std::vector<Bonus> bonuses;
    float playTime = 0.f;
    float nextBonusAt = kBonusInterval;
    int bonusCount = 0;
    float freezeTimer = 0.f;

    std::mt19937 rng{std::random_device{}()};
};

// ---------------------------------------------------------------------------
// 에셋
// ---------------------------------------------------------------------------
struct Sfx {
    sf::SoundBuffer buffer;
    std::unique_ptr<sf::Sound> sound;

    bool load(const char* path) {
        if (!buffer.loadFromFile(path)) {
            return false;
        }
        sound = std::make_unique<sf::Sound>(buffer);
        return true;
    }

    void play() {
        if (sound) {
            sound->stop();
            sound->play();
        }
    }
};

struct Assets {
    sf::Font font;
    sf::Texture enemies;
    sf::Texture player;
    sf::Texture bonus;
    Sfx shoot;
    Sfx hit;
    Sfx lose;
    Sfx bonusSfx;
    sf::Music music;
};

bool need(bool ok, const char* path) {
    if (!ok) {
        std::cerr << "failed to load: " << path << '\n';
    }
    return ok;
}

bool loadAssets(Assets& a) {
    return need(a.font.openFromFile("assets/fonts/game.ttf"), "assets/fonts/game.ttf")
        && need(a.enemies.loadFromFile("assets/textures/fm_enemies.png"), "assets/textures/fm_enemies.png")
        && need(a.player.loadFromFile("assets/textures/fm_player.png"), "assets/textures/fm_player.png")
        && need(a.bonus.loadFromFile("assets/textures/fm_bonus.png"), "assets/textures/fm_bonus.png")
        && need(a.shoot.load("assets/sfx/shoot.wav"), "assets/sfx/shoot.wav")
        && need(a.hit.load("assets/sfx/hit.wav"), "assets/sfx/hit.wav")
        && need(a.lose.load("assets/sfx/lose.wav"), "assets/sfx/lose.wav")
        && need(a.bonusSfx.load("assets/sfx/bonus.wav"), "assets/sfx/bonus.wav")
        && need(a.music.openFromFile("assets/music/bgm_formation.ogg"), "assets/music/bgm_formation.ogg");
}

// ---------------------------------------------------------------------------
// 유틸
// ---------------------------------------------------------------------------
bool overlaps(float ax, float ay, float aw, float ah,
              float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

bool circleHitsRect(sf::Vector2f c, float r, float x, float y, float w, float h) {
    const float nx = std::clamp(c.x, x, x + w);
    const float ny = std::clamp(c.y, y, y + h);
    const float dx = c.x - nx;
    const float dy = c.y - ny;
    return dx * dx + dy * dy <= r * r;
}

sf::Vector2f enemyPos(const Game& g, int r, int c) {
    return sf::Vector2f{kFormX0 + kColGap * static_cast<float>(c) + g.formX,
                        kFormY0 + kRowGap * static_cast<float>(r) + g.formY};
}

int enemyScore(int r) {
    if (r == 0) return 30;
    if (r == 1) return 20;
    return 10;
}

// ---------------------------------------------------------------------------
// 게임 상태 초기화
// ---------------------------------------------------------------------------
void refillBarriers(Game& g) {
    g.pieces.clear();
    for (float cx : kBarrierCenters) {
        const float left = cx - 24.f;
        for (int r = 0; r < kPieceRows; ++r) {
            for (int c = 0; c < kPieceCols; ++c) {
                g.pieces.push_back(Piece{left + kPieceSize * static_cast<float>(c),
                                         kBarrierY + kPieceSize * static_cast<float>(r),
                                         true});
            }
        }
    }
}

void spawnWave(Game& g) {
    g.formX = 0.f;
    g.formY = std::min(kFormDrop * static_cast<float>(g.wave - 1), kFormDropMax);
    g.formDir = 1;
    g.alive.fill(true);
    g.shotTimer = kEnemyShotInterval;
    g.enemyBullets.clear();
    g.bonuses.clear();
    g.playerBulletActive = false;
    refillBarriers(g);
}

void resetGame(Game& g) {
    g.score = 0;
    g.lives = kStartLives;
    g.wave = 1;
    g.playerX = 460.f;
    g.playTime = 0.f;
    g.nextBonusAt = kBonusInterval;
    g.bonusCount = 0;
    g.freezeTimer = 0.f;
    g.animTimer = 0.f;
    g.animPhase = 0;
    spawnWave(g);
}

void startPlay(Game& g, Assets& a) {
    resetGame(g);
    g.screen = Screen::Play;
    a.music.setVolume(40.f);
    a.music.setLooping(true);
    a.music.stop();
    a.music.play();
}

void enterResult(Game& g, Assets& a) {
    g.screen = Screen::Result;
    a.music.stop();
}

// ---------------------------------------------------------------------------
// 시뮬레이션
// ---------------------------------------------------------------------------
void moveFormation(Game& g) {
    int aliveCount = 0;
    for (bool al : g.alive) {
        if (al) ++aliveCount;
    }
    if (aliveCount == 0) return;

    const float speed = 40.f + 4.f * static_cast<float>(kEnemyCount - aliveCount);
    g.formX += static_cast<float>(g.formDir) * speed * kStep;

    float minLeft = 1e9f;
    float maxRight = -1e9f;
    for (int i = 0; i < kEnemyCount; ++i) {
        if (!g.alive[i]) continue;
        const float x = kFormX0 + kColGap * static_cast<float>(i % kCols) + g.formX;
        minLeft = std::min(minLeft, x);
        maxRight = std::max(maxRight, x + kEnemyW);
    }

    if (g.formDir > 0 && maxRight >= kFormRightEdge) {
        g.formDir = -1;
        g.formY += kFormDrop;
    } else if (g.formDir < 0 && minLeft <= kFormLeftEdge) {
        g.formDir = 1;
        g.formY += kFormDrop;
    }
}

void moveBonuses(Game& g) {
    for (auto& b : g.bonuses) {
        b.x += b.dir * kBonusSpeed * kStep;
    }
    std::erase_if(g.bonuses, [](const Bonus& b) {
        return (b.dir > 0.f && b.x > kScreenW) || (b.dir < 0.f && b.x + kBonusW < 0.f);
    });
}

void fireEnemyShot(Game& g) {
    if (static_cast<int>(g.enemyBullets.size()) >= kMaxEnemyBullets) return;

    const float playerCx = g.playerX + kPlayerW / 2.f;
    int bestC = -1;
    float bestD = 1e9f;
    for (int c = 0; c < kCols; ++c) {
        bool any = false;
        for (int r = 0; r < kRows; ++r) {
            if (g.alive[r * kCols + c]) { any = true; break; }
        }
        if (!any) continue;
        const float cx = kFormX0 + kColGap * static_cast<float>(c) + g.formX + kEnemyW / 2.f;
        const float d = std::fabs(cx - playerCx);
        if (d < bestD) { // 같은 거리면 먼저 나온(왼쪽) 열 유지
            bestD = d;
            bestC = c;
        }
    }
    if (bestC < 0) return;

    int bestR = -1;
    for (int r = 0; r < kRows; ++r) {
        if (g.alive[r * kCols + bestC]) bestR = r; // 가장 아래 적
    }
    if (bestR < 0) return;

    const sf::Vector2f ep = enemyPos(g, bestR, bestC);
    const sf::Vector2f start{ep.x + kEnemyW / 2.f, ep.y + kEnemyH};
    const sf::Vector2f target{g.playerX + kPlayerW / 2.f, kPlayerY + kPlayerH / 2.f};
    const float dx = target.x - start.x;
    const float dy = target.y - start.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.f) return;

    g.enemyBullets.push_back(EnemyBullet{
        start, sf::Vector2f{dx / len * kEBulletSpeed, dy / len * kEBulletSpeed}});
}

void updatePlay(Game& g, Assets& a, const Input& in) {
    // 사건 후 1초 정지: 모든 진행을 멈춤 (입력도 무시)
    if (g.freezeTimer > 0.f) {
        g.freezeTimer -= kStep;
        return;
    }

    g.playTime += kStep;

    g.animTimer += kStep;
    if (g.animTimer >= 0.5f) {
        g.animTimer -= 0.5f;
        g.animPhase ^= 1;
    }

    // 1. 플레이어 이동
    const int dirInput = (in.right ? 1 : 0) - (in.left ? 1 : 0);
    g.playerX = std::clamp(g.playerX + static_cast<float>(dirInput) * kPlayerSpeed * kStep,
                           kPlayerMinX, kPlayerMaxX);

    // 2. 발사 (누른 순간, 동시에 하나)
    if (in.fire && !g.playerBulletActive) {
        g.playerBulletActive = true;
        g.playerBullet = sf::Vector2f{g.playerX + kPlayerW / 2.f - kPBulletW / 2.f,
                                      kPlayerY - kPBulletH};
        a.shoot.play();
    }

    // 3. 대열 이동
    moveFormation(g);

    // 4. 보너스 비행체 생성 및 이동
    if (g.playTime >= g.nextBonusAt) {
        g.nextBonusAt += kBonusInterval;
        Bonus b;
        if (g.bonusCount % 2 == 0) {
            b.x = -kBonusW;
            b.dir = 1.f;
        } else {
            b.x = kScreenW;
            b.dir = -1.f;
        }
        ++g.bonusCount;
        g.bonuses.push_back(b);
    }
    moveBonuses(g);

    // 5. 플레이어 탄 이동과 판정 (스윕 판정으로 건너뛰기 방지)
    if (g.playerBulletActive) {
        const float prevY = g.playerBullet.y;
        g.playerBullet.y -= kPBulletSpeed * kStep;
        const float top = g.playerBullet.y;
        const float bottom = prevY + kPBulletH;
        const float bx = g.playerBullet.x;

        if (top + kPBulletH < 0.f) {
            g.playerBulletActive = false;
        } else {
            bool consumed = false;

            for (auto& p : g.pieces) {
                if (p.alive && overlaps(bx, top, kPBulletW, bottom - top,
                                        p.x, p.y, kPieceSize, kPieceSize)) {
                    p.alive = false;
                    consumed = true;
                    break;
                }
            }

            if (!consumed) {
                for (int i = 0; i < kEnemyCount; ++i) {
                    if (!g.alive[i]) continue;
                    const int r = i / kCols;
                    const int c = i % kCols;
                    const sf::Vector2f pos = enemyPos(g, r, c);
                    if (overlaps(bx, top, kPBulletW, bottom - top,
                                 pos.x, pos.y, kEnemyW, kEnemyH)) {
                        g.alive[i] = false;
                        g.score += enemyScore(r);
                        a.hit.play();
                        consumed = true;
                        break;
                    }
                }
            }

            if (!consumed) {
                for (auto it = g.bonuses.begin(); it != g.bonuses.end(); ++it) {
                    if (overlaps(bx, top, kPBulletW, bottom - top,
                                 it->x, kBonusY, kBonusW, kBonusH)) {
                        std::uniform_int_distribution<int> pick(0, 2);
                        g.score += kBonusPoints[pick(g.rng)];
                        g.bonuses.erase(it);
                        a.bonusSfx.play();
                        consumed = true;
                        break;
                    }
                }
            }

            if (consumed) {
                g.playerBulletActive = false;
            }
        }
    }

    // 6. 적 사격 (1초 뒤부터 1초마다)
    g.shotTimer -= kStep;
    if (g.shotTimer <= 0.f) {
        g.shotTimer += kEnemyShotInterval;
        fireEnemyShot(g);
    }

    // 7. 적 탄 이동과 판정
    bool hitPlayer = false;
    for (auto it = g.enemyBullets.begin(); it != g.enemyBullets.end();) {
        it->pos.x += it->vel.x * kStep;
        it->pos.y += it->vel.y * kStep;

        bool remove = it->pos.x < -10.f || it->pos.x > kScreenW + 10.f ||
                      it->pos.y < -10.f || it->pos.y > kScreenH + 10.f;

        if (!remove) {
            for (auto& p : g.pieces) {
                if (p.alive && circleHitsRect(it->pos, kEBulletR, p.x, p.y, kPieceSize, kPieceSize)) {
                    p.alive = false;
                    remove = true;
                    break;
                }
            }
        }

        if (!remove && circleHitsRect(it->pos, kEBulletR, g.playerX, kPlayerY, kPlayerW, kPlayerH)) {
            hitPlayer = true;
            break;
        }

        if (remove) {
            it = g.enemyBullets.erase(it);
        } else {
            ++it;
        }
    }

    if (hitPlayer) {
        a.lose.play();
        g.lives -= 1;
        g.enemyBullets.clear();
        g.playerBulletActive = false;
        if (g.lives <= 0) {
            enterResult(g, a);
            return;
        }
        g.freezeTimer = kFreezeTime;
        return;
    }

    // 8. 적이 방어벽 조각에 겹치면 조각 제거
    for (int i = 0; i < kEnemyCount; ++i) {
        if (!g.alive[i]) continue;
        const sf::Vector2f pos = enemyPos(g, i / kCols, i % kCols);
        for (auto& p : g.pieces) {
            if (p.alive && overlaps(pos.x, pos.y, kEnemyW, kEnemyH,
                                    p.x, p.y, kPieceSize, kPieceSize)) {
                p.alive = false;
            }
        }
    }

    // 9. 적이 하단 한계에 닿으면 결과 화면
    for (int i = 0; i < kEnemyCount; ++i) {
        if (!g.alive[i]) continue;
        const sf::Vector2f pos = enemyPos(g, i / kCols, i % kCols);
        if (pos.y >= kEnemyBottomLimit) {
            enterResult(g, a);
            return;
        }
    }

    // 10. 웨이브 클리어 -> 다음 웨이브 (점수와 목숨은 유지)
    const bool anyAlive = std::any_of(g.alive.begin(), g.alive.end(), [](bool v) { return v; });
    if (!anyAlive) {
        g.wave += 1;
        spawnWave(g);
    }
}

void step(Game& g, Assets& a, const Input& in) {
    switch (g.screen) {
    case Screen::Title:
        g.titleTimer += kStep;
        if (in.enter || g.titleTimer >= kTitleTime) {
            startPlay(g, a);
        }
        break;
    case Screen::Play:
        updatePlay(g, a, in);
        break;
    case Screen::Result:
        if (in.restart) {
            startPlay(g, a);
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// 렌더링
// ---------------------------------------------------------------------------
void drawText(sf::RenderWindow& w, const sf::Font& font, const std::string& s,
              unsigned int size, sf::Color color, float x, float y, Align align) {
    sf::Text t(font, sf::String(s), size);
    t.setFillColor(color);
    const sf::FloatRect b = t.getLocalBounds();
    float ox = b.position.x;
    if (align == Align::Center) {
        ox += b.size.x / 2.f;
    } else if (align == Align::Right) {
        ox += b.size.x;
    }
    t.setOrigin(sf::Vector2f{ox, b.position.y + b.size.y / 2.f});
    t.setPosition(sf::Vector2f{x, y});
    w.draw(t);
}

void drawPlay(sf::RenderWindow& w, const Game& g, const Assets& a) {
    // 방어벽
    sf::RectangleShape piece(sf::Vector2f{kPieceSize, kPieceSize});
    piece.setFillColor(sf::Color(0x4C, 0xC2, 0x6B));
    for (const auto& p : g.pieces) {
        if (!p.alive) continue;
        piece.setPosition(sf::Vector2f{p.x, p.y});
        w.draw(piece);
    }

    // 적
    for (int i = 0; i < kEnemyCount; ++i) {
        if (!g.alive[i]) continue;
        const int r = i / kCols;
        const int c = i % kCols;
        const int base = (r == 0) ? 0 : (r == 1 ? 2 : 4);
        const int frame = base + g.animPhase;

        sf::Sprite s(a.enemies);
        s.setTextureRect(sf::IntRect({frame * 32, 0}, {32, 24}));
        s.setPosition(enemyPos(g, r, c));
        w.draw(s);
    }

    // 보너스 비행체
    for (const auto& b : g.bonuses) {
        sf::Sprite bs(a.bonus);
        bs.setPosition(sf::Vector2f{b.x, kBonusY});
        w.draw(bs);
    }

    // 플레이어
    sf::Sprite ps(a.player);
    ps.setPosition(sf::Vector2f{g.playerX, kPlayerY});
    w.draw(ps);

    // 플레이어 탄
    if (g.playerBulletActive) {
        sf::RectangleShape pb(sf::Vector2f{kPBulletW, kPBulletH});
        pb.setFillColor(sf::Color::White);
        pb.setPosition(g.playerBullet);
        w.draw(pb);
    }

    // 적 탄
    sf::CircleShape eb(kEBulletR);
    eb.setOrigin(sf::Vector2f{kEBulletR, kEBulletR});
    eb.setFillColor(sf::Color(0xFF, 0x6B, 0x6B));
    for (const auto& b : g.enemyBullets) {
        eb.setPosition(b.pos);
        w.draw(eb);
    }

    // 점수 HUD
    const sf::Color white = sf::Color::White;
    drawText(w, a.font, "SCORE " + std::to_string(g.score), 16, white, 16.f, 16.f, Align::Left);
    drawText(w, a.font, "WAVE " + std::to_string(g.wave), 16, white, 480.f, 16.f, Align::Center);
    drawText(w, a.font, "LIVES " + std::to_string(g.lives), 16, white, 944.f, 16.f, Align::Right);
}

void render(sf::RenderWindow& w, const Game& g, const Assets& a) {
    w.clear(sf::Color::Black);
    const sf::Color white = sf::Color::White;

    switch (g.screen) {
    case Screen::Title:
        drawText(w, a.font, "STAR FORMATION", 48, white, 480.f, 200.f, Align::Center);
        drawText(w, a.font, "PRESS ENTER", 16, white, 480.f, 300.f, Align::Center);
        break;
    case Screen::Play:
        drawPlay(w, g, a);
        break;
    case Screen::Result:
        drawText(w, a.font, "GAME OVER", 48, white, 480.f, 160.f, Align::Center);
        drawText(w, a.font, "SCORE " + std::to_string(g.score), 32, white, 480.f, 250.f, Align::Center);
        drawText(w, a.font, "WAVE " + std::to_string(g.wave), 32, white, 480.f, 310.f, Align::Center);
        drawText(w, a.font, "R: RESTART  ESC: QUIT", 16, white, 480.f, 420.f, Align::Center);
        break;
    }

    w.display();
}

} // namespace

// ---------------------------------------------------------------------------
// 메인
// ---------------------------------------------------------------------------
int main() {
    Assets assets;
    if (!loadAssets(assets)) {
        return 1;
    }

    Game game;

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "STAR FORMATION");
    window.setFramerateLimit(60);

    Input latch; // 이벤트로 기록된 입력(다음 갱신에서 소비)
    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                switch (key->code) {
                case sf::Keyboard::Key::Escape:
                    window.close();
                    break;
                case sf::Keyboard::Key::Left:
                case sf::Keyboard::Key::A:
                    latch.left = true;
                    break;
                case sf::Keyboard::Key::Right:
                case sf::Keyboard::Key::D:
                    latch.right = true;
                    break;
                case sf::Keyboard::Key::Space:
                    latch.fire = true;
                    break;
                case sf::Keyboard::Key::Enter:
                    latch.enter = true;
                    break;
                case sf::Keyboard::Key::R:
                    latch.restart = true;
                    break;
                default:
                    break;
                }
            }
        }

        if (!window.isOpen()) {
            break;
        }

        accumulator += std::min(clock.restart().asSeconds(), 0.25f);

        while (accumulator >= kStep) {
            Input in = latch;
            in.left = latch.left || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)
                                 || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
            in.right = latch.right || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)
                                   || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
            latch = Input{}; // 첫 갱신에서 소비

            step(game, assets, in);
            accumulator -= kStep;
        }

        render(window, game, assets);
    }

    return 0;
}
