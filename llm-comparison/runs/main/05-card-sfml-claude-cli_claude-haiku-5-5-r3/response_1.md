```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kCardW = 80.f;
constexpr float kCardH = 100.f;
constexpr float kGap = 12.f;
constexpr int kGrid = 4;
constexpr int kCardCount = kGrid * kGrid;   // 16
constexpr int kPairCount = kCardCount / 2;  // 8
constexpr float kBoardW = kGrid * kCardW + (kGrid - 1) * kGap;
constexpr float kBoardH = kGrid * kCardH + (kGrid - 1) * kGap;
constexpr float kBoardX = (kViewW - kBoardW) / 2.f;
constexpr float kBoardY = (kViewH - kBoardH) / 2.f;
constexpr float kMismatchDelay = 0.8f;
constexpr float kTitleDelay = 2.0f;
constexpr float kBgmVolume = 40.f;
constexpr std::size_t kMaxNameLen = 8;
constexpr std::size_t kTopCount = 5;

const sf::Color kBgColor{27, 42, 31};          // #1B2A1F
const sf::Color kTextColor{255, 255, 255};     // #FFFFFF
const sf::Color kHighlightColor{255, 213, 74}; // #FFD54A

enum class Screen { Title, Play, Result };

struct Card {
    int value = 0;  // 1..8, 같은 값 두 장이 한 쌍
    bool faceUp = false;
    bool matched = false;
};

struct Record {
    std::string name;
    int moves = 0;
    int tenths = 0;  // TIME을 0.1초 단위 정수로 보관
    int seq = 0;     // 먼저 낸 기록이 앞서도록 쓰는 번호
};

struct Game {
    Screen screen = Screen::Title;
    std::array<Card, kCardCount> cards{};
    int firstIdx = -1;
    int secondIdx = -1;
    bool checking = false;
    float mismatchTimer = 0.f;
    int moves = 0;
    int matchedPairs = 0;
    bool timerStarted = false;
    float elapsed = 0.f;
    float titleTimer = 0.f;

    int resultMoves = 0;
    int resultTenths = 0;
    std::string nameBuf;
    bool nameConfirmed = false;
    int recordSeq = -1;
    int lastRank = 0;
    int nextSeq = 0;
    std::vector<Record> records;  // 이번 실행 동안만 유지
};

int tenthsOf(float seconds) {
    return static_cast<int>(std::floor(seconds * 10.f + 1e-3f));
}

std::string timeStr(int tenths) {
    return std::format("{}.{}", tenths / 10, tenths % 10);
}

std::string rankLine(int rank, const Record& r) {
    return std::format("{}. {}   MOVES {}   TIME {}", rank, r.name, r.moves, timeStr(r.tenths));
}

sf::Vector2f cardPos(int idx) {
    const int row = idx / kGrid;
    const int col = idx % kGrid;
    return {kBoardX + col * (kCardW + kGap), kBoardY + row * (kCardH + kGap)};
}

// 창 크기가 바뀌어도 960x540 논리 화면을 16:9 비율로 가운데 맞춘다 (나머지는 검은 띠).
void updateView(sf::RenderWindow& window) {
    const sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0) {
        return;
    }
    const float scale = std::min(size.x / kViewW, size.y / kViewH);
    const float vw = kViewW * scale / size.x;
    const float vh = kViewH * scale / size.y;
    sf::View view(sf::FloatRect({0.f, 0.f}, {kViewW, kViewH}));
    view.setViewport(sf::FloatRect({(1.f - vw) / 2.f, (1.f - vh) / 2.f}, {vw, vh}));
    window.setView(view);
}

void resetPlay(Game& g, std::mt19937& rng) {
    std::array<int, kCardCount> values{};
    for (int i = 0; i < kCardCount; ++i) {
        values[i] = i / 2 + 1;
    }
    std::shuffle(values.begin(), values.end(), rng);
    for (int i = 0; i < kCardCount; ++i) {
        g.cards[i] = Card{values[i]};
    }
    g.firstIdx = -1;
    g.secondIdx = -1;
    g.checking = false;
    g.mismatchTimer = 0.f;
    g.moves = 0;
    g.matchedPairs = 0;
    g.timerStarted = false;
    g.elapsed = 0.f;
}

void drawText(sf::RenderTarget& target, const sf::Font& font, std::string_view str,
              unsigned int size, sf::Vector2f pos, sf::Color color, bool centered = true) {
    sf::Text text(font, sf::String(str), size);
    text.setFillColor(color);
    if (centered) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x / 2.f, 0.f});
    }
    text.setPosition(pos);
    target.draw(text);
}

void drawBoard(sf::RenderTarget& target, const sf::Texture& cardsTex, const Game& g) {
    for (int i = 0; i < kCardCount; ++i) {
        const Card& c = g.cards[i];
        // 0번 칸은 뒷면, 1~8번 칸은 앞면
        const int frame = (c.faceUp || c.matched) ? c.value : 0;
        sf::Sprite sprite(cardsTex);
        sprite.setTextureRect(sf::IntRect({frame * 80, 0}, {80, 100}));
        sprite.setPosition(cardPos(i));
        target.draw(sprite);
    }
}

void render(sf::RenderWindow& window, const Game& g, const sf::Font& font, const sf::Texture& cardsTex) {
    window.clear(sf::Color::Black);

    sf::RectangleShape bg(sf::Vector2f{kViewW, kViewH});
    bg.setFillColor(kBgColor);
    window.draw(bg);

    switch (g.screen) {
    case Screen::Title:
        drawText(window, font, "PAIRS", 48, {kViewW / 2.f, 200.f}, kTextColor);
        drawText(window, font, "PRESS ENTER", 32, {kViewW / 2.f, 300.f}, kTextColor);
        break;

    case Screen::Play:
        drawText(window, font,
                 std::format("MOVES {}   TIME {}", g.moves, timeStr(tenthsOf(g.elapsed))),
                 32, {kViewW / 2.f, 4.f}, kTextColor);
        drawBoard(window, cardsTex, g);
        break;

    case Screen::Result: {
        drawText(window, font, "CLEAR", 48, {480.f, 40.f}, kTextColor);
        drawText(window, font,
                 std::format("MOVES {}   TIME {}", g.resultMoves, timeStr(g.resultTenths)),
                 32, {480.f, 110.f}, kTextColor);
        drawText(window, font, "NAME: " + g.nameBuf, 32, {480.f, 170.f}, kTextColor);

        if (g.nameConfirmed) {
            const std::size_t shown = std::min(kTopCount, g.records.size());
            for (std::size_t i = 0; i < shown; ++i) {
                const Record& r = g.records[i];
                const bool mine = (r.seq == g.recordSeq);
                drawText(window, font, rankLine(static_cast<int>(i) + 1, r), 32,
                         {480.f, 240.f + static_cast<float>(i) * 40.f},
                         mine ? kHighlightColor : kTextColor);
            }
            if (g.lastRank > static_cast<int>(kTopCount)) {
                const Record& r = g.records[static_cast<std::size_t>(g.lastRank - 1)];
                drawText(window, font, rankLine(g.lastRank, r), 32, {480.f, 440.f}, kHighlightColor);
            }
        }

        drawText(window, font, "R: RESTART  ESC: QUIT", 16, {480.f, 500.f}, kTextColor);
        break;
    }
    }
}

bool loadOrReport(bool ok, const char* path) {
    if (!ok) {
        std::cerr << "error: failed to load " << path << '\n';
    }
    return ok;
}

}  // namespace

int main() {
    std::mt19937 rng{std::random_device{}()};

    sf::Font font;
    sf::Texture cardsTex;
    sf::SoundBuffer flipBuf;
    sf::SoundBuffer pointBuf;
    sf::Music bgm;

    if (!loadOrReport(font.openFromFile("assets/fonts/game.ttf"), "assets/fonts/game.ttf")) {
        return 1;
    }
    if (!loadOrReport(cardsTex.loadFromFile("assets/textures/cards.png"), "assets/textures/cards.png")) {
        return 1;
    }
    if (!loadOrReport(flipBuf.loadFromFile("assets/sfx/flip.wav"), "assets/sfx/flip.wav")) {
        return 1;
    }
    if (!loadOrReport(pointBuf.loadFromFile("assets/sfx/point.wav"), "assets/sfx/point.wav")) {
        return 1;
    }
    if (!loadOrReport(bgm.openFromFile("assets/music/bgm_card.ogg"), "assets/music/bgm_card.ogg")) {
        return 1;
    }
    bgm.setLooping(true);
    bgm.setVolume(kBgmVolume);

    sf::Sound flipSound(flipBuf);
    sf::Sound pointSound(pointBuf);

    sf::RenderWindow window(sf::VideoMode({960, 540}), "PAIRS");
    window.setFramerateLimit(60);
    updateView(window);

    Game game;

    auto startPlay = [&]() {
        resetPlay(game, rng);
        game.screen = Screen::Play;
        bgm.play();
    };

    auto finish = [&]() {
        game.resultMoves = game.moves;
        game.resultTenths = tenthsOf(game.elapsed);
        game.screen = Screen::Result;
        game.nameBuf.clear();
        game.nameConfirmed = false;
        game.recordSeq = -1;
        game.lastRank = 0;
        bgm.stop();
    };

    auto flip = [&](int i) {
        Card& card = game.cards[i];
        card.faceUp = true;
        flipSound.play();
        game.timerStarted = true;

        if (game.firstIdx < 0) {
            game.firstIdx = i;
            return;
        }

        game.secondIdx = i;
        ++game.moves;
        const int firstValue = game.cards[game.firstIdx].value;
        if (firstValue == card.value) {
            game.cards[game.firstIdx].matched = true;
            card.matched = true;
            ++game.matchedPairs;
            pointSound.play();
            game.firstIdx = -1;
            game.secondIdx = -1;
            if (game.matchedPairs == kPairCount) {
                finish();
            }
        } else {
            game.checking = true;
            game.mismatchTimer = kMismatchDelay;
        }
    };

    auto onClick = [&](sf::Vector2f p) {
        if (game.screen != Screen::Play || game.checking) {
            return;
        }
        for (int i = 0; i < kCardCount; ++i) {
            const Card& card = game.cards[i];
            if (card.faceUp || card.matched) {
                continue;
            }
            if (sf::FloatRect(cardPos(i), {kCardW, kCardH}).contains(p)) {
                flip(i);
                return;
            }
        }
    };

    auto confirmName = [&]() {
        if (game.nameConfirmed || game.nameBuf.empty()) {
            return;
        }
        game.nameConfirmed = true;
        game.recordSeq = game.nextSeq++;
        game.records.push_back(Record{game.nameBuf, game.resultMoves, game.resultTenths, game.recordSeq});
        std::sort(game.records.begin(), game.records.end(), [](const Record& a, const Record& b) {
            if (a.moves != b.moves) {
                return a.moves < b.moves;
            }
            if (a.tenths != b.tenths) {
                return a.tenths < b.tenths;
            }
            return a.seq < b.seq;
        });
        for (std::size_t i = 0; i < game.records.size(); ++i) {
            if (game.records[i].seq == game.recordSeq) {
                game.lastRank = static_cast<int>(i) + 1;
            }
        }
    };

    sf::Clock clock;
    while (window.isOpen()) {
        const float dt = clock.restart().asSeconds();

        // 입력: 프레임 안에 쌓인 이벤트를 모두 처리한다.
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (event->is<sf::Event::Resized>()) {
                updateView(window);
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using S = sf::Keyboard::Scancode;
                if (key->scancode == S::Escape) {
                    window.close();
                } else if (game.screen == Screen::Title && key->scancode == S::Enter) {
                    startPlay();
                } else if (game.screen == Screen::Result) {
                    if (key->scancode == S::Enter) {
                        confirmName();
                    } else if (key->scancode == S::Backspace && !game.nameConfirmed && !game.nameBuf.empty()) {
                        game.nameBuf.pop_back();
                    } else if (key->scancode == S::R && game.nameConfirmed) {
                        startPlay();
                    }
                }
            } else if (const auto* text = event->getIf<sf::Event::TextEntered>()) {
                if (game.screen == Screen::Result && !game.nameConfirmed) {
                    const std::uint32_t c = text->unicode;
                    const bool digit = (c >= '0' && c <= '9');
                    const bool upper = (c >= 'A' && c <= 'Z');
                    const bool lower = (c >= 'a' && c <= 'z');
                    if ((digit || upper || lower) && game.nameBuf.size() < kMaxNameLen) {
                        char ch = static_cast<char>(c);
                        if (lower) {
                            ch = static_cast<char>(ch - 'a' + 'A');
                        }
                        game.nameBuf.push_back(ch);
                    }
                }
            } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    onClick(window.mapPixelToCoords(mb->position));
                }
            }
        }
        if (!window.isOpen()) {
            break;
        }

        // 갱신: 모든 시간은 초 단위 dt로 진행한다.
        if (game.screen == Screen::Title) {
            game.titleTimer += dt;
            if (game.titleTimer >= kTitleDelay) {
                startPlay();
            }
        } else if (game.screen == Screen::Play) {
            if (game.timerStarted) {
                game.elapsed += dt;
            }
            if (game.checking) {
                game.mismatchTimer -= dt;
                if (game.mismatchTimer <= 0.f) {
                    game.cards[game.firstIdx].faceUp = false;
                    game.cards[game.secondIdx].faceUp = false;
                    game.firstIdx = -1;
                    game.secondIdx = -1;
                    game.checking = false;
                }
            }
        }

        // 그리기
        render(window, game, font, cardsTex);
        window.display();
    }

    return 0;
}
```