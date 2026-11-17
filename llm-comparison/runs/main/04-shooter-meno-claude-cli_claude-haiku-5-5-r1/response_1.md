```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWorldWidth = 1920.f;
constexpr float kWorldHeight = 1080.f;
constexpr float kCameraWidth = 960.f;
constexpr float kCameraHeight = 540.f;
constexpr float kPlayerEdge = 16.f;
constexpr float kPlayerSpeed = 240.f;
constexpr float kDiagonal = 0.70710678f;
constexpr float kPlayerRadius = 14.f;
constexpr float kEnemyRadius = 14.f;
constexpr float kEnemySpeed = 100.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletSpeed = 600.f;
constexpr double kBulletLife = 1.4;
constexpr double kFireInterval = 0.15;
constexpr double kInvulnerableTime = 1.0;
constexpr int kMaxHp = 3;
constexpr std::size_t kMaxEnemies = 200;
constexpr double kPi = 3.14159265358979323846;

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
    double life;
};

struct Enemy {
    meno::Vec2f pos;
};

float distanceSquared(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::string formatFixed1(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f", value);
    return buffer;
}

meno::Vec2f cameraCenterFor(meno::Vec2f player) {
    return meno::Vec2f{std::clamp(player.x, kCameraWidth / 2.f, kWorldWidth - kCameraWidth / 2.f),
                       std::clamp(player.y, kCameraHeight / 2.f, kWorldHeight - kCameraHeight / 2.f)};
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "ARENA"}};
    meno::ResourceManager& resources = game.resources();
    meno::SceneManager& scenes = game.scenes();

    // 에셋 로딩: 하나라도 실패하면 경로를 알리고 종료합니다.
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto playerTexture = resources.loadTexture("assets/textures/player.png");
    const auto enemyTexture = resources.loadTexture("assets/textures/enemy.png");
    const auto bulletTexture = resources.loadTexture("assets/textures/bullet.png");
    const auto shootBuffer = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    const auto hitBuffer = resources.loadSoundBuffer("assets/sfx/hit.wav");
    const auto loseBuffer = resources.loadSoundBuffer("assets/sfx/lose.wav");
    std::optional<meno::Music> bgmTrack =
        meno::Music::openFromFile("assets/music/bgm_shooter.ogg");

    std::vector<std::string> missing;
    const auto require = [&missing](bool loaded, const char* path) {
        if (!loaded) {
            missing.emplace_back(path);
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(playerTexture != nullptr, "assets/textures/player.png");
    require(enemyTexture != nullptr, "assets/textures/enemy.png");
    require(bulletTexture != nullptr, "assets/textures/bullet.png");
    require(shootBuffer != nullptr, "assets/sfx/shoot.wav");
    require(hitBuffer != nullptr, "assets/sfx/hit.wav");
    require(loseBuffer != nullptr, "assets/sfx/lose.wav");
    require(bgmTrack.has_value(), "assets/music/bgm_shooter.ogg");
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    meno::Sound shootSound{shootBuffer};
    meno::Sound hitSound{hitBuffer};
    meno::Sound loseSound{loseBuffer};
    meno::Music& bgm = *bgmTrack;
    bgm.setLooping(true);
    bgm.setVolume(0.4f);

    const meno::Color background = meno::Color::fromRgb(0x14181F);
    const meno::Color gridColor = meno::Color::fromRgb(0x222A35);
    const meno::Color borderColor = meno::Color::fromRgb(0x5C6B80);
    const meno::Camera2D hud{.center = {kCameraWidth / 2.f, kCameraHeight / 2.f},
                             .size = {kCameraWidth, kCameraHeight}};

    std::mt19937 rng{std::random_device{}()};

    // 플레이 상태 (플레이 Scene에 들어갈 때 초기화됩니다).
    meno::Vec2f playerPos{kWorldWidth / 2.f, kWorldHeight / 2.f};
    meno::Vec2f aimDir{1.f, 0.f};
    int hp = kMaxHp;
    int score = 0;
    double playTime = 0.0;
    double invulnTimer = 0.0;
    double fireTimer = 0.0;
    double spawnTimer = 0.5;
    double titleTimer = 0.0;
    bool paused = false;
    meno::Camera2D camera{.center = cameraCenterFor(playerPos),
                          .size = {kCameraWidth, kCameraHeight}};
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;

    const auto drawCentered = [&font](meno::Renderer& renderer, const std::string& text, float y,
                                      int size) {
        meno::TextParams params;
        params.characterSize = size;
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, meno::Vec2f{kCameraWidth / 2.f, y}, params);
    };

    const auto resetPlay = [&] {
        playerPos = meno::Vec2f{kWorldWidth / 2.f, kWorldHeight / 2.f};
        aimDir = meno::Vec2f{1.f, 0.f};
        hp = kMaxHp;
        score = 0;
        playTime = 0.0;
        invulnTimer = 0.0;
        fireTimer = 0.0;
        spawnTimer = 0.5;
        paused = false;
        camera.center = cameraCenterFor(playerPos);
        bullets.clear();
        enemies.clear();
    };

    // 카메라 시야를 사방으로 64px 넓힌 사각형의 테두리 위, 월드 안의 무작위 점에 적을 만듭니다.
    const auto spawnEnemy = [&] {
        const float halfW = kCameraWidth / 2.f + 64.f;
        const float halfH = kCameraHeight / 2.f + 64.f;
        const float left = camera.center.x - halfW;
        const float right = camera.center.x + halfW;
        const float top = camera.center.y - halfH;
        const float bottom = camera.center.y + halfH;
        const float w = right - left;
        const float h = bottom - top;
        const auto borderPoint = [&](float t) -> meno::Vec2f {
            if (t < w) {
                return meno::Vec2f{left + t, top};
            }
            t -= w;
            if (t < h) {
                return meno::Vec2f{right, top + t};
            }
            t -= h;
            if (t < w) {
                return meno::Vec2f{right - t, bottom};
            }
            t -= w;
            return meno::Vec2f{left, bottom - t};
        };
        std::uniform_real_distribution<float> along{0.f, 2.f * (w + h)};
        for (int attempt = 0; attempt < 100; ++attempt) {
            const meno::Vec2f p = borderPoint(along(rng));
            if (p.x >= 0.f && p.x <= kWorldWidth && p.y >= 0.f && p.y <= kWorldHeight) {
                enemies.push_back(Enemy{p});
                return;
            }
        }
    };

    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // 타이틀: Enter를 누르거나 2초가 지나면 플레이를 시작합니다.
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
        renderer.drawRect({{0.f, 0.f}, {kCameraWidth, kCameraHeight}}, {.fill = background});
        drawCentered(renderer, "ARENA", 220.f, 48);
        drawCentered(renderer, "PRESS ENTER", 300.f, 32);
    });

    // 플레이: 들어올 때마다 상태를 모두 초기화하고 배경음을 처음부터 재생합니다.
    play.setEnterHandler([&] {
        resetPlay();
        bgm.play();
    });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::P)) {
            paused = !paused;
            if (paused) {
                bgm.pause();
            } else {
                bgm.play();
            }
        }
        if (paused) {
            return;
        }

        const float fdt = static_cast<float>(dt);
        playTime += dt;
        invulnTimer = std::max(0.0, invulnTimer - dt);

        // 이동: WASD 또는 방향키, 대각선도 같은 속력.
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
        if (mx != 0.f && my != 0.f) {
            mx *= kDiagonal;
            my *= kDiagonal;
        }
        playerPos.x = std::clamp(playerPos.x + mx * kPlayerSpeed * fdt, kPlayerEdge,
                                 kWorldWidth - kPlayerEdge);
        playerPos.y = std::clamp(playerPos.y + my * kPlayerSpeed * fdt, kPlayerEdge,
                                 kWorldHeight - kPlayerEdge);

        // 카메라: 플레이어를 가운데에 두되 월드 밖이 보이지 않게 제한합니다.
        camera.center = cameraCenterFor(playerPos);

        // 조준: 커서가 가리키는 월드 위치 방향.
        const meno::Vec2f mouse{input.mousePosition()};
        const meno::Vec2f aimWorld =
            meno::screenToWorld(camera, game.renderer().framebufferSize(), mouse);
        const float ax = aimWorld.x - playerPos.x;
        const float ay = aimWorld.y - playerPos.y;
        const float aimLength = std::sqrt(ax * ax + ay * ay);
        if (aimLength > 0.001f) {
            aimDir = meno::Vec2f{ax / aimLength, ay / aimLength};
        }

        // 사격: 누르고 있는 동안 0.15초마다. 짧게 눌러도 한 발은 나갑니다.
        const bool pressedNow = input.wasPressed(meno::MouseButton::Left);
        if (pressedNow || input.isDown(meno::MouseButton::Left)) {
            if (pressedNow) {
                fireTimer = 0.0;
            }
            fireTimer -= dt;
            if (fireTimer <= 0.0) {
                fireTimer += kFireInterval;
                bullets.push_back(Bullet{playerPos,
                                         meno::Vec2f{aimDir.x * kBulletSpeed, aimDir.y * kBulletSpeed},
                                         kBulletLife});
                shootSound.play();
            }
        } else {
            fireTimer = 0.0;
        }

        // 총알 이동과 삭제.
        for (Bullet& b : bullets) {
            b.pos.x += b.vel.x * fdt;
            b.pos.y += b.vel.y * fdt;
            b.life -= dt;
        }
        std::erase_if(bullets, [](const Bullet& b) {
            return b.life <= 0.0 || b.pos.x < 0.f || b.pos.x > kWorldWidth || b.pos.y < 0.f ||
                   b.pos.y > kWorldHeight;
        });

        // 적 생성: 0.5초 뒤부터, 10초마다 간격이 0.1초씩 줄어 최소 0.3초.
        const double interval = std::max(0.3, 1.0 - 0.1 * std::floor(playTime / 10.0));
        spawnTimer -= dt;
        if (spawnTimer <= 0.0) {
            if (enemies.size() < kMaxEnemies) {
                spawnEnemy();
            }
            spawnTimer += interval;
        }

        // 적 이동: 플레이어를 향해 100 px/s.
        for (Enemy& e : enemies) {
            const float dx = playerPos.x - e.pos.x;
            const float dy = playerPos.y - e.pos.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.f) {
                const float step = std::min(length, kEnemySpeed * fdt);
                e.pos.x += dx / length * step;
                e.pos.y += dy / length * step;
            }
        }

        // 총알과 적: 둘 다 삭제하고 점수 +10.
        const float bulletHitDist = kBulletRadius + kEnemyRadius;
        std::erase_if(bullets, [&](const Bullet& b) {
            const auto it = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& e) {
                return distanceSquared(b.pos, e.pos) < bulletHitDist * bulletHitDist;
            });
            if (it == enemies.end()) {
                return false;
            }
            enemies.erase(it);
            score += 10;
            hitSound.play();
            return true;
        });

        // 플레이어와 적: 무적이 아닐 때 겹친 적 하나만 처리하고 무적을 시작합니다.
        if (invulnTimer <= 0.0) {
            const float playerHitDist = kPlayerRadius + kEnemyRadius;
            const auto it = std::find_if(enemies.begin(), enemies.end(), [&](const Enemy& e) {
                return distanceSquared(playerPos, e.pos) < playerHitDist * playerHitDist;
            });
            if (it != enemies.end()) {
                enemies.erase(it);
                --hp;
                invulnTimer = kInvulnerableTime;
                loseSound.play();
            }
        }

        if (hp <= 0) {
            (void)scenes.changeScene(result.id());
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);
        renderer.drawRect({{0.f, 0.f}, {kWorldWidth, kWorldHeight}}, {.fill = background});

        // 격자선 (120px 간격)과 월드 경계선.
        for (float x = 0.f; x <= kWorldWidth; x += 120.f) {
            renderer.drawLine({x, 0.f}, {x, kWorldHeight}, {.color = gridColor, .thickness = 1.f});
        }
        for (float y = 0.f; y <= kWorldHeight; y += 120.f) {
            renderer.drawLine({0.f, y}, {kWorldWidth, y}, {.color = gridColor, .thickness = 1.f});
        }
        renderer.drawLine({0.f, 0.f}, {kWorldWidth, 0.f}, {.color = borderColor, .thickness = 2.f});
        renderer.drawLine({0.f, kWorldHeight}, {kWorldWidth, kWorldHeight},
                          {.color = borderColor, .thickness = 2.f});
        renderer.drawLine({0.f, 0.f}, {0.f, kWorldHeight}, {.color = borderColor, .thickness = 2.f});
        renderer.drawLine({kWorldWidth, 0.f}, {kWorldWidth, kWorldHeight},
                          {.color = borderColor, .thickness = 2.f});

        for (const Enemy& e : enemies) {
            renderer.draw(*enemyTexture, {.position = e.pos, .origin = {16.f, 16.f}});
        }
        for (const Bullet& b : bullets) {
            renderer.draw(*bulletTexture, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        // 무적 중에는 반투명과 불투명을 0.1초마다 번갈아 보여 줍니다.
        meno::Color tint = meno::colors::White;
        if (invulnTimer > 0.0 && static_cast<int>(invulnTimer * 10.0) % 2 == 0) {
            tint = meno::colors::White.withAlpha(90);
        }
        const float angle = static_cast<float>(std::atan2(aimDir.y, aimDir.x) * 180.0 / kPi);
        renderer.draw(*playerTexture, {.position = playerPos,
                                       .origin = {16.f, 16.f},
                                       .rotation = angle,
                                       .tint = tint});

        // HUD는 화면 고정 카메라로 그립니다.
        renderer.setCamera(hud);
        const std::string hudText = "HP " + std::to_string(hp) + "  SCORE " +
                                    std::to_string(score) + "  TIME " + formatFixed1(playTime);
        renderer.drawText(*font, hudText, {16.f, 16.f}, {.characterSize = 32});
        if (paused) {
            drawCentered(renderer, "PAUSED", kCameraHeight / 2.f - 24.f, 48);
        }
    });

    // 결과: 배경음을 멈추고, R을 누르면 플레이를 다시 시작합니다.
    result.setEnterHandler([&] { bgm.stop(); });
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
        renderer.drawRect({{0.f, 0.f}, {kCameraWidth, kCameraHeight}}, {.fill = background});
        drawCentered(renderer, "GAME OVER", 200.f, 48);
        drawCentered(renderer, "SCORE " + std::to_string(score), 280.f, 32);
        drawCentered(renderer, "TIME " + formatFixed1(playTime), 330.f, 32);
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 430.f, 32);
    });

    (void)scenes.changeScene(title.id());
    game.run();
}
```