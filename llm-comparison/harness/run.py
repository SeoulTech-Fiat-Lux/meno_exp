"""LLM 생성 비교 실험 하니스 (SeoulTech-Fiat-Lux/Meno#29).

회차 = (모델, 게임, 조건, 반복 번호). 회차마다 LLM에게 main.cpp를 받아 빌드하고, 실패하면 오류를 돌려주며
최대 max_rounds번 시도한다. 빌드되면 무입력 실행·녹화·프레임 시간 측정을 하고, 결과를 JSONL 한 줄로 남긴다.

사용 (llm-comparison/ 에서):
    python3 harness/run.py --games pilot --dry-run          # 실행할 회차 목록만 출력
    python3 harness/run.py --games pilot --mock --reps 1    # LLM 없이 흐름 점검 (runs-mock/)
    python3 harness/run.py --models top --probe             # 구독 CLI 연결 확인 (짧은 2왕복, 기록 안 함)
    python3 harness/run.py --games pilot                    # 시범 실행 (runs/, games.pilot_models)
    python3 harness/run.py --games main --models top,light  # 본 실행: Claude 구독자
    python3 harness/run.py --games main --models other-vendor --shard 1/2   # 본 실행: ChatGPT 구독자 1
    python3 harness/run.py --games main --models other-vendor --shard 2/2   # 본 실행: ChatGPT 구독자 2

모듈: records.py(회차 계획과 기록 형식), llm.py(구독 CLI 백엔드), freeze.py(동결 잠금). 설정은 experiment.toml 하나다.
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
import re
import shutil
import subprocess
import sys
import time
import tomllib
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent  # llm-comparison/
sys.path.insert(0, str(Path(__file__).resolve().parent))  # python -I로 실행해도 같은 폴더의 모듈을 찾도록 한다
import freeze  # noqa: E402
import llm  # noqa: E402
import records as runlog  # noqa: E402
from records import Run  # noqa: E402

CONFIG = BASE / "experiment.toml"


def sha256(data: str | bytes) -> str:
    if isinstance(data, str):
        data = data.encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def render(template: str, **values: str) -> str:
    for key, value in values.items():
        template = template.replace("{{" + key + "}}", value)
    return template


def prompt_file(name: str) -> str:
    return (BASE / "prompts" / name).read_text(encoding="utf-8")


def build_prompt(cfg: dict, condition: str, game: str) -> tuple[str, dict]:
    cond = cfg["conditions"][condition]
    context_path = BASE / cond["context"]
    if not context_path.exists():
        sys.exit(f"문맥 묶음이 없음: {context_path}\ncontexts/build_context.py로 먼저 만든다 (README의 동결 절차 참고).")
    context = context_path.read_text(encoding="utf-8")
    template = (BASE / cond["template_dir"] / "CMakeLists.txt").read_text(encoding="utf-8")
    spec = (BASE / "specs" / "games" / f"{game}.md").read_text(encoding="utf-8")
    prompt = render(prompt_file("instruction.md"), LIBRARY=cond["library"], RULE=cond["rule"],
                    CONTEXT=context.rstrip(), TEMPLATE=template.rstrip(), SPEC=spec.rstrip())
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
    shutil.copy(BASE / cond["mock_source"], work / "main.cpp")  # configure할 때 쓸 임시 main.cpp
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
        game.unlink()  # 빌드가 실패했을 때 이전 실행 파일이 남지 않도록 지운다
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


def run_noinput(cfg: dict, work: Path, out: Path) -> dict:
    """무입력 실행·녹화·프레임 시간 측정을 한다. 실패해도 하니스를 멈추지 않는다(멈추면 이 회차를 LLM에게 새로 받게 된다)."""
    steps = [("run_noinput.sh", [str(BASE / "harness" / "run_noinput.sh"), str(work / "build"), str(out),
                                 str(cfg["experiment"]["noinput_seconds"])]),
             ("frametime_stats.py", [sys.executable, str(BASE / "harness" / "frametime_stats.py"), str(out)])]
    errors = []
    for label, command in steps:
        proc = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        if proc.returncode != 0:
            errors.append(f"{label} 종료 코드 {proc.returncode}: {proc.stderr.strip()[-500:]}")
            break
    result = {}
    if (out / "result.json").exists():
        raw = (out / "result.json").read_text()
        try:
            result = json.loads(raw)
        except json.JSONDecodeError as error:
            errors.append(f"result.json을 읽지 못함 ({error}): {raw.strip()[:300]}")
    if errors:
        result["harness_error"] = errors
    return result


# ---------------------------------------------------------------------------
# 회차 실행
# ---------------------------------------------------------------------------
def environment() -> dict:
    """프레임 시간과 빌드 결과를 해석할 때 필요한 실행 환경. 소프트웨어 렌더링(llvmpipe)을 쓰므로 CPU와 Mesa 버전이 중요하다."""
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
            "gcc": llm.first_line(["gcc", "--version"]), "cmake": llm.first_line(["cmake", "--version"]),
            "mesa": llm.first_line(["dpkg-query", "-W", "-f=${Version}", "libgl1-mesa-dri"]),
            "wsl": "microsoft" in platform.release().lower()}


def run_once(cfg: dict, run: Run, model_llm, out_dir: Path, skip_noinput: bool, context: dict) -> dict:
    exp = cfg["experiment"]
    run_dir = out_dir / run.run_id
    # 기록에 없는데 폴더가 있으면 이전 시도가 중간에 끊긴 것이다 (Ctrl-C, 전원, 하니스 설정 오류, 사용 한도).
    # 결과와 무관한 원인으로 끊긴 것이므로 다시 받되, 이전 시도를 지우지 않고 옮겨 두고 몇 번째 시도인지 기록한다.
    interrupted = sorted(p.name for p in out_dir.glob(f"{run.run_id}.interrupted-*"))
    if run_dir.exists() and any(run_dir.iterdir()):
        target = out_dir / f"{run.run_id}.interrupted-{len(interrupted) + 1}"
        run_dir.rename(target)
        interrupted.append(target.name)
        print(f"  끊긴 이전 시도를 {target.name}로 옮기고 다시 시작", flush=True)
    run_dir.mkdir(parents=True, exist_ok=True)
    work = prepare_workspace(cfg, run.condition)
    prompt, meta = build_prompt(cfg, run.condition, run.game)
    (run_dir / "prompt.md").write_text(prompt, encoding="utf-8")

    messages = [{"role": "user", "content": prompt}]
    rounds: list[dict] = []
    started = dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")
    success, code, failure, failure_detail = False, None, None, None
    for number in range(1, exp["max_rounds"] + 1):
        try:
            reply = model_llm.complete(run, messages)
        except llm.RunFailure as error:
            # 문맥 창 초과: 다시 보내도 같으므로 이 왕복을 실패로 남기고 회차를 끝낸다.
            rounds.append({"round": number, "build_ok": False, "api_failure": error.reason,
                           "refused": False, "code_found": False})
            failure, failure_detail = error.reason, error.detail
            print(f"  왕복 {number}: {error.reason} → 실패", flush=True)
            break
        (run_dir / f"response_{number}.md").write_text(reply.content, encoding="utf-8")
        messages.append({"role": "assistant", "content": reply.content})
        code, blocks = llm.extract_code(reply.content)
        truncated = reply.finish_reason == "length"
        record = {"round": number, "model_served": reply.model, "prompt_tokens": reply.prompt_tokens,
                  "completion_tokens": reply.completion_tokens, "reasoning_tokens": reply.reasoning_tokens,
                  "cached_tokens": reply.cached_tokens, "cache_write_tokens": reply.cache_write_tokens,
                  "finish_reason": reply.finish_reason, "truncated": truncated, "refused": reply.refused,
                  "latency_s": round(reply.latency_s, 2), "code_found": code is not None, "code_blocks": blocks,
                  **reply.extra}  # CLI 버전, 세션, 재시도, 도구 호출 수 등
        if reply.refused:
            # 공급자가 거부를 알리면 바로 실패로 끝낸다. 같은 요청을 다시 보내도 대개 또 거부해 왕복만 쓴다.
            rounds.append(record | {"build_ok": False})
            failure = "refused"
            print(f"  왕복 {number}: 공급자 거부 → 실패", flush=True)
            break
        if code is None:
            rounds.append(record | {"build_ok": False})
            # 길이 제한으로 잘렸으면 그 사실을 알려 준다. 형식 오류와 구분해 기록한다.
            messages.append({"role": "user", "content": prompt_file("truncated.md" if truncated else "format_error.md")})
            continue
        (run_dir / f"main_{number}.cpp").write_text(code, encoding="utf-8")
        ok, log, build_s = build(cfg, work, code)
        (run_dir / f"build_{number}.log").write_text(log, encoding="utf-8")
        rounds.append(record | {"build_ok": ok, "build_s": round(build_s, 2)})
        print(f"  왕복 {number}: 빌드 {'성공' if ok else '실패'}", flush=True)
        if ok:
            success = True
            break
        feedback = render(prompt_file("build_error.md"), ERRORS=error_excerpt(log, exp["error_feedback_lines"], work))
        (run_dir / f"feedback_{number}.md").write_text(feedback, encoding="utf-8")
        messages.append({"role": "user", "content": feedback})
    if not success and failure is None:
        failure = "rounds_exhausted"

    noinput = None
    if success:
        (run_dir / "main.cpp").write_text(code, encoding="utf-8")
        shutil.copy(work / "build" / "game", run_dir / "game")  # 채점용. git에는 넣지 않는다
        if not skip_noinput:
            noinput = run_noinput(cfg, work, run_dir / "noinput")

    def total(key: str) -> int | float | None:
        values = [r.get(key) for r in rounds if r.get(key) is not None]
        return sum(values) if values else None

    tags = freeze.template_tags(cfg)
    return {
        "schema_version": runlog.SCHEMA_VERSION,  # 형식은 harness/records.py에 적혀 있다
        "run_id": run.run_id, "started_at": started,
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "order": run.order, "seed": exp["seed"],
        "model_requested": run.model["id"], "model_tier": run.model.get("tier"),
        "models_served": sorted({r["model_served"] for r in rounds if r.get("model_served")}),
        "backend": run.model.get("backend", "mock"), "model_cli": run.model.get("model"),
        "effort": run.model.get("effort"), "operator": run.model.get("operator"),
        "client_versions": sorted({r["client_version"] for r in rounds if r.get("client_version")}),
        "game": run.game, "condition": run.condition, "rep": run.rep,
        "rounds": len(rounds), "build_success": success, "failure": failure, "failure_detail": failure_detail,
        "attempt": len(interrupted) + 1, "interrupted_attempts": interrupted,
        # 비교 지표는 캐시와 무관한 원래 토큰 수(prompt_tokens, completion_tokens)다. 캐시 토큰은 참고용이다.
        "prompt_tokens": total("prompt_tokens"), "completion_tokens": total("completion_tokens"),
        "reasoning_tokens": total("reasoning_tokens"),
        "cached_tokens": total("cached_tokens"), "cache_write_tokens": total("cache_write_tokens"),
        "cost_equiv_usd": total("cost_equiv_usd"), "tool_calls": total("tool_calls"),
        "cli_retries": total("cli_retries"), "llm_latency_s": total("latency_s"),
        "code_lines": sum(1 for line in code.splitlines() if line.strip()) if success else None,
        "truncated_rounds": sum(1 for r in rounds if r.get("truncated")),
        "context_bytes": meta["context_bytes"], "hashes": meta["hashes"],
        "inputs_sha256": context["inputs_sha256"], "frozen": context["frozen"],
        "max_output_tokens": exp["max_output_tokens"],
        "versions": {"meno_commit": tags["meno"].get("meno"), "sfml_commit": tags["sfml"].get("SFML"),
                     "sfml_commit_meno_condition": tags["meno"].get("SFML"), "python": platform.python_version()},
        "environment": context["environment"],
        "noinput": noinput, "rounds_detail": rounds,
    }


# ---------------------------------------------------------------------------
# 문맥 토큰 측정
# ---------------------------------------------------------------------------
MEASURE_ASK = "\n\n위 문서를 받았으면 OK라고만 답하세요."


def measure_context(cfg: dict, model: dict, model_llm, conditions: list[str]) -> list[dict]:
    """조건별 문맥 묶음의 입력 토큰 수를 모델의 실제 토크나이저로 잰다. 한국어(meno)와 영어(SFML)는
    바이트당 토큰 수가 달라 바이트로는 비교할 수 없기 때문이다.
    (문맥 + 짧은 요청)과 (짧은 요청만)을 따로 보내 입력 토큰의 차이를 문맥 토큰으로 본다. 시스템 프롬프트처럼
    CLI가 붙이는 부분은 두 호출에 똑같이 들어가므로 차이에서 빠진다. 결과는 runs/context_tokens.jsonl에
    모델·조건·묶음 해시별로 쌓는다."""
    rows = []
    base = model_llm.complete(Run(model, "context", "none", 0), [{"role": "user", "content": MEASURE_ASK.strip()}])
    for condition in conditions:
        raw = (BASE / cfg["conditions"][condition]["context"]).read_text(encoding="utf-8")
        text = raw.rstrip()  # build_prompt와 같이 끝 공백을 떼고 넣는다
        reply = model_llm.complete(Run(model, "context", condition, 0), [{"role": "user", "content": text + MEASURE_ASK}])
        tokens = (reply.prompt_tokens - base.prompt_tokens
                  if reply.prompt_tokens is not None and base.prompt_tokens is not None else None)
        rows.append({"model": model["id"], "backend": model.get("backend"), "condition": condition,
                     # 회차 기록의 hashes.context와 맞추려고 build_prompt와 같은 원문(raw)으로 해시한다
                     "context_sha256": sha256(raw), "context_bytes": len(text.encode("utf-8")),
                     "context_tokens": tokens, "prompt_tokens_with": reply.prompt_tokens,
                     "prompt_tokens_without": base.prompt_tokens,
                     "client_version": reply.extra.get("client_version"),
                     "measured_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds")})
        print(f"  {model['id']} {condition}: 문맥 {rows[-1]['context_bytes']:,}바이트 = {tokens}토큰", flush=True)
    runlog.CONTEXT_TOKENS.parent.mkdir(parents=True, exist_ok=True)
    with runlog.CONTEXT_TOKENS.open("a", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")
    return rows


# ---------------------------------------------------------------------------
# 명령줄
# ---------------------------------------------------------------------------
def select_models(cfg: dict, args) -> list[dict]:
    if args.mock:
        return [{"tier": "mock", "id": "mock"}]
    wanted = args.models
    if args.games == "pilot" and wanted == "all" and cfg["games"].get("pilot_models"):
        wanted = ",".join(cfg["games"]["pilot_models"])  # 시범 실행은 정해 둔 모델로만 돌린다
        args.models = wanted
    models = cfg["models"] if wanted == "all" else \
        [m for m in cfg["models"] if m["id"] in set(wanted.split(",")) or m.get("tier") in set(wanted.split(","))]
    if not models:
        sys.exit(f"--models {wanted}에 맞는 모델이 없음")
    for m in models:
        if m.get("backend") not in llm.BACKENDS:
            sys.exit(f"{m['id']}: backend는 {', '.join(llm.BACKENDS)} 중 하나여야 함 (지금 {m.get('backend')})")
        if not args.dry_run and (runlog.model_undecided(m) or not m.get("effort")):
            sys.exit(f"{m['id']}: experiment.toml에서 model과 effort를 먼저 정한다.")
    return models


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--games", default="pilot", help="pilot, main, 또는 쉼표로 나눈 게임 이름")
    parser.add_argument("--conditions", default="meno,sfml")
    parser.add_argument("--models", default="all", help="all 또는 쉼표로 나눈 tier/id")
    parser.add_argument("--reps", type=int, help="설정의 reps 대신 쓸 반복 횟수")
    parser.add_argument("--mock", action="store_true", help="LLM 없이 smoke 프로그램으로 흐름 점검")
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

    cfg = tomllib.loads(CONFIG.read_text(encoding="utf-8"))
    exp = cfg["experiment"]
    games = cfg["games"].get(args.games, None) or args.games.split(",")
    conditions = args.conditions.split(",")
    models = select_models(cfg, args)

    backends: dict[str, object] = {}

    def llm_for(model: dict):
        kind = "mock" if args.mock else model["backend"]
        if kind not in backends:
            backends[kind] = llm.MockLLM(cfg, args.mock_fail_first) if args.mock else llm.BACKENDS[kind](cfg)
        return backends[kind]

    if args.probe:
        sys.exit(0 if all(llm.probe(model, llm_for(model)) for model in models) else 1)
    if args.measure_context:
        for model in models:
            measure_context(cfg, model, llm_for(model), conditions)
        return

    # 입력(설정, 지시문, 템플릿, 명세, 문맥, 에셋, 채점표, 하니스)의 해시. 본 실행은 동결 잠금과 같아야 시작한다.
    inputs = freeze.inputs_hash()
    lock = json.loads(freeze.LOCK.read_text(encoding="utf-8")) if freeze.LOCK.exists() else None
    frozen = lock is not None and lock["inputs_sha256"] == inputs
    if main_run and not frozen and not args.dry_run:  # 회차 목록은 동결 전에도 볼 수 있다
        freeze.check()
        sys.exit("본 실행은 동결 상태에서만 시작한다. harness/freeze.py write로 동결하거나 바뀐 입력을 되돌린다.")

    runs = runlog.plan_runs(games, conditions, models, args.reps or exp["reps"], exp["seed"])
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
    # 사람마다 다른 기록 파일에 쓰므로 각자 커밋해도 git에서 충돌하지 않는다. 건너뛸 회차는 모든 기록 파일에서 찾는다.
    who = "" if args.models == "all" else "-" + runlog.slug(args.models)
    log_path = out_dir / f"runs{who}{shard_tag}.jsonl"
    records = runlog.load_records(out_dir, required=False)
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
            result = run_once(cfg, run, llm_for(run.model), out_dir, args.skip_noinput, context)
        except llm.UsageLimitReached as error:
            # 이 회차는 기록하지 않는다. 다음에 이어서 돌리면 끊긴 시도로 옮겨지고 attempt로 남는다.
            sys.exit(f"\n사용 한도가 {exp.get('limit_wait_hours', 6)}시간 안에 풀리지 않음: {error}\n"
                     f"한도가 풀린 뒤 같은 명령으로 이어서 돌린다. 남은 회차 {len(pending) - index + 1}개.")
        with log_path.open("a", encoding="utf-8") as log:
            log.write(json.dumps(result, ensure_ascii=False) + "\n")


if __name__ == "__main__":
    main()
