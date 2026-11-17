#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <deque>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

struct Cell {
    int x;
    int y;

    bool operator==(const Cell&) const = default;
};

enum class Screen { Title, Play, Result };
enum class Direction { Up, Down, Left, Right };

bool opposite(Direction a, Direction b) {
    return (a == Direction::Up && b == Direction::Down) ||
           (a == Direction::Down && b == Direction::Up) ||
           (a == Direction::Left && b == Direction::Right) ||
           (a == Direction::Right && b == Direction::Left);
}

Cell advance(Cell cell, Direction direction) {
    switch (direction) {
    case Direction::Up:    --cell.y; break;
    case Direction::Down:  ++cell.y; break;
    case Direction::Left:  --cell.x; break;
    case Direction::Right: ++cell.x; break;
    }
    return cell;
}

int runGame() {
    constexpr int columns = 32;
    constexpr int rows = 17;
    constexpr float cellSize = 30.f;
    constexpr float boardTop = 30.f;

    const auto background = meno::Color::fromRgb(0x0E1A12);
    const auto scoreBar = meno::Color::fromRgb(0x1F3A26);
    const auto bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const auto headColor = meno::Color::fromRgb(0xB6F2A0);
    const auto foodColor = meno::Color::fromRgb(0xE5533D);
    const auto textColor = meno::Color::fromRgb(0xE8F5E9);

    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SNAKE",
                     .clearColor = background}};

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
    require(static_cast<bool>(music), "assets/music/bgm_snake.ogg");
    if (missing) {
        return 1;
    }

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};
    music->setLooping(true);
    music->setVolume(0.4f);

    game.renderer().setCamera(
        meno::Camera2D{.center = {480.f, 270.f}, .size = {960.f, 540.f}});

    std::mt19937 random{std::random_device{}()};
    Screen screen = Screen::Title;
    std::deque<Cell> snake;
    std::deque<Direction> pending;
    Direction direction = Direction::Right;
    Cell food{0, 0};
    bool hasFood = false;
    int score = 0;
    int eaten = 0;
    double titleTime = 0.0;
    double movementTime = 0.0;
    double interval = 0.125;

    const auto placeFood = [&] {
        std::vector<Cell> freeCells;
        freeCells.reserve(columns * rows);
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                const Cell cell{x, y};
                if (std::find(snake.begin(), snake.end(), cell) == snake.end()) {
                    freeCells.push_back(cell);
                }
            }
        }

        hasFood = !freeCells.empty();
        if (hasFood) {
            std::uniform_int_distribution<std::size_t> choose(
                0, freeCells.size() - 1);
            food = freeCells[choose(random)];
        }
    };

    const auto startPlay = [&] {
        snake = {{16, 8}, {15, 8}, {14, 8}};
        pending.clear();
        direction = Direction::Right;
        score = 0;
        eaten = 0;
        titleTime = 0.0;
        movementTime = 0.0;
        interval = 0.125;
        pointSound.stop();
        loseSound.stop();
        placeFood();
        music->stop();
        music->play();
        screen = Screen::Play;
    };

    const auto finishPlay = [&] {
        music->stop();
        loseSound.play();
        pending.clear();
        movementTime = 0.0;
        screen = Screen::Result;
    };

    const auto enqueue = [&](Direction requested) {
        if (pending.size() >= 2) {
            return;
        }
        const Direction previous = pending.empty() ? direction : pending.back();
        if (requested != previous && !opposite(requested, previous)) {
            pending.push_back(requested);
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
            if (input.wasPressed(meno::Key::Enter) ||
                titleTime + 1e-10 >= 2.0) {
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

        const std::array bindings{
            std::pair{meno::Key::Up, Direction::Up},
            std::pair{meno::Key::W, Direction::Up},
            std::pair{meno::Key::Down, Direction::Down},
            std::pair{meno::Key::S, Direction::Down},
            std::pair{meno::Key::Left, Direction::Left},
            std::pair{meno::Key::A, Direction::Left},
            std::pair{meno::Key::Right, Direction::Right},
            std::pair{meno::Key::D, Direction::Right}
        };
        for (const auto& [key, requested] : bindings) {
            if (input.wasPressed(key)) {
                enqueue(requested);
            }
        }

        movementTime += dt;
        while (screen == Screen::Play && movementTime + 1e-10 >= interval) {
            movementTime = std::max(0.0, movementTime - interval);

            if (!pending.empty()) {
                direction = pending.front();
                pending.pop_front();
            }

            const Cell next = advance(snake.front(), direction);
            if (next.x < 0 || next.x >= columns ||
                next.y < 0 || next.y >= rows) {
                finishPlay();
                break;
            }

            const bool eating = hasFood && next == food;
            const std::size_t checkedLength =
                snake.size() - (eating ? 0u : 1u);
            bool collision = false;
            for (std::size_t i = 0; i < checkedLength; ++i) {
                if (snake[i] == next) {
                    collision = true;
                    break;
                }
            }
            if (collision) {
                finishPlay();
                break;
            }

            snake.push_front(next);
            if (eating) {
                score += 10;
                ++eaten;
                pointSound.play();
                if (eaten % 5 == 0) {
                    interval = std::max(0.06, interval * 0.9);
                }
                placeFood();
            } else {
                snake.pop_back();
            }
        }
    });

    const auto centeredText = [&](meno::Renderer& renderer,
                                  const std::string& text,
                                  float y,
                                  unsigned int size) {
        meno::TextParams params{.characterSize = size, .color = textColor};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {480.f, y}, params);
    };

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                          {.fill = background});

        if (screen == Screen::Title) {
            centeredText(renderer, "SNAKE", 220.f, 48);
            centeredText(renderer, "PRESS ENTER", 310.f, 32);
            return;
        }

        if (screen == Screen::Result) {
            centeredText(renderer, "GAME OVER", 180.f, 48);
            centeredText(renderer, "SCORE " + std::to_string(score), 260.f, 32);
            centeredText(renderer, "R: RESTART  ESC: QUIT", 340.f, 32);
            return;
        }

        renderer.drawRect({{0.f, 0.f}, {960.f, boardTop}},
                          {.fill = scoreBar});
        renderer.drawText(*font, "SCORE " + std::to_string(score),
                          {8.f, 4.f},
                          {.characterSize = 16, .color = textColor});

        if (hasFood) {
            renderer.drawCircle(
                {static_cast<float>(food.x) * cellSize + cellSize / 2.f,
                 boardTop + static_cast<float>(food.y) * cellSize +
                     cellSize / 2.f},
                12.f, {.fill = foodColor});
        }

        for (std::size_t i = snake.size(); i-- > 0;) {
            const Cell cell = snake[i];
            renderer.drawRect(
                {{static_cast<float>(cell.x) * cellSize + 1.f,
                  boardTop + static_cast<float>(cell.y) * cellSize + 1.f},
                 {28.f, 28.f}},
                {.fill = i == 0 ? headColor : bodyColor});
        }
    });

    scene.setExitHandler([&] { music->stop(); });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}

} // namespace

int main() {
    try {
        return runGame();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
