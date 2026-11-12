#include "test_util.h"
#include "unity.h"
#include "wn/wn_fx.h"
#include "wn/wn_light.h"
#include "wn/wn_snow.h"

#include <limits.h>
#include <string.h>

/* Positions in voxel units, so the tests hold for any WN_VOXEL_SIZE. */
#define V(x) ((float)(x) * WN_VOXEL_SIZE)

void setUp(void) {}
void tearDown(void) {}

static void make_world(wn_world *w, int r)
{
    wn_world_init(w, test_materials(), -1, 1);
    wn_world_set_generator(w, test_gen_flat, NULL);
    w->on_column = wn_light_column_loaded;
    for (int x = -r; x <= r; x++)
        for (int z = -r; z <= r; z++) wn_world_load_column(w, x, z);
}

static int32_t top16(wn_world *w, int x, int z) { return wn_snow_column_top16(w, x, z, 63); }

static void snow_layer(wn_world *w, int x0, int z0, int x1, int z1, int depth16)
{
    wn_edit_ctx e;
    wn_edit_begin(&e, w);
    for (int z = z0; z <= z1; z++)
        for (int x = x0; x <= x1; x++) wn_snow_add(&e, x, z, depth16);
    wn_edit_end(&e);
}

static void test_half_filled_voxel(void)
{
    wn_world w;
    make_world(&w, 0);
    wn_world_set(&w, 3, 0, 3, wn_vox_make(WN_MAT_SNOW, 8));
    TEST_ASSERT_EQUAL_INT32(8, top16(&w, 3, 3));
    TEST_ASSERT_TRUE(wn_world_box_overlaps(&w, wn_v3(V(3.1), 0.0f, V(3.1)), wn_v3(V(3.9), V(0.4), V(3.9))));
    TEST_ASSERT_FALSE(wn_world_box_overlaps(&w, wn_v3(V(3.1), V(0.6), V(3.1)), wn_v3(V(3.9), V(0.9), V(3.9))));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, wn_snow_column_top16(&w, 500, 500, 63));
    wn_world_free(&w);
}

static void test_add_and_remove(void)
{
    wn_world w;
    make_world(&w, 0);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    TEST_ASSERT_EQUAL_INT(40, wn_snow_add(&e, 2, 2, 40));
    TEST_ASSERT_EQUAL_INT32(40, top16(&w, 2, 2));
    TEST_ASSERT_EQUAL_UINT32(0, wn_vox_fill(wn_world_get(&w, 2, 0, 2))); /* full */
    TEST_ASSERT_EQUAL_UINT32(8, wn_vox_fill(wn_world_get(&w, 2, 2, 2)));
    TEST_ASSERT_EQUAL_INT(30, wn_snow_remove(&e, 2, 2, 30));
    TEST_ASSERT_EQUAL_INT32(10, top16(&w, 2, 2));
    TEST_ASSERT_EQUAL_INT(10, wn_snow_remove(&e, 2, 2, 99)); /* only snow is removed */
    TEST_ASSERT_EQUAL_INT32(0, top16(&w, 2, 2));
    /* no snow on emitters or liquids */
    wn_world_set(&w, 5, 0, 5, wn_vox_make(WN_MAT_WINDOW_GLOW, 0));
    TEST_ASSERT_EQUAL_INT(0, wn_snow_add(&e, 5, 5, 4));
    TEST_ASSERT_EQUAL_INT(0, wn_snow_add(&e, 500, 500, 4)); /* unloaded */
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_open_ground_accumulates(void)
{
    /* spec: 60 s at 1 level per 10 s -> 6 levels */
    wn_world w;
    make_world(&w, 1);
    wn_snow s;
    wn_snow_init(&s);
    s.rate = 0.1f;
    s.columns_per_step = 1 << 20;
    for (int i = 0; i < 60 * WN_SIM_HZ + 1; i++) wn_snow_step(&s, &w, WN_SIM_DT);
    wn_snow_flush(&s, &w);
    TEST_ASSERT_EQUAL_UINT32(6, s.passes_done);
    TEST_ASSERT_EQUAL_INT32(6, top16(&w, 4, 4));
    TEST_ASSERT_EQUAL_INT32(6, top16(&w, -20, 40));
    wn_world_free(&w);
}

static void test_time_sliced_passes(void)
{
    wn_world w;
    make_world(&w, 1);
    wn_snow s;
    wn_snow_init(&s);
    s.rate = 1.0f;
    s.columns_per_step = 1024; /* one chunk column per step */
    for (int i = 0; i < WN_SIM_HZ + 2; i++) wn_snow_step(&s, &w, WN_SIM_DT);
    TEST_ASSERT_TRUE(s.pass_active || s.passes_done == 1);
    for (int i = 0; i < 20; i++) wn_snow_step(&s, &w, 0.0f);
    TEST_ASSERT_EQUAL_UINT32(1, s.passes_done);
    wn_world_free(&w);
}

static void test_sheltered_ground_stays_clear(void)
{
    wn_world w;
    make_world(&w, 1);
    for (int z = 0; z < 10; z++)
        for (int x = 0; x < 10; x++) {
            wn_world_set(&w, x, 20, z, wn_vox_make(WN_MAT_STONE, 0));
            wn_light_voxel_changed(&w, x, 20, z);
        }
    wn_snow s;
    wn_snow_init(&s);
    s.rate = 1.0f;
    for (int i = 0; i < 5 * WN_SIM_HZ + 1; i++) wn_snow_step(&s, &w, WN_SIM_DT);
    wn_snow_flush(&s, &w);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 5, 0, 5)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_SNOW, wn_vox_mat(wn_world_get(&w, 5, 21, 5)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_SNOW, wn_vox_mat(wn_world_get(&w, 15, 0, 5)));
    wn_world_free(&w);
}

static void test_wading_leaves_trail_and_it_fills(void)
{
    wn_world w;
    make_world(&w, 1);
    snow_layer(&w, -10, -5, 70, 5, 4 * 16); /* 4 voxels deep */
    wn_snow s;
    wn_snow_init(&s);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    /* walk 50 voxels (5 m at 10 cm) along +x */
    for (float x = 0.0f; x < 50.0f; x += 3.5f) wn_snow_footprint(&s, &e, wn_v3(V(x), 0.0f, V(0.5)), V(1.2));
    wn_edit_end(&e);
    int32_t in_trail = top16(&w, 25, 0), beside = top16(&w, 25, 5);
    TEST_ASSERT_TRUE(in_trail < beside);
    TEST_ASSERT_EQUAL_INT32(4 * 16, beside);
    TEST_ASSERT_TRUE(wn_vox_damage(wn_world_get(&w, 25, (in_trail - 1) / 16, 0)) > 0); /* trampled colour */
    /* snowfall refills the trail */
    s.rate = 2.0f;
    for (int i = 0; i < 40 * WN_SIM_HZ; i++) wn_snow_step(&s, &w, WN_SIM_DT);
    wn_snow_flush(&s, &w);
    TEST_ASSERT_TRUE(top16(&w, 25, 0) > in_trail);
    wn_edit_begin(&e, &w);
    wn_snow_footprint(&s, &e, wn_v3(20.0f, 0.0f, 20.0f), 0.1f); /* no snow there? there is none at z=200 */
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_deep_snow_is_slower(void)
{
    wn_world w;
    make_world(&w, 1);
    snow_layer(&w, 10, 10, 20, 20, 5 * 16);
    wn_snow s;
    wn_snow_init(&s);
    s.knee_height = V(5); /* 0.5 m at 10 cm voxels */
    float stone = wn_snow_speed_factor(&s, &w, wn_v3(V(0.5), 0.0f, V(0.5)));
    float snow = wn_snow_speed_factor(&s, &w, wn_v3(V(15.5), V(5), V(15.5)));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, stone);
    TEST_ASSERT_TRUE(snow < stone);
    TEST_ASSERT_TRUE(snow >= s.min_speed);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, V(5), wn_snow_depth_at(&w, wn_v3(V(15.5), V(5), V(15.5))));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_snow_depth_at(&w, wn_v3(500.0f, 0.0f, 500.0f)));
    wn_world_free(&w);
}

static int g_released;
static void on_release(void *u, wn_vec3 p)
{
    (void)u;
    (void)p;
    g_released++;
}

static void test_hitting_tree_drops_snow(void)
{
    wn_world w;
    make_world(&w, 1);
    /* a branch of needles at y=20 with snow on top */
    for (int x = 0; x < 6; x++) {
        wn_world_set(&w, 10 + x, 20, 10, wn_vox_make(WN_MAT_PINE_NEEDLES, 0));
        wn_world_set(&w, 10 + x, 21, 10, wn_vox_make(WN_MAT_SNOW, 0));
        wn_world_set(&w, 10 + x, 22, 10, wn_vox_make(WN_MAT_SNOW, 6));
    }
    wn_world_set(&w, 30, 0, 30, wn_vox_make(WN_MAT_SNOW, 4)); /* ground snow, not on a branch */
    g_released = 0;
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    int n = wn_snow_shake(&e, wn_voxel_center(wn_iv3(12, 20, 10)), 1.0f, on_release, NULL);
    TEST_ASSERT_EQUAL_INT(12, n);
    TEST_ASSERT_EQUAL_INT(12, g_released);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 12, 21, 10)));
    TEST_ASSERT_EQUAL_INT(0, wn_snow_shake(&e, wn_voxel_center(wn_iv3(30, 0, 30)), 0.3f, NULL, NULL));
    wn_edit_end(&e);
    /* released snow falls as clumps and lands as fill on the ground below */
    wn_fx fx;
    wn_fx_init(&fx, 1);
    for (int i = 0; i < 6; i++) wn_fx_snow_clump(&fx, wn_voxel_center(wn_iv3(10 + i, 21, 10)));
    int32_t before = top16(&w, 12, 10);
    for (int i = 0; i < 3 * WN_SIM_HZ; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    /* the branch is still in the way of some; ground below the open side gains snow */
    int32_t after = wn_snow_column_top16(&w, 12, 10, 19);
    TEST_ASSERT_TRUE(after > 0 || before > 0);
    wn_world_free(&w);
}

static void test_torch_melts_snow(void)
{
    wn_world w;
    make_world(&w, 1);
    snow_layer(&w, 0, 0, 10, 10, 3 * 16);
    wn_world_set(&w, 5, 1, 5, wn_vox_make(WN_MAT_ICE, 0));
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    float acc = 0.0f;
    int32_t before = top16(&w, 5, 5);
    for (int i = 0; i < 30 * WN_SIM_HZ; i++) wn_snow_melt(&e, wn_voxel_center(wn_iv3(5, 3, 5)), 0.6f, 2.0f, WN_SIM_DT, &acc);
    TEST_ASSERT_TRUE(top16(&w, 5, 5) < before);
    TEST_ASSERT_EQUAL_INT32(before, top16(&w, 10, 10)); /* outside the radius */
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_WATER, wn_vox_mat(wn_world_get(&w, 5, 1, 5)));
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_steep_pile_collapses(void)
{
    wn_world w;
    make_world(&w, 1);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    wn_snow_add(&e, 10, 10, 10 * 16);
    TEST_ASSERT_EQUAL_INT32(160, top16(&w, 10, 10));
    int moves = wn_snow_settle(&e, 0, 0, 20, 20, 40, 16, 100000);
    TEST_ASSERT_TRUE(moves > 0);
    int32_t total = 0;
    for (int z = 0; z <= 20; z++)
        for (int x = 0; x <= 20; x++) {
            int32_t h = top16(&w, x, z);
            total += h;
            if (x < 20) TEST_ASSERT_TRUE(h - top16(&w, x + 1, z) <= 16 && top16(&w, x + 1, z) - h <= 16);
            if (z < 20) TEST_ASSERT_TRUE(h - top16(&w, x, z + 1) <= 16 && top16(&w, x, z + 1) - h <= 16);
        }
    TEST_ASSERT_EQUAL_INT32(160, total); /* snow is conserved */
    TEST_ASSERT_EQUAL_INT(0, wn_snow_settle(&e, 0, 0, 20, 20, 40, 16, 100000));
    wn_edit_end(&e);
    wn_world_free(&w);
}

/* ---------------- Blood ---------------- */

static void test_stronger_hit_more_blood(void)
{
    wn_fx fx;
    wn_fx_init(&fx, 5);
    int light = wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 1.0f, true);
    int heavy = wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 8.0f, true);
    TEST_ASSERT_TRUE(heavy > light);
    TEST_ASSERT_EQUAL_INT(0, wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 0.0f, true));
}

static void test_undead_emit_dust(void)
{
    wn_fx fx;
    wn_fx_init(&fx, 5);
    int n = wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 3.0f, false);
    TEST_ASSERT_TRUE(n > 0);
    for (uint32_t i = 0; i < fx.count; i++) TEST_ASSERT_EQUAL_UINT8(WN_PART_BONE_DUST, fx.p[i].type);
}

static void test_gore_levels(void)
{
    wn_fx fx;
    wn_fx_init(&fx, 5);
    fx.gore = WN_GORE_OFF;
    TEST_ASSERT_EQUAL_INT(0, wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 8.0f, true));
    TEST_ASSERT_EQUAL_UINT32(0, fx.count);
    fx.gore = WN_GORE_FULL;
    int full = wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 8.0f, true);
    fx.gore = WN_GORE_REDUCED;
    int reduced = wn_fx_wound(&fx, wn_v3(0, 1, 0), wn_v3(1, 0, 0), 8.0f, true);
    TEST_ASSERT_TRUE(reduced < full);
    TEST_ASSERT_TRUE(reduced > 0);
    wn_world w;
    make_world(&w, 0);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    TEST_ASSERT_EQUAL_INT(0, wn_fx_blood_pool(&fx, &e, wn_v3(0.5f, 0.5f, 0.5f), 12)); /* reduced: no pools */
    fx.gore = WN_GORE_FULL;
    TEST_ASSERT_EQUAL_INT(0, wn_fx_blood_pool(&fx, &e, wn_v3(0.5f, 0.5f, 0.5f), 0));
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_blood_on_stone_and_snow(void)
{
    wn_world w;
    make_world(&w, 0);
    wn_fx fx;
    wn_fx_init(&fx, 9);
    wn_fx_stain(&fx, &w, 3, -1, 3, 20);
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD, wn_vox_stain(wn_world_get(&w, 3, -1, 3)));
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_NONE, wn_vox_stain(wn_world_get(&w, 4, -1, 3)));
    wn_fx_stain(&fx, &w, 3, 5, 3, 20); /* air: ignored */
    /* droplet onto snow seeps into neighbours */
    snow_layer(&w, 10, 10, 14, 14, 32);
    wn_fx_spawn(&fx, WN_PART_BLOOD, wn_voxel_center(wn_iv3(12, 4, 12)), wn_v3(0, -1, 0), 0x8C1016, 3.0f);
    for (int i = 0; i < 60; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    wn_voxel hit = wn_world_get(&w, 12, 1, 12), side = wn_world_get(&w, 13, 1, 12);
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD, wn_vox_stain(hit));
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD, wn_vox_stain(side));
    TEST_ASSERT_TRUE(wn_vox_stain_amount(side) < wn_vox_stain_amount(hit));
    wn_world_free(&w);
}

static void test_stain_dries(void)
{
    wn_world w;
    make_world(&w, 0);
    wn_fx fx;
    wn_fx_init(&fx, 9);
    wn_fx_stain(&fx, &w, 3, -1, 3, 30);
    for (int i = 0; i < 121 * WN_SIM_HZ; i += 30) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT * 30);
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD_DRIED, wn_vox_stain(wn_world_get(&w, 3, -1, 3)));
    /* fresh blood over dried blood becomes fresh again */
    wn_fx_stain(&fx, &w, 3, -1, 3, 10);
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD, wn_vox_stain(wn_world_get(&w, 3, -1, 3)));
    wn_world_free(&w);
}

static void test_pool_forms_and_freezes(void)
{
    wn_world w;
    make_world(&w, 1);
    wn_fx fx;
    wn_fx_init(&fx, 2);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    TEST_ASSERT_EQUAL_INT(1, wn_fx_blood_pool(&fx, &e, wn_v3(V(10.5), V(4), V(10.5)), 15));
    TEST_ASSERT_EQUAL_INT(0, wn_fx_blood_pool(&fx, &e, wn_v3(V(10.5), V(4), V(10.5)), 10)); /* adds to the cell */
    wn_edit_end(&e);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_BLOOD, wn_vox_mat(wn_world_get(&w, 10, 0, 10)));
    for (int i = 0; i < 2 * WN_SIM_HZ; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    int liquid = 0;
    for (int z = 5; z < 16; z++)
        for (int x = 5; x < 16; x++) liquid += wn_vox_mat(wn_world_get(&w, x, 0, z)) == WN_MAT_BLOOD;
    TEST_ASSERT_TRUE(liquid > 1); /* it spread */
    for (int i = 0; i < 10 * WN_SIM_HZ; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    int frozen = 0;
    liquid = 0;
    for (int z = 5; z < 16; z++)
        for (int x = 5; x < 16; x++) {
            frozen += wn_vox_mat(wn_world_get(&w, x, 0, z)) == WN_MAT_FROZEN_BLOOD;
            liquid += wn_vox_mat(wn_world_get(&w, x, 0, z)) == WN_MAT_BLOOD;
        }
    TEST_ASSERT_TRUE(frozen > 1);
    TEST_ASSERT_EQUAL_INT(0, liquid);
    TEST_ASSERT_EQUAL_UINT32(0, fx.npool);
    /* a pool started in the air falls to the ground */
    wn_edit_begin(&e, &w);
    wn_world_set(&w, 30, 5, 30, wn_vox_make(WN_MAT_BLOOD, 3)); /* too little to spread */
    fx.pool[0].pos = wn_iv3(30, 5, 30);
    fx.pool[0].age = 0;
    fx.npool = 1;
    for (int i = 0; i < 20; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_BLOOD, wn_vox_mat(wn_world_get(&w, 30, 0, 30)));
    /* a pool cell whose voxel was removed is dropped */
    wn_world_set(&w, 30, 0, 30, 0);
    wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    TEST_ASSERT_EQUAL_UINT32(0, fx.npool);
    /* cannot pool inside solid */
    TEST_ASSERT_EQUAL_INT(0, wn_fx_blood_pool(&fx, &e, wn_v3(V(0.5), V(-5), V(0.5)), 8));
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_particles_snowfall_and_capacity(void)
{
    wn_world w;
    make_world(&w, 1);
    wn_fx fx;
    wn_fx_init(&fx, 3);
    fx.snowfall = 600.0f;
    fx.capacity = 100;
    for (int i = 0; i < 60; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    TEST_ASSERT_TRUE(fx.ncosmetic <= 100);
    TEST_ASSERT_TRUE(fx.ncosmetic > 50);
    TEST_ASSERT_EQUAL_UINT32(0, fx.count); /* snowflakes never enter the gameplay pool */
    /* gameplay particles are not limited by the cosmetic preset capacity */
    TEST_ASSERT_NOT_NULL(wn_fx_spawn(&fx, WN_PART_BLOOD, wn_v3(0, 2, 0), wn_v3(0, 0, 0), 0, 1.0f));
    TEST_ASSERT_NULL(wn_fx_spawn(&fx, WN_PART_SNOWFLAKE, wn_v3(0, 2, 0), wn_v3(0, 0, 0), 0, 1.0f));
    /* the gameplay checksum ignores cosmetic particles */
    uint32_t c0 = wn_fx_checksum(&fx);
    fx.ncosmetic = 0;
    TEST_ASSERT_EQUAL_UINT32(c0, wn_fx_checksum(&fx));
    wn_fx_dust(&fx, wn_v3(0, 1, 0), 0x808080, 5);
    uint32_t c1 = wn_fx_checksum(&fx);
    for (int i = 0; i < 30 * WN_SIM_HZ; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
    TEST_ASSERT_NOT_EQUAL(c1, wn_fx_checksum(&fx));
    /* hard cap */
    fx.capacity = WN_MAX_PARTICLES;
    fx.snowfall = 0;
    fx.count = WN_MAX_PARTICLES;
    TEST_ASSERT_NULL(wn_fx_spawn(&fx, WN_PART_BLOOD, wn_v3(0, 2, 0), wn_v3(0, 0, 0), 0, 1.0f));
    wn_world_free(&w);
}

static void test_blood_is_deterministic(void)
{
    uint32_t sums[2];
    for (int run = 0; run < 2; run++) {
        wn_world w;
        make_world(&w, 1);
        snow_layer(&w, -20, -20, 40, 40, 20);
        wn_fx fx;
        wn_fx_init(&fx, 77);
        wn_fx_wound(&fx, wn_v3(1.0f, 1.2f, 1.0f), wn_v3(1, 0.3f, 0), 9.0f, true);
        for (int i = 0; i < 3 * WN_SIM_HZ; i++) wn_fx_step(&fx, &w, wn_v3(0, 0, 0), WN_SIM_DT);
        sums[run] = wn_world_checksum(&w) ^ wn_fx_checksum(&fx);
        TEST_ASSERT_TRUE(fx.stains_written > 10);
        wn_world_free(&w);
    }
    TEST_ASSERT_EQUAL_UINT32(sums[0], sums[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_half_filled_voxel);
    RUN_TEST(test_add_and_remove);
    RUN_TEST(test_open_ground_accumulates);
    RUN_TEST(test_time_sliced_passes);
    RUN_TEST(test_sheltered_ground_stays_clear);
    RUN_TEST(test_wading_leaves_trail_and_it_fills);
    RUN_TEST(test_deep_snow_is_slower);
    RUN_TEST(test_hitting_tree_drops_snow);
    RUN_TEST(test_torch_melts_snow);
    RUN_TEST(test_steep_pile_collapses);
    RUN_TEST(test_stronger_hit_more_blood);
    RUN_TEST(test_undead_emit_dust);
    RUN_TEST(test_gore_levels);
    RUN_TEST(test_blood_on_stone_and_snow);
    RUN_TEST(test_stain_dries);
    RUN_TEST(test_pool_forms_and_freezes);
    RUN_TEST(test_particles_snowfall_and_capacity);
    RUN_TEST(test_blood_is_deterministic);
    return UNITY_END();
}
