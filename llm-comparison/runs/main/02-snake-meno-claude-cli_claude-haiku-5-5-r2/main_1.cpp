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
constexpr float kCell = 30.f;
constexpr float kBarHeight = 30.f;
constexpr float kBoardHeight = 510.f;
constexpr double kStartInterval = 0.125;
constexpr double kMinInterval = 0.06;
constexpr std::size_t kMaxPending = 2;

constexpr const char* kFontPath = "assets/fonts/game.ttf";
constexpr const char* kPointPath = "assets/sfx/point.wav";
constexpr const char* kLosePath = "assets/sfx/lose.wav";
constexpr const char* kBgmPath = "assets/music/bgm_snake.ogg";

struct Cell {
    int x = 0;
    int y = 0;
    friend bool operator==(const Cell&, const Cell&) = default;
};

Cell operator+(Cell a, Cell b) {
    return Cell{a.x + b.x, a.y + b.y};
}

enum class StepResult { Moved, Ate, Died };

class Snake {
public:
    Snake() { reset(); }

    void reset() {
        body_ = {Cell{16, 8}, Cell{15, 8}, Cell{14, 8}};
        dir_ = Cell{1, 0};
        pending_.clear();
        moveTimer_ = 0.0;
        interval_ = kStartInterval;
        eaten_ = 0;
        score_ = 0;
        placeFood();
    }

    // 한 이동 간격 안에 누른 방향을 최대 2개까지 보관합니다.
    void queueTurn(Cell direction) {
        if (pending_.size() >= kMaxPending) {
            return;
        }
        const Cell last = pending_.empty() ? dir_ : pending_.back();
        if (direction == last || direction + last == Cell{}) {
            return;
        }
        pending_.push_back(direction);
    }

    bool moveDue(double dt) {
        moveTimer_ += dt;
        if (moveTimer_ < interval_) {
            return false;
        }
        moveTimer_ -= interval_;
        return true;
    }

    StepResult step() {
        if (!pending_.empty()) {
            dir_ = pending_.front();
            pending_.pop_front();
        }
        const Cell next = body_.front() + dir_;
        if (next.x < 0 || next.x >= kCols || next.y < 0 || next.y >= kRows) {
            return StepResult::Died;
        }
        const bool eat = next == food_;
        if (!eat) {
            body_.pop_back(); // 꼬리가 빠져나가는 칸은 충돌로 보지 않습니다.
        }
        if (std::find(body_.begin(), body_.end(), next) != body_.end()) {
            return StepResult::Died;
        }
        body_.push_front(next);
        if (!eat) {
            return StepResult::Moved;
        }
        score_ += 10;
        ++eaten_;
        if (eaten_ % 5 == 0) {
            interval_ = std::max(kMinInterval, interval_ * 0.9);
        }
        placeFood();
        return StepResult::Ate;
    }

    const std::deque<Cell>& body() const { return body_; }
    const Cell& food() const { return food_; }
    int score() const { return score_; }

private:
    void placeFood() {
        std::vector<Cell> open;
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                const Cell c{x, y};
                if (std::find(body_.begin(), body_.end(), c) == body_.end()) {
                    open.push_back(c);
                }
            }
        }
        if (open.empty()) {
            return;
        }
        std::uniform_int_distribution<std::size_t> pick{0, open.size() - 1};
        food_ = open[pick(rng_)];
    }

    std::deque<Cell> body_;
    Cell dir_{};
    std::deque<Cell> pending_;
    Cell food_{};
    double moveTimer_ = 0.0;
    double interval_ = kStartInterval;
    int eaten_ = 0;
    int score_ = 0;
    std::mt19937 rng_{std::random_device{}()};
};

meno::Rectf cellRect(Cell c) {
    return meno::Rectf{{static_cast<float>(c.x) * kCell + 1.f,
                        kBarHeight + static_cast<float>(c.y) * kCell + 1.f},
                       {28.f, 28.f}};
}

meno::Vec2f cellCenter(Cell c) {
    return meno::Vec2f{static_cast<float>(c.x) * kCell + kCell / 2.f,
                       kBarHeight + static_cast<float>(c.y) * kCell + kCell / 2.f};
}

void drawLabel(meno::Renderer& renderer, const meno::Font& font, const std::string& text,
               meno::Vec2f position, int size, meno::Color color, bool centered) {
    meno::TextParams params;
    params.characterSize = static_cast<decltype(params.characterSize)>(size);
    params.color = color;
    if (centered) {
        params.origin = renderer.measureTextBounds(font, text, params).center();
    }
    renderer.drawText(font, text, position, params);
}

} // namespace

int main() {
    const meno::Color boardColor = meno::Color::fromRgb(0x0E1A12);
    const meno::Color barColor = meno::Color::fromRgb(0x1F3A26);
    const meno::Color snakeBodyColor = meno::Color::fromRgb(0x5FCB5F);
    const meno::Color snakeHeadColor = meno::Color::fromRgb(0xB6F2A0);
    const meno::Color foodColor = meno::Color::fromRgb(0xE5533D);
    const meno::Color textColor = meno::Color::fromRgb(0xE8F5E9);

    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960u, 540u},
                     .title = "SNAKE",
                     .clearColor = boardColor}};

    const auto font = game.resources().loadFont(kFontPath);
    const auto pointBuffer = game.resources().loadSoundBuffer(kPointPath);
    const auto loseBuffer = game.resources().loadSoundBuffer(kLosePath);
    auto bgmOpt = meno::Music::openFromFile(kBgmPath);

    bool assetsOk = true;
    const auto requireAsset = [&](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            assetsOk = false;
        }
    };
    requireAsset(font != nullptr, kFontPath);
    requireAsset(pointBuffer != nullptr, kPointPath);
    requireAsset(loseBuffer != nullptr, kLosePath);
    requireAsset(bgmOpt.has_value(), kBgmPath);
    if (!assetsOk) {
        return 1;
    }

    const meno::Font& fontRef = *font;
    meno::Sound pointSfx{pointBuffer};
    meno::Sound loseSfx{loseBuffer};
    meno::Music& bgm = *bgmOpt;
    bgm.setLooping(true);
    bgm.setVolume(0.4f);

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    Snake snake;
    double titleTimer = 0.0;

    // 타이틀: Enter를 누르거나 2초가 지나면 플레이를 시작합니다.
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
        drawLabel(renderer, fontRef, "SNAKE", {480.f, 200.f}, 48, textColor, true);
        drawLabel(renderer, fontRef, "PRESS ENTER", {480.f, 300.f}, 32, textColor, true);
    });

    // 플레이: 들어올 때마다 상태를 초기화하고 배경음악을 재생합니다.
    play.setEnterHandler([&] {
        snake.reset();
        bgm.play();
    });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (input.wasPressed(meno::Key::Up) || input.wasPressed(meno::Key::W)) {
            snake.queueTurn(Cell{0, -1});
        }
        if (input.wasPressed(meno::Key::Down) || input.wasPressed(meno::Key::S)) {
            snake.queueTurn(Cell{0, 1});
        }
        if (input.wasPressed(meno::Key::Left) || input.wasPressed(meno::Key::A)) {
            snake.queueTurn(Cell{-1, 0});
        }
        if (input.wasPressed(meno::Key::Right) || input.wasPressed(meno::Key::D)) {
            snake.queueTurn(Cell{1, 0});
        }

        if (!snake.moveDue(dt)) {
            return;
        }
        switch (snake.step()) {
        case StepResult::Died:
            loseSfx.play();
            (void)scenes.changeScene(result.id());
            break;
        case StepResult::Ate:
            pointSfx.play();
            break;
        case StepResult::Moved:
            break;
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {960.f, kBarHeight}}, {.fill = barColor});
        renderer.drawRect({{0.f, kBarHeight}, {960.f, kBoardHeight}}, {.fill = boardColor});

        renderer.drawCircle(cellCenter(snake.food()), 12.f, {.fill = foodColor});

        const std::deque<Cell>& body = snake.body();
        for (std::size_t i = 0; i < body.size(); ++i) {
            renderer.drawRect(cellRect(body[i]),
                              {.fill = i == 0 ? snakeHeadColor : snakeBodyColor});
        }

        drawLabel(renderer, fontRef, "SCORE " + std::to_string(snake.score()), {16.f, 7.f}, 16,
                  textColor, false);
    });

    // 결과: 들어오면 배경음악을 멈추고, R로 플레이를 다시 시작합니다.
    result.setEnterHandler([&] { bgm.stop(); });
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
        drawLabel(renderer, fontRef, "GAME OVER", {480.f, 180.f}, 48, textColor, true);
        drawLabel(renderer, fontRef, "SCORE " + std::to_string(snake.score()), {480.f, 280.f},
                  32, textColor, true);
        drawLabel(renderer, fontRef, "R: RESTART  ESC: QUIT", {480.f, 380.f}, 16, textColor,
                  true);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
