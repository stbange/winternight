# Winternight — Game Concept

> Working concept. The priority is the image on screen and the editable voxel
> world beneath it. Story beats, names and camera preferences below are proposals
> unless already present in the prototype. This document does not approve the
> provisional voxel size or claim the complete game exists.

## The promise

A warrior crosses a deep-snow forest toward a feast and an old friend. The only
light comes from the moon, the stars and the castle's inviting windows. A
necromancer steals that warmth, taking the mage away on an enormous bone dragon.
The warrior breaks free and carves a path through winter to bring the mage home.

The game should feel like stepping into a small, severe winter painting that
remembers everything you do to it. Look back and see a trail through the snow,
the missing branch, the broken doorway and the dark stain being buried by fresh
flakes. The appeal is the contrast between a carefully composed scene and the
player's ability to change its materials.

Start with a short, authored, single-player action adventure. Interconnected
routes and compact encounters let the engine spend detail on nearby surfaces.
An enormous open world, loot systems and a survival simulation are unnecessary
to establish this experience.

## Four pillars

1. **Warmth is a destination.** Blue snow and dark pines frame amber windows.
   The opening teaches the player to follow light before anyone gives directions.
2. **Materials answer the player.** Snow compresses, pine splits, ice cracks,
   stone chips and bone breaks. Each response has its own shape and rhythm.
3. **The world keeps a record.** Tracks, cuts, debris and blood remain spatially
   consistent while the player revisits an area. Fresh snow can gradually obscure
   them. Long-term saved games are a later concern.
4. **A deliberate retro image.** Chunky geometry, restrained colour, crisp pixels
   and strong silhouettes carry the mood. More geometry is useful only when it
   improves a silhouette, an interaction or a readable trail.

## The opening experience

The forest walk is the first playable visual study. Aim for approximately five
to ten minutes as a design target, not a forced timer.

| Beat | What the player sees and does | What it proves |
|------|-------------------------------|----------------|
| Alone in the trees | A dark cloak, silver-blue shoulder edge, uneven snow and a narrow moonlit opening. Walk forward. | Readable darkness, scale and a grounded camera. |
| The first trail | Leave the packed road for knee-deep snow, then look back. | Physical snow depth, slower movement, persistent footprints. |
| Warmth appears | A bend reveals the castle on the upper third of the image. A few orange windows carry the composition. | Fog layers, distant silhouettes, emissive light. |
| A small obstruction | Chop a fallen pine. Its exposed wood is warmer than the bark; snow falls from it. | Material-specific damage, detached voxels and settling. |
| At the wall | A window casts a local amber patch onto blue snow. Shelter interrupts the snowfall. | Local lighting, exposure, scale and snow accumulation. |
| Arrival | Reach the gate and end the current slice. | A complete route with a clear destination. |

The first seconds can hide the castle. The **benchmark shot** comes at its first
clear reveal; it is not required to be the literal first frame. The route should
offer deliberate views without locking the camera or taking movement away.

Blood belongs in a separate material study at this stage. The forest walk does
not need an invented combat encounter merely to demonstrate the effect.

## Story scaffold, easy to replace

| Movement | Emotional and visual purpose | Engine opportunity |
|----------|------------------------------|--------------------|
| The invitation | Anticipation: a warrior looks forward to recounting adventures with the mage. | Cold exterior against warm interior. |
| The feast | Brief shelter and companionship. Keep any conversation compact. | Hearth light, timber, cloth and crowded silhouettes. |
| The seizure | The necromancer takes the castle; the bone dragon carries away the mage. | Roof rupture, falling snow, pale bone against fire. |
| Breaking free | The warrior frees himself from confinement or rubble. Exact circumstances remain open. | A small, solvable deformation puzzle. |
| The pursuit | Follow the necromancer to his fortress. | Revisit altered places, cross broken ice, follow marks in snow. |
| The rescue | Reach and free the mage. The confrontation and ending remain open. | A later large-creature and fortress showcase. |

Recollections at the table, the host's fate, a collapsed great hall, a frozen
lake and a final aurora are optional scene ideas. They should not determine the
engine architecture or require new systems before the forest is convincing.

Existing placeholder names are Halvar (warrior), Ysolde (mage), Morvaine
(necromancer), Hearthholm (feast castle), Gravenspire (enemy fortress), and
Ossuary Wyrm (dragon). Use stable role or scene identifiers in tests; keep names
and dialogue in content. None of these names is a final narrative decision.

## Interactions worth building around

The immediate loop is **notice a material → choose an action → change the world
→ read the result → continue**. A few actions can support many scenes:

| Material | First response | Lasting result | Later scene idea |
|----------|----------------|----------------|------------------|
| Powder snow | A compressed trough, a small puff and slower steps. | An uneven blue-shadowed trail. | Follow someone else's tracks. |
| Pine | A cut with a pale interior; branch snow drops. | A missing section and a settled log. | Open a blocked route. |
| Ice | A chipped, brighter edge before a local break. | A hole with a readable boundary. | Break a crossing under an enemy. |
| Masonry | Dust and small chips; more effort than wood. | A notch or a deliberate opening. | Break a damaged door surround. |
| Bone | Ivory fragments and powder. | A visibly missing piece. | Disable part of an undead creature. |
| Flesh / blood | Directional droplets and a local dark stain. | Seepage into snow and a fading trail. | Track an injured creature. |

These are target responses, not an assertion that every row is production-ready.
See [engine concept](voxel-engine.md) for the prototype inventory and limits.

The first combat direction is weighty melee with a sword or axe and a shield.
Use swept weapon volumes so the cut follows the action. Hardness controls how
much a hit achieves; a sword should not excavate intact castle walls as quickly
as it parts snow. Detailed combat balance, climbing and elemental spells follow
the visual and engine studies.

## View, body and controls

The existing flat-screen prototype uses an over-the-shoulder third-person view.
Keep it as the working default: the cloak, gait and trail communicate cold and
weight. Avoid automatic camera swings during destruction; collision and a clear
view matter more than dramatic framing. First-person flat-screen play remains
an option to evaluate, not a second required camera implementation now.

In WebXR use first person at metre scale, with no forced camera shake or head
bob. A controller swing and a button attack should express the same action.
Offer snap turning and a comfort vignette; validate their comfort on hardware.

| Target | Interaction direction | Visual consequence |
|--------|-----------------------|--------------------|
| Windows / Linux | Keyboard + mouse and gamepad. | Standard low-resolution image and third-person body. |
| Desktop web | Same actions through SDL3 / Emscripten. | WebGL2 uses the same visual rules. |
| Mobile web | Automatically show a left virtual stick, right-side camera drag and large action buttons on touch input. Allow hybrid devices to switch input modes. | Keep the destination and trail clear of buttons; use safe-area insets and support landscape layouts. |
| WebXR | Head tracking, controller locomotion, turning and melee. Enter on a user gesture; return to flat play on exit. | Stereo views, a more legible eye resolution and stable effects replace flat-screen pixel tricks. |

The user should recognize the same forest on every target. Reduce decorative
particles and distant detail before making nearby snow or cuts unreadable.

## The next showcase, in stages

1. **Approve the winter image.** Compare the existing forest against the
   [visual guide](visual-style.md) and [composition board](boards/winter-night.svg).
   Review the 10 cm and 5 cm looks in the same framing before choosing a scale.
2. **Prove a material courtyard.** One pine, snow bank, stone arch, ice patch,
   warm window and blood-on-snow study. Revisit them after walking away.
3. **Prove one warm room becoming exposed.** Remove part of a roof and show moon,
   fire, snowfall and debris interacting in a bounded space.
4. **Stage a dragon silhouette and one roof impact.** Validate scale and cost
   before committing to a fully simulated castle collapse or a campaign.

Each stage needs visual review, automated checks and measurements. These are
future milestones, not checked-off implementation tasks in this concept pass.

## Decisions kept open

- The final story, character identities, amount of dialogue and exact escape.
- Voxel size: 10 cm is the prototype default; 5 cm costs substantially more at
  equal world coverage. Character detail may need a separate scale later.
- How much structural collapse is entertaining and affordable on phones / XR.
- Whether a first-person flat-screen mode improves the material interactions.
- How prominent gore should be by default. Full, Reduced and Off remain useful
  options; the winter mood should work at all three levels.

Audio production, multiplayer, a survival economy, a general-purpose editor,
console support, macOS and native OpenXR are outside this pass. Persistence
across chunk streaming matters now; persistence across app restarts needs a
separate saved-world design.
