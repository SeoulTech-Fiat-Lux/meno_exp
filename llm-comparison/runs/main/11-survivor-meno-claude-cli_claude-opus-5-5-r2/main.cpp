#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
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
constexpr float kPlayerRadius = 14.f;
constexpr float kBulletRadius = 4.f;
constexpr float kGemRadius = 6.f;
constexpr std::size_t kMaxEnemies = 400;

struct EnemyKind {
    float radius;
    float hp;
    float speed;
    float dps;
    int frameSize;
    double period;
    int xp;
};

constexpr std::array<EnemyKind, 3> kKinds{{
    {10.f, 10.f, 80.f, 10.f, 24, 0.3, 1},  // 걷는 적
    {10.f, 6.f, 140.f, 6.f, 24, 0.15, 1},  // 뛰는 적
    {20.f, 60.f, 50.f, 20.f, 48, 0.4, 5},  // 큰 적
}};

struct Enemy {
    int kind = 0;
    meno::Vec2f pos{0.f, 0.f};
    float hp = 0.f;
    meno::Vec2f knockDir{0.f, 0.f};
    double knock = 0.0;
    double flash = 0.0;
    bool dead = false;
    double deathTimer = 0.0;
};

struct Bullet {
    meno::Vec2f pos{0.f, 0.f};
    meno::Vec2f dir{0.f, 0.f};
    double life = 1.2;
};

struct Gem {
    meno::Vec2f pos{0.f, 0.f};
    int value = 1;
    double life = 30.0;
};

struct PlayState {
    meno::Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    float speed = 200.f;
    double fireInterval = 0.8;
    float damage = 10.f;
    int projectiles = 1;
    float magnet = 60.f;
    double fireTimer = 0.0;
    double animTimer = 0.0;
    int frame = 0;
    bool facingLeft = false;
    bool hurt = false;
    double time = 0.0;
    double spawnTimer = 0.5;
    int kills = 0;
    int level = 1;
    int xp = 0;
    int pending = 0;
    bool choosing = false;
    std::array<int, 3> cards{0, 1, 2};
    bool paused = false;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};

struct SfxPool {
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

const std::array<const char*, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40",
};

std::string formatTime(double t) {
    const int s = static_cast<int>(std::max(t, 0.0));
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

float dist2(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

meno::Vec2f cameraCenter(meno::Vec2f player) {
    return {std::clamp(player.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
            std::clamp(player.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
}

meno::Rectf cardRect(int i) {
    return meno::Rectf{{80.f + 280.f * static_cast<float>(i), 200.f}, {240.f, 140.f}};
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "LAST STAND",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& res = game.resources();

    bool failed = false;
    const auto check = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };

    const auto font = res.loadFont("assets/fonts/game.ttf");
    check(font != nullptr, "assets/fonts/game.ttf");
    const auto texPlayer = res.loadTexture("assets/textures/sv_player.png");
    check(texPlayer != nullptr, "assets/textures/sv_player.png");
    const auto texWalker = res.loadTexture("assets/textures/sv_walker.png");
    check(texWalker != nullptr, "assets/textures/sv_walker.png");
    const auto texRunner = res.loadTexture("assets/textures/sv_runner.png");
    check(texRunner != nullptr, "assets/textures/sv_runner.png");
    const auto texBrute = res.loadTexture("assets/textures/sv_brute.png");
    check(texBrute != nullptr, "assets/textures/sv_brute.png");
    const auto texBullet = res.loadTexture("assets/textures/sv_bullet.png");
    check(texBullet != nullptr, "assets/textures/sv_bullet.png");
    const auto texGem = res.loadTexture("assets/textures/sv_gem.png");
    check(texGem != nullptr, "assets/textures/sv_gem.png");
    const auto bufShoot = res.loadSoundBuffer("assets/sfx/shoot.wav");
    check(bufShoot != nullptr, "assets/sfx/shoot.wav");
    const auto bufHit = res.loadSoundBuffer("assets/sfx/hit.wav");
    check(bufHit != nullptr, "assets/sfx/hit.wav");
    const auto bufPickup = res.loadSoundBuffer("assets/sfx/pickup.wav");
    check(bufPickup != nullptr, "assets/sfx/pickup.wav");
    const auto bufLevelup = res.loadSoundBuffer("assets/sfx/levelup.wav");
    check(bufLevelup != nullptr, "assets/sfx/levelup.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
    check(bgm.has_value(), "assets/music/bgm_survivor.ogg");
    if (failed) {
        return 1;
    }
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    const auto makePool = [](const std::shared_ptr<const meno::SoundBuffer>& buf, int n) {
        SfxPool pool;
        pool.sounds.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            pool.sounds.emplace_back(buf);
        }
        return pool;
    };
    SfxPool sfxShoot = makePool(bufShoot, 6);
    SfxPool sfxHit = makePool(bufHit, 8);
    SfxPool sfxPickup = makePool(bufPickup, 8);
    SfxPool sfxLevelup = makePool(bufLevelup, 2);

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
    const meno::Color colWhite = meno::Color::fromRgb(0xFFFFFF);

    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> unit{0.f, 1.f};

    PlayState st;
    double titleTimer = 0.0;
    bool survived = false;

    // align: 0 왼쪽, 1 가운데, 2 오른쪽
    const auto text = [&](meno::Renderer& r, const std::string& s, meno::Vec2f pos,
                          unsigned int size, int align) {
        meno::TextParams p{.characterSize = size, .color = colWhite};
        const meno::Rectf b = r.measureTextBounds(*font, s, p);
        if (align == 1) {
            p.origin = b.center();
        } else if (align == 2) {
            p.origin = meno::Vec2f{b.position.x + b.size.x, b.position.y};
        } else {
            p.origin = meno::Vec2f{b.position.x, b.position.y};
        }
        r.drawText(*font, s, pos, p);
    };

    const auto makeCards = [&] {
        std::array<int, 6> idx{0, 1, 2, 3, 4, 5};
        std::shuffle(idx.begin(), idx.end(), rng);
        st.cards = {idx[0], idx[1], idx[2]};
    };

    const auto applyCard = [&](int card) {
        switch (card) {
        case 0: st.fireInterval *= 0.85; break;
        case 1: st.damage += 5.f; break;
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

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    const auto checkQuit = [&] {
        if (game.window().input().wasPressed(meno::Key::Escape)) {
            game.stop();
            return true;
        }
        return false;
    };

    // ---------- 타이틀 ----------
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        if (checkQuit()) {
            return;
        }
        titleTimer += dt;
        if (game.window().input().wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = colBg});
        text(r, "LAST STAND", {480.f, 220.f}, 48u, 1);
        text(r, "PRESS ENTER", {480.f, 320.f}, 16u, 1);
    });

    // ---------- 플레이 ----------
    play.setEnterHandler([&] {
        st = PlayState{};
        st.enemies.reserve(kMaxEnemies);
        st.gems.reserve(512);
        st.bullets.reserve(64);
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });

    play.setUpdateHandler([&](double fixedDt) {
        if (checkQuit()) {
            return;
        }
        const meno::Input& input = game.window().input();
        const float dt = static_cast<float>(fixedDt);

        // 능력 선택
        if (st.choosing) {
            int chosen = -1;
            if (input.wasPressed(meno::Key::Num1) || input.wasPressed(meno::Key::Numpad1)) {
                chosen = 0;
            } else if (input.wasPressed(meno::Key::Num2) || input.wasPressed(meno::Key::Numpad2)) {
                chosen = 1;
            } else if (input.wasPressed(meno::Key::Num3) || input.wasPressed(meno::Key::Numpad3)) {
                chosen = 2;
            } else if (input.wasPressed(meno::MouseButton::Left)) {
                const meno::Vec2f mouse{input.mousePosition()};
                const meno::Vec2f point =
                    meno::screenToWorld(hud, game.renderer().framebufferSize(), mouse);
                for (int i = 0; i < 3; ++i) {
                    if (cardRect(i).contains(point)) {
                        chosen = i;
                        break;
                    }
                }
            }
            if (chosen >= 0) {
                applyCard(st.cards[static_cast<std::size_t>(chosen)]);
                --st.pending;
                if (st.pending > 0) {
                    makeCards();
                } else {
                    st.choosing = false;
                }
            }
            return;
        }

        if (input.wasPressed(meno::Key::P)) {
            st.paused = !st.paused;
        }
        if (st.paused) {
            return;
        }

        st.time += fixedDt;

        // 플레이어 이동
        float mx = 0.f;
        float my = 0.f;
        if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left)) {
            mx -= 1.f;
        }
        if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right)) {
            mx += 1.f;
        }
        if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up)) {
            my -= 1.f;
        }
        if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down)) {
            my += 1.f;
        }
        const bool moving = mx != 0.f || my != 0.f;
        if (moving) {
            const float len = std::sqrt(mx * mx + my * my);
            st.player.x += mx / len * st.speed * dt;
            st.player.y += my / len * st.speed * dt;
            st.player.x = std::clamp(st.player.x, 16.f, kWorldW - 16.f);
            st.player.y = std::clamp(st.player.y, 16.f, kWorldH - 16.f);
            if (mx != 0.f) {
                st.facingLeft = mx < 0.f;
            }
            st.animTimer += fixedDt;
            while (st.animTimer >= 0.15) {
                st.animTimer -= 0.15;
                st.frame = 1 - st.frame;
            }
        } else {
            st.animTimer = 0.0;
            st.frame = 0;
        }

        // 적 생성
        st.spawnTimer -= fixedDt;
        if (st.spawnTimer <= 0.0) {
            const double steps = std::floor(st.time / 15.0);
            const double interval = std::max(0.15, 0.6 - 0.05 * steps);
            st.spawnTimer += interval;
            if (st.enemies.size() < kMaxEnemies) {
                int kind = 0;
                const float roll = unit(rng);
                if (st.time >= 120.0) {
                    kind = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
                } else if (st.time >= 60.0) {
                    kind = roll < 0.7f ? 0 : 1;
                }
                const meno::Vec2f cc = cameraCenter(st.player);
                const float left = cc.x - kViewW / 2.f - 64.f;
                const float top = cc.y - kViewH / 2.f - 64.f;
                const float w = kViewW + 128.f;
                const float h = kViewH + 128.f;
                const float per = 2.f * (w + h);
                meno::Vec2f p{0.f, 0.f};
                bool ok = false;
                for (int tries = 0; tries < 32 && !ok; ++tries) {
                    float u = unit(rng) * per;
                    if (u < w) {
                        p = {left + u, top};
                    } else if ((u -= w) < h) {
                        p = {left + w, top + u};
                    } else if ((u -= h) < w) {
                        p = {left + w - u, top + h};
                    } else {
                        u -= w;
                        p = {left, top + h - u};
                    }
                    ok = p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
                }
                p.x = std::clamp(p.x, 0.f, kWorldW);
                p.y = std::clamp(p.y, 0.f, kWorldH);
                Enemy e;
                e.kind = kind;
                e.pos = p;
                e.hp = kKinds[static_cast<std::size_t>(kind)].hp;
                st.enemies.push_back(e);
            }
        }

        // 적 이동
        for (Enemy& e : st.enemies) {
            if (e.flash > 0.0) {
                e.flash -= fixedDt;
            }
            if (e.dead) {
                e.deathTimer -= fixedDt;
                continue;
            }
            if (e.knock > 0.0) {
                e.pos += e.knockDir * (120.f * dt);
                e.knock -= fixedDt;
                e.pos.x = std::clamp(e.pos.x, 0.f, kWorldW);
                e.pos.y = std::clamp(e.pos.y, 0.f, kWorldH);
                continue;
            }
            const float dx = st.player.x - e.pos.x;
            const float dy = st.player.y - e.pos.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f) {
                const float step =
                    std::min(kKinds[static_cast<std::size_t>(e.kind)].speed * dt, len);
                e.pos.x += dx / len * step;
                e.pos.y += dy / len * step;
            }
        }

        // 자동 사격
        st.fireTimer += fixedDt;
        if (st.fireTimer >= st.fireInterval) {
            st.fireTimer -= st.fireInterval;
            const Enemy* target = nullptr;
            float best = 400.f * 400.f;
            for (const Enemy& e : st.enemies) {
                if (e.dead) {
                    continue;
                }
                const float d = dist2(e.pos, st.player);
                if (d <= best) {
                    best = d;
                    target = &e;
                }
            }
            if (target != nullptr) {
                const float base = std::atan2(target->pos.y - st.player.y,
                                              target->pos.x - st.player.x);
                const float n = static_cast<float>(st.projectiles);
                for (int i = 0; i < st.projectiles; ++i) {
                    const float off =
                        (static_cast<float>(i) - (n - 1.f) / 2.f) * (10.f * kPi / 180.f);
                    const float a = base + off;
                    Bullet b;
                    b.pos = st.player;
                    b.dir = {std::cos(a), std::sin(a)};
                    st.bullets.push_back(b);
                }
                sfxShoot.play();
            }
        }

        // 총알 이동과 충돌
        for (Bullet& b : st.bullets) {
            b.pos += b.dir * (500.f * dt);
            b.life -= fixedDt;
            if (b.life <= 0.0) {
                continue;
            }
            for (Enemy& e : st.enemies) {
                if (e.dead) {
                    continue;
                }
                const float rr = kBulletRadius + kKinds[static_cast<std::size_t>(e.kind)].radius;
                if (dist2(b.pos, e.pos) < rr * rr) {
                    e.hp -= st.damage;
                    e.flash = 0.1;
                    if (e.hp <= 0.f) {
                        e.dead = true;
                        e.deathTimer = 0.1;
                        e.knock = 0.0;
                        Gem g;
                        g.pos = e.pos;
                        g.value = kKinds[static_cast<std::size_t>(e.kind)].xp;
                        st.gems.push_back(g);
                        sfxHit.play();
                        ++st.kills;
                    } else {
                        e.knock = 0.1;
                        e.knockDir = b.dir;
                    }
                    b.life = -1.0;
                    break;
                }
            }
        }

        // 접촉 피해
        st.hurt = false;
        for (const Enemy& e : st.enemies) {
            if (e.dead) {
                continue;
            }
            const EnemyKind& k = kKinds[static_cast<std::size_t>(e.kind)];
            const float rr = kPlayerRadius + k.radius;
            if (dist2(e.pos, st.player) < rr * rr) {
                st.hp -= k.dps * dt;
                st.hurt = true;
            }
        }

        // 보석
        for (Gem& g : st.gems) {
            g.life -= fixedDt;
            if (g.life <= 0.0) {
                continue;
            }
            const float dx = st.player.x - g.pos.x;
            const float dy = st.player.y - g.pos.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len <= st.magnet && len > 0.001f) {
                const float step = std::min(300.f * dt, len);
                g.pos.x += dx / len * step;
                g.pos.y += dy / len * step;
            }
            const float rr = kPlayerRadius + kGemRadius;
            if (dist2(g.pos, st.player) < rr * rr) {
                st.xp += g.value;
                g.life = -1.0;
                sfxPickup.play();
                while (st.xp >= 5 * st.level) {
                    st.xp -= 5 * st.level;
                    ++st.level;
                    ++st.pending;
                    sfxLevelup.play();
                }
            }
        }

        // 삭제
        std::erase_if(st.enemies, [](const Enemy& e) { return e.dead && e.deathTimer <= 0.0; });
        std::erase_if(st.bullets, [](const Bullet& b) { return b.life <= 0.0; });
        std::erase_if(st.gems, [](const Gem& g) { return g.life <= 0.0; });

        if (st.pending > 0 && !st.choosing) {
            makeCards();
            st.choosing = true;
        }

        // 승패
        if (st.hp <= 0.f) {
            st.hp = 0.f;
            survived = false;
            (void)scenes.changeScene(result.id());
        } else if (st.time >= 300.0) {
            st.time = 300.0;
            survived = true;
            (void)scenes.changeScene(result.id());
        }
    });

    play.setDrawHandler([&](meno::Renderer& r) {
        const meno::Vec2f cc = cameraCenter(st.player);
        r.setCamera(meno::Camera2D{.center = cc, .size = {kViewW, kViewH}});
        const meno::Rectf visible{{cc.x - kViewW / 2.f - 64.f, cc.y - kViewH / 2.f - 64.f},
                                  {kViewW + 128.f, kViewH + 128.f}};
        const auto inView = [&](meno::Vec2f p) { return visible.contains(p); };

        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = colBg});
        for (float x = 120.f; x < kWorldW; x += 120.f) {
            r.drawLine({x, 0.f}, {x, kWorldH}, {.color = colGrid, .thickness = 1.f});
        }
        for (float y = 120.f; y < kWorldH; y += 120.f) {
            r.drawLine({0.f, y}, {kWorldW, y}, {.color = colGrid, .thickness = 1.f});
        }
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                   {.fill = meno::colors::Transparent, .outline = colBorder,
                    .outlineThickness = -2.f});

        for (const Gem& g : st.gems) {
            if (!inView(g.pos)) {
                continue;
            }
            r.draw(*texGem, {.position = g.pos,
                             .origin = {6.f, 6.f},
                             .tint = g.value >= 5 ? colGem5 : colGem1});
        }

        for (const Enemy& e : st.enemies) {
            if (!inView(e.pos)) {
                continue;
            }
            const EnemyKind& k = kKinds[static_cast<std::size_t>(e.kind)];
            int frame = static_cast<int>(st.time / k.period) % 2;
            if (e.flash > 0.0 || e.dead) {
                frame = 2;
            }
            const float half = static_cast<float>(k.frameSize) / 2.f;
            r.draw(*enemyTex[static_cast<std::size_t>(e.kind)],
                   {.position = e.pos,
                    .origin = {half, half},
                    .source = meno::Recti{frame * k.frameSize, 0, k.frameSize, k.frameSize},
                    .flipX = st.player.x < e.pos.x});
        }

        for (const Bullet& b : st.bullets) {
            r.draw(*texBullet, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        r.draw(*texPlayer, {.position = st.player,
                            .origin = {16.f, 16.f},
                            .tint = st.hurt ? colHurt : colWhite,
                            .source = meno::Recti{st.frame * 32, 0, 32, 32},
                            .flipX = st.facingLeft});

        // HUD
        r.setCamera(hud);
        const float need = static_cast<float>(5 * st.level);
        r.drawRect({{0.f, 0.f}, {kViewW, 6.f}}, {.fill = colXpBg});
        r.drawRect({{0.f, 0.f}, {kViewW * std::clamp(static_cast<float>(st.xp) / need, 0.f, 1.f),
                                 6.f}},
                   {.fill = colXpFill});
        r.drawRect({{16.f, 16.f}, {200.f, 12.f}}, {.fill = colHpBg});
        r.drawRect({{16.f, 16.f},
                    {200.f * std::clamp(st.hp / st.maxHp, 0.f, 1.f), 12.f}},
                   {.fill = colHpFill});
        const int hpShown = static_cast<int>(std::ceil(std::max(st.hp, 0.f)));
        text(r, "HP " + std::to_string(hpShown), {226.f, 14.f}, 16u, 0);
        text(r, "LV " + std::to_string(st.level), {944.f, 14.f}, 16u, 2);
        text(r, "TIME " + formatTime(st.time), {944.f, 36.f}, 16u, 2);
        text(r, "KILLS " + std::to_string(st.kills), {944.f, 58.f}, 16u, 2);

        if (st.choosing) {
            for (int i = 0; i < 3; ++i) {
                const meno::Rectf rc = cardRect(i);
                r.drawRect(rc, {.fill = colCardBg, .outline = colCardEdge,
                                .outlineThickness = -2.f});
                text(r, kCardNames[static_cast<std::size_t>(st.cards[static_cast<std::size_t>(i)])],
                     rc.center(), 16u, 1);
            }
        } else if (st.paused) {
            text(r, "PAUSED", {480.f, 270.f}, 48u, 1);
        }
    });

    // ---------- 결과 ----------
    result.setUpdateHandler([&](double) {
        if (checkQuit()) {
            return;
        }
        if (game.window().input().wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& r) {
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = colBg});
        text(r, survived ? "SURVIVED" : "GAME OVER", {480.f, 150.f}, 48u, 1);
        text(r, "TIME " + formatTime(st.time), {480.f, 240.f}, 32u, 1);
        text(r, "LEVEL " + std::to_string(st.level), {480.f, 290.f}, 32u, 1);
        text(r, "KILLS " + std::to_string(st.kills), {480.f, 340.f}, 32u, 1);
        text(r, "R: RESTART  ESC: QUIT", {480.f, 430.f}, 16u, 1);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
