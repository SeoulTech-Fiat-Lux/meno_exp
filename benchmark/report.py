"""최종 보고서 (SeoulTech-Fiat-Lux/Meno#30).

GPU마다 잰 결과 폴더를 모아 보고서 하나를 만든다. 숫자와 문장은 모두 결과 파일에서 계산하므로 사람이 채울 자리는
없다. tracy/notes.md에 사람이 쓴 해석이 있으면 그 GPU의 "병목" 절에 덧붙인다. 실행할 때마다 덮어쓴다.

사용 (benchmark/ 에서, 결과 폴더마다 vsync_check.py·run.py·summarize.py·figures.py·tracy.py capture를 마친 뒤):
    python3 report.py results/<날짜>-<meno 커밋>-<GPU> [결과 폴더 ...]      # → results/report.md

맨 위에 "경고"가 있으면 그 결과는 쓰지 않는다. 원인을 고치고 다시 잰 뒤 돌린다.
"""
from __future__ import annotations

import argparse
import csv
import os
import re
import statistics
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
    """요약 파일을 문서에 넣을 때 맨 위 제목(#)은 빼고, 소제목은 shift 단계 내린다."""
    lines = [l for l in md.splitlines() if not l.startswith("# ")]
    return "\n".join(("#" * shift + l) if re.match(r"#{2,4} ", l) else l for l in lines).strip()


def filled_notes(notes: str) -> str:
    """notes.md에서 사람이 채운 "관찰"과 "판단"만 꺼낸다. 양식을 채우지 않았으면(항목이 ':'로 끝남) 빈 문자열."""
    parts = []
    for title in ("관찰", "판단"):
        m = re.search(rf"^## {title}\s*$(.*?)(?=^## |\Z)", notes, re.S | re.M)
        # 채우지 않은 항목(':'로 끝남)은 빼고 채운 줄만 옮긴다.
        items = [l for l in (m.group(1).strip().splitlines() if m else [])
                 if l.strip() and not l.rstrip().endswith(":")]
        if items:
            parts.append(f"**{title}** (tracy/notes.md, 사람이 씀)\n\n" + "\n".join(items))
    return "\n\n".join(parts)


def ratio_range(folders: list[Folder], a: str, b: str) -> str:
    """모든 GPU, 모든 N에서 a/b(평균)의 범위."""
    xs = [f.mean(a, n) / f.mean(b, n) for f in folders for n in f.counts if f.mean(a, n) and f.mean(b, n)]
    return f"{min(xs):.2f}~{max(xs):.2f}" if xs else "-"


def budget(f: Folder) -> str:
    """대상마다 p99가 60 fps 예산 안인 가장 큰 N. 모든 대상이 끝까지 안이면 한 줄로 줄인다."""
    def last_ok(t: str) -> int | None:
        ok = [n for n in f.counts if (v := f.mean(t, n, "p99")) is not None and v <= BUDGET_MS]
        return ok[-1] if ok else None

    last = {t: last_ok(t) for t in f.targets}
    if all(n == f.counts[-1] for n in last.values()):
        return f"{f.gpu}는 모든 대상이 측정한 가장 큰 N({f.counts[-1]:,})까지 안"
    return f"{f.gpu}: " + ", ".join(f"`{t}` " + (f"N = {n:,}까지 안" if n else "가장 작은 N에서도 넘음")
                                    for t, n in last.items())


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
    power = f.env.get("windows_power", {})
    parts = []
    if power:
        parts.append(f"전원 모드 {power.get('power_mode', '?')}, 최대 프로세서 상태 "
                     f"{power.get('max_processor_state', {}).get('ac', '?')}%")
    host, other = cpu_values(f.runs)
    if host:
        parts.append(f"측정 직전 Windows CPU 중앙값 {statistics.median(host):.0f}%")
    if other:
        parts.append(f"측정 중 다른 WSL 작업 최대 {max(other):.2f}코어")
    return ", ".join(parts) or "기록 없음"


# --- 요약 (모두 결과 파일에서 계산) --------------------------------------------

def summary(folders: list[Folder]) -> list[str]:
    f0 = folders[0]
    n = f0.counts[-1]
    lines = [f"N = {n:,}에서 프레임 시간 평균(ms):", "",
             "| 대상 | " + " | ".join(f.gpu for f in folders) + " |", "|---|" + "---:|" * len(folders)]
    lines += [f"| `{t}` | " + " | ".join(f"{v:.2f}" if (v := f.mean(t, n)) else "-" for f in folders) + " |"
              for t in f0.targets]
    lines.append("")
    if all(f.mean("sfml", n) and f.mean("sfml-batched", n) for f in folders):
        lines.append("- **배칭 효과** (`sfml` / `sfml-batched`, N = " + f"{n:,}): " + ", ".join(
            f"{f.gpu} {f.mean('sfml', n) / f.mean('sfml-batched', n):.2f}배" for f in folders))
    lines.append(f"- **meno가 더하는 비용**: `meno-renderer` / `sfml` {ratio_range(folders, 'meno-renderer', 'sfml')}, "
                 f"`meno-object` / `meno-renderer` {ratio_range(folders, 'meno-object', 'meno-renderer')} "
                 "(모든 GPU·N의 범위. 1.00이면 더하는 비용이 없다)")
    lines.append("- **60 fps 예산** (p99 ≤ 16.7 ms): " + "; ".join(budget(f) for f in folders))
    lines += cross_gpu(folders, n)
    for f in folders:
        if f.zones:
            m = max(f.zones["counts"])
            share = {z: 100 * by[m] / f.zones["frame_ms"][m] for z, by in f.zones["zones"].items()}
            top = sorted(share, key=lambda z: -share[z])[:2]
            lines.append(f"- **Tracy** ({f.gpu}, `{f.zones.get('target', 'meno-object')}` N = {m:,}): "
                         + ", ".join(f"`{z}` {share[z]:.0f}%" for z in top))
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
        verdict = "배칭하지 않은 비용은 GPU를 바꿔도 거의 같으므로 CPU(그리기 호출 제출)가 정한다. 배칭하면 GPU가 정한다."
    elif plain <= SAME_COST:
        verdict = "배칭 여부와 관계없이 GPU를 바꿔도 비용이 거의 같다. CPU가 정한다."
    else:
        verdict = "배칭하지 않은 비용도 GPU에 따라 달라진다. GPU가 정하는 몫이 있다."
    return [f"- **GPU 사이 차이** (같은 CPU, N = {n:,}): `sfml` {100 * plain:.0f}%, `sfml-batched` {100 * batched:.0f}%. "
            f"→ {verdict} (판정 기준: `sfml` 차이 {100 * SAME_COST:.0f}% 이하)"]


# --- 문서 -------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folders", type=Path, nargs="+", help="결과 폴더 (GPU마다 하나)")
    ap.add_argument("--out", type=Path, default=HERE / "results" / "report.md")
    args = ap.parse_args()
    folders = [load(p.resolve()) for p in args.folders]
    doc_dir = args.out.resolve().parent

    def rel(p: Path) -> str:
        """보고서에서 본 상대 경로. 그림은 복사하지 않고 결과 폴더의 것을 가리킨다."""
        return Path(os.path.relpath(p, doc_dir)).as_posix()

    f0 = folders[0]
    sweep = f0.env.get("config", {}).get("sweep", {})

    warn = [f"{f.gpu}: {w}" for f in folders for w in warnings(f)] + cross_warnings(folders)
    head = (["> **경고: 아래를 해결하기 전에는 이 결과를 쓰지 않는다.**", ">", *(f"> - {w}" for w in warn), ""]
            if warn else [])
    env_rows = [("날짜", lambda f: f.env.get("recorded_at", "-")),
                ("GPU (GL 렌더러)", lambda f: f.env.get("gl_renderer", "-")),
                ("CPU", lambda f: f"{f.env.get('cpu', '-')} ({f.env.get('nproc', '-')}스레드)"),
                ("CPU 상태", cpu_state),
                ("OpenGL", lambda f: f.env.get("gl_version", "-")),
                ("OS", lambda f: f.env.get("platform", "-")),
                ("meno", lambda f: f"`{f.env.get('meno', {}).get('commit', '-')[:7]}`"),
                ("측정 프로그램", lambda f: f"meno_exp `{f.env.get('bench_app', {}).get('commit', '-')[:7]}`의 bench_app"),
                ("수직 동기화 확인", lambda f: f.vsync.get("message", "없음")),
                ("결과 폴더", lambda f: f"[{f.out.name}]({rel(f.out)})")]

    doc = ["# 성능 측정: 스프라이트 N개 그리기", "", *head,
           "배칭을 적용할지 판단할 근거로, 스프라이트 N개를 그리는 프레임 시간을 네 경로로 쟀다(SeoulTech-Fiat-Lux/Meno#30). "
           "이 문서는 `report.py`가 결과 폴더에서 만든다. 고치지 말고 다시 만든다.", "",
           "## 요약", "", *summary(folders), "",
           "## 측정 방법", "",
           "| 대상 | 프레임마다 하는 일 |", "|---|---|",
           "| `sfml-batched` | 사각형 N개를 `sf::VertexArray` 하나에 모아 `draw` 1번 (배칭했을 때의 하한) |",
           "| `sfml` | SFML을 직접 써서 `sf::Sprite`마다 `draw` (N번) |",
           "| `meno-renderer` | meno `Game` 루프에서 `Renderer::draw` N번 |",
           "| `meno-object` | `GameObject` + `Sprite` 컴포넌트 N개 (사용자가 실제로 쓰는 경로) |", "",
           f"- N: {', '.join(f'{n:,}' for n in sweep.get('counts', []))}. (대상, N)마다 새 프로세스로 "
           f"{sweep.get('reps', '-')}회, 순서를 섞음. 워밍업 {sweep.get('warmup_frames', '-')}프레임을 버리고 "
           f"{sweep.get('frames', '-')}프레임 기록.",
           "- Release 빌드, 프레임 제한·수직 동기화 끔. 네 대상이 같은 텍스처·시드·이동 함수를 쓴다(고정 dt 1/60초).",
           "- 측정마다 평균·p50·p99를 내고, 반복 사이에서는 중앙값을 쓴다.", "",
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
                f"![큰 N에서 프레임 시간 분포]({fig('4-distribution.png')})", "",
                f"그림의 값: [figures.md]({fig('figures.md')})", ""]
    doc += ["## 병목 (Tracy)", "",
            "Tracy를 켠 빌드로 `meno-object`를 캡처했다. 프레임이 느려지므로 구간 비율만 보고, 프레임 시간은 위 표를 쓴다. "
            "OpenGL 드라이버는 그리기 명령을 모아 두었다가 나중에 처리할 수 있어서, 그리기 비용 일부가 "
            "`window.display`에 잡히기도 한다. `scene.draw`와 `window.display`는 합쳐서 읽는다.", ""]
    for f in folders:
        if not f.zones:
            continue
        doc += [f"### {f.gpu}", "", f"![Tracy 구간별 프레임 비용]({rel(f.out / 'tracy' / 'zones.png')})", "",
                body(read(f.out / "tracy" / "zones.md"), 2), ""]
        if f.notes:
            doc += [f.notes, ""]
    doc += ["## 한계", "",
            "- 수치는 위 측정 환경 기준이다. 다른 GPU·드라이버·CPU에서는 절대값이 달라진다.",
            *(["- WSL2(WSLg)에서 쟀다. Windows 화면 합성을 거치고, OpenGL 호출을 Direct3D 12로 바꾸는 비용이 CPU에 더해진다."]
              if any(f.env.get("wsl") for f in folders) else []),
            f"- CPU는 {', '.join(sorted({f.env.get('cpu', '?') for f in folders}))}만 썼다. 느린 CPU에서의 수치는 없다.",
            "- Tracy 결과는 계측을 켠 빌드에서 얻었다. 비율을 보는 데 쓰고 프레임 시간 표에는 쓰지 않았다.", "",
            "## 재현", "", "meno_exp의 `benchmark/README.md`를 위에서부터 따라 한다.", ""]
    text = re.sub(r"\n{3,}", "\n\n", "\n".join(doc))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text, encoding="utf-8", newline="\n")
    print(f"보고서: {args.out}")
    if warn:
        print("경고:\n  - " + "\n  - ".join(warn))


if __name__ == "__main__":
    main()
