```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWorldWidth = 960.f;
constexpr float kWorldHeight = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kTargetTopY = 476.f; // 48x24 텍스처의 아랫변이 y 500에 닿도록 하는 윗변
constexpr float kTargetHalfWidth = 24.f;
constexpr float kFireLimitY = 470.f;
constexpr float kInterceptorSpeed = 600.f;
constexpr float kExplosionLife = 1.f;
constexpr float kExplosionMaxRadius = 48.f;
constexpr int kAmmoPerBase = 10;
constexpr int kKillScore = 25;
constexpr int kAmmoScore = 5;
constexpr int kCityScore = 100;
constexpr float kClearDuration = 3.f;
constexpr float kTitleDuration = 2.f;
constexpr float kFirstSpawnDelay = 1.f;
constexpr float kSpawnInterval = 1.2f;

const meno::Color kBlack = meno::Color::fromRgb(0x000000);
const meno::Color kWhite = meno::Color::fromRgb(0xFFFFFF);
const meno::Color kGroundColor = meno::Color::fromRgb(0xB07A2A);
const meno::Color kMissileTrailColor = meno::Color::fromRgb(0xFF4D4D);
const meno::Color kInterceptorColor = meno::Color::fromRgb(0x4FC3F7);
const meno::Color kBlastYellow = meno::Color::fromRgb(0xFFD54A);

struct Assets {
    std::shared_ptr<const meno::Texture> city;
    std::shared_ptr<const meno::Texture> base;
    std::shared_ptr<const meno::Texture> crosshair;
    std::shared_ptr<const meno::Font> font;
    std::shared_ptr<const meno::SoundBuffer> launch;
    std::shared_ptr<const meno::SoundBuffer> explode;
    std::shared_ptr<const meno::SoundBuffer> lose;
};

// 실패한 경로를 표준 에러에 출력하고 false를 돌려줍니다.
bool loadAssets(meno::ResourceManager& res, Assets& a) {
    bool ok = true;
    const auto check = [&ok](const auto& resource, const char* path) {
        if (!resource) {
            std::cerr << "Missing asset: " << path << '\n';
            ok = false;
        }
    };
    a.city = res.loadTexture("assets/textures/md_city.png");
    check(a.city, "assets/textures/md_city.png");
    a.base = res.loadTexture("assets/textures/md_base.png");
    check(a.base, "assets/textures/md_base.png");
    a.crosshair = res.loadTexture("assets/textures/md_crosshair.png");
    check(a.crosshair, "assets/textures/md_crosshair.png");
    a.font = res.loadFont("assets/fonts/game.ttf");
    check(a.font, "assets/fonts/game.ttf");
    a.launch = res.loadSoundBuffer("assets/sfx/launch.wav");
    check(a.launch, "assets/sfx/launch.wav");
    a.explode = res.loadSoundBuffer("assets/sfx/explode.wav");
    check(a.explode, "assets/sfx/explode.wav");
    a.lose = res.loadSoundBuffer("assets/sfx/lose.wav");
    check(a.lose, "assets/sfx/lose.wav");
    return ok;
}

// 같은 효과음을 겹쳐 낼 수 있도록 Sound를 여러 개 두고 차례로 씁니다.
class SoundPool {
public:
    SoundPool(const std::shared_ptr<const meno::SoundBuffer>& buffer, std::size_t count) {
        if (buffer) {
            for (std::size_t i = 0; i < count; ++i) {
                sounds_.emplace_back(buffer);
            }
        }
    }

    void play() {
        if (sounds_.empty()) {
            return;
        }
        sounds_[next_].play();
        next_ = (next_ + 1) % sounds_.size();
    }

private:
    std::vector<meno::Sound> sounds_;
    std::size_t next_ = 0;
};

struct Target {
    meno::Vec2f top{};       // 위쪽 가운데(조준점)
    bool isBase = false;
    bool destroyed = false;
    int ammo = kAmmoPerBase; // 발사대만 씀
};

struct Missile {
    meno::Vec2f start{};
    meno::Vec2f pos{};
    meno::Vec2f aim{};
    float speed = 0.f;
    std::size_t target = 0;
};

struct Interceptor {
    meno::Vec2f from{};
    meno::Vec2f pos{};
    meno::Vec2f aim{};
};

struct Explosion {
    meno::Vec2f center{};
    float age = 0.f;

    // 0.5초 동안 0 → 48px, 다음 0.5초 동안 48 → 0px
    float radius() const {
        if (age < 0.f || age >= kExplosionLife) {
            return 0.f;
        }
        const float half = kExplosionLife / 2.f;
        const float ratio = age < half ? age / half : (kExplosionLife - age) / half;
        return kExplosionMaxRadius * ratio;
    }
};

enum class Stage { Waving, Clearing, Ending };

float vecLength(meno::Vec2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

Target makeTarget(float x, bool isBase) {
    Target t;
    t.top = {x, kTargetTopY};
    t.isBase = isBase;
    return t;
}

class SkyShield {
public:
    SkyShield(meno::Game& game, Assets assets, std::optional<meno::Music> music)
        : game_{game},
          assets_{std::move(assets)},
          music_{std::move(music)},
          launch_{assets_.launch, 4},
          explode_{assets_.explode, 6},
          lose_{assets_.lose, 2},
          random_{std::random_device{}()} {}

    // 씬을 만들고 핸들러를 붙인 뒤 타이틀로 시작합니다.
    void setUp() {
        meno::SceneManager& scenes = game_.scenes();
        title_ = &scenes.createScene();
        play_ = &scenes.createScene();
        result_ = &scenes.createScene();

        title_->setEnterHandler([this] { titleTimer_ = 0.f; });
        title_->setUpdateHandler([this](double dt) { updateTitle(static_cast<float>(dt)); });
        title_->setDrawHandler([this](meno::Renderer& r) { drawTitle(r); });

        play_->setEnterHandler([this] {
            startGame();
            if (music_) {
                music_->play();
            }
        });
        play_->setExitHandler([this] {
            if (music_) {
                music_->stop();
            }
        });
        play_->setUpdateHandler([this](double dt) { updatePlay(static_cast<float>(dt)); });
        play_->setDrawHandler([this](meno::Renderer& r) { drawPlay(r); });

        result_->setUpdateHandler([this](double) { updateResult(); });
        result_->setDrawHandler([this](meno::Renderer& r) { drawResult(r); });

        (void)game_.scenes().changeScene(title_->id());
    }

private:
    meno::Game& game_;
    Assets assets_;
    std::optional<meno::Music> music_;
    SoundPool launch_;
    SoundPool explode_;
    SoundPool lose_;
    std::mt19937 random_;

    meno::Scene* title_ = nullptr;
    meno::Scene* play_ = nullptr;
    meno::Scene* result_ = nullptr;

    std::vector<Target> targets_;
    std::vector<Missile> missiles_;
    std::vector<Interceptor> interceptors_;
    std::vector<Explosion> explosions_;

    Stage stage_ = Stage::Waving;
    int score_ = 0;
    int wave_ = 1;
    float waveTime_ = 0.f;
    int spawned_ = 0;
    int total_ = 0;
    float clearTimer_ = 0.f;
    float titleTimer_ = 0.f;
    meno::Vec2f cursor_{};

    // ---- 게임 상태 ----

    void initTargets() {
        targets_.clear();
        for (const float x : {160.f, 260.f, 360.f, 600.f, 700.f, 800.f}) {
            targets_.push_back(makeTarget(x, false));
        }
        for (const float x : {60.f, 480.f, 900.f}) {
            targets_.push_back(makeTarget(x, true));
        }
    }

    void startGame() {
        score_ = 0;
        wave_ = 1;
        missiles_.clear();
        interceptors_.clear();
        explosions_.clear();
        initTargets();
        startWave();
    }

    void startWave() {
        for (Target& t : targets_) {
            if (t.isBase) {
                t.destroyed = false;
                t.ammo = kAmmoPerBase;
            }
        }
        waveTime_ = 0.f;
        spawned_ = 0;
        total_ = 10 + 2 * (wave_ - 1);
        stage_ = Stage::Waving;
    }

    int aliveCities() const {
        return static_cast<int>(std::count_if(targets_.begin(), targets_.end(), [](const Target& t) {
            return !t.isBase && !t.destroyed;
        }));
    }

    bool idle() const {
        return missiles_.empty() && interceptors_.empty() && explosions_.empty();
    }

    void addExplosion(meno::Vec2f at) {
        explosions_.push_back(Explosion{at});
        explode_.play();
    }

    void fire(meno::Vec2f click) {
        std::size_t best = targets_.size();
        float bestGap = 0.f;
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            const Target& t = targets_[i];
            if (!t.isBase || t.destroyed || t.ammo <= 0) {
                continue;
            }
            const float gap = std::abs(t.top.x - click.x);
            // 같은 거리면 먼저 나온(왼쪽) 발사대를 고릅니다.
            if (best == targets_.size() || gap < bestGap) {
                best = i;
                bestGap = gap;
            }
        }
        if (best == targets_.size()) {
            return;
        }
        Target& base = targets_[best];
        --base.ammo;
        interceptors_.push_back(Interceptor{base.top, base.top, click});
        launch_.play();
    }

    void spawnMissile() {
        std::vector<std::size_t> open;
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            if (!targets_[i].destroyed) {
                open.push_back(i);
            }
        }
        if (open.empty()) {
            return;
        }
        std::uniform_int_distribution<std::size_t> pickTarget(0, open.size() - 1);
        std::uniform_real_distribution<float> pickX(0.f, kWorldWidth);
        const std::size_t index = open[pickTarget(random_)];
        const meno::Vec2f start{pickX(random_), 0.f};
        const float speed = 70.f + 10.f * static_cast<float>(wave_ - 1);
        missiles_.push_back(Missile{start, start, targets_[index].top, speed, index});
    }

    void spawnDue() {
        while (spawned_ < total_ &&
               waveTime_ >= kFirstSpawnDelay + kSpawnInterval * static_cast<float>(spawned_)) {
            spawnMissile();
            ++spawned_;
        }
    }

    void moveInterceptors(float dt) {
        const float step = kInterceptorSpeed * dt;
        for (auto it = interceptors_.begin(); it != interceptors_.end();) {
            const meno::Vec2f delta = it->aim - it->pos;
            const float dist = vecLength(delta);
            if (dist <= step) {
                const meno::Vec2f at = it->aim;
                it = interceptors_.erase(it);
                addExplosion(at);
            } else {
                it->pos += delta * (step / dist);
                ++it;
            }
        }
    }

    void moveMissiles(float dt) {
        for (auto it = missiles_.begin(); it != missiles_.end();) {
            const meno::Vec2f delta = it->aim - it->pos;
            const float dist = vecLength(delta);
            const float step = it->speed * dt;
            if (dist <= step) {
                const meno::Vec2f at = it->aim;
                Target& target = targets_[it->target];
                if (!target.destroyed) {
                    target.destroyed = true;
                    lose_.play();
                }
                it = missiles_.erase(it);
                addExplosion(at);
            } else {
                it->pos += delta * (step / dist);
                ++it;
            }
        }
    }

    void updateExplosions(float dt) {
        for (Explosion& blast : explosions_) {
            blast.age += dt;
        }
        // 폭발 중에 새 폭발이 생길 수 있으므로 인덱스로 끝까지 돕니다.
        for (std::size_t i = 0; i < explosions_.size(); ++i) {
            const Explosion blast = explosions_[i]; // push_back으로 무효화되지 않도록 복사
            const float radius = blast.radius();
            if (radius <= 0.f) {
                continue;
            }
            for (auto it = missiles_.begin(); it != missiles_.end();) {
                const float dx = it->pos.x - blast.center.x;
                const float dy = it->pos.y - blast.center.y;
                if (dx * dx + dy * dy <= radius * radius) {
                    const meno::Vec2f at = it->pos;
                    it = missiles_.erase(it);
                    score_ += kKillScore;
                    addExplosion(at);
                } else {
                    ++it;
                }
            }
        }
        std::erase_if(explosions_, [](const Explosion& e) { return e.age >= kExplosionLife; });
    }

    void settleWave() {
        if (aliveCities() == 0) {
            stage_ = Stage::Ending;
            return;
        }
        int bonus = 0;
        for (const Target& t : targets_) {
            if (t.isBase) {
                if (!t.destroyed) {
                    bonus += t.ammo * kAmmoScore;
                }
            } else if (!t.destroyed) {
                bonus += kCityScore;
            }
        }
        score_ += bonus;
        clearTimer_ = kClearDuration;
        stage_ = Stage::Clearing;
    }

    // ---- 장면 갱신 ----

    void updateTitle(float dt) {
        const meno::Input& in = game_.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }
        titleTimer_ += dt;
        if (in.wasPressed(meno::Key::Enter) || titleTimer_ >= kTitleDuration) {
            (void)game_.scenes().changeScene(play_->id());
        }
    }

    void updatePlay(float dt) {
        const meno::Input& in = game_.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }
        cursor_ = game_.renderer().screenToWorld(meno::Vec2f{in.mousePosition()});

        if (stage_ == Stage::Clearing) {
            clearTimer_ -= dt;
            if (clearTimer_ <= 0.f) {
                ++wave_;
                startWave();
            }
            return;
        }

        if (in.wasPressed(meno::MouseButton::Left) && cursor_.y < kFireLimitY) {
            fire(cursor_);
        }

        if (stage_ == Stage::Waving && aliveCities() == 0) {
            stage_ = Stage::Ending;
        }
        if (stage_ == Stage::Waving) {
            waveTime_ += dt;
            spawnDue();
        }

        moveInterceptors(dt);
        moveMissiles(dt);
        updateExplosions(dt);

        if (stage_ == Stage::Waving && spawned_ >= total_ && idle()) {
            settleWave();
        }
        if (stage_ == Stage::Ending && idle()) {
            (void)game_.scenes().changeScene(result_->id());
        }
    }

    void updateResult() {
        const meno::Input& in = game_.window().input();
        if (in.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }
        if (in.wasPressed(meno::Key::R)) {
            (void)game_.scenes().changeScene(play_->id());
        }
    }

    // ---- 그리기 ----

    void drawCentered(meno::Renderer& r, const std::string& text, meno::Vec2f at, int size) {
        meno::TextParams params{.color = kWhite};
        params.characterSize = static_cast<decltype(params.characterSize)>(size);
        params.origin = r.measureTextBounds(*assets_.font, text, params).center();
        r.drawText(*assets_.font, text, at, params);
    }

    void drawTitle(meno::Renderer& r) {
        drawCentered(r, "SKY SHIELD", {480.f, 200.f}, 48);
        drawCentered(r, "PRESS ENTER", {480.f, 300.f}, 32);
    }

    void drawResult(meno::Renderer& r) {
        drawCentered(r, "GAME OVER", {480.f, 180.f}, 48);
        drawCentered(r, "SCORE " + std::to_string(score_), {480.f, 260.f}, 32);
        drawCentered(r, "WAVE " + std::to_string(wave_), {480.f, 310.f}, 32);
        drawCentered(r, "R: RESTART  ESC: QUIT", {480.f, 400.f}, 16);
    }

    void drawPlay(meno::Renderer& r) {
        r.drawRect({{0.f, kGroundY}, {kWorldWidth, kWorldHeight - kGroundY}},
                   {.fill = kGroundColor});

        for (const Target& t : targets_) {
            if (t.destroyed) {
                continue;
            }
            const auto& texture = t.isBase ? assets_.base : assets_.city;
            r.draw(*texture, {.position = {t.top.x - kTargetHalfWidth, kTargetTopY}});
            if (t.isBase) {
                drawCentered(r, std::to_string(t.ammo), {t.top.x, 520.f}, 16);
            }
        }

        for (const Interceptor& shot : interceptors_) {
            r.drawLine(shot.from, shot.pos, {.color = kInterceptorColor, .thickness = 2.f});
        }
        for (const Missile& m : missiles_) {
            r.drawLine(m.start, m.pos, {.color = kMissileTrailColor, .thickness = 2.f});
        }
        for (const Missile& m : missiles_) {
            r.drawRect({{m.pos.x - 2.f, m.pos.y - 2.f}, {4.f, 4.f}}, {.fill = kWhite});
        }

        for (const Explosion& blast : explosions_) {
            const float radius = blast.radius();
            if (radius <= 0.f) {
                continue;
            }
            const meno::Color fill =
                static_cast<int>(blast.age / 0.1f) % 2 == 0 ? kWhite : kBlastYellow;
            r.drawCircle(blast.center, radius, {.fill = fill});
        }

        r.draw(*assets_.crosshair, {.position = cursor_ - meno::Vec2f{12.f, 12.f}});

        drawCentered(r, "SCORE " + std::to_string(score_), {120.f, 24.f}, 32);
        drawCentered(r, "WAVE " + std::to_string(wave_), {840.f, 24.f}, 32);
        if (stage_ == Stage::Clearing) {
            drawCentered(r, "WAVE " + std::to_string(wave_) + " CLEAR", {480.f, 260.f}, 48);
        }
    }
};

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SKY SHIELD",
                     .clearColor = kBlack}};
    game.renderer().setCamera(meno::Camera2D{.center = {480.f, 270.f}, .size = {960.f, 540.f}});

    Assets assets;
    bool ok = loadAssets(game.resources(), assets);

    std::optional<meno::Music> music = meno::Music::openFromFile("assets/music/bgm_missile.ogg");
    if (!music) {
        std::cerr << "Missing asset: assets/music/bgm_missile.ogg\n";
        ok = false;
    }
    if (!ok) {
        return 1;
    }
    music->setLooping(true);
    music->setVolume(0.4f);

    SkyShield shield{game, std::move(assets), std::move(music)};
    shield.setUp();
    game.run();
    return 0;
}
```