// LAST STAND - SFML 3.1 서바이버류 미니게임
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float WORLD_W = 2880.f;
constexpr float WORLD_H = 1620.f;
constexpr float VIEW_W  = 960.f;
constexpr float VIEW_H  = 540.f;
constexpr float DT      = 1.f / 60.f;

constexpr float PLAYER_R        = 14.f;
constexpr float PLAYER_MAX_HP   = 100.f;
constexpr float PLAYER_SPEED    = 200.f;
constexpr float FIRE_INTERVAL   = 0.8f;
constexpr float FIRE_RANGE      = 400.f;
constexpr float BULLET_SPEED    = 500.f;
constexpr float BULLET_LIFE     = 1.2f;
constexpr float BULLET_R        = 4.f;
constexpr int   BULLET_DAMAGE   = 10;
constexpr float KNOCK_TIME      = 0.1f;
constexpr float KNOCK_SPEED     = 120.f;
constexpr float FLASH_TIME      = 0.1f;
constexpr float DEATH_TIME      = 0.1f;
constexpr float GEM_R           = 6.f;
constexpr float GEM_LIFE        = 30.f;
constexpr float GEM_SPEED       = 300.f;
constexpr float MAGNET_RANGE    = 60.f;
constexpr int   MAX_ENEMIES     = 400;
constexpr float WIN_TIME        = 300.f;
constexpr float PI              = 3.14159265358979f;

struct EnemyDef
{
    float radius;
    float hp;
    float speed;
    float dps;
    float cell;
    float animPeriod;
    int   gemValue;
};

constexpr std::array<EnemyDef, 3> ENEMY_DEFS{{
    {10.f, 10.f, 80.f, 10.f, 24.f, 0.3f, 1},  // walker
    {10.f, 6.f, 140.f, 6.f, 24.f, 0.15f, 1},  // runner
    {20.f, 60.f, 50.f, 20.f, 48.f, 0.4f, 5},  // brute
}};

const std::array<std::string, 6> CARD_NAMES{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40"};

const sf::Color COL_BG(0x1A, 0x14, 0x20);
const sf::Color COL_GRID(0x2A, 0x22, 0x33);
const sf::Color COL_BORDER(0x6B, 0x5B, 0x7B);
const sf::Color COL_XP_BG(0x2A, 0x22, 0x33);
const sf::Color COL_XP_FILL(0x4F, 0xC3, 0xF7);
const sf::Color COL_HP_BG(0x3A, 0x1F, 0x24);
const sf::Color COL_HP_FILL(0xE5, 0x53, 0x3D);
const sf::Color COL_CARD_BG(0x2A, 0x22, 0x33);
const sf::Color COL_CARD_EDGE(0xFF, 0xD5, 0x4A);
const sf::Color COL_GEM1(0x4F, 0xC3, 0xF7);
const sf::Color COL_GEM5(0xFF, 0xD5, 0x4A);
const sf::Color COL_HURT(0xFF, 0x60, 0x60);
const sf::Color COL_TEXT(0xFF, 0xFF, 0xFF);

struct Enemy
{
    sf::Vector2f pos;
    int          type       = 0;
    float        hp         = 0.f;
    float        flash      = 0.f;
    float        knock      = 0.f;
    sf::Vector2f knockDir;
    bool         dead       = false;
    float        deathTimer = 0.f;
};

struct Bullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float        life = BULLET_LIFE;
    bool         gone = false;
};

struct Gem
{
    sf::Vector2f pos;
    int          value = 1;
    float        life  = GEM_LIFE;
    bool         gone  = false;
};

enum class Screen
{
    Title,
    Play,
    Result
};

enum class Act
{
    Enter,
    Pause,
    Restart,
    Card1,
    Card2,
    Card3
};

struct SoundPool
{
    std::vector<sf::Sound> sounds;
    std::size_t            next = 0;

    SoundPool(const sf::SoundBuffer& buffer, std::size_t count)
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

struct Game
{
    sf::Vector2f playerPos{WORLD_W / 2.f, WORLD_H / 2.f};
    float        hp           = PLAYER_MAX_HP;
    float        maxHp        = PLAYER_MAX_HP;
    float        speed        = PLAYER_SPEED;
    float        fireInterval = FIRE_INTERVAL;
    float        magnet       = MAGNET_RANGE;
    int          damage       = BULLET_DAMAGE;
    int          projectiles  = 1;

    int   level     = 1;
    int   xp        = 0;
    int   kills     = 0;
    float playTime  = 0.f;
    float shootCd   = 0.f;
    float nextSpawn = 0.5f;

    float animTimer  = 0.f;
    int   animFrame  = 0;
    bool  facingLeft = false;
    bool  hurt       = false;

    bool               paused        = false;
    bool               choosing      = false;
    int                pendingLevels = 0;
    std::array<int, 3> cards{0, 1, 2};

    bool won = false;

    std::vector<Enemy>  enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem>    gems;

    Game()
    {
        enemies.reserve(MAX_ENEMIES + 8);
        bullets.reserve(64);
        gems.reserve(512);
    }
};

float dist2(sf::Vector2f a, sf::Vector2f b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

sf::Vector2f cameraCenter(sf::Vector2f p)
{
    return {std::clamp(p.x, VIEW_W / 2.f, WORLD_W - VIEW_W / 2.f),
            std::clamp(p.y, VIEW_H / 2.f, WORLD_H - VIEW_H / 2.f)};
}

std::string formatTime(float t)
{
    const int s = static_cast<int>(std::floor(std::max(0.f, t)));
    char      buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    return buf;
}

void addQuad(std::vector<sf::Vertex>& v,
             sf::Vector2f              center,
             float                     w,
             float                     h,
             float                     u,
             float                     tv,
             bool                      flip,
             sf::Color                 color)
{
    const float l  = center.x - w / 2.f;
    const float r  = center.x + w / 2.f;
    const float t  = center.y - h / 2.f;
    const float b  = center.y + h / 2.f;
    float       u0 = u;
    float       u1 = u + w;
    if (flip)
        std::swap(u0, u1);
    const float v0 = tv;
    const float v1 = tv + h;
    v.push_back(sf::Vertex{{l, t}, color, {u0, v0}});
    v.push_back(sf::Vertex{{r, t}, color, {u1, v0}});
    v.push_back(sf::Vertex{{l, b}, color, {u0, v1}});
    v.push_back(sf::Vertex{{l, b}, color, {u0, v1}});
    v.push_back(sf::Vertex{{r, t}, color, {u1, v0}});
    v.push_back(sf::Vertex{{r, b}, color, {u1, v1}});
}

void addRect(std::vector<sf::Vertex>& v, float x, float y, float w, float h, sf::Color color)
{
    v.push_back(sf::Vertex{{x, y}, color, {}});
    v.push_back(sf::Vertex{{x + w, y}, color, {}});
    v.push_back(sf::Vertex{{x, y + h}, color, {}});
    v.push_back(sf::Vertex{{x, y + h}, color, {}});
    v.push_back(sf::Vertex{{x + w, y}, color, {}});
    v.push_back(sf::Vertex{{x + w, y + h}, color, {}});
}

sf::FloatRect letterboxViewport(sf::Vector2u size)
{
    if (size.x == 0 || size.y == 0)
        return sf::FloatRect({0.f, 0.f}, {1.f, 1.f});
    const float winRatio    = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float targetRatio = VIEW_W / VIEW_H;
    if (winRatio > targetRatio)
    {
        const float w = targetRatio / winRatio;
        return sf::FloatRect({(1.f - w) / 2.f, 0.f}, {w, 1.f});
    }
    const float h = winRatio / targetRatio;
    return sf::FloatRect({0.f, (1.f - h) / 2.f}, {1.f, h});
}

bool keyIs(const sf::Event::KeyPressed& k, sf::Keyboard::Scan sc, sf::Keyboard::Key code)
{
    return k.scancode == sc || k.code == code;
}

int moveIndex(sf::Keyboard::Scancode sc, sf::Keyboard::Key code)
{
    using S = sf::Keyboard::Scan;
    using K = sf::Keyboard::Key;
    if (sc == S::A || code == K::A)
        return 0;
    if (sc == S::Left || code == K::Left)
        return 1;
    if (sc == S::D || code == K::D)
        return 2;
    if (sc == S::Right || code == K::Right)
        return 3;
    if (sc == S::W || code == K::W)
        return 4;
    if (sc == S::Up || code == K::Up)
        return 5;
    if (sc == S::S || code == K::S)
        return 6;
    if (sc == S::Down || code == K::Down)
        return 7;
    return -1;
}

int run()
{
    bool        loadFailed = false;
    const auto  fail       = [&](const std::string& path)
    {
        std::cerr << "Failed to load asset: " << path << std::endl;
        loadFailed = true;
    };

    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
        fail("assets/fonts/game.ttf");

    sf::Texture texPlayer, texWalker, texRunner, texBrute, texBullet, texGem;
    const std::array<std::pair<sf::Texture*, const char*>, 6> texList{{
        {&texPlayer, "assets/textures/sv_player.png"},
        {&texWalker, "assets/textures/sv_walker.png"},
        {&texRunner, "assets/textures/sv_runner.png"},
        {&texBrute, "assets/textures/sv_brute.png"},
        {&texBullet, "assets/textures/sv_bullet.png"},
        {&texGem, "assets/textures/sv_gem.png"},
    }};
    for (const auto& [tex, path] : texList)
    {
        if (!loadFailed && !tex->loadFromFile(path))
            fail(path);
    }

    sf::SoundBuffer bufShoot, bufHit, bufPickup, bufLevelup;
    const std::array<std::pair<sf::SoundBuffer*, const char*>, 4> sndList{{
        {&bufShoot, "assets/sfx/shoot.wav"},
        {&bufHit, "assets/sfx/hit.wav"},
        {&bufPickup, "assets/sfx/pickup.wav"},
        {&bufLevelup, "assets/sfx/levelup.wav"},
    }};
    for (const auto& [buf, path] : sndList)
    {
        if (!loadFailed && !buf->loadFromFile(path))
            fail(path);
    }

    sf::Music music;
    if (!loadFailed && !music.openFromFile("assets/music/bgm_survivor.ogg"))
        fail("assets/music/bgm_survivor.ogg");

    if (loadFailed)
        return 1;

    music.setLooping(true);
    music.setVolume(40.f);

    SoundPool sfxShoot(bufShoot, 6);
    SoundPool sfxHit(bufHit, 8);
    SoundPool sfxPickup(bufPickup, 8);
    SoundPool sfxLevelup(bufLevelup, 2);

    std::array<const sf::Texture*, 3> enemyTex{&texWalker, &texRunner, &texBrute};

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "LAST STAND");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::View worldView({VIEW_W / 2.f, VIEW_H / 2.f}, {VIEW_W, VIEW_H});
    sf::View uiView({VIEW_W / 2.f, VIEW_H / 2.f}, {VIEW_W, VIEW_H});
    {
        const sf::FloatRect vp = letterboxViewport(window.getSize());
        worldView.setViewport(vp);
        uiView.setViewport(vp);
    }

    std::mt19937 rng(std::random_device{}());

    // 정적 배경 격자
    std::vector<sf::Vertex> gridVerts;
    for (float x = 120.f; x < WORLD_W; x += 120.f)
        addRect(gridVerts, x, 0.f, 1.f, WORLD_H, COL_GRID);
    for (float y = 120.f; y < WORLD_H; y += 120.f)
        addRect(gridVerts, 0.f, y, WORLD_W, 1.f, COL_GRID);

    sf::RectangleShape worldBg({WORLD_W, WORLD_H});
    worldBg.setFillColor(COL_BG);
    sf::RectangleShape worldBorder({WORLD_W, WORLD_H});
    worldBorder.setFillColor(sf::Color::Transparent);
    worldBorder.setOutlineColor(COL_BORDER);
    worldBorder.setOutlineThickness(-2.f);

    sf::RectangleShape screenBg({VIEW_W, VIEW_H});
    screenBg.setFillColor(COL_BG);

    sf::Text text(font, "", 16);
    text.setFillColor(COL_TEXT);

    std::vector<sf::Vertex>                gemVerts, bulletVerts, playerVerts;
    std::array<std::vector<sf::Vertex>, 3> enemyVerts;
    gemVerts.reserve(6 * 600);
    bulletVerts.reserve(6 * 64);
    for (auto& ev : enemyVerts)
        ev.reserve(6 * (MAX_ENEMIES + 8));

    Screen screen     = Screen::Title;
    float  titleTimer = 0.f;
    Game   game;

    std::vector<Act>     actions;
    std::array<bool, 8>  held{};
    std::array<bool, 8>  latched{};

    const auto makeCards = [&]()
    {
        std::array<int, 6> idx{0, 1, 2, 3, 4, 5};
        std::shuffle(idx.begin(), idx.end(), rng);
        game.cards = {idx[0], idx[1], idx[2]};
    };

    const auto startPlay = [&]()
    {
        game   = Game();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    const auto endGame = [&](bool won)
    {
        game.won = won;
        screen   = Screen::Result;
        music.stop();
    };

    const auto applyCard = [&](int slot)
    {
        switch (game.cards[static_cast<std::size_t>(slot)])
        {
            case 0: game.fireInterval *= 0.85f; break;
            case 1: game.damage += 5; break;
            case 2: game.projectiles += 1; break;
            case 3: game.speed *= 1.1f; break;
            case 4:
                game.maxHp += 20.f;
                game.hp = std::min(game.maxHp, game.hp + 20.f);
                break;
            case 5: game.magnet += 40.f; break;
            default: break;
        }
        game.pendingLevels--;
        if (game.pendingLevels > 0)
            makeCards();
        else
            game.choosing = false;
    };

    const auto spawnEnemy = [&]()
    {
        const sf::Vector2f cam = cameraCenter(game.playerPos);
        const float        L   = cam.x - VIEW_W / 2.f - 64.f;
        const float        R   = cam.x + VIEW_W / 2.f + 64.f;
        const float        T   = cam.y - VIEW_H / 2.f - 64.f;
        const float        B   = cam.y + VIEW_H / 2.f + 64.f;

        struct Seg
        {
            sf::Vector2f a, b;
            float        len;
        };
        std::vector<Seg> segs;
        const float      x0 = std::max(L, 0.f), x1 = std::min(R, WORLD_W);
        const float      y0 = std::max(T, 0.f), y1 = std::min(B, WORLD_H);
        if (T >= 0.f && T <= WORLD_H && x1 > x0)
            segs.push_back({{x0, T}, {x1, T}, x1 - x0});
        if (B >= 0.f && B <= WORLD_H && x1 > x0)
            segs.push_back({{x0, B}, {x1, B}, x1 - x0});
        if (L >= 0.f && L <= WORLD_W && y1 > y0)
            segs.push_back({{L, y0}, {L, y1}, y1 - y0});
        if (R >= 0.f && R <= WORLD_W && y1 > y0)
            segs.push_back({{R, y0}, {R, y1}, y1 - y0});
        if (segs.empty())
            return;

        float total = 0.f;
        for (const auto& s : segs)
            total += s.len;
        float pick = std::uniform_real_distribution<float>(0.f, total)(rng);
        sf::Vector2f pos = segs.back().b;
        for (const auto& s : segs)
        {
            if (pick <= s.len)
            {
                const float t = s.len > 0.f ? pick / s.len : 0.f;
                pos           = s.a + (s.b - s.a) * t;
                break;
            }
            pick -= s.len;
        }

        const float roll = std::uniform_real_distribution<float>(0.f, 1.f)(rng);
        int         type = 0;
        if (game.playTime >= 120.f)
            type = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
        else if (game.playTime >= 60.f)
            type = roll < 0.7f ? 0 : 1;

        const EnemyDef& def = ENEMY_DEFS[static_cast<std::size_t>(type)];
        Enemy           e;
        e.pos.x = std::clamp(pos.x, def.radius, WORLD_W - def.radius);
        e.pos.y = std::clamp(pos.y, def.radius, WORLD_H - def.radius);
        e.type  = type;
        e.hp    = def.hp;
        game.enemies.push_back(e);
    };

    const auto updatePlay = [&](float dt)
    {
        game.playTime += dt;

        // 플레이어 이동
        const auto dirHeld = [&](int a, int b)
        { return held[static_cast<std::size_t>(a)] || held[static_cast<std::size_t>(b)] ||
                 latched[static_cast<std::size_t>(a)] || latched[static_cast<std::size_t>(b)]; };
        float dx = 0.f, dy = 0.f;
        if (dirHeld(0, 1))
            dx -= 1.f;
        if (dirHeld(2, 3))
            dx += 1.f;
        if (dirHeld(4, 5))
            dy -= 1.f;
        if (dirHeld(6, 7))
            dy += 1.f;
        const bool moving = (dx != 0.f || dy != 0.f);
        if (moving)
        {
            const float len = std::sqrt(dx * dx + dy * dy);
            game.playerPos.x += dx / len * game.speed * dt;
            game.playerPos.y += dy / len * game.speed * dt;
            game.playerPos.x = std::clamp(game.playerPos.x, PLAYER_R, WORLD_W - PLAYER_R);
            game.playerPos.y = std::clamp(game.playerPos.y, PLAYER_R, WORLD_H - PLAYER_R);
            if (dx < 0.f)
                game.facingLeft = true;
            else if (dx > 0.f)
                game.facingLeft = false;
            game.animTimer += dt;
            game.animFrame = static_cast<int>(game.animTimer / 0.15f) % 2;
        }
        else
        {
            game.animTimer = 0.f;
            game.animFrame = 0;
        }

        // 적 생성
        while (game.playTime >= game.nextSpawn)
        {
            if (static_cast<int>(game.enemies.size()) < MAX_ENEMIES)
                spawnEnemy();
            const float interval =
                std::max(0.15f, 0.6f - 0.05f * std::floor(game.nextSpawn / 15.f));
            game.nextSpawn += interval;
        }

        // 적 이동
        for (auto& e : game.enemies)
        {
            if (e.dead)
            {
                e.deathTimer -= dt;
                continue;
            }
            const EnemyDef& def = ENEMY_DEFS[static_cast<std::size_t>(e.type)];
            if (e.flash > 0.f)
                e.flash -= dt;
            if (e.knock > 0.f)
            {
                const float step = std::min(e.knock, dt);
                e.pos += e.knockDir * (KNOCK_SPEED * step);
                e.knock -= dt;
            }
            else
            {
                const sf::Vector2f d   = game.playerPos - e.pos;
                const float        len = std::sqrt(d.x * d.x + d.y * d.y);
                if (len > 0.0001f)
                    e.pos += d / len * (def.speed * dt);
            }
            e.pos.x = std::clamp(e.pos.x, def.radius, WORLD_W - def.radius);
            e.pos.y = std::clamp(e.pos.y, def.radius, WORLD_H - def.radius);
        }

        // 총알 이동과 충돌
        for (auto& b : game.bullets)
        {
            b.life -= dt;
            if (b.life <= 0.f)
            {
                b.gone = true;
                continue;
            }
            b.pos += b.vel * dt;
            for (auto& e : game.enemies)
            {
                if (e.dead)
                    continue;
                const EnemyDef& def = ENEMY_DEFS[static_cast<std::size_t>(e.type)];
                const float     rr  = BULLET_R + def.radius;
                if (dist2(b.pos, e.pos) < rr * rr)
                {
                    b.gone = true;
                    e.hp -= static_cast<float>(game.damage);
                    e.flash = FLASH_TIME;
                    if (e.hp <= 0.f)
                    {
                        e.dead       = true;
                        e.deathTimer = DEATH_TIME;
                        e.knock      = 0.f;
                        Gem g;
                        g.pos   = e.pos;
                        g.value = def.gemValue;
                        game.gems.push_back(g);
                        sfxHit.play();
                        game.kills++;
                    }
                    else
                    {
                        const float vl = std::sqrt(b.vel.x * b.vel.x + b.vel.y * b.vel.y);
                        e.knockDir     = vl > 0.f ? b.vel / vl : sf::Vector2f{1.f, 0.f};
                        e.knock        = KNOCK_TIME;
                    }
                    break;
                }
            }
        }

        // 자동 사격
        game.shootCd -= dt;
        if (game.shootCd <= 0.f)
        {
            const Enemy* target = nullptr;
            float        best   = FIRE_RANGE * FIRE_RANGE;
            for (const auto& e : game.enemies)
            {
                if (e.dead)
                    continue;
                const float d = dist2(e.pos, game.playerPos);
                if (d <= best)
                {
                    best   = d;
                    target = &e;
                }
            }
            if (target)
            {
                const sf::Vector2f d    = target->pos - game.playerPos;
                const float        base = std::atan2(d.y, d.x);
                const int          n    = game.projectiles;
                for (int i = 0; i < n; ++i)
                {
                    const float off = (static_cast<float>(i) - static_cast<float>(n - 1) / 2.f) *
                                      (10.f * PI / 180.f);
                    const float a = base + off;
                    Bullet      b;
                    b.pos = game.playerPos;
                    b.vel = {std::cos(a) * BULLET_SPEED, std::sin(a) * BULLET_SPEED};
                    game.bullets.push_back(b);
                }
                sfxShoot.play();
                game.shootCd += game.fireInterval;
                if (game.shootCd < 0.f)
                    game.shootCd = 0.f;
            }
            else
            {
                game.shootCd = 0.f;
            }
        }

        // 접촉 피해
        game.hurt = false;
        for (const auto& e : game.enemies)
        {
            if (e.dead)
                continue;
            const EnemyDef& def = ENEMY_DEFS[static_cast<std::size_t>(e.type)];
            const float     rr  = PLAYER_R + def.radius;
            if (dist2(e.pos, game.playerPos) < rr * rr)
            {
                game.hp -= def.dps * dt;
                game.hurt = true;
            }
        }

        // 보석
        bool picked = false;
        for (auto& g : game.gems)
        {
            g.life -= dt;
            if (g.life <= 0.f)
            {
                g.gone = true;
                continue;
            }
            sf::Vector2f d   = game.playerPos - g.pos;
            float        len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len <= game.magnet && len > 0.0001f)
            {
                const float step = std::min(GEM_SPEED * dt, len);
                g.pos += d / len * step;
                d   = game.playerPos - g.pos;
                len = std::sqrt(d.x * d.x + d.y * d.y);
            }
            if (len < PLAYER_R + GEM_R)
            {
                g.gone = true;
                game.xp += g.value;
                picked = true;
            }
        }
        if (picked)
            sfxPickup.play();

        // 삭제
        std::erase_if(game.enemies, [](const Enemy& e) { return e.dead && e.deathTimer <= 0.f; });
        std::erase_if(game.bullets, [](const Bullet& b) { return b.gone; });
        std::erase_if(game.gems, [](const Gem& g) { return g.gone; });

        // 레벨업
        bool leveled = false;
        while (game.xp >= 5 * game.level)
        {
            game.xp -= 5 * game.level;
            game.level++;
            game.pendingLevels++;
            leveled = true;
        }
        if (leveled)
            sfxLevelup.play();

        // 승패
        if (game.hp <= 0.f)
        {
            game.hp = 0.f;
            endGame(false);
            return;
        }
        if (game.playTime >= WIN_TIME)
        {
            game.playTime = WIN_TIME;
            endGame(true);
            return;
        }

        if (game.pendingLevels > 0 && !game.choosing)
        {
            makeCards();
            game.choosing = true;
        }
    };

    const auto tick = [&](float dt)
    {
        for (const Act act : actions)
        {
            switch (screen)
            {
                case Screen::Title:
                    if (act == Act::Enter)
                        startPlay();
                    break;
                case Screen::Play:
                    if (act == Act::Pause && !game.choosing)
                        game.paused = !game.paused;
                    else if ((act == Act::Card1 || act == Act::Card2 || act == Act::Card3) &&
                             game.choosing && !game.paused)
                        applyCard(static_cast<int>(act) - static_cast<int>(Act::Card1));
                    break;
                case Screen::Result:
                    if (act == Act::Restart)
                        startPlay();
                    break;
            }
        }
        actions.clear();

        if (screen == Screen::Title)
        {
            titleTimer += dt;
            if (titleTimer >= 2.f)
                startPlay();
        }
        else if (screen == Screen::Play)
        {
            if (!game.paused && !game.choosing)
                updatePlay(dt);
        }
        latched.fill(false);
    };

    const auto drawText = [&](const std::string& s, unsigned int size, sf::Vector2f pos, float ax, float ay)
    {
        text.setString(s);
        text.setCharacterSize(size);
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({std::round(b.position.x + b.size.x * ax), std::round(b.position.y + b.size.y * ay)});
        text.setPosition({std::round(pos.x), std::round(pos.y)});
        window.draw(text);
    };

    const auto drawBatch = [&](const std::vector<sf::Vertex>& v, const sf::Texture* tex)
    {
        if (v.empty())
            return;
        sf::RenderStates st;
        st.texture = tex;
        window.draw(v.data(), v.size(), sf::PrimitiveType::Triangles, st);
    };

    const auto drawWorld = [&]()
    {
        const sf::Vector2f cam = cameraCenter(game.playerPos);
        worldView.setCenter({std::round(cam.x), std::round(cam.y)});
        window.setView(worldView);

        window.draw(worldBg);
        drawBatch(gridVerts, nullptr);
        window.draw(worldBorder);

        gemVerts.clear();
        for (const auto& g : game.gems)
        {
            const sf::Vector2f p{std::round(g.pos.x), std::round(g.pos.y)};
            addQuad(gemVerts, p, 12.f, 12.f, 0.f, 0.f, false, g.value >= 5 ? COL_GEM5 : COL_GEM1);
        }
        drawBatch(gemVerts, &texGem);

        for (auto& ev : enemyVerts)
            ev.clear();
        std::array<int, 3> frames{};
        for (std::size_t t = 0; t < 3; ++t)
            frames[t] = static_cast<int>(std::floor(game.playTime / ENEMY_DEFS[t].animPeriod)) % 2;
        for (const auto& e : game.enemies)
        {
            const auto      t     = static_cast<std::size_t>(e.type);
            const EnemyDef& def   = ENEMY_DEFS[t];
            const int       frame = (e.dead || e.flash > 0.f) ? 2 : frames[t];
            const bool      flip  = game.playerPos.x < e.pos.x;
            const sf::Vector2f p{std::round(e.pos.x), std::round(e.pos.y)};
            addQuad(enemyVerts[t], p, def.cell, def.cell, def.cell * static_cast<float>(frame), 0.f, flip,
                    sf::Color::White);
        }
        for (std::size_t t = 0; t < 3; ++t)
            drawBatch(enemyVerts[t], enemyTex[t]);

        bulletVerts.clear();
        for (const auto& b : game.bullets)
        {
            const sf::Vector2f p{std::round(b.pos.x), std::round(b.pos.y)};
            addQuad(bulletVerts, p, 8.f, 8.f, 0.f, 0.f, false, sf::Color::White);
        }
        drawBatch(bulletVerts, &texBullet);

        playerVerts.clear();
        const sf::Vector2f pp{std::round(game.playerPos.x), std::round(game.playerPos.y)};
        addQuad(playerVerts, pp, 32.f, 32.f, 32.f * static_cast<float>(game.animFrame), 0.f, game.facingLeft,
                game.hurt ? COL_HURT : sf::Color::White);
        drawBatch(playerVerts, &texPlayer);
    };

    const auto drawHud = [&]()
    {
        window.setView(uiView);

        sf::RectangleShape r;
        r.setSize({VIEW_W, 6.f});
        r.setPosition({0.f, 0.f});
        r.setFillColor(COL_XP_BG);
        window.draw(r);
        const float need = static_cast<float>(5 * game.level);
        const float xpFrac = std::clamp(static_cast<float>(game.xp) / need, 0.f, 1.f);
        r.setSize({VIEW_W * xpFrac, 6.f});
        r.setFillColor(COL_XP_FILL);
        window.draw(r);

        r.setPosition({16.f, 16.f});
        r.setSize({200.f, 12.f});
        r.setFillColor(COL_HP_BG);
        window.draw(r);
        const float hpFrac = std::clamp(game.hp / game.maxHp, 0.f, 1.f);
        r.setSize({200.f * hpFrac, 12.f});
        r.setFillColor(COL_HP_FILL);
        window.draw(r);

        const int hpShown = static_cast<int>(std::ceil(std::max(0.f, game.hp) - 1e-4f));
        drawText("HP " + std::to_string(hpShown), 16, {228.f, 22.f}, 0.f, 0.5f);

        drawText("LV " + std::to_string(game.level), 16, {944.f, 16.f}, 1.f, 0.f);
        drawText("TIME " + formatTime(game.playTime), 16, {944.f, 38.f}, 1.f, 0.f);
        drawText("KILLS " + std::to_string(game.kills), 16, {944.f, 60.f}, 1.f, 0.f);

        if (game.choosing)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float        x = 80.f + 280.f * static_cast<float>(i);
                sf::RectangleShape card({240.f, 140.f});
                card.setPosition({x, 200.f});
                card.setFillColor(COL_CARD_BG);
                card.setOutlineColor(COL_CARD_EDGE);
                card.setOutlineThickness(-2.f);
                window.draw(card);
                drawText(std::to_string(i + 1), 32, {x + 120.f, 220.f}, 0.5f, 0.f);
                drawText(CARD_NAMES[static_cast<std::size_t>(game.cards[static_cast<std::size_t>(i)])], 16,
                         {x + 120.f, 290.f}, 0.5f, 0.5f);
            }
        }

        if (game.paused)
            drawText("PAUSED", 48, {VIEW_W / 2.f, VIEW_H / 2.f}, 0.5f, 0.5f);
    };

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
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                const sf::FloatRect vp = letterboxViewport(resized->size);
                worldView.setViewport(vp);
                uiView.setViewport(vp);
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                held.fill(false);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                using S = sf::Keyboard::Scan;
                using K = sf::Keyboard::Key;
                if (keyIs(*key, S::Escape, K::Escape))
                {
                    window.close();
                    continue;
                }
                const int mi = moveIndex(key->scancode, key->code);
                if (mi >= 0)
                {
                    held[static_cast<std::size_t>(mi)]    = true;
                    latched[static_cast<std::size_t>(mi)] = true;
                }
                if (keyIs(*key, S::Enter, K::Enter) || key->scancode == S::NumpadEnter)
                    actions.push_back(Act::Enter);
                else if (keyIs(*key, S::P, K::P))
                    actions.push_back(Act::Pause);
                else if (keyIs(*key, S::R, K::R))
                    actions.push_back(Act::Restart);
                else if (keyIs(*key, S::Num1, K::Num1) || keyIs(*key, S::Numpad1, K::Numpad1))
                    actions.push_back(Act::Card1);
                else if (keyIs(*key, S::Num2, K::Num2) || keyIs(*key, S::Numpad2, K::Numpad2))
                    actions.push_back(Act::Card2);
                else if (keyIs(*key, S::Num3, K::Num3) || keyIs(*key, S::Numpad3, K::Numpad3))
                    actions.push_back(Act::Card3);
            }
            else if (const auto* rel = event->getIf<sf::Event::KeyReleased>())
            {
                const int mi = moveIndex(rel->scancode, rel->code);
                if (mi >= 0)
                    held[static_cast<std::size_t>(mi)] = false;
            }
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                {
                    const sf::Vector2f p = window.mapPixelToCoords(mb->position, uiView);
                    for (int i = 0; i < 3; ++i)
                    {
                        const sf::FloatRect rect({80.f + 280.f * static_cast<float>(i), 200.f}, {240.f, 140.f});
                        if (rect.contains(p))
                        {
                            actions.push_back(static_cast<Act>(static_cast<int>(Act::Card1) + i));
                            break;
                        }
                    }
                }
            }
        }
        if (!window.isOpen())
            break;

        accumulator += clock.restart().asSeconds();
        if (accumulator > 0.25f)
            accumulator = 0.25f;
        while (accumulator >= DT)
        {
            tick(DT);
            accumulator -= DT;
        }

        window.clear(sf::Color::Black);
        if (screen == Screen::Title)
        {
            window.setView(uiView);
            window.draw(screenBg);
            drawText("LAST STAND", 48, {VIEW_W / 2.f, 220.f}, 0.5f, 0.5f);
            drawText("PRESS ENTER", 16, {VIEW_W / 2.f, 320.f}, 0.5f, 0.5f);
        }
        else if (screen == Screen::Play)
        {
            drawWorld();
            drawHud();
        }
        else
        {
            window.setView(uiView);
            window.draw(screenBg);
            drawText(game.won ? "SURVIVED" : "GAME OVER", 48, {VIEW_W / 2.f, 170.f}, 0.5f, 0.5f);
            drawText("TIME " + formatTime(game.playTime), 32, {VIEW_W / 2.f, 250.f}, 0.5f, 0.5f);
            drawText("LEVEL " + std::to_string(game.level), 32, {VIEW_W / 2.f, 300.f}, 0.5f, 0.5f);
            drawText("KILLS " + std::to_string(game.kills), 32, {VIEW_W / 2.f, 350.f}, 0.5f, 0.5f);
            drawText("R: RESTART  ESC: QUIT", 16, {VIEW_W / 2.f, 430.f}, 0.5f, 0.5f);
        }
        window.display();
    }

    music.stop();
    return 0;
}
} // namespace

int main()
{
    try
    {
        return run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
