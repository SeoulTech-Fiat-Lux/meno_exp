#include <meno/Meno.hpp>

#include <algorithm>
#include <deque>
#include <iostream>
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
constexpr float kWinW = 960.f;
constexpr float kWinH = 540.f;
constexpr double kStartInterval = 0.125;
constexpr double kMinInterval = 0.06;

} // namespace

int main() {
    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color bodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color headColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);
    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);

    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SNAKE",
                     .clearColor = boardColor}};

    const char* fontPath = "assets/fonts/game.ttf";
    const char* pointPath = "assets/sfx/point.wav";
    const char* losePath = "assets/sfx/lose.wav";
    const char* musicPath = "assets/music/bgm_snake.ogg";

    const auto font = game.resources().loadFont(fontPath);
    const auto pointBuffer = game.resources().loadSoundBuffer(pointPath);
    const auto loseBuffer = game.resources().loadSoundBuffer(losePath);
    std::optional<meno::Music> music = meno::Music::openFromFile(musicPath);

    std::vector<std::string> missing;
    if (!font) missing.emplace_back(fontPath);
    if (!pointBuffer) missing.emplace_back(pointPath);
    if (!loseBuffer) missing.emplace_back(losePath);
    if (!music) missing.emplace_back(musicPath);
    if (!missing.empty()) {
        for (const auto& p : missing) {
            std::cerr << "Failed to load asset: " << p << '\n';
        }
        return 1;
    }

    meno::Sound pointSound{pointBuffer};
    meno::Sound loseSound{loseBuffer};
    music->setLooping(true);
    music->setVolume(0.4f);

    std::mt19937 rng{std::random_device{}()};

    // ---- 게임 상태 ----
    std::deque<Cell> snake; // front = 머리
    Cell dir{1, 0};
    std::deque<Cell> pending;
    std::optional<Cell> food;
    int score = 0;
    int eaten = 0;
    double interval = kStartInterval;
    double moveTimer = 0.0;
    bool dead = false;
    double titleTimer = 0.0;

    const auto placeFood = [&] {
        std::vector<Cell> free;
        free.reserve(kCols * kRows);
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                const Cell c{x, y};
                if (std::find(snake.begin(), snake.end(), c) == snake.end()) {
                    free.push_back(c);
                }
            }
        }
        if (free.empty()) {
            food.reset();
            return;
        }
        std::uniform_int_distribution<std::size_t> pick{0, free.size() - 1};
        food = free[pick(rng)];
    };

    const auto resetGame = [&] {
        snake.clear();
        snake.push_back({16, 8});
        snake.push_back({15, 8});
        snake.push_back({14, 8});
        dir = {1, 0};
        pending.clear();
        score = 0;
        eaten = 0;
        interval = kStartInterval;
        moveTimer = 0.0;
        dead = false;
        placeFood();
    };

    // ---- 글자 그리기 도우미 ----
    const auto drawCentered = [&](meno::Renderer& r, const std::string& text, unsigned int size,
                                  meno::Vec2f pos) {
        meno::TextParams params{.characterSize = size, .color = textColor};
        params.origin = r.measureTextBounds(*font, text, params).center();
        r.drawText(*font, text, pos, params);
    };
    const auto drawLeft = [&](meno::Renderer& r, const std::string& text, unsigned int size,
                              meno::Vec2f pos) {
        meno::TextParams params{.characterSize = size, .color = textColor};
        const meno::Rectf b = r.measureTextBounds(*font, text, params);
        params.origin = {b.position.x, b.center().y};
        r.drawText(*font, text, pos, params);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    const auto escPressed = [&] {
        if (game.window().input().wasPressed(meno::Key::Escape)) {
            game.stop();
            return true;
        }
        return false;
    };

    // ---- 타이틀 ----
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        if (escPressed()) return;
        titleTimer += dt;
        if (game.window().input().wasPressed(meno::Key::Enter) || titleTimer >= 2.0) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        r.drawRect({{0.f, 0.f}, {kWinW, kWinH}}, {.fill = boardColor});
        drawCentered(r, "SNAKE", 48, {kWinW / 2.f, 220.f});
        drawCentered(r, "PRESS ENTER", 16, {kWinW / 2.f, 300.f});
    });

    // ---- 플레이 ----
    const auto queueDir = [&](Cell d) {
        const Cell last = pending.empty() ? dir : pending.back();
        if (d == last || (d.x == -last.x && d.y == -last.y)) return;
        if (pending.size() < 2) pending.push_back(d);
    };

    const auto step = [&] {
        if (!pending.empty()) {
            dir = pending.front();
            pending.pop_front();
        }
        const Cell head = snake.front();
        const Cell next{head.x + dir.x, head.y + dir.y};
        const bool eating = food && next == *food;

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
            dead = true;
            loseSound.play();
            (void)scenes.changeScene(result.id());
            return;
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
    };

    play.setEnterHandler([&] {
        resetGame();
        music->play();
    });
    play.setExitHandler([&] { music->stop(); });
    play.setUpdateHandler([&](double dt) {
        if (escPressed()) return;
        if (dead) return;
        const meno::Input& in = game.window().input();
        if (in.wasPressed(meno::Key::Up) || in.wasPressed(meno::Key::W)) queueDir({0, -1});
        if (in.wasPressed(meno::Key::Down) || in.wasPressed(meno::Key::S)) queueDir({0, 1});
        if (in.wasPressed(meno::Key::Left) || in.wasPressed(meno::Key::A)) queueDir({-1, 0});
        if (in.wasPressed(meno::Key::Right) || in.wasPressed(meno::Key::D)) queueDir({1, 0});

        moveTimer += dt;
        while (moveTimer >= interval - 1e-9) {
            moveTimer -= interval;
            step();
            if (dead) break;
        }
    });
    play.setDrawHandler([&](meno::Renderer& r) {
        r.drawRect({{0.f, kBarHeight}, {kWinW, kWinH - kBarHeight}}, {.fill = boardColor});
        r.drawRect({{0.f, 0.f}, {kWinW, kBarHeight}}, {.fill = barColor});

        if (food) {
            const meno::Vec2f c{food->x * kCell + kCell / 2.f,
                                kBarHeight + food->y * kCell + kCell / 2.f};
            r.drawCircle(c, 12.f, {.fill = foodColor});
        }
        for (std::size_t i = snake.size(); i-- > 0;) {
            const Cell& s = snake[i];
            r.drawRect({{s.x * kCell + 1.f, kBarHeight + s.y * kCell + 1.f}, {28.f, 28.f}},
                       {.fill = i == 0 ? headColor : bodyColor});
        }
        drawLeft(r, "SCORE " + std::to_string(score), 16, {10.f, kBarHeight / 2.f});
    });

    // ---- 결과 ----
    result.setUpdateHandler([&](double) {
        if (escPressed()) return;
        if (game.window().input().wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& r) {
        r.drawRect({{0.f, 0.f}, {kWinW, kWinH}}, {.fill = boardColor});
        drawCentered(r, "GAME OVER", 48, {kWinW / 2.f, 200.f});
        drawCentered(r, "SCORE " + std::to_string(score), 32, {kWinW / 2.f, 280.f});
        drawCentered(r, "R: RESTART  ESC: QUIT", 16, {kWinW / 2.f, 350.f});
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
