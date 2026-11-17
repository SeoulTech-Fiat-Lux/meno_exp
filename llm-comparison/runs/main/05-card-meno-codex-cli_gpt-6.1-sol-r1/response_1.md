```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr float Width = 960.f;
constexpr float Height = 540.f;
constexpr float CardWidth = 80.f;
constexpr float CardHeight = 100.f;
constexpr float Gap = 12.f;
constexpr float BoardX = 302.f;
constexpr float BoardY = 52.f;

enum class Screen { Title, Play, Result };

struct Record {
    std::string name;
    int moves;
    int tenths;
    std::uint64_t id;
};

std::string timeText(int tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string scoreText(int moves, int tenths) {
    return "MOVES " + std::to_string(moves) + "   TIME " + timeText(tenths);
}

meno::Rectf cardRect(int index) {
    return {{BoardX + static_cast<float>(index % 4) * (CardWidth + Gap),
             BoardY + static_cast<float>(index / 4) * (CardHeight + Gap)},
            {CardWidth, CardHeight}};
}
} // namespace

int main() {
    meno::Game game{
        {.application = {.framerateLimit = 60},
         .windowSize = {960, 540},
         .title = "PAIRS",
         .clearColor = meno::colors::Black}};

    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    const auto cards = game.resources().loadTexture("assets/textures/cards.png");
    const auto flipBuffer = game.resources().loadSoundBuffer("assets/sfx/flip.wav");
    const auto pointBuffer = game.resources().loadSoundBuffer("assets/sfx/point.wav");
    auto music = meno::Music::openFromFile("assets/music/bgm_card.ogg");

    bool missing = false;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            std::cerr << "Failed to load: " << path << '\n';
            missing = true;
        }
    };
    require(static_cast<bool>(font), "assets/fonts/game.ttf");
    require(static_cast<bool>(cards), "assets/textures/cards.png");
    require(static_cast<bool>(flipBuffer), "assets/sfx/flip.wav");
    require(static_cast<bool>(pointBuffer), "assets/sfx/point.wav");
    require(static_cast<bool>(music), "assets/music/bgm_card.ogg");
    if (missing) {
        return 1;
    }

    std::vector<meno::Sound> flips;
    for (int i = 0; i < 4; ++i) {
        flips.emplace_back(flipBuffer);
    }
    std::size_t nextFlip = 0;
    meno::Sound point{pointBuffer};
    music->setVolume(0.4f);
    music->setLooping(true);

    const meno::Camera2D camera{
        .center = {Width / 2.f, Height / 2.f},
        .size = {Width, Height}};
    game.renderer().setCamera(camera);

    const auto background = meno::Color::fromRgb(0x1B2A1F);
    const auto highlight = meno::Color::fromRgb(0xFFD54A);

    Screen screen = Screen::Title;
    double titleTime = 0.0;
    std::array<int, 16> deck{};
    std::array<bool, 16> matched{};
    std::mt19937 random{std::random_device{}()};
    int first = -1;
    int second = -1;
    int moves = 0;
    int pairs = 0;
    double elapsed = 0.0;
    double mismatchLeft = 0.0;
    bool timerStarted = false;
    int finalTenths = 0;
    std::string name;
    bool nameConfirmed = false;
    std::vector<Record> records;
    std::uint64_t nextRecordId = 1;
    std::uint64_t currentRecordId = 0;

    const auto displayedTenths = [&]() {
        return static_cast<int>(std::floor(elapsed * 10.0 + 1e-7));
    };

    const auto startGame = [&]() {
        music->stop();
        point.stop();
        for (auto& sound : flips) {
            sound.stop();
        }
        nextFlip = 0;
        for (int i = 0; i < 16; ++i) {
            deck[static_cast<std::size_t>(i)] = i / 2 + 1;
        }
        std::shuffle(deck.begin(), deck.end(), random);
        matched.fill(false);
        first = -1;
        second = -1;
        moves = 0;
        pairs = 0;
        elapsed = 0.0;
        mismatchLeft = 0.0;
        timerStarted = false;
        finalTenths = 0;
        titleTime = 0.0;
        name.clear();
        nameConfirmed = false;
        currentRecordId = 0;
        screen = Screen::Play;
        music->play();
    };

    const auto centeredText = [&](meno::Renderer& renderer,
                                  const std::string& text,
                                  float y,
                                  unsigned int size,
                                  meno::Color color) {
        meno::TextParams params{.characterSize = size, .color = color};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {Width / 2.f, y}, params);
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
            if (input.wasPressed(meno::Key::Enter) || titleTime + 1e-7 >= 2.0) {
                startGame();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (nameConfirmed) {
                if (input.wasPressed(meno::Key::R)) {
                    startGame();
                }
                return;
            }

            for (unsigned char c : input.text()) {
                if (name.size() >= 8) {
                    break;
                }
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<unsigned char>(c - 'a' + 'A');
                }
                if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                    name.push_back(static_cast<char>(c));
                }
            }

            if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
                name.pop_back();
            }

            if (input.wasPressed(meno::Key::Enter) && !name.empty()) {
                currentRecordId = nextRecordId++;
                records.push_back({name, moves, finalTenths, currentRecordId});
                std::stable_sort(records.begin(), records.end(),
                                 [](const Record& a, const Record& b) {
                                     if (a.moves != b.moves) {
                                         return a.moves < b.moves;
                                     }
                                     return a.tenths < b.tenths;
                                 });
                nameConfirmed = true;
            }
            return;
        }

        if (timerStarted) {
            elapsed += dt;
        }

        if (mismatchLeft > 0.0) {
            mismatchLeft -= dt;
            if (mismatchLeft <= 1e-7) {
                mismatchLeft = 0.0;
                first = -1;
                second = -1;
            }
            return;
        }

        if (!input.wasPressed(meno::MouseButton::Left)) {
            return;
        }

        const meno::Vec2f mouse{input.mousePosition()};
        if (!meno::viewportPixels(camera, game.renderer().framebufferSize()).contains(mouse)) {
            return;
        }
        const meno::Vec2f position = meno::screenToWorld(
            camera, game.renderer().framebufferSize(), mouse);

        for (int i = 0; i < 16; ++i) {
            if (!cardRect(i).contains(position)) {
                continue;
            }
            if (matched[static_cast<std::size_t>(i)] || i == first || i == second) {
                return;
            }

            timerStarted = true;
            flips[nextFlip].play();
            nextFlip = (nextFlip + 1) % flips.size();

            if (first < 0) {
                first = i;
                return;
            }

            second = i;
            ++moves;
            if (deck[static_cast<std::size_t>(first)] ==
                deck[static_cast<std::size_t>(second)]) {
                matched[static_cast<std::size_t>(first)] = true;
                matched[static_cast<std::size_t>(second)] = true;
                first = -1;
                second = -1;
                ++pairs;
                point.play();

                if (pairs == 8) {
                    finalTenths = displayedTenths();
                    timerStarted = false;
                    music->stop();
                    screen = Screen::Result;
                }
            } else {
                mismatchLeft = 0.8;
            }
            return;
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);
        renderer.drawRect({{0.f, 0.f}, {Width, Height}}, {.fill = background});

        if (screen == Screen::Title) {
            centeredText(renderer, "PAIRS", 220.f, 48, meno::colors::White);
            centeredText(renderer, "PRESS ENTER", 310.f, 32, meno::colors::White);
            return;
        }

        if (screen == Screen::Play) {
            centeredText(renderer, scoreText(moves, displayedTenths()),
                         24.f, 32, meno::colors::White);

            for (int i = 0; i < 16; ++i) {
                const bool faceUp =
                    matched[static_cast<std::size_t>(i)] || i == first || i == second;
                const int frame = faceUp ? deck[static_cast<std::size_t>(i)] : 0;
                renderer.draw(
                    *cards,
                    {.position = cardRect(i).position,
                     .source = meno::Recti{frame * 80, 0, 80, 100}});
            }
            return;
        }

        centeredText(renderer, "CLEAR", 55.f, 48, meno::colors::White);
        centeredText(renderer, scoreText(moves, finalTenths),
                     116.f, 32, meno::colors::White);
        centeredText(renderer, "NAME: " + name, 182.f, 32, meno::colors::White);

        if (nameConfirmed) {
            const auto drawRecord = [&](std::size_t index, float y) {
                const Record& record = records[index];
                const std::string line =
                    std::to_string(index + 1) + ". " + record.name + "   " +
                    scoreText(record.moves, record.tenths);
                centeredText(renderer, line, y, 16,
                             record.id == currentRecordId
                                 ? highlight
                                 : meno::colors::White);
            };

            const std::size_t count = std::min<std::size_t>(5, records.size());
            for (std::size_t i = 0; i < count; ++i) {
                drawRecord(i, 254.f + static_cast<float>(i) * 28.f);
            }

            for (std::size_t i = 5; i < records.size(); ++i) {
                if (records[i].id == currentRecordId) {
                    drawRecord(i, 418.f);
                    break;
                }
            }
        }

        centeredText(renderer, "R: RESTART  ESC: QUIT",
                     500.f, 16, meno::colors::White);
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}
```