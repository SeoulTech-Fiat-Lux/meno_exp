import sys, json, tomllib, tempfile
from pathlib import Path
from types import SimpleNamespace as NS

H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import run

cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
run.time.sleep = lambda s: (_ for _ in ()).throw(AssertionError("must not sleep/retry on output limit"))
calls, script = [], []


def fake_run(cmd, **kw):
    calls.append((cmd, kw)); out = script.pop(0)
    return NS(returncode=out.get("rc", 0), stdout=out["stdout"], stderr="")


run.subprocess.run = fake_run
run.shutil.which = lambda n: f"/fake/{n}"
run._first_line = lambda c: "9.9.9"
LIMIT_MSG = ("API Error: Claude's response exceeded the 32000 output token maximum. To configure this behavior, "
             "set the CLAUDE_CODE_MAX_OUTPUT_TOKENS environment variable.")


def ok(text, sid):
    return {"stdout": json.dumps({"is_error": False, "result": text, "session_id": sid, "stop_reason": "end_turn",
                                  "usage": {"input_tokens": 10, "output_tokens": 500}, "modelUsage": {"h": {}}})}


def limit(sid):
    return {"rc": 1, "stdout": json.dumps({"is_error": True, "result": LIMIT_MSG, "session_id": sid,
                                           "usage": {"input_tokens": 10, "output_tokens": 32000}})}


light = next(m for m in cfg["models"] if m["tier"] == "light")
c = run.ClaudeCLI(cfg)
run.prepare_workspace = lambda cfg, cnd: Path(tempfile.mkdtemp())


def fb(cfg, work, code):
    (work / "build").mkdir(exist_ok=True); (work / "build" / "game").write_bytes(b"x"); return True, "", 0.1


run.build = fb
CTX = {"inputs_sha256": "x", "frozen": False, "environment": {}}
out = Path(tempfile.mkdtemp())
TRUNC = (H.parent / "prompts" / "truncated.md").read_text(encoding="utf-8")

# Case A: round 1 cut off -> round 2 = fresh session, original prompt + truncated notice (once each)
script[:] = [limit("x1"), ok("```cpp\nint main(){}\n```", "s2")]
res = run.run_once(cfg, run.Run(light, "01-pong", "meno", 1), c, out, True, 1, CTX)
assert res["build_success"] and res["rounds"] == 2 and res["truncated_rounds"] == 1, res
r1, r2 = res["rounds_detail"]
assert r1["truncated"] and r1["finish_reason"] == "length" and "output_limit_error" in r1 and r1["completion_tokens"] == 32000
cmd2, kw2 = calls[-1]
prompt0 = (out / "01-pong-meno-claude-cli_claude-haiku-5-5-r1" / "prompt.md").read_text(encoding="utf-8")
assert "--resume" not in cmd2 and kw2["input"] == prompt0 + "\n\n" + TRUNC, kw2["input"][-300:]
assert kw2["input"].count("# 요구사항 문서") == 1

# Case B: round 2 cut off -> round 3 resumes from round-1 session, resends the build-error feedback + notice
calls.clear()
run.build = lambda cfg, work, code: (False, "main.cpp:1: error: boom", 0.1) if "BAD" in code else fb(cfg, work, code)
script[:] = [ok("```cpp\nBAD\n```", "s1"), limit("x2"), ok("```cpp\nint main(){}\n```", "s3")]
res = run.run_once(cfg, run.Run(light, "01-pong", "sfml", 1), c, out, True, 1, CTX)
assert res["build_success"] and res["rounds"] == 3 and res["truncated_rounds"] == 1, res
cmd3, kw3 = calls[-1]
assert cmd3[cmd3.index("--resume") + 1] == "s1", cmd3
feedback = (out / "01-pong-sfml-claude-cli_claude-haiku-5-5-r1" / "feedback_1.md").read_text(encoding="utf-8")
assert kw3["input"] == feedback + "\n\n" + TRUNC
assert len(calls) == 3  # no retries
print("ALL OK")
