"""게임별 요구사항 md에서 채점표 초안(CSV)을 만든다.

사용 (llm-comparison/ 에서): python3 grading/make_sheets.py [게임 이름...]
출력: grading/sheets/<게임>.csv

- 항목 하나 = md의 최상위 목록 한 줄. 하위 목록은 그 항목의 문장에 이어 붙인다.
- 개요 절(게임 개발, 주요 기능·비기능 요구사항), 정보 절(게임 실행 환경, 제공 에셋), 지시 절(주의 사항)은 뺀다.
- 프레임 시간처럼 측정 하니스로만 확인하는 문장은 "harness"로 표시한다.
출력은 초안이다. 팀 내부 문서의 항목 수(예: 퐁 기능 12·비기능 3)와 맞도록 사람이 나누거나 합친 뒤 확정한다.
확정한 채점표는 본 실행 전에 커밋하고, 그 뒤에는 바꾸지 않는다.
"""
import csv
import re
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
SKIP = ("게임 개발", "Major Functional Requirements", "Major Non-functional Requirements",
        "게임 실행 환경", "제공 에셋", "주의 사항")
COMMON = ("화면 흐름과 공통 조작", "코드와 빌드", "실행 품질")
HARNESS = re.compile(r"프레임 시간|측정 하니스|비정상 종료|종료 코드|오류 없이 빌드")


def items(md: str):
    section, current = None, None
    for line in md.splitlines():
        if line.startswith("## "):
            if current:
                yield current
            section, current = line[3:].strip(), None
            continue
        if section is None or any(section.endswith(s) for s in SKIP):
            continue
        if line.startswith("- "):
            if current:
                yield current
            current = [section, line[2:].strip()]
        elif line.startswith("  ") and line.strip().startswith("- ") and current:
            current[1] += " / " + line.strip()[2:]
    if current:
        yield current


def main() -> None:
    games = sys.argv[1:] or [p.stem for p in sorted((BASE / "specs" / "games").glob("*.md"))]
    out = BASE / "grading" / "sheets"
    out.mkdir(parents=True, exist_ok=True)
    for game in games:
        md = (BASE / "specs" / "games" / f"{game}.md").read_text(encoding="utf-8")
        prefix = game.split("-", 1)[1].upper()
        rows = []
        for index, (section, text) in enumerate(items(md), start=1):
            kind = "공통" if section in COMMON else "게임별"
            how = "harness" if HARNESS.search(text) else "play"
            rows.append({"id": f"{prefix}-{index:02d}", "kind": kind, "section": section,
                         "requirement": text, "how": how, "result": "", "note": ""})
        with (out / f"{game}.csv").open("w", encoding="utf-8-sig", newline="") as f:
            # csv 모듈의 기본 줄바꿈은 CRLF다. 저장소(LF)와 바이트가 달라지면 동결 잠금의 해시가 PC마다 달라진다.
            writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
        counts = {k: sum(1 for r in rows if r["kind"] == k) for k in ("게임별", "공통")}
        print(f"{game}: 항목 {len(rows)}개 (게임별 {counts['게임별']}, 공통 {counts['공통']}, "
              f"harness {sum(1 for r in rows if r['how'] == 'harness')})")


if __name__ == "__main__":
    main()
