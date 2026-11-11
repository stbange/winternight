/* Winternight engine - particles and blood (spec blood-effects, snow particles
 * from spec snow-simulation, design D8). */
#ifndef WN_FX_H
#define WN_FX_H

#include "wn_core.h"
#include "wn_edit.h"
#include "wn_snow.h"

typedef enum wn_particle_type {
    WN_PART_SNOWFLAKE,  /* cosmetic falling snow */
    WN_PART_SNOW_CLUMP, /* falling branch snow: lands as fill */
    WN_PART_BLOOD,      /* stains what it hits */
    WN_PART_BONE_DUST,
    WN_PART_DUST,
    WN_PART_SPLINTER,
    WN_PART_TYPE_COUNT
} wn_particle_type;

typedef struct wn_particle {
    wn_vec3 pos, vel;
    float life;
    uint32_t color; /* 0xRRGGBB */
    uint8_t type;
    uint8_t size;   /* pixels at internal resolution */
} wn_particle;

typedef enum wn_gore_level { WN_GORE_OFF = 0, WN_GORE_REDUCED = 1, WN_GORE_FULL = 2 } wn_gore_level;

#define WN_MAX_PARTICLES 4096
#define WN_MAX_STAINS 2048
#define WN_MAX_POOL_CELLS 512

typedef struct wn_stain_rec {
    wn_ivec3 pos;
    float age;
} wn_stain_rec;

typedef struct wn_pool_cell {
    wn_ivec3 pos;
    float age;
} wn_pool_cell;

typedef struct wn_fx {
    wn_particle p[WN_MAX_PARTICLES];
    uint32_t count;
    uint32_t capacity;   /* quality preset limit (<= WN_MAX_PARTICLES) */
    wn_rng rng;          /* simulation stream: blood/clumps affect the world */
    wn_rng cosmetic_rng; /* snowflakes only */
    wn_gore_level gore;
    wn_vec3 wind;
    float snowfall;      /* flakes per second around the viewer */
    float flake_accum;
    float stain_dry_time;
    float pool_freeze_time;
    wn_stain_rec stains[WN_MAX_STAINS];
    uint32_t stain_head, nstains;
    wn_pool_cell pool[WN_MAX_POOL_CELLS];
    uint32_t npool;
    uint32_t stains_written;
} wn_fx;

void wn_fx_init(wn_fx *fx, uint64_t seed);
wn_particle *wn_fx_spawn(wn_fx *fx, wn_particle_type type, wn_vec3 pos, wn_vec3 vel, uint32_t color, float life);

/* Emits from a wound (spec: Wounds emit blood droplets). bleeds=false emits
 * bone dust instead (undead). Respects the gore level. Returns particle count. */
int wn_fx_wound(wn_fx *fx, wn_vec3 pos, wn_vec3 dir, float damage, bool bleeds);
/* Starts a blood pool under a heavily bleeding creature (Full gore only). */
int wn_fx_blood_pool(wn_fx *fx, wn_edit_ctx *e, wn_vec3 pos, int volume16);
/* Adds a stain to a voxel; on snow also to neighbouring snow at half strength. */
void wn_fx_stain(wn_fx *fx, wn_world *w, int32_t x, int32_t y, int32_t z, int amount);
/* Falling snow from branches. */
void wn_fx_snow_clump(void *fx_user, wn_vec3 pos);
/* Breaking debris dust. */
void wn_fx_dust(wn_fx *fx, wn_vec3 pos, uint32_t color, int count);

/* Advances particles, stain ageing and pool flow/freezing by one step. The
 * viewer position anchors the cosmetic snowfall. */
void wn_fx_step(wn_fx *fx, wn_world *w, wn_vec3 viewer, float dt);
uint32_t wn_fx_checksum(const wn_fx *fx);

#endif
