#include "wn/wn_light.h"

#include <stdlib.h>
#include <string.h>

typedef struct lnode {
    int32_t x, y, z;
    uint8_t l;
} lnode;

typedef struct lqueue {
    lnode *a;
    size_t head, tail, cap;
} lqueue;

static uint32_t g_work;

uint32_t wn_light_last_work(void) { return g_work; }

static void q_push(lqueue *q, int32_t x, int32_t y, int32_t z, uint8_t l)
{
    if (q->tail == q->cap) {
        if (q->head > 0) {
            memmove(q->a, q->a + q->head, (q->tail - q->head) * sizeof *q->a);
            q->tail -= q->head;
            q->head = 0;
        }
        if (q->tail == q->cap) {
            q->cap = q->cap ? q->cap * 2 : 4096;
            q->a = (lnode *)realloc(q->a, q->cap * sizeof *q->a);
        }
    }
    lnode n = {x, y, z, l};
    q->a[q->tail++] = n;
}

static void q_free(lqueue *q)
{
    free(q->a);
    memset(q, 0, sizeof *q);
}

static inline uint32_t chan_get(wn_voxel v, wn_light_channel ch) { return ch == WN_LIGHT_SKY ? wn_vox_sky(v) : wn_vox_block(v); }
static inline wn_voxel chan_set(wn_voxel v, wn_light_channel ch, uint32_t l)
{
    return ch == WN_LIGHT_SKY ? wn_vox_set_sky(v, l) : wn_vox_set_block(v, l);
}

uint32_t wn_light_get(wn_world *w, wn_light_channel ch, int32_t x, int32_t y, int32_t z)
{
    return chan_get(wn_world_get(w, x, y, z), ch);
}

static const int k_dirs[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

static bool in_range(const wn_world *w, int32_t y)
{
    return y >= w->y_min * WN_CHUNK_SIZE && y < (w->y_max + 1) * WN_CHUNK_SIZE;
}

/* Spreads light from every queued node. */
static void propagate(wn_world *w, lqueue *q, wn_light_channel ch)
{
    while (q->head < q->tail) {
        lnode n = q->a[q->head++];
        g_work++;
        wn_voxel cur = wn_world_get(w, n.x, n.y, n.z);
        uint32_t l = chan_get(cur, ch);
        if (l <= 1) continue;
        for (int d = 0; d < 6; d++) {
            int32_t x = n.x + k_dirs[d][0], y = n.y + k_dirs[d][1], z = n.z + k_dirs[d][2];
            if (!in_range(w, y)) continue;
            wn_chunk *c = wn_world_chunk(w, wn_iv3(x >> WN_CHUNK_BITS, y >> WN_CHUNK_BITS, z >> WN_CHUNK_BITS));
            if (!c) continue;
            wn_voxel nv = wn_chunk_get(c, x & WN_CHUNK_MASK, y & WN_CHUNK_MASK, z & WN_CHUNK_MASK);
            if (wn_mat_is_opaque(w->mats, wn_vox_mat(nv))) continue;
            uint32_t target = (ch == WN_LIGHT_SKY && d == 3 && l == 15) ? 15u : l - 1u;
            if (chan_get(nv, ch) >= target) continue;
            wn_world_set_quiet(w, x, y, z, chan_set(nv, ch, target));
            q_push(q, x, y, z, (uint8_t)target);
        }
    }
}

/* Removes light that originated from (x,y,z) and collects re-light seeds. */
static void remove_light(wn_world *w, int32_t sx, int32_t sy, int32_t sz, wn_light_channel ch, lqueue *relight)
{
    lqueue rq;
    memset(&rq, 0, sizeof rq);
    wn_voxel v = wn_world_get(w, sx, sy, sz);
    uint32_t old = chan_get(v, ch);
    if (old == 0) return;
    wn_world_set_quiet(w, sx, sy, sz, chan_set(v, ch, 0));
    q_push(&rq, sx, sy, sz, (uint8_t)old);
    while (rq.head < rq.tail) {
        lnode n = rq.a[rq.head++];
        g_work++;
        for (int d = 0; d < 6; d++) {
            int32_t x = n.x + k_dirs[d][0], y = n.y + k_dirs[d][1], z = n.z + k_dirs[d][2];
            if (!in_range(w, y)) continue;
            wn_chunk *c = wn_world_chunk(w, wn_iv3(x >> WN_CHUNK_BITS, y >> WN_CHUNK_BITS, z >> WN_CHUNK_BITS));
            if (!c) continue;
            wn_voxel nv = wn_chunk_get(c, x & WN_CHUNK_MASK, y & WN_CHUNK_MASK, z & WN_CHUNK_MASK);
            uint32_t nl = chan_get(nv, ch);
            if (nl == 0) continue;
            bool from_here = nl < n.l || (ch == WN_LIGHT_SKY && d == 3 && n.l == 15 && nl == 15);
            uint32_t emit = w->mats->m[wn_vox_mat(nv)].emit;
            if (from_here && !(ch == WN_LIGHT_BLOCK && emit > 0)) {
                wn_world_set_quiet(w, x, y, z, chan_set(nv, ch, 0));
                q_push(&rq, x, y, z, (uint8_t)nl);
            } else {
                q_push(relight, x, y, z, (uint8_t)nl);
            }
        }
    }
    q_free(&rq);
}

void wn_light_voxel_changed(wn_world *w, int32_t x, int32_t y, int32_t z)
{
    g_work = 0;
    for (int ch = 0; ch < 2; ch++) {
        lqueue relight;
        memset(&relight, 0, sizeof relight);
        remove_light(w, x, y, z, (wn_light_channel)ch, &relight);
        wn_voxel v = wn_world_get(w, x, y, z);
        uint32_t emit = w->mats->m[wn_vox_mat(v)].emit;
        if (ch == WN_LIGHT_BLOCK && emit > 0) {
            wn_world_set_quiet(w, x, y, z, wn_vox_set_block(v, emit));
            q_push(&relight, x, y, z, (uint8_t)emit);
        }
        /* Neighbours may light the changed voxel now. */
        for (int d = 0; d < 6; d++) {
            int32_t nx = x + k_dirs[d][0], ny = y + k_dirs[d][1], nz = z + k_dirs[d][2];
            if (!in_range(w, ny)) continue;
            uint32_t l = chan_get(wn_world_get(w, nx, ny, nz), (wn_light_channel)ch);
            if (l > 0) q_push(&relight, nx, ny, nz, (uint8_t)l);
        }
        /* The top of the world is always open sky. */
        if (ch == WN_LIGHT_SKY && y == (w->y_max + 1) * WN_CHUNK_SIZE - 1 &&
            !wn_mat_is_opaque(w->mats, wn_vox_mat(v))) {
            wn_world_set_quiet(w, x, y, z, wn_vox_set_sky(wn_world_get(w, x, y, z), 15));
            q_push(&relight, x, y, z, 15);
        }
        propagate(w, &relight, (wn_light_channel)ch);
        q_free(&relight);
    }
}

/* ---- Column initialisation ---- */

void wn_light_column_loaded(void *user, wn_world *w, int cx, int cz)
{
    (void)user;
    g_work = 0;
    const wn_materials *mats = w->mats;
    bool open[WN_CHUNK_SIZE * WN_CHUNK_SIZE];
    for (int i = 0; i < WN_CHUNK_SIZE * WN_CHUNK_SIZE; i++) open[i] = true;
    int open_count = WN_CHUNK_SIZE * WN_CHUNK_SIZE;
    lqueue sky, blk;
    memset(&sky, 0, sizeof sky);
    memset(&blk, 0, sizeof blk);

    for (int cy = w->y_max; cy >= w->y_min; cy--) {
        wn_chunk *c = wn_world_chunk(w, wn_iv3(cx, cy, cz));
        if (!c) continue;
        if (!c->vox) {
            uint32_t m = wn_vox_mat(c->uniform);
            if (wn_mat_is_opaque(mats, m)) {
                if (mats->m[m].emit == 0) {
                    for (int i = 0; i < WN_CHUNK_SIZE * WN_CHUNK_SIZE; i++) open[i] = false;
                    open_count = 0;
                    c->uniform = wn_vox_set_sky(wn_vox_set_block(c->uniform, 0), 0);
                    continue;
                }
            } else if (open_count == WN_CHUNK_SIZE * WN_CHUNK_SIZE) {
                c->uniform = wn_vox_set_sky(c->uniform, 15);
                continue; /* fully open: neighbours spread into it on demand */
            }
            wn_chunk_expand(c);
        }
        int32_t bx = cx * WN_CHUNK_SIZE, by = cy * WN_CHUNK_SIZE, bz = cz * WN_CHUNK_SIZE;
        for (int y = WN_CHUNK_SIZE - 1; y >= 0; y--) {
            for (int z = 0; z < WN_CHUNK_SIZE; z++) {
                for (int x = 0; x < WN_CHUNK_SIZE; x++) {
                    int idx = wn_chunk_index(x, y, z);
                    wn_voxel v = c->vox[idx];
                    uint32_t m = wn_vox_mat(v);
                    bool opaque = wn_mat_is_opaque(mats, m);
                    int col = z * WN_CHUNK_SIZE + x;
                    if (opaque && open[col]) {
                        open[col] = false;
                        open_count--;
                    }
                    uint32_t s = open[col] ? 15u : 0u;
                    uint32_t e = mats->m[m].emit;
                    v = wn_vox_set_block(wn_vox_set_sky(v, s), e);
                    c->vox[idx] = v;
                    if (s == 15) {
                        /* Only seed sky voxels next to something darker. */
                        q_push(&sky, bx + x, by + y, bz + z, 15);
                    }
                    if (e > 0) q_push(&blk, bx + x, by + y, bz + z, (uint8_t)e);
                }
            }
        }
        c->flags |= WN_CHUNK_DIRTY_MESH;
        c->mesh_rev++;
    }
    /* Pull light in from neighbouring columns across the border. */
    int32_t y0 = w->y_min * WN_CHUNK_SIZE, y1 = (w->y_max + 1) * WN_CHUNK_SIZE;
    for (int side = 0; side < 4; side++) {
        int ncx = cx + (side == 0) - (side == 1), ncz = cz + (side == 2) - (side == 3);
        if (!wn_world_column_loaded(w, ncx, ncz)) continue;
        for (int i = 0; i < WN_CHUNK_SIZE; i++) {
            int32_t x, z;
            if (side < 2) {
                x = side == 0 ? (cx + 1) * WN_CHUNK_SIZE : cx * WN_CHUNK_SIZE - 1;
                z = cz * WN_CHUNK_SIZE + i;
            } else {
                z = side == 2 ? (cz + 1) * WN_CHUNK_SIZE : cz * WN_CHUNK_SIZE - 1;
                x = cx * WN_CHUNK_SIZE + i;
            }
            for (int32_t y = y0; y < y1; y++) {
                wn_voxel v = wn_world_get(w, x, y, z);
                if (wn_vox_sky(v) > 1) q_push(&sky, x, y, z, (uint8_t)wn_vox_sky(v));
                if (wn_vox_block(v) > 1) q_push(&blk, x, y, z, (uint8_t)wn_vox_block(v));
            }
        }
    }
    propagate(w, &sky, WN_LIGHT_SKY);
    propagate(w, &blk, WN_LIGHT_BLOCK);
    q_free(&sky);
    q_free(&blk);
}
