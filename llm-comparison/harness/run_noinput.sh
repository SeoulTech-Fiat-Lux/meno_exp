#!/usr/bin/env bash
# 무입력 실행: 가상 화면(Xvfb)에서 입력 없이 SECONDS_TOTAL초 실행하며 녹화하고, 프레임 시간을 기록한다.
#
#   run_noinput.sh <빌드 폴더(game과 assets/가 있는 곳)> <출력 폴더> [초=30]
#
# 출력: record.mp4, stdout.txt, stderr.txt, frametime/(Mesa HUD 덤프), result.json
#   result.json의 exit_code: 124면 끝까지 살아 있었음(timeout이 끝냄), 그 밖의 값은 스스로 끝났거나 죽음.
#   early_exit: 10초 안에 끝났으면 true (요구사항: 10초 안에 비정상 종료가 없어야 함).
set -euo pipefail

build_dir=$(realpath "$1")
out_dir=$(realpath -m "$2")
seconds=${3:-30}
mkdir -p "$out_dir/frametime"

display_num=$((100 + RANDOM % 800))
export DISPLAY=":$display_num"
Xvfb "$DISPLAY" -screen 0 960x540x24 -nolisten tcp >/dev/null 2>&1 &
xvfb_pid=$!
trap 'kill $xvfb_pid 2>/dev/null || true' EXIT
for _ in $(seq 50); do xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break; sleep 0.1; done

ffmpeg -loglevel error -y -f x11grab -video_size 960x540 -framerate 30 -i "$DISPLAY" \
       -t "$((seconds + 1))" -pix_fmt yuv420p "$out_dir/record.mp4" &
ffmpeg_pid=$!

# Mesa(llvmpipe)의 HUD로 프레임 시간을 파일에 남긴다. 화면에는 그리지 않는다.
# 게임 코드에 손대지 않으므로 meno·SFML 두 조건에 똑같이 적용된다.
start=$(date +%s.%N)
set +e
(cd "$build_dir" && \
 GALLIUM_HUD=frametime GALLIUM_HUD_VISIBLE=false GALLIUM_HUD_PERIOD=0 \
 GALLIUM_HUD_DUMP_DIR="$out_dir/frametime" \
 timeout --signal=TERM --kill-after=5 "$seconds" ./game \
   >"$out_dir/stdout.txt" 2>"$out_dir/stderr.txt")
exit_code=$?
set -e
end=$(date +%s.%N)
wait "$ffmpeg_pid" 2>/dev/null || true

elapsed=$(echo "$end - $start" | bc)
early=$(echo "$elapsed < 10 && $exit_code != 124" | bc)
cat >"$out_dir/result.json" <<EOF
{"exit_code": $exit_code, "elapsed_s": $elapsed, "early_exit": $([ "$early" = 1 ] && echo true || echo false)}
EOF
cat "$out_dir/result.json"
