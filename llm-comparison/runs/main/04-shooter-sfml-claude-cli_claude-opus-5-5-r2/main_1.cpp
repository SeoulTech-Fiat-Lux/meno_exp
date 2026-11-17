// ARENA - top-down shooter (SFML 3.1, C++20)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float kWinW = 960.f;
constexpr float kWinH = 540.f;
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kDt = 1.f / 60.f;

constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerMargin = 16.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletLife = 1.4f;
constexpr float kFireInterval = 0.15f;
constexpr float kEnemyRadius = 14.f;
constexpr float kEnemySpeed = 100.f;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kInvulnTime = 1.f;
constexpr float kTitleTime = 2.f;

const sf::Color kBgColor(0x14, 0x18, 0x1F);
const sf::Color kGridColor(0x22, 0x2A, 0x35);
const sf::Color kBorderColor(0x5C, 0x6B, 0x80);

enum class State
{
    Title,
    Play,
    Result
};

struct Bullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float        life = 0.f;
};

struct Enemy
{
    sf::Vector2f pos;
};

struct SfxPool
{
    std::vector<sf::Sound> sounds;
    std::size_t            next = 0;

    SfxPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            sounds.emplace_back(buffer);
    }

    void play()
    {
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

sf::FloatRect computeViewport(sf::Vector2u size)
{
    if (size.x == 0 || size.y == 0)
        return sf::FloatRect({0.f, 0.f}, {1.f, 1.f});
    const float w      = static_cast<float>(size.x);
    const float h      = static_cast<float>(size.y);
    const float target = 16.f / 9.f;
    const float ratio  = w / h;
    if (ratio > target)
    {
        const float vw = target / ratio;
        return sf::FloatRect({(1.f - vw) / 2.f, 0.f}, {vw, 1.f});
    }
    const float vh = ratio / target;
    return sf::FloatRect({0.f, (1.f - vh) / 2.f}, {1.f, vh});
}

void centerOrigin(sf::Text& text)
{
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin(b.position + b.size / 2.f);
}

float distSq(sf::Vector2f a, sf::Vector2f b)
{
    const sf::Vector2f d = a - b;
    return d.x * d.x + d.y * d.y;
}
} // namespace

int main()
{
    // ---------- assets ----------
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
    {
        std::cerr << "Failed to load asset: assets/fonts/game.ttf\n";
        return 1;
    }
    sf::Texture playerTex, enemyTex, bulletTex;
    if (!playerTex.loadFromFile("assets/textures/player.png"))
    {
        std::cerr << "Failed to load asset: assets/textures/player.png\n";
        return 1;
    }
    if (!enemyTex.loadFromFile("assets/textures/enemy.png"))
    {
        std::cerr << "Failed to load asset: assets/textures/enemy.png\n";
        return 1;
    }
    if (!bulletTex.loadFromFile("assets/textures/bullet.png"))
    {
        std::cerr << "Failed to load asset: assets/textures/bullet.png\n";
        return 1;
    }
    sf::SoundBuffer shootBuf, hitBuf, loseBuf;
    if (!shootBuf.loadFromFile("assets/sfx/shoot.wav"))
    {
        std::cerr << "Failed to load asset: assets/sfx/shoot.wav\n";
        return 1;
    }
    if (!hitBuf.loadFromFile("assets/sfx/hit.wav"))
    {
        std::cerr << "Failed to load asset: assets/sfx/hit.wav\n";
        return 1;
    }
    if (!loseBuf.loadFromFile("assets/sfx/lose.wav"))
    {
        std::cerr << "Failed to load asset: assets/sfx/lose.wav\n";
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_shooter.ogg"))
    {
        std::cerr << "Failed to load asset: assets/music/bgm_shooter.ogg\n";
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    SfxPool shootSfx(shootBuf, 6);
    SfxPool hitSfx(hitBuf, 6);
    SfxPool loseSfx(loseBuf, 3);

    // ---------- window ----------
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA", sf::Style::Default);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::View gameView(sf::FloatRect({0.f, 0.f}, {kWinW, kWinH}));
    sf::View hudView(sf::FloatRect({0.f, 0.f}, {kWinW, kWinH}));
    {
        const sf::FloatRect vp = computeViewport(window.getSize());
        gameView.setViewport(vp);
        hudView.setViewport(vp);
    }

    // ---------- static drawables ----------
    sf::RectangleShape background({kWorldW, kWorldH});
    background.setFillColor(kBgColor);

    sf::VertexArray grid(sf::PrimitiveType::Lines);
    for (float x = 0.f; x <= kWorldW; x += 120.f)
    {
        grid.append(sf::Vertex{{x, 0.f}, kGridColor});
        grid.append(sf::Vertex{{x, kWorldH}, kGridColor});
    }
    for (float y = 0.f; y <= kWorldH; y += 120.f)
    {
        grid.append(sf::Vertex{{0.f, y}, kGridColor});
        grid.append(sf::Vertex{{kWorldW, y}, kGridColor});
    }

    sf::RectangleShape border({kWorldW, kWorldH});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(kBorderColor);
    border.setOutlineThickness(-2.f); // inward so it stays visible

    sf::Sprite playerSprite(playerTex);
    playerSprite.setOrigin({16.f, 16.f});
    sf::Sprite enemySprite(enemyTex);
    enemySprite.setOrigin({16.f, 16.f});
    sf::Sprite bulletSprite(bulletTex);
    bulletSprite.setOrigin({4.f, 4.f});

    sf::Text titleText(font, "ARENA", 48);
    titleText.setFillColor(sf::Color::White);
    centerOrigin(titleText);
    titleText.setPosition({kWinW / 2.f, kWinH / 2.f - 40.f});

    sf::Text pressText(font, "PRESS ENTER", 16);
    pressText.setFillColor(sf::Color::White);
    centerOrigin(pressText);
    pressText.setPosition({kWinW / 2.f, kWinH / 2.f + 32.f});

    sf::Text hudText(font, "", 16);
    hudText.setFillColor(sf::Color::White);
    hudText.setPosition({8.f, 8.f});

    sf::Text pausedText(font, "PAUSED", 32);
    pausedText.setFillColor(sf::Color::White);
    centerOrigin(pausedText);
    pausedText.setPosition({kWinW / 2.f, kWinH / 2.f});

    sf::Text overText(font, "GAME OVER", 48);
    overText.setFillColor(sf::Color::White);
    sf::Text scoreText(font, "", 32);
    scoreText.setFillColor(sf::Color::White);
    sf::Text timeText(font, "", 32);
    timeText.setFillColor(sf::Color::White);
    sf::Text restartText(font, "R: RESTART  ESC: QUIT", 16);
    restartText.setFillColor(sf::Color::White);

    // ---------- game state ----------
    std::mt19937 rng(std::random_device{}());

    State state      = State::Title;
    float titleTimer = 0.f;
    bool  paused     = false;

    sf::Vector2f        playerPos{kWorldW / 2.f, kWorldH / 2.f};
    int                 hp         = 3;
    int                 score      = 0;
    float               playTime   = 0.f;
    float               spawnTimer = 0.5f;
    float               invuln     = 0.f;
    float               fireCooldown = 0.f;
    sf::Vector2f        aimDir{1.f, 0.f};
    std::vector<Bullet> bullets;
    std::vector<Enemy>  enemies;
    bullets.reserve(32);
    enemies.reserve(kMaxEnemies + 8);

    // input
    bool held[4]    = {false, false, false, false}; // L R U D
    bool tapped[4]  = {false, false, false, false};
    bool mouseHeld  = false;
    bool mouseLatch = false;
    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);

    auto updateCamera = [&]()
    {
        const float cx = std::clamp(playerPos.x, kWinW / 2.f, kWorldW - kWinW / 2.f);
        const float cy = std::clamp(playerPos.y, kWinH / 2.f, kWorldH - kWinH / 2.f);
        gameView.setCenter({cx, cy});
    };

    auto updateAim = [&]()
    {
        const sf::Vector2f world = window.mapPixelToCoords(mousePixel, gameView);
        const sf::Vector2f d     = world - playerPos;
        const float        len   = std::sqrt(d.x * d.x + d.y * d.y);
        if (len > 0.0001f)
            aimDir = d / len;
    };

    auto resetGame = [&]()
    {
        playerPos    = {kWorldW / 2.f, kWorldH / 2.f};
        hp           = 3;
        score        = 0;
        playTime     = 0.f;
        spawnTimer   = 0.5f;
        invuln       = 0.f;
        fireCooldown = 0.f;
        paused       = false;
        aimDir       = {1.f, 0.f};
        bullets.clear();
        enemies.clear();
        mouseLatch = false;
        for (bool& t : tapped)
            t = false;
        updateCamera();
    };

    auto startPlay = [&]()
    {
        resetGame();
        state = State::Play;
        music.stop();
        music.play();
    };

    auto spawnEnemy = [&]()
    {
        const sf::Vector2f c    = gameView.getCenter();
        const float        left = c.x - kWinW / 2.f - 64.f;
        const float        top  = c.y - kWinH / 2.f - 64.f;
        const float        w    = kWinW + 128.f;
        const float        h    = kWinH + 128.f;
        const float        per  = 2.f * (w + h);
        std::uniform_real_distribution<float> dist(0.f, per);
        sf::Vector2f p;
        bool         ok = false;
        for (int tries = 0; tries < 64 && !ok; ++tries)
        {
            float t = dist(rng);
            if (t < w)
                p = {left + t, top};
            else if ((t -= w) < h)
                p = {left + w, top + t};
            else if ((t -= h) < w)
                p = {left + w - t, top + h};
            else
            {
                t -= w;
                p = {left, top + h - t};
            }
            ok = p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
        }
        if (!ok)
        {
            p.x = std::clamp(p.x, 0.f, kWorldW);
            p.y = std::clamp(p.y, 0.f, kWorldH);
        }
        enemies.push_back({p});
    };

    auto goResult = [&]()
    {
        state = State::Result;
        music.stop();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "SCORE %d", score);
        scoreText.setString(buf);
        std::snprintf(buf, sizeof(buf), "TIME %.1f", static_cast<double>(playTime));
        timeText.setString(buf);
        centerOrigin(overText);
        centerOrigin(scoreText);
        centerOrigin(timeText);
        centerOrigin(restartText);
        overText.setPosition({kWinW / 2.f, kWinH / 2.f - 80.f});
        scoreText.setPosition({kWinW / 2.f, kWinH / 2.f - 10.f});
        timeText.setPosition({kWinW / 2.f, kWinH / 2.f + 34.f});
        restartText.setPosition({kWinW / 2.f, kWinH / 2.f + 96.f});
    };

    auto updatePlay = [&](float dt)
    {
        playTime += dt;

        // movement
        const bool left  = held[0] || tapped[0];
        const bool right = held[1] || tapped[1];
        const bool up    = held[2] || tapped[2];
        const bool down  = held[3] || tapped[3];
        for (bool& t : tapped)
            t = false;
        sf::Vector2f mv{0.f, 0.f};
        if (left)
            mv.x -= 1.f;
        if (right)
            mv.x += 1.f;
        if (up)
            mv.y -= 1.f;
        if (down)
            mv.y += 1.f;
        const float mlen = std::sqrt(mv.x * mv.x + mv.y * mv.y);
        if (mlen > 0.f)
            playerPos += mv / mlen * kPlayerSpeed * dt;
        playerPos.x = std::clamp(playerPos.x, kPlayerMargin, kWorldW - kPlayerMargin);
        playerPos.y = std::clamp(playerPos.y, kPlayerMargin, kWorldH - kPlayerMargin);

        updateCamera();
        updateAim();

        // shooting
        if (fireCooldown > 0.f)
            fireCooldown -= dt;
        if ((mouseHeld || mouseLatch) && fireCooldown <= 0.f)
        {
            bullets.push_back({playerPos, aimDir * kBulletSpeed, kBulletLife});
            shootSfx.play();
            fireCooldown += kFireInterval;
            if (fireCooldown < 0.f)
                fireCooldown = kFireInterval;
            mouseLatch = false;
        }

        // bullets
        for (Bullet& b : bullets)
        {
            b.pos += b.vel * dt;
            b.life -= dt;
        }
        std::erase_if(bullets,
                      [](const Bullet& b)
                      {
                          return b.life <= 0.f || b.pos.x < 0.f || b.pos.x > kWorldW || b.pos.y < 0.f ||
                                 b.pos.y > kWorldH;
                      });

        // enemies move
        for (Enemy& e : enemies)
        {
            const sf::Vector2f d   = playerPos - e.pos;
            const float        len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len > 0.0001f)
            {
                const float step = std::min(kEnemySpeed * dt, len);
                e.pos += d / len * step;
            }
        }

        // spawning
        spawnTimer -= dt;
        while (spawnTimer <= 0.f)
        {
            if (enemies.size() < kMaxEnemies)
                spawnEnemy();
            const float interval = std::max(0.3f, 1.0f - 0.1f * std::floor(playTime / 10.f));
            spawnTimer += interval;
        }

        // bullet vs enemy
        const float beR2 = (kBulletRadius + kEnemyRadius) * (kBulletRadius + kEnemyRadius);
        for (Bullet& b : bullets)
        {
            for (std::size_t i = 0; i < enemies.size(); ++i)
            {
                if (distSq(b.pos, enemies[i].pos) < beR2)
                {
                    enemies[i] = enemies.back();
                    enemies.pop_back();
                    b.life = -1.f;
                    score += 10;
                    hitSfx.play();
                    break;
                }
            }
        }
        std::erase_if(bullets, [](const Bullet& b) { return b.life <= 0.f; });

        // enemy vs player
        if (invuln > 0.f)
        {
            invuln -= dt;
        }
        else
        {
            const float peR2 = (kPlayerRadius + kEnemyRadius) * (kPlayerRadius + kEnemyRadius);
            for (std::size_t i = 0; i < enemies.size(); ++i)
            {
                if (distSq(playerPos, enemies[i].pos) < peR2)
                {
                    enemies[i] = enemies.back();
                    enemies.pop_back();
                    --hp;
                    loseSfx.play();
                    invuln = kInvulnTime;
                    break;
                }
            }
            if (hp <= 0)
            {
                hp = 0;
                goResult();
            }
        }
    };

    auto setDir = [&](sf::Keyboard::Scancode sc, bool down)
    {
        int idx = -1;
        switch (sc)
        {
            case sf::Keyboard::Scancode::A:
            case sf::Keyboard::Scancode::Left: idx = 0; break;
            case sf::Keyboard::Scancode::D:
            case sf::Keyboard::Scancode::Right: idx = 1; break;
            case sf::Keyboard::Scancode::W:
            case sf::Keyboard::Scancode::Up: idx = 2; break;
            case sf::Keyboard::Scancode::S:
            case sf::Keyboard::Scancode::Down: idx = 3; break;
            default: break;
        }
        if (idx < 0)
            return;
        held[idx] = down;
        if (down)
            tapped[idx] = true;
    };

    updateCamera();

    sf::Clock frameClock;
    float     accumulator = 0.f;

    while (window.isOpen())
    {
        // ---------- events ----------
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                const sf::FloatRect vp = computeViewport(resized->size);
                gameView.setViewport(vp);
                hudView.setViewport(vp);
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                for (bool& h : held)
                    h = false;
                mouseHeld = false;
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                const auto sc = kp->scancode;
                if (sc == sf::Keyboard::Scancode::Escape)
                {
                    window.close();
                }
                else if (state == State::Title)
                {
                    if (sc == sf::Keyboard::Scancode::Enter || sc == sf::Keyboard::Scancode::NumpadEnter)
                        startPlay();
                    setDir(sc, true);
                    for (bool& t : tapped)
                        t = false;
                }
                else if (state == State::Play)
                {
                    if (sc == sf::Keyboard::Scancode::P)
                    {
                        paused = !paused;
                        if (paused)
                            music.pause();
                        else
                            music.play();
                    }
                    else
                    {
                        setDir(sc, true);
                    }
                }
                else if (state == State::Result)
                {
                    if (sc == sf::Keyboard::Scancode::R)
                        startPlay();
                    setDir(sc, true);
                    for (bool& t : tapped)
                        t = false;
                }
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                const bool keepTap[4] = {tapped[0], tapped[1], tapped[2], tapped[3]};
                setDir(kr->scancode, false);
                for (int i = 0; i < 4; ++i)
                    tapped[i] = keepTap[i];
            }
            else if (const auto* mm = event->getIf<sf::Event::MouseMoved>())
            {
                mousePixel = mm->position;
            }
            else if (const auto* mp = event->getIf<sf::Event::MouseButtonPressed>())
            {
                mousePixel = mp->position;
                if (mp->button == sf::Mouse::Button::Left)
                {
                    mouseHeld = true;
                    if (state == State::Play && !paused)
                        mouseLatch = true;
                }
            }
            else if (const auto* mr = event->getIf<sf::Event::MouseButtonReleased>())
            {
                mousePixel = mr->position;
                if (mr->button == sf::Mouse::Button::Left)
                    mouseHeld = false;
            }
        }
        if (!window.isOpen())
            break;

        // ---------- update (fixed timestep) ----------
        float frameTime = frameClock.restart().asSeconds();
        if (frameTime > 0.25f)
            frameTime = 0.25f;
        accumulator += frameTime;
        while (accumulator >= kDt)
        {
            accumulator -= kDt;
            if (state == State::Title)
            {
                titleTimer += kDt;
                if (titleTimer >= kTitleTime)
                    startPlay();
            }
            else if (state == State::Play && !paused)
            {
                updatePlay(kDt);
            }
        }
        if (state == State::Play)
            updateAim();

        // ---------- render ----------
        window.clear(sf::Color::Black);

        if (state == State::Title)
        {
            window.setView(hudView);
            sf::RectangleShape bg({kWinW, kWinH});
            bg.setFillColor(kBgColor);
            window.draw(bg);
            window.draw(titleText);
            window.draw(pressText);
        }
        else if (state == State::Play)
        {
            window.setView(gameView);
            window.draw(background);
            window.draw(grid);
            window.draw(border);

            for (const Enemy& e : enemies)
            {
                enemySprite.setPosition(e.pos);
                window.draw(enemySprite);
            }
            for (const Bullet& b : bullets)
            {
                bulletSprite.setPosition(b.pos);
                window.draw(bulletSprite);
            }

            playerSprite.setPosition(playerPos);
            playerSprite.setRotation(sf::radians(std::atan2(aimDir.y, aimDir.x)));
            if (invuln > 0.f)
            {
                const bool dim = static_cast<int>(invuln / 0.1f) % 2 == 0;
                playerSprite.setColor(sf::Color(255, 255, 255, dim ? 90 : 200));
            }
            else
            {
                playerSprite.setColor(sf::Color::White);
            }
            window.draw(playerSprite);

            window.setView(hudView);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "HP %d  SCORE %d  TIME %.1f", hp, score, static_cast<double>(playTime));
            hudText.setString(buf);
            window.draw(hudText);
            if (paused)
                window.draw(pausedText);
        }
        else // Result
        {
            window.setView(hudView);
            sf::RectangleShape bg({kWinW, kWinH});
            bg.setFillColor(kBgColor);
            window.draw(bg);
            window.draw(overText);
            window.draw(scoreText);
            window.draw(timeText);
            window.draw(restartText);
        }

        window.display();
    }

    music.stop();
    return 0;
}
