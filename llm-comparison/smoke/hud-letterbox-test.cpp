// 레터박스 상태에서 HUD 위치를 확인한다: resetCamera()와 고정 UI 카메라 비교
#include <meno/Meno.hpp>
#include <optional>
class T final : public meno::Application {
public:
    T() : Application({.framerateLimit = 60}) {}
private:
    meno::Window window{{960u, 540u}, "HUDTEST"};
    meno::Renderer renderer{window};
    std::optional<meno::Font> font = meno::Font::loadFromFile("assets/fonts/game.ttf");
    const meno::Camera2D world{.center = {480.f, 270.f}, .size = {960.f, 540.f}};
    void processEvents() override { window.pollEvents(); if (!window.isOpen()) stop(); }
    void update(double) override {}
    void render(double) override {
        renderer.beginFrame(meno::colors::Black);
        renderer.setCamera(world);
        renderer.drawRect({{0.f, 0.f}, {960.f, 540.f}}, {.fill = meno::Color::fromRgb(0x14181F)});
        // 고정 UI 카메라(같은 size, Letterbox): 게임 화면 왼쪽 위
        renderer.drawText(*font, "UI CAMERA", {16.f, 16.f}, {.characterSize = 32, .color = meno::colors::Yellow});
        // 가이드 방식: resetCamera() 뒤 화면 좌표
        renderer.resetCamera();
        renderer.drawText(*font, "RESETCAMERA", {16.f, 60.f}, {.characterSize = 32, .color = meno::colors::Red});
        renderer.endFrame();
    }
};
int main() { T t; t.run(); }
