"""요구사항 pptx를 md로 옮긴다.

사용: python3 -I pptx2md.py <pptx 폴더> <출력 specs 폴더>

- 제목·본문 자리표시자는 들여쓰기 수준을 지켜 목록으로 옮긴다.
- 'Map text' 상자는 코드 블록으로 옮긴다.
- 'Screen …' 도형(화면 배치 예시)과 슬라이드 번호는 버린다.
- 발표자 노트는 LLM에게 주지 않는 rationale.md로 모은다.
- EDITS의 문구 수정을 적용하고, 적용 여부를 CHANGES로 기록한다.
"""
import re
import sys
from pathlib import Path

from pptx import Presentation
from pptx.enum.shapes import PP_PLACEHOLDER

GAMES = [
    ("MiniGame_Req1_Pong", "01-pong"),
    ("MiniGame_Req2_Snake", "02-snake"),
    ("MiniGame_Req3_Flappy", "03-flappy"),
    ("MiniGame_Req4_Shooter", "04-shooter"),
    ("MiniGame_Req5_Card", "05-card"),
    ("MiniGame_Req6_Maze", "06-maze"),
    ("MiniGame_Req7_Rocks", "07-rocks"),
    ("MiniGame_Req8_Formation", "08-formation"),
    ("MiniGame_Req9_Tank", "09-tank"),
    ("MiniGame_Req10_Missile", "10-missile"),
    ("MiniGame_Req11_Survivor", "11-survivor"),
]

# (찾을 문구, 바꿀 문구, 이유). 모든 문서에 적용하며, 적용된 횟수를 기록한다.
EDITS = [
    # md에는 그림이 없으므로 '오른쪽 예시' 문장을 뺀다. 화면 배치는 수치로 이미 명세돼 있다.
    ("오른쪽은 화면 배치 예시이며, 색은 '화면 색' 슬라이드와 제공된 에셋을 따름.",
     "색은 '화면 색' 절과 제공된 에셋을 따름.",
     "md에는 화면 예시 그림이 없음"),
    # 상대 경로는 작업 디렉터리 기준으로 열린다. 실행 파일 위치를 구하는 코드를 유도하지 않도록 명확히 한다.
    ("에셋 경로는 실행 파일 기준 상대 경로 assets/…임.",
     "에셋은 상대 경로 assets/…로 엶. 실행 파일은 assets/와 같은 폴더에 있으며, 그 폴더를 작업 디렉터리로 하여 실행함.",
     "상대 경로의 기준(작업 디렉터리) 명확화"),
    # 슬라이드 → 절
    ("이어지는 슬라이드", "이어지는 절", "md 구조에 맞춤"),
    ("앞 슬라이드와", "앞 절과", "md 구조에 맞춤"),
    ("앞 슬라이드", "앞 절", "md 구조에 맞춤"),
    ("'화면 색' 슬라이드", "'화면 색' 절", "md 구조에 맞춤"),
    # 팀 내부 문서: #35로 이미 보완된 기능
    ("보완 예정인 다각형 그리기의 검증 대상이기도 함.",
     "#35에서 보완한 다각형 외곽선 그리기의 검증 대상이기도 함.",
     "#35 완료 반영"),
    ("보완 예정인 원호·런타임 색의 검증 대상이기도 함.",
     "#35에서 보완한 런타임 색(폭발 원의 두 색 번갈아 쓰기)의 검증 대상이기도 함.",
     "#35 완료 반영. 미사일 방어 명세에 원호가 없어 '원호'는 뺌"),
    ("계산 근거는 각 게임 요구사항 문서의 비기능 슬라이드 노트에 있음.",
     "계산 근거는 rationale.md에 있음.",
     "노트를 rationale.md로 옮김"),
    # 노트의 시뮬레이션 경로
    ("scratchpad/sim/", "sim/", "시뮬레이션 스크립트를 저장소의 specs/sim/에 둔다는 가정"),
]

CHANGES: dict[str, list[str]] = {}


def edit(text: str, doc: str) -> str:
    for old, new, why in EDITS:
        if old in text:
            text = text.replace(old, new)
            CHANGES.setdefault(doc, []).append(f"{old} → {new} ({why})")
    return text


def body_lines(shape, doc: str) -> list[str]:
    lines = []
    for p in shape.text_frame.paragraphs:
        text = "".join(r.text for r in p.runs).strip()
        if not text:
            continue
        lines.append("  " * p.level + "- " + edit(text, doc))
    return lines


def convert(pptx: Path, doc: str, notes_out: list[str]) -> str:
    prs = Presentation(pptx)
    out: list[str] = []
    for index, slide in enumerate(prs.slides, start=1):
        title = ""
        body: list[str] = []
        code: list[str] = []
        for shape in slide.shapes:
            if shape.is_placeholder:
                kind = shape.placeholder_format.type
                if kind in (PP_PLACEHOLDER.TITLE, PP_PLACEHOLDER.CENTER_TITLE):
                    title = shape.text_frame.text.strip()
                elif kind in (PP_PLACEHOLDER.BODY, PP_PLACEHOLDER.SUBTITLE, PP_PLACEHOLDER.OBJECT):
                    body += body_lines(shape, doc)
            elif shape.name.startswith("Map text") and shape.has_text_frame:
                code.append(shape.text_frame.text.replace("\v", "\n").strip("\n"))
        if index == 1:
            out.append(f"# {title}\n")
            out += [line[2:] for line in body]  # 부제는 목록이 아닌 한 줄
            out.append("")
            continue
        out.append(f"## {title}\n")
        out += body
        for block in code:
            out += ["", "```text", block, "```"]
        out.append("")
        if slide.has_notes_slide:
            note = slide.notes_slide.notes_text_frame.text.strip()
            if note:
                notes_out.append(f"### {title}\n\n{edit(note, doc)}\n")
    return "\n".join(out).rstrip() + "\n"


def main() -> None:
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    (dst / "games").mkdir(parents=True, exist_ok=True)
    rationale = ["# 검증 근거\n",
                 "요구사항 문서의 수치(무입력 실행 시각, 판정 여유 등)를 정한 근거다. "
                 "팀 내부 자료이며 LLM에게 주지 않는다.\n"]
    for stem, name in GAMES:
        notes: list[str] = []
        md = convert(src / f"{stem}.pptx", name, notes)
        (dst / "games" / f"{name}.md").write_text(md, encoding="utf-8")
        if notes:
            heading = md.splitlines()[0].lstrip("# ").strip()
            rationale.append(f"## {heading}\n")
            rationale += notes
    team_notes: list[str] = []
    (dst / "team-internal.md").write_text(
        convert(src / "MiniGame_Team_Internal.pptx", "team-internal", team_notes), encoding="utf-8")
    (dst / "rationale.md").write_text("\n".join(rationale).rstrip() + "\n", encoding="utf-8")

    for doc, items in CHANGES.items():
        counted: dict[str, int] = {}
        for item in items:
            counted[item] = counted.get(item, 0) + 1
        print(f"[{doc}]")
        for item, n in counted.items():
            print(f"  {n}× {item}")


if __name__ == "__main__":
    main()
