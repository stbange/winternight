# Spec Delta

## Purpose

Builds and runs the same game from one codebase on Windows, Linux and the web,
with identical simulation results.

## ADDED Requirements

### Requirement: Supported targets
The project SHALL build from one source tree for Windows (x64), Linux (x64) and the web (WebAssembly via Emscripten, WebGL2), each using its required graphics API: OpenGL 3.3 core on desktop and WebGL2 on the web.

#### Scenario: All targets build in CI
- **WHEN** CI runs on a commit to main
- **THEN** Windows, Linux and web builds all succeed

### Requirement: Fixed simulation step
Game simulation SHALL run at a fixed 60 Hz step, decoupled from the render frame rate, with rendering interpolating between the last two simulation states.

#### Scenario: Low frame rate does not change the game
- **WHEN** the same input recording runs once at 144 rendered fps and once at 20 rendered fps
- **THEN** the simulation state after the recording is identical

### Requirement: Cross-platform determinism
Given the same seed, settings and input recording, the simulation SHALL produce identical state checksums on every target.

#### Scenario: Same replay, same result everywhere
- **WHEN** the Prologue replay test runs on Windows, Linux and web
- **THEN** all three report the same final state checksum

### Requirement: Browser main loop
The web build SHALL run without blocking the browser's main thread and SHALL pause the simulation when the tab is hidden.

#### Scenario: Hidden tab pauses
- **WHEN** the browser tab is hidden for 10 seconds and shown again
- **THEN** the simulation has not advanced by those 10 seconds

### Requirement: Graceful start-up failure
If the required graphics API is unavailable, the game SHALL show a readable error (a message box on desktop, an HTML message on the web) instead of crashing.

#### Scenario: No WebGL2
- **WHEN** the web build starts in a browser without WebGL2
- **THEN** the page shows a message that WebGL2 is required

### Requirement: Performance budgets
The Prologue benchmark scene SHALL run smoothly on an average phone: 60 fps in the reference mobile browser with no frame over 33 ms in normal play. It SHALL also reach 60 fps on the reference desktop and desktop browser, and the headset's native refresh rate in WebXR on the reference headset.

#### Scenario: Average phone benchmark
- **WHEN** the automated benchmark flies the camera along the Prologue road in the browser of the reference phone with the default mobile preset
- **THEN** the 95th-percentile frame time is at most 16.7 ms and the 99th-percentile frame time is at most 33 ms

#### Scenario: Desktop benchmark
- **WHEN** the automated benchmark flies the camera along the Prologue road on the reference desktop
- **THEN** the 95th-percentile frame time is at most 16.7 ms

### Requirement: Quality presets and automatic scaling
The game SHALL offer quality presets (Mobile, Standard, High) that set view radius, shadow resolution, particle counts and glow, SHALL pick the Mobile preset on phones by default, and SHALL lower internal resolution or view radius step by step when frame time stays over budget.

#### Scenario: Phone starts on Mobile preset
- **WHEN** the web build starts for the first time on a device with a coarse primary pointer and a small screen
- **THEN** the Mobile preset is active

#### Scenario: Sustained slow frames
- **WHEN** frame time stays above 16.7 ms for 2 seconds
- **THEN** the renderer lowers one quality step and does not raise it again for at least 10 seconds
