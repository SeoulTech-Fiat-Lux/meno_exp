<!-- #30 종료 댓글 초안. 숫자는 결과에서 옮겼다. (작성) 자리를 채운 뒤 올린다. -->
측정 결과는 SeoulTech-Fiat-Lux/meno_exp의 `benchmark/results/20261114-46f8934-rtx-5070-ti/report/benchmark.md`에 정리했다 (meno `46f8934`, D3D12 (NVIDIA GeForce RTX 5070 Ti)).

- N = 10,000: sfml-batched 1.91 ms, sfml 10.18 ms, meno-renderer 10.24 ms, meno-object 10.15 ms
- sfml/sfml-batched = 5.34, meno-renderer/sfml-batched = 5.37, meno-object/sfml-batched = 5.32

**배칭 판단:** (작성)

### 🇬🇧 TL;DR for English Readers

**:sparkles: Proposed Feature:**
A sprite-count benchmark (N = 100, 200, 400, 1,000, 2,000, 5,000, 10,000) comparing batched SFML (one draw call), raw SFML (one draw call per sprite), meno's `Renderer`, and meno's `GameObject` + `Sprite` path, to decide whether meno needs sprite batching.

**:hammer_and_wrench: Implementation Details:**
A standalone benchmark program fetches meno at the frozen commit and uses only its public API, so meno itself is unchanged. It draws N moving sprites with frame limit and vsync off, updating exactly once per frame on every path; a runner repeats each (target, N) 5 times in fresh processes and reports mean and p99 frame time. A Tracy build replays `Game::run()` step by step through public calls to break a frame into stages by self time. (작성)

**:microscope: Academic/Technical Significance:**
(작성) What the stage breakdown shows about where the cost of a high-level 2D API comes from.
