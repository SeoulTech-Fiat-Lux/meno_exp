```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using meno::Vec2f;
using meno::Color;

constexpr float WorldWidth = 2880.f;
constexpr float WorldHeight = 1620.f;
constexpr float ViewWidth = 960.f;
constexpr float ViewHeight = 540.f;
constexpr float PlayerRadius = 14.f;

const Color Background = Color::fromRgb(0x1A1420);
const Color Grid = Color::fromRgb(0x2A2233);
const Color Border = Color::fromRgb(0x6B5B7B);
const Color Blue = Color::fromRgb(0x4FC3F7);
const Color Gold = Color::fromRgb(0xFFD54A);
const Color HpBackground = Color::fromRgb(0x3A1F24);
const Color HpFill = Color::fromRgb(0xE5533D);
const Color Hurt = Color::fromRgb(0xFF6060);

float lengthSquared(Vec2f v) {
    return v.x * v.x + v.y * v.y;
}

Vec2f normalized(Vec2f v) {
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.f ? v * (1.f / length) : Vec2f{0.f, 0.f};
}

void clampToWorld(Vec2f& p, float radius) {
    p.x = std::clamp(p.x, radius, WorldWidth - radius);
    p.y = std::clamp(p.y, radius, WorldHeight - radius);
}

bool overlaps(Vec2f a, float ar, Vec2f b, float br) {
    const float radius = ar + br;
    return lengthSquared(a - b) <= radius * radius;
}

// Returns the first intersection along a bullet's movement segment.
float segmentHit(Vec2f start, Vec2f end, Vec2f center, float radius) {
    const Vec2f offset = start - center;
    const float c = lengthSquared(offset) - radius * radius;
    if (c <= 0.f) {
        return 0.f;
    }

    const Vec2f movement = end - start;
    const float a = lengthSquared(movement);
    if (a <= 0.f) {
        return std::numeric_limits<float>::infinity();
    }

    const float b = offset.x * movement.x + offset.y * movement.y;
    const float discriminant = b * b - a * c;
    if (discriminant < 0.f) {
        return std::numeric_limits<float>::infinity();
    }

    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.f && t <= 1.f
               ? t
               : std::numeric_limits<float>::infinity();
}

std::string timeText(double time) {
    const int seconds = static_cast<int>(std::max(0.0, time) + 0.000001);
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << seconds / 60
           << ':' << std::setw(2) << seconds % 60;
    return stream.str();
}

enum class Screen { Title, Play, Result };

struct EnemyType {
    float radius;
    float hp;
    float speed;
    float contactDamage;
    float frameDuration;
    int cellSize;
    int experience;
};

constexpr std::array<EnemyType, 3> EnemyTypes{{
    {10.f, 10.f, 80.f, 10.f, 0.3f, 24, 1},
    {10.f, 6.f, 140.f, 6.f, 0.15f, 24, 1},
    {20.f, 60.f, 50.f, 20.f, 0.4f, 48, 5},
}};

struct Enemy {
    Vec2f position{};
    Vec2f knockDirection{};
    int type = 0;
    float hp = 10.f;
    float hitTime = 0.f;
    float knockTime = 0.f;
    double deathTime = 0.0;
};

struct Bullet {
    Vec2f position{};
    Vec2f velocity{};
    float damage = 10.f;
    double life = 1.2;
    bool removed = false;
};

struct Gem {
    Vec2f position{};
    int experience = 1;
    double life = 30.0;
    bool removed = false;
};

struct PlayState {
    Vec2f player{WorldWidth * 0.5f, WorldHeight * 0.5f};
    float hp = 100.f;
    float maxHp = 100.f;
    float speed = 200.f;
    float damage = 10.f;
    float magnet = 60.f;
    double fireInterval = 0.8;
    double fireTimer = 0.8;
    int projectiles = 1;
    int level = 1;
    int experience = 0;
    int kills = 0;

    double elapsed = 0.0;
    double nextSpawn = 0.5;
    double walkTime = 0.0;
    bool moving = false;
    bool flip = false;
    bool takingDamage = false;
    bool paused = false;
    bool selecting = false;
    bool survived = false;

    std::array<int, 3> cards{};
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Gem> gems;
};

constexpr std::array<const char*, 6> UpgradeNames{{
    "FIRE RATE +15%",
    "DAMAGE +5",
    "PROJECTILE +1",
    "SPEED +10%",
    "MAX HP +20",
    "MAGNET +40",
}};

const std::array<meno::Rectf, 3> CardRects{{
    {{80.f, 200.f}, {240.f, 140.f}},
    {{360.f, 200.f}, {240.f, 140.f}},
    {{640.f, 200.f}, {240.f, 140.f}},
}};

int runGame() {
    meno::Game game{{
        .application = {.framerateLimit = 60},
        .windowSize = {960, 540},
        .title = "LAST STAND",
        .clearColor = meno::colors::Black,
    }};

    auto& resources = game.resources();
    bool assetsValid = true;
    const auto require = [&](bool valid, const char* path) {
        if (!valid) {
            std::cerr << "Failed to load: " << path << '\n';
            assetsValid = false;
        }
    };

    const char* fontPath = "assets/fonts/game.ttf";
    const auto font = resources.loadFont(fontPath);
    require(static_cast<bool>(font), fontPath);

    constexpr std::array<const char*, 6> texturePaths{{
        "assets/textures/sv_player.png",
        "assets/textures/sv_walker.png",
        "assets/textures/sv_runner.png",
        "assets/textures/sv_brute.png",
        "assets/textures/sv_bullet.png",
        "assets/textures/sv_gem.png",
    }};
    std::array<std::shared_ptr<const meno::Texture>, 6> textures;
    for (std::size_t i = 0; i < textures.size(); ++i) {
        textures[i] = resources.loadTexture(texturePaths[i]);
        require(static_cast<bool>(textures[i]), texturePaths[i]);
    }

    constexpr std::array<const char*, 4> soundPaths{{
        "assets/sfx/shoot.wav",
        "assets/sfx/hit.wav",
        "assets/sfx/pickup.wav",
        "assets/sfx/levelup.wav",
    }};
    std::array<std::vector<meno::Sound>, 4> sounds;
    std::array<std::size_t, 4> soundCursor{};
    for (std::size_t i = 0; i < sounds.size(); ++i) {
        const auto buffer = resources.loadSoundBuffer(soundPaths[i]);
        require(static_cast<bool>(buffer), soundPaths[i]);
        if (buffer) {
            sounds[i].reserve(16);
            for (int j = 0; j < 16; ++j) {
                sounds[i].emplace_back(buffer);
            }
        }
    }

    const char* musicPath = "assets/music/bgm_survivor.ogg";
    auto music = meno::Music::openFromFile(musicPath);
    require(static_cast<bool>(music), musicPath);

    if (!assetsValid) {
        return 1;
    }

    music->setVolume(0.4f);
    music->setLooping(true);

    const auto playSound = [&](std::size_t index) {
        auto& pool = sounds[index];
        pool[soundCursor[index]].play();
        soundCursor[index] = (soundCursor[index] + 1) % pool.size();
    };

    const auto stopEffects = [&] {
        for (auto& pool : sounds) {
            for (auto& sound : pool) {
                sound.stop();
            }
        }
        soundCursor.fill(0);
    };

    Screen screen = Screen::Title;
    double titleTime = 0.0;
    PlayState state;
    std::mt19937 random{std::random_device{}()};

    const meno::Camera2D hud{
        .center = {ViewWidth * 0.5f, ViewHeight * 0.5f},
        .size = {ViewWidth, ViewHeight},
    };
    meno::Camera2D camera = hud;

    const auto updateCamera = [&] {
        camera.center = {
            std::clamp(state.player.x, ViewWidth * 0.5f,
                       WorldWidth - ViewWidth * 0.5f),
            std::clamp(state.player.y, ViewHeight * 0.5f,
                       WorldHeight - ViewHeight * 0.5f),
        };
        game.renderer().setCamera(camera);
    };

    const auto startPlay = [&] {
        stopEffects();
        music->stop();
        state = PlayState{};
        state.enemies.reserve(400);
        state.bullets.reserve(128);
        state.gems.reserve(512);
        screen = Screen::Play;
        updateCamera();
        music->play();
    };

    const auto finishPlay = [&](bool survived) {
        state.survived = survived;
        state.paused = false;
        state.selecting = false;
        screen = Screen::Result;
        music->stop();
    };

    // Only one level is presented at a time. Excess experience is retained.
    const auto beginLevelUp = [&] {
        if (state.experience < 5 * state.level) {
            return false;
        }
        state.experience -= 5 * state.level;
        ++state.level;
        std::array<int, 6> choices{0, 1, 2, 3, 4, 5};
        std::shuffle(choices.begin(), choices.end(), random);
        std::copy_n(choices.begin(), 3, state.cards.begin());
        state.selecting = true;
        music->pause();
        playSound(3);
        return true;
    };

    const auto chooseUpgrade = [&](int index) {
        switch (state.cards[static_cast<std::size_t>(index)]) {
        case 0:
            state.fireInterval *= 0.85;
            state.fireTimer = std::min(state.fireTimer, state.fireInterval);
            break;
        case 1:
            state.damage += 5.f;
            break;
        case 2:
            ++state.projectiles;
            break;
        case 3:
            state.speed *= 1.1f;
            break;
        case 4:
            state.maxHp += 20.f;
            state.hp = std::min(state.maxHp, state.hp + 20.f);
            break;
        case 5:
            state.magnet += 40.f;
            break;
        }
        state.selecting = false;
        if (!beginLevelUp()) {
            music->play();
        }
    };

    const auto spawnEnemy = [&] {
        if (state.enemies.size() >= 400) {
            return;
        }

        const float roll = std::uniform_real_distribution<float>{0.f, 1.f}(random);
        int type = 0;
        if (state.elapsed >= 120.0) {
            type = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
        } else if (state.elapsed >= 60.0) {
            type = roll < 0.7f ? 0 : 1;
        }

        const float radius = EnemyTypes[type].radius;
        const float left = camera.center.x - ViewWidth * 0.5f - 64.f;
        const float right = camera.center.x + ViewWidth * 0.5f + 64.f;
        const float top = camera.center.y - ViewHeight * 0.5f - 64.f;
        const float bottom = camera.center.y + ViewHeight * 0.5f + 64.f;

        struct Segment {
            Vec2f start;
            Vec2f end;
            float length;
        };
        std::array<Segment, 4> segments{};
        std::size_t count = 0;
        float total = 0.f;

        const auto addHorizontal = [&](float y) {
            if (y < radius || y > WorldHeight - radius) {
                return;
            }
            const float a = std::max(left, radius);
            const float b = std::min(right, WorldWidth - radius);
            if (b > a) {
                segments[count++] = {{a, y}, {b, y}, b - a};
                total += b - a;
            }
        };
        const auto addVertical = [&](float x) {
            if (x < radius || x > WorldWidth - radius) {
                return;
            }
            const float a = std::max(top, radius);
            const float b = std::min(bottom, WorldHeight - radius);
            if (b > a) {
                segments[count++] = {{x, a}, {x, b}, b - a};
                total += b - a;
            }
        };
        addHorizontal(top);
        addHorizontal(bottom);
        addVertical(left);
        addVertical(right);

        if (count == 0 || total <= 0.f) {
            return;
        }

        float distance =
            std::uniform_real_distribution<float>{0.f, total}(random);
        Vec2f position = segments[count - 1].end;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& segment = segments[i];
            if (distance <= segment.length) {
                position = segment.start +
                           (segment.end - segment.start) *
                               (distance / segment.length);
                break;
            }
            distance -= segment.length;
        }

        Enemy enemy;
        enemy.position = position;
        enemy.type = type;
        enemy.hp = EnemyTypes[type].hp;
        state.enemies.push_back(enemy);
    };

    const auto fire = [&] {
        Vec2f target{};
        float closest = 400.f * 400.f;
        bool found = false;
        for (const Enemy& enemy : state.enemies) {
            if (enemy.hp <= 0.f) {
                continue;
            }
            const float distance = lengthSquared(enemy.position - state.player);
            if (distance <= closest) {
                closest = distance;
                target = enemy.position;
                found = true;
            }
        }
        if (!found) {
            return;
        }

        Vec2f direction = normalized(target - state.player);
        if (lengthSquared(direction) == 0.f) {
            direction = {1.f, 0.f};
        }
        const float middle = static_cast<float>(state.projectiles - 1) * 0.5f;
        for (int i = 0; i < state.projectiles; ++i) {
            const float angle =
                (static_cast<float>(i) - middle) * 10.f *
                std::numbers::pi_v<float> / 180.f;
            const float cosine = std::cos(angle);
            const float sine = std::sin(angle);
            const Vec2f spread{
                direction.x * cosine - direction.y * sine,
                direction.x * sine + direction.y * cosine,
            };
            state.bullets.push_back(
                {state.player, spread * 500.f, state.damage, 1.2, false});
        }
        playSound(0);
    };

    meno::Scene& scene = game.scenes().createScene();

    scene.setUpdateHandler([&](double delta) {
        const auto& input = game.window().input();
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
            return;
        }

        if (screen == Screen::Title) {
            titleTime += delta;
            if (input.wasPressed(meno::Key::Enter) || titleTime >= 2.0) {
                startPlay();
            }
            return;
        }

        if (screen == Screen::Result) {
            if (input.wasPressed(meno::Key::R)) {
                startPlay();
            }
            return;
        }

        if (state.selecting) {
            int choice = -1;
            if (input.wasPressed(meno::Key::Num1)) {
                choice = 0;
            } else if (input.wasPressed(meno::Key::Num2)) {
                choice = 1;
            } else if (input.wasPressed(meno::Key::Num3)) {
                choice = 2;
            }

            if (choice < 0 &&
                input.wasPressed(meno::MouseButton::Left)) {
                const Vec2f mouse{input.mousePosition()};
                const auto framebuffer = game.renderer().framebufferSize();
                if (meno::viewportPixels(hud, framebuffer).contains(mouse)) {
                    const Vec2f point =
                        meno::screenToWorld(hud, framebuffer, mouse);
                    for (int i = 0; i < 3; ++i) {
                        if (CardRects[i].contains(point)) {
                            choice = i;
                            break;
                        }
                    }
                }
            }

            if (choice >= 0) {
                chooseUpgrade(choice);
            }
            return;
        }

        if (input.wasPressed(meno::Key::P)) {
            state.paused = !state.paused;
            if (state.paused) {
                music->pause();
            } else {
                music->play();
            }
            return;
        }
        if (state.paused) {
            return;
        }

        // The final simulation step never advances beyond five minutes.
        delta = std::min(delta, 300.0 - state.elapsed);
        const float dt = static_cast<float>(delta);
        state.elapsed = std::min(300.0, state.elapsed + delta);

        Vec2f movement{0.f, 0.f};
        if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left)) {
            movement.x -= 1.f;
        }
        if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right)) {
            movement.x += 1.f;
        }
        if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up)) {
            movement.y -= 1.f;
        }
        if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down)) {
            movement.y += 1.f;
        }

        if (movement.x != 0.f) {
            state.flip = movement.x < 0.f;
        }
        const Vec2f oldPlayer = state.player;
        state.player += normalized(movement) * (state.speed * dt);
        clampToWorld(state.player, PlayerRadius);
        state.moving = lengthSquared(state.player - oldPlayer) > 0.f;
        if (state.moving) {
            state.walkTime += delta;
        } else {
            state.walkTime = 0.0;
        }
        updateCamera();

        for (Enemy& enemy : state.enemies) {
            if (enemy.hp <= 0.f) {
                enemy.deathTime -= delta;
            } else {
                enemy.hitTime = std::max(0.f, enemy.hitTime - dt);
            }
        }
        std::erase_if(state.enemies, [](const Enemy& enemy) {
            return enemy.hp <= 0.f && enemy.deathTime <= 0.0;
        });

        while (state.elapsed + 0.0000001 >= state.nextSpawn) {
            spawnEnemy();
            const double interval =
                std::max(0.15, 0.6 - 0.05 * std::floor(state.elapsed / 15.0));
            state.nextSpawn += interval;
        }

        for (Enemy& enemy : state.enemies) {
            if (enemy.hp <= 0.f) {
                continue;
            }
            const auto& type = EnemyTypes[enemy.type];
            float chaseTime = dt;
            if (enemy.knockTime > 0.f) {
                const float pushedTime = std::min(dt, enemy.knockTime);
                enemy.position += enemy.knockDirection * (120.f * pushedTime);
                enemy.knockTime = std::max(0.f, enemy.knockTime - pushedTime);
                chaseTime -= pushedTime;
            }
            if (chaseTime > 0.f) {
                const Vec2f difference = state.player - enemy.position;
                const float distance = std::sqrt(lengthSquared(difference));
                if (distance > 0.f) {
                    enemy.position += difference *
                        (std::min(type.speed * chaseTime, distance) / distance);
                }
            }
            clampToWorld(enemy.position, type.radius);
        }

        // Age existing gems before generating this tick's new gems.
        for (Gem& gem : state.gems) {
            gem.life -= delta;
        }

        state.fireTimer -= delta;
        if (state.fireTimer <= 0.0000001) {
            fire();
            // No accumulated burst when no valid target was available.
            state.fireTimer += state.fireInterval;
            state.fireTimer = std::max(state.fireTimer, 0.0);
        }

        for (Bullet& bullet : state.bullets) {
            if (bullet.life <= 0.0 || bullet.removed) {
                bullet.removed = true;
                continue;
            }
            const float step = static_cast<float>(std::min(delta, bullet.life));
            const Vec2f end = bullet.position + bullet.velocity * step;
            float firstHit = std::numeric_limits<float>::infinity();
            Enemy* victim = nullptr;

            for (Enemy& enemy : state.enemies) {
                if (enemy.hp <= 0.f) {
                    continue;
                }
                const float hit = segmentHit(
                    bullet.position, end, enemy.position,
                    EnemyTypes[enemy.type].radius + 4.f);
                if (hit < firstHit) {
                    firstHit = hit;
                    victim = &enemy;
                }
            }

            if (victim) {
                bullet.position += (end - bullet.position) * firstHit;
                bullet.removed = true;
                victim->hp -= bullet.damage;
                victim->hitTime = 0.1f;

                if (victim->hp <= 0.f) {
                    victim->deathTime = 0.1;
                    victim->knockTime = 0.f;
                    state.gems.push_back({
                        victim->position,
                        EnemyTypes[victim->type].experience,
                        30.0,
                        false,
                    });
                    ++state.kills;
                    playSound(1);
                } else {
                    victim->knockDirection = normalized(bullet.velocity);
                    victim->knockTime = 0.1f;
                }
            } else {
                bullet.position = end;
            }
            bullet.life -= delta;
            if (bullet.life <= 0.0) {
                bullet.removed = true;
            }
        }
        std::erase_if(state.bullets, [](const Bullet& bullet) {
            return bullet.removed;
        });

        float contactDamage = 0.f;
        for (const Enemy& enemy : state.enemies) {
            if (enemy.hp > 0.f &&
                overlaps(state.player, PlayerRadius, enemy.position,
                         EnemyTypes[enemy.type].radius)) {
                contactDamage += EnemyTypes[enemy.type].contactDamage * dt;
            }
        }
        state.takingDamage = contactDamage > 0.f;
        state.hp = std::max(0.f, state.hp - contactDamage);

        if (state.hp <= 0.f) {
            finishPlay(false);
            return;
        }

        for (Gem& gem : state.gems) {
            if (gem.life <= 0.0) {
                gem.removed = true;
                continue;
            }

            Vec2f difference = state.player - gem.position;
            float distance = std::sqrt(lengthSquared(difference));
            if (distance <= state.magnet && distance > 0.f) {
                gem.position += difference *
                    (std::min(300.f * dt, distance) / distance);
            }

            if (overlaps(state.player, PlayerRadius, gem.position, 6.f)) {
                state.experience += gem.experience;
                gem.removed = true;
                playSound(2);
            }
        }
        std::erase_if(state.gems, [](const Gem& gem) {
            return gem.removed;
        });

        if (state.elapsed >= 300.0 - 0.0000001) {
            state.elapsed = 300.0;
            finishPlay(true);
            return;
        }
        (void)beginLevelUp();
    });

    const auto text = [&](meno::Renderer& renderer, const std::string& string,
                          Vec2f position, unsigned int size) {
        renderer.drawText(
            *font, string, position,
            {.characterSize = size, .color = meno::colors::White});
    };

    const auto centeredText = [&](meno::Renderer& renderer,
                                  const std::string& string,
                                  Vec2f position, unsigned int size) {
        meno::TextParams params{
            .characterSize = size,
            .color = meno::colors::White,
        };
        params.origin =
            renderer.measureTextBounds(*font, string, params).center();
        renderer.drawText(*font, string, position, params);
    };

    const auto rightText = [&](meno::Renderer& renderer,
                               const std::string& string,
                               Vec2f position) {
        meno::TextParams params{
            .characterSize = 16,
            .color = meno::colors::White,
        };
        const auto bounds = renderer.measureTextBounds(*font, string, params);
        params.origin = {bounds.position.x + bounds.size.x, bounds.position.y};
        renderer.drawText(*font, string, position, params);
    };

    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.setCamera(hud);
        renderer.drawRect(
            {{0.f, 0.f}, {ViewWidth, ViewHeight}}, {.fill = Background});

        if (screen == Screen::Title) {
            centeredText(renderer, "LAST STAND", {480.f, 220.f}, 48);
            centeredText(renderer, "PRESS ENTER", {480.f, 310.f}, 32);
            return;
        }

        if (screen == Screen::Result) {
            centeredText(renderer, state.survived ? "SURVIVED" : "GAME OVER",
                         {480.f, 140.f}, 48);
            centeredText(renderer, "TIME " + timeText(state.elapsed),
                         {480.f, 225.f}, 32);
            centeredText(renderer, "LEVEL " + std::to_string(state.level),
                         {480.f, 275.f}, 32);
            centeredText(renderer, "KILLS " + std::to_string(state.kills),
                         {480.f, 325.f}, 32);
            centeredText(renderer, "R: RESTART  ESC: QUIT",
                         {480.f, 430.f}, 32);
            return;
        }

        renderer.setCamera(camera);
        renderer.drawRect(
            {{0.f, 0.f}, {WorldWidth, WorldHeight}}, {.fill = Background});

        const float left = camera.center.x - ViewWidth * 0.5f;
        const float right = camera.center.x + ViewWidth * 0.5f;
        const float top = camera.center.y - ViewHeight * 0.5f;
        const float bottom = camera.center.y + ViewHeight * 0.5f;

        for (float x = std::ceil(left / 120.f) * 120.f;
             x <= right; x += 120.f) {
            renderer.drawLine({x, top}, {x, bottom},
                              {.color = Grid, .thickness = 1.f});
        }
        for (float y = std::ceil(top / 120.f) * 120.f;
             y <= bottom; y += 120.f) {
            renderer.drawLine({left, y}, {right, y},
                              {.color = Grid, .thickness = 1.f});
        }
        renderer.drawRect(
            {{0.f, 0.f}, {WorldWidth, WorldHeight}},
            {.fill = meno::colors::Transparent,
             .outline = Border,
             .outlineThickness = -2.f});

        const auto visible = [&](Vec2f p, float radius) {
            return p.x + radius >= left && p.x - radius <= right &&
                   p.y + radius >= top && p.y - radius <= bottom;
        };

        for (const Gem& gem : state.gems) {
            if (!visible(gem.position, 6.f)) {
                continue;
            }
            renderer.draw(
                *textures[5],
                {.position = gem.position,
                 .origin = {6.f, 6.f},
                 .tint = gem.experience == 5 ? Gold : Blue});
        }

        for (const Enemy& enemy : state.enemies) {
            const auto& type = EnemyTypes[enemy.type];
            if (!visible(enemy.position,
                         static_cast<float>(type.cellSize) * 0.5f)) {
                continue;
            }
            const int frame =
                enemy.hp <= 0.f || enemy.hitTime > 0.f
                    ? 2
                    : static_cast<int>(
                          std::floor(state.elapsed / type.frameDuration)) % 2;
            const float half = static_cast<float>(type.cellSize) * 0.5f;
            renderer.draw(
                *textures[static_cast<std::size_t>(enemy.type) + 1],
                {.position = enemy.position,
                 .origin = {half, half},
                 .source = meno::Recti{
                     frame * type.cellSize, 0, type.cellSize, type.cellSize},
                 .flipX = state.player.x < enemy.position.x});
        }

        for (const Bullet& bullet : state.bullets) {
            if (visible(bullet.position, 4.f)) {
                renderer.draw(
                    *textures[4],
                    {.position = bullet.position, .origin = {4.f, 4.f}});
            }
        }

        const int playerFrame =
            state.moving
                ? static_cast<int>(std::floor(state.walkTime / 0.15)) % 2
                : 0;
        renderer.draw(
            *textures[0],
            {.position = state.player,
             .origin = {16.f, 16.f},
             .tint = state.takingDamage ? Hurt : meno::colors::White,
             .source = meno::Recti{playerFrame * 32, 0, 32, 32},
             .flipX = state.flip});

        renderer.setCamera(hud);

        renderer.drawRect(
            {{0.f, 0.f}, {ViewWidth, 6.f}}, {.fill = Grid});
        const float experienceRatio = std::clamp(
            static_cast<float>(state.experience) /
                (5.f * static_cast<float>(state.level)),
            0.f, 1.f);
        if (experienceRatio > 0.f) {
            renderer.drawRect(
                {{0.f, 0.f}, {ViewWidth * experienceRatio, 6.f}},
                {.fill = Blue});
        }

        renderer.drawRect(
            {{16.f, 18.f}, {200.f, 12.f}}, {.fill = HpBackground});
        const float hpRatio = std::clamp(state.hp / state.maxHp, 0.f, 1.f);
        if (hpRatio > 0.f) {
            renderer.drawRect(
                {{16.f, 18.f}, {200.f * hpRatio, 12.f}},
                {.fill = HpFill});
        }

        text(renderer, "HP " + std::to_string(
                 static_cast<int>(std::ceil(state.hp))),
             {16.f, 36.f}, 16);
        rightText(renderer, "LV " + std::to_string(state.level),
                  {944.f, 18.f});
        rightText(renderer, "TIME " + timeText(state.elapsed),
                  {944.f, 44.f});
        rightText(renderer, "KILLS " + std::to_string(state.kills),
                  {944.f, 70.f});

        if (state.selecting) {
            renderer.drawRect(
                {{0.f, 0.f}, {ViewWidth, ViewHeight}},
                {.fill = meno::colors::Black.withAlpha(170)});

            for (int i = 0; i < 3; ++i) {
                const auto& card = CardRects[i];
                renderer.drawRect(
                    card,
                    {.fill = Grid,
                     .outline = Gold,
                     .outlineThickness = 2.f});

                centeredText(renderer, std::to_string(i + 1),
                             {card.position.x + 120.f, 231.f}, 32);
                centeredText(
                    renderer, UpgradeNames[state.cards[i]],
                    {card.position.x + 120.f, 292.f}, 16);
            }
        } else if (state.paused) {
            renderer.drawRect(
                {{0.f, 0.f}, {ViewWidth, ViewHeight}},
                {.fill = meno::colors::Black.withAlpha(170)});
            centeredText(renderer, "PAUSED", {480.f, 270.f}, 48);
        }
    });

    scene.setExitHandler([&] {
        music->stop();
        stopEffects();
    });

    game.renderer().setCamera(hud);
    (void)game.scenes().changeScene(scene.id());
    game.run();
    return 0;
}

} // namespace

int main() {
    try {
        return runGame();
    } catch (const std::exception& exception) {
        std::cerr << "LAST STAND: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "LAST STAND: UNEXPECTED ERROR\n";
        return 1;
    }
}
```