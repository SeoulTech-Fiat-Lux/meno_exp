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
#include <utility>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;

constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerMargin = 16.f;
constexpr int kStartHp = 3;
constexpr float kInvulnTime = 1.f;

constexpr float kFireInterval = 0.15f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletLifetime = 1.4f;

constexpr float kEnemySpeed = 100.f;
constexpr float kEnemyRadius = 14.f;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kFirstSpawnDelay = 0.5f;
constexpr float kSpawnInterval = 1.f;
constexpr float kMinSpawnInterval = 0.3f;
constexpr float kSpawnMargin = 64.f;

constexpr float kTitleTime = 2.f;
constexpr float kMusicVolume = 0.4f;

enum class Screen { Title, Play, Result };

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f dir;
    float age = 0.f;
    bool dead = false;
};

struct Enemy {
    meno::Vec2f pos;
    bool dead = false;
};

struct Assets {
    std::shared_ptr<const meno::Texture> player;
    std::shared_ptr<const meno::Texture> enemy;
    std::shared_ptr<const meno::Texture> bullet;
    std::shared_ptr<const meno::Font> font;
    std::shared_ptr<const meno::SoundBuffer> shoot;
    std::shared_ptr<const meno::SoundBuffer> hit;
    std::shared_ptr<const meno::SoundBuffer> lose;
    std::optional<meno::Music> bgm;
};

bool loadAssets(meno::ResourceManager& resources, Assets& a) {
    bool ok = true;
    const auto require = [&ok](const auto& asset, const char* path) {
        if (!asset) {
            std::cerr << "Missing file: " << path << '\n';
            ok = false;
        }
    };

    a.player = resources.loadTexture("assets/textures/player.png");
    require(a.player, "assets/textures/player.png");
    a.enemy = resources.loadTexture("assets/textures/enemy.png");
    require(a.enemy, "assets/textures/enemy.png");
    a.bullet = resources.loadTexture("assets/textures/bullet.png");
    require(a.bullet, "assets/textures/bullet.png");
    a.font = resources.loadFont("assets/fonts/game.ttf");
    require(a.font, "assets/fonts/game.ttf");
    a.shoot = resources.loadSoundBuffer("assets/sfx/shoot.wav");
    require(a.shoot, "assets/sfx/shoot.wav");
    a.hit = resources.loadSoundBuffer("assets/sfx/hit.wav");
    require(a.hit, "assets/sfx/hit.wav");
    a.lose = resources.loadSoundBuffer("assets/sfx/lose.wav");
    require(a.lose, "assets/sfx/lose.wav");

    a.bgm = meno::Music::openFromFile("assets/music/bgm_shooter.ogg");
    if (!a.bgm) {
        std::cerr << "Missing file: assets/music/bgm_shooter.ogg\n";
        ok = false;
    }
    return ok;
}

float lengthOf(meno::Vec2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float distanceSq(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool inWorld(meno::Vec2f p) {
    return p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
}

class Arena {
public:
    Arena(meno::Game& game, Assets assets)
        : game_{game},
          playerTex_{std::move(assets.player)},
          enemyTex_{std::move(assets.enemy)},
          bulletTex_{std::move(assets.bullet)},
          font_{std::move(assets.font)},
          shootSfx_{assets.shoot},
          hitSfx_{assets.hit},
          loseSfx_{assets.lose},
          bgm_{std::move(assets.bgm)},
          rng_{std::random_device{}()} {}

    void install() {
        meno::Scene& scene = game_.scenes().createScene();
        scene.setUpdateHandler([this](double dt) { update(static_cast<float>(dt)); });
        scene.setDrawHandler([this](meno::Renderer& renderer) { draw(renderer); });
        (void)game_.scenes().changeScene(scene.id());
    }

private:
    meno::Game& game_;
    std::shared_ptr<const meno::Texture> playerTex_;
    std::shared_ptr<const meno::Texture> enemyTex_;
    std::shared_ptr<const meno::Texture> bulletTex_;
    std::shared_ptr<const meno::Font> font_;
    meno::Sound shootSfx_;
    meno::Sound hitSfx_;
    meno::Sound loseSfx_;
    std::optional<meno::Music> bgm_;
    std::mt19937 rng_;

    Screen screen_ = Screen::Title;
    float titleTimer_ = 0.f;
    meno::Camera2D world_{.center = {kWorldW / 2.f, kWorldH / 2.f}, .size = {kViewW, kViewH}};
    meno::Camera2D hud_{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};

    meno::Vec2f player_{kWorldW / 2.f, kWorldH / 2.f};
    meno::Vec2f aimDir_{1.f, 0.f};
    float playerAngle_ = 0.f;
    float fireTimer_ = kFireInterval;
    int hp_ = kStartHp;
    int score_ = 0;
    double playTime_ = 0.0;
    float invulnTimer_ = 0.f;
    float spawnTimer_ = kFirstSpawnDelay;
    bool paused_ = false;
    std::vector<Bullet> bullets_;
    std::vector<Enemy> enemies_;

    void update(float dt) {
        const meno::Input& input = game_.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }
        switch (screen_) {
        case Screen::Title:
            titleTimer_ += dt;
            if (input.wasPressed(meno::Key::Enter) || titleTimer_ >= kTitleTime) {
                startPlay();
            }
            return;
        case Screen::Result:
            if (input.wasPressed(meno::Key::R)) {
                startPlay();
            }
            return;
        case Screen::Play:
            updatePlay(input, dt);
            return;
        }
    }

    void startPlay() {
        player_ = meno::Vec2f{kWorldW / 2.f, kWorldH / 2.f};
        aimDir_ = meno::Vec2f{1.f, 0.f};
        playerAngle_ = 0.f;
        fireTimer_ = kFireInterval;
        hp_ = kStartHp;
        score_ = 0;
        playTime_ = 0.0;
        invulnTimer_ = 0.f;
        spawnTimer_ = kFirstSpawnDelay;
        paused_ = false;
        bullets_.clear();
        enemies_.clear();
        screen_ = Screen::Play;
        if (bgm_) {
            bgm_->stop();
            bgm_->setLooping(true);
            bgm_->setVolume(kMusicVolume);
            bgm_->play();
        }
    }

    void updatePlay(const meno::Input& input, float dt) {
        if (input.wasPressed(meno::Key::P)) {
            paused_ = !paused_;
            if (bgm_) {
                if (paused_) {
                    bgm_->pause();
                } else {
                    bgm_->play();
                }
            }
        }
        if (paused_) {
            return;
        }

        playTime_ += static_cast<double>(dt);
        movePlayer(input, dt);
        updateCamera();
        aim(input);
        updateFire(input, dt);
        updateBullets(dt);
        updateSpawn(dt);
        updateEnemies(dt);
        resolveBulletHits();
        resolvePlayerHits(dt);

        if (hp_ <= 0) {
            screen_ = Screen::Result;
            if (bgm_) {
                bgm_->stop();
            }
        }
    }

    void movePlayer(const meno::Input& input, float dt) {
        meno::Vec2f dir{0.f, 0.f};
        if (input.isDown(meno::Key::Left) || input.isDown(meno::Key::A)) {
            dir.x -= 1.f;
        }
        if (input.isDown(meno::Key::Right) || input.isDown(meno::Key::D)) {
            dir.x += 1.f;
        }
        if (input.isDown(meno::Key::Up) || input.isDown(meno::Key::W)) {
            dir.y -= 1.f;
        }
        if (input.isDown(meno::Key::Down) || input.isDown(meno::Key::S)) {
            dir.y += 1.f;
        }
        const float len = lengthOf(dir);
        if (len > 0.f) {
            player_ += dir * (kPlayerSpeed * dt / len);
        }
        player_.x = std::clamp(player_.x, kPlayerMargin, kWorldW - kPlayerMargin);
        player_.y = std::clamp(player_.y, kPlayerMargin, kWorldH - kPlayerMargin);
    }

    void updateCamera() {
        world_.center = meno::Vec2f{std::clamp(player_.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                                    std::clamp(player_.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
        game_.renderer().setCamera(world_);
    }

    void aim(const meno::Input& input) {
        const meno::Vec2f mouse{input.mousePosition()};
        const meno::Vec2f cursor = game_.renderer().screenToWorld(mouse);
        const meno::Vec2f toCursor = cursor - player_;
        const float len = lengthOf(toCursor);
        aimDir_ = len > 1e-3f ? toCursor * (1.f / len) : meno::Vec2f{1.f, 0.f};
        playerAngle_ = std::atan2(aimDir_.y, aimDir_.x) * 180.f / kPi;
    }

    void updateFire(const meno::Input& input, float dt) {
        fireTimer_ = std::min(fireTimer_ + dt, kFireInterval);
        if (input.isDown(meno::MouseButton::Left) && fireTimer_ >= kFireInterval - 1e-4f) {
            bullets_.push_back(Bullet{player_, aimDir_});
            fireTimer_ -= kFireInterval;
            shootSfx_.play();
        }
    }

    void updateBullets(float dt) {
        for (Bullet& b : bullets_) {
            b.pos += b.dir * (kBulletSpeed * dt);
            b.age += dt;
            if (b.age >= kBulletLifetime || !inWorld(b.pos)) {
                b.dead = true;
            }
        }
        std::erase_if(bullets_, [](const Bullet& b) { return b.dead; });
    }

    float spawnInterval() const {
        const double steps = std::floor(playTime_ / 10.0);
        return std::max(kMinSpawnInterval, kSpawnInterval - 0.1f * static_cast<float>(steps));
    }

    void updateSpawn(float dt) {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.f) {
            spawnTimer_ += spawnInterval();
            if (enemies_.size() < kMaxEnemies) {
                spawnEnemy();
            }
        }
    }

    void spawnEnemy() {
        const float left = world_.center.x - kViewW / 2.f - kSpawnMargin;
        const float top = world_.center.y - kViewH / 2.f - kSpawnMargin;
        const float width = kViewW + 2.f * kSpawnMargin;
        const float height = kViewH + 2.f * kSpawnMargin;
        std::uniform_real_distribution<float> unit{0.f, 1.f};
        std::uniform_int_distribution<int> side{0, 3};

        for (int attempt = 0; attempt < 32; ++attempt) {
            const float t = unit(rng_);
            meno::Vec2f p{0.f, 0.f};
            switch (side(rng_)) {
            case 0:
                p = meno::Vec2f{left + t * width, top};
                break;
            case 1:
                p = meno::Vec2f{left + t * width, top + height};
                break;
            case 2:
                p = meno::Vec2f{left, top + t * height};
                break;
            default:
                p = meno::Vec2f{left + width, top + t * height};
                break;
            }
            if (inWorld(p)) {
                enemies_.push_back(Enemy{p});
                return;
            }
        }
    }

    void updateEnemies(float dt) {
        for (Enemy& e : enemies_) {
            const meno::Vec2f toPlayer = player_ - e.pos;
            const float len = lengthOf(toPlayer);
            if (len > 1e-3f) {
                e.pos += toPlayer * (kEnemySpeed * dt / len);
            }
        }
    }

    void resolveBulletHits() {
        const float reach = kBulletRadius + kEnemyRadius;
        for (Bullet& b : bullets_) {
            for (Enemy& e : enemies_) {
                if (!e.dead && distanceSq(b.pos, e.pos) <= reach * reach) {
                    e.dead = true;
                    b.dead = true;
                    score_ += 10;
                    hitSfx_.play();
                    break;
                }
            }
        }
        std::erase_if(bullets_, [](const Bullet& b) { return b.dead; });
        std::erase_if(enemies_, [](const Enemy& e) { return e.dead; });
    }

    void resolvePlayerHits(float dt) {
        invulnTimer_ = std::max(0.f, invulnTimer_ - dt);
        if (invulnTimer_ > 0.f) {
            return;
        }
        const float reach = kPlayerRadius + kEnemyRadius;
        for (Enemy& e : enemies_) {
            if (distanceSq(player_, e.pos) <= reach * reach) {
                e.dead = true;
                hp_ -= 1;
                invulnTimer_ = kInvulnTime;
                loseSfx_.play();
                break;
            }
        }
        std::erase_if(enemies_, [](const Enemy& e) { return e.dead; });
    }

    void draw(meno::Renderer& renderer) {
        if (screen_ == Screen::Play) {
            renderer.setCamera(world_);
            drawWorld(renderer);
        }
        renderer.setCamera(hud_);
        switch (screen_) {
        case Screen::Title:
            drawTitle(renderer);
            break;
        case Screen::Play:
            drawHud(renderer);
            break;
        case Screen::Result:
            drawResult(renderer);
            break;
        }
    }

    void drawWorld(meno::Renderer& r) {
        const meno::Color background = meno::Color::fromRgb(0x14181F);
        const meno::Color grid = meno::Color::fromRgb(0x222A35);
        const meno::Color border = meno::Color::fromRgb(0x5C6B80);

        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = background});
        for (int i = 0; i <= 16; ++i) {
            const float x = static_cast<float>(i) * 120.f;
            r.drawLine({x, 0.f}, {x, kWorldH}, {.color = grid, .thickness = 1.f});
        }
        for (int j = 0; j <= 9; ++j) {
            const float y = static_cast<float>(j) * 120.f;
            r.drawLine({0.f, y}, {kWorldW, y}, {.color = grid, .thickness = 1.f});
        }
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                   {.fill = meno::colors::Transparent, .outline = border, .outlineThickness = 2.f});

        for (const Enemy& e : enemies_) {
            r.draw(*enemyTex_, {.position = e.pos, .origin = {16.f, 16.f}});
        }
        for (const Bullet& b : bullets_) {
            r.draw(*bulletTex_, {.position = b.pos, .origin = {kBulletRadius, kBulletRadius}});
        }

        meno::Color tint = meno::colors::White;
        if (invulnTimer_ > 0.f && static_cast<int>(invulnTimer_ * 10.f) % 2 == 0) {
            tint = meno::colors::White.withAlpha(90);
        }
        r.draw(*playerTex_, {.position = player_,
                             .origin = {16.f, 16.f},
                             .rotation = playerAngle_,
                             .tint = tint});
    }

    void fillView(meno::Renderer& r) {
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x14181F)});
    }

    void drawTitle(meno::Renderer& r) {
        fillView(r);
        text(r, "ARENA", {kViewW / 2.f, 200.f}, 48, true);
        text(r, "PRESS ENTER", {kViewW / 2.f, 300.f}, 32, true);
    }

    void drawHud(meno::Renderer& r) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "HP %d  SCORE %d  TIME %.1f", hp_, score_, playTime_);
        text(r, buf, {16.f, 16.f}, 32, false);
        if (paused_) {
            text(r, "PAUSED", {kViewW / 2.f, kViewH / 2.f}, 48, true);
        }
    }

    void drawResult(meno::Renderer& r) {
        fillView(r);
        char timeBuf[32];
        std::snprintf(timeBuf, sizeof timeBuf, "TIME %.1f", playTime_);
        text(r, "GAME OVER", {kViewW / 2.f, 180.f}, 48, true);
        text(r, "SCORE " + std::to_string(score_), {kViewW / 2.f, 260.f}, 32, true);
        text(r, timeBuf, {kViewW / 2.f, 320.f}, 32, true);
        text(r, "R: RESTART  ESC: QUIT", {kViewW / 2.f, 460.f}, 16, true);
    }

    void text(meno::Renderer& r, const std::string& s, meno::Vec2f pos, int size, bool centered) {
        meno::TextParams params{};
        params.characterSize = size;
        params.color = meno::colors::White;
        if (centered) {
            params.origin = r.measureTextBounds(*font_, s, params).center();
        }
        r.drawText(*font_, s, pos, params);
    }
};

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "ARENA",
                     .clearColor = meno::colors::Black}};

    Assets assets;
    if (!loadAssets(game.resources(), assets)) {
        return 1;
    }

    Arena arena{game, std::move(assets)};
    arena.install();
    game.run();
}
```