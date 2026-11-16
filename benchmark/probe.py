"""측정 환경 조사: GL 렌더러, 코드 버전(git), CPU·전원 상태. 결과 폴더의 env.json과 runs.jsonl에 남는다.

실행 도구(run.py, tracy.py, vsync_check.py)가 쓴다. 조사 도구가 없거나 실패해도 측정을 멈추지 않고 빈 값을
남긴다. 단, GPU에서 재는지(check_renderer)는 측정 전에 확인하고 아니면 멈춘다.
"""
from __future__ import annotations

import datetime as dt
import os
import platform
import re
import select
import subprocess
import sys
import time
from pathlib import Path
from typing import NamedTuple

from common import HERE, pinned_meno

SOFTWARE_RENDERERS = ("llvmpipe", "softpipe", "swrast")
# 렌더러 선택에 영향을 주는 환경 변수. WSL에서는 이 값에 따라 어느 GPU로 재는지가 정해진다.
GL_ENV_PREFIXES = ("GALLIUM_", "MESA_", "LIBGL_", "__GL_")


def sh(cmd: list[str], **kw) -> str:
    """환경 기록용. 도구가 없거나 실패해도 멈추지 않고 빈 문자열을 돌려준다."""
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=30, **kw).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


# --- 코드 버전 --------------------------------------------------------------

def git_info(repo: Path, path: str = ".") -> dict:
    """path를 주면 그 경로가 고쳐졌는지만 본다(이 저장소의 bench_app/처럼 저장소 일부만 볼 때)."""
    return {
        "commit": sh(["git", "-C", str(repo), "rev-parse", "HEAD"]),
        # --dirty는 저장소 전체를 본다. path를 주었을 때는 붙이지 않는다(그 경로만 본 결과는 "dirty"에 있다).
        "describe": sh(["git", "-C", str(repo), "describe", "--tags", "--always",
                        *(["--dirty"] if path == "." else [])]),
        # 커밋하지 않은 변경이 있는 채로 잰 결과는 어느 코드로 쟀는지 재현할 수 없으므로 표시해 둔다.
        "dirty": bool(sh(["git", "-C", str(repo), "status", "--porcelain", "--", path])),
    }


def bench_versions(build_dir: Path | None) -> dict:
    """측정한 meno와 측정 프로그램(이 저장소의 bench_app/)의 버전."""
    meno_src = build_dir / "_deps" / "meno-src" if build_dir else None
    if meno_src and meno_src.exists():
        meno = git_info(meno_src)
    else:  # --bin으로 받은 실행 파일이나 mock: 고정한 커밋을 적는다
        meno = {"commit": pinned_meno(), "describe": "", "dirty": False}
    return {"meno": meno, "bench_app": git_info(HERE, "bench_app")}


# --- GL 렌더러 --------------------------------------------------------------

def gl_env(env: dict) -> dict:
    return {k: v for k, v in sorted(env.items()) if k.startswith(GL_ENV_PREFIXES) or k == "vblank_mode"}


def gl_info(env: dict) -> dict:
    """glxinfo -B에서 렌더러·버전만 뽑는다. 전체 출력에는 남은 비디오 메모리처럼 실행마다 바뀌는 값이
    있으므로, 이어서 돌릴 때 같은 환경인지는 이 값들로만 비교한다."""
    # 실행 파일과 같은 환경 변수로 조회해야 실행 파일이 실제로 쓸 렌더러를 알 수 있다(WSL의 GALLIUM_DRIVER 등).
    raw = sh(["glxinfo", "-B"], env=env)

    def pick(name: str) -> str:
        return next((l.split(":", 1)[1].strip() for l in raw.splitlines() if l.strip().startswith(name + ":")), "")

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


def probe_gl(env: dict, mock: bool, allow_software: bool) -> dict:
    """실행 도구가 측정 전에 하는 렌더러 조회와 확인. mock이면 조회하지 않고 빈 dict를 돌려준다."""
    if mock:
        return {}
    gl = gl_info(env)
    check_renderer(gl, allow_software)
    return gl


# --- CPU 상태 ---------------------------------------------------------------
# 그리기 호출을 하나씩 보내는 비용은 CPU가 정한다. WSL 안에서는 CPU 클럭을 정하는 Windows 전원 설정도,
# Windows 프로그램(브라우저 등)이 쓰는 CPU도 보이지 않으므로 Windows에 직접 묻는다.

# Windows 11 "전원 모드"(설정 > 시스템 > 전원). 레지스트리에 값이 없으면 기본값(균형)이다.
POWER_MODES = {"ded574b5-45a0-4f42-8737-46345c09c238": "최고 성능",
               "961cc777-2547-4f9d-8174-7d86181b8a7a": "최고의 전원 효율성",
               "00000000-0000-0000-0000-000000000000": "균형"}


def windows_power() -> dict:
    """Windows 전원 계획, 전원 모드, 최대·최소 프로세서 상태(%). Windows가 아니면(powercfg.exe가 없으면) 빈 dict."""
    scheme = sh(["powercfg.exe", "/getactivescheme"])
    if not scheme:
        return {}

    def state(alias: str) -> dict:
        # 출력 끝의 두 값이 현재 AC(전원 연결), DC(배터리) 설정이다. 앞의 0x 값들은 허용 범위.
        vals = re.findall(r"0x([0-9a-fA-F]{8})", sh(["powercfg.exe", "/query", "SCHEME_CURRENT", "SUB_PROCESSOR", alias]))
        return {"ac": int(vals[-2], 16), "dc": int(vals[-1], 16)} if len(vals) >= 2 else {}

    m = re.search(r"([0-9a-fA-F-]{36})\s+\((.*)\)", scheme)
    mode = re.search(r"REG_SZ\s+\{?([0-9a-fA-F-]{36})", sh(
        ["reg.exe", "query", r"HKLM\SYSTEM\CurrentControlSet\Control\Power\User\PowerSchemes",
         "/v", "ActiveOverlayAcPowerScheme"]))
    guid = mode.group(1).lower() if mode else ""
    return {"scheme": m.group(2) if m else scheme, "scheme_guid": m.group(1) if m else "",
            "power_mode": POWER_MODES.get(guid, guid) if guid else "균형 (기본값)",
            "max_processor_state": state("PROCTHROTTLEMAX"), "min_processor_state": state("PROCTHROTTLEMIN")}


class HostLoad:
    """Windows 전체 CPU 사용률(%)을 측정 직전마다 묻는다. PowerShell은 띄울 때마다 1.5초쯤 걸리고 CPU도 쓰므로
    한 번 띄워 두고 다시 쓴다. Windows가 아니거나 TIMEOUT_S 안에 답이 없으면 None을 돌려주고 더 묻지 않는다."""
    QUERY = "(Get-CimInstance Win32_Processor | Measure-Object LoadPercentage -Average).Average\n"
    TIMEOUT_S = 10  # 한 번에 약 1초 걸린다. 답이 없을 때 측정 전체가 멈추지 않게 한다

    def __init__(self) -> None:
        try:
            self.p = subprocess.Popen(["powershell.exe", "-NoProfile", "-NoLogo", "-Command", "-"], text=True,
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        except OSError:
            self.p = None

    def sample(self) -> float | None:
        if not self.p:
            return None
        try:
            self.p.stdin.write(self.QUERY)
            self.p.stdin.flush()
            ready, _, _ = select.select([self.p.stdout], [], [], self.TIMEOUT_S)
            if ready:
                return float(self.p.stdout.readline())
        except (OSError, ValueError):
            pass
        print("Windows CPU 사용률을 읽지 못해 더 묻지 않는다 (host_cpu_pct 없이 기록)")
        self.close()
        return None

    def close(self) -> None:
        if self.p:
            try:
                self.p.stdin.close()
                self.p.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.p.kill()
            self.p = None


class CpuSnapshot(NamedTuple):
    """한 시점의 CPU 누적 사용. 두 시점의 차로 측정 한 번 동안의 사용을 낸다(cpu_usage)."""
    at: float                # time.monotonic()
    busy_s: float | None     # WSL 전체에서 바빴던 CPU 초(/proc/stat). 읽지 못하면 None
    children_s: float        # 끝난 자식 프로세스(측정 프로그램)의 CPU 초


def cpu_snapshot() -> CpuSnapshot:
    import resource  # 리눅스 전용. 맨 위에 두면 report.py(is_software만 씀)를 Windows Python에서 돌릴 수 없다

    try:
        f = [int(x) for x in Path("/proc/stat").read_text().split("\n", 1)[0].split()[1:9]]
        busy = (sum(f) - f[3] - f[4]) / os.sysconf("SC_CLK_TCK")  # user..steal에서 idle, iowait를 뺌
    except (OSError, ValueError):
        busy = None
    ch = resource.getrusage(resource.RUSAGE_CHILDREN)
    return CpuSnapshot(time.monotonic(), busy, ch.ru_utime + ch.ru_stime)


def cpu_usage(before: CpuSnapshot, after: CpuSnapshot) -> dict:
    """측정 한 번 동안 쓴 CPU(코어 수). self_cores는 측정 프로그램(모든 스레드), other_cores는 WSL 안의 나머지다.
    other_cores에는 화면 합성처럼 측정이 일으키는 몫도 들어간다(report.py의 OTHER_CORES_LIMIT)."""
    wall = after.at - before.at
    own = (after.children_s - before.children_s) / wall
    if before.busy_s is None or after.busy_s is None:
        return {"self_cores": round(own, 2), "other_cores": None}
    busy = (after.busy_s - before.busy_s) / wall
    return {"self_cores": round(own, 2), "other_cores": round(max(busy - own, 0.0), 2)}


# --- env.json ---------------------------------------------------------------

def environment(build_dir: Path | None, gl: dict) -> dict:
    """결과 폴더의 env.json. 이어서 돌릴 때 같은 PC·코드인지 비교하는 데도 쓴다(run.py)."""
    env = {
        "recorded_at": dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cpu": next((l.split(":", 1)[1].strip() for l in sh(["lscpu"]).splitlines()
                     if l.startswith("Model name")), platform.processor()),
        "nproc": os.cpu_count(),
        **gl,
        "gcc": sh(["gcc", "--version"]).splitlines()[:1],
        "cmake": sh(["cmake", "--version"]).splitlines()[:1],
        "wsl": "microsoft" in platform.release().lower(),
        "display": os.environ.get("DISPLAY", ""),
        "gl_env": gl_env(dict(os.environ)),
        "cpu_governor": sh(["cat", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"]),
        # WSL에서는 위 governor가 비고 CPU 클럭은 Windows 전원 설정을 따른다.
        "windows_power": windows_power(),
    }
    env |= bench_versions(build_dir)
    if build_dir and (build_dir / "CMakeCache.txt").exists():
        cache = (build_dir / "CMakeCache.txt").read_text(errors="replace").splitlines()
        env["cmake_cache"] = {l.split(":", 1)[0]: l.split("=", 1)[1] for l in cache
                              if l.startswith(("CMAKE_BUILD_TYPE:", "CMAKE_CXX_COMPILER:", "BENCH_ENABLE_TRACY:"))}
    return env
