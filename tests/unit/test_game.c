#include "unity.h"
#include "wn/wn_core.h"
#include "wn/wn_game.h"
#include "wn/wn_gametest.h"
#include "wn/wn_xr.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static wn_game *new_game(void)
{
    static wn_game g;
    if (g.loaded) wn_game_free(&g);
    char err[256];
    TEST_ASSERT_TRUE_MESSAGE(wn_game_init_from_dir(&g, WN_DATA_DIR, "prologue", 0, err, sizeof err), err);
    g.view_radius = 2;
    wn_game_stream_all(&g);
    return &g;
}

static wn_input_frame move(float fwd, uint32_t buttons)
{
    wn_input_frame f;
    memset(&f, 0, sizeof f);
    f.axis[WN_ACT_MOVE_Y] = fwd;
    f.down = buttons;
    f.pressed = buttons;
    return f;
}

static bool g_follow_road;

static void steps(wn_game *g, int n, wn_input_frame f)
{
    for (int i = 0; i < n; i++) {
        if (g_follow_road) {
            float z = g->player.pos.z + 3.0f;
            g->cam.yaw = wn_atan2f(wn_scene_road_x(&g->scene, z) - g->player.pos.x, 3.0f);
        }
        wn_game_step(g, &f);
        wn_game_stream(g, 1 << 20);
        f.pressed = 0;
    }
}

static void test_init_errors(void)
{
    static wn_game g;
    char err[256];
    TEST_ASSERT_FALSE(wn_game_init_from_dir(&g, "/nonexistent", "prologue", 0, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "cannot read"));
    TEST_ASSERT_FALSE(wn_game_init(&g, "nonsense", "", "", 0, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "materials"));
    char *m = wn_read_file(WN_DATA_DIR "/materials.txt", NULL);
    TEST_ASSERT_FALSE(wn_game_init(&g, m, "box\n", "", 0, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "prefabs"));
    TEST_ASSERT_FALSE(wn_game_init(&g, m, "", "scene x\nplace nothing 0 0\n", 0, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "scene"));
    free(m);
    wn_game_free(&g); /* not loaded: no-op */
}

static void test_new_game_opens_in_the_forest(void)
{
    /* spec game-flow: New game opens in the forest */
    wn_game *g = new_game();
    TEST_ASSERT_EQUAL_STRING("prologue", g->scene.name);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, g->scene.spawn.z, g->player.pos.z);
    TEST_ASSERT_TRUE(g->scene.place[2].pos.z - g->player.pos.z > 150.0f); /* castle far away */
    TEST_ASSERT_FALSE(g->complete);
    /* night at the start, no cut-scene */
    TEST_ASSERT_EQUAL_UINT64(0, g->step);
}

static void test_walking_moves_and_leaves_footprints(void)
{
    wn_game *g = new_game();
    steps(g, 10, move(0, 0)); /* settle on the ground */
    TEST_ASSERT_TRUE(g->player.on_ground);
    wn_vec3 p0 = g->player.pos;
    uint32_t edits = g->world.edit_serial;
    g_follow_road = true;
    steps(g, 120, move(1, 0));
    float d = wn_v3_len(wn_v3_sub(g->player.pos, p0));
    TEST_ASSERT_TRUE(d > 2.0f);
    TEST_ASSERT_TRUE(g->world.edit_serial > edits); /* footprints carved into the snow */
    TEST_ASSERT_TRUE(g->player.walk_phase > 0.0f);
    /* running is faster than walking */
    wn_vec3 p1 = g->player.pos;
    steps(g, 60, move(1, 1u << WN_ACT_RUN));
    TEST_ASSERT_TRUE(wn_v3_len(wn_v3_sub(g->player.pos, p1)) > d / 2.0f);
    g_follow_road = false;
}

static void test_deep_snow_slows_the_warrior(void)
{
    /* spec snow-simulation: Deep snow is slower */
    wn_game *g = new_game();
    float on_road = wn_snow_speed_factor(&g->snow, &g->world, wn_v3_add(g->player.pos, wn_v3(0, 0.3f, 0)));
    wn_vec3 off = wn_v3(g->player.pos.x + 1.2f * WN_CHUNK_SIZE * WN_VOXEL_SIZE, 0, g->player.pos.z); /* still loaded */
    off.y = wn_scene_terrain_height(&g->scene, off.x, off.z);
    float deep = wn_snow_speed_factor(&g->snow, &g->world, wn_v3_add(off, wn_v3(0, 0.3f, 0)));
    TEST_ASSERT_TRUE(deep < on_road);
}

static void test_jump(void)
{
    wn_game *g = new_game();
    steps(g, 10, move(0, 0));
    float y0 = g->player.pos.y;
    steps(g, 12, move(0, 1u << WN_ACT_JUMP));
    TEST_ASSERT_TRUE(g->player.pos.y > y0 + 0.2f);
    steps(g, 60, move(0, 0));
    TEST_ASSERT_TRUE(g->player.on_ground);
}

static void test_blocked_road_and_chopping_through(void)
{
    /* spec game-flow: Blocked road; tasks 9.3 chop through the blocking tree */
    wn_game *g = new_game();
    float z = 61.5f, x = wn_scene_road_x(&g->scene, z);
    wn_game_teleport(g, wn_v3(x, wn_scene_terrain_height(&g->scene, x, z) + 0.5f, z), 0.0f);
    wn_game_stream_all(g);
    steps(g, 240, move(1, 0));
    TEST_ASSERT_TRUE_MESSAGE(g->player.pos.z < 63.5f, "the fallen pine blocks the road");
    /* jumping does not clear it either */
    for (int i = 0; i < 4; i++) steps(g, 30, move(1, 1u << WN_ACT_JUMP));
    TEST_ASSERT_TRUE(g->player.pos.z < 63.5f);
    /* chop: repeated attacks while pushing forward */
    for (int i = 0; i < 12 && g->player.pos.z < 64.5f; i++) {
        steps(g, 1, move(1, 1u << WN_ACT_ATTACK));
        steps(g, 29, move(1, 0));
    }
    steps(g, 120, move(1, 0));
    TEST_ASSERT_TRUE_MESSAGE(g->player.pos.z > 64.5f, "the warrior chopped through");
    TEST_ASSERT_TRUE(g->player.swing_count > 0);
}

static void test_reaching_the_gate_completes_the_scene(void)
{
    /* spec game-flow: Reaching the castle gate */
    wn_game *g = new_game();
    const wn_trigger *t = &g->scene.triggers[0];
    float x = (t->mn.x + t->mx.x) * 0.5f, z = t->mn.z - 1.0f;
    wn_game_teleport(g, wn_v3(x, wn_scene_terrain_height(&g->scene, x, z) + 0.5f, z), 0.0f);
    wn_game_stream_all(g);
    steps(g, 120, move(1, 0));
    TEST_ASSERT_TRUE(g->complete);
    TEST_ASSERT_EQUAL_STRING("Castle Hearthholm", g->message);
    TEST_ASSERT_TRUE(g->scene.triggers[0].fired);
    TEST_ASSERT_EQUAL_UINT32(1, g->events_fired);
}

static void test_events(void)
{
    wn_game *g = new_game();
    TEST_ASSERT_EQUAL_INT(0, wn_game_fire_event(g, "nothing"));
    /* add actions at runtime */
    wn_event_action *a = &g->scene.actions[g->scene.nactions++];
    memset(a, 0, sizeof *a);
    snprintf(a->event, sizeof a->event, "necro");
    a->kind = WN_EVT_FIRE_COLOR;
    a->color = 0x7DFFC0;
    a->value = 0.5f;
    a = &g->scene.actions[g->scene.nactions++];
    memset(a, 0, sizeof *a);
    snprintf(a->event, sizeof a->event, "necro");
    a->kind = WN_EVT_SNOWFALL;
    a->value = 0.0f;
    TEST_ASSERT_EQUAL_INT(2, wn_game_fire_event(g, "necro"));
    steps(g, 60, move(0, 0));
    TEST_ASSERT_EQUAL_HEX32(0x7DFFC0, g->atmos.fire_color);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->fx.snowfall);
    a->value = 0.2f;
    wn_game_fire_event(g, "necro");
    TEST_ASSERT_TRUE(g->fx.snowfall > 0.0f);
}

static void test_camera_avoids_walls(void)
{
    /* spec game-flow: Camera avoids walls */
    wn_game *g = new_game();
    steps(g, 5, move(0, 0));
    wn_camera free_cam = wn_game_camera(g, 1.0f);
    float open = g->cam.actual_dist;
    TEST_ASSERT_FLOAT_WITHIN(0.3f, g->cam.distance, open);
    /* build a stone wall right behind the warrior */
    wn_edit_ctx e;
    wn_edit_begin(&e, &g->world);
    wn_vec3 back = wn_v3_sub(g->player.pos, wn_v3(wn_sinf(g->cam.yaw) * 1.0f, 0, wn_cosf(g->cam.yaw) * 1.0f));
    wn_ivec3 c = wn_world_to_voxel(back);
    for (int y = -5; y < 40; y++)
        for (int dx = -12; dx <= 12; dx++)
            for (int dz = -12; dz <= 12; dz++)
                if (abs(dx) + abs(dz) > 3) wn_edit_set(&e, c.x + dx, c.y + y, c.z + dz, wn_vox_make(WN_MAT_STONE, 0));
    wn_edit_end(&e);
    wn_camera walled = wn_game_camera(g, 1.0f);
    TEST_ASSERT_TRUE(g->cam.actual_dist < open - 0.5f);
    TEST_ASSERT_TRUE(wn_v3_len(wn_v3_sub(walled.pos, free_cam.pos)) > 0.5f);
    /* first-person and fixed cameras */
    g->cam.first_person = true;
    wn_camera fp = wn_game_camera(g, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, g->player.pos.y + 1.65f, fp.pos.y);
    g->cam.fixed = true;
    g->cam.fixed_pos = wn_v3(1, 2, 3);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, wn_game_camera(g, 0.5f).pos.y);
}

static void test_warrior_pose_and_model(void)
{
    wn_game *g = new_game();
    TEST_ASSERT_TRUE(g->warrior.nparts >= 8);
    steps(g, 30, move(1, 0));
    wn_game_pose_warrior(g, 0.5f);
    int legl = wn_model_find_part(&g->warrior, "leg_l");
    TEST_ASSERT_NOT_EQUAL(0.0f, g->warrior.parts[legl].rx);
    steps(g, 1, move(0, 1u << WN_ACT_ATTACK));
    steps(g, 3, move(0, 0));
    wn_game_pose_warrior(g, 1.0f);
    steps(g, 20, move(0, 0));
    wn_game_pose_warrior(g, 1.0f);
    steps(g, 5, move(0, 1u << WN_ACT_BLOCK));
    TEST_ASSERT_TRUE(g->player.blocking);
    wn_game_pose_warrior(g, 1.0f);
    wn_materials empty;
    memset(&empty, 0, sizeof empty);
    wn_model m;
    wn_build_warrior(&m, &empty); /* missing materials fall back */
    TEST_ASSERT_TRUE(m.nparts >= 8);
    wn_model_free(&m);
}

static void test_brushing_trees_drops_snow_and_blood_helper(void)
{
    wn_game *g = new_game();
    /* walk straight into the forest beside the road */
    g->cam.yaw = WN_PI * 0.5f;
    steps(g, 400, move(1, 0));
    TEST_ASSERT_TRUE(g->fx.count + g->fx.ncosmetic > 0); /* snow flakes and clumps */
    wn_game_spill_blood(g, wn_v3_add(g->player.pos, wn_v3(0.5f, 1.0f, 0.0f)), 6.0f);
    steps(g, 120, move(0, 0));
    TEST_ASSERT_TRUE(g->fx.stains_written > 0);
}

static void test_simulation_is_deterministic(void)
{
    /* spec platform-targets: Same replay, same result; spec input-controls: Replay reproduces a run */
    wn_replay rec;
    wn_replay_init(&rec, 1234);
    uint32_t sum_a;
    {
        wn_game *g = new_game();
        wn_rng r;
        wn_rng_seed(&r, 99, 1);
        for (int i = 0; i < 400; i++) {
            wn_input_frame f = move(1, 0);
            f.axis[WN_ACT_LOOK_X] = wn_rng_frange(&r, -0.02f, 0.03f);
            if (i % 50 == 10) f.pressed = f.down = 1u << WN_ACT_ATTACK;
            wn_replay_push(&rec, &f, 0);
            wn_game_step(g, &f);
            wn_game_stream(g, 1 << 20);
        }
        sum_a = wn_game_checksum(g);
    }
    size_t n;
    uint8_t *buf = wn_replay_serialize(&rec, &n);
    wn_replay back;
    char err[64];
    TEST_ASSERT_TRUE(wn_replay_deserialize(&back, buf, n, err, sizeof err));
    wn_game *g = new_game();
    wn_input_frame f;
    while (wn_replay_next(&back, &f, NULL)) {
        wn_game_step(g, &f);
        wn_game_stream(g, 1 << 20);
    }
    TEST_ASSERT_EQUAL_UINT32(sum_a, wn_game_checksum(g));
    free(buf);
    wn_replay_free(&rec);
    wn_replay_free(&back);
}

static void test_xr_rules(void)
{
    /* spec webxr-mode: Swing attack - damage follows swing speed */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_swing_damage(1.0f));
    TEST_ASSERT_TRUE(wn_xr_swing_damage(3.0f) > 0.0f);
    TEST_ASSERT_TRUE(wn_xr_swing_damage(5.0f) > wn_xr_swing_damage(3.0f));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, wn_xr_swing_damage(50.0f));
    wn_xr_turn t;
    wn_xr_turn_init(&t, true);
    TEST_ASSERT_EQUAL_FLOAT(t.snap_angle, wn_xr_turn_step(&t, 0.9f, WN_SIM_DT));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_turn_step(&t, 0.9f, WN_SIM_DT)); /* held: one snap only */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_turn_step(&t, 0.5f, WN_SIM_DT));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_turn_step(&t, 0.0f, WN_SIM_DT));
    TEST_ASSERT_EQUAL_FLOAT(-t.snap_angle, wn_xr_turn_step(&t, -0.9f, WN_SIM_DT));
    wn_xr_turn_init(&t, false);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_turn_step(&t, 0.1f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, wn_xr_turn_step(&t, 0.5f, 1.0f));
    /* spec webxr-mode: Comfort vignette */
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_vignette(false, 1.0f, 2.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_xr_vignette(true, 0.0f, 0.0f));
    TEST_ASSERT_TRUE(wn_xr_vignette(true, 1.0f, 0.0f) > 0.3f);
    TEST_ASSERT_TRUE(wn_xr_vignette(true, 1.0f, 5.0f) <= 0.8f);
    /* a tracked swing attacks the world */
    wn_game *g = new_game();
    TEST_ASSERT_FALSE(wn_game_xr_swing(g, 1.0f));
    TEST_ASSERT_TRUE(wn_game_xr_swing(g, 4.0f));
    TEST_ASSERT_FALSE(wn_game_xr_swing(g, 4.0f)); /* already swinging */
    TEST_ASSERT_TRUE(g->player.swing_power > 1.0f);
    steps(g, 30, move(0, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->player.swing);
}

/* ---------------- game test runner ---------------- */

static void test_gametest_script(void)
{
    static wn_gametest t;
    char err[200];
    const char *script =
        "scene prologue\nseed 1234\npreset high\nreplay x.wnrec\nend 50\n"
        "at 0 input move_y 1\nat 1 press attack\nat 3 release attack\nat 2 look 0.1 -0.1\n"
        "at 4 teleport 0 ground 0 0\nat 5 teleport 1 2 3 0\nat 6 camera 1 2 3 0 0\nat 7 follow\n"
        "at 8 expect player.z > -100\nat 8 expect complete == 0\nat 9 expect complete\nat 9 expect bogus > 1\n"
        "at 10 capture shot tolerance 4 maxdiff 2\nat 10 blood 0 1 0 3\nat 11 event reach_gate\nat 12 stream\n"
        "at 13 autopilot run chop 30\nat 20 autopilot off\n"
        "at 14 expect player.x >= -100\nat 14 expect player.y <= 100\nat 14 expect events < 100\nat 14 expect step != 0\n"
        "at 14 expect debris >= 0\nat 14 expect stains >= 0\nat 14 expect camera.dist >= 0\nat 14 expect snow_depth >= 0\n"
        "at 15 expect player.z < -1000\n";
    TEST_ASSERT_TRUE_MESSAGE(wn_gametest_load(&t, script, err, sizeof err), err);
    TEST_ASSERT_EQUAL_INT(WN_PRESET_HIGH, t.preset);
    TEST_ASSERT_EQUAL_UINT32(50, t.end_step);
    TEST_ASSERT_EQUAL_STRING("x.wnrec", t.replay);
    wn_game *g = new_game();
    int captures = 0;
    for (uint32_t s = 0; s <= 20; s++) {
        wn_gt_cmd caps[4];
        captures += wn_gametest_run(&t, g, s, caps, 4);
        wn_gametest_steer(&t, g);
        wn_input_frame f = wn_gametest_input(&t, s);
        if (s == 1) TEST_ASSERT_TRUE(wn_frame_pressed(&f, WN_ACT_ATTACK));
        if (s == 2) TEST_ASSERT_FALSE(wn_frame_pressed(&f, WN_ACT_ATTACK));
        if (s == 0) TEST_ASSERT_EQUAL_FLOAT(1.0f, f.axis[WN_ACT_MOVE_Y]);
        wn_game_step(g, &f);
    }
    TEST_ASSERT_EQUAL_INT(1, captures);
    /* "expect complete" at step 9 fails (event fired at 11), bogus var fails, z < -1000 fails */
    TEST_ASSERT_EQUAL_INT(3, t.failures);
    TEST_ASSERT_NOT_NULL(strstr(t.log, "unknown variable"));
    TEST_ASSERT_TRUE(g->complete);
    TEST_ASSERT_FALSE(g->cam.fixed);
    float v;
    TEST_ASSERT_FALSE(wn_gametest_var(g, "nope", &v));
    /* operators */
    const char *ops = "at 0 expect step < 1\nat 0 expect step <= 0\nat 0 expect step > -1\nat 0 expect step >= 0\n"
                      "at 0 expect step == 0\nat 0 expect step != 1\n";
    TEST_ASSERT_TRUE(wn_gametest_load(&t, ops, err, sizeof err));
    g->step = 0;
    wn_gametest_run(&t, g, 0, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, t.failures);
    /* log overflow is safe */
    for (int i = 0; i < 100; i++) wn_gametest_fail(&t, "failure number %d with some padding text to fill the log", i);
    TEST_ASSERT_EQUAL_INT(100, t.failures);
}

static void test_gametest_errors(void)
{
    static wn_gametest t;
    char err[200];
    const char *bad[] = {"at x input move_y 1\n", "at 1 input fly 1\n", "at 1 teleport 1 2\n", "at 1 teleport 1 q 2 0\n",
                         "at 1 teleport q 0 0 0\n", "at 1 teleport 1 0 q 0\n", "at 1 look 1\n", "at 1 expect\n",
                         "at 1 expect a ~ 1\n", "at 1 expect a > q\n", "at 1 capture\n", "at 1 capture a tolerance q\n",
                         "at 1 capture a size 3\n", "at 1 blood 1 2\n", "at 1 blood 1 2 3 q\n", "at 1 event\n",
                         "at 1 dance\n", "dance\n", "preset ultra\n", "scene\n", "seed q\n", "replay\n", "end q\n",
                         "at 1 camera 1 2\n", "at 1 camera 1 2 3 4 q\n", "at 1 autopilot\n", "at 1 autopilot run chop q\n",
                         "at 1 input move_y q\n"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
        TEST_ASSERT_FALSE_MESSAGE(wn_gametest_load(&t, bad[i], err, sizeof err), bad[i]);
    char big[16384] = "";
    for (int i = 0; i < WN_GT_MAX_CMDS + 1; i++) strcat(big, "at 1 stream\n");
    TEST_ASSERT_FALSE(wn_gametest_load(&t, big, err, sizeof err));
    TEST_ASSERT_TRUE(wn_gametest_load(&t, "preset mobile\npreset standard\n", err, sizeof err));
}

static void test_image_compare_and_png(void)
{
    /* spec quality-gates: Visual regression detected */
    const int w = 8, h = 4;
    uint8_t a[8 * 4 * 4], b[8 * 4 * 4], diff[8 * 4 * 4];
    for (int i = 0; i < w * h * 4; i++) a[i] = b[i] = (uint8_t)(i * 7);
    wn_image_diff d = wn_image_compare(a, b, w, h, 0, diff);
    TEST_ASSERT_EQUAL_INT(0, d.differing);
    b[4 * 5 + 1] = (uint8_t)(b[4 * 5 + 1] + 40);
    d = wn_image_compare(a, b, w, h, 8, diff);
    TEST_ASSERT_EQUAL_INT(1, d.differing);
    TEST_ASSERT_EQUAL_INT(40, d.max_delta);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f / 32.0f, d.fraction);
    TEST_ASSERT_EQUAL_UINT8(255, diff[4 * 5]);
    d = wn_image_compare(a, b, w, h, 50, NULL);
    TEST_ASSERT_EQUAL_INT(0, d.differing);
    d = wn_image_compare(a, b, 0, 0, 0, NULL);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, d.fraction);
    char path[512];
    snprintf(path, sizeof path, "%s/wn_png_test.png", WN_TEST_TMP);
    TEST_ASSERT_TRUE(wn_png_write(path, a, w, h));
    int rw, rh;
    uint8_t *r = wn_png_read(path, &rw, &rh);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_INT(w, rw);
    TEST_ASSERT_EQUAL_MEMORY(a, r, sizeof a);
    free(r);
    TEST_ASSERT_NULL(wn_png_read("/nonexistent.png", &rw, &rh));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_errors);
    RUN_TEST(test_new_game_opens_in_the_forest);
    RUN_TEST(test_walking_moves_and_leaves_footprints);
    RUN_TEST(test_deep_snow_slows_the_warrior);
    RUN_TEST(test_jump);
    RUN_TEST(test_blocked_road_and_chopping_through);
    RUN_TEST(test_reaching_the_gate_completes_the_scene);
    RUN_TEST(test_events);
    RUN_TEST(test_camera_avoids_walls);
    RUN_TEST(test_warrior_pose_and_model);
    RUN_TEST(test_brushing_trees_drops_snow_and_blood_helper);
    RUN_TEST(test_simulation_is_deterministic);
    RUN_TEST(test_xr_rules);
    RUN_TEST(test_gametest_script);
    RUN_TEST(test_gametest_errors);
    RUN_TEST(test_image_compare_and_png);
    return UNITY_END();
}
