```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr float kWidth = 960.f;
constexpr float kHeight = 540.f;
constexpr int kCardW = 80;
constexpr int kCardH = 100;
constexpr float kGap = 12.f;
constexpr int kRows = 4;
constexpr int kCols = 4;
constexpr int kCardCount = kRows * kCols;
constexpr float kBoardW = kCols * kCardW + (kCols - 1) * kGap;
constexpr float kBoardH = kRows * kCardH + (kRows - 1) * kGap;
constexpr float kBoardX = (kWidth - kBoardW) / 2.f;
constexpr float kBoardY = (kHeight - kBoardH) / 2.f;
constexpr double kMismatchDelay = 0.8;
constexpr double kTitleDelay = 2.0;

const meno::Color kBackground = meno::Color::fromRgb(0x1B2A1F);
const meno::Color kText = meno::Color::fromRgb(0xFFFFFF);
const meno::Color kHighlight = meno::Color::fromRgb(0xFFD54A);

struct Card {
    int value = 1; // 1~8: 앞면 칸 번호
    bool faceUp = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    long long tenths = 0;
    int seq = 0;
};

std::string formatTenths(long long tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

meno::Rectf cardRect(int index) {
    const int row = index / kCols;
    const int col = index % kCols;
    return meno::Rectf{{kBoardX + static_cast<float>(col) * (kCardW + kGap),
                        kBoardY + static_cast<float>(row) * (kCardH + kGap)},
                       {static_cast<float>(kCardW), static_cast<float>(kCardH)}};
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::colors::Black}};

    // 에셋 읽기
    bool failed = false;
    const auto fail = [&](const char* path) {
        std::cerr << "Failed to load: " << path << '\n';
        failed = true;
    };
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    if (!font) {
        fail("assets/fonts/game.ttf");
    }
    const auto cardsTex = game.resources().loadTexture("assets/textures/cards.png");
    if (!cardsTex) {
        fail("assets/textures/cards.png");
    }
    const auto flipBuffer = game.resources().loadSoundBuffer("assets/sfx/flip.wav");
    if (!flipBuffer) {
        fail("assets/sfx/flip.wav");
    }
    const auto pointBuffer = game.resources().loadSoundBuffer("assets/sfx/point.wav");
    if (!pointBuffer) {
        fail("assets/sfx/point.wav");
    }
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_card.ogg");
    if (!bgm) {
        fail("assets/music/bgm_card.ogg");
    }
    if (failed) {
        return 1;
    }
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    std::vector<meno::Sound> flipSounds;
    for (int i = 0; i < 4; ++i) {
        flipSounds.emplace_back(flipBuffer);
    }
    std::size_t nextFlip = 0;
    std::vector<meno::Sound> pointSounds;
    for (int i = 0; i < 2; ++i) {
        pointSounds.emplace_back(pointBuffer);
    }
    std::size_t nextPoint = 0;
    const auto playFlip = [&] {
        flipSounds[nextFlip].play();
        nextFlip = (nextFlip + 1) % flipSounds.size();
    };
    const auto playPoint = [&] {
        pointSounds[nextPoint].play();
        nextPoint = (nextPoint + 1) % pointSounds.size();
    };

    // 16:9 레터박스 카메라
    const meno::Camera2D camera{.center = {kWidth / 2.f, kHeight / 2.f}, .size = {kWidth, kHeight}};
    game.renderer().setCamera(camera);

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    double titleTimer = 0.0;
    std::array<Card, kCardCount> cards{};
    int moves = 0;
    double elapsed = 0.0;
    bool timerStarted = false;
    int matchedPairs = 0;
    int firstPick = -1;
    int secondPick = -1;
    double pendingTimer = 0.0;
    bool pending = false;

    // 결과/기록 상태 (기록은 실행 중 유지)
    std::vector<Record> records;
    int nextSeq = 0;
    int lastSeq = -1;
    std::string name;
    bool confirmed = false;
    int finalMoves = 0;
    long long finalTenths = 0;

    const auto currentTenths = [&] { return static_cast<long long>(elapsed * 10.0 + 1e-6); };

    const auto drawBackground = [](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {kWidth, kHeight}}, {.fill = kBackground});
    };
    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text, float y,
                                  unsigned int size, meno::Color color) {
        meno::TextParams params{.characterSize = size, .color = color};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {kWidth / 2.f, y}, params);
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---------- 타이틀 ----------
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleDelay) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "PAIRS", 220.f, 48, kText);
        drawCentered(renderer, "PRESS ENTER", 310.f, 32, kText);
    });

    // ---------- 플레이 ----------
    play.setEnterHandler([&] {
        std::array<int, kCardCount> values{};
        for (int i = 0; i < kCardCount; ++i) {
            values[static_cast<std::size_t>(i)] = i / 2 + 1;
        }
        std::shuffle(values.begin(), values.end(), rng);
        for (int i = 0; i < kCardCount; ++i) {
            cards[static_cast<std::size_t>(i)] = Card{values[static_cast<std::size_t>(i)], false, false};
        }
        moves = 0;
        elapsed = 0.0;
        timerStarted = false;
        matchedPairs = 0;
        firstPick = -1;
        secondPick = -1;
        pending = false;
        pendingTimer = 0.0;
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (timerStarted && matchedPairs < kCardCount / 2) {
            elapsed += dt;
        }

        if (pending) {
            pendingTimer -= dt;
            if (pendingTimer <= 0.0) {
                cards[static_cast<std::size_t>(firstPick)].faceUp = false;
                cards[static_cast<std::size_t>(secondPick)].faceUp = false;
                firstPick = -1;
                secondPick = -1;
                pending = false;
            }
        }

        if (!pending && input.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse{input.mousePosition()};
            const meno::Vec2f point = game.renderer().screenToWorld(mouse);
            int hit = -1;
            for (int i = 0; i < kCardCount; ++i) {
                if (cardRect(i).contains(point)) {
                    hit = i;
                    break;
                }
            }
            if (hit >= 0) {
                Card& card = cards[static_cast<std::size_t>(hit)];
                if (!card.faceUp && !card.matched) {
                    card.faceUp = true;
                    playFlip();
                    timerStarted = true;
                    if (firstPick < 0) {
                        firstPick = hit;
                    } else {
                        secondPick = hit;
                        ++moves;
                        Card& a = cards[static_cast<std::size_t>(firstPick)];
                        Card& b = cards[static_cast<std::size_t>(secondPick)];
                        if (a.value == b.value) {
                            a.matched = true;
                            b.matched = true;
                            ++matchedPairs;
                            playPoint();
                            firstPick = -1;
                            secondPick = -1;
                            if (matchedPairs == kCardCount / 2) {
                                finalMoves = moves;
                                finalTenths = currentTenths();
                                (void)scenes.changeScene(result.id());
                            }
                        } else {
                            pending = true;
                            pendingTimer = kMismatchDelay;
                        }
                    }
                }
            }
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        const long long tenths = matchedPairs == kCardCount / 2 ? finalTenths : currentTenths();
        drawCentered(renderer, "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(tenths),
                     kBoardY / 2.f, 32, kText);
        for (int i = 0; i < kCardCount; ++i) {
            const Card& card = cards[static_cast<std::size_t>(i)];
            const int cell = (card.faceUp || card.matched) ? card.value : 0;
            renderer.draw(*cardsTex, {.position = cardRect(i).position,
                                      .source = meno::Recti{cell * kCardW, 0, kCardW, kCardH}});
        }
    });

    // ---------- 결과 ----------
    result.setEnterHandler([&] {
        name.clear();
        confirmed = false;
        lastSeq = -1;
    });
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (!confirmed) {
            for (const char ch : input.text()) {
                const auto uc = static_cast<unsigned char>(ch);
                if (uc < 128 && std::isalnum(uc) && name.size() < 8) {
                    name.push_back(static_cast<char>(std::toupper(uc)));
                }
            }
            if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
                name.pop_back();
            }
            if (input.wasPressed(meno::Key::Enter) && !name.empty()) {
                Record rec{name, finalMoves, finalTenths, nextSeq++};
                lastSeq = rec.seq;
                records.push_back(rec);
                std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
                    if (a.moves != b.moves) {
                        return a.moves < b.moves;
                    }
                    if (a.tenths != b.tenths) {
                        return a.tenths < b.tenths;
                    }
                    return a.seq < b.seq;
                });
                confirmed = true;
            }
        } else if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "CLEAR", 56.f, 48, kText);
        drawCentered(renderer,
                     "MOVES " + std::to_string(finalMoves) + "   TIME " + formatTenths(finalTenths),
                     116.f, 32, kText);
        if (!confirmed) {
            drawCentered(renderer, "NAME: " + name, 260.f, 32, kText);
        } else {
            const auto line = [](std::size_t rank, const Record& r) {
                return std::to_string(rank) + ". " + r.name + "   MOVES " + std::to_string(r.moves) +
                       "   TIME " + formatTenths(r.tenths);
            };
            float y = 176.f;
            const std::size_t shown = std::min<std::size_t>(records.size(), 5);
            for (std::size_t i = 0; i < shown; ++i) {
                const Record& r = records[i];
                drawCentered(renderer, line(i + 1, r), y, 32,
                             r.seq == lastSeq ? kHighlight : kText);
                y += 44.f;
            }
            for (std::size_t i = 5; i < records.size(); ++i) {
                if (records[i].seq == lastSeq) {
                    drawCentered(renderer, line(i + 1, records[i]), y, 32, kHighlight);
                    break;
                }
            }
        }
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 490.f, 32, kText);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```