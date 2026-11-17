#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <list>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float Width = 960.f;
constexpr float Height = 540.f;
constexpr double Step = 1.0 / 240.0;

constexpr std::array<float, 6> CityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr std::array<float, 3> BaseX{60.f, 480.f, 900.f};

const sf::Color GroundColor{0xB0, 0x7A, 0x2A};
const sf::Color EnemyColor{0xFF, 0x4D, 0x4D};
const sf::Color InterceptorColor{0x4F, 0xC3, 0xF7};
const sf::Color ExplosionColor{0xFF, 0xD5, 0x4A};

float length(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

struct Resources
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

struct Missile
{
    sf::Vector2f start;
    sf::Vector2f position;
    sf::Vector2f target;
    int targetIndex{};
    float speed{};
    bool dead{};
};

struct Interceptor
{
    sf::Vector2f start;
    sf::Vector2f position;
    sf::Vector2f target;
    bool dead{};
};

struct Explosion
{
    sf::Vector2f position;
    double age{};

    float radius() const
    {
        const double factor = age <= 0.5 ? age : 1.0 - age;
        return static_cast<float>(std::max(0.0, factor) * 96.0);
    }
};

enum class Screen
{
    Title,
    Play,
    Result
};

class Game
{
public:
    explicit Game(Resources& resources) : assets(resources)
    {
    }

    Screen screen{Screen::Title};

    void start()
    {
        score = 0;
        wave = 1;
        titleTime = 0.0;
        cities.fill(true);
        missiles.clear();
        interceptors.clear();
        explosions.clear();
        sounds.clear();
        ending = false;
        clearing = false;
        clearTime = 0.0;
        screen = Screen::Play;
        beginWave();
        assets.music.stop();
        assets.music.play();
    }

    void fire(sf::Vector2f point)
    {
        if (screen != Screen::Play || ending || clearing ||
            point.x < 0.f || point.x >= Width ||
            point.y < 0.f || point.y >= 470.f)
            return;

        int selected = -1;
        float nearest = Width + 1.f;

        for (int i = 0; i < 3; ++i)
        {
            if (!bases[i] || ammunition[i] == 0)
                continue;

            const float distance = std::abs(point.x - BaseX[i]);
            if (distance < nearest)
            {
                selected = i;
                nearest = distance;
            }
        }

        if (selected < 0)
            return;

        --ammunition[selected];
        const sf::Vector2f origin{BaseX[selected], 476.f};
        interceptors.push_back({origin, origin, point, false});
        playSound(assets.launch);
    }

    void update(double dt)
    {
        if (screen == Screen::Title)
        {
            titleTime += dt;
            if (titleTime >= 2.0)
                start();
            return;
        }

        if (screen != Screen::Play)
            return;

        if (clearing)
        {
            clearTime += dt;
            if (clearTime >= 3.0)
            {
                ++wave;
                beginWave();
            }
            return;
        }

        for (auto& explosion : explosions)
            explosion.age += dt;

        std::erase_if(explosions, [](const Explosion& explosion)
        {
            return explosion.age >= 1.0;
        });

        if (ending)
        {
            if (explosions.empty())
            {
                screen = Screen::Result;
                assets.music.stop();
            }
            return;
        }

        waveTime += dt;
        while (spawned < missileCount && waveTime + 1e-9 >= nextSpawn)
        {
            spawnMissile();
            ++spawned;
            nextSpawn += 1.2;
        }

        const float seconds = static_cast<float>(dt);

        for (auto& interceptor : interceptors)
        {
            if (advance(interceptor.position, interceptor.target, 600.f * seconds))
            {
                interceptor.dead = true;
                addExplosion(interceptor.target);
            }
        }

        for (auto& missile : missiles)
        {
            const bool arrived = advance(
                missile.position, missile.target, missile.speed * seconds);

            bool intercepted = false;
            for (const auto& explosion : explosions)
            {
                const sf::Vector2f delta = missile.position - explosion.position;
                const float radius = explosion.radius();
                if (delta.x * delta.x + delta.y * delta.y <= radius * radius)
                {
                    intercepted = true;
                    break;
                }
            }

            if (intercepted)
            {
                missile.dead = true;
                score += 25;
                addExplosion(missile.position);
            }
            else if (arrived)
            {
                missile.dead = true;
                addExplosion(missile.target);

                if (missile.targetIndex < 6)
                {
                    if (cities[missile.targetIndex])
                    {
                        cities[missile.targetIndex] = false;
                        playSound(assets.lose);
                    }
                }
                else
                {
                    const int index = missile.targetIndex - 6;
                    if (bases[index])
                    {
                        bases[index] = false;
                        playSound(assets.lose);
                    }
                }

                if (std::none_of(cities.begin(), cities.end(),
                                 [](bool alive) { return alive; }))
                {
                    ending = true;
                    break;
                }
            }
        }

        std::erase_if(missiles, [](const Missile& m) { return m.dead; });
        std::erase_if(interceptors, [](const Interceptor& i) { return i.dead; });

        if (ending)
        {
            // 도시가 모두 사라지면 남은 폭발의 종료만 기다린다.
            missiles.clear();
            interceptors.clear();
            return;
        }

        if (spawned == missileCount && missiles.empty() &&
            interceptors.empty() && explosions.empty())
        {
            for (int i = 0; i < 3; ++i)
                if (bases[i])
                    score += ammunition[i] * 5;

            for (bool alive : cities)
                if (alive)
                    score += 100;

            clearing = true;
            clearTime = 0.0;
        }
    }

    void cleanSounds()
    {
        sounds.remove_if([](const sf::Sound& sound)
        {
            return sound.getStatus() == sf::SoundSource::Status::Stopped;
        });
    }

    void draw(sf::RenderTarget& target, sf::Vector2f mouse, bool mouseInside)
    {
        if (screen == Screen::Title)
        {
            text(target, "SKY SHIELD", {480.f, 190.f}, 48, true);
            text(target, "PRESS ENTER", {480.f, 290.f}, 32, true);
            return;
        }

        if (screen == Screen::Result)
        {
            text(target, "GAME OVER", {480.f, 135.f}, 48, true);
            text(target, "SCORE " + std::to_string(score),
                 {480.f, 225.f}, 32, true);
            text(target, "WAVE " + std::to_string(wave),
                 {480.f, 280.f}, 32, true);
            text(target, "R: RESTART  ESC: QUIT",
                 {480.f, 375.f}, 32, true);
            return;
        }

        sf::RectangleShape ground({Width, 40.f});
        ground.setPosition({0.f, 500.f});
        ground.setFillColor(GroundColor);
        target.draw(ground);

        for (int i = 0; i < 6; ++i)
        {
            if (!cities[i])
                continue;
            sf::Sprite sprite(assets.city);
            sprite.setPosition({CityX[i] - 24.f, 476.f});
            target.draw(sprite);
        }

        for (int i = 0; i < 3; ++i)
        {
            if (!bases[i])
                continue;
            sf::Sprite sprite(assets.base);
            sprite.setPosition({BaseX[i] - 24.f, 476.f});
            target.draw(sprite);
            text(target, std::to_string(ammunition[i]),
                 {BaseX[i], 508.f}, 16, true);
        }

        for (const auto& missile : missiles)
        {
            line(target, missile.start, missile.position, EnemyColor);
            sf::RectangleShape head({4.f, 4.f});
            head.setOrigin({2.f, 2.f});
            head.setPosition(missile.position);
            head.setFillColor(sf::Color::White);
            target.draw(head);
        }

        for (const auto& interceptor : interceptors)
            line(target, interceptor.start, interceptor.position, InterceptorColor);

        for (const auto& explosion : explosions)
        {
            const float radius = explosion.radius();
            if (radius <= 0.f)
                continue;
            sf::CircleShape circle(radius, 64);
            circle.setOrigin({radius, radius});
            circle.setPosition(explosion.position);
            const int phase = static_cast<int>(
                std::floor((explosion.age + 1e-9) / 0.1));
            circle.setFillColor(phase % 2 == 0 ?
                                sf::Color::White : ExplosionColor);
            target.draw(circle);
        }

        text(target, "SCORE " + std::to_string(score), {16.f, 12.f}, 32);
        text(target, "WAVE " + std::to_string(wave), {740.f, 12.f}, 32);

        if (clearing)
        {
            text(target, "WAVE " + std::to_string(wave) + " CLEAR",
                 {480.f, 220.f}, 48, true);
        }

        if (mouseInside)
        {
            sf::Sprite crosshair(assets.crosshair);
            crosshair.setOrigin({12.f, 12.f});
            crosshair.setPosition(mouse);
            target.draw(crosshair);
        }
    }

private:
    Resources& assets;
    std::mt19937 random{std::random_device{}()};
    std::array<bool, 6> cities{};
    std::array<bool, 3> bases{};
    std::array<int, 3> ammunition{};
    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
    std::list<sf::Sound> sounds;

    std::int64_t score{};
    int wave{1};
    int missileCount{10};
    int spawned{};
    double titleTime{};
    double waveTime{};
    double nextSpawn{1.0};
    double clearTime{};
    bool clearing{};
    bool ending{};

    void beginWave()
    {
        bases.fill(true);
        ammunition.fill(10);
        missiles.clear();
        interceptors.clear();
        explosions.clear();
        missileCount = 10 + 2 * (wave - 1);
        spawned = 0;
        waveTime = 0.0;
        nextSpawn = 1.0;
        clearing = false;
        clearTime = 0.0;
        ending = false;
    }

    void spawnMissile()
    {
        std::vector<int> targets;
        for (int i = 0; i < 6; ++i)
            if (cities[i])
                targets.push_back(i);
        for (int i = 0; i < 3; ++i)
            if (bases[i])
                targets.push_back(i + 6);

        if (targets.empty())
            return;

        const int targetIndex = targets[
            std::uniform_int_distribution<std::size_t>(
                0, targets.size() - 1)(random)];

        const sf::Vector2f start{
            std::uniform_real_distribution<float>(0.f, Width)(random), 0.f};
        const sf::Vector2f destination{
            targetIndex < 6 ? CityX[targetIndex] : BaseX[targetIndex - 6],
            476.f};

        missiles.push_back({
            start, start, destination, targetIndex,
            70.f + 10.f * static_cast<float>(wave - 1), false
        });
    }

    static bool advance(sf::Vector2f& position, sf::Vector2f destination,
                        float distance)
    {
        const sf::Vector2f delta = destination - position;
        const float remaining = length(delta);
        if (remaining <= distance)
        {
            position = destination;
            return true;
        }
        position += delta * (distance / remaining);
        return false;
    }

    void playSound(const sf::SoundBuffer& buffer)
    {
        sounds.emplace_back(buffer);
        sounds.back().play();
    }

    void addExplosion(sf::Vector2f position)
    {
        explosions.push_back({position, 0.0});
        playSound(assets.explode);
    }

    static void line(sf::RenderTarget& target, sf::Vector2f start,
                     sf::Vector2f end, sf::Color color)
    {
        const sf::Vector2f delta = end - start;
        const float distance = length(delta);
        if (distance <= 0.f)
            return;

        sf::RectangleShape shape({distance, 2.f});
        shape.setOrigin({0.f, 1.f});
        shape.setPosition(start);
        shape.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
        shape.setFillColor(color);
        target.draw(shape);
    }

    void text(sf::RenderTarget& target, const std::string& value,
              sf::Vector2f position, unsigned int size, bool centered = false)
    {
        sf::Text label(assets.font, value, size);
        label.setFillColor(sf::Color::White);
        const sf::FloatRect bounds = label.getLocalBounds();
        label.setOrigin({
            bounds.position.x + (centered ? bounds.size.x * 0.5f : 0.f),
            bounds.position.y
        });
        label.setPosition(position);
        target.draw(label);
    }
};

void updateViewport(sf::View& view, sf::Vector2u size)
{
    if (size.x == 0 || size.y == 0)
        return;

    const float aspect = static_cast<float>(size.x) /
                         static_cast<float>(size.y);
    constexpr float desired = Width / Height;

    if (aspect > desired)
    {
        const float width = desired / aspect;
        view.setViewport(sf::FloatRect(
            {(1.f - width) * 0.5f, 0.f}, {width, 1.f}));
    }
    else
    {
        const float height = aspect / desired;
        view.setViewport(sf::FloatRect(
            {0.f, (1.f - height) * 0.5f}, {1.f, height}));
    }
}
} // namespace

int main()
{
    try
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Resources assets;
        if (!assets.load())
            return 1;

        sf::View view(sf::FloatRect({0.f, 0.f}, {Width, Height}));
        updateViewport(view, window.getSize());
        window.setView(view);

        Game game(assets);
        sf::Clock clock;
        double accumulator = 0.0;

        while (window.isOpen())
        {
            const double elapsed =
                static_cast<double>(clock.restart().asMicroseconds()) / 1000000.0;

            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                }
                else if (const auto* resized = event->getIf<sf::Event::Resized>())
                {
                    updateViewport(view, resized->size);
                    window.setView(view);
                }
                else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        window.close();
                    }
                    else if (key->code == sf::Keyboard::Key::Enter &&
                             game.screen == Screen::Title)
                    {
                        game.start();
                        accumulator = 0.0;
                    }
                    else if (key->code == sf::Keyboard::Key::R &&
                             game.screen == Screen::Result)
                    {
                        game.start();
                        accumulator = 0.0;
                    }
                }
                else if (const auto* mouse =
                             event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button == sf::Mouse::Button::Left &&
                        window.getSize().x > 0 && window.getSize().y > 0)
                    {
                        game.fire(window.mapPixelToCoords(mouse->position, view));
                    }
                }
            }

            if (!window.isOpen())
                break;

            accumulator += elapsed;
            while (accumulator >= Step)
            {
                game.update(Step);
                accumulator -= Step;
            }

            game.cleanSounds();

            const bool validSize =
                window.getSize().x > 0 && window.getSize().y > 0;
            sf::Vector2f mouse{-1.f, -1.f};
            if (validSize)
                mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), view);

            const bool mouseInside = validSize && window.hasFocus() &&
                mouse.x >= 0.f && mouse.x < Width &&
                mouse.y >= 0.f && mouse.y < Height;

            window.setMouseCursorVisible(
                game.screen != Screen::Play || !mouseInside);

            window.clear(sf::Color::Black);
            window.setView(view);
            game.draw(window, mouse, mouseInside);
            window.display();
        }

        assets.music.stop();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
