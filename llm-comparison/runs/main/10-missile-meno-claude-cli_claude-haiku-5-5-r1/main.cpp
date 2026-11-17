#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

// 명세의 수치. 화면은 960×540 논리 좌표를 쓰고 Letterbox로 맞춘다.
constexpr float kScreenW = 960.f;
constexpr float kGroundY = 500.f;          // 땅 띠의 윗변
constexpr float kGroundH = 40.f;
constexpr float kStructTopY = 476.f;       // 도시·발사대 윗면 중앙 y (아랫변 500 - 높이 24)
constexpr float kAimLimitY = 470.f;        // 이 y보다 아래를 클릭하면 요격하지 않는다
constexpr float kInterceptorSpeed = 600.f; // px/s
constexpr float kBlastLife = 1.f;          // 폭발 수명(초)
constexpr float kBlastMaxRadius = 48.f;
constexpr int kAmmoPerBase = 10;
constexpr int kMissileScore = 25;
constexpr int kAmmoScore = 5;
constexpr int kCityScore = 100;
constexpr double kTitleSeconds = 2.0;
constexpr double kFirstLaunchDelay = 1.0;  // 웨이브 시작 1초 뒤부터
constexpr double kLaunchInterval = 1.2;    // 1.2초마다 하나씩
constexpr double kClearSeconds = 3.0;

// 폭발 반지름: 0.5초 동안 0 → 48px, 다음 0.5초 동안 48px → 0.
float blastRadius(float age) {
    const float phase = std::min(age, kBlastLife - age);
    return std::max(0.f, phase) * 2.f * kBlastMaxRadius;
}

// pos를 target 쪽으로 step만큼 옮긴다. 도착하면 true를 돌려준다.
bool moveToward(meno::Vec2f& pos, meno::Vec2f target, float step) {
    const float dx = target.x - pos.x;
    const float dy = target.y - pos.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= step) {
        pos = target;
        return true;
    }
    pos.x += dx / dist * step;
    pos.y += dy / dist * step;
    return false;
}

// 글자를 그린다. centered가 true면 pos가 글자의 가운데이다.
void printText(meno::Renderer& r, const meno::Font& font, const std::string& text,
               meno::Vec2f pos, int size, bool centered) {
    meno::TextParams params;
    params.characterSize = static_cast<decltype(params.characterSize)>(size);
    params.color = meno::colors::White;
    if (centered) {
        params.origin = r.measureTextBounds(font, text, params).center();
    }
    r.drawText(font, text, pos, params);
}

// 로드한 에셋. 실패한 항목은 main에서 걸러낸다.
struct Assets {
    std::shared_ptr<const meno::Texture> city;
    std::shared_ptr<const meno::Texture> base;
    std::shared_ptr<const meno::Texture> crosshair;
    std::shared_ptr<const meno::Font> font;
    std::shared_ptr<const meno::SoundBuffer> launch;
    std::shared_ptr<const meno::SoundBuffer> explode;
    std::shared_ptr<const meno::SoundBuffer> lose;
};

// 같은 효과음을 겹쳐 내기 위해 Sound를 여러 개 두고 돌아가며 쓴다.
class SoundPool {
public:
    SoundPool(const std::shared_ptr<const meno::SoundBuffer>& buffer, std::size_t count) {
        sounds_.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            sounds_.emplace_back(buffer);
        }
    }

    void play() {
        sounds_[next_].play();
        next_ = (next_ + 1) % sounds_.size();
    }

private:
    std::vector<meno::Sound> sounds_;
    std::size_t next_ = 0;
};

// 도시(isBase = false) 6개와 발사대(isBase = true) 3개.
struct Site {
    float x;
    bool isBase;
    bool alive = true;
    int ammo = 0;
};

// 적 미사일: 출발점에서 목표까지 날아간다.
struct Missile {
    meno::Vec2f start;
    meno::Vec2f head;
    meno::Vec2f target;
    std::size_t site;
    float speed;
};

// 요격탄: 발사대 윗면 가운데에서 클릭 지점까지 날아간다.
struct Shot {
    meno::Vec2f from;
    meno::Vec2f head;
    meno::Vec2f to;
};

// 폭발: 생성 후 경과 시간(age)으로 반지름을 정한다.
struct Blast {
    meno::Vec2f center;
    float age = 0.f;
};

class Battle {
public:
    explicit Battle(const Assets& assets)
        : assets_(assets),
          launchSfx_(assets.launch, 4),
          explodeSfx_(assets.explode, 8),
          loseSfx_(assets.lose, 4),
          rng_(std::random_device{}()) {
        reset();
    }

    // 게임 상태를 모두 초기화하고 웨이브 1을 시작한다.
    void reset() {
        sites_.clear();
        for (const float x : {160.f, 260.f, 360.f, 600.f, 700.f, 800.f}) {
            sites_.push_back(Site{x, false, true, 0});
        }
        for (const float x : {60.f, 480.f, 900.f}) {
            sites_.push_back(Site{x, true, true, 0});
        }
        missiles_.clear();
        shots_.clear();
        blasts_.clear();
        score_ = 0;
        wave_ = 1;
        startWave();
    }

    int score() const { return score_; }
    int wave() const { return wave_; }

    // 한 번 갱신한다. 게임 오버가 되었으면 true를 돌려준다.
    bool update(double dt, bool clicked, meno::Vec2f click) {
        if (breaking_) {
            breakTimer_ -= dt;
            if (breakTimer_ <= 0.0) {
                ++wave_;
                startWave();
            }
            return false;
        }

        const auto step = static_cast<float>(dt);
        waveTime_ += dt;
        if (clicked) {
            fire(click);
        }
        moveShots(step);
        ageBlasts(step);
        launchMissile();
        moveMissiles(step);
        resolveHits();

        if (!anyCityAlive()) {
            return missiles_.empty() && shots_.empty() && blasts_.empty();
        }
        if (launched_ >= missileTotal() && missiles_.empty() && shots_.empty() && blasts_.empty()) {
            finishWave();
        }
        return false;
    }

    void draw(meno::Renderer& r, const meno::Font& font, meno::Vec2f aim) const {
        r.drawRect({{0.f, kGroundY}, {kScreenW, kGroundH}},
                   {.fill = meno::Color::fromRgb(0xB07A2A)});

        for (const Site& s : sites_) {
            if (!s.alive) {
                continue;
            }
            if (s.isBase) {
                r.draw(*assets_.base, {.position = {s.x, kGroundY}, .origin = {24.f, 24.f}});
                printText(r, font, std::to_string(s.ammo), {s.x, 512.f}, 16, true);
            } else {
                r.draw(*assets_.city, {.position = {s.x, kGroundY}, .origin = {24.f, 24.f}});
            }
        }

        for (const Shot& s : shots_) {
            r.drawLine(s.from, s.head, {.color = meno::Color::fromRgb(0x4FC3F7), .thickness = 2.f});
        }
        for (const Missile& m : missiles_) {
            r.drawLine(m.start, m.head, {.color = meno::Color::fromRgb(0xFF4D4D), .thickness = 2.f});
            r.drawRect({m.head - meno::Vec2f{2.f, 2.f}, {4.f, 4.f}}, {.fill = meno::colors::White});
        }
        for (const Blast& b : blasts_) {
            const float radius = blastRadius(b.age);
            if (radius <= 0.f) {
                continue;
            }
            const bool yellow = static_cast<int>(b.age * 10.f) % 2 == 1;
            const meno::Color inner = yellow ? meno::Color::fromRgb(0xFFD54A) : meno::colors::White;
            r.drawCircle(b.center, radius, {.fill = inner});
        }

        printText(r, font, "SCORE " + std::to_string(score_), {16.f, 16.f}, 32, false);
        printText(r, font, "WAVE " + std::to_string(wave_), {784.f, 16.f}, 32, false);
        if (breaking_) {
            printText(r, font, "WAVE " + std::to_string(wave_) + " CLEAR", {480.f, 240.f}, 48, true);
        }

        r.draw(*assets_.crosshair, {.position = aim, .origin = {12.f, 12.f}});
    }

private:
    int missileTotal() const { return 10 + 2 * (wave_ - 1); }

    bool anyCityAlive() const {
        return std::any_of(sites_.begin(), sites_.end(),
                           [](const Site& s) { return !s.isBase && s.alive; });
    }

    // 웨이브를 시작한다. 발사대는 모두 다시 쓸 수 있고 요격탄을 10발씩 채운다.
    void startWave() {
        breaking_ = false;
        waveTime_ = 0.0;
        launched_ = 0;
        for (Site& s : sites_) {
            if (s.isBase) {
                s.alive = true;
                s.ammo = kAmmoPerBase;
            }
        }
    }

    // 웨이브 정산: 남은 요격탄과 도시에 점수를 더하고 잠깐 쉰다.
    void finishWave() {
        int bonus = 0;
        for (const Site& s : sites_) {
            if (!s.alive) {
                continue;
            }
            if (s.isBase) {
                bonus += s.ammo * kAmmoScore;
            } else {
                bonus += kCityScore;
            }
        }
        score_ += bonus;
        breaking_ = true;
        breakTimer_ = kClearSeconds;
    }

    void spawnBlast(meno::Vec2f center) {
        blasts_.push_back(Blast{center, 0.f});
        explodeSfx_.play();
    }

    // 클릭 x와 가장 가까운, 탄이 남은 발사대에서 쏜다. 같은 거리면 왼쪽.
    void fire(meno::Vec2f click) {
        const std::size_t none = sites_.size();
        std::size_t best = none;
        for (std::size_t i = 0; i < sites_.size(); ++i) {
            const Site& s = sites_[i];
            if (!s.isBase || !s.alive || s.ammo <= 0) {
                continue;
            }
            if (best == none || std::abs(s.x - click.x) < std::abs(sites_[best].x - click.x)) {
                best = i;
            }
        }
        if (best == none) {
            return;
        }
        Site& s = sites_[best];
        --s.ammo;
        const meno::Vec2f from{s.x, kStructTopY};
        shots_.push_back(Shot{from, from, click});
        launchSfx_.play();
    }

    void moveShots(float dt) {
        for (auto it = shots_.begin(); it != shots_.end();) {
            if (moveToward(it->head, it->to, kInterceptorSpeed * dt)) {
                spawnBlast(it->to);
                it = shots_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ageBlasts(float dt) {
        for (Blast& b : blasts_) {
            b.age += dt;
        }
        std::erase_if(blasts_, [](const Blast& b) { return b.age >= kBlastLife; });
    }

    // 웨이브 시작 1초 뒤부터 1.2초마다 한 발씩 출발시킨다.
    void launchMissile() {
        if (launched_ >= missileTotal() || !anyCityAlive()) {
            return;
        }
        if (waveTime_ < kFirstLaunchDelay + kLaunchInterval * launched_) {
            return;
        }

        std::vector<std::size_t> alive;
        for (std::size_t i = 0; i < sites_.size(); ++i) {
            if (sites_[i].alive) {
                alive.push_back(i);
            }
        }
        std::uniform_int_distribution<std::size_t> pick(0, alive.size() - 1);
        const std::size_t siteIndex = alive[pick(rng_)];

        std::uniform_real_distribution<float> xDist(0.f, kScreenW);
        const meno::Vec2f start{xDist(rng_), 0.f};
        const meno::Vec2f impact{sites_[siteIndex].x, kStructTopY};
        const float speed = 70.f + 10.f * static_cast<float>(wave_ - 1);
        missiles_.push_back(Missile{start, start, impact, siteIndex, speed});
        ++launched_;
    }

    void moveMissiles(float dt) {
        for (auto it = missiles_.begin(); it != missiles_.end();) {
            if (!moveToward(it->head, it->target, it->speed * dt)) {
                ++it;
                continue;
            }
            const meno::Vec2f impact = it->target;
            Site& site = sites_[it->site];
            if (site.alive) {
                site.alive = false;
                loseSfx_.play();
            }
            it = missiles_.erase(it);
            spawnBlast(impact);
        }
    }

    bool isCovered(meno::Vec2f p) const {
        for (const Blast& b : blasts_) {
            const float r = blastRadius(b.age);
            const float dx = p.x - b.center.x;
            const float dy = p.y - b.center.y;
            if (r > 0.f && dx * dx + dy * dy <= r * r) {
                return true;
            }
        }
        return false;
    }

    // 폭발 원 안에 들어온 미사일은 한 번만 세고 없앤다. 그 자리에 연쇄 폭발이 생긴다.
    void resolveHits() {
        for (auto it = missiles_.begin(); it != missiles_.end();) {
            if (!isCovered(it->head)) {
                ++it;
                continue;
            }
            const meno::Vec2f head = it->head;
            it = missiles_.erase(it);
            score_ += kMissileScore;
            spawnBlast(head);
        }
    }

    const Assets& assets_;
    SoundPool launchSfx_;
    SoundPool explodeSfx_;
    SoundPool loseSfx_;
    std::mt19937 rng_;
    std::vector<Site> sites_;
    std::vector<Missile> missiles_;
    std::vector<Shot> shots_;
    std::vector<Blast> blasts_;
    int score_ = 0;
    int wave_ = 1;
    int launched_ = 0;
    double waveTime_ = 0.0;
    bool breaking_ = false;
    double breakTimer_ = 0.0;
};

} // namespace

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {960, 540},
                     .title = "SKY SHIELD",
                     .clearColor = meno::colors::Black}};

    // 월드 카메라: 960×540 논리 좌표를 그대로 쓰고, 남는 부분은 검은 띠로 채운다.
    const meno::Camera2D camera{.center = {480.f, 270.f}, .size = {960.f, 540.f}};
    game.renderer().setCamera(camera);

    // 에셋을 모두 읽는다. 하나라도 실패하면 경로를 알리고 종료한다.
    Assets assets;
    std::vector<std::string> missing;
    const auto require = [&missing](bool loaded, const char* path) {
        if (!loaded) {
            missing.emplace_back(path);
        }
    };

    assets.city = game.resources().loadTexture("assets/textures/md_city.png");
    require(assets.city != nullptr, "assets/textures/md_city.png");
    assets.base = game.resources().loadTexture("assets/textures/md_base.png");
    require(assets.base != nullptr, "assets/textures/md_base.png");
    assets.crosshair = game.resources().loadTexture("assets/textures/md_crosshair.png");
    require(assets.crosshair != nullptr, "assets/textures/md_crosshair.png");
    assets.font = game.resources().loadFont("assets/fonts/game.ttf");
    require(assets.font != nullptr, "assets/fonts/game.ttf");
    assets.launch = game.resources().loadSoundBuffer("assets/sfx/launch.wav");
    require(assets.launch != nullptr, "assets/sfx/launch.wav");
    assets.explode = game.resources().loadSoundBuffer("assets/sfx/explode.wav");
    require(assets.explode != nullptr, "assets/sfx/explode.wav");
    assets.lose = game.resources().loadSoundBuffer("assets/sfx/lose.wav");
    require(assets.lose != nullptr, "assets/sfx/lose.wav");
    auto bgm = meno::Music::openFromFile("assets/music/bgm_missile.ogg");
    require(bgm.has_value(), "assets/music/bgm_missile.ogg");

    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Failed to load: " << path << '\n';
        }
        return 1;
    }

    bgm->setLooping(true);
    bgm->setVolume(0.4f);
    const meno::Font& font = *assets.font;

    Battle battle{assets};
    meno::Vec2f aim{480.f, 270.f};
    double titleTimer = 0.0;

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();

    // 타이틀: Enter를 누르거나 2초가 지나면 플레이를 시작한다.
    title.setEnterHandler([&] { titleTimer = 0.0; });
    title.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        titleTimer += dt;
        if (input.wasPressed(meno::Key::Enter) || titleTimer >= kTitleSeconds) {
            (void)scenes.changeScene(play.id());
        }
    });
    title.setDrawHandler([&](meno::Renderer& r) {
        printText(r, font, "SKY SHIELD", {480.f, 200.f}, 48, true);
        printText(r, font, "PRESS ENTER", {480.f, 300.f}, 32, true);
    });

    // 플레이: 들어올 때마다 전체 상태를 초기화하고 배경음악을 튼다.
    play.setEnterHandler([&] {
        battle.reset();
        bgm->play();
    });
    play.setExitHandler([&] { bgm->stop(); });
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }
        aim = game.renderer().screenToWorld(meno::Vec2f{input.mousePosition()});
        const bool clicked = input.wasPressed(meno::MouseButton::Left) && aim.y < kAimLimitY;
        if (battle.update(dt, clicked, aim)) {
            (void)scenes.changeScene(result.id());
        }
    });
    play.setDrawHandler([&](meno::Renderer& r) {
        battle.draw(r, font, aim);
    });

    // 결과: R이면 다시 시작하고, Esc면 종료한다.
    result.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
        } else if (input.wasPressed(meno::Key::R)) {
            (void)scenes.changeScene(play.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& r) {
        printText(r, font, "GAME OVER", {480.f, 170.f}, 48, true);
        printText(r, font, "SCORE " + std::to_string(battle.score()), {480.f, 250.f}, 32, true);
        printText(r, font, "WAVE " + std::to_string(battle.wave()), {480.f, 300.f}, 32, true);
        printText(r, font, "R: RESTART  ESC: QUIT", {480.f, 420.f}, 16, true);
    });

    (void)scenes.changeScene(title.id());
    game.run();
}
