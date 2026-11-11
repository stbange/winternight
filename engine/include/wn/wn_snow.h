/* Winternight engine - snow as fill-level voxels (spec snow-simulation,
 * design D8). */
#ifndef WN_SNOW_H
#define WN_SNOW_H

#include "wn_edit.h"
#include "wn_voxel.h"

typedef struct wn_snow {
    float rate;          /* fill levels (1/16 voxel) per second on exposed ground */
    float timer;         /* time until the next accumulation pass */
    bool pass_active;
    int pass_cx, pass_cz; /* progress of the current pass (column cursor) */
    int pass_x0, pass_x1, pass_z0, pass_z1;
    int columns_per_step; /* voxel columns processed per simulation step */
    float min_speed;     /* slowest movement factor in deep snow */
    float knee_height;   /* snow depth (m) giving the strongest slowdown */
    int packed16;        /* snow height left in a footprint (1/16 voxel units) */
    uint32_t passes_done;
} wn_snow;

void wn_snow_init(wn_snow *s);

/* Height of the top surface of the topmost solid voxel in column (x,z),
 * scanning down from y_top. In 1/16 voxel units; INT32_MIN if none. */
int32_t wn_snow_column_top16(wn_world *w, int32_t x, int32_t z, int32_t y_top);

/* Adds amount16 sixteenths of snow on top of the column at (x,z). Snow only
 * lands where the top has an open view of the sky. Returns sixteenths added. */
int wn_snow_add(wn_edit_ctx *e, int32_t x, int32_t z, int amount16);
/* Removes up to amount16 sixteenths of snow from the top of the column.
 * Returns sixteenths removed. */
int wn_snow_remove(wn_edit_ctx *e, int32_t x, int32_t z, int amount16);

/* Advances snowfall: accumulation passes sweep the loaded chunks of the world
 * in a time-sliced, deterministic order. */
void wn_snow_step(wn_snow *s, wn_world *w, float dt);
/* Runs any pending accumulation work to completion (tests, scene start). */
void wn_snow_flush(wn_snow *s, wn_world *w);

/* Compresses snow in a disc around a foot position (footprint/trail). */
void wn_snow_footprint(wn_snow *s, wn_edit_ctx *e, wn_vec3 foot, float radius);
/* Depth of snow (m) at a position: snow between the first non-snow solid below
 * and the snow surface. */
float wn_snow_depth_at(wn_world *w, wn_vec3 pos);
/* Movement speed factor in [min_speed, 1] for a character at pos. */
float wn_snow_speed_factor(const wn_snow *s, wn_world *w, wn_vec3 pos);

/* Knocks snow off pine branches within radius. Calls on_release for every
 * released snow voxel (spawn falling snow). Returns the count. */
typedef void (*wn_snow_release_fn)(void *user, wn_vec3 pos);
int wn_snow_shake(wn_edit_ctx *e, wn_vec3 center, float radius, wn_snow_release_fn on_release, void *user);

/* Melts snow and ice within radius of a heat source. power = fill levels per
 * second at the centre. Snow vanishes, ice turns into water. */
void wn_snow_melt(wn_edit_ctx *e, wn_vec3 center, float radius, float power, float dt, float *accum);

/* Lets snow slide until no neighbouring column differs by more than
 * slope16 sixteenths. Works inside [x0,x1]x[z0,z1] below y_top. Returns the
 * number of transfers; stops after max_moves. */
int wn_snow_settle(wn_edit_ctx *e, int32_t x0, int32_t z0, int32_t x1, int32_t z1, int32_t y_top, int slope16,
                   int max_moves);

#endif
