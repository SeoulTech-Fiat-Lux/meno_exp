// SNAKE - SFML 3.1 구현 (main.cpp 한 파일)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr unsigned int kWindowW = 960;
constexpr unsigned int kWindowH = 540;
constexpr int kCols = 32;
constexpr int kRows = 17;
constexpr float kCell = 30.f;
constexpr float kBarH = 30.f;
constexpr float kStartInterval = 0.125f;
constexpr float kMinInterval = 0.06f;
constexpr float kTitleTime = 2.f;

const sf::Color kBoardBg(0x0E, 0x1A, 0x12);
const sf::Color kBarColor(0x1F, 0x3A, 0x26);
const sf::Color kBodyColor(0x5F, 0xCB, 0x5F);
const sf::Color kHeadColor(0xB6, 0xF2, 0xA0);
const sf::Color kFoodColor(0xE5, 0x53, 0x3D);
const sf::Color kTextColor(0xE8, 0xF5, 0xE9);

enum class Screen
{
    Title,
    Play,
    Result
};

void centerText(sf::Text& text, sf::Vector2f center)
{
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    text.setPosition(center);
}

bool isKey(const sf::Event::KeyPressed& k, sf::Keyboard::Key key, sf::Keyboard::Scan scan)
{
    return k.code == key || k.scancode == scan;
}
} // namespace

int main()
{
    // 에셋 로드
    const std::string fontPath = "assets/fonts/game.ttf";
    const std::string pointPath = "assets/sfx/point.wav";
    const std::string losePath = "assets/sfx/lose.wav";
    const std::string musicPath = "assets/music/bgm_snake.ogg";

    sf::Font font;
    if (!font.openFromFile(fontPath))
    {
        std::cerr << "Failed to load asset: " << fontPath << '\n';
        return 1;
    }
    sf::SoundBuffer pointBuffer;
    if (!pointBuffer.loadFromFile(pointPath))
    {
        std::cerr << "Failed to load asset: " << pointPath << '\n';
        return 1;
    }
    sf::SoundBuffer loseBuffer;
    if (!loseBuffer.loadFromFile(losePath))
    {
        std::cerr << "Failed to load asset: " << losePath << '\n';
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile(musicPath))
    {
        std::cerr << "Failed to load asset: " << musicPath << '\n';
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    sf::Sound pointSound(pointBuffer);
    sf::Sound loseSound(loseBuffer);

    sf::RenderWindow window(sf::VideoMode({kWindowW, kWindowH}), "SNAKE", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    std::mt19937 rng(std::random_device{}());

    // 게임 상태
    Screen screen = Screen::Title;
    float titleTimer = 0.f;
    std::deque<sf::Vector2i> snake;
    sf::Vector2i dir{1, 0};
    std::deque<sf::Vector2i> dirQueue;
    std::optional<sf::Vector2i> food;
    int score = 0;
    int eaten = 0;
    float interval = kStartInterval;
    float accumulator = 0.f;

    auto placeFood = [&]()
    {
        std::vector<sf::Vector2i> empty;
        empty.reserve(kCols * kRows);
        for (int y = 0; y < kRows; ++y)
            for (int x = 0; x < kCols; ++x)
            {
                const sf::Vector2i c{x, y};
                if (std::find(snake.begin(), snake.end(), c) == snake.end())
                    empty.push_back(c);
            }
        if (empty.empty())
        {
            food.reset();
            return;
        }
        std::uniform_int_distribution<std::size_t> dist(0, empty.size() - 1);
        food = empty[dist(rng)];
    };

    auto startPlay = [&]()
    {
        snake.clear();
        snake.push_back({16, 8});
        snake.push_back({15, 8});
        snake.push_back({14, 8});
        dir = {1, 0};
        dirQueue.clear();
        score = 0;
        eaten = 0;
        interval = kStartInterval;
        accumulator = 0.f;
        placeFood();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto gameOver = [&]()
    {
        loseSound.play();
        music.stop();
        screen = Screen::Result;
    };

    auto queueDir = [&](sf::Vector2i d)
    {
        const sf::Vector2i last = dirQueue.empty() ? dir : dirQueue.back();
        if (d == last || d == -last)
            return;
        if (dirQueue.size() < 2)
            dirQueue.push_back(d);
    };

    // 한 칸 전진. 게임이 끝나면 false
    auto step = [&]() -> bool
    {
        if (!dirQueue.empty())
        {
            dir = dirQueue.front();
            dirQueue.pop_front();
        }
        const sf::Vector2i newHead = snake.front() + dir;
        if (newHead.x < 0 || newHead.x >= kCols || newHead.y < 0 || newHead.y >= kRows)
        {
            gameOver();
            return false;
        }
        const bool eating = food.has_value() && newHead == *food;
        const std::size_t checkCount = eating ? snake.size() : snake.size() - 1;
        for (std::size_t i = 0; i < checkCount; ++i)
        {
            if (snake[i] == newHead)
            {
                gameOver();
                return false;
            }
        }
        snake.push_front(newHead);
        if (eating)
        {
            score += 10;
            ++eaten;
            pointSound.play();
            if (eaten % 5 == 0)
                interval = std::max(kMinInterval, interval * 0.9f);
            placeFood();
        }
        else
        {
            snake.pop_back();
        }
        return true;
    };

    sf::Text scoreText(font, "SCORE 0", 16);
    scoreText.setFillColor(kTextColor);
    sf::Text bigText(font, "SNAKE", 48);
    bigText.setFillColor(kTextColor);
    sf::Text midText(font, "PRESS ENTER", 32);
    midText.setFillColor(kTextColor);
    sf::Text smallText(font, "R: RESTART  ESC: QUIT", 16);
    smallText.setFillColor(kTextColor);

    sf::RectangleShape bar({static_cast<float>(kWindowW), kBarH});
    bar.setFillColor(kBarColor);
    sf::RectangleShape cellShape({28.f, 28.f});
    sf::CircleShape foodShape(12.f);
    foodShape.setOrigin({12.f, 12.f});
    foodShape.setFillColor(kFoodColor);

    sf::Clock clock;
    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();
        if (dt > 0.25f)
            dt = 0.25f;

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* k = event->getIf<sf::Event::KeyPressed>())
            {
                using K = sf::Keyboard::Key;
                using S = sf::Keyboard::Scan;
                if (isKey(*k, K::Escape, S::Escape))
                {
                    window.close();
                    continue;
                }
                switch (screen)
                {
                    case Screen::Title:
                        if (isKey(*k, K::Enter, S::Enter) || k->scancode == S::NumpadEnter)
                            startPlay();
                        break;
                    case Screen::Play:
                        if (isKey(*k, K::Up, S::Up) || isKey(*k, K::W, S::W))
                            queueDir({0, -1});
                        else if (isKey(*k, K::Down, S::Down) || isKey(*k, K::S, S::S))
                            queueDir({0, 1});
                        else if (isKey(*k, K::Left, S::Left) || isKey(*k, K::A, S::A))
                            queueDir({-1, 0});
                        else if (isKey(*k, K::Right, S::Right) || isKey(*k, K::D, S::D))
                            queueDir({1, 0});
                        break;
                    case Screen::Result:
                        if (isKey(*k, K::R, S::R))
                            startPlay();
                        break;
                }
            }
        }
        if (!window.isOpen())
            break;

        // 갱신
        if (screen == Screen::Title)
        {
            titleTimer += dt;
            if (titleTimer >= kTitleTime)
                startPlay();
        }
        else if (screen == Screen::Play)
        {
            accumulator += dt;
            while (accumulator >= interval)
            {
                accumulator -= interval;
                if (!step())
                    break;
            }
        }

        // 그리기
        window.clear(kBoardBg);
        const sf::Vector2f center{kWindowW / 2.f, kWindowH / 2.f};

        if (screen == Screen::Title)
        {
            bigText.setString("SNAKE");
            centerText(bigText, {center.x, center.y - 40.f});
            window.draw(bigText);
            midText.setString("PRESS ENTER");
            centerText(midText, {center.x, center.y + 30.f});
            window.draw(midText);
        }
        else if (screen == Screen::Play)
        {
            window.draw(bar);
            scoreText.setString("SCORE " + std::to_string(score));
            const sf::FloatRect b = scoreText.getLocalBounds();
            scoreText.setOrigin({b.position.x, b.position.y + b.size.y / 2.f});
            scoreText.setPosition({10.f, kBarH / 2.f});
            window.draw(scoreText);

            if (food)
            {
                foodShape.setPosition({food->x * kCell + kCell / 2.f, kBarH + food->y * kCell + kCell / 2.f});
                window.draw(foodShape);
            }
            for (std::size_t i = snake.size(); i-- > 0;)
            {
                const sf::Vector2i c = snake[i];
                cellShape.setFillColor(i == 0 ? kHeadColor : kBodyColor);
                cellShape.setPosition({c.x * kCell + 1.f, kBarH + c.y * kCell + 1.f});
                window.draw(cellShape);
            }
        }
        else
        {
            bigText.setString("GAME OVER");
            centerText(bigText, {center.x, center.y - 60.f});
            window.draw(bigText);
            midText.setString("SCORE " + std::to_string(score));
            centerText(midText, {center.x, center.y + 5.f});
            window.draw(midText);
            smallText.setString("R: RESTART  ESC: QUIT");
            centerText(smallText, {center.x, center.y + 60.f});
            window.draw(smallText);
        }

        window.display();
    }

    music.stop();
    return 0;
}
