"""benchmark/ 도구들이 함께 쓰는 설정: bench.toml, 실행 명령, 측정하는 meno 커밋.

나머지 공용 모듈은 이 모듈 위에 쌓인다.
  probe.py    측정 환경 조사(렌더러, git, CPU·전원). 실행 도구(run.py, tracy.py, vsync_check.py)가 쓴다
  build.py    bench_app, vsync_check 빌드
  results.py  결과 폴더 이름과 읽기. 분석 도구(summarize.py, figures.py, report.py)가 쓴다
"""
from __future__ import annotations

import os
import re
import sys
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
BENCH_APP = HERE / "bench_app"  # 측정 프로그램. 동결한 meno를 받아 공개 API로만 잰다(Meno 저장소는 고치지 않음)


def load_config(path: Path = HERE / "bench.toml") -> dict:
    return tomllib.loads(path.read_text(encoding="utf-8"))


def run_env(cfg: dict) -> dict:
    """실행 파일에 넘길 환경 변수: 현재 셸 + bench.toml [run].env (수직 동기화 끄기 등)."""
    return os.environ | cfg["run"].get("env", {})


def bench_command(run_cfg: dict, binary, target: str, count: int, seed: int, warmup: int, frames: int,
                  out: Path) -> list[str]:
    """bench.toml [run].command 틀을 채운다. 측정(run.py)과 Tracy 캡처(tracy.py)가 같은 실행 파일 약속을 쓴다."""
    return [a.format(bin=binary, target=target, count=count, seed=seed, warmup=warmup, frames=frames, out=out)
            for a in run_cfg["command"]]


def pinned_meno() -> str:
    """bench_app/CMakeLists.txt가 받는 meno 커밋. 결과 폴더 이름, 빌드 확인, 환경 기록에 쓴다."""
    text = (BENCH_APP / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"Meno\.git\s+GIT_TAG\s+([0-9a-f]{40})", text)
    if not m:
        sys.exit("bench_app/CMakeLists.txt에서 meno의 GIT_TAG(커밋 해시)를 찾지 못했다")
    return m.group(1)
