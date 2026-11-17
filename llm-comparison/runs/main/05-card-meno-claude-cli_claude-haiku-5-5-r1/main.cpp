#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr std::size_t boardCols = 4;
constexpr std::size_t cardCount = 16;
constexpr float cardWidth = 80.f;
constexpr float cardHeight = 100.f;
constexpr float cardGap = 12.f;
constexpr float boardLeft = (960.f - (4.f * cardWidth + 3.f * cardGap)) / 2.f;
constexpr float boardTop = 52.f;
constexpr double titleDuration = 2.0;
constexpr double mismatchDelay = 0.8;
constexpr std::size_t maxNameLength = 8;
constexpr std::size_t leaderboardSize = 5;

struct Card {
    int face = 0;
    bool faceUp = false;
    bool matched = false;
};

struct Record {
    int seq = 0;
    std::string name;
    int moves = 0;
    int tenths = 0;
};

std::string formatTime(int tenths) {
    return std::format("{}.{}", tenths / 10, tenths % 10);
}

std::string recordLine(std::size_t rank, const Record& rec) {
    return std::format("{}. {}   MOVES {}   TIME {}", rank, rec.name, rec.moves,
                       formatTime(rec.tenths));
}

meno::Rectf cardRect(std::size_t index) {
    const float col = static_cast<float>(index % boardCols);
    const float row = static_cast<float>(index / boardCols);
    return meno::Rectf{{boardLeft + col * (cardWidth + cardGap), boardTop + row * (cardHeight + cardGap)},
                       {cardWidth, cardHeight}};
}

bool isAsciiAlnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

char toUpperAscii(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

}  // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::Color::fromRgb(0x1B2A1F)}};
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
            std::cerr << "Missing file: " << path << '\n';
        }
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);
    meno::Sound flipSound{flipBuffer};
    meno::Sound pointSound{pointBuffer};

    game.renderer().setCamera(meno::Camera2D{.center = {480.f, 270.f}, .size = {960.f, 540.f}});

    // 게임 상태
    std::mt19937 rng{std::random_device{}()};
    std::array<Card, cardCount> deck{};
    std::vector<std::size_t> flipped;
    int moves = 0;
    int tenths = 0;
    double tenthAccumulator = 0.0;
    bool timerStarted = false;
    std::size_t matchedCount = 0;
    double mismatchTimer = 0.0;
    double titleTimer = 0.0;
    int finalMoves = 0;
    int finalTenths = 0;

    // 결과 화면 상태와 이번 실행의 순위표
    std::vector<Record> board;
    int nextSeq = 0;
    int highlightSeq = -1;
    std::string nameInput;
    bool confirmed = false;

    const auto resetGame = [&] {
        std::vector<int> faces;
        for (int face = 1; face <= 8; ++face) {
            faces.push_back(face);
            faces.push_back(face);
        }
        std::shuffle(faces.begin(), faces.end(), rng);
        for (std::size_t i = 0; i < cardCount; ++i) {
            deck[i] = Card{faces[i], false, false};
        }
        flipped.clear();
        moves = 0;
        tenths = 0;
        tenthAccumulator = 0.0;
        timerStarted = false;
        matchedCount = 0;
        mismatchTimer = 0.0;
    };
    resetGame();

    const auto quitIfEscape = [&] {
        if (game.window().input().wasPressed(meno::Key::Escape)) {
            game.stop();
        }
    };
    const auto printCentered = [&](meno::Renderer& renderer, const std::string& text, float y,
                                   int size, meno::Color color) {
        meno::TextParams params{};
        params.characterSize = static_cast<decltype(params.characterSize)>(size);
        params.color = color;
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {480.f, y}, params);
    };

    meno::Scene& title = game.scenes().createScene();
    meno::Scene& play = game.scenes().createScene();
    meno::Scene& result = game.scenes().createScene();

    // 타이틀: Enter를 누르거나 2초가 지나면 플레이를 시작합니다.
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        quitIfEscape();
        const meno::Input& input = game.window().input();
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= titleDuration) {
            (void)game.scenes().changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        printCentered(renderer, "PAIRS", 220.f, 48, meno::colors::White);
        printCentered(renderer, "PRESS ENTER", 300.f, 32, meno::colors::White);
    });

    // 플레이: 들어올 때마다 상태를 초기화하고 BGM을 반복 재생합니다.
    play.setEnterHandler([&] {
        resetGame();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double dt) {
        quitIfEscape();
        const meno::Input& input = game.window().input();

        // 틀린 짝을 기다리는 중이면 0.8초 뒤 뒷면으로 돌립니다.
        if (mismatchTimer > 0.0) {
            mismatchTimer -= dt;
            if (mismatchTimer <= 0.0) {
                for (const std::size_t i : flipped) {
                    deck[i].faceUp = false;
                }
                flipped.clear();
                mismatchTimer = 0.0;
            }
        }

        // 첫 카드를 뒤집은 뒤부터 0.1초 단위로 시간을 셉니다.
        if (timerStarted && matchedCount < cardCount) {
            tenthAccumulator += dt;
            while (tenthAccumulator >= 0.1) {
                tenthAccumulator -= 0.1;
                ++tenths;
            }
        }

        if (mismatchTimer <= 0.0 && input.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse{input.mousePosition()};
            const meno::Vec2f point = game.renderer().screenToWorld(mouse);
            for (std::size_t i = 0; i < cardCount; ++i) {
                if (!cardRect(i).contains(point)) {
                    continue;
                }
                Card& card = deck[i];
                if (card.faceUp) {
                    break;
                }
                card.faceUp = true;
                flipped.push_back(i);
                flipSound.play();
                timerStarted = true;

                if (flipped.size() == 2) {
                    ++moves;
                    const std::size_t first = flipped[0];
                    const std::size_t second = flipped[1];
                    if (deck[first].face == deck[second].face) {
                        deck[first].matched = true;
                        deck[second].matched = true;
                        flipped.clear();
                        matchedCount += 2;
                        pointSound.play();
                        if (matchedCount == cardCount) {
                            finalMoves = moves;
                            finalTenths = tenths;
                            (void)game.scenes().changeScene(result.id());
                        }
                    } else {
                        mismatchTimer = mismatchDelay;
                    }
                }
                break;
            }
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        printCentered(renderer, std::format("MOVES {}   TIME {}", moves, formatTime(tenths)),
                      28.f, 32, meno::colors::White);
        for (std::size_t i = 0; i < cardCount; ++i) {
            const Card& card = deck[i];
            const int cell = card.faceUp ? card.face : 0;
            renderer.draw(*cards, {.position = cardRect(i).position,
                                   .source = meno::Recti{cell * 80, 0, 80, 100}});
        }
    });

    // 결과: 이름을 입력해 확정하면 순위표에 기록하고, 확정 뒤 R로 다시 시작합니다.
    result.setEnterHandler([&] {
        bgm->stop();
        nameInput.clear();
        confirmed = false;
        highlightSeq = -1;
    });
    result.setUpdateHandler([&](double) {
        quitIfEscape();
        const meno::Input& input = game.window().input();

        if (!confirmed) {
            for (const char c : input.text()) {
                if (nameInput.size() < maxNameLength && isAsciiAlnum(c)) {
                    nameInput.push_back(toUpperAscii(c));
                }
            }
            if (input.wasPressed(meno::Key::Backspace) && !nameInput.empty()) {
                nameInput.pop_back();
            }
            if (input.wasPressed(meno::Key::Enter) && !nameInput.empty()) {
                const int seq = nextSeq++;
                board.push_back(Record{seq, nameInput, finalMoves, finalTenths});
                std::stable_sort(board.begin(), board.end(),
                                 [](const Record& a, const Record& b) {
                                     if (a.moves != b.moves) {
                                         return a.moves < b.moves;
                                     }
                                     return a.tenths < b.tenths;
                                 });
                highlightSeq = seq;
                confirmed = true;
            }
        } else if (input.wasPressed(meno::Key::R)) {
            (void)game.scenes().changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        const meno::Color white = meno::colors::White;
        const meno::Color highlight = meno::Color::fromRgb(0xFFD54A);

        printCentered(renderer, "CLEAR", 40.f, 48, white);
        printCentered(renderer, std::format("MOVES {}   TIME {}", finalMoves, formatTime(finalTenths)),
                      110.f, 32, white);

        if (!confirmed) {
            printCentered(renderer, "NAME: " + nameInput, 190.f, 32, white);
        } else {
            const std::size_t shown = std::min(board.size(), leaderboardSize);
            for (std::size_t i = 0; i < shown; ++i) {
                const Record& rec = board[i];
                const meno::Color color = rec.seq == highlightSeq ? highlight : white;
                printCentered(renderer, recordLine(i + 1, rec),
                              200.f + static_cast<float>(i) * 44.f, 32, color);
            }
            const auto it = std::find_if(board.begin(), board.end(), [&](const Record& rec) {
                return rec.seq == highlightSeq;
            });
            if (it != board.end()) {
                const auto index = static_cast<std::size_t>(it - board.begin());
                if (index >= leaderboardSize) {
                    printCentered(renderer, recordLine(index + 1, *it),
                                  200.f + static_cast<float>(leaderboardSize) * 44.f + 20.f, 32,
                                  highlight);
                }
            }
        }

        printCentered(renderer, "R: RESTART  ESC: QUIT", 490.f, 32, white);
    });

    (void)game.scenes().changeScene(title.id());
    game.run();
    return 0;
}
