```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

using Vec = meno::Vec2f;

float length(Vec v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

struct SoundPool {
    std::vector<meno::Sound> sounds;
    std::size_t next = 0;

    void initialize(const std::shared_ptr<const meno::SoundBuffer>& buffer,
                    std::size_t count) {
        sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            sounds.emplace_back(buffer);
        }
    }

    void play() {
        if (!sounds.empty()) {
            sounds[next].play();
            next = (next + 1) % sounds.size();
        }
    }

    void reset() {
        for (auto& sound : sounds) {
            sound.stop();
        }
        next = 0;
    }
};

struct Target {
    float x;
    bool alive = true;
    int ammo = 10;
};

struct Missile {
    Vec start;
    Vec position;
    Vec destination;
    int target;
    bool alive = true;
};

struct Interceptor {
    Vec start;
    Vec position;
    Vec destination;
};

struct Explosion {
    Vec center;
    double age = 0.0;

    float radius() const {
        const double value = age <= 0.5 ? age * 96.0 : (1.0 - age) * 96.0;
        return static_cast<float>(std::max(0.0, value));
    }
};

class SkyShield {
public:
    explicit SkyShield(meno::Game& game)
        : game_(game), random_(seed_) {}

    bool loadAssets() {
        auto& resources = game_.resources();
        font_ = resources.loadFont("assets/fonts/game.ttf");
        city_ = resources.loadTexture("assets/textures/md_city.png");
        base_ = resources.loadTexture("assets/textures/md_base.png");
        crosshair_ = resources.loadTexture("assets/textures/md_crosshair.png");

        const auto launch = resources.loadSoundBuffer("assets/sfx/launch.wav");
        const auto explode = resources.loadSoundBuffer("assets/sfx/explode.wav");
        const auto lose = resources.loadSoundBuffer("assets/sfx/lose.wav");
        music_ = meno::Music::openFromFile("assets/music/bgm_missile.ogg");

        bool valid = true;
        const auto require = [&](bool loaded, const char* path) {
            if (!loaded) {
                std::cerr << "Failed to load: " << path << '\n';
                valid = false;
            }
        };

        require(static_cast<bool>(font_), "assets/fonts/game.ttf");
        require(static_cast<bool>(city_), "assets/textures/md_city.png");
        require(static_cast<bool>(base_), "assets/textures/md_base.png");
        require(static_cast<bool>(crosshair_), "assets/textures/md_crosshair.png");
        require(static_cast<bool>(launch), "assets/sfx/launch.wav");
        require(static_cast<bool>(explode), "assets/sfx/explode.wav");
        require(static_cast<bool>(lose), "assets/sfx/lose.wav");
        require(music_.has_value(), "assets/music/bgm_missile.ogg");

        if (!valid) {
            return false;
        }

        launchSounds_.initialize(launch, 12);
        explosionSounds_.initialize(explode, 48);
        loseSounds_.initialize(lose, 12);
        music_->setVolume(0.4f);
        music_->setLooping(true);
        return true;
    }

    void configure() {
        game_.renderer().setCamera(camera_);
        auto& scene = game_.scenes().createScene();
        scene.setUpdateHandler([this](double dt) { update(dt); });
        scene.setDrawHandler([this](meno::Renderer& renderer) { draw(renderer); });
        scene.setExitHandler([this] {
            if (music_) {
                music_->stop();
            }
        });
        (void)game_.scenes().changeScene(scene.id());
    }

private:
    enum class Screen { Title, Play, Result };

    meno::Game& game_;
    std::shared_ptr<const meno::Font> font_;
    std::shared_ptr<const meno::Texture> city_;
    std::shared_ptr<const meno::Texture> base_;
    std::shared_ptr<const meno::Texture> crosshair_;
    std::optional<meno::Music> music_;
    SoundPool launchSounds_;
    SoundPool explosionSounds_;
    SoundPool loseSounds_;

    const meno::Camera2D camera_{
        .center = {480.f, 270.f},
        .size = {960.f, 540.f}
    };

    const unsigned int seed_ = std::random_device{}();
    std::mt19937 random_;

    std::array<Target, 9> targets_{{
        {160.f}, {260.f}, {360.f}, {600.f}, {700.f}, {800.f},
        {60.f}, {480.f}, {900.f}
    }};

    Screen screen_ = Screen::Title;
    double titleTime_ = 0.0;
    std::int64_t score_ = 0;
    int wave_ = 1;
    int spawned_ = 0;
    double spawnTimer_ = 1.0;
    bool clearing_ = false;
    double clearTime_ = 0.0;
    bool dying_ = false;
    Vec mouse_{480.f, 270.f};

    std::vector<Missile> missiles_;
    std::vector<Interceptor> interceptors_;
    std::vector<Explosion> explosions_;

    int citiesLeft() const {
        int count = 0;
        for (int i = 0; i < 6; ++i) {
            if (targets_[i].alive) {
                ++count;
            }
        }
        return count;
    }

    void beginWave() {
        spawned_ = 0;
        spawnTimer_ = 1.0;
        clearing_ = false;
        clearTime_ = 0.0;
        dying_ = false;
        missiles_.clear();
        interceptors_.clear();
        explosions_.clear();

        for (int i = 6; i < 9; ++i) {
            targets_[i].alive = true;
            targets_[i].ammo = 10;
        }
    }

    void restart() {
        launchSounds_.reset();
        explosionSounds_.reset();
        loseSounds_.reset();
        random_.seed(seed_);
        titleTime_ = 0.0;
        score_ = 0;
        wave_ = 1;
        for (auto& target : targets_) {
            target.alive = true;
            target.ammo = 10;
        }
        beginWave();
        screen_ = Screen::Play;
        music_->stop();
        music_->play();
    }

    void showResult() {
        screen_ = Screen::Result;
        music_->stop();
    }

    void addExplosion(Vec center) {
        explosions_.push_back({center, 0.0});
        explosionSounds_.play();
    }

    void ageExplosions(double dt) {
        for (auto& explosion : explosions_) {
            explosion.age += dt;
        }
        std::erase_if(explosions_, [](const Explosion& explosion) {
            return explosion.age >= 1.0;
        });
    }

    void spawnMissile() {
        std::vector<int> available;
        for (int i = 0; i < 9; ++i) {
            if (targets_[i].alive) {
                available.push_back(i);
            }
        }
        if (available.empty()) {
            return;
        }

        std::uniform_int_distribution<int> choose(
            0, static_cast<int>(available.size()) - 1);
        std::uniform_real_distribution<float> chooseX(0.f, 960.f);
        const int target = available[choose(random_)];
        const Vec start{chooseX(random_), 0.f};
        missiles_.push_back({
            start, start, {targets_[target].x, 476.f}, target, true
        });
        ++spawned_;
    }

    void fire() {
        const auto& input = game_.window().input();
        if (!input.wasPressed(meno::MouseButton::Left)) {
            return;
        }

        const Vec pixel{input.mousePosition()};
        if (!meno::viewportPixels(camera_, game_.renderer().framebufferSize())
                 .contains(pixel)) {
            return;
        }
        if (mouse_.y >= 470.f) {
            return;
        }

        int selected = -1;
        float closest = 0.f;
        // Bases are ordered from left to right; strict comparison preserves ties.
        for (int i = 6; i < 9; ++i) {
            const auto& target = targets_[i];
            if (!target.alive || target.ammo <= 0) {
                continue;
            }
            const float distance = std::abs(mouse_.x - target.x);
            if (selected < 0 || distance < closest) {
                selected = i;
                closest = distance;
            }
        }

        if (selected >= 0) {
            --targets_[selected].ammo;
            const Vec start{targets_[selected].x, 476.f};
            interceptors_.push_back({start, start, mouse_});
            launchSounds_.play();
        }
    }

    void updateInterceptors(float dt) {
        std::erase_if(interceptors_, [&](Interceptor& interceptor) {
            const Vec delta = interceptor.destination - interceptor.position;
            const float distance = length(delta);
            const float step = 600.f * dt;
            if (distance <= step) {
                addExplosion(interceptor.destination);
                return true;
            }
            interceptor.position += delta * (step / distance);
            return false;
        });
    }

    void updateMissiles(float dt) {
        const float step = (70.f + 10.f * static_cast<float>(wave_ - 1)) * dt;

        for (auto& missile : missiles_) {
            const Vec delta = missile.destination - missile.position;
            const float distance = length(delta);
            const bool arrived = distance <= step;

            if (arrived) {
                missile.position = missile.destination;
            } else {
                missile.position += delta * (step / distance);
            }

            bool intercepted = false;
            for (const auto& explosion : explosions_) {
                const Vec difference = missile.position - explosion.center;
                const float radius = explosion.radius();
                if (difference.x * difference.x + difference.y * difference.y
                    <= radius * radius) {
                    intercepted = true;
                    break;
                }
            }

            if (intercepted) {
                missile.alive = false;
                score_ += 25;
                addExplosion(missile.position);
            } else if (arrived) {
                missile.alive = false;
                addExplosion(missile.destination);

                auto& target = targets_[missile.target];
                if (target.alive) {
                    target.alive = false;
                    loseSounds_.play();
                }

                if (citiesLeft() == 0) {
                    dying_ = true;
                    break;
                }
            }
        }

        std::erase_if(missiles_, [](const Missile& missile) {
            return !missile.alive;
        });

        if (dying_) {
            // Once every city is lost, only existing explosions remain to finish.
            missiles_.clear();
            interceptors_.clear();
        }
    }

    void update(double dt) {
        const auto& input = game_.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game_.stop();
            return;
        }

        mouse_ = game_.renderer().screenToWorld(Vec{input.mousePosition()});

        if (screen_ == Screen::Title) {
            titleTime_ += dt;
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

        if (clearing_) {
            clearTime_ += dt;
            if (clearTime_ >= 3.0) {
                ++wave_;
                beginWave();
            }
            return;
        }

        ageExplosions(dt);

        if (dying_) {
            if (explosions_.empty()) {
                showResult();
            }
            return;
        }

        fire();

        const int total = 10 + 2 * (wave_ - 1);
        spawnTimer_ -= dt;
        while (spawned_ < total && spawnTimer_ <= 0.0) {
            spawnMissile();
            spawnTimer_ += 1.2;
        }

        updateInterceptors(static_cast<float>(dt));
        updateMissiles(static_cast<float>(dt));

        if (dying_) {
            return;
        }

        if (spawned_ == total && missiles_.empty()
            && interceptors_.empty() && explosions_.empty()) {
            for (int i = 6; i < 9; ++i) {
                if (targets_[i].alive) {
                    score_ += static_cast<std::int64_t>(targets_[i].ammo) * 5;
                }
            }
            score_ += static_cast<std::int64_t>(citiesLeft()) * 100;
            clearing_ = true;
            clearTime_ = 0.0;
        }
    }

    void text(meno::Renderer& renderer, const std::string& value,
              Vec position, unsigned int size, bool centered = false) const {
        meno::TextParams params{
            .characterSize = size,
            .color = meno::colors::White
        };
        if (centered) {
            params.origin = renderer.measureTextBounds(*font_, value, params).center();
        }
        renderer.drawText(*font_, value, position, params);
    }

    void draw(meno::Renderer& renderer) const {
        renderer.setCamera(camera_);
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                          {.fill = meno::colors::Black});

        if (screen_ == Screen::Title) {
            text(renderer, "SKY SHIELD", {480.f, 220.f}, 48, true);
            text(renderer, "PRESS ENTER", {480.f, 300.f}, 32, true);
            return;
        }

        if (screen_ == Screen::Result) {
            text(renderer, "GAME OVER", {480.f, 160.f}, 48, true);
            text(renderer, "SCORE " + std::to_string(score_),
                 {480.f, 240.f}, 32, true);
            text(renderer, "WAVE " + std::to_string(wave_),
                 {480.f, 296.f}, 32, true);
            text(renderer, "R: RESTART  ESC: QUIT",
                 {480.f, 380.f}, 32, true);
            return;
        }

        renderer.drawRect({{0.f, 500.f}, {960.f, 40.f}},
                          {.fill = meno::Color::fromRgb(0xB07A2A)});

        for (int i = 0; i < 9; ++i) {
            const auto& target = targets_[i];
            if (!target.alive) {
                continue;
            }
            renderer.draw(i < 6 ? *city_ : *base_,
                          {.position = {target.x - 24.f, 476.f}});
            if (i >= 6) {
                text(renderer, std::to_string(target.ammo),
                     {target.x, 520.f}, 16, true);
            }
        }

        for (const auto& missile : missiles_) {
            renderer.drawLine(missile.start, missile.position,
                              {.color = meno::Color::fromRgb(0xFF4D4D),
                               .thickness = 2.f});
            renderer.drawRect(
                {missile.position - Vec{2.f, 2.f}, {4.f, 4.f}},
                {.fill = meno::colors::White});
        }

        for (const auto& interceptor : interceptors_) {
            renderer.drawLine(interceptor.start, interceptor.position,
                              {.color = meno::Color::fromRgb(0x4FC3F7),
                               .thickness = 2.f});
        }

        for (const auto& explosion : explosions_) {
            const float radius = explosion.radius();
            if (radius > 0.f) {
                const bool yellow =
                    static_cast<int>(explosion.age * 10.0) % 2 != 0;
                renderer.drawCircle(
                    explosion.center, radius,
                    {.fill = yellow ? meno::Color::fromRgb(0xFFD54A)
                                    : meno::colors::White});
            }
        }

        text(renderer, "SCORE " + std::to_string(score_),
             {16.f, 12.f}, 32);
        text(renderer, "WAVE " + std::to_string(wave_),
             {800.f, 30.f}, 32, true);

        if (clearing_) {
            text(renderer, "WAVE " + std::to_string(wave_) + " CLEAR",
                 {480.f, 250.f}, 48, true);
        }

        renderer.draw(*crosshair_,
                      {.position = mouse_, .origin = {12.f, 12.f}});
    }
};

} // namespace

int main() {
    try {
        meno::Game game{{
            .application = {.framerateLimit = 60},
            .windowSize = {960, 540},
            .title = "SKY SHIELD",
            .clearColor = meno::colors::Black
        }};

        SkyShield shield{game};
        if (!shield.loadAssets()) {
            return 1;
        }
        shield.configure();
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```