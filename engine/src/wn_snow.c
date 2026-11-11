#include "wn/wn_snow.h"

#include "wn/wn_light.h"

#include <limits.h>
#include <string.h>

void wn_snow_init(wn_snow *s)
{
    memset(s, 0, sizeof *s);
    s->rate = 0.0f;
    s->columns_per_step = 2048;
    s->min_speed = 0.45f;
    s->knee_height = 0.5f;
    s->packed16 = 4;
}

int32_t wn_snow_column_top16(wn_world *w, int32_t x, int32_t z, int32_t y_top)
{
    int32_t y_bottom = w->y_min * WN_CHUNK_SIZE;
    for (int32_t y = y_top; y >= y_bottom; y--) {
        wn_chunk *c = wn_world_chunk(w, wn_iv3(x >> WN_CHUNK_BITS, y >> WN_CHUNK_BITS, z >> WN_CHUNK_BITS));
        if (!c) continue;
        if (!c->vox && !wn_mat_is_solid(w->mats, wn_vox_mat(c->uniform))) {
            y = (y & ~WN_CHUNK_MASK); /* skip the rest of this empty chunk */
            continue;
        }
        wn_voxel v = wn_chunk_get(c, x & WN_CHUNK_MASK, y & WN_CHUNK_MASK, z & WN_CHUNK_MASK);
        if (wn_mat_is_solid(w->mats, wn_vox_mat(v))) return y * 16 + (int32_t)wn_vox_height16(v);
    }
    return INT32_MIN;
}

static int32_t world_top(const wn_world *w) { return (w->y_max + 1) * WN_CHUNK_SIZE - 1; }

int wn_snow_add(wn_edit_ctx *e, int32_t x, int32_t z, int amount16)
{
    wn_world *w = e->world;
    int32_t top = wn_snow_column_top16(w, x, z, world_top(w));
    if (top == INT32_MIN) return 0;
    int added = 0;
    while (amount16 > 0) {
        int32_t y = (top - 1) / 16; /* voxel holding the surface */
        if (top % 16 == 0) y = top / 16 - 1;
        wn_voxel v = wn_world_get(w, x, y, z);
        uint32_t m = wn_vox_mat(v);
        int32_t ty;
        int h;
        if (m == WN_MAT_SNOW && wn_vox_fill(v) != 0) {
            ty = y;
            h = (int)wn_vox_fill(v);
        } else {
            /* Start a new snow voxel above; it must be open to the sky. */
            ty = y + 1;
            h = 0;
            if (m == WN_MAT_FIRE || w->mats->m[m].kind == WN_KIND_LIQUID || w->mats->m[m].emit) break;
            wn_voxel above = wn_world_get(w, x, ty, z);
            if (wn_vox_mat(above) != WN_MAT_AIR || ty > world_top(w)) break;
        }
        int add = WN_MIN(amount16, 16 - h);
        int nh = h + add;
        wn_edit_set(e, x, ty, z, wn_vox_make(WN_MAT_SNOW, nh >= 16 ? 0 : (uint32_t)nh));
        amount16 -= add;
        added += add;
        top = ty * 16 + nh;
    }
    return added;
}

int wn_snow_remove(wn_edit_ctx *e, int32_t x, int32_t z, int amount16)
{
    wn_world *w = e->world;
    int32_t top = wn_snow_column_top16(w, x, z, world_top(w));
    int removed = 0;
    while (amount16 > 0 && top != INT32_MIN) {
        int32_t y = (top % 16 == 0) ? top / 16 - 1 : top / 16;
        wn_voxel v = wn_world_get(w, x, y, z);
        if (wn_vox_mat(v) != WN_MAT_SNOW) break;
        int h = (int)wn_vox_height16(v);
        int take = WN_MIN(amount16, h);
        int nh = h - take;
        wn_voxel nv = nh == 0 ? 0 : wn_vox_set_fill(v, (uint32_t)nh);
        wn_edit_set(e, x, y, z, nv);
        removed += take;
        amount16 -= take;
        top = y * 16 + nh;
    }
    return removed;
}

/* ---- Accumulation passes ---- */

static void bounds_of_loaded(wn_world *w, int *x0, int *x1, int *z0, int *z1)
{
    *x0 = *z0 = INT32_MAX;
    *x1 = *z1 = INT32_MIN;
    for (uint32_t b = 0; b < w->nbuckets; b++)
        for (wn_chunk *c = w->buckets[b]; c; c = c->next) {
            *x0 = WN_MIN(*x0, c->pos.x);
            *x1 = WN_MAX(*x1, c->pos.x);
            *z0 = WN_MIN(*z0, c->pos.z);
            *z1 = WN_MAX(*z1, c->pos.z);
        }
}

/* Processes up to budget voxel columns of the current pass. Returns remaining budget. */
static int run_pass(wn_snow *s, wn_world *w, int budget)
{
    wn_edit_ctx e;
    wn_edit_begin(&e, w);
    while (s->pass_active && budget > 0) {
        if (s->pass_cz > s->pass_z1) {
            s->pass_active = false;
            s->passes_done++;
            break;
        }
        if (wn_world_column_loaded(w, s->pass_cx, s->pass_cz)) {
            for (int z = 0; z < WN_CHUNK_SIZE; z++)
                for (int x = 0; x < WN_CHUNK_SIZE; x++) {
                    int32_t wx = s->pass_cx * WN_CHUNK_SIZE + x, wz = s->pass_cz * WN_CHUNK_SIZE + z;
                    /* The topmost surface of the column is the one the sky sees:
                     * under a roof only the roof receives snow. */
                    wn_snow_add(&e, wx, wz, 1);
                }
            budget -= WN_CHUNK_SIZE * WN_CHUNK_SIZE;
        }
        if (++s->pass_cx > s->pass_x1) {
            s->pass_cx = s->pass_x0;
            s->pass_cz++;
        }
    }
    wn_edit_end(&e);
    return budget;
}

void wn_snow_step(wn_snow *s, wn_world *w, float dt)
{
    if (s->rate > 0.0f) {
        s->timer += dt;
        float period = 1.0f / s->rate;
        if (s->timer >= period && !s->pass_active) {
            s->timer -= period;
            bounds_of_loaded(w, &s->pass_x0, &s->pass_x1, &s->pass_z0, &s->pass_z1);
            if (s->pass_x0 <= s->pass_x1) {
                s->pass_active = true;
                s->pass_cx = s->pass_x0;
                s->pass_cz = s->pass_z0;
            }
        }
    }
    if (s->pass_active) run_pass(s, w, s->columns_per_step);
}

void wn_snow_flush(wn_snow *s, wn_world *w)
{
    while (s->pass_active) run_pass(s, w, INT32_MAX);
}

/* ---- Footprints and movement ---- */

void wn_snow_footprint(wn_snow *s, wn_edit_ctx *e, wn_vec3 foot, float radius)
{
    wn_world *w = e->world;
    wn_ivec3 c = wn_world_to_voxel(foot);
    int r = (int)(radius / WN_VOXEL_SIZE + 0.5f);
    for (int dz = -r; dz <= r; dz++)
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dz * dz > r * r) continue;
            int32_t x = c.x + dx, z = c.z + dz;
            int32_t top = wn_snow_column_top16(w, x, z, c.y + 8);
            if (top == INT32_MIN) continue;
            /* Find the snow base (first non-snow solid). */
            int32_t y = (top % 16 == 0) ? top / 16 - 1 : top / 16;
            int32_t base = y;
            while (base > c.y - 24 && wn_vox_mat(wn_world_get(w, x, base, z)) == WN_MAT_SNOW) base--;
            if (base == y) continue; /* no snow here */
            int32_t floor16 = (base + 1) * 16 + s->packed16;
            /* Snow at the foot level is pushed down to the packed floor. */
            int32_t target = WN_MAX(floor16, (int32_t)(foot.y / WN_VOXEL_SIZE * 16.0f) - 2);
            if (top > target) wn_snow_remove(e, x, z, top - target);
            /* Mark the remaining top as trampled (darker colour). */
            int32_t ny = wn_snow_column_top16(w, x, z, c.y + 8);
            int32_t ty = (ny % 16 == 0) ? ny / 16 - 1 : ny / 16;
            wn_voxel tv = wn_world_get(w, x, ty, z);
            if (wn_vox_mat(tv) == WN_MAT_SNOW && wn_vox_damage(tv) < 6) wn_world_set(w, x, ty, z, wn_vox_set_damage(tv, 6));
        }
}

float wn_snow_depth_at(wn_world *w, wn_vec3 pos)
{
    wn_ivec3 v = wn_world_to_voxel(pos);
    int32_t top = wn_snow_column_top16(w, v.x, v.z, v.y + 2);
    if (top == INT32_MIN) return 0.0f;
    int32_t y = (top % 16 == 0) ? top / 16 - 1 : top / 16;
    int32_t base = y;
    while (base > v.y - 40 && wn_vox_mat(wn_world_get(w, v.x, base, v.z)) == WN_MAT_SNOW) base--;
    return (float)(top - (base + 1) * 16) / 16.0f * WN_VOXEL_SIZE;
}

float wn_snow_speed_factor(const wn_snow *s, wn_world *w, wn_vec3 pos)
{
    float d = wn_snow_depth_at(w, pos);
    float f = 1.0f - (1.0f - s->min_speed) * WN_CLAMP(d / s->knee_height, 0.0f, 1.0f);
    return WN_MAX(f, s->min_speed);
}

/* ---- Branch snow ---- */

int wn_snow_shake(wn_edit_ctx *e, wn_vec3 c, float r, wn_snow_release_fn on_release, void *user)
{
    wn_world *w = e->world;
    wn_ivec3 a = wn_world_to_voxel(wn_v3(c.x - r, c.y - r, c.z - r));
    wn_ivec3 b = wn_world_to_voxel(wn_v3(c.x + r, c.y + r, c.z + r));
    int n = 0;
    /* Top-down so stacked snow is released before the layer it rests on. */
    for (int32_t y = b.y; y >= a.y; y--)
        for (int32_t z = a.z; z <= b.z; z++)
            for (int32_t x = a.x; x <= b.x; x++) {
                wn_voxel v = wn_world_get(w, x, y, z);
                if (wn_vox_mat(v) != WN_MAT_SNOW) continue;
                /* Snow resting on needles (possibly via more snow). */
                int32_t yy = y - 1;
                while (yy > y - 4 && wn_vox_mat(wn_world_get(w, x, yy, z)) == WN_MAT_SNOW) yy--;
                if (wn_vox_mat(wn_world_get(w, x, yy, z)) != WN_MAT_PINE_NEEDLES) continue;
                wn_edit_set(e, x, y, z, 0);
                if (on_release) on_release(user, wn_voxel_center(wn_iv3(x, y, z)));
                n++;
            }
    return n;
}

/* ---- Melting ---- */

void wn_snow_melt(wn_edit_ctx *e, wn_vec3 c, float r, float power, float dt, float *accum)
{
    *accum += power * dt;
    if (*accum < 1.0f) return;
    int levels = (int)*accum;
    *accum -= (float)levels;
    wn_world *w = e->world;
    wn_ivec3 a = wn_world_to_voxel(wn_v3(c.x - r, c.y - r, c.z - r));
    wn_ivec3 b = wn_world_to_voxel(wn_v3(c.x + r, c.y + r, c.z + r));
    for (int32_t y = b.y; y >= a.y; y--)
        for (int32_t z = a.z; z <= b.z; z++)
            for (int32_t x = a.x; x <= b.x; x++) {
                float d = wn_v3_len(wn_v3_sub(wn_voxel_center(wn_iv3(x, y, z)), c));
                if (d > r) continue;
                int melt = (int)((float)levels * (1.0f - d / r) + 0.5f);
                if (melt <= 0) continue;
                wn_voxel v = wn_world_get(w, x, y, z);
                uint32_t m = wn_vox_mat(v);
                if (m == WN_MAT_SNOW) {
                    int h = (int)wn_vox_height16(v) - melt;
                    wn_edit_set(e, x, y, z, h <= 0 ? 0 : wn_vox_set_fill(v, (uint32_t)h));
                } else if (m == WN_MAT_ICE) {
                    wn_edit_set(e, x, y, z, wn_vox_make(WN_MAT_WATER, wn_vox_fill(v)));
                }
            }
}

/* ---- Avalanche settling ---- */

int wn_snow_settle(wn_edit_ctx *e, int32_t x0, int32_t z0, int32_t x1, int32_t z1, int32_t y_top, int slope16,
                   int max_moves)
{
    wn_world *w = e->world;
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int moves = 0;
    bool changed = true;
    while (changed && moves < max_moves) {
        changed = false;
        for (int32_t z = z0; z <= z1 && moves < max_moves; z++)
            for (int32_t x = x0; x <= x1 && moves < max_moves; x++) {
                int32_t h = wn_snow_column_top16(w, x, z, y_top);
                if (h == INT32_MIN) continue;
                int32_t y = (h % 16 == 0) ? h / 16 - 1 : h / 16;
                if (wn_vox_mat(wn_world_get(w, x, y, z)) != WN_MAT_SNOW) continue;
                /* Move towards the lowest neighbour first. */
                int best = -1;
                int32_t best_h = INT32_MAX;
                for (int d = 0; d < 4; d++) {
                    int32_t nx = x + dirs[d][0], nz = z + dirs[d][1];
                    if (nx < x0 || nx > x1 || nz < z0 || nz > z1) continue;
                    int32_t nh = wn_snow_column_top16(w, nx, nz, y_top);
                    if (nh != INT32_MIN && nh < best_h) {
                        best_h = nh;
                        best = d;
                    }
                }
                if (best < 0 || h - best_h <= slope16) continue;
                int amount = (int)((h - best_h - slope16 + 1) / 2);
                amount = WN_CLAMP(amount, 1, 16);
                int got = wn_snow_remove(e, x, z, amount);
                int put = wn_snow_add(e, x + dirs[best][0], z + dirs[best][1], got);
                if (put < got) wn_snow_add(e, x, z, got - put); /* could not land: put it back */
                if (put > 0) {
                    moves++;
                    changed = true;
                }
            }
    }
    return moves;
}
