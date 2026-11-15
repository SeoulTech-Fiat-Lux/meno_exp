# Tracy 병목 분석 노트

자동 표와 그림은 `zones.md`, `zones.png`. 이 파일은 사람이 쓴다(다시 만들지 않음).

> 초안(Claude 작성, 2026-11-15). 수치를 직접 확인하고 판단을 고친 뒤 이 줄을 지운다.

## 캡처 조건

- 날짜, 캡처한 사람: 2026-11-14, (작성)
- 환경: `../env.json`과 같은 PC인가 (GL 렌더러): 같다. D3D12 (AMD Radeon(TM) Graphics, CPU 내장), AMD Ryzen 7 9700X, WSLg
- Tracy 버전: v0.15.0 (bench.toml `[tracy].version`과 bench_app/CMakeLists.txt의 tracy GIT_TAG f2a4f25가 같다)
- N: 1,000, 10,000 (대상 meno-object, `--loop direct`, 워밍업 120 + 기록 600프레임)

## 관찰

- 가장 비싼 구간과 그 비율 (N이 가장 클 때): N=10,000에서 `window.display`가 5.87 ms로 60%, `scene.draw`는 3.65 ms로 37%. RTX와 순서가 반대다.
- N에 비례해 늘어나는 구간: `scene.draw` 0.25→3.65 ms(+3.39 ms)와 `window.display` 2.75→5.87 ms(+3.12 ms). 두 구간이 거의 같은 만큼 는다.
- 두 구간을 합쳐 읽는다: OpenGL 드라이버는 그리기 명령을 모아 두었다가 나중에 처리할 수 있다. 이 드라이버는 그리기 비용의 절반 가까이를 화면을 넘기는 `window.display` 때 치르는 것으로 보인다. 그래서 `scene.draw` + `window.display`를 그리기 비용으로 본다. 이 합은 3.00→9.50 ms(중앙값)로 는다.
- 거의 그대로인 구간(고정 비용): `window.display`의 N=1,000 몫(2.75 ms), `renderer.begin` 0.11~0.15 ms, `app.events` 0.01 ms.
- 스프라이트마다 하는 CPU 계산: `bench.move` + `bench.apply`가 N=10,000에서 0.11 ms(1%).
- `not instrumented`: 0.001 ms.
- 평균과 중앙값이 거의 같다(N=10,000 프레임 평균 9.75, 중앙값 9.72 ms).
- Tracy를 켠 프레임 시간: N=10,000에서 9.75 ms. Tracy 없이 잰 값은 9.40 ms(`../summary.md`). RTX보다 덜 느려진다.
- Tracy 화면 캡처: 없음. 비율 그림은 `zones.png`.

## 판단

- 병목: 스프라이트마다 한 번씩 하는 그리기 호출. 이 GPU에서는 그 비용이 `scene.draw`와 `window.display`에 나뉘어 잡힌다.
- 배칭으로 줄어들 부분: 두 구간에서 N에 비례해 는 몫(+6.5 ms). Tracy 없이 잰 표에서 sfml-batched는 N=10,000에 3.95 ms, sfml은 9.42 ms(2.39배).
- 배칭으로 안 줄어드는 부분: `window.display`의 고정 몫, 움직임 계산과 Transform 쓰기(0.11 ms). 배칭한 경우에도 N=100→10,000에서 1.83→3.95 ms로 늘어난다. 이 GPU에서는 스프라이트 수에 따른 GPU 쪽 비용이 남는다.
- 결론과 다음 할 일: (작성) 예: 내장 GPU에서도 N=10,000 p99가 10.5 ms라 60 fps 예산 안이다. 배칭 효과는 RTX보다 작다(2.39배 대 5.43배). 배칭 뒤에는 GPU 성능이 상한을 정한다.
