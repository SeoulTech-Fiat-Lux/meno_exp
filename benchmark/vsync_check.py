"""수직 동기화가 정말 꺼지는지 확인한다 (SeoulTech-Fiat-Lux/Meno#30).

본 측정 전에 측정할 PC에서 한 번 돌린다. 실행 파일 약속대로 vsync를 꺼도, 드라이버나 화면 합성기(WSLg 등)가
동기화를 강제하면 프레임 시간이 주사율 주기(60Hz면 16.7ms)에 묶인다. 그러면 N을 바꿔도 프레임 시간이 거의
그대로라 측정 전체가 쓸모없어진다.

방법: vsync_check/를 빌드하고, 사각형 몇 개만 그리는 창을 vsync 끔·켬으로 한 번씩 돌린다.
판정은 끔으로 한다: 프레임 시간이 한 주사율 주기에 촘촘히 모여 있으면 실패다.
켬은 대조군이다: 이 환경에서 동기화가 실제로 걸리는지(묶임을 감지할 수 있는지) 함께 보고한다.

사용 (benchmark/ 에서, 측정과 같은 셸 환경 변수로):
    python3 vsync_check.py                  # 결과를 출력. 묶여 있으면 종료 코드 1
    python3 vsync_check.py --out results/<…>   # 결과 폴더에 vsync.json도 남긴다
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tomllib
from pathlib import Path

import run as runner

HERE = runner.HERE
# 흔한 주사율의 한 프레임 주기(ms). 켬일 때 이 근처에 머물면 동기화가 동작한다는 뜻이다.
REFRESH_MS = {60: 1000 / 60, 75: 1000 / 75, 90: 1000 / 90, 120: 1000 / 120, 144: 1000 / 144, 165: 1000 / 165,
              240: 1000 / 240}


def nearest_refresh(ms: float) -> tuple[int, float]:
    hz, period = min(REFRESH_MS.items(), key=lambda kv: abs(kv[1] - ms))
    return hz, abs(period - ms) / period


def locked(r: dict) -> tuple[int, bool]:
    """프레임 시간이 한 주사율 주기에 촘촘히 모여 있으면 동기화에 묶인 것이다."""
    hz, err = nearest_refresh(r["median_ms"])
    period = REFRESH_MS[hz]
    # 묶인 프레임은 주기에서 1~2%만 흔들린다. 허용 폭을 넓히면 우연히 주기 근처인 측정까지 잡힌다.
    return hz, err < 0.02 and abs(r["p10_ms"] - period) / period < 0.03


def verdict(on: dict, off: dict, on_unsupported: bool) -> tuple[bool, str]:
    """판정은 끔 결과로 한다. 켬은 이 환경에서 동기화가 걸릴 수 있는지 보여 주는 참고 값이다."""
    hz_off, off_locked = locked(off)
    hz_on, on_locked = locked(on)
    if off_locked:
        return False, (f"실패: 끔인데도 {off['median_ms']:.2f} ms로 {hz_off} Hz 주기에 묶여 있다. "
                       "드라이버나 화면 합성기가 동기화를 강제한다. README의 '수직 동기화 확인'을 따른다")
    if on_locked:
        control = f"대조군: 켜면 {on['median_ms']:.2f} ms({hz_on} Hz)에 묶이므로, 묶임을 감지할 수 있는 환경이다"
    elif on_unsupported:
        control = (f"대조군: 이 환경은 vsync 요청을 지원하지 않아 켜도 {on['median_ms']:.2f} ms다"
                   "(WSLg의 d3d12 등). 프레임이 늘 동기화 없이 돈다")
    else:
        control = f"대조군: 켜도 {on['median_ms']:.2f} ms로 묶이지 않았다. 드라이버가 vsync 요청을 무시한다"
    return True, (f"통과: 끔 {off['median_ms']:.2f} ms(p10 {off['p10_ms']:.2f}, p90 {off['p90_ms']:.2f})로 "
                  f"어느 주사율 주기에도 묶여 있지 않다. {control}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", type=Path, default=HERE / "bench.toml")
    ap.add_argument("--out", type=Path, help="결과 폴더. 주면 vsync.json을 남긴다")
    ap.add_argument("--allow-software", action="store_true")
    args = ap.parse_args()

    cfg = tomllib.loads(args.config.read_text(encoding="utf-8"))
    env = os.environ | cfg["run"].get("env", {})
    gl = runner.gl_info(env)
    runner.check_renderer(gl, args.allow_software)

    build = HERE / "work" / "vsync-build"
    subprocess.run(["cmake", "-S", str(HERE / "vsync_check"), "-B", str(build)], check=True,
                   stdout=subprocess.DEVNULL)
    subprocess.run(["cmake", "--build", str(build), "-j", str(cfg["build"]["jobs"])], check=True,
                   stdout=subprocess.DEVNULL)

    results, unsupported = {}, False
    overrides = cfg["run"].get("env", {})
    for mode in ("on", "off"):
        # 켬(대조군)은 측정용 덮어쓰기(vblank_mode=0 등) 없이 돌린다. 덮어쓰면 켬도 강제로 꺼진다.
        mode_env = {k: v for k, v in env.items() if k not in overrides} if mode == "on" else env
        p = subprocess.run([str(build / "vsync_check"), "--vsync", mode], env=mode_env, capture_output=True,
                           text=True, timeout=120)
        if mode == "on":
            unsupported = "vertical sync not supported" in p.stderr
        if p.returncode != 0:
            sys.exit(f"vsync_check --vsync {mode} 실패 (종료 코드 {p.returncode}): {p.stderr[-1000:]}")
        results[mode] = json.loads(p.stdout.strip().splitlines()[-1])
        r = results[mode]
        print(f"vsync {mode:<3}: 중앙값 {r['median_ms']:.3f} ms (p10 {r['p10_ms']:.3f}, p90 {r['p90_ms']:.3f})")

    ok, message = verdict(results["on"], results["off"], unsupported)
    print(message)
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
        record = {"ok": ok, "message": message, "gl_renderer": gl["gl_renderer"],
                  "env": {k: v for k, v in env.items() if k in cfg["run"].get("env", {}) or
                          k.startswith(("GALLIUM_", "MESA_", "LIBGL_", "__GL_"))}, **results}
        (args.out / "vsync.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
