# 성능 측정 (SeoulTech-Fiat-Lux/Meno#30)

스프라이트 N개의 프레임 시간을 meno와 SFML 직접 사용으로 나눠 잰다. 배칭 적용 여부의 근거다. #29 D4의 "적 200·400마리"도 여기서 잰다.

- meno 동결 커밋 `46f8934`와 SFML 3.1.0(`8835b6b`)을 FetchContent로 받아 공개 API로만 잰다. Meno 저장소는 고치지 않는다.
- 결과는 `results/<날짜>-<meno 커밋>-<GPU>/`에 커밋한다(예: `rtx-5070-ti`, `radeon`).
- 실행 도구(`run.py`, `tracy.py`, `vsync_check.py`)와 분석 도구(`summarize.py`, `figures.py`, `report.py`)는 서로 import하지 않는다. 분석 도구끼리는 결과 파일로만 주고받는다. 경고 판정은 `summary.json`을 읽는다.

```
bench.toml       N 목록, 대상, 반복 수, 워밍업·기록 프레임 수, 실행 명령
bench_app/       측정 프로그램 meno_bench (C++, 동결한 meno·SFML·Tracy를 커밋 해시로 받음)
run.py           bench_app을 Release로 빌드하고 (대상, N, 반복)마다 새 프로세스로 실행, 환경과 함께 기록
summarize.py     평균·p50·p99 표(summary.md, summary.csv)와 판정 결과(summary.json)
figures.py       배칭 판단용 그림 4장과 그 값 표(figures/)
tracy.py         Tracy 캡처 도구 빌드, 캡처, 구간별 비용 표·그림(tracy/)
vsync_check.py   수직 동기화가 정말 꺼지는지 확인 (vsync_check/ 프로그램 빌드·실행)
report.py        결과 보고서(report/benchmark.md)
common.py        공용: bench.toml 읽기, 실행 명령 틀, 측정하는 meno 커밋 (다른 공용 모듈의 바탕)
probe.py         공용(실행 도구): 측정 환경 조사. GL 렌더러, git 버전, CPU·전원 상태, env.json
build.py         공용(실행 도구): bench_app·vsync_check 빌드, 받은 meno 커밋 확인
results.py       공용(분석 도구): 결과 폴더 이름과 읽기(runs.jsonl, raw/, 대상 순서)
stats.py         공용: 백분위, 수직 동기화 판정 (입출력 없는 계산)
style.py         공용: 그림 색과 서식 (figures.py, tracy.py)
results/<날짜>-<meno 커밋>-<GPU>/   결과 폴더. 요약·그림·보고서만 커밋하고 원자료는 git 밖에 둔다 ("결과 폴더에서 커밋하는 것")
```

## 실행 파일 약속

```
meno_bench --target sfml|meno-renderer|meno-object --count N --warmup W --frames F --out <경로.csv> --seed S [--loop run|direct]
```

- CSV: 머리글 `frame,frame_ms` + 기록 프레임 F줄(워밍업 제외). 줄 수가 F가 아니거나 0 이하 값이 있으면 실패다.
- 표준 오류: `frames=… updates=… recorded=…` 한 줄. 종료 코드: 정상 0, 인자 오류 2, 텍스처·장면 생성 실패 1.

| 대상 | 프레임마다 하는 일 |
|---|---|
| `sfml-batched` (기준) | `step()` → 사각형 N개를 `sf::VertexArray` 하나에 → `draw` 1번 → `display` |
| `sfml` | `step()` → `sf::Sprite` N개 → `draw` N번 → `display` |
| `meno-renderer` | `game.run()`. update에서 `step()`, draw에서 `Renderer::draw` N번 |
| `meno-object` | `game.run()`. `step()` 후 GameObject N개의 `Transform` 갱신, `Scene::drawScene`이 그림 |

- 배칭 판단에 쓰는 숫자는 `meno-object`다. 나머지는 비용이 어느 단계에서 붙는지 나누려고 잰다.
- 공통: 32×32 텍스처 하나, 1280×720 창, 같은 시드, 프레임마다 `step()` 한 번(등속 이동, 경계 튕김, 회전, dt 1/60초).
- meno 대상은 `ApplicationConfig{.fixedTimeStep = 1e-6, .maxUpdatesPerFrame = 1, .framerateLimit = 0}`로 갱신을 프레임마다 한 번 한다.
- 프레임 제한·vsync는 끈다. 실행기가 `vblank_mode=0`, `__GL_SYNC_TO_VBLANK=0`도 넘긴다.
- `--loop direct`는 `Game::run()`과 같은 순서로 공개 API를 부른다(차이 1.4% 이내). Tracy 캡처에만 쓴다.
- Tracy는 `BENCH_ENABLE_TRACY`(기본 `OFF`)로 켠다. Tracy 빌드(`work/build-tracy`)의 수치는 측정 표에 쓰지 않는다.

## 측정 환경

Ubuntu 24.04(WSL2 포함). `mesa-utils`가 없으면 실행기가 멈춘다.

```bash
sudo apt-get install -y mesa-utils
```

```bash
glxinfo -B | grep -E "OpenGL (renderer|version)"
```

- `OpenGL renderer`가 `D3D12 (<GPU>)`나 GPU 이름이면 측정 가능. WSL이면 보고에 "WSL2(WSLg, D3D12)"라고 적는다.
- `llvmpipe`, `softpipe`, `swrast`, 빈 값이면 실행기가 멈춘다. `--allow-software`는 흐름 점검용이고 결과는 쓰지 않는다.
- WSL에서 llvmpipe가 나오면 측정하는 셸에서만 아래를 설정한다(`~/.bashrc`에 넣지 않는다).

```bash
export GALLIUM_DRIVER=d3d12 MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA   # 외장 GPU 이름의 일부 (AMD면 Radeon 등)
```

```bash
glxinfo -B | grep -E "OpenGL renderer|Accelerated"   # D3D12 (<외장 GPU 이름>), Accelerated: yes 여야 한다
```

- 그래도 llvmpipe면 `ls /dev/dxg /usr/lib/wsl/lib/libd3d12.so` 확인 → GPU 드라이버 업데이트 → `wsl --update` → `wsl --shutdown`. `LIBGL_ALWAYS_SOFTWARE`는 지운다.
- WSLg 절대 수치는 그 환경 기준임을 보고에 적는다.
- 측정 중: 전원 연결, 고성능 모드, 다른 프로그램 종료, 창 가리지 않기. 노트북은 `order = "interleaved"`.
- 빌드한 meno가 고정 커밋과 다르거나, 이어서 돌릴 때 CPU·GL 렌더러·meno·bench_app·Windows 전원 설정이 바뀌면 실행기가 멈춘다.

### CPU 상태 기록

그리기 호출을 하나씩 보내는 비용은 CPU가 정한다. WSL 안에서는 CPU 클럭을 정하는 Windows 전원 설정도, Windows 프로그램이 쓰는 CPU도 보이지 않으므로(`cpu_governor`가 빈다) 실행기가 Windows에 직접 묻는다.

| 기록 | 위치 | 경고 (report.py) |
|---|---|---|
| 전원 계획, 전원 모드, 최대·최소 프로세서 상태(%) | `env.json`의 `windows_power` | 최대 프로세서 상태가 100% 미만 |
| 측정 직전 1초의 Windows 전체 CPU 사용률(%) | `runs.jsonl`의 `host_cpu_pct` | 30% 초과 |
| 측정 중 CPU 사용(코어 수): 측정 프로그램 `self_cores`, WSL 안의 나머지 `other_cores` | `runs.jsonl`의 `cpu` | `other_cores` 1.5 초과 |

- 보고서 측정 환경 표의 "CPU 상태" 칸에 요약한다. 기록이 없는 옛 결과 폴더는 "기록 없음"이다.
- `other_cores`는 쉴 때 0이지만, 측정 중에는 WSLg가 창을 합성하는 몫으로 0.3~0.6코어가 늘 잡힌다. 경고는 그 위로 병렬 빌드처럼 코어를 여럿 쓰는 작업이 겹칠 때만 낸다.
- Windows 사용률은 측정 사이에 묻는다(측정 중에 물으면 그 질의가 CPU를 쓴다). 한 번에 약 1초라 측정 140회에 2분 남짓 늘어난다.
- CPU 비교 측정: Windows 제어판 > 전원 옵션 > 고급 설정 > 프로세서 전원 관리 > 최대 프로세서 상태를 낮춰 잰다. 값이 `windows_power`에 남으므로 결과 폴더를 따로 둔다(`--out`).

### 수직 동기화 확인

본 측정 전, 같은 셸 환경으로 한 번 돌린다. 동기화가 강제되면 프레임 시간이 16.7ms(60Hz)에 묶여 측정이 무효다.

```bash
python3 vsync_check.py --out results/<날짜>-<meno 커밋>-<GPU>   # 처음엔 SFML 빌드로 1분 남짓. 실패하면 종료 코드 1
```

- 실패 기준: vsync 끔의 중앙값이 주사율 주기의 2% 안이고 p10도 3% 안.
- `summarize.py`도 한 대상이 N 두 개 이상에서 주기에 묶이면 `summary.md`에 경고한다.
- 실패하면 WSL은 `GALLIUM_DRIVER`를, 리눅스는 NVIDIA "Sync to VBlank"나 합성기 없는 세션을 확인한다.

## 실행

`benchmark/`에서 실행한다. 중단되면 같은 명령으로 이어 돌린다(기록된 측정은 건너뜀).

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
pip install -r requirements.txt && python3 summarize.py results/<날짜>-<meno 커밋>-<GPU>
```

```bash
python3 figures.py results/<날짜>-<meno 커밋>-<GPU>     # summarize.py 다음에
```

- 측정 전 대상마다 10프레임 사전 점검을 하고, 실패하면 멈춘다.
- 빌드 폴더는 `work/build/`. 이미 빌드한 실행 파일은 `--bin`으로 준다.
- 실패한 측정은 `runs.jsonl`에 `ok: false`로 남고 표에서 빠진다.
- 비: `sfml/sfml-batched`(배칭 최대 이득), `meno-renderer/sfml`(`Renderer` 단계), `meno-object/meno-renderer`(객체 단계).
- 그림: `1-budget.png`(60·30fps 한계 N), `2-stages.png`(단계별 비용), `3-per-sprite.png`(스프라이트당 비용), `4-distribution.png`(느린 프레임 분포). 값은 `figures.md`. 그림 글자는 영어다.

## Tracy 병목 분석

`meno-object`를 N = 1000, 10000에서 캡처해 구간별 비용을 본다(`bench.toml`의 `[tracy]`).

```bash
python3 tracy.py tools                    # 처음 한 번. Tracy 캡처 도구를 work/에 빌드 (cmake, 인터넷 필요)
```

```bash
python3 tracy.py capture                  # Tracy 빌드(work/build-tracy) → N마다 캡처 → CSV 내보내기 → 보고서
```

```bash
python3 tracy.py capture --mock           # Tracy 없이 보고서 흐름 점검 (results-mock/)
```

- Tracy 버전 `v0.15.0`은 `bench_app/CMakeLists.txt`와 `[tracy].version`이 같아야 한다.
- 구간: `app.events`, `app.update`, `bench.move`, `bench.apply`, `renderer.begin`, `scene.draw`, `window.display`. 프레임 전체는 `frame`.
- 구간 비용은 self time이라 합이 프레임 시간이다. 5% 넘게 다르면 `zones.md`에 주의가 붙는다.
- 배칭으로 주는 것은 `scene.draw`(CPU)와 `window.display`(GPU 대기)다.
- 주의: Tracy를 켜면 `window.display`가 부풀려진다. CPU 구간끼리 비율로 읽고, 절대 시간은 `summary.md`를 쓴다.
- 분석은 `tracy/notes.md`에 사람이 쓴다.

## 결과 보고서 (report.py)

```bash
python3 report.py results/<날짜>-<meno 커밋>-<GPU>   # summarize.py, figures.py, tracy.py, vsync_check.py 다음에
```

- `report/benchmark.md`(보고서)를 만든다.
- 순서: `tracy/notes.md`의 "관찰"·"판단" → `report.py` → `benchmark.md`의 `(작성)` 채우기.
- `(작성)`이 다 채워진 `benchmark.md`는 덮어쓰지 않는다. 다시 만들려면 지운다.
- 경고가 붙은 보고서는 결과로 쓰지 않는다. 경고 조건: mock, 소프트웨어 렌더러, `dirty` 빌드, vsync 실패·경고, 빠진 측정, 빈 `notes.md`.

## 결과 폴더에서 커밋하는 것

- 커밋: `env.json`, `vsync.json`, `runs.jsonl`, `summary.*`, `figures/`, `tracy/zones.md`, `tracy/zones.png`, `tracy/notes.md`, `report/`.
- git 밖: `raw/*.csv`, `tracy/*.tracy`, `tracy/zones-*.csv`, `tracy/frames-*.csv`.
- `summarize.py`, `figures.py`, `tracy.py report`는 원자료가 없으면 멈춘다.

### 원자료 보관

결과 폴더마다 압축해 팀 공유 드라이브나 GitHub Release에 올린다.

```bash
cd results/<결과 폴더> && tar czf ../<결과 폴더>-raw.tar.gz raw tracy/*.tracy tracy/zones-*.csv tracy/frames-*.csv
```

- 풀 때: 결과 폴더 안에서 `tar xzf ../<결과 폴더>-raw.tar.gz`. `*.tar.gz`는 커밋하지 않는다.
