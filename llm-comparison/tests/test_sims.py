"""specs/sim/의 무입력 시뮬레이션이 명세의 무입력 기준과 rationale.md의 수치를 계속 뒷받침하는지 확인한다.
명세 수치(속력, 간격 등)를 고치면 이 테스트가 걸러 준다. 회차를 줄여 몇 초 안에 끝낸다."""
import importlib.util
import random
import statistics
from pathlib import Path

SIM = Path(__file__).resolve().parents[1] / "specs" / "sim"


def load(name):
    spec = importlib.util.spec_from_file_location(name, SIM / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# 탑다운 슈터: 첫 피격 약 5.5~8.6초, 결과 화면 약 7.6~10.6초, 모두 20초 안 (명세 기준)
shooter = load("shooter_noinput")
rng = random.Random(1)
hits = [shooter.run(rng) for _ in range(300)]
first = [h[0] for h in hits]
over = [h[2] for h in hits]
assert 5.5 <= min(first) and max(first) <= 8.7, (min(first), max(first))
assert 7.5 <= min(over) and max(over) <= 10.7 and max(over) <= 20, (min(over), max(over))

# 서바이버류: 30초 안에 접촉하거나 레벨업 카드에서 멈춤 (명세 기준), 첫 접촉 중앙값 약 20초, 레벨업은 드묾
survivor = load("survivor_noinput")
rng = random.Random(2)
results = [survivor.run(rng) for _ in range(150)]
assert not [r for r in results if r[0] == "none"], "30초 안에 접촉도 레벨업도 없는 회차가 있음"
contact = [t for kind, t in results if kind == "contact"]
assert 17 <= statistics.median(contact) <= 22, statistics.median(contact)
assert sum(kind == "levelup" for kind, _ in results) / len(results) < 0.08
print("ALL OK")
