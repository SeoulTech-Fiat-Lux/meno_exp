"""회차 기록을 (모델, 조건)별로 묶어 요약하고, 본 실행의 일정을 추정한다. 시범 실행 뒤 예산·일정을 정하는 데 쓴다.

사용 (llm-comparison/ 에서):
    python3 analysis/summarize.py runs/pilot-<입력 해시>       # 시범 결과 요약 + 본 실행 추정
    python3 analysis/summarize.py runs/main --no-projection    # 본 실행 진행 상황

읽는 것: <폴더>/runs*.jsonl (사람마다 따로 쓴 기록 모두), runs/context_tokens.jsonl (run.py --measure-context).
형식은 harness/records.py에 있다.
본 실행 추정은 시범 회차의 평균을 본 실행의 모든 모델에 그대로 적용한 어림값이다. 게임이 퐁보다 크면 실제로는 더 든다.
"""
from __future__ import annotations

import argparse
import statistics
import sys
import tomllib
from collections import defaultdict
from datetime import datetime
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE / "harness"))
import records as runlog  # noqa: E402


def mean(values) -> float | None:
    values = [v for v in values if v is not None]
    return statistics.fmean(values) if values else None


def fmt(value, digits: int = 0) -> str:
    if value is None:
        return "-"
    return f"{value:,.{digits}f}"


def wall_seconds(record: dict) -> float | None:
    try:
        return (datetime.fromisoformat(record["finished_at"]) - datetime.fromisoformat(record["started_at"])).total_seconds()
    except (KeyError, ValueError):
        return None


def context_tokens_for(rows: list[dict], record: dict) -> int | None:
    """같은 모델·조건·문맥 묶음으로 잰 문맥 토큰 (가장 최근 값)."""
    matches = [r for r in rows if r["model"] == record["model_requested"] and r["condition"] == record["condition"]
               and r["context_sha256"] == record["hashes"]["context"] and r.get("context_tokens") is not None]
    return matches[-1]["context_tokens"] if matches else None


def summarize(records: list[dict], context_rows: list[dict]) -> list[dict]:
    groups: dict[tuple, list[dict]] = defaultdict(list)
    for record in records:
        groups[(record["model_requested"], record["condition"])].append(record)
    rows = []
    for (model, condition), runs in sorted(groups.items()):
        ctx = context_tokens_for(context_rows, runs[0])
        # 문맥은 왕복마다 다시 보내므로(대화 전체가 입력) 회차의 문맥 몫은 왕복 수 × 문맥 토큰이다.
        rest = [r["prompt_tokens"] - r["rounds"] * ctx for r in runs
                if ctx is not None and r.get("prompt_tokens") is not None]
        failures = defaultdict(int)
        for r in runs:
            if r.get("failure"):
                failures[r["failure"]] += 1
        noinput = [r["noinput"] for r in runs if r.get("noinput")]
        rows.append({
            "model": model, "condition": condition, "runs": len(runs),
            "build_success": sum(r["build_success"] for r in runs),
            "first_round_success": sum(bool(r["rounds_detail"]) and bool(r["rounds_detail"][0].get("build_ok"))
                                       for r in runs),
            "rounds": mean(r["rounds"] for r in runs),
            "prompt_tokens": mean(r.get("prompt_tokens") for r in runs),
            "context_tokens": ctx,
            "noncontext_prompt_tokens": mean(rest),
            "completion_tokens": mean(r.get("completion_tokens") for r in runs),
            "reasoning_tokens": mean(r.get("reasoning_tokens") for r in runs),
            "llm_latency_s": mean(r.get("llm_latency_s") for r in runs),
            "wall_s": mean(wall_seconds(r) for r in runs),
            "cost_equiv_usd": mean(r.get("cost_equiv_usd") for r in runs),
            "truncated_rounds": sum(r.get("truncated_rounds") or 0 for r in runs),
            "tool_calls": sum(r.get("tool_calls") or 0 for r in runs),
            "restarted": sum((r.get("attempt") or 1) > 1 for r in runs),
            "early_exit": sum(bool(n.get("early_exit")) for n in noinput),
            "frame_p95_ms": mean(n.get("frame_ms_p95") for n in noinput),
            "failures": dict(failures),
        })
    return rows


def print_table(rows: list[dict]) -> None:
    print("## 모델·조건별 요약 (값은 회차 평균, 개수는 합)\n")
    print("| 모델 | 조건 | 회차 | 빌드 성공 | 1왕복 성공 | 왕복 | 입력 토큰 | 문맥 토큰(1왕복) | 문맥 밖 입력 | "
          "출력 토큰 | 그중 추론 | LLM 시간(초) | 회차 시간(초) | API 환산($) |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for r in rows:
        print(f"| {r['model']} | {r['condition']} | {r['runs']} | {r['build_success']} | {r['first_round_success']} | "
              f"{fmt(r['rounds'], 1)} | {fmt(r['prompt_tokens'])} | {fmt(r['context_tokens'])} | "
              f"{fmt(r['noncontext_prompt_tokens'])} | {fmt(r['completion_tokens'])} | {fmt(r['reasoning_tokens'])} | "
              f"{fmt(r['llm_latency_s'])} | {fmt(r['wall_s'])} | {fmt(r['cost_equiv_usd'], 2)} |")
    print()
    for r in rows:
        notes = []
        if r["failures"]:
            notes.append("실패 사유 " + ", ".join(f"{k} {v}" for k, v in r["failures"].items()))
        for key, label in (("truncated_rounds", "잘린 왕복"), ("tool_calls", "도구 호출"),
                           ("restarted", "다시 받은 회차"), ("early_exit", "10초 안 종료")):
            if r[key]:
                notes.append(f"{label} {r[key]}")
        if r["frame_p95_ms"] is not None:
            notes.append(f"프레임 p95 평균 {r['frame_p95_ms']:.1f}ms")
        if notes:
            print(f"- {r['model']} / {r['condition']}: " + "; ".join(notes))
    if any(r["context_tokens"] is None for r in rows):
        print("- 문맥 토큰이 '-'인 줄은 아직 재지 않음: python3 harness/run.py --models <모델> --measure-context")
    print()


def print_projection(rows: list[dict], cfg: dict) -> None:
    """시범 평균을 본 실행 회차 수에 곱한다. 사람(구독)별로 나눠 보여 준다."""
    games, reps = cfg["games"]["main"], cfg["experiment"]["reps"]
    conditions = list(cfg["conditions"])
    per_condition = {c: [r for r in rows if r["condition"] == c] for c in conditions}
    print(f"## 본 실행 추정 (게임 {len(games)} × 조건 {len(conditions)} × 반복 {reps}, 시범 평균을 모든 모델에 적용)\n")
    print("| 맡는 사람 | 모델 | 회차 | LLM 호출 | 입력 토큰 합 | 출력 토큰 합 | 걸리는 시간(시간) |")
    print("|---|---|---|---|---|---|---|")
    totals: dict[str, list[float]] = defaultdict(lambda: [0, 0, 0, 0, 0])
    for model in cfg["models"]:
        shards = 2 if model.get("backend") == "codex-cli" else 1  # README에 따라 ChatGPT 구독자 2인이 --shard로 나눠 돌린다
        runs = calls = prompt = completion = hours = 0.0
        for condition in conditions:
            stats = per_condition[condition]
            n = len(games) * reps
            rounds = mean(r["rounds"] for r in stats) or 0
            runs += n
            calls += n * rounds
            prompt += n * (mean(r["prompt_tokens"] for r in stats) or 0)
            completion += n * (mean(r["completion_tokens"] for r in stats) or 0)
            hours += n * (mean(r["wall_s"] for r in stats) or 0) / 3600
        who = model.get("operator", model["id"])
        print(f"| {who} | {model['id']} | {runs:.0f} | {calls:.0f} | {prompt:,.0f} | {completion:,.0f} | {hours:.1f} |")
        for i, value in enumerate((runs, calls, prompt, completion, hours)):
            totals[who][i] += value / shards
    print()
    for who, (runs, calls, prompt, completion, hours) in totals.items():
        print(f"- {who}: 1인당 회차 {runs:.0f}, LLM 호출 {calls:.0f}, 입력 토큰 {prompt:,.0f}, 쉬지 않고 돌릴 때 {hours:.1f}시간")
    print("- 구독 한도: 5시간 창에 몇 번 부를 수 있는지는 시범 실행에서 한도에 걸린 시점(cli_retries)으로 본다.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("runs_dir", type=Path)
    parser.add_argument("--no-projection", action="store_true")
    args = parser.parse_args()
    records = runlog.load_records(args.runs_dir)
    rows = summarize(records, runlog.load_context_tokens())
    print(f"# {args.runs_dir} (기록 파일 {len(runlog.run_logs(args.runs_dir))}개, 회차 {len(records)}개)\n")
    print_table(rows)
    if not args.no_projection:
        cfg = tomllib.loads((BASE / "experiment.toml").read_text(encoding="utf-8"))
        print_projection(rows, cfg)


if __name__ == "__main__":
    main()
