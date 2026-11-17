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

constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;

constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerMargin = 16.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletLife = 1.4f;
constexpr double kFireInterval = 0.15;
constexpr float kEnemyRadius = 14.f;
constexpr float kEnemySpeed = 100.f;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kSpawnMargin = 64.f;
constexpr int kStartHp = 3;
constexpr double kInvulnTime = 1.0;
constexpr float kPi = 3.14159265358979323846f;

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
    float age = 0.f;
};

struct Enemy {
    meno::Vec2f pos;
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

float lengthOf(meno::Vec2f v) { return std::sqrt(v.x * v.x + v.y * v.y); }

float distSq(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

meno::Vec2f clampCamera(meno::Vec2f center) {
    return {std::clamp(center.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
            std::clamp(center.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
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
    const auto shootBuf = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuf = resources.loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuf = resources.loadSoundBuffer("assets/sfx/lose.wav");
    std::optional<meno::Music> music = meno::Music::openFromFile("assets/music/bgm_shooter.ogg");

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(playerTex != nullptr, "assets/textures/player.png");
    require(enemyTex != nullptr, "assets/textures/enemy.png");
    require(bulletTex != nullptr, "assets/textures/bullet.png");
    require(shootBuf != nullptr, "assets/sfx/shoot.wav");
    require(hitBuf != nullptr, "assets/sfx/hit.wav");
    require(loseBuf != nullptr, "assets/sfx/lose.wav");
    require(music.has_value(), "assets/music/bgm_shooter.ogg");
    if (failed) {
        return 1;
    }
    music->setLooping(true);
    music->setVolume(0.4f);

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

    const meno::Color bgColor = meno::Color::fromRgb(0x14181F);
    const meno::Color gridColor = meno::Color::fromRgb(0x222A35);
    const meno::Color borderColor = meno::Color::fromRgb(0x5C6B80);
    const meno::Color textColor = meno::Color::fromRgb(0xFFFFFF);
    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    meno::Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    float aimAngle = 0.f;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    int hp = kStartHp;
    int score = 0;
    double playTime = 0.0;
    double fireCooldown = 0.0;
    double nextSpawn = 0.5;
    double invuln = 0.0;
    bool paused = false;
    meno::Camera2D camera{.center = clampCamera(player), .size = {kViewW, kViewH}};

    int finalScore = 0;
    double finalTime = 0.0;
    double titleTimer = 0.0;

    const auto resetGame = [&] {
        player = {kWorldW / 2.f, kWorldH / 2.f};
        aimAngle = 0.f;
        bullets.clear();
        enemies.clear();
        hp = kStartHp;
        score = 0;
        playTime = 0.0;
        fireCooldown = 0.0;
        nextSpawn = 0.5;
        invuln = 0.0;
        paused = false;
        camera.center = clampCamera(player);
        game.renderer().setCamera(camera);
    };

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text,
                                  unsigned int size, meno::Vec2f pos) {
        meno::TextParams params{.characterSize = size, .color = textColor};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, pos, params);
    };

    const auto spawnEnemy = [&] {
        const float left = camera.center.x - kViewW / 2.f - kSpawnMargin;
        const float right = camera.center.x + kViewW / 2.f + kSpawnMargin;
        const float top = camera.center.y - kViewH / 2.f - kSpawnMargin;
        const float bottom = camera.center.y + kViewH / 2.f + kSpawnMargin;

        struct Segment {
            meno::Vec2f a;
            meno::Vec2f b;
            float len;
        };
        std::vector<Segment> segments;
        const auto addHorizontal = [&](float y) {
            if (y < 0.f || y > kWorldH) {
                return;
            }
            const float x0 = std::max(left, 0.f);
            const float x1 = std::min(right, kWorldW);
            if (x1 > x0) {
                segments.push_back({{x0, y}, {x1, y}, x1 - x0});
            }
        };
        const auto addVertical = [&](float x) {
            if (x < 0.f || x > kWorldW) {
                return;
            }
            const float y0 = std::max(top, 0.f);
            const float y1 = std::min(bottom, kWorldH);
            if (y1 > y0) {
                segments.push_back({{x, y0}, {x, y1}, y1 - y0});
            }
        };
        addHorizontal(top);
        addHorizontal(bottom);
        addVertical(left);
        addVertical(right);

        meno::Vec2f pos;
        if (segments.empty()) {
            std::uniform_real_distribution<float> rx{0.f, kWorldW};
            std::uniform_real_distribution<float> ry{0.f, kWorldH};
            pos = {rx(rng), ry(rng)};
        } else {
            float total = 0.f;
            for (const Segment& s : segments) {
                total += s.len;
            }
            std::uniform_real_distribution<float> pick{0.f, total};
            float r = pick(rng);
            const Segment* chosen = &segments.back();
            for (const Segment& s : segments) {
                if (r <= s.len) {
                    chosen = &s;
                    break;
                }
                r -= s.len;
            }
            const float t = std::clamp(r / chosen->len, 0.f, 1.f);
            pos = chosen->a + (chosen->b - chosen->a) * t;
        }
        enemies.push_back({pos});
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---- 타이틀 ----
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
    title.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(hud);
        renderer.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = bgColor});
        drawCentered(renderer, "ARENA", 48, {kViewW / 2.f, 220.f});
        drawCentered(renderer, "PRESS ENTER", 32, {kViewW / 2.f, 310.f});
    });

    // ---- 플레이 ----
    play.setEnterHandler([&] {
        resetGame();
        music->stop();
        music->play();
    });
    play.setExitHandler([&] { music->stop(); });
    play.setUpdateHandler([&](double fixedDt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::P)) {
            paused = !paused;
            if (paused) {
                music->pause();
            } else {
                music->play();
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
        const float dirLen = lengthOf(dir);
        if (dirLen > 0.f) {
            player += dir * (kPlayerSpeed * dt / dirLen);
        }
        player.x = std::clamp(player.x, kPlayerMargin, kWorldW - kPlayerMargin);
        player.y = std::clamp(player.y, kPlayerMargin, kWorldH - kPlayerMargin);

        // 카메라
        camera.center = clampCamera(player);
        game.renderer().setCamera(camera);

        // 조준
        const meno::Vec2f mouseScreen{input.mousePosition()};
        const meno::Vec2f mouseWorld = game.renderer().screenToWorld(mouseScreen);
        meno::Vec2f aim = mouseWorld - player;
        float aimLen = lengthOf(aim);
        if (aimLen > 0.0001f) {
            aimAngle = std::atan2(aim.y, aim.x) * 180.f / kPi;
            aim = aim * (1.f / aimLen);
        } else {
            const float rad = aimAngle * kPi / 180.f;
            aim = {std::cos(rad), std::sin(rad)};
        }

        // 사격
        fireCooldown -= fixedDt;
        if (fireCooldown < 0.0) {
            fireCooldown = 0.0;
        }
        const bool fireHeld = input.isDown(meno::MouseButton::Left) ||
                              input.wasPressed(meno::MouseButton::Left);
        if (fireHeld && fireCooldown <= 0.0) {
            bullets.push_back({player, aim * kBulletSpeed, 0.f});
            shootSfx.play();
            fireCooldown = kFireInterval;
        }

        // 총알 이동과 삭제
        for (Bullet& b : bullets) {
            b.pos += b.vel * dt;
            b.age += dt;
        }
        std::erase_if(bullets, [](const Bullet& b) {
            return b.age >= kBulletLife || b.pos.x < 0.f || b.pos.x > kWorldW || b.pos.y < 0.f ||
                   b.pos.y > kWorldH;
        });

        // 적 생성
        while (playTime >= nextSpawn) {
            if (enemies.size() < kMaxEnemies) {
                spawnEnemy();
            }
            const double level = std::floor(playTime / 10.0);
            const double interval = std::max(0.3, 1.0 - 0.1 * level);
            nextSpawn += interval;
        }

        // 적 이동
        const float enemyStep = kEnemySpeed * dt;
        for (Enemy& e : enemies) {
            const meno::Vec2f to = player - e.pos;
            const float d = lengthOf(to);
            if (d <= enemyStep) {
                e.pos = player;
            } else {
                e.pos += to * (enemyStep / d);
            }
        }

        // 총알-적 충돌
        constexpr float beHit = (kBulletRadius + kEnemyRadius) * (kBulletRadius + kEnemyRadius);
        std::vector<bool> enemyDead(enemies.size(), false);
        std::vector<bool> bulletDead(bullets.size(), false);
        for (std::size_t bi = 0; bi < bullets.size(); ++bi) {
            for (std::size_t ei = 0; ei < enemies.size(); ++ei) {
                if (enemyDead[ei]) {
                    continue;
                }
                if (distSq(bullets[bi].pos, enemies[ei].pos) < beHit) {
                    enemyDead[ei] = true;
                    bulletDead[bi] = true;
                    score += 10;
                    hitSfx.play();
                    break;
                }
            }
        }
        {
            std::size_t i = 0;
            std::erase_if(bullets, [&](const Bullet&) { return bulletDead[i++]; });
            std::size_t j = 0;
            std::erase_if(enemies, [&](const Enemy&) { return enemyDead[j++]; });
        }

        // 적-플레이어 충돌
        if (invuln > 0.0) {
            invuln -= fixedDt;
            if (invuln < 0.0) {
                invuln = 0.0;
            }
        } else {
            constexpr float pe = (kPlayerRadius + kEnemyRadius) * (kPlayerRadius + kEnemyRadius);
            for (std::size_t ei = 0; ei < enemies.size(); ++ei) {
                if (distSq(player, enemies[ei].pos) < pe) {
                    enemies.erase(enemies.begin() + static_cast<std::ptrdiff_t>(ei));
                    --hp;
                    loseSfx.play();
                    invuln = kInvulnTime;
                    break;
                }
            }
        }

        if (hp <= 0) {
            hp = 0;
            finalScore = score;
            finalTime = playTime;
            (void)scenes.changeScene(result.id());
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        // 배경
        renderer.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = bgColor});
        // 격자선
        for (float x = 120.f; x < kWorldW; x += 120.f) {
            renderer.drawLine({x, 0.f}, {x, kWorldH}, {.color = gridColor, .thickness = 1.f});
        }
        for (float y = 120.f; y < kWorldH; y += 120.f) {
            renderer.drawLine({0.f, y}, {kWorldW, y}, {.color = gridColor, .thickness = 1.f});
        }
        // 월드 경계
        renderer.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                          {.fill = meno::colors::Transparent,
                           .outline = borderColor,
                           .outlineThickness = -2.f});

        const meno::Rectf visible = renderer.visibleWorldBounds();
        for (const Enemy& e : enemies) {
            const meno::Rectf box{{e.pos.x - 16.f, e.pos.y - 16.f}, {32.f, 32.f}};
            if (!box.intersects(visible)) {
                continue;
            }
            renderer.draw(*enemyTex, {.position = e.pos, .origin = {16.f, 16.f}});
        }
        for (const Bullet& b : bullets) {
            renderer.draw(*bulletTex, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        meno::Color tint = meno::colors::White;
        if (invuln > 0.0 && std::fmod(invuln, 0.2) >= 0.1) {
            tint = meno::colors::White.withAlpha(80);
        }
        renderer.draw(*playerTex, {.position = player,
                                   .origin = {16.f, 16.f},
                                   .rotation = aimAngle,
                                   .tint = tint});

        // HUD
        renderer.setCamera(hud);
        const std::string hudText = "HP " + std::to_string(hp) + "  SCORE " +
                                    std::to_string(score) + "  TIME " + formatTime(playTime);
        renderer.drawText(*font, hudText, {16.f, 12.f}, {.characterSize = 32, .color = textColor});
        if (paused) {
            drawCentered(renderer, "PAUSED", 48, {kViewW / 2.f, kViewH / 2.f});
        }
    });

    // ---- 결과 ----
    result.setEnterHandler([&] { music->stop(); });
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
    result.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(hud);
        renderer.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = bgColor});
        drawCentered(renderer, "GAME OVER", 48, {kViewW / 2.f, 170.f});
        drawCentered(renderer, "SCORE " + std::to_string(finalScore), 32, {kViewW / 2.f, 250.f});
        drawCentered(renderer, "TIME " + formatTime(finalTime), 32, {kViewW / 2.f, 300.f});
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 32, {kViewW / 2.f, 380.f});
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
