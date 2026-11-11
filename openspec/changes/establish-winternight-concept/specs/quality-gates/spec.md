# Spec Delta

## Purpose

Keeps the engine and game reliable through high unit-test coverage,
deterministic automated game tests and CI checks on every target.

## ADDED Requirements

### Requirement: Unit test coverage
The platform-independent core (everything not part of the SDL, OpenGL or browser glue layers) SHALL have unit tests with at least 95% line coverage, and CI SHALL fail when coverage is lower.

#### Scenario: Coverage drop fails CI
- **WHEN** a commit lowers core line coverage to 94%
- **THEN** the CI coverage job fails and reports the uncovered files

### Requirement: Thin platform layer
Code that calls SDL, OpenGL or browser APIs SHALL live in a separate platform layer that contains no game logic, and SHALL be covered by automated game tests instead of unit tests.

#### Scenario: Core builds without platform libraries
- **WHEN** the core library is built for the unit tests
- **THEN** it compiles and links without SDL, OpenGL or Emscripten

### Requirement: Automated game tests
The project SHALL include scripted game tests that start the real game headless, feed recorded or scripted input, and check game state and rendered images against expectations.

#### Scenario: Scripted test runs headless
- **WHEN** a game test runs on a CI machine without a display
- **THEN** it renders off-screen and reports pass or fail with a reason

### Requirement: Golden-image checks
Game tests SHALL be able to capture the internal-resolution frame at a given step and compare it with a stored golden image using a per-pixel tolerance and a maximum share of differing pixels, saving a difference image on failure.

#### Scenario: Visual regression detected
- **WHEN** a change alters the colour of moonlit snow in the benchmark shot beyond tolerance
- **THEN** the golden-image test fails and a difference image is saved as a CI artefact

### Requirement: Browser tests
The web build SHALL be tested in a real browser, including a touch-device emulation that checks touch controls appear and move the character, and an emulated WebXR device that checks a session can be entered and rendered.

#### Scenario: Emulated touch device
- **WHEN** the browser test opens the web build with mobile touch emulation
- **THEN** touch controls are visible and a drag on the virtual stick moves the character

#### Scenario: Emulated XR session
- **WHEN** the browser test enters VR using an emulated headset
- **THEN** frames are rendered for two views

### Requirement: Sanitizers and warnings
Linux CI SHALL build and run the unit tests with address and undefined-behaviour sanitizers, and all targets SHALL compile with warnings treated as errors.

#### Scenario: Memory error fails CI
- **WHEN** a commit introduces an out-of-bounds read exercised by a unit test
- **THEN** the sanitizer job fails
