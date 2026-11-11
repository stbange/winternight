/* Winternight engine - deformation: damage, shaped edits, detached pieces and
 * debris rigid bodies (spec voxel-deformation, design D6). */
#ifndef WN_EDIT_H
#define WN_EDIT_H

#include "wn_mesh.h"
#include "wn_voxel.h"

/* Optional hook: called for every voxel destroyed by an edit (particles, sound). */
typedef void (*wn_destroy_fn)(void *user, wn_ivec3 pos, wn_voxel old);

typedef struct wn_edit_ctx {
    wn_world *world;
    wn_destroy_fn on_destroy;
    void *user;
    /* Voxels destroyed by the current edit; used for connectivity checks. */
    wn_ivec3 *touched;
    uint32_t ntouched, cap_touched;
    uint32_t destroyed, damaged;
} wn_edit_ctx;

void wn_edit_begin(wn_edit_ctx *e, wn_world *w);
void wn_edit_end(wn_edit_ctx *e);

/* Applies damage to one voxel. Damage accumulates as (damage / hardness) in
 * sixteenths; the voxel becomes air when the total reaches 16. Returns true if
 * the voxel was destroyed. Non-solid voxels are ignored. */
bool wn_edit_damage(wn_edit_ctx *e, int32_t x, int32_t y, int32_t z, float damage);

/* Spherical impact with linear falloff from full damage at the centre to 0 at
 * the radius (spec: Explosion falloff). */
void wn_edit_sphere(wn_edit_ctx *e, wn_vec3 center, float radius, float damage);
/* Damages every voxel whose centre lies in the box. */
void wn_edit_box(wn_edit_ctx *e, wn_vec3 mn, wn_vec3 mx, float damage);
/* Damages every voxel within radius of the segment a-b (blade swing). */
void wn_edit_capsule(wn_edit_ctx *e, wn_vec3 a, wn_vec3 b, float radius, float damage);
/* Adds material: fills air voxels in a sphere. */
void wn_edit_fill_sphere(wn_edit_ctx *e, wn_vec3 center, float radius, wn_voxel v);
/* Sets one voxel with light update (no damage logic). */
void wn_edit_set(wn_edit_ctx *e, int32_t x, int32_t y, int32_t z, wn_voxel v);

/* ---- Debris ---- */

#define WN_MAX_DEBRIS 48

typedef struct wn_debris {
    bool active;
    uint32_t id;
    wn_voxel *grid; /* padded grid (sx, sy, sz) incl. 1-cell border */
    int sx, sy, sz;
    uint32_t nvoxels;
    wn_vec3 com_local; /* centre of mass in grid voxel units (interior origin) */
    wn_vec3 pos;       /* world position of the centre of mass */
    wn_quat rot;
    wn_vec3 vel, angvel;
    float mass, inv_mass, inv_inertia;
    float rest_time;
    float age;
    uint32_t mesh_rev;
    /* Surface sample points (local, voxel units relative to COM). */
    wn_vec3 *samples;
    uint32_t nsamples;
} wn_debris;

typedef struct wn_debris_set {
    wn_debris d[WN_MAX_DEBRIS];
    uint32_t next_id;
    uint32_t min_voxels;     /* smaller pieces vanish when they settle */
    uint32_t max_island;     /* connectivity search limit (design D6) */
    float break_speed;       /* impact speed that splits a piece */
    wn_vec3 detach_push;     /* velocity (m/s) given to the top of new pieces */
    wn_destroy_fn on_vanish; /* small debris turned into particles */
    void *user;
} wn_debris_set;

void wn_debris_init(wn_debris_set *s);
void wn_debris_free(wn_debris_set *s);
uint32_t wn_debris_count(const wn_debris_set *s);

/* After an edit: finds voxel groups next to the destroyed voxels that are no
 * longer connected to an anchor and turns them into debris. Returns the number
 * of debris objects created. */
int wn_debris_detach(wn_debris_set *s, wn_edit_ctx *e);
/* Creates debris directly from a padded voxel grid placed at a world voxel
 * origin (used for severed body parts). Takes a copy of the grid. */
wn_debris *wn_debris_spawn(wn_debris_set *s, const wn_voxel *grid, int sx, int sy, int sz, wn_vec3 world_origin,
                           const wn_materials *mats);
/* Advances all debris by one fixed step: gravity, collision, sleeping, breaking
 * and settling (write-back or removal). */
void wn_debris_step(wn_debris_set *s, wn_world *w, float dt);
/* Model matrix for rendering: maps grid interior voxel coords to world metres. */
wn_mat4 wn_debris_matrix(const wn_debris *d);
uint32_t wn_debris_checksum(const wn_debris_set *s);

#endif
