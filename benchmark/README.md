# 성능 측정 (SeoulTech-Fiat-Lux/Meno#30)

스프라이트 N개를 그리는 프레임 시간을 meno로 그릴 때와 SFML을 직접 쓸 때로 나눠 잰다. 결과는 배칭을 적용할지 정하는 근거로 쓴다.
#29(개발 비용)와 따로 재는 실행 비용이다. #29의 D4에서 넘겨받은 "적 200·400마리" 상황도 여기서 잰다.

- **측정 프로그램:** 이 폴더의 `bench_app/`. Meno는 동결했으므로(`46f8934`, #29와 같은 커밋) Meno 저장소는 고치지 않는다. `bench_app`이 그 커밋을 FetchContent로 받아 공개 API로만 잰다.
- **이 폴더:** 측정 프로그램, 그것을 정해진 조건으로 돌리는 실행기, 요약·그림·보고서 도구, 측정 결과. 결과(보고서·도표 포함)는 `results/` 아래에 두고 이 저장소에 커밋한다. Meno 저장소로 옮기지 않는다.
- **결과 보고서:** `results/<날짜>-<meno 커밋>-<GPU>/report/benchmark.md` ("결과 보고서" 절). GPU가 여럿인 PC에서 결과가 섞이지 않게 폴더 이름에 GPU를 붙인다(예: `rtx-5070-ti`, `radeon`).

```
bench.toml       N 목록, 대상, 반복 수, 워밍업·기록 프레임 수, 실행 명령
bench_app/       측정 프로그램 meno_bench (C++, 동결한 meno·SFML·Tracy를 커밋 해시로 받음)
run.py           bench_app을 Release로 빌드하고 (대상, N, 반복)마다 새 프로세스로 실행, 환경과 함께 기록
summarize.py     평균·p50·p99 표(summary.md, summary.csv)와 판정 결과(summary.json)
figures.py       배칭 판단용 그림 4장과 그 값 표(figures/)
tracy.py         Tracy 캡처 도구 빌드, 캡처, 구간별 비용 표·그림(tracy/)
vsync_check.py   수직 동기화가 정말 꺼지는지 확인 (vsync_check/ 프로그램 빌드·실행)
report.py        결과 보고서와 #30 종료 댓글 초안(report/)
common.py        공용: 설정, 측정 환경(렌더러·git·빌드), 결과 폴더 읽기(대상 순서 등)
stats.py         공용: 백분위, 수직 동기화 판정 (입출력 없는 계산)
style.py         공용: 그림 색과 서식 (figures.py, tracy.py)
results/<날짜>-<meno 커밋>-<GPU>/   결과 폴더. 요약·그림·보고서만 커밋하고 원자료는 git 밖에 둔다 ("결과 폴더에서 커밋하는 것")
```

- **의존 방향:** 실행 도구(`run.py`, `tracy.py`, `vsync_check.py`)와 분석 도구(`summarize.py`, `figures.py`, `report.py`)는 서로를 import하지 않고 `common.py`, `stats.py`, `style.py`만 쓴다. 분석 도구끼리는 결과 폴더의 파일로만 주고받는다.
- **판정은 JSON으로:** `report.py`는 경고를 낼 때 `summary.md`의 문장이 아니라 `summary.json`(수직 동기화에 묶인 대상, 실패한 측정)을 읽는다. 사람이 읽는 문장은 고쳐도 된다.

## 측정 프로그램 (bench_app/)

Meno 저장소를 고치지 않고 재려고, 측정 프로그램을 이 저장소에 둔다. `llm-comparison/templates/meno`와 같은 방식으로 meno `46f8934`와 SFML 3.1.0(`8835b6b`)을 커밋 해시로 받는다. `run.py`·`tracy.py`가 알아서 빌드하므로 직접 빌드할 일은 없다.

- **공개 API만으로 충분한 이유 (2026-11-14, `46f8934` 헤더·소스로 확인):**
  - 필요한 함수가 모두 공개다: `Game::window()`, `renderer()`, `scenes()`, `SceneManager::update()`·`draw()`·`changeScene()`·`shutdown()`, `Window::pollEvents()`, `Renderer::beginFrame()`·`endFrame()`·`camera()`.
  - `changeScene()`은 진행 중인 갱신이 없으면 바로 적용된다(비공개 `applyPending()`은 필요 없다).
  - `meno::Time`은 `Application::run`만 쓰고, 라이브러리 안에서 읽는 곳이 없다.
  - meno는 수직 동기화를 건드리지 않아 SFML 기본값(꺼짐)을 따른다. `Renderer::draw` 한 번은 `sf::Sprite` 하나를 만들어 `draw`를 한 번 부른다(SFML 직접과 같은 일).

### 실행 파일 약속

실행기는 아래 형식만 가정한다(`bench.toml`의 `[run].command`).

```
meno_bench --target sfml|meno-renderer|meno-object --count N --warmup W --frames F --out <경로.csv> --seed S [--loop run|direct]
```

- **출력 CSV:** 머리글 `frame,frame_ms`, 이어서 기록 프레임 F줄. 워밍업 W프레임은 기록하지 않는다. 실행기는 줄 수가 F가 아니거나 0 이하인 값이 있으면 실패로 기록한다.
- **표준 오류:** `frames=… updates=… recorded=…` 한 줄. 갱신이 프레임마다 한 번이었는지 확인할 수 있다(`runs.jsonl`의 `stderr_tail`에 남는다).
- **종료:** 정상 종료면 0, 인자가 틀리면 2, 텍스처·장면을 만들지 못하면 1.

### 네 대상과 공정하게 만드는 규칙

| 대상 | 루프 | 프레임마다 하는 일 |
|---|---|---|
| `sfml-batched` | `sf::RenderWindow` 루프 | 이벤트 → `step()` → 회전한 사각형 N개의 꼭짓점을 `sf::VertexArray` 하나에 → `clear` → `draw` 1번 → `display` |
| `sfml` | `sf::RenderWindow` 루프 | 이벤트 → `step()` → `sf::Sprite` N개에 위치·회전 → `clear` → `draw` N번 → `display` |
| `meno-renderer` | `game.run()` | update 핸들러에서 `step()`, draw 핸들러에서 `Renderer::draw` N번 |
| `meno-object` | `game.run()` | update 핸들러에서 `step()` 후 GameObject N개의 `Transform`에 위치·회전, `Scene::drawScene`이 그림 |

- **같은 입력:** 32×32 텍스처 하나(같은 픽셀로 만듦), 1280×720 창, 시드로 정한 같은 초기 위치·속도·회전, 기준점은 텍스처 중심. `meno-object`는 텍스처를 `shared_ptr`로 공유한다(`Texture&&`로 넘기면 객체마다 따로 생김).
- **같은 그림:** `sfml-batched`는 `sf::Sprite`와 같은 그림이 나와야 한다(`--screenshot`으로 마지막 프레임을 저장해 비교). 2026-11-14에 N=300으로 비교했을 때 그려진 픽셀 약 25만 8천 개 중 130개만 달랐다(가장자리 반올림 차이).
- **같은 이동:** 네 대상 모두 프레임마다 공통 이동 함수 `step()`을 한 번 부른다(등속 이동, 경계에서 튕김, 일정한 회전, 고정 dt 1/60초).
- **갱신은 프레임마다 정확히 한 번:** meno의 `Application`은 고정 시간 간격(기본 1/60초)으로 `update()`를 부른다. 그대로 두면 프레임이 1ms일 때 16프레임에 한 번만 갱신돼 SFML 대상보다 일을 덜 한다. 그래서 meno 대상은 `ApplicationConfig{.fixedTimeStep = 1e-6, .maxUpdatesPerFrame = 1, .framerateLimit = 0}`으로 만든다. 확인 결과 SFML은 721프레임에 갱신 721번, meno는 721프레임에 720번이었다(첫 프레임 하나만 빠지며, 이 프레임은 워밍업에 속한다).
- **프레임 시간:** 프레임마다 같은 지점에서 찍은 시각의 간격이다. SFML은 `display()` 뒤, meno는 draw 핸들러(`display()` 직전, 프레임마다 한 번)에서 찍는다. 간격은 어느 쪽이든 한 프레임 전체다.
- **프레임 제한:** meno는 `framerateLimit = 0`, SFML 대상은 `setFramerateLimit(0)`, `setVerticalSyncEnabled(false)`. 드라이버가 강제하는 경우를 막으려고 실행기가 `vblank_mode=0`, `__GL_SYNC_TO_VBLANK=0`을 넘긴다.

### `--loop direct` (Tracy용)

Tracy 구간은 호출 사이에 넣어야 하는데 `Game::run()` 안은 고칠 수 없다. 그래서 `--loop direct`는 `Game::run()`이 프레임마다 하는 일을 공개 API로 같은 순서로 부른다: `pollEvents` → `scenes().update` → `beginFrame` → 카메라 저장 → `scenes().draw` → 카메라 복원 → `endFrame`. 빠지는 것은 `Clock`과 `meno::Time` 갱신뿐이다.

- **검증 (2026-11-14, RTX 5070 Ti, `meno-object`, Tracy 끔, 3회씩 번갈아 돌린 중앙값):** `game.run()` 대비 N=1000에서 +1.4%, N=10000에서 +0.4%. 같은 루프의 반복 사이 차이(3~4%) 안이다.
- **쓰는 곳:** Tracy 캡처에만 쓴다(`bench.toml`의 `[tracy].extra_args`). 측정 수치는 `game.run()`(`--loop run`, 기본)으로 낸다.

## 정한 사항

배칭을 할지 판단할 근거라는 #30의 목적에 맞춰 정했다.

| # | 정한 것 | 근거 |
|---|---|---|
| B1 | **대상 4개:** `sfml-batched`(SFML, 배칭), `sfml`(SFML 직접), `meno-renderer`(meno `Renderer`로 직접), `meno-object`(`GameObject` + `Sprite` 컴포넌트). 기준은 `sfml-batched`. 처음에는 3개였고, 배칭의 이득을 보려고 2026-11-14에 `sfml-batched`를 더했다 | 배칭 판단에 실제로 쓰는 숫자는 사용자가 쓰는 경로인 `meno-object`다. 그것만 재면 느린 이유가 그리기 때문인지, 장면 순회·컴포넌트 비용 때문인지 나눌 수 없다. 세 단계를 재면 비용이 어디서 붙는지 보이고, 배칭이 줄이는 것은 `Renderer` 단계의 비용이다. 늘어나는 시간은 7개 N × 5회 = 대상당 35번 실행이라 몇 분 |
| B2 | **정해진 규칙으로 움직인다:** 스프라이트마다 시드로 정한 속도로 이동하고, 창 경계에 닿으면 튕기고, 일정한 각속도로 회전한다. 고정 dt(1/60초)로 갱신한다 | 실제 게임은 매 프레임 변환이 바뀐다. 멈춰 있으면 변환 갱신 비용이 빠져 meno에 유리하게 나온다 |
| B3 | **Tracy 옵션 `BENCH_ENABLE_TRACY`(기본 `OFF`, `bench_app`의 옵션)** | Meno를 동결했으므로 Meno 쪽 옵션(`MENO_ENABLE_TRACY`)은 두지 않는다. 무엇을 빌드하는 게 아니라 계측을 켜는 옵션이라 `BUILD`가 아니라 `ENABLE` |

- **B2 주의:** 세 대상이 같은 이동 계산을 해야 공정하다. 이동 코드는 공통 함수로 빼고, 각 대상은 그 결과(위치·회전)만 받아 자기 경로로 그린다. 그래야 대상 사이의 차이가 그리는 경로의 차이만 남는다.
- **B2 선택:** 여유가 있으면 정지 상태도 한 번 따로 잰다(`--static` 같은 옵션). 순수 그리기 비용이 따로 보인다. 필수는 아니다.
- **B3 구현:** `bench_app`이 Tracy를 커밋 해시로 받고, 옵션이 켜졌을 때만 `TRACY_ENABLE`을 켠다. 코드는 `BENCH_ZONE`·`BENCH_FRAME` 매크로로 감싸므로, 옵션이 꺼지면 빈 매크로가 된다. meno 안쪽은 계측하지 않고, `--loop direct`로 meno 공개 함수 호출 사이를 잰다.
- **B3 측정 분리:** Tracy를 켠 빌드의 수치는 측정 표에 쓰지 않는다(계측 비용이 섞임). `run.py`는 `BENCH_ENABLE_TRACY`를 넘기지 않고, 빌드 폴더도 따로 쓴다(`work/build`, `work/build-tracy`).

## 측정 환경

### 준비 (Ubuntu 24.04 또는 WSL2의 Ubuntu 24.04)

루트 README의 빌드 패키지에 더해 렌더러 확인용 `mesa-utils`가 필요하다. 없으면 실행기가 GPU에서 재는지 알 수 없어 멈춘다.

```bash
sudo apt-get install -y mesa-utils
```

```bash
glxinfo -B | grep -E "OpenGL (renderer|version)"
```

### 렌더러 판정

| `OpenGL renderer` | 뜻 | 측정 |
|---|---|---|
| `D3D12 (<GPU 이름>)` | WSLg가 Windows GPU를 쓴다 | 가능. 보고에 "WSL2(WSLg, D3D12)"라고 적는다 |
| GPU 이름 (`NVIDIA ...`, `AMD ...`, `Mesa Intel ...`) | 리눅스 기본 설치에서 GPU 드라이버를 쓴다 | 가능 |
| `llvmpipe`, `softpipe`, `swrast` | 소프트웨어 렌더링 (CPU가 그림) | 불가. 실행기가 멈춘다 |
| 빈 값, `unable to open display` | `glxinfo`가 없거나 화면에 연결되지 않음 | 불가. 실행기가 멈춘다 |

- **llvmpipe가 나올 때 (WSL):** Ubuntu 24.04의 Mesa 25.x는 WSLg에서 GPU가 연결돼 있어도 기본으로 llvmpipe를 고른다(2026-11-14, Mesa 25.2.8, WSL 3.0.5에서 확인). d3d12 드라이버를 직접 고른다. GPU가 둘(내장 + 외장)이면 아무것도 지정하지 않을 때 내장 GPU가 잡히므로 외장 GPU 이름의 일부도 준다.

  ```bash
  export GALLIUM_DRIVER=d3d12 MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA   # 외장 GPU 이름의 일부 (AMD면 Radeon 등)
  ```

  ```bash
  glxinfo -B | grep -E "OpenGL renderer|Accelerated"   # D3D12 (<외장 GPU 이름>), Accelerated: yes 여야 한다
  ```

  - 측정하는 셸에서만 설정한다. `~/.bashrc`에 넣으면 다른 프로그램(#29의 Xvfb 실행 등)에도 적용된다.
  - 실행기는 이 변수들을 실행 파일과 `glxinfo` 확인에 똑같이 넘기고, `env.json`의 `gl_env`에 기록한다.
  - 그래도 llvmpipe면 `ls /dev/dxg /usr/lib/wsl/lib/libd3d12.so`로 GPU 연결을 확인하고, Windows GPU 드라이버 업데이트, `wsl --update`, `wsl --shutdown` 순서로 해 본다. `LIBGL_ALWAYS_SOFTWARE`가 설정돼 있으면 지운다.
- **#29 환경과 다름:** #29의 Xvfb 환경은 원래 llvmpipe라 여기에 쓰지 않는다.
- **WSLg의 한계:** Windows 합성기를 거치므로 리눅스 기본 설치보다 느릴 수 있다. 세 대상이 같은 조건이라 비교에는 쓸 수 있지만, 절대 수치(몇 개까지 60fps)는 그 환경 기준임을 보고에 적는다.
- **흐름 점검만 할 때:** `--allow-software`를 주면 소프트웨어 렌더러에서도 돌지만, 그 결과는 표에 쓰지 않는다.

### 수직 동기화 확인 (vsync_check.py)

본 측정 전에 측정할 PC에서, 측정과 같은 셸 환경 변수로 한 번 돌린다. 드라이버나 화면 합성기가 동기화를 강제하면 프레임 시간이 주사율 주기(60Hz면 16.7ms)에 묶여, N을 바꿔도 프레임 시간이 그대로 나온다. 그러면 측정 전체가 쓸모없다.

```bash
python3 vsync_check.py --out results/<날짜>-<meno 커밋>   # 처음엔 SFML 빌드로 1분 남짓. 실패하면 종료 코드 1
```

- **방법:** `vsync_check/`(SFML 3.1.0, 템플릿과 같은 커밋)로 사각형 100개만 그리는 창을 vsync 끔·켬으로 600프레임씩 돌린다.
- **판정:** 끔의 중앙값이 한 주사율 주기의 2% 안이고 p10도 3% 안이면 실패다. 켬은 대조군으로, 측정용 덮어쓰기(`vblank_mode=0` 등) 없이 돌려 이 환경에서 동기화가 실제로 걸리는지 함께 보고한다.
- **기록:** `--out`을 주면 결과 폴더에 `vsync.json`이 남고, `report.py`가 문서에 옮긴다.
- **확인한 결과 (2026-11-14, RTX 5070 Ti, WSLg d3d12):** 통과. 끔 중앙값 1.30 ms. WSLg의 d3d12는 vsync 요청 자체를 지원하지 않아("Setting vertical sync not supported") 켬도 1.37 ms로, 프레임이 늘 동기화 없이 돈다.
- **측정 뒤 확인:** `summarize.py`도 같은 기준으로 본다. 한 대상이 서로 다른 N 두 개 이상에서 같은 주사율 주기에 머물면 `summary.md` 맨 위에 경고를 붙이고 `summary.json`의 `vsync_locked`에 남긴다(한 N에서만 주기 근처인 것은 우연일 수 있어 넘어간다).
- **실패하면:** WSL은 위 `GALLIUM_DRIVER` 설정을 다시 확인한다. 리눅스 기본 설치는 NVIDIA 설정의 "Sync to VBlank"를 끄거나, 화면 합성기(GNOME 등)가 없는 세션에서 잰다.

### 측정할 때

- **같은 조건으로:** 전원 연결, 고성능 전원 모드, 다른 프로그램 종료, 창을 가리지 않기. 노트북은 발열로 클럭이 내려가므로 `order = "interleaved"`로 (대상, N)을 섞어 한쪽에 몰리지 않게 한다.
- **기록:** `env.json`에 CPU, GL 렌더러·드라이버, 컴파일러, CMake 빌드 타입, 실제로 빌드한 meno 커밋(`meno`), 측정 프로그램의 버전과 커밋하지 않은 수정이 있었는지(`bench_app`의 `dirty`)가 남는다. 빌드한 meno가 `bench_app/CMakeLists.txt`에 고정한 커밋과 다르면 실행기가 멈춘다. 이어서 돌릴 때 CPU, GL 렌더러, meno, bench_app 중 하나라도 바뀌었으면 멈춘다(남은 비디오 메모리처럼 실행마다 바뀌는 값은 비교하지 않는다).

## 실행

모두 `benchmark/`에서 실행한다. 이미 기록된 (대상, N, 반복)은 건너뛰므로, 중단되면 같은 명령으로 이어서 돌린다.

```bash
python3 run.py --mock --reps 2      # 실행 파일 없이 흐름 점검 (results-mock/, 커밋 안 함)
```

```bash
python3 run.py --dry-run
```

```bash
python3 run.py                      # bench_app Release 빌드 후 측정. 결과 폴더 이름을 출력함
```

```bash
pip install -r requirements.txt && python3 summarize.py results/<날짜>-<meno 커밋>
```

```bash
python3 figures.py results/<날짜>-<meno 커밋>     # summarize.py 다음에
```

- **사전 점검:** 본 측정 전에 대상마다 가장 작은 N으로 10프레임만 돌려 본다(기록하지 않음). 하나라도 실패하면 결과 폴더를 만들기 전에 멈춘다. 대상 이름이나 출력 형식이 이 문서의 약속과 어긋나도 회차를 낭비하지 않게 하려는 것이다. `--mock`에서는 건너뛴다.
- **빌드 폴더:** `work/build/` (커밋 안 함). 처음에는 meno·SFML을 받아 빌드하느라 몇 분 걸리고, 그다음부터는 바뀐 것만 다시 빌드한다. 이미 빌드한 실행 파일이 있으면 `--bin`으로 준다.
- **실패:** 실패한 측정은 `runs.jsonl`에 `ok: false`와 `stderr_tail`로 남고 표에서 빠진다. 요약 아래에 빠진 목록이 나온다.
- **통계:** 측정마다 평균·p50·p99를 내고(백분위는 #29의 `frametime_stats.py`와 같은 방식), 반복 사이에서는 중앙값과 최소·최대를 낸다.
- **비:** 표에는 이웃한 두 대상의 비(`sfml/sfml-batched`는 스프라이트마다 `draw`를 부르는 비용, 곧 배칭으로 얻을 수 있는 최대 이득, `meno-renderer/sfml`은 `Renderer` 단계, `meno-object/meno-renderer`는 객체 단계의 비용)와 전체 비(`meno-object/sfml-batched`)가 나온다. 대상 순서는 `bench.toml`의 `targets`를 따른다.

## 그림 (figures.py)

결과 폴더의 `figures/`에 그림 4장과, 그림에서 읽을 값을 표로 옮긴 `figures.md`를 쓴다.

| 그림 | 답하는 질문 | 읽는 법 |
|---|---|---|
| `1-budget.png` | 몇 개까지 60·30 fps를 지키는가 | 평균·p99 프레임 시간과 16.7ms·33.3ms 예산선. ▼가 예산을 넘는 N(log-log 보간). 측정한 가장 큰 N까지 넘지 않으면 표시 없음 |
| `2-stages.png` | 차이가 어느 단계에서 생기는가 | `meno-object` 평균 프레임 시간을 배칭 하한선 / 스프라이트마다 `draw` / Renderer 단계 / 객체 단계 비율로 나눈 막대 |
| `3-per-sprite.png` | 개수에 비례하는 비용이 주인가 | 평균 프레임 시간 ÷ N(μs). N이 커질 때 평평해지면 스프라이트마다 드는 비용(그리기 호출 등)이 주도 |
| `4-distribution.png` | p99가 높은 이유가 끊김인가 | N = 1000, 10000에서 "x보다 느린 프레임 비율"(로그 축). 급하게 떨어지면 고른 프레임, 오른쪽으로 긴 꼬리면 가끔 튀는 프레임 |

- **2번 해석:** `sfml-batched → sfml` 칸이 배칭으로 줄일 수 있는 몫이다(스프라이트마다 `draw`를 부르는 비용). 그 위의 두 칸은 meno가 SFML 위에 얹은 비용이다.
- **2번 음수:** 뒤 대상이 앞 대상보다 빠르면 그 단계를 0으로 자르고 그림 아래와 `figures.md`에 표시한다.
- **색:** 대상마다 색이 정해져 있다(`sfml` 파랑, `meno-renderer` 주황, `meno-object` 청록, `sfml-batched` 노랑). 대상을 더하거나 빼도 색이 바뀌지 않는다. 4개까지 쓴다(선·누적 막대는 이웃한 색끼리 구분되면 되는 범위). 더 늘리면 스크립트가 멈추므로 그림을 나눈다.
- **글자:** 그림 글자는 영어다. WSL에 한글 글꼴이 없으면 깨진다.

## Tracy 병목 분석 (tracy.py)

측정과 따로, Tracy를 켠 빌드로 `meno-object`를 캡처해 프레임 시간이 어느 구간에 쓰이는지 본다. 이 빌드의 수치는 측정 표에 쓰지 않는다(계측 비용이 섞임). 조건은 `bench.toml`의 `[tracy]`(N = 1000, 10000, 워밍업 120, 기록 600프레임).

### 구간 (bench_app이 넣는 것)

- **버전:** Tracy는 계측되는 쪽과 캡처 도구의 버전이 같아야 연결된다. `bench_app/CMakeLists.txt`와 `bench.toml`의 `[tracy].version`이 같은 `v0.15.0`(`f2a4f2529e7e71844c9e434b877770a21ecf7587`)이다. 하나를 바꾸면 다른 쪽도 바꾼다.
- **프레임 구간:** 프레임 하나를 통째로 `frame` 구간으로 감싸고 끝에 `FrameMark`를 부른다. 보고서는 `frame` 구간으로 워밍업 프레임을 잘라 내고 프레임당 비용을 낸다.
- **구간 단위:** 구간은 단계마다 하나다. 스프라이트마다 구간을 열지 않는다(N = 10000이면 프레임마다 1만 개가 쌓여 캡처가 커지고 계측 비용이 결과를 바꾼다). 그래서 그리기 호출 하나(`Renderer::draw`)에는 구간이 없고, 그 비용은 대상 비교(`meno-renderer` 대 `sfml`)로 본다.
- **구간 이름:** `app.events`(`pollEvents`), `app.update`(`scenes().update`), `bench.move`(공통 이동 함수), `bench.apply`(이동 결과를 `Transform`에 옮기기), `renderer.begin`(`beginFrame`), `scene.draw`(`scenes().draw`: 장면 순회와 그리기 호출), `window.display`(`endFrame`). SFML 대상도 같은 이름으로 감싼다.

### 실행

```bash
python3 tracy.py tools                    # 처음 한 번. Tracy 캡처 도구를 work/에 빌드 (cmake, 인터넷 필요)
```

```bash
python3 tracy.py capture                  # Tracy 빌드(work/build-tracy) → N마다 캡처 → CSV 내보내기 → 보고서
```

```bash
python3 tracy.py capture --mock           # Tracy 없이 보고서 흐름 점검 (results-mock/)
```

- **렌더러:** `run.py`와 같은 확인을 거친다(WSL은 `GALLIUM_DRIVER` 등 설정, "측정 환경").
- **결과 폴더:** `results/<날짜>-<meno 커밋>/tracy/`. 같은 날짜·커밋의 측정 결과 옆에 놓인다.

| 파일 | 내용 | 커밋 |
|---|---|---|
| `<대상>-<N>.tracy` | 캡처 원본. Tracy 프로그램(같은 버전)으로 연다 | 안 함 (크다) |
| `zones-<N>.csv`, `frames-<N>.csv` | 구간 호출마다 한 줄(`tracy-csvexport -u -e`, self time), `frame` 구간 호출마다 한 줄(`-u -f frame`). N = 10000에서 각각 약 4천 줄, 7백 줄 | 함 |
| `zones.md`, `zones.png` | 프레임당 구간별 비용(ms, %), N이 커질 때 늘어난 비용. 다시 만들어진다 | 함 |
| `notes.md` | 사람이 쓰는 분석(캡처 조건, 관찰, 판단). 없을 때만 양식을 만든다 | 함 |

- **읽는 법:** 워밍업을 뺀 기록 프레임만 센다. 구간 비용은 self time(그 구간에서 자식 구간을 뺀 시간)이라 더하면 프레임 시간이 된다. `not instrumented`는 `frame` 구간의 self time, 곧 어느 하위 구간에도 속하지 않은 시간이다. 이게 크면 구간이 빠진 곳이 있다. 구간 합이 프레임 평균과 5% 넘게 다르면 `zones.md` 위에 주의가 붙는다.
- **배칭 판단:** `zones.md`의 "늘어난 비용"에서 N에 비례해 늘어나는 구간 중 그리기에 해당하는 것(`scene.draw`, `window.display`)이 배칭으로 줄어든다. SFML의 `draw`는 OpenGL 명령을 바로 내지만 GPU는 그 명령을 나중에 처리한다. GPU가 밀리면 그 대기가 `display()`에 잡히므로, `scene.draw`는 CPU 쪽 비용, `window.display`는 GPU를 기다린 시간으로 읽는다. 장면 순회·변환 갱신은 배칭으로 줄지 않는다.
- **Tracy를 켜면 느려진다:** 2026-11-14 RTX 5070 Ti에서 `meno-object` 프레임 평균이 N=1000에서 2.1 → 2.7ms, N=10000에서 9.7 → 14.2ms로 늘었고, 늘어난 시간은 주로 `window.display`에 잡혔다(원인은 확인하지 않았다). 그래서 `window.display`의 몫은 실제보다 크게 나올 수 있다. CPU 쪽 구간(`scene.draw`, `bench.*`, `app.*`)끼리의 비율 위주로 읽고, 절대 시간은 측정 표(`summary.md`)를 쓴다.
- **화면 캡처:** Tracy 프로그램에서 본 타임라인·통계 화면은 png로 `tracy/`에 두고 `notes.md`에 설명을 단다.

## 결과 보고서 (report.py)

측정·요약·그림·Tracy 결과를 모아 결과 보고서와 #30 종료 댓글 초안을 만든다. 숫자와 표는 결과 파일에서 옮기고, 해석과 결론은 `(작성)` 자리로 남긴다. 보고서는 결과 폴더의 `figures/`, `tracy/`에 있는 그림을 상대 경로로 가리킨다(복사하지 않음).

```bash
python3 report.py results/<날짜>-<meno 커밋>   # summarize.py, figures.py, tracy.py, vsync_check.py 다음에
```

| 결과 (`<결과 폴더>/report/`) | 내용 |
|---|---|
| `benchmark.md` | 결과 보고서. `(작성)` 자리를 채워 완성한다 |
| `issue-30-comment.md` | SeoulTech-Fiat-Lux/Meno#30 종료 댓글 초안 (이슈 양식의 TL;DR 형식). 보고서 위치를 가리킨다 |

- **초안 경고:** 아래 중 하나라도 있으면 보고서 맨 위에 경고가 붙는다. 경고가 남은 보고서는 결과로 쓰지 않는다.
  - mock 데이터, GL 렌더러 없음·소프트웨어 렌더러, meno 커밋 없음·고친 채로 빌드(`dirty`), 측정 프로그램(`bench_app/`)을 커밋하지 않은 채로 잼
  - `vsync.json` 없음·실패, `summary.md`의 수직 동기화 경고, 실패해 빠진 측정
  - `tracy/notes.md`의 관찰·판단이 비어 있음
- **순서:** `tracy/notes.md`의 "관찰"과 "판단"을 먼저 채우고 `report.py`를 돌린다(채운 부분만 보고서로 옮긴다). 그다음 `benchmark.md`의 `(작성)` 자리를 채운다.
- **덮어쓰기 방지:** `report.py`는 `(작성)` 자리가 하나도 남지 않은 `benchmark.md`는 덮어쓰지 않고 멈춘다. 다시 만들려면 그 파일을 지운 뒤 돌린다.

## 결과 폴더에서 커밋하는 것

측정 한 번에 원자료가 수백 개 생긴다(대상 4 × N 7 × 반복 5 = CSV 140개). 저장소에는 결과를 읽고 다시 그리는 데 필요한 것만 두고, 원자료는 git 밖에 보관한다(`.gitignore`).

| 커밋한다 (폴더당 16개 안팎) | git 밖에 둔다 |
|---|---|
| `env.json`, `vsync.json`, `runs.jsonl`(측정마다 한 줄: 성공 여부, 걸린 시간, 표준 오류 끝부분) | `raw/<대상>-<N>-rep<k>.csv` (측정마다 하나, 프레임 시간 원자료) |
| `summary.md`, `summary.csv`, `summary.json` | `tracy/*.tracy` (캡처 원본) |
| `figures/*.png`, `figures/figures.md` | `tracy/zones-<N>.csv`, `tracy/frames-<N>.csv` (구간 호출마다 한 줄) |
| `tracy/zones.md`, `tracy/zones.png`, `tracy/notes.md` | |
| `report/benchmark.md`, `report/issue-30-comment.md` | |

- **다시 만들 수 있는 범위:** 커밋한 것만으로 보고서(`report.py`)는 다시 만들 수 있다. 요약(`summarize.py`), 그림(`figures.py`), Tracy 표(`tracy.py report`)를 다시 만들려면 원자료를 풀어 둬야 한다. 원자료가 없으면 이 도구들은 커밋된 결과를 덮어쓰지 않고 멈춘다.

### 원자료 보관

결과 폴더마다 원자료를 압축해 팀 공유 드라이브나 GitHub Release에 올린다(RTX 5070 Ti 측정 기준 약 8.5 MB, 대부분 Tracy 캡처).

```bash
cd results/<결과 폴더> && tar czf ../<결과 폴더>-raw.tar.gz raw tracy/*.tracy tracy/zones-*.csv tracy/frames-*.csv
```

- **풀 때:** 같은 결과 폴더 안에서 `tar xzf ../<결과 폴더>-raw.tar.gz`. 압축 파일(`*.tar.gz`)은 `results/` 바로 아래에 생기며, 커밋하지 않는다.

## 이슈 체크리스트와의 대응

- **측정 실행 파일:** 이 폴더의 `bench_app/`(Meno는 고치지 않음). `run.py`가 측정 전에 대상마다 짧게 돌려 약속을 확인한다.
- **측정 환경 확정:** "측정 환경"의 렌더러 판정과 `vsync_check.py`.
- **결과 표와 그래프:** `summarize.py`(`summary.md`), `figures.py`(`figures/`).
- **Tracy로 병목 분석:** `tracy.py`(`tracy/zones.md`, `zones.png`)와 사람이 쓴 `notes.md`.
- **결과 보고서와 #30 정리:** `report.py`(`report/`). 보고서는 이 저장소에 둔다.
