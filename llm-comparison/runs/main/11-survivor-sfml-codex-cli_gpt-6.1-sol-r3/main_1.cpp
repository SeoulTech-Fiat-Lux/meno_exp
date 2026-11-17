#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
constexpr float ScreenW = 960.f;
constexpr float ScreenH = 540.f;
constexpr float WorldW = 2880.f;
constexpr float WorldH = 1620.f;
constexpr double Step = 1.0 / 120.0;
constexpr float Pi = 3.14159265358979323846f;

const sf::Color Background{26, 20, 32};
const sf::Color GridColor{42, 34, 51};
const sf::Color BorderColor{107, 91, 123};
const sf::Color Blue{79, 195, 247};
const sf::Color Gold{255, 213, 74};

float lengthSquared(sf::Vector2f v)
{
    return v.x * v.x + v.y * v.y;
}

sf::Vector2f unit(sf::Vector2f v)
{
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.f ? v / length : sf::Vector2f{};
}

void constrain(sf::Vector2f& p, float radius)
{
    p.x = std::clamp(p.x, radius, WorldW - radius);
    p.y = std::clamp(p.y, radius, WorldH - radius);
}

bool overlap(sf::Vector2f a, sf::Vector2f b, float radius)
{
    return lengthSquared(a - b) <= radius * radius;
}

// Returns the first contact along a segment, including an initial overlap.
float segmentHit(sf::Vector2f start, sf::Vector2f end,
                 sf::Vector2f center, float radius)
{
    const sf::Vector2f d = end - start;
    const sf::Vector2f m = start - center;
    const float c = lengthSquared(m) - radius * radius;
    if (c <= 0.f)
        return 0.f;

    const float a = lengthSquared(d);
    if (a <= 0.f)
        return std::numeric_limits<float>::infinity();

    const float b = m.x * d.x + m.y * d.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.f)
        return std::numeric_limits<float>::infinity();

    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.f && t <= 1.f
               ? t
               : std::numeric_limits<float>::infinity();
}

std::string timeString(double seconds)
{
    const int total = static_cast<int>(std::floor(seconds + 0.000001));
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << total / 60
        << ':' << std::setw(2) << total % 60;
    return out.str();
}

struct Assets
{
    sf::Font font;
    std::array<sf::Texture, 6> textures;
    std::array<sf::SoundBuffer, 4> buffers;
    sf::Music music;

    bool load()
    {
        const std::string fontPath = "assets/fonts/game.ttf";
        if (!font.openFromFile(fontPath))
        {
            std::cerr << fontPath << '\n';
            return false;
        }

        const std::array<std::string, 6> imagePaths{
            "assets/textures/sv_player.png",
            "assets/textures/sv_walker.png",
            "assets/textures/sv_runner.png",
            "assets/textures/sv_brute.png",
            "assets/textures/sv_bullet.png",
            "assets/textures/sv_gem.png"
        };

        for (std::size_t i = 0; i < textures.size(); ++i)
        {
            if (!textures[i].loadFromFile(imagePaths[i]))
            {
                std::cerr << imagePaths[i] << '\n';
                return false;
            }
        }

        const std::array<std::string, 4> soundPaths{
            "assets/sfx/shoot.wav",
            "assets/sfx/hit.wav",
            "assets/sfx/pickup.wav",
            "assets/sfx/levelup.wav"
        };

        for (std::size_t i = 0; i < buffers.size(); ++i)
        {
            if (!buffers[i].loadFromFile(soundPaths[i]))
            {
                std::cerr << soundPaths[i] << '\n';
                return false;
            }
        }

        const std::string musicPath = "assets/music/bgm_survivor.ogg";
        if (!music.openFromFile(musicPath))
        {
            std::cerr << musicPath << '\n';
            return false;
        }

        music.setVolume(40.f);
        music.setLooping(true);
        return true;
    }
};

struct Enemy
{
    sf::Vector2f position;
    sf::Vector2f knockDirection{};
    int type = 0;
    float hp = 10.f;
    float hitTime = 0.f;
    float knockTime = 0.f;
    float deathTime = 0.f;
    bool dead = false;
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
    int value = 1;
    float life = 30.f;
    bool attracted = false;
};

constexpr std::array<float, 3> EnemyRadius{10.f, 10.f, 20.f};
constexpr std::array<float, 3> EnemyHP{10.f, 6.f, 60.f};
constexpr std::array<float, 3> EnemySpeed{80.f, 140.f, 50.f};
constexpr std::array<float, 3> EnemyDamage{10.f, 6.f, 20.f};
constexpr std::array<float, 3> EnemyFrameTime{0.3f, 0.15f, 0.4f};
constexpr std::array<int, 3> EnemyCell{24, 24, 48};

constexpr std::array<const char*, 6> AbilityNames{
    "FIRE RATE +15%",
    "DAMAGE +5",
    "PROJECTILE +1",
    "SPEED +10%",
    "MAX HP +20",
    "MAGNET +40"
};

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

class Game
{
public:
    explicit Game(Assets& resources)
        : assets(resources),
          window(sf::VideoMode({960, 540}), "LAST STAND"),
          uiView(sf::FloatRect({0.f, 0.f}, {ScreenW, ScreenH})),
          worldView({WorldW / 2.f, WorldH / 2.f}, {ScreenW, ScreenH}),
          rng(std::random_device{}())
    {
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        for (int i = 0; i < 48; ++i)
            sounds.push_back(std::make_unique<sf::Sound>(assets.buffers[0]));

        enemies.reserve(400);
        bullets.reserve(128);
        gems.reserve(512);

        grid.setPrimitiveType(sf::PrimitiveType::Lines);
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

        resize(window.getSize());
    }

    void run()
    {
        sf::Clock clock;
        while (window.isOpen())
        {
            const double elapsed = clock.restart().asSeconds();
            handleEvents();
            if (!window.isOpen())
                break;

            if (state == State::Title)
            {
                titleTime += elapsed;
                if (titleTime >= 2.0)
                    restart();
            }
            else if (state == State::Play && !paused && !choosing)
            {
                accumulator += elapsed;
                while (accumulator >= Step &&
                       state == State::Play && !paused && !choosing)
                {
                    accumulator -= Step;
                    update(static_cast<float>(Step));
                    tapped.fill(false);
                }
                if (state != State::Play || paused || choosing)
                    accumulator = 0.0;
            }
            else
            {
                accumulator = 0.0;
            }

            render();
        }
    }

private:
    enum class State { Title, Play, Result };

    Assets& assets;
    sf::RenderWindow window;
    sf::View uiView;
    sf::View worldView;
    sf::VertexArray grid;
    std::mt19937 rng;
    std::vector<std::unique_ptr<sf::Sound>> sounds;
    std::size_t nextSound = 0;

    State state = State::Title;
    bool paused = false;
    bool choosing = false;
    bool survived = false;
    bool facingLeft = false;
    bool touching = false;
    bool moving = false;

    std::array<bool, 8> held{};
    std::array<bool, 8> tapped{};
    std::array<int, 3> choices{};

    sf::Vector2f player{WorldW / 2.f, WorldH / 2.f};
    float hp = 100.f;
    float maxHP = 100.f;
    float speed = 200.f;
    float damage = 10.f;
    float magnet = 60.f;
    double fireInterval = 0.8;
    double fireTime = 0.0;
    int projectiles = 1;
    int level = 1;
    int experience = 0;
    int kills = 0;

    double playTime = 0.0;
    double titleTime = 0.0;
    double nextSpawn = 0.5;
    double accumulator = 0.0;
    double walkTime = 0.0;

    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;

    void sound(int index)
    {
        std::size_t slot = nextSound;
        for (std::size_t i = 0; i < sounds.size(); ++i)
        {
            const std::size_t candidate = (nextSound + i) % sounds.size();
            if (sounds[candidate]->getStatus() == sf::SoundSource::Status::Stopped)
            {
                slot = candidate;
                break;
            }
        }
        auto& voice = *sounds[slot];
        voice.stop();
        voice.setBuffer(assets.buffers[static_cast<std::size_t>(index)]);
        voice.play();
        nextSound = (slot + 1) % sounds.size();
    }

    void resize(sf::Vector2u size)
    {
        if (size.x == 0 || size.y == 0)
            return;

        const float aspect = static_cast<float>(size.x) /
                             static_cast<float>(size.y);
        constexpr float target = ScreenW / ScreenH;
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
    }

    void updateCamera()
    {
        worldView.setCenter({
            std::clamp(player.x, ScreenW / 2.f, WorldW - ScreenW / 2.f),
            std::clamp(player.y, ScreenH / 2.f, WorldH - ScreenH / 2.f)
        });
    }

    void restart()
    {
        for (auto& voice : sounds)
            voice->stop();

        state = State::Play;
        paused = choosing = survived = false;
        facingLeft = touching = moving = false;
        held.fill(false);
        tapped.fill(false);
        player = {WorldW / 2.f, WorldH / 2.f};
        hp = maxHP = 100.f;
        speed = 200.f;
        damage = 10.f;
        magnet = 60.f;
        fireInterval = 0.8;
        fireTime = playTime = walkTime = accumulator = 0.0;
        nextSpawn = 0.5;
        projectiles = level = 1;
        experience = kills = 0;
        enemies.clear();
        bullets.clear();
        gems.clear();
        updateCamera();

        assets.music.stop();
        assets.music.play();
    }

    void syncMusic()
    {
        if (state != State::Play)
            return;
        if (paused || choosing)
            assets.music.pause();
        else
            assets.music.play();
    }

    void handleEvents()
    {
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* resized = event->getIf<sf::Event::Resized>())
                resize(resized->size);

            if (event->is<sf::Event::FocusLost>())
            {
                held.fill(false);
                tapped.fill(false);
                if (state == State::Play)
                {
                    paused = true;
                    accumulator = 0.0;
                    syncMusic();
                }
            }

            if (const auto* release = event->getIf<sf::Event::KeyReleased>())
            {
                const int index = movementIndex(release->code);
                if (index >= 0)
                    held[static_cast<std::size_t>(index)] = false;
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                using K = sf::Keyboard::Key;
                if (key->code == K::Escape)
                {
                    window.close();
                    continue;
                }

                if (state == State::Title && key->code == K::Enter)
                {
                    restart();
                    continue;
                }
                if (state == State::Result && key->code == K::R)
                {
                    restart();
                    continue;
                }
                if (state != State::Play)
                    continue;

                const int index = movementIndex(key->code);
                if (index >= 0)
                {
                    held[static_cast<std::size_t>(index)] = true;
                    if (!paused && !choosing)
                        tapped[static_cast<std::size_t>(index)] = true;
                }

                if (key->code == K::P)
                {
                    paused = !paused;
                    tapped.fill(false);
                    accumulator = 0.0;
                    syncMusic();
                }
                else if (choosing && !paused)
                {
                    if (key->code == K::Num1 || key->code == K::Numpad1)
                        choose(0);
                    else if (key->code == K::Num2 || key->code == K::Numpad2)
                        choose(1);
                    else if (key->code == K::Num3 || key->code == K::Numpad3)
                        choose(2);
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (state == State::Play && choosing && !paused &&
                    mouse->button == sf::Mouse::Button::Left)
                {
                    const sf::Vector2f p =
                        window.mapPixelToCoords(mouse->position, uiView);
                    for (int i = 0; i < 3; ++i)
                    {
                        const sf::FloatRect card(
                            {80.f + 280.f * static_cast<float>(i), 200.f},
                            {240.f, 140.f});
                        if (card.contains(p))
                        {
                            choose(i);
                            break;
                        }
                    }
                }
            }
        }
    }

    bool beginLevel()
    {
        if (experience < 5 * level)
            return false;

        experience -= 5 * level;
        ++level;
        std::array<int, 6> available{0, 1, 2, 3, 4, 5};
        std::shuffle(available.begin(), available.end(), rng);
        std::copy_n(available.begin(), 3, choices.begin());
        choosing = true;
        accumulator = 0.0;
        tapped.fill(false);
        sound(3);
        syncMusic();
        return true;
    }

    void choose(int card)
    {
        switch (choices[static_cast<std::size_t>(card)])
        {
            case 0: fireInterval *= 0.85; break;
            case 1: damage += 5.f; break;
            case 2: ++projectiles; break;
            case 3: speed *= 1.1f; break;
            case 4:
                maxHP += 20.f;
                hp = std::min(maxHP, hp + 20.f);
                break;
            case 5: magnet += 40.f; break;
        }

        choosing = false;
        tapped.fill(false);
        accumulator = 0.0;
        if (!beginLevel())
            syncMusic();
    }

    void finish(bool win)
    {
        survived = win;
        state = State::Result;
        paused = choosing = false;
        accumulator = 0.0;
        held.fill(false);
        tapped.fill(false);
        assets.music.stop();
    }

    void spawnEnemy()
    {
        // Clip each perimeter segment to the world, then sample by length.
        const sf::Vector2f center = worldView.getCenter();
        const float left = center.x - ScreenW / 2.f - 64.f;
        const float right = center.x + ScreenW / 2.f + 64.f;
        const float top = center.y - ScreenH / 2.f - 64.f;
        const float bottom = center.y + ScreenH / 2.f + 64.f;

        struct Segment
        {
            sf::Vector2f a;
            sf::Vector2f b;
            float length;
        };
        std::array<Segment, 4> segments{};
        int count = 0;
        float total = 0.f;

        const auto add = [&](sf::Vector2f a, sf::Vector2f b)
        {
            const float distance = std::sqrt(lengthSquared(b - a));
            if (distance > 0.f)
            {
                segments[static_cast<std::size_t>(count++)] = {a, b, distance};
                total += distance;
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

        if (total <= 0.f)
            return;

        float distance = std::uniform_real_distribution<float>(0.f, total)(rng);
        sf::Vector2f position{};
        for (int i = 0; i < count; ++i)
        {
            const auto& segment = segments[static_cast<std::size_t>(i)];
            if (distance <= segment.length || i == count - 1)
            {
                position = segment.a +
                           (segment.b - segment.a) *
                               std::clamp(distance / segment.length, 0.f, 1.f);
                break;
            }
            distance -= segment.length;
        }

        const int roll = std::uniform_int_distribution<int>(0, 99)(rng);
        int type = 0;
        if (playTime >= 120.0)
            type = roll < 60 ? 0 : (roll < 90 ? 1 : 2);
        else if (playTime >= 60.0)
            type = roll < 70 ? 0 : 1;

        Enemy enemy;
        enemy.position = position;
        enemy.type = type;
        enemy.hp = EnemyHP[static_cast<std::size_t>(type)];
        enemies.push_back(enemy);
    }

    void shoot()
    {
        const Enemy* target = nullptr;
        float nearest = 400.f * 400.f;
        for (const auto& enemy : enemies)
        {
            if (enemy.dead)
                continue;
            const float distance = lengthSquared(enemy.position - player);
            if (distance <= nearest)
            {
                nearest = distance;
                target = &enemy;
            }
        }

        if (!target)
            return;

        const sf::Vector2f direction = target->position - player;
        const float angle = std::atan2(direction.y, direction.x);
        for (int i = 0; i < projectiles; ++i)
        {
            const float offset =
                (static_cast<float>(i) -
                 static_cast<float>(projectiles - 1) * 0.5f) * 10.f * Pi / 180.f;
            const float a = angle + offset;
            bullets.push_back({
                player,
                {std::cos(a) * 500.f, std::sin(a) * 500.f},
                1.2f,
                damage
            });
        }
        sound(0);
    }

    void update(float dt)
    {
        playTime = std::min(300.0, playTime + Step);

        const auto down = [&](int index)
        {
            const auto i = static_cast<std::size_t>(index);
            return held[i] || tapped[i];
        };

        sf::Vector2f direction{
            static_cast<float>(down(6) || down(7)) -
                static_cast<float>(down(4) || down(5)),
            static_cast<float>(down(2) || down(3)) -
                static_cast<float>(down(0) || down(1))
        };

        const sf::Vector2f previous = player;
        player += unit(direction) * speed * dt;
        constrain(player, 14.f);
        moving = lengthSquared(player - previous) > 0.f;
        if (direction.x != 0.f)
            facingLeft = direction.x < 0.f;
        if (moving)
            walkTime += Step;
        else
            walkTime = 0.0;

        updateCamera();

        while (playTime + 0.0000001 >= nextSpawn)
        {
            if (enemies.size() < 400)
                spawnEnemy();

            const double interval = std::max(
                0.15, 0.6 - 0.05 * std::floor(nextSpawn / 15.0));
            nextSpawn += interval;
        }

        for (auto& enemy : enemies)
        {
            if (enemy.dead)
            {
                enemy.deathTime -= dt;
                continue;
            }

            enemy.hitTime = std::max(0.f, enemy.hitTime - dt);
            float remaining = dt;
            if (enemy.knockTime > 0.f)
            {
                const float knockStep = std::min(remaining, enemy.knockTime);
                enemy.position += enemy.knockDirection * (120.f * knockStep);
                enemy.knockTime = std::max(0.f, enemy.knockTime - knockStep);
                remaining -= knockStep;
            }
            if (remaining > 0.f)
            {
                enemy.position += unit(player - enemy.position) *
                                  EnemySpeed[static_cast<std::size_t>(enemy.type)] *
                                  remaining;
            }
            constrain(enemy.position,
                      EnemyRadius[static_cast<std::size_t>(enemy.type)]);
        }

        std::erase_if(enemies, [](const Enemy& enemy)
        {
            return enemy.dead && enemy.deathTime <= 0.f;
        });

        fireTime += Step;
        while (fireTime >= fireInterval)
        {
            fireTime -= fireInterval;
            shoot();
        }

        for (auto& bullet : bullets)
        {
            const float travelTime = std::min(dt, bullet.life);
            const sf::Vector2f end =
                bullet.position + bullet.velocity * travelTime;

            Enemy* hitEnemy = nullptr;
            float earliest = std::numeric_limits<float>::infinity();

            for (auto& enemy : enemies)
            {
                if (enemy.dead)
                    continue;
                const float t = segmentHit(
                    bullet.position, end, enemy.position,
                    4.f + EnemyRadius[static_cast<std::size_t>(enemy.type)]);
                if (t < earliest)
                {
                    earliest = t;
                    hitEnemy = &enemy;
                }
            }

            if (hitEnemy)
            {
                bullet.position += (end - bullet.position) * earliest;
                bullet.life = 0.f;
                hitEnemy->hp -= bullet.damage;
                hitEnemy->hitTime = 0.1f;

                if (hitEnemy->hp <= 0.f)
                {
                    hitEnemy->dead = true;
                    hitEnemy->deathTime = 0.1f;
                    gems.push_back({
                        hitEnemy->position,
                        hitEnemy->type == 2 ? 5 : 1,
                        30.f,
                        false
                    });
                    ++kills;
                    sound(1);
                }
                else
                {
                    hitEnemy->knockDirection = unit(bullet.velocity);
                    hitEnemy->knockTime = 0.1f;
                }
            }
            else
            {
                bullet.position = end;
                bullet.life -= dt;
            }
        }

        std::erase_if(bullets, [](const Bullet& bullet)
        {
            return bullet.life <= 0.f;
        });

        touching = false;
        for (const auto& enemy : enemies)
        {
            const auto type = static_cast<std::size_t>(enemy.type);
            if (!enemy.dead &&
                overlap(player, enemy.position, 14.f + EnemyRadius[type]))
            {
                hp -= EnemyDamage[type] * dt;
                touching = true;
            }
        }

        if (hp <= 0.f)
        {
            hp = 0.f;
            finish(false);
            return;
        }
        if (playTime >= 300.0)
        {
            finish(true);
            return;
        }

        for (auto& gem : gems)
        {
            gem.life -= dt;
            if (gem.life <= 0.f)
                continue;

            if (overlap(player, gem.position, magnet))
                gem.attracted = true;

            if (gem.attracted)
            {
                const sf::Vector2f delta = player - gem.position;
                const float distance = std::sqrt(lengthSquared(delta));
                if (distance > 0.f)
                    gem.position += delta / distance *
                                    std::min(300.f * dt, distance);
            }

            if (overlap(player, gem.position, 20.f))
            {
                experience += gem.value;
                gem.life = 0.f;
                sound(2);
            }
        }

        std::erase_if(gems, [](const Gem& gem)
        {
            return gem.life <= 0.f;
        });

        beginLevel();
    }

    void rectangle(sf::Vector2f position, sf::Vector2f size,
                   sf::Color fill, float outline = 0.f,
                   sf::Color outlineColor = sf::Color::Transparent)
    {
        sf::RectangleShape shape(size);
        shape.setPosition(position);
        shape.setFillColor(fill);
        shape.setOutlineThickness(outline);
        shape.setOutlineColor(outlineColor);
        window.draw(shape);
    }

    void text(const std::string& value, unsigned int size,
              sf::Vector2f position, bool centered = false)
    {
        sf::Text label(assets.font, value, size);
        label.setFillColor(sf::Color::White);
        if (centered)
        {
            const sf::FloatRect bounds = label.getLocalBounds();
            label.setOrigin({
                bounds.position.x + bounds.size.x / 2.f,
                bounds.position.y + bounds.size.y / 2.f
            });
        }
        label.setPosition(position);
        window.draw(label);
    }

    void sprite(int texture, int cell, int frame,
                sf::Vector2f position, bool flip = false,
                sf::Color color = sf::Color::White)
    {
        sf::Sprite image(assets.textures[static_cast<std::size_t>(texture)]);
        image.setTextureRect(sf::IntRect({frame * cell, 0}, {cell, cell}));
        image.setOrigin({
            static_cast<float>(cell) / 2.f,
            static_cast<float>(cell) / 2.f
        });
        image.setPosition(position);
        image.setScale({flip ? -1.f : 1.f, 1.f});
        image.setColor(color);
        window.draw(image);
    }

    bool visible(sf::Vector2f position, float radius) const
    {
        const sf::Vector2f d = position - worldView.getCenter();
        return std::abs(d.x) <= ScreenW / 2.f + radius &&
               std::abs(d.y) <= ScreenH / 2.f + radius;
    }

    void drawWorld()
    {
        window.setView(worldView);
        rectangle({0.f, 0.f}, {WorldW, WorldH}, Background);
        window.draw(grid);
        rectangle({1.f, 1.f}, {WorldW - 2.f, WorldH - 2.f},
                  sf::Color::Transparent, 2.f, BorderColor);

        for (const auto& gem : gems)
        {
            if (visible(gem.position, 6.f))
                sprite(5, 12, 0, gem.position, false,
                       gem.value == 5 ? Gold : Blue);
        }

        for (const auto& enemy : enemies)
        {
            const auto type = static_cast<std::size_t>(enemy.type);
            if (!visible(enemy.position,
                         static_cast<float>(EnemyCell[type]) / 2.f))
                continue;

            const int frame = enemy.dead || enemy.hitTime > 0.f
                                  ? 2
                                  : static_cast<int>(
                                        playTime / EnemyFrameTime[type]) % 2;
            sprite(enemy.type + 1, EnemyCell[type], frame,
                   enemy.position, player.x < enemy.position.x);
        }

        for (const auto& bullet : bullets)
        {
            if (visible(bullet.position, 4.f))
                sprite(4, 8, 0, bullet.position);
        }

        const int playerFrame =
            moving ? static_cast<int>(walkTime / 0.15) % 2 : 0;
        sprite(0, 32, playerFrame, player, facingLeft,
               touching ? sf::Color{255, 96, 96} : sf::Color::White);
    }

    void drawHUD()
    {
        window.setView(uiView);
        rectangle({0.f, 0.f}, {ScreenW, 6.f}, GridColor);
        const float xpRatio = std::clamp(
            static_cast<float>(experience) / static_cast<float>(5 * level),
            0.f, 1.f);
        rectangle({0.f, 0.f}, {ScreenW * xpRatio, 6.f}, Blue);

        rectangle({16.f, 18.f}, {200.f, 12.f}, sf::Color{58, 31, 36});
        rectangle({16.f, 18.f},
                  {200.f * std::clamp(hp / maxHP, 0.f, 1.f), 12.f},
                  sf::Color{229, 83, 61});
        text("HP " + std::to_string(static_cast<int>(std::ceil(hp))),
             16, {16.f, 36.f});

        text("LV " + std::to_string(level), 16, {704.f, 16.f});
        text("TIME " + timeString(playTime), 16, {704.f, 40.f});
        text("KILLS " + std::to_string(kills), 16, {704.f, 64.f});
    }

    void render()
    {
        window.clear(sf::Color::Black);

        if (state == State::Title)
        {
            window.setView(uiView);
            rectangle({0.f, 0.f}, {ScreenW, ScreenH}, Background);
            text("LAST STAND", 48, {480.f, 220.f}, true);
            text("PRESS ENTER", 32, {480.f, 320.f}, true);
        }
        else if (state == State::Result)
        {
            window.setView(uiView);
            rectangle({0.f, 0.f}, {ScreenW, ScreenH}, Background);
            text(survived ? "SURVIVED" : "GAME OVER",
                 48, {480.f, 140.f}, true);
            text("TIME " + timeString(playTime), 32, {480.f, 230.f}, true);
            text("LEVEL " + std::to_string(level),
                 32, {480.f, 280.f}, true);
            text("KILLS " + std::to_string(kills),
                 32, {480.f, 330.f}, true);
            text("R: RESTART  ESC: QUIT",
                 32, {480.f, 430.f}, true);
        }
        else
        {
            drawWorld();
            drawHUD();

            if (choosing)
            {
                for (int i = 0; i < 3; ++i)
                {
                    const float x = 80.f + 280.f * static_cast<float>(i);
                    // Inward outline keeps the complete card within 240x140.
                    rectangle({x, 200.f}, {240.f, 140.f},
                              GridColor, -2.f, Gold);
                    text(std::to_string(i + 1),
                         32, {x + 120.f, 235.f}, true);
                    text(AbilityNames[static_cast<std::size_t>(
                             choices[static_cast<std::size_t>(i)])],
                         16, {x + 120.f, 290.f}, true);
                }
            }

            if (paused)
                text("PAUSED", 48, {480.f, 140.f}, true);
        }

        window.display();
    }
};
} // namespace

int main()
{
    try
    {
        Assets assets;
        if (!assets.load())
            return 1;

        Game game(assets);
        game.run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
