# meno_exp

SeoulTech-Fiat-Lux/Meno의 실험 자료.

- Meno#29 LLM 생성 비교 실험: 계획 [PLAN.md](PLAN.md), 운영 규칙 [docs/harness.md](docs/harness.md)
- Meno#30 성능 측정: [benchmark/README.md](benchmark/README.md)

## 구성

```
PLAN.md                     계획, 결정, 할 일
docs/                       운영 규칙(harness.md), 시범 보고, 검증 캡처
LICENSE                     코드 라이선스 (zlib). 폴더별 예외는 아래 "라이선스와 출처"
llm-comparison/
  experiment.toml           실험 설정 (게임, 조건, 모델, 반복·왕복 수)
  specs/                    요구사항 (games/*.md가 LLM 입력 원본), 계산 근거
  assets/                   에셋 52종
  templates/{meno,sfml}/    빌드 템플릿 (LLM은 main.cpp만 작성)
  contexts/                 조건별 문맥 목록과 묶음
  prompts/                  지시문과 피드백 문구
  harness/                  회차 실행 코드 (폴더째 동결)
  grading/                  채점표, 블라인드 묶음, 집계
  analysis/                 요약, 사전 등록 분석, 그림, 대표작
  tests/                    LLM 없이 하니스·채점·분석 확인
  smoke/                    템플릿 점검용 프로그램 (mock 실행의 답으로도 씀)
  runs/                     회차 결과
benchmark/                  #30 성능 측정
```

## 본 실험 따라 하기

본 실험은 아래 순서로 진행한다. 각자 자기 역할의 단계만 위에서부터 차례로 따라 한다.

| 단계 | 누가 | 하는 일 |
|---|---|---|
| 0. 공통 준비 | 모두 | 설치, clone, 테스트 |
| 1. 본 실행 | Claude 1명, Codex 2명 | 자기 몫의 회차를 돌려 묶음 담당에게 보낸다 |
| 2. 블라인드 묶음 | 묶음 담당 1명 (Claude 실행자 권장) | 결과를 합쳐 채점자에게 나눠 준다 |
| 3. 채점 | 채점자 3명 | 플레이하며 채점표를 채워 돌려준다 |
| 4. 집계 | 묶음 담당 | 채점표를 모아 일치도를 내고, 합의 뒤 통과율을 낸다 |
| 5. 분석과 마무리 | 묶음 담당 | 분석, 그림, 대표작, 커밋 |

명령은 따로 적지 않으면 모두 WSL(또는 Ubuntu)의 `~/meno_exp/llm-comparison`에서 실행한다.

### 0. 공통 준비 (모두)

WSL에서는 반드시 리눅스 쪽 홈에 clone한다. `/mnt/c/...`에서는 빌드가 느리고 심볼릭 링크와 실행 권한이 깨진다.
하니스는 LLM이 만든 코드를 실행하므로, 개인 파일이 없는 전용 배포판이나 계정을 권한다.

```bash
sudo apt-get update && sudo apt-get install -y build-essential cmake git python3 python3-venv xvfb ffmpeg x11-utils \
  xdotool bc libxrandr-dev libxcursor-dev libxi-dev libudev-dev libfreetype-dev libgl1-mesa-dev \
  libegl1-mesa-dev libflac-dev libvorbis-dev libogg-dev libharfbuzz-dev
```

```bash
git clone https://github.com/SeoulTech-Fiat-Lux/meno_exp.git ~/meno_exp
```

```bash
cd ~/meno_exp/llm-comparison && python3 tests/run_all.py
```

테스트가 모두 통과해야 한다. 파이썬 패키지는 설치하지 않는다(Python 3.11 이상, 표준 라이브러리만).

### 1. 본 실행 (Claude 1명, Codex 2명)

**1-1. 구독 CLI를 설치하고 로그인한다.** 설치 방법이 바뀌었으면 각 공식 안내를 따른다.

- Claude Max 구독자: Claude Code를 설치하고(`curl -fsSL https://claude.ai/install.sh | bash`) `claude`로 로그인한다.
  `ANTHROPIC_API_KEY`는 설정하지 않는다.
- ChatGPT Plus 구독자: Codex CLI를 설치하고(`sudo apt-get install -y nodejs npm && sudo npm install -g @openai/codex`)
  `codex login`으로 로그인한다.
  `~/.codex/AGENTS.md`, `~/.codex/AGENTS.override.md`가 있으면 이름을 바꿔 둔다. `codex`의 `/model` 목록에
  `gpt-6.1-sol`이 없으면 팀에 알린다.

**1-2. 동결된 최신 상태인지 확인한다.** `동결 상태와 같음`이 나와야 한다. 다르면 돌리지 말고 팀에 알린다.
옛 커밋으로 돌린 결과는 2단계에서 합쳐지지 않는다.

```bash
git pull && python3 harness/freeze.py check
```

**1-3. 연결을 확인한다.** 출력의 모든 줄이 `OK`여야 한다.

```bash
python3 harness/run.py --models top,light --probe        # Claude 구독자
```

```bash
python3 harness/run.py --models other-vendor --probe     # ChatGPT 구독자
```

Codex에서 실패하면 출력 전체를 팀에 공유한다. 고칠 곳은 `harness/llm.py`의 `CodexCLI`이다.

**1-4. 자기 몫을 돌린다.** 끝난 회차는 건너뛰므로, 창을 닫았거나 구독 한도로 멈췄으면 같은 명령을 다시 실행한다.

```bash
python3 harness/run.py --games main --models top,light                  # Claude Max 구독자: 72회차
```

```bash
python3 harness/run.py --games main --models other-vendor --shard 1/2   # ChatGPT 구독자 1: 18회차
```

```bash
python3 harness/run.py --games main --models other-vendor --shard 2/2   # ChatGPT 구독자 2: 18회차
```

- 본 실행 중에는 같은 PC에서 다른 무거운 작업을 하지 않는다. 프레임 시간이 흔들린다.
- `runs/`는 push하지 않는다. 먼저 올리면 채점자가 조건을 알 수 있다.

**1-5. 결과를 압축해 묶음 담당에게 보낸다** (git 밖, 예: 메신저나 드라이브). 묶음 담당 본인은 보내지 않는다.

```bash
tar czf ~/runs-<이름>.tgz -C runs main
```

### 2. 블라인드 묶음 (묶음 담당)

**2-1. 받은 결과를 푼다.** 사람마다 다른 폴더에 푼다.

```bash
mkdir -p ~/incoming/chatgpt1 && tar xzf ~/runs-chatgpt1.tgz -C ~/incoming/chatgpt1 --strip-components=1
```

```bash
mkdir -p ~/incoming/chatgpt2 && tar xzf ~/runs-chatgpt2.tgz -C ~/incoming/chatgpt2 --strip-components=1
```

**2-2. 합치고 검사한다.** 묶음 담당이 Claude 실행자이면 자기 결과는 이미 `runs/main`에 있으므로 첫 원본으로 `runs/main`을 그대로 준다.
아니면 Claude 결과도 2-1처럼 `~/incoming/claude`에 풀고 그 경로를 준다. 입력 해시가 다르거나 빠진 회차가 있으면 합치지 않고 멈춘다.

```bash
python3 grading/merge_runs.py runs/main runs/main ~/incoming/chatgpt1 ~/incoming/chatgpt2
```

**2-3. 블라인드 폴더를 만든다.** `~/blind`는 없거나 비어 있어야 한다. 이미 있으면 지우거나 다른 이름을 쓴다.
대응표(`~/sealed/...`)는 반드시 저장소 밖에 두고, 채점이 끝날 때까지 아무도 열지 않는다. 가능하면 채점하지 않는 사람이 보관한다.

```bash
python3 grading/blind.py runs/main ~/blind ~/sealed/meno-exp-mapping.csv --graders 이름1,이름2,이름3
```

**2-4. 묶음을 압축해 채점자 모두에게 보낸다.** 배정은 `~/blind/assignments.csv`에 있다(ID별 채점자).
zip이 아니라 tar를 쓴다. zip은 에셋 심볼릭 링크를 깨뜨린다.

```bash
tar czf ~/blind.tgz -C ~ blind
```

### 3. 채점 (채점자)

**3-1. 런타임 패키지를 설치하고 묶음을 푼다.**

```bash
sudo apt-get install -y libx11-6 libxrandr2 libxcursor1 libxi6 libudev1 libgl1 libgl1-mesa-dri \
  libfreetype6 libharfbuzz0b libflac12t64 libvorbis0a libvorbisenc2 libvorbisfile3 libogg0
```

```bash
tar xzf ~/blind.tgz -C ~
```

**3-2. 배정된 폴더마다 게임을 실행해 채점한다.** `~/blind/assignments.csv`에서 자기 이름이 적힌 ID만 채점한다.

```bash
cd ~/blind/<ID> && ./game
```

- 채점표 `~/blind/<ID>/sheet-<자기 이름>.csv`를 연다. Windows에서는 `explorer.exe ~/blind`로 폴더를 열어 Excel로 열면 된다.
- `result` 칸에 통과는 `O`, 실패는 `X`를 적는다. `requirement`의 한 문장만 확인한다.
- 수치는 재지 않는다. 눈에 띄게 다르면 실패이고, 애매하면 `note`에 적어 두면 합의 회의에서 정한다.
- 효과음은 채점 전에 `~/blind/_assets/sfx/`의 파일을 한 번씩 들어 둔다.
- **Excel에서 저장할 때는 형식을 "CSV UTF-8 (쉼표로 분리)"로 둔다.** 그냥 "CSV"로 저장하면 한국어가 깨진다.
- `~/blind` 안의 파일은 Windows 탐색기로 지우지 않는다(심볼릭 링크 때문에 오류가 난다).

**3-3. 채운 채점표만 묶어 묶음 담당에게 돌려준다.**

```bash
cd ~ && tar czf ~/sheets-<자기 이름>.tgz blind/*/sheet-<자기 이름>.csv
```

### 4. 집계 (묶음 담당)

**4-1. 돌려받은 채점표를 제자리에 푼다.** 채점자마다 한 번씩 실행한다. 빈 채점표가 채워진 것으로 바뀐다.

```bash
tar xzf ~/sheets-<채점자 이름>.tgz -C ~
```

**4-2. 대응표 없이 일치도를 낸다.** 빈 칸이 있으면 `grading/results/incomplete.csv`에 나오므로, 해당 채점자에게 마저 받는다.

```bash
python3 grading/collect.py agree ~/blind grading/results
```

**4-3. 합의 회의를 한다.** `grading/results/disagreements.csv`의 어긋난 항목을 함께 다시 보고 `final` 칸에 `O`/`X`를 채운다.

**4-4. 처음으로 대응표를 열어 회차별 통과율을 낸다.**

```bash
python3 grading/collect.py final ~/blind grading/results ~/sealed/meno-exp-mapping.csv
```

### 5. 분석과 마무리 (묶음 담당)

**5-1. 그림을 그린다** (matplotlib이 필요한 유일한 단계). 보고서보다 먼저 그려야 보고서 본문에 그림이 들어간다.

```bash
python3 -m venv .venv && .venv/bin/pip install -r analysis/requirements.txt
```

```bash
.venv/bin/python analysis/figures.py runs/main grading/results/run_scores.csv analysis/results
```

**5-2. 사전 등록한 분석을 돌린다.** 결과는 `analysis/results/report.md`다. 맨 위 "요약"에 1차 지표와 민감도 판정이 있다.
읽기용 docx는 이 파일을 Claude에 넣어 만들되, 숫자와 판정 문구는 고치지 않고 옮기게 한 뒤 원본과 대조한다.

```bash
python3 analysis/analyze.py runs/main grading/results/run_scores.csv analysis/results
```

**5-3. 대표작을 녹화한다.** 폴더를 만든 뒤 `cd ~/showcase/<게임>-<조건> && ./game`으로 띄워 직접 플레이하며
Windows 게임 바(Win+Alt+R)로 녹화한다.

```bash
python3 analysis/showcase.py runs/main grading/results/run_scores.csv ~/showcase
```

**5-4. 결과를 커밋하고 push한다.** 이때 처음으로 `runs/`를 올린다. 실행 파일과 녹화는 `.gitignore`에 있어 올라가지 않는다.

```bash
cp ~/sealed/meno-exp-mapping.csv grading/mapping.csv
```

```bash
git add runs/main grading/mapping.csv grading/results analysis/results && git commit -m "research: Add main run results" && git push
```

## 참고: 시범 실행과 점검 명령

`llm-comparison/`에서 실행한다. 시범은 이미 끝났다([docs/pilot-2026-11-14.md](docs/pilot-2026-11-14.md)). 다시 돌리면 구독 한도를 쓴다.

```bash
python3 harness/run.py --games pilot --mock --mock-fail-first --reps 1   # LLM 없이 흐름 점검
```

```bash
python3 harness/run.py --games main --dry-run           # 실행할 회차 목록과 기록 파일
```

```bash
python3 harness/run.py --games pilot --measure-context  # 조건별 문맥 토큰 재기 (3호출, runs/context_tokens.jsonl)
```

```bash
python3 harness/run.py --games pilot                    # 시범 실행 (퐁 × 2조건 × 3회)
```

```bash
python3 analysis/summarize.py runs/pilot-<입력 해시>      # 결과 요약과 본 실행 추정 (폴더 이름은 시범 실행이 출력함)
```

```bash
python3 analysis/analyze.py runs/pilot-<입력 해시> - analysis/pilot   # 채점 전: 하니스 지표만
```

## 참고: 동결

`freeze-2`로 동결했다. 이후 명세·에셋·템플릿·문맥·지시문·채점표·하니스를 바꾸면 모든 회차를 다시 돌린다.

다시 동결해야 할 때:
1. 문맥 묶음을 고정 커밋에서 다시 만든다.

   ```bash
   cd llm-comparison/contexts && mkdir -p src
   git clone https://github.com/SeoulTech-Fiat-Lux/Meno.git src/Meno && git -C src/Meno checkout 46f8934f0f7754a7e09033c4413b6a5222d4493d
   git clone https://github.com/SFML/SFML.git src/SFML && git -C src/SFML checkout 8835b6b955c96fa1ad1278a8dae6f05c4143effc
   git clone https://github.com/SFML/SFML-Website.git src/SFML-Website && git -C src/SFML-Website checkout 6d461d96c47d795c671f10e723a1b9ce13533edb
   python3 build_context.py manifest.json meno bundles/meno.md SeoulTech-Fiat-Lux/Meno=src/Meno
   python3 build_context.py manifest.json sfml bundles/sfml.md SFML/SFML=src/SFML SFML/SFML-Website=src/SFML-Website
   ```

2. `python3 tests/run_all.py`로 확인한다.
3. 잠금 파일을 만든다. 일관성 검사에 걸리면 잠그지 않는다.

   ```bash
   python3 harness/freeze.py write
   ```

4. 잠금 파일을 커밋하고 다음 번호의 태그(`freeze-N`)를 달아 push한다. 실행 담당은 1-2부터 다시 한다.

## 라이선스와 출처

이 저장소에서 만든 것:

| 대상 | 라이선스 | 파일 |
|---|---|---|
| 코드 (`harness/`, `grading/`, `analysis/`, `tests/`, `contexts/*.py`, `specs/sim/*.py`, `benchmark/*.py`) | zlib | `LICENSE` |
| 요구사항 문서 (`llm-comparison/specs/`) | CC BY 4.0 | `llm-comparison/specs/LICENSE` |
| 에셋 (`llm-comparison/assets/` 중 폰트 제외) | CC BY 4.0 | `llm-comparison/assets/LICENSE`. 제작 방식은 `assets-spec.xlsx`의 출처·라이선스 열 |
| 실험 결과 (`llm-comparison/runs/`, `benchmark/results/`) | CC BY 4.0 | 각 폴더의 `LICENSE` |

가져온 것:

| 대상 | 출처 | 라이선스 |
|---|---|---|
| `llm-comparison/assets/fonts/game.ttf` | Neo둥근모 (Dalgona.) https://neodgm.dalgona.dev/ | SIL OFL 1.1 (`fonts/LICENSE.txt`). 수정하지 않음 |
| `llm-comparison/contexts/bundles/meno.md` | SeoulTech-Fiat-Lux/Meno의 README와 사용자 가이드 | Meno 저장소의 라이선스 (zlib) |
| `llm-comparison/contexts/bundles/sfml.md` 중 readme | SFML/SFML `readme.md` (태그 3.1.0) | zlib |
| `llm-comparison/contexts/bundles/sfml.md` 중 튜토리얼 | SFML/SFML-Website `pages/tutorials/3.1/` — SFML Website by the SFML Team | CC BY 4.0. 파일마다 출처 머리글을 붙였고 내용은 고치지 않음 |
| 빌드 시 내려받는 SFML 3.1.0 | https://github.com/SFML/SFML | zlib |

## 비밀 정보

구독 로그인 정보(`~/.claude`, `~/.codex`)는 저장소에 넣지 않는다. 녹화, 실행 파일, 블라인드 폴더, 대응표는 `.gitignore`에 있다.
