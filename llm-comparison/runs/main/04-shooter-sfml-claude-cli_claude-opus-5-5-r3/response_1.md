SFML 3.1 기준으로 요구사항을 모두 반영한 `main.cpp`입니다.

- **고정 시간 간격**: 갱신은 1/60초 단위로 돌려 프레임레이트가 달라져도 진행 속도가 같습니다.
- **짧은 입력**: 한 프레임보다 짧은 키·마우스 입력은 래치로 붙잡아 다음 갱신에 반영합니다.
- **화면 비율**: 창 크기를 바꾸면 월드 뷰와 HUD 뷰에 같은 레터박스 뷰포트를 적용합니다.
- **조준**: `mapPixelToCoords`로 계산하므로 카메라가 움직이거나 창 크기가 바뀌어도 커서를 정확히 가리킵니다.

```cpp
// ARENA - 탑다운 슈터 (SFML 3.1, C++20)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kDt = 1.f / 60.f;

constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerMargin = 16.f;

constexpr float kFireInterval = 0.15f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletLife = 1.4f;

constexpr float kEnemySpeed = 100.f;
constexpr float kEnemyRadius = 14.f;
constexpr std::size_t kMaxEnemies = 200;

constexpr float kInvulnTime = 1.f;
constexpr int kStartHp = 3;

const sf::Color kBgColor(0x14, 0x18, 0x1F);
const sf::Color kGridColor(0x22, 0x2A, 0x35);
const sf::Color kBorderColor(0x5C, 0x6B, 0x80);
const sf::Color kTextColor(0xFF, 0xFF, 0xFF);

enum class Screen
{
    Title,
    Playing,
    Result
};

struct Bullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float life = 0.f;
    bool dead = false;
};

struct Enemy
{
    sf::Vector2f pos;
    bool dead = false;
};

struct SfxPool
{
    std::vector<sf::Sound> sounds;
    std::size_t next = 0;

    SfxPool(const sf::SoundBuffer& buffer, std::size_t count)
    {
        sounds.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
            sounds.emplace_back(buffer);
    }

    void play()
    {
        // 멈춰 있는 소리를 우선 사용하고, 없으면 순환
        for (std::size_t i = 0; i < sounds.size(); ++i)
        {
            std::size_t idx = (next + i) % sounds.size();
            if (sounds[idx].getStatus() != sf::Sound::Status::Playing)
            {
                sounds[idx].play();
                next = (idx + 1) % sounds.size();
                return;
            }
        }
        sounds[next].stop();
        sounds[next].play();
        next = (next + 1) % sounds.size();
    }

    void stopAll()
    {
        for (auto& s : sounds)
            s.stop();
    }
};

float lengthOf(sf::Vector2f v)
{
    return std::sqrt(v.x * v.x + v.y * v.y);
}

sf::FloatRect computeViewport(sf::Vector2u size)
{
    const float w = static_cast<float>(std::max(1u, size.x));
    const float h = static_cast<float>(std::max(1u, size.y));
    const float targetRatio = kViewW / kViewH;
    const float ratio = w / h;
    if (ratio > targetRatio)
    {
        const float vw = (h * targetRatio) / w;
        return sf::FloatRect({(1.f - vw) * 0.5f, 0.f}, {vw, 1.f});
    }
    const float vh = (w / targetRatio) / h;
    return sf::FloatRect({0.f, (1.f - vh) * 0.5f}, {1.f, vh});
}

void centerOrigin(sf::Text& text)
{
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
}

sf::Vector2f clampCamera(sf::Vector2f p)
{
    return {std::clamp(p.x, kViewW * 0.5f, kWorldW - kViewW * 0.5f),
            std::clamp(p.y, kViewH * 0.5f, kWorldH - kViewH * 0.5f)};
}
} // namespace

int runGame()
{
    // ---- 에셋 로드 ----
    const std::string fontPath = "assets/fonts/game.ttf";
    const std::string hitPath = "assets/sfx/hit.wav";
    const std::string losePath = "assets/sfx/lose.wav";
    const std::string shootPath = "assets/sfx/shoot.wav";
    const std::string musicPath = "assets/music/bgm_shooter.ogg";
    const std::string playerPath = "assets/textures/player.png";
    const std::string enemyPath = "assets/textures/enemy.png";
    const std::string bulletPath = "assets/textures/bullet.png";

    sf::Font font;
    if (!font.openFromFile(fontPath))
    {
        std::cerr << "Failed to load asset: " << fontPath << '\n';
        return 1;
    }
    sf::SoundBuffer hitBuf, loseBuf, shootBuf;
    if (!hitBuf.loadFromFile(hitPath))
    {
        std::cerr << "Failed to load asset: " << hitPath << '\n';
        return 1;
    }
    if (!loseBuf.loadFromFile(losePath))
    {
        std::cerr << "Failed to load asset: " << losePath << '\n';
        return 1;
    }
    if (!shootBuf.loadFromFile(shootPath))
    {
        std::cerr << "Failed to load asset: " << shootPath << '\n';
        return 1;
    }
    sf::Music music;
    if (!music.openFromFile(musicPath))
    {
        std::cerr << "Failed to load asset: " << musicPath << '\n';
        return 1;
    }
    sf::Texture playerTex, enemyTex, bulletTex;
    if (!playerTex.loadFromFile(playerPath))
    {
        std::cerr << "Failed to load asset: " << playerPath << '\n';
        return 1;
    }
    if (!enemyTex.loadFromFile(enemyPath))
    {
        std::cerr << "Failed to load asset: " << enemyPath << '\n';
        return 1;
    }
    if (!bulletTex.loadFromFile(bulletPath))
    {
        std::cerr << "Failed to load asset: " << bulletPath << '\n';
        return 1;
    }

    music.setLooping(true);
    music.setVolume(40.f);

    SfxPool shootSfx(shootBuf, 6);
    SfxPool hitSfx(hitBuf, 6);
    SfxPool loseSfx(loseBuf, 2);

    // ---- 창 ----
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::View worldView({kWorldW * 0.5f, kWorldH * 0.5f}, {kViewW, kViewH});
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {kViewW, kViewH}));
    sf::FloatRect viewport = computeViewport(window.getSize());
    worldView.setViewport(viewport);
    uiView.setViewport(viewport);

    // ---- 정적 그래픽 ----
    sf::VertexArray grid(sf::PrimitiveType::Lines);
    for (float x = 120.f; x < kWorldW; x += 120.f)
    {
        grid.append(sf::Vertex{{x, 0.f}, kGridColor});
        grid.append(sf::Vertex{{x, kWorldH}, kGridColor});
    }
    for (float y = 120.f; y < kWorldH; y += 120.f)
    {
        grid.append(sf::Vertex{{0.f, y}, kGridColor});
        grid.append(sf::Vertex{{kWorldW, y}, kGridColor});
    }

    sf::RectangleShape border({kWorldW, kWorldH});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(kBorderColor);
    border.setOutlineThickness(-2.f);

    sf::RectangleShape background({kViewW, kViewH});
    background.setFillColor(kBgColor);

    sf::Sprite playerSprite(playerTex);
    playerSprite.setOrigin({16.f, 16.f});
    sf::Sprite enemySprite(enemyTex);
    enemySprite.setOrigin({16.f, 16.f});
    sf::Sprite bulletSprite(bulletTex);
    bulletSprite.setOrigin({4.f, 4.f});

    sf::Text titleText(font, "ARENA", 48);
    titleText.setFillColor(kTextColor);
    centerOrigin(titleText);
    titleText.setPosition({kViewW * 0.5f, kViewH * 0.5f - 40.f});

    sf::Text pressText(font, "PRESS ENTER", 32);
    pressText.setFillColor(kTextColor);
    centerOrigin(pressText);
    pressText.setPosition({kViewW * 0.5f, kViewH * 0.5f + 32.f});

    sf::Text pausedText(font, "PAUSED", 48);
    pausedText.setFillColor(kTextColor);
    centerOrigin(pausedText);
    pausedText.setPosition({kViewW * 0.5f, kViewH * 0.5f});

    sf::Text hudText(font, "", 16);
    hudText.setFillColor(kTextColor);
    hudText.setPosition({8.f, 8.f});

    sf::Text resultTitle(font, "GAME OVER", 48);
    resultTitle.setFillColor(kTextColor);
    sf::Text resultScore(font, "", 32);
    resultScore.setFillColor(kTextColor);
    sf::Text resultTime(font, "", 32);
    resultTime.setFillColor(kTextColor);
    sf::Text resultHint(font, "R: RESTART  ESC: QUIT", 32);
    resultHint.setFillColor(kTextColor);

    // ---- 게임 상태 ----
    std::mt19937 rng(std::random_device{}());

    Screen screen = Screen::Title;
    float titleTimer = 0.f;
    bool paused = false;

    sf::Vector2f playerPos{kWorldW * 0.5f, kWorldH * 0.5f};
    int hp = kStartHp;
    int score = 0;
    float playTime = 0.f;
    float invuln = 0.f;
    float fireCooldown = 0.f;
    float spawnTimer = 0.5f;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    bullets.reserve(64);
    enemies.reserve(kMaxEnemies + 8);

    // 입력 상태
    const std::array<sf::Keyboard::Scancode, 8> moveKeys = {
        sf::Keyboard::Scan::W,  sf::Keyboard::Scan::Up,   sf::Keyboard::Scan::S,    sf::Keyboard::Scan::Down,
        sf::Keyboard::Scan::A,  sf::Keyboard::Scan::Left, sf::Keyboard::Scan::D,    sf::Keyboard::Scan::Right};
    // 인덱스/2 → 0:위 1:아래 2:왼쪽 3:오른쪽
    std::array<bool, 8> keyHeld{};
    std::array<bool, 8> keyTapped{};
    bool mouseHeld = false;
    bool mouseTapped = false;

    auto resetGame = [&]()
    {
        playerPos = {kWorldW * 0.5f, kWorldH * 0.5f};
        hp = kStartHp;
        score = 0;
        playTime = 0.f;
        invuln = 0.f;
        fireCooldown = 0.f;
        spawnTimer = 0.5f;
        bullets.clear();
        enemies.clear();
        paused = false;
        mouseTapped = false;
        keyTapped.fill(false);
    };

    auto startPlay = [&]()
    {
        resetGame();
        screen = Screen::Playing;
        music.stop();
        music.play();
    };

    auto cameraCenter = [&]() { return clampCamera(playerPos); };

    auto mouseWorld = [&]()
    {
        worldView.setCenter(cameraCenter());
        const sf::Vector2i pix = sf::Mouse::getPosition(window);
        return window.mapPixelToCoords(pix, worldView);
    };

    auto spawnEnemy = [&]()
    {
        const sf::Vector2f c = cameraCenter();
        const float L = c.x - kViewW * 0.5f - 64.f;
        const float R = c.x + kViewW * 0.5f + 64.f;
        const float T = c.y - kViewH * 0.5f - 64.f;
        const float B = c.y + kViewH * 0.5f + 64.f;

        struct Seg
        {
            sf::Vector2f a, b;
        };
        std::vector<Seg> segs;
        const float x0 = std::max(L, 0.f), x1 = std::min(R, kWorldW);
        const float y0 = std::max(T, 0.f), y1 = std::min(B, kWorldH);
        if (T >= 0.f && T <= kWorldH && x1 > x0)
            segs.push_back({{x0, T}, {x1, T}});
        if (B >= 0.f && B <= kWorldH && x1 > x0)
            segs.push_back({{x0, B}, {x1, B}});
        if (L >= 0.f && L <= kWorldW && y1 > y0)
            segs.push_back({{L, y0}, {L, y1}});
        if (R >= 0.f && R <= kWorldW && y1 > y0)
            segs.push_back({{R, y0}, {R, y1}});

        sf::Vector2f pos;
        float total = 0.f;
        for (const auto& s : segs)
            total += lengthOf(s.b - s.a);
        if (segs.empty() || total <= 0.f)
        {
            std::uniform_real_distribution<float> dx(0.f, kWorldW), dy(0.f, kWorldH);
            pos = {dx(rng), dy(rng)};
        }
        else
        {
            std::uniform_real_distribution<float> d(0.f, total);
            float r = d(rng);
            pos = segs.back().b;
            for (const auto& s : segs)
            {
                const float len = lengthOf(s.b - s.a);
                if (r <= len)
                {
                    const float t = len > 0.f ? r / len : 0.f;
                    pos = s.a + (s.b - s.a) * t;
                    break;
                }
                r -= len;
            }
        }
        enemies.push_back({pos, false});
    };

    auto updatePlaying = [&](float dt)
    {
        // 플레이어 이동
        auto dirActive = [&](int dir)
        {
            const int a = dir * 2, b = dir * 2 + 1;
            return keyHeld[a] || keyHeld[b] || keyTapped[a] || keyTapped[b];
        };
        sf::Vector2f move{0.f, 0.f};
        if (dirActive(0)) move.y -= 1.f;
        if (dirActive(1)) move.y += 1.f;
        if (dirActive(2)) move.x -= 1.f;
        if (dirActive(3)) move.x += 1.f;
        const float ml = lengthOf(move);
        if (ml > 0.f)
            playerPos += move / ml * kPlayerSpeed * dt;
        playerPos.x = std::clamp(playerPos.x, kPlayerMargin, kWorldW - kPlayerMargin);
        playerPos.y = std::clamp(playerPos.y, kPlayerMargin, kWorldH - kPlayerMargin);

        // 사격
        const bool firing = mouseHeld || mouseTapped;
        fireCooldown -= dt;
        if (firing && fireCooldown <= 1e-5f)
        {
            sf::Vector2f dir = mouseWorld() - playerPos;
            const float dl = lengthOf(dir);
            dir = dl > 1e-4f ? dir / dl : sf::Vector2f{1.f, 0.f};
            bullets.push_back({playerPos, dir * kBulletSpeed, kBulletLife, false});
            shootSfx.play();
            fireCooldown += kFireInterval;
            if (fireCooldown < 0.f)
                fireCooldown = kFireInterval;
        }
        if (!firing && fireCooldown < 0.f)
            fireCooldown = 0.f;

        // 총알 이동
        for (auto& b : bullets)
        {
            b.pos += b.vel * dt;
            b.life -= dt;
            if (b.life <= 0.f || b.pos.x < 0.f || b.pos.x > kWorldW || b.pos.y < 0.f || b.pos.y > kWorldH)
                b.dead = true;
        }

        // 적 생성
        spawnTimer -= dt;
        if (spawnTimer <= 1e-5f)
        {
            if (enemies.size() < kMaxEnemies)
                spawnEnemy();
            const float steps = std::floor(playTime / 10.f);
            const float interval = std::max(0.3f, 1.0f - 0.1f * steps);
            spawnTimer += interval;
            if (spawnTimer <= 0.f)
                spawnTimer = interval;
        }

        // 적 이동
        for (auto& e : enemies)
        {
            sf::Vector2f d = playerPos - e.pos;
            const float l = lengthOf(d);
            if (l > 1e-4f)
            {
                const float step = kEnemySpeed * dt;
                if (step >= l)
                    e.pos = playerPos;
                else
                    e.pos += d / l * step;
            }
        }

        // 총알-적 충돌
        constexpr float beR = kBulletRadius + kEnemyRadius;
        for (auto& b : bullets)
        {
            if (b.dead)
                continue;
            for (auto& e : enemies)
            {
                if (e.dead)
                    continue;
                const sf::Vector2f d = b.pos - e.pos;
                if (d.x * d.x + d.y * d.y < beR * beR)
                {
                    b.dead = true;
                    e.dead = true;
                    score += 10;
                    hitSfx.play();
                    break;
                }
            }
        }

        // 적-플레이어 충돌
        if (invuln > 0.f)
            invuln = std::max(0.f, invuln - dt);
        if (invuln <= 0.f)
        {
            constexpr float epR = kEnemyRadius + kPlayerRadius;
            for (auto& e : enemies)
            {
                if (e.dead)
                    continue;
                const sf::Vector2f d = e.pos - playerPos;
                if (d.x * d.x + d.y * d.y < epR * epR)
                {
                    e.dead = true;
                    hp -= 1;
                    invuln = kInvulnTime;
                    loseSfx.play();
                    break;
                }
            }
        }

        bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) { return b.dead; }),
                      bullets.end());
        enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.dead; }),
                      enemies.end());

        playTime += dt;

        if (hp <= 0)
        {
            hp = 0;
            screen = Screen::Result;
            paused = false;
            music.stop();
        }
    };

    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen())
    {
        // ---- 이벤트 ----
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                viewport = computeViewport(resized->size);
                worldView.setViewport(viewport);
                uiView.setViewport(viewport);
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                keyHeld.fill(false);
                mouseHeld = false;
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                const auto sc = kp->scancode;
                if (sc == sf::Keyboard::Scan::Escape)
                {
                    window.close();
                    continue;
                }
                for (std::size_t i = 0; i < moveKeys.size(); ++i)
                {
                    if (moveKeys[i] == sc)
                    {
                        keyHeld[i] = true;
                        keyTapped[i] = true;
                    }
                }
                if (screen == Screen::Title &&
                    (sc == sf::Keyboard::Scan::Enter || sc == sf::Keyboard::Scan::NumpadEnter))
                {
                    startPlay();
                }
                else if (screen == Screen::Result && sc == sf::Keyboard::Scan::R)
                {
                    startPlay();
                }
                else if (screen == Screen::Playing && sc == sf::Keyboard::Scan::P)
                {
                    paused = !paused;
                    if (paused)
                        music.pause();
                    else
                        music.play();
                }
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                for (std::size_t i = 0; i < moveKeys.size(); ++i)
                    if (moveKeys[i] == kr->scancode)
                        keyHeld[i] = false;
            }
            else if (const auto* mp = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mp->button == sf::Mouse::Button::Left)
                {
                    mouseHeld = true;
                    mouseTapped = true;
                }
            }
            else if (const auto* mr = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mr->button == sf::Mouse::Button::Left)
                    mouseHeld = false;
            }
        }
        if (!window.isOpen())
            break;

        // ---- 고정 시간 갱신 ----
        accumulator += clock.restart().asSeconds();
        if (accumulator > 0.25f)
            accumulator = 0.25f;
        while (accumulator >= kDt)
        {
            accumulator -= kDt;
            if (screen == Screen::Title)
            {
                titleTimer += kDt;
                if (titleTimer >= 2.f)
                    startPlay();
            }
            else if (screen == Screen::Playing && !paused)
            {
                updatePlaying(kDt);
            }
            keyTapped.fill(false);
            mouseTapped = false;
        }

        // ---- 그리기 ----
        window.clear(sf::Color::Black);
        window.setView(uiView);
        window.draw(background);

        if (screen == Screen::Playing)
        {
            worldView.setCenter(cameraCenter());
            window.setView(worldView);
            window.draw(grid);
            window.draw(border);

            for (const auto& b : bullets)
            {
                bulletSprite.setPosition(b.pos);
                window.draw(bulletSprite);
            }
            for (const auto& e : enemies)
            {
                enemySprite.setPosition(e.pos);
                window.draw(enemySprite);
            }

            const sf::Vector2f aim = window.mapPixelToCoords(sf::Mouse::getPosition(window), worldView) - playerPos;
            if (lengthOf(aim) > 1e-4f)
                playerSprite.setRotation(sf::radians(std::atan2(aim.y, aim.x)));
            playerSprite.setPosition(playerPos);
            if (invuln > 0.f)
            {
                const bool dim = (static_cast<int>(invuln * 10.f) % 2) == 0;
                playerSprite.setColor(sf::Color(255, 255, 255, dim ? 90 : 200));
            }
            else
            {
                playerSprite.setColor(sf::Color::White);
            }
            window.draw(playerSprite);

            window.setView(uiView);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "HP %d  SCORE %d  TIME %.1f", hp, score, playTime);
            hudText.setString(buf);
            window.draw(hudText);
            if (paused)
                window.draw(pausedText);
        }
        else if (screen == Screen::Title)
        {
            window.setView(uiView);
            window.draw(titleText);
            window.draw(pressText);
        }
        else
        {
            window.setView(uiView);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "SCORE %d", score);
            resultScore.setString(buf);
            std::snprintf(buf, sizeof(buf), "TIME %.1f", playTime);
            resultTime.setString(buf);

            centerOrigin(resultTitle);
            centerOrigin(resultScore);
            centerOrigin(resultTime);
            centerOrigin(resultHint);
            resultTitle.setPosition({kViewW * 0.5f, 150.f});
            resultScore.setPosition({kViewW * 0.5f, 240.f});
            resultTime.setPosition({kViewW * 0.5f, 290.f});
            resultHint.setPosition({kViewW * 0.5f, 380.f});
            window.draw(resultTitle);
            window.draw(resultScore);
            window.draw(resultTime);
            window.draw(resultHint);
        }

        window.display();
    }

    music.stop();
    shootSfx.stopAll();
    hitSfx.stopAll();
    loseSfx.stopAll();
    return 0;
}

int main()
{
    try
    {
        return runGame();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
```