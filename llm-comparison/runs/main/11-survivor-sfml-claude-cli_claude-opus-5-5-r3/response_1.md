```cpp
// LAST STAND - SFML 3.1 서바이버류 미니게임 (main.cpp 단일 파일)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kViewW  = 960.f;
constexpr float kViewH  = 540.f;
constexpr float kDt     = 1.f / 60.f;
constexpr float kPlayerR = 14.f;
constexpr float kPi = 3.14159265358979f;

sf::Color hex(std::uint32_t v, std::uint8_t a = 255)
{
    return sf::Color(static_cast<std::uint8_t>((v >> 16) & 0xFF), static_cast<std::uint8_t>((v >> 8) & 0xFF),
                     static_cast<std::uint8_t>(v & 0xFF), a);
}

struct EnemyDef
{
    float frame;
    float radius;
    float hp;
    float speed;
    float dps;
    float period;
    int   gem;
};

const std::array<EnemyDef, 3> kDefs = {{
    {24.f, 10.f, 10.f, 80.f, 10.f, 0.3f, 1},  // walker
    {24.f, 10.f, 6.f, 140.f, 6.f, 0.15f, 1},  // runner
    {48.f, 20.f, 60.f, 50.f, 20.f, 0.4f, 5},  // brute
}};

const std::array<const char*, 6> kCardNames = {"FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1",
                                               "SPEED +10%",     "MAX HP +20", "MAGNET +40"};

struct Enemy
{
    sf::Vector2f pos;
    int          type = 0;
    float        hp   = 0.f;
    float        knock = 0.f;
    sf::Vector2f knockDir;
    float        flash = 0.f;
    bool         dying = false;
    float        death = 0.f;
};

struct Bullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float        life = 1.2f;
};

struct Gem
{
    sf::Vector2f pos;
    int          value = 1;
    float        life  = 30.f;
};

struct State
{
    sf::Vector2f player{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f, maxHp = 100.f;
    float speed = 200.f;
    float fireInterval = 0.8f;
    float fireTimer = 0.f;
    int   damage = 10;
    int   projectiles = 1;
    float magnet = 60.f;
    int   level = 1;
    int   xp = 0;
    int   kills = 0;
    float time = 0.f;
    float spawnTimer = 0.5f;
    bool  facingLeft = false;
    float animT = 0.f;
    bool  moving = false;
    bool  hurt = false;
    bool  paused = false;
    bool  choosing = false;
    int   pendingLevels = 0;
    std::array<int, 3> cards{0, 1, 2};
    bool  won = false;
    std::vector<Enemy>  enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem>    gems;
};

struct SfxPool
{
    std::vector<sf::Sound> sounds;
    std::size_t            next = 0;
    SfxPool(const sf::SoundBuffer& buf, int n)
    {
        sounds.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            sounds.emplace_back(buf);
    }
    void play()
    {
        for (auto& s : sounds)
            if (s.getStatus() != sf::Sound::Status::Playing)
            {
                s.play();
                return;
            }
        sounds[next % sounds.size()].play();
        ++next;
    }
};

void addQuad(std::vector<sf::Vertex>& v, sf::Vector2f c, float w, float h, float tx, float ty, bool flip, sf::Color col)
{
    const float l = c.x - w / 2.f, r = c.x + w / 2.f, t = c.y - h / 2.f, b = c.y + h / 2.f;
    float u0 = tx, u1 = tx + w;
    if (flip)
        std::swap(u0, u1);
    const float v0 = ty, v1 = ty + h;
    v.push_back(sf::Vertex{{l, t}, col, {u0, v0}});
    v.push_back(sf::Vertex{{r, t}, col, {u1, v0}});
    v.push_back(sf::Vertex{{l, b}, col, {u0, v1}});
    v.push_back(sf::Vertex{{l, b}, col, {u0, v1}});
    v.push_back(sf::Vertex{{r, t}, col, {u1, v0}});
    v.push_back(sf::Vertex{{r, b}, col, {u1, v1}});
}

std::string mmss(float t)
{
    int s = static_cast<int>(std::floor(std::max(0.f, t)));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    return buf;
}

sf::Vector2f cameraCenter(sf::Vector2f p)
{
    return {std::clamp(p.x, kViewW / 2.f, kWorldW - kViewW / 2.f), std::clamp(p.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
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
    auto loadTex = [](sf::Texture& t, const char* path) {
        if (!t.loadFromFile(path))
        {
            std::cerr << path << '\n';
            return false;
        }
        return true;
    };
    sf::Texture texPlayer, texWalker, texRunner, texBrute, texBullet, texGem;
    if (!loadTex(texPlayer, "assets/textures/sv_player.png") || !loadTex(texWalker, "assets/textures/sv_walker.png") ||
        !loadTex(texRunner, "assets/textures/sv_runner.png") || !loadTex(texBrute, "assets/textures/sv_brute.png") ||
        !loadTex(texBullet, "assets/textures/sv_bullet.png") || !loadTex(texGem, "assets/textures/sv_gem.png"))
        return 1;

    auto loadBuf = [](sf::SoundBuffer& b, const char* path) {
        if (!b.loadFromFile(path))
        {
            std::cerr << path << '\n';
            return false;
        }
        return true;
    };
    sf::SoundBuffer bufShoot, bufHit, bufPickup, bufLevel;
    if (!loadBuf(bufShoot, "assets/sfx/shoot.wav") || !loadBuf(bufHit, "assets/sfx/hit.wav") ||
        !loadBuf(bufPickup, "assets/sfx/pickup.wav") || !loadBuf(bufLevel, "assets/sfx/levelup.wav"))
        return 1;

    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_survivor.ogg"))
    {
        std::cerr << "assets/music/bgm_survivor.ogg\n";
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    SfxPool sfxShoot(bufShoot, 6), sfxHit(bufHit, 8), sfxPickup(bufPickup, 8), sfxLevel(bufLevel, 3);

    // ---- 창과 뷰 ----
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "LAST STAND");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::View worldView({kViewW / 2.f, kViewH / 2.f}, {kViewW, kViewH});
    sf::View hudView(sf::FloatRect({0.f, 0.f}, {kViewW, kViewH}));
    auto updateViewport = [&](sf::Vector2u size) {
        const float w = static_cast<float>(std::max(1u, size.x));
        const float h = static_cast<float>(std::max(1u, size.y));
        const float ratio = kViewW / kViewH;
        sf::FloatRect vp({0.f, 0.f}, {1.f, 1.f});
        if (w / h > ratio)
        {
            vp.size.x     = (h * ratio) / w;
            vp.position.x = (1.f - vp.size.x) / 2.f;
        }
        else
        {
            vp.size.y     = (w / ratio) / h;
            vp.position.y = (1.f - vp.size.y) / 2.f;
        }
        worldView.setViewport(vp);
        hudView.setViewport(vp);
    };
    updateViewport(window.getSize());

    // ---- 정적 배경 ----
    sf::RectangleShape worldBg({kWorldW, kWorldH});
    worldBg.setFillColor(hex(0x1A1420));
    sf::VertexArray grid(sf::PrimitiveType::Lines);
    const sf::Color gridCol = hex(0x2A2233);
    for (float x = 120.f; x < kWorldW; x += 120.f)
    {
        grid.append(sf::Vertex{{x + 0.5f, 0.f}, gridCol});
        grid.append(sf::Vertex{{x + 0.5f, kWorldH}, gridCol});
    }
    for (float y = 120.f; y < kWorldH; y += 120.f)
    {
        grid.append(sf::Vertex{{0.f, y + 0.5f}, gridCol});
        grid.append(sf::Vertex{{kWorldW, y + 0.5f}, gridCol});
    }
    sf::RectangleShape border({kWorldW, kWorldH});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(hex(0x6B5B7B));
    border.setOutlineThickness(-2.f);

    sf::RectangleShape screenBg({kViewW, kViewH});
    screenBg.setFillColor(hex(0x1A1420));

    sf::Text text(font, "", 16);
    text.setFillColor(sf::Color::White);
    auto drawText = [&](const std::string& s, unsigned size, sf::Vector2f pos, int align) {
        text.setString(s);
        text.setCharacterSize(size);
        const sf::FloatRect b = text.getLocalBounds();
        if (align == 1)
            text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        else if (align == 2)
            text.setOrigin({b.position.x + b.size.x, 0.f});
        else
            text.setOrigin({0.f, 0.f});
        text.setPosition(pos);
        window.draw(text);
    };

    std::mt19937 rng(std::random_device{}());
    auto rand01 = [&]() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); };

    // ---- 게임 상태 ----
    enum class Scene { Title, Play, Result };
    Scene scene = Scene::Title;
    float titleTimer = 0.f;
    State st;

    auto startPlay = [&]() {
        st    = State{};
        scene = Scene::Play;
        music.stop();
        music.play();
    };
    auto rollCards = [&]() {
        std::array<int, 6> idx{0, 1, 2, 3, 4, 5};
        std::shuffle(idx.begin(), idx.end(), rng);
        st.cards = {idx[0], idx[1], idx[2]};
    };
    auto applyCard = [&](int c) {
        switch (c)
        {
            case 0: st.fireInterval *= 0.85f; break;
            case 1: st.damage += 5; break;
            case 2: st.projectiles += 1; break;
            case 3: st.speed *= 1.1f; break;
            case 4:
                st.maxHp += 20.f;
                st.hp = std::min(st.hp + 20.f, st.maxHp);
                break;
            case 5: st.magnet += 40.f; break;
            default: break;
        }
    };

    // ---- 입력 ----
    // 방향: 0 left, 1 right, 2 up, 3 down
    std::array<bool, 4> held{}, tapped{};
    bool inEnter = false, inR = false, inP = false;
    int  inNum = 0;
    std::optional<sf::Vector2f> inClick;

    auto dirOf = [](sf::Keyboard::Scancode sc) -> int {
        using S = sf::Keyboard::Scancode;
        switch (sc)
        {
            case S::A:
            case S::Left: return 0;
            case S::D:
            case S::Right: return 1;
            case S::W:
            case S::Up: return 2;
            case S::S:
            case S::Down: return 3;
            default: return -1;
        }
    };

    auto spawnEnemy = [&]() {
        const sf::Vector2f c = cameraCenter(st.player);
        const float L = c.x - kViewW / 2.f - 64.f, T = c.y - kViewH / 2.f - 64.f;
        const float W = kViewW + 128.f, H = kViewH + 128.f;
        sf::Vector2f p;
        bool ok = false;
        for (int tries = 0; tries < 32 && !ok; ++tries)
        {
            float u = rand01() * 2.f * (W + H);
            if (u < W)
                p = {L + u, T};
            else if ((u -= W) < H)
                p = {L + W, T + u};
            else if ((u -= H) < W)
                p = {L + W - u, T + H};
            else
                p = {L, T + H - (u - W)};
            ok = p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
        }
        p.x = std::clamp(p.x, 0.f, kWorldW);
        p.y = std::clamp(p.y, 0.f, kWorldH);
        int type = 0;
        const float r = rand01();
        if (st.time >= 120.f)
            type = r < 0.6f ? 0 : (r < 0.9f ? 1 : 2);
        else if (st.time >= 60.f)
            type = r < 0.7f ? 0 : 1;
        Enemy e;
        e.pos  = p;
        e.type = type;
        e.hp   = kDefs[static_cast<std::size_t>(type)].hp;
        st.enemies.push_back(e);
    };

    auto updatePlay = [&](float dt) {
        if (inP && !st.choosing)
            st.paused = !st.paused;

        if (st.choosing)
        {
            int pick = -1;
            if (inNum >= 1 && inNum <= 3)
                pick = inNum - 1;
            if (inClick)
                for (int i = 0; i < 3; ++i)
                    if (sf::FloatRect({80.f + 280.f * static_cast<float>(i), 200.f}, {240.f, 140.f}).contains(*inClick))
                        pick = i;
            if (pick >= 0)
            {
                applyCard(st.cards[static_cast<std::size_t>(pick)]);
                --st.pendingLevels;
                if (st.pendingLevels > 0)
                    rollCards();
                else
                    st.choosing = false;
            }
            return;
        }
        if (st.paused)
            return;

        // 플레이어 이동
        sf::Vector2f dir;
        if (held[0] || tapped[0]) dir.x -= 1.f;
        if (held[1] || tapped[1]) dir.x += 1.f;
        if (held[2] || tapped[2]) dir.y -= 1.f;
        if (held[3] || tapped[3]) dir.y += 1.f;
        st.moving = dir.x != 0.f || dir.y != 0.f;
        if (st.moving)
        {
            if (dir.x < 0.f) st.facingLeft = true;
            else if (dir.x > 0.f) st.facingLeft = false;
            st.player += dir.normalized() * st.speed * dt;
            st.player.x = std::clamp(st.player.x, kPlayerR, kWorldW - kPlayerR);
            st.player.y = std::clamp(st.player.y, kPlayerR, kWorldH - kPlayerR);
            st.animT += dt;
        }
        else
            st.animT = 0.f;

        st.time += dt;

        // 생성
        st.spawnTimer -= dt;
        while (st.spawnTimer <= 0.f)
        {
            if (st.enemies.size() < 400)
                spawnEnemy();
            const float interval = std::max(0.15f, 0.6f - 0.05f * std::floor(st.time / 15.f));
            st.spawnTimer += interval;
        }

        // 적 이동
        for (auto& e : st.enemies)
        {
            if (e.dying)
            {
                e.death -= dt;
                continue;
            }
            if (e.flash > 0.f) e.flash -= dt;
            if (e.knock > 0.f)
            {
                e.pos += e.knockDir * 120.f * dt;
                e.knock -= dt;
            }
            else
            {
                const sf::Vector2f d = st.player - e.pos;
                const float len = d.length();
                if (len > 0.001f)
                    e.pos += d / len * std::min(kDefs[static_cast<std::size_t>(e.type)].speed * dt, len);
            }
            e.pos.x = std::clamp(e.pos.x, 0.f, kWorldW);
            e.pos.y = std::clamp(e.pos.y, 0.f, kWorldH);
        }

        // 자동 사격
        st.fireTimer += dt;
        if (st.fireTimer >= st.fireInterval)
        {
            st.fireTimer -= st.fireInterval;
            const Enemy* target = nullptr;
            float best = 400.f * 400.f;
            for (const auto& e : st.enemies)
            {
                if (e.dying) continue;
                const float d2 = (e.pos - st.player).lengthSquared();
                if (d2 <= best)
                {
                    best   = d2;
                    target = &e;
                }
            }
            if (target)
            {
                sf::Vector2f d = target->pos - st.player;
                const float base = d.lengthSquared() > 0.f ? std::atan2(d.y, d.x) : 0.f;
                const int n = st.projectiles;
                for (int i = 0; i < n; ++i)
                {
                    const float a = base + (static_cast<float>(i) - static_cast<float>(n - 1) / 2.f) * 10.f * kPi / 180.f;
                    Bullet b;
                    b.pos = st.player;
                    b.vel = {std::cos(a) * 500.f, std::sin(a) * 500.f};
                    st.bullets.push_back(b);
                }
                sfxShoot.play();
            }
        }

        // 총알
        for (auto& b : st.bullets)
        {
            b.pos += b.vel * dt;
            b.life -= dt;
            if (b.life <= 0.f) continue;
            for (auto& e : st.enemies)
            {
                if (e.dying) continue;
                const EnemyDef& def = kDefs[static_cast<std::size_t>(e.type)];
                const float rr = 4.f + def.radius;
                if ((e.pos - b.pos).lengthSquared() < rr * rr)
                {
                    e.hp -= static_cast<float>(st.damage);
                    if (e.hp <= 0.f)
                    {
                        e.dying = true;
                        e.death = 0.1f;
                        Gem g;
                        g.pos   = e.pos;
                        g.value = def.gem;
                        st.gems.push_back(g);
                        sfxHit.play();
                        ++st.kills;
                    }
                    else
                    {
                        e.knock    = 0.1f;
                        e.knockDir = b.vel.normalized();
                        e.flash    = 0.1f;
                    }
                    b.life = 0.f;
                    break;
                }
            }
        }

        // 접촉 피해
        st.hurt = false;
        for (const auto& e : st.enemies)
        {
            if (e.dying) continue;
            const EnemyDef& def = kDefs[static_cast<std::size_t>(e.type)];
            const float rr = kPlayerR + def.radius;
            if ((e.pos - st.player).lengthSquared() < rr * rr)
            {
                st.hp -= def.dps * dt;
                st.hurt = true;
            }
        }
        st.hp = std::max(0.f, st.hp);

        // 보석
        for (auto& g : st.gems)
        {
            g.life -= dt;
            if (g.life <= 0.f) continue;
            sf::Vector2f d = st.player - g.pos;
            float len = d.length();
            if (len < st.magnet && len > 0.001f)
            {
                g.pos += d / len * std::min(300.f * dt, len);
                len = (st.player - g.pos).length();
            }
            if (len < 6.f + kPlayerR)
            {
                st.xp += g.value;
                g.life = -1.f;
                sfxPickup.play();
            }
        }

        // 삭제
        std::erase_if(st.enemies, [](const Enemy& e) { return e.dying && e.death <= 0.f; });
        std::erase_if(st.bullets, [](const Bullet& b) { return b.life <= 0.f; });
        std::erase_if(st.gems, [](const Gem& g) { return g.life <= 0.f; });

        // 승패
        if (st.hp <= 0.f)
        {
            st.won = false;
            scene  = Scene::Result;
            music.stop();
            return;
        }
        if (st.time >= 300.f)
        {
            st.time = 300.f;
            st.won  = true;
            scene   = Scene::Result;
            music.stop();
            return;
        }

        // 레벨업
        while (st.xp >= 5 * st.level)
        {
            st.xp -= 5 * st.level;
            ++st.level;
            ++st.pendingLevels;
            sfxLevel.play();
        }
        if (st.pendingLevels > 0 && !st.choosing)
        {
            st.choosing = true;
            rollCards();
        }
    };

    auto update = [&](float dt) {
        switch (scene)
        {
            case Scene::Title:
                titleTimer += dt;
                if (inEnter || titleTimer >= 2.f)
                    startPlay();
                break;
            case Scene::Play: updatePlay(dt); break;
            case Scene::Result:
                if (inR)
                    startPlay();
                break;
        }
        inEnter = inR = inP = false;
        inNum   = 0;
        inClick.reset();
        tapped.fill(false);
    };

    std::vector<sf::Vertex> vWalker, vRunner, vBrute, vBullet, vGem, vPlayer;
    vWalker.reserve(2400);
    vRunner.reserve(2400);
    vBrute.reserve(2400);
    vGem.reserve(2400);
    vBullet.reserve(256);

    auto drawWorld = [&]() {
        worldView.setCenter(cameraCenter(st.player));
        window.setView(worldView);
        window.draw(worldBg);
        window.draw(grid);
        window.draw(border);

        vWalker.clear();
        vRunner.clear();
        vBrute.clear();
        vBullet.clear();
        vGem.clear();
        vPlayer.clear();

        for (const auto& g : st.gems)
            addQuad(vGem, g.pos, 12.f, 12.f, 0.f, 0.f, false, g.value >= 5 ? hex(0xFFD54A) : hex(0x4FC3F7));
        for (const auto& e : st.enemies)
        {
            const EnemyDef& def = kDefs[static_cast<std::size_t>(e.type)];
            int frame = (e.dying || e.flash > 0.f) ? 2 : static_cast<int>(st.time / def.period) % 2;
            const bool flip = st.player.x < e.pos.x;
            auto& vv = e.type == 0 ? vWalker : (e.type == 1 ? vRunner : vBrute);
            addQuad(vv, e.pos, def.frame, def.frame, def.frame * static_cast<float>(frame), 0.f, flip, sf::Color::White);
        }
        for (const auto& b : st.bullets)
            addQuad(vBullet, b.pos, 8.f, 8.f, 0.f, 0.f, false, sf::Color::White);
        const int pframe = st.moving ? static_cast<int>(st.animT / 0.15f) % 2 : 0;
        addQuad(vPlayer, st.player, 32.f, 32.f, 32.f * static_cast<float>(pframe), 0.f, st.facingLeft,
                st.hurt ? hex(0xFF6060) : sf::Color::White);

        auto flush = [&](const std::vector<sf::Vertex>& v, const sf::Texture& t) {
            if (!v.empty())
                window.draw(v.data(), v.size(), sf::PrimitiveType::Triangles, sf::RenderStates(&t));
        };
        flush(vGem, texGem);
        flush(vWalker, texWalker);
        flush(vRunner, texRunner);
        flush(vBrute, texBrute);
        flush(vPlayer, texPlayer);
        flush(vBullet, texBullet);
    };

    auto drawHud = [&]() {
        window.setView(hudView);
        sf::RectangleShape r;
        r.setPosition({0.f, 0.f});
        r.setSize({kViewW, 6.f});
        r.setFillColor(hex(0x2A2233));
        window.draw(r);
        const float frac = std::clamp(static_cast<float>(st.xp) / static_cast<float>(5 * st.level), 0.f, 1.f);
        r.setSize({kViewW * frac, 6.f});
        r.setFillColor(hex(0x4FC3F7));
        window.draw(r);

        r.setPosition({8.f, 14.f});
        r.setSize({200.f, 12.f});
        r.setFillColor(hex(0x3A1F24));
        window.draw(r);
        r.setSize({200.f * std::clamp(st.hp / st.maxHp, 0.f, 1.f), 12.f});
        r.setFillColor(hex(0xE5533D));
        window.draw(r);
        drawText("HP " + std::to_string(static_cast<int>(std::ceil(st.hp))), 16, {216.f, 10.f}, 0);

        drawText("LV " + std::to_string(st.level), 16, {950.f, 12.f}, 2);
        drawText("TIME " + mmss(st.time), 16, {950.f, 32.f}, 2);
        drawText("KILLS " + std::to_string(st.kills), 16, {950.f, 52.f}, 2);

        if (st.choosing)
        {
            sf::RectangleShape dim({kViewW, kViewH});
            dim.setFillColor(sf::Color(0, 0, 0, 120));
            window.draw(dim);
            for (int i = 0; i < 3; ++i)
            {
                const float x = 80.f + 280.f * static_cast<float>(i);
                sf::RectangleShape card({240.f, 140.f});
                card.setPosition({x, 200.f});
                card.setFillColor(hex(0x2A2233));
                card.setOutlineColor(hex(0xFFD54A));
                card.setOutlineThickness(-2.f);
                window.draw(card);
                drawText(kCardNames[static_cast<std::size_t>(st.cards[static_cast<std::size_t>(i)])], 16,
                         {x + 120.f, 270.f}, 1);
            }
        }
        else if (st.paused)
            drawText("PAUSED", 32, {kViewW / 2.f, kViewH / 2.f}, 1);
    };

    // ---- 메인 루프 ----
    sf::Clock clock;
    float accumulator = 0.f;
    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }
            if (const auto* rs = event->getIf<sf::Event::Resized>())
                updateViewport(rs->size);
            else if (event->is<sf::Event::FocusLost>())
                held.fill(false);
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                using S = sf::Keyboard::Scancode;
                if (kp->scancode == S::Escape)
                {
                    window.close();
                    break;
                }
                const int d = dirOf(kp->scancode);
                if (d >= 0)
                {
                    held[static_cast<std::size_t>(d)]   = true;
                    tapped[static_cast<std::size_t>(d)] = true;
                }
                if (kp->scancode == S::Enter || kp->scancode == S::NumpadEnter) inEnter = true;
                if (kp->scancode == S::R) inR = true;
                if (kp->scancode == S::P) inP = true;
                if (kp->scancode == S::Num1 || kp->scancode == S::Numpad1) inNum = 1;
                if (kp->scancode == S::Num2 || kp->scancode == S::Numpad2) inNum = 2;
                if (kp->scancode == S::Num3 || kp->scancode == S::Numpad3) inNum = 3;
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                const int d = dirOf(kr->scancode);
                if (d >= 0)
                    held[static_cast<std::size_t>(d)] = false;
            }
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                    inClick = window.mapPixelToCoords(mb->position, hudView);
            }
        }
        if (!window.isOpen())
            break;

        accumulator += std::min(clock.restart().asSeconds(), 0.25f);
        while (accumulator >= kDt)
        {
            update(kDt);
            accumulator -= kDt;
        }

        window.clear(sf::Color::Black);
        if (scene == Scene::Title)
        {
            window.setView(hudView);
            window.draw(screenBg);
            drawText("LAST STAND", 48, {kViewW / 2.f, 220.f}, 1);
            drawText("PRESS ENTER", 16, {kViewW / 2.f, 320.f}, 1);
        }
        else if (scene == Scene::Play)
        {
            drawWorld();
            drawHud();
        }
        else
        {
            window.setView(hudView);
            window.draw(screenBg);
            drawText(st.won ? "SURVIVED" : "GAME OVER", 48, {kViewW / 2.f, 160.f}, 1);
            drawText("TIME " + mmss(st.time), 32, {kViewW / 2.f, 250.f}, 1);
            drawText("LEVEL " + std::to_string(st.level), 32, {kViewW / 2.f, 295.f}, 1);
            drawText("KILLS " + std::to_string(st.kills), 32, {kViewW / 2.f, 340.f}, 1);
            drawText("R: RESTART  ESC: QUIT", 16, {kViewW / 2.f, 420.f}, 1);
        }
        window.display();
    }
    music.stop();
    return 0;
}
```