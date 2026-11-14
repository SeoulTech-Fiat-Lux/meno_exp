"""실패한 빌드 로그의 첫 오류를 유형으로 나눈다 (PLAN 2.4절, 탐색적 분석).

유형 (위에서부터 처음 맞는 것을 고른다):
    sfml2_api       SFML 3에 SFML 2 방식의 API를 쓴 경우 (이벤트, 기본 생성자, Angle, 열거형 범위 등)
    sfml_in_meno    meno 조건에서 SFML 헤더를 include한 경우 (규칙 위반이며, 경로가 없어 컴파일되지 않는다)
    meno_api        meno API를 잘못 쓴 경우 (오류 문장에 meno::가 나온다)
    link            링크 오류
    cpp             그 밖의 일반 C++ 오류
규칙은 시범 빌드 로그를 보며 다듬는다. 바꾸면 그 사실을 보고서에 적는다. 단독 실행:
    python3 analysis/errors.py runs/pilot-<해시>
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

# 패턴은 "|"로 이어 하나의 정규식이 된다. 그래서 묶음은 (?:…)만 쓰고 역참조(\1)는 쓰지 않는다
# (이어 붙이면 묶음 번호가 밀려 역참조가 다른 묶음을 가리킨다. 기본 생성자 패턴이 그래서 맞지 않았다).
SFML2_PATTERNS = [
    r"has no member named '?type'?",                                   # event.type (3에서는 event->is<T>())
    r"pollEvent\(sf::Event&\)",                                        # 3에서는 pollEvent()가 optional을 돌려준다
    r"'?(?:Closed|KeyPressed|KeyReleased|MouseButtonPressed|MouseMoved|TextEntered)'? is not a member of 'sf::Event'",
    r"is not a member of 'sf::(?:Keyboard|Mouse)'",                     # sf::Keyboard::A → sf::Keyboard::Key::A
    r"has no member named '?(?:setLoop|loadFromFile|getLoop)'?",        # setLooping, openFromFile
    r"no matching function for call to 'sf::(?:Text::Text|Sprite::Sprite|Sound::Sound)\(\)'",  # 기본 생성자 없음
    r"sf::VideoMode::VideoMode\(int, int",                             # VideoMode({w, h})
    r"(?:could not convert|cannot convert) .* to 'sf::Angle'",         # setRotation(sf::degrees(x))
    r"has no member named '?intersects'?",                             # findIntersection
    r"'sf::(?:Color|Vector2f|FloatRect)'.*(?:no matching|no member)",
]
SFML2 = re.compile("|".join(SFML2_PATTERNS))
assert SFML2.groups == 0, "SFML2_PATTERNS에는 캡처 묶음을 쓰지 않는다"
SFML_HEADER = re.compile(r"SFML/[\w/]+\.hpp: No such file")
MENO = re.compile(r"meno::")
LINK = re.compile(r"undefined reference|ld returned|collect2")
ERROR_LINE = re.compile(r"\b(error|fatal error):")


def classify(log: str, condition: str) -> tuple[str, str]:
    """(유형, 첫 오류 줄)을 돌려준다."""
    lines = [line for line in log.splitlines() if ERROR_LINE.search(line) or LINK.search(line)]
    if not lines:
        return "cpp", ""
    first = lines[0]
    context = "\n".join(lines[:3])  # 첫 오류와 바로 뒤의 설명 몇 줄
    if condition == "meno" and SFML_HEADER.search(context):
        return "sfml_in_meno", first
    if condition == "sfml" and SFML2.search(context):
        return "sfml2_api", first
    if condition == "meno" and MENO.search(first):
        return "meno_api", first
    if LINK.search(first):
        return "link", first
    return "cpp", first


def classify_runs(runs_dir: Path, records: list[dict]) -> list[dict]:
    """실패한 모든 빌드 왕복의 첫 오류 유형."""
    rows = []
    for record in records:
        for detail in record.get("rounds_detail") or []:
            if not detail.get("code_found") or detail.get("build_ok"):
                continue
            log_path = runs_dir / record["run_id"] / f"build_{detail['round']}.log"
            if not log_path.exists():
                continue
            category, first = classify(log_path.read_text(encoding="utf-8", errors="replace"), record["condition"])
            rows.append({"run_id": record["run_id"], "model": record["model_requested"], "game": record["game"],
                         "condition": record["condition"], "round": detail["round"], "category": category,
                         "first_error": first[:300]})
    return rows


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "harness"))
    import records as runlog  # 형식은 harness/records.py에 있다
    runs_dir = Path(sys.argv[1])
    for row in classify_runs(runs_dir, runlog.load_records(runs_dir)):
        print(f"{row['condition']:5} {row['category']:13} {row['run_id']} r{row['round']}: {row['first_error'][:120]}")
