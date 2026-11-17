#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Vec = meno::Vec2f;
using Color = meno::Color;

constexpr float WorldW = 2880.f;
constexpr float WorldH = 1620.f;
constexpr float Pi = 3.14159265358979323846f;

Color rgb(unsigned value) {
    return Color::fromRgb(value);
}

float lengthSquared(Vec v) {
    return v.x * v.x + v.y * v.y;
}

float length(Vec v) {
    return std::sqrt(lengthSquared(v));
}

Vec normalized(Vec v) {
    const float n = length(v);
    return n > 0.f ? v * (1.f / n) : Vec{0.f, 0.f};
}

void clampPosition(Vec& p, float radius) {
    p.x = std::clamp(p.x, radius, WorldW - radius);
    p.y = std::clamp(p.y, radius, WorldH - radius);
}

bool circlesOverlap(Vec a, float ar, Vec b, float br) {
    const float r = ar + br;
    return lengthSquared(a - b) <= r * r;
}

// Returns the first intersection along a segment, including initial overlap.
float segmentHit(Vec from, Vec to, Vec center, float radius) {
    const Vec offset = from - center;
    const float c = lengthSquared(offset) - radius * radius;
    if (c <= 0.f) {
        return 0.f;
    }

    const Vec delta = to - from;
    const float a = lengthSquared(delta);
    if (a <= 0.f) {
        return std::numeric_limits<float>::infinity();
    }

    const float b = offset.x * delta.x + offset.y * delta.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.f) {
        return std::numeric_limits<float>::infinity();
    }

    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.f && t <= 1.f
               ? t
               : std::numeric_limits<float>::infinity();
}

std::string formatTime(double seconds) {
    const int total = static_cast<int>(std::clamp(seconds, 0.0, 300.0));
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << total / 60
        << ':' << std::setw(2) << total % 60;
    return out.str();
}

struct EnemyStats {
    float radius;
    float health;
    float speed;
    float contactDamage;
    float frameDuration;
    int cell;
    int experience;
};

constexpr std::array<EnemyStats, 3> EnemyTypes{{
    {10.f, 10.f, 80.f, 10.f, 0.3f, 24, 1},
    {10.f, 6.f, 140.f, 6.f, 0.15f, 24, 1},
    {20.f, 60.f, 50.f, 20.f, 0.4f, 48, 5}
}};

struct Enemy {
    Vec position{};
    Vec knockVelocity{};
    float health = 10.f;
    float knockRemaining = 0.f;
    float hitRemaining = 0.f;
    float deathRemaining = 0.f;
    int type = 0;
};

struct Bullet {
    Vec position{};
    Vec velocity{};
    float remaining = 1.2f;
};

struct Gem {
    Vec position{};
    float remaining = 30.f;
    int experience = 1;
};

struct SoundPool {
    std::vector<meno::Sound> voices;
    std::size_t next = 0;

    void initialize(const std::shared_ptr<const meno::SoundBuffer>& buffer,
                    std::size_t count) {
        voices.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            voices.emplace_back(buffer);
        }
    }

    void play() {
        if (!voices.empty()) {
            voices[next].play();
            next = (next + 1) % voices.size();
        }
    }

    void stop() {
        for (auto& voice : voices) {
            voice.stop();
        }
        next = 0;
    }
};

class LastStand {
public:
    LastStand()
        : game_({.application = {.fixedTimeStep = 1.0 / 60.0,
                                 .framerateLimit = 60},
                 .windowSize = {960, 540},
                 .title = "LAST STAND",
                 .clearColor = meno::colors::Black}),
          random_(std::random_device{}()) {}

    bool initialize() {
        bool valid = true;
        const auto require = [&](bool success, const char* path) {
            if (!success) {
                std::cerr << "FAILED TO LOAD: " << path << '\n';
                valid = false;
            }
        };

        font_ = game_.resources().loadFont("assets/fonts/game.ttf");
        require(static_cast<bool>(font_), "assets/fonts/game.ttf");

        constexpr std::array<const char*, 6> paths{{
            "assets/textures/sv_player.png",
            "assets/textures/sv_walker.png",
            "assets/textures/sv_runner.png",
            "assets/textures/sv_brute.png",
            "assets/textures/sv_bullet.png",
            "assets/textures/sv_gem.png"
        }};
        for (std::size_t i = 0; i < paths.size(); ++i) {
            textures_[i] = game_.resources().loadTexture(paths[i]);
            require(static_cast<bool>(textures_[i]), paths[i]);
        }

        constexpr std::array<const char*, 4> soundPaths{{
            "assets/sfx/shoot.wav",
            "assets/sfx/hit.wav",
            "assets/sfx/pickup.wav",
            "assets/sfx/levelup.wav"
        }};
        for (std::size_t i = 0; i < soundPaths.size(); ++i) {
            const auto buffer =
                game_.resources().loadSoundBuffer(soundPaths[i]);
            require(static_cast<bool>(buffer), soundPaths[i]);
            if (buffer) {
                sounds_[i].initialize(buffer, i == 3 ? 2 : 12);
            }
        }

        music_ = meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
        require(music_.has_value(), "assets/music/bgm_survivor.ogg");

        if (!valid) {
            return false;
        }

        music_->setVolume(0.4f);
        music_->setLooping(true);

        enemies_.reserve(400);
        bullets_.reserve(64);
        gems_.reserve(512);

        meno::Scene& scene = game_.scenes().createScene();
        scene.setUpdateHandler([this](double dt) { update(dt); });
        scene.setDrawHandler([this](meno::Renderer& r) { draw(r); });
        game_.renderer().setCamera(hud_);
        (void)game_.scenes().changeScene(scene.id());
        return true;
    }

    void run() {
        game_.run();
    }

private:
    enum class Screen { Title, Play, Result };

    meno::Game game_;
    std::shared_ptr<const meno::Font> font_;
    std::array<std::shared_ptr<const meno::Texture>, 6> textures_{};
    std::array<SoundPool, 4> sounds_;
    std::optional<meno::Music> music_;
    std::mt19937 random_;

    Screen screen_ = Screen::Title;
    double titleTime_ = 0.0;
    double elapsed_ = 0.0;
    double spawnRemaining_ = 0.5;
    double fireRemaining_ = 0.8;

    Vec player_{WorldW * 0.5f, WorldH * 0.5f};
    meno::Camera2D camera_{.center = player_, .size = {960.f, 540.f}};
    const meno::Camera2D hud_{.center = {480.f, 270.f},
                              .size = {960.f, 540.f}};

    float health_ = 100.f;
    float maxHealth_ = 100.f;
    float moveSpeed_ = 200.f;
    float damage_ = 10.f;
    float magnet_ = 60.f;
    double fireInterval_ = 0.8;
    double walkTime_ = 0.0;

    int projectileCount_ = 1;
    int level_ = 1;
    int experience_ = 0;
    int kills_ = 0;
    int playerFrame_ = 0;

    bool facingLeft_ = false;
    bool takingDamage_ = false;
    bool paused_ = false;
    bool choosing_ = false;
    bool survived_ = false;

    std::array<int, 3> choices_{};
    std::vector<Enemy> enemies_;
    std::vector<Bullet> bullets_;
    std::vector<Gem> gems_;

    static constexpr std::array<const char*, 6> AbilityNames{{
        "FIRE RATE +15%",
        "DAMAGE +5",
        "PROJECTILE +1",
        "SPEED +10%",
        "MAX HP +20",
        "MAGNET +40"
    }};

    static meno::Rectf cardRect(int index) {
        return {{80.f + 280.f * static_cast<float>(index), 200.f},
                {240.f, 140.f}};
    }

    void updateCamera() {
        camera_.center = {
            std::clamp(player_.x, 480.f, WorldW - 480.f),
            std::clamp(player_.y, 270.f, WorldH - 270.f)
        };
        game_.renderer().setCamera(camera_);
    }

    void restart() {
        for (auto& sound : sounds_) {
            sound.stop();
        }

        screen_ = Screen::Play;
        elapsed_ = 0.0;
        spawnRemaining_ = 0.5;
        fireRemaining_ = 0.8;
        player_ = {WorldW * 0.5f, WorldH * 0.5f};
        health_ = maxHealth_ = 100.f;
        moveSpeed_ = 200.f;
        damage_ = 10.f;
        magnet_ = 60.f;
        fireInterval_ = 0.8;
        projectileCount_ = 1;
        level_ = 1;
        experience_ = kills_ = 0;
        playerFrame_ = 0;
        walkTime_ = 0.0;
        facingLeft_ = takingDamage_ = paused_ = choosing_ = survived_ = false;
        choices_.fill(0);
        enemies_.clear();
        bullets_.clear();
        gems_.clear();
        updateCamera();

        music_->stop();
        music_->play();
    }

    void finish(bool survived) {
        survived_ = survived;
        screen_ = Screen::Result;
        paused_ = choosing_ = false;
        music_->stop();
        for (auto& sound : sounds_) {
            sound.stop();
        }
        game_.renderer().setCamera(hud_);
    }

    void beginLevelIfNeeded() {
        if (experience_ < 5 * level_) {
            choosing_ = false;
            return;
        }

        experience_ -= 5 * level_;
        ++level_;
        choosing_ = true;
        sounds_[3].play();

        std::array<int, 6> abilities{{0, 1, 2, 3, 4, 5}};
        std::shuffle(abilities.begin(), abilities.end(), random_);
        std::copy_n(abilities.begin(), 3, choices_.begin());
    }

    void chooseAbility(int index) {
        switch (choices_[static_cast<std::size_t>(index)]) {
        case 0:
            fireInterval_ *= 0.85;
            fireRemaining_ = std::min(fireRemaining_, fireInterval_);
            break;
        case 1:
            damage_ += 5.f;
            break;
        case 2:
            ++projectileCount_;
            break;
        case 3:
            moveSpeed_ *= 1.1f;
            break;
        case 4:
            maxHealth_ += 20.f;
            health_ = std::min(maxHealth_, health_ + 20.f);
            break;
        case 5:
            magnet_ += 40.f;
            break;
        }

        choosing_ = false;
        beginLevelIfNeeded();
    }

    void handleCards(const meno::Input& input) {
        int selected = -1;
        if (input.wasPressed(meno::Key::Num1)) {
            selected = 0;
        } else if (input.wasPressed(meno::Key::Num2)) {
            selected = 1;
        } else if (input.wasPressed(meno::Key::Num3)) {
            selected = 2;
        } else if (input.wasPressed(meno::MouseButton::Left)) {
            const Vec mouse{input.mousePosition()};
            const Vec point = meno::screenToWorld(
                hud_, game_.renderer().framebufferSize(), mouse);
            for (int i = 0; i < 3; ++i) {
                if (cardRect(i).contains(point)) {
                    selected = i;
                    break;
                }
            }
        }

        if (selected >= 0) {
            chooseAbility(selected);
        }
    }

    Vec spawnPosition() {
        const float left = camera_.center.x - 480.f - 64.f;
        const float right = camera_.center.x + 480.f + 64.f;
        const float top = camera_.center.y - 270.f - 64.f;
        const float bottom = camera_.center.y + 270.f + 64.f;

        struct Segment {
            Vec from;
            Vec to;
            float distance;
        };
        std::array<Segment, 4> segments{};
        int count = 0;
        float total = 0.f;

        const auto add = [&](Vec a, Vec b) {
            const float distance = length(b - a);
            if (distance > 0.f) {
                segments[static_cast<std::size_t>(count++)] =
                    {a, b, distance};
                total += distance;
            }
        };

        const float x0 = std::max(0.f, left);
        const float x1 = std::min(WorldW, right);
        const float y0 = std::max(0.f, top);
        const float y1 = std::min(WorldH, bottom);

        if (top >= 0.f && top <= WorldH) {
            add({x0, top}, {x1, top});
        }
        if (bottom >= 0.f && bottom <= WorldH) {
            add({x0, bottom}, {x1, bottom});
        }
        if (left >= 0.f && left <= WorldW) {
            add({left, y0}, {left, y1});
        }
        if (right >= 0.f && right <= WorldW) {
            add({right, y0}, {right, y1});
        }

        float distance = std::uniform_real_distribution<float>(0.f, total)(random_);
        for (int i = 0; i < count; ++i) {
            const Segment& segment = segments[static_cast<std::size_t>(i)];
            if (distance <= segment.distance || i == count - 1) {
                return segment.from +
                       (segment.to - segment.from) *
                           std::clamp(distance / segment.distance, 0.f, 1.f);
            }
            distance -= segment.distance;
        }
        return {0.f, 0.f};
    }

    void spawnEnemy() {
        if (enemies_.size() >= 400) {
            return;
        }

        const float roll =
            std::uniform_real_distribution<float>(0.f, 1.f)(random_);
        int type = 0;
        if (elapsed_ >= 120.0) {
            type = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
        } else if (elapsed_ >= 60.0) {
            type = roll < 0.7f ? 0 : 1;
        }

        Enemy enemy;
        enemy.position = spawnPosition();
        enemy.type = type;
        enemy.health = EnemyTypes[static_cast<std::size_t>(type)].health;
        enemies_.push_back(enemy);
    }

    void fire() {
        const Enemy* target = nullptr;
        float nearest = 400.f * 400.f;
        for (const Enemy& enemy : enemies_) {
            if (enemy.health <= 0.f) {
                continue;
            }
            const float distance = lengthSquared(enemy.position - player_);
            if (distance <= nearest) {
                nearest = distance;
                target = &enemy;
            }
        }

        if (!target) {
            return;
        }

        const Vec delta = target->position - player_;
        const float baseAngle = lengthSquared(delta) > 0.f
                                    ? std::atan2(delta.y, delta.x)
                                    : 0.f;
        for (int i = 0; i < projectileCount_; ++i) {
            const float offset =
                (static_cast<float>(i) -
                 static_cast<float>(projectileCount_ - 1) * 0.5f) *
                10.f * Pi / 180.f;
            const float angle = baseAngle + offset;
            bullets_.push_back(
                {player_, {500.f * std::cos(angle), 500.f * std::sin(angle)}, 1.2f});
        }
        sounds_[0].play();
    }

    void updateEnemies(float dt) {
        for (Enemy& enemy : enemies_) {
            if (enemy.health <= 0.f) {
                enemy.deathRemaining -= dt;
                continue;
            }

            const EnemyStats& stats =
                EnemyTypes[static_cast<std::size_t>(enemy.type)];
            enemy.hitRemaining = std::max(0.f, enemy.hitRemaining - dt);

            float movementTime = dt;
            if (enemy.knockRemaining > 0.f) {
                const float knockTime = std::min(dt, enemy.knockRemaining);
                enemy.position += enemy.knockVelocity * knockTime;
                enemy.knockRemaining =
                    std::max(0.f, enemy.knockRemaining - knockTime);
                movementTime -= knockTime;
            }

            const Vec difference = player_ - enemy.position;
            const float distance = length(difference);
            if (distance > 0.f && movementTime > 0.f) {
                enemy.position += difference *
                    (std::min(stats.speed * movementTime, distance) / distance);
            }
            clampPosition(enemy.position, stats.radius);
        }

        std::erase_if(enemies_, [](const Enemy& enemy) {
            return enemy.health <= 0.f && enemy.deathRemaining <= 0.f;
        });
    }

    void updateBullets(float dt) {
        for (Bullet& bullet : bullets_) {
            const float travelTime = std::min(dt, bullet.remaining);
            const Vec end = bullet.position + bullet.velocity * travelTime;

            Enemy* struck = nullptr;
            float earliest = std::numeric_limits<float>::infinity();
            for (Enemy& enemy : enemies_) {
                if (enemy.health <= 0.f) {
                    continue;
                }
                const float radius =
                    EnemyTypes[static_cast<std::size_t>(enemy.type)].radius + 4.f;
                const float hit =
                    segmentHit(bullet.position, end, enemy.position, radius);
                if (hit < earliest) {
                    earliest = hit;
                    struck = &enemy;
                }
            }

            bullet.position = end;
            bullet.remaining -= dt;
            if (!struck) {
                continue;
            }

            bullet.remaining = 0.f;
            struck->health -= damage_;
            struck->hitRemaining = 0.1f;

            if (struck->health <= 0.f) {
                struck->deathRemaining = 0.1f;
                struck->knockRemaining = 0.f;
                gems_.push_back(
                    {struck->position, 30.f,
                     EnemyTypes[static_cast<std::size_t>(struck->type)].experience});
                ++kills_;
                sounds_[1].play();
            } else {
                struck->knockRemaining = 0.1f;
                struck->knockVelocity = normalized(bullet.velocity) * 120.f;
            }
        }

        std::erase_if(bullets_, [](const Bullet& bullet) {
            return bullet.remaining <= 0.f;
        });
    }

    void updateGems(float dt) {
        for (Gem& gem : gems_) {
            gem.remaining -= dt;
            if (gem.remaining <= 0.f) {
                continue;
            }

            Vec delta = player_ - gem.position;
            float distance = length(delta);
            if (distance <= magnet_ && distance > 0.f) {
                gem.position += delta *
                    (std::min(300.f * dt, distance) / distance);
            }

            if (circlesOverlap(player_, 14.f, gem.position, 6.f)) {
                experience_ += gem.experience;
                gem.remaining = 0.f;
                sounds_[2].play();
            }
        }

        std::erase_if(gems_, [](const Gem& gem) {
            return gem.remaining <= 0.f;
        });
    }

    void update(double fixedDt) {
        const meno::Input& input = game_.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }

        if (screen_ == Screen::Title) {
            titleTime_ += fixedDt;
            if (input.wasPressed(meno::Key::Enter) || titleTime_ >= 2.0) {
                restart();
            }
            return;
        }

        if (screen_ == Screen::Result) {
            if (input.wasPressed(meno::Key::R)) {
                restart();
            }
            return;
        }

        if (choosing_) {
            handleCards(input);
            return;
        }

        if (input.wasPressed(meno::Key::P)) {
            paused_ = !paused_;
        }
        if (paused_) {
            return;
        }

        const float dt = static_cast<float>(fixedDt);
        elapsed_ = std::min(300.0, elapsed_ + fixedDt);

        Vec movement{0.f, 0.f};
        if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left)) {
            movement.x -= 1.f;
        }
        if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right)) {
            movement.x += 1.f;
        }
        if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up)) {
            movement.y -= 1.f;
        }
        if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down)) {
            movement.y += 1.f;
        }

        if (movement.x != 0.f) {
            facingLeft_ = movement.x < 0.f;
        }

        if (lengthSquared(movement) > 0.f) {
            player_ += normalized(movement) * (moveSpeed_ * dt);
            walkTime_ += fixedDt;
            playerFrame_ = static_cast<int>(walkTime_ / 0.15) % 2;
        } else {
            walkTime_ = 0.0;
            playerFrame_ = 0;
        }

        clampPosition(player_, 14.f);
        updateCamera();

        spawnRemaining_ -= fixedDt;
        while (spawnRemaining_ <= 0.0) {
            spawnEnemy();
            const double interval =
                std::max(0.15, 0.6 - 0.05 * std::floor(elapsed_ / 15.0));
            spawnRemaining_ += interval;
        }

        updateEnemies(dt);

        fireRemaining_ -= fixedDt;
        if (fireRemaining_ <= 0.0) {
            fire();
            // One simultaneous volley per fixed update at extremely high levels.
            fireRemaining_ = std::max(
                fireRemaining_ + fireInterval_, fixedDt);
        }

        updateBullets(dt);

        takingDamage_ = false;
        float contactDamage = 0.f;
        for (const Enemy& enemy : enemies_) {
            if (enemy.health <= 0.f) {
                continue;
            }
            const EnemyStats& stats =
                EnemyTypes[static_cast<std::size_t>(enemy.type)];
            if (circlesOverlap(player_, 14.f, enemy.position, stats.radius)) {
                takingDamage_ = true;
                contactDamage += stats.contactDamage * dt;
            }
        }
        health_ = std::max(0.f, health_ - contactDamage);

        updateGems(dt);

        if (health_ <= 0.f) {
            finish(false);
        } else if (elapsed_ >= 300.0) {
            finish(true);
        } else {
            beginLevelIfNeeded();
        }
    }

    void text(meno::Renderer& r, const std::string& value, Vec position,
              unsigned size = 16,
              Color color = meno::colors::White) const {
        r.drawText(*font_, value, position,
                   {.characterSize = size, .color = color});
    }

    void centered(meno::Renderer& r, const std::string& value, Vec position,
                  unsigned size = 32) const {
        meno::TextParams params{.characterSize = size,
                                .color = meno::colors::White};
        params.origin = r.measureTextBounds(*font_, value, params).center();
        r.drawText(*font_, value, position, params);
    }

    void rightAligned(meno::Renderer& r, const std::string& value,
                      Vec position) const {
        meno::TextParams params{.characterSize = 16,
                                .color = meno::colors::White};
        const auto bounds = r.measureTextBounds(*font_, value, params);
        params.origin = {bounds.right(), bounds.top()};
        r.drawText(*font_, value, position, params);
    }

    void drawCharacter(meno::Renderer& r, std::size_t texture,
                       Vec position, int cell, int frame, bool flip,
                       Color tint = meno::colors::White) const {
        r.draw(*textures_[texture],
               {.position = position,
                .origin = {static_cast<float>(cell) * 0.5f,
                           static_cast<float>(cell) * 0.5f},
                .tint = tint,
                .source = meno::Recti{frame * cell, 0, cell, cell},
                .flipX = flip});
    }

    void drawWorld(meno::Renderer& r) const {
        r.setCamera(camera_);
        r.drawRect({{0.f, 0.f}, {WorldW, WorldH}},
                   {.fill = rgb(0x1A1420)});

        const float left = camera_.center.x - 480.f;
        const float right = camera_.center.x + 480.f;
        const float top = camera_.center.y - 270.f;
        const float bottom = camera_.center.y + 270.f;

        for (float x = std::ceil(left / 120.f) * 120.f;
             x <= right; x += 120.f) {
            r.drawLine({x, top}, {x, bottom},
                       {.color = rgb(0x2A2233), .thickness = 1.f});
        }
        for (float y = std::ceil(top / 120.f) * 120.f;
             y <= bottom; y += 120.f) {
            r.drawLine({left, y}, {right, y},
                       {.color = rgb(0x2A2233), .thickness = 1.f});
        }

        r.drawRect({{0.f, 0.f}, {WorldW, WorldH}},
                   {.fill = meno::colors::Transparent,
                    .outline = rgb(0x6B5B7B),
                    .outlineThickness = -2.f});

        const auto visible = [&](Vec p, float radius) {
            return p.x + radius >= left && p.x - radius <= right &&
                   p.y + radius >= top && p.y - radius <= bottom;
        };

        for (const Gem& gem : gems_) {
            if (visible(gem.position, 6.f)) {
                r.draw(*textures_[5],
                       {.position = gem.position,
                        .origin = {6.f, 6.f},
                        .tint = gem.experience == 5
                                    ? rgb(0xFFD54A)
                                    : rgb(0x4FC3F7)});
            }
        }

        for (const Enemy& enemy : enemies_) {
            const EnemyStats& stats =
                EnemyTypes[static_cast<std::size_t>(enemy.type)];
            if (!visible(enemy.position, static_cast<float>(stats.cell))) {
                continue;
            }

            const int frame =
                enemy.health <= 0.f || enemy.hitRemaining > 0.f
                    ? 2
                    : static_cast<int>(elapsed_ / stats.frameDuration) % 2;

            drawCharacter(r, static_cast<std::size_t>(enemy.type + 1),
                          enemy.position, stats.cell, frame,
                          player_.x < enemy.position.x);
        }

        for (const Bullet& bullet : bullets_) {
            if (visible(bullet.position, 4.f)) {
                r.draw(*textures_[4],
                       {.position = bullet.position,
                        .origin = {4.f, 4.f}});
            }
        }

        drawCharacter(r, 0, player_, 32, playerFrame_, facingLeft_,
                      takingDamage_ ? rgb(0xFF6060) : meno::colors::White);
    }

    void drawHud(meno::Renderer& r) const {
        r.setCamera(hud_);

        r.drawRect({{0.f, 0.f}, {960.f, 6.f}},
                   {.fill = rgb(0x2A2233)});
        const float experienceRatio = std::clamp(
            static_cast<float>(experience_) /
                (5.f * static_cast<float>(level_)),
            0.f, 1.f);
        r.drawRect({{0.f, 0.f}, {960.f * experienceRatio, 6.f}},
                   {.fill = rgb(0x4FC3F7)});

        r.drawRect({{16.f, 18.f}, {200.f, 12.f}},
                   {.fill = rgb(0x3A1F24)});
        r.drawRect({{16.f, 18.f},
                    {200.f * std::clamp(health_ / maxHealth_, 0.f, 1.f), 12.f}},
                   {.fill = rgb(0xE5533D)});
        text(r, "HP " + std::to_string(static_cast<int>(std::ceil(health_))),
             {16.f, 36.f});

        rightAligned(r, "LV " + std::to_string(level_), {944.f, 14.f});
        rightAligned(r, "TIME " + formatTime(elapsed_), {944.f, 36.f});
        rightAligned(r, "KILLS " + std::to_string(kills_), {944.f, 58.f});
    }

    void draw(meno::Renderer& r) const {
        r.setCamera(hud_);

        if (screen_ == Screen::Title) {
            r.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                       {.fill = rgb(0x1A1420)});
            centered(r, "LAST STAND", {480.f, 220.f}, 48);
            centered(r, "PRESS ENTER", {480.f, 320.f}, 32);
            return;
        }

        if (screen_ == Screen::Result) {
            r.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                       {.fill = rgb(0x1A1420)});
            centered(r, survived_ ? "SURVIVED" : "GAME OVER",
                     {480.f, 130.f}, 48);
            centered(r, "TIME " + formatTime(elapsed_),
                     {480.f, 230.f}, 32);
            centered(r, "LEVEL " + std::to_string(level_),
                     {480.f, 280.f}, 32);
            centered(r, "KILLS " + std::to_string(kills_),
                     {480.f, 330.f}, 32);
            centered(r, "R: RESTART  ESC: QUIT",
                     {480.f, 430.f}, 32);
            return;
        }

        drawWorld(r);
        drawHud(r);

        if (paused_ || choosing_) {
            r.drawRect({{0.f, 6.f}, {960.f, 534.f}},
                       {.fill = Color{0, 0, 0, 160}});
        }

        if (choosing_) {
            centered(r, "LEVEL " + std::to_string(level_),
                     {480.f, 130.f}, 32);

            for (int i = 0; i < 3; ++i) {
                const auto rect = cardRect(i);
                r.drawRect(rect,
                           {.fill = rgb(0x2A2233),
                            .outline = rgb(0xFFD54A),
                            .outlineThickness = 2.f});
                const float centerX = rect.position.x + 120.f;
                centered(r, std::to_string(i + 1),
                         {centerX, 230.f}, 32);
                centered(r, AbilityNames[static_cast<std::size_t>(
                                choices_[static_cast<std::size_t>(i)])],
                         {centerX, 290.f}, 16);
            }
        } else if (paused_) {
            centered(r, "PAUSED", {480.f, 270.f}, 48);
        }
    }
};

} // namespace

int main() {
    try {
        LastStand game;
        if (!game.initialize()) {
            return 1;
        }
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
