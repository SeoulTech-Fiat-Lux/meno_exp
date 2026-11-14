"""그림 색과 서식 (figures.py, tracy.py). 이 모듈은 matplotlib를 import하지 않으며, 서식은 apply()를 부를 때만 바뀐다.

색은 dataviz 기본 팔레트(밝은 모드)다. 이 순서가 색각 이상이 있어도 색이 구분되도록 보장하므로 바꾸지 않는다.
"""
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
GRID = "#e4e3df"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948"]
# 선 그래프와 누적 막대는 이웃한 칸끼리만 구분되면 된다(dataviz 팔레트의 "adjacent" 검증, 여덟 칸 모두 통과).
# 대상은 4개까지 쓴다. 넷째 칸(노랑)은 밝은 배경 대비가 3:1보다 낮으므로 선 끝 이름표와 figures.md 표를 함께 둔다.
MAX_OVERLAID = 4
# 색은 대상 이름을 따른다(순서를 따르지 않음). 대상을 더하거나 빼도 기존 대상의 색이 바뀌지 않게 한다.
TARGET_COLORS = {"sfml": SERIES[0], "meno-renderer": SERIES[1], "meno-object": SERIES[2], "sfml-batched": SERIES[3]}
# 흰 글자가 잘 안 보이는 밝은 칸. 그 위의 글자는 INK로 쓴다.
LIGHT = {"#1baf7a", "#eda100", "#e87ba4"}
MUTED = ["#b5b3ad", "#dddbd5"]  # "그 밖", "계측 안 됨"처럼 낮춰 보일 칸


def apply(plt) -> None:
    plt.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "axes.edgecolor": GRID, "axes.labelcolor": INK_2, "xtick.color": INK_2, "ytick.color": INK_2,
        "text.color": INK, "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
        "axes.spines.top": False, "axes.spines.right": False, "font.size": 10,
        "axes.titlesize": 11, "axes.titleweight": "bold", "axes.titlelocation": "left",
        "lines.linewidth": 2, "lines.markersize": 5, "legend.frameon": False,
    })


def target_colors(targets: list[str]) -> dict[str, str]:
    """이름이 정해진 대상은 그 색을, 처음 보는 대상은 남은 칸을 차례로 쓴다."""
    spare = [c for c in SERIES if c not in TARGET_COLORS.values()]
    return {t: TARGET_COLORS.get(t) or spare.pop(0) for t in targets}


def text_on(color: str) -> str:
    return INK if color in LIGHT or color in MUTED else SURFACE
