```cpp
// PAIRS - 카드 짝 맞추기 (SFML 3.1, C++20)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float kWidth  = 960.f;
constexpr float kHeight = 540.f;
constexpr float kCardW  = 80.f;
constexpr float kCardH  = 100.f;
constexpr float kGap    = 12.f;
constexpr int   kRows   = 4;
constexpr int   kCols   = 4;
constexpr int   kCards  = kRows * kCols;
constexpr int   kPairs  = kCards / 2;

constexpr float kBoardW = kCols * kCardW + (kCols - 1) * kGap;
constexpr float kBoardH = kRows * kCardH + (kRows - 1) * kGap;
constexpr float kBoardX = (kWidth - kBoardW) / 2.f;
constexpr float kBoardY = (kHeight - kBoardH) / 2.f + 10.f;

constexpr float kTitleDuration    = 2.0f;
constexpr float kMismatchDuration = 0.8f;

const sf::Color kBackground(0x1B, 0x2A, 0x1F);
const sf::Color kTextColor(0xFF, 0xFF, 0xFF);
const sf::Color kHighlight(0xFF, 0xD5, 0x4A);

enum class Screen
{
    Title,
    Play,
    Result
};

struct Card
{
    int  value   = 1; // 1..8
    bool faceUp  = false;
    bool matched = false;
};

struct Record
{
    std::string name;
    int         moves  = 0;
    int         tenths = 0;
    int         seq    = 0;
};

std::string formatTenths(int tenths)
{
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::string recordLine(int rank, const Record& r)
{
    return std::to_string(rank) + ". " + r.name + "   MOVES " + std::to_string(r.moves) + "   TIME " +
           formatTenths(r.tenths);
}

sf::View makeLetterboxView(sf::Vector2u windowSize)
{
    sf::View view(sf::FloatRect({0.f, 0.f}, {kWidth, kHeight}));
    if (windowSize.x == 0 || windowSize.y == 0)
        return view;

    const float windowRatio = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
    const float targetRatio = kWidth / kHeight;

    float vx = 0.f, vy = 0.f, vw = 1.f, vh = 1.f;
    if (windowRatio > targetRatio)
    {
        vw = targetRatio / windowRatio;
        vx = (1.f - vw) / 2.f;
    }
    else
    {
        vh = windowRatio / targetRatio;
        vy = (1.f - vh) / 2.f;
    }
    view.setViewport(sf::FloatRect({vx, vy}, {vw, vh}));
    return view;
}

sf::FloatRect cardRect(int index)
{
    const int row = index / kCols;
    const int col = index % kCols;
    return sf::FloatRect({kBoardX + col * (kCardW + kGap), kBoardY + row * (kCardH + kGap)}, {kCardW, kCardH});
}

void drawCentered(sf::RenderWindow& window,
                  const sf::Font&   font,
                  const std::string& str,
                  unsigned int      size,
                  const sf::Color&  color,
                  float             centerX,
                  float             top)
{
    sf::Text text(font, str, size);
    text.setFillColor(color);
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x / 2.f, 0.f});
    text.setPosition({std::round(centerX), std::round(top)});
    window.draw(text);
}
} // namespace

int main()
{
    // ---------- 에셋 로드 ----------
    const std::string fontPath  = "assets/fonts/game.ttf";
    const std::string texPath   = "assets/textures/cards.png";
    const std::string flipPath  = "assets/sfx/flip.wav";
    const std::string pointPath = "assets/sfx/point.wav";
    const std::string bgmPath   = "assets/music/bgm_card.ogg";

    sf::Font font;
    if (!font.openFromFile(fontPath))
    {
        std::cerr << "Failed to load: " << fontPath << '\n';
        return 1;
    }
    sf::Texture cardTexture;
    if (!cardTexture.loadFromFile(texPath))
    {
        std::cerr << "Failed to load: " << texPath << '\n';
        return 1;
    }
    sf::SoundBuffer flipBuffer;
    if (!flipBuffer.loadFromFile(flipPath))
    {
        std::cerr << "Failed to load: " << flipPath << '\n';
        return 1;
    }
    sf::SoundBuffer pointBuffer;
    if (!pointBuffer.loadFromFile(pointPath))
    {
        std::cerr << "Failed to load: " << pointPath << '\n';
        return 1;
    }
    sf::Music bgm;
    if (!bgm.openFromFile(bgmPath))
    {
        std::cerr << "Failed to load: " << bgmPath << '\n';
        return 1;
    }
    bgm.setLooping(true);
    bgm.setVolume(40.f);

    sf::Sound flipSound(flipBuffer);
    sf::Sound pointSound(pointBuffer);

    // ---------- 창 ----------
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "PAIRS");
    window.setFramerateLimit(60);
    sf::View view = makeLetterboxView(window.getSize());
    window.setView(view);

    std::mt19937 rng(std::random_device{}());

    // ---------- 게임 상태 ----------
    Screen screen      = Screen::Title;
    float  titleTimer  = 0.f;

    std::array<Card, kCards> cards{};
    int   firstPick     = -1;
    int   secondPick    = -1;
    float mismatchTimer = 0.f;
    bool  waiting       = false;
    int   moves         = 0;
    int   matchedPairs  = 0;
    bool  timerStarted  = false;
    bool  timerStopped  = false;
    float elapsed       = 0.f;
    int   finalTenths   = 0;

    // 결과/기록
    std::string         nameInput;
    bool                nameConfirmed = false;
    std::vector<Record> records; // 프로그램 실행 중 유지
    int                 nextSeq    = 0;
    int                 currentSeq = -1;

    auto currentTenths = [&]() -> int
    {
        if (timerStopped)
            return finalTenths;
        return static_cast<int>(std::floor(elapsed * 10.f + 1e-4f));
    };

    auto startGame = [&]()
    {
        std::array<int, kCards> values{};
        for (int i = 0; i < kCards; ++i)
            values[static_cast<std::size_t>(i)] = i / 2 + 1;
        std::shuffle(values.begin(), values.end(), rng);
        for (int i = 0; i < kCards; ++i)
        {
            auto& c   = cards[static_cast<std::size_t>(i)];
            c.value   = values[static_cast<std::size_t>(i)];
            c.faceUp  = false;
            c.matched = false;
        }
        firstPick     = -1;
        secondPick    = -1;
        mismatchTimer = 0.f;
        waiting       = false;
        moves         = 0;
        matchedPairs  = 0;
        timerStarted  = false;
        timerStopped  = false;
        elapsed       = 0.f;
        finalTenths   = 0;
        nameInput.clear();
        nameConfirmed = false;
        currentSeq    = -1;

        screen = Screen::Play;
        bgm.stop();
        bgm.play();
    };

    auto goResult = [&]()
    {
        screen = Screen::Result;
        bgm.stop();
        nameInput.clear();
        nameConfirmed = false;
    };

    auto handleClick = [&](sf::Vector2f p)
    {
        if (screen != Screen::Play || waiting)
            return;
        for (int i = 0; i < kCards; ++i)
        {
            if (!cardRect(i).contains(p))
                continue;
            Card& c = cards[static_cast<std::size_t>(i)];
            if (c.faceUp || c.matched)
                return;

            c.faceUp = true;
            flipSound.play();
            if (!timerStarted)
            {
                timerStarted = true;
                elapsed      = 0.f;
            }

            if (firstPick < 0)
            {
                firstPick = i;
            }
            else
            {
                secondPick = i;
                ++moves;
                Card& a = cards[static_cast<std::size_t>(firstPick)];
                Card& b = cards[static_cast<std::size_t>(secondPick)];
                if (a.value == b.value)
                {
                    a.matched = true;
                    b.matched = true;
                    pointSound.play();
                    firstPick  = -1;
                    secondPick = -1;
                    ++matchedPairs;
                    if (matchedPairs >= kPairs)
                    {
                        finalTenths  = static_cast<int>(std::floor(elapsed * 10.f + 1e-4f));
                        timerStopped = true;
                        goResult();
                    }
                }
                else
                {
                    waiting       = true;
                    mismatchTimer = kMismatchDuration;
                }
            }
            return;
        }
    };

    auto confirmName = [&]()
    {
        if (nameInput.empty())
            return;
        Record r;
        r.name   = nameInput;
        r.moves  = moves;
        r.tenths = finalTenths;
        r.seq    = nextSeq++;
        records.push_back(r);
        currentSeq = r.seq;
        std::stable_sort(records.begin(),
                         records.end(),
                         [](const Record& x, const Record& y)
                         {
                             if (x.moves != y.moves)
                                 return x.moves < y.moves;
                             if (x.tenths != y.tenths)
                                 return x.tenths < y.tenths;
                             return x.seq < y.seq;
                         });
        nameConfirmed = true;
    };

    sf::Clock clock;
    sf::Sprite cardSprite(cardTexture);
    sf::RectangleShape background({kWidth, kHeight});
    background.setFillColor(kBackground);

    while (window.isOpen())
    {
        // ---------- 입력 ----------
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                view = makeLetterboxView(resized->size);
                window.setView(view);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                }
                else if (screen == Screen::Title && key->code == sf::Keyboard::Key::Enter)
                {
                    startGame();
                }
                else if (screen == Screen::Result)
                {
                    if (!nameConfirmed)
                    {
                        if (key->code == sf::Keyboard::Key::Enter)
                            confirmName();
                        else if (key->code == sf::Keyboard::Key::Backspace && !nameInput.empty())
                            nameInput.pop_back();
                    }
                    else if (key->code == sf::Keyboard::Key::R)
                    {
                        startGame();
                    }
                }
            }
            else if (const auto* text = event->getIf<sf::Event::TextEntered>())
            {
                if (screen == Screen::Result && !nameConfirmed && nameInput.size() < 8)
                {
                    const char32_t ch = text->unicode;
                    if (ch >= U'a' && ch <= U'z')
                        nameInput.push_back(static_cast<char>(ch - U'a' + U'A'));
                    else if ((ch >= U'A' && ch <= U'Z') || (ch >= U'0' && ch <= U'9'))
                        nameInput.push_back(static_cast<char>(ch));
                }
            }
            else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                    handleClick(window.mapPixelToCoords(mouse->position, view));
            }
        }
        if (!window.isOpen())
            break;

        // ---------- 갱신 ----------
        float dt = clock.restart().asSeconds();
        dt       = std::min(dt, 0.25f);

        if (screen == Screen::Title)
        {
            titleTimer += dt;
            if (titleTimer >= kTitleDuration)
                startGame();
        }
        else if (screen == Screen::Play)
        {
            if (timerStarted && !timerStopped)
                elapsed += dt;

            if (waiting)
            {
                mismatchTimer -= dt;
                if (mismatchTimer <= 0.f)
                {
                    if (firstPick >= 0)
                        cards[static_cast<std::size_t>(firstPick)].faceUp = false;
                    if (secondPick >= 0)
                        cards[static_cast<std::size_t>(secondPick)].faceUp = false;
                    firstPick  = -1;
                    secondPick = -1;
                    waiting    = false;
                }
            }
        }

        // ---------- 그리기 ----------
        window.clear(sf::Color::Black);
        window.setView(view);
        window.draw(background);

        const float cx = kWidth / 2.f;

        if (screen == Screen::Title)
        {
            drawCentered(window, font, "PAIRS", 48, kTextColor, cx, 180.f);
            drawCentered(window, font, "PRESS ENTER", 32, kTextColor, cx, 300.f);
        }
        else if (screen == Screen::Play)
        {
            drawCentered(window,
                         font,
                         "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(currentTenths()),
                         32,
                         kTextColor,
                         cx,
                         6.f);

            for (int i = 0; i < kCards; ++i)
            {
                const Card& c     = cards[static_cast<std::size_t>(i)];
                const int   frame = (c.faceUp || c.matched) ? c.value : 0;
                cardSprite.setTextureRect(sf::IntRect({frame * 80, 0}, {80, 100}));
                cardSprite.setPosition(cardRect(i).position);
                window.draw(cardSprite);
            }
        }
        else // Result
        {
            drawCentered(window, font, "CLEAR", 48, kTextColor, cx, 24.f);
            drawCentered(window,
                         font,
                         "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(finalTenths),
                         32,
                         kTextColor,
                         cx,
                         90.f);

            if (!nameConfirmed)
            {
                drawCentered(window, font, "NAME: " + nameInput, 32, kTextColor, cx, 220.f);
            }
            else
            {
                int currentRank = -1;
                for (std::size_t i = 0; i < records.size(); ++i)
                    if (records[i].seq == currentSeq)
                        currentRank = static_cast<int>(i) + 1;

                const std::size_t shown = std::min<std::size_t>(5, records.size());
                for (std::size_t i = 0; i < shown; ++i)
                {
                    const Record& r     = records[i];
                    const sf::Color col = (r.seq == currentSeq) ? kHighlight : kTextColor;
                    drawCentered(window,
                                 font,
                                 recordLine(static_cast<int>(i) + 1, r),
                                 32,
                                 col,
                                 cx,
                                 150.f + static_cast<float>(i) * 40.f);
                }
                if (currentRank > 5)
                {
                    drawCentered(window,
                                 font,
                                 recordLine(currentRank, records[static_cast<std::size_t>(currentRank - 1)]),
                                 32,
                                 kHighlight,
                                 cx,
                                 150.f + 5.f * 40.f + 16.f);
                }
            }

            drawCentered(window, font, "R: RESTART  ESC: QUIT", 32, kTextColor, cx, 480.f);
        }

        window.display();
    }

    bgm.stop();
    return 0;
}
```