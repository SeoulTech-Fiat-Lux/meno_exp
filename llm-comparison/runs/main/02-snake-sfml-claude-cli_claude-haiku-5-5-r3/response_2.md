이전 코드에서 `kWinW` 상수 정의를 빠뜨렸습니다. 함께 `RectangleShape`에 넘기는 크기 인자도 `sf::Vector2f`로 명시해, 중괄호 초기화 변환 문제가 없도록 고쳤습니다.

```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr int   kCols          = 32;
constexpr int   kRows          = 17;
constexpr float kCell          = 30.f;
constexpr float kHud           = 30.f;
constexpr float kWinW          = 960.f;
constexpr float kWinH          = 540.f;
constexpr float kMinInterval   = 0.06f;
constexpr float kStartInterval = 0.125f;
constexpr float kTitleSeconds  = 2.f;

sf::Color rgb(std::uint32_t v)
{
    return sf::Color(static_cast<std::uint8_t>(v >> 16),
                     static_cast<std::uint8_t>(v >> 8),
                     static_cast<std::uint8_t>(v));
}

const sf::Color kBoardBg = rgb(0x0E1A12);
const sf::Color kBarBg   = rgb(0x1F3A26);
const sf::Color kBody    = rgb(0x5FCB5F);
const sf::Color kHead    = rgb(0xB6F2A0);
const sf::Color kFood    = rgb(0xE5533D);
const sf::Color kText    = rgb(0xE8F5E9);

enum class Screen { Title, Play, Result };

sf::Text makeText(const sf::Font& font, const std::string& s, unsigned int size)
{
    sf::Text t(font, s, size);
    t.setFillColor(kText);
    return t;
}

void placeCentered(sf::Text& t, float x, float y)
{
    const sf::FloatRect b = t.getLocalBounds();
    t.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    t.setPosition({x, y});
}

class Game
{
public:
    Game(const sf::Font& f, sf::Sound& point, sf::Sound& lose, sf::Music& bgm)
        : m_font(f), m_pointSound(point), m_loseSound(lose), m_bgm(bgm)
    {
        resetState();
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
        {
            sf::Vector2i nd{0, 0};
            bool isDir = true;
            switch (key)
            {
            case sf::Keyboard::Key::Up:
            case sf::Keyboard::Key::W:    nd = {0, -1}; break;
            case sf::Keyboard::Key::Down:
            case sf::Keyboard::Key::S:    nd = {0, 1};  break;
            case sf::Keyboard::Key::Left:
            case sf::Keyboard::Key::A:    nd = {-1, 0}; break;
            case sf::Keyboard::Key::Right:
            case sf::Keyboard::Key::D:    nd = {1, 0};  break;
            default:                      isDir = false; break;
            }
            if (isDir)
            {
                const sf::Vector2i last = m_pending.empty() ? m_dir : m_pending.back();
                const bool same     = (nd == last);
                const bool opposite = (nd == sf::Vector2i{-last.x, -last.y});
                if (!same && !opposite && m_pending.size() < 2)
                    m_pending.push_back(nd);
            }
            break;
        }

        case Screen::Result:
            if (key == sf::Keyboard::Key::R)
                startPlay();
            break;
        }
    }

    void update(float dt)
    {
        switch (m_screen)
        {
        case Screen::Title:
            m_titleTimer += dt;
            if (m_titleTimer >= kTitleSeconds)
                startPlay();
            break;

        case Screen::Play:
            m_acc += dt;
            while (m_screen == Screen::Play && m_acc >= m_interval)
            {
                m_acc -= m_interval;
                step();
            }
            break;

        case Screen::Result:
            break;
        }
    }

    void draw(sf::RenderWindow& w)
    {
        switch (m_screen)
        {
        case Screen::Title:
        {
            w.clear(kBoardBg);
            sf::Text title = makeText(m_font, "SNAKE", 48);
            placeCentered(title, kWinW / 2.f, 220.f);
            w.draw(title);

            sf::Text prompt = makeText(m_font, "PRESS ENTER", 32);
            placeCentered(prompt, kWinW / 2.f, 320.f);
            w.draw(prompt);
            break;
        }

        case Screen::Play:
        {
            w.clear(kBoardBg);

            sf::RectangleShape bar(sf::Vector2f{kWinW, kHud});
            bar.setFillColor(kBarBg);
            bar.setPosition(sf::Vector2f{0.f, 0.f});
            w.draw(bar);

            sf::Text score = makeText(m_font, "SCORE " + std::to_string(m_score), 16);
            const sf::FloatRect b = score.getLocalBounds();
            score.setOrigin(sf::Vector2f{b.position.x, b.position.y + b.size.y / 2.f});
            score.setPosition(sf::Vector2f{12.f, kHud / 2.f});
            w.draw(score);

            sf::RectangleShape cell(sf::Vector2f{kCell - 2.f, kCell - 2.f});
            for (std::size_t i = 0; i < m_snake.size(); ++i)
            {
                cell.setFillColor(i == 0 ? kHead : kBody);
                cell.setPosition(sf::Vector2f{static_cast<float>(m_snake[i].x) * kCell + 1.f,
                                              kHud + static_cast<float>(m_snake[i].y) * kCell + 1.f});
                w.draw(cell);
            }

            sf::CircleShape food(12.f);
            food.setFillColor(kFood);
            food.setPosition(sf::Vector2f{static_cast<float>(m_food.x) * kCell + 3.f,
                                          kHud + static_cast<float>(m_food.y) * kCell + 3.f});
            w.draw(food);
            break;
        }

        case Screen::Result:
        {
            w.clear(kBoardBg);

            sf::Text over = makeText(m_font, "GAME OVER", 48);
            placeCentered(over, kWinW / 2.f, 200.f);
            w.draw(over);

            sf::Text score = makeText(m_font, "SCORE " + std::to_string(m_score), 32);
            placeCentered(score, kWinW / 2.f, 280.f);
            w.draw(score);

            sf::Text help = makeText(m_font, "R: RESTART  ESC: QUIT", 16);
            placeCentered(help, kWinW / 2.f, 360.f);
            w.draw(help);
            break;
        }
        }
    }

private:
    void resetState()
    {
        m_snake.clear();
        m_snake.push_back({16, 8});
        m_snake.push_back({15, 8});
        m_snake.push_back({14, 8});
        m_dir      = {1, 0};
        m_pending.clear();
        m_score    = 0;
        m_eaten    = 0;
        m_interval = kStartInterval;
        m_acc      = 0.f;
        spawnFood();
    }

    void startPlay()
    {
        resetState();
        m_screen = Screen::Play;
        m_bgm.stop();
        m_bgm.play();
    }

    bool onSnake(sf::Vector2i c) const
    {
        return std::find(m_snake.begin(), m_snake.end(), c) != m_snake.end();
    }

    void spawnFood()
    {
        std::vector<sf::Vector2i> freeCells;
        for (int y = 0; y < kRows; ++y)
            for (int x = 0; x < kCols; ++x)
                if (!onSnake({x, y}))
                    freeCells.push_back({x, y});

        if (!freeCells.empty())
        {
            std::uniform_int_distribution<std::size_t> dist(0, freeCells.size() - 1);
            m_food = freeCells[dist(m_rng)];
        }
    }

    void step()
    {
        if (!m_pending.empty())
        {
            m_dir = m_pending.front();
            m_pending.pop_front();
        }

        const sf::Vector2i next = m_snake.front() + m_dir;
        const bool outside = next.x < 0 || next.x >= kCols || next.y < 0 || next.y >= kRows;
        const bool eating  = (next == m_food);

        // 먹지 않는 이동에서는 꼬리가 빠져나가므로 꼬리 칸은 충돌 검사에서 제외한다.
        const std::size_t bodyChecks = eating ? m_snake.size() : m_snake.size() - 1;
        bool hit = outside;
        for (std::size_t i = 0; !hit && i < bodyChecks; ++i)
            hit = (m_snake[i] == next);

        if (hit)
        {
            m_loseSound.play();
            m_bgm.stop();
            m_screen = Screen::Result;
            return;
        }

        m_snake.push_front(next);
        if (eating)
        {
            ++m_eaten;
            m_score += 10;
            m_pointSound.play();
            if (m_eaten % 5 == 0)
                m_interval = std::max(kMinInterval, m_interval * 0.9f);
            spawnFood();
        }
        else
        {
            m_snake.pop_back();
        }
    }

    const sf::Font& m_font;
    sf::Sound&      m_pointSound;
    sf::Sound&      m_loseSound;
    sf::Music&      m_bgm;

    Screen m_screen     = Screen::Title;
    float  m_titleTimer = 0.f;

    std::deque<sf::Vector2i> m_snake;
    sf::Vector2i             m_dir{1, 0};
    std::deque<sf::Vector2i> m_pending;
    sf::Vector2i             m_food{0, 0};

    int   m_score    = 0;
    int   m_eaten    = 0;
    float m_interval = kStartInterval;
    float m_acc      = 0.f;

    std::mt19937 m_rng{std::random_device{}()};
};
} // namespace

int main()
{
    const std::filesystem::path fontPath  = "assets/fonts/game.ttf";
    const std::filesystem::path pointPath = "assets/sfx/point.wav";
    const std::filesystem::path losePath  = "assets/sfx/lose.wav";
    const std::filesystem::path bgmPath   = "assets/music/bgm_snake.ogg";

    sf::Font        font;
    sf::SoundBuffer pointBuf;
    sf::SoundBuffer loseBuf;
    sf::Music       bgm;

    if (!font.openFromFile(fontPath))
    {
        std::cerr << "failed to load: " << fontPath.string() << '\n';
        return 1;
    }
    if (!pointBuf.loadFromFile(pointPath))
    {
        std::cerr << "failed to load: " << pointPath.string() << '\n';
        return 1;
    }
    if (!loseBuf.loadFromFile(losePath))
    {
        std::cerr << "failed to load: " << losePath.string() << '\n';
        return 1;
    }
    if (!bgm.openFromFile(bgmPath))
    {
        std::cerr << "failed to load: " << bgmPath.string() << '\n';
        return 1;
    }

    bgm.setVolume(40.f);
    bgm.setLooping(true);

    sf::Sound pointSound(pointBuf);
    sf::Sound loseSound(loseBuf);

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SNAKE");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    Game game(font, pointSound, loseSound, bgm);

    sf::Clock clock;
    while (window.isOpen())
    {
        const float dt = clock.restart().asSeconds();

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                    window.close();
                else
                    game.onKey(key->code);
            }
        }

        if (!window.isOpen())
            break;

        game.update(dt);
        game.draw(window);
        window.display();
    }

    return 0;
}
```