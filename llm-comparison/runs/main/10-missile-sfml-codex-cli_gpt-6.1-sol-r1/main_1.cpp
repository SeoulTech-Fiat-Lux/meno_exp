#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr float TargetY = 476.f;
constexpr double Step = 1.0 / 240.0;

const sf::Color GroundColor{0xB0, 0x7A, 0x2A};
const sf::Color EnemyColor{0xFF, 0x4D, 0x4D};
const sf::Color ShotColor{0x4F, 0xC3, 0xF7};
const sf::Color ExplosionColor{0xFF, 0xD5, 0x4A};

float length(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

struct Missile
{
    sf::Vector2f start;
    sf::Vector2f position;
    sf::Vector2f target;
    int targetIndex;
};

struct Interceptor
{
    sf::Vector2f start;
    sf::Vector2f position;
    sf::Vector2f target;
};

struct Explosion
{
    sf::Vector2f position;
    double age = 0.0;

    float radius() const
    {
        const double scale = age <= 0.5 ? age : 1.0 - age;
        return static_cast<float>(96.0 * std::max(0.0, scale));
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
    bool load()
    {
        if (!loadAsset(font, "assets/fonts/game.ttf"))
            return false;
        if (!loadAsset(cityTexture, "assets/textures/md_city.png"))
            return false;
        if (!loadAsset(baseTexture, "assets/textures/md_base.png"))
            return false;
        if (!loadAsset(crosshairTexture, "assets/textures/md_crosshair.png"))
            return false;
        if (!loadAsset(launchBuffer, "assets/sfx/launch.wav"))
            return false;
        if (!loadAsset(explodeBuffer, "assets/sfx/explode.wav"))
            return false;
        if (!loadAsset(loseBuffer, "assets/sfx/lose.wav"))
            return false;

        const std::string path = "assets/music/bgm_missile.ogg";
        if (!music.openFromFile(path))
        {
            std::cerr << "FAILED TO LOAD: " << path << '\n';
            return false;
        }
        music.setVolume(40.f);
        music.setLooping(true);
        return true;
    }

    void run()
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);
        window.setMouseCursorVisible(false);

        sf::View view(sf::FloatRect({0.f, 0.f}, {Width, Height}));
        const auto updateView = [&]
        {
            const auto size = window.getSize();
            if (size.x == 0 || size.y == 0)
                return;

            const float aspect = static_cast<float>(size.x) /
                                 static_cast<float>(size.y);
            constexpr float desired = Width / Height;
            if (aspect > desired)
            {
                const float fraction = desired / aspect;
                view.setViewport(sf::FloatRect(
                    {(1.f - fraction) * 0.5f, 0.f}, {fraction, 1.f}));
            }
            else
            {
                const float fraction = aspect / desired;
                view.setViewport(sf::FloatRect(
                    {0.f, (1.f - fraction) * 0.5f}, {1.f, fraction}));
            }
            window.setView(view);
        };
        updateView();

        sf::Clock clock;
        while (window.isOpen())
        {
            const double elapsed =
                static_cast<double>(clock.restart().asSeconds());

            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                    window.close();

                if (event->is<sf::Event::Resized>())
                    updateView();

                if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                        window.close();
                    else if (screen == Screen::Title &&
                             key->code == sf::Keyboard::Key::Enter)
                        restart();
                    else if (screen == Screen::Result &&
                             key->code == sf::Keyboard::Key::R)
                        restart();
                }

                if (const auto* mouse =
                        event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button == sf::Mouse::Button::Left)
                    {
                        const sf::Vector2f point =
                            window.mapPixelToCoords(mouse->position, view);
                        if (point.x >= 0.f && point.x < Width &&
                            point.y >= 0.f && point.y < 470.f)
                            fire(point);
                    }
                }
            }

            if (!window.isOpen())
                break;

            double remaining = elapsed;
            while (remaining > 0.0)
            {
                const double dt = std::min(remaining, Step);
                update(dt);
                remaining -= dt;
            }

            std::erase_if(sounds, [](const auto& sound)
            {
                return sound->getStatus() == sf::SoundSource::Status::Stopped;
            });

            window.clear(sf::Color::Black);
            window.setView(view);
            draw(window);

            const sf::Vector2f mouse =
                window.mapPixelToCoords(sf::Mouse::getPosition(window), view);
            if (mouse.x >= 0.f && mouse.x < Width &&
                mouse.y >= 0.f && mouse.y < Height)
            {
                sf::Sprite cursor(crosshairTexture);
                cursor.setOrigin({12.f, 12.f});
                cursor.setPosition(mouse);
                window.draw(cursor);
            }

            window.display();
        }
        music.stop();
    }

private:
    template <typename Resource>
    bool loadAsset(Resource& resource, const std::string& path)
    {
        bool success = false;
        if constexpr (std::is_same_v<Resource, sf::Font>)
            success = resource.openFromFile(path);
        else
            success = resource.loadFromFile(path);

        if (!success)
            std::cerr << "FAILED TO LOAD: " << path << '\n';
        return success;
    }

    void play(const sf::SoundBuffer& buffer)
    {
        auto sound = std::make_unique<sf::Sound>(buffer);
        sound->play();
        sounds.push_back(std::move(sound));
    }

    void explode(sf::Vector2f position)
    {
        explosions.push_back({position, 0.0});
        play(explodeBuffer);
    }

    int cityCount() const
    {
        return static_cast<int>(std::count(cities.begin(), cities.end(), true));
    }

    sf::Vector2f targetPosition(int index) const
    {
        if (index < 6)
            return {cityX[static_cast<std::size_t>(index)], TargetY};
        return {baseX[static_cast<std::size_t>(index - 6)], TargetY};
    }

    bool targetAlive(int index) const
    {
        return index < 6 ? cities[static_cast<std::size_t>(index)]
                         : bases[static_cast<std::size_t>(index - 6)];
    }

    void restart()
    {
        music.stop();
        sounds.clear();
        cities.fill(true);
        score = 0;
        wave = 1;
        startWave();
        music.play();
    }

    void startWave()
    {
        missiles.clear();
        interceptors.clear();
        explosions.clear();
        bases.fill(true);
        ammunition.fill(10);
        spawned = 0;
        waveTime = 0.0;
        stateTime = 0.0;
        dying = false;
        screen = Screen::Playing;
    }

    void fire(sf::Vector2f target)
    {
        if (screen != Screen::Playing || dying)
            return;

        int selected = -1;
        float distance = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            if (!bases[i] || ammunition[i] == 0)
                continue;

            const float candidate = std::abs(target.x - baseX[i]);
            if (selected == -1 || candidate < distance)
            {
                selected = i;
                distance = candidate;
            }
        }

        if (selected == -1)
            return;

        --ammunition[selected];
        const sf::Vector2f start{baseX[selected], TargetY};
        interceptors.push_back({start, start, target});
        play(launchBuffer);
    }

    void spawnMissile()
    {
        std::vector<int> targets;
        for (int i = 0; i < 9; ++i)
        {
            if (targetAlive(i))
                targets.push_back(i);
        }
        if (targets.empty())
            return;

        const int index = targets[std::uniform_int_distribution<std::size_t>(
            0, targets.size() - 1)(random)];
        const sf::Vector2f start{
            std::uniform_real_distribution<float>(0.f, Width)(random), 0.f};
        missiles.push_back({start, start, targetPosition(index), index});
        ++spawned;
    }

    static bool advance(sf::Vector2f& position, sf::Vector2f target,
                        float travel)
    {
        const sf::Vector2f difference = target - position;
        const float distance = length(difference);
        if (distance <= travel)
        {
            position = target;
            return true;
        }
        if (distance > 0.f)
            position += difference * (travel / distance);
        return false;
    }

    void update(double dt)
    {
        if (screen == Screen::Title)
        {
            stateTime += dt;
            if (stateTime >= 2.0)
                restart();
            return;
        }

        if (screen == Screen::Result)
            return;

        if (screen == Screen::Clear)
        {
            stateTime += dt;
            if (stateTime >= 3.0)
            {
                ++wave;
                startWave();
            }
            return;
        }

        for (auto& explosion : explosions)
            explosion.age += dt;
        std::erase_if(explosions, [](const Explosion& explosion)
        {
            return explosion.age >= 1.0;
        });

        if (dying)
        {
            if (explosions.empty())
            {
                screen = Screen::Result;
                music.stop();
            }
            return;
        }

        waveTime += dt;
        const int waveMissiles = 10 + 2 * (wave - 1);
        while (spawned < waveMissiles &&
               waveTime >= 1.0 + 1.2 * static_cast<double>(spawned))
            spawnMissile();

        for (std::size_t i = 0; i < interceptors.size();)
        {
            auto& shot = interceptors[i];
            if (advance(shot.position, shot.target,
                        600.f * static_cast<float>(dt)))
            {
                const sf::Vector2f position = shot.target;
                interceptors.erase(interceptors.begin() +
                                   static_cast<std::ptrdiff_t>(i));
                explode(position);
            }
            else
                ++i;
        }

        const float speed = 70.f + 10.f * static_cast<float>(wave - 1);
        for (std::size_t i = 0; i < missiles.size();)
        {
            auto& missile = missiles[i];
            const bool arrived = advance(
                missile.position, missile.target, speed * static_cast<float>(dt));

            bool intercepted = false;
            for (const auto& explosion : explosions)
            {
                const sf::Vector2f delta =
                    missile.position - explosion.position;
                const float radius = explosion.radius();
                if (delta.x * delta.x + delta.y * delta.y <= radius * radius)
                {
                    intercepted = true;
                    break;
                }
            }

            if (!intercepted && !arrived)
            {
                ++i;
                continue;
            }

            const sf::Vector2f position = missile.position;
            const int target = missile.targetIndex;
            missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(i));

            if (intercepted)
                score += 25;
            else if (targetAlive(target))
            {
                if (target < 6)
                    cities[static_cast<std::size_t>(target)] = false;
                else
                    bases[static_cast<std::size_t>(target - 6)] = false;
                play(loseBuffer);
            }
            explode(position);

            if (cityCount() == 0)
            {
                dying = true;
                missiles.clear();
                interceptors.clear();
                return;
            }
        }

        if (spawned == waveMissiles && missiles.empty() &&
            interceptors.empty() && explosions.empty())
        {
            for (std::size_t i = 0; i < bases.size(); ++i)
            {
                if (bases[i])
                    score += ammunition[i] * 5;
            }
            score += cityCount() * 100;
            stateTime = 0.0;
            screen = Screen::Clear;
        }
    }

    void text(sf::RenderTarget& target, const std::string& value,
              unsigned int size, sf::Vector2f position,
              bool centered = false) const
    {
        sf::Text label(font, value, size);
        label.setFillColor(sf::Color::White);
        if (centered)
        {
            const sf::FloatRect bounds = label.getLocalBounds();
            label.setOrigin({
                bounds.position.x + bounds.size.x * 0.5f,
                bounds.position.y + bounds.size.y * 0.5f});
        }
        label.setPosition(position);
        target.draw(label);
    }

    static void line(sf::RenderTarget& target, sf::Vector2f start,
                     sf::Vector2f end, sf::Color color)
    {
        const sf::Vector2f difference = end - start;
        const float distance = length(difference);
        if (distance <= 0.f)
            return;

        sf::RectangleShape shape({distance, 2.f});
        shape.setOrigin({0.f, 1.f});
        shape.setPosition(start);
        shape.setRotation(sf::radians(std::atan2(difference.y, difference.x)));
        shape.setFillColor(color);
        target.draw(shape);
    }

    void draw(sf::RenderTarget& target) const
    {
        if (screen == Screen::Title)
        {
            text(target, "SKY SHIELD", 48, {480.f, 220.f}, true);
            text(target, "PRESS ENTER", 32, {480.f, 310.f}, true);
            return;
        }

        if (screen == Screen::Result)
        {
            text(target, "GAME OVER", 48, {480.f, 160.f}, true);
            text(target, "SCORE " + std::to_string(score),
                 32, {480.f, 240.f}, true);
            text(target, "WAVE " + std::to_string(wave),
                 32, {480.f, 290.f}, true);
            text(target, "R: RESTART  ESC: QUIT",
                 32, {480.f, 380.f}, true);
            return;
        }

        sf::RectangleShape ground({Width, 40.f});
        ground.setPosition({0.f, 500.f});
        ground.setFillColor(GroundColor);
        target.draw(ground);

        for (std::size_t i = 0; i < cities.size(); ++i)
        {
            if (!cities[i])
                continue;
            sf::Sprite city(cityTexture);
            city.setPosition({cityX[i] - 24.f, TargetY});
            target.draw(city);
        }

        for (std::size_t i = 0; i < bases.size(); ++i)
        {
            if (!bases[i])
                continue;
            sf::Sprite base(baseTexture);
            base.setPosition({baseX[i] - 24.f, TargetY});
            target.draw(base);
            text(target, std::to_string(ammunition[i]),
                 16, {baseX[i], 520.f}, true);
        }

        for (const auto& shot : interceptors)
            line(target, shot.start, shot.position, ShotColor);

        for (const auto& missile : missiles)
        {
            line(target, missile.start, missile.position, EnemyColor);
            sf::RectangleShape head({4.f, 4.f});
            head.setOrigin({2.f, 2.f});
            head.setPosition(missile.position);
            head.setFillColor(sf::Color::White);
            target.draw(head);
        }

        for (const auto& explosion : explosions)
        {
            const float radius = explosion.radius();
            if (radius <= 0.f)
                continue;
            sf::CircleShape circle(radius, 64);
            circle.setOrigin({radius, radius});
            circle.setPosition(explosion.position);
            const int phase = static_cast<int>(
                std::floor(explosion.age / 0.1));
            circle.setFillColor(phase % 2 == 0
                                    ? sf::Color::White
                                    : ExplosionColor);
            target.draw(circle);
        }

        text(target, "SCORE " + std::to_string(score),
             32, {16.f, 8.f});
        text(target, "WAVE " + std::to_string(wave),
             32, {740.f, 8.f});

        if (screen == Screen::Clear)
            text(target, "WAVE " + std::to_string(wave) + " CLEAR",
                 48, {480.f, 260.f}, true);
    }

    sf::Font font;
    sf::Texture cityTexture;
    sf::Texture baseTexture;
    sf::Texture crosshairTexture;
    sf::SoundBuffer launchBuffer;
    sf::SoundBuffer explodeBuffer;
    sf::SoundBuffer loseBuffer;
    sf::Music music;
    std::vector<std::unique_ptr<sf::Sound>> sounds;

    const std::array<float, 6> cityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
    const std::array<float, 3> baseX{60.f, 480.f, 900.f};
    std::array<bool, 6> cities{true, true, true, true, true, true};
    std::array<bool, 3> bases{true, true, true};
    std::array<int, 3> ammunition{10, 10, 10};

    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
    std::mt19937 random{std::random_device{}()};

    Screen screen = Screen::Title;
    int score = 0;
    int wave = 1;
    int spawned = 0;
    double stateTime = 0.0;
    double waveTime = 0.0;
    bool dying = false;
};
}

int main()
{
    try
    {
        Game game;
        if (!game.load())
            return 1;
        game.run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
