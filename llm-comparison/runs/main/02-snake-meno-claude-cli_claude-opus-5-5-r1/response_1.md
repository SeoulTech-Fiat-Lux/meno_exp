```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

constexpr int kCols = 32;
constexpr int kRows = 17;
constexpr float kCell = 30.f;
constexpr float kBarHeight = 30.f;
constexpr double kStartInterval = 0.125;
constexpr double kMinInterval = 0.06;

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SNAKE",
                     .clearColor = meno::Color::fromRgb(0x0E1A12)}};

    const char* fontPath = "assets/fonts/game.ttf";
    const char* pointPath = "assets/sfx/point.wav";
    const char* losePath = "assets/sfx/lose.wav";
    const char* musicPath = "assets/music/bgm_snake.ogg";

    const auto font = game.resources().loadFont(fontPath);
    const auto pointBuffer = game.resources().loadSoundBuffer(pointPath);
    const auto loseBuffer = game.resources().loadSoundBuffer(losePath);
    std::optional<meno::Music> music = meno::Music::openFromFile(musicPath);

    bool failed = false;
    if (!font) {
        std::cerr << "Failed to load: " << fontPath << '\n';
        failed = true;
    }
    if (!pointBuffer) {
        std::cerr << "Failed to load: " << pointPath << '\n';
        failed = true;
    }
    if (!loseBuffer) {
        std::cerr << "Failed to load: " << losePath << '\n';
        failed = true;
    }
    if (!music) {
        std::cerr << "Failed to load: " << musicPath << '\n';
        failed = true;
    }
    if (failed) {
        return 1;
    }

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};
    music->setLooping(true);
    music->setVolume(0.4f);

    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color headColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);
    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);

    // 게임 상태
    std::deque<Cell> snake;
    Cell direction{1, 0};
    std::deque<Cell> pending;
    Cell food{0, 0};
    bool hasFood = false;
    int score = 0;
    int eaten = 0;
    double interval = kStartInterval;
    double accumulator = 0.0;
    double titleTimer = 0.0;

    std::mt19937 rng{std::random_device{}()};

    const auto placeFood = [&] {
        std::vector<Cell> freeCells;
        freeCells.reserve(kCols * kRows);
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                const Cell c{x, y};
                if (std::find(snake.begin(), snake.end(), c) == snake.end()) {
                    freeCells.push_back(c);
                }
            }
        }
        if (freeCells.empty()) {
            hasFood = false;
            return;
        }
        std::uniform_int_distribution<std::size_t> pick{0, freeCells.size() - 1};
        food = freeCells[pick(rng)];
        hasFood = true;
    };

    const auto resetGame = [&] {
        snake.clear();
        snake.push_back({16, 8});
        snake.push_back({15, 8});
        snake.push_back({14, 8});
        direction = {1, 0};
        pending.clear();
        score = 0;
        eaten = 0;
        interval = kStartInterval;
        accumulator = 0.0;
        placeFood();
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text,
                                  unsigned int size, meno::Vec2f pos) {
        meno::TextParams params{.characterSize = size, .color = textColor};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, pos, params);
    };

    const auto drawBoard = [&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, kBarHeight}, {kCols * kCell, kRows * kCell}},
                          {.fill = boardColor});
        renderer.drawRect({{0.f, 0.f}, {960.f, kBarHeight}}, {.fill = barColor});
        if (hasFood) {
            renderer.drawCircle({static_cast<float>(food.x) * kCell + kCell / 2.f,
                                 kBarHeight + static_cast<float>(food.y) * kCell + kCell / 2.f},
                                12.f, {.fill = foodColor});
        }
        for (std::size_t i = snake.size(); i-- > 0;) {
            const Cell& c = snake[i];
            renderer.drawRect({{static_cast<float>(c.x) * kCell + 1.f,
                                kBarHeight + static_cast<float>(c.y) * kCell + 1.f},
                               {28.f, 28.f}},
                              {.fill = i == 0 ? headColor : bodyColor});
        }
        const std::string scoreText = "SCORE " + std::to_string(score);
        meno::TextParams params{.characterSize = 16, .color = textColor};
        const meno::Rectf bounds = renderer.measureTextBounds(*font, scoreText, params);
        params.origin = {bounds.left(), bounds.center().y};
        renderer.drawText(*font, scoreText, {8.f, kBarHeight / 2.f}, params);
    };

    // 타이틀
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}}, {.fill = boardColor});
        drawCentered(renderer, "SNAKE", 48, {480.f, 230.f});
        drawCentered(renderer, "PRESS ENTER", 16, {480.f, 310.f});
    });

    // 플레이
    play.setEnterHandler([&] {
        resetGame();
        music->play();
    });
    play.setExitHandler([&] { music->stop(); });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        const auto queueDir = [&](Cell d) {
            const Cell last = pending.empty() ? direction : pending.back();
            if (d == last || (d.x == -last.x && d.y == -last.y)) {
                return;
            }
            if (pending.size() < 2) {
                pending.push_back(d);
            }
        };
        if (input.wasPressed(meno::Key::Up) || input.wasPressed(meno::Key::W)) {
            queueDir({0, -1});
        }
        if (input.wasPressed(meno::Key::Down) || input.wasPressed(meno::Key::S)) {
            queueDir({0, 1});
        }
        if (input.wasPressed(meno::Key::Left) || input.wasPressed(meno::Key::A)) {
            queueDir({-1, 0});
        }
        if (input.wasPressed(meno::Key::Right) || input.wasPressed(meno::Key::D)) {
            queueDir({1, 0});
        }

        accumulator += dt;
        while (accumulator >= interval) {
            accumulator -= interval;

            if (!pending.empty()) {
                direction = pending.front();
                pending.pop_front();
            }
            const Cell head{snake.front().x + direction.x, snake.front().y + direction.y};

            bool dead = head.x < 0 || head.x >= kCols || head.y < 0 || head.y >= kRows;
            const bool eating = hasFood && head == food;
            if (!dead) {
                const std::size_t checkCount = eating ? snake.size() : snake.size() - 1;
                for (std::size_t i = 0; i < checkCount; ++i) {
                    if (snake[i] == head) {
                        dead = true;
                        break;
                    }
                }
            }
            if (dead) {
                loseSound.play();
                (void)scenes.changeScene(result.id());
                return;
            }

            snake.push_front(head);
            if (eating) {
                score += 10;
                ++eaten;
                pointSound.play();
                if (eaten % 5 == 0) {
                    interval = std::max(kMinInterval, interval * 0.9);
                }
                placeFood();
            } else {
                snake.pop_back();
            }
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) { drawBoard(renderer); });

    // 결과
    result.setEnterHandler([&] { music->stop(); });
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}}, {.fill = boardColor});
        drawCentered(renderer, "GAME OVER", 48, {480.f, 200.f});
        drawCentered(renderer, "SCORE " + std::to_string(score), 32, {480.f, 270.f});
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 16, {480.f, 340.f});
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```