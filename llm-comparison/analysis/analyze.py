"""사전 등록한 분석(PLAN 3절)을 그대로 계산한다. 결과를 보기 전에 만든 스크립트이며, 바꾸면 그 사실을 보고서에 적는다.

사용 (llm-comparison/ 에서):
    python3 analysis/analyze.py runs/main grading/results/run_scores.csv analysis/results
    python3 analysis/analyze.py runs/pilot-<해시> - analysis/pilot     # 채점 전: 하니스 지표만 계산 (통과율은 비움)

figures.py를 먼저 돌리면 그 그림이 report.md 본문에 들어간다.
출력: report.md (요약, 1차·2차·탐색 지표, 1차 지표 민감도, 채점하지 못한 셀을 뺀 결과), cells.csv (셀별 조건 평균과 차이), errors.csv (빌드 오류 유형)
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
# 지표 정의 (PLAN 3절). value(record, score) → 회차 값 또는 None(빠짐)
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
    # 결과를 본 뒤 추가했다(PLAN 3절). 출력 토큰은 추론 토큰을 포함하므로, 빼면 답으로 쓴 양(주로 코드)이 남는다.
    ("answer_tokens", "답변 토큰 (출력 − 추론)", "exploratory",
     lambda r, s: r["completion_tokens"] - r["reasoning_tokens"]
     if r.get("completion_tokens") is not None and r.get("reasoning_tokens") is not None else None),
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
    if m > 22:  # 짝이 22개를 넘으면 조합(2^m)이 너무 많다. 사전 등록 규모(최대 18쌍)에서는 이런 일이 없다
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


def stratified_bootstrap_ci(diffs: dict[tuple, float], seed: int) -> dict:
    """모델 안에서만 셀을 다시 뽑는다. 18쌍이 모델 3개에 묶여 있어 일반 부트스트랩은 구간을 좁게 잡을 수 있기 때문이다."""
    rng = random.Random(seed)
    by_model: dict[str, list[float]] = defaultdict(list)
    for (model, _), d in sorted(diffs.items()):
        by_model[model].append(d)
    means = []
    for _ in range(BOOTSTRAP):
        sample = [rng.choice(v) for v in by_model.values() for _ in v]
        means.append(statistics.fmean(sample))
    means.sort()
    lo, hi = int(0.025 * BOOTSTRAP), int(0.975 * BOOTSTRAP) - 1
    return {"mean_ci": (means[lo], means[hi])}


def sign(x: float) -> int:
    return (x > 0) - (x < 0)


def sensitivity(diffs: dict[tuple, float], seed: int) -> dict:
    """1차 지표의 민감도 분석 (PLAN 3절).

    - 모델별: 짝 6개의 평균 차이, 부호 개수, 정확 Wilcoxon p (보정 없음, 참고)
    - 모델 하나 빼기: 남은 두 모델의 짝으로 다시 계산
    - 모델 층화 부트스트랩 95% CI
    강건 = 전체 평균 차이가 0이 아니고, 모델별 평균 차이와 모델 하나를 뺀 평균 차이가 모두 전체와 같은 부호.
    """
    models = sorted({m for m, _ in diffs})
    overall = statistics.fmean(diffs.values())
    per_model, leave_out = {}, {}
    for m in models:
        own = [d for (mm, _), d in diffs.items() if mm == m]
        rest = [d for (mm, _), d in diffs.items() if mm != m]
        per_model[m] = {"pairs": len(own), "mean": statistics.fmean(own),
                        "pos": sum(d > 0 for d in own), "neg": sum(d < 0 for d in own), "zero": sum(d == 0 for d in own),
                        "p": wilcoxon_exact(own)["p"]}
        if rest:
            leave_out[m] = {"pairs": len(rest), "mean": statistics.fmean(rest), "median": statistics.median(rest),
                            "p": wilcoxon_exact(rest)["p"]}
    robust = (sign(overall) != 0 and len(models) > 1
              and all(sign(v["mean"]) == sign(overall) for v in per_model.values())
              and all(sign(v["mean"]) == sign(overall) for v in leave_out.values()))
    return {"overall": overall, "per_model": per_model, "leave_out": leave_out,
            "stratified": stratified_bootstrap_ci(diffs, seed), "robust": robust}


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


# 채점 결과를 쓰는 지표. 채점하지 못한 회차가 있으면 이 지표들만 영향을 받는다.
SCORED = ("pass_all", "pass_built", "complete")


def ungraded_cells(records: list[dict], scores: dict[str, float]) -> set[tuple]:
    """빌드됐는데 채점 결과가 없는 회차가 하나라도 있는 셀 (PLAN 3절 "빠진 값")."""
    return {(r["model_requested"], r["game"]) for r in records if r["build_success"] and r["run_id"] not in scores}


def without_cells(cells: dict, results: dict, dropped: set[tuple], seed: int) -> dict:
    """dropped 셀을 통째로 뺀 짝으로 채점 지표를 다시 계산한다. 본 결과(남은 회차로 낸 셀 평균)와 함께 보고한다."""
    out = {}
    for key in SCORED:
        diffs = {cell: d for cell, d in paired_diffs(cells.get(key, {})).items() if cell not in dropped}
        values = list(diffs.values())
        row = {"label": results[key]["label"], "pairs": len(values),
               "mean_diff": statistics.fmean(values) if values else None,
               "median_diff": statistics.median(values) if values else None}
        if values:
            row |= wilcoxon_exact(values) | bootstrap_ci(values, seed)
        out[key] = row
    return out


# ---------------------------------------------------------------------------
# 보고서
# ---------------------------------------------------------------------------
def fmt(v, digits=3):
    return "-" if v is None else f"{v:.{digits}f}"


# 지표마다 읽기 좋은 단위로 쓴다. 계산은 바꾸지 않고 보여 주는 형식만 정한다.
RATES = {"pass_all", "first_round", "built", "pass_built", "complete", "early_exit", "frame_p95_ok"}
DIGITS = {"rounds_to_build": 2, "llm_latency_s": 1, "code_lines": 0, "completion_tokens": 0, "reasoning_tokens": 0, "answer_tokens": 0}
ERROR_LABELS = {"sfml2_api": "SFML 2 API를 SFML 3에 씀", "sfml_in_meno": "meno 조건에서 SFML 헤더", "meno_api": "meno API 오용",
                "link": "링크 오류", "cpp": "그 밖의 C++ 오류"}


def fmt_value(key: str, v) -> str:
    """조건별 값. 비율은 %, 토큰·줄 수는 천 단위 쉼표."""
    if v is None:
        return "-"
    if key in RATES:
        return f"{v * 100:.1f}%"
    return f"{v:,.{DIGITS.get(key, 2)}f}"


def fmt_diff(key: str, v) -> str:
    """차이. 비율의 차이는 %p(퍼센트포인트)로, 부호를 붙인다."""
    if v is None:
        return "-"
    if key in RATES:
        return f"{v * 100:+.1f}%p"
    return f"{v:+,.{DIGITS.get(key, 2)}f}"


def fmt_ci(key: str, ci) -> str:
    lo, hi = ci if ci else (None, None)
    return "-" if lo is None else f"[{fmt_diff(key, lo)}, {fmt_diff(key, hi)}]"


def fmt_p(p) -> str:
    return "-" if p is None else ("<0.001" if p < 0.001 else f"{p:.3f}")


def display_path(path: Path) -> str:
    """보고서에 컴퓨터마다 다른 절대 경로가 찍히지 않도록, 가능하면 llm-comparison/ 기준 상대 경로로 쓴다."""
    try:
        return path.resolve().relative_to(BASE.resolve()).as_posix()
    except ValueError:
        return path.name


def figure_link(out: Path, name: str, alt: str) -> list[str]:
    """figures.py가 그린 그림이 있으면 본문에 넣는다 (그림은 figures.py를 먼저 돌려야 생긴다)."""
    return [f"![{alt}](figures/{name})", ""] if (out / "figures" / name).exists() else []


def compute(records: list[dict], scores: dict[str, float], seed: int) -> tuple[dict, dict]:
    """(셀 표, 지표별 결과)를 돌려준다. 보고서(analyze)와 그림(figures.py)이 같은 계산을 쓴다."""
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

    seed = cfg["experiment"]["seed"]
    primary = results["pass_all"]["diffs"]
    s = sensitivity(primary, seed) if primary else None
    dropped = ungraded_cells(records, scores) if scores else set()
    verdict = ("**강건함.** 모든 모델과 모델 하나를 뺀 모든 경우에서 차이의 방향이 전체와 같다." if s and s["robust"] else
               "**강건하지 않음.** 어떤 모델 또는 모델 하나를 뺀 경우에서 차이의 방향이 전체와 다르거나 0이다. "
               "1차 결론을 특정 모델의 결과로 한정해 보고한다.")

    lines = ["# 분석 결과", "",
             f"회차 폴더 `{display_path(runs_dir)}`: 회차 {len(records)}개, 채점된 회차 {len(scores)}개.",
             "차이 = meno − SFML (셀 = 모델 × 게임, 셀 안 반복 평균). 비율의 차이는 %p(퍼센트포인트)다.",
             "검정: 양측 정확 Wilcoxon 부호순위(0인 차이 제외). 신뢰구간: 셀 부트스트랩 10,000회 95%.", ""]

    # 요약: 사전 등록한 판정을 숫자 그대로 옮긴다. 해석은 덧붙이지 않는다.
    lines += ["## 요약", ""]
    p_row = results["pass_all"]
    if not scores:
        lines.append("- 채점 전이다. 1차 지표(요구사항 통과율)는 채점이 끝난 뒤에 나온다.")
    elif p_row["mean_diff"] is None:
        lines.append("- 1차 지표를 계산할 짝이 없다.")
    else:
        significant = p_row.get("p") is not None and p_row["p"] < 0.05
        lines += [f"- **1차 지표(요구사항 통과율, 전체 회차):** meno − SFML = **{fmt_diff('pass_all', p_row['mean_diff'])}** "
                  f"{fmt_ci('pass_all', p_row.get('mean_ci'))}, 정확 Wilcoxon p = {fmt_p(p_row.get('p'))} "
                  f"({p_row['pairs']}쌍). α = 0.05에서 {'유의하다' if significant else '유의하지 않다'}.",
                  f"- **민감도 판정:** {verdict}"]
        if dropped:
            lines.append(f"- 채점하지 못한 회차가 있는 셀 {len(dropped)}개: 그 셀을 뺀 결과는 아래 \"빠진 값\" 절에 있다.")
    lines.append("")

    figures_for = {"primary": figure_link(out, "1-primary-pairs.png", "셀별 요구사항 통과율"),
                   "secondary": figure_link(out, "2-effects.png", "짝 차이와 95% 신뢰구간")}
    for kind, title in (("primary", "1차 지표"), ("secondary", "2차 지표 (Holm 보정)"), ("exploratory", "탐색적 지표 (보정 없음)")):
        lines += [f"## {title}", "",
                  "| 지표 | 짝 | meno | SFML | 평균 차이 [95% CI] | 중앙값 차이 [95% CI] | p | " +
                  ("Holm p |" if kind == "secondary" else ""),
                  "|---|---|---|---|---|---|---|" + ("---|" if kind == "secondary" else "")]
        for r in (r for r in results.values() if r["kind"] == kind):
            k = r["key"]
            lines.append(f"| {r['label']} | {r['pairs']} | {fmt_value(k, r['meno'])} | {fmt_value(k, r['sfml'])} | "
                         f"{fmt_diff(k, r['mean_diff'])} {fmt_ci(k, r.get('mean_ci'))} | "
                         f"{fmt_diff(k, r['median_diff'])} {fmt_ci(k, r.get('median_ci'))} | {fmt_p(r.get('p'))} |" +
                         (f" {fmt_p(r.get('p_holm'))} |" if kind == "secondary" else ""))
        lines += ["", *figures_for.get(kind, [])]
    notes = [f"- {r['label']}: 값이 빠진 회차 {r['missing_runs']}개" for r in results.values() if r["missing_runs"]]
    if not scores:
        notes.insert(0, "- 채점 결과 없이 돌렸다. 통과율 지표는 비어 있다.")
    restarted = sum((r.get("attempt") or 1) > 1 for r in records)
    if restarted:
        notes.append(f"- 하니스가 다시 받은 회차: {restarted}개 (그대로 사용)")
    lines += ["## 참고", "", *notes, ""]

    # 사전 등록: 채점하지 못한 회차가 있는 셀은 남은 회차로 평균을 내고(위 표), 그 셀을 통째로 뺀 결과도 보고한다
    if dropped:
        rows = without_cells(cells, results, dropped, seed)
        lines += ["## 빠진 값: 채점하지 못한 회차가 있는 셀을 뺀 결과 (사전 등록)", "",
                  "뺀 셀: " + ", ".join(f"{m} × {g}" for m, g in sorted(dropped)), "",
                  "| 지표 | 짝 | 평균 차이 [95% CI] | 중앙값 차이 [95% CI] | p |", "|---|---|---|---|---|"]
        for k, r in rows.items():
            lines.append(f"| {r['label']} | {r['pairs']} | {fmt_diff(k, r['mean_diff'])} {fmt_ci(k, r.get('mean_ci'))} | "
                         f"{fmt_diff(k, r['median_diff'])} {fmt_ci(k, r.get('median_ci'))} | {fmt_p(r.get('p'))} |")
        lines.append("")

    # 사전 등록 민감도 분석: 18쌍이 모델 3개에 묶여 있으므로 한 모델이 결과를 끌고 가는지 본다
    if s:
        lines += ["## 1차 지표 민감도 분석 (사전 등록)", "",
                  f"모델 층화 부트스트랩 평균 차이 95% CI: {fmt_ci('pass_all', s['stratified']['mean_ci'])}", "",
                  "| 모델 | 짝 | 평균 차이 | meno 우세 / SFML 우세 / 같음 | p (참고) | 이 모델을 뺀 평균 차이 | 뺀 뒤 p |",
                  "|---|---|---|---|---|---|---|"]
        for m, v in s["per_model"].items():
            lo = s["leave_out"].get(m, {})
            lines.append(f"| {m} | {v['pairs']} | {fmt_diff('pass_all', v['mean'])} | {v['pos']} / {v['neg']} / {v['zero']} | "
                         f"{fmt_p(v['p'])} | {fmt_diff('pass_all', lo.get('mean'))} | {fmt_p(lo.get('p'))} |")
        lines += ["", "판정: " + verdict, ""]
        groups = defaultdict(list)
        for cell, d in primary.items():
            groups[cell[1]].append(d)
        lines += ["## 탐색: 1차 지표의 게임별 평균 차이", "", "| 게임 | 짝 | 평균 차이 |", "|---|---|---|"]
        lines += [f"| {g} | {len(v)} | {fmt_diff('pass_all', statistics.fmean(v))} |" for g, v in sorted(groups.items())]
        lines.append("")

    # 탐색: 빌드 오류 유형
    error_rows = errors.classify_runs(runs_dir, records)
    if error_rows:
        counts = defaultdict(lambda: defaultdict(int))
        for row in error_rows:
            counts[row["condition"]][row["category"]] += 1
        categories = sorted({row["category"] for row in error_rows})
        lines += ["## 탐색: 실패한 빌드의 첫 오류 유형 (왕복 단위)", "",
                  "| 조건 | " + " | ".join(f"{ERROR_LABELS.get(c, c)} (`{c}`)" for c in categories) + " |",
                  "|---|" + "---|" * len(categories)]
        for condition in sorted(counts):
            lines.append(f"| {condition} | " + " | ".join(str(counts[condition][c]) for c in categories) + " |")
        lines += ["", *figure_link(out, "3-errors.png", "실패한 빌드의 첫 오류 유형")]
        with (out / "errors.csv").open("w", encoding="utf-8-sig", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(error_rows[0]))
            writer.writeheader()
            writer.writerows(error_rows)

    (out / "report.md").write_text("\n".join(lines), encoding="utf-8", newline="\n")
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
