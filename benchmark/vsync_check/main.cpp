// 수직 동기화 확인: 창에 사각형 몇 개를 그리며 프레임 시간을 잰다.
// 사용: vsync_check --vsync on|off [--frames 600] [--warmup 60] [--shapes 100]
// 출력: 한 줄 JSON {"vsync":..., "frames":..., "median_ms":..., "p10_ms":..., "p90_ms":...}
// 그리는 양이 아주 적으므로, vsync를 꺼도 프레임 시간이 주사율 주기(예: 16.7ms)에 머물면
// 드라이버나 화면 합성기가 동기화를 강제하는 것이다.
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    bool vsync = false;
    int frames = 600, warmup = 60, shapes = 100;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string key = argv[i], value = argv[i + 1];
        if (key == "--vsync") vsync = value == "on";
        else if (key == "--frames") frames = std::atoi(value.c_str());
        else if (key == "--warmup") warmup = std::atoi(value.c_str());
        else if (key == "--shapes") shapes = std::atoi(value.c_str());
    }

    sf::RenderWindow window(sf::VideoMode({800u, 600u}), "vsync check");
    window.setFramerateLimit(0);
    window.setVerticalSyncEnabled(vsync);

    std::vector<sf::RectangleShape> rects;
    for (int i = 0; i < shapes; ++i) {
        sf::RectangleShape r({16.f, 16.f});
        r.setPosition({float(i * 37 % 784), float(i * 53 % 584)});
        r.setFillColor(sf::Color(40 + i % 200, 120, 200));
        rects.push_back(r);
    }

    std::vector<double> times;
    sf::Clock clock;
    for (int f = 0; f < warmup + frames && window.isOpen(); ++f) {
        while (const auto event = window.pollEvent())
            if (event->is<sf::Event::Closed>()) window.close();
        window.clear();
        for (auto& r : rects) {
            r.move({1.f, 0.f});
            window.draw(r);
        }
        window.display();
        const double ms = clock.restart().asMicroseconds() / 1000.0;
        if (f >= warmup) times.push_back(ms);
    }
    if (times.empty()) return 1;
    std::sort(times.begin(), times.end());
    const auto at = [&](double q) { return times[std::min(times.size() - 1, size_t(q * times.size()))]; };
    std::printf("{\"vsync\": \"%s\", \"frames\": %zu, \"median_ms\": %.3f, \"p10_ms\": %.3f, \"p90_ms\": %.3f}\n",
                vsync ? "on" : "off", times.size(), at(0.5), at(0.1), at(0.9));
    return 0;
}
