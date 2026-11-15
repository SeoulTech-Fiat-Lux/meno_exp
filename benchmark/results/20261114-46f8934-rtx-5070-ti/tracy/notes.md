# Tracy 병목 분석 노트

자동 표와 그림은 `zones.md`, `zones.png`. 이 파일은 사람이 쓴다(다시 만들지 않음).

> 초안(Claude 작성, 2026-11-15). 수치를 직접 확인하고 판단을 고친 뒤 이 줄을 지운다.

## 캡처 조건

- 날짜, 캡처한 사람: 2026-11-14, (작성)
- 환경: `../env.json`과 같은 PC인가 (GL 렌더러): 같다. D3D12 (NVIDIA GeForce RTX 5070 Ti), AMD Ryzen 7 9700X, WSLg
- Tracy 버전: v0.15.0 (bench.toml `[tracy].version`과 bench_app/CMakeLists.txt의 tracy GIT_TAG f2a4f25가 같다)
- N: 1,000, 10,000 (대상 meno-object, `--loop direct`, 워밍업 120 + 기록 600프레임)

## 관찰

- 가장 비싼 구간과 그 비율 (N이 가장 클 때): N=10,000에서 `scene.draw`가 9.94 ms로 프레임의 71%. 그다음 `window.display` 3.82 ms(27%).
- N에 비례해 늘어나는 구간: `scene.draw`가 N=1,000→10,000에서 0.25→9.94 ms(+9.69 ms, 39배). 스프라이트 하나에 약 1.1 µs. `window.display`도 2.48→3.82 ms(+1.34 ms)로 조금 는다.
- 거의 그대로인 구간(고정 비용): `window.display`의 대부분(N=1,000에서 2.48 ms), `renderer.begin` 0.12 ms, `app.events` 0.01 ms.
- 스프라이트마다 하는 CPU 계산: `bench.move` + `bench.apply`가 N=10,000에서 0.13 ms(1%). 움직임 계산과 Transform 쓰기는 병목이 아니다.
- `not instrumented`: 0.001 ms. 프레임 시간은 모두 구간 안에 들어가 있다.
- 평균과 중앙값이 거의 같다(N=10,000 프레임 평균 14.03, 중앙값 13.93 ms). 튀는 프레임이 평균을 끌어올린 것이 아니다.
- Tracy를 켜면 프레임이 느려진다: N=10,000에서 14.03 ms. Tracy 없이 잰 meno-object는 10.17 ms(`../summary.md`). 그래서 이 표의 ms는 비율로만 읽고, 프레임 시간 수치는 `../summary.md`를 쓴다.
- Tracy 화면 캡처: 없음. 비율 그림은 `zones.png`.

## 판단

- 병목: 스프라이트마다 한 번씩 하는 그리기 호출(`scene.draw`). N=1,000→10,000에서 프레임이 11.15 ms 늘었고, 그중 9.69 ms(87%)가 여기서 생긴다.
- 배칭으로 줄어들 부분: `scene.draw` 안의 그리기 호출 비용. Tracy 없이 잰 표에서 sfml-batched는 N=10,000에 1.88 ms, sfml은 10.21 ms(5.43배).
- 배칭으로 안 줄어드는 부분: `window.display`의 고정 비용(약 1.3~2.5 ms), 움직임 계산과 Transform 쓰기(0.13 ms). 장면 순회 비용은 Tracy로는 따로 나누지 않았지만, meno-object/meno-renderer가 모든 N에서 0.97~1.02라 무시할 만하다.
- 결론과 다음 할 일: (작성) 예: 이 PC에서는 N=10,000에서도 p99 12.5 ms로 60 fps 예산(16.7 ms) 안이라 배칭이 당장 필요하지는 않다. 다만 N에 비례하는 비용의 대부분이 그리기 호출이므로, 스프라이트 수가 더 많거나 CPU가 느린 환경을 목표로 하면 배칭이 가장 효과가 크다.
