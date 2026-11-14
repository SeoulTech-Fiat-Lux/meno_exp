"""빌드에 성공한 회차를 무작위 ID로 묶어 블라인드 채점용 폴더를 만든다.

사용 (llm-comparison/ 에서):
    python3 grading/blind.py <runs 폴더> <출력 폴더> <대응표 CSV 경로(저장소 밖)>
예:
    python3 grading/blind.py runs ../blind ~/meno-exp-mapping.csv

출력 폴더에는 <ID>/game, <ID>/assets(공용 에셋 링크), <ID>/sheet.csv(해당 게임 채점표 사본)가 생긴다.
채점자는 `cd <ID> && ./game`으로 실행한다. 창 제목으로 게임은 알 수 있지만, 조건과 모델은 대응표에만 있다.
대응표는 채점이 끝날 때까지 채점자가 볼 수 없는 곳(저장소 밖)에 둔다. 채점이 끝나면 grading/mapping.csv로 커밋한다.
"""
import csv
import json
import secrets
import shutil
import string
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
ALPHABET = string.ascii_lowercase + string.digits


def main() -> None:
    runs_dir, out_dir, mapping_path = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]).expanduser()
    if mapping_path.resolve().is_relative_to(BASE.parent.resolve()):
        sys.exit("대응표를 저장소 안에 두면 채점자가 볼 수 있다. 저장소 밖 경로를 준다.")
    out_dir.mkdir(parents=True, exist_ok=True)
    shared_assets = out_dir / "_assets"
    if not shared_assets.exists():
        shutil.copytree(BASE / "assets", shared_assets)

    records = [json.loads(line) for line in (runs_dir / "runs.jsonl").read_text().splitlines() if line.strip()]
    used, rows = set(), []
    for record in sorted(records, key=lambda r: secrets.randbelow(1 << 30)):  # 순서도 섞는다
        if not record["build_success"] or not (runs_dir / record["run_id"] / "game").exists():
            continue
        blind_id = "".join(secrets.choice(ALPHABET) for _ in range(6))
        while blind_id in used:
            blind_id = "".join(secrets.choice(ALPHABET) for _ in range(6))
        used.add(blind_id)
        target = out_dir / blind_id
        target.mkdir()
        shutil.copy(runs_dir / record["run_id"] / "game", target / "game")
        (target / "assets").symlink_to("../_assets")
        sheet = BASE / "grading" / "sheets" / f"{record['game']}.csv"
        if sheet.exists():
            shutil.copy(sheet, target / "sheet.csv")
        rows.append({"blind_id": blind_id, "run_id": record["run_id"], "game": record["game"],
                     "condition": record["condition"], "model": record["model_requested"],
                     "rep": record["rep"]})
    with mapping_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["blind_id", "run_id", "game", "condition", "model", "rep"])
        writer.writeheader()
        writer.writerows(rows)
    print(f"블라인드 폴더 {len(rows)}개 → {out_dir}, 대응표 → {mapping_path}")


if __name__ == "__main__":
    main()
