// SNAKE - SFML 3.1 구현 (main.cpp 단일 파일)
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
constexpr unsigned int kWindowW   = 960;
constexpr unsigned int kWindowH   = 540;
constexpr int          kCols      = 32;
constexpr int          kRows      = 17;
constexpr float        kCell      = 30.f;
constexpr float        kBarH      = 30.f;
constexpr float        kBaseStep  = 0.125f;
constexpr float        kMinStep   = 0.06f;
constexpr float        kTitleTime = 2.f;

const sf::Color kBoardColor(0x0E, 0x1A, 0x12);
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

void centerText(sf::Text& text, sf::Vector2f pos)
{
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    text.setPosition(pos);
}

std::optional<sf::Vector2i> directionFromKey(const sf::Event::KeyPressed& k)
{
    using K = sf::Keyboard::Key;
    using S = sf::Keyboard::Scancode;
    if (k.code == K::Up || k.code == K::W || k.scancode == S::Up || k.scancode == S::W)
        return sf::Vector2i{0, -1};
    if (k.code == K::Down || k.code == K::S || k.scancode == S::Down || k.scancode == S::S)
        return sf::Vector2i{0, 1};
    if (k.code == K::Left || k.code == K::A || k.scancode == S::Left || k.scancode == S::A)
        return sf::Vector2i{-1, 0};
    if (k.code == K::Right || k.code == K::D || k.scancode == S::Right || k.scancode == S::D)
        return sf::Vector2i{1, 0};
    return std::nullopt;
}

struct Game
{
    std::deque<sf::Vector2i> snake; // front = head
    sf::Vector2i             dir{1, 0};
    std::vector<sf::Vector2i> queue;
    sf::Vector2i             food{0, 0};
    int                      score    = 0;
    int                      eaten    = 0;
    float                    interval = kBaseStep;
    float                    acc      = 0.f;
};
} // namespace

int main()
{
    const std::string fontPath  = "assets/fonts/game.ttf";
    const std::string pointPath = "assets/sfx/point.wav";
    const std::string losePath  = "assets/sfx/lose.wav";
    const std::string musicPath = "assets/music/bgm_snake.ogg";

    sf::Font font;
    if (!font.openFromFile(fontPath))
    {
        std::cerr << "Failed to load: " << fontPath << '\n';
        return 1;
    }
    sf::SoundBuffer pointBuf;
    if (!pointBuf.loadFromFile(pointPath))
    {
        std::cerr << "Failed to load: " << pointPath << '\n';
        return 1;
    }
    sf::SoundBuffer loseBuf;
    if (!loseBuf.loadFromFile(losePath))
    {
        std::cerr << "Failed to load: " << losePath << '\n';
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile(musicPath))
    {
        std::cerr << "Failed to load: " << musicPath << '\n';
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    sf::Sound pointSound(pointBuf);
    sf::Sound loseSound(loseBuf);

    sf::RenderWindow window(sf::VideoMode({kWindowW, kWindowH}), "SNAKE", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    std::mt19937 rng(std::random_device{}());

    Game   game;
    Screen screen     = Screen::Title;
    float  titleTimer = 0.f;

    auto placeFood = [&]() -> bool
    {
        std::vector<sf::Vector2i> freeCells;
        freeCells.reserve(kCols * kRows);
        for (int y = 0; y < kRows; ++y)
            for (int x = 0; x < kCols; ++x)
            {
                const sf::Vector2i c{x, y};
                if (std::find(game.snake.begin(), game.snake.end(), c) == game.snake.end())
                    freeCells.push_back(c);
            }
        if (freeCells.empty())
            return false;
        std::uniform_int_distribution<std::size_t> dist(0, freeCells.size() - 1);
        game.food = freeCells[dist(rng)];
        return true;
    };

    auto resetGame = [&]()
    {
        game          = Game{};
        game.snake    = {{16, 8}, {15, 8}, {14, 8}};
        game.dir      = {1, 0};
        game.interval = kBaseStep;
        game.acc      = 0.f;
        placeFood();
    };

    auto startPlay = [&]()
    {
        resetGame();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto endGame = [&](bool playLose)
    {
        if (playLose)
            loseSound.play();
        music.stop();
        screen = Screen::Result;
    };

    // 한 칸 전진. 게임이 끝나면 false.
    auto step = [&]() -> bool
    {
        if (!game.queue.empty())
        {
            game.dir = game.queue.front();
            game.queue.erase(game.queue.begin());
        }
        const sf::Vector2i newHead = game.snake.front() + game.dir;
        if (newHead.x < 0 || newHead.x >= kCols || newHead.y < 0 || newHead.y >= kRows)
        {
            endGame(true);
            return false;
        }
        const bool        eating = (newHead == game.food);
        const std::size_t checkN = eating ? game.snake.size() : game.snake.size() - 1;
        for (std::size_t i = 0; i < checkN; ++i)
        {
            if (game.snake[i] == newHead)
            {
                endGame(true);
                return false;
            }
        }
        game.snake.push_front(newHead);
        if (!eating)
        {
            game.snake.pop_back();
        }
        else
        {
            game.score += 10;
            ++game.eaten;
            pointSound.play();
            if (game.eaten % 5 == 0)
                game.interval = std::max(kMinStep, game.interval * 0.9f);
            if (!placeFood())
            {
                endGame(false);
                return false;
            }
        }
        return true;
    };

    resetGame();

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
                break;
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape || key->scancode == sf::Keyboard::Scancode::Escape)
                {
                    window.close();
                    break;
                }
                if (screen == Screen::Title)
                {
                    if (key->code == sf::Keyboard::Key::Enter || key->scancode == sf::Keyboard::Scancode::Enter ||
                        key->scancode == sf::Keyboard::Scancode::NumpadEnter)
                        startPlay();
                }
                else if (screen == Screen::Play)
                {
                    if (const auto d = directionFromKey(*key))
                    {
                        const sf::Vector2i last = game.queue.empty() ? game.dir : game.queue.back();
                        if (*d != last && *d != -last && game.queue.size() < 2)
                            game.queue.push_back(*d);
                    }
                }
                else if (screen == Screen::Result)
                {
                    if (key->code == sf::Keyboard::Key::R || key->scancode == sf::Keyboard::Scancode::R)
                        startPlay();
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
            game.acc += dt;
            while (game.acc >= game.interval)
            {
                game.acc -= game.interval;
                if (!step())
                    break;
            }
        }

        // 그리기
        window.clear(kBoardColor);

        if (screen == Screen::Title)
        {
            sf::Text title(font, "SNAKE", 48);
            title.setFillColor(kTextColor);
            centerText(title, {kWindowW / 2.f, kWindowH / 2.f - 40.f});
            window.draw(title);

            sf::Text press(font, "PRESS ENTER", 16);
            press.setFillColor(kTextColor);
            centerText(press, {kWindowW / 2.f, kWindowH / 2.f + 30.f});
            window.draw(press);
        }
        else if (screen == Screen::Play)
        {
            sf::RectangleShape bar({static_cast<float>(kWindowW), kBarH});
            bar.setFillColor(kBarColor);
            window.draw(bar);

            sf::Text scoreText(font, "SCORE " + std::to_string(game.score), 16);
            scoreText.setFillColor(kTextColor);
            const sf::FloatRect sb = scoreText.getLocalBounds();
            scoreText.setOrigin({sb.position.x, sb.position.y + sb.size.y / 2.f});
            scoreText.setPosition({10.f, kBarH / 2.f});
            window.draw(scoreText);

            sf::CircleShape foodShape(12.f);
            foodShape.setFillColor(kFoodColor);
            foodShape.setOrigin({12.f, 12.f});
            foodShape.setPosition({game.food.x * kCell + kCell / 2.f, kBarH + game.food.y * kCell + kCell / 2.f});
            window.draw(foodShape);

            sf::RectangleShape cell({28.f, 28.f});
            for (std::size_t i = game.snake.size(); i-- > 0;)
            {
                const sf::Vector2i c = game.snake[i];
                cell.setFillColor(i == 0 ? kHeadColor : kBodyColor);
                cell.setPosition({c.x * kCell + 1.f, kBarH + c.y * kCell + 1.f});
                window.draw(cell);
            }
        }
        else
        {
            sf::Text over(font, "GAME OVER", 48);
            over.setFillColor(kTextColor);
            centerText(over, {kWindowW / 2.f, kWindowH / 2.f - 60.f});
            window.draw(over);

            sf::Text sc(font, "SCORE " + std::to_string(game.score), 32);
            sc.setFillColor(kTextColor);
            centerText(sc, {kWindowW / 2.f, kWindowH / 2.f + 10.f});
            window.draw(sc);

            sf::Text hint(font, "R: RESTART  ESC: QUIT", 16);
            hint.setFillColor(kTextColor);
            centerText(hint, {kWindowW / 2.f, kWindowH / 2.f + 70.f});
            window.draw(hint);
        }

        window.display();
    }

    music.stop();
    return 0;
}
