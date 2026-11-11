#include "wn/wn_remesh.h"

#include <stdlib.h>

typedef struct plan_ctx {
    wn_remesh_item *items;
    uint32_t n, cap;
    int pcx, pcy, pcz, radius;
} plan_ctx;

static void collect(void *user, wn_chunk *c)
{
    plan_ctx *p = (plan_ctx *)user;
    if (!(c->flags & WN_CHUNK_DIRTY_MESH)) return;
    int dx = c->pos.x - p->pcx, dy = c->pos.y - p->pcy, dz = c->pos.z - p->pcz;
    if (abs(dx) > p->radius || abs(dz) > p->radius) return;
    if (p->n == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 256;
        p->items = (wn_remesh_item *)realloc(p->items, p->cap * sizeof *p->items);
    }
    p->items[p->n].chunk = c;
    p->items[p->n].dist2 = dx * dx + dy * dy + dz * dz;
    p->n++;
}

static int cmp_item(const void *a, const void *b)
{
    const wn_remesh_item *x = (const wn_remesh_item *)a, *y = (const wn_remesh_item *)b;
    if (x->dist2 != y->dist2) return x->dist2 < y->dist2 ? -1 : 1;
    if (x->chunk->pos.x != y->chunk->pos.x) return x->chunk->pos.x < y->chunk->pos.x ? -1 : 1;
    if (x->chunk->pos.y != y->chunk->pos.y) return x->chunk->pos.y < y->chunk->pos.y ? -1 : 1;
    return (x->chunk->pos.z > y->chunk->pos.z) - (x->chunk->pos.z < y->chunk->pos.z);
}

uint32_t wn_remesh_plan(wn_world *w, wn_vec3 viewer, int radius, wn_remesh_item *out, uint32_t max)
{
    wn_ivec3 v = wn_world_to_voxel(viewer);
    plan_ctx p = {NULL, 0, 0, v.x >> WN_CHUNK_BITS, v.y >> WN_CHUNK_BITS, v.z >> WN_CHUNK_BITS, radius};
    wn_world_foreach_chunk(w, collect, &p);
    if (p.n) qsort(p.items, p.n, sizeof *p.items, cmp_item);
    for (uint32_t i = 0; i < p.n && i < max; i++) out[i] = p.items[i];
    free(p.items);
    return p.n;
}

uint32_t wn_remesh_fit_budget(const float *cost_ms, uint32_t n, float budget_ms)
{
    if (n == 0) return 0;
    float used = cost_ms[0];
    uint32_t k = 1;
    while (k < n && used + cost_ms[k] <= budget_ms) used += cost_ms[k++];
    return k;
}
