#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWinW = 960.f;
constexpr float kWinH = 540.f;
constexpr float kBarH = 30.f;
constexpr float kCell = 30.f;
constexpr float kFoodRadius = 12.f;
constexpr int   kCols = 32;
constexpr int   kRows = 17;
constexpr float kStepStart = 0.125f;
constexpr float kStepMin   = 0.06f;
constexpr float kStepScale = 0.9f;
constexpr int   kEatPerSpeedUp = 5;
constexpr float kTitleTime = 2.f;

const sf::Color kBoardColor{14, 26, 18};
const sf::Color kBarColor{31, 58, 38};
const sf::Color kBodyColor{95, 203, 95};
const sf::Color kHeadColor{182, 242, 160};
const sf::Color kFoodColor{229, 83, 61};
const sf::Color kTextColor{232, 245, 233};

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

Cell negate(Cell c) {
    return {-c.x, -c.y};
}

enum class Screen { Title, Play, Result };
enum class StepResult { Moved, Ate, Died };

struct Game {
    Screen screen = Screen::Title;
    std::deque<Cell> snake;    // front = head
    Cell dir{1, 0};
    std::deque<Cell> pending;  // buffered directions, max 2
    Cell food{};
    int score = 0;
    int eaten = 0;
    float interval = kStepStart;
    float moveAcc = 0.f;
    float titleAcc = 0.f;
    std::mt19937 rng{std::random_device{}()};

    void reset() {
        snake = {{16, 8}, {15, 8}, {14, 8}};
        dir = {1, 0};
        pending.clear();
        score = 0;
        eaten = 0;
        interval = kStepStart;
        moveAcc = 0.f;
        spawnFood();
    }

    bool occupied(Cell c) const {
        return std::find(snake.begin(), snake.end(), c) != snake.end();
    }

    void spawnFood() {
        std::vector<Cell> freeCells;
        for (int y = 0; y < kRows; ++y)
            for (int x = 0; x < kCols; ++x)
                if (!occupied({x, y}))
                    freeCells.push_back({x, y});
        if (freeCells.empty())
            return;
        std::uniform_int_distribution<std::size_t> dist(0, freeCells.size() - 1);
        food = freeCells[dist(rng)];
    }

    void queueDir(Cell d) {
        const Cell last = pending.empty() ? dir : pending.back();
        if (d == last || d == negate(last))
            return;
        if (pending.size() >= 2)
            return;
        pending.push_back(d);
    }
};

StepResult step(Game& g) {
    if (!g.pending.empty()) {
        g.dir = g.pending.front();
        g.pending.pop_front();
    }

    const Cell head = g.snake.front();
    const Cell next{head.x + g.dir.x, head.y + g.dir.y};

    if (next.x < 0 || next.x >= kCols || next.y < 0 || next.y >= kRows)
        return StepResult::Died;

    const bool eating = (next == g.food);

    // When not eating, the tail cell vacates this move and is not a collision.
    const std::size_t checkCount = eating ? g.snake.size() : g.snake.size() - 1;
    for (std::size_t i = 0; i < checkCount; ++i)
        if (g.snake[i] == next)
            return StepResult::Died;

    g.snake.push_front(next);

    if (eating) {
        g.score += 10;
        ++g.eaten;
        if (g.eaten % kEatPerSpeedUp == 0)
            g.interval = std::max(kStepMin, g.interval * kStepScale);
        g.spawnFood();
        return StepResult::Ate;
    }

    g.snake.pop_back();
    return StepResult::Moved;
}

void startPlay(Game& g, sf::Music& bgm) {
    g.screen = Screen::Play;
    g.moveAcc = 0.f;
    bgm.play();
}

void drawCentered(sf::RenderWindow& window, sf::Text& text, float y) {
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    text.setPosition({kWinW / 2.f, y});
    window.draw(text);
}

sf::String scoreLabel(int score) {
    return sf::String(std::string("SCORE ") + std::to_string(score));
}

int fail(const char* path) {
    std::cerr << "failed to load: " << path << '\n';
    return 1;
}

} // namespace

int main() {
    // Assets (declared in dependency order: buffers before sounds, font before texts)
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
        return fail("assets/fonts/game.ttf");

    sf::SoundBuffer pointBuf;
    if (!pointBuf.loadFromFile("assets/sfx/point.wav"))
        return fail("assets/sfx/point.wav");

    sf::SoundBuffer loseBuf;
    if (!loseBuf.loadFromFile("assets/sfx/lose.wav"))
        return fail("assets/sfx/lose.wav");

    sf::Music bgm;
    if (!bgm.openFromFile("assets/music/bgm_snake.ogg"))
        return fail("assets/music/bgm_snake.ogg");
    bgm.setVolume(40.f);
    bgm.setLooping(true);

    sf::Sound pointSnd(pointBuf);
    sf::Sound loseSnd(loseBuf);

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SNAKE");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Text titleText(font, "SNAKE", 48);
    titleText.setFillColor(kTextColor);
    sf::Text promptText(font, "PRESS ENTER", 32);
    promptText.setFillColor(kTextColor);
    sf::Text overText(font, "GAME OVER", 48);
    overText.setFillColor(kTextColor);
    sf::Text scoreText(font, "", 32);
    scoreText.setFillColor(kTextColor);
    sf::Text hintText(font, "R: RESTART  ESC: QUIT", 16);
    hintText.setFillColor(kTextColor);
    sf::Text hud(font, "", 16);
    hud.setFillColor(kTextColor);

    Game game;
    game.reset();

    sf::Clock clock;

    while (window.isOpen()) {
        // Input: handle every event that arrived since the last frame
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }

            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (!key)
                continue;

            if (key->scancode == sf::Keyboard::Scancode::Escape) {
                window.close();
                break;
            }

            switch (game.screen) {
            case Screen::Title:
                if (key->code == sf::Keyboard::Key::Enter)
                    startPlay(game, bgm);
                break;

            case Screen::Play:
                if (key->scancode == sf::Keyboard::Scancode::Up ||
                    key->scancode == sf::Keyboard::Scancode::W)
                    game.queueDir({0, -1});
                else if (key->scancode == sf::Keyboard::Scancode::Down ||
                         key->scancode == sf::Keyboard::Scancode::S)
                    game.queueDir({0, 1});
                else if (key->scancode == sf::Keyboard::Scancode::Left ||
                         key->scancode == sf::Keyboard::Scancode::A)
                    game.queueDir({-1, 0});
                else if (key->scancode == sf::Keyboard::Scancode::Right ||
                         key->scancode == sf::Keyboard::Scancode::D)
                    game.queueDir({1, 0});
                break;

            case Screen::Result:
                if (key->code == sf::Keyboard::Key::R) {
                    game.reset();
                    startPlay(game, bgm);
                }
                break;
            }
        }

        if (!window.isOpen())
            break;

        const float dt = clock.restart().asSeconds();

        // Update
        if (game.screen == Screen::Title) {
            game.titleAcc += dt;
            if (game.titleAcc >= kTitleTime)
                startPlay(game, bgm);
        } else if (game.screen == Screen::Play) {
            game.moveAcc += dt;
            while (game.screen == Screen::Play && game.moveAcc >= game.interval) {
                game.moveAcc -= game.interval;
                switch (step(game)) {
                case StepResult::Moved:
                    break;
                case StepResult::Ate:
                    pointSnd.play();
                    break;
                case StepResult::Died:
                    game.screen = Screen::Result;
                    bgm.stop();
                    loseSnd.play();
                    break;
                }
            }
        }

        // Draw
        window.clear(kBoardColor);

        if (game.screen == Screen::Title) {
            drawCentered(window, titleText, 200.f);
            drawCentered(window, promptText, 320.f);
        } else if (game.screen == Screen::Play) {
            sf::RectangleShape bar(sf::Vector2f{kWinW, kBarH});
            bar.setFillColor(kBarColor);
            window.draw(bar);

            hud.setString(scoreLabel(game.score));
            hud.setPosition({16.f, 6.f});
            window.draw(hud);

            sf::CircleShape food(kFoodRadius);
            food.setFillColor(kFoodColor);
            food.setPosition({game.food.x * kCell + (kCell / 2.f - kFoodRadius),
                              kBarH + game.food.y * kCell + (kCell / 2.f - kFoodRadius)});
            window.draw(food);

            sf::RectangleShape seg(sf::Vector2f{kCell - 2.f, kCell - 2.f});
            for (std::size_t i = 0; i < game.snake.size(); ++i) {
                seg.setFillColor(i == 0 ? kHeadColor : kBodyColor);
                seg.setPosition({game.snake[i].x * kCell + 1.f,
                                 kBarH + game.snake[i].y * kCell + 1.f});
                window.draw(seg);
            }
        } else {
            overText.setFillColor(kTextColor);
            drawCentered(window, overText, 170.f);
            scoreText.setString(scoreLabel(game.score));
            drawCentered(window, scoreText, 260.f);
            drawCentered(window, hintText, 340.f);
        }

        window.display();
    }

    return 0;
}
