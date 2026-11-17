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

constexpr unsigned int kWindowWidth = 960;
constexpr unsigned int kWindowHeight = 540;
constexpr float kBarHeight = 30.f;
constexpr float kCellSize = 30.f;
constexpr int kCols = 32;
constexpr int kRows = 17;
constexpr double kStartInterval = 0.125;
constexpr double kMinInterval = 0.06;
constexpr double kTitleDuration = 2.0;
constexpr std::size_t kMaxQueued = 2;

struct Cell {
    int x = 0;
    int y = 0;
    bool operator==(const Cell&) const = default;
};

bool isOpposite(Cell a, Cell b) {
    return a.x == -b.x && a.y == -b.y;
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {kWindowWidth, kWindowHeight},
                     .title = "SNAKE",
                     .clearColor = meno::Color::fromRgb(0x0E1A12)}};

    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color headColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);
    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);

    // 에셋 읽기
    const char* fontPath = "assets/fonts/game.ttf";
    const char* pointPath = "assets/sfx/point.wav";
    const char* losePath = "assets/sfx/lose.wav";
    const char* musicPath = "assets/music/bgm_snake.ogg";

    const auto font = game.resources().loadFont(fontPath);
    const auto pointBuffer = game.resources().loadSoundBuffer(pointPath);
    const auto loseBuffer = game.resources().loadSoundBuffer(losePath);
    std::optional<meno::Music> music = meno::Music::openFromFile(musicPath);

    std::vector<std::string> missing;
    if (!font) {
        missing.emplace_back(fontPath);
    }
    if (!pointBuffer) {
        missing.emplace_back(pointPath);
    }
    if (!loseBuffer) {
        missing.emplace_back(losePath);
    }
    if (!music) {
        missing.emplace_back(musicPath);
    }
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};
    music->setLooping(true);
    music->setVolume(0.4f);

    // 게임 상태
    std::mt19937 rng{std::random_device{}()};
    std::deque<Cell> snake;
    Cell direction{1, 0};
    std::deque<Cell> queued;
    Cell food{0, 0};
    bool hasFood = false;
    int score = 0;
    int eaten = 0;
    double interval = kStartInterval;
    double accumulator = 0.0;
    bool over = false;
    double titleTimer = 0.0;

    const auto placeFood = [&] {
        std::vector<Cell> empty;
        empty.reserve(static_cast<std::size_t>(kCols * kRows));
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                const Cell c{x, y};
                if (std::find(snake.begin(), snake.end(), c) == snake.end()) {
                    empty.push_back(c);
                }
            }
        }
        if (empty.empty()) {
            hasFood = false;
            return;
        }
        std::uniform_int_distribution<std::size_t> pick{0, empty.size() - 1};
        food = empty[pick(rng)];
        hasFood = true;
    };

    const auto resetGame = [&] {
        snake.clear();
        snake.push_back({16, 8});
        snake.push_back({15, 8});
        snake.push_back({14, 8});
        direction = {1, 0};
        queued.clear();
        score = 0;
        eaten = 0;
        interval = kStartInterval;
        accumulator = 0.0;
        over = false;
        placeFood();
    };

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text,
                                  unsigned int size, float y) {
        meno::TextParams params{.characterSize = size, .color = textColor};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {static_cast<float>(kWindowWidth) / 2.f, y}, params);
    };

    const auto drawBackground = [&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f},
                           {static_cast<float>(kWindowWidth), static_cast<float>(kWindowHeight)}},
                          {.fill = boardColor});
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // 타이틀
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleDuration) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "SNAKE", 48, 220.f);
        drawCentered(renderer, "PRESS ENTER", 16, 300.f);
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
        if (over) {
            return;
        }

        // 방향 입력 보관
        const auto tryQueue = [&](Cell d) {
            const Cell last = queued.empty() ? direction : queued.back();
            if (d == last || isOpposite(d, last)) {
                return;
            }
            if (queued.size() < kMaxQueued) {
                queued.push_back(d);
            }
        };
        if (input.wasPressed(meno::Key::Up) || input.wasPressed(meno::Key::W)) {
            tryQueue({0, -1});
        }
        if (input.wasPressed(meno::Key::Down) || input.wasPressed(meno::Key::S)) {
            tryQueue({0, 1});
        }
        if (input.wasPressed(meno::Key::Left) || input.wasPressed(meno::Key::A)) {
            tryQueue({-1, 0});
        }
        if (input.wasPressed(meno::Key::Right) || input.wasPressed(meno::Key::D)) {
            tryQueue({1, 0});
        }

        accumulator += dt;
        while (!over && accumulator >= interval - 1e-9) {
            accumulator -= interval;

            if (!queued.empty()) {
                direction = queued.front();
                queued.pop_front();
            }

            const Cell head = snake.front();
            const Cell next{head.x + direction.x, head.y + direction.y};
            const bool eating = hasFood && next == food;

            bool hit = next.x < 0 || next.x >= kCols || next.y < 0 || next.y >= kRows;
            if (!hit) {
                const std::size_t count = eating ? snake.size() : snake.size() - 1;
                for (std::size_t i = 0; i < count; ++i) {
                    if (snake[i] == next) {
                        hit = true;
                        break;
                    }
                }
            }

            if (hit) {
                over = true;
                loseSound.play();
                (void)scenes.changeScene(result.id());
                break;
            }

            snake.push_front(next);
            if (eating) {
                score += 10;
                ++eaten;
                pointSound.play();
                if (eaten % 5 == 0) {
                    interval = std::max(interval * 0.9, kMinInterval);
                }
                placeFood();
            } else {
                snake.pop_back();
            }
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        renderer.drawRect({{0.f, 0.f}, {static_cast<float>(kWindowWidth), kBarHeight}},
                          {.fill = barColor});

        if (hasFood) {
            const meno::Vec2f center{static_cast<float>(food.x) * kCellSize + kCellSize / 2.f,
                                     kBarHeight + static_cast<float>(food.y) * kCellSize +
                                         kCellSize / 2.f};
            renderer.drawCircle(center, 12.f, {.fill = foodColor});
        }

        for (std::size_t i = snake.size(); i-- > 0;) {
            const Cell c = snake[i];
            const meno::Rectf rect{{static_cast<float>(c.x) * kCellSize + 1.f,
                                    kBarHeight + static_cast<float>(c.y) * kCellSize + 1.f},
                                   {28.f, 28.f}};
            renderer.drawRect(rect, {.fill = i == 0 ? headColor : bodyColor});
        }

        const std::string text = "SCORE " + std::to_string(score);
        meno::TextParams params{.characterSize = 16, .color = textColor};
        const meno::Rectf bounds = renderer.measureTextBounds(*font, text, params);
        params.origin = {bounds.position.x, bounds.position.y + bounds.size.y / 2.f};
        renderer.drawText(*font, text, {10.f, kBarHeight / 2.f}, params);
    });

    // 결과
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
        drawBackground(renderer);
        drawCentered(renderer, "GAME OVER", 48, 200.f);
        drawCentered(renderer, "SCORE " + std::to_string(score), 32, 270.f);
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 16, 330.f);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```