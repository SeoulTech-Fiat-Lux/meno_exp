"""서바이버류(11-survivor)의 무입력 실행 시뮬레이션. rationale.md "서바이버류 요구사항"의 근거.

명세의 무입력 기준: 적이 몰려와 자동 사격에 처치되고, 30초 안에 플레이어에 닿아 HP가 줄어들기 시작해야 한다.
그 전에 레벨업 카드가 뜨면 그 상태로 멈춰 있어도 된다.
입력이 없으면 플레이어는 월드 가운데에 서 있고, 자동 사격만 한다.

    python3 specs/sim/survivor_noinput.py [회차=200] [시드=20261114]

출력: 첫 접촉(HP 감소 시작) 시각의 분포와, 그 전에 레벨업 카드에서 멈추는 비율 (실행 시작 기준, 초).

명세(specs/games/11-survivor.md)에서 그대로 옮긴 규칙 (30초 안에 쓰이는 것만):
  - 타이틀 2초 뒤 플레이 시작. 60 FPS 고정 갱신(1/60초).
  - 월드 2880×1620, 플레이어는 가운데(1440, 810), 반지름 14. 카메라 960×540이 플레이어를 가운데에 둔다.
  - 적은 플레이 시작 0.5초 뒤부터 0.6초마다 생기고, 간격은 15초마다 0.05초씩 줄어 최소 0.15초. 60초 전에는 모두 걷는 적.
  - 걷는 적: 반지름 10, HP 10, 80 px/s, 매 갱신 플레이어를 향해 움직임. 생성 위치는 시야를 64px 넓힌 사각형의 테두리.
  - 자동 사격: 0.8초마다 플레이어 중심에서 400px 안의 가장 가까운 적을 향해 한 발. 없으면 쏘지 않음.
  - 총알: 반지름 4, 500 px/s, 1.2초 뒤 사라짐. 적 하나에 맞으면 사라지고 피해 10 (걷는 적은 한 발에 처치).
  - 처치된 적은 그 자리에 경험치 1짜리 보석을 남기고 0.1초 뒤 없어짐. 그동안 움직이지 않고 조준·충돌·접촉 대상이 아님.
  - 보석: 반지름 6, 30초 뒤 사라짐. 플레이어 중심에서 60px 안이면 300 px/s로 날아오고, 닿으면(중심 거리 < 20) 경험치.
  - 레벨 1에서 2로 오르려면 경험치 5. 오르면 게임이 멈추고 카드를 보여 준다 (무입력이면 거기서 멈춤).
  - 접촉: 적과 플레이어의 중심 거리 < 24이면 HP가 줄기 시작한다.
명세가 정하지 않아 고른 해석:
  - "0.8초마다"는 플레이 시작부터 0.8초 간격의 고정 시각으로 본다 (0.8, 1.6, …). 그 시각에 대상이 없으면 그 회는 건너뜀.
  - 총알은 쏜 순간의 목표 위치를 향해 곧게 날아간다 (따라가지 않음). 겹친 적이 여럿이면 가장 가까운 적에 맞는다.
  - 테두리 위의 점은 둘레 길이에 대해 고르게 뽑는다. 적끼리는 서로 밀어내지 않는다 (명세에 없음).
  - 한 갱신의 순서: 적 생성 → 사격 → 총알 이동·명중 → 적 이동 → 처치된 적 정리 → 보석 → 접촉 판정.
"""
import random
import statistics
import sys

DT = 1 / 60
TITLE = 2.0
PLAYER = (1440.0, 810.0)
VIEW, MARGIN = (960, 540), 64
WALKER_SPEED, WALKER_HP, WALKER_R = 80.0, 10, 10
PLAYER_R = 14
FIRE_EVERY, FIRE_RANGE = 0.8, 400.0
BULLET_SPEED, BULLET_LIFE, BULLET_R, DAMAGE = 500.0, 1.2, 4, 10
GEM_R, MAGNET, GEM_SPEED, GEM_LIFE = 6, 60.0, 300.0, 30.0
XP_TO_LEVEL_2 = 5
LIMIT = 30.0  # 무입력 기준의 시간 (실행 시작 기준)


def spawn_rect():
    x0, y0 = PLAYER[0] - VIEW[0] / 2 - MARGIN, PLAYER[1] - VIEW[1] / 2 - MARGIN
    return x0, y0, x0 + VIEW[0] + 2 * MARGIN, y0 + VIEW[1] + 2 * MARGIN


def point_on_border(rng, rect):
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


def dist2(a, b):
    return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2


def spawn_interval(play_time: float) -> float:
    return max(0.15, 0.6 - 0.05 * int(play_time // 15))


def run(rng: random.Random) -> tuple[str, float]:
    """('contact' | 'levelup' | 'none', 실행 시작 기준 시각)."""
    rect = spawn_rect()
    enemies = []   # {"pos": [x, y], "dying": 남은 시간 또는 None}
    bullets = []   # {"pos", "vel", "life"}
    gems = []      # {"pos", "life"}
    next_spawn, next_fire, xp = 0.5, FIRE_EVERY, 0
    t = 0.0
    while TITLE + t < LIMIT:
        t += DT
        while t >= next_spawn:
            if len(enemies) < 400:
                enemies.append({"pos": list(point_on_border(rng, rect)), "dying": None})
            next_spawn += spawn_interval(next_spawn)
        if t >= next_fire:
            next_fire += FIRE_EVERY
            alive = [e for e in enemies if e["dying"] is None and dist2(e["pos"], PLAYER) <= FIRE_RANGE ** 2]
            if alive:
                target = min(alive, key=lambda e: dist2(e["pos"], PLAYER))
                dx, dy = target["pos"][0] - PLAYER[0], target["pos"][1] - PLAYER[1]
                d = (dx * dx + dy * dy) ** 0.5 or 1.0
                bullets.append({"pos": list(PLAYER), "vel": (dx / d * BULLET_SPEED, dy / d * BULLET_SPEED),
                                "life": BULLET_LIFE})
        for b in bullets:
            b["pos"][0] += b["vel"][0] * DT
            b["pos"][1] += b["vel"][1] * DT
            b["life"] -= DT
            hit = [e for e in enemies if e["dying"] is None
                   and dist2(e["pos"], b["pos"]) < (WALKER_R + BULLET_R) ** 2]
            if hit:
                target = min(hit, key=lambda e: dist2(e["pos"], b["pos"]))
                target["dying"] = 0.1  # 걷는 적은 HP 10 = 피해 10으로 한 발에 처치
                gems.append({"pos": list(target["pos"]), "life": GEM_LIFE})
                b["life"] = 0
        bullets = [b for b in bullets if b["life"] > 0]
        for e in enemies:
            if e["dying"] is not None:
                e["dying"] -= DT
                continue
            dx, dy = PLAYER[0] - e["pos"][0], PLAYER[1] - e["pos"][1]
            d = (dx * dx + dy * dy) ** 0.5
            if d > 0:
                step = min(d, WALKER_SPEED * DT)
                e["pos"][0] += dx / d * step
                e["pos"][1] += dy / d * step
        enemies = [e for e in enemies if e["dying"] is None or e["dying"] > 0]
        for g in gems:
            g["life"] -= DT
            d = dist2(g["pos"], PLAYER) ** 0.5
            if 0 < d <= MAGNET:
                step = min(d, GEM_SPEED * DT)
                g["pos"][0] += (PLAYER[0] - g["pos"][0]) / d * step
                g["pos"][1] += (PLAYER[1] - g["pos"][1]) / d * step
            if dist2(g["pos"], PLAYER) < (PLAYER_R + GEM_R) ** 2:
                xp += 1
                g["life"] = 0
        gems = [g for g in gems if g["life"] > 0]
        if xp >= XP_TO_LEVEL_2:
            return "levelup", TITLE + t
        if any(e["dying"] is None and dist2(e["pos"], PLAYER) < (WALKER_R + PLAYER_R) ** 2 for e in enemies):
            return "contact", TITLE + t
    return "none", LIMIT


def main() -> None:
    trials = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 20261114)
    results = [run(rng) for _ in range(trials)]
    contact = [t for kind, t in results if kind == "contact"]
    levelup = sum(kind == "levelup" for kind, _ in results)
    none = sum(kind == "none" for kind, _ in results)
    print(f"서바이버류 무입력 실행, {trials}회 (실행 시작 기준, 타이틀 {TITLE:.0f}초 포함)")
    if contact:
        q = statistics.quantiles(contact, n=20) if len(contact) > 1 else [contact[0]] * 19
        print(f"첫 접촉: {len(contact)}회, 최소 {min(contact):.1f}  5% {q[0]:.1f}  중앙 {statistics.median(contact):.1f}  "
              f"95% {q[-1]:.1f}  최대 {max(contact):.1f} 초")
    print(f"접촉 전에 레벨업 카드에서 멈춤: {levelup}회 ({levelup / trials:.1%})")
    print(f"{LIMIT:.0f}초 안에 둘 다 없음 (무입력 기준 실패): {none}회")


if __name__ == "__main__":
    main()
