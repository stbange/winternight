#include "test_util.h"
#include "unity.h"
#include "wn/wn_font.h"
#include "wn/wn_light.h"
#include "wn/wn_model.h"
#include "wn/wn_overlay.h"
#include "wn/wn_remesh.h"
#include "wn/wn_view.h"

#include <stdlib.h>
#include <string.h>

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

/* A simple skeleton: torso with an arm. */
static void build_skeleton(wn_model *m)
{
    wn_model_init(m);
    m->bleeds = false;
    int torso = wn_model_add_part(m, "torso", -1, 4, 6, 2, wn_v3(2, 0, 1), wn_v3(0, 0, 0));
    wn_model_part_box(m, torso, 0, 0, 0, 3, 5, 1, wn_vox_make(WN_MAT_ICE, 0));
    int arm = wn_model_add_part(m, "arm", torso, 1, 6, 1, wn_v3(0.5f, 6, 0.5f), wn_v3(-0.5f, 6, 1));
    wn_model_part_box(m, arm, 0, 0, 0, 0, 5, 0, wn_vox_make(WN_MAT_ICE, 0));
    int hand = wn_model_add_part(m, "hand", arm, 1, 1, 1, wn_v3(0.5f, 1, 0.5f), wn_v3(0.5f, 0, 0.5f));
    wn_model_part_set(m, hand, 0, 0, 0, wn_vox_make(WN_MAT_ICE, 0));
}

static void test_model_parts_and_matrices(void)
{
    wn_model m;
    build_skeleton(&m);
    TEST_ASSERT_EQUAL_INT(3, m.nparts);
    TEST_ASSERT_EQUAL_INT(1, wn_model_find_part(&m, "arm"));
    TEST_ASSERT_EQUAL_INT(-1, wn_model_find_part(&m, "tail"));
    TEST_ASSERT_EQUAL_UINT32(48, m.parts[0].nvoxels);
    TEST_ASSERT_EQUAL_UINT32(6, m.parts[1].nvoxels);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_ICE, wn_vox_mat(wn_model_part_get(&m, 0, 1, 1, 1)));
    TEST_ASSERT_EQUAL_UINT32(0, wn_model_part_get(&m, 0, 99, 1, 1));
    wn_model_part_set(&m, 0, 99, 0, 0, wn_vox_make(WN_MAT_ICE, 0)); /* outside: ignored */
    m.pos = wn_v3(1, 2, 3);
    wn_mat4 t = wn_model_part_matrix(&m, 0);
    wn_vec3 pivot = wn_mat4_mul_point(t, wn_v3(2, 0, 1));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, pivot.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, pivot.y);
    /* Rotating the arm moves its tip around the shoulder. */
    wn_vec3 tip0 = wn_mat4_mul_point(wn_model_part_matrix(&m, 1), wn_v3(0.5f, 0, 0.5f));
    m.parts[1].rx = 1.2f;
    m.parts[1].rz = 0.3f;
    wn_vec3 tip1 = wn_mat4_mul_point(wn_model_part_matrix(&m, 1), wn_v3(0.5f, 0, 0.5f));
    TEST_ASSERT_TRUE(wn_v3_len(wn_v3_sub(tip0, tip1)) > 0.1f);
    for (int i = 0; i < WN_MODEL_MAX_PARTS; i++) wn_model_add_part(&m, "x", 0, 1, 1, 1, wn_v3(0, 0, 0), wn_v3(0, 0, 0));
    TEST_ASSERT_EQUAL_INT(-1, wn_model_add_part(&m, "y", 0, 1, 1, 1, wn_v3(0, 0, 0), wn_v3(0, 0, 0)));
    wn_model_free(&m);
}

static void test_skeleton_loses_an_arm(void)
{
    /* spec voxel-deformation: Skeleton loses an arm */
    wn_model m;
    build_skeleton(&m);
    m.pos = wn_v3(5, 1, 5);
    wn_debris_set ds;
    wn_debris_init(&ds);
    wn_mat4 arm = wn_model_part_matrix(&m, 1);
    wn_vec3 shoulder = wn_mat4_mul_point(arm, wn_v3(0.5f, 5.5f, 0.5f));
    /* a blade passing through the top of the arm */
    wn_model_hit h = wn_model_damage(&m, wn_v3_add(shoulder, wn_v3(0, 0, -1)), wn_v3_add(shoulder, wn_v3(0, 0, 1)),
                                     0.12f, &ds, test_materials());
    TEST_ASSERT_TRUE(h.voxels_removed > 0);
    TEST_ASSERT_TRUE(h.severed);
    TEST_ASSERT_TRUE(m.parts[1].severed);
    TEST_ASSERT_TRUE(m.parts[2].severed); /* the hand goes with it */
    TEST_ASSERT_FALSE(m.parts[0].severed);
    TEST_ASSERT_EQUAL_UINT32(2, wn_debris_count(&ds));
    /* The rest keeps working: a miss does nothing. */
    wn_model_hit miss = wn_model_damage(&m, wn_v3(50, 50, 50), wn_v3(51, 50, 50), 0.1f, &ds, test_materials());
    TEST_ASSERT_EQUAL_INT(0, miss.voxels_removed);
    TEST_ASSERT_EQUAL_INT(-1, miss.part);
    /* Damage on the root never severs it. */
    wn_vec3 chest = wn_mat4_mul_point(wn_model_part_matrix(&m, 0), wn_v3(2, 3, 1));
    wn_model_hit body = wn_model_damage(&m, wn_v3_add(chest, wn_v3(-1, 0, 0)), wn_v3_add(chest, wn_v3(1, 0, 0)), 0.3f,
                                        NULL, test_materials());
    TEST_ASSERT_TRUE(body.voxels_removed > 0);
    TEST_ASSERT_FALSE(m.parts[0].severed);
    wn_debris_free(&ds);
    wn_model_free(&m);
}

static void test_remesh_plan_priority(void)
{
    wn_world w;
    make_world(&w, 2);
    wn_remesh_item items[200];
    uint32_t total = wn_remesh_plan(&w, wn_v3(0.5f, 0.5f, 0.5f), 2, items, 200);
    TEST_ASSERT_EQUAL_UINT32(5 * 5 * 3, total);
    for (uint32_t i = 1; i < total; i++) TEST_ASSERT_TRUE(items[i - 1].dist2 <= items[i].dist2);
    TEST_ASSERT_EQUAL_INT(0, items[0].chunk->pos.x);
    TEST_ASSERT_EQUAL_INT(0, items[0].chunk->pos.z);
    /* only a limited number written */
    TEST_ASSERT_EQUAL_UINT32(75, wn_remesh_plan(&w, wn_v3(0.5f, 0.5f, 0.5f), 2, items, 3));
    /* radius 0 */
    TEST_ASSERT_EQUAL_UINT32(3, wn_remesh_plan(&w, wn_v3(0.5f, 0.5f, 0.5f), 0, items, 200));
    float cost[4] = {3.0f, 1.0f, 1.0f, 5.0f};
    TEST_ASSERT_EQUAL_UINT32(3, wn_remesh_fit_budget(cost, 4, 5.0f));
    TEST_ASSERT_EQUAL_UINT32(1, wn_remesh_fit_budget(cost, 4, 0.5f)); /* always progress */
    TEST_ASSERT_EQUAL_UINT32(0, wn_remesh_fit_budget(cost, 0, 5.0f));
    wn_world_free(&w);
}

static int g_budget;
static bool budget(void *u)
{
    (void)u;
    return --g_budget > 0;
}

static void test_view_lod_columns_and_budget(void)
{
    wn_world w;
    make_world(&w, 4);
    wn_view v;
    wn_view_init(&v);
    TEST_ASSERT_EQUAL_INT(0, wn_view_lod_for_distance(&v, 1));
    TEST_ASSERT_EQUAL_INT(1, wn_view_lod_for_distance(&v, 2));
    TEST_ASSERT_EQUAL_INT(2, wn_view_lod_for_distance(&v, 4));
    /* With a budget only part of the work is done per update. */
    g_budget = 10;
    wn_view_update(&v, &w, wn_v3(0.5f, 0.5f, 0.5f), 4, budget, NULL);
    TEST_ASSERT_TRUE(v.meshed_last <= 10);
    TEST_ASSERT_TRUE(v.pending_last > 0);
    /* Nearest chunks first: the centre column is done. */
    wn_view_column *c0 = wn_view_find(&v, 0, 0);
    TEST_ASSERT_NOT_NULL(c0);
    TEST_ASSERT_EQUAL_INT(0, c0->lod);
    TEST_ASSERT_EQUAL_INT(0, c0->chunks[0].lod);
    while (v.pending_last > 0) wn_view_update(&v, &w, wn_v3(0.5f, 0.5f, 0.5f), 4, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(81, v.ncols);
    wn_view_column *far = wn_view_find(&v, 4, 4);
    TEST_ASSERT_EQUAL_INT(2, far->lod);
    TEST_ASSERT_TRUE(far->gpu_dirty);
    TEST_ASSERT_TRUE(far->mesh.nverts > 0);
    /* A changed chunk is remeshed on the next update. */
    c0->gpu_dirty = false;
    wn_world_set(&w, 3, 0, 3, wn_vox_make(WN_MAT_STONE, 0));
    wn_view_update(&v, &w, wn_v3(0.5f, 0.5f, 0.5f), 4, NULL, NULL);
    TEST_ASSERT_TRUE(v.meshed_last >= 1);
    TEST_ASSERT_TRUE(c0->gpu_dirty);
    /* Moving away drops far columns and reports their backend handles. */
    wn_view_find(&v, -4, 0)->gpu = 77;
    float s = WN_CHUNK_SIZE * WN_VOXEL_SIZE;
    for (int x = 5; x <= 8; x++)
        for (int z = -4; z <= 4; z++) wn_world_load_column(&w, x, z);
    wn_view_update(&v, &w, wn_v3(3.5f * s, 0.5f, 0.5f), 4, NULL, NULL);
    TEST_ASSERT_NULL(wn_view_find(&v, -4, 0));
    uint32_t freed[8];
    TEST_ASSERT_EQUAL_UINT32(1, wn_view_take_freed(&v, freed, 8));
    TEST_ASSERT_EQUAL_UINT32(77, freed[0]);
    TEST_ASSERT_EQUAL_UINT32(0, wn_view_take_freed(&v, freed, 8));
    /* LOD changed for the previous centre: it is no longer full detail. */
    TEST_ASSERT_TRUE(wn_view_find(&v, 0, 0)->lod > 0);
    wn_view_find(&v, 1, 0)->gpu = 5;
    wn_view_free(&v);
    wn_world_free(&w);
}

static void test_frustum(void)
{
    wn_mat4 proj = wn_mat4_perspective(1.0f, 1.0f, 0.1f, 100.0f);
    wn_mat4 view = wn_mat4_look_at(wn_v3(0, 0, 0), wn_v3(0, 0, -1), wn_v3(0, 1, 0));
    wn_frustum f = wn_frustum_from_matrix(wn_mat4_mul(proj, view));
    TEST_ASSERT_TRUE(wn_frustum_box(&f, wn_v3(-1, -1, -11), wn_v3(1, 1, -9)));
    TEST_ASSERT_FALSE(wn_frustum_box(&f, wn_v3(-1, -1, 9), wn_v3(1, 1, 11)));   /* behind */
    TEST_ASSERT_FALSE(wn_frustum_box(&f, wn_v3(50, -1, -11), wn_v3(52, 1, -9))); /* far right */
    TEST_ASSERT_FALSE(wn_frustum_box(&f, wn_v3(-1, -1, -300), wn_v3(1, 1, -200)));
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 2);
    wn_vec3 mn, mx;
    wn_view_column_bounds(&w, 1, -1, &mn, &mx);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, WN_CHUNK_SIZE * WN_VOXEL_SIZE, mn.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -WN_CHUNK_SIZE * WN_VOXEL_SIZE, mn.y);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3 * WN_CHUNK_SIZE * WN_VOXEL_SIZE, mx.y);
    wn_world_free(&w);
}

static void test_lod_meshing_and_append(void)
{
    wn_world w;
    make_world(&w, 1);
    wn_mesh a, b, col;
    wn_mesh_init(&a);
    wn_mesh_init(&b);
    wn_mesh_init(&col);
    wn_mesh_chunk_lod(&a, &w, wn_iv3(0, -1, 0), 0);
    wn_mesh_chunk_lod(&b, &w, wn_iv3(0, -1, 0), 2);
    TEST_ASSERT_TRUE(a.nverts > 0 && b.nverts > 0);
    wn_mesh_chunk_lod(&b, &w, wn_iv3(0, 1, 0), 1); /* uniform air chunk: empty */
    TEST_ASSERT_EQUAL_UINT32(0, b.nverts);
    wn_mesh_append(&col, &a, 0);
    wn_mesh_append(&col, &a, 512);
    TEST_ASSERT_EQUAL_UINT32(2 * a.nverts, col.nverts);
    TEST_ASSERT_EQUAL_UINT32(a.verts[0].y + 512, col.verts[a.nverts].y);
    TEST_ASSERT_EQUAL_UINT32(a.indices[0] + a.nverts, col.indices[a.nindices]);
    /* downsampling */
    wn_materials *m = test_materials();
    wn_voxel blk[8];
    for (int i = 0; i < 8; i++) blk[i] = wn_vox_set_sky(0, (uint32_t)i);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_AIR, wn_vox_mat(wn_mesh_downsample(blk, 8, m)));
    TEST_ASSERT_EQUAL_UINT32(7, wn_vox_sky(wn_mesh_downsample(blk, 8, m)));
    for (int i = 0; i < 5; i++) blk[i] = wn_vox_make(i < 3 ? WN_MAT_STONE : WN_MAT_SNOW, 0);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_STONE, wn_vox_mat(wn_mesh_downsample(blk, 8, m)));
    blk[4] = wn_vox_set_stain(wn_vox_make(WN_MAT_SNOW, 0), WN_STAIN_BLOOD, 40);
    TEST_ASSERT_EQUAL_UINT32(WN_STAIN_BLOOD, wn_vox_stain(wn_mesh_downsample(blk, 8, m)));
    for (int i = 0; i < 8; i++) blk[i] = 0;
    blk[0] = wn_vox_make(WN_MAT_WINDOW_GLOW, 0);
    blk[1] = wn_vox_make(WN_MAT_STONE, 0);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_WINDOW_GLOW, wn_vox_mat(wn_mesh_downsample(blk, 8, m))); /* windows stay visible */
    TEST_ASSERT_EQUAL_HEX16(WN_MAT_SNOW | (1 << 8) | (1 << 10) | (10 << 12),
                            wn_surface_word(wn_vox_set_stain(wn_vox_set_damage(wn_vox_make(WN_MAT_SNOW, 0), 6), 1, 40)));
    wn_mesh_free(&a);
    wn_mesh_free(&b);
    wn_mesh_free(&col);
    wn_world_free(&w);
}

static void test_font_and_overlay(void)
{
    TEST_ASSERT_EQUAL_INT(0, wn_font_width(""));
    TEST_ASSERT_EQUAL_INT(5, wn_font_width("A"));
    TEST_ASSERT_EQUAL_INT(11, wn_font_width("Ab"));
    uint8_t buf[20 * 10];
    memset(buf, 0, sizeof buf);
    wn_font_draw(buf, 20, 10, 0, 0, "I");
    TEST_ASSERT_EQUAL_UINT8(255, buf[0 * 20 + 2]); /* top bar of I */
    TEST_ASSERT_EQUAL_UINT8(0, buf[0 * 20 + 0]);
    memset(buf, 0, sizeof buf);
    wn_font_draw(buf, 20, 10, -2, -2, "~"); /* unknown glyph: box, clipped */
    TEST_ASSERT_EQUAL_UINT8(255, buf[0 * 20 + 2]);

    wn_overlay o;
    wn_overlay_init(&o, 480, 270);
    wn_overlay_finish(&o);
    uint32_t empty = o.hash;
    wn_overlay_text(&o, 10, 10, "CASTLE HEARTHHOLM", 0xFFE2A0);
    wn_overlay_finish(&o);
    TEST_ASSERT_TRUE(o.changed);
    TEST_ASSERT_NOT_EQUAL(empty, o.hash);
    wn_overlay_finish(&o);
    TEST_ASSERT_FALSE(o.changed);
    wn_overlay_clear(&o);
    wn_overlay_circle(&o, 100, 100, 10, 0.0f, 0xFF0000, 200);
    TEST_ASSERT_EQUAL_UINT8(200, o.rgba[(100 * 480 + 100) * 4 + 3]);
    wn_overlay_circle(&o, 200, 100, 10, 2.0f, 0xFF0000, 200);
    TEST_ASSERT_EQUAL_UINT8(0, o.rgba[(100 * 480 + 200) * 4 + 3]); /* ring is hollow */
    wn_overlay_circle(&o, -5, -5, 10, 0.0f, 0xFF0000, 200);         /* clipped */
    /* touch controls */
    wn_touch t;
    wn_input in;
    wn_input_init(&in);
    wn_touch_init(&t, 16.0f / 9.0f, false);
    wn_overlay_clear(&o);
    wn_overlay_touch(&o, &t); /* hidden: nothing drawn */
    wn_overlay_finish(&o);
    TEST_ASSERT_EQUAL_UINT32(empty, o.hash);
    t.visible = true;
    wn_touch_down(&t, &in, 1, 0.3f, 0.7f);
    wn_touch_move(&t, &in, 1, 0.35f, 0.65f);
    wn_touch_down(&t, &in, 2, t.buttons[0].x, t.buttons[0].y);
    wn_overlay_touch(&o, &t);
    wn_overlay_finish(&o);
    TEST_ASSERT_NOT_EQUAL(empty, o.hash);
    wn_touch_up(&t, &in, 1);
    wn_overlay_clear(&o);
    wn_overlay_touch(&o, &t); /* stick hint when no thumb is down */
    wn_overlay_free(&o);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_model_parts_and_matrices);
    RUN_TEST(test_skeleton_loses_an_arm);
    RUN_TEST(test_remesh_plan_priority);
    RUN_TEST(test_view_lod_columns_and_budget);
    RUN_TEST(test_frustum);
    RUN_TEST(test_lod_meshing_and_append);
    RUN_TEST(test_font_and_overlay);
    return UNITY_END();
}
