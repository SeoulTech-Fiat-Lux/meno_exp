"""동결 잠금 파일(freeze.lock.json)을 만들거나 확인한다.

사용 (llm-comparison/ 에서):
    python3 harness/freeze.py check   # 일관성 검사 + 현재 입력이 잠금과 같은지 확인
    python3 harness/freeze.py write   # 일관성 검사를 통과하면 현재 입력의 해시를 잠금 파일에 기록 (동결)

잠금 대상은 회차 결과를 바꿀 수 있는 모든 입력이다: 설정, 지시문, 템플릿, 명세, 문맥 목록과 묶음, 에셋,
채점표, 하니스. 본 실행(run.py --games main)은 잠금 파일이 없거나 입력이 잠금과 다르면 시작하지 않는다.

일관성 검사:
- 템플릿의 meno 커밋과 문맥 목록(manifest.json)의 meno 커밋이 같다 (빌드하는 코드와 LLM에게 주는 문서가 같은 버전).
- 문맥 묶음의 출처 머리글이 manifest.json의 저장소·커밋·파일 목록과 같다 (목록을 바꾸고 묶음을 다시 만들지 않은 경우를 잡음).
- 모델 ID가 정해져 있다 (TBD 없음).
"""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import re
import sys
import tomllib
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
LOCK = BASE / "freeze.lock.json"
FROZEN = [
    "experiment.toml", "requirements.txt", "prompts", "templates", "specs/games",
    "contexts/manifest.json", "contexts/bundles", "assets", "grading/sheets",
    "harness/run.py", "harness/run_noinput.sh", "harness/frametime_stats.py",
]


def file_hashes() -> dict[str, str]:
    hashes: dict[str, str] = {}
    for entry in FROZEN:
        path = BASE / entry
        files = sorted(p for p in path.rglob("*") if p.is_file()) if path.is_dir() else [path]
        for file in files:
            if "__pycache__" in file.parts:
                continue
            hashes[file.relative_to(BASE).as_posix()] = hashlib.sha256(file.read_bytes()).hexdigest()
    return hashes


def inputs_hash(hashes: dict[str, str] | None = None) -> str:
    hashes = hashes or file_hashes()
    return hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()


def consistency_problems() -> list[str]:
    problems: list[str] = []
    cfg = tomllib.loads((BASE / "experiment.toml").read_text(encoding="utf-8"))
    manifest = json.loads((BASE / "contexts" / "manifest.json").read_text(encoding="utf-8"))

    template = (BASE / cfg["conditions"]["meno"]["template_dir"] / "CMakeLists.txt").read_text(encoding="utf-8")
    tag = re.search(r"GIT_TAG\s+(\S+)", template)
    manifest_meno = manifest["meno"][0]["commit"]
    if not tag or tag.group(1) != manifest_meno:
        problems.append(f"템플릿의 meno 커밋({tag.group(1)[:7] if tag else '없음'})과 "
                        f"manifest.json의 meno 커밋({manifest_meno[:7]})이 다름")

    for condition, sources in manifest.items():
        if condition == "rule":
            continue
        expected = [f"{s['repo']}@{s['commit'][:7]}:{f}" for s in sources for f in s["files"]]
        bundle = BASE / cfg["conditions"][condition]["context"]
        if not bundle.exists():
            problems.append(f"문맥 묶음이 없음: {bundle.relative_to(BASE)}")
            continue
        actual = re.findall(r"^<<< (.+?) >>>$", bundle.read_text(encoding="utf-8"), re.M)
        if actual != expected:
            problems.append(f"{bundle.relative_to(BASE)}가 manifest.json과 다름 (build_context.py로 다시 만들 것)")

    if any(m["id"].startswith("TBD") for m in cfg["models"]):
        problems.append("experiment.toml의 모델 ID가 아직 TBD")
    return problems


def check(verbose: bool = True) -> bool:
    """잠금 파일이 있고 현재 입력이 같으면 True."""
    ok = True
    for problem in consistency_problems():
        ok = False
        if verbose:
            print(f"일관성: {problem}")
    if not LOCK.exists():
        if verbose:
            print("잠금 파일이 없음 (아직 동결하지 않음)")
        return False
    lock = json.loads(LOCK.read_text(encoding="utf-8"))
    current = file_hashes()
    changed = sorted(k for k in set(lock["files"]) | set(current) if lock["files"].get(k) != current.get(k))
    if changed:
        ok = False
        if verbose:
            print(f"잠금 이후 바뀐 파일 {len(changed)}개:")
            for name in changed[:20]:
                print(f"  {name}")
    if ok and verbose:
        print(f"동결 상태와 같음 (inputs {lock['inputs_sha256'][:12]}, {lock['created_at']})")
    return ok


def write() -> None:
    problems = consistency_problems()
    if problems:
        for problem in problems:
            print(f"일관성: {problem}")
        sys.exit("일관성 문제를 먼저 고친다. 잠금 파일을 만들지 않았다.")
    hashes = file_hashes()
    LOCK.write_text(json.dumps({
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"),
        "inputs_sha256": inputs_hash(hashes),
        "files": hashes,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"잠금 파일을 만들었다: {LOCK.relative_to(BASE)} (파일 {len(hashes)}개, inputs {inputs_hash(hashes)[:12]})")


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else "check"
    if command == "write":
        write()
    else:
        sys.exit(0 if check() else 1)
