# Spec Delta

## Purpose

Lights the winter night with cold moonlight and starlight against warm fire,
which is the central contrast of the game's look.

## ADDED Requirements

### Requirement: Moonlight with shadows
Each scene SHALL define a moon direction, colour and intensity, and the renderer SHALL light the world with it and cast shadows from voxel geometry.

#### Scenario: Tree casts a moon shadow
- **WHEN** the moon is low in the west and a pine stands on open snow
- **THEN** the snow east of the pine is darker than open snow

### Requirement: Night sky
The renderer SHALL draw a procedural night sky with a moon disc, a starfield with some twinkling stars and an optional aurora, all fixed relative to the world rather than the camera position.

#### Scenario: Stars stay in place
- **WHEN** the camera moves forward 100 m without rotating
- **THEN** the stars stay at the same screen positions

### Requirement: Flood-fill block light
Light-emitting voxels (fire, torches, lit windows) SHALL spread coloured light through non-solid voxels, decreasing by one level per voxel, and the light SHALL update when voxels are added or removed.

#### Scenario: Light spills out of a window
- **WHEN** a room lit by a hearth has a window opening
- **THEN** voxels outside the window receive warm light and voxels behind the solid wall do not

#### Scenario: Breaking a wall lets light out
- **WHEN** part of a wall between a lit room and the outside is destroyed
- **THEN** voxels outside next to the new hole receive light

### Requirement: Sky light
Voxels with an open view of the sky SHALL receive sky light, spread downward and sideways like block light, so caves and interiors are darker than the outside.

#### Scenario: Interior is darker
- **WHEN** sky light is computed for a closed building
- **THEN** voxels inside have lower sky light than voxels outside

### Requirement: Emissive glow
Light-emitting voxels SHALL be drawn at full brightness regardless of shading and SHALL produce a soft glow halo around bright areas of the image.

#### Scenario: Windows glow from afar
- **WHEN** lit castle windows are viewed from 200 m
- **THEN** the window pixels are brighter than the fogged surroundings and a halo is visible

### Requirement: Dynamic light colour change
A scene event SHALL be able to change the colour of selected light sources over time.

#### Scenario: Fire turns necromantic
- **WHEN** the necromancer-arrival event fires in the feast hall
- **THEN** hearth and torch light change from warm orange to green-cyan over the event's duration
