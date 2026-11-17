```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kPlayerR = 14.f;
constexpr float kPi = 3.14159265358979f;
constexpr float kRunSeconds = 300.f;

const sf::Color kBg{0x1A, 0x14, 0x20};
const sf::Color kGridC{0x2A, 0x22, 0x33};
const sf::Color kBorderC{0x6B, 0x5B, 0x7B};
const sf::Color kPanelC{0x2A, 0x22, 0x33};
const sf::Color kXpC{0x4F, 0xC3, 0xF7};
const sf::Color kGoldC{0xFF, 0xD5, 0x4A};
const sf::Color kHpBgC{0x3A, 0x1F, 0x24};
const sf::Color kHpC{0xE5, 0x53, 0x3D};
const sf::Color kHurtC{0xFF, 0x60, 0x60};
const sf::Color kWhite{0xFF, 0xFF, 0xFF};

std::mt19937 rng{std::random_device{}()};

float frand() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
float clampf(float v, float lo, float hi) { return std::clamp(v, lo, hi); }
float sqDist(sf::Vector2f a, sf::Vector2f b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}
float lenOf(sf::Vector2f v) { return std::hypot(v.x, v.y); }
sf::Vector2f normOf(sf::Vector2f v) {
    const float L = lenOf(v);
    return L > 1e-6f ? sf::Vector2f{v.x / L, v.y / L} : sf::Vector2f{0.f, 0.f};
}

struct EnemyDef {
    float hp, speed, radius, dps, period;
    int size;
    int gem;
};
constexpr EnemyDef kDefs[3] = {
    {10.f, 80.f, 10.f, 10.f, 0.30f, 24, 1},   // 걷는 적
    {6.f, 140.f, 10.f, 6.f, 0.15f, 24, 1},    // 뛰는 적
    {60.f, 50.f, 20.f, 20.f, 0.40f, 48, 5},   // 큰 적
};

constexpr std::array<std::string_view, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40"};

struct Assets {
    sf::Font font;
    sf::Texture player, walker, runner, brute, bullet, gem;
    sf::SoundBuffer shoot, hit, pickup, levelup;
    sf::Music bgm;
};

bool loadAssets(Assets& a) {
    bool ok = true;
    auto check = [&ok](const char* path, bool loaded) {
        if (!loaded) {
            std::cerr << "failed to load asset: " << path << '\n';
            ok = false;
        }
    };
    const char* p = nullptr;
    p = "assets/fonts/game.ttf";            check(p, a.font.openFromFile(p));
    p = "assets/textures/sv_player.png";    check(p, a.player.loadFromFile(p));
    p = "assets/textures/sv_walker.png";    check(p, a.walker.loadFromFile(p));
    p = "assets/textures/sv_runner.png";    check(p, a.runner.loadFromFile(p));
    p = "assets/textures/sv_brute.png";     check(p, a.brute.loadFromFile(p));
    p = "assets/textures/sv_bullet.png";    check(p, a.bullet.loadFromFile(p));
    p = "assets/textures/sv_gem.png";       check(p, a.gem.loadFromFile(p));
    p = "assets/sfx/shoot.wav";             check(p, a.shoot.loadFromFile(p));
    p = "assets/sfx/hit.wav";               check(p, a.hit.loadFromFile(p));
    p = "assets/sfx/pickup.wav";            check(p, a.pickup.loadFromFile(p));
    p = "assets/sfx/levelup.wav";           check(p, a.levelup.loadFromFile(p));
    p = "assets/music/bgm_survivor.ogg";    check(p, a.bgm.openFromFile(p));
    return ok;
}

struct Sfx {
    sf::Sound shoot, hit, pickup, levelup;
    explicit Sfx(const Assets& a)
        : shoot(a.shoot), hit(a.hit), pickup(a.pickup), levelup(a.levelup) {}
};

struct Enemy {
    int type = 0;
    sf::Vector2f pos;
    float hp = 0.f, flashT = 0.f, knockT = 0.f, deadT = 0.f;
    bool dying = false;
    sf::Vector2f knockVel;
};

struct Bullet {
    sf::Vector2f pos, vel;
    float life = 0.f, dmg = 0.f;
};

struct Gem {
    sf::Vector2f pos;
    int value = 1;
    float life = 30.f;
    bool collected = false;
};

enum class Scene { Title, Playing, Result };

struct Game {
    Scene scene = Scene::Title;
    float titleT = 0.f, gameT = 0.f, nextSpawnT = 0.5f, fireTimer = 0.f, animT = 0.f;
    sf::Vector2f playerPos{1440.f, 810.f};
    float playerHp = 100.f, maxHp = 100.f, moveMul = 1.f, fireMul = 1.f;
    float dmgBonus = 0.f, magnetR = 60.f;
    int projCount = 1, level = 1, xp = 0, kills = 0, pending = 0;
    bool facingLeft = false, moving = false, hurt = false;
    bool paused = false, choosing = false, won = false;
    std::array<int, 3> cards{};
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};

struct Input {
    bool enter = false, pause = false, restart = false;
    int pick = -1;
    std::optional<sf::Vector2f> click;
};

sf::Vector2f camCenter(sf::Vector2f p) {
    return {clampf(p.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
            clampf(p.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
}

sf::FloatRect letterbox(sf::Vector2u ws) {
    if (ws.x == 0 || ws.y == 0) return sf::FloatRect({0.f, 0.f}, {1.f, 1.f});
    const float s = std::min(ws.x / kViewW, ws.y / kViewH);
    const float w = kViewW * s / ws.x;
    const float h = kViewH * s / ws.y;
    return sf::FloatRect({(1.f - w) / 2.f, (1.f - h) / 2.f}, {w, h});
}

sf::View makeView(sf::Vector2f center, sf::FloatRect vp) {
    sf::View v(center, {kViewW, kViewH});
    v.setViewport(vp);
    return v;
}

int cardAt(sf::Vector2f p) {
    for (int i = 0; i < 3; ++i) {
        if (sf::FloatRect({80.f + 280.f * i, 200.f}, {240.f, 140.f}).contains(p)) return i;
    }
    return -1;
}

void resetRun(Game& g) {
    Game fresh;
    fresh.scene = Scene::Playing;
    g = std::move(fresh);
}

void finish(Game& g, Assets& a, bool won) {
    g.scene = Scene::Result;
    g.won = won;
    a.bgm.pause();
}

void rollCards(Game& g) {
    std::array<int, 6> ids{0, 1, 2, 3, 4, 5};
    std::shuffle(ids.begin(), ids.end(), rng);
    g.cards = {ids[0], ids[1], ids[2]};
    g.choosing = true;
}

void applyCard(Game& g, int id) {
    switch (id) {
        case 0: g.fireMul *= 0.85f; break;
        case 1: g.dmgBonus += 5.f; break;
        case 2: g.projCount += 1; break;
        case 3: g.moveMul *= 1.1f; break;
        case 4:
            g.maxHp += 20.f;
            g.playerHp = std::min(g.playerHp + 20.f, g.maxHp);
            break;
        default: g.magnetR += 40.f; break;
    }
    g.pending--;
    if (g.pending > 0) rollCards(g);
    else g.choosing = false;
}

void spawnEnemy(Game& g) {
    int type = 0;
    const float r = frand();
    if (g.gameT >= 120.f) type = r < 0.6f ? 0 : (r < 0.9f ? 1 : 2);
    else if (g.gameT >= 60.f) type = r < 0.7f ? 0 : 1;

    const sf::Vector2f c = camCenter(g.playerPos);
    const float l = c.x - kViewW / 2.f - 64.f, rt = c.x + kViewW / 2.f + 64.f;
    const float tp = c.y - kViewH / 2.f - 64.f, bt = c.y + kViewH / 2.f + 64.f;
    const float w = rt - l, h = bt - tp;
    const float s = frand() * 2.f * (w + h);
    sf::Vector2f p;
    if (s < w) p = {l + s, tp};
    else if (s < w + h) p = {rt, tp + (s - w)};
    else if (s < 2.f * w + h) p = {rt - (s - w - h), bt};
    else p = {l, bt - (s - 2.f * w - h)};

    Enemy e;
    e.type = type;
    e.hp = kDefs[type].hp;
    e.pos = {clampf(p.x, 0.f, kWorldW), clampf(p.y, 0.f, kWorldH)};
    g.enemies.push_back(e);
}

void stepWorld(Game& g, float dt, Sfx& sfx) {
    using K = sf::Keyboard::Key;
    g.gameT += dt;

    // 이동
    sf::Vector2f dir{0.f, 0.f};
    if (sf::Keyboard::isKeyPressed(K::A) || sf::Keyboard::isKeyPressed(K::Left)) dir.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(K::D) || sf::Keyboard::isKeyPressed(K::Right)) dir.x += 1.f;
    if (sf::Keyboard::isKeyPressed(K::W) || sf::Keyboard::isKeyPressed(K::Up)) dir.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(K::S) || sf::Keyboard::isKeyPressed(K::Down)) dir.y += 1.f;
    g.moving = dir.x != 0.f || dir.y != 0.f;
    if (g.moving) {
        dir = normOf(dir);
        if (dir.x < 0.f) g.facingLeft = true;
        else if (dir.x > 0.f) g.facingLeft = false;
        g.animT += dt;
        const float sp = 200.f * g.moveMul * dt;
        g.playerPos.x += dir.x * sp;
        g.playerPos.y += dir.y * sp;
    }
    g.playerPos.x = clampf(g.playerPos.x, kPlayerR, kWorldW - kPlayerR);
    g.playerPos.y = clampf(g.playerPos.y, kPlayerR, kWorldH - kPlayerR);

    // 자동 사격
    const float interval = 0.8f * g.fireMul;
    g.fireTimer = std::min(g.fireTimer + dt, interval);
    const Enemy* target = nullptr;
    float best = 400.f * 400.f;
    for (const Enemy& e : g.enemies) {
        if (e.dying) continue;
        const float d2 = sqDist(e.pos, g.playerPos);
        if (d2 <= best) {
            best = d2;
            target = &e;
        }
    }
    if (target && g.fireTimer >= interval) {
        g.fireTimer = 0.f;
        const float ang = std::atan2(target->pos.y - g.playerPos.y, target->pos.x - g.playerPos.x);
        const int n = g.projCount;
        for (int i = 0; i < n; ++i) {
            const float a = ang + (i - (n - 1) / 2.f) * (10.f * kPi / 180.f);
            g.bullets.push_back({g.playerPos, {500.f * std::cos(a), 500.f * std::sin(a)},
                                 1.2f, 10.f + g.dmgBonus});
        }
        sfx.shoot.play();
    }

    // 총알 이동과 적중
    for (Bullet& b : g.bullets) {
        b.pos += b.vel * dt;
        b.life -= dt;
    }
    for (Bullet& b : g.bullets) {
        if (b.life <= 0.f) continue;
        for (Enemy& e : g.enemies) {
            if (e.dying) continue;
            const float R = kDefs[e.type].radius + 4.f;
            if (sqDist(e.pos, b.pos) >= R * R) continue;
            b.life = 0.f;
            e.hp -= b.dmg;
            e.flashT = 0.1f;
            if (e.hp <= 0.f) {
                e.dying = true;
                e.deadT = 0.1f;
                g.gems.push_back({e.pos, kDefs[e.type].gem, 30.f, false});
                ++g.kills;
                sfx.hit.play();
            } else {
                e.knockT = 0.1f;
                const sf::Vector2f d = normOf(b.vel);
                e.knockVel = {d.x * 120.f, d.y * 120.f};
            }
            break;
        }
    }
    std::erase_if(g.bullets, [](const Bullet& b) { return b.life <= 0.f; });

    // 적 이동
    for (Enemy& e : g.enemies) {
        if (e.dying) {
            e.deadT -= dt;
            continue;
        }
        e.flashT = std::max(0.f, e.flashT - dt);
        if (e.knockT > 0.f) {
            e.knockT -= dt;
            e.pos.x += e.knockVel.x * dt;
            e.pos.y += e.knockVel.y * dt;
        } else {
            const sf::Vector2f d = g.playerPos - e.pos;
            const float L = lenOf(d);
            if (L > 0.f) {
                const float sp = kDefs[e.type].speed * dt;
                e.pos.x += d.x / L * sp;
                e.pos.y += d.y / L * sp;
            }
        }
        const float r = kDefs[e.type].radius;
        e.pos.x = clampf(e.pos.x, r, kWorldW - r);
        e.pos.y = clampf(e.pos.y, r, kWorldH - r);
    }

    // 접촉 피해
    g.hurt = false;
    for (const Enemy& e : g.enemies) {
        if (e.dying) continue;
        const float R = kPlayerR + kDefs[e.type].radius;
        if (sqDist(e.pos, g.playerPos) < R * R) {
            g.playerHp -= kDefs[e.type].dps * dt;
            g.hurt = true;
        }
    }
    std::erase_if(g.enemies, [](const Enemy& e) { return e.dying && e.deadT <= 0.f; });

    // 보석
    bool got = false;
    for (Gem& gm : g.gems) {
        gm.life -= dt;
        const sf::Vector2f d = g.playerPos - gm.pos;
        const float L = lenOf(d);
        if (L > 0.f && L <= g.magnetR) {
            gm.pos.x += d.x / L * 300.f * dt;
            gm.pos.y += d.y / L * 300.f * dt;
        }
        const float pr = kPlayerR + 6.f;
        if (sqDist(gm.pos, g.playerPos) < pr * pr) {
            gm.collected = true;
            g.xp += gm.value;
            got = true;
        }
    }
    if (got) sfx.pickup.play();
    std::erase_if(g.gems, [](const Gem& gm) { return gm.collected || gm.life <= 0.f; });

    // 레벨업
    while (g.xp >= 5 * g.level) {
        g.xp -= 5 * g.level;
        ++g.level;
        ++g.pending;
        sfx.levelup.play();
    }
    if (g.pending > 0 && !g.choosing) rollCards(g);

    // 적 생성
    if (g.gameT >= g.nextSpawnT) {
        if (g.enemies.size() < 400) spawnEnemy(g);
        g.nextSpawnT = g.gameT + std::max(0.15f, 0.6f - 0.05f * std::floor(g.gameT / 15.f));
    }
}

void drawFrame(sf::RenderTarget& t, const sf::Texture& tex, int frame, sf::Vector2i size,
               sf::Vector2f pos, bool flip, sf::Color color = sf::Color::White) {
    sf::Sprite s(tex);
    s.setTextureRect(sf::IntRect({frame * size.x, 0}, size));
    s.setOrigin({size.x / 2.f, size.y / 2.f});
    s.setPosition(pos);
    s.setScale({flip ? -1.f : 1.f, 1.f});
    s.setColor(color);
    t.draw(s);
}

void drawText(sf::RenderTarget& t, const sf::Font& f, std::string_view s, unsigned size,
              sf::Vector2f pos, sf::Color c, bool center = false) {
    sf::Text tx(f, sf::String(s), size);
    if (center) {
        const sf::FloatRect b = tx.getLocalBounds();
        tx.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    }
    tx.setFillColor(c);
    tx.setPosition(pos);
    t.draw(tx);
}

void drawWorld(sf::RenderTarget& t, const Assets& a, const Game& g) {
    sf::RectangleShape r({kWorldW, kWorldH});
    r.setFillColor(kBg);
    t.draw(r);

    r.setFillColor(kGridC);
    for (float x = 0.f; x <= kWorldW; x += 120.f) {
        r.setSize({1.f, kWorldH});
        r.setPosition({x, 0.f});
        t.draw(r);
    }
    for (float y = 0.f; y <= kWorldH; y += 120.f) {
        r.setSize({kWorldW, 1.f});
        r.setPosition({0.f, y});
        t.draw(r);
    }

    r.setSize({kWorldW, kWorldH});
    r.setPosition({0.f, 0.f});
    r.setFillColor(sf::Color::Transparent);
    r.setOutlineColor(kBorderC);
    r.setOutlineThickness(-2.f);
    t.draw(r);

    for (const Gem& gm : g.gems) {
        drawFrame(t, a.gem, 0, {12, 12}, gm.pos, false, gm.value >= 5 ? kGoldC : kXpC);
    }
    for (const Enemy& e : g.enemies) {
        const EnemyDef& d = kDefs[e.type];
        const sf::Texture& tex = e.type == 0 ? a.walker : (e.type == 1 ? a.runner : a.brute);
        const int fr = (e.dying || e.flashT > 0.f) ? 2 : static_cast<int>(g.gameT / d.period) % 2;
        drawFrame(t, tex, fr, {d.size, d.size}, e.pos, g.playerPos.x < e.pos.x);
    }
    for (const Bullet& b : g.bullets) {
        drawFrame(t, a.bullet, 0, {8, 8}, b.pos, false);
    }
    const int pf = g.moving ? static_cast<int>(g.animT / 0.15f) % 2 : 0;
    drawFrame(t, a.player, pf, {32, 32}, g.playerPos, g.facingLeft, g.hurt ? kHurtC : kWhite);
}

void drawHud(sf::RenderTarget& t, const Assets& a, const Game& g) {
    sf::RectangleShape r;
    const float need = 5.f * static_cast<float>(g.level);

    r.setPosition({0.f, 0.f});
    r.setSize({kViewW, 6.f});
    r.setFillColor(kPanelC);
    t.draw(r);
    r.setSize({kViewW * clampf(g.xp / need, 0.f, 1.f), 6.f});
    r.setFillColor(kXpC);
    t.draw(r);

    r.setPosition({8.f, 14.f});
    r.setSize({200.f, 12.f});
    r.setFillColor(kHpBgC);
    t.draw(r);
    r.setSize({200.f * clampf(g.playerHp / g.maxHp, 0.f, 1.f), 12.f});
    r.setFillColor(kHpC);
    t.draw(r);

    const int hpShow = std::max(0, static_cast<int>(std::ceil(g.playerHp)));
    drawText(t, a.font, std::format("HP {}", hpShow), 16, {216.f, 12.f}, kWhite);
    const int sec = std::min(static_cast<int>(g.gameT), 300);
    drawText(t, a.font, std::format("LV {}", g.level), 16, {600.f, 12.f}, kWhite);
    drawText(t, a.font, std::format("TIME {:02}:{:02}", sec / 60, sec % 60), 16, {700.f, 12.f}, kWhite);
    drawText(t, a.font, std::format("KILLS {}", g.kills), 16, {820.f, 12.f}, kWhite);
}

void drawCards(sf::RenderTarget& t, const Assets& a, const Game& g) {
    sf::RectangleShape card({240.f, 140.f});
    for (int i = 0; i < 3; ++i) {
        const float x = 80.f + 280.f * i, y = 200.f;
        card.setPosition({x, y});
        card.setFillColor(kPanelC);
        card.setOutlineColor(kGoldC);
        card.setOutlineThickness(-2.f);
        t.draw(card);
        drawText(t, a.font, kCardNames[g.cards[i]], 16, {x + 120.f, y + 70.f}, kWhite, true);
    }
}

void drawTitle(sf::RenderTarget& t, const Assets& a) {
    sf::RectangleShape bg({kViewW, kViewH});
    bg.setFillColor(kBg);
    t.draw(bg);
    drawText(t, a.font, "LAST STAND", 48, {480.f, 200.f}, kWhite, true);
    drawText(t, a.font, "PRESS ENTER", 32, {480.f, 320.f}, kWhite, true);
}

void drawResult(sf::RenderTarget& t, const Assets& a, const Game& g) {
    sf::RectangleShape bg({kViewW, kViewH});
    bg.setFillColor(kBg);
    t.draw(bg);
    drawText(t, a.font, g.won ? "SURVIVED" : "GAME OVER", 48, {480.f, 150.f}, kWhite, true);
    const int sec = std::min(static_cast<int>(g.gameT), 300);
    drawText(t, a.font, std::format("TIME {:02}:{:02}", sec / 60, sec % 60), 32, {480.f, 240.f}, kWhite, true);
    drawText(t, a.font, std::format("LEVEL {}", g.level), 32, {480.f, 300.f}, kWhite, true);
    drawText(t, a.font, std::format("KILLS {}", g.kills), 32, {480.f, 360.f}, kWhite, true);
    drawText(t, a.font, "R: RESTART  ESC: QUIT", 32, {480.f, 460.f}, kWhite, true);
}

}  // namespace

int main() {
    Assets a;
    if (!loadAssets(a)) return 1;
    Sfx sfx(a);
    a.bgm.setLooping(true);
    a.bgm.setVolume(40.f);

    sf::RenderWindow window(sf::VideoMode({960, 540}), "LAST STAND");
    window.setFramerateLimit(60);

    Game g;
    sf::Clock clock;
    while (window.isOpen()) {
        const float dt = std::min(clock.restart().asSeconds(), 0.1f);
        const sf::FloatRect vp = letterbox(window.getSize());
        const sf::View uiView = makeView({kViewW / 2.f, kViewH / 2.f}, vp);

        Input in;
        while (const std::optional<sf::Event> ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                switch (k->code) {
                    case sf::Keyboard::Key::Escape: window.close(); break;
                    case sf::Keyboard::Key::Enter: in.enter = true; break;
                    case sf::Keyboard::Key::P: in.pause = true; break;
                    case sf::Keyboard::Key::R: in.restart = true; break;
                    case sf::Keyboard::Key::Num1: in.pick = 0; break;
                    case sf::Keyboard::Key::Num2: in.pick = 1; break;
                    case sf::Keyboard::Key::Num3: in.pick = 2; break;
                    default: break;
                }
            } else if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    in.click = window.mapPixelToCoords(m->position, uiView);
                }
            }
        }

        window.clear(sf::Color::Black);
        switch (g.scene) {
            case Scene::Title: {
                g.titleT += dt;
                if (in.enter || g.titleT >= 2.f) {
                    resetRun(g);
                    a.bgm.play();
                } else {
                    window.setView(uiView);
                    drawTitle(window, a);
                }
                break;
            }
            case Scene::Playing: {
                if (g.choosing) {
                    int pick = in.pick;
                    if (in.click) pick = cardAt(*in.click);
                    if (pick >= 0) applyCard(g, g.cards[pick]);
                } else if (in.pause) {
                    g.paused = !g.paused;
                }

                if (!g.choosing && !g.paused) {
                    stepWorld(g, dt, sfx);
                    if (g.playerHp <= 0.f) finish(g, a, false);
                    else if (g.gameT >= kRunSeconds) finish(g, a, true);
                }

                window.setView(makeView(camCenter(g.playerPos), vp));
                drawWorld(window, a, g);
                window.setView(uiView);
                drawHud(window, a, g);
                if (g.choosing) {
                    drawCards(window, a, g);
                } else if (g.paused) {
                    sf::RectangleShape dim({kViewW, kViewH});
                    dim.setFillColor(sf::Color(0, 0, 0, 120));
                    window.draw(dim);
                    drawText(window, a.font, "PAUSED", 48, {480.f, 270.f}, kWhite, true);
                }
                break;
            }
            case Scene::Result: {
                if (in.restart) {
                    resetRun(g);
                    a.bgm.stop();
                    a.bgm.play();
                } else {
                    window.setView(uiView);
                    drawResult(window, a, g);
                }
                break;
            }
        }
        window.display();
    }
    return 0;
}
```