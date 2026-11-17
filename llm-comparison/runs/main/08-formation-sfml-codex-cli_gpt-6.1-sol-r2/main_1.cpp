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

struct Assets
{
    sf::Font font;
    sf::Texture enemies, player, bonus;
    sf::SoundBuffer shoot, hit, lose, bonusSound;
    sf::Music music;

    bool load()
    {
        const auto read = [](const char* path, auto&& loader)
        {
            try
            {
                if (loader())
                    return true;
            }
            catch (const std::exception&)
            {
            }
            std::cerr << path << '\n';
            return false;
        };

        return read("assets/fonts/game.ttf",
                    [&] { return font.openFromFile("assets/fonts/game.ttf"); }) &&
               read("assets/textures/fm_enemies.png",
                    [&] { return enemies.loadFromFile("assets/textures/fm_enemies.png"); }) &&
               read("assets/textures/fm_player.png",
                    [&] { return player.loadFromFile("assets/textures/fm_player.png"); }) &&
               read("assets/textures/fm_bonus.png",
                    [&] { return bonus.loadFromFile("assets/textures/fm_bonus.png"); }) &&
               read("assets/sfx/shoot.wav",
                    [&] { return shoot.loadFromFile("assets/sfx/shoot.wav"); }) &&
               read("assets/sfx/hit.wav",
                    [&] { return hit.loadFromFile("assets/sfx/hit.wav"); }) &&
               read("assets/sfx/lose.wav",
                    [&] { return lose.loadFromFile("assets/sfx/lose.wav"); }) &&
               read("assets/sfx/bonus.wav",
                    [&] { return bonusSound.loadFromFile("assets/sfx/bonus.wav"); }) &&
               read("assets/music/bgm_formation.ogg",
                    [&] { return music.openFromFile("assets/music/bgm_formation.ogg"); });
    }
};

struct Enemy
{
    int column;
    int row;
};

struct EnemyBullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
};

struct Bonus
{
    float x;
    float velocity;
};

bool overlaps(const sf::FloatRect& a, const sf::FloatRect& b)
{
    return a.findIntersection(b).has_value();
}

bool circleHits(sf::Vector2f center, const sf::FloatRect& box)
{
    const float x = std::clamp(center.x, box.position.x,
                               box.position.x + box.size.x);
    const float y = std::clamp(center.y, box.position.y,
                               box.position.y + box.size.y);
    const float dx = center.x - x;
    const float dy = center.y - y;
    return dx * dx + dy * dy <= 9.f;
}

class Game
{
public:
    explicit Game(Assets& assets)
        : assets(assets),
          shootSound(assets.shoot),
          hitSound(assets.hit),
          loseSound(assets.lose),
          bonusSound(assets.bonusSound),
          random(std::random_device{}())
    {
        assets.music.setVolume(40.f);
        assets.music.setLooping(true);
    }

    void keyPressed(sf::Keyboard::Key key)
    {
        using Key = sf::Keyboard::Key;
        if (key == Key::Left || key == Key::A)
        {
            heldLeft[key == Key::A ? 1 : 0] = true;
            tapLeft = true;
        }
        if (key == Key::Right || key == Key::D)
        {
            heldRight[key == Key::D ? 1 : 0] = true;
            tapRight = true;
        }

        if (state == State::Title && key == Key::Enter)
            restart();
        else if (state == State::Result && key == Key::R)
            restart();
        else if (state == State::Playing && key == Key::Space)
            pendingShot = true;
    }

    void keyReleased(sf::Keyboard::Key key)
    {
        using Key = sf::Keyboard::Key;
        if (key == Key::Left || key == Key::A)
            heldLeft[key == Key::A ? 1 : 0] = false;
        if (key == Key::Right || key == Key::D)
            heldRight[key == Key::D ? 1 : 0] = false;
    }

    void clearInput()
    {
        heldLeft.fill(false);
        heldRight.fill(false);
        tapLeft = tapRight = pendingShot = false;
    }

    void update(float dt)
    {
        if (state == State::Title)
        {
            titleTime += dt;
            if (titleTime >= 2.f)
                restart();
            return;
        }
        if (state != State::Playing)
            return;

        if (freezeTime > 0.f)
        {
            freezeTime = std::max(0.f, freezeTime - dt);
            return;
        }

        const bool left = heldLeft[0] || heldLeft[1] || tapLeft;
        const bool right = heldRight[0] || heldRight[1] || tapRight;
        playerX = std::clamp(playerX + (static_cast<int>(right) -
                                      static_cast<int>(left)) * 300.f * dt,
                             16.f, 904.f);
        tapLeft = tapRight = false;

        if (pendingShot && !playerBullet)
        {
            playerBullet = sf::Vector2f{playerX + 18.f, 478.f};
            shootSound.play();
        }
        pendingShot = false;

        animationTime += dt;
        playTime += dt;
        moveFormation(dt);

        for (const auto& enemy : enemies)
        {
            if (enemyBox(enemy).position.y + 24.f >= 490.f)
            {
                finish();
                return;
            }
        }

        std::erase_if(walls, [&](const sf::FloatRect& wall)
        {
            return std::any_of(enemies.begin(), enemies.end(),
                               [&](const Enemy& e)
                               {
                                   return overlaps(wall, enemyBox(e));
                               });
        });

        shotTime += dt;
        while (shotTime >= 1.f)
        {
            shotTime -= 1.f;
            if (enemyBullets.size() < 3)
                fireEnemy();
        }

        while (playTime >= nextBonusTime)
        {
            nextBonusTime += 20.f;
            bonus = Bonus{bonusFromLeft ? -48.f : 960.f,
                          bonusFromLeft ? 120.f : -120.f};
            bonusFromLeft = !bonusFromLeft;
        }

        if (bonus)
        {
            bonus->x += bonus->velocity * dt;
            if ((bonus->velocity > 0.f && bonus->x >= 960.f) ||
                (bonus->velocity < 0.f && bonus->x + 48.f <= 0.f))
                bonus.reset();
        }

        if (playerBullet)
        {
            playerBullet->y -= 600.f * dt;
            const sf::FloatRect bulletBox(*playerBullet, {4.f, 12.f});

            // Small fixed steps ensure that a bullet cannot skip an 8px tile.
            for (auto it = walls.begin(); it != walls.end(); ++it)
            {
                if (overlaps(bulletBox, *it))
                {
                    walls.erase(it);
                    playerBullet.reset();
                    break;
                }
            }

            if (playerBullet)
            {
                for (auto it = enemies.begin(); it != enemies.end(); ++it)
                {
                    if (overlaps(bulletBox, enemyBox(*it)))
                    {
                        score += it->row == 0 ? 30 : (it->row == 1 ? 20 : 10);
                        enemies.erase(it);
                        playerBullet.reset();
                        hitSound.play();
                        break;
                    }
                }
            }

            if (playerBullet && bonus &&
                overlaps(bulletBox, sf::FloatRect({bonus->x, 50.f}, {48.f, 20.f})))
            {
                score += 50 * std::uniform_int_distribution<int>(1, 3)(random);
                bonus.reset();
                playerBullet.reset();
                bonusSound.play();
            }

            if (playerBullet && playerBullet->y + 12.f <= 0.f)
                playerBullet.reset();
        }

        if (enemies.empty())
        {
            ++wave;
            startWave();
            return;
        }

        const sf::FloatRect playerBox({playerX, 490.f}, {40.f, 24.f});
        for (std::size_t i = 0; i < enemyBullets.size();)
        {
            auto& bullet = enemyBullets[i];
            bullet.position += bullet.velocity * dt;
            bool removed = false;

            for (auto it = walls.begin(); it != walls.end(); ++it)
            {
                if (circleHits(bullet.position, *it))
                {
                    walls.erase(it);
                    removed = true;
                    break;
                }
            }

            if (!removed && circleHits(bullet.position, playerBox))
            {
                --lives;
                loseSound.play();
                enemyBullets.clear();
                playerBullet.reset();
                if (lives == 0)
                    finish();
                else
                    freezeTime = 1.f;
                return;
            }

            const auto p = bullet.position;
            if (removed || p.x < -3.f || p.x > 963.f ||
                p.y < -3.f || p.y > 543.f)
                enemyBullets.erase(enemyBullets.begin() +
                                   static_cast<std::ptrdiff_t>(i));
            else
                ++i;
        }
    }

    void draw(sf::RenderWindow& window)
    {
        if (state == State::Title)
        {
            centered(window, "STAR FORMATION", 48, 174.f);
            centered(window, "PRESS ENTER", 32, 278.f);
            return;
        }

        if (state == State::Result)
        {
            centered(window, "GAME OVER", 48, 130.f);
            centered(window, "SCORE " + std::to_string(score), 32, 218.f);
            centered(window, "WAVE " + std::to_string(wave), 32, 270.f);
            centered(window, "R: RESTART  ESC: QUIT", 16, 364.f);
            return;
        }

        sf::RectangleShape tile({8.f, 8.f});
        tile.setFillColor(sf::Color(0x4C, 0xC2, 0x6B));
        for (const auto& wall : walls)
        {
            tile.setPosition(wall.position);
            window.draw(tile);
        }

        sf::Sprite enemySprite(assets.enemies);
        const int frame = static_cast<int>(std::fmod(animationTime, 1.f) / 0.5f);
        for (const auto& enemy : enemies)
        {
            const int base = enemy.row == 0 ? 0 : (enemy.row == 1 ? 2 : 4);
            enemySprite.setTextureRect(sf::IntRect({(base + frame) * 32, 0},
                                                   {32, 24}));
            enemySprite.setPosition(enemyBox(enemy).position);
            window.draw(enemySprite);
        }

        if (bonus)
        {
            sf::Sprite sprite(assets.bonus);
            sprite.setPosition({bonus->x, 50.f});
            window.draw(sprite);
        }

        sf::Sprite playerSprite(assets.player);
        playerSprite.setPosition({playerX, 490.f});
        window.draw(playerSprite);

        if (playerBullet)
        {
            sf::RectangleShape bullet({4.f, 12.f});
            bullet.setFillColor(sf::Color::White);
            bullet.setPosition(*playerBullet);
            window.draw(bullet);
        }

        sf::CircleShape bullet(3.f);
        bullet.setOrigin({3.f, 3.f});
        bullet.setFillColor(sf::Color(0xFF, 0x6B, 0x6B));
        for (const auto& b : enemyBullets)
        {
            bullet.setPosition(b.position);
            window.draw(bullet);
        }

        text(window, "SCORE " + std::to_string(score), 16, {16.f, 12.f});
        centered(window, "WAVE " + std::to_string(wave), 16, 12.f);

        sf::Text livesText(assets.font, "LIVES " + std::to_string(lives), 16);
        const auto bounds = livesText.getLocalBounds();
        livesText.setPosition({944.f - bounds.position.x - bounds.size.x, 12.f});
        window.draw(livesText);
    }

private:
    enum class State { Title, Playing, Result };

    void restart()
    {
        assets.music.stop();
        shootSound.stop();
        hitSound.stop();
        loseSound.stop();
        bonusSound.stop();

        state = State::Playing;
        score = 0;
        wave = 1;
        lives = 3;
        playerX = 460.f;
        freezeTime = 0.f;
        playTime = 0.0;
        nextBonusTime = 20.0;
        bonusFromLeft = true;
        clearInput();
        startWave();
        assets.music.play();
    }

    void startWave()
    {
        enemies.clear();
        walls.clear();
        enemyBullets.clear();
        playerBullet.reset();
        bonus.reset();
        formationX = 296.f;
        formationY = 80.f + 16.f * static_cast<float>(std::min(wave - 1, 4));
        direction = 1.f;
        shotTime = 0.f;
        animationTime = 0.f;

        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 8; ++c)
                enemies.push_back({c, r});

        for (float center : {192.f, 384.f, 576.f, 768.f})
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 6; ++c)
                    walls.emplace_back(
                        sf::Vector2f{center - 24.f + 8.f * c, 400.f + 8.f * r},
                        sf::Vector2f{8.f, 8.f});
    }

    sf::FloatRect enemyBox(const Enemy& enemy) const
    {
        return {{formationX + 48.f * enemy.column,
                 formationY + 40.f * enemy.row}, {32.f, 24.f}};
    }

    void moveFormation(float dt)
    {
        if (enemies.empty())
            return;

        int leftColumn = 7;
        int rightColumn = 0;
        for (const auto& e : enemies)
        {
            leftColumn = std::min(leftColumn, e.column);
            rightColumn = std::max(rightColumn, e.column);
        }

        const float lower = 16.f - 48.f * leftColumn;
        const float upper = 944.f - 32.f - 48.f * rightColumn;
        float travel = (40.f + 4.f * (32.f - static_cast<float>(enemies.size()))) * dt;

        while (travel > 0.f)
        {
            const float distance = std::max(
                0.f, direction > 0.f ? upper - formationX : formationX - lower);
            if (travel < distance)
            {
                formationX += direction * travel;
                break;
            }

            formationX = direction > 0.f ? upper : lower;
            travel -= distance;
            formationY += 16.f;
            direction = -direction;
        }
    }

    void fireEnemy()
    {
        const Enemy* chosen = nullptr;
        float bestDistance = std::numeric_limits<float>::max();

        for (int c = 0; c < 8; ++c)
        {
            const Enemy* bottom = nullptr;
            for (const auto& e : enemies)
                if (e.column == c && (!bottom || e.row > bottom->row))
                    bottom = &e;

            if (!bottom)
                continue;

            const float distance =
                std::abs(formationX + 48.f * c + 16.f - (playerX + 20.f));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                chosen = bottom;
            }
        }

        if (!chosen)
            return;

        const auto box = enemyBox(*chosen);
        const sf::Vector2f origin = box.position + sf::Vector2f{16.f, 24.f};
        const sf::Vector2f delta = sf::Vector2f{playerX + 20.f, 502.f} - origin;
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        const sf::Vector2f velocity =
            length > 0.f ? delta * (240.f / length) : sf::Vector2f{0.f, 240.f};
        enemyBullets.push_back({origin, velocity});
    }

    void finish()
    {
        state = State::Result;
        assets.music.stop();
        enemyBullets.clear();
        playerBullet.reset();
        pendingShot = false;
    }

    void text(sf::RenderWindow& window, const std::string& string,
              unsigned int size, sf::Vector2f position)
    {
        sf::Text label(assets.font, string, size);
        label.setFillColor(sf::Color::White);
        label.setPosition(position);
        window.draw(label);
    }

    void centered(sf::RenderWindow& window, const std::string& string,
                  unsigned int size, float y)
    {
        sf::Text label(assets.font, string, size);
        label.setFillColor(sf::Color::White);
        const auto bounds = label.getLocalBounds();
        label.setPosition({480.f - bounds.position.x - bounds.size.x / 2.f, y});
        window.draw(label);
    }

    Assets& assets;
    sf::Sound shootSound, hitSound, loseSound, bonusSound;
    std::mt19937 random;
    State state = State::Title;
    std::vector<Enemy> enemies;
    std::vector<sf::FloatRect> walls;
    std::vector<EnemyBullet> enemyBullets;
    std::optional<sf::Vector2f> playerBullet;
    std::optional<Bonus> bonus;
    std::array<bool, 2> heldLeft{}, heldRight{};
    bool tapLeft = false;
    bool tapRight = false;
    bool pendingShot = false;
    bool bonusFromLeft = true;
    int score = 0;
    int wave = 1;
    int lives = 3;
    float playerX = 460.f;
    float formationX = 296.f;
    float formationY = 80.f;
    float direction = 1.f;
    float titleTime = 0.f;
    float freezeTime = 0.f;
    float shotTime = 0.f;
    float animationTime = 0.f;
    double playTime = 0.0;
    double nextBonusTime = 20.0;
};
}

int main()
{
    try
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "STAR FORMATION",
                                sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Assets assets;
        if (!assets.load())
            return 1;

        Game game(assets);
        sf::Clock clock;
        double accumulator = 0.0;

        while (window.isOpen())
        {
            accumulator += clock.restart().asSeconds();

            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                    window.close();
                else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                        window.close();
                    else
                        game.keyPressed(key->code);
                }
                else if (const auto* key = event->getIf<sf::Event::KeyReleased>())
                    game.keyReleased(key->code);
                else if (event->is<sf::Event::FocusLost>())
                    game.clearInput();
            }

            if (!window.isOpen())
                break;

            while (accumulator >= static_cast<double>(Tick))
            {
                game.update(Tick);
                accumulator -= static_cast<double>(Tick);
            }

            window.clear(sf::Color::Black);
            game.draw(window);
            window.display();
        }

        assets.music.stop();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
