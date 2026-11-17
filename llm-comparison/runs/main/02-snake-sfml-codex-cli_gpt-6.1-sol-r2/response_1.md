```cpp
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
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
constexpr float HeaderHeight = 30.f;

const sf::Color BoardColor{0x0E, 0x1A, 0x12};
const sf::Color HeaderColor{0x1F, 0x3A, 0x26};
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

std::optional<sf::Vector2i> directionFor(sf::Keyboard::Key key)
{
    using Key = sf::Keyboard::Key;
    switch (key)
    {
        case Key::Up:
        case Key::W:     return sf::Vector2i{0, -1};
        case Key::Down:
        case Key::S:     return sf::Vector2i{0, 1};
        case Key::Left:
        case Key::A:     return sf::Vector2i{-1, 0};
        case Key::Right:
        case Key::D:     return sf::Vector2i{1, 0};
        default:        return std::nullopt;
    }
}

int run()
{
    sf::Font font;
    sf::SoundBuffer pointBuffer;
    sf::SoundBuffer loseBuffer;
    sf::Music music;

    const auto assetFailure = [](const char* path)
    {
        std::cerr << "FAILED TO LOAD: " << path << '\n';
        return 1;
    };

    if (!font.openFromFile("assets/fonts/game.ttf"))
        return assetFailure("assets/fonts/game.ttf");
    if (!pointBuffer.loadFromFile("assets/sfx/point.wav"))
        return assetFailure("assets/sfx/point.wav");
    if (!loseBuffer.loadFromFile("assets/sfx/lose.wav"))
        return assetFailure("assets/sfx/lose.wav");
    if (!music.openFromFile("assets/music/bgm_snake.ogg"))
        return assetFailure("assets/music/bgm_snake.ogg");

    sf::Sound pointSound(pointBuffer);
    sf::Sound loseSound(loseBuffer);
    music.setVolume(40.f);
    music.setLooping(true);

    sf::RenderWindow window(
        sf::VideoMode({960u, 540u}),
        "SNAKE",
        sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    Screen screen = Screen::Title;
    std::deque<sf::Vector2i> snake;
    std::deque<sf::Vector2i> pendingDirections;
    sf::Vector2i direction{1, 0};
    std::optional<sf::Vector2i> food;

    int score = 0;
    int foodsEaten = 0;
    double interval = 0.125;
    double accumulator = 0.0;

    std::mt19937 random(std::random_device{}());
    sf::Clock clock;
    double lastUpdate = 0.0;

    const auto secondsNow = [&]()
    {
        return static_cast<double>(clock.getElapsedTime().asMicroseconds())
             / 1'000'000.0;
    };

    const auto placeFood = [&]()
    {
        std::vector<sf::Vector2i> emptyCells;
        emptyCells.reserve(Columns * Rows);

        for (int y = 0; y < Rows; ++y)
        {
            for (int x = 0; x < Columns; ++x)
            {
                const sf::Vector2i cell{x, y};
                if (std::find(snake.begin(), snake.end(), cell) == snake.end())
                    emptyCells.push_back(cell);
            }
        }

        if (emptyCells.empty())
        {
            food.reset();
            return;
        }

        const std::size_t index =
            std::uniform_int_distribution<std::size_t>(
                0, emptyCells.size() - 1)(random);
        food = emptyCells[index];
    };

    const auto startGame = [&](double now)
    {
        music.stop();
        pointSound.stop();
        loseSound.stop();

        snake = {{16, 8}, {15, 8}, {14, 8}};
        pendingDirections.clear();
        direction = {1, 0};
        score = 0;
        foodsEaten = 0;
        interval = 0.125;
        accumulator = 0.0;
        lastUpdate = now;
        placeFood();
        screen = Screen::Playing;
        music.play();
    };

    const auto finishGame = [&](bool collision)
    {
        screen = Screen::Result;
        accumulator = 0.0;
        pendingDirections.clear();
        music.stop();
        if (collision)
            loseSound.play();
    };

    const auto moveSnake = [&]()
    {
        if (!pendingDirections.empty())
        {
            direction = pendingDirections.front();
            pendingDirections.pop_front();
        }

        const sf::Vector2i next = snake.front() + direction;
        const bool eating = food && next == *food;

        if (next.x < 0 || next.x >= Columns ||
            next.y < 0 || next.y >= Rows)
        {
            finishGame(true);
            return;
        }

        // A non-growing move vacates its tail before occupying the new cell.
        const std::size_t collisionCount =
            snake.size() - (eating ? 0u : 1u);
        for (std::size_t i = 0; i < collisionCount; ++i)
        {
            if (snake[i] == next)
            {
                finishGame(true);
                return;
            }
        }

        snake.push_front(next);
        if (!eating)
        {
            snake.pop_back();
            return;
        }

        score += 10;
        ++foodsEaten;
        pointSound.play();

        if (foodsEaten % 5 == 0)
            interval = std::max(0.06, interval * 0.9);

        placeFood();
        if (!food)
            finishGame(false);
    };

    const auto drawText = [&](const std::string& string,
                              unsigned int size,
                              sf::Vector2f position,
                              bool centered)
    {
        sf::Text text(font, string, size);
        text.setFillColor(TextColor);
        if (centered)
        {
            const sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin({
                bounds.position.x + bounds.size.x * 0.5f,
                bounds.position.y + bounds.size.y * 0.5f
            });
        }
        text.setPosition(position);
        window.draw(text);
    };

    while (window.isOpen())
    {
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }

            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (!key)
                continue;

            if (key->code == sf::Keyboard::Key::Escape)
            {
                window.close();
                break;
            }

            if (screen == Screen::Title &&
                key->code == sf::Keyboard::Key::Enter)
            {
                startGame(secondsNow());
            }
            else if (screen == Screen::Result &&
                     key->code == sf::Keyboard::Key::R)
            {
                startGame(secondsNow());
            }
            else if (screen == Screen::Playing)
            {
                if (const auto requested = directionFor(key->code))
                {
                    const sf::Vector2i previous = pendingDirections.empty()
                        ? direction : pendingDirections.back();

                    if (pendingDirections.size() < 2 &&
                        *requested != previous &&
                        *requested != sf::Vector2i{-previous.x, -previous.y})
                    {
                        pendingDirections.push_back(*requested);
                    }
                }
            }
        }

        if (!window.isOpen())
            break;

        const double now = secondsNow();

        if (screen == Screen::Title && now >= 2.0)
            startGame(2.0);

        if (screen == Screen::Playing)
        {
            accumulator += now - lastUpdate;
            lastUpdate = now;

            while (screen == Screen::Playing && accumulator >= interval)
            {
                // Subtract the old interval; eating may change the next one.
                accumulator -= interval;
                moveSnake();
            }
        }

        window.clear(BoardColor);

        if (screen == Screen::Title)
        {
            drawText("SNAKE", 48, {480.f, 220.f}, true);
            drawText("PRESS ENTER", 32, {480.f, 310.f}, true);
        }
        else if (screen == Screen::Playing)
        {
            sf::RectangleShape header({960.f, HeaderHeight});
            header.setFillColor(HeaderColor);
            window.draw(header);
            drawText("SCORE " + std::to_string(score),
                     16, {10.f, 4.f}, false);

            if (food)
            {
                sf::CircleShape foodShape(12.f);
                foodShape.setFillColor(FoodColor);
                foodShape.setPosition({
                    static_cast<float>(food->x) * CellSize + 3.f,
                    HeaderHeight +
                        static_cast<float>(food->y) * CellSize + 3.f
                });
                window.draw(foodShape);
            }

            sf::RectangleShape segment({28.f, 28.f});
            for (std::size_t i = 0; i < snake.size(); ++i)
            {
                segment.setFillColor(i == 0 ? HeadColor : BodyColor);
                segment.setPosition({
                    static_cast<float>(snake[i].x) * CellSize + 1.f,
                    HeaderHeight +
                        static_cast<float>(snake[i].y) * CellSize + 1.f
                });
                window.draw(segment);
            }
        }
        else
        {
            drawText("GAME OVER", 48, {480.f, 190.f}, true);
            drawText("SCORE " + std::to_string(score),
                     32, {480.f, 275.f}, true);
            drawText("R: RESTART  ESC: QUIT",
                     32, {480.f, 355.f}, true);
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
```