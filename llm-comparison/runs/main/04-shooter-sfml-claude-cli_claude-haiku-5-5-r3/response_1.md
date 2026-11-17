```cpp
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float kStep = 1.f / 60.f;
constexpr float kViewW = 960.f;
constexpr float kViewH = 540.f;
constexpr float kWorldW = 1920.f;
constexpr float kWorldH = 1080.f;
constexpr float kPlayerSpeed = 240.f;
constexpr float kPlayerR = 14.f;
constexpr float kPlayerMargin = 16.f;
constexpr float kBulletSpeed = 600.f;
constexpr float kBulletR = 4.f;
constexpr float kBulletLife = 1.4f;
constexpr float kFireInterval = 0.15f;
constexpr float kEnemyR = 14.f;
constexpr float kEnemySpeed = 100.f;
constexpr std::size_t kMaxEnemies = 200;
constexpr float kSpawnMargin = 64.f;
constexpr float kFirstSpawn = 0.5f;
constexpr float kInvulnTime = 1.f;
constexpr int kStartHp = 3;
constexpr float kTitleTime = 2.f;
constexpr float kMaxFrameDt = 0.25f;

const sf::Color kBgColor(0x14, 0x18, 0x1F);
const sf::Color kGridColor(0x22, 0x2A, 0x35);
const sf::Color kBorderColor(0x5C, 0x6B, 0x80);

const char* const kFontPath = "assets/fonts/game.ttf";
const char* const kPlayerPath = "assets/textures/player.png";
const char* const kEnemyPath = "assets/textures/enemy.png";
const char* const kBulletPath = "assets/textures/bullet.png";
const char* const kShootPath = "assets/sfx/shoot.wav";
const char* const kHitPath = "assets/sfx/hit.wav";
const char* const kLosePath = "assets/sfx/lose.wav";
const char* const kMusicPath = "assets/music/bgm_shooter.ogg";

enum class Scene { Title, Play, Result };

struct Bullet {
    sf::Vector2f pos;
    sf::Vector2f vel;
    float age = 0.f;
};

struct Enemy {
    sf::Vector2f pos;
};

struct Game {
    Scene scene = Scene::Title;
    float titleTimer = 0.f;
    bool paused = false;
    int hp = kStartHp;
    int score = 0;
    float playTime = 0.f;
    float spawnTimer = kFirstSpawn;
    float fireCooldown = 0.f;
    float invulnTimer = 0.f;
    sf::Vector2f player{kWorldW / 2.f, kWorldH / 2.f};
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    std::mt19937 rng{std::random_device{}()};

    // 입력 래치: 이벤트로 받은 짧은 입력을 다음 갱신에서 놓치지 않기 위함
    bool mouseHeld = false;
    bool fireLatch = false;
    bool tapUp = false;
    bool tapDown = false;
    bool tapLeft = false;
    bool tapRight = false;
};

struct Assets {
    sf::Font font;
    sf::Texture playerTex;
    sf::Texture enemyTex;
    sf::Texture bulletTex;
    sf::SoundBuffer shootBuf;
    sf::SoundBuffer hitBuf;
    sf::SoundBuffer loseBuf;
    sf::Music music;
};

struct Ctx {
    sf::RenderWindow& window;
    sf::Music& music;
    sf::Sound& shoot;
    sf::Sound& hit;
    sf::Sound& lose;
    sf::FloatRect viewport;
    sf::Vector2i mousePix;
};

bool reportLoadFailure(const char* path) {
    std::cerr << "asset load failed: " << path << '\n';
    return false;
}

bool loadAssets(Assets& a) {
    if (!a.font.openFromFile(kFontPath)) return reportLoadFailure(kFontPath);
    if (!a.playerTex.loadFromFile(kPlayerPath)) return reportLoadFailure(kPlayerPath);
    if (!a.enemyTex.loadFromFile(kEnemyPath)) return reportLoadFailure(kEnemyPath);
    if (!a.bulletTex.loadFromFile(kBulletPath)) return reportLoadFailure(kBulletPath);
    if (!a.shootBuf.loadFromFile(kShootPath)) return reportLoadFailure(kShootPath);
    if (!a.hitBuf.loadFromFile(kHitPath)) return reportLoadFailure(kHitPath);
    if (!a.loseBuf.loadFromFile(kLosePath)) return reportLoadFailure(kLosePath);
    if (!a.music.openFromFile(kMusicPath)) return reportLoadFailure(kMusicPath);
    return true;
}

sf::String toStr(std::string_view s) {
    return sf::String(s);
}

void setStr(sf::Text& t, std::string_view s) {
    t.setString(toStr(s));
}

sf::Text makeText(const sf::Font& font, std::string_view s, unsigned int size) {
    sf::Text t(font, toStr(s), size);
    t.setFillColor(sf::Color::White);
    return t;
}

void placeCenter(sf::Text& t, float x, float y) {
    const sf::FloatRect b = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f));
    t.setPosition(sf::Vector2f(x, y));
}

bool inWorld(sf::Vector2f p) {
    return p.x >= 0.f && p.x <= kWorldW && p.y >= 0.f && p.y <= kWorldH;
}

// 카메라 중심: 960x540 시야가 월드 밖을 보지 않도록 제한
sf::Vector2f cameraCenter(sf::Vector2f p) {
    return sf::Vector2f(std::clamp(p.x, kViewW / 2.f, kWorldW - kViewW / 2.f),
                        std::clamp(p.y, kViewH / 2.f, kWorldH - kViewH / 2.f));
}

// 16:9를 유지하는 레터박스 뷰포트 (정규화 좌표)
sf::FloatRect letterboxViewport(sf::Vector2u ws) {
    if (ws.x == 0 || ws.y == 0)
        return sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(1.f, 1.f));
    const float wx = static_cast<float>(ws.x);
    const float wy = static_cast<float>(ws.y);
    const float s = std::min(wx / kViewW, wy / kViewH);
    const float w = kViewW * s / wx;
    const float h = kViewH * s / wy;
    return sf::FloatRect(sf::Vector2f((1.f - w) / 2.f, (1.f - h) / 2.f), sf::Vector2f(w, h));
}

sf::View makeView(sf::Vector2f center, const sf::FloatRect& vp) {
    sf::View view(sf::FloatRect(sf::Vector2f(center.x - kViewW / 2.f, center.y - kViewH / 2.f),
                                sf::Vector2f(kViewW, kViewH)));
    view.setViewport(vp);
    return view;
}

float spawnInterval(float t) {
    return std::max(0.3f, 1.0f - 0.1f * std::floor(t / 10.f));
}

// 카메라 시야를 사방 64px 넓힌 사각형의 테두리 위, 월드 안의 무작위 점
void spawnEnemy(Game& g, sf::Vector2f cam) {
    const float L = cam.x - kViewW / 2.f - kSpawnMargin;
    const float R = cam.x + kViewW / 2.f + kSpawnMargin;
    const float T = cam.y - kViewH / 2.f - kSpawnMargin;
    const float B = cam.y + kViewH / 2.f + kSpawnMargin;
    const float W = R - L;
    const float H = B - T;

    const int side = std::uniform_int_distribution<int>(0, 3)(g.rng);
    const float u = std::uniform_real_distribution<float>(0.f, 1.f)(g.rng);

    sf::Vector2f p;
    switch (side) {
        case 0:  p = sf::Vector2f(L + u * W, T); break;
        case 1:  p = sf::Vector2f(L + u * W, B); break;
        case 2:  p = sf::Vector2f(L, T + u * H); break;
        default: p = sf::Vector2f(R, T + u * H); break;
    }
    p.x = std::clamp(p.x, 0.f, kWorldW);
    p.y = std::clamp(p.y, 0.f, kWorldH);
    g.enemies.push_back(Enemy{p});
}

template <typename T>
void removeMarked(std::vector<T>& v, const std::vector<bool>& dead) {
    std::size_t w = 0;
    for (std::size_t r = 0; r < v.size(); ++r) {
        if (!dead[r]) {
            if (w != r) v[w] = v[r];
            ++w;
        }
    }
    v.resize(w);
}

void startPlay(Game& g, sf::Music& music) {
    g.scene = Scene::Play;
    g.paused = false;
    g.hp = kStartHp;
    g.score = 0;
    g.playTime = 0.f;
    g.spawnTimer = kFirstSpawn;
    g.fireCooldown = 0.f;
    g.invulnTimer = 0.f;
    g.player = sf::Vector2f(kWorldW / 2.f, kWorldH / 2.f);
    g.bullets.clear();
    g.enemies.clear();
    g.fireLatch = false;
    g.tapUp = false;
    g.tapDown = false;
    g.tapLeft = false;
    g.tapRight = false;
    music.stop();
    music.play();
}

void stepGame(Game& g, const Ctx& c) {
    // 탭 래치는 이 갱신에서 한 번만 소비한다
    const bool tapU = std::exchange(g.tapUp, false);
    const bool tapD = std::exchange(g.tapDown, false);
    const bool tapL = std::exchange(g.tapLeft, false);
    const bool tapR = std::exchange(g.tapRight, false);

    if (g.scene == Scene::Title) {
        g.titleTimer += kStep;
        if (g.titleTimer >= kTitleTime) startPlay(g, c.music);
        return;
    }
    if (g.scene != Scene::Play || g.paused) return;

    g.playTime += kStep;
    g.invulnTimer = std::max(0.f, g.invulnTimer - kStep);

    // 이동 (8방향, 대각선도 같은 속력)
    const bool up = tapU || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
                    sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
    const bool down = tapD || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
                      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);
    const bool left = tapL || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
                      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
    const bool right = tapR || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
                       sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
    sf::Vector2f dir((right ? 1.f : 0.f) - (left ? 1.f : 0.f),
                     (down ? 1.f : 0.f) - (up ? 1.f : 0.f));
    if (dir.lengthSquared() > 0.f)
        g.player += dir.normalized() * (kPlayerSpeed * kStep);
    g.player.x = std::clamp(g.player.x, kPlayerMargin, kWorldW - kPlayerMargin);
    g.player.y = std::clamp(g.player.y, kPlayerMargin, kWorldH - kPlayerMargin);

    // 현재 카메라와 커서로 조준 월드 좌표를 구한다
    const sf::Vector2f cam = cameraCenter(g.player);
    const sf::Vector2f aim = c.window.mapPixelToCoords(c.mousePix, makeView(cam, c.viewport));

    // 사격
    g.fireCooldown -= kStep;
    if ((g.mouseHeld || g.fireLatch) && g.fireCooldown <= 1e-4f) {
        g.fireLatch = false;
        g.fireCooldown = kFireInterval;
        sf::Vector2f d = aim - g.player;
        if (d.lengthSquared() < 1e-6f) d = sf::Vector2f(1.f, 0.f);
        g.bullets.push_back(Bullet{g.player, d.normalized() * kBulletSpeed, 0.f});
        c.shoot.play();
    }

    // 적 생성
    g.spawnTimer -= kStep;
    if (g.spawnTimer <= 0.f) {
        if (g.enemies.size() < kMaxEnemies) spawnEnemy(g, cam);
        g.spawnTimer += spawnInterval(g.playTime);
    }

    // 총알 이동과 수명 처리
    for (Bullet& b : g.bullets) {
        b.pos += b.vel * kStep;
        b.age += kStep;
    }
    std::erase_if(g.bullets, [](const Bullet& b) {
        return b.age >= kBulletLife || !inWorld(b.pos);
    });

    // 적 이동 (플레이어를 향해)
    for (Enemy& e : g.enemies) {
        const sf::Vector2f d = g.player - e.pos;
        if (d.lengthSquared() > 1e-6f)
            e.pos += d.normalized() * (kEnemySpeed * kStep);
    }

    // 총알과 적 충돌
    std::vector<bool> bulletDead(g.bullets.size(), false);
    std::vector<bool> enemyDead(g.enemies.size(), false);
    const float hitR = kBulletR + kEnemyR;
    for (std::size_t i = 0; i < g.bullets.size(); ++i) {
        for (std::size_t j = 0; j < g.enemies.size(); ++j) {
            if (enemyDead[j]) continue;
            if ((g.bullets[i].pos - g.enemies[j].pos).lengthSquared() < hitR * hitR) {
                bulletDead[i] = true;
                enemyDead[j] = true;
                g.score += 10;
                c.hit.play();
                break;
            }
        }
    }
    removeMarked(g.bullets, bulletDead);
    removeMarked(g.enemies, enemyDead);

    // 적과 플레이어 충돌 (한 갱신에 하나만 처리, 이후 무적)
    if (g.invulnTimer <= 0.f) {
        const float r = kPlayerR + kEnemyR;
        auto it = std::find_if(g.enemies.begin(), g.enemies.end(), [&](const Enemy& e) {
            return (e.pos - g.player).lengthSquared() < r * r;
        });
        if (it != g.enemies.end()) {
            g.enemies.erase(it);
            --g.hp;
            g.invulnTimer = kInvulnTime;
            c.lose.play();
        }
    }

    if (g.hp <= 0) {
        g.scene = Scene::Result;
        c.music.pause();
    }
}

}  // namespace

int main() {
    Assets as;
    if (!loadAssets(as)) return 1;

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "ARENA");
    window.setFramerateLimit(60);

    sf::Sprite playerSpr(as.playerTex);
    sf::Sprite enemySpr(as.enemyTex);
    sf::Sprite bulletSpr(as.bulletTex);
    playerSpr.setOrigin(sf::Vector2f(16.f, 16.f));
    enemySpr.setOrigin(sf::Vector2f(16.f, 16.f));
    bulletSpr.setOrigin(sf::Vector2f(4.f, 4.f));

    sf::Sound shootSnd(as.shootBuf);
    sf::Sound hitSnd(as.hitBuf);
    sf::Sound loseSnd(as.loseBuf);

    as.music.setVolume(40.f);
    as.music.setLooping(true);

    sf::RectangleShape screenBg(sf::Vector2f(kViewW, kViewH));
    screenBg.setFillColor(kBgColor);
    sf::RectangleShape worldBg(sf::Vector2f(kWorldW, kWorldH));
    worldBg.setFillColor(kBgColor);
    sf::RectangleShape vLine(sf::Vector2f(1.f, kWorldH));
    vLine.setFillColor(kGridColor);
    sf::RectangleShape hLine(sf::Vector2f(kWorldW, 1.f));
    hLine.setFillColor(kGridColor);
    sf::RectangleShape border(sf::Vector2f(kWorldW, kWorldH));
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(kBorderColor);
    border.setOutlineThickness(-2.f);

    sf::Text titleText = makeText(as.font, "ARENA", 48);
    sf::Text pressText = makeText(as.font, "PRESS ENTER", 32);
    sf::Text hudText = makeText(as.font, "", 16);
    sf::Text pausedText = makeText(as.font, "PAUSED", 32);
    sf::Text resultTitle = makeText(as.font, "GAME OVER", 48);
    sf::Text resultScore = makeText(as.font, "", 32);
    sf::Text resultTime = makeText(as.font, "", 32);
    sf::Text resultHint = makeText(as.font, "R: RESTART  ESC: QUIT", 16);

    Game game;
    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                const auto code = key->code;
                if (code == sf::Keyboard::Key::Escape) {
                    window.close();
                } else if (code == sf::Keyboard::Key::Enter && game.scene == Scene::Title) {
                    startPlay(game, as.music);
                } else if (code == sf::Keyboard::Key::P && game.scene == Scene::Play) {
                    game.paused = !game.paused;
                    if (game.paused) as.music.pause();
                    else as.music.play();
                } else if (code == sf::Keyboard::Key::R && game.scene == Scene::Result) {
                    startPlay(game, as.music);
                } else if (code == sf::Keyboard::Key::W || code == sf::Keyboard::Key::Up) {
                    game.tapUp = true;
                } else if (code == sf::Keyboard::Key::S || code == sf::Keyboard::Key::Down) {
                    game.tapDown = true;
                } else if (code == sf::Keyboard::Key::A || code == sf::Keyboard::Key::Left) {
                    game.tapLeft = true;
                } else if (code == sf::Keyboard::Key::D || code == sf::Keyboard::Key::Right) {
                    game.tapRight = true;
                }
            }

            if (const auto* press = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (press->button == sf::Mouse::Button::Left) {
                    game.mouseHeld = true;
                    game.fireLatch = true;
                }
            }

            if (const auto* release = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (release->button == sf::Mouse::Button::Left) game.mouseHeld = false;
            }

            if (event->is<sf::Event::FocusLost>()) game.mouseHeld = false;
        }

        if (!window.isOpen()) break;

        // 고정 간격(1/60초) 갱신: 프레임레이트와 무관하게 같은 속도로 진행
        const float frameDt = clock.restart().asSeconds();
        accumulator += std::min(frameDt, kMaxFrameDt);

        const sf::FloatRect vp = letterboxViewport(window.getSize());
        const sf::Vector2i mousePix = sf::Mouse::getPosition(window);
        const Ctx ctx{window, as.music, shootSnd, hitSnd, loseSnd, vp, mousePix};

        while (accumulator >= kStep) {
            stepGame(game, ctx);
            accumulator -= kStep;
        }

        // 렌더링
        window.clear(sf::Color::Black);
        const sf::Vector2f cam = cameraCenter(game.player);
        const sf::View gameView = makeView(cam, vp);
        const sf::View hudView = makeView(sf::Vector2f(kViewW / 2.f, kViewH / 2.f), vp);

        if (game.scene == Scene::Title) {
            window.setView(hudView);
            window.draw(screenBg);
            placeCenter(titleText, kViewW / 2.f, 220.f);
            window.draw(titleText);
            placeCenter(pressText, kViewW / 2.f, 300.f);
            window.draw(pressText);
        } else if (game.scene == Scene::Play) {
            window.setView(gameView);
            window.draw(worldBg);
            for (int i = 0; i <= 16; ++i) {
                vLine.setPosition(sf::Vector2f(static_cast<float>(i) * 120.f, 0.f));
                window.draw(vLine);
            }
            for (int i = 0; i <= 9; ++i) {
                hLine.setPosition(sf::Vector2f(0.f, static_cast<float>(i) * 120.f));
                window.draw(hLine);
            }
            window.draw(border);

            for (const Bullet& b : game.bullets) {
                bulletSpr.setPosition(b.pos);
                window.draw(bulletSpr);
            }
            for (const Enemy& e : game.enemies) {
                enemySpr.setPosition(e.pos);
                window.draw(enemySpr);
            }

            const sf::Vector2f aim = window.mapPixelToCoords(mousePix, gameView);
            const sf::Vector2f d = aim - game.player;
            if (d.lengthSquared() > 1e-6f)
                playerSpr.setRotation(sf::radians(std::atan2(d.y, d.x)));
            playerSpr.setPosition(game.player);

            std::uint8_t alpha = 255;
            if (game.invulnTimer > 0.f && std::fmod(game.invulnTimer, 0.2f) < 0.1f)
                alpha = 90;
            playerSpr.setColor(sf::Color(255, 255, 255, alpha));
            window.draw(playerSpr);

            window.setView(hudView);
            setStr(hudText, std::format("HP {}  SCORE {}  TIME {:.1f}",
                                        game.hp, game.score, game.playTime));
            hudText.setPosition(sf::Vector2f(16.f, 16.f));
            window.draw(hudText);

            if (game.paused) {
                placeCenter(pausedText, kViewW / 2.f, kViewH / 2.f);
                window.draw(pausedText);
            }
        } else {
            window.setView(hudView);
            window.draw(screenBg);
            placeCenter(resultTitle, kViewW / 2.f, 180.f);
            window.draw(resultTitle);
            setStr(resultScore, std::format("SCORE {}", game.score));
            placeCenter(resultScore, kViewW / 2.f, 260.f);
            window.draw(resultScore);
            setStr(resultTime, std::format("TIME {:.1f}", game.playTime));
            placeCenter(resultTime, kViewW / 2.f, 320.f);
            window.draw(resultTime);
            placeCenter(resultHint, kViewW / 2.f, 420.f);
            window.draw(resultHint);
        }

        window.display();
    }

    return 0;
}
```