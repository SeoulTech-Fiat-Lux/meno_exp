import sys, types, tomllib, tempfile, json, subprocess
from pathlib import Path
from types import SimpleNamespace as NS
H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import run, freeze
cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
CTX = {"inputs_sha256": "x", "frozen": False, "environment": {}}

# --- #4 freeze checks
tags = freeze.template_tags(cfg)
assert tags["meno"] == {"meno": "46f8934f0f7754a7e09033c4413b6a5222d4493d", "SFML": "8835b6b955c96fa1ad1278a8dae6f05c4143effc"}, tags
assert tags["sfml"] == {"SFML": "8835b6b955c96fa1ad1278a8dae6f05c4143effc"}, tags
probs = freeze.consistency_problems()
assert not [p for p in probs if "커밋" in p or "GIT_TAG" in p or "해시" in p], probs
bad = 'FetchContent_Declare(\n SFML\n GIT_REPOSITORY x\n GIT_TAG 3.1.0\n GIT_SHALLOW ON\n)'
assert freeze.declared_tags(bad) == {"SFML": "3.1.0"} and not freeze.COMMIT.match("3.1.0")
v = run.template_versions(cfg); assert v["sfml_commit"] == v["sfml_commit_meno_condition"] == tags["sfml"]["SFML"], v

# --- #1a frametime_stats without dump
d = Path(tempfile.mkdtemp()); (d / "result.json").write_text('{"exit_code": 1}')
p = subprocess.run([sys.executable, "-I", str(H / "frametime_stats.py"), str(d)], capture_output=True, text=True)
assert p.returncode == 0, p.stderr
r = json.loads((d / "result.json").read_text()); assert r == {"exit_code": 1, "frames": 0, "frametime_dump": False}, r

# --- #1b recorded API failures
class BadRequestError(Exception): pass
class ContextWindowExceededError(BadRequestError): pass
class ContentPolicyViolationError(BadRequestError): pass
class AuthenticationError(Exception): pass
exc = types.SimpleNamespace(BadRequestError=BadRequestError, ContextWindowExceededError=ContextWindowExceededError,
                            ContentPolicyViolationError=ContentPolicyViolationError, AuthenticationError=AuthenticationError)
queue = []
def completion(**kw):
    item = queue.pop(0)
    if isinstance(item, Exception): raise item
    return item
sys.modules["litellm"] = types.SimpleNamespace(exceptions=exc, drop_params=False, completion=completion,
                                               completion_cost=lambda completion_response: 0.0)
llm = run.LiteLLM(cfg)
run.prepare_workspace = lambda cfg, c: Path(tempfile.mkdtemp())
ok_reply = NS(model="m", usage=None, choices=[NS(finish_reason="stop", message=NS(content="```cpp\nint main(){}\n```", refusal=None))])
bad_reply = NS(model="m", usage=None, choices=[NS(finish_reason="stop", message=NS(content="```cpp\nint main(\n```", refusal=None))])
out = Path(tempfile.mkdtemp())
M = {"id": "openai/gpt-x", "tier": "top"}

# context window on round 2 (after one failed build)
run.build = lambda cfg, work, code: (False, "error: x", 0.1)
queue[:] = [bad_reply, ContextWindowExceededError("too long")]
res = run.run_once(cfg, run.Run(M, "02-snake", "sfml", 1), llm, out, True, 1, CTX)
assert res["failure"] == "context_window" and res["rounds"] == 2 and res["rounds_detail"][1]["api_failure"] == "context_window", res
assert "ContextWindowExceededError" in res["failure_detail"] and res["attempt"] == 1, res
# content policy -> refused
queue[:] = [ContentPolicyViolationError("nope")]
res = run.run_once(cfg, run.Run(M, "02-snake", "sfml", 2), llm, out, True, 1, CTX)
assert res["failure"] == "refused" and res["rounds_detail"][0]["refused"], res
# auth error must stop the harness
queue[:] = [AuthenticationError("bad key")]
try:
    run.run_once(cfg, run.Run(M, "02-snake", "sfml", 3), llm, out, True, 1, CTX); raise SystemExit("should have raised")
except AuthenticationError: pass

# --- #1c interrupted attempt: run r3 left a folder behind -> moved, attempt 2
assert (out / "02-snake-sfml-openai_gpt-x-r3" / "prompt.md").exists()
def fake_build(cfg, work, code):
    (work / "build").mkdir(exist_ok=True); (work / "build" / "game").write_bytes(b"x"); return True, "", 0.1
run.build = fake_build
# --- #1d no-input step fails -> recorded, not raised
real_run = subprocess.run
def fake_sub(cmd, **kw):
    if "run_noinput.sh" in cmd[0]:
        return NS(returncode=2, stderr="Xvfb failed")
    return real_run(cmd, **kw)
run.subprocess.run = fake_sub
queue[:] = [ok_reply]
res = run.run_once(cfg, run.Run(M, "02-snake", "sfml", 3), llm, out, False, 1, CTX)
assert res["attempt"] == 2 and res["interrupted_attempts"] == ["02-snake-sfml-openai_gpt-x-r3.interrupted-1"], res
assert (out / "02-snake-sfml-openai_gpt-x-r3.interrupted-1" / "prompt.md").exists()
assert res["build_success"] and res["failure"] is None and "Xvfb failed" in res["noinput"]["harness_error"][0], res
json.dumps(res)
print("ALL OK")
