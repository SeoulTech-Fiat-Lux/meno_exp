"""탑다운 슈터(04-shooter)의 무입력 실행 시뮬레이션. rationale.md "탑다운 슈터 요구사항"의 근거.

명세의 무입력 기준: 적이 다가와 세 번 피격되고, 20초 안에 결과 화면으로 가야 한다.
입력이 없으면 플레이어는 월드 가운데에 서 있고 쏘지 않는다. 적이 플레이어에게 다가와 부딪히는 시각만 정해진다.

    python3 specs/sim/shooter_noinput.py [회차=2000] [시드=20261114]

출력: 첫 피격 시각과 결과 화면(세 번째 피격) 시각의 분포 (실행 시작 기준, 초).

명세(specs/games/04-shooter.md)에서 그대로 옮긴 규칙:
  - 타이틀 2초 뒤 플레이 시작. 60 FPS 고정 갱신(1/60초).
  - 월드 1920×1080, 플레이어는 가운데(960, 540), 반지름 14. 카메라 960×540이 플레이어를 가운데에 둔다.
  - 적은 플레이 시작 0.5초 뒤부터 1초마다 생기고, 간격은 10초마다 0.1초씩 줄어 최소 0.3초. 최대 200마리.
  - 생성 위치: 카메라 시야를 사방으로 64px 넓힌 사각형의 테두리 위, 월드 안의 무작위 점.
  - 적은 반지름 14, 매 갱신 플레이어를 향해 100 px/s. 겹치면(중심 거리 < 28) 그 적을 지우고 HP −1, 1초 무적.
  - 한 갱신에 여러 적과 겹치면 하나만 처리하고 바로 무적. 무적 동안에는 겹쳐도 아무 일 없음. HP 3에서 0이 되면 결과 화면.
명세가 정하지 않아 고른 해석 (결과에 주는 영향은 작음):
  - 테두리 위의 점은 둘레 길이에 대해 고르게 뽑는다.
  - 한 갱신의 순서: 적 생성 → 적 이동 → 충돌 판정 → 무적 시간 감소.
  - 생성 간격은 직전 생성 시점의 플레이 시간으로 정한다.
"""
import random
import statistics
import sys

DT = 1 / 60
TITLE = 2.0
PLAYER = (960.0, 540.0)
VIEW = (960, 540)
MARGIN = 64
ENEMY_SPEED = 100.0
HIT_DISTANCE = 14 + 14
INVULNERABLE = 1.0
HP = 3


def spawn_rect() -> tuple[float, float, float, float]:
    """카메라 시야를 64px 넓힌 사각형 (월드 1920×1080 안에 들어감)."""
    x0, y0 = PLAYER[0] - VIEW[0] / 2 - MARGIN, PLAYER[1] - VIEW[1] / 2 - MARGIN
    return x0, y0, x0 + VIEW[0] + 2 * MARGIN, y0 + VIEW[1] + 2 * MARGIN


def point_on_border(rng: random.Random, rect) -> tuple[float, float]:
    x0, y0, x1, y1 = rect
    w, h = x1 - x0, y1 - y0
    d = rng.uniform(0, 2 * (w + h))
    if d < w:
        return x0 + d, y0
    if d < w + h:
        return x1, y0 + (d - w)
    if d < 2 * w + h:
        return x1 - (d - w - h), y1
    return x0, y1 - (d - 2 * w - h)


def spawn_interval(play_time: float) -> float:
    return max(0.3, 1.0 - 0.1 * int(play_time // 10))


def run(rng: random.Random, limit: float = 60.0) -> list[float]:
    """피격 시각 목록 (실행 시작 기준)."""
    rect = spawn_rect()
    enemies: list[list[float]] = []
    next_spawn, invulnerable, hp, hits = 0.5, 0.0, HP, []
    t = 0.0  # 플레이 시간
    while t < limit and hp > 0:
        t += DT
        while t >= next_spawn:
            if len(enemies) < 200:
                enemies.append(list(point_on_border(rng, rect)))
            next_spawn += spawn_interval(next_spawn)
        for e in enemies:
            dx, dy = PLAYER[0] - e[0], PLAYER[1] - e[1]
            dist = (dx * dx + dy * dy) ** 0.5
            step = min(dist, ENEMY_SPEED * DT)
            if dist > 0:
                e[0] += dx / dist * step
                e[1] += dy / dist * step
        if invulnerable <= 0:
            for i, e in enumerate(enemies):
                if (e[0] - PLAYER[0]) ** 2 + (e[1] - PLAYER[1]) ** 2 < HIT_DISTANCE ** 2:
                    del enemies[i]
                    hp -= 1
                    hits.append(TITLE + t)
                    invulnerable = INVULNERABLE
                    break
        invulnerable -= DT
    return hits


def describe(name: str, values: list[float]) -> None:
    q = statistics.quantiles(values, n=20)
    print(f"{name}: 최소 {min(values):.2f}  5% {q[0]:.2f}  중앙 {statistics.median(values):.2f}  "
          f"95% {q[-1]:.2f}  최대 {max(values):.2f} 초")


def main() -> None:
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
    rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261114)
    results = [run(rng) for _ in range(trials)]
    first = [h[0] for h in results]
    over = [h[HP - 1] for h in results if len(h) >= HP]
    print(f"탑다운 슈터 무입력 실행, {trials}회 (실행 시작 기준, 타이틀 {TITLE:.0f}초 포함)")
    describe("첫 피격", first)
    describe("결과 화면(세 번째 피격)", over)
    print(f"20초 안에 결과 화면: {sum(t <= 20 for t in over)}/{trials}회")


if __name__ == "__main__":
    main()
