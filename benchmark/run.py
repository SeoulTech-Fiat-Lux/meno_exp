"""성능 측정 실행기 (SeoulTech-Fiat-Lux/Meno#30).

측정 프로그램(bench_app/)을 Release로 빌드하고, bench.toml의 (대상, N, 반복)마다 새 프로세스로 돌려
프레임 시간 CSV와 측정 환경을 결과 폴더에 모은다. bench_app은 동결한 meno(46f8934)를 받아 공개 API로만
재므로 Meno 저장소는 필요 없다.

사용 (benchmark/ 에서):
    python3 run.py --mock                  # 실행 파일 없이 흐름 점검 (results-mock/)
    python3 run.py --dry-run               # 돌릴 목록만 출력
    OUT=$(python3 run.py --print-out)      # 기본 결과 폴더 경로만 출력 (다른 도구에 --out으로 넘긴다)
    python3 run.py                         # 빌드 후 측정 (results/<날짜>-<meno 커밋>-<GPU>/)
    python3 run.py --bin <실행 파일>       # 이미 빌드한 실행 파일로 측정

이미 기록된 (대상, N, 반복)은 건너뛰므로, 중단되어도 같은 명령(같은 --out)으로 이어서 돌릴 수 있다.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import subprocess
import sys
import time
from pathlib import Path

from build import build_bench
from common import HERE, bench_command, load_config, run_env
from probe import HostLoad, cpu_snapshot, cpu_usage, environment, probe_gl
from results import ok_runs, raw_path, read_frame_ms, read_runs, result_dir


def plan(sweep: dict) -> list[dict]:
    """반복마다 모든 (대상, N)을 한 바퀴 돈다. 순서는 반복 번호로 정한 시드로 섞어 재현할 수 있게 한다."""
    jobs = []
    for rep in range(1, sweep["reps"] + 1):
        cells = [(t, n) for t in sweep["targets"] for n in sweep["counts"]]
        if sweep.get("order", "interleaved") == "interleaved":
            random.Random(rep).shuffle(cells)
        jobs += [{"target": t, "count": n, "rep": rep, "seed": rep} for t, n in cells]
    return jobs


def key(job: dict) -> tuple:
    return job["target"], job["count"], job["rep"]


def read_frames(path: Path, expected: int) -> list[float]:
    """실행 파일 약속을 검사한다: 머리글 frame,frame_ms와 기록 프레임 수만큼의 줄이 있어야 한다."""
    values = read_frame_ms(path)
    if len(values) != expected:
        raise ValueError(f"프레임 {len(values)}개 (기대 {expected}개)")
    if not all(math.isfinite(v) and v > 0 for v in values):
        raise ValueError("0 이하이거나 유한하지 않은 프레임 시간이 있음")
    return values


def mock_frames(job: dict, frames: int) -> list[float]:
    # 흐름 점검용 가짜 값. N에 비례하는 비용에 가끔 튀는 프레임을 더한다. 단계가 늘수록 스프라이트당 비용을 조금씩 얹는다.
    rng = random.Random(str(key(job)))  # 문자열 시드는 프로세스가 바뀌어도 같은 난수열을 낸다
    per_sprite = {"sfml-batched": 0.0002, "sfml": 0.0007, "meno-renderer": 0.0008}.get(job["target"], 0.0010)
    base = 0.4 + job["count"] * per_sprite
    return [base * rng.uniform(0.95, 1.05) * (3 if rng.random() < 0.01 else 1) for _ in range(frames)]


def run_binary(binary, run_cfg: dict, proc_env: dict, job: dict, warmup: int, frames: int,
               csv_path: Path, record: dict) -> None:
    """실행 파일을 한 번 돌리고 출력 CSV를 검사한다. 실패하면 예외를 던지고, 종료 코드·stderr는 record에 남긴다."""
    cmd = bench_command(run_cfg, binary, job["target"], job["count"], job["seed"], warmup, frames, csv_path)
    p = subprocess.run(cmd, env=proc_env, capture_output=True, text=True, timeout=run_cfg["timeout_s"])
    record |= {"exit_code": p.returncode, "stderr_tail": p.stderr[-2000:]}
    if p.returncode != 0:
        raise RuntimeError(f"종료 코드 {p.returncode}")
    read_frames(csv_path, frames)


RUN_ERRORS = (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyError)


def preflight(binary, cfg: dict, proc_env: dict, work: Path) -> None:
    """본 측정 전에 대상마다 가장 작은 N으로 짧게 한 번 돌려 본다. 대상 이름이나 출력 형식이 약속과
    어긋나면 회차를 하나도 기록하기 전에 멈춘다. 이 실행은 기록하지 않는다."""
    work.mkdir(parents=True, exist_ok=True)
    failures = []
    for target in cfg["sweep"]["targets"]:
        job = {"target": target, "count": min(cfg["sweep"]["counts"]), "seed": 0}
        record: dict = {}
        try:
            run_binary(binary, cfg["run"], proc_env, job, 5, 10, work / f"{target}.csv", record)
            print(f"사전 점검 {target:<14} OK")
        except RUN_ERRORS as e:
            failures.append(target)
            print(f"사전 점검 {target:<14} 실패: {e}")
            if record.get("stderr_tail"):
                print(record["stderr_tail"].rstrip())
    if failures:
        sys.exit(f"사전 점검 실패: {failures}. bench.toml의 targets·command가 실행 파일과 맞는지 확인한다 (아무것도 기록하지 않음)")


def measure_all(todo: list[dict], mock: bool, binary, run_cfg: dict, proc_env: dict, sweep: dict, out: Path,
                log_path: Path, host: HostLoad | None) -> None:
    """남은 측정을 차례로 돌리고 한 줄씩 runs.jsonl에 덧붙인다. 한 측정이 실패해도 다음으로 넘어간다."""
    with log_path.open("a", encoding="utf-8") as log:
        for i, job in enumerate(todo, 1):
            csv_path = raw_path(out, job["target"], job["count"], job["rep"])
            record = dict(job, ok=False)
            if host:
                record["host_cpu_pct"] = host.sample()  # 측정 직전. 측정 중에 물으면 그 질의가 CPU를 쓴다
            start = time.monotonic()
            before = cpu_snapshot()
            try:
                if mock:
                    values = mock_frames(job, sweep["frames"])
                    csv_path.write_text("frame,frame_ms\n" + "".join(
                        f"{k},{v:.4f}\n" for k, v in enumerate(values)))
                    read_frames(csv_path, sweep["frames"])
                else:
                    run_binary(binary, run_cfg, proc_env, job, sweep["warmup_frames"], sweep["frames"],
                               csv_path, record)
                    record["cpu"] = cpu_usage(before, cpu_snapshot())
                record["ok"] = True
            except RUN_ERRORS as e:
                record["error"] = str(e)
            record["wall_s"] = round(time.monotonic() - start, 2)
            log.write(json.dumps(record, ensure_ascii=False) + "\n")
            log.flush()
            print(f"[{i}/{len(todo)}] rep {job['rep']} {job['target']:<14} N={job['count']:<6} "
                  f"{'OK' if record['ok'] else '실패: ' + record['error']}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", type=Path, help="이미 빌드한 실행 파일. 주면 빌드하지 않는다")
    ap.add_argument("--out", type=Path, help="결과 폴더 (기본 results/<날짜>-<meno 커밋 7자리>-<GPU>)")
    ap.add_argument("--config", type=Path, default=HERE / "bench.toml")
    ap.add_argument("--reps", type=int, help="bench.toml의 반복 수 덮어쓰기 (점검용)")
    ap.add_argument("--mock", action="store_true", help="실행 파일 대신 가짜 프레임 시간으로 흐름 점검")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--print-out", action="store_true",
                    help="기본 결과 폴더 경로만 출력하고 끝낸다. 날짜가 바뀌어도 모든 도구가 같은 폴더를 쓰게 한다")
    ap.add_argument("--allow-software", action="store_true",
                    help="소프트웨어 렌더러(llvmpipe 등)에서도 돌린다. 흐름 점검용이고 결과는 표에 쓰지 않는다")
    args = ap.parse_args()

    cfg = load_config(args.config)
    if args.reps:
        cfg["sweep"]["reps"] = args.reps

    # 렌더러는 결과 폴더 이름(GPU)에 쓰이며, 빌드에 몇 분을 들이기 전에 미리 확인한다.
    sweep, run_cfg = cfg["sweep"], cfg["run"]
    proc_env = run_env(cfg)
    gl = probe_gl(proc_env, args.mock, args.allow_software)
    out = args.out or result_dir(args.mock, gl)
    if args.print_out:
        print(out)
        return
    jobs = plan(cfg["sweep"])
    log_path = out / "runs.jsonl"
    done = ok_runs(read_runs(out))
    todo = [j for j in jobs if key(j) not in done]
    print(f"결과 폴더: {out}\n전체 {len(jobs)}개, 남은 것 {len(todo)}개")
    if args.dry_run:
        for j in todo:
            print(f"  rep {j['rep']}  {j['target']:<14} N={j['count']}")
        return

    build_dir = HERE / "work" / "build"
    binary = args.bin
    if not args.mock and not binary:
        binary = build_bench(build_dir, cfg["build"]["cmake_args"], cfg["build"]["jobs"])
    if not args.mock and not Path(binary).exists():
        sys.exit(f"실행 파일이 없다: {binary}")
    if not args.mock and todo:
        preflight(binary, cfg, proc_env, HERE / "work" / "preflight")

    (out / "raw").mkdir(parents=True, exist_ok=True)
    # 환경은 처음 한 번만 기록한다. 이어서 돌리는 PC가 다르면 결과가 섞이므로, 기록된 환경과 비교해 다르면 멈춘다.
    env_path = out / "env.json"
    env = environment(None if args.mock or args.bin else build_dir, gl)
    if env_path.exists():
        old = json.loads(env_path.read_text(encoding="utf-8"))
        # windows_power는 기록이 없는 옛 결과 폴더에서는 비교하지 않는다.
        changed = [k for k in ("cpu", "gl_renderer", "meno", "bench_app", "windows_power")
                   if k in old and old.get(k) != env.get(k)]
        if changed:
            sys.exit(f"이전 기록과 측정 환경이 다르다: {changed}. 다른 --out을 쓴다")
    else:
        env["config"] = cfg
        env_path.write_text(json.dumps(env, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    host = None if args.mock else HostLoad()
    try:
        measure_all(todo, args.mock, binary, run_cfg, proc_env, sweep, out, log_path, host)
    finally:  # 중간에 멈춰도(Ctrl+C 등) 띄워 둔 PowerShell을 닫는다
        if host:
            host.close()
    print(f"요약: python3 summarize.py {out}")


if __name__ == "__main__":
    main()
