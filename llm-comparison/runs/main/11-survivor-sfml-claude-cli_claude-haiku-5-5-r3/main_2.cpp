#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <list>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
constexpr float kW = 960.f;
constexpr float kH = 540.f;
constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kPi = 3.14159265358979f;
constexpr float kPlayerR = 14.f;
constexpr float kBulletR = 4.f;
constexpr float kGemR = 6.f;
constexpr float kPlayTime = 300.f;
constexpr std::size_t kMaxEnemies = 400;
constexpr std::size_t kMaxSfx = 64;

std::mt19937 rng{std::random_device{}()};

float rnd(float a, float b)
{
    return std::uniform_real_distribution<float>(a, b)(rng);
}

sf::Color hex(unsigned v)
{
    return sf::Color((v >> 16) & 0xFFu, (v >> 8) & 0xFFu, v & 0xFFu);
}

sf::Vector2f norm(sf::Vector2f v)
{
    const float l = v.length();
    return l > 1e-6f ? v / l : sf::Vector2f{0.f, 0.f};
}

sf::Vector2f clampWorld(sf::Vector2f p, float m)
{
    return {std::clamp(p.x, m, kWorldW - m), std::clamp(p.y, m, kWorldH - m)};
}

sf::Vector2f camCenter(sf::Vector2f p)
{
    return {std::clamp(p.x, kW / 2.f, kWorldW - kW / 2.f),
            std::clamp(p.y, kH / 2.f, kWorldH - kH / 2.f)};
}

// 960x540 논리 영역을 16:9로 맞추고 남는 부분은 검은 띠로 둔다.
sf::View makeView(const sf::RenderWindow& win, sf::Vector2f center)
{
    const sf::Vector2u s = win.getSize();
    const float sx = static_cast<float>(std::max(s.x, 1u));
    const float sy = static_cast<float>(std::max(s.y, 1u));
    const float sc = std::min(sx / kW, sy / kH);
    const float vw = kW * sc / sx;
    const float vh = kH * sc / sy;
    sf::View v(sf::FloatRect({center.x - kW / 2.f, center.y - kH / 2.f}, {kW, kH}));
    v.setViewport(sf::FloatRect({(1.f - vw) / 2.f, (1.f - vh) / 2.f}, {vw, vh}));
    return v;
}

float segDist(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b)
{
    const sf::Vector2f ab = b - a;
    const float l2 = ab.lengthSquared();
    const float t = l2 > 0.f
        ? std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2, 0.f, 1.f)
        : 0.f;
    return (a + ab * t - p).length();
}

struct Bullet
{
    sf::Vector2f pos{};
    sf::Vector2f vel{};
    float age = 0.f;
    float dmg = 10.f;
    bool dead = false;
};

struct Gem
{
    sf::Vector2f pos{};
    int value = 1;
    float age = 0.f;
    bool taken = false;
};

struct EnemyInfo
{
    float radius;
    float hp;
    float speed;
    float dps;
    int gem;
    float period;
    int cell;
};

constexpr std::array<EnemyInfo, 3> kEI{{
    {10.f, 10.f, 80.f, 10.f, 1, 0.3f, 24},
    {10.f, 6.f, 140.f, 6.f, 1, 0.15f, 24},
    {20.f, 60.f, 50.f, 20.f, 5, 0.4f, 48},
}};

struct Enemy
{
    sf::Vector2f pos{};
    int type = 0;
    float hp = 0.f;
    float knockT = 0.f;
    sf::Vector2f kv{};
    float flashT = 0.f;
    float dyingT = 0.f;
    bool dying = false;
};

constexpr std::array<const char*, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40"};

struct Game
{
    enum class Scr { Title, Play, Result };

    Scr scr = Scr::Title;
    float titleT = 0.f;
    bool paused = false;
    bool cardsOpen = false;
    bool won = false;
    bool hurt = false;
    bool moving = false;
    bool faceLeft = false;

    float t = 0.f;
    float nextSpawn = 0.5f;
    float fireT = 0.f;
    float playerAnim = 0.f;

    sf::Vector2f player{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    float speed = 200.f;
    float fireInt = 0.8f;
    float dmg = 10.f;
    float magnet = 60.f;
    int proj = 1;
    int xp = 0;
    int level = 1;
    int pendingLv = 0;
    int kills = 0;
    std::array<int, 3> cards{};

    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};

struct Res
{
    sf::Font font;
    sf::Texture tPlayer, tWalker, tRunner, tBrute, tBullet, tGem;
    sf::SoundBuffer bShoot, bHit, bPickup, bLevel;
    sf::Music bgm;
    std::list<sf::Sound> sfx; // 버퍼보다 나중에 선언해서 먼저 파괴되게 한다.
};

template <class T>
bool loadRes(T& r, const char* path)
{
    bool ok;
    if constexpr (std::is_same_v<T, sf::Font> || std::is_same_v<T, sf::Music>)
        ok = r.openFromFile(path);
    else
        ok = r.loadFromFile(path);
    if (!ok)
        std::cerr << "failed to load: " << path << '\n';
    return ok;
}

void playSfx(Res& res, const sf::SoundBuffer& b)
{
    if (res.sfx.size() >= kMaxSfx)
        return;
    res.sfx.emplace_back(b);
    res.sfx.back().play();
}

void drawFrame(sf::RenderTarget& t, const sf::Texture& tex, int idx, int cell,
               sf::Vector2f pos, bool flip, sf::Color col)
{
    sf::Sprite s(tex);
    s.setTextureRect(sf::IntRect({idx * cell, 0}, {cell, cell}));
    s.setOrigin({cell / 2.f, cell / 2.f});
    s.setPosition(pos);
    s.setScale({flip ? -1.f : 1.f, 1.f});
    s.setColor(col);
    t.draw(s);
}

// align: 0 왼쪽, 1 가운데, 2 오른쪽. pos는 글자 상자의 위쪽 기준점.
void drawText(sf::RenderTarget& t, const sf::Font& f, const std::string& s, unsigned size,
              sf::Vector2f pos, sf::Color col, int align = 0)
{
    sf::Text text(f, sf::String(s), size);
    text.setFillColor(col);
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x * align * 0.5f, b.position.y});
    text.setPosition(pos);
    t.draw(text);
}

std::string fmtTime(float t)
{
    const int s = std::max(0, static_cast<int>(t));
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

int keyIndex(sf::Keyboard::Key k)
{
    using K = sf::Keyboard::Key;
    if (k == K::Num1) return 0;
    if (k == K::Num2) return 1;
    if (k == K::Num3) return 2;
    return -1;
}

void spawnEnemy(Game& g)
{
    const sf::Vector2f c = camCenter(g.player);
    const float L = c.x - kW / 2.f - 64.f;
    const float R = c.x + kW / 2.f + 64.f;
    const float T = c.y - kH / 2.f - 64.f;
    const float B = c.y + kH / 2.f + 64.f;
    const float w = R - L;
    const float h = B - T;

    const float s = rnd(0.f, 2.f * w + 2.f * h);
    sf::Vector2f p;
    if (s < w)
        p = {L + s, T};
    else if (s < 2.f * w)
        p = {L + s - w, B};
    else if (s < 2.f * w + h)
        p = {L, T + s - 2.f * w};
    else
        p = {R, T + s - 2.f * w - h};
    p = clampWorld(p, 0.f);

    int type = 0;
    if (g.t >= 120.f)
    {
        const float r = rnd(0.f, 1.f);
        type = r < 0.6f ? 0 : (r < 0.9f ? 1 : 2);
    }
    else if (g.t >= 60.f)
    {
        type = rnd(0.f, 1.f) < 0.7f ? 0 : 1;
    }

    Enemy e;
    e.pos = p;
    e.type = type;
    e.hp = kEI[type].hp;
    g.enemies.push_back(e);
}

void rollCards(Game& g)
{
    std::array<int, 6> idx{0, 1, 2, 3, 4, 5};
    std::shuffle(idx.begin(), idx.end(), rng);
    for (int i = 0; i < 3; ++i)
        g.cards[i] = idx[i];
    g.cardsOpen = true;
}

void applyCard(Game& g, int c)
{
    switch (c)
    {
    case 0: g.fireInt *= 0.85f; break;
    case 1: g.dmg += 5.f; break;
    case 2: g.proj += 1; break;
    case 3: g.speed *= 1.1f; break;
    case 4: g.maxHp += 20.f; g.hp += 20.f; break;
    case 5: g.magnet += 40.f; break;
    default: break;
    }
}

void pickCard(Game& g, int i)
{
    applyCard(g, g.cards[i]);
    --g.pendingLv;
    if (g.pendingLv > 0)
        rollCards(g);
    else
        g.cardsOpen = false;
}

void updatePlay(Game& g, Res& res, float dt)
{
    g.hurt = false;
    g.t += dt;

    // 이동
    sf::Vector2f d{0.f, 0.f};
    using K = sf::Keyboard::Key;
    if (sf::Keyboard::isKeyPressed(K::A) || sf::Keyboard::isKeyPressed(K::Left)) d.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(K::D) || sf::Keyboard::isKeyPressed(K::Right)) d.x += 1.f;
    if (sf::Keyboard::isKeyPressed(K::W) || sf::Keyboard::isKeyPressed(K::Up)) d.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(K::S) || sf::Keyboard::isKeyPressed(K::Down)) d.y += 1.f;
    const float dl = d.length();
    g.moving = dl > 0.f;
    if (g.moving)
    {
        d = d / dl;
        g.playerAnim += dt;
    }
    if (d.x < 0.f) g.faceLeft = true;
    else if (d.x > 0.f) g.faceLeft = false;
    g.player = clampWorld(g.player + d * (g.speed * dt), 0.f);

    // 적 생성
    if (g.t >= g.nextSpawn && g.enemies.size() < kMaxEnemies)
    {
        spawnEnemy(g);
        g.nextSpawn = g.t + std::max(0.15f, 0.6f - 0.05f * std::floor(g.t / 15.f));
    }

    // 자동 사격
    g.fireT += dt;
    if (g.fireT >= g.fireInt)
    {
        const Enemy* best = nullptr;
        float bd = 400.f * 400.f;
        for (const Enemy& e : g.enemies)
        {
            if (e.dying) continue;
            const float d2 = (e.pos - g.player).lengthSquared();
            if (d2 <= bd)
            {
                bd = d2;
                best = &e;
            }
        }
        if (best)
        {
            const float base = std::atan2(best->pos.y - g.player.y, best->pos.x - g.player.x);
            for (int i = 0; i < g.proj; ++i)
            {
                const float a = base + (i - (g.proj - 1) * 0.5f) * (10.f * kPi / 180.f);
                Bullet b;
                b.pos = g.player;
                b.vel = {500.f * std::cos(a), 500.f * std::sin(a)};
                b.dmg = g.dmg;
                g.bullets.push_back(b);
            }
            playSfx(res, res.bShoot);
            g.fireT = 0.f;
        }
        else
        {
            g.fireT = g.fireInt; // 적이 나타나면 바로 쏠 수 있게 대기
        }
    }

    // 탄환 이동과 충돌 (선분 검사로 관통 방지)
    for (Bullet& b : g.bullets)
    {
        if (b.dead) continue;
        const sf::Vector2f prev = b.pos;
        b.pos = b.pos + b.vel * dt;
        b.age += dt;
        if (b.age >= 1.2f)
        {
            b.dead = true;
            continue;
        }
        for (Enemy& e : g.enemies)
        {
            if (e.dying) continue;
            const EnemyInfo& in = kEI[e.type];
            if (segDist(e.pos, prev, b.pos) > in.radius + kBulletR) continue;

            e.hp -= b.dmg;
            b.dead = true;
            if (e.hp <= 0.f)
            {
                e.dying = true;
                e.dyingT = 0.1f;
                Gem gm;
                gm.pos = e.pos;
                gm.value = in.gem;
                g.gems.push_back(gm);
                ++g.kills;
                playSfx(res, res.bHit);
            }
            else
            {
                e.flashT = 0.1f;
                e.knockT = 0.1f;
                e.kv = norm(b.vel) * 120.f;
            }
            break;
        }
    }

    // 적 이동과 접촉 피해
    for (Enemy& e : g.enemies)
    {
        const EnemyInfo& in = kEI[e.type];
        if (e.dying)
        {
            e.dyingT -= dt;
            continue;
        }
        e.flashT = std::max(0.f, e.flashT - dt);
        if (e.knockT > 0.f)
        {
            e.knockT -= dt;
            e.pos = e.pos + e.kv * dt;
        }
        else
        {
            e.pos = e.pos + norm(g.player - e.pos) * (in.speed * dt);
        }
        e.pos = clampWorld(e.pos, in.radius);

        if ((g.player - e.pos).length() < in.radius + kPlayerR)
        {
            g.hp -= in.dps * dt;
            g.hurt = true;
        }
    }

    // 보석 자석과 획득
    for (Gem& gm : g.gems)
    {
        gm.age += dt;
        const sf::Vector2f d2 = g.player - gm.pos;
        if (d2.length() < g.magnet)
            gm.pos = gm.pos + norm(d2) * (300.f * dt);
        if ((g.player - gm.pos).length() <= kPlayerR + kGemR)
        {
            gm.taken = true;
            g.xp += gm.value;
            playSfx(res, res.bPickup);
            while (g.xp >= 5 * g.level)
            {
                g.xp -= 5 * g.level;
                ++g.level;
                ++g.pendingLv;
                playSfx(res, res.bLevel);
            }
        }
    }

    // 삭제
    std::erase_if(g.gems, [](const Gem& x) { return x.taken || x.age >= 30.f; });
    std::erase_if(g.enemies, [](const Enemy& e) { return e.dying && e.dyingT <= 0.f; });
    std::erase_if(g.bullets, [](const Bullet& b) { return b.dead; });

    if (g.pendingLv > 0 && !g.cardsOpen)
        rollCards(g);

    // 종료 판정
    if (g.hp <= 0.f || g.t >= kPlayTime)
    {
        g.won = g.hp > 0.f;
        g.scr = Game::Scr::Result;
        res.bgm.pause();
    }
}

void drawWorld(sf::RenderTarget& t, const Game& g, const Res& res)
{
    sf::RectangleShape bg({kWorldW, kWorldH});
    bg.setFillColor(hex(0x1A1420));
    t.draw(bg);

    static const sf::VertexArray grid = [] {
        sf::VertexArray va(sf::PrimitiveType::Lines, 78);
        std::size_t i = 0;
        for (int x = 0; x <= 2880; x += 120, i += 2)
        {
            va[i].position = {static_cast<float>(x), 0.f};
            va[i].color = hex(0x2A2233);
            va[i + 1].position = {static_cast<float>(x), kWorldH};
            va[i + 1].color = hex(0x2A2233);
        }
        for (int y = 0; y < 1620; y += 120, i += 2)
        {
            va[i].position = {0.f, static_cast<float>(y)};
            va[i].color = hex(0x2A2233);
            va[i + 1].position = {kWorldW, static_cast<float>(y)};
            va[i + 1].color = hex(0x2A2233);
        }
        return va;
    }();
    t.draw(grid);

    sf::RectangleShape border({kWorldW, kWorldH});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineThickness(-2.f);
    border.setOutlineColor(hex(0x6B5B7B));
    t.draw(border);

    for (const Gem& gm : g.gems)
    {
        drawFrame(t, res.tGem, 0, 12, gm.pos, false,
                  gm.value >= 5 ? hex(0xFFD54A) : hex(0x4FC3F7));
    }

    for (const Enemy& e : g.enemies)
    {
        const EnemyInfo& in = kEI[e.type];
        const sf::Texture& tex = e.type == 0 ? res.tWalker : (e.type == 1 ? res.tRunner : res.tBrute);
        const int frame = e.flashT > 0.f ? 2 : static_cast<int>(g.t / in.period) % 2;
        drawFrame(t, tex, frame, in.cell, e.pos, g.player.x < e.pos.x, sf::Color::White);
    }

    for (const Bullet& b : g.bullets)
        drawFrame(t, res.tBullet, 0, 8, b.pos, false, sf::Color::White);

    const int pf = g.moving ? static_cast<int>(g.playerAnim / 0.15f) % 2 : 0;
    drawFrame(t, res.tPlayer, pf, 32, g.player, g.faceLeft,
              g.hurt ? hex(0xFF6060) : sf::Color::White);
}

void drawHud(sf::RenderTarget& t, const Game& g, const Res& res)
{
    const sf::Color white = sf::Color::White;

    sf::RectangleShape xpBg({kW, 6.f});
    xpBg.setFillColor(hex(0x2A2233));
    t.draw(xpBg);
    sf::RectangleShape xpFill({kW * static_cast<float>(g.xp) / static_cast<float>(5 * g.level), 6.f});
    xpFill.setFillColor(hex(0x4FC3F7));
    t.draw(xpFill);

    sf::RectangleShape hpBg({200.f, 12.f});
    hpBg.setPosition({12.f, 12.f});
    hpBg.setFillColor(hex(0x3A1F24));
    t.draw(hpBg);
    sf::RectangleShape hpFill({200.f * std::max(0.f, g.hp) / g.maxHp, 12.f});
    hpFill.setPosition({12.f, 12.f});
    hpFill.setFillColor(hex(0xE5533D));
    t.draw(hpFill);

    drawText(t, res.font, "HP " + std::to_string(static_cast<int>(std::ceil(std::max(0.f, g.hp)))),
             16, {222.f, 10.f}, white);
    drawText(t, res.font,
             "LV " + std::to_string(g.level) + "    TIME " + fmtTime(g.t) +
                 "    KILLS " + std::to_string(g.kills),
             16, {948.f, 10.f}, white, 2);

    if (g.cardsOpen)
    {
        sf::RectangleShape dim({kW, kH});
        dim.setFillColor(sf::Color(0, 0, 0, 160));
        t.draw(dim);
        for (int i = 0; i < 3; ++i)
        {
            const float x = 80.f + 280.f * static_cast<float>(i);
            sf::RectangleShape card({240.f, 140.f});
            card.setPosition({x, 200.f});
            card.setFillColor(hex(0x2A2233));
            card.setOutlineThickness(-2.f);
            card.setOutlineColor(hex(0xFFD54A));
            t.draw(card);
            drawText(t, res.font, kCardNames[g.cards[i]], 16, {x + 120.f, 262.f}, white, 1);
            drawText(t, res.font, std::to_string(i + 1), 16, {x + 10.f, 210.f}, white);
        }
    }

    if (g.paused)
    {
        sf::RectangleShape dim({kW, kH});
        dim.setFillColor(sf::Color(0, 0, 0, 120));
        t.draw(dim);
        drawText(t, res.font, "PAUSED", 48, {480.f, 230.f}, white, 1);
    }
}

void drawMenu(sf::RenderTarget& t, const Game& g, const Res& res)
{
    const sf::Color white = sf::Color::White;
    sf::RectangleShape bg({kW, kH});
    bg.setFillColor(hex(0x1A1420));
    t.draw(bg);

    if (g.scr == Game::Scr::Title)
    {
        drawText(t, res.font, "LAST STAND", 48, {480.f, 180.f}, white, 1);
        drawText(t, res.font, "PRESS ENTER", 16, {480.f, 300.f}, white, 1);
    }
    else
    {
        drawText(t, res.font, g.won ? "SURVIVED" : "GAME OVER", 48, {480.f, 100.f}, white, 1);
        drawText(t, res.font, "TIME " + fmtTime(g.t), 32, {480.f, 200.f}, white, 1);
        drawText(t, res.font, "LEVEL " + std::to_string(g.level), 32, {480.f, 260.f}, white, 1);
        drawText(t, res.font, "KILLS " + std::to_string(g.kills), 32, {480.f, 320.f}, white, 1);
        drawText(t, res.font, "R: RESTART  ESC: QUIT", 16, {480.f, 470.f}, white, 1);
    }
}
} // namespace

int main()
{
    Res res;
    bool ok = true;
    ok &= loadRes(res.font, "assets/fonts/game.ttf");
    ok &= loadRes(res.tPlayer, "assets/textures/sv_player.png");
    ok &= loadRes(res.tWalker, "assets/textures/sv_walker.png");
    ok &= loadRes(res.tRunner, "assets/textures/sv_runner.png");
    ok &= loadRes(res.tBrute, "assets/textures/sv_brute.png");
    ok &= loadRes(res.tBullet, "assets/textures/sv_bullet.png");
    ok &= loadRes(res.tGem, "assets/textures/sv_gem.png");
    ok &= loadRes(res.bShoot, "assets/sfx/shoot.wav");
    ok &= loadRes(res.bHit, "assets/sfx/hit.wav");
    ok &= loadRes(res.bPickup, "assets/sfx/pickup.wav");
    ok &= loadRes(res.bLevel, "assets/sfx/levelup.wav");
    ok &= loadRes(res.bgm, "assets/music/bgm_survivor.ogg");
    if (!ok)
        return 1;

    res.bgm.setLooping(true);
    res.bgm.setVolume(40.f);

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "LAST STAND");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    Game g;
    const auto startPlay = [&g, &res] {
        Game fresh;
        fresh.scr = Game::Scr::Play;
        g = fresh;
        res.bgm.stop();
        res.bgm.play();
    };

    sf::Clock clock;
    while (window.isOpen())
    {
        const float dt = std::min(clock.restart().asSeconds(), 0.1f);

        while (const std::optional ev = window.pollEvent())
        {
            if (ev->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }

            if (const auto* kp = ev->getIf<sf::Event::KeyPressed>())
            {
                const auto k = kp->code;
                if (k == sf::Keyboard::Key::Escape)
                {
                    window.close();
                    break;
                }
                if (g.scr == Game::Scr::Title && k == sf::Keyboard::Key::Enter)
                {
                    startPlay();
                }
                else if (g.scr == Game::Scr::Result && k == sf::Keyboard::Key::R)
                {
                    startPlay();
                }
                else if (g.scr == Game::Scr::Play && g.cardsOpen)
                {
                    const int idx = keyIndex(k);
                    if (idx >= 0)
                        pickCard(g, idx);
                }
                else if (g.scr == Game::Scr::Play && k == sf::Keyboard::Key::P)
                {
                    g.paused = !g.paused;
                    if (g.paused)
                        res.bgm.pause();
                    else
                        res.bgm.play();
                }
            }

            if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left && g.scr == Game::Scr::Play && g.cardsOpen)
                {
                    const sf::Vector2f p =
                        window.mapPixelToCoords(mb->position, makeView(window, {kW / 2.f, kH / 2.f}));
                    for (int i = 0; i < 3; ++i)
                    {
                        const float x = 80.f + 280.f * static_cast<float>(i);
                        if (p.x >= x && p.x < x + 240.f && p.y >= 200.f && p.y < 340.f)
                        {
                            pickCard(g, i);
                            break;
                        }
                    }
                }
            }
        }
        if (!window.isOpen())
            break;

        // 갱신
        if (g.scr == Game::Scr::Title)
        {
            g.titleT += dt;
            if (g.titleT >= 2.f)
                startPlay();
        }
        else if (g.scr == Game::Scr::Play && !g.paused && !g.cardsOpen)
        {
            updatePlay(g, res, dt);
        }
        res.sfx.remove_if([](const sf::Sound& s) {
            return s.getStatus() == sf::SoundSource::Status::Stopped;
        });

        // 그리기
        window.clear(sf::Color::Black);
        const sf::View hud = makeView(window, {kW / 2.f, kH / 2.f});
        if (g.scr == Game::Scr::Play)
        {
            window.setView(makeView(window, camCenter(g.player)));
            drawWorld(window, g, res);
            window.setView(hud);
            drawHud(window, g, res);
        }
        else
        {
            window.setView(hud);
            drawMenu(window, g, res);
        }
        window.display();
    }

    return 0;
}
