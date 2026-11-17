```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kPi = 3.14159265358979f;
constexpr double kEps = 1e-9;

constexpr float kPlayerRadius = 14.f;
constexpr float kBulletRadius = 4.f;
constexpr float kGemRadius = 6.f;

struct EnemyType {
    float radius;
    float hp;
    float speed;
    float dps;
    int cell;
    double framePeriod;
    int xp;
};

const std::array<EnemyType, 3> kTypes{{
    {10.f, 10.f, 80.f, 10.f, 24, 0.3, 1},  // 걷는 적
    {10.f, 6.f, 140.f, 6.f, 24, 0.15, 1},  // 뛰는 적
    {20.f, 60.f, 50.f, 20.f, 48, 0.4, 5},  // 큰 적
}};

const std::array<const char*, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40",
};

struct Enemy {
    int type = 0;
    meno::Vec2f pos{0.f, 0.f};
    float hp = 0.f;
    double knock = 0.0;
    meno::Vec2f knockDir{0.f, 0.f};
    double flash = 0.0;
    bool dead = false;
    double deathTimer = 0.0;
};

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
    double life;
};

struct Gem {
    meno::Vec2f pos;
    int value;
    double life;
};

struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;
    void play() {
        if (sounds.empty()) {
            return;
        }
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

struct PlayState {
    meno::Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    bool facingLeft = false;
    double animTimer = 0.0;
    int playerFrame = 0;
    bool hurt = false;

    double time = 0.0;
    double spawnTimer = 0.5;
    double shootTimer = 0.8;

    double fireInterval = 0.8;
    float damage = 10.f;
    int projectiles = 1;
    float speedMul = 1.f;
    float magnet = 60.f;

    int level = 1;
    int xp = 0;
    int kills = 0;
    int pendingCards = 0;
    std::vector<int> cards;
    bool paused = false;

    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};

float lengthOf(meno::Vec2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

std::string formatTime(double t) {
    const int s = static_cast<int>(std::floor(std::clamp(t, 0.0, 300.0)));
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

meno::Rectf cardRect(int i) {
    return meno::Rectf{{80.f + 280.f * static_cast<float>(i), 200.f}, {240.f, 140.f}};
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "LAST STAND",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& res = game.resources();

    std::vector<std::string> missing;
    const auto font = res.loadFont("assets/fonts/game.ttf");
    if (!font) missing.emplace_back("assets/fonts/game.ttf");

    const auto loadTex = [&](const char* path) {
        auto t = res.loadTexture(path);
        if (!t) missing.emplace_back(path);
        return t;
    };
    const auto texPlayer = loadTex("assets/textures/sv_player.png");
    const auto texWalker = loadTex("assets/textures/sv_walker.png");
    const auto texRunner = loadTex("assets/textures/sv_runner.png");
    const auto texBrute = loadTex("assets/textures/sv_brute.png");
    const auto texBullet = loadTex("assets/textures/sv_bullet.png");
    const auto texGem = loadTex("assets/textures/sv_gem.png");

    const auto loadBuf = [&](const char* path) {
        auto b = res.loadSoundBuffer(path);
        if (!b) missing.emplace_back(path);
        return b;
    };
    const auto bufShoot = loadBuf("assets/sfx/shoot.wav");
    const auto bufHit = loadBuf("assets/sfx/hit.wav");
    const auto bufPickup = loadBuf("assets/sfx/pickup.wav");
    const auto bufLevelup = loadBuf("assets/sfx/levelup.wav");

    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
    if (!bgm) missing.emplace_back("assets/music/bgm_survivor.ogg");

    if (!missing.empty()) {
        for (const std::string& p : missing) {
            std::cerr << "Failed to load: " << p << '\n';
        }
        return 1;
    }
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    const auto makePool = [](const std::shared_ptr<const meno::SoundBuffer>& buf, int n) {
        SoundPool pool;
        for (int i = 0; i < n; ++i) {
            pool.sounds.emplace_back(buf);
        }
        return pool;
    };
    SoundPool sndShoot = makePool(bufShoot, 6);
    SoundPool sndHit = makePool(bufHit, 8);
    SoundPool sndPickup = makePool(bufPickup, 6);
    SoundPool sndLevelup = makePool(bufLevelup, 2);

    const std::array<std::shared_ptr<const meno::Texture>, 3> enemyTex{texWalker, texRunner,
                                                                       texBrute};

    const meno::Color colBg = meno::Color::fromRgb(0x1A1420);
    const meno::Color colGrid = meno::Color::fromRgb(0x2A2233);
    const meno::Color colBorder = meno::Color::fromRgb(0x6B5B7B);
    const meno::Color colXpBg = meno::Color::fromRgb(0x2A2233);
    const meno::Color colXpFill = meno::Color::fromRgb(0x4FC3F7);
    const meno::Color colHpBg = meno::Color::fromRgb(0x3A1F24);
    const meno::Color colHpFill = meno::Color::fromRgb(0xE5533D);
    const meno::Color colCardBg = meno::Color::fromRgb(0x2A2233);
    const meno::Color colCardEdge = meno::Color::fromRgb(0xFFD54A);
    const meno::Color colGem1 = meno::Color::fromRgb(0x4FC3F7);
    const meno::Color colGem5 = meno::Color::fromRgb(0xFFD54A);
    const meno::Color colHurt = meno::Color::fromRgb(0xFF6060);
    const meno::Color colText = meno::Color::fromRgb(0xFFFFFF);

    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};
    meno::Camera2D worldCam{.center = {kWorldW / 2.f, kWorldH / 2.f}, .size = {kViewW, kViewH}};

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> unit{0.f, 1.f};

    PlayState s;
    bool survived = false;
    double titleTime = 0.0;

    // 글자: align 0=왼쪽, 1=가운데, 2=오른쪽
    const auto drawText = [&](meno::Renderer& r, const std::string& str, meno::Vec2f pos,
                              unsigned int size, int align) {
        meno::TextParams p;
        p.characterSize = size;
        p.color = colText;
        const meno::Rectf b = r.measureTextBounds(*font, str, p);
        if (align == 1) {
            p.origin = b.center();
        } else if (align == 2) {
            p.origin = {b.position.x + b.size.x, b.position.y};
        } else {
            p.origin = {b.position.x, b.position.y};
        }
        r.drawText(*font, str, pos, p);
    };

    const auto updateCamera = [&] {
        worldCam.center = {std::clamp(s.player.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                           std::clamp(s.player.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
        game.renderer().setCamera(worldCam);
    };

    const auto rollCards = [&] {
        std::vector<int> all(kCardNames.size());
        std::iota(all.begin(), all.end(), 0);
        std::shuffle(all.begin(), all.end(), rng);
        s.cards.assign(all.begin(), all.begin() + 3);
    };

    const auto applyCard = [&](int card) {
        switch (card) {
            case 0: s.fireInterval *= 0.85; break;
            case 1: s.damage += 5.f; break;
            case 2: s.projectiles += 1; break;
            case 3: s.speedMul *= 1.1f; break;
            case 4:
                s.maxHp += 20.f;
                s.hp = std::min(s.maxHp, s.hp + 20.f);
                break;
            case 5: s.magnet += 40.f; break;
            default: break;
        }
    };

    const auto spawnEnemy = [&] {
        const float left = worldCam.center.x - kViewW / 2.f - 64.f;
        const float top = worldCam.center.y - kViewH / 2.f - 64.f;
        const float w = kViewW + 128.f;
        const float h = kViewH + 128.f;
        const float perimeter = 2.f * (w + h);
        meno::Vec2f p{0.f, 0.f};
        bool ok = false;
        for (int attempt = 0; attempt < 64 && !ok; ++attempt) {
            float t = unit(rng) * perimeter;
            if (t < w) {
                p = {left + t, top};
            } else if ((t -= w) < h) {
                p = {left + w, top + t};
            } else if ((t -= h) < w) {
                p = {left + w - t, top + h};
            } else {
                t -= w;
                p = {left, top + h - t};
            }
            ok = p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
        }
        p.x = std::clamp(p.x, 0.f, kWorldW);
        p.y = std::clamp(p.y, 0.f, kWorldH);

        int type = 0;
        const float roll = unit(rng);
        if (s.time >= 120.0) {
            type = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
        } else if (s.time >= 60.0) {
            type = roll < 0.7f ? 0 : 1;
        }
        Enemy e;
        e.type = type;
        e.pos = p;
        e.hp = kTypes[static_cast<std::size_t>(type)].hp;
        s.enemies.push_back(e);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---------- 타이틀 ----------
    title.setEnterHandler([&] { titleTime = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTime += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = colBg});
        drawText(r, "LAST STAND", {480.f, 220.f}, 48, 1);
        drawText(r, "PRESS ENTER", {480.f, 320.f}, 16, 1);
    });

    // ---------- 플레이 ----------
    play.setEnterHandler([&] {
        s = PlayState{};
        updateCamera();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });

    play.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        const float dt = static_cast<float>(fixedDeltaTime);

        // 능력 선택
        if (!s.cards.empty()) {
            int choice = -1;
            if (input.wasPressed(meno::Key::Num1) || input.wasPressed(meno::Key::Numpad1)) {
                choice = 0;
            } else if (input.wasPressed(meno::Key::Num2) || input.wasPressed(meno::Key::Numpad2)) {
                choice = 1;
            } else if (input.wasPressed(meno::Key::Num3) || input.wasPressed(meno::Key::Numpad3)) {
                choice = 2;
            }
            if (choice < 0 && input.wasPressed(meno::MouseButton::Left)) {
                const meno::Vec2f mouse{input.mousePosition()};
                const meno::Vec2f point =
                    meno::screenToWorld(hud, game.renderer().framebufferSize(), mouse);
                for (int i = 0; i < 3; ++i) {
                    if (cardRect(i).contains(point)) {
                        choice = i;
                        break;
                    }
                }
            }
            if (choice >= 0) {
                applyCard(s.cards[static_cast<std::size_t>(choice)]);
                s.cards.clear();
                s.pendingCards -= 1;
                if (s.pendingCards > 0) {
                    rollCards();
                }
            }
            return;
        }

        if (input.wasPressed(meno::Key::P)) {
            s.paused = !s.paused;
        }
        if (s.paused) {
            return;
        }

        s.time += fixedDeltaTime;
        s.hurt = false;

        // 플레이어 이동
        meno::Vec2f dir{0.f, 0.f};
        if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left)) dir.x -= 1.f;
        if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right)) dir.x += 1.f;
        if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up)) dir.y -= 1.f;
        if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down)) dir.y += 1.f;
        const float dirLen = lengthOf(dir);
        if (dirLen > 0.f) {
            dir = dir * (1.f / dirLen);
            s.player += dir * (200.f * s.speedMul * dt);
            s.player.x = std::clamp(s.player.x, 16.f, kWorldW - 16.f);
            s.player.y = std::clamp(s.player.y, 16.f, kWorldH - 16.f);
            if (dir.x < 0.f) {
                s.facingLeft = true;
            } else if (dir.x > 0.f) {
                s.facingLeft = false;
            }
            s.animTimer += fixedDeltaTime;
            while (s.animTimer >= 0.15 - kEps) {
                s.animTimer -= 0.15;
                s.playerFrame = 1 - s.playerFrame;
            }
        } else {
            s.animTimer = 0.0;
            s.playerFrame = 0;
        }

        // 적 생성
        s.spawnTimer -= fixedDeltaTime;
        if (s.spawnTimer <= kEps) {
            if (s.enemies.size() < 400) {
                spawnEnemy();
            }
            const double steps = std::floor(s.time / 15.0);
            const double interval = std::max(0.15, 0.6 - 0.05 * steps);
            s.spawnTimer += interval;
        }

        // 자동 사격
        s.shootTimer -= fixedDeltaTime;
        if (s.shootTimer <= kEps) {
            const Enemy* target = nullptr;
            float best = 400.f * 400.f;
            for (const Enemy& e : s.enemies) {
                if (e.dead) continue;
                const meno::Vec2f d = e.pos - s.player;
                const float d2 = d.x * d.x + d.y * d.y;
                if (d2 <= best) {
                    best = d2;
                    target = &e;
                }
            }
            if (target != nullptr) {
                const meno::Vec2f d = target->pos - s.player;
                const float base = std::atan2(d.y, d.x);
                for (int i = 0; i < s.projectiles; ++i) {
                    const float offset = (static_cast<float>(i) -
                                          static_cast<float>(s.projectiles - 1) / 2.f) *
                                         (10.f * kPi / 180.f);
                    const float a = base + offset;
                    s.bullets.push_back(
                        {s.player, meno::Vec2f{std::cos(a), std::sin(a)} * 500.f, 1.2});
                }
                sndShoot.play();
                s.shootTimer = s.fireInterval;
            } else {
                s.shootTimer = 0.0;
            }
        }

        // 총알 이동과 충돌
        for (Bullet& b : s.bullets) {
            b.pos += b.vel * dt;
            b.life -= fixedDeltaTime;
            if (b.life <= kEps) {
                b.life = -1.0;
                continue;
            }
            for (Enemy& e : s.enemies) {
                if (e.dead) continue;
                const EnemyType& t = kTypes[static_cast<std::size_t>(e.type)];
                const meno::Vec2f d = e.pos - b.pos;
                const float rr = t.radius + kBulletRadius;
                if (d.x * d.x + d.y * d.y < rr * rr) {
                    b.life = -1.0;
                    e.hp -= s.damage;
                    e.flash = 0.1;
                    if (e.hp <= 0.f) {
                        e.dead = true;
                        e.deathTimer = 0.1;
                        s.gems.push_back({e.pos, t.xp, 30.0});
                        sndHit.play();
                        s.kills += 1;
                    } else {
                        e.knock = 0.1;
                        const float vl = lengthOf(b.vel);
                        e.knockDir = vl > 0.f ? b.vel * (1.f / vl) : meno::Vec2f{0.f, 0.f};
                    }
                    break;
                }
            }
        }
        std::erase_if(s.bullets, [](const Bullet& b) { return b.life <= 0.0; });

        // 적 이동과 접촉 피해
        for (Enemy& e : s.enemies) {
            const EnemyType& t = kTypes[static_cast<std::size_t>(e.type)];
            if (e.flash > 0.0) e.flash -= fixedDeltaTime;
            if (e.dead) {
                e.deathTimer -= fixedDeltaTime;
                continue;
            }
            if (e.knock > kEps) {
                e.pos += e.knockDir * (120.f * dt);
                e.knock -= fixedDeltaTime;
            } else {
                const meno::Vec2f d = s.player - e.pos;
                const float len = lengthOf(d);
                const float step = t.speed * dt;
                if (len > 0.0001f) {
                    e.pos += d * (std::min(step, len) / len);
                }
            }
            e.pos.x = std::clamp(e.pos.x, 0.f, kWorldW);
            e.pos.y = std::clamp(e.pos.y, 0.f, kWorldH);

            const meno::Vec2f d = s.player - e.pos;
            const float rr = t.radius + kPlayerRadius;
            if (d.x * d.x + d.y * d.y < rr * rr) {
                s.hp -= t.dps * dt;
                s.hurt = true;
            }
        }
        std::erase_if(s.enemies, [](const Enemy& e) { return e.dead && e.deathTimer <= kEps; });

        // 보석
        for (Gem& g : s.gems) {
            g.life -= fixedDeltaTime;
            if (g.life <= kEps) {
                g.life = -1.0;
                continue;
            }
            meno::Vec2f d = s.player - g.pos;
            float len = lengthOf(d);
            if (len < s.magnet && len > 0.0001f) {
                g.pos += d * (std::min(300.f * dt, len) / len);
                d = s.player - g.pos;
                len = lengthOf(d);
            }
            if (len < kGemRadius + kPlayerRadius) {
                s.xp += g.value;
                sndPickup.play();
                g.life = -1.0;
            }
        }
        std::erase_if(s.gems, [](const Gem& g) { return g.life <= 0.0; });

        // 레벨업
        while (s.xp >= 5 * s.level) {
            s.xp -= 5 * s.level;
            s.level += 1;
            s.pendingCards += 1;
            sndLevelup.play();
        }
        if (s.pendingCards > 0 && s.cards.empty()) {
            rollCards();
        }

        updateCamera();

        // 승패
        if (s.hp <= 0.f) {
            s.hp = 0.f;
            survived = false;
            (void)scenes.changeScene(result.id());
        } else if (s.time >= 300.0 - kEps) {
            s.time = 300.0;
            survived = true;
            (void)scenes.changeScene(result.id());
        }
    });

    play.setDrawHandler([&](meno::Renderer& r) {
        const meno::Rectf visible = r.visibleWorldBounds();

        // 배경과 격자
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = colBg});
        for (float x = 0.f; x <= kWorldW; x += 120.f) {
            if (x < visible.position.x - 2.f || x > visible.position.x + visible.size.x + 2.f) {
                continue;
            }
            r.drawLine({x, 0.f}, {x, kWorldH}, {.color = colGrid, .thickness = 1.f});
        }
        for (float y = 0.f; y <= kWorldH; y += 120.f) {
            if (y < visible.position.y - 2.f || y > visible.position.y + visible.size.y + 2.f) {
                continue;
            }
            r.drawLine({0.f, y}, {kWorldW, y}, {.color = colGrid, .thickness = 1.f});
        }
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                   {.fill = meno::colors::Transparent, .outline = colBorder,
                    .outlineThickness = -2.f});

        // 보석
        for (const Gem& g : s.gems) {
            const meno::Rectf box{{g.pos.x - 6.f, g.pos.y - 6.f}, {12.f, 12.f}};
            if (!box.intersects(visible)) continue;
            r.draw(*texGem, {.position = g.pos,
                             .origin = {6.f, 6.f},
                             .tint = g.value >= 5 ? colGem5 : colGem1});
        }

        // 적
        for (const Enemy& e : s.enemies) {
            const EnemyType& t = kTypes[static_cast<std::size_t>(e.type)];
            const float c = static_cast<float>(t.cell);
            const meno::Rectf box{{e.pos.x - c / 2.f, e.pos.y - c / 2.f}, {c, c}};
            if (!box.intersects(visible)) continue;
            int frame = static_cast<int>(std::floor(s.time / t.framePeriod)) % 2;
            if (e.dead || e.flash > kEps) {
                frame = 2;
            }
            r.draw(*enemyTex[static_cast<std::size_t>(e.type)],
                   {.position = e.pos,
                    .origin = {c / 2.f, c / 2.f},
                    .source = meno::Recti{frame * t.cell, 0, t.cell, t.cell},
                    .flipX = s.player.x < e.pos.x});
        }

        // 총알
        for (const Bullet& b : s.bullets) {
            r.draw(*texBullet, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        // 플레이어
        r.draw(*texPlayer, {.position = s.player,
                            .origin = {16.f, 16.f},
                            .tint = s.hurt ? colHurt : meno::colors::White,
                            .source = meno::Recti{s.playerFrame * 32, 0, 32, 32},
                            .flipX = s.facingLeft});

        // HUD
        r.setCamera(hud);
        const float need = static_cast<float>(5 * s.level);
        const float xpRatio = std::clamp(static_cast<float>(s.xp) / need, 0.f, 1.f);
        r.drawRect({{0.f, 0.f}, {kViewW, 6.f}}, {.fill = colXpBg});
        r.drawRect({{0.f, 0.f}, {kViewW * xpRatio, 6.f}}, {.fill = colXpFill});

        const float hpRatio = std::clamp(s.hp / s.maxHp, 0.f, 1.f);
        r.drawRect({{16.f, 18.f}, {200.f, 12.f}}, {.fill = colHpBg});
        r.drawRect({{16.f, 18.f}, {200.f * hpRatio, 12.f}}, {.fill = colHpFill});
        const int hpShown = static_cast<int>(std::ceil(std::max(0.f, s.hp)));
        drawText(r, "HP " + std::to_string(hpShown), {226.f, 16.f}, 16, 0);

        drawText(r, "LV " + std::to_string(s.level), {944.f, 16.f}, 16, 2);
        drawText(r, "TIME " + formatTime(s.time), {944.f, 38.f}, 16, 2);
        drawText(r, "KILLS " + std::to_string(s.kills), {944.f, 60.f}, 16, 2);

        if (!s.cards.empty()) {
            for (int i = 0; i < 3; ++i) {
                const meno::Rectf rect = cardRect(i);
                r.drawRect(rect, {.fill = colCardBg, .outline = colCardEdge,
                                  .outlineThickness = 2.f});
                drawText(r, kCardNames[static_cast<std::size_t>(s.cards[static_cast<std::size_t>(i)])],
                         rect.center(), 16, 1);
            }
        } else if (s.paused) {
            drawText(r, "PAUSED", {480.f, 270.f}, 48, 1);
        }
    });

    // ---------- 결과 ----------
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = colBg});
        drawText(r, survived ? "SURVIVED" : "GAME OVER", {480.f, 160.f}, 48, 1);
        drawText(r, "TIME " + formatTime(s.time), {480.f, 250.f}, 32, 1);
        drawText(r, "LEVEL " + std::to_string(s.level), {480.f, 295.f}, 32, 1);
        drawText(r, "KILLS " + std::to_string(s.kills), {480.f, 340.f}, 32, 1);
        drawText(r, "R: RESTART  ESC: QUIT", {480.f, 430.f}, 16, 1);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```