"""동결 잠금 파일(freeze.lock.json)을 만들거나 확인한다.

사용 (llm-comparison/ 에서):
    python3 harness/freeze.py check   # 일관성 검사 + 현재 입력이 잠금과 같은지 확인
    python3 harness/freeze.py write   # 일관성 검사를 통과하면 현재 입력의 해시를 잠금 파일에 기록 (동결)

잠금 대상은 회차 결과를 바꿀 수 있는 모든 입력이다: 설정, 지시문, 템플릿, 명세, 문맥 목록과 묶음, 에셋,
채점표, 하니스(harness/ 폴더 전체). 본 실행(run.py --games main)은 잠금 파일이 없거나 입력이 잠금과 다르면 시작하지 않는다.

일관성 검사:
- 템플릿의 meno 커밋과 문맥 목록(manifest.json)의 meno 커밋이 같다 (빌드하는 코드와 LLM에게 주는 문서가 같은 버전).
- 두 템플릿의 SFML 커밋이 manifest.json의 SFML/SFML 커밋과 같다 (두 조건이 같은 SFML로 빌드됨).
- 템플릿의 모든 GIT_TAG가 40자리 커밋 해시다.
- 동결 전에 끝낼 일: specs/sim/ 시뮬레이션 스크립트(D8), 채점표 항목 문장이 지금 명세와 같음.
- 문맥 묶음의 출처 머리글이 manifest.json의 저장소·커밋·파일 목록과 같다 (목록을 바꾸고 묶음을 다시 만들지 않은 경우를 잡음).
- 모델 ID가 정해져 있다 (TBD 없음).
"""
from __future__ import annotations

import csv
import datetime as dt
import hashlib
import json
import re
import sys
import tomllib
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
LOCK = BASE / "freeze.lock.json"
# harness/는 폴더째 잠근다. 회차 결과를 바꾸는 코드만 이 폴더에 둔다 (결과를 읽는 도구는 analysis/, 채점은 grading/).
# 파일을 하나하나 적으면, 새 모듈을 만들고 목록에 넣는 것을 잊었을 때 동결 검사를 조용히 빠져나간다.
FROZEN = [
    "experiment.toml", "requirements.txt", "prompts", "templates", "specs/games",
    "contexts/manifest.json", "contexts/bundles", "assets", "grading/sheets",
    "harness",
]


def file_hashes() -> dict[str, str]:
    hashes: dict[str, str] = {}
    for entry in FROZEN:
        path = BASE / entry
        files = sorted(p for p in path.rglob("*") if p.is_file()) if path.is_dir() else [path]
        for file in files:
            if "__pycache__" in file.parts or file.suffix == ".pyc":
                continue
            hashes[file.relative_to(BASE).as_posix()] = hashlib.sha256(file.read_bytes()).hexdigest()
    return hashes


def inputs_hash(hashes: dict[str, str] | None = None) -> str:
    hashes = hashes or file_hashes()
    return hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()


COMMIT = re.compile(r"^[0-9a-f]{40}$")


def declared_tags(cmake_text: str) -> dict[str, str]:
    """CMakeLists.txt의 FetchContent_Declare(<이름> ... GIT_TAG <값> ...)를 {이름: 값}으로 돌려준다."""
    tags: dict[str, str] = {}
    for name, body in re.findall(r"FetchContent_Declare\(\s*(\w+)(.*?)\)", cmake_text, re.S):
        tag = re.search(r"GIT_TAG\s+(\S+)", body)
        if tag:
            tags[name] = tag.group(1)
    return tags


def template_tags(cfg: dict) -> dict[str, dict[str, str]]:
    """조건별 템플릿의 {의존성 이름: GIT_TAG}."""
    return {c: declared_tags((BASE / cond["template_dir"] / "CMakeLists.txt").read_text(encoding="utf-8"))
            for c, cond in cfg["conditions"].items()}


# rationale.md의 무입력 실행 근거가 가리키는 시뮬레이션 스크립트 (D8). 동결 태그에 함께 남아야 수치를 다시 확인할 수 있다.
SIM_SCRIPTS = ("chase.py", "tank.py", "survivor_noinput.py")


def prerequisite_problems(cfg: dict) -> list[str]:
    """동결 전에 끝나 있어야 하는 일. 잠근 뒤에는 고치면 전부 다시 돌려야 하므로 일찍 잠그지 못하게 한다."""
    problems = []
    missing = [name for name in SIM_SCRIPTS if not (BASE / "specs" / "sim" / name).exists()]
    if missing:
        problems.append(f"specs/sim/에 시뮬레이션 스크립트가 없음: {', '.join(missing)} (D8)")
    # 채점표의 모든 항목 문장이 지금 명세에 있어야 한다. 명세를 고치고 채점표를 다시 만들지 않은 경우를 잡는다.
    # 팀이 항목을 합칠 때는 make_sheets.py처럼 명세 문장을 " / "로 이어 붙이므로, 조각마다 명세에 있는지 본다.
    for game in [*cfg["games"]["main"], *cfg["games"].get("pilot", [])]:
        sheet = BASE / "grading" / "sheets" / f"{game}.csv"
        spec = BASE / "specs" / "games" / f"{game}.md"
        if not sheet.exists():
            problems.append(f"채점표가 없음: grading/sheets/{game}.csv")
            continue
        text = " ".join(spec.read_text(encoding="utf-8").split())
        with sheet.open(encoding="utf-8-sig", newline="") as f:
            stale = [row["id"] for row in csv.DictReader(f)
                     if any(" ".join(part.split()) not in text for part in row["requirement"].split(" / "))]
        if stale:
            problems.append(f"grading/sheets/{game}.csv의 항목 {len(stale)}개가 지금 명세 문장과 다름 "
                            f"(예: {', '.join(stale[:3])}). 명세를 고쳤다면 채점표를 다시 만든다")
    return problems


def consistency_problems() -> list[str]:
    problems: list[str] = []
    cfg = tomllib.loads((BASE / "experiment.toml").read_text(encoding="utf-8"))
    manifest = json.loads((BASE / "contexts" / "manifest.json").read_text(encoding="utf-8"))
    tags = template_tags(cfg)

    meno_tag = tags["meno"].get("meno")
    manifest_meno = manifest["meno"][0]["commit"]
    if meno_tag != manifest_meno:
        problems.append(f"템플릿의 meno 커밋({meno_tag[:7] if meno_tag else '없음'})과 "
                        f"manifest.json의 meno 커밋({manifest_meno[:7]})이 다름")

    # 두 조건이 같은 SFML 커밋으로 빌드되고, LLM에게 주는 SFML readme도 그 커밋에서 나와야 한다.
    manifest_sfml = next((s["commit"] for s in manifest["sfml"] if s["repo"] == "SFML/SFML"), None)
    for condition in ("meno", "sfml"):
        sfml_tag = tags[condition].get("SFML")
        if sfml_tag != manifest_sfml:
            problems.append(f"{condition} 템플릿의 SFML({sfml_tag or '선언 없음'})이 "
                            f"manifest.json의 SFML/SFML 커밋({(manifest_sfml or '없음')[:7]})과 다름")

    for condition, deps in tags.items():
        for name, value in deps.items():
            if not COMMIT.match(value):
                problems.append(f"{condition} 템플릿의 {name}이 커밋 해시가 아님({value}). 태그는 옮겨질 수 있음")

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

    problems += prerequisite_problems(cfg)

    if any("TBD" in m["id"] or m.get("model") == "TBD" for m in cfg["models"]):
        problems.append("experiment.toml의 모델이 아직 TBD")
    for m in cfg["models"]:
        if m.get("backend", "litellm") != "litellm" and not m.get("effort"):
            problems.append(f"{m['id']}의 effort가 없음. 구독 CLI는 기본 추론 수준이 바뀔 수 있어 고정한다")
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
