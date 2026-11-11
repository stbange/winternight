# Proposal

## Why

Winternight is a new project with no code, specs or agreed direction yet. Before
writing engine code we need one shared description of what we are building:
the fantasy and mood (see `docs/concept/`), the voxel engine's abilities
(deformation, snow, blood, night lighting, retro rendering), the four platform
targets, and the quality gates. Getting these right early matters because they
are expensive to retrofit: the voxel data layout and determinism decide whether
deformation, snow, blood and automated testing are possible at all.

## What Changes

- Add the game concept and visual style guide (`docs/concept/game-concept.md`,
  `docs/concept/visual-style.md`).
- Define the engine and game capabilities below as OpenSpec requirements.
- Record the architecture and technology decisions in `design.md`: C11, SDL3,
  OpenGL 3.3 / GLES 3.0 / WebGL2, CMake, Emscripten, WebXR bridge, test stack.
- Plan the first milestone in `tasks.md`: engine foundation plus the
  **Prologue vertical slice** ("The Road to Hearthholm"), which is the visual
  benchmark scene.

**Out of scope for this change:** final story and dialogue, audio, save games,
combat balancing, Acts I–IV content, macOS/consoles, native desktop VR (OpenXR),
multiplayer, and any implementation code (this change only plans).

## Capabilities

### New Capabilities
- `voxel-world`: chunked voxel storage, materials, world streaming and queries.
- `voxel-deformation`: damaging, carving, adding and detaching voxels, including debris.
- `snow-simulation`: snow as fill-level voxels, snowfall, accumulation, footprints, branch snow.
- `blood-effects`: blood droplets, stains, pools, freezing, gore level setting.
- `retro-rendering`: low-res render target, palette quantisation, dithering, fog, voxel meshing.
- `night-lighting`: moonlight with shadows, sky with stars, flood-fill fire light, emissive windows.
- `platform-targets`: Windows, Linux, Web builds from one codebase; main loop and timing.
- `input-controls`: action mapping for keyboard/mouse, gamepad, touch and XR controllers.
- `webxr-mode`: immersive VR sessions in the web build.
- `game-flow`: scenes, the Prologue vertical slice and its triggers.
- `quality-gates`: unit test coverage, determinism, automated game tests, CI.

### Modified Capabilities
- None (greenfield project).

## Impact

- New repository layout (`engine/`, `game/`, `platform/`, `tests/`, `web/`, `docs/`).
- Dependencies: SDL3 (zlib), Emscripten SDK, Unity test framework (MIT),
  cglm (MIT) for math, gcovr for coverage, Playwright plus the Immersive Web Emulation
  Runtime (IWER) for browser and WebXR tests.
- CI: GitHub Actions matrix (Linux, Windows, Web) with coverage gates.
