# Winternight

A retro-3D fantasy action game on a fully deformable voxel engine.

> A warrior crosses a snow-buried forest under the moon to a feast in a castle
> whose windows glow with firelight. A necromancer takes the castle and carries
> the mage away on a dragon of bone. The warrior must break free and follow.

**Status:** first playable version: the Prologue, *The Road to Hearthholm*.
Walk the moonlit forest road, wade through snow, chop through the fallen pine
that blocks the road, and reach the gate of Castle Hearthholm. The story is a
working draft, and the voxel size (10 cm) is provisional until it has been
reviewed (design D15).

**Play it in the browser:** <https://stbange.github.io/winternight/>. It is rebuilt
from `main` on every push. On a phone, touch controls appear. In a VR headset
browser, use **Enter VR**.

## Concept and specs

- [Game concept](docs/concept/game-concept.md): story outline, characters, pillars, gameplay
- [Visual style guide](docs/concept/visual-style.md): retro rendering, palette, lighting, snow, blood, destruction
- [Performance notes](docs/performance.md): budgets, reference devices, measurements
- [OpenSpec change `establish-winternight-concept`](openspec/changes/establish-winternight-concept/):
  [proposal](openspec/changes/establish-winternight-concept/proposal.md),
  [design](openspec/changes/establish-winternight-concept/design.md),
  [specs](openspec/changes/establish-winternight-concept/specs/),
  [tasks](openspec/changes/establish-winternight-concept/tasks.md)

## Controls

| Action | Keyboard + mouse | Gamepad | Touch (phones/tablets) | WebXR |
|--------|------------------|---------|------------------------|-------|
| Move | W A S D / arrows | Left stick | Left half: virtual stick | Left stick |
| Look | Mouse (click to capture), ← → | Right stick | Right half: drag | Head, right stick turns |
| Attack / chop | Left mouse, F | X / Right trigger | ATK | Swing the right controller (or trigger) |
| Block | Right mouse, Ctrl | Left trigger | BLK | Grip |
| Jump | Space | A | JMP | A |
| Run | Shift | Left stick press | – | – |
| Frame-time overlay | F3 | – | – | – |
| Fullscreen | F11 | – | – | – |

Touch controls appear automatically on touch devices and hide again when a
keyboard or gamepad is used.

## Building

Requirements: CMake ≥ 3.25, a C11 compiler (GCC, Clang or MSVC), Ninja (optional).
SDL3 is fetched automatically (pinned to `release-3.4.20`), or pass
`-DWN_SDL3_SOURCE_DIR=<path>` to use a local checkout.

### Linux

```bash
sudo apt-get install -y ninja-build libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libegl-dev libgl-dev libgles-dev
```

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
```

```bash
./build/winternight
```

### Windows (Visual Studio 2022)

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

```bash
cmake --build build --config Release
```

```bash
build\Release\winternight.exe
```

### Web (Emscripten, WebGL2, WebXR)

```bash
emcmake cmake -S . -B build/web -DCMAKE_BUILD_TYPE=Release -DWN_BUILD_TESTS=OFF
```

```bash
cmake --build build/web
```

```bash
python3 -m http.server -d build/web 8000
```

Open <http://localhost:8000/winternight.html>. A VR headset browser (for
example the Meta Quest Browser) shows an **Enter VR** button. WebXR needs a
secure context (HTTPS or localhost).

Useful options: `--preset mobile|standard|high`, `--stats`, `--seed N`,
`--record FILE` / `--replay FILE`, `--benchmark`. On the web the same options
are URL parameters, for example `?preset=mobile&stats`.

## Tests

### Unit tests (core, ≥ 95% line coverage)

```bash
cmake -S . -B build/cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWN_COVERAGE=ON -DWN_BUILD_GAME=OFF
```

```bash
cmake --build build/cov && ctest --test-dir build/cov
```

```bash
gcovr -r . build/cov --filter engine/ --filter game/ --exclude-unreachable-branches --fail-under-line 95 --print-summary
```

The voxel size is a build option (`-DWN_VOXEL_SIZE=0.05f`); CI runs the unit
tests at 10 cm and 5 cm. `-DWN_SANITIZE=ON` builds with ASan and UBSan.

### Automated game tests (golden images, headless)

Scripts in `tests/game/scripts` drive the real game step by step (scripted
input, an autopilot that walks the road, checks, captures). Captured frames are
compared with `tests/game/golden` (480×270, Mobile preset).

```bash
xvfb-run -a build/winternight --test tests/game/scripts/prologue_playthrough.wntest --golden tests/game/golden --out build/test-output
```

Add `--update-golden` to regenerate golden images after an intended visual
change. `build/wn_simtest SCRIPT` runs a script without a GPU (determinism checks).

### Browser tests (web build, touch emulation, WebXR emulation)

```bash
cd tests/web && npm ci && npx playwright install chromium && npm test
```

They cover loading, the WebGL2 error message, pausing in hidden tabs, touch
controls on an emulated phone, WebXR with an emulated Meta Quest 3 (IWER), and
the determinism script in the browser.

## Repository layout

```
engine/    voxel engine core (pure C11: world, light, meshing, deformation, snow, blood, render logic)
game/      game rules, scenes, test runner (pure C11)   game/data: materials, palette, prefabs, scenes
platform/  SDL3 app, OpenGL 3.3 / WebGL2 renderer, web shell and WebXR bridge
tests/     unit/ (Unity), game/ (scripts + golden images), web/ (Playwright)
tools/     wn_simtest (GPU-free simulation runner)
openspec/  specs and the change plan          docs/  concept and performance notes
```

## Third-party code

SDL3 (zlib), Unity test framework (MIT), xxHash (BSD-2), stb_image / stb_image_write
(public domain / MIT), IWER (MIT, tests only). Licences are in `third_party/`.
