아래 `main.cpp`는 SFML 3.1 헤더와 C++20 표준 라이브러리만 사용합니다. 이 환경에서는 컴파일하거나 실행해 보지 못했습니다.

```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;

constexpr std::size_t kCols = 4;
constexpr std::size_t kRows = 4;
constexpr std::size_t kCardCount = kCols * kRows;
constexpr std::size_t kPairCount = kCardCount / 2;
constexpr float kCardW = 80.f;
constexpr float kCardH = 100.f;
constexpr float kGap = 12.f;
constexpr float kBoardX = (kViewW - (static_cast<float>(kCols) * kCardW + static_cast<float>(kCols - 1) * kGap)) / 2.f;
constexpr float kBoardY = (kViewH - (static_cast<float>(kRows) * kCardH + static_cast<float>(kRows - 1) * kGap)) / 2.f;

constexpr float kTitleTime = 2.f;
constexpr float kMismatchTime = 0.8f;
constexpr std::size_t kMaxNameLen = 8;
constexpr std::size_t kTopN = 5;
constexpr std::size_t kNone = static_cast<std::size_t>(-1);

constexpr sf::Color kBg(0x1B, 0x2A, 0x1F);
constexpr sf::Color kWhite(0xFF, 0xFF, 0xFF);
constexpr sf::Color kHighlight(0xFF, 0xD5, 0x4A);

enum class Screen { Title, Play, Result };

struct Card
{
    int  face   = 0;
    bool faceUp = false;
    bool matched = false;
};

struct Record
{
    std::string name;
    int         moves  = 0;
    int         tenths = 0; // TIME을 0.1초 단위로 기록
    int         seq    = 0; // 먼저 낸 기록이 앞서도록 하는 순서 번호
};

sf::String toSfString(const std::string& s)
{
    return sf::String(std::string_view(s));
}

void drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& str,
              unsigned int size, sf::Color color, float y)
{
    sf::Text text(font, toSfString(str), size);
    text.setFillColor(color);
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y});
    text.setPosition({kViewW / 2.f, y});
    target.draw(text);
}

std::string recordLine(std::size_t rank, const Record& r)
{
    return std::format("{}. {}   MOVES {}   TIME {}.{}",
                       rank, r.name, r.moves, r.tenths / 10, r.tenths % 10);
}

void applyLetterbox(sf::RenderWindow& window, sf::View& view, sf::Vector2u size)
{
    const float w = static_cast<float>(size.x);
    const float h = static_cast<float>(size.y);
    if (w <= 0.f || h <= 0.f)
        return;

    const float scale = std::min(w / kViewW, h / kViewH);
    const float vw = kViewW * scale;
    const float vh = kViewH * scale;
    const float vx = (w - vw) / 2.f;
    const float vy = (h - vh) / 2.f;

    view.setViewport(sf::FloatRect({vx / w, vy / h}, {vw / w, vh / h}));
    window.setView(view);
}

// 짧은 효과음이 겹쳐 나도 끊기지 않도록 여러 개의 Sound를 돌려 쓴다.
class SfxPool
{
public:
    SfxPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
            m_sounds.push_back(std::make_unique<sf::Sound>(buffer));
    }

    void play()
    {
        m_sounds[m_next]->play();
        m_next = (m_next + 1) % m_sounds.size();
    }

private:
    std::vector<std::unique_ptr<sf::Sound>> m_sounds;
    std::size_t                             m_next = 0;
};

class Game
{
public:
    Game(const sf::Font& font, const sf::Texture& cardTexture,
         const sf::SoundBuffer& flipBuffer, const sf::SoundBuffer& pointBuffer,
         sf::Music& music)
        : m_font(font)
        , m_sprite(cardTexture)
        , m_flipSfx(flipBuffer, 4)
        , m_pointSfx(pointBuffer, 4)
        , m_music(music)
    {
        m_music.setLooping(true);
        m_music.setVolume(40.f);
    }

    void onKey(sf::Keyboard::Key key)
    {
        switch (m_screen)
        {
        case Screen::Title:
            if (key == sf::Keyboard::Key::Enter)
                startPlay();
            break;

        case Screen::Play:
            break;

        case Screen::Result:
            if (!m_confirmed)
            {
                if (key == sf::Keyboard::Key::Enter && !m_name.empty())
                    confirmName();
                else if (key == sf::Keyboard::Key::Backspace && !m_name.empty())
                    m_name.pop_back();
            }
            else if (key == sf::Keyboard::Key::R)
            {
                startPlay();
            }
            break;
        }
    }

    void onText(char32_t ch)
    {
        if (m_screen != Screen::Result || m_confirmed)
            return;
        if (ch >= 128)
            return;

        const auto c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c) || m_name.size() >= kMaxNameLen)
            return;

        m_name.push_back(static_cast<char>(std::toupper(c)));
    }

    void onClick(sf::Vector2f p)
    {
        if (m_screen != Screen::Play || m_busy)
            return;

        for (std::size_t i = 0; i < kCardCount; ++i)
        {
            const float x = kBoardX + static_cast<float>(i % kCols) * (kCardW + kGap);
            const float y = kBoardY + static_cast<float>(i / kCols) * (kCardH + kGap);
            if (p.x >= x && p.x < x + kCardW && p.y >= y && p.y < y + kCardH)
            {
                flipCard(i);
                return;
            }
        }
    }

    void update(float dt)
    {
        switch (m_screen)
        {
        case Screen::Title:
            m_titleTime += dt;
            if (m_titleTime >= kTitleTime)
                startPlay();
            break;

        case Screen::Play:
            if (m_timerRunning)
                m_elapsed += static_cast<double>(dt);

            if (m_busy)
            {
                m_busyTime -= dt;
                if (m_busyTime <= 0.f)
                {
                    m_cards[m_first].faceUp  = false;
                    m_cards[m_second].faceUp = false;
                    m_first  = kNone;
                    m_second = kNone;
                    m_busy   = false;
                }
            }
            break;

        case Screen::Result:
            break;
        }
    }

    void draw(sf::RenderTarget& target)
    {
        sf::RectangleShape bg(sf::Vector2f{kViewW, kViewH});
        bg.setFillColor(kBg);
        target.draw(bg);

        switch (m_screen)
        {
        case Screen::Title:
            drawTitle(target);
            break;
        case Screen::Play:
            drawPlay(target);
            break;
        case Screen::Result:
            drawResult(target);
            break;
        }
    }

private:
    int tenths() const
    {
        return static_cast<int>(m_elapsed * 10.0 + 1e-6);
    }

    void deal()
    {
        std::array<int, kCardCount> faces{};
        for (std::size_t i = 0; i < kCardCount; ++i)
            faces[i] = static_cast<int>(i / 2) + 1;

        std::shuffle(faces.begin(), faces.end(), m_rng);

        for (std::size_t i = 0; i < kCardCount; ++i)
            m_cards[i] = Card{faces[i], false, false};
    }

    void startPlay()
    {
        m_screen = Screen::Play;
        deal();
        m_first      = kNone;
        m_second     = kNone;
        m_busy       = false;
        m_busyTime   = 0.f;
        m_moves      = 0;
        m_pairs      = 0;
        m_timerStarted = false;
        m_timerRunning = false;
        m_elapsed    = 0.0;
        m_confirmed  = false;
        m_name.clear();
        m_highlight  = -1;
        m_music.play();
    }

    void finishGame()
    {
        m_timerRunning = false;
        m_finalMoves   = m_moves;
        m_finalTenths  = tenths();
        m_screen       = Screen::Result;
        m_confirmed    = false;
        m_name.clear();
        m_music.stop();
    }

    void confirmName()
    {
        m_confirmed = true;
        m_records.push_back(Record{m_name, m_finalMoves, m_finalTenths, m_seq++});
        m_highlight = m_records.back().seq;
    }

    void flipCard(std::size_t i)
    {
        Card& c = m_cards[i];
        if (c.faceUp || c.matched)
            return;

        c.faceUp = true;
        m_flipSfx.play();

        if (!m_timerStarted)
        {
            m_timerStarted = true;
            m_timerRunning = true;
        }

        if (m_first == kNone)
        {
            m_first = i;
        }
        else
        {
            m_second = i;
            judge();
        }
    }

    void judge()
    {
        ++m_moves;
        Card& a = m_cards[m_first];
        Card& b = m_cards[m_second];

        if (a.face == b.face)
        {
            a.matched = true;
            b.matched = true;
            m_first  = kNone;
            m_second = kNone;
            ++m_pairs;
            m_pointSfx.play();
            if (m_pairs == kPairCount)
                finishGame();
        }
        else
        {
            m_busy     = true;
            m_busyTime = kMismatchTime;
        }
    }

    void drawTitle(sf::RenderTarget& target)
    {
        drawText(target, m_font, "PAIRS", 48, kWhite, 170.f);
        drawText(target, m_font, "PRESS ENTER", 32, kWhite, 290.f);
    }

    void drawPlay(sf::RenderTarget& target)
    {
        const int t = tenths();
        drawText(target, m_font,
                 std::format("MOVES {}   TIME {}.{}", m_moves, t / 10, t % 10),
                 32, kWhite, 6.f);

        for (std::size_t i = 0; i < kCardCount; ++i)
        {
            const Card& c = m_cards[i];
            const int rx = c.faceUp ? c.face * static_cast<int>(kCardW) : 0;
            const float x = kBoardX + static_cast<float>(i % kCols) * (kCardW + kGap);
            const float y = kBoardY + static_cast<float>(i / kCols) * (kCardH + kGap);

            m_sprite.setTextureRect(sf::IntRect({rx, 0},
                                                {static_cast<int>(kCardW), static_cast<int>(kCardH)}));
            m_sprite.setPosition({x, y});
            target.draw(m_sprite);
        }
    }

    void drawResult(sf::RenderTarget& target)
    {
        drawText(target, m_font, "CLEAR", 48, kWhite, 40.f);
        drawText(target, m_font,
                 std::format("MOVES {}   TIME {}.{}", m_finalMoves, m_finalTenths / 10, m_finalTenths % 10),
                 32, kWhite, 110.f);

        if (!m_confirmed)
            drawText(target, m_font, std::format("NAME: {}", m_name), 32, kWhite, 200.f);
        else
            drawLeaderboard(target);

        drawText(target, m_font, "R: RESTART  ESC: QUIT", 16, kWhite, 500.f);
    }

    void drawLeaderboard(sf::RenderTarget& target)
    {
        std::vector<const Record*> order;
        order.reserve(m_records.size());
        for (const Record& r : m_records)
            order.push_back(&r);

        std::sort(order.begin(), order.end(), [](const Record* a, const Record* b)
        {
            if (a->moves != b->moves)
                return a->moves < b->moves;
            if (a->tenths != b->tenths)
                return a->tenths < b->tenths;
            return a->seq < b->seq;
        });

        for (std::size_t i = 0; i < order.size() && i < kTopN; ++i)
        {
            const Record& r = *order[i];
            const sf::Color color = (r.seq == m_highlight) ? kHighlight : kWhite;
            drawText(target, m_font, recordLine(i + 1, r), 32, color,
                     200.f + 40.f * static_cast<float>(i));
        }

        // 5위 밖이면 순위표 아래 줄에 따로 표시
        for (std::size_t i = kTopN; i < order.size(); ++i)
        {
            const Record& r = *order[i];
            if (r.seq == m_highlight)
                drawText(target, m_font, recordLine(i + 1, r), 32, kHighlight, 420.f);
        }
    }

    const sf::Font&  m_font;
    sf::Sprite       m_sprite;
    SfxPool          m_flipSfx;
    SfxPool          m_pointSfx;
    sf::Music&       m_music;

    Screen                     m_screen = Screen::Title;
    double                     m_titleTime = 0.0;
    std::array<Card, kCardCount> m_cards{};

    std::size_t m_first  = kNone;
    std::size_t m_second = kNone;
    bool        m_busy   = false;
    float       m_busyTime = 0.f;

    int    m_moves = 0;
    int    m_pairs = 0;
    bool   m_timerStarted = false;
    bool   m_timerRunning = false;
    double m_elapsed = 0.0;

    int  m_finalMoves  = 0;
    int  m_finalTenths = 0;
    bool m_confirmed = false;
    std::string m_name;

    std::vector<Record> m_records;
    int                 m_seq       = 0;
    int                 m_highlight = -1;

    std::mt19937 m_rng{std::random_device{}()};
};

int reportLoadFailure(const char* path)
{
    std::cerr << "failed to load: " << path << '\n';
    return 1;
}

} // namespace

int main()
{
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{960u, 540u}), "PAIRS", sf::Style::Default);
    window.setFramerateLimit(60);

    sf::View view(sf::FloatRect({0.f, 0.f}, {kViewW, kViewH}));
    applyLetterbox(window, view, window.getSize());

    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
        return reportLoadFailure("assets/fonts/game.ttf");

    sf::Texture cardTexture;
    if (!cardTexture.loadFromFile("assets/textures/cards.png"))
        return reportLoadFailure("assets/textures/cards.png");

    sf::SoundBuffer flipBuffer;
    if (!flipBuffer.loadFromFile("assets/sfx/flip.wav"))
        return reportLoadFailure("assets/sfx/flip.wav");

    sf::SoundBuffer pointBuffer;
    if (!pointBuffer.loadFromFile("assets/sfx/point.wav"))
        return reportLoadFailure("assets/sfx/point.wav");

    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_card.ogg"))
        return reportLoadFailure("assets/music/bgm_card.ogg");

    Game game(font, cardTexture, flipBuffer, pointBuffer, music);

    sf::Clock clock;
    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                break;
            }

            if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                applyLetterbox(window, view, resized->size);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                {
                    window.close();
                    break;
                }
                game.onKey(key->code);
            }
            else if (const auto* text = event->getIf<sf::Event::TextEntered>())
            {
                game.onText(text->unicode);
            }
            else if (const auto* button = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (button->button == sf::Mouse::Button::Left)
                    game.onClick(window.mapPixelToCoords(button->position));
            }
        }

        if (!window.isOpen())
            break;

        const float dt = clock.restart().asSeconds();
        game.update(dt);

        window.clear(sf::Color::Black);
        game.draw(window);
        window.display();
    }

    return 0;
}
```