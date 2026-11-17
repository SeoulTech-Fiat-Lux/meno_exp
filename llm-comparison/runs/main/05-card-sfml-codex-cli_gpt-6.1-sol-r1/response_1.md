```cpp
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

constexpr float Width = 960.f;
constexpr float Height = 540.f;
constexpr float CardWidth = 80.f;
constexpr float CardHeight = 100.f;
constexpr float Gap = 12.f;
constexpr float BoardX = (Width - (4.f * CardWidth + 3.f * Gap)) / 2.f;
constexpr float BoardY = (Height - (4.f * CardHeight + 3.f * Gap)) / 2.f;

const sf::Color Background{0x1B, 0x2A, 0x1F};
const sf::Color Highlight{0xFF, 0xD5, 0x4A};

enum class Screen
{
    Title,
    Play,
    Result
};

struct Card
{
    int face = 1;
    bool revealed = false;
    bool matched = false;
};

struct Record
{
    std::string name;
    int moves = 0;
    std::int64_t ticks = 0;
    std::uint64_t id = 0;
};

std::string timeString(std::int64_t ticks)
{
    return std::to_string(ticks / 10) + "." + std::to_string(ticks % 10);
}

sf::FloatRect cardBounds(std::size_t index)
{
    return {{BoardX + static_cast<float>(index % 4) * (CardWidth + Gap),
             BoardY + static_cast<float>(index / 4) * (CardHeight + Gap)},
            {CardWidth, CardHeight}};
}

int assetFailure(const char* path)
{
    std::cerr << "FAILED TO LOAD: " << path << '\n';
    return 1;
}
}

int main()
{
    try
    {
        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "PAIRS");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        sf::Font font;
        if (!font.openFromFile("assets/fonts/game.ttf"))
            return assetFailure("assets/fonts/game.ttf");

        sf::Texture cardsTexture;
        if (!cardsTexture.loadFromFile("assets/textures/cards.png"))
            return assetFailure("assets/textures/cards.png");

        sf::SoundBuffer flipBuffer;
        if (!flipBuffer.loadFromFile("assets/sfx/flip.wav"))
            return assetFailure("assets/sfx/flip.wav");

        sf::SoundBuffer pointBuffer;
        if (!pointBuffer.loadFromFile("assets/sfx/point.wav"))
            return assetFailure("assets/sfx/point.wav");

        sf::Music music;
        if (!music.openFromFile("assets/music/bgm_card.ogg"))
            return assetFailure("assets/music/bgm_card.ogg");

        sf::Sound flipSound(flipBuffer);
        sf::Sound pointSound(pointBuffer);
        music.setVolume(40.f);
        music.setLooping(true);

        sf::View view(sf::FloatRect({0.f, 0.f}, {Width, Height}));
        const auto resizeView = [&](sf::Vector2u size)
        {
            if (size.x == 0 || size.y == 0)
                return;

            const float aspect = static_cast<float>(size.x) /
                                 static_cast<float>(size.y);
            constexpr float targetAspect = Width / Height;

            if (aspect > targetAspect)
            {
                const float viewportWidth = targetAspect / aspect;
                view.setViewport(
                    sf::FloatRect({(1.f - viewportWidth) / 2.f, 0.f},
                                  {viewportWidth, 1.f}));
            }
            else
            {
                const float viewportHeight = aspect / targetAspect;
                view.setViewport(
                    sf::FloatRect({0.f, (1.f - viewportHeight) / 2.f},
                                  {1.f, viewportHeight}));
            }
            window.setView(view);
        };
        resizeView(window.getSize());

        sf::RectangleShape background({Width, Height});
        background.setFillColor(Background);

        const auto drawText = [&](const std::string& value,
                                  unsigned int size,
                                  sf::Vector2f position,
                                  sf::Color color = sf::Color::White,
                                  bool centered = false)
        {
            sf::Text text(font, value, size);
            text.setFillColor(color);
            const auto bounds = text.getLocalBounds();
            if (centered)
                text.setOrigin({bounds.position.x + bounds.size.x / 2.f,
                                bounds.position.y});
            else
                text.setOrigin(bounds.position);
            text.setPosition(position);
            window.draw(text);
        };

        std::mt19937 random(std::random_device{}());
        std::array<Card, 16> cards{};
        std::vector<Record> records;

        Screen screen = Screen::Title;
        const TimePoint titleStart = Clock::now();
        TimePoint timerStart{};
        TimePoint mismatchDeadline{};

        bool timerRunning = false;
        bool mismatchPending = false;
        bool nameConfirmed = false;
        int first = -1;
        int second = -1;
        int moves = 0;
        int matchedPairs = 0;
        std::int64_t finalTicks = 0;
        std::string name;
        std::uint64_t nextRecordId = 1;
        std::uint64_t currentRecordId = 0;

        const auto elapsedTicks = [&](TimePoint now) -> std::int64_t
        {
            if (!timerRunning)
                return finalTicks;
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - timerStart).count() / 100;
        };

        const auto startGame = [&]()
        {
            music.stop();
            flipSound.stop();
            pointSound.stop();

            for (std::size_t i = 0; i < cards.size(); ++i)
                cards[i] = Card{static_cast<int>(i / 2) + 1, false, false};
            std::shuffle(cards.begin(), cards.end(), random);

            first = -1;
            second = -1;
            moves = 0;
            matchedPairs = 0;
            finalTicks = 0;
            timerStart = {};
            mismatchDeadline = {};
            timerRunning = false;
            mismatchPending = false;
            nameConfirmed = false;
            name.clear();
            currentRecordId = 0;
            screen = Screen::Play;
            music.play();
        };

        const auto advance = [&](TimePoint now)
        {
            if (screen == Screen::Title &&
                now - titleStart >= std::chrono::seconds(2))
            {
                startGame();
            }

            if (screen == Screen::Play &&
                mismatchPending && now >= mismatchDeadline)
            {
                cards[static_cast<std::size_t>(first)].revealed = false;
                cards[static_cast<std::size_t>(second)].revealed = false;
                first = -1;
                second = -1;
                mismatchPending = false;
            }
        };

        const auto confirmName = [&]()
        {
            if (name.empty() || nameConfirmed)
                return;

            currentRecordId = nextRecordId++;
            records.push_back({name, moves, finalTicks, currentRecordId});
            std::stable_sort(records.begin(), records.end(),
                             [](const Record& a, const Record& b)
                             {
                                 if (a.moves != b.moves)
                                     return a.moves < b.moves;
                                 return a.ticks < b.ticks;
                             });
            nameConfirmed = true;
        };

        const auto flipCard = [&](std::size_t index, TimePoint now)
        {
            if (screen != Screen::Play || mismatchPending)
                return;

            Card& card = cards[index];
            if (card.revealed || card.matched)
                return;

            if (!timerRunning)
            {
                timerStart = now;
                timerRunning = true;
            }

            card.revealed = true;
            flipSound.play();

            if (first < 0)
            {
                first = static_cast<int>(index);
                return;
            }

            second = static_cast<int>(index);
            ++moves;

            Card& firstCard = cards[static_cast<std::size_t>(first)];
            if (firstCard.face == card.face)
            {
                firstCard.matched = true;
                card.matched = true;
                ++matchedPairs;
                first = -1;
                second = -1;
                pointSound.play();

                if (matchedPairs == 8)
                {
                    finalTicks = elapsedTicks(now);
                    timerRunning = false;
                    screen = Screen::Result;
                    music.stop();
                }
            }
            else
            {
                mismatchPending = true;
                mismatchDeadline = now + std::chrono::milliseconds(800);
            }
        };

        while (window.isOpen())
        {
            advance(Clock::now());

            while (const auto event = window.pollEvent())
            {
                const TimePoint now = Clock::now();
                advance(now);

                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                    break;
                }

                if (const auto* resized = event->getIf<sf::Event::Resized>())
                    resizeView(resized->size);

                if (const auto* key = event->getIf<sf::Event::KeyPressed>())
                {
                    if (key->code == sf::Keyboard::Key::Escape)
                    {
                        window.close();
                        break;
                    }

                    if (screen == Screen::Title)
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
                                if (!name.empty())
                                    name.pop_back();
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

                if (const auto* entered = event->getIf<sf::Event::TextEntered>())
                {
                    if (screen == Screen::Result &&
                        !nameConfirmed && name.size() < 8)
                    {
                        auto character = entered->unicode;
                        if (character >= U'a' && character <= U'z')
                            character = character - U'a' + U'A';

                        if ((character >= U'A' && character <= U'Z') ||
                            (character >= U'0' && character <= U'9'))
                        {
                            name.push_back(static_cast<char>(character));
                        }
                    }
                }

                if (const auto* mouse =
                        event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouse->button == sf::Mouse::Button::Left &&
                        screen == Screen::Play && !mismatchPending)
                    {
                        const auto size = window.getSize();
                        if (size.x == 0 || size.y == 0)
                            continue;

                        const sf::Vector2f point =
                            window.mapPixelToCoords(mouse->position, view);

                        for (std::size_t i = 0; i < cards.size(); ++i)
                        {
                            if (cardBounds(i).contains(point))
                            {
                                flipCard(i, now);
                                break;
                            }
                        }
                    }
                }
            }

            if (!window.isOpen())
                break;

            const TimePoint now = Clock::now();
            advance(now);

            window.clear(sf::Color::Black);
            window.setView(view);
            window.draw(background);

            if (screen == Screen::Title)
            {
                drawText("PAIRS", 48, {Width / 2.f, 190.f},
                         sf::Color::White, true);
                drawText("PRESS ENTER", 32, {Width / 2.f, 290.f},
                         sf::Color::White, true);
            }
            else if (screen == Screen::Play)
            {
                drawText("MOVES " + std::to_string(moves) +
                             "   TIME " + timeString(elapsedTicks(now)),
                         32, {Width / 2.f, 8.f}, sf::Color::White, true);

                sf::Sprite sprite(cardsTexture);
                for (std::size_t i = 0; i < cards.size(); ++i)
                {
                    const Card& card = cards[i];
                    const int tile = card.revealed ? card.face : 0;
                    sprite.setTextureRect(
                        sf::IntRect({tile * 80, 0}, {80, 100}));
                    sprite.setPosition(cardBounds(i).position);
                    window.draw(sprite);
                }
            }
            else
            {
                drawText("CLEAR", 48, {Width / 2.f, 24.f},
                         sf::Color::White, true);
                drawText("MOVES " + std::to_string(moves) +
                             "   TIME " + timeString(finalTicks),
                         32, {Width / 2.f, 96.f}, sf::Color::White, true);
                drawText("NAME: " + name, 32, {Width / 2.f, 152.f},
                         sf::Color::White, true);

                if (nameConfirmed)
                {
                    const auto recordLine = [](std::size_t rank,
                                               const Record& record)
                    {
                        return std::to_string(rank) + ". " + record.name +
                               "   MOVES " + std::to_string(record.moves) +
                               "   TIME " + timeString(record.ticks);
                    };

                    const std::size_t count =
                        std::min<std::size_t>(5, records.size());

                    for (std::size_t i = 0; i < count; ++i)
                    {
                        const bool current = records[i].id == currentRecordId;
                        drawText(recordLine(i + 1, records[i]), 16,
                                 {120.f, 220.f + static_cast<float>(i) * 32.f},
                                 current ? Highlight : sf::Color::White);
                    }

                    for (std::size_t i = 5; i < records.size(); ++i)
                    {
                        if (records[i].id == currentRecordId)
                        {
                            drawText(recordLine(i + 1, records[i]), 16,
                                     {120.f, 396.f}, Highlight);
                            break;
                        }
                    }
                }

                drawText("R: RESTART  ESC: QUIT", 16,
                         {Width / 2.f, 496.f}, sf::Color::White, true);
            }

            window.display();
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
```