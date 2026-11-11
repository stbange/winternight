# Spec Delta

## Purpose

Stores the whole game world as a grid of material voxels that can be read,
changed and streamed in and out efficiently on every target.

## ADDED Requirements

### Requirement: World is a grid of voxels in chunks
The world SHALL be made of cubic voxels whose edge length is one engine-wide setting (default 0.1 world units = 10 cm, provisional), grouped into chunks of 32×32×32 voxels addressed by integer chunk coordinates. No other code SHALL depend on the specific voxel size.

#### Scenario: World position maps to a voxel
- **WHEN** the world is queried at position (1.25, 0.05, -0.01)
- **THEN** the voxel at integer coordinates (12, 0, -1) is returned, inside chunk (0, 0, -1)

#### Scenario: Voxel size can be changed
- **WHEN** the engine is built with a voxel size of 0.05 and position (1.25, 0.05, -0.01) is queried
- **THEN** the voxel at integer coordinates (25, 1, -1) is returned and all unit tests still pass

#### Scenario: Negative coordinates are handled correctly
- **WHEN** voxel (-1, -33, 0) is written and read back
- **THEN** the same value is returned and it lies in chunk (-1, -2, 0)

### Requirement: Voxel state
Every voxel SHALL store a material id, a fill level (0–15), a damage value (0–15), a stain (type and intensity) and light levels (sky and emitted). Air SHALL be material id 0.

#### Scenario: Default voxel is air
- **WHEN** a voxel in a newly created empty chunk is read
- **THEN** it has material air, fill 0, damage 0, no stain and no light

#### Scenario: Fields are independent
- **WHEN** the stain of a stone voxel is changed
- **THEN** its material, fill, damage and light are unchanged

### Requirement: Material table
The engine SHALL provide a data-driven material table defining for each material its colour ramp, hardness (hit points), density, flammability, whether it is solid, liquid or granular, and whether it emits light.

#### Scenario: Material properties are looked up
- **WHEN** the material table is loaded with "snow", "pine-wood", "stone" and "ice"
- **THEN** hardness increases in the order snow < pine-wood < ice < stone

#### Scenario: Invalid material data is rejected
- **WHEN** a material definition has a missing name or a negative hardness
- **THEN** loading fails with an error naming the bad entry and no partial table is used

### Requirement: Memory-efficient chunk storage
Chunks that contain only one voxel value SHALL be stored without a per-voxel array, and SHALL expand when written to and compact when they become uniform again.

#### Scenario: Uniform chunk stays compact
- **WHEN** a chunk filled entirely with air is created
- **THEN** it uses the compact representation

#### Scenario: Chunk compacts after becoming uniform
- **WHEN** the only stone voxel in an otherwise empty chunk is removed and the chunk is compacted
- **THEN** the chunk returns to the compact representation

### Requirement: Streaming around the viewer
The world SHALL keep chunks loaded within a configurable radius of the viewer, generate or load chunks entering the radius and release chunks leaving it, while keeping changes made by the player.

#### Scenario: Chunks load as the viewer moves
- **WHEN** the viewer moves 10 chunks along +X with a radius of 6
- **THEN** chunks within 6 of the new position are loaded and chunks further than radius + 1 are unloaded

#### Scenario: Player changes survive unloading
- **WHEN** a voxel is destroyed, its chunk is unloaded and later loaded again
- **THEN** the voxel is still destroyed

### Requirement: Deterministic generation
World generation SHALL be deterministic for a given seed and scene description on all targets.

#### Scenario: Same seed gives same world
- **WHEN** the Prologue forest is generated twice with seed 1234, once on a desktop build and once on the web build
- **THEN** the checksums of all generated chunks are identical

### Requirement: Spatial queries
The world SHALL support ray casts against solid voxels (returning hit voxel, face normal and distance) and axis-aligned box overlap tests.

#### Scenario: Ray hits the first solid voxel
- **WHEN** a ray is cast from above toward a stone floor covered by 3 voxels of snow
- **THEN** the hit is the top snow voxel with an upward normal

#### Scenario: Ray misses
- **WHEN** a ray of length 5 is cast into empty sky
- **THEN** no hit is reported
