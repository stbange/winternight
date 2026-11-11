#include "wn/wn_voxel.h"

#include "wn/wn_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- Materials ---------------- */

static const char *k_builtin_names[WN_MAT_BUILTIN_COUNT] = {
    "air",   "bedrock",      "earth",        "stone",        "snow",        "ice",  "water",
    "blood", "frozen-blood", "pine-wood",    "pine-needles", "castle-stone", "window-glow", "fire"};

const char *wn_material_builtin_name(int id)
{
    return (id >= 0 && id < WN_MAT_BUILTIN_COUNT) ? k_builtin_names[id] : NULL;
}

int wn_materials_find(const wn_materials *mats, const char *name)
{
    for (int i = 0; i < mats->count; i++)
        if (mats->m[i].defined && strcmp(mats->m[i].name, name) == 0) return i;
    return -1;
}

static bool parse_kind(const char *s, wn_mat_kind *k)
{
    if (strcmp(s, "solid") == 0) *k = WN_KIND_SOLID;
    else if (strcmp(s, "granular") == 0) *k = WN_KIND_GRANULAR;
    else if (strcmp(s, "liquid") == 0) *k = WN_KIND_LIQUID;
    else if (strcmp(s, "gas") == 0) *k = WN_KIND_GAS;
    else return false;
    return true;
}

bool wn_materials_load(wn_materials *out, const char *text, char *err, size_t errlen)
{
    wn_materials *t = (wn_materials *)calloc(1, sizeof *t);
    /* Air always exists. */
    snprintf(t->m[0].name, sizeof t->m[0].name, "air");
    t->m[0].kind = WN_KIND_GAS;
    t->m[0].defined = true;
    int next_free = WN_MAT_BUILTIN_COUNT;
    t->count = 1;

    wn_parser p;
    wn_parser_init(&p, text, strlen(text));
    while (wn_parser_next(&p)) {
        if (!wn_parser_is(&p, 0, "material")) {
            wn_parser_fail(&p, "expected 'material'");
            break;
        }
        if (!wn_parser_expect_fields(&p, 8)) break;
        const char *name = p.fields[1];
        if (strlen(name) == 0 || strlen(name) >= sizeof t->m[0].name) {
            wn_parser_fail(&p, "missing or too long material name");
            break;
        }
        int id = -1;
        for (int b = 0; b < WN_MAT_BUILTIN_COUNT; b++)
            if (strcmp(k_builtin_names[b], name) == 0) id = b;
        if (id < 0) {
            if (next_free >= WN_MAX_MATERIALS) {
                wn_parser_fail(&p, "too many materials");
                break;
            }
            id = next_free++;
        }
        if (id != 0 && t->m[id].defined) {
            wn_parser_fail(&p, "material defined twice");
            break;
        }
        wn_material *m = &t->m[id];
        snprintf(m->name, sizeof m->name, "%s", name);
        if (!parse_kind(p.fields[2], &m->kind)) {
            wn_parser_fail(&p, "unknown material kind");
            break;
        }
        int32_t emit;
        if (!wn_parser_float(&p, 3, &m->hardness) || !wn_parser_float(&p, 4, &m->density) ||
            !wn_parser_int(&p, 5, &emit))
            break;
        if (m->hardness < 0.0f || m->density < 0.0f || emit < 0 || emit > 15) {
            char msg[96];
            snprintf(msg, sizeof msg, "material '%s' has a negative hardness/density or bad emit", name);
            wn_parser_fail(&p, msg);
            break;
        }
        m->emit = (uint8_t)emit;
        for (const char *f = p.fields[6]; *f; f++) {
            if (*f == 'f') m->flammable = true;
            else if (*f == 'a') m->anchor = true;
            else if (*f != '-') {
                wn_parser_fail(&p, "unknown material flag");
                break;
            }
        }
        if (p.error[0]) break;
        m->ramp_n = 0;
        for (int i = 7; i < p.nfields && m->ramp_n < WN_RAMP_MAX; i++)
            if (!wn_parser_color(&p, i, &m->ramp[m->ramp_n++])) break;
        if (p.error[0]) break;
        m->defined = true;
        if (id + 1 > t->count) t->count = id + 1;
    }
    if (!p.error[0]) {
        for (int b = 1; b < WN_MAT_BUILTIN_COUNT; b++) {
            if (!t->m[b].defined) {
                snprintf(p.error, sizeof p.error, "built-in material '%s' is not defined", k_builtin_names[b]);
                break;
            }
        }
    }
    if (p.error[0]) {
        if (err) snprintf(err, errlen, "%s", p.error);
        free(t);
        return false;
    }
    *out = *t;
    free(t);
    return true;
}

/* ---------------- Chunks ---------------- */

wn_chunk *wn_chunk_new(wn_ivec3 pos, wn_voxel fill)
{
    wn_chunk *c = (wn_chunk *)calloc(1, sizeof *c);
    c->pos = pos;
    c->uniform = fill;
    c->flags = WN_CHUNK_DIRTY_MESH;
    return c;
}

void wn_chunk_free(wn_chunk *c)
{
    if (!c) return;
    free(c->vox);
    free(c);
}

void wn_chunk_expand(wn_chunk *c)
{
    if (c->vox) return;
    c->vox = (wn_voxel *)malloc(WN_CHUNK_VOLUME * sizeof(wn_voxel));
    for (int i = 0; i < WN_CHUNK_VOLUME; i++) c->vox[i] = c->uniform;
}

void wn_chunk_set(wn_chunk *c, int x, int y, int z, wn_voxel v)
{
    if (!c->vox) {
        if (v == c->uniform) return;
        wn_chunk_expand(c);
    }
    c->vox[wn_chunk_index(x, y, z)] = v;
}

bool wn_chunk_is_uniform(const wn_chunk *c) { return c->vox == NULL; }

bool wn_chunk_compact(wn_chunk *c)
{
    if (!c->vox) return false;
    wn_voxel v0 = c->vox[0];
    for (int i = 1; i < WN_CHUNK_VOLUME; i++)
        if (c->vox[i] != v0) return false;
    free(c->vox);
    c->vox = NULL;
    c->uniform = v0;
    return true;
}

/* ---------------- World ---------------- */

static uint32_t chunk_hash(wn_ivec3 p)
{
    return (uint32_t)p.x * 73856093u ^ (uint32_t)p.y * 19349663u ^ (uint32_t)p.z * 83492791u;
}

void wn_world_init(wn_world *w, const wn_materials *mats, int y_min, int y_max)
{
    memset(w, 0, sizeof *w);
    w->nbuckets = 4096;
    w->buckets = (wn_chunk **)calloc(w->nbuckets, sizeof(wn_chunk *));
    w->saved = (wn_chunk **)calloc(w->nbuckets, sizeof(wn_chunk *));
    w->mats = mats;
    w->y_min = y_min;
    w->y_max = y_max;
}

static void free_table(wn_chunk **t, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        wn_chunk *c = t[i];
        while (c) {
            wn_chunk *nx = c->next;
            wn_chunk_free(c);
            c = nx;
        }
    }
    free(t);
}

void wn_world_free(wn_world *w)
{
    free_table(w->buckets, w->nbuckets);
    free_table(w->saved, w->nbuckets);
    memset(w, 0, sizeof *w);
}

void wn_world_set_generator(wn_world *w, wn_chunk_gen_fn gen, void *user)
{
    w->gen = gen;
    w->gen_user = user;
}

wn_ivec3 wn_world_to_voxel(wn_vec3 p)
{
    const float inv = 1.0f / WN_VOXEL_SIZE;
    return wn_iv3(wn_floorf_i(p.x * inv), wn_floorf_i(p.y * inv), wn_floorf_i(p.z * inv));
}

wn_vec3 wn_voxel_to_world(wn_ivec3 v)
{
    return wn_v3((float)v.x * WN_VOXEL_SIZE, (float)v.y * WN_VOXEL_SIZE, (float)v.z * WN_VOXEL_SIZE);
}

wn_vec3 wn_voxel_center(wn_ivec3 v)
{
    return wn_v3(((float)v.x + 0.5f) * WN_VOXEL_SIZE, ((float)v.y + 0.5f) * WN_VOXEL_SIZE,
                 ((float)v.z + 0.5f) * WN_VOXEL_SIZE);
}

static wn_chunk **find_slot(wn_chunk **table, uint32_t nb, wn_ivec3 p)
{
    wn_chunk **s = &table[chunk_hash(p) & (nb - 1)];
    while (*s && ((*s)->pos.x != p.x || (*s)->pos.y != p.y || (*s)->pos.z != p.z)) s = &(*s)->next;
    return s;
}

wn_chunk *wn_world_chunk(wn_world *w, wn_ivec3 p)
{
    wn_chunk *c = w->cache;
    if (c && c->pos.x == p.x && c->pos.y == p.y && c->pos.z == p.z) return c;
    c = *find_slot(w->buckets, w->nbuckets, p);
    if (c) w->cache = c;
    return c;
}

wn_chunk *wn_world_load_chunk(wn_world *w, wn_ivec3 p)
{
    wn_chunk **slot = find_slot(w->buckets, w->nbuckets, p);
    if (*slot) return *slot;
    wn_chunk **sv = find_slot(w->saved, w->nbuckets, p);
    wn_chunk *c;
    if (*sv) {
        c = *sv;
        *sv = c->next;
        w->nsaved--;
        c->next = NULL;
        c->flags |= WN_CHUNK_DIRTY_MESH;
        c->mesh_rev++;
    } else {
        c = wn_chunk_new(p, wn_vox_set_sky(0, 15));
        if (w->gen) w->gen(w->gen_user, c);
    }
    *slot = c;
    w->nchunks++;
    return c;
}

void wn_world_unload_chunk(wn_world *w, wn_ivec3 p)
{
    wn_chunk **slot = find_slot(w->buckets, w->nbuckets, p);
    wn_chunk *c = *slot;
    if (!c) return;
    *slot = c->next;
    c->next = NULL;
    w->nchunks--;
    if (w->cache == c) w->cache = NULL;
    if (c->flags & WN_CHUNK_MODIFIED) {
        wn_chunk **sv = find_slot(w->saved, w->nbuckets, p);
        *sv = c;
        w->nsaved++;
    } else {
        wn_chunk_free(c);
    }
}

bool wn_world_column_loaded(wn_world *w, int cx, int cz)
{
    return wn_world_chunk(w, wn_iv3(cx, w->y_max, cz)) != NULL;
}

void wn_world_load_column(wn_world *w, int cx, int cz)
{
    if (wn_world_column_loaded(w, cx, cz)) return;
    for (int cy = w->y_min; cy <= w->y_max; cy++) wn_world_load_chunk(w, wn_iv3(cx, cy, cz));
    if (w->on_column) w->on_column(w->column_user, w, cx, cz);
}

wn_voxel wn_world_get(wn_world *w, int32_t x, int32_t y, int32_t z)
{
    wn_chunk *c = wn_world_chunk(w, wn_iv3(x >> WN_CHUNK_BITS, y >> WN_CHUNK_BITS, z >> WN_CHUNK_BITS));
    if (!c) return wn_vox_set_sky(0, 15);
    return wn_chunk_get(c, x & WN_CHUNK_MASK, y & WN_CHUNK_MASK, z & WN_CHUNK_MASK);
}

static void mark_dirty(wn_world *w, wn_chunk *c, int lx, int ly, int lz, int32_t x, int32_t y, int32_t z)
{
    c->flags |= WN_CHUNK_DIRTY_MESH;
    c->mesh_rev++;
    /* Faces and AO of neighbours touch this voxel when it lies on a border. */
    int dxs[2] = {0, 0}, dys[2] = {0, 0}, dzs[2] = {0, 0};
    if (lx == 0) dxs[0] = -1;
    if (lx == WN_CHUNK_MASK) dxs[1] = 1;
    if (ly == 0) dys[0] = -1;
    if (ly == WN_CHUNK_MASK) dys[1] = 1;
    if (lz == 0) dzs[0] = -1;
    if (lz == WN_CHUNK_MASK) dzs[1] = 1;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                int dx = i ? dxs[1] : dxs[0], dy = j ? dys[1] : dys[0], dz = k ? dzs[1] : dzs[0];
                if (!dx && !dy && !dz) continue;
                wn_chunk *n = wn_world_chunk(w, wn_iv3((x >> WN_CHUNK_BITS) + dx, (y >> WN_CHUNK_BITS) + dy,
                                                       (z >> WN_CHUNK_BITS) + dz));
                if (n && n != c) {
                    n->flags |= WN_CHUNK_DIRTY_MESH;
                    n->mesh_rev++;
                }
            }
        }
    }
}

static void world_set(wn_world *w, int32_t x, int32_t y, int32_t z, wn_voxel v, bool modified)
{
    wn_chunk *c = wn_world_chunk(w, wn_iv3(x >> WN_CHUNK_BITS, y >> WN_CHUNK_BITS, z >> WN_CHUNK_BITS));
    if (!c) return;
    int lx = x & WN_CHUNK_MASK, ly = y & WN_CHUNK_MASK, lz = z & WN_CHUNK_MASK;
    wn_voxel old = wn_chunk_get(c, lx, ly, lz);
    /* Light belongs to the lighting system: gameplay writes keep the old light
     * so wn_light_voxel_changed can remove it correctly. */
    if (modified) v = (v & 0x00FFFFFFu) | (old & 0xFF000000u);
    if (old == v) return;
    wn_chunk_set(c, lx, ly, lz, v);
    if (modified) {
        c->flags |= WN_CHUNK_MODIFIED;
        w->edit_serial++;
    }
    mark_dirty(w, c, lx, ly, lz, x, y, z);
}

void wn_world_set(wn_world *w, int32_t x, int32_t y, int32_t z, wn_voxel v) { world_set(w, x, y, z, v, true); }
void wn_world_set_quiet(wn_world *w, int32_t x, int32_t y, int32_t z, wn_voxel v) { world_set(w, x, y, z, v, false); }

bool wn_world_is_solid(wn_world *w, int32_t x, int32_t y, int32_t z)
{
    return wn_mat_is_solid(w->mats, wn_vox_mat(wn_world_get(w, x, y, z)));
}

/* ---- Streaming ---- */

int wn_world_stream(wn_world *w, wn_vec3 viewer, int radius, int max_loads)
{
    wn_ivec3 vv = wn_world_to_voxel(viewer);
    int pcx = vv.x >> WN_CHUNK_BITS, pcz = vv.z >> WN_CHUNK_BITS;
    /* Unload columns outside radius + 1 (hysteresis). */
    int keep = radius + 1;
    for (uint32_t b = 0; b < w->nbuckets; b++) {
        wn_chunk *c = w->buckets[b];
        while (c) {
            wn_chunk *nx = c->next;
            int dx = c->pos.x - pcx, dz = c->pos.z - pcz;
            if (dx < -keep || dx > keep || dz < -keep || dz > keep) wn_world_unload_chunk(w, c->pos);
            c = nx;
        }
    }
    /* Load missing columns nearest first (rings of growing distance). */
    int loaded = 0;
    for (int ring = 0; ring <= radius && loaded < max_loads; ring++) {
        for (int dz = -ring; dz <= ring && loaded < max_loads; dz++) {
            for (int dx = -ring; dx <= ring && loaded < max_loads; dx++) {
                if (WN_MAX(abs(dx), abs(dz)) != ring) continue;
                if (wn_world_column_loaded(w, pcx + dx, pcz + dz)) continue;
                wn_world_load_column(w, pcx + dx, pcz + dz);
                loaded++;
            }
        }
    }
    return loaded;
}

/* ---- Checksum ---- */

typedef struct sum_entry {
    wn_ivec3 pos;
    uint32_t hash;
} sum_entry;

static int cmp_entry(const void *a, const void *b)
{
    const sum_entry *x = (const sum_entry *)a, *y = (const sum_entry *)b;
    if (x->pos.x != y->pos.x) return x->pos.x < y->pos.x ? -1 : 1;
    if (x->pos.y != y->pos.y) return x->pos.y < y->pos.y ? -1 : 1;
    if (x->pos.z != y->pos.z) return x->pos.z < y->pos.z ? -1 : 1;
    return 0;
}

static uint32_t chunk_shape_hash(const wn_chunk *c)
{
    const uint32_t mask = 0x00FFFFFFu; /* drop light bits */
    if (!c->vox) {
        uint32_t v = c->uniform & mask;
        return wn_hash32(&v, sizeof v, 1u) ^ 0xA5A5A5A5u;
    }
    wn_hasher h;
    wn_hasher_init(&h, 1u);
    uint32_t buf[WN_CHUNK_SIZE];
    for (int i = 0; i < WN_CHUNK_VOLUME; i += WN_CHUNK_SIZE) {
        for (int j = 0; j < WN_CHUNK_SIZE; j++) buf[j] = c->vox[i + j] & mask;
        wn_hasher_add(&h, buf, sizeof buf);
    }
    return wn_hasher_finish(&h);
}

/* A uniform chunk and an expanded chunk with identical content must hash the
 * same, so expanded chunks that happen to be uniform are hashed as uniform. */
static uint32_t chunk_hash_canonical(const wn_chunk *c)
{
    if (c->vox) {
        uint32_t v0 = c->vox[0] & 0x00FFFFFFu;
        bool same = true;
        for (int i = 1; i < WN_CHUNK_VOLUME && same; i++) same = (c->vox[i] & 0x00FFFFFFu) == v0;
        if (same) return wn_hash32(&v0, sizeof v0, 1u) ^ 0xA5A5A5A5u;
    }
    return chunk_shape_hash(c);
}

uint32_t wn_world_checksum(wn_world *w)
{
    uint32_t n = w->nchunks + w->nsaved;
    sum_entry *e = (sum_entry *)malloc((n ? n : 1) * sizeof *e);
    uint32_t k = 0;
    wn_chunk **tables[2] = {w->buckets, w->saved};
    for (int t = 0; t < 2; t++) {
        for (uint32_t b = 0; b < w->nbuckets; b++) {
            for (wn_chunk *c = tables[t][b]; c; c = c->next) {
                e[k].pos = c->pos;
                e[k].hash = chunk_hash_canonical(c);
                k++;
            }
        }
    }
    qsort(e, k, sizeof *e, cmp_entry);
    uint32_t h = wn_hash32(e, k * sizeof *e, 7u);
    free(e);
    return h;
}

void wn_world_foreach_chunk(wn_world *w, void (*fn)(void *user, wn_chunk *c), void *user)
{
    for (uint32_t b = 0; b < w->nbuckets; b++) {
        wn_chunk *c = w->buckets[b];
        while (c) {
            wn_chunk *nx = c->next;
            fn(user, c);
            c = nx;
        }
    }
}

/* ---- Queries ---- */

static bool voxel_blocks_point(wn_world *w, wn_ivec3 v, float local_y)
{
    wn_voxel vox = wn_world_get(w, v.x, v.y, v.z);
    if (!wn_mat_is_solid(w->mats, wn_vox_mat(vox))) return false;
    return local_y <= (float)wn_vox_height16(vox) / 16.0f;
}

wn_ray_hit wn_world_raycast(wn_world *w, wn_vec3 o, wn_vec3 d, float max_dist)
{
    wn_ray_hit r;
    memset(&r, 0, sizeof r);
    d = wn_v3_norm(d);
    const float inv = 1.0f / WN_VOXEL_SIZE;
    /* Work in voxel units. */
    float ox = o.x * inv, oy = o.y * inv, oz = o.z * inv;
    float maxv = max_dist * inv;
    wn_ivec3 v = wn_iv3(wn_floorf_i(ox), wn_floorf_i(oy), wn_floorf_i(oz));
    int sx = d.x > 0 ? 1 : -1, sy = d.y > 0 ? 1 : -1, sz = d.z > 0 ? 1 : -1;
    float tdx = d.x != 0.0f ? wn_absf(1.0f / d.x) : 1e30f;
    float tdy = d.y != 0.0f ? wn_absf(1.0f / d.y) : 1e30f;
    float tdz = d.z != 0.0f ? wn_absf(1.0f / d.z) : 1e30f;
    float tmx = d.x != 0.0f ? ((sx > 0 ? (float)(v.x + 1) - ox : ox - (float)v.x) * tdx) : 1e30f;
    float tmy = d.y != 0.0f ? ((sy > 0 ? (float)(v.y + 1) - oy : oy - (float)v.y) * tdy) : 1e30f;
    float tmz = d.z != 0.0f ? ((sz > 0 ? (float)(v.z + 1) - oz : oz - (float)v.z) * tdz) : 1e30f;
    wn_ivec3 n = wn_iv3(0, 0, 0);
    float t = 0.0f;
    while (t <= maxv) {
        wn_voxel vox = wn_world_get(w, v.x, v.y, v.z);
        if (wn_mat_is_solid(w->mats, wn_vox_mat(vox))) {
            float h = (float)wn_vox_height16(vox) / 16.0f;
            float ly = oy + d.y * t - (float)v.y;
            bool hit = h >= 1.0f || ly <= h + 1e-4f;
            if (!hit && d.y < 0.0f) {
                /* Ray enters above a partial voxel: find where it reaches the top. */
                float th = ((float)v.y + h - oy) / d.y;
                float tx = ox + d.x * th - (float)v.x, tz = oz + d.z * th - (float)v.z;
                if (th <= maxv && tx >= 0.0f && tx <= 1.0f && tz >= 0.0f && tz <= 1.0f) {
                    hit = true;
                    t = th;
                    n = wn_iv3(0, 1, 0);
                }
            }
            if (hit && voxel_blocks_point(w, v, WN_CLAMP(oy + d.y * t - (float)v.y, 0.0f, 1.0f))) {
                r.hit = true;
                r.voxel = v;
                r.normal = n;
                r.distance = t * WN_VOXEL_SIZE;
                return r;
            }
        }
        if (tmx < tmy && tmx < tmz) {
            v.x += sx;
            t = tmx;
            tmx += tdx;
            n = wn_iv3(-sx, 0, 0);
        } else if (tmy < tmz) {
            v.y += sy;
            t = tmy;
            tmy += tdy;
            n = wn_iv3(0, -sy, 0);
        } else {
            v.z += sz;
            t = tmz;
            tmz += tdz;
            n = wn_iv3(0, 0, -sz);
        }
    }
    return r;
}

bool wn_world_box_overlaps(wn_world *w, wn_vec3 mn, wn_vec3 mx)
{
    wn_ivec3 a = wn_world_to_voxel(mn), b = wn_world_to_voxel(mx);
    for (int32_t y = a.y; y <= b.y; y++) {
        for (int32_t z = a.z; z <= b.z; z++) {
            for (int32_t x = a.x; x <= b.x; x++) {
                wn_voxel v = wn_world_get(w, x, y, z);
                if (!wn_mat_is_solid(w->mats, wn_vox_mat(v))) continue;
                float top = ((float)y + (float)wn_vox_height16(v) / 16.0f) * WN_VOXEL_SIZE;
                if (mn.y < top) return true;
            }
        }
    }
    return false;
}
