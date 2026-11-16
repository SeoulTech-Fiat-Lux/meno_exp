"""측정 결과 요약 (SeoulTech-Fiat-Lux/Meno#30).

사용: python3 summarize.py <결과 폴더>

raw/의 프레임 시간 CSV를 (대상, N, 반복)마다 평균·p50·p99로 줄이고, 반복 사이에서는 중앙값과 범위를 낸다.
결과 폴더에 쓰는 것:
  summary.csv   (대상, N)마다 통계. figures.py, report.py가 읽는다
  summary.md    사람이 읽는 표
  summary.json  판정 결과(대상 순서, 수직 동기화에 묶인 대상, 실패한 측정). report.py가 경고를 낼 때 읽는다
그림은 figures.py가 그린다.
"""
from __future__ import annotations

import csv
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

from results import ok_runs, raw_path, read_frame_ms, read_runs, require_raw, target_order
from stats import locked, percentile

STATS = ("mean", "p50", "p99")


def per_run(path: Path) -> dict:
    values = sorted(read_frame_ms(path))
    return {"mean": sum(values) / len(values), "p10": percentile(values, 0.10), "p50": percentile(values, 0.50),
            "p99": percentile(values, 0.99)}


def main() -> None:
    out = Path(sys.argv[1])
    require_raw(out)
    runs = read_runs(out)
    ok = ok_runs(runs)
    failed = {(r["target"], r["count"], r["rep"]) for r in runs if not r["ok"]} - ok

    cells: dict[tuple, list[dict]] = defaultdict(list)
    near_refresh: dict[tuple, set] = defaultdict(set)
    for target, count, rep in sorted(ok):
        r = per_run(raw_path(out, target, count, rep))
        cells[target, count].append(r)
        # 프레임이 한 주사율 주기에 촘촘히 모인 측정을 모은다(vsync_check.py와 같은 기준).
        hz, is_locked = locked(r["p50"], r["p10"])
        if is_locked:
            near_refresh[target, hz].add(count)
    # 한 N에서만 주기 근처면 우연일 수 있다. 같은 대상이 서로 다른 N 두 개 이상에서 같은 주기에 머물면
    # N을 바꿔도 프레임 시간이 그대로라는 뜻이므로 동기화에 묶인 것이다.
    vsync_locked = [f"{t}: N = {', '.join(f'{n:,}' for n in sorted(ns))}에서 {1000 / hz:.2f} ms ({hz} Hz)"
                    for (t, hz), ns in sorted(near_refresh.items()) if len(ns) >= 2]

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
    targets = target_order(out, {r["target"] for r in rows})
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
    if vsync_locked:
        lines += [f"> **주의: 수직 동기화에 묶인 대상 {len(vsync_locked)}개.** N을 바꿔도 프레임 시간이 주사율 주기에 머물러 "
                  "N에 따른 차이가 가려진다. 이 결과는 쓰지 말고 vsync_check.py로 환경을 확인한 뒤 다시 잰다.", ">",
                  *(f"> - {x}" for x in vsync_locked), ""]
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
    # 사람이 읽는 summary.md의 문장은 바뀔 수 있다. 다른 도구는 판정을 이 파일에서 읽는다.
    verdicts = {"targets": targets, "counts": counts, "vsync_locked": vsync_locked,
                "failed": [list(f) for f in sorted(failed)]}
    (out / "summary.json").write_text(json.dumps(verdicts, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
