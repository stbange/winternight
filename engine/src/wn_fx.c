#include "wn/wn_fx.h"

#include "wn/wn_light.h"

#include <string.h>

/* Random vector with each component drawn in a fixed order. Never put several
 * RNG calls into one function call's arguments: C leaves their evaluation
 * order unspecified (GCC and Clang differ), which breaks determinism. */
static wn_vec3 rand_v3(wn_rng *r, float x0, float x1, float y0, float y1, float z0, float z1)
{
    float x = wn_rng_frange(r, x0, x1);
    float y = wn_rng_frange(r, y0, y1);
    float z = wn_rng_frange(r, z0, z1);
    return wn_v3(x, y, z);
}

void wn_fx_init(wn_fx *fx, uint64_t seed)
{
    memset(fx, 0, sizeof *fx);
    fx->capacity = WN_MAX_PARTICLES;
    wn_rng_seed(&fx->rng, seed, 101);
    wn_rng_seed(&fx->cosmetic_rng, seed, 202);
    fx->gore = WN_GORE_FULL;
    fx->stain_dry_time = 120.0f;
    fx->pool_freeze_time = 8.0f;
    fx->wind = wn_v3(0.35f, 0.0f, 0.15f);
}

static bool is_cosmetic(wn_particle_type type) { return type == WN_PART_SNOWFLAKE || type == WN_PART_DUST; }

wn_particle *wn_fx_spawn(wn_fx *fx, wn_particle_type type, wn_vec3 pos, wn_vec3 vel, uint32_t color, float life)
{
    wn_particle *p;
    if (is_cosmetic(type)) {
        /* Cosmetic particles live in their own pool so they can never change
         * the order or capacity of gameplay particles (determinism). */
        uint32_t cap = WN_MIN(fx->capacity, (uint32_t)WN_MAX_COSMETIC);
        if (fx->ncosmetic >= cap) return NULL;
        p = &fx->cosmetic[fx->ncosmetic++];
    } else {
        if (fx->count >= WN_MAX_PARTICLES) return NULL;
        p = &fx->p[fx->count++];
    }
    p->pos = pos;
    p->vel = vel;
    p->life = life;
    p->color = color;
    p->type = (uint8_t)type;
    p->size = 1;
    return p;
}

int wn_fx_wound(wn_fx *fx, wn_vec3 pos, wn_vec3 dir, float damage, bool bleeds)
{
    if (damage <= 0.0f) return 0;
    int n;
    if (!bleeds) {
        n = (int)(damage * 4.0f) + 2;
        for (int i = 0; i < n; i++) {
            wn_vec3 v = rand_v3(&fx->rng, -1, 1, 0.2f, 1.5f, -1, 1);
            wn_fx_spawn(fx, WN_PART_BONE_DUST, pos, wn_v3_add(v, wn_v3_scale(dir, 1.5f)), 0xD8CFB4, 1.2f);
        }
        return n;
    }
    if (fx->gore == WN_GORE_OFF) return 0;
    n = (int)(damage * 6.0f) + 3;
    if (fx->gore == WN_GORE_REDUCED) n = n / 3 + 1;
    wn_vec3 d = wn_v3_norm(dir);
    for (int i = 0; i < n; i++) {
        float sp = wn_rng_frange(&fx->rng, 1.0f, 2.5f + damage * 0.4f);
        wn_vec3 jitter = rand_v3(&fx->rng, -0.6f, 0.6f, -0.2f, 0.8f, -0.6f, 0.6f);
        wn_vec3 v = wn_v3_scale(wn_v3_norm(wn_v3_add(d, jitter)), sp);
        uint32_t col = wn_rng_range(&fx->rng, 3) == 0 ? 0xC21E22 : 0x8C1016;
        wn_particle *p = wn_fx_spawn(fx, WN_PART_BLOOD, pos, v, col, 3.0f);
        if (p && damage > 4.0f && i % 4 == 0) p->size = 2;
    }
    return n;
}

void wn_fx_stain(wn_fx *fx, wn_world *w, int32_t x, int32_t y, int32_t z, int amount)
{
    wn_voxel v = wn_world_get(w, x, y, z);
    if (!wn_mat_is_solid(w->mats, wn_vox_mat(v))) return;
    uint32_t a = wn_vox_stain_amount(v);
    uint32_t type = wn_vox_stain(v);
    if (type != WN_STAIN_BLOOD) a = type == WN_STAIN_BLOOD_DRIED ? a / 2 : 0; /* fresh blood over old */
    uint32_t na = WN_MIN(63u, a + (uint32_t)amount);
    wn_world_set(w, x, y, z, wn_vox_set_stain(v, WN_STAIN_BLOOD, na));
    fx->stains_written++;
    wn_stain_rec *r = &fx->stains[fx->stain_head];
    r->pos = wn_iv3(x, y, z);
    r->age = 0.0f;
    fx->stain_head = (fx->stain_head + 1) % WN_MAX_STAINS;
    if (fx->nstains < WN_MAX_STAINS) fx->nstains++;
}

static void stain_hit(wn_fx *fx, wn_world *w, wn_ivec3 v, int amount)
{
    wn_fx_stain(fx, w, v.x, v.y, v.z, amount);
    if (wn_vox_mat(wn_world_get(w, v.x, v.y, v.z)) != WN_MAT_SNOW) return;
    /* Blood seeps into snow: weaker stain on adjacent snow voxels. */
    static const int n6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (int d = 0; d < 6; d++) {
        wn_ivec3 n = wn_iv3(v.x + n6[d][0], v.y + n6[d][1], v.z + n6[d][2]);
        if (wn_vox_mat(wn_world_get(w, n.x, n.y, n.z)) == WN_MAT_SNOW) wn_fx_stain(fx, w, n.x, n.y, n.z, amount / 2);
    }
}

void wn_fx_snow_clump(void *fx_user, wn_vec3 pos)
{
    wn_fx *fx = (wn_fx *)fx_user;
    wn_vec3 v = rand_v3(&fx->rng, -0.3f, 0.3f, 0.0f, 0.0f, -0.3f, 0.3f);
    wn_particle *p = wn_fx_spawn(fx, WN_PART_SNOW_CLUMP, pos, v, 0xC9D6F2, 6.0f);
    if (p) p->size = 2;
}

void wn_fx_dust(wn_fx *fx, wn_vec3 pos, uint32_t color, int count)
{
    for (int i = 0; i < count; i++) {
        wn_vec3 v = rand_v3(&fx->cosmetic_rng, -1.2f, 1.2f, 0.3f, 2.0f, -1.2f, 1.2f);
        wn_fx_spawn(fx, WN_PART_DUST, pos, v, color, 0.8f);
    }
}

int wn_fx_blood_pool(wn_fx *fx, wn_edit_ctx *e, wn_vec3 pos, int volume16)
{
    if (fx->gore != WN_GORE_FULL || volume16 <= 0) return 0;
    wn_world *w = e->world;
    wn_ivec3 c = wn_world_to_voxel(pos);
    /* Find the ground below. */
    int32_t y = c.y;
    while (y > c.y - 20 && !wn_world_is_solid(w, c.x, y - 1, c.z)) y--;
    int created = 0;
    wn_voxel cur = wn_world_get(w, c.x, y, c.z);
    if (wn_vox_mat(cur) == WN_MAT_AIR) {
        wn_edit_set(e, c.x, y, c.z, wn_vox_make(WN_MAT_BLOOD, (uint32_t)WN_CLAMP(volume16, 1, 15)));
        created = 1;
    } else if (wn_vox_mat(cur) == WN_MAT_BLOOD) {
        uint32_t h = WN_MIN(15u, wn_vox_height16(cur) + (uint32_t)volume16);
        wn_edit_set(e, c.x, y, c.z, wn_vox_make(WN_MAT_BLOOD, h));
    } else {
        return 0;
    }
    if (created && fx->npool < WN_MAX_POOL_CELLS) {
        fx->pool[fx->npool].pos = wn_iv3(c.x, y, c.z);
        fx->pool[fx->npool].age = 0.0f;
        fx->npool++;
    }
    /* Blood pooling on snow stains it below. */
    stain_hit(fx, w, wn_iv3(c.x, y - 1, c.z), 30);
    return created;
}

static void pool_step(wn_fx *fx, wn_world *w, float dt)
{
    wn_edit_ctx e;
    wn_edit_begin(&e, w);
    static const int d4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (uint32_t i = 0; i < fx->npool;) {
        wn_pool_cell *c = &fx->pool[i];
        c->age += dt;
        wn_voxel v = wn_world_get(w, c->pos.x, c->pos.y, c->pos.z);
        if (wn_vox_mat(v) != WN_MAT_BLOOD) {
            fx->pool[i] = fx->pool[--fx->npool];
            continue;
        }
        if (c->age >= fx->pool_freeze_time) {
            wn_edit_set(&e, c->pos.x, c->pos.y, c->pos.z, wn_vox_make(WN_MAT_FROZEN_BLOOD, wn_vox_fill(v)));
            fx->pool[i] = fx->pool[--fx->npool];
            continue;
        }
        uint32_t h = wn_vox_height16(v);
        /* Fall down if nothing below. */
        wn_voxel below = wn_world_get(w, c->pos.x, c->pos.y - 1, c->pos.z);
        if (wn_vox_mat(below) == WN_MAT_AIR) {
            wn_edit_set(&e, c->pos.x, c->pos.y, c->pos.z, 0);
            c->pos.y--;
            wn_edit_set(&e, c->pos.x, c->pos.y, c->pos.z, wn_vox_make(WN_MAT_BLOOD, h));
            i++;
            continue;
        }
        /* Spread to a lower neighbour (every few ticks, while it has volume). */
        if (h >= 4) {
            for (int d = 0; d < 4; d++) {
                int32_t nx = c->pos.x + d4[d][0], nz = c->pos.z + d4[d][1];
                wn_voxel n = wn_world_get(w, nx, c->pos.y, nz);
                uint32_t nm = wn_vox_mat(n);
                uint32_t nh = nm == WN_MAT_BLOOD ? wn_vox_height16(n) : 0;
                if ((nm != WN_MAT_AIR && nm != WN_MAT_BLOOD) || nh + 2 > h) continue;
                /* Flowing over an edge is fine: the cell falls on the next tick. */
                uint32_t move = (h - nh) / 2;
                h -= move;
                wn_edit_set(&e, c->pos.x, c->pos.y, c->pos.z, wn_vox_make(WN_MAT_BLOOD, h));
                wn_edit_set(&e, nx, c->pos.y, nz, wn_vox_make(WN_MAT_BLOOD, nh + move));
                if (nm == WN_MAT_AIR && fx->npool < WN_MAX_POOL_CELLS) {
                    fx->pool[fx->npool].pos = wn_iv3(nx, c->pos.y, nz);
                    fx->pool[fx->npool].age = c->age;
                    fx->npool++;
                }
                stain_hit(fx, w, wn_iv3(nx, c->pos.y - 1, nz), 20);
                break;
            }
        }
        i++;
    }
    wn_edit_end(&e);
}

static void stains_step(wn_fx *fx, wn_world *w, float dt)
{
    for (uint32_t k = 0; k < fx->nstains; k++) {
        uint32_t i = (fx->stain_head + WN_MAX_STAINS - 1 - k) % WN_MAX_STAINS;
        wn_stain_rec *r = &fx->stains[i];
        if (r->age < 0.0f) continue; /* already dried */
        r->age += dt;
        if (r->age < fx->stain_dry_time) continue;
        r->age = -1.0f;
        wn_voxel v = wn_world_get(w, r->pos.x, r->pos.y, r->pos.z);
        if (wn_vox_stain(v) == WN_STAIN_BLOOD)
            wn_world_set(w, r->pos.x, r->pos.y, r->pos.z, wn_vox_set_stain(v, WN_STAIN_BLOOD_DRIED, wn_vox_stain_amount(v)));
    }
}

static void spawn_snowfall(wn_fx *fx, wn_vec3 viewer, float dt)
{
    if (fx->snowfall <= 0.0f) return;
    fx->flake_accum += fx->snowfall * dt;
    while (fx->flake_accum >= 1.0f) {
        fx->flake_accum -= 1.0f;
        wn_vec3 p = wn_v3_add(viewer, rand_v3(&fx->cosmetic_rng, -14.0f, 14.0f, 4.0f, 12.0f, -14.0f, 14.0f));
        wn_vec3 v = wn_v3(0.0f, wn_rng_frange(&fx->cosmetic_rng, -1.3f, -0.7f), 0.0f);
        uint32_t col = wn_rng_range(&fx->cosmetic_rng, 4) == 0 ? 0xFFFFFF : 0xC9D6F2;
        if (!wn_fx_spawn(fx, WN_PART_SNOWFLAKE, p, v, col, 14.0f)) break;
    }
}

static void step_pool(wn_fx *fx, wn_world *w, wn_edit_ctx *e, wn_particle *pool, uint32_t *count, float dt)
{
    for (uint32_t i = 0; i < *count;) {
        wn_particle *p = &pool[i];
        p->life -= dt;
        bool dead = p->life <= 0.0f;
        if (!dead) {
            switch (p->type) {
            case WN_PART_SNOWFLAKE: {
                /* Gentle sway in the wind. */
                float sway = wn_sinf(p->pos.y * 1.7f + p->pos.x) * 0.3f;
                p->pos = wn_v3_add(p->pos, wn_v3_scale(wn_v3_add(p->vel, wn_v3(fx->wind.x + sway, 0, fx->wind.z)), dt));
                break;
            }
            case WN_PART_DUST:
            case WN_PART_BONE_DUST:
                p->vel.y -= 4.0f * dt;
                p->vel = wn_v3_scale(p->vel, 0.96f);
                p->pos = wn_v3_add(p->pos, wn_v3_scale(p->vel, dt));
                break;
            default:
                p->vel.y -= 9.81f * dt;
                p->pos = wn_v3_add(p->pos, wn_v3_scale(p->vel, dt));
                break;
            }
            wn_ivec3 v = wn_world_to_voxel(p->pos);
            wn_voxel cell = wn_world_get(w, v.x, v.y, v.z);
            if (wn_mat_is_solid(w->mats, wn_vox_mat(cell)) &&
                p->pos.y <= ((float)v.y + (float)wn_vox_height16(cell) / 16.0f) * WN_VOXEL_SIZE) {
                dead = true;
                if (p->type == WN_PART_BLOOD) stain_hit(fx, w, v, p->size > 1 ? 24 : 12);
                if (p->type == WN_PART_SNOW_CLUMP) wn_snow_add(e, v.x, v.z, 6);
            }
        }
        if (dead) {
            pool[i] = pool[--*count];
            continue;
        }
        i++;
    }
}

void wn_fx_step(wn_fx *fx, wn_world *w, wn_vec3 viewer, float dt)
{
    spawn_snowfall(fx, viewer, dt);
    wn_edit_ctx e;
    wn_edit_begin(&e, w);
    step_pool(fx, w, &e, fx->p, &fx->count, dt);
    step_pool(fx, w, &e, fx->cosmetic, &fx->ncosmetic, dt);
    wn_edit_end(&e);
    stains_step(fx, w, dt);
    pool_step(fx, w, dt);
}

uint32_t wn_fx_checksum(const wn_fx *fx)
{
    /* Gameplay-relevant state only: snowflakes are cosmetic. */
    wn_hasher h;
    wn_hasher_init(&h, 13u);
    for (uint32_t i = 0; i < fx->count; i++) {
        const wn_particle *p = &fx->p[i];
        wn_hasher_add(&h, &p->pos, sizeof p->pos);
        wn_hasher_add(&h, &p->type, 1);
    }
    wn_hasher_add(&h, &fx->stains_written, sizeof fx->stains_written);
    wn_hasher_add(&h, &fx->npool, sizeof fx->npool);
    return wn_hasher_finish(&h);
}
