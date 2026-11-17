```cpp
// main.cpp - ARENA (탑다운 슈터), SFML 3.1
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

// ---- 상수 ----
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kStep = 1.f / 60.f; // 고정 시간 간격(60 FPS)
constexpr float kGridStep = 120.f;

constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerRadius = 14.f;
constexpr float kPlayerMargin = 16.f;
constexpr int kStartHp = 3;
constexpr float kInvincibleTime = 1.f;

constexpr float kBulletSpeed = 600.f;
constexpr float kBulletRadius = 4.f;
constexpr float kBulletLife = 1.4f;
constexpr float kFireInterval = 0.15f;

constexpr float kEnemyRadius = 14.f;
constexpr float kEnemySpeed = 100.f;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kFirstSpawnDelay = 0.5f;
constexpr float kSpawnMargin = 64.f;

constexpr float kTitleDuration = 2.f;
constexpr float kScorePerHit = 10;

constexpr const char* kFontPath = "assets/fonts/game.ttf";
constexpr const char* kPlayerPath = "assets/textures/player.png";
constexpr const char* kEnemyPath = "assets/textures/enemy.png";
constexpr const char* kBulletPath = "assets/textures/bullet.png";
constexpr const char* kShootPath = "assets/sfx/shoot.wav";
constexpr const char* kHitPath = "assets/sfx/hit.wav";
constexpr const char* kLosePath = "assets/sfx/lose.wav";
constexpr const char* kBgmPath = "assets/music/bgm_shooter.ogg";

const sf::Color kBgColor{20, 24, 31};      // #14181F
const sf::Color kGridColor{34, 42, 53};    // #222A35
const sf::Color kBorderColor{92, 107, 128}; // #5C6B80
const sf::Color kTextColor{255, 255, 255};  // #FFFFFF

// ---- 타입 ----
enum class Scene { Title, Play, Result };

struct Input {
    bool enter = false;
    bool pause = false;
    bool restart = false;
    bool shootTap = false; // 짧게 눌렀다 뗀 클릭도 놓치지 않도록 발사 요청을 유지
};

struct Bullet {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float life = 0.f;
    bool dead = false;
};

struct Enemy {
    sf::Vector2f pos;
    bool dead = false;
};

struct Resources {
    sf::Font font;
    sf::Texture playerTex;
    sf::Texture enemyTex;
    sf::Texture bulletTex;
    sf::SoundBuffer shootBuf;
    sf::SoundBuffer hitBuf;
    sf::SoundBuffer loseBuf;
    std::optional<sf::Sound> shootSnd;
    std::optional<sf::Sound> hitSnd;
    std::optional<sf::Sound> loseSnd;
    sf::Music bgm;
};

struct Game {
    Resources res;
    sf::View gameView{sf::FloatRect({0.f, 0.f}, {kViewW, kViewH})};
    sf::View uiView{sf::FloatRect({0.f, 0.f}, {kViewW, kViewH})};
    std::mt19937 rng{std::random_device{}()};

    Scene scene = Scene::Title;
    float titleTime = 0.f;
    bool paused = false;
    float playTime = 0.f;
    int hp = kStartHp;
    int score = 0;

    sf::Vector2f player{kWorldW / 2.f, kWorldH / 2.f};
    sf::Vector2f camera{kWorldW / 2.f, kWorldH / 2.f};
    sf::Vector2f aim{kWorldW / 2.f, kWorldH / 2.f};
    sf::Vector2f aimDir{1.f, 0.f};
    sf::Angle playerRot = sf::degrees(0.f);
    float invincible = 0.f;
    float fireCooldown = 0.f;
    float spawnCountdown = kFirstSpawnDelay;

    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
};

// ---- 유틸 ----
float vlen(sf::Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

float distSq(sf::Vector2f a, sf::Vector2f b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool insideWorld(sf::Vector2f p) {
    return p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
}

int reportMissing(const char* path) {
    std::cerr << "failed to load asset: " << path << '\n';
    return 1;
}

void drawText(sf::RenderTarget& target, const Resources& res, const std::string& str,
              unsigned int size, sf::Vector2f pos, sf::Color color, bool centered) {
    sf::Text text(res.font, sf::String(str), size);
    text.setFillColor(color);
    if (centered) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin(sf::Vector2f{b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    }
    text.setPosition(pos);
    target.draw(text);
}

// ---- 뷰 배치 (16:9 레터박스) ----
void layoutViews(const sf::RenderWindow& window, Game& g) {
    const sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0) {
        return;
    }
    const float scale = std::min(static_cast<float>(size.x) / kViewW,
                                 static_cast<float>(size.y) / kViewH);
    const float vw = kViewW * scale / static_cast<float>(size.x);
    const float vh = kViewH * scale / static_cast<float>(size.y);
    const sf::FloatRect viewport(sf::Vector2f{(1.f - vw) / 2.f, (1.f - vh) / 2.f},
                                 sf::Vector2f{vw, vh});
    g.gameView.setViewport(viewport);
    g.uiView.setViewport(viewport);
    g.uiView.setCenter(sf::Vector2f{kViewW / 2.f, kViewH / 2.f});
}

// ---- 입력 수집 (이벤트 기반: 짧은 입력도 누락되지 않음) ----
void pollInput(sf::RenderWindow& window, Input& in) {
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Escape) {
                window.close();
            } else if (key->code == sf::Keyboard::Key::Enter) {
                in.enter = true;
            } else if (key->code == sf::Keyboard::Key::P) {
                in.pause = true;
            } else if (key->code == sf::Keyboard::Key::R) {
                in.restart = true;
            }
        } else if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (click->button == sf::Mouse::Button::Left) {
                in.shootTap = true;
            }
        }
    }
}

// ---- 상태 전이 ----
void startPlay(Game& g) {
    g.scene = Scene::Play;
    g.paused = false;
    g.playTime = 0.f;
    g.hp = kStartHp;
    g.score = 0;
    g.player = sf::Vector2f{kWorldW / 2.f, kWorldH / 2.f};
    g.camera = sf::Vector2f{kWorldW / 2.f, kWorldH / 2.f};
    g.gameView.setCenter(g.camera);
    g.aim = g.camera;
    g.aimDir = sf::Vector2f{1.f, 0.f};
    g.playerRot = sf::degrees(0.f);
    g.invincible = 0.f;
    g.fireCooldown = 0.f;
    g.spawnCountdown = kFirstSpawnDelay;
    g.bullets.clear();
    g.enemies.clear();
    g.res.bgm.stop();
    g.res.bgm.play();
}

void togglePause(Game& g) {
    g.paused = !g.paused;
    if (g.paused) {
        g.res.bgm.pause();
    } else {
        g.res.bgm.play();
    }
}

// 카메라 시야를 사방 64px 넓힌 사각형 테두리 위, 월드 안의 무작위 점
sf::Vector2f spawnPoint(Game& g) {
    const float left = g.camera.x - kViewW / 2.f - kSpawnMargin;
    const float right = g.camera.x + kViewW / 2.f + kSpawnMargin;
    const float top = g.camera.y - kViewH / 2.f - kSpawnMargin;
    const float bottom = g.camera.y + kViewH / 2.f + kSpawnMargin;

    std::uniform_int_distribution<int> sideDist(0, 3);
    std::uniform_real_distribution<float> xDist(left, right);
    std::uniform_real_distribution<float> yDist(top, bottom);

    for (int attempt = 0; attempt < 32; ++attempt) {
        sf::Vector2f p;
        switch (sideDist(g.rng)) {
            case 0:  p = sf::Vector2f{xDist(g.rng), top}; break;
            case 1:  p = sf::Vector2f{xDist(g.rng), bottom}; break;
            case 2:  p = sf::Vector2f{left, yDist(g.rng)}; break;
            default: p = sf::Vector2f{right, yDist(g.rng)}; break;
        }
        if (insideWorld(p)) {
            return p;
        }
    }
    return sf::Vector2f{std::clamp(left, 0.f, kWorldW), std::clamp(top, 0.f, kWorldH)};
}

// ---- 플레이 한 고정 스텝 ----
void stepPlay(Game& g, Input& in, sf::RenderWindow& window, float dt) {
    g.playTime += dt;
    g.invincible = std::max(0.f, g.invincible - dt);
    g.fireCooldown = std::max(0.f, g.fireCooldown - dt);

    // 1) 플레이어 이동 (8방향, 대각선도 같은 속력)
    sf::Vector2f move{0.f, 0.f};
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left)) {
        move.x -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) {
        move.x += 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)) {
        move.y -= 1.f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)) {
        move.y += 1.f;
    }
    const float moveLen = vlen(move);
    if (moveLen > 0.f) {
        g.player.x += move.x / moveLen * kPlayerSpeed * dt;
        g.player.y += move.y / moveLen * kPlayerSpeed * dt;
    }
    g.player.x = std::clamp(g.player.x, kPlayerMargin, kWorldW - kPlayerMargin);
    g.player.y = std::clamp(g.player.y, kPlayerMargin, kWorldH - kPlayerMargin);

    // 2) 카메라: 플레이어를 가운데에 두되 월드 밖이 보이지 않게 제한
    g.camera.x = std::clamp(g.player.x, kViewW / 2.f, kWorldW - kViewW / 2.f);
    g.camera.y = std::clamp(g.player.y, kViewH / 2.f, kWorldH - kViewH / 2.f);
    g.gameView.setCenter(g.camera);

    // 3) 조준: 현재 카메라 기준으로 마우스가 가리키는 월드 좌표
    g.aim = window.mapPixelToCoords(sf::Mouse::getPosition(window), g.gameView);
    const sf::Vector2f toAim{g.aim.x - g.player.x, g.aim.y - g.player.y};
    const float aimLen = vlen(toAim);
    if (aimLen > 0.f) {
        g.aimDir = sf::Vector2f{toAim.x / aimLen, toAim.y / aimLen};
    }
    g.playerRot = sf::radians(std::atan2(g.aimDir.y, g.aimDir.x));

    // 4) 사격: 누르고 있는 동안 0.15초마다, 짧은 클릭도 한 발 보장
    const bool held = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    if ((in.shootTap || held) && g.fireCooldown <= 0.f) {
        Bullet b;
        b.pos = g.player;
        b.vel = sf::Vector2f{g.aimDir.x * kBulletSpeed, g.aimDir.y * kBulletSpeed};
        b.life = kBulletLife;
        g.bullets.push_back(b);
        g.fireCooldown = kFireInterval;
        in.shootTap = false;
        g.res.shootSnd->play();
    }

    // 5) 총알 이동과 수명
    for (Bullet& b : g.bullets) {
        b.pos.x += b.vel.x * dt;
        b.pos.y += b.vel.y * dt;
        b.life -= dt;
        if (b.life <= 0.f || !insideWorld(b.pos)) {
            b.dead = true;
        }
    }

    // 6) 적 생성: 0.5초 뒤부터 1초마다, 10초마다 0.1초씩 빨라져 최소 0.3초
    g.spawnCountdown -= dt;
    if (g.spawnCountdown <= 0.f) {
        if (g.enemies.size() < kMaxEnemies) {
            g.enemies.push_back(Enemy{spawnPoint(g), false});
        }
        g.spawnCountdown += std::max(0.3f, 1.f - 0.1f * std::floor(g.playTime / 10.f));
    }

    // 7) 적 이동: 플레이어를 향해 100 px/s
    for (Enemy& e : g.enemies) {
        const sf::Vector2f d{g.player.x - e.pos.x, g.player.y - e.pos.y};
        const float len = vlen(d);
        if (len > 0.f) {
            e.pos.x += d.x / len * kEnemySpeed * dt;
            e.pos.y += d.y / len * kEnemySpeed * dt;
        }
    }

    // 8) 총알과 적 충돌 (반지름 합 18)
    const float hitDist = kBulletRadius + kEnemyRadius;
    for (Bullet& b : g.bullets) {
        if (b.dead) {
            continue;
        }
        for (Enemy& e : g.enemies) {
            if (e.dead) {
                continue;
            }
            if (distSq(b.pos, e.pos) < hitDist * hitDist) {
                b.dead = true;
                e.dead = true;
                g.score += static_cast<int>(kScorePerHit);
                g.res.hitSnd->play();
                break;
            }
        }
    }

    // 9) 적과 플레이어 충돌 (무적 중에는 무시, 한 갱신에 하나만 처리)
    if (g.invincible <= 0.f) {
        const float touchDist = kPlayerRadius + kEnemyRadius;
        for (Enemy& e : g.enemies) {
            if (e.dead) {
                continue;
            }
            if (distSq(e.pos, g.player) < touchDist * touchDist) {
                e.dead = true;
                --g.hp;
                g.invincible = kInvincibleTime;
                g.res.loseSnd->play();
                break;
            }
        }
    }

    // 10) 삭제 처리
    std::erase_if(g.bullets, [](const Bullet& b) { return b.dead; });
    std::erase_if(g.enemies, [](const Enemy& e) { return e.dead; });

    // 11) 결과 전환
    if (g.hp <= 0) {
        g.scene = Scene::Result;
        g.paused = false;
        g.res.bgm.stop();
    }
}

void update(Game& g, Input& in, sf::RenderWindow& window, float dt) {
    // 일회성 입력은 이 스텝에서 소비. 발사 요청(shootTap)은 실제 발사될 때까지 유지.
    const bool enterPressed = in.enter;
    const bool pausePressed = in.pause;
    const bool restartPressed = in.restart;
    in.enter = false;
    in.pause = false;
    in.restart = false;

    switch (g.scene) {
        case Scene::Title:
            in.shootTap = false;
            g.titleTime += dt;
            if (enterPressed || g.titleTime >= kTitleDuration) {
                startPlay(g);
            }
            break;

        case Scene::Play:
            if (pausePressed) {
                togglePause(g);
            }
            if (g.paused) {
                in.shootTap = false;
                break;
            }
            stepPlay(g, in, window, dt);
            break;

        case Scene::Result:
            in.shootTap = false;
            if (restartPressed) {
                startPlay(g);
            }
            break;
    }
}

// ---- 렌더링 ----
void drawPlay(sf::RenderWindow& window, const Game& g) {
    window.setView(g.gameView);

    // 배경과 격자
    sf::RectangleShape bg(sf::Vector2f{kWorldW, kWorldH});
    bg.setFillColor(kBgColor);
    window.draw(bg);

    sf::RectangleShape line;
    line.setFillColor(kGridColor);
    for (float x = 0.f; x <= kWorldW; x += kGridStep) {
        line.setSize(sf::Vector2f{1.f, kWorldH});
        line.setPosition(sf::Vector2f{x, 0.f});
        window.draw(line);
    }
    for (float y = 0.f; y <= kWorldH; y += kGridStep) {
        line.setSize(sf::Vector2f{kWorldW, 1.f});
        line.setPosition(sf::Vector2f{0.f, y});
        window.draw(line);
    }

    // 월드 경계 (안쪽으로 2px)
    sf::RectangleShape border(sf::Vector2f{kWorldW, kWorldH});
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(kBorderColor);
    border.setOutlineThickness(-2.f);
    window.draw(border);

    // 총알
    sf::Sprite bulletSprite(g.res.bulletTex);
    bulletSprite.setOrigin(sf::Vector2f{4.f, 4.f});
    for (const Bullet& b : g.bullets) {
        bulletSprite.setPosition(b.pos);
        window.draw(bulletSprite);
    }

    // 적
    sf::Sprite enemySprite(g.res.enemyTex);
    enemySprite.setOrigin(sf::Vector2f{16.f, 16.f});
    for (const Enemy& e : g.enemies) {
        enemySprite.setPosition(e.pos);
        window.draw(enemySprite);
    }

    // 플레이어 (무적 중에는 반투명하게 깜빡임)
    sf::Sprite playerSprite(g.res.playerTex);
    playerSprite.setOrigin(sf::Vector2f{16.f, 16.f});
    playerSprite.setPosition(g.player);
    playerSprite.setRotation(g.playerRot);
    std::uint8_t alpha = 255;
    if (g.invincible > 0.f && static_cast<int>(g.invincible * 10.f) % 2 == 0) {
        alpha = 90;
    }
    playerSprite.setColor(sf::Color(255, 255, 255, alpha));
    window.draw(playerSprite);

    // HUD (화면 고정)
    window.setView(g.uiView);
    drawText(window, g.res,
             std::format("HP {}  SCORE {}  TIME {:.1f}", g.hp, g.score, g.playTime),
             32, sf::Vector2f{8.f, 8.f}, kTextColor, false);

    if (g.paused) {
        drawText(window, g.res, "PAUSED", 48, sf::Vector2f{kViewW / 2.f, kViewH / 2.f},
                 kTextColor, true);
    }
}

void drawUiBackground(sf::RenderWindow& window) {
    sf::RectangleShape bg(sf::Vector2f{kViewW, kViewH});
    bg.setFillColor(kBgColor);
    window.draw(bg);
}

void render(Game& g, sf::RenderWindow& window) {
    window.clear(sf::Color::Black);

    if (g.scene == Scene::Play) {
        drawPlay(window, g);
    } else {
        window.setView(g.uiView);
        drawUiBackground(window);
        if (g.scene == Scene::Title) {
            drawText(window, g.res, "ARENA", 48, sf::Vector2f{kViewW / 2.f, 220.f}, kTextColor, true);
            drawText(window, g.res, "PRESS ENTER", 32, sf::Vector2f{kViewW / 2.f, 320.f}, kTextColor, true);
        } else {
            drawText(window, g.res, "GAME OVER", 48, sf::Vector2f{kViewW / 2.f, 170.f}, kTextColor, true);
            drawText(window, g.res, std::format("SCORE {}", g.score), 32,
                     sf::Vector2f{kViewW / 2.f, 260.f}, kTextColor, true);
            drawText(window, g.res, std::format("TIME {:.1f}", g.playTime), 32,
                     sf::Vector2f{kViewW / 2.f, 310.f}, kTextColor, true);
            drawText(window, g.res, "R: RESTART  ESC: QUIT", 32,
                     sf::Vector2f{kViewW / 2.f, 420.f}, kTextColor, true);
        }
    }
}

// ---- 실행 ----
int run() {
    Game game;

    // 에셋 로드 (실패 시 경로를 표준 에러에 출력하고 종료 코드 1)
    if (!game.res.font.openFromFile(kFontPath)) return reportMissing(kFontPath);
    if (!game.res.playerTex.loadFromFile(kPlayerPath)) return reportMissing(kPlayerPath);
    if (!game.res.enemyTex.loadFromFile(kEnemyPath)) return reportMissing(kEnemyPath);
    if (!game.res.bulletTex.loadFromFile(kBulletPath)) return reportMissing(kBulletPath);
    if (!game.res.shootBuf.loadFromFile(kShootPath)) return reportMissing(kShootPath);
    if (!game.res.hitBuf.loadFromFile(kHitPath)) return reportMissing(kHitPath);
    if (!game.res.loseBuf.loadFromFile(kLosePath)) return reportMissing(kLosePath);
    if (!game.res.bgm.openFromFile(kBgmPath)) return reportMissing(kBgmPath);

    game.res.shootSnd.emplace(game.res.shootBuf);
    game.res.hitSnd.emplace(game.res.hitBuf);
    game.res.loseSnd.emplace(game.res.loseBuf);
    game.res.bgm.setLooping(true);
    game.res.bgm.setVolume(40.f);

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    Input input;
    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen()) {
        pollInput(window, input);
        if (!window.isOpen()) {
            break;
        }

        layoutViews(window, game);

        // 고정 시간 스텝: 프레임레이트와 무관하게 같은 속도로 진행
        accumulator += std::min(clock.restart().asSeconds(), 0.25f);
        while (accumulator >= kStep) {
            update(game, input, window, kStep);
            accumulator -= kStep;
        }

        render(game, window);
        window.display();
    }

    return 0;
}

} // namespace

int main() {
    try {
        return run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
```