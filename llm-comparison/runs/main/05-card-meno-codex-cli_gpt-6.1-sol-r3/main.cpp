#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {

enum class Screen { Title, Play, Result };

struct Card {
    int face = 0;
    bool revealed = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    int tenths = 0;
    std::size_t id = 0;
};

constexpr float Width = 960.f;
constexpr float Height = 540.f;
constexpr float CardWidth = 80.f;
constexpr float CardHeight = 100.f;
constexpr float Gap = 12.f;
constexpr float BoardX = 302.f;
constexpr float BoardY = 52.f;

meno::Rectf cardBounds(int index) {
    return {{BoardX + static_cast<float>(index % 4) * (CardWidth + Gap),
             BoardY + static_cast<float>(index / 4) * (CardHeight + Gap)},
            {CardWidth, CardHeight}};
}

std::string timeText(int tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string scoreText(int moves, int tenths) {
    return "MOVES " + std::to_string(moves) + "   TIME " + timeText(tenths);
}

std::string recordText(std::size_t rank, const Record& record) {
    return std::to_string(rank) + ". " + record.name + "   " +
           scoreText(record.moves, record.tenths);
}

int runGame() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::colors::Black}};

    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    const auto texture = game.resources().loadTexture("assets/textures/cards.png");
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
    require(static_cast<bool>(texture), "assets/textures/cards.png");
    require(static_cast<bool>(flipBuffer), "assets/sfx/flip.wav");
    require(static_cast<bool>(pointBuffer), "assets/sfx/point.wav");
    require(static_cast<bool>(music), "assets/music/bgm_card.ogg");
    if (missing) {
        return 1;
    }

    music->setVolume(0.4f);
    music->setLooping(true);

    std::vector<meno::Sound> flips;
    std::vector<meno::Sound> points;
    for (int i = 0; i < 4; ++i) {
        flips.emplace_back(flipBuffer);
        points.emplace_back(pointBuffer);
    }
    std::size_t nextFlip = 0;
    std::size_t nextPoint = 0;

    const meno::Camera2D camera{
        .center = {Width / 2.f, Height / 2.f},
        .size = {Width, Height}
    };
    game.renderer().setCamera(camera);

    const meno::Color background = meno::Color::fromRgb(0x1B2A1F);
    const meno::Color highlight = meno::Color::fromRgb(0xFFD54A);

    Screen screen = Screen::Title;
    std::array<Card, 16> cards{};
    std::mt19937 random{std::random_device{}()};
    double titleTime = 0.0;
    double elapsed = 0.0;
    double mismatchRemaining = 0.0;
    bool timerStarted = false;
    int first = -1;
    int second = -1;
    int moves = 0;
    int pairs = 0;
    int finalTenths = 0;
    std::string name;
    bool confirmed = false;

    std::vector<Record> records;
    std::size_t nextRecordId = 0;
    std::size_t currentRecordId = 0;

    const auto displayedTenths = [&]() {
        return static_cast<int>(std::floor(elapsed * 10.0 + 0.0000001));
    };

    const auto startPlay = [&]() {
        std::array<int, 16> faces{};
        for (int i = 0; i < 16; ++i) {
            faces[static_cast<std::size_t>(i)] = i / 2 + 1;
        }
        std::shuffle(faces.begin(), faces.end(), random);
        for (std::size_t i = 0; i < cards.size(); ++i) {
            cards[i] = Card{faces[i], false, false};
        }

        titleTime = 0.0;
        elapsed = 0.0;
        mismatchRemaining = 0.0;
        timerStarted = false;
        first = -1;
        second = -1;
        moves = 0;
        pairs = 0;
        finalTenths = 0;
        name.clear();
        confirmed = false;
        nextFlip = 0;
        nextPoint = 0;
        for (auto& sound : flips) {
            sound.stop();
        }
        for (auto& sound : points) {
            sound.stop();
        }
        screen = Screen::Play;
        music->stop();
        music->play();
    };

    const auto finishPlay = [&]() {
        finalTenths = displayedTenths();
        timerStarted = false;
        music->stop();
        name.clear();
        confirmed = false;
        screen = Screen::Result;
    };

    const auto submitRecord = [&]() {
        currentRecordId = nextRecordId++;
        records.push_back({name, moves, finalTenths, currentRecordId});
        std::stable_sort(records.begin(), records.end(),
                         [](const Record& a, const Record& b) {
                             if (a.moves != b.moves) {
                                 return a.moves < b.moves;
                             }
                             return a.tenths < b.tenths;
                         });
        confirmed = true;
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
            if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
                startPlay();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (confirmed) {
                if (input.wasPressed(meno::Key::R)) {
                    startPlay();
                }
                return;
            }

            for (unsigned char ch : input.text()) {
                if (name.size() >= 8) {
                    break;
                }
                if (ch >= 'a' && ch <= 'z') {
                    ch = static_cast<unsigned char>(ch - 'a' + 'A');
                }
                if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                    name.push_back(static_cast<char>(ch));
                }
            }

            if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
                name.pop_back();
            }
            if (input.wasPressed(meno::Key::Enter) && !name.empty()) {
                submitRecord();
            }
            return;
        }

        if (timerStarted) {
            elapsed += dt;
        }

        if (mismatchRemaining > 0.0) {
            mismatchRemaining -= dt;
            if (mismatchRemaining <= 0.0000001) {
                cards[static_cast<std::size_t>(first)].revealed = false;
                cards[static_cast<std::size_t>(second)].revealed = false;
                first = -1;
                second = -1;
                mismatchRemaining = 0.0;
            }
            // The input received during the waiting tick is ignored.
            return;
        }

        if (!input.wasPressed(meno::MouseButton::Left)) {
            return;
        }

        const meno::Vec2f mouse{input.mousePosition()};
        if (!meno::viewportPixels(camera, game.renderer().framebufferSize())
                 .contains(mouse)) {
            return;
        }
        const meno::Vec2f position =
            meno::screenToWorld(camera, game.renderer().framebufferSize(), mouse);

        for (int i = 0; i < 16; ++i) {
            Card& card = cards[static_cast<std::size_t>(i)];
            if (!cardBounds(i).contains(position) || card.revealed || card.matched) {
                continue;
            }

            card.revealed = true;
            flips[nextFlip].play();
            nextFlip = (nextFlip + 1) % flips.size();
            timerStarted = true;

            if (first < 0) {
                first = i;
                return;
            }

            second = i;
            ++moves;
            Card& previous = cards[static_cast<std::size_t>(first)];
            if (previous.face == card.face) {
                previous.matched = true;
                card.matched = true;
                ++pairs;
                points[nextPoint].play();
                nextPoint = (nextPoint + 1) % points.size();
                first = -1;
                second = -1;
                if (pairs == 8) {
                    finishPlay();
                }
            } else {
                mismatchRemaining = 0.8;
            }
            return;
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);
        renderer.drawRect({{0.f, 0.f}, {Width, Height}}, {.fill = background});

        const auto centered = [&](const std::string& text, float y,
                                  unsigned int size,
                                  meno::Color color = meno::colors::White) {
            meno::TextParams params{.characterSize = size, .color = color};
            params.origin = renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {Width / 2.f, y}, params);
        };

        if (screen == Screen::Title) {
            centered("PAIRS", 220.f, 48);
            centered("PRESS ENTER", 310.f, 32);
            return;
        }

        if (screen == Screen::Play) {
            centered(scoreText(moves, displayedTenths()), 24.f, 32);
            for (int i = 0; i < 16; ++i) {
                const Card& card = cards[static_cast<std::size_t>(i)];
                const int frame = card.revealed || card.matched ? card.face : 0;
                renderer.draw(*texture,
                              {.position = cardBounds(i).position,
                               .source = meno::Recti{frame * 80, 0, 80, 100}});
            }
            return;
        }

        centered("CLEAR", 52.f, 48);
        centered(scoreText(moves, finalTenths), 115.f, 32);
        centered("NAME: " + name, 174.f, 32);

        if (confirmed) {
            const std::size_t count = std::min<std::size_t>(5, records.size());
            std::size_t currentRank = 0;
            for (std::size_t i = 0; i < records.size(); ++i) {
                if (records[i].id == currentRecordId) {
                    currentRank = i;
                    break;
                }
            }

            for (std::size_t i = 0; i < count; ++i) {
                centered(recordText(i + 1, records[i]),
                         232.f + static_cast<float>(i) * 32.f, 16,
                         records[i].id == currentRecordId
                             ? highlight
                             : meno::colors::White);
            }
            if (currentRank >= 5) {
                centered(recordText(currentRank + 1, records[currentRank]),
                         408.f, 16, highlight);
            }
        }

        centered("R: RESTART  ESC: QUIT", 490.f, 32);
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    music->stop();
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
