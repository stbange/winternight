#include "wn/wn_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wn_model_init(wn_model *m)
{
    memset(m, 0, sizeof *m);
    m->bleeds = true;
}

void wn_model_free(wn_model *m)
{
    for (int i = 0; i < m->nparts; i++) free(m->parts[i].grid);
    memset(m, 0, sizeof *m);
}

int wn_model_add_part(wn_model *m, const char *name, int parent, int w, int h, int d, wn_vec3 pivot, wn_vec3 attach)
{
    if (m->nparts >= WN_MODEL_MAX_PARTS) return -1;
    int i = m->nparts++;
    wn_model_part *p = &m->parts[i];
    memset(p, 0, sizeof *p);
    snprintf(p->name, sizeof p->name, "%s", name);
    p->sx = w + 2;
    p->sy = h + 2;
    p->sz = d + 2;
    p->grid = (wn_voxel *)calloc((size_t)p->sx * p->sy * p->sz, sizeof(wn_voxel));
    p->parent = parent;
    p->pivot = pivot;
    p->attach = attach;
    return i;
}

static inline size_t pidx(const wn_model_part *p, int x, int y, int z)
{
    return ((size_t)(y + 1) * (size_t)p->sz + (size_t)(z + 1)) * (size_t)p->sx + (size_t)(x + 1);
}

static inline bool in_part(const wn_model_part *p, int x, int y, int z)
{
    return x >= 0 && y >= 0 && z >= 0 && x < p->sx - 2 && y < p->sy - 2 && z < p->sz - 2;
}

void wn_model_part_set(wn_model *m, int part, int x, int y, int z, wn_voxel v)
{
    wn_model_part *p = &m->parts[part];
    if (!in_part(p, x, y, z)) return;
    wn_voxel *c = &p->grid[pidx(p, x, y, z)];
    if (wn_vox_mat(*c) && !wn_vox_mat(v)) p->nvoxels--;
    if (!wn_vox_mat(*c) && wn_vox_mat(v)) p->nvoxels++;
    *c = v;
    p->mesh_rev++;
}

void wn_model_part_box(wn_model *m, int part, int x0, int y0, int z0, int x1, int y1, int z1, wn_voxel v)
{
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) wn_model_part_set(m, part, x, y, z, v);
}

wn_voxel wn_model_part_get(const wn_model *m, int part, int x, int y, int z)
{
    const wn_model_part *p = &m->parts[part];
    if (!in_part(p, x, y, z)) return 0;
    return p->grid[pidx(p, x, y, z)];
}

int wn_model_find_part(const wn_model *m, const char *name)
{
    for (int i = 0; i < m->nparts; i++)
        if (strcmp(m->parts[i].name, name) == 0) return i;
    return -1;
}

/* Local transform (voxel units): translate to attach in parent, rotate, move pivot to origin. */
static wn_mat4 local_matrix(const wn_model_part *p)
{
    wn_mat4 t = wn_mat4_translate(p->attach);
    wn_mat4 rz = wn_mat4_identity();
    float c = wn_cosf(p->rz), s = wn_sinf(p->rz);
    rz.m[0] = c;
    rz.m[1] = s;
    rz.m[4] = -s;
    rz.m[5] = c;
    wn_mat4 r = wn_mat4_mul(wn_mat4_rotate_x(p->rx), rz);
    wn_mat4 back = wn_mat4_translate(wn_v3(-p->pivot.x, -p->pivot.y, -p->pivot.z));
    return wn_mat4_mul(t, wn_mat4_mul(r, back));
}

wn_mat4 wn_model_part_matrix(const wn_model *m, int part)
{
    wn_mat4 acc = wn_mat4_identity();
    int chain[WN_MODEL_MAX_PARTS];
    int n = 0;
    for (int i = part; i >= 0 && n < WN_MODEL_MAX_PARTS; i = m->parts[i].parent) chain[n++] = i;
    for (int k = n - 1; k >= 0; k--) acc = wn_mat4_mul(acc, local_matrix(&m->parts[chain[k]]));
    wn_mat4 world = wn_mat4_mul(wn_mat4_translate(m->pos), wn_mat4_rotate_y(m->yaw));
    wn_mat4 scale = wn_mat4_scale(wn_v3(WN_VOXEL_SIZE, WN_VOXEL_SIZE, WN_VOXEL_SIZE));
    return wn_mat4_mul(world, wn_mat4_mul(scale, acc));
}

static float seg_dist(wn_vec3 p, wn_vec3 a, wn_vec3 b)
{
    wn_vec3 ab = wn_v3_sub(b, a);
    float l2 = wn_v3_dot(ab, ab);
    float t = l2 > 0.0f ? wn_v3_dot(wn_v3_sub(p, a), ab) / l2 : 0.0f;
    t = WN_CLAMP(t, 0.0f, 1.0f);
    return wn_v3_len(wn_v3_sub(p, wn_v3_add(a, wn_v3_scale(ab, t))));
}

/* A part stays attached while any voxel within 1.5 voxels of its pivot remains. */
static bool joint_intact(const wn_model_part *p)
{
    for (int y = 0; y < p->sy - 2; y++)
        for (int z = 0; z < p->sz - 2; z++)
            for (int x = 0; x < p->sx - 2; x++) {
                if (!wn_vox_mat(p->grid[pidx(p, x, y, z)])) continue;
                wn_vec3 c = wn_v3((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f);
                if (wn_v3_len(wn_v3_sub(c, p->pivot)) <= 1.6f) return true;
            }
    return false;
}

static void sever(wn_model *m, int part, wn_debris_set *debris, const wn_materials *mats)
{
    wn_model_part *p = &m->parts[part];
    if (p->severed) return;
    /* Children go with it. */
    for (int i = 0; i < m->nparts; i++)
        if (m->parts[i].parent == part) sever(m, i, debris, mats);
    p->severed = true;
    if (!debris || p->nvoxels == 0) return;
    wn_mat4 mat = wn_model_part_matrix(m, part);
    wn_vec3 origin = wn_mat4_mul_point(mat, wn_v3(0, 0, 0));
    wn_debris *d = wn_debris_spawn(debris, p->grid, p->sx, p->sy, p->sz, origin, mats);
    if (d) {
        /* Keep the part's world orientation (yaw only; good enough for a toss). */
        d->rot.y = wn_sinf(m->yaw * 0.5f);
        d->rot.w = wn_cosf(m->yaw * 0.5f);
        d->pos = wn_mat4_mul_point(mat, d->com_local);
        d->vel = wn_v3(0.0f, 1.5f, 0.0f);
    }
}

wn_model_hit wn_model_damage(wn_model *m, wn_vec3 a, wn_vec3 b, float radius, wn_debris_set *debris,
                             const wn_materials *mats)
{
    wn_model_hit hit;
    memset(&hit, 0, sizeof hit);
    hit.part = -1;
    for (int i = 0; i < m->nparts; i++) {
        wn_model_part *p = &m->parts[i];
        if (p->severed) continue;
        wn_mat4 mat = wn_model_part_matrix(m, i);
        int removed = 0;
        for (int y = 0; y < p->sy - 2; y++)
            for (int z = 0; z < p->sz - 2; z++)
                for (int x = 0; x < p->sx - 2; x++) {
                    if (!wn_vox_mat(p->grid[pidx(p, x, y, z)])) continue;
                    wn_vec3 wp = wn_mat4_mul_point(mat, wn_v3((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f));
                    if (seg_dist(wp, a, b) > radius) continue;
                    if (hit.voxels_removed == 0 && removed == 0) hit.point = wp;
                    wn_model_part_set(m, i, x, y, z, 0);
                    removed++;
                }
        if (removed) {
            if (hit.part < 0) hit.part = i;
            hit.voxels_removed += removed;
            if (p->parent >= 0 && !joint_intact(p)) {
                sever(m, i, debris, mats);
                hit.severed = true;
            }
        }
    }
    return hit;
}
