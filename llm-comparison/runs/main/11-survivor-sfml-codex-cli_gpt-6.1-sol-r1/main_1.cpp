#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr float WorldW = 2880.f;
constexpr float WorldH = 1620.f;
constexpr double Tick = 1.0 / 120.0;
constexpr float Pi = 3.14159265358979323846f;

const sf::Color Background{0x1A, 0x14, 0x20};
const sf::Color GridColor{0x2A, 0x22, 0x33};
const sf::Color BorderColor{0x6B, 0x5B, 0x7B};
const sf::Color Blue{0x4F, 0xC3, 0xF7};
const sf::Color Gold{0xFF, 0xD5, 0x4A};
const sf::Color HpBack{0x3A, 0x1F, 0x24};
const sf::Color HpFill{0xE5, 0x53, 0x3D};

float lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

sf::Vector2f unit(sf::Vector2f v)
{
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.f ? v / length : sf::Vector2f{};
}

sf::Vector2f constrain(sf::Vector2f p, float radius)
{
    return {std::clamp(p.x, radius, WorldW - radius),
            std::clamp(p.y, radius, WorldH - radius)};
}

sf::Vector2f cameraPosition(sf::Vector2f player)
{
    return {std::clamp(player.x, 480.f, WorldW - 480.f),
            std::clamp(player.y, 270.f, WorldH - 270.f)};
}

bool overlaps(sf::Vector2f a, sf::Vector2f b, float radius)
{
    return lengthSquared(a - b) <= radius * radius;
}

std::string timeString(double time)
{
    const int seconds = static_cast<int>(time + 0.000001);
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << seconds / 60
           << ':' << std::setw(2) << seconds % 60;
    return stream.str();
}

void require(bool success, const std::string& path)
{
    if (!success)
        throw std::runtime_error(path);
}

struct Resources
{
    sf::Font font;
    std::array<sf::Texture, 6> textures;
    std::array<sf::SoundBuffer, 4> buffers;
    sf::Music music;
    std::vector<sf::Sound> voices;
    std::size_t nextVoice = 0;

    Resources()
    {
        const std::string fontPath = "assets/fonts/game.ttf";
        require(font.openFromFile(fontPath), fontPath);

        const std::array<std::string, 6> texturePaths{
            "assets/textures/sv_player.png",
            "assets/textures/sv_walker.png",
            "assets/textures/sv_runner.png",
            "assets/textures/sv_brute.png",
            "assets/textures/sv_bullet.png",
            "assets/textures/sv_gem.png"
        };

        for (std::size_t i = 0; i < textures.size(); ++i)
            require(textures[i].loadFromFile(texturePaths[i]), texturePaths[i]);

        const std::array<std::string, 4> soundPaths{
            "assets/sfx/shoot.wav",
            "assets/sfx/hit.wav",
            "assets/sfx/pickup.wav",
            "assets/sfx/levelup.wav"
        };

        for (std::size_t i = 0; i < buffers.size(); ++i)
            require(buffers[i].loadFromFile(soundPaths[i]), soundPaths[i]);

        const std::string musicPath = "assets/music/bgm_survivor.ogg";
        require(music.openFromFile(musicPath), musicPath);
        music.setVolume(40.f);
        music.setLooping(true);

        voices.reserve(64);
        for (int i = 0; i < 64; ++i)
            voices.emplace_back(buffers[0]);
    }

    void sound(std::size_t index)
    {
        auto& voice = voices[nextVoice];
        nextVoice = (nextVoice + 1) % voices.size();
        voice.stop();
        voice.setBuffer(buffers[index]);
        voice.play();
    }

    void stopSounds()
    {
        for (auto& voice : voices)
            voice.stop();
    }
};

struct Enemy
{
    sf::Vector2f position;
    sf::Vector2f knockDirection{};
    int type = 0;
    float hp = 10.f;
    double hitTime = 0.;
    double deathTime = 0.;
    bool dead = false;
};

struct Bullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    float damage;
    double life = 1.2;
    bool removed = false;
};

struct Gem
{
    sf::Vector2f position;
    int value = 1;
    double life = 30.;
    bool removed = false;
};

constexpr std::array<float, 3> EnemyRadius{10.f, 10.f, 20.f};
constexpr std::array<float, 3> EnemyHp{10.f, 6.f, 60.f};
constexpr std::array<float, 3> EnemySpeed{80.f, 140.f, 50.f};
constexpr std::array<float, 3> EnemyDamage{10.f, 6.f, 20.f};
constexpr std::array<double, 3> EnemyAnimation{0.3, 0.15, 0.4};

const std::array<std::string, 6> AbilityNames{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1",
    "SPEED +10%", "MAX HP +20", "MAGNET +40"
};

enum class Screen
{
    Title,
    Playing,
    Result
};

class Game
{
public:
    explicit Game(Resources& resources) : assets(resources)
    {
        enemies.reserve(400);
        bullets.reserve(128);
        gems.reserve(512);

        for (float x = 120.f; x < WorldW; x += 120.f)
        {
            grid.append(sf::Vertex{{x, 0.f}, GridColor});
            grid.append(sf::Vertex{{x, WorldH}, GridColor});
        }
        for (float y = 120.f; y < WorldH; y += 120.f)
        {
            grid.append(sf::Vertex{{0.f, y}, GridColor});
            grid.append(sf::Vertex{{WorldW, y}, GridColor});
        }
    }

    Screen screen = Screen::Title;
    bool paused = false;
    bool choosing = false;

    void restart()
    {
        enemies.clear();
        bullets.clear();
        gems.clear();
        player = {WorldW / 2.f, WorldH / 2.f};
        hp = maxHp = 100.f;
        speed = 200.f;
        magnet = 60.f;
        damage = 10.f;
        fireInterval = 0.8;
        shotTimer = 0.8;
        nextSpawn = 0.5;
        elapsed = 0.;
        walkTime = 0.;
        kills = 0;
        level = 1;
        experience = 0;
        projectileCount = 1;
        playerLeft = false;
        moving = false;
        touching = false;
        paused = false;
        choosing = false;
        survived = false;
        screen = Screen::Playing;
        assets.stopSounds();
        assets.music.stop();
        assets.music.play();
    }

    void togglePause()
    {
        if (screen == Screen::Playing && !choosing)
            paused = !paused;
    }

    void select(int card)
    {
        if (!choosing || card < 0 || card >= 3)
            return;

        switch (cards[static_cast<std::size_t>(card)])
        {
            case 0: fireInterval *= 0.85; break;
            case 1: damage += 5.f; break;
            case 2: ++projectileCount; break;
            case 3: speed *= 1.1f; break;
            case 4:
                maxHp += 20.f;
                hp = std::min(maxHp, hp + 20.f);
                break;
            case 5: magnet += 40.f; break;
        }

        choosing = false;
        checkLevel();
    }

    void click(sf::Vector2f p)
    {
        for (int i = 0; i < 3; ++i)
        {
            const sf::FloatRect rectangle(
                {80.f + 280.f * static_cast<float>(i), 200.f},
                {240.f, 140.f});
            if (rectangle.contains(p))
            {
                select(i);
                return;
            }
        }
    }

    void update(float dt, sf::Vector2f movement)
    {
        if (screen != Screen::Playing || paused || choosing)
            return;

        const double step = std::min(static_cast<double>(dt), 300. - elapsed);
        dt = static_cast<float>(step);
        elapsed += step;

        moving = lengthSquared(movement) > 0.f;
        if (movement.x != 0.f)
            playerLeft = movement.x < 0.f;

        if (moving)
        {
            player = constrain(player + unit(movement) * speed * dt, 14.f);
            walkTime += step;
        }
        else
        {
            walkTime = 0.;
        }

        while (elapsed + 1e-9 >= nextSpawn)
        {
            if (enemies.size() < 400)
                spawn();

            const double interval =
                std::max(0.15, 0.6 - 0.05 * std::floor((elapsed + 1e-9) / 15.));
            nextSpawn += interval;
        }

        for (auto& enemy : enemies)
        {
            if (enemy.dead)
            {
                enemy.deathTime -= step;
                continue;
            }

            const auto type = static_cast<std::size_t>(enemy.type);
            const double knocked = std::min(step, enemy.hitTime);
            if (knocked > 0.)
            {
                enemy.position += enemy.knockDirection *
                                  (120.f * static_cast<float>(knocked));
                enemy.hitTime = std::max(0., enemy.hitTime - step);
            }

            const float pursuit = static_cast<float>(step - knocked);
            enemy.position += unit(player - enemy.position) *
                              EnemySpeed[type] * pursuit;
            enemy.position = constrain(enemy.position, EnemyRadius[type]);
        }

        shotTimer += step;
        if (shotTimer + 1e-9 >= fireInterval)
        {
            shotTimer = std::fmod(shotTimer, fireInterval);
            fire();
        }

        for (auto& bullet : bullets)
        {
            const float travelTime =
                static_cast<float>(std::min(step, bullet.life));
            const sf::Vector2f delta = bullet.velocity * travelTime;
            float earliest = std::numeric_limits<float>::infinity();
            Enemy* target = nullptr;

            for (auto& enemy : enemies)
            {
                if (enemy.dead)
                    continue;

                const float radius =
                    EnemyRadius[static_cast<std::size_t>(enemy.type)] + 4.f;
                const sf::Vector2f offset = bullet.position - enemy.position;
                const float c = lengthSquared(offset) - radius * radius;
                float hit = 0.f;

                if (c > 0.f)
                {
                    const float a = lengthSquared(delta);
                    if (a <= 0.f)
                        continue;
                    const float b = offset.x * delta.x + offset.y * delta.y;
                    const float discriminant = b * b - a * c;
                    if (discriminant < 0.f)
                        continue;
                    hit = (-b - std::sqrt(discriminant)) / a;
                    if (hit < 0.f || hit > 1.f)
                        continue;
                }

                if (hit < earliest)
                {
                    earliest = hit;
                    target = &enemy;
                }
            }

            if (target)
            {
                bullet.position += delta * earliest;
                bullet.removed = true;
                target->hp -= bullet.damage;
                target->hitTime = 0.1;
                target->knockDirection = unit(bullet.velocity);

                if (target->hp <= 0.f)
                {
                    target->dead = true;
                    target->deathTime = 0.1;
                    ++kills;
                    gems.push_back(
                        {target->position, target->type == 2 ? 5 : 1});
                    assets.sound(1);
                }
            }
            else
            {
                bullet.position += delta;
            }

            bullet.life -= step;
            if (bullet.life <= 1e-9)
                bullet.removed = true;
        }

        touching = false;
        for (const auto& enemy : enemies)
        {
            if (enemy.dead)
                continue;
            const auto type = static_cast<std::size_t>(enemy.type);
            if (overlaps(player, enemy.position, 14.f + EnemyRadius[type]))
            {
                touching = true;
                hp -= EnemyDamage[type] * dt;
            }
        }

        for (auto& gem : gems)
        {
            gem.life -= step;
            if (gem.life <= 0.)
            {
                gem.removed = true;
                continue;
            }

            const sf::Vector2f difference = player - gem.position;
            const float distance = std::sqrt(lengthSquared(difference));

            if (distance <= magnet && distance > 0.f)
                gem.position += difference / distance *
                                std::min(300.f * dt, distance);

            if (overlaps(player, gem.position, 20.f))
            {
                experience += gem.value;
                gem.removed = true;
                assets.sound(2);
            }
        }

        std::erase_if(enemies, [](const Enemy& e)
        {
            return e.dead && e.deathTime <= 1e-9;
        });
        std::erase_if(bullets, [](const Bullet& b) { return b.removed; });
        std::erase_if(gems, [](const Gem& g) { return g.removed; });

        if (hp <= 0.f)
        {
            hp = 0.f;
            finish(false);
        }
        else if (elapsed >= 300. - 1e-9)
        {
            elapsed = 300.;
            finish(true);
        }
        else
        {
            checkLevel();
        }
    }

    void draw(sf::RenderWindow& window, sf::View ui)
    {
        if (screen == Screen::Title)
        {
            window.setView(ui);
            text(window, "LAST STAND", {480.f, 170.f}, 48, true);
            text(window, "PRESS ENTER", {480.f, 290.f}, 32, true);
            return;
        }

        if (screen == Screen::Result)
        {
            window.setView(ui);
            text(window, survived ? "SURVIVED" : "GAME OVER",
                 {480.f, 100.f}, 48, true);
            text(window, "TIME " + timeString(elapsed),
                 {480.f, 205.f}, 32, true);
            text(window, "LEVEL " + std::to_string(level),
                 {480.f, 255.f}, 32, true);
            text(window, "KILLS " + std::to_string(kills),
                 {480.f, 305.f}, 32, true);
            text(window, "R: RESTART  ESC: QUIT",
                 {480.f, 415.f}, 16, true);
            return;
        }

        sf::View world = ui;
        world.setCenter(cameraPosition(player));
        window.setView(world);
        window.draw(grid);

        rectangle(window, {0.f, 0.f}, {WorldW, 2.f}, BorderColor);
        rectangle(window, {0.f, WorldH - 2.f}, {WorldW, 2.f}, BorderColor);
        rectangle(window, {0.f, 0.f}, {2.f, WorldH}, BorderColor);
        rectangle(window, {WorldW - 2.f, 0.f}, {2.f, WorldH}, BorderColor);

        for (const auto& gem : gems)
            sprite(window, 5, gem.position, 12, 0, false,
                   gem.value == 5 ? Gold : Blue);

        for (const auto& enemy : enemies)
        {
            const auto type = static_cast<std::size_t>(enemy.type);
            const int frame = enemy.dead || enemy.hitTime > 1e-9
                ? 2
                : static_cast<int>(elapsed / EnemyAnimation[type]) % 2;
            sprite(window, static_cast<std::size_t>(enemy.type + 1),
                   enemy.position, enemy.type == 2 ? 48 : 24, frame,
                   player.x < enemy.position.x);
        }

        for (const auto& bullet : bullets)
            sprite(window, 4, bullet.position, 8, 0, false);

        const int playerFrame =
            moving ? static_cast<int>(walkTime / 0.15) % 2 : 0;
        sprite(window, 0, player, 32, playerFrame, playerLeft,
               touching ? sf::Color{255, 96, 96} : sf::Color::White);

        window.setView(ui);
        rectangle(window, {0.f, 0.f}, {960.f, 6.f}, GridColor);
        const float ratio = std::clamp(
            static_cast<float>(experience) / (5.f * static_cast<float>(level)),
            0.f, 1.f);
        rectangle(window, {0.f, 0.f}, {960.f * ratio, 6.f}, Blue);

        rectangle(window, {16.f, 18.f}, {200.f, 12.f}, HpBack);
        rectangle(window, {16.f, 18.f},
                  {200.f * std::clamp(hp / maxHp, 0.f, 1.f), 12.f}, HpFill);
        text(window, "HP " + std::to_string(static_cast<int>(std::ceil(hp))),
             {16.f, 36.f}, 16);

        rightText(window, "LV " + std::to_string(level), 15.f);
        rightText(window, "TIME " + timeString(elapsed), 37.f);
        rightText(window, "KILLS " + std::to_string(kills), 59.f);

        if (paused || choosing)
        {
            rectangle(window, {0.f, 0.f}, {960.f, 540.f},
                      sf::Color{0, 0, 0, 170});
        }

        if (paused)
            text(window, "PAUSED", {480.f, 235.f}, 48, true);

        if (choosing)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float x = 80.f + 280.f * static_cast<float>(i);
                sf::RectangleShape card({240.f, 140.f});
                card.setPosition({x, 200.f});
                card.setFillColor(GridColor);
                card.setOutlineColor(Gold);
                card.setOutlineThickness(-2.f);
                window.draw(card);
                text(window, std::to_string(i + 1),
                     {x + 120.f, 217.f}, 32, true);
                text(window,
                     AbilityNames[static_cast<std::size_t>(
                         cards[static_cast<std::size_t>(i)])],
                     {x + 120.f, 285.f}, 16, true);
            }
        }
    }

private:
    Resources& assets;
    std::mt19937 random{std::random_device{}()};
    sf::VertexArray grid{sf::PrimitiveType::Lines};
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;

    sf::Vector2f player{WorldW / 2.f, WorldH / 2.f};
    float hp = 100.f;
    float maxHp = 100.f;
    float speed = 200.f;
    float magnet = 60.f;
    float damage = 10.f;
    double elapsed = 0.;
    double walkTime = 0.;
    double fireInterval = 0.8;
    double shotTimer = 0.8;
    double nextSpawn = 0.5;
    int kills = 0;
    int level = 1;
    int experience = 0;
    int projectileCount = 1;
    bool playerLeft = false;
    bool moving = false;
    bool touching = false;
    bool survived = false;
    std::array<int, 3> cards{};

    float randomFloat(float low, float high)
    {
        return std::uniform_real_distribution<float>(low, high)(random);
    }

    void spawn()
    {
        const auto camera = cameraPosition(player);
        const float left = camera.x - 544.f;
        const float right = camera.x + 544.f;
        const float top = camera.y - 334.f;
        const float bottom = camera.y + 334.f;

        struct Segment
        {
            sf::Vector2f start;
            sf::Vector2f end;
            float length;
        };
        std::array<Segment, 4> segments{};
        std::size_t count = 0;
        float total = 0.f;

        auto add = [&](sf::Vector2f a, sf::Vector2f b)
        {
            const float length = std::sqrt(lengthSquared(b - a));
            if (length > 0.f)
            {
                segments[count++] = {a, b, length};
                total += length;
            }
        };

        const float x0 = std::max(0.f, left);
        const float x1 = std::min(WorldW, right);
        const float y0 = std::max(0.f, top);
        const float y1 = std::min(WorldH, bottom);

        if (top >= 0.f && top <= WorldH)
            add({x0, top}, {x1, top});
        if (bottom >= 0.f && bottom <= WorldH)
            add({x0, bottom}, {x1, bottom});
        if (left >= 0.f && left <= WorldW)
            add({left, y0}, {left, y1});
        if (right >= 0.f && right <= WorldW)
            add({right, y0}, {right, y1});

        if (count == 0)
            return;

        float choice = randomFloat(0.f, total);
        sf::Vector2f position = segments[count - 1].end;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (choice <= segments[i].length)
            {
                position = segments[i].start +
                    (segments[i].end - segments[i].start) *
                    (choice / segments[i].length);
                break;
            }
            choice -= segments[i].length;
        }

        const int roll = std::uniform_int_distribution<int>(0, 99)(random);
        int type = 0;
        if (elapsed >= 120.)
            type = roll < 60 ? 0 : (roll < 90 ? 1 : 2);
        else if (elapsed >= 60.)
            type = roll < 70 ? 0 : 1;

        Enemy enemy;
        enemy.position = position;
        enemy.type = type;
        enemy.hp = EnemyHp[static_cast<std::size_t>(type)];
        enemies.push_back(enemy);
    }

    void fire()
    {
        const Enemy* nearest = nullptr;
        float nearestDistance = 400.f * 400.f;

        for (const auto& enemy : enemies)
        {
            if (enemy.dead)
                continue;
            const float distance = lengthSquared(enemy.position - player);
            if (distance <= nearestDistance)
            {
                nearestDistance = distance;
                nearest = &enemy;
            }
        }

        if (!nearest)
            return;

        const sf::Vector2f difference = nearest->position - player;
        const float angle = lengthSquared(difference) > 0.f
            ? std::atan2(difference.y, difference.x) : 0.f;

        for (int i = 0; i < projectileCount; ++i)
        {
            const float offset =
                (static_cast<float>(i) -
                 (static_cast<float>(projectileCount) - 1.f) * 0.5f) *
                10.f * Pi / 180.f;
            const float direction = angle + offset;
            bullets.push_back(
                {player,
                 {500.f * std::cos(direction), 500.f * std::sin(direction)},
                 damage});
        }
        assets.sound(0);
    }

    void checkLevel()
    {
        if (experience < 5 * level)
            return;

        experience -= 5 * level;
        ++level;
        choosing = true;
        std::array<int, 6> options{0, 1, 2, 3, 4, 5};
        std::shuffle(options.begin(), options.end(), random);
        std::copy_n(options.begin(), 3, cards.begin());
        assets.sound(3);
    }

    void finish(bool victory)
    {
        survived = victory;
        screen = Screen::Result;
        paused = false;
        choosing = false;
        assets.music.stop();
    }

    static void rectangle(sf::RenderTarget& target, sf::Vector2f position,
                          sf::Vector2f size, sf::Color color)
    {
        sf::RectangleShape shape(size);
        shape.setPosition(position);
        shape.setFillColor(color);
        target.draw(shape);
    }

    void text(sf::RenderTarget& target, const std::string& string,
              sf::Vector2f position, unsigned int size, bool centered = false)
    {
        sf::Text label(assets.font, string, size);
        label.setFillColor(sf::Color::White);
        if (centered)
        {
            const auto bounds = label.getLocalBounds();
            label.setOrigin(
                {bounds.position.x + bounds.size.x * 0.5f, bounds.position.y});
        }
        label.setPosition(position);
        target.draw(label);
    }

    void rightText(sf::RenderTarget& target, const std::string& string, float y)
    {
        sf::Text label(assets.font, string, 16);
        const auto bounds = label.getLocalBounds();
        label.setPosition({944.f - bounds.position.x - bounds.size.x, y});
        target.draw(label);
    }

    void sprite(sf::RenderTarget& target, std::size_t texture,
                sf::Vector2f position, int size, int frame, bool left,
                sf::Color color = sf::Color::White)
    {
        sf::Sprite image(assets.textures[texture]);
        image.setTextureRect(sf::IntRect({frame * size, 0}, {size, size}));
        const float half = static_cast<float>(size) * 0.5f;
        image.setOrigin({half, half});
        image.setPosition(position);
        image.setScale({left ? -1.f : 1.f, 1.f});
        image.setColor(color);
        target.draw(image);
    }
};

struct MovementInput
{
    static constexpr std::size_t Count =
        static_cast<std::size_t>(sf::Keyboard::Key::KeyCount);
    std::array<bool, Count> held{};
    std::array<bool, Count> taps{};

    void press(sf::Keyboard::Key key)
    {
        const int index = static_cast<int>(key);
        if (index >= 0 && static_cast<std::size_t>(index) < Count)
        {
            held[static_cast<std::size_t>(index)] = true;
            taps[static_cast<std::size_t>(index)] = true;
        }
    }

    void release(sf::Keyboard::Key key)
    {
        const int index = static_cast<int>(key);
        if (index >= 0 && static_cast<std::size_t>(index) < Count)
            held[static_cast<std::size_t>(index)] = false;
    }

    bool down(sf::Keyboard::Key key) const
    {
        const auto index = static_cast<std::size_t>(key);
        return held[index] || taps[index];
    }

    sf::Vector2f movement() const
    {
        using K = sf::Keyboard::Key;
        return {
            static_cast<float>(down(K::D) || down(K::Right)) -
                static_cast<float>(down(K::A) || down(K::Left)),
            static_cast<float>(down(K::S) || down(K::Down)) -
                static_cast<float>(down(K::W) || down(K::Up))
        };
    }

    void clear()
    {
        held.fill(false);
        taps.fill(false);
    }
};

sf::View makeUiView(sf::Vector2u size)
{
    sf::View view(sf::FloatRect({0.f, 0.f}, {960.f, 540.f}));
    if (size.x == 0 || size.y == 0)
        return view;

    const float aspect = static_cast<float>(size.x) /
                         static_cast<float>(size.y);
    constexpr float desired = 960.f / 540.f;
    if (aspect > desired)
    {
        const float width = desired / aspect;
        view.setViewport(sf::FloatRect({(1.f - width) * 0.5f, 0.f},
                                       {width, 1.f}));
    }
    else
    {
        const float height = aspect / desired;
        view.setViewport(sf::FloatRect({0.f, (1.f - height) * 0.5f},
                                       {1.f, height}));
    }
    return view;
}
}

int main()
{
    try
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "LAST STAND");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Resources resources;
        Game game(resources);
        MovementInput input;
        sf::View ui = makeUiView(window.getSize());
        sf::Clock clock;
        double accumulator = 0.;
        double titleTime = 0.;

        while (window.isOpen())
        {
            const double frameTime =
                static_cast<double>(clock.restart().asSeconds());
            bool transition = false;

            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                    window.close();

                if (const auto* resized = event->getIf<sf::Event::Resized>())
                    ui = makeUiView(resized->size);

                if (event->is<sf::Event::FocusLost>())
                    input.clear();

                if (const auto* key = event->getIf<sf::Event::KeyReleased>())
                    input.release(key->code);

                if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    using K = sf::Keyboard::Key;
                    input.press(key->code);

                    if (key->code == K::Escape)
                        window.close();
                    else if (game.screen == Screen::Title &&
                             key->code == K::Enter)
                    {
                        game.restart();
                        input.clear();
                        transition = true;
                    }
                    else if (game.screen == Screen::Result &&
                             key->code == K::R)
                    {
                        game.restart();
                        input.clear();
                        transition = true;
                    }
                    else if (game.screen == Screen::Playing)
                    {
                        if (game.choosing)
                        {
                            int selection = -1;
                            if (key->code == K::Num1 || key->code == K::Numpad1)
                                selection = 0;
                            if (key->code == K::Num2 || key->code == K::Numpad2)
                                selection = 1;
                            if (key->code == K::Num3 || key->code == K::Numpad3)
                                selection = 2;
                            if (selection >= 0)
                            {
                                game.select(selection);
                                input.taps.fill(false);
                                transition = true;
                            }
                        }
                        else if (key->code == K::P)
                        {
                            game.togglePause();
                            input.taps.fill(false);
                            transition = true;
                        }
                    }
                }

                if (const auto* mouse =
                        event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button == sf::Mouse::Button::Left &&
                        game.choosing)
                    {
                        const auto viewport = window.getViewport(ui);
                        if (viewport.contains(mouse->position))
                        {
                            game.click(window.mapPixelToCoords(
                                mouse->position, ui));
                            input.taps.fill(false);
                            transition = true;
                        }
                    }
                }
            }

            if (!window.isOpen())
                break;

            if (game.screen == Screen::Title)
            {
                titleTime += frameTime;
                if (titleTime >= 2.)
                {
                    game.restart();
                    input.clear();
                    transition = true;
                }
            }

            if (game.screen == Screen::Playing &&
                !game.paused && !game.choosing && !transition)
            {
                accumulator += frameTime;
                while (accumulator >= Tick)
                {
                    game.update(static_cast<float>(Tick), input.movement());
                    input.taps.fill(false);
                    accumulator -= Tick;

                    if (game.screen != Screen::Playing ||
                        game.paused || game.choosing)
                    {
                        accumulator = 0.;
                        break;
                    }
                }
            }
            else
            {
                accumulator = 0.;
                input.taps.fill(false);
            }

            window.clear(sf::Color::Black);

            window.setView(ui);
            sf::RectangleShape background({960.f, 540.f});
            background.setFillColor(Background);
            window.draw(background);

            game.draw(window, ui);
            window.display();
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
