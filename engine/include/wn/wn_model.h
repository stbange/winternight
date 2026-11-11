/* Winternight engine - voxel character models on animated bones
 * (spec voxel-deformation: Dynamic voxel models). */
#ifndef WN_MODEL_H
#define WN_MODEL_H

#include "wn_edit.h"

#define WN_MODEL_MAX_PARTS 12

typedef struct wn_model_part {
    char name[16];
    wn_voxel *grid; /* padded grid (sx,sy,sz) */
    int sx, sy, sz;
    int parent;     /* -1 for the root */
    wn_vec3 pivot;  /* joint position in the part's grid (voxel units, interior origin) */
    wn_vec3 attach; /* where the joint sits in the parent's grid (voxel units) */
    float rx, rz;   /* current joint rotation around X (swing) and Z (spread) */
    bool severed;
    uint32_t nvoxels;
    uint32_t mesh_rev;
} wn_model_part;

typedef struct wn_model {
    wn_model_part parts[WN_MODEL_MAX_PARTS];
    int nparts;
    wn_vec3 pos; /* world position of the root pivot */
    float yaw;
    bool bleeds; /* living creatures bleed, undead emit dust */
} wn_model;

void wn_model_init(wn_model *m);
void wn_model_free(wn_model *m);
/* Adds a part with an empty grid of interior size (w,h,d). Returns its index. */
int wn_model_add_part(wn_model *m, const char *name, int parent, int w, int h, int d, wn_vec3 pivot, wn_vec3 attach);
void wn_model_part_set(wn_model *m, int part, int x, int y, int z, wn_voxel v);
void wn_model_part_box(wn_model *m, int part, int x0, int y0, int z0, int x1, int y1, int z1, wn_voxel v);
wn_voxel wn_model_part_get(const wn_model *m, int part, int x, int y, int z);

/* World transform of a part: grid interior voxel coords -> world metres. */
wn_mat4 wn_model_part_matrix(const wn_model *m, int part);
int wn_model_find_part(const wn_model *m, const char *name);

typedef struct wn_model_hit {
    int part;
    int voxels_removed;
    wn_vec3 point;  /* world position of the first removed voxel */
    bool severed;
} wn_model_hit;

/* Damages the model with a world-space capsule (blade). Voxels whose centre is
 * within radius of the segment are removed. A part whose joint region is cut
 * through is severed and turned into debris. Returns what happened. */
wn_model_hit wn_model_damage(wn_model *m, wn_vec3 a, wn_vec3 b, float radius, wn_debris_set *debris,
                             const wn_materials *mats);

#endif
