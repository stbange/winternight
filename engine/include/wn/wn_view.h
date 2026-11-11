/* Winternight engine - render-side view of the world: per-chunk mesh cache
 * with distance level of detail, per-column mesh assembly (one draw call per
 * column), remesh budgeting and frustum culling (design D3, D14). Pure C:
 * the GL backend only uploads columns flagged gpu_dirty. */
#ifndef WN_VIEW_H
#define WN_VIEW_H

#include "wn_mesh.h"
#include "wn_voxel.h"

#define WN_VIEW_MAX_Y 24 /* chunks per column */

typedef struct wn_view_chunk {
    uint32_t rev; /* chunk->mesh_rev the mesh was built from */
    int lod;      /* -1 = never meshed */
    wn_mesh mesh;
} wn_view_chunk;

typedef struct wn_view_column {
    bool used;
    int cx, cz;
    wn_view_chunk chunks[WN_VIEW_MAX_Y];
    wn_mesh mesh;  /* assembled column mesh (all chunks share one cell size per column) */
    int lod;       /* current detail level of the column */
    bool dirty;    /* needs reassembly */
    bool gpu_dirty;
    uint32_t gpu;  /* backend handle (0 = none) */
} wn_view_column;

typedef struct wn_view {
    wn_view_column *cols;
    uint32_t cap;
    uint32_t ncols;
    int lod_near; /* chunk rings with full detail */
    int lod_mid;  /* rings with half detail; beyond that quarter detail */
    /* Handles of removed columns the backend must free. */
    uint32_t *freed;
    uint32_t nfreed, capfreed;
    /* statistics of the last update */
    uint32_t meshed_last;
    uint32_t pending_last;
} wn_view;

void wn_view_init(wn_view *v);
void wn_view_free(wn_view *v);
/* Detail level wanted for a column at chunk distance d (Chebyshev). */
int wn_view_lod_for_distance(const wn_view *v, int d);

/* Callback asked before meshing each chunk: return false to stop (time budget). */
typedef bool (*wn_view_budget_fn)(void *user);

/* Updates the view around the viewer: drops far columns, (re)meshes changed
 * chunks nearest first while budget allows, and reassembles columns. */
void wn_view_update(wn_view *v, wn_world *w, wn_vec3 viewer, int radius, wn_view_budget_fn budget, void *user);
wn_view_column *wn_view_find(wn_view *v, int cx, int cz);
/* Drains the list of freed backend handles. */
uint32_t wn_view_take_freed(wn_view *v, uint32_t *out, uint32_t max);

/* ---- Frustum culling ---- */
typedef struct wn_frustum {
    float planes[6][4]; /* ax + by + cz + d >= 0 inside */
} wn_frustum;

wn_frustum wn_frustum_from_matrix(wn_mat4 viewproj);
bool wn_frustum_box(const wn_frustum *f, wn_vec3 mn, wn_vec3 mx);
/* World-space bounds of a column. */
void wn_view_column_bounds(const wn_world *w, int cx, int cz, wn_vec3 *mn, wn_vec3 *mx);

#endif
