# Spec Delta

## Purpose

Describes how the game's scenes are put together and run, starting with the
Prologue vertical slice that proves the engine's look and features.

## ADDED Requirements

### Requirement: Data-driven scenes
Scenes SHALL be defined in data files that set world generation, prefabs (castle, trees, props), lighting, weather, spawn points and triggers, so new scenes need no engine code changes.

#### Scenario: Scene loads from data
- **WHEN** the Prologue scene file is loaded
- **THEN** the forest, road, castle, moon, snowfall and player spawn defined in it are present

#### Scenario: Broken scene file
- **WHEN** a scene file references a prefab that does not exist
- **THEN** loading fails with an error that names the missing prefab

### Requirement: Triggers and events
Scenes SHALL support trigger volumes and timed events that can start other events, change lights or weather, spawn entities and change scenes.

#### Scenario: Reaching the castle gate
- **WHEN** the player enters the castle-gate trigger in the Prologue
- **THEN** the scene-complete event fires

### Requirement: Prologue vertical slice
The game SHALL include a playable Prologue in which the player walks a snowy forest road at night toward a castle with lit windows, must chop through a fallen tree blocking the road, and completes the scene at the castle gate.

#### Scenario: Prologue can be completed
- **WHEN** the scripted Prologue playthrough walks the road, destroys the blocking tree and reaches the gate
- **THEN** the scene-complete event fires within the time limit

#### Scenario: Blocked road
- **WHEN** the player tries to pass the fallen tree without destroying it
- **THEN** the player cannot get past it

### Requirement: Third-person camera
On flat screens the camera SHALL follow the warrior from behind the shoulder, avoid passing through solid voxels and be rotated by the look action.

#### Scenario: Camera avoids walls
- **WHEN** the warrior stands with his back to a stone wall
- **THEN** the camera moves closer to the warrior instead of into the wall
