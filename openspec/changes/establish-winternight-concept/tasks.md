# Tasks

Milestone 1: engine foundation + Prologue vertical slice. Coverage gate (≥ 95%
core lines) and game tests apply from group 1 onward.

## 1. Project scaffolding and CI

- [ ] 1.1 Create the repository layout (D13), root `CMakeLists.txt` and `CMakePresets.json` with linux/windows/web/coverage/asan presets; verify `cmake --preset linux-debug && cmake --build` produces an empty `winternight` binary
- [ ] 1.2 Vendor Unity, cglm, xxHash and stb_image_write in `third_party/` with licence files; verify a trivial Unity test runs under CTest
- [ ] 1.3 Add the SDL3 dependency (FetchContent on desktop, `--use-port=sdl3` on web); verify an empty window opens on Linux and a blank canvas loads in the web build
- [ ] 1.4 Add `.github/workflows/ci.yml` with Linux (build, unit tests, gcovr `--fail-under-line 95`), Linux ASan/UBSan, Windows MSVC and Emscripten jobs; verify all jobs are green on main
- [ ] 1.5 Write `README.md` build instructions for all targets and verify each documented command works as written

## 2. Core foundations

- [ ] 2.1 Implement `wn_core` basics: allocator/pool, logging, PCG32 RNG streams, fixed-step clock and xxHash checksum, with unit tests (determinism of RNG, clock accumulation edge cases)
- [ ] 2.2 Implement the text data parser used by materials, palette and scene files, with unit tests for valid input and every error path
- [ ] 2.3 Implement input action mapping and per-step input frames (spec input-controls: Action mapping), with unit tests for binding, rebinding and axis combination
- [ ] 2.4 Implement input recording and replay files (spec input-controls: Input recording and replay), with unit tests for round-trip and corrupted files

## 3. Voxel world

- [ ] 3.1 Implement the 32-bit voxel cell accessors and chunk coordinates with voxel size as the single constant `WN_VOXEL_SIZE` (spec voxel-world: grid, voxel state), with unit tests including negative coordinates; verify CI runs the unit tests at both 0.1 and 0.05 voxel size
- [ ] 3.2 Implement uniform/expanded chunk storage and compaction, with unit tests
- [ ] 3.3 Implement the material table loader (spec voxel-world: Material table), with unit tests for ordering and invalid data
- [ ] 3.4 Implement ray casting and box overlap queries, with unit tests for hit, miss, chunk-border and fill-level cases
- [ ] 3.5 Implement chunk streaming with modification logs (spec voxel-world: Streaming), with unit tests for load/unload radius and persistence of edits
- [ ] 3.6 Implement deterministic procedural generation for the Prologue (terrain height field, road, pines, base snow layer) with a checksum unit test for seed 1234

## 4. Rendering (retro pipeline)

- [ ] 4.1 Implement greedy meshing with AO and fill-level tops (spec retro-rendering: Voxel meshing), with unit tests for culling, merging and determinism
- [ ] 4.2 Implement the core render command list and the `platform/gl` backend with shader version prefixing for GL 3.3 / GLES 3.0; verify a test chunk renders on Linux and web
- [ ] 4.3 Implement the low-res target and integer nearest upscale with letterboxing, unit-testing the scale/letterbox calculation and verifying with a game-test capture
- [ ] 4.4 Implement per-voxel hashed colour ramps, fog, palette LUT and Bayer dithering (unit tests for hash stability and the palette LUT; golden image "palette_only_colours")
- [ ] 4.5 Implement the headless game-test runner (`--test <script>`, SDL offscreen + llvmpipe, capture/compare/diff PNG); verify a first golden image test passes in CI
- [ ] 4.6 Implement the meshing job system with a main-thread time-slice fallback, with unit tests for budget and priority ordering
- [ ] 4.7 Implement quality presets (Mobile, Standard, High), phone detection for the default preset and automatic quality scaling (spec platform-targets), with unit tests for the scaling state machine and golden images taken on the Mobile preset
- [ ] 4.8 Add a frame-time and draw-call overlay plus a benchmark fly-through mode; verify it reports 95th/99th-percentile frame times and per-frame draw calls/triangles against the D14 budget

## 5. Night lighting

- [ ] 5.1 Implement incremental sky and block light flood fill (add/remove), with unit tests for window spill, wall breaking and interiors
- [ ] 5.2 Implement the moon shadow map pass with texel snapping; verify with the golden image "moon_shadow_pine"
- [ ] 5.3 Implement the procedural sky (gradient, stars, moon, aurora toggle); verify with the golden image "night_sky" and a game test that stars do not move with camera translation
- [ ] 5.4 Implement emissive glow and timed light colour changes (unit tests for colour interpolation; golden image "castle_windows")

## 6. Deformation

- [ ] 6.1 Implement damage and shaped edits (sphere, box, swept capsule), with unit tests for hardness ordering and falloff
- [ ] 6.2 Implement connectivity checks and debris extraction, with unit tests for the tree-cut and connected-wall scenarios
- [ ] 6.3 Implement debris rigid bodies (collision, sleeping, breaking, write-back, cleanup), with unit tests and a game test that a chopped tree falls and settles deterministically
- [ ] 6.4 Implement the remesh budget and priority under heavy edits, with a game test asserting frame meshing time stays within budget

## 7. Snow

- [ ] 7.1 Implement snowfall accumulation with sky exposure, with unit tests for open vs. sheltered columns
- [ ] 7.2 Implement footprints, trails and snow speed modifiers, with unit tests and a game test that a walked path shows a trail in the capture
- [ ] 7.3 Implement branch snow release, melting near heat and avalanche settling, each with unit tests
- [ ] 7.4 Implement snow particles (falling, puffs) in the renderer; verify with the golden image "snowfall_road"

## 8. Blood

- [ ] 8.1 Implement the particle pool and droplet emission scaled by damage and gore level, with unit tests for all three gore levels and undead dust
- [ ] 8.2 Implement stain writing, snow seepage and ageing, with unit tests and the golden image "blood_on_snow"
- [ ] 8.3 Implement liquid pools with flow and freezing, with unit tests for flat ground and slopes

## 9. Characters, camera and Prologue

- [ ] 9.1 Implement voxel character models on animated bones with damage and severing, with unit tests for severing a limb
- [ ] 9.2 Implement warrior movement, collision and third-person camera with wall avoidance, with unit tests and a game test for the camera-against-wall scenario
- [ ] 9.3 Implement melee swings as swept-capsule edits against world and characters, with unit tests and a game test that chops through the blocking tree
- [ ] 9.4 Implement the scene loader, triggers and events (spec game-flow), with unit tests including the missing-prefab error
- [ ] 9.5 Publish the first playable web build of the forest walk (opening shot, snow, castle in the distance) for the user to review voxel size and look; verify it loads on an average phone and record the voxel-size decision in design.md D15 before 9.6 starts
- [ ] 9.6 Complete the Prologue scene (forest road, wolf tracks, fallen tree, Castle Hearthholm with lit windows, gate trigger) and verify the full scripted playthrough game test plus the benchmark golden image "benchmark_road"

## 10. Web, mobile touch and WebXR

- [ ] 10.1 Create the web shell (`shell.html`), WebGL2 check with error message and hidden-tab pause; verify with Playwright tests
- [ ] 10.2 Implement touch controls overlay and coarse-pointer detection (spec input-controls), with unit tests for the virtual stick maths and a Playwright touch-emulation test
- [ ] 10.3 Implement the WebXR JS library bridge, stereo rendering and session enter/exit; verify with a Playwright + IWER test that two views render and the game continues after exit
- [ ] 10.4 Implement XR controls (stick locomotion, snap/smooth turn, swing attacks) and the comfort vignette, with unit tests for swing-speed damage and turning

## 11. Integration checks

- [ ] 11.1 Run the Prologue replay on Linux, Windows and web in CI and verify identical checksums
- [ ] 11.2 Fix the reference phone models (D14), run the benchmark fly-through on them, the desktop and the headset, and verify the average phone meets 95th-percentile ≤ 16.7 ms and 99th-percentile ≤ 33 ms; record results in `docs/performance.md`
- [ ] 11.3 Verify the coverage report shows ≥ 95% line coverage for the core and all game tests pass on every target

## Workflow follow-up

- Archive the change with `openspec archive establish-winternight-concept` once milestone 1 is accepted.
