#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr int Columns = 32;
constexpr int Rows = 17;
constexpr float CellSize = 30.f;
constexpr float BoardTop = 30.f;

const sf::Color BoardColor{0x0E, 0x1A, 0x12};
const sf::Color BarColor{0x1F, 0x3A, 0x26};
const sf::Color BodyColor{0x5F, 0xCB, 0x5F};
const sf::Color HeadColor{0xB6, 0xF2, 0xA0};
const sf::Color FoodColor{0xE5, 0x53, 0x3D};
const sf::Color TextColor{0xE8, 0xF5, 0xE9};

enum class Screen
{
    Title,
    Playing,
    Result
};

struct Cell
{
    int x;
    int y;

    bool operator==(const Cell&) const = default;
};

bool opposite(Cell a, Cell b)
{
    return a.x == -b.x && a.y == -b.y;
}

std::optional<Cell> directionFor(sf::Keyboard::Key key)
{
    using Key = sf::Keyboard::Key;
    switch (key)
    {
        case Key::Up:
        case Key::W:
            return Cell{0, -1};
        case Key::Down:
        case Key::S:
            return Cell{0, 1};
        case Key::Left:
        case Key::A:
            return Cell{-1, 0};
        case Key::Right:
        case Key::D:
            return Cell{1, 0};
        default:
            return std::nullopt;
    }
}

void drawText(sf::RenderTarget& target, const sf::Font& font,
              const std::string& string, unsigned int size,
              sf::Vector2f position, bool centered = false)
{
    sf::Text text(font, string, size);
    text.setFillColor(TextColor);

    if (centered)
    {
        const auto bounds = text.getLocalBounds();
        text.setOrigin({
            bounds.position.x + bounds.size.x / 2.f,
            bounds.position.y + bounds.size.y / 2.f
        });
    }

    text.setPosition(position);
    target.draw(text);
}

int run()
{
    sf::Font font;
    sf::SoundBuffer pointBuffer;
    sf::SoundBuffer loseBuffer;
    sf::Music music;

    const auto failed = [](const char* path)
    {
        std::cerr << "FAILED TO LOAD: " << path << '\n';
        return 1;
    };

    if (!font.openFromFile("assets/fonts/game.ttf"))
        return failed("assets/fonts/game.ttf");
    if (!pointBuffer.loadFromFile("assets/sfx/point.wav"))
        return failed("assets/sfx/point.wav");
    if (!loseBuffer.loadFromFile("assets/sfx/lose.wav"))
        return failed("assets/sfx/lose.wav");
    if (!music.openFromFile("assets/music/bgm_snake.ogg"))
        return failed("assets/music/bgm_snake.ogg");

    sf::Sound pointSound(pointBuffer);
    sf::Sound loseSound(loseBuffer);
    music.setVolume(40.f);
    music.setLooping(true);

    sf::RenderWindow window(
        sf::VideoMode({960u, 540u}), "SNAKE",
        sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    std::mt19937 random(std::random_device{}());

    Screen screen = Screen::Title;
    std::deque<Cell> snake;
    std::deque<Cell> turns;
    Cell direction{1, 0};
    std::optional<Cell> food;
    int score = 0;
    int eaten = 0;
    double interval = 0.125;
    double accumulator = 0.0;
    double titleElapsed = 0.0;

    const auto placeFood = [&]()
    {
        std::vector<Cell> empty;
        empty.reserve(Columns * Rows);

        for (int y = 0; y < Rows; ++y)
        {
            for (int x = 0; x < Columns; ++x)
            {
                const Cell cell{x, y};
                if (std::find(snake.begin(), snake.end(), cell) == snake.end())
                    empty.push_back(cell);
            }
        }

        if (empty.empty())
        {
            food.reset();
            return false;
        }

        const auto index =
            std::uniform_int_distribution<std::size_t>(
                0, empty.size() - 1)(random);
        food = empty[index];
        return true;
    };

    const auto startGame = [&]()
    {
        music.stop();
        pointSound.stop();
        loseSound.stop();

        snake = {{16, 8}, {15, 8}, {14, 8}};
        turns.clear();
        direction = {1, 0};
        score = 0;
        eaten = 0;
        interval = 0.125;
        accumulator = 0.0;
        titleElapsed = 0.0;
        food.reset();
        placeFood();
        screen = Screen::Playing;
        music.play();
    };

    const auto finishGame = [&](bool collision)
    {
        screen = Screen::Result;
        accumulator = 0.0;
        turns.clear();
        music.stop();
        if (collision)
            loseSound.play();
    };

    const auto moveSnake = [&]()
    {
        if (!turns.empty())
        {
            direction = turns.front();
            turns.pop_front();
        }

        const Cell next{
            snake.front().x + direction.x,
            snake.front().y + direction.y
        };

        if (next.x < 0 || next.x >= Columns ||
            next.y < 0 || next.y >= Rows)
        {
            finishGame(true);
            return;
        }

        const bool growing = food && next == *food;
        const std::size_t collisionCount =
            snake.size() - (growing ? 0u : 1u);

        for (std::size_t i = 0; i < collisionCount; ++i)
        {
            if (snake[i] == next)
            {
                finishGame(true);
                return;
            }
        }

        snake.push_front(next);

        if (!growing)
        {
            snake.pop_back();
            return;
        }

        score += 10;
        ++eaten;
        pointSound.play();

        if (eaten % 5 == 0)
            interval = std::max(0.06, interval * 0.9);

        // A completely filled board has no remaining food cell.
        if (!placeFood())
            finishGame(false);
    };

    sf::Clock clock;

    while (window.isOpen())
    {
        double elapsed = clock.restart().asSeconds();
        bool startedByInput = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                continue;
            }

            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (!key)
                continue;

            if (key->code == sf::Keyboard::Key::Escape)
            {
                window.close();
                continue;
            }

            if (screen == Screen::Title &&
                key->code == sf::Keyboard::Key::Enter)
            {
                startGame();
                startedByInput = true;
            }
            else if (screen == Screen::Result &&
                     key->code == sf::Keyboard::Key::R)
            {
                startGame();
                startedByInput = true;
            }
            else if (screen == Screen::Playing)
            {
                if (const auto requested = directionFor(key->code))
                {
                    const Cell previous =
                        turns.empty() ? direction : turns.back();

                    if (turns.size() < 2 &&
                        *requested != previous &&
                        !opposite(*requested, previous))
                    {
                        turns.push_back(*requested);
                    }
                }
            }
        }

        if (!window.isOpen())
            break;

        if (startedByInput)
            elapsed = 0.0;

        if (screen == Screen::Title)
        {
            titleElapsed += elapsed;
            if (titleElapsed >= 2.0)
            {
                const double remainder = titleElapsed - 2.0;
                startGame();
                elapsed = remainder;
            }
        }

        if (screen == Screen::Playing)
        {
            accumulator += elapsed;

            while (screen == Screen::Playing && accumulator >= interval)
            {
                const double stepInterval = interval;
                accumulator -= stepInterval;
                moveSnake();
            }
        }

        window.clear(BoardColor);

        if (screen == Screen::Title)
        {
            drawText(window, font, "SNAKE", 48, {480.f, 220.f}, true);
            drawText(window, font, "PRESS ENTER", 32, {480.f, 310.f}, true);
        }
        else if (screen == Screen::Playing)
        {
            sf::RectangleShape bar({960.f, BoardTop});
            bar.setFillColor(BarColor);
            window.draw(bar);

            drawText(window, font, "SCORE " + std::to_string(score),
                     16, {8.f, 3.f});

            if (food)
            {
                sf::CircleShape circle(12.f);
                circle.setFillColor(FoodColor);
                circle.setPosition({
                    static_cast<float>(food->x) * CellSize + 3.f,
                    BoardTop + static_cast<float>(food->y) * CellSize + 3.f
                });
                window.draw(circle);
            }

            sf::RectangleShape segment({28.f, 28.f});
            for (std::size_t i = snake.size(); i > 0; --i)
            {
                const std::size_t index = i - 1;
                segment.setFillColor(index == 0 ? HeadColor : BodyColor);
                segment.setPosition({
                    static_cast<float>(snake[index].x) * CellSize + 1.f,
                    BoardTop +
                        static_cast<float>(snake[index].y) * CellSize + 1.f
                });
                window.draw(segment);
            }
        }
        else
        {
            drawText(window, font, "GAME OVER", 48,
                     {480.f, 190.f}, true);
            drawText(window, font, "SCORE " + std::to_string(score),
                     32, {480.f, 275.f}, true);
            drawText(window, font, "R: RESTART  ESC: QUIT",
                     32, {480.f, 360.f}, true);
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
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
