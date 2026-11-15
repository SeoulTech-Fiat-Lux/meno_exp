"""LLM 호출: 구독 CLI 백엔드(Claude Code, Codex)와 mock, 연결 확인(probe).

API 키 대신 각자의 구독 로그인으로 CLI를 부른다 (docs/harness.md "구독 CLI"). 변수를 통제하기 위해 두 CLI 모두 다음과 같이 돌린다:
  - 자체 시스템 프롬프트를 prompts/system.md로 바꾼다 (에이전트용 긴 지시문을 빼고, 두 공급자에 같은 문장을 준다).
  - 도구(파일 읽기·명령 실행·웹)를 끄고, 빈 임시 폴더에서 돌린다. 모델이 직접 컴파일해 볼 수 없게 하려는 것이다.
  - 사용자 설정(CLAUDE.md, AGENTS.md, 플러그인, MCP)을 읽지 않는다.
  - 왕복은 CLI의 세션 이어 가기로 같은 대화에 붙인다.

백엔드는 complete(run, messages) -> Reply 하나만 지키면 된다. run은 records.Run, messages는
[{"role": "user"|"assistant", "content": str}, ...]이고 마지막이 이번에 보낼 user 메시지다.
CLI 플래그나 출력 형식이 바뀌면 이 파일만 고친다. 고친 뒤에는 python3 tests/run_all.py와 --probe로 확인한다.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

from records import Run

BASE = Path(__file__).resolve().parent.parent  # llm-comparison/


@dataclass
class Reply:
    content: str
    model: str
    prompt_tokens: int | None = None          # 캐시와 무관한 원래 입력 토큰 (비교 지표)
    completion_tokens: int | None = None      # 출력 토큰 (추론 포함)
    cached_tokens: int | None = None          # 그중 캐시에서 읽은 입력 토큰
    cache_write_tokens: int | None = None     # 그중 캐시에 새로 쓴 입력 토큰 (Claude)
    reasoning_tokens: int | None = None       # 출력 토큰 중 추론 토큰
    finish_reason: str | None = None          # stop, length(잘림), refusal. 알 수 없으면 None
    refused: bool = False                     # 공급자가 거부했다고 알려 준 답
    latency_s: float = 0.0
    extra: dict = field(default_factory=dict)  # 회차 기록의 왕복 항목에 그대로 들어간다 (CLI 버전, 세션 등)


class RunFailure(Exception):
    """다시 보내도 같은 결과가 나오는 오류(문맥 창 초과). 하니스를 멈추지 않고 그 회차를 실패로 기록한다.
    멈추면 이어서 돌릴 때 같은 회차를 LLM에게 새로 받게 되어, 실패한 표본이 다른 표본으로 바뀐다."""

    def __init__(self, reason: str, detail: str):
        super().__init__(f"{reason}: {detail}")
        self.reason, self.detail = reason, detail


class UsageLimitReached(Exception):
    """구독 사용 한도에 걸려 기다린 시간이 상한을 넘었다. 하니스를 멈추고 나중에 이어서 돌린다."""


CODE_BLOCK = re.compile(r"```(?:cpp|c\+\+|cc|cxx)[^\n]*\n(.*?)```", re.S)


def extract_code(text: str) -> tuple[str | None, int]:
    """닫힌 cpp 코드 블록 중 가장 긴 것을 main.cpp로 본다. 바뀐 부분만 보여 주는 짧은 블록이 앞에 있어도
    전체 파일을 고르게 하기 위함이다. (코드, 블록 수)를 돌려준다."""
    blocks = CODE_BLOCK.findall(text or "")
    return (max(blocks, key=len) if blocks else None), len(blocks)


def first_line(command: list[str]) -> str | None:
    """명령 출력의 첫 줄을 돌려준다(버전 기록용). 실패하면 None을 돌려준다."""
    try:
        return subprocess.run(command, capture_output=True, text=True, check=True).stdout.splitlines()[0].strip()
    except Exception:
        return None


# ---------------------------------------------------------------------------
# mock
# ---------------------------------------------------------------------------
class MockLLM:
    """LLM 없이 흐름을 점검한다. smoke 프로그램을 답으로 내고, fail_first면 첫 답에 컴파일 오류를 넣는다."""

    def __init__(self, cfg: dict, fail_first: bool):
        self.cfg, self.fail_first = cfg, fail_first

    def complete(self, run: Run, messages: list[dict]) -> Reply:
        code = (BASE / self.cfg["conditions"][run.condition]["mock_source"]).read_text(encoding="utf-8")
        if self.fail_first and len(messages) == 1:
            code = code.replace("int main()", "int main( // 일부러 넣은 오류", 1)
        words = sum(len(m["content"]) for m in messages) // 4
        return Reply(content=f"```cpp\n{code}```", model="mock", prompt_tokens=words,
                     completion_tokens=len(code) // 4, finish_reason="stop")


# ---------------------------------------------------------------------------
# 구독 CLI 공통
# ---------------------------------------------------------------------------
LIMIT_ERROR = re.compile(r"usage limit|rate.?limit|limit reached|limit will reset|too many requests|\b429\b", re.I)
CONTEXT_ERROR = re.compile(r"prompt is too long|context.?(length|window)|maximum context|input is too long", re.I)
AUTH_ERROR = re.compile(r"authenticat|oauth|not logged in|log ?in again|unauthorized|\b401\b", re.I)
# CLI가 명령줄을 거부한 경우 (없는 플래그, 잘못된 설정 키·값). 다시 보내도 결과가 같으므로 기다리지 않고 바로 멈춘다.
USAGE_ERROR = re.compile(r"unexpected argument|unrecognized (option|argument|subcommand)|"
                         r"unknown (option|argument|flag|field|key|feature|variant)|invalid value|"
                         r"error parsing|failed to parse|^Usage: |For more information, try '--help'", re.I | re.M)


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
        self.version = first_line([self.exe, "--version"])
        self.system_prompt = (BASE / "prompts" / "system.md").read_text(encoding="utf-8").strip()
        self.sessions: dict[str, tuple[str, str]] = {}  # 회차 ID → (작업 폴더, 마지막으로 성공한 세션 ID)
        self.unanswered: dict[str, str] = {}            # 회차 ID → 출력 상한에 걸려 답을 못 받은 요청

    def session(self, run: Run, messages: list[dict]) -> tuple[str, str | None, str]:
        """(작업 폴더, 이어 갈 세션 ID 또는 None, 이번에 보낼 user 메시지)."""
        if len(messages) == 1 or run.run_id not in self.sessions:
            # 회차마다 빈 폴더에서 시작해, 프로젝트 지시문(CLAUDE.md, AGENTS.md)이나 소스 파일이 끼어들지 않게 한다.
            return tempfile.mkdtemp(prefix=f"meno-exp-{run.run_id}-"), None, messages[0]["content"]
        cwd, session_id = self.sessions[run.run_id]
        return cwd, session_id, messages[-1]["content"]

    def error_text(self, proc: subprocess.CompletedProcess) -> str | None:
        raise NotImplementedError

    def is_truncation(self, error: str) -> bool:
        """출력 상한에 걸린 것을 오류로 알리는 CLI가 있다. 일시 오류가 아니므로 다시 보내지 않고 잘린 답으로 다룬다."""
        return False

    def call(self, command: list[str], prompt: str, cwd: str, env: dict | None = None):
        """CLI를 한 번 부르고 (프로세스, 경과 초, 재시도 횟수)를 돌려준다.
        프롬프트는 225KB로 명령줄 인자 한도를 넘으므로 stdin으로 준다.
        문맥 창을 넘으면 RunFailure를 던지고, 사용 한도에 걸리면 기다렸다가 같은 왕복을 다시 보내며, 로그인·명령줄 문제면
        바로 멈춘다. 그 밖의 오류(서버 오류, 시간 초과)는 api_retries번까지 다시 보낸다."""
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
            if error is None or (proc is not None and self.is_truncation(error)):
                return proc, elapsed, retries
            if proc is not None and USAGE_ERROR.search(error + "\n" + (proc.stderr or "")):
                shown = " ".join(c if len(c) < 200 else c[:200] + "…" for c in command)
                sys.exit(f"{self.executable}가 명령줄을 거부함 (CLI 버전 {self.version}). 플래그나 설정 키가 이 버전과 "
                         f"맞지 않는다. harness/llm.py의 {type(self).__name__}를 고친다.\n명령: {shown}\n"
                         f"출력:\n{((proc.stderr or '') + (proc.stdout or '')).strip()[-1500:]}")
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


# ---------------------------------------------------------------------------
# Claude Code
# ---------------------------------------------------------------------------
class ClaudeCLI(CliBackend):
    """Claude Code의 print 모드. 구독(Pro·Max) 로그인을 쓴다."""
    executable = "claude"
    client = "claude-code"
    STOP = {"end_turn": "stop", "stop_sequence": "stop", "max_tokens": "length", "refusal": "refusal"}
    # Claude Code는 출력 상한(CLAUDE_CODE_MAX_OUTPUT_TOKENS)을 넘으면 잘린 답 대신 오류를 내거나(이전 버전),
    # 이어 쓴 마지막 조각만 돌려준다(num_turns > 1, complete() 참고). 상한은 추론 토큰도 포함한다.
    OUTPUT_LIMIT = re.compile(r"exceeded the \d+ output token maximum", re.I)

    def error_text(self, proc):
        try:
            data = json.loads(proc.stdout)
        except json.JSONDecodeError:
            return (proc.stderr or proc.stdout or f"종료 코드 {proc.returncode}").strip()[-1000:]
        if data.get("is_error"):
            return f"{data.get('result', '')} (status {data.get('api_error_status')}, {data.get('terminal_reason')})"
        return None

    def is_truncation(self, error: str) -> bool:
        return bool(self.OUTPUT_LIMIT.search(error))

    def complete(self, run: Run, messages: list[dict]) -> Reply:
        exp = self.cfg["experiment"]
        cwd, resume, prompt = self.session(run, messages)
        # 앞 왕복이 출력 상한에 걸렸으면 그 답은 이어 가지 않는다(마지막 성공 세션에서 이어 가므로).
        # 모델이 답하지 못한 요청을 이번 안내(prompts/truncated.md)와 함께 다시 보낸다.
        unanswered = self.unanswered.pop(run.run_id, None)
        if unanswered is not None and len(messages) > 1:
            prompt = unanswered + "\n\n" + messages[-1]["content"]
        command = [self.exe, "-p", "--output-format", "json",
                   "--safe-mode",                  # CLAUDE.md, 스킬, 플러그인, 훅, MCP, 출력 스타일을 끈다. 구독 로그인은 유지된다
                   "--tools", "",                  # 도구를 모두 끈다
                   "--strict-mcp-config", "--disable-slash-commands",
                   "--system-prompt", self.system_prompt,
                   "--model", run.model["model"], "--effort", run.model["effort"]]
        if resume:
            # 마지막으로 성공한 세션에서 갈라져 나오므로, 실패한 호출을 다시 보내도 대화에 같은 메시지가 겹치지 않는다.
            command += ["--resume", resume, "--fork-session"]
        env = os.environ | {"CLAUDE_CODE_MAX_OUTPUT_TOKENS": str(exp["max_output_tokens"])}
        env.pop("ANTHROPIC_API_KEY", None)  # 키가 있으면 구독 대신 API로 과금될 수 있다
        proc, elapsed, retries = self.call(command, prompt, cwd, env)
        data = json.loads(proc.stdout)
        limit_error = bool(data.get("is_error"))  # call()이 돌려준 오류는 출력 상한 초과뿐이다
        # 2.1.3xx부터는 오류 대신 상한에서 끊고 한 턴 더 이어 쓰며, result에는 마지막 턴의 조각만 담긴다.
        # 도구를 껐으므로 정상 답은 항상 1턴이다. 2턴 이상이면 잘린 답으로 보고 오류일 때와 똑같이 다시 보낸다.
        continued = not limit_error and (data.get("num_turns") or 1) > 1
        truncated = limit_error or continued
        if truncated:
            # 세션은 앞의 성공 지점에 그대로 둔다 (첫 왕복이면 세션이 없으므로 다음 호출에서 새로 시작한다).
            self.unanswered[run.run_id] = prompt
            if resume is None:
                self.sessions.pop(run.run_id, None)
        else:
            self.sessions[run.run_id] = (cwd, data["session_id"])
        usage = data.get("usage", {})
        cache_read = usage.get("cache_read_input_tokens") or 0
        cache_write = usage.get("cache_creation_input_tokens") or 0
        stop = "max_tokens" if truncated else data.get("stop_reason")
        return Reply(
            content="" if truncated else data.get("result") or "",
            model=",".join(sorted(data.get("modelUsage", {}))) or run.model["model"],
            # Anthropic의 input_tokens는 캐시 밖의 토큰만 센다. 비교 지표는 캐시와 무관한 원래 입력 토큰이므로 더한다.
            prompt_tokens=(usage.get("input_tokens") or 0) + cache_read + cache_write,
            completion_tokens=usage.get("output_tokens"),
            cached_tokens=cache_read, cache_write_tokens=cache_write,
            reasoning_tokens=(usage.get("output_tokens_details") or {}).get("thinking_tokens"),
            finish_reason=self.STOP.get(stop, stop), refused=stop == "refusal",
            latency_s=elapsed,
            extra={"client": self.client, "client_version": self.version, "session_id": data["session_id"],
                   # 구독이므로 과금되지 않는다. API로 썼다면 들었을 비용이다 (참고용)
                   "cost_equiv_usd": data.get("total_cost_usd"), "cli_retries": retries,
                   "num_turns": data.get("num_turns")}
                  | ({"output_limit_error": data.get("result", "")[:300]} if limit_error else {})
                  | ({"output_limit_continued": True} if continued else {}))


# ---------------------------------------------------------------------------
# Codex
# ---------------------------------------------------------------------------
class CodexCLI(CliBackend):
    """Codex CLI의 exec 모드. ChatGPT 구독 로그인을 쓴다.

    문서만 보고 맞춘 부분이 있다(첫 --probe로 확인한다): --ignore-user-config, features.shell_tool,
    model_instructions_file, 그리고 exec 옵션을 resume 앞에 두는 순서. 맞지 않으면 call()이 CLI 출력과 함께 바로 멈춘다.
    Claude와 다른 점 (보고서의 한계 절에 적는다):
      - 갈라져 나오는 이어 가기를 지원하지 않아, 실패한 호출을 다시 보내면 대화에 같은 메시지가 겹칠 수 있다 (cli_retries에 남는다).
      - 종료 사유를 알려 주지 않아, 잘린 답은 닫히지 않은 코드 블록 때문에 형식 오류로 집계된다.
      - 응답 모델 ID를 알려 주지 않아, 요청한 모델 이름과 CLI 버전을 기록한다.
    """
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
        return ["--json", "--skip-git-repo-check", "--ignore-user-config", "--sandbox", "read-only",
                "-m", run.model["model"],
                "-c", "approval_policy=" + toml_string("never"),
                "-c", "web_search=" + toml_string("disabled"),
                "-c", "features.shell_tool=false",                                       # 명령 실행 도구를 끈다
                "-c", "model_instructions_file=" + toml_string(str(self.instructions)),  # 기본 지시문을 바꾼다
                "-c", "model_reasoning_effort=" + toml_string(run.model["effort"])]

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
        command.append("-")  # 프롬프트는 stdin으로 준다
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
            model=run.model["model"],
            prompt_tokens=total("input_tokens"), completion_tokens=total("output_tokens"),
            cached_tokens=total("cached_input_tokens"), reasoning_tokens=total("reasoning_output_tokens"),
            latency_s=elapsed,
            extra={"client": self.client, "client_version": self.version, "session_id": thread,
                   "cli_retries": retries,
                   "tool_calls": sum(1 for i in items if i.get("type") in self.TOOL_ITEMS)})


BACKENDS = {"claude-cli": ClaudeCLI, "codex-cli": CodexCLI}


# ---------------------------------------------------------------------------
# 연결 확인
# ---------------------------------------------------------------------------
PROBE_CODE = "int main() { return 0; }"


def probe(model: dict, llm) -> bool:
    """짧은 2왕복으로 CLI 연결을 확인한다: 로그인, 코드 블록, 세션 이어 가기, 토큰 기록, 도구 꺼짐. 기록은 남기지 않는다."""
    run = Run(model, "probe", "probe", 0)
    messages = [{"role": "user", "content": "아래 C++ 코드를 ```cpp 코드 블록 하나로 그대로 돌려주세요.\n\n"
                                            f"```cpp\n{PROBE_CODE}\n```"}]
    print(f"[probe] {model['id']} ({model.get('backend')})", flush=True)
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
