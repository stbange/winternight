#include "wn/wn_view.h"

#include <stdlib.h>
#include <string.h>

void wn_view_init(wn_view *v)
{
    memset(v, 0, sizeof *v);
    v->cap = 1024;
    v->cols = (wn_view_column *)calloc(v->cap, sizeof *v->cols);
    v->lod_near = 1;
    v->lod_mid = 3;
}

static void free_column(wn_view *v, wn_view_column *c)
{
    for (int i = 0; i < WN_VIEW_MAX_Y; i++) wn_mesh_free(&c->chunks[i].mesh);
    wn_mesh_free(&c->mesh);
    if (c->gpu) {
        if (v->nfreed == v->capfreed) {
            v->capfreed = v->capfreed ? v->capfreed * 2 : 64;
            v->freed = (uint32_t *)realloc(v->freed, v->capfreed * sizeof(uint32_t));
        }
        v->freed[v->nfreed++] = c->gpu;
    }
    memset(c, 0, sizeof *c);
}

void wn_view_free(wn_view *v)
{
    for (uint32_t i = 0; i < v->cap; i++)
        if (v->cols[i].used) free_column(v, &v->cols[i]);
    free(v->cols);
    free(v->freed);
    memset(v, 0, sizeof *v);
}

int wn_view_lod_for_distance(const wn_view *v, int d)
{
    if (d <= v->lod_near) return 0;
    if (d <= v->lod_mid) return 1;
    return 2;
}

static uint32_t col_hash(int cx, int cz) { return ((uint32_t)cx * 73856093u) ^ ((uint32_t)cz * 83492791u); }

wn_view_column *wn_view_find(wn_view *v, int cx, int cz)
{
    uint32_t h = col_hash(cx, cz) & (v->cap - 1);
    for (uint32_t i = 0; i < v->cap; i++) {
        wn_view_column *c = &v->cols[(h + i) & (v->cap - 1)];
        if (!c->used) return NULL;
        if (c->cx == cx && c->cz == cz) return c;
    }
    return NULL;
}

static wn_view_column *insert(wn_view *v, int cx, int cz)
{
    uint32_t h = col_hash(cx, cz) & (v->cap - 1);
    for (uint32_t i = 0; i < v->cap; i++) {
        wn_view_column *c = &v->cols[(h + i) & (v->cap - 1)];
        if (!c->used) {
            memset(c, 0, sizeof *c);
            c->used = true;
            c->cx = cx;
            c->cz = cz;
            c->lod = -1;
            for (int k = 0; k < WN_VIEW_MAX_Y; k++) c->chunks[k].lod = -1;
            v->ncols++;
            return c;
        }
    }
    return NULL; /* table full: radius too large for the table */
}

/* Removing from open addressing: rebuild the table without the column. */
static void remove_far(wn_view *v, int pcx, int pcz, int keep)
{
    bool any = false;
    for (uint32_t i = 0; i < v->cap; i++) {
        wn_view_column *c = &v->cols[i];
        if (c->used && (abs(c->cx - pcx) > keep || abs(c->cz - pcz) > keep)) {
            free_column(v, c);
            v->ncols--;
            any = true;
        }
    }
    if (!any) return;
    wn_view_column *old = v->cols;
    v->cols = (wn_view_column *)calloc(v->cap, sizeof *v->cols);
    uint32_t n = v->ncols;
    v->ncols = 0;
    for (uint32_t i = 0; i < v->cap; i++) {
        if (!old[i].used) continue;
        wn_view_column *c = insert(v, old[i].cx, old[i].cz);
        *c = old[i];
        v->ncols++;
        (void)n;
    }
    v->ncols = n;
    free(old);
}

typedef struct todo {
    wn_view_column *col;
    int cy;     /* chunk index within column */
    int lod;
    int dist;
} todo;

static int cmp_todo(const void *a, const void *b)
{
    const todo *x = (const todo *)a, *y = (const todo *)b;
    if (x->dist != y->dist) return x->dist - y->dist;
    if (x->col->cx != y->col->cx) return x->col->cx - y->col->cx;
    if (x->col->cz != y->col->cz) return x->col->cz - y->col->cz;
    return x->cy - y->cy;
}

static void assemble(wn_view_column *c)
{
    wn_mesh_clear(&c->mesh);
    for (int k = 0; k < WN_VIEW_MAX_Y; k++) {
        wn_view_chunk *vc = &c->chunks[k];
        if (vc->lod < 0 || vc->mesh.nverts == 0) continue;
        int cells = WN_CHUNK_SIZE >> c->lod;
        wn_mesh_append(&c->mesh, &vc->mesh, (uint16_t)(k * cells * 16));
    }
    c->dirty = false;
    c->gpu_dirty = true;
}

void wn_view_update(wn_view *v, wn_world *w, wn_vec3 viewer, int radius, wn_view_budget_fn budget, void *user)
{
    wn_ivec3 pv = wn_world_to_voxel(viewer);
    int pcx = pv.x >> WN_CHUNK_BITS, pcz = pv.z >> WN_CHUNK_BITS;
    remove_far(v, pcx, pcz, radius + 1);
    int ny = w->y_max - w->y_min + 1;
    if (ny > WN_VIEW_MAX_Y) ny = WN_VIEW_MAX_Y;
    /* Collect work. */
    uint32_t cap = 256, n = 0;
    todo *list = (todo *)malloc(cap * sizeof *list);
    for (int dz = -radius; dz <= radius; dz++)
        for (int dx = -radius; dx <= radius; dx++) {
            int cx = pcx + dx, cz = pcz + dz;
            if (!wn_world_column_loaded(w, cx, cz)) continue;
            wn_view_column *c = wn_view_find(v, cx, cz);
            if (!c) c = insert(v, cx, cz);
            if (!c) continue;
            int d = WN_MAX(abs(dx), abs(dz));
            int lod = wn_view_lod_for_distance(v, d);
            if (c->lod != lod) {
                /* Detail level changed: every chunk of the column must follow. */
                c->lod = lod;
                for (int k = 0; k < ny; k++) c->chunks[k].lod = -1;
            }
            for (int k = 0; k < ny; k++) {
                wn_chunk *ch = wn_world_chunk(w, wn_iv3(cx, w->y_min + k, cz));
                if (!ch) continue;
                wn_view_chunk *vc = &c->chunks[k];
                if (vc->lod == lod && vc->rev == ch->mesh_rev) continue;
                if (n == cap) {
                    cap *= 2;
                    list = (todo *)realloc(list, cap * sizeof *list);
                }
                list[n].col = c;
                list[n].cy = k;
                list[n].lod = lod;
                list[n].dist = d * 64 + abs(k - ((pv.y >> WN_CHUNK_BITS) - w->y_min));
                n++;
            }
        }
    if (n) qsort(list, n, sizeof *list, cmp_todo);
    v->meshed_last = 0;
    uint32_t i = 0;
    for (; i < n; i++) {
        if (budget && v->meshed_last > 0 && !budget(user)) break;
        todo *t = &list[i];
        wn_chunk *ch = wn_world_chunk(w, wn_iv3(t->col->cx, w->y_min + t->cy, t->col->cz));
        wn_view_chunk *vc = &t->col->chunks[t->cy];
        wn_mesh_chunk_lod(&vc->mesh, w, ch->pos, t->lod);
        vc->lod = t->lod;
        vc->rev = ch->mesh_rev;
        ch->flags &= ~(uint32_t)WN_CHUNK_DIRTY_MESH;
        t->col->dirty = true;
        v->meshed_last++;
    }
    v->pending_last = n - i;
    free(list);
    /* Reassemble columns whose chunks are all at the column's detail level. */
    for (uint32_t k = 0; k < v->cap; k++) {
        wn_view_column *c = &v->cols[k];
        if (!c->used || !c->dirty) continue;
        bool complete = true;
        for (int j = 0; j < ny && complete; j++)
            if (wn_world_chunk(w, wn_iv3(c->cx, w->y_min + j, c->cz)) && c->chunks[j].lod != c->lod) complete = false;
        if (complete) assemble(c);
    }
}

uint32_t wn_view_take_freed(wn_view *v, uint32_t *out, uint32_t max)
{
    uint32_t n = WN_MIN(max, v->nfreed);
    memcpy(out, v->freed, n * sizeof(uint32_t));
    memmove(v->freed, v->freed + n, (v->nfreed - n) * sizeof(uint32_t));
    v->nfreed -= n;
    return n;
}

/* ---- Frustum ---- */

wn_frustum wn_frustum_from_matrix(wn_mat4 m)
{
    wn_frustum f;
    const float *a = m.m;
    /* rows of the column-major matrix */
    float r[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) r[i][j] = a[j * 4 + i];
    for (int k = 0; k < 4; k++) {
        f.planes[0][k] = r[3][k] + r[0][k];
        f.planes[1][k] = r[3][k] - r[0][k];
        f.planes[2][k] = r[3][k] + r[1][k];
        f.planes[3][k] = r[3][k] - r[1][k];
        f.planes[4][k] = r[3][k] + r[2][k];
        f.planes[5][k] = r[3][k] - r[2][k];
    }
    return f;
}

bool wn_frustum_box(const wn_frustum *f, wn_vec3 mn, wn_vec3 mx)
{
    for (int i = 0; i < 6; i++) {
        const float *p = f->planes[i];
        wn_vec3 v = wn_v3(p[0] >= 0 ? mx.x : mn.x, p[1] >= 0 ? mx.y : mn.y, p[2] >= 0 ? mx.z : mn.z);
        if (p[0] * v.x + p[1] * v.y + p[2] * v.z + p[3] < 0.0f) return false;
    }
    return true;
}

void wn_view_column_bounds(const wn_world *w, int cx, int cz, wn_vec3 *mn, wn_vec3 *mx)
{
    float s = WN_CHUNK_SIZE * WN_VOXEL_SIZE;
    *mn = wn_v3((float)cx * s, (float)w->y_min * s, (float)cz * s);
    *mx = wn_v3((float)(cx + 1) * s, (float)(w->y_max + 1) * s, (float)(cz + 1) * s);
}
