"""결과 합치기(grading/merge_runs.py), 대표작 고르기(analysis/showcase.py), 동결 전제 조건(freeze.py)을 확인한다."""
import csv
import json
import shutil
import sys
import tempfile
import tomllib
import zlib
from pathlib import Path

LC = Path(__file__).resolve().parents[1]
for sub in ("harness", "grading", "analysis"):
    sys.path.insert(0, str(LC / sub))
import freeze
import merge_runs
import showcase

cfg = tomllib.loads((LC / "experiment.toml").read_text(encoding="utf-8"))
tmp = Path(tempfile.mkdtemp())
LOCK = "a" * 64
(tmp / "freeze.lock.json").write_text(json.dumps({"inputs_sha256": LOCK}), encoding="utf-8")
merge_runs.BASE = tmp  # 저장소의 잠금 파일 대신 가짜 잠금을 읽도록 한다


def make_sources(root: Path) -> list[Path]:
    """계획대로 모든 회차를 세 사람 몫으로 나눠 만든다 (Claude 1인, ChatGPT 2인)."""
    runs = merge_runs.runlog.main_plan(cfg)
    other_games = sorted(set(cfg["games"]["main"]))
    sources = {name: root / name for name in ("claude", "chatgpt1", "chatgpt2")}
    for run in runs:
        if run.model.get("backend") == "codex-cli":
            who = "chatgpt1" if other_games.index(run.game) % 2 == 0 else "chatgpt2"
            log = f"runs-other-vendor-{1 if who == 'chatgpt1' else 2}of2.jsonl"
        else:
            who, log = "claude", "runs-top_light.jsonl"
        folder = sources[who] / run.run_id
        folder.mkdir(parents=True)
        built = zlib.crc32(run.run_id.encode()) % 4 != 0
        if built:
            (folder / "game").write_bytes(b"x")
        record = {"schema_version": 1, "run_id": run.run_id, "inputs_sha256": LOCK, "frozen": True,
                  "build_success": built, "game": run.game, "condition": run.condition,
                  "model_requested": run.model["id"], "rounds": 1 + zlib.crc32(run.run_id.encode()) % 3, "completion_tokens": 1000}
        with (sources[who] / log).open("a", encoding="utf-8") as f:
            f.write(json.dumps(record) + "\n")
    return list(sources.values())


# 1) 정상: 문제가 없고, 합치면 모든 회차와 기록 파일이 한곳에 모인다
sources = make_sources(tmp / "ok")
problems, records = merge_runs.check(sources, cfg)
assert problems == [], problems
plan = merge_runs.expected_run_ids(cfg)
assert len(records) == len(plan) == 3 * len(cfg["games"]["main"]) * 2 * cfg["experiment"]["reps"]
dest = tmp / "merged"
merge_runs.merge(sources, dest)
assert merge_runs.check([dest], cfg)[0] == []
assert len(list(p for p in dest.iterdir() if p.is_dir())) == len(plan)

# 2) 문제들: 같은 회차 두 번, 다른 입력 해시, 동결 아님, 실행 파일 빠짐, 회차 빠짐
bad = make_sources(tmp / "bad")
claude_log = bad[0] / "runs-top_light.jsonl"
lines = claude_log.read_text(encoding="utf-8").splitlines()
first = json.loads(lines[0])
dup_dir = bad[1] / first["run_id"]
shutil.copytree(bad[0] / first["run_id"], dup_dir)
with (bad[1] / "runs-extra.jsonl").open("w", encoding="utf-8") as f:
    f.write(json.dumps(first) + "\n")
second = json.loads(lines[1]) | {"inputs_sha256": "b" * 64, "frozen": False}
built = next(json.loads(l) for l in lines[2:] if json.loads(l)["build_success"])
(bad[0] / built["run_id"] / "game").unlink()
lines = [lines[0], json.dumps(second)] + lines[2:-1]  # 마지막 회차는 기록에서 뺀다
claude_log.write_text("\n".join(lines) + "\n", encoding="utf-8")
problems, _ = merge_runs.check(bad, cfg)
text = "\n".join(problems)
for expected in ("2번 기록됨", "입력 해시가 여러 개", "동결 잠금", "동결 상태가 아닐 때", "실행 파일이 없음", "기록이 없는 회차 1개"):
    assert expected in text, (expected, problems)

# 3) 대표작: 통과율 → 왕복 수 → 출력 토큰 → 회차 ID
recs = [
    {"run_id": "g-meno-a", "game": "g", "condition": "meno", "build_success": True, "rounds": 3, "completion_tokens": 10, "model_requested": "m"},
    {"run_id": "g-meno-b", "game": "g", "condition": "meno", "build_success": True, "rounds": 1, "completion_tokens": 99, "model_requested": "m"},
    {"run_id": "g-meno-c", "game": "g", "condition": "meno", "build_success": True, "rounds": 1, "completion_tokens": 50, "model_requested": "m"},
    {"run_id": "g-sfml-a", "game": "g", "condition": "sfml", "build_success": True, "rounds": 1, "completion_tokens": 1, "model_requested": "m"},
    {"run_id": "g-sfml-b", "game": "g", "condition": "sfml", "build_success": True, "rounds": 5, "completion_tokens": 1, "model_requested": "m"},
    {"run_id": "g-sfml-x", "game": "g", "condition": "sfml", "build_success": False, "rounds": 5, "model_requested": "m"},
]
scores = {"g-meno-a": 0.9, "g-meno-b": 0.9, "g-meno-c": 0.9, "g-sfml-a": 0.5, "g-sfml-b": 0.8}
chosen = {(r["game"], r["condition"]): r["run_id"] for r in showcase.pick(recs, scores)}
assert chosen == {("g", "meno"): "g-meno-c", ("g", "sfml"): "g-sfml-b"}, chosen

# 대표작 폴더를 만든다 (Windows에서는 심볼릭 링크 권한이 없을 수 있어 폴더로 대신한다)
Path.symlink_to = lambda self, target: self.mkdir()
scores_csv = tmp / "run_scores.csv"
with scores_csv.open("w", encoding="utf-8-sig", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["run_id", "pass_rate"])
    w.writeheader()
    w.writerows({"run_id": r["run_id"], "pass_rate": 0.7} for r in records if r["build_success"])
sys.argv = ["showcase.py", str(dest), str(scores_csv), str(tmp / "showcase")]
showcase.main()
rows = list(csv.DictReader(open(tmp / "showcase" / "showcase.csv", encoding="utf-8-sig")))
assert len(rows) == len(cfg["games"]["main"]) * 2, rows
assert all((tmp / "showcase" / r["folder"] / "game").exists() for r in rows)

# 4) 동결 전제 조건: sim 스크립트가 있는지, 채점표 문장이 명세와 같은지
fake = tmp / "freeze_base"
(fake / "specs" / "games").mkdir(parents=True)
(fake / "grading" / "sheets").mkdir(parents=True)
game = "11-survivor"  # 시뮬레이션 스크립트가 필요한 게임
shutil.copy(LC / "specs" / "games" / f"{game}.md", fake / "specs" / "games" / f"{game}.md")
shutil.copy(LC / "grading" / "sheets" / f"{game}.csv", fake / "grading" / "sheets" / f"{game}.csv")
shutil.copy(LC / "grading" / "sheets" / "items.csv", fake / "grading" / "sheets" / "items.csv")
freeze.BASE = fake
mini = {"games": {"main": [game]}}
probs = freeze.prerequisite_problems(mini)
assert len(probs) == 1 and "survivor_noinput.py" in probs[0], probs
assert not [p for p in freeze.prerequisite_problems({"games": {"main": []}}) if "sim" in p]  # 쓰지 않는 게임의 스크립트는 요구하지 않는다
(fake / "specs" / "sim").mkdir()
for name in freeze.SIM_SCRIPTS.values():
    (fake / "specs" / "sim" / name).write_text("# sim\n")
assert freeze.prerequisite_problems(mini) == []
spec = fake / "specs" / "games" / f"{game}.md"
spec.write_text(spec.read_text(encoding="utf-8").replace("assets/fonts/game.ttf로 그리며", "fonts/game.ttf로 그리며"),
                encoding="utf-8")  # 명세만 고치고 채점표는 그대로 둔다
probs = freeze.prerequisite_problems(mini)
assert len(probs) == 1 and "명세 문장과 다름" in probs[0], probs
(fake / "grading" / "sheets" / f"{game}.csv").unlink()
assert "채점표가 없음" in freeze.prerequisite_problems(mini)[0]
print("ALL OK")
