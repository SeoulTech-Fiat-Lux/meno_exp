"""run_noinput.sh가 남긴 Mesa HUD 덤프에서 프레임 시간 통계를 낸다.

사용: python3 frametime_stats.py <run 출력 폴더> [건너뛸 프레임 수=60]

덤프 파일(frametime/frametime_(ms))은 한 줄에 한 프레임의 시간(ms)이다. 창이 뜬 직후의 프레임은
로딩이 섞이므로 앞쪽 일부를 버린다. 결과를 result.json에 덧붙이고 출력한다.
"""
import json
import sys
from pathlib import Path


def percentile(sorted_values: list[float], q: float) -> float:
    index = min(len(sorted_values) - 1, int(q * len(sorted_values)))
    return sorted_values[index]


def main() -> None:
    run_dir = Path(sys.argv[1])
    skip = int(sys.argv[2]) if len(sys.argv) > 2 else 60
    dump = run_dir / "frametime" / "frametime_(ms)"
    values = [float(line) for line in dump.read_text().split() if line.strip()][skip:]
    stats = {"frames": len(values)}
    if values:
        values.sort()
        stats |= {
            "frame_ms_mean": round(sum(values) / len(values), 3),
            "frame_ms_p50": round(percentile(values, 0.50), 3),
            "frame_ms_p95": round(percentile(values, 0.95), 3),
            "frame_ms_p99": round(percentile(values, 0.99), 3),
            "frame_ms_max": round(values[-1], 3),
        }
    result_path = run_dir / "result.json"
    result = json.loads(result_path.read_text()) if result_path.exists() else {}
    result |= stats
    result_path.write_text(json.dumps(result, ensure_ascii=False) + "\n")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
