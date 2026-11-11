#include "unity.h"
#include "wn/wn_input.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define SC_W 26
#define SC_F 9
#define SC_SPACE 44

static void test_keyboard_moves_and_buttons(void)
{
    wn_input in;
    wn_input_init(&in);
    wn_input_set(&in, WN_SRC_KEY, SC_W, 1.0f);
    wn_input_set(&in, WN_SRC_KEY, SC_SPACE, 1.0f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f.axis[WN_ACT_MOVE_Y]);
    TEST_ASSERT_TRUE(wn_frame_down(&f, WN_ACT_JUMP));
    TEST_ASSERT_TRUE(wn_frame_pressed(&f, WN_ACT_JUMP));
    f = wn_input_poll(&in);
    TEST_ASSERT_TRUE(wn_frame_down(&f, WN_ACT_JUMP));
    TEST_ASSERT_FALSE(wn_frame_pressed(&f, WN_ACT_JUMP));
    TEST_ASSERT_EQUAL_INT(WN_DEVICE_KEYBOARD_MOUSE, in.last_device);
}

static void test_axis_combination_is_clamped(void)
{
    wn_input in;
    wn_input_init(&in);
    wn_input_set(&in, WN_SRC_KEY, SC_W, 1.0f);
    wn_input_set(&in, WN_SRC_KEY, 82 /* up */, 1.0f);
    wn_input_set(&in, WN_SRC_PAD_AXIS, 1, -1.0f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f.axis[WN_ACT_MOVE_Y]);
    wn_input_set(&in, WN_SRC_KEY, SC_W, 0.0f);
    wn_input_set(&in, WN_SRC_KEY, 82, 0.0f);
    wn_input_set(&in, WN_SRC_KEY, 22 /* s */, 1.0f);
    wn_input_set(&in, WN_SRC_PAD_AXIS, 1, 1.0f);
    f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, f.axis[WN_ACT_MOVE_Y]);
}

static void test_mouse_motion_is_consumed(void)
{
    wn_input in;
    wn_input_init(&in);
    wn_input_add(&in, WN_SRC_MOUSE_MOTION, 0, 10.0f);
    wn_input_add(&in, WN_SRC_MOUSE_MOTION, 0, 10.0f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_TRUE(f.axis[WN_ACT_LOOK_X] > 0.0f);
    f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.axis[WN_ACT_LOOK_X]);
    /* out-of-range codes are ignored */
    wn_input_add(&in, WN_SRC_MOUSE_MOTION, -1, 1.0f);
    wn_input_set(&in, WN_SRC_KEY, WN_MAX_SOURCE_CODES, 1.0f);
}

static void test_gamepad_deadzone_and_device(void)
{
    wn_input in;
    wn_input_init(&in);
    wn_input_set(&in, WN_SRC_PAD_AXIS, 0, 0.05f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.axis[WN_ACT_MOVE_X]);
    TEST_ASSERT_EQUAL_INT(WN_DEVICE_KEYBOARD_MOUSE, in.last_device);
    wn_input_set(&in, WN_SRC_PAD_AXIS, 0, 0.8f);
    f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(0.8f, f.axis[WN_ACT_MOVE_X]);
    TEST_ASSERT_EQUAL_INT(WN_DEVICE_GAMEPAD, in.last_device);
    wn_input_set(&in, WN_SRC_XR, 0, 1.0f);
    TEST_ASSERT_EQUAL_INT(WN_DEVICE_XR, in.last_device);
}

static void test_rebinding_attack(void)
{
    wn_input in;
    wn_input_init(&in);
    TEST_ASSERT_TRUE(wn_input_unbind_action(&in, WN_ACT_ATTACK) >= 2);
    TEST_ASSERT_TRUE(wn_input_bind(&in, WN_SRC_KEY, 10 /* g */, WN_ACT_ATTACK, 1.0f));
    wn_input_set(&in, WN_SRC_MOUSE_BUTTON, 1, 1.0f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_FALSE(wn_frame_down(&f, WN_ACT_ATTACK));
    wn_input_set(&in, WN_SRC_KEY, 10, 1.0f);
    f = wn_input_poll(&in);
    TEST_ASSERT_TRUE(wn_frame_down(&f, WN_ACT_ATTACK));
}

static void test_binding_limits(void)
{
    wn_input in;
    wn_input_init(&in);
    wn_input_clear_bindings(&in);
    TEST_ASSERT_FALSE(wn_input_bind(&in, WN_SRC_KEY, -1, WN_ACT_JUMP, 1.0f));
    for (int i = 0; i < WN_MAX_BINDINGS; i++) TEST_ASSERT_TRUE(wn_input_bind(&in, WN_SRC_KEY, i, WN_ACT_JUMP, 1.0f));
    TEST_ASSERT_FALSE(wn_input_bind(&in, WN_SRC_KEY, 1, WN_ACT_JUMP, 1.0f));
}

static void test_load_bindings_from_text(void)
{
    wn_input in;
    wn_input_init(&in);
    char err[200];
    TEST_ASSERT_TRUE(wn_input_load_bindings(&in, "bind key 9 attack\nbind padaxis 1 move_y -1\nbind mouse 3 block\n"
                                                 "bind motion 0 look_x 0.01\nbind pad 0 jump\nbind touch 4 attack\nbind xr 1 attack\n",
                                            err, sizeof err));
    TEST_ASSERT_EQUAL_INT(7, in.nbindings);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, in.bindings[1].scale);
    const char *bad[] = {"bond key 9 attack\n", "bind key 9\n", "bind nose 9 attack\n", "bind key x attack\n",
                         "bind key 9 fly\n", "bind key 9 attack big\n", "bind key 9999 attack\n"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        TEST_ASSERT_FALSE_MESSAGE(wn_input_load_bindings(&in, bad[i], err, sizeof err), bad[i]);
        TEST_ASSERT_EQUAL_INT(7, in.nbindings); /* unchanged */
    }
    wn_action a;
    TEST_ASSERT_TRUE(wn_action_from_name("menu", &a));
    TEST_ASSERT_EQUAL_INT(WN_ACT_MENU, a);
}

static void test_touch_shows_on_coarse_pointer(void)
{
    wn_touch t;
    wn_touch_init(&t, 16.0f / 9.0f, true);
    TEST_ASSERT_TRUE(t.visible);
    wn_touch_init(&t, 16.0f / 9.0f, false);
    TEST_ASSERT_FALSE(t.visible);
}

static void test_touch_stick_moves_proportionally(void)
{
    wn_input in;
    wn_touch t;
    wn_input_init(&in);
    wn_touch_init(&t, 16.0f / 9.0f, false);
    wn_touch_down(&t, &in, 1, 0.3f, 0.7f);
    TEST_ASSERT_TRUE(t.visible);
    wn_touch_move(&t, &in, 1, 0.3f, 0.7f - t.stick_radius * 0.5f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, f.axis[WN_ACT_MOVE_Y]);
    wn_touch_move(&t, &in, 1, 0.3f, 0.7f - t.stick_radius * 3.0f);
    f = wn_input_poll(&in);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, f.axis[WN_ACT_MOVE_Y]);
    wn_touch_up(&t, &in, 1);
    f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.axis[WN_ACT_MOVE_Y]);
    float x, y;
    wn_touch_stick_value(3.0f, 4.0f, 1.0f, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.6f, x);
}

static void test_touch_move_and_look_together(void)
{
    wn_input in;
    wn_touch t;
    wn_input_init(&in);
    float aspect = 16.0f / 9.0f;
    wn_touch_init(&t, aspect, true);
    wn_touch_down(&t, &in, 1, 0.3f, 0.6f);      /* stick */
    wn_touch_down(&t, &in, 2, aspect * 0.6f, 0.3f); /* look */
    wn_touch_move(&t, &in, 1, 0.3f, 0.5f);
    wn_touch_move(&t, &in, 2, aspect * 0.6f + 0.1f, 0.3f);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_TRUE(f.axis[WN_ACT_MOVE_Y] > 0.0f);
    TEST_ASSERT_TRUE(f.axis[WN_ACT_LOOK_X] > 0.0f);
    f = wn_input_poll(&in);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.axis[WN_ACT_LOOK_X]); /* look is a delta */
    TEST_ASSERT_TRUE(f.axis[WN_ACT_MOVE_Y] > 0.0f);       /* stick is held */
    wn_touch_up(&t, &in, 2);
    wn_touch_up(&t, &in, 99); /* unknown finger */
    wn_touch_move(&t, &in, 99, 0, 0);
}

static void test_touch_buttons(void)
{
    wn_input in;
    wn_touch t;
    wn_input_init(&in);
    wn_touch_init(&t, 16.0f / 9.0f, true);
    wn_touch_button b = t.buttons[0];
    wn_touch_down(&t, &in, 5, b.x, b.y);
    wn_input_frame f = wn_input_poll(&in);
    TEST_ASSERT_TRUE(wn_frame_pressed(&f, WN_ACT_ATTACK));
    wn_touch_up(&t, &in, 5);
    f = wn_input_poll(&in);
    TEST_ASSERT_FALSE(wn_frame_down(&f, WN_ACT_ATTACK));
    /* Running out of finger slots is harmless. */
    for (int i = 0; i < WN_TOUCH_MAX_FINGERS + 2; i++) wn_touch_down(&t, &in, 100 + i, 0.1f, 0.1f);
}

static void test_keyboard_hides_touch(void)
{
    wn_touch t;
    wn_touch_init(&t, 1.5f, true);
    wn_touch_notice_device(&t, WN_DEVICE_TOUCH);
    TEST_ASSERT_TRUE(t.visible);
    wn_touch_notice_device(&t, WN_DEVICE_KEYBOARD_MOUSE);
    TEST_ASSERT_FALSE(t.visible);
    wn_touch_init(&t, 1.5f, true);
    wn_touch_notice_device(&t, WN_DEVICE_GAMEPAD);
    TEST_ASSERT_FALSE(t.visible);
}

static void test_replay_round_trip(void)
{
    wn_replay r;
    wn_replay_init(&r, 0x123456789ull);
    for (int i = 0; i < 1000; i++) {
        wn_input_frame f;
        memset(&f, 0, sizeof f);
        f.axis[WN_ACT_MOVE_Y] = (float)i / 1000.0f;
        f.down = (uint32_t)i & 0x3F0u;
        wn_replay_push(&r, &f, (uint32_t)i * 7u);
    }
    size_t n;
    uint8_t *buf = wn_replay_serialize(&r, &n);
    wn_replay back;
    char err[100];
    TEST_ASSERT_TRUE(wn_replay_deserialize(&back, buf, n, err, sizeof err));
    TEST_ASSERT_EQUAL_UINT64(r.seed, back.seed);
    TEST_ASSERT_EQUAL_UINT32(1000, back.count);
    wn_input_frame f;
    uint32_t cs;
    int k = 0;
    while (wn_replay_next(&back, &f, &cs)) {
        TEST_ASSERT_EQUAL_FLOAT(r.frames[k].axis[WN_ACT_MOVE_Y], f.axis[WN_ACT_MOVE_Y]);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)k * 7u, cs);
        k++;
    }
    TEST_ASSERT_EQUAL_INT(1000, k);
    TEST_ASSERT_FALSE(wn_replay_next(&back, &f, NULL));
    /* Corrupted files are rejected. */
    TEST_ASSERT_FALSE(wn_replay_deserialize(&back, buf, n - 3, err, sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "truncated"));
    buf[0] = 'X';
    TEST_ASSERT_FALSE(wn_replay_deserialize(&back, buf, n, err, sizeof err));
    TEST_ASSERT_FALSE(wn_replay_deserialize(&back, buf, 4, err, sizeof err));
    free(buf);
    wn_replay_free(&r);
    wn_replay_free(&back);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_keyboard_moves_and_buttons);
    RUN_TEST(test_axis_combination_is_clamped);
    RUN_TEST(test_mouse_motion_is_consumed);
    RUN_TEST(test_gamepad_deadzone_and_device);
    RUN_TEST(test_rebinding_attack);
    RUN_TEST(test_binding_limits);
    RUN_TEST(test_load_bindings_from_text);
    RUN_TEST(test_touch_shows_on_coarse_pointer);
    RUN_TEST(test_touch_stick_moves_proportionally);
    RUN_TEST(test_touch_move_and_look_together);
    RUN_TEST(test_touch_buttons);
    RUN_TEST(test_keyboard_hides_touch);
    RUN_TEST(test_replay_round_trip);
    return UNITY_END();
}
