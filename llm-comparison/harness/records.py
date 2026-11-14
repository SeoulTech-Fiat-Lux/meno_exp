"""회차 계획과 회차 기록의 형식. 회차를 만들거나 기록을 읽는 곳은 모두 이 모듈을 쓴다.

회차 = (모델, 게임, 조건, 반복 번호). 회차 ID 규칙과 회차 계획(섞는 순서)은 여기 한 곳에만 둔다.
하니스(run.py)가 회차를 돌려 기록을 쓰고, 결과 합치기(grading/merge_runs.py)는 같은 계획과 대조하며,
시범 요약(analysis/summarize.py), 분석(analysis/analyze.py, errors.py), 블라인드 묶음(grading/blind.py)이 기록을 읽는다.
필드를 바꾸거나 지우면 SCHEMA_VERSION을 올리고, 읽는 쪽을 함께 고친다.

회차 기록 한 줄에서 읽는 쪽이 기대는 필드:
    run_id, model_requested, game, condition, rep         회차 식별
    build_success, rounds, rounds_detail[], failure       빌드 결과 (rounds_detail[i]: round, code_found, build_ok, truncated)
    prompt_tokens, completion_tokens, reasoning_tokens    토큰 (prompt_tokens는 캐시와 무관한 원래 입력)
    code_lines, llm_latency_s, started_at, finished_at    크기와 시간
    noinput {exit_code, elapsed_s, early_exit, frame_ms_p95, ...}, attempt, hashes.context, inputs_sha256, frozen
기록 파일은 사람마다 따로 쓴다: runs.jsonl, runs-<모델>[-<몫>].jsonl. 읽을 때는 폴더의 기록 파일을 모두 모은다.

형식 버전:
    1  첫 형식 (시범 실행 pilot-e1712ed0까지)
    2  API(LiteLLM) 백엔드를 뺐다. cost, model_params, prompt_cache, versions.litellm, rounds_detail[].refusal_text를 지웠다.
       읽는 쪽이 기대는 필드는 그대로이므로 버전 1도 그대로 읽는다.
"""
from __future__ import annotations

import json
import random
import re
from dataclasses import dataclass
from pathlib import Path

SCHEMA_VERSION = 2  # 기록 형식 버전. 필드를 바꾸면 올린다
RUNS_GLOB = "runs*.jsonl"
BASE = Path(__file__).resolve().parent.parent  # llm-comparison/
CONTEXT_TOKENS = BASE / "runs" / "context_tokens.jsonl"  # run.py --measure-context가 쌓는 문맥 토큰


# ---------------------------------------------------------------------------
# 회차 계획
# ---------------------------------------------------------------------------
def slug(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9.-]+", "_", text).strip("_")


@dataclass
class Run:
    model: dict
    game: str
    condition: str
    rep: int
    order: int = 0

    @property
    def run_id(self) -> str:
        return f"{self.game}-{self.condition}-{slug(self.model['id'])}-r{self.rep}"


def plan_runs(games: list[str], conditions: list[str], models: list[dict], reps: int, seed: int) -> list[Run]:
    """모든 회차를 시드로 섞은 순서로 만든다. 시간대에 따른 응답 변동이 한 조건에 몰리지 않게 한다."""
    runs = [Run(m, g, c, r) for m in models for g in games for c in conditions for r in range(1, reps + 1)]
    random.Random(seed).shuffle(runs)
    for index, run in enumerate(runs):
        run.order = index
    return runs


def main_plan(cfg: dict) -> list[Run]:
    """experiment.toml의 본 실행 계획 (모델 × games.main × 조건 × 반복)."""
    exp = cfg["experiment"]
    return plan_runs(cfg["games"]["main"], list(cfg["conditions"]), cfg["models"], exp["reps"], exp["seed"])


def model_undecided(model: dict) -> bool:
    return "TBD" in model["id"] or model.get("model") == "TBD"


# ---------------------------------------------------------------------------
# 기록 읽기
# ---------------------------------------------------------------------------
def load_jsonl(*paths: Path) -> list[dict]:
    return [json.loads(line) for path in paths if path.exists()
            for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def run_logs(runs_dir: Path) -> list[Path]:
    return sorted(Path(runs_dir).glob(RUNS_GLOB))


def load_records(runs_dir: Path, required: bool = True) -> list[dict]:
    """폴더의 모든 기록 파일에서 회차 기록을 모은다. 이 모듈보다 새 형식의 기록이 있으면 멈춘다."""
    records = load_jsonl(*run_logs(runs_dir))
    if required and not records:
        raise SystemExit(f"{runs_dir}에 회차 기록({RUNS_GLOB})이 없음")
    newer = {r.get("schema_version", 1) for r in records} - set(range(1, SCHEMA_VERSION + 1))
    if newer:
        raise SystemExit(f"{runs_dir}의 기록 형식(schema_version {sorted(newer)})이 이 코드({SCHEMA_VERSION})보다 새로움. "
                         "저장소를 최신으로 받는다.")
    return records


def load_context_tokens() -> list[dict]:
    return load_jsonl(CONTEXT_TOKENS)
