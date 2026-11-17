#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float kStep = 1.f / 60.f; // 고정 시뮬레이션 간격(초)
constexpr unsigned kWinW = 960u;
constexpr unsigned kWinH = 540u;
constexpr float kScreenW = 960.f;
constexpr float kScreenH = 540.f;

// 적 대열
constexpr int kCols = 8;
constexpr int kRows = 4;
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr float kColGap = 48.f;
constexpr float kRowGap = 40.f;
constexpr float kFormX0 = 296.f;
constexpr float kFormY0 = 80.f;
constexpr float kFormDrop = 16.f;
constexpr float kFormMinEdge = 16.f;
constexpr float kFormMaxEdge = 944.f;
constexpr float kFormLimitY = 490.f;
constexpr float kFormBaseSpeed = 40.f;
constexpr float kFormSpeedPerKill = 4.f;
constexpr float kEnemyShotPeriod = 1.f;
constexpr std::size_t kMaxEnemyBullets = 3;
constexpr float kEnemyBulletSpeed = 240.f;
constexpr float kEnemyBulletR = 3.f;

// 플레이어
constexpr float kPlayerY = 490.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerSpeed = 300.f;
constexpr float kPlayerMinX = 16.f;
constexpr float kPlayerMaxX = kScreenW - 16.f - kPlayerW;
constexpr float kPBulletW = 4.f;
constexpr float kPBulletH = 12.f;
constexpr float kPBulletSpeed = 600.f;

// 방어벽
constexpr int kBarriers = 4;
constexpr float kBarrierCX[kBarriers] = {192.f, 384.f, 576.f, 768.f};
constexpr float kBarrierY = 400.f;
constexpr int kPieceCols = 6;
constexpr int kPieceRows = 4;
constexpr float kPieceSize = 8.f;

// 보너스 비행체
constexpr float kBonusY = 50.f;
constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusSpeed = 120.f;
constexpr float kBonusPeriod = 20.f;
constexpr int kBonusPoints[3] = {50, 100, 150};

constexpr int kStartLives = 3;
constexpr float kFreezeTime = 1.f;
constexpr float kTitleTime = 2.f;
constexpr float kAnimPeriod = 0.5f;

const sf::Color kEnemyBulletColor(0xFF, 0x6B, 0x6B);
const sf::Color kBarrierColor(0x4C, 0xC2, 0x6B);

struct Box {
    float x;
    float y;
    float w;
    float h;
};

bool overlap(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool circleHitsBox(sf::Vector2f c, float r, const Box& b) {
    const float cx = std::clamp(c.x, b.x, b.x + b.w);
    const float cy = std::clamp(c.y, b.y, b.y + b.h);
    const float dx = c.x - cx;
    const float dy = c.y - cy;
    return dx * dx + dy * dy <= r * r;
}

Box pieceBox(int barrier, int row, int col) {
    return Box{kBarrierCX[barrier] - 24.f + static_cast<float>(col) * kPieceSize,
               kBarrierY + static_cast<float>(row) * kPieceSize,
               kPieceSize, kPieceSize};
}

// 입력 플래그를 읽고 비운다.
bool take(bool& flag) {
    const bool value = flag;
    flag = false;
    return value;
}

int rowScore(int row) {
    if (row == 0) return 30;
    if (row == 1) return 20;
    return 10;
}

int rowFrameGroup(int row) {
    if (row == 0) return 0;
    if (row == 1) return 1;
    return 2;
}

sf::String toStr(const std::string& s) {
    return sf::String(std::string_view(s));
}

[[noreturn]] void failLoad(const char* path) {
    std::cerr << "failed to load asset: " << path << '\n';
    throw std::runtime_error("asset load failed");
}

sf::Font& loadFont(sf::Font& font, const char* path) {
    if (!font.openFromFile(path)) failLoad(path);
    return font;
}

sf::Texture& loadTexture(sf::Texture& tex, const char* path) {
    if (!tex.loadFromFile(path)) failLoad(path);
    return tex;
}

sf::SoundBuffer& loadBuffer(sf::SoundBuffer& buf, const char* path) {
    if (!buf.loadFromFile(path)) failLoad(path);
    return buf;
}

enum class Screen { Title, Playing, Result };
enum class Anchor { Left, Center, Right };

class Game {
public:
    Game();
    int run();

private:
    struct EnemyBullet {
        sf::Vector2f pos;
        sf::Vector2f vel;
    };

    // 멤버 선언 순서가 생성 순서다. 버퍼는 사운드보다 먼저 선언한다.
    sf::RenderWindow window;
    sf::Font font;
    sf::Texture enemyTex;
    sf::Texture playerTex;
    sf::Texture bonusTex;
    sf::SoundBuffer shootBuf;
    sf::SoundBuffer hitBuf;
    sf::SoundBuffer loseBuf;
    sf::SoundBuffer bonusBuf;
    sf::Sound shootSnd;
    sf::Sound hitSnd;
    sf::Sound loseSnd;
    sf::Sound bonusSnd;
    sf::Music bgm;
    std::mt19937 rng{std::random_device{}()};

    Screen screen = Screen::Title;
    bool pendEnter = false;
    bool pendR = false;
    bool pendSpace = false;
    bool pendLeft = false;
    bool pendRight = false;

    float titleTime = 0.f;
    float freezeLeft = 0.f;
    float animTime = 0.f;
    float playTime = 0.f;
    float waveTime = 0.f;
    float nextShotAt = kEnemyShotPeriod;
    float nextBonusAt = kBonusPeriod;
    int bonusCount = 0;

    int score = 0;
    int wave = 1;
    int lives = kStartLives;

    std::array<std::array<bool, kRows>, kCols> alive{};
    float formX = kFormX0;
    float formY = kFormY0;
    int formDir = 1;

    float playerX = 0.f;
    bool pbActive = false;
    sf::Vector2f pbPos{};

    std::vector<EnemyBullet> ebullets;

    bool bonusActive = false;
    float bonusX = 0.f;
    int bonusDir = 1;

    std::array<std::array<std::array<bool, kPieceCols>, kPieceRows>, kBarriers> pieces{};

    // ---- 상태 흐름 ----
    void resetGame() {
        score = 0;
        wave = 1;
        lives = kStartLives;
        playTime = 0.f;
        freezeLeft = 0.f;
        animTime = 0.f;
        nextBonusAt = kBonusPeriod;
        bonusCount = 0;
        playerX = (kScreenW - kPlayerW) / 2.f;
        startWave();
    }

    void startWave() {
        for (auto& col : alive) col.fill(true);
        for (auto& barrier : pieces)
            for (auto& row : barrier) row.fill(true);
        formX = kFormX0;
        formY = kFormY0 + std::min(kFormDrop * static_cast<float>(wave - 1), 64.f);
        formDir = 1;
        waveTime = 0.f;
        nextShotAt = kEnemyShotPeriod;
        ebullets.clear();
        pbActive = false;
        bonusActive = false;
    }

    void nextWave() {
        ++wave;
        startWave();
    }

    void startGame() {
        resetGame();
        screen = Screen::Playing;
        bgm.play();
    }

    void enterResult() {
        screen = Screen::Result;
        bgm.stop();
        freezeLeft = 0.f;
        pbActive = false;
        ebullets.clear();
    }

    void onPlayerHit() {
        --lives;
        loseSnd.play();
        ebullets.clear();
        pbActive = false;
        if (lives <= 0) {
            enterResult();
        } else {
            freezeLeft = kFreezeTime;
        }
    }

    // ---- 입력 ----
    void pollEvents() {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                continue;
            }
            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (!key) continue;
            switch (key->scancode) {
            case sf::Keyboard::Scancode::Escape: window.close(); break;
            case sf::Keyboard::Scancode::Enter: pendEnter = true; break;
            case sf::Keyboard::Scancode::Space: pendSpace = true; break;
            case sf::Keyboard::Scancode::R: pendR = true; break;
            case sf::Keyboard::Scancode::Left:
            case sf::Keyboard::Scancode::A: pendLeft = true; break;
            case sf::Keyboard::Scancode::Right:
            case sf::Keyboard::Scancode::D: pendRight = true; break;
            default: break;
            }
        }
    }

    // ---- 갱신 ----
    void update() {
        switch (screen) {
        case Screen::Title: updateTitle(); break;
        case Screen::Playing: updatePlaying(); break;
        case Screen::Result: updateResult(); break;
        }
    }

    void updateTitle() {
        titleTime += kStep;
        const bool enter = take(pendEnter);
        pendR = false;
        pendSpace = false;
        pendLeft = false;
        pendRight = false;
        if (enter || titleTime >= kTitleTime) startGame();
    }

    void updateResult() {
        const bool again = take(pendR);
        pendEnter = false;
        pendSpace = false;
        pendLeft = false;
        pendRight = false;
        if (again) startGame();
    }

    void updatePlaying() {
        pendEnter = false;
        pendR = false;
        if (freezeLeft > 0.f) {
            // 멈춘 동안에도 발사·이동 입력은 남겨 둔다.
            freezeLeft -= kStep;
            return;
        }

        playTime += kStep;
        animTime += kStep;
        waveTime += kStep;

        if (!moveFormation()) {
            enterResult();
            return;
        }

        if (waveTime >= nextShotAt) {
            nextShotAt += kEnemyShotPeriod;
            enemyFire();
        }

        updatePlayer();
        updateBonus();
        updatePlayerBullet();

        if (updateEnemyBullets()) {
            onPlayerHit();
            return;
        }

        removePiecesUnderEnemies();
        if (aliveCount() == 0) nextWave();
    }

    int aliveCount() const {
        int n = 0;
        for (const auto& col : alive)
            for (bool a : col)
                if (a) ++n;
        return n;
    }

    int lowestRow(int col) const {
        for (int r = kRows - 1; r >= 0; --r)
            if (alive[col][r]) return r;
        return -1;
    }

    Box enemyBox(int c, int r) const {
        return Box{formX + static_cast<float>(c) * kColGap,
                   formY + static_cast<float>(r) * kRowGap,
                   kEnemyW, kEnemyH};
    }

    Box playerBox() const {
        return Box{playerX, kPlayerY, kPlayerW, kPlayerH};
    }

    // 대열 이동. 적이 바닥선에 닿으면 false.
    bool moveFormation() {
        const int n = aliveCount();
        if (n == 0) return true;

        const float speed = kFormBaseSpeed
            + kFormSpeedPerKill * static_cast<float>(kCols * kRows - n);
        formX += static_cast<float>(formDir) * speed * kStep;

        float minLeft = std::numeric_limits<float>::max();
        float maxRight = std::numeric_limits<float>::lowest();
        for (int c = 0; c < kCols; ++c)
            for (int r = 0; r < kRows; ++r) {
                if (!alive[c][r]) continue;
                const float x = formX + static_cast<float>(c) * kColGap;
                minLeft = std::min(minLeft, x);
                maxRight = std::max(maxRight, x + kEnemyW);
            }

        if (formDir > 0 && maxRight >= kFormMaxEdge) {
            formDir = -1;
            formY += kFormDrop;
        } else if (formDir < 0 && minLeft <= kFormMinEdge) {
            formDir = 1;
            formY += kFormDrop;
        }

        for (int c = 0; c < kCols; ++c)
            for (int r = 0; r < kRows; ++r)
                if (alive[c][r] && formY + static_cast<float>(r) * kRowGap >= kFormLimitY)
                    return false;
        return true;
    }

    // 플레이어 중심과 x 거리가 가장 가까운 열의 맨 아래 적이 쏜다.
    void enemyFire() {
        if (ebullets.size() >= kMaxEnemyBullets) return;

        const float px = playerX + kPlayerW / 2.f;
        int best = -1;
        float bestDist = std::numeric_limits<float>::max();
        for (int c = 0; c < kCols; ++c) {
            if (lowestRow(c) < 0) continue;
            const float cx = formX + static_cast<float>(c) * kColGap + kEnemyW / 2.f;
            const float d = std::fabs(cx - px);
            if (d < bestDist) {
                bestDist = d;
                best = c;
            }
        }
        if (best < 0) return;

        const int row = lowestRow(best);
        const sf::Vector2f origin{
            formX + static_cast<float>(best) * kColGap + kEnemyW / 2.f,
            formY + static_cast<float>(row) * kRowGap + kEnemyH};
        const sf::Vector2f target{px, kPlayerY + kPlayerH / 2.f};
        const sf::Vector2f d = target - origin;
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len <= 0.f) return;

        ebullets.push_back(EnemyBullet{origin, d / len * kEnemyBulletSpeed});
    }

    void updatePlayer() {
        const bool tapL = take(pendLeft);
        const bool tapR = take(pendRight);
        const bool left = tapL
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Left)
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::A);
        const bool right = tapR
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::Right)
            || sf::Keyboard::isKeyPressed(sf::Keyboard::Scancode::D);
        const float dir = (right ? 1.f : 0.f) - (left ? 1.f : 0.f);
        playerX = std::clamp(playerX + dir * kPlayerSpeed * kStep, kPlayerMinX, kPlayerMaxX);

        if (pendSpace && !pbActive) {
            pendSpace = false;
            pbActive = true;
            pbPos = sf::Vector2f{playerX + kPlayerW / 2.f - kPBulletW / 2.f,
                                 kPlayerY - kPBulletH};
            shootSnd.play();
        }
    }

    void updateBonus() {
        if (playTime >= nextBonusAt) {
            nextBonusAt += kBonusPeriod;
            const bool fromLeft = (bonusCount % 2) == 0;
            ++bonusCount;
            bonusActive = true;
            bonusX = fromLeft ? -kBonusW : kScreenW;
            bonusDir = fromLeft ? 1 : -1;
        }
        if (bonusActive) {
            bonusX += static_cast<float>(bonusDir) * kBonusSpeed * kStep;
            if ((bonusDir > 0 && bonusX >= kScreenW)
                || (bonusDir < 0 && bonusX + kBonusW <= 0.f)) {
                bonusActive = false;
            }
        }
    }

    void updatePlayerBullet() {
        if (!pbActive) return;

        pbPos.y -= kPBulletSpeed * kStep;
        if (pbPos.y + kPBulletH < 0.f) {
            pbActive = false;
            return;
        }
        const Box shot{pbPos.x, pbPos.y, kPBulletW, kPBulletH};

        for (int b = 0; b < kBarriers; ++b)
            for (int row = 0; row < kPieceRows; ++row)
                for (int col = 0; col < kPieceCols; ++col) {
                    if (pieces[b][row][col] && overlap(shot, pieceBox(b, row, col))) {
                        pieces[b][row][col] = false;
                        pbActive = false;
                        return;
                    }
                }

        for (int c = 0; c < kCols; ++c)
            for (int r = 0; r < kRows; ++r) {
                if (alive[c][r] && overlap(shot, enemyBox(c, r))) {
                    alive[c][r] = false;
                    score += rowScore(r);
                    pbActive = false;
                    hitSnd.play();
                    return;
                }
            }

        if (bonusActive && overlap(shot, Box{bonusX, kBonusY, kBonusW, kBonusH})) {
            bonusActive = false;
            pbActive = false;
            score += kBonusPoints[std::uniform_int_distribution<int>(0, 2)(rng)];
            bonusSnd.play();
        }
    }

    bool removePieceAtCircle(sf::Vector2f c) {
        for (int b = 0; b < kBarriers; ++b)
            for (int row = 0; row < kPieceRows; ++row)
                for (int col = 0; col < kPieceCols; ++col) {
                    if (pieces[b][row][col]
                        && circleHitsBox(c, kEnemyBulletR, pieceBox(b, row, col))) {
                        pieces[b][row][col] = false;
                        return true;
                    }
                }
        return false;
    }

    // 적 탄 이동과 충돌. 플레이어가 맞았으면 true.
    bool updateEnemyBullets() {
        bool hit = false;
        for (std::size_t i = 0; i < ebullets.size();) {
            EnemyBullet& eb = ebullets[i];
            eb.pos += eb.vel * kStep;

            bool remove = false;
            if (eb.pos.x < -20.f || eb.pos.x > kScreenW + 20.f
                || eb.pos.y < -20.f || eb.pos.y > kScreenH + 20.f) {
                remove = true;
            } else if (removePieceAtCircle(eb.pos)) {
                remove = true;
            } else if (circleHitsBox(eb.pos, kEnemyBulletR, playerBox())) {
                hit = true;
                remove = true;
            }

            if (remove) {
                ebullets[i] = ebullets.back();
                ebullets.pop_back();
            } else {
                ++i;
            }
            if (hit) break;
        }
        return hit;
    }

    void removePiecesUnderEnemies() {
        for (int c = 0; c < kCols; ++c)
            for (int r = 0; r < kRows; ++r) {
                if (!alive[c][r]) continue;
                const Box e = enemyBox(c, r);
                for (int b = 0; b < kBarriers; ++b)
                    for (int row = 0; row < kPieceRows; ++row)
                        for (int col = 0; col < kPieceCols; ++col) {
                            if (pieces[b][row][col] && overlap(e, pieceBox(b, row, col)))
                                pieces[b][row][col] = false;
                        }
            }
    }

    // ---- 그리기 ----
    void drawText(const std::string& s, unsigned size, float x, float y, Anchor anchor) {
        sf::Text text(font, toStr(s), size);
        text.setFillColor(sf::Color::White);
        const sf::FloatRect b = text.getLocalBounds();
        float ox = b.position.x;
        if (anchor == Anchor::Center) ox += b.size.x / 2.f;
        else if (anchor == Anchor::Right) ox += b.size.x;
        text.setOrigin({ox, b.position.y});
        text.setPosition({x, y});
        window.draw(text);
    }

    void drawBarriers() {
        sf::RectangleShape cell(sf::Vector2f{kPieceSize, kPieceSize});
        cell.setFillColor(kBarrierColor);
        for (int b = 0; b < kBarriers; ++b)
            for (int row = 0; row < kPieceRows; ++row)
                for (int col = 0; col < kPieceCols; ++col) {
                    if (!pieces[b][row][col]) continue;
                    const Box box = pieceBox(b, row, col);
                    cell.setPosition({box.x, box.y});
                    window.draw(cell);
                }
    }

    void drawEnemies() {
        sf::Sprite spr(enemyTex);
        const int phase = static_cast<int>(animTime / kAnimPeriod) % 2;
        for (int c = 0; c < kCols; ++c)
            for (int r = 0; r < kRows; ++r) {
                if (!alive[c][r]) continue;
                const int frame = rowFrameGroup(r) * 2 + phase;
                spr.setTextureRect(sf::IntRect({frame * 32, 0}, {32, 24}));
                spr.setPosition({formX + static_cast<float>(c) * kColGap,
                                 formY + static_cast<float>(r) * kRowGap});
                window.draw(spr);
            }
    }

    void drawBonus() {
        if (!bonusActive) return;
        sf::Sprite spr(bonusTex);
        spr.setPosition({bonusX, kBonusY});
        window.draw(spr);
    }

    void drawPlayer() {
        sf::Sprite spr(playerTex);
        spr.setPosition({playerX, kPlayerY});
        window.draw(spr);
    }

    void drawBullets() {
        if (pbActive) {
            sf::RectangleShape shot(sf::Vector2f{kPBulletW, kPBulletH});
            shot.setFillColor(sf::Color::White);
            shot.setPosition(pbPos);
            window.draw(shot);
        }
        if (!ebullets.empty()) {
            sf::CircleShape c(kEnemyBulletR);
            c.setOrigin({kEnemyBulletR, kEnemyBulletR});
            c.setFillColor(kEnemyBulletColor);
            for (const EnemyBullet& eb : ebullets) {
                c.setPosition(eb.pos);
                window.draw(c);
            }
        }
    }

    void drawHud() {
        drawText("SCORE " + std::to_string(score), 16, 16.f, 12.f, Anchor::Left);
        drawText("WAVE " + std::to_string(wave), 16, kScreenW / 2.f, 12.f, Anchor::Center);
        drawText("LIVES " + std::to_string(lives), 16, kScreenW - 16.f, 12.f, Anchor::Right);
    }

    void render() {
        window.clear(sf::Color::Black);
        switch (screen) {
        case Screen::Title:
            drawText("STAR FORMATION", 48, kScreenW / 2.f, 200.f, Anchor::Center);
            drawText("PRESS ENTER", 32, kScreenW / 2.f, 300.f, Anchor::Center);
            break;
        case Screen::Playing:
            drawBarriers();
            drawEnemies();
            drawBonus();
            drawPlayer();
            drawBullets();
            drawHud();
            break;
        case Screen::Result:
            drawText("GAME OVER", 48, kScreenW / 2.f, 180.f, Anchor::Center);
            drawText("SCORE " + std::to_string(score), 32, kScreenW / 2.f, 260.f, Anchor::Center);
            drawText("WAVE " + std::to_string(wave), 32, kScreenW / 2.f, 320.f, Anchor::Center);
            drawText("R: RESTART  ESC: QUIT", 16, kScreenW / 2.f, 420.f, Anchor::Center);
            break;
        }
    }
};

Game::Game()
    : window(sf::VideoMode({kWinW, kWinH}), "STAR FORMATION"),
      shootSnd(loadBuffer(shootBuf, "assets/sfx/shoot.wav")),
      hitSnd(loadBuffer(hitBuf, "assets/sfx/hit.wav")),
      loseSnd(loadBuffer(loseBuf, "assets/sfx/lose.wav")),
      bonusSnd(loadBuffer(bonusBuf, "assets/sfx/bonus.wav")) {
    loadFont(font, "assets/fonts/game.ttf");
    loadTexture(enemyTex, "assets/textures/fm_enemies.png");
    loadTexture(playerTex, "assets/textures/fm_player.png");
    loadTexture(bonusTex, "assets/textures/fm_bonus.png");
    if (!bgm.openFromFile("assets/music/bgm_formation.ogg")) {
        failLoad("assets/music/bgm_formation.ogg");
    }
    bgm.setLooping(true);
    bgm.setVolume(40.f);

    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);
    resetGame();
}

int Game::run() {
    sf::Clock clock;
    float accumulator = 0.f;
    while (window.isOpen()) {
        pollEvents();
        accumulator += std::min(clock.restart().asSeconds(), 0.25f);
        while (accumulator >= kStep && window.isOpen()) {
            update();
            accumulator -= kStep;
        }
        if (!window.isOpen()) break;
        render();
        window.display();
    }
    return 0;
}

} // namespace

int main() {
    try {
        Game game;
        return game.run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
