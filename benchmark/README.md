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
meno_bench --target meno|sfml --count N --warmup W --frames F --out <경로.csv> --seed S
```

- **출력 CSV:** 머리글 `frame,frame_ms`, 이어서 기록 프레임 F줄. 워밍업 W프레임은 기록하지 않는다. 실행기는 줄 수가 F가 아니거나 0 이하인 값이 있으면 실패로 기록한다.
- **프레임 시간:** 한 프레임의 시작부터 다음 프레임 시작까지(갱신·그리기·`display()` 포함), 고해상도 시계로 ms.
- **같게 할 것:** 두 대상은 같은 텍스처 1장, 같은 창 크기, 같은 시드로 정한 같은 위치·크기·회전, 같은 움직임 규칙을 쓴다. 다른 것은 그리는 경로뿐이어야 한다.
- **프레임 제한:** meno는 `ApplicationConfig::framerateLimit = 0`, SFML 대상은 `setFramerateLimit(0)`, `setVerticalSync(false)`. meno에는 수직 동기화를 켜고 끄는 API가 없어 SFML 기본값(꺼짐)을 따른다. 드라이버가 강제하는 경우를 막으려고 실행기가 `vblank_mode=0`, `__GL_SYNC_TO_VBLANK=0`을 넘긴다.
- **종료:** 정상 종료면 0, 에셋 로드 실패 등은 0이 아닌 값.

## Meno 쪽에서 정할 것

- **B1. meno 대상의 그리기 경로:** `Renderer`로 직접 그리기, `GameObject` + `Sprite` 컴포넌트로 그리기, 둘 다. 사용자가 실제로 쓰는 경로는 후자라 배칭 판단에는 둘 다 있는 편이 낫다(둘 다면 `--target`에 `meno-object` 같은 값을 하나 더 두고 `bench.toml`의 `targets`에 추가).
- **B2. 움직임 여부:** 스프라이트가 매 프레임 움직이는지. 움직이면 변환 갱신 비용도 함께 잰다.
- **B3. Tracy 연동:** Tracy를 켜는 CMake 옵션(예: `MENO_ENABLE_TRACY`)과 구간 표시 위치. Tracy를 켠 빌드는 측정 수치에 쓰지 않는다(계측 비용이 섞임).

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

- **빌드 폴더:** `work/build-<커밋>/` (커밋 안 함). 이미 빌드한 실행 파일이 있으면 `--bin`으로 준다.
- **실패:** 실패한 측정은 `runs.jsonl`에 `ok: false`와 `stderr_tail`로 남고 표에서 빠진다. 요약 아래에 빠진 목록이 나온다.
- **통계:** 측정마다 평균·p50·p99를 내고(백분위는 #29의 `frametime_stats.py`와 같은 방식), 반복 사이에서는 중앙값과 최소·최대를 낸다.

## Tracy 병목 분석

- 측정과 따로, Tracy를 켠 빌드로 N = 1000, 10000 정도에서 캡처한다.
- `.tracy` 캡처 파일은 크므로 커밋하지 않는다(`.gitignore`). 대신 `results/<…>/tracy/`에 화면 캡처(png)와 `notes.md`(어느 구간이 몇 ms, 무엇이 병목인지)를 둔다.

## 이슈 체크리스트와의 대응

- **측정 실행 파일:** Meno 저장소 `benchmarks/`. 이 폴더의 약속을 따른다.
- **결과 표와 그래프:** `summarize.py`가 만든 `summary.md`, `frametime.png`를 Meno의 `docs/benchmark.md`로 옮긴다.
- **Tracy로 병목 분석:** `results/<…>/tracy/notes.md`.
