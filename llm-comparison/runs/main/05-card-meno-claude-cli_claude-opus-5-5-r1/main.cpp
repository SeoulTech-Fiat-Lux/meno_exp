#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cctype>
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
constexpr float kBoardW = kCols * kCardW + (kCols - 1) * kGap;
constexpr float kBoardH = kRows * kCardH + (kRows - 1) * kGap;
constexpr float kBoardX = (kWidth - kBoardW) / 2.f;
constexpr float kBoardY = (kHeight - kBoardH) / 2.f;
constexpr double kTitleTime = 2.0;
constexpr double kMismatchWait = 0.8;
constexpr std::size_t kMaxName = 8;

struct Card {
    int face = 1; // 1~8
    bool faceUp = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    int tenths = 0;
    int seq = 0;
};

std::string formatTenths(int tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string scoreText(int moves, int tenths) {
    return "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(tenths);
}

meno::Rectf cardRect(int index) {
    const int row = index / kCols;
    const int col = index % kCols;
    return meno::Rectf{{kBoardX + static_cast<float>(col) * (kCardW + kGap),
                        kBoardY + static_cast<float>(row) * (kCardH + kGap)},
                       {kCardW, kCardH}};
}

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "PAIRS",
                     .clearColor = meno::colors::Black}};

    const char* fontPath = "assets/fonts/game.ttf";
    const char* cardsPath = "assets/textures/cards.png";
    const char* flipPath = "assets/sfx/flip.wav";
    const char* pointPath = "assets/sfx/point.wav";
    const char* musicPath = "assets/music/bgm_card.ogg";

    meno::ResourceManager& resources = game.resources();
    const auto font = resources.loadFont(fontPath);
    const auto cardsTex = resources.loadTexture(cardsPath);
    const auto flipBuffer = resources.loadSoundBuffer(flipPath);
    const auto pointBuffer = resources.loadSoundBuffer(pointPath);
    std::optional<meno::Music> music = meno::Music::openFromFile(musicPath);

    bool failed = false;
    const auto require = [&](bool ok, const char* path) {
        if (!ok) {
            std::cerr << "Failed to load: " << path << '\n';
            failed = true;
        }
    };
    require(font != nullptr, fontPath);
    require(cardsTex != nullptr, cardsPath);
    require(flipBuffer != nullptr, flipPath);
    require(pointBuffer != nullptr, pointPath);
    require(music.has_value(), musicPath);
    if (failed) {
        return 1;
    }

    music->setLooping(true);
    music->setVolume(0.4f);

    std::vector<meno::Sound> flipSounds;
    std::vector<meno::Sound> pointSounds;
    for (int i = 0; i < 4; ++i) {
        flipSounds.emplace_back(flipBuffer);
        pointSounds.emplace_back(pointBuffer);
    }
    std::size_t nextFlip = 0;
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

    const meno::Color bgColor = meno::Color::fromRgb(0x1B2A1F);
    const meno::Color textColor = meno::Color::fromRgb(0xFFFFFF);
    const meno::Color highlight = meno::Color::fromRgb(0xFFD54A);

    std::mt19937 rng{std::random_device{}()};

    // 게임 상태
    std::array<Card, kCardCount> cards{};
    int firstIdx = -1;
    int secondIdx = -1;
    double waitLeft = 0.0;
    int moves = 0;
    int matchedPairs = 0;
    bool timerStarted = false;
    bool finished = false;
    double elapsed = 0.0;
    int finalMoves = 0;
    int finalTenths = 0;

    // 결과/기록 상태 (기록은 실행 중 유지)
    std::vector<Record> records;
    int nextSeq = 0;
    std::string name;
    bool confirmed = false;
    int currentSeq = -1;

    double titleTimer = 0.0;

    const auto tenthsOf = [](double t) {
        return static_cast<int>(std::floor(t * 10.0 + 1e-9));
    };

    const auto drawBackground = [&](meno::Renderer& renderer) {
        renderer.drawRect({{0.f, 0.f}, {kWidth, kHeight}}, {.fill = bgColor});
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
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleTime) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "PAIRS", 220.f, 48, textColor);
        drawCentered(renderer, "PRESS ENTER", 320.f, 32, textColor);
    });

    // ---------- 플레이 ----------
    play.setEnterHandler([&] {
        std::array<int, kCardCount> faces{};
        for (int i = 0; i < kCardCount; ++i) {
            faces[static_cast<std::size_t>(i)] = i / 2 + 1;
        }
        std::shuffle(faces.begin(), faces.end(), rng);
        for (int i = 0; i < kCardCount; ++i) {
            cards[static_cast<std::size_t>(i)] = Card{faces[static_cast<std::size_t>(i)], false, false};
        }
        firstIdx = -1;
        secondIdx = -1;
        waitLeft = 0.0;
        moves = 0;
        matchedPairs = 0;
        timerStarted = false;
        finished = false;
        elapsed = 0.0;
        music->play();
    });
    play.setExitHandler([&] { music->stop(); });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        if (finished) {
            return;
        }

        if (timerStarted) {
            elapsed += dt;
        }

        // 불일치 대기
        if (waitLeft > 0.0) {
            waitLeft -= dt;
            if (waitLeft <= 0.0) {
                waitLeft = 0.0;
                if (firstIdx >= 0) {
                    cards[static_cast<std::size_t>(firstIdx)].faceUp = false;
                }
                if (secondIdx >= 0) {
                    cards[static_cast<std::size_t>(secondIdx)].faceUp = false;
                }
                firstIdx = -1;
                secondIdx = -1;
            }
            return; // 대기 중 클릭은 무시
        }

        if (!input.wasPressed(meno::MouseButton::Left)) {
            return;
        }
        const meno::Vec2f mouse{input.mousePosition()};
        const meno::Vec2f point = game.renderer().screenToWorld(mouse);
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
        if (card.faceUp || card.matched) {
            return;
        }
        card.faceUp = true;
        playFlip();
        timerStarted = true;

        if (firstIdx < 0) {
            firstIdx = hit;
            return;
        }
        secondIdx = hit;
        ++moves;
        Card& a = cards[static_cast<std::size_t>(firstIdx)];
        Card& b = cards[static_cast<std::size_t>(secondIdx)];
        if (a.face == b.face) {
            a.matched = true;
            b.matched = true;
            playPoint();
            ++matchedPairs;
            firstIdx = -1;
            secondIdx = -1;
            if (matchedPairs >= kPairs) {
                finished = true;
                finalMoves = moves;
                finalTenths = tenthsOf(elapsed);
                (void)scenes.changeScene(result.id());
            }
        } else {
            waitLeft = kMismatchWait;
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        const int shownTenths = finished ? finalTenths : tenthsOf(elapsed);
        drawCentered(renderer, scoreText(moves, shownTenths), 26.f, 32, textColor);
        for (int i = 0; i < kCardCount; ++i) {
            const Card& card = cards[static_cast<std::size_t>(i)];
            const int cell = (card.faceUp || card.matched) ? card.face : 0;
            const meno::Rectf rect = cardRect(i);
            renderer.draw(*cardsTex,
                          {.position = rect.position,
                           .source = meno::Recti{cell * 80, 0, 80, 100}});
        }
    });

    // ---------- 결과 ----------
    result.setEnterHandler([&] {
        name.clear();
        confirmed = false;
        currentSeq = -1;
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
                if (uc >= 128 || !std::isalnum(uc)) {
                    continue;
                }
                if (name.size() < kMaxName) {
                    name.push_back(static_cast<char>(std::toupper(uc)));
                }
            }
            if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
                name.pop_back();
            }
            if (input.wasPressed(meno::Key::Enter) && !name.empty()) {
                currentSeq = nextSeq++;
                records.push_back(Record{name, finalMoves, finalTenths, currentSeq});
                std::sort(records.begin(), records.end(), [](const Record& l, const Record& r) {
                    if (l.moves != r.moves) {
                        return l.moves < r.moves;
                    }
                    if (l.tenths != r.tenths) {
                        return l.tenths < r.tenths;
                    }
                    return l.seq < r.seq;
                });
                confirmed = true;
            }
            return;
        }
        if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        drawBackground(renderer);
        drawCentered(renderer, "CLEAR", 70.f, 48, textColor);
        drawCentered(renderer, scoreText(finalMoves, finalTenths), 130.f, 32, textColor);
        if (!confirmed) {
            drawCentered(renderer, "NAME: " + name, 260.f, 32, textColor);
        } else {
            const auto line = [](std::size_t rank, const Record& r) {
                return std::to_string(rank) + ". " + r.name + "   " + scoreText(r.moves, r.tenths);
            };
            float y = 200.f;
            const std::size_t shown = std::min<std::size_t>(records.size(), 5);
            for (std::size_t i = 0; i < shown; ++i) {
                const Record& r = records[i];
                drawCentered(renderer, line(i + 1, r), y, 32,
                             r.seq == currentSeq ? highlight : textColor);
                y += 40.f;
            }
            for (std::size_t i = shown; i < records.size(); ++i) {
                if (records[i].seq == currentSeq) {
                    drawCentered(renderer, line(i + 1, records[i]), y, 32, highlight);
                    break;
                }
            }
        }
        drawCentered(renderer, "R: RESTART  ESC: QUIT", 500.f, 32, textColor);
    });

    (void)scenes.changeScene(title.id());
    game.run();
    return 0;
}
