"""보고서 초안 (SeoulTech-Fiat-Lux/Meno#30).

GPU마다 잰 결과 폴더를 모아 보고서 초안 하나를 만든다. 숫자, 표, 그림은 결과 파일에서 옮기고, tracy/notes.md에
사람이 쓴 해석이 있으면 그 GPU의 "병목" 절에 덧붙인다. 초안은 실행할 때마다 덮어쓰며 커밋하지 않는다.
최종본(results/report.md)은 사람이 초안을 다듬어 저장한 것이고, 이 스크립트는 최종본을 건드리지 않는다.

사용 (benchmark/ 에서, 결과 폴더마다 vsync_check.py·run.py·summarize.py·figures.py·tracy.py capture를 마친 뒤):
    python3 report.py results/<날짜>-<meno 커밋>-<GPU> [결과 폴더 ...]      # → results/report-draft.md

맨 위에 "경고"가 있으면 그 결과는 쓰지 않는다. 원인을 고치고 다시 잰 뒤 돌린다.
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

from common import HERE
from probe import is_software
from results import read_json, read_runs

BUDGET_MS = 1000 / 60  # 60 fps 한 프레임
# 측정 중 WSL 안에서 측정 프로그램 밖이 쓴 CPU(코어 수)가 이보다 많으면 경고. 쉴 때는 0인데, 측정 중에는 창을 화면에
# 합성하는 WSLg와 GPU로 넘기는 커널 작업 몫으로 0.3~0.6코어가 늘 잡힌다(RTX 5070 Ti 기준). 그 위로 병렬 빌드처럼
# 코어를 여럿 쓰는 작업이 겹쳤을 때만 잡는다.
OTHER_CORES_LIMIT = 1.5
# 측정 직전 Windows 전체 CPU 사용률(%)이 이보다 높으면 경고. 다른 프로그램을 거의 닫은 상태에서 4~22%였다(16스레드에서 한 코어가 약 6%).
HOST_LOAD_LIMIT = 30.0
# GPU를 바꿔도 배칭하지 않은 비용이 이 비율 안에서만 달라지면 "CPU가 정한다"고 판정한다(아래 cross_gpu).
SAME_COST = 0.15


@dataclass
class Folder:
    """결과 폴더 하나(GPU 하나)에서 보고서가 쓰는 것."""
    out: Path
    env: dict
    vsync: dict
    verdicts: dict
    runs: list[dict]
    rows: dict = field(default_factory=dict)   # (대상, N) → summary.csv 한 줄
    zones: dict = field(default_factory=dict)  # tracy/zones.json
    notes: str = ""

    @property
    def gpu(self) -> str:
        """"D3D12 (NVIDIA GeForce RTX 5070 Ti)" → "NVIDIA GeForce RTX 5070 Ti"."""
        r = self.env.get("gl_renderer", "") or self.out.name
        m = re.match(r"D3D12 \((.*)\)$", r)
        return m.group(1) if m else r

    @property
    def targets(self) -> list[str]:
        return self.verdicts.get("targets", [])

    @property
    def counts(self) -> list[int]:
        return self.verdicts.get("counts", [])

    def mean(self, target: str, n: int, stat: str = "mean") -> float | None:
        r = self.rows.get((target, n))
        return float(r[f"{stat}_ms"]) if r else None


def load(out: Path) -> Folder:
    if not (out / "summary.json").exists() or not (out / "figures" / "figures.md").exists():
        sys.exit(f"{out}에 summary.json이나 figures/figures.md가 없다. summarize.py와 figures.py를 먼저 돌린다")
    f = Folder(out, read_json(out / "env.json"), read_json(out / "vsync.json"), read_json(out / "summary.json"),
               read_runs(out))
    with (out / "summary.csv").open(newline="", encoding="utf-8") as fh:
        f.rows = {(r["target"], int(r["count"])): r for r in csv.DictReader(fh)}
    z = read_json(out / "tracy" / "zones.json")
    if z:  # JSON 키는 문자열이다
        f.zones = {"counts": z["counts"], "frame_ms": {int(k): v for k, v in z["frame_ms"].items()},
                   "zones": {name: {int(k): v for k, v in by.items()} for name, by in z["zones"].items()}}
    f.notes = filled_notes(read(out / "tracy" / "notes.md"))
    return f


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8") if path.exists() else ""


def body(md: str, shift: int = 0) -> str:
    """요약 파일을 문서에 넣을 때 맨 위 제목(#)과 저장소 파일을 가리키는 끝줄("값은 반복 사이의 …")은 빼고,
    소제목은 shift 단계 내린다. 중앙값이라는 설명은 보고서의 "측정 방법"에 있다."""
    lines = [l for l in md.splitlines() if not l.startswith(("# ", "값은 반복 사이의 중앙값이다."))]
    return "\n".join(("#" * shift + l) if re.match(r"#{2,4} ", l) else l for l in lines).strip()


# Tracy 표에서 빼는 구간. 둘 다 프레임의 0%에 가까워 읽는 사람에게 뜻이 없다(zones.md에는 남아 있다).
HIDDEN_ZONES = ("other zones", "not instrumented")


def tracy_tables(f: Folder) -> list[str]:
    """zones.json으로 구간별 비용 표와 N에 따른 증가 표를 만든다. zones.md는 도구 설명이 섞여 있어 옮기지 않는다."""
    z, tcfg = f.zones, f.env.get("config", {}).get("tracy", {})
    counts = z["counts"]
    names = [name for name in z["zones"] if name not in HIDDEN_ZONES]  # zones.json은 가장 큰 N에서 비싼 순이다
    pct = lambda name, n: 100 * z["zones"][name][n] / z["frame_ms"][n]  # noqa: E731
    lines = [f"워밍업 {tcfg.get('warmup_frames', '-')}프레임을 제외한 {tcfg.get('frames', 0):,}프레임에서 구한 "
             "프레임당 self time(ms)이다. 구간끼리 겹치지 않으므로 모두 더하면 프레임 시간이 된다.", "",
             "| 구간 | " + " | ".join(f"N={n:,} (ms)" for n in counts) + " | "
             + " | ".join(f"N={n:,} (%)" for n in counts) + " |",
             "|---|" + "---:|" * (2 * len(counts))]
    lines += [f"| {name} | " + " | ".join(f"{z['zones'][name][n]:.3f}" for n in counts) + " | "
              + " | ".join(f"{pct(name, n):.0f}" for n in counts) + " |" for name in names]
    lines.append("| **프레임 평균** | " + " | ".join(f"**{z['frame_ms'][n]:.3f}**" for n in counts) + " | "
                 + " | ".join("100" for _ in counts) + " |")
    if len(counts) >= 2:
        a, b = counts[0], counts[-1]
        grow = lambda name: z["zones"][name][b] - z["zones"][name][a]  # noqa: E731
        lines += ["", f"#### N={a:,} → N={b:,}에서 늘어난 비용", "",
                  "N에 비례해 늘어나는 구간은 배칭과 최적화의 대상이고, 거의 변하지 않는 구간은 고정 비용이다.", "",
                  "| 구간 | 늘어난 ms | 배수 |", "|---|---:|---:|"]
        for name in sorted(names, key=lambda x: -grow(x)):
            d = grow(name)
            ratio = z["zones"][name][b] / z["zones"][name][a] if z["zones"][name][a] > 0 else None
            lines.append(f"| {name} | {0.0 if abs(d) < 5e-4 else d:.3f} | "
                         + (f"{ratio:.1f}×" if ratio is not None else "-") + " |")
    return lines


def filled_notes(notes: str) -> str:
    """notes.md에서 사람이 채운 "관찰"과 "판단"만 꺼낸다. 양식을 채우지 않았으면(항목이 ':'로 끝남) 빈 문자열."""
    parts = []
    for title in ("관찰", "판단"):
        m = re.search(rf"^## {title}\s*$(.*?)(?=^## |\Z)", notes, re.S | re.M)
        # 채우지 않은 항목(':'로 끝남)은 빼고 채운 줄만 옮긴다.
        items = [l for l in (m.group(1).strip().splitlines() if m else [])
                 if l.strip() and not l.rstrip().endswith(":")]
        if items:
            parts.append(f"#### {title}\n\n" + "\n".join(items))
    return "\n\n".join(parts)


def ratio_range(folders: list[Folder], a: str, b: str) -> str:
    """모든 GPU, 모든 N에서 a/b(평균)의 범위. 물결표(~)는 두 개가 한 줄에 있으면 Markdown 취소선이 되므로 하이픈을 쓴다."""
    xs = [f.mean(a, n) / f.mean(b, n) for f in folders for n in f.counts if f.mean(a, n) and f.mean(b, n)]
    return f"{min(xs):.2f}-{max(xs):.2f}" if xs else "-"


def budget(folders: list[Folder]) -> str:
    """대상마다 p99가 60 fps 예산 안에 드는 가장 큰 N. 모든 GPU의 모든 대상이 끝까지 들면 한 문장으로 줄인다."""
    def last_ok(f: Folder, t: str) -> int | None:
        ok = [n for n in f.counts if (v := f.mean(t, n, "p99")) is not None and v <= BUDGET_MS]
        return ok[-1] if ok else None

    if all(last_ok(f, t) == f.counts[-1] for f in folders for t in f.targets):
        return f"모든 GPU에서 모든 대상이 측정한 가장 큰 N({folders[0].counts[-1]:,})까지 예산 안에 든다."
    # 대상 이름 뒤 조사(이/가)가 이름마다 달라지므로 "이름: 값" 꼴로 적는다.
    return " ".join(f"{f.gpu}에서 예산 안에 드는 가장 큰 N은 " + ", ".join(
        f"`{t}` " + (f"{n:,}" if (n := last_ok(f, t)) else "없음") for t in f.targets) + "이다." for f in folders)


# --- 경고 -------------------------------------------------------------------

def warnings(f: Folder) -> list[str]:
    """이 폴더의 결과를 쓰면 안 되는 이유."""
    env, w = f.env, []
    if f.out.parent.name == "results-mock" or f.out.name.endswith("-mock"):
        w.append("가짜(mock) 데이터다.")
    renderer = env.get("gl_renderer", "")
    if not renderer:
        w.append("env.json에 GL 렌더러가 없다. GPU에서 잰 것인지 알 수 없다.")
    elif is_software(renderer):
        w.append(f"소프트웨어 렌더러({renderer})로 쟀다. GPU 성능이 아니다.")
    if not env.get("meno", {}).get("commit"):
        w.append("env.json에 meno 커밋이 없다.")
    elif env["meno"].get("dirty"):
        w.append("meno를 고친 채로 빌드했다(dirty).")
    if not env.get("bench_app", {}).get("commit"):
        w.append("측정 프로그램(bench_app/)의 버전을 알 수 없다. git 저장소 안에서 run.py를 돌린다.")
    elif env["bench_app"].get("dirty"):
        w.append("측정 프로그램(bench_app/)을 커밋하지 않은 채로 쟀다. 커밋한 뒤 다시 잰다.")
    if not f.vsync:
        w.append("vsync.json이 없다. vsync_check.py --out <결과 폴더>를 돌린다.")
    elif not f.vsync.get("ok"):
        w.append(f"수직 동기화 확인 실패: {f.vsync.get('message', '')}")
    if f.verdicts.get("vsync_locked"):
        w.append(f"수직 동기화에 묶인 대상이 있다: {'; '.join(f.verdicts['vsync_locked'])}")
    if f.verdicts.get("failed"):
        w.append(f"실패해 빠진 측정 {len(f.verdicts['failed'])}개가 있다(summary.md 끝).")
    if not f.zones:
        w.append("tracy/zones.json이 없다. tracy.py capture --out <결과 폴더>/tracy를 돌린다.")
    if "(작성)" in f.notes:
        w.append('tracy/notes.md에 채우지 않은 "(작성)" 자리가 있다. 채우거나 그 줄을 지운다.')
    cap = env.get("windows_power", {}).get("max_processor_state", {}).get("ac")
    if cap is not None and cap < 100:
        w.append(f"Windows 최대 프로세서 상태가 {cap}%로 제한된 채 쟀다.")
    host, other = cpu_values(f.runs)
    if busy := sum(v > OTHER_CORES_LIMIT for v in other):
        w.append(f"측정 중 WSL 안의 다른 작업이 CPU를 {OTHER_CORES_LIMIT}코어 넘게 쓴 측정이 {busy}개 있다(runs.jsonl의 cpu).")
    if busy := sum(v > HOST_LOAD_LIMIT for v in host):
        w.append(f"측정 직전 Windows CPU 사용률이 {HOST_LOAD_LIMIT:.0f}%를 넘은 측정이 {busy}개 있다(runs.jsonl의 host_cpu_pct).")
    return w


def cross_warnings(folders: list[Folder]) -> list[str]:
    """폴더끼리 비교할 수 없게 만드는 차이."""
    w = []
    if len({f.env.get("meno", {}).get("commit") for f in folders}) > 1:
        w.append("결과 폴더마다 meno 커밋이 다르다.")
    if len({repr(f.env.get("config", {}).get("sweep")) for f in folders}) > 1:
        w.append("결과 폴더마다 bench.toml [sweep]이 다르다(N, 대상, 반복, 프레임 수).")
    return w


def cpu_values(runs: list[dict]) -> tuple[list[float], list[float]]:
    """(측정 직전 Windows CPU 사용률, 측정 중 WSL의 측정 프로그램 밖 CPU). 기록이 없는 옛 결과는 빈 목록."""
    host = [r["host_cpu_pct"] for r in runs if r.get("host_cpu_pct") is not None]
    other = [r["cpu"]["other_cores"] for r in runs if (r.get("cpu") or {}).get("other_cores") is not None]
    return host, other


def cpu_state(f: Folder) -> str:
    """측정 환경 표의 전원 칸. 측정 중 CPU 사용률은 측정이 괜찮았는지 점검하는 값이라 경고로만 쓴다."""
    power = f.env.get("windows_power", {})
    if not power:
        return "기록 없음"
    return (f"전원 모드 {power.get('power_mode', '?')}, 최대 프로세서 상태 "
            f"{power.get('max_processor_state', {}).get('ac', '?')}%")


# --- 요약 (모두 결과 파일에서 계산) --------------------------------------------

def summary(folders: list[Folder]) -> list[str]:
    f0 = folders[0]
    n = f0.counts[-1]
    lines = [f"N = {n:,}에서의 평균 프레임 시간(ms)은 다음과 같다.", "",
             "| 대상 | " + " | ".join(f.gpu for f in folders) + " |", "|---|" + "---:|" * len(folders)]
    lines += [f"| `{t}` | " + " | ".join(f"{v:.2f}" if (v := f.mean(t, n)) else "-" for f in folders) + " |"
              for t in f0.targets]
    lines.append("")
    if all(f.mean("sfml", n) and f.mean("sfml-batched", n) for f in folders):
        lines.append(f"- **배칭 효과**: N = {n:,}에서 `sfml`의 프레임 시간은 `sfml-batched`의 " + ", ".join(
            f"{f.mean('sfml', n) / f.mean('sfml-batched', n):.2f}배({f.gpu})" for f in folders) + "이다.")
    lines.append(f"- **meno가 더하는 비용**: 모든 GPU와 N에서 `meno-renderer`/`sfml`은 "
                 f"{ratio_range(folders, 'meno-renderer', 'sfml')}, `meno-object`/`meno-renderer`는 "
                 f"{ratio_range(folders, 'meno-object', 'meno-renderer')}이다. 1.00에 가까울수록 더하는 비용이 없다는 뜻이다.")
    lines.append("- **60 fps 예산**(p99 16.7 ms 이하): " + budget(folders))
    lines += cross_gpu(folders, n)
    tracy = []
    for f in folders:
        if f.zones:
            m = max(f.zones["counts"])
            share = {z: 100 * by[m] / f.zones["frame_ms"][m] for z, by in f.zones["zones"].items()}
            top = sorted(share, key=lambda z: -share[z])[:2]
            tracy.append(f"{f.gpu}에서는 " + ", ".join(f"`{z}` {share[z]:.0f}%" for z in top))
    if tracy:
        z0 = next(f.zones for f in folders if f.zones)
        lines.append(f"- **Tracy**(`{z0.get('target', 'meno-object')}`, N = {max(z0['counts']):,}): 프레임 시간 중 "
                     + ", ".join(tracy) + "를 차지한다.")
    return lines


def cross_gpu(folders: list[Folder], n: int) -> list[str]:
    """같은 CPU에서 GPU만 바꿔 잰 결과가 둘 이상이면, 배칭 전후 비용을 무엇이 정하는지 판정한다."""
    same_cpu = len({f.env.get("cpu") for f in folders}) == 1
    if len(folders) < 2 or not same_cpu or not all(f.mean("sfml", n) and f.mean("sfml-batched", n) for f in folders):
        return []

    def spread(t: str) -> float:
        xs = [f.mean(t, n) for f in folders]
        return max(xs) / min(xs) - 1

    plain, batched = spread("sfml"), spread("sfml-batched")
    if plain <= SAME_COST and batched > 2 * plain:
        verdict = ("따라서 배칭하지 않을 때의 비용은 CPU(그리기 호출 제출)가 결정하고, "
                   "배칭하면 GPU가 결정한다.")
    elif plain <= SAME_COST:
        verdict = "따라서 배칭 여부와 관계없이 비용은 CPU가 결정한다."
    else:
        verdict = "따라서 배칭하지 않을 때의 비용에도 GPU가 결정하는 몫이 있다."
    return [f"- **GPU 사이 차이**(같은 CPU, N = {n:,}): GPU에 따라 `sfml`은 {100 * plain:.0f}%, "
            f"`sfml-batched`는 {100 * batched:.0f}% 차이가 난다. {verdict}"]


# --- 문서 -------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folders", type=Path, nargs="+", help="결과 폴더 (GPU마다 하나)")
    ap.add_argument("--out", type=Path, default=HERE / "results" / "report-draft.md",
                    help="초안 경로. 최종본(results/report.md)은 사람이 다듬어 저장하므로 여기에 주지 않는다")
    args = ap.parse_args()
    if args.out.resolve() == (HERE / "results" / "report.md").resolve():
        sys.exit("results/report.md는 사람이 다듬은 최종본이라 덮어쓰지 않는다. 초안은 기본 경로(report-draft.md)에 만든다")
    folders = [load(p.resolve()) for p in args.folders]
    doc_dir = args.out.resolve().parent

    def rel(p: Path) -> str:
        """보고서에서 본 상대 경로. 그림은 복사하지 않고 결과 폴더의 것을 가리킨다."""
        return Path(os.path.relpath(p, doc_dir)).as_posix()

    f0 = folders[0]
    sweep = f0.env.get("config", {}).get("sweep", {})

    warn = [f"{f.gpu}: {w}" for f in folders for w in warnings(f)] + cross_warnings(folders)
    head = (["> **경고: 아래 문제를 해결하기 전에는 이 결과를 사용하지 않는다.**", ">", *(f"> - {w}" for w in warn), ""]
            if warn else [])

    def vsync_cell(f: Folder) -> str:
        if not f.vsync:
            return "없음"
        if not f.vsync.get("ok"):
            return f.vsync.get("message", "실패")
        return f"통과(vsync 끔 중앙값 {f.vsync['off']['median_ms']:.2f} ms, 주사율 주기에 묶이지 않음)"

    env_rows = [("측정 시각", lambda f: f.env.get("recorded_at", "-")[:16].replace("T", " ")),
                ("GPU (GL 렌더러)", lambda f: f.env.get("gl_renderer", "-")),
                ("CPU", lambda f: f"{f.env.get('cpu', '-')} ({f.env.get('nproc', '-')}스레드)"),
                ("CPU 상태", cpu_state),
                ("OpenGL", lambda f: f.env.get("gl_version", "-")),
                ("meno", lambda f: f"`{f.env.get('meno', {}).get('commit', '-')[:7]}`"),
                ("수직 동기화 확인", vsync_cell)]

    doc = ["# 성능 측정: 스프라이트 N개 그리기", "", *head,
           "배칭 적용 여부를 판단하기 위해, 스프라이트 N개를 그리는 데 걸리는 프레임 시간을 네 가지 경로로 측정했다"
           "(SeoulTech-Fiat-Lux/Meno#30).", "",
           "## 요약", "", *summary(folders), "",
           "## 측정 방법", "",
           "| 대상 | 프레임마다 하는 일 |", "|---|---|",
           "| `sfml-batched` | 사각형 N개를 `sf::VertexArray` 하나에 모아 `draw`를 한 번 호출한다(배칭했을 때의 하한). |",
           "| `sfml` | SFML의 `sf::Sprite`마다 `draw`를 호출한다(N번). |",
           "| `meno-renderer` | meno의 `Game` 루프에서 `Renderer::draw`를 N번 호출한다. |",
           "| `meno-object` | `GameObject`와 `Sprite` 컴포넌트 N개로 그린다(사용자가 실제로 쓰는 경로). |", "",
           f"- N은 {', '.join(f'{n:,}' for n in sweep.get('counts', []))}이다. (대상, N)마다 새 프로세스로 "
           f"{sweep.get('reps', '-')}회 측정했으며, 측정 순서는 무작위로 섞었다.",
           f"- 측정마다 워밍업 {sweep.get('warmup_frames', '-')}프레임을 버리고 {sweep.get('frames', 0):,}프레임을 기록해 "
           "프레임 시간의 평균, p50, p99를 구했다. 표의 값은 반복 사이의 중앙값이다.",
           "- Release 빌드에서 프레임 제한과 수직 동기화를 끄고 측정했다. 네 대상은 같은 텍스처, 시드, "
           "이동 함수(고정 dt 1/60초)를 쓴다.", "",
           "## 측정 환경", "",
           "| 항목 | " + " | ".join(f.gpu for f in folders) + " |", "|---|" + "---|" * len(folders),
           *(f"| {name} | " + " | ".join(str(get(f)) for f in folders) + " |" for name, get in env_rows), ""]
    for f in folders:
        def fig(name: str, f: Folder = f) -> str:
            return rel(f.out / "figures" / name)

        doc += [f"## 결과: {f.gpu}", "",
                f"![N에 따른 프레임 시간과 60·30 fps 예산]({fig('1-budget.png')})", "",
                body(read(f.out / "summary.md")), "",
                f"![단계별 비율]({fig('2-stages.png')})", "",
                f"![스프라이트 하나당 비용]({fig('3-per-sprite.png')})", "",
                f"![큰 N에서 프레임 시간 분포]({fig('4-distribution.png')})", ""]
    doc += ["## 병목 (Tracy)", "",
            "Tracy를 켠 빌드로 `meno-object`를 캡처했다. 계측 때문에 프레임이 느려지므로 구간의 비율만 해석하고, "
            "프레임 시간은 위의 표를 따른다. OpenGL 드라이버는 그리기 명령을 모아 두었다가 나중에 처리할 수 있으므로, "
            "그리기 비용의 일부가 `window.display`에 잡히기도 한다. 따라서 `scene.draw`와 `window.display`는 합쳐서 읽는다.", ""]
    for f in folders:
        if not f.zones:
            continue
        doc += [f"### {f.gpu}", "", f"![Tracy 구간별 프레임 비용]({rel(f.out / 'tracy' / 'zones.png')})", "",
                *tracy_tables(f), ""]
        if f.notes:
            doc += [f.notes, ""]
    doc += ["## 한계", "",
            "- 모든 수치는 위의 측정 환경을 기준으로 한다. GPU, 드라이버, CPU가 달라지면 절댓값도 달라진다.",
            *(["- WSL2(WSLg)에서 측정했으므로 Windows 화면 합성을 거치며, OpenGL 호출을 Direct3D 12로 변환하는 비용이 "
               "CPU에 더해진다."] if any(f.env.get("wsl") for f in folders) else []),
            f"- CPU는 {', '.join(sorted({f.env.get('cpu', '?') for f in folders}))} 한 종류만 사용했으므로, "
            "느린 CPU에서의 수치는 얻지 못했다.", ""]
    text = re.sub(r"\n{3,}", "\n\n", "\n".join(doc))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8", newline="\n")
    print(f"초안: {args.out}\n다듬어 results/report.md로 저장한다(초안은 커밋하지 않는다).")
    if warn:
        print("경고:\n  - " + "\n  - ".join(warn))


if __name__ == "__main__":
    main()
