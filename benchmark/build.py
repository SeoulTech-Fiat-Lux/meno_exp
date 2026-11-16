"""bench_app과 vsync_check 빌드. 빌드한 meno가 고정한 커밋인지도 여기서 확인한다."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from common import BENCH_APP, pinned_meno
from probe import git_info


def cmake_build(src: Path, build_dir: Path, cmake_args: list[str], jobs: int, quiet: bool = False) -> None:
    out = subprocess.DEVNULL if quiet else None
    subprocess.run(["cmake", "-S", str(src), "-B", str(build_dir), *cmake_args], check=True, stdout=out)
    subprocess.run(["cmake", "--build", str(build_dir), "-j", str(jobs)], check=True, stdout=out)


def build_bench(build_dir: Path, cmake_args: list[str], jobs: int) -> Path:
    """bench_app을 빌드하고, 실제로 받은 meno가 고정한 커밋인지 확인한 뒤 실행 파일 경로를 돌려준다."""
    cmake_build(BENCH_APP, build_dir, cmake_args, jobs)
    built = git_info(build_dir / "_deps" / "meno-src")["commit"]
    if built and built != pinned_meno():
        sys.exit(f"빌드한 meno({built[:7]})가 고정한 커밋({pinned_meno()[:7]})과 다르다. {build_dir}를 지우고 다시 빌드한다")
    return build_dir / "meno_bench"
