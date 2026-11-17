#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <format>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr float kStep = 1.f / 60.f;
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerMargin = 16.f;
constexpr float kPlayerR = 14.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletLife = 1.4f;
constexpr float kBulletR = 4.f;
constexpr float kFireInterval = 0.15f;
constexpr float kEnemySpeed = 100.f;
constexpr float kEnemyR = 14.f;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kSpawnMargin = 64.f;
constexpr float kInvulnTime = 1.f;
constexpr float kTitleTime = 2.f;
constexpr float kFirstSpawnTime = 0.5f;

constexpr const char* kFontPath = "assets/fonts/game.ttf";
constexpr const char* kPlayerPath = "assets/textures/player.png";
constexpr const char* kEnemyPath = "assets/textures/enemy.png";
constexpr const char* kBulletPath = "assets/textures/bullet.png";
constexpr const char* kShootPath = "assets/sfx/shoot.wav";
constexpr const char* kHitPath = "assets/sfx/hit.wav";
constexpr const char* kLosePath = "assets/sfx/lose.wav";
constexpr const char* kBgmPath = "assets/music/bgm_shooter.ogg";

float vlen(sf::Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float dist2(sf::Vector2f a, sf::Vector2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool check(bool ok, const char* path) {
    if (!ok)
        std::cerr << "failed to load: " << path << '\n';
    return ok;
}

template <class T>
void compactRemove(std::vector<T>& v, const std::vector<bool>& dead) {
    std::size_t w = 0;
    for (std::size_t r = 0; r < v.size(); ++r) {
        if (!dead[r]) {
            if (w != r)
                v[w] = v[r];
            ++w;
        }
    }
    v.erase(v.begin() + static_cast<std::ptrdiff_t>(w), v.end());
}

struct SoundBank {
    sf::SoundBuffer buffer;
    std::vector<std::unique_ptr<sf::Sound>> voices;

    bool load(const char* path, std::size_t voiceCount) {
        if (!check(buffer.loadFromFile(path), path))
            return false;
        for (std::size_t i = 0; i < voiceCount; ++i)
            voices.push_back(std::make_unique<sf::Sound>(buffer));
        return true;
    }

    void play() {
        for (auto& v : voices) {
            if (v->getStatus() != sf::SoundSource::Status::Playing) {
                v->play();
                return;
            }
        }
        if (!voices.empty())
            voices.front()->play();
    }
};

struct Assets {
    sf::Font font;
    sf::Texture playerTex;
    sf::Texture enemyTex;
    sf::Texture bulletTex;
    SoundBank shoot;
    SoundBank hit;
    SoundBank lose;
    sf::Music bgm;

    bool load() {
        if (!check(font.openFromFile(kFontPath), kFontPath))
            return false;
        if (!check(playerTex.loadFromFile(kPlayerPath), kPlayerPath))
            return false;
        if (!check(enemyTex.loadFromFile(kEnemyPath), kEnemyPath))
            return false;
        if (!check(bulletTex.loadFromFile(kBulletPath), kBulletPath))
            return false;
        if (!shoot.load(kShootPath, 8))
            return false;
        if (!hit.load(kHitPath, 8))
            return false;
        if (!lose.load(kLosePath, 4))
            return false;
        if (!check(bgm.openFromFile(kBgmPath), kBgmPath))
            return false;
        bgm.setLooping(true);
        bgm.setVolume(40.f);
        return true;
    }
};

struct Art {
    sf::Sprite player;
    sf::Sprite enemy;
    sf::Sprite bullet;
    sf::VertexArray grid;
    sf::RectangleShape bg;
    sf::RectangleShape border;

    Art(const sf::Texture& playerTex, const sf::Texture& enemyTex, const sf::Texture& bulletTex)
        : player(playerTex),
          enemy(enemyTex),
          bullet(bulletTex),
          grid(sf::PrimitiveType::Lines),
          bg(sf::Vector2f(kWorldW, kWorldH)),
          border(sf::Vector2f(kWorldW, kWorldH)) {
        player.setOrigin(sf::Vector2f(16.f, 16.f));
        enemy.setOrigin(sf::Vector2f(16.f, 16.f));
        bullet.setOrigin(sf::Vector2f(4.f, 4.f));

        bg.setFillColor(sf::Color(0x14, 0x18, 0x1F));

        border.setFillColor(sf::Color::Transparent);
        border.setOutlineColor(sf::Color(0x5C, 0x6B, 0x80));
        border.setOutlineThickness(-2.f); // 안쪽으로 그려 월드 밖으로 나가지 않게 함

        const sf::Color gridColor(0x22, 0x2A, 0x35);
        for (int i = 0; i <= 16; ++i) {
            const float x = static_cast<float>(i) * 120.f;
            grid.append(sf::Vertex{sf::Vector2f(x, 0.f), gridColor});
            grid.append(sf::Vertex{sf::Vector2f(x, kWorldH), gridColor});
        }
        for (int j = 0; j <= 9; ++j) {
            const float y = static_cast<float>(j) * 120.f;
            grid.append(sf::Vertex{sf::Vector2f(0.f, y), gridColor});
            grid.append(sf::Vertex{sf::Vector2f(kWorldW, y), gridColor});
        }
    }
};

enum class Screen { Title, Playing, Result };

struct Bullet {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float life;
};

struct Enemy {
    sf::Vector2f pos;
};

struct Game {
    Screen screen = Screen::Title;
    float titleTimer = 0.f;
    bool paused = false;
    sf::Vector2f player{kWorldW / 2.f, kWorldH / 2.f};
    sf::Vector2f camera{kWorldW / 2.f, kWorldH / 2.f};
    sf::Vector2f aim{1.f, 0.f};
    int hp = 3;
    int score = 0;
    float playTime = 0.f;
    float fireCooldown = 0.f;
    float invuln = 0.f;
    float spawnTimer = kFirstSpawnTime;
    bool firePending = false;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
};

struct Input {
    float mx = 0.f;
    float my = 0.f;
    bool fireHeld = false;
};

void startGame(Game& g, Assets& a) {
    g = Game{};
    g.screen = Screen::Playing;
    a.bgm.stop();
    a.bgm.play();
}

sf::Vector2f spawnPoint(const Game& g, std::mt19937& rng) {
    const float x0 = g.camera.x - kViewW / 2.f - kSpawnMargin;
    const float y0 = g.camera.y - kViewH / 2.f - kSpawnMargin;
    const float w = kViewW + 2.f * kSpawnMargin;
    const float h = kViewH + 2.f * kSpawnMargin;

    std::uniform_real_distribution<float> dist(0.f, 2.f * (w + h));
    const float t = dist(rng);

    float x = x0;
    float y = y0;
    if (t < w) {
        x = x0 + t;
        y = y0;
    } else if (t < w + h) {
        x = x0 + w;
        y = y0 + (t - w);
    } else if (t < 2.f * w + h) {
        x = x0 + w - (t - w - h);
        y = y0 + h;
    } else {
        x = x0;
        y = y0 + h - (t - 2.f * w - h);
    }
    return sf::Vector2f(std::clamp(x, 0.f, kWorldW), std::clamp(y, 0.f, kWorldH));
}

void step(Game& g, const Input& in, Assets& a, std::mt19937& rng) {
    if (g.screen == Screen::Title) {
        g.titleTimer += kStep;
        if (g.titleTimer >= kTitleTime)
            startGame(g, a);
        return;
    }
    if (g.screen != Screen::Playing || g.paused)
        return;

    g.playTime += kStep;
    g.fireCooldown = std::max(0.f, g.fireCooldown - kStep);
    g.invuln = std::max(0.f, g.invuln - kStep);

    // 플레이어 이동 (대각선도 같은 속력)
    float dx = in.mx;
    float dy = in.my;
    const float dlen = std::sqrt(dx * dx + dy * dy);
    if (dlen > 0.f) {
        dx /= dlen;
        dy /= dlen;
    }
    g.player.x = std::clamp(g.player.x + dx * kPlayerSpeed * kStep,
                            kPlayerMargin, kWorldW - kPlayerMargin);
    g.player.y = std::clamp(g.player.y + dy * kPlayerSpeed * kStep,
                            kPlayerMargin, kWorldH - kPlayerMargin);

    // 사격
    if ((in.fireHeld || g.firePending) && g.fireCooldown <= 0.0001f) {
        g.bullets.push_back(Bullet{g.player, g.aim * kBulletSpeed, kBulletLife});
        g.fireCooldown = kFireInterval;
        g.firePending = false;
        a.shoot.play();
    }

    // 총알 이동과 삭제
    for (auto& b : g.bullets) {
        b.pos = b.pos + b.vel * kStep;
        b.life -= kStep;
    }
    std::erase_if(g.bullets, [](const Bullet& b) {
        return b.life <= 0.f || b.pos.x < 0.f || b.pos.x > kWorldW ||
               b.pos.y < 0.f || b.pos.y > kWorldH;
    });

    // 적 생성
    g.spawnTimer -= kStep;
    if (g.spawnTimer <= 0.f) {
        if (g.enemies.size() < kMaxEnemies)
            g.enemies.push_back(Enemy{spawnPoint(g, rng)});
        const float interval = std::max(0.3f, 1.f - 0.1f * std::floor(g.playTime / 10.f));
        g.spawnTimer += interval;
    }

    // 적 이동 (플레이어를 향해)
    for (auto& e : g.enemies) {
        const sf::Vector2f d = g.player - e.pos;
        const float l = vlen(d);
        if (l > 0.f)
            e.pos = e.pos + d * (kEnemySpeed * kStep / l);
    }

    // 총알과 적 충돌
    {
        std::vector<bool> bDead(g.bullets.size(), false);
        std::vector<bool> eDead(g.enemies.size(), false);
        const float r = kBulletR + kEnemyR;
        for (std::size_t i = 0; i < g.bullets.size(); ++i) {
            for (std::size_t j = 0; j < g.enemies.size(); ++j) {
                if (eDead[j])
                    continue;
                if (dist2(g.bullets[i].pos, g.enemies[j].pos) < r * r) {
                    bDead[i] = true;
                    eDead[j] = true;
                    g.score += 10;
                    a.hit.play();
                    break;
                }
            }
        }
        compactRemove(g.bullets, bDead);
        compactRemove(g.enemies, eDead);
    }

    // 적과 플레이어 충돌 (무적 중에는 무시)
    if (g.invuln <= 0.f) {
        const float r = kPlayerR + kEnemyR;
        for (std::size_t j = 0; j < g.enemies.size(); ++j) {
            if (dist2(g.player, g.enemies[j].pos) < r * r) {
                g.enemies.erase(g.enemies.begin() + static_cast<std::ptrdiff_t>(j));
                --g.hp;
                g.invuln = kInvulnTime;
                a.lose.play();
                break;
            }
        }
    }

    // 카메라: 플레이어를 가운데에 두되 월드 밖이 보이지 않게 제한
    g.camera.x = std::clamp(g.player.x, kViewW / 2.f, kWorldW - kViewW / 2.f);
    g.camera.y = std::clamp(g.player.y, kViewH / 2.f, kWorldH - kViewH / 2.f);

    if (g.hp <= 0) {
        g.screen = Screen::Result;
        a.bgm.pause();
    }
}

void drawWorld(sf::RenderTarget& t, const Game& g, Art& art, sf::Vector2f mouseWorld) {
    t.draw(art.bg);
    t.draw(art.grid);
    t.draw(art.border);

    for (const auto& b : g.bullets) {
        art.bullet.setPosition(b.pos);
        t.draw(art.bullet);
    }
    for (const auto& e : g.enemies) {
        art.enemy.setPosition(e.pos);
        t.draw(art.enemy);
    }

    const sf::Vector2f d = mouseWorld - g.player;
    const sf::Vector2f face = vlen(d) > 1e-3f ? d : g.aim;
    art.player.setPosition(g.player);
    art.player.setRotation(sf::radians(std::atan2(face.y, face.x)));

    const bool blink = g.invuln > 0.f && (static_cast<int>(g.invuln * 10.f) % 2 == 0);
    art.player.setColor(blink ? sf::Color(255, 255, 255, 90) : sf::Color::White);
    t.draw(art.player);
}

void drawText(sf::RenderTarget& t, const sf::Font& font, std::string_view s,
              unsigned int size, sf::Vector2f pos, bool center) {
    sf::Text text(font, sf::String(s), size);
    text.setFillColor(sf::Color::White);
    if (center) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin(sf::Vector2f(b.position.x + b.size.x / 2.f,
                                    b.position.y + b.size.y / 2.f));
    }
    text.setPosition(pos);
    t.draw(text);
}

void drawOverlay(sf::RenderTarget& t, const Game& g, const sf::Font& font) {
    switch (g.screen) {
        case Screen::Title:
            drawText(t, font, "ARENA", 48, sf::Vector2f(480.f, 200.f), true);
            drawText(t, font, "PRESS ENTER", 32, sf::Vector2f(480.f, 300.f), true);
            break;
        case Screen::Playing: {
            const std::string hud =
                std::format("HP {}  SCORE {}  TIME {:.1f}", g.hp, g.score, g.playTime);
            drawText(t, font, hud, 16, sf::Vector2f(12.f, 8.f), false);
            if (g.paused)
                drawText(t, font, "PAUSED", 32, sf::Vector2f(480.f, 270.f), true);
            break;
        }
        case Screen::Result: {
            const std::string score = std::format("SCORE {}", g.score);
            const std::string time = std::format("TIME {:.1f}", g.playTime);
            drawText(t, font, "GAME OVER", 48, sf::Vector2f(480.f, 200.f), true);
            drawText(t, font, score, 32, sf::Vector2f(480.f, 280.f), true);
            drawText(t, font, time, 32, sf::Vector2f(480.f, 340.f), true);
            drawText(t, font, "R: RESTART  ESC: QUIT", 16, sf::Vector2f(480.f, 420.f), true);
            break;
        }
    }
}

// 창 크기가 바뀌어도 16:9를 유지하고 남는 부분은 검은 띠로 둔다.
sf::FloatRect letterbox(sf::Vector2u ws) {
    const float sw = static_cast<float>(ws.x);
    const float sh = static_cast<float>(ws.y);
    if (sw <= 0.f || sh <= 0.f)
        return sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(1.f, 1.f));
    const float s = std::min(sw / kViewW, sh / kViewH);
    const float vw = kViewW * s;
    const float vh = kViewH * s;
    const float ox = (sw - vw) / 2.f;
    const float oy = (sh - vh) / 2.f;
    return sf::FloatRect(sf::Vector2f(ox / sw, oy / sh), sf::Vector2f(vw / sw, vh / sh));
}

Input makeInput(const std::set<sf::Keyboard::Scancode>& held,
                const std::set<sf::Keyboard::Scancode>* pending) {
    using K = sf::Keyboard::Scancode;
    auto on = [&](std::initializer_list<K> keys) {
        for (K k : keys) {
            if (held.contains(k) || (pending && pending->contains(k)))
                return true;
        }
        return false;
    };

    Input in;
    in.mx = (on({K::D, K::Right}) ? 1.f : 0.f) - (on({K::A, K::Left}) ? 1.f : 0.f);
    in.my = (on({K::S, K::Down}) ? 1.f : 0.f) - (on({K::W, K::Up}) ? 1.f : 0.f);
    in.fireHeld = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    return in;
}

int runGame() {
    Assets assets;
    if (!assets.load())
        return 1;

    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(960u, 540u)), "ARENA");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    Art art(assets.playerTex, assets.enemyTex, assets.bulletTex);

    sf::View gameView(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(kViewW, kViewH)));
    sf::View hudView(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(kViewW, kViewH)));

    using K = sf::Keyboard::Scancode;
    Game g;
    std::mt19937 rng(std::random_device{}());
    std::set<K> held;
    std::set<K> pendingMove;
    std::set<K> pendingAction;

    sf::Clock clock;
    float acc = 0.f;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                if (kp->scancode == K::Escape)
                    window.close();
                held.insert(kp->scancode);
                pendingMove.insert(kp->scancode);
                pendingAction.insert(kp->scancode);
            } else if (const auto* kr = event->getIf<sf::Event::KeyReleased>()) {
                held.erase(kr->scancode);
            } else if (event->is<sf::Event::FocusLost>()) {
                held.clear();
            } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left)
                    g.firePending = true;
            }
        }
        if (!window.isOpen())
            break;

        const float frameDt = std::min(clock.restart().asSeconds(), 0.25f);
        acc += frameDt;

        // 뷰 갱신 (창 크기 대응) 및 조준 방향 계산
        const sf::FloatRect vp = letterbox(window.getSize());
        gameView.setViewport(vp);
        hudView.setViewport(vp);
        gameView.setCenter(g.camera);

        const sf::Vector2i mousePix = sf::Mouse::getPosition(window);
        {
            const sf::Vector2f mw = window.mapPixelToCoords(mousePix, gameView);
            const sf::Vector2f d = mw - g.player;
            const float len = vlen(d);
            if (len > 1e-3f)
                g.aim = d / len;
        }

        // 화면 전환 등 즉시 처리하는 동작
        if (pendingAction.contains(K::Enter) && g.screen == Screen::Title)
            startGame(g, assets);
        if (pendingAction.contains(K::P) && g.screen == Screen::Playing) {
            g.paused = !g.paused;
            if (g.paused)
                assets.bgm.pause();
            else
                assets.bgm.play();
        }
        if (pendingAction.contains(K::R) && g.screen == Screen::Result)
            startGame(g, assets);
        pendingAction.clear();

        // 고정 시간 간격 갱신 (프레임레이트와 무관하게 같은 속도)
        bool ran = false;
        while (acc >= kStep) {
            const Input in = makeInput(held, ran ? nullptr : &pendingMove);
            step(g, in, assets, rng);
            acc -= kStep;
            ran = true;
        }
        if (ran)
            pendingMove.clear();

        gameView.setCenter(g.camera);

        // 렌더링
        window.clear(sf::Color::Black);
        const sf::Vector2f mouseWorld = window.mapPixelToCoords(mousePix, gameView);
        window.setView(gameView);
        drawWorld(window, g, art, mouseWorld);
        window.setView(hudView);
        drawOverlay(window, g, assets.font);
        window.display();
    }

    return 0;
}

} // namespace

int main() {
    try {
        return runGame();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
