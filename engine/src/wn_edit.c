#include "wn/wn_edit.h"

#include "wn/wn_core.h"
#include "wn/wn_light.h"

#include <stdlib.h>
#include <string.h>

/* ---------------- Edits ---------------- */

void wn_edit_begin(wn_edit_ctx *e, wn_world *w)
{
    memset(e, 0, sizeof *e);
    e->world = w;
}

void wn_edit_end(wn_edit_ctx *e)
{
    free(e->touched);
    e->touched = NULL;
    e->ntouched = e->cap_touched = 0;
}

static void touch(wn_edit_ctx *e, int32_t x, int32_t y, int32_t z)
{
    if (e->ntouched == e->cap_touched) {
        e->cap_touched = e->cap_touched ? e->cap_touched * 2 : 64;
        e->touched = (wn_ivec3 *)realloc(e->touched, e->cap_touched * sizeof *e->touched);
    }
    e->touched[e->ntouched++] = wn_iv3(x, y, z);
}

void wn_edit_set(wn_edit_ctx *e, int32_t x, int32_t y, int32_t z, wn_voxel v)
{
    wn_voxel old = wn_world_get(e->world, x, y, z);
    if ((old & 0x00FFFFFFu) == (v & 0x00FFFFFFu)) return;
    wn_world_set(e->world, x, y, z, v);
    if (wn_world_chunk(e->world, wn_voxel_to_chunk(wn_iv3(x, y, z)))) wn_light_voxel_changed(e->world, x, y, z);
}

bool wn_edit_damage(wn_edit_ctx *e, int32_t x, int32_t y, int32_t z, float damage)
{
    if (damage <= 0.0f) return false;
    wn_voxel v = wn_world_get(e->world, x, y, z);
    uint32_t m = wn_vox_mat(v);
    const wn_material *mat = &e->world->mats->m[m];
    if (!wn_mat_is_solid(e->world->mats, m) || mat->anchor) return false;
    float add = mat->hardness > 0.0f ? damage * 16.0f / mat->hardness : 16.0f;
    float total = (float)wn_vox_damage(v) + add;
    if (total >= 15.5f) {
        if (e->on_destroy) e->on_destroy(e->user, wn_iv3(x, y, z), v);
        wn_edit_set(e, x, y, z, 0);
        touch(e, x, y, z);
        e->destroyed++;
        return true;
    }
    uint32_t d = (uint32_t)(total + 0.5f);
    if (d > 15) d = 15;
    if (d != wn_vox_damage(v)) {
        wn_world_set(e->world, x, y, z, wn_vox_set_damage(v, d));
        e->damaged++;
    }
    return false;
}

static void voxel_range(wn_vec3 mn, wn_vec3 mx, wn_ivec3 *a, wn_ivec3 *b)
{
    *a = wn_world_to_voxel(mn);
    *b = wn_world_to_voxel(mx);
}

void wn_edit_sphere(wn_edit_ctx *e, wn_vec3 c, float r, float damage)
{
    wn_ivec3 a, b;
    voxel_range(wn_v3(c.x - r, c.y - r, c.z - r), wn_v3(c.x + r, c.y + r, c.z + r), &a, &b);
    for (int32_t y = a.y; y <= b.y; y++)
        for (int32_t z = a.z; z <= b.z; z++)
            for (int32_t x = a.x; x <= b.x; x++) {
                float d = wn_v3_len(wn_v3_sub(wn_voxel_center(wn_iv3(x, y, z)), c));
                if (d > r) continue;
                wn_edit_damage(e, x, y, z, damage * (1.0f - d / r));
            }
}

void wn_edit_box(wn_edit_ctx *e, wn_vec3 mn, wn_vec3 mx, float damage)
{
    wn_ivec3 a, b;
    voxel_range(mn, mx, &a, &b);
    for (int32_t y = a.y; y <= b.y; y++)
        for (int32_t z = a.z; z <= b.z; z++)
            for (int32_t x = a.x; x <= b.x; x++) {
                wn_vec3 p = wn_voxel_center(wn_iv3(x, y, z));
                if (p.x < mn.x || p.y < mn.y || p.z < mn.z || p.x > mx.x || p.y > mx.y || p.z > mx.z) continue;
                wn_edit_damage(e, x, y, z, damage);
            }
}

static float seg_dist(wn_vec3 p, wn_vec3 a, wn_vec3 b)
{
    wn_vec3 ab = wn_v3_sub(b, a);
    float l2 = wn_v3_dot(ab, ab);
    float t = l2 > 0.0f ? wn_v3_dot(wn_v3_sub(p, a), ab) / l2 : 0.0f;
    t = WN_CLAMP(t, 0.0f, 1.0f);
    return wn_v3_len(wn_v3_sub(p, wn_v3_add(a, wn_v3_scale(ab, t))));
}

void wn_edit_capsule(wn_edit_ctx *e, wn_vec3 a, wn_vec3 b, float r, float damage)
{
    wn_vec3 mn = wn_v3(WN_MIN(a.x, b.x) - r, WN_MIN(a.y, b.y) - r, WN_MIN(a.z, b.z) - r);
    wn_vec3 mx = wn_v3(WN_MAX(a.x, b.x) + r, WN_MAX(a.y, b.y) + r, WN_MAX(a.z, b.z) + r);
    wn_ivec3 lo, hi;
    voxel_range(mn, mx, &lo, &hi);
    for (int32_t y = lo.y; y <= hi.y; y++)
        for (int32_t z = lo.z; z <= hi.z; z++)
            for (int32_t x = lo.x; x <= hi.x; x++)
                if (seg_dist(wn_voxel_center(wn_iv3(x, y, z)), a, b) <= r) wn_edit_damage(e, x, y, z, damage);
}

void wn_edit_fill_sphere(wn_edit_ctx *e, wn_vec3 c, float r, wn_voxel v)
{
    wn_ivec3 a, b;
    voxel_range(wn_v3(c.x - r, c.y - r, c.z - r), wn_v3(c.x + r, c.y + r, c.z + r), &a, &b);
    for (int32_t y = a.y; y <= b.y; y++)
        for (int32_t z = a.z; z <= b.z; z++)
            for (int32_t x = a.x; x <= b.x; x++) {
                if (wn_v3_len(wn_v3_sub(wn_voxel_center(wn_iv3(x, y, z)), c)) > r) continue;
                if (wn_vox_mat(wn_world_get(e->world, x, y, z)) != WN_MAT_AIR) continue;
                wn_edit_set(e, x, y, z, v);
            }
}

/* ---------------- Debris ---------------- */

void wn_debris_init(wn_debris_set *s)
{
    memset(s, 0, sizeof *s);
    s->min_voxels = 24;
    s->max_island = 150000;
    s->break_speed = 9.0f;
}

static void debris_release(wn_debris *d)
{
    free(d->grid);
    free(d->samples);
    memset(d, 0, sizeof *d);
}

void wn_debris_free(wn_debris_set *s)
{
    for (int i = 0; i < WN_MAX_DEBRIS; i++) debris_release(&s->d[i]);
}

uint32_t wn_debris_count(const wn_debris_set *s)
{
    uint32_t n = 0;
    for (int i = 0; i < WN_MAX_DEBRIS; i++) n += s->d[i].active;
    return n;
}

static inline size_t gidx(const wn_debris *d, int x, int y, int z)
{
    return ((size_t)y * (size_t)d->sz + (size_t)z) * (size_t)d->sx + (size_t)x;
}

/* Computes mass properties and surface samples from the grid. */
static bool debris_prepare(wn_debris *d, const wn_materials *mats)
{
    double mx = 0, my = 0, mz = 0, mass = 0;
    uint32_t n = 0;
    for (int y = 1; y < d->sy - 1; y++)
        for (int z = 1; z < d->sz - 1; z++)
            for (int x = 1; x < d->sx - 1; x++) {
                wn_voxel v = d->grid[gidx(d, x, y, z)];
                if (wn_vox_mat(v) == WN_MAT_AIR) continue;
                double m = mats->m[wn_vox_mat(v)].density + 0.05;
                mx += m * (x - 1 + 0.5);
                my += m * (y - 1 + 0.5);
                mz += m * (z - 1 + 0.5);
                mass += m;
                n++;
            }
    if (n == 0) return false;
    d->nvoxels = n;
    d->com_local = wn_v3((float)(mx / mass), (float)(my / mass), (float)(mz / mass));
    float vol = WN_VOXEL_SIZE * WN_VOXEL_SIZE * WN_VOXEL_SIZE * 1000.0f; /* litres per voxel */
    d->mass = (float)mass * vol;
    d->inv_mass = 1.0f / d->mass;
    /* Inertia of the bounding box (scalar approximation). */
    float ex = (float)(d->sx - 2) * WN_VOXEL_SIZE, ey = (float)(d->sy - 2) * WN_VOXEL_SIZE,
          ez = (float)(d->sz - 2) * WN_VOXEL_SIZE;
    float inertia = d->mass * (ex * ex + ey * ey + ez * ez) / 12.0f;
    d->inv_inertia = inertia > 0.0f ? 1.0f / inertia : 0.0f;
    /* Surface samples: solid voxels with an empty neighbour, thinned out. */
    uint32_t cap = 512, k = 0;
    free(d->samples);
    d->samples = (wn_vec3 *)malloc(cap * sizeof(wn_vec3));
    uint32_t surface = 0;
    for (int pass = 0; pass < 2; pass++) {
        uint32_t stride = pass == 0 ? 1 : (surface / cap) + 1, i = 0;
        for (int y = 1; y < d->sy - 1; y++)
            for (int z = 1; z < d->sz - 1; z++)
                for (int x = 1; x < d->sx - 1; x++) {
                    if (wn_vox_mat(d->grid[gidx(d, x, y, z)]) == WN_MAT_AIR) continue;
                    bool exposed = !wn_vox_mat(d->grid[gidx(d, x + 1, y, z)]) || !wn_vox_mat(d->grid[gidx(d, x - 1, y, z)]) ||
                                   !wn_vox_mat(d->grid[gidx(d, x, y + 1, z)]) || !wn_vox_mat(d->grid[gidx(d, x, y - 1, z)]) ||
                                   !wn_vox_mat(d->grid[gidx(d, x, y, z + 1)]) || !wn_vox_mat(d->grid[gidx(d, x, y, z - 1)]);
                    if (!exposed) continue;
                    if (pass == 0) {
                        surface++;
                    } else if (i++ % stride == 0 && k < cap) {
                        d->samples[k++] = wn_v3((float)(x - 1) + 0.5f - d->com_local.x, (float)(y - 1) + 0.5f - d->com_local.y,
                                                (float)(z - 1) + 0.5f - d->com_local.z);
                    }
                }
    }
    d->nsamples = k;
    d->mesh_rev++;
    return true;
}

static wn_debris *alloc_debris(wn_debris_set *s)
{
    for (int i = 0; i < WN_MAX_DEBRIS; i++)
        if (!s->d[i].active) return &s->d[i];
    /* Full: recycle the oldest piece (deterministic: highest age, lowest index). */
    int best = 0;
    for (int i = 1; i < WN_MAX_DEBRIS; i++)
        if (s->d[i].age > s->d[best].age) best = i;
    debris_release(&s->d[best]);
    return &s->d[best];
}

wn_debris *wn_debris_spawn(wn_debris_set *s, const wn_voxel *grid, int sx, int sy, int sz, wn_vec3 origin,
                           const wn_materials *mats)
{
    wn_debris *d = alloc_debris(s);
    memset(d, 0, sizeof *d);
    d->sx = sx;
    d->sy = sy;
    d->sz = sz;
    size_t n = (size_t)sx * sy * sz;
    d->grid = (wn_voxel *)malloc(n * sizeof(wn_voxel));
    memcpy(d->grid, grid, n * sizeof(wn_voxel));
    if (!debris_prepare(d, mats)) {
        debris_release(d);
        return NULL;
    }
    d->active = true;
    d->id = ++s->next_id;
    d->rot = wn_quat_identity();
    d->pos = wn_v3_add(origin, wn_v3_scale(d->com_local, WN_VOXEL_SIZE));
    return d;
}

/* ---- Connectivity ---- */

typedef struct visit_set {
    uint64_t *keys;
    uint32_t cap, count;
} visit_set;

static uint64_t pack(int32_t x, int32_t y, int32_t z)
{
    return ((uint64_t)(uint32_t)(x + 0x100000) & 0x1FFFFF) | (((uint64_t)(uint32_t)(y + 0x100000) & 0x1FFFFF) << 21) |
           (((uint64_t)(uint32_t)(z + 0x100000) & 0x1FFFFF) << 42) | (1ull << 63);
}

static bool vs_insert(visit_set *v, uint64_t k)
{
    if ((v->count + 1) * 2 > v->cap) {
        uint32_t nc = v->cap ? v->cap * 2 : 1024;
        uint64_t *nk = (uint64_t *)calloc(nc, sizeof(uint64_t));
        for (uint32_t i = 0; i < v->cap; i++) {
            if (!v->keys[i]) continue;
            uint32_t h = (uint32_t)(v->keys[i] * 0x9E3779B97F4A7C15ull >> 32) & (nc - 1);
            while (nk[h]) h = (h + 1) & (nc - 1);
            nk[h] = v->keys[i];
        }
        free(v->keys);
        v->keys = nk;
        v->cap = nc;
    }
    uint32_t h = (uint32_t)(k * 0x9E3779B97F4A7C15ull >> 32) & (v->cap - 1);
    while (v->keys[h]) {
        if (v->keys[h] == k) return false;
        h = (h + 1) & (v->cap - 1);
    }
    v->keys[h] = k;
    v->count++;
    return true;
}

static const int k_n6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

/* Flood fill of solid voxels from start. Returns true if anchored (anchor
 * material, world floor, unloaded chunk or search limit reached). */
static bool flood(wn_world *w, wn_ivec3 start, uint32_t limit, visit_set *seen, wn_ivec3 **out, uint32_t *nout)
{
    uint32_t cap = 1024, n = 0, head = 0;
    wn_ivec3 *q = (wn_ivec3 *)malloc(cap * sizeof *q);
    q[n++] = start;
    vs_insert(seen, pack(start.x, start.y, start.z));
    bool anchored = false;
    int32_t floor_y = w->y_min * WN_CHUNK_SIZE;
    while (head < n) {
        wn_ivec3 p = q[head++];
        if (p.y <= floor_y || w->mats->m[wn_vox_mat(wn_world_get(w, p.x, p.y, p.z))].anchor) {
            anchored = true;
            break;
        }
        if (n >= limit) {
            anchored = true;
            break;
        }
        for (int d = 0; d < 6; d++) {
            wn_ivec3 np = wn_iv3(p.x + k_n6[d][0], p.y + k_n6[d][1], p.z + k_n6[d][2]);
            if (!wn_world_chunk(w, wn_voxel_to_chunk(np))) {
                anchored = true; /* unknown world beyond: assume connected */
                break;
            }
            if (!wn_world_is_solid(w, np.x, np.y, np.z)) continue;
            if (!vs_insert(seen, pack(np.x, np.y, np.z))) continue;
            if (n == cap) {
                cap *= 2;
                q = (wn_ivec3 *)realloc(q, cap * sizeof *q);
            }
            q[n++] = np;
        }
        if (anchored) break;
    }
    if (anchored) {
        free(q);
        *out = NULL;
        *nout = 0;
    } else {
        *out = q;
        *nout = n;
    }
    return anchored;
}

static void island_to_debris(wn_debris_set *s, wn_edit_ctx *e, wn_ivec3 *vox, uint32_t n)
{
    wn_world *w = e->world;
    wn_ivec3 mn = vox[0], mx = vox[0];
    for (uint32_t i = 1; i < n; i++) {
        mn = wn_iv3(WN_MIN(mn.x, vox[i].x), WN_MIN(mn.y, vox[i].y), WN_MIN(mn.z, vox[i].z));
        mx = wn_iv3(WN_MAX(mx.x, vox[i].x), WN_MAX(mx.y, vox[i].y), WN_MAX(mx.z, vox[i].z));
    }
    int sx = mx.x - mn.x + 3, sy = mx.y - mn.y + 3, sz = mx.z - mn.z + 3;
    wn_voxel *g = (wn_voxel *)calloc((size_t)sx * sy * sz, sizeof(wn_voxel));
    for (uint32_t i = 0; i < n; i++) {
        wn_ivec3 p = vox[i];
        wn_voxel v = wn_world_get(w, p.x, p.y, p.z) & 0x00FFFFFFu;
        g[((size_t)(p.y - mn.y + 1) * sz + (size_t)(p.z - mn.z + 1)) * sx + (size_t)(p.x - mn.x + 1)] = v;
    }
    /* Remove from the world (light updates included). */
    for (uint32_t i = 0; i < n; i++) wn_edit_set(e, vox[i].x, vox[i].y, vox[i].z, 0);
    wn_debris *d = wn_debris_spawn(s, g, sx, sy, sz, wn_voxel_to_world(mn), w->mats);
    if (d) {
        /* Tipping push (e.g. the direction of the axe swing): turns a cut tree
         * over instead of letting it balance on its stump. */
        wn_vec3 up = wn_v3(0, 1, 0);
        float height = (float)(sy - 2) * WN_VOXEL_SIZE;
        if (height > 0.0f) d->angvel = wn_v3_scale(wn_v3_cross(up, s->detach_push), 1.0f / height);
        d->vel = wn_v3_scale(s->detach_push, 0.2f);
    }
    free(g);
}

int wn_debris_detach(wn_debris_set *s, wn_edit_ctx *e)
{
    wn_world *w = e->world;
    visit_set seen;
    memset(&seen, 0, sizeof seen);
    int created = 0;
    /* Only the destroyed voxels of this edit seed the search. Edits made while
     * detaching (removing islands) must not seed new searches. */
    uint32_t nt = e->ntouched;
    wn_ivec3 *seeds = (wn_ivec3 *)malloc((nt ? nt : 1) * sizeof(wn_ivec3));
    memcpy(seeds, e->touched, nt * sizeof(wn_ivec3));
    for (uint32_t t = 0; t < nt; t++) {
        for (int d = 0; d < 6; d++) {
            wn_ivec3 p = wn_iv3(seeds[t].x + k_n6[d][0], seeds[t].y + k_n6[d][1], seeds[t].z + k_n6[d][2]);
            if (!wn_world_is_solid(w, p.x, p.y, p.z)) continue;
            /* Each search uses its own visited set; a voxel already proven
             * connected is skipped through the shared set. */
            if (!vs_insert(&seen, pack(p.x, p.y, p.z))) continue;
            visit_set local;
            memset(&local, 0, sizeof local);
            wn_ivec3 *island = NULL;
            uint32_t n = 0;
            bool anchored = flood(w, p, s->max_island, &local, &island, &n);
            if (!anchored) {
                island_to_debris(s, e, island, n);
                created++;
            } else {
                /* Mark the explored region as known-connected. */
                for (uint32_t i = 0; i < local.cap; i++)
                    if (local.keys[i]) vs_insert(&seen, local.keys[i]);
            }
            free(island);
            free(local.keys);
        }
    }
    free(seeds);
    free(seen.keys);
    return created;
}

/* ---- Rigid body step ---- */

wn_mat4 wn_debris_matrix(const wn_debris *d)
{
    wn_mat4 t = wn_mat4_translate(d->pos);
    wn_mat4 r = wn_mat4_from_quat(d->rot);
    wn_mat4 s = wn_mat4_scale(wn_v3(WN_VOXEL_SIZE, WN_VOXEL_SIZE, WN_VOXEL_SIZE));
    wn_mat4 c = wn_mat4_translate(wn_v3(-d->com_local.x, -d->com_local.y, -d->com_local.z));
    return wn_mat4_mul(t, wn_mat4_mul(r, wn_mat4_mul(s, c)));
}

static bool point_in_solid(wn_world *w, wn_vec3 p, wn_ivec3 *vox)
{
    wn_ivec3 v = wn_world_to_voxel(p);
    wn_voxel cell = wn_world_get(w, v.x, v.y, v.z);
    if (!wn_mat_is_solid(w->mats, wn_vox_mat(cell))) return false;
    float top = ((float)v.y + (float)wn_vox_height16(cell) / 16.0f) * WN_VOXEL_SIZE;
    if (p.y > top) return false;
    *vox = v;
    return true;
}

/* Splits a piece in two along its longest axis. */
static void debris_split(wn_debris_set *s, wn_debris *d, const wn_materials *mats)
{
    int ix = d->sx - 2, iy = d->sy - 2, iz = d->sz - 2;
    int axis = (ix >= iy && ix >= iz) ? 0 : (iy >= iz ? 1 : 2);
    int len = axis == 0 ? ix : (axis == 1 ? iy : iz);
    if (len < 4) return;
    int cut = len / 2;
    wn_voxel *a = (wn_voxel *)calloc((size_t)d->sx * d->sy * d->sz, sizeof(wn_voxel));
    wn_voxel *b = (wn_voxel *)calloc((size_t)d->sx * d->sy * d->sz, sizeof(wn_voxel));
    for (int y = 1; y < d->sy - 1; y++)
        for (int z = 1; z < d->sz - 1; z++)
            for (int x = 1; x < d->sx - 1; x++) {
                int c = axis == 0 ? x - 1 : (axis == 1 ? y - 1 : z - 1);
                (c < cut ? a : b)[gidx(d, x, y, z)] = d->grid[gidx(d, x, y, z)];
            }
    wn_debris orig = *d;
    orig.grid = NULL;
    orig.samples = NULL;
    wn_vec3 origin_local = wn_v3_scale(orig.com_local, -1.0f); /* grid origin relative to COM */
    debris_release(d);
    wn_voxel *halves[2] = {a, b};
    for (int h = 0; h < 2; h++) {
        /* Spawn in the parent's frame: compute world position of the grid origin. */
        wn_vec3 world_origin = wn_v3_add(orig.pos, wn_v3_scale(wn_quat_rotate(orig.rot, origin_local), WN_VOXEL_SIZE));
        wn_debris *n = wn_debris_spawn(s, halves[h], orig.sx, orig.sy, orig.sz, world_origin, mats);
        if (n) {
            n->rot = orig.rot;
            n->pos = wn_v3_add(world_origin, wn_v3_scale(wn_quat_rotate(orig.rot, n->com_local), WN_VOXEL_SIZE));
            n->vel = wn_v3_scale(orig.vel, 0.5f);
            n->angvel = orig.angvel;
            n->age = orig.age;
        }
        free(halves[h]);
    }
}

/* Writes a resting piece back into the world grid (spec: Large debris settles). */
static void debris_settle(wn_debris_set *s, wn_debris *d, wn_world *w)
{
    if (d->nvoxels < s->min_voxels) {
        if (s->on_vanish) s->on_vanish(s->user, wn_world_to_voxel(d->pos), d->grid[gidx(d, 1, 1, 1)]);
        debris_release(d);
        return;
    }
    wn_edit_ctx e;
    wn_edit_begin(&e, w);
    for (int y = 1; y < d->sy - 1; y++)
        for (int z = 1; z < d->sz - 1; z++)
            for (int x = 1; x < d->sx - 1; x++) {
                wn_voxel v = d->grid[gidx(d, x, y, z)];
                if (!wn_vox_mat(v)) continue;
                wn_vec3 local = wn_v3((float)(x - 1) + 0.5f - d->com_local.x, (float)(y - 1) + 0.5f - d->com_local.y,
                                      (float)(z - 1) + 0.5f - d->com_local.z);
                wn_vec3 wp = wn_v3_add(d->pos, wn_v3_scale(wn_quat_rotate(d->rot, local), WN_VOXEL_SIZE));
                wn_ivec3 t = wn_world_to_voxel(wp);
                wn_voxel cur = wn_world_get(w, t.x, t.y, t.z);
                if (wn_mat_is_solid(w->mats, wn_vox_mat(cur)) && wn_vox_fill(cur) == 0) continue;
                wn_edit_set(&e, t.x, t.y, t.z, wn_vox_set_fill(v, 0));
            }
    wn_edit_end(&e);
    debris_release(d);
}

void wn_debris_step(wn_debris_set *s, wn_world *w, float dt)
{
    const float g = -9.81f;
    for (int i = 0; i < WN_MAX_DEBRIS; i++) {
        wn_debris *d = &s->d[i];
        if (!d->active) continue;
        d->age += dt;
        d->vel.y += g * dt;
        /* Air drag keeps things calm. */
        d->vel = wn_v3_scale(d->vel, 0.998f);
        d->angvel = wn_v3_scale(d->angvel, 0.99f);
        d->pos = wn_v3_add(d->pos, wn_v3_scale(d->vel, dt));
        d->rot = wn_quat_integrate(d->rot, d->angvel, dt);
        /* Contacts: gather first, then share the impulse between them. */
        float max_pen = 0.0f;
        float impact = 0.0f;
        int contacts = 0;
        enum { MAXC = 64 };
        wn_vec3 cr[MAXC], cn[MAXC];
        for (uint32_t k = 0; k < d->nsamples; k++) {
            wn_vec3 r = wn_v3_scale(wn_quat_rotate(d->rot, d->samples[k]), WN_VOXEL_SIZE);
            wn_vec3 p = wn_v3_add(d->pos, r);
            wn_ivec3 v;
            if (!point_in_solid(w, p, &v)) continue;
            /* Contact normal: up if free above, otherwise the freest axis. */
            wn_vec3 n = wn_v3(0, 1, 0);
            if (wn_world_is_solid(w, v.x, v.y + 1, v.z)) {
                static const int dirs[4][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
                for (int k2 = 0; k2 < 4; k2++)
                    if (!wn_world_is_solid(w, v.x + dirs[k2][0], v.y, v.z + dirs[k2][2])) {
                        n = wn_v3((float)dirs[k2][0], 0, (float)dirs[k2][2]);
                        break;
                    }
            }
            wn_voxel cell = wn_world_get(w, v.x, v.y, v.z);
            float top = ((float)v.y + (float)wn_vox_height16(cell) / 16.0f) * WN_VOXEL_SIZE;
            float pen = n.y > 0.5f ? top - p.y : WN_VOXEL_SIZE * 0.5f;
            if (pen > max_pen) max_pen = pen;
            if (contacts < MAXC) {
                cr[contacts] = r;
                cn[contacts] = n;
            }
            contacts++;
        }
        int nc = WN_MIN(contacts, (int)MAXC);
        for (int k = 0; k < nc; k++) {
            wn_vec3 r = cr[k], n = cn[k];
            wn_vec3 vp = wn_v3_add(d->vel, wn_v3_cross(d->angvel, r));
            float vn = wn_v3_dot(vp, n);
            if (vn >= 0.0f) continue;
            if (-vn > impact) impact = -vn;
            wn_vec3 rn = wn_v3_cross(r, n);
            float denom = d->inv_mass + wn_v3_dot(rn, rn) * d->inv_inertia;
            float j = -(1.0f + 0.15f) * vn / denom / (float)nc;
            wn_vec3 imp = wn_v3_scale(n, j);
            /* Friction. */
            wn_vec3 vt = wn_v3_sub(vp, wn_v3_scale(n, vn));
            float vtl = wn_v3_len(vt);
            if (vtl > 1e-4f) {
                float jt = WN_MIN(0.6f * j, vtl / denom / (float)nc);
                imp = wn_v3_sub(imp, wn_v3_scale(vt, jt / vtl));
            }
            d->vel = wn_v3_add(d->vel, wn_v3_scale(imp, d->inv_mass));
            d->angvel = wn_v3_add(d->angvel, wn_v3_scale(wn_v3_cross(r, imp), d->inv_inertia));
        }
        if (max_pen > 0.0f) d->pos.y += WN_MIN(max_pen, WN_VOXEL_SIZE) * 0.8f;
        if (contacts > 0) {
            /* Rolling resistance: resting contact quickly kills spin. */
            d->angvel = wn_v3_scale(d->angvel, 0.85f);
        }
        float spin = wn_v3_len(d->angvel);
        if (spin > 8.0f) d->angvel = wn_v3_scale(d->angvel, 8.0f / spin);
        if (contacts > 0 && impact > s->break_speed && d->nvoxels >= s->min_voxels * 4) {
            debris_split(s, d, w->mats);
            continue;
        }
        /* Sleeping / settling. */
        float sp = wn_v3_len(d->vel) + wn_v3_len(d->angvel) * 0.1f;
        if (contacts > 0 && sp < 0.3f) d->rest_time += dt;
        else d->rest_time = 0.0f;
        if (d->rest_time > 0.5f || d->age > 20.0f || d->pos.y < (float)(w->y_min * WN_CHUNK_SIZE) * WN_VOXEL_SIZE) {
            if (d->pos.y < (float)(w->y_min * WN_CHUNK_SIZE) * WN_VOXEL_SIZE) {
                debris_release(d);
            } else {
                debris_settle(s, d, w);
            }
        }
    }
}

uint32_t wn_debris_checksum(const wn_debris_set *s)
{
    wn_hasher h;
    wn_hasher_init(&h, 11u);
    for (int i = 0; i < WN_MAX_DEBRIS; i++) {
        const wn_debris *d = &s->d[i];
        if (!d->active) continue;
        wn_hasher_add(&h, &d->id, sizeof d->id);
        wn_hasher_add(&h, &d->pos, sizeof d->pos);
        wn_hasher_add(&h, &d->rot, sizeof d->rot);
        wn_hasher_add(&h, &d->nvoxels, sizeof d->nvoxels);
    }
    return wn_hasher_finish(&h);
}
