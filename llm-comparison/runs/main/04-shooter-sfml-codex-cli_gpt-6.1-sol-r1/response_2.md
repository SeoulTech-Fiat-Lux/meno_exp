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
#include <limits>
#include <memory>
#include <optional>
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
constexpr double Step = 1.0 / 120.0;
constexpr double Epsilon = 1e-9;

const sf::Color Background{0x14, 0x18, 0x1F};
const sf::Color GridColor{0x22, 0x2A, 0x35};
const sf::Color BorderColor{0x5C, 0x6B, 0x80};

float dot(sf::Vector2f a, sf::Vector2f b)
{
    return a.x * b.x + a.y * b.y;
}

sf::Vector2f normalized(sf::Vector2f v)
{
    const float length = std::sqrt(dot(v, v));
    return length > 0.00001f ? v / length : sf::Vector2f{};
}

std::string decimal(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

struct Assets
{
    sf::Font font;
    sf::Texture player;
    sf::Texture enemy;
    sf::Texture bullet;
    sf::SoundBuffer shoot;
    sf::SoundBuffer hit;
    sf::SoundBuffer lose;
    sf::Music music;

    bool load()
    {
        const auto check = [](bool success, const char* path)
        {
            if (!success)
                std::cerr << "FAILED TO LOAD: " << path << '\n';
            return success;
        };

        if (!check(font.openFromFile("assets/fonts/game.ttf"),
                   "assets/fonts/game.ttf"))
            return false;
        if (!check(player.loadFromFile("assets/textures/player.png"),
                   "assets/textures/player.png"))
            return false;
        if (!check(enemy.loadFromFile("assets/textures/enemy.png"),
                   "assets/textures/enemy.png"))
            return false;
        if (!check(bullet.loadFromFile("assets/textures/bullet.png"),
                   "assets/textures/bullet.png"))
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
        if (!check(music.openFromFile("assets/music/bgm_shooter.ogg"),
                   "assets/music/bgm_shooter.ogg"))
            return false;

        music.setVolume(40.f);
        music.setLooping(true);
        return true;
    }
};

struct Input
{
    // Only movement keys require persistent and short-press state.
    static constexpr std::array<sf::Keyboard::Key, 8> MovementKeys{
        sf::Keyboard::Key::A,
        sf::Keyboard::Key::D,
        sf::Keyboard::Key::W,
        sf::Keyboard::Key::S,
        sf::Keyboard::Key::Left,
        sf::Keyboard::Key::Right,
        sf::Keyboard::Key::Up,
        sf::Keyboard::Key::Down
    };

    std::array<bool, MovementKeys.size()> held{};
    std::array<bool, MovementKeys.size()> tapped{};
    bool mouseHeld = false;
    bool mouseTapped = false;

    void key(sf::Keyboard::Key code, bool down)
    {
        for (std::size_t i = 0; i < MovementKeys.size(); ++i)
        {
            if (MovementKeys[i] == code)
            {
                held[i] = down;
                if (down)
                    tapped[i] = true;
                return;
            }
        }
    }

    bool active(sf::Keyboard::Key code) const
    {
        for (std::size_t i = 0; i < MovementKeys.size(); ++i)
        {
            if (MovementKeys[i] == code)
                return held[i] || tapped[i];
        }
        return false;
    }

    void consume()
    {
        tapped.fill(false);
        mouseTapped = false;
    }

    void clear()
    {
        held.fill(false);
        mouseHeld = false;
        consume();
    }
};

struct Bullet
{
    sf::Vector2f position;
    sf::Vector2f previous;
    sf::Vector2f velocity;
    double age = 0.0;
    bool alive = true;
};

struct Enemy
{
    sf::Vector2f position;
    sf::Vector2f previous;
    bool alive = true;
};

std::optional<float> impactTime(sf::Vector2f start,
                                sf::Vector2f end,
                                float radius)
{
    const float c = dot(start, start) - radius * radius;
    if (c <= 0.f)
        return 0.f;

    const sf::Vector2f displacement = end - start;
    const float a = dot(displacement, displacement);
    if (a <= 0.000001f)
        return std::nullopt;

    const float b = 2.f * dot(start, displacement);
    const float discriminant = b * b - 4.f * a * c;
    if (discriminant < 0.f)
        return std::nullopt;

    const float t = (-b - std::sqrt(discriminant)) / (2.f * a);
    if (t >= 0.f && t <= 1.f)
        return t;
    return std::nullopt;
}

class Game
{
public:
    enum class Screen
    {
        Title,
        Playing,
        Result
    };

    Game(sf::RenderWindow& window, Assets& assets)
        : window_(window),
          assets_(assets),
          playerSprite_(assets.player),
          enemySprite_(assets.enemy),
          bulletSprite_(assets.bullet),
          text_(assets.font),
          grid_(sf::PrimitiveType::Lines),
          random_(std::random_device{}())
    {
        playerSprite_.setOrigin({16.f, 16.f});
        enemySprite_.setOrigin({16.f, 16.f});
        bulletSprite_.setOrigin({4.f, 4.f});

        bullets_.reserve(10);
        enemies_.reserve(200);
        sounds_.reserve(32);

        for (int i = 0; i < 32; ++i)
            sounds_.push_back(std::make_unique<sf::Sound>(assets_.shoot));

        for (float x = 120.f; x < WorldWidth; x += 120.f)
        {
            grid_.append(sf::Vertex{{x, 0.f}, GridColor});
            grid_.append(sf::Vertex{{x, WorldHeight}, GridColor});
        }
        for (float y = 120.f; y < WorldHeight; y += 120.f)
        {
            grid_.append(sf::Vertex{{0.f, y}, GridColor});
            grid_.append(sf::Vertex{{WorldWidth, y}, GridColor});
        }

        resize(window_.getSize());
        updateCamera();
    }

    Screen screen() const { return screen_; }
    bool paused() const { return paused_; }

    void resize(sf::Vector2u size)
    {
        if (size.x == 0 || size.y == 0)
            return;

        const float aspect = static_cast<float>(size.x) /
                             static_cast<float>(size.y);
        constexpr float desired = ViewWidth / ViewHeight;

        sf::FloatRect viewport({0.f, 0.f}, {1.f, 1.f});
        if (aspect > desired)
        {
            viewport.size.x = desired / aspect;
            viewport.position.x = (1.f - viewport.size.x) * 0.5f;
        }
        else
        {
            viewport.size.y = aspect / desired;
            viewport.position.y = (1.f - viewport.size.y) * 0.5f;
        }

        uiView_ = sf::View({ViewWidth * 0.5f, ViewHeight * 0.5f},
                           {ViewWidth, ViewHeight});
        uiView_.setViewport(viewport);
        worldView_.setSize({ViewWidth, ViewHeight});
        worldView_.setViewport(viewport);
    }

    void start()
    {
        for (auto& sound : sounds_)
            sound->stop();

        soundCursor_ = 0;
        screen_ = Screen::Playing;
        paused_ = false;
        player_ = {WorldWidth * 0.5f, WorldHeight * 0.5f};
        aim_ = {1.f, 0.f};
        hp_ = 3;
        score_ = 0;
        time_ = 0.0;
        invulnerability_ = 0.0;
        shotCooldown_ = 0.0;
        nextSpawn_ = 0.5;
        titleTime_ = 0.0;
        bullets_.clear();
        enemies_.clear();
        updateCamera();

        assets_.music.stop();
        assets_.music.play();
    }

    void togglePause()
    {
        if (screen_ != Screen::Playing)
            return;

        paused_ = !paused_;
        if (paused_)
            assets_.music.pause();
        else
            assets_.music.play();
    }

    bool advanceTitle(double dt)
    {
        if (screen_ != Screen::Title)
            return false;

        titleTime_ += dt;
        if (titleTime_ >= 2.0)
        {
            start();
            return true;
        }
        return false;
    }

    void updateAim(sf::Vector2i mouse)
    {
        const sf::Vector2f target =
            window_.mapPixelToCoords(mouse, worldView_);
        const sf::Vector2f direction = normalized(target - player_);
        if (dot(direction, direction) > 0.f)
            aim_ = direction;
    }

    void update(Input& input, sf::Vector2i mouse)
    {
        if (screen_ != Screen::Playing || paused_)
            return;

        constexpr float dt = static_cast<float>(Step);
        time_ += Step;
        invulnerability_ = std::max(0.0, invulnerability_ - Step);
        shotCooldown_ = std::max(0.0, shotCooldown_ - Step);

        using Key = sf::Keyboard::Key;
        sf::Vector2f movement{};
        if (input.active(Key::A) || input.active(Key::Left))
            movement.x -= 1.f;
        if (input.active(Key::D) || input.active(Key::Right))
            movement.x += 1.f;
        if (input.active(Key::W) || input.active(Key::Up))
            movement.y -= 1.f;
        if (input.active(Key::S) || input.active(Key::Down))
            movement.y += 1.f;

        player_ += normalized(movement) * (240.f * dt);
        player_.x = std::clamp(player_.x, 16.f, WorldWidth - 16.f);
        player_.y = std::clamp(player_.y, 16.f, WorldHeight - 16.f);
        updateCamera();
        updateAim(mouse);

        while (time_ + Epsilon >= nextSpawn_)
        {
            if (enemies_.size() < 200)
                spawnEnemy();

            const double interval =
                std::max(0.3, 1.0 - 0.1 * std::floor(nextSpawn_ / 10.0));
            nextSpawn_ += interval;
        }

        for (auto& enemy : enemies_)
        {
            enemy.previous = enemy.position;
            enemy.position += normalized(player_ - enemy.position) *
                              (100.f * dt);
        }

        for (auto& bullet : bullets_)
        {
            bullet.previous = bullet.position;
            bullet.position += bullet.velocity * dt;
            bullet.age += Step;

            Enemy* victim = nullptr;
            float earliest = std::numeric_limits<float>::max();

            for (auto& enemy : enemies_)
            {
                if (!enemy.alive)
                    continue;

                const auto impact =
                    impactTime(bullet.previous - enemy.previous,
                               bullet.position - enemy.position, 18.f);
                if (impact && *impact < earliest)
                {
                    earliest = *impact;
                    victim = &enemy;
                }
            }

            if (victim)
            {
                victim->alive = false;
                bullet.alive = false;
                score_ += 10;
                playSound(assets_.hit);
            }

            if (bullet.age + Epsilon >= 1.4 ||
                bullet.position.x < 0.f ||
                bullet.position.x > WorldWidth ||
                bullet.position.y < 0.f ||
                bullet.position.y > WorldHeight)
            {
                bullet.alive = false;
            }
        }

        if (invulnerability_ <= Epsilon)
        {
            for (auto& enemy : enemies_)
            {
                if (!enemy.alive)
                    continue;

                const sf::Vector2f distance = enemy.position - player_;
                if (dot(distance, distance) <= 28.f * 28.f)
                {
                    enemy.alive = false;
                    --hp_;
                    invulnerability_ = 1.0;
                    playSound(assets_.lose);
                    break;
                }
            }
        }

        std::erase_if(bullets_, [](const Bullet& b) { return !b.alive; });
        std::erase_if(enemies_, [](const Enemy& e) { return !e.alive; });

        if (hp_ <= 0)
        {
            screen_ = Screen::Result;
            assets_.music.stop();
            return;
        }

        if ((input.mouseHeld || input.mouseTapped) &&
            shotCooldown_ <= Epsilon)
        {
            bullets_.push_back(
                Bullet{player_, player_, aim_ * 600.f, 0.0, true});
            shotCooldown_ = 0.15;
            playSound(assets_.shoot);
        }
    }

    void render()
    {
        window_.clear(sf::Color::Black);
        window_.setView(uiView_);
        rectangle({0.f, 0.f}, {ViewWidth, ViewHeight}, Background);

        if (screen_ == Screen::Title)
        {
            centered("ARENA", 48, 185.f);
            centered("PRESS ENTER", 32, 285.f);
        }
        else if (screen_ == Screen::Result)
        {
            centered("GAME OVER", 48, 130.f);
            centered("SCORE " + std::to_string(score_), 32, 225.f);
            centered("TIME " + decimal(time_), 32, 280.f);
            centered("R: RESTART  ESC: QUIT", 16, 375.f);
        }
        else
        {
            window_.setView(worldView_);
            rectangle({0.f, 0.f}, {WorldWidth, WorldHeight}, Background);
            window_.draw(grid_);

            sf::RectangleShape border({WorldWidth, WorldHeight});
            border.setFillColor(sf::Color::Transparent);
            border.setOutlineColor(BorderColor);
            border.setOutlineThickness(-2.f);
            window_.draw(border);

            for (const auto& bullet : bullets_)
            {
                bulletSprite_.setPosition(bullet.position);
                window_.draw(bulletSprite_);
            }

            for (const auto& enemy : enemies_)
            {
                enemySprite_.setPosition(enemy.position);
                window_.draw(enemySprite_);
            }

            playerSprite_.setPosition(player_);
            playerSprite_.setRotation(
                sf::radians(std::atan2(aim_.y, aim_.x)));

            sf::Color playerColor = sf::Color::White;
            if (invulnerability_ > Epsilon)
            {
                playerColor.a =
                    static_cast<int>(invulnerability_ * 12.0) % 2 == 0
                        ? 80
                        : 180;
            }
            playerSprite_.setColor(playerColor);
            window_.draw(playerSprite_);

            window_.setView(uiView_);
            label("HP " + std::to_string(hp_) +
                      "  SCORE " + std::to_string(score_) +
                      "  TIME " + decimal(time_),
                  16, {16.f, 12.f});

            if (paused_)
                centered("PAUSED", 48, 235.f);
        }

        window_.display();
    }

private:
    void updateCamera()
    {
        worldView_.setCenter(
            {std::clamp(player_.x, ViewWidth * 0.5f,
                        WorldWidth - ViewWidth * 0.5f),
             std::clamp(player_.y, ViewHeight * 0.5f,
                        WorldHeight - ViewHeight * 0.5f)});
    }

    void spawnEnemy()
    {
        const sf::Vector2f center = worldView_.getCenter();
        const float left = center.x - ViewWidth * 0.5f - 64.f;
        const float right = center.x + ViewWidth * 0.5f + 64.f;
        const float top = center.y - ViewHeight * 0.5f - 64.f;
        const float bottom = center.y + ViewHeight * 0.5f + 64.f;

        struct Segment
        {
            sf::Vector2f a;
            sf::Vector2f b;
            float length;
        };

        std::array<Segment, 4> segments{};
        std::size_t count = 0;
        float total = 0.f;

        const auto add = [&](sf::Vector2f a, sf::Vector2f b)
        {
            const float length = std::sqrt(dot(b - a, b - a));
            if (length > 0.f)
            {
                segments[count++] = {a, b, length};
                total += length;
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

        if (count == 0)
            return;

        float choice =
            std::uniform_real_distribution<float>(0.f, total)(random_);

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto& segment = segments[i];
            if (choice <= segment.length || i + 1 == count)
            {
                const float fraction =
                    std::clamp(choice / segment.length, 0.f, 1.f);
                const sf::Vector2f position =
                    segment.a + (segment.b - segment.a) * fraction;
                enemies_.push_back(Enemy{position, position, true});
                return;
            }
            choice -= segment.length;
        }
    }

    void playSound(const sf::SoundBuffer& buffer)
    {
        sf::Sound* selected = nullptr;
        for (auto& sound : sounds_)
        {
            if (sound->getStatus() == sf::SoundSource::Status::Stopped)
            {
                selected = sound.get();
                break;
            }
        }

        if (!selected)
        {
            selected = sounds_[soundCursor_].get();
            soundCursor_ = (soundCursor_ + 1) % sounds_.size();
            selected->stop();
        }

        selected->setBuffer(buffer);
        selected->play();
    }

    void rectangle(sf::Vector2f position, sf::Vector2f size,
                   sf::Color color)
    {
        sf::RectangleShape shape(size);
        shape.setPosition(position);
        shape.setFillColor(color);
        window_.draw(shape);
    }

    void label(const std::string& string, unsigned int size,
               sf::Vector2f position)
    {
        text_.setString(string);
        text_.setCharacterSize(size);
        text_.setFillColor(sf::Color::White);
        text_.setOrigin({0.f, 0.f});
        text_.setPosition(position);
        window_.draw(text_);
    }

    void centered(const std::string& string, unsigned int size, float y)
    {
        text_.setString(string);
        text_.setCharacterSize(size);
        text_.setFillColor(sf::Color::White);

        const sf::FloatRect bounds = text_.getLocalBounds();
        text_.setOrigin(
            {bounds.position.x + bounds.size.x * 0.5f,
             bounds.position.y});
        text_.setPosition({ViewWidth * 0.5f, y});
        window_.draw(text_);
    }

    sf::RenderWindow& window_;
    Assets& assets_;
    sf::Sprite playerSprite_;
    sf::Sprite enemySprite_;
    sf::Sprite bulletSprite_;
    sf::Text text_;
    sf::VertexArray grid_;
    sf::View worldView_;
    sf::View uiView_;

    std::mt19937 random_;
    std::vector<Bullet> bullets_;
    std::vector<Enemy> enemies_;
    std::vector<std::unique_ptr<sf::Sound>> sounds_;
    std::size_t soundCursor_ = 0;

    Screen screen_ = Screen::Title;
    bool paused_ = false;
    sf::Vector2f player_{WorldWidth * 0.5f, WorldHeight * 0.5f};
    sf::Vector2f aim_{1.f, 0.f};
    int hp_ = 3;
    int score_ = 0;
    double titleTime_ = 0.0;
    double time_ = 0.0;
    double invulnerability_ = 0.0;
    double shotCooldown_ = 0.0;
    double nextSpawn_ = 0.5;
};
}

int main()
{
    try
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        Assets assets;
        if (!assets.load())
            return 1;

        Game game(window, assets);
        Input input;
        sf::Clock clock;
        double accumulator = 0.0;

        while (window.isOpen())
        {
            const double frameTime = clock.restart().asSeconds();
            bool resetTiming = false;

            while (const auto event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                }
                else if (const auto* resized =
                             event->getIf<sf::Event::Resized>())
                {
                    game.resize(resized->size);
                }
                else if (event->is<sf::Event::FocusLost>())
                {
                    input.clear();
                }
                else if (const auto* key =
                             event->getIf<sf::Event::KeyPressed>())
                {
                    input.key(key->code, true);
                    using Key = sf::Keyboard::Key;

                    if (key->code == Key::Escape)
                    {
                        window.close();
                    }
                    else if ((game.screen() == Game::Screen::Title &&
                              key->code == Key::Enter) ||
                             (game.screen() == Game::Screen::Result &&
                              key->code == Key::R))
                    {
                        game.start();
                        input.consume();
                        resetTiming = true;
                    }
                    else if (game.screen() == Game::Screen::Playing &&
                             key->code == Key::P)
                    {
                        game.togglePause();
                        input.consume();
                        resetTiming = true;
                    }
                }
                else if (const auto* key =
                             event->getIf<sf::Event::KeyReleased>())
                {
                    input.key(key->code, false);
                }
                else if (const auto* mouse =
                             event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button == sf::Mouse::Button::Left)
                    {
                        input.mouseHeld = true;
                        input.mouseTapped = true;
                    }
                }
                else if (const auto* mouse =
                             event->getIf<sf::Event::MouseButtonReleased>())
                {
                    if (mouse->button == sf::Mouse::Button::Left)
                        input.mouseHeld = false;
                }
            }

            if (!window.isOpen())
                break;

            if (game.advanceTitle(resetTiming ? 0.0 : frameTime))
            {
                resetTiming = true;
                input.consume();
            }

            const sf::Vector2i mouse = sf::Mouse::getPosition(window);

            if (resetTiming || game.paused() ||
                game.screen() != Game::Screen::Playing)
            {
                accumulator = 0.0;
            }
            else
            {
                accumulator += frameTime;
                while (accumulator + Epsilon >= Step &&
                       game.screen() == Game::Screen::Playing)
                {
                    game.update(input, mouse);
                    input.consume();
                    accumulator = std::max(0.0, accumulator - Step);
                }
            }

            if (game.paused() ||
                game.screen() != Game::Screen::Playing)
            {
                input.consume();
            }

            if (game.screen() == Game::Screen::Playing)
                game.updateAim(mouse);

            game.render();
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
```