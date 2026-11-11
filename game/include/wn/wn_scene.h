/* Winternight game - data-driven scenes: terrain, road, forest, prefabs,
 * lighting, weather, spawn, triggers and events (spec game-flow). */
#ifndef WN_SCENE_H
#define WN_SCENE_H

#include "wn/wn_render.h"
#include "wn/wn_voxel.h"

/* ---- Prefabs: lists of primitives in metres, y = 0 at the ground ---- */

typedef enum wn_prim_kind {
    WN_PRIM_BOX,
    WN_PRIM_CYL,    /* vertical cylinder */
    WN_PRIM_HCYL_X, /* horizontal cylinder along x */
    WN_PRIM_HCYL_Z,
    WN_PRIM_CONE,   /* vertical cone, radius r at y0 -> 0 at y1 */
    WN_PRIM_SPHERE,
    WN_PRIM_GABLE_X, /* roof ridge along x */
    WN_PRIM_GABLE_Z,
    WN_PRIM_CRENEL   /* box solid only on alternating merlons */
} wn_prim_kind;

typedef struct wn_prim {
    wn_prim_kind kind;
    bool carve;
    uint8_t mat;
    float a[8];
    wn_vec3 mn, mx; /* bounds */
} wn_prim;

#define WN_MAX_PREFABS 16
#define WN_MAX_PRIMS 160

typedef struct wn_prefab {
    char name[32];
    wn_prim prims[WN_MAX_PRIMS];
    int nprims;
    wn_vec3 mn, mx;
    bool snow_cap; /* put snow on upward-facing surfaces */
} wn_prefab;

typedef struct wn_prefab_lib {
    wn_prefab p[WN_MAX_PREFABS];
    int count;
} wn_prefab_lib;

bool wn_prefabs_load(wn_prefab_lib *lib, const wn_materials *mats, const char *text, char *err, size_t errlen);
const wn_prefab *wn_prefab_find(const wn_prefab_lib *lib, const char *name);
/* Material of the prefab at a local point: >0 solid material, 0 = carved air,
 * -1 = not part of the prefab. */
int wn_prefab_sample(const wn_prefab *p, wn_vec3 local);

/* ---- Scene ---- */

#define WN_MAX_PLACEMENTS 16
#define WN_MAX_HILLS 8
#define WN_MAX_TRIGGERS 16
#define WN_MAX_EVENTS 24
#define WN_MAX_TRACKS 8

typedef struct wn_placement {
    const wn_prefab *prefab;
    wn_vec3 pos; /* world position of the prefab origin */
    int rot;     /* quarter turns around y */
    wn_vec3 mn, mx; /* world bounds */
} wn_placement;

typedef enum wn_action_kind {
    WN_EVT_COMPLETE,
    WN_EVT_FIRE_COLOR,
    WN_EVT_SNOWFALL,
    WN_EVT_MESSAGE
} wn_action_kind;

typedef struct wn_event_action {
    char event[24];
    wn_action_kind kind;
    uint32_t color;
    float value;
    char text[64];
} wn_event_action;

typedef struct wn_trigger {
    char name[24];
    wn_vec3 mn, mx;
    char event[24];
    bool fired;
} wn_trigger;

typedef struct wn_scene {
    char name[32];
    uint64_t seed;
    wn_vec3 bounds_mn, bounds_mx; /* playable area (m) */
    int chunk_y_min, chunk_y_max;
    float terrain_base, terrain_amp, terrain_scale;
    float snow_depth, snow_var;
    struct {
        float x, z, radius, height;
    } hills[WN_MAX_HILLS];
    int nhills;
    float road_width, road_amp, road_freq, road_z0, road_z1;
    float forest_cell, forest_prob, forest_clear;
    struct {
        float x0, z0, x1, z1;
    } tracks[WN_MAX_TRACKS];
    int ntracks;
    wn_placement place[WN_MAX_PLACEMENTS];
    int nplace;
    wn_atmos atmos;
    float snow_rate;      /* accumulation, fill levels per second */
    float snowfall_flakes; /* falling particles per second */
    wn_vec3 spawn;
    float spawn_yaw;
    wn_trigger triggers[WN_MAX_TRIGGERS];
    int ntriggers;
    wn_event_action actions[WN_MAX_EVENTS];
    int nactions;
    const wn_materials *mats;
} wn_scene;

/* Loads a scene description. Prefab names are resolved against lib; a missing
 * prefab is an error that names it (spec: Broken scene file). */
bool wn_scene_load(wn_scene *s, const wn_materials *mats, const wn_prefab_lib *lib, const char *text, char *err,
                   size_t errlen);

/* World generation (deterministic for a scene + seed). */
float wn_scene_road_x(const wn_scene *s, float z);
float wn_scene_terrain_height(const wn_scene *s, float x, float z);
float wn_scene_snow_height(const wn_scene *s, float x, float z);
/* Material voxel at world voxel coordinates (light bits zero). */
wn_voxel wn_scene_voxel(const wn_scene *s, int32_t x, int32_t y, int32_t z);
/* wn_chunk_gen_fn for wn_world. */
void wn_scene_generate_chunk(void *scene, wn_chunk *c);

/* Tree placement query (for tests and gameplay). */
typedef struct wn_tree {
    float x, z, base, height, trunk_r, crown_r;
    int tiers;
    uint32_t hash;
} wn_tree;
bool wn_scene_tree_in_cell(const wn_scene *s, int cx, int cz, wn_tree *out);

/* Far scenery: a coarse grid of the whole scene, one cell per 2^shift voxels
 * (design D14 / far LOD). Includes a one-cell padding for meshing. */
typedef struct wn_far_grid {
    wn_voxel *cells;
    int sx, sy, sz; /* padded */
    int shift;
    wn_ivec3 origin; /* world voxel coordinate of interior cell (0,0,0) */
} wn_far_grid;

void wn_scene_build_far(const wn_scene *s, int shift, wn_far_grid *out);
void wn_far_grid_free(wn_far_grid *g);

#endif
