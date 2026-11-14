"""동결 일관성, 프레임 덤프 없음, 회차 흐름(문맥 창 초과·거부·형식 오류·잘림), 끊긴 회차, 무입력 실패를 확인한다.
LLM 대신 정해 둔 답을 차례로 내는 가짜 백엔드를 쓴다."""
import json
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path
from types import SimpleNamespace as NS

H = Path(__file__).resolve().parents[1] / "harness"; sys.path.insert(0, str(H))
import freeze
import llm
import records
import run

cfg = tomllib.loads((H.parent / "experiment.toml").read_text(encoding="utf-8"))
CTX = {"inputs_sha256": "x", "frozen": False, "environment": {}}
MENO, SFML = "46f8934f0f7754a7e09033c4413b6a5222d4493d", "8835b6b955c96fa1ad1278a8dae6f05c4143effc"

# 동결 검사: 템플릿 해시, 태그 파싱
tags = freeze.template_tags(cfg)
assert tags["meno"] == {"meno": MENO, "SFML": SFML} and tags["sfml"] == {"SFML": SFML}, tags
probs = freeze.consistency_problems()
assert not [p for p in probs if "커밋" in p or "GIT_TAG" in p or "해시" in p or "backend" in p or "effort" in p], probs
bad = 'FetchContent_Declare(\n SFML\n GIT_REPOSITORY x\n GIT_TAG 3.1.0\n GIT_SHALLOW ON\n)'
assert freeze.declared_tags(bad) == {"SFML": "3.1.0"} and not freeze.COMMIT.match("3.1.0")
assert "requirements.txt" not in freeze.FROZEN

# 프레임 덤프가 없어도 통계가 멈추지 않음
d = Path(tempfile.mkdtemp()); (d / "result.json").write_text('{"exit_code": 1}')
p = subprocess.run([sys.executable, "-I", str(H / "frametime_stats.py"), str(d)], capture_output=True, text=True)
assert p.returncode == 0, p.stderr
assert json.loads((d / "result.json").read_text()) == {"exit_code": 1, "frames": 0, "frametime_dump": False}


class Scripted:
    """정해 둔 답(또는 예외)을 차례로 낸다."""
    def __init__(self, *items):
        self.items, self.seen = list(items), []

    def complete(self, run_, messages):
        self.seen.append(messages[-1]["content"])
        item = self.items.pop(0)
        if isinstance(item, BaseException):
            raise item
        return item


def reply(text, **kw):
    return llm.Reply(content=text, model="m", prompt_tokens=100, completion_tokens=50, **kw)


GOOD, BAD = "```cpp\nint main(){}\n```", "```cpp\nint main(\n```"
run.prepare_workspace = lambda c, n: Path(tempfile.mkdtemp())


def fake_build(c, work, code):
    if "main(\n" in code:
        return False, "main.cpp:1: error: expected ')'", 0.1
    (work / "build").mkdir(exist_ok=True); (work / "build" / "game").write_bytes(b"x")
    return True, "", 0.1


run.build = fake_build
out = Path(tempfile.mkdtemp())
M = {"id": "codex-cli/gpt-x", "tier": "other-vendor", "backend": "codex-cli", "model": "gpt-x", "effort": "medium"}
R = lambda game, cond, rep: records.Run(M, game, cond, rep)

# 문맥 창 초과는 둘째 왕복에서 회차 실패로 기록 (하니스는 계속)
fake = Scripted(reply(BAD), llm.RunFailure("context_window", "prompt is too long"))
res = run.run_once(cfg, R("02-snake", "sfml", 1), fake, out, True, CTX)
assert res["failure"] == "context_window" and res["rounds"] == 2 and res["rounds_detail"][1]["api_failure"] == "context_window", res
assert "too long" in res["failure_detail"] and res["attempt"] == 1
assert "expected ')'" in fake.seen[1]  # 둘째 왕복에는 빌드 오류 피드백이 감

# 공급자 거부는 바로 실패
res = run.run_once(cfg, R("02-snake", "sfml", 2), Scripted(reply("", finish_reason="refusal", refused=True)), out, True, CTX)
assert res["failure"] == "refused" and res["rounds"] == 1 and res["rounds_detail"][0]["refused"], res

# 코드 없는 답은 형식 오류 안내, 잘린 답은 잘림 안내를 보내고 왕복으로 셈
fake = Scripted(reply("설명만"), reply("```cpp\nint", finish_reason="length"), reply(GOOD))
res = run.run_once(cfg, R("02-snake", "meno", 1), fake, out, True, CTX)
assert res["build_success"] and res["rounds"] == 3 and res["truncated_rounds"] == 1, res
assert fake.seen[1] == run.prompt_file("format_error.md") and fake.seen[2] == run.prompt_file("truncated.md")

# 다섯 왕복 모두 코드 없음 → rounds_exhausted
res = run.run_once(cfg, R("02-snake", "meno", 2), Scripted(*[reply("x")] * cfg["experiment"]["max_rounds"]), out, True, CTX)
assert res["failure"] == "rounds_exhausted" and res["rounds"] == cfg["experiment"]["max_rounds"]

# 그 밖의 예외는 하니스를 멈추고(기록 없음), 폴더가 남는다
try:
    run.run_once(cfg, R("02-snake", "sfml", 3), Scripted(RuntimeError("codex 오류가 계속됨")), out, True, CTX)
    raise AssertionError("멈춰야 함")
except RuntimeError:
    pass
assert (out / R("02-snake", "sfml", 3).run_id / "prompt.md").exists()

# 이어서 돌리면 끊긴 폴더를 옮기고 attempt 2로 기록. 무입력 실행이 실패해도 기록하고 계속
real_run = subprocess.run
def fake_sub(cmd, **kw):
    if "run_noinput.sh" in cmd[0]:
        return NS(returncode=2, stderr="Xvfb failed")
    return real_run(cmd, **kw)
run.subprocess.run = fake_sub
res = run.run_once(cfg, R("02-snake", "sfml", 3), Scripted(reply(GOOD)), out, False, CTX)
run_id = R("02-snake", "sfml", 3).run_id
assert res["attempt"] == 2 and res["interrupted_attempts"] == [f"{run_id}.interrupted-1"], res
assert (out / f"{run_id}.interrupted-1" / "prompt.md").exists()
assert res["build_success"] and res["failure"] is None and "Xvfb failed" in res["noinput"]["harness_error"][0], res

# 기록 형식: 버전·커밋, 뺀 필드(LiteLLM 시절)는 없음
assert res["schema_version"] == records.SCHEMA_VERSION == 2
assert res["versions"]["meno_commit"] == MENO and res["versions"]["sfml_commit"] == res["versions"]["sfml_commit_meno_condition"] == SFML
for gone in ("cost", "model_params", "prompt_cache"):
    assert gone not in res, gone
assert "litellm" not in res["versions"]
json.dumps(res)
print("ALL OK")
