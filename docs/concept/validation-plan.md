# Winternight — Validation Plan

> Concept-to-evidence map. Existing checks below were inspected at foundation
> revision `15c13a8`; future cases are proposed work, not tests added by this
> documentation pass. A passing image comparison protects an accepted frame.
> It does not establish artistic quality, hardware comfort or full simulation
> correctness.

## What >=95% coverage means

The existing CI fails below 95% line coverage across `engine/` and `game/`, the
platform-independent C core. Unity / CTest exercise it without SDL or OpenGL;
gcovr reports line, function and branch coverage. The core includes mesh and
render planning, not just gameplay. Report all included files and uncovered
lines. Branch coverage is reported, with the foundation design aiming for 85%.

SDL, GL calls and browser glue live in `platform/` and need automated integration
checks. The 95% figure is **not whole-repository unit coverage**. Do not move
logic into platform code or suppress uncovered core files to satisfy the gate.

For each future core feature, add behavior tests for its normal result,
boundaries, exhaustion policy and failure path, then run coverage. For each
visible feature, also add a game or browser assertion that reaches the real
integration path. Percentage alone cannot prove that a footprint or cut works.

## Existing checks and proposed extensions

| Claim | Existing evidence location | Proposed next check / limitation |
|-------|----------------------------|----------------------------------|
| Coordinates and fill heights agree. | [Voxel unit tests](../../tests/unit/test_voxel.c), [snow tests](../../tests/unit/test_snow_fx.c). | Extend cross-chunk, partial-fill and negative-coordinate edit / collision cases as new rules are added. |
| Cuts change the route. | [Edit unit tests](../../tests/unit/test_edit.c), [prologue replay](../../tests/game/scripts/prologue_playthrough.wntest). | Add repeated border edits, same-count different-topology replay checks and near-field mesh latency. |
| Footprints persist and refill. | Snow unit tests, prologue movement and [snowfall capture](../../tests/game/scripts/visuals.wntest). | Assert the actual walked path's fill values, burial times and image change before / after snowfall. A snowy image alone is insufficient. |
| Trees detach and settle. | Edit unit tests and the chopped-log capture in the prologue replay. | Saturate all debris slots; preserve a supporting piece; cross a streaming boundary while a body is active. |
| Blood belongs to snow. | Snow / FX unit tests and `blood_on_snow` in the visual script. | Same-cell burial, stain-age record exhaustion, reload ageing and transfer to / from detached pieces. |
| Windows remain inviting. | `castle_windows`, `night_sky`, `moon_shadow_pine` golden captures. | Broken-window light update; no warm leak through intact walls; greyscale and real-display review. |
| A roof admits winter when cut. | Local snow and lighting APIs exist; no complete room-rupture scene. | Roof intact vs. removed: exposure, accumulation, light, debris and navigation in one replay. |
| Streaming preserves changes. | Voxel storage unit tests. | Long edited route with a memory budget, canonical reload state and correct far-scenery invalidation. |
| Touch is usable. | [Touch browser tests](../../tests/web/touch.test.js). | Multitouch move + look + attack, cancellation, hybrid input, safe areas and orientation; actual-device review. |
| XR can enter, render and exit. | [IWER browser tests](../../tests/web/xr.test.js), [XR core](../../game/src/wn_xr.c). | Controller hits, interrupted sessions, context loss, comfort and sustained real-headset frame time. |
| Simulation agrees across builds. | [Determinism script](../../tests/game/scripts/determinism.wntest), [web check](../../tests/web/determinism.test.js), `wn_simtest`. | Expand canonical hashes beyond the replay edit counter; include material state, RNG, queued work and settled debris. |
| The scene meets performance goals. | [Performance notes](../performance.md), benchmark tool, [benchmark script](../../tests/game/scripts/benchmark_road.wntest). | Moving camera, destruction spikes, long-session peak memory and measurements on the named phones / Quest. |

The current Linux CI runs golden-image game tests using Xvfb / Mesa llvmpipe.
Windows runs native unit tests and the GPU-free determinism runner; web runs
browser tests. Do not describe this as full golden-image parity on every target.
The emulated XR test checks session progress and per-eye view counts; it cannot
establish a comfortable rendered view or correct motion-controlled combat.

## Four proposed showcase acceptance passes

### 1. Winter road and scale

- Replay the same route and camera at 10 cm and 5 cm; use metre-based framing and
  comparable view distance, not an unchanged number of chunks.
- Capture the castle reveal, foot-level snow, the warrior silhouette and a cut
  pine at fixed simulation ticks and seed.
- Human review uses the [visual checklist](visual-style.md#visual-review-checklist).
  Record the chosen scale and reason before accepting new image baselines.
- Record frame-time percentiles, mesh counts and peak memory for both scales.
  A finer screenshot without an interaction cost is not enough to choose it.

### 2. Material courtyard

- At a fixed seed, walk through snow, chop a pine, strike stone and ice, and
  apply a bounded blood-on-snow study. Check exact cell / collision changes.
- Walk beyond the load radius and return. Compare canonical material state and
  captures; the distant silhouette must reflect the changes too.
- Run with Full, Reduced and Off gore, plus a low decorative-particle preset.
  Assert that navigation and deterministic physical results remain coherent.
- Saturate particle, stain, pool and debris capacities and assert each documented
  fallback. Fail visibly on invalid state rather than treating a crash-free run
  as the only criterion.

### 3. Exposed room

- Capture an intact warm room. Remove a specified roof section, then check the
  new opening, detached structure, snowfall exposure and resulting light.
- Advance a fixed amount of weather time. Assert snow below the opening grows
  while sheltered control cells do not; capture the changed room.
- Verify a valid escape remains after settling and after reload. Essential
  rubble must not vanish because a decorative debris pool fills.

### 4. Dragon impact

- Start with a silhouette and one bounded structural impact, not a full battle.
- Replay impacts while the camera observes them closely; record p95, p99 and
  worst frame, mesh backlog, cells touched, fragments and peak memory.
- In XR, inspect each eye's projection and viewport, arm's-length cuts and the
  return to flat play. Confirm comfort and performance on the headset itself.

These passes are a sequencing proposal. Each implementation change must name
its exact tests, scenarios and numerical bounds in OpenSpec before it starts.

## Determinism and image policy

Use fixed simulation ticks, a scene version, explicit input and subsystem seeds.
Hash canonical serialized state in stable coordinate / entity order rather than
native struct padding. Compare hashes at checkpoints across Linux, Windows and
WebAssembly. A future hash must detect different world contents even when edit
counts match. Wall-clock meshing budgets must not change the physical outcome.

Keep approved golden frames small and deterministic. Pin the CI environment
and record the software renderer version; broad OS labels alone do not pin
Mesa behavior. Document per-capture tolerances and masks when introduced.
Use state assertions alongside images: similar frames can conceal wrong
collision, missing persistence or a particle that never deposited snow.

Golden updates require a meaningful intended visual change and review of the
before, after and difference images. They are not the automatic fix for a
failing test. This concept pass does not regenerate any golden images.

## Performance evidence

For every run, record commit, seed, route, voxel size, preset, internal / eye
resolution, device, OS, browser / driver, warm-up and measurement duration.
Measure a sustained route after warm-up, including repeated destruction, and
keep the raw frame-time trace. Report p50, p95, p99, worst frame, memory peak,
draw calls, triangles, world-work time and outstanding mesh work.

Use the provisional budgets in [the engine concept](voxel-engine.md#budgets-and-measurements).
A Chromium CPU throttle is a regression signal, not phone GPU certification.
IWER is a functional emulator, not a Quest performance test. Software-renderer
timings are useful for comparisons on that same machine, not for claiming a
mobile frame rate. Real devices and human comfort review remain required.

## Reproduce the current checks

Follow [README build and test instructions](../../README.md#tests). Minimal
core-only validation requires no SDL or GPU:

```sh
cmake -S . -B build/cov -DCMAKE_BUILD_TYPE=Debug -DWN_BUILD_GAME=OFF -DWN_COVERAGE=ON
cmake --build build/cov
ctest --test-dir build/cov --output-on-failure
gcovr -r . build/cov --filter engine/ --filter game/ --exclude-unreachable-branches --fail-under-line 95 --print-summary
npx --yes @fission-ai/openspec@1.17.0 validate --strict --all
```

The existing [CI workflow](../../.github/workflows/ci.yml) adds sanitizer,
native-rendered game, Windows and browser checks. Keep documentation validation
separate from runtime evidence, and explicitly report which checks ran for a
particular change.
