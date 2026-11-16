"""결과 보고서 만들기 (SeoulTech-Fiat-Lux/Meno#30).

측정·요약·그림·Tracy 결과를 모아 결과 보고서를 만든다. 보고서는 meno_exp의 결과 폴더에
그대로 두고 커밋한다(Meno 저장소로 옮기지 않음). 숫자와 표는 결과 파일에서 옮기고, 해석과 결론은 사람이 쓰도록
"(작성)" 자리를 남긴다.

사용 (benchmark/ 에서, summarize.py·figures.py·tracy.py 다음에):
    python3 report.py results/<날짜>-<meno 커밋>-<GPU>

결과 (<결과 폴더>/report/):
    benchmark.md            결과 보고서. "(작성)" 자리를 채워 완성한다
    (그림은 복사하지 않고 결과 폴더의 figures/*.png, tracy/zones.png를 상대 경로로 가리킨다)

맨 위에 "초안 경고"가 있으면 먼저 해결한다. 경고가 남은 보고서는 결과로 쓰지 않는다.
"(작성)" 자리를 모두 채운 benchmark.md는 덮어쓰지 않는다. 다시 만들려면 지운 뒤 돌린다.
"""
from __future__ import annotations

import csv
import re
import subprocess
import sys
from pathlib import Path

from common import HERE
from probe import is_software
from results import read_json, read_runs
TODO = "(작성)"


def img(path: str, alt: str) -> str:
    """report/benchmark.md에서 본 상대 경로로 그림을 가리킨다. 복사하지 않아 같은 파일이 두 벌 생기지 않는다."""
    return f"![{alt}](../{path})"


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


# 측정 중 WSL 안에서 측정 프로그램 밖이 쓴 CPU(코어 수)가 이보다 많으면 경고. 쉴 때는 0인데, 측정 중에는 창을 화면에
# 합성하는 WSLg와 GPU로 넘기는 커널 작업 몫으로 0.3~0.6코어가 늘 잡힌다(RTX 5070 Ti 기준). 그 위로 병렬 빌드처럼
# 코어를 여럿 쓰는 작업이 겹쳤을 때만 잡는다.
OTHER_CORES_LIMIT = 1.5
# 측정 직전 Windows 전체 CPU 사용률(%)이 이보다 높으면 경고. 다른 프로그램을 거의 닫은 상태에서 4~22%였다(16스레드에서 한 코어가 약 6%).
HOST_LOAD_LIMIT = 30.0


def cpu_values(runs: list[dict]) -> tuple[list[float], list[float]]:
    """runs.jsonl에 남은 (측정 직전 Windows CPU 사용률, 측정 중 WSL의 측정 프로그램 밖 CPU). 기록이 없는 옛 결과는 빈 목록."""
    host = [r["host_cpu_pct"] for r in runs if r.get("host_cpu_pct") is not None]
    other = [r["cpu"]["other_cores"] for r in runs if (r.get("cpu") or {}).get("other_cores") is not None]
    return host, other


def cpu_warnings(env: dict, runs: list[dict]) -> list[str]:
    """CPU 상태 때문에 프레임 시간이 달라졌을 수 있는 경우."""
    w = []
    cap = env.get("windows_power", {}).get("max_processor_state", {}).get("ac")
    if cap is not None and cap < 100:
        w.append(f"Windows 최대 프로세서 상태가 {cap}%로 제한된 채 쟀다. CPU 비교 측정이 아니라면 100%로 되돌리고 다시 잰다.")
    host, other = cpu_values(runs)
    if busy := sum(v > OTHER_CORES_LIMIT for v in other):
        w.append(f"측정 중 WSL 안에서 측정 프로그램 밖이 CPU를 {OTHER_CORES_LIMIT}코어 넘게 쓴 측정이 {busy}개 있다"
                 "(runs.jsonl의 cpu.other_cores). 병렬 빌드 같은 작업이 겹쳤는지 확인한다.")
    if busy := sum(v > HOST_LOAD_LIMIT for v in host):
        w.append(f"측정 직전 Windows CPU 사용률이 {HOST_LOAD_LIMIT:.0f}%를 넘은 측정이 {busy}개 있다"
                 "(runs.jsonl의 host_cpu_pct). 다른 프로그램을 닫고 다시 잰다.")
    return w


def cpu_state(env: dict, runs: list[dict]) -> str:
    """측정 환경 표의 "CPU 상태" 칸: 전원 설정과 측정 중 CPU 사용 요약."""
    power = env.get("windows_power", {})
    parts = []
    if power:
        cap = power.get("max_processor_state", {}).get("ac", "?")
        parts.append(f"전원 계획 {power.get('scheme', '?')}, 전원 모드 {power.get('power_mode', '?')}, 최대 프로세서 상태 {cap}%")
    host, other = cpu_values(runs)
    if host:
        parts.append(f"측정 직전 Windows CPU 사용률 중앙값 {sorted(host)[len(host) // 2]:.0f}% (최대 {max(host):.0f}%)")
    if other:
        parts.append(f"측정 중 WSL에서 측정 프로그램 밖 CPU 최대 {max(other):.2f}코어")
    return "; ".join(parts) or "기록 없음"


def warnings(out: Path, env: dict, runs: list[dict], vsync: dict, verdicts: dict, notes_ok: bool) -> list[str]:
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
    w += cpu_warnings(env, runs)
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
    runs = read_runs(out)
    summary, figs = read(out / "summary.md"), read(out / "figures" / "figures.md")
    tracy_dir = out / "tracy"
    zones, notes = read(tracy_dir / "zones.md"), read(tracy_dir / "notes.md")
    sweep = env.get("config", {}).get("sweep", {})
    targets = verdicts["targets"]
    meno = env.get("meno", {})
    # 측정 당시의 측정 프로그램 버전(env.json). 없으면 지금 저장소의 커밋을 적는다.
    exp_commit = env.get("bench_app", {}).get("commit", "")[:7] or subprocess.run(
        ["git", "-C", str(HERE), "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip()

    report = out / "report"
    report.mkdir(parents=True, exist_ok=True)

    notes_text = filled_notes(notes)
    warn = warnings(out, env, runs, vsync, verdicts, bool(notes_text))
    head = []
    if warn:
        head = ["> **초안 경고: 아래가 해결되기 전에는 결과로 쓰지 않는다.**", ">",
                *(f"> - {w}" for w in warn), ""]
    wsl_note = ("WSL2(WSLg)에서 쟀다. Windows 화면 합성을 거치므로 리눅스 기본 설치보다 느릴 수 있다. "
                "대상 사이의 비교에는 쓸 수 있지만, 절대 수치는 이 환경 기준이다.") if env.get("wsl") else ""

    doc = f"""# 성능 측정: 스프라이트 N개 그리기

{chr(10).join(head)}
배칭을 적용할지 판단하기 위해, 스프라이트 N개를 그리는 프레임 시간을 배칭한 SFML, SFML 직접 사용, meno의 두 경로로 나눠 쟀다(SeoulTech-Fiat-Lux/Meno#30).

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
| 움직임 | 시드로 정한 속도로 이동, 경계에서 튕김, 일정하게 회전. 고정 dt 1/60초. 네 대상이 같은 이동 함수를 씀 |
| 통계 | 측정마다 평균·p50·p99, 반복 사이에서는 중앙값 |

- `sfml-batched`: 사각형 N개를 `sf::VertexArray` 하나에 모아 `draw`를 한 번만 부름 (배칭했을 때의 하한선)
- `sfml`: SFML을 직접 써서 스프라이트마다 `draw`
- `meno-renderer`: meno의 `Renderer`로 직접 그리기
- `meno-object`: `GameObject` + `Sprite` 컴포넌트 (사용자가 실제로 쓰는 경로)

## 측정 환경

| 항목 | 값 |
|---|---|
| 날짜 | {env.get("recorded_at", "-")} |
| CPU | {env.get("cpu", "-")} ({env.get("nproc", "-")}스레드) |
| CPU 상태 | {cpu_state(env, runs)} |
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

{img("figures/1-budget.png", "N에 따른 프레임 시간과 60·30 fps 예산")}

{section(figs, "1.")}

{body(summary)}

### 단계별 비용

{img("figures/2-stages.png", "meno-object 프레임 시간의 단계별 비율")}

{section(figs, "2.")}

{TODO} 어느 단계에서 비용이 붙는가. `sfml-batched → sfml` 칸이 스프라이트마다 `draw`를 부르는 비용, 곧 배칭으로 줄일 수 있는 몫이다.

### 스프라이트 하나당 비용

{img("figures/3-per-sprite.png", "스프라이트 하나당 평균 비용")}

{section(figs, "3.")}

{TODO} N이 커질 때 평평해지는가(개수에 비례하는 비용이 주도), 내려가는가(고정 비용이 주도).

### 프레임 시간 분포

{img("figures/4-distribution.png", "큰 N에서 프레임 시간 분포")}

{section(figs, "4.")}

{TODO} p99가 높은 이유가 가끔 튀는 프레임인가, 전반적인 느림인가.

## 병목 (Tracy)

{img("tracy/zones.png", "Tracy 구간별 프레임 비용") if (tracy_dir / "zones.png").exists() else "(Tracy 결과 없음. tracy.py capture를 먼저 돌린다)"}

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
    target = report / "benchmark.md"
    if target.exists() and TODO not in target.read_text(encoding="utf-8"):
        # 사람이 (작성) 자리를 모두 채운 보고서는 덮어쓰지 않는다. 다시 만들려면 지우고 돌린다.
        sys.exit(f"{target}는 이미 완성된 보고서다(\"{TODO}\" 자리가 없음). 다시 만들려면 지운 뒤 돌린다")
    target.write_text(doc, encoding="utf-8")
    print(f"보고서: {target}")
    if warn:
        print("초안 경고:\n  - " + "\n  - ".join(warn))


if __name__ == "__main__":
    main()
