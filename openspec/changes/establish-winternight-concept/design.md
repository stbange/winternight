# Design

## Context

The repository starts empty. Motivation and scope are in `proposal.md`, and the
required behaviour is in `specs/`. The mood and art rules are in
`docs/concept/visual-style.md`. These constraints shape the design:

- **Language and libraries:** C11, SDL3 and OpenGL. The web build must use WebGL2
  (≈ GLES 3.0), so the desktop renderer cannot use features beyond that subset
  (no compute shaders, no geometry shaders, no SSBOs).
- **WebXR:** SDL3 does not provide WebXR. It has to be reached through JavaScript
  from the Emscripten build.
- **Test gates:** 95% coverage and deterministic game tests mean that game logic
  must be separate from SDL, GL and browser APIs, and must be deterministic.

## Goals / Non-Goals

**Goals:**
- One architecture that serves desktop, web, mobile web and WebXR without forks.
- A voxel data model that supports deformation, snow fill, blood stains and light
  in one place, so the systems work together (e.g. blood on snow, fire melting snow).
- A core that can be unit-tested and replayed deterministically.
- Render the benchmark shot (`visual-style.md`) within the performance budgets.

**Non-Goals:**
- Smooth or marching-cubes voxel surfaces. The blocky look is intended.
- Physically accurate fluids or snow mechanics. Cellular rules are enough.
- A general-purpose engine or an editor in this milestone (scenes are text files).
- Native VR (OpenXR) and macOS.

## Decisions

### D1. Three-layer architecture
```
game/        Game rules, scenes, AI, combat         (pure C, no platform calls)
engine/      Voxel world, deformation, snow, blood, (pure C, no platform calls)
             lighting, meshing, physics, input mapping,
             replay, render *commands*
platform/    SDL3 window/input/time, OpenGL backend,  (thin glue, no game logic)
             Emscripten main loop, JS bridges (touch, WebXR)
```
`engine` and `game` form the **core** library (`wn_core`), which never includes
SDL or GL headers. The renderer is split: the core builds plain data (mesh
buffers, a draw list, uniforms, render settings) and `platform/gl` turns it into
GL calls. This keeps meshing, culling, palette and lighting logic unit-testable.
*Alternative:* call GL directly from engine code. Rejected because it makes 95%
coverage unreachable without a GPU.
*Web/XR impact:* only `platform/` has `#ifdef __EMSCRIPTEN__` code.

### D2. 32-bit voxel cell and 32³ chunks
```
bits  0–7   material id   (256 materials, 0 = air)
bits  8–11  fill level    (0–15; 0 = full for non-granular materials)
bits 12–15  damage        (0–15)
bits 16–17  stain type    (none, blood, ichor, soot)
bits 18–23  stain amount  (0–63)
bits 24–27  sky light     (0–15)
bits 28–31  block light   (0–15)  + colour from nearest emitter table
```
A chunk with one value for all voxels is stored as that single value (spec
"Memory-efficient chunk storage"). A full chunk takes 128 KiB. A 12-chunk view
radius with a vertical span of 4 chunks means about 2,300 chunks, but most are
uniform air or solid, so the expected live memory is under 100 MiB on desktop. The
Mobile preset uses a 6-chunk radius (about 600 chunks, under 40 MiB) (D14).
The voxel edge length is a single constant (`WN_VOXEL_SIZE`, default 0.1 m), and
nothing else may hard-code it, because it will be reviewed after the first
playable version (D15).
*Alternatives:* separate layers for stain and light (more cache misses and more
code), or 16-bit cells (no room for stain, which is central to the look).
*Web impact:* there is no `mmap`; chunks come from a pooled allocator to avoid
fragmenting the WASM heap.

### D3. Meshing: greedy quads with baked AO, on worker threads
- Faces are merged when material colour index, AO, light and stain match. Since
  per-voxel hash variation (retro-rendering spec) changes the colour per voxel, merging works on the
  **colour index after hashing**. That still gives large merges on uniform snow
  thanks to its narrow ramp.
- Fill-level voxels (snow, liquid) are meshed with a lowered top face and are
  never merged with full voxels on their sides.
- Vertex format: 8 bytes (position 3×u8 inside the chunk + u8 fill/AO, normal +
  palette index + light packed in the remaining 4 bytes).
- Work is spread across a job system (SDL threads on desktop; Emscripten pthreads
  when the page is cross-origin isolated, otherwise a per-frame time slice on the
  main thread).
*Web impact:* cross-origin isolation (COOP/COEP headers) is needed for threads.
The fallback keeps simple hosting working.

### D4. Rendering pipeline (GL 3.3 core / GLES 3.0 common subset)
1. Shadow pass: the moon's orthographic shadow map (1024² on desktop, 512² on the Mobile preset, a single cascade
   aligned to the view and snapped to texels to stop shimmering).
2. Opaque voxel chunks → low-res colour and depth target (480×270 default).
3. Dynamic voxel models (characters, debris) drawn as instanced voxel meshes.
4. Sky: full-screen pass for the gradient, hashed starfield, moon disc and aurora.
5. Particles (snow, blood, dust) as point-sprite pixels.
6. Post-processing at low resolution: emissive glow (a 2-pass box blur at ¼ res),
   fog, then palette mapping using a 3D LUT (32³ → 64-colour palette) plus a 4×4 Bayer
   dither.
7. Upscale blit to the window with integer scale and nearest filtering.
One shader source per stage, prefixed with `#version 330 core` or
`#version 300 es` + precision lines when it is loaded.
*Alternative:* a separate renderer per platform. Rejected because it doubles the work
and breaks cross-platform golden images.
*XR impact:* steps 1–6 run once per eye at a higher internal resolution (D9).

### D5. Light: flood-fill per voxel combined with a moon shadow map
Sky and block light use the classic BFS flood fill with incremental add and remove
queues when voxels change. Moonlight uses the shadow map, because its long, crisp
shadows are the main feature of the night look. Fire colour comes from a small table
of emitter colours selected by the nearest emitter, which is enough for "warm vs.
necromantic". *Alternative:* per-pixel shadowed point lights (too costly on mobile).

### D6. Deformation, connectivity and debris
- Edits go through `wn_edit_*` functions that mark chunks dirty, queue light updates
  and record the change in a per-chunk **modification log**, so player changes
  survive streaming (spec "Player changes survive unloading").
- After an edit, a bounded flood fill starts from the edited area's solid neighbours.
  If it hits an anchor or exceeds N voxels (default 64k) before finishing, the area
  counts as connected. Smaller islands without an anchor become debris. *Trade-off:*
  very large overhangs never fall. That is acceptable, and it bounds the cost.
- Debris is a small voxel grid with a rigid body (OBB vs. voxel-grid collision,
  impulse solver, sleeping). Our own small implementation, about 1k lines, instead of
  an external physics engine, so it stays deterministic and testable.

### D7. Determinism
- The simulation step only uses fixed-point or carefully ordered float maths with
  `-ffp-contract=off`, no `-ffast-math`, and the same compiler-independent math
  functions (our own `wn_sinf`, `wn_sqrtf` wrappers, using IEEE `sqrt` which is
  exact). Order-dependent loops (light queues, debris lists) iterate in a stable
  order.
- One seeded PCG32 stream per subsystem. No `rand()`, no time-based seeds.
- Per-step state checksum (xxHash32 over the world modification logs, entities and
  RNG states), used by replays and cross-platform tests.
- Rendering may vary between targets. The simulation may not.

### D8. Snow and blood as cellular automata on the voxel grid
- Snow accumulation: per simulation tick, a budgeted set of columns (round-robin)
  receives fill if the column's highest solid voxel has sky light 15. Avalanches use
  a sand-pile rule on fill levels with a limited number of cells per tick.
  Footprints lower fill by a footprint stamp.
- Blood: particles live in a fixed pool. When they collide they write stain bits.
  On snow they add a 6-neighbour spread at ½ intensity. Pools are liquid voxels
  ("blood") with fill levels and a flow rule that freezes into "frozen blood" after
  T seconds when the temperature is below zero. Stain colour depends on age, which is
  kept at low resolution in a separate per-chunk age map (only allocated when stains
  exist).
- Gore level is read at the emission point, so systems never create content
  that is turned off.

### D9. WebXR through an Emscripten JS library
`platform/web/wn_webxr.js` (an `--js-library`) handles `navigator.xr`:
session request, reference space, `XRWebGLLayer` on the same WebGL2 context SDL
created, and per-frame `getViewerPose`. Each XR frame calls an exported C
function `wn_xr_frame(views, count, framebuffer)`. During an XR session the normal
`emscripten_set_main_loop` is paused, and `XRSession.requestAnimationFrame` drives
the loop. Controller poses and buttons are put into the same action system
(spec "Action mapping"). Internal resolution in XR: 50% of the recommended eye size,
nearest upscale, palette dithering optional (default on, with a less noticeable
blue-noise pattern so it doesn't swim between the eyes).
*Alternative:* WebXR via a third-party framework (three.js etc.). Rejected because
of the C-only constraint and because it would mean a second renderer.

### D10. Input and touch
SDL3 already delivers touch (`SDL_EVENT_FINGER_*`), gamepads and keyboard/mouse
on all targets. The core gets a per-step `wn_input_frame` of action values. The
touch UI (stick, buttons) is drawn by the engine's own 2D overlay so it appears in
the golden images and works the same in all browsers. "Is this a touch device?"
comes from `matchMedia('(pointer: coarse)')` through a tiny JS helper, plus the
first finger event.

### D11. Build and dependencies
- **CMake ≥ 3.25** with presets: `linux-debug`, `linux-release`, `linux-coverage`,
  `linux-asan`, `windows-msvc`, `web-release`, `web-debug`.
- SDL3 via `FetchContent` (pinned tag) on desktop, and the Emscripten SDL3 port
  (`--use-port=sdl3`) on the web.
- Vendored single-file libraries: Unity (tests), cglm (math, header-only),
  xxHash, stb_image_write (golden-image PNGs, tests only).
- Output: `winternight` / `winternight.exe`, and `web/` with `index.html`,
  `.js`, `.wasm`, `.data`.

### D12. Test strategy
| Layer | Tool | Gate |
|-------|------|------|
| Unit (core) | Unity + CTest, built with `--coverage`, reported by gcovr | ≥ 95% lines on `engine/` + `game/`. Branch coverage reported, aim ≥ 85% |
| Sanitizers | ASan + UBSan on unit tests and game tests (Linux) | zero findings |
| Game tests (native) | `winternight --test <script>` runs with the SDL offscreen video driver and Mesa llvmpipe (EGL surfaceless) | all pass, with golden images within tolerance |
| Game tests (web) | Playwright (Chromium): page loads, replay checksum, touch emulation, WebXR via IWER | all pass |
| Determinism | Same replay on Linux, Windows and web, then compare checksums | identical |
| Performance | Benchmark fly-through, record frame times (not gated in CI; tracked) | report |

Game test scripts are plain text:
```
scene    prologue
seed     1234
replay   tests/game/replays/prologue_walk.wnrec
at 600   expect player.pos.z > 40
at 600   capture benchmark_road   tolerance 3 maxdiff 0.5%
at 3600  expect event scene_complete
```
Golden images are stored at internal resolution (480×270 PNG, about 50 KB each),
so the repository stays small and llvmpipe output stays stable.

### D13. Repository layout
```
CMakeLists.txt  CMakePresets.json
engine/   include/wn/*.h  src/*.c
game/     src/*.c  data/scenes/*.scene  data/materials.txt  data/palette.txt
platform/ sdl/  gl/  web/ (shell.html, wn_webxr.js, wn_touch.js)
tests/    unit/  game/ (scripts, replays, golden/)  web/ (Playwright)
third_party/  docs/  openspec/
.github/workflows/ci.yml
```

### D14. Mobile-first performance budget
The game must run smoothly on an **average phone**, so the phone, not the desktop,
sets the budget. Every feature is designed to fit it first.
- **Reference devices** (widely used hardware rather than enthusiast devices):

  | Target | Reference device | Browser / OS | Why |
  |--------|------------------|--------------|-----|
  | Android phone | **Samsung Galaxy A54** (Exynos 1380, Mali-G68) | Chrome | Samsung's best-selling mid-range line; a typical "average phone" |
  | iPhone | **iPhone 13** (A15) | Safari | Widely used older iPhone; Safari is the strictest WebGL2/memory target |
  | Desktop | **NVIDIA GeForce RTX 3060 class GPU, 6-core CPU, 16 GB RAM, 1080p** | Windows 11 + Chrome, Linux | Top of the Steam Hardware Survey for years; the typical gaming PC |
  | Low-end desktop | **Intel Iris Xe integrated graphics** laptop | Windows 11 + Chrome | Common office/student laptop; must reach the Standard preset at 60 fps |
  | XR headset | **Meta Quest 3** | Meta Quest Browser | The most widely used standalone headset; 72 Hz minimum, 90 Hz target |

- **CI proxy for phones:** real devices can't run on every commit, so CI also runs the
  benchmark in Chromium with the **Lighthouse standard mobile profile** (4× CPU slowdown,
  412×823 viewport, Moto G Power emulation) and fails if the Mobile preset's CPU frame
  time goes over budget. The real reference devices are measured before each milestone
  is accepted (task 11.2).
- **Target:** 60 fps (95th-percentile frame ≤ 16.7 ms), never over 33 ms
  (99th percentile). Smoothness beats detail.
- **Mobile preset budget per frame:** ≤ 150 draw calls, ≤ 250k triangles,
  6-chunk view radius hidden by fog, 512² shadow map, ≤ 2,000 particles, glow at ¼
  of the internal resolution, internal resolution 480×270 (dynamic down to 320×180).
- **CPU:** meshing and light updates are time-sliced on the main thread (no reliance
  on threads), at most 4 ms per frame for world updates.
- **Memory:** WASM heap ≤ 384 MiB, texture memory ≤ 32 MiB, download ≤ 15 MiB compressed.
- **Automatic scaling:** if frames stay over budget, first lower the internal
  resolution, then the view radius, then particles (spec "Quality presets and
  automatic scaling").
- **Battery and heat:** 60 fps cap. No rendering when the tab is hidden.
*Why it works:* the retro look (low internal resolution, flat shading, no textures)
is cheap on fill rate, which is what limits phone GPUs. The main costs are mesh
count and meshing time, so greedy meshing and the remesh budget (D3, D6) matter most.
*Desktop impact:* desktop presets only raise radius and resolution. Visual rules
stay the same, so golden images are taken on the Mobile preset.

### D15. Provisional voxel size
The voxel size stays at 10 cm until the user has seen the first playable version
(task 9.5). To keep that change cheap: one constant, materials and scene files in
metres rather than voxels, prefabs stored as voxel models with their own scale that
are resampled when loaded, and unit tests run at two voxel sizes in CI.

### D16. Implementation notes (milestone 1)
Decisions taken while building the first playable version. They refine D1–D15;
where they differ, this section wins.
- **Colour variation in the shader.** The vertex carries a 16-bit *surface word*
  (material, damage, stain) instead of a baked colour. The fragment shader picks
  the ramp colour with the same integer hash as the CPU (`wn_voxel_color`), so
  equal surfaces still merge into large greedy quads. Baking per-voxel colours had
  blocked merging (5 million triangles in view).
- **Partial tops are merged.** Snow tops of equal fill height merge greedily.
  Generated snow uses quarter-voxel steps, and low risers are shaded like the top
  surface (`SOFT_STEP`) so gentle slopes do not stripe.
- **Distance level of detail for chunks.** Each column is meshed at full, half or
  quarter detail by its ring distance (`wn_view`). The rings belong to the quality
  preset (Mobile: full detail only in the player's column). All chunks of a column
  are combined into one mesh, which means one draw call per column, with frustum
  culling per column.
- **Far scenery.** Beyond the streamed square, a coarse grid of the whole scene
  (1.6 m cells, 3.2 m on Mobile) is drawn. The shader discards it inside the
  streamed square, and fog turns it into a silhouette. Emissive windows fog
  three times less, so the castle reads from far away.
- **Sky light decays faster.** Sky light spreads sideways with a decay of 3 per
  voxel (`wn_world.sky_decay`). At 10 cm voxels a decay of 1 floods whole forests
  with flood-fill work. Moonlight has its own shadow map, so this only affects
  ambient light under cover. Block light (fire) keeps a decay of 1.
- **Streaming is budgeted per chunk**, nearest columns first. Lighting runs when a
  column is complete. Players cannot walk into a column that is not loaded yet.
- **Meshing runs on the main thread** within a time budget (3 ms on Mobile). The
  job system with worker threads (D3) is deferred until profiling on the reference
  phones shows it is needed.
- **Determinism.** Cosmetic particles (snowflakes, dust) live in their own pool and
  RNG stream and are excluded from the checksum. Never draw several random
  numbers inside one function call's argument list: GCC and Clang evaluate
  arguments in different orders. Surface heights use floor division, because C
  division truncates towards zero below y = 0. Native and web traces are
  bit-identical over the test runs.
- **Headless tests use Xvfb with Mesa llvmpipe** (GLX), not EGL surfaceless. The
  GPU-free runner `wn_simtest` checks the determinism checksums on machines
  without OpenGL (Windows CI).
- **Math.** A small own math header (`wn_base.h`) replaces cglm: polynomial
  `wn_sinf`/`wn_cosf`/`wn_atan2f` keep the simulation independent of libm.
- **Chopping** is a diagonal stroke plus vertical strokes in front of the body,
  4.5 damage each: wood breaks in one hit, frozen earth needs two.

## Risks / Trade-offs

- [llvmpipe images differ between Mesa versions] → Pin the CI image, use tolerances,
  and keep golden images low-res and free of texture filtering.
- [Floating-point drift across compilers and WASM breaks replay determinism] → Keep
  the simulation maths simple, use no transcendental libm calls in the simulation
  path, and run the cross-platform checksum test on every commit to catch drift early.
- [95% coverage pushes people toward trivial tests] → Specs give the scenarios that
  tests must cover. Code review checks assertion quality, not just coverage numbers.
- [WebXR performance on standalone headsets] → Lower internal resolution, smaller view
  radius in XR, and fixed foveation where available. Budget enforced by the benchmark.
- [No threads without COOP/COEP] → Main-thread time-sliced meshing fallback.
  Document the hosting headers.
- [Mobile memory limits (iOS Safari)] → Uniform-chunk compaction, a smaller view
  radius on mobile, and a WASM heap cap that is tested in the browser suite.
- [Large destruction (dragon through the roof) overloads remeshing] → Remesh budget
  with priorities, a debris cap, and turning small debris into particles.
- [Gore can block distribution] → Gore levels from day one (spec "Gore level setting").

## Migration Plan

Not applicable (greenfield project).

## Open Questions

- Voxel size (world and characters): decided with the user after the first playable
  version (D15, task 9.5). The engine keeps it configurable until then.
- Worker-thread meshing and lighting (D3): needed on the reference phones or not?
  Decide after measuring on real devices (task 11.2).
