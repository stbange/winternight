#include "test_util.h"
#include "unity.h"
#include "wn/wn_edit.h"
#include "wn/wn_light.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void make_world(wn_world *w)
{
    wn_world_init(w, test_materials(), -1, 2);
    wn_world_set_generator(w, test_gen_flat, NULL);
    w->on_column = wn_light_column_loaded;
    for (int x = -1; x <= 1; x++)
        for (int z = -1; z <= 1; z++) wn_world_load_column(w, x, z);
}

static int g_destroyed_cb;
static void on_destroy(void *u, wn_ivec3 p, wn_voxel v)
{
    (void)u;
    (void)p;
    (void)v;
    g_destroyed_cb++;
}

static void test_soft_material_breaks_first(void)
{
    wn_world w;
    make_world(&w);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    e.on_destroy = on_destroy;
    g_destroyed_cb = 0;
    wn_world_set(&w, 1, 0, 1, wn_vox_make(WN_MAT_SNOW, 0));
    wn_world_set(&w, 3, 0, 1, wn_vox_make(WN_MAT_STONE, 0));
    TEST_ASSERT_TRUE(wn_edit_damage(&e, 1, 0, 1, 1.0f));
    TEST_ASSERT_FALSE(wn_edit_damage(&e, 3, 0, 1, 1.0f));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 1, 0, 1)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_STONE, wn_vox_mat(wn_world_get(&w, 3, 0, 1)));
    TEST_ASSERT_TRUE(wn_vox_damage(wn_world_get(&w, 3, 0, 1)) > 0);
    TEST_ASSERT_EQUAL_INT(1, g_destroyed_cb);
    TEST_ASSERT_EQUAL_UINT32(1, e.destroyed);
    TEST_ASSERT_EQUAL_UINT32(1, e.damaged);
    /* air, anchors and zero damage are ignored */
    TEST_ASSERT_FALSE(wn_edit_damage(&e, 5, 5, 5, 100.0f));
    TEST_ASSERT_FALSE(wn_edit_damage(&e, 3, 0, 1, 0.0f));
    wn_world_set(&w, 4, 0, 1, wn_vox_make(WN_MAT_BEDROCK, 0));
    TEST_ASSERT_FALSE(wn_edit_damage(&e, 4, 0, 1, 1e6f));
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_repeated_hits_destroy(void)
{
    wn_world w;
    make_world(&w);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    wn_world_set(&w, 2, 0, 2, wn_vox_make(WN_MAT_PINE_WOOD, 0)); /* hardness 4 */
    int hits = 0;
    while (!wn_edit_damage(&e, 2, 0, 2, 1.0f) && hits < 20) hits++;
    TEST_ASSERT_EQUAL_INT(3, hits); /* 4th hit destroys */
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 2, 0, 2)));
    /* zero-hardness solids break immediately */
    wn_materials soft = *test_materials();
    soft.m[WN_MAT_ICE].hardness = 0.0f;
    w.mats = &soft;
    wn_world_set(&w, 6, 0, 6, wn_vox_make(WN_MAT_ICE, 0));
    TEST_ASSERT_TRUE(wn_edit_damage(&e, 6, 0, 6, 0.01f));
    w.mats = test_materials();
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void fill_box(wn_world *w, int x0, int y0, int z0, int x1, int y1, int z1, uint32_t mat)
{
    for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) wn_world_set(w, x, y, z, wn_vox_make(mat, 0));
}

static void test_sword_swing_carves_slice(void)
{
    wn_world w;
    make_world(&w);
    fill_box(&w, 0, 0, 0, 20, 8, 20, WN_MAT_SNOW);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    float v = WN_VOXEL_SIZE;
    wn_edit_capsule(&e, wn_v3(2 * v, 5 * v, 10 * v), wn_v3(18 * v, 5 * v, 10 * v), 1.2f * v, 5.0f);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 10, 5, 10)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_SNOW, wn_vox_mat(wn_world_get(&w, 10, 5, 13)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_SNOW, wn_vox_mat(wn_world_get(&w, 10, 2, 10)));
    TEST_ASSERT_TRUE(e.ntouched > 10);
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void test_explosion_falloff(void)
{
    wn_world w;
    make_world(&w);
    fill_box(&w, 0, 0, 10, 30, 20, 11, WN_MAT_PINE_WOOD);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    wn_vec3 c = wn_voxel_center(wn_iv3(15, 10, 10));
    wn_edit_sphere(&e, c, 10 * WN_VOXEL_SIZE, 8.0f);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 15, 10, 10)));
    wn_voxel edge = wn_world_get(&w, 15, 10 + 8, 10);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_PINE_WOOD, wn_vox_mat(edge));
    TEST_ASSERT_TRUE(wn_vox_damage(edge) > 0);
    wn_edit_box(&e, wn_voxel_to_world(wn_iv3(0, 0, 10)), wn_voxel_to_world(wn_iv3(3, 3, 12)), 100.0f);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 1, 1, 10)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_PINE_WOOD, wn_vox_mat(wn_world_get(&w, 4, 1, 10)));
    wn_edit_fill_sphere(&e, wn_voxel_center(wn_iv3(1, 1, 10)), 1.5f * WN_VOXEL_SIZE, wn_vox_make(WN_MAT_ICE, 0));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_ICE, wn_vox_mat(wn_world_get(&w, 1, 1, 10)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_PINE_WOOD, wn_vox_mat(wn_world_get(&w, 5, 1, 10))); /* not air: kept */
    wn_edit_end(&e);
    wn_world_free(&w);
}

static void build_tree(wn_world *w, int x, int z, int h)
{
    for (int y = 0; y < h; y++) wn_world_set(w, x, y, z, wn_vox_make(WN_MAT_PINE_WOOD, 0));
    fill_box(w, x - 3, h - 4, z - 3, x + 3, h - 1, z + 3, WN_MAT_PINE_NEEDLES);
    for (int y = 0; y < h; y++) wn_light_voxel_changed(w, x, y, z);
}

static void test_cutting_tree_trunk_makes_debris(void)
{
    wn_world w;
    make_world(&w);
    build_tree(&w, 10, 10, 30);
    wn_debris_set ds;
    wn_debris_init(&ds);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    wn_edit_damage(&e, 10, 3, 10, 100.0f);
    ds.detach_push = wn_v3(2.0f, 0.0f, 0.0f); /* the swing pushes it over */
    int made = wn_debris_detach(&ds, &e);
    TEST_ASSERT_EQUAL_INT(1, made);
    TEST_ASSERT_EQUAL_UINT32(1, wn_debris_count(&ds));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 10, 20, 10)));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_PINE_WOOD, wn_vox_mat(wn_world_get(&w, 10, 1, 10))); /* stump stays */
    wn_debris *d = NULL;
    for (int i = 0; i < WN_MAX_DEBRIS; i++)
        if (ds.d[i].active) d = &ds.d[i];
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL_UINT32(26 + 7 * 7 * 4 - 4, d->nvoxels);
    /* It falls under gravity... */
    float y0 = d->pos.y;
    for (int i = 0; i < 30; i++) wn_debris_step(&ds, &w, WN_SIM_DT);
    TEST_ASSERT_TRUE(d->pos.y < y0);
    wn_mat4 m = wn_debris_matrix(d);
    TEST_ASSERT_TRUE(m.m[15] == 1.0f);
    /* ...and eventually settles back into the world. */
    uint32_t cs = wn_debris_checksum(&ds);
    TEST_ASSERT_NOT_EQUAL(0, cs);
    for (int i = 0; i < 60 * 25 && wn_debris_count(&ds) > 0; i++) wn_debris_step(&ds, &w, WN_SIM_DT);
    TEST_ASSERT_EQUAL_UINT32(0, wn_debris_count(&ds));
    int wood = 0;
    int high = 0;
    for (int y = 0; y < 40; y++)
        for (int z = -20; z < 40; z++)
            for (int x = -20; x < 60; x++) {
                bool n = wn_vox_mat(wn_world_get(&w, x, y, z)) == WN_MAT_PINE_NEEDLES;
                wood += n && y < 12;
                high += n && y >= 20;
            }
    TEST_ASSERT_TRUE(wood > 50); /* the crown now lies on the ground */
    TEST_ASSERT_EQUAL_INT(0, high);
    wn_edit_end(&e);
    wn_debris_free(&ds);
    wn_world_free(&w);
}

static void test_tree_fall_is_deterministic(void)
{
    uint32_t sums[2];
    for (int run = 0; run < 2; run++) {
        wn_world w;
        make_world(&w);
        build_tree(&w, 10, 10, 30);
        wn_debris_set ds;
        wn_debris_init(&ds);
        wn_edit_ctx e;
        wn_edit_begin(&e, &w);
        wn_edit_damage(&e, 10, 3, 10, 100.0f);
        wn_debris_detach(&ds, &e);
        for (int i = 0; i < 300; i++) wn_debris_step(&ds, &w, WN_SIM_DT);
        sums[run] = wn_world_checksum(&w) ^ wn_debris_checksum(&ds);
        wn_edit_end(&e);
        wn_debris_free(&ds);
        wn_world_free(&w);
    }
    TEST_ASSERT_EQUAL_UINT32(sums[0], sums[1]);
}

static void test_connected_structure_stays(void)
{
    wn_world w;
    make_world(&w);
    fill_box(&w, 0, 0, 5, 20, 15, 5, WN_MAT_STONE); /* wall on the ground */
    wn_debris_set ds;
    wn_debris_init(&ds);
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    wn_edit_sphere(&e, wn_voxel_center(wn_iv3(10, 7, 5)), 3 * WN_VOXEL_SIZE, 500.0f);
    TEST_ASSERT_TRUE(e.destroyed > 0);
    TEST_ASSERT_EQUAL_INT(0, wn_debris_detach(&ds, &e));
    TEST_ASSERT_EQUAL_UINT32(0, wn_debris_count(&ds));
    wn_edit_end(&e);
    wn_debris_free(&ds);
    wn_world_free(&w);
}

static int g_vanished;
static void on_vanish(void *u, wn_ivec3 p, wn_voxel v)
{
    (void)u;
    (void)p;
    (void)v;
    g_vanished++;
}

static void test_small_debris_is_cleaned_up(void)
{
    wn_world w;
    make_world(&w);
    wn_debris_set ds;
    wn_debris_init(&ds);
    ds.on_vanish = on_vanish;
    g_vanished = 0;
    /* a floating 2x2x2 piece */
    fill_box(&w, 5, 10, 5, 6, 11, 6, WN_MAT_STONE);
    wn_world_set(&w, 5, 9, 5, wn_vox_make(WN_MAT_STONE, 0));
    wn_edit_ctx e;
    wn_edit_begin(&e, &w);
    wn_edit_damage(&e, 5, 9, 5, 1000.0f);
    TEST_ASSERT_EQUAL_INT(1, wn_debris_detach(&ds, &e));
    for (int i = 0; i < 600 && wn_debris_count(&ds); i++) wn_debris_step(&ds, &w, WN_SIM_DT);
    TEST_ASSERT_EQUAL_UINT32(0, wn_debris_count(&ds));
    TEST_ASSERT_EQUAL_INT(1, g_vanished);
    /* nothing written back */
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 5, 0, 5)));
    wn_edit_end(&e);
    wn_debris_free(&ds);
    wn_world_free(&w);
}

static void test_hard_impact_breaks_piece(void)
{
    wn_world w;
    make_world(&w);
    wn_debris_set ds;
    wn_debris_init(&ds);
    ds.break_speed = 3.0f;
    const int sx = 22, sy = 6, sz = 6;
    wn_voxel *g = (wn_voxel *)calloc(sx * sy * sz, sizeof(wn_voxel));
    for (int y = 1; y < sy - 1; y++)
        for (int z = 1; z < sz - 1; z++)
            for (int x = 1; x < sx - 1; x++) g[(y * sz + z) * sx + x] = wn_vox_make(WN_MAT_STONE, 0);
    wn_debris *d = wn_debris_spawn(&ds, g, sx, sy, sz, wn_v3(0.5f, 3.0f, 0.5f), test_materials());
    TEST_ASSERT_NOT_NULL(d);
    d->vel = wn_v3(0, -12.0f, 0);
    for (int i = 0; i < 120; i++) wn_debris_step(&ds, &w, WN_SIM_DT);
    TEST_ASSERT_TRUE(ds.next_id >= 3); /* split into two new pieces */
    /* empty grids spawn nothing */
    memset(g, 0, sx * sy * sz * sizeof(wn_voxel));
    TEST_ASSERT_NULL(wn_debris_spawn(&ds, g, sx, sy, sz, wn_v3(0, 0, 0), test_materials()));
    free(g);
    wn_debris_free(&ds);
    wn_world_free(&w);
}

static void test_debris_pool_recycles_and_falls_out(void)
{
    wn_world w;
    make_world(&w);
    wn_debris_set ds;
    wn_debris_init(&ds);
    const int s = 5;
    wn_voxel g[5 * 5 * 5];
    memset(g, 0, sizeof g);
    for (int y = 1; y < 4; y++)
        for (int z = 1; z < 4; z++)
            for (int x = 1; x < 4; x++) g[(y * s + z) * s + x] = wn_vox_make(WN_MAT_ICE, 0);
    for (int i = 0; i < WN_MAX_DEBRIS + 3; i++) {
        wn_debris *d = wn_debris_spawn(&ds, g, s, s, s, wn_v3(0.5f + i, 5.0f, 0.5f), test_materials());
        TEST_ASSERT_NOT_NULL(d);
        d->age = (float)i * 0.01f;
    }
    TEST_ASSERT_EQUAL_UINT32(WN_MAX_DEBRIS, wn_debris_count(&ds));
    wn_debris_free(&ds);
    wn_debris_init(&ds);
    /* A piece outside the loaded world falls below the floor and is removed. */
    wn_debris *d = wn_debris_spawn(&ds, g, s, s, s, wn_v3(500.0f, 0.0f, 500.0f), test_materials());
    TEST_ASSERT_NOT_NULL(d);
    for (int i = 0; i < 600 && wn_debris_count(&ds); i++) wn_debris_step(&ds, &w, WN_SIM_DT);
    TEST_ASSERT_EQUAL_UINT32(0, wn_debris_count(&ds));
    wn_debris_free(&ds);
    wn_world_free(&w);
}

static void test_debris_against_walls(void)
{
    /* A piece sliding into a wall gets a sideways contact normal. */
    wn_world w;
    make_world(&w);
    fill_box(&w, 20, 0, 0, 22, 20, 20, WN_MAT_STONE);
    wn_debris_set ds;
    wn_debris_init(&ds);
    const int s = 5;
    wn_voxel g[5 * 5 * 5];
    memset(g, 0, sizeof g);
    for (int y = 1; y < 4; y++)
        for (int z = 1; z < 4; z++)
            for (int x = 1; x < 4; x++) g[(y * s + z) * s + x] = wn_vox_make(WN_MAT_STONE, 0);
    wn_debris *d = wn_debris_spawn(&ds, g, s, s, s, wn_v3(1.6f, 0.6f, 0.8f), test_materials());
    d->vel = wn_v3(6.0f, 0.0f, 0.0f);
    for (int i = 0; i < 60; i++) wn_debris_step(&ds, &w, WN_SIM_DT);
    TEST_ASSERT_TRUE(wn_debris_count(&ds) == 0 || ds.d[0].pos.x < 2.3f);
    wn_debris_free(&ds);
    wn_world_free(&w);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_soft_material_breaks_first);
    RUN_TEST(test_repeated_hits_destroy);
    RUN_TEST(test_sword_swing_carves_slice);
    RUN_TEST(test_explosion_falloff);
    RUN_TEST(test_cutting_tree_trunk_makes_debris);
    RUN_TEST(test_tree_fall_is_deterministic);
    RUN_TEST(test_connected_structure_stays);
    RUN_TEST(test_small_debris_is_cleaned_up);
    RUN_TEST(test_hard_impact_breaks_piece);
    RUN_TEST(test_debris_pool_recycles_and_falls_out);
    RUN_TEST(test_debris_against_walls);
    return UNITY_END();
}
