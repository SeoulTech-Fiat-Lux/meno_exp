"""사전 등록한 분석(PLAN 2.4절)을 그대로 계산한다. 결과를 보기 전에 만든 스크립트이며, 바꾸면 그 사실을 보고서에 적는다.

사용 (llm-comparison/ 에서):
    python3 analysis/analyze.py runs/main grading/results/run_scores.csv analysis/results
    python3 analysis/analyze.py runs/pilot-<해시> - analysis/pilot     # 채점 전: 하니스 지표만 계산 (통과율은 비움)

출력: report.md (1차·2차·탐색 지표), cells.csv (셀별 조건 평균과 차이), errors.csv (빌드 오류 유형)
외부 패키지를 쓰지 않고, 정확 Wilcoxon 검정과 부트스트랩을 직접 계산한다.
"""
from __future__ import annotations

import argparse
import csv
import itertools
import json
import random
import statistics
import sys
import tomllib
from collections import defaultdict
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(BASE / "harness"))
import errors  # noqa: E402
import records as runlog  # noqa: E402

BOOTSTRAP = 10_000


# ---------------------------------------------------------------------------
# 지표 정의 (PLAN 2.4절). value(record, score) → 회차 값 또는 None(빠짐)
# ---------------------------------------------------------------------------
def first_round(record, score):
    detail = record.get("rounds_detail") or []
    return int(bool(detail) and bool(detail[0].get("build_ok")))


METRICS = [
    # (키, 이름, 종류, 함수)
    ("pass_all", "요구사항 통과율 (전체 회차, 빌드 실패 = 0)", "primary",
     lambda r, s: (s if s is not None else None) if r["build_success"] else 0.0),
    ("first_round", "1왕복 빌드 성공률", "secondary", first_round),
    ("built", "최종 빌드 성공률", "secondary", lambda r, s: int(r["build_success"])),
    ("rounds_to_build", "빌드 성공까지의 왕복 수", "secondary",
     lambda r, s: r["rounds"] if r["build_success"] else None),
    ("completion_tokens", "출력 토큰", "secondary", lambda r, s: r.get("completion_tokens")),
    ("pass_built", "요구사항 통과율 (빌드한 회차만)", "secondary", lambda r, s: s if r["build_success"] else None),
    # 팀 내부 문서의 정의("게임 완성 = 대상 항목 모두 통과")를 따른다. 빌드 실패는 미완성(0)으로 센다.
    ("complete", "게임 완성률 (모든 플레이 항목 통과)", "secondary",
     lambda r, s: (int(s >= 1.0) if s is not None else None) if r["build_success"] else 0),
    ("code_lines", "코드 줄 수 (빈 줄 제외)", "exploratory", lambda r, s: r.get("code_lines")),
    ("reasoning_tokens", "추론 토큰", "exploratory", lambda r, s: r.get("reasoning_tokens")),
    ("llm_latency_s", "LLM 시간(초)", "exploratory", lambda r, s: r.get("llm_latency_s")),
    ("early_exit", "무입력 10초 안 종료", "exploratory",
     lambda r, s: int(bool((r.get("noinput") or {}).get("early_exit"))) if r["build_success"] and r.get("noinput") else None),
    ("frame_p95_ok", "프레임 시간 p95 ≤ 20ms", "exploratory",
     lambda r, s: (int(p <= 20.0) if (p := (r.get("noinput") or {}).get("frame_ms_p95")) is not None else None)),
]


# ---------------------------------------------------------------------------
# 통계
# ---------------------------------------------------------------------------
def ranks(values: list[float]) -> list[float]:
    """평균 순위. 동점이면 순위를 나눠 가진다."""
    order = sorted(range(len(values)), key=lambda i: values[i])
    result = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        for k in range(i, j + 1):
            result[order[k]] = (i + j) / 2 + 1
        i = j + 1
    return result


def wilcoxon_exact(diffs: list[float]) -> dict:
    """양측 Wilcoxon 부호순위 정확 검정. 0인 차이는 빼고 모든 부호 조합(2^m)을 센다."""
    d = [x for x in diffs if x != 0]
    m = len(d)
    if m == 0:
        return {"n_nonzero": 0, "w_plus": None, "p": None}
    r = ranks([abs(x) for x in d])
    w_plus = sum(rank for rank, x in zip(r, d) if x > 0)
    center = sum(r) / 2
    observed = abs(w_plus - center)
    if m > 22:  # 짝이 22개를 넘으면 조합(2^m)이 너무 많다. 사전 등록 규모(최대 18쌍)에서는 생기지 않는다
        raise ValueError("짝이 너무 많아 정확 검정을 하지 않음")
    extreme = 0
    for signs in itertools.product((0, 1), repeat=m):
        w = sum(rank for rank, s in zip(r, signs) if s)
        if abs(w - center) >= observed - 1e-9:
            extreme += 1
    return {"n_nonzero": m, "w_plus": w_plus, "p": extreme / 2 ** m}


def bootstrap_ci(diffs: list[float], seed: int) -> dict:
    """셀을 다시 뽑아(부트스트랩) 평균·중앙값 차이의 95% 백분위 신뢰구간을 구한다."""
    rng = random.Random(seed)
    means, medians = [], []
    for _ in range(BOOTSTRAP):
        sample = [rng.choice(diffs) for _ in diffs]
        means.append(statistics.fmean(sample))
        medians.append(statistics.median(sample))
    means.sort(), medians.sort()
    lo, hi = int(0.025 * BOOTSTRAP), int(0.975 * BOOTSTRAP) - 1
    return {"mean_ci": (means[lo], means[hi]), "median_ci": (medians[lo], medians[hi])}


def holm(pvalues: dict[str, float | None]) -> dict[str, float | None]:
    """Holm 보정한 p값. 보정한 값이 단조 증가하도록 보장한다."""
    valid = sorted((p, k) for k, p in pvalues.items() if p is not None)
    adjusted, running = {}, 0.0
    for i, (p, k) in enumerate(valid):
        running = max(running, min(1.0, (len(valid) - i) * p))
        adjusted[k] = running
    return {k: adjusted.get(k) for k in pvalues}


# ---------------------------------------------------------------------------
# 데이터
# ---------------------------------------------------------------------------
load_records = runlog.load_records  # 형식은 harness/records.py에 있다


def load_scores(path: str) -> dict[str, float]:
    if path == "-":
        return {}
    with open(path, encoding="utf-8-sig", newline="") as f:
        return {row["run_id"]: float(row["pass_rate"]) for row in csv.DictReader(f)}


def cell_table(records: list[dict], scores: dict[str, float]):
    """{지표: {(모델, 게임): {조건: 셀 평균}}}와 빠진 값 수."""
    values = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    missing = defaultdict(int)
    for r in records:
        score = scores.get(r["run_id"])
        for key, _, _, fn in METRICS:
            v = fn(r, score)
            if v is None:
                missing[key] += 1
                continue
            values[key][(r["model_requested"], r["game"])][r["condition"]].append(float(v))
    cells = {key: {cell: {c: statistics.fmean(v) for c, v in conds.items()} for cell, conds in by_cell.items()}
             for key, by_cell in values.items()}
    return cells, missing


def paired_diffs(cells: dict, a: str = "meno", b: str = "sfml") -> dict[tuple, float]:
    return {cell: conds[a] - conds[b] for cell, conds in sorted(cells.items()) if a in conds and b in conds}


# ---------------------------------------------------------------------------
# 보고서
# ---------------------------------------------------------------------------
def fmt(v, digits=3):
    return "-" if v is None else f"{v:.{digits}f}"


def compute(records: list[dict], scores: dict[str, float], seed: int) -> tuple[dict, dict]:
    """(셀 표, 지표별 결과). 보고서(analyze)와 그림(figures.py)이 같은 계산을 쓴다."""
    cells, missing = cell_table(records, scores)
    results = {}
    for key, label, kind, _ in METRICS:
        diffs = paired_diffs(cells.get(key, {}))
        values = list(diffs.values())
        row = {"key": key, "label": label, "kind": kind, "pairs": len(values), "missing_runs": missing[key],
               "meno": statistics.fmean([c["meno"] for c in cells.get(key, {}).values() if "meno" in c]) if values else None,
               "sfml": statistics.fmean([c["sfml"] for c in cells.get(key, {}).values() if "sfml" in c]) if values else None,
               "mean_diff": statistics.fmean(values) if values else None,
               "median_diff": statistics.median(values) if values else None,
               "diffs": diffs}
        if values:
            row |= wilcoxon_exact(values) | bootstrap_ci(values, seed)
        results[key] = row
    adjusted = holm({k: r.get("p") for k, r in results.items() if r["kind"] == "secondary"})
    for k, p in adjusted.items():
        results[k]["p_holm"] = p
    return cells, results


def analyze(runs_dir: Path, scores_path: str, out: Path) -> None:
    cfg = tomllib.loads((BASE / "experiment.toml").read_text(encoding="utf-8"))
    records = load_records(runs_dir)
    scores = load_scores(scores_path)
    cells, results = compute(records, scores, cfg["experiment"]["seed"])
    out.mkdir(parents=True, exist_ok=True)

    lines = [f"# 분석 결과: {runs_dir}", "",
             f"회차 {len(records)}개, 채점된 회차 {len(scores)}개. 차이 = meno − SFML (셀 = 모델 × 게임, 셀 안 반복 평균).",
             "검정: 양측 정확 Wilcoxon 부호순위(0인 차이 제외). 신뢰구간: 셀 부트스트랩 10,000회 95%.", ""]
    for kind, title in (("primary", "1차 지표"), ("secondary", "2차 지표 (Holm 보정)"), ("exploratory", "탐색적 지표 (보정 없음)")):
        lines += [f"## {title}", "",
                  "| 지표 | 짝 | meno | SFML | 평균 차이 [95% CI] | 중앙값 차이 [95% CI] | p | " +
                  ("Holm p |" if kind == "secondary" else ""),
                  "|---|---|---|---|---|---|---|" + ("---|" if kind == "secondary" else "")]
        for r in (r for r in results.values() if r["kind"] == kind):
            ci = r.get("mean_ci", (None, None))
            mci = r.get("median_ci", (None, None))
            lines.append(f"| {r['label']} | {r['pairs']} | {fmt(r['meno'])} | {fmt(r['sfml'])} | "
                         f"{fmt(r['mean_diff'])} [{fmt(ci[0])}, {fmt(ci[1])}] | "
                         f"{fmt(r['median_diff'])} [{fmt(mci[0])}, {fmt(mci[1])}] | {fmt(r.get('p'))} |" +
                         (f" {fmt(r.get('p_holm'))} |" if kind == "secondary" else ""))
        lines.append("")
    notes = [f"- {r['label']}: 값이 빠진 회차 {r['missing_runs']}개" for r in results.values() if r["missing_runs"]]
    if not scores:
        notes.insert(0, "- 채점 결과 없이 돌렸다. 통과율 지표는 비어 있다.")
    restarted = sum((r.get("attempt") or 1) > 1 for r in records)
    if restarted:
        notes.append(f"- 하니스가 다시 받은 회차: {restarted}개 (그대로 사용)")
    lines += ["## 참고", "", *notes, ""]

    # 탐색: 모델별·게임별 1차 지표 차이
    primary = results["pass_all"]["diffs"]
    if primary:
        lines += ["## 탐색: 1차 지표의 모델별·게임별 평균 차이", ""]
        for index, title in ((0, "모델"), (1, "게임")):
            groups = defaultdict(list)
            for cell, d in primary.items():
                groups[cell[index]].append(d)
            lines += [f"| {title} | 짝 | 평균 차이 |", "|---|---|---|"]
            lines += [f"| {g} | {len(v)} | {fmt(statistics.fmean(v))} |" for g, v in sorted(groups.items())]
            lines.append("")

    # 탐색: 빌드 오류 유형
    error_rows = errors.classify_runs(runs_dir, records)
    if error_rows:
        counts = defaultdict(lambda: defaultdict(int))
        for row in error_rows:
            counts[row["condition"]][row["category"]] += 1
        categories = sorted({row["category"] for row in error_rows})
        lines += ["## 탐색: 실패한 빌드의 첫 오류 유형 (왕복 단위)", "",
                  "| 조건 | " + " | ".join(categories) + " |", "|---|" + "---|" * len(categories)]
        for condition in sorted(counts):
            lines.append(f"| {condition} | " + " | ".join(str(counts[condition][c]) for c in categories) + " |")
        lines.append("")
        with (out / "errors.csv").open("w", encoding="utf-8-sig", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(error_rows[0]))
            writer.writeheader()
            writer.writerows(error_rows)

    (out / "report.md").write_text("\n".join(lines), encoding="utf-8")
    with (out / "cells.csv").open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["metric", "model", "game", "meno", "sfml", "diff"])
        for key, by_cell in cells.items():
            for (model, game), conds in sorted(by_cell.items()):
                writer.writerow([key, model, game, conds.get("meno"), conds.get("sfml"),
                                 conds["meno"] - conds["sfml"] if {"meno", "sfml"} <= set(conds) else None])
    print(f"→ {out}/report.md")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("runs_dir", type=Path)
    parser.add_argument("scores", help="grading/results/run_scores.csv, 채점 전이면 -")
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    analyze(args.runs_dir, args.scores, args.out)


if __name__ == "__main__":
    main()
