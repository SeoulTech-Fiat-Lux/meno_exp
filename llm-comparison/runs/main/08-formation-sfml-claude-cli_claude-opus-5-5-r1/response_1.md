```cpp
// STAR FORMATION - 고정 화면 슈팅 (SFML 3.1)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float kWinW = 960.f;
constexpr float kWinH = 540.f;
constexpr float kDt   = 1.f / 60.f;

constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;

constexpr float kPlayerW     = 40.f;
constexpr float kPlayerH     = 24.f;
constexpr float kPlayerY     = 490.f;
constexpr float kPlayerSpeed = 300.f;

constexpr float kBulletW     = 4.f;
constexpr float kBulletH     = 12.f;
constexpr float kBulletSpeed = 600.f;

constexpr float kEnemyBulletR     = 3.f;
constexpr float kEnemyBulletSpeed = 240.f;

constexpr float kBonusW     = 48.f;
constexpr float kBonusH     = 20.f;
constexpr float kBonusY     = 50.f;
constexpr float kBonusSpeed = 120.f;

const sf::Color kShieldColor(0x4C, 0xC2, 0x6B);
const sf::Color kEnemyBulletColor(0xFF, 0x6B, 0x6B);

enum class State
{
    Title,
    Play,
    Result
};

struct Enemy
{
    float x;
    float y;
    int   col;
    int   row;
};

struct EnemyBullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
};

struct Bonus
{
    float x;
    float vx;
};

bool rectsOverlap(const sf::FloatRect& a, const sf::FloatRect& b)
{
    return a.position.x < b.position.x + b.size.x && b.position.x < a.position.x + a.size.x &&
           a.position.y < b.position.y + b.size.y && b.position.y < a.position.y + a.size.y;
}

bool circleRect(sf::Vector2f c, float r, const sf::FloatRect& rect)
{
    const float cx = std::clamp(c.x, rect.position.x, rect.position.x + rect.size.x);
    const float cy = std::clamp(c.y, rect.position.y, rect.position.y + rect.size.y);
    const float dx = c.x - cx;
    const float dy = c.y - cy;
    return dx * dx + dy * dy <= r * r;
}

struct Game
{
    State state = State::Title;
    float titleTimer = 0.f;

    int score = 0;
    int lives = 3;
    int wave  = 1;

    std::vector<Enemy> enemies;
    float              enemyDir   = 1.f;
    float              fireTimer  = 1.f;
    float              animTimer  = 0.f;

    float                       playerX = (kWinW - kPlayerW) / 2.f;
    std::optional<sf::Vector2f> playerBullet; // 왼쪽 위
    std::vector<EnemyBullet>    enemyBullets;
    std::vector<sf::FloatRect>  shieldPieces;

    std::optional<Bonus> bonus;
    float                bonusTimer    = 0.f;
    bool                 bonusFromLeft = true;

    float freezeTimer = 0.f;

    void setupWave()
    {
        enemies.clear();
        const float offset = std::min(16.f * static_cast<float>(wave - 1), 64.f);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 8; ++c)
                enemies.push_back({296.f + 48.f * c, 80.f + 40.f * r + offset, c, r});
        enemyDir  = 1.f;
        fireTimer = 1.f;
        animTimer = 0.f;

        shieldPieces.clear();
        const float centers[4] = {192.f, 384.f, 576.f, 768.f};
        for (float cx : centers)
        {
            const float left = cx - 24.f;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 6; ++c)
                    shieldPieces.emplace_back(sf::Vector2f{left + 8.f * c, 400.f + 8.f * r}, sf::Vector2f{8.f, 8.f});
        }

        playerBullet.reset();
        enemyBullets.clear();
        bonus.reset();
    }

    void reset()
    {
        score         = 0;
        lives         = 3;
        wave          = 1;
        playerX       = (kWinW - kPlayerW) / 2.f;
        bonusTimer    = 0.f;
        bonusFromLeft = true;
        freezeTimer   = 0.f;
        setupWave();
    }
};

int enemyScore(int row)
{
    if (row == 0)
        return 30;
    if (row == 1)
        return 20;
    return 10;
}

int enemyFrameBase(int row)
{
    if (row == 0)
        return 0;
    if (row == 1)
        return 2;
    return 4;
}
} // namespace

int main()
{
    // ---- 에셋 로드 ----
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
    {
        std::cerr << "assets/fonts/game.ttf\n";
        return 1;
    }
    sf::Texture texEnemies;
    if (!texEnemies.loadFromFile("assets/textures/fm_enemies.png"))
    {
        std::cerr << "assets/textures/fm_enemies.png\n";
        return 1;
    }
    sf::Texture texPlayer;
    if (!texPlayer.loadFromFile("assets/textures/fm_player.png"))
    {
        std::cerr << "assets/textures/fm_player.png\n";
        return 1;
    }
    sf::Texture texBonus;
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

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "STAR FORMATION", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    std::mt19937 rng(std::random_device{}());

    Game game;
    game.reset();

    // ---- 입력 상태 ----
    bool heldLeft = false, heldRight = false;
    bool tapLeft = false, tapRight = false;
    bool reqFire = false, reqEnter = false, reqRestart = false;

    auto goResult = [&]()
    {
        game.state = State::Result;
        music.stop();
    };

    auto startPlay = [&]()
    {
        game.reset();
        game.state = State::Play;
        music.stop();
        music.play();
    };

    auto updatePlay = [&](float dt)
    {
        if (game.freezeTimer > 0.f)
        {
            game.freezeTimer -= dt;
            if (game.freezeTimer < 0.f)
                game.freezeTimer = 0.f;
            return;
        }

        // 플레이어 이동
        const bool left  = heldLeft || tapLeft;
        const bool right = heldRight || tapRight;
        float      dir   = 0.f;
        if (left)
            dir -= 1.f;
        if (right)
            dir += 1.f;
        game.playerX += dir * kPlayerSpeed * dt;
        game.playerX = std::clamp(game.playerX, 16.f, kWinW - 16.f - kPlayerW);

        // 사격
        if (reqFire && !game.playerBullet)
        {
            game.playerBullet = sf::Vector2f{game.playerX + (kPlayerW - kBulletW) / 2.f, kPlayerY - kBulletH};
            sndShoot.play();
        }

        // 플레이어 총알
        if (game.playerBullet)
        {
            game.playerBullet->y -= kBulletSpeed * dt;
            const sf::FloatRect br(*game.playerBullet, {kBulletW, kBulletH});
            bool                consumed = false;

            if (br.position.y + br.size.y < 0.f)
                consumed = true;

            if (!consumed)
            {
                // 방어벽: 가장 아래쪽 조각 하나
                int   best  = -1;
                float bestY = -1e9f;
                for (int i = 0; i < static_cast<int>(game.shieldPieces.size()); ++i)
                {
                    if (rectsOverlap(br, game.shieldPieces[i]) && game.shieldPieces[i].position.y > bestY)
                    {
                        bestY = game.shieldPieces[i].position.y;
                        best  = i;
                    }
                }
                if (best >= 0)
                {
                    game.shieldPieces.erase(game.shieldPieces.begin() + best);
                    consumed = true;
                }
            }
            if (!consumed)
            {
                for (auto it = game.enemies.begin(); it != game.enemies.end(); ++it)
                {
                    const sf::FloatRect er({it->x, it->y}, {kEnemyW, kEnemyH});
                    if (rectsOverlap(br, er))
                    {
                        game.score += enemyScore(it->row);
                        game.enemies.erase(it);
                        sndHit.play();
                        consumed = true;
                        break;
                    }
                }
            }
            if (!consumed && game.bonus)
            {
                const sf::FloatRect bon({game.bonus->x, kBonusY}, {kBonusW, kBonusH});
                if (rectsOverlap(br, bon))
                {
                    std::uniform_int_distribution<int> d(1, 3);
                    game.score += 50 * d(rng);
                    game.bonus.reset();
                    sndBonus.play();
                    consumed = true;
                }
            }
            if (consumed)
                game.playerBullet.reset();
        }

        // 웨이브 클리어
        if (game.enemies.empty())
        {
            ++game.wave;
            game.setupWave();
            return;
        }

        // 적 대열 이동
        const float speed = 40.f + 4.f * static_cast<float>(32 - static_cast<int>(game.enemies.size()));
        for (auto& e : game.enemies)
            e.x += game.enemyDir * speed * dt;
        float minX = 1e9f, maxX = -1e9f;
        for (const auto& e : game.enemies)
        {
            minX = std::min(minX, e.x);
            maxX = std::max(maxX, e.x + kEnemyW);
        }
        if (game.enemyDir > 0.f && maxX >= 944.f)
        {
            const float shift = 944.f - maxX;
            for (auto& e : game.enemies)
            {
                e.x += shift;
                e.y += 16.f;
            }
            game.enemyDir = -1.f;
        }
        else if (game.enemyDir < 0.f && minX <= 16.f)
        {
            const float shift = 16.f - minX;
            for (auto& e : game.enemies)
            {
                e.x += shift;
                e.y += 16.f;
            }
            game.enemyDir = 1.f;
        }

        // 적과 방어벽 겹침
        for (const auto& e : game.enemies)
        {
            const sf::FloatRect er({e.x, e.y}, {kEnemyW, kEnemyH});
            std::erase_if(game.shieldPieces, [&](const sf::FloatRect& p) { return rectsOverlap(er, p); });
        }

        // 적이 y 490에 닿음
        for (const auto& e : game.enemies)
        {
            if (e.y + kEnemyH >= kPlayerY)
            {
                goResult();
                return;
            }
        }

        // 적 사격
        game.fireTimer -= dt;
        if (game.fireTimer <= 0.f)
        {
            game.fireTimer += 1.f;
            if (game.enemyBullets.size() < 3)
            {
                const float pcx      = game.playerX + kPlayerW / 2.f;
                const float pcy      = kPlayerY + kPlayerH / 2.f;
                int         bestCol  = -1;
                float       bestDist = 1e9f;
                for (int c = 0; c < 8; ++c)
                {
                    for (const auto& e : game.enemies)
                    {
                        if (e.col == c)
                        {
                            const float d = std::abs(e.x + kEnemyW / 2.f - pcx);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestCol  = c;
                            }
                            break;
                        }
                    }
                }
                const Enemy* shooter = nullptr;
                for (const auto& e : game.enemies)
                    if (e.col == bestCol && (!shooter || e.y > shooter->y))
                        shooter = &e;
                if (shooter)
                {
                    const sf::Vector2f start{shooter->x + kEnemyW / 2.f, shooter->y + kEnemyH};
                    sf::Vector2f       d{pcx - start.x, pcy - start.y};
                    const float        len = d.length();
                    if (len > 0.0001f)
                        d /= len;
                    else
                        d = {0.f, 1.f};
                    game.enemyBullets.push_back({start, d * kEnemyBulletSpeed});
                }
            }
        }

        // 적 탄
        const sf::FloatRect playerRect({game.playerX, kPlayerY}, {kPlayerW, kPlayerH});
        bool                playerHit = false;
        for (auto it = game.enemyBullets.begin(); it != game.enemyBullets.end();)
        {
            it->pos += it->vel * dt;
            bool remove = false;
            if (it->pos.y - kEnemyBulletR > kWinH || it->pos.y + kEnemyBulletR < 0.f ||
                it->pos.x + kEnemyBulletR < 0.f || it->pos.x - kEnemyBulletR > kWinW)
                remove = true;
            if (!remove)
            {
                int   best  = -1;
                float bestY = 1e9f;
                for (int i = 0; i < static_cast<int>(game.shieldPieces.size()); ++i)
                {
                    if (circleRect(it->pos, kEnemyBulletR, game.shieldPieces[i]) &&
                        game.shieldPieces[i].position.y < bestY)
                    {
                        bestY = game.shieldPieces[i].position.y;
                        best  = i;
                    }
                }
                if (best >= 0)
                {
                    game.shieldPieces.erase(game.shieldPieces.begin() + best);
                    remove = true;
                }
            }
            if (!remove && circleRect(it->pos, kEnemyBulletR, playerRect))
            {
                playerHit = true;
                remove    = true;
            }
            if (remove)
                it = game.enemyBullets.erase(it);
            else
                ++it;
        }
        if (playerHit)
        {
            --game.lives;
            sndLose.play();
            game.enemyBullets.clear();
            game.playerBullet.reset();
            if (game.lives <= 0)
            {
                game.lives = 0;
                goResult();
                return;
            }
            game.freezeTimer = 1.f;
            return;
        }

        // 보너스 비행체
        game.bonusTimer += dt;
        if (game.bonusTimer >= 20.f)
        {
            game.bonusTimer -= 20.f;
            if (game.bonusFromLeft)
                game.bonus = Bonus{-kBonusW, kBonusSpeed};
            else
                game.bonus = Bonus{kWinW, -kBonusSpeed};
            game.bonusFromLeft = !game.bonusFromLeft;
        }
        if (game.bonus)
        {
            game.bonus->x += game.bonus->vx * dt;
            if ((game.bonus->vx > 0.f && game.bonus->x >= kWinW) ||
                (game.bonus->vx < 0.f && game.bonus->x + kBonusW <= 0.f))
                game.bonus.reset();
        }

        game.animTimer += dt;
    };

    auto update = [&](float dt)
    {
        switch (game.state)
        {
            case State::Title:
                game.titleTimer += dt;
                if (reqEnter || game.titleTimer >= 2.f)
                    startPlay();
                break;
            case State::Play:
                updatePlay(dt);
                break;
            case State::Result:
                if (reqRestart)
                    startPlay();
                break;
        }
    };

    // ---- 그리기 도구 ----
    sf::Text text(font, "", 16);
    text.setFillColor(sf::Color::White);
    // align: 0 = 왼쪽, 1 = 가운데, 2 = 오른쪽
    auto drawText = [&](const std::string& s, unsigned size, float x, float y, int align)
    {
        text.setString(s);
        text.setCharacterSize(size);
        const sf::FloatRect b = text.getLocalBounds();
        float               ox = b.position.x;
        if (align == 1)
            ox = b.position.x + b.size.x / 2.f;
        else if (align == 2)
            ox = b.position.x + b.size.x;
        float oy = (align == 1) ? b.position.y + b.size.y / 2.f : 0.f;
        text.setOrigin({ox, oy});
        text.setPosition({std::round(x), std::round(y)});
        window.draw(text);
    };

    sf::Sprite         enemySprite(texEnemies);
    sf::Sprite         playerSprite(texPlayer);
    sf::Sprite         bonusSprite(texBonus);
    sf::RectangleShape pieceShape({8.f, 8.f});
    pieceShape.setFillColor(kShieldColor);
    sf::RectangleShape bulletShape({kBulletW, kBulletH});
    bulletShape.setFillColor(sf::Color::White);
    sf::CircleShape ebShape(kEnemyBulletR);
    ebShape.setFillColor(kEnemyBulletColor);
    ebShape.setOrigin({kEnemyBulletR, kEnemyBulletR});

    sf::Clock clock;
    float     accumulator = 0.f;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                heldLeft  = false;
                heldRight = false;
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                using S = sf::Keyboard::Scancode;
                switch (kp->scancode)
                {
                    case S::Escape:
                        window.close();
                        break;
                    case S::Left:
                    case S::A:
                        heldLeft = true;
                        tapLeft  = true;
                        break;
                    case S::Right:
                    case S::D:
                        heldRight = true;
                        tapRight  = true;
                        break;
                    case S::Space:
                        reqFire = true;
                        break;
                    case S::Enter:
                    case S::NumpadEnter:
                        reqEnter = true;
                        break;
                    case S::R:
                        reqRestart = true;
                        break;
                    default:
                        break;
                }
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                using S = sf::Keyboard::Scancode;
                if (kr->scancode == S::Left || kr->scancode == S::A)
                    heldLeft = false;
                else if (kr->scancode == S::Right || kr->scancode == S::D)
                    heldRight = false;
            }
        }
        if (!window.isOpen())
            break;

        float frame = clock.restart().asSeconds();
        if (frame > 0.1f)
            frame = 0.1f;
        accumulator += frame;
        while (accumulator >= kDt)
        {
            update(kDt);
            accumulator -= kDt;
            tapLeft    = false;
            tapRight   = false;
            reqFire    = false;
            reqEnter   = false;
            reqRestart = false;
        }

        // ---- 렌더 ----
        window.clear(sf::Color::Black);

        if (game.state == State::Title)
        {
            drawText("STAR FORMATION", 48, kWinW / 2.f, 220.f, 1);
            drawText("PRESS ENTER", 32, kWinW / 2.f, 320.f, 1);
        }
        else if (game.state == State::Play)
        {
            // 방어벽
            for (const auto& p : game.shieldPieces)
            {
                pieceShape.setPosition(p.position);
                window.draw(pieceShape);
            }
            // 적
            const int frameIdx = static_cast<int>(game.animTimer / 0.5f) % 2;
            for (const auto& e : game.enemies)
            {
                const int cell = enemyFrameBase(e.row) + frameIdx;
                enemySprite.setTextureRect(sf::IntRect({cell * 32, 0}, {32, 24}));
                enemySprite.setPosition({e.x, e.y});
                window.draw(enemySprite);
            }
            // 보너스
            if (game.bonus)
            {
                bonusSprite.setPosition({game.bonus->x, kBonusY});
                window.draw(bonusSprite);
            }
            // 플레이어
            playerSprite.setPosition({game.playerX, kPlayerY});
            window.draw(playerSprite);
            // 탄
            if (game.playerBullet)
            {
                bulletShape.setPosition(*game.playerBullet);
                window.draw(bulletShape);
            }
            for (const auto& b : game.enemyBullets)
            {
                ebShape.setPosition(b.pos);
                window.draw(ebShape);
            }
            // 점수
            drawText("SCORE " + std::to_string(game.score), 16, 16.f, 8.f, 0);
            drawText("WAVE " + std::to_string(game.wave), 16, kWinW / 2.f, 16.f, 1);
            drawText("LIVES " + std::to_string(game.lives), 16, kWinW - 16.f, 8.f, 2);
        }
        else
        {
            drawText("GAME OVER", 48, kWinW / 2.f, 180.f, 1);
            drawText("SCORE " + std::to_string(game.score), 32, kWinW / 2.f, 260.f, 1);
            drawText("WAVE " + std::to_string(game.wave), 32, kWinW / 2.f, 310.f, 1);
            drawText("R: RESTART  ESC: QUIT", 16, kWinW / 2.f, 380.f, 1);
        }

        window.display();
    }

    music.stop();
    return 0;
}
```