"""그림(analysis/figures.py)과 빌드 오류 분류(analysis/errors.py)를 확인한다.
matplotlib이 있으면 합성 데이터로 그림을 실제로 그리고, 없으면 그리기 전 단계까지만 본다 (하니스에는 matplotlib이 필요 없다)."""
import json
import random
import sys
import tempfile
import tomllib
from pathlib import Path

LC = Path(__file__).resolve().parents[1]
for sub in ("harness", "analysis"):
    sys.path.insert(0, str(LC / sub))
import errors
import figures
import records

# 빌드 오류 분류: SFML 2 습관(기본 생성자 등)이 일반 C++ 오류로 새지 않아야 한다
cases = [
    ("main.cpp:3: error: no matching function for call to 'sf::Text::Text()'", "sfml", "sfml2_api"),
    ("main.cpp:3: error: no matching function for call to 'sf::Sprite::Sprite()'", "sfml", "sfml2_api"),
    ("main.cpp:9: error: 'class sf::Event' has no member named 'type'", "sfml", "sfml2_api"),
    ("main.cpp:1: error: 'A' is not a member of 'sf::Keyboard'", "sfml", "sfml2_api"),
    ("main.cpp:2: error: 'class sf::Music' has no member named 'setLoop'", "sfml", "sfml2_api"),
    ("main.cpp:3:10: fatal error: SFML/Graphics.hpp: No such file or directory", "meno", "sfml_in_meno"),
    ("main.cpp:9: error: 'class meno::Scene' has no member named 'add'", "meno", "meno_api"),
    ("main.cpp:(.text+0x1): undefined reference to `foo'", "sfml", "link"),
    ("main.cpp:5: error: expected ';' before '}' token", "sfml", "cpp"),
]
for log, condition, expected in cases:
    assert errors.classify(log, condition)[0] == expected, (log, errors.classify(log, condition))
assert errors.SFML2.groups == 0  # 패턴을 이어 붙여도 역참조가 밀리지 않아야 한다

# 오류 유형마다 색이 고정된다 (조건에 없는 유형이 있어도 밀리지 않는다)
assert len(figures.ERROR_TYPES) == len(figures.SERIES) and figures.CONDITION == {"meno": "#2a78d6", "sfml": "#eb6834"}
assert figures.p_text({"p": 0.0004}, False) == "p < 0.001" and figures.p_text({"p_holm": 0.25}, True) == "Holm p = 0.250"
assert figures.p_text({}, True) == "p = n/a"

try:
    import matplotlib  # noqa: F401
except ImportError:
    print("matplotlib 없음: 그리기는 건너뜀 (pip install -r analysis/requirements.txt)")
    print("ALL OK")
    sys.exit(0)

# 합성 본 실행으로 실제 그리기: 채점 뒤 3장, 채점 전 2장
cfg = tomllib.loads((LC / "experiment.toml").read_text(encoding="utf-8"))
rng = random.Random(3)
tmp = Path(tempfile.mkdtemp())
runs = tmp / "runs"; runs.mkdir()
scores = []
with (runs / "runs.jsonl").open("w", encoding="utf-8") as f:
    for run in records.main_plan(cfg):
        built = rng.random() < 0.85
        rounds = 1 if built and rng.random() < 0.6 else (rng.randint(2, 5) if built else 5)
        d = runs / run.run_id; d.mkdir()
        for i in range(1, rounds + (0 if built else 1)):
            (d / f"build_{i}.log").write_text(rng.choice([c[0] for c in cases if c[1] == run.condition]), encoding="utf-8")
        detail = [{"round": i, "code_found": True, "build_ok": built and i == rounds} for i in range(1, rounds + 1)]
        f.write(json.dumps({"schema_version": 2, "run_id": run.run_id, "model_requested": run.model["id"],
                            "game": run.game, "condition": run.condition, "rep": run.rep, "build_success": built,
                            "rounds": rounds, "rounds_detail": detail, "completion_tokens": rng.randint(20000, 40000),
                            "code_lines": 300 if built else None, "attempt": 1}) + "\n")
        if built:
            scores.append(f"{run.run_id},{rng.random():.3f}")
(tmp / "run_scores.csv").write_text("run_id,pass_rate\n" + "\n".join(scores) + "\n", encoding="utf-8")
for scores_arg, out, expected in ((str(tmp / "run_scores.csv"), tmp / "graded", 3), ("-", tmp / "pre", 2)):
    sys.argv = ["figures.py", str(runs), scores_arg, str(out)]
    figures.main()
    made = sorted(p.name for p in (out / "figures").glob("*.png"))
    assert len(made) == expected and all((out / "figures" / m).stat().st_size > 10_000 for m in made), made
print("ALL OK")
