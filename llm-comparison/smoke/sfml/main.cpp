// 템플릿 점검용 최소 프로그램 (SFML 3.1 조건). 실험 결과물이 아니다.
// meno 쪽 smoke와 같은 항목을 확인한다.
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <iostream>
#include <optional>

int main() {
    sf::RenderWindow window(sf::VideoMode({960u, 540u}), "SMOKE");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("assets/fonts/game.ttf")) { std::cerr << "assets/fonts/game.ttf\n"; return 1; }
    sf::Texture sheet;
    if (!sheet.loadFromFile("assets/textures/maze_player.png")) { std::cerr << "assets/textures/maze_player.png\n"; return 1; }
    sf::Music bgm;
    if (!bgm.openFromFile("assets/music/bgm_pong.ogg")) { std::cerr << "assets/music/bgm_pong.ogg\n"; return 1; }
    sf::SoundBuffer hitBuffer;
    if (!hitBuffer.loadFromFile("assets/sfx/hit.wav")) { std::cerr << "assets/sfx/hit.wav\n"; return 1; }
    sf::Sound hit(hitBuffer);
    bgm.setLooping(true);
    bgm.setVolume(40.f);
    bgm.play();

    sf::Clock clock;
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) window.close();
                if (key->code == sf::Keyboard::Key::Space) hit.play();
            }
        }
        const float t = clock.getElapsedTime().asSeconds();
        window.clear(sf::Color(0x14, 0x18, 0x1F));

        const int frame = static_cast<int>(t / 0.1f) % 2;
        sf::Sprite sprite(sheet, sf::IntRect({frame * 24, 0}, {24, 24}));
        sprite.setOrigin({12.f, 12.f});
        sprite.setPosition({480.f, 270.f});
        sprite.setScale({-4.f, 4.f});
        sprite.setRotation(sf::degrees(t * 90.f));
        window.draw(sprite);

        sf::Text text(font, "SMOKE 123 +15% 01:23", 32);
        text.setPosition({16.f, 16.f});
        text.setOutlineThickness(2.f);
        text.setOutlineColor(sf::Color::Black);
        window.draw(text);
        window.display();
    }
    return 0;
}
