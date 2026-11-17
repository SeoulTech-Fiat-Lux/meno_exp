```cpp
// PAIRS - 카드 짝 맞추기 (SFML 3.1, C++20)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

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
constexpr unsigned kWinW = 960;
constexpr unsigned kWinH = 540;
constexpr float    kW    = 960.f;
constexpr float    kH    = 540.f;

constexpr int   kCardW   = 80;
constexpr int   kCardH   = 100;
constexpr int   kGap     = 12;
constexpr int   kCols    = 4;
constexpr int   kRows    = 4;
constexpr int   kCards   = kCols * kRows;
constexpr float kBoardW  = kCols * kCardW + (kCols - 1) * kGap; // 356
constexpr float kBoardH  = kRows * kCardH + (kRows - 1) * kGap; // 436
constexpr float kBoardX  = (kW - kBoardW) / 2.f;                 // 302
constexpr float kBoardY  = (kH - kBoardH) / 2.f;                 // 52

constexpr float kTitleTime    = 2.0f;
constexpr float kMismatchTime = 0.8f;

const sf::Color kBg(0x1B, 0x2A, 0x1F);
const sf::Color kText(0xFF, 0xFF, 0xFF);
const sf::Color kHighlight(0xFF, 0xD5, 0x4A);

enum class Screen
{
    Title,
    Play,
    Result
};

struct Card
{
    int  value   = 0; // 0..7
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

std::string formatRecord(int rank, const Record& r)
{
    return std::to_string(rank) + ". " + r.name + "   MOVES " + std::to_string(r.moves) + "   TIME " +
           formatTenths(r.tenths);
}

sf::View makeLetterboxView(sf::Vector2u size)
{
    sf::View view(sf::FloatRect({0.f, 0.f}, {kW, kH}));
    if (size.x == 0 || size.y == 0)
        return view;
    const float winRatio  = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float gameRatio = kW / kH;
    float       vx = 0.f, vy = 0.f, vw = 1.f, vh = 1.f;
    if (winRatio > gameRatio)
    {
        vw = gameRatio / winRatio;
        vx = (1.f - vw) / 2.f;
    }
    else
    {
        vh = winRatio / gameRatio;
        vy = (1.f - vh) / 2.f;
    }
    view.setViewport(sf::FloatRect({vx, vy}, {vw, vh}));
    return view;
}
} // namespace

int main()
{
    try
    {
        // ---------- 에셋 ----------
        sf::Font font;
        if (!font.openFromFile("assets/fonts/game.ttf"))
        {
            std::cerr << "Failed to load: assets/fonts/game.ttf\n";
            return 1;
        }
        sf::Texture cardsTex;
        if (!cardsTex.loadFromFile("assets/textures/cards.png"))
        {
            std::cerr << "Failed to load: assets/textures/cards.png\n";
            return 1;
        }
        sf::SoundBuffer flipBuf;
        if (!flipBuf.loadFromFile("assets/sfx/flip.wav"))
        {
            std::cerr << "Failed to load: assets/sfx/flip.wav\n";
            return 1;
        }
        sf::SoundBuffer pointBuf;
        if (!pointBuf.loadFromFile("assets/sfx/point.wav"))
        {
            std::cerr << "Failed to load: assets/sfx/point.wav\n";
            return 1;
        }
        sf::Music music;
        if (!music.openFromFile("assets/music/bgm_card.ogg"))
        {
            std::cerr << "Failed to load: assets/music/bgm_card.ogg\n";
            return 1;
        }
        music.setLooping(true);
        music.setVolume(40.f);

        sf::Sound flipSound(flipBuf);
        sf::Sound pointSound(pointBuf);

        // ---------- 창 ----------
        sf::RenderWindow window(sf::VideoMode({kWinW, kWinH}), "PAIRS");
        window.setFramerateLimit(60);
        sf::View view = makeLetterboxView(window.getSize());

        // ---------- 상태 ----------
        std::mt19937 rng(std::random_device{}());

        Screen screen     = Screen::Title;
        float  titleTimer = 0.f;

        std::array<Card, kCards> cards{};
        int    moves        = 0;
        double elapsed      = 0.0;
        bool   timerStarted = false;
        bool   finished     = false;
        int    finalTenths  = 0;
        int    firstPick    = -1;
        int    secondPick   = -1;
        float  mismatchWait = 0.f;
        int    matchedPairs = 0;

        std::string         nameInput;
        bool                nameConfirmed = false;
        std::vector<Record> records; // 프로그램 실행 중 유지
        int                 nextSeq    = 0;
        int                 currentSeq = -1;

        auto startGame = [&]()
        {
            std::array<int, kCards> values{};
            for (int i = 0; i < kCards; ++i)
                values[i] = i / 2;
            std::shuffle(values.begin(), values.end(), rng);
            for (int i = 0; i < kCards; ++i)
                cards[i] = Card{values[i], false, false};
            moves         = 0;
            elapsed       = 0.0;
            timerStarted  = false;
            finished      = false;
            finalTenths   = 0;
            firstPick     = -1;
            secondPick    = -1;
            mismatchWait  = 0.f;
            matchedPairs  = 0;
            nameInput.clear();
            nameConfirmed = false;
            currentSeq    = -1;
            screen        = Screen::Play;
            music.stop();
            music.play();
        };

        auto currentTenths = [&]() -> int
        {
            if (finished)
                return finalTenths;
            return static_cast<int>(std::floor(elapsed * 10.0 + 1e-9));
        };

        auto cardAt = [&](sf::Vector2f p) -> int
        {
            for (int i = 0; i < kCards; ++i)
            {
                const float x = kBoardX + static_cast<float>((i % kCols) * (kCardW + kGap));
                const float y = kBoardY + static_cast<float>((i / kCols) * (kCardH + kGap));
                if (p.x >= x && p.x < x + kCardW && p.y >= y && p.y < y + kCardH)
                    return i;
            }
            return -1;
        };

        auto handleClick = [&](sf::Vector2f p)
        {
            if (screen != Screen::Play || finished)
                return;
            if (secondPick != -1) // 판정 대기 중
                return;
            const int idx = cardAt(p);
            if (idx < 0)
                return;
            Card& c = cards[idx];
            if (c.faceUp || c.matched)
                return;

            c.faceUp = true;
            flipSound.play();
            if (!timerStarted)
                timerStarted = true;

            if (firstPick == -1)
            {
                firstPick = idx;
                return;
            }

            // 두 번째 카드: 판정
            ++moves;
            if (cards[firstPick].value == c.value)
            {
                cards[firstPick].matched = true;
                c.matched                = true;
                pointSound.play();
                firstPick = -1;
                ++matchedPairs;
                if (matchedPairs == kCards / 2)
                {
                    finalTenths = static_cast<int>(std::floor(elapsed * 10.0 + 1e-9));
                    finished    = true;
                    screen      = Screen::Result;
                    music.stop();
                }
            }
            else
            {
                secondPick   = idx;
                mismatchWait = kMismatchTime;
            }
        };

        auto confirmName = [&]()
        {
            if (nameInput.empty() || nameConfirmed)
                return;
            nameConfirmed = true;
            currentSeq    = nextSeq++;
            records.push_back(Record{nameInput, moves, finalTenths, currentSeq});
            std::stable_sort(records.begin(),
                             records.end(),
                             [](const Record& a, const Record& b)
                             {
                                 if (a.moves != b.moves)
                                     return a.moves < b.moves;
                                 if (a.tenths != b.tenths)
                                     return a.tenths < b.tenths;
                                 return a.seq < b.seq;
                             });
        };

        auto drawText = [&](const std::string& s, unsigned size, float cx, float y, sf::Color color)
        {
            sf::Text t(font, s, size);
            t.setFillColor(color);
            const sf::FloatRect b = t.getLocalBounds();
            t.setOrigin({std::round(b.position.x + b.size.x / 2.f), 0.f});
            t.setPosition({std::round(cx), std::round(y)});
            window.draw(t);
        };

        sf::Clock clock;

        // ---------- 메인 루프 ----------
        while (window.isOpen())
        {
            while (const std::optional event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                }
                else if (const auto* resized = event->getIf<sf::Event::Resized>())
                {
                    view = makeLetterboxView(resized->size);
                }
                else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        window.close();
                    }
                    else if (screen == Screen::Title)
                    {
                        if (key->code == sf::Keyboard::Key::Enter)
                            startGame();
                    }
                    else if (screen == Screen::Result)
                    {
                        if (!nameConfirmed)
                        {
                            if (key->code == sf::Keyboard::Key::Backspace)
                            {
                                if (!nameInput.empty())
                                    nameInput.pop_back();
                            }
                            else if (key->code == sf::Keyboard::Key::Enter)
                            {
                                confirmName();
                            }
                        }
                        else if (key->code == sf::Keyboard::Key::R)
                        {
                            startGame();
                        }
                    }
                }
                else if (const auto* text = event->getIf<sf::Event::TextEntered>())
                {
                    if (screen == Screen::Result && !nameConfirmed)
                    {
                        const char32_t ch = text->unicode;
                        const bool     isAlpha =
                            (ch >= U'a' && ch <= U'z') || (ch >= U'A' && ch <= U'Z');
                        const bool isDigit = (ch >= U'0' && ch <= U'9');
                        if ((isAlpha || isDigit) && nameInput.size() < 8)
                        {
                            char c = static_cast<char>(ch);
                            if (c >= 'a' && c <= 'z')
                                c = static_cast<char>(c - 'a' + 'A');
                            nameInput.push_back(c);
                        }
                    }
                }
                else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button == sf::Mouse::Button::Left)
                    {
                        const sf::Vector2f p = window.mapPixelToCoords(mouse->position, view);
                        handleClick(p);
                    }
                }
            }
            if (!window.isOpen())
                break;

            // ---------- 갱신 ----------
            float dt = clock.restart().asSeconds();
            if (dt > 0.5f)
                dt = 0.5f;

            if (screen == Screen::Title)
            {
                titleTimer += dt;
                if (titleTimer >= kTitleTime)
                    startGame();
            }
            else if (screen == Screen::Play)
            {
                if (timerStarted && !finished)
                    elapsed += dt;
                if (secondPick != -1)
                {
                    mismatchWait -= dt;
                    if (mismatchWait <= 0.f)
                    {
                        cards[firstPick].faceUp  = false;
                        cards[secondPick].faceUp = false;
                        firstPick                = -1;
                        secondPick               = -1;
                        mismatchWait             = 0.f;
                    }
                }
            }

            // ---------- 그리기 ----------
            window.clear(sf::Color::Black);
            window.setView(view);

            sf::RectangleShape bg({kW, kH});
            bg.setFillColor(kBg);
            window.draw(bg);

            if (screen == Screen::Title)
            {
                drawText("PAIRS", 48, kW / 2.f, 190.f, kText);
                drawText("PRESS ENTER", 32, kW / 2.f, 300.f, kText);
            }
            else if (screen == Screen::Play)
            {
                drawText("MOVES " + std::to_string(moves) + "   TIME " + formatTenths(currentTenths()),
                         32,
                         kW / 2.f,
                         8.f,
                         kText);
                sf::Sprite sprite(cardsTex);
                for (int i = 0; i < kCards; ++i)
                {
                    const Card& c     = cards[i];
                    const int   frame = (c.faceUp || c.matched) ? c.value + 1 : 0;
                    sprite.setTextureRect(sf::IntRect({frame * kCardW, 0}, {kCardW, kCardH}));
                    sprite.setPosition({kBoardX + static_cast<float>((i % kCols) * (kCardW + kGap)),
                                        kBoardY + static_cast<float>((i / kCols) * (kCardH + kGap))});
                    window.draw(sprite);
                }
            }
            else // Result
            {
                drawText("CLEAR", 48, kW / 2.f, 40.f, kText);
                drawText("MOVES " + std::to_string(moves) + "   TIME " + formatTenths(finalTenths),
                         32,
                         kW / 2.f,
                         110.f,
                         kText);

                if (!nameConfirmed)
                {
                    drawText("NAME: " + nameInput, 32, kW / 2.f, 200.f, kText);
                }
                else
                {
                    int currentRank = -1;
                    for (std::size_t i = 0; i < records.size(); ++i)
                        if (records[i].seq == currentSeq)
                            currentRank = static_cast<int>(i) + 1;

                    const std::size_t shown = std::min<std::size_t>(5, records.size());
                    float             y     = 170.f;
                    for (std::size_t i = 0; i < shown; ++i)
                    {
                        const Record& r = records[i];
                        drawText(formatRecord(static_cast<int>(i) + 1, r),
                                 32,
                                 kW / 2.f,
                                 y,
                                 r.seq == currentSeq ? kHighlight : kText);
                        y += 40.f;
                    }
                    if (currentRank > 5)
                    {
                        drawText(formatRecord(currentRank, records[static_cast<std::size_t>(currentRank - 1)]),
                                 32,
                                 kW / 2.f,
                                 y + 10.f,
                                 kHighlight);
                    }
                }

                drawText("R: RESTART  ESC: QUIT", 32, kW / 2.f, 470.f, kText);
            }

            window.display();
        }

        music.stop();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
```