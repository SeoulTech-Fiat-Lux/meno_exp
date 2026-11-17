#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int   kCols = 32;
constexpr int   kRows = 17;
constexpr float kCell = 30.f;
constexpr float kBarH = 30.f;
constexpr float kWinW = 960.f;
constexpr float kWinH = 540.f;

constexpr float kStartInterval = 0.125f;
constexpr float kMinInterval   = 0.06f;
constexpr float kTitleSeconds  = 2.f;
constexpr int   kPointsPerFood = 10;
constexpr int   kFoodPerSpeedUp = 5;

const sf::Color kBoardBg(0x0E, 0x1A, 0x12);
const sf::Color kBarColor(0x1F, 0x3A, 0x26);
const sf::Color kBodyColor(0x5F, 0xCB, 0x5F);
const sf::Color kHeadColor(0xB6, 0xF2, 0xA0);
const sf::Color kFoodColor(0xE5, 0x53, 0x3D);
const sf::Color kTextColor(0xE8, 0xF5, 0xE9);

struct Cell {
    int x;
    int y;
    bool operator==(const Cell&) const = default;
};

Cell operator+(Cell a, Cell b) {
    return {a.x + b.x, a.y + b.y};
}

Cell opposite(Cell c) {
    return {-c.x, -c.y};
}

enum class Screen { Title, Play, Result };
enum class StepResult { Moved, Ate, Died };

struct Game {
    Screen screen = Screen::Title;
    std::deque<Cell> body;
    Cell dir{1, 0};
    std::deque<Cell> inputs;   // 최대 2개 보관
    Cell food{-1, -1};
    int eaten = 0;
    int score = 0;
    float interval = kStartInterval;
    float moveTimer = 0.f;
    std::mt19937 rng{std::random_device{}()};

    void resetState() {
        body = {{16, 8}, {15, 8}, {14, 8}};
        dir = {1, 0};
        inputs.clear();
        eaten = 0;
        score = 0;
        interval = kStartInterval;
        moveTimer = 0.f;
        spawnFood();
    }

    void startGame() {
        resetState();
        screen = Screen::Play;
    }

    void spawnFood() {
        std::vector<Cell> freeCells;
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                const Cell c{x, y};
                if (std::find(body.begin(), body.end(), c) == body.end())
                    freeCells.push_back(c);
            }
        }
        if (freeCells.empty()) {
            food = {-1, -1};
            return;
        }
        std::uniform_int_distribution<std::size_t> dist(0, freeCells.size() - 1);
        food = freeCells[dist(rng)];
    }

    void queueDir(Cell d) {
        const Cell last = inputs.empty() ? dir : inputs.back();
        if (d == last || d == opposite(last))
            return;
        if (inputs.size() >= 2)
            return;
        inputs.push_back(d);
    }

    StepResult step() {
        if (!inputs.empty()) {
            dir = inputs.front();
            inputs.pop_front();
        }

        const Cell next = body.front() + dir;
        if (next.x < 0 || next.x >= kCols || next.y < 0 || next.y >= kRows)
            return StepResult::Died;

        const bool eating = (next == food);
        // 먹이를 먹는 이동에서는 꼬리가 남으므로 전체 몸을 검사한다.
        const std::size_t checkCount = eating ? body.size() : body.size() - 1;
        for (std::size_t i = 0; i < checkCount; ++i) {
            if (body[i] == next)
                return StepResult::Died;
        }

        body.push_front(next);
        if (!eating) {
            body.pop_back();
            return StepResult::Moved;
        }

        ++eaten;
        score += kPointsPerFood;
        if (eaten % kFoodPerSpeedUp == 0)
            interval = std::max(kMinInterval, interval * 0.9f);
        spawnFood();
        return StepResult::Ate;
    }
};

std::optional<Cell> dirFromKey(sf::Keyboard::Scancode s) {
    switch (s) {
    case sf::Keyboard::Scancode::Up:
    case sf::Keyboard::Scancode::W:
        return Cell{0, -1};
    case sf::Keyboard::Scancode::Down:
    case sf::Keyboard::Scancode::S:
        return Cell{0, 1};
    case sf::Keyboard::Scancode::Left:
    case sf::Keyboard::Scancode::A:
        return Cell{-1, 0};
    case sf::Keyboard::Scancode::Right:
    case sf::Keyboard::Scancode::D:
        return Cell{1, 0};
    default:
        return std::nullopt;
    }
}

void requireLoaded(bool ok, const char* path) {
    if (!ok) {
        std::cerr << "failed to load asset: " << path << '\n';
        std::exit(1);
    }
}

void drawCentered(sf::RenderWindow& w, const sf::Font& font, const std::string& s,
                  unsigned int size, float y, sf::Color color) {
    sf::Text t(font, sf::String(s), size);
    t.setFillColor(color);
    const sf::FloatRect b = t.getLocalBounds();
    t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    t.setPosition({kWinW / 2.f, y});
    w.draw(t);
}

void drawPlay(sf::RenderWindow& w, const sf::Font& font, const Game& game) {
    // 점수 표시줄
    sf::RectangleShape bar(sf::Vector2f{kWinW, kBarH});
    bar.setPosition({0.f, 0.f});
    bar.setFillColor(kBarColor);
    w.draw(bar);

    sf::Text label(font, sf::String(std::string("SCORE ") + std::to_string(game.score)), 16);
    label.setFillColor(kTextColor);
    const sf::FloatRect lb = label.getLocalBounds();
    label.setOrigin({lb.position.x, lb.position.y + lb.size.y / 2.f});
    label.setPosition({12.f, kBarH / 2.f});
    w.draw(label);

    // 먹이: 칸 가운데 반지름 12 원 (칸 원점 + 3)
    if (game.food.x >= 0) {
        sf::CircleShape food(12.f);
        food.setFillColor(kFoodColor);
        food.setPosition({game.food.x * kCell + 3.f, kBarH + game.food.y * kCell + 3.f});
        w.draw(food);
    }

    // 뱀: 칸 안 1px 여백의 28x28 사각형
    sf::RectangleShape cellShape(sf::Vector2f{28.f, 28.f});
    for (std::size_t i = 0; i < game.body.size(); ++i) {
        const Cell c = game.body[i];
        cellShape.setFillColor(i == 0 ? kHeadColor : kBodyColor);
        cellShape.setPosition({c.x * kCell + 1.f, kBarH + c.y * kCell + 1.f});
        w.draw(cellShape);
    }
}

}  // namespace

int main() {
    sf::Font font;
    requireLoaded(font.openFromFile("assets/fonts/game.ttf"), "assets/fonts/game.ttf");

    sf::SoundBuffer pointBuf;
    requireLoaded(pointBuf.loadFromFile("assets/sfx/point.wav"), "assets/sfx/point.wav");
    sf::SoundBuffer loseBuf;
    requireLoaded(loseBuf.loadFromFile("assets/sfx/lose.wav"), "assets/sfx/lose.wav");
    sf::Sound pointSound(pointBuf);
    sf::Sound loseSound(loseBuf);

    sf::Music music;
    requireLoaded(music.openFromFile("assets/music/bgm_snake.ogg"), "assets/music/bgm_snake.ogg");
    music.setLooping(true);
    music.setVolume(40.f);

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{960u, 540u}), "SNAKE");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    Game game;
    float titleTimer = 0.f;

    const auto startPlay = [&] {
        game.startGame();
        music.play();
    };

    sf::Clock clock;

    while (window.isOpen()) {
        // 입력: 이벤트 큐에 쌓인 키를 모두 처리하므로 짧게 누른 키도 놓치지 않는다.
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }
            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (!key)
                continue;

            const sf::Keyboard::Scancode code = key->scancode;
            if (code == sf::Keyboard::Scancode::Escape) {
                window.close();
                break;
            }

            switch (game.screen) {
            case Screen::Title:
                if (code == sf::Keyboard::Scancode::Enter)
                    startPlay();
                break;
            case Screen::Play:
                if (const auto d = dirFromKey(code))
                    game.queueDir(*d);
                break;
            case Screen::Result:
                if (code == sf::Keyboard::Scancode::R)
                    startPlay();
                break;
            }
        }
        if (!window.isOpen())
            break;

        // 갱신: 시간 기반이라 프레임레이트와 무관하게 같은 속도로 진행한다.
        const float dt = clock.restart().asSeconds();

        if (game.screen == Screen::Title) {
            titleTimer += dt;
            if (titleTimer >= kTitleSeconds)
                startPlay();
        } else if (game.screen == Screen::Play) {
            game.moveTimer += dt;
            while (game.screen == Screen::Play && game.moveTimer >= game.interval) {
                game.moveTimer -= game.interval;
                switch (game.step()) {
                case StepResult::Moved:
                    break;
                case StepResult::Ate:
                    pointSound.play();
                    break;
                case StepResult::Died:
                    loseSound.play();
                    music.stop();
                    game.screen = Screen::Result;
                    break;
                }
            }
        }

        // 그리기
        window.clear(kBoardBg);
        switch (game.screen) {
        case Screen::Title:
            drawCentered(window, font, "SNAKE", 48, 200.f, kTextColor);
            drawCentered(window, font, "PRESS ENTER", 32, 300.f, kTextColor);
            break;
        case Screen::Play:
            drawPlay(window, font, game);
            break;
        case Screen::Result:
            drawCentered(window, font, "GAME OVER", 48, 200.f, kTextColor);
            drawCentered(window, font, "SCORE " + std::to_string(game.score), 32, 280.f, kTextColor);
            drawCentered(window, font, "R: RESTART  ESC: QUIT", 16, 360.f, kTextColor);
            break;
        }
        window.display();
    }

    return 0;
}
