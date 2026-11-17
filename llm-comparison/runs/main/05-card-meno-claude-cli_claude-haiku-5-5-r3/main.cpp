#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <vector>

namespace {

constexpr float kCardWidth = 80.f;
constexpr float kCardHeight = 100.f;
constexpr float kGap = 12.f;
constexpr int kCols = 4;
constexpr int kRows = 4;
constexpr int kCardCount = kCols * kRows;
constexpr int kPairCount = kCardCount / 2;
constexpr double kTitleSeconds = 2.0;
constexpr double kMismatchSeconds = 0.8;
constexpr std::size_t kMaxNameLength = 8;
constexpr std::size_t kRankLimit = 5;

enum class Phase { Title, Play, Result };

struct Card {
    int face = 1; // 1~8: cards.png에서 앞면이 있는 칸 번호
    bool faceUp = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    int tenths = 0; // TIME을 0.1초 단위로 센 값
    int seq = 0;    // 먼저 낸 기록이 위로 오도록 쓰는 번호
};

// 0.1초 단위 정수를 "34.5" 형식의 문자열로 바꿉니다.
std::string formatTime(int tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

// 경과 초를 0.1초 단위 정수로 바꿉니다.
int toTenths(double seconds) {
    return static_cast<int>(seconds * 10.0 + 1e-9);
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::Color::fromRgb(0x1B2A1F)}};

    // 960×540 기준 카메라입니다. 창 크기가 바뀌면 Letterbox로 맞추므로 클릭 판정도 함께 맞습니다.
    const meno::Camera2D camera{.center = {480.f, 270.f}, .size = {960.f, 540.f}};
    game.renderer().setCamera(camera);

    // 에셋 로딩과 실패 확인
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    const auto cards = game.resources().loadTexture("assets/textures/cards.png");
    const auto flipBuffer = game.resources().loadSoundBuffer("assets/sfx/flip.wav");
    const auto pointBuffer = game.resources().loadSoundBuffer("assets/sfx/point.wav");
    std::optional<meno::Music> bgmOpt = meno::Music::openFromFile("assets/music/bgm_card.ogg");

    std::vector<std::string> missing;
    const auto require = [&missing](bool loaded, const char* path) {
        if (!loaded) {
            missing.emplace_back(path);
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(cards != nullptr, "assets/textures/cards.png");
    require(flipBuffer != nullptr, "assets/sfx/flip.wav");
    require(pointBuffer != nullptr, "assets/sfx/point.wav");
    require(bgmOpt.has_value(), "assets/music/bgm_card.ogg");
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing file: " << path << '\n';
        }
        return 1;
    }

    bgmOpt->setLooping(true);
    bgmOpt->setVolume(0.4f);
    meno::Music& bgm = *bgmOpt;
    meno::Sound flipSound{flipBuffer};
    meno::Sound pointSound{pointBuffer};

    // 게임 상태
    std::mt19937 rng{std::random_device{}()};
    std::array<Card, kCardCount> board{};
    std::vector<int> open; // 앞면이고 아직 맞추지 않은 카드(최대 2장)
    Phase phase = Phase::Title;
    double titleTimer = 0.0;
    bool started = false;
    bool finished = false;
    double elapsed = 0.0;
    double mismatchTimer = 0.0; // 0보다 크면 판정 대기 중
    int moves = 0;
    int matchedPairs = 0;
    int finalTenths = 0;
    std::string name;
    bool confirmed = false;
    std::vector<Record> records; // 이번 실행 중의 기록
    int recordSeq = 0;
    int lastSeq = -1;            // 방금 낸 기록의 번호

    // 카드 index의 화면(월드) 사각형입니다.
    const auto cardRect = [](int index) -> meno::Rectf {
        const int col = index % kCols;
        const int row = index / kCols;
        const float boardW = kCols * kCardWidth + (kCols - 1) * kGap;
        const float boardH = kRows * kCardHeight + (kRows - 1) * kGap;
        const float left = (960.f - boardW) / 2.f;
        const float top = (540.f - boardH) / 2.f;
        return meno::Rectf{{left + static_cast<float>(col) * (kCardWidth + kGap),
                            top + static_cast<float>(row) * (kCardHeight + kGap)},
                           {kCardWidth, kCardHeight}};
    };

    const auto shuffleBoard = [&] {
        std::vector<int> faces;
        for (int face = 1; face <= 8; ++face) {
            faces.push_back(face);
            faces.push_back(face);
        }
        std::shuffle(faces.begin(), faces.end(), rng);
        for (int i = 0; i < kCardCount; ++i) {
            board[static_cast<std::size_t>(i)] = Card{faces[static_cast<std::size_t>(i)], false, false};
        }
    };

    const auto resetRound = [&] {
        shuffleBoard();
        open.clear();
        moves = 0;
        matchedPairs = 0;
        elapsed = 0.0;
        started = false;
        finished = false;
        mismatchTimer = 0.0;
        name.clear();
        confirmed = false;
    };

    const auto startPlay = [&] {
        resetRound();
        phase = Phase::Play;
        bgm.play();
    };

    const auto finishRound = [&] {
        finished = true;
        finalTenths = toTenths(elapsed);
        phase = Phase::Result;
        bgm.pause();
    };

    // 가운데 정렬로 글자를 그리는 도우미입니다.
    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text, float y,
                                  unsigned int size, meno::Color color) {
        meno::TextParams params;
        params.characterSize = size;
        params.color = color;
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, meno::Vec2f{480.f, y}, params);
    };

    const meno::Color white = meno::colors::White;
    const meno::Color highlight = meno::Color::fromRgb(0xFFD54A);

    meno::Scene& scene = game.scenes().createScene();

    scene.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (phase == Phase::Title) {
            titleTimer += dt;
            if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleSeconds) {
                titleTimer = 0.0;
                startPlay();
            }
            return;
        }

        if (phase == Phase::Result) {
            if (!confirmed) {
                // 영문자와 숫자만 최대 8자까지 받고, 영문자는 대문자로 저장합니다.
                for (const char c : input.text()) {
                    const auto uc = static_cast<unsigned char>(c);
                    if (name.size() < kMaxNameLength && std::isalnum(uc) != 0) {
                        name.push_back(static_cast<char>(std::toupper(uc)));
                    }
                }
                if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
                    name.pop_back();
                }
                if (input.wasPressed(meno::Key::Enter) && !name.empty()) {
                    records.push_back(Record{name, moves, finalTenths, recordSeq});
                    lastSeq = recordSeq;
                    ++recordSeq;
                    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
                        return std::tie(a.moves, a.tenths, a.seq) < std::tie(b.moves, b.tenths, b.seq);
                    });
                    confirmed = true;
                }
            } else if (input.wasPressed(meno::Key::R)) {
                startPlay();
            }
            return;
        }

        // Play
        if (started && !finished) {
            elapsed += dt;
        }

        // 틀린 짝은 0.8초 뒤에 뒷면으로 돌립니다. 그 동안은 클릭을 무시합니다.
        if (mismatchTimer > 0.0) {
            mismatchTimer -= dt;
            if (mismatchTimer <= 0.0) {
                mismatchTimer = 0.0;
                for (const int i : open) {
                    board[static_cast<std::size_t>(i)].faceUp = false;
                }
                open.clear();
            }
        }

        if (open.size() < 2 && input.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse =
                game.renderer().screenToWorld(meno::Vec2f{input.mousePosition()});
            for (int i = 0; i < kCardCount; ++i) {
                Card& card = board[static_cast<std::size_t>(i)];
                if (card.faceUp || card.matched || !cardRect(i).contains(mouse)) {
                    continue;
                }
                card.faceUp = true;
                open.push_back(i);
                flipSound.play();
                started = true;

                if (open.size() == 2) {
                    ++moves;
                    const Card& a = board[static_cast<std::size_t>(open[0])];
                    const Card& b = board[static_cast<std::size_t>(open[1])];
                    if (a.face == b.face) {
                        board[static_cast<std::size_t>(open[0])].matched = true;
                        board[static_cast<std::size_t>(open[1])].matched = true;
                        open.clear();
                        ++matchedPairs;
                        pointSound.play();
                        if (matchedPairs == kPairCount) {
                            finishRound();
                            return;
                        }
                    } else {
                        mismatchTimer = kMismatchSeconds;
                    }
                }
                break;
            }
        }
    });

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        if (phase == Phase::Title) {
            drawCentered(renderer, "PAIRS", 200.f, 48, white);
            drawCentered(renderer, "PRESS ENTER", 300.f, 32, white);
            return;
        }

        if (phase == Phase::Play) {
            drawCentered(renderer,
                         "MOVES " + std::to_string(moves) + "   TIME " +
                             formatTime(toTenths(elapsed)),
                         28.f, 32, white);
            for (int i = 0; i < kCardCount; ++i) {
                const Card& card = board[static_cast<std::size_t>(i)];
                const bool showFace = card.faceUp || card.matched;
                const int cell = showFace ? card.face : 0;
                const meno::Rectf rect = cardRect(i);
                renderer.draw(*cards, {.position = rect.position,
                                       .source = meno::Recti{cell * 80, 0, 80, 100}});
            }
            return;
        }

        // Result
        drawCentered(renderer, "CLEAR", 60.f, 48, white);
        drawCentered(renderer,
                     "MOVES " + std::to_string(moves) + "   TIME " + formatTime(finalTenths),
                     120.f, 32, white);

        const auto recordLine = [](std::size_t k, const Record& r) {
            return std::to_string(k + 1) + ". " + r.name + "   MOVES " +
                   std::to_string(r.moves) + "   TIME " + formatTime(r.tenths);
        };

        if (!confirmed) {
            drawCentered(renderer, "NAME: " + name, 200.f, 32, white);
        } else {
            std::size_t lastIndex = records.size();
            for (std::size_t k = 0; k < records.size(); ++k) {
                if (records[k].seq == lastSeq) {
                    lastIndex = k;
                }
            }

            const std::size_t shown = std::min(records.size(), kRankLimit);
            for (std::size_t k = 0; k < shown; ++k) {
                const bool isLast = records[k].seq == lastSeq;
                drawCentered(renderer, recordLine(k, records[k]),
                             180.f + static_cast<float>(k) * 40.f, 32,
                             isLast ? highlight : white);
            }

            // 방금 기록이 5위 밖이면 순위표 아래 줄에 따로 표시합니다.
            if (lastIndex != records.size() && lastIndex >= kRankLimit) {
                drawCentered(renderer, recordLine(lastIndex, records[lastIndex]), 390.f, 32,
                             highlight);
            }
        }

        drawCentered(renderer, "R: RESTART  ESC: QUIT", 470.f, 32, white);
    });

    resetRound();
    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}
