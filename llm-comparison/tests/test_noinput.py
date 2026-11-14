import sys, json, tomllib, tempfile
from pathlib import Path
from types import SimpleNamespace as NS
H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import run
import llm
cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
real = run.subprocess.run
def fake(cmd, **kw):
    if "run_noinput.sh" in cmd[0]:
        out = Path(cmd[2]); out.mkdir(parents=True, exist_ok=True)
        (out / "result.json").write_text('{"exit_code": 1, "elapsed_s": .31, "early_exit": true}\n')
        return NS(returncode=0, stderr="")
    return real(cmd, **kw)
run.subprocess.run = fake
run.prepare_workspace = lambda c, n: Path(tempfile.mkdtemp())
def fb(c, work, code):
    (work / "build").mkdir(exist_ok=True); (work / "build" / "game").write_bytes(b"x"); return True, "", 0.1
run.build = fb
res = run.run_once(cfg, run.Run({"id": "mock"}, "01-pong", "sfml", 1), llm.MockLLM(cfg, False), Path(tempfile.mkdtemp()),
                   False, {"inputs_sha256": "x", "frozen": False, "environment": {}})
err = res["noinput"]["harness_error"]
assert res["build_success"] and len(err) == 2 and "result.json" in err[-1] and ".31" in err[-1], err
print("ALL OK")
