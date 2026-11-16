# 성능 측정 (SeoulTech-Fiat-Lux/Meno#30)

스프라이트 N개의 프레임 시간을 meno와 SFML 직접 사용으로 나눠 잰다. 배칭 적용 여부의 근거다. #29 D4의 "적 200·400마리"도 여기서 잰다.

- 최종 보고서: [results/report.md](results/report.md)
- meno 동결 커밋 `46f8934`와 SFML 3.1.0(`8835b6b`)을 FetchContent로 받아 공개 API로만 잰다. Meno 저장소는 고치지 않는다.

```
bench.toml       N 목록, 대상, 반복 수, 워밍업·기록 프레임 수, 실행 명령
bench_app/       측정 프로그램 meno_bench (C++, 동결한 meno·SFML·Tracy를 커밋 해시로 받음)
vsync_check.py   수직 동기화가 정말 꺼지는지 확인 (vsync_check/ 프로그램 빌드·실행)
run.py           bench_app을 Release로 빌드하고 (대상, N, 반복)마다 새 프로세스로 실행, 환경과 함께 기록
summarize.py     평균·p50·p99 표(summary.*)
figures.py       그림 4장과 그 값 표(figures/)
tracy.py         Tracy 캡처와 구간별 비용 표·그림(tracy/)
report.py        결과 폴더들을 모아 보고서 초안(results/report-draft.md). 최종본은 사람이 다듬는다
common.py        공용: bench.toml 읽기, 실행 명령 틀, 측정하는 meno 커밋
probe.py         공용(실행 도구): GL 렌더러, git 버전, CPU·전원 상태 조사
build.py         공용(실행 도구): 빌드와 meno 커밋 확인
results.py       공용(분석 도구): 결과 폴더 이름과 읽기
stats.py         공용: 백분위, 수직 동기화 판정
style.py         공용: 그림 색과 서식
results/<날짜>-<meno 커밋>-<GPU>/   GPU 하나의 결과 폴더
```

## 따라 하기

명령은 모두 WSL(Ubuntu 24.04)의 `~/meno_exp/benchmark`에서 위에서부터 차례로 실행한다.

### 0. 준비 (처음 한 번)

리눅스 쪽 홈에 clone한다. `/mnt/c/...`에서는 빌드가 느리다.

```bash
sudo apt-get update && sudo apt-get install -y build-essential cmake git python3 python3-venv mesa-utils \
  libxrandr-dev libxcursor-dev libxi-dev libudev-dev libfreetype-dev libgl1-mesa-dev libegl1-mesa-dev \
  libflac-dev libvorbis-dev libogg-dev libharfbuzz-dev
```

```bash
git clone https://github.com/SeoulTech-Fiat-Lux/meno_exp.git ~/meno_exp
```

```bash
cd ~/meno_exp/benchmark && python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
```

### 1. 측정할 PC 상태

- 전원을 연결하고 Windows 전원 모드를 "최고 성능"으로 둔다(설정 > 시스템 > 전원).
- 다른 프로그램을 닫는다. 측정 중에는 PC를 쓰지 않고, 측정 창을 가리지 않는다.
- 상태는 결과에 기록되고, 어긋나면 보고서 맨 위에 경고가 붙는다.

### 2. GPU마다 측정 (GPU 하나에 약 15분, 처음은 빌드로 10분 더)

새 터미널을 열 때마다 2-1부터 한다. 측정은 중단되어도 같은 명령으로 이어서 돌린다.

**2-1. 터미널을 준비하고 GPU를 고른다.** `NVIDIA` 자리에 GPU 이름의 일부를 쓴다(내장 AMD면 `Radeon`).

```bash
cd ~/meno_exp/benchmark && source .venv/bin/activate
```

```bash
export GALLIUM_DRIVER=d3d12 MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
```

```bash
glxinfo -B | grep "OpenGL renderer"
```

`D3D12 (<GPU 이름>)`이 나와야 한다. `llvmpipe`면 아래 "문제 해결".

**2-2. 결과 폴더 이름을 정한다.** 이후 명령은 모두 이 폴더에 쓴다.

```bash
OUT=$(python3 run.py --print-out) && echo $OUT
```

**2-3. 수직 동기화가 꺼지는지 확인한다.** `통과`가 나와야 한다.

```bash
python3 vsync_check.py --out $OUT
```

**2-4. 측정한다.** 대상 4 × N 7 × 5회 = 140회.

```bash
python3 run.py --out $OUT
```

**2-5. 요약과 그림을 만든다.**

```bash
python3 summarize.py $OUT && python3 figures.py $OUT
```

**2-6. Tracy로 구간별 비용을 캡처한다.** 처음에는 캡처 도구와 Tracy 빌드를 만든다.

```bash
python3 tracy.py capture --out $OUT/tracy
```

**2-7. (선택) 해석을 쓴다.** `$OUT/tracy/notes.md`의 "관찰"·"판단"을 채우면 보고서에 들어간다. 비워 두어도 된다.

GPU가 더 있으면 2-1부터 다시 한다(`MESA_D3D12_DEFAULT_ADAPTER_NAME=Radeon` 등).

### 3. 보고서를 만든다

**3-1. 초안을 만든다.** 2에서 만든 결과 폴더를 모두 준다(`<폴더>` 자리에 실제 이름을 쓴다). `results/report-draft.md`가 생긴다.

```bash
ls -d results/*/
```

```bash
python3 report.py results/<폴더 1> results/<폴더 2>
```

경고가 나오면 초안 맨 위에도 붙는다. 원인을 고치고 그 GPU를 다시 잰다(새 `--out` 폴더로).

**3-2. 초안을 다듬어 최종본으로 저장한다.** 숫자와 표는 그대로 두고, 문장을 다듬어 `results/report.md`로 저장한다.

```bash
cp results/report-draft.md results/report.md
```

- 초안은 커밋하지 않는다(`.gitignore`). `report.py`는 최종본을 덮어쓰지 않는다.
- 결과를 다시 쟀다면 초안을 새로 만들고, 바뀐 숫자를 최종본에 옮긴다.

### 4. 원자료를 보관하고 커밋한다

원자료(측정마다 CSV, Tracy 캡처)는 git 밖에 둔다. 원자료가 있는 결과 폴더마다 압축해(`results/<폴더>-raw.tar.gz`) 팀 드라이브에 올린다.

```bash
for d in results/*/; do if [ -d "$d/raw" ]; then (cd "$d" && tar czf "../$(basename "$d")-raw.tar.gz" raw tracy/*.tracy tracy/*-*.csv); fi; done
```

```bash
git add results && git commit -m "research: Add benchmark results #5" && git push
```

## 참고

### 흐름 점검 (GPU·Tracy 없이)

```bash
python3 run.py --mock --reps 2 && python3 summarize.py results-mock/*-mock && python3 figures.py results-mock/*-mock
```

```bash
python3 tracy.py capture --mock --out results-mock/$(date +%Y%m%d)-mock/tracy && python3 report.py results-mock/*-mock --out results-mock/report-draft.md
```

### 문제 해결

- `llvmpipe`(소프트웨어 렌더러)면 측정하지 않고 멈춘다. `ls /dev/dxg /usr/lib/wsl/lib/libd3d12.so` 확인 → GPU 드라이버 업데이트 → PowerShell에서 `wsl --update`, `wsl --shutdown`.
- `GALLIUM_DRIVER` 등은 측정하는 터미널에서만 설정한다(`~/.bashrc`에 넣지 않는다). `LIBGL_ALWAYS_SOFTWARE`는 지운다.
- 수직 동기화 확인이 실패하면(프레임 시간이 16.7 ms 같은 주사율 주기에 묶임) 측정이 무효다. WSL은 `GALLIUM_DRIVER`를, 리눅스는 NVIDIA "Sync to VBlank"를 확인한다.
- 이어서 돌릴 때 CPU·GL 렌더러·meno·bench_app·전원 설정이 기록과 다르면 실행기가 멈춘다. 다른 `--out`을 쓴다.

### 측정 대상과 실행 파일 약속

```
meno_bench --target <대상> --count N --warmup W --frames F --out <경로.csv> --seed S [--loop run|direct]
```

| 대상 | 프레임마다 하는 일 |
|---|---|
| `sfml-batched` (기준) | `step()` → 사각형 N개를 `sf::VertexArray` 하나에 → `draw` 1번 → `display` |
| `sfml` | `step()` → `sf::Sprite` N개 → `draw` N번 → `display` |
| `meno-renderer` | `game.run()`. update에서 `step()`, draw에서 `Renderer::draw` N번 |
| `meno-object` | `game.run()`. `step()` 후 GameObject N개의 `Transform` 갱신, 장면이 그림 |

- 공통: 32×32 텍스처 하나, 1280×720 창, 같은 시드, 프레임마다 `step()` 한 번(등속 이동, 경계 튕김, 회전, dt 1/60초).
- meno 대상은 `ApplicationConfig{.fixedTimeStep = 1e-6, .maxUpdatesPerFrame = 1, .framerateLimit = 0}`로 갱신을 프레임마다 한 번 한다.
- 프레임 제한·vsync는 끈다. 실행기가 `vblank_mode=0`, `__GL_SYNC_TO_VBLANK=0`도 넘긴다.
- 출력: CSV 머리글 `frame,frame_ms` + F줄. 종료 코드 정상 0, 인자 오류 2, 생성 실패 1.
- `--loop direct`는 `Game::run()`과 같은 순서로 공개 API를 부른다(차이 1.4% 이내). Tracy 캡처에만 쓴다.
- Tracy는 `BENCH_ENABLE_TRACY`로 켠다. 버전 `v0.15.0`은 `bench_app/CMakeLists.txt`와 `bench.toml [tracy]`가 같아야 한다.
- Tracy 구간: `app.events`, `app.update`, `bench.move`, `bench.apply`, `renderer.begin`, `scene.draw`, `window.display`. 프레임 전체는 `frame`.

### 보고서 경고 기준

| 경고 | 기준 |
|---|---|
| 렌더러 | 소프트웨어 렌더러이거나 기록 없음 |
| 코드 | meno나 bench_app을 커밋하지 않은 채로 잼 |
| 수직 동기화 | `vsync_check.py` 실패, 또는 한 대상이 N 두 개 이상에서 주사율 주기에 묶임 |
| 빠진 측정 | `runs.jsonl`에 실패한 측정이 남음 |
| Tracy | `tracy/zones.json` 없음, `notes.md`에 `(작성)` 남음 |
| CPU | 최대 프로세서 상태 100% 미만, 측정 직전 Windows CPU 30% 초과, 측정 중 다른 WSL 작업 1.5코어 초과 |
| 폴더끼리 | meno 커밋이나 `bench.toml [sweep]`이 다름 |

- CPU 기록: 전원 설정은 `env.json`의 `windows_power`, 측정마다 CPU 사용은 `runs.jsonl`의 `host_cpu_pct`, `cpu`. 측정 중에는 WSLg 화면 합성 몫으로 0.3~0.6코어가 늘 잡힌다.
- "GPU 사이 차이" 판정: 같은 CPU에서 `sfml`의 GPU 사이 차이가 15% 이하이면 배칭하지 않은 비용을 CPU가 정한다고 본다.

### 결과 폴더에서 커밋하는 것

- 커밋: `env.json`, `vsync.json`, `runs.jsonl`, `summary.*`, `figures/`, `tracy/zones.*`, `tracy/notes.md`, 그리고 `results/report.md`(최종본). `results/report-draft.md`(초안)는 커밋하지 않는다.
- git 밖: `raw/`, `tracy/*.tracy`, `tracy/zones-*.csv`, `tracy/frames-*.csv`, `*-raw.tar.gz`.
- 원자료를 풀려면 결과 폴더 안에서 `tar xzf ../<결과 폴더>-raw.tar.gz`. `summarize.py`, `figures.py`, `tracy.py report`는 원자료가 있어야 다시 돌릴 수 있다. `report.py`는 커밋된 파일만으로 돈다.
