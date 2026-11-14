"""측정 결과 요약 (SeoulTech-Fiat-Lux/Meno#30).

사용: python3 summarize.py <결과 폴더>

raw/의 프레임 시간 CSV를 (대상, N, 반복)마다 평균·p50·p99로 줄이고, 반복 사이에서는 중앙값과 범위를 낸다.
summary.csv, summary.md(표), frametime.png(그래프, matplotlib가 있을 때)를 결과 폴더에 쓴다.
"""
from __future__ import annotations

import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

STATS = ("mean", "p50", "p99")


def percentile(sorted_values: list[float], q: float) -> float:
    # llm-comparison/harness/frametime_stats.py와 같은 방식 (가장 가까운 순위)
    return sorted_values[min(len(sorted_values) - 1, int(q * len(sorted_values)))]


def per_run(path: Path) -> dict:
    with path.open(newline="") as f:
        values = sorted(float(r["frame_ms"]) for r in csv.DictReader(f))
    return {"mean": sum(values) / len(values), "p50": percentile(values, 0.50), "p99": percentile(values, 0.99)}


def main() -> None:
    out = Path(sys.argv[1])
    runs = [json.loads(l) for l in (out / "runs.jsonl").read_text(encoding="utf-8").splitlines()]
    ok = {(r["target"], r["count"], r["rep"]) for r in runs if r["ok"]}
    failed = {(r["target"], r["count"], r["rep"]) for r in runs if not r["ok"]} - ok

    cells: dict[tuple, list[dict]] = defaultdict(list)
    for target, count, rep in sorted(ok):
        cells[target, count].append(per_run(out / "raw" / f"{target}-{count}-rep{rep}.csv"))

    rows = []
    for (target, count), reps in sorted(cells.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        row = {"target": target, "count": count, "reps": len(reps)}
        for s in STATS:
            xs = [r[s] for r in reps]
            row |= {f"{s}_ms": statistics.median(xs), f"{s}_min": min(xs), f"{s}_max": max(xs)}
        rows.append(row)
    if not rows:
        sys.exit(f"성공한 측정이 없다. 실패 내용은 {out / 'runs.jsonl'}의 error, stderr_tail")

    with (out / "summary.csv").open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows({k: round(v, 4) if isinstance(v, float) else v for k, v in row.items()} for row in rows)

    by = {(r["target"], r["count"]): r for r in rows}
    counts = sorted({r["count"] for r in rows})
    # 대상 순서는 bench.toml의 targets를 따른다(env.json에 기록됨). 첫 대상이 기준이고, 이어지는 대상은
    # 앞 대상에 한 단계를 얹은 것이라 이웃한 두 대상의 비가 그 단계의 비용이다.
    env_path = out / "env.json"
    order = json.loads(env_path.read_text(encoding="utf-8")).get("config", {}).get("sweep", {}).get("targets", [])         if env_path.exists() else []
    targets = [t for t in order if any(r["target"] == t for r in rows)] +         sorted({r["target"] for r in rows} - set(order))
    pairs = list(zip(targets[1:], targets[:-1]))
    if len(targets) > 2:
        pairs.append((targets[-1], targets[0]))

    def cell(target: str, n: int, k: str) -> str:
        r = by.get((target, n))
        return f"{r[k + '_ms']:.3f}" if r else "-"

    def ratio(a: str, b: str, n: int, k: str) -> str:
        ra, rb = by.get((a, n)), by.get((b, n))
        return f"{ra[k + '_ms'] / rb[k + '_ms']:.2f}" if ra and rb else "-"

    lines = []
    for k, title in (("mean", "평균"), ("p99", "p99")):
        header = ["N", *(f"{t} (ms)" for t in targets), *(f"{a}/{b}" for a, b in pairs)]
        lines += [f"### 프레임 시간 {title}", "", "| " + " | ".join(header) + " |",
                  "|" + "---:|" * len(header)]
        for n in counts:
            row = [str(n), *(cell(t, n, k) for t in targets), *(ratio(a, b, n, k) for a, b in pairs)]
            lines.append("| " + " | ".join(row) + " |")
        lines.append("")
    lines.append("값은 반복 사이의 중앙값이다. 반복별 범위는 summary.csv의 *_min, *_max.")
    if failed:
        lines.append(f"\n실패해 빠진 (대상, N, 반복): {sorted(failed)}")
    (out / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib가 없어 그래프는 건너뜀 (pip install -r requirements.txt)")
        return
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharex=True)
    for ax, s in zip(axes, ("mean", "p99")):
        for target in targets:
            rs = [r for r in rows if r["target"] == target]
            ax.errorbar([r["count"] for r in rs], [r[f"{s}_ms"] for r in rs],
                        yerr=[[r[f"{s}_ms"] - r[f"{s}_min"] for r in rs], [r[f"{s}_max"] - r[f"{s}_ms"] for r in rs]],
                        marker="o", capsize=3, label=target)
        ax.set(xscale="log", yscale="log", xlabel="sprites (N)", ylabel=f"frame time {s} (ms)", title=s)
        ax.set_xlim(counts[0] / 1.3, counts[-1] * 1.3)
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
    fig.tight_layout()
    fig.savefig(out / "frametime.png", dpi=150)
    print(f"그래프: {out / 'frametime.png'}")


if __name__ == "__main__":
    main()
