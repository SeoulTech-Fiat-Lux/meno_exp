import sys, json, tomllib, tempfile, subprocess, os
from pathlib import Path
from types import SimpleNamespace as NS

H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import run
import llm

cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
tmp = Path(tempfile.mkdtemp())
run.runlog.CONTEXT_TOKENS = tmp / "context_tokens.jsonl"
real_sub = subprocess.run


def fake_run(cmd, **kw):
    prompt = kw.get("input", "")
    # fake tokenizer: 1 token per 3 bytes + 2000 tokens of CLI system prompt
    tokens = len(prompt.encode("utf-8")) // 3 + 2000
    rounds_text = "```cpp\nint main(){}\n```" if "main.cpp" in prompt or "오류" in prompt else "OK"
    return NS(returncode=0, stderr="", stdout=json.dumps({
        "is_error": False, "result": rounds_text, "session_id": "s", "stop_reason": "end_turn",
        "total_cost_usd": 0.1, "num_turns": 1, "modelUsage": {"claude-haiku-5-5": {}},
        "usage": {"input_tokens": tokens, "cache_creation_input_tokens": 0, "cache_read_input_tokens": 0,
                  "output_tokens": 700, "output_tokens_details": {"thinking_tokens": 100}}}))


run.subprocess.run = fake_run
run.shutil.which = lambda n: f"/fake/{n}"
llm.first_line = lambda c: "9.9.9"
light = next(m for m in cfg["models"] if m["tier"] == "light")
c = llm.ClaudeCLI(cfg)

# context measurement: difference must equal context bytes // 3 (approx, boundary)
rows = run.measure_context(cfg, light, c, ["meno", "sfml"])
for r in rows:
    expected = len((H.parent / cfg["conditions"][r["condition"]]["context"]).read_text(encoding="utf-8").rstrip().encode()) // 3
    assert abs(r["context_tokens"] - expected) <= 2, (r, expected)
assert len(run.runlog.CONTEXT_TOKENS.read_text(encoding="utf-8").splitlines()) == 2

# runs: pong, 2 conditions x 3 reps, build always ok
run.prepare_workspace = lambda cfg, cnd: Path(tempfile.mkdtemp())


def fb(cfg, work, code):
    (work / "build").mkdir(exist_ok=True); (work / "build" / "game").write_bytes(b"x")
    return True, "", 0.1


run.build = fb
out = tmp / "pilot"
CTX = {"inputs_sha256": "x", "frozen": False, "environment": {}}
with (out.mkdir(parents=True) or (out / "runs-light.jsonl")).open("w", encoding="utf-8") as f:
    for cond in ("meno", "sfml"):
        for rep in (1, 2, 3):
            res = run.run_once(cfg, run.Run(light, "01-pong", cond, rep), c, out, True, CTX)
            f.write(json.dumps(res, ensure_ascii=False) + "\n")

# summary: point BASE runs/context_tokens to our temp copy
subprocess.run = real_sub
sys.path.insert(0, str(H.parent / "analysis")); import summarize, records as runlog
records = runlog.load_records(out)
srows = summarize.summarize(records, runlog.load_jsonl(run.runlog.CONTEXT_TOKENS))
assert [r["condition"] for r in srows] == ["meno", "sfml"] and all(r["runs"] == 3 and r["build_success"] == 3 for r in srows)
for r in srows:
    assert r["context_tokens"] and r["noncontext_prompt_tokens"] and 0 < r["noncontext_prompt_tokens"] < r["prompt_tokens"], r
summarize.print_table(srows)
summarize.print_projection(srows, cfg)
print("ALL OK")
