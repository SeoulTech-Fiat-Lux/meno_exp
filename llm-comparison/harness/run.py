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
sys.path.insert(0, str(Path(__file__).resolve().parent))  # python -I로 실행해도 freeze.py를 찾게
import freeze  # noqa: E402


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


def extract_code(text: str) -> tuple[str | None, int]:
    """닫힌 cpp 코드 블록 중 가장 긴 것을 main.cpp로 본다. 바뀐 부분만 보여 주는 짧은 블록이 앞에 있어도
    전체 파일을 고르게 하기 위함이다. (코드, 블록 수)를 돌려준다."""
    blocks = CODE_BLOCK.findall(text or "")
    return (max(blocks, key=len) if blocks else None), len(blocks)


@dataclass
class Reply:
    content: str
    model: str
    prompt_tokens: int | None = None
    completion_tokens: int | None = None
    cached_tokens: int | None = None          # 캐시에서 읽은 입력 토큰 (비용 계산용)
    cache_write_tokens: int | None = None     # 캐시에 새로 쓴 입력 토큰 (Anthropic)
    reasoning_tokens: int | None = None       # 출력 토큰 중 추론 토큰 (추론 모델만)
    cost: float | None = None
    finish_reason: str | None = None
    refused: bool = False                     # 공급자가 거부라고 알려 준 답
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


REFUSAL_FINISH = {"content_filter", "refusal"}  # 공급자가 거부를 알리는 종료 사유 (OpenAI, Anthropic)


def wants_cache_control(model_id: str) -> bool:
    """명시적 캐시 지점이 있어야 캐시가 걸리는 모델(Claude 계열)인지. OpenAI·Gemini는 자동으로 캐시한다."""
    return "claude" in model_id.lower() or model_id.lower().startswith("anthropic/")


def with_cache_breakpoint(messages: list[dict]) -> list[dict]:
    """첫 user 메시지(지시문 + 문맥 + 템플릿 + 명세) 끝에 캐시 지점을 둔다. 왕복마다 이 앞부분이 같으므로
    두 번째 왕복부터 캐시에서 읽는다. 캐시는 답에 영향을 주지 않고 비용과 응답 시간만 줄인다."""
    first, rest = messages[0], messages[1:]
    block = {"type": "text", "text": first["content"], "cache_control": {"type": "ephemeral"}}
    return [{"role": first["role"], "content": [block]}, *rest]


def _usage_field(obj, *names):
    """LiteLLM usage 객체·dict 어느 쪽이든 중첩 필드를 꺼낸다."""
    for name in names:
        if obj is None:
            return None
        obj = obj.get(name) if isinstance(obj, dict) else getattr(obj, name, None)
    return obj


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
        # 출력 길이 상한은 공급자 기본값에 맡기지 않고 모든 모델에 같은 값을 준다. 모델 params가 있으면 그것이 우선.
        params = {"max_tokens": exp["max_output_tokens"]} | run.model.get("params", {})
        if exp.get("prompt_cache", False) and wants_cache_control(run.model["id"]):
            messages = with_cache_breakpoint(messages)
        for attempt in range(exp["api_retries"] + 1):
            start = time.monotonic()
            try:
                response = self.litellm.completion(model=run.model["id"], messages=messages,
                                                   timeout=exp["llm_timeout_seconds"], **params)
                break
            except retryable as error:  # API 오류는 왕복으로 세지 않고 다시 시도
                if attempt == exp["api_retries"]:
                    raise
                wait = min(300, 10 * 2 ** attempt)
                print(f"  API 오류, {wait}초 뒤 다시 시도: {type(error).__name__}", flush=True)
                time.sleep(wait)
        latency = time.monotonic() - start
        usage = getattr(response, "usage", None)
        try:
            cost = self.litellm.completion_cost(completion_response=response)
        except Exception:  # 가격표에 없는 모델
            cost = None
        choice = response.choices[0]
        finish = getattr(choice, "finish_reason", None)
        # 거부는 공급자가 알려 준 신호로만 판별한다: 종료 사유, 또는 OpenAI의 message.refusal.
        # 설명만 있고 코드가 없는 답은 거부로 보지 않고 형식 오류로 다룬다 (사람의 판단이 섞이지 않게).
        refusal_text = getattr(choice.message, "refusal", None)
        return Reply(content=choice.message.content or "", model=getattr(response, "model", ""),
                     prompt_tokens=_usage_field(usage, "prompt_tokens"),
                     completion_tokens=_usage_field(usage, "completion_tokens"),
                     cached_tokens=_usage_field(usage, "prompt_tokens_details", "cached_tokens"),
                     cache_write_tokens=_usage_field(usage, "cache_creation_input_tokens"),
                     reasoning_tokens=_usage_field(usage, "completion_tokens_details", "reasoning_tokens"),
                     cost=cost, finish_reason=finish, refused=finish in REFUSAL_FINISH or bool(refusal_text),
                     latency_s=latency, extra={"refusal": refusal_text} if refusal_text else {})


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


def _first_line(command: list[str]) -> str | None:
    try:
        return subprocess.run(command, capture_output=True, text=True, check=True).stdout.splitlines()[0].strip()
    except Exception:
        return None


def environment() -> dict:
    """프레임 시간과 빌드 결과를 해석할 때 필요한 실행 환경. 소프트웨어 렌더링(llvmpipe)이라 CPU와 Mesa 버전이 중요하다."""
    os_name = None
    if Path("/etc/os-release").exists():
        for line in Path("/etc/os-release").read_text().splitlines():
            if line.startswith("PRETTY_NAME="):
                os_name = line.split("=", 1)[1].strip('"')
    cpu = None
    if Path("/proc/cpuinfo").exists():
        cpu = next((l.split(":", 1)[1].strip() for l in Path("/proc/cpuinfo").read_text().splitlines()
                    if l.startswith("model name")), None)
    return {"os": os_name, "kernel": platform.release(), "cpu": cpu, "cpus": os.cpu_count(),
            "gcc": _first_line(["gcc", "--version"]), "cmake": _first_line(["cmake", "--version"]),
            "mesa": _first_line(["dpkg-query", "-W", "-f=${Version}", "libgl1-mesa-dri"]),
            "wsl": "microsoft" in platform.release().lower()}


def run_once(cfg: dict, run: Run, llm, out_dir: Path, skip_noinput: bool, seed: int, context: dict) -> dict:
    exp = cfg["experiment"]
    run_dir = out_dir / run.run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    work = prepare_workspace(cfg, run.condition)
    prompt, meta = build_prompt(cfg, run.condition, run.game)
    (run_dir / "prompt.md").write_text(prompt, encoding="utf-8")

    messages = [{"role": "user", "content": prompt}]
    rounds: list[dict] = []
    started = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    success, code, failure = False, None, None
    for number in range(1, exp["max_rounds"] + 1):
        reply = llm.complete(run, messages)
        (run_dir / f"response_{number}.md").write_text(reply.content, encoding="utf-8")
        messages.append({"role": "assistant", "content": reply.content})
        code, blocks = extract_code(reply.content)
        truncated = reply.finish_reason == "length"
        record = {"round": number, "model_served": reply.model, "prompt_tokens": reply.prompt_tokens,
                  "completion_tokens": reply.completion_tokens, "reasoning_tokens": reply.reasoning_tokens,
                  "cached_tokens": reply.cached_tokens, "cache_write_tokens": reply.cache_write_tokens,
                  "cost": reply.cost, "finish_reason": reply.finish_reason, "truncated": truncated,
                  "refused": reply.refused,
                  "latency_s": round(reply.latency_s, 2), "code_found": code is not None, "code_blocks": blocks}
        if reply.extra.get("refusal"):
            record["refusal_text"] = reply.extra["refusal"]
        if reply.refused:
            # 공급자가 거부를 알리면 바로 실패로 끝낸다. 같은 요청을 다시 보내도 대개 또 거부해 왕복만 쓴다.
            record["build_ok"] = False
            rounds.append(record)
            failure = "refused"
            print(f"  왕복 {number}: 공급자 거부 ({reply.finish_reason}) → 실패", flush=True)
            break
        if code is None:
            record["build_ok"] = False
            rounds.append(record)
            # 길이 제한으로 잘렸으면 그 사실을 알려 준다. 형식 오류와 구분해 기록한다.
            notice = "truncated.md" if truncated else "format_error.md"
            messages.append({"role": "user", "content": (BASE / "prompts" / notice).read_text(encoding="utf-8")})
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
    if not success and failure is None:
        failure = "rounds_exhausted"

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
        "rounds": len(rounds), "build_success": success, "failure": failure,
        # 비교 지표는 캐시와 무관한 원래 토큰 수(prompt_tokens, completion_tokens)다. 캐시 토큰은 비용 설명용.
        "prompt_tokens": total("prompt_tokens"), "completion_tokens": total("completion_tokens"),
        "reasoning_tokens": total("reasoning_tokens"),
        "cached_tokens": total("cached_tokens"), "cache_write_tokens": total("cache_write_tokens"),
        "prompt_cache": bool(exp.get("prompt_cache", False) and wants_cache_control(run.model["id"])),
        "cost": total("cost"),
        "llm_latency_s": total("latency_s"), "code_lines": code_lines,
        "truncated_rounds": sum(1 for r in rounds if r.get("truncated")),
        "context_bytes": meta["context_bytes"], "hashes": meta["hashes"],
        "inputs_sha256": context["inputs_sha256"], "frozen": context["frozen"],
        "max_output_tokens": cfg["experiment"]["max_output_tokens"],
        "versions": template_versions(cfg) | {"python": platform.python_version(),
                                              "litellm": _litellm_version()},
        "environment": context["environment"],
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
    parser.add_argument("--out", help="결과 폴더 (기본: 본 실행은 runs/main, 그 밖에는 runs/<게임 묶음>-<입력 해시>)")
    args = parser.parse_args()
    main_run = args.games == "main" and not args.mock

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

    # 입력(설정, 지시문, 템플릿, 명세, 문맥, 에셋, 채점표, 하니스)의 해시. 본 실행은 동결 잠금과 같아야 시작한다.
    inputs = freeze.inputs_hash()
    lock = json.loads(freeze.LOCK.read_text(encoding="utf-8")) if freeze.LOCK.exists() else None
    frozen = lock is not None and lock["inputs_sha256"] == inputs
    if main_run:
        if Path(args.config).resolve() != (BASE / "experiment.toml").resolve():
            sys.exit("본 실행은 experiment.toml로만 돌린다.")
        if not frozen:
            freeze.check()
            sys.exit("본 실행은 동결 상태에서만 시작한다. harness/freeze.py write로 동결하거나 바뀐 입력을 되돌린다.")

    runs = plan_runs(cfg, games, conditions, models, reps, seed)
    # 시범 실행은 입력 해시별로 폴더를 나눈다. 명세나 지시문을 고친 뒤 다시 돌리면 새 폴더에 기록되므로,
    # 이전 결과 때문에 회차를 건너뛰지 않고, 서로 다른 입력의 결과가 한 기록에 섞이지 않는다.
    label = args.games if args.games in cfg["games"] else "custom"
    if args.out:
        out_dir = Path(args.out)
    elif main_run:
        out_dir = BASE / "runs" / "main"
    else:
        out_dir = BASE / ("runs-mock" if args.mock else "runs") / f"{label}-{inputs[:8]}"
    log_path = out_dir / "runs.jsonl"
    records = []
    if log_path.exists():
        records = [json.loads(line) for line in log_path.read_text().splitlines() if line.strip()]
    if any(r.get("inputs_sha256") != inputs for r in records):
        sys.exit(f"{out_dir}에 다른 입력으로 돌린 회차가 있다. 입력을 되돌리거나 다른 폴더(--out)를 쓴다.")
    done = {r["run_id"] for r in records}
    pending = [r for r in runs if r.run_id not in done]
    print(f"회차 {len(runs)}개 중 남은 것 {len(pending)}개 → {out_dir}")
    if args.dry_run:
        for run in pending:
            print(f"  {run.order:3} {run.run_id}")
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    llm = MockLLM(cfg, args.mock_fail_first) if args.mock else LiteLLM(cfg)
    context = {"inputs_sha256": inputs, "frozen": frozen, "environment": environment()}
    for run in pending:
        print(f"[{run.order + 1}/{len(runs)}] {run.run_id}", flush=True)
        result = run_once(cfg, run, llm, out_dir, args.skip_noinput, seed, context)
        with log_path.open("a", encoding="utf-8") as log:
            log.write(json.dumps(result, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
