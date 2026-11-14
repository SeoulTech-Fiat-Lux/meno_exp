# meno_exp

meno 실험 저장소. 공개 저장소 SeoulTech-Fiat-Lux/Meno의 다음 이슈를 위한 자료를 둔다.

- SeoulTech-Fiat-Lux/Meno#29 LLM 생성 비교 실험
- SeoulTech-Fiat-Lux/Meno#30 성능 측정 (결과만 둠. 측정 코드는 Meno 저장소에 있음)

실험 설계와 정한 사항(D1–D12)은 [PLAN.md](PLAN.md)에 있다.

## 구성

```
PLAN.md                     실행 계획과 결정 사항
LICENSES.md                 라이선스와 출처
llm-comparison/
  experiment.toml           실험 설정 (게임, 조건, 모델, 반복·왕복 수)
  specs/                    요구사항 (games/*.md가 LLM 입력 원본), 변경 기록, 발표용 pptx
  assets/                   에셋 52종과 폰트 라이선스, assets-spec.xlsx
  templates/{meno,sfml}/    빌드 템플릿 (LLM은 main.cpp만 작성)
  contexts/                 조건별 문맥 규칙(manifest.json), 묶음 생성기, 묶음(bundles/)
  freeze.lock.json          동결 잠금 (동결 때 생성. 본 실행은 이 파일과 입력이 같아야 시작)
  prompts/                  지시문, 빌드 오류·형식 오류·길이 초과 피드백 문구
  harness/                  run.py(회차 실행), freeze.py(동결 잠금), run_noinput.sh(무입력 실행·녹화), frametime_stats.py
  grading/                  채점표 초안(sheets/), 채점표 생성기, 블라인드 묶음 생성기
  smoke/                    템플릿 점검용 프로그램
  runs/                     회차 결과 (runs.jsonl, 회차별 폴더)
```

## 준비 (Ubuntu 24.04 또는 WSL2의 Ubuntu 24.04)

**WSL에서는 저장소를 리눅스 쪽 홈(`~/meno_exp`)에 따로 clone해서 돌린다.** `/mnt/c/...`(Windows 폴더)에서 돌리면
SFML 빌드가 몇 배 느려지고, 블라인드 폴더의 심볼릭 링크와 실행 권한이 제대로 만들어지지 않을 수 있다.

하니스는 LLM이 만든 코드를 그대로 빌드·실행한다. 개인 파일이 없는 전용 WSL 배포판이나 계정에서 돌리기를 권한다.

```bash
sudo apt-get install -y build-essential cmake git python3-pip xvfb ffmpeg x11-utils xdotool bc \
  libxrandr-dev libxcursor-dev libxi-dev libudev-dev libfreetype-dev libgl1-mesa-dev \
  libegl1-mesa-dev libflac-dev libvorbis-dev libogg-dev libharfbuzz-dev
```

```bash
cd llm-comparison && python3 -m venv .venv && . .venv/bin/activate && pip install -r requirements.txt
```

API 키는 `llm-comparison/.env`에 둔다. 커밋하지 않는다.

```
ANTHROPIC_API_KEY=...
OPENAI_API_KEY=...
```

## 실행

모두 `llm-comparison/`에서 실행한다. 이미 `runs.jsonl`에 있는 회차는 건너뛰므로, 중단되면 같은 명령으로 이어서 돌린다.
중간에 끊긴 회차는 이전 폴더를 `<회차 ID>.interrupted-N`으로 옮겨 두고 다시 받으며, 몇 번째 시도인지 기록된다.

```bash
python3 harness/run.py --games pilot --mock --mock-fail-first --reps 1   # API 없이 흐름 점검
```

```bash
python3 harness/run.py --games pilot --dry-run   # 실행할 회차 목록과 기록 폴더
```

```bash
python3 harness/run.py --games pilot             # 시범 실행 (퐁)
```

```bash
python3 harness/run.py --games main              # 본 실행 (동결 상태에서만 시작)
```

- **기록 폴더:** 본 실행은 `runs/main/`이다. 시범 실행과 mock은 `runs/pilot-<입력 해시>/`, `runs-mock/pilot-<입력 해시>/`처럼 입력별로 나뉜다. 명세·지시문 등을 고친 뒤 다시 돌리면 새 폴더에 처음부터 기록된다.
- **빌드 시간:** 조건마다 처음 한 번은 meno·SFML을 내려받아 빌드한다(4코어에서 1분 안팎). 그 뒤 회차마다 `main.cpp`만 바꿔 다시 빌드한다(약 2초).
- **출력 길이:** 모든 모델에 `experiment.toml`의 `max_output_tokens`를 같은 상한으로 준다. 길이 제한으로 잘린 답은 회차 기록에 따로 표시된다.
- **push 시점:** `runs/`는 채점이 끝난 뒤에 push한다. 먼저 올리면 채점자가 코드와 조건을 볼 수 있다.

## 채점

```bash
python3 grading/make_sheets.py            # 채점표 초안 다시 만들기 (확정 후에는 쓰지 않음)
```

```bash
python3 grading/blind.py runs/main ~/blind ~/sealed/meno-exp-mapping.csv --graders A,B,C   # 대응표는 반드시 저장소 밖에
```

- **블라인드 폴더 배포:** `~/blind`를 압축해 채점자에게 나눠 준다. `assignments.csv`에 ID별 채점자가 있다.
- **채점 방법:** 채점자는 자기에게 배정된 `<ID>/`에서 `./game`을 실행하고, `sheet-<자기 이름>.csv`에 통과·실패를 적는다.
- **대응표:** 채점이 끝날 때까지 아무도 열지 않는다. 가능하면 채점하지 않는 사람(지도교수·조교 등)이 보관한다.
- **채점자 PC(WSL Ubuntu 24.04)에 필요한 런타임 패키지:**

```bash
sudo apt-get install -y libx11-6 libxrandr2 libxcursor1 libxi6 libudev1 libgl1 libgl1-mesa-dri \
  libfreetype6 libharfbuzz0b libflac12t64 libvorbis0a libvorbisenc2 libvorbisfile3 libogg0
```

## 동결 절차

본 실행 전에 아래를 순서대로 마치고, 이 저장소에 `freeze-1` 태그를 단다. 이후에는 명세·에셋·템플릿·문맥·지시문·채점표를 바꾸지 않는다(바꾸면 전부 다시 돌린다).

1. **Meno 쪽 정리 (완료):** `v0.2.0` = `46f8934`로 동결했다. 고친 내용은 `specs/CHANGELOG.md`에 있다(D2, D10). **이제 meno 문서는 고치지 않는다.**
2. **meno 버전 고정 (완료):** `templates/meno/CMakeLists.txt`의 `GIT_TAG`와 `contexts/manifest.json`의 meno `commit`이 `46f8934`다.
3. **문맥 묶음 재생성:** 출처를 고정 커밋으로 받아 묶음을 만들고 커밋한다.
   ```bash
   cd llm-comparison/contexts && mkdir -p src
   git clone https://github.com/SeoulTech-Fiat-Lux/Meno.git src/Meno && git -C src/Meno checkout 46f8934f0f7754a7e09033c4413b6a5222d4493d
   git clone https://github.com/SFML/SFML.git src/SFML && git -C src/SFML checkout 8835b6b955c96fa1ad1278a8dae6f05c4143effc
   git clone https://github.com/SFML/SFML-Website.git src/SFML-Website && git -C src/SFML-Website checkout 6d461d96c47d795c671f10e723a1b9ce13533edb
   python3 build_context.py manifest.json meno bundles/meno.md SeoulTech-Fiat-Lux/Meno=src/Meno
   python3 build_context.py manifest.json sfml bundles/sfml.md SFML/SFML=src/SFML SFML/SFML-Website=src/SFML-Website
   ```
4. **설정 확정:** `experiment.toml`의 모델 ID(TBD)와 `games.main`을 정한다.
5. **시범 실행:** `--games pilot`으로 돌려 비용·문맥 창·녹화 판정·채점표를 확인한다. 고칠 수 있는 것은 명세 문구·템플릿·하니스까지다.
6. **채점표 확정:** `grading/sheets/`를 팀 내부 문서의 항목 수와 맞춰 고친다.
7. **잠그기:** `specs/sim/`에 시뮬레이션 스크립트를 넣는다. 그다음 아래 명령으로 잠금 파일(`freeze.lock.json`)을 만들고 커밋한 뒤 `freeze-1` 태그를 단다.
   - 잠금 도구는 일관성을 먼저 검사한다: 템플릿과 문맥 목록의 meno 커밋이 같은지, 문맥 묶음이 목록과 맞는지, 모델 ID가 정해졌는지.
   - 본 실행은 잠금 파일이 없거나 입력이 잠금과 다르면 시작하지 않는다.
   ```bash
   python3 harness/freeze.py write
   ```
   ```bash
   python3 harness/freeze.py check   # 본 실행 직전에 다시 확인
   ```

## 비밀 정보

API 키는 `.env`에만 둔다. 녹화 영상, 실행 파일, 블라인드 폴더, 채점 전 대응표는 커밋하지 않는다(`.gitignore`에 등록됨).
