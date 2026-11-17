#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Vec = sf::Vector2f;

constexpr float kW = 960.f;
constexpr float kH = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kTopY = 476.f;      // 도시·발사대 윗변 (y 500 - 24)
constexpr float kFireMaxY = 470.f;  // 이 값보다 아래(y >= 470)를 클릭하면 발사하지 않음
constexpr float kInterceptSpeed = 600.f;
constexpr float kExplosionLife = 1.0f;
constexpr float kSpawnFirst = 1.0f;
constexpr float kSpawnGap = 1.2f;
constexpr float kClearBanner = 3.0f;
constexpr float kTitleTime = 2.0f;
constexpr int kAmmo = 10;

constexpr std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr std::array<float, 3> kBaseX{60.f, 480.f, 900.f};

const sf::Color kGroundColor(176, 122, 42);     // #B07A2A
const sf::Color kMissileTrail(255, 77, 77);     // #FF4D4D
const sf::Color kWhite(255, 255, 255);          // #FFFFFF
const sf::Color kInterceptColor(79, 195, 247);  // #4FC3F7
const sf::Color kBoomYellow(255, 213, 74);      // #FFD54A

struct Missile {
    Vec start;
    Vec head;
    Vec target;
    float speed;
    bool toCity;
    std::size_t idx;
};

struct Interceptor {
    Vec from;
    Vec pos;
    Vec target;
};

struct Explosion {
    Vec center;
    float age;
};

struct City {
    float x;
    bool destroyed;
};

struct Base {
    float x;
    int ammo;
    bool destroyed;
};

enum class Phase { Title, Play, Result };

struct Assets {
    sf::Font font;
    sf::Texture cityTex;
    sf::Texture baseTex;
    sf::Texture crossTex;
    sf::SoundBuffer launchBuf;
    sf::SoundBuffer explodeBuf;
    sf::SoundBuffer loseBuf;
    std::optional<sf::Sound> launch;
    std::optional<sf::Sound> explode;
    std::optional<sf::Sound> lose;
    sf::Music bgm;
};

struct Game {
    Phase phase = Phase::Title;
    float titleTimer = 0.f;
    int score = 0;
    int wave = 0;
    bool clearing = false;
    float clearTimer = 0.f;
    float waveTimer = 0.f;
    float nextSpawn = 0.f;
    int spawned = 0;
    int total = 0;
    std::array<City, 6> cities{};
    std::array<Base, 3> bases{};
    std::vector<Missile> missiles;
    std::vector<Interceptor> shots;
    std::vector<Explosion> booms;
    std::mt19937 rng{std::random_device{}()};
};

bool fail(const char* path) {
    std::cerr << "failed to load: " << path << '\n';
    return false;
}

bool loadAssets(Assets& a) {
    if (!a.font.openFromFile("assets/fonts/game.ttf")) return fail("assets/fonts/game.ttf");
    if (!a.cityTex.loadFromFile("assets/textures/md_city.png")) return fail("assets/textures/md_city.png");
    if (!a.baseTex.loadFromFile("assets/textures/md_base.png")) return fail("assets/textures/md_base.png");
    if (!a.crossTex.loadFromFile("assets/textures/md_crosshair.png")) return fail("assets/textures/md_crosshair.png");
    if (!a.launchBuf.loadFromFile("assets/sfx/launch.wav")) return fail("assets/sfx/launch.wav");
    if (!a.explodeBuf.loadFromFile("assets/sfx/explode.wav")) return fail("assets/sfx/explode.wav");
    if (!a.loseBuf.loadFromFile("assets/sfx/lose.wav")) return fail("assets/sfx/lose.wav");
    if (!a.bgm.openFromFile("assets/music/bgm_missile.ogg")) return fail("assets/music/bgm_missile.ogg");

    a.launch.emplace(a.launchBuf);
    a.explode.emplace(a.explodeBuf);
    a.lose.emplace(a.loseBuf);
    return true;
}

void playSound(std::optional<sf::Sound>& s) {
    if (s) {
        s->stop();
        s->play();
    }
}

float boomRadius(float age) {
    if (age < 0.f) return 0.f;
    if (age < 0.5f) return 96.f * age;   // 0 -> 48px
    if (age < 1.0f) return 96.f * (1.f - age);  // 48 -> 0px
    return 0.f;
}

void addBoom(Game& g, Assets& a, Vec p) {
    g.booms.push_back(Explosion{p, 0.f});
    playSound(a.explode);
}

bool citiesAlive(const Game& g) {
    for (const City& c : g.cities)
        if (!c.destroyed) return true;
    return false;
}

float missileSpeed(const Game& g) {
    return 70.f + 10.f * static_cast<float>(g.wave - 1);
}

void startWave(Game& g) {
    ++g.wave;
    for (Base& b : g.bases) {
        b.ammo = kAmmo;
        b.destroyed = false;
    }
    g.spawned = 0;
    g.total = 10 + 2 * (g.wave - 1);
    g.waveTimer = 0.f;
    g.nextSpawn = kSpawnFirst;
    g.clearing = false;
    g.clearTimer = 0.f;
}

void spawnMissile(Game& g) {
    struct Cand {
        bool city;
        std::size_t idx;
    };
    std::vector<Cand> cand;
    for (std::size_t i = 0; i < g.cities.size(); ++i)
        if (!g.cities[i].destroyed) cand.push_back(Cand{true, i});
    for (std::size_t i = 0; i < g.bases.size(); ++i)
        if (!g.bases[i].destroyed) cand.push_back(Cand{false, i});
    if (cand.empty()) return;

    std::uniform_int_distribution<std::size_t> pick(0, cand.size() - 1);
    const Cand c = cand[pick(g.rng)];
    const float tx = c.city ? g.cities[c.idx].x : g.bases[c.idx].x;

    std::uniform_real_distribution<float> sx(0.f, kW);
    const float x = sx(g.rng);

    g.missiles.push_back(Missile{Vec{x, 0.f}, Vec{x, 0.f}, Vec{tx, kTopY},
                                 missileSpeed(g), c.city, c.idx});
}

void hitTarget(Game& g, Assets& a, const Missile& m) {
    addBoom(g, a, m.target);
    if (m.toCity) {
        City& c = g.cities[m.idx];
        if (!c.destroyed) {
            c.destroyed = true;
            playSound(a.lose);
        }
    } else {
        Base& b = g.bases[m.idx];
        if (!b.destroyed) {
            b.destroyed = true;
            playSound(a.lose);
        }
    }
}

void goResult(Game& g, Assets& a) {
    g.phase = Phase::Result;
    a.bgm.stop();
}

void startPlay(Game& g, Assets& a) {
    g.phase = Phase::Play;
    g.score = 0;
    g.wave = 0;
    g.titleTimer = 0.f;
    for (std::size_t i = 0; i < g.cities.size(); ++i) g.cities[i] = City{kCityX[i], false};
    for (std::size_t i = 0; i < g.bases.size(); ++i) g.bases[i] = Base{kBaseX[i], kAmmo, false};
    g.missiles.clear();
    g.shots.clear();
    g.booms.clear();
    startWave(g);

    a.bgm.setVolume(40.f);
    a.bgm.setLooping(true);
    a.bgm.play();
}

void updatePlay(Game& g, Assets& a, float dt, const std::vector<Vec>& clicks) {
    // 1) 발사
    if (!g.clearing) {
        for (const Vec& c : clicks) {
            if (c.y >= kFireMaxY) continue;

            int best = -1;
            float bestD = 0.f;
            for (std::size_t i = 0; i < g.bases.size(); ++i) {
                const Base& bs = g.bases[i];
                if (bs.destroyed || bs.ammo <= 0) continue;
                const float d = std::fabs(bs.x - c.x);
                if (best < 0 || d < bestD) {  // 같으면 먼저 나온(왼쪽) 발사대 유지
                    best = static_cast<int>(i);
                    bestD = d;
                }
            }
            if (best < 0) continue;

            Base& bs = g.bases[static_cast<std::size_t>(best)];
            --bs.ammo;
            const Vec from{bs.x, kTopY};
            g.shots.push_back(Interceptor{from, from, c});
            playSound(a.launch);
        }
    }

    // 2) 요격탄 이동
    for (std::size_t i = 0; i < g.shots.size();) {
        const Vec d = g.shots[i].target - g.shots[i].pos;
        const float dist = std::hypot(d.x, d.y);
        const float step = kInterceptSpeed * dt;
        if (dist <= step) {
            const Vec target = g.shots[i].target;
            g.shots.erase(g.shots.begin() + static_cast<std::ptrdiff_t>(i));
            addBoom(g, a, target);
        } else {
            g.shots[i].pos += d * (step / dist);
            ++i;
        }
    }

    // 3) 미사일 스폰
    g.waveTimer += dt;
    while (g.spawned < g.total && g.waveTimer >= g.nextSpawn && citiesAlive(g)) {
        spawnMissile(g);
        ++g.spawned;
        g.nextSpawn += kSpawnGap;
    }

    // 4) 미사일 이동과 목표 도달
    for (std::size_t i = 0; i < g.missiles.size();) {
        Missile& m = g.missiles[i];
        const Vec d = m.target - m.head;
        const float dist = std::hypot(d.x, d.y);
        const float step = m.speed * dt;
        if (dist <= step) {
            const Missile arrived = m;
            g.missiles.erase(g.missiles.begin() + static_cast<std::ptrdiff_t>(i));
            hitTarget(g, a, arrived);
        } else {
            m.head += d * (step / dist);
            ++i;
        }
    }

    // 5) 폭발 진행과 연쇄 판정
    for (Explosion& e : g.booms) e.age += dt;
    for (std::size_t e = 0; e < g.booms.size(); ++e) {
        const Vec c = g.booms[e].center;
        const float r = boomRadius(g.booms[e].age);
        for (std::size_t m = 0; m < g.missiles.size();) {
            const Vec d = g.missiles[m].head - c;
            if (std::hypot(d.x, d.y) <= r) {
                const Vec p = g.missiles[m].head;
                g.missiles.erase(g.missiles.begin() + static_cast<std::ptrdiff_t>(m));
                g.score += 25;
                addBoom(g, a, p);  // 연쇄 폭발은 뒤쪽 루프에서 같은 규칙으로 처리됨
            } else {
                ++m;
            }
        }
    }
    std::erase_if(g.booms, [](const Explosion& x) { return x.age >= kExplosionLife; });

    // 6) 상태 판정
    const bool empty = g.missiles.empty() && g.shots.empty() && g.booms.empty();
    const bool alive = citiesAlive(g);
    if (!alive && empty) {
        goResult(g, a);
        return;
    }
    if (!g.clearing && alive && g.spawned >= g.total && empty) {
        int bonus = 0;
        for (const Base& b : g.bases)
            if (!b.destroyed) bonus += 5 * b.ammo;
        for (const City& c : g.cities)
            if (!c.destroyed) bonus += 100;
        g.score += bonus;
        g.clearing = true;
        g.clearTimer = 0.f;
    }
    if (g.clearing) {
        g.clearTimer += dt;
        if (g.clearTimer >= kClearBanner) startWave(g);
    }
}

void update(Game& g, Assets& a, float dt, std::vector<Vec>& clicks, bool enter, bool rKey) {
    switch (g.phase) {
        case Phase::Title:
            clicks.clear();
            g.titleTimer += dt;
            if (enter || g.titleTimer >= kTitleTime) startPlay(g, a);
            break;
        case Phase::Play:
            updatePlay(g, a, dt, clicks);
            clicks.clear();
            break;
        case Phase::Result:
            clicks.clear();
            if (rKey) startPlay(g, a);
            break;
    }
}

void updateView(sf::View& view, sf::Vector2u size) {
    if (size.x == 0 || size.y == 0) return;
    const float scale = std::min(static_cast<float>(size.x) / kW, static_cast<float>(size.y) / kH);
    const float vw = kW * scale / static_cast<float>(size.x);
    const float vh = kH * scale / static_cast<float>(size.y);
    view.setViewport(sf::FloatRect({(1.f - vw) / 2.f, (1.f - vh) / 2.f}, {vw, vh}));
}

void drawText(sf::RenderTarget& t, const sf::Font& f, const std::string& s,
              unsigned int size, Vec pos, bool centered) {
    sf::Text text(f, sf::String(std::string_view(s)), size);
    if (centered) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    }
    text.setPosition(pos);
    t.draw(text);
}

void drawLine(sf::RenderTarget& t, Vec a, Vec b, float thick, sf::Color color) {
    const Vec d = b - a;
    const float len = std::hypot(d.x, d.y);
    if (len <= 0.f) return;
    sf::RectangleShape r(sf::Vector2f(len, thick));
    r.setOrigin({0.f, thick / 2.f});
    r.setPosition(a);
    r.setRotation(sf::radians(std::atan2(d.y, d.x)));
    r.setFillColor(color);
    t.draw(r);
}

void drawPlay(sf::RenderTarget& t, const Game& g, const Assets& a, Vec mouse) {
    sf::RectangleShape ground(sf::Vector2f(kW, 40.f));
    ground.setPosition({0.f, kGroundY});
    ground.setFillColor(kGroundColor);
    t.draw(ground);

    for (const City& c : g.cities) {
        if (c.destroyed) continue;
        sf::Sprite s(a.cityTex);
        s.setPosition({c.x - 24.f, kTopY});
        t.draw(s);
    }
    for (const Base& b : g.bases) {
        if (b.destroyed) continue;
        sf::Sprite s(a.baseTex);
        s.setPosition({b.x - 24.f, kTopY});
        t.draw(s);
        drawText(t, a.font, std::to_string(b.ammo), 16, Vec{b.x, 514.f}, true);
    }

    for (const Missile& m : g.missiles) {
        drawLine(t, m.start, m.head, 2.f, kMissileTrail);
        sf::RectangleShape head(sf::Vector2f(4.f, 4.f));
        head.setPosition({m.head.x - 2.f, m.head.y - 2.f});
        head.setFillColor(kWhite);
        t.draw(head);
    }

    for (const Interceptor& s : g.shots)
        drawLine(t, s.from, s.pos, 2.f, kInterceptColor);

    for (const Explosion& e : g.booms) {
        const float r = boomRadius(e.age);
        if (r <= 0.f) continue;
        sf::CircleShape c(r);
        c.setOrigin({r, r});
        c.setPosition(e.center);
        const int phase = static_cast<int>(std::floor(e.age / 0.1f)) % 2;
        c.setFillColor(phase == 0 ? kWhite : kBoomYellow);
        t.draw(c);
    }

    drawText(t, a.font, "SCORE " + std::to_string(g.score), 16, Vec{10.f, 10.f}, false);
    drawText(t, a.font, "WAVE " + std::to_string(g.wave), 16, Vec{kW - 120.f, 10.f}, false);
    if (g.clearing)
        drawText(t, a.font, "WAVE " + std::to_string(g.wave) + " CLEAR", 32,
                 Vec{kW / 2.f, 240.f}, true);

    sf::Sprite cross(a.crossTex);
    cross.setPosition({mouse.x - 12.f, mouse.y - 12.f});
    t.draw(cross);
}

void draw(sf::RenderTarget& t, const Game& g, const Assets& a, Vec mouse) {
    switch (g.phase) {
        case Phase::Title:
            drawText(t, a.font, "SKY SHIELD", 48, Vec{kW / 2.f, 200.f}, true);
            drawText(t, a.font, "PRESS ENTER", 16, Vec{kW / 2.f, 300.f}, true);
            break;
        case Phase::Play:
            drawPlay(t, g, a, mouse);
            break;
        case Phase::Result:
            drawText(t, a.font, "GAME OVER", 48, Vec{kW / 2.f, 180.f}, true);
            drawText(t, a.font, "SCORE " + std::to_string(g.score), 32, Vec{kW / 2.f, 260.f}, true);
            drawText(t, a.font, "WAVE " + std::to_string(g.wave), 32, Vec{kW / 2.f, 320.f}, true);
            drawText(t, a.font, "R: RESTART  ESC: QUIT", 16, Vec{kW / 2.f, 430.f}, true);
            break;
    }
}

}  // namespace

int main() {
    Assets assets;
    if (!loadAssets(assets)) return 1;

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
    window.setFramerateLimit(60);

    sf::View view;
    view.setCenter({kW / 2.f, kH / 2.f});
    view.setSize({kW, kH});
    updateView(view, window.getSize());

    Game game;
    std::vector<Vec> clicks;
    sf::Clock clock;

    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();
        bool enter = false;
        bool rKey = false;

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape)
                    window.close();
                else if (key->code == sf::Keyboard::Key::Enter)
                    enter = true;
                else if (key->code == sf::Keyboard::Key::R)
                    rKey = true;
            } else if (const auto* res = event->getIf<sf::Event::Resized>()) {
                updateView(view, res->size);
            } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    const Vec p = window.mapPixelToCoords(mb->position, view);
                    if (p.x >= 0.f && p.x < kW && p.y >= 0.f && p.y < kH)
                        clicks.push_back(p);
                }
            }
        }
        if (!window.isOpen()) break;

        update(game, assets, dt, clicks, enter, rKey);

        const Vec mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), view);
        window.clear(sf::Color::Black);
        window.setView(view);
        draw(window, game, assets, mouse);
        window.display();
    }

    return 0;
}
