"""benchmark/ 도구들이 함께 쓰는 기능: 설정, 측정 환경(렌더러·git·빌드), 결과 폴더 읽기.

측정 실행(run.py), Tracy(tracy.py), 수직 동기화 확인(vsync_check.py)은 환경 쪽을,
요약(summarize.py), 그림(figures.py), 문서 초안(report.py)은 결과 폴더 쪽을 쓴다.
"""
from __future__ import annotations

import csv
import datetime as dt
import json
import os
import re
import subprocess
import sys
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
BENCH_APP = HERE / "bench_app"  # 측정 프로그램. 동결한 meno를 받아 공개 API로만 잰다(Meno 저장소는 고치지 않음)
SOFTWARE_RENDERERS = ("llvmpipe", "softpipe", "swrast")
# 렌더러 선택에 영향을 주는 환경 변수. WSL에서는 이 값에 따라 어느 GPU로 재는지가 정해진다.
GL_ENV_PREFIXES = ("GALLIUM_", "MESA_", "LIBGL_", "__GL_")


# --- 설정 -------------------------------------------------------------------

def load_config(path: Path = HERE / "bench.toml") -> dict:
    return tomllib.loads(path.read_text(encoding="utf-8"))


def run_env(cfg: dict) -> dict:
    """실행 파일에 넘길 환경 변수: 현재 셸 + bench.toml [run].env (수직 동기화 끄기 등)."""
    return os.environ | cfg["run"].get("env", {})


# --- 측정 환경 --------------------------------------------------------------

def sh(cmd: list[str], **kw) -> str:
    """환경 기록용. 도구가 없거나 실패해도 멈추지 않고 빈 문자열을 돌려준다."""
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=30, **kw).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def git_info(repo: Path, path: str = ".") -> dict:
    """path를 주면 그 경로가 고쳐졌는지만 본다(이 저장소의 bench_app/처럼 저장소 일부만 볼 때)."""
    return {
        "commit": sh(["git", "-C", str(repo), "rev-parse", "HEAD"]),
        "describe": sh(["git", "-C", str(repo), "describe", "--tags", "--always", "--dirty"]),
        # 커밋하지 않은 변경이 있는 채로 잰 결과는 어느 코드로 쟀는지 재현할 수 없으므로 표시해 둔다.
        "dirty": bool(sh(["git", "-C", str(repo), "status", "--porcelain", "--", path])),
    }


def gl_env(env: dict) -> dict:
    return {k: v for k, v in sorted(env.items()) if k.startswith(GL_ENV_PREFIXES) or k == "vblank_mode"}


def gl_info(env: dict) -> dict:
    """glxinfo -B에서 렌더러·버전만 뽑는다. 전체 출력에는 남은 비디오 메모리처럼 실행마다 바뀌는 값이
    있으므로, 이어서 돌릴 때 같은 환경인지는 이 값들로만 비교한다."""
    # 실행 파일과 같은 환경 변수로 조회해야 실행 파일이 실제로 쓸 렌더러를 알 수 있다(WSL의 GALLIUM_DRIVER 등).
    raw = sh(["glxinfo", "-B"], env=env)
    pick = lambda name: next((l.split(":", 1)[1].strip() for l in raw.splitlines()
                              if l.strip().startswith(name + ":")), "")
    return {"gl_renderer": pick("OpenGL renderer string"), "gl_version": pick("OpenGL version string"),
            "gl_vendor": pick("OpenGL vendor string"), "glxinfo": raw}


def is_software(renderer: str) -> bool:
    return any(s in renderer.lower() for s in SOFTWARE_RENDERERS)


def check_renderer(gl: dict, allow_software: bool) -> None:
    """GPU에서 재는지 측정 전에 확인한다. 소프트웨어 렌더러의 수치는 GPU 성능이 아니다."""
    renderer = gl["gl_renderer"]
    if not renderer:
        sys.exit("GL 렌더러를 알 수 없다. glxinfo가 없거나 화면(DISPLAY)에 연결되지 않았다.\n"
                 "  sudo apt-get install -y mesa-utils 후 glxinfo -B 로 확인한다 (README의 '측정 환경')")
    print(f"GL 렌더러: {renderer}")
    if is_software(renderer):
        if not allow_software:
            sys.exit(f"소프트웨어 렌더러({renderer})라 GPU 성능을 잴 수 없다. README의 '측정 환경'을 따르거나, "
                     "흐름만 볼 때는 --allow-software")
        print("경고: 소프트웨어 렌더러로 잰다. 이 결과는 표에 쓰지 않는다")


def cmake_build(src: Path, build_dir: Path, cmake_args: list[str], jobs: int, quiet: bool = False) -> None:
    out = subprocess.DEVNULL if quiet else None
    subprocess.run(["cmake", "-S", str(src), "-B", str(build_dir), *cmake_args], check=True, stdout=out)
    subprocess.run(["cmake", "--build", str(build_dir), "-j", str(jobs)], check=True, stdout=out)


def pinned_meno() -> str:
    """bench_app/CMakeLists.txt가 받는 meno 커밋. 결과 폴더 이름과 빌드 확인에 쓴다."""
    text = (BENCH_APP / "CMakeLists.txt").read_text(encoding="utf-8")
    m = re.search(r"Meno\.git\s+GIT_TAG\s+([0-9a-f]{40})", text)
    if not m:
        sys.exit("bench_app/CMakeLists.txt에서 meno의 GIT_TAG(커밋 해시)를 찾지 못했다")
    return m.group(1)


def build_bench(build_dir: Path, cmake_args: list[str], jobs: int) -> Path:
    """bench_app을 빌드하고, 실제로 받은 meno가 고정한 커밋인지 확인한 뒤 실행 파일 경로를 돌려준다."""
    cmake_build(BENCH_APP, build_dir, cmake_args, jobs)
    built = git_info(build_dir / "_deps" / "meno-src")["commit"]
    if built and built != pinned_meno():
        sys.exit(f"빌드한 meno({built[:7]})가 고정한 커밋({pinned_meno()[:7]})과 다르다. {build_dir}를 지우고 다시 빌드한다")
    return build_dir / "meno_bench"


def bench_versions(build_dir: Path | None) -> dict:
    """env.json에 남길 코드 버전: 측정한 meno와 측정 프로그램(이 저장소의 bench_app/)."""
    meno_src = build_dir / "_deps" / "meno-src" if build_dir else None
    if meno_src and meno_src.exists():
        meno = git_info(meno_src)
    else:  # --bin으로 받은 실행 파일이나 mock: 고정한 커밋을 적는다
        meno = {"commit": pinned_meno(), "describe": "", "dirty": False}
    return {"meno": meno, "bench_app": git_info(HERE, "bench_app")}


def gpu_slug(renderer: str) -> str:
    """결과 폴더 이름에 붙일 GPU 이름. "D3D12 (NVIDIA GeForce RTX 5070 Ti)" → "rtx-5070-ti",
    "D3D12 (AMD Radeon(TM) Graphics)" → "radeon". GPU가 여럿인 PC에서 결과 폴더가 겹치지 않게 한다."""
    m = re.match(r"D3D12 \((.*)\)$", renderer.strip())
    words = re.findall(r"[a-z0-9]+", (m.group(1) if m else renderer).lower())
    drop = {"nvidia", "geforce", "amd", "tm", "r", "graphics", "intel", "corporation", "mesa"}
    return "-".join([w for w in words if w not in drop][:4]) or "gpu"


def result_dir(mock: bool, gl: dict) -> Path:
    """기본 결과 폴더: results/<날짜>-<meno 커밋>-<GPU>/ (mock은 results-mock/<날짜>-mock/)."""
    if mock:
        return HERE / "results-mock" / f"{dt.date.today():%Y%m%d}-mock"
    return HERE / "results" / f"{dt.date.today():%Y%m%d}-{pinned_meno()[:7]}-{gpu_slug(gl.get('gl_renderer', ''))}"


# --- 결과 폴더 --------------------------------------------------------------

def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}


def read_runs(out: Path) -> list[dict]:
    return [json.loads(l) for l in (out / "runs.jsonl").read_text(encoding="utf-8").splitlines()]


def ok_runs(runs: list[dict]) -> set[tuple]:
    """성공한 (대상, N, 반복)의 집합. 중단 뒤 다시 돌려 성공한 것도 포함한다."""
    return {(r["target"], r["count"], r["rep"]) for r in runs if r["ok"]}


def raw_path(out: Path, target: str, count: int, rep: int) -> Path:
    return out / "raw" / f"{target}-{count}-rep{rep}.csv"


def read_frame_ms(path: Path) -> list[float]:
    """실행 파일 약속의 CSV(frame,frame_ms)에서 프레임 시간만 읽는다. 검사는 run.py가 기록할 때 한다."""
    with path.open(newline="") as f:
        return [float(r["frame_ms"]) for r in csv.DictReader(f)]


def target_order(out: Path, present: set[str]) -> list[str]:
    """표·그림·문서의 대상 순서. bench.toml의 targets 순서(env.json에 기록됨)를 따르고, 첫 대상이 기준이다.
    이어지는 대상은 앞 대상에 한 단계를 얹은 것이므로, 이웃한 두 대상의 비가 곧 그 단계의 비용이다."""
    order = read_json(out / "env.json").get("config", {}).get("sweep", {}).get("targets", [])
    return [t for t in order if t in present] + sorted(present - set(order))
