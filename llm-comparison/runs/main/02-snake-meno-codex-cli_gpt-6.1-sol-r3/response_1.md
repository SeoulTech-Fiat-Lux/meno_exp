```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int Columns = 32;
constexpr int Rows = 17;
constexpr float CellSize = 30.f;

struct Cell {
    int x;
    int y;

    bool operator==(const Cell&) const = default;
};

enum class Direction { Up, Down, Left, Right };
enum class Screen { Title, Play, Result };

Cell displacement(Direction direction) {
    switch (direction) {
    case Direction::Up:    return {0, -1};
    case Direction::Down:  return {0, 1};
    case Direction::Left:  return {-1, 0};
    case Direction::Right: return {1, 0};
    }
    return {0, 0};
}

bool opposite(Direction a, Direction b) {
    const Cell first = displacement(a);
    const Cell second = displacement(b);
    return first.x == -second.x && first.y == -second.y;
}

} // namespace

int main() {
    meno::Game game{{
        .application = {.framerateLimit = 60},
        .windowSize = {960, 540},
        .title = "SNAKE",
        .clearColor = meno::Color::fromRgb(0x0E1A12)
    }};

    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    const auto pointBuffer =
        game.resources().loadSoundBuffer("assets/sfx/point.wav");
    const auto loseBuffer =
        game.resources().loadSoundBuffer("assets/sfx/lose.wav");
    auto music = meno::Music::openFromFile("assets/music/bgm_snake.ogg");

    bool missing = false;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            missing = true;
        }
    };

    require(static_cast<bool>(font), "assets/fonts/game.ttf");
    require(static_cast<bool>(pointBuffer), "assets/sfx/point.wav");
    require(static_cast<bool>(loseBuffer), "assets/sfx/lose.wav");
    require(music.has_value(), "assets/music/bgm_snake.ogg");

    if (missing) {
        return 1;
    }

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};
    music->setVolume(0.4f);
    music->setLooping(true);

    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color headColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);
    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);

    const meno::Camera2D camera{
        .center = {480.f, 270.f},
        .size = {960.f, 540.f}
    };
    game.renderer().setCamera(camera);

    std::mt19937 random{std::random_device{}()};
    Screen screen = Screen::Title;
    std::deque<Cell> snake;
    std::deque<Direction> turns;
    Direction direction = Direction::Right;
    Cell food{-1, -1};
    int score = 0;
    int eaten = 0;
    double titleTime = 0.0;
    double movementTime = 0.0;
    double movementInterval = 0.125;

    const auto placeFood = [&]() -> bool {
        std::vector<Cell> available;
        available.reserve(Columns * Rows);
        for (int y = 0; y < Rows; ++y) {
            for (int x = 0; x < Columns; ++x) {
                const Cell cell{x, y};
                if (std::find(snake.begin(), snake.end(), cell) == snake.end()) {
                    available.push_back(cell);
                }
            }
        }
        if (available.empty()) {
            food = {-1, -1};
            return false;
        }

        std::uniform_int_distribution<std::size_t> choose{
            0, available.size() - 1
        };
        food = available[choose(random)];
        return true;
    };

    const auto startPlay = [&] {
        music->stop();
        pointSound.stop();
        loseSound.stop();

        snake = {{16, 8}, {15, 8}, {14, 8}};
        turns.clear();
        direction = Direction::Right;
        score = 0;
        eaten = 0;
        titleTime = 0.0;
        movementTime = 0.0;
        movementInterval = 0.125;
        (void)placeFood();
        screen = Screen::Play;
        music->play();
    };

    const auto finish = [&](bool collision) {
        screen = Screen::Result;
        movementTime = 0.0;
        turns.clear();
        music->stop();
        if (collision) {
            loseSound.play();
        }
    };

    const auto queueTurn = [&](Direction requested) {
        if (turns.size() >= 2) {
            return;
        }
        const Direction previous = turns.empty() ? direction : turns.back();
        if (requested != previous && !opposite(requested, previous)) {
            turns.push_back(requested);
        }
    };

    meno::Scene& scene = game.scenes().createScene();

    scene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();

        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (screen == Screen::Title) {
            titleTime += dt;
            if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
                startPlay();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (input.wasPressed(meno::Key::R)) {
                startPlay();
            }
            return;
        }

        if (input.wasPressed(meno::Key::Up) ||
            input.wasPressed(meno::Key::W)) {
            queueTurn(Direction::Up);
        }
        if (input.wasPressed(meno::Key::Down) ||
            input.wasPressed(meno::Key::S)) {
            queueTurn(Direction::Down);
        }
        if (input.wasPressed(meno::Key::Left) ||
            input.wasPressed(meno::Key::A)) {
            queueTurn(Direction::Left);
        }
        if (input.wasPressed(meno::Key::Right) ||
            input.wasPressed(meno::Key::D)) {
            queueTurn(Direction::Right);
        }

        movementTime += dt;
        while (screen == Screen::Play &&
               movementTime >= movementInterval) {
            movementTime -= movementInterval;

            if (!turns.empty()) {
                direction = turns.front();
                turns.pop_front();
            }

            const Cell step = displacement(direction);
            const Cell next{
                snake.front().x + step.x,
                snake.front().y + step.y
            };
            const bool grows = next == food;

            if (next.x < 0 || next.x >= Columns ||
                next.y < 0 || next.y >= Rows) {
                finish(true);
                break;
            }

            const std::size_t occupied =
                snake.size() - (grows ? 0u : 1u);
            bool hitsBody = false;
            for (std::size_t i = 0; i < occupied; ++i) {
                if (snake[i] == next) {
                    hitsBody = true;
                    break;
                }
            }
            if (hitsBody) {
                finish(true);
                break;
            }

            snake.push_front(next);
            if (!grows) {
                snake.pop_back();
                continue;
            }

            score += 10;
            ++eaten;
            pointSound.play();

            if (eaten % 5 == 0) {
                movementInterval =
                    std::max(0.06, movementInterval * 0.9);
            }

            if (!placeFood()) {
                finish(false);
            }
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect(
            {{0.f, 0.f}, {960.f, 540.f}},
            {.fill = boardColor}
        );

        const auto centered = [&](const std::string& text, float y,
                                  unsigned int size) {
            meno::TextParams params{
                .characterSize = size,
                .color = textColor
            };
            params.origin =
                renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {480.f, y}, params);
        };

        if (screen == Screen::Title) {
            centered("SNAKE", 225.f, 48);
            centered("PRESS ENTER", 305.f, 32);
            return;
        }

        if (screen == Screen::Result) {
            centered("GAME OVER", 190.f, 48);
            centered("SCORE " + std::to_string(score), 270.f, 32);
            centered("R: RESTART  ESC: QUIT", 345.f, 32);
            return;
        }

        renderer.drawRect(
            {{0.f, 0.f}, {960.f, 30.f}},
            {.fill = barColor}
        );

        meno::TextParams scoreParams{
            .characterSize = 16,
            .color = textColor
        };
        const std::string scoreText = "SCORE " + std::to_string(score);
        const auto bounds =
            renderer.measureTextBounds(*font, scoreText, scoreParams);
        scoreParams.origin = bounds.center();
        renderer.drawText(
            *font, scoreText, {12.f + bounds.size.x * 0.5f, 15.f},
            scoreParams
        );

        if (food.x >= 0) {
            renderer.drawCircle(
                {static_cast<float>(food.x) * CellSize + 15.f,
                 30.f + static_cast<float>(food.y) * CellSize + 15.f},
                12.f,
                {.fill = foodColor}
            );
        }

        for (std::size_t i = snake.size(); i-- > 0;) {
            const Cell cell = snake[i];
            renderer.drawRect(
                {{static_cast<float>(cell.x) * CellSize + 1.f,
                  30.f + static_cast<float>(cell.y) * CellSize + 1.f},
                 {28.f, 28.f}},
                {.fill = i == 0 ? headColor : bodyColor}
            );
        }
    });

    scene.setExitHandler([&] {
        music->stop();
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}
```