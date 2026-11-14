// 템플릿 점검용 최소 프로그램 (meno 조건). 실험 결과물이 아니다.
// 확인 항목: 창 제목·크기, 60 FPS, 폰트·텍스처·효과음·배경음 로드, 시트 일부 그리기, 회전·뒤집기,
// 외곽선 글자, 레터박스 카메라, Esc 종료(0), 에셋 실패(1).
#include <meno/Meno.hpp>

#include <cstdio>
#include <optional>

class Smoke final : public meno::Application {
public:
    Smoke() : Application({.framerateLimit = 60}) {}
    int exitCode = 0;

private:
    meno::Window window{{960u, 540u}, "SMOKE"};
    meno::Renderer renderer{window};
    meno::Camera2D camera{.center = {480.f, 270.f}, .size = {960.f, 540.f}};
    std::optional<meno::Font> font;
    std::optional<meno::Texture> sheet;
    std::optional<meno::Music> bgm;
    std::optional<meno::Sound> hit;
    double elapsed = 0.0;

    bool fail(const char* path) {
        std::fprintf(stderr, "%s\n", path);
        exitCode = 1;
        stop();
        return false;
    }

    void onStart() override {
        font = meno::Font::loadFromFile("assets/fonts/game.ttf");
        if (!font) { fail("assets/fonts/game.ttf"); return; }
        sheet = meno::Texture::loadFromFile("assets/textures/maze_player.png");
        if (!sheet) { fail("assets/textures/maze_player.png"); return; }
        bgm = meno::Music::openFromFile("assets/music/bgm_pong.ogg");
        if (!bgm) { fail("assets/music/bgm_pong.ogg"); return; }
        hit = meno::Sound::loadFromFile("assets/sfx/hit.wav");
        if (!hit) { fail("assets/sfx/hit.wav"); return; }
        bgm->setLooping(true);
        bgm->setVolume(0.4f);
        bgm->play();
    }

    void processEvents() override {
        window.pollEvents();
        if (!window.isOpen()) stop();
    }

    void update(double dt) override {
        const meno::Input& input = window.input();
        if (input.wasPressed(meno::Key::Escape)) { stop(); return; }
        if (input.wasPressed(meno::Key::Space)) hit->play();
        elapsed += dt;
    }

    void render(double) override {
        renderer.beginFrame(meno::colors::Black); // 레터박스 띠
        renderer.setCamera(camera);
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}}, {.fill = meno::Color::fromRgb(0x14181F)});
        const int frame = static_cast<int>(elapsed / 0.1) % 2;
        renderer.draw(*sheet, {.position = {480.f, 270.f},
                               .origin = {12.f, 12.f},
                               .scale = {4.f, 4.f},
                               .rotation = static_cast<float>(elapsed * 90.0),
                               .source = meno::Recti{{frame * 24, 0}, {24, 24}},
                               .flipX = true});
        renderer.drawPolygon({200.f, 270.f}, {{40.f, 0.f}, {12.f, 38.f}, {-32.f, 24.f}, {-32.f, -24.f}, {12.f, -38.f}},
                             {.fill = meno::colors::Transparent, .outline = meno::colors::White, .outlineThickness = 2.f});
        renderer.drawText(*font, "SMOKE 123 +15% 01:23", {16.f, 16.f},
                          {.characterSize = 32, .color = meno::colors::White, .outlineThickness = 2.f,
                           .outlineColor = meno::colors::Black});
        renderer.endFrame();
    }
};

int main() {
    Smoke game;
    game.run();
    return game.exitCode;
}
