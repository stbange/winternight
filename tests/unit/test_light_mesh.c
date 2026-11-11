#include "test_util.h"
#include "unity.h"
#include "wn/wn_core.h"
#include "wn/wn_light.h"
#include "wn/wn_mesh.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void make_world(wn_world *w, int radius)
{
    wn_world_init(w, test_materials(), -1, 1);
    wn_world_set_generator(w, test_gen_flat, NULL);
    w->on_column = wn_light_column_loaded;
    for (int x = -radius; x <= radius; x++)
        for (int z = -radius; z <= radius; z++) wn_world_load_column(w, x, z);
}

/* Builds a closed stone room [x0..x1]x[0..h]x[z0..z1] (walls included). */
static void build_room(wn_world *w, int x0, int z0, int x1, int z1, int h)
{
    for (int y = 0; y <= h; y++)
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) {
                bool wall = x == x0 || x == x1 || z == z0 || z == z1 || y == h;
                if (wall) {
                    wn_world_set(w, x, y, z, wn_vox_make(WN_MAT_STONE, 0));
                    wn_light_voxel_changed(w, x, y, z);
                }
            }
}

static void test_open_ground_gets_sky(void)
{
    wn_world w;
    make_world(&w, 1);
    TEST_ASSERT_EQUAL_UINT32(15, wn_light_get(&w, WN_LIGHT_SKY, 5, 0, 5));
    TEST_ASSERT_EQUAL_UINT32(0, wn_light_get(&w, WN_LIGHT_SKY, 5, -5, 5));
    TEST_ASSERT_TRUE(wn_light_last_work() > 0);
    wn_world_free(&w);
}

static void test_interior_is_darker(void)
{
    /* spec night-lighting: Interior is darker */
    wn_world w;
    make_world(&w, 1);
    build_room(&w, 0, 0, 20, 20, 10);
    TEST_ASSERT_EQUAL_UINT32(0, wn_light_get(&w, WN_LIGHT_SKY, 10, 2, 10));
    TEST_ASSERT_EQUAL_UINT32(15, wn_light_get(&w, WN_LIGHT_SKY, 25, 2, 10));
    wn_world_free(&w);
}

static void test_light_spills_out_of_window(void)
{
    /* spec night-lighting: Light spills out of a window */
    wn_world w;
    make_world(&w, 1);
    build_room(&w, 0, 0, 12, 12, 8);
    /* hearth inside */
    wn_world_set(&w, 6, 1, 6, wn_vox_make(WN_MAT_WINDOW_GLOW, 0));
    wn_light_voxel_changed(&w, 6, 1, 6);
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 6, 2, 6) >= 12);
    /* behind the solid wall: dark */
    TEST_ASSERT_EQUAL_UINT32(0, wn_light_get(&w, WN_LIGHT_BLOCK, 14, 2, 6));
    /* open a window in the +x wall */
    wn_world_set(&w, 12, 2, 6, 0);
    wn_light_voxel_changed(&w, 12, 2, 6);
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 13, 2, 6) > 0);
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 14, 2, 6) > 0);
    /* A voxel outside but shielded by the wall stays darker than one in the window's line. */
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 13, 2, 6) > wn_light_get(&w, WN_LIGHT_BLOCK, 13, 2, 1));
    /* close it again: light outside disappears */
    wn_world_set(&w, 12, 2, 6, wn_vox_make(WN_MAT_STONE, 0));
    wn_light_voxel_changed(&w, 12, 2, 6);
    TEST_ASSERT_EQUAL_UINT32(0, wn_light_get(&w, WN_LIGHT_BLOCK, 14, 2, 6));
    /* removing the emitter darkens the room */
    wn_world_set(&w, 6, 1, 6, 0);
    wn_light_voxel_changed(&w, 6, 1, 6);
    TEST_ASSERT_EQUAL_UINT32(0, wn_light_get(&w, WN_LIGHT_BLOCK, 6, 2, 6));
    wn_world_free(&w);
}

static void test_breaking_wall_lets_light_out(void)
{
    /* spec night-lighting: Breaking a wall lets light out (and sky in) */
    wn_world w;
    make_world(&w, 1);
    build_room(&w, 0, 0, 10, 10, 6);
    wn_world_set(&w, 5, 1, 5, wn_vox_make(WN_MAT_WINDOW_GLOW, 0));
    wn_light_voxel_changed(&w, 5, 1, 5);
    for (int y = 1; y <= 3; y++) {
        wn_world_set(&w, 10, y, 5, 0);
        wn_light_voxel_changed(&w, 10, y, 5);
    }
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 11, 2, 5) > 0);
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_SKY, 9, 2, 5) > 0);
    /* Removing the roof floods the room with sky light. */
    for (int z = 1; z < 10; z++)
        for (int x = 1; x < 10; x++) {
            wn_world_set(&w, x, 6, z, 0);
            wn_light_voxel_changed(&w, x, 6, z);
        }
    TEST_ASSERT_EQUAL_UINT32(15, wn_light_get(&w, WN_LIGHT_SKY, 3, 2, 3));
    wn_world_free(&w);
}

static void test_light_crosses_column_borders(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    wn_world_set_generator(&w, test_gen_flat, NULL);
    w.on_column = wn_light_column_loaded;
    wn_world_load_column(&w, 0, 0);
    /* An emitter next to the border lights the neighbour column when it loads. */
    wn_world_set(&w, 31, 1, 5, wn_vox_make(WN_MAT_WINDOW_GLOW, 0));
    wn_light_voxel_changed(&w, 31, 1, 5);
    wn_world_load_column(&w, 1, 0);
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 33, 1, 5) > 0);
    wn_world_load_column(&w, -1, 0);
    wn_world_load_column(&w, 0, 1);
    wn_world_load_column(&w, 0, -1);
    wn_world_free(&w);
}

static void test_column_with_canopy(void)
{
    /* A column whose top chunk contains an overhang is computed per voxel. */
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 1);
    w.on_column = wn_light_column_loaded;
    wn_world_set_generator(&w, test_gen_flat, NULL);
    wn_world_load_column(&w, 0, 0);
    for (int z = 0; z < 32; z++)
        for (int x = 0; x < 32; x++) wn_world_set(&w, x, 40, z, wn_vox_make(WN_MAT_PINE_NEEDLES, 0));
    /* reload column so the initial pass sees the canopy */
    wn_world_load_column(&w, 1, 0);
    TEST_ASSERT_EQUAL_UINT32(1, w.nchunks / 6);
    for (int cy = -1; cy <= 1; cy++) wn_world_unload_chunk(&w, wn_iv3(0, cy, 0));
    wn_world_load_column(&w, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(0, wn_light_get(&w, WN_LIGHT_SKY, 16, 10, 16)); /* deep under the canopy */
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_SKY, 30, 10, 16) > 0);       /* light leaks in from the side */
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_SKY, 30, 10, 16) < 15);
    TEST_ASSERT_EQUAL_UINT32(15, wn_light_get(&w, WN_LIGHT_SKY, 16, 41, 16));
    wn_world_free(&w);
}

static void test_emissive_uniform_and_solid_columns(void)
{
    wn_world w;
    wn_world_init(&w, test_materials(), -1, 0);
    w.on_column = wn_light_column_loaded;
    wn_world_load_chunk(&w, wn_iv3(0, -1, 0))->uniform = wn_vox_make(WN_MAT_WINDOW_GLOW, 0);
    wn_world_load_chunk(&w, wn_iv3(0, 0, 0));
    wn_light_column_loaded(NULL, &w, 0, 0);
    TEST_ASSERT_EQUAL_UINT32(14, wn_light_get(&w, WN_LIGHT_BLOCK, 3, -3, 3));
    TEST_ASSERT_TRUE(wn_light_get(&w, WN_LIGHT_BLOCK, 3, 2, 3) > 0);
    /* top-of-world sky refill after an edit */
    wn_world_set(&w, 2, 31, 2, wn_vox_make(WN_MAT_STONE, 0));
    wn_light_voxel_changed(&w, 2, 31, 2);
    wn_world_set(&w, 2, 31, 2, 0);
    wn_light_voxel_changed(&w, 2, 31, 2);
    TEST_ASSERT_EQUAL_UINT32(15, wn_light_get(&w, WN_LIGHT_SKY, 2, 31, 2));
    wn_world_free(&w);
}

/* ---------------- Mesh ---------------- */

static wn_voxel *grid_new(int s)
{
    return (wn_voxel *)calloc((size_t)s * s * s, sizeof(wn_voxel));
}
static void grid_set(wn_voxel *g, int s, int x, int y, int z, wn_voxel v) { g[(y * s + z) * s + x] = v; }

static wn_mesh_opts opts_default(void)
{
    wn_mesh_opts o;
    memset(&o, 0, sizeof o);
    o.full_light = 0xF0;
    return o;
}

/* Checks every triangle faces the direction encoded in its normal index. */
static void assert_winding(const wn_mesh *m)
{
    static const int n[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (uint32_t i = 0; i < m->nindices; i += 3) {
        const wn_vertex *a = &m->verts[m->indices[i]], *b = &m->verts[m->indices[i + 1]], *c = &m->verts[m->indices[i + 2]];
        float e1[3] = {(float)b->x - a->x, (float)b->y - a->y, (float)b->z - a->z};
        float e2[3] = {(float)c->x - a->x, (float)c->y - a->y, (float)c->z - a->z};
        float cr[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
        int ni = a->nao & 7;
        float dot = cr[0] * n[ni][0] + cr[1] * n[ni][1] + cr[2] * n[ni][2];
        TEST_ASSERT_TRUE_MESSAGE(dot > 0.0f, "triangle winding does not match normal");
    }
}

static void test_hidden_faces_culled(void)
{
    /* spec retro-rendering: Hidden faces are culled (2x2x2 block -> 6 merged quads) */
    const int S = 6;
    wn_voxel *g = grid_new(S);
    for (int y = 2; y < 4; y++)
        for (int z = 2; z < 4; z++)
            for (int x = 2; x < 4; x++) grid_set(g, S, x, y, z, wn_vox_make(WN_MAT_STONE, 0));
    /* Uniform colour so faces can merge: use a single-colour material. */
    wn_materials mats = *test_materials();
    mats.m[WN_MAT_STONE].ramp_n = 1;
    wn_mesh m;
    wn_mesh_init(&m);
    wn_mesh_opts o = opts_default();
    wn_mesh_grid(&m, g, S, S, S, &mats, &o);
    TEST_ASSERT_EQUAL_UINT32(6 * 4, m.nverts);
    TEST_ASSERT_EQUAL_UINT32(6 * 6, m.nindices);
    assert_winding(&m);
    /* every quad spans 2 voxels = 32 sixteenths */
    for (uint32_t i = 0; i < m.nverts; i++) {
        TEST_ASSERT_TRUE(m.verts[i].x == 16 || m.verts[i].x == 48);
    }
    wn_mesh_free(&m);
    free(g);
}

static void test_mesh_is_deterministic(void)
{
    wn_world w;
    make_world(&w, 1);
    for (int i = 0; i < 200; i++) {
        uint32_t h = wn_hash_ivec3(i, 0, 0, 3);
        wn_world_set(&w, (int)(h % 32), (int)((h >> 8) % 12), (int)((h >> 16) % 32),
                     wn_vox_make(i % 3 ? WN_MAT_STONE : WN_MAT_SNOW, i % 5 == 0 ? 6 : 0));
    }
    wn_mesh a, b;
    wn_mesh_init(&a);
    wn_mesh_init(&b);
    wn_mesh_chunk(&a, &w, wn_iv3(0, 0, 0));
    wn_mesh_chunk(&b, &w, wn_iv3(0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(a.nverts, b.nverts);
    TEST_ASSERT_TRUE(a.nverts > 0);
    TEST_ASSERT_EQUAL_MEMORY(a.verts, b.verts, a.nverts * sizeof(wn_vertex));
    TEST_ASSERT_EQUAL_MEMORY(a.indices, b.indices, a.nindices * sizeof(uint32_t));
    assert_winding(&a);
    wn_mesh_chunk(&a, &w, wn_iv3(0, -1, 0)); /* ground surface */
    TEST_ASSERT_TRUE(a.nverts > 0);
    wn_mesh_free(&a);
    wn_mesh_free(&b);
    wn_world_free(&w);
}

static void test_partial_voxel_faces(void)
{
    const int S = 5;
    wn_voxel *g = grid_new(S);
    grid_set(g, S, 2, 1, 2, wn_vox_make(WN_MAT_STONE, 0));   /* ground under */
    grid_set(g, S, 2, 2, 2, wn_vox_make(WN_MAT_SNOW, 8));    /* half snow on top */
    grid_set(g, S, 3, 2, 2, wn_vox_make(WN_MAT_SNOW, 4));    /* lower neighbour */
    grid_set(g, S, 1, 2, 2, wn_vox_make(WN_MAT_SNOW, 0));    /* full neighbour hides that side */
    wn_mesh m;
    wn_mesh_init(&m);
    wn_mesh_opts o = opts_default();
    wn_mesh_grid(&m, g, S, S, S, test_materials(), &o);
    assert_winding(&m);
    /* The half-height voxel's top face sits at y = 1*16 + 8. */
    bool found_top = false, found_step = false;
    for (uint32_t i = 0; i < m.nverts; i++) {
        if ((m.verts[i].nao & 7) == 2 && m.verts[i].y == 24 && m.verts[i].x >= 16 && m.verts[i].x <= 32) found_top = true;
        if ((m.verts[i].nao & 7) == 0 && m.verts[i].x == 32 && (m.verts[i].y == 20 || m.verts[i].y == 24)) found_step = true;
    }
    TEST_ASSERT_TRUE(found_top);
    TEST_ASSERT_TRUE(found_step);
    wn_mesh_free(&m);
    free(g);
}

static void test_emissive_flag_and_colors(void)
{
    const int S = 3;
    wn_voxel *g = grid_new(S);
    grid_set(g, S, 1, 1, 1, wn_vox_make(WN_MAT_WINDOW_GLOW, 0));
    wn_mesh m;
    wn_mesh_init(&m);
    wn_mesh_opts o = opts_default();
    wn_mesh_grid(&m, g, S, S, S, test_materials(), &o);
    TEST_ASSERT_EQUAL_UINT32(24, m.nverts);
    TEST_ASSERT_EQUAL_UINT8(WN_VFLAG_EMISSIVE, m.verts[0].flags);
    TEST_ASSERT_EQUAL_UINT8(0xF0, m.verts[0].light);
    wn_mesh_clear(&m);
    TEST_ASSERT_EQUAL_UINT32(0, m.nverts);
    wn_mesh_free(&m);
    free(g);

    wn_materials *mats = test_materials();
    /* stable colours */
    wn_voxel snow = wn_vox_make(WN_MAT_SNOW, 0);
    TEST_ASSERT_EQUAL_HEX32(wn_voxel_color(mats, snow, 1, 2, 3), wn_voxel_color(mats, snow, 1, 2, 3));
    /* colours vary between voxels but come from the ramp */
    int different = 0;
    for (int i = 0; i < 64; i++) different += wn_voxel_color(mats, snow, i, 0, 0) != wn_voxel_color(mats, snow, 0, 0, 0);
    TEST_ASSERT_TRUE(different > 0);
    /* stains and damage change the colour */
    uint32_t base = wn_voxel_color(mats, snow, 0, 0, 0);
    TEST_ASSERT_NOT_EQUAL(base, wn_voxel_color(mats, wn_vox_set_stain(snow, WN_STAIN_BLOOD, 63), 0, 0, 0));
    TEST_ASSERT_NOT_EQUAL(base, wn_voxel_color(mats, wn_vox_set_damage(snow, 4), 0, 0, 0));
    wn_voxel st = wn_vox_make(WN_MAT_STONE, 0);
    TEST_ASSERT_NOT_EQUAL(wn_voxel_color(mats, st, 0, 0, 0), wn_voxel_color(mats, wn_vox_set_damage(st, 15), 0, 0, 0));
    wn_materials empty = *mats;
    empty.m[WN_MAT_STONE].ramp_n = 0;
    TEST_ASSERT_EQUAL_HEX32(0xFF00FF, wn_voxel_color(&empty, st, 0, 0, 0));
    TEST_ASSERT_EQUAL_HEX32(0xF8FCF8, wn_565_to_rgb(wn_rgb_to_565(0xFFFFFF)) & 0xF8FCF8);
}

static void test_ao_darkens_corners(void)
{
    const int S = 5;
    wn_voxel *g = grid_new(S);
    /* floor + wall: floor faces next to the wall get AO */
    for (int z = 1; z < 4; z++)
        for (int x = 1; x < 4; x++) grid_set(g, S, x, 1, z, wn_vox_make(WN_MAT_STONE, 0));
    for (int z = 1; z < 4; z++) grid_set(g, S, 3, 2, z, wn_vox_make(WN_MAT_STONE, 0));
    wn_mesh m;
    wn_mesh_init(&m);
    wn_mesh_opts o = opts_default();
    wn_mesh_grid(&m, g, S, S, S, test_materials(), &o);
    int dark = 0;
    for (uint32_t i = 0; i < m.nverts; i++)
        if ((m.verts[i].nao & 7) == 2 && ((m.verts[i].nao >> 3) & 3) < 3) dark++;
    TEST_ASSERT_TRUE(dark > 0);
    wn_mesh_free(&m);
    free(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_open_ground_gets_sky);
    RUN_TEST(test_interior_is_darker);
    RUN_TEST(test_light_spills_out_of_window);
    RUN_TEST(test_breaking_wall_lets_light_out);
    RUN_TEST(test_light_crosses_column_borders);
    RUN_TEST(test_column_with_canopy);
    RUN_TEST(test_emissive_uniform_and_solid_columns);
    RUN_TEST(test_hidden_faces_culled);
    RUN_TEST(test_mesh_is_deterministic);
    RUN_TEST(test_partial_voxel_faces);
    RUN_TEST(test_emissive_flag_and_colors);
    RUN_TEST(test_ao_darkens_corners);
    return UNITY_END();
}
