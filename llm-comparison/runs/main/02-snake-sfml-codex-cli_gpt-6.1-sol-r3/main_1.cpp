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
constexpr float ScoreBarHeight = 30.f;

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

std::optional<sf::Vector2i> directionFor(sf::Keyboard::Key key)
{
    using Key = sf::Keyboard::Key;
    switch (key)
    {
        case Key::Up:
        case Key::W:
            return sf::Vector2i{0, -1};
        case Key::Down:
        case Key::S:
            return sf::Vector2i{0, 1};
        case Key::Left:
        case Key::A:
            return sf::Vector2i{-1, 0};
        case Key::Right:
        case Key::D:
            return sf::Vector2i{1, 0};
        default:
            return std::nullopt;
    }
}

template <typename Loader>
bool loadAsset(const char* path, Loader loader)
{
    try
    {
        if (loader(path))
            return true;
    }
    catch (const std::exception& exception)
    {
        std::cerr << path << ": " << exception.what() << '\n';
        return false;
    }

    std::cerr << "FAILED TO LOAD: " << path << '\n';
    return false;
}

int run()
{
    sf::Font font;
    sf::SoundBuffer pointBuffer;
    sf::SoundBuffer loseBuffer;
    sf::Music music;

    if (!loadAsset("assets/fonts/game.ttf",
                   [&](const char* path) { return font.openFromFile(path); }))
        return 1;
    if (!loadAsset("assets/sfx/point.wav",
                   [&](const char* path) { return pointBuffer.loadFromFile(path); }))
        return 1;
    if (!loadAsset("assets/sfx/lose.wav",
                   [&](const char* path) { return loseBuffer.loadFromFile(path); }))
        return 1;
    if (!loadAsset("assets/music/bgm_snake.ogg",
                   [&](const char* path) { return music.openFromFile(path); }))
        return 1;

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

    std::mt19937 random(std::random_device{}());
    Screen screen = Screen::Title;
    std::deque<sf::Vector2i> snake;
    std::deque<sf::Vector2i> directions;
    sf::Vector2i direction{1, 0};
    std::optional<sf::Vector2i> food;
    int score = 0;
    int foodsEaten = 0;
    double interval = 0.125;
    double accumulator = 0.0;
    double titleElapsed = 0.0;

    auto placeFood = [&]()
    {
        std::vector<sf::Vector2i> freeCells;
        freeCells.reserve(Columns * Rows);
        for (int y = 0; y < Rows; ++y)
        {
            for (int x = 0; x < Columns; ++x)
            {
                const sf::Vector2i cell{x, y};
                if (std::find(snake.begin(), snake.end(), cell) == snake.end())
                    freeCells.push_back(cell);
            }
        }

        if (freeCells.empty())
        {
            food.reset();
            return false;
        }

        std::uniform_int_distribution<std::size_t> distribution(
            0, freeCells.size() - 1);
        food = freeCells[distribution(random)];
        return true;
    };

    auto finish = [&](bool collision)
    {
        screen = Screen::Result;
        accumulator = 0.0;
        directions.clear();
        music.stop();
        if (collision)
            loseSound.play();
    };

    auto start = [&]()
    {
        pointSound.stop();
        loseSound.stop();
        music.stop();

        snake = {{16, 8}, {15, 8}, {14, 8}};
        directions.clear();
        direction = {1, 0};
        score = 0;
        foodsEaten = 0;
        interval = 0.125;
        accumulator = 0.0;
        titleElapsed = 0.0;
        food.reset();
        placeFood();

        screen = Screen::Playing;
        music.play();
    };

    auto enqueueDirection = [&](sf::Vector2i next)
    {
        if (directions.size() >= 2)
            return;

        const sf::Vector2i previous =
            directions.empty() ? direction : directions.back();

        if (next == previous ||
            (next.x == -previous.x && next.y == -previous.y))
            return;

        directions.push_back(next);
    };

    auto moveSnake = [&]()
    {
        if (!directions.empty())
        {
            direction = directions.front();
            directions.pop_front();
        }

        const sf::Vector2i next = snake.front() + direction;
        if (next.x < 0 || next.x >= Columns ||
            next.y < 0 || next.y >= Rows)
        {
            finish(true);
            return;
        }

        const bool eating = food && next == *food;
        const std::size_t collisionCount =
            snake.size() - (eating ? 0u : 1u);

        for (std::size_t i = 0; i < collisionCount; ++i)
        {
            if (snake[i] == next)
            {
                finish(true);
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

        // 보드가 가득 찬 경우에도 무한 재추첨 없이 안전하게 종료한다.
        if (!placeFood())
            finish(false);
    };

    auto drawText = [&](const std::string& string,
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
                bounds.position.y + bounds.size.y * 0.5f});
        }
        text.setPosition(position);
        window.draw(text);
    };

    sf::RectangleShape scoreBar({960.f, ScoreBarHeight});
    scoreBar.setFillColor(BarColor);

    sf::RectangleShape segment({28.f, 28.f});
    sf::CircleShape foodShape(12.f);
    foodShape.setFillColor(FoodColor);

    sf::Clock frameClock;
    while (window.isOpen())
    {
        double elapsed =
            static_cast<double>(frameClock.restart().asSeconds());
        bool startedThisFrame = false;

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                    break;
                }

                if (screen == Screen::Title &&
                    key->code == sf::Keyboard::Key::Enter)
                {
                    start();
                    startedThisFrame = true;
                }
                else if (screen == Screen::Result &&
                         key->code == sf::Keyboard::Key::R)
                {
                    start();
                    startedThisFrame = true;
                }
                else if (screen == Screen::Playing)
                {
                    if (const auto next = directionFor(key->code))
                        enqueueDirection(*next);
                }
            }
        }

        if (!window.isOpen())
            break;

        if (startedThisFrame)
        {
            elapsed = 0.0;
            static_cast<void>(frameClock.restart());
        }

        if (screen == Screen::Title)
        {
            titleElapsed += elapsed;
            if (titleElapsed >= 2.0)
            {
                const double overflow = titleElapsed - 2.0;
                start();
                accumulator = overflow;
            }
        }
        else if (screen == Screen::Playing)
        {
            accumulator += elapsed;
        }

        while (screen == Screen::Playing && accumulator >= interval)
        {
            // 가속은 이번 이동을 마친 뒤 다음 간격부터 적용한다.
            accumulator -= interval;
            moveSnake();
        }

        window.clear(BoardColor);

        if (screen == Screen::Title)
        {
            drawText("SNAKE", 48, {480.f, 220.f}, true);
            drawText("PRESS ENTER", 32, {480.f, 310.f}, true);
        }
        else if (screen == Screen::Playing)
        {
            window.draw(scoreBar);
            drawText("SCORE " + std::to_string(score),
                     16, {8.f, 4.f}, false);

            if (food)
            {
                foodShape.setPosition({
                    static_cast<float>(food->x) * CellSize + 3.f,
                    ScoreBarHeight +
                        static_cast<float>(food->y) * CellSize + 3.f});
                window.draw(foodShape);
            }

            for (std::size_t i = 0; i < snake.size(); ++i)
            {
                segment.setFillColor(i == 0 ? HeadColor : BodyColor);
                segment.setPosition({
                    static_cast<float>(snake[i].x) * CellSize + 1.f,
                    ScoreBarHeight +
                        static_cast<float>(snake[i].y) * CellSize + 1.f});
                window.draw(segment);
            }
        }
        else
        {
            drawText("GAME OVER", 48, {480.f, 190.f}, true);
            drawText("SCORE " + std::to_string(score),
                     32, {480.f, 270.f}, true);
            drawText("R: RESTART  ESC: QUIT",
                     32, {480.f, 350.f}, true);
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
