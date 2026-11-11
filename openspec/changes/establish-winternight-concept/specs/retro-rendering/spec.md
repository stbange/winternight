# Spec Delta

## Purpose

Produces the game's retro 3D look — low resolution, limited palette, dithering
and fog — from the voxel world, the same way on every target.

## ADDED Requirements

### Requirement: Low-resolution render target
The scene SHALL be rendered into an off-screen target at a selectable internal resolution (320×180, 480×270 default, 640×360) and shown on screen with nearest-neighbour scaling at the largest integer factor that fits, with letterboxing.

#### Scenario: Integer upscale on 1080p
- **WHEN** the internal resolution is 480×270 and the window is 1920×1080
- **THEN** the image is scaled by 4 with no filtering and fills the window

#### Scenario: Letterboxing on odd window size
- **WHEN** the internal resolution is 480×270 and the window is 1000×700
- **THEN** the image is scaled by 2 and centred with black borders

### Requirement: Voxel meshing
Chunks SHALL be converted to meshes that contain only faces between solid and non-solid voxels, merge coplanar faces of equal appearance, and carry per-vertex ambient occlusion.

#### Scenario: Hidden faces are culled
- **WHEN** a solid 2×2×2 block of stone is meshed
- **THEN** the mesh contains only the 6 outer faces (as merged quads) and no inner faces

#### Scenario: Meshing is deterministic
- **WHEN** the same chunk and neighbours are meshed twice
- **THEN** both meshes are byte-identical

### Requirement: Per-voxel colour variation
Each voxel SHALL be coloured from its material's colour ramp, picked by a stable hash of its world position, modified by stain and damage.

#### Scenario: Stable colours
- **WHEN** a chunk is unloaded and reloaded
- **THEN** every voxel shows the same colour as before

### Requirement: Palette quantisation and dithering
When the retro palette option is on, the final image SHALL be mapped to the 64-colour master palette using 4×4 ordered dithering; when off, full colour SHALL be shown.

#### Scenario: Only palette colours on screen
- **WHEN** a frame is rendered with the palette option on
- **THEN** every pixel of the internal image is one of the 64 palette colours

### Requirement: Distance fog
The renderer SHALL apply exponential fog toward a scene-defined colour so that geometry at the edge of the loaded radius is fully fogged.

#### Scenario: No visible world edge
- **WHEN** the camera looks toward the edge of the loaded chunk radius
- **THEN** pixels at the streaming boundary equal the fog colour

### Requirement: Optional vertex snapping
The renderer SHALL offer a retro vertex-snapping option that rounds projected vertices to the internal pixel grid, and it SHALL always be disabled in XR sessions.

#### Scenario: Disabled in XR
- **WHEN** an XR session starts with vertex snapping enabled in the options
- **THEN** rendering in the session does not snap vertices

### Requirement: Same image on every target
For the same scene state and settings, desktop and web builds SHALL produce internal images that match within the golden-image tolerance.

#### Scenario: Cross-platform golden image
- **WHEN** the Prologue benchmark shot is rendered on Linux and in the web build
- **THEN** both match the stored golden image within tolerance
