"""LLM 생성 비교 실험 하니스 (SeoulTech-Fiat-Lux/Meno#29).

회차 = (모델, 게임, 조건, 반복 번호). 회차마다 LLM에게 main.cpp를 받아 빌드하고, 실패하면 오류를 돌려주며
최대 max_rounds번 시도한다. 빌드되면 무입력 실행·녹화·프레임 시간 측정을 하고, 결과를 JSONL 한 줄로 남긴다.

사용 (llm-comparison/ 에서):
    python3 harness/run.py --games pilot --dry-run          # 실행할 회차 목록만 출력
    python3 harness/run.py --games pilot --mock --reps 1    # API 없이 흐름 점검 (runs-mock/)
    python3 harness/run.py --games pilot                    # 시범 실행 (runs/)
    python3 harness/run.py --games main                     # 본 실행

API 키는 llm-comparison/.env 또는 환경 변수로 준다 (예: ANTHROPIC_API_KEY=...). .env는 커밋하지 않는다.
이미 runs.jsonl에 있는 회차는 건너뛰므로, 중단돼도 같은 명령으로 이어서 돌리면 된다.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import platform
import random
import re
import shutil
import subprocess
import sys
import time
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent  # llm-comparison/


# ---------------------------------------------------------------------------
# 준비
# ---------------------------------------------------------------------------
def sha256(data: str | bytes) -> str:
    if isinstance(data, str):
        data = data.encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def load_dotenv(path: Path) -> None:
    if not path.exists():
        return
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            os.environ.setdefault(key.strip(), value.strip().strip('"').strip("'"))


def render(template: str, **values: str) -> str:
    for key, value in values.items():
        template = template.replace("{{" + key + "}}", value)
    return template


def slug(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9.-]+", "_", text).strip("_")


@dataclass
class Run:
    model: dict
    game: str
    condition: str
    rep: int
    order: int = 0

    @property
    def run_id(self) -> str:
        return f"{self.game}-{self.condition}-{slug(self.model['id'])}-r{self.rep}"


def plan_runs(cfg: dict, games: list[str], conditions: list[str], models: list[dict], reps: int,
              seed: int) -> list[Run]:
    runs = [Run(m, g, c, r) for m in models for g in games for c in conditions for r in range(1, reps + 1)]
    random.Random(seed).shuffle(runs)  # 시간대별 API 변동이 한 조건에 몰리지 않게
    for index, run in enumerate(runs):
        run.order = index
    return runs


def build_prompt(cfg: dict, condition: str, game: str) -> tuple[str, dict]:
    cond = cfg["conditions"][condition]
    context_path = BASE / cond["context"]
    if not context_path.exists():
        sys.exit(f"문맥 묶음이 없음: {context_path}\ncontexts/build_context.py로 먼저 만든다 (README의 동결 절차 참고).")
    context = context_path.read_text(encoding="utf-8")
    template = (BASE / cond["template_dir"] / "CMakeLists.txt").read_text(encoding="utf-8")
    spec = (BASE / "specs" / "games" / f"{game}.md").read_text(encoding="utf-8")
    prompt = render((BASE / "prompts" / "instruction.md").read_text(encoding="utf-8"),
                    LIBRARY=cond["library"], RULE=cond["rule"], CONTEXT=context.rstrip(),
                    TEMPLATE=template.rstrip(), SPEC=spec.rstrip())
    hashes = {"context": sha256(context), "template": sha256(template), "spec": sha256(spec),
              "prompt": sha256(prompt)}
    return prompt, {"hashes": hashes, "context_bytes": len(context.encode("utf-8"))}


# ---------------------------------------------------------------------------
# 빌드
# ---------------------------------------------------------------------------
def prepare_workspace(cfg: dict, condition: str) -> Path:
    """조건마다 한 번 configure해 두고, 회차마다 main.cpp만 바꿔 다시 빌드한다."""
    cond = cfg["conditions"][condition]
    template = BASE / cond["template_dir"] / "CMakeLists.txt"
    work = BASE / "work" / condition
    work.mkdir(parents=True, exist_ok=True)
    stamp = work / ".template_sha256"
    template_hash = sha256(template.read_bytes())
    if stamp.exists() and stamp.read_text() == template_hash and (work / "build" / "CMakeCache.txt").exists():
        return work
    shutil.copy(template, work / "CMakeLists.txt")
    shutil.copy(BASE / cond["mock_source"], work / "main.cpp")  # configure용 자리 채움
    print(f"[{condition}] configure (처음 한 번, SFML 빌드 포함 수 분)", flush=True)
    subprocess.run(["cmake", "-S", str(work), "-B", str(work / "build"),
                    f"-DMINIGAME_ASSETS_DIR={BASE / 'assets'}"], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["cmake", "--build", str(work / "build"), "--target", "game",
                    "-j", str(cfg["experiment"]["build_jobs"])], check=True, stdout=subprocess.DEVNULL)
    stamp.write_text(template_hash)
    return work


NOISE = re.compile(r"^(\[\s*\d+%\]|gmake|make\[|Consolidate compiler|Built target|-- )")


def build(cfg: dict, work: Path, code: str) -> tuple[bool, str, float]:
    (work / "main.cpp").write_text(code, encoding="utf-8")
    game = work / "build" / "game"
    if game.exists():
        game.unlink()  # 실패한 빌드가 이전 실행 파일을 남기지 않게
    start = time.monotonic()
    proc = subprocess.run(["cmake", "--build", str(work / "build"), "--target", "game",
                           "-j", str(cfg["experiment"]["build_jobs"])],
                          capture_output=True, text=True)
    elapsed = time.monotonic() - start
    log = proc.stdout + proc.stderr
    return proc.returncode == 0 and game.exists(), log, elapsed


def error_excerpt(log: str, lines: int, work: Path | None = None) -> str:
    """컴파일러 출력에서 진행 표시를 빼고 앞부분만 남긴다. 작업 폴더 경로는 지워 컴퓨터마다 같은 문구가 되게 한다."""
    if work is not None:
        log = log.replace(str(work.resolve()) + "/", "").replace(str(work) + "/", "")
    kept = [line for line in log.splitlines() if line.strip() and not NOISE.match(line)]
    return "\n".join(kept[:lines])


# ---------------------------------------------------------------------------
# LLM
# ---------------------------------------------------------------------------
CODE_BLOCK = re.compile(r"```(?:cpp|c\+\+|cc|cxx)[^\n]*\n(.*?)```", re.S)


def extract_code(text: str) -> str | None:
    match = CODE_BLOCK.search(text or "")
    return match.group(1) if match else None


@dataclass
class Reply:
    content: str
    model: str
    prompt_tokens: int | None = None
    completion_tokens: int | None = None
    cached_tokens: int | None = None
    cost: float | None = None
    finish_reason: str | None = None
    latency_s: float = 0.0
    extra: dict = field(default_factory=dict)


class MockLLM:
    """API 없이 흐름을 점검한다. smoke 프로그램을 답으로 내고, fail_first면 첫 답에 컴파일 오류를 넣는다."""

    def __init__(self, cfg: dict, fail_first: bool):
        self.cfg, self.fail_first = cfg, fail_first

    def complete(self, run: Run, messages: list[dict]) -> Reply:
        code = (BASE / self.cfg["conditions"][run.condition]["mock_source"]).read_text(encoding="utf-8")
        if self.fail_first and len(messages) == 1:
            code = code.replace("int main()", "int main( // 일부러 넣은 오류", 1)
        words = sum(len(m["content"]) for m in messages) // 4
        return Reply(content=f"```cpp\n{code}```", model="mock", prompt_tokens=words,
                     completion_tokens=len(code) // 4, finish_reason="stop")


class LiteLLM:
    def __init__(self, cfg: dict):
        import litellm  # 실제 호출할 때만 필요
        self.litellm, self.cfg = litellm, cfg
        litellm.drop_params = True

    def complete(self, run: Run, messages: list[dict]) -> Reply:
        exp = self.cfg["experiment"]
        retryable = tuple(getattr(self.litellm.exceptions, name) for name in (
            "RateLimitError", "Timeout", "APIConnectionError", "ServiceUnavailableError",
            "InternalServerError") if hasattr(self.litellm.exceptions, name))
        for attempt in range(exp["api_retries"] + 1):
            start = time.monotonic()
            try:
                response = self.litellm.completion(model=run.model["id"], messages=messages,
                                                   timeout=exp["llm_timeout_seconds"],
                                                   **run.model.get("params", {}))
                break
            except retryable as error:  # API 오류는 왕복으로 세지 않고 다시 시도
                if attempt == exp["api_retries"]:
                    raise
                wait = min(300, 10 * 2 ** attempt)
                print(f"  API 오류, {wait}초 뒤 다시 시도: {type(error).__name__}", flush=True)
                time.sleep(wait)
        latency = time.monotonic() - start
        usage = getattr(response, "usage", None)
        details = getattr(usage, "prompt_tokens_details", None) if usage else None
        try:
            cost = self.litellm.completion_cost(completion_response=response)
        except Exception:  # 가격표에 없는 모델
            cost = None
        choice = response.choices[0]
        return Reply(content=choice.message.content or "", model=getattr(response, "model", ""),
                     prompt_tokens=getattr(usage, "prompt_tokens", None),
                     completion_tokens=getattr(usage, "completion_tokens", None),
                     cached_tokens=getattr(details, "cached_tokens", None) if details else None,
                     cost=cost, finish_reason=getattr(choice, "finish_reason", None), latency_s=latency)


# ---------------------------------------------------------------------------
# 회차 실행
# ---------------------------------------------------------------------------
def template_versions(cfg: dict) -> dict:
    meno = (BASE / cfg["conditions"]["meno"]["template_dir"] / "CMakeLists.txt").read_text(encoding="utf-8")
    sfml = (BASE / cfg["conditions"]["sfml"]["template_dir"] / "CMakeLists.txt").read_text(encoding="utf-8")
    meno_tag = re.search(r"GIT_TAG\s+(\S+)", meno)
    sfml_tag = re.search(r"GIT_TAG\s+(\S+)", sfml)
    return {"meno_commit": meno_tag.group(1) if meno_tag else None,
            "sfml_tag": sfml_tag.group(1) if sfml_tag else None}


def run_once(cfg: dict, run: Run, llm, out_dir: Path, skip_noinput: bool, seed: int) -> dict:
    exp = cfg["experiment"]
    run_dir = out_dir / run.run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    work = prepare_workspace(cfg, run.condition)
    prompt, meta = build_prompt(cfg, run.condition, run.game)
    (run_dir / "prompt.md").write_text(prompt, encoding="utf-8")

    messages = [{"role": "user", "content": prompt}]
    rounds: list[dict] = []
    started = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    success, code = False, None
    for number in range(1, exp["max_rounds"] + 1):
        reply = llm.complete(run, messages)
        (run_dir / f"response_{number}.md").write_text(reply.content, encoding="utf-8")
        messages.append({"role": "assistant", "content": reply.content})
        code = extract_code(reply.content)
        record = {"round": number, "model_served": reply.model, "prompt_tokens": reply.prompt_tokens,
                  "completion_tokens": reply.completion_tokens, "cached_tokens": reply.cached_tokens,
                  "cost": reply.cost, "finish_reason": reply.finish_reason,
                  "latency_s": round(reply.latency_s, 2), "code_found": code is not None}
        if code is None:
            record["build_ok"] = False
            rounds.append(record)
            messages.append({"role": "user",
                             "content": (BASE / "prompts" / "format_error.md").read_text(encoding="utf-8")})
            continue
        (run_dir / f"main_{number}.cpp").write_text(code, encoding="utf-8")
        ok, log, build_s = build(cfg, work, code)
        (run_dir / f"build_{number}.log").write_text(log, encoding="utf-8")
        record |= {"build_ok": ok, "build_s": round(build_s, 2)}
        rounds.append(record)
        print(f"  왕복 {number}: 빌드 {'성공' if ok else '실패'}", flush=True)
        if ok:
            success = True
            break
        feedback = render((BASE / "prompts" / "build_error.md").read_text(encoding="utf-8"),
                          ERRORS=error_excerpt(log, exp["error_feedback_lines"], work))
        (run_dir / f"feedback_{number}.md").write_text(feedback, encoding="utf-8")
        messages.append({"role": "user", "content": feedback})

    noinput = None
    if success:
        (run_dir / "main.cpp").write_text(code, encoding="utf-8")
        shutil.copy(work / "build" / "game", run_dir / "game")  # 채점용. git에는 넣지 않는다
        if not skip_noinput:
            subprocess.run([str(BASE / "harness" / "run_noinput.sh"), str(work / "build"),
                            str(run_dir / "noinput"), str(exp["noinput_seconds"])],
                           check=True, stdout=subprocess.DEVNULL)
            subprocess.run([sys.executable, str(BASE / "harness" / "frametime_stats.py"),
                            str(run_dir / "noinput")], check=True, stdout=subprocess.DEVNULL)
            noinput = json.loads((run_dir / "noinput" / "result.json").read_text())

    def total(key: str) -> int | float | None:
        values = [r.get(key) for r in rounds if r.get(key) is not None]
        return sum(values) if values else None

    code_lines = sum(1 for line in (code or "").splitlines() if line.strip()) if success else None
    return {
        "run_id": run.run_id, "started_at": started,
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "order": run.order, "seed": seed,
        "model_requested": run.model["id"], "model_tier": run.model.get("tier"),
        "model_params": run.model.get("params", {}),
        "models_served": sorted({r["model_served"] for r in rounds if r.get("model_served")}),
        "game": run.game, "condition": run.condition, "rep": run.rep,
        "rounds": len(rounds), "build_success": success,
        "prompt_tokens": total("prompt_tokens"), "completion_tokens": total("completion_tokens"),
        "cached_tokens": total("cached_tokens"), "cost": total("cost"),
        "llm_latency_s": total("latency_s"), "code_lines": code_lines,
        "context_bytes": meta["context_bytes"], "hashes": meta["hashes"],
        "versions": template_versions(cfg) | {"python": platform.python_version(),
                                              "litellm": _litellm_version()},
        "noinput": noinput, "rounds_detail": rounds,
    }


def _litellm_version() -> str | None:
    try:
        from importlib.metadata import version
        return version("litellm")
    except Exception:
        return None


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", default=str(BASE / "experiment.toml"))
    parser.add_argument("--games", default="pilot", help="pilot, main, 또는 쉼표로 나눈 게임 이름")
    parser.add_argument("--conditions", default="meno,sfml")
    parser.add_argument("--models", default="all", help="all 또는 쉼표로 나눈 tier/id")
    parser.add_argument("--reps", type=int, help="설정의 reps 대신 쓸 반복 횟수")
    parser.add_argument("--mock", action="store_true", help="API 없이 smoke 프로그램으로 흐름 점검")
    parser.add_argument("--mock-fail-first", action="store_true", help="mock의 첫 답에 컴파일 오류를 넣음")
    parser.add_argument("--skip-noinput", action="store_true", help="무입력 실행을 건너뜀")
    parser.add_argument("--dry-run", action="store_true", help="실행할 회차 목록만 출력")
    parser.add_argument("--out", help="결과 폴더 (기본: runs/, mock이면 runs-mock/)")
    args = parser.parse_args()

    cfg = tomllib.loads(Path(args.config).read_text(encoding="utf-8"))
    load_dotenv(BASE / ".env")
    exp = cfg["experiment"]
    games = cfg["games"].get(args.games, None) or args.games.split(",")
    conditions = args.conditions.split(",")
    models = cfg["models"]
    if args.mock:
        models = [{"tier": "mock", "id": "mock", "params": {}}]
    elif args.models != "all":
        wanted = set(args.models.split(","))
        models = [m for m in models if m["id"] in wanted or m.get("tier") in wanted]
    if not args.mock and any(m["id"].startswith("TBD") for m in models):
        sys.exit("experiment.toml의 모델 ID(TBD)를 먼저 정한다.")
    reps = args.reps or exp["reps"]
    seed = exp["seed"]

    runs = plan_runs(cfg, games, conditions, models, reps, seed)
    out_dir = Path(args.out) if args.out else BASE / ("runs-mock" if args.mock else "runs")
    out_dir.mkdir(parents=True, exist_ok=True)
    log_path = out_dir / "runs.jsonl"
    done = set()
    if log_path.exists():
        done = {json.loads(line)["run_id"] for line in log_path.read_text().splitlines() if line.strip()}
    pending = [r for r in runs if r.run_id not in done]
    print(f"회차 {len(runs)}개 중 남은 것 {len(pending)}개 → {out_dir}")
    if args.dry_run:
        for run in pending:
            print(f"  {run.order:3} {run.run_id}")
        return

    llm = MockLLM(cfg, args.mock_fail_first) if args.mock else LiteLLM(cfg)
    for run in pending:
        print(f"[{run.order + 1}/{len(runs)}] {run.run_id}", flush=True)
        result = run_once(cfg, run, llm, out_dir, args.skip_noinput, seed)
        with log_path.open("a", encoding="utf-8") as log:
            log.write(json.dumps(result, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
