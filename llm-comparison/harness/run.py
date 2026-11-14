"""LLM 생성 비교 실험 하니스 (SeoulTech-Fiat-Lux/Meno#29).

회차 = (모델, 게임, 조건, 반복 번호). 회차마다 LLM에게 main.cpp를 받아 빌드하고, 실패하면 오류를 돌려주며
최대 max_rounds번 시도한다. 빌드되면 무입력 실행·녹화·프레임 시간 측정을 하고, 결과를 JSONL 한 줄로 남긴다.

사용 (llm-comparison/ 에서):
    python3 harness/run.py --games pilot --dry-run          # 실행할 회차 목록만 출력
    python3 harness/run.py --games pilot --mock --reps 1    # API 없이 흐름 점검 (runs-mock/)
    python3 harness/run.py --models top --probe             # 구독 CLI 연결 확인 (짧은 2왕복, 기록 안 함)
    python3 harness/run.py --games pilot --models light     # 시범 실행 (runs/)
    python3 harness/run.py --games main --models top,light  # 본 실행: Claude 구독자
    python3 harness/run.py --games main --models other-vendor --shard 1/2   # 본 실행: ChatGPT 구독자 1
    python3 harness/run.py --games main --models other-vendor --shard 2/2   # 본 실행: ChatGPT 구독자 2

모델은 experiment.toml의 backend에 따라 구독 CLI(claude-cli, codex-cli) 또는 API(litellm)로 부른다.
기록은 사람마다 runs<-모델><-몫>.jsonl에 따로 쓴다. 이미 어느 기록 파일에든 있는 회차는 건너뛰므로,
중단돼도 같은 명령으로 이어서 돌리면 된다.
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
import tempfile
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


class RunFailure(Exception):
    """다시 보내도 같은 결과가 나오는 API 오류. 하니스를 멈추지 않고 그 회차를 실패로 기록한다.
    멈추면 이어서 돌릴 때 같은 회차를 LLM에게 새로 받게 되어, 실패한 표본이 다른 표본으로 바뀐다."""

    def __init__(self, reason: str, detail: str):
        super().__init__(f"{reason}: {detail}")
        self.reason, self.detail = reason, detail


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
        # 다시 보내도 같은 결과인 오류만 회차 실패로 기록한다. 그 밖의 오류(인증, 잘못된 모델 ID, 잘못된 인자)는
        # 모든 회차에 똑같이 생기는 설정 문제이므로 하니스를 멈춘다.
        recorded = [(getattr(self.litellm.exceptions, name), reason) for name, reason in (
            ("ContextWindowExceededError", "context_window"), ("ContentPolicyViolationError", "refused"))
            if hasattr(self.litellm.exceptions, name)]
        recorded_types = tuple(kind for kind, _ in recorded)
        for attempt in range(exp["api_retries"] + 1):
            start = time.monotonic()
            try:
                response = self.litellm.completion(model=run.model["id"], messages=messages,
                                                   timeout=exp["llm_timeout_seconds"], **params)
                break
            except recorded_types as error:
                reason = next(r for kind, r in recorded if isinstance(error, kind))
                raise RunFailure(reason, f"{type(error).__name__}: {str(error)[:500]}") from error
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
# 구독 CLI 백엔드 (Claude Code, Codex)
# ---------------------------------------------------------------------------
# API 키 대신 각자의 구독 로그인으로 모델을 부른다. 변수 통제를 위해 두 CLI 모두:
#   - 자체 시스템 프롬프트를 prompts/system.md로 바꾼다 (에이전트용 긴 지시문을 빼고 두 공급자에 같은 문장).
#   - 도구(파일 읽기·명령 실행·웹)를 끄고, 빈 임시 폴더에서 돌린다. 모델이 직접 컴파일해 보는 일이 없게.
#   - 사용자 설정(CLAUDE.md, AGENTS.md, 플러그인, MCP)을 읽지 않는다.
#   - 왕복은 CLI의 세션 이어 가기로 같은 대화에 붙인다.
LIMIT_ERROR = re.compile(r"usage limit|rate.?limit|limit reached|limit will reset|too many requests|\b429\b", re.I)
CONTEXT_ERROR = re.compile(r"prompt is too long|context.?(length|window)|maximum context|input is too long", re.I)
AUTH_ERROR = re.compile(r"authenticat|oauth|not logged in|log ?in again|unauthorized|\b401\b", re.I)


class UsageLimitReached(Exception):
    """구독 사용 한도에 걸려 기다리는 시간 상한을 넘김. 하니스를 멈추고 나중에 이어서 돌린다."""


def toml_string(value: str) -> str:
    return json.dumps(value)  # JSON 문자열은 TOML 기본 문자열로도 읽힌다 (codex -c 값)


class CliBackend:
    executable = ""
    client = ""

    def __init__(self, cfg: dict):
        self.cfg = cfg
        self.exe = shutil.which(self.executable)
        if not self.exe:
            sys.exit(f"{self.executable} 명령을 찾지 못함. 설치하고 구독 계정으로 로그인한다 (README).")
        self.version = _first_line([self.exe, "--version"])
        self.system_prompt = (BASE / "prompts" / "system.md").read_text(encoding="utf-8").strip()
        self.sessions: dict[str, tuple[str, str]] = {}  # 회차 ID → (작업 폴더, 마지막으로 성공한 세션 ID)

    def session(self, run: Run, messages: list[dict]) -> tuple[str, str | None, str]:
        """(작업 폴더, 이어 갈 세션 ID 또는 None, 이번에 보낼 user 메시지)."""
        if len(messages) == 1 or run.run_id not in self.sessions:
            # 회차마다 빈 폴더에서 시작한다. 프로젝트 지시문(CLAUDE.md, AGENTS.md)이나 소스 파일이 없게.
            return tempfile.mkdtemp(prefix=f"meno-exp-{run.run_id}-"), None, messages[0]["content"]
        cwd, session_id = self.sessions[run.run_id]
        return cwd, session_id, messages[-1]["content"]

    def error_text(self, proc: subprocess.CompletedProcess) -> str | None:
        raise NotImplementedError

    def call(self, command: list[str], prompt: str, cwd: str, env: dict | None = None):
        """CLI를 한 번 부른다. 프롬프트는 stdin으로 준다 (225KB라 명령줄 인자 한도를 넘음).
        돌려줌: (프로세스, 경과 초, 재시도 횟수). 문맥 창 초과는 RunFailure, 로그인 문제는 멈춤."""
        exp = self.cfg["experiment"]
        wait_limit = exp.get("limit_wait_minutes", 30) * 60
        give_up_at = time.monotonic() + exp.get("limit_wait_hours", 6) * 3600
        retries = 0
        while True:
            start = time.monotonic()
            try:
                proc = subprocess.run(command, input=prompt, capture_output=True, text=True, encoding="utf-8",
                                      cwd=cwd, env=env, timeout=exp["llm_timeout_seconds"])
                error = self.error_text(proc)
            except subprocess.TimeoutExpired:
                error = f"{self.executable} 응답 시간 초과 ({exp['llm_timeout_seconds']}초)"
                proc = None
            elapsed = time.monotonic() - start
            if error is None:
                return proc, elapsed, retries
            if CONTEXT_ERROR.search(error):
                raise RunFailure("context_window", error[:500])
            if AUTH_ERROR.search(error):
                sys.exit(f"{self.executable} 로그인 문제: {error[:300]}\n다시 로그인한 뒤 같은 명령으로 이어서 돌린다.")
            if LIMIT_ERROR.search(error):
                # 한도는 결과와 무관하다. 회차를 새로 받지 않도록 같은 왕복을 기다렸다가 다시 보낸다.
                if time.monotonic() + wait_limit > give_up_at:
                    raise UsageLimitReached(error[:300])
                print(f"  사용 한도: {wait_limit // 60}분 뒤 같은 왕복을 다시 보냄 ({error[:120]})", flush=True)
                time.sleep(wait_limit)
                retries += 1
                continue
            retries += 1
            if retries > exp["api_retries"]:
                raise RuntimeError(f"{self.executable} 오류가 계속됨: {error[:500]}")
            wait = min(300, 10 * 2 ** retries)
            print(f"  {self.executable} 오류, {wait}초 뒤 다시 시도: {error[:120]}", flush=True)
            time.sleep(wait)


class ClaudeCLI(CliBackend):
    """Claude Code의 print 모드. 구독(Pro·Max) 로그인을 쓴다."""
    executable = "claude"
    client = "claude-code"
    STOP = {"end_turn": "stop", "stop_sequence": "stop", "max_tokens": "length", "refusal": "refusal"}

    def error_text(self, proc):
        try:
            data = json.loads(proc.stdout)
        except json.JSONDecodeError:
            return (proc.stderr or proc.stdout or f"종료 코드 {proc.returncode}").strip()[-1000:]
        if data.get("is_error"):
            return f"{data.get('result', '')} (status {data.get('api_error_status')}, {data.get('terminal_reason')})"
        return None

    def complete(self, run: Run, messages: list[dict]) -> Reply:
        exp = self.cfg["experiment"]
        cwd, resume, prompt = self.session(run, messages)
        command = [self.exe, "-p", "--output-format", "json",
                   "--safe-mode",                  # CLAUDE.md, 스킬, 플러그인, 훅, MCP, 출력 스타일을 끔. 구독 로그인은 그대로
                   "--tools", "",                  # 도구 모두 끔
                   "--strict-mcp-config", "--disable-slash-commands",
                   "--system-prompt", self.system_prompt,
                   "--model", run.model["model"]]
        if run.model.get("effort"):
            command += ["--effort", run.model["effort"]]
        if resume:
            # 마지막으로 성공한 세션에서 갈라져 나온다. 실패한 호출을 다시 보내도 대화에 같은 메시지가 겹치지 않게.
            command += ["--resume", resume, "--fork-session"]
        env = os.environ | {"CLAUDE_CODE_MAX_OUTPUT_TOKENS": str(exp["max_output_tokens"])}
        env.pop("ANTHROPIC_API_KEY", None)  # 키가 있으면 구독 대신 API로 과금될 수 있다
        proc, elapsed, retries = self.call(command, prompt, cwd, env)
        data = json.loads(proc.stdout)
        self.sessions[run.run_id] = (cwd, data["session_id"])
        usage = data.get("usage", {})
        cache_read = usage.get("cache_read_input_tokens") or 0
        cache_write = usage.get("cache_creation_input_tokens") or 0
        stop = data.get("stop_reason")
        return Reply(
            content=data.get("result") or "",
            model=",".join(sorted(data.get("modelUsage", {}))) or run.model["model"],
            # Anthropic의 input_tokens는 캐시 밖의 토큰만 센다. 비교 지표는 캐시와 무관한 원래 입력 토큰이므로 더한다.
            prompt_tokens=(usage.get("input_tokens") or 0) + cache_read + cache_write,
            completion_tokens=usage.get("output_tokens"),
            cached_tokens=cache_read, cache_write_tokens=cache_write,
            reasoning_tokens=(usage.get("output_tokens_details") or {}).get("thinking_tokens"),
            cost=None,  # 구독이라 과금되지 않음. API로 샀다면 얼마였는지는 extra.cost_equiv_usd
            finish_reason=self.STOP.get(stop, stop), refused=stop == "refusal",
            latency_s=elapsed,
            extra={"client": self.client, "client_version": self.version, "session_id": data["session_id"],
                   "cost_equiv_usd": data.get("total_cost_usd"), "cli_retries": retries,
                   "num_turns": data.get("num_turns")})


class CodexCLI(CliBackend):
    """Codex CLI의 exec 모드. ChatGPT 구독 로그인을 쓴다."""
    executable = "codex"
    client = "codex-cli"
    TOOL_ITEMS = {"command_execution", "file_change", "mcp_tool_call", "web_search"}

    def __init__(self, cfg: dict):
        super().__init__(cfg)
        # 사용자 전역 지시문(AGENTS.md)은 --ignore-user-config로도 빠지지 않는다. 있으면 실험 동안 옮겨 두게 한다.
        home = Path(os.environ.get("CODEX_HOME", Path.home() / ".codex"))
        found = [p for p in (home / "AGENTS.override.md", home / "AGENTS.md") if p.exists() and p.read_text().strip()]
        if found:
            sys.exit(f"Codex 전역 지시문이 있음: {', '.join(map(str, found))}\n"
                     "모든 회차에 섞여 들어가므로, 실험 동안 다른 이름으로 옮겨 둔다.")
        self.instructions = Path(tempfile.mkdtemp(prefix="meno-exp-codex-")) / "system.md"
        self.instructions.write_text(self.system_prompt + "\n", encoding="utf-8")

    def flags(self, run: Run) -> list[str]:
        flags = ["--json", "--skip-git-repo-check", "--ignore-user-config", "--sandbox", "read-only",
                 "-m", run.model["model"],
                 "-c", "approval_policy=" + toml_string("never"),
                 "-c", "web_search=" + toml_string("disabled"),
                 "-c", "features.shell_tool=false",                       # 명령 실행 도구를 끔
                 "-c", "model_instructions_file=" + toml_string(str(self.instructions))]  # 기본 지시문 대체
        if run.model.get("effort"):
            flags += ["-c", "model_reasoning_effort=" + toml_string(run.model["effort"])]
        return flags

    @staticmethod
    def events(proc) -> list[dict]:
        events = []
        for line in (proc.stdout or "").splitlines():
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                continue
        return events

    def error_text(self, proc):
        errors = []
        for event in self.events(proc):
            if event.get("type") == "error":
                errors.append(str(event.get("message", event)))
            elif event.get("type") == "turn.failed":
                errors.append(str((event.get("error") or {}).get("message", event)))
        if errors:
            return " | ".join(errors)
        if proc.returncode != 0:
            return (proc.stderr or f"종료 코드 {proc.returncode}").strip()[-1000:]
        return None

    def complete(self, run: Run, messages: list[dict]) -> Reply:
        cwd, resume, prompt = self.session(run, messages)
        command = [self.exe, "exec", *self.flags(run)]
        if resume:
            command += ["resume", resume]
        command.append("-")  # 프롬프트는 stdin
        proc, elapsed, retries = self.call(command, prompt, cwd)
        events = self.events(proc)
        thread = next((e.get("thread_id") for e in events if e.get("type") == "thread.started"), None) or resume
        self.sessions[run.run_id] = (cwd, thread)
        items = [e.get("item", {}) for e in events if e.get("type") == "item.completed"]
        messages_out = [i.get("text", "") for i in items if i.get("type") == "agent_message"]
        usage = [e.get("usage", {}) for e in events if e.get("type") == "turn.completed"]

        def total(key: str) -> int | None:
            values = [u.get(key) for u in usage if u.get(key) is not None]
            return sum(values) if values else None

        return Reply(
            content=messages_out[-1] if messages_out else "",
            model=run.model["model"],  # Codex는 응답 모델 ID를 알려 주지 않는다
            prompt_tokens=total("input_tokens"), completion_tokens=total("output_tokens"),
            cached_tokens=total("cached_input_tokens"), reasoning_tokens=total("reasoning_output_tokens"),
            finish_reason=None,  # Codex는 종료 사유를 알려 주지 않는다. 잘린 답은 코드 블록이 닫히지 않아 형식 오류로 잡힘
            latency_s=elapsed,
            extra={"client": self.client, "client_version": self.version, "session_id": thread,
                   "cli_retries": retries,
                   "tool_calls": sum(1 for i in items if i.get("type") in self.TOOL_ITEMS)})


BACKENDS = {"litellm": LiteLLM, "claude-cli": ClaudeCLI, "codex-cli": CodexCLI}


def model_undecided(model: dict) -> bool:
    return "TBD" in model["id"] or model.get("model") == "TBD"


# ---------------------------------------------------------------------------
# 회차 실행
# ---------------------------------------------------------------------------
def template_versions(cfg: dict) -> dict:
    tags = freeze.template_tags(cfg)
    return {"meno_commit": tags["meno"].get("meno"), "sfml_commit": tags["sfml"].get("SFML"),
            "sfml_commit_meno_condition": tags["meno"].get("SFML")}


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
    backend = "mock" if run.model["id"] == "mock" else run.model.get("backend", "litellm")
    run_dir = out_dir / run.run_id
    # runs.jsonl에 없는데 폴더가 있으면 이전 시도가 중간에 끊긴 것이다 (Ctrl-C, 전원, 하니스 설정 오류, API 장애).
    # 결과와 무관한 원인으로 끊긴 것이므로 다시 받되, 이전 시도를 지우지 않고 옮겨 두고 몇 번째 시도인지 기록한다.
    interrupted = sorted(p.name for p in out_dir.glob(f"{run.run_id}.interrupted-*"))
    if run_dir.exists() and any(run_dir.iterdir()):
        target = out_dir / f"{run.run_id}.interrupted-{len(interrupted) + 1}"
        run_dir.rename(target)
        interrupted.append(target.name)
        print(f"  끊긴 이전 시도를 {target.name}로 옮기고 다시 시작", flush=True)
    attempt = len(interrupted) + 1
    run_dir.mkdir(parents=True, exist_ok=True)
    work = prepare_workspace(cfg, run.condition)
    prompt, meta = build_prompt(cfg, run.condition, run.game)
    (run_dir / "prompt.md").write_text(prompt, encoding="utf-8")

    messages = [{"role": "user", "content": prompt}]
    rounds: list[dict] = []
    started = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    success, code, failure = False, None, None
    failure_detail = None
    for number in range(1, exp["max_rounds"] + 1):
        try:
            reply = llm.complete(run, messages)
        except RunFailure as error:
            # 문맥 창 초과·정책 거부: 다시 보내도 같으므로 이 왕복을 실패로 남기고 회차를 끝낸다.
            rounds.append({"round": number, "build_ok": False, "api_failure": error.reason,
                           "refused": error.reason == "refused", "code_found": False})
            failure, failure_detail = error.reason, error.detail
            print(f"  왕복 {number}: {error.reason} → 실패", flush=True)
            break
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
        record |= {k: v for k, v in reply.extra.items() if k != "refusal"}  # CLI 버전, 세션, 재시도, 도구 호출 수 등
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
            # 무입력 실행이나 통계가 실패해도 하니스를 멈추지 않는다. 멈추면 이 회차를 LLM에게 새로 받게 된다.
            result_path = run_dir / "noinput" / "result.json"
            errors = []
            steps = [("run_noinput.sh", [str(BASE / "harness" / "run_noinput.sh"), str(work / "build"),
                                         str(run_dir / "noinput"), str(exp["noinput_seconds"])]),
                     ("frametime_stats.py", [sys.executable, str(BASE / "harness" / "frametime_stats.py"),
                                             str(run_dir / "noinput")])]
            for label, command in steps:
                proc = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
                if proc.returncode != 0:
                    errors.append(f"{label} 종료 코드 {proc.returncode}: {proc.stderr.strip()[-500:]}")
                    break
            noinput = json.loads(result_path.read_text()) if result_path.exists() else {}
            if errors:
                noinput["harness_error"] = errors

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
        "rounds": len(rounds), "build_success": success, "failure": failure, "failure_detail": failure_detail,
        "attempt": attempt, "interrupted_attempts": interrupted,
        # 비교 지표는 캐시와 무관한 원래 토큰 수(prompt_tokens, completion_tokens)다. 캐시 토큰은 비용 설명용.
        "prompt_tokens": total("prompt_tokens"), "completion_tokens": total("completion_tokens"),
        "reasoning_tokens": total("reasoning_tokens"),
        "cached_tokens": total("cached_tokens"), "cache_write_tokens": total("cache_write_tokens"),
        "backend": backend, "model_cli": run.model.get("model"), "effort": run.model.get("effort"),
        "operator": run.model.get("operator"),
        "client_versions": sorted({r["client_version"] for r in rounds if r.get("client_version")}),
        # 구독 CLI는 스스로 캐시한다. litellm은 설정에 따라 Claude 계열에만 캐시 지점을 둔다.
        "prompt_cache": True if backend != "litellm" else
        bool(exp.get("prompt_cache", False) and wants_cache_control(run.model["id"])),
        "cost": total("cost"), "cost_equiv_usd": total("cost_equiv_usd"),
        "tool_calls": total("tool_calls"), "cli_retries": total("cli_retries"),
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
    parser.add_argument("--shard", help="K/N: 게임을 N몫으로 나눠 K번째만 돌림 (같은 모델을 여러 사람이 나눌 때). "
                                        "한 게임의 두 조건·모든 반복은 같은 사람(같은 PC)에게 감")
    parser.add_argument("--probe", action="store_true",
                        help="고른 모델에 짧은 2왕복 대화를 보내 로그인·세션 이어 가기·토큰 기록·도구 꺼짐을 확인")
    parser.add_argument("--measure-context", action="store_true",
                        help="고른 모델로 조건별 문맥 묶음의 토큰 수를 재서 runs/context_tokens.jsonl에 남김 (모델마다 3호출)")
    args = parser.parse_args()
    main_run = args.games == "main" and not args.mock

    cfg = tomllib.loads(Path(args.config).read_text(encoding="utf-8"))
    load_dotenv(BASE / ".env")
    exp = cfg["experiment"]
    games = cfg["games"].get(args.games, None) or args.games.split(",")
    conditions = args.conditions.split(",")
    models = cfg["models"]
    if args.games == "pilot" and args.models == "all" and cfg["games"].get("pilot_models"):
        args.models = ",".join(cfg["games"]["pilot_models"])  # 시범 실행은 정해 둔 모델로만
    if args.mock:
        models = [{"tier": "mock", "id": "mock", "params": {}}]
    elif args.models != "all":
        wanted = set(args.models.split(","))
        models = [m for m in models if m["id"] in wanted or m.get("tier") in wanted]
    if not models:
        sys.exit(f"--models {args.models}에 맞는 모델이 없음")
    if not args.mock and not args.dry_run and any(model_undecided(m) for m in models):
        sys.exit("experiment.toml의 모델(TBD)을 먼저 정한다.")
    for model in models:
        if not args.mock and model.get("backend", "litellm") not in BACKENDS:
            sys.exit(f"알 수 없는 backend: {model.get('backend')} ({model['id']})")
    reps = args.reps or exp["reps"]
    seed = exp["seed"]

    backends: dict[str, object] = {}

    def llm_for(model: dict):
        if args.mock:
            return backends.setdefault("mock", MockLLM(cfg, args.mock_fail_first))
        kind = model.get("backend", "litellm")
        if kind not in backends:
            backends[kind] = BACKENDS[kind](cfg)
        return backends[kind]

    if args.probe:
        sys.exit(0 if all(probe(cfg, model, llm_for(model)) for model in models) else 1)
    if args.measure_context:
        for model in models:
            measure_context(cfg, model, llm_for(model), conditions)
        return

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
    shard_tag = ""
    if args.shard:
        k, n = (int(x) for x in args.shard.split("/"))
        if not 1 <= k <= n:
            sys.exit("--shard는 1/2, 2/2처럼 K/N으로 준다.")
        # 게임 단위로 나눈다. 같은 (모델, 게임)의 두 조건이 같은 PC에서 돌아야 빌드 시간·프레임 시간을 짝지어 비교할 수 있다.
        mine = {g for i, g in enumerate(sorted(set(games))) if i % n == k - 1}
        runs = [r for r in runs if r.game in mine]
        shard_tag = f"-{k}of{n}"
        print(f"shard {k}/{n}: 게임 {sorted(mine)}")
    # 시범 실행은 입력 해시별로 폴더를 나눈다. 명세나 지시문을 고친 뒤 다시 돌리면 새 폴더에 기록되므로,
    # 이전 결과 때문에 회차를 건너뛰지 않고, 서로 다른 입력의 결과가 한 기록에 섞이지 않는다.
    label = args.games if args.games in cfg["games"] else "custom"
    if args.out:
        out_dir = Path(args.out)
    elif main_run:
        out_dir = BASE / "runs" / "main"
    else:
        out_dir = BASE / ("runs-mock" if args.mock else "runs") / f"{label}-{inputs[:8]}"
    # 사람마다 다른 기록 파일에 쓴다. 각자 커밋해도 git에서 충돌하지 않는다. 건너뛸 회차는 모든 기록 파일에서 찾는다.
    who = "" if args.models == "all" else "-" + slug(args.models)
    log_path = out_dir / f"runs{who}{shard_tag}.jsonl"
    records = [json.loads(line) for path in sorted(out_dir.glob("runs*.jsonl"))
               for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if any(r.get("inputs_sha256") != inputs for r in records):
        sys.exit(f"{out_dir}에 다른 입력으로 돌린 회차가 있다. 입력을 되돌리거나 다른 폴더(--out)를 쓴다.")
    done = {r["run_id"] for r in records}
    pending = [r for r in runs if r.run_id not in done]
    print(f"회차 {len(runs)}개 중 남은 것 {len(pending)}개 → {log_path}")
    if args.dry_run:
        for run in pending:
            print(f"  {run.order:3} {run.run_id}")
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    context = {"inputs_sha256": inputs, "frozen": frozen, "environment": environment()}
    for index, run in enumerate(pending, 1):
        print(f"[{index}/{len(pending)}] {run.run_id}", flush=True)
        try:
            result = run_once(cfg, run, llm_for(run.model), out_dir, args.skip_noinput, seed, context)
        except UsageLimitReached as error:
            # 이 회차는 기록하지 않는다. 다음에 이어서 돌리면 끊긴 시도로 옮겨지고 attempt로 남는다.
            sys.exit(f"\n사용 한도가 {exp.get('limit_wait_hours', 6)}시간 안에 풀리지 않음: {error}\n"
                     f"한도가 풀린 뒤 같은 명령으로 이어서 돌린다. 남은 회차 {len(pending) - index + 1}개.")
        with log_path.open("a", encoding="utf-8") as log:
            log.write(json.dumps(result, ensure_ascii=False) + "\n")


CONTEXT_TOKENS = BASE / "runs" / "context_tokens.jsonl"
MEASURE_ASK = "\n\n위 문서를 받았으면 OK라고만 답하세요."


def measure_context(cfg: dict, model: dict, llm, conditions: list[str]) -> list[dict]:
    """조건별 문맥 묶음의 입력 토큰 수를 모델의 실제 토크나이저로 잰다.
    (문맥 + 짧은 요청)과 (짧은 요청만)을 따로 보내 입력 토큰의 차이를 문맥 토큰으로 본다. 시스템 프롬프트 등
    CLI가 붙이는 부분은 두 호출에 똑같이 들어가 빠진다. 한국어(meno)와 영어(SFML)는 바이트당 토큰 수가 달라
    바이트로는 비교할 수 없기 때문이다. 결과는 runs/context_tokens.jsonl에 쌓는다 (모델·조건·묶음 해시별)."""
    rows = []
    base = llm.complete(Run(model, "context", "none", 0), [{"role": "user", "content": MEASURE_ASK.strip()}])
    for condition in conditions:
        raw = (BASE / cfg["conditions"][condition]["context"]).read_text(encoding="utf-8")
        text = raw.rstrip()  # build_prompt와 같이 끝 공백을 떼고 넣는다
        reply = llm.complete(Run(model, "context", condition, 0), [{"role": "user", "content": text + MEASURE_ASK}])
        tokens = (reply.prompt_tokens - base.prompt_tokens
                  if reply.prompt_tokens is not None and base.prompt_tokens is not None else None)
        rows.append({"model": model["id"], "backend": model.get("backend", "litellm"), "condition": condition,
                     # 회차 기록의 hashes.context와 맞추려고 build_prompt와 같은 원문(raw)으로 해시한다
                     "context_sha256": sha256(raw), "context_bytes": len(text.encode("utf-8")),
                     "context_tokens": tokens, "prompt_tokens_with": reply.prompt_tokens,
                     "prompt_tokens_without": base.prompt_tokens,
                     "client_version": reply.extra.get("client_version"),
                     "measured_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")})
        print(f"  {model['id']} {condition}: 문맥 {rows[-1]['context_bytes']:,}바이트 = {tokens}토큰", flush=True)
    CONTEXT_TOKENS.parent.mkdir(parents=True, exist_ok=True)
    with CONTEXT_TOKENS.open("a", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")
    return rows


PROBE_CODE = "int main() { return 0; }"


def probe(cfg: dict, model: dict, llm) -> bool:
    """짧은 2왕복으로 CLI·API 연결을 확인한다. 실험 기록에는 남기지 않는다."""
    run = Run(model, "probe", "probe", 0)
    messages = [{"role": "user", "content": "아래 C++ 코드를 ```cpp 코드 블록 하나로 그대로 돌려주세요.\n\n"
                                            f"```cpp\n{PROBE_CODE}\n```"}]
    print(f"[probe] {model['id']} ({model.get('backend', 'litellm')})", flush=True)
    first = llm.complete(run, messages)
    messages += [{"role": "assistant", "content": first.content},
                 {"role": "user", "content": "방금 돌려준 코드에 있는 함수의 이름만 한 단어로 답해 주세요."}]
    second = llm.complete(run, messages)
    code, _ = extract_code(first.content)
    checks = {
        "1왕복 코드 블록": code is not None and "main" in code,
        "2왕복 대화 이어짐 (main이라고 답함)": "main" in second.content.lower(),
        "입력·출력 토큰 기록": all(r.prompt_tokens and r.completion_tokens for r in (first, second)),
        "도구 호출 없음": not any(r.extra.get("tool_calls") for r in (first, second)),
    }
    for reply in (first, second):
        print(f"  model={reply.model} in={reply.prompt_tokens} cached={reply.cached_tokens} "
              f"out={reply.completion_tokens} reasoning={reply.reasoning_tokens} finish={reply.finish_reason} "
              f"{reply.latency_s:.1f}s extra={ {k: v for k, v in reply.extra.items() if k != 'session_id'} }")
    for name, ok in checks.items():
        print(f"  {'OK ' if ok else 'FAIL'} {name}")
    if not checks["입력·출력 토큰 기록"]:
        print("  주의: 토큰이 안 잡히면 토큰 지표를 쓸 수 없다. CLI 버전의 출력 형식을 확인한다.")
    return all(checks.values())


if __name__ == "__main__":
    main()
