/* Winternight engine - remesh planning (spec voxel-deformation: Edit budget,
 * design D3). Picks which dirty chunks to remesh first. */
#ifndef WN_REMESH_H
#define WN_REMESH_H

#include "wn_voxel.h"

typedef struct wn_remesh_item {
    wn_chunk *chunk;
    int32_t dist2; /* squared chunk distance to the viewer */
} wn_remesh_item;

/* Collects dirty, loaded chunks within the horizontal radius (chunks) of the
 * viewer, nearest first (ties broken by coordinates for determinism). Writes
 * up to max items and returns the total number of dirty chunks found. */
uint32_t wn_remesh_plan(wn_world *w, wn_vec3 viewer, int radius, wn_remesh_item *out, uint32_t max);

/* Time budget helper: given per-chunk meshing cost estimates, how many of the
 * planned chunks fit into budget_ms (at least one, so progress is guaranteed). */
uint32_t wn_remesh_fit_budget(const float *cost_ms, uint32_t n, float budget_ms);

#endif
