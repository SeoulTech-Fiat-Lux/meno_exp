"""프레임 시간 통계와 수직 동기화 판정. 입출력 없는 순수 계산만 둔다."""
from __future__ import annotations

# 흔한 주사율의 한 프레임 주기(ms).
REFRESH_MS = {hz: 1000 / hz for hz in (60, 75, 90, 120, 144, 165, 240)}


def percentile(sorted_values: list[float], q: float) -> float:
    """가장 가까운 순위 방식. llm-comparison/harness/frametime_stats.py, Tracy csvexport와 같다."""
    return sorted_values[min(len(sorted_values) - 1, int(q * len(sorted_values)))]


def nearest_refresh(ms: float) -> tuple[int, float]:
    """가장 가까운 주사율과, 그 주기에서 떨어진 비율."""
    hz, period = min(REFRESH_MS.items(), key=lambda kv: abs(kv[1] - ms))
    return hz, abs(period - ms) / period


def locked(median_ms: float, p10_ms: float) -> tuple[int, bool]:
    """프레임 시간이 한 주사율 주기에 촘촘히 모여 있으면 수직 동기화에 묶인 것이다.
    묶인 프레임은 주기에서 1~2%만 흔들린다. 허용 폭을 넓히면 우연히 주기 근처인 측정까지 잡힌다."""
    hz, err = nearest_refresh(median_ms)
    period = REFRESH_MS[hz]
    return hz, err < 0.02 and abs(p10_ms - period) / period < 0.03
