"""블라인드 채점 결과를 모은다. 두 단계로 나눠 쓴다 (PLAN 2·3절).

1단계, 채점이 끝나면 (대응표 없이, 조건을 모르는 채로):
    python3 grading/collect.py agree ~/blind grading/results
    - judgments.csv      모든 채점자의 항목별 판정 (플레이 항목만)
    - agreement.md       채점자 일치도: 일치율, Cohen's κ, Gwet AC1 (전체와 게임별). 같은 값을 agreement.json으로도 쓴다
    - disagreements.csv  두 채점자의 판정이 어긋난 항목. 합의 회의에서 final 칸(통과/실패)을 채운다
    - incomplete.csv     비어 있는 판정 (이 칸들을 마저 채점한다)

2단계, 합의가 끝나면 (이때 처음으로 대응표를 연다):
    python3 grading/collect.py final ~/blind grading/results ~/sealed/meno-exp-mapping.csv
    - run_scores.csv     회차별 통과율 (analysis/analyze.py가 읽는다)

판정 표기는 통과/실패, pass/fail, P/F, O/X, 1/0을 받는다. 채점표에는 플레이 항목만 있다.
명세 문장과 채점하지 않는 항목(하니스·제외)의 이유는 grading/sheets/items.csv에 있다.
"""
from __future__ import annotations

import argparse
import csv
import itertools
import json
from collections import defaultdict
from pathlib import Path

PASS = {"통과", "pass", "p", "o", "1", "y", "yes", "true"}
FAIL = {"실패", "fail", "f", "x", "0", "n", "no", "false"}


def verdict(text: str | None) -> int | None:
    value = (text or "").strip().lower()
    if value in PASS:
        return 1
    if value in FAIL:
        return 0
    return None


def read_csv(path: Path) -> list[dict]:
    with path.open(encoding="utf-8-sig", newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def load_judgments(blind_dir: Path) -> tuple[list[dict], list[dict], dict]:
    """(판정, 빈 칸, 항목 정보)를 돌려준다. 게임은 채점표 항목 ID의 앞부분으로 알아낸다(조건 정보는 없다)."""
    judgments, blanks, items = [], [], {}
    for sheet in sorted(blind_dir.glob("*/sheet-*.csv")):
        blind_id, grader = sheet.parent.name, sheet.stem.removeprefix("sheet-")
        for row in read_csv(sheet):
            # 채점표에는 플레이 항목만 있다(나머지는 sheets/items.csv).
            items[(blind_id, row["id"])] = row
            value = verdict(row.get("result"))
            record = {"blind_id": blind_id, "item": row["id"], "game": row["id"].rsplit("-", 1)[0],
                      "grader": grader, "result": value}
            (judgments if value is not None else blanks).append(record)
    return judgments, blanks, items


def agreement(pairs: list[tuple[int, int]]) -> dict:
    """두 채점자의 이진 판정 짝으로 일치율, Cohen's κ, Gwet AC1을 계산한다."""
    n = len(pairs)
    if n == 0:
        return {"n": 0, "agreement": None, "kappa": None, "ac1": None}
    po = sum(a == b for a, b in pairs) / n
    pa, pb = sum(a for a, _ in pairs) / n, sum(b for _, b in pairs) / n
    pe_kappa = pa * pb + (1 - pa) * (1 - pb)
    q = (pa + pb) / 2
    pe_ac1 = 2 * q * (1 - q)
    kappa = (po - pe_kappa) / (1 - pe_kappa) if pe_kappa < 1 else None  # 모두 같은 값이면 κ가 정의되지 않는다
    ac1 = (po - pe_ac1) / (1 - pe_ac1) if pe_ac1 < 1 else None
    return {"n": n, "agreement": po, "kappa": kappa, "ac1": ac1, "pass_rate": q}


def paired(judgments: list[dict]) -> dict[tuple, list[dict]]:
    by_item: dict[tuple, list[dict]] = defaultdict(list)
    for j in judgments:
        by_item[(j["blind_id"], j["item"])].append(j)
    return by_item


def agree(blind_dir: Path, out: Path) -> None:
    judgments, blanks, items = load_judgments(blind_dir)
    by_item = paired(judgments)
    pair_rows, disagreements = defaultdict(list), []
    for (blind_id, item), js in sorted(by_item.items()):
        js = sorted(js, key=lambda j: j["grader"])
        for a, b in itertools.combinations(js, 2):
            pair_rows["전체"].append((a["result"], b["result"]))
            pair_rows[a["game"]].append((a["result"], b["result"]))
        if len({j["result"] for j in js}) > 1:
            info = items[(blind_id, item)]
            disagreements.append({"blind_id": blind_id, "item": item, "requirement": info["requirement"],
                                  **{f"grader_{i + 1}": j["grader"] for i, j in enumerate(js)},
                                  **{f"result_{i + 1}": "통과" if j["result"] else "실패" for i, j in enumerate(js)},
                                  "final": "", "note": ""})
    write_csv(out / "judgments.csv", judgments, ["blind_id", "game", "item", "grader", "result"])
    write_csv(out / "disagreements.csv", disagreements,
              ["blind_id", "item", "requirement", "grader_1", "result_1", "grader_2", "result_2", "final", "note"])
    write_csv(out / "incomplete.csv", blanks, ["blind_id", "game", "item", "grader"])
    stats = {name: agreement(rows) for name, rows in pair_rows.items()}
    (out / "agreement.json").write_text(json.dumps(stats, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    lines = ["# 채점자 일치도 (합의 전)", "",
             "통과가 대부분이면 κ가 낮게 나오므로 일치율·AC1과 함께 본다.", "",
             "| 범위 | 항목 짝 | 일치율 | Cohen's κ | Gwet AC1 | 통과 비율 |", "|---|---|---|---|---|---|"]
    for name, s in sorted(stats.items(), key=lambda kv: (kv[0] != "전체", kv[0])):
        f = lambda v: "-" if v is None else f"{v:.3f}"
        lines.append(f"| {name} | {s['n']} | {f(s['agreement'])} | {f(s['kappa'])} | {f(s['ac1'])} | {f(s.get('pass_rate'))} |")
    (out / "agreement.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    single = sum(1 for js in by_item.values() if len(js) < 2)
    print(f"판정 {len(judgments)}개, 빈 칸 {len(blanks)}개, 한 사람만 채점한 항목 {single}개, 어긋난 항목 {len(disagreements)}개")
    print(f"→ {out}/agreement.md, disagreements.csv (합의 회의에서 final 칸을 채운다)")


def final(blind_dir: Path, out: Path, mapping_path: Path) -> None:
    judgments, blanks, items = load_judgments(blind_dir)
    if blanks:
        raise SystemExit(f"빈 판정이 {len(blanks)}개 있다. agree 단계의 incomplete.csv를 먼저 채운다.")
    decided = {}
    if (out / "disagreements.csv").exists():
        for row in read_csv(out / "disagreements.csv"):
            value = verdict(row.get("final"))
            if value is None:
                raise SystemExit(f"합의되지 않은 항목: {row['blind_id']} {row['item']} (disagreements.csv의 final 칸)")
            decided[(row["blind_id"], row["item"])] = value
    scores: dict[str, list[int]] = defaultdict(list)
    for key, js in paired(judgments).items():
        values = {j["result"] for j in js}
        if len(values) > 1 and key not in decided:
            raise SystemExit(f"어긋났는데 합의 기록이 없음: {key}. agree 단계를 다시 돌린다.")
        scores[key[0]].append(decided.get(key, next(iter(values))))
    mapping = {row["blind_id"]: row for row in read_csv(mapping_path)}
    runs = []
    for blind_id, values in sorted(scores.items()):
        if blind_id not in mapping:
            raise SystemExit(f"대응표에 없는 블라인드 ID: {blind_id}")
        m = mapping[blind_id]
        runs.append({**m, "play_items": len(values), "passed": sum(values), "pass_rate": sum(values) / len(values)})
    write_csv(out / "run_scores.csv", runs,
              ["blind_id", "run_id", "game", "condition", "model", "rep", "play_items", "passed", "pass_rate"])
    print(f"회차 {len(runs)}개 → {out}/run_scores.csv")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="step", required=True)
    a = sub.add_parser("agree")
    a.add_argument("blind_dir", type=Path)
    a.add_argument("out", type=Path)
    f = sub.add_parser("final")
    f.add_argument("blind_dir", type=Path)
    f.add_argument("out", type=Path)
    f.add_argument("mapping", type=Path)
    args = parser.parse_args()
    if args.step == "agree":
        agree(args.blind_dir.expanduser(), args.out)
    else:
        final(args.blind_dir.expanduser(), args.out, args.mapping.expanduser())


if __name__ == "__main__":
    main()
