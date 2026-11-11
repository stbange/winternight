#include "wn/wn_game.h"

#include "wn/wn_core.h"
#include "wn/wn_light.h"
#include "wn/wn_xr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================= Initialisation ======================= */

bool wn_game_init(wn_game *g, const char *materials_txt, const char *prefabs_txt, const char *scene_txt, uint64_t seed,
                  char *err, size_t errlen)
{
    memset(g, 0, sizeof *g);
    char e2[200];
    if (!wn_materials_load(&g->mats, materials_txt, e2, sizeof e2)) {
        snprintf(err, errlen, "materials: %s", e2);
        return false;
    }
    if (!wn_prefabs_load(&g->prefabs, &g->mats, prefabs_txt, e2, sizeof e2)) {
        snprintf(err, errlen, "prefabs: %s", e2);
        return false;
    }
    if (!wn_scene_load(&g->scene, &g->mats, &g->prefabs, scene_txt, e2, sizeof e2)) {
        snprintf(err, errlen, "scene: %s", e2);
        return false;
    }
    /* Placements point into g->prefabs, which is stable inside g. */
    if (seed) g->scene.seed = seed;
    wn_rng_seed(&g->rng, g->scene.seed, 1);
    wn_world_init(&g->world, &g->mats, g->scene.chunk_y_min, g->scene.chunk_y_max);
    wn_world_set_generator(&g->world, wn_scene_generate_chunk, &g->scene);
    g->world.on_column = wn_light_column_loaded;
    wn_debris_init(&g->debris);
    wn_fx_init(&g->fx, g->scene.seed);
    g->fx.snowfall = g->scene.snowfall_flakes;
    g->debris.on_vanish = NULL;
    wn_snow_init(&g->snow);
    g->snow.rate = g->scene.snow_rate;
    g->snow.columns_per_step = 1024;
    g->atmos = g->scene.atmos;
    g->view_radius = 5;
    memset(&g->player, 0, sizeof g->player);
    g->player.pos = g->scene.spawn;
    g->player.yaw = g->scene.spawn_yaw;
    g->player.health = 100.0f;
    g->player.swing_power = 1.0f;
    g->prev_player = g->player;
    g->cam.yaw = g->player.yaw;
    g->cam.pitch = -0.12f;
    g->cam.distance = 3.2f;
    g->cam.shoulder = 0.45f;
    g->cam.actual_dist = g->cam.distance;
    wn_build_warrior(&g->warrior, &g->mats);
    g->loaded = true;
    return true;
}

bool wn_game_init_from_dir(wn_game *g, const char *dir, const char *scene_name, uint64_t seed, char *err, size_t errlen)
{
    char path[512];
    snprintf(path, sizeof path, "%s/materials.txt", dir);
    char *mats = wn_read_file(path, NULL);
    snprintf(path, sizeof path, "%s/prefabs.txt", dir);
    char *pref = wn_read_file(path, NULL);
    snprintf(path, sizeof path, "%s/scenes/%s.scene", dir, scene_name);
    char *scene = wn_read_file(path, NULL);
    bool ok = false;
    if (!mats || !pref || !scene) {
        snprintf(err, errlen, "cannot read game data in '%s' (scene '%s')", dir, scene_name);
    } else {
        ok = wn_game_init(g, mats, pref, scene, seed, err, errlen);
    }
    free(mats);
    free(pref);
    free(scene);
    return ok;
}

void wn_game_free(wn_game *g)
{
    if (!g->loaded) return;
    wn_world_free(&g->world);
    wn_debris_free(&g->debris);
    wn_model_free(&g->warrior);
    g->loaded = false;
}

int wn_game_stream(wn_game *g, int max_columns)
{
    return wn_world_stream(&g->world, g->player.pos, g->view_radius, max_columns);
}

void wn_game_stream_all(wn_game *g)
{
    while (wn_game_stream(g, 64) > 0) {
    }
}

/* ======================= Warrior model ======================= */

static wn_voxel mat_or(const wn_materials *mats, const char *name, uint32_t fallback)
{
    int id = wn_materials_find(mats, name);
    return wn_vox_make(id > 0 ? (uint32_t)id : fallback, 0);
}

void wn_build_warrior(wn_model *m, const wn_materials *mats)
{
    wn_model_init(m);
    wn_voxel fur = mat_or(mats, "fur", WN_MAT_EARTH), leather = mat_or(mats, "leather", WN_MAT_EARTH),
             skin = mat_or(mats, "skin", WN_MAT_EARTH), hair = mat_or(mats, "hair", WN_MAT_EARTH),
             steel = mat_or(mats, "steel", WN_MAT_STONE), cloth = mat_or(mats, "cloth-red", WN_MAT_EARTH),
             iron = mat_or(mats, "iron", WN_MAT_STONE);
    /* Pelvis is the root: 6 wide, 3 high, 4 deep; pivot at its centre bottom. */
    int pelvis = wn_model_add_part(m, "pelvis", -1, 6, 3, 4, wn_v3(3, 0, 2), wn_v3(0, 0, 0));
    wn_model_part_box(m, pelvis, 0, 0, 0, 5, 2, 3, leather);
    int torso = wn_model_add_part(m, "torso", pelvis, 6, 6, 4, wn_v3(3, 0, 2), wn_v3(3, 3, 2));
    wn_model_part_box(m, torso, 0, 0, 0, 5, 5, 3, cloth);
    wn_model_part_box(m, torso, 0, 4, 0, 5, 5, 3, fur);     /* fur collar */
    wn_model_part_box(m, torso, 0, 0, 0, 5, 1, 3, leather); /* belt */
    wn_model_part_box(m, torso, 0, 1, 0, 5, 5, 0, fur);     /* cloak at the back (-z) */
    wn_model_part_set(m, torso, 2, 2, 3, iron);             /* buckle */
    int head = wn_model_add_part(m, "head", torso, 4, 4, 4, wn_v3(2, 0, 2), wn_v3(3, 6, 2));
    wn_model_part_box(m, head, 0, 0, 0, 3, 3, 3, skin);
    wn_model_part_box(m, head, 0, 2, 0, 3, 3, 3, hair);
    wn_model_part_box(m, head, 0, 0, 0, 3, 3, 0, hair);      /* back of head */
    wn_model_part_box(m, head, 0, 0, 3, 3, 0, 3, hair);      /* beard */
    int legl = wn_model_add_part(m, "leg_l", pelvis, 2, 8, 2, wn_v3(1, 8, 1), wn_v3(1.5f, 0, 2));
    wn_model_part_box(m, legl, 0, 0, 0, 1, 7, 1, leather);
    wn_model_part_box(m, legl, 0, 0, 0, 1, 2, 1, fur); /* boots */
    int legr = wn_model_add_part(m, "leg_r", pelvis, 2, 8, 2, wn_v3(1, 8, 1), wn_v3(4.5f, 0, 2));
    wn_model_part_box(m, legr, 0, 0, 0, 1, 7, 1, leather);
    wn_model_part_box(m, legr, 0, 0, 0, 1, 2, 1, fur);
    int arml = wn_model_add_part(m, "arm_l", torso, 2, 6, 2, wn_v3(1, 6, 1), wn_v3(-1, 6, 2));
    wn_model_part_box(m, arml, 0, 0, 0, 1, 5, 1, cloth);
    wn_model_part_box(m, arml, 0, 0, 0, 1, 1, 1, skin);
    int armr = wn_model_add_part(m, "arm_r", torso, 2, 6, 2, wn_v3(1, 6, 1), wn_v3(7, 6, 2));
    wn_model_part_box(m, armr, 0, 0, 0, 1, 5, 1, cloth);
    wn_model_part_box(m, armr, 0, 0, 0, 1, 1, 1, skin);
    /* Sword in the right hand: blade pointing forward (+z) from the fist. */
    int sword = wn_model_add_part(m, "sword", armr, 1, 1, 10, wn_v3(0.5f, 0.5f, 0.5f), wn_v3(1, 0.5f, 1));
    wn_model_part_box(m, sword, 0, 0, 2, 0, 0, 9, steel);
    wn_model_part_box(m, sword, 0, 0, 0, 0, 0, 1, leather);
    m->bleeds = true;
}

/* ======================= Player ======================= */

/* Height the player can stand on at (x,z): solid tops, or deep snow. */
static float support_height(wn_game *g, float x, float z, float feet)
{
    wn_world *w = &g->world;
    wn_ivec3 v = wn_world_to_voxel(wn_v3(x, feet + 0.45f, z));
    float best = -1e9f;
    float snow_top = -1e9f;
    for (int32_t y = v.y; y > v.y - 30; y--) {
        wn_voxel c = wn_world_get(w, v.x, y, v.z);
        uint32_t m = wn_vox_mat(c);
        if (!wn_mat_is_solid(w->mats, m)) continue;
        float top = ((float)y + (float)wn_vox_height16(c) / 16.0f) * WN_VOXEL_SIZE;
        if (m == WN_MAT_SNOW) {
            if (snow_top < -1e8f) snow_top = top;
            continue;
        }
        best = top;
        break;
    }
    /* The player wades through snow, but deep drifts carry them. */
    if (snow_top > -1e8f) best = WN_MAX(best, snow_top - WN_WADE_DEPTH);
    return best;
}

/* Blocking test against non-snow solids above the step height. */
static bool body_blocked(wn_game *g, wn_vec3 feet, float step_up)
{
    wn_world *w = &g->world;
    float r = WN_PLAYER_RADIUS;
    wn_ivec3 a = wn_world_to_voxel(wn_v3(feet.x - r, feet.y + step_up, feet.z - r));
    wn_ivec3 b = wn_world_to_voxel(wn_v3(feet.x + r, feet.y + WN_PLAYER_HEIGHT, feet.z + r));
    for (int32_t y = a.y; y <= b.y; y++)
        for (int32_t z = a.z; z <= b.z; z++)
            for (int32_t x = a.x; x <= b.x; x++) {
                wn_voxel c = wn_world_get(w, x, y, z);
                uint32_t m = wn_vox_mat(c);
                if (!wn_mat_is_solid(w->mats, m) || m == WN_MAT_SNOW) continue;
                float top = ((float)y + (float)wn_vox_height16(c) / 16.0f) * WN_VOXEL_SIZE;
                if (top > feet.y + step_up) return true;
            }
    return false;
}

static void brush_trees(wn_game *g)
{
    wn_player *p = &g->player;
    if (p->brush_cooldown > 0.0f) return;
    wn_vec3 c = wn_v3_add(p->pos, wn_v3(0, 1.5f, 0));
    wn_ivec3 v = wn_world_to_voxel(c);
    int r = 4;
    bool needles = false;
    for (int dy = -6; dy <= 6 && !needles; dy += 3)
        for (int dz = -r; dz <= r && !needles; dz += 2)
            for (int dx = -r; dx <= r && !needles; dx += 2)
                needles = wn_vox_mat(wn_world_get(&g->world, v.x + dx, v.y + dy, v.z + dz)) == WN_MAT_PINE_NEEDLES;
    if (!needles) return;
    wn_edit_ctx e;
    wn_edit_begin(&e, &g->world);
    int n = wn_snow_shake(&e, wn_v3_add(c, wn_v3(0, 0.6f, 0)), 0.9f, wn_fx_snow_clump, &g->fx);
    wn_edit_end(&e);
    if (n > 0) p->brush_cooldown = 1.0f;
}

static void on_destroy_voxel(void *user, wn_ivec3 pos, wn_voxel old)
{
    wn_game *g = (wn_game *)user;
    uint32_t col = g->mats.m[wn_vox_mat(old)].ramp_n ? g->mats.m[wn_vox_mat(old)].ramp[0] : 0x808080;
    if (wn_rng_range(&g->fx.cosmetic_rng, 3) == 0) wn_fx_dust(&g->fx, wn_voxel_center(pos), col, 1);
}

/* Melee chop: a downward diagonal blade path in front of the player. Odd
 * swings come from the left so repeated chops widen the cut. */
static void melee_hit(wn_game *g)
{
    wn_player *p = &g->player;
    float side = (p->swing_count & 1) ? -1.0f : 1.0f;
    wn_vec3 fwd = wn_v3(wn_sinf(p->yaw), 0, wn_cosf(p->yaw));
    wn_vec3 right = wn_v3(fwd.z, 0, -fwd.x);
    wn_vec3 base = p->pos;
    wn_vec3 a = wn_v3_add(base, wn_v3_add(wn_v3_scale(right, 0.5f * side), wn_v3(0, 1.6f, 0)));
    a = wn_v3_add(a, wn_v3_scale(fwd, 0.3f));
    wn_vec3 b = wn_v3_add(base, wn_v3_add(wn_v3_scale(right, -0.2f * side), wn_v3(0, 0.12f, 0)));
    b = wn_v3_add(b, wn_v3_scale(fwd, 1.05f));
    wn_edit_ctx e;
    wn_edit_begin(&e, &g->world);
    e.on_destroy = on_destroy_voxel;
    e.user = g;
    /* 4.5: wood (4) breaks in one hit, frozen earth (6) needs two. A chop is
     * a diagonal stroke plus the vertical cut it opens in front of the body. */
    float dmg = 4.5f * p->swing_power;
    wn_edit_capsule(&e, a, b, 0.22f, dmg);
    for (int k = 0; k < 3; k++) {
        float f = 0.42f + 0.3f * (float)k;
        wn_vec3 top = wn_v3_add(base, wn_v3_add(wn_v3_scale(fwd, f), wn_v3(0, 1.5f, 0)));
        top = wn_v3_add(top, wn_v3_scale(right, 0.2f * side));
        wn_vec3 bot = wn_v3_add(top, wn_v3(0, -1.35f, 0));
        wn_edit_capsule(&e, top, bot, 0.24f, dmg);
    }
    /* Shake snow off trees that were hit. */
    if (e.destroyed || e.damaged) {
        wn_vec3 mid = wn_v3_scale(wn_v3_add(a, b), 0.5f);
        wn_snow_shake(&e, wn_v3_add(mid, wn_v3(0, 1.5f, 0)), 2.0f, wn_fx_snow_clump, &g->fx);
    }
    g->debris.detach_push = wn_v3_scale(fwd, 2.5f);
    wn_debris_detach(&g->debris, &e);
    wn_edit_end(&e);
}

static void update_player(wn_game *g, const wn_input_frame *in)
{
    wn_player *p = &g->player;
    const float dt = WN_SIM_DT;
    /* Camera look. */
    g->cam.yaw += in->axis[WN_ACT_LOOK_X];
    g->cam.pitch = WN_CLAMP(g->cam.pitch + in->axis[WN_ACT_LOOK_Y], -1.2f, 0.7f);
    /* Movement relative to the camera. */
    float mx = in->axis[WN_ACT_MOVE_X], my = in->axis[WN_ACT_MOVE_Y];
    float len = wn_sqrtf(mx * mx + my * my);
    if (len > 1.0f) {
        mx /= len;
        my /= len;
        len = 1.0f;
    }
    wn_vec3 cf = wn_v3(wn_sinf(g->cam.yaw), 0, wn_cosf(g->cam.yaw));
    wn_vec3 cr = wn_v3(cf.z, 0, -cf.x);
    wn_vec3 wish = wn_v3_add(wn_v3_scale(cf, my), wn_v3_scale(cr, -mx));
    float speed = wn_frame_down(in, WN_ACT_RUN) ? WN_PLAYER_RUN : WN_PLAYER_WALK;
    speed *= wn_snow_speed_factor(&g->snow, &g->world, wn_v3_add(p->pos, wn_v3(0, 0.3f, 0)));
    p->blocking = wn_frame_down(in, WN_ACT_BLOCK);
    if (p->swing > 0.0f || p->blocking) speed *= 0.35f;
    if (len > 0.05f) {
        float target = wn_atan2f(wish.x, wish.z);
        float d = target - p->yaw;
        while (d > WN_PI) d -= 2.0f * WN_PI;
        while (d < -WN_PI) d += 2.0f * WN_PI;
        p->yaw += d * WN_MIN(1.0f, 12.0f * dt);
    }
    p->vel.x = wish.x * speed;
    p->vel.z = wish.z * speed;
    /* Jump and gravity. */
    if (p->on_ground && wn_frame_pressed(in, WN_ACT_JUMP)) {
        p->vel.y = WN_PLAYER_JUMP;
        p->on_ground = false;
    }
    p->vel.y -= 9.81f * dt;
    /* Horizontal moves, axis by axis, with step-up. */
    const float step_up = 0.35f;
    for (int axis = 0; axis < 2; axis++) {
        wn_vec3 np = p->pos;
        if (axis == 0) np.x += p->vel.x * dt;
        else np.z += p->vel.z * dt;
        /* Stay inside the playable area. */
        np.x = WN_CLAMP(np.x, g->scene.bounds_mn.x, g->scene.bounds_mx.x);
        np.z = WN_CLAMP(np.z, g->scene.bounds_mn.z, g->scene.bounds_mx.z);
        /* Never walk into a column that is not streamed in yet. */
        wn_ivec3 nv = wn_world_to_voxel(np);
        if (!wn_world_column_loaded(&g->world, nv.x >> WN_CHUNK_BITS, nv.z >> WN_CHUNK_BITS)) continue;
        if (!body_blocked(g, np, step_up)) p->pos = np;
    }
    /* Vertical (paused while the column under the player is still loading). */
    wn_ivec3 pv = wn_world_to_voxel(p->pos);
    if (!wn_world_column_loaded(&g->world, pv.x >> WN_CHUNK_BITS, pv.z >> WN_CHUNK_BITS)) p->vel.y = 0.0f;
    float ground = support_height(g, p->pos.x, p->pos.z, p->pos.y);
    if (ground < -1e8f) ground = p->pos.y; /* nothing below in the loaded world: hold still */
    p->pos.y += p->vel.y * dt;
    if (p->pos.y <= ground) {
        p->pos.y = ground;
        p->vel.y = 0.0f;
        p->on_ground = true;
    } else if (p->pos.y > ground + 0.05f) {
        p->on_ground = false;
    }
    /* Footprints and walking animation. */
    float moved = wn_sqrtf(p->vel.x * p->vel.x + p->vel.z * p->vel.z) * dt;
    p->walk_phase += moved * 3.2f;
    p->step_dist += moved;
    if (p->step_dist > 0.38f && p->on_ground) {
        p->step_dist = 0.0f;
        p->foot ^= 1;
        wn_vec3 right = wn_v3(wn_cosf(p->yaw), 0, -wn_sinf(p->yaw));
        wn_vec3 foot = wn_v3_add(p->pos, wn_v3_scale(right, p->foot ? 0.13f : -0.13f));
        wn_edit_ctx e;
        wn_edit_begin(&e, &g->world);
        wn_snow_footprint(&g->snow, &e, foot, 0.11f);
        wn_edit_end(&e);
    }
    /* Attack. */
    if (p->swing <= 0.0f && wn_frame_pressed(in, WN_ACT_ATTACK)) {
        p->swing = dt;
        p->swing_hit_done = false;
        p->swing_power = 1.0f;
    } else if (p->swing > 0.0f) {
        p->swing += dt;
        if (!p->swing_hit_done && p->swing >= 0.2f) {
            melee_hit(g);
            p->swing_hit_done = true;
        }
        if (p->swing >= 0.45f) {
            p->swing = 0.0f;
            p->swing_count++;
        }
    }
    if (p->brush_cooldown > 0.0f) p->brush_cooldown -= dt;
    if (len > 0.05f) brush_trees(g);
}

/* ======================= Triggers & events ======================= */

int wn_game_fire_event(wn_game *g, const char *event)
{
    int n = 0;
    for (int i = 0; i < g->scene.nactions; i++) {
        wn_event_action *a = &g->scene.actions[i];
        if (strcmp(a->event, event) != 0) continue;
        n++;
        switch (a->kind) {
        case WN_EVT_COMPLETE: g->complete = true; break;
        case WN_EVT_FIRE_COLOR: wn_atmos_fade_fire(&g->atmos, a->color, a->value); break;
        case WN_EVT_SNOWFALL:
            g->snow.rate = a->value;
            g->fx.snowfall = a->value > 0.0f ? g->scene.snowfall_flakes : 0.0f;
            break;
        default:
            snprintf(g->message, sizeof g->message, "%s", a->text);
            g->message_time = 4.0f;
            break;
        }
    }
    if (n) g->events_fired++;
    return n;
}

static void update_triggers(wn_game *g)
{
    wn_vec3 p = g->player.pos;
    for (int i = 0; i < g->scene.ntriggers; i++) {
        wn_trigger *t = &g->scene.triggers[i];
        if (t->fired) continue;
        /* Trigger boxes are footprints on the ground: only x/z are tested,
         * because the terrain height varies across a scene. */
        if (p.x >= t->mn.x && p.x <= t->mx.x && p.z >= t->mn.z && p.z <= t->mx.z) {
            t->fired = true;
            wn_game_fire_event(g, t->event);
        }
    }
}

/* ======================= Step ======================= */

void wn_game_step(wn_game *g, const wn_input_frame *in)
{
    g->prev_player = g->player;
    update_player(g, in);
    update_triggers(g);
    wn_debris_step(&g->debris, &g->world, WN_SIM_DT);
    wn_fx_step(&g->fx, &g->world, g->player.pos, WN_SIM_DT);
    wn_snow_step(&g->snow, &g->world, WN_SIM_DT);
    wn_atmos_step(&g->atmos, WN_SIM_DT);
    if (g->message_time > 0.0f) g->message_time -= WN_SIM_DT;
    g->step++;
}

uint32_t wn_game_checksum(wn_game *g)
{
    wn_hasher h;
    wn_hasher_init(&h, 3u);
    wn_hasher_add(&h, &g->player.pos, sizeof g->player.pos);
    wn_hasher_add(&h, &g->player.yaw, sizeof g->player.yaw);
    wn_hasher_add(&h, &g->step, sizeof g->step);
    wn_hasher_add(&h, &g->complete, sizeof g->complete);
    uint32_t parts[4] = {g->world.edit_serial, wn_debris_checksum(&g->debris), wn_fx_checksum(&g->fx),
                         g->events_fired};
    wn_hasher_add(&h, parts, sizeof parts);
    return wn_hasher_finish(&h);
}

/* ======================= Camera ======================= */

wn_camera wn_game_camera(const wn_game *gc, float alpha)
{
    wn_game *g = (wn_game *)gc; /* raycasts read the world cache */
    wn_vec3 pos = wn_v3_lerp(g->prev_player.pos, g->player.pos, alpha);
    wn_camera c;
    c.yaw = g->cam.yaw;
    c.pitch = g->cam.pitch;
    c.fov_y = 1.05f;
    c.znear = 0.08f;
    c.zfar = 400.0f;
    wn_vec3 head = wn_v3_add(pos, wn_v3(0, 1.65f, 0));
    if (g->cam.fixed) {
        c.pos = g->cam.fixed_pos;
        return c;
    }
    if (g->cam.first_person) {
        c.pos = head;
        return c;
    }
    wn_vec3 fwd = wn_camera_forward(&c);
    wn_vec3 right = wn_v3(wn_cosf(c.yaw), 0, -wn_sinf(c.yaw));
    wn_vec3 pivot = wn_v3_add(head, wn_v3_scale(right, -g->cam.shoulder));
    wn_vec3 back = wn_v3_scale(fwd, -1.0f);
    wn_ray_hit hit = wn_world_raycast(&g->world, pivot, back, g->cam.distance + 0.2f);
    float d = g->cam.distance;
    if (hit.hit) d = WN_MAX(0.3f, hit.distance - 0.25f);
    g->cam.actual_dist = d;
    c.pos = wn_v3_add(pivot, wn_v3_scale(back, d));
    return c;
}

void wn_game_pose_warrior(wn_game *g, float alpha)
{
    wn_model *m = &g->warrior;
    wn_vec3 pos = wn_v3_lerp(g->prev_player.pos, g->player.pos, alpha);
    m->pos = wn_v3_add(pos, wn_v3(0, 0.8f, 0)); /* pelvis bottom at leg length */
    m->yaw = g->player.yaw;
    float ph = g->player.walk_phase;
    float swing = wn_sinf(ph) * 0.6f;
    int legl = wn_model_find_part(m, "leg_l"), legr = wn_model_find_part(m, "leg_r");
    int arml = wn_model_find_part(m, "arm_l"), armr = wn_model_find_part(m, "arm_r");
    int torso = wn_model_find_part(m, "torso");
    if (legl >= 0) m->parts[legl].rx = swing;
    if (legr >= 0) m->parts[legr].rx = -swing;
    if (arml >= 0) m->parts[arml].rx = -swing * 0.6f;
    if (armr >= 0) {
        float s = g->player.swing;
        if (s > 0.0f) {
            /* raise then chop down */
            float t = s / 0.45f;
            m->parts[armr].rx = t < 0.4f ? -2.6f * (t / 0.4f) : -2.6f + 3.4f * ((t - 0.4f) / 0.6f);
        } else if (g->player.blocking) {
            m->parts[armr].rx = -1.2f;
        } else {
            m->parts[armr].rx = swing * 0.6f - 0.25f;
        }
    }
    if (torso >= 0) m->parts[torso].rx = g->player.swing > 0.0f ? 0.25f : 0.05f;
}

/* ======================= Helpers ======================= */

void wn_game_teleport(wn_game *g, wn_vec3 pos, float yaw)
{
    g->player.pos = pos;
    g->player.vel = wn_v3(0, 0, 0);
    g->player.yaw = yaw;
    g->cam.yaw = yaw;
    g->prev_player = g->player;
}

void wn_game_spill_blood(wn_game *g, wn_vec3 pos, float amount)
{
    wn_fx_wound(&g->fx, pos, wn_v3(0.3f, 0.6f, 0.2f), amount, true);
    wn_edit_ctx e;
    wn_edit_begin(&e, &g->world);
    wn_fx_blood_pool(&g->fx, &e, pos, (int)(amount * 2.0f));
    wn_edit_end(&e);
}

bool wn_game_xr_swing(wn_game *g, float speed)
{
    float power = wn_xr_swing_damage(speed);
    if (power <= 0.0f || g->player.swing > 0.0f) return false;
    g->player.swing = WN_SIM_DT;
    g->player.swing_hit_done = false;
    g->player.swing_power = power;
    return true;
}
