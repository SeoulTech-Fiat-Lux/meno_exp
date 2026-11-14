import sys, json, tomllib, tempfile, os
from pathlib import Path
from types import SimpleNamespace as NS

H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import run
import llm

cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
cfg["experiment"]["limit_wait_minutes"] = 0
run.time.sleep = lambda s: None
calls, script = [], []


def fake_run(cmd, **kw):
    calls.append((cmd, kw))
    out = script.pop(0)
    return NS(returncode=out.get("rc", 0), stdout=out.get("stdout", ""), stderr=out.get("stderr", ""))


run.subprocess.run = fake_run
run.shutil.which = lambda name: f"/fake/{name}"
llm.first_line = lambda cmd: "9.9.9 (fake)"
home = Path(tempfile.mkdtemp()); os.environ["CODEX_HOME"] = str(home)
CTX = {"inputs_sha256": "x", "frozen": False, "environment": {}}


def claude_ok(text, sid, stop="end_turn", **u):
    usage = {"input_tokens": 100, "cache_creation_input_tokens": 50000, "cache_read_input_tokens": 0,
             "output_tokens": 900, "output_tokens_details": {"thinking_tokens": 300}} | u
    return {"stdout": json.dumps({"type": "result", "is_error": False, "result": text, "session_id": sid,
                                  "stop_reason": stop, "total_cost_usd": 0.42, "usage": usage, "num_turns": 1,
                                  "modelUsage": {"claude-opus-5-5": {}}})}


def claude_err(msg, status=None):
    return {"rc": 1, "stdout": json.dumps({"type": "result", "is_error": True, "result": msg,
                                           "api_error_status": status, "session_id": "x", "usage": {}})}


def arg(cmd, flag):
    return cmd[cmd.index(flag) + 1]


TOP = next(m for m in cfg["models"] if m["tier"] == "top")

# Claude: flags, stdin, token mapping
c = llm.ClaudeCLI(cfg)
script[:] = [claude_ok("```cpp\nint main(){}\n```", "s1"),
             claude_ok("main", "s2", cache_read_input_tokens=50000, cache_creation_input_tokens=0)]
r = run.Run(TOP, "probe", "probe", 0)
m = [{"role": "user", "content": "BIG PROMPT"}]
a = c.complete(r, m)
cmd, kw = calls[-1]
assert kw["input"] == "BIG PROMPT" and "--safe-mode" in cmd and arg(cmd, "--tools") == "" and "--resume" not in cmd
assert arg(cmd, "--model") == "claude-opus-5-5" and arg(cmd, "--effort") == TOP["effort"]
assert "당신은" in arg(cmd, "--system-prompt") and "ANTHROPIC_API_KEY" not in kw["env"]
assert kw["env"]["CLAUDE_CODE_MAX_OUTPUT_TOKENS"] == "64000" and kw["cwd"] and kw["cwd"] != os.getcwd()
assert (a.prompt_tokens, a.cached_tokens, a.cache_write_tokens, a.reasoning_tokens, a.finish_reason) == \
       (50100, 0, 50000, 300, "stop"), a
assert a.extra["cost_equiv_usd"] == 0.42 and a.model == "claude-opus-5-5"
# round 2: resume + fork from s1, same cwd
m += [{"role": "assistant", "content": a.content}, {"role": "user", "content": "FEEDBACK"}]
b = c.complete(r, m)
cmd2, kw2 = calls[-1]
assert kw2["input"] == "FEEDBACK" and arg(cmd2, "--resume") == "s1" and "--fork-session" in cmd2 and kw2["cwd"] == kw["cwd"]
assert b.prompt_tokens == 50100 and b.cached_tokens == 50000
# round 3: transient error then success, both from last good session s2
m += [{"role": "assistant", "content": "x"}, {"role": "user", "content": "F2"}]
script[:] = [claude_err("Server overloaded", 529), claude_ok("ok", "s3")]
c.complete(r, m)
assert arg(calls[-1][0], "--resume") == "s2" and arg(calls[-2][0], "--resume") == "s2"
assert c.sessions[r.run_id][1] == "s3"
# refusal / length
script[:] = [claude_ok("", "s9", stop="refusal")]
assert c.complete(run.Run(TOP, "g", "meno", 9), [{"role": "user", "content": "P"}]).refused
script[:] = [claude_ok("```cpp\nint", "s9", stop="max_tokens")]
assert c.complete(run.Run(TOP, "g", "meno", 8), [{"role": "user", "content": "P"}]).finish_reason == "length"
# context window -> RunFailure
script[:] = [claude_err("Prompt is too long")]
try:
    c.complete(run.Run(TOP, "g", "meno", 7), [{"role": "user", "content": "P"}]); raise AssertionError("no fail")
except llm.RunFailure as e:
    assert e.reason == "context_window"
# usage limit -> wait and resend same round
script[:] = [claude_err("Claude AI usage limit reached|1731600000"), claude_ok("ok", "s10")]
x = c.complete(run.Run(TOP, "g", "meno", 6), [{"role": "user", "content": "P"}])
assert x.extra["cli_retries"] == 1
cfg["experiment"]["limit_wait_hours"] = 0
cfg["experiment"]["limit_wait_minutes"] = 1
script[:] = [claude_err("usage limit reached")]
try:
    c.complete(run.Run(TOP, "g", "meno", 5), [{"role": "user", "content": "P"}]); raise AssertionError("no raise")
except llm.UsageLimitReached:
    pass
cfg["experiment"]["limit_wait_hours"] = 6
# auth -> stop harness
script[:] = [claude_err("Failed to authenticate: OAuth session expired and could not be refreshed")]
try:
    c.complete(run.Run(TOP, "g", "meno", 4), [{"role": "user", "content": "P"}]); raise AssertionError("no exit")
except SystemExit as e:
    assert "로그인" in str(e)

# Codex: refuses global AGENTS.md
(home / "AGENTS.md").write_text("my rules")
try:
    llm.CodexCLI(cfg); raise AssertionError("should refuse AGENTS.md")
except SystemExit as e:
    assert "AGENTS.md" in str(e)
(home / "AGENTS.md").unlink()
OV = dict(next(m for m in cfg["models"] if m["tier"] == "other-vendor"), id="codex-cli/gpt-x", model="gpt-x")
x = llm.CodexCLI(cfg)


def jl(*ev):
    return {"stdout": "\n".join(json.dumps(e) for e in ev)}


script[:] = [
    jl({"type": "thread.started", "thread_id": "t1"}, {"type": "turn.started"},
       {"type": "item.completed", "item": {"type": "reasoning", "text": "hm"}},
       {"type": "item.completed", "item": {"type": "agent_message", "text": "```cpp\nint main(){}\n```"}},
       {"type": "turn.completed", "usage": {"input_tokens": 60000, "cached_input_tokens": 0,
                                            "output_tokens": 800, "reasoning_output_tokens": 200}}),
    jl({"type": "turn.started"}, {"type": "item.completed", "item": {"type": "command_execution", "command": "ls"}},
       {"type": "item.completed", "item": {"type": "agent_message", "text": "main"}},
       {"type": "turn.completed", "usage": {"input_tokens": 61000, "cached_input_tokens": 60000,
                                            "output_tokens": 5, "reasoning_output_tokens": 0}})]
rr = run.Run(OV, "probe", "probe", 0)
m = [{"role": "user", "content": "BIG"}]
a = x.complete(rr, m)
cmd, kw = calls[-1]
assert cmd[:2] == ["/fake/codex", "exec"] and cmd[-1] == "-" and "resume" not in cmd and kw["input"] == "BIG"
for f in ("--json", "--skip-git-repo-check", "--ignore-user-config", "features.shell_tool=false",
          'approval_policy="never"', f'model_reasoning_effort="{OV["effort"]}"'):
    assert f in cmd, f
assert arg(cmd, "--sandbox") == "read-only" and arg(cmd, "-m") == "gpt-x"
inst = next(v for v in cmd if v.startswith("model_instructions_file="))
assert "당신은" in Path(json.loads(inst.split("=", 1)[1])).read_text(encoding="utf-8")
assert (a.prompt_tokens, a.cached_tokens, a.completion_tokens, a.reasoning_tokens, a.extra["tool_calls"]) == \
       (60000, 0, 800, 200, 0), a
m += [{"role": "assistant", "content": a.content}, {"role": "user", "content": "FB"}]
b = x.complete(rr, m)
cmd, kw = calls[-1]
assert arg(cmd, "resume") == "t1" and cmd[-1] == "-" and kw["input"] == "FB" and b.extra["tool_calls"] == 1
script[:] = [jl({"type": "thread.started", "thread_id": "t2"},
                {"type": "turn.failed", "error": {"message": "context_length_exceeded: input too long"}})]
try:
    x.complete(run.Run(OV, "g", "sfml", 1), [{"role": "user", "content": "P"}]); raise AssertionError("no fail")
except llm.RunFailure as e:
    assert e.reason == "context_window"

# CLI가 명령줄을 거부하면(모르는 플래그·설정 키) 기다리거나 다시 보내지 않고 바로 멈춘다
def no_sleep(seconds):
    raise AssertionError("명령줄 오류에서 기다리면 안 됨")
llm.time.sleep = no_sleep
for backend, rejected in (
        (x, {"rc": 2, "stderr": "error: unexpected argument '--ignore-user-config' found\n\nUsage: codex exec [OPTIONS]"}),
        (x, {"rc": 1, "stderr": "Error loading config: unknown feature `shell_tool`"}),
        (c, {"rc": 1, "stdout": "", "stderr": "error: unknown option '--safe-mode'"})):
    script[:] = [rejected]
    n = len(calls)
    try:
        backend.complete(run.Run(OV if backend is x else TOP, "g", "meno", 1), [{"role": "user", "content": "P"}])
        raise AssertionError("멈춰야 함")
    except SystemExit as e:
        assert "명령줄을 거부함" in str(e) and type(backend).__name__ in str(e), str(e)
    assert len(calls) == n + 1  # 한 번만 불렀다
llm.time.sleep = lambda s: None

# probe() on claude
script[:] = [claude_ok("```cpp\nint main() { return 0; }\n```", "p1"), claude_ok("main", "p2")]
assert llm.probe(TOP, c)

# run_once end-to-end with claude backend (build stubbed)
run.prepare_workspace = lambda cfg, cnd: Path(tempfile.mkdtemp())


def fb(cfg, work, code):
    (work / "build").mkdir(exist_ok=True); (work / "build" / "game").write_bytes(b"x")
    return True, "", 0.1


run.build = fb
script[:] = [claude_ok("```cpp\nint main(){}\n```", "e1")]
out = Path(tempfile.mkdtemp())
res = run.run_once(cfg, run.Run(TOP, "02-snake", "meno", 1), c, out, True, CTX)
assert res["backend"] == "claude-cli" and res["model_cli"] == "claude-opus-5-5" and res["effort"] == TOP["effort"]
assert res["client_versions"] == ["9.9.9 (fake)"] and res["cost_equiv_usd"] == 0.42 and "cost" not in res
assert res["backend"] == "claude-cli" and res["rounds_detail"][0]["session_id"] == "e1" and res["reasoning_tokens"] == 300
json.dumps(res)
print("ALL OK")
