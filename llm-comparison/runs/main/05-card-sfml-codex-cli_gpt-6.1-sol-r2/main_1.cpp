#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
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
    int value{};
    bool faceUp{};
    bool matched{};
};

struct Record
{
    std::string name;
    int moves{};
    std::int64_t ticks{};
    std::uint64_t id{};
};

std::string formatTime(std::int64_t ticks)
{
    return std::to_string(ticks / 10) + "." + std::to_string(ticks % 10);
}

std::string scoreText(int moves, std::int64_t ticks)
{
    return "MOVES " + std::to_string(moves) + "   TIME " + formatTime(ticks);
}

std::string recordText(std::size_t rank, const Record& record)
{
    return std::to_string(rank) + ". " + record.name + "   " +
           scoreText(record.moves, record.ticks);
}

void drawText(sf::RenderTarget& target,
              const sf::Font& font,
              const std::string& string,
              unsigned int size,
              sf::Vector2f position,
              bool centered = false,
              sf::Color color = sf::Color::White)
{
    sf::Text text(font, string, size);
    text.setFillColor(color);

    const auto bounds = text.getLocalBounds();
    text.setPosition({
        position.x - bounds.position.x - (centered ? bounds.size.x / 2.f : 0.f),
        position.y - bounds.position.y
    });
    target.draw(text);
}

int run()
{
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "PAIRS");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::Font font;
    sf::Texture texture;
    sf::SoundBuffer flipBuffer;
    sf::SoundBuffer pointBuffer;
    sf::Music music;

    const auto assetFailure = [](const char* path)
    {
        std::cerr << "FAILED TO LOAD: " << path << '\n';
        return 1;
    };

    if (!font.openFromFile("assets/fonts/game.ttf"))
        return assetFailure("assets/fonts/game.ttf");
    if (!texture.loadFromFile("assets/textures/cards.png"))
        return assetFailure("assets/textures/cards.png");
    if (!flipBuffer.loadFromFile("assets/sfx/flip.wav"))
        return assetFailure("assets/sfx/flip.wav");
    if (!pointBuffer.loadFromFile("assets/sfx/point.wav"))
        return assetFailure("assets/sfx/point.wav");
    if (!music.openFromFile("assets/music/bgm_card.ogg"))
        return assetFailure("assets/music/bgm_card.ogg");

    texture.setSmooth(false);
    music.setVolume(40.f);
    music.setLooping(true);

    // Independent voices allow closely spaced flips to overlap.
    std::vector<sf::Sound> voices;
    voices.reserve(16);
    for (int i = 0; i < 16; ++i)
        voices.emplace_back(flipBuffer);
    std::size_t nextVoice = 0;

    const auto playSound = [&](const sf::SoundBuffer& buffer)
    {
        auto& voice = voices[nextVoice];
        nextVoice = (nextVoice + 1) % voices.size();
        voice.stop();
        voice.setBuffer(buffer);
        voice.play();
    };

    sf::View view(sf::FloatRect({0.f, 0.f}, {Width, Height}));
    const auto resizeView = [&](sf::Vector2u size)
    {
        if (size.x == 0 || size.y == 0)
            return;

        const float ratio = static_cast<float>(size.x) /
                            static_cast<float>(size.y);
        constexpr float desired = Width / Height;

        if (ratio > desired)
        {
            const float fraction = desired / ratio;
            view.setViewport(sf::FloatRect(
                {(1.f - fraction) / 2.f, 0.f}, {fraction, 1.f}));
        }
        else
        {
            const float fraction = ratio / desired;
            view.setViewport(sf::FloatRect(
                {0.f, (1.f - fraction) / 2.f}, {1.f, fraction}));
        }
        window.setView(view);
    };
    resizeView(window.getSize());

    sf::RectangleShape background({Width, Height});
    background.setFillColor(Background);
    sf::Sprite cardSprite(texture);

    std::mt19937 random(std::random_device{}());
    std::array<Card, 16> cards{};
    std::vector<Record> records;

    Screen screen = Screen::Title;
    const TimePoint titleStarted = Clock::now();
    TimePoint timerStarted{};
    TimePoint mismatchStarted{};

    bool timerRunning = false;
    bool waiting = false;
    bool confirmed = false;
    int first = -1;
    int second = -1;
    int moves = 0;
    int pairs = 0;
    std::int64_t finalTicks = 0;
    std::string name;
    std::uint64_t nextRecordId = 0;
    std::uint64_t currentRecordId = 0;

    const auto elapsedTicks = [&](TimePoint now) -> std::int64_t
    {
        if (!timerRunning)
            return 0;
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   now - timerStarted).count() / 100000;
    };

    const auto startGame = [&]
    {
        music.stop();
        for (auto& voice : voices)
            voice.stop();
        nextVoice = 0;

        std::array<int, 16> values{};
        for (int i = 0; i < 16; ++i)
            values[static_cast<std::size_t>(i)] = i / 2 + 1;
        std::shuffle(values.begin(), values.end(), random);

        for (std::size_t i = 0; i < cards.size(); ++i)
            cards[i] = Card{values[i], false, false};

        first = -1;
        second = -1;
        moves = 0;
        pairs = 0;
        waiting = false;
        timerRunning = false;
        timerStarted = TimePoint{};
        mismatchStarted = TimePoint{};
        finalTicks = 0;
        confirmed = false;
        name.clear();
        screen = Screen::Play;
        music.play();
    };

    const auto update = [&](TimePoint now)
    {
        if (screen == Screen::Title &&
            now - titleStarted >= std::chrono::seconds(2))
        {
            startGame();
        }

        if (screen == Screen::Play && waiting &&
            now - mismatchStarted >= std::chrono::milliseconds(800))
        {
            cards[static_cast<std::size_t>(first)].faceUp = false;
            cards[static_cast<std::size_t>(second)].faceUp = false;
            first = second = -1;
            waiting = false;
        }
    };

    const auto confirmName = [&]
    {
        if (confirmed || name.empty())
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

        confirmed = true;
    };

    while (window.isOpen())
    {
        update(Clock::now());

        while (const auto event = window.pollEvent())
        {
            const TimePoint now = Clock::now();
            update(now);

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

                if (screen == Screen::Title &&
                    key->code == sf::Keyboard::Key::Enter)
                {
                    startGame();
                }
                else if (screen == Screen::Result)
                {
                    if (!confirmed)
                    {
                        if (key->code == sf::Keyboard::Key::Backspace &&
                            !name.empty())
                        {
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

            if (const auto* text = event->getIf<sf::Event::TextEntered>())
            {
                if (screen == Screen::Result && !confirmed && name.size() < 8)
                {
                    auto character = text->unicode;
                    if (character >= U'a' && character <= U'z')
                        character = character - U'a' + U'A';

                    if ((character >= U'A' && character <= U'Z') ||
                        (character >= U'0' && character <= U'9'))
                    {
                        name.push_back(static_cast<char>(character));
                    }
                }
            }

            if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (screen != Screen::Play || waiting ||
                    mouse->button != sf::Mouse::Button::Left)
                {
                    continue;
                }

                const auto size = window.getSize();
                if (size.x == 0 || size.y == 0 ||
                    !window.getViewport(view).contains(mouse->position))
                {
                    continue;
                }

                const auto position = window.mapPixelToCoords(mouse->position, view);

                for (std::size_t i = 0; i < cards.size(); ++i)
                {
                    const sf::FloatRect bounds(
                        {BoardX + static_cast<float>(i % 4) * (CardWidth + Gap),
                         BoardY + static_cast<float>(i / 4) * (CardHeight + Gap)},
                        {CardWidth, CardHeight});

                    Card& card = cards[i];
                    if (!bounds.contains(position) || card.faceUp || card.matched)
                        continue;

                    if (!timerRunning)
                    {
                        timerRunning = true;
                        timerStarted = now;
                    }

                    card.faceUp = true;
                    playSound(flipBuffer);

                    if (first < 0)
                    {
                        first = static_cast<int>(i);
                    }
                    else
                    {
                        second = static_cast<int>(i);
                        ++moves;

                        if (cards[static_cast<std::size_t>(first)].value ==
                            card.value)
                        {
                            cards[static_cast<std::size_t>(first)].matched = true;
                            card.matched = true;
                            ++pairs;
                            playSound(pointBuffer);
                            first = second = -1;

                            if (pairs == 8)
                            {
                                finalTicks = elapsedTicks(now);
                                timerRunning = false;
                                music.stop();
                                screen = Screen::Result;
                            }
                        }
                        else
                        {
                            waiting = true;
                            mismatchStarted = now;
                        }
                    }
                    break;
                }
            }
        }

        if (!window.isOpen())
            break;

        const TimePoint now = Clock::now();
        update(now);

        window.clear(sf::Color::Black);
        window.setView(view);
        window.draw(background);

        if (screen == Screen::Title)
        {
            drawText(window, font, "PAIRS", 48, {480.f, 190.f}, true);
            drawText(window, font, "PRESS ENTER", 32, {480.f, 285.f}, true);
        }
        else if (screen == Screen::Play)
        {
            drawText(window, font, scoreText(moves, elapsedTicks(now)),
                     32, {480.f, 8.f}, true);

            for (std::size_t i = 0; i < cards.size(); ++i)
            {
                const int cell = cards[i].faceUp ? cards[i].value : 0;
                cardSprite.setTextureRect(
                    sf::IntRect({cell * 80, 0}, {80, 100}));
                cardSprite.setPosition({
                    BoardX + static_cast<float>(i % 4) * (CardWidth + Gap),
                    BoardY + static_cast<float>(i / 4) * (CardHeight + Gap)
                });
                window.draw(cardSprite);
            }
        }
        else
        {
            drawText(window, font, "CLEAR", 48, {480.f, 26.f}, true);
            drawText(window, font, scoreText(moves, finalTicks),
                     32, {480.f, 100.f}, true);
            drawText(window, font, "NAME: " + name,
                     32, {480.f, 160.f}, true);

            if (confirmed)
            {
                const std::size_t count = std::min<std::size_t>(5, records.size());
                std::size_t currentRank = 0;

                for (std::size_t i = 0; i < records.size(); ++i)
                {
                    if (records[i].id == currentRecordId)
                        currentRank = i + 1;
                }

                for (std::size_t i = 0; i < count; ++i)
                {
                    const auto color = records[i].id == currentRecordId
                                           ? Highlight
                                           : sf::Color::White;
                    drawText(window, font, recordText(i + 1, records[i]),
                             16, {120.f, 230.f + static_cast<float>(i) * 34.f},
                             false, color);
                }

                if (currentRank > 5)
                {
                    drawText(window, font,
                             recordText(currentRank, records[currentRank - 1]),
                             16, {120.f, 414.f}, false, Highlight);
                }
            }

            drawText(window, font, "R: RESTART  ESC: QUIT",
                     32, {480.f, 482.f}, true);
        }

        window.display();
    }

    music.stop();
    return 0;
}
}

int main()
{
    try
    {
        return run();
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
