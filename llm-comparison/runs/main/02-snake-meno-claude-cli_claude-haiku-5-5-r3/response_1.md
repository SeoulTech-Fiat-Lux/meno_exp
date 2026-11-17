```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kCols = 32;
constexpr int kRows = 17;
constexpr float kCellSize = 30.f;
constexpr float kBarHeight = 30.f;
constexpr double kTitleSeconds = 2.0;
constexpr double kStartInterval = 0.125;
constexpr double kMinInterval = 0.06;
constexpr double kSpeedUpFactor = 0.9;
constexpr int kFoodPerSpeedUp = 5;
constexpr int kPointsPerFood = 10;

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

bool isOpposite(Cell a, Cell b) {
    return a.x == -b.x && a.y == -b.y;
}

enum class Step { Moved, Ate, Died };

struct SnakeGame {
    std::deque<Cell> body; // 앞이 머리
    std::deque<Cell> queued; // 아직 적용되지 않은 방향 입력(최대 2개)
    Cell heading{1, 0};
    Cell food{};
    int score = 0;
    int eaten = 0;
    double interval = kStartInterval;
    double moveTimer = 0.0;
    std::mt19937 rng{std::random_device{}()};

    void reset() {
        body = {{16, 8}, {15, 8}, {14, 8}};
        heading = {1, 0};
        queued.clear();
        score = 0;
        eaten = 0;
        interval = kStartInterval;
        moveTimer = 0.0;
        spawnFood();
    }

    bool occupied(Cell c) const {
        return std::find(body.begin(), body.end(), c) != body.end();
    }

    void spawnFood() {
        std::vector<Cell> freeCells;
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                if (const Cell c{x, y}; !occupied(c)) {
                    freeCells.push_back(c);
                }
            }
        }
        if (freeCells.empty()) {
            return;
        }
        std::uniform_int_distribution<std::size_t> pick{0, freeCells.size() - 1};
        food = freeCells[pick(rng)];
    }

    void enqueue(Cell dir) {
        const Cell last = queued.empty() ? heading : queued.back();
        if (dir == last || isOpposite(dir, last) || queued.size() >= 2) {
            return;
        }
        queued.push_back(dir);
    }

    Step step() {
        if (!queued.empty()) {
            heading = queued.front();
            queued.pop_front();
        }
        const Cell next{body.front().x + heading.x, body.front().y + heading.y};
        if (next.x < 0 || next.x >= kCols || next.y < 0 || next.y >= kRows) {
            return Step::Died;
        }
        const bool eating = next == food;
        // 이번 이동에서 꼬리가 빠져나가면 꼬리 칸은 충돌로 보지 않는다.
        const std::size_t checked = eating ? body.size() : body.size() - 1;
        for (std::size_t i = 0; i < checked; ++i) {
            if (body[i] == next) {
                return Step::Died;
            }
        }
        body.push_front(next);
        if (!eating) {
            body.pop_back();
            return Step::Moved;
        }
        score += kPointsPerFood;
        ++eaten;
        if (eaten % kFoodPerSpeedUp == 0) {
            interval = std::max(kMinInterval, interval * kSpeedUpFactor);
        }
        spawnFood();
        return Step::Ate;
    }
};

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SNAKE",
                     .clearColor = meno::Color::fromRgb(0x0E1A12)}};

    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto pointBuffer = resources.loadSoundBuffer("assets/sfx/point.wav");
    const auto loseBuffer = resources.loadSoundBuffer("assets/sfx/lose.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_snake.ogg");

    std::vector<std::string> missing;
    if (!font) {
        missing.emplace_back("assets/fonts/game.ttf");
    }
    if (!pointBuffer) {
        missing.emplace_back("assets/sfx/point.wav");
    }
    if (!loseBuffer) {
        missing.emplace_back("assets/sfx/lose.wav");
    }
    if (!bgm) {
        missing.emplace_back("assets/music/bgm_snake.ogg");
    }
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};

    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color headColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);

    SnakeGame snake;
    snake.reset();
    double titleTimer = 0.0;

    const auto centered = [&](meno::Renderer& renderer, const std::string& text, float y,
                              meno::TextParams params) {
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {480.f, y}, params);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& titleScene = scenes.createScene();
    meno::Scene& playScene = scenes.createScene();
    meno::Scene& resultScene = scenes.createScene();

    // 타이틀: Enter 또는 2초 뒤 플레이로 넘어간다.
    titleScene.setEnterHandler([&] { titleTimer = 0.0; });
    titleScene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleSeconds) {
            (void)scenes.changeScene(playScene.id());
        }
    });
    titleScene.setDrawHandler([&](meno::Renderer& renderer) {
        centered(renderer, "SNAKE", 200.f, {.characterSize = 48, .color = textColor});
        centered(renderer, "PRESS ENTER", 300.f, {.characterSize = 32, .color = textColor});
    });

    // 플레이: 들어올 때마다 상태를 초기화한다(R로 재시작해도 여기를 거친다).
    playScene.setEnterHandler([&] {
        snake.reset();
        bgm->play();
    });
    playScene.setExitHandler([&] { bgm->stop(); });
    playScene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::Up) || input.wasPressed(meno::Key::W)) {
            snake.enqueue({0, -1});
        }
        if (input.wasPressed(meno::Key::Down) || input.wasPressed(meno::Key::S)) {
            snake.enqueue({0, 1});
        }
        if (input.wasPressed(meno::Key::Left) || input.wasPressed(meno::Key::A)) {
            snake.enqueue({-1, 0});
        }
        if (input.wasPressed(meno::Key::Right) || input.wasPressed(meno::Key::D)) {
            snake.enqueue({1, 0});
        }

        snake.moveTimer += dt;
        while (snake.moveTimer >= snake.interval) {
            snake.moveTimer -= snake.interval;
            const Step result = snake.step();
            if (result == Step::Died) {
                loseSound.play();
                (void)scenes.changeScene(resultScene.id());
                return;
            }
            if (result == Step::Ate) {
                pointSound.play();
            }
        }
    });
    playScene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, kBarHeight}}, {.fill = barColor});
        renderer.drawRect({{0.f, kBarHeight}, {960.f, kRows * kCellSize}}, {.fill = boardColor});

        for (std::size_t i = 0; i < snake.body.size(); ++i) {
            const Cell c = snake.body[i];
            const meno::Rectf rect{{c.x * kCellSize + 1.f, kBarHeight + c.y * kCellSize + 1.f},
                                   {28.f, 28.f}};
            renderer.drawRect(rect, {.fill = i == 0 ? headColor : bodyColor});
        }

        const meno::Vec2f foodCenter{snake.food.x * kCellSize + kCellSize / 2.f,
                                     kBarHeight + snake.food.y * kCellSize + kCellSize / 2.f};
        renderer.drawCircle(foodCenter, 12.f, {.fill = foodColor});

        renderer.drawText(*font, "SCORE " + std::to_string(snake.score), {12.f, 6.f},
                          {.characterSize = 16, .color = textColor});
    });

    // 결과: R로 플레이를 다시 시작하고, Esc로 종료한다.
    resultScene.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(playScene.id());
        }
    });
    resultScene.setDrawHandler([&](meno::Renderer& renderer) {
        centered(renderer, "GAME OVER", 180.f, {.characterSize = 48, .color = textColor});
        centered(renderer, "SCORE " + std::to_string(snake.score), 270.f,
                 {.characterSize = 32, .color = textColor});
        centered(renderer, "R: RESTART  ESC: QUIT", 360.f,
                 {.characterSize = 16, .color = textColor});
    });

    (void)scenes.changeScene(titleScene.id());
    game.run();
}
```