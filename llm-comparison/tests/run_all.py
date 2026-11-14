"""tests/의 테스트를 모두 돌린다. LLM·빌드·Xvfb 없이 가짜 출력으로 하니스·채점·분석 흐름을 확인한다.

사용 (llm-comparison/ 에서):
    python3 tests/run_all.py            # 모두
    python3 tests/run_all.py cli trunc  # 이름에 해당 글자가 들어간 것만

하니스(harness/)를 고친 뒤, 특히 동결 전에 돌린다. 테스트는 모듈 전역을 바꿔 가짜 CLI를 끼우므로
파일마다 따로 된 프로세스에서 돌린다. 저장소 안에는 아무것도 쓰지 않는다 (임시 폴더만 씀).

    test_run       거부·캐시·추론 토큰 기록 (LiteLLM 백엔드)
    test_fix       동결 일관성, 실패 기록(문맥 창 초과·정책 거부), 끊긴 회차, 무입력 실패
    test_cli       Claude Code·Codex 백엔드: 플래그, 세션 이어 가기, 한도 대기, 로그인 오류, probe
    test_trunc     Claude Code 출력 상한 오류를 잘린 왕복으로 처리
    test_noinput   1초 미만 종료·깨진 result.json에서도 하니스가 멈추지 않음
    test_summary   문맥 토큰 측정과 시범 요약
    test_records   기록 형식 버전, 기록 파일 모으기, 동결 범위
    test_analysis  블라인드 묶음 → 채점 → 합의 → 최종 점수 → 사전 등록 분석 (합성 데이터)
"""
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> None:
    sys.stdout.reconfigure(errors="replace")  # 한국어를 못 쓰는 콘솔(Windows cp949 등)에서도 실패 내용을 보여 주게
    wanted = sys.argv[1:]
    tests = [p for p in sorted(HERE.glob("test_*.py")) if not wanted or any(w in p.stem for w in wanted)]
    failed = []
    for test in tests:
        start = time.monotonic()
        # -X utf8: 테스트의 출력 인코딩을 UTF-8로 (-I는 PYTHONIOENCODING을 무시함)
        proc = subprocess.run([sys.executable, "-I", "-X", "utf8", str(test)], capture_output=True, text=True,
                              encoding="utf-8", errors="replace")
        ok = proc.returncode == 0 and proc.stdout.rstrip().endswith("ALL OK")
        print(f"{'ok  ' if ok else 'FAIL'} {test.stem} ({time.monotonic() - start:.1f}s)", flush=True)
        if not ok:
            failed.append(test.stem)
            print("\n".join((proc.stdout + proc.stderr).strip().splitlines()[-25:]))
    print(f"\n{len(tests) - len(failed)}/{len(tests)} 통과" + (f", 실패: {', '.join(failed)}" if failed else ""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
