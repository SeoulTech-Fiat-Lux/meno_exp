"""빌드에 성공한 회차를 무작위 ID로 묶어 블라인드 채점용 폴더를 만들고, 채점자를 배정한다.

사용 (llm-comparison/ 에서):
    python3 grading/blind.py <runs 폴더> <출력 폴더> <대응표 CSV 경로(저장소 밖)> --graders 가,나,다 [--per-run 2]
예:
    python3 grading/blind.py runs/main ~/blind ~/sealed/meno-exp-mapping.csv --graders A,B,C

출력 폴더:
    <ID>/game, <ID>/assets(공용 에셋 링크)   채점자는 `cd <ID> && ./game`으로 실행한다
    <ID>/sheet-<채점자>.csv                 배정된 채점자마다 따로 쓰는 채점표 (같은 파일을 덮어쓰지 않게)
    <ID>/meta-<채점자>.csv                  어느 조건 같은지 추측, 확신, 채점에 쓴 시간 (블라인드 점검·부담 측정)
    assignments.csv                         ID별 채점자 배정표 (조건·모델 정보 없음)
창 제목으로 게임은 알 수 있지만, 조건과 모델은 대응표에만 있다.
대응표는 채점이 끝날 때까지 아무도 열지 않는다. 저장소 안 경로는 거부한다. 채점이 끝나면 grading/mapping.csv로 커밋한다.
"""
import argparse
import csv
import itertools
import json
import secrets
import shutil
import string
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
ALPHABET = string.ascii_lowercase + string.digits


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("runs_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("mapping", type=Path)
    parser.add_argument("--graders", required=True, help="쉼표로 나눈 채점자 이름")
    parser.add_argument("--per-run", type=int, default=2, help="회차마다 채점자 수")
    args = parser.parse_args()

    mapping_path = args.mapping.expanduser()
    if mapping_path.resolve().is_relative_to(BASE.parent.resolve()):
        parser.exit(1, "대응표를 저장소 안에 두면 채점자가 볼 수 있다. 저장소 밖 경로를 준다.\n")
    graders = [g.strip() for g in args.graders.split(",") if g.strip()]
    if args.per_run > len(graders):
        parser.exit(1, "회차마다 채점자 수가 채점자 수보다 많다.\n")

    out_dir = args.out_dir.expanduser()
    out_dir.mkdir(parents=True, exist_ok=True)
    shared_assets = out_dir / "_assets"
    if not shared_assets.exists():
        shutil.copytree(BASE / "assets", shared_assets)

    # 사람마다 기록 파일이 따로 있다 (runs.jsonl, runs-<모델>-<몫>.jsonl). 모두 모은다.
    records = [json.loads(line) for path in sorted(args.runs_dir.glob("runs*.jsonl"))
               for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    records = [r for r in records if r["build_success"] and (args.runs_dir / r["run_id"] / "game").exists()]
    # 채점자 조합을 돌려 가며 배정해 부담을 고르게 한다. 순서도 무작위로 섞는다.
    pairs = itertools.cycle(itertools.combinations(graders, args.per_run))
    used, mapping_rows, assignment_rows = set(), [], []
    for record in sorted(records, key=lambda r: secrets.randbelow(1 << 30)):
        blind_id = "".join(secrets.choice(ALPHABET) for _ in range(6))
        while blind_id in used:
            blind_id = "".join(secrets.choice(ALPHABET) for _ in range(6))
        used.add(blind_id)
        target = out_dir / blind_id
        target.mkdir()
        shutil.copy(args.runs_dir / record["run_id"] / "game", target / "game")
        (target / "assets").symlink_to("../_assets")
        sheet = BASE / "grading" / "sheets" / f"{record['game']}.csv"
        for grader in next(pairs):
            if sheet.exists():
                shutil.copy(sheet, target / f"sheet-{grader}.csv")
            # 블라인드 점검과 채점 부담을 재는 칸. 채점이 끝난 뒤 적는다 (PLAN 2.4절).
            with (target / f"meta-{grader}.csv").open("w", encoding="utf-8-sig", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["guess_condition", "confidence", "minutes", "note"])
                writer.writerow(["", "", "", ""])
                writer.writerow(["# guess_condition: meno / sfml / 모름, confidence: 1(찍음)~3(확실), minutes: 채점에 쓴 분"])
            assignment_rows.append({"blind_id": blind_id, "game": record["game"], "grader": grader})
        mapping_rows.append({"blind_id": blind_id, "run_id": record["run_id"], "game": record["game"],
                             "condition": record["condition"], "model": record["model_requested"],
                             "rep": record["rep"]})

    mapping_path.parent.mkdir(parents=True, exist_ok=True)
    with mapping_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(mapping_rows[0]) if mapping_rows else ["blind_id"])
        writer.writeheader()
        writer.writerows(mapping_rows)
    with (out_dir / "assignments.csv").open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["blind_id", "game", "grader"])
        writer.writeheader()
        writer.writerows(sorted(assignment_rows, key=lambda r: (r["grader"], r["blind_id"])))
    load = {g: sum(1 for r in assignment_rows if r["grader"] == g) for g in graders}
    print(f"블라인드 폴더 {len(mapping_rows)}개 → {out_dir}, 대응표 → {mapping_path}, 채점자별 회차 {load}")


if __name__ == "__main__":
    main()
