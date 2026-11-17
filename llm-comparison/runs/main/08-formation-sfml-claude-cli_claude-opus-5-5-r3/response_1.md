```cpp
// STAR FORMATION - SFML 3.1 구현 (main.cpp 단일 파일)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr unsigned WIN_W = 960;
constexpr unsigned WIN_H = 540;
constexpr float DT = 1.f / 60.f;

constexpr int COLS = 8;
constexpr int ROWS = 4;
constexpr float ENEMY_W = 32.f;
constexpr float ENEMY_H = 24.f;

constexpr float PLAYER_W = 40.f;
constexpr float PLAYER_H = 24.f;
constexpr float PLAYER_Y = 490.f;
constexpr float PLAYER_SPEED = 300.f;

constexpr float PBULLET_W = 4.f;
constexpr float PBULLET_H = 12.f;
constexpr float PBULLET_SPEED = 600.f;

constexpr float EBULLET_R = 3.f;
constexpr float EBULLET_SPEED = 240.f;
constexpr std::size_t EBULLET_MAX = 3;

constexpr int SH_COLS = 6;
constexpr int SH_ROWS = 4;
constexpr float SH_PIECE = 8.f;
constexpr float SH_TOP = 400.f;
constexpr std::array<float, 4> SH_CENTERS = {192.f, 384.f, 576.f, 768.f};

constexpr float BONUS_W = 48.f;
constexpr float BONUS_H = 20.f;
constexpr float BONUS_Y = 50.f;
constexpr float BONUS_SPEED = 120.f;
constexpr float BONUS_INTERVAL = 20.f;

enum class Screen
{
    Title,
    Play,
    Result
};

struct EnemyBullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
};

struct Bonus
{
    float x;
    float dir;
};

bool rectsOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

bool circleRect(sf::Vector2f c, float r, float rx, float ry, float rw, float rh)
{
    const float cx = std::clamp(c.x, rx, rx + rw);
    const float cy = std::clamp(c.y, ry, ry + rh);
    const float dx = c.x - cx;
    const float dy = c.y - cy;
    return dx * dx + dy * dy <= r * r;
}
} // namespace

int main()
{
    // ---------- 에셋 로드 ----------
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
    {
        std::cerr << "assets/fonts/game.ttf\n";
        return 1;
    }
    sf::Texture texEnemies, texPlayer, texBonus;
    if (!texEnemies.loadFromFile("assets/textures/fm_enemies.png"))
    {
        std::cerr << "assets/textures/fm_enemies.png\n";
        return 1;
    }
    if (!texPlayer.loadFromFile("assets/textures/fm_player.png"))
    {
        std::cerr << "assets/textures/fm_player.png\n";
        return 1;
    }
    if (!texBonus.loadFromFile("assets/textures/fm_bonus.png"))
    {
        std::cerr << "assets/textures/fm_bonus.png\n";
        return 1;
    }
    sf::SoundBuffer bufShoot, bufHit, bufLose, bufBonus;
    if (!bufShoot.loadFromFile("assets/sfx/shoot.wav"))
    {
        std::cerr << "assets/sfx/shoot.wav\n";
        return 1;
    }
    if (!bufHit.loadFromFile("assets/sfx/hit.wav"))
    {
        std::cerr << "assets/sfx/hit.wav\n";
        return 1;
    }
    if (!bufLose.loadFromFile("assets/sfx/lose.wav"))
    {
        std::cerr << "assets/sfx/lose.wav\n";
        return 1;
    }
    if (!bufBonus.loadFromFile("assets/sfx/bonus.wav"))
    {
        std::cerr << "assets/sfx/bonus.wav\n";
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_formation.ogg"))
    {
        std::cerr << "assets/music/bgm_formation.ogg\n";
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    sf::Sound sndShoot(bufShoot);
    sf::Sound sndHit(bufHit);
    sf::Sound sndLose(bufLose);
    sf::Sound sndBonus(bufBonus);

    sf::RenderWindow window(sf::VideoMode({WIN_W, WIN_H}), "STAR FORMATION", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    std::mt19937 rng(std::random_device{}());

    // ---------- 게임 상태 ----------
    Screen screen = Screen::Title;
    float titleTimer = 0.f;

    std::array<bool, COLS * ROWS> enemyAlive{};
    float formX = 0.f, formY = 0.f, waveOffset = 0.f, formDir = 1.f;
    float animTimer = 0.f;
    float fireTimer = 1.f;

    std::array<std::array<bool, SH_COLS * SH_ROWS>, 4> shields{};

    float playerX = (WIN_W - PLAYER_W) / 2.f;
    std::optional<sf::Vector2f> playerBullet;
    std::vector<EnemyBullet> enemyBullets;
    std::optional<Bonus> bonus;
    float bonusTimer = BONUS_INTERVAL;
    bool bonusFromLeft = true;

    int score = 0;
    int lives = 3;
    int wave = 1;
    float freezeTimer = 0.f;

    // 입력 상태
    bool leftHeld = false, rightHeld = false;
    bool spaceLatch = false, enterLatch = false, rLatch = false;

    auto enemyPos = [&](int c, int r) {
        return sf::Vector2f(296.f + 48.f * c + formX, 80.f + 40.f * r + formY + waveOffset);
    };
    auto aliveCount = [&]() {
        return static_cast<int>(std::count(enemyAlive.begin(), enemyAlive.end(), true));
    };
    auto shieldLeft = [&](int s) { return SH_CENTERS[static_cast<std::size_t>(s)] - SH_COLS * SH_PIECE / 2.f; };

    auto startWave = [&]() {
        enemyAlive.fill(true);
        formX = 0.f;
        formY = 0.f;
        formDir = 1.f;
        waveOffset = std::min(16.f * static_cast<float>(wave - 1), 64.f);
        for (auto& s : shields)
            s.fill(true);
        playerBullet.reset();
        enemyBullets.clear();
        bonus.reset();
        fireTimer = 1.f;
    };

    auto startPlay = [&]() {
        score = 0;
        lives = 3;
        wave = 1;
        playerX = (WIN_W - PLAYER_W) / 2.f;
        animTimer = 0.f;
        bonusTimer = BONUS_INTERVAL;
        bonusFromLeft = true;
        freezeTimer = 0.f;
        spaceLatch = false;
        startWave();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto gotoResult = [&]() {
        screen = Screen::Result;
        music.stop();
        rLatch = false;
        playerBullet.reset();
        enemyBullets.clear();
        bonus.reset();
    };

    // ---------- 플레이 1틱 ----------
    auto updatePlay = [&]() {
        const bool fire = spaceLatch;
        spaceLatch = false;

        if (freezeTimer > 0.f)
        {
            freezeTimer -= DT;
            return;
        }

        animTimer += DT;

        // 플레이어 이동
        float mv = 0.f;
        if (leftHeld)
            mv -= 1.f;
        if (rightHeld)
            mv += 1.f;
        playerX = std::clamp(playerX + mv * PLAYER_SPEED * DT, 16.f, static_cast<float>(WIN_W) - 16.f - PLAYER_W);

        // 사격
        if (fire && !playerBullet)
        {
            playerBullet = sf::Vector2f(playerX + PLAYER_W / 2.f - PBULLET_W / 2.f, PLAYER_Y - PBULLET_H);
            sndShoot.play();
        }

        // 플레이어 총알 (쓸고 지나간 영역으로 판정)
        if (playerBullet)
        {
            const float oldY = playerBullet->y;
            const float newY = oldY - PBULLET_SPEED * DT;
            const float bx = playerBullet->x;
            const float sweepTop = newY;
            const float sweepH = (oldY + PBULLET_H) - newY;

            // 가장 먼저 닿는(아래쪽이 가장 큰) 대상 선택
            int kind = 0; // 1 shield, 2 enemy, 3 bonus
            float bestBottom = -1e9f;
            int bestA = -1, bestB = -1;

            for (int s = 0; s < 4; ++s)
                for (int j = 0; j < SH_ROWS; ++j)
                    for (int i = 0; i < SH_COLS; ++i)
                    {
                        if (!shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(j * SH_COLS + i)])
                            continue;
                        const float px = shieldLeft(s) + SH_PIECE * i;
                        const float py = SH_TOP + SH_PIECE * j;
                        if (rectsOverlap(bx, sweepTop, PBULLET_W, sweepH, px, py, SH_PIECE, SH_PIECE) &&
                            py + SH_PIECE > bestBottom)
                        {
                            bestBottom = py + SH_PIECE;
                            kind = 1;
                            bestA = s;
                            bestB = j * SH_COLS + i;
                        }
                    }
            for (int r = 0; r < ROWS; ++r)
                for (int c = 0; c < COLS; ++c)
                {
                    if (!enemyAlive[static_cast<std::size_t>(r * COLS + c)])
                        continue;
                    const sf::Vector2f p = enemyPos(c, r);
                    if (rectsOverlap(bx, sweepTop, PBULLET_W, sweepH, p.x, p.y, ENEMY_W, ENEMY_H) &&
                        p.y + ENEMY_H > bestBottom)
                    {
                        bestBottom = p.y + ENEMY_H;
                        kind = 2;
                        bestA = r;
                        bestB = c;
                    }
                }
            if (bonus && rectsOverlap(bx, sweepTop, PBULLET_W, sweepH, bonus->x, BONUS_Y, BONUS_W, BONUS_H) &&
                BONUS_Y + BONUS_H > bestBottom)
            {
                bestBottom = BONUS_Y + BONUS_H;
                kind = 3;
            }

            if (kind == 1)
            {
                shields[static_cast<std::size_t>(bestA)][static_cast<std::size_t>(bestB)] = false;
                playerBullet.reset();
            }
            else if (kind == 2)
            {
                enemyAlive[static_cast<std::size_t>(bestA * COLS + bestB)] = false;
                score += (bestA == 0) ? 30 : (bestA == 1 ? 20 : 10);
                sndHit.play();
                playerBullet.reset();
            }
            else if (kind == 3)
            {
                std::uniform_int_distribution<int> d(1, 3);
                score += 50 * d(rng);
                sndBonus.play();
                bonus.reset();
                playerBullet.reset();
            }
            else
            {
                playerBullet->y = newY;
                if (newY + PBULLET_H < 0.f)
                    playerBullet.reset();
            }
        }

        // 웨이브 클리어
        if (aliveCount() == 0)
        {
            ++wave;
            startWave();
            return;
        }

        // 대열 이동
        {
            const float speed = 40.f + 4.f * static_cast<float>(32 - aliveCount());
            formX += formDir * speed * DT;
            float minX = 1e9f, maxX = -1e9f;
            for (int r = 0; r < ROWS; ++r)
                for (int c = 0; c < COLS; ++c)
                    if (enemyAlive[static_cast<std::size_t>(r * COLS + c)])
                    {
                        const sf::Vector2f p = enemyPos(c, r);
                        minX = std::min(minX, p.x);
                        maxX = std::max(maxX, p.x + ENEMY_W);
                    }
            if (formDir > 0.f && maxX >= 944.f)
            {
                formX -= (maxX - 944.f);
                formY += 16.f;
                formDir = -1.f;
            }
            else if (formDir < 0.f && minX <= 16.f)
            {
                formX += (16.f - minX);
                formY += 16.f;
                formDir = 1.f;
            }
        }

        // 적과 방어벽 겹침, 적 도달 판정
        bool reached = false;
        for (int r = 0; r < ROWS; ++r)
            for (int c = 0; c < COLS; ++c)
            {
                if (!enemyAlive[static_cast<std::size_t>(r * COLS + c)])
                    continue;
                const sf::Vector2f p = enemyPos(c, r);
                if (p.y + ENEMY_H >= PLAYER_Y)
                    reached = true;
                for (int s = 0; s < 4; ++s)
                    for (int j = 0; j < SH_ROWS; ++j)
                        for (int i = 0; i < SH_COLS; ++i)
                        {
                            auto& piece = shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(j * SH_COLS + i)];
                            if (piece && rectsOverlap(p.x, p.y, ENEMY_W, ENEMY_H, shieldLeft(s) + SH_PIECE * i,
                                                      SH_TOP + SH_PIECE * j, SH_PIECE, SH_PIECE))
                                piece = false;
                        }
            }
        if (reached)
        {
            gotoResult();
            return;
        }

        const sf::Vector2f playerCenter(playerX + PLAYER_W / 2.f, PLAYER_Y + PLAYER_H / 2.f);

        // 적 사격
        fireTimer -= DT;
        if (fireTimer <= 0.f)
        {
            fireTimer += 1.f;
            if (enemyBullets.size() < EBULLET_MAX)
            {
                int bestCol = -1;
                float bestDist = 1e9f;
                for (int c = 0; c < COLS; ++c)
                {
                    bool any = false;
                    for (int r = 0; r < ROWS; ++r)
                        any = any || enemyAlive[static_cast<std::size_t>(r * COLS + c)];
                    if (!any)
                        continue;
                    const float d = std::abs(enemyPos(c, 0).x + ENEMY_W / 2.f - playerCenter.x);
                    if (d < bestDist)
                    {
                        bestDist = d;
                        bestCol = c;
                    }
                }
                if (bestCol >= 0)
                {
                    int bottom = -1;
                    for (int r = ROWS - 1; r >= 0; --r)
                        if (enemyAlive[static_cast<std::size_t>(r * COLS + bestCol)])
                        {
                            bottom = r;
                            break;
                        }
                    const sf::Vector2f p = enemyPos(bestCol, bottom);
                    const sf::Vector2f origin(p.x + ENEMY_W / 2.f, p.y + ENEMY_H);
                    sf::Vector2f dir = playerCenter - origin;
                    if (dir.lengthSquared() < 1e-6f)
                        dir = {0.f, 1.f};
                    enemyBullets.push_back({origin, dir.normalized() * EBULLET_SPEED});
                }
            }
        }

        // 적 탄 이동과 판정
        bool playerHit = false;
        for (auto& b : enemyBullets)
        {
            b.pos += b.vel * DT;
        }
        enemyBullets.erase(std::remove_if(enemyBullets.begin(), enemyBullets.end(),
                                          [&](const EnemyBullet& b) {
                                              if (b.pos.x < -EBULLET_R || b.pos.x > WIN_W + EBULLET_R ||
                                                  b.pos.y < -EBULLET_R || b.pos.y > WIN_H + EBULLET_R)
                                                  return true;
                                              for (int s = 0; s < 4; ++s)
                                                  for (int j = 0; j < SH_ROWS; ++j)
                                                      for (int i = 0; i < SH_COLS; ++i)
                                                      {
                                                          auto& piece = shields[static_cast<std::size_t>(s)]
                                                                               [static_cast<std::size_t>(j * SH_COLS + i)];
                                                          if (piece &&
                                                              circleRect(b.pos, EBULLET_R, shieldLeft(s) + SH_PIECE * i,
                                                                         SH_TOP + SH_PIECE * j, SH_PIECE, SH_PIECE))
                                                          {
                                                              piece = false;
                                                              return true;
                                                          }
                                                      }
                                              if (circleRect(b.pos, EBULLET_R, playerX, PLAYER_Y, PLAYER_W, PLAYER_H))
                                              {
                                                  playerHit = true;
                                                  return true;
                                              }
                                              return false;
                                          }),
                           enemyBullets.end());

        if (playerHit)
        {
            --lives;
            sndLose.play();
            enemyBullets.clear();
            playerBullet.reset();
            if (lives <= 0)
            {
                lives = 0;
                gotoResult();
                return;
            }
            freezeTimer = 1.f;
        }

        // 보너스 비행체
        bonusTimer -= DT;
        if (bonusTimer <= 0.f)
        {
            bonusTimer += BONUS_INTERVAL;
            if (!bonus)
            {
                if (bonusFromLeft)
                    bonus = Bonus{-BONUS_W, 1.f};
                else
                    bonus = Bonus{static_cast<float>(WIN_W), -1.f};
                bonusFromLeft = !bonusFromLeft;
            }
        }
        if (bonus)
        {
            bonus->x += bonus->dir * BONUS_SPEED * DT;
            if ((bonus->dir > 0.f && bonus->x >= static_cast<float>(WIN_W)) ||
                (bonus->dir < 0.f && bonus->x + BONUS_W <= 0.f))
                bonus.reset();
        }
    };

    // ---------- 그리기 도우미 ----------
    auto drawCentered = [&](const std::string& s, unsigned size, float cy) {
        sf::Text t(font, s, size);
        t.setFillColor(sf::Color::White);
        const sf::FloatRect b = t.getLocalBounds();
        t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        t.setPosition({WIN_W / 2.f, cy});
        window.draw(t);
    };

    sf::RectangleShape piece({SH_PIECE, SH_PIECE});
    piece.setFillColor(sf::Color(0x4C, 0xC2, 0x6B));
    sf::RectangleShape pbShape({PBULLET_W, PBULLET_H});
    pbShape.setFillColor(sf::Color::White);
    sf::CircleShape ebShape(EBULLET_R);
    ebShape.setOrigin({EBULLET_R, EBULLET_R});
    ebShape.setFillColor(sf::Color(0xFF, 0x6B, 0x6B));
    sf::Sprite enemySprite(texEnemies);
    sf::Sprite playerSprite(texPlayer);
    sf::Sprite bonusSprite(texBonus);

    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen())
    {
        // ---------- 이벤트 ----------
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                leftHeld = rightHeld = false;
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                switch (kp->code)
                {
                    case sf::Keyboard::Key::Escape: window.close(); break;
                    case sf::Keyboard::Key::Left:
                    case sf::Keyboard::Key::A: leftHeld = true; break;
                    case sf::Keyboard::Key::Right:
                    case sf::Keyboard::Key::D: rightHeld = true; break;
                    case sf::Keyboard::Key::Space: spaceLatch = true; break;
                    case sf::Keyboard::Key::Enter: enterLatch = true; break;
                    case sf::Keyboard::Key::R: rLatch = true; break;
                    default: break;
                }
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                switch (kr->code)
                {
                    case sf::Keyboard::Key::Left:
                    case sf::Keyboard::Key::A: leftHeld = false; break;
                    case sf::Keyboard::Key::Right:
                    case sf::Keyboard::Key::D: rightHeld = false; break;
                    default: break;
                }
            }
        }
        if (!window.isOpen())
            break;

        // ---------- 고정 틱 갱신 ----------
        accumulator += std::min(clock.restart().asSeconds(), 0.25f);
        while (accumulator >= DT)
        {
            accumulator -= DT;
            switch (screen)
            {
                case Screen::Title:
                    titleTimer += DT;
                    if (enterLatch || titleTimer >= 2.f)
                    {
                        enterLatch = false;
                        startPlay();
                    }
                    break;
                case Screen::Play:
                    enterLatch = false;
                    rLatch = false;
                    updatePlay();
                    break;
                case Screen::Result:
                    spaceLatch = false;
                    enterLatch = false;
                    if (rLatch)
                    {
                        rLatch = false;
                        startPlay();
                    }
                    break;
            }
        }

        // ---------- 그리기 ----------
        window.clear(sf::Color::Black);

        if (screen == Screen::Title)
        {
            drawCentered("STAR FORMATION", 48, 220.f);
            drawCentered("PRESS ENTER", 16, 320.f);
        }
        else if (screen == Screen::Play)
        {
            // 방어벽
            for (int s = 0; s < 4; ++s)
                for (int j = 0; j < SH_ROWS; ++j)
                    for (int i = 0; i < SH_COLS; ++i)
                        if (shields[static_cast<std::size_t>(s)][static_cast<std::size_t>(j * SH_COLS + i)])
                        {
                            piece.setPosition({shieldLeft(s) + SH_PIECE * i, SH_TOP + SH_PIECE * j});
                            window.draw(piece);
                        }
            // 적
            const int frame = static_cast<int>(animTimer / 0.5f) % 2;
            for (int r = 0; r < ROWS; ++r)
                for (int c = 0; c < COLS; ++c)
                {
                    if (!enemyAlive[static_cast<std::size_t>(r * COLS + c)])
                        continue;
                    const int base = (r == 0) ? 0 : (r == 1 ? 2 : 4);
                    enemySprite.setTextureRect(sf::IntRect({(base + frame) * 32, 0}, {32, 24}));
                    enemySprite.setPosition(enemyPos(c, r));
                    window.draw(enemySprite);
                }
            // 보너스
            if (bonus)
            {
                bonusSprite.setPosition({bonus->x, BONUS_Y});
                window.draw(bonusSprite);
            }
            // 플레이어
            playerSprite.setPosition({playerX, PLAYER_Y});
            window.draw(playerSprite);
            // 탄
            if (playerBullet)
            {
                pbShape.setPosition(*playerBullet);
                window.draw(pbShape);
            }
            for (const auto& b : enemyBullets)
            {
                ebShape.setPosition(b.pos);
                window.draw(ebShape);
            }
            // 점수
            sf::Text tScore(font, "SCORE " + std::to_string(score), 16);
            tScore.setPosition({16.f, 12.f});
            window.draw(tScore);
            sf::Text tWave(font, "WAVE " + std::to_string(wave), 16);
            {
                const sf::FloatRect b = tWave.getLocalBounds();
                tWave.setPosition({WIN_W / 2.f - (b.position.x + b.size.x / 2.f), 12.f});
            }
            window.draw(tWave);
            sf::Text tLives(font, "LIVES " + std::to_string(lives), 16);
            {
                const sf::FloatRect b = tLives.getLocalBounds();
                tLives.setPosition({WIN_W - 16.f - (b.position.x + b.size.x), 12.f});
            }
            window.draw(tLives);
        }
        else
        {
            drawCentered("GAME OVER", 48, 180.f);
            drawCentered("SCORE " + std::to_string(score), 32, 260.f);
            drawCentered("WAVE " + std::to_string(wave), 32, 310.f);
            drawCentered("R: RESTART  ESC: QUIT", 16, 390.f);
        }

        window.display();
    }

    music.stop();
    return 0;
}
```