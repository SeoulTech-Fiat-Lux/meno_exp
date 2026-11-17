```cpp
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float Width = 960.f;
constexpr float Height = 540.f;
constexpr double Step = 1.0 / 240.0;

const sf::Color EnemyColor{255, 77, 77};
const sf::Color ShotColor{79, 195, 247};
const sf::Color GroundColor{176, 122, 42};
const sf::Color Yellow{255, 213, 74};

float length(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

struct Assets
{
    sf::Font font;
    sf::Texture city;
    sf::Texture base;
    sf::Texture crosshair;
    sf::SoundBuffer launch;
    sf::SoundBuffer explode;
    sf::SoundBuffer lose;
    sf::Music music;

    bool load()
    {
        const auto checked = [](bool success, const char* path)
        {
            if (!success)
                std::cerr << "FAILED TO LOAD: " << path << '\n';
            return success;
        };

        if (!checked(font.openFromFile("assets/fonts/game.ttf"),
                     "assets/fonts/game.ttf"))
            return false;
        if (!checked(city.loadFromFile("assets/textures/md_city.png"),
                     "assets/textures/md_city.png"))
            return false;
        if (!checked(base.loadFromFile("assets/textures/md_base.png"),
                     "assets/textures/md_base.png"))
            return false;
        if (!checked(crosshair.loadFromFile("assets/textures/md_crosshair.png"),
                     "assets/textures/md_crosshair.png"))
            return false;
        if (!checked(launch.loadFromFile("assets/sfx/launch.wav"),
                     "assets/sfx/launch.wav"))
            return false;
        if (!checked(explode.loadFromFile("assets/sfx/explode.wav"),
                     "assets/sfx/explode.wav"))
            return false;
        if (!checked(lose.loadFromFile("assets/sfx/lose.wav"),
                     "assets/sfx/lose.wav"))
            return false;
        if (!checked(music.openFromFile("assets/music/bgm_missile.ogg"),
                     "assets/music/bgm_missile.ogg"))
            return false;

        music.setVolume(40.f);
        music.setLooping(true);
        return true;
    }
};

struct Target
{
    sf::Vector2f top;
    bool alive = true;
    int ammunition = 10;
};

struct Projectile
{
    sf::Vector2f start;
    sf::Vector2f position;
    sf::Vector2f destination;
    int target = -1;
    bool dead = false;
};

struct Explosion
{
    sf::Vector2f position;
    double age = 0.0;

    float radius() const
    {
        return static_cast<float>(
            96.0 * std::max(0.0, std::min(age, 1.0 - age)));
    }
};

enum class Screen
{
    Title,
    Playing,
    Clear,
    Result
};

class Game
{
public:
    Game(sf::RenderWindow& window, Assets& assets)
        : window_(window), assets_(assets), random_(std::random_device{}())
    {
        resetTargets();
        updateViewport(window_.getSize());
    }

    void updateViewport(sf::Vector2u size)
    {
        if (size.x == 0 || size.y == 0)
            return;

        const float aspect = static_cast<float>(size.x) /
                             static_cast<float>(size.y);
        constexpr float desired = Width / Height;

        sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});
        if (aspect > desired)
        {
            viewport.size.x = desired / aspect;
            viewport.position.x = (1.f - viewport.size.x) * 0.5f;
        }
        else
        {
            viewport.size.y = aspect / desired;
            viewport.position.y = (1.f - viewport.size.y) * 0.5f;
        }

        view_.setViewport(viewport);
        window_.setView(view_);
    }

    void handleKey(sf::Keyboard::Key key)
    {
        if (key == sf::Keyboard::Key::Escape)
            window_.close();
        else if (screen_ == Screen::Title &&
                 key == sf::Keyboard::Key::Enter)
            restart();
        else if (screen_ == Screen::Result &&
                 key == sf::Keyboard::Key::R)
            restart();
    }

    void fire(sf::Vector2i pixel)
    {
        if (screen_ != Screen::Playing || dying_)
            return;

        const sf::Vector2f destination =
            window_.mapPixelToCoords(pixel, view_);

        if (destination.x < 0.f || destination.x >= Width ||
            destination.y < 0.f || destination.y >= 470.f)
            return;

        int selected = -1;
        float nearest = Width + 1.f;

        // Bases are stored from left to right; strict comparison breaks ties left.
        for (int i = 6; i < 9; ++i)
        {
            const auto& base = targets_[static_cast<std::size_t>(i)];
            if (!base.alive || base.ammunition == 0)
                continue;

            const float distance = std::abs(base.top.x - destination.x);
            if (distance < nearest)
            {
                nearest = distance;
                selected = i;
            }
        }

        if (selected < 0)
            return;

        auto& base = targets_[static_cast<std::size_t>(selected)];
        --base.ammunition;
        shots_.push_back({base.top, base.top, destination});
        play(assets_.launch);
    }

    void cleanSounds()
    {
        std::erase_if(sounds_, [](const auto& sound)
        {
            return sound->getStatus() == sf::SoundSource::Status::Stopped;
        });
    }

    void update(double dt)
    {
        if (screen_ == Screen::Title)
        {
            screenTime_ += dt;
            if (screenTime_ >= 2.0)
                restart();
            return;
        }

        if (screen_ == Screen::Result)
            return;

        if (screen_ == Screen::Clear)
        {
            screenTime_ += dt;
            if (screenTime_ >= 3.0)
            {
                ++wave_;
                beginWave();
            }
            return;
        }

        for (auto& explosion : explosions_)
            explosion.age += dt;

        std::erase_if(explosions_, [](const Explosion& explosion)
        {
            return explosion.age >= 1.0;
        });

        if (dying_)
        {
            if (explosions_.empty())
                showResult();
            return;
        }

        waveTime_ += dt;
        const int missileCount = 10 + 2 * (wave_ - 1);
        while (spawned_ < missileCount &&
               waveTime_ >= 1.0 + 1.2 * static_cast<double>(spawned_))
        {
            spawnMissile();
            ++spawned_;
        }

        for (auto& shot : shots_)
        {
            if (advance(shot, 600.f, dt))
            {
                shot.dead = true;
                addExplosion(shot.destination);
            }
        }
        std::erase_if(shots_, [](const Projectile& shot) { return shot.dead; });

        const float enemySpeed = 70.f + 10.f * static_cast<float>(wave_ - 1);
        for (auto& missile : missiles_)
            advance(missile, enemySpeed, dt);

        // Resolve blast hits before impacts. Marking missiles immediately prevents
        // a missile from ever awarding points more than once.
        resolveBlastHits();

        for (auto& missile : missiles_)
        {
            if (missile.dead ||
                length(missile.destination - missile.position) > 0.001f)
                continue;

            missile.dead = true;
            addExplosion(missile.destination);

            auto& target = targets_[static_cast<std::size_t>(missile.target)];
            if (target.alive)
            {
                target.alive = false;
                play(assets_.lose);
            }
        }

        // Impact explosions participate in the same chain-explosion rules.
        resolveBlastHits();
        std::erase_if(missiles_, [](const Projectile& missile)
        {
            return missile.dead;
        });

        if (livingCities() == 0)
        {
            dying_ = true;
            missiles_.clear();
            shots_.clear();
            if (explosions_.empty())
                showResult();
            return;
        }

        if (spawned_ == missileCount && missiles_.empty() &&
            shots_.empty() && explosions_.empty())
        {
            score_ += static_cast<std::int64_t>(livingCities()) * 100;
            for (std::size_t i = 6; i < targets_.size(); ++i)
            {
                if (targets_[i].alive)
                    score_ += targets_[i].ammunition * 5;
            }

            screen_ = Screen::Clear;
            screenTime_ = 0.0;
        }
    }

    void draw()
    {
        window_.clear(sf::Color::Black);
        window_.setView(view_);

        if (screen_ == Screen::Title)
        {
            text("SKY SHIELD", 48, {Width / 2.f, 190.f}, true);
            text("PRESS ENTER", 32, {Width / 2.f, 290.f}, true);
        }
        else if (screen_ == Screen::Result)
        {
            text("GAME OVER", 48, {Width / 2.f, 130.f}, true);
            text("SCORE " + std::to_string(score_), 32,
                 {Width / 2.f, 220.f}, true);
            text("WAVE " + std::to_string(wave_), 32,
                 {Width / 2.f, 275.f}, true);
            text("R: RESTART  ESC: QUIT", 16,
                 {Width / 2.f, 365.f}, true);
        }
        else
        {
            drawWorld();

            text("SCORE " + std::to_string(score_), 16, {16.f, 12.f});
            text("WAVE " + std::to_string(wave_), 16, {800.f, 12.f});

            if (screen_ == Screen::Clear)
            {
                text("WAVE " + std::to_string(wave_) + " CLEAR",
                     32, {Width / 2.f, 225.f}, true);
            }

            sf::Sprite crosshair(assets_.crosshair);
            crosshair.setOrigin({12.f, 12.f});
            crosshair.setPosition(window_.mapPixelToCoords(
                sf::Mouse::getPosition(window_), view_));
            window_.draw(crosshair);
        }

        window_.display();
    }

private:
    void resetTargets()
    {
        constexpr std::array<float, 9> positions{
            160.f, 260.f, 360.f, 600.f, 700.f, 800.f,
            60.f, 480.f, 900.f
        };

        for (std::size_t i = 0; i < targets_.size(); ++i)
            targets_[i] = {{positions[i], 476.f}, true, 10};
    }

    void restart()
    {
        for (auto& sound : sounds_)
            sound->stop();
        sounds_.clear();

        assets_.music.stop();
        resetTargets();
        missiles_.clear();
        shots_.clear();
        explosions_.clear();
        score_ = 0;
        wave_ = 1;
        dying_ = false;
        beginWave();
        assets_.music.play();
    }

    void beginWave()
    {
        screen_ = Screen::Playing;
        screenTime_ = 0.0;
        waveTime_ = 0.0;
        spawned_ = 0;

        for (std::size_t i = 6; i < targets_.size(); ++i)
        {
            targets_[i].alive = true;
            targets_[i].ammunition = 10;
        }
    }

    void showResult()
    {
        screen_ = Screen::Result;
        screenTime_ = 0.0;
        assets_.music.stop();
    }

    int livingCities() const
    {
        int count = 0;
        for (std::size_t i = 0; i < 6; ++i)
            count += targets_[i].alive ? 1 : 0;
        return count;
    }

    void spawnMissile()
    {
        std::vector<int> available;
        for (int i = 0; i < 9; ++i)
        {
            if (targets_[static_cast<std::size_t>(i)].alive)
                available.push_back(i);
        }
        if (available.empty())
            return;

        const auto index = std::uniform_int_distribution<std::size_t>(
            0, available.size() - 1)(random_);
        const int target = available[index];
        const sf::Vector2f start{
            std::uniform_real_distribution<float>(0.f, Width)(random_), 0.f
        };
        missiles_.push_back({
            start, start, targets_[static_cast<std::size_t>(target)].top,
            target, false
        });
    }

    static bool advance(Projectile& projectile, float speed, double dt)
    {
        const sf::Vector2f delta = projectile.destination - projectile.position;
        const float distance = length(delta);
        const float travel = speed * static_cast<float>(dt);

        if (distance <= travel)
        {
            projectile.position = projectile.destination;
            return true;
        }

        projectile.position += delta * (travel / distance);
        return false;
    }

    void play(const sf::SoundBuffer& buffer)
    {
        auto sound = std::make_unique<sf::Sound>(buffer);
        sound->play();
        sounds_.push_back(std::move(sound));
    }

    void addExplosion(sf::Vector2f position)
    {
        explosions_.push_back({position, 0.0});
        play(assets_.explode);
    }

    void resolveBlastHits()
    {
        bool changed;
        do
        {
            changed = false;
            for (auto& missile : missiles_)
            {
                if (missile.dead)
                    continue;

                bool hit = false;
                for (const auto& explosion : explosions_)
                {
                    const sf::Vector2f delta =
                        missile.position - explosion.position;
                    const float radius = explosion.radius();
                    if (delta.x * delta.x + delta.y * delta.y <= radius * radius)
                    {
                        hit = true;
                        break;
                    }
                }

                // Append only after leaving the explosion iteration.
                if (hit)
                {
                    missile.dead = true;
                    score_ += 25;
                    addExplosion(missile.position);
                    changed = true;
                }
            }
        } while (changed);
    }

    void text(const std::string& string, unsigned int size,
              sf::Vector2f position, bool centered = false)
    {
        sf::Text label(assets_.font, string, size);
        label.setFillColor(sf::Color::White);
        if (centered)
        {
            const auto bounds = label.getLocalBounds();
            label.setOrigin({
                bounds.position.x + bounds.size.x * 0.5f,
                bounds.position.y + bounds.size.y * 0.5f
            });
        }
        label.setPosition(position);
        window_.draw(label);
    }

    void line(sf::Vector2f from, sf::Vector2f to, sf::Color color)
    {
        const sf::Vector2f delta = to - from;
        const float distance = length(delta);
        if (distance <= 0.f)
            return;

        sf::RectangleShape shape({distance, 2.f});
        shape.setOrigin({0.f, 1.f});
        shape.setPosition(from);
        shape.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
        shape.setFillColor(color);
        window_.draw(shape);
    }

    void drawWorld()
    {
        sf::RectangleShape ground({Width, 40.f});
        ground.setPosition({0.f, 500.f});
        ground.setFillColor(GroundColor);
        window_.draw(ground);

        for (std::size_t i = 0; i < targets_.size(); ++i)
        {
            const auto& target = targets_[i];
            if (!target.alive)
                continue;

            sf::Sprite sprite(i < 6 ? assets_.city : assets_.base);
            sprite.setPosition({target.top.x - 24.f, target.top.y});
            window_.draw(sprite);

            if (i >= 6)
                text(std::to_string(target.ammunition), 16,
                     {target.top.x, 520.f}, true);
        }

        for (const auto& shot : shots_)
            line(shot.start, shot.position, ShotColor);

        for (const auto& missile : missiles_)
        {
            line(missile.start, missile.position, EnemyColor);

            sf::RectangleShape head({4.f, 4.f});
            head.setOrigin({2.f, 2.f});
            head.setPosition(missile.position);
            head.setFillColor(sf::Color::White);
            window_.draw(head);
        }

        for (const auto& explosion : explosions_)
        {
            const float radius = explosion.radius();
            if (radius <= 0.f)
                continue;

            sf::CircleShape circle(radius, 64);
            circle.setOrigin({radius, radius});
            circle.setPosition(explosion.position);
            const auto phase = static_cast<int>(
                std::floor((explosion.age + 1e-9) / 0.1));
            circle.setFillColor(phase % 2 == 0 ? sf::Color::White : Yellow);
            window_.draw(circle);
        }
    }

    sf::RenderWindow& window_;
    Assets& assets_;
    sf::View view_{sf::FloatRect({0.f, 0.f}, {Width, Height})};
    std::mt19937 random_;

    Screen screen_ = Screen::Title;
    double screenTime_ = 0.0;
    double waveTime_ = 0.0;
    int wave_ = 1;
    int spawned_ = 0;
    std::int64_t score_ = 0;
    bool dying_ = false;

    std::array<Target, 9> targets_;
    std::vector<Projectile> missiles_;
    std::vector<Projectile> shots_;
    std::vector<Explosion> explosions_;
    std::vector<std::unique_ptr<sf::Sound>> sounds_;
};
} // namespace

int main()
{
    try
    {
        sf::RenderWindow window(
            sf::VideoMode({960u, 540u}), "SKY SHIELD");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);
        window.setMouseCursorVisible(false);

        Assets assets;
        if (!assets.load())
            return 1;

        Game game(window, assets);
        sf::Clock clock;
        double accumulator = 0.0;

        while (window.isOpen())
        {
            accumulator += static_cast<double>(clock.restart().asSeconds());

            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                    window.close();
                else if (const auto* resized =
                             event->getIf<sf::Event::Resized>())
                    game.updateViewport(resized->size);
                else if (const auto* key =
                             event->getIf<sf::Event::KeyPressed>())
                    game.handleKey(key->code);
                else if (const auto* button =
                             event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (button->button == sf::Mouse::Button::Left)
                        game.fire(button->position);
                }
            }

            if (!window.isOpen())
                break;

            while (accumulator >= Step)
            {
                game.update(Step);
                accumulator -= Step;
            }

            game.cleanSounds();
            game.draw();
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```