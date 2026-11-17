```cpp
// main.cpp - LAST STAND (서바이버류), SFML 3.1
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace {

constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kStep = 1.f / 60.f;
constexpr float kPlayerR = 14.f;
constexpr float kSurviveTime = 300.f;
constexpr std::size_t kMaxEnemies = 400;
constexpr float kPi = 3.14159265f;
constexpr float kCardY = 200.f;

const sf::Color kBg(0x1A, 0x14, 0x20);
const sf::Color kGridC(0x2A, 0x22, 0x33);
const sf::Color kBorderC(0x6B, 0x5B, 0x7B);
const sf::Color kXpC(0x4F, 0xC3, 0xF7);
const sf::Color kGoldC(0xFF, 0xD5, 0x4A);
const sf::Color kHpBgC(0x3A, 0x1F, 0x24);
const sf::Color kHpC(0xE5, 0x53, 0x3D);
const sf::Color kHurtC(0xFF, 0x60, 0x60);

const std::array<std::string, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1",
    "SPEED +10%", "MAX HP +20", "MAGNET +40"};

using Key = sf::Keyboard::Key;

struct EType {
    float radius;
    float hp;
    float speed;
    float dps;
    float period;
    int xp;
    int size;
};

constexpr std::array<EType, 3> kTypes{{
    {10.f, 10.f, 80.f, 10.f, 0.3f, 1, 24},
    {10.f, 6.f, 140.f, 6.f, 0.15f, 1, 24},
    {20.f, 60.f, 50.f, 20.f, 0.4f, 5, 48},
}};

// ---------- 수학/난수 ----------
sf::Vector2f V(float x, float y) { return sf::Vector2f(x, y); }
float len2(sf::Vector2f v) { return v.x * v.x + v.y * v.y; }
float len(sf::Vector2f v) { return std::sqrt(len2(v)); }
sf::Vector2f norm(sf::Vector2f v) {
    const float l = len(v);
    return l > 1e-6f ? sf::Vector2f(v.x / l, v.y / l) : sf::Vector2f(0.f, 0.f);
}
std::mt19937& rng() {
    static std::mt19937 g{std::random_device{}()};
    return g;
}
float rf(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng()); }
int ri(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng()); }

// ---------- 자원 ----------
struct Assets {
    sf::Font font;
    sf::Texture player, walker, runner, brute, bullet, gem;
    sf::SoundBuffer shoot, hit, pickup, levelup;
    sf::Music bgm;
};

template <typename T>
bool loadRes(T& res, const std::string& path) {
    bool ok = false;
    if constexpr (std::is_same_v<T, sf::Font> || std::is_same_v<T, sf::Music>) {
        ok = res.openFromFile(path);
    } else {
        ok = res.loadFromFile(path);
    }
    if (!ok) std::cerr << "failed to load: " << path << '\n';
    return ok;
}

// ---------- 게임 객체 ----------
enum class Scene { Title, Play, Result };

struct Player {
    sf::Vector2f pos{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    float speed = 200.f;
    float fireCd = 0.8f;
    float fireT = 0.f;
    float dmg = 10.f;
    int proj = 1;
    float magnet = 60.f;
    float hurtT = 0.f;
    float animT = 0.f;
    bool moving = false;
    bool faceLeft = false;
};

struct Enemy {
    int type = 0;
    sf::Vector2f pos;
    float hp = 0.f;
    bool dead = false;
    float deadT = 0.f;
    float hitT = 0.f;
    float knockT = 0.f;
    sf::Vector2f knockV;
};

struct Bullet {
    sf::Vector2f pos, vel;
    float life = 0.f;
    float dmg = 0.f;
};

struct Gem {
    sf::Vector2f pos;
    int value = 1;
    float life = 30.f;
};

struct World {
    Player p;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
    float time = 0.f;
    float nextSpawn = 0.5f;
    int kills = 0;
    int level = 1;
    int xp = 0;
    int pendingLv = 0;
    bool paused = false;
    bool choosing = false;
    std::array<int, 3> cards{};
};

struct App {
    Assets a;
    std::vector<std::unique_ptr<sf::Sound>> sfx;
    World w;
    Scene scene = Scene::Title;
    float titleT = 0.f;
    std::string resultTitle;
};

// ---------- 뷰 ----------
sf::FloatRect gVp{sf::Vector2f(0.f, 0.f), sf::Vector2f(1.f, 1.f)};

void updateViewport(const sf::RenderWindow& win) {
    const sf::Vector2u s = win.getSize();
    if (s.x == 0 || s.y == 0) return;
    const float sx = static_cast<float>(s.x);
    const float sy = static_cast<float>(s.y);
    const float k = std::min(sx / kViewW, sy / kViewH);
    const float vw = kViewW * k;
    const float vh = kViewH * k;
    const float vx = (sx - vw) / 2.f;
    const float vy = (sy - vh) / 2.f;
    gVp = sf::FloatRect(V(vx / sx, vy / sy), V(vw / sx, vh / sy));
}

sf::View makeView(sf::Vector2f center) {
    sf::View v(center, V(kViewW, kViewH));
    v.setViewport(gVp);
    return v;
}

sf::Vector2f camCenter(sf::Vector2f p) {
    return V(std::clamp(p.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
             std::clamp(p.y, kViewH / 2.f, kWorldH - kViewH / 2.f));
}

// ---------- 그리기 헬퍼 ----------
void drawRect(sf::RenderTarget& t, sf::Vector2f pos, sf::Vector2f size, sf::Color c) {
    sf::RectangleShape r(size);
    r.setPosition(pos);
    r.setFillColor(c);
    t.draw(r);
}

sf::Text makeText(const sf::Font& f, const std::string& s, unsigned size, sf::Vector2f pos, bool center = false) {
    sf::Text t(f, sf::String(s), size);
    if (center) {
        const sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(V(b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f));
    }
    t.setPosition(pos);
    t.setFillColor(sf::Color::White);
    return t;
}

std::string timeStr(float t) {
    const int s = static_cast<int>(std::min(t, kSurviveTime));
    return std::format("{:02}:{:02}", s / 60, s % 60);
}

// ---------- 사운드/상태 전환 ----------
void playSfx(App& app, const sf::SoundBuffer& buf) {
    std::erase_if(app.sfx, [](const std::unique_ptr<sf::Sound>& s) {
        return s->getStatus() == sf::SoundSource::Status::Stopped;
    });
    app.sfx.push_back(std::make_unique<sf::Sound>(buf));
    app.sfx.back()->play();
}

void startPlay(App& app) {
    app.w = World{};
    app.scene = Scene::Play;
    app.a.bgm.stop();
    app.a.bgm.play();
}

void endGame(App& app, bool survived) {
    app.scene = Scene::Result;
    app.resultTitle = survived ? "SURVIVED" : "GAME OVER";
    app.a.bgm.pause();
}

// ---------- 카드 ----------
void rollCards(World& w) {
    std::array<std::size_t, 6> idx{0, 1, 2, 3, 4, 5};
    std::shuffle(idx.begin(), idx.end(), rng());
    for (std::size_t i = 0; i < 3; ++i) w.cards[i] = static_cast<int>(idx[i]);
}

void applyCard(World& w, int id) {
    Player& p = w.p;
    switch (id) {
        case 0: p.fireCd *= 0.85f; break;
        case 1: p.dmg += 5.f; break;
        case 2: p.proj += 1; break;
        case 3: p.speed *= 1.1f; break;
        case 4: p.maxHp += 20.f; p.hp = std::min(p.maxHp, p.hp + 20.f); break;
        case 5: p.magnet += 40.f; break;
        default: break;
    }
}

void pickCard(App& app, int i) {
    World& w = app.w;
    if (!w.choosing) return;
    applyCard(w, w.cards[static_cast<std::size_t>(i)]);
    if (--w.pendingLv > 0) rollCards(w);
    else w.choosing = false;
}

// ---------- 적 ----------
void spawnEnemy(World& w) {
    int type = 0;
    if (w.time >= 120.f) {
        const float r = rf(0.f, 1.f);
        type = r < 0.6f ? 0 : (r < 0.9f ? 1 : 2);
    } else if (w.time >= 60.f) {
        type = rf(0.f, 1.f) < 0.7f ? 0 : 1;
    }
    const sf::Vector2f c = camCenter(w.p.pos);
    const float l = c.x - kViewW / 2.f - 64.f;
    const float r = c.x + kViewW / 2.f + 64.f;
    const float t = c.y - kViewH / 2.f - 64.f;
    const float b = c.y + kViewH / 2.f + 64.f;
    sf::Vector2f pos;
    switch (ri(0, 3)) {
        case 0: pos = V(rf(l, r), t); break;
        case 1: pos = V(rf(l, r), b); break;
        case 2: pos = V(l, rf(t, b)); break;
        default: pos = V(r, rf(t, b)); break;
    }
    pos.x = std::clamp(pos.x, 0.f, kWorldW);
    pos.y = std::clamp(pos.y, 0.f, kWorldH);
    Enemy e;
    e.type = type;
    e.pos = pos;
    e.hp = kTypes[static_cast<std::size_t>(type)].hp;
    w.enemies.push_back(e);
}

void killEnemy(App& app, Enemy& e) {
    World& w = app.w;
    e.dead = true;
    e.deadT = 0.1f;
    e.hitT = 0.1f;
    ++w.kills;
    Gem g;
    g.pos = e.pos;
    g.value = kTypes[static_cast<std::size_t>(e.type)].xp;
    w.gems.push_back(g);
    playSfx(app, app.a.hit);
}

// ---------- 한 고정 스텝 (1/60초) ----------
void stepWorld(App& app) {
    World& w = app.w;
    Player& p = w.p;
    constexpr float dt = kStep;
    w.time += dt;

    // 이동
    sf::Vector2f in{0.f, 0.f};
    if (sf::Keyboard::isKeyPressed(Key::A) || sf::Keyboard::isKeyPressed(Key::Left)) in.x -= 1.f;
    if (sf::Keyboard::isKeyPressed(Key::D) || sf::Keyboard::isKeyPressed(Key::Right)) in.x += 1.f;
    if (sf::Keyboard::isKeyPressed(Key::W) || sf::Keyboard::isKeyPressed(Key::Up)) in.y -= 1.f;
    if (sf::Keyboard::isKeyPressed(Key::S) || sf::Keyboard::isKeyPressed(Key::Down)) in.y += 1.f;
    p.moving = len2(in) > 0.f;
    if (in.x < 0.f) p.faceLeft = true;
    else if (in.x > 0.f) p.faceLeft = false;
    p.pos += norm(in) * (p.speed * dt);
    p.pos.x = std::clamp(p.pos.x, kPlayerR, kWorldW - kPlayerR);
    p.pos.y = std::clamp(p.pos.y, kPlayerR, kWorldH - kPlayerR);
    if (p.moving) p.animT += dt;
    p.hurtT -= dt;

    // 생성
    if (w.time >= w.nextSpawn) {
        if (w.enemies.size() < kMaxEnemies) spawnEnemy(w);
        w.nextSpawn = w.time + std::max(0.15f, 0.6f - 0.05f * std::floor(w.time / 15.f));
    }

    // 적 이동과 접촉 피해
    for (Enemy& e : w.enemies) {
        if (e.dead) {
            e.deadT -= dt;
            continue;
        }
        const EType& ty = kTypes[static_cast<std::size_t>(e.type)];
        if (e.hitT > 0.f) e.hitT -= dt;
        if (e.knockT > 0.f) {
            e.knockT -= dt;
            e.pos += e.knockV * dt;
        } else {
            e.pos += norm(p.pos - e.pos) * (ty.speed * dt);
        }
        e.pos.x = std::clamp(e.pos.x, 0.f, kWorldW);
        e.pos.y = std::clamp(e.pos.y, 0.f, kWorldH);
        if (len(p.pos - e.pos) < ty.radius + kPlayerR) {
            p.hp -= ty.dps * dt;
            p.hurtT = 0.1f;
        }
    }

    // 자동 사격
    p.fireT += dt;
    if (p.fireT >= p.fireCd) {
        const Enemy* best = nullptr;
        float bestD = 400.f * 400.f;
        for (const Enemy& e : w.enemies) {
            if (e.dead) continue;
            const float d2 = len2(e.pos - p.pos);
            if (d2 <= bestD) {
                bestD = d2;
                best = &e;
            }
        }
        if (best) {
            p.fireT = 0.f;
            const sf::Vector2f dir = norm(best->pos - p.pos);
            for (int i = 0; i < p.proj; ++i) {
                const float a = (static_cast<float>(i) - static_cast<float>(p.proj - 1) / 2.f) * (10.f * kPi / 180.f);
                const float c = std::cos(a);
                const float s = std::sin(a);
                const sf::Vector2f d(dir.x * c - dir.y * s, dir.x * s + dir.y * c);
                Bullet b;
                b.pos = p.pos;
                b.vel = d * 500.f;
                b.life = 1.2f;
                b.dmg = p.dmg;
                w.bullets.push_back(b);
            }
            playSfx(app, app.a.shoot);
        }
    }

    // 총알 이동과 충돌
    for (Bullet& b : w.bullets) {
        b.pos += b.vel * dt;
        b.life -= dt;
    }
    for (Bullet& b : w.bullets) {
        if (b.life <= 0.f) continue;
        for (Enemy& e : w.enemies) {
            if (e.dead) continue;
            const EType& ty = kTypes[static_cast<std::size_t>(e.type)];
            if (len(b.pos - e.pos) >= 4.f + ty.radius) continue;
            b.life = 0.f;
            e.hp -= b.dmg;
            e.hitT = 0.1f;
            if (e.hp <= 0.f) {
                killEnemy(app, e);
            } else {
                e.knockT = 0.1f;
                e.knockV = norm(b.vel) * 120.f;
            }
            break;
        }
    }

    // 보석 자석, 획득, 레벨업
    for (Gem& g : w.gems) {
        const sf::Vector2f d = p.pos - g.pos;
        if (len(d) <= p.magnet) g.pos += norm(d) * (300.f * dt);
        g.life -= dt;
        if (len(p.pos - g.pos) < kPlayerR + 6.f) {
            g.life = 0.f;
            w.xp += g.value;
            playSfx(app, app.a.pickup);
            while (w.xp >= 5 * w.level) {
                w.xp -= 5 * w.level;
                ++w.level;
                ++w.pendingLv;
                playSfx(app, app.a.levelup);
            }
            if (w.pendingLv > 0 && !w.choosing) {
                w.choosing = true;
                rollCards(w);
            }
        }
    }

    // 정리
    std::erase_if(w.enemies, [](const Enemy& e) { return e.dead && e.deadT <= 0.f; });
    std::erase_if(w.bullets, [](const Bullet& b) { return b.life <= 0.f; });
    std::erase_if(w.gems, [](const Gem& g) { return g.life <= 0.f; });

    // 승패
    if (p.hp <= 0.f) {
        p.hp = 0.f;
        endGame(app, false);
    } else if (w.time >= kSurviveTime) {
        w.time = kSurviveTime;
        endGame(app, true);
    }
}

// ---------- 그리기 ----------
void drawWorld(const App& app, sf::RenderWindow& win) {
    const World& w = app.w;
    win.setView(makeView(camCenter(w.p.pos)));

    sf::RectangleShape bg(V(kWorldW, kWorldH));
    bg.setFillColor(kBg);
    win.draw(bg);

    for (float x = 0.f; x <= kWorldW; x += 120.f) drawRect(win, V(x, 0.f), V(1.f, kWorldH), kGridC);
    for (float y = 0.f; y <= kWorldH; y += 120.f) drawRect(win, V(0.f, y), V(kWorldW, 1.f), kGridC);

    sf::RectangleShape border(V(kWorldW, kWorldH));
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(kBorderC);
    border.setOutlineThickness(-2.f);
    win.draw(border);

    for (const Gem& g : w.gems) {
        sf::Sprite s(app.a.gem);
        s.setOrigin(V(6.f, 6.f));
        s.setPosition(g.pos);
        s.setColor(g.value >= 5 ? kGoldC : kXpC);
        win.draw(s);
    }

    for (const Bullet& b : w.bullets) {
        sf::Sprite s(app.a.bullet);
        s.setOrigin(V(4.f, 4.f));
        s.setPosition(b.pos);
        win.draw(s);
    }

    for (const Enemy& e : w.enemies) {
        const EType& ty = kTypes[static_cast<std::size_t>(e.type)];
        const sf::Texture& tex = e.type == 0 ? app.a.walker : (e.type == 1 ? app.a.runner : app.a.brute);
        const int frame = (e.dead || e.hitT > 0.f) ? 2 : (static_cast<int>(w.time / ty.period) % 2);
        sf::Sprite s(tex);
        s.setTextureRect(sf::IntRect(sf::Vector2i(frame * ty.size, 0), sf::Vector2i(ty.size, ty.size)));
        s.setOrigin(V(ty.size / 2.f, ty.size / 2.f));
        s.setPosition(e.pos);
        if (w.p.pos.x < e.pos.x) s.setScale(V(-1.f, 1.f));
        win.draw(s);
    }

    const Player& p = w.p;
    const int pf = p.moving ? (static_cast<int>(p.animT / 0.15f) % 2) : 0;
    sf::Sprite ps(app.a.player);
    ps.setTextureRect(sf::IntRect(sf::Vector2i(pf * 32, 0), sf::Vector2i(32, 32)));
    ps.setOrigin(V(16.f, 16.f));
    ps.setPosition(p.pos);
    if (p.faceLeft) ps.setScale(V(-1.f, 1.f));
    if (p.hurtT > 0.f) ps.setColor(kHurtC);
    win.draw(ps);
}

void drawHud(const App& app, sf::RenderWindow& win) {
    const World& w = app.w;
    const sf::Font& f = app.a.font;
    const float need = 5.f * static_cast<float>(w.level);

    drawRect(win, V(0.f, 0.f), V(kViewW, 6.f), kGridC);
    drawRect(win, V(0.f, 0.f), V(kViewW * std::min(1.f, static_cast<float>(w.xp) / need), 6.f), kXpC);

    drawRect(win, V(8.f, 14.f), V(200.f, 12.f), kHpBgC);
    const float hr = std::clamp(w.p.hp / w.p.maxHp, 0.f, 1.f);
    drawRect(win, V(8.f, 14.f), V(200.f * hr, 12.f), kHpC);

    const int hpShow = static_cast<int>(std::ceil(std::max(0.f, w.p.hp)));
    win.draw(makeText(f, std::format("HP {}", hpShow), 16, V(216.f, 10.f)));
    win.draw(makeText(f, std::format("LV {}", w.level), 16, V(620.f, 10.f)));
    win.draw(makeText(f, "TIME " + timeStr(w.time), 16, V(720.f, 10.f)));
    win.draw(makeText(f, std::format("KILLS {}", w.kills), 16, V(840.f, 10.f)));
}

float cardX(int i) { return 80.f + 280.f * static_cast<float>(i); }

void drawCards(const App& app, sf::RenderWindow& win) {
    const World& w = app.w;
    const sf::Font& f = app.a.font;
    for (int i = 0; i < 3; ++i) {
        const float x = cardX(i);
        sf::RectangleShape r(V(240.f, 140.f));
        r.setPosition(V(x, kCardY));
        r.setFillColor(kGridC);
        r.setOutlineColor(kGoldC);
        r.setOutlineThickness(-2.f);
        win.draw(r);
        win.draw(makeText(f, std::to_string(i + 1), 32, V(x + 12.f, kCardY + 8.f)));
        win.draw(makeText(f, kCardNames[static_cast<std::size_t>(w.cards[static_cast<std::size_t>(i)])],
                          16, V(x + 120.f, kCardY + 70.f), true));
    }
}

} // namespace

int main() {
    App app;
    if (!loadRes(app.a.font, "assets/fonts/game.ttf")) return 1;
    if (!loadRes(app.a.player, "assets/textures/sv_player.png")) return 1;
    if (!loadRes(app.a.walker, "assets/textures/sv_walker.png")) return 1;
    if (!loadRes(app.a.runner, "assets/textures/sv_runner.png")) return 1;
    if (!loadRes(app.a.brute, "assets/textures/sv_brute.png")) return 1;
    if (!loadRes(app.a.bullet, "assets/textures/sv_bullet.png")) return 1;
    if (!loadRes(app.a.gem, "assets/textures/sv_gem.png")) return 1;
    if (!loadRes(app.a.shoot, "assets/sfx/shoot.wav")) return 1;
    if (!loadRes(app.a.hit, "assets/sfx/hit.wav")) return 1;
    if (!loadRes(app.a.pickup, "assets/sfx/pickup.wav")) return 1;
    if (!loadRes(app.a.levelup, "assets/sfx/levelup.wav")) return 1;
    if (!loadRes(app.a.bgm, "assets/music/bgm_survivor.ogg")) return 1;
    app.a.bgm.setLooping(true);
    app.a.bgm.setVolume(40.f);

    sf::RenderWindow window(sf::VideoMode({960, 540}), "LAST STAND");
    window.setFramerateLimit(60);

    sf::Clock clock;
    float acc = 0.f;

    while (window.isOpen()) {
        const float frameDt = std::min(clock.restart().asSeconds(), 0.25f);
        updateViewport(window);
        const sf::View uiView = makeView(V(kViewW / 2.f, kViewH / 2.f));

        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                World& w = app.w;
                if (k->code == Key::Escape) {
                    window.close();
                } else if (app.scene == Scene::Title && k->code == Key::Enter) {
                    startPlay(app);
                } else if (app.scene == Scene::Result && k->code == Key::R) {
                    startPlay(app);
                } else if (app.scene == Scene::Play && k->code == Key::P && !w.choosing) {
                    w.paused = !w.paused;
                } else if (app.scene == Scene::Play && w.choosing) {
                    if (k->code == Key::Num1 || k->code == Key::Numpad1) pickCard(app, 0);
                    else if (k->code == Key::Num2 || k->code == Key::Numpad2) pickCard(app, 1);
                    else if (k->code == Key::Num3 || k->code == Key::Numpad3) pickCard(app, 2);
                }
            } else if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left && app.scene == Scene::Play && app.w.choosing) {
                    const sf::Vector2f pt = window.mapPixelToCoords(m->position, uiView);
                    for (int i = 0; i < 3; ++i) {
                        const float x = cardX(i);
                        if (pt.x >= x && pt.x < x + 240.f && pt.y >= kCardY && pt.y < kCardY + 140.f) {
                            pickCard(app, i);
                            break;
                        }
                    }
                }
            }
        }
        if (!window.isOpen()) break;

        // 갱신
        if (app.scene == Scene::Title) {
            app.titleT += frameDt;
            if (app.titleT >= 2.f) startPlay(app);
        } else if (app.scene == Scene::Play) {
            if (app.w.paused || app.w.choosing) {
                acc = 0.f;
            } else {
                acc += frameDt;
                while (acc >= kStep && app.scene == Scene::Play && !app.w.paused && !app.w.choosing) {
                    acc -= kStep;
                    stepWorld(app);
                }
            }
        }

        // 그리기
        window.clear(sf::Color::Black);
        const sf::Font& font = app.a.font;
        if (app.scene == Scene::Title) {
            window.setView(uiView);
            window.draw(makeText(font, "LAST STAND", 48, V(480.f, 200.f), true));
            window.draw(makeText(font, "PRESS ENTER", 32, V(480.f, 300.f), true));
        } else {
            drawWorld(app, window);
            window.setView(uiView);
            if (app.scene == Scene::Play) {
                drawHud(app, window);
                if (app.w.choosing) drawCards(app, window);
                if (app.w.paused) window.draw(makeText(font, "PAUSED", 48, V(480.f, 270.f), true));
            } else {
                const World& w = app.w;
                window.draw(makeText(font, app.resultTitle, 48, V(480.f, 160.f), true));
                window.draw(makeText(font, "TIME " + timeStr(w.time), 32, V(480.f, 240.f), true));
                window.draw(makeText(font, std::format("LEVEL {}", w.level), 32, V(480.f, 290.f), true));
                window.draw(makeText(font, std::format("KILLS {}", w.kills), 32, V(480.f, 340.f), true));
                window.draw(makeText(font, "R: RESTART  ESC: QUIT", 16, V(480.f, 460.f), true));
            }
        }
        window.display();
    }
    return 0;
}
```