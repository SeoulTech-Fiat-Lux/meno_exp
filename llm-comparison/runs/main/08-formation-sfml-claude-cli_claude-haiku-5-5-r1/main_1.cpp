#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace {

constexpr float kDt = 1.f / 60.f;
constexpr float kScreenW = 960.f;
constexpr float kScreenH = 540.f;

constexpr float kPlayerY = 490.f;
constexpr float kPlayerW = 40.f;
constexpr float kPlayerH = 24.f;
constexpr float kPlayerSpeed = 300.f;
constexpr float kPlayerMinX = 16.f;
constexpr float kPlayerMaxX = 904.f; // 960 - 16 - 40

constexpr float kPBulletSpeed = 600.f;
constexpr float kPBulletW = 4.f;
constexpr float kPBulletH = 12.f;

constexpr float kEBulletSpeed = 240.f;
constexpr float kEBulletR = 3.f;
constexpr std::size_t kMaxEBullets = 3;

constexpr float kBonusSpeed = 120.f;
constexpr float kBonusY = 50.f;
constexpr float kBonusW = 48.f;
constexpr float kBonusH = 20.f;
constexpr float kBonusPeriod = 20.f;

constexpr float kBarrierTop = 400.f;
constexpr float kPieceSize = 8.f;

constexpr int kCols = 8;
constexpr int kRows = 4;
constexpr int kEnemyCount = kCols * kRows;
constexpr float kEnemyW = 32.f;
constexpr float kEnemyH = 24.f;
constexpr float kEnemyBaseX = 296.f;
constexpr float kEnemyBaseY = 80.f;
constexpr float kColStep = 48.f;
constexpr float kRowStep = 40.f;
constexpr float kEdgeLeft = 16.f;
constexpr float kEdgeRight = 944.f;
constexpr float kDescend = 16.f;
constexpr float kEnemyFrameTime = 0.5f;

constexpr float kFreezeTime = 1.f;
constexpr float kTitleTime = 2.f;

const sf::Color kBarrierColor(0x4C, 0xC2, 0x6B);
const sf::Color kEnemyBulletColor(0xFF, 0x6B, 0x6B);

sf::FloatRect makeRect(float x, float y, float w, float h) {
    return sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(w, h));
}

bool overlaps(const sf::FloatRect& a, const sf::FloatRect& b) {
    return a.position.x < b.position.x + b.size.x &&
           b.position.x < a.position.x + a.size.x &&
           a.position.y < b.position.y + b.size.y &&
           b.position.y < a.position.y + a.size.y;
}

bool circleHitsRect(sf::Vector2f c, float r, const sf::FloatRect& rc) {
    const float nx = std::clamp(c.x, rc.position.x, rc.position.x + rc.size.x);
    const float ny = std::clamp(c.y, rc.position.y, rc.position.y + rc.size.y);
    const float dx = c.x - nx;
    const float dy = c.y - ny;
    return dx * dx + dy * dy <= r * r;
}

bool fail(const char* path) {
    std::cerr << "asset load failed: " << path << '\n';
    return false;
}

void playSound(sf::Sound& s) {
    s.stop();
    s.play();
}

struct Assets {
    sf::Font font;
    sf::Texture enemies;
    sf::Texture player;
    sf::Texture bonus;
    sf::SoundBuffer shoot;
    sf::SoundBuffer hit;
    sf::SoundBuffer lose;
    sf::SoundBuffer bonusSfx;
    sf::Music bgm;

    bool load() {
        const char* p = "assets/fonts/game.ttf";
        if (!font.openFromFile(p)) return fail(p);

        p = "assets/textures/fm_enemies.png";
        if (!enemies.loadFromFile(p)) return fail(p);
        p = "assets/textures/fm_player.png";
        if (!player.loadFromFile(p)) return fail(p);
        p = "assets/textures/fm_bonus.png";
        if (!bonus.loadFromFile(p)) return fail(p);

        p = "assets/sfx/shoot.wav";
        if (!shoot.loadFromFile(p)) return fail(p);
        p = "assets/sfx/hit.wav";
        if (!hit.loadFromFile(p)) return fail(p);
        p = "assets/sfx/lose.wav";
        if (!lose.loadFromFile(p)) return fail(p);
        p = "assets/sfx/bonus.wav";
        if (!bonusSfx.loadFromFile(p)) return fail(p);

        p = "assets/music/bgm_formation.ogg";
        if (!bgm.openFromFile(p)) return fail(p);
        bgm.setLooping(true);
        bgm.setVolume(40.f);
        return true;
    }
};

enum class Screen { Title, Play, Result };

struct PBullet {
    float x; // 좌상단
    float y;
};

struct EBullet {
    sf::Vector2f pos; // 중심
    sf::Vector2f vel;
};

struct Piece {
    sf::FloatRect rect;
    bool alive;
};

struct Bonus {
    float x;
    float dir;
};

void drawText(sf::RenderWindow& w, const sf::Font& font, std::string_view s,
              unsigned int size, sf::Vector2f pos, bool centered) {
    sf::Text t(font, sf::String(s), size);
    t.setFillColor(sf::Color::White);
    if (centered) {
        const sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f));
    }
    t.setPosition(pos);
    w.draw(t);
}

class Game {
public:
    explicit Game(Assets& a)
        : assets_(a),
          shootSnd_(a.shoot),
          hitSnd_(a.hit),
          loseSnd_(a.lose),
          bonusSnd_(a.bonusSfx),
          rng_(std::random_device{}()) {
        resetState();
    }

    void onKey(sf::Keyboard::Key k) {
        if (k == sf::Keyboard::Key::Space) spacePend_ = true;
        else if (k == sf::Keyboard::Key::Enter) enterPend_ = true;
        else if (k == sf::Keyboard::Key::R) rPend_ = true;
    }

    void update() {
        switch (scr_) {
            case Screen::Title:  updateTitle(); break;
            case Screen::Play:   updatePlay(); break;
            case Screen::Result: updateResult(); break;
        }
    }

    void draw(sf::RenderWindow& w) const {
        w.clear(sf::Color::Black);
        switch (scr_) {
            case Screen::Title:  drawTitle(w); break;
            case Screen::Play:   drawPlay(w); break;
            case Screen::Result: drawResult(w); break;
        }
    }

private:
    // ---------- 상태 전환 ----------
    void resetState() {
        score_ = 0;
        lives_ = 3;
        wave_ = 1;
        playTime_ = 0.f;
        nextBonus_ = kBonusPeriod;
        bonusCount_ = 0;
        freeze_ = 0.f;
        px_ = 460.f;
        animTimer_ = 0.f;
        animToggle_ = false;
        setupWave();
    }

    void startPlay() {
        resetState();
        scr_ = Screen::Play;
        spacePend_ = false;
        enterPend_ = false;
        rPend_ = false;
        assets_.bgm.stop();
        assets_.bgm.play();
    }

    void enterResult() {
        scr_ = Screen::Result;
        assets_.bgm.stop();
    }

    void setupWave() {
        const float dy = std::min(kDescend * static_cast<float>(wave_ - 1), 64.f);
        ox_ = 0.f;
        oy_ = dy;
        dir_ = 1;
        alive_.fill(true);
        aliveCount_ = kEnemyCount;
        waveTime_ = 0.f;
        nextShot_ = 1.f;
        pb_.reset();
        eb_.clear();
        bonus_.clear();

        pieces_.clear();
        for (int b = 0; b < 4; ++b) {
            const float left = 168.f + 192.f * static_cast<float>(b); // 중심 192+192b, 폭 48
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 6; ++c) {
                    pieces_.push_back({makeRect(left + kPieceSize * c,
                                                kBarrierTop + kPieceSize * r,
                                                kPieceSize, kPieceSize),
                                       true});
                }
            }
        }
    }

    // ---------- 화면별 갱신 ----------
    void updateTitle() {
        spacePend_ = false;
        titleTime_ += kDt;
        if (enterPend_ || titleTime_ >= kTitleTime) {
            titleTime_ = 0.f;
            startPlay();
        }
        enterPend_ = false;
    }

    void updateResult() {
        enterPend_ = false;
        spacePend_ = false;
        if (rPend_) {
            rPend_ = false;
            startPlay();
        }
    }

    void updatePlay() {
        enterPend_ = false;
        rPend_ = false;

        // 피격 후 정지 중에는 모든 것을 멈춘다. 발사 입력은 정지가 끝날 때까지 보관한다.
        if (freeze_ > 0.f) {
            freeze_ -= kDt;
            return;
        }

        playTime_ += kDt;
        if (playTime_ >= nextBonus_) {
            spawnBonus();
            nextBonus_ += kBonusPeriod;
        }

        updatePlayer();
        updateFormation();
        updateEnemyFire();
        moveEnemyBullets();
        moveBonuses();
        resolvePlayerBullet();

        if (resolveEnemyBullets()) {
            onPlayerHit();
            return;
        }

        barriersEatEnemies();
        if (enemyReachedBottom()) {
            enterResult();
            return;
        }

        if (aliveCount_ == 0) {
            ++wave_;
            setupWave();
        }
    }

    void updatePlayer() {
        const bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
                          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
        const bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
                           sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
        float dx = 0.f;
        if (left) dx -= 1.f;
        if (right) dx += 1.f;
        px_ = std::clamp(px_ + dx * kPlayerSpeed * kDt, kPlayerMinX, kPlayerMaxX);

        if (spacePend_) {
            spacePend_ = false;
            if (!pb_) {
                pb_ = PBullet{px_ + 18.f, kPlayerY - 12.f}; // 가로 중앙, 포대 위
                playSound(shootSnd_);
            }
        }
    }

    float enemyX(int c) const {
        return kEnemyBaseX + kColStep * static_cast<float>(c) + ox_;
    }

    float enemyY(int r) const {
        return kEnemyBaseY + kRowStep * static_cast<float>(r) + oy_;
    }

    sf::FloatRect enemyRect(int i) const {
        return makeRect(enemyX(i % kCols), enemyY(i / kCols), kEnemyW, kEnemyH);
    }

    sf::FloatRect playerRect() const {
        return makeRect(px_, kPlayerY, kPlayerW, kPlayerH);
    }

    static sf::FloatRect bonusRect(const Bonus& b) {
        return makeRect(b.x, kBonusY, kBonusW, kBonusH);
    }

    void updateFormation() {
        if (aliveCount_ == 0) return;

        animTimer_ += kDt;
        if (animTimer_ >= kEnemyFrameTime) {
            animTimer_ -= kEnemyFrameTime;
            animToggle_ = !animToggle_;
        }

        const float speed = 40.f + 4.f * static_cast<float>(kEnemyCount - aliveCount_);
        ox_ += static_cast<float>(dir_) * speed * kDt;

        float minL = 1e9f;
        float maxR = -1e9f;
        for (int i = 0; i < kEnemyCount; ++i) {
            if (!alive_[i]) continue;
            const float x = enemyX(i % kCols);
            minL = std::min(minL, x);
            maxR = std::max(maxR, x + kEnemyW);
        }

        if (dir_ > 0 && maxR >= kEdgeRight) {
            oy_ += kDescend;
            dir_ = -1;
        } else if (dir_ < 0 && minL <= kEdgeLeft) {
            oy_ += kDescend;
            dir_ = 1;
        }
    }

    void updateEnemyFire() {
        waveTime_ += kDt;
        if (waveTime_ >= nextShot_) {
            nextShot_ += 1.f;
            fireEnemy();
        }
    }

    void fireEnemy() {
        if (eb_.size() >= kMaxEBullets) return;

        const float pcx = px_ + kPlayerW / 2.f;
        int bestC = -1;
        float bestD = 1e9f;
        for (int c = 0; c < kCols; ++c) {
            bool any = false;
            for (int r = 0; r < kRows; ++r) {
                if (alive_[r * kCols + c]) {
                    any = true;
                    break;
                }
            }
            if (!any) continue;
            const float d = std::abs(enemyX(c) + kEnemyW / 2.f - pcx);
            if (d < bestD) { // 같으면 먼저 나온(왼쪽) 열 유지
                bestD = d;
                bestC = c;
            }
        }
        if (bestC < 0) return;

        int bestR = -1;
        for (int r = kRows - 1; r >= 0; --r) {
            if (alive_[r * kCols + bestC]) {
                bestR = r;
                break;
            }
        }

        const sf::Vector2f from(enemyX(bestC) + kEnemyW / 2.f, enemyY(bestR) + kEnemyH);
        const sf::Vector2f to(px_ + kPlayerW / 2.f, kPlayerY + kPlayerH / 2.f);
        const sf::Vector2f d = to - from;
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len <= 0.f) return;

        eb_.push_back(EBullet{from, d * (kEBulletSpeed / len)});
    }

    void moveEnemyBullets() {
        for (auto& b : eb_) {
            b.pos = b.pos + b.vel * kDt;
        }
        std::erase_if(eb_, [](const EBullet& b) {
            return b.pos.y > kScreenH + 10.f || b.pos.x < -10.f || b.pos.x > kScreenW + 10.f;
        });
    }

    void spawnBonus() {
        if (bonusCount_ % 2 == 0) bonus_.push_back(Bonus{-kBonusW, 1.f});
        else bonus_.push_back(Bonus{kScreenW, -1.f});
        ++bonusCount_;
    }

    void moveBonuses() {
        for (auto& b : bonus_) {
            b.x += b.dir * kBonusSpeed * kDt;
        }
        std::erase_if(bonus_, [](const Bonus& b) {
            return b.x > kScreenW || b.x < -kBonusW;
        });
    }

    // 플레이어 총알: 틱당 이동 거리를 포함한 스윕 사각형으로 판정해 조각을 건너뛰지 않는다.
    void resolvePlayerBullet() {
        if (!pb_) return;

        const float oldTop = pb_->y;
        const float newTop = oldTop - kPBulletSpeed * kDt;
        const float oldBottom = oldTop + kPBulletH;
        pb_->y = newTop;

        if (newTop + kPBulletH < 0.f) {
            pb_.reset();
            return;
        }

        const sf::FloatRect sweep = makeRect(pb_->x, newTop, kPBulletW, oldBottom - newTop);

        for (auto& p : pieces_) {
            if (p.alive && overlaps(sweep, p.rect)) {
                p.alive = false;
                pb_.reset();
                return;
            }
        }

        for (int i = 0; i < kEnemyCount; ++i) {
            if (alive_[i] && overlaps(sweep, enemyRect(i))) {
                const int row = i / kCols;
                alive_[i] = false;
                --aliveCount_;
                score_ += pointsFor(row);
                pb_.reset();
                playSound(hitSnd_);
                return;
            }
        }

        for (std::size_t i = 0; i < bonus_.size(); ++i) {
            if (overlaps(sweep, bonusRect(bonus_[i]))) {
                std::uniform_int_distribution<int> pick(0, 2);
                score_ += 50 * (pick(rng_) + 1);
                bonus_.erase(bonus_.begin() + static_cast<std::ptrdiff_t>(i));
                pb_.reset();
                playSound(bonusSnd_);
                return;
            }
        }
    }

    static int pointsFor(int row) {
        if (row == 0) return 30;
        if (row == 1) return 20;
        return 10;
    }

    // 적 탄 처리. 플레이어에 맞았으면 true.
    bool resolveEnemyBullets() {
        for (auto it = eb_.begin(); it != eb_.end();) {
            bool removed = false;
            for (auto& p : pieces_) {
                if (p.alive && circleHitsRect(it->pos, kEBulletR, p.rect)) {
                    p.alive = false;
                    removed = true;
                    break;
                }
            }
            if (removed) {
                it = eb_.erase(it);
                continue;
            }
            if (circleHitsRect(it->pos, kEBulletR, playerRect())) {
                return true;
            }
            ++it;
        }
        return false;
    }

    void onPlayerHit() {
        eb_.clear();
        pb_.reset();
        --lives_;
        playSound(loseSnd_);
        if (lives_ <= 0) {
            enterResult();
            return;
        }
        freeze_ = kFreezeTime;
    }

    void barriersEatEnemies() {
        for (int i = 0; i < kEnemyCount; ++i) {
            if (!alive_[i]) continue;
            const sf::FloatRect er = enemyRect(i);
            for (auto& p : pieces_) {
                if (p.alive && overlaps(er, p.rect)) {
                    p.alive = false;
                }
            }
        }
    }

    bool enemyReachedBottom() const {
        for (int i = 0; i < kEnemyCount; ++i) {
            if (alive_[i] && enemyY(i / kCols) >= kPlayerY) return true;
        }
        return false;
    }

    // ---------- 그리기 ----------
    void drawTitle(sf::RenderWindow& w) const {
        drawText(w, assets_.font, "STAR FORMATION", 48, {480.f, 200.f}, true);
        drawText(w, assets_.font, "PRESS ENTER", 16, {480.f, 300.f}, true);
    }

    void drawResult(sf::RenderWindow& w) const {
        drawText(w, assets_.font, "GAME OVER", 48, {480.f, 170.f}, true);
        drawText(w, assets_.font, std::format("SCORE {}", score_), 32, {480.f, 260.f}, true);
        drawText(w, assets_.font, std::format("WAVE {}", wave_), 32, {480.f, 320.f}, true);
        drawText(w, assets_.font, "R: RESTART  ESC: QUIT", 16, {480.f, 430.f}, true);
    }

    void drawPlay(sf::RenderWindow& w) const {
        // 그리는 순서: 방어벽 → 적 → 보너스 비행체 → 플레이어 → 탄 → 점수
        for (const auto& p : pieces_) {
            if (!p.alive) continue;
            sf::RectangleShape s(sf::Vector2f(kPieceSize, kPieceSize));
            s.setFillColor(kBarrierColor);
            s.setPosition(p.rect.position);
            w.draw(s);
        }

        for (int i = 0; i < kEnemyCount; ++i) {
            if (!alive_[i]) continue;
            const int row = i / kCols;
            const int base = (row == 0) ? 0 : (row == 1) ? 2 : 4;
            const int frame = base + (animToggle_ ? 1 : 0);
            sf::Sprite s(assets_.enemies);
            s.setTextureRect(sf::IntRect({frame * 32, 0}, {32, 24}));
            s.setPosition(sf::Vector2f(enemyX(i % kCols), enemyY(row)));
            w.draw(s);
        }

        for (const auto& b : bonus_) {
            sf::Sprite s(assets_.bonus);
            s.setPosition(sf::Vector2f(b.x, kBonusY));
            w.draw(s);
        }

        {
            sf::Sprite s(assets_.player);
            s.setPosition(sf::Vector2f(px_, kPlayerY));
            w.draw(s);
        }

        if (pb_) {
            sf::RectangleShape s(sf::Vector2f(kPBulletW, kPBulletH));
            s.setFillColor(sf::Color::White);
            s.setPosition(sf::Vector2f(pb_->x, pb_->y));
            w.draw(s);
        }

        for (const auto& b : eb_) {
            sf::CircleShape c(kEBulletR);
            c.setOrigin(sf::Vector2f(kEBulletR, kEBulletR));
            c.setFillColor(kEnemyBulletColor);
            c.setPosition(b.pos);
            w.draw(c);
        }

        drawText(w, assets_.font, std::format("SCORE {}", score_), 16, {16.f, 12.f}, false);
        drawText(w, assets_.font, std::format("WAVE {}", wave_), 16, {400.f, 12.f}, false);
        drawText(w, assets_.font, std::format("LIVES {}", lives_), 16, {760.f, 12.f}, false);
    }

    Assets& assets_;
    sf::Sound shootSnd_;
    sf::Sound hitSnd_;
    sf::Sound loseSnd_;
    sf::Sound bonusSnd_;
    std::mt19937 rng_;

    Screen scr_ = Screen::Title;
    float titleTime_ = 0.f;
    bool spacePend_ = false;
    bool enterPend_ = false;
    bool rPend_ = false;

    int score_ = 0;
    int lives_ = 3;
    int wave_ = 1;
    float playTime_ = 0.f;
    float nextBonus_ = kBonusPeriod;
    int bonusCount_ = 0;
    float freeze_ = 0.f;

    float px_ = 460.f;

    float ox_ = 0.f;
    float oy_ = 0.f;
    int dir_ = 1;
    std::array<bool, kEnemyCount> alive_{};
    int aliveCount_ = kEnemyCount;
    float waveTime_ = 0.f;
    float nextShot_ = 1.f;
    float animTimer_ = 0.f;
    bool animToggle_ = false;

    std::optional<PBullet> pb_;
    std::vector<EBullet> eb_;
    std::vector<Bonus> bonus_;
    std::vector<Piece> pieces_;
};

} // namespace

int main() {
    Assets assets;
    if (!assets.load()) {
        return 1;
    }

    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "STAR FORMATION");
    window.setFramerateLimit(60);

    Game game(assets);
    sf::Clock clock;
    float accumulator = 0.f;

    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    window.close();
                } else {
                    game.onKey(key->code);
                }
            }
        }
        if (!window.isOpen()) {
            break;
        }

        // 고정 시간 간격(1/60초)으로 갱신하므로 프레임레이트와 무관하게 진행 속도가 같다.
        accumulator += clock.restart().asSeconds();
        if (accumulator > 0.25f) {
            accumulator = 0.25f;
        }
        while (accumulator >= kDt && window.isOpen()) {
            game.update();
            accumulator -= kDt;
        }

        game.draw(window);
        window.display();
    }

    return 0;
}
