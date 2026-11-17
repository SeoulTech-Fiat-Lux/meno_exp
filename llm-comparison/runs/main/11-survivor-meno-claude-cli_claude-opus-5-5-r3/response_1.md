```cpp
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
constexpr float kEps = 1e-6f;

constexpr float kPlayerRadius = 14.f;
constexpr float kBulletRadius = 4.f;
constexpr float kGemRadius = 6.f;
constexpr std::size_t kMaxEnemies = 400;

struct EnemyType {
    const char* path;
    int frame;
    float radius;
    float hp;
    float speed;
    float dps;
    float animPeriod;
    int xp;
};

const std::array<EnemyType, 3> kTypes{{
    {"assets/textures/sv_walker.png", 24, 10.f, 10.f, 80.f, 10.f, 0.3f, 1},
    {"assets/textures/sv_runner.png", 24, 10.f, 6.f, 140.f, 6.f, 0.15f, 1},
    {"assets/textures/sv_brute.png", 48, 20.f, 60.f, 50.f, 20.f, 0.4f, 5},
}};

const std::array<const char*, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40",
};

struct Enemy {
    int type = 0;
    meno::Vec2f pos{0.f, 0.f};
    float hp = 0.f;
    float knock = 0.f;
    meno::Vec2f knockDir{0.f, 0.f};
    float flash = 0.f;
    bool dying = false;
    float dyingTimer = 0.f;
};

struct Bullet {
    meno::Vec2f pos{0.f, 0.f};
    meno::Vec2f vel{0.f, 0.f};
    float life = 1.2f;
    bool dead = false;
};

struct Gem {
    meno::Vec2f pos{0.f, 0.f};
    int value = 1;
    float life = 30.f;
};

struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;
    void init(const std::shared_ptr<const meno::SoundBuffer>& buffer, int count) {
        sounds.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            sounds.emplace_back(buffer);
        }
    }
    void play() {
        if (sounds.empty()) {
            return;
        }
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

float lengthOf(meno::Vec2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

std::string formatTime(double seconds) {
    int s = static_cast<int>(std::floor(std::clamp(seconds, 0.0, 300.0)));
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", s / 60, s % 60);
    return buffer;
}

meno::Rectf cardRect(int index) {
    return meno::Rectf{{80.f + 280.f * static_cast<float>(index), 200.f}, {240.f, 140.f}};
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "LAST STAND",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& res = game.resources();

    std::vector<std::string> missing;
    const auto font = res.loadFont("assets/fonts/game.ttf");
    if (!font) {
        missing.emplace_back("assets/fonts/game.ttf");
    }
    const auto loadTex = [&](const char* path) {
        auto t = res.loadTexture(path);
        if (!t) {
            missing.emplace_back(path);
        }
        return t;
    };
    const auto loadSfx = [&](const char* path) {
        auto b = res.loadSoundBuffer(path);
        if (!b) {
            missing.emplace_back(path);
        }
        return b;
    };
    const auto texPlayer = loadTex("assets/textures/sv_player.png");
    std::array<std::shared_ptr<const meno::Texture>, 3> texEnemy{
        loadTex(kTypes[0].path), loadTex(kTypes[1].path), loadTex(kTypes[2].path)};
    const auto texBullet = loadTex("assets/textures/sv_bullet.png");
    const auto texGem = loadTex("assets/textures/sv_gem.png");
    const auto bufShoot = loadSfx("assets/sfx/shoot.wav");
    const auto bufHit = loadSfx("assets/sfx/hit.wav");
    const auto bufPickup = loadSfx("assets/sfx/pickup.wav");
    const auto bufLevelup = loadSfx("assets/sfx/levelup.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
    if (!bgm) {
        missing.emplace_back("assets/music/bgm_survivor.ogg");
    }
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    SoundPool sndShoot;
    SoundPool sndHit;
    SoundPool sndPickup;
    SoundPool sndLevelup;
    sndShoot.init(bufShoot, 6);
    sndHit.init(bufHit, 8);
    sndPickup.init(bufPickup, 8);
    sndLevelup.init(bufLevelup, 2);

    const meno::Color colBg = meno::Color::fromRgb(0x1A1420);
    const meno::Color colGrid = meno::Color::fromRgb(0x2A2233);
    const meno::Color colBorder = meno::Color::fromRgb(0x6B5B7B);
    const meno::Color colXpBg = meno::Color::fromRgb(0x2A2233);
    const meno::Color colXpFill = meno::Color::fromRgb(0x4FC3F7);
    const meno::Color colHpBg = meno::Color::fromRgb(0x3A1F24);
    const meno::Color colHpFill = meno::Color::fromRgb(0xE5533D);
    const meno::Color colCardBg = meno::Color::fromRgb(0x2A2233);
    const meno::Color colCardBorder = meno::Color::fromRgb(0xFFD54A);
    const meno::Color colGem1 = meno::Color::fromRgb(0x4FC3F7);
    const meno::Color colGem5 = meno::Color::fromRgb(0xFFD54A);
    const meno::Color colHurt = meno::Color::fromRgb(0xFF6060);
    const meno::Color colText = meno::colors::White;

    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};
    meno::Camera2D worldCam{.center = {kWorldW / 2.f, kWorldH / 2.f}, .size = {kViewW, kViewH}};

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> uni01{0.f, 1.f};

    // ---- 게임 상태 ----
    meno::Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    bool facingLeft = false;
    float animTimer = 0.f;
    int playerFrame = 0;
    bool hurt = false;
    float fireTimer = 0.f;
    float fireInterval = 0.8f;
    int damage = 10;
    int projectiles = 1;
    float speed = 200.f;
    float magnet = 60.f;
    int level = 1;
    int xp = 0;
    int kills = 0;
    double playTime = 0.0;
    double nextSpawn = 0.5;
    bool paused = false;
    bool choosing = false;
    int pendingLevels = 0;
    std::array<int, 3> cards{0, 1, 2};
    bool survived = false;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
    enemies.reserve(512);
    bullets.reserve(64);
    gems.reserve(512);

    const auto updateCamera = [&] {
        worldCam.center = {std::clamp(player.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                           std::clamp(player.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
        game.renderer().setCamera(worldCam);
    };

    const auto resetGame = [&] {
        player = {kWorldW / 2.f, kWorldH / 2.f};
        hp = 100.f;
        maxHp = 100.f;
        facingLeft = false;
        animTimer = 0.f;
        playerFrame = 0;
        hurt = false;
        fireTimer = 0.f;
        fireInterval = 0.8f;
        damage = 10;
        projectiles = 1;
        speed = 200.f;
        magnet = 60.f;
        level = 1;
        xp = 0;
        kills = 0;
        playTime = 0.0;
        nextSpawn = 0.5;
        paused = false;
        choosing = false;
        pendingLevels = 0;
        survived = false;
        enemies.clear();
        bullets.clear();
        gems.clear();
        updateCamera();
    };

    const auto openCards = [&] {
        std::array<int, 6> all{0, 1, 2, 3, 4, 5};
        std::shuffle(all.begin(), all.end(), rng);
        cards = {all[0], all[1], all[2]};
        choosing = true;
    };

    const auto applyCard = [&](int card) {
        switch (card) {
        case 0: fireInterval *= 0.85f; break;
        case 1: damage += 5; break;
        case 2: projectiles += 1; break;
        case 3: speed *= 1.1f; break;
        case 4:
            maxHp += 20.f;
            hp = std::min(hp + 20.f, maxHp);
            break;
        case 5: magnet += 40.f; break;
        default: break;
        }
    };

    const auto spawnEnemy = [&] {
        const float left = worldCam.center.x - kViewW / 2.f - 64.f;
        const float top = worldCam.center.y - kViewH / 2.f - 64.f;
        const float right = worldCam.center.x + kViewW / 2.f + 64.f;
        const float bottom = worldCam.center.y + kViewH / 2.f + 64.f;
        struct Seg {
            meno::Vec2f a;
            meno::Vec2f b;
        };
        std::vector<Seg> segs;
        const float x0 = std::max(left, 0.f);
        const float x1 = std::min(right, kWorldW);
        const float y0 = std::max(top, 0.f);
        const float y1 = std::min(bottom, kWorldH);
        if (top >= 0.f && top <= kWorldH && x1 > x0) {
            segs.push_back({{x0, top}, {x1, top}});
        }
        if (bottom >= 0.f && bottom <= kWorldH && x1 > x0) {
            segs.push_back({{x0, bottom}, {x1, bottom}});
        }
        if (left >= 0.f && left <= kWorldW && y1 > y0) {
            segs.push_back({{left, y0}, {left, y1}});
        }
        if (right >= 0.f && right <= kWorldW && y1 > y0) {
            segs.push_back({{right, y0}, {right, y1}});
        }
        float total = 0.f;
        for (const Seg& s : segs) {
            total += lengthOf(s.b - s.a);
        }
        meno::Vec2f pos{0.f, 0.f};
        if (total <= 0.f) {
            pos = {uni01(rng) * kWorldW, 0.f};
        } else {
            float u = uni01(rng) * total;
            pos = segs.back().b;
            for (const Seg& s : segs) {
                const float len = lengthOf(s.b - s.a);
                if (u <= len) {
                    const float t = len > 0.f ? u / len : 0.f;
                    pos = s.a + (s.b - s.a) * t;
                    break;
                }
                u -= len;
            }
        }
        int type = 0;
        const float r = uni01(rng);
        if (playTime >= 120.0) {
            type = r < 0.6f ? 0 : (r < 0.9f ? 1 : 2);
        } else if (playTime >= 60.0) {
            type = r < 0.7f ? 0 : 1;
        }
        Enemy e;
        e.type = type;
        e.pos = pos;
        e.hp = kTypes[static_cast<std::size_t>(type)].hp;
        enemies.push_back(e);
    };

    // ---- 텍스트 도우미 ----
    const auto textCentered = [&](meno::Renderer& r, const std::string& s, meno::Vec2f pos,
                                  unsigned int size) {
        meno::TextParams p{.characterSize = size, .color = colText};
        p.origin = r.measureTextBounds(*font, s, p).center();
        r.drawText(*font, s, pos, p);
    };
    const auto textLeft = [&](meno::Renderer& r, const std::string& s, meno::Vec2f pos,
                              unsigned int size) {
        meno::TextParams p{.characterSize = size, .color = colText};
        const meno::Rectf b = r.measureTextBounds(*font, s, p);
        p.origin = {b.left(), b.top()};
        r.drawText(*font, s, pos, p);
    };
    const auto textRight = [&](meno::Renderer& r, const std::string& s, meno::Vec2f pos,
                               unsigned int size) {
        meno::TextParams p{.characterSize = size, .color = colText};
        const meno::Rectf b = r.measureTextBounds(*font, s, p);
        p.origin = {b.right(), b.top()};
        r.drawText(*font, s, pos, p);
    };
    const auto screenBg = [&](meno::Renderer& r) {
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = colBg});
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---- 타이틀 ----
    double titleTimer = 0.0;
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (in.wasPressed(meno::Key::Enter) || titleTimer >= 2.0 - 1e-9) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        screenBg(r);
        textCentered(r, "LAST STAND", {kViewW / 2.f, 220.f}, 48);
        textCentered(r, "PRESS ENTER", {kViewW / 2.f, 320.f}, 32);
    });

    // ---- 플레이 ----
    play.setEnterHandler([&] {
        resetGame();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });

    play.setUpdateHandler([&](double fixedDt) {
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (choosing) {
            int pick = -1;
            if (in.wasPressed(meno::Key::Num1)) {
                pick = 0;
            } else if (in.wasPressed(meno::Key::Num2)) {
                pick = 1;
            } else if (in.wasPressed(meno::Key::Num3)) {
                pick = 2;
            }
            if (pick < 0 && in.wasPressed(meno::MouseButton::Left)) {
                const meno::Vec2f mouse{in.mousePosition()};
                const meno::Vec2f point =
                    meno::screenToWorld(hud, game.renderer().framebufferSize(), mouse);
                for (int i = 0; i < 3; ++i) {
                    if (cardRect(i).contains(point)) {
                        pick = i;
                        break;
                    }
                }
            }
            if (pick >= 0) {
                applyCard(cards[static_cast<std::size_t>(pick)]);
                --pendingLevels;
                if (pendingLevels > 0) {
                    openCards();
                } else {
                    choosing = false;
                }
            }
            return;
        }

        if (in.wasPressed(meno::Key::P)) {
            paused = !paused;
        }
        if (paused) {
            return;
        }

        const float dt = static_cast<float>(fixedDt);

        // 플레이어 이동
        meno::Vec2f dir{0.f, 0.f};
        if (in.isDown(meno::Key::A) || in.isDown(meno::Key::Left)) {
            dir.x -= 1.f;
        }
        if (in.isDown(meno::Key::D) || in.isDown(meno::Key::Right)) {
            dir.x += 1.f;
        }
        if (in.isDown(meno::Key::W) || in.isDown(meno::Key::Up)) {
            dir.y -= 1.f;
        }
        if (in.isDown(meno::Key::S) || in.isDown(meno::Key::Down)) {
            dir.y += 1.f;
        }
        const float dirLen = lengthOf(dir);
        if (dirLen > 0.f) {
            dir = dir * (1.f / dirLen);
            player += dir * (speed * dt);
            player.x = std::clamp(player.x, 16.f, kWorldW - 16.f);
            player.y = std::clamp(player.y, 16.f, kWorldH - 16.f);
            if (dir.x < 0.f) {
                facingLeft = true;
            } else if (dir.x > 0.f) {
                facingLeft = false;
            }
            animTimer += dt;
            playerFrame = static_cast<int>(std::floor(animTimer / 0.15f)) % 2;
        } else {
            animTimer = 0.f;
            playerFrame = 0;
        }
        updateCamera();

        playTime += fixedDt;

        // 적 생성
        while (playTime >= nextSpawn) {
            if (enemies.size() < kMaxEnemies) {
                spawnEnemy();
            }
            const double interval =
                std::max(0.15, 0.6 - 0.05 * std::floor(nextSpawn / 15.0));
            nextSpawn += interval;
        }

        // 적 이동
        for (Enemy& e : enemies) {
            const EnemyType& t = kTypes[static_cast<std::size_t>(e.type)];
            if (e.flash > 0.f) {
                e.flash -= dt;
            }
            if (e.dying) {
                e.dyingTimer -= dt;
                continue;
            }
            if (e.knock > kEps) {
                e.pos += e.knockDir * (120.f * dt);
                e.knock -= dt;
            } else {
                const meno::Vec2f d = player - e.pos;
                const float len = lengthOf(d);
                if (len > 0.001f) {
                    e.pos += d * (std::min(t.speed * dt, len) / len);
                }
            }
            e.pos.x = std::clamp(e.pos.x, t.radius, kWorldW - t.radius);
            e.pos.y = std::clamp(e.pos.y, t.radius, kWorldH - t.radius);
        }

        // 자동 사격
        fireTimer += dt;
        if (fireTimer >= fireInterval - kEps) {
            fireTimer -= fireInterval;
            if (fireTimer < 0.f) {
                fireTimer = 0.f;
            }
            const Enemy* target = nullptr;
            float best = 400.f * 400.f;
            for (const Enemy& e : enemies) {
                if (e.dying) {
                    continue;
                }
                const meno::Vec2f d = e.pos - player;
                const float d2 = d.x * d.x + d.y * d.y;
                if (d2 <= best) {
                    best = d2;
                    target = &e;
                }
            }
            if (target != nullptr) {
                const meno::Vec2f d = target->pos - player;
                const float base = std::atan2(d.y, d.x);
                for (int i = 0; i < projectiles; ++i) {
                    const float offset =
                        (static_cast<float>(i) - static_cast<float>(projectiles - 1) / 2.f) *
                        10.f * kPi / 180.f;
                    const float a = base + offset;
                    Bullet b;
                    b.pos = player;
                    b.vel = meno::Vec2f{std::cos(a), std::sin(a)} * 500.f;
                    bullets.push_back(b);
                }
                sndShoot.play();
            }
        }

        // 총알
        for (Bullet& b : bullets) {
            b.pos += b.vel * dt;
            b.life -= dt;
            if (b.life <= kEps) {
                b.dead = true;
                continue;
            }
            for (Enemy& e : enemies) {
                if (e.dying) {
                    continue;
                }
                const EnemyType& t = kTypes[static_cast<std::size_t>(e.type)];
                const meno::Vec2f d = e.pos - b.pos;
                const float rr = t.radius + kBulletRadius;
                if (d.x * d.x + d.y * d.y < rr * rr) {
                    b.dead = true;
                    e.hp -= static_cast<float>(damage);
                    e.flash = 0.1f;
                    if (e.hp <= 0.f) {
                        e.dying = true;
                        e.dyingTimer = 0.1f;
                        Gem g;
                        g.pos = e.pos;
                        g.value = t.xp;
                        gems.push_back(g);
                        sndHit.play();
                        ++kills;
                    } else {
                        e.knock = 0.1f;
                        const float vl = lengthOf(b.vel);
                        e.knockDir = vl > 0.f ? b.vel * (1.f / vl) : meno::Vec2f{0.f, 0.f};
                    }
                    break;
                }
            }
        }
        std::erase_if(bullets, [](const Bullet& b) { return b.dead; });
        std::erase_if(enemies, [](const Enemy& e) { return e.dying && e.dyingTimer <= kEps; });

        // 접촉 피해
        hurt = false;
        for (const Enemy& e : enemies) {
            if (e.dying) {
                continue;
            }
            const EnemyType& t = kTypes[static_cast<std::size_t>(e.type)];
            const meno::Vec2f d = e.pos - player;
            const float rr = t.radius + kPlayerRadius;
            if (d.x * d.x + d.y * d.y < rr * rr) {
                hp -= t.dps * dt;
                hurt = true;
            }
        }

        // 보석
        std::erase_if(gems, [&](Gem& g) {
            g.life -= dt;
            if (g.life <= kEps) {
                return true;
            }
            meno::Vec2f d = player - g.pos;
            float len = lengthOf(d);
            if (len < magnet && len > 0.001f) {
                g.pos += d * (std::min(300.f * dt, len) / len);
                d = player - g.pos;
                len = lengthOf(d);
            }
            if (len < kPlayerRadius + kGemRadius) {
                xp += g.value;
                sndPickup.play();
                return true;
            }
            return false;
        });

        // 레벨업
        bool leveled = false;
        while (xp >= 5 * level) {
            xp -= 5 * level;
            ++level;
            ++pendingLevels;
            leveled = true;
        }
        if (leveled) {
            sndLevelup.play();
        }

        // 승패
        if (hp <= 0.f) {
            hp = 0.f;
            survived = false;
            (void)scenes.changeScene(result.id());
            return;
        }
        if (playTime >= 300.0 - 1e-9) {
            survived = true;
            (void)scenes.changeScene(result.id());
            return;
        }

        if (pendingLevels > 0 && !choosing) {
            openCards();
        }
    });

    play.setDrawHandler([&](meno::Renderer& r) {
        // 월드
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

        const meno::Rectf visible = r.visibleWorldBounds();
        const meno::Rectf cull{{visible.left() - 32.f, visible.top() - 32.f},
                               {visible.size.x + 64.f, visible.size.y + 64.f}};

        for (const Gem& g : gems) {
            if (!cull.contains(g.pos)) {
                continue;
            }
            r.draw(*texGem, {.position = g.pos, .origin = {6.f, 6.f},
                             .tint = g.value >= 5 ? colGem5 : colGem1});
        }

        std::array<int, 3> enemyFrame{};
        for (std::size_t i = 0; i < 3; ++i) {
            enemyFrame[i] =
                static_cast<int>(std::floor(playTime / static_cast<double>(kTypes[i].animPeriod))) %
                2;
        }
        for (const Enemy& e : enemies) {
            if (!cull.contains(e.pos)) {
                continue;
            }
            const std::size_t ti = static_cast<std::size_t>(e.type);
            const EnemyType& t = kTypes[ti];
            const int frame = e.flash > kEps ? 2 : enemyFrame[ti];
            const float half = static_cast<float>(t.frame) / 2.f;
            r.draw(*texEnemy[ti],
                   {.position = e.pos, .origin = {half, half},
                    .source = meno::Recti{frame * t.frame, 0, t.frame, t.frame},
                    .flipX = player.x < e.pos.x});
        }

        r.draw(*texPlayer, {.position = player, .origin = {16.f, 16.f},
                            .tint = hurt ? colHurt : meno::colors::White,
                            .source = meno::Recti{playerFrame * 32, 0, 32, 32},
                            .flipX = facingLeft});

        for (const Bullet& b : bullets) {
            r.draw(*texBullet, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        // HUD
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, 6.f}}, {.fill = colXpBg});
        const float xpRatio =
            std::clamp(static_cast<float>(xp) / static_cast<float>(5 * level), 0.f, 1.f);
        r.drawRect({{0.f, 0.f}, {kViewW * xpRatio, 6.f}}, {.fill = colXpFill});

        r.drawRect({{16.f, 16.f}, {200.f, 12.f}}, {.fill = colHpBg});
        const float hpRatio = maxHp > 0.f ? std::clamp(hp / maxHp, 0.f, 1.f) : 0.f;
        r.drawRect({{16.f, 16.f}, {200.f * hpRatio, 12.f}}, {.fill = colHpFill});
        const int hpShown = std::max(0, static_cast<int>(std::ceil(hp)));
        textLeft(r, "HP " + std::to_string(hpShown), {226.f, 14.f}, 16);

        textRight(r, "LV " + std::to_string(level), {944.f, 14.f}, 16);
        textRight(r, "TIME " + formatTime(playTime), {944.f, 36.f}, 16);
        textRight(r, "KILLS " + std::to_string(kills), {944.f, 58.f}, 16);

        if (choosing) {
            for (int i = 0; i < 3; ++i) {
                const meno::Rectf rect = cardRect(i);
                r.drawRect(rect, {.fill = colCardBg, .outline = colCardBorder,
                                  .outlineThickness = -2.f});
                textCentered(r, kCardNames[static_cast<std::size_t>(cards[static_cast<std::size_t>(i)])],
                             rect.center(), 16);
            }
        } else if (paused) {
            textCentered(r, "PAUSED", {kViewW / 2.f, kViewH / 2.f}, 48);
        }
    });

    // ---- 결과 ----
    result.setUpdateHandler([&](double) {
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (in.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& r) {
        screenBg(r);
        textCentered(r, survived ? "SURVIVED" : "GAME OVER", {kViewW / 2.f, 140.f}, 48);
        textCentered(r, "TIME " + formatTime(playTime), {kViewW / 2.f, 230.f}, 32);
        textCentered(r, "LEVEL " + std::to_string(level), {kViewW / 2.f, 280.f}, 32);
        textCentered(r, "KILLS " + std::to_string(kills), {kViewW / 2.f, 330.f}, 32);
        textCentered(r, "R: RESTART  ESC: QUIT", {kViewW / 2.f, 420.f}, 16);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```