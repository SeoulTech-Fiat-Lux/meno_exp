```cpp
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float WorldW = 1920.f;
constexpr float WorldH = 1080.f;
constexpr float ViewW = 960.f;
constexpr float ViewH = 540.f;
constexpr double Tick = 1.0 / 120.0;

enum class Screen { Title, Play, Result };

struct Bullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    double age = 0.0;
    bool dead = false;
};

struct Enemy
{
    sf::Vector2f position;
    bool dead = false;
};

sf::Vector2f unit(sf::Vector2f v)
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y);
    return length > 0.f ? v / length : sf::Vector2f{1.f, 0.f};
}

bool overlap(sf::Vector2f a, sf::Vector2f b, float radius)
{
    const auto d = a - b;
    return d.x * d.x + d.y * d.y <= radius * radius;
}

std::string decimal(double value)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;
    return out.str();
}

int movementIndex(sf::Keyboard::Key key)
{
    using K = sf::Keyboard::Key;
    switch (key)
    {
        case K::W:     return 0;
        case K::Up:    return 1;
        case K::S:     return 2;
        case K::Down:  return 3;
        case K::A:     return 4;
        case K::Left:  return 5;
        case K::D:     return 6;
        case K::Right: return 7;
        default:      return -1;
    }
}

class SoundPool
{
public:
    SoundPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            sounds.emplace_back(buffer);
    }

    void play()
    {
        for (auto& sound : sounds)
        {
            if (sound.getStatus() == sf::SoundSource::Status::Stopped)
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

int run()
{
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Font font;
    sf::Texture playerTexture, enemyTexture, bulletTexture;
    sf::SoundBuffer shootBuffer, hitBuffer, loseBuffer;
    sf::Music music;

    auto checked = [](bool success, const char* path)
    {
        if (!success)
            std::cerr << "FAILED TO LOAD: " << path << '\n';
        return success;
    };

    if (!checked(font.openFromFile("assets/fonts/game.ttf"),
                 "assets/fonts/game.ttf"))
        return 1;
    if (!checked(playerTexture.loadFromFile("assets/textures/player.png"),
                 "assets/textures/player.png"))
        return 1;
    if (!checked(enemyTexture.loadFromFile("assets/textures/enemy.png"),
                 "assets/textures/enemy.png"))
        return 1;
    if (!checked(bulletTexture.loadFromFile("assets/textures/bullet.png"),
                 "assets/textures/bullet.png"))
        return 1;
    if (!checked(shootBuffer.loadFromFile("assets/sfx/shoot.wav"),
                 "assets/sfx/shoot.wav"))
        return 1;
    if (!checked(hitBuffer.loadFromFile("assets/sfx/hit.wav"),
                 "assets/sfx/hit.wav"))
        return 1;
    if (!checked(loseBuffer.loadFromFile("assets/sfx/lose.wav"),
                 "assets/sfx/lose.wav"))
        return 1;
    if (!checked(music.openFromFile("assets/music/bgm_shooter.ogg"),
                 "assets/music/bgm_shooter.ogg"))
        return 1;

    music.setVolume(40.f);
    music.setLooping(true);

    SoundPool shootSounds(shootBuffer, 4);
    SoundPool hitSounds(hitBuffer, 16);
    SoundPool loseSounds(loseBuffer, 2);

    sf::Sprite playerSprite(playerTexture);
    sf::Sprite enemySprite(enemyTexture);
    sf::Sprite bulletSprite(bulletTexture);
    playerSprite.setOrigin({16.f, 16.f});
    enemySprite.setOrigin({16.f, 16.f});
    bulletSprite.setOrigin({4.f, 4.f});

    sf::View uiView({ViewW / 2.f, ViewH / 2.f}, {ViewW, ViewH});
    sf::View worldView = uiView;

    auto resizeViews = [&](sf::Vector2u size)
    {
        if (size.x == 0 || size.y == 0)
            return;

        const float aspect = static_cast<float>(size.x) /
                             static_cast<float>(size.y);
        constexpr float target = ViewW / ViewH;
        sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});
        if (aspect > target)
        {
            viewport.size.x = target / aspect;
            viewport.position.x = (1.f - viewport.size.x) / 2.f;
        }
        else
        {
            viewport.size.y = aspect / target;
            viewport.position.y = (1.f - viewport.size.y) / 2.f;
        }
        uiView.setViewport(viewport);
        worldView.setViewport(viewport);
    };
    resizeViews(window.getSize());

    sf::RectangleShape background({WorldW, WorldH});
    background.setFillColor(sf::Color(0x14, 0x18, 0x1F));

    sf::VertexArray grid(sf::PrimitiveType::Lines);
    const sf::Color gridColor(0x22, 0x2A, 0x35);
    for (float x = 120.f; x < WorldW; x += 120.f)
    {
        grid.append(sf::Vertex{{x, 0.f}, gridColor});
        grid.append(sf::Vertex{{x, WorldH}, gridColor});
    }
    for (float y = 120.f; y < WorldH; y += 120.f)
    {
        grid.append(sf::Vertex{{0.f, y}, gridColor});
        grid.append(sf::Vertex{{WorldW, y}, gridColor});
    }

    sf::RectangleShape boundary({WorldW, WorldH});
    boundary.setFillColor(sf::Color::Transparent);
    boundary.setOutlineColor(sf::Color(0x5C, 0x6B, 0x80));
    boundary.setOutlineThickness(-2.f);

    auto text = [&](const std::string& string, unsigned size,
                    sf::Vector2f position, bool centered = false)
    {
        sf::Text label(font, string, size);
        label.setFillColor(sf::Color::White);
        if (centered)
        {
            const auto bounds = label.getLocalBounds();
            label.setOrigin({bounds.position.x + bounds.size.x / 2.f,
                             bounds.position.y + bounds.size.y / 2.f});
        }
        label.setPosition(position);
        window.draw(label);
    };

    Screen screen = Screen::Title;
    bool paused = false;
    bool focused = true;
    std::array<bool, 8> held{};
    std::array<bool, 8> pressed{};
    bool mouseHeld = false;
    bool mousePressed = false;

    sf::Vector2f player{WorldW / 2.f, WorldH / 2.f};
    sf::Vector2f aim{1.f, 0.f};
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    bullets.reserve(10);
    enemies.reserve(200);

    int hp = 3;
    int score = 0;
    double titleTime = 0.0;
    double playTime = 0.0;
    double accumulator = 0.0;
    double nextSpawn = 0.5;
    double fireCooldown = 0.0;
    double invincibility = 0.0;

    std::mt19937 random(std::random_device{}());

    auto updateCamera = [&]
    {
        worldView.setCenter({
            std::clamp(player.x, ViewW / 2.f, WorldW - ViewW / 2.f),
            std::clamp(player.y, ViewH / 2.f, WorldH - ViewH / 2.f)
        });
    };

    auto updateAim = [&]
    {
        if (window.getSize().x == 0 || window.getSize().y == 0)
            return;
        const auto target =
            window.mapPixelToCoords(sf::Mouse::getPosition(window), worldView);
        const auto delta = target - player;
        if (delta.x * delta.x + delta.y * delta.y > 0.000001f)
            aim = unit(delta);
    };

    auto startGame = [&]
    {
        screen = Screen::Play;
        paused = false;
        hp = 3;
        score = 0;
        playTime = 0.0;
        accumulator = 0.0;
        nextSpawn = 0.5;
        fireCooldown = 0.0;
        invincibility = 0.0;
        player = {WorldW / 2.f, WorldH / 2.f};
        aim = {1.f, 0.f};
        bullets.clear();
        enemies.clear();
        pressed.fill(false);
        mousePressed = false;
        shootSounds.stop();
        hitSounds.stop();
        loseSounds.stop();
        updateCamera();
        music.stop();
        music.play();
    };

    // Clip each side of the expanded camera rectangle to the world.
    auto spawnEnemy = [&]
    {
        struct Segment
        {
            sf::Vector2f a;
            sf::Vector2f b;
            float length;
        };
        std::vector<Segment> segments;
        segments.reserve(4);

        const auto center = worldView.getCenter();
        const float left = center.x - ViewW / 2.f - 64.f;
        const float right = center.x + ViewW / 2.f + 64.f;
        const float top = center.y - ViewH / 2.f - 64.f;
        const float bottom = center.y + ViewH / 2.f + 64.f;

        auto horizontal = [&](float y)
        {
            const float a = std::max(0.f, left);
            const float b = std::min(WorldW, right);
            if (y >= 0.f && y <= WorldH && b > a)
                segments.push_back({{a, y}, {b, y}, b - a});
        };
        auto vertical = [&](float x)
        {
            const float a = std::max(0.f, top);
            const float b = std::min(WorldH, bottom);
            if (x >= 0.f && x <= WorldW && b > a)
                segments.push_back({{x, a}, {x, b}, b - a});
        };
        horizontal(top);
        horizontal(bottom);
        vertical(left);
        vertical(right);

        float total = 0.f;
        for (const auto& segment : segments)
            total += segment.length;

        if (total <= 0.f)
            return;

        float distance = std::uniform_real_distribution<float>(0.f, total)(random);
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            const auto& segment = segments[i];
            if (distance <= segment.length || i + 1 == segments.size())
            {
                const float t = std::clamp(distance / segment.length, 0.f, 1.f);
                enemies.push_back({segment.a + (segment.b - segment.a) * t});
                return;
            }
            distance -= segment.length;
        }
    };

    auto update = [&]
    {
        constexpr float dt = static_cast<float>(Tick);
        playTime += Tick;
        invincibility = std::max(0.0, invincibility - Tick);

        auto active = [&](std::size_t i)
        {
            return focused && (held[i] || pressed[i]);
        };

        sf::Vector2f movement{
            static_cast<float>(active(6) || active(7)) -
                static_cast<float>(active(4) || active(5)),
            static_cast<float>(active(2) || active(3)) -
                static_cast<float>(active(0) || active(1))
        };
        if (movement.x != 0.f || movement.y != 0.f)
            player += unit(movement) * (240.f * dt);

        player.x = std::clamp(player.x, 16.f, WorldW - 16.f);
        player.y = std::clamp(player.y, 16.f, WorldH - 16.f);
        updateCamera();
        updateAim();

        // Expire old bullets before creating any new ones.
        for (auto& bullet : bullets)
        {
            bullet.age += Tick;
            bullet.position += bullet.velocity * dt;
            bullet.dead = bullet.age >= 1.4 - 1e-9 ||
                          bullet.position.x < 0.f ||
                          bullet.position.x > WorldW ||
                          bullet.position.y < 0.f ||
                          bullet.position.y > WorldH;
        }
        std::erase_if(bullets, [](const Bullet& b) { return b.dead; });

        fireCooldown = std::max(0.0, fireCooldown - Tick);
        if (focused && (mouseHeld || mousePressed) &&
            fireCooldown <= 1e-9)
        {
            bullets.push_back({player, aim * 600.f});
            shootSounds.play();
            fireCooldown = 0.15;
        }

        while (playTime + 1e-9 >= nextSpawn)
        {
            if (enemies.size() < 200)
                spawnEnemy();
            const double interval =
                std::max(0.3, 1.0 - 0.1 * std::floor(nextSpawn / 10.0));
            nextSpawn += interval;
        }

        for (auto& enemy : enemies)
        {
            const auto delta = player - enemy.position;
            const float distance =
                std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance > 0.f)
                enemy.position += delta * (std::min(100.f * dt, distance) /
                                            distance);
        }

        // A 120 Hz tick moves bullets only 5 pixels.
        for (auto& bullet : bullets)
        {
            for (auto& enemy : enemies)
            {
                if (!enemy.dead &&
                    overlap(bullet.position, enemy.position, 18.f))
                {
                    bullet.dead = true;
                    enemy.dead = true;
                    score += 10;
                    hitSounds.play();
                    break;
                }
            }
        }

        if (invincibility <= 0.0)
        {
            for (auto& enemy : enemies)
            {
                if (!enemy.dead && overlap(player, enemy.position, 28.f))
                {
                    enemy.dead = true;
                    --hp;
                    invincibility = 1.0;
                    loseSounds.play();
                    break;
                }
            }
        }

        std::erase_if(bullets, [](const Bullet& b) { return b.dead; });
        std::erase_if(enemies, [](const Enemy& e) { return e.dead; });

        pressed.fill(false);
        mousePressed = false;

        if (hp <= 0)
        {
            screen = Screen::Result;
            paused = false;
            accumulator = 0.0;
            music.stop();
        }
    };

    updateCamera();
    sf::Clock clock;

    while (window.isOpen())
    {
        double frameTime = static_cast<double>(clock.restart().asSeconds());
        bool timingChanged = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* resized = event->getIf<sf::Event::Resized>())
                resizeViews(resized->size);

            if (event->is<sf::Event::FocusLost>())
            {
                focused = false;
                held.fill(false);
                pressed.fill(false);
                mouseHeld = false;
                mousePressed = false;
            }
            if (event->is<sf::Event::FocusGained>())
                focused = true;

            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape)
                    window.close();

                const int index = movementIndex(key->code);
                if (index >= 0)
                {
                    held[static_cast<std::size_t>(index)] = true;
                    if (screen == Screen::Play && !paused)
                        pressed[static_cast<std::size_t>(index)] = true;
                }

                if (screen == Screen::Title && key->code == K::Enter)
                {
                    startGame();
                    timingChanged = true;
                }
                else if (screen == Screen::Result && key->code == K::R)
                {
                    startGame();
                    timingChanged = true;
                }
                else if (screen == Screen::Play && key->code == K::P)
                {
                    paused = !paused;
                    accumulator = 0.0;
                    pressed.fill(false);
                    mousePressed = false;
                    timingChanged = true;
                    if (paused)
                        music.pause();
                    else
                        music.play();
                }
            }

            if (const auto* key = event->getIf<sf::Event::KeyReleased>())
            {
                const int index = movementIndex(key->code);
                if (index >= 0)
                    held[static_cast<std::size_t>(index)] = false;
            }

            if (const auto* mouse =
                    event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                {
                    mouseHeld = true;
                    if (screen == Screen::Play && !paused)
                        mousePressed = true;
                }
            }

            if (const auto* mouse =
                    event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                    mouseHeld = false;
            }
        }

        if (!window.isOpen())
            break;

        if (timingChanged)
            frameTime = 0.0;

        if (screen == Screen::Title)
        {
            titleTime += frameTime;
            if (titleTime >= 2.0)
            {
                startGame();
                frameTime = 0.0;
            }
        }

        if (screen == Screen::Play && !paused)
        {
            accumulator += frameTime;
            while (accumulator + 1e-12 >= Tick &&
                   screen == Screen::Play && !paused)
            {
                accumulator -= Tick;
                update();
            }
        }

        if (screen == Screen::Play)
        {
            updateCamera();
            updateAim();
        }

        window.clear(sf::Color::Black);
        window.setView(uiView);

        sf::RectangleShape screenBackground({ViewW, ViewH});
        screenBackground.setFillColor(sf::Color(0x14, 0x18, 0x1F));
        window.draw(screenBackground);

        if (screen == Screen::Title)
        {
            text("ARENA", 48, {480.f, 220.f}, true);
            text("PRESS ENTER", 32, {480.f, 310.f}, true);
        }
        else if (screen == Screen::Result)
        {
            text("GAME OVER", 48, {480.f, 160.f}, true);
            text("SCORE " + std::to_string(score), 32,
                 {480.f, 245.f}, true);
            text("TIME " + decimal(playTime), 32,
                 {480.f, 300.f}, true);
            text("R: RESTART  ESC: QUIT", 16,
                 {480.f, 390.f}, true);
        }
        else
        {
            window.setView(worldView);
            window.draw(background);
            window.draw(grid);
            window.draw(boundary);

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
            const bool dim = invincibility > 0.0 &&
                             static_cast<int>(invincibility * 12.0) % 2 == 0;
            playerSprite.setColor(
                sf::Color(255, 255, 255, dim ? 96 : 255));
            window.draw(playerSprite);

            window.setView(uiView);
            text("HP " + std::to_string(hp) +
                 "  SCORE " + std::to_string(score) +
                 "  TIME " + decimal(playTime),
                 16, {16.f, 16.f});

            if (paused)
                text("PAUSED", 48, {480.f, 270.f}, true);
        }

        window.display();
    }

    music.stop();
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
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```