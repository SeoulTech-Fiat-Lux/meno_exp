"""세 사람이 따로 돌린 본 실행 결과를 하나의 runs/main으로 합치고, 블라인드 묶음을 만들기 전에 검사한다.

사용 (llm-comparison/ 에서, 블라인드 묶음을 만드는 사람이):
    python3 grading/merge_runs.py runs/main ~/incoming/claude ~/incoming/chatgpt1 ~/incoming/chatgpt2
    python3 grading/merge_runs.py runs/main --check     # 합친 뒤의 폴더(또는 한 사람의 폴더)를 검사만 한다

각 원본 폴더는 한 사람이 보낸 runs/main 압축을 푼 것이다 (runs-*.jsonl, 회차 폴더, 실행 파일 game 포함).

검사 (하나라도 걸리면 합치지 않고 종료 코드 1):
    - 모든 회차의 입력 해시가 같고, 동결 잠금(freeze.lock.json)과 같으며, 동결 상태에서 돌았다
    - 기록 형식(schema_version)이 이 코드가 읽을 수 있는 것이다
    - 같은 회차가 두 번 기록되지 않았다 (두 사람이 같은 몫을 돌린 경우)
    - 같은 이름의 기록 파일이 서로 다른 내용으로 오지 않았다
    - 회차마다 폴더가 있고, 빌드에 성공한 회차는 실행 파일(game)이 있다
    - experiment.toml의 본 실행 계획(모델 × 게임 × 조건 × 반복)과 비교해 빠지거나 남는 회차가 없다
"""
from __future__ import annotations

import argparse
import json
import shutil
import sys
import tomllib
from collections import Counter
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE / "harness"))
import records as runlog  # noqa: E402  회차 계획·회차 ID 규칙과 기록 형식 (하니스와 같은 모듈)


def expected_run_ids(cfg: dict) -> set[str]:
    return {r.run_id for r in runlog.main_plan(cfg)}


def check(sources: list[Path], cfg: dict) -> tuple[list[str], list[dict]]:
    """(문제 목록, 모든 회차 기록)을 돌려준다. 각 기록에 _source(원본 폴더)를 붙인다."""
    problems, records = [], []
    for source in sources:
        for record in runlog.load_records(source, required=False):
            records.append(record | {"_source": source})
        if not runlog.run_logs(source):
            problems.append(f"{source}: 기록 파일(runs*.jsonl)이 없음")

    lock_path = BASE / "freeze.lock.json"
    lock = json.loads(lock_path.read_text(encoding="utf-8"))["inputs_sha256"] if lock_path.exists() else None
    if lock is None:
        problems.append("freeze.lock.json이 없음: 동결하지 않은 입력으로 돈 결과는 본 실행으로 쓸 수 없음")
    inputs = Counter(r.get("inputs_sha256") for r in records)
    if len(inputs) > 1:
        detail = ", ".join(f"{(h or '없음')[:12]} {n}회차" for h, n in inputs.items())
        problems.append(f"입력 해시가 여러 개: {detail}. 누군가 다른 버전의 저장소로 돌림")
    for h in inputs:
        if lock and h != lock:
            problems.append(f"입력 해시 {(h or '없음')[:12]}가 동결 잠금 {lock[:12]}과 다름")
    unfrozen = [r["run_id"] for r in records if not r.get("frozen")]
    if unfrozen:
        problems.append(f"동결 상태가 아닐 때 돈 회차 {len(unfrozen)}개 (예: {unfrozen[0]})")

    ids = Counter(r["run_id"] for r in records)
    for run_id, n in ids.items():
        if n > 1:
            where = sorted({str(r["_source"]) for r in records if r["run_id"] == run_id})
            problems.append(f"회차가 {n}번 기록됨: {run_id} ({', '.join(where)})")

    names: dict[str, bytes] = {}
    for source in sources:
        for log in runlog.run_logs(source):
            data = log.read_bytes()
            if log.name in names and names[log.name] != data:
                problems.append(f"같은 이름의 기록 파일이 내용이 다름: {log.name}")
            names[log.name] = data

    for r in records:
        folder = r["_source"] / r["run_id"]
        if not folder.is_dir():
            problems.append(f"회차 폴더가 없음: {folder}")
        elif r.get("build_success") and not (folder / "game").exists():
            problems.append(f"빌드에 성공했는데 실행 파일이 없음: {folder}/game (압축할 때 빠졌는지 확인)")

    expected = expected_run_ids(cfg)
    missing, extra = sorted(expected - set(ids)), sorted(set(ids) - expected)
    if missing:
        problems.append(f"계획에 있는데 기록이 없는 회차 {len(missing)}개 (예: {', '.join(missing[:3])})")
    if extra:
        problems.append(f"계획에 없는 회차 {len(extra)}개 (예: {', '.join(extra[:3])}). experiment.toml이 다른 버전일 수 있음")
    return problems, records


def merge(sources: list[Path], dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    for source in sources:
        if source.resolve() == dest.resolve():
            continue
        for log in runlog.run_logs(source):
            shutil.copy2(log, dest / log.name)
        for folder in sorted(p for p in source.iterdir() if p.is_dir()):
            target = dest / folder.name
            if target.exists():
                raise SystemExit(f"이미 있는 회차 폴더: {target}. 합치기를 다시 하려면 빈 폴더에서 시작한다.")
            shutil.copytree(folder, target, symlinks=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dest", type=Path, help="합칠 곳 (보통 runs/main)")
    parser.add_argument("sources", type=Path, nargs="*", help="사람마다 보낸 runs/main 폴더")
    parser.add_argument("--check", action="store_true", help="합치지 않고 dest만 검사")
    args = parser.parse_args()
    cfg = tomllib.loads((BASE / "experiment.toml").read_text(encoding="utf-8"))
    sources = [args.dest] if args.check else [s.expanduser() for s in args.sources]
    if not sources:
        parser.error("원본 폴더를 주거나 --check를 쓴다")
    problems, records = check(sources, cfg)
    built = sum(bool(r.get("build_success")) for r in records)
    print(f"회차 {len(records)}개 (빌드 성공 {built}개), 계획 {len(expected_run_ids(cfg))}개")
    if problems:
        print("\n".join(f"문제: {p}" for p in problems))
        sys.exit(1)
    if not args.check:
        merge(sources, args.dest)
        print(f"합침 → {args.dest}. 다음: python3 grading/blind.py {args.dest} ...")
    else:
        print("문제 없음")


if __name__ == "__main__":
    main()
