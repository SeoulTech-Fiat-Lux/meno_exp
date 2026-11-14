"""그림 색과 서식 (figures.py, tracy.py). matplotlib를 import하지 않으며, 서식은 apply()를 부를 때만 바뀐다.

색은 dataviz 기본 팔레트(밝은 모드)다. 순서가 색각 이상 대비 구분을 보장하므로 바꾸지 않는다.
"""
SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
GRID = "#e4e3df"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948"]
# 겹쳐 그리는 선·점(모든 쌍이 서로 구분돼야 함)은 앞 세 칸까지만 검증됐다. 이웃한 칸만 구분되면 되는
# 누적 막대는 여덟 칸을 다 쓴다.
MAX_OVERLAID = 3
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


def text_on(color: str) -> str:
    return INK if color in LIGHT or color in MUTED else SURFACE
