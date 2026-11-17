```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerSpeed = 200.f;
constexpr double kPlayLimit = 300.0;
constexpr float kBulletSpeed = 500.f;
constexpr double kBulletLife = 1.2;
constexpr float kBulletRadius = 4.f;
constexpr float kGemRadius = 6.f;
constexpr float kGemFlySpeed = 300.f;
constexpr double kGemLife = 30.0;
constexpr float kDyingTime = 0.1f;
constexpr float kKnockTime = 0.1f;
constexpr float kKnockSpeed = 120.f;
constexpr float kTargetRange = 400.f;
constexpr double kFireInterval = 0.8;
constexpr std::size_t kMaxEnemies = 400;
constexpr float kPi = std::numbers::pi_v<float>;

struct EnemyKind {
    float radius;
    float hp;
    float speed;
    float contact;
    int frame;
    double animPeriod;
    int gemValue;
};

constexpr std::array<EnemyKind, 3> kKinds{{
    {10.f, 10.f, 80.f, 10.f, 24, 0.3, 1},
    {10.f, 6.f, 140.f, 6.f, 24, 0.15, 1},
    {20.f, 60.f, 50.f, 20.f, 48, 0.4, 5},
}};

constexpr std::array<const char*, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40"};

enum class Phase { Title, Play, Result };

struct Enemy {
    meno::Vec2f pos{0.f, 0.f};
    int kind = 0;
    float hp = 0.f;
    float knockTime = 0.f;
    meno::Vec2f knockDir{0.f, 0.f};
    float flashTime = 0.f;
    float dyingTime = -1.f;  // 0 이상이면 죽는 중
};

struct Bullet {
    meno::Vec2f pos{0.f, 0.f};
    meno::Vec2f dir{0.f, 0.f};
    double age = 0.0;
    float damage = 10.f;
};

struct Gem {
    meno::Vec2f pos{0.f, 0.f};
    int value = 1;
    double age = 0.0;
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

struct Assets {
    std::shared_ptr<const meno::Texture> player, walker, runner, brute, bullet, gem;
    std::shared_ptr<const meno::Font> font;
    SoundPool shoot, hit, pickup, levelup;
    std::optional<meno::Music> bgm;
};

float dist(meno::Vec2f a, meno::Vec2f b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

std::string mmss(double t) {
    const int s = static_cast<int>(t);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

void drawText(meno::Renderer& r, const meno::Font& f, const std::string& s, meno::Vec2f pos,
              unsigned int size, bool centered = false,
              meno::Color color = meno::colors::White) {
    meno::TextParams p{};
    p.characterSize = size;
    p.color = color;
    if (centered) {
        p.origin = r.measureTextBounds(f, s, p).center();
    }
    r.drawText(f, s, pos, p);
}

meno::Camera2D hudCamera() {
    return meno::Camera2D{.center = {kViewW / 2.f, kViewH / 2.f}, .size = {kViewW, kViewH}};
}

meno::Rectf cardRect(int i) {
    return meno::Rectf{{80.f + 280.f * static_cast<float>(i), 200.f}, {240.f, 140.f}};
}

class Survivor {
public:
    Survivor(meno::Game& game, Assets assets)
        : game_(game), assets_(std::move(assets)), rng_(std::random_device{}()) {
        resetState();
    }

    void update(double dt) {
        const meno::Input& in = game_.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }
        switch (phase_) {
            case Phase::Title:
                titleTimer_ += dt;
                if (in.wasPressed(meno::Key::Enter) || titleTimer_ >= 2.0) {
                    startPlay();
                }
                return;
            case Phase::Result:
                if (in.wasPressed(meno::Key::R)) {
                    startPlay();
                }
                return;
            case Phase::Play:
                break;
        }
        if (choosing_) {
            handleCards(in);
            return;
        }
        if (in.wasPressed(meno::Key::P)) {
            paused_ = !paused_;
        }
        if (paused_) {
            return;
        }
        updatePlay(in, dt);
    }

    void draw(meno::Renderer& r) {
        const meno::Font& font = *assets_.font;
        if (phase_ == Phase::Title) {
            r.setCamera(hudCamera());
            r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
            drawText(r, font, "LAST STAND", {kViewW / 2.f, 200.f}, 48, true);
            drawText(r, font, "PRESS ENTER", {kViewW / 2.f, 300.f}, 32, true);
            return;
        }
        if (phase_ == Phase::Result) {
            r.setCamera(hudCamera());
            r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
            drawText(r, font, victory_ ? "SURVIVED" : "GAME OVER", {kViewW / 2.f, 160.f}, 48, true);
            drawText(r, font, "TIME " + mmss(playTime_), {kViewW / 2.f, 240.f}, 32, true);
            drawText(r, font, "LEVEL " + std::to_string(level_), {kViewW / 2.f, 290.f}, 32, true);
            drawText(r, font, "KILLS " + std::to_string(kills_), {kViewW / 2.f, 340.f}, 32, true);
            drawText(r, font, "R: RESTART  ESC: QUIT", {kViewW / 2.f, 440.f}, 16, true);
            return;
        }
        drawPlay(r, font);
    }

private:
    meno::Game& game_;
    Assets assets_;
    std::mt19937 rng_;

    Phase phase_ = Phase::Title;
    double titleTimer_ = 0.0;
    bool victory_ = false;
    double playTime_ = 0.0;
    bool paused_ = false;
    bool choosing_ = false;
    int pendingCards_ = 0;
    std::array<int, 3> offer_{};
    float hp_ = 100.f;
    float maxHp_ = 100.f;
    int level_ = 1;
    float xp_ = 0.f;
    int kills_ = 0;
    float fireMul_ = 1.f;
    float speedMul_ = 1.f;
    float magnetRange_ = 60.f;
    float damage_ = 10.f;
    int projectiles_ = 1;
    double fireTimer_ = 0.0;
    double spawnTimer_ = 0.5;
    meno::Vec2f player_{kWorldW / 2.f, kWorldH / 2.f};
    bool moving_ = false;
    bool faceLeft_ = false;
    bool touching_ = false;
    std::vector<Enemy> enemies_;
    std::vector<Bullet> bullets_;
    std::vector<Gem> gems_;

    float rnd(float lo, float hi) {
        return std::uniform_real_distribution<float>{lo, hi}(rng_);
    }

    void resetState() {
        playTime_ = 0.0;
        paused_ = false;
        choosing_ = false;
        pendingCards_ = 0;
        hp_ = 100.f;
        maxHp_ = 100.f;
        level_ = 1;
        xp_ = 0.f;
        kills_ = 0;
        fireMul_ = 1.f;
        speedMul_ = 1.f;
        magnetRange_ = 60.f;
        damage_ = 10.f;
        projectiles_ = 1;
        fireTimer_ = 0.0;
        spawnTimer_ = 0.5;
        player_ = meno::Vec2f{kWorldW / 2.f, kWorldH / 2.f};
        moving_ = false;
        faceLeft_ = false;
        touching_ = false;
        enemies_.clear();
        bullets_.clear();
        gems_.clear();
    }

    void startPlay() {
        resetState();
        phase_ = Phase::Play;
        if (assets_.bgm) {
            assets_.bgm->stop();
            assets_.bgm->play();
        }
    }

    void endGame(bool victory) {
        phase_ = Phase::Result;
        victory_ = victory;
        choosing_ = false;
        paused_ = false;
        if (assets_.bgm) {
            assets_.bgm->stop();
        }
    }

    meno::Vec2f cameraCenter() const {
        return meno::Vec2f{std::clamp(player_.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                           std::clamp(player_.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
    }

    meno::Camera2D worldCamera() const {
        return meno::Camera2D{.center = cameraCenter(), .size = {kViewW, kViewH}};
    }

    double spawnInterval() const {
        return std::max(0.15, 0.6 - 0.05 * std::floor(playTime_ / 15.0));
    }

    int pickKind() {
        if (playTime_ < 60.0) {
            return 0;
        }
        const float r = rnd(0.f, 1.f);
        if (playTime_ < 120.0) {
            return r < 0.7f ? 0 : 1;
        }
        if (r < 0.6f) {
            return 0;
        }
        return r < 0.9f ? 1 : 2;
    }

    void spawnEnemy() {
        const meno::Vec2f c = cameraCenter();
        const float hw = kViewW / 2.f + 64.f;
        const float hh = kViewH / 2.f + 64.f;
        const int side = std::min(3, static_cast<int>(rnd(0.f, 4.f)));
        meno::Vec2f p{c.x, c.y};
        switch (side) {
            case 0: p = meno::Vec2f{rnd(c.x - hw, c.x + hw), c.y - hh}; break;
            case 1: p = meno::Vec2f{rnd(c.x - hw, c.x + hw), c.y + hh}; break;
            case 2: p = meno::Vec2f{c.x - hw, rnd(c.y - hh, c.y + hh)}; break;
            default: p = meno::Vec2f{c.x + hw, rnd(c.y - hh, c.y + hh)}; break;
        }
        p.x = std::clamp(p.x, 0.f, kWorldW);
        p.y = std::clamp(p.y, 0.f, kWorldH);
        Enemy e;
        e.pos = p;
        e.kind = pickKind();
        e.hp = kKinds[e.kind].hp;
        enemies_.push_back(e);
    }

    void fire() {
        const Enemy* target = nullptr;
        float best = kTargetRange * kTargetRange;
        for (const Enemy& e : enemies_) {
            if (e.dyingTime >= 0.f) {
                continue;
            }
            const float dx = e.pos.x - player_.x;
            const float dy = e.pos.y - player_.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= best) {
                best = d2;
                target = &e;
            }
        }
        if (target == nullptr) {
            return;
        }
        const float base = std::atan2(target->pos.y - player_.y, target->pos.x - player_.x);
        const int n = projectiles_;
        for (int i = 0; i < n; ++i) {
            const float offset =
                (static_cast<float>(i) - static_cast<float>(n - 1) / 2.f) * (10.f * kPi / 180.f);
            const float a = base + offset;
            Bullet b;
            b.pos = player_;
            b.dir = meno::Vec2f{std::cos(a), std::sin(a)};
            b.damage = damage_;
            bullets_.push_back(b);
        }
        assets_.shoot.play();
    }

    void hitEnemy(Enemy& e, const Bullet& b) {
        e.hp -= b.damage;
        if (e.hp <= 0.f) {
            e.hp = 0.f;
            e.dyingTime = 0.f;
            Gem g;
            g.pos = e.pos;
            g.value = kKinds[e.kind].gemValue;
            gems_.push_back(g);
            ++kills_;
            assets_.hit.play();
            return;
        }
        e.knockDir = b.dir;
        e.knockTime = kKnockTime;
        e.flashTime = kKnockTime;
    }

    void openCards() {
        std::array<int, 6> idx{0, 1, 2, 3, 4, 5};
        std::shuffle(idx.begin(), idx.end(), rng_);
        offer_ = {idx[0], idx[1], idx[2]};
        choosing_ = true;
    }

    void applyCard(int c) {
        switch (c) {
            case 0: fireMul_ *= 0.85f; break;
            case 1: damage_ += 5.f; break;
            case 2: projectiles_ += 1; break;
            case 3: speedMul_ *= 1.1f; break;
            case 4: maxHp_ += 20.f; hp_ += 20.f; break;
            default: magnetRange_ += 40.f; break;
        }
    }

    void handleCards(const meno::Input& in) {
        int choice = -1;
        if (in.wasPressed(meno::Key::Num1) || in.wasPressed(meno::Key::Numpad1)) {
            choice = 0;
        } else if (in.wasPressed(meno::Key::Num2) || in.wasPressed(meno::Key::Numpad2)) {
            choice = 1;
        } else if (in.wasPressed(meno::Key::Num3) || in.wasPressed(meno::Key::Numpad3)) {
            choice = 2;
        }
        if (in.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f point = meno::screenToWorld(
                hudCamera(), game_.renderer().framebufferSize(), meno::Vec2f{in.mousePosition()});
            for (int i = 0; i < 3; ++i) {
                if (cardRect(i).contains(point)) {
                    choice = i;
                }
            }
        }
        if (choice < 0) {
            return;
        }
        applyCard(offer_[choice]);
        if (--pendingCards_ > 0) {
            openCards();
        } else {
            choosing_ = false;
        }
    }

    void updatePlay(const meno::Input& in, double dt) {
        const float fdt = static_cast<float>(dt);
        playTime_ += dt;
        if (playTime_ >= kPlayLimit) {
            endGame(true);
            return;
        }

        // 플레이어 이동 (8방향, 대각선도 같은 속력)
        float mx = 0.f;
        float my = 0.f;
        if (in.isDown(meno::Key::Left) || in.isDown(meno::Key::A)) mx -= 1.f;
        if (in.isDown(meno::Key::Right) || in.isDown(meno::Key::D)) mx += 1.f;
        if (in.isDown(meno::Key::Up) || in.isDown(meno::Key::W)) my -= 1.f;
        if (in.isDown(meno::Key::Down) || in.isDown(meno::Key::S)) my += 1.f;
        const float len = std::hypot(mx, my);
        moving_ = len > 0.f;
        if (moving_) {
            mx /= len;
            my /= len;
            if (mx < 0.f) {
                faceLeft_ = true;
            } else if (mx > 0.f) {
                faceLeft_ = false;
            }
            const float step = kPlayerSpeed * speedMul_ * fdt;
            player_.x = std::clamp(player_.x + mx * step, kPlayerRadius, kWorldW - kPlayerRadius);
            player_.y = std::clamp(player_.y + my * step, kPlayerRadius, kWorldH - kPlayerRadius);
        }

        // 적 생성
        spawnTimer_ -= dt;
        while (spawnTimer_ <= 0.0) {
            if (enemies_.size() < kMaxEnemies) {
                spawnEnemy();
            }
            spawnTimer_ += spawnInterval();
        }

        // 적 이동과 접촉 피해
        touching_ = false;
        for (Enemy& e : enemies_) {
            if (e.dyingTime >= 0.f) {
                e.dyingTime += fdt;
                continue;
            }
            const EnemyKind& k = kKinds[e.kind];
            e.flashTime = std::max(0.f, e.flashTime - fdt);
            if (e.knockTime > 0.f) {
                e.knockTime = std::max(0.f, e.knockTime - fdt);
                e.pos.x = std::clamp(e.pos.x + e.knockDir.x * kKnockSpeed * fdt, 0.f, kWorldW);
                e.pos.y = std::clamp(e.pos.y + e.knockDir.y * kKnockSpeed * fdt, 0.f, kWorldH);
            } else {
                const float d = dist(player_, e.pos);
                if (d > 0.f) {
                    e.pos.x += (player_.x - e.pos.x) / d * k.speed * fdt;
                    e.pos.y += (player_.y - e.pos.y) / d * k.speed * fdt;
                }
            }
            if (dist(e.pos, player_) < k.radius + kPlayerRadius) {
                hp_ -= k.contact * fdt;
                touching_ = true;
            }
        }

        // 자동 사격
        fireTimer_ += dt;
        const double interval = kFireInterval * static_cast<double>(fireMul_);
        if (fireTimer_ >= interval) {
            fireTimer_ -= interval;
            fire();
        }

        // 총알 이동과 피격
        std::erase_if(bullets_, [&](Bullet& b) {
            b.pos.x += b.dir.x * kBulletSpeed * fdt;
            b.pos.y += b.dir.y * kBulletSpeed * fdt;
            b.age += dt;
            if (b.age >= kBulletLife) {
                return true;
            }
            for (Enemy& e : enemies_) {
                if (e.dyingTime >= 0.f) {
                    continue;
                }
                if (dist(b.pos, e.pos) < kBulletRadius + kKinds[e.kind].radius) {
                    hitEnemy(e, b);
                    return true;
                }
            }
            return false;
        });

        // 보석: 자석 이동과 획득
        std::erase_if(gems_, [&](Gem& g) {
            g.age += dt;
            if (g.age >= kGemLife) {
                return true;
            }
            const float d = dist(g.pos, player_);
            if (d > 0.f && d <= magnetRange_) {
                g.pos.x += (player_.x - g.pos.x) / d * kGemFlySpeed * fdt;
                g.pos.y += (player_.y - g.pos.y) / d * kGemFlySpeed * fdt;
            }
            if (dist(g.pos, player_) <= kPlayerRadius + kGemRadius) {
                xp_ += static_cast<float>(g.value);
                assets_.pickup.play();
                return true;
            }
            return false;
        });

        // 레벨업
        while (xp_ >= 5.f * static_cast<float>(level_)) {
            xp_ -= 5.f * static_cast<float>(level_);
            ++level_;
            ++pendingCards_;
            assets_.levelup.play();
        }
        if (pendingCards_ > 0 && !choosing_) {
            openCards();
        }

        // 죽은 적 정리
        std::erase_if(enemies_, [](const Enemy& e) { return e.dyingTime >= kDyingTime; });

        if (hp_ <= 0.f) {
            hp_ = 0.f;
            endGame(false);
        }
    }

    void drawPlay(meno::Renderer& r, const meno::Font& font) {
        const meno::Color panel = meno::Color::fromRgb(0x2A2233);

        // 월드
        r.setCamera(worldCamera());
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
        const meno::Color grid = meno::Color::fromRgb(0x2A2233);
        for (int i = 0; i <= 24; ++i) {
            const float x = 120.f * static_cast<float>(i);
            r.drawLine({x, 0.f}, {x, kWorldH}, {.color = grid, .thickness = 1.f});
        }
        for (int j = 0; j <= 13; ++j) {
            const float y = 120.f * static_cast<float>(j);
            r.drawLine({0.f, y}, {kWorldW, y}, {.color = grid, .thickness = 1.f});
        }
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                   {.fill = meno::colors::Transparent,
                    .outline = meno::Color::fromRgb(0x6B5B7B),
                    .outlineThickness = 2.f});

        for (const Gem& g : gems_) {
            r.draw(*assets_.gem,
                   {.position = g.pos,
                    .origin = {6.f, 6.f},
                    .tint = g.value >= 5 ? meno::Color::fromRgb(0xFFD54A)
                                         : meno::Color::fromRgb(0x4FC3F7)});
        }

        for (const Enemy& e : enemies_) {
            const EnemyKind& k = kKinds[e.kind];
            const auto& tex = e.kind == 0 ? assets_.walker
                                          : (e.kind == 1 ? assets_.runner : assets_.brute);
            const bool hitPose = e.dyingTime >= 0.f || e.flashTime > 0.f;
            const int frame = hitPose ? 2 : static_cast<int>(playTime_ / k.animPeriod) % 2;
            const int s = k.frame;
            r.draw(*tex,
                   {.position = e.pos,
                    .origin = {static_cast<float>(s) / 2.f, static_cast<float>(s) / 2.f},
                    .source = meno::Recti{frame * s, 0, s, s},
                    .flipX = player_.x < e.pos.x});
        }

        for (const Bullet& b : bullets_) {
            r.draw(*assets_.bullet, {.position = b.pos, .origin = {4.f, 4.f}});
        }

        const int pf = moving_ ? static_cast<int>(playTime_ / 0.15) % 2 : 0;
        r.draw(*assets_.player,
               {.position = player_,
                .origin = {16.f, 16.f},
                .tint = touching_ ? meno::Color::fromRgb(0xFF6060) : meno::colors::White,
                .source = meno::Recti{pf * 32, 0, 32, 32},
                .flipX = faceLeft_});

        // HUD (화면 고정)
        r.setCamera(hudCamera());
        const float xpRatio = std::clamp(xp_ / (5.f * static_cast<float>(level_)), 0.f, 1.f);
        r.drawRect({{0.f, 0.f}, {kViewW, 6.f}}, {.fill = panel});
        r.drawRect({{0.f, 0.f}, {kViewW * xpRatio, 6.f}},
                   {.fill = meno::Color::fromRgb(0x4FC3F7)});

        const float hpRatio = std::clamp(hp_ / maxHp_, 0.f, 1.f);
        r.drawRect({{16.f, 14.f}, {200.f, 12.f}}, {.fill = meno::Color::fromRgb(0x3A1F24)});
        r.drawRect({{16.f, 14.f}, {200.f * hpRatio, 12.f}},
                   {.fill = meno::Color::fromRgb(0xE5533D)});
        const int hpShown = static_cast<int>(std::ceil(std::max(hp_, 0.f)));
        drawText(r, font, "HP " + std::to_string(hpShown), {226.f, 12.f}, 16);

        const std::string stats = "LV " + std::to_string(level_) + "   TIME " + mmss(playTime_) +
                                  "   KILLS " + std::to_string(kills_);
        meno::TextParams sp{};
        sp.characterSize = 16;
        const float statsWidth = r.measureTextBounds(font, stats, sp).size.x;
        drawText(r, font, stats, {kViewW - 16.f - statsWidth, 12.f}, 16);

        if (choosing_) {
            drawText(r, font, "LEVEL UP", {kViewW / 2.f, 140.f}, 32, true);
            for (int i = 0; i < 3; ++i) {
                const meno::Rectf box = cardRect(i);
                r.drawRect(box, {.fill = panel,
                                 .outline = meno::Color::fromRgb(0xFFD54A),
                                 .outlineThickness = 2.f});
                drawText(r, font, kCardNames[offer_[i]],
                         {box.position.x + box.size.x / 2.f, box.position.y + box.size.y / 2.f},
                         16, true);
            }
        }
        if (paused_) {
            drawText(r, font, "PAUSED", {kViewW / 2.f, kViewH / 2.f}, 48, true);
        }
    }
};

struct PoolLoad {};

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "LAST STAND",
                     .clearColor = meno::colors::Black}};

    std::vector<std::string> missing;

    const auto texture = [&](const char* path) {
        auto t = game.resources().loadTexture(path);
        if (!t) {
            missing.emplace_back(path);
        }
        return t;
    };
    const auto pool = [&](const char* path, int count) {
        SoundPool p;
        auto b = game.resources().loadSoundBuffer(path);
        if (!b) {
            missing.emplace_back(path);
            return p;
        }
        for (int i = 0; i < count; ++i) {
            p.sounds.emplace_back(b);
        }
        return p;
    };

    Assets assets;
    assets.player = texture("assets/textures/sv_player.png");
    assets.walker = texture("assets/textures/sv_walker.png");
    assets.runner = texture("assets/textures/sv_runner.png");
    assets.brute = texture("assets/textures/sv_brute.png");
    assets.bullet = texture("assets/textures/sv_bullet.png");
    assets.gem = texture("assets/textures/sv_gem.png");
    assets.font = game.resources().loadFont("assets/fonts/game.ttf");
    if (!assets.font) {
        missing.emplace_back("assets/fonts/game.ttf");
    }
    assets.shoot = pool("assets/sfx/shoot.wav", 8);
    assets.hit = pool("assets/sfx/hit.wav", 8);
    assets.pickup = pool("assets/sfx/pickup.wav", 8);
    assets.levelup = pool("assets/sfx/levelup.wav", 2);
    assets.bgm = meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
    if (!assets.bgm) {
        missing.emplace_back("assets/music/bgm_survivor.ogg");
    } else {
        assets.bgm->setLooping(true);
        assets.bgm->setVolume(0.4f);
    }

    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing file: " << path << '\n';
        }
        return 1;
    }

    Survivor survivor{game, std::move(assets)};
    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double dt) { survivor.update(dt); });
    scene.setDrawHandler([&](meno::Renderer& r) { survivor.draw(r); });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}
```