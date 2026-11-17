```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kWorldW = 2880.f;
constexpr float kWorldH = 1620.f;
constexpr float kPi = 3.14159265f;

enum class Phase { Title, Play, Result };
enum class Kind { Walker, Runner, Brute };

struct KindStats {
    float radius;
    float hp;
    float speed;
    float dps;
    float animInterval;
    int frameSize;
};

constexpr std::array<KindStats, 3> kKinds{{
    {10.f, 10.f, 80.f, 10.f, 0.3f, 24},
    {10.f, 6.f, 140.f, 6.f, 0.15f, 24},
    {20.f, 60.f, 50.f, 20.f, 0.4f, 48},
}};

constexpr std::array<const char*, 6> kCardNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40"};

const KindStats& stats(Kind k) { return kKinds[static_cast<std::size_t>(k)]; }
std::size_t idx(Kind k) { return static_cast<std::size_t>(k); }

struct Assets {
    std::shared_ptr<const meno::Font> font;
    std::shared_ptr<const meno::Texture> player, bullet, gem;
    std::array<std::shared_ptr<const meno::Texture>, 3> enemy;
    std::shared_ptr<const meno::SoundBuffer> shoot, hit, pickup, levelup;
    std::optional<meno::Music> bgm;
};

struct Enemy {
    Kind kind = Kind::Walker;
    meno::Vec2f pos{};
    float hp = 0.f;
    bool dying = false;
    float dieT = 0.f;
    float knockT = 0.f;
    meno::Vec2f knockDir{};
    float flashT = 0.f;
};

struct Bullet {
    meno::Vec2f pos;
    meno::Vec2f vel;
    float life;
    float damage;
};

struct Gem {
    meno::Vec2f pos;
    int value;
    float life;
};

float dist2(meno::Vec2f a, meno::Vec2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::string two(int v) { return v < 10 ? "0" + std::to_string(v) : std::to_string(v); }

void drawCentered(meno::Renderer& r, const meno::Font& font, const std::string& text,
                  meno::Vec2f pos, meno::TextParams params) {
    params.origin = r.measureTextBounds(font, text, params).center();
    r.drawText(font, text, pos, params);
}

void drawFrame(meno::Renderer& r, const meno::Texture& tex, int size, int frame, meno::Vec2f pos,
               bool flip, meno::Color tint) {
    const float half = static_cast<float>(size) / 2.f;
    r.draw(tex, {.position = pos,
                 .origin = {half, half},
                 .tint = tint,
                 .source = meno::Recti{frame * size, 0, size, size},
                 .flipX = flip});
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
        : game_(game),
          a_(std::move(assets)),
          shoot_(a_.shoot),
          hit_(a_.hit),
          pickup_(a_.pickup),
          levelup_(a_.levelup),
          rng_(std::random_device{}()) {
        reset();
    }

    void update(float dt) {
        const meno::Input& in = game_.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }
        switch (phase_) {
            case Phase::Title:
                titleTime_ += dt;
                if (in.wasPressed(meno::Key::Enter) || titleTime_ >= 2.f) {
                    startPlay();
                }
                break;
            case Phase::Result:
                if (in.wasPressed(meno::Key::R)) {
                    startPlay();
                }
                break;
            case Phase::Play:
                updatePlay(dt);
                break;
        }
    }

    void draw(meno::Renderer& r) {
        switch (phase_) {
            case Phase::Title:
                drawTitle(r);
                break;
            case Phase::Result:
                drawResult(r);
                break;
            case Phase::Play:
                drawWorld(r);
                drawHud(r);
                if (pending_ > 0) {
                    drawCards(r);
                } else if (paused_) {
                    r.setCamera(hudCamera());
                    drawCentered(r, *a_.font, "PAUSED", {kViewW / 2.f, kViewH / 2.f},
                                 {.characterSize = 48, .color = meno::colors::White});
                }
                break;
        }
    }

private:
    meno::Game& game_;
    Assets a_;
    meno::Sound shoot_;
    meno::Sound hit_;
    meno::Sound pickup_;
    meno::Sound levelup_;
    std::mt19937 rng_;
    std::uniform_real_distribution<float> unit_{0.f, 1.f};

    Phase phase_ = Phase::Title;
    float titleTime_ = 0.f;
    bool won_ = false;
    bool paused_ = false;
    double playTime_ = 0.0;
    float spawnCd_ = 0.5f;
    meno::Vec2f player_{};
    float hp_ = 100.f;
    float maxHp_ = 100.f;
    float speedMul_ = 1.f;
    float fireMul_ = 1.f;
    float magnet_ = 0.f;
    float damage_ = 10.f;
    int projectiles_ = 1;
    int level_ = 1;
    int xp_ = 0;
    int kills_ = 0;
    int pending_ = 0;
    std::array<int, 3> cards_{};
    float fireT_ = 0.f;
    float walkT_ = 0.f;
    bool facingLeft_ = false;
    bool moving_ = false;
    bool hurt_ = false;
    std::vector<Enemy> enemies_;
    std::vector<Bullet> bullets_;
    std::vector<Gem> gems_;

    void reset() {
        player_ = meno::Vec2f{kWorldW / 2.f, kWorldH / 2.f};
        hp_ = 100.f;
        maxHp_ = 100.f;
        speedMul_ = 1.f;
        fireMul_ = 1.f;
        magnet_ = 0.f;
        damage_ = 10.f;
        projectiles_ = 1;
        level_ = 1;
        xp_ = 0;
        kills_ = 0;
        pending_ = 0;
        paused_ = false;
        won_ = false;
        playTime_ = 0.0;
        spawnCd_ = 0.5f;
        fireT_ = 0.f;
        walkT_ = 0.f;
        facingLeft_ = false;
        moving_ = false;
        hurt_ = false;
        enemies_.clear();
        bullets_.clear();
        gems_.clear();
    }

    void startPlay() {
        reset();
        phase_ = Phase::Play;
        if (a_.bgm) {
            a_.bgm->play();
        }
    }

    void endGame(bool survived) {
        won_ = survived;
        phase_ = Phase::Result;
        if (a_.bgm) {
            a_.bgm->stop();
        }
    }

    void updatePlay(float dt) {
        const meno::Input& in = game_.window().input();
        if (pending_ > 0) {
            chooseCard();
            return;
        }
        if (in.wasPressed(meno::Key::P)) {
            paused_ = !paused_;
        }
        if (paused_) {
            return;
        }

        playTime_ += dt;
        movePlayer(dt);
        spawn(dt);
        updateEnemies(dt);
        contactDamage(dt);
        fire(dt);
        updateBullets(dt);
        updateGems(dt);
        checkLevel();

        if (hp_ <= 0.f) {
            endGame(false);
        } else if (playTime_ >= 300.0) {
            endGame(true);
        }
    }

    void movePlayer(float dt) {
        const meno::Input& in = game_.window().input();
        float dx = 0.f;
        float dy = 0.f;
        if (in.isDown(meno::Key::Left) || in.isDown(meno::Key::A)) dx -= 1.f;
        if (in.isDown(meno::Key::Right) || in.isDown(meno::Key::D)) dx += 1.f;
        if (in.isDown(meno::Key::Up) || in.isDown(meno::Key::W)) dy -= 1.f;
        if (in.isDown(meno::Key::Down) || in.isDown(meno::Key::S)) dy += 1.f;
        if (dx != 0.f && dy != 0.f) {
            dx *= 0.70710678f;
            dy *= 0.70710678f;
        }
        const float step = 200.f * speedMul_ * dt;
        player_.x = std::clamp(player_.x + dx * step, 14.f, kWorldW - 14.f);
        player_.y = std::clamp(player_.y + dy * step, 14.f, kWorldH - 14.f);
        moving_ = dx != 0.f || dy != 0.f;
        if (dx < 0.f) {
            facingLeft_ = true;
        } else if (dx > 0.f) {
            facingLeft_ = false;
        }
        walkT_ = moving_ ? walkT_ + dt : 0.f;
    }

    meno::Vec2f cameraCenter() const {
        return meno::Vec2f{std::clamp(player_.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                           std::clamp(player_.y, kViewH / 2.f, kWorldH - kViewH / 2.f)};
    }

    Kind pickKind() {
        const float r = unit_(rng_);
        if (playTime_ < 60.0) return Kind::Walker;
        if (playTime_ < 120.0) return r < 0.7f ? Kind::Walker : Kind::Runner;
        if (r < 0.6f) return Kind::Walker;
        if (r < 0.9f) return Kind::Runner;
        return Kind::Brute;
    }

    meno::Vec2f spawnPoint() {
        const meno::Vec2f c = cameraCenter();
        const float left = c.x - kViewW / 2.f - 64.f;
        const float right = c.x + kViewW / 2.f + 64.f;
        const float top = c.y - kViewH / 2.f - 64.f;
        const float bottom = c.y + kViewH / 2.f + 64.f;
        const float t = unit_(rng_);
        const int side = std::min(3, static_cast<int>(unit_(rng_) * 4.f));
        meno::Vec2f p{left, top};
        switch (side) {
            case 0: p = meno::Vec2f{left + t * (right - left), top}; break;
            case 1: p = meno::Vec2f{left + t * (right - left), bottom}; break;
            case 2: p = meno::Vec2f{left, top + t * (bottom - top)}; break;
            default: p = meno::Vec2f{right, top + t * (bottom - top)}; break;
        }
        return meno::Vec2f{std::clamp(p.x, 0.f, kWorldW), std::clamp(p.y, 0.f, kWorldH)};
    }

    void spawn(float dt) {
        spawnCd_ -= dt;
        if (spawnCd_ > 0.f) {
            return;
        }
        const float interval =
            std::max(0.15f, 0.6f - 0.05f * static_cast<float>(std::floor(playTime_ / 15.0)));
        spawnCd_ += interval;
        if (enemies_.size() >= 400) {
            return;
        }
        Enemy e;
        e.kind = pickKind();
        e.hp = stats(e.kind).hp;
        e.pos = spawnPoint();
        enemies_.push_back(e);
    }

    void updateEnemies(float dt) {
        for (Enemy& e : enemies_) {
            if (e.dying) {
                e.dieT -= dt;
                continue;
            }
            const KindStats& s = stats(e.kind);
            if (e.flashT > 0.f) {
                e.flashT -= dt;
            }
            if (e.knockT > 0.f) {
                e.knockT -= dt;
                e.pos.x += e.knockDir.x * 120.f * dt;
                e.pos.y += e.knockDir.y * 120.f * dt;
            } else {
                const float dx = player_.x - e.pos.x;
                const float dy = player_.y - e.pos.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.001f) {
                    e.pos.x += dx / len * s.speed * dt;
                    e.pos.y += dy / len * s.speed * dt;
                }
            }
            e.pos.x = std::clamp(e.pos.x, s.radius, kWorldW - s.radius);
            e.pos.y = std::clamp(e.pos.y, s.radius, kWorldH - s.radius);
        }
        std::erase_if(enemies_, [](const Enemy& e) { return e.dying && e.dieT <= 0.f; });
    }

    void contactDamage(float dt) {
        hurt_ = false;
        for (const Enemy& e : enemies_) {
            if (e.dying) {
                continue;
            }
            const float reach = 14.f + stats(e.kind).radius;
            if (dist2(player_, e.pos) <= reach * reach) {
                hp_ -= stats(e.kind).dps * dt;
                hurt_ = true;
            }
        }
    }

    std::optional<meno::Vec2f> nearestTarget(float range) const {
        std::optional<meno::Vec2f> best;
        float bestD2 = range * range;
        for (const Enemy& e : enemies_) {
            if (e.dying) {
                continue;
            }
            const float d2 = dist2(player_, e.pos);
            if (d2 <= bestD2) {
                bestD2 = d2;
                best = e.pos;
            }
        }
        return best;
    }

    void fire(float dt) {
        fireT_ += dt;
        const float interval = 0.8f * fireMul_;
        if (fireT_ < interval) {
            return;
        }
        const std::optional<meno::Vec2f> target = nearestTarget(400.f);
        if (!target) {
            return;
        }
        fireT_ = 0.f;
        const float base = std::atan2(target->y - player_.y, target->x - player_.x);
        const int n = projectiles_;
        for (int i = 0; i < n; ++i) {
            const float spread = (static_cast<float>(i) - (static_cast<float>(n) - 1.f) / 2.f) *
                                 10.f * kPi / 180.f;
            const float a = base + spread;
            bullets_.push_back(Bullet{player_,
                                      meno::Vec2f{std::cos(a) * 500.f, std::sin(a) * 500.f},
                                      1.2f, damage_});
        }
        shoot_.play();
    }

    void hitEnemy(Enemy& e, const Bullet& b) {
        e.hp -= b.damage;
        e.flashT = 0.1f;
        if (e.hp <= 0.f) {
            e.dying = true;
            e.dieT = 0.1f;
            ++kills_;
            gems_.push_back(Gem{e.pos, e.kind == Kind::Brute ? 5 : 1, 30.f});
            hit_.play();
        } else {
            const float speed = std::sqrt(b.vel.x * b.vel.x + b.vel.y * b.vel.y);
            e.knockDir = meno::Vec2f{b.vel.x / speed, b.vel.y / speed};
            e.knockT = 0.1f;
        }
    }

    void updateBullets(float dt) {
        for (Bullet& b : bullets_) {
            b.pos.x += b.vel.x * dt;
            b.pos.y += b.vel.y * dt;
            b.life -= dt;
            if (b.life <= 0.f) {
                continue;
            }
            for (Enemy& e : enemies_) {
                if (e.dying) {
                    continue;
                }
                const float reach = 4.f + stats(e.kind).radius;
                if (dist2(b.pos, e.pos) <= reach * reach) {
                    hitEnemy(e, b);
                    b.life = 0.f;
                    break;
                }
            }
        }
        std::erase_if(bullets_, [](const Bullet& b) { return b.life <= 0.f; });
    }

    void updateGems(float dt) {
        const float magnetR = 60.f + magnet_;
        for (Gem& g : gems_) {
            g.life -= dt;
            const float d2 = dist2(player_, g.pos);
            if (d2 > 0.f && d2 <= magnetR * magnetR) {
                const float d = std::sqrt(d2);
                g.pos.x += (player_.x - g.pos.x) / d * 300.f * dt;
                g.pos.y += (player_.y - g.pos.y) / d * 300.f * dt;
            }
        }
        for (Gem& g : gems_) {
            if (g.life > 0.f && dist2(player_, g.pos) <= 20.f * 20.f) {
                xp_ += g.value;
                g.life = 0.f;
                pickup_.play();
            }
        }
        std::erase_if(gems_, [](const Gem& g) { return g.life <= 0.f; });
    }

    void checkLevel() {
        bool leveled = false;
        while (xp_ >= 5 * level_) {
            xp_ -= 5 * level_;
            ++level_;
            ++pending_;
            leveled = true;
        }
        if (leveled) {
            levelup_.play();
            rollCards();
        }
    }

    void rollCards() {
        std::array<int, 6> pool{0, 1, 2, 3, 4, 5};
        std::shuffle(pool.begin(), pool.end(), rng_);
        cards_ = {pool[0], pool[1], pool[2]};
    }

    void applyCard(int id) {
        switch (id) {
            case 0: fireMul_ *= 0.85f; break;
            case 1: damage_ += 5.f; break;
            case 2: projectiles_ += 1; break;
            case 3: speedMul_ *= 1.1f; break;
            case 4: maxHp_ += 20.f; hp_ += 20.f; break;
            case 5: magnet_ += 40.f; break;
            default: break;
        }
    }

    void chooseCard() {
        const meno::Input& in = game_.window().input();
        int pick = -1;
        if (in.wasPressed(meno::Key::Num1)) pick = 0;
        if (in.wasPressed(meno::Key::Num2)) pick = 1;
        if (in.wasPressed(meno::Key::Num3)) pick = 2;
        if (in.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse{in.mousePosition()};
            const meno::Vec2f p =
                meno::screenToWorld(hudCamera(), game_.renderer().framebufferSize(), mouse);
            for (int i = 0; i < 3; ++i) {
                if (cardRect(i).contains(p)) {
                    pick = i;
                }
            }
        }
        if (pick < 0) {
            return;
        }
        applyCard(cards_[static_cast<std::size_t>(pick)]);
        --pending_;
        if (pending_ > 0) {
            rollCards();
        }
    }

    std::string timeText() const {
        const int s = static_cast<int>(std::min(playTime_, 300.0));
        return two(s / 60) + ":" + two(s % 60);
    }

    void drawTitle(meno::Renderer& r) {
        r.setCamera(hudCamera());
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
        drawCentered(r, *a_.font, "LAST STAND", {kViewW / 2.f, 200.f},
                     {.characterSize = 48, .color = meno::colors::White});
        drawCentered(r, *a_.font, "PRESS ENTER", {kViewW / 2.f, 300.f},
                     {.characterSize = 32, .color = meno::colors::White});
    }

    void drawResult(meno::Renderer& r) {
        r.setCamera(hudCamera());
        r.drawRect({{0.f, 0.f}, {kViewW, kViewH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
        drawCentered(r, *a_.font, won_ ? "SURVIVED" : "GAME OVER", {kViewW / 2.f, 160.f},
                     {.characterSize = 48, .color = meno::colors::White});
        drawCentered(r, *a_.font, "TIME " + timeText(), {kViewW / 2.f, 240.f},
                     {.characterSize = 32, .color = meno::colors::White});
        drawCentered(r, *a_.font, "LEVEL " + std::to_string(level_), {kViewW / 2.f, 290.f},
                     {.characterSize = 32, .color = meno::colors::White});
        drawCentered(r, *a_.font, "KILLS " + std::to_string(kills_), {kViewW / 2.f, 340.f},
                     {.characterSize = 32, .color = meno::colors::White});
        drawCentered(r, *a_.font, "R: RESTART  ESC: QUIT", {kViewW / 2.f, 440.f},
                     {.characterSize = 16, .color = meno::colors::White});
    }

    void drawWorld(meno::Renderer& r) {
        r.setCamera(meno::Camera2D{.center = cameraCenter(), .size = {kViewW, kViewH}});
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}}, {.fill = meno::Color::fromRgb(0x1A1420)});
        const meno::Color grid = meno::Color::fromRgb(0x2A2233);
        for (int x = 0; x <= 2880; x += 120) {
            const float fx = static_cast<float>(x);
            r.drawLine({fx, 0.f}, {fx, kWorldH}, {.color = grid, .thickness = 1.f});
        }
        for (int y = 0; y <= 1620; y += 120) {
            const float fy = static_cast<float>(y);
            r.drawLine({0.f, fy}, {kWorldW, fy}, {.color = grid, .thickness = 1.f});
        }
        r.drawRect({{0.f, 0.f}, {kWorldW, kWorldH}},
                   {.fill = meno::colors::Transparent,
                    .outline = meno::Color::fromRgb(0x6B5B7B),
                    .outlineThickness = -2.f});

        for (const Gem& g : gems_) {
            r.draw(*a_.gem, {.position = g.pos,
                             .origin = {6.f, 6.f},
                             .tint = g.value > 1 ? meno::Color::fromRgb(0xFFD54A)
                                                 : meno::Color::fromRgb(0x4FC3F7)});
        }
        for (const Enemy& e : enemies_) {
            const KindStats& s = stats(e.kind);
            const int anim = static_cast<int>(playTime_ / s.animInterval) % 2;
            const int frame = (e.dying || e.flashT > 0.f) ? 2 : anim;
            drawFrame(r, *a_.enemy[idx(e.kind)], s.frameSize, frame, e.pos,
                      player_.x < e.pos.x, meno::colors::White);
        }
        for (const Bullet& b : bullets_) {
            r.draw(*a_.bullet, {.position = b.pos, .origin = {4.f, 4.f}});
        }
        const int walkFrame = moving_ ? static_cast<int>(walkT_ / 0.15f) % 2 : 0;
        drawFrame(r, *a_.player, 32, walkFrame, player_, facingLeft_,
                  hurt_ ? meno::Color::fromRgb(0xFF6060) : meno::colors::White);
    }

    void drawHud(meno::Renderer& r) {
        r.setCamera(hudCamera());
        const float need = 5.f * static_cast<float>(level_);
        const float xpRatio = std::clamp(static_cast<float>(xp_) / need, 0.f, 1.f);
        r.drawRect({{0.f, 0.f}, {kViewW, 6.f}}, {.fill = meno::Color::fromRgb(0x2A2233)});
        r.drawRect({{0.f, 0.f}, {kViewW * xpRatio, 6.f}}, {.fill = meno::Color::fromRgb(0x4FC3F7)});

        const float hpRatio = std::clamp(hp_ / maxHp_, 0.f, 1.f);
        r.drawRect({{16.f, 16.f}, {200.f, 12.f}}, {.fill = meno::Color::fromRgb(0x3A1F24)});
        r.drawRect({{16.f, 16.f}, {200.f * hpRatio, 12.f}},
                   {.fill = meno::Color::fromRgb(0xE5533D)});

        const int hpShown = static_cast<int>(std::ceil(std::max(hp_, 0.f)));
        const meno::TextParams text{.characterSize = 16, .color = meno::colors::White};
        r.drawText(*a_.font, "HP " + std::to_string(hpShown), {226.f, 12.f}, text);
        r.drawText(*a_.font, "LV " + std::to_string(level_), {560.f, 12.f}, text);
        r.drawText(*a_.font, "TIME " + timeText(), {640.f, 12.f}, text);
        r.drawText(*a_.font, "KILLS " + std::to_string(kills_), {800.f, 12.f}, text);
    }

    void drawCards(meno::Renderer& r) {
        r.setCamera(hudCamera());
        for (int i = 0; i < 3; ++i) {
            const meno::Rectf rect = cardRect(i);
            r.drawRect(rect, {.fill = meno::Color::fromRgb(0x2A2233),
                              .outline = meno::Color::fromRgb(0xFFD54A),
                              .outlineThickness = 2.f});
            drawCentered(r, *a_.font, kCardNames[static_cast<std::size_t>(cards_[i])],
                         {rect.position.x + 120.f, rect.position.y + 70.f},
                         {.characterSize = 16, .color = meno::colors::White});
            r.drawText(*a_.font, std::to_string(i + 1),
                       {rect.position.x + 8.f, rect.position.y + 8.f},
                       {.characterSize = 16, .color = meno::colors::White});
        }
    }
};

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "LAST STAND"}};

    std::vector<std::string> missing;
    const auto texture = [&](const char* path) {
        auto t = game.resources().loadTexture(path);
        if (!t) {
            missing.emplace_back(path);
        }
        return t;
    };
    const auto sound = [&](const char* path) {
        auto b = game.resources().loadSoundBuffer(path);
        if (!b) {
            missing.emplace_back(path);
        }
        return b;
    };

    Assets assets;
    assets.font = game.resources().loadFont("assets/fonts/game.ttf");
    if (!assets.font) {
        missing.emplace_back("assets/fonts/game.ttf");
    }
    assets.player = texture("assets/textures/sv_player.png");
    assets.enemy[0] = texture("assets/textures/sv_walker.png");
    assets.enemy[1] = texture("assets/textures/sv_runner.png");
    assets.enemy[2] = texture("assets/textures/sv_brute.png");
    assets.bullet = texture("assets/textures/sv_bullet.png");
    assets.gem = texture("assets/textures/sv_gem.png");
    assets.shoot = sound("assets/sfx/shoot.wav");
    assets.hit = sound("assets/sfx/hit.wav");
    assets.pickup = sound("assets/sfx/pickup.wav");
    assets.levelup = sound("assets/sfx/levelup.wav");
    assets.bgm = meno::Music::openFromFile("assets/music/bgm_survivor.ogg");
    if (!assets.bgm) {
        missing.emplace_back("assets/music/bgm_survivor.ogg");
    }

    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing asset: " << path << '\n';
        }
        return 1;
    }
    if (assets.bgm) {
        assets.bgm->setLooping(true);
        assets.bgm->setVolume(0.4f);
    }

    Survivor survivor{game, std::move(assets)};
    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double dt) { survivor.update(static_cast<float>(dt)); });
    scene.setDrawHandler([&](meno::Renderer& renderer) { survivor.draw(renderer); });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```