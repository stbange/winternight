# Proposal

## Why

Winternight already has a forest-road prototype and an unarchived foundation
change, but its concept documents leave the visual experience and the limits of
an editable voxel world underspecified. This pass makes that direction concrete
while keeping the feast, abduction and rescue story easy to revise.

## What Changes

- Expand the game concept around the player's experience, material interactions
  and a short visual showcase rather than additional lore.
- Expand the visual guide with shot composition, material treatment, readable
  darkness, camera rules and an original SVG composition board.
- Add an engine concept explaining storage, deformation, snow, blood, rendering,
  streaming, platform constraints and explicit resource limits.
- Add a validation plan linking those goals to the existing unit, replay, golden
  image and browser tests, with missing evidence clearly identified.
- Link the package from the README and record deferred decisions and milestones.

**Out of scope:** runtime code, renderer changes, new tests or golden images,
final story or character names, a full campaign, engine implementation, asset
production, performance certification, and approval of the provisional voxel size.

## Capabilities

### New Capabilities

None. This is a documentation change (`skip_specs: true`), not a new runtime
contract. The existing foundation change already specifies the engine features.

### Modified Capabilities

None. Requirements in `establish-winternight-concept` remain unchanged. Proposed
follow-up work in the concept documents must get its own spec delta before
implementation; it must not silently rewrite the previous milestone's scope.

## Impact

Changes are confined to README links, `docs/concept/` and this OpenSpec change.
The C11/SDL3/OpenGL and Emscripten/WebXR stack and the existing >=95% core line
coverage gate remain the basis for the design. No dependency, build, API,
deployment configuration or saved world format changes are introduced.
