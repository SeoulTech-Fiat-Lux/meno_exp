#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float Tick = 1.f / 120.f;
constexpr float Infinity = std::numeric_limits<float>::infinity();

struct Assets
{
    sf::Font font;
    sf::Texture enemies, player, bonus;
    sf::SoundBuffer shoot, hit, lose, bonusSound;
    sf::Music music;

    bool load()
    {
        const auto checked = [](const char* path, auto&& operation)
        {
            try
            {
                if (operation())
                    return true;
            }
            catch (const std::exception&)
            {
            }
            std::cerr << "FAILED TO LOAD: " << path << '\n';
            return false;
        };

        return
            checked("assets/fonts/game.ttf", [&] {
                return font.openFromFile("assets/fonts/game.ttf");
            }) &&
            checked("assets/textures/fm_enemies.png", [&] {
                return enemies.loadFromFile("assets/textures/fm_enemies.png");
            }) &&
            checked("assets/textures/fm_player.png", [&] {
                return player.loadFromFile("assets/textures/fm_player.png");
            }) &&
            checked("assets/textures/fm_bonus.png", [&] {
                return bonus.loadFromFile("assets/textures/fm_bonus.png");
            }) &&
            checked("assets/sfx/shoot.wav", [&] {
                return shoot.loadFromFile("assets/sfx/shoot.wav");
            }) &&
            checked("assets/sfx/hit.wav", [&] {
                return hit.loadFromFile("assets/sfx/hit.wav");
            }) &&
            checked("assets/sfx/lose.wav", [&] {
                return lose.loadFromFile("assets/sfx/lose.wav");
            }) &&
            checked("assets/sfx/bonus.wav", [&] {
                return bonusSound.loadFromFile("assets/sfx/bonus.wav");
            }) &&
            checked("assets/music/bgm_formation.ogg", [&] {
                return music.openFromFile("assets/music/bgm_formation.ogg");
            });
    }
};

sf::FloatRect expanded(sf::FloatRect rect, sf::Vector2f amount)
{
    return {rect.position - amount, rect.size + amount * 2.f};
}

// Earliest intersection of a segment and a closed rectangle.
float segmentRect(sf::Vector2f start, sf::Vector2f delta,
                  const sf::FloatRect& rect)
{
    float first = 0.f;
    float last = 1.f;

    const auto axis = [&](float p, float d, float low, float high)
    {
        if (std::abs(d) < 0.000001f)
            return p >= low && p <= high;

        float a = (low - p) / d;
        float b = (high - p) / d;
        if (a > b)
            std::swap(a, b);

        first = std::max(first, a);
        last = std::min(last, b);
        return first <= last;
    };

    if (!axis(start.x, delta.x, rect.position.x,
              rect.position.x + rect.size.x) ||
        !axis(start.y, delta.y, rect.position.y,
              rect.position.y + rect.size.y))
        return Infinity;

    return first;
}

float segmentCircle(sf::Vector2f start, sf::Vector2f delta,
                    sf::Vector2f center, float radius)
{
    const auto offset = start - center;
    const float c = offset.x * offset.x + offset.y * offset.y -
                    radius * radius;
    if (c <= 0.f)
        return 0.f;

    const float a = delta.x * delta.x + delta.y * delta.y;
    if (a <= 0.f)
        return Infinity;

    const float b = 2.f * (offset.x * delta.x + offset.y * delta.y);
    const float discriminant = b * b - 4.f * a * c;
    if (discriminant < 0.f)
        return Infinity;

    const float t = (-b - std::sqrt(discriminant)) / (2.f * a);
    return t >= 0.f && t <= 1.f ? t : Infinity;
}

// Exact swept circle-versus-axis-aligned-rectangle collision.
float sweptCircle(sf::Vector2f start, sf::Vector2f delta,
                  sf::FloatRect rect, float radius)
{
    float best = std::min(
        segmentRect(start, delta,
                    {{rect.position.x - radius, rect.position.y},
                     {rect.size.x + radius * 2.f, rect.size.y}}),
        segmentRect(start, delta,
                    {{rect.position.x, rect.position.y - radius},
                     {rect.size.x, rect.size.y + radius * 2.f}}));

    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            best = std::min(best, segmentCircle(
                start, delta,
                rect.position + sf::Vector2f{
                    rect.size.x * static_cast<float>(x),
                    rect.size.y * static_cast<float>(y)},
                radius));

    return best;
}

struct Enemy
{
    sf::Vector2f position;
    int row;
    int column;
    bool alive = true;

    sf::FloatRect bounds() const
    {
        return {position, {32.f, 24.f}};
    }
};

struct WallPiece
{
    sf::Vector2f position;
    bool alive = true;

    sf::FloatRect bounds() const
    {
        return {position, {8.f, 8.f}};
    }
};

struct EnemyBullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
    bool alive = true;
};

enum class Screen { Title, Play, Result };

class Game
{
public:
    Game(sf::RenderWindow& window, Assets& assets)
        : window(window),
          assets(assets),
          shootSound(assets.shoot),
          hitSound(assets.hit),
          loseSound(assets.lose),
          bonusSound(assets.bonusSound),
          random(std::random_device{}())
    {
        assets.music.setVolume(40.f);
        assets.music.setLooping(true);
    }

    void handleEvent(const sf::Event& event)
    {
        if (event.is<sf::Event::Closed>())
            window.close();

        if (event.is<sf::Event::FocusLost>())
        {
            leftHeld = rightHeld = aHeld = dHeld = false;
            leftTap = rightTap = false;
        }

        if (const auto* key = event.getIf<sf::Event::KeyPressed>())
        {
            using Key = sf::Keyboard::Key;
            switch (key->code)
            {
                case Key::Escape: window.close(); break;
                case Key::Enter: enterPending = true; break;
                case Key::R: restartPending = true; break;
                case Key::Space: shotPending = true; break;
                case Key::Left: leftHeld = true; leftTap = true; break;
                case Key::A: aHeld = true; leftTap = true; break;
                case Key::Right: rightHeld = true; rightTap = true; break;
                case Key::D: dHeld = true; rightTap = true; break;
                default: break;
            }
        }

        if (const auto* key = event.getIf<sf::Event::KeyReleased>())
        {
            using Key = sf::Keyboard::Key;
            switch (key->code)
            {
                case Key::Left: leftHeld = false; break;
                case Key::A: aHeld = false; break;
                case Key::Right: rightHeld = false; break;
                case Key::D: dHeld = false; break;
                default: break;
            }
        }
    }

    void update(float dt)
    {
        const bool enter = enterPending;
        const bool restart = restartPending;
        const bool shoot = shotPending;
        const bool left = leftHeld || aHeld || leftTap;
        const bool right = rightHeld || dHeld || rightTap;

        enterPending = restartPending = shotPending = false;
        leftTap = rightTap = false;

        if (screen == Screen::Title)
        {
            titleTime += dt;
            if (enter || titleTime >= 2.0)
                startGame();
            return;
        }

        if (screen == Screen::Result)
        {
            if (restart)
                startGame();
            return;
        }

        if (freezeTime > 0.f)
        {
            freezeTime = std::max(0.f, freezeTime - dt);
            return;
        }

        playerX = std::clamp(
            playerX + (static_cast<int>(right) - static_cast<int>(left)) *
                          300.f * dt,
            16.f, 904.f);

        if (shoot && !playerBullet)
        {
            playerBullet = sf::Vector2f{playerX + 20.f, 484.f};
            shootSound.play();
        }

        animationTime += dt;
        moveFormation(dt);

        for (const auto& enemy : enemies)
        {
            if (!enemy.alive)
                continue;

            if (enemy.position.y + 24.f >= 490.f)
            {
                finishGame();
                return;
            }

            for (auto& wall : walls)
                if (wall.alive &&
                    enemy.bounds().findIntersection(wall.bounds()))
                    wall.alive = false;
        }

        bonusTimer += dt;
        if (bonusTimer >= 20.0)
        {
            bonusTimer -= 20.0;
            bonusActive = true;
            bonusX = nextBonusFromLeft ? -48.f : 960.f;
            bonusDirection = nextBonusFromLeft ? 1.f : -1.f;
            nextBonusFromLeft = !nextBonusFromLeft;
        }

        if (bonusActive)
        {
            bonusX += bonusDirection * 120.f * dt;
            if ((bonusDirection > 0.f && bonusX >= 960.f) ||
                (bonusDirection < 0.f && bonusX + 48.f <= 0.f))
                bonusActive = false;
        }

        fireTimer += dt;
        if (fireTimer >= 1.0)
        {
            fireTimer -= 1.0;
            fireEnemyBullet();
        }

        updatePlayerBullet(dt);

        if (std::none_of(enemies.begin(), enemies.end(),
                         [](const Enemy& enemy) { return enemy.alive; }))
        {
            ++wave;
            startWave();
            return;
        }

        updateEnemyBullets(dt);

        std::erase_if(walls, [](const WallPiece& wall) {
            return !wall.alive;
        });
        std::erase_if(enemies, [](const Enemy& enemy) {
            return !enemy.alive;
        });
    }

    void draw()
    {
        window.clear(sf::Color::Black);

        if (screen == Screen::Title)
        {
            centered("STAR FORMATION", 48, 175.f);
            centered("PRESS ENTER", 32, 285.f);
        }
        else if (screen == Screen::Result)
        {
            centered("GAME OVER", 48, 130.f);
            centered("SCORE " + std::to_string(score), 32, 225.f);
            centered("WAVE " + std::to_string(wave), 32, 275.f);
            centered("R: RESTART  ESC: QUIT", 32, 365.f);
        }
        else
        {
            sf::RectangleShape piece({8.f, 8.f});
            piece.setFillColor(sf::Color(0x4C, 0xC2, 0x6B));
            for (const auto& wall : walls)
            {
                if (!wall.alive)
                    continue;
                piece.setPosition(wall.position);
                window.draw(piece);
            }

            sf::Sprite enemySprite(assets.enemies);
            const int animation = static_cast<int>(
                std::fmod(animationTime, 1.0) >= 0.5);
            for (const auto& enemy : enemies)
            {
                if (!enemy.alive)
                    continue;
                const int base = enemy.row == 0 ? 0 :
                                 enemy.row == 1 ? 2 : 4;
                enemySprite.setTextureRect(
                    {{32 * (base + animation), 0}, {32, 24}});
                enemySprite.setPosition(enemy.position);
                window.draw(enemySprite);
            }

            if (bonusActive)
            {
                sf::Sprite sprite(assets.bonus);
                sprite.setPosition({bonusX, 50.f});
                window.draw(sprite);
            }

            sf::Sprite playerSprite(assets.player);
            playerSprite.setPosition({playerX, 490.f});
            window.draw(playerSprite);

            if (playerBullet)
            {
                sf::RectangleShape bullet({4.f, 12.f});
                bullet.setOrigin({2.f, 6.f});
                bullet.setPosition(*playerBullet);
                bullet.setFillColor(sf::Color::White);
                window.draw(bullet);
            }

            sf::CircleShape bullet(3.f);
            bullet.setOrigin({3.f, 3.f});
            bullet.setFillColor(sf::Color(0xFF, 0x6B, 0x6B));
            for (const auto& enemyBullet : enemyBullets)
            {
                if (!enemyBullet.alive)
                    continue;
                bullet.setPosition(enemyBullet.position);
                window.draw(bullet);
            }

            text("SCORE " + std::to_string(score), 16, {16.f, 12.f});
            text("WAVE " + std::to_string(wave), 16, {432.f, 12.f});
            text("LIVES " + std::to_string(lives), 16, {800.f, 12.f});
        }

        window.display();
    }

private:
    void startGame()
    {
        score = 0;
        wave = 1;
        lives = 3;
        playerX = 460.f;
        freezeTime = 0.f;
        bonusTimer = 0.0;
        nextBonusFromLeft = true;
        shotPending = false;

        shootSound.stop();
        hitSound.stop();
        loseSound.stop();
        bonusSound.stop();

        startWave();
        screen = Screen::Play;
        assets.music.stop();
        assets.music.play();
    }

    void startWave()
    {
        enemies.clear();
        walls.clear();
        enemyBullets.clear();
        playerBullet.reset();
        bonusActive = false;
        direction = 1.f;
        fireTimer = 0.0;
        animationTime = 0.0;

        const float offset = 16.f * static_cast<float>(std::min(wave - 1, 4));
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 8; ++column)
                enemies.push_back({
                    {296.f + 48.f * column, 80.f + 40.f * row + offset},
                    row, column, true});

        for (float center : {192.f, 384.f, 576.f, 768.f})
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 6; ++column)
                    walls.push_back({
                        {center - 24.f + column * 8.f, 400.f + row * 8.f},
                        true});
    }

    void finishGame()
    {
        screen = Screen::Result;
        playerBullet.reset();
        enemyBullets.clear();
        assets.music.stop();
    }

    void moveFormation(float dt)
    {
        int count = 0;
        float left = Infinity;
        float right = -Infinity;
        for (const auto& enemy : enemies)
        {
            if (!enemy.alive)
                continue;
            ++count;
            left = std::min(left, enemy.position.x);
            right = std::max(right, enemy.position.x + 32.f);
        }

        if (count == 0)
            return;

        const float speed = 40.f + 4.f * (32 - count);
        float remaining = dt;

        while (remaining > 0.f)
        {
            const float distance = direction > 0.f ? 944.f - right :
                                                             left - 16.f;
            const float timeToEdge = std::max(0.f, distance) / speed;
            const bool touchesEdge = timeToEdge <= remaining;
            const float elapsed = touchesEdge ? timeToEdge : remaining;
            const float dx = direction * speed * elapsed;

            for (auto& enemy : enemies)
                if (enemy.alive)
                    enemy.position.x += dx;

            left += dx;
            right += dx;
            remaining = std::max(0.f, remaining - elapsed);

            if (!touchesEdge)
                break;

            for (auto& enemy : enemies)
                if (enemy.alive)
                    enemy.position.y += 16.f;
            direction = -direction;
        }
    }

    void fireEnemyBullet()
    {
        if (enemyBullets.size() >= 3)
            return;

        std::array<const Enemy*, 8> bottom{};
        for (const auto& enemy : enemies)
            if (enemy.alive &&
                (!bottom[enemy.column] ||
                 enemy.position.y > bottom[enemy.column]->position.y))
                bottom[enemy.column] = &enemy;

        const Enemy* shooter = nullptr;
        float closest = Infinity;
        for (const Enemy* enemy : bottom)
        {
            if (!enemy)
                continue;

            const float distance =
                std::abs(enemy->position.x + 16.f - (playerX + 20.f));
            if (distance < closest)
            {
                closest = distance;
                shooter = enemy;
            }
        }

        if (!shooter)
            return;

        const sf::Vector2f start =
            shooter->position + sf::Vector2f{16.f, 24.f};
        const sf::Vector2f difference =
            sf::Vector2f{playerX + 20.f, 502.f} - start;
        const float length = std::sqrt(
            difference.x * difference.x + difference.y * difference.y);

        const sf::Vector2f velocity = length > 0.f ?
            difference * (240.f / length) : sf::Vector2f{0.f, 240.f};
        enemyBullets.push_back({start, velocity, true});
    }

    void updatePlayerBullet(float dt)
    {
        if (!playerBullet)
            return;

        const sf::Vector2f delta{0.f, -600.f * dt};
        float earliest = Infinity;
        enum class Target { None, Wall, Enemy, Bonus };
        Target target = Target::None;
        std::size_t index = 0;

        const auto consider = [&](float time, Target type, std::size_t i)
        {
            if (time < earliest)
            {
                earliest = time;
                target = type;
                index = i;
            }
        };

        for (std::size_t i = 0; i < walls.size(); ++i)
            if (walls[i].alive)
                consider(segmentRect(*playerBullet, delta,
                                     expanded(walls[i].bounds(), {2.f, 6.f})),
                         Target::Wall, i);

        for (std::size_t i = 0; i < enemies.size(); ++i)
            if (enemies[i].alive)
                consider(segmentRect(*playerBullet, delta,
                                     expanded(enemies[i].bounds(), {2.f, 6.f})),
                         Target::Enemy, i);

        if (bonusActive)
            consider(segmentRect(*playerBullet, delta,
                                 expanded({{bonusX, 50.f}, {48.f, 20.f}},
                                          {2.f, 6.f})),
                     Target::Bonus, 0);

        if (target == Target::None)
        {
            *playerBullet += delta;
            if (playerBullet->y + 6.f < 0.f)
                playerBullet.reset();
            return;
        }

        playerBullet.reset();
        if (target == Target::Wall)
            walls[index].alive = false;
        else if (target == Target::Enemy)
        {
            Enemy& enemy = enemies[index];
            enemy.alive = false;
            score += enemy.row == 0 ? 30 : enemy.row == 1 ? 20 : 10;
            hitSound.play();
        }
        else
        {
            bonusActive = false;
            score += 50 * std::uniform_int_distribution<int>(1, 3)(random);
            bonusSound.play();
        }
    }

    void updateEnemyBullets(float dt)
    {
        for (auto& bullet : enemyBullets)
        {
            const sf::Vector2f delta = bullet.velocity * dt;
            float earliest = Infinity;
            std::optional<std::size_t> wallIndex;

            for (std::size_t i = 0; i < walls.size(); ++i)
            {
                if (!walls[i].alive)
                    continue;
                const float time =
                    sweptCircle(bullet.position, delta, walls[i].bounds(), 3.f);
                if (time < earliest)
                {
                    earliest = time;
                    wallIndex = i;
                }
            }

            const float playerTime = sweptCircle(
                bullet.position, delta, {{playerX, 490.f}, {40.f, 24.f}}, 3.f);

            if (playerTime < earliest)
            {
                --lives;
                loseSound.play();
                playerBullet.reset();
                enemyBullets.clear();

                if (lives == 0)
                    finishGame();
                else
                    freezeTime = 1.f;
                return;
            }

            if (wallIndex)
            {
                walls[*wallIndex].alive = false;
                bullet.alive = false;
            }
            else
            {
                bullet.position += delta;
                if (bullet.position.x + 3.f < 0.f ||
                    bullet.position.x - 3.f > 960.f ||
                    bullet.position.y + 3.f < 0.f ||
                    bullet.position.y - 3.f > 540.f)
                    bullet.alive = false;
            }
        }

        std::erase_if(enemyBullets, [](const EnemyBullet& bullet) {
            return !bullet.alive;
        });
    }

    void text(const std::string& string, unsigned size, sf::Vector2f position)
    {
        sf::Text label(assets.font, string, size);
        label.setFillColor(sf::Color::White);
        label.setPosition(position);
        window.draw(label);
    }

    void centered(const std::string& string, unsigned size, float y)
    {
        sf::Text label(assets.font, string, size);
        label.setFillColor(sf::Color::White);
        const auto bounds = label.getLocalBounds();
        label.setPosition({
            480.f - bounds.position.x - bounds.size.x / 2.f,
            y - bounds.position.y});
        window.draw(label);
    }

    sf::RenderWindow& window;
    Assets& assets;
    sf::Sound shootSound, hitSound, loseSound, bonusSound;
    std::mt19937 random;

    Screen screen = Screen::Title;
    std::vector<Enemy> enemies;
    std::vector<WallPiece> walls;
    std::vector<EnemyBullet> enemyBullets;
    std::optional<sf::Vector2f> playerBullet;

    int score = 0;
    int wave = 1;
    int lives = 3;
    float playerX = 460.f;
    float direction = 1.f;
    float freezeTime = 0.f;
    double titleTime = 0.0;
    double fireTimer = 0.0;
    double animationTime = 0.0;
    double bonusTimer = 0.0;

    bool bonusActive = false;
    bool nextBonusFromLeft = true;
    float bonusX = -48.f;
    float bonusDirection = 1.f;

    bool leftHeld = false;
    bool rightHeld = false;
    bool aHeld = false;
    bool dHeld = false;
    bool leftTap = false;
    bool rightTap = false;
    bool shotPending = false;
    bool enterPending = false;
    bool restartPending = false;
};
}

int main()
{
    try
    {
        sf::RenderWindow window(
            sf::VideoMode({960u, 540u}),
            "STAR FORMATION",
            sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Assets assets;
        if (!assets.load())
            return 1;

        Game game(window, assets);
        sf::Clock clock;
        double accumulator = 0.0;

        while (window.isOpen())
        {
            accumulator += clock.restart().asSeconds();

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

        assets.music.stop();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
