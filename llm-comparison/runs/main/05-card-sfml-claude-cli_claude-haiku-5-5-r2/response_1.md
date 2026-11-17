```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr float kW = 960.f;
constexpr float kH = 540.f;

constexpr float kCardW = 80.f;
constexpr float kCardH = 100.f;
constexpr float kGap   = 12.f;
constexpr int   kCols  = 4;
constexpr int   kCards = 16;

constexpr float kBoardX = (kW - (4.f * kCardW + 3.f * kGap)) / 2.f;
constexpr float kBoardY = (kH - (4.f * kCardH + 3.f * kGap)) / 2.f;

const sf::Color kBgColor(0x1B, 0x2A, 0x1F);
const sf::Color kTextColor(0xFF, 0xFF, 0xFF);
const sf::Color kHighlight(0xFF, 0xD5, 0x4A);

enum class Scene { Title, Play, Result };

struct Card
{
    int  value   = 0;
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

struct Assets
{
    sf::Font        font;
    sf::Texture     cards;
    sf::SoundBuffer flipBuf;
    sf::SoundBuffer pointBuf;
    sf::Music       bgm;
    std::optional<sf::Sound> flipSnd;
    std::optional<sf::Sound> pointSnd;
};

struct Game
{
    Scene scene      = Scene::Title;
    float titleTimer = 0.f;

    std::vector<Card> cards;
    int    moves        = 0;
    int    pairsFound   = 0;
    double elapsed      = 0.0;
    bool   timerStarted = false;
    int    firstIdx     = -1;
    int    secondIdx    = -1;
    float  mismatchTimer = 0.f;

    int         finalMoves  = 0;
    int         finalTenths = 0;
    std::string nameInput;
    bool        confirmed  = false;
    int         currentSeq = -1;
    std::vector<Record> records;
    int nextSeq = 0;

    std::mt19937 rng{std::random_device{}()};
};

bool loadAssets(Assets& a)
{
    auto fail = [](const char* path)
    {
        std::cerr << "failed to load: " << path << '\n';
        return false;
    };

    if (!a.font.openFromFile("assets/fonts/game.ttf"))
        return fail("assets/fonts/game.ttf");
    if (!a.cards.loadFromFile("assets/textures/cards.png"))
        return fail("assets/textures/cards.png");
    if (!a.flipBuf.loadFromFile("assets/sfx/flip.wav"))
        return fail("assets/sfx/flip.wav");
    if (!a.pointBuf.loadFromFile("assets/sfx/point.wav"))
        return fail("assets/sfx/point.wav");
    if (!a.bgm.openFromFile("assets/music/bgm_card.ogg"))
        return fail("assets/music/bgm_card.ogg");

    a.flipSnd.emplace(a.flipBuf);
    a.pointSnd.emplace(a.pointBuf);
    a.bgm.setLooping(true);
    a.bgm.setVolume(40.f);
    return true;
}

sf::Vector2f cardPos(int i)
{
    const float x = kBoardX + static_cast<float>(i % kCols) * (kCardW + kGap);
    const float y = kBoardY + static_cast<float>(i / kCols) * (kCardH + kGap);
    return {x, y};
}

int currentTenths(const Game& g)
{
    return static_cast<int>(g.elapsed * 10.0);
}

std::string fmtTime(int tenths)
{
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

void updateView(const sf::RenderWindow& window, sf::View& view)
{
    const sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0)
        return;

    const float winW   = static_cast<float>(size.x);
    const float winH   = static_cast<float>(size.y);
    const float aspect = kW / kH;

    float vw = 1.f;
    float vh = 1.f;
    if (winW / winH > aspect)
        vw = winH * aspect / winW;
    else
        vh = winW / aspect / winH;

    view.setViewport(sf::FloatRect({(1.f - vw) / 2.f, (1.f - vh) / 2.f}, {vw, vh}));
}

void startGame(Game& g, Assets& a)
{
    std::vector<int> values;
    for (int v = 1; v <= 8; ++v)
    {
        values.push_back(v);
        values.push_back(v);
    }
    std::shuffle(values.begin(), values.end(), g.rng);

    g.cards.clear();
    for (int v : values)
        g.cards.push_back(Card{v, false, false});

    g.moves         = 0;
    g.pairsFound    = 0;
    g.elapsed       = 0.0;
    g.timerStarted  = false;
    g.firstIdx      = -1;
    g.secondIdx     = -1;
    g.mismatchTimer = 0.f;
    g.nameInput.clear();
    g.confirmed     = false;
    g.currentSeq    = -1;
    g.scene         = Scene::Play;

    a.bgm.play();
}

void finishGame(Game& g, Assets& a)
{
    g.finalMoves  = g.moves;
    g.finalTenths = currentTenths(g);
    g.scene       = Scene::Result;
    g.nameInput.clear();
    g.confirmed   = false;
    g.currentSeq  = -1;
    a.bgm.stop();
}

void onPlayClick(Game& g, Assets& a, sf::Vector2f world)
{
    if (g.mismatchTimer > 0.f)
        return;

    int idx = -1;
    for (int i = 0; i < kCards; ++i)
    {
        const sf::FloatRect box(cardPos(i), {kCardW, kCardH});
        if (box.contains(world))
        {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;

    Card& card = g.cards[static_cast<std::size_t>(idx)];
    if (card.faceUp || card.matched)
        return;

    card.faceUp = true;
    if (a.flipSnd)
        a.flipSnd->play();

    g.timerStarted = true;

    if (g.firstIdx < 0)
    {
        g.firstIdx = idx;
        return;
    }

    g.secondIdx = idx;
    ++g.moves;

    Card& first = g.cards[static_cast<std::size_t>(g.firstIdx)];
    if (first.value == card.value)
    {
        first.matched = true;
        card.matched  = true;
        ++g.pairsFound;
        g.firstIdx  = -1;
        g.secondIdx = -1;

        if (a.pointSnd)
            a.pointSnd->play();

        if (g.pairsFound == 8)
            finishGame(g, a);
    }
    else
    {
        g.mismatchTimer = 0.8f;
    }
}

void update(Game& g, Assets& a, float dt)
{
    if (g.scene == Scene::Title)
    {
        g.titleTimer += dt;
        if (g.titleTimer >= 2.f)
            startGame(g, a);
        return;
    }

    if (g.scene == Scene::Play)
    {
        if (g.timerStarted)
            g.elapsed += static_cast<double>(dt);

        if (g.mismatchTimer > 0.f)
        {
            g.mismatchTimer -= dt;
            if (g.mismatchTimer <= 0.f)
            {
                g.mismatchTimer = 0.f;
                g.cards[static_cast<std::size_t>(g.firstIdx)].faceUp  = false;
                g.cards[static_cast<std::size_t>(g.secondIdx)].faceUp = false;
                g.firstIdx  = -1;
                g.secondIdx = -1;
            }
        }
    }
}

void addNameChar(Game& g, std::uint32_t code)
{
    if (code >= 128)
        return;
    const unsigned char c = static_cast<unsigned char>(code);
    if (!std::isalnum(c) || g.nameInput.size() >= 8)
        return;
    g.nameInput.push_back(static_cast<char>(std::toupper(c)));
}

void confirmName(Game& g)
{
    if (g.nameInput.empty())
        return;

    Record r{g.nameInput, g.finalMoves, g.finalTenths, g.nextSeq++};
    g.currentSeq = r.seq;
    g.records.push_back(r);
    g.confirmed = true;
}

void drawCentered(sf::RenderWindow& w, const sf::Font& font, const std::string& s,
                  unsigned int size, float y, sf::Color color)
{
    sf::Text text(font, sf::String(std::string_view(s)), size);
    text.setFillColor(color);
    const sf::FloatRect b = text.getLocalBounds();
    text.setPosition({(kW - b.size.x) / 2.f - b.position.x, y});
    w.draw(text);
}

std::string rankLine(std::size_t rank, const Record& r)
{
    return std::to_string(rank) + ". " + r.name + "   MOVES " + std::to_string(r.moves)
         + "   TIME " + fmtTime(r.tenths);
}

void drawResult(sf::RenderWindow& w, const sf::Font& f, const Game& g)
{
    drawCentered(w, f, "CLEAR", 48, 40.f, kTextColor);
    drawCentered(w, f, "MOVES " + std::to_string(g.finalMoves) + "   TIME " + fmtTime(g.finalTenths),
                 32, 110.f, kTextColor);

    if (!g.confirmed)
    {
        drawCentered(w, f, "NAME: " + g.nameInput, 32, 180.f, kTextColor);
    }
    else
    {
        std::vector<Record> sorted = g.records;
        std::sort(sorted.begin(), sorted.end(), [](const Record& a, const Record& b)
        {
            if (a.moves != b.moves)
                return a.moves < b.moves;
            if (a.tenths != b.tenths)
                return a.tenths < b.tenths;
            return a.seq < b.seq;
        });

        const std::size_t shown = std::min<std::size_t>(5, sorted.size());
        std::size_t myIndex = sorted.size();

        for (std::size_t i = 0; i < sorted.size(); ++i)
        {
            if (sorted[i].seq == g.currentSeq)
            {
                myIndex = i;
                break;
            }
        }

        for (std::size_t i = 0; i < shown; ++i)
        {
            const bool mine = sorted[i].seq == g.currentSeq;
            drawCentered(w, f, rankLine(i + 1, sorted[i]), 32,
                         180.f + static_cast<float>(i) * 40.f,
                         mine ? kHighlight : kTextColor);
        }

        if (myIndex < sorted.size() && myIndex >= 5)
        {
            drawCentered(w, f, rankLine(myIndex + 1, sorted[myIndex]), 32, 400.f, kHighlight);
        }
    }

    drawCentered(w, f, "R: RESTART  ESC: QUIT", 16, 500.f, kTextColor);
}

void draw(sf::RenderWindow& window, const Assets& a, const Game& g)
{
    window.clear(sf::Color::Black);

    sf::RectangleShape bg(sf::Vector2f{kW, kH});
    bg.setFillColor(kBgColor);
    window.draw(bg);

    switch (g.scene)
    {
    case Scene::Title:
        drawCentered(window, a.font, "PAIRS", 48, 190.f, kTextColor);
        drawCentered(window, a.font, "PRESS ENTER", 32, 290.f, kTextColor);
        break;

    case Scene::Play:
    {
        const std::string hud = "MOVES " + std::to_string(g.moves) + "   TIME "
                              + fmtTime(currentTenths(g));
        drawCentered(window, a.font, hud, 32, 8.f, kTextColor);

        for (int i = 0; i < kCards; ++i)
        {
            const Card& c = g.cards[static_cast<std::size_t>(i)];
            const int cell = c.faceUp ? c.value : 0;
            sf::Sprite spr(a.cards, sf::IntRect(sf::Vector2i{cell * 80, 0}, sf::Vector2i{80, 100}));
            spr.setPosition(cardPos(i));
            window.draw(spr);
        }
        break;
    }

    case Scene::Result:
        drawResult(window, a.font, g);
        break;
    }
}
} // namespace

int main()
{
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "PAIRS");
    window.setFramerateLimit(60);

    Assets assets;
    if (!loadAssets(assets))
        return 1;

    sf::View view(sf::FloatRect({0.f, 0.f}, {kW, kH}));
    Game game;
    sf::Clock clock;

    while (window.isOpen())
    {
        updateView(window, view);

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                    break;
                }

                if (game.scene == Scene::Title)
                {
                    if (key->code == sf::Keyboard::Key::Enter)
                        startGame(game, assets);
                }
                else if (game.scene == Scene::Result)
                {
                    if (game.confirmed)
                    {
                        if (key->code == sf::Keyboard::Key::R)
                            startGame(game, assets);
                    }
                    else
                    {
                        if (key->code == sf::Keyboard::Key::Enter)
                            confirmName(game);
                        else if (key->code == sf::Keyboard::Key::Backspace && !game.nameInput.empty())
                            game.nameInput.pop_back();
                    }
                }
            }

            if (const auto* text = event->getIf<sf::Event::TextEntered>())
            {
                if (game.scene == Scene::Result && !game.confirmed)
                    addNameChar(game, static_cast<std::uint32_t>(text->unicode));
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left && game.scene == Scene::Play)
                    onPlayClick(game, assets, window.mapPixelToCoords(mb->position, view));
            }
        }

        if (!window.isOpen())
            break;

        const float dt = clock.restart().asSeconds();
        update(game, assets, dt);

        window.setView(view);
        draw(window, assets, game);
        window.display();
    }

    return 0;
}
```