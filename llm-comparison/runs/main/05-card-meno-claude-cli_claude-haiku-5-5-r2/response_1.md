```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kCols = 4;
constexpr int kRows = 4;
constexpr int kCardCount = kCols * kRows;
constexpr int kPairCount = 8;
constexpr float kCardW = 80.f;
constexpr float kCardH = 100.f;
constexpr float kGap = 12.f;
constexpr float kBoardLeft = (960.f - (kCols * kCardW + (kCols - 1) * kGap)) / 2.f;
constexpr float kBoardTop = 64.f;
constexpr double kHideDelay = 0.8;
constexpr double kTitleSeconds = 2.0;
constexpr std::size_t kMaxName = 8;
constexpr std::size_t kTopCount = 5;

struct Card {
    int face = 0;
    bool up = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    int tenths = 0;
    int seq = 0;
};

meno::Rectf cardRect(int index) {
    const float x = kBoardLeft + static_cast<float>(index % kCols) * (kCardW + kGap);
    const float y = kBoardTop + static_cast<float>(index / kCols) * (kCardH + kGap);
    return meno::Rectf{{x, y}, {kCardW, kCardH}};
}

std::string formatTime(int tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string formatRecord(std::size_t rank, const Record& record) {
    return std::to_string(rank) + ". " + record.name + "   MOVES " + std::to_string(record.moves) +
           "   TIME " + formatTime(record.tenths);
}

void drawCenteredText(meno::Renderer& renderer, const meno::Font& font, const std::string& text,
                      float centerY, int size, meno::Color color = meno::colors::White) {
    meno::TextParams params{};
    params.characterSize = size;
    params.color = color;
    params.origin = renderer.measureTextBounds(font, text, params).center();
    renderer.drawText(font, text, {480.f, centerY}, params);
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::Color::fromRgb(0x1B2A1F)}};
    game.renderer().setCamera(meno::Camera2D{.center = {480.f, 270.f}, .size = {960.f, 540.f}});

    const meno::Color highlight = meno::Color::fromRgb(0xFFD54A);

    // 에셋 로딩 및 실패 확인
    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto cards = resources.loadTexture("assets/textures/cards.png");
    const auto flipBuffer = resources.loadSoundBuffer("assets/sfx/flip.wav");
    const auto pointBuffer = resources.loadSoundBuffer("assets/sfx/point.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_card.ogg");

    std::vector<std::string> missing;
    if (!font) {
        missing.emplace_back("assets/fonts/game.ttf");
    }
    if (!cards) {
        missing.emplace_back("assets/textures/cards.png");
    }
    if (!flipBuffer) {
        missing.emplace_back("assets/sfx/flip.wav");
    }
    if (!pointBuffer) {
        missing.emplace_back("assets/sfx/point.wav");
    }
    if (!bgm) {
        missing.emplace_back("assets/music/bgm_card.ogg");
    }
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing asset: " << path << '\n';
        }
        return 1;
    }

    const meno::Font& fontRef = *font;
    const meno::Texture& cardTexture = *cards;
    meno::Sound flipSound{flipBuffer};
    meno::Sound pointSound{pointBuffer};
    bgm->setLooping(true);
    bgm->setVolume(0.4f);

    // 게임 상태
    std::mt19937 rng{std::random_device{}()};
    std::vector<Card> board(kCardCount);
    int moves = 0;
    double elapsed = 0.0;
    bool started = false;
    bool finished = false;
    int matchedPairs = 0;
    int openIndex = -1;
    int hideA = -1;
    int hideB = -1;
    double hideTimer = 0.0;
    double titleTimer = 0.0;

    std::vector<Record> records;
    int nextSeq = 0;
    int highlightSeq = -1;
    std::string nameInput;
    bool confirmed = false;
    int resultMoves = 0;
    int resultTenths = 0;

    const auto tenthsNow = [&] { return static_cast<int>(elapsed * 10.0); };

    const auto resetBoard = [&] {
        std::vector<int> faces;
        for (int face = 1; face <= kPairCount; ++face) {
            faces.push_back(face);
            faces.push_back(face);
        }
        std::shuffle(faces.begin(), faces.end(), rng);
        for (int i = 0; i < kCardCount; ++i) {
            board[static_cast<std::size_t>(i)] = Card{faces[static_cast<std::size_t>(i)], false, false};
        }
        moves = 0;
        elapsed = 0.0;
        started = false;
        finished = false;
        matchedPairs = 0;
        openIndex = -1;
        hideA = -1;
        hideB = -1;
        hideTimer = 0.0;
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
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleSeconds) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawCenteredText(renderer, fontRef, "PAIRS", 200.f, 48);
        drawCenteredText(renderer, fontRef, "PRESS ENTER", 300.f, 32);
    });

    // 플레이
    play.setEnterHandler([&] {
        resetBoard();
        bgm->play();
    });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (started && !finished) {
            elapsed += dt;
        }

        // 틀린 짝을 0.8초 뒤에 뒤집습니다.
        if (hideTimer > 0.0) {
            hideTimer -= dt;
            if (hideTimer <= 0.0) {
                hideTimer = 0.0;
                board[static_cast<std::size_t>(hideA)].up = false;
                board[static_cast<std::size_t>(hideB)].up = false;
                hideA = -1;
                hideB = -1;
            }
        }

        if (input.wasPressed(meno::MouseButton::Left) && hideTimer <= 0.0) {
            const meno::Vec2f mouse{input.mousePosition()};
            const meno::Vec2f point = game.renderer().screenToWorld(mouse);
            for (int i = 0; i < kCardCount; ++i) {
                Card& card = board[static_cast<std::size_t>(i)];
                if (card.up || card.matched) {
                    continue;
                }
                if (!cardRect(i).contains(point)) {
                    continue;
                }

                started = true;
                card.up = true;
                flipSound.play();

                if (openIndex < 0) {
                    openIndex = i;
                } else {
                    ++moves;
                    const int first = openIndex;
                    openIndex = -1;
                    Card& firstCard = board[static_cast<std::size_t>(first)];
                    if (firstCard.face == card.face) {
                        firstCard.matched = true;
                        card.matched = true;
                        ++matchedPairs;
                        pointSound.play();
                        if (matchedPairs == kPairCount) {
                            finished = true;
                            resultMoves = moves;
                            resultTenths = tenthsNow();
                            (void)scenes.changeScene(result.id());
                        }
                    } else {
                        hideA = first;
                        hideB = i;
                        hideTimer = kHideDelay;
                    }
                }
                break;
            }
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        const std::string hud =
            "MOVES " + std::to_string(moves) + "   TIME " + formatTime(tenthsNow());
        drawCenteredText(renderer, fontRef, hud, 32.f, 32);

        for (int i = 0; i < kCardCount; ++i) {
            const Card& card = board[static_cast<std::size_t>(i)];
            const int cell = (card.up || card.matched) ? card.face : 0;
            renderer.draw(cardTexture, {.position = cardRect(i).position,
                                        .source = meno::Recti{cell * 80, 0, 80, 100}});
        }
    });
    play.setExitHandler([&] {});

    // 결과
    result.setEnterHandler([&] {
        bgm->stop();
        nameInput.clear();
        confirmed = false;
        highlightSeq = -1;
    });
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (!confirmed) {
            const std::string typed = input.text();
            for (const char c : typed) {
                const auto uc = static_cast<unsigned char>(c);
                if (std::isalnum(uc) && nameInput.size() < kMaxName) {
                    nameInput.push_back(static_cast<char>(std::toupper(uc)));
                }
            }
            if (input.wasPressed(meno::Key::Backspace) && !nameInput.empty()) {
                nameInput.pop_back();
            }
            if (input.wasPressed(meno::Key::Enter) && !nameInput.empty()) {
                const int seq = nextSeq++;
                records.push_back(Record{nameInput, resultMoves, resultTenths, seq});
                std::stable_sort(records.begin(), records.end(),
                                 [](const Record& a, const Record& b) {
                                     return a.moves != b.moves ? a.moves < b.moves
                                                               : a.tenths < b.tenths;
                                 });
                highlightSeq = seq;
                confirmed = true;
            }
        } else if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawCenteredText(renderer, fontRef, "CLEAR", 56.f, 48);
        drawCenteredText(renderer, fontRef,
                         "MOVES " + std::to_string(resultMoves) + "   TIME " +
                             formatTime(resultTenths),
                         120.f, 32);
        drawCenteredText(renderer, fontRef, "NAME: " + nameInput, 180.f, 32);

        if (confirmed) {
            const std::size_t shown = std::min(records.size(), kTopCount);
            for (std::size_t i = 0; i < shown; ++i) {
                const bool fresh = records[i].seq == highlightSeq;
                drawCenteredText(renderer, fontRef, formatRecord(i + 1, records[i]),
                                 240.f + 40.f * static_cast<float>(i), 32,
                                 fresh ? highlight : meno::colors::White);
            }
            for (std::size_t i = kTopCount; i < records.size(); ++i) {
                if (records[i].seq == highlightSeq) {
                    drawCenteredText(renderer, fontRef, formatRecord(i + 1, records[i]), 460.f,
                                     32, highlight);
                }
            }
        }

        drawCenteredText(renderer, fontRef, "R: RESTART  ESC: QUIT", 510.f, 16);
    });

    (void)scenes.changeScene(title.id());
    game.run();
}
```