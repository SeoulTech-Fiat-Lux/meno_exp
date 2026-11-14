#!/usr/bin/env bash
# 무입력 녹화(record.mp4)에서 1초에 한 장씩 시각을 찍은 장면을 뽑아 한 장의 그림으로 모은다.
# 명세의 무입력 기준(예: 퐁은 약 4.5초에 CPU 첫 득점, 10초 안에 2점)을 영상을 돌려 보지 않고 판정할 때 쓴다.
#
#   analysis/contact_sheet.sh <record.mp4> <출력.png> [초=15]
#
# 5칸씩 줄을 채운다 (15초면 5×3). 시각은 녹화 시작 기준이며, 게임 시작보다 0.1초 안팎 이르다.
set -euo pipefail

input=$1
output=$2
seconds=${3:-15}
rows=$(( (seconds + 4) / 5 ))

ffmpeg -loglevel error -y -t "$seconds" -i "$input" \
  -vf "fps=1,scale=320:-1,drawtext=text='%{pts\:hms}':x=5:y=5:fontsize=16:fontcolor=yellow,tile=5x${rows}" \
  -frames:v 1 "$output"
echo "$output"
