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
constexpr float kW = 960.f;
constexpr float kH = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kObjTopY = 476.f; // 500 - 24
constexpr std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr std::array<float, 3> kBaseX{60.f, 480.f, 900.f};

const sf::Color kGroundColor(0xB0, 0x7A, 0x2A);
const sf::Color kMissileColor(0xFF, 0x4D, 0x4D);
const sf::Color kInterceptColor(0x4F, 0xC3, 0xF7);
const sf::Color kExplYellow(0xFF, 0xD5, 0x4A);

struct SoundPool
{
    std::vector<sf::Sound> sounds;
    std::size_t next = 0;

    SoundPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            sounds.emplace_back(buffer);
    }

    void play()
    {
        for (auto& s : sounds)
        {
            if (s.getStatus() == sf::Sound::Status::Stopped)
            {
                s.play();
                return;
            }
        }
        sounds[next].stop();
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }
};

struct Explosion
{
    sf::Vector2f center;
    float age = 0.f;
};

struct Interceptor
{
    sf::Vector2f start;
    sf::Vector2f target;
    sf::Vector2f pos;
};

struct Missile
{
    sf::Vector2f start;
    sf::Vector2f target;
    sf::Vector2f pos;
    bool targetIsCity = true;
    int targetIndex = 0;
    float speed = 70.f;
};

float explosionRadius(float age)
{
    if (age < 0.f)
        return 0.f;
    if (age < 0.5f)
        return 96.f * age;
    return std::max(0.f, 96.f * (1.f - age));
}

sf::View makeView(sf::Vector2u size)
{
    sf::View v(sf::FloatRect({0.f, 0.f}, {kW, kH}));
    if (size.x == 0 || size.y == 0)
        return v;
    const float target = kW / kH;
    const float ratio = static_cast<float>(size.x) / static_cast<float>(size.y);
    float vx = 0.f, vy = 0.f, vw = 1.f, vh = 1.f;
    if (ratio > target)
    {
        vw = target / ratio;
        vx = (1.f - vw) * 0.5f;
    }
    else
    {
        vh = ratio / target;
        vy = (1.f - vh) * 0.5f;
    }
    v.setViewport(sf::FloatRect({vx, vy}, {vw, vh}));
    return v;
}

void addLine(std::vector<sf::Vertex>& verts, sf::Vector2f a, sf::Vector2f b, sf::Color color, float thickness)
{
    const sf::Vector2f d = b - a;
    const float len = d.length();
    if (len < 0.0001f)
        return;
    const sf::Vector2f n = sf::Vector2f(-d.y / len, d.x / len) * (thickness * 0.5f);
    const sf::Vertex v0{a + n, color};
    const sf::Vertex v1{a - n, color};
    const sf::Vertex v2{b + n, color};
    const sf::Vertex v3{b - n, color};
    verts.push_back(v0);
    verts.push_back(v1);
    verts.push_back(v2);
    verts.push_back(v2);
    verts.push_back(v1);
    verts.push_back(v3);
}

enum class Screen
{
    Title,
    Play,
    Result
};

} // namespace

int main()
{
    // ---- Asset loading ----
    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf"))
    {
        std::cerr << "Failed to load: assets/fonts/game.ttf\n";
        return 1;
    }
    sf::Texture texCity, texBase, texCross;
    if (!texCity.loadFromFile("assets/textures/md_city.png"))
    {
        std::cerr << "Failed to load: assets/textures/md_city.png\n";
        return 1;
    }
    if (!texBase.loadFromFile("assets/textures/md_base.png"))
    {
        std::cerr << "Failed to load: assets/textures/md_base.png\n";
        return 1;
    }
    if (!texCross.loadFromFile("assets/textures/md_crosshair.png"))
    {
        std::cerr << "Failed to load: assets/textures/md_crosshair.png\n";
        return 1;
    }
    sf::SoundBuffer bufLaunch, bufExplode, bufLose;
    if (!bufLaunch.loadFromFile("assets/sfx/launch.wav"))
    {
        std::cerr << "Failed to load: assets/sfx/launch.wav\n";
        return 1;
    }
    if (!bufExplode.loadFromFile("assets/sfx/explode.wav"))
    {
        std::cerr << "Failed to load: assets/sfx/explode.wav\n";
        return 1;
    }
    if (!bufLose.loadFromFile("assets/sfx/lose.wav"))
    {
        std::cerr << "Failed to load: assets/sfx/lose.wav\n";
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile("assets/music/bgm_missile.ogg"))
    {
        std::cerr << "Failed to load: assets/music/bgm_missile.ogg\n";
        return 1;
    }
    music.setLooping(true);
    music.setVolume(40.f);

    SoundPool sndLaunch(bufLaunch, 6);
    SoundPool sndExplode(bufExplode, 10);
    SoundPool sndLose(bufLose, 4);

    // ---- Window ----
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
    window.setFramerateLimit(60);
    sf::View view = makeView(window.getSize());

    sf::Sprite citySprite(texCity);
    sf::Sprite baseSprite(texBase);
    sf::Sprite crossSprite(texCross);
    crossSprite.setOrigin({12.f, 12.f});
    sf::Text text(font, "", 16);
    text.setFillColor(sf::Color::White);

    std::mt19937 rng(std::random_device{}());

    // ---- Game state ----
    Screen screen = Screen::Title;
    float titleTime = 0.f;

    std::array<bool, 6> cityAlive{};
    std::array<bool, 3> baseAlive{};
    std::array<int, 3> baseAmmo{};
    std::vector<Explosion> explosions;
    std::vector<Interceptor> interceptors;
    std::vector<Missile> missiles;
    int score = 0;
    int wave = 1;
    int spawned = 0;
    int total = 10;
    float waveTime = 0.f;
    float clearTimer = 0.f;
    int clearedWave = 0;

    auto startWave = [&](int n)
    {
        wave = n;
        spawned = 0;
        total = 10 + 2 * (n - 1);
        waveTime = 0.f;
        clearTimer = 0.f;
        baseAlive.fill(true);
        baseAmmo.fill(10);
    };

    auto startPlay = [&]()
    {
        cityAlive.fill(true);
        explosions.clear();
        interceptors.clear();
        missiles.clear();
        score = 0;
        startWave(1);
        screen = Screen::Play;
        music.stop();
        music.play();
    };

    auto addExplosion = [&](sf::Vector2f p)
    {
        explosions.push_back({p, 0.f});
        sndExplode.play();
    };

    auto cityCount = [&]()
    {
        int c = 0;
        for (bool a : cityAlive)
            c += a ? 1 : 0;
        return c;
    };

    auto tryFire = [&](sf::Vector2f p)
    {
        if (p.x < 0.f || p.x > kW || p.y < 0.f || p.y >= 470.f)
            return;
        int best = -1;
        float bestDist = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            if (!baseAlive[i] || baseAmmo[i] <= 0)
                continue;
            const float d = std::abs(kBaseX[i] - p.x);
            if (best < 0 || d < bestDist)
            {
                best = i;
                bestDist = d;
            }
        }
        if (best < 0)
            return;
        --baseAmmo[best];
        const sf::Vector2f start(kBaseX[best], kObjTopY);
        interceptors.push_back({start, p, start});
        sndLaunch.play();
    };

    auto spawnMissile = [&]()
    {
        std::vector<std::pair<bool, int>> targets;
        for (int i = 0; i < 6; ++i)
            if (cityAlive[i])
                targets.push_back({true, i});
        for (int i = 0; i < 3; ++i)
            if (baseAlive[i])
                targets.push_back({false, i});
        if (targets.empty())
            return;
        std::uniform_int_distribution<std::size_t> pick(0, targets.size() - 1);
        const auto [isCity, idx] = targets[pick(rng)];
        std::uniform_real_distribution<float> xDist(0.f, kW);
        Missile m;
        m.start = {xDist(rng), 0.f};
        m.pos = m.start;
        m.targetIsCity = isCity;
        m.targetIndex = idx;
        m.target = {isCity ? kCityX[idx] : kBaseX[idx], kObjTopY};
        m.speed = 70.f + 10.f * static_cast<float>(wave - 1);
        missiles.push_back(m);
    };

    auto stepPlay = [&](float h)
    {
        // Explosions age
        for (auto& e : explosions)
            e.age += h;
        explosions.erase(std::remove_if(explosions.begin(), explosions.end(),
                                        [](const Explosion& e) { return e.age >= 1.f; }),
                         explosions.end());

        // Wave timing / spawning
        if (clearTimer > 0.f)
        {
            clearTimer -= h;
            if (clearTimer <= 0.f)
                startWave(wave + 1);
        }
        else
        {
            waveTime += h;
            while (spawned < total && cityCount() > 0 &&
                   waveTime >= 1.f + 1.2f * static_cast<float>(spawned))
            {
                spawnMissile();
                ++spawned;
            }
        }

        // Interceptors
        for (std::size_t i = 0; i < interceptors.size();)
        {
            Interceptor& it = interceptors[i];
            const sf::Vector2f d = it.target - it.pos;
            const float dist = d.length();
            const float step = 600.f * h;
            if (dist <= step)
            {
                const sf::Vector2f p = it.target;
                interceptors.erase(interceptors.begin() + static_cast<std::ptrdiff_t>(i));
                addExplosion(p);
                continue;
            }
            it.pos += d / dist * step;
            ++i;
        }

        // Missiles movement
        for (std::size_t i = 0; i < missiles.size();)
        {
            Missile& m = missiles[i];
            const sf::Vector2f d = m.target - m.pos;
            const float dist = d.length();
            const float step = m.speed * h;
            if (dist <= step)
            {
                const Missile hit = m;
                missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(i));
                addExplosion(hit.target);
                bool destroyed = false;
                if (hit.targetIsCity)
                {
                    if (cityAlive[hit.targetIndex])
                    {
                        cityAlive[hit.targetIndex] = false;
                        destroyed = true;
                    }
                }
                else
                {
                    if (baseAlive[hit.targetIndex])
                    {
                        baseAlive[hit.targetIndex] = false;
                        destroyed = true;
                    }
                }
                if (destroyed)
                    sndLose.play();
                continue;
            }
            m.pos += d / dist * step;
            ++i;
        }

        // Missile vs explosion collisions (chain)
        for (std::size_t i = 0; i < missiles.size();)
        {
            const sf::Vector2f head = missiles[i].pos;
            bool hit = false;
            for (const auto& e : explosions)
            {
                const float r = explosionRadius(e.age);
                if (r > 0.f && (head - e.center).lengthSquared() <= r * r)
                {
                    hit = true;
                    break;
                }
            }
            if (hit)
            {
                missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(i));
                score += 25;
                addExplosion(head);
                continue;
            }
            ++i;
        }

        // End conditions
        if (cityCount() == 0)
        {
            if (explosions.empty())
            {
                screen = Screen::Result;
                music.stop();
            }
        }
        else if (clearTimer <= 0.f && spawned >= total && missiles.empty() && interceptors.empty() &&
                 explosions.empty())
        {
            for (int i = 0; i < 3; ++i)
                if (baseAlive[i])
                    score += 5 * baseAmmo[i];
            score += 100 * cityCount();
            clearedWave = wave;
            clearTimer = 3.f;
        }
    };

    auto drawCentered = [&](const std::string& s, unsigned int size, sf::Vector2f center)
    {
        text.setString(s);
        text.setCharacterSize(size);
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
        text.setPosition(center);
        window.draw(text);
    };

    auto drawAt = [&](const std::string& s, unsigned int size, sf::Vector2f pos, bool rightAlign)
    {
        text.setString(s);
        text.setCharacterSize(size);
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({rightAlign ? b.position.x + b.size.x : b.position.x, 0.f});
        text.setPosition(pos);
        window.draw(text);
    };

    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    std::vector<sf::Vector2f> pendingClicks;
    std::vector<sf::Vertex> lineVerts;
    sf::Clock clock;

    while (window.isOpen())
    {
        bool startRequested = false;
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                view = makeView(resized->size);
            }
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->code == sf::Keyboard::Key::Escape)
                    window.close();
                else if (key->code == sf::Keyboard::Key::Enter && screen == Screen::Title)
                    startRequested = true;
                else if (key->code == sf::Keyboard::Key::R && screen == Screen::Result)
                    startRequested = true;
            }
            else if (const auto* mm = event->getIf<sf::Event::MouseMoved>())
            {
                mousePixel = mm->position;
            }
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                mousePixel = mb->position;
                if (mb->button == sf::Mouse::Button::Left)
                    pendingClicks.push_back(window.mapPixelToCoords(mb->position, view));
            }
        }
        if (!window.isOpen())
            break;

        float dt = clock.restart().asSeconds();
        dt = std::min(dt, 0.1f);

        if (startRequested)
        {
            startPlay();
            pendingClicks.clear();
        }

        // Apply clicks
        if (screen == Screen::Play && clearTimer <= 0.f)
        {
            for (const auto& p : pendingClicks)
                tryFire(p);
        }
        pendingClicks.clear();

        // Update with substeps
        const int steps = std::max(1, static_cast<int>(std::ceil(dt / (1.f / 120.f))));
        const float h = dt / static_cast<float>(steps);
        for (int s = 0; s < steps; ++s)
        {
            if (screen == Screen::Title)
            {
                titleTime += h;
                if (titleTime >= 2.f)
                {
                    startPlay();
                }
            }
            else if (screen == Screen::Play)
            {
                stepPlay(h);
            }
        }

        // ---- Draw ----
        window.setView(window.getDefaultView());
        window.clear(sf::Color::Black);
        window.setView(view);

        if (screen == Screen::Title)
        {
            drawCentered("SKY SHIELD", 48, {kW * 0.5f, 220.f});
            drawCentered("PRESS ENTER", 32, {kW * 0.5f, 320.f});
        }
        else if (screen == Screen::Play)
        {
            sf::RectangleShape ground({kW, 40.f});
            ground.setPosition({0.f, kGroundY});
            ground.setFillColor(kGroundColor);
            window.draw(ground);

            for (int i = 0; i < 6; ++i)
            {
                if (!cityAlive[i])
                    continue;
                citySprite.setPosition({kCityX[i] - 24.f, kObjTopY});
                window.draw(citySprite);
            }
            for (int i = 0; i < 3; ++i)
            {
                if (!baseAlive[i])
                    continue;
                baseSprite.setPosition({kBaseX[i] - 24.f, kObjTopY});
                window.draw(baseSprite);
                drawCentered(std::to_string(baseAmmo[i]), 16, {kBaseX[i], 518.f});
            }

            lineVerts.clear();
            for (const auto& m : missiles)
                addLine(lineVerts, m.start, m.pos, kMissileColor, 2.f);
            for (const auto& it : interceptors)
                addLine(lineVerts, it.start, it.pos, kInterceptColor, 2.f);
            if (!lineVerts.empty())
                window.draw(lineVerts.data(), lineVerts.size(), sf::PrimitiveType::Triangles);

            sf::RectangleShape head({4.f, 4.f});
            head.setOrigin({2.f, 2.f});
            head.setFillColor(sf::Color::White);
            for (const auto& m : missiles)
            {
                head.setPosition(m.pos);
                window.draw(head);
            }

            sf::CircleShape circle(1.f, 48);
            for (const auto& e : explosions)
            {
                const float r = explosionRadius(e.age);
                if (r <= 0.f)
                    continue;
                circle.setRadius(r);
                circle.setOrigin({r, r});
                circle.setPosition(e.center);
                const int phase = static_cast<int>(std::floor(e.age / 0.1f));
                circle.setFillColor(phase % 2 == 0 ? sf::Color::White : kExplYellow);
                window.draw(circle);
            }

            drawAt("SCORE " + std::to_string(score), 16, {16.f, 12.f}, false);
            drawAt("WAVE " + std::to_string(wave), 16, {kW - 16.f, 12.f}, true);

            if (clearTimer > 0.f)
                drawCentered("WAVE " + std::to_string(clearedWave) + " CLEAR", 32, {kW * 0.5f, 240.f});

            crossSprite.setPosition(window.mapPixelToCoords(mousePixel, view));
            window.draw(crossSprite);
        }
        else
        {
            drawCentered("GAME OVER", 48, {kW * 0.5f, 170.f});
            drawCentered("SCORE " + std::to_string(score), 32, {kW * 0.5f, 250.f});
            drawCentered("WAVE " + std::to_string(wave), 32, {kW * 0.5f, 300.f});
            drawCentered("R: RESTART  ESC: QUIT", 16, {kW * 0.5f, 380.f});
        }

        window.display();
    }

    music.stop();
    return 0;
}
