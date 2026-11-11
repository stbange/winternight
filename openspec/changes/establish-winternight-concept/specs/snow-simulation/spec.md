# Spec Delta

## Purpose

Makes deep snow a living part of the world: it falls, piles up, is trampled
and carved, and slows down whoever wades through it.

## ADDED Requirements

### Requirement: Snow uses fill levels
Snow voxels SHALL use the fill level (1–15 of 16 parts) to represent partial height, and a snow surface SHALL be rendered and collided at that height.

#### Scenario: Half-filled snow voxel
- **WHEN** a snow voxel with fill level 8 is rendered and collided
- **THEN** its top surface is at half the voxel height

### Requirement: Snowfall accumulates
While snowfall is active the engine SHALL draw falling snow particles and SHALL raise the fill level of exposed upward-facing snow or solid surfaces at a configurable rate, without filling sheltered areas.

#### Scenario: Open ground accumulates
- **WHEN** snowfall runs for 60 simulated seconds at a rate of 1 fill level per 10 seconds
- **THEN** exposed open ground gains 6 fill levels of snow

#### Scenario: Sheltered ground stays clear
- **WHEN** snowfall runs over ground covered by a roof
- **THEN** the covered ground gains no snow

### Requirement: Footprints and trails
Characters moving through snow SHALL compress the snow under their feet, lowering the fill level, and leave a visible trail that later snowfall slowly fills in.

#### Scenario: Wading leaves a trail
- **WHEN** the warrior walks 5 m through snow with fill depth 4 voxels
- **THEN** voxels along the path have lower fill levels than the untouched snow beside it

#### Scenario: Trails fill in again
- **WHEN** snowfall continues long enough over a trail
- **THEN** the trail's fill levels rise back toward those of the surrounding snow

### Requirement: Snow slows movement
Character movement speed SHALL be reduced in proportion to the snow depth at the character's legs, down to a configurable minimum.

#### Scenario: Deep snow is slower
- **WHEN** the warrior moves with the same input on bare stone and in knee-deep snow
- **THEN** the speed in snow is lower than on stone and not lower than the minimum

### Requirement: Snow on branches falls when disturbed
Snow lying on tree branches SHALL fall as snow particles and land as fill on the ground below when the tree is hit or a character brushes past.

#### Scenario: Hitting a tree drops snow
- **WHEN** the warrior strikes a snow-covered pine
- **THEN** snow is removed from its branches and the ground below gains snow

### Requirement: Snow melts near heat
Snow and ice voxels within range of a heat source SHALL lose fill level over time, with snow turning into nothing and ice turning into water.

#### Scenario: Torch melts snow
- **WHEN** a burning torch lies on snow for 30 simulated seconds
- **THEN** the snow fill level around it has decreased

### Requirement: Snow avalanche
Granular snow piled above a configurable slope angle SHALL slide down when disturbed, moving fill from higher voxels to lower neighbours until stable.

#### Scenario: Steep pile collapses
- **WHEN** a column of snow 10 voxels high is placed on flat ground and disturbed
- **THEN** it spreads out until no neighbouring height difference exceeds the slope limit
