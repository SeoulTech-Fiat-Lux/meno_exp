```cpp
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
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr float WorldWidth = 2880.f;
constexpr float WorldHeight = 1620.f;
constexpr float ScreenWidth = 960.f;
constexpr float ScreenHeight = 540.f;
constexpr float Tick = 1.f / 120.f;
constexpr float Pi = 3.14159265358979323846f;

const sf::Color Background{26, 20, 32};
const sf::Color GridColor{42, 34, 51};
const sf::Color BoundaryColor{107, 91, 123};
const sf::Color ExperienceColor{79, 195, 247};
const sf::Color Gold{255, 213, 74};
const sf::Color HpBackground{58, 31, 36};
const sf::Color HpColor{229, 83, 61};

float lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

sf::Vector2f unit(sf::Vector2f v)
{
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.00001f ? v / length : sf::Vector2f{};
}

sf::Vector2f clampToWorld(sf::Vector2f p, float radius)
{
    p.x = std::clamp(p.x, radius, WorldWidth - radius);
    p.y = std::clamp(p.y, radius, WorldHeight - radius);
    return p;
}

std::string timeString(float seconds)
{
    const int whole = static_cast<int>(seconds);
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << whole / 60
        << ':' << std::setw(2) << whole % 60;
    return out.str();
}

// Returns the first intersection parameter along a swept bullet segment.
float segmentHit(sf::Vector2f from, sf::Vector2f to,
                 sf::Vector2f center, float radius)
{
    const sf::Vector2f offset = from - center;
    const float c = lengthSquared(offset) - radius * radius;
    if (c <= 0.f)
        return 0.f;

    const sf::Vector2f travel = to - from;
    const float a = lengthSquared(travel);
    if (a < 0.000001f)
        return -1.f;

    const float b = offset.x * travel.x + offset.y * travel.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.f)
        return -1.f;

    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.f && t <= 1.f ? t : -1.f;
}

struct EnemyStats
{
    float radius;
    float hp;
    float speed;
    float contactDamage;
    float animationInterval;
    int cell;
    int experience;
};

constexpr std::array<EnemyStats, 3> EnemyTypes{{
    {10.f, 10.f, 80.f, 10.f, 0.30f, 24, 1},
    {10.f, 6.f, 140.f, 6.f, 0.15f, 24, 1},
    {20.f, 60.f, 50.f, 20.f, 0.40f, 48, 5}
}};

struct Enemy
{
    sf::Vector2f position;
    sf::Vector2f knockDirection;
    int type = 0;
    float hp = 10.f;
    float hitRemaining = 0.f;
    float deathRemaining = 0.f;
    bool facingLeft = false;
};

struct Bullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    float life = 1.2f;
    float damage = 10.f;
};

struct Gem
{
    sf::Vector2f position;
    int experience = 1;
    float life = 30.f;
    bool attracted = false;
};

class SoundPool
{
public:
    void initialize(const sf::SoundBuffer& buffer, std::size_t count)
    {
        voices.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            voices.push_back(std::make_unique<sf::Sound>(buffer));
    }

    void play()
    {
        if (voices.empty())
            return;

        for (auto& voice : voices)
        {
            if (voice->getStatus() == sf::SoundSource::Status::Stopped)
            {
                voice->play();
                return;
            }
        }

        voices[next]->stop();
        voices[next]->play();
        next = (next + 1) % voices.size();
    }

    void stop()
    {
        for (auto& voice : voices)
            voice->stop();
        next = 0;
    }

private:
    std::vector<std::unique_ptr<sf::Sound>> voices;
    std::size_t next = 0;
};

class Game
{
public:
    explicit Game(sf::RenderWindow& renderWindow)
        : window(renderWindow),
          uiView(sf::FloatRect({0.f, 0.f}, {ScreenWidth, ScreenHeight})),
          worldView({WorldWidth / 2.f, WorldHeight / 2.f},
                    {ScreenWidth, ScreenHeight}),
          random(std::random_device{}()),
          grid(sf::PrimitiveType::Lines)
    {
    }

    bool load()
    {
        if (!loadAsset("assets/fonts/game.ttf",
                       [&] { return font.openFromFile("assets/fonts/game.ttf"); }))
            return false;

        const std::array<const char*, 6> texturePaths{{
            "assets/textures/sv_player.png",
            "assets/textures/sv_walker.png",
            "assets/textures/sv_runner.png",
            "assets/textures/sv_brute.png",
            "assets/textures/sv_bullet.png",
            "assets/textures/sv_gem.png"
        }};

        for (std::size_t i = 0; i < texturePaths.size(); ++i)
        {
            if (!loadAsset(texturePaths[i],
                           [&] { return textures[i].loadFromFile(texturePaths[i]); }))
                return false;
            textures[i].setSmooth(false);
        }

        const std::array<const char*, 4> soundPaths{{
            "assets/sfx/shoot.wav",
            "assets/sfx/hit.wav",
            "assets/sfx/pickup.wav",
            "assets/sfx/levelup.wav"
        }};

        for (std::size_t i = 0; i < soundPaths.size(); ++i)
        {
            if (!loadAsset(soundPaths[i],
                           [&] { return soundBuffers[i].loadFromFile(soundPaths[i]); }))
                return false;
            sounds[i].initialize(soundBuffers[i], i == 3 ? 4 : 16);
        }

        if (!loadAsset("assets/music/bgm_survivor.ogg",
                       [&] {
                           return music.openFromFile("assets/music/bgm_survivor.ogg");
                       }))
            return false;

        music.setVolume(40.f);
        music.setLooping(true);

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

        enemies.reserve(400);
        gems.reserve(512);
        bullets.reserve(64);
        resize(window.getSize());
        return true;
    }

    void resize(sf::Vector2u size)
    {
        if (size.x == 0 || size.y == 0)
            return;

        const float aspect = static_cast<float>(size.x) /
                             static_cast<float>(size.y);
        constexpr float targetAspect = ScreenWidth / ScreenHeight;
        sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});

        if (aspect > targetAspect)
        {
            viewport.size.x = targetAspect / aspect;
            viewport.position.x = (1.f - viewport.size.x) / 2.f;
        }
        else
        {
            viewport.size.y = aspect / targetAspect;
            viewport.position.y = (1.f - viewport.size.y) / 2.f;
        }

        uiView.setViewport(viewport);
        worldView.setViewport(viewport);
    }

    void handleEvent(const sf::Event& event)
    {
        if (event.is<sf::Event::Closed>())
        {
            window.close();
            return;
        }

        if (const auto* resized = event.getIf<sf::Event::Resized>())
            resize(resized->size);

        if (event.is<sf::Event::FocusLost>())
        {
            held.fill(false);
            tapped.fill(false);
        }

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            if (key->code == sf::Keyboard::Key::Escape)
            {
                window.close();
                return;
            }

            const int direction = movementIndex(key->code);
            if (direction >= 0)
            {
                held[static_cast<std::size_t>(direction)] = true;
                tapped[static_cast<std::size_t>(direction)] = true;
            }

            if (state == State::Title)
            {
                if (key->code == sf::Keyboard::Key::Enter)
                    restart();
            }
            else if (state == State::Result)
            {
                if (key->code == sf::Keyboard::Key::R)
                    restart();
            }
            else if (selecting)
            {
                if (key->code == sf::Keyboard::Key::Num1 ||
                    key->code == sf::Keyboard::Key::Numpad1)
                    chooseCard(0);
                else if (key->code == sf::Keyboard::Key::Num2 ||
                         key->code == sf::Keyboard::Key::Numpad2)
                    chooseCard(1);
                else if (key->code == sf::Keyboard::Key::Num3 ||
                         key->code == sf::Keyboard::Key::Numpad3)
                    chooseCard(2);
            }
            else if (key->code == sf::Keyboard::Key::P)
            {
                paused = !paused;
                tapped.fill(false);
            }
        }

        if (const auto* key = event.getIf<sf::Event::KeyReleased>())
        {
            const int direction = movementIndex(key->code);
            if (direction >= 0)
                held[static_cast<std::size_t>(direction)] = false;
        }

        if (const auto* mouse = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (state == State::Playing && selecting &&
                mouse->button == sf::Mouse::Button::Left)
            {
                const sf::Vector2f position =
                    window.mapPixelToCoords(mouse->position, uiView);

                for (int i = 0; i < 3; ++i)
                {
                    if (cardRectangle(i).contains(position))
                    {
                        chooseCard(i);
                        break;
                    }
                }
            }
        }
    }

    void update(float dt)
    {
        if (state == State::Title)
        {
            titleTime += dt;
            tapped.fill(false);
            if (titleTime >= 2.f)
                restart();
            return;
        }

        if (state != State::Playing || paused || selecting)
        {
            tapped.fill(false);
            return;
        }

        const float step = std::min(dt, 300.f - elapsed);
        elapsed = std::min(300.f, elapsed + step);

        movePlayer(step);
        updateCamera();
        spawnEnemies();
        moveEnemies(step);
        fire(step);
        updateBullets(step);
        applyContactDamage(step);

        if (hp <= 0.f)
        {
            hp = 0.f;
            finish(false);
            return;
        }

        updateGems(step);

        if (elapsed >= 300.f)
        {
            finish(true);
            return;
        }

        checkLevelUp();
    }

    void draw()
    {
        window.clear(sf::Color::Black);

        window.setView(uiView);
        rectangle({0.f, 0.f}, {ScreenWidth, ScreenHeight}, Background);

        if (state == State::Title)
        {
            centeredText("LAST STAND", {480.f, 190.f}, 48);
            centeredText("PRESS ENTER", {480.f, 290.f}, 32);
        }
        else if (state == State::Result)
        {
            centeredText(survived ? "SURVIVED" : "GAME OVER",
                         {480.f, 115.f}, 48);
            centeredText("TIME " + timeString(elapsed), {480.f, 210.f}, 32);
            centeredText("LEVEL " + std::to_string(level), {480.f, 258.f}, 32);
            centeredText("KILLS " + std::to_string(kills), {480.f, 306.f}, 32);
            centeredText("R: RESTART  ESC: QUIT", {480.f, 410.f}, 32);
        }
        else
        {
            drawWorld();
            window.setView(uiView);
            drawHud();

            if (selecting)
                drawCards();
            else if (paused)
            {
                rectangle({0.f, 0.f}, {ScreenWidth, ScreenHeight},
                          sf::Color{0, 0, 0, 160});
                centeredText("PAUSED", {480.f, 250.f}, 48);
            }
        }

        window.display();
    }

private:
    enum class State
    {
        Title,
        Playing,
        Result
    };

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

    static int movementIndex(sf::Keyboard::Key key)
    {
        switch (key)
        {
            case sf::Keyboard::Key::W:     return 0;
            case sf::Keyboard::Key::A:     return 1;
            case sf::Keyboard::Key::S:     return 2;
            case sf::Keyboard::Key::D:     return 3;
            case sf::Keyboard::Key::Up:    return 4;
            case sf::Keyboard::Key::Left:  return 5;
            case sf::Keyboard::Key::Down:  return 6;
            case sf::Keyboard::Key::Right: return 7;
            default: return -1;
        }
    }

    static sf::FloatRect cardRectangle(int index)
    {
        return sf::FloatRect(
            {80.f + 280.f * static_cast<float>(index), 200.f},
            {240.f, 140.f});
    }

    void restart()
    {
        state = State::Playing;
        paused = false;
        selecting = false;
        survived = false;
        elapsed = 0.f;
        titleTime = 0.f;
        player = {WorldWidth / 2.f, WorldHeight / 2.f};
        playerFacingLeft = false;
        playerMoving = false;
        playerAnimation = 0.f;
        takingDamage = false;
        hp = 100.f;
        maxHp = 100.f;
        speed = 200.f;
        damage = 10.f;
        fireInterval = 0.8f;
        fireRemaining = 0.8f;
        projectileCount = 1;
        magnetRadius = 60.f;
        nextSpawn = 0.5;
        level = 1;
        experience = 0;
        kills = 0;
        cards = {0, 1, 2};
        enemies.clear();
        bullets.clear();
        gems.clear();
        held.fill(false);
        tapped.fill(false);
        for (auto& pool : sounds)
            pool.stop();
        updateCamera();
        music.stop();
        music.play();
    }

    void finish(bool won)
    {
        survived = won;
        state = State::Result;
        paused = false;
        selecting = false;
        tapped.fill(false);
        music.stop();
    }

    void movePlayer(float dt)
    {
        const auto down = [&](std::size_t i) {
            return held[i] || tapped[i];
        };

        sf::Vector2f direction{
            static_cast<float>(down(3) || down(7)) -
                static_cast<float>(down(1) || down(5)),
            static_cast<float>(down(2) || down(6)) -
                static_cast<float>(down(0) || down(4))
        };

        tapped.fill(false);

        if (direction.x != 0.f)
            playerFacingLeft = direction.x < 0.f;

        playerMoving = lengthSquared(direction) > 0.f;
        if (playerMoving)
        {
            playerAnimation += dt;
            player = clampToWorld(player + unit(direction) * speed * dt, 14.f);
        }
        else
        {
            playerAnimation = 0.f;
        }
    }

    void updateCamera()
    {
        worldView.setCenter({
            std::clamp(player.x, ScreenWidth / 2.f,
                       WorldWidth - ScreenWidth / 2.f),
            std::clamp(player.y, ScreenHeight / 2.f,
                       WorldHeight - ScreenHeight / 2.f)
        });
    }

    float randomFloat(float low, float high)
    {
        return std::uniform_real_distribution<float>(low, high)(random);
    }

    sf::Vector2f spawnPosition()
    {
        const sf::Vector2f camera = worldView.getCenter();
        const float left = camera.x - ScreenWidth / 2.f - 64.f;
        const float right = camera.x + ScreenWidth / 2.f + 64.f;
        const float top = camera.y - ScreenHeight / 2.f - 64.f;
        const float bottom = camera.y + ScreenHeight / 2.f + 64.f;

        struct Segment
        {
            sf::Vector2f a;
            sf::Vector2f b;
            float length;
        };

        std::array<Segment, 4> segments{};
        std::size_t count = 0;
        float total = 0.f;

        const auto add = [&](sf::Vector2f a, sf::Vector2f b) {
            const float size = std::sqrt(lengthSquared(b - a));
            if (size > 0.f)
            {
                segments[count++] = {a, b, size};
                total += size;
            }
        };

        const float x0 = std::max(0.f, left);
        const float x1 = std::min(WorldWidth, right);
        const float y0 = std::max(0.f, top);
        const float y1 = std::min(WorldHeight, bottom);

        if (top >= 0.f && top <= WorldHeight)
            add({x0, top}, {x1, top});
        if (bottom >= 0.f && bottom <= WorldHeight)
            add({x0, bottom}, {x1, bottom});
        if (left >= 0.f && left <= WorldWidth)
            add({left, y0}, {left, y1});
        if (right >= 0.f && right <= WorldWidth)
            add({right, y0}, {right, y1});

        float distance = randomFloat(0.f, total);
        for (std::size_t i = 0; i < count; ++i)
        {
            if (distance <= segments[i].length || i + 1 == count)
            {
                const float t = std::clamp(
                    distance / segments[i].length, 0.f, 1.f);
                return segments[i].a +
                       (segments[i].b - segments[i].a) * t;
            }
            distance -= segments[i].length;
        }

        return {WorldWidth / 2.f, 64.f};
    }

    void spawnEnemies()
    {
        while (static_cast<double>(elapsed) + 0.000001 >= nextSpawn)
        {
            if (enemies.size() < 400)
            {
                const float roll = randomFloat(0.f, 1.f);
                int type = 0;

                if (elapsed >= 120.f)
                    type = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
                else if (elapsed >= 60.f)
                    type = roll < 0.7f ? 0 : 1;

                const sf::Vector2f position = spawnPosition();
                Enemy enemy;
                enemy.position = position;
                enemy.type = type;
                enemy.hp = EnemyTypes[static_cast<std::size_t>(type)].hp;
                enemy.facingLeft = player.x < position.x;
                enemies.push_back(enemy);
            }

            const double interval = std::max(
                0.15, 0.6 - 0.05 * std::floor(nextSpawn / 15.0));
            nextSpawn += interval;
        }
    }

    void moveEnemies(float dt)
    {
        for (auto& enemy : enemies)
        {
            if (enemy.hp <= 0.f)
            {
                enemy.deathRemaining -= dt;
                continue;
            }

            const auto& stats = EnemyTypes[static_cast<std::size_t>(enemy.type)];
            enemy.facingLeft = player.x < enemy.position.x;

            float movementTime = dt;
            if (enemy.hitRemaining > 0.f)
            {
                const float knockTime = std::min(dt, enemy.hitRemaining);
                enemy.position = clampToWorld(
                    enemy.position + enemy.knockDirection * (120.f * knockTime),
                    stats.radius);
                enemy.hitRemaining = std::max(0.f, enemy.hitRemaining - dt);
                movementTime -= knockTime;
            }

            if (movementTime > 0.f)
            {
                const sf::Vector2f difference = player - enemy.position;
                const float distance = std::sqrt(lengthSquared(difference));
                const float travel = std::min(distance, stats.speed * movementTime);
                enemy.position = clampToWorld(
                    enemy.position + unit(difference) * travel, stats.radius);
            }
        }

        std::erase_if(enemies, [](const Enemy& enemy) {
            return enemy.hp <= 0.f && enemy.deathRemaining <= 0.f;
        });
    }

    void fire(float dt)
    {
        fireRemaining = std::max(0.f, fireRemaining - dt);
        if (fireRemaining > 0.f)
            return;

        const Enemy* closest = nullptr;
        float closestDistance = 400.f * 400.f;

        for (const auto& enemy : enemies)
        {
            if (enemy.hp <= 0.f)
                continue;

            const float distance = lengthSquared(enemy.position - player);
            if (distance <= closestDistance)
            {
                closestDistance = distance;
                closest = &enemy;
            }
        }

        if (!closest)
            return;

        const sf::Vector2f difference = closest->position - player;
        const float baseAngle = lengthSquared(difference) > 0.00001f
                                    ? std::atan2(difference.y, difference.x)
                                    : 0.f;

        for (int i = 0; i < projectileCount; ++i)
        {
            const float offset =
                (static_cast<float>(i) -
                 (static_cast<float>(projectileCount) - 1.f) / 2.f) *
                (10.f * Pi / 180.f);
            const float angle = baseAngle + offset;
            bullets.push_back({
                player,
                {500.f * std::cos(angle), 500.f * std::sin(angle)},
                1.2f,
                damage
            });
        }

        sounds[0].play();
        fireRemaining = fireInterval;
    }

    void updateBullets(float dt)
    {
        for (auto& bullet : bullets)
        {
            const sf::Vector2f from = bullet.position;
            const sf::Vector2f to =
                from + bullet.velocity * std::min(dt, bullet.life);

            std::size_t target = enemies.size();
            float firstHit = std::numeric_limits<float>::infinity();

            for (std::size_t i = 0; i < enemies.size(); ++i)
            {
                const auto& enemy = enemies[i];
                if (enemy.hp <= 0.f)
                    continue;

                const float radius =
                    EnemyTypes[static_cast<std::size_t>(enemy.type)].radius + 4.f;
                const float hit = segmentHit(from, to, enemy.position, radius);
                if (hit >= 0.f && hit < firstHit)
                {
                    firstHit = hit;
                    target = i;
                }
            }

            bullet.position = to;
            bullet.life -= dt;

            if (target != enemies.size())
            {
                auto& enemy = enemies[target];
                enemy.hp -= bullet.damage;
                enemy.hitRemaining = 0.1f;
                enemy.knockDirection = unit(bullet.velocity);
                bullet.life = 0.f;

                if (enemy.hp <= 0.f)
                {
                    enemy.deathRemaining = 0.1f;
                    gems.push_back({
                        enemy.position,
                        EnemyTypes[static_cast<std::size_t>(enemy.type)].experience,
                        30.f,
                        false
                    });
                    ++kills;
                    sounds[1].play();
                }
            }
        }

        std::erase_if(bullets, [](const Bullet& bullet) {
            return bullet.life <= 0.f;
        });
    }

    void applyContactDamage(float dt)
    {
        takingDamage = false;
        for (const auto& enemy : enemies)
        {
            if (enemy.hp <= 0.f)
                continue;

            const auto& stats = EnemyTypes[static_cast<std::size_t>(enemy.type)];
            const float radius = 14.f + stats.radius;
            if (lengthSquared(enemy.position - player) <= radius * radius)
            {
                hp -= stats.contactDamage * dt;
                takingDamage = true;
            }
        }
    }

    void updateGems(float dt)
    {
        for (auto& gem : gems)
        {
            gem.life -= dt;
            if (gem.life <= 0.f)
                continue;

            sf::Vector2f difference = player - gem.position;
            float distance = std::sqrt(lengthSquared(difference));

            if (distance <= magnetRadius)
                gem.attracted = true;

            if (gem.attracted && distance > 0.f)
            {
                gem.position += unit(difference) * std::min(300.f * dt, distance);
                difference = player - gem.position;
                distance = std::sqrt(lengthSquared(difference));
            }

            if (distance <= 20.f)
            {
                experience += gem.experience;
                gem.life = 0.f;
                sounds[2].play();
            }
        }

        std::erase_if(gems, [](const Gem& gem) {
            return gem.life <= 0.f;
        });
    }

    void checkLevelUp()
    {
        if (experience < 5 * level)
            return;

        experience -= 5 * level;
        ++level;
        selecting = true;
        tapped.fill(false);
        sounds[3].play();

        std::array<int, 6> choices{0, 1, 2, 3, 4, 5};
        std::shuffle(choices.begin(), choices.end(), random);
        std::copy_n(choices.begin(), 3, cards.begin());
    }

    void chooseCard(int index)
    {
        if (!selecting || index < 0 || index >= 3)
            return;

        switch (cards[static_cast<std::size_t>(index)])
        {
            case 0:
                fireInterval *= 0.85f;
                break;
            case 1:
                damage += 5.f;
                break;
            case 2:
                ++projectileCount;
                break;
            case 3:
                speed *= 1.1f;
                break;
            case 4:
                maxHp += 20.f;
                hp = std::min(maxHp, hp + 20.f);
                break;
            case 5:
                magnetRadius += 40.f;
                break;
        }

        selecting = false;
        tapped.fill(false);
        checkLevelUp();
    }

    void rectangle(sf::Vector2f position, sf::Vector2f size,
                   sf::Color color)
    {
        sf::RectangleShape shape(size);
        shape.setPosition(position);
        shape.setFillColor(color);
        window.draw(shape);
    }

    void text(const std::string& value, sf::Vector2f position,
              unsigned int size = 16, sf::Color color = sf::Color::White)
    {
        sf::Text drawable(font, value, size);
        drawable.setPosition(position);
        drawable.setFillColor(color);
        window.draw(drawable);
    }

    void centeredText(const std::string& value, sf::Vector2f center,
                      unsigned int size)
    {
        sf::Text drawable(font, value, size);
        const sf::FloatRect bounds = drawable.getLocalBounds();
        drawable.setOrigin(bounds.position + bounds.size / 2.f);
        drawable.setPosition(center);
        drawable.setFillColor(sf::Color::White);
        window.draw(drawable);
    }

    void sprite(std::size_t textureIndex, sf::Vector2f position,
                int cell, int frame, bool flipped = false,
                sf::Color tint = sf::Color::White)
    {
        sf::Sprite drawable(textures[textureIndex]);
        drawable.setTextureRect(sf::IntRect({frame * cell, 0}, {cell, cell}));
        const float half = static_cast<float>(cell) / 2.f;
        drawable.setOrigin({half, half});
        drawable.setPosition(position);
        drawable.setScale({flipped ? -1.f : 1.f, 1.f});
        drawable.setColor(tint);
        window.draw(drawable);
    }

    bool visible(sf::Vector2f position, float radius) const
    {
        const sf::Vector2f delta = position - worldView.getCenter();
        return std::abs(delta.x) <= ScreenWidth / 2.f + radius &&
               std::abs(delta.y) <= ScreenHeight / 2.f + radius;
    }

    void drawWorld()
    {
        window.setView(worldView);
        window.draw(grid);

        sf::RectangleShape boundary({WorldWidth, WorldHeight});
        boundary.setFillColor(sf::Color::Transparent);
        boundary.setOutlineColor(BoundaryColor);
        boundary.setOutlineThickness(-2.f);
        window.draw(boundary);

        for (const auto& gem : gems)
        {
            if (visible(gem.position, 6.f))
            {
                sprite(5, gem.position, 12, 0, false,
                       gem.experience == 5 ? Gold : ExperienceColor);
            }
        }

        for (const auto& enemy : enemies)
        {
            const auto& stats = EnemyTypes[static_cast<std::size_t>(enemy.type)];
            if (!visible(enemy.position, static_cast<float>(stats.cell)))
                continue;

            const int frame =
                enemy.hp <= 0.f || enemy.hitRemaining > 0.f
                    ? 2
                    : static_cast<int>(elapsed / stats.animationInterval) % 2;

            sprite(static_cast<std::size_t>(enemy.type + 1),
                   enemy.position, stats.cell, frame, enemy.facingLeft);
        }

        const int playerFrame =
            playerMoving ? static_cast<int>(playerAnimation / 0.15f) % 2 : 0;
        sprite(0, player, 32, playerFrame, playerFacingLeft,
               takingDamage ? sf::Color{255, 96, 96} : sf::Color::White);

        for (const auto& bullet : bullets)
        {
            if (visible(bullet.position, 4.f))
                sprite(4, bullet.position, 8, 0);
        }
    }

    void drawHud()
    {
        rectangle({0.f, 0.f}, {ScreenWidth, 6.f}, GridColor);
        const float xpRatio = std::clamp(
            static_cast<float>(experience) / static_cast<float>(5 * level),
            0.f, 1.f);
        rectangle({0.f, 0.f}, {ScreenWidth * xpRatio, 6.f}, ExperienceColor);

        rectangle({16.f, 18.f}, {200.f, 12.f}, HpBackground);
        rectangle({16.f, 18.f},
                  {200.f * std::clamp(hp / maxHp, 0.f, 1.f), 12.f}, HpColor);
        text("HP " + std::to_string(static_cast<int>(std::ceil(hp))),
             {16.f, 38.f}, 16);

        text("LV " + std::to_string(level), {736.f, 14.f}, 16);
        text("TIME " + timeString(elapsed), {736.f, 38.f}, 16);
        text("KILLS " + std::to_string(kills), {736.f, 62.f}, 16);
    }

    void drawCards()
    {
        static const std::array<std::string, 6> labels{{
            "FIRE RATE +15%",
            "DAMAGE +5",
            "PROJECTILE +1",
            "SPEED +10%",
            "MAX HP +20",
            "MAGNET +40"
        }};

        rectangle({0.f, 0.f}, {ScreenWidth, ScreenHeight},
                  sf::Color{0, 0, 0, 160});

        for (int i = 0; i < 3; ++i)
        {
            const auto bounds = cardRectangle(i);
            sf::RectangleShape card(bounds.size);
            card.setPosition(bounds.position);
            card.setFillColor(GridColor);
            card.setOutlineColor(Gold);
            card.setOutlineThickness(2.f);
            window.draw(card);

            const float centerX = bounds.position.x + bounds.size.x / 2.f;
            centeredText(std::to_string(i + 1), {centerX, 234.f}, 32);
            centeredText(labels[static_cast<std::size_t>(
                             cards[static_cast<std::size_t>(i)])],
                         {centerX, 291.f}, 16);
        }
    }

    sf::RenderWindow& window;
    sf::Font font;
    std::array<sf::Texture, 6> textures;
    std::array<sf::SoundBuffer, 4> soundBuffers;
    std::array<SoundPool, 4> sounds;
    sf::Music music;
    sf::View uiView;
    sf::View worldView;
    std::mt19937 random;
    sf::VertexArray grid;

    State state = State::Title;
    float titleTime = 0.f;
    float elapsed = 0.f;
    bool paused = false;
    bool selecting = false;
    bool survived = false;

    sf::Vector2f player{WorldWidth / 2.f, WorldHeight / 2.f};
    bool playerFacingLeft = false;
    bool playerMoving = false;
    bool takingDamage = false;
    float playerAnimation = 0.f;

    float hp = 100.f;
    float maxHp = 100.f;
    float speed = 200.f;
    float damage = 10.f;
    float fireInterval = 0.8f;
    float fireRemaining = 0.8f;
    float magnetRadius = 60.f;
    int projectileCount = 1;
    int level = 1;
    int experience = 0;
    int kills = 0;
    double nextSpawn = 0.5;

    std::array<int, 3> cards{0, 1, 2};
    std::array<bool, 8> held{};
    std::array<bool, 8> tapped{};

    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};
}

int main()
{
    try
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "LAST STAND");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Game game(window);
        if (!game.load())
            return 1;

        sf::Clock clock;
        double accumulator = 0.0;

        while (window.isOpen())
        {
            accumulator += static_cast<double>(clock.restart().asSeconds());

            while (const auto event = window.pollEvent())
                game.handleEvent(*event);

            if (!window.isOpen())
                break;

            while (accumulator >= static_cast<double>(Tick))
            {
                game.update(Tick);
                accumulator -= static_cast<double>(Tick);
            }

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