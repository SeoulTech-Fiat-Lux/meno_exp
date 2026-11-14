# 성능 측정 (SeoulTech-Fiat-Lux/Meno#30)

스프라이트 N개를 그리는 프레임 시간을 meno로 그릴 때와 SFML을 직접 쓸 때로 나눠 잰다. 결과는 배칭을 적용할지 정하는 근거로 쓴다.
#29(개발 비용)와 따로 재는 실행 비용이다. #29의 D4에서 넘겨받은 "적 200·400마리" 상황도 여기서 잰다.

- **측정 코드:** Meno 저장소의 `benchmarks/`(CMake 옵션 `MENO_BUILD_BENCHMARKS`, 기본 `OFF`). 문서는 Meno의 `docs/benchmark.md`.
- **이 폴더:** 그 실행 파일을 정해진 조건으로 돌리는 실행기, 결과 요약기, 측정 결과.

```
bench.toml       N 목록, 대상, 반복 수, 워밍업·기록 프레임 수, 실행 명령
run.py           Meno를 Release로 빌드하고 (대상, N, 반복)마다 새 프로세스로 실행, 환경과 함께 기록
summarize.py     평균·p50·p99 표(summary.md, summary.csv)와 그래프(frametime.png)
results/<날짜>-<meno 커밋>/   env.json, runs.jsonl, raw/<대상>-<N>-rep<k>.csv, 요약 파일, tracy/
```

## 실행 파일 약속 (Meno 쪽에서 맞출 것)

실행기는 아래 형식만 가정한다. Meno 쪽에서 다르게 정하면 `bench.toml`의 `[build].binary`와 `[run].command`만 고친다.

```
meno_bench --target sfml|meno-renderer|meno-object --count N --warmup W --frames F --out <경로.csv> --seed S
```

- **출력 CSV:** 머리글 `frame,frame_ms`, 이어서 기록 프레임 F줄. 워밍업 W프레임은 기록하지 않는다. 실행기는 줄 수가 F가 아니거나 0 이하인 값이 있으면 실패로 기록한다.
- **프레임 시간:** 한 프레임의 시작부터 다음 프레임 시작까지(갱신·그리기·`display()` 포함), 고해상도 시계로 ms.
- **같게 할 것:** 세 대상은 같은 텍스처 1장, 같은 창 크기, 같은 시드로 정한 같은 초기 위치·크기·회전·속도를 쓴다. 다른 것은 그리는 경로뿐이어야 한다.
- **프레임 제한:** meno는 `ApplicationConfig::framerateLimit = 0`, SFML 대상은 `setFramerateLimit(0)`, `setVerticalSync(false)`. meno에는 수직 동기화를 켜고 끄는 API가 없어 SFML 기본값(꺼짐)을 따른다. 드라이버가 강제하는 경우를 막으려고 실행기가 `vblank_mode=0`, `__GL_SYNC_TO_VBLANK=0`을 넘긴다.
- **종료:** 정상 종료면 0, 에셋 로드 실패 등은 0이 아닌 값.

## 정한 사항

배칭을 할지 판단할 근거라는 #30의 목적에 맞춰 정했다.

| # | 정한 것 | 근거 |
|---|---|---|
| B1 | **대상 3개:** `sfml`(SFML 직접), `meno-renderer`(meno `Renderer`로 직접), `meno-object`(`GameObject` + `Sprite` 컴포넌트). 기준은 `sfml` | 배칭 판단에 실제로 쓰는 숫자는 사용자가 쓰는 경로인 `meno-object`다. 그것만 재면 느린 이유가 그리기 때문인지, 장면 순회·컴포넌트 비용 때문인지 나눌 수 없다. 세 단계를 재면 비용이 어디서 붙는지 보이고, 배칭이 줄이는 것은 `Renderer` 단계의 비용이다. 늘어나는 시간은 7개 N × 5회 = 대상당 35번 실행이라 몇 분 |
| B2 | **정해진 규칙으로 움직인다:** 스프라이트마다 시드로 정한 속도로 이동하고, 창 경계에 닿으면 튕기고, 일정한 각속도로 회전한다. 고정 dt(1/60초)로 갱신한다 | 실제 게임은 매 프레임 변환이 바뀐다. 멈춰 있으면 변환 갱신 비용이 빠져 meno에 유리하게 나온다 |
| B3 | **Tracy 옵션 `MENO_ENABLE_TRACY`(기본 `OFF`)** | 기존 `MENO_BUILD_EXAMPLES`, `MENO_BUILD_TESTS`, `MENO_BUILD_BENCHMARKS`와 같은 `MENO_` 규칙. 무엇을 빌드하는 게 아니라 계측을 켜는 옵션이라 `BUILD`가 아니라 `ENABLE` |

- **B2 주의:** 세 대상이 같은 이동 계산을 해야 공정하다. 이동 코드는 공통 함수로 빼고, 각 대상은 그 결과(위치·회전)만 받아 자기 경로로 그린다. 그래야 대상 사이의 차이가 그리는 경로의 차이만 남는다.
- **B2 선택:** 여유가 있으면 정지 상태도 한 번 따로 잰다(`--static` 같은 옵션). 순수 그리기 비용이 따로 보인다. 필수는 아니다.
- **B3 구현:** Tracy는 FetchContent로 고정 태그를 받고, 옵션이 켜졌을 때만 `TRACY_ENABLE`을 정의한다. 코드에는 Tracy 매크로를 직접 쓰지 말고 meno 쪽 매크로(예: `MENO_PROFILE_SCOPE`)로 감싼다. 옵션이 꺼지면 빈 매크로가 되므로 사용자 빌드에 Tracy 의존성이 생기지 않는다.
- **B3 측정 분리:** Tracy를 켠 빌드의 수치는 측정 표에 쓰지 않는다(계측 비용이 섞임). 실행기는 `MENO_ENABLE_TRACY`를 넘기지 않는다.

## 측정 환경

- **GPU에서 잴 것:** `env.json`의 `glxinfo`에서 `OpenGL renderer`가 `llvmpipe`면 소프트웨어 렌더링이다. 그 수치는 GPU 성능이 아니므로 표에 쓰지 않는다. #29의 Xvfb 환경은 llvmpipe라 여기에는 맞지 않는다.
- **WSL2(WSLg):** 렌더러가 `D3D12 (...)`로 나오면 GPU를 쓴다. 다만 Windows 합성기를 거치므로 리눅스 기본 설치보다 느릴 수 있다. 결과 보고에 어느 쪽인지 적는다.
- **같은 조건으로:** 전원 연결, 고성능 전원 모드, 다른 프로그램 종료, 창을 가리지 않기. 노트북은 발열로 클럭이 내려가므로 `order = "interleaved"`로 (대상, N)을 섞어 한쪽에 몰리지 않게 한다.
- **기록:** `env.json`에 CPU, GL 렌더러·드라이버, 컴파일러, CMake 빌드 타입, meno 커밋과 고친 채로 빌드했는지(`dirty`)가 남는다. 이어서 돌릴 때 CPU·GPU·meno 커밋이 바뀌었으면 실행기가 멈춘다.

## 실행

모두 `benchmark/`에서 실행한다. 이미 기록된 (대상, N, 반복)은 건너뛰므로, 중단되면 같은 명령으로 이어서 돌린다.

```bash
python3 run.py --mock --reps 2      # 실행 파일 없이 흐름 점검 (results-mock/, 커밋 안 함)
```

```bash
python3 run.py --meno ~/Meno --dry-run
```

```bash
python3 run.py --meno ~/Meno        # Release 빌드 후 측정. 결과 폴더 이름을 출력함
```

```bash
pip install -r requirements.txt && python3 summarize.py results/<날짜>-<meno 커밋>
```

- **사전 점검:** 본 측정 전에 대상마다 가장 작은 N으로 10프레임만 돌려 본다(기록하지 않음). 하나라도 실패하면 결과 폴더를 만들기 전에 멈춘다. 대상 이름이나 출력 형식이 이 문서의 약속과 어긋나도 회차를 낭비하지 않게 하려는 것이다. `--mock`에서는 건너뛴다.
- **빌드 폴더:** `work/build-<커밋>/` (커밋 안 함). 이미 빌드한 실행 파일이 있으면 `--bin`으로 준다.
- **실패:** 실패한 측정은 `runs.jsonl`에 `ok: false`와 `stderr_tail`로 남고 표에서 빠진다. 요약 아래에 빠진 목록이 나온다.
- **통계:** 측정마다 평균·p50·p99를 내고(백분위는 #29의 `frametime_stats.py`와 같은 방식), 반복 사이에서는 중앙값과 최소·최대를 낸다.
- **비:** 표에는 이웃한 두 대상의 비(`meno-renderer/sfml`은 `Renderer` 단계, `meno-object/meno-renderer`는 객체 단계의 비용)와 전체 비(`meno-object/sfml`)가 나온다. 대상 순서는 `bench.toml`의 `targets`를 따른다.

## Tracy 병목 분석

- 측정과 따로, `-DMENO_ENABLE_TRACY=ON` 빌드로 `meno-object` 대상을 N = 1000, 10000 정도에서 캡처한다.
- `.tracy` 캡처 파일은 크므로 커밋하지 않는다(`.gitignore`). 대신 `results/<…>/tracy/`에 화면 캡처(png)와 `notes.md`(어느 구간이 몇 ms, 무엇이 병목인지)를 둔다.

## 이슈 체크리스트와의 대응

- **측정 실행 파일:** Meno 저장소 `benchmarks/`. 이 폴더의 약속을 따른다.
- **결과 표와 그래프:** `summarize.py`가 만든 `summary.md`, `frametime.png`를 Meno의 `docs/benchmark.md`로 옮긴다.
- **Tracy로 병목 분석:** `results/<…>/tracy/notes.md`.
