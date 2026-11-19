"""사전 등록한 분석(PLAN 3절)의 결과를 그림으로 그린다. 계산은 analyze.compute()를 그대로 쓴다.

사용 (llm-comparison/ 에서, matplotlib 필요: pip install -r analysis/requirements.txt):
    python3 analysis/figures.py runs/main grading/results/run_scores.csv analysis/results
    python3 analysis/figures.py runs/pilot-<해시> - analysis/pilot      # 채점 전: 통과율 그림은 만들지 않음

출력 (<out>/figures/):
    1-primary-pairs.png   1차 지표를 셀(모델 × 게임)마다 두 조건의 점으로 찍는다. 무엇과 무엇을 짝지어 비교하는지 보여 준다
    2-effects.png         짝 차이(meno − SFML)의 평균과 95% 신뢰구간. 1차 지표만 색을 칠하고 2차 지표는 회색으로 둔다
    3-errors.png          실패한 빌드의 첫 오류 유형을 조건별로 쌓은 막대 (탐색 지표)
숫자 표는 analyze.py의 report.md와 cells.csv에 있다. 그림 글자는 영어다 (한글 글꼴 없이 어디서나 같게 그리려고).

색은 dataviz 기본 팔레트(밝은 모드)이고 benchmark/style.py와 같은 값이다. 조건 두 색(파랑 meno, 주황 SFML)은
겹쳐 그려도 색각 이상이 있는 사람이 구분할 수 있다고 검증된 앞 두 칸이다. 오류 유형은 이웃한 칸만 구분되면 되는 누적 막대라 다섯 칸을 쓴다.
"""
from __future__ import annotations

import argparse
import sys
import tomllib
from collections import Counter
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze  # noqa: E402
import errors  # noqa: E402

SURFACE, INK, INK_2, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#e4e3df"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4"]
CONDITION = {"meno": SERIES[0], "sfml": SERIES[1]}  # 조건마다 고정 (순위나 개수로 바꾸지 않음)
CONDITION_NAME = {"meno": "meno", "sfml": "SFML 3.1"}
LIGHT = {"#1baf7a", "#eda100", "#e87ba4"}            # 이 칸 위의 글자는 검정
SECONDARY_GRAY = "#8a8883"
# 오류 유형은 순서를 고정한다 (한 조건에 없는 유형이 생겨도 색이 밀리지 않게).
ERROR_TYPES = ["sfml2_api", "sfml_in_meno", "meno_api", "link", "cpp"]
ERROR_NAME = {"sfml2_api": "SFML 2 API in SFML 3", "sfml_in_meno": "SFML header in meno",
              "meno_api": "meno API misuse", "link": "link error", "cpp": "other C++ error"}
RATES = ["pass_all", "first_round", "built", "pass_built", "complete"]  # 0~1 비율 지표 (한 축에 같이 그림)
RATE_NAME = {"pass_all": "Requirement pass rate (all runs)", "first_round": "Built in round 1",
             "built": "Built within 5 rounds", "pass_built": "Pass rate (built runs only)",
             "complete": "Game complete (all items pass)"}


def apply(plt) -> None:
    plt.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "axes.edgecolor": GRID, "axes.labelcolor": INK_2, "xtick.color": INK_2, "ytick.color": INK_2,
        "text.color": INK, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
        "axes.spines.top": False, "axes.spines.right": False, "axes.spines.left": False, "font.size": 10,
        "axes.titlesize": 11, "axes.titleweight": "bold", "axes.titlelocation": "left",
        "lines.linewidth": 2, "legend.frameon": False,
    })


def short_model(model_id: str) -> str:
    return model_id.split("/", 1)[-1]


def p_text(row: dict, holm: bool) -> str:
    p = row.get("p_holm") if holm else row.get("p")
    if p is None:
        return "p = n/a"
    return ("Holm " if holm else "") + ("p < 0.001" if p < 0.001 else f"p = {p:.3f}")


# ---------------------------------------------------------------------------
# 1. 1차 지표: 셀마다 두 조건
# ---------------------------------------------------------------------------
def primary_pairs(plt, cells: dict, row: dict, out: Path) -> None:
    by_cell = cells.get("pass_all", {})
    models = sorted({m for m, _ in by_cell})
    games = sorted({g for _, g in by_cell})
    fig, axes = plt.subplots(1, max(1, len(models)), figsize=(3.6 * max(1, len(models)) + 1.2, 0.42 * len(games) + 1.9),
                             sharey=True, squeeze=False)
    for ax, model in zip(axes[0], models or [None]):
        for y, game in enumerate(games):
            conds = by_cell.get((model, game), {})
            if {"meno", "sfml"} <= set(conds):
                ax.plot([conds["meno"], conds["sfml"]], [y, y], color=GRID, linewidth=2, zorder=1)  # 짝을 잇는 선
            for c in ("meno", "sfml"):  # 범례 순서도 이 순서 (meno 먼저)
                if c in conds:
                    ax.scatter(conds[c], y, s=64, color=CONDITION[c], edgecolors=SURFACE, linewidths=2, zorder=3,
                               label=CONDITION_NAME[c] if y == 0 else None)
        ax.set(xlim=(-0.03, 1.03), title=short_model(model) if model else "no graded runs")
        ax.set_yticks(range(len(games)), games)
        ax.xaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:.0%}"))
        ax.grid(axis="y", visible=False)
        ax.invert_yaxis()
    handles, labels = axes[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=2, fontsize=9)
    fig.suptitle("Requirement pass rate per model and game (mean of 3 runs; build failure counts as 0)",
                 x=0.01, ha="left", fontsize=11, fontweight="bold")
    note = "No graded runs yet." if row.get("mean_diff") is None else (
        f"Paired difference meno − SFML over {row['pairs']} cells: mean {row['mean_diff']:+.1%} "
        f"[95% CI {row['mean_ci'][0]:+.1%}, {row['mean_ci'][1]:+.1%}], exact Wilcoxon {p_text(row, False)}.")
    fig.text(0.01, 0.01, note, color=INK_2, fontsize=8.5)
    fig.tight_layout(rect=(0, 0.05, 1, 0.93))
    fig.savefig(out / "1-primary-pairs.png", dpi=150)
    plt.close(fig)


# ---------------------------------------------------------------------------
# 2. 효과 크기: 짝 차이와 신뢰구간
# ---------------------------------------------------------------------------
def forest(ax, rows: list[dict], scale: float, fmt: str, xlabel: str, title: str, unit: str = "",
           row_labels: bool = True) -> None:
    ax.axvline(0, color=INK_2, linewidth=1, zorder=1)
    for y, row in enumerate(rows):
        primary = row["kind"] == "primary"
        color = SERIES[0] if primary else SECONDARY_GRAY
        if row.get("mean_diff") is None:
            ax.text(0, y, "no data", ha="center", va="center", color=INK_2, fontsize=8)
            continue
        lo, hi = (v * scale for v in row["mean_ci"])
        ax.plot([lo, hi], [y, y], color=color, linewidth=2, solid_capstyle="round", zorder=2)
        ax.scatter(row["mean_diff"] * scale, y, s=64, color=color, edgecolors=SURFACE, linewidths=2, zorder=3)
        ax.annotate(f"{row['mean_diff'] * scale:{fmt}}{unit}  ({p_text(row, not primary)})", (hi, y),
                    xytext=(8, 0), textcoords="offset points", va="center", color=INK_2, fontsize=8)
    # 한 줄짜리 칸은 칸 제목이 이름을 대신한다 (옆 칸과 겹치지 않게 y 눈금 글자를 뺀다)
    ax.set_yticks(range(len(rows)), [r["name"] for r in rows] if row_labels else [""] * len(rows))
    if not row_labels:
        ax.tick_params(axis="y", length=0)
    for tick, row in zip(ax.get_yticklabels(), rows):
        if row["kind"] == "primary":
            tick.set_fontweight("bold"); tick.set_color(INK)
    ax.set(xlabel=xlabel, title=title)
    ax.grid(axis="y", visible=False)
    ax.invert_yaxis()
    left, right = ax.get_xlim()
    ax.set_xlim(left, right + (right - left) * 0.7)  # 오른쪽 주석 자리


SCORED = {"pass_all", "pass_built", "complete"}  # 채점 결과가 있어야 뜻이 있는 지표


def effects(plt, results: dict, out: Path, graded: bool) -> None:
    # 채점 전에는 통과율 지표를 그리지 않는다. 빌드 실패(0)만 남은 반쪽 값이라 잘못 읽힌다.
    rate_rows = [results[k] | {"name": RATE_NAME[k]} for k in RATES if k in results and (graded or k not in SCORED)]
    fig = plt.figure(figsize=(10, 3.4 + 0.6 * len(rate_rows)))  # 줄 수에 맞춘 높이 (채점 전 2줄, 채점 뒤 5줄)
    grid = fig.add_gridspec(2, 2, height_ratios=[len(rate_rows), 1.4], hspace=0.8, wspace=0.12)
    forest(fig.add_subplot(grid[0, :]), rate_rows, 100, "+.1f",
           "meno − SFML, percentage points (right = meno higher)", "Rates", unit=" pp")
    forest(fig.add_subplot(grid[1, 0]), [results["rounds_to_build"] | {"name": "Rounds to build"}], 1, "+.2f",
           "meno − SFML, rounds\n(left = meno needs fewer)", "Rounds to build", row_labels=False)
    forest(fig.add_subplot(grid[1, 1]), [results["completion_tokens"] | {"name": "Output tokens"}], 1 / 1000, "+.1f",
           "meno − SFML, thousand tokens\n(left = meno writes fewer)", "Output tokens (incl. reasoning)", unit="k",
           row_labels=False)
    fig.suptitle("Paired effect, meno − SFML: mean over model × game cells with 95% bootstrap CI",
                 x=0.01, ha="left", fontsize=11, fontweight="bold")
    fig.text(0.01, 0.01, ("Blue: primary metric (exact Wilcoxon). Gray: secondary metrics (Holm-adjusted p). "
                          if graded else "Not graded yet: pass-rate metrics are omitted. Holm-adjusted p for secondary metrics. ")
             + "Each axis has its own scale.", color=INK_2, fontsize=8.5)
    fig.subplots_adjust(left=0.27, right=0.97, top=0.9, bottom=0.16)
    fig.savefig(out / "2-effects.png", dpi=150)
    plt.close(fig)


# ---------------------------------------------------------------------------
# 3. 빌드 오류 유형
# ---------------------------------------------------------------------------
def error_types(plt, rows: list[dict], out: Path) -> None:
    counts = {c: Counter(r["category"] for r in rows if r["condition"] == c) for c in ("meno", "sfml")}
    fig, ax = plt.subplots(figsize=(10, 2.5))
    ax.set_axisbelow(True)  # 눈금선이 막대 위로 지나가지 않게
    for y, condition in enumerate(("meno", "sfml")):
        left = 0
        for color, kind in zip(SERIES, ERROR_TYPES):
            n = counts[condition][kind]
            if not n:
                continue
            # 조각 사이에 2px 표면색 틈을 둔다. 막대 두께는 줄 높이의 일부만 쓴다 (≤ 24px 상당)
            ax.barh(y, n, left=left, height=0.42, color=color, edgecolor=SURFACE, linewidth=2)
            ax.text(left + n / 2, y, str(n), ha="center", va="center", fontsize=8.5,
                    color=INK if color in LIGHT else SURFACE)
            left += n
        ax.text(left, y, f"  {left} failed builds", va="center", color=INK_2, fontsize=8.5)
    handles = [plt.Rectangle((0, 0), 1, 1, color=c) for c in SERIES]
    ax.legend(handles, [ERROR_NAME[k] for k in ERROR_TYPES], loc="upper center", bbox_to_anchor=(0.5, -0.38),
              ncol=5, fontsize=8.5)
    ax.set_yticks([0, 1], [CONDITION_NAME[c] for c in ("meno", "sfml")])
    ax.set(xlabel="failed build rounds (first error of each log)",
           title="Build errors by type (exploratory)")
    ax.grid(axis="y", visible=False)
    ax.invert_yaxis()
    right = max(sum(c.values()) for c in counts.values()) or 1
    ax.set_xlim(0, right * 1.18)
    fig.subplots_adjust(left=0.1, right=0.98, top=0.86, bottom=0.36)
    fig.savefig(out / "3-errors.png", dpi=150)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("runs_dir", type=Path)
    parser.add_argument("scores", help="grading/results/run_scores.csv, 채점 전이면 -")
    parser.add_argument("out", type=Path)
    args = parser.parse_args()
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        raise SystemExit("matplotlib이 없음: pip install -r analysis/requirements.txt")
    apply(plt)
    cfg = tomllib.loads((BASE / "experiment.toml").read_text(encoding="utf-8"))
    records = analyze.load_records(args.runs_dir)
    scores = analyze.load_scores(args.scores)
    cells, results = analyze.compute(records, scores, cfg["experiment"]["seed"])
    out = args.out / "figures"
    out.mkdir(parents=True, exist_ok=True)
    made = []
    if scores:
        primary_pairs(plt, cells, results["pass_all"], out)
        made.append("1-primary-pairs.png")
    else:
        print("채점 결과가 없어 1-primary-pairs.png는 만들지 않음 (2-effects.png에서도 통과율 지표는 뺌)")
    effects(plt, results, out, graded=bool(scores))
    error_types(plt, errors.classify_runs(args.runs_dir, records), out)
    print(f"→ {out}/ ({', '.join(made + ['2-effects.png', '3-errors.png'])})")


if __name__ == "__main__":
    main()
