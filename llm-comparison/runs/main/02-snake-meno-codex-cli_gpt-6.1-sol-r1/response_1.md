```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <deque>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int Columns = 32;
constexpr int Rows = 17;
constexpr float CellSize = 30.f;
constexpr float ScoreBarHeight = 30.f;

struct Cell {
    int x;
    int y;

    bool operator==(const Cell&) const = default;
};

enum class Direction { Up, Down, Left, Right };
enum class Screen { Title, Play, Result };

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
    const auto boardColor = meno::Color::fromRgb(0x0E1A12);
    const auto barColor = meno::Color::fromRgb(0x1F3A26);
    const auto bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const auto headColor = meno::Color::fromRgb(0xB6F2A0);
    const auto foodColor = meno::Color::fromRgb(0xE5533D);
    const auto textColor = meno::Color::fromRgb(0xE8F5E9);

    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SNAKE",
                     .clearColor = boardColor}};

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

    const meno::Camera2D camera{
        .center = {480.f, 270.f},
        .size = {960.f, 540.f}
    };
    game.renderer().setCamera(camera);

    std::mt19937 random{std::random_device{}()};
    Screen screen = Screen::Title;
    std::deque<Cell> snake;
    std::deque<Direction> pendingDirections;
    Direction direction = Direction::Right;
    Cell food{-1, -1};
    int score = 0;
    int foodsEaten = 0;
    double titleElapsed = 0.0;
    double moveElapsed = 0.0;
    double moveInterval = 0.125;

    const auto placeFood = [&]() -> bool {
        std::vector<Cell> freeCells;
        freeCells.reserve(Columns * Rows);

        for (int y = 0; y < Rows; ++y) {
            for (int x = 0; x < Columns; ++x) {
                const Cell candidate{x, y};
                if (std::find(snake.begin(), snake.end(), candidate) == snake.end()) {
                    freeCells.push_back(candidate);
                }
            }
        }

        if (freeCells.empty()) {
            food = {-1, -1};
            return false;
        }

        std::uniform_int_distribution<std::size_t> distribution(
            0, freeCells.size() - 1);
        food = freeCells[distribution(random)];
        return true;
    };

    const auto startPlay = [&]() {
        music->stop();
        pointSound.stop();
        loseSound.stop();

        snake = {{16, 8}, {15, 8}, {14, 8}};
        pendingDirections.clear();
        direction = Direction::Right;
        score = 0;
        foodsEaten = 0;
        titleElapsed = 0.0;
        moveElapsed = 0.0;
        moveInterval = 0.125;
        (void)placeFood();

        screen = Screen::Play;
        music->play();
    };

    const auto finishPlay = [&](bool collision) {
        screen = Screen::Result;
        music->stop();
        pendingDirections.clear();
        moveElapsed = 0.0;
        if (collision) {
            loseSound.play();
        }
    };

    const auto queueDirection = [&](Direction requested) {
        if (pendingDirections.size() >= 2) {
            return;
        }

        const Direction previous =
            pendingDirections.empty() ? direction : pendingDirections.back();

        if (requested != previous && !opposite(requested, previous)) {
            pendingDirections.push_back(requested);
        }
    };

    const auto moveOnce = [&]() {
        if (!pendingDirections.empty()) {
            direction = pendingDirections.front();
            pendingDirections.pop_front();
        }

        const Cell next = advance(snake.front(), direction);
        if (next.x < 0 || next.x >= Columns ||
            next.y < 0 || next.y >= Rows) {
            finishPlay(true);
            return;
        }

        const bool eating = next == food;
        const std::size_t collisionCount =
            snake.size() - (eating ? 0u : 1u);

        for (std::size_t i = 0; i < collisionCount; ++i) {
            if (snake[i] == next) {
                finishPlay(true);
                return;
            }
        }

        snake.push_front(next);
        if (!eating) {
            snake.pop_back();
            return;
        }

        score += 10;
        ++foodsEaten;
        pointSound.play();

        if (foodsEaten % 5 == 0) {
            moveInterval = std::max(0.06, moveInterval * 0.9);
        }

        // A completely filled board has no remaining food position.
        if (!placeFood()) {
            finishPlay(false);
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
            titleElapsed += dt;
            if (input.wasPressed(meno::Key::Enter) ||
                titleElapsed + 1e-9 >= 2.0) {
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
            queueDirection(Direction::Up);
        }
        if (input.wasPressed(meno::Key::Down) ||
            input.wasPressed(meno::Key::S)) {
            queueDirection(Direction::Down);
        }
        if (input.wasPressed(meno::Key::Left) ||
            input.wasPressed(meno::Key::A)) {
            queueDirection(Direction::Left);
        }
        if (input.wasPressed(meno::Key::Right) ||
            input.wasPressed(meno::Key::D)) {
            queueDirection(Direction::Right);
        }

        moveElapsed += dt;
        while (screen == Screen::Play &&
               moveElapsed + 1e-9 >= moveInterval) {
            const double interval = moveInterval;
            moveElapsed = std::max(0.0, moveElapsed - interval);
            moveOnce();
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                          {.fill = boardColor});

        const auto centeredText = [&](const std::string& text,
                                      float y, unsigned int size) {
            meno::TextParams params{
                .characterSize = size,
                .color = textColor
            };
            params.origin =
                renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {480.f, y}, params);
        };

        if (screen == Screen::Title) {
            centeredText("SNAKE", 220.f, 48);
            centeredText("PRESS ENTER", 310.f, 32);
            return;
        }

        if (screen == Screen::Result) {
            centeredText("GAME OVER", 190.f, 48);
            centeredText("SCORE " + std::to_string(score), 270.f, 32);
            centeredText("R: RESTART  ESC: QUIT", 350.f, 32);
            return;
        }

        renderer.drawRect({{0.f, 0.f}, {960.f, ScoreBarHeight}},
                          {.fill = barColor});
        renderer.drawText(*font, "SCORE " + std::to_string(score),
                          {8.f, 4.f},
                          {.characterSize = 16, .color = textColor});

        if (food.x >= 0) {
            renderer.drawCircle(
                {(static_cast<float>(food.x) + 0.5f) * CellSize,
                 ScoreBarHeight +
                     (static_cast<float>(food.y) + 0.5f) * CellSize},
                12.f, {.fill = foodColor});
        }

        for (std::size_t i = 0; i < snake.size(); ++i) {
            const Cell cell = snake[i];
            renderer.drawRect(
                {{static_cast<float>(cell.x) * CellSize + 1.f,
                  ScoreBarHeight +
                      static_cast<float>(cell.y) * CellSize + 1.f},
                 {28.f, 28.f}},
                {.fill = i == 0 ? headColor : bodyColor});
        }
    });

    scene.setExitHandler([&]() {
        music->stop();
    });

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
```