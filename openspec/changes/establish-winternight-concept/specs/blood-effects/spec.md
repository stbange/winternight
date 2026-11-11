# Spec Delta

## Purpose

Shows the violence of combat through blood that sprays, stains surfaces,
pools and freezes, at a level the player can choose.

## ADDED Requirements

### Requirement: Wounds emit blood droplets
Damage to a creature that bleeds SHALL emit blood droplet particles from the wound, with count and speed scaled by damage and direction following the hit.

#### Scenario: Stronger hit, more blood
- **WHEN** a creature that bleeds receives a light hit and then a heavy hit
- **THEN** the heavy hit emits more droplets than the light hit

#### Scenario: Undead emit dust instead
- **WHEN** a skeleton is hit
- **THEN** bone-dust particles are emitted and no red blood droplets

### Requirement: Droplets stain voxels
A blood droplet that hits a voxel SHALL add blood stain intensity to that voxel, and on snow SHALL also spread a weaker stain to neighbouring snow voxels.

#### Scenario: Blood on stone
- **WHEN** a droplet hits a stone voxel
- **THEN** that voxel gets a blood stain and its neighbours do not

#### Scenario: Blood seeps into snow
- **WHEN** a droplet hits a snow voxel
- **THEN** that voxel gets a blood stain and adjacent snow voxels get a weaker stain

### Requirement: Stains age
Blood stains SHALL change colour from fresh to dried over time, and SHALL be covered by new snow when snowfall raises the fill above them.

#### Scenario: Stain dries
- **WHEN** 120 simulated seconds pass after a stain is created
- **THEN** the stain is rendered with the dried blood colour

### Requirement: Blood pools and freezes
Heavy bleeding SHALL create liquid blood voxels that flow to lower neighbours for a limited time and then freeze into a solid, stained surface in cold areas.

#### Scenario: Pool forms on flat ground
- **WHEN** a creature bleeds heavily while lying on flat stone
- **THEN** liquid blood voxels appear around it and stop flowing within the freeze time

### Requirement: Gore level setting
The player SHALL be able to choose a gore level of Full, Reduced (fewer droplets, no pools, no severed parts showing flesh) or Off (no blood particles, stains or pools), which applies immediately.

#### Scenario: Gore off
- **WHEN** the gore level is Off and a creature is hit
- **THEN** no blood particles, stains or pools are created

#### Scenario: Reduced gore
- **WHEN** the gore level is Reduced and a heavy hit occurs
- **THEN** fewer droplets than at Full are emitted and no pools are created
