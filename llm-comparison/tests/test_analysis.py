import sys, json, csv, random, tempfile, runpy, os
from pathlib import Path

LC = Path(__file__).resolve().parents[1]  # llm-comparison/
sys.path.insert(0, str(LC / "harness"))
sys.path.insert(0, str(LC / "analysis")); sys.path.insert(0, str(LC / "grading"))
import analyze, errors, collect

# --- unit checks of the statistics
w = analyze.wilcoxon_exact([1, 2, 3, 4, 5, 6])
assert abs(w["p"] - 2 / 64) < 1e-12, w
w = analyze.wilcoxon_exact([1, -1, 2, -2])
assert w["p"] == 1.0, w
assert analyze.wilcoxon_exact([0, 0])["p"] is None
assert analyze.ranks([3, 1, 3, 2]) == [3.5, 1, 3.5, 2]
h = analyze.holm({"a": 0.01, "b": 0.04, "c": 0.03, "d": None})
assert abs(h["a"] - 0.03) < 1e-12 and abs(h["c"] - 0.06) < 1e-12 and abs(h["b"] - 0.06) < 1e-12 and h["d"] is None, h
a = collect.agreement([(1, 1)] * 8 + [(0, 0)] + [(1, 0)])
assert abs(a["agreement"] - 0.9) < 1e-12
# kappa: pa=0.9 pb=0.8 pe=0.72+0.02=0.74 -> (0.9-0.74)/0.26
assert abs(a["kappa"] - (0.16 / 0.26)) < 1e-9, a
q = 0.85; pe = 2 * q * (1 - q); assert abs(a["ac1"] - (0.9 - pe) / (1 - pe)) < 1e-9

# --- error classifier
assert errors.classify("main.cpp:10:15: error: 'class sf::Event' has no member named 'type'", "sfml")[0] == "sfml2_api"
assert errors.classify("main.cpp:3:10: fatal error: SFML/Graphics.hpp: No such file or directory", "meno")[0] == "sfml_in_meno"
assert errors.classify("main.cpp:5: error: 'class meno::Scene' has no member named 'foo'", "meno")[0] == "meno_api"
assert errors.classify("main.cpp:5: error: expected ';' before '}' token", "sfml")[0] == "cpp"
assert errors.classify("main.cpp:(.text+0x1): undefined reference to `foo'", "meno")[0] == "link"

# --- synthetic main run
rng = random.Random(1)
tmp = Path(tempfile.mkdtemp())
runs = tmp / "runs-main"; runs.mkdir()
models = ["claude-cli/claude-opus-5-5", "claude-cli/claude-haiku-5-5", "codex-cli/gpt-6.1-sol"]
games = ["02-snake", "04-shooter", "05-card", "08-formation", "10-missile", "11-survivor"]
truth_quality = {}
with (runs / "runs-top_light.jsonl").open("w", encoding="utf-8") as f1, (runs / "runs-other-vendor-1of2.jsonl").open("w", encoding="utf-8") as f2:
    for m in models:
        for g in games:
            for c in ("meno", "sfml"):
                for rep in (1, 2, 3):
                    rid = f"{g}-{c}-{m.replace('/', '_')}-r{rep}"
                    built = rng.random() < (0.9 if c == "meno" else 0.75)
                    first_ok = built and rng.random() < 0.6
                    rounds = 1 if first_ok else (rng.randint(2, 5) if built else 5)
                    detail = [{"round": i + 1, "code_found": True, "build_ok": built and i + 1 == rounds} for i in range(rounds)]
                    d = runs / rid; d.mkdir()
                    for i in range(rounds):
                        if not detail[i]["build_ok"]:
                            msg = ("main.cpp:9: error: 'class sf::Event' has no member named 'type'" if c == "sfml"
                                   else "main.cpp:9: error: 'meno::Game' has no member named 'run2'")
                            (d / f"build_{i + 1}.log").write_text(msg, encoding="utf-8")
                    if built:
                        (d / "game").write_bytes(b"x")
                    truth_quality[rid] = (0.85 if c == "meno" else 0.7) if built else None
                    rec = {"run_id": rid, "model_requested": m, "game": g, "condition": c, "rep": rep,
                           "build_success": built, "rounds": rounds, "rounds_detail": detail,
                           "completion_tokens": rng.randint(3000, 9000), "code_lines": rng.randint(150, 500) if built else None,
                           "reasoning_tokens": rng.randint(0, 3000), "llm_latency_s": rng.uniform(30, 200), "attempt": 1,
                           "noinput": {"early_exit": False, "frame_ms_p95": 17.6} if built else None}
                    (f2 if m.startswith("codex") else f1).write(json.dumps(rec) + "\n")

# blind packs (symlink may need privileges on Windows -> replace with copy)
Path.symlink_to = lambda self, target: self.mkdir()
blind_dir, mapping = tmp / "blind", tmp / "sealed" / "map.csv"
sys.argv = ["blind.py", str(runs), str(blind_dir), str(mapping), "--graders", "A,B,C"]
runpy.run_path(str(LC / "grading" / "blind.py"), run_name="__main__")
ids = [p for p in blind_dir.iterdir() if p.is_dir() and not p.name.startswith("_")]
assert len(ids) == sum(1 for v in truth_quality.values() if v is not None)
mp = {r["blind_id"]: r for r in csv.DictReader(open(mapping, encoding="utf-8"))}

# fill sheets: each item passes with run quality; second grader flips 5%
for d in ids:
    rid = mp[d.name]["run_id"]; qlt = truth_quality[rid]
    item_truth = {}
    for i, sheet in enumerate(sorted(d.glob("sheet-*.csv"))):
        rows = list(csv.DictReader(open(sheet, encoding="utf-8-sig")))
        for r in rows:
            if r["how"] != "play":
                continue
            t = item_truth.setdefault(r["id"], rng.random() < qlt)
            v = t if (i == 0 or rng.random() > 0.05) else (not t)
            r["result"] = "통과" if v else "실패"
        with open(sheet, "w", encoding="utf-8-sig", newline="") as f:
            wr = csv.DictWriter(f, fieldnames=list(rows[0])); wr.writeheader(); wr.writerows(rows)
    for meta in d.glob("meta-*.csv"):
        with open(meta, "w", encoding="utf-8-sig", newline="") as f:
            wr = csv.writer(f); wr.writerow(["guess_condition", "confidence", "minutes", "note"])
            wr.writerow([rng.choice(["meno", "sfml", "모름"]), "2", "14", ""])

results = tmp / "results"
collect.agree(blind_dir, results)
dis = list(csv.DictReader(open(results / "disagreements.csv", encoding="utf-8-sig")))
assert dis, "expected some disagreements"
# final before reconciliation must refuse
try:
    collect.final(blind_dir, results, mapping); raise AssertionError("should refuse unreconciled")
except SystemExit as e:
    assert "합의" in str(e)
for r in dis:
    r["final"] = r["result_1"]
with open(results / "disagreements.csv", "w", encoding="utf-8-sig", newline="") as f:
    wr = csv.DictWriter(f, fieldnames=list(dis[0])); wr.writeheader(); wr.writerows(dis)
collect.final(blind_dir, results, mapping)
scores = list(csv.DictReader(open(results / "run_scores.csv", encoding="utf-8-sig")))
assert len(scores) == len(ids) and all(0 <= float(s["pass_rate"]) <= 1 for s in scores)
bl = list(csv.DictReader(open(results / "blinding.csv", encoding="utf-8-sig")))
assert len(bl) == 2 * len(ids) and {b["truth"] for b in bl} == {"meno", "sfml"}
agreement = json.loads((results / "agreement.json").read_text(encoding="utf-8"))
assert 0.9 < agreement["전체"]["agreement"] < 1.0, agreement["전체"]

# analysis
out = tmp / "analysis"
analyze.analyze(runs, str(results / "run_scores.csv"), out)
report = (out / "report.md").read_text(encoding="utf-8")
assert "1차 지표" in report and "Holm" in report and "sfml2_api" in report and "meno_api" in report
cells = list(csv.DictReader(open(out / "cells.csv", encoding="utf-8-sig")))
primary = [float(c["diff"]) for c in cells if c["metric"] == "pass_all"]
assert len(primary) == 18 and sum(primary) / 18 > 0, primary  # meno built/passes more in synthetic data
# pre-grading mode
analyze.analyze(runs, "-", tmp / "pre")
assert "채점 결과 없이" in (tmp / "pre" / "report.md").read_text(encoding="utf-8")
print("ALL OK")
