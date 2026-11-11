# Winternight — Game Concept

> Status: **working draft**. The story is a placeholder that will be revised.
> The current focus is the *look* of the game and the voxel engine behind it.
> Names below are working titles.

## High concept

A lone warrior walks through a snow-buried forest on a moonlit winter night,
invited to a feast in a castle whose windows glow with firelight. He looks
forward to trading stories with an old friend, a mage. The feast turns into a
massacre: a necromancer seizes the castle and carries the mage away on a vast
dragon of bone. The warrior must break free and pursue him across the frozen
wilds to his own fortress.

**Pillars**

1. **Cold outside, warm inside.** Every scene plays blue moonlight and starlight
   against orange firelight. Contrast is the core of the art direction.
2. **Everything is made of voxels, and voxels can break.** Trees, walls, snow,
   ice, bone and flesh can all be cut, crushed, melted and stained.
3. **Snow and blood leave marks.** Footprints, drifts, trails, sprays and pools
   stay in the world and tell the story of what happened there.
4. **Retro, not cheap.** Low resolution, a limited palette and dithering are
   deliberate choices, held together by careful lighting and composition.

## Characters (working names)

| Role | Working name | Notes |
|------|--------------|-------|
| The warrior (player) | **Halvar** | Veteran sword-and-axe fighter in a fur cloak, heavy and grounded. Third person on flat screens, first person in XR. |
| The mage | **Ysolde** | The warrior's old companion. A storyteller; their shared adventures are the emotional anchor. |
| The necromancer | **Morvaine** | Arrives uninvited. Pale and calm, wrapped in grave-cloth and frost. |
| The bone dragon | **The Ossuary Wyrm** | A colossal dragon built from the bones of many creatures. Its size shows off voxel destruction. |
| The host | **Lord Edric of Hearthholm** | Holds the feast. Fate open. |

## Story outline (draft)

### Prologue — *The Road to Hearthholm*
The game opens with the warrior already on his way, approaching the castle
through the dark, snowy forest. There is no menu-driven intro: the first thing
the player sees is the forest at night. This scene is also the vertical slice and
the visual benchmark for the engine.
- Opening shot: the warrior alone between black pines, the castle not yet visible.
- A narrow road through a dark pine forest. Deep snow, moon and stars, light snowfall.
- The player wades through knee-deep snow and leaves trails. Branches shed snow when brushed.
- Wolves howl in the distance, and their tracks cross the road.
- Through the trees the castle appears on a hill, its windows glowing warm and orange.
- Gameplay: movement, snow interaction, first contact with destruction (chop a fallen tree blocking the road).

### Act I — *The Feast*
- The great hall: long tables, hearth fire, torches, music. A warm palette.
- The warrior meets the mage, and short playable "recollection" vignettes can be told as stories at the table (an option for later).
- The candles gutter, the fire turns pale green and frost creeps over the windows. Morvaine enters.
- The dead in the crypt below rise. The guests are slaughtered (blood on snow-white tablecloths).
- The Ossuary Wyrm breaks through the roof, an engine showcase of large-scale voxel destruction, and carries the mage away.
- The warrior is struck down.

### Act II — *Bound*
- The warrior wakes chained in the castle dungeon (or pinned under rubble; to be decided).
- He frees himself by breaking the chain anchor out of a crumbling wall, which teaches deformation-based puzzles.
- He fights his way out through the ruined, burning and freezing castle.

### Act III — *The Frozen Pursuit*
- The Wyrm's trail: bone fragments, scorched black ice, dead trees.
- A frozen lake where ice cracks and breaks under weight. A mountain pass with avalanches made of snow voxels.
- Undead pursuers. Skeletons shatter into bones, and ghouls bleed into the snow.

### Act IV — *Gravenspire*
- The necromancer's fortress of black stone, bone and blue ice, under an aurora.
- A climb, then a fight against Morvaine, and the Wyrm torn apart voxel by voxel.
- The mage is rescued. The ending is to be defined.

## Core gameplay (draft)

- **Movement:** walk, run and wade (speed depends on snow depth), climb, slide.
- **Combat:** weighty melee with sword, axe and shield. Hits remove or deform voxels
  on enemies and the world, and cutting angles matter.
- **Destruction as a tool:** chop through doors, collapse pillars, break ice under enemies.
- **Elements:** fire melts snow and ice and thaws frozen undead. Cold freezes blood
  and water.
- **Environmental storytelling:** tracks in the snow and blood trails guide the player.

## Platforms and controls

| Target | Input | Notes |
|--------|-------|-------|
| Windows, Linux | Keyboard + mouse, gamepad | Native builds. |
| Web (desktop) | Keyboard + mouse, gamepad | Emscripten / WebGL2. |
| Web (mobile) | **Touch** (virtual stick, camera drag, action buttons) | Switches automatically on touch-capable devices. **Performance reference:** the game must run smoothly on an average phone (see design). |
| WebXR | XR controllers (motion swing for melee), hand-tracking later | Immersive VR session from the web build, first-person view. |

## Voxel size

Voxel size is **provisional** (10 cm to start). It will be reviewed together with
the first playable version and is kept as one engine constant so it can change.

## Content notes

Blood and gore are part of the tone. They can be dialled down (stains only, or
off) in the options so the game works for more audiences and stores.

## Out of scope (for now)

Audio design, final narrative, multiplayer, save system, macOS / consoles,
desktop-native VR (OpenXR).
