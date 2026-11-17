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

using meno::Vec2f;

constexpr float kPi = 3.14159265358979f;
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerEdge = 16.f;
constexpr float kEnemyRadius = 14.f;
constexpr float kEnemySpeed = 100.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletLife = 1.4f;
constexpr double kFireInterval = 0.15;
constexpr double kFirstSpawn = 0.5;
constexpr double kInvincibleTime = 1.0;
constexpr double kTitleTime = 2.0;
constexpr int kMaxHp = 3;
constexpr int kHitScore = 10;
constexpr std::size_t kMaxEnemies = 200;

struct Bullet {
    Vec2f pos;
    Vec2f vel;
    float life;
};

struct Enemy {
    Vec2f pos;
};

float vecLength(Vec2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Vec2f normalized(Vec2f v) {
    const float len = vecLength(v);
    return len > 0.f ? Vec2f{v.x / len, v.y / len} : Vec2f{0.f, 0.f};
}

float distSq(Vec2f a, Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool inWorld(Vec2f p) {
    return p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
}

Vec2f cameraCenterFor(Vec2f p) {
    return Vec2f{std::clamp(p.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                 std::clamp(p.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
}

double spawnInterval(double playTime) {
    return std::max(0.3, 1.0 - 0.1 * std::floor(playTime / 10.0));
}

std::string formatTime(double seconds) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f", seconds);
    return std::string{buf};
}

template <typename T>
void removeMarked(std::vector<T>& items, const std::vector<bool>& dead) {
    std::size_t out = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!dead[i]) {
            items[out++] = items[i];
        }
    }
    items.resize(out);
}

meno::TextParams makeText(unsigned int size) {
    meno::TextParams params{};
    params.characterSize = size;
    params.color = meno::colors::White;
    return params;
}

void drawCentered(meno::Renderer& renderer, const meno::Font& font, const std::string& text,
                  float y, unsigned int size) {
    meno::TextParams params = makeText(size);
    params.origin = renderer.measureTextBounds(font, text, params).center();
    renderer.drawText(font, text, Vec2f{480.f, y}, params);
}

struct World {
    Vec2f player{kWorldW / 2.f, kWorldH / 2.f};
    float facing = 0.f;
    Vec2f aimDir{1.f, 0.f};
    meno::Camera2D camera{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    int hp = kMaxHp;
    int score = 0;
    double playTime = 0.0;
    double invincible = 0.0;
    double nextSpawn = kFirstSpawn;
    double fireCooldown = 0.0;
    bool shotQueued = false;
    bool paused = false;

    void reset() { *this = World{}; }
};

}  // namespace

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
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_shooter.ogg");

    bool ok = true;
    const auto require = [&ok](const auto& asset, const char* path) {
        if (!asset) {
            std::cerr << "Failed to load: " << path << '\n';
            ok = false;
        }
    };
    require(font, "assets/fonts/game.ttf");
    require(playerTex, "assets/textures/player.png");
    require(enemyTex, "assets/textures/enemy.png");
    require(bulletTex, "assets/textures/bullet.png");
    require(shootBuf, "assets/sfx/shoot.wav");
    require(hitBuf, "assets/sfx/hit.wav");
    require(loseBuf, "assets/sfx/lose.wav");
    require(bgm, "assets/music/bgm_shooter.ogg");
    if (!ok) {
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);
    meno::Sound shootSound{shootBuf};
    meno::Sound hitSound{hitBuf};
    meno::Sound loseSound{loseBuf};

    const meno::Color background = meno::Color::fromRgb(0x14181F);
    const meno::Color gridColor = meno::Color::fromRgb(0x222A35);
    const meno::Color borderColor = meno::Color::fromRgb(0x5C6B80);
    const meno::Camera2D hud{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};

    World world;
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> sideDist{0, 3};
    std::uniform_real_distribution<float> unitDist{0.f, 1.f};
    double titleTimer = 0.0;

    // 카메라 시야를 사방으로 64px 넓힌 사각형의 테두리 위, 월드 안의 무작위 점
    const auto spawnPoint = [&]() -> Vec2f {
        const Vec2f c = world.camera.center;
        const float left = c.x - kViewW / 2.f - 64.f;
        const float right = c.x + kViewW / 2.f + 64.f;
        const float top = c.y - kViewH / 2.f - 64.f;
        const float bottom = c.y + kViewH / 2.f + 64.f;
        for (int attempt = 0; attempt < 32; ++attempt) {
            const float u = unitDist(rng);
            Vec2f p{left, top};
            switch (sideDist(rng)) {
                case 0: p = Vec2f{left + u * (right - left), top}; break;
                case 1: p = Vec2f{left + u * (right - left), bottom}; break;
                case 2: p = Vec2f{left, top + u * (bottom - top)}; break;
                default: p = Vec2f{right, top + u * (bottom - top)}; break;
            }
            if (inWorld(p)) {
                return p;
            }
        }
        return Vec2f{std::clamp(left, 0.f, kWorldW), std::clamp(top, 0.f, kWorldH)};
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // 타이틀
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += fixedDeltaTime;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleTime) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(hud);
        renderer.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = background});
        if (font) {
            drawCentered(renderer, *font, "ARENA", 200.f, 48);
            drawCentered(renderer, *font, "PRESS ENTER", 300.f, 32);
        }
    });

    // 플레이
    play.setEnterHandler([&] {
        world.reset();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::P)) {
            world.paused = !world.paused;
            if (world.paused) {
                bgm->pause();
            } else {
                bgm->play();
            }
        }
        if (world.paused) {
            return;
        }

        const float dt = static_cast<float>(fixedDeltaTime);
        meno::Renderer& renderer = game.renderer();

        // 이동 (8방향, 대각선도 같은 속력)
        Vec2f dir{0.f, 0.f};
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
        dir = normalized(dir);
        world.player += dir * (kPlayerSpeed * dt);
        world.player.x = std::clamp(world.player.x, kPlayerEdge, kWorldW - kPlayerEdge);
        world.player.y = std::clamp(world.player.y, kPlayerEdge, kWorldH - kPlayerEdge);

        // 카메라: 플레이어를 가운데에 두되 월드 밖이 보이지 않게 제한
        world.camera.center = cameraCenterFor(world.player);
        renderer.setCamera(world.camera);

        // 조준: 마우스가 가리키는 월드 위치를 향함
        const Vec2f aimWorld = renderer.screenToWorld(Vec2f{input.mousePosition()});
        const Vec2f toAim{aimWorld.x - world.player.x, aimWorld.y - world.player.y};
        if (vecLength(toAim) > 0.001f) {
            world.aimDir = normalized(toAim);
        }
        world.facing = std::atan2(world.aimDir.y, world.aimDir.x) * 180.f / kPi;

        // 사격: 누르고 있는 동안 0.15초마다, 눌렀다 뗀 짧은 입력도 놓치지 않음
        if (input.wasPressed(meno::MouseButton::Left)) {
            world.shotQueued = true;
        }
        world.fireCooldown = std::max(0.0, world.fireCooldown - fixedDeltaTime);
        const bool wantFire = input.isDown(meno::MouseButton::Left) || world.shotQueued;
        if (wantFire && world.fireCooldown <= 0.0) {
            world.bullets.push_back(
                Bullet{world.player, world.aimDir * kBulletSpeed, kBulletLife});
            shootSound.play();
            world.fireCooldown = kFireInterval;
            world.shotQueued = false;
        }

        // 총알 이동과 삭제
        for (Bullet& b : world.bullets) {
            b.pos += b.vel * dt;
            b.life -= dt;
        }
        std::erase_if(world.bullets, [](const Bullet& b) {
            return b.life <= 0.f || !inWorld(b.pos);
        });

        // 적 생성
        world.playTime += fixedDeltaTime;
        if (world.playTime >= world.nextSpawn && world.enemies.size() < kMaxEnemies) {
            world.enemies.push_back(Enemy{spawnPoint()});
            world.nextSpawn = world.playTime + spawnInterval(world.playTime);
        }

        // 적 이동
        for (Enemy& e : world.enemies) {
            e.pos += normalized(world.player - e.pos) * (kEnemySpeed * dt);
        }

        // 무적 시간 감소
        world.invincible = std::max(0.0, world.invincible - fixedDeltaTime);

        // 총알과 적 충돌: 둘 다 삭제하고 점수
        {
            std::vector<bool> bulletDead(world.bullets.size(), false);
            std::vector<bool> enemyDead(world.enemies.size(), false);
            const float reach = kBulletRadius + kEnemyRadius;
            for (std::size_t b = 0; b < world.bullets.size(); ++b) {
                for (std::size_t e = 0; e < world.enemies.size(); ++e) {
                    if (enemyDead[e]) {
                        continue;
                    }
                    if (distSq(world.bullets[b].pos, world.enemies[e].pos) <= reach * reach) {
                        bulletDead[b] = true;
                        enemyDead[e] = true;
                        world.score += kHitScore;
                        hitSound.play();
                        break;
                    }
                }
            }
            removeMarked(world.bullets, bulletDead);
            removeMarked(world.enemies, enemyDead);
        }

        // 적과 플레이어 충돌: 한 갱신에 하나만 처리하고 무적 시작
        if (world.invincible <= 0.0) {
            const float reach = kPlayerRadius + kEnemyRadius;
            for (std::size_t e = 0; e < world.enemies.size(); ++e) {
                if (distSq(world.player, world.enemies[e].pos) <= reach * reach) {
                    world.enemies.erase(world.enemies.begin() + static_cast<std::ptrdiff_t>(e));
                    --world.hp;
                    loseSound.play();
                    world.invincible = kInvincibleTime;
                    break;
                }
            }
        }

        if (world.hp <= 0) {
            (void)scenes.changeScene(result.id());
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(world.camera);
        renderer.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = background});
        for (float x = 0.f; x <= kWorldW; x += 120.f) {
            renderer.drawLine(Vec2f{x, 0.f}, Vec2f{x, kWorldH},
                              {.color = gridColor, .thickness = 1.f});
        }
        for (float y = 0.f; y <= kWorldH; y += 120.f) {
            renderer.drawLine(Vec2f{0.f, y}, Vec2f{kWorldW, y},
                              {.color = gridColor, .thickness = 1.f});
        }
        renderer.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                          {.fill = meno::colors::Transparent,
                           .outline = borderColor,
                           .outlineThickness = -2.f});

        for (const Bullet& b : world.bullets) {
            renderer.draw(*bulletTex, {.position = b.pos, .origin = {4.f, 4.f}});
        }
        for (const Enemy& e : world.enemies) {
            renderer.draw(*enemyTex, {.position = e.pos, .origin = {16.f, 16.f}});
        }

        meno::Color tint = meno::colors::White;
        if (world.invincible > 0.0 && std::fmod(world.invincible, 0.2) < 0.1) {
            tint = meno::colors::White.withAlpha(80);
        }
        renderer.draw(*playerTex, {.position = world.player,
                                   .origin = {16.f, 16.f},
                                   .rotation = world.facing,
                                   .tint = tint});

        if (font) {
            renderer.setCamera(hud);
            const std::string hudText = "HP " + std::to_string(world.hp) + "  SCORE " +
                                        std::to_string(world.score) + "  TIME " +
                                        formatTime(world.playTime);
            renderer.drawText(*font, hudText, Vec2f{16.f, 16.f}, makeText(16));
            if (world.paused) {
                drawCentered(renderer, *font, "PAUSED", kViewH / 2.f, 48);
            }
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
    result.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(hud);
        renderer.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = background});
        if (font) {
            drawCentered(renderer, *font, "GAME OVER", 170.f, 48);
            drawCentered(renderer, *font, "SCORE " + std::to_string(world.score), 250.f, 32);
            drawCentered(renderer, *font, "TIME " + formatTime(world.playTime), 300.f, 32);
            drawCentered(renderer, *font, "R: RESTART  ESC: QUIT", 460.f, 16);
        }
    });

    (void)scenes.changeScene(title.id());
    game.run();
}
```