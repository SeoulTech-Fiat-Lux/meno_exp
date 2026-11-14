"""대표작을 고르고, 플레이·녹화할 수 있는 폴더를 만든다 (PLAN 2.4절).

사용 (llm-comparison/ 에서, 채점이 끝나고 run_scores.csv가 생긴 뒤):
    python3 analysis/showcase.py runs/main grading/results/run_scores.csv ~/showcase

고르는 규칙 (게임 × 조건마다 하나, 결과를 보기 전에 정함):
    1. 요구사항 통과율이 가장 높은 회차 (빌드한 회차 중)
    2. 같으면 빌드 성공까지의 왕복 수가 적은 회차
    3. 그래도 같으면 출력 토큰이 적은 회차
    4. 그래도 같으면 회차 ID 순서
출력: showcase.csv와, <게임>-<조건>/ 폴더마다 실행 파일 game과 assets(공용 _assets 링크).

녹화: 무입력 실행(run_noinput.sh)은 보이지 않는 화면(Xvfb)에서 실행되므로 소리가 없고 사람이 조작할 수 없다. 대표작 영상은
WSL에서 `cd <폴더> && ./game`으로 띄워(WSLg가 창과 소리를 Windows로 보냄) 직접 플레이하면서, Windows의
게임 바(Win+Alt+R, 활성 창과 시스템 소리 녹화)로 녹화한다. 영상과 실행 파일은 git에 넣지 않고 Releases나 공유 드라이브에 둔다.
"""
from __future__ import annotations

import argparse
import csv
import shutil
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(BASE / "harness"))
import records as runlog  # noqa: E402


def pick(records: list[dict], scores: dict[str, float]) -> list[dict]:
    best: dict[tuple, dict] = {}
    for r in records:
        if not r.get("build_success") or r["run_id"] not in scores:
            continue
        key = (-scores[r["run_id"]], r.get("rounds") or 99, r.get("completion_tokens") or 0, r["run_id"])
        cell = (r["game"], r["condition"])
        if cell not in best or key < best[cell]["_key"]:
            best[cell] = r | {"_key": key, "pass_rate": scores[r["run_id"]]}
    return [best[cell] for cell in sorted(best)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("runs_dir", type=Path)
    parser.add_argument("scores", type=Path, help="grading/results/run_scores.csv")
    parser.add_argument("out", type=Path, help="플레이·녹화용 폴더를 만들 곳 (저장소 밖 권장)")
    args = parser.parse_args()
    records = runlog.load_records(args.runs_dir)
    with args.scores.open(encoding="utf-8-sig", newline="") as f:
        scores = {row["run_id"]: float(row["pass_rate"]) for row in csv.DictReader(f)}
    chosen = pick(records, scores)

    out = args.out.expanduser()
    out.mkdir(parents=True, exist_ok=True)
    shared = out / "_assets"
    if not shared.exists():
        shutil.copytree(BASE / "assets", shared)
    rows = []
    for r in chosen:
        folder = out / f"{r['game']}-{r['condition']}"
        folder.mkdir(exist_ok=True)
        shutil.copy2(args.runs_dir / r["run_id"] / "game", folder / "game")
        if not (folder / "assets").exists():
            (folder / "assets").symlink_to("../_assets")
        rows.append({"game": r["game"], "condition": r["condition"], "run_id": r["run_id"],
                     "model": r["model_requested"], "pass_rate": f"{r['pass_rate']:.3f}", "rounds": r.get("rounds"),
                     "folder": folder.name})
    with (out / "showcase.csv").open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["game", "condition", "run_id", "model", "pass_rate", "rounds", "folder"],
                               lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    for row in rows:
        print(f"{row['game']:12} {row['condition']:5} 통과율 {row['pass_rate']} 왕복 {row['rounds']}  {row['run_id']}")
    print(f"\n→ {out}/showcase.csv. 녹화: cd {out}/<폴더> && ./game 을 띄우고 Windows에서 Win+Alt+R")


if __name__ == "__main__":
    main()
