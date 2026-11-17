#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float kW = 960.f;
constexpr float kH = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kGroundH = 40.f;
constexpr float kLaunchTopY = 476.f;  // 도시·발사대의 위쪽 가운데 y
constexpr float kFireMaxY = 470.f;    // 이 y 이상(아래쪽)의 클릭은 무시
constexpr float kInterceptorSpeed = 600.f;
constexpr float kExplosionLife = 1.f;
constexpr float kExplosionHalf = 0.5f;
constexpr float kExplosionMaxR = 48.f;
constexpr int kKillScore = 25;
constexpr int kCityScore = 100;
constexpr int kAmmoScore = 5;
constexpr int kAmmoPerWave = 10;
constexpr float kTitleTime = 2.f;
constexpr float kClearTime = 3.f;
constexpr float kFirstSpawn = 1.f;
constexpr float kSpawnGap = 1.2f;

constexpr std::array<float, 6> kCityX{160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr std::array<float, 3> kLaunchX{60.f, 480.f, 900.f};

const sf::Color kGroundColor{176, 122, 42};   // #B07A2A
const sf::Color kMissileRed{255, 77, 77};     // #FF4D4D
const sf::Color kInterceptBlue{79, 195, 247}; // #4FC3F7
const sf::Color kExplosionYellow{255, 213, 74}; // #FFD54A

const char* const kFontPath = "assets/fonts/game.ttf";
const char* const kCityPath = "assets/textures/md_city.png";
const char* const kBasePath = "assets/textures/md_base.png";
const char* const kCrossPath = "assets/textures/md_crosshair.png";
const char* const kLaunchSfx = "assets/sfx/launch.wav";
const char* const kExplodeSfx = "assets/sfx/explode.wav";
const char* const kLoseSfx = "assets/sfx/lose.wav";
const char* const kBgm = "assets/music/bgm_missile.ogg";

float vlen(sf::Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

bool inArea(sf::Vector2f p) {
    return p.x >= 0.f && p.x <= kW && p.y >= 0.f && p.y <= kH;
}

float explosionRadius(float age) {
    if (age < 0.f || age >= kExplosionLife) {
        return 0.f;
    }
    const float ratio = age < kExplosionHalf
                            ? age / kExplosionHalf
                            : (kExplosionLife - age) / kExplosionHalf;
    return kExplosionMaxR * ratio;
}

void drawLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, float thickness, sf::Color color) {
    const sf::Vector2f d = b - a;
    const float length = vlen(d);
    if (length <= 0.f) {
        return;
    }
    sf::RectangleShape rect({length, thickness});
    rect.setOrigin({0.f, thickness / 2.f});
    rect.setPosition(a);
    rect.setRotation(sf::radians(std::atan2(d.y, d.x)));
    rect.setFillColor(color);
    target.draw(rect);
}

void drawText(sf::RenderTarget& target, const sf::Font& font, std::string_view str,
              unsigned int size, sf::Vector2f pos, bool centered) {
    sf::Text text(font, sf::String(str), size);
    text.setFillColor(sf::Color::White);
    if (centered) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    }
    text.setPosition(pos);
    target.draw(text);
}

struct Missile {
    sf::Vector2f start;
    sf::Vector2f head;
    sf::Vector2f target;
    int kind;            // 0: 도시, 1: 발사대
    std::size_t index;
    float speed;
};

struct Interceptor {
    sf::Vector2f origin;
    sf::Vector2f target;
    sf::Vector2f pos;
};

struct Explosion {
    sf::Vector2f center;
    float age;
};

enum class Screen { Title, Play, Result };

class Game {
public:
    explicit Game(sf::RenderWindow& w) : window(w) {}

    bool loadAssets() {
        auto fail = [](const char* path) {
            std::cerr << "failed to load asset: " << path << '\n';
            return false;
        };
        if (!font.openFromFile(kFontPath)) return fail(kFontPath);
        if (!cityTex.loadFromFile(kCityPath)) return fail(kCityPath);
        if (!baseTex.loadFromFile(kBasePath)) return fail(kBasePath);
        if (!crossTex.loadFromFile(kCrossPath)) return fail(kCrossPath);
        if (!launchBuf.loadFromFile(kLaunchSfx)) return fail(kLaunchSfx);
        if (!explodeBuf.loadFromFile(kExplodeSfx)) return fail(kExplodeSfx);
        if (!loseBuf.loadFromFile(kLoseSfx)) return fail(kLoseSfx);
        if (!music.openFromFile(kBgm)) return fail(kBgm);
        music.setVolume(40.f);
        music.setLooping(true);
        return true;
    }

    void run() {
        window.setFramerateLimit(60);
        mousePix = sf::Mouse::getPosition(window);
        updateView();
        sf::Clock clock;

        while (window.isOpen()) {
            bool enter = false;
            bool rKey = false;
            std::vector<sf::Vector2f> clicks;

            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                } else if (event->is<sf::Event::Resized>()) {
                    updateView();
                } else if (const auto* mm = event->getIf<sf::Event::MouseMoved>()) {
                    mousePix = mm->position;
                } else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    mousePix = mb->position;
                    if (mb->button == sf::Mouse::Button::Left) {
                        clicks.push_back(toGame(mb->position));
                    }
                } else if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Escape) {
                        window.close();
                    } else if (kp->code == sf::Keyboard::Key::Enter) {
                        enter = true;
                    } else if (kp->code == sf::Keyboard::Key::R) {
                        rKey = true;
                    }
                }
            }
            if (!window.isOpen()) {
                break;
            }

            const float dt = std::min(clock.restart().asSeconds(), 0.1f);

            switch (screen) {
                case Screen::Title:
                    titleTime += dt;
                    if (enter || titleTime >= kTitleTime) {
                        startPlay();
                    }
                    break;
                case Screen::Play:
                    updatePlay(dt, clicks);
                    break;
                case Screen::Result:
                    if (rKey) {
                        startPlay();
                    }
                    break;
            }

            std::erase_if(sounds, [](const std::unique_ptr<sf::Sound>& s) {
                return s->getStatus() == sf::SoundSource::Status::Stopped;
            });

            window.clear(sf::Color::Black);
            drawScreen();
            drawCrosshair();
            window.display();
        }
    }

private:
    // ---- 화면 / 좌표 ----
    void updateView() {
        const sf::Vector2u s = window.getSize();
        if (s.x == 0 || s.y == 0) {
            return;
        }
        const float scale = std::min(static_cast<float>(s.x) / kW, static_cast<float>(s.y) / kH);
        const float vw = kW * scale;
        const float vh = kH * scale;
        const float vx = (static_cast<float>(s.x) - vw) / 2.f;
        const float vy = (static_cast<float>(s.y) - vh) / 2.f;
        view.setViewport(sf::FloatRect({vx / static_cast<float>(s.x), vy / static_cast<float>(s.y)},
                                       {vw / static_cast<float>(s.x), vh / static_cast<float>(s.y)}));
        window.setView(view);
    }

    sf::Vector2f toGame(sf::Vector2i pix) const {
        return window.mapPixelToCoords(pix, view);
    }

    // ---- 상태 전이 ----
    void resetAll() {
        score = 0;
        wave = 1;
        gameOver = false;
        cityAlive.fill(true);
        missiles.clear();
        interceptors.clear();
        explosions.clear();
        startWave();
    }

    void startWave() {
        spawned = 0;
        total = static_cast<std::size_t>(10 + 2 * (wave - 1));
        waveTime = 0.f;
        clearing = false;
        clearTimer = 0.f;
        launchDestroyed.fill(false);
        ammo.fill(kAmmoPerWave);
    }

    void startPlay() {
        resetAll();
        screen = Screen::Play;
        music.play();
    }

    void enterResult() {
        screen = Screen::Result;
        music.stop();
    }

    void beginClear() {
        int bonus = 0;
        for (std::size_t i = 0; i < launchDestroyed.size(); ++i) {
            if (!launchDestroyed[i]) {
                bonus += ammo[i] * kAmmoScore;
            }
        }
        for (bool alive : cityAlive) {
            if (alive) {
                bonus += kCityScore;
            }
        }
        score += bonus;
        clearing = true;
        clearTimer = kClearTime;
    }

    // ---- 플레이 ----
    void updatePlay(float dt, const std::vector<sf::Vector2f>& clicks) {
        for (const sf::Vector2f& c : clicks) {
            if (inArea(c)) {
                fire(c);
            }
        }

        if (clearing) {
            clearTimer -= dt;
            if (clearTimer <= 0.f) {
                ++wave;
                startWave();
            }
            return;
        }

        waveTime += dt;
        while (!gameOver && spawned < total &&
               waveTime >= kFirstSpawn + kSpawnGap * static_cast<float>(spawned)) {
            spawnMissile();
            ++spawned;
        }

        updateInterceptors(dt);
        updateMissiles(dt);
        updateExplosions(dt);

        const bool quiet = missiles.empty() && interceptors.empty() && explosions.empty();
        if (gameOver) {
            if (quiet) {
                enterResult();
            }
        } else if (spawned >= total && quiet) {
            beginClear();
        }
    }

    void fire(sf::Vector2f click) {
        if (clearing || click.y >= kFireMaxY) {
            return;
        }
        std::optional<std::size_t> best;
        for (std::size_t i = 0; i < kLaunchX.size(); ++i) {
            if (launchDestroyed[i] || ammo[i] <= 0) {
                continue;
            }
            if (!best || std::abs(kLaunchX[i] - click.x) < std::abs(kLaunchX[*best] - click.x)) {
                best = i;
            }
        }
        if (!best) {
            return;
        }
        --ammo[*best];
        const sf::Vector2f origin{kLaunchX[*best], kLaunchTopY};
        interceptors.push_back(Interceptor{origin, click, origin});
        playSound(launchBuf);
    }

    void spawnMissile() {
        std::vector<std::pair<int, std::size_t>> cands;
        for (std::size_t i = 0; i < cityAlive.size(); ++i) {
            if (cityAlive[i]) {
                cands.push_back({0, i});
            }
        }
        for (std::size_t i = 0; i < launchDestroyed.size(); ++i) {
            if (!launchDestroyed[i]) {
                cands.push_back({1, i});
            }
        }
        if (cands.empty()) {
            return;
        }

        std::uniform_int_distribution<std::size_t> pick(0, cands.size() - 1);
        const auto [kind, index] = cands[pick(rng)];

        std::uniform_real_distribution<float> xDist(0.f, kW);
        const sf::Vector2f start{xDist(rng), 0.f};

        sf::Vector2f target{};
        if (kind == 0) {
            target = {kCityX[index], kLaunchTopY};
        } else {
            target = {kLaunchX[index], kLaunchTopY};
        }

        const float speed = 70.f + 10.f * static_cast<float>(wave - 1);
        missiles.push_back(Missile{start, start, target, kind, index, speed});
    }

    void spawnExplosion(sf::Vector2f center) {
        explosions.push_back(Explosion{center, 0.f});
        playSound(explodeBuf);
    }

    void playSound(const sf::SoundBuffer& buffer) {
        auto s = std::make_unique<sf::Sound>(buffer);
        s->play();
        sounds.push_back(std::move(s));
    }

    void resolveHit(const Missile& m) {
        spawnExplosion(m.target);
        if (m.kind == 0) {
            if (cityAlive[m.index]) {
                cityAlive[m.index] = false;
                playSound(loseBuf);
            }
        } else {
            if (!launchDestroyed[m.index]) {
                launchDestroyed[m.index] = true;
                playSound(loseBuf);
            }
        }
        if (std::none_of(cityAlive.begin(), cityAlive.end(), [](bool a) { return a; })) {
            gameOver = true;
        }
    }

    void updateInterceptors(float dt) {
        for (std::size_t i = 0; i < interceptors.size();) {
            Interceptor& it = interceptors[i];
            const sf::Vector2f toT = it.target - it.pos;
            const float d = vlen(toT);
            const float step = kInterceptorSpeed * dt;
            if (d <= step) {
                const sf::Vector2f at = it.target;
                interceptors.erase(interceptors.begin() + static_cast<std::ptrdiff_t>(i));
                spawnExplosion(at);
            } else {
                it.pos += toT / d * step;
                ++i;
            }
        }
    }

    void updateMissiles(float dt) {
        for (std::size_t i = 0; i < missiles.size();) {
            Missile& m = missiles[i];
            const sf::Vector2f toT = m.target - m.head;
            const float d = vlen(toT);
            const float step = m.speed * dt;
            if (d <= step) {
                const Missile hit = m;
                missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(i));
                resolveHit(hit);
            } else {
                m.head += toT / d * step;
                ++i;
            }
        }
    }

    void updateExplosions(float dt) {
        for (Explosion& e : explosions) {
            e.age += dt;
        }
        std::erase_if(explosions, [](const Explosion& e) { return e.age >= kExplosionLife; });

        // 연쇄: 새로 생긴 폭발도 같은 루프에서 검사된다.
        for (std::size_t e = 0; e < explosions.size(); ++e) {
            const sf::Vector2f center = explosions[e].center;
            const float radius = explosionRadius(explosions[e].age);
            for (std::size_t m = missiles.size(); m-- > 0;) {
                if (vlen(missiles[m].head - center) <= radius) {
                    const sf::Vector2f head = missiles[m].head;
                    missiles.erase(missiles.begin() + static_cast<std::ptrdiff_t>(m));
                    score += kKillScore;
                    spawnExplosion(head);
                }
            }
        }
    }

    // ---- 그리기 ----
    void drawScreen() {
        switch (screen) {
            case Screen::Title:
                drawText(window, font, "SKY SHIELD", 48, {kW / 2.f, 220.f}, true);
                drawText(window, font, "PRESS ENTER", 16, {kW / 2.f, 320.f}, true);
                break;
            case Screen::Play:
                drawPlay();
                break;
            case Screen::Result:
                drawText(window, font, "GAME OVER", 48, {kW / 2.f, 180.f}, true);
                drawText(window, font, "SCORE " + std::to_string(score), 32, {kW / 2.f, 270.f}, true);
                drawText(window, font, "WAVE " + std::to_string(wave), 32, {kW / 2.f, 330.f}, true);
                drawText(window, font, "R: RESTART  ESC: QUIT", 16, {kW / 2.f, 440.f}, true);
                break;
        }
    }

    void drawPlay() {
        sf::RectangleShape ground({kW, kGroundH});
        ground.setPosition({0.f, kGroundY});
        ground.setFillColor(kGroundColor);
        window.draw(ground);

        for (std::size_t i = 0; i < kCityX.size(); ++i) {
            if (!cityAlive[i]) {
                continue;
            }
            sf::Sprite s(cityTex);
            s.setOrigin({24.f, 24.f});
            s.setPosition({kCityX[i], kGroundY});
            window.draw(s);
        }

        for (std::size_t i = 0; i < kLaunchX.size(); ++i) {
            if (launchDestroyed[i]) {
                continue;
            }
            sf::Sprite s(baseTex);
            s.setOrigin({24.f, 24.f});
            s.setPosition({kLaunchX[i], kGroundY});
            window.draw(s);
            drawText(window, font, std::to_string(ammo[i]), 16, {kLaunchX[i], 519.f}, true);
        }

        for (const Missile& m : missiles) {
            drawLine(window, m.start, m.head, 2.f, kMissileRed);
            sf::RectangleShape head({4.f, 4.f});
            head.setOrigin({2.f, 2.f});
            head.setPosition(m.head);
            head.setFillColor(sf::Color::White);
            window.draw(head);
        }

        for (const Interceptor& it : interceptors) {
            drawLine(window, it.origin, it.pos, 2.f, kInterceptBlue);
        }

        for (const Explosion& e : explosions) {
            const float r = explosionRadius(e.age);
            if (r <= 0.f) {
                continue;
            }
            const int phase = static_cast<int>(e.age / 0.1f) % 2;
            sf::CircleShape circle(r, 48);
            circle.setOrigin({r, r});
            circle.setPosition(e.center);
            circle.setFillColor(phase == 0 ? sf::Color::White : kExplosionYellow);
            window.draw(circle);
        }

        drawText(window, font, "SCORE " + std::to_string(score), 16, {16.f, 8.f}, false);
        drawText(window, font, "WAVE " + std::to_string(wave), 16, {16.f, 32.f}, false);

        if (clearing) {
            drawText(window, font, "WAVE " + std::to_string(wave) + " CLEAR", 32,
                     {kW / 2.f, 240.f}, true);
        }
    }

    void drawCrosshair() {
        sf::Sprite cross(crossTex);
        cross.setOrigin({12.f, 12.f});
        cross.setPosition(toGame(mousePix));
        window.draw(cross);
    }

    sf::RenderWindow& window;
    sf::View view{sf::FloatRect({0.f, 0.f}, {kW, kH})};

    sf::Font font;
    sf::Texture cityTex;
    sf::Texture baseTex;
    sf::Texture crossTex;
    sf::SoundBuffer launchBuf;
    sf::SoundBuffer explodeBuf;
    sf::SoundBuffer loseBuf;
    sf::Music music;
    std::vector<std::unique_ptr<sf::Sound>> sounds;
    std::mt19937 rng{std::random_device{}()};

    Screen screen = Screen::Title;
    float titleTime = 0.f;
    int score = 0;
    int wave = 1;
    bool gameOver = false;
    bool clearing = false;
    float waveTime = 0.f;
    float clearTimer = 0.f;
    std::size_t spawned = 0;
    std::size_t total = 0;

    std::array<bool, 6> cityAlive{};
    std::array<bool, 3> launchDestroyed{};
    std::array<int, 3> ammo{};

    sf::Vector2i mousePix{};
    std::vector<Missile> missiles;
    std::vector<Interceptor> interceptors;
    std::vector<Explosion> explosions;
};

}  // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode({960, 540}), "SKY SHIELD");
    Game game(window);
    if (!game.loadAssets()) {
        return 1;
    }
    game.run();
    return 0;
}
