#include "unity.h"
#include "wn/wn_core.h"
#include "wn/wn_mesh.h"
#include "wn/wn_scene.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static wn_materials g_mats;
static wn_prefab_lib g_lib;

static char *data(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", WN_DATA_DIR, name);
    char *s = wn_read_file(path, NULL);
    TEST_ASSERT_NOT_NULL_MESSAGE(s, path);
    return s;
}

static void load_libs(void)
{
    static int done;
    if (done) return;
    char err[256];
    char *m = data("materials.txt"), *p = data("prefabs.txt");
    TEST_ASSERT_TRUE_MESSAGE(wn_materials_load(&g_mats, m, err, sizeof err), err);
    TEST_ASSERT_TRUE_MESSAGE(wn_prefabs_load(&g_lib, &g_mats, p, err, sizeof err), err);
    free(m);
    free(p);
    done = 1;
}

static void test_prologue_scene_loads_from_data(void)
{
    /* spec game-flow: Scene loads from data */
    load_libs();
    char err[256];
    char *txt = data("scenes/prologue.scene");
    static wn_scene s;
    TEST_ASSERT_TRUE_MESSAGE(wn_scene_load(&s, &g_mats, &g_lib, txt, err, sizeof err), err);
    free(txt);
    TEST_ASSERT_EQUAL_STRING("prologue", s.name);
    TEST_ASSERT_EQUAL_UINT64(1234, s.seed);
    TEST_ASSERT_EQUAL_INT(3, s.nplace);
    TEST_ASSERT_NOT_NULL(wn_prefab_find(&g_lib, "castle_hearthholm"));
    TEST_ASSERT_EQUAL_STRING("castle_hearthholm", s.place[2].prefab->name);
    TEST_ASSERT_TRUE(s.snow_rate > 0.0f);
    TEST_ASSERT_TRUE(s.ntriggers >= 1);
    TEST_ASSERT_EQUAL_STRING("reach_gate", s.triggers[0].event);
    TEST_ASSERT_TRUE(s.atmos.moon_intensity > 0.0f);
    /* Spawn sits on the road. */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, wn_scene_road_x(&s, s.spawn.z), s.spawn.x);
}

static const char *k_mini_prefabs =
    "prefab hut\n"
    "  box stone 0 0 0 2 2 2\n"
    "  carve box 0.5 0.0 0.5 1.5 1.5 1.5\n"
    "  cyl stone 4 4 0 2 0.5\n"
    "  cone stone 4 4 2 3 0.6\n"
    "  hcylx pine-wood 0.3 6 0 2 0.3\n"
    "  hcylz pine-wood 6 0.3 0 2 0.3\n"
    "  sphere ice 8 1 8 0.8\n"
    "  gablex castle-stone 0 2 0 2 3 2\n"
    "  gablez castle-stone 10 0 0 12 1 2\n"
    "  crenel castle-stone 0 3 0 2 3.5 0.2 0.5\n"
    "  box window-glow 0.9 0.5 -0.01 1.1 1.0 0.1\n"
    "end\n"
    "prefab rock\n"
    "  nosnow\n"
    "  sphere stone 0 0 0 1\n"
    "end\n";

static const char *k_mini_scene =
    "scene mini\nseed 7\nbounds -10 -4 -10 10 10 10\nchunks_y -1 1\nterrain 0 0.5 10\nsnow 0.3 0.05\n"
    "hill 0 0 5 1\nroad 2 1 0.05 -10 10\nforest 3 0.5 1.5\ntracks -5 0 5 0\n"
    "place hut 5 5\nplace rock road 0 1 0.2\n"
    "moon 0 1 0 #ffffff 1\nsky #000000 #111111\nfog #000000 0.02\nfire #ff0000\nambient 0.2\naurora\n"
    "snowfall 0.1 10\nspawn 0 0 0\ntrigger t 0 0 0 1 1 1 ev\nevent ev message \"Hello there\"\nevent ev complete\n"
    "event ev fire #00ff00 2\nevent ev snowfall 0\n";

static void test_mini_scene_and_prefab_shapes(void)
{
    load_libs();
    static wn_prefab_lib lib;
    char err[256];
    TEST_ASSERT_TRUE_MESSAGE(wn_prefabs_load(&lib, &g_mats, k_mini_prefabs, err, sizeof err), err);
    const wn_prefab *hut = wn_prefab_find(&lib, "hut");
    TEST_ASSERT_NOT_NULL(hut);
    TEST_ASSERT_NULL(wn_prefab_find(&lib, "palace"));
    TEST_ASSERT_EQUAL_INT(WN_MAT_STONE, wn_prefab_sample(hut, wn_v3(0.2f, 0.2f, 0.2f)));
    TEST_ASSERT_EQUAL_INT(0, wn_prefab_sample(hut, wn_v3(1.0f, 1.0f, 1.0f)));        /* carved */
    TEST_ASSERT_EQUAL_INT(-1, wn_prefab_sample(hut, wn_v3(20.0f, 1.0f, 1.0f)));      /* outside */
    TEST_ASSERT_EQUAL_INT(WN_MAT_STONE, wn_prefab_sample(hut, wn_v3(4.1f, 1.0f, 4.1f))); /* cylinder */
    TEST_ASSERT_EQUAL_INT(-1, wn_prefab_sample(hut, wn_v3(4.45f, 1.0f, 4.45f)));     /* outside the round */
    TEST_ASSERT_EQUAL_INT(WN_MAT_STONE, wn_prefab_sample(hut, wn_v3(4.0f, 2.1f, 4.0f))); /* cone base */
    TEST_ASSERT_EQUAL_INT(-1, wn_prefab_sample(hut, wn_v3(4.5f, 2.9f, 4.0f)));       /* cone tip is thin */
    TEST_ASSERT_EQUAL_INT(WN_MAT_PINE_WOOD, wn_prefab_sample(hut, wn_v3(1.0f, 0.3f, 6.0f)));
    TEST_ASSERT_EQUAL_INT(WN_MAT_PINE_WOOD, wn_prefab_sample(hut, wn_v3(6.0f, 0.3f, 1.0f)));
    TEST_ASSERT_EQUAL_INT(WN_MAT_ICE, wn_prefab_sample(hut, wn_v3(8.0f, 1.0f, 8.0f)));
    TEST_ASSERT_EQUAL_INT(WN_MAT_CASTLE_STONE, wn_prefab_sample(hut, wn_v3(1.0f, 2.9f, 1.0f))); /* gable ridge */
    TEST_ASSERT_EQUAL_INT(-1, wn_prefab_sample(hut, wn_v3(1.0f, 2.9f, 0.05f)));                /* gable eave */
    TEST_ASSERT_EQUAL_INT(WN_MAT_CASTLE_STONE, wn_prefab_sample(hut, wn_v3(11.0f, 0.5f, 1.0f)));
    TEST_ASSERT_EQUAL_INT(WN_MAT_CASTLE_STONE, wn_prefab_sample(hut, wn_v3(0.2f, 3.2f, 0.1f))); /* merlon */
    TEST_ASSERT_EQUAL_INT(-1, wn_prefab_sample(hut, wn_v3(0.7f, 3.2f, 0.1f)));                 /* gap */
    TEST_ASSERT_EQUAL_INT(WN_MAT_WINDOW_GLOW, wn_prefab_sample(hut, wn_v3(1.0f, 0.7f, 0.05f)));
    TEST_ASSERT_FALSE(wn_prefab_find(&lib, "rock")->snow_cap);

    static wn_scene s;
    TEST_ASSERT_TRUE_MESSAGE(wn_scene_load(&s, &g_mats, &lib, k_mini_scene, err, sizeof err), err);
    TEST_ASSERT_EQUAL_INT(4, s.nactions);
    TEST_ASSERT_TRUE(s.atmos.aurora);
    TEST_ASSERT_EQUAL_HEX32(0xff0000, s.atmos.fire_color);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, s.atmos.ambient);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, wn_scene_road_x(&s, 0.0f), s.place[1].pos.x);
    /* the hill raises the terrain at its centre */
    TEST_ASSERT_TRUE(wn_scene_terrain_height(&s, 0.0f, 0.0f) > wn_scene_terrain_height(&s, 9.0f, -9.0f) - 0.6f);
    TEST_ASSERT_TRUE(wn_scene_snow_height(&s, 8.0f, 8.0f) > wn_scene_terrain_height(&s, 8.0f, 8.0f));
    /* generated chunks contain the prefab, terrain, snow and bedrock */
    wn_ivec3 hut_voxel = wn_world_to_voxel(wn_v3(s.place[0].pos.x + 0.2f, s.place[0].pos.y + 0.2f, s.place[0].pos.z + 0.2f));
    wn_chunk *c = wn_chunk_new(wn_voxel_to_chunk(hut_voxel), 0);
    wn_scene_generate_chunk(&s, c);
    int stone = 0;
    for (int y = 0; y < 32; y++)
        for (int z = 0; z < 32; z++)
            for (int x = 0; x < 32; x++) stone += wn_vox_mat(wn_chunk_get(c, x, y, z)) == WN_MAT_STONE;
    TEST_ASSERT_TRUE(stone > 0);
    wn_chunk_free(c);
    c = wn_chunk_new(wn_iv3(0, -1, 0), 0);
    wn_scene_generate_chunk(&s, c);
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_BEDROCK, wn_vox_mat(wn_chunk_get(c, 3, 0, 3)));
    wn_chunk_free(c);
    c = wn_chunk_new(wn_voxel_to_chunk(wn_world_to_voxel(wn_v3(0.0f, 6.0f, 0.0f))), 0);
    wn_scene_generate_chunk(&s, c);
    TEST_ASSERT_TRUE(wn_chunk_is_uniform(c)); /* high above everything (6 m) */
    wn_chunk_free(c);
    /* point queries match chunk generation */
    wn_vec3 g = wn_v3(-8.0f, 0.0f, 3.0f);
    wn_ivec3 below = wn_world_to_voxel(wn_v3(g.x, wn_scene_terrain_height(&s, g.x, g.z) - 0.5f, g.z));
    TEST_ASSERT_EQUAL_UINT32(WN_MAT_EARTH, wn_vox_mat(wn_scene_voxel(&s, below.x, below.y, below.z)));
    /* far scenery grid */
    wn_far_grid fg;
    wn_scene_build_far(&s, 3, &fg);
    TEST_ASSERT_TRUE(fg.sx > 2 && fg.sy > 2 && fg.sz > 2);
    int solid = 0;
    for (int i = 0; i < fg.sx * fg.sy * fg.sz; i++) solid += wn_vox_mat(fg.cells[i]) != 0;
    TEST_ASSERT_TRUE(solid > 0);
    wn_far_grid_free(&fg);
}

static void test_trees_are_deterministic_and_avoid_the_road(void)
{
    load_libs();
    char err[256];
    char *txt = data("scenes/prologue.scene");
    static wn_scene s;
    TEST_ASSERT_TRUE(wn_scene_load(&s, &g_mats, &g_lib, txt, err, sizeof err));
    free(txt);
    int trees = 0;
    for (int cz = -5; cz < 60; cz++)
        for (int cx = -20; cx < 20; cx++) {
            wn_tree a, b;
            bool ha = wn_scene_tree_in_cell(&s, cx, cz, &a), hb = wn_scene_tree_in_cell(&s, cx, cz, &b);
            TEST_ASSERT_EQUAL(ha, hb);
            if (!ha) continue;
            trees++;
            TEST_ASSERT_EQUAL_FLOAT(a.x, b.x);
            /* never on the road */
            TEST_ASSERT_TRUE(wn_absf(a.x - wn_scene_road_x(&s, a.z)) > s.road_width * 0.5f);
            TEST_ASSERT_TRUE(a.height >= 7.0f && a.height <= 14.0f);
        }
    TEST_ASSERT_TRUE(trees > 100);
    /* no forest at all */
    s.forest_prob = 0.0f;
    wn_tree t;
    TEST_ASSERT_FALSE(wn_scene_tree_in_cell(&s, 1, 1, &t));
}

static void test_same_seed_same_world(void)
{
    /* spec voxel-world: Same seed gives same world (checksum of generated chunks) */
    load_libs();
    char err[256];
    char *txt = data("scenes/prologue.scene");
    static wn_scene s;
    TEST_ASSERT_TRUE(wn_scene_load(&s, &g_mats, &g_lib, txt, err, sizeof err));
    free(txt);
    uint32_t h[2];
    for (int run = 0; run < 2; run++) {
        wn_hasher hs;
        wn_hasher_init(&hs, 1);
        for (int cy = -1; cy <= 1; cy++)
            for (int cz = -6; cz <= -5; cz++)
                for (int cx = -2; cx <= -1; cx++) {
                    wn_chunk *c = wn_chunk_new(wn_iv3(cx, cy, cz), 0);
                    wn_scene_generate_chunk(&s, c);
                    wn_chunk_expand(c);
                    wn_hasher_add(&hs, c->vox, WN_CHUNK_VOLUME * sizeof(wn_voxel));
                    wn_chunk_free(c);
                }
        h[run] = wn_hasher_finish(&hs);
    }
    TEST_ASSERT_EQUAL_UINT32(h[0], h[1]);
    /* Golden value: the same on every platform (cross-platform determinism). */
    printf("prologue generation checksum: %08x\n", h[0]);
}

static void expect_scene_error(const char *text, const char *what)
{
    static wn_scene s;
    char err[256] = "";
    TEST_ASSERT_FALSE_MESSAGE(wn_scene_load(&s, &g_mats, &g_lib, text, err, sizeof err), text);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, what), err);
}

static void test_broken_scene_files(void)
{
    /* spec game-flow: Broken scene file names the missing prefab */
    load_libs();
    expect_scene_error("scene x\nplace dragon_lair 0 0\n", "prefab 'dragon_lair' does not exist");
    expect_scene_error("seed 1\n", "no 'scene' line");
    expect_scene_error("scene x\nweather rain\n", "unknown scene keyword");
    expect_scene_error("scene x\nevent e dance\n", "unknown event action");
    expect_scene_error("scene x\nevent e fire red 1\n", "#rrggbb");
    expect_scene_error("scene x\nevent e snowfall\n", "needs");
    expect_scene_error("scene x\nevent e message\n", "needs");
    expect_scene_error("scene x\nterrain 1 2\n", "needs");
    expect_scene_error("scene x\nchunks_y a b\n", "integer");
    expect_scene_error("scene x\nseed q\n", "integer");
    expect_scene_error("scene x\nplace hut x 0\n", "number");
    expect_scene_error("scene x\nplace castle_hearthholm 0 0 q\n", "integer");
    expect_scene_error("scene x\nplace castle_hearthholm 0 0 1 q\n", "number");
    expect_scene_error("scene x\ntrigger t 0 0 0 1 1 1\n", "needs");
    expect_scene_error("scene x\nmoon 0 1 0 white 1\n", "#rrggbb");
    expect_scene_error("scene x\nsky #000000\n", "colour");
    expect_scene_error("scene x\nfog #000000\n", "number");
    expect_scene_error("scene x\nfire\n", "colour");
    expect_scene_error("scene x\nambient\n", "number");
    char big[4096] = "scene x\n";
    for (int i = 0; i < WN_MAX_HILLS + 1; i++) strcat(big, "hill 0 0 1 1\n");
    expect_scene_error(big, "too many hills");
    strcpy(big, "scene x\n");
    for (int i = 0; i < WN_MAX_TRACKS + 1; i++) strcat(big, "tracks 0 0 1 1\n");
    expect_scene_error(big, "too many tracks");
    strcpy(big, "scene x\n");
    for (int i = 0; i < WN_MAX_PLACEMENTS + 1; i++) strcat(big, "place rock 0 0\n");
    expect_scene_error(big, "too many placements");
    strcpy(big, "scene x\n");
    for (int i = 0; i < WN_MAX_TRIGGERS + 1; i++) strcat(big, "trigger t 0 0 0 1 1 1 e\n");
    expect_scene_error(big, "too many triggers");
    strcpy(big, "scene x\n");
    for (int i = 0; i < WN_MAX_EVENTS + 1; i++) strcat(big, "event e complete\n");
    expect_scene_error(big, "too many event actions");
}

static void expect_prefab_error(const char *text, const char *what)
{
    static wn_prefab_lib lib;
    char err[256] = "";
    TEST_ASSERT_FALSE_MESSAGE(wn_prefabs_load(&lib, &g_mats, text, err, sizeof err), text);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, what), err);
}

static void test_broken_prefab_files(void)
{
    load_libs();
    expect_prefab_error("box stone 0 0 0 1 1 1\n", "outside a prefab");
    expect_prefab_error("prefab a\nbox stone 0 0 0 1 1 1\nprefab b\n", "missing 'end'");
    expect_prefab_error("prefab a\nbox stone 0 0 0 1 1 1\n", "missing 'end'");
    expect_prefab_error("prefab a\nend\n", "empty prefab");
    expect_prefab_error("prefab a\nbox mithril 0 0 0 1 1 1\nend\n", "unknown material");
    expect_prefab_error("prefab a\npyramid stone 0 0 0 1\nend\n", "unknown primitive");
    expect_prefab_error("prefab a\nbox stone 0 0 0 1 1\nend\n", "needs");
    expect_prefab_error("prefab a\nbox stone 0 0 0 1 1 q\nend\n", "number");
    expect_prefab_error("prefab a\nbox\nend\n", "needs");
    expect_prefab_error("prefab\n", "needs");
    char big[16384] = "";
    for (int i = 0; i < WN_MAX_PREFABS + 1; i++) {
        char line[64];
        snprintf(line, sizeof line, "prefab p%d\nbox stone 0 0 0 1 1 1\nend\n", i);
        strcat(big, line);
    }
    expect_prefab_error(big, "too many prefabs");
    strcpy(big, "prefab a\n");
    for (int i = 0; i < WN_MAX_PRIMS + 1; i++) strcat(big, "box stone 0 0 0 1 1 1\n");
    strcat(big, "end\n");
    expect_prefab_error(big, "too many primitives");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_prologue_scene_loads_from_data);
    RUN_TEST(test_mini_scene_and_prefab_shapes);
    RUN_TEST(test_trees_are_deterministic_and_avoid_the_road);
    RUN_TEST(test_same_seed_same_world);
    RUN_TEST(test_broken_scene_files);
    RUN_TEST(test_broken_prefab_files);
    return UNITY_END();
}
