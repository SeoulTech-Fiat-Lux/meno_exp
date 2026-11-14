import sys, types, tomllib, tempfile, json
from pathlib import Path
from types import SimpleNamespace as NS
H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import run
cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
calls = []
def make(finish, content, refusal=None, usage=None):
    return NS(model="served-x", usage=usage, choices=[NS(finish_reason=finish, message=NS(content=content, refusal=refusal))])
queue = []
fake = types.SimpleNamespace(exceptions=types.SimpleNamespace(), drop_params=False,
    completion=lambda **kw: (calls.append(kw), queue.pop(0))[1],
    completion_cost=lambda completion_response: 0.01)
sys.modules["litellm"] = fake
llm = run.LiteLLM(cfg)
usage = NS(prompt_tokens=50000, completion_tokens=9000,
           prompt_tokens_details=NS(cached_tokens=48000), cache_creation_input_tokens=0,
           completion_tokens_details=NS(reasoning_tokens=4000))
# 1) Claude: cache breakpoint + token fields
queue.append(make("stop", "```cpp\nint main(){}\n```", usage=usage))
r = llm.complete(run.Run({"id": "anthropic/claude-x"}, "02-snake", "meno", 1), [{"role": "user", "content": "P"}])
m = calls[-1]["messages"][0]
assert m["content"][0]["cache_control"] == {"type": "ephemeral"} and m["content"][0]["text"] == "P", m
assert (r.reasoning_tokens, r.cached_tokens, r.cache_write_tokens, r.refused) == (4000, 48000, 0, False), r
# 2) OpenAI: no breakpoint, refusal field, dict usage
queue.append(make("stop", "", refusal="I can't", usage={"prompt_tokens": 1, "completion_tokens": 2}))
r = llm.complete(run.Run({"id": "openai/gpt-x"}, "02-snake", "meno", 1), [{"role": "user", "content": "P"}])
assert calls[-1]["messages"][0]["content"] == "P" and r.refused and r.reasoning_tokens is None, r
# 3) finish_reason refusal / content_filter
for f in ("refusal", "content_filter"):
    queue.append(make(f, "", usage=None)); assert llm.complete(run.Run({"id":"anthropic/claude-x"},"g","meno",1),[{"role":"user","content":"P"}]).refused
# 4) run_once: refusal ends immediately as failure, no build
run.prepare_workspace = lambda cfg, c: Path(tempfile.mkdtemp())
run.build = lambda *a: (_ for _ in ()).throw(AssertionError("build must not run"))
queue.append(make("refusal", "", usage=usage))
out = Path(tempfile.mkdtemp())
res = run.run_once(cfg, run.Run({"id": "anthropic/claude-x", "tier": "top"}, "02-snake", "meno", 1), llm, out, True, 1,
                   {"inputs_sha256": "x", "frozen": False, "environment": {}})
assert res["failure"] == "refused" and res["rounds"] == 1 and not res["build_success"] and res["prompt_cache"], res
assert res["reasoning_tokens"] == 4000 and res["cached_tokens"] == 48000
json.dumps(res)
# 5) no-code answer w/o refusal -> format error, continues; then exhausted
queue.extend([make("stop", "설명만", usage=usage) for _ in range(cfg["experiment"]["max_rounds"])])
res = run.run_once(cfg, run.Run({"id": "openai/gpt-x"}, "02-snake", "sfml", 2), llm, out, True, 1,
                   {"inputs_sha256": "x", "frozen": False, "environment": {}})
assert res["failure"] == "rounds_exhausted" and res["rounds"] == cfg["experiment"]["max_rounds"] and not res["prompt_cache"], res
# 6) mock path still works
mres = run.MockLLM(cfg, False).complete(run.Run({"id":"mock"},"01-pong","meno",1), [{"role":"user","content":"P"}])
assert mres.refused is False
print("ALL OK")
