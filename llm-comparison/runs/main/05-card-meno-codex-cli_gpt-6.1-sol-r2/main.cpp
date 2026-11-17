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

enum class Screen { Title, Play, Result };

struct Card {
    int face = 0;
    bool revealed = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    long long tenths = 0;
    std::uint64_t id = 0;
};

std::string timeText(long long tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

meno::Rectf cardBounds(std::size_t index) {
    return {{302.f + static_cast<float>(index % 4) * 92.f,
             52.f + static_cast<float>(index / 4) * 112.f},
            {80.f, 100.f}};
}

} // namespace

int main() {
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
    require(music.has_value(), "assets/music/bgm_card.ogg");
    if (missing) {
        return 1;
    }

    std::vector<meno::Sound> flips;
    std::vector<meno::Sound> points;
    flips.reserve(4);
    points.reserve(4);
    for (int i = 0; i < 4; ++i) {
        flips.emplace_back(flipBuffer);
        points.emplace_back(pointBuffer);
    }
    std::size_t nextFlip = 0;
    std::size_t nextPoint = 0;

    music->setVolume(0.4f);
    music->setLooping(true);

    const meno::Camera2D camera{.center = {480.f, 270.f},
                                .size = {960.f, 540.f}};
    game.renderer().setCamera(camera);

    const meno::Color background = meno::Color::fromRgb(0x1B2A1F);
    const meno::Color highlight = meno::Color::fromRgb(0xFFD54A);

    Screen screen = Screen::Title;
    std::array<Card, 16> cards{};
    std::mt19937 random{std::random_device{}()};
    double titleTime = 0.0;
    double elapsed = 0.0;
    double mismatchLeft = 0.0;
    bool clockRunning = false;
    int first = -1;
    int second = -1;
    int moves = 0;
    int pairs = 0;

    std::string name;
    bool nameConfirmed = false;
    std::vector<Record> records;
    std::uint64_t nextRecordId = 1;
    std::uint64_t currentRecordId = 0;
    long long finalTenths = 0;

    const auto elapsedTenths = [&]() {
        return static_cast<long long>(std::floor(elapsed * 10.0 + 1e-8));
    };

    const auto startGame = [&]() {
        for (std::size_t i = 0; i < cards.size(); ++i) {
            cards[i] = Card{static_cast<int>(i / 2) + 1, false, false};
        }
        std::shuffle(cards.begin(), cards.end(), random);
        elapsed = 0.0;
        mismatchLeft = 0.0;
        clockRunning = false;
        first = -1;
        second = -1;
        moves = 0;
        pairs = 0;
        name.clear();
        nameConfirmed = false;
        currentRecordId = 0;
        finalTenths = 0;
        titleTime = 0.0;
        nextFlip = 0;
        nextPoint = 0;
        for (auto& sound : flips) {
            sound.stop();
        }
        for (auto& sound : points) {
            sound.stop();
        }
        music->stop();
        music->play();
        screen = Screen::Play;
    };

    const auto finishGame = [&]() {
        clockRunning = false;
        finalTenths = elapsedTenths();
        name.clear();
        nameConfirmed = false;
        music->stop();
        screen = Screen::Result;
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
                startGame();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (!nameConfirmed) {
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
            } else if (input.wasPressed(meno::Key::R)) {
                startGame();
            }
            return;
        }

        if (clockRunning) {
            elapsed += dt;
        }

        if (mismatchLeft > 0.0) {
            mismatchLeft -= dt;
            if (mismatchLeft > 1e-8) {
                return;
            }
            mismatchLeft = 0.0;
            cards[static_cast<std::size_t>(first)].revealed = false;
            cards[static_cast<std::size_t>(second)].revealed = false;
            first = -1;
            second = -1;
        }

        if (!input.wasPressed(meno::MouseButton::Left)) {
            return;
        }

        const meno::Vec2f mouse{input.mousePosition()};
        if (!meno::viewportPixels(camera, game.renderer().framebufferSize()).contains(mouse)) {
            return;
        }

        const meno::Vec2f point =
            meno::screenToWorld(camera, game.renderer().framebufferSize(), mouse);

        for (std::size_t i = 0; i < cards.size(); ++i) {
            Card& card = cards[i];
            if (!cardBounds(i).contains(point) || card.revealed || card.matched) {
                continue;
            }

            card.revealed = true;
            flips[nextFlip].play();
            nextFlip = (nextFlip + 1) % flips.size();
            clockRunning = true;

            if (first < 0) {
                first = static_cast<int>(i);
            } else {
                second = static_cast<int>(i);
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
                        finishGame();
                    }
                } else {
                    mismatchLeft = 0.8;
                }
            }
            break;
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(camera);
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}}, {.fill = background});

        const auto centered = [&](const std::string& text, float y, unsigned int size,
                                  meno::Color color = meno::colors::White) {
            meno::TextParams params{.characterSize = size, .color = color};
            params.origin = renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {480.f, y}, params);
        };

        if (screen == Screen::Title) {
            centered("PAIRS", 220.f, 48);
            centered("PRESS ENTER", 310.f, 32);
            return;
        }

        if (screen == Screen::Play) {
            centered("MOVES " + std::to_string(moves) + "   TIME " +
                         timeText(elapsedTenths()),
                     25.f, 32);

            for (std::size_t i = 0; i < cards.size(); ++i) {
                const Card& card = cards[i];
                const int frame = card.revealed || card.matched ? card.face : 0;
                renderer.draw(*texture,
                              {.position = cardBounds(i).position,
                               .source = meno::Recti{frame * 80, 0, 80, 100}});
            }
            return;
        }

        centered("CLEAR", 48.f, 48);
        centered("MOVES " + std::to_string(moves) + "   TIME " + timeText(finalTenths),
                 108.f, 32);

        if (!nameConfirmed) {
            centered("NAME: " + name, 220.f, 32);
        } else {
            const auto recordText = [](std::size_t rank, const Record& record) {
                return std::to_string(rank) + ". " + record.name + "   MOVES " +
                       std::to_string(record.moves) + "   TIME " +
                       timeText(record.tenths);
            };

            const std::size_t count = std::min<std::size_t>(5, records.size());
            for (std::size_t i = 0; i < count; ++i) {
                const Record& record = records[i];
                centered(recordText(i + 1, record), 174.f + static_cast<float>(i) * 44.f,
                         32, record.id == currentRecordId ? highlight : meno::colors::White);
            }

            for (std::size_t i = 5; i < records.size(); ++i) {
                if (records[i].id == currentRecordId) {
                    centered(recordText(i + 1, records[i]), 410.f, 32, highlight);
                    break;
                }
            }
        }

        centered("R: RESTART  ESC: QUIT", 500.f, 32);
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
    music->stop();
    return 0;
}
