/* Winternight engine - greedy voxel meshing with baked ambient occlusion
 * (spec retro-rendering: Voxel meshing, design D3). The same mesher is used
 * for world chunks, far-scenery grids, character parts and debris. */
#ifndef WN_MESH_H
#define WN_MESH_H

#include "wn_voxel.h"

/* 12-byte vertex. Positions are in 1/16 voxel units relative to the grid's
 * interior origin. */
typedef struct wn_vertex {
    uint16_t x, y, z;
    uint16_t color; /* RGB565 */
    uint8_t nao;    /* bits 0-2 normal index, bits 3-4 ambient occlusion (0 dark - 3 open) */
    uint8_t light;  /* sky << 4 | block */
    uint8_t flags;  /* bit 0 emissive */
    uint8_t pad;
} wn_vertex;

enum { WN_VFLAG_EMISSIVE = 1 };
/* Normal indices: 0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z */

typedef struct wn_mesh {
    wn_vertex *verts;
    uint32_t *indices;
    uint32_t nverts, nindices;
    uint32_t vcap, icap;
} wn_mesh;

typedef struct wn_mesh_opts {
    wn_ivec3 hash_origin; /* world voxel coordinate of the grid interior (0,0,0) */
    bool use_light;       /* take light from the cells; otherwise full_light */
    uint8_t full_light;
    uint8_t scale_shift;  /* far-scenery grids: each cell is 2^shift voxels */
} wn_mesh_opts;

void wn_mesh_init(wn_mesh *m);
void wn_mesh_free(wn_mesh *m);
void wn_mesh_clear(wn_mesh *m);

/* Meshes a padded grid of size (sx, sy, sz), which includes a one-cell border
 * on every side that is read for culling/AO but not meshed. */
void wn_mesh_grid(wn_mesh *out, const wn_voxel *grid, int sx, int sy, int sz, const wn_materials *mats,
                  const wn_mesh_opts *opts);

/* Copies a chunk plus a one-voxel border from the world into a 34^3 grid. */
void wn_mesh_gather_chunk(wn_world *w, wn_ivec3 cpos, wn_voxel *grid34);
/* Convenience: gather + mesh one world chunk. */
void wn_mesh_chunk(wn_mesh *out, wn_world *w, wn_ivec3 cpos);

/* Colour of a voxel (0xRRGGBB) including per-voxel variation, damage and
 * stain (spec retro-rendering: Per-voxel colour variation). */
uint32_t wn_voxel_color(const wn_materials *mats, wn_voxel v, int32_t x, int32_t y, int32_t z);
uint16_t wn_rgb_to_565(uint32_t rgb);
uint32_t wn_565_to_rgb(uint16_t c);

#endif
