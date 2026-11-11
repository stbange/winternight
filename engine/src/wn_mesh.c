#include "wn/wn_mesh.h"

#include "wn/wn_core.h"

#include <stdlib.h>
#include <string.h>

void wn_mesh_init(wn_mesh *m) { memset(m, 0, sizeof *m); }

void wn_mesh_free(wn_mesh *m)
{
    free(m->verts);
    free(m->indices);
    memset(m, 0, sizeof *m);
}

void wn_mesh_clear(wn_mesh *m)
{
    m->nverts = 0;
    m->nindices = 0;
}

uint16_t wn_rgb_to_565(uint32_t rgb)
{
    uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint32_t wn_565_to_rgb(uint16_t c)
{
    uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return (r << 16) | (g << 8) | b;
}

static uint32_t mix_rgb(uint32_t a, uint32_t b, uint32_t t256)
{
    uint32_t out = 0;
    for (int s = 0; s < 24; s += 8) {
        uint32_t ca = (a >> s) & 0xFF, cb = (b >> s) & 0xFF;
        uint32_t c = (ca * (256 - t256) + cb * t256) >> 8;
        out |= (c & 0xFF) << s;
    }
    return out;
}

uint32_t wn_voxel_color(const wn_materials *mats, wn_voxel v, int32_t x, int32_t y, int32_t z)
{
    const wn_material *m = &mats->m[wn_vox_mat(v)];
    uint32_t base = 0xFF00FF; /* undefined material: magenta */
    if (m->ramp_n > 0) {
        uint32_t h = wn_hash_ivec3(x, y, z, 0x5EEDu) & 15u;
        int idx = h < 10 ? 0 : (h < 14 ? 1 : 2);
        if (idx >= m->ramp_n) idx = m->ramp_n - 1;
        base = m->ramp[idx];
    }
    uint32_t dmg = wn_vox_damage(v);
    if (dmg) {
        if (wn_vox_mat(v) == WN_MAT_SNOW) {
            base = mix_rgb(base, 0x6B7CAB, 40u + dmg * 8u); /* trampled snow */
        } else {
            base = mix_rgb(base, 0x000000, dmg * 9u);
        }
    }
    uint32_t st = wn_vox_stain(v), amt = wn_vox_stain_amount(v);
    if (st != WN_STAIN_NONE && amt) {
        static const uint32_t stain_rgb[4] = {0, 0xA8141A, 0x4A0A0C, 0x1E2A22};
        uint32_t t = 60u + amt * 3u; /* 63 -> ~250 */
        if (t > 250u) t = 250u;
        base = mix_rgb(base, stain_rgb[st], t);
    }
    return base;
}

/* ---------------- Mesh building ---------------- */

static void reserve(wn_mesh *m, uint32_t nv, uint32_t ni)
{
    if (m->nverts + nv > m->vcap) {
        m->vcap = WN_MAX(m->vcap * 2, m->nverts + nv + 1024);
        m->verts = (wn_vertex *)realloc(m->verts, m->vcap * sizeof *m->verts);
    }
    if (m->nindices + ni > m->icap) {
        m->icap = WN_MAX(m->icap * 2, m->nindices + ni + 1536);
        m->indices = (uint32_t *)realloc(m->indices, m->icap * sizeof *m->indices);
    }
}

/* Axis setup per face direction: d = axis of the normal, u/v the face plane. */
static const int k_axis[6][3] = {
    /* d, u, v */
    {0, 2, 1}, {0, 2, 1}, {1, 0, 2}, {1, 0, 2}, {2, 0, 1}, {2, 0, 1}};
static const int k_sign[6] = {1, -1, 1, -1, 1, -1};

typedef struct grid_ctx {
    const wn_voxel *g;
    int sx, sy, sz;
    const wn_materials *mats;
    const wn_mesh_opts *opts;
} grid_ctx;

static inline wn_voxel cell(const grid_ctx *c, int x, int y, int z)
{
    return c->g[((size_t)y * (size_t)c->sz + (size_t)z) * (size_t)c->sx + (size_t)x];
}

static inline bool visible(const grid_ctx *c, wn_voxel v) { return c->mats->m[wn_vox_mat(v)].kind != WN_KIND_GAS; }
static inline bool occludes_ao(const grid_ctx *c, wn_voxel v) { return wn_mat_is_opaque(c->mats, wn_vox_mat(v)); }

/* Does neighbour nv fully cover the face of a full voxel pointing in dir? */
static bool covers(const grid_ctx *c, wn_voxel nv, int dir)
{
    if (!visible(c, nv)) return false;
    if (wn_vox_fill(nv) == 0) return true;
    return dir == 2; /* a partial voxel above still sits on our top face */
}

static void emit_quad(wn_mesh *m, const uint16_t p[4][3], uint16_t color, int normal, const uint8_t ao[4], uint8_t light,
                      uint8_t flags)
{
    reserve(m, 4, 6);
    uint32_t base = m->nverts;
    for (int i = 0; i < 4; i++) {
        wn_vertex *v = &m->verts[m->nverts++];
        v->x = p[i][0];
        v->y = p[i][1];
        v->z = p[i][2];
        v->color = color;
        v->nao = (uint8_t)(normal | (ao[i] << 3));
        v->light = light;
        v->flags = flags;
        v->pad = 0;
    }
    /* Flip the diagonal so AO interpolates without the anisotropy artefact. */
    static const uint32_t a[6] = {0, 1, 2, 0, 2, 3}, b[6] = {1, 2, 3, 1, 3, 0};
    const uint32_t *ord = (ao[0] + ao[2] >= ao[1] + ao[3]) ? a : b;
    for (int i = 0; i < 6; i++) m->indices[m->nindices++] = base + ord[i];
}

static uint8_t vertex_ao(bool s1, bool s2, bool corner)
{
    if (s1 && s2) return 0;
    return (uint8_t)(3 - (int)s1 - (int)s2 - (int)corner);
}

/* AO for the 4 corners of a face of voxel (x,y,z) in direction dir.
 * Corner order: (u0,v0) (u1,v0) (u1,v1) (u0,v1). */
static void face_ao(const grid_ctx *c, int x, int y, int z, int dir, uint8_t ao[4])
{
    int d = k_axis[dir][0], u = k_axis[dir][1], v = k_axis[dir][2];
    int p[3] = {x, y, z};
    p[d] += k_sign[dir];
    static const int cu[4] = {-1, 1, 1, -1}, cv[4] = {-1, -1, 1, 1};
    for (int i = 0; i < 4; i++) {
        int a[3] = {p[0], p[1], p[2]}, b[3] = {p[0], p[1], p[2]}, k[3] = {p[0], p[1], p[2]};
        a[u] += cu[i];
        b[v] += cv[i];
        k[u] += cu[i];
        k[v] += cv[i];
        bool s1 = occludes_ao(c, cell(c, a[0], a[1], a[2]));
        bool s2 = occludes_ao(c, cell(c, b[0], b[1], b[2]));
        bool cc = occludes_ao(c, cell(c, k[0], k[1], k[2]));
        ao[i] = vertex_ao(s1, s2, cc);
    }
}

static uint8_t face_light(const grid_ctx *c, wn_voxel front)
{
    if (!c->opts->use_light) return c->opts->full_light;
    return (uint8_t)((wn_vox_sky(front) << 4) | wn_vox_block(front));
}

static uint16_t cell_color(const grid_ctx *c, wn_voxel v, int x, int y, int z)
{
    int sh = c->opts->scale_shift;
    int32_t wx = c->opts->hash_origin.x + ((x - 1) << sh);
    int32_t wy = c->opts->hash_origin.y + ((y - 1) << sh);
    int32_t wz = c->opts->hash_origin.z + ((z - 1) << sh);
    return wn_rgb_to_565(wn_voxel_color(c->mats, v, wx, wy, wz));
}

/* Key packing for greedy merging: 0 = no face. */
static uint64_t face_key(uint16_t color, const uint8_t ao[4], uint8_t light, uint8_t flags)
{
    uint64_t k = 1ull << 63;
    k |= (uint64_t)color;
    k |= (uint64_t)(ao[0] | ao[1] << 2 | ao[2] << 4 | ao[3] << 6) << 16;
    k |= (uint64_t)light << 24;
    k |= (uint64_t)flags << 32;
    return k;
}

static void greedy_full(wn_mesh *out, const grid_ctx *c)
{
    int dims[3] = {c->sx - 2, c->sy - 2, c->sz - 2};
    int maxuv = WN_MAX(WN_MAX(dims[0], dims[1]), dims[2]);
    uint64_t *mask = (uint64_t *)malloc((size_t)maxuv * (size_t)maxuv * sizeof(uint64_t));
    for (int dir = 0; dir < 6; dir++) {
        int d = k_axis[dir][0], u = k_axis[dir][1], v = k_axis[dir][2];
        int nu = dims[u], nv = dims[v];
        for (int s = 0; s < dims[d]; s++) {
            /* Build mask for this slice. */
            for (int j = 0; j < nv; j++) {
                for (int i = 0; i < nu; i++) {
                    int p[3];
                    p[d] = s + 1;
                    p[u] = i + 1;
                    p[v] = j + 1;
                    wn_voxel vox = cell(c, p[0], p[1], p[2]);
                    uint64_t key = 0;
                    if (visible(c, vox) && wn_vox_fill(vox) == 0) {
                        int q[3] = {p[0], p[1], p[2]};
                        q[d] += k_sign[dir];
                        wn_voxel nb = cell(c, q[0], q[1], q[2]);
                        if (!covers(c, nb, dir)) {
                            uint8_t ao[4];
                            face_ao(c, p[0], p[1], p[2], dir, ao);
                            uint8_t fl = c->mats->m[wn_vox_mat(vox)].emit ? WN_VFLAG_EMISSIVE : 0;
                            key = face_key(cell_color(c, vox, p[0], p[1], p[2]), ao, face_light(c, nb), fl);
                        }
                    }
                    mask[j * nu + i] = key;
                }
            }
            /* Greedy merge. */
            for (int j = 0; j < nv; j++) {
                for (int i = 0; i < nu;) {
                    uint64_t key = mask[j * nu + i];
                    if (!key) {
                        i++;
                        continue;
                    }
                    int w = 1;
                    while (i + w < nu && mask[j * nu + i + w] == key) w++;
                    int h = 1;
                    for (; j + h < nv; h++) {
                        int k = 0;
                        while (k < w && mask[(j + h) * nu + i + k] == key) k++;
                        if (k < w) break;
                    }
                    for (int hh = 0; hh < h; hh++)
                        for (int ww = 0; ww < w; ww++) mask[(j + hh) * nu + i + ww] = 0;
                    /* Quad corners in 1/16 voxel units. */
                    int plane = (s + (k_sign[dir] > 0 ? 1 : 0)) * 16;
                    int u0 = i * 16, u1 = (i + w) * 16, v0 = j * 16, v1 = (j + h) * 16;
                    uint16_t pts[4][3];
                    int cu[4] = {u0, u1, u1, u0}, cv[4] = {v0, v0, v1, v1};
                    for (int k = 0; k < 4; k++) {
                        pts[k][d] = (uint16_t)plane;
                        pts[k][u] = (uint16_t)cu[k];
                        pts[k][v] = (uint16_t)cv[k];
                    }
                    uint8_t ao[4];
                    uint32_t aob = (uint32_t)(key >> 16) & 0xFF;
                    for (int k = 0; k < 4; k++) ao[k] = (uint8_t)((aob >> (2 * k)) & 3);
                    /* Keep counter-clockwise winding seen from outside. */
                    bool flip = (dir == 0 || dir == 2 || dir == 5);
                    if (flip) {
                        uint16_t t[3];
                        uint8_t ta;
                        memcpy(t, pts[1], sizeof t);
                        memcpy(pts[1], pts[3], sizeof t);
                        memcpy(pts[3], t, sizeof t);
                        ta = ao[1];
                        ao[1] = ao[3];
                        ao[3] = ta;
                    }
                    emit_quad(out, (const uint16_t(*)[3])pts, (uint16_t)(key & 0xFFFF), dir, ao, (uint8_t)(key >> 24),
                              (uint8_t)(key >> 32));
                    i += w;
                }
            }
        }
    }
    free(mask);
}

/* Partial (fill-level) voxels: one quad per exposed face, top at fill height. */
static void partial_faces(wn_mesh *out, const grid_ctx *c)
{
    for (int y = 1; y < c->sy - 1; y++) {
        for (int z = 1; z < c->sz - 1; z++) {
            for (int x = 1; x < c->sx - 1; x++) {
                wn_voxel vox = cell(c, x, y, z);
                if (!visible(c, vox) || wn_vox_fill(vox) == 0) continue;
                int top = (int)wn_vox_fill(vox); /* 1..15 sixteenths */
                uint16_t color = cell_color(c, vox, x, y, z);
                uint8_t fl = c->mats->m[wn_vox_mat(vox)].emit ? WN_VFLAG_EMISSIVE : 0;
                int bx = (x - 1) * 16, by = (y - 1) * 16, bz = (z - 1) * 16;
                for (int dir = 0; dir < 6; dir++) {
                    int d = k_axis[dir][0];
                    int q[3] = {x, y, z};
                    q[d] += k_sign[dir];
                    wn_voxel nb = cell(c, q[0], q[1], q[2]);
                    int lo = 0, hi = top;
                    if (dir == 2) {
                        /* top face always visible unless something solid sits right on it */
                        if (visible(c, nb) && top == 16) continue;
                    } else if (dir == 3) {
                        if (visible(c, nb)) continue;
                    } else if (visible(c, nb)) {
                        int nh = wn_vox_fill(nb) == 0 ? 16 : (int)wn_vox_fill(nb);
                        if (nh >= top) continue;
                        lo = nh;
                    }
                    uint8_t ao[4];
                    face_ao(c, x, y, z, dir, ao);
                    if (dir != 3 && dir != 2) {
                        ao[2] = ao[3] = 3; /* the top edge of a partial side is open */
                    }
                    uint8_t light = face_light(c, dir == 2 ? cell(c, x, y + 1, z) : nb);
                    uint16_t p[4][3];
                    int du = 16;
                    int u = k_axis[dir][1], v = k_axis[dir][2];
                    int base[3] = {bx, by, bz};
                    int plane = base[d] + (k_sign[dir] > 0 ? (d == 1 ? top : 16) : 0);
                    int cu[4] = {0, du, du, 0};
                    int cv0 = (v == 1) ? lo : 0, cv1 = (v == 1) ? hi : 16;
                    int cv[4] = {cv0, cv0, cv1, cv1};
                    for (int k = 0; k < 4; k++) {
                        int pt[3];
                        pt[d] = plane;
                        pt[u] = base[u] + cu[k];
                        pt[v] = base[v] + cv[k];
                        p[k][0] = (uint16_t)pt[0];
                        p[k][1] = (uint16_t)pt[1];
                        p[k][2] = (uint16_t)pt[2];
                    }
                    bool flip = (dir == 0 || dir == 2 || dir == 5);
                    if (flip) {
                        uint16_t t[3];
                        memcpy(t, p[1], sizeof t);
                        memcpy(p[1], p[3], sizeof t);
                        memcpy(p[3], t, sizeof t);
                        uint8_t ta = ao[1];
                        ao[1] = ao[3];
                        ao[3] = ta;
                    }
                    emit_quad(out, (const uint16_t(*)[3])p, color, dir, ao, light, fl);
                }
            }
        }
    }
}

void wn_mesh_grid(wn_mesh *out, const wn_voxel *grid, int sx, int sy, int sz, const wn_materials *mats,
                  const wn_mesh_opts *opts)
{
    grid_ctx c = {grid, sx, sy, sz, mats, opts};
    greedy_full(out, &c);
    partial_faces(out, &c);
}

void wn_mesh_gather_chunk(wn_world *w, wn_ivec3 cp, wn_voxel *g)
{
    const int S = WN_CHUNK_SIZE + 2;
    int32_t bx = cp.x * WN_CHUNK_SIZE - 1, by = cp.y * WN_CHUNK_SIZE - 1, bz = cp.z * WN_CHUNK_SIZE - 1;
    wn_chunk *center = wn_world_chunk(w, cp);
    for (int y = 0; y < S; y++) {
        for (int z = 0; z < S; z++) {
            wn_voxel *row = &g[(y * S + z) * S];
            bool inner_yz = y > 0 && y < S - 1 && z > 0 && z < S - 1;
            if (inner_yz && center) {
                row[0] = wn_world_get(w, bx, by + y, bz + z);
                for (int x = 1; x < S - 1; x++) row[x] = wn_chunk_get(center, x - 1, y - 1, z - 1);
                row[S - 1] = wn_world_get(w, bx + S - 1, by + y, bz + z);
            } else {
                for (int x = 0; x < S; x++) row[x] = wn_world_get(w, bx + x, by + y, bz + z);
            }
        }
    }
}

void wn_mesh_chunk(wn_mesh *out, wn_world *w, wn_ivec3 cp)
{
    const int S = WN_CHUNK_SIZE + 2;
    wn_voxel *g = (wn_voxel *)malloc((size_t)S * S * S * sizeof(wn_voxel));
    wn_mesh_gather_chunk(w, cp, g);
    wn_mesh_opts o;
    memset(&o, 0, sizeof o);
    o.hash_origin = wn_iv3(cp.x * WN_CHUNK_SIZE, cp.y * WN_CHUNK_SIZE, cp.z * WN_CHUNK_SIZE);
    o.use_light = true;
    wn_mesh_clear(out);
    wn_mesh_grid(out, g, S, S, S, w->mats, &o);
    free(g);
}
