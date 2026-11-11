# Spec Delta

## Purpose

Lets any voxel structure — terrain, buildings, trees, creatures — be damaged,
carved, broken apart and rebuilt during play.

## ADDED Requirements

### Requirement: Damage accumulates per voxel
Applying damage to a voxel SHALL raise its damage value in proportion to the damage divided by the material's hardness, and SHALL destroy the voxel (turn it into air) when the damage limit is reached.

#### Scenario: Soft material breaks first
- **WHEN** the same damage is applied once to a snow voxel and once to a stone voxel
- **THEN** the snow voxel is destroyed and the stone voxel is only damaged

#### Scenario: Repeated hits destroy
- **WHEN** enough hits are applied to a wood voxel to exceed its hardness
- **THEN** the voxel becomes air

### Requirement: Shaped edits
The engine SHALL support carving and adding voxels in spheres, boxes and swept capsules (for blade swings), with damage falling off from the centre for spherical impacts.

#### Scenario: Sword swing carves a slice
- **WHEN** a capsule sweep with sword damage passes through a snow drift
- **THEN** snow voxels along the swept path are removed and voxels outside it are unchanged

#### Scenario: Explosion falloff
- **WHEN** a spherical impact of radius 1 m hits a wooden wall
- **THEN** voxels near the centre are destroyed and voxels near the edge are only damaged

### Requirement: Detached pieces become debris
After an edit, voxel groups no longer connected to an anchored voxel (bedrock or a voxel marked as anchor) SHALL be removed from the grid and turned into dynamic debris objects that keep their voxels.

#### Scenario: Cutting a tree trunk
- **WHEN** a ring of voxels around the base of a tree trunk is destroyed
- **THEN** the trunk and crown become one debris object that falls under gravity

#### Scenario: Connected structure stays
- **WHEN** a hole is cut in the middle of a wall that is still connected to the ground on both sides
- **THEN** no debris object is created

### Requirement: Debris physics and settling
Debris objects SHALL move with rigid-body physics against the world, SHALL break into smaller pieces on hard impacts, and SHALL either be written back into the grid or removed when they come to rest.

#### Scenario: Small debris is cleaned up
- **WHEN** a debris object smaller than the configured minimum size comes to rest
- **THEN** it is removed and replaced by a short particle effect

#### Scenario: Large debris settles into the world
- **WHEN** a fallen tree comes to rest on the ground
- **THEN** its voxels are written back into the world grid at their resting position

### Requirement: Dynamic voxel models
Characters and creatures SHALL be voxel models attached to animated bones, and damage to them SHALL remove or stain voxels of the model, with severed parts becoming debris.

#### Scenario: Skeleton loses an arm
- **WHEN** a blade sweep cuts through all voxels joining a skeleton's arm to its shoulder
- **THEN** the arm becomes debris and the skeleton keeps animating without it

### Requirement: Edit budget
Edits SHALL be applied in the simulation step they are issued, while remeshing of affected chunks SHALL be spread across frames within a configurable time budget, with the chunks nearest the camera first.

#### Scenario: Large destruction keeps the frame rate
- **WHEN** 50 chunks are changed in one simulation step
- **THEN** no frame spends more than the remesh budget on meshing and all chunks are updated within a bounded number of frames
