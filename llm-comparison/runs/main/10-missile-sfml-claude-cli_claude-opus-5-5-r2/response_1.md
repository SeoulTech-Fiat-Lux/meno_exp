```cpp
// SKY SHIELD - 미사일 방어 미니게임 (SFML 3.1, C++20)
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
constexpr float kW       = 960.f;
constexpr float kH       = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kTopY    = 476.f; // 도시·발사대 윗변 (아랫변 500, 높이 24)

constexpr float kInterceptorSpeed = 600.f;
constexpr float kExplosionMax     = 48.f;
constexpr float kExplosionLife    = 1.f;
constexpr float kClearTime        = 3.f;

const sf::Color kGroundColor(0xB0, 0x7A, 0x2A);
const sf::Color kMissileColor(0xFF, 0x4D, 0x4D);
const sf::Color kInterceptColor(0x4F, 0xC3, 0xF7);
const sf::Color kYellow(0xFF, 0xD5, 0x4A);

enum class Screen
{
    Title,
    Play,
    Result
};

struct City
{
    float x;
    bool  alive;
};

struct Base
{
    float x;
    bool  alive;
    int   ammo;
};

struct Missile
{
    sf::Vector2f start;
    sf::Vector2f pos;
    sf::Vector2f target;
    bool         targetIsBase;
    int          targetIndex;
    float        speed;
};

struct Interceptor
{
    sf::Vector2f start;
    sf::Vector2f pos;
    sf::Vector2f target;
};

struct Explosion
{
    sf::Vector2f center;
    float        age;
};

float explosionRadius(float age)
{
    if (age < 0.f)
        return 0.f;
    if (age < 0.5f)
        return kExplosionMax * (age / 0.5f);
    if (age < 1.f)
        return kExplosionMax * (1.f - (age - 0.5f) / 0.5f);
    return 0.f;
}

class SoundPool
{
public:
    SoundPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        m_sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            m_sounds.emplace_back(buffer);
    }

    void play()
    {
        for (auto& s : m_sounds)
        {
            if (s.getStatus() != sf::SoundSource::Status::Playing)
            {
                s.play();
                return;
            }
        }
        m_sounds[m_next].stop();
        m_sounds[m_next].play();
        m_next = (m_next + 1) % m_sounds.size();
    }

private:
    std::vector<sf::Sound> m_sounds;
    std::size_t            m_next = 0;
};

void addLine(std::vector<sf::Vertex>& tris, sf::Vector2f a, sf::Vector2f b, sf::Color color, float thickness)
{
    sf::Vector2f d   = b - a;
    float        len = d.length();
    if (len < 0.001f)
        return;
    sf::Vector2f n  = sf::Vector2f(-d.y / len, d.x / len) * (thickness * 0.5f);
    sf::Vector2f p0 = a + n, p1 = b + n, p2 = b - n, p3 = a - n;
    tris.push_back(sf::Vertex{p0, color});
    tris.push_back(sf::Vertex{p1, color});
    tris.push_back(sf::Vertex{p2, color});
    tris.push_back(sf::Vertex{p0, color});
    tris.push_back(sf::Vertex{p2, color});
    tris.push_back(sf::Vertex{p3, color});
}

void applyLetterbox(sf::View& view, sf::Vector2u size)
{
    if (size.x == 0 || size.y == 0)
        return;
    float windowRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
    float viewRatio   = kW / kH;
    float sx = 1.f, sy = 1.f, px = 0.f, py = 0.f;
    if (windowRatio > viewRatio)
    {
        sx = viewRatio / windowRatio;
        px = (1.f - sx) * 0.5f;
    }
    else
    {
        sy = windowRatio / viewRatio;
        py = (1.f - sy) * 0.5f;
    }
    view.setViewport(sf::FloatRect({px, py}, {sx, sy}));
}

struct Game
{
    std::array<City, 6>      cities{};
    std::array<Base, 3>      bases{};
    std::vector<Missile>     missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion>   explosions;

    int   score       = 0;
    int   wave        = 1;
    float waveTime    = 0.f;
    int   spawned     = 0;
    float clearTimer  = 0.f; // > 0 이면 WAVE CLEAR 표시 중
    int   clearedWave = 0;

    void reset()
    {
        const float cityX[6] = {160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
        const float baseX[3] = {60.f, 480.f, 900.f};
        for (int i = 0; i < 6; ++i)
            cities[i] = City{cityX[i], true};
        for (int i = 0; i < 3; ++i)
            bases[i] = Base{baseX[i], true, 10};
        missiles.clear();
        interceptors.clear();
        explosions.clear();
        score       = 0;
        wave        = 1;
        waveTime    = 0.f;
        spawned     = 0;
        clearTimer  = 0.f;
        clearedWave = 0;
    }

    int citiesAlive() const
    {
        int n = 0;
        for (const auto& c : cities)
            if (c.alive)
                ++n;
        return n;
    }

    int missileCount() const { return 10 + 2 * (wave - 1); }
    float missileSpeed() const { return 70.f + 10.f * static_cast<float>(wave - 1); }

    void startNextWave()
    {
        ++wave;
        waveTime = 0.f;
        spawned  = 0;
        for (auto& b : bases)
        {
            b.alive = true;
            b.ammo  = 10;
        }
    }
};
} // namespace

int main()
{
    // ---------- 에셋 ----------
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
    {
        std::cerr << "assets/fonts/game.ttf" << std::endl;
        return 1;
    }
    sf::Texture texCity, texBase, texCross;
    if (!texCity.loadFromFile("assets/textures/md_city.png"))
    {
        std::cerr << "assets/textures/md_city.png" << std::endl;
        return 1;
    }
    if (!texBase.loadFromFile("assets/textures/md_base.png"))
    {
        std::cerr << "assets/textures/md_base.png" << std::endl;
        return 1;
    }
    if (!texCross.loadFromFile("assets/textures/md_crosshair.png"))
    {
        std::cerr << "assets/textures/md_crosshair.png" << std::endl;
        return 1;
    }
    sf::SoundBuffer bufLaunch, bufExplode, bufLose;
    if (!bufLaunch.loadFromFile("assets/sfx/launch.wav"))
    {
        std::cerr << "assets/sfx/launch.wav" << std::endl;
        return 1;
    }
    if (!bufExplode.loadFromFile("assets/sfx/explode.wav"))
    {
        std::cerr << "assets/sfx/explode.wav" << std::endl;
        return 1;
    }
    if (!bufLose.loadFromFile("assets/sfx/lose.wav"))
    {
        std::cerr << "assets/sfx/lose.wav" << std::endl;
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_missile.ogg"))
    {
        std::cerr << "assets/music/bgm_missile.ogg" << std::endl;
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    SoundPool sndLaunch(bufLaunch, 8);
    SoundPool sndExplode(bufExplode, 12);
    SoundPool sndLose(bufLose, 6);

    // ---------- 창 ----------
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
    window.setFramerateLimit(60);
    sf::View view(sf::FloatRect({0.f, 0.f}, {kW, kH}));
    applyLetterbox(view, window.getSize());

    std::mt19937 rng(std::random_device{}());

    Game   game;
    Screen screen     = Screen::Title;
    float  titleTimer = 0.f;

    auto startPlay = [&]()
    {
        game.reset();
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto makeExplosion = [&](sf::Vector2f c)
    {
        game.explosions.push_back(Explosion{c, 0.f});
        sndExplode.play();
    };

    // ---------- 갱신 ----------
    auto updatePlay = [&](float dt, const std::vector<sf::Vector2f>& clicks)
    {
        const bool canFire = game.clearTimer <= 0.f;

        // 1. 클릭 → 요격탄 발사
        for (const auto& c : clicks)
        {
            if (!canFire || c.y >= 470.f)
                continue;
            int   best     = -1;
            float bestDist = 0.f;
            for (int i = 0; i < 3; ++i)
            {
                const auto& b = game.bases[i];
                if (!b.alive || b.ammo <= 0)
                    continue;
                float d = std::abs(b.x - c.x);
                if (best < 0 || d < bestDist)
                {
                    best     = i;
                    bestDist = d;
                }
            }
            if (best < 0)
                continue;
            game.bases[best].ammo--;
            sf::Vector2f start(game.bases[best].x, kTopY);
            game.interceptors.push_back(Interceptor{start, start, c});
            sndLaunch.play();
        }

        // WAVE CLEAR 표시 중
        if (game.clearTimer > 0.f)
        {
            game.clearTimer -= dt;
            if (game.clearTimer <= 0.f)
            {
                game.clearTimer = 0.f;
                game.startNextWave();
            }
            return;
        }

        // 2. 미사일 출발
        game.waveTime += dt;
        if (game.citiesAlive() > 0)
        {
            while (game.spawned < game.missileCount() &&
                   game.waveTime >= 1.f + 1.2f * static_cast<float>(game.spawned))
            {
                std::vector<std::pair<bool, int>> targets;
                for (int i = 0; i < 6; ++i)
                    if (game.cities[i].alive)
                        targets.push_back({false, i});
                for (int i = 0; i < 3; ++i)
                    if (game.bases[i].alive)
                        targets.push_back({true, i});
                game.spawned++;
                if (targets.empty())
                    continue;
                std::uniform_real_distribution<float> dx(0.f, kW);
                std::uniform_int_distribution<int>    dt_(0, static_cast<int>(targets.size()) - 1);
                auto [isBase, idx] = targets[static_cast<std::size_t>(dt_(rng))];
                float        tx    = isBase ? game.bases[idx].x : game.cities[idx].x;
                sf::Vector2f start(dx(rng), 0.f);
                game.missiles.push_back(
                    Missile{start, start, sf::Vector2f(tx, kTopY), isBase, idx, game.missileSpeed()});
            }
        }

        // 3. 요격탄 이동
        for (std::size_t i = 0; i < game.interceptors.size();)
        {
            auto&        it   = game.interceptors[i];
            sf::Vector2f d    = it.target - it.pos;
            float        dist = d.length();
            float        step = kInterceptorSpeed * dt;
            if (step >= dist)
            {
                sf::Vector2f c = it.target;
                game.interceptors.erase(game.interceptors.begin() + static_cast<std::ptrdiff_t>(i));
                makeExplosion(c);
            }
            else
            {
                it.pos += d / dist * step;
                ++i;
            }
        }

        // 4. 미사일 이동
        for (std::size_t i = 0; i < game.missiles.size();)
        {
            auto&        m    = game.missiles[i];
            sf::Vector2f d    = m.target - m.pos;
            float        dist = d.length();
            float        step = m.speed * dt;
            if (step >= dist)
            {
                sf::Vector2f c      = m.target;
                bool         isBase = m.targetIsBase;
                int          idx    = m.targetIndex;
                game.missiles.erase(game.missiles.begin() + static_cast<std::ptrdiff_t>(i));
                makeExplosion(c);
                if (isBase)
                {
                    if (game.bases[idx].alive)
                    {
                        game.bases[idx].alive = false;
                        sndLose.play();
                    }
                }
                else if (game.cities[idx].alive)
                {
                    game.cities[idx].alive = false;
                    sndLose.play();
                }
            }
            else
            {
                m.pos += d / dist * step;
                ++i;
            }
        }

        // 5. 폭발 진행
        for (auto& e : game.explosions)
            e.age += dt;
        std::erase_if(game.explosions, [](const Explosion& e) { return e.age >= kExplosionLife; });

        // 6. 폭발-미사일 판정 (연쇄)
        std::vector<sf::Vector2f> newExplosions;
        for (std::size_t i = 0; i < game.missiles.size();)
        {
            const auto& m   = game.missiles[i];
            bool        hit = false;
            for (const auto& e : game.explosions)
            {
                float r = explosionRadius(e.age);
                if (r <= 0.f)
                    continue;
                if ((m.pos - e.center).lengthSquared() <= r * r)
                {
                    hit = true;
                    break;
                }
            }
            if (hit)
            {
                newExplosions.push_back(m.pos);
                game.score += 25;
                game.missiles.erase(game.missiles.begin() + static_cast<std::ptrdiff_t>(i));
            }
            else
                ++i;
        }
        for (const auto& c : newExplosions)
            makeExplosion(c);

        // 7. 종료 / 웨이브 정산
        if (game.citiesAlive() == 0)
        {
            if (game.explosions.empty())
            {
                screen = Screen::Result;
                music.stop();
            }
            return;
        }
        if (game.spawned >= game.missileCount() && game.missiles.empty() && game.interceptors.empty() &&
            game.explosions.empty())
        {
            int bonus = 0;
            for (const auto& b : game.bases)
                if (b.alive)
                    bonus += b.ammo * 5;
            bonus += game.citiesAlive() * 100;
            game.score += bonus;
            game.clearedWave = game.wave;
            game.clearTimer  = kClearTime;
        }
    };

    // ---------- 그리기 도우미 ----------
    auto drawText = [&](const std::string& str, unsigned int size, sf::Vector2f pos, bool centered)
    {
        sf::Text text(font, str, size);
        text.setFillColor(sf::Color::White);
        if (centered)
        {
            sf::FloatRect b = text.getLocalBounds();
            text.setOrigin({b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
        }
        text.setPosition(pos);
        window.draw(text);
    };

    sf::Clock clock;
    while (window.isOpen())
    {
        std::vector<sf::Vector2f> clicks;
        bool                      enterPressed = false;
        bool                      rPressed     = false;

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                applyLetterbox(view, resized->size);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape || key->code == sf::Keyboard::Key::Escape)
                    window.close();
                else if (key->code == sf::Keyboard::Key::Enter || key->scancode == sf::Keyboard::Scancode::Enter ||
                         key->scancode == sf::Keyboard::Scancode::NumpadEnter)
                    enterPressed = true;
                else if (key->scancode == sf::Keyboard::Scancode::R || key->code == sf::Keyboard::Key::R)
                    rPressed = true;
            }
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left && screen == Screen::Play)
                    clicks.push_back(window.mapPixelToCoords(mb->position, view));
            }
        }
        if (!window.isOpen())
            break;

        float dt = std::min(clock.restart().asSeconds(), 0.1f);

        // ---------- 갱신 ----------
        if (screen == Screen::Title)
        {
            titleTimer += dt;
            if (enterPressed || titleTimer >= 2.f)
                startPlay();
        }
        else if (screen == Screen::Play)
        {
            constexpr float maxStep = 1.f / 120.f;
            int             steps   = std::max(1, static_cast<int>(std::ceil(dt / maxStep)));
            float           step    = dt / static_cast<float>(steps);
            for (int s = 0; s < steps && screen == Screen::Play; ++s)
            {
                static const std::vector<sf::Vector2f> noClicks;
                updatePlay(step, s == 0 ? clicks : noClicks);
            }
        }
        else if (screen == Screen::Result)
        {
            if (rPressed)
                startPlay();
        }

        // ---------- 그리기 ----------
        window.clear(sf::Color::Black);
        window.setView(view);

        if (screen == Screen::Title)
        {
            drawText("SKY SHIELD", 48, {kW * 0.5f, 220.f}, true);
            drawText("PRESS ENTER", 16, {kW * 0.5f, 300.f}, true);
        }
        else if (screen == Screen::Play)
        {
            // 땅
            sf::RectangleShape ground({kW, 40.f});
            ground.setPosition({0.f, kGroundY});
            ground.setFillColor(kGroundColor);
            window.draw(ground);

            // 도시
            for (const auto& c : game.cities)
            {
                if (!c.alive)
                    continue;
                sf::Sprite sp(texCity);
                sp.setOrigin({24.f, 24.f});
                sp.setPosition({c.x, kGroundY});
                window.draw(sp);
            }
            // 발사대 + 남은 탄
            for (const auto& b : game.bases)
            {
                if (!b.alive)
                    continue;
                sf::Sprite sp(texBase);
                sp.setOrigin({24.f, 24.f});
                sp.setPosition({b.x, kGroundY});
                window.draw(sp);
                drawText(std::to_string(b.ammo), 16, {b.x, kGroundY + 20.f}, true);
            }

            // 궤적
            std::vector<sf::Vertex> tris;
            for (const auto& m : game.missiles)
                addLine(tris, m.start, m.pos, kMissileColor, 2.f);
            for (const auto& it : game.interceptors)
                addLine(tris, it.start, it.pos, kInterceptColor, 2.f);
            if (!tris.empty())
                window.draw(tris.data(), tris.size(), sf::PrimitiveType::Triangles);

            // 미사일 머리
            for (const auto& m : game.missiles)
            {
                sf::RectangleShape head({4.f, 4.f});
                head.setOrigin({2.f, 2.f});
                head.setPosition(m.pos);
                head.setFillColor(sf::Color::White);
                window.draw(head);
            }

            // 폭발
            for (const auto& e : game.explosions)
            {
                float r = explosionRadius(e.age);
                if (r <= 0.f)
                    continue;
                sf::CircleShape circle(r, 48);
                circle.setOrigin({r, r});
                circle.setPosition(e.center);
                int phase = static_cast<int>(e.age / 0.1f);
                circle.setFillColor(phase % 2 == 0 ? sf::Color::White : kYellow);
                window.draw(circle);
            }

            // HUD
            drawText("SCORE " + std::to_string(game.score), 16, {16.f, 12.f}, false);
            {
                sf::Text waveText(font, "WAVE " + std::to_string(game.wave), 16);
                waveText.setFillColor(sf::Color::White);
                sf::FloatRect b = waveText.getLocalBounds();
                waveText.setPosition({kW - 16.f - b.position.x - b.size.x, 12.f});
                window.draw(waveText);
            }
            if (game.clearTimer > 0.f)
                drawText("WAVE " + std::to_string(game.clearedWave) + " CLEAR", 32, {kW * 0.5f, 240.f}, true);

            // 조준점
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), view);
            sf::Sprite   cross(texCross);
            cross.setOrigin({12.f, 12.f});
            cross.setPosition(mouse);
            window.draw(cross);
        }
        else // Result
        {
            drawText("GAME OVER", 48, {kW * 0.5f, 180.f}, true);
            drawText("SCORE " + std::to_string(game.score), 32, {kW * 0.5f, 250.f}, true);
            drawText("WAVE " + std::to_string(game.wave), 32, {kW * 0.5f, 300.f}, true);
            drawText("R: RESTART  ESC: QUIT", 16, {kW * 0.5f, 370.f}, true);
        }

        window.display();
    }

    music.stop();
    return 0;
}
```