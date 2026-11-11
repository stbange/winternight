# Winternight — Visual Style Guide

## Reference feel

Late-90s console 3D seen through a voxel lens: chunky geometry, crisp pixels,
strong silhouettes, deep atmospheric fog and few but meaningful light sources.
The night must read clearly. Darkness is shaped by moonlight, not by hiding things.

## Rendering look

| Element | Rule |
|---------|------|
| Internal resolution | Render to a low-resolution target (default **480×270**, options 320×180 and 640×360) and upscale with **nearest-neighbour** filtering in integer multiples, letterboxing where needed. |
| Voxel size | 1 voxel = **10 cm** (provisional, to be reviewed after the first playable version). The warrior is about 18 voxels tall. Voxels are visible but not huge. |
| Colour | Each material has a small ramp of palette colours, varied per voxel by hash noise so surfaces don't look flat. |
| Palette | Final image is quantised to a **64-colour master palette** using a 4×4 Bayer ordered dither. Can be switched off. |
| Shading | Lambert + baked per-vertex voxel ambient occlusion. No specular, except a faint glint on ice and snow. |
| Fog | Exponential distance fog tinted toward deep night blue. It hides the view distance and sets the mood. |
| Retro jitter | Optional PS1-style vertex snapping to the low-res grid. **Always off in XR.** |
| Outlines | None. Silhouettes come from fog and rim light. |

## Lighting

- **Moon:** one directional light, cold blue-white (`#9fb4e0`), casting crisp
  low-resolution shadows. Moon phase and position are set per scene.
- **Stars and sky:** a procedural starfield with a few twinkling stars, a faint Milky Way band
  and an optional aurora (Act IV).
- **Fire:** warm point light sources (`#ffb050` → `#ff6a1a`) for hearths, torches and
  windows. Their light spreads through the voxel grid (flood-fill), so lit windows
  cast warm patches onto the snow outside.
- **Window glow:** emissive voxels with bloom-like halos, done cheaply at low resolution.
- **Necromantic light:** a sickly green-cyan (`#7dffc0`) that replaces fire when Morvaine
  appears.

## Key palette (excerpt)

| Use | Colours |
|-----|---------|
| Night sky | `#05070f` `#0b1026` `#16204a` |
| Moonlit snow | `#c9d6f2` `#9fb0d8` `#6b7cab` `#3d4a75` (shadowed) |
| Pine | `#0e1f1a` `#173328` `#24493a` |
| Stone (castle) | `#2a2a35` `#454555` `#6a6a7a` |
| Firelight | `#ffe2a0` `#ffb050` `#ff6a1a` `#a83210` |
| Blood | `#5a0a0c` (dried) `#8c1016` `#c21e22` (fresh) |
| Bone | `#d8cfb4` `#a99c7a` `#6e6249` |
| Necromancy | `#2affb0` `#7dffc0` `#1a5a50` |

## Snow

- Snow is a **real voxel material with fill levels** (16 heights per voxel), so drifts
  are smooth and footprints are carved as real dents.
- Falling snow is drawn as pixel-sized particles. Over time it builds up again and fills in
  old tracks and wounds.
- Disturbed snow (trampled or bloodied) uses a darker, bluer ramp.
- Snow clumps fall from branches when trees are hit or brushed.

## Blood

- Blood is drawn as droplet particles. When a droplet hits a surface it becomes a **stain**
  on that voxel. Stains spread a little into snow and fade from fresh red to dark, dried red.
- Heavy wounds leave **pools** (liquid voxels that flow a little before freezing in the cold).
- Undead "bleed" bone dust and dark ichor instead of red blood.
- Every amount can be scaled down by the gore setting.

## Destruction

- Every voxel has hit points that depend on its material (snow < wood < ice < stone < iron).
- Pieces that lose their connection to the ground break off as **debris objects** with simple
  rigid-body physics. They crumble into smaller pieces when they land.
- Dust, snow powder and splinters are short-lived particles, not voxels, to keep the cost down.

## The benchmark shot

> Over-the-shoulder view on the forest road. Knee-deep, moonlit snow with the warrior's
> trail behind him. Black pine silhouettes. Light snowfall. Far away on a hill, the dark
> mass of Castle Hearthholm with a row of glowing orange windows whose light falls onto the
> snow below. Stars, and a moon partly behind thin cloud.

This is also the game's opening shot. The engine is "done enough" for production
when it can render this shot smoothly **on an average phone**, at the budgets in
`openspec/changes/establish-winternight-concept/design.md` (D14).
