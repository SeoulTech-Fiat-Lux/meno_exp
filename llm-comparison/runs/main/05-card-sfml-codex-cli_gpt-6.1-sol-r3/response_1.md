```cpp
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
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
    int value = 0;
    bool faceUp = false;
    bool matched = false;
};

struct Record
{
    std::string name;
    int moves = 0;
    std::int64_t ticks = 0;
    std::uint64_t id = 0;
};

std::string formatTime(std::int64_t ticks)
{
    return std::to_string(ticks / 10) + "." + std::to_string(ticks % 10);
}

std::string formatScore(int moves, std::int64_t ticks)
{
    return "MOVES " + std::to_string(moves) + "   TIME " + formatTime(ticks);
}

std::string formatRecord(std::size_t rank, const Record& record)
{
    return std::to_string(rank) + ". " + record.name + "   " +
           formatScore(record.moves, record.ticks);
}

sf::FloatRect cardBounds(std::size_t index)
{
    return {{BoardX + static_cast<float>(index % 4) * (CardWidth + Gap),
             BoardY + static_cast<float>(index / 4) * (CardHeight + Gap)},
            {CardWidth, CardHeight}};
}
}

int main()
{
    try
    {
        sf::Font font;
        sf::Texture texture;
        sf::SoundBuffer flipBuffer;
        sf::SoundBuffer pointBuffer;
        sf::Music music;

        const auto failed = [](const char* path)
        {
            std::cerr << "FAILED TO LOAD: " << path << '\n';
            return 1;
        };

        if (!font.openFromFile("assets/fonts/game.ttf"))
            return failed("assets/fonts/game.ttf");
        if (!texture.loadFromFile("assets/textures/cards.png"))
            return failed("assets/textures/cards.png");
        if (!flipBuffer.loadFromFile("assets/sfx/flip.wav"))
            return failed("assets/sfx/flip.wav");
        if (!pointBuffer.loadFromFile("assets/sfx/point.wav"))
            return failed("assets/sfx/point.wav");
        if (!music.openFromFile("assets/music/bgm_card.ogg"))
            return failed("assets/music/bgm_card.ogg");

        sf::Sound flipSound(flipBuffer);
        sf::Sound pointSound(pointBuffer);
        music.setVolume(40.f);
        music.setLooping(true);

        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "PAIRS");
        window.setFramerateLimit(60);
        window.setKeyRepeatEnabled(false);

        sf::View view(sf::FloatRect({0.f, 0.f}, {Width, Height}));
        bool drawable = true;

        const auto resizeView = [&](sf::Vector2u size)
        {
            drawable = size.x != 0 && size.y != 0;
            if (!drawable)
                return;

            const float aspect = static_cast<float>(size.x) /
                                 static_cast<float>(size.y);
            constexpr float targetAspect = Width / Height;
            if (aspect > targetAspect)
            {
                const float width = targetAspect / aspect;
                view.setViewport({{(1.f - width) / 2.f, 0.f}, {width, 1.f}});
            }
            else
            {
                const float height = aspect / targetAspect;
                view.setViewport({{0.f, (1.f - height) / 2.f}, {1.f, height}});
            }
            window.setView(view);
        };
        resizeView(window.getSize());

        sf::Text text(font);
        const auto drawText = [&](const std::string& string, float x, float y,
                                  unsigned int size, sf::Color color,
                                  bool centered = false)
        {
            text.setString(string);
            text.setCharacterSize(size);
            text.setFillColor(color);
            text.setOrigin({0.f, 0.f});
            if (centered)
            {
                const auto bounds = text.getLocalBounds();
                text.setOrigin({bounds.position.x + bounds.size.x / 2.f,
                                bounds.position.y});
            }
            text.setPosition({x, y});
            window.draw(text);
        };

        sf::Sprite sprite(texture);
        sf::RectangleShape background({Width, Height});
        background.setFillColor(Background);

        std::mt19937 random(std::random_device{}());
        std::array<Card, 16> cards{};
        std::vector<Record> records;
        std::string name;
        std::uint64_t nextRecordId = 0;
        std::uint64_t currentRecordId = 0;
        bool nameConfirmed = false;

        Screen screen = Screen::Title;
        int first = -1;
        int second = -1;
        int moves = 0;
        int pairs = 0;
        bool timerStarted = false;
        bool waiting = false;
        double timerStart = 0.0;
        double hideAt = 0.0;
        std::int64_t finalTicks = 0;

        sf::Clock clock;
        const auto now = [&]()
        {
            return static_cast<double>(clock.getElapsedTime().asMicroseconds()) /
                   1000000.0;
        };
        const double titleStart = now();

        const auto elapsedTicks = [&](double time) -> std::int64_t
        {
            if (!timerStarted)
                return 0;
            return static_cast<std::int64_t>(
                std::floor(std::max(0.0, time - timerStart) * 10.0));
        };

        const auto startGame = [&]()
        {
            std::array<int, 16> values{};
            for (std::size_t i = 0; i < values.size(); ++i)
                values[i] = static_cast<int>(i / 2) + 1;
            std::shuffle(values.begin(), values.end(), random);

            for (std::size_t i = 0; i < cards.size(); ++i)
                cards[i] = Card{values[i], false, false};

            first = second = -1;
            moves = pairs = 0;
            timerStarted = waiting = false;
            timerStart = hideAt = 0.0;
            finalTicks = 0;
            name.clear();
            nameConfirmed = false;
            currentRecordId = 0;
            flipSound.stop();
            pointSound.stop();
            music.stop();
            music.play();
            screen = Screen::Play;
        };

        const auto update = [&](double time)
        {
            if (screen == Screen::Title && time - titleStart >= 2.0)
                startGame();

            if (screen == Screen::Play && waiting && time >= hideAt)
            {
                cards[static_cast<std::size_t>(first)].faceUp = false;
                cards[static_cast<std::size_t>(second)].faceUp = false;
                first = second = -1;
                waiting = false;
            }
        };

        const auto confirmName = [&]()
        {
            if (name.empty())
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

        while (window.isOpen())
        {
            update(now());

            while (const auto event = window.pollEvent())
            {
                const double eventTime = now();
                update(eventTime);

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
                        if (!nameConfirmed)
                        {
                            if (key->code == sf::Keyboard::Key::Backspace &&
                                !name.empty())
                                name.pop_back();
                            else if (key->code == sf::Keyboard::Key::Enter)
                                confirmName();
                        }
                        else if (key->code == sf::Keyboard::Key::R)
                        {
                            startGame();
                        }
                    }
                }

                if (const auto* entered = event->getIf<sf::Event::TextEntered>())
                {
                    if (screen == Screen::Result && !nameConfirmed &&
                        name.size() < 8)
                    {
                        auto character = entered->unicode;
                        if (character >= U'a' && character <= U'z')
                            character = character - U'a' + U'A';
                        if ((character >= U'A' && character <= U'Z') ||
                            (character >= U'0' && character <= U'9'))
                            name.push_back(static_cast<char>(character));
                    }
                }

                if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (screen != Screen::Play || waiting || !drawable ||
                        click->button != sf::Mouse::Button::Left)
                        continue;

                    if (!window.getViewport(view).contains(click->position))
                        continue;

                    const auto position = window.mapPixelToCoords(click->position, view);
                    for (std::size_t i = 0; i < cards.size(); ++i)
                    {
                        Card& card = cards[i];
                        if (!cardBounds(i).contains(position) ||
                            card.faceUp || card.matched)
                            continue;

                        if (!timerStarted)
                        {
                            timerStarted = true;
                            timerStart = eventTime;
                        }

                        card.faceUp = true;
                        flipSound.play();

                        if (first < 0)
                        {
                            first = static_cast<int>(i);
                        }
                        else
                        {
                            second = static_cast<int>(i);
                            ++moves;
                            Card& previous = cards[static_cast<std::size_t>(first)];
                            if (previous.value == card.value)
                            {
                                previous.matched = card.matched = true;
                                ++pairs;
                                pointSound.play();
                                first = second = -1;

                                if (pairs == 8)
                                {
                                    finalTicks = elapsedTicks(eventTime);
                                    music.stop();
                                    screen = Screen::Result;
                                }
                            }
                            else
                            {
                                waiting = true;
                                hideAt = eventTime + 0.8;
                            }
                        }
                        break;
                    }
                }
            }

            if (!window.isOpen())
                break;

            const double renderTime = now();
            update(renderTime);
            window.clear(sf::Color::Black);

            if (drawable)
            {
                window.setView(view);
                window.draw(background);

                if (screen == Screen::Title)
                {
                    drawText("PAIRS", Width / 2.f, 190.f, 48,
                             sf::Color::White, true);
                    drawText("PRESS ENTER", Width / 2.f, 290.f, 32,
                             sf::Color::White, true);
                }
                else if (screen == Screen::Play)
                {
                    drawText(formatScore(moves, elapsedTicks(renderTime)),
                             Width / 2.f, 12.f, 16, sf::Color::White, true);

                    for (std::size_t i = 0; i < cards.size(); ++i)
                    {
                        const int tile = cards[i].faceUp ? cards[i].value : 0;
                        sprite.setTextureRect({{tile * 80, 0}, {80, 100}});
                        sprite.setPosition(cardBounds(i).position);
                        window.draw(sprite);
                    }
                }
                else
                {
                    drawText("CLEAR", Width / 2.f, 24.f, 48,
                             sf::Color::White, true);
                    drawText(formatScore(moves, finalTicks),
                             Width / 2.f, 94.f, 32, sf::Color::White, true);

                    if (!nameConfirmed)
                    {
                        drawText("NAME: " + name, Width / 2.f, 190.f, 32,
                                 sf::Color::White, true);
                    }
                    else
                    {
                        std::size_t currentRank = 0;
                        const std::size_t count =
                            std::min<std::size_t>(5, records.size());

                        for (std::size_t i = 0; i < records.size(); ++i)
                        {
                            if (records[i].id == currentRecordId)
                                currentRank = i;
                        }

                        for (std::size_t i = 0; i < count; ++i)
                        {
                            drawText(formatRecord(i + 1, records[i]),
                                     160.f, 172.f + static_cast<float>(i) * 40.f,
                                     16, records[i].id == currentRecordId
                                             ? Highlight : sf::Color::White);
                        }

                        if (currentRank >= 5)
                        {
                            drawText(formatRecord(currentRank + 1,
                                                  records[currentRank]),
                                     160.f, 388.f, 16, Highlight);
                        }
                    }

                    drawText("R: RESTART  ESC: QUIT", Width / 2.f, 478.f,
                             32, sf::Color::White, true);
                }
            }

            window.display();
        }

        music.stop();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```