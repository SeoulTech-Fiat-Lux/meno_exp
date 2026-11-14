"""기록 형식(harness/records.py)과 동결 범위(harness/ 폴더 전체)를 확인한다."""
import json
import sys
import tempfile
import tomllib
from pathlib import Path

H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import freeze
import records as runlog
import run
import llm

# 여러 사람의 기록 파일을 모두 모은다
d = Path(tempfile.mkdtemp())
(d / "runs-top_light.jsonl").write_text(json.dumps({"run_id": "a", "schema_version": 1}) + "\n", encoding="utf-8")
(d / "runs-other-vendor-1of2.jsonl").write_text(json.dumps({"run_id": "b"}) + "\n\n", encoding="utf-8")
(d / "context_tokens.jsonl").write_text("{}\n", encoding="utf-8")  # runs*.jsonl이 아니므로 읽지 않음
assert sorted(r["run_id"] for r in runlog.load_records(d)) == ["a", "b"]
assert [p.name for p in runlog.run_logs(d)] == ["runs-other-vendor-1of2.jsonl", "runs-top_light.jsonl"]

# 빈 폴더: 하니스(이어서 돌리기)는 허용, 분석은 멈춤
empty = Path(tempfile.mkdtemp())
assert runlog.load_records(empty, required=False) == []
try:
    runlog.load_records(empty); raise AssertionError("빈 폴더에서 멈춰야 함")
except SystemExit:
    pass

# 코드보다 새 형식의 기록이 섞이면 멈춤
(d / "runs-new.jsonl").write_text(json.dumps({"run_id": "c", "schema_version": runlog.SCHEMA_VERSION + 1}) + "\n",
                                  encoding="utf-8")
try:
    runlog.load_records(d); raise AssertionError("새 형식에서 멈춰야 함")
except SystemExit as e:
    assert "schema_version" in str(e)

# 동결은 harness/ 폴더 전체를 덮는다 (새 모듈도 자동으로)
hashes = freeze.file_hashes()
for name in ("run.py", "records.py", "freeze.py", "run_noinput.sh", "frametime_stats.py"):
    assert f"harness/{name}" in hashes, name
assert not any(k.startswith("analysis/") or k.startswith("tests/") for k in hashes)
assert not any("__pycache__" in k or k.endswith(".pyc") for k in hashes)

# 하니스가 쓰는 기록에 형식 버전이 들어감
cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
run.prepare_workspace = lambda c, n: Path(tempfile.mkdtemp())
run.build = lambda c, work, code: (False, "error: x", 0.1)
res = run.run_once(cfg, run.Run({"id": "mock"}, "01-pong", "meno", 1), llm.MockLLM(cfg, False), Path(tempfile.mkdtemp()),
                   True, {"inputs_sha256": "x", "frozen": False, "environment": {}})
assert res["schema_version"] == runlog.SCHEMA_VERSION
print("ALL OK")
