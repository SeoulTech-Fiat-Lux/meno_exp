"""Meno 문서 초안 만들기 (SeoulTech-Fiat-Lux/Meno#30).

측정·요약·그림·Tracy 결과를 모아 Meno 저장소의 docs/benchmark.md 초안과 #30 종료 댓글 초안을 만든다.
숫자와 표는 결과 파일에서 옮기고, 해석과 결론은 사람이 쓰도록 "(작성)" 자리를 남긴다.

사용 (benchmark/ 에서, summarize.py·figures.py·tracy.py 다음에):
    python3 report.py results/<날짜>-<meno 커밋>

결과 (<결과 폴더>/report/):
    docs/benchmark.md               Meno 저장소의 docs/benchmark.md로 옮길 초안
    docs/images/benchmark/*.png     그 문서가 쓰는 그림. Meno의 docs/images/benchmark/로 함께 옮긴다
    issue-30-comment.md             #30 종료 댓글 초안 (이슈 양식의 TL;DR 형식)

옮기기 전에 맨 위의 "초안 경고"가 있으면 먼저 해결한다. 경고가 남은 문서는 결과로 쓰지 않는다.
"""
from __future__ import annotations

import csv
import re
import shutil
import subprocess
import sys
from pathlib import Path

from common import HERE, is_software, read_json
TODO = "(작성)"
IMAGES = "images/benchmark"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def body(md: str) -> str:
    """요약 파일을 문서에 넣을 때 맨 위 제목(#)은 빼고, 소제목은 한 단계 내린다."""
    lines = [l for l in md.splitlines() if not l.startswith("# ")]
    return "\n".join(("#" + l) if re.match(r"#{2,5} ", l) else l for l in lines).strip()


def section(md: str, title_prefix: str) -> str:
    """figures.md처럼 '## n. …'으로 나뉜 파일에서 한 절의 본문만 꺼낸다."""
    m = re.search(rf"^## {re.escape(title_prefix)}.*?$(.*?)(?=^## |\Z)", md, re.S | re.M)
    return m.group(1).strip() if m else ""


def filled_notes(notes: str) -> str:
    """notes.md에서 사람이 채운 "관찰"과 "판단"만 꺼낸다. 양식을 채우지 않았으면(항목이 ':'로 끝남) 빈 문자열을 돌려준다."""
    parts = []
    for title in ("관찰", "판단"):
        m = re.search(rf"^## {title}\s*$(.*?)(?=^## |\Z)", notes, re.S | re.M)
        items = [l for l in (m.group(1).strip().splitlines() if m else []) if l.strip()]
        if any(not l.rstrip().endswith(":") for l in items):
            parts.append(f"### {title}\n\n" + "\n".join(items))
    return "\n\n".join(parts)


def warnings(out: Path, env: dict, vsync: dict, verdicts: dict, notes_ok: bool) -> list[str]:
    w = []
    if out.parent.name == "results-mock" or out.name.endswith("-mock"):
        w.append("가짜(mock) 데이터다. 실제 측정 결과로 다시 만든다.")
    renderer = env.get("gl_renderer", "")
    if not renderer:
        w.append("env.json에 GL 렌더러가 없다. GPU에서 잰 것인지 알 수 없다.")
    elif is_software(renderer):
        w.append(f"소프트웨어 렌더러({renderer})로 쟀다. GPU 성능이 아니다.")
    if env.get("meno", {}).get("dirty"):
        w.append("meno를 고친 채로 빌드했다(dirty). 어느 코드인지 재현할 수 없다.")
    if env.get("bench_app", {}).get("dirty"):
        w.append("측정 프로그램(bench_app/)을 커밋하지 않은 채로 쟀다. 어느 코드인지 재현할 수 없다.")
    if not vsync:
        w.append("vsync.json이 없다. vsync_check.py --out <결과 폴더>로 수직 동기화가 꺼지는지 확인한다.")
    elif not vsync.get("ok"):
        w.append(f"수직 동기화 확인 실패: {vsync.get('message', '')}")
    if verdicts.get("vsync_locked"):
        w.append(f"수직 동기화에 묶인 대상이 있다: {'; '.join(verdicts['vsync_locked'])}")
    if "bench_app" in env and not env["bench_app"].get("commit"):
        w.append("측정 프로그램(bench_app/)의 버전을 알 수 없다. git 저장소 안에서 run.py를 돌렸는지 확인한다.")
    if not env.get("meno", {}).get("commit"):
        w.append("env.json에 meno 커밋이 없다. 어느 meno로 쟀는지 알 수 없다.")
    if not notes_ok:
        w.append("tracy/notes.md의 관찰·판단이 비어 있다.")
    if verdicts.get("failed"):
        w.append(f"실패해 빠진 측정 {len(verdicts['failed'])}개가 있다(summary.md 끝). 표의 반복 수가 모자란다.")
    return w


def key_numbers(out: Path, targets: list[str]) -> list[str]:
    """TL;DR에 넣을 숫자: 가장 큰 N에서의 대상별 평균과 기준 대비 비."""
    rows = list(csv.DictReader((out / "summary.csv").open(newline="", encoding="utf-8")))
    n = max(int(r["count"]) for r in rows)
    mean = {r["target"]: float(r["mean_ms"]) for r in rows if int(r["count"]) == n}
    base = targets[0] if targets else None
    lines = [f"- N = {n:,}: " + ", ".join(f"{t} {mean[t]:.2f} ms" for t in targets if t in mean)]
    if base in mean:
        lines.append("- " + ", ".join(f"{t}/{base} = {mean[t] / mean[base]:.2f}" for t in targets[1:] if t in mean))
    return lines


def main() -> None:
    out = Path(sys.argv[1]).resolve()
    if not (out / "summary.json").exists() or not (out / "figures" / "figures.md").exists():
        sys.exit("summary.json이나 figures/figures.md가 없다. summarize.py와 figures.py를 먼저 돌린다")
    env, vsync, verdicts = read_json(out / "env.json"), read_json(out / "vsync.json"), read_json(out / "summary.json")
    summary, figs = read(out / "summary.md"), read(out / "figures" / "figures.md")
    tracy_dir = out / "tracy"
    zones, notes = read(tracy_dir / "zones.md"), read(tracy_dir / "notes.md")
    sweep = env.get("config", {}).get("sweep", {})
    targets = verdicts["targets"]
    meno = env.get("meno", {})
    # 측정한 당시의 측정 프로그램 버전(env.json). 없으면 지금 저장소의 커밋을 적는다.
    exp_commit = env.get("bench_app", {}).get("commit", "")[:7] or subprocess.run(
        ["git", "-C", str(HERE), "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()

    report = out / "report"
    images = report / "docs" / IMAGES
    images.mkdir(parents=True, exist_ok=True)
    pictures = [p for p in sorted((out / "figures").glob("*.png"))]
    if (tracy_dir / "zones.png").exists():
        pictures.append(tracy_dir / "zones.png")
    for p in pictures:
        shutil.copy2(p, images / ("tracy-zones.png" if p.parent == tracy_dir else p.name))
    img = lambda name, alt: f"![{alt}]({IMAGES}/{name})"

    notes_text = filled_notes(notes)
    warn = warnings(out, env, vsync, verdicts, bool(notes_text))
    head = []
    if warn:
        head = ["> **초안 경고: 아래가 해결되기 전에는 결과로 쓰지 않는다.**", ">",
                *(f"> - {w}" for w in warn), ""]
    wsl_note = ("WSL2(WSLg)에서 쟀다. Windows 화면 합성을 거치므로 리눅스 기본 설치보다 느릴 수 있다. "
                "대상 사이의 비교에는 쓸 수 있지만, 절대 수치는 이 환경 기준이다.") if env.get("wsl") else ""

    doc = f"""# 성능 측정: 스프라이트 N개 그리기

{chr(10).join(head)}
배칭을 적용할지 판단하기 위해, 스프라이트 N개를 그리는 프레임 시간을 SFML 직접 사용과 meno의 두 경로로 나눠 쟀다(SeoulTech-Fiat-Lux/Meno#30).

## 요약

{TODO} 한두 문장으로 결론. 아래 숫자에서 고른다.

{chr(10).join(key_numbers(out, targets))}

## 측정 방법

| 항목 | 값 |
|---|---|
| 대상 | {", ".join(f"`{t}`" for t in targets)} (기준: `{targets[0] if targets else "-"}`) |
| N | {", ".join(f"{n:,}" for n in sweep.get("counts", []))} |
| 반복 | (대상, N)마다 새 프로세스로 {sweep.get("reps", "-")}회, 순서를 섞음 |
| 프레임 | 워밍업 {sweep.get("warmup_frames", "-")}프레임을 버리고 {sweep.get("frames", "-")}프레임 기록 |
| 빌드 | {env.get("cmake_cache", {}).get("CMAKE_BUILD_TYPE:STRING", "Release")}, 프레임 제한·수직 동기화 끔 |
| 움직임 | 시드로 정한 속도로 이동, 경계에서 튕김, 일정하게 회전. 고정 dt 1/60초. 세 대상이 같은 이동 함수를 씀 |
| 통계 | 측정마다 평균·p50·p99, 반복 사이에서는 중앙값 |

- `sfml`: SFML을 직접 써서 스프라이트마다 `draw`
- `meno-renderer`: meno의 `Renderer`로 직접 그리기
- `meno-object`: `GameObject` + `Sprite` 컴포넌트 (사용자가 실제로 쓰는 경로)

## 측정 환경

| 항목 | 값 |
|---|---|
| 날짜 | {env.get("recorded_at", "-")} |
| CPU | {env.get("cpu", "-")} ({env.get("nproc", "-")}스레드) |
| GPU (GL 렌더러) | {env.get("gl_renderer", "-")} |
| OpenGL | {env.get("gl_version", "-")} |
| OS | {env.get("platform", "-")} |
| 컴파일러 | {" ".join(env.get("gcc", [])) or "-"} |
| meno | `{meno.get("commit", "-")[:7]}` ({meno.get("describe", "-")}) |
| 측정 프로그램 | meno_exp `{exp_commit or "-"}`의 `benchmark/bench_app/` (meno를 고치지 않고 공개 API로만 잼) |
| 수직 동기화 확인 | {vsync.get("message", "없음")} |

{wsl_note}

## 결과

### 프레임 시간

{img("1-budget.png", "N에 따른 프레임 시간과 60·30 fps 예산")}

{section(figs, "1.")}

{body(summary)}

### 단계별 비용

{img("2-stages.png", "meno-object 프레임 시간의 단계별 비율")}

{section(figs, "2.")}

{TODO} 어느 단계에서 비용이 붙는가. 배칭이 줄이는 것은 그리기 호출 수다. `sfml` 칸에도 스프라이트마다 `draw`를 부르는 비용이 들어 있다.

### 스프라이트 하나당 비용

{img("3-per-sprite.png", "스프라이트 하나당 평균 비용")}

{section(figs, "3.")}

{TODO} N이 커질 때 평평해지는가(개수에 비례하는 비용이 주도), 내려가는가(고정 비용이 주도).

### 프레임 시간 분포

{img("4-distribution.png", "큰 N에서 프레임 시간 분포")}

{section(figs, "4.")}

{TODO} p99가 높은 이유가 가끔 튀는 프레임인가, 전반적인 느림인가.

## 병목 (Tracy)

{img("tracy-zones.png", "Tracy 구간별 프레임 비용") if (tracy_dir / "zones.png").exists() else "(Tracy 결과 없음. tracy.py capture를 먼저 돌린다)"}

{body(zones)}

{notes_text or TODO + " tracy/notes.md의 관찰·판단을 채운 뒤 report.py를 다시 돌린다."}

## 배칭 판단

- **결론:** {TODO} 적용한다 / 적용하지 않는다 / 조건부
- **근거:** {TODO} 위 숫자 두세 개
- **예상 효과:** {TODO} 줄어들 구간과 그 크기
- **다음 할 일:** {TODO}

## 한계

- 수치는 위 측정 환경 기준이다. {"WSLg를 거친 값이다. " if env.get("wsl") else ""}다른 GPU·드라이버에서는 절대값이 달라진다.
- Tracy 결과는 계측을 켠 빌드에서 얻었다. 비율을 보는 데 쓰고, 프레임 시간 표에는 쓰지 않았다.
- {TODO} 그 밖의 한계

## 재현

meno_exp의 `benchmark/README.md`를 따른다.

```bash
python3 vsync_check.py --out results/<결과 폴더>
python3 run.py
python3 summarize.py results/<결과 폴더> && python3 figures.py results/<결과 폴더>
python3 tracy.py capture
```
"""
    doc = re.sub(r"\n{3,}", "\n\n", doc)
    (report / "docs" / "benchmark.md").write_text(doc, encoding="utf-8")

    comment = f"""<!-- #30 종료 댓글 초안. 숫자는 결과에서 옮겼다. (작성) 자리를 채운 뒤 올린다. -->
측정 결과는 `docs/benchmark.md`에 정리했다 (meno `{meno.get("commit", "-")[:7]}`, {env.get("gl_renderer", "-")}).

{chr(10).join(key_numbers(out, targets))}

**배칭 판단:** {TODO}

### 🇬🇧 TL;DR for English Readers

**:sparkles: Proposed Feature:**
A sprite-count benchmark (N = {", ".join(f"{n:,}" for n in sweep.get("counts", []))}) comparing raw SFML, meno's `Renderer`, and meno's `GameObject` + `Sprite` path, to decide whether meno needs sprite batching.

**:hammer_and_wrench: Implementation Details:**
A standalone benchmark program fetches meno at the frozen commit and uses only its public API, so meno itself is unchanged. It draws N moving sprites with frame limit and vsync off, updating exactly once per frame on every path; a runner repeats each (target, N) {sweep.get("reps", "-")} times in fresh processes and reports mean and p99 frame time. A Tracy build replays `Game::run()` step by step through public calls to break a frame into stages by self time. {TODO}

**:microscope: Academic/Technical Significance:**
{TODO} What the stage breakdown shows about where the cost of a high-level 2D API comes from.
"""
    (report / "issue-30-comment.md").write_text(comment, encoding="utf-8")
    print(f"초안: {report / 'docs' / 'benchmark.md'}")
    print(f"그림 {len(pictures)}장: {images}")
    print(f"댓글 초안: {report / 'issue-30-comment.md'}")
    if warn:
        print("초안 경고:\n  - " + "\n  - ".join(warn))


if __name__ == "__main__":
    main()
