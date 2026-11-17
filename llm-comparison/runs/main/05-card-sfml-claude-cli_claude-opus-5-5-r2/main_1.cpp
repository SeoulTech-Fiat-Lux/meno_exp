// PAIRS - card matching mini game (SFML 3.1, C++20)
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
constexpr unsigned int kWidth  = 960;
constexpr unsigned int kHeight = 540;

constexpr int   kCardW   = 80;
constexpr int   kCardH   = 100;
constexpr int   kGap     = 12;
constexpr int   kCols    = 4;
constexpr int   kRows    = 4;
constexpr int   kPairs   = 8;
constexpr float kBoardW  = kCols * kCardW + (kCols - 1) * kGap;
constexpr float kBoardH  = kRows * kCardH + (kRows - 1) * kGap;
constexpr float kBoardX  = (static_cast<float>(kWidth) - kBoardW) / 2.f;
constexpr float kBoardY  = (static_cast<float>(kHeight) - kBoardH) / 2.f;
constexpr float kTitleDuration = 2.f;
constexpr float kMismatchDelay = 0.8f;

const sf::Color kBgColor(0x1B, 0x2A, 0x1F);
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
    int  face    = 1;
    bool up      = false;
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

int toTenths(float seconds)
{
    return static_cast<int>(std::floor(seconds * 10.f + 1e-4f));
}

sf::FloatRect cardRect(int index)
{
    const int col = index % kCols;
    const int row = index / kCols;
    return sf::FloatRect({kBoardX + static_cast<float>(col * (kCardW + kGap)),
                          kBoardY + static_cast<float>(row * (kCardH + kGap))},
                         {static_cast<float>(kCardW), static_cast<float>(kCardH)});
}

void drawCentered(sf::RenderTarget&  target,
                  const sf::Font&    font,
                  const std::string& str,
                  unsigned int       size,
                  float              cx,
                  float              cy,
                  sf::Color          color)
{
    sf::Text text(font, str, size);
    text.setFillColor(color);
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({std::round(b.position.x + b.size.x / 2.f), std::round(b.position.y + b.size.y / 2.f)});
    text.setPosition({std::round(cx), std::round(cy)});
    target.draw(text);
}

sf::View makeLetterboxView(sf::Vector2u windowSize)
{
    sf::View view(sf::FloatRect({0.f, 0.f}, {static_cast<float>(kWidth), static_cast<float>(kHeight)}));
    const float w = static_cast<float>(std::max(1u, windowSize.x));
    const float h = static_cast<float>(std::max(1u, windowSize.y));
    const float targetRatio = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    const float windowRatio = w / h;
    if (windowRatio > targetRatio)
    {
        const float vw = (h * targetRatio) / w;
        view.setViewport(sf::FloatRect({(1.f - vw) / 2.f, 0.f}, {vw, 1.f}));
    }
    else
    {
        const float vh = (w / targetRatio) / h;
        view.setViewport(sf::FloatRect({0.f, (1.f - vh) / 2.f}, {1.f, vh}));
    }
    return view;
}

bool isAsciiAlnum(char32_t c)
{
    return (c >= U'A' && c <= U'Z') || (c >= U'a' && c <= U'z') || (c >= U'0' && c <= U'9');
}
} // namespace

int main()
{
    const std::string fontPath    = "assets/fonts/game.ttf";
    const std::string pointPath   = "assets/sfx/point.wav";
    const std::string flipPath    = "assets/sfx/flip.wav";
    const std::string musicPath   = "assets/music/bgm_card.ogg";
    const std::string texturePath = "assets/textures/cards.png";

    sf::Font font;
    if (!font.openFromFile(fontPath))
    {
        std::cerr << "Failed to load: " << fontPath << '\n';
        return 1;
    }
    sf::SoundBuffer pointBuffer;
    if (!pointBuffer.loadFromFile(pointPath))
    {
        std::cerr << "Failed to load: " << pointPath << '\n';
        return 1;
    }
    sf::SoundBuffer flipBuffer;
    if (!flipBuffer.loadFromFile(flipPath))
    {
        std::cerr << "Failed to load: " << flipPath << '\n';
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile(musicPath))
    {
        std::cerr << "Failed to load: " << musicPath << '\n';
        return 1;
    }
    sf::Texture cardsTexture;
    if (!cardsTexture.loadFromFile(texturePath))
    {
        std::cerr << "Failed to load: " << texturePath << '\n';
        return 1;
    }

    music.setLooping(true);
    music.setVolume(40.f);

    sf::Sound pointSound(pointBuffer);
    sf::Sound flipSound(flipBuffer);

    sf::RenderWindow window(sf::VideoMode({kWidth, kHeight}), "PAIRS");
    window.setFramerateLimit(60);
    sf::View view = makeLetterboxView(window.getSize());

    std::mt19937 rng(std::random_device{}());

    // Game state
    Screen              screen    = Screen::Title;
    float               titleTime = 0.f;
    std::array<Card, 16> cards{};
    int                 firstIdx  = -1;
    int                 secondIdx = -1;
    bool                waiting   = false;
    float               waitTimer = 0.f;
    int                 moves     = 0;
    float               timeSec   = 0.f;
    bool                timerRunning = false;
    bool                timerStarted = false;
    int                 matchedPairs = 0;
    int                 finalTenths  = 0;

    // Result / records (records persist while program runs)
    std::vector<Record> records;
    int                 nextSeq       = 0;
    int                 currentSeq    = -1;
    bool                nameConfirmed = false;
    std::string         nameInput;

    auto startGame = [&]()
    {
        std::array<int, 16> faces{};
        for (int i = 0; i < 16; ++i)
            faces[static_cast<std::size_t>(i)] = i / 2 + 1;
        std::shuffle(faces.begin(), faces.end(), rng);
        for (std::size_t i = 0; i < cards.size(); ++i)
            cards[i] = Card{faces[i], false, false};
        firstIdx      = -1;
        secondIdx     = -1;
        waiting       = false;
        waitTimer     = 0.f;
        moves         = 0;
        timeSec       = 0.f;
        timerRunning  = false;
        timerStarted  = false;
        matchedPairs  = 0;
        finalTenths   = 0;
        currentSeq    = -1;
        nameConfirmed = false;
        nameInput.clear();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto goToResult = [&]()
    {
        timerRunning = false;
        finalTenths  = toTenths(timeSec);
        music.stop();
        nameInput.clear();
        nameConfirmed = false;
        currentSeq    = -1;
        screen        = Screen::Result;
    };

    auto handleClick = [&](sf::Vector2i pixel)
    {
        if (screen != Screen::Play || waiting)
            return;
        const sf::Vector2f world = window.mapPixelToCoords(pixel, view);
        for (int i = 0; i < 16; ++i)
        {
            if (!cardRect(i).contains(world))
                continue;
            Card& card = cards[static_cast<std::size_t>(i)];
            if (card.up || card.matched)
                return;
            card.up = true;
            flipSound.play();
            if (!timerStarted)
            {
                timerStarted = true;
                timerRunning = true;
            }
            if (firstIdx < 0)
            {
                firstIdx = i;
            }
            else
            {
                secondIdx = i;
                ++moves;
                Card& a = cards[static_cast<std::size_t>(firstIdx)];
                Card& b = cards[static_cast<std::size_t>(secondIdx)];
                if (a.face == b.face)
                {
                    a.matched = true;
                    b.matched = true;
                    pointSound.play();
                    firstIdx  = -1;
                    secondIdx = -1;
                    ++matchedPairs;
                    if (matchedPairs >= kPairs)
                        goToResult();
                }
                else
                {
                    waiting   = true;
                    waitTimer = kMismatchDelay;
                }
            }
            return;
        }
    };

    auto confirmName = [&]()
    {
        if (nameInput.empty())
            return;
        Record r{nameInput, moves, finalTenths, nextSeq};
        currentSeq = nextSeq;
        ++nextSeq;
        records.push_back(r);
        nameConfirmed = true;
    };

    sf::Clock clock;
    while (window.isOpen())
    {
        float dt = clock.restart().asSeconds();
        if (dt > 0.25f)
            dt = 0.25f;

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                view = makeLetterboxView(resized->size);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                    break;
                }
                switch (screen)
                {
                    case Screen::Title:
                        if (key->code == sf::Keyboard::Key::Enter)
                            startGame();
                        break;
                    case Screen::Play:
                        break;
                    case Screen::Result:
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
                        break;
                }
            }
            else if (const auto* text = event->getIf<sf::Event::TextEntered>())
            {
                if (screen == Screen::Result && !nameConfirmed)
                {
                    const char32_t c = text->unicode;
                    if (isAsciiAlnum(c) && nameInput.size() < 8)
                    {
                        char ch = static_cast<char>(c);
                        if (ch >= 'a' && ch <= 'z')
                            ch = static_cast<char>(ch - 'a' + 'A');
                        nameInput.push_back(ch);
                    }
                }
            }
            else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button == sf::Mouse::Button::Left)
                    handleClick(mouse->position);
            }
        }
        if (!window.isOpen())
            break;

        // Update
        if (screen == Screen::Title)
        {
            titleTime += dt;
            if (titleTime >= kTitleDuration)
                startGame();
        }
        else if (screen == Screen::Play)
        {
            if (timerRunning)
                timeSec += dt;
            if (waiting)
            {
                waitTimer -= dt;
                if (waitTimer <= 0.f)
                {
                    if (firstIdx >= 0)
                        cards[static_cast<std::size_t>(firstIdx)].up = false;
                    if (secondIdx >= 0)
                        cards[static_cast<std::size_t>(secondIdx)].up = false;
                    firstIdx  = -1;
                    secondIdx = -1;
                    waiting   = false;
                    waitTimer = 0.f;
                }
            }
        }

        // Draw
        window.clear(sf::Color::Black);
        window.setView(view);
        sf::RectangleShape bg({static_cast<float>(kWidth), static_cast<float>(kHeight)});
        bg.setFillColor(kBgColor);
        window.draw(bg);

        const float cx = static_cast<float>(kWidth) / 2.f;

        if (screen == Screen::Title)
        {
            drawCentered(window, font, "PAIRS", 48, cx, 220.f, kTextColor);
            drawCentered(window, font, "PRESS ENTER", 32, cx, 320.f, kTextColor);
        }
        else if (screen == Screen::Play)
        {
            const std::string hud = "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(toTenths(timeSec));
            drawCentered(window, font, hud, 16, cx, kBoardY / 2.f, kTextColor);

            sf::Sprite sprite(cardsTexture);
            for (int i = 0; i < 16; ++i)
            {
                const Card& card = cards[static_cast<std::size_t>(i)];
                const int   cell = (card.up || card.matched) ? card.face : 0;
                sprite.setTextureRect(sf::IntRect({cell * kCardW, 0}, {kCardW, kCardH}));
                sprite.setPosition(cardRect(i).position);
                window.draw(sprite);
            }
        }
        else
        {
            drawCentered(window, font, "CLEAR", 48, cx, 60.f, kTextColor);
            const std::string score = "MOVES " + std::to_string(moves) + "   TIME " + formatTenths(finalTenths);
            drawCentered(window, font, score, 32, cx, 120.f, kTextColor);

            if (!nameConfirmed)
            {
                drawCentered(window, font, "NAME: " + nameInput, 32, cx, 250.f, kTextColor);
            }
            else
            {
                std::vector<Record> sorted = records;
                std::sort(sorted.begin(),
                          sorted.end(),
                          [](const Record& a, const Record& b)
                          {
                              if (a.moves != b.moves)
                                  return a.moves < b.moves;
                              if (a.tenths != b.tenths)
                                  return a.tenths < b.tenths;
                              return a.seq < b.seq;
                          });
                auto lineFor = [](std::size_t rank, const Record& r)
                {
                    return std::to_string(rank) + ". " + r.name + "   MOVES " + std::to_string(r.moves) +
                           "   TIME " + formatTenths(r.tenths);
                };
                float y = 185.f;
                for (std::size_t i = 0; i < sorted.size() && i < 5; ++i)
                {
                    const sf::Color c = (sorted[i].seq == currentSeq) ? kHighlight : kTextColor;
                    drawCentered(window, font, lineFor(i + 1, sorted[i]), 32, cx, y, c);
                    y += 44.f;
                }
                for (std::size_t i = 5; i < sorted.size(); ++i)
                {
                    if (sorted[i].seq == currentSeq)
                    {
                        drawCentered(window, font, lineFor(i + 1, sorted[i]), 32, cx, 185.f + 5 * 44.f + 10.f, kHighlight);
                        break;
                    }
                }
            }

            drawCentered(window, font, "R: RESTART  ESC: QUIT", 16, cx, 505.f, kTextColor);
        }

        window.display();
    }

    music.stop();
    return 0;
}
