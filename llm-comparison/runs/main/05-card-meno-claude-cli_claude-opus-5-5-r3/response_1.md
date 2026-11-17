```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr float kCardW = 80.f;
constexpr float kCardH = 100.f;
constexpr float kGap = 12.f;
constexpr int kRows = 4;
constexpr int kCols = 4;
constexpr int kCardCount = kRows * kCols;
constexpr int kPairs = kCardCount / 2;
constexpr double kTitleTime = 2.0;
constexpr double kMismatchDelay = 0.8;
constexpr std::size_t kMaxName = 8;

const meno::Color kBackground = meno::Color::fromRgb(0x1B2A1F);
const meno::Color kText = meno::Color::fromRgb(0xFFFFFF);
const meno::Color kHighlight = meno::Color::fromRgb(0xFFD54A);

enum class CardState { Back, Up, Matched };

struct Card {
    int face = 1;
    CardState state = CardState::Back;
};

struct Record {
    std::string name;
    int moves = 0;
    long long tenths = 0;
    long long seq = 0;
};

std::string formatTenths(long long tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

meno::Rectf cardRect(int index) {
    const float boardW = kCols * kCardW + (kCols - 1) * kGap;
    const float boardH = kRows * kCardH + (kRows - 1) * kGap;
    const float x0 = (kWidth - boardW) / 2.f;
    const float y0 = (kHeight - boardH) / 2.f;
    const int row = index / kCols;
    const int col = index % kCols;
    return meno::Rectf{{x0 + static_cast<float>(col) * (kCardW + kGap),
                        y0 + static_cast<float>(row) * (kCardH + kGap)},
                       {kCardW, kCardH}};
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::colors::Black}};

    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto cardsTexture = resources.loadTexture("assets/textures/cards.png");
    const auto flipBuffer = resources.loadSoundBuffer("assets/sfx/flip.wav");
    const auto pointBuffer = resources.loadSoundBuffer("assets/sfx/point.wav");
    std::optional<meno::Music> bgm = meno::Music::openFromFile("assets/music/bgm_card.ogg");

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, "assets/fonts/game.ttf");
    require(cardsTexture != nullptr, "assets/textures/cards.png");
    require(flipBuffer != nullptr, "assets/sfx/flip.wav");
    require(pointBuffer != nullptr, "assets/sfx/point.wav");
    require(bgm.has_value(), "assets/music/bgm_card.ogg");
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

    // 16:9 고정 카메라 (레터박스)
    const meno::Camera2D camera{.center = {kWidth / 2.f, kHeight / 2.f}, .size = {kWidth, kHeight}};
    game.renderer().setCamera(camera);

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    std::array<Card, kCardCount> cards{};
    int firstPick = -1;
    int secondPick = -1;
    double mismatchTimer = 0.0;
    int moves = 0;
    int matchedPairs = 0;
    bool timerStarted = false;
    bool finished = false;
    double elapsed = 0.0;
    long long finalTenths = 0;
    int finalMoves = 0;

    // 결과/기록 상태
    std::string name;
    bool confirmed = false;
    std::vector<Record> records;
    long long recordSeq = 0;
    long long currentSeq = -1;

    double titleTimer = 0.0;

    const auto currentTenths = [&]() -> long long {
        return static_cast<long long>(std::floor(elapsed * 10.0 + 1e-9));
    };

    const auto resetGame = [&] {
        std::vector<int> faces;
        for (int f = 1; f <= kPairs; ++f) {
            faces.push_back(f);
            faces.push_back(f);
        }
        std::shuffle(faces.begin(), faces.end(), rng);
        for (int i = 0; i < kCardCount; ++i) {
            cards[static_cast<std::size_t>(i)] = Card{faces[static_cast<std::size_t>(i)], CardState::Back};
        }
        firstPick = -1;
        secondPick = -1;
        mismatchTimer = 0.0;
        moves = 0;
        matchedPairs = 0;
        timerStarted = false;
        finished = false;
        elapsed = 0.0;
        finalTenths = 0;
        finalMoves = 0;
    };

    const auto drawCentered = [&](meno::Renderer& renderer, const std::string& text, float y,
                                  unsigned int size, meno::Color color) {
        meno::TextParams params{.characterSize = size, .color = color};
        params.origin = renderer.measureTextBounds(*font, text, params).center();
        renderer.drawText(*font, text, {kWidth / 2.f, y}, params);
    };
    const auto drawBackground = [&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {kWidth, kHeight}}, {.fill = kBackground});
    };
    const auto checkQuit = [&] {
        if (game.window().input().wasPressed(meno::Key::Escape)) {
            game.stop();
            return true;
        }
        return false;
    };

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // ---------- 타이틀 ----------
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        if (checkQuit()) {
            return;
        }
        titleTimer += dt;
        if (game.window().input().wasPressed(meno::Key::Enter) || titleTimer >= kTitleTime) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "PAIRS", 220.f, 48, kText);
        drawCentered(renderer, "PRESS ENTER", 300.f, 32, kText);
    });

    // ---------- 플레이 ----------
    play.setEnterHandler([&] {
        resetGame();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double dt) {
        if (checkQuit()) {
            return;
        }
        if (finished) {
            return;
        }
        const meno::Input& input = game.window().input();

        if (timerStarted) {
            elapsed += dt;
        }

        // 불일치 대기
        if (mismatchTimer > 0.0) {
            mismatchTimer -= dt;
            if (mismatchTimer <= 0.0) {
                mismatchTimer = 0.0;
                cards[static_cast<std::size_t>(firstPick)].state = CardState::Back;
                cards[static_cast<std::size_t>(secondPick)].state = CardState::Back;
                firstPick = -1;
                secondPick = -1;
            }
            return; // 대기 중(및 해제되는 틱)의 클릭은 무시
        }

        if (!input.wasPressed(meno::MouseButton::Left)) {
            return;
        }
        const meno::Vec2f mouse{input.mousePosition()};
        const meno::Vec2f point =
            meno::screenToWorld(camera, game.renderer().framebufferSize(), mouse);

        int hit = -1;
        for (int i = 0; i < kCardCount; ++i) {
            if (cardRect(i).contains(point)) {
                hit = i;
                break;
            }
        }
        if (hit < 0) {
            return;
        }
        Card& card = cards[static_cast<std::size_t>(hit)];
        if (card.state != CardState::Back) {
            return;
        }

        card.state = CardState::Up;
        flipSounds[nextFlip].play();
        nextFlip = (nextFlip + 1) % flipSounds.size();
        if (!timerStarted) {
            timerStarted = true;
            elapsed = 0.0;
        }

        if (firstPick < 0) {
            firstPick = hit;
            return;
        }
        secondPick = hit;
        ++moves;
        Card& a = cards[static_cast<std::size_t>(firstPick)];
        Card& b = cards[static_cast<std::size_t>(secondPick)];
        if (a.face == b.face) {
            a.state = CardState::Matched;
            b.state = CardState::Matched;
            pointSounds[nextPoint].play();
            nextPoint = (nextPoint + 1) % pointSounds.size();
            firstPick = -1;
            secondPick = -1;
            ++matchedPairs;
            if (matchedPairs >= kPairs) {
                finished = true;
                finalTenths = currentTenths();
                finalMoves = moves;
                (void)scenes.changeScene(result.id());
            }
        } else {
            mismatchTimer = kMismatchDelay;
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        for (int i = 0; i < kCardCount; ++i) {
            const Card& card = cards[static_cast<std::size_t>(i)];
            const int cell = card.state == CardState::Back ? 0 : card.face;
            const meno::Rectf rect = cardRect(i);
            renderer.draw(*cardsTexture,
                          {.position = rect.position,
                           .source = meno::Recti{cell * 80, 0, 80, 100}});
        }
        const long long tenths = finished ? finalTenths : currentTenths();
        drawCentered(renderer, "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(tenths),
                     26.f, 32, kText);
    });

    // ---------- 결과 ----------
    result.setEnterHandler([&] {
        name.clear();
        confirmed = false;
        currentSeq = -1;
    });
    result.setUpdateHandler([&](double) {
        if (checkQuit()) {
            return;
        }
        const meno::Input& input = game.window().input();
        if (!confirmed) {
            for (const char ch : input.text()) {
                const auto u = static_cast<unsigned char>(ch);
                if (u >= 128) {
                    continue;
                }
                char c = ch;
                if (c >= 'a' && c <= 'z') {
                    c = static_cast<char>(c - 'a' + 'A');
                }
                const bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
                if (ok && name.size() < kMaxName) {
                    name.push_back(c);
                }
            }
            if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
                name.pop_back();
            }
            if (input.wasPressed(meno::Key::Enter) && !name.empty()) {
                confirmed = true;
                currentSeq = recordSeq++;
                records.push_back(Record{name, finalMoves, finalTenths, currentSeq});
                std::stable_sort(records.begin(), records.end(),
                                 [](const Record& x, const Record& y) {
                                     if (x.moves != y.moves) {
                                         return x.moves < y.moves;
                                     }
                                     if (x.tenths != y.tenths) {
                                         return x.tenths < y.tenths;
                                     }
                                     return x.seq < y.seq;
                                 });
            }
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "CLEAR", 60.f, 48, kText);
        drawCentered(renderer,
                     "MOVES " + std::to_string(finalMoves) + "   TIME " + formatTenths(finalTenths),
                     120.f, 32, kText);

        if (!confirmed) {
            drawCentered(renderer, "NAME: " + name, 260.f, 32, kText);
        } else {
            const auto line = [](std::size_t rank, const Record& r) {
                return std::to_string(rank) + ". " + r.name + "   MOVES " + std::to_string(r.moves) +
                       "   TIME " + formatTenths(r.tenths);
            };
            float y = 180.f;
            std::size_t currentRank = 0;
            for (std::size_t i = 0; i < records.size(); ++i) {
                if (records[i].seq == currentSeq) {
                    currentRank = i + 1;
                }
            }
            const std::size_t shown = std::min<std::size_t>(records.size(), 5);
            for (std::size_t i = 0; i < shown; ++i) {
                const bool isCurrent = records[i].seq == currentSeq;
                drawCentered(renderer, line(i + 1, records[i]), y, 32, isCurrent ? kHighlight : kText);
                y += 40.f;
            }
            if (currentRank > 5) {
                drawCentered(renderer, line(currentRank, records[currentRank - 1]), y, 32,
                             kHighlight);
            }
        }
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 490.f, 32, kText);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
```