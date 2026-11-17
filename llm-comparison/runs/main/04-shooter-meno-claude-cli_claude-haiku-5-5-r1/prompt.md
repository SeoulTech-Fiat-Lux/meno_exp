당신은 C++20 게임 프로그래머입니다. 아래 요구사항 문서에 적힌 미니게임을 meno로 구현합니다.

이번 구현은 meno 조건입니다.

- meno 공개 헤더(<meno/...>)와 C++20 표준 라이브러리만 씁니다. SFML 헤더는 쓸 수 없습니다.
- main.cpp 한 파일만 작성합니다. 제공된 CMakeLists.txt와 assets/는 바꿀 수 없습니다.
- 답변에는 main.cpp 전체를 ```cpp 코드 블록 하나로 넣습니다.

# meno 문서

<<< SeoulTech-Fiat-Lux/Meno@46f8934:README.md >>>
<!-- 로고는 GitHub 테마 설정을 따라 하나만 보인다(#gh-light-mode-only, #gh-dark-mode-only).
     <picture>는 OS 설정만 따르므로, GitHub만 다크일 때 검은 글자 로고가 나와 글자가
     보이지 않는다. -->
<p align="center">
  <a href="https://seoultech-fiat-lux.github.io/Meno/#gh-light-mode-only">
    <img src="docs/theme/logo/meno-logo-on-light.svg" alt="meno" width="360">
  </a>
  <a href="https://seoultech-fiat-lux.github.io/Meno/#gh-dark-mode-only">
    <img src="docs/theme/logo/meno-logo-on-dark.svg" alt="meno" width="360">
  </a>
</p>

<p align="center">
  SFML 기반 C++20 2D 게임 개발 라이브러리
</p>

<p align="center">
  <a href="https://seoultech-fiat-lux.github.io/Meno/guide.html">설명서</a>
  ·
  <a href="https://seoultech-fiat-lux.github.io/Meno/">API 레퍼런스</a>
  ·
  <a href="examples">예제</a>
  ·
  <a href="#시작하기">시작하기</a>
</p>

서울과학기술대학교 캡스톤디자인(2) 프로젝트인 meno는 SFML 3 위에서 2D 게임을 만드는 C++20 라이브러리입니다.

## 짧은 예제

방향키로 원을 움직입니다. 그림이나 글꼴 파일 없이 그대로 실행됩니다.

```cpp
#include <meno/Meno.hpp>

int main() {
    meno::Game game{{.windowSize = {640, 480}, .title = "Hello meno"}};
    meno::Vec2f position{320.f, 240.f};

    meno::Scene& play = game.scenes().createScene();
    play.setUpdateHandler([&](double dt) {
        const meno::Input& input = game.window().input();
        const float step = 240.f * static_cast<float>(dt); // 1초에 240픽셀
        if (input.isDown(meno::Key::Left)) {
            position.x -= step;
        }
        if (input.isDown(meno::Key::Right)) {
            position.x += step;
        }
        if (input.isDown(meno::Key::Up)) {
            position.y -= step;
        }
        if (input.isDown(meno::Key::Down)) {
            position.y += step;
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawCircle(position, 24.f, {.fill = meno::Color::fromRgb(0x00B1ED)});
    });
    (void)game.scenes().changeScene(play.id());
    game.run(); // 창을 닫으면 돌아옵니다.
}
```

`Game`이 창, 렌더러, 리소스, 씬을 묶어 게임 루프를 돌립니다. update 핸들러는 고정 시간 간격으로,
draw 핸들러는 프레임마다 불립니다.

## 시작하기

CMake 3.28 이상과 C++20 컴파일러(MSVC 2022, GCC 13 이상, Clang 16 이상)가 필요합니다.
SFML 3은 CMake가 함께 받아 빌드하므로 따로 설치하지 않습니다.

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_game LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    meno
    GIT_REPOSITORY https://github.com/SeoulTech-Fiat-Lux/Meno.git
    GIT_TAG        v0.2.0
)
FetchContent_MakeAvailable(meno)

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE meno::meno)
```

- 첫 빌드는 SFML과 그 의존성까지 빌드하므로 조금 걸립니다. 두 번째부터는 빠릅니다.
- 저장소를 프로젝트 안에 받아 두었다면 FetchContent 대신 `add_subdirectory(<meno 경로>)`를 씁니다.
- 이렇게 가져가면 meno의 테스트와 예제는 빌드하지 않으며, 사용자 프로젝트의 `CMAKE_BUILD_TYPE`과
  `BUILD_SHARED_LIBS`도 바꾸지 않습니다.

<details>
<summary>Linux에서는 SFML이 쓰는 시스템 패키지를 먼저 설치합니다 (Ubuntu)</summary>

CI와 같은 목록입니다. Windows는 따로 설치할 것이 없습니다.

```bash
sudo apt-get install -y \
  libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
  libfreetype-dev libgl1-mesa-dev libegl1-mesa-dev \
  libflac-dev libvorbis-dev libogg-dev \
  libharfbuzz-dev
```

</details>

예제를 바로 실행하려면, 이 저장소를 빌드하면 됩니다.

```bash
git clone https://github.com/SeoulTech-Fiat-Lux/Meno.git
cd Meno
cmake -S . -B build
cmake --build build
./build/examples/meno_example_input_demo
```

Visual Studio 생성기(Windows의 기본값)를 쓰면 실행 파일은 `build/examples/Debug/`에 생깁니다.
다른 예제는 [examples](examples)에 있습니다.

## 문서

- [사용자 설명서](https://seoultech-fiat-lux.github.io/Meno/guide.html): 설치와 첫 게임부터
  GameObject, 충돌 판정, 씬 전환, 오디오, 카메라까지 게임을 만드는 순서에 따라 설명합니다. 원문은
  [docs/guide](docs/guide)에 있습니다.
- [API 레퍼런스](https://seoultech-fiat-lux.github.io/Meno/): 클래스·함수마다 동작, 주의할 점, 예제를
  적었습니다. 게임, 씬, 그래픽, 입력 같은 모듈별로 찾아볼 수 있습니다.
- [예제](examples): 실행해 볼 수 있는 작은 데모 게임입니다.
- [테스트와 CI](https://seoultech-fiat-lux.github.io/Meno/md_docs_2testing.html): 기능별 테스트와
  실행 방법, CI 구성을 정리했습니다. 원문은 [docs/testing.md](docs/testing.md)에 있습니다.

## 한계

0.x 버전이므로 API가 바뀔 수 있습니다. 지금은 다음과 같은 한계가 있습니다.

- 3D, 네트워크, 물리 시뮬레이션, 타일맵, UI 위젯은 없습니다.
- 충돌은 겹치는지만 판정하며, 사각형과 원형 콜라이더만 제공합니다.
- 한 번에 하나의 `Game`(`Application`)만 실행할 수 있습니다.
- `ResourceManager`는 한 번 읽은 리소스를 매니저가 사라질 때까지 캐시에 둡니다.
- Windows(MSVC)와 Ubuntu(GCC, Clang)에서 빌드하고 테스트합니다. macOS에서는 확인하지
  않았습니다.

## 기여하기

빌드와 테스트, 저장소 구조, 브랜치·커밋 규칙, 코드 스타일은 [CONTRIBUTING.md](CONTRIBUTING.md)에
있습니다.

## 라이선스

[zlib](LICENSE).

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/02-first-game.md >>>
# 2장. 첫 게임: Game과 Scene

이 장에서는 화면 안을 튕겨 다니는 공을 만들면서 meno 게임의 기본 구조를 익힙니다. `Game`은 창과 게임
루프를, `Scene`에 붙인 핸들러는 게임 규칙을 담당합니다.

## 완성 코드

```cpp
#include <meno/Meno.hpp>

#include <cmath>

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {640, 480},
                     .title = "Bouncing ball",
                     .clearColor = meno::Color::fromRgb(0x1E2430)}};

    const float radius = 16.f;
    meno::Vec2f position{320.f, 240.f};
    meno::Vec2f velocity{180.f, 120.f}; // 초당 이동 거리(픽셀)

    meno::Scene& play = game.scenes().createScene();
    play.setUpdateHandler([&](double fixedDeltaTime) {
        position += velocity * static_cast<float>(fixedDeltaTime);

        // 벽을 넘으면 안쪽으로 되돌리고, 그 축의 이동 방향을 바꿉니다.
        const meno::Vec2f size{game.window().size()};
        if (position.x < radius) {
            position.x = radius;
            velocity.x = std::abs(velocity.x);
        } else if (position.x > size.x - radius) {
            position.x = size.x - radius;
            velocity.x = -std::abs(velocity.x);
        }
        if (position.y < radius) {
            position.y = radius;
            velocity.y = std::abs(velocity.y);
        } else if (position.y > size.y - radius) {
            position.y = size.y - radius;
            velocity.y = -std::abs(velocity.y);
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawCircle(position, radius, {.fill = meno::Color::fromRgb(0x00B1ED)});
    });

    (void)game.scenes().changeScene(play.id());
    game.run();
}
```

## Game: 창과 게임 루프

`meno::Game`은 창(`Window`), 렌더러(`Renderer`), 리소스 캐시(`ResourceManager`), 씬
관리자(`SceneManager`)를 한곳에 묶은 클래스입니다. `Game` 객체를 만드는 즉시 창이 뜨고, `run()`을
호출하면 창이 닫힐 때까지 게임 루프가 실행됩니다. 네 구성 요소에는 각각 `game.window()`,
`game.renderer()`, `game.resources()`, `game.scenes()`로 접근합니다.

설정은 `GameConfig`로 넘깁니다.

| 필드 | 기본값 | 의미 |
|---|---|---|
| `application` | `{}` | 게임 루프의 시간 설정(`ApplicationConfig`) |
| `windowSize` | `{800, 600}` | 창 내부(그리기 영역)의 픽셀 크기 |
| `title` | `"meno"` | 창 제목 |
| `clearColor` | 검은색 | 프레임마다 화면을 지울 배경색 |

지정 초기화(`.windowSize = ...`)에서는 필드를 위 표의 순서대로 써야 하며, 순서가 바뀌면 컴파일되지
않습니다. 기본값을 쓸 필드는 생략해도 됩니다.

## Scene과 핸들러

게임 규칙은 `Scene`에 핸들러(람다)로 붙입니다. Scene은 타이틀, 게임, 게임 오버처럼 화면 단위로
만들며, 한 번에 하나만 활성 상태가 됩니다.

| 핸들러 | 호출 시점 | 주로 하는 일 |
|---|---|---|
| `setEnterHandler` | 이 Scene이 활성화될 때 | 점수 초기화 같은 준비 |
| `setUpdateHandler` | 고정 시간 간격마다(기본 초당 60회) | 게임 상태 갱신 |
| `setDrawHandler` | 프레임마다, Scene의 GameObject를 그린 뒤 | 그리기 |
| `setExitHandler` | 다른 Scene으로 바뀌거나 게임이 끝날 때 | 정리 |

Scene은 `game.scenes().createScene()`으로 만들고 `changeScene(id)`로 활성화합니다. 활성 Scene이
없으면 화면은 배경색으로만 칠해집니다. `changeScene()`은 존재하지 않는 ID를 받으면 `false`를
돌려줍니다. 이 반환값을 쓰지 않으면 컴파일러가 경고하므로, 확인할 필요가 없을 때는 `(void)`로
명시적으로 버립니다.

## 한 프레임에 일어나는 일

`run()`은 프레임마다 다음 과정을 되풀이합니다.

1. 창 이벤트와 입력을 처리합니다. 창이 닫혔으면 루프를 멈춥니다.
2. 흐른 시간에 맞춰 update 핸들러를 0번 이상 호출합니다.
3. 화면을 `clearColor`로 지우고 활성 Scene을 그립니다. GameObject를 먼저 그린 다음 draw 핸들러를
   호출합니다.

update 핸들러는 **고정 시간 간격**으로 호출됩니다. 인자 `fixedDeltaTime`은 항상 같은 값(기본
1/60초)입니다. 따라서 "초당 180픽셀"처럼 초 단위로 정한 속도에 이 값을 곱하면, 컴퓨터 성능과
관계없이 게임이 같은 속도로 진행됩니다. 프레임이 느려지면 한 프레임에서 update를 여러 번 호출해 밀린
시간을 따라잡고, 프레임이 빠르면 update 없이 그리기만 하는 프레임도 생깁니다. 밀린 update가
`maxUpdatesPerFrame`(기본 8회)을 넘으면 남은 시간은 버립니다. 덕분에 게임은 멈추지 않고 느려지기만
합니다.

draw 핸들러는 프레임마다 한 번 호출되는데, 초당 프레임 수는 컴퓨터와 상황에 따라 달라집니다. 따라서
게임 상태는 update 핸들러에서만 바꾸고, draw 핸들러에서는 그리기만 합니다.

`.application = {.framerateLimit = 60}`은 초당 그리는 횟수를 60회로 제한합니다. 기본값 0은 제한이
없다는 뜻이어서, 화면이 바뀌지 않아도 게임이 쉬지 않고 다시 그리느라 CPU를 많이 씁니다.
`framerateLimit`은 그리는 횟수에만 영향을 주며 update 간격은 바꾸지 않습니다.

## 게임 상태를 둘 곳

위 예제에서는 `position` 같은 상태를 `main`의 지역 변수로 두고, 람다가 이를 참조(`[&]`)로
캡처합니다. 이 지역 변수들은 `game.run()`이 끝날 때까지 살아 있으므로 안전합니다. 반대로 `run()`보다
먼저 끝나는 함수의 지역 변수를 참조로 캡처하면, 핸들러가 호출될 때 그 변수는 이미 사라진 뒤입니다.

게임이 커지면 `Game`을 상속하고 상태를 멤버 변수로 두는 편이 관리하기 쉽습니다. 상속한 클래스에서는
루프가 시작되기 직전에 한 번 호출되는 `onStart()`에서 Scene을 구성합니다.

```cpp
#include <meno/Meno.hpp>

class BouncingBall final : public meno::Game {
public:
    BouncingBall() : Game({.application = {.framerateLimit = 60}, .title = "Bouncing ball"}) {}

private:
    meno::Vec2f position_{320.f, 240.f};
    meno::Vec2f velocity_{180.f, 120.f};

    void onStart() override {
        meno::Scene& play = scenes().createScene();
        play.setUpdateHandler([this](double fixedDeltaTime) {
            position_ += velocity_ * static_cast<float>(fixedDeltaTime);
        });
        play.setDrawHandler([this](meno::Renderer& renderer) {
            renderer.drawCircle(position_, 16.f, {.fill = meno::colors::White});
        });
        (void)scenes().changeScene(play.id());
    }
};

int main() {
    BouncingBall game;
    game.run();
}
```

Scene마다 따로 둘 상태는 `Scene::setState()`로 Scene에 맡길 수도 있습니다. 이 방법은
[7장](07-scenes.md)에서 설명합니다.

## 게임 끝내기

창의 닫기 버튼을 누르거나 핸들러 안에서 `game.stop()`을 호출하면 루프가 끝납니다. 이때 활성 Scene의
exit 핸들러가 호출된 뒤 `run()`이 돌아옵니다. Esc 키로 게임을 끝내는 방법은 [4장](04-input.md)에서
다룹니다.

## 주의할 점

- `Vec2f`에 `double`을 곱하면 컴파일되지 않습니다. `fixedDeltaTime`은
  `static_cast<float>(fixedDeltaTime)`로 변환한 뒤 곱합니다.
- 게임 상태는 update 핸들러에서 바꾸고, draw 핸들러에서는 그리기만 합니다.
- 한 번에 하나의 `Game`만 `run()`할 수 있습니다. 이미 실행 중인 `Game`이 있을 때 `run()`을 호출하면
  `std::logic_error` 예외가 발생합니다.
- `framerateLimit`을 정하지 않으면 그리는 횟수에 제한이 없어 CPU를 많이 씁니다.

## 관련 예제와 API

- 예제:
  [scene_transition](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/scene_transition/main.cpp)
  (Game을 상속하고, Scene 두 개를 3초마다 전환합니다)
- API: [Game](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_game.html),
  [GameConfig](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_game_config.html),
  [Scene](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_scene.html),
  [SceneManager](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_scene_manager.html),
  [ApplicationConfig](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_application_config.html)

이전 장: [1장. 설치와 첫 빌드](01-setup.md) · 다음 장: [3장. 그리기](03-drawing.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/03-drawing.md >>>
# 3장. 그리기

이 장에서는 draw 핸들러에서 도형과 텍스트, 이미지를 그리고 색을 지정하는 방법을 다룹니다. 그리기는
모두 핸들러가 인자로 받는 `meno::Renderer`로 합니다.

## 완성 코드

에셋 파일 없이 실행되는 그리기 연습 예제입니다. 폰트 파일이 없으면 글자만 그려지지 않습니다.

```cpp
#include <meno/Meno.hpp>

#include <array>
#include <cstdint>
#include <optional>

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {640, 480},
                     .title = "Drawing",
                     .clearColor = meno::Color::fromRgb(0x1E2430)}};

    // 파일 대신 픽셀 배열로 만든 2×2 체크무늬 텍스처입니다.
    const std::array<std::uint8_t, 16> pixels{
        255, 255, 255, 255, 0,   177, 237, 255, //
        0,   177, 237, 255, 255, 255, 255, 255,
    };
    const std::optional<meno::Texture> checker = meno::Texture::fromPixels({2, 2}, pixels.data());
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");

    meno::Scene& scene = game.scenes().createScene();
    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawRect({{40.f, 40.f}, {160.f, 100.f}}, {.fill = meno::colors::White});
        renderer.drawCircle({320.f, 90.f}, 50.f,
                            {.fill = meno::colors::Transparent,
                             .outline = meno::Color::fromRgb(0x00B1ED),
                             .outlineThickness = 4.f});
        renderer.drawLine({440.f, 40.f}, {600.f, 140.f}, {.thickness = 3.f});
        renderer.drawPolygon({120.f, 260.f}, {{0.f, -50.f}, {50.f, 40.f}, {-50.f, 40.f}},
                             {.fill = meno::colors::Yellow});
        renderer.drawSector({320.f, 260.f}, 50.f, -90.f, 270.f, {.fill = meno::colors::Gray});
        if (checker) {
            renderer.draw(*checker, {.position = {460.f, 210.f}, .scale = {50.f, 50.f}});
        }
        if (font) {
            renderer.drawText(*font, "Hello, meno", {40.f, 380.f}, {.characterSize = 32});
        }
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## 좌표와 그리는 순서

- 단위는 픽셀이고 원점은 창의 왼쪽 위입니다. x는 오른쪽으로, **y는 아래쪽으로** 갈수록 커집니다.
- 각도는 도(degree) 단위이며 **시계 방향**이 양수입니다. 0도는 오른쪽 방향입니다.
- 그리기 함수를 호출한 순서대로 그려지므로 나중에 그린 것이 위에 놓입니다. 따라서 배경을 먼저,
  캐릭터와 UI를 나중에 그립니다.

카메라를 쓰면 그리기 좌표가 "월드 좌표"로 해석되어, 화면에 보이는 영역을 옮기거나 확대할 수
있습니다. 카메라를 정하지 않으면 위의 화면 좌표를 그대로 씁니다. 카메라는 [10장](10-camera.md)에서
다룹니다.

## 도형

| 함수 | 그리는 것 |
|---|---|
| `drawRect(rect, params)` | 사각형. 기본값에서는 `rect.position`이 왼쪽 위 꼭짓점 |
| `drawCircle(center, radius, params)` | 원 |
| `drawLine(from, to, params)` | 두께가 있는 선. 옵션은 `LineParams{.color, .thickness}` |
| `drawPolygon(position, points, params)` | 점을 차례로 이은 다각형. 각 점은 `position` 기준 좌표 |
| `drawSector(center, radius, startDeg, sweepDeg, params)` | 부채꼴(쿨다운 표시 등) |

선을 제외한 도형은 `ShapeParams`로 모양을 정하며, 필드는 아래 표의 순서대로 지정합니다.

| 필드 | 기본값 | 의미 |
|---|---|---|
| `fill` | 흰색 | 채우기 색 |
| `outline` | 투명 | 외곽선 색 |
| `outlineThickness` | 0 | 외곽선 두께. 양수면 바깥쪽, 음수면 안쪽 |
| `origin` | `{0, 0}` | `position`에 놓일 점이자 회전 중심 |
| `rotation` | 0 | 회전 각도(도) |

`origin`의 기준은 도형마다 다릅니다. 사각형과 다각형은 로컬 원점(사각형은 왼쪽 위 꼭짓점)이, 원과
부채꼴은 중심이 기준입니다. 사각형을 중심 기준으로 배치하고 회전하려면 `origin`을 크기의 절반으로
지정합니다.

```cpp
const meno::Vec2f size{40.f, 20.f};
renderer.drawRect({{320.f, 240.f}, size}, {.origin = size * 0.5f, .rotation = 30.f});
```

외곽선만 그리려면 `fill`을 `meno::colors::Transparent`로 두고 `outline`과 `outlineThickness`를 함께
지정합니다.

## 색

| 만드는 방법 | 예시 |
|---|---|
| 채널 값(0~255) | `meno::Color{255, 128, 0}`, 반투명은 `meno::Color{255, 128, 0, 128}` |
| 6자리 `0xRRGGBB` | `meno::Color::fromRgb(0x4FC3F7)` |
| 8자리 `0xRRGGBBAA` | `meno::Color::fromRgba(0x4FC3F780)` |
| 불투명도만 바꾸기 | `meno::colors::White.withAlpha(128)` |
| 미리 정의된 색 | `meno::colors::White`, `Black`, `Red`, `Yellow`, `Transparent` 등 |

## 텍스트

글자를 그리려면 폰트가 필요합니다. 폰트 파일은 `game.resources().loadFont()`로 한 번만 읽어 두고,
그릴 때마다 그 폰트를 넘깁니다.

```cpp
const auto font = game.resources().loadFont("assets/fonts/game.ttf"); // run() 전에 한 번
int score = 0;

play.setDrawHandler([&](meno::Renderer& renderer) {
    if (font) {
        renderer.drawText(*font, "점수: " + std::to_string(score), {16.f, 16.f},
                          {.characterSize = 24, .color = meno::colors::White});
    }
});
```

`loadFont()`는 실패하면 빈 `shared_ptr`을 돌려주므로, 사용하기 전에 `if (font)`로 확인합니다.
`TextParams`의 필드는 `characterSize`(기본 24), `color`, `origin`, `rotation`, `outlineThickness`,
`outlineColor` 순서로 지정합니다.

글자를 가운데 정렬하려면 `measureTextBounds()`로 글자가 실제로 차지하는 영역을 구한 뒤, 그 중심을
`origin`으로 지정합니다.

```cpp
meno::TextParams params{.characterSize = 48};
params.origin = renderer.measureTextBounds(*font, "GAME OVER", params).center();
renderer.drawText(*font, "GAME OVER", {320.f, 240.f}, params); // 화면 가운데
```

## 이미지

그림 파일은 `game.resources().loadTexture()`로 읽습니다. 같은 경로를 다시 요청하면 처음 읽은
텍스처를 그대로 돌려주므로, 여러 곳에서 사용해도 파일은 한 번만 읽힙니다.

```cpp
const auto player = game.resources().loadTexture("assets/player.png");

play.setDrawHandler([&](meno::Renderer& renderer) {
    if (player) {
        renderer.draw(*player, {.position = {100.f, 50.f}});
    }
});
```

`SpriteParams`의 필드는 `position`, `origin`, `scale`, `rotation`, `tint`, `source`, `flipX`,
`flipY` 순서로 지정합니다.

- `origin`은 텍스처 픽셀 단위의 기준점입니다. 기본값 `{0, 0}`이면 텍스처의 왼쪽 위 꼭짓점이
  `position`에 놓입니다.
- `tint`는 텍스처 색에 곱해지는 색입니다. 흰색이면 원본 그대로이며, `withAlpha()`로 불투명도를
  낮추면 반투명하게 그려집니다.
- 좌우를 뒤집을 때는 음수 `scale` 대신 `flipX`를 씁니다. 그러면 그려지는 위치가 바뀌지 않습니다.
- `source`로 텍스처의 일부만 그릴 수 있으며, 스프라이트 시트와 애니메이션에 이 필드를 씁니다.

픽셀 아트를 흐려지지 않게 확대하려면 보간을 끈 상태(기본값)로 둡니다. 부드럽게 확대하려면
`loadTexture(path, {.smooth = true})`로 읽습니다.

파일 없이 픽셀 배열로 텍스처를 만들 때는 `meno::Texture::fromPixels()`를 씁니다. 위 완성 코드의
체크무늬가 그 예입니다.

## 주의할 점

- `fromRgb()`에는 6자리만 넣습니다. 8자리를 넣으면 상수식에서는 컴파일 오류가 나고, 그 밖에서는 눈에
  띄도록 자홍색이 됩니다. 반투명한 색은 `withAlpha()`나 `fromRgba()`로 만듭니다.
- 외곽선 색 `outline`의 기본값은 투명입니다. 따라서 `outlineThickness`만 바꾸면 외곽선이 보이지
  않습니다.
- `drawPolygon()`은 볼록한 다각형만 올바르게 채웁니다. 오목한 모양은 외곽선만 그리거나, 여러 개의
  볼록한 조각으로 나눠 그립니다.
- 한글을 그리려면 한글 글리프가 있는 폰트를 씁니다(예: Windows의 `C:/Windows/Fonts/malgun.ttf`).
  폰트에 글리프가 없는 글자는 화면에 나타나지 않습니다.
- 파일은 `run()` 전이나 Scene을 준비할 때 한 번 읽어 두고, draw 핸들러에서는 그리기만 합니다.
- 옵션 구조체(`ShapeParams`, `TextParams`, `SpriteParams` 등)의 필드는 선언된 순서대로 지정해야
  컴파일됩니다.

## 관련 예제와 API

- 예제:
  [render_basics](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/render_basics/main.cpp)
  (그리기 API를 한 화면에서 모두 보여 줍니다)
- API: [Renderer](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_renderer.html),
  [ShapeParams](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_shape_params.html),
  [TextParams](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_text_params.html),
  [SpriteParams](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_sprite_params.html),
  [Color](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_color.html),
  [Texture](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_texture.html),
  [Font](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_font.html),
  [ResourceManager](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_resource_manager.html)

이전 장: [2장. 첫 게임: Game과 Scene](02-first-game.md) · 다음 장: [4장. 입력](04-input.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/04-input.md >>>
# 4장. 입력

이 장에서는 키보드와 마우스 입력을 읽는 방법을 익히고, 마지막으로 지금까지 배운 내용을 모아
퐁(Pong)을 만듭니다.

## 입력 읽기

입력은 `game.window().input()`으로 읽습니다. 창이 프레임마다 입력 상태를 갱신하므로 이벤트를 직접
처리할 필요가 없습니다. 게임 규칙에 쓰는 입력은 update 핸들러에서 읽습니다.

```cpp
play.setUpdateHandler([&](double fixedDeltaTime) {
    const meno::Input& input = game.window().input();
    const float step = 300.f * static_cast<float>(fixedDeltaTime);
    if (input.isDown(meno::Key::Right)) { // 누르고 있는 동안 계속
        position.x += step;
    }
    if (input.wasPressed(meno::Key::Space)) { // 누른 순간 한 번만
        velocity.y = -400.f;
    }
    if (input.wasPressed(meno::Key::Escape)) {
        game.stop(); // 게임을 끝냅니다.
    }
});
```

| 함수 | true를 돌려주는 때 |
|---|---|
| `isDown(key)` | 눌려 있는 동안 계속 |
| `wasPressed(key)` | 누른 순간 한 번 |
| `wasReleased(key)` | 뗀 순간 한 번 |

세 함수 모두 `meno::Key`와 `meno::MouseButton`을 인자로 받을 수 있습니다. 키를 누르고 있을 때
운영체제가 보내는 반복 입력은 무시되므로, `wasPressed()`는 한 번 누를 때마다 한 번만 true를
돌려줍니다.

## "이번 틱" 규칙

`wasPressed()`, `wasReleased()`, `mouseWheelDelta()`, `text()`는 **이번 틱**에 일어난 일만 알려
줍니다. 동작 이름으로 묻는 `wasActionPressed()`와 `wasActionReleased()`도 마찬가지입니다. update
핸들러 안에서는 update 한 번이 한 틱입니다. 한 프레임에서 update가 여러 번 호출되더라도, 한 번 누른
키는 그 뒤 처음 실행되는 update에서만 `wasPressed()`가 true입니다. 따라서 점프가 두 번 일어나거나
입력을 놓치는 일이 없습니다.

draw 핸들러처럼 update 밖에서 읽을 때는 한 프레임이 한 틱입니다. 그러므로 게임 규칙은 update
핸들러에서 처리합니다.

## 마우스

```cpp
const meno::Input& input = game.window().input();
if (input.wasPressed(meno::MouseButton::Left)) {
    const meno::Vec2f clicked{input.mousePosition()}; // 창 왼쪽 위 기준 픽셀
}
const float wheel = input.mouseWheelDelta(); // 이번 틱에 굴린 양. 위로 굴리면 양수
```

`mousePosition()`은 정수 벡터(`Vec2i`)를 돌려주므로, `Vec2f`와 함께 계산하려면 위처럼 변환합니다. 이
좌표는 창을 기준으로 한 값이어서 카메라를 쓰면 월드 좌표와 달라집니다. 이때는
`game.renderer().screenToWorld()`로 변환합니다([10장](10-camera.md)).

## 동작 이름으로 묶기

한 동작에 키를 여러 개 쓰려면 "jump" 같은 동작 이름에 키를 묶습니다. 묶인 키 중 하나라도 눌려 있으면
그 동작이 눌린 것으로 봅니다.

```cpp
meno::Input& input = game.window().input();
input.bind("jump", meno::Key::Space);
input.bind("jump", meno::Key::W);
input.bind("jump", meno::MouseButton::Left);

play.setUpdateHandler([&](double) {
    if (game.window().input().wasActionPressed("jump")) {
        // Space를 누른 채 W를 더 눌러도 한 번만 들어옵니다.
    }
});
```

`isActionDown()`, `wasActionPressed()`, `wasActionReleased()`는 각각 `isDown()`, `wasPressed()`,
`wasReleased()`에 해당합니다. `unbind("jump")`는 그 동작에 묶인 키를 모두 풉니다. 키 하나만 따로 풀
수는 없으므로, 묶인 키를 바꾸려면 `unbind()`로 푼 뒤 다시 `bind()`합니다.

## 글자 입력

이름 입력처럼 글자를 입력받을 때는 `text()`를 씁니다. `text()`는 이번 틱에 입력된 글자를 UTF-8
문자열로 돌려주며, 한글은 조합이 끝난 글자만 들어옵니다.

```cpp
std::string name;

play.setUpdateHandler([&](double) {
    const meno::Input& input = game.window().input();
    name += input.text();
    if (input.wasPressed(meno::Key::Backspace) && !name.empty()) {
        // UTF-8에서 한 글자는 여러 바이트일 수 있으므로, 뒤따르는 바이트(10xxxxxx)까지 지웁니다.
        while (!name.empty() && (static_cast<unsigned char>(name.back()) & 0xC0) == 0x80) {
            name.pop_back();
        }
        if (!name.empty()) {
            name.pop_back();
        }
    }
});
```

백스페이스, 엔터, 탭 같은 제어 문자는 `text()`에 들어오지 않으므로, 위처럼 `wasPressed()`로 따로
처리합니다.

## 완성 코드: 퐁

왼쪽 막대는 W·S 키로, 오른쪽 막대는 ↑·↓ 키로 움직입니다. 공이 막대에 닿으면 튕겨 나가고, 화면의
왼쪽이나 오른쪽 끝을 지나가면 상대가 1점을 얻습니다. 충돌은 두 사각형이 겹치는지 검사하는
`Rectf::intersects()`로 판정합니다.

```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <cmath>
#include <string>

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {800, 600},
                     .title = "Pong",
                     .clearColor = meno::Color::fromRgb(0x0D1117)}};
    const meno::Vec2f field{game.window().size()};
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");

    const meno::Vec2f paddleSize{16.f, 96.f};
    const float paddleSpeed = 420.f;
    const float ballRadius = 8.f;
    meno::Rectf left{{40.f, field.y / 2.f - paddleSize.y / 2.f}, paddleSize};
    meno::Rectf right{{field.x - 40.f - paddleSize.x, left.position.y}, paddleSize};
    meno::Vec2f ball = field * 0.5f;
    meno::Vec2f velocity{360.f, 180.f};
    int leftScore = 0;
    int rightScore = 0;

    const auto serve = [&](float direction) {
        ball = field * 0.5f;
        velocity = {360.f * direction, 180.f};
    };

    meno::Scene& play = game.scenes().createScene();
    play.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        const float dt = static_cast<float>(fixedDeltaTime);
        if (input.wasPressed(meno::Key::Escape)) {
            game.stop();
        }

        // 막대를 움직이고, 화면 밖으로 나가지 않게 위치를 제한합니다.
        const auto move = [&](meno::Rectf& paddle, meno::Key up, meno::Key down) {
            if (input.isDown(up)) {
                paddle.position.y -= paddleSpeed * dt;
            }
            if (input.isDown(down)) {
                paddle.position.y += paddleSpeed * dt;
            }
            paddle.position.y = std::clamp(paddle.position.y, 0.f, field.y - paddle.size.y);
        };
        move(left, meno::Key::W, meno::Key::S);
        move(right, meno::Key::Up, meno::Key::Down);

        ball += velocity * dt;
        if (ball.y < ballRadius || ball.y > field.y - ballRadius) {
            ball.y = std::clamp(ball.y, ballRadius, field.y - ballRadius);
            velocity.y = -velocity.y;
        }

        // 공을 감싸는 사각형이 막대와 겹치면, 그 막대에서 멀어지는 쪽으로 튕깁니다.
        const meno::Rectf ballBox{ball - meno::Vec2f{ballRadius, ballRadius},
                                  {ballRadius * 2.f, ballRadius * 2.f}};
        if (ballBox.intersects(left)) {
            velocity.x = std::abs(velocity.x);
        } else if (ballBox.intersects(right)) {
            velocity.x = -std::abs(velocity.x);
        }

        if (ball.x < 0.f) {
            ++rightScore;
            serve(1.f);
        } else if (ball.x > field.x) {
            ++leftScore;
            serve(-1.f);
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.drawLine({field.x / 2.f, 0.f}, {field.x / 2.f, field.y},
                          {.color = meno::colors::Gray, .thickness = 2.f});
        renderer.drawRect(left);
        renderer.drawRect(right);
        renderer.drawCircle(ball, ballRadius, {.fill = meno::Color::fromRgb(0x00B1ED)});
        if (font) {
            const std::string score =
                std::to_string(leftScore) + "   " + std::to_string(rightScore);
            meno::TextParams params{.characterSize = 40};
            params.origin = renderer.measureTextBounds(*font, score, params).center();
            renderer.drawText(*font, score, {field.x / 2.f, 40.f}, params);
        }
    });

    (void)game.scenes().changeScene(play.id());
    game.run();
}
```

점수는 `assets/fonts/game.ttf`가 있을 때만 표시됩니다. 가지고 있는 TTF 폰트 파일을 이 이름으로
복사해 두면 됩니다.

이로써 1부를 마칩니다. 게임 루프와 그리기, 입력, 사각형 충돌 판정만으로도 간단한 게임 하나를
처음부터 끝까지 만들 수 있습니다.

## 주의할 점

- 키 이름은 자판에 적힌 글자를 따릅니다. 예를 들어 AZERTY 자판에서 QWERTY의 Q 위치에 있는 키를
  누르면 `Key::A`가 들어옵니다.
- `Num0`~`Num9`는 자판 위쪽 줄의 숫자 키이고, `Numpad0`~`Numpad9`는 숫자 패드의 키입니다.
- 창이 포커스를 잃으면 눌려 있던 키와 버튼은 모두 뗀 것으로 처리되며, 그 틱에서 `wasReleased()`가
  true를 돌려줍니다.
- `Rectf::intersects()`는 경계만 맞닿은 경우를 겹치지 않은 것으로 봅니다.

## 관련 예제와 API

- 예제:
  [input_demo](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/input_demo/main.cpp)
  (키보드, 마우스, 동작 묶기를 한 화면에서 보여 줍니다)
- API: [Input](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_input.html),
  [입력 모듈](https://seoultech-fiat-lux.github.io/Meno/group__input.html)(`Key`, `MouseButton`),
  [Rect](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_rect.html),
  [Vec2](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_vec2.html)

이전 장: [3장. 그리기](03-drawing.md) · 다음 장: [5장. GameObject와 컴포넌트](05-gameobjects.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/05-gameobjects.md >>>
# 5장. GameObject와 컴포넌트

1부에서는 게임 상태를 변수에 두고 draw 핸들러에서 직접 그렸습니다. 이 장에서는 화면에 놓이는 물체를
`GameObject`로 만들고, 위치와 그림, 직접 정의한 데이터를 컴포넌트로 붙여 관리합니다.

## 완성 코드

눈송이가 계속 내리는 장면입니다. 눈송이마다 GameObject를 하나씩 만들고, 화면 아래로 벗어나면
지웁니다.

```cpp
#include <meno/Meno.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

// 초당 이동 거리를 담는 컴포넌트입니다.
struct Velocity : meno::Component {
    meno::Vec2f value;
    explicit Velocity(meno::Vec2f initial) : value(initial) {}
};

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {640, 480},
                     .title = "Snow",
                     .clearColor = meno::Color::fromRgb(0x0D1117)}};
    const meno::Vec2f field{game.window().size()};

    // 모든 눈송이가 함께 쓰는 4×4 흰색 텍스처입니다.
    std::array<std::uint8_t, 4 * 4 * 4> pixels{};
    pixels.fill(255);
    std::shared_ptr<const meno::Texture> flake;
    if (auto texture = meno::Texture::fromPixels({4, 4}, pixels.data())) {
        flake = std::make_shared<const meno::Texture>(std::move(*texture));
    }

    std::mt19937 random{std::random_device{}()};
    std::uniform_real_distribution<float> randomX{0.f, field.x};
    std::uniform_real_distribution<float> randomScale{1.f, 3.f};
    std::vector<meno::GameObject::GameObjectID> flakes;
    double untilNext = 0.0;

    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double fixedDeltaTime) {
        // 0.1초마다 눈송이를 하나씩 만듭니다.
        untilNext -= fixedDeltaTime;
        if (untilNext <= 0.0 && flake) {
            untilNext = 0.1;
            const float scale = randomScale(random);
            meno::GameObject& object = scene.createGameObject();
            object.transform().position = {randomX(random), -16.f};
            object.transform().scale = scale;
            object.sprite().setTexture(flake);
            object.addComponent<Velocity>(meno::Vec2f{0.f, 40.f * scale});
            (void)scene.setLayer(object.id(), scale > 2.f ? 1 : 0); // 큰 눈송이를 앞에 그립니다.
            flakes.push_back(object.id());
        }

        // 눈송이를 움직이고, 화면 아래로 벗어난 것은 지웁니다.
        const float dt = static_cast<float>(fixedDeltaTime);
        std::erase_if(flakes, [&](meno::GameObject::GameObjectID id) {
            meno::GameObject* object = scene.getGameObject(id);
            if (object == nullptr) {
                return true;
            }
            if (const Velocity* velocity = object->getComponent<Velocity>()) {
                object->transform().position += velocity->value * dt;
            }
            if (object->transform().position.y > field.y) {
                (void)scene.deleteGameObject(id);
                return true;
            }
            return false;
        });
    });
    scene.setDrawHandler([&](meno::Renderer& renderer) {
        // draw 핸들러에서 그린 것은 GameObject보다 위에 놓이므로, 눈송이가 땅에 가려집니다.
        renderer.drawRect({{0.f, field.y - 40.f}, {field.x, 40.f}},
                          {.fill = meno::Color::fromRgb(0x334155)});
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## GameObject의 기본 구성

`scene.createGameObject()`는 GameObject를 만들어 Scene에 넣고, 그 참조를 돌려줍니다. GameObject는
Scene이 소유합니다. 모든 GameObject는 다음 두 가지 기본 컴포넌트를 가집니다.

| 기본 컴포넌트 | 접근 | 내용 |
|---|---|---|
| `Transform` | `object.transform()` | 위치(`position`), 배율(`scale`), 회전(`rotation`) |
| `Sprite` | `object.sprite()` | 그림(텍스처)과 그리기 옵션(`offset`, `origin`, `tint` 등) |

`transform().position`은 GameObject의 왼쪽 위 꼭짓점(로컬 원점)이 놓일 위치입니다. 배율과 회전도 이
꼭짓점을 중심으로 적용됩니다. `scale`은 가로와 세로에 함께 쓰이는 값 하나입니다.

Sprite는 텍스처가 있을 때만 그려집니다. `sprite().setTexture()`에는
`ResourceManager::loadTexture()`가 돌려준 `shared_ptr`을 넘깁니다. 위 예제처럼 코드로 만든 텍스처를
여러 GameObject가 함께 쓰게 하려면 `std::make_shared`로 감싸서 넘깁니다.

## 그리는 순서와 레이어

Scene은 GameObject를 다음 순서로 그린 뒤에 draw 핸들러를 호출합니다.

1. 레이어가 작은 것부터 그립니다. 레이어의 기본값은 0이며 `scene.setLayer(id, layer)`로 바꿉니다.
2. 레이어가 같으면 먼저 만든 것(ID가 작은 것)부터 그립니다.

나중에 그린 것이 위에 놓이기 때문에 레이어가 큰 GameObject가 앞에 보입니다. draw 핸들러는
GameObject를 모두 그린 뒤에 호출되므로, 점수와 같은 UI는 여기서 그립니다.

## 컴포넌트 붙이기

위치와 그림 외의 데이터는 `meno::Component`를 상속한 구조체로 정의하여 붙입니다.

```cpp
struct Health : meno::Component {
    int value = 3;
};

meno::GameObject& player = play.createGameObject();
player.addComponent<Health>();

if (Health* health = player.getComponent<Health>()) {
    health->value -= 1;
}
```

- `addComponent<T>(인자...)`는 T를 만들어 붙이고 그 참조를 돌려줍니다. 인자는 T의 생성자에 그대로
  전달됩니다.
- `getComponent<T>()`는 붙어 있는 T의 포인터를, T가 없으면 `nullptr`을 돌려줍니다.
- 컴포넌트는 타입마다 하나씩만 붙습니다. 같은 타입을 다시 붙이면 이전 컴포넌트는 파괴됩니다.
- 붙인 컴포넌트는 GameObject가 소유하며, 떼어 내는 함수는 없습니다.

meno의 컴포넌트에는 매 프레임마다 호출되는 함수가 없습니다. 컴포넌트는 데이터만 담고, 그 데이터를
쓰는 동작은 Scene의 update 핸들러에서 처리합니다. 위 예제에서도 update 핸들러가 `Velocity`를 꺼내
위치에 더합니다.

## GameObject 목록 관리하기

Scene은 GameObject 목록을 돌려주는 함수를 제공하지 않습니다. 따라서 여러 GameObject를 함께 다루려면
위 예제의 `flakes`처럼 ID를 직접 모아 두고, `scene.getGameObject(id)`로 꺼내 씁니다.
`getGameObject()`는 그 ID의 GameObject가 없으면 `nullptr`을 돌려주므로, 이미 지운 객체를 실수로 쓰는
일을 막을 수 있습니다.

GameObject는 `scene.deleteGameObject(id)`로 지웁니다. 지우면 붙어 있던 컴포넌트도 함께 파괴됩니다.

## update 중에 만들고 지우기

update 핸들러 안에서 GameObject를 만들거나 지우면, 변경 사항은 그 update가 끝날 때 반영됩니다.

| 동작 | update 핸들러 안 | 핸들러 밖 |
|---|---|---|
| `createGameObject()` | 바로 쓸 수 있지만, 그 update가 끝난 뒤부터 그려짐 | 바로 그려짐 |
| `deleteGameObject(id)` | 그 update가 끝날 때 파괴됨 | 바로 파괴됨 |

update 안에서 지운 GameObject는 그 update가 끝날 때까지 남아 있으므로, 같은 update 안에서는 계속
`getGameObject()`로 찾을 수 있습니다. 반대로 핸들러 밖에서 지우면 바로 파괴되므로, 그 객체를
가리키던 참조와 포인터도 그 즉시 쓸 수 없게 됩니다.

## 주의할 점

- `getComponent<T>()`는 정확히 같은 타입만 찾습니다. 예를 들어 `BoxCollider`를 붙였다면
  `getComponent<meno::Collider>()`는 `nullptr`을 돌려줍니다.
- 같은 타입의 컴포넌트를 다시 붙이면 이전 컴포넌트는 파괴되므로, 앞서 받아 둔 참조를 써서는 안
  됩니다.
- `Transform`과 `Sprite`는 `addComponent()`로 붙일 수 없으며, `transform()`과 `sprite()`로
  접근합니다.
- GameObject의 참조나 포인터를 오래 보관해야 한다면 대신 ID를 저장하고, 쓸 때마다
  `getGameObject()`로 꺼냅니다.

## 관련 예제와 API

- 예제:
  [gameobject_demo](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/gameobject_demo/main.cpp)
  (Scene, GameObject, Sprite로 장면을 구성합니다)
- API: [GameObject](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_game_object.html),
  [Component](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_component.html),
  [Transform](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_transform.html),
  [Sprite](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_sprite.html),
  [Scene](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_scene.html)

이전 장: [4장. 입력](04-input.md) · 다음 장: [6장. 충돌 판정](06-collision.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/06-collision.md >>>
# 6장. 충돌 판정

이 장에서는 GameObject에 콜라이더를 붙여 서로 겹치는지 판정하고, 겹쳤을 때 막거나 줍는 처리를
구현합니다.

## 완성 코드

방향키로 파란 상자를 움직여 노란 동전을 모읍니다. 가운데 벽은 통과할 수 없습니다.

```cpp
#include <meno/Meno.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// size 크기의 단색 텍스처를 만듭니다. round가 true면 원 바깥을 투명하게 둡니다.
std::shared_ptr<const meno::Texture> makeTexture(meno::Vec2u size, meno::Color color, bool round) {
    std::vector<std::uint8_t> pixels(std::size_t{size.x} * size.y * 4);
    const float radius = static_cast<float>(size.x) / 2.f;
    for (unsigned int y = 0; y < size.y; ++y) {
        for (unsigned int x = 0; x < size.x; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - radius;
            const float dy = static_cast<float>(y) + 0.5f - radius;
            const bool inside = !round || dx * dx + dy * dy <= radius * radius;
            const std::size_t i = (std::size_t{y} * size.x + x) * 4;
            pixels[i] = color.r;
            pixels[i + 1] = color.g;
            pixels[i + 2] = color.b;
            pixels[i + 3] = inside ? color.a : std::uint8_t{0};
        }
    }
    auto texture = meno::Texture::fromPixels(size, pixels.data());
    return texture ? std::make_shared<const meno::Texture>(std::move(*texture)) : nullptr;
}

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {640, 480},
                     .title = "Coins",
                     .clearColor = meno::Color::fromRgb(0x1E2430)}};
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");
    meno::Scene& scene = game.scenes().createScene();

    meno::GameObject& player = scene.createGameObject();
    player.transform().position = {80.f, 220.f};
    player.sprite().setTexture(makeTexture({32, 32}, meno::Color::fromRgb(0x00B1ED), false));
    player.addComponent<meno::BoxCollider>(meno::Vec2f{0.f, 0.f}, meno::Vec2f{32.f, 32.f});

    meno::GameObject& wall = scene.createGameObject();
    wall.transform().position = {304.f, 120.f};
    wall.sprite().setTexture(makeTexture({32, 240}, meno::colors::Gray, false));
    wall.addComponent<meno::BoxCollider>(meno::Vec2f{0.f, 0.f}, meno::Vec2f{32.f, 240.f});

    const auto coinTexture = makeTexture({16, 16}, meno::colors::Yellow, true);
    std::vector<meno::GameObject::GameObjectID> coins;
    for (const meno::Vec2f position :
         {meno::Vec2f{200.f, 100.f}, meno::Vec2f{200.f, 360.f}, meno::Vec2f{440.f, 100.f},
          meno::Vec2f{440.f, 360.f}, meno::Vec2f{540.f, 230.f}}) {
        meno::GameObject& coin = scene.createGameObject();
        coin.transform().position = position;
        coin.sprite().setTexture(coinTexture);
        coin.addComponent<meno::CircleCollider>(meno::Vec2f{8.f, 8.f}, 8.f); // 중심과 반지름
        coins.push_back(coin.id());
    }
    int collected = 0;

    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        const float step = 240.f * static_cast<float>(fixedDeltaTime);
        meno::Vec2f move{0.f, 0.f};
        if (input.isDown(meno::Key::Left)) {
            move.x -= step;
        }
        if (input.isDown(meno::Key::Right)) {
            move.x += step;
        }
        if (input.isDown(meno::Key::Up)) {
            move.y -= step;
        }
        if (input.isDown(meno::Key::Down)) {
            move.y += step;
        }

        // 축마다 따로 움직여 보고, 벽과 겹치면 그 축의 이동만 되돌립니다.
        meno::Vec2f& position = player.transform().position;
        position.x += move.x;
        if (meno::intersects(player, wall)) {
            position.x -= move.x;
        }
        position.y += move.y;
        if (meno::intersects(player, wall)) {
            position.y -= move.y;
        }

        // 닿은 동전을 지우고 점수를 올립니다.
        std::erase_if(coins, [&](meno::GameObject::GameObjectID id) {
            const meno::GameObject* coin = scene.getGameObject(id);
            if (coin == nullptr || !meno::intersects(player, *coin)) {
                return false;
            }
            (void)scene.deleteGameObject(id);
            ++collected;
            return true;
        });
    });
    scene.setDrawHandler([&](meno::Renderer& renderer) {
        if (font) {
            const std::string text = coins.empty() ? "Clear!" : std::to_string(collected) + " / 5";
            renderer.drawText(*font, text, {16.f, 16.f});
        }
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## 콜라이더 붙이기

콜라이더는 GameObject에 붙이는 컴포넌트입니다. 사각형 콜라이더 `BoxCollider`와 원형 콜라이더
`CircleCollider` 두 가지가 있습니다.

| 콜라이더 | 생성자 | `offset`이 가리키는 곳 |
|---|---|---|
| `BoxCollider` | `BoxCollider(offset, size)` | 사각형의 왼쪽 위 꼭짓점 |
| `CircleCollider` | `CircleCollider(offset, radius)` | 원의 중심 |

`offset`과 크기는 GameObject의 로컬 좌표로 지정합니다. 로컬 좌표의 원점은 GameObject의 왼쪽 위
꼭짓점이므로, 32×32 텍스처를 쓰는 GameObject에 같은 크기의 BoxCollider를 붙이려면 `offset`을
`{0, 0}`, `size`를 `{32, 32}`로 지정합니다. 마찬가지로 16×16 동전의 중심은 `{8, 8}`입니다.

콜라이더는 GameObject의 Transform을 따릅니다. `transform().scale`을 키우면 콜라이더도 함께 커지고,
`rotation`을 주면 BoxCollider도 회전합니다. 따라서 텍스처를 4배로 키운 GameObject라도 콜라이더의
크기는 텍스처의 원래 픽셀 크기로 지정합니다.

## 겹치는지 판정하기

`meno::intersects(a, b)`는 두 GameObject의 콜라이더가 하나라도 겹치면 true를 돌려줍니다. 경계만
맞닿은 경우는 겹치지 않은 것으로 봅니다. 점 하나가 사각형 안에 있는지는
`BoxCollider::contains(worldPoint)`로 확인합니다.

```cpp
if (const auto* box = player.getComponent<meno::BoxCollider>()) {
    const meno::Vec2f mouse{game.window().input().mousePosition()};
    if (box->contains(mouse)) {
        // 마우스가 player 위에 있습니다.
    }
}
```

GameObject 없이 사각형 두 개만 비교할 때는 4장의 퐁처럼 `Rectf::intersects()`를 씁니다.

## 겹쳤을 때 처리하기

meno는 겹침 여부만 판정합니다. 겹친 물체를 밀어내거나 속도를 바꾸는 처리는 게임 코드가 맡습니다.
자주 쓰는 방법은 다음과 같습니다.

- **막기:** 위 예제처럼 움직인 뒤 겹치면 이동을 되돌립니다. x축과 y축을 따로 검사하면 벽에 닿은
  채로도 벽을 따라 미끄러지듯 움직일 수 있습니다.
- **착지:** 떨어지던 물체가 바닥과 겹치면 위치를 바닥 위로 맞추고 세로 속도를 0으로 둡니다. 이번
  update에서 움직이기 전에 바닥보다 위에 있었는지도 함께 확인하면, 옆에서 부딪힌 것을 착지로
  오인하지 않습니다.
  [jumping_box](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/jumping_box/main.cpp)
  예제가 이 방식을 씁니다.
- **튕기기:** 4장의 퐁처럼 부딪힌 축의 속도 부호를 바꿉니다. 부딪힌 물체에서 멀어지는 방향으로
  부호를 정하면, 한 번 겹친 상태가 여러 update 동안 이어져도 다시 안쪽으로 튕기지 않습니다.
- **줍기:** 위 예제의 동전처럼 겹친 GameObject를 지우고 점수를 올립니다.

## 주의할 점

- `intersects()`는 `BoxCollider`와 `CircleCollider`만 검사합니다. `Collider`를 상속해 직접 만든
  타입은 판정에 쓰이지 않습니다.
- 크기나 반지름이 0 이하인 콜라이더와 배율이 0인 GameObject에 붙은 콜라이더는 어떤 것과도 겹치지
  않습니다.
- `getComponent<meno::Collider>()`로는 BoxCollider나 CircleCollider를 찾을 수 없습니다.
  `getComponent<meno::BoxCollider>()`처럼 정확한 타입으로 찾습니다.
- 물체가 아주 빠르면 update 한 번에 얇은 벽을 통째로 건너뛸 수 있습니다. 이를 막으려면 속도를
  줄이거나 벽을 두껍게 만듭니다.

## 관련 예제와 API

- 예제:
  [jumping_box](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/jumping_box/main.cpp)
  (중력과 착지를 게임 코드로 처리합니다)
- API: [Collider](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_collider.html),
  [BoxCollider](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_box_collider.html),
  [CircleCollider](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_circle_collider.html),
  [씬 모듈](https://seoultech-fiat-lux.github.io/Meno/group__scene.html)(`intersects`),
  [Rect](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_rect.html)

이전 장: [5장. GameObject와 컴포넌트](05-gameobjects.md) · 다음 장: [7장. 씬 전환과 씬 상태](07-scenes.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/07-scenes.md >>>
# 7장. 씬 전환과 씬 상태

이 장에서는 타이틀, 게임, 결과 화면을 각각 Scene으로 만들고 서로 전환합니다. Scene마다 상태를 맡기는
방법과 게임을 처음부터 다시 시작하는 방법도 다룹니다.

## 완성 코드

타이틀에서 Enter를 누르면 게임이 시작되어 10초 동안 Space를 누른 횟수를 셉니다. 시간이 다 되면
결과를 보여 주고, 결과 화면에서 Enter를 누르면 타이틀로 돌아갑니다. 폰트가 없으면 글자 없이 배경색과
시간 막대만 보입니다.

```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <memory>
#include <string>

// 게임 Scene에 맡길 상태입니다.
struct PlayState {
    int score = 0;
    double timeLeft = 10.0;
};

int main() {
    meno::Game game{
        {.application = {.framerateLimit = 60}, .windowSize = {640, 480}, .title = "Scenes"}};
    const auto font = game.resources().loadFont("assets/fonts/game.ttf");

    meno::SceneManager& scenes = game.scenes();
    meno::Scene& title = scenes.createScene();
    meno::Scene& play = scenes.createScene();
    meno::Scene& result = scenes.createScene();
    play.setState(std::make_unique<PlayState>());

    // 배경을 칠하고, 폰트가 있으면 화면 가운데에 글자를 씁니다.
    const auto background = [](meno::Renderer& renderer, meno::Color color) {
        renderer.drawRect({{0.f, 0.f}, {640.f, 480.f}}, {.fill = color});
    };
    const auto caption = [&](meno::Renderer& renderer, const std::string& text, float y) {
        if (font) {
            meno::TextParams params{.characterSize = 32};
            params.origin = renderer.measureTextBounds(*font, text, params).center();
            renderer.drawText(*font, text, {320.f, y}, params);
        }
    };
    const auto enterPressed = [&] { return game.window().input().wasPressed(meno::Key::Enter); };

    // 타이틀: Enter를 누르면 게임을 시작합니다.
    title.setUpdateHandler([&](double) {
        if (enterPressed()) {
            (void)scenes.changeScene(play.id()); // 이 update가 끝난 직후에 바뀝니다.
        }
    });
    title.setDrawHandler([&](meno::Renderer& renderer) {
        background(renderer, meno::Color::fromRgb(0x1E3A5F));
        caption(renderer, "Press Enter", 240.f);
    });

    // 게임: 들어올 때마다 상태를 초기화하므로, 다시 시작해도 처음부터 진행됩니다.
    play.setEnterHandler([&] { *play.getState<PlayState>() = PlayState{}; });
    play.setUpdateHandler([&](double fixedDeltaTime) {
        PlayState& state = *play.getState<PlayState>();
        if (game.window().input().wasPressed(meno::Key::Space)) {
            ++state.score;
        }
        state.timeLeft -= fixedDeltaTime;
        if (state.timeLeft <= 0.0) {
            (void)scenes.changeScene(result.id());
        }
    });
    play.setDrawHandler([&](meno::Renderer& renderer) {
        const PlayState& state = *play.getState<PlayState>();
        background(renderer, meno::Color::fromRgb(0x1E2430));
        const float ratio = static_cast<float>(std::max(state.timeLeft, 0.0) / 10.0);
        renderer.drawRect({{40.f, 400.f}, {560.f * ratio, 16.f}},
                          {.fill = meno::Color::fromRgb(0x00B1ED)}); // 남은 시간
        caption(renderer, std::to_string(state.score), 220.f);
    });

    // 결과: 게임 Scene의 상태에서 점수를 읽어 보여 줍니다.
    result.setUpdateHandler([&](double) {
        if (enterPressed()) {
            (void)scenes.changeScene(title.id());
        }
    });
    result.setDrawHandler([&](meno::Renderer& renderer) {
        background(renderer, meno::Color::fromRgb(0x5F1E2A));
        caption(renderer, "Score: " + std::to_string(play.getState<PlayState>()->score), 220.f);
        caption(renderer, "Press Enter", 280.f);
    });

    (void)scenes.changeScene(title.id());
    game.run();
}
```

## Scene을 여러 개 두기

Scene은 타이틀, 게임, 결과처럼 화면 단위로 하나씩 만듭니다. update 핸들러가 호출되고 화면에 그려지는
Scene은 활성 Scene뿐이며, 나머지 Scene은 멈춰 있습니다. 멈춰 있는 Scene도 자신의 GameObject와 상태를
그대로 가지고 있으므로, 다시 활성화하면 떠날 때의 상태에서 이어집니다.

`createScene()`이 돌려준 참조는 그 Scene을 지우거나 게임이 끝날 때까지 유효합니다. 위 예제처럼 지역
변수에 받아 두고 핸들러에서 써도 됩니다.

## 전환 시점

`scenes.changeScene(id)`는 호출한 곳에 따라 전환되는 시점이 다릅니다.

| 호출한 곳 | 전환 시점 |
|---|---|
| 핸들러 밖(`run()` 전 등) | 바로 |
| update 핸들러 안 | 그 update가 끝난 직후 |
| enter·exit·draw 핸들러 안 | 다음 update가 시작될 때 |

전환할 때는 이전 Scene의 exit 핸들러가 먼저, 새 Scene의 enter 핸들러가 나중에 호출됩니다. 핸들러
안에서 전환을 여러 번 요청하면 마지막 요청만 반영됩니다.

핸들러 안에서 `changeScene()`을 호출해도 그 핸들러는 끝까지 실행됩니다. 위 예제에서 시간이 다 되어
결과 Scene으로 전환을 요청한 update도, 요청 뒤에 남은 코드를 모두 실행한 다음에 끝납니다.

## enter와 exit 핸들러

enter 핸들러는 Scene이 활성화될 때마다 호출됩니다. 위 예제처럼 여기서 점수와 타이머를 초기화하면,
결과 화면에서 다시 시작해도 게임이 처음부터 진행됩니다.

exit 핸들러는 다른 Scene으로 바뀔 때와 게임이 끝날 때 호출됩니다. 여기서는 배경음악을 멈추는 것처럼
그 Scene에서 시작한 일을 정리합니다. exit 핸들러가 예외를 던지면 전환이 취소되어 이전 Scene이 그대로
활성 상태로 남습니다.

## Scene 상태

여러 Scene에 걸친 상태는 1부처럼 `main`의 지역 변수나 `Game`을 상속한 클래스의 멤버로 둘 수
있습니다. 특정 Scene에만 속한 상태는 `setState()`로 그 Scene에 맡길 수도 있습니다.

```cpp
struct PlayState {
    int score = 0;
};

play.setState(std::make_unique<PlayState>());

if (PlayState* state = play.getState<PlayState>()) {
    state->score += 10;
}
```

- `setState()`는 상태 객체의 소유권을 Scene으로 옮깁니다. 상태는 Scene이 사라질 때 함께 파괴되며,
  Scene을 전환하는 것만으로는 사라지지 않습니다.
- `getState<T>()`는 등록한 상태를 포인터로 돌려줍니다. 상태를 등록하지 않았거나 T가 등록할 때의
  타입과 다르면 `nullptr`을 돌려줍니다.
- 한 Scene에는 상태를 하나만 등록할 수 있습니다. 상태가 이미 등록된 Scene의 `setState()`를 다시
  호출하면 `std::logic_error` 예외가 발생합니다. 상태를 초기화하려면 위 예제처럼 등록된 객체에 새
  값을 대입합니다.

## Scene 지우기

더는 쓰지 않는 Scene은 `scenes.deleteScene(id)`로 지웁니다. 그 Scene의 GameObject와 상태도 함께
파괴됩니다. 단, 활성 Scene의 ID를 넘기거나 Scene의 핸들러 안에서 호출하면 지우지 않고 `false`를
돌려줍니다. 게임이 끝날 때는 `Game`이 남은 Scene을 모두 정리하므로 따로 지울 필요가 없습니다.

## 주의할 점

- 핸들러 안에서 요청한 전환은 바로 일어나지 않습니다. 요청한 뒤에도 그 핸들러는 이전 Scene에서 계속
  실행됩니다.
- `wasPressed()`는 한 틱에서만 true이므로, 타이틀에서 누른 Enter가 바로 다음 Scene의 update에서 다시
  감지되지 않습니다.
- 활성 Scene은 지울 수 없습니다. 다른 Scene으로 전환한 뒤에 지웁니다.
- `getState<T>()`는 실패하면 `nullptr`을 돌려줍니다. 상태를 등록했는지 확실하지 않다면 결과를 확인한
  뒤 씁니다.

## 관련 예제와 API

- 예제:
  [scene_transition](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/scene_transition/main.cpp)
  (Scene 두 개를 3초마다 전환합니다)
- API: [SceneManager](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_scene_manager.html),
  [Scene](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_scene.html)

이전 장: [6장. 충돌 판정](06-collision.md) · 다음 장: [8장. 오디오](08-audio.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/08-audio.md >>>
# 8장. 오디오

이 장에서는 효과음과 배경음악을 재생합니다. meno의 오디오는 짧은 효과음용 `Sound`, 긴 배경음악용
`Music`, GameObject에 붙이는 `AudioSource` 컴포넌트로 나뉩니다.

## 완성 코드

Space를 누르면 효과음이 나고, M을 누르면 배경음악이 멈추거나 다시 재생됩니다. 아래 두 파일이
필요하며, 파일이 없으면 그 소리만 나지 않습니다.

```
assets/sfx/jump.wav
assets/music/theme.ogg
```

```cpp
#include <meno/Meno.hpp>

#include <cstddef>
#include <optional>
#include <vector>

int main() {
    meno::Game game{{.application = {.framerateLimit = 60}, .title = "Audio"}};

    // 같은 효과음을 겹쳐 낼 수 있도록, 버퍼 하나를 공유하는 Sound를 네 개 둡니다.
    std::vector<meno::Sound> jumps;
    if (const auto buffer = game.resources().loadSoundBuffer("assets/sfx/jump.wav")) {
        for (int i = 0; i < 4; ++i) {
            jumps.emplace_back(buffer);
        }
    }
    std::size_t nextJump = 0;

    // Music은 재생하는 동안 살아 있어야 하므로 run()보다 오래 사는 곳에 둡니다.
    std::optional<meno::Music> theme = meno::Music::openFromFile("assets/music/theme.ogg");
    if (theme) {
        theme->setLooping(true);
        theme->setVolume(0.5f);
    }

    meno::Scene& scene = game.scenes().createScene();
    scene.setEnterHandler([&] {
        if (theme) {
            theme->play();
        }
    });
    scene.setExitHandler([&] {
        if (theme) {
            theme->stop();
        }
    });
    scene.setUpdateHandler([&](double) {
        const meno::Input& input = game.window().input();
        if (input.wasPressed(meno::Key::Space) && !jumps.empty()) {
            jumps[nextJump].play();
            nextJump = (nextJump + 1) % jumps.size();
        }
        if (input.wasPressed(meno::Key::M) && theme) {
            if (theme->isPlaying()) {
                theme->pause();
            } else {
                theme->play();
            }
        }
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## Sound, SoundBuffer, Music

| 타입 | 용도 | 파일을 읽는 방식 |
|---|---|---|
| `SoundBuffer` | 효과음 데이터 | 파일 전체를 메모리에 올림 |
| `Sound` | 효과음 재생기 | `SoundBuffer`를 공유해 재생 |
| `Music` | 배경음악 | 재생하면서 파일을 조금씩 읽음 |

지원하는 파일 형식은 WAV, OGG, FLAC, MP3입니다.

효과음 데이터는 `game.resources().loadSoundBuffer()`로 읽습니다. 같은 경로를 다시 요청하면 처음 읽은
버퍼를 그대로 돌려주므로, 여러 `Sound`가 데이터 하나를 공유합니다. `Sound`가 버퍼를 함께 소유하기
때문에 버퍼의 수명은 신경 쓰지 않아도 됩니다. 재생 위치와 볼륨은 각 `Sound`가 따로 가집니다.

`Sound::loadFromFile()`과 `Music::openFromFile()`은 실패하면 `std::nullopt`를, `loadSoundBuffer()`는
빈 `shared_ptr`을 돌려줍니다. 모두 예외를 던지지 않으므로 사용하기 전에 결과를 확인합니다.

## 재생 함수

`Sound`, `Music`, `AudioSource`는 같은 재생 함수를 가집니다.

| 함수 | 동작 |
|---|---|
| `play()` | 멈춰 있으면 처음부터, 일시정지 중이면 이어서, 재생 중이면 처음부터 다시 재생 |
| `pause()` | 일시정지. 다시 `play()`하면 이어서 재생 |
| `stop()` | 멈추고 재생 위치를 처음으로 되돌림 |
| `isPlaying()` | 재생 중이면 true. 일시정지나 정지 상태면 false |
| `setVolume(v)` | 볼륨을 0(무음)~1(최대)로 지정. 범위를 벗어난 값은 범위 안으로 자름 |

`Music::setLooping(true)`를 호출하면 음악이 끝난 뒤 처음부터 다시 재생됩니다. 기본으로는 반복하지
않으며, `Sound`에는 반복 재생 기능이 없습니다.

## 같은 효과음 겹쳐 내기

`play()`는 이미 재생 중인 소리를 처음부터 다시 시작합니다. 따라서 `Sound` 하나로는 같은 효과음을
겹쳐 낼 수 없습니다. 점프나 총소리를 연달아 낼 때처럼 소리가 겹쳐야 한다면, 위 예제와 같이 버퍼
하나를 공유하는 `Sound`를 여러 개 두고 차례로 재생합니다.

## GameObject에 소리 붙이기

GameObject마다 소리를 두려면 `AudioSource` 컴포넌트를 붙입니다. `AudioSource`는 `Sound` 하나를
소유하며, GameObject가 사라지면 재생 중이던 소리도 멈춥니다.

```cpp
const auto hit = game.resources().loadSoundBuffer("assets/sfx/hit.wav");

meno::GameObject& enemy = play.createGameObject();
auto& audio = enemy.addComponent<meno::AudioSource>();
audio.setSound(meno::Sound{hit});
audio.setVolume(0.8f);

// 적이 맞았을 때
audio.play();
```

볼륨은 `AudioSource`가 기억하므로 `setSound()`로 음원을 바꿔도 유지됩니다.
`AudioSource::loadFromFile()`도 있지만, 이 함수는 호출할 때마다 파일을 새로 읽습니다. 같은 소리를
여러 GameObject에서 쓴다면 위처럼 `loadSoundBuffer()`로 읽은 버퍼를 공유합니다.

## 주의할 점

- `Sound`와 `Music`은 소멸하는 순간 소리도 멈춥니다. 재생하는 동안 살아 있는 곳(게임 클래스의 멤버,
  `run()`보다 오래 사는 변수 등)에 둡니다. 함수 안의 지역 변수로 만들어 `play()`하면 함수가 끝날 때
  바로 멈춥니다.
- `Music`은 재생하면서 파일을 읽으므로, `Music`이 살아 있는 동안 원본 파일을 옮기거나 지워서는 안
  됩니다.
- `Sound::loadFromFile()`은 호출할 때마다 파일을 새로 읽습니다. 같은 효과음을 여러 곳에서 쓴다면
  `loadSoundBuffer()`로 버퍼를 공유합니다.

## 관련 예제와 API

- API: [Sound](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_sound.html),
  [SoundBuffer](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_sound_buffer.html),
  [Music](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_music.html),
  [AudioSource](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_audio_source.html),
  [ResourceManager](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_resource_manager.html)

이로써 2부를 마칩니다. GameObject와 컴포넌트, 충돌 판정, 여러 Scene, 소리를 조합하면 슈팅, 스네이크,
플래피 버드 같은 게임을 만들 수 있습니다.

이전 장: [7장. 씬 전환과 씬 상태](07-scenes.md) · 다음 장: [9장. 리소스 관리](09-resources.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/09-resources.md >>>
# 9장. 리소스 관리

이 장에서는 `ResourceManager`가 텍스처, 폰트, 효과음 파일을 어떻게 읽고 공유하는지 정리하고, 게임에
필요한 파일을 시작할 때 한 번에 읽어 확인하는 방법을 다룹니다.

## 완성 코드

이 예제는 게임을 시작하기 전에 필요한 파일을 모두 읽고, 하나라도 없으면 빠진 파일을 알린 뒤
종료합니다. 파일이 모두 있으면 그림과 글자를 그리고, Space를 누르면 효과음을 냅니다.

```cpp
#include <meno/Meno.hpp>

#include <iostream>
#include <string>
#include <vector>

int main() {
    meno::Game game{{.application = {.framerateLimit = 60}, .title = "Resources"}};
    meno::ResourceManager& resources = game.resources();

    const auto player = resources.loadTexture("assets/player.png");
    const auto font = resources.loadFont("assets/fonts/game.ttf");
    const auto jumpBuffer = resources.loadSoundBuffer("assets/sfx/jump.wav");

    // 읽지 못한 파일을 모아 알리고, 게임을 시작하지 않습니다.
    std::vector<std::string> missing;
    const auto require = [&](bool loaded, const char* path) {
        if (!loaded) {
            missing.emplace_back(path);
        }
    };
    require(player != nullptr, "assets/player.png");
    require(font != nullptr, "assets/fonts/game.ttf");
    require(jumpBuffer != nullptr, "assets/sfx/jump.wav");
    if (!missing.empty()) {
        for (const std::string& path : missing) {
            std::cerr << "Missing file: " << path << '\n';
        }
        return 1;
    }

    meno::Sound jump{jumpBuffer};
    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double) {
        if (game.window().input().wasPressed(meno::Key::Space)) {
            jump.play();
        }
    });
    scene.setDrawHandler([&](meno::Renderer& renderer) {
        renderer.draw(*player, {.position = {100.f, 100.f}});
        renderer.drawText(*font, "Press Space", {100.f, 300.f});
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## ResourceManager가 하는 일

`ResourceManager`는 파일 경로마다 리소스를 한 번만 읽어 두고 공유하는 캐시입니다. `Game`을 쓰면
`game.resources()`로 이 캐시를 얻습니다.

| 함수 | 돌려주는 값 |
|---|---|
| `loadTexture(path, options)` | `std::shared_ptr<const Texture>` |
| `loadFont(path)` | `std::shared_ptr<const Font>` |
| `loadSoundBuffer(path)` | `std::shared_ptr<const SoundBuffer>` |

- 같은 경로를 다시 요청하면 처음 읽었을 때와 같은 `shared_ptr`을 돌려줍니다. 따라서 여러
  GameObject가 같은 텍스처를 써도 파일은 한 번만 읽힙니다.
- 읽지 못하면 빈 `shared_ptr`을 돌려줍니다. 실패는 캐시하지 않으므로 파일을 고친 뒤 다시 요청하면
  새로 읽습니다.
- 돌려받은 리소스는 `const`이므로, 텍스처 보간은 `setSmooth()` 대신
  `loadTexture(path, {.smooth = true})`처럼 옵션으로 정합니다. 옵션이 다르면 같은 경로라도 따로
  읽습니다.

## 경로가 같은지 판단하는 방법

`ResourceManager`는 경로를 글자 단위로만 정리해 같은 파일인지 판단합니다. `./`와 `../`를 정리하고
경로 구분자를 통일하므로 `assets/./player.png`와 `assets/player.png`는 같은 리소스입니다. 하지만
다음 경우에는 같은 파일이라도 따로 읽습니다.

- 상대 경로와 절대 경로
- 대소문자가 다른 경로. 대소문자는 실제 파일 이름과 똑같이 씁니다. Windows에서는 대소문자가 틀려도
  파일이 열리지만, Linux에서는 열리지 않습니다.

한 게임 안에서는 경로를 한 가지 방식으로 적는 것이 안전합니다. 상대 경로의 기준은 1장에서 설명한
현재 작업 디렉터리입니다.

## 리소스의 수명

한 번 읽은 리소스는 `ResourceManager`가 사라질 때까지 캐시에 남습니다. 캐시를 비우는 함수는
없으므로, 게임 도중에 큰 리소스를 계속 새로 읽는다면 메모리 사용량에 주의합니다.

돌려받은 `shared_ptr`이 남아 있으면 `ResourceManager`가 사라진 뒤에도 그 리소스는 해제되지 않습니다.
또한 캐시가 `Font`를 가지고 있는 동안에는 폰트 파일이 계속 열려 있으므로, 게임 실행 중에 폰트 파일을
옮기거나 지워서는 안 됩니다.

## ResourceManager를 거치지 않는 방법

각 리소스 타입에는 파일을 직접 읽는 함수도 있습니다. 이 함수들은 캐시를 쓰지 않으므로 호출할 때마다
파일을 새로 읽습니다.

| 함수 | 돌려주는 값 | 알맞은 용도 |
|---|---|---|
| `Texture::loadFromFile(path)` | `std::optional<Texture>` | 한 곳에서만 쓰는 이미지 |
| `Font::loadFromFile(path)` | `std::optional<Font>` | 한 곳에서만 쓰는 폰트 |
| `SoundBuffer::loadFromFile(path)` | `std::optional<SoundBuffer>` | 한 곳에서만 쓰는 효과음 |
| `Music::openFromFile(path)` | `std::optional<Music>` | 배경음악(캐시하지 않음) |

`Music`은 재생하면서 파일을 읽으므로 `ResourceManager`가 다루지 않습니다. 배경음악은 8장에서처럼
`Music::openFromFile()`로 엽니다.

## 주의할 점

- 리소스를 사용하기 전에 돌려받은 `shared_ptr`이 비어 있지 않은지 확인합니다. 위 예제처럼 시작할 때
  한 번에 확인해 두면 그 뒤에는 다시 확인하지 않아도 됩니다.
- 매 프레임 `loadTexture()`를 호출해도 캐시 덕분에 파일을 다시 읽지는 않지만, 경로를 정리해 캐시에서
  찾는 비용은 매번 듭니다. 따라서 리소스는 시작할 때 읽어 변수에 받아 둡니다.
- `ResourceManager`를 여러 스레드에서 동시에 사용해서는 안 됩니다.

## 관련 예제와 API

- API:
  [ResourceManager](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_resource_manager.html),
  [TextureOptions](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_texture_options.html),
  [Texture](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_texture.html),
  [Font](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_font.html),
  [SoundBuffer](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_sound_buffer.html),
  [Music](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_music.html)

이전 장: [8장. 오디오](08-audio.md) · 다음 장: [10장. 카메라와 좌표](10-camera.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/10-camera.md >>>
# 10장. 카메라와 좌표

이 장에서는 화면보다 넓은 월드를 카메라로 비춥니다. 플레이어를 따라가는 카메라, 확대와 축소, 마우스
좌표를 월드 좌표로 바꾸는 방법, 화면에 고정된 UI를 그리는 방법을 다룹니다.

## 완성 코드

2000×2000 크기의 월드를 방향키로 돌아다닙니다. 카메라가 플레이어를 따라가고, 마우스 휠로 확대하거나
축소할 수 있으며, 클릭한 곳에는 표시가 남습니다. 왼쪽 위의 막대는 확대 배율을 보여 주는 UI입니다.

```cpp
#include <meno/Meno.hpp>

#include <algorithm>
#include <vector>

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {800, 600},
                     .title = "Camera",
                     .clearColor = meno::Color::fromRgb(0x0D1117)}};
    const meno::Rectf world{{0.f, 0.f}, {2000.f, 2000.f}};
    meno::Vec2f player{1000.f, 1000.f};
    std::vector<meno::Vec2f> markers;
    meno::Camera2D camera{.center = player, .size = {800.f, 600.f}};
    game.renderer().setCamera(camera);
    // UI를 그리는 카메라입니다. 월드 카메라와 size가 같고 움직이지 않습니다.
    const meno::Camera2D hud{.center = {400.f, 300.f}, .size = {800.f, 600.f}};

    meno::Scene& scene = game.scenes().createScene();
    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        const float step = 300.f * static_cast<float>(fixedDeltaTime);
        if (input.isDown(meno::Key::Left)) {
            player.x -= step;
        }
        if (input.isDown(meno::Key::Right)) {
            player.x += step;
        }
        if (input.isDown(meno::Key::Up)) {
            player.y -= step;
        }
        if (input.isDown(meno::Key::Down)) {
            player.y += step;
        }
        player.x = std::clamp(player.x, world.left(), world.right());
        player.y = std::clamp(player.y, world.top(), world.bottom());

        // 휠로 확대·축소하고, 카메라가 플레이어를 따라가게 합니다.
        camera.zoom = std::clamp(camera.zoom + input.mouseWheelDelta() * 0.1f, 0.5f, 3.f);
        camera.center = player;
        game.renderer().setCamera(camera);

        // 클릭한 곳의 월드 좌표에 표시를 남깁니다.
        if (input.wasPressed(meno::MouseButton::Left)) {
            const meno::Vec2f mouse{input.mousePosition()};
            markers.push_back(game.renderer().screenToWorld(mouse));
        }
    });
    scene.setDrawHandler([&](meno::Renderer& renderer) {
        // 화면에 보이는 칸만 그립니다.
        const meno::Rectf visible = renderer.visibleWorldBounds();
        const float cell = 100.f;
        for (float y = world.top(); y < world.bottom(); y += cell) {
            for (float x = world.left(); x < world.right(); x += cell) {
                const meno::Rectf tile{{x, y}, {cell, cell}};
                if (!tile.intersects(visible)) {
                    continue;
                }
                const bool even =
                    (static_cast<int>(x / cell) + static_cast<int>(y / cell)) % 2 == 0;
                renderer.drawRect(tile, {.fill = even ? meno::Color::fromRgb(0x1E2430)
                                                      : meno::Color::fromRgb(0x253041)});
            }
        }
        for (const meno::Vec2f marker : markers) {
            renderer.drawCircle(marker, 6.f, {.fill = meno::colors::Yellow});
        }
        renderer.drawCircle(player, 16.f, {.fill = meno::Color::fromRgb(0x00B1ED)});

        // UI는 HUD 카메라로 그립니다. 핸들러가 끝나면 Game이 월드 카메라를 되돌립니다.
        renderer.setCamera(hud);
        renderer.drawRect({{16.f, 16.f}, {40.f * camera.zoom, 8.f}}, {.fill = meno::colors::White});
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## 월드 좌표와 화면 좌표

카메라를 정하지 않으면 그리기 좌표는 화면 좌표, 즉 창의 왼쪽 위를 원점으로 하는 픽셀 좌표입니다.
`renderer.setCamera(camera)`로 카메라를 정하면 그 뒤의 그리기 좌표는 **월드 좌표**로 해석됩니다.
월드 좌표의 단위와 방향은 화면 좌표와 같지만(y는 아래쪽으로 커짐), 화면에 비치는 영역은 카메라가
정합니다.

## Camera2D

`Camera2D`는 다음 필드를 가진 값 타입이며, 지정 초기화는 이 순서대로 씁니다.

| 필드 | 기본값 | 의미 |
|---|---|---|
| `center` | `{0, 0}` | 화면 가운데에 올 월드 좌표 |
| `size` | `{0, 0}` | 화면에 담을 월드 영역의 크기. `{0, 0}`이면 창의 픽셀 크기를 그대로 씀 |
| `zoom` | 1 | 배율. 1보다 크면 확대되어 보이는 영역이 `size / zoom`으로 좁아짐 |
| `rotation` | 0 | 회전 각도(도). 카메라가 시계 방향으로 돌면 화면 속 월드는 반시계 방향으로 돎 |
| `viewport` | `{0, 0, 1, 1}` | 창에서 이 카메라가 차지할 영역. 창 전체를 1로 보는 비율 |
| `fit` | `Letterbox` | `size`와 창의 종횡비가 다를 때의 처리 |

`size`는 "이 해상도로 설계했다"는 기준입니다. `size`를 정해 두면 창 크기가 바뀌어도 같은 영역이
비칩니다. `fit`의 기본값인 `Letterbox`는 종횡비를 지키는 대신 창의 남는 부분을 띠로 비워 두고,
`Stretch`는 띠 없이 창을 채우는 대신 그림을 늘립니다. 확대와 축소는 `size`를 바꾸는 대신 `zoom`으로
합니다.

띠는 프레임마다 화면을 지우는 색인 `GameConfig::clearColor`로 채워집니다. 띠와 배경의 색을 다르게
하려면 `clearColor`는 띠 색으로 두고, 배경은 게임 화면이나 월드 크기의 사각형으로 그립니다. draw
핸들러에서 그린 것은 GameObject 위에 놓이므로, GameObject를 함께 쓴다면 배경은 레이어가 가장 작은
GameObject의 Sprite로 둡니다.

## 카메라를 정하는 곳

`Renderer`는 카메라의 복사본을 저장합니다. 따라서 카메라 값을 바꿨다면 `setCamera()`를 다시 호출해야
반영됩니다.

- **월드 카메라**는 update 핸들러에서 정합니다. 위 예제처럼 플레이어를 움직인 뒤 `camera.center`를
  옮기고 `game.renderer().setCamera(camera)`를 호출합니다. 한 번 정한 카메라는 다시 바꿀 때까지
  유지됩니다.
- **UI**는 draw 핸들러에서 HUD 카메라(다음 절)로 바꾼 뒤에 그립니다. `Game`은 draw 핸들러가 끝나면
  카메라를 핸들러가 호출되기 전의 상태로 되돌리므로, 다음 프레임의 월드 그리기에는 영향이 없습니다.

GameObject의 Sprite는 draw 핸들러보다 먼저, 그 시점의 월드 카메라로 그려집니다.

## 화면에 고정된 UI

점수나 체력 막대처럼 카메라가 움직여도 화면의 같은 자리에 있어야 하는 UI는 **HUD 카메라**로
그립니다. HUD 카메라는 월드 카메라와 `size`가 같고, `center`를 `size`의 절반에 고정한 카메라입니다.
이 카메라로 그리면 UI 좌표의 원점은 띠를 뺀 게임 화면의 왼쪽 위가 됩니다. 창 크기가 바뀌면 UI도 게임
화면과 함께 커지거나 작아지므로, 창을 크게 키우면 글자가 조금 흐려질 수 있습니다.

UI의 클릭을 판정할 때는 마우스 위치도 HUD 카메라의 좌표로 바꿉니다. update 핸들러에서
`game.renderer().screenToWorld()`는 월드 카메라를 기준으로 변환하므로, 카메라를 인자로 받는
`meno::screenToWorld()`를 씁니다.

```cpp
const meno::Camera2D hud{.center = {400.f, 300.f}, .size = {800.f, 600.f}};
const meno::Rectf button{{16.f, 64.f}, {160.f, 48.f}};

play.setUpdateHandler([&](double) {
    // 마우스 위치를 HUD 카메라의 좌표로 바꿔서 판정합니다.
    const meno::Input& input = game.window().input();
    const meno::Vec2f mouse{input.mousePosition()};
    const meno::Vec2f point = meno::screenToWorld(hud, game.renderer().framebufferSize(), mouse);
    if (input.wasPressed(meno::MouseButton::Left) && button.contains(point)) {
        // 버튼을 눌렀습니다.
    }
});
play.setDrawHandler([&](meno::Renderer& renderer) {
    // 월드를 그린 뒤에 HUD 카메라로 바꿉니다.
    renderer.setCamera(hud);
    renderer.drawText(*font, "SCORE 0", {16.f, 16.f}, {.characterSize = 32});
    renderer.drawRect(button, {.fill = meno::colors::Gray});
});
```

`renderer.resetCamera()`는 그리기 좌표를 창의 픽셀 좌표로 되돌립니다. 이 좌표는 띠를 따르지
않으므로, 창의 종횡비가 바뀌면 (16, 16)에 그린 UI가 게임 화면 밖의 띠 위에 놓일 수 있습니다.
`resetCamera()`는 창 모서리에 붙어야 하는 표시를 그리거나, `viewportPixels()`로 구한 픽셀 영역에
그릴 때 씁니다.

## 마우스 위치를 월드 좌표로

`input.mousePosition()`은 창 기준 픽셀 좌표를 돌려줍니다. 카메라가 움직였거나 확대되어 있으면 이
좌표는 월드 좌표와 다르므로, `renderer.screenToWorld()`로 변환합니다. update 핸들러에서는
`game.renderer()`로 렌더러를 얻습니다.

```cpp
const meno::Vec2f mouse{game.window().input().mousePosition()};
const meno::Vec2f worldPoint = game.renderer().screenToWorld(mouse);
```

반대로 월드의 한 점이 창의 어디에 그려지는지는 `worldToScreen()`으로 구합니다. `Letterbox`의 띠
위에서 클릭하면 보이는 영역 밖의 월드 좌표가 나오므로, 필요하면
`meno::viewportPixels(camera, renderer.framebufferSize()).contains(mouse)`로 먼저 확인합니다.

## 보이는 것만 그리기

`renderer.visibleWorldBounds()`는 지금 카메라에 보이는 월드 영역을 돌려줍니다. 위 예제처럼 이 영역과
겹치지 않는 물체를 건너뛰면 넓은 월드도 가볍게 그릴 수 있습니다. 카메라가 회전해 있으면 실제보다
조금 넓은 영역을 돌려주므로, 보이는 물체를 놓치지는 않습니다.

## 화면 분할과 미니맵

`viewport`를 지정하면 카메라가 비추는 장면이 창의 일부에만 그려집니다. 카메라마다 `setCamera()`를
호출한 뒤 월드를 다시 그리면 화면 분할이나 미니맵을 만들 수 있습니다.

```cpp
const meno::Vec2f hero{200.f, 300.f};
const meno::Vec2f rival{1800.f, 300.f};
const meno::Camera2D leftView{
    .center = hero, .size = {400.f, 600.f}, .viewport = {0.f, 0.f, 0.5f, 1.f}};
const meno::Camera2D rightView{
    .center = rival, .size = {400.f, 600.f}, .viewport = {0.5f, 0.f, 0.5f, 1.f}};

play.setDrawHandler([&](meno::Renderer& renderer) {
    for (const meno::Camera2D& view : {leftView, rightView}) {
        renderer.setCamera(view);
        // 월드를 카메라마다 다시 그립니다.
        renderer.drawCircle(hero, 16.f, {.fill = meno::colors::White});
        renderer.drawCircle(rival, 16.f, {.fill = meno::colors::Red});
    }
    renderer.resetCamera();
});
```

Scene이 자동으로 그리는 GameObject에는 그 시점의 카메라 하나만 적용됩니다. 화면 분할에 GameObject를
함께 쓰려면 draw 핸들러에서 카메라마다 `object.draw(renderer)`를 호출해 직접 그립니다.

## 레터박스와 화면 분할

`viewport`는 창 전체에 대한 비율이고, `Letterbox`는 카메라마다 따로 적용됩니다. 따라서 위 예제의 두
화면은 각자 띠를 가지며, 창을 가로로 늘리면 두 화면 사이에도 띠가 생깁니다. 두 화면을 하나의 게임
화면 안에 붙여 두려면, 게임 화면이 창에서 차지하는 픽셀 영역을 `viewportPixels()`로 구한 뒤 그
안에서 `viewport`를 계산합니다.

```cpp
// 800×600 게임 화면 안의 사각형(게임 화면 좌표)을 창 전체에 대한 viewport 비율로 바꿉니다.
const auto viewportInGame = [](meno::Vec2u framebuffer, const meno::Rectf& inGame) {
    const meno::Rectf area =
        meno::viewportPixels(meno::Camera2D{.size = {800.f, 600.f}}, framebuffer);
    const float scaleX = area.size.x / 800.f;
    const float scaleY = area.size.y / 600.f;
    const auto width = static_cast<float>(framebuffer.x);
    const auto height = static_cast<float>(framebuffer.y);
    return meno::Rectf{(area.left() + inGame.left() * scaleX) / width,
                       (area.top() + inGame.top() * scaleY) / height,
                       inGame.size.x * scaleX / width, inGame.size.y * scaleY / height};
};
const meno::Camera2D hud{.center = {400.f, 300.f}, .size = {800.f, 600.f}};

play.setDrawHandler([&](meno::Renderer& renderer) {
    // 창 크기가 바뀔 수 있으므로 viewport는 프레임마다 계산합니다.
    const meno::Vec2u framebuffer = renderer.framebufferSize();
    meno::Camera2D leftView{.center = {200.f, 300.f}, .size = {400.f, 600.f}};
    meno::Camera2D rightView{.center = {1800.f, 300.f}, .size = {400.f, 600.f}};
    leftView.viewport = viewportInGame(framebuffer, {0.f, 0.f, 400.f, 600.f});
    rightView.viewport = viewportInGame(framebuffer, {400.f, 0.f, 400.f, 600.f});
    for (const meno::Camera2D& view : {leftView, rightView}) {
        renderer.setCamera(view);
        // 월드를 카메라마다 다시 그립니다.
    }
    // 두 화면의 경계선처럼 게임 화면에 붙는 UI는 HUD 카메라로 그립니다.
    renderer.setCamera(hud);
    renderer.drawRect({{398.f, 0.f}, {4.f, 600.f}});
});
```

미니맵처럼 게임 화면의 일부에만 월드를 비추는 카메라도 같은 방법으로 `viewport`를 정합니다.

## 주의할 점

- 카메라 값을 바꾼 뒤에는 `setCamera()`를 다시 호출해야 화면에 반영됩니다.
- draw 핸들러 안에서 바꾼 카메라는 핸들러가 끝나면 되돌아갑니다. 따라서 월드 카메라는 update
  핸들러에서 정합니다.
- `resetCamera()`의 화면 좌표는 띠를 따르지 않습니다. 게임 화면에 붙어 있어야 하는 UI는 HUD 카메라로
  그립니다.
- 띠는 `clearColor`로 채워집니다. 배경색이 띠와 다르면 배경을 사각형으로 그립니다.
- `Camera2D`의 `rotation`은 스프라이트의 `rotation`과 반대로 보입니다. 같은 +45도라도 카메라를
  돌리면 화면 속 월드는 반시계 방향으로 돕니다.
- 지정 초기화는 `center`, `size`, `zoom`, `rotation`, `viewport`, `fit` 순서로 씁니다.

## 관련 예제와 API

- 예제:
  [render_basics](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/render_basics/main.cpp)
  (월드 카메라와 미니맵을 함께 보여 줍니다)
- API: [Camera2D](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_camera2_d.html),
  [Renderer](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_renderer.html),
  [수학 모듈](https://seoultech-fiat-lux.github.io/Meno/group__math.html)(`CameraFit`,
  `viewportPixels`, `screenToWorld` 등)

이전 장: [9장. 리소스 관리](09-resources.md) · 다음 장: [11장. 스프라이트 애니메이션](11-animation.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/11-animation.md >>>
# 11장. 스프라이트 애니메이션

이 장에서는 프레임 여러 장을 이어 붙인 스프라이트 시트에서 한 프레임씩 골라 그리는 방식으로
애니메이션을 만듭니다.

## 완성 코드

← → 키로 캐릭터를 움직입니다. 움직이는 동안에만 걷는 동작의 프레임이 바뀌고, 왼쪽으로 갈 때는 그림이
뒤집힙니다. 스프라이트 시트는 파일 대신 코드로 만듭니다.

```cpp
#include <meno/Meno.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

constexpr int frameSize = 16; // 프레임 한 장의 크기(픽셀)
constexpr int frameCount = 4;

// 16×16 프레임 네 장을 가로로 이은 64×16 스프라이트 시트를 만듭니다. 몸통은 모든 프레임에서 같고,
// 아래쪽의 흰 "다리"는 프레임마다 다른 위치에 그려집니다.
std::shared_ptr<const meno::Texture> makeSheet() {
    const meno::Vec2u size{frameSize * frameCount, frameSize};
    std::vector<std::uint8_t> pixels(std::size_t{size.x} * size.y * 4);
    for (unsigned int y = 0; y < size.y; ++y) {
        for (unsigned int x = 0; x < size.x; ++x) {
            const unsigned int frame = x / frameSize;
            const unsigned int local = x % frameSize;
            const bool body = y < 12 && local >= 2 && local < 14;
            const bool leg = y >= 12 && local / 4 == frame;
            const meno::Color color = body  ? meno::Color::fromRgb(0x00B1ED)
                                      : leg ? meno::colors::White
                                            : meno::colors::Transparent;
            const std::size_t i = (std::size_t{y} * size.x + x) * 4;
            pixels[i] = color.r;
            pixels[i + 1] = color.g;
            pixels[i + 2] = color.b;
            pixels[i + 3] = color.a;
        }
    }
    auto texture = meno::Texture::fromPixels(size, pixels.data());
    return texture ? std::make_shared<const meno::Texture>(std::move(*texture)) : nullptr;
}

int main() {
    meno::Game game{{.application = {.framerateLimit = 60},
                     .windowSize = {640, 480},
                     .title = "Animation",
                     .clearColor = meno::Color::fromRgb(0x1E2430)}};
    meno::Scene& scene = game.scenes().createScene();

    meno::GameObject& hero = scene.createGameObject();
    hero.transform().position = {288.f, 208.f};
    hero.transform().scale = 4.f;
    hero.sprite().setTexture(makeSheet());
    hero.sprite().source = meno::Recti{0, 0, frameSize, frameSize};

    const double frameTime = 0.12; // 프레임 한 장을 보여 주는 시간(초)
    double timer = 0.0;
    int frame = 0;

    scene.setUpdateHandler([&](double fixedDeltaTime) {
        const meno::Input& input = game.window().input();
        float direction = 0.f;
        if (input.isDown(meno::Key::Left)) {
            direction -= 1.f;
        }
        if (input.isDown(meno::Key::Right)) {
            direction += 1.f;
        }
        hero.transform().position.x += direction * 160.f * static_cast<float>(fixedDeltaTime);

        if (direction != 0.f) {
            hero.sprite().flipX = direction < 0.f;
            timer += fixedDeltaTime;
            while (timer >= frameTime) {
                timer -= frameTime;
                frame = (frame + 1) % frameCount;
            }
        } else {
            timer = 0.0;
            frame = 0; // 멈추면 첫 프레임으로 돌아갑니다.
        }
        hero.sprite().source = meno::Recti{frame * frameSize, 0, frameSize, frameSize};
    });

    (void)game.scenes().changeScene(scene.id());
    game.run();
}
```

## 스프라이트 시트와 source

스프라이트 시트는 애니메이션의 프레임을 이미지 하나에 나란히 모은 것입니다. 프레임마다 파일을 따로
두지 않으므로 텍스처를 한 번만 읽으면 됩니다.

시트에서 한 프레임만 그리려면 `source`에 그 프레임의 영역을 **텍스처 픽셀 좌표**로 지정합니다.
`source`는 `Recti{x, y, 너비, 높이}`이며, 비어 있으면 텍스처 전체가 그려집니다.

- GameObject로 그릴 때는 `object.sprite().source`를 바꿉니다.
- GameObject 없이 그릴 때는 `renderer.draw(texture, {.source = ...})`처럼 `SpriteParams::source`를
  씁니다.

프레임이 가로 한 줄로 놓여 있다면 i번째 프레임(0부터 셈)의 영역은
`Recti{i * 너비, 0, 너비, 높이}`입니다. 여러 줄로 놓여 있다면 행과 열을 계산해 `x`와 `y`를 정합니다.

## 프레임을 넘기는 시점

프레임은 update 핸들러에서 시간을 누적해 넘깁니다. 이 핸들러가 받는 `fixedDeltaTime`은 항상 같은
값이므로, 위 예제처럼 누적한 시간이 프레임 한 장의 표시 시간에 이를 때마다 다음 프레임으로 넘기면
컴퓨터 성능과 관계없이 같은 속도로 재생됩니다.

`while`로 넘기는 것은 표시 시간이 update 간격보다 짧은 경우에도 프레임을 빠뜨리지 않기 위해서입니다.
멈췄을 때 첫 프레임으로 되돌리는 처리나, 공격 동작처럼 한 번만 재생하고 멈추는 애니메이션도 같은
방식에 조건만 더하면 구현할 수 있습니다.

## 방향 바꾸기

왼쪽을 볼 때 그림을 뒤집으려면 `sprite().flipX`를 true로 둡니다. `flipX`는 음수 배율과 달리 그려지는
위치를 바꾸지 않으므로, 뒤집어도 캐릭터가 제자리에 있습니다. 위아래는 `flipY`로 뒤집습니다.

## 주의할 점

- `source`는 배율을 적용하기 전의 텍스처 픽셀 좌표입니다. `transform().scale`로 키운 크기를 넣지
  않습니다.
- 픽셀 아트 시트는 보간을 끈 상태(기본값)로 둡니다. 보간을 켜면 확대할 때 이웃한 프레임의 색이
  가장자리에 번질 수 있습니다.
- `Sprite::origin`과 `SpriteParams::origin`은 `source`로 고른 영역의 왼쪽 위를 기준으로 한
  좌표입니다.

## 관련 예제와 API

- API: [Sprite](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_sprite.html),
  [SpriteParams](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_sprite_params.html),
  [Rect](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_rect.html)

이전 장: [10장. 카메라와 좌표](10-camera.md) · 다음 장: [12장. Game 없이 루프 직접 돌리기](12-application.md)

<<< SeoulTech-Fiat-Lux/Meno@46f8934:docs/guide/12-application.md >>>
# 12장. Game 없이 루프 직접 돌리기

`Game`은 창, 렌더러, 리소스 캐시, Scene을 묶어 루프의 흐름을 정해 둡니다. 이 장에서는 그 아래 계층인
`Application`을 직접 상속해 게임 루프의 각 단계를 손수 채웁니다. 이 방식은 Scene이 필요 없는 작은
도구나 실험용 프로그램을 만들 때, 또는 `Game`이 정한 흐름과 다르게 그려야 할 때 씁니다.

## 완성 코드

공 하나가 좌우로 오갑니다. 이 예제는 일부러 update를 초당 30회로 줄이고 그리기는 초당 120회까지
허용해, 보간으로 움직임을 부드럽게 만드는 방법을 보여 줍니다. Esc를 누르면 끝납니다.

```cpp
#include <meno/Meno.hpp>

#include <cmath>

class Demo final : public meno::Application {
public:
    Demo() : Application({.fixedTimeStep = 1.0 / 30.0, .framerateLimit = 120}) {}

private:
    meno::Window window_{{640u, 480u}, "Application"};
    meno::Renderer renderer_{window_}; // Window보다 뒤에 선언해야 Window가 먼저 만들어집니다.
    meno::Vec2f previous_{100.f, 240.f};
    meno::Vec2f current_{100.f, 240.f};
    float speed_ = 240.f;

    void processEvents() override {
        window_.pollEvents();
        if (!window_.isOpen() || window_.input().wasPressed(meno::Key::Escape)) {
            stop();
        }
    }

    void update(double fixedDeltaTime) override {
        previous_ = current_;
        current_.x += speed_ * static_cast<float>(fixedDeltaTime);
        if (current_.x > 540.f) {
            speed_ = -std::abs(speed_);
        } else if (current_.x < 100.f) {
            speed_ = std::abs(speed_);
        }
    }

    void render(double interpolationAlpha) override {
        // 직전 update와 마지막 update 사이를 alpha만큼 섞은 위치에 그립니다.
        const float alpha = static_cast<float>(interpolationAlpha);
        const meno::Vec2f drawn = previous_ + (current_ - previous_) * alpha;
        renderer_.beginFrame(meno::Color::fromRgb(0x1E2430));
        renderer_.drawCircle(drawn, 20.f, {.fill = meno::Color::fromRgb(0x00B1ED)});
        renderer_.endFrame();
    }
};

int main() {
    Demo demo;
    demo.run();
}
```

## Game과 Application

| | `Game` | `Application` |
|---|---|---|
| 창과 렌더러 | 만들어 둠 | 직접 만듦(`Window`, `Renderer`) |
| 리소스 캐시, Scene | 있음 | 필요하면 직접 둠 |
| 루프 단계 | 정해져 있어 재정의할 수 없음 | 각 단계를 재정의함 |
| 게임 규칙 | Scene의 핸들러에 붙임 | `update()`에 직접 씀 |

`Game`도 `Application`을 상속한 클래스입니다. 따라서 고정 시간 간격과 프레임 제한, 그리고 한 번에
하나만 실행할 수 있다는 규칙은 두 방식에 똑같이 적용됩니다.

## 루프의 각 단계

`run()`은 루프에 들어가기 전에 `onStart()`를 한 번 호출하고, 그 뒤 프레임마다 다음 함수를 차례로
호출합니다. `stop()`으로 루프가 끝나면 마지막으로 `onStop()`을 호출합니다.

| 함수 | 호출 시점 | 할 일 |
|---|---|---|
| `onStart()` | 루프 시작 전 한 번 | 에셋 로딩 같은 준비 |
| `processEvents()` | 프레임마다 한 번, update 전 | `pollEvents()`, 창이 닫히면 `stop()` |
| `update(fixedDeltaTime)` | 프레임마다 흐른 시간에 맞춰 0번 이상 | 게임 상태 갱신 |
| `render(alpha)` | 프레임마다 한 번, update 뒤 | `beginFrame()` → 그리기 → `endFrame()` |
| `onStop()` | `stop()`으로 루프가 끝난 뒤 한 번 | 정리 |

`onStart()`에 걸린 시간은 게임 시간에 들어가지 않으므로, 로딩이 길어도 첫 프레임에 update가 한꺼번에
몰리지 않습니다. 창이 닫혔을 때 `stop()`을 호출하는 일은 직접 해야 합니다. 호출하지 않으면 창이 닫힌
뒤에도 루프가 계속 실행됩니다.

`render()`에서는 화면을 지우는 `beginFrame()`과 화면에 표시하는 `endFrame()`을 직접 호출합니다.
`beginFrame()`은 현재 창 크기에 맞춰 카메라를 다시 적용하므로, 그보다 먼저 `pollEvents()`가 창 크기
변경을 반영해 두어야 합니다. 위 표의 순서대로 구현하면 이 조건은 저절로 지켜집니다.

## 보간으로 부드럽게 그리기

update는 고정 간격으로만 호출되므로, 그리기가 update보다 자주 일어나면 같은 위치를 여러 번 그리게
되어 움직임이 끊겨 보입니다. `render()`가 받는 `interpolationAlpha`는 마지막 update 이후 다음
update까지 진행된 비율(0 이상 1 미만)입니다. 위 예제처럼 update마다 직전 상태를 저장해 두고, 두 상태
사이를 alpha만큼 섞은 위치에 그리면 움직임이 부드러워집니다. 보간하지 않는다면 이 값은 무시해도
됩니다.

## 시간 읽기

`meno::Time`의 정적 함수로 어디서든 시간을 읽을 수 있습니다. 단위는 초입니다.

| 함수 | 값 |
|---|---|
| `Time::fixedDeltaTime()` | `ApplicationConfig::fixedTimeStep` |
| `Time::deltaTime()` | 현재 update의 시간 간격. update 안에서는 `fixedDeltaTime()`과 같음 |
| `Time::elapsedTime()` | 지금까지 진행된 update 시간의 합(게임 시간) |
| `Time::frameDeltaTime()` | 이번 프레임의 실제 시간. 최대 `fixedTimeStep × maxUpdatesPerFrame` |
| `Time::realElapsedTime()` | `onStart()`가 끝난 뒤 흐른 실제 시간의 합 |

게임 시간(`elapsedTime`)은 update가 진행된 만큼만 흐릅니다. 프레임이 크게 밀려 시간이 버려지면 실제
시간(`realElapsedTime`)보다 느리게 흐르므로, 게임 규칙에는 게임 시간을 씁니다.

## 입력의 틱

`Application`에서도 4장의 "이번 틱" 규칙이 그대로 적용됩니다. `update()` 안에서는 update 한 번이 한
틱이고, `processEvents()`나 `render()`처럼 update 밖에서는 `pollEvents()`부터 다음
`pollEvents()`까지가 한 틱입니다. 위 예제는 Esc를 `processEvents()`에서 확인하는데, 이 규칙 덕분에
프레임마다 정확히 한 번만 판정됩니다.

## 가짜 시계로 테스트하기

`run(clock)`처럼 `Clock`을 넘기면 루프가 그 시계로 시간을 잽니다. 시각 함수와 대기 함수를 주입한
가짜 시계를 쓰면 실제로 기다리지 않고 원하는 만큼 시간을 진행시킬 수 있습니다. 이 방법은 루프의
동작을 자동 테스트로 확인할 때 유용합니다.

```cpp
meno::Clock::TimePoint now{};
meno::Clock clock{[&] { return now; }, [&](meno::Clock::Duration wait) { now += wait; }};
now += std::chrono::milliseconds(16);
const double seconds = clock.restart(); // 0.016
```

## 주의할 점

- `Renderer`는 자신이 그리는 `Window`보다 먼저 사라져야 합니다. 멤버로 둘 때는 `Window`를 먼저
  선언합니다.
- 창이 닫혔을 때 `stop()`을 호출하지 않으면 루프가 끝나지 않습니다.
- 한 번에 하나의 `Application`(`Game` 포함)만 `run()`할 수 있습니다.
- `update()`에서 `Vec2f`에 시간을 곱할 때는 `static_cast<float>(fixedDeltaTime)`로 변환합니다.

## 관련 예제와 API

- 예제:
  [input_demo](https://github.com/SeoulTech-Fiat-Lux/Meno/blob/v0.2.0/examples/input_demo/main.cpp)
  (`Application`을 직접 상속하고 보간해 그립니다)
- API: [Application](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_application.html),
  [ApplicationConfig](https://seoultech-fiat-lux.github.io/Meno/structmeno_1_1_application_config.html),
  [Time](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_time.html),
  [Clock](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_clock.html),
  [Window](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_window.html),
  [Renderer](https://seoultech-fiat-lux.github.io/Meno/classmeno_1_1_renderer.html)

이로써 사용자 설명서를 마칩니다. 각 클래스와 함수의 자세한 규칙은
[API 레퍼런스](https://seoultech-fiat-lux.github.io/Meno/topics.html)에서 모듈별로 찾을 수 있습니다.

이전 장: [11장. 스프라이트 애니메이션](11-animation.md)

# 빌드 템플릿 (CMakeLists.txt)

```cmake
# meno 조건 빌드 템플릿. LLM은 main.cpp만 작성하며, 이 파일은 고치지 않는다.
#
#     cmake -S . -B build -DMINIGAME_ASSETS_DIR=<공통 assets 폴더>
#     cmake --build build --target game
#     cd build && ./game        # 작업 디렉터리에 assets/가 있어야 한다
cmake_minimum_required(VERSION 3.28)
project(minigame_meno LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# 실험 동결 버전. 태그는 옮겨질 수 있으므로 커밋 해시로 고정한다.
# 동결 태그를 새로 만들면 이 해시를 바꾼다 (harness/freeze.py가 contexts/manifest.json과 대조한다).
FetchContent_Declare(
        meno
        GIT_REPOSITORY https://github.com/SeoulTech-Fiat-Lux/Meno.git
        GIT_TAG        46f8934f0f7754a7e09033c4413b6a5222d4493d # v0.2.0 (태그가 3a1b67c에서 옮겨진 뒤의 커밋)
)
# meno는 내부에서 SFML을 태그(3.1.0)로 받는다. FetchContent는 먼저 선언한 것을 쓰므로, 여기서 같은 이름으로
# 커밋 해시를 먼저 선언해 두 조건이 같은 SFML 커밋으로 빌드되게 한다. 옵션(SYSTEM, EXCLUDE_FROM_ALL)은 meno와 같다.
# sfml 템플릿의 해시와 같아야 한다 (freeze.py가 확인).
FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG        8835b6b955c96fa1ad1278a8dae6f05c4143effc # 3.1.0
        SYSTEM
        EXCLUDE_FROM_ALL
)
# meno가 SFML을 정적 라이브러리로 빌드한다(sfml 템플릿과 같은 조건).
FetchContent_MakeAvailable(meno)

add_executable(game main.cpp)
target_link_libraries(game PRIVATE meno::meno)

# 두 조건이 같은 에셋을 쓰도록 공통 폴더를 실행 파일 옆 assets/로 복사한다.
set(MINIGAME_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets" CACHE PATH "공통 에셋 폴더")
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MINIGAME_ASSETS_DIR}" "$<TARGET_FILE_DIR:game>/assets")
```

# 요구사항 문서

# 탑다운 슈터 요구사항

meno vs SFML 3 : LLM 생성 비교 실험

## 탑다운 슈터 게임 개발

- 이 문서는 LLM 생성 비교 실험에 쓰는 미니게임 중 탑다운 슈터의 요구사항을 정리함.
- 같은 명세를 meno 조건과 SFML 3.1 조건에 그대로 주며, LLM은 main.cpp 하나만 작성함.
- 창 제목은 "ARENA"임.
- 색은 '화면 색' 절과 제공된 에셋을 따름.

## Major Functional Requirements

- 탑다운 슈터의 주요 기능적 요구사항(Functional Requirement)은 다음과 같음.
  - 타이틀·플레이·결과로 이어지는 화면 흐름
  - 월드, 카메라, 창 크기 변경 대응
  - 플레이어 이동과 마우스 조준
  - 사격, 적 생성, 충돌
  - HUD, 배경음, 일시정지, 종료
- 게임 실행 후 종료까지 거치는 기본 기능이며, 자세한 요구사항은 이어지는 절에서 설명함.

## Major Non-functional Requirements

- 주요 비기능적 요구사항(Non-Functional Requirement)은 다음과 같음.
  - 제공된 템플릿에서 main.cpp 한 파일로 오류 없이 빌드되어야 함.
  - 60 FPS 제한에서 프레임레이트와 무관하게 같은 속도로 진행해야 함.
  - 입력을 놓치지 않고 다음 갱신에 반영해야 함.
  - 입력 없이 실행해도 비정상 종료 없이 명세된 흐름을 보여야 함.
  - 판정·삭제·조준·HUD가 정확하고, 오브젝트가 많아도 성능을 유지해야 함.
- 자세한 요구사항은 이어지는 절에서 설명함.

## 화면 흐름과 공통 조작

- 창은 960×540이며, 제목 표시줄에는 "ARENA"을 표시해야 함.
- 화면은 타이틀 → 플레이 → 결과 순서로 진행함.
  - 타이틀: 게임 이름 "ARENA"(창 제목과 같음)과 "PRESS ENTER"를 표시. Enter를 누르거나 2초가 지나면 플레이 시작.
  - 결과: 결과 문구, 최종 점수, "R: RESTART  ESC: QUIT"를 표시.
- 결과 화면에서 R을 누르면 게임 상태를 모두 초기화하고(프로그램이 켜져 있는 동안 유지한다고 명세에 적힌 기록은 제외) 플레이를 다시 시작함 (타이틀은 거치지 않음).
- Esc는 어느 화면에서든 프로그램을 종료함. 창 닫기 버튼도 같음.
- 화면 문구는 영문 대문자이며, 명세에 적힌 문자열을 그대로 씀.
- 글자는 assets/fonts/game.ttf로 그리며, 크기는 16의 배수(16·32·48px)로 함.
- 효과음은 명세에 지정된 사건에서, 지정된 파일로만 재생함.

## 월드와 플레이어

- 월드는 1920×1080임. 월드 경계를 사각형 외곽선으로, 120px 간격의 격자선을 배경으로 그림.
- 카메라는 960×540 영역을 보여 주며 플레이어를 화면 가운데에 두되, 월드 밖이 보이지 않도록 월드 안으로 제한함.
- 창 크기를 바꾸면 16:9 비율을 유지하고, 남는 부분은 검은 띠로 채움.
- 플레이어는 assets/textures/player.png(32×32, 오른쪽을 향한 그림)로 그리며 월드 가운데에서 시작함. 충돌 영역은 반지름 14의 원임.
- WASD 또는 방향키로 8방향으로 240 px/s 이동함. 대각선도 같은 속력이며, 플레이어 중심은 월드 경계에서 16px 안쪽에 머묾.
- 플레이어 그림은 항상 마우스 커서가 가리키는 월드 위치를 향해 회전함.

## 사격과 적

- 마우스 왼쪽 버튼을 누르고 있는 동안 0.15초마다 총알을 쏘고 assets/sfx/shoot.wav를 재생함. 총알은 assets/textures/bullet.png(8×8), 반지름 4의 원이며, 플레이어 중심에서 조준 방향으로 600 px/s로 날아감.
- 총알은 1.4초가 지나거나 월드 밖으로 나가면 삭제함.
- 적은 assets/textures/enemy.png(32×32), 반지름 14의 원임.
  - 플레이 시작 0.5초 뒤부터 1초마다 한 마리씩 생김. 생성 간격은 10초마다 0.1초씩 줄어 최소 0.3초.
  - 적은 최대 200마리이며, 200마리일 때는 생성하지 않음.
- 적은 카메라 시야를 사방으로 64px 넓힌 사각형의 테두리 위, 월드 안에 있는 무작위 점에서 생김. 매 갱신마다 플레이어를 향해 100 px/s로 움직임.

## 충돌과 화면

- 총알과 적이 겹치면 둘 다 삭제하고 점수 +10, assets/sfx/hit.wav를 재생함.
- 적과 플레이어가 겹치면 그 적을 삭제하고 HP를 1 줄이며 assets/sfx/lose.wav를 재생함. HP는 3에서 시작함.
  - 피격 뒤 1초 동안은 무적이며, 플레이어를 반투명하게 깜빡임. 무적 동안에는 적과 겹쳐도 아무 일도 없음.
  - 한 갱신에 여러 적과 겹치면 그중 하나만 처리하고, 곧바로 무적이 시작됨.
- 화면 왼쪽 위에 "HP 3  SCORE 40  TIME 12.3" 형식으로 표시함. 이 표시는 카메라 이동과 관계없이 화면에 고정됨.
- 플레이 중에는 assets/music/bgm_shooter.ogg를 볼륨 40%로 반복 재생하고, 결과 화면에서는 멈춤.
- P를 누르면 일시정지하고 "PAUSED"를 표시함. 다시 P를 누르면 이어서 진행함. 일시정지 중에는 TIME과 배경음도 멈춤.
- HP가 0이 되면 결과 화면으로 감. 문구는 "GAME OVER", "SCORE n", "TIME t"(소수 첫째 자리).

## 화면 색

- 배경: #14181F
- 격자선: #222A35, 두께 1px
- 월드 경계선: #5C6B80, 두께 2px
- 글자: 흰색 #FFFFFF
- 텍스처가 있는 요소는 텍스처의 색을 그대로 씀.

## 판정·성능과 무입력 실행

- 총알이 적을 뚫고 지나가지 않아야 함. (틱당 10px < 겹침 구간 36px)
- 1분 플레이 뒤에도 총알은 동시에 10개 이하, 적은 200마리 이하여야 함 (삭제 누락 없음).
- 카메라가 움직이거나 창 크기를 바꿔도 조준 방향이 커서를 3° 이내로 가리켜야 함.
- 창 크기를 바꿔도 HUD는 게임 화면 왼쪽 위에 그대로 있어야 함.
- 적 200마리와 총알 10개가 있을 때도 프레임 시간 95백분위가 20ms 이하여야 함 (측정 하니스로 확인).
- 무입력 실행: 적이 다가와 세 번 피격되고, 20초 안에 결과 화면으로 가야 함.
  - 예상 흐름 (실행 시작 기준, 근사값): 0초 타이틀 → 2초 플레이 시작 → 2.5초 첫 적 생성 → 약 5.5~9초 첫 피격 → 20초 안에 결과 화면

## 코드와 빌드

- 코드는 main.cpp 한 파일이며, 제공된 CMakeLists.txt와 assets/는 수정하지 않음.
- meno 조건은 meno 공개 헤더만 쓰고 SFML 헤더를 직접 include하지 않음. SFML 조건은 SFML 3.1 모듈만 씀. 두 조건 모두 C++20 표준 라이브러리 외의 라이브러리는 쓰지 않음.
- 기준 환경에서 오류 없이 빌드되어야 함.
- 에셋을 읽지 못하면 실패한 경로를 표준 에러에 출력하고 종료 코드 1로 끝내야 함.

## 실행 품질

- 프레임은 60 FPS로 제한함. 모든 속도와 시간은 초 단위이며, 프레임레이트가 달라져도 게임 진행 속도는 같아야 함.
- 키와 마우스 입력은 다음 갱신에 반영하며, 한 프레임보다 짧게 눌렀다 뗀 입력도 놓치지 않아야 함.
- 입력 없이 10초 실행하는 동안 비정상 종료나 처리되지 않은 예외가 없어야 함. Esc나 창 닫기로 끝나면 종료 코드 0.
- 기준 환경에서 프레임 시간 95백분위가 20ms 이하여야 함.
  - 이 항목은 측정 하니스로만 확인함 (플레이 채점 제외).

## 게임 실행 환경

- 기준 환경: Ubuntu 24.04, GCC 13.3, CMake 3.28 이상
- SFML 3.1.0 (meno의 백엔드와 같은 버전)
- 화면 없는 실행은 xvfb-run을 씀.
- 제공물: 조건별 CMakeLists.txt 템플릿과 공통 assets/ 폴더. 두 조건이 같은 에셋을 씀.
- 에셋은 상대 경로 assets/…로 엶. 실행 파일은 assets/와 같은 폴더에 있으며, 그 폴더를 작업 디렉터리로 하여 실행함.
- 텍스처는 제공된 크기 그대로 게임 안에서 그림.

## 제공 에셋

- 이 게임이 쓰는 에셋:
  - assets/fonts/game.ttf
  - assets/sfx/hit.wav
  - assets/sfx/lose.wav
  - assets/sfx/shoot.wav
  - assets/music/bgm_shooter.ogg
  - assets/textures/player.png (32×32)
  - assets/textures/enemy.png (32×32)
  - assets/textures/bullet.png (8×8)

## 주의 사항

- 명세의 수치와 문자열은 정확히 따라야 함.
- 색과 그림은 명세와 제공된 에셋에 정한 것을 씀.
- 명세가 모호하면 가장 단순한 해석을 택함.

# 할 일

위 요구사항 문서의 게임을 meno로 구현한 main.cpp 전체를 ```cpp 코드 블록 하나로 보내 주세요.
