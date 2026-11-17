```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

using meno::Vec2f;

constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kPlayerR = 14.f;
constexpr float kBulletR = 4.f;
constexpr float kGemR = 6.f;
constexpr float kBulletSpeed = 500.f;
constexpr float kBulletLife = 1.2f;
constexpr float kHitTime = 0.1f;
constexpr float kKnockSpeed = 120.f;
constexpr float kGemLife = 30.f;
constexpr float kGemSpeed = 300.f;
constexpr float kAimRange = 400.f;
constexpr float kPlayerSpeed = 200.f;
constexpr float kBaseFire = 0.8f;
constexpr double kSurviveTime = 300.0;
constexpr std::size_t kMaxEnemies = 400;
constexpr float kPi = std::numbers::pi_v<float>;

struct Kind {
    float hp;
    float radius;
    float speed;
    float dps;
    int cell;
    float period;
};
constexpr Kind kKinds[3] = {
    {10.f, 10.f, 80.f, 10.f, 24, 0.3f},
    {6.f, 10.f, 140.f, 6.f, 24, 0.15f},
    {60.f, 20.f, 50.f, 20.f, 48, 0.4f},
};

constexpr const char* kCards[6] = {"FIRE RATE +15%", "DAMAGE +5",  "PROJECTILE +1",
                                   "SPEED +10%",     "MAX HP +20", "MAGNET +40"};

struct Enemy {
    int kind = 0;
    Vec2f pos{0.f, 0.f};
    float hp = 0.f;
    float hitTimer = 0.f;
    bool dying = false;
    float dieTimer = 0.f;
    Vec2f knock{0.f, 0.f};
};

struct Bullet {
    Vec2f pos{0.f, 0.f};
    Vec2f dir{0.f, 0.f};
    float damage = 10.f;
    float age = 0.f;
    bool dead = false;
};

struct Gem {
    Vec2f pos{0.f, 0.f};
    int value = 1;
    float life = kGemLife;
    bool dead = false;
};

struct Run {
    Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    float flash = 0.f;
    float animTimer = 0.f;
    int frame = 0;
    bool facingLeft = false;
    float speed = kPlayerSpeed;
    float fireInterval = kBaseFire;
    float fireTimer = 0.f;
    int projectiles = 1;
    float damage = 10.f;
    float magnet = 60.f;
    int xp = 0;
    int level = 1;
    int pending = 0;
    bool choosing = false;
    bool paused = false;
    int cards[3] = {0, 1, 2};
    double playTime = 0.0;
    double nextSpawn = 0.5;
    int kills = 0;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};

std::mt19937 gRng{std::random_device{}()};

float rnd01() {
    return std::uniform_real_distribution<float>{0.f, 1.f}(gRng);
}

float dist2(Vec2f a, Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Vec2f unitTo(Vec2f from, Vec2f to) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float l = std::sqrt(dx * dx + dy * dy);
    if (l <= 0.f) {
        return Vec2f{0.f, 0.f};
    }
    return Vec2f{dx / l, dy / l};
}

meno::Rectf cardRect(int i) {
    return meno::Rectf{{80.f + 280.f * static_cast<float>(i), 200.f}, {240.f, 140.f}};
}

meno::TextParams textParams(int size, meno::Color color = meno::colors::White) {
    meno::TextParams p{};
    p.characterSize = static_cast<decltype(p.characterSize)>(size);
    p.color = color;
    return p;
}

void drawCentered(meno::Renderer& rd, const meno::Font& font, const std::string& text,
                  Vec2f center, int size, meno::Color color = meno::colors::White) {
    meno::TextParams p = textParams(size, color);
    p.origin = rd.measureTextBounds(font, text, p).center();
    rd.drawText(font, text, center, p);
}

void drawSprite(meno::Renderer& rd, const meno::Texture& tex, Vec2f pos, Vec2f origin,
                meno::Recti src, bool flip, meno::Color tint) {
    meno::SpriteParams p{};
    p.position = pos;
    p.origin = origin;
    p.source = src;
    p.flipX = flip;
    p.tint = tint;
    rd.draw(tex, p);
}

std::string timeText(double seconds) {
    const int s = static_cast<int>(seconds);
    return std::format("TIME {:02}:{:02}", s / 60, s % 60);
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "LAST STAND"}};

    // ---- 에셋 로드: 하나라도 없으면 경로를 출력하고 종료 ----
    std::vector<std::string> missing;
    const auto loadTex = [&](const char* path) {
        auto t = game.resources().loadTexture(path);
        if (!t) {
            missing.emplace_back(path);
        }
        return t;
    };
    const auto loadBuf = [&](const char* path) {
        auto b = game.resources().loadSoundBuffer(path);
        if (!b) {
            missing.emplace_back(path);
        }
        return b;
    };
    const auto playerTex = loadTex("assets/textures/sv_player.png");
    const auto walkerTex = loadTex("assets/textures/sv_walker.png");
    const auto runnerTex = loadTex("assets/textures/sv_runner.png");
    const auto bruteTex = loadTex("assets/textures/sv_brute.png");
    const auto bulletTex = loadTex("assets/textures/sv_bullet.png");
    const auto gemTex = loadTex("assets/textures/sv_gem.png");
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    if (!font) {
        missing.emplace_back("assets/fonts/game.ttf");
    }
    const auto shootBuf = loadBuf("assets/sfx/shoot.wav");
    const auto hitBuf = loadBuf("assets/sfx/hit.wav");
    const auto pickupBuf = loadBuf("assets/sfx/pickup.wav");
    const auto levelBuf = loadBuf("assets/sfx/levelup.wav");
    std::optional<meno::Music> bgm =
        meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
    if (!bgm) {
        missing.emplace_back("assets/music/bgm_survivor.ogg");
    }
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing file: " << path << '\n';
        }
        return 1;
    }

    meno::Sound shootSfx{shootBuf};
    meno::Sound hitSfx{hitBuf};
    meno::Sound pickupSfx{pickupBuf};
    meno::Sound levelSfx{levelBuf};
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    const std::shared_ptr<const meno::Texture> kindTex[3] = {walkerTex, runnerTex, bruteTex};
    const meno::Font& fnt = *font;
    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};

    Run r;
    float titleTime = 0.f;
    bool survived = false;

    const auto cameraCenter = [&]() -> Vec2f {
        return Vec2f{std::clamp(r.player.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                     std::clamp(r.player.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
    };

    const auto rollCards = [&] {
        std::array<int, 6> order{0, 1, 2, 3, 4, 5};
        std::shuffle(order.begin(), order.end(), gRng);
        r.cards[0] = order[0];
        r.cards[1] = order[1];
        r.cards[2] = order[2];
    };

    const auto applyCard = [&](int id) {
        switch (id) {
            case 0: r.fireInterval *= 0.85f; break;
            case 1: r.damage += 5.f; break;
            case 2: r.projectiles += 1; break;
            case 3: r.speed *= 1.1f; break;
            case 4: r.maxHp += 20.f; r.hp += 20.f; break;
            default: r.magnet += 40.f; break;
        }
    };

    const auto spawnEnemy = [&] {
        const Vec2f cam = cameraCenter();
        const float x0 = cam.x - kViewW / 2.f - 64.f;
        const float y0 = cam.y - kViewH / 2.f - 64.f;
        const float w = kViewW + 128.f;
        const float h = kViewH + 128.f;
        const float t = rnd01() * 2.f * (w + h);
        Vec2f p{0.f, 0.f};
        if (t < w) {
            p = Vec2f{x0 + t, y0};
        } else if (t < w + h) {
            p = Vec2f{x0 + w, y0 + (t - w)};
        } else if (t < 2.f * w + h) {
            p = Vec2f{x0 + w - (t - w - h), y0 + h};
        } else {
            p = Vec2f{x0, y0 + h - (t - 2.f * w - h)};
        }
        p.x = std::clamp(p.x, 0.f, kWorldW);
        p.y = std::clamp(p.y, 0.f, kWorldH);

        const float tm = static_cast<float>(r.playTime);
        const float roll = rnd01();
        int kind = 0;
        if (tm < 60.f) {
            kind = 0;
        } else if (tm < 120.f) {
            kind = roll < 0.7f ? 0 : 1;
        } else {
            kind = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
        }
        Enemy e;
        e.kind = kind;
        e.pos = p;
        e.hp = kKinds[kind].hp;
        r.enemies.push_back(e);
    };

    // 한 틱의 플레이 로직
    const auto stepPlay = [&](float dt) {
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (r.choosing) {
            const Vec2f mouse = meno::screenToWorld(hud, game.renderer().framebufferSize(),
                                                    Vec2f{in.mousePosition()});
            int pick = -1;
            if (in.wasPressed(meno::Key::Num1)) pick = 0;
            if (in.wasPressed(meno::Key::Num2)) pick = 1;
            if (in.wasPressed(meno::Key::Num3)) pick = 2;
            if (in.wasPressed(meno::MouseButton::Left)) {
                for (int i = 0; i < 3; ++i) {
                    if (cardRect(i).contains(mouse)) {
                        pick = i;
                    }
                }
            }
            if (pick >= 0) {
                applyCard(r.cards[pick]);
                if (--r.pending > 0) {
                    rollCards();
                } else {
                    r.choosing = false;
                }
            }
            return;
        }

        if (in.wasPressed(meno::Key::P)) {
            r.paused = !r.paused;
        }
        if (r.paused) {
            return;
        }

        r.playTime += dt;

        // 플레이어 이동 (8방향, 대각선도 같은 속력)
        float dx = 0.f;
        float dy = 0.f;
        if (in.isDown(meno::Key::Left) || in.isDown(meno::Key::A)) dx -= 1.f;
        if (in.isDown(meno::Key::Right) || in.isDown(meno::Key::D)) dx += 1.f;
        if (in.isDown(meno::Key::Up) || in.isDown(meno::Key::W)) dy -= 1.f;
        if (in.isDown(meno::Key::Down) || in.isDown(meno::Key::S)) dy += 1.f;
        const float dl = std::sqrt(dx * dx + dy * dy);
        if (dl > 0.f) {
            dx /= dl;
            dy /= dl;
        }
        r.player.x = std::clamp(r.player.x + dx * r.speed * dt, 0.f, kWorldW);
        r.player.y = std::clamp(r.player.y + dy * r.speed * dt, 0.f, kWorldH);
        if (dx < 0.f) {
            r.facingLeft = true;
        } else if (dx > 0.f) {
            r.facingLeft = false;
        }
        if (dl > 0.f) {
            r.animTimer = std::fmod(r.animTimer + dt, 0.3f);
            r.frame = r.animTimer < 0.15f ? 0 : 1;
        } else {
            r.animTimer = 0.f;
            r.frame = 0;
        }
        r.flash = std::max(0.f, r.flash - dt);

        // 적 생성
        if (r.playTime >= r.nextSpawn) {
            const double interval =
                std::max(0.15, 0.6 - 0.05 * std::floor(r.playTime / 15.0));
            r.nextSpawn += interval;
            if (r.enemies.size() < kMaxEnemies) {
                spawnEnemy();
            }
        }

        // 적 이동과 접촉 피해
        for (Enemy& e : r.enemies) {
            const Kind& k = kKinds[e.kind];
            if (e.dying) {
                e.dieTimer -= dt;
                continue;
            }
            if (e.hitTimer > 0.f) {
                e.pos.x += e.knock.x * kKnockSpeed * dt;
                e.pos.y += e.knock.y * kKnockSpeed * dt;
                e.hitTimer -= dt;
            } else {
                const Vec2f d = unitTo(e.pos, r.player);
                e.pos.x += d.x * k.speed * dt;
                e.pos.y += d.y * k.speed * dt;
            }
            e.pos.x = std::clamp(e.pos.x, 0.f, kWorldW);
            e.pos.y = std::clamp(e.pos.y, 0.f, kWorldH);
            const float rr = k.radius + kPlayerR;
            if (dist2(e.pos, r.player) < rr * rr) {
                r.hp -= k.dps * dt;
                r.flash = kHitTime;
            }
        }

        // 자동 사격
        r.fireTimer += dt;
        const Enemy* target = nullptr;
        float best = kAimRange * kAimRange;
        for (const Enemy& e : r.enemies) {
            if (e.dying) {
                continue;
            }
            const float d2 = dist2(e.pos, r.player);
            if (d2 < best) {
                best = d2;
                target = &e;
            }
        }
        if (target != nullptr && r.fireTimer >= r.fireInterval) {
            r.fireTimer = 0.f;
            const Vec2f base = unitTo(r.player, target->pos);
            const int n = r.projectiles;
            for (int i = 0; i < n; ++i) {
                const float a =
                    (static_cast<float>(i) - static_cast<float>(n - 1) / 2.f) * (10.f * kPi / 180.f);
                const float c = std::cos(a);
                const float s = std::sin(a);
                Bullet b;
                b.pos = r.player;
                b.dir = Vec2f{base.x * c - base.y * s, base.x * s + base.y * c};
                b.damage = r.damage;
                r.bullets.push_back(b);
            }
            shootSfx.play();
        }

        // 총알 이동과 피격
        for (Bullet& b : r.bullets) {
            b.pos.x += b.dir.x * kBulletSpeed * dt;
            b.pos.y += b.dir.y * kBulletSpeed * dt;
            b.age += dt;
            if (b.age >= kBulletLife) {
                b.dead = true;
                continue;
            }
            for (Enemy& e : r.enemies) {
                if (e.dying) {
                    continue;
                }
                const float rr = kKinds[e.kind].radius + kBulletR;
                if (dist2(b.pos, e.pos) >= rr * rr) {
                    continue;
                }
                b.dead = true;
                e.hp -= b.damage;
                e.hitTimer = kHitTime;
                e.knock = b.dir;
                if (e.hp <= 0.f) {
                    e.dying = true;
                    e.dieTimer = kHitTime;
                    Gem g;
                    g.pos = e.pos;
                    g.value = e.kind == 2 ? 5 : 1;
                    r.gems.push_back(g);
                    hitSfx.play();
                    ++r.kills;
                }
                break;
            }
        }
        std::erase_if(r.bullets, [](const Bullet& b) { return b.dead; });
        std::erase_if(r.enemies, [](const Enemy& e) { return e.dying && e.dieTimer <= 0.f; });

        // 보석: 자석 범위 안이면 날아오고, 닿으면 경험치
        int gained = 0;
        for (Gem& g : r.gems) {
            g.life -= dt;
            if (dist2(g.pos, r.player) <= r.magnet * r.magnet) {
                const Vec2f d = unitTo(g.pos, r.player);
                g.pos.x += d.x * kGemSpeed * dt;
                g.pos.y += d.y * kGemSpeed * dt;
            }
            const float rr = kPlayerR + kGemR;
            if (dist2(g.pos, r.player) < rr * rr) {
                g.dead = true;
                gained += g.value;
            }
            if (g.life <= 0.f) {
                g.dead = true;
            }
        }
        std::erase_if(r.gems, [](const Gem& g) { return g.dead; });
        if (gained > 0) {
            pickupSfx.play();
            r.xp += gained;
            bool leveled = false;
            while (r.xp >= 5 * r.level) {
                r.xp -= 5 * r.level;
                ++r.level;
                ++r.pending;
                leveled = true;
            }
            if (leveled) {
                levelSfx.play();
                r.choosing = true;
                rollCards();
            }
        }

        // 승패 판정
        if (r.hp <= 0.f) {
            survived = false;
            (void)game.scenes().changeScene(game.scenes().createScene().id());
            return;
        }
    };

    meno::Scene& title = game.scenes().createScene();
    meno::Scene& play = game.scenes().createScene();
    meno::Scene& result = game.scenes().createScene();
    meno::SceneManager& scenes = game.scenes();

    // ---- 타이틀 ----
    title.setUpdateHandler([&](double dt) {
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTime += static_cast<float>(dt);
        if (in.wasPressed(meno::Key::Enter) || titleTime >= 2.f) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& rd) {
        rd.setCamera(hud);
        rd.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
        drawCentered(rd, fnt, "LAST STAND", {kViewW / 2.f, 200.f}, 48);
        drawCentered(rd, fnt, "PRESS ENTER", {kViewW / 2.f, 320.f}, 32);
    });

    // ---- 플레이 ----
    play.setEnterHandler([&] {
        r = Run{};
        survived = false;
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double dt) { stepPlay(static_cast<float>(dt)); });
    play.setDrawHandler([&](meno::Renderer& rd) {
        const Vec2f c = cameraCenter();
        rd.setCamera(meno::Camera2D{.center = c, .size = {kViewW, kViewH}});
        rd.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = meno::Color::fromRgb(0x1A1420)});

        // 격자선(화면에 보이는 범위만)
        const meno::Rectf vis = rd.visibleWorldBounds();
        const meno::Color gridColor = meno::Color::fromRgb(0x2A2233);
        for (float x = std::max(0.f, std::floor(vis.left() / 120.f) * 120.f);
             x <= std::min(kWorldW, vis.right()); x += 120.f) {
            rd.drawLine({x, std::max(0.f, vis.top())}, {x, std::min(kWorldH, vis.bottom())},
                        {.color = gridColor, .thickness = 1.f});
        }
        for (float y = std::max(0.f, std::floor(vis.top() / 120.f) * 120.f);
             y <= std::min(kWorldH, vis.bottom()); y += 120.f) {
            rd.drawLine({std::max(0.f, vis.left()), y}, {std::min(kWorldW, vis.right()), y},
                        {.color = gridColor, .thickness = 1.f});
        }
        rd.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                    {.fill = meno::colors::Transparent,
                     .outline = meno::Color::fromRgb(0x6B5B7B),
                     .outlineThickness = 2.f});

        // 보석
        for (const Gem& g : r.gems) {
            drawSprite(rd, *gemTex, g.pos, {6.f, 6.f}, meno::Recti{0, 0, 12, 12}, false,
                       g.value > 1 ? meno::Color::fromRgb(0xFFD54A)
                                   : meno::Color::fromRgb(0x4FC3F7));
        }

        // 적
        const float t = static_cast<float>(r.playTime);
        for (const Enemy& e : r.enemies) {
            const Kind& k = kKinds[e.kind];
            const int frame = (e.dying || e.hitTimer > 0.f)
                                  ? 2
                                  : static_cast<int>(t / k.period) % 2;
            const int cell = k.cell;
            drawSprite(rd, *kindTex[e.kind], e.pos, {k.cell / 2.f, k.cell / 2.f},
                       meno::Recti{frame * cell, 0, cell, cell}, r.player.x < e.pos.x,
                       meno::colors::White);
        }

        // 총알
        for (const Bullet& b : r.bullets) {
            drawSprite(rd, *bulletTex, b.pos, {4.f, 4.f}, meno::Recti{0, 0, 8, 8}, false,
                       meno::colors::White);
        }

        // 플레이어
        drawSprite(rd, *playerTex, r.player, {16.f, 16.f},
                   meno::Recti{r.frame * 32, 0, 32, 32}, r.facingLeft,
                   r.flash > 0.f ? meno::Color::fromRgb(0xFF6060) : meno::colors::White);

        // HUD (화면 고정)
        rd.setCamera(hud);
        rd.drawRect({{0.f, 0.f}, {kViewW, 6.f}}, {.fill = meno::Color::fromRgb(0x2A2233)});
        const float need = 5.f * static_cast<float>(r.level);
        const float xpFrac = std::clamp(static_cast<float>(r.xp) / need, 0.f, 1.f);
        rd.drawRect({{0.f, 0.f}, {kViewW * xpFrac, 6.f}}, {.fill = meno::Color::fromRgb(0x4FC3F7)});

        const float hpFrac = std::clamp(r.hp / r.maxHp, 0.f, 1.f);
        rd.drawRect({{16.f, 14.f}, {200.f, 12.f}}, {.fill = meno::Color::fromRgb(0x3A1F24)});
        rd.drawRect({{16.f, 14.f}, {200.f * hpFrac, 12.f}}, {.fill = meno::Color::fromRgb(0xE5533D)});
        const std::string hpText =
            std::format("HP {}", static_cast<int>(std::ceil(std::max(0.f, r.hp))));
        rd.drawText(fnt, hpText, {228.f, 12.f}, textParams(16));

        const std::string lines[3] = {std::format("LV {}", r.level), timeText(r.playTime),
                                      std::format("KILLS {}", r.kills)};
        for (int i = 0; i < 3; ++i) {
            const float w = rd.measureTextBounds(fnt, lines[i], textParams(16)).size.x;
            rd.drawText(fnt, lines[i], {944.f - w, 14.f + 18.f * static_cast<float>(i)},
                        textParams(16));
        }

        if (r.choosing) {
            for (int i = 0; i < 3; ++i) {
                const meno::Rectf card = cardRect(i);
                rd.drawRect(card, {.fill = meno::Color::fromRgb(0x2A2233),
                                   .outline = meno::Color::fromRgb(0xFFD54A),
                                   .outlineThickness = 2.f});
                drawCentered(rd, fnt, kCards[r.cards[i]], card.center(), 16);
            }
        }
        if (r.paused) {
            drawCentered(rd, fnt, "PAUSED", {kViewW / 2.f, kViewH / 2.f}, 32);
        }
    });

    // ---- 결과 ----
    result.setUpdateHandler([&](double) {
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game.stop();
        } else if (in.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& rd) {
        rd.setCamera(hud);
        rd.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
        drawCentered(rd, fnt, survived ? "SURVIVED" : "GAME OVER", {kViewW / 2.f, 120.f}, 48);
        drawCentered(rd, fnt, timeText(r.playTime), {kViewW / 2.f, 230.f}, 32);
        drawCentered(rd, fnt, std::format("LEVEL {}", r.level), {kViewW / 2.f, 280.f}, 32);
        drawCentered(rd, fnt, std::format("KILLS {}", r.kills), {kViewW / 2.f, 330.f}, 32);
        drawCentered(rd, fnt, "R: RESTART  ESC: QUIT", {kViewW / 2.f, 480.f}, 16);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```