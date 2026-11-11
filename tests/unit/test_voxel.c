#include "test_util.h"
#include "unity.h"
#include "wn/wn_core.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_world_position_maps_to_voxel(void)
{
    /* spec voxel-world: World position maps to a voxel / Voxel size can be changed */
    wn_ivec3 v = wn_world_to_voxel(wn_v3(1.25f, 0.05f, -0.01f));
    if (WN_VOXEL_SIZE == 0.1f) {
        TEST_ASSERT_EQUAL_INT(12, v.x);
        TEST_ASSERT_EQUAL_INT(0, v.y);
    } else {
        TEST_ASSERT_EQUAL_INT(wn_floorf_i(1.25f / WN_VOXEL_SIZE), v.x);
        TEST_ASSERT_EQUAL_INT(wn_floorf_i(0.05f / WN_VOXEL_SIZE), v.y);
    }
    TEST_ASSERT_EQUAL_INT(-1, v.z);
    wn_ivec3 c = wn_voxel_to_chunk(v);
    TEST_ASSERT_EQUAL_INT(0, c.x);
    TEST_ASSERT_EQUAL_INT(-1, c.z);
    wn_vec3 corner = wn_voxel_to_world(wn_iv3(10, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10 * WN_VOXEL_SIZE, corner.x);
    wn_vec3 center = wn_voxel_center(wn_iv3(0, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, WN_VOXEL_SIZE * 0.5f, center.y);
}

static void test_negative_coordinates(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -3, 3);
    wn_world_load_chunk(&w, wn_iv3(-1, -2, 0));
    wn_voxel v = wn_vox_make(WN_MAT_STONE, 0);
    wn_world_set(&w, -1, -33, 0, v);
    TEST_ASSERT_EQUAL_HEX32(v, wn_world_get(&w, -1, -33, 0) & 0x00FFFFFFu); /* light bits are kept */
    wn_ivec3 c = wn_voxel_to_chunk(wn_iv3(-1, -33, 0));
    TEST_ASSERT_EQUAL_INT(-1, c.x);
    TEST_ASSERT_EQUAL_INT(-2, c.y);
    TEST_ASSERT_EQUAL_INT(0, c.z);
    wn_world_free(&w);
}

static void test_voxel_fields_are_independent(void)
{
    wn_voxel v = 0;
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(v));
    v = wn_vox_make(WN_MAT_STONE, 0);
    v = wn_vox_set_sky(v, 9);
    v = wn_vox_set_block(v, 4);
    v = wn_vox_set_damage(v, 3);
    v = wn_vox_set_fill(v, 7);
    wn_voxel before = v;
    v = wn_vox_set_stain(v, WN_STAIN_BLOOD, 40);
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD, wn_vox_stain(v));
    TEST_ASSERT_EQUAL_UINT32(40, wn_vox_stain_amount(v));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_STONE, wn_vox_mat(v));
    TEST_ASSERT_EQUAL_UINT32(7, wn_vox_fill(v));
    TEST_ASSERT_EQUAL_UINT32(3, wn_vox_damage(v));
    TEST_ASSERT_EQUAL_UINT32(9, wn_vox_sky(v));
    TEST_ASSERT_EQUAL_UINT32(4, wn_vox_block(v));
    TEST_ASSERT_EQUAL_HEX32(before, wn_vox_set_stain(v, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(16, wn_vox_height16(wn_vox_make(WN_MAT_STONE, 0)));
    TEST_ASSERT_EQUAL_UINT32(8, wn_vox_height16(wn_vox_make(WN_MAT_SNOW, 8)));
    TEST_ASSERT_EQUAL_UINT32(5, wn_vox_mat(wn_vox_set_mat(v, 5)));
}

static void test_default_voxel_is_air(void)
{
    wn_chunk *c = wn_chunk_new(wn_iv3(0, 0, 0), 0);
    wn_voxel v = wn_chunk_get(c, 3, 4, 5);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(v));
    TEST_ASSERT_EQUAL_UINT32(0, wn_vox_fill(v));
    TEST_ASSERT_EQUAL_UINT32(0, wn_vox_damage(v));
    TEST_ASSERT_EQUAL_UINT32(0, wn_vox_stain(v));
    TEST_ASSERT_EQUAL_UINT32(0, wn_vox_sky(v) + wn_vox_block(v));
    wn_chunk_free(c);
    wn_chunk_free(NULL);
}

static void test_material_ordering(void)
{
    wn_materials *m = test_materials();
    int snow = wn_materials_find(m, "snow"), wood = wn_materials_find(m, "pine-wood");
    int ice = wn_materials_find(m, "ice"), stone = wn_materials_find(m, "stone");
    TEST_ASSERT_EQUAL_INT(WN_MAT_SNOW, snow);
    TEST_ASSERT_TRUE(m->m[snow].hardness < m->m[wood].hardness);
    TEST_ASSERT_TRUE(m->m[wood].hardness < m->m[ice].hardness);
    TEST_ASSERT_TRUE(m->m[ice].hardness < m->m[stone].hardness);
    TEST_ASSERT_TRUE(m->m[WN_MAT_PINE_WOOD].flammable);
    TEST_ASSERT_TRUE(m->m[WN_MAT_BEDROCK].anchor);
    TEST_ASSERT_EQUAL_INT(14, m->m[WN_MAT_WINDOW_GLOW].emit);
    TEST_ASSERT_EQUAL_INT(-1, wn_materials_find(m, "unobtainium"));
    TEST_ASSERT_TRUE(wn_materials_find(m, "bone") >= WN_MAT_BUILTIN_COUNT);
    TEST_ASSERT_EQUAL_STRING("snow", wn_material_builtin_name(WN_MAT_SNOW));
    TEST_ASSERT_NULL(wn_material_builtin_name(999));
    TEST_ASSERT_TRUE(wn_mat_is_solid(m, WN_MAT_SNOW));
    TEST_ASSERT_FALSE(wn_mat_is_solid(m, WN_MAT_WATER));
}

static void test_real_material_file_loads(void)
{
    char *txt = wn_read_file(WN_DATA_DIR "/materials.txt", NULL);
    TEST_ASSERT_NOT_NULL(txt);
    wn_materials m;
    char err[200] = "";
    TEST_ASSERT_TRUE_MESSAGE(wn_materials_load(&m, txt, err, sizeof err), err);
    free(txt);
}

static void test_invalid_material_data_rejected(void)
{
    wn_materials m = *test_materials();
    wn_materials before = m;
    char err[200];
    const char *bad[] = {
        "material \"\" solid 1 1 0 - #ffffff\n",
        "material stone solid -5 1 0 - #ffffff\n",
        "material stone solid 5 1 99 - #ffffff\n",
        "material stone rubbery 5 1 0 - #ffffff\n",
        "material stone solid 5 1 0 z #ffffff\n",
        "material stone solid 5 1 0 -\n",
        "material stone solid x 1 0 - #ffffff\n",
        "material stone solid 5 1 0 - #fff\n",
        "stuff\n",
        "material stone solid 5 1 0 - #ffffff\nmaterial stone solid 5 1 0 - #ffffff\n",
        "material aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa solid 5 1 0 - #ffffff\n",
        "material stone solid 5 1 0 - #ffffff\n", /* misses other built-ins */
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        TEST_ASSERT_FALSE_MESSAGE(wn_materials_load(&m, bad[i], err, sizeof err), bad[i]);
        TEST_ASSERT_TRUE(strlen(err) > 0);
    }
    TEST_ASSERT_EQUAL_MEMORY(&before, &m, sizeof m);
    TEST_ASSERT_NOT_NULL(strstr(err, "built-in material"));
    /* Too many materials. */
    size_t cap = 300 * 64;
    char *many = (char *)malloc(cap);
    size_t n = 0;
    n += (size_t)snprintf(many + n, cap - n, "%s", k_test_materials);
    for (int i = 0; i < 260; i++) n += (size_t)snprintf(many + n, cap - n, "material extra%d solid 1 1 0 - #ffffff\n", i);
    TEST_ASSERT_FALSE(wn_materials_load(&m, many, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "too many"));
    free(many);
}

static void test_uniform_chunk_compaction(void)
{
    wn_chunk *c = wn_chunk_new(wn_iv3(0, 0, 0), 0);
    TEST_ASSERT_TRUE(wn_chunk_is_uniform(c));
    wn_chunk_set(c, 1, 1, 1, 0); /* same value keeps it compact */
    TEST_ASSERT_TRUE(wn_chunk_is_uniform(c));
    TEST_ASSERT_FALSE(wn_chunk_compact(c));
    wn_chunk_set(c, 1, 1, 1, wn_vox_make(WN_MAT_STONE, 0));
    TEST_ASSERT_FALSE(wn_chunk_is_uniform(c));
    TEST_ASSERT_FALSE(wn_chunk_compact(c));
    wn_chunk_set(c, 1, 1, 1, 0);
    TEST_ASSERT_TRUE(wn_chunk_compact(c));
    TEST_ASSERT_TRUE(wn_chunk_is_uniform(c));
    wn_chunk_expand(c);
    wn_chunk_expand(c);
    TEST_ASSERT_EQUAL_HEX32(0, wn_chunk_get(c, 31, 31, 31));
    wn_chunk_free(c);
}

static void test_streaming_loads_and_unloads(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    float chunk_m = WN_CHUNK_SIZE * WN_VOXEL_SIZE;
    int loaded = 0;
    while (wn_world_stream(&w, wn_v3(0.5f * chunk_m, 0, 0.5f * chunk_m), 6, 50) > 0) loaded++;
    TEST_ASSERT_EQUAL_UINT32(13 * 13 * 3, w.nchunks);
    wn_vec3 far = wn_v3(10.5f * chunk_m, 0, 0.5f * chunk_m);
    while (wn_world_stream(&w, far, 6, 1000) > 0) {}
    /* Chunks within radius 6 of the new position are loaded... */
    for (int dx = -6; dx <= 6; dx++) TEST_ASSERT_TRUE(wn_world_column_loaded(&w, 10 + dx, 0));
    /* ...and chunks further than radius + 1 are gone. */
    TEST_ASSERT_FALSE(wn_world_column_loaded(&w, 2, 0));
    TEST_ASSERT_FALSE(wn_world_column_loaded(&w, -3, 0));
    wn_world_free(&w);
}

static void test_player_changes_survive_unloading(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    wn_world_load_column(&w, 0, 0);
    wn_world_load_column(&w, 0, 0); /* second call is a no-op */
    wn_world_load_column(&w, 5, 0);
    TEST_ASSERT_TRUE(wn_world_is_solid(&w, 3, -1, 3));
    uint32_t before = wn_world_checksum(&w);
    wn_world_set(&w, 3, -1, 3, 0); /* destroy */
    TEST_ASSERT_NOT_EQUAL(before, wn_world_checksum(&w));
    uint32_t after = wn_world_checksum(&w);
    for (int cy = -1; cy <= 1; cy++) {
        wn_world_unload_chunk(&w, wn_iv3(0, cy, 0));
        wn_world_unload_chunk(&w, wn_iv3(5, cy, 0));
    }
    wn_world_unload_chunk(&w, wn_iv3(77, 0, 0)); /* not loaded: no-op */
    TEST_ASSERT_EQUAL_UINT32(1, w.nsaved);
    TEST_ASSERT_FALSE(wn_world_is_solid(&w, 3, -1, 3) && wn_world_chunk(&w, wn_iv3(0, -1, 0)));
    wn_world_load_column(&w, 0, 0);
    wn_world_load_column(&w, 5, 0);
    TEST_ASSERT_EQUAL_UINT32(0, w.nsaved);
    TEST_ASSERT_FALSE(wn_world_is_solid(&w, 3, -1, 3));
    TEST_ASSERT_TRUE(wn_world_is_solid(&w, 4, -1, 3));
    TEST_ASSERT_EQUAL_UINT32(after, wn_world_checksum(&w));
    wn_world_free(&w);
}

static void test_checksum_canonical(void)
{
    /* An expanded but uniform chunk hashes like a compact one; light is ignored. */
    wn_world a, b;
    wn_world_init(&a, test_materials(), 0, 0);
    wn_world_init(&b, test_materials(), 0, 0);
    wn_world_load_chunk(&a, wn_iv3(0, 0, 0));
    wn_chunk *c = wn_world_load_chunk(&b, wn_iv3(0, 0, 0));
    wn_chunk_expand(c);
    c->vox[5] = wn_vox_set_sky(c->vox[5], 3);
    TEST_ASSERT_EQUAL_UINT32(wn_world_checksum(&a), wn_world_checksum(&b));
    wn_world_set(&b, 1, 2, 3, wn_vox_make(WN_MAT_STONE, 0));
    TEST_ASSERT_NOT_EQUAL(wn_world_checksum(&a), wn_world_checksum(&b));
    wn_world_free(&a);
    wn_world_free(&b);
}

static void test_unloaded_reads_are_air(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_world_get(&w, 1000, 0, 0)));
    TEST_ASSERT_EQUAL_UINT32(15, wn_vox_sky(wn_world_get(&w, 1000, 0, 0)));
    wn_world_set(&w, 1000, 0, 0, wn_vox_make(WN_MAT_STONE, 0)); /* ignored */
    TEST_ASSERT_EQUAL_UINT32(0, w.nchunks);
    wn_world_free(&w);
}

static void test_border_edits_dirty_neighbours(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    for (int x = -1; x <= 1; x++)
        for (int z = -1; z <= 1; z++) wn_world_load_column(&w, x, z);
    wn_chunk *n = wn_world_chunk(&w, wn_iv3(-1, -1, -1));
    n->flags = 0;
    uint32_t rev = n->mesh_rev;
    wn_world_set(&w, 0, -32, 0, 0); /* corner voxel of chunk (0,-1,0) */
    TEST_ASSERT_TRUE(n->mesh_rev > rev);
    TEST_ASSERT_TRUE(n->flags & WN_CHUNK_DIRTY_MESH);
    wn_chunk *m = wn_world_chunk(&w, wn_iv3(0, -1, 0));
    m->flags = 0;
    wn_world_set_quiet(&w, 31, -1, 31, wn_vox_make(WN_MAT_SNOW, 3));
    TEST_ASSERT_FALSE(m->flags & WN_CHUNK_MODIFIED);
    TEST_ASSERT_TRUE(m->flags & WN_CHUNK_DIRTY_MESH);
    wn_world_free(&w);
}

static void test_ray_hits_top_snow(void)
{
    /* spec voxel-world: Ray hits the first solid voxel */
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    wn_world_load_column(&w, 0, 0);
    for (int y = 0; y < 3; y++) wn_world_set(&w, 5, y, 5, wn_vox_make(WN_MAT_SNOW, 0));
    wn_vec3 o = wn_voxel_center(wn_iv3(5, 20, 5));
    wn_ray_hit h = wn_world_raycast(&w, o, wn_v3(0, -1, 0), 5.0f);
    TEST_ASSERT_TRUE(h.hit);
    TEST_ASSERT_EQUAL_INT(2, h.voxel.y);
    TEST_ASSERT_EQUAL_INT(1, h.normal.y);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 17.5f * WN_VOXEL_SIZE, h.distance);
    /* Partial snow: the ray stops at the fill height. */
    wn_world_set(&w, 5, 3, 5, wn_vox_make(WN_MAT_SNOW, 8));
    h = wn_world_raycast(&w, o, wn_v3(0, -1, 0), 5.0f);
    TEST_ASSERT_TRUE(h.hit);
    TEST_ASSERT_EQUAL_INT(3, h.voxel.y);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 17.0f * WN_VOXEL_SIZE, h.distance);
    /* A diagonal ray that just passes above the partial voxel misses it. */
    wn_world_set(&w, 8, 0, 5, wn_vox_make(WN_MAT_SNOW, 2));
    wn_vec3 o2 = wn_v3(7.0f * WN_VOXEL_SIZE, 0.9f * WN_VOXEL_SIZE, 5.5f * WN_VOXEL_SIZE);
    h = wn_world_raycast(&w, o2, wn_v3(1, 0, 0), 0.25f);
    TEST_ASSERT_FALSE(h.hit);
    /* Horizontal ray into a wall. */
    wn_world_set(&w, 12, 0, 5, wn_vox_make(WN_MAT_STONE, 0));
    h = wn_world_raycast(&w, wn_voxel_center(wn_iv3(9, 0, 5)), wn_v3(1, 0, 0), 2.0f);
    TEST_ASSERT_TRUE(h.hit);
    TEST_ASSERT_EQUAL_INT(-1, h.normal.x);
    h = wn_world_raycast(&w, wn_voxel_center(wn_iv3(14, 0, 5)), wn_v3(-1, 0, 0), 2.0f);
    TEST_ASSERT_EQUAL_INT(1, h.normal.x);
    h = wn_world_raycast(&w, wn_voxel_center(wn_iv3(12, 0, 2)), wn_v3(0, 0, 1), 2.0f);
    TEST_ASSERT_EQUAL_INT(-1, h.normal.z);
    wn_world_free(&w);
}

static void test_ray_misses_sky(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    wn_world_load_column(&w, 0, 0);
    wn_ray_hit h = wn_world_raycast(&w, wn_v3(1, 1, 1), wn_v3(0.2f, 1, 0.1f), 5.0f);
    TEST_ASSERT_FALSE(h.hit);
    wn_world_free(&w);
}

static void test_box_overlap(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    wn_world_load_column(&w, 0, 0);
    float v = WN_VOXEL_SIZE;
    TEST_ASSERT_TRUE(wn_world_box_overlaps(&w, wn_v3(v, -0.5f * v, v), wn_v3(2 * v, v, 2 * v)));
    TEST_ASSERT_FALSE(wn_world_box_overlaps(&w, wn_v3(v, 0.01f, v), wn_v3(2 * v, v, 2 * v)));
    wn_world_set(&w, 1, 0, 1, wn_vox_make(WN_MAT_SNOW, 4)); /* quarter height */
    TEST_ASSERT_FALSE(wn_world_box_overlaps(&w, wn_v3(v, 0.3f * v, v), wn_v3(1.9f * v, v, 1.9f * v)));
    TEST_ASSERT_TRUE(wn_world_box_overlaps(&w, wn_v3(v, 0.2f * v, v), wn_v3(1.9f * v, v, 1.9f * v)));
    wn_world_free(&w);
}

static int g_count;
static void count_chunk(void *u, wn_chunk *c)
{
    (void)u;
    (void)c;
    g_count++;
}

static void test_foreach_chunk(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_load_column(&w, 0, 0);
    g_count = 0;
    wn_world_foreach_chunk(&w, count_chunk, NULL);
    TEST_ASSERT_EQUAL_INT(3, g_count);
    wn_world_free(&w);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_world_position_maps_to_voxel);
    RUN_TEST(test_negative_coordinates);
    RUN_TEST(test_voxel_fields_are_independent);
    RUN_TEST(test_default_voxel_is_air);
    RUN_TEST(test_material_ordering);
    RUN_TEST(test_real_material_file_loads);
    RUN_TEST(test_invalid_material_data_rejected);
    RUN_TEST(test_uniform_chunk_compaction);
    RUN_TEST(test_streaming_loads_and_unloads);
    RUN_TEST(test_player_changes_survive_unloading);
    RUN_TEST(test_checksum_canonical);
    RUN_TEST(test_unloaded_reads_are_air);
    RUN_TEST(test_border_edits_dirty_neighbours);
    RUN_TEST(test_ray_hits_top_snow);
    RUN_TEST(test_ray_misses_sky);
    RUN_TEST(test_box_overlap);
    RUN_TEST(test_foreach_chunk);
    return UNITY_END();
}
