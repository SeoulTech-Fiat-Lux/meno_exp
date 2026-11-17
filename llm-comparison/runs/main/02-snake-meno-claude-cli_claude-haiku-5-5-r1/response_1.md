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

constexpr int cols = 32;
constexpr int rows = 17;
constexpr float cell = 30.f;
constexpr float barHeight = 30.f;
constexpr double minInterval = 0.06;

struct Cell {
    int x = 0;
    int y = 0;
};

bool same(Cell a, Cell b) {
    return a.x == b.x && a.y == b.y;
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SNAKE",
                     .clearColor = meno::Color::fromRgb(0x0E1A12)}};

    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color headColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);
    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);

    // 에셋 로드와 확인
    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto pointBuffer = resources.loadSoundBuffer("assets/sfx/point.wav");
    const auto loseBuffer = resources.loadSoundBuffer("assets/sfx/lose.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_snake.ogg");

    std::vector<std::string> missing;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            missing.emplace_back(path);
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(pointBuffer != nullptr, "assets/sfx/point.wav");
    require(loseBuffer != nullptr, "assets/sfx/lose.wav");
    require(bgm.has_value(), "assets/music/bgm_snake.ogg");
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing file: " << path << '\n';
        }
        return 1;
    }

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    std::mt19937 rng{std::random_device{}()};

    meno::Scene& title = game.scenes().createScene();
    meno::Scene& play = game.scenes().createScene();
    meno::Scene& result = game.scenes().createScene();

    // 뱀 상태
    std::deque<Cell> body;
    Cell dir{1, 0};
    std::vector<Cell> pending;
    Cell food{-1, -1};
    int score = 0;
    int eaten = 0;
    double interval = 0.125;
    double moveTimer = 0.0;
    double titleTimer = 0.0;

    const auto drawCaption = [&](meno::Renderer& renderer, const std::string& text,
                                 meno::Vec2f position, meno::TextParams params) {
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, position, params);
    };

    const auto cellPos = [](Cell c) {
        return meno::Vec2f{static_cast<float>(c.x) * cell,
                           barHeight + static_cast<float>(c.y) * cell};
    };

    const auto spawnFood = [&] {
        std::vector<Cell> freeCells;
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const Cell c{x, y};
                if (std::ranges::none_of(body, [&](Cell s) { return same(s, c); })) {
                    freeCells.push_back(c);
                }
            }
        }
        if (!freeCells.empty()) {
            std::uniform_int_distribution<std::size_t> pick{0, freeCells.size() - 1};
            food = freeCells[pick(rng)];
        } else {
            food = Cell{-1, -1};
        }
    };

    const auto resetGame = [&] {
        body.assign({Cell{16, 8}, Cell{15, 8}, Cell{14, 8}});
        dir = Cell{1, 0};
        pending.clear();
        score = 0;
        eaten = 0;
        interval = 0.125;
        moveTimer = 0.0;
        spawnFood();
    };

    const auto queueDirection = [&](Cell d) {
        const Cell last = pending.empty() ? dir : pending.back();
        if (same(last, d) || (last.x == -d.x && last.y == -d.y)) {
            return;
        }
        if (pending.size() < 2) {
            pending.push_back(d);
        }
    };

    const auto step = [&] {
        if (!pending.empty()) {
            dir = pending.front();
            pending.erase(pending.begin());
        }
        const Cell head = body.front();
        const Cell next{head.x + dir.x, head.y + dir.y};
        const bool eats = same(next, food);

        bool hit = next.x < 0 || next.x >= cols || next.y < 0 || next.y >= rows;
        if (!hit) {
            // 먹이를 먹는 이동에서는 꼬리가 남으므로 전체 몸을 검사합니다.
            const std::size_t checked = eats ? body.size() : body.size() - 1;
            for (std::size_t i = 0; i < checked; ++i) {
                if (same(body[i], next)) {
                    hit = true;
                    break;
                }
            }
        }
        if (hit) {
            loseSound.play();
            (void)game.scenes().changeScene(result.id());
            return;
        }

        body.push_front(next);
        if (eats) {
            score += 10;
            ++eaten;
            pointSound.play();
            if (eaten % 5 == 0) {
                interval = std::max(minInterval, interval * 0.9);
            }
            spawnFood();
        } else {
            body.pop_back();
        }
    };

    // 타이틀: Enter 또는 2초 후 플레이 시작
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)game.scenes().changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawCaption(renderer, "SNAKE", {480.f, 220.f}, {.characterSize = 48, .color = textColor});
        drawCaption(renderer, "PRESS ENTER", {480.f, 320.f},
                    {.characterSize = 32, .color = textColor});
    });

    // 플레이
    play.setEnterHandler([&] {
        resetGame();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::Up) || input.wasPressed(meno::Key::W)) {
            queueDirection(Cell{0, -1});
        }
        if (input.wasPressed(meno::Key::Down) || input.wasPressed(meno::Key::S)) {
            queueDirection(Cell{0, 1});
        }
        if (input.wasPressed(meno::Key::Left) || input.wasPressed(meno::Key::A)) {
            queueDirection(Cell{-1, 0});
        }
        if (input.wasPressed(meno::Key::Right) || input.wasPressed(meno::Key::D)) {
            queueDirection(Cell{1, 0});
        }

        moveTimer += dt;
        if (moveTimer >= interval) {
            moveTimer -= interval;
            step();
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, barHeight}}, {.fill = barColor});
        renderer.drawRect({{0.f, barHeight}, {960.f, 510.f}}, {.fill = boardColor});

        if (food.x >= 0) {
            const meno::Vec2f p = cellPos(food);
            renderer.drawCircle({p.x + cell / 2.f, p.y + cell / 2.f}, 12.f, {.fill = foodColor});
        }
        for (std::size_t i = 0; i < body.size(); ++i) {
            const meno::Vec2f p = cellPos(body[i]);
            renderer.drawRect({{p.x + 1.f, p.y + 1.f}, {28.f, 28.f}},
                              {.fill = i == 0 ? headColor : bodyColor});
        }

        renderer.drawText(*font, "SCORE " + std::to_string(score), {12.f, 6.f},
                          {.characterSize = 16, .color = textColor});
    });

    // 결과
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
        } else if (input.wasPressed(meno::Key::R)) {
            (void)game.scenes().changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawCaption(renderer, "GAME OVER", {480.f, 200.f},
                    {.characterSize = 48, .color = textColor});
        drawCaption(renderer, "SCORE " + std::to_string(score), {480.f, 290.f},
                    {.characterSize = 32, .color = textColor});
        drawCaption(renderer, "R: RESTART  ESC: QUIT", {480.f, 380.f},
                    {.characterSize = 16, .color = textColor});
    });

    (void)game.scenes().changeScene(title.id());
    game.run();
    return 0;
}
```