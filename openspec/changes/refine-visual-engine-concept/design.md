# Design

## Context

See [proposal.md](proposal.md) for motivation. The repository already contains a
C11 core, SDL3 application, GL / WebGL2 renderer, WebXR bridge and forest-road
prototype. `establish-winternight-concept` remains active with implementation
history and outstanding hardware / performance tasks. Its early sections also
describe intentions that differ from the prototype. This documentation pass
needs to make those distinctions visible without rewriting that history.

## Goals / Non-Goals

**Goals:** give the visual direction a reviewable form, explain the voxel engine
in terms of visible results and resource costs, and map each major claim to
existing evidence or specific future validation. Keep narrative decisions cheap.

**Non-Goals:** changing cell formats, accepting a voxel scale, implementing a
new collapse scene, modifying runtime requirements, or turning budget targets
into measured claims.

## Decisions

### D1. A documentation-only change with no spec delta

Set `skip_specs: true`. Existing capability requirements already cover voxels,
deformation, snow, blood, rendering, platforms, input, XR and quality gates.
Inventing another runtime capability for a concept document would duplicate
them. Do not archive or mark the original change's unfinished tasks complete.

Alternative: rewrite the foundation change as though the repository were still
empty. Rejected because it obscures what already exists and what remains open.

### D2. Separate the experience, image, engine and evidence

Use `game-concept.md` for the player's experience and a replaceable story
scaffold, `visual-style.md` plus an SVG board for composition and materials,
`voxel-engine.md` for representation and limits, and `validation-plan.md` for
test mapping and review. Link these from the README and from one another.

The SVG uses no scripts, external fonts or remote images. It is labelled as a
schematic study and stays outside `game/data/`. It adds no runtime download or
GPU cost on desktop, web/mobile or WebXR.

Alternative: add a large mood illustration or runtime assets. Deferred because
the current need is a concrete, revisable direction rather than final assets.

### D3. Document a shared engine with explicit bounds

The engine guide keeps the existing rasterized chunk-mesh approach and compares
it with octree ray traversal, smooth density surfaces and fixed meshes. It
defines deformation as editable materials / topology, not elastic deformation
or unbounded fragment physics. It records the actual 32-bit cell convention,
12-byte vertices, retained modified chunks, debris recycling and limited replay
checksum instead of repeating early intentions as implemented facts.

For each system, distinguish existing behavior from proposed follow-ups. In
particular, compression / age persistence, far-proxy invalidation, saturation
rules and canonical hashes need scoped future work before production claims.
Numerical budgets stay provisional; dense-cell arithmetic exposes the memory
cost of finer voxels and expanded light state.

**Web/mobile impact:** no compute dependency or mandatory threads; account for
saved chunks, temporary buffers, local light spikes, touch and thermal limits.
**WebXR impact:** share world state / meshes, account for stereo cost, preserve
physical scale and use stable presentation. Eye scale and dither need hardware
review; no fixed half-resolution setting is endorsed by this document.

Alternative: a technology wishlist without limits. Rejected because it would
leave the most expensive parts of the image unexplained.

### D4. Preserve the winter scene as the first decision point

Place the castle reveal after the initial enclosed walk; distinguish it from
the literal opening frame. Develop a material courtyard, exposed room and one
dragon roof strike in that order as proposed future studies. Separate a blood
material study from the peaceful forest route so visual testing does not force
an early narrative or combat scene.

**Web/mobile impact:** review matched shots and memory at 10 cm and 5 cm before
spending more cells; include the real touch overlay in composition review.
**WebXR impact:** evaluate close interactions at metre scale and headset refresh
rate before promising the large impact scene. Keep snapping and forced camera
motion out of immersive presentation.

Alternative: immediately expand the campaign and simulate the whole castle.
Deferred until the small combined-material studies support it.

### D5. Require evidence beyond the percentage

Retain >=95% line coverage of the entire portable core and report branches.
Map existing unit / game / browser files to each claim and label new scenarios
as future work. Clarify that Linux golden tests, Windows simulation checks and
browser / IWER tests cover different paths. Include human visual review and
real-device measurement; neither can be replaced by screenshot tolerances or
emulated headsets.

**Web/mobile impact:** browser integration and actual phone memory / GPU / heat
measurements supplement core unit coverage. **WebXR impact:** emulated session
tests supplement, but cannot establish, headset comfort and sustained stereo
frame rate. No new test implementation is implied by these documents.

## Risks / Trade-offs

- [Readers mistake a concept for a completed feature] → Label source inventory,
  proposed work, illustrations and unmeasured budgets explicitly.
- [Refinement drifts from earlier specs] → Retain the original change, cite the
  differences and require a new scoped delta before runtime changes.
- [Narrative hardens too early] → Use roles and visual purposes; keep names,
  escape details and later locations provisional.
- [A finite cache undermines persistent destruction] → Document the existing
  retained-chunk implementation and require a bounded persistence policy later.
- [Low-resolution aesthetics harm XR] → Keep the material language but review
  eye resolution, dithering and motion separately on a headset.

## Migration Plan

Documentation and SVG only. Validate all OpenSpec changes, resolve local links,
parse and render the board, inspect the diff and rerun the existing core checks.
Commit the package directly to `main` as requested. No build or data migration
is needed; rollback would revert these documentation changes.

## Open Questions

Final story, names, production voxel scale and precise artistic intensity remain
intentionally undecided. They do not block this documentation pass. Future
implementation must resolve its own material-state and resource policies in a
separate change, not infer approval from this concept.
