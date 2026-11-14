# Tracy 병목 분석 노트

자동 표와 그림은 `zones.md`, `zones.png`. 이 파일은 사람이 쓴다(다시 만들지 않음).

## 캡처 조건

- 날짜, 캡처한 사람:
- 환경: `../env.json`과 같은 PC인가 (GL 렌더러):
- Tracy 버전: (bench.toml `[tracy].version`과 Meno의 FetchContent 태그가 같아야 함)
- N: 1,000, 10,000

## 관찰

- 가장 비싼 구간과 그 비율 (N이 가장 클 때):
- N에 비례해 늘어나는 구간 (`zones.md`의 "늘어난 비용"):
- 거의 그대로인 구간(고정 비용):
- `not instrumented`가 크면 어디가 빠졌는가:
- Tracy 화면 캡처 (`*.png`)와 그 설명:

## 판단

- 병목:
- 배칭으로 줄어들 부분 (그리기 호출 수에 비례하는 구간):
- 배칭으로 안 줄어드는 부분 (장면 순회, 변환 갱신 등):
- 결론과 다음 할 일 (SeoulTech-Fiat-Lux/Meno#30에 옮길 내용):
