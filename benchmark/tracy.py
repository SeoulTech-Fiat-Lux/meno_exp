"""Tracy 병목 분석 (SeoulTech-Fiat-Lux/Meno#30).

측정(run.py)과 따로, Tracy를 켠 bench_app 빌드(BENCH_ENABLE_TRACY)로 meno-object를 캡처하고 구간(zone)별
비용을 표·그림으로 낸다. Meno 코드는 고치지 않으므로, bench_app이 Game::run()이 하는 일을 같은 순서의
공개 API 호출로 직접 하고(--loop direct) 호출 사이에 구간을 넣는다. Tracy를 켠 빌드의 수치는 측정 표에 쓰지 않는다.

사용 (benchmark/ 에서):
    python3 tracy.py tools                          # bench.toml의 Tracy 버전으로 tracy-capture, tracy-csvexport 빌드
    python3 tracy.py capture                        # Tracy 빌드 → N마다 캡처 → CSV로 내보내기 → 보고서
    python3 tracy.py capture --bin <Tracy를 켠 실행 파일>
    python3 tracy.py report <tracy 폴더>            # 내보낸 CSV로 보고서만 다시 만들기
    python3 tracy.py capture --mock                 # Tracy 없이 보고서 흐름 점검 (results-mock/)

결과 (기본 results/<날짜>-<meno 커밋>-<GPU>/tracy/):
    <대상>-<N>.tracy          캡처 원본. 크므로 커밋하지 않는다(.gitignore). Tracy 프로그램으로 연다
    zones-<N>.csv             구간 호출마다 한 줄, self time (tracy-csvexport -u -e)
    frames-<N>.csv            "frame" 구간 호출마다 한 줄, 포함 시간 (tracy-csvexport -u -f frame)
    zones.md, zones.png       프레임당 구간별 비용 표와 그림 (자동 생성, 실행할 때마다 다시 만든다)
    zones.json                같은 값. report.py가 읽는다(원자료 없이도 보고서를 만들 수 있게)
    notes.md                  사람이 쓰는 분석. 없을 때만 양식을 만든다(덮어쓰지 않음)
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import random
import subprocess
import sys
import time
from pathlib import Path

import style
from build import build_bench, cmake_build
from common import HERE, bench_command, load_config, run_env
from probe import probe_gl
from results import read_json, result_dir


def tool_dir(version: str) -> Path:
    return HERE / "work" / f"tracy-{version}"


def build_tools(cfg: dict) -> Path:
    """Tracy 저장소를 고정 태그로 받아 캡처 도구 둘을 빌드한다. 이미 있으면 건너뛴다."""
    version, out = cfg["version"], tool_dir(cfg["version"])
    capture, export = out / "bin" / "tracy-capture", out / "bin" / "tracy-csvexport"
    if capture.exists() and export.exists():
        return out / "bin"
    src = out / "src"
    if not src.exists():
        subprocess.run(["git", "clone", "--depth", "1", "--branch", version, cfg["repo"], str(src)], check=True)
    for name in ("capture", "csvexport"):
        # 캡처 도구는 GUI가 필요 없다. NO_ISA_EXTENSIONS는 다른 PC로 옮겨도 실행되게 한다.
        cmake_build(src / name, out / f"build-{name}", ["-DCMAKE_BUILD_TYPE=Release", "-DNO_ISA_EXTENSIONS=ON"],
                    os.cpu_count() or 4)
    (out / "bin").mkdir(exist_ok=True)
    for name, dest in (("capture", capture), ("csvexport", export)):
        built = next((out / f"build-{name}").glob(f"tracy-{name}"), None) or next((out / f"build-{name}").rglob(f"tracy-{name}"))
        dest.write_bytes(built.read_bytes())
        dest.chmod(0o755)
    return out / "bin"


FRAME_ZONE = "frame"  # bench_app이 프레임 하나를 통째로 감싸는 구간 이름 (README의 "Tracy 병목 분석")


def export(bin_dir: Path, trace: Path, out: Path, n: int) -> None:
    # 합계가 아니라 호출마다 내보낸다(-u). 워밍업 프레임을 시간으로 잘라 내려면 각 호출의 시작 시각이 필요하다.
    for flags, name in ((["-u", "-e"], f"zones-{n}.csv"), (["-u", "-c", "-f", FRAME_ZONE], f"frames-{n}.csv")):
        p = subprocess.run([str(bin_dir / "tracy-csvexport"), *flags, str(trace)], capture_output=True, text=True)
        if p.returncode != 0:
            raise RuntimeError(f"tracy-csvexport {flags} 실패: {p.stderr.strip()}")
        (out / name).write_text(p.stdout, encoding="utf-8")


def capture_one(bin_dir: Path, binary: Path, run_cfg: dict, tcfg: dict, env: dict, out: Path, n: int) -> None:
    trace = out / f"{tcfg['target']}-{n}.tracy"
    # tracy-capture는 계측된 프로그램이 뜰 때까지 기다렸다가 붙고, 프로그램이 끝나면 파일을 쓰고 끝난다.
    cap = subprocess.Popen([str(bin_dir / "tracy-capture"), "-o", str(trace), "-f"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(0.5)
    csv_out = out / f".frames-{n}.csv"  # 실행 파일 약속상 필요하지만 Tracy 빌드의 수치는 쓰지 않는다
    cmd = bench_command(run_cfg, binary, tcfg["target"], n, 1, tcfg["warmup_frames"], tcfg["frames"],
                        csv_out) + tcfg.get("extra_args", [])
    # TRACY_NO_EXIT=1: 프로그램이 끝나도 캡처 도구가 데이터를 다 받을 때까지 기다린다.
    p = subprocess.run(cmd, env=env | {"TRACY_NO_EXIT": "1"}, capture_output=True, text=True,
                       timeout=run_cfg["timeout_s"])
    try:
        log, _ = cap.communicate(timeout=120)
    except subprocess.TimeoutExpired:
        cap.kill()
        raise RuntimeError("tracy-capture가 끝나지 않았다. 실행 파일이 Tracy를 켜고 빌드됐는지, Tracy 버전이 같은지 확인한다")
    csv_out.unlink(missing_ok=True)
    if p.returncode != 0:
        raise RuntimeError(f"실행 파일 종료 코드 {p.returncode}: {p.stderr[-1000:]}")
    if cap.returncode != 0 or not trace.exists():
        raise RuntimeError(f"캡처 실패: {log[-1000:]}")
    export(bin_dir, trace, out, n)
    print(f"N={n}: {trace.name} ({trace.stat().st_size / 1e6:.1f} MB)")


UNWRAP_HEADER = "name,src_file,src_line,ns_since_start,exec_time_ns,thread,value"


def mock_capture(tcfg: dict, out: Path, n: int) -> None:
    """Tracy 없이 보고서를 점검하려고 tracy-csvexport -u와 같은 형식의 가짜 CSV를 쓴다."""
    rng = random.Random(n)
    per_frame_us = {"app.events": 12, "app.update": 1, "bench.move": 0.008 * n, "bench.apply": 0.005 * n,
                    "renderer.begin": 120, "scene.draw": 1.0 * n, "window.display": 2300 + 0.15 * n}
    zones, frames, t = [UNWRAP_HEADER], [UNWRAP_HEADER], 0
    for f in range(tcfg["warmup_frames"] + tcfg["frames"] + 1):  # bench_app처럼 시작 프레임 하나를 더 돈다
        start = t
        slow = 30 if f == 0 else 1  # 첫 프레임은 창 생성·연결 때문에 길다. 워밍업에서 잘려야 한다
        for name, us in per_frame_us.items():
            d = int(us * 1000 * rng.uniform(0.9, 1.1) * slow)
            zones.append(f"{name},src/mock.cpp,1,{t},{d},1,")
            t += d
        gap = int(sum(per_frame_us.values()) * 80)  # 계측되지 않은 시간 약 8%
        zones.append(f"{FRAME_ZONE},src/mock.cpp,1,{start},{gap},1,")  # frame의 self time = 계측 안 된 시간
        t += gap
        frames.append(f"{FRAME_ZONE},src/mock.cpp,1,{start},{t - start},1,")
    (out / f"zones-{n}.csv").write_text("\n".join(zones) + "\n", encoding="utf-8")
    (out / f"frames-{n}.csv").write_text("\n".join(frames) + "\n", encoding="utf-8")


def read_unwrap(path: Path) -> list[tuple[str, int, int]]:
    with path.open(newline="", encoding="utf-8") as f:
        return [(r["name"], int(r["ns_since_start"]), int(r["exec_time_ns"])) for r in csv.DictReader(f)]


def load_capture(out: Path, n: int, warmup: int) -> tuple[float, dict[str, float], int]:
    """기록 프레임만으로 (프레임 평균 ms, 구간별 프레임당 self ms, 프레임 수)를 낸다.

    앞쪽 warmup + 1개의 frame 호출은 버리고(bench_app은 첫 프레임에서 시각만 찍고, 그다음 warmup개를 버린다), 남은 frame 호출이 덮는 시간 안에서 시작한 구간 호출만 센다.
    frame 구간의 self time은 프레임 안에서 다른 구간에 속하지 않은 시간, 곧 계측되지 않은 시간이다."""
    frames = sorted((s, d) for name, s, d in read_unwrap(out / f"frames-{n}.csv") if name == FRAME_ZONE)
    skip = warmup + 1
    if len(frames) <= skip:
        raise ValueError(f"frames-{n}.csv에 '{FRAME_ZONE}' 구간이 {len(frames)}개뿐이다(워밍업 {warmup}개). "
                         f"Tracy를 켜고 빌드한 bench_app인지(BENCH_ENABLE_TRACY), 프레임마다 BENCH_ZONE(\"{FRAME_ZONE}\")으로 감싸는지 확인한다")
    kept = frames[skip:]
    lo, hi = kept[0][0], kept[-1][0] + kept[-1][1]
    frame_ms = sum(d for _, d in kept) / len(kept) / 1e6
    zones: dict[str, float] = {}
    for name, start, d in read_unwrap(out / f"zones-{n}.csv"):
        if lo <= start < hi:
            zones[name] = zones.get(name, 0.0) + d / len(kept) / 1e6
    return frame_ms, zones, len(kept)


def report(out: Path, tcfg: dict) -> None:
    top = tcfg["top_zones"]
    counts = sorted(int(p.stem.split("-")[1]) for p in out.glob("zones-*.csv"))
    if not counts:
        sys.exit(f"{out}에 zones-*.csv가 없다. 원자료를 풀어 둔 뒤 돌린다(README의 \"원자료 보관\")")
    data = {n: load_capture(out, n, tcfg["warmup_frames"]) for n in counts}
    # 가장 큰 N에서 비싼 구간부터 이름을 붙이고, 나머지는 other로 묶는다. 순서를 N마다 바꾸지 않으므로 구간의 색이 고정된다.
    ranked = sorted((z for z in data[counts[-1]][1] if z != FRAME_ZONE), key=lambda z: -data[counts[-1]][1][z])
    named = ranked[:top]
    stacks = {}
    for n, (frame_ms, zones, _) in data.items():
        parts = {z: zones.get(z, 0.0) for z in named}
        parts["other zones"] = sum(v for z, v in zones.items() if z not in named and z != FRAME_ZONE)
        parts["not instrumented"] = zones.get(FRAME_ZONE, 0.0)
        stacks[n] = (frame_ms, parts)
    labels = named + ["other zones", "not instrumented"]

    lines = ["# Tracy 구간별 비용 (자동 생성)", "",
             f"워밍업 {tcfg['warmup_frames']}프레임을 뺀 기록 프레임(" +
             ", ".join(f"N={n:,}: {data[n][2]}개" for n in counts) + ")의 프레임당 self time(ms). "
             "구간끼리 겹치지 않으므로 더하면 프레임 시간이 된다. "
             f"`not instrumented`는 `{FRAME_ZONE}` 구간의 self time, 곧 어느 하위 구간에도 속하지 않은 시간이다.", "",
             "| 구간 | " + " | ".join(f"N={n:,} (ms)" for n in counts) + " | " +
             " | ".join(f"N={n:,} (%)" for n in counts) + " |",
             "|---|" + "---:|" * (2 * len(counts))]
    for z in labels:
        ms = [stacks[n][1][z] for n in counts]
        pct = [100 * stacks[n][1][z] / stacks[n][0] for n in counts]
        lines.append(f"| {z} | " + " | ".join(f"{v:.3f}" for v in ms) + " | " +
                     " | ".join(f"{v:.0f}" for v in pct) + " |")
    # 구간 합이 프레임 평균과 크게 다르면 다른 스레드의 구간이 섞였거나 프레임 밖의 구간이 있다는 뜻이다.
    for n in counts:
        total = sum(stacks[n][1].values())
        if abs(total - stacks[n][0]) > 0.05 * stacks[n][0]:
            lines.insert(4, f"> 주의: N={n:,}에서 구간 합 {total:.3f} ms가 프레임 평균 {stacks[n][0]:.3f} ms와 5% 넘게 다르다. "
                            "다른 스레드의 구간이나 프레임 밖 구간이 섞였는지 본다.\n")
    lines.append("| **프레임 평균** | " + " | ".join(f"**{stacks[n][0]:.3f}**" for n in counts) + " | " +
                 " | ".join("100" for _ in counts) + " |")
    if len(counts) >= 2:
        a, b = counts[0], counts[-1]
        lines += ["", f"## N={a:,} → N={b:,}에서 늘어난 비용", "",
                  "N에 비례해 늘어나는 구간이 배칭·최적화 대상이다. 거의 그대로인 구간은 고정 비용이다.", "",
                  "| 구간 | 늘어난 ms | 배수 |", "|---|---:|---:|"]
        for z in sorted(labels, key=lambda z: -(stacks[b][1][z] - stacks[a][1][z])):
            d = stacks[b][1][z] - stacks[a][1][z]
            d = 0.0 if abs(d) < 5e-4 else d  # -0.000 표시 방지
            ratio = stacks[b][1][z] / stacks[a][1][z] if stacks[a][1][z] > 0 else float("inf")
            lines.append(f"| {z} | {d:.3f} | {'-' if ratio == float('inf') else f'{ratio:.1f}×'} |")
    (out / "zones.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    # 사람이 읽는 zones.md의 문장은 바뀔 수 있다. report.py는 값을 이 파일에서 읽는다.
    record = {"target": tcfg["target"], "counts": counts, "frame_ms": {n: stacks[n][0] for n in counts},
              "zones": {z: {n: stacks[n][1][z] for n in counts} for z in labels}}
    (out / "zones.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("\n".join(lines))
    figure(out, counts, stacks, labels)
    notes_template(out, counts)


def figure(out: Path, counts: list[int], stacks: dict, labels: list[str]) -> None:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib가 없어 그림은 건너뜀 (pip install -r requirements.txt)")
        return
    # 구간 색: 이름 붙은 구간은 팔레트 순서대로 칠하고, 묶음 둘은 회색 계열로 낮춘다. 막대가 N마다 하나이므로 이웃한 칸만 구분되면 된다.
    style.apply(plt)
    colors = {z: style.SERIES[i % len(style.SERIES)] for i, z in enumerate(labels[:-2])}
    colors |= {"other zones": style.MUTED[0], "not instrumented": style.MUTED[1]}
    fig, axes = plt.subplots(len(counts), 1, figsize=(10, 1.0 + 0.9 * len(counts)), squeeze=False)
    for ax, n in zip(axes[:, 0], counts):
        frame_ms, parts = stacks[n]
        left = 0.0
        for z in labels:
            w = 100 * parts[z] / frame_ms
            ax.barh(0, w, left=left, height=0.6, color=colors[z], edgecolor=style.SURFACE, linewidth=2,
                    label=z if n == counts[0] else None)
            if w >= 6:
                ax.text(left + w / 2, 0, f"{w:.0f}%", ha="center", va="center", fontsize=8, fontweight="bold",
                        color=style.text_on(colors[z]))
            left += w
        ax.set(xlim=(0, 100), yticks=[0], yticklabels=[f"N={n:,}\n{frame_ms:.2f} ms"])
        ax.grid(axis="y", visible=False)
        ax.set_axisbelow(True)
        ax.tick_params(axis="y", length=0)
        if n != counts[-1]:
            ax.set_xticklabels([])
    axes[-1, 0].set_xlabel("share of mean frame time (%) — self time per zone")
    axes[0, 0].set_title("Tracy: where a meno-object frame goes", loc="left")
    fig.legend(loc="lower center", ncol=4, bbox_to_anchor=(0.5, -0.02), fontsize=8)
    fig.tight_layout(rect=(0, 0.12 if len(labels) > 4 else 0.08, 1, 1))
    fig.savefig(out / "zones.png", dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"그림: {out / 'zones.png'}")


def notes_template(out: Path, counts: list[int]) -> None:
    path = out / "notes.md"
    if path.exists():
        return
    path.write_text(f"""# Tracy 병목 분석 노트

자동 표와 그림은 `zones.md`, `zones.png`. 이 파일은 사람이 쓴다(다시 만들지 않음).

## 캡처 조건

- 날짜, 캡처한 사람:
- 환경 (GL 렌더러는 tracy.py capture가 `../env.json`과 같은지 확인함):
- Tracy 버전: (bench.toml `[tracy].version`과 bench_app/CMakeLists.txt의 tracy GIT_TAG가 같아야 함)
- N: {", ".join(f"{n:,}" for n in counts)}

## 관찰

- 가장 비싼 구간과 그 비율 (N이 가장 클 때):
- N에 비례해 늘어나는 구간 (`zones.md`의 "늘어난 비용"):
- 거의 그대로인 구간(고정 비용):
- `not instrumented`가 크면 어디가 빠졌는가:
- Tracy 화면 캡처 (`*.png`)와 그 설명:

## 판단

- 병목:
- 배칭으로 줄어들 부분 (그리기 호출 수에 비례하는 구간):
- 배칭으로 안 줄어드는 부분 (장면 순회, 변환 갱신 등):
- 결론과 다음 할 일 (SeoulTech-Fiat-Lux/Meno#30에 옮길 내용):
""", encoding="utf-8")
    print(f"양식: {path}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("tools", help="Tracy 캡처 도구 빌드")
    c = sub.add_parser("capture", help="캡처하고 보고서 만들기")
    c.add_argument("--bin", type=Path, help="Tracy를 켜고 빌드한 실행 파일. 주면 빌드하지 않는다")
    c.add_argument("--out", type=Path, help="기본 results/<날짜>-<meno 커밋>-<GPU>/tracy")
    c.add_argument("--mock", action="store_true")
    c.add_argument("--allow-software", action="store_true")
    r = sub.add_parser("report", help="내보낸 CSV로 보고서만 다시 만들기")
    r.add_argument("dir", type=Path)
    for p in sub.choices.values():
        p.add_argument("--config", type=Path, default=HERE / "bench.toml")
    args = ap.parse_args()

    cfg = load_config(args.config)
    tcfg = cfg["tracy"]
    if args.cmd == "tools":
        print(f"도구: {build_tools(tcfg)}")
        return
    if args.cmd == "report":
        report(args.dir, tcfg)
        return

    env = run_env(cfg)
    gl = probe_gl(env, args.mock, args.allow_software)
    out = args.out or result_dir(args.mock, gl) / "tracy"
    # 측정 결과 폴더 안에 캡처할 때는 같은 GPU인지 확인한다. 다른 GPU의 구간 비율이 그 폴더의 보고서에 섞이지 않게 한다.
    measured = read_json(out.parent / "env.json").get("gl_renderer")
    if gl and measured and measured != gl["gl_renderer"]:
        sys.exit(f"{out.parent}는 {measured}로 잰 결과인데 지금 렌더러는 {gl['gl_renderer']}다. "
                 "README의 '측정 환경'대로 GPU를 맞추거나 다른 --out을 쓴다")
    out.mkdir(parents=True, exist_ok=True)
    if args.mock:
        for n in tcfg["counts"]:
            mock_capture(tcfg, out, n)
    else:
        bin_dir = build_tools(tcfg)
        binary = args.bin
        if not binary:
            binary = build_bench(HERE / "work" / "build-tracy", tcfg["cmake_args"], cfg["build"]["jobs"])
        for n in tcfg["counts"]:
            capture_one(bin_dir, Path(binary), cfg["run"], tcfg, env, out, n)
    report(out, tcfg)


if __name__ == "__main__":
    main()
