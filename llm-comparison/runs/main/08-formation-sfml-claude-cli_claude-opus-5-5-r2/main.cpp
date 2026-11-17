// STAR FORMATION - SFML 3.1 구현 (main.cpp 단일 파일)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr unsigned int kWinW = 960;
constexpr unsigned int kWinH = 540;
constexpr float kStep = 1.f / 60.f;

constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerY = 490.f;
constexpr float kPlayerSpeed = 300.f;
constexpr float kBulletW = 4.f;
constexpr float kBulletH = 12.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kEnemyShotSpeed = 240.f;
constexpr float kEnemyShotR = 3.f;
constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusY = 50.f;
constexpr float kBonusSpeed = 120.f;
constexpr float kPiece = 8.f;

enum class Screen { Title, Play, Result };

struct Enemy
{
    float x, y;
    int col, row;
};

struct EnemyShot
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

    sf::Sound sndShoot(bufShoot), sndHit(bufHit), sndLose(bufLose), sndBonus(bufBonus);

    sf::RenderWindow window(sf::VideoMode({kWinW, kWinH}), "STAR FORMATION", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    std::mt19937 rng(std::random_device{}());

    // ---------- 게임 상태 ----------
    Screen screen = Screen::Title;
    float titleTimer = 0.f;

    int score = 0;
    int lives = 3;
    int wave = 1;

    std::vector<Enemy> enemies;
    float formDir = 1.f;
    float animTimer = 0.f;
    float fireTimer = 0.f;

    std::vector<sf::FloatRect> pieces;

    float playerX = (kWinW - kPlayerW) / 2.f;
    std::optional<sf::Vector2f> playerBullet;
    std::vector<EnemyShot> enemyShots;

    std::optional<Bonus> bonus;
    float bonusTimer = 0.f;
    bool bonusNextLeft = true;

    float freezeTimer = 0.f;

    // 입력 상태
    bool leftHeld = false, rightHeld = false;
    bool leftTap = false, rightTap = false;
    bool spaceTap = false, enterTap = false, rTap = false;

    auto setupWave = [&]()
    {
        enemies.clear();
        const float offset = std::min(16.f * static_cast<float>(wave - 1), 64.f);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 8; ++c)
                enemies.push_back({296.f + 48.f * c, 80.f + 40.f * r + offset, c, r});
        formDir = 1.f;
        animTimer = 0.f;
        fireTimer = 0.f;

        pieces.clear();
        const float centers[4] = {192.f, 384.f, 576.f, 768.f};
        for (float cx : centers)
        {
            const float left = cx - 24.f;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 6; ++c)
                    pieces.emplace_back(sf::Vector2f{left + kPiece * c, 400.f + kPiece * r}, sf::Vector2f{kPiece, kPiece});
        }
        playerBullet.reset();
        enemyShots.clear();
        bonus.reset();
    };

    auto startPlay = [&]()
    {
        score = 0;
        lives = 3;
        wave = 1;
        playerX = (kWinW - kPlayerW) / 2.f;
        bonusTimer = 0.f;
        bonusNextLeft = true;
        freezeTimer = 0.f;
        setupWave();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto gotoResult = [&]()
    {
        screen = Screen::Result;
        music.stop();
        playerBullet.reset();
        enemyShots.clear();
    };

    auto enemyPoints = [](int row) { return row == 0 ? 30 : (row == 1 ? 20 : 10); };

    // ---------- 고정 틱 갱신 ----------
    auto updatePlay = [&](float dt)
    {
        if (freezeTimer > 0.f)
        {
            freezeTimer -= dt;
            if (freezeTimer < 0.f)
                freezeTimer = 0.f;
            return;
        }

        // 플레이어 이동
        float move = 0.f;
        if (leftHeld || leftTap)
            move -= 1.f;
        if (rightHeld || rightTap)
            move += 1.f;
        playerX += move * kPlayerSpeed * dt;
        playerX = std::clamp(playerX, 16.f, static_cast<float>(kWinW) - 16.f - kPlayerW);

        // 사격
        if (spaceTap && !playerBullet)
        {
            playerBullet = sf::Vector2f{playerX + kPlayerW / 2.f - kBulletW / 2.f, kPlayerY - kBulletH};
            sndShoot.play();
        }

        // 적 대열 이동
        animTimer += dt;
        if (!enemies.empty())
        {
            const float speed = 40.f + 4.f * static_cast<float>(32 - static_cast<int>(enemies.size()));
            for (auto& e : enemies)
                e.x += formDir * speed * dt;
            float minX = enemies.front().x, maxX = enemies.front().x;
            for (const auto& e : enemies)
            {
                minX = std::min(minX, e.x);
                maxX = std::max(maxX, e.x);
            }
            const bool hitRight = formDir > 0.f && maxX + kEnemyW >= 944.f;
            const bool hitLeft = formDir < 0.f && minX <= 16.f;
            if (hitRight || hitLeft)
            {
                for (auto& e : enemies)
                    e.y += 16.f;
                formDir = -formDir;
            }
        }

        // 적과 방어벽 겹침
        for (const auto& e : enemies)
        {
            std::erase_if(pieces, [&](const sf::FloatRect& p)
                          { return rectsOverlap(e.x, e.y, kEnemyW, kEnemyH, p.position.x, p.position.y, kPiece, kPiece); });
        }

        // 적이 y 490에 닿음
        for (const auto& e : enemies)
        {
            if (e.y + kEnemyH >= kPlayerY)
            {
                gotoResult();
                return;
            }
        }

        // 적 사격
        fireTimer += dt;
        if (fireTimer >= 1.f)
        {
            fireTimer -= 1.f;
            if (!enemies.empty() && enemyShots.size() < 3)
            {
                const float pcx = playerX + kPlayerW / 2.f;
                const float pcy = kPlayerY + kPlayerH / 2.f;
                int bestCol = -1;
                float bestDist = 0.f;
                for (int c = 0; c < 8; ++c)
                {
                    for (const auto& e : enemies)
                    {
                        if (e.col != c)
                            continue;
                        const float d = std::abs(e.x + kEnemyW / 2.f - pcx);
                        if (bestCol < 0 || d < bestDist)
                        {
                            bestCol = c;
                            bestDist = d;
                        }
                        break;
                    }
                }
                const Enemy* shooter = nullptr;
                for (const auto& e : enemies)
                    if (e.col == bestCol && (!shooter || e.y > shooter->y))
                        shooter = &e;
                if (shooter)
                {
                    const sf::Vector2f origin{shooter->x + kEnemyW / 2.f, shooter->y + kEnemyH};
                    sf::Vector2f dir{pcx - origin.x, pcy - origin.y};
                    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    dir = len > 0.0001f ? sf::Vector2f{dir.x / len, dir.y / len} : sf::Vector2f{0.f, 1.f};
                    enemyShots.push_back({origin, dir * kEnemyShotSpeed});
                }
            }
        }

        // 보너스 비행체
        bonusTimer += dt;
        if (bonusTimer >= 20.f)
        {
            bonusTimer -= 20.f;
            if (!bonus)
            {
                if (bonusNextLeft)
                    bonus = Bonus{-kBonusW, 1.f};
                else
                    bonus = Bonus{static_cast<float>(kWinW), -1.f};
                bonusNextLeft = !bonusNextLeft;
            }
        }
        if (bonus)
        {
            bonus->x += bonus->dir * kBonusSpeed * dt;
            if ((bonus->dir > 0.f && bonus->x >= static_cast<float>(kWinW)) ||
                (bonus->dir < 0.f && bonus->x + kBonusW <= 0.f))
                bonus.reset();
        }

        // 플레이어 총알
        if (playerBullet)
        {
            playerBullet->y -= kBulletSpeed * dt;
            const sf::Vector2f b = *playerBullet;
            bool gone = false;

            for (auto it = pieces.begin(); it != pieces.end(); ++it)
            {
                if (rectsOverlap(b.x, b.y, kBulletW, kBulletH, it->position.x, it->position.y, kPiece, kPiece))
                {
                    pieces.erase(it);
                    gone = true;
                    break;
                }
            }
            if (!gone)
            {
                for (auto it = enemies.begin(); it != enemies.end(); ++it)
                {
                    if (rectsOverlap(b.x, b.y, kBulletW, kBulletH, it->x, it->y, kEnemyW, kEnemyH))
                    {
                        score += enemyPoints(it->row);
                        enemies.erase(it);
                        sndHit.play();
                        gone = true;
                        break;
                    }
                }
            }
            if (!gone && bonus &&
                rectsOverlap(b.x, b.y, kBulletW, kBulletH, bonus->x, kBonusY, kBonusW, kBonusH))
            {
                std::uniform_int_distribution<int> pick(1, 3);
                score += 50 * pick(rng);
                bonus.reset();
                sndBonus.play();
                gone = true;
            }
            if (!gone && b.y + kBulletH < 0.f)
                gone = true;
            if (gone)
                playerBullet.reset();
        }

        // 적 탄
        bool playerHit = false;
        for (auto& s : enemyShots)
            s.pos += s.vel * dt;
        std::erase_if(enemyShots,
                      [&](const EnemyShot& s)
                      {
                          for (auto it = pieces.begin(); it != pieces.end(); ++it)
                          {
                              if (circleRect(s.pos, kEnemyShotR, it->position.x, it->position.y, kPiece, kPiece))
                              {
                                  pieces.erase(it);
                                  return true;
                              }
                          }
                          if (circleRect(s.pos, kEnemyShotR, playerX, kPlayerY, kPlayerW, kPlayerH))
                          {
                              playerHit = true;
                              return true;
                          }
                          return s.pos.y - kEnemyShotR > static_cast<float>(kWinH) || s.pos.x + kEnemyShotR < 0.f ||
                                 s.pos.x - kEnemyShotR > static_cast<float>(kWinW) || s.pos.y + kEnemyShotR < 0.f;
                      });

        if (playerHit)
        {
            --lives;
            sndLose.play();
            enemyShots.clear();
            playerBullet.reset();
            if (lives <= 0)
            {
                lives = 0;
                gotoResult();
                return;
            }
            freezeTimer = 1.f;
        }

        // 웨이브 클리어
        if (enemies.empty())
        {
            ++wave;
            setupWave();
        }
    };

    auto update = [&](float dt)
    {
        switch (screen)
        {
        case Screen::Title:
            titleTimer += dt;
            if (enterTap || titleTimer >= 2.f)
                startPlay();
            break;
        case Screen::Play:
            updatePlay(dt);
            break;
        case Screen::Result:
            if (rTap)
                startPlay();
            break;
        }
        leftTap = rightTap = spaceTap = enterTap = rTap = false;
    };

    // ---------- 그리기 도우미 ----------
    sf::Sprite enemySprite(texEnemies);
    sf::Sprite playerSprite(texPlayer);
    sf::Sprite bonusSprite(texBonus);
    sf::RectangleShape pieceShape({kPiece, kPiece});
    pieceShape.setFillColor(sf::Color(0x4C, 0xC2, 0x6B));
    sf::RectangleShape bulletShape({kBulletW, kBulletH});
    bulletShape.setFillColor(sf::Color::White);
    sf::CircleShape shotShape(kEnemyShotR);
    shotShape.setOrigin({kEnemyShotR, kEnemyShotR});
    shotShape.setFillColor(sf::Color(0xFF, 0x6B, 0x6B));
    sf::Text text(font, "", 16);
    text.setFillColor(sf::Color::White);

    auto drawText = [&](const std::string& s, unsigned int size, float x, float y, int align)
    {
        text.setString(s);
        text.setCharacterSize(size);
        const sf::FloatRect b = text.getLocalBounds();
        float px = x - b.position.x;
        if (align == 1)
            px = x - (b.position.x + b.size.x / 2.f);
        else if (align == 2)
            px = x - (b.position.x + b.size.x);
        text.setPosition({std::round(px), std::round(y)});
        window.draw(text);
    };

    // ---------- 메인 루프 ----------
    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                using S = sf::Keyboard::Scan;
                switch (kp->scancode)
                {
                case S::Escape: window.close(); break;
                case S::Left:
                case S::A:
                    leftHeld = true;
                    leftTap = true;
                    break;
                case S::Right:
                case S::D:
                    rightHeld = true;
                    rightTap = true;
                    break;
                case S::Space: spaceTap = true; break;
                case S::Enter:
                case S::NumpadEnter: enterTap = true; break;
                case S::R: rTap = true; break;
                default: break;
                }
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                using S = sf::Keyboard::Scan;
                if (kr->scancode == S::Left || kr->scancode == S::A)
                    leftHeld = false;
                else if (kr->scancode == S::Right || kr->scancode == S::D)
                    rightHeld = false;
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                leftHeld = rightHeld = false;
            }
        }
        if (!window.isOpen())
            break;

        float frame = clock.restart().asSeconds();
        if (frame > 0.25f)
            frame = 0.25f;
        accumulator += frame;
        while (accumulator >= kStep)
        {
            update(kStep);
            accumulator -= kStep;
        }

        window.clear(sf::Color::Black);

        if (screen == Screen::Title)
        {
            drawText("STAR FORMATION", 48, 480.f, 200.f, 1);
            drawText("PRESS ENTER", 16, 480.f, 300.f, 1);
        }
        else if (screen == Screen::Play)
        {
            for (const auto& p : pieces)
            {
                pieceShape.setPosition(p.position);
                window.draw(pieceShape);
            }
            const int anim = static_cast<int>(animTimer / 0.5f) % 2;
            for (const auto& e : enemies)
            {
                const int base = e.row == 0 ? 0 : (e.row == 1 ? 2 : 4);
                enemySprite.setTextureRect(sf::IntRect({(base + anim) * 32, 0}, {32, 24}));
                enemySprite.setPosition({std::round(e.x), std::round(e.y)});
                window.draw(enemySprite);
            }
            if (bonus)
            {
                bonusSprite.setPosition({std::round(bonus->x), kBonusY});
                window.draw(bonusSprite);
            }
            playerSprite.setPosition({std::round(playerX), kPlayerY});
            window.draw(playerSprite);
            if (playerBullet)
            {
                bulletShape.setPosition(*playerBullet);
                window.draw(bulletShape);
            }
            for (const auto& s : enemyShots)
            {
                shotShape.setPosition(s.pos);
                window.draw(shotShape);
            }
            drawText("SCORE " + std::to_string(score), 16, 16.f, 12.f, 0);
            drawText("WAVE " + std::to_string(wave), 16, 480.f, 12.f, 1);
            drawText("LIVES " + std::to_string(lives), 16, 944.f, 12.f, 2);
        }
        else
        {
            drawText("GAME OVER", 48, 480.f, 160.f, 1);
            drawText("SCORE " + std::to_string(score), 32, 480.f, 250.f, 1);
            drawText("WAVE " + std::to_string(wave), 32, 480.f, 300.f, 1);
            drawText("R: RESTART  ESC: QUIT", 16, 480.f, 380.f, 1);
        }

        window.display();
    }

    music.stop();
    return 0;
}
