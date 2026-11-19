"""결과 폴더: 이름 짓기와 읽기. 쓰는 쪽은 각 도구다(run.py는 raw/와 runs.jsonl, summarize.py는 summary.* 등).

결과 폴더 구조와 커밋하는 것은 README의 "결과 폴더에서 커밋하는 것"을 본다.
"""
from __future__ import annotations

import csv
import datetime as dt
import json
import re
import sys
from pathlib import Path

from common import HERE, pinned_meno


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


def require_raw(out: Path) -> None:
    """원자료(raw/)에서 결과를 다시 만드는 도구가 부른다. 원자료는 git 밖에 보관하므로, 없을 때 돌리면 커밋된
    결과를 빈 값으로 덮어쓰게 된다. 그 전에 멈춘다."""
    if not (out / "raw").is_dir():
        sys.exit(f"{out / 'raw'}가 없다. 원자료를 풀어 둔 뒤 돌린다(README의 \"4. 원자료를 보관하고 커밋한다\")")


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}


def read_runs(out: Path) -> list[dict]:
    """runs.jsonl을 읽는다. 아직 측정하지 않은 폴더면 빈 목록을 돌려준다."""
    path = out / "runs.jsonl"
    return [json.loads(l) for l in path.read_text(encoding="utf-8").splitlines()] if path.exists() else []


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
