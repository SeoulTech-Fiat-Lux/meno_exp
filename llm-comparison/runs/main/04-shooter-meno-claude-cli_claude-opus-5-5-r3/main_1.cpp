#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;

constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerMargin = 16.f;

constexpr float kBulletSpeed = 600.f;
constexpr float kBulletRadius = 4.f;
constexpr double kBulletLife = 1.4;
constexpr double kFireInterval = 0.15;

constexpr float kEnemySpeed = 100.f;
constexpr float kEnemyRadius = 14.f;
constexpr std::size_t kMaxEnemies = 200;

constexpr int kStartHp = 3;
constexpr double kInvulnTime = 1.0;

const meno::Color kBgColor = meno::Color::fromRgb(0x14181F);
const meno::Color kGridColor = meno::Color::fromRgb(0x222A35);
const meno::Color kBorderColor = meno::Color::fromRgb(0x5C6B80);
const meno::Color kTextColor = meno::Color::fromRgb(0xFFFFFF);

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
    double life = kBulletLife;
    bool alive = true;
};

struct Enemy {
    meno::Vec2f pos;
    bool alive = true;
};

float distSq(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::string formatTime(double t) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", t);
    return std::string{buf};
}

// 같은 효과음을 겹쳐 낼 수 있도록 Sound 여러 개를 돌려 씁니다.
struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;

    void init(const std::shared_ptr<const meno::SoundBuffer>& buffer, int count) {
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

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "ARENA",
                     .clearColor = meno::colors::Black}};
    meno::ResourceManager& resources = game.resources();

    const char* fontPath = "assets/fonts/game.ttf";
    const char* hitPath = "assets/sfx/hit.wav";
    const char* losePath = "assets/sfx/lose.wav";
    const char* shootPath = "assets/sfx/shoot.wav";
    const char* musicPath = "assets/music/bgm_shooter.ogg";
    const char* playerPath = "assets/textures/player.png";
    const char* enemyPath = "assets/textures/enemy.png";
    const char* bulletPath = "assets/textures/bullet.png";

    const auto font = resources.loadFont(fontPath);
    const auto hitBuffer = resources.loadSoundBuffer(hitPath);
    const auto loseBuffer = resources.loadSoundBuffer(losePath);
    const auto shootBuffer = resources.loadSoundBuffer(shootPath);
    std::optional<meno::Music> music = meno::Music::openFromFile(musicPath);
    const auto playerTex = resources.loadTexture(playerPath);
    const auto enemyTex = resources.loadTexture(enemyPath);
    const auto bulletTex = resources.loadTexture(bulletPath);

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, fontPath);
    require(hitBuffer != nullptr, hitPath);
    require(loseBuffer != nullptr, losePath);
    require(shootBuffer != nullptr, shootPath);
    require(music.has_value(), musicPath);
    require(playerTex != nullptr, playerPath);
    require(enemyTex != nullptr, enemyPath);
    require(bulletTex != nullptr, bulletPath);
    if (failed) {
        return 1;
    }

    music->setLooping(true);
    music->setVolume(0.4f);

    SoundPool shootSfx;
    SoundPool hitSfx;
    SoundPool loseSfx;
    shootSfx.init(shootBuffer, 8);
    hitSfx.init(hitBuffer, 8);
    loseSfx.init(loseBuffer, 2);

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    meno::Vec2f playerPos{kWorldW / 2.f, kWorldH / 2.f};
    float playerAngle = 0.f;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    int hp = kStartHp;
    int score = 0;
    double playTime = 0.0;
    double fireCooldown = 0.0;
    double spawnTimer = 0.5;
    double invuln = 0.0;
    bool paused = false;
    double titleTimer = 0.0;

    meno::Camera2D camera{.center = playerPos, .size = {kViewW, kViewH}};
    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};

    const auto updateCamera = [&] {
        camera.center.x = std::clamp(playerPos.x, kViewW / 2.f, kWorldW - kViewW / 2.f);
        camera.center.y = std::clamp(playerPos.y, kViewH / 2.f, kWorldH - kViewH / 2.f);
        game.renderer().setCamera(camera);
    };

    const auto resetGame = [&] {
        playerPos = {kWorldW / 2.f, kWorldH / 2.f};
        playerAngle = 0.f;
        bullets.clear();
        enemies.clear();
        hp = kStartHp;
        score = 0;
        playTime = 0.0;
        fireCooldown = 0.0;
        spawnTimer = 0.5;
        invuln = 0.0;
        paused = false;
        updateCamera();
    };

    const auto spawnPoint = [&]() -> meno::Vec2f {
        const float l = camera.center.x - kViewW / 2.f - 64.f;
        const float r = camera.center.x + kViewW / 2.f + 64.f;
        const float t = camera.center.y - kViewH / 2.f - 64.f;
        const float b = camera.center.y + kViewH / 2.f + 64.f;
        const float w = r - l;
        const float h = b - t;
        const float perimeter = 2.f * (w + h);
        std::uniform_real_distribution<float> dist{0.f, perimeter};
        meno::Vec2f p{l, t};
        for (int attempt = 0; attempt < 64; ++attempt) {
            float s = dist(rng);
            if (s < w) {
                p = {l + s, t};
            } else if ((s -= w) < h) {
                p = {r, t + s};
            } else if ((s -= h) < w) {
                p = {r - s, b};
            } else {
                s -= w;
                p = {l, b - s};
            }
            if (p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH) {
                return p;
            }
        }
        p.x = std::clamp(p.x, 0.f, kWorldW);
        p.y = std::clamp(p.y, 0.f, kWorldH);
        return p;
    };

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text,
                                  unsigned int size, meno::Vec2f pos) {
        meno::TextParams params{.characterSize = size, .color = kTextColor};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, pos, params);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---------- 타이틀 ----------
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
        renderer.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = kBgColor});
        drawCentered(renderer, "ARENA", 48, {kViewW / 2.f, 220.f});
        drawCentered(renderer, "PRESS ENTER", 32, {kViewW / 2.f, 300.f});
    });

    // ---------- 플레이 ----------
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
        const float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (dirLen > 0.f) {
            playerPos += dir * (kPlayerSpeed * dt / dirLen);
        }
        playerPos.x = std::clamp(playerPos.x, kPlayerMargin, kWorldW - kPlayerMargin);
        playerPos.y = std::clamp(playerPos.y, kPlayerMargin, kWorldH - kPlayerMargin);

        updateCamera();

        // 조준
        const meno::Vec2f mouse{input.mousePosition()};
        const meno::Vec2f target = game.renderer().screenToWorld(mouse);
        const float ax = target.x - playerPos.x;
        const float ay = target.y - playerPos.y;
        if (ax * ax + ay * ay > 1e-4f) {
            playerAngle = std::atan2(ay, ax) * 180.f / std::numbers::pi_v<float>;
        }
        const float rad = playerAngle * std::numbers::pi_v<float> / 180.f;
        const meno::Vec2f aimDir{std::cos(rad), std::sin(rad)};

        // 사격
        const bool wantFire = input.isDown(meno::MouseButton::Left) ||
                              input.wasPressed(meno::MouseButton::Left);
        fireCooldown -= fixedDt;
        if (wantFire && fireCooldown <= 1e-6) {
            bullets.push_back({playerPos, aimDir * kBulletSpeed});
            shootSfx.play();
            fireCooldown += kFireInterval;
        }
        if (!wantFire && fireCooldown < 0.0) {
            fireCooldown = 0.0;
        }

        // 총알 이동
        for (Bullet& b : bullets) {
            b.pos += b.vel * dt;
            b.life -= fixedDt;
            if (b.life <= 0.0 || b.pos.x < 0.f || b.pos.x > kWorldW || b.pos.y < 0.f ||
                b.pos.y > kWorldH) {
                b.alive = false;
            }
        }

        // 적 생성
        spawnTimer -= fixedDt;
        if (spawnTimer <= 1e-9) {
            const double interval =
                std::max(0.3, 1.0 - 0.1 * std::floor(playTime / 10.0));
            if (enemies.size() < kMaxEnemies) {
                enemies.push_back({spawnPoint()});
            }
            spawnTimer += interval;
        }

        // 적 이동
        for (Enemy& e : enemies) {
            const float dx = playerPos.x - e.pos.x;
            const float dy = playerPos.y - e.pos.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 1e-4f) {
                const float step = std::min(kEnemySpeed * dt, len);
                e.pos.x += dx / len * step;
                e.pos.y += dy / len * step;
            }
        }

        // 총알-적 충돌
        const float beR = kBulletRadius + kEnemyRadius;
        for (Bullet& b : bullets) {
            if (!b.alive) {
                continue;
            }
            for (Enemy& e : enemies) {
                if (e.alive && distSq(b.pos, e.pos) < beR * beR) {
                    b.alive = false;
                    e.alive = false;
                    score += 10;
                    hitSfx.play();
                    break;
                }
            }
        }

        // 적-플레이어 충돌
        if (invuln > 0.0) {
            invuln -= fixedDt;
        }
        if (invuln <= 0.0) {
            const float peR = kPlayerRadius + kEnemyRadius;
            for (Enemy& e : enemies) {
                if (e.alive && distSq(playerPos, e.pos) < peR * peR) {
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
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        // 월드 배경
        renderer.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = kBgColor});
        for (float x = 120.f; x < kWorldW; x += 120.f) {
            renderer.drawLine({x, 0.f}, {x, kWorldH}, {.color = kGridColor, .thickness = 1.f});
        }
        for (float y = 120.f; y < kWorldH; y += 120.f) {
            renderer.drawLine({0.f, y}, {kWorldW, y}, {.color = kGridColor, .thickness = 1.f});
        }
        renderer.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                          {.fill = meno::colors::Transparent,
                           .outline = kBorderColor,
                           .outlineThickness = -2.f});

        for (const Enemy& e : enemies) {
            renderer.draw(*enemyTex, {.position = e.pos, .origin = {16.f, 16.f}});
        }
        for (const Bullet& b : bullets) {
            renderer.draw(*bulletTex, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        std::uint8_t alpha = 255;
        if (invuln > 0.0) {
            alpha = (static_cast<int>(invuln * 10.0) % 2 == 0) ? std::uint8_t{90}
                                                                : std::uint8_t{255};
        }
        renderer.draw(*playerTex, {.position = playerPos,
                                   .origin = {16.f, 16.f},
                                   .rotation = playerAngle,
                                   .tint = meno::colors::White.withAlpha(alpha)});

        // HUD
        renderer.setCamera(hud);
        const std::string hudText = "HP " + std::to_string(hp) + "  SCORE " +
                                    std::to_string(score) + "  TIME " + formatTime(playTime);
        renderer.drawText(*font, hudText, {16.f, 16.f},
                          {.characterSize = 16, .color = kTextColor});
        if (paused) {
            drawCentered(renderer, "PAUSED", 48, {kViewW / 2.f, kViewH / 2.f});
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
    result.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(hud);
        renderer.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = kBgColor});
        drawCentered(renderer, "GAME OVER", 48, {kViewW / 2.f, 190.f});
        drawCentered(renderer, "SCORE " + std::to_string(score), 32, {kViewW / 2.f, 260.f});
        drawCentered(renderer, "TIME " + formatTime(playTime), 32, {kViewW / 2.f, 310.f});
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 16, {kViewW / 2.f, 390.f});
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
