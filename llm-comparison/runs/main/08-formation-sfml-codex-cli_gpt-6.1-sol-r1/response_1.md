```cpp
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float Width = 960.f;
constexpr float Height = 540.f;
constexpr float Tick = 1.f / 60.f;

const sf::Color WallColor{0x4C, 0xC2, 0x6B};
const sf::Color EnemyBulletColor{0xFF, 0x6B, 0x6B};

struct Assets
{
    sf::Font font;
    sf::Texture enemies, player, bonus;
    sf::SoundBuffer shoot, hit, lose, bonusSound;
    sf::Music music;

    bool load()
    {
        auto check = [](bool success, const char* path)
        {
            if (!success)
                std::cerr << "FAILED TO LOAD: " << path << '\n';
            return success;
        };

        if (!check(font.openFromFile("assets/fonts/game.ttf"),
                   "assets/fonts/game.ttf"))
            return false;
        if (!check(enemies.loadFromFile("assets/textures/fm_enemies.png"),
                   "assets/textures/fm_enemies.png"))
            return false;
        if (!check(player.loadFromFile("assets/textures/fm_player.png"),
                   "assets/textures/fm_player.png"))
            return false;
        if (!check(bonus.loadFromFile("assets/textures/fm_bonus.png"),
                   "assets/textures/fm_bonus.png"))
            return false;
        if (!check(shoot.loadFromFile("assets/sfx/shoot.wav"),
                   "assets/sfx/shoot.wav"))
            return false;
        if (!check(hit.loadFromFile("assets/sfx/hit.wav"),
                   "assets/sfx/hit.wav"))
            return false;
        if (!check(lose.loadFromFile("assets/sfx/lose.wav"),
                   "assets/sfx/lose.wav"))
            return false;
        if (!check(bonusSound.loadFromFile("assets/sfx/bonus.wav"),
                   "assets/sfx/bonus.wav"))
            return false;
        if (!check(music.openFromFile("assets/music/bgm_formation.ogg"),
                   "assets/music/bgm_formation.ogg"))
            return false;

        music.setVolume(40.f);
        music.setLooping(true);
        return true;
    }
};

struct Enemy
{
    sf::Vector2f position;
    int row;
    int column;

    sf::FloatRect bounds() const
    {
        return {position, {32.f, 24.f}};
    }
};

struct Bullet
{
    sf::Vector2f position;
    sf::Vector2f velocity;
};

// Earliest intersection along a segment, including an initially overlapping point.
std::optional<float> segmentBox(sf::Vector2f p, sf::Vector2f delta,
                                const sf::FloatRect& box)
{
    float first = 0.f;
    float last = 1.f;

    auto axis = [&](float origin, float movement, float low, float high)
    {
        if (std::abs(movement) < 0.000001f)
            return origin >= low && origin <= high;

        float a = (low - origin) / movement;
        float b = (high - origin) / movement;
        if (a > b)
            std::swap(a, b);
        first = std::max(first, a);
        last = std::min(last, b);
        return first <= last;
    };

    if (!axis(p.x, delta.x, box.position.x, box.position.x + box.size.x) ||
        !axis(p.y, delta.y, box.position.y, box.position.y + box.size.y))
        return std::nullopt;

    return first;
}

std::optional<float> sweptRectangle(sf::Vector2f p, sf::Vector2f delta,
                                    sf::Vector2f halfSize,
                                    const sf::FloatRect& target)
{
    return segmentBox(p, delta,
                      {target.position - halfSize,
                       target.size + halfSize * 2.f});
}

// Exact swept circle against an axis-aligned rectangle: two strips and four corners.
std::optional<float> sweptCircle(sf::Vector2f p, sf::Vector2f delta,
                                 float radius, const sf::FloatRect& target)
{
    std::optional<float> best;
    auto consider = [&](std::optional<float> t)
    {
        if (t && (!best || *t < *best))
            best = t;
    };

    consider(segmentBox(
        p, delta,
        {{target.position.x - radius, target.position.y},
         {target.size.x + 2.f * radius, target.size.y}}));
    consider(segmentBox(
        p, delta,
        {{target.position.x, target.position.y - radius},
         {target.size.x, target.size.y + 2.f * radius}}));

    const float a = delta.x * delta.x + delta.y * delta.y;
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            const sf::Vector2f corner{
                target.position.x + static_cast<float>(x) * target.size.x,
                target.position.y + static_cast<float>(y) * target.size.y};
            const sf::Vector2f offset = p - corner;
            const float c = offset.x * offset.x + offset.y * offset.y -
                            radius * radius;
            if (c <= 0.f)
            {
                consider(0.f);
                continue;
            }

            if (a <= 0.f)
                continue;
            const float b = 2.f * (offset.x * delta.x + offset.y * delta.y);
            const float discriminant = b * b - 4.f * a * c;
            if (discriminant < 0.f)
                continue;
            const float t = (-b - std::sqrt(discriminant)) / (2.f * a);
            if (t >= 0.f && t <= 1.f)
                consider(t);
        }
    }
    return best;
}

struct Input
{
    bool left = false;
    bool a = false;
    bool right = false;
    bool d = false;
    bool leftTap = false;
    bool rightTap = false;
    bool fire = false;
    bool enter = false;
    bool restart = false;

    void clearActions()
    {
        leftTap = rightTap = fire = enter = restart = false;
    }
};

enum class Screen { Title, Playing, Result };

class Game
{
public:
    explicit Game(Assets& assets)
        : assets_(assets),
          shootSound_(assets.shoot),
          hitSound_(assets.hit),
          loseSound_(assets.lose),
          bonusSound_(assets.bonusSound),
          random_(static_cast<std::mt19937::result_type>(
              std::chrono::steady_clock::now().time_since_epoch().count()))
    {
    }

    void update(float dt, Input& input)
    {
        if (screen_ == Screen::Title)
        {
            titleTime_ += dt;
            if (input.enter || titleTime_ >= 2.f)
                restart();
            input.clearActions();
            return;
        }

        if (screen_ == Screen::Result)
        {
            if (input.restart)
                restart();
            input.clearActions();
            return;
        }

        const bool fire = input.fire;
        const bool left = input.left || input.a || input.leftTap;
        const bool right = input.right || input.d || input.rightTap;
        input.clearActions();

        if (freeze_ > 0.f)
        {
            freeze_ = std::max(0.f, freeze_ - dt);
            return;
        }

        playerX_ = std::clamp(
            playerX_ + (static_cast<float>(right) - static_cast<float>(left)) *
                           300.f * dt,
            16.f, 904.f);

        if (fire && !playerBullet_)
        {
            playerBullet_ = Bullet{{playerX_ + 20.f, 484.f}, {0.f, -600.f}};
            shootSound_.play();
        }

        animationTime_ += dt;
        moveFormation(dt);

        for (const Enemy& enemy : enemies_)
        {
            if (enemy.position.y + 24.f >= 490.f)
            {
                finish();
                return;
            }
        }

        std::erase_if(walls_, [&](const sf::FloatRect& wall)
        {
            return std::any_of(enemies_.begin(), enemies_.end(),
                               [&](const Enemy& enemy)
            {
                return wall.findIntersection(enemy.bounds()).has_value();
            });
        });

        bonusTimer_ += dt;
        while (bonusTimer_ >= 20.f)
        {
            bonusTimer_ -= 20.f;
            bonusActive_ = true;
            bonusX_ = bonusFromLeft_ ? -48.f : 960.f;
            bonusDirection_ = bonusFromLeft_ ? 1.f : -1.f;
            bonusFromLeft_ = !bonusFromLeft_;
        }
        if (bonusActive_)
        {
            bonusX_ += bonusDirection_ * 120.f * dt;
            if ((bonusDirection_ > 0.f && bonusX_ >= Width) ||
                (bonusDirection_ < 0.f && bonusX_ + 48.f <= 0.f))
                bonusActive_ = false;
        }

        shotTimer_ += dt;
        while (shotTimer_ >= 1.f)
        {
            shotTimer_ -= 1.f;
            if (enemyBullets_.size() < 3)
                enemyFire();
        }

        updatePlayerBullet(dt);

        if (enemies_.empty())
        {
            ++wave_;
            startWave();
            return;
        }

        updateEnemyBullets(dt);
    }

    void draw(sf::RenderWindow& window)
    {
        if (screen_ == Screen::Title)
        {
            centered(window, "STAR FORMATION", 48, 180.f);
            centered(window, "PRESS ENTER", 32, 290.f);
            return;
        }

        if (screen_ == Screen::Result)
        {
            centered(window, "GAME OVER", 48, 130.f);
            centered(window, "SCORE " + std::to_string(score_), 32, 225.f);
            centered(window, "WAVE " + std::to_string(wave_), 32, 280.f);
            centered(window, "R: RESTART  ESC: QUIT", 32, 380.f);
            return;
        }

        sf::RectangleShape piece({8.f, 8.f});
        piece.setFillColor(WallColor);
        for (const auto& wall : walls_)
        {
            piece.setPosition(wall.position);
            window.draw(piece);
        }

        sf::Sprite enemySprite(assets_.enemies);
        const int frame = static_cast<int>(animationTime_ / 0.5f) % 2;
        for (const Enemy& enemy : enemies_)
        {
            const int base = enemy.row == 0 ? 0 : (enemy.row == 1 ? 2 : 4);
            enemySprite.setTextureRect({{(base + frame) * 32, 0}, {32, 24}});
            enemySprite.setPosition(enemy.position);
            window.draw(enemySprite);
        }

        if (bonusActive_)
        {
            sf::Sprite bonusSprite(assets_.bonus);
            bonusSprite.setPosition({bonusX_, 50.f});
            window.draw(bonusSprite);
        }

        sf::Sprite playerSprite(assets_.player);
        playerSprite.setPosition({playerX_, 490.f});
        window.draw(playerSprite);

        if (playerBullet_)
        {
            sf::RectangleShape bullet({4.f, 12.f});
            bullet.setOrigin({2.f, 6.f});
            bullet.setPosition(playerBullet_->position);
            bullet.setFillColor(sf::Color::White);
            window.draw(bullet);
        }

        sf::CircleShape bullet(3.f);
        bullet.setOrigin({3.f, 3.f});
        bullet.setFillColor(EnemyBulletColor);
        for (const Bullet& shot : enemyBullets_)
        {
            bullet.setPosition(shot.position);
            window.draw(bullet);
        }

        text(window, "SCORE " + std::to_string(score_), 16, {16.f, 12.f});
        text(window, "WAVE " + std::to_string(wave_), 16, {420.f, 12.f});
        text(window, "LIVES " + std::to_string(lives_), 16, {800.f, 12.f});
    }

private:
    void restart()
    {
        score_ = 0;
        wave_ = 1;
        lives_ = 3;
        playerX_ = 460.f;
        freeze_ = 0.f;
        bonusTimer_ = 0.f;
        bonusFromLeft_ = true;
        shootSound_.stop();
        hitSound_.stop();
        loseSound_.stop();
        bonusSound_.stop();
        startWave();
        screen_ = Screen::Playing;
        assets_.music.stop();
        assets_.music.play();
    }

    void startWave()
    {
        enemies_.clear();
        walls_.clear();
        playerBullet_.reset();
        enemyBullets_.clear();
        bonusActive_ = false;
        direction_ = 1.f;
        shotTimer_ = 0.f;
        animationTime_ = 0.f;

        const float offset = 16.f * static_cast<float>(std::min(wave_ - 1, 4));
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 8; ++c)
                enemies_.push_back(
                    {{296.f + 48.f * c, 80.f + 40.f * r + offset}, r, c});

        for (float center : {192.f, 384.f, 576.f, 768.f})
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 6; ++c)
                    walls_.push_back(
                        {{center - 24.f + 8.f * c, 400.f + 8.f * r},
                         {8.f, 8.f}});
    }

    void finish()
    {
        screen_ = Screen::Result;
        playerBullet_.reset();
        enemyBullets_.clear();
        assets_.music.stop();
    }

    void moveFormation(float dt)
    {
        if (enemies_.empty())
            return;

        float left = Width;
        float right = 0.f;
        for (const Enemy& enemy : enemies_)
        {
            left = std::min(left, enemy.position.x);
            right = std::max(right, enemy.position.x + 32.f);
        }

        float remaining =
            (40.f + 4.f * static_cast<float>(32 - enemies_.size())) * dt;

        while (remaining > 0.f)
        {
            const float distance =
                std::max(0.f, direction_ > 0.f ? 944.f - right : left - 16.f);
            const float movement = std::min(remaining, distance);
            const float dx = direction_ * movement;
            for (Enemy& enemy : enemies_)
                enemy.position.x += dx;
            left += dx;
            right += dx;
            remaining -= movement;

            if (movement >= distance)
            {
                for (Enemy& enemy : enemies_)
                    enemy.position.y += 16.f;
                direction_ = -direction_;
            }
            else
                break;
        }
    }

    void enemyFire()
    {
        const Enemy* selected = nullptr;
        float bestDistance = std::numeric_limits<float>::infinity();

        for (int column = 0; column < 8; ++column)
        {
            const Enemy* bottom = nullptr;
            for (const Enemy& enemy : enemies_)
            {
                if (enemy.column == column &&
                    (!bottom || enemy.row > bottom->row))
                    bottom = &enemy;
            }
            if (!bottom)
                continue;

            const float distance =
                std::abs(bottom->position.x + 16.f - (playerX_ + 20.f));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                selected = bottom;
            }
        }

        if (!selected)
            return;

        const sf::Vector2f start = selected->position + sf::Vector2f{16.f, 24.f};
        const sf::Vector2f aim = sf::Vector2f{playerX_ + 20.f, 502.f} - start;
        const float length = std::sqrt(aim.x * aim.x + aim.y * aim.y);
        const sf::Vector2f velocity =
            length > 0.f ? aim * (240.f / length) : sf::Vector2f{0.f, 240.f};
        enemyBullets_.push_back({start, velocity});
    }

    void updatePlayerBullet(float dt)
    {
        if (!playerBullet_)
            return;

        const sf::Vector2f start = playerBullet_->position;
        const sf::Vector2f delta = playerBullet_->velocity * dt;
        float nearest = 2.f;
        int kind = 0;
        std::size_t index = 0;

        auto test = [&](const sf::FloatRect& box, int type, std::size_t i)
        {
            const auto t = sweptRectangle(start, delta, {2.f, 6.f}, box);
            if (t && *t < nearest)
            {
                nearest = *t;
                kind = type;
                index = i;
            }
        };

        for (std::size_t i = 0; i < walls_.size(); ++i)
            test(walls_[i], 1, i);
        for (std::size_t i = 0; i < enemies_.size(); ++i)
            test(enemies_[i].bounds(), 2, i);
        if (bonusActive_)
            test({{bonusX_, 50.f}, {48.f, 20.f}}, 3, 0);

        if (kind != 0)
        {
            playerBullet_.reset();
            if (kind == 1)
                walls_.erase(walls_.begin() + index);
            else if (kind == 2)
            {
                const int row = enemies_[index].row;
                score_ += row == 0 ? 30 : (row == 1 ? 20 : 10);
                enemies_.erase(enemies_.begin() + index);
                hitSound_.play();
            }
            else
            {
                bonusActive_ = false;
                score_ += 50 * std::uniform_int_distribution<int>(1, 3)(random_);
                bonusSound_.play();
            }
            return;
        }

        playerBullet_->position += delta;
        if (playerBullet_->position.y + 6.f < 0.f)
            playerBullet_.reset();
    }

    void updateEnemyBullets(float dt)
    {
        for (std::size_t i = 0; i < enemyBullets_.size();)
        {
            Bullet& shot = enemyBullets_[i];
            const sf::Vector2f delta = shot.velocity * dt;
            float nearest = 2.f;
            std::optional<std::size_t> wallIndex;

            for (std::size_t w = 0; w < walls_.size(); ++w)
            {
                const auto t = sweptCircle(shot.position, delta, 3.f, walls_[w]);
                if (t && *t < nearest)
                {
                    nearest = *t;
                    wallIndex = w;
                }
            }

            const auto playerHit = sweptCircle(
                shot.position, delta, 3.f,
                {{playerX_, 490.f}, {40.f, 24.f}});

            if (playerHit && *playerHit < nearest)
            {
                --lives_;
                loseSound_.play();
                playerBullet_.reset();
                enemyBullets_.clear();
                if (lives_ == 0)
                    finish();
                else
                    freeze_ = 1.f;
                return;
            }

            if (wallIndex)
            {
                walls_.erase(walls_.begin() + *wallIndex);
                enemyBullets_.erase(enemyBullets_.begin() + i);
                continue;
            }

            shot.position += delta;
            if (shot.position.y - 3.f > Height ||
                shot.position.y + 3.f < 0.f ||
                shot.position.x + 3.f < 0.f ||
                shot.position.x - 3.f > Width)
            {
                enemyBullets_.erase(enemyBullets_.begin() + i);
                continue;
            }
            ++i;
        }
    }

    void text(sf::RenderWindow& window, const std::string& value,
              unsigned int size, sf::Vector2f position)
    {
        sf::Text label(assets_.font, value, size);
        label.setFillColor(sf::Color::White);
        label.setPosition(position);
        window.draw(label);
    }

    void centered(sf::RenderWindow& window, const std::string& value,
                  unsigned int size, float y)
    {
        sf::Text label(assets_.font, value, size);
        const auto bounds = label.getLocalBounds();
        label.setOrigin({bounds.position.x + bounds.size.x / 2.f,
                         bounds.position.y});
        label.setPosition({Width / 2.f, y});
        label.setFillColor(sf::Color::White);
        window.draw(label);
    }

    Assets& assets_;
    sf::Sound shootSound_;
    sf::Sound hitSound_;
    sf::Sound loseSound_;
    sf::Sound bonusSound_;

    Screen screen_ = Screen::Title;
    std::vector<Enemy> enemies_;
    std::vector<sf::FloatRect> walls_;
    std::optional<Bullet> playerBullet_;
    std::vector<Bullet> enemyBullets_;
    std::mt19937 random_;

    int score_ = 0;
    int wave_ = 1;
    int lives_ = 3;
    float playerX_ = 460.f;
    float direction_ = 1.f;
    float titleTime_ = 0.f;
    float animationTime_ = 0.f;
    float shotTimer_ = 0.f;
    float bonusTimer_ = 0.f;
    float freeze_ = 0.f;
    bool bonusActive_ = false;
    bool bonusFromLeft_ = true;
    float bonusX_ = -48.f;
    float bonusDirection_ = 1.f;
};
}

int main()
{
    try
    {
        sf::RenderWindow window(
            sf::VideoMode({960u, 540u}), "STAR FORMATION",
            sf::Style::Titlebar | sf::Style::Close);
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Assets assets;
        if (!assets.load())
            return 1;

        Game game(assets);
        Input input;
        sf::Clock clock;
        double accumulator = 0.0;
        bool focused = true;

        while (window.isOpen())
        {
            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                    window.close();

                if (event->is<sf::Event::FocusLost>())
                {
                    focused = false;
                    input = {};
                }
                if (event->is<sf::Event::FocusGained>())
                    focused = true;

                if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    using Key = sf::Keyboard::Key;
                    if (key->code == Key::Escape)
                        window.close();
                    if (!focused)
                        continue;

                    switch (key->code)
                    {
                        case Key::Left:
                            input.left = input.leftTap = true;
                            break;
                        case Key::A:
                            input.a = input.leftTap = true;
                            break;
                        case Key::Right:
                            input.right = input.rightTap = true;
                            break;
                        case Key::D:
                            input.d = input.rightTap = true;
                            break;
                        case Key::Space: input.fire = true; break;
                        case Key::Enter: input.enter = true; break;
                        case Key::R: input.restart = true; break;
                        default: break;
                    }
                }

                if (const auto* key = event->getIf<sf::Event::KeyReleased>())
                {
                    using Key = sf::Keyboard::Key;
                    switch (key->code)
                    {
                        case Key::Left: input.left = false; break;
                        case Key::A: input.a = false; break;
                        case Key::Right: input.right = false; break;
                        case Key::D: input.d = false; break;
                        default: break;
                    }
                }
            }

            if (!window.isOpen())
                break;

            accumulator += static_cast<double>(clock.restart().asSeconds());
            while (accumulator >= static_cast<double>(Tick))
            {
                game.update(Tick, input);
                accumulator -= static_cast<double>(Tick);
            }

            window.clear(sf::Color::Black);
            game.draw(window);
            window.display();
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ERROR: " << exception.what() << '\n';
        return 1;
    }
}
```