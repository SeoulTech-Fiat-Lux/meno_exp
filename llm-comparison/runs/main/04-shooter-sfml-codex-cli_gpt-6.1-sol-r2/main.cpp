#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float WorldWidth = 1920.f;
constexpr float WorldHeight = 1080.f;
constexpr float ViewWidth = 960.f;
constexpr float ViewHeight = 540.f;
constexpr double Tick = 1.0 / 60.0;
constexpr float Pi = 3.14159265358979323846f;

const sf::Color Background{20, 24, 31};
const sf::Color GridColor{34, 42, 53};
const sf::Color BorderColor{92, 107, 128};

float lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

sf::Vector2f normalized(sf::Vector2f v)
{
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.00001f ? v / length : sf::Vector2f{1.f, 0.f};
}

sf::Vector2f cameraCenter(sf::Vector2f player)
{
    return {
        std::clamp(player.x, ViewWidth / 2.f, WorldWidth - ViewWidth / 2.f),
        std::clamp(player.y, ViewHeight / 2.f, WorldHeight - ViewHeight / 2.f)
    };
}

std::string decimal(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

template <class Loader>
bool loadAsset(const char* path, Loader loader)
{
    try
    {
        if (loader())
            return true;
    }
    catch (const std::exception&)
    {
    }

    std::cerr << "FAILED TO LOAD: " << path << '\n';
    return false;
}

struct Bullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    double age = 0.0;
    bool alive = true;
};

struct Enemy
{
    sf::Vector2f position;
    bool alive = true;
};

// Returns the first intersection along a moving point's segment.
float segmentHit(sf::Vector2f from, sf::Vector2f to,
                 sf::Vector2f center, float radius)
{
    const sf::Vector2f offset = from - center;
    const sf::Vector2f movement = to - from;
    const float c = lengthSquared(offset) - radius * radius;
    if (c <= 0.f)
        return 0.f;

    const float a = lengthSquared(movement);
    if (a <= 0.f)
        return std::numeric_limits<float>::infinity();

    const float b = offset.x * movement.x + offset.y * movement.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.f)
        return std::numeric_limits<float>::infinity();

    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.f && t <= 1.f
        ? t : std::numeric_limits<float>::infinity();
}

class SoundPool
{
public:
    explicit SoundPool(const sf::SoundBuffer& buffer)
    {
        sounds.reserve(16);
        for (int i = 0; i < 16; ++i)
            sounds.emplace_back(buffer);
    }

    void play()
    {
        for (auto& sound : sounds)
        {
            if (sound.getStatus() != sf::SoundSource::Status::Playing)
            {
                sound.play();
                return;
            }
        }

        sounds[next].stop();
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }

    void stop()
    {
        for (auto& sound : sounds)
            sound.stop();
        next = 0;
    }

private:
    std::vector<sf::Sound> sounds;
    std::size_t next = 0;
};

enum class Screen
{
    Title,
    Playing,
    Result
};

int movementIndex(sf::Keyboard::Key key)
{
    using Key = sf::Keyboard::Key;
    switch (key)
    {
        case Key::W:     return 0;
        case Key::Up:    return 1;
        case Key::S:     return 2;
        case Key::Down:  return 3;
        case Key::A:     return 4;
        case Key::Left:  return 5;
        case Key::D:     return 6;
        case Key::Right: return 7;
        default:        return -1;
    }
}

int run()
{
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Font font;
    sf::Texture playerTexture;
    sf::Texture enemyTexture;
    sf::Texture bulletTexture;
    sf::SoundBuffer shootBuffer;
    sf::SoundBuffer hitBuffer;
    sf::SoundBuffer loseBuffer;
    sf::Music music;

    if (!loadAsset("assets/fonts/game.ttf", [&] {
            return font.openFromFile("assets/fonts/game.ttf");
        }) ||
        !loadAsset("assets/textures/player.png", [&] {
            return playerTexture.loadFromFile("assets/textures/player.png");
        }) ||
        !loadAsset("assets/textures/enemy.png", [&] {
            return enemyTexture.loadFromFile("assets/textures/enemy.png");
        }) ||
        !loadAsset("assets/textures/bullet.png", [&] {
            return bulletTexture.loadFromFile("assets/textures/bullet.png");
        }) ||
        !loadAsset("assets/sfx/shoot.wav", [&] {
            return shootBuffer.loadFromFile("assets/sfx/shoot.wav");
        }) ||
        !loadAsset("assets/sfx/hit.wav", [&] {
            return hitBuffer.loadFromFile("assets/sfx/hit.wav");
        }) ||
        !loadAsset("assets/sfx/lose.wav", [&] {
            return loseBuffer.loadFromFile("assets/sfx/lose.wav");
        }) ||
        !loadAsset("assets/music/bgm_shooter.ogg", [&] {
            return music.openFromFile("assets/music/bgm_shooter.ogg");
        }))
    {
        return 1;
    }

    SoundPool shootSounds(shootBuffer);
    SoundPool hitSounds(hitBuffer);
    SoundPool loseSounds(loseBuffer);

    music.setVolume(40.f);
    music.setLooping(true);

    sf::Sprite playerSprite(playerTexture);
    sf::Sprite enemySprite(enemyTexture);
    sf::Sprite bulletSprite(bulletTexture);
    playerSprite.setOrigin({16.f, 16.f});
    enemySprite.setOrigin({16.f, 16.f});
    bulletSprite.setOrigin({4.f, 4.f});

    sf::View worldView({ViewWidth / 2.f, ViewHeight / 2.f},
                       {ViewWidth, ViewHeight});
    sf::View hudView({ViewWidth / 2.f, ViewHeight / 2.f},
                     {ViewWidth, ViewHeight});

    auto updateViewport = [&] {
        const auto size = window.getSize();
        if (size.x == 0 || size.y == 0)
            return;

        const float aspect = static_cast<float>(size.x) /
                             static_cast<float>(size.y);
        constexpr float desired = ViewWidth / ViewHeight;
        sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});

        if (aspect > desired)
        {
            viewport.size.x = desired / aspect;
            viewport.position.x = (1.f - viewport.size.x) / 2.f;
        }
        else
        {
            viewport.size.y = aspect / desired;
            viewport.position.y = (1.f - viewport.size.y) / 2.f;
        }

        worldView.setViewport(viewport);
        hudView.setViewport(viewport);
    };
    updateViewport();

    sf::VertexArray grid(sf::PrimitiveType::Lines);
    for (float x = 120.f; x < WorldWidth; x += 120.f)
    {
        grid.append(sf::Vertex{{x, 0.f}, GridColor});
        grid.append(sf::Vertex{{x, WorldHeight}, GridColor});
    }
    for (float y = 120.f; y < WorldHeight; y += 120.f)
    {
        grid.append(sf::Vertex{{0.f, y}, GridColor});
        grid.append(sf::Vertex{{WorldWidth, y}, GridColor});
    }

    sf::RectangleShape worldBackground({WorldWidth, WorldHeight});
    worldBackground.setFillColor(Background);

    sf::RectangleShape border({WorldWidth, WorldHeight});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(BorderColor);
    border.setOutlineThickness(-2.f);

    sf::RectangleShape screenBackground({ViewWidth, ViewHeight});
    screenBackground.setFillColor(Background);

    auto drawText = [&](const std::string& string, unsigned size,
                        sf::Vector2f position, bool centered = false) {
        sf::Text text(font, string, size);
        text.setFillColor(sf::Color::White);
        if (centered)
        {
            const auto bounds = text.getLocalBounds();
            text.setOrigin(bounds.position + bounds.size / 2.f);
        }
        text.setPosition(position);
        window.draw(text);
    };

    Screen screen = Screen::Title;
    bool paused = false;
    double titleTime = 0.0;
    double gameTime = 0.0;
    double invulnerability = 0.0;
    double nextSpawn = 0.5;
    double shotCooldown = 0.0;
    int hp = 3;
    int score = 0;
    sf::Vector2f player{WorldWidth / 2.f, WorldHeight / 2.f};
    sf::Vector2f aim{1.f, 0.f};

    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    bullets.reserve(10);
    enemies.reserve(200);

    std::array<bool, 8> held{};
    std::array<bool, 8> tapped{};
    bool mouseHeld = false;
    bool mouseTapped = false;

    std::mt19937 random(std::random_device{}());

    auto clearInput = [&] {
        held.fill(false);
        tapped.fill(false);
        mouseHeld = false;
        mouseTapped = false;
    };

    auto startGame = [&] {
        shootSounds.stop();
        hitSounds.stop();
        loseSounds.stop();
        music.stop();

        screen = Screen::Playing;
        paused = false;
        titleTime = 0.0;
        gameTime = 0.0;
        invulnerability = 0.0;
        nextSpawn = 0.5;
        shotCooldown = 0.0;
        hp = 3;
        score = 0;
        player = {WorldWidth / 2.f, WorldHeight / 2.f};
        aim = {1.f, 0.f};
        bullets.clear();
        enemies.clear();
        tapped.fill(false);
        mouseTapped = false;
        worldView.setCenter(cameraCenter(player));
        music.play();
    };

    auto updateAim = [&] {
        worldView.setCenter(cameraCenter(player));
        const sf::Vector2f cursor =
            window.mapPixelToCoords(sf::Mouse::getPosition(window), worldView);
        if (lengthSquared(cursor - player) > 0.00001f)
            aim = normalized(cursor - player);
    };

    auto spawnEnemy = [&] {
        // Clip each original perimeter segment to the world, rather than
        // introducing artificial perimeter segments along the world boundary.
        struct Segment
        {
            sf::Vector2f start;
            sf::Vector2f end;
            float length;
        };
        std::array<Segment, 4> segments{};
        std::size_t count = 0;
        float total = 0.f;

        const sf::Vector2f center = cameraCenter(player);
        const float left = center.x - ViewWidth / 2.f - 64.f;
        const float right = center.x + ViewWidth / 2.f + 64.f;
        const float top = center.y - ViewHeight / 2.f - 64.f;
        const float bottom = center.y + ViewHeight / 2.f + 64.f;

        auto addHorizontal = [&](float y) {
            if (y < 0.f || y > WorldHeight)
                return;
            const float a = std::max(0.f, left);
            const float b = std::min(WorldWidth, right);
            if (b > a)
            {
                segments[count++] = {{a, y}, {b, y}, b - a};
                total += b - a;
            }
        };

        auto addVertical = [&](float x) {
            if (x < 0.f || x > WorldWidth)
                return;
            const float a = std::max(0.f, top);
            const float b = std::min(WorldHeight, bottom);
            if (b > a)
            {
                segments[count++] = {{x, a}, {x, b}, b - a};
                total += b - a;
            }
        };

        addHorizontal(top);
        addHorizontal(bottom);
        addVertical(left);
        addVertical(right);

        if (count == 0)
            return;

        float distance = std::uniform_real_distribution<float>(0.f, total)(random);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& segment = segments[i];
            if (distance <= segment.length || i + 1 == count)
            {
                const float fraction =
                    std::clamp(distance / segment.length, 0.f, 1.f);
                enemies.push_back({
                    segment.start + (segment.end - segment.start) * fraction,
                    true
                });
                return;
            }
            distance -= segment.length;
        }
    };

    auto updateGame = [&] {
        constexpr float dt = static_cast<float>(Tick);
        gameTime += Tick;
        invulnerability = std::max(0.0, invulnerability - Tick);

        auto active = [&](std::size_t index) {
            return held[index] || tapped[index];
        };

        sf::Vector2f movement{
            static_cast<float>(active(6) || active(7)) -
                static_cast<float>(active(4) || active(5)),
            static_cast<float>(active(2) || active(3)) -
                static_cast<float>(active(0) || active(1))
        };

        if (lengthSquared(movement) > 0.f)
            player += normalized(movement) * (240.f * dt);

        player.x = std::clamp(player.x, 16.f, WorldWidth - 16.f);
        player.y = std::clamp(player.y, 16.f, WorldHeight - 16.f);
        updateAim();

        shotCooldown = std::max(0.0, shotCooldown - Tick);
        if ((mouseHeld || mouseTapped) && shotCooldown <= 1e-9)
        {
            bullets.push_back({player, aim * 600.f, 0.0, true});
            shotCooldown = 0.15;
            shootSounds.play();
        }

        tapped.fill(false);
        mouseTapped = false;

        while (gameTime + 1e-9 >= nextSpawn)
        {
            if (enemies.size() < 200)
                spawnEnemy();

            const double interval = std::max(
                0.3, 1.0 - 0.1 * std::floor(gameTime / 10.0));
            nextSpawn += interval;
        }

        for (auto& enemy : enemies)
        {
            const sf::Vector2f displacement = player - enemy.position;
            const float distance = std::sqrt(lengthSquared(displacement));
            if (distance > 0.f)
            {
                enemy.position += displacement *
                    (std::min(100.f * dt, distance) / distance);
            }
        }

        for (auto& bullet : bullets)
        {
            const sf::Vector2f previous = bullet.position;
            bullet.position += bullet.velocity * dt;
            bullet.age += Tick;

            float earliest = std::numeric_limits<float>::infinity();
            Enemy* victim = nullptr;

            for (auto& enemy : enemies)
            {
                if (!enemy.alive)
                    continue;

                const float hit = segmentHit(
                    previous, bullet.position, enemy.position, 18.f);
                if (hit < earliest)
                {
                    earliest = hit;
                    victim = &enemy;
                }
            }

            if (victim)
            {
                victim->alive = false;
                bullet.alive = false;
                score += 10;
                hitSounds.play();
            }

            if (bullet.age + 1e-9 >= 1.4 ||
                bullet.position.x < 0.f || bullet.position.x > WorldWidth ||
                bullet.position.y < 0.f || bullet.position.y > WorldHeight)
            {
                bullet.alive = false;
            }
        }

        if (invulnerability <= 0.0)
        {
            for (auto& enemy : enemies)
            {
                if (enemy.alive &&
                    lengthSquared(enemy.position - player) <= 28.f * 28.f)
                {
                    enemy.alive = false;
                    --hp;
                    invulnerability = 1.0;
                    loseSounds.play();
                    break;
                }
            }
        }

        std::erase_if(bullets, [](const Bullet& bullet) {
            return !bullet.alive;
        });
        std::erase_if(enemies, [](const Enemy& enemy) {
            return !enemy.alive;
        });

        if (hp <= 0)
        {
            screen = Screen::Result;
            music.stop();
            clearInput();
        }
    };

    sf::Clock frameClock;
    double accumulator = 0.0;

    while (window.isOpen())
    {
        double elapsed = static_cast<double>(frameClock.restart().asSeconds());
        bool resetFrameTime = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (event->is<sf::Event::Resized>())
            {
                updateViewport();
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                clearInput();
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                using Key = sf::Keyboard::Key;
                if (key->code == Key::Escape)
                {
                    window.close();
                }
                else if (key->code == Key::Enter && screen == Screen::Title)
                {
                    startGame();
                    resetFrameTime = true;
                }
                else if (key->code == Key::R && screen == Screen::Result)
                {
                    startGame();
                    resetFrameTime = true;
                }
                else if (key->code == Key::P && screen == Screen::Playing)
                {
                    paused = !paused;
                    tapped.fill(false);
                    mouseTapped = false;
                    if (paused)
                        music.pause();
                    else
                        music.play();
                    resetFrameTime = true;
                }

                const int index = movementIndex(key->code);
                if (index >= 0)
                {
                    held[static_cast<std::size_t>(index)] = true;
                    if (screen == Screen::Playing && !paused)
                        tapped[static_cast<std::size_t>(index)] = true;
                }
            }
            else if (const auto* key = event->getIf<sf::Event::KeyReleased>())
            {
                const int index = movementIndex(key->code);
                if (index >= 0)
                    held[static_cast<std::size_t>(index)] = false;
            }
            else if (const auto* button =
                         event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (button->button == sf::Mouse::Button::Left)
                {
                    mouseHeld = true;
                    if (screen == Screen::Playing && !paused)
                        mouseTapped = true;
                }
            }
            else if (const auto* button =
                         event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (button->button == sf::Mouse::Button::Left)
                    mouseHeld = false;
            }
        }

        if (!window.isOpen())
            break;

        if (resetFrameTime)
        {
            accumulator = 0.0;
            elapsed = 0.0;
            (void)frameClock.restart();
        }

        if (screen == Screen::Playing && paused)
        {
            accumulator = 0.0;
        }
        else
        {
            accumulator += elapsed;
            while (accumulator + 1e-12 >= Tick)
            {
                accumulator -= Tick;
                if (screen == Screen::Title)
                {
                    titleTime += Tick;
                    if (titleTime + 1e-9 >= 2.0)
                        startGame();
                }
                else if (screen == Screen::Playing)
                {
                    updateGame();
                }
                else
                {
                    accumulator = 0.0;
                    break;
                }
            }
        }

        window.clear(sf::Color::Black);
        window.setView(hudView);
        window.draw(screenBackground);

        if (screen == Screen::Title)
        {
            drawText("ARENA", 48, {480.f, 215.f}, true);
            drawText("PRESS ENTER", 32, {480.f, 315.f}, true);
        }
        else if (screen == Screen::Playing)
        {
            updateAim();
            window.setView(worldView);
            window.draw(worldBackground);
            window.draw(grid);
            window.draw(border);

            for (const auto& bullet : bullets)
            {
                bulletSprite.setPosition(bullet.position);
                window.draw(bulletSprite);
            }

            for (const auto& enemy : enemies)
            {
                enemySprite.setPosition(enemy.position);
                window.draw(enemySprite);
            }

            playerSprite.setPosition(player);
            playerSprite.setRotation(
                sf::radians(std::atan2(aim.y, aim.x)));

            const bool dim = invulnerability > 0.0 &&
                (static_cast<int>((1.0 - invulnerability) * 12.0) % 2 == 0);
            playerSprite.setColor(
                sf::Color{255, 255, 255,
                          static_cast<std::uint8_t>(dim ? 90 : 255)});
            window.draw(playerSprite);

            window.setView(hudView);
            drawText("HP " + std::to_string(hp) +
                     "  SCORE " + std::to_string(score) +
                     "  TIME " + decimal(gameTime),
                     16, {16.f, 12.f});

            if (paused)
                drawText("PAUSED", 48, {480.f, 270.f}, true);
        }
        else
        {
            drawText("GAME OVER", 48, {480.f, 150.f}, true);
            drawText("SCORE " + std::to_string(score),
                     32, {480.f, 235.f}, true);
            drawText("TIME " + decimal(gameTime),
                     32, {480.f, 290.f}, true);
            drawText("R: RESTART  ESC: QUIT",
                     32, {480.f, 385.f}, true);
        }

        window.display();
    }

    return 0;
}
}

int main()
{
    try
    {
        return run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
