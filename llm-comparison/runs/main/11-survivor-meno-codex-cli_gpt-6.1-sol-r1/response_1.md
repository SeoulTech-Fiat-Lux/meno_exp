```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Vec = meno::Vec2f;

constexpr float WorldW = 2880.f;
constexpr float WorldH = 1620.f;
constexpr float Pi = 3.14159265358979323846f;

meno::Color rgb(std::uint32_t value) {
    return meno::Color::fromRgb(value);
}

float lengthSquared(Vec v) {
    return v.x * v.x + v.y * v.y;
}

Vec normalized(Vec v) {
    const float length = std::sqrt(lengthSquared(v));
    return length > 0.f ? v * (1.f / length) : Vec{0.f, 0.f};
}

void clampPosition(Vec& p, float radius) {
    p.x = std::clamp(p.x, radius, WorldW - radius);
    p.y = std::clamp(p.y, radius, WorldH - radius);
}

bool touching(Vec a, Vec b, float radius) {
    return lengthSquared(a - b) <= radius * radius;
}

std::string clockText(double seconds) {
    const int total = static_cast<int>(seconds + 0.000001);
    std::ostringstream stream;
    stream << std::setfill('0') << std::setw(2) << total / 60
           << ':' << std::setw(2) << total % 60;
    return stream.str();
}

struct EnemyType {
    int frameSize;
    float radius;
    float hp;
    float speed;
    float contactDamage;
    double frameDuration;
    int experience;
};

constexpr std::array<EnemyType, 3> Types{{
    {24, 10.f, 10.f, 80.f, 10.f, 0.30, 1},
    {24, 10.f, 6.f, 140.f, 6.f, 0.15, 1},
    {48, 20.f, 60.f, 50.f, 20.f, 0.40, 5}
}};

struct Enemy {
    Vec position;
    Vec knockDirection{0.f, 0.f};
    int type = 0;
    float hp = 10.f;
    double hitTime = 0.0;
    double deathTime = 0.0;
    bool dead = false;
};

struct Bullet {
    Vec position;
    Vec direction;
    double life = 1.2;
    bool dead = false;
};

struct Gem {
    Vec position;
    int experience = 1;
    double life = 30.0;
    bool collected = false;
};

struct SoundPool {
    std::vector<meno::Sound> voices;
    std::size_t next = 0;

    void initialize(const std::shared_ptr<const meno::SoundBuffer>& buffer, int count) {
        voices.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            voices.emplace_back(buffer);
        }
    }

    void play() {
        if (!voices.empty()) {
            voices[next].play();
            next = (next + 1) % voices.size();
        }
    }

    void stop() {
        for (auto& voice : voices) {
            voice.stop();
        }
        next = 0;
    }
};

// First intersection along a bullet's movement segment.
float segmentHit(Vec start, Vec end, Vec center, float radius) {
    const Vec offset = start - center;
    const float c = lengthSquared(offset) - radius * radius;
    if (c <= 0.f) {
        return 0.f;
    }
    const Vec movement = end - start;
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
        ? t : std::numeric_limits<float>::infinity();
}
} // namespace

int main() {
    try {
        meno::Game game{{
            .application = {.framerateLimit = 60},
            .windowSize = {960, 540},
            .title = "LAST STAND",
            .clearColor = meno::colors::Black
        }};

        bool missing = false;
        const auto require = [&](bool loaded, const char* path) {
            if (!loaded) {
                std::cerr << "Failed to load: " << path << '\n';
                missing = true;
            }
        };

        const char* fontPath = "assets/fonts/game.ttf";
        const auto font = game.resources().loadFont(fontPath);
        require(static_cast<bool>(font), fontPath);

        constexpr std::array<const char*, 6> texturePaths{{
            "assets/textures/sv_player.png",
            "assets/textures/sv_walker.png",
            "assets/textures/sv_runner.png",
            "assets/textures/sv_brute.png",
            "assets/textures/sv_bullet.png",
            "assets/textures/sv_gem.png"
        }};
        std::array<std::shared_ptr<const meno::Texture>, 6> textures;
        for (std::size_t i = 0; i < textures.size(); ++i) {
            textures[i] = game.resources().loadTexture(texturePaths[i]);
            require(static_cast<bool>(textures[i]), texturePaths[i]);
        }

        constexpr std::array<const char*, 4> soundPaths{{
            "assets/sfx/shoot.wav",
            "assets/sfx/hit.wav",
            "assets/sfx/pickup.wav",
            "assets/sfx/levelup.wav"
        }};
        std::array<std::shared_ptr<const meno::SoundBuffer>, 4> buffers;
        for (std::size_t i = 0; i < buffers.size(); ++i) {
            buffers[i] = game.resources().loadSoundBuffer(soundPaths[i]);
            require(static_cast<bool>(buffers[i]), soundPaths[i]);
        }

        const char* musicPath = "assets/music/bgm_survivor.ogg";
        auto music = meno::Music::openFromFile(musicPath);
        require(static_cast<bool>(music), musicPath);
        if (missing) {
            return 1;
        }

        std::array<SoundPool, 4> sounds;
        sounds[0].initialize(buffers[0], 8);
        sounds[1].initialize(buffers[1], 16);
        sounds[2].initialize(buffers[2], 16);
        sounds[3].initialize(buffers[3], 2);
        music->setLooping(true);
        music->setVolume(0.4f);

        enum class Screen { Title, Play, Result };
        Screen screen = Screen::Title;

        std::mt19937 random{std::random_device{}()};
        const auto randomUnit = [&]() {
            return std::uniform_real_distribution<float>{0.f, 1.f}(random);
        };

        const meno::Camera2D hud{
            .center = {480.f, 270.f},
            .size = {960.f, 540.f}
        };
        meno::Camera2D camera = hud;
        game.renderer().setCamera(hud);

        std::vector<Enemy> enemies;
        std::vector<Bullet> bullets;
        std::vector<Gem> gems;
        enemies.reserve(400);
        bullets.reserve(128);
        gems.reserve(512);

        Vec player{WorldW / 2.f, WorldH / 2.f};
        float hp = 100.f;
        float maxHp = 100.f;
        float moveSpeed = 200.f;
        float damage = 10.f;
        float magnetRange = 60.f;
        int projectiles = 1;
        int level = 1;
        int experience = 0;
        int kills = 0;
        double elapsed = 0.0;
        double titleTime = 0.0;
        double fireInterval = 0.8;
        double fireTimer = 0.8;
        double spawnTimer = 0.5;
        double walkTime = 0.0;
        bool moving = false;
        bool faceLeft = false;
        bool damaged = false;
        bool paused = false;
        bool choosing = false;
        bool survived = false;
        std::array<int, 3> choices{0, 1, 2};

        constexpr std::array<const char*, 6> abilityNames{{
            "FIRE RATE +15%",
            "DAMAGE +5",
            "PROJECTILE +1",
            "SPEED +10%",
            "MAX HP +20",
            "MAGNET +40"
        }};
        const std::array<meno::Rectf, 3> cards{{
            {{80.f, 200.f}, {240.f, 140.f}},
            {{360.f, 200.f}, {240.f, 140.f}},
            {{640.f, 200.f}, {240.f, 140.f}}
        }};

        const auto updateCamera = [&]() {
            camera.center = {
                std::clamp(player.x, 480.f, WorldW - 480.f),
                std::clamp(player.y, 270.f, WorldH - 270.f)
            };
            game.renderer().setCamera(camera);
        };

        const auto startPlay = [&]() {
            for (auto& sound : sounds) {
                sound.stop();
            }
            music->stop();
            enemies.clear();
            bullets.clear();
            gems.clear();
            player = {WorldW / 2.f, WorldH / 2.f};
            hp = maxHp = 100.f;
            moveSpeed = 200.f;
            damage = 10.f;
            magnetRange = 60.f;
            projectiles = 1;
            level = 1;
            experience = 0;
            kills = 0;
            elapsed = 0.0;
            titleTime = 0.0;
            fireInterval = fireTimer = 0.8;
            spawnTimer = 0.5;
            walkTime = 0.0;
            moving = faceLeft = damaged = paused = choosing = survived = false;
            choices = {0, 1, 2};
            screen = Screen::Play;
            updateCamera();
            music->play();
        };

        const auto beginLevel = [&]() {
            if (experience < 5 * level) {
                return false;
            }
            experience -= 5 * level;
            ++level;
            std::array<int, 6> shuffled{0, 1, 2, 3, 4, 5};
            std::shuffle(shuffled.begin(), shuffled.end(), random);
            std::copy_n(shuffled.begin(), 3, choices.begin());
            choosing = true;
            sounds[3].play();
            music->pause();
            return true;
        };

        const auto finish = [&](bool won) {
            survived = won;
            screen = Screen::Result;
            paused = choosing = false;
            music->stop();
            game.renderer().setCamera(hud);
        };

        // Select uniformly by length from the valid pieces of the expanded
        // camera rectangle's perimeter.
        const auto spawnEnemy = [&]() {
            struct Edge {
                Vec a;
                Vec b;
                float length;
            };
            std::array<Edge, 4> edges{};
            int count = 0;
            float total = 0.f;
            const float left = camera.center.x - 480.f - 64.f;
            const float right = camera.center.x + 480.f + 64.f;
            const float top = camera.center.y - 270.f - 64.f;
            const float bottom = camera.center.y + 270.f + 64.f;

            const auto add = [&](Vec a, Vec b) {
                const float length = std::sqrt(lengthSquared(b - a));
                if (length > 0.f) {
                    edges[count++] = {a, b, length};
                    total += length;
                }
            };
            const float x0 = std::max(0.f, left);
            const float x1 = std::min(WorldW, right);
            const float y0 = std::max(0.f, top);
            const float y1 = std::min(WorldH, bottom);
            if (top >= 0.f && top <= WorldH) {
                add({x0, top}, {x1, top});
            }
            if (bottom >= 0.f && bottom <= WorldH) {
                add({x0, bottom}, {x1, bottom});
            }
            if (left >= 0.f && left <= WorldW) {
                add({left, y0}, {left, y1});
            }
            if (right >= 0.f && right <= WorldW) {
                add({right, y0}, {right, y1});
            }
            if (count == 0) {
                return;
            }

            float distance = randomUnit() * total;
            const Edge* selected = &edges[count - 1];
            for (int i = 0; i < count; ++i) {
                if (distance <= edges[i].length) {
                    selected = &edges[i];
                    break;
                }
                distance -= edges[i].length;
            }
            Vec position = selected->a +
                (selected->b - selected->a) *
                std::clamp(distance / selected->length, 0.f, 1.f);

            int type = 0;
            const float roll = randomUnit();
            if (elapsed >= 120.0) {
                type = roll < 0.6f ? 0 : (roll < 0.9f ? 1 : 2);
            } else if (elapsed >= 60.0) {
                type = roll < 0.7f ? 0 : 1;
            }
            enemies.push_back(Enemy{
                .position = position,
                .type = type,
                .hp = Types[type].hp
            });
        };

        meno::Scene& scene = game.scenes().createScene();
        scene.setUpdateHandler([&](double dt) {
            const meno::Input& input = game.window().input();
            if (input.wasPressed(meno::Key::Escape)) {
                game.stop();
                return;
            }

            if (screen == Screen::Title) {
                titleTime += dt;
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

            if (choosing) {
                int selected = -1;
                if (input.wasPressed(meno::Key::Num1)) selected = 0;
                else if (input.wasPressed(meno::Key::Num2)) selected = 1;
                else if (input.wasPressed(meno::Key::Num3)) selected = 2;

                if (input.wasPressed(meno::MouseButton::Left)) {
                    const Vec mouse{input.mousePosition()};
                    const Vec point = meno::screenToWorld(
                        hud, game.renderer().framebufferSize(), mouse);
                    for (int i = 0; i < 3; ++i) {
                        if (cards[i].contains(point)) {
                            selected = i;
                        }
                    }
                }
                if (selected >= 0) {
                    switch (choices[selected]) {
                    case 0: fireInterval *= 0.85; break;
                    case 1: damage += 5.f; break;
                    case 2: ++projectiles; break;
                    case 3: moveSpeed *= 1.1f; break;
                    case 4:
                        maxHp += 20.f;
                        hp = std::min(maxHp, hp + 20.f);
                        break;
                    case 5: magnetRange += 40.f; break;
                    }
                    choosing = false;
                    if (!beginLevel()) {
                        music->play();
                    }
                }
                return;
            }

            if (input.wasPressed(meno::Key::P)) {
                paused = !paused;
                if (paused) music->pause();
                else music->play();
                return;
            }
            if (paused) {
                return;
            }

            const double step = std::min(dt, 300.0 - elapsed);
            const float fdt = static_cast<float>(step);
            elapsed = std::min(300.0, elapsed + step);

            Vec movement{0.f, 0.f};
            if (input.isDown(meno::Key::A) || input.isDown(meno::Key::Left))
                movement.x -= 1.f;
            if (input.isDown(meno::Key::D) || input.isDown(meno::Key::Right))
                movement.x += 1.f;
            if (input.isDown(meno::Key::W) || input.isDown(meno::Key::Up))
                movement.y -= 1.f;
            if (input.isDown(meno::Key::S) || input.isDown(meno::Key::Down))
                movement.y += 1.f;

            moving = lengthSquared(movement) > 0.f;
            if (movement.x != 0.f) {
                faceLeft = movement.x < 0.f;
            }
            if (moving) {
                player += normalized(movement) * (moveSpeed * fdt);
                walkTime += step;
            } else {
                walkTime = 0.0;
            }
            clampPosition(player, 14.f);
            updateCamera();

            for (auto& enemy : enemies) {
                if (enemy.dead) {
                    enemy.deathTime -= step;
                    continue;
                }
                if (enemy.hitTime > 0.0) {
                    const float knockStep =
                        static_cast<float>(std::min(step, enemy.hitTime));
                    enemy.position += enemy.knockDirection * (120.f * knockStep);
                    enemy.hitTime = std::max(0.0, enemy.hitTime - step);
                } else {
                    enemy.position += normalized(player - enemy.position) *
                        (Types[enemy.type].speed * fdt);
                }
                clampPosition(enemy.position, Types[enemy.type].radius);
            }
            std::erase_if(enemies, [](const Enemy& enemy) {
                return enemy.dead && enemy.deathTime <= 0.0;
            });

            spawnTimer -= step;
            while (spawnTimer <= 0.0000001) {
                if (enemies.size() < 400) {
                    spawnEnemy();
                }
                const double interval = std::max(
                    0.15, 0.6 - 0.05 * std::floor(elapsed / 15.0));
                spawnTimer += interval;
            }

            fireTimer -= step;
            while (fireTimer <= 0.0000001) {
                fireTimer += fireInterval;
                const Enemy* target = nullptr;
                float nearest = 400.f * 400.f;
                for (const auto& enemy : enemies) {
                    if (enemy.dead) continue;
                    const float distance = lengthSquared(enemy.position - player);
                    if (distance <= nearest) {
                        nearest = distance;
                        target = &enemy;
                    }
                }
                if (target != nullptr) {
                    const Vec aim = target->position - player;
                    const float angle = std::atan2(aim.y, aim.x);
                    for (int i = 0; i < projectiles; ++i) {
                        const float offset =
                            (static_cast<float>(i) -
                             static_cast<float>(projectiles - 1) * 0.5f) *
                            10.f * Pi / 180.f;
                        bullets.push_back({
                            player,
                            {std::cos(angle + offset), std::sin(angle + offset)}
                        });
                    }
                    sounds[0].play();
                }
            }

            for (auto& bullet : bullets) {
                const float travelTime =
                    static_cast<float>(std::min(step, bullet.life));
                const Vec end = bullet.position +
                    bullet.direction * (500.f * travelTime);
                Enemy* victim = nullptr;
                float firstHit = std::numeric_limits<float>::infinity();

                for (auto& enemy : enemies) {
                    if (enemy.dead) continue;
                    const float hit = segmentHit(
                        bullet.position, end, enemy.position,
                        4.f + Types[enemy.type].radius);
                    if (hit < firstHit) {
                        firstHit = hit;
                        victim = &enemy;
                    }
                }

                if (victim != nullptr) {
                    bullet.dead = true;
                    victim->hp -= damage;
                    victim->hitTime = 0.1;
                    victim->knockDirection = bullet.direction;
                    if (victim->hp <= 0.f) {
                        victim->dead = true;
                        victim->deathTime = 0.1;
                        gems.push_back({
                            victim->position, Types[victim->type].experience
                        });
                        ++kills;
                        sounds[1].play();
                    }
                }
                bullet.position = end;
                bullet.life -= step;
                if (bullet.life <= 0.0) {
                    bullet.dead = true;
                }
            }
            std::erase_if(bullets, [](const Bullet& bullet) {
                return bullet.dead;
            });

            damaged = false;
            for (const auto& enemy : enemies) {
                if (!enemy.dead && touching(
                        player, enemy.position, 14.f + Types[enemy.type].radius)) {
                    hp -= Types[enemy.type].contactDamage * fdt;
                    damaged = true;
                }
            }
            hp = std::max(0.f, hp);

            for (auto& gem : gems) {
                gem.life -= step;
                if (gem.life <= 0.0) continue;
                const Vec delta = player - gem.position;
                const float distance = std::sqrt(lengthSquared(delta));
                if (distance <= magnetRange && distance > 0.f) {
                    gem.position += delta *
                        (std::min(300.f * fdt, distance) / distance);
                }
                if (touching(player, gem.position, 20.f)) {
                    experience += gem.experience;
                    gem.collected = true;
                    sounds[2].play();
                }
            }
            std::erase_if(gems, [](const Gem& gem) {
                return gem.collected || gem.life <= 0.0;
            });

            if (hp <= 0.f) {
                finish(false);
            } else if (elapsed >= 300.0) {
                finish(true);
            } else {
                (void)beginLevel();
            }
        });

        scene.setDrawHandler([&](meno::Renderer& renderer) {
            const auto text = [&](const std::string& value, Vec position,
                                  unsigned int size, bool centered = false) {
                meno::TextParams params{
                    .characterSize = size,
                    .color = meno::colors::White
                };
                if (centered) {
                    params.origin =
                        renderer.measureTextBounds(*font, value, params).center();
                }
                renderer.drawText(*font, value, position, params);
            };

            renderer.setCamera(hud);
            renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}},
                              {.fill = rgb(0x1A1420)});

            if (screen == Screen::Title) {
                text("LAST STAND", {480.f, 210.f}, 48, true);
                text("PRESS ENTER", {480.f, 300.f}, 32, true);
                return;
            }
            if (screen == Screen::Result) {
                text(survived ? "SURVIVED" : "GAME OVER",
                     {480.f, 150.f}, 48, true);
                text("TIME " + clockText(elapsed), {480.f, 230.f}, 32, true);
                text("LEVEL " + std::to_string(level), {480.f, 280.f}, 32, true);
                text("KILLS " + std::to_string(kills), {480.f, 330.f}, 32, true);
                text("R: RESTART  ESC: QUIT", {480.f, 420.f}, 32, true);
                return;
            }

            renderer.setCamera(camera);
            const float left = camera.center.x - 480.f;
            const float right = camera.center.x + 480.f;
            const float top = camera.center.y - 270.f;
            const float bottom = camera.center.y + 270.f;
            for (float x = std::ceil(left / 120.f) * 120.f;
                 x <= right; x += 120.f) {
                renderer.drawLine({x, top}, {x, bottom},
                                  {.color = rgb(0x2A2233), .thickness = 1.f});
            }
            for (float y = std::ceil(top / 120.f) * 120.f;
                 y <= bottom; y += 120.f) {
                renderer.drawLine({left, y}, {right, y},
                                  {.color = rgb(0x2A2233), .thickness = 1.f});
            }
            renderer.drawRect(
                {{0.f, 0.f}, {WorldW, WorldH}},
                {.fill = meno::colors::Transparent,
                 .outline = rgb(0x6B5B7B),
                 .outlineThickness = -2.f});

            const auto visible = [&](Vec p, float margin) {
                return p.x >= left - margin && p.x <= right + margin &&
                       p.y >= top - margin && p.y <= bottom + margin;
            };

            for (const auto& gem : gems) {
                if (!visible(gem.position, 6.f)) continue;
                renderer.draw(*textures[5], {
                    .position = gem.position,
                    .origin = {6.f, 6.f},
                    .tint = rgb(gem.experience == 5 ? 0xFFD54A : 0x4FC3F7)
                });
            }

            for (const auto& enemy : enemies) {
                const auto& type = Types[enemy.type];
                if (!visible(enemy.position, static_cast<float>(type.frameSize)))
                    continue;
                const int frame = enemy.dead || enemy.hitTime > 0.0
                    ? 2
                    : static_cast<int>(elapsed / type.frameDuration) % 2;
                const float half = static_cast<float>(type.frameSize) * 0.5f;
                renderer.draw(*textures[1 + enemy.type], {
                    .position = enemy.position,
                    .origin = {half, half},
                    .source = meno::Recti{
                        frame * type.frameSize, 0,
                        type.frameSize, type.frameSize
                    },
                    .flipX = player.x < enemy.position.x
                });
            }

            for (const auto& bullet : bullets) {
                if (!visible(bullet.position, 4.f)) continue;
                renderer.draw(*textures[4], {
                    .position = bullet.position,
                    .origin = {4.f, 4.f}
                });
            }

            const int playerFrame =
                moving ? static_cast<int>(walkTime / 0.15) % 2 : 0;
            renderer.draw(*textures[0], {
                .position = player,
                .origin = {16.f, 16.f},
                .tint = damaged ? rgb(0xFF6060) : meno::colors::White,
                .source = meno::Recti{playerFrame * 32, 0, 32, 32},
                .flipX = faceLeft
            });

            renderer.setCamera(hud);
            renderer.drawRect({{0.f, 0.f}, {960.f, 6.f}},
                              {.fill = rgb(0x2A2233)});
            const float experienceRatio = std::clamp(
                static_cast<float>(experience) / (5.f * level), 0.f, 1.f);
            renderer.drawRect({{0.f, 0.f}, {960.f * experienceRatio, 6.f}},
                              {.fill = rgb(0x4FC3F7)});
            renderer.drawRect({{16.f, 16.f}, {200.f, 12.f}},
                              {.fill = rgb(0x3A1F24)});
            renderer.drawRect(
                {{16.f, 16.f}, {200.f * std::clamp(hp / maxHp, 0.f, 1.f), 12.f}},
                {.fill = rgb(0xE5533D)});
            text("HP " + std::to_string(static_cast<int>(std::ceil(hp))),
                 {16.f, 32.f}, 16);
            text("LV " + std::to_string(level), {720.f, 12.f}, 16);
            text("TIME " + clockText(elapsed), {720.f, 34.f}, 16);
            text("KILLS " + std::to_string(kills), {720.f, 56.f}, 16);

            if (choosing || paused) {
                renderer.drawRect(
                    {{0.f, 0.f}, {960.f, 540.f}},
                    {.fill = meno::Color{0, 0, 0, 160}});
            }
            if (paused) {
                text("PAUSED", {480.f, 270.f}, 48, true);
            }
            if (choosing) {
                for (int i = 0; i < 3; ++i) {
                    renderer.drawRect(cards[i], {
                        .fill = rgb(0x2A2233),
                        .outline = rgb(0xFFD54A),
                        .outlineThickness = 2.f
                    });
                    const float centerX = cards[i].position.x + 120.f;
                    text(std::to_string(i + 1), {centerX, 228.f}, 32, true);
                    text(abilityNames[choices[i]], {centerX, 285.f}, 16, true);
                }
            }
        });

        (void)game.scenes().changeScene(scene.id());
        game.run();
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
```