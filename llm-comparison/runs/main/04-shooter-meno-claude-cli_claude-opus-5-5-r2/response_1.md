```cpp
#include <meno/Meno.hpp>

#include <algorithm>
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

constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kEnemyRadius = 14.f;
constexpr float kBulletRadius = 4.f;
constexpr float kPlayerSpeed = 240.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kEnemySpeed = 100.f;
constexpr double kFireInterval = 0.15;
constexpr double kBulletLife = 1.4;
constexpr double kInvulnTime = 1.0;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kPi = 3.14159265358979323846f;

constexpr meno::Color kBgColor = meno::Color::fromRgb(0x14181F);
constexpr meno::Color kGridColor = meno::Color::fromRgb(0x222A35);
constexpr meno::Color kBorderColor = meno::Color::fromRgb(0x5C6B80);
constexpr meno::Color kTextColor = meno::Color::fromRgb(0xFFFFFF);

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
    double life = 0.0;
    bool alive = true;
};

struct Enemy {
    meno::Vec2f pos;
    bool alive = true;
};

// 같은 효과음을 겹쳐 낼 수 있도록 Sound 여러 개를 돌려 씁니다.
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

float dist2(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::string formatTime(double t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", t);
    return buf;
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "ARENA",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& resources = game.resources();

    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto playerTex = resources.loadTexture("assets/textures/player.png");
    const auto enemyTex = resources.loadTexture("assets/textures/enemy.png");
    const auto bulletTex = resources.loadTexture("assets/textures/bullet.png");
    const auto hitBuf = resources.loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuf = resources.loadSoundBuffer("assets/sfx/lose.wav");
    const auto shootBuf = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_shooter.ogg");

    std::vector<std::string> missing;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            missing.emplace_back(path);
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(playerTex != nullptr, "assets/textures/player.png");
    require(enemyTex != nullptr, "assets/textures/enemy.png");
    require(bulletTex != nullptr, "assets/textures/bullet.png");
    require(hitBuf != nullptr, "assets/sfx/hit.wav");
    require(loseBuf != nullptr, "assets/sfx/lose.wav");
    require(shootBuf != nullptr, "assets/sfx/shoot.wav");
    require(bgm.has_value(), "assets/music/bgm_shooter.ogg");
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    SoundPool shootSfx;
    SoundPool hitSfx;
    SoundPool loseSfx;
    for (int i = 0; i < 6; ++i) {
        shootSfx.sounds.emplace_back(shootBuf);
        hitSfx.sounds.emplace_back(hitBuf);
    }
    for (int i = 0; i < 2; ++i) {
        loseSfx.sounds.emplace_back(loseBuf);
    }

    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};
    meno::Camera2D camera{.center = {kWorldW / 2.f, kWorldH / 2.f}, .size = {kViewW, kViewH}};

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    meno::Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    meno::Vec2f aim{1.f, 0.f};
    float aimDeg = 0.f;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    bullets.reserve(32);
    enemies.reserve(kMaxEnemies + 8);
    int hp = 3;
    int score = 0;
    double playTime = 0.0;
    double fireCooldown = 0.0;
    double spawnCooldown = 0.5;
    double invuln = 0.0;
    bool paused = false;
    double titleTimer = 0.0;

    const auto clampCamera = [&] {
        camera.center.x = std::clamp(player.x, kViewW / 2.f, kWorldW - kViewW / 2.f);
        camera.center.y = std::clamp(player.y, kViewH / 2.f, kWorldH - kViewH / 2.f);
    };

    const auto resetGame = [&] {
        player = {kWorldW / 2.f, kWorldH / 2.f};
        aim = {1.f, 0.f};
        aimDeg = 0.f;
        bullets.clear();
        enemies.clear();
        hp = 3;
        score = 0;
        playTime = 0.0;
        fireCooldown = 0.0;
        spawnCooldown = 0.5;
        invuln = 0.0;
        paused = false;
        clampCamera();
    };

    const auto spawnEnemy = [&] {
        const float left = camera.center.x - kViewW / 2.f - 64.f;
        const float top = camera.center.y - kViewH / 2.f - 64.f;
        const float w = kViewW + 128.f;
        const float h = kViewH + 128.f;
        const float perimeter = 2.f * (w + h);
        std::uniform_real_distribution<float> dist{0.f, perimeter};
        for (int attempt = 0; attempt < 64; ++attempt) {
            float t = dist(rng);
            meno::Vec2f p;
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
            if (p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH) {
                enemies.push_back({p, true});
                return;
            }
        }
        // 드물게 실패하면 오른쪽 아래 모서리 근처의 테두리 점을 씁니다.
        meno::Vec2f p{std::clamp(left + w, 0.f, kWorldW), std::clamp(top + h, 0.f, kWorldH)};
        enemies.push_back({p, true});
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    const auto drawBackground = [&](meno::Renderer& r) {
        r.setCamera(hud);
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = kBgColor});
    };
    const auto centered = [&](meno::Renderer& r, const std::string& text, float y,
                              unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = kTextColor};
        params.origin = r.measureTextBounds(*font, text, params).center();
        r.drawText(*font, text, {kViewW / 2.f, y}, params);
    };

    // 타이틀
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        drawBackground(r);
        centered(r, "ARENA", 220.f, 48);
        centered(r, "PRESS ENTER", 310.f, 32);
    });

    // 플레이
    play.setEnterHandler([&] {
        resetGame();
        game.renderer().setCamera(camera);
        bgm->stop();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double fixedDt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::P)) {
            paused = !paused;
            if (paused) {
                bgm->pause();
            } else {
                bgm->play();
            }
        }
        if (paused) {
            return;
        }

        const float dt = static_cast<float>(fixedDt);
        playTime += fixedDt;

        // 이동
        meno::Vec2f dir{0.f, 0.f};
        if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left)) {
            dir.x -= 1.f;
        }
        if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right)) {
            dir.x += 1.f;
        }
        if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up)) {
            dir.y -= 1.f;
        }
        if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down)) {
            dir.y += 1.f;
        }
        const float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (dirLen > 0.f) {
            player.x += dir.x / dirLen * kPlayerSpeed * dt;
            player.y += dir.y / dirLen * kPlayerSpeed * dt;
        }
        player.x = std::clamp(player.x, 16.f, kWorldW - 16.f);
        player.y = std::clamp(player.y, 16.f, kWorldH - 16.f);

        clampCamera();
        game.renderer().setCamera(camera);

        // 조준
        const meno::Vec2f mouse{input.mousePosition()};
        const meno::Vec2f target =
            meno::screenToWorld(camera, game.renderer().framebufferSize(), mouse);
        const float ax = target.x - player.x;
        const float ay = target.y - player.y;
        const float aimLen = std::sqrt(ax * ax + ay * ay);
        if (aimLen > 0.0001f) {
            aim = {ax / aimLen, ay / aimLen};
            aimDeg = std::atan2(ay, ax) * 180.f / kPi;
        }

        // 사격
        fireCooldown = std::max(fireCooldown - fixedDt, 0.0);
        const bool firing = input.isDown(meno::MouseButton::Left) ||
                            input.wasPressed(meno::MouseButton::Left);
        if (firing && fireCooldown <= 1e-9) {
            bullets.push_back({player, aim * kBulletSpeed, 0.0, true});
            shootSfx.play();
            fireCooldown += kFireInterval;
        }

        // 총알 이동
        for (Bullet& b : bullets) {
            b.pos += b.vel * dt;
            b.life += fixedDt;
            if (b.life >= kBulletLife || b.pos.x < 0.f || b.pos.x > kWorldW || b.pos.y < 0.f ||
                b.pos.y > kWorldH) {
                b.alive = false;
            }
        }

        // 적 이동
        const float enemyStep = kEnemySpeed * dt;
        for (Enemy& e : enemies) {
            const float dx = player.x - e.pos.x;
            const float dy = player.y - e.pos.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 0.0001f) {
                const float s = std::min(enemyStep, d);
                e.pos.x += dx / d * s;
                e.pos.y += dy / d * s;
            }
        }

        // 총알과 적
        const float beR = kBulletRadius + kEnemyRadius;
        for (Bullet& b : bullets) {
            if (!b.alive) {
                continue;
            }
            for (Enemy& e : enemies) {
                if (e.alive && dist2(b.pos, e.pos) < beR * beR) {
                    b.alive = false;
                    e.alive = false;
                    score += 10;
                    hitSfx.play();
                    break;
                }
            }
        }

        // 적과 플레이어
        if (invuln > 0.0) {
            invuln = std::max(invuln - fixedDt, 0.0);
        } else {
            const float peR = kPlayerRadius + kEnemyRadius;
            for (Enemy& e : enemies) {
                if (e.alive && dist2(player, e.pos) < peR * peR) {
                    e.alive = false;
                    --hp;
                    loseSfx.play();
                    invuln = kInvulnTime;
                    break;
                }
            }
        }

        std::erase_if(bullets, [](const Bullet& b) { return !b.alive; });
        std::erase_if(enemies, [](const Enemy& e) { return !e.alive; });

        if (hp <= 0) {
            hp = 0;
            (void)scenes.changeScene(result.id());
            return;
        }

        // 적 생성
        spawnCooldown -= fixedDt;
        if (spawnCooldown <= 1e-9) {
            if (enemies.size() < kMaxEnemies) {
                spawnEnemy();
            }
            const double interval =
                std::max(0.3, 1.0 - 0.1 * std::floor(playTime / 10.0));
            spawnCooldown += interval;
        }
    });
    play.setDrawHandler([&](meno::Renderer& r) {
        drawBackground(r);

        r.setCamera(camera);
        for (float x = 120.f; x < kWorldW; x += 120.f) {
            r.drawLine({x, 0.f}, {x, kWorldH}, {.color = kGridColor, .thickness = 1.f});
        }
        for (float y = 120.f; y < kWorldH; y += 120.f) {
            r.drawLine({0.f, y}, {kWorldW, y}, {.color = kGridColor, .thickness = 1.f});
        }
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                   {.fill = meno::colors::Transparent,
                    .outline = kBorderColor,
                    .outlineThickness = -2.f});

        for (const Enemy& e : enemies) {
            r.draw(*enemyTex, {.position = e.pos, .origin = {16.f, 16.f}});
        }
        for (const Bullet& b : bullets) {
            r.draw(*bulletTex, {.position = b.pos, .origin = {4.f, 4.f}});
        }
        meno::Color tint = meno::colors::White;
        if (invuln > 0.0) {
            const bool dim = static_cast<int>(invuln / 0.1) % 2 == 0;
            tint = meno::colors::White.withAlpha(dim ? 80 : 200);
        }
        r.draw(*playerTex,
               {.position = player, .origin = {16.f, 16.f}, .rotation = aimDeg, .tint = tint});

        r.setCamera(hud);
        const std::string hudText = "HP " + std::to_string(hp) + "  SCORE " +
                                    std::to_string(score) + "  TIME " + formatTime(playTime);
        r.drawText(*font, hudText, {16.f, 16.f}, {.characterSize = 16, .color = kTextColor});
        if (paused) {
            centered(r, "PAUSED", kViewH / 2.f, 48);
        }
    });

    // 결과
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
        drawBackground(r);
        centered(r, "GAME OVER", 170.f, 48);
        centered(r, "SCORE " + std::to_string(score), 250.f, 32);
        centered(r, "TIME " + formatTime(playTime), 300.f, 32);
        centered(r, "R: RESTART  ESC: QUIT", 380.f, 32);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```