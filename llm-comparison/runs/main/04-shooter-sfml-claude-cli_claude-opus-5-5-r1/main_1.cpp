// ARENA - top-down shooter (SFML 3.1, C++20)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float WORLD_W = 1920.f;
constexpr float WORLD_H = 1080.f;
constexpr float VIEW_W  = 960.f;
constexpr float VIEW_H  = 540.f;
constexpr float DT      = 1.f / 60.f;

constexpr float PLAYER_SPEED   = 240.f;
constexpr float PLAYER_RADIUS  = 14.f;
constexpr float PLAYER_MARGIN  = 16.f;
constexpr float FIRE_INTERVAL  = 0.15f;
constexpr float BULLET_SPEED   = 600.f;
constexpr float BULLET_RADIUS  = 4.f;
constexpr float BULLET_LIFE    = 1.4f;
constexpr float ENEMY_SPEED    = 100.f;
constexpr float ENEMY_RADIUS   = 14.f;
constexpr std::size_t MAX_ENEMIES = 200;
constexpr float SPAWN_MARGIN   = 64.f;
constexpr int   START_HP       = 3;
constexpr float INVULN_TIME    = 1.f;
constexpr float TITLE_TIME     = 2.f;

const sf::Color COLOR_BG(0x14, 0x18, 0x1F);
const sf::Color COLOR_GRID(0x22, 0x2A, 0x35);
const sf::Color COLOR_BORDER(0x5C, 0x6B, 0x80);
const sf::Color COLOR_TEXT(0xFF, 0xFF, 0xFF);

struct Bullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float        life  = 0.f;
    bool         alive = true;
};

struct Enemy
{
    sf::Vector2f pos;
    bool         alive = true;
};

class SoundPool
{
public:
    SoundPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        m_sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            m_sounds.emplace_back(buffer);
    }

    void play()
    {
        m_sounds[m_next].stop();
        m_sounds[m_next].play();
        m_next = (m_next + 1) % m_sounds.size();
    }

private:
    std::vector<sf::Sound> m_sounds;
    std::size_t            m_next = 0;
};

enum class Screen
{
    Title,
    Play,
    Result
};

sf::FloatRect computeViewport(sf::Vector2u size)
{
    if (size.x == 0 || size.y == 0)
        return sf::FloatRect({0.f, 0.f}, {1.f, 1.f});
    const float w      = static_cast<float>(size.x);
    const float h      = static_cast<float>(size.y);
    const float target = VIEW_W / VIEW_H;
    if (w / h > target)
    {
        const float vw = (h * target) / w;
        return sf::FloatRect({(1.f - vw) / 2.f, 0.f}, {vw, 1.f});
    }
    const float vh = (w / target) / h;
    return sf::FloatRect({0.f, (1.f - vh) / 2.f}, {1.f, vh});
}

sf::Vector2f clampCamera(sf::Vector2f p)
{
    return {std::clamp(p.x, VIEW_W / 2.f, WORLD_W - VIEW_W / 2.f),
            std::clamp(p.y, VIEW_H / 2.f, WORLD_H - VIEW_H / 2.f)};
}

void centerText(sf::Text& text, float x, float y)
{
    const sf::FloatRect bb = text.getLocalBounds();
    text.setOrigin({bb.position.x + bb.size.x / 2.f, bb.position.y + bb.size.y / 2.f});
    text.setPosition({x, y});
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
        std::cerr << "Failed to load assets/fonts/game.ttf\n";
        return 1;
    }
    sf::Texture playerTex, enemyTex, bulletTex;
    if (!playerTex.loadFromFile("assets/textures/player.png"))
    {
        std::cerr << "Failed to load assets/textures/player.png\n";
        return 1;
    }
    if (!enemyTex.loadFromFile("assets/textures/enemy.png"))
    {
        std::cerr << "Failed to load assets/textures/enemy.png\n";
        return 1;
    }
    if (!bulletTex.loadFromFile("assets/textures/bullet.png"))
    {
        std::cerr << "Failed to load assets/textures/bullet.png\n";
        return 1;
    }
    sf::SoundBuffer shootBuf, hitBuf, loseBuf;
    if (!shootBuf.loadFromFile("assets/sfx/shoot.wav"))
    {
        std::cerr << "Failed to load assets/sfx/shoot.wav\n";
        return 1;
    }
    if (!hitBuf.loadFromFile("assets/sfx/hit.wav"))
    {
        std::cerr << "Failed to load assets/sfx/hit.wav\n";
        return 1;
    }
    if (!loseBuf.loadFromFile("assets/sfx/lose.wav"))
    {
        std::cerr << "Failed to load assets/sfx/lose.wav\n";
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_shooter.ogg"))
    {
        std::cerr << "Failed to load assets/music/bgm_shooter.ogg\n";
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    SoundPool shootSfx(shootBuf, 8);
    SoundPool hitSfx(hitBuf, 8);
    SoundPool loseSfx(loseBuf, 4);

    // ---------- window ----------
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::FloatRect viewport = computeViewport(window.getSize());

    // ---------- drawables ----------
    sf::Sprite playerSprite(playerTex);
    playerSprite.setOrigin({16.f, 16.f});
    sf::Sprite enemySprite(enemyTex);
    enemySprite.setOrigin({16.f, 16.f});
    sf::Sprite bulletSprite(bulletTex);
    bulletSprite.setOrigin({4.f, 4.f});

    sf::VertexArray grid(sf::PrimitiveType::Lines);
    for (float x = 120.f; x < WORLD_W; x += 120.f)
    {
        grid.append(sf::Vertex{{x + 0.5f, 0.f}, COLOR_GRID});
        grid.append(sf::Vertex{{x + 0.5f, WORLD_H}, COLOR_GRID});
    }
    for (float y = 120.f; y < WORLD_H; y += 120.f)
    {
        grid.append(sf::Vertex{{0.f, y + 0.5f}, COLOR_GRID});
        grid.append(sf::Vertex{{WORLD_W, y + 0.5f}, COLOR_GRID});
    }

    sf::RectangleShape border({WORLD_W, WORLD_H});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(COLOR_BORDER);
    border.setOutlineThickness(-2.f);

    sf::RectangleShape background({VIEW_W, VIEW_H});
    background.setFillColor(COLOR_BG);

    sf::Text hudText(font, "", 16);
    hudText.setFillColor(COLOR_TEXT);
    hudText.setPosition({8.f, 8.f});

    sf::Text bigText(font, "", 48);
    bigText.setFillColor(COLOR_TEXT);
    sf::Text midText(font, "", 32);
    midText.setFillColor(COLOR_TEXT);
    sf::Text smallText(font, "", 16);
    smallText.setFillColor(COLOR_TEXT);

    // ---------- game state ----------
    std::mt19937 rng(std::random_device{}());

    Screen screen      = Screen::Title;
    float  titleTimer  = 0.f;
    bool   paused      = false;

    sf::Vector2f playerPos{WORLD_W / 2.f, WORLD_H / 2.f};
    sf::Vector2f aimDir{1.f, 0.f};
    sf::Vector2f camCenter = clampCamera(playerPos);
    int          hp        = START_HP;
    int          score     = 0;
    float        playTime  = 0.f;
    float        spawnTimer = 0.5f;
    float        fireCooldown = 0.f;
    float        invuln    = 0.f;
    std::vector<Bullet> bullets;
    std::vector<Enemy>  enemies;
    bullets.reserve(64);
    enemies.reserve(MAX_ENEMIES + 8);

    // input state: 0 up(W),1 down(S),2 left(A),3 right(D),4 Up,5 Down,6 Left,7 Right
    std::array<bool, 8> keyHeld{};
    std::array<bool, 8> keyTapped{};
    bool mouseHeld    = false;
    bool clickPending = false;

    auto keyIndex = [](sf::Keyboard::Scancode sc) -> int
    {
        using S = sf::Keyboard::Scan;
        switch (sc)
        {
            case S::W: return 0;
            case S::S: return 1;
            case S::A: return 2;
            case S::D: return 3;
            case S::Up: return 4;
            case S::Down: return 5;
            case S::Left: return 6;
            case S::Right: return 7;
            default: return -1;
        }
    };

    auto startPlay = [&]()
    {
        screen       = Screen::Play;
        paused       = false;
        playerPos    = {WORLD_W / 2.f, WORLD_H / 2.f};
        aimDir       = {1.f, 0.f};
        camCenter    = clampCamera(playerPos);
        hp           = START_HP;
        score        = 0;
        playTime     = 0.f;
        spawnTimer   = 0.5f;
        fireCooldown = 0.f;
        invuln       = 0.f;
        bullets.clear();
        enemies.clear();
        clickPending = false;
        music.stop();
        music.play();
    };

    auto randomSpawnPoint = [&]() -> sf::Vector2f
    {
        const float l = camCenter.x - VIEW_W / 2.f - SPAWN_MARGIN;
        const float r = camCenter.x + VIEW_W / 2.f + SPAWN_MARGIN;
        const float t = camCenter.y - VIEW_H / 2.f - SPAWN_MARGIN;
        const float b = camCenter.y + VIEW_H / 2.f + SPAWN_MARGIN;
        struct Seg
        {
            bool  horiz;
            float fixed;
            float a;
            float b;
        };
        std::vector<Seg> segs;
        auto addH = [&](float y)
        {
            if (y < 0.f || y > WORLD_H)
                return;
            const float a = std::max(l, 0.f), e = std::min(r, WORLD_W);
            if (e > a)
                segs.push_back({true, y, a, e});
        };
        auto addV = [&](float x)
        {
            if (x < 0.f || x > WORLD_W)
                return;
            const float a = std::max(t, 0.f), e = std::min(b, WORLD_H);
            if (e > a)
                segs.push_back({false, x, a, e});
        };
        addH(t);
        addH(b);
        addV(l);
        addV(r);
        float total = 0.f;
        for (const auto& s : segs)
            total += s.b - s.a;
        if (segs.empty() || total <= 0.f)
        {
            std::uniform_real_distribution<float> dx(0.f, WORLD_W), dy(0.f, WORLD_H);
            return {dx(rng), dy(rng)};
        }
        std::uniform_real_distribution<float> dist(0.f, total);
        float pick = dist(rng);
        for (const auto& s : segs)
        {
            const float len = s.b - s.a;
            if (pick <= len)
            {
                const float v = s.a + pick;
                return s.horiz ? sf::Vector2f{v, s.fixed} : sf::Vector2f{s.fixed, v};
            }
            pick -= len;
        }
        const auto& s = segs.back();
        return s.horiz ? sf::Vector2f{s.b, s.fixed} : sf::Vector2f{s.fixed, s.b};
    };

    auto makeGameView = [&]()
    {
        sf::View v(camCenter, {VIEW_W, VIEW_H});
        v.setViewport(viewport);
        return v;
    };
    auto makeHudView = [&]()
    {
        sf::View v(sf::FloatRect({0.f, 0.f}, {VIEW_W, VIEW_H}));
        v.setViewport(viewport);
        return v;
    };

    auto updatePlay = [&]()
    {
        playTime += DT;

        // movement
        auto active = [&](int i) { return keyHeld[i] || keyTapped[i]; };
        sf::Vector2f move{0.f, 0.f};
        if (active(0) || active(4))
            move.y -= 1.f;
        if (active(1) || active(5))
            move.y += 1.f;
        if (active(2) || active(6))
            move.x -= 1.f;
        if (active(3) || active(7))
            move.x += 1.f;
        if (move.x != 0.f || move.y != 0.f)
        {
            move = move.normalized();
            playerPos += move * PLAYER_SPEED * DT;
        }
        playerPos.x = std::clamp(playerPos.x, PLAYER_MARGIN, WORLD_W - PLAYER_MARGIN);
        playerPos.y = std::clamp(playerPos.y, PLAYER_MARGIN, WORLD_H - PLAYER_MARGIN);
        keyTapped.fill(false);

        // camera & aim
        camCenter = clampCamera(playerPos);
        {
            const sf::View     gv    = makeGameView();
            const sf::Vector2i mpix  = sf::Mouse::getPosition(window);
            const sf::Vector2f world = window.mapPixelToCoords(mpix, gv);
            const sf::Vector2f d     = world - playerPos;
            if (d.lengthSquared() > 0.0001f)
                aimDir = d.normalized();
        }

        // shooting
        fireCooldown = std::max(0.f, fireCooldown - DT);
        if ((mouseHeld || clickPending) && fireCooldown <= 0.f)
        {
            bullets.push_back({playerPos, aimDir * BULLET_SPEED, 0.f, true});
            shootSfx.play();
            fireCooldown = FIRE_INTERVAL;
            clickPending = false;
        }

        // bullets
        for (auto& b : bullets)
        {
            b.pos += b.vel * DT;
            b.life += DT;
            if (b.life >= BULLET_LIFE || b.pos.x < 0.f || b.pos.x > WORLD_W || b.pos.y < 0.f ||
                b.pos.y > WORLD_H)
                b.alive = false;
        }

        // spawning
        spawnTimer -= DT;
        if (spawnTimer <= 0.f)
        {
            const float interval =
                std::max(0.3f, 1.0f - 0.1f * std::floor(playTime / 10.f));
            if (enemies.size() < MAX_ENEMIES)
                enemies.push_back({randomSpawnPoint(), true});
            spawnTimer += interval;
            if (spawnTimer <= 0.f)
                spawnTimer = interval;
        }

        // enemies move
        for (auto& e : enemies)
        {
            const sf::Vector2f d = playerPos - e.pos;
            const float        l = d.length();
            if (l > 0.0001f)
            {
                const float step = std::min(ENEMY_SPEED * DT, l);
                e.pos += d / l * step;
            }
        }

        // bullet-enemy collisions
        constexpr float beR = BULLET_RADIUS + ENEMY_RADIUS;
        for (auto& b : bullets)
        {
            if (!b.alive)
                continue;
            for (auto& e : enemies)
            {
                if (!e.alive)
                    continue;
                if (distSq(b.pos, e.pos) < beR * beR)
                {
                    b.alive = false;
                    e.alive = false;
                    score += 10;
                    hitSfx.play();
                    break;
                }
            }
        }

        // player-enemy collisions
        invuln = std::max(0.f, invuln - DT);
        if (invuln <= 0.f)
        {
            constexpr float peR = PLAYER_RADIUS + ENEMY_RADIUS;
            for (auto& e : enemies)
            {
                if (!e.alive)
                    continue;
                if (distSq(playerPos, e.pos) < peR * peR)
                {
                    e.alive = false;
                    hp      = std::max(0, hp - 1);
                    loseSfx.play();
                    invuln = INVULN_TIME;
                    break;
                }
            }
        }

        std::erase_if(bullets, [](const Bullet& b) { return !b.alive; });
        std::erase_if(enemies, [](const Enemy& e) { return !e.alive; });

        if (hp <= 0)
        {
            screen = Screen::Result;
            music.stop();
            mouseHeld    = false;
            clickPending = false;
        }
    };

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
                viewport = computeViewport(resized->size);
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                keyHeld.fill(false);
                mouseHeld = false;
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                using S = sf::Keyboard::Scan;
                if (kp->scancode == S::Escape)
                {
                    window.close();
                    continue;
                }
                const int idx = keyIndex(kp->scancode);
                if (idx >= 0)
                {
                    keyHeld[idx]   = true;
                    keyTapped[idx] = true;
                }
                if (screen == Screen::Title && (kp->scancode == S::Enter || kp->scancode == S::NumpadEnter))
                {
                    startPlay();
                }
                else if (screen == Screen::Play && kp->scancode == S::P)
                {
                    paused = !paused;
                    if (paused)
                        music.pause();
                    else
                        music.play();
                }
                else if (screen == Screen::Result && kp->scancode == S::R)
                {
                    startPlay();
                }
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                const int idx = keyIndex(kr->scancode);
                if (idx >= 0)
                    keyHeld[idx] = false;
            }
            else if (const auto* mp = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mp->button == sf::Mouse::Button::Left)
                {
                    mouseHeld = true;
                    if (screen == Screen::Play && !paused)
                        clickPending = true;
                }
            }
            else if (const auto* mr = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mr->button == sf::Mouse::Button::Left)
                    mouseHeld = false;
            }
        }
        if (!window.isOpen())
            break;

        // ---------- update (fixed timestep) ----------
        float frameDt = frameClock.restart().asSeconds();
        frameDt       = std::min(frameDt, 0.25f);
        if (screen == Screen::Play && paused)
        {
            accumulator = 0.f;
        }
        else
        {
            accumulator += frameDt;
            while (accumulator >= DT)
            {
                accumulator -= DT;
                if (screen == Screen::Title)
                {
                    titleTimer += DT;
                    if (titleTimer >= TITLE_TIME)
                        startPlay();
                }
                else if (screen == Screen::Play)
                {
                    if (paused)
                        break;
                    updatePlay();
                }
                else
                {
                    break;
                }
            }
            if (screen != Screen::Play)
                accumulator = std::min(accumulator, DT);
        }

        // ---------- render ----------
        window.clear(sf::Color::Black);
        const sf::View hudView = makeHudView();

        if (screen == Screen::Play)
        {
            const sf::View gameView = makeGameView();
            window.setView(gameView);
            background.setPosition({camCenter.x - VIEW_W / 2.f, camCenter.y - VIEW_H / 2.f});
            window.draw(background);
            window.draw(grid);
            window.draw(border);

            for (const auto& e : enemies)
            {
                enemySprite.setPosition(e.pos);
                window.draw(enemySprite);
            }
            for (const auto& b : bullets)
            {
                bulletSprite.setPosition(b.pos);
                window.draw(bulletSprite);
            }

            playerSprite.setPosition(playerPos);
            playerSprite.setRotation(sf::radians(std::atan2(aimDir.y, aimDir.x)));
            if (invuln > 0.f && std::fmod(invuln, 0.2f) < 0.1f)
                playerSprite.setColor(sf::Color(255, 255, 255, 90));
            else if (invuln > 0.f)
                playerSprite.setColor(sf::Color(255, 255, 255, 180));
            else
                playerSprite.setColor(sf::Color::White);
            window.draw(playerSprite);

            window.setView(hudView);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "HP %d  SCORE %d  TIME %.1f", hp, score,
                          static_cast<double>(playTime));
            hudText.setString(buf);
            window.draw(hudText);

            if (paused)
            {
                bigText.setString("PAUSED");
                centerText(bigText, VIEW_W / 2.f, VIEW_H / 2.f);
                window.draw(bigText);
            }
        }
        else
        {
            window.setView(hudView);
            background.setPosition({0.f, 0.f});
            window.draw(background);

            if (screen == Screen::Title)
            {
                bigText.setString("ARENA");
                centerText(bigText, VIEW_W / 2.f, VIEW_H / 2.f - 40.f);
                window.draw(bigText);
                midText.setString("PRESS ENTER");
                centerText(midText, VIEW_W / 2.f, VIEW_H / 2.f + 40.f);
                window.draw(midText);
            }
            else
            {
                bigText.setString("GAME OVER");
                centerText(bigText, VIEW_W / 2.f, VIEW_H / 2.f - 90.f);
                window.draw(bigText);

                midText.setString("SCORE " + std::to_string(score));
                centerText(midText, VIEW_W / 2.f, VIEW_H / 2.f - 20.f);
                window.draw(midText);

                char buf[64];
                std::snprintf(buf, sizeof(buf), "TIME %.1f", static_cast<double>(playTime));
                midText.setString(buf);
                centerText(midText, VIEW_W / 2.f, VIEW_H / 2.f + 30.f);
                window.draw(midText);

                smallText.setString("R: RESTART  ESC: QUIT");
                centerText(smallText, VIEW_W / 2.f, VIEW_H / 2.f + 100.f);
                window.draw(smallText);
            }
        }

        window.display();
    }

    music.stop();
    return 0;
}
