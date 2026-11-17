// SKY SHIELD: SFML 3.1 미사일 방어 (main.cpp 단일 파일)
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

// ---------- 상수 ----------
constexpr float kW = 960.f;
constexpr float kH = 540.f;
constexpr float kGroundY = 500.f;
constexpr float kGroundH = 40.f;
constexpr float kSiteTopY = 476.f;   // 48x24 도시/발사대의 윗변 (아랫변은 y 500)
constexpr float kSiteHalfW = 24.f;
constexpr float kFireLineY = 470.f;
constexpr float kStep = 1.f / 60.f;  // 고정 시뮬레이션 간격
constexpr float kMaxFrameDelta = 0.25f;
constexpr int kMaxStepsPerFrame = 15;

constexpr float kTitleDelay = 2.f;
constexpr float kClearDelay = 3.f;
constexpr float kFirstSpawn = 1.f;
constexpr float kSpawnGap = 1.2f;
constexpr int kAmmoPerBase = 10;
constexpr int kKillScore = 25;
constexpr int kAmmoBonus = 5;
constexpr int kCityBonus = 100;
constexpr float kInterceptorSpeed = 600.f;
constexpr float kBlastLife = 1.f;
constexpr float kBlastMaxR = 48.f;
constexpr float kTrailW = 2.f;
constexpr float kMusicVolume = 40.f;

constexpr float kCityXs[] = {160.f, 260.f, 360.f, 600.f, 700.f, 800.f};
constexpr float kBaseXs[] = {60.f, 480.f, 900.f};

constexpr const char* kFontPath = "assets/fonts/game.ttf";
constexpr const char* kCityPath = "assets/textures/md_city.png";
constexpr const char* kBasePath = "assets/textures/md_base.png";
constexpr const char* kCrossPath = "assets/textures/md_crosshair.png";
constexpr const char* kLaunchPath = "assets/sfx/launch.wav";
constexpr const char* kExplodePath = "assets/sfx/explode.wav";
constexpr const char* kLosePath = "assets/sfx/lose.wav";
constexpr const char* kBgmPath = "assets/music/bgm_missile.ogg";

sf::Color hex(std::uint32_t rgb) {
    return sf::Color(static_cast<std::uint8_t>(rgb >> 16),
                     static_cast<std::uint8_t>(rgb >> 8),
                     static_cast<std::uint8_t>(rgb));
}

const sf::Color kGroundColor = hex(0xB07A2A);
const sf::Color kMissileTrail = hex(0xFF4D4D);
const sf::Color kInterTrail = hex(0x4FC3F7);
const sf::Color kBlastAlt = hex(0xFFD54A);

// ---------- 리소스 ----------
struct Assets {
    sf::Font font;
    sf::Texture city;
    sf::Texture base;
    sf::Texture crosshair;
    sf::SoundBuffer launchBuf;
    sf::SoundBuffer explodeBuf;
    sf::SoundBuffer loseBuf;
    // Sound는 버퍼를 참조하므로 버퍼보다 나중에 선언해 먼저 파괴되게 한다.
    std::optional<sf::Sound> launchSnd;
    std::optional<sf::Sound> explodeSnd;
    std::optional<sf::Sound> loseSnd;
    sf::Music bgm;
};

bool failLoad(const char* path) {
    std::cerr << "failed to load asset: " << path << '\n';
    return false;
}

bool loadAssets(Assets& a) {
    if (!a.font.openFromFile(kFontPath)) return failLoad(kFontPath);
    if (!a.city.loadFromFile(kCityPath)) return failLoad(kCityPath);
    if (!a.base.loadFromFile(kBasePath)) return failLoad(kBasePath);
    if (!a.crosshair.loadFromFile(kCrossPath)) return failLoad(kCrossPath);

    if (!a.launchBuf.loadFromFile(kLaunchPath)) return failLoad(kLaunchPath);
    if (!a.explodeBuf.loadFromFile(kExplodePath)) return failLoad(kExplodePath);
    if (!a.loseBuf.loadFromFile(kLosePath)) return failLoad(kLosePath);
    a.launchSnd.emplace(a.launchBuf);
    a.explodeSnd.emplace(a.explodeBuf);
    a.loseSnd.emplace(a.loseBuf);

    if (!a.bgm.openFromFile(kBgmPath)) return failLoad(kBgmPath);
    a.bgm.setLooping(true);
    a.bgm.setVolume(kMusicVolume);
    return true;
}

// ---------- 그리기 도우미 ----------
void playSfx(std::optional<sf::Sound>& snd) {
    if (snd) {
        snd->stop();
        snd->play();
    }
}

sf::String toStr(const std::string& s) {
    return sf::String(std::string_view(s));
}

void drawText(sf::RenderTarget& target, const sf::Font& font, const std::string& s,
              unsigned int size, sf::Vector2f pos, bool centered) {
    sf::Text text(font, toStr(s), size);
    if (centered) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    }
    text.setPosition(pos);
    text.setFillColor(sf::Color::White);
    target.draw(text);
}

void drawLine(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, sf::Color color) {
    const sf::Vector2f d = b - a;
    const float len = d.length();
    if (len <= 0.f) return;
    sf::RectangleShape line({len, kTrailW});
    line.setOrigin({0.f, kTrailW / 2.f});
    line.setPosition(a);
    line.setRotation(sf::radians(std::atan2(d.y, d.x)));
    line.setFillColor(color);
    target.draw(line);
}

// 뷰포트를 16:9로 유지하며 가운데에 배치 (남는 부분은 검은 띠)
void updateView(sf::View& view, sf::Vector2u size) {
    const float w = static_cast<float>(size.x);
    const float h = static_cast<float>(size.y);
    if (w <= 0.f || h <= 0.f) return;
    const float s = std::min(w / kW, h / kH);
    const float vw = kW * s;
    const float vh = kH * s;
    const float ox = (w - vw) / 2.f;
    const float oy = (h - vh) / 2.f;
    view.setViewport(sf::FloatRect({ox / w, oy / h}, {vw / w, vh / h}));
}

// ---------- 게임 객체 ----------
enum class Scene { Title, Playing, Result };

struct Site {
    float cx = 0.f;
    bool destroyed = false;
    int ammo = 0;
};

struct Missile {
    sf::Vector2f start{};
    sf::Vector2f head{};
    sf::Vector2f target{};
    float speed = 0.f;
    bool isCity = false;
    std::size_t index = 0;
    bool dead = false;
};

struct Shot {
    sf::Vector2f from{};
    sf::Vector2f pos{};
    sf::Vector2f to{};
    bool dead = false;
};

struct Blast {
    sf::Vector2f center{};
    float t = 0.f;
    bool dead = false;
};

// 0 -> 0.5초: 0에서 48px, 0.5 -> 1초: 48에서 0px (0.25초에 24px)
float blastRadius(float t) {
    const float r = (t <= 0.5f) ? kBlastMaxR * 2.f * t : kBlastMaxR * 2.f * (1.f - t);
    return std::max(0.f, r);
}

class Game {
public:
    explicit Game(Assets& assets) : as_(assets) {
        resetWorld();
    }

    void onKey(sf::Keyboard::Key key) {
        if (scene_ == Scene::Title && key == sf::Keyboard::Key::Enter) {
            startPlay();
        } else if (scene_ == Scene::Result && key == sf::Keyboard::Key::R) {
            startPlay();
        }
    }

    void onClick(sf::Vector2f p) {
        if (scene_ != Scene::Playing || clearing_) return;
        if (p.x < 0.f || p.x > kW || p.y < 0.f || p.y > kH) return;
        if (p.y >= kFireLineY) return;
        fire(p);
    }

    void update(float dt) {
        switch (scene_) {
            case Scene::Title:
                titleTimer_ += dt;
                if (titleTimer_ >= kTitleDelay) startPlay();
                break;
            case Scene::Playing:
                updatePlay(dt);
                break;
            case Scene::Result:
                break;
        }
    }

    void draw(sf::RenderTarget& t, sf::Vector2f mouse) const {
        // 땅
        sf::RectangleShape ground({kW, kGroundH});
        ground.setPosition({0.f, kGroundY});
        ground.setFillColor(kGroundColor);
        t.draw(ground);

        // 도시
        for (const Site& c : cities_) {
            if (c.destroyed) continue;
            sf::Sprite s(as_.city);
            s.setPosition({c.cx - kSiteHalfW, kSiteTopY});
            t.draw(s);
        }

        // 발사대와 남은 요격탄 수
        for (const Site& b : bases_) {
            if (b.destroyed) continue;
            sf::Sprite s(as_.base);
            s.setPosition({b.cx - kSiteHalfW, kSiteTopY});
            t.draw(s);
            if (scene_ == Scene::Playing) {
                drawText(t, as_.font, std::to_string(b.ammo), 16, {b.cx, 520.f}, true);
            }
        }

        // 적 미사일
        for (const Missile& m : missiles_) {
            drawLine(t, m.start, m.head, kMissileTrail);
            sf::RectangleShape head({4.f, 4.f});
            head.setPosition({m.head.x - 2.f, m.head.y - 2.f});
            head.setFillColor(sf::Color::White);
            t.draw(head);
        }

        // 요격탄 궤적
        for (const Shot& s : shots_) {
            drawLine(t, s.from, s.pos, kInterTrail);
        }

        // 폭발
        for (const Blast& b : blasts_) {
            const float r = blastRadius(b.t);
            if (r <= 0.f) continue;
            sf::CircleShape c(r, 48);
            c.setOrigin({r, r});
            c.setPosition(b.center);
            const bool alt = (static_cast<int>(b.t / 0.1f) % 2) == 1;
            c.setFillColor(alt ? kBlastAlt : sf::Color::White);
            t.draw(c);
        }

        if (scene_ == Scene::Title) {
            drawText(t, as_.font, "SKY SHIELD", 48, {kW / 2.f, 200.f}, true);
            drawText(t, as_.font, "PRESS ENTER", 32, {kW / 2.f, 300.f}, true);
        } else if (scene_ == Scene::Playing) {
            sf::Sprite cross(as_.crosshair);
            cross.setPosition({mouse.x - 12.f, mouse.y - 12.f});
            t.draw(cross);

            drawText(t, as_.font, "SCORE " + std::to_string(score_), 16, {16.f, 12.f}, false);
            drawText(t, as_.font, "WAVE " + std::to_string(wave_), 16, {16.f, 36.f}, false);
            if (clearing_) {
                drawText(t, as_.font, "WAVE " + std::to_string(wave_) + " CLEAR", 32,
                         {kW / 2.f, 200.f}, true);
            }
        } else {
            drawText(t, as_.font, "GAME OVER", 48, {kW / 2.f, 170.f}, true);
            drawText(t, as_.font, "SCORE " + std::to_string(score_), 32, {kW / 2.f, 250.f}, true);
            drawText(t, as_.font, "WAVE " + std::to_string(wave_), 32, {kW / 2.f, 300.f}, true);
            drawText(t, as_.font, "R: RESTART  ESC: QUIT", 16, {kW / 2.f, 380.f}, true);
        }
    }

private:
    void resetWorld() {
        score_ = 0;
        wave_ = 1;
        cities_.clear();
        for (float x : kCityXs) cities_.push_back(Site{x, false, 0});
        bases_.clear();
        for (float x : kBaseXs) bases_.push_back(Site{x, false, kAmmoPerBase});
        missiles_.clear();
        shots_.clear();
        blasts_.clear();
        startWave(1);
    }

    void startWave(int n) {
        wave_ = n;
        spawned_ = 0;
        total_ = 10 + 2 * (n - 1);
        waveTimer_ = 0.f;
        clearing_ = false;
        clearTimer_ = 0.f;
        for (Site& b : bases_) {
            b.destroyed = false;
            b.ammo = kAmmoPerBase;
        }
    }

    void startPlay() {
        resetWorld();
        scene_ = Scene::Playing;
        as_.bgm.stop();
        as_.bgm.play();
    }

    bool anyCityAlive() const {
        return std::any_of(cities_.begin(), cities_.end(),
                           [](const Site& c) { return !c.destroyed; });
    }

    // 클릭 x와 가장 가까운, 파괴되지 않고 탄이 남은 발사대에서 발사 (같으면 왼쪽)
    void fire(sf::Vector2f target) {
        int best = -1;
        float bestDist = 0.f;
        for (std::size_t i = 0; i < bases_.size(); ++i) {
            const Site& b = bases_[i];
            if (b.destroyed || b.ammo <= 0) continue;
            const float d = std::abs(b.cx - target.x);
            if (best < 0 || d < bestDist) {
                best = static_cast<int>(i);
                bestDist = d;
            }
        }
        if (best < 0) return;

        Site& b = bases_[static_cast<std::size_t>(best)];
        --b.ammo;
        const sf::Vector2f origin{b.cx, kSiteTopY};
        shots_.push_back(Shot{origin, origin, target});
        playSfx(as_.launchSnd);
    }

    void spawnMissile() {
        struct Pick {
            bool isCity;
            std::size_t idx;
        };
        std::vector<Pick> picks;
        for (std::size_t i = 0; i < cities_.size(); ++i) {
            if (!cities_[i].destroyed) picks.push_back(Pick{true, i});
        }
        for (std::size_t i = 0; i < bases_.size(); ++i) {
            if (!bases_[i].destroyed) picks.push_back(Pick{false, i});
        }
        if (picks.empty()) return;

        std::uniform_int_distribution<std::size_t> pickDist(0, picks.size() - 1);
        const Pick p = picks[pickDist(rng_)];
        const float cx = p.isCity ? cities_[p.idx].cx : bases_[p.idx].cx;

        std::uniform_real_distribution<float> xDist(0.f, kW);
        Missile m;
        m.start = {xDist(rng_), 0.f};
        m.head = m.start;
        m.target = {cx, kSiteTopY};
        m.speed = 70.f + 10.f * static_cast<float>(wave_ - 1);
        m.isCity = p.isCity;
        m.index = p.idx;
        missiles_.push_back(m);
    }

    void explode(sf::Vector2f c) {
        blasts_.push_back(Blast{c, 0.f, false});
        playSfx(as_.explodeSnd);
    }

    void landOn(const Missile& m) {
        if (m.isCity) {
            Site& c = cities_[m.index];
            if (!c.destroyed) {
                c.destroyed = true;
                playSfx(as_.loseSnd);
            }
        } else {
            Site& b = bases_[m.index];
            if (!b.destroyed) {
                b.destroyed = true;
                playSfx(as_.loseSnd);
            }
        }
    }

    void updatePlay(float dt) {
        if (clearing_) {
            clearTimer_ -= dt;
            if (clearTimer_ <= 0.f) startWave(wave_ + 1);
            return;
        }

        if (anyCityAlive()) {
            waveTimer_ += dt;
            const float due = kFirstSpawn + kSpawnGap * static_cast<float>(spawned_);
            if (spawned_ < total_ && waveTimer_ >= due) {
                spawnMissile();
                ++spawned_;
            }
        }

        updateShots(dt);
        updateBlasts(dt);
        updateMissiles(dt);
        collideBlasts();

        const bool idle = missiles_.empty() && shots_.empty() && blasts_.empty();
        if (!anyCityAlive() && idle) {
            scene_ = Scene::Result;
            as_.bgm.pause();
            return;
        }
        if (idle && spawned_ >= total_ && anyCityAlive()) {
            int bonus = 0;
            for (const Site& b : bases_) {
                if (!b.destroyed) bonus += b.ammo * kAmmoBonus;
            }
            for (const Site& c : cities_) {
                if (!c.destroyed) bonus += kCityBonus;
            }
            score_ += bonus;
            clearing_ = true;
            clearTimer_ = kClearDelay;
        }
    }

    void updateShots(float dt) {
        for (Shot& s : shots_) {
            const sf::Vector2f d = s.to - s.pos;
            const float dist = d.length();
            const float step = kInterceptorSpeed * dt;
            if (dist <= step) {
                s.pos = s.to;
                s.dead = true;
                explode(s.to);
            } else {
                s.pos += d * (step / dist);
            }
        }
        std::erase_if(shots_, [](const Shot& s) { return s.dead; });
    }

    void updateBlasts(float dt) {
        for (Blast& b : blasts_) {
            b.t += dt;
            if (b.t >= kBlastLife) b.dead = true;
        }
        std::erase_if(blasts_, [](const Blast& b) { return b.dead; });
    }

    void updateMissiles(float dt) {
        for (Missile& m : missiles_) {
            const sf::Vector2f d = m.target - m.head;
            const float dist = d.length();
            const float step = m.speed * dt;
            if (dist <= step) {
                m.head = m.target;
                m.dead = true;
                explode(m.target);
                landOn(m);
            } else {
                m.head += d * (step / dist);
            }
        }
        std::erase_if(missiles_, [](const Missile& m) { return m.dead; });
    }

    // 폭발 원 안에 들어온 미사일 머리는 제거하고 25점, 그 자리에 연쇄 폭발
    void collideBlasts() {
        std::vector<sf::Vector2f> pending;
        for (Missile& m : missiles_) {
            for (const Blast& b : blasts_) {
                const float r = blastRadius(b.t);
                if (r <= 0.f) continue;
                const sf::Vector2f d = m.head - b.center;
                if (d.lengthSquared() < r * r) {
                    m.dead = true;
                    score_ += kKillScore;
                    pending.push_back(m.head);
                    break;
                }
            }
        }
        std::erase_if(missiles_, [](const Missile& m) { return m.dead; });
        for (const sf::Vector2f& p : pending) {
            explode(p);
        }
    }

    Assets& as_;
    Scene scene_ = Scene::Title;
    float titleTimer_ = 0.f;
    int score_ = 0;
    int wave_ = 1;
    std::vector<Site> cities_;
    std::vector<Site> bases_;
    std::vector<Missile> missiles_;
    std::vector<Shot> shots_;
    std::vector<Blast> blasts_;
    int spawned_ = 0;
    int total_ = 0;
    float waveTimer_ = 0.f;
    bool clearing_ = false;
    float clearTimer_ = 0.f;
    std::mt19937 rng_{std::random_device{}()};
};

} // namespace

int main() {
    Assets assets;
    if (!loadAssets(assets)) return 1;

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SKY SHIELD");
    window.setFramerateLimit(60);

    sf::View view(sf::FloatRect({0.f, 0.f}, {kW, kH}));
    updateView(view, window.getSize());

    Game game(assets);
    sf::Clock clock;
    float acc = 0.f;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }
            if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                updateView(view, resized->size);
                continue;
            }
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                    break;
                }
                game.onKey(key->code);
                continue;
            }
            if (const auto* click = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (click->button == sf::Mouse::Button::Left) {
                    game.onClick(window.mapPixelToCoords(click->position, view));
                }
            }
        }
        if (!window.isOpen()) break;

        acc += std::min(clock.restart().asSeconds(), kMaxFrameDelta);
        int steps = 0;
        while (acc >= kStep && steps < kMaxStepsPerFrame) {
            game.update(kStep);
            acc -= kStep;
            ++steps;
        }
        if (steps == kMaxStepsPerFrame) acc = 0.f;

        window.clear(sf::Color::Black);
        window.setView(view);
        game.draw(window, window.mapPixelToCoords(sf::Mouse::getPosition(window), view));
        window.display();
    }

    return 0;
}
