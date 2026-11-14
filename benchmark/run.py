"""성능 측정 실행기 (SeoulTech-Fiat-Lux/Meno#30).

Meno 저장소의 벤치마크 실행 파일을 Release로 빌드하고, bench.toml의 (대상, N, 반복)마다 새 프로세스로 돌려
프레임 시간 CSV와 측정 환경을 결과 폴더에 모은다. 측정 코드 자체는 Meno 저장소에 있다.

사용 (benchmark/ 에서):
    python3 run.py --mock                          # 실행 파일 없이 흐름 점검 (results-mock/)
    python3 run.py --meno ~/Meno --dry-run         # 돌릴 목록만 출력
    python3 run.py --meno ~/Meno                   # 빌드 후 측정 (results/<날짜>-<meno 커밋>/)
    python3 run.py --meno ~/Meno --bin <실행 파일>  # 이미 빌드한 실행 파일로 측정

이미 기록된 (대상, N, 반복)은 건너뛰므로, 중단돼도 같은 명령(같은 --out)으로 이어서 돌린다.
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import math
import os
import platform
import random
import subprocess
import sys
import time
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent


def sh(cmd: list[str], **kw) -> str:
    """환경 기록용. 도구가 없거나 실패해도 멈추지 않고 빈 문자열을 돌려준다."""
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=30, **kw).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def git_info(repo: Path) -> dict:
    return {
        "commit": sh(["git", "-C", str(repo), "rev-parse", "HEAD"]),
        "describe": sh(["git", "-C", str(repo), "describe", "--tags", "--always", "--dirty"]),
        # 고친 채로 잰 결과는 어느 코드인지 재현할 수 없으므로 표시해 둔다.
        "dirty": bool(sh(["git", "-C", str(repo), "status", "--porcelain"])),
    }


def environment(meno: Path | None, build_dir: Path | None) -> dict:
    env = {
        "recorded_at": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cpu": next((l.split(":", 1)[1].strip() for l in sh(["lscpu"]).splitlines()
                     if l.startswith("Model name")), platform.processor()),
        "nproc": os.cpu_count(),
        # GL 렌더러가 llvmpipe면 소프트웨어 렌더링이라 GPU 성능이 아니다. README의 "측정 환경" 참고.
        "glxinfo": sh(["glxinfo", "-B"]),
        "gcc": sh(["gcc", "--version"]).splitlines()[:1],
        "cmake": sh(["cmake", "--version"]).splitlines()[:1],
        "wsl": "microsoft" in platform.release().lower(),
        "display": os.environ.get("DISPLAY", ""),
        "cpu_governor": sh(["cat", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"]),
    }
    if meno:
        env["meno"] = git_info(meno)
    if build_dir and (build_dir / "CMakeCache.txt").exists():
        cache = (build_dir / "CMakeCache.txt").read_text(errors="replace").splitlines()
        env["cmake_cache"] = {l.split(":", 1)[0]: l.split("=", 1)[1] for l in cache
                              if l.startswith(("CMAKE_BUILD_TYPE:", "CMAKE_CXX_COMPILER:", "MENO_BUILD_BENCHMARKS:"))}
    return env


def build(meno: Path, build_dir: Path, cfg: dict) -> None:
    subprocess.run(["cmake", "-S", str(meno), "-B", str(build_dir), *cfg["cmake_args"]], check=True)
    subprocess.run(["cmake", "--build", str(build_dir), "-j", str(cfg["jobs"])], check=True)


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
    """실행 파일 약속: 머리글 frame,frame_ms 와 기록 프레임 수만큼의 줄."""
    with path.open(newline="") as f:
        rows = list(csv.DictReader(f))
    values = [float(r["frame_ms"]) for r in rows]
    if len(values) != expected:
        raise ValueError(f"프레임 {len(values)}개 (기대 {expected}개)")
    if not all(math.isfinite(v) and v > 0 for v in values):
        raise ValueError("0 이하이거나 유한하지 않은 프레임 시간이 있음")
    return values


def mock_frames(job: dict, frames: int) -> list[float]:
    # 흐름 점검용 가짜 값. N에 비례하는 비용 + 가끔 튀는 프레임. meno 쪽에 약간의 고정 비용을 얹는다.
    rng = random.Random(str(key(job)))  # 문자열 시드는 실행마다 같다
    base = 0.4 + job["count"] * (0.0009 if job["target"] == "meno" else 0.0007)
    return [base * rng.uniform(0.95, 1.05) * (3 if rng.random() < 0.01 else 1) for _ in range(frames)]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--meno", type=Path, help="Meno 저장소 경로 (--mock이 아니면 필요)")
    ap.add_argument("--bin", type=Path, help="이미 빌드한 실행 파일. 주면 빌드하지 않는다")
    ap.add_argument("--out", type=Path, help="결과 폴더 (기본 results/<날짜>-<meno 커밋 7자리>)")
    ap.add_argument("--config", type=Path, default=HERE / "bench.toml")
    ap.add_argument("--reps", type=int, help="bench.toml의 반복 수 덮어쓰기 (점검용)")
    ap.add_argument("--mock", action="store_true", help="실행 파일 대신 가짜 프레임 시간으로 흐름 점검")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    cfg = tomllib.loads(args.config.read_text(encoding="utf-8"))
    if args.reps:
        cfg["sweep"]["reps"] = args.reps
    if not args.mock and not args.meno:
        ap.error("--meno가 필요하다 (흐름 점검만 하려면 --mock)")

    meno = args.meno.expanduser().resolve() if args.meno else None
    commit = git_info(meno)["commit"][:7] if meno else "mock"
    out = args.out or HERE / ("results-mock" if args.mock else "results") / f"{dt.date.today():%Y%m%d}-{commit}"
    jobs = plan(cfg["sweep"])
    log_path = out / "runs.jsonl"
    done = set()
    if log_path.exists():
        for line in log_path.read_text(encoding="utf-8").splitlines():
            r = json.loads(line)
            if r["ok"]:
                done.add(key(r))
    todo = [j for j in jobs if key(j) not in done]
    print(f"결과 폴더: {out}\n전체 {len(jobs)}개, 남은 것 {len(todo)}개")
    if args.dry_run:
        for j in todo:
            print(f"  rep {j['rep']}  {j['target']:<5} N={j['count']}")
        return

    build_dir = HERE / "work" / f"build-{commit}"
    binary = args.bin
    if not args.mock and not binary:
        build(meno, build_dir, cfg["build"])
        binary = build_dir / cfg["build"]["binary"]
    if not args.mock and not Path(binary).exists():
        sys.exit(f"실행 파일이 없다: {binary}")

    (out / "raw").mkdir(parents=True, exist_ok=True)
    # 환경은 처음 한 번만 쓴다. 이어서 돌릴 때 다른 PC에서 돌면 섞이므로 비교해서 경고한다.
    env_path = out / "env.json"
    env = environment(meno, None if args.mock else build_dir)
    if env_path.exists():
        old = json.loads(env_path.read_text(encoding="utf-8"))
        changed = [k for k in ("cpu", "glxinfo", "meno") if old.get(k) != env.get(k)]
        if changed:
            sys.exit(f"이전 기록과 측정 환경이 다르다: {changed}. 다른 --out을 쓴다")
    else:
        env["config"] = cfg
        env_path.write_text(json.dumps(env, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    sweep, run_cfg = cfg["sweep"], cfg["run"]
    proc_env = os.environ | run_cfg.get("env", {})
    with log_path.open("a", encoding="utf-8") as log:
        for i, job in enumerate(todo, 1):
            csv_path = out / "raw" / f"{job['target']}-{job['count']}-rep{job['rep']}.csv"
            record = dict(job, ok=False)
            start = time.monotonic()
            try:
                if args.mock:
                    values = mock_frames(job, sweep["frames"])
                    csv_path.write_text("frame,frame_ms\n" + "".join(
                        f"{k},{v:.4f}\n" for k, v in enumerate(values)))
                else:
                    cmd = [a.format(bin=binary, target=job["target"], count=job["count"], seed=job["seed"],
                                    warmup=sweep["warmup_frames"], frames=sweep["frames"], out=csv_path)
                           for a in run_cfg["command"]]
                    p = subprocess.run(cmd, env=proc_env, capture_output=True, text=True,
                                       timeout=run_cfg["timeout_s"])
                    record |= {"exit_code": p.returncode, "stderr_tail": p.stderr[-2000:]}
                    if p.returncode != 0:
                        raise RuntimeError(f"종료 코드 {p.returncode}")
                read_frames(csv_path, sweep["frames"])
                record["ok"] = True
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, KeyError) as e:
                record["error"] = str(e)
            record["wall_s"] = round(time.monotonic() - start, 2)
            log.write(json.dumps(record, ensure_ascii=False) + "\n")
            log.flush()
            print(f"[{i}/{len(todo)}] rep {job['rep']} {job['target']:<5} N={job['count']:<6} "
                  f"{'OK' if record['ok'] else '실패: ' + record['error']}")

    print(f"요약: python3 summarize.py {out}")


if __name__ == "__main__":
    main()
