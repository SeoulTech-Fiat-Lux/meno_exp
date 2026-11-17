// SKY SHIELD - 미사일 방어 미니게임 (SFML 3.1, C++20)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float kWidth  = 960.f;
constexpr float kHeight = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kTargetTopY = 476.f; // 500 - 24
constexpr float kFireLimitY = 470.f;
constexpr float kInterceptorSpeed = 600.f;
constexpr float kExplosionMaxRadius = 48.f;
constexpr float kExplosionHalf = 0.5f;
constexpr float kExplosionLife = 1.0f;

const std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
const std::array<float, 3> kBaseX{60.f, 480.f, 900.f};

const sf::Color kGroundColor(0xB0, 0x7A, 0x2A);
const sf::Color kMissileTrail(0xFF, 0x4D, 0x4D);
const sf::Color kInterceptorTrail(0x4F, 0xC3, 0xF7);
const sf::Color kExplosionAlt(0xFF, 0xD5, 0x4A);

enum class Screen
{
    Title,
    Play,
    Result
};

struct Missile
{
    sf::Vector2f start;
    sf::Vector2f pos;
    sf::Vector2f target;
    int          targetIndex; // 0..5 도시, 6..8 발사대
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
    float        age = 0.f;
};

float explosionRadius(float age)
{
    if (age < 0.f)
        return 0.f;
    if (age < kExplosionHalf)
        return kExplosionMaxRadius * (age / kExplosionHalf);
    if (age < kExplosionLife)
        return kExplosionMaxRadius * (1.f - (age - kExplosionHalf) / kExplosionHalf);
    return 0.f;
}

sf::View makeLetterboxView(sf::Vector2u size)
{
    sf::View view({kWidth / 2.f, kHeight / 2.f}, {kWidth, kHeight});
    if (size.x == 0 || size.y == 0)
        return view;
    const float windowRatio = static_cast<float>(size.x) / static_cast<float>(size.y);
    const float targetRatio = kWidth / kHeight;
    if (windowRatio > targetRatio)
    {
        const float w = targetRatio / windowRatio;
        view.setViewport(sf::FloatRect({(1.f - w) / 2.f, 0.f}, {w, 1.f}));
    }
    else
    {
        const float h = windowRatio / targetRatio;
        view.setViewport(sf::FloatRect({0.f, (1.f - h) / 2.f}, {1.f, h}));
    }
    return view;
}

void appendThickLine(sf::VertexArray& va, sf::Vector2f a, sf::Vector2f b, sf::Color color, float thickness)
{
    const sf::Vector2f d   = b - a;
    const float        len = d.length();
    if (len < 0.001f)
        return;
    const sf::Vector2f n = sf::Vector2f(-d.y, d.x) / len * (thickness / 2.f);
    va.append(sf::Vertex{a + n, color});
    va.append(sf::Vertex{a - n, color});
    va.append(sf::Vertex{b + n, color});
    va.append(sf::Vertex{b + n, color});
    va.append(sf::Vertex{a - n, color});
    va.append(sf::Vertex{b - n, color});
}

// align: 0 왼쪽, 1 가운데, 2 오른쪽. pos.y는 글자의 세로 중심.
void drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& str, unsigned size, sf::Vector2f pos, int align)
{
    sf::Text text(font, str, size);
    text.setFillColor(sf::Color::White);
    const sf::FloatRect b = text.getLocalBounds();
    float ox = b.position.x;
    if (align == 1)
        ox = b.position.x + b.size.x / 2.f;
    else if (align == 2)
        ox = b.position.x + b.size.x;
    text.setOrigin({ox, b.position.y + b.size.y / 2.f});
    text.setPosition(pos);
    target.draw(text);
}

struct SoundPool
{
    std::vector<sf::Sound> sounds;
    std::size_t            next = 0;

    SoundPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            sounds.emplace_back(buffer);
    }

    void play()
    {
        if (sounds.empty())
            return;
        sounds[next].stop();
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

class Game
{
public:
    Game(SoundPool& launch, SoundPool& explode, SoundPool& lose, sf::Music& music) :
        m_launch(launch), m_explode(explode), m_lose(lose), m_music(music), m_rng(std::random_device{}())
    {
    }

    Screen screen = Screen::Title;
    float  titleTime = 0.f;

    int   score = 0;
    int   wave  = 1;
    float clearTimer = 0.f;

    std::array<bool, 6> cityAlive{};
    std::array<bool, 3> baseAlive{};
    std::array<int, 3>  ammo{};

    std::vector<Missile>     missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion>   explosions;

    void startGame()
    {
        score = 0;
        wave  = 1;
        cityAlive.fill(true);
        startWave();
        screen = Screen::Play;
        m_music.stop();
        m_music.play();
    }

    void update(float dt, bool enterPressed, bool rPressed, const std::vector<sf::Vector2f>& clicks)
    {
        switch (screen)
        {
            case Screen::Title:
                titleTime += dt;
                if (enterPressed || titleTime >= 2.f)
                    startGame();
                break;
            case Screen::Play:
                updatePlay(dt, clicks);
                break;
            case Screen::Result:
                if (rPressed)
                    startGame();
                break;
        }
    }

    int citiesAlive() const
    {
        int n = 0;
        for (bool a : cityAlive)
            n += a ? 1 : 0;
        return n;
    }

private:
    SoundPool&   m_launch;
    SoundPool&   m_explode;
    SoundPool&   m_lose;
    sf::Music&   m_music;
    std::mt19937 m_rng;

    float m_waveTime     = 0.f;
    int   m_spawned      = 0;
    int   m_missileCount = 0;

    void startWave()
    {
        baseAlive.fill(true);
        ammo.fill(10);
        m_waveTime     = 0.f;
        m_spawned      = 0;
        m_missileCount = 10 + 2 * (wave - 1);
        clearTimer     = 0.f;
        missiles.clear();
        interceptors.clear();
        explosions.clear();
    }

    void addExplosion(sf::Vector2f at)
    {
        explosions.push_back(Explosion{at, 0.f});
        m_explode.play();
    }

    static sf::Vector2f targetPoint(int index)
    {
        if (index < 6)
            return {kCityX[static_cast<std::size_t>(index)], kTargetTopY};
        return {kBaseX[static_cast<std::size_t>(index - 6)], kTargetTopY};
    }

    void fire(sf::Vector2f click)
    {
        int   best     = -1;
        float bestDist = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            if (!baseAlive[static_cast<std::size_t>(i)] || ammo[static_cast<std::size_t>(i)] <= 0)
                continue;
            const float d = std::abs(kBaseX[static_cast<std::size_t>(i)] - click.x);
            if (best < 0 || d < bestDist)
            {
                best     = i;
                bestDist = d;
            }
        }
        if (best < 0)
            return;
        ammo[static_cast<std::size_t>(best)] -= 1;
        const sf::Vector2f start{kBaseX[static_cast<std::size_t>(best)], kTargetTopY};
        interceptors.push_back(Interceptor{start, start, click});
        m_launch.play();
    }

    void spawnMissile()
    {
        std::vector<int> candidates;
        for (int i = 0; i < 6; ++i)
            if (cityAlive[static_cast<std::size_t>(i)])
                candidates.push_back(i);
        for (int i = 0; i < 3; ++i)
            if (baseAlive[static_cast<std::size_t>(i)])
                candidates.push_back(6 + i);
        if (candidates.empty())
            return;
        std::uniform_real_distribution<float> xDist(0.f, kWidth);
        std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
        const int          idx   = candidates[pick(m_rng)];
        const sf::Vector2f start{xDist(m_rng), 0.f};
        missiles.push_back(Missile{start, start, targetPoint(idx), idx, 70.f + 10.f * static_cast<float>(wave - 1)});
    }

    void updatePlay(float dt, const std::vector<sf::Vector2f>& clicks)
    {
        if (clearTimer > 0.f)
        {
            clearTimer -= dt;
            if (clearTimer <= 0.f)
            {
                ++wave;
                startWave();
            }
            return;
        }

        const bool citiesLeft = citiesAlive() > 0;

        // 발사
        if (citiesLeft)
        {
            for (const sf::Vector2f& c : clicks)
            {
                if (c.x < 0.f || c.x > kWidth || c.y < 0.f || c.y >= kFireLimitY)
                    continue;
                fire(c);
            }
        }

        // 미사일 출발
        m_waveTime += dt;
        if (citiesLeft)
        {
            while (m_spawned < m_missileCount && m_waveTime >= 1.f + 1.2f * static_cast<float>(m_spawned))
            {
                spawnMissile();
                ++m_spawned;
            }
        }

        // 폭발 시간 경과 및 제거
        for (Explosion& e : explosions)
            e.age += dt;
        std::erase_if(explosions, [](const Explosion& e) { return e.age >= kExplosionLife; });

        // 요격탄 이동
        std::vector<sf::Vector2f> arrived;
        for (Interceptor& it : interceptors)
        {
            const sf::Vector2f d    = it.target - it.pos;
            const float        dist = d.length();
            const float        step = kInterceptorSpeed * dt;
            if (dist <= step || dist < 0.0001f)
            {
                it.pos = it.target;
                arrived.push_back(it.target);
            }
            else
            {
                it.pos += d / dist * step;
            }
        }
        std::erase_if(interceptors, [](const Interceptor& it) { return it.pos == it.target; });
        for (const sf::Vector2f& p : arrived)
            addExplosion(p);

        // 미사일 이동
        std::vector<Missile> remaining;
        remaining.reserve(missiles.size());
        for (Missile& m : missiles)
        {
            const sf::Vector2f d    = m.target - m.pos;
            const float        dist = d.length();
            const float        step = m.speed * dt;
            if (dist <= step || dist < 0.0001f)
            {
                m.pos = m.target;
                addExplosion(m.target);
                if (m.targetIndex < 6)
                {
                    auto& alive = cityAlive[static_cast<std::size_t>(m.targetIndex)];
                    if (alive)
                    {
                        alive = false;
                        m_lose.play();
                    }
                }
                else
                {
                    auto& alive = baseAlive[static_cast<std::size_t>(m.targetIndex - 6)];
                    if (alive)
                    {
                        alive = false;
                        m_lose.play();
                    }
                }
            }
            else
            {
                m.pos += d / dist * step;
                remaining.push_back(m);
            }
        }
        missiles = std::move(remaining);

        // 폭발과 미사일 판정 (연쇄)
        bool changed = true;
        while (changed)
        {
            changed = false;
            std::vector<Missile>      survivors;
            std::vector<sf::Vector2f> newExplosions;
            for (const Missile& m : missiles)
            {
                bool hit = false;
                for (const Explosion& e : explosions)
                {
                    const float r = explosionRadius(e.age);
                    if (r > 0.f && (m.pos - e.center).lengthSquared() <= r * r)
                    {
                        hit = true;
                        break;
                    }
                }
                if (hit)
                {
                    score += 25;
                    newExplosions.push_back(m.pos);
                }
                else
                {
                    survivors.push_back(m);
                }
            }
            missiles = std::move(survivors);
            for (const sf::Vector2f& p : newExplosions)
                addExplosion(p);
            // 새 폭발의 반지름은 0이므로 이번 프레임에 다시 판정할 필요는 없음
        }

        // 게임 오버 / 웨이브 종료
        if (citiesAlive() == 0)
        {
            if (explosions.empty())
            {
                screen = Screen::Result;
                m_music.stop();
            }
            return;
        }

        if (m_spawned >= m_missileCount && missiles.empty() && interceptors.empty() && explosions.empty())
        {
            int bonus = 0;
            for (std::size_t i = 0; i < 3; ++i)
                if (baseAlive[i])
                    bonus += ammo[i] * 5;
            bonus += citiesAlive() * 100;
            score += bonus;
            clearTimer = 3.f;
        }
    }
};

template <typename T>
bool loadResource(T& res, const std::string& path)
{
    bool ok = false;
    if constexpr (std::is_same_v<T, sf::Font> || std::is_same_v<T, sf::Music>)
        ok = res.openFromFile(path);
    else
        ok = res.loadFromFile(path);
    if (!ok)
        std::cerr << "Failed to load asset: " << path << '\n';
    return ok;
}

} // namespace

int main()
{
    try
    {
        sf::Font        font;
        sf::Texture     cityTex;
        sf::Texture     baseTex;
        sf::Texture     crossTex;
        sf::SoundBuffer launchBuf;
        sf::SoundBuffer explodeBuf;
        sf::SoundBuffer loseBuf;
        sf::Music       music;

        if (!loadResource(font, "assets/fonts/game.ttf"))
            return 1;
        if (!loadResource(launchBuf, "assets/sfx/launch.wav"))
            return 1;
        if (!loadResource(explodeBuf, "assets/sfx/explode.wav"))
            return 1;
        if (!loadResource(loseBuf, "assets/sfx/lose.wav"))
            return 1;
        if (!loadResource(music, "assets/music/bgm_missile.ogg"))
            return 1;
        if (!loadResource(cityTex, "assets/textures/md_city.png"))
            return 1;
        if (!loadResource(baseTex, "assets/textures/md_base.png"))
            return 1;
        if (!loadResource(crossTex, "assets/textures/md_crosshair.png"))
            return 1;

        music.setLooping(true);
        music.setVolume(40.f);

        SoundPool launchPool(launchBuf, 8);
        SoundPool explodePool(explodeBuf, 12);
        SoundPool losePool(loseBuf, 6);

        sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
        window.setFramerateLimit(60);
        window.setMouseCursorVisible(false);
        sf::View view = makeLetterboxView(window.getSize());

        Game game(launchPool, explodePool, losePool, music);

        sf::Sprite citySprite(cityTex);
        citySprite.setOrigin({24.f, 24.f});
        sf::Sprite baseSprite(baseTex);
        baseSprite.setOrigin({24.f, 24.f});
        sf::Sprite crossSprite(crossTex);
        crossSprite.setOrigin({12.f, 12.f});

        sf::RectangleShape ground({kWidth, 40.f});
        ground.setPosition({0.f, kGroundY});
        ground.setFillColor(kGroundColor);

        sf::RectangleShape head({4.f, 4.f});
        head.setOrigin({2.f, 2.f});
        head.setFillColor(sf::Color::White);

        sf::CircleShape circle(1.f, 48);

        sf::Clock clock;
        std::vector<sf::Vector2f> clicks;

        while (window.isOpen())
        {
            bool enterPressed = false;
            bool rPressed     = false;
            clicks.clear();

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
                    if (key->code == sf::Keyboard::Key::Escape || key->scancode == sf::Keyboard::Scan::Escape)
                        window.close();
                    else if (key->code == sf::Keyboard::Key::Enter || key->scancode == sf::Keyboard::Scan::Enter ||
                             key->scancode == sf::Keyboard::Scan::NumpadEnter)
                        enterPressed = true;
                    else if (key->code == sf::Keyboard::Key::R || key->scancode == sf::Keyboard::Scan::R)
                        rPressed = true;
                }
                else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mb->button == sf::Mouse::Button::Left)
                        clicks.push_back(window.mapPixelToCoords(mb->position, view));
                }
            }
            if (!window.isOpen())
                break;

            float dt = clock.restart().asSeconds();
            if (dt > 0.1f)
                dt = 0.1f;

            game.update(dt, enterPressed, rPressed, clicks);

            // 그리기
            window.clear(sf::Color::Black);
            window.setView(view);

            if (game.screen == Screen::Title)
            {
                drawText(window, font, "SKY SHIELD", 48, {kWidth / 2.f, 220.f}, 1);
                drawText(window, font, "PRESS ENTER", 32, {kWidth / 2.f, 310.f}, 1);
            }
            else if (game.screen == Screen::Play)
            {
                window.draw(ground);

                for (std::size_t i = 0; i < 6; ++i)
                {
                    if (!game.cityAlive[i])
                        continue;
                    citySprite.setPosition({kCityX[i], kGroundY});
                    window.draw(citySprite);
                }
                for (std::size_t i = 0; i < 3; ++i)
                {
                    if (!game.baseAlive[i])
                        continue;
                    baseSprite.setPosition({kBaseX[i], kGroundY});
                    window.draw(baseSprite);
                    drawText(window, font, std::to_string(game.ammo[i]), 16, {kBaseX[i], 518.f}, 1);
                }

                sf::VertexArray missileLines(sf::PrimitiveType::Triangles);
                for (const Missile& m : game.missiles)
                    appendThickLine(missileLines, m.start, m.pos, kMissileTrail, 2.f);
                window.draw(missileLines);

                sf::VertexArray interceptorLines(sf::PrimitiveType::Triangles);
                for (const Interceptor& it : game.interceptors)
                    appendThickLine(interceptorLines, it.start, it.pos, kInterceptorTrail, 2.f);
                window.draw(interceptorLines);

                for (const Missile& m : game.missiles)
                {
                    head.setPosition(m.pos);
                    window.draw(head);
                }

                for (const Explosion& e : game.explosions)
                {
                    const float r = explosionRadius(e.age);
                    if (r <= 0.f)
                        continue;
                    circle.setRadius(r);
                    circle.setOrigin({r, r});
                    circle.setPosition(e.center);
                    const int phase = static_cast<int>(e.age / 0.1f);
                    circle.setFillColor(phase % 2 == 0 ? sf::Color::White : kExplosionAlt);
                    window.draw(circle);
                }

                drawText(window, font, "SCORE " + std::to_string(game.score), 16, {16.f, 18.f}, 0);
                drawText(window, font, "WAVE " + std::to_string(game.wave), 16, {kWidth - 16.f, 18.f}, 2);

                if (game.clearTimer > 0.f)
                    drawText(window, font, "WAVE " + std::to_string(game.wave) + " CLEAR", 32,
                             {kWidth / 2.f, kHeight / 2.f - 40.f}, 1);
            }
            else
            {
                drawText(window, font, "GAME OVER", 48, {kWidth / 2.f, 170.f}, 1);
                drawText(window, font, "SCORE " + std::to_string(game.score), 32, {kWidth / 2.f, 250.f}, 1);
                drawText(window, font, "WAVE " + std::to_string(game.wave), 32, {kWidth / 2.f, 300.f}, 1);
                drawText(window, font, "R: RESTART  ESC: QUIT", 16, {kWidth / 2.f, 380.f}, 1);
            }

            const sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window), view);
            crossSprite.setPosition(mouse);
            window.draw(crossSprite);

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
