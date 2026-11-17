```cpp
// SNAKE - SFML 3.1 / C++20 단일 파일 구현
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <cstdlib>
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
constexpr int          kCols    = 32;
constexpr int          kRows    = 17;
constexpr float        kCell    = 30.f;
constexpr float        kBarH    = 30.f;

constexpr float kStartInterval = 0.125f;
constexpr float kMinInterval   = 0.06f;
constexpr float kTitleTime     = 2.f;
constexpr float kMaxDt         = 0.25f;

const sf::Color kBoardBg(0x0E, 0x1A, 0x12);
const sf::Color kBarBg(0x1F, 0x3A, 0x26);
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

void centerText(sf::Text& text, sf::Vector2f pos)
{
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    text.setPosition(pos);
}
} // namespace

int main()
{
    // ---- 에셋 로드 ----
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
    {
        std::cerr << "assets/fonts/game.ttf\n";
        return 1;
    }
    sf::SoundBuffer pointBuf;
    if (!pointBuf.loadFromFile("assets/sfx/point.wav"))
    {
        std::cerr << "assets/sfx/point.wav\n";
        return 1;
    }
    sf::SoundBuffer loseBuf;
    if (!loseBuf.loadFromFile("assets/sfx/lose.wav"))
    {
        std::cerr << "assets/sfx/lose.wav\n";
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_snake.ogg"))
    {
        std::cerr << "assets/music/bgm_snake.ogg\n";
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    sf::Sound pointSound(pointBuf);
    sf::Sound loseSound(loseBuf);

    sf::RenderWindow window(sf::VideoMode({kWindowW, kWindowH}), "SNAKE", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    std::mt19937 rng(std::random_device{}());

    // ---- 게임 상태 ----
    Screen                  screen     = Screen::Title;
    float                   titleTimer = 0.f;
    std::deque<sf::Vector2i> snake;
    sf::Vector2i            dir{1, 0};
    std::deque<sf::Vector2i> dirQueue;
    sf::Vector2i            food{0, 0};
    int                     score    = 0;
    int                     eaten    = 0;
    float                   interval = kStartInterval;
    float                   acc      = 0.f;

    auto placeFood = [&]() -> bool
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
            return false;
        std::uniform_int_distribution<std::size_t> dist(0, empty.size() - 1);
        food = empty[dist(rng)];
        return true;
    };

    auto startPlay = [&]()
    {
        snake.clear();
        snake.push_back({16, 8});
        snake.push_back({15, 8});
        snake.push_back({14, 8});
        dir = {1, 0};
        dirQueue.clear();
        score    = 0;
        eaten    = 0;
        interval = kStartInterval;
        acc      = 0.f;
        placeFood();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto gameOver = [&]()
    {
        screen = Screen::Result;
        music.stop();
        loseSound.play();
        dirQueue.clear();
    };

    auto pushDir = [&](sf::Vector2i d)
    {
        const sf::Vector2i last = dirQueue.empty() ? dir : dirQueue.back();
        if (d == last || (d.x == -last.x && d.y == -last.y))
            return;
        if (dirQueue.size() < 2)
            dirQueue.push_back(d);
    };

    auto step = [&]()
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
            return;
        }
        const bool eating = (newHead == food);
        const auto endIt  = eating ? snake.end() : std::prev(snake.end());
        if (std::find(snake.begin(), endIt, newHead) != endIt)
        {
            gameOver();
            return;
        }
        snake.push_front(newHead);
        if (eating)
        {
            score += 10;
            ++eaten;
            pointSound.play();
            if (eaten % 5 == 0)
                interval = std::max(kMinInterval, interval * 0.9f);
            if (!placeFood())
            {
                gameOver();
                return;
            }
        }
        else
        {
            snake.pop_back();
        }
    };

    // ---- 텍스트 ----
    sf::Text titleText(font, "SNAKE", 48);
    titleText.setFillColor(kTextColor);
    centerText(titleText, {kWindowW / 2.f, kWindowH / 2.f - 40.f});

    sf::Text pressText(font, "PRESS ENTER", 32);
    pressText.setFillColor(kTextColor);
    centerText(pressText, {kWindowW / 2.f, kWindowH / 2.f + 30.f});

    sf::Text scoreText(font, "SCORE 0", 16);
    scoreText.setFillColor(kTextColor);

    sf::Text overText(font, "GAME OVER", 48);
    overText.setFillColor(kTextColor);
    centerText(overText, {kWindowW / 2.f, kWindowH / 2.f - 70.f});

    sf::Text finalScore(font, "SCORE 0", 32);
    finalScore.setFillColor(kTextColor);

    sf::Text restartText(font, "R: RESTART  ESC: QUIT", 16);
    restartText.setFillColor(kTextColor);
    centerText(restartText, {kWindowW / 2.f, kWindowH / 2.f + 60.f});

    sf::RectangleShape barRect({static_cast<float>(kWindowW), kBarH});
    barRect.setFillColor(kBarBg);
    sf::RectangleShape cellRect({28.f, 28.f});
    sf::CircleShape    foodCircle(12.f);
    foodCircle.setFillColor(kFoodColor);

    sf::Clock clock;

    while (window.isOpen())
    {
        // ---- 이벤트 ----
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                using Sc = sf::Keyboard::Scancode;
                const Sc sc = key->scancode;
                if (sc == Sc::Escape)
                {
                    window.close();
                    break;
                }
                switch (screen)
                {
                    case Screen::Title:
                        if (sc == Sc::Enter || sc == Sc::NumpadEnter)
                            startPlay();
                        break;
                    case Screen::Play:
                        if (sc == Sc::Up || sc == Sc::W)
                            pushDir({0, -1});
                        else if (sc == Sc::Down || sc == Sc::S)
                            pushDir({0, 1});
                        else if (sc == Sc::Left || sc == Sc::A)
                            pushDir({-1, 0});
                        else if (sc == Sc::Right || sc == Sc::D)
                            pushDir({1, 0});
                        break;
                    case Screen::Result:
                        if (sc == Sc::R)
                            startPlay();
                        break;
                }
            }
        }
        if (!window.isOpen())
            break;

        // ---- 갱신 ----
        const float dt = std::min(clock.restart().asSeconds(), kMaxDt);
        if (screen == Screen::Title)
        {
            titleTimer += dt;
            if (titleTimer >= kTitleTime)
                startPlay();
        }
        else if (screen == Screen::Play)
        {
            acc += dt;
            while (screen == Screen::Play && acc >= interval)
            {
                acc -= interval;
                step();
            }
        }

        // ---- 그리기 ----
        window.clear(kBoardBg);
        if (screen == Screen::Title)
        {
            window.draw(titleText);
            window.draw(pressText);
        }
        else if (screen == Screen::Play)
        {
            window.draw(barRect);
            scoreText.setString("SCORE " + std::to_string(score));
            scoreText.setPosition({8.f, 6.f});
            window.draw(scoreText);

            foodCircle.setPosition({food.x * kCell + 15.f - 12.f, kBarH + food.y * kCell + 15.f - 12.f});
            window.draw(foodCircle);

            for (std::size_t i = snake.size(); i-- > 0;)
            {
                const sf::Vector2i c = snake[i];
                cellRect.setFillColor(i == 0 ? kHeadColor : kBodyColor);
                cellRect.setPosition({c.x * kCell + 1.f, kBarH + c.y * kCell + 1.f});
                window.draw(cellRect);
            }
        }
        else
        {
            window.draw(overText);
            finalScore.setString("SCORE " + std::to_string(score));
            centerText(finalScore, {kWindowW / 2.f, kWindowH / 2.f});
            window.draw(finalScore);
            window.draw(restartText);
        }
        window.display();
    }

    music.stop();
    return 0;
}
```