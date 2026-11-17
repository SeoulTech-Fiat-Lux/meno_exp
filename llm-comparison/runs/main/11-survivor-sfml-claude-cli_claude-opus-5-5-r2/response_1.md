`main.cpp` 전체 코드입니다. 빌드나 실행은 해 보지 않았으니, 기준 환경에서 빌드하고 무입력 실행으로 한 번 확인해 주세요.

명세가 모호한 곳은 다음처럼 정했습니다.
- **갱신 방식:** 1/60초 고정 틱으로 진행해 프레임레이트와 무관하게 같은 속도를 냅니다. 입력은 이벤트로 모아 다음 틱에서 처리합니다.
- **짧은 입력:** 이동 키를 한 프레임보다 짧게 눌렀다 떼도 최소 한 틱은 움직입니다.
- **사격 대기:** 사격 간격이 지났는데 400px 안에 적이 없으면 기다렸다가, 적이 들어오는 즉시 쏩니다.
- **월드 경계:** 플레이어는 반지름 14만큼 안쪽으로, 밀려나는 적은 중심이 월드 안에 있도록 제한합니다.
- **월드 경계선:** 카메라가 월드 밖을 보여 주지 않으므로, 선이 보이도록 안쪽으로 2px 그립니다.
- **추가 표시:** 카드 왼쪽 위에 선택 키 번호(1·2·3)를 함께 표시합니다. 명세에 없는 표시이니 필요 없으면 지우면 됩니다.

```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr float WORLD_W = 2880.f;
constexpr float WORLD_H = 1620.f;
constexpr float VIEW_W  = 960.f;
constexpr float VIEW_H  = 540.f;
constexpr float DT      = 1.f / 60.f;

constexpr float PLAYER_RADIUS = 14.f;
constexpr float BULLET_RADIUS = 4.f;
constexpr float GEM_RADIUS    = 6.f;
constexpr int   MAX_ENEMIES   = 400;
constexpr float WIN_TIME      = 300.f;

sf::Color hex(std::uint32_t v)
{
    return sf::Color(static_cast<std::uint8_t>((v >> 16) & 0xFF),
                     static_cast<std::uint8_t>((v >> 8) & 0xFF),
                     static_cast<std::uint8_t>(v & 0xFF));
}

const sf::Color COL_BG       = hex(0x1A1420);
const sf::Color COL_GRID     = hex(0x2A2233);
const sf::Color COL_BORDER   = hex(0x6B5B7B);
const sf::Color COL_XP_BG    = hex(0x2A2233);
const sf::Color COL_XP_FILL  = hex(0x4FC3F7);
const sf::Color COL_HP_BG    = hex(0x3A1F24);
const sf::Color COL_HP_FILL  = hex(0xE5533D);
const sf::Color COL_CARD_BG  = hex(0x2A2233);
const sf::Color COL_CARD_OUT = hex(0xFFD54A);
const sf::Color COL_GEM1     = hex(0x4FC3F7);
const sf::Color COL_GEM5     = hex(0xFFD54A);
const sf::Color COL_HURT     = hex(0xFF6060);
const sf::Color COL_TEXT     = sf::Color::White;

struct EnemyType
{
    float radius;
    float hp;
    float speed;
    float dps;
    int   cell;
    int   gemValue;
    float animPeriod;
};

const std::array<EnemyType, 3> ETYPES{{
    {10.f, 10.f, 80.f, 10.f, 24, 1, 0.3f},  // walker
    {10.f, 6.f, 140.f, 6.f, 24, 1, 0.15f},  // runner
    {20.f, 60.f, 50.f, 20.f, 48, 5, 0.4f},  // brute
}};

const std::array<const char*, 6> ABILITY_NAMES{
    "FIRE RATE +15%", "DAMAGE +5", "PROJECTILE +1", "SPEED +10%", "MAX HP +20", "MAGNET +40"};

struct Enemy
{
    int          type = 0;
    sf::Vector2f pos;
    float        hp     = 0.f;
    float        knockT = 0.f;
    sf::Vector2f knockDir;
    float        flashT = 0.f;
    bool         dying  = false;
    float        dieT   = 0.f;
};

struct Bullet
{
    sf::Vector2f pos;
    sf::Vector2f vel;
    float        life   = 1.2f;
    float        damage = 10.f;
    bool         dead   = false;
};

struct Gem
{
    sf::Vector2f pos;
    int          value = 1;
    float        life  = 30.f;
    bool         dead  = false;
};

struct Assets
{
    sf::Font        font;
    sf::Texture     player, walker, runner, brute, bullet, gem;
    sf::SoundBuffer shoot, hit, pickup, levelup;
    sf::Music       music;
};

bool loadAssets(Assets& a)
{
    auto fail = [](const char* path)
    {
        std::cerr << "Failed to load asset: " << path << '\n';
        return false;
    };
    if (!a.font.openFromFile("assets/fonts/game.ttf"))
        return fail("assets/fonts/game.ttf");
    if (!a.player.loadFromFile("assets/textures/sv_player.png"))
        return fail("assets/textures/sv_player.png");
    if (!a.walker.loadFromFile("assets/textures/sv_walker.png"))
        return fail("assets/textures/sv_walker.png");
    if (!a.runner.loadFromFile("assets/textures/sv_runner.png"))
        return fail("assets/textures/sv_runner.png");
    if (!a.brute.loadFromFile("assets/textures/sv_brute.png"))
        return fail("assets/textures/sv_brute.png");
    if (!a.bullet.loadFromFile("assets/textures/sv_bullet.png"))
        return fail("assets/textures/sv_bullet.png");
    if (!a.gem.loadFromFile("assets/textures/sv_gem.png"))
        return fail("assets/textures/sv_gem.png");
    if (!a.shoot.loadFromFile("assets/sfx/shoot.wav"))
        return fail("assets/sfx/shoot.wav");
    if (!a.hit.loadFromFile("assets/sfx/hit.wav"))
        return fail("assets/sfx/hit.wav");
    if (!a.pickup.loadFromFile("assets/sfx/pickup.wav"))
        return fail("assets/sfx/pickup.wav");
    if (!a.levelup.loadFromFile("assets/sfx/levelup.wav"))
        return fail("assets/sfx/levelup.wav");
    if (!a.music.openFromFile("assets/music/bgm_survivor.ogg"))
        return fail("assets/music/bgm_survivor.ogg");
    return true;
}

struct SfxPool
{
    std::vector<std::unique_ptr<sf::Sound>> sounds;
    std::size_t                             index = 0;

    SfxPool(const sf::SoundBuffer& buffer, int count)
    {
        for (int i = 0; i < count; ++i)
            sounds.push_back(std::make_unique<sf::Sound>(buffer));
    }

    void play()
    {
        sounds[index]->play();
        index = (index + 1) % sounds.size();
    }
};

void addQuad(std::vector<sf::Vertex>& v,
             sf::Vector2f             center,
             sf::Vector2f             size,
             sf::IntRect              tex,
             bool                     flip,
             sf::Color                color)
{
    const float l  = center.x - size.x * 0.5f;
    const float t  = center.y - size.y * 0.5f;
    const float r  = l + size.x;
    const float b  = t + size.y;
    float       u0 = static_cast<float>(tex.position.x);
    float       u1 = u0 + static_cast<float>(tex.size.x);
    const float v0 = static_cast<float>(tex.position.y);
    const float v1 = v0 + static_cast<float>(tex.size.y);
    if (flip)
        std::swap(u0, u1);

    v.push_back(sf::Vertex{{l, t}, color, {u0, v0}});
    v.push_back(sf::Vertex{{r, t}, color, {u1, v0}});
    v.push_back(sf::Vertex{{l, b}, color, {u0, v1}});
    v.push_back(sf::Vertex{{l, b}, color, {u0, v1}});
    v.push_back(sf::Vertex{{r, t}, color, {u1, v0}});
    v.push_back(sf::Vertex{{r, b}, color, {u1, v1}});
}

void drawVerts(sf::RenderTarget& target, const std::vector<sf::Vertex>& v, const sf::Texture& tex)
{
    if (v.empty())
        return;
    sf::RenderStates states;
    states.texture = &tex;
    target.draw(v.data(), v.size(), sf::PrimitiveType::Triangles, states);
}

void drawText(sf::RenderTarget&  target,
              const sf::Font&    font,
              const std::string& str,
              unsigned int       size,
              sf::Vector2f       pos,
              float              ax,
              float              ay)
{
    sf::Text text(font, str, size);
    text.setFillColor(COL_TEXT);
    const sf::FloatRect b = text.getLocalBounds();
    text.setOrigin({std::round(b.position.x + b.size.x * ax), std::round(b.position.y + b.size.y * ay)});
    text.setPosition({std::round(pos.x), std::round(pos.y)});
    target.draw(text);
}

std::string formatTime(float t)
{
    int s = static_cast<int>(std::floor(std::clamp(t, 0.f, WIN_TIME)));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", s / 60, s % 60);
    return buf;
}

sf::Vector2f cameraCenterFor(sf::Vector2f p)
{
    return {std::clamp(p.x, VIEW_W * 0.5f, WORLD_W - VIEW_W * 0.5f),
            std::clamp(p.y, VIEW_H * 0.5f, WORLD_H - VIEW_H * 0.5f)};
}

enum KeyIdx
{
    K_W = 0,
    K_A,
    K_S,
    K_D,
    K_UP,
    K_LEFT,
    K_DOWN,
    K_RIGHT,
    K_COUNT
};

int keyIndex(sf::Keyboard::Key k)
{
    using K = sf::Keyboard::Key;
    switch (k)
    {
        case K::W: return K_W;
        case K::A: return K_A;
        case K::S: return K_S;
        case K::D: return K_D;
        case K::Up: return K_UP;
        case K::Left: return K_LEFT;
        case K::Down: return K_DOWN;
        case K::Right: return K_RIGHT;
        default: return -1;
    }
}

struct Pending
{
    bool                        enter = false;
    bool                        r     = false;
    bool                        p     = false;
    int                         card  = -1;
    std::optional<sf::Vector2f> click;
};

class Game
{
public:
    enum class State
    {
        Title,
        Play,
        Result
    };

    explicit Game(Assets& a) :
        assets(a),
        sfxShoot(a.shoot, 8),
        sfxHit(a.hit, 8),
        sfxPickup(a.pickup, 8),
        sfxLevelup(a.levelup, 4),
        rng(std::random_device{}())
    {
        assets.music.setLooping(true);
        assets.music.setVolume(40.f);
        resetPlay();
        state  = State::Title;
        titleT = 0.f;
    }

    void onKey(sf::Keyboard::Key code, bool pressed)
    {
        const int idx = keyIndex(code);
        if (idx >= 0)
        {
            held[static_cast<std::size_t>(idx)] = pressed;
            if (pressed)
                tapped[static_cast<std::size_t>(idx)] = true;
        }
        if (!pressed)
            return;
        using K = sf::Keyboard::Key;
        switch (code)
        {
            case K::Enter: pending.enter = true; break;
            case K::R: pending.r = true; break;
            case K::P: pending.p = true; break;
            case K::Num1:
            case K::Numpad1: pending.card = 0; break;
            case K::Num2:
            case K::Numpad2: pending.card = 1; break;
            case K::Num3:
            case K::Numpad3: pending.card = 2; break;
            default: break;
        }
    }

    void onClick(sf::Vector2f uiPos)
    {
        pending.click = uiPos;
    }

    void clearHeld()
    {
        held.fill(false);
    }

    void update(float dt)
    {
        const Pending in = pending;
        pending          = Pending{};
        std::array<bool, K_COUNT> dirKeys{};
        for (std::size_t i = 0; i < K_COUNT; ++i)
            dirKeys[i] = held[i] || tapped[i];
        tapped.fill(false);

        switch (state)
        {
            case State::Title:
                titleT += dt;
                if (in.enter || titleT >= 2.f)
                    startPlay();
                break;
            case State::Result:
                if (in.r)
                    startPlay();
                break;
            case State::Play: updatePlay(dt, in, dirKeys); break;
        }
    }

    void render(sf::RenderWindow& window, sf::View& gameView, const sf::View& uiView)
    {
        window.clear(sf::Color::Black);

        if (state == State::Title)
        {
            window.setView(uiView);
            drawUiBackground(window);
            drawText(window, assets.font, "LAST STAND", 48, {480.f, 220.f}, 0.5f, 0.5f);
            drawText(window, assets.font, "PRESS ENTER", 16, {480.f, 310.f}, 0.5f, 0.5f);
            return;
        }

        if (state == State::Result)
        {
            window.setView(uiView);
            drawUiBackground(window);
            drawText(window, assets.font, survived ? "SURVIVED" : "GAME OVER", 48, {480.f, 170.f}, 0.5f, 0.5f);
            drawText(window, assets.font, "TIME " + formatTime(playTime), 32, {480.f, 250.f}, 0.5f, 0.5f);
            drawText(window, assets.font, "LEVEL " + std::to_string(level), 32, {480.f, 295.f}, 0.5f, 0.5f);
            drawText(window, assets.font, "KILLS " + std::to_string(kills), 32, {480.f, 340.f}, 0.5f, 0.5f);
            drawText(window, assets.font, "R: RESTART  ESC: QUIT", 16, {480.f, 420.f}, 0.5f, 0.5f);
            return;
        }

        // ---- World ----
        sf::Vector2f cam = cameraCenterFor(ppos);
        cam              = {std::round(cam.x), std::round(cam.y)};
        gameView.setCenter(cam);
        window.setView(gameView);

        sf::RectangleShape bg({VIEW_W, VIEW_H});
        bg.setPosition(cam - sf::Vector2f(VIEW_W * 0.5f, VIEW_H * 0.5f));
        bg.setFillColor(COL_BG);
        window.draw(bg);

        window.draw(gridLines.data(), gridLines.size(), sf::PrimitiveType::Lines);

        sf::RectangleShape border({WORLD_W, WORLD_H});
        border.setPosition({0.f, 0.f});
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(COL_BORDER);
        border.setOutlineThickness(-2.f);
        window.draw(border);

        // gems
        vbuf.clear();
        for (const Gem& g : gems)
            addQuad(vbuf, g.pos, {12.f, 12.f}, sf::IntRect({0, 0}, {12, 12}), false, g.value >= 5 ? COL_GEM5 : COL_GEM1);
        drawVerts(window, vbuf, assets.gem);

        // enemies by type
        const std::array<const sf::Texture*, 3> etex{&assets.walker, &assets.runner, &assets.brute};
        for (int t = 0; t < 3; ++t)
        {
            vbuf.clear();
            const EnemyType& et        = ETYPES[static_cast<std::size_t>(t)];
            const int        animFrame = static_cast<int>(std::floor(playTime / et.animPeriod)) % 2;
            const float      cs        = static_cast<float>(et.cell);
            for (const Enemy& e : enemies)
            {
                if (e.type != t)
                    continue;
                const int  frame = e.flashT > 0.f ? 2 : animFrame;
                const bool flip  = ppos.x < e.pos.x;
                addQuad(vbuf, e.pos, {cs, cs}, sf::IntRect({frame * et.cell, 0}, {et.cell, et.cell}), flip, sf::Color::White);
            }
            drawVerts(window, vbuf, *etex[static_cast<std::size_t>(t)]);
        }

        // bullets
        vbuf.clear();
        for (const Bullet& b : bullets)
            addQuad(vbuf, b.pos, {8.f, 8.f}, sf::IntRect({0, 0}, {8, 8}), false, sf::Color::White);
        drawVerts(window, vbuf, assets.bullet);

        // player
        vbuf.clear();
        {
            const int frame = moving ? (static_cast<int>(std::floor(animT / 0.15f)) % 2) : 0;
            addQuad(vbuf, ppos, {32.f, 32.f}, sf::IntRect({frame * 32, 0}, {32, 32}), faceLeft, hurt ? COL_HURT : sf::Color::White);
        }
        drawVerts(window, vbuf, assets.player);

        // ---- HUD ----
        window.setView(uiView);
        drawHud(window);

        if (cardsActive)
        {
            drawDim(window);
            for (std::size_t i = 0; i < 3; ++i)
            {
                const sf::Vector2f pos = cardPos(static_cast<int>(i));
                sf::RectangleShape card({240.f, 140.f});
                card.setPosition(pos);
                card.setFillColor(COL_CARD_BG);
                card.setOutlineColor(COL_CARD_OUT);
                card.setOutlineThickness(-2.f);
                window.draw(card);
                drawText(window, assets.font, std::to_string(i + 1), 16, pos + sf::Vector2f(14.f, 14.f), 0.f, 0.f);
                drawText(window, assets.font, ABILITY_NAMES[static_cast<std::size_t>(cards[i])], 16, pos + sf::Vector2f(120.f, 70.f), 0.5f, 0.5f);
            }
        }
        else if (paused)
        {
            drawDim(window);
            drawText(window, assets.font, "PAUSED", 48, {480.f, 270.f}, 0.5f, 0.5f);
        }
    }

private:
    Assets& assets;
    SfxPool sfxShoot;
    SfxPool sfxHit;
    SfxPool sfxPickup;
    SfxPool sfxLevelup;

    std::mt19937 rng;

    State state    = State::Title;
    float titleT   = 0.f;
    float playTime = 0.f;
    bool  paused   = false;
    bool  survived = false;

    sf::Vector2f ppos;
    float        hp           = 100.f;
    float        maxHp        = 100.f;
    float        speed        = 200.f;
    float        fireInterval = 0.8f;
    float        fireT        = 0.8f;
    float        damage       = 10.f;
    int          projectiles  = 1;
    float        magnet       = 60.f;
    float        animT        = 0.f;
    bool         moving       = false;
    bool         faceLeft     = false;
    bool         hurt         = false;

    int  level         = 1;
    int  xp            = 0;
    int  kills         = 0;
    int  pendingLevels = 0;
    bool cardsActive   = false;
    std::array<int, 3> cards{0, 1, 2};

    float spawnT = 0.5f;

    std::vector<Enemy>  enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem>    gems;

    Pending                   pending;
    std::array<bool, K_COUNT> held{};
    std::array<bool, K_COUNT> tapped{};

    std::vector<sf::Vertex> vbuf;
    std::vector<sf::Vertex> gridLines = buildGrid();

    static std::vector<sf::Vertex> buildGrid()
    {
        std::vector<sf::Vertex> v;
        for (float x = 0.f; x <= WORLD_W; x += 120.f)
        {
            v.push_back(sf::Vertex{{x + 0.5f, 0.f}, COL_GRID, {}});
            v.push_back(sf::Vertex{{x + 0.5f, WORLD_H}, COL_GRID, {}});
        }
        for (float y = 0.f; y <= WORLD_H; y += 120.f)
        {
            v.push_back(sf::Vertex{{0.f, y + 0.5f}, COL_GRID, {}});
            v.push_back(sf::Vertex{{WORLD_W, y + 0.5f}, COL_GRID, {}});
        }
        return v;
    }

    static sf::Vector2f cardPos(int i)
    {
        return {80.f + 280.f * static_cast<float>(i), 200.f};
    }

    float rand01()
    {
        return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
    }

    void resetPlay()
    {
        playTime      = 0.f;
        paused        = false;
        survived      = false;
        ppos          = {WORLD_W * 0.5f, WORLD_H * 0.5f};
        hp            = 100.f;
        maxHp         = 100.f;
        speed         = 200.f;
        fireInterval  = 0.8f;
        fireT         = 0.8f;
        damage        = 10.f;
        projectiles   = 1;
        magnet        = 60.f;
        animT         = 0.f;
        moving        = false;
        faceLeft      = false;
        hurt          = false;
        level         = 1;
        xp            = 0;
        kills         = 0;
        pendingLevels = 0;
        cardsActive   = false;
        spawnT        = 0.5f;
        enemies.clear();
        bullets.clear();
        gems.clear();
        enemies.reserve(MAX_ENEMIES + 8);
        bullets.reserve(64);
        gems.reserve(512);
    }

    void startPlay()
    {
        resetPlay();
        state = State::Play;
        assets.music.stop();
        assets.music.play();
    }

    void endPlay(bool win)
    {
        survived = win;
        state    = State::Result;
        assets.music.stop();
    }

    void makeCards()
    {
        std::array<int, 6> idx{0, 1, 2, 3, 4, 5};
        std::shuffle(idx.begin(), idx.end(), rng);
        cards       = {idx[0], idx[1], idx[2]};
        cardsActive = true;
    }

    void applyAbility(int a)
    {
        switch (a)
        {
            case 0: fireInterval *= 0.85f; break;
            case 1: damage += 5.f; break;
            case 2: projectiles += 1; break;
            case 3: speed *= 1.1f; break;
            case 4:
                maxHp += 20.f;
                hp = std::min(maxHp, hp + 20.f);
                break;
            case 5: magnet += 40.f; break;
            default: break;
        }
    }

    void chooseCard(int i)
    {
        applyAbility(cards[static_cast<std::size_t>(i)]);
        --pendingLevels;
        if (pendingLevels > 0)
            makeCards();
        else
            cardsActive = false;
    }

    sf::Vector2f spawnPoint()
    {
        const sf::Vector2f cam = cameraCenterFor(ppos);
        const float        l   = cam.x - VIEW_W * 0.5f - 64.f;
        const float        r   = cam.x + VIEW_W * 0.5f + 64.f;
        const float        t   = cam.y - VIEW_H * 0.5f - 64.f;
        const float        b   = cam.y + VIEW_H * 0.5f + 64.f;

        struct Seg
        {
            sf::Vector2f a, b;
            float        len;
        };
        std::vector<Seg> segs;
        auto horiz = [&](float y)
        {
            if (y < 0.f || y > WORLD_H)
                return;
            const float x0 = std::max(l, 0.f);
            const float x1 = std::min(r, WORLD_W);
            if (x1 > x0)
                segs.push_back({{x0, y}, {x1, y}, x1 - x0});
        };
        auto vert = [&](float x)
        {
            if (x < 0.f || x > WORLD_W)
                return;
            const float y0 = std::max(t, 0.f);
            const float y1 = std::min(b, WORLD_H);
            if (y1 > y0)
                segs.push_back({{x, y0}, {x, y1}, y1 - y0});
        };
        horiz(t);
        horiz(b);
        vert(l);
        vert(r);

        if (segs.empty())
            return {rand01() * WORLD_W, rand01() * WORLD_H};

        float total = 0.f;
        for (const Seg& s : segs)
            total += s.len;
        float u = rand01() * total;
        for (const Seg& s : segs)
        {
            if (u <= s.len)
            {
                const float k = s.len > 0.f ? u / s.len : 0.f;
                return s.a + (s.b - s.a) * k;
            }
            u -= s.len;
        }
        return segs.back().b;
    }

    void spawnEnemy()
    {
        int         type = 0;
        const float u    = rand01();
        if (playTime < 60.f)
            type = 0;
        else if (playTime < 120.f)
            type = u < 0.7f ? 0 : 1;
        else
            type = u < 0.6f ? 0 : (u < 0.9f ? 1 : 2);

        Enemy e;
        e.type = type;
        e.pos  = spawnPoint();
        e.hp   = ETYPES[static_cast<std::size_t>(type)].hp;
        enemies.push_back(e);
    }

    void updatePlay(float dt, const Pending& in, const std::array<bool, K_COUNT>& keys)
    {
        // card selection
        if (cardsActive)
        {
            int choice = in.card;
            if (choice < 0 && in.click)
            {
                const sf::Vector2f c = *in.click;
                for (int i = 0; i < 3; ++i)
                {
                    const sf::FloatRect rect(cardPos(i), {240.f, 140.f});
                    if (rect.contains(c))
                    {
                        choice = i;
                        break;
                    }
                }
            }
            if (choice >= 0)
                chooseCard(choice);
            return;
        }

        if (in.p)
            paused = !paused;
        if (paused)
            return;

        playTime += dt;

        // ---- player movement ----
        sf::Vector2f dir{0.f, 0.f};
        if (keys[K_W] || keys[K_UP])
            dir.y -= 1.f;
        if (keys[K_S] || keys[K_DOWN])
            dir.y += 1.f;
        if (keys[K_A] || keys[K_LEFT])
            dir.x -= 1.f;
        if (keys[K_D] || keys[K_RIGHT])
            dir.x += 1.f;
        moving = dir.x != 0.f || dir.y != 0.f;
        if (moving)
        {
            if (dir.x < 0.f)
                faceLeft = true;
            else if (dir.x > 0.f)
                faceLeft = false;
            dir = dir.normalized();
            ppos += dir * speed * dt;
            ppos.x = std::clamp(ppos.x, PLAYER_RADIUS, WORLD_W - PLAYER_RADIUS);
            ppos.y = std::clamp(ppos.y, PLAYER_RADIUS, WORLD_H - PLAYER_RADIUS);
            animT += dt;
        }
        else
        {
            animT = 0.f;
        }

        // ---- spawning ----
        spawnT -= dt;
        if (spawnT <= 0.f)
        {
            if (static_cast<int>(enemies.size()) < MAX_ENEMIES)
                spawnEnemy();
            const float interval = std::max(0.15f, 0.6f - 0.05f * std::floor(playTime / 15.f));
            spawnT += interval;
            if (spawnT < 0.f)
                spawnT = interval;
        }

        // ---- auto fire ----
        fireT -= dt;
        if (fireT <= 0.f)
        {
            const Enemy* target = nullptr;
            float        best   = 400.f * 400.f;
            for (const Enemy& e : enemies)
            {
                if (e.dying)
                    continue;
                const float d2 = (e.pos - ppos).lengthSquared();
                if (d2 <= best)
                {
                    best   = d2;
                    target = &e;
                }
            }
            if (target)
            {
                sf::Vector2f d = target->pos - ppos;
                if (d.lengthSquared() < 1e-6f)
                    d = {1.f, 0.f};
                const sf::Angle base = d.angle();
                for (int i = 0; i < projectiles; ++i)
                {
                    const float     offs = (static_cast<float>(i) - static_cast<float>(projectiles - 1) * 0.5f) * 10.f;
                    const sf::Angle a    = base + sf::degrees(offs);
                    Bullet          b;
                    b.pos    = ppos;
                    b.vel    = sf::Vector2f(500.f, a);
                    b.life   = 1.2f;
                    b.damage = damage;
                    bullets.push_back(b);
                }
                sfxShoot.play();
                fireT += fireInterval;
                if (fireT <= 0.f)
                    fireT = fireInterval;
            }
            else
            {
                fireT = 0.f;
            }
        }

        // ---- bullets ----
        for (Bullet& b : bullets)
        {
            b.pos += b.vel * dt;
            b.life -= dt;
            if (b.life <= 0.f)
            {
                b.dead = true;
                continue;
            }
            for (Enemy& e : enemies)
            {
                if (e.dying)
                    continue;
                const float rr = BULLET_RADIUS + ETYPES[static_cast<std::size_t>(e.type)].radius;
                if ((e.pos - b.pos).lengthSquared() < rr * rr)
                {
                    e.hp -= b.damage;
                    e.flashT = 0.1f;
                    if (e.hp <= 0.f)
                    {
                        e.dying = true;
                        e.dieT  = 0.1f;
                        Gem g;
                        g.pos   = e.pos;
                        g.value = ETYPES[static_cast<std::size_t>(e.type)].gemValue;
                        g.life  = 30.f;
                        gems.push_back(g);
                        sfxHit.play();
                        ++kills;
                    }
                    else
                    {
                        e.knockT   = 0.1f;
                        e.knockDir = b.vel.normalized();
                    }
                    b.dead = true;
                    break;
                }
            }
        }
        std::erase_if(bullets, [](const Bullet& b) { return b.dead; });

        // ---- enemies ----
        for (Enemy& e : enemies)
        {
            if (e.flashT > 0.f)
                e.flashT -= dt;
            if (e.dying)
            {
                e.dieT -= dt;
                continue;
            }
            const EnemyType& et = ETYPES[static_cast<std::size_t>(e.type)];
            if (e.knockT > 0.f)
            {
                const float s = std::min(dt, e.knockT);
                e.pos += e.knockDir * 120.f * s;
                e.knockT -= dt;
                e.pos.x = std::clamp(e.pos.x, 0.f, WORLD_W);
                e.pos.y = std::clamp(e.pos.y, 0.f, WORLD_H);
            }
            else
            {
                const sf::Vector2f d = ppos - e.pos;
                const float        L = d.length();
                if (L > 1e-4f)
                    e.pos += d / L * std::min(et.speed * dt, L);
            }
        }
        std::erase_if(enemies, [](const Enemy& e) { return e.dying && e.dieT <= 1e-6f; });

        // ---- contact damage ----
        hurt = false;
        for (const Enemy& e : enemies)
        {
            if (e.dying)
                continue;
            const EnemyType& et = ETYPES[static_cast<std::size_t>(e.type)];
            const float      rr = PLAYER_RADIUS + et.radius;
            if ((e.pos - ppos).lengthSquared() < rr * rr)
            {
                hp -= et.dps * dt;
                hurt = true;
            }
        }

        // ---- gems ----
        for (Gem& g : gems)
        {
            g.life -= dt;
            if (g.life <= 0.f)
            {
                g.dead = true;
                continue;
            }
            sf::Vector2f d = ppos - g.pos;
            float        L = d.length();
            if (L <= magnet)
            {
                const float step = 300.f * dt;
                if (step >= L)
                    g.pos = ppos;
                else
                    g.pos += d / L * step;
                d = ppos - g.pos;
                L = d.length();
            }
            if (L < PLAYER_RADIUS + GEM_RADIUS)
            {
                g.dead = true;
                sfxPickup.play();
                xp += g.value;
                while (xp >= 5 * level)
                {
                    xp -= 5 * level;
                    ++level;
                    ++pendingLevels;
                    sfxLevelup.play();
                }
            }
        }
        std::erase_if(gems, [](const Gem& g) { return g.dead; });

        // ---- end conditions ----
        if (hp <= 0.f)
        {
            hp = 0.f;
            endPlay(false);
            return;
        }
        if (playTime >= WIN_TIME)
        {
            playTime = WIN_TIME;
            endPlay(true);
            return;
        }

        if (pendingLevels > 0 && !cardsActive)
            makeCards();
    }

    void drawUiBackground(sf::RenderTarget& target)
    {
        sf::RectangleShape bg({VIEW_W, VIEW_H});
        bg.setFillColor(COL_BG);
        target.draw(bg);
    }

    void drawDim(sf::RenderTarget& target)
    {
        sf::RectangleShape dim({VIEW_W, VIEW_H});
        dim.setFillColor(sf::Color(0, 0, 0, 140));
        target.draw(dim);
    }

    void drawHud(sf::RenderTarget& target)
    {
        // XP bar
        sf::RectangleShape xpBg({VIEW_W, 6.f});
        xpBg.setFillColor(COL_XP_BG);
        target.draw(xpBg);
        const float ratio = std::clamp(static_cast<float>(xp) / static_cast<float>(5 * level), 0.f, 1.f);
        if (ratio > 0.f)
        {
            sf::RectangleShape xpFill({VIEW_W * ratio, 6.f});
            xpFill.setFillColor(COL_XP_FILL);
            target.draw(xpFill);
        }

        // HP bar
        sf::RectangleShape hpBg({200.f, 12.f});
        hpBg.setPosition({10.f, 16.f});
        hpBg.setFillColor(COL_HP_BG);
        target.draw(hpBg);
        const float hpRatio = std::clamp(hp / maxHp, 0.f, 1.f);
        if (hpRatio > 0.f)
        {
            sf::RectangleShape hpFill({200.f * hpRatio, 12.f});
            hpFill.setPosition({10.f, 16.f});
            hpFill.setFillColor(COL_HP_FILL);
            target.draw(hpFill);
        }
        const int hpShown = static_cast<int>(std::ceil(std::max(hp, 0.f) - 1e-4f));
        drawText(target, assets.font, "HP " + std::to_string(std::max(hpShown, 0)), 16, {220.f, 22.f}, 0.f, 0.5f);

        // right side
        drawText(target, assets.font, "LV " + std::to_string(level), 16, {950.f, 14.f}, 1.f, 0.f);
        drawText(target, assets.font, "TIME " + formatTime(playTime), 16, {950.f, 36.f}, 1.f, 0.f);
        drawText(target, assets.font, "KILLS " + std::to_string(kills), 16, {950.f, 58.f}, 1.f, 0.f);
    }
};

sf::FloatRect letterboxViewport(sf::Vector2u size)
{
    const float w = static_cast<float>(std::max(1u, size.x));
    const float h = static_cast<float>(std::max(1u, size.y));
    const float target = VIEW_W / VIEW_H;
    if (w / h > target)
    {
        const float vw = (h * target) / w;
        return sf::FloatRect({(1.f - vw) * 0.5f, 0.f}, {vw, 1.f});
    }
    const float vh = (w / target) / h;
    return sf::FloatRect({0.f, (1.f - vh) * 0.5f}, {1.f, vh});
}

} // namespace

int main()
{
    Assets assets;
    if (!loadAssets(assets))
        return 1;

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "LAST STAND");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    sf::View gameView(sf::FloatRect({0.f, 0.f}, {VIEW_W, VIEW_H}));
    sf::View uiView(sf::FloatRect({0.f, 0.f}, {VIEW_W, VIEW_H}));
    {
        const sf::FloatRect vp = letterboxViewport(window.getSize());
        gameView.setViewport(vp);
        uiView.setViewport(vp);
    }

    Game game(assets);

    sf::Clock clock;
    float     accumulator = 0.f;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            else if (const auto* kp = event->getIf<sf::Event::KeyPressed>())
            {
                if (kp->code == sf::Keyboard::Key::Escape)
                    window.close();
                else
                    game.onKey(kp->code, true);
            }
            else if (const auto* kr = event->getIf<sf::Event::KeyReleased>())
            {
                game.onKey(kr->code, false);
            }
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                    game.onClick(window.mapPixelToCoords(mb->position, uiView));
            }
            else if (const auto* rs = event->getIf<sf::Event::Resized>())
            {
                const sf::FloatRect vp = letterboxViewport(rs->size);
                gameView.setViewport(vp);
                uiView.setViewport(vp);
            }
            else if (event->is<sf::Event::FocusLost>())
            {
                game.clearHeld();
            }
        }
        if (!window.isOpen())
            break;

        float frame = clock.restart().asSeconds();
        frame       = std::min(frame, 0.25f);
        accumulator += frame;
        int steps = 0;
        while (accumulator >= DT && steps < 8)
        {
            game.update(DT);
            accumulator -= DT;
            ++steps;
        }
        if (steps >= 8)
            accumulator = std::min(accumulator, DT);

        game.render(window, gameView, uiView);
        window.display();
    }

    return 0;
}
```