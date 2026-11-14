"""배칭 판단용 그림 4장 (SeoulTech-Fiat-Lux/Meno#30).

사용: python3 figures.py <결과 폴더>      (summarize.py를 먼저 돌려 summary.csv가 있어야 한다)

결과 폴더의 figures/에 쓴다.
  1-budget.png        N에 따른 프레임 시간(평균, p99)과 60·30 fps 예산선. 예산을 넘는 N을 표시
  2-stages.png        meno-object 프레임 시간을 SFML 기본 / Renderer 단계 / 객체 단계로 나눈 비율
  3-per-sprite.png    스프라이트 하나당 평균 비용(μs). 평평하면 개수 비례 비용이 주도
  4-distribution.png  큰 N 두 개에서 프레임 시간 누적 분포. p99가 튀는 프레임 때문인지 전반적 느림인지
  figures.md          그림에서 읽을 값(예산을 넘는 N, 단계 비율)을 표로. 그림을 못 보는 사람용

그림 글자는 영어다. WSL에 한글 글꼴이 없으면 글자가 깨지기 때문이다.
"""
from __future__ import annotations

import csv
import json
import math
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import FuncFormatter, LogLocator, NullFormatter  # noqa: E402

# dataviz 기본 팔레트(밝은 모드). 대상 3개까지는 모든 쌍이 색각 이상에서도 구분된다고 검증된 앞 세 칸만 쓴다.
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
GRID = "#e4e3df"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]
BUDGETS = [(1000 / 60, "60 fps"), (1000 / 30, "30 fps")]

plt.rcParams.update({
    "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
    "axes.edgecolor": GRID, "axes.labelcolor": INK_2, "xtick.color": INK_2, "ytick.color": INK_2,
    "text.color": INK, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
    "axes.spines.top": False, "axes.spines.right": False, "font.size": 10,
    "axes.titlesize": 11, "axes.titleweight": "bold", "axes.titlelocation": "left",
    "lines.linewidth": 2, "lines.markersize": 5, "legend.frameon": False,
})


def load(out: Path) -> tuple[list[str], list[int], dict]:
    rows = list(csv.DictReader((out / "summary.csv").open(newline="")))
    by = {(r["target"], int(r["count"])): {k: float(v) for k, v in r.items() if k.endswith(("_ms", "_min", "_max"))}
          for r in rows}
    env = json.loads((out / "env.json").read_text(encoding="utf-8")) if (out / "env.json").exists() else {}
    order = env.get("config", {}).get("sweep", {}).get("targets", [])
    present = {t for t, _ in by}
    targets = [t for t in order if t in present] + sorted(present - set(order))
    if len(targets) > len(SERIES):
        sys.exit(f"대상이 {len(targets)}개다. 색 구분이 검증된 것은 {len(SERIES)}개까지라 그림을 나눠야 한다")
    counts = sorted({n for _, n in by})
    return targets, counts, by


def plain_ticks(ax, axis: str = "both") -> None:
    """로그 축 눈금을 4×10^0 대신 4처럼 쓴다. 보조 눈금 글자는 지운다."""
    fmt = FuncFormatter(lambda v, _: f"{v:,.0f}" if v >= 1 else f"{v:g}")
    for a in ((ax.xaxis, ax.yaxis) if axis == "both" else ((ax.xaxis,) if axis == "x" else (ax.yaxis,))):
        if a.get_scale() == "log":
            # 범위가 좁으면 10의 거듭제곱만으로는 눈금 글자가 한두 개뿐이라 2·5 단위(더 좁으면 1~9)도 붙인다.
            lo, hi = sorted(a.get_view_interval())
            decades = math.log10(hi / lo)
            subs = (1.0,) if decades >= 2 else (1.0, 2.0, 5.0) if decades >= 0.8 else tuple(range(1, 10))
            a.set_major_locator(LogLocator(base=10, subs=subs))
        a.set_major_formatter(fmt)
        a.set_minor_formatter(NullFormatter())


def crossing(counts: list[int], values: list[float], limit: float) -> float | None:
    """값이 처음 limit을 넘는 N을 log-log 보간으로 추정. 처음부터 넘으면 counts[0], 끝까지 안 넘으면 None."""
    for i, (n, v) in enumerate(zip(counts, values)):
        if v >= limit:
            if i == 0:
                return n
            n0, v0 = counts[i - 1], values[i - 1]
            t = (math.log(limit) - math.log(v0)) / (math.log(v) - math.log(v0))
            return math.exp(math.log(n0) + t * (math.log(n) - math.log(n0)))
    return None


def end_labels(ax, xs_by_target: dict, ys_by_target: dict, colors: dict, fmt=lambda t: t) -> None:
    """선 끝에 대상 이름을 직접 붙인다. 겹치면 위아래로 벌린다(로그 축 기준)."""
    ends = sorted(((ys_by_target[t][-1], t) for t in ys_by_target), key=lambda p: p[0])
    log = ax.get_yscale() == "log"
    placed = []
    for y, t in ends:
        pos = math.log10(y) if log else y
        span = (math.log10(ax.get_ylim()[1]) - math.log10(ax.get_ylim()[0])) if log else (ax.get_ylim()[1] - ax.get_ylim()[0])
        if placed and pos - placed[-1] < span * 0.05:
            pos = placed[-1] + span * 0.05
        placed.append(pos)
        ax.annotate(fmt(t), (xs_by_target[t][-1], 10 ** pos if log else pos), xytext=(6, 0),
                    textcoords="offset points", va="center", color=INK, fontsize=9)


def fig_budget(out: Path, targets, counts, by, colors, notes: list[str]) -> None:
    fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
    notes += ["## 1. 프레임 예산을 넘는 N", "", "| 대상 | 통계 | 60 fps (16.7ms) | 30 fps (33.3ms) |", "|---|---|---:|---:|"]
    for ax, stat in zip(axes, ("mean", "p99")):
        xs, ys = {}, {}
        for t in targets:
            pts = [(n, by[t, n][f"{stat}_ms"]) for n in counts if (t, n) in by]
            xs[t], ys[t] = [p[0] for p in pts], [p[1] for p in pts]
            ax.plot(xs[t], ys[t], marker="o", color=colors[t], label=t,
                    markeredgecolor=SURFACE, markeredgewidth=1.5)
            cells = []
            for limit, _ in BUDGETS:
                c = crossing(xs[t], ys[t], limit)
                cells.append(f"{xs[t][-1]:,}까지 넘지 않음" if c is None else f"약 {c:,.0f}")
                if c is not None:
                    ax.plot([c], [limit], marker="v", markersize=8, color=colors[t],
                            markeredgecolor=SURFACE, markeredgewidth=1.5, zorder=5)
            notes.append(f"| {t} | {stat} | {cells[0]} | {cells[1]} |")
        for limit, name in BUDGETS:
            ax.axhline(limit, color=INK_2, linewidth=1, linestyle=(0, (4, 3)), zorder=1)
            ax.annotate(name, (counts[0], limit), xytext=(0, 3), textcoords="offset points",
                        color=INK_2, fontsize=8)
        ax.set(xscale="log", yscale="log", xlabel="sprites (N)", title=f"Frame time, {stat}")
        lo = min(min(v) for v in ys.values())
        ax.set_ylim(lo / 1.5, max(max(max(v) for v in ys.values()), BUDGETS[-1][0]) * 1.6)
        ax.set_xlim(counts[0] / 1.3, counts[-1] * 2.6)
        end_labels(ax, xs, ys, colors)
        plain_ticks(ax)
    axes[0].set_ylabel("ms per frame (log)")
    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=len(targets), bbox_to_anchor=(1, 1))
    fig.text(0.01, 0.005, "▼ = N where the line crosses a budget (log-log interpolation). Median of reps.",
             color=INK_2, fontsize=8)
    fig.tight_layout(rect=(0, 0.03, 1, 0.93))
    fig.savefig(out / "figures" / "1-budget.png", dpi=150)
    plt.close(fig)
    notes.append("")


def fig_stages(out: Path, targets, counts, by, colors, notes: list[str]) -> None:
    if len(targets) < 2:
        return
    stages = [targets[0]] + [f"+{t}" for t in targets[1:]]
    labels = [f"{targets[0]} base"] + [f"{b} → {a}" for a, b in zip(targets[1:], targets[:-1])]
    shares, negative = defaultdict(list), []
    ns = [n for n in counts if all((t, n) in by for t in targets)]
    for n in ns:
        means = [by[t, n]["mean_ms"] for t in targets]
        parts = [means[0]] + [b - a for a, b in zip(means[:-1], means[1:])]
        if any(p < 0 for p in parts):
            negative.append(n)
        parts = [max(p, 0) for p in parts]
        total = sum(parts)
        for s, p in zip(stages, parts):
            shares[s].append(100 * p / total)
    fig, ax = plt.subplots(figsize=(9, 4.2))
    x = range(len(ns))
    bottom = [0.0] * len(ns)
    for s, lab, t in zip(stages, labels, targets):
        ax.bar(x, shares[s], bottom=bottom, width=0.62, color=colors[t], label=lab,
               edgecolor=SURFACE, linewidth=2)
        for i, (b, h) in enumerate(zip(bottom, shares[s])):
            if h >= 7:
                ax.text(i, b + h / 2, f"{h:.0f}%", ha="center", va="center", color=SURFACE if t != targets[-1] else INK,
                        fontsize=8, fontweight="bold")
        bottom = [b + h for b, h in zip(bottom, shares[s])]
    ax.set_xticks(list(x), [f"{n:,}" for n in ns])
    ax.set(xlabel="sprites (N)", ylabel=f"share of {targets[-1]} mean frame time (%)", ylim=(0, 100),
           title=f"Where {targets[-1]} spends its frame, by stage")
    ax.grid(axis="x", visible=False)
    ax.legend(loc="upper left", bbox_to_anchor=(1.01, 1))
    note = "Each stage = mean difference from the previous target."
    if negative:
        note += f" Negative stage clamped to 0 at N = {', '.join(map(str, negative))} (see figures.md)."
    fig.text(0.01, 0.005, note, color=INK_2, fontsize=8)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(out / "figures" / "2-stages.png", dpi=150)
    plt.close(fig)
    notes += ["## 2. 단계별 비율 (평균 기준, %)", "", "| N | " + " | ".join(labels) + " |",
              "|---:|" + "---:|" * len(labels)]
    notes += [f"| {n:,} | " + " | ".join(f"{shares[s][i]:.0f}" for s in stages) + " |" for i, n in enumerate(ns)]
    if negative:
        notes.append(f"\n음수 단계가 있어 0으로 잘랐다: N = {negative}. 앞 대상보다 빨랐다는 뜻이므로 summary.md에서 확인한다.")
    notes.append("")


def fig_per_sprite(out: Path, targets, counts, by, colors, notes: list[str]) -> None:
    fig, ax = plt.subplots(figsize=(9, 4.2))
    xs, ys = {}, {}
    for t in targets:
        pts = [(n, 1000 * by[t, n]["mean_ms"] / n) for n in counts if (t, n) in by]
        xs[t], ys[t] = [p[0] for p in pts], [p[1] for p in pts]
        ax.plot(xs[t], ys[t], marker="o", color=colors[t], label=t, markeredgecolor=SURFACE, markeredgewidth=1.5)
    ax.set(xscale="log", yscale="log", xlabel="sprites (N)", ylabel="μs per sprite (mean frame time / N, log)",
           title="Cost per sprite: flat = per-sprite work dominates, falling = fixed cost dominates")
    ax.set_xlim(counts[0] / 1.3, counts[-1] * 2.6)
    end_labels(ax, xs, ys, colors)
    plain_ticks(ax)
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(out / "figures" / "3-per-sprite.png", dpi=150)
    plt.close(fig)
    notes += ["## 3. 스프라이트 하나당 비용 (μs)", "", "| 대상 | " + " | ".join(f"N={n:,}" for n in counts) + " |",
              "|---|" + "---:|" * len(counts)]
    notes += [f"| {t} | " + " | ".join(f"{1000 * by[t, n]['mean_ms'] / n:.2f}" if (t, n) in by else "-" for n in counts)
              + " |" for t in targets]
    notes.append("")


def fig_distribution(out: Path, targets, counts, colors, notes: list[str]) -> None:
    runs = [json.loads(l) for l in (out / "runs.jsonl").read_text(encoding="utf-8").splitlines()]
    ok = {(r["target"], r["count"], r["rep"]) for r in runs if r["ok"]}
    picks = [n for n in (1000, 10000) if n in counts] or counts[-2:]
    fig, axes = plt.subplots(1, len(picks), figsize=(5.5 * len(picks), 4.2), squeeze=False)
    notes += ["## 4. 프레임 시간 분포 (반복을 합친 값, ms)", "", "| N | 대상 | p50 | p99 | 최대 | p99/p50 |",
              "|---:|---|---:|---:|---:|---:|"]
    for ax, n in zip(axes[0], picks):
        for t in targets:
            values = []
            for target, count, rep in ok:
                if target == t and count == n:
                    with (out / "raw" / f"{t}-{n}-rep{rep}.csv").open(newline="") as f:
                        values += [float(r["frame_ms"]) for r in csv.DictReader(f)]
            if not values:
                continue
            values.sort()
            k = len(values)
            # 초과 비율(그 시간보다 느린 프레임의 비율). 로그 축이라 드물게 튀는 프레임(꼬리)이 잘 보인다.
            ax.step(values, [(k - i - 1) / k if i < k - 1 else 1 / k for i in range(k)], where="post",
                    color=colors[t], label=t)
            p50, p99 = values[min(k - 1, int(0.5 * k))], values[min(k - 1, int(0.99 * k))]
            notes.append(f"| {n:,} | {t} | {p50:.3f} | {p99:.3f} | {values[-1]:.3f} | {p99 / p50:.2f} |")
        ax.axhline(0.01, color=INK_2, linewidth=1, linestyle=(0, (4, 3)))
        ax.annotate("p99 (1% of frames slower)", (1, 0.01), xycoords=("axes fraction", "data"), xytext=(-4, 4),
                    textcoords="offset points", ha="right", color=INK_2, fontsize=8)
        ax.set(xscale="log", yscale="log", xlabel="frame time (ms, log)", ylabel="share of frames slower than x (log)",
               title=f"N = {n:,}")
        plain_ticks(ax, "x")
        ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:.0%}" if v >= 0.01 else f"{v:.1%}"))
        ax.legend(loc="upper right")
    fig.text(0.01, 0.005, "Sharp drop = steady frames. A long tail to the right = occasional spikes (stutter).",
             color=INK_2, fontsize=8)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(out / "figures" / "4-distribution.png", dpi=150)
    plt.close(fig)
    notes.append("")


def main() -> None:
    out = Path(sys.argv[1])
    targets, counts, by = load(out)
    colors = dict(zip(targets, SERIES))
    (out / "figures").mkdir(exist_ok=True)
    notes = [f"# 그림 요약: {out.name}", "", "값은 반복 사이의 중앙값(4번은 반복을 합친 값)이다. 그림은 figures/*.png.", ""]
    fig_budget(out, targets, counts, by, colors, notes)
    fig_stages(out, targets, counts, by, colors, notes)
    fig_per_sprite(out, targets, counts, by, colors, notes)
    fig_distribution(out, targets, counts, colors, notes)
    (out / "figures" / "figures.md").write_text("\n".join(notes), encoding="utf-8")
    print(f"그림: {out / 'figures'}")


if __name__ == "__main__":
    main()
