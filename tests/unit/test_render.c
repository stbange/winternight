#include "unity.h"
#include "wn/wn_core.h"
#include "wn/wn_render.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_integer_upscale_1080p(void)
{
    wn_viewport v = wn_upscale(wn_internal_res[1], 1920, 1080);
    TEST_ASSERT_EQUAL_INT(4, v.scale);
    TEST_ASSERT_EQUAL_INT(1920, v.w);
    TEST_ASSERT_EQUAL_INT(1080, v.h);
    TEST_ASSERT_EQUAL_INT(0, v.x);
    TEST_ASSERT_EQUAL_INT(0, v.y);
}

static void test_letterbox_odd_window(void)
{
    wn_viewport v = wn_upscale(wn_internal_res[1], 1000, 700);
    TEST_ASSERT_EQUAL_INT(2, v.scale);
    TEST_ASSERT_EQUAL_INT(960, v.w);
    TEST_ASSERT_EQUAL_INT(540, v.h);
    TEST_ASSERT_EQUAL_INT(20, v.x);
    TEST_ASSERT_EQUAL_INT(80, v.y);
    /* smaller than the internal image: fractional fit */
    v = wn_upscale(wn_internal_res[1], 240, 300);
    TEST_ASSERT_EQUAL_INT(0, v.scale);
    TEST_ASSERT_EQUAL_INT(240, v.w);
    TEST_ASSERT_EQUAL_INT(135, v.h);
}

static void test_presets(void)
{
    wn_quality m = wn_quality_preset(WN_PRESET_MOBILE), s = wn_quality_preset(WN_PRESET_STANDARD),
               h = wn_quality_preset(WN_PRESET_HIGH);
    TEST_ASSERT_TRUE(m.view_radius < s.view_radius && s.view_radius < h.view_radius);
    TEST_ASSERT_EQUAL_INT(512, m.shadow_size);
    TEST_ASSERT_TRUE(m.particles <= 2000);
    TEST_ASSERT_EQUAL_INT(480, wn_internal_res[m.res_index].w);
    TEST_ASSERT_FALSE(m.vertex_snap);
    TEST_ASSERT_EQUAL_STRING("Mobile", wn_preset_name(WN_PRESET_MOBILE));
    TEST_ASSERT_EQUAL_STRING("?", wn_preset_name((wn_preset_id)9));
    TEST_ASSERT_EQUAL_INT(WN_PRESET_HIGH, wn_quality_preset((wn_preset_id)7).preset);
}

static void test_phone_starts_on_mobile(void)
{
    TEST_ASSERT_EQUAL_INT(WN_PRESET_MOBILE, wn_preset_for_device(true, 412, 915));
    TEST_ASSERT_EQUAL_INT(WN_PRESET_STANDARD, wn_preset_for_device(true, 2048, 1536));
    TEST_ASSERT_EQUAL_INT(WN_PRESET_STANDARD, wn_preset_for_device(false, 1920, 1080));
}

static void test_sustained_slow_frames_lower_quality(void)
{
    wn_quality q = wn_quality_preset(WN_PRESET_STANDARD);
    wn_autoscale a;
    wn_autoscale_init(&a, 16.7f);
    int changes = 0;
    float t = 0.0f;
    /* 2 seconds at 25 ms per frame */
    for (; t < 2.05f; t += 0.025f) changes += wn_autoscale_frame(&a, &q, 25.0f, 0.025f);
    TEST_ASSERT_EQUAL_INT(-1, changes);
    TEST_ASSERT_EQUAL_INT(0, q.res_index);
    /* still slow, but held for 10 s: at most one more step after the hold */
    int more = 0;
    for (float u = 0; u < 9.0f; u += 0.025f) more += wn_autoscale_frame(&a, &q, 25.0f, 0.025f);
    TEST_ASSERT_TRUE(more <= 0 && more >= -5);
    /* fast frames for a long time raise it again */
    int up = 0;
    for (float u = 0; u < 60.0f; u += 0.01f) up += wn_autoscale_frame(&a, &q, 5.0f, 0.01f);
    TEST_ASSERT_TRUE(up > 0);
    a.enabled = false;
    TEST_ASSERT_EQUAL_INT(0, wn_autoscale_frame(&a, &q, 100.0f, 5.0f));
}

static void test_quality_lower_raise_chain(void)
{
    wn_quality q = wn_quality_preset(WN_PRESET_HIGH);
    wn_quality ceil = q;
    int steps = 0;
    while (wn_quality_lower(&q)) steps++;
    TEST_ASSERT_TRUE(steps > 5);
    TEST_ASSERT_EQUAL_INT(0, q.res_index);
    TEST_ASSERT_EQUAL_INT(3, q.view_radius);
    TEST_ASSERT_EQUAL_INT(0, q.glow_divisor);
    int up = 0;
    while (wn_quality_raise(&q, &ceil)) up++;
    TEST_ASSERT_TRUE(up >= steps - 1);
    TEST_ASSERT_EQUAL_INT(ceil.res_index, q.res_index);
    TEST_ASSERT_EQUAL_INT(ceil.view_radius, q.view_radius);
}

static void test_palette_lut(void)
{
    char *txt = wn_read_file(WN_DATA_DIR "/palette.txt", NULL);
    TEST_ASSERT_NOT_NULL(txt);
    wn_palette p;
    char err[128];
    TEST_ASSERT_TRUE_MESSAGE(wn_palette_load(&p, txt, err, sizeof err), err);
    free(txt);
    TEST_ASSERT_EQUAL_INT(64, p.count);
    TEST_ASSERT_EQUAL_INT(0, wn_palette_nearest(&p, 0x000000));
    TEST_ASSERT_EQUAL_HEX32(0xffffff, p.colors[wn_palette_nearest(&p, 0xfefefe)]);
    uint8_t *lut = (uint8_t *)malloc(WN_LUT_SIZE * WN_LUT_SIZE * WN_LUT_SIZE * 4);
    wn_palette_build_lut(&p, lut);
    /* Every LUT entry is a palette colour (spec: Only palette colours on screen). */
    for (int i = 0; i < WN_LUT_SIZE * WN_LUT_SIZE * WN_LUT_SIZE; i++) {
        uint32_t c = ((uint32_t)lut[i * 4] << 16) | ((uint32_t)lut[i * 4 + 1] << 8) | lut[i * 4 + 2];
        bool found = false;
        for (int k = 0; k < p.count && !found; k++) found = p.colors[k] == c;
        TEST_ASSERT_TRUE(found);
    }
    free(lut);
    TEST_ASSERT_FALSE(wn_palette_load(&p, "", err, sizeof err));
    TEST_ASSERT_FALSE(wn_palette_load(&p, "#12345\n", err, sizeof err));
    char big[64 * 9 + 20];
    big[0] = 0;
    for (int i = 0; i < 65; i++) strcat(big, "#102030 ");
    TEST_ASSERT_FALSE(wn_palette_load(&p, big, err, sizeof err));
    TEST_ASSERT_EQUAL_INT(64, p.count); /* unchanged */
}

static void test_bayer(void)
{
    float sum = 0.0f;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            float v = wn_bayer4(x, y);
            TEST_ASSERT_TRUE(v >= 0.0f && v < 1.0f);
            sum += v;
        }
    TEST_ASSERT_EQUAL_FLOAT(7.5f, sum);
    TEST_ASSERT_EQUAL_FLOAT(wn_bayer4(1, 2), wn_bayer4(5, 6));
}

static void test_fire_turns_necromantic(void)
{
    wn_atmos a;
    wn_atmos_default(&a);
    uint32_t warm = a.fire_color;
    wn_atmos_fade_fire(&a, 0x7DFFC0, 2.0f);
    for (int i = 0; i < 60; i++) wn_atmos_step(&a, 1.0f / 60.0f);
    TEST_ASSERT_NOT_EQUAL(warm, a.fire_color);
    TEST_ASSERT_NOT_EQUAL(0x7DFFC0, a.fire_color);
    for (int i = 0; i < 70; i++) wn_atmos_step(&a, 1.0f / 60.0f);
    TEST_ASSERT_EQUAL_HEX32(0x7DFFC0, a.fire_color);
    wn_atmos_fade_fire(&a, warm, 0.0f);
    TEST_ASSERT_EQUAL_HEX32(warm, a.fire_color);
    TEST_ASSERT_EQUAL_HEX32(0x808080, wn_rgb_lerp(0x000000, 0xFFFFFF, 0.502f) & 0xFFFFFF);
    wn_vec3 v = wn_rgb_to_vec(0xFF0000);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, v.x);
}

static void test_camera(void)
{
    wn_camera c = {wn_v3(0, 0, 0), 0.0f, 0.0f, 1.0f, 0.1f, 100.0f};
    wn_vec3 f = wn_camera_forward(&c);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, f.z);
    wn_vec3 p = wn_mat4_mul_point(wn_camera_view(&c), wn_v3(0, 0, 5));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -5.0f, p.z);
    wn_mat4 pr = wn_camera_proj(&c, 16.0f / 9.0f);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, pr.m[11]);
}

static void test_stats(void)
{
    static wn_frame_stats s;
    wn_stats_reset(&s);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_stats_percentile(&s, 95));
    for (int i = 1; i <= 100; i++) wn_stats_push(&s, (float)i, (uint32_t)i, (uint32_t)i * 10);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 95.05f, wn_stats_percentile(&s, 95));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, wn_stats_percentile(&s, 100));
    TEST_ASSERT_EQUAL_UINT32(100, s.max_draw_calls);
    TEST_ASSERT_EQUAL_UINT32(1000, s.triangles);
    for (int i = 0; i < WN_STATS_MAX; i++) wn_stats_push(&s, 1.0f, 1, 1);
    TEST_ASSERT_EQUAL_UINT32(WN_STATS_MAX, s.n);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_integer_upscale_1080p);
    RUN_TEST(test_letterbox_odd_window);
    RUN_TEST(test_presets);
    RUN_TEST(test_phone_starts_on_mobile);
    RUN_TEST(test_sustained_slow_frames_lower_quality);
    RUN_TEST(test_quality_lower_raise_chain);
    RUN_TEST(test_palette_lut);
    RUN_TEST(test_bayer);
    RUN_TEST(test_fire_turns_necromantic);
    RUN_TEST(test_camera);
    RUN_TEST(test_stats);
    return UNITY_END();
}
