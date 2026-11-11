#include "unity.h"
#include "wn/wn_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_rng_is_deterministic(void)
{
    wn_rng a, b;
    wn_rng_seed(&a, 1234, 1);
    wn_rng_seed(&b, 1234, 1);
    for (int i = 0; i < 100; i++) TEST_ASSERT_EQUAL_UINT32(wn_rng_u32(&a), wn_rng_u32(&b));
    /* Known first value guards against accidental algorithm changes across platforms. */
    wn_rng c;
    wn_rng_seed(&c, 42, 54);
    TEST_ASSERT_EQUAL_HEX32(0xa15c02b7u, wn_rng_u32(&c));
}

static void test_rng_streams_differ(void)
{
    wn_rng a, b;
    wn_rng_seed(&a, 1234, 1);
    wn_rng_seed(&b, 1234, 2);
    int same = 0;
    for (int i = 0; i < 32; i++) same += wn_rng_u32(&a) == wn_rng_u32(&b);
    TEST_ASSERT_LESS_THAN(2, same);
}

static void test_rng_ranges(void)
{
    wn_rng r;
    wn_rng_seed(&r, 7, 3);
    TEST_ASSERT_EQUAL_UINT32(0, wn_rng_range(&r, 0));
    for (int i = 0; i < 1000; i++) {
        TEST_ASSERT_LESS_THAN_UINT32(10, wn_rng_range(&r, 10));
        float f = wn_rng_f01(&r);
        TEST_ASSERT_TRUE(f >= 0.0f && f < 1.0f);
        float g = wn_rng_frange(&r, -2.0f, 3.0f);
        TEST_ASSERT_TRUE(g >= -2.0f && g < 3.0f);
    }
}

static void test_hashing(void)
{
    const char *s = "winternight";
    TEST_ASSERT_EQUAL_UINT32(wn_hash32(s, strlen(s), 0), wn_hash32(s, strlen(s), 0));
    TEST_ASSERT_NOT_EQUAL(wn_hash32(s, strlen(s), 0), wn_hash32(s, strlen(s), 1));
    wn_hasher h;
    wn_hasher_init(&h, 0);
    wn_hasher_add(&h, "winter", 6);
    wn_hasher_add(&h, "night", 5);
    TEST_ASSERT_EQUAL_UINT32(wn_hash32(s, strlen(s), 0), wn_hasher_finish(&h));
    TEST_ASSERT_EQUAL_UINT32(wn_hash_ivec3(1, 2, 3, 9), wn_hash_ivec3(1, 2, 3, 9));
    TEST_ASSERT_NOT_EQUAL(wn_hash_ivec3(1, 2, 3, 9), wn_hash_ivec3(1, 2, 4, 9));
}

static void test_clock_steps(void)
{
    wn_clock c;
    wn_clock_init(&c);
    TEST_ASSERT_EQUAL_INT(1, wn_clock_advance(&c, 1.0 / 60.0 + 1e-9));
    TEST_ASSERT_EQUAL_INT(0, wn_clock_advance(&c, 0.005));
    TEST_ASSERT_EQUAL_INT(2, wn_clock_advance(&c, 0.03));
    TEST_ASSERT_EQUAL_UINT64(3, c.step);
    /* Negative deltas are ignored and huge ones clamped. */
    TEST_ASSERT_EQUAL_INT(0, wn_clock_advance(&c, -1.0));
    int n = wn_clock_advance(&c, 10.0);
    TEST_ASSERT_TRUE(n >= 14 && n <= 16);
    float a = wn_clock_alpha(&c);
    TEST_ASSERT_TRUE(a >= 0.0f && a < 1.0f);
}

static void test_clock_frame_rate_independent(void)
{
    /* 144 fps and 20 fps over the same total time give the same step count. */
    wn_clock a, b;
    wn_clock_init(&a);
    wn_clock_init(&b);
    int sa = 0, sb = 0;
    for (int i = 0; i < 144 * 3; i++) sa += wn_clock_advance(&a, 1.0 / 144.0);
    for (int i = 0; i < 20 * 3; i++) sb += wn_clock_advance(&b, 1.0 / 20.0);
    TEST_ASSERT_INT_WITHIN(1, sa, sb);
}

static int g_sink_calls;
static wn_log_level g_sink_level;
static void sink(wn_log_level l, const char *msg, void *user)
{
    (void)msg;
    (void)user;
    g_sink_calls++;
    g_sink_level = l;
}

static void test_logging(void)
{
    wn_log_set_sink(sink, NULL);
    wn_log_set_level(WN_LOG_WARN);
    g_sink_calls = 0;
    wn_log(WN_LOG_INFO, "hidden %d", 1);
    TEST_ASSERT_EQUAL_INT(0, g_sink_calls);
    wn_log(WN_LOG_ERROR, "broken %s", "thing");
    TEST_ASSERT_EQUAL_INT(1, g_sink_calls);
    TEST_ASSERT_EQUAL_INT(WN_LOG_ERROR, g_sink_level);
    TEST_ASSERT_EQUAL_STRING("broken thing", wn_log_last_error());
    wn_log_set_sink(NULL, NULL);
    wn_log(WN_LOG_ERROR, "to stderr");
    wn_log_set_level(WN_LOG_INFO);
}

static void test_parser_records(void)
{
    const char *src = "# comment\n\nscene prologue\n  seed 1234  # trailing\nname \"Castle Hearthholm\" 2.5 #ff8000\n";
    wn_parser p;
    wn_parser_init(&p, src, strlen(src));
    TEST_ASSERT_TRUE(wn_parser_next(&p));
    TEST_ASSERT_TRUE(wn_parser_is(&p, 0, "scene"));
    TEST_ASSERT_EQUAL_STRING("prologue", p.fields[1]);
    TEST_ASSERT_TRUE(wn_parser_next(&p));
    int32_t seed;
    TEST_ASSERT_TRUE(wn_parser_int(&p, 1, &seed));
    TEST_ASSERT_EQUAL_INT(1234, seed);
    TEST_ASSERT_EQUAL_INT(2, p.nfields);
    TEST_ASSERT_TRUE(wn_parser_next(&p));
    TEST_ASSERT_EQUAL_STRING("Castle Hearthholm", p.fields[1]);
    float f;
    uint32_t rgb;
    TEST_ASSERT_TRUE(wn_parser_float(&p, 2, &f));
    TEST_ASSERT_EQUAL_FLOAT(2.5f, f);
    TEST_ASSERT_TRUE(wn_parser_color(&p, 3, &rgb));
    TEST_ASSERT_EQUAL_HEX32(0xff8000, rgb);
    TEST_ASSERT_TRUE(wn_parser_expect_fields(&p, 4));
    TEST_ASSERT_FALSE(wn_parser_is(&p, 9, "x"));
    TEST_ASSERT_FALSE(wn_parser_next(&p));
    TEST_ASSERT_EQUAL_STRING("", p.error);
}

static void expect_error(const char *src, const char *what)
{
    wn_parser p;
    wn_parser_init(&p, src, strlen(src));
    while (wn_parser_next(&p)) {
        int32_t i;
        float f;
        uint32_t c;
        if (wn_parser_is(&p, 0, "i")) wn_parser_int(&p, 1, &i);
        if (wn_parser_is(&p, 0, "f")) wn_parser_float(&p, 1, &f);
        if (wn_parser_is(&p, 0, "c")) wn_parser_color(&p, 1, &c);
        if (wn_parser_is(&p, 0, "n")) wn_parser_expect_fields(&p, 3);
        if (p.error[0]) break;
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(p.error, what), p.error);
}

static void test_parser_errors(void)
{
    expect_error("i abc\n", "not an integer");
    expect_error("i\n", "missing integer");
    expect_error("f 1.5x\n", "not a number");
    expect_error("f\n", "missing number");
    expect_error("c red\n", "#rrggbb");
    expect_error("c #12345g\n", "#rrggbb");
    expect_error("c\n", "missing colour");
    expect_error("n 1\n", "needs 2 values");
    expect_error("x \"open\n", "unterminated quote");
    expect_error("x 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25\n", "too many fields");
    char longline[700];
    memset(longline, 'a', sizeof longline - 1);
    longline[sizeof longline - 1] = '\0';
    expect_error(longline, "line too long");
    /* The first error wins. */
    wn_parser p;
    wn_parser_init(&p, "", 0);
    p.line_no = 3;
    wn_parser_fail(&p, "first");
    wn_parser_fail(&p, "second");
    TEST_ASSERT_EQUAL_STRING("line 3: first", p.error);
}

static void test_read_file(void)
{
    size_t n = 0;
    char *s = wn_read_file(WN_DATA_DIR "/materials.txt", &n);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL_size_t(n, strlen(s));
    free(s);
    TEST_ASSERT_NULL(wn_read_file("/nonexistent/file.txt", NULL));
}

static void test_math(void)
{
    for (float x = -20.0f; x < 20.0f; x += 0.37f) {
        TEST_ASSERT_FLOAT_WITHIN(2e-4f, sinf(x), wn_sinf(x));
        TEST_ASSERT_FLOAT_WITHIN(2e-4f, cosf(x), wn_cosf(x));
    }
    for (float y = -3.0f; y <= 3.0f; y += 0.5f)
        for (float x = -3.0f; x <= 3.0f; x += 0.5f)
            if (x != 0.0f || y != 0.0f) TEST_ASSERT_FLOAT_WITHIN(2e-3f, atan2f(y, x), wn_atan2f(y, x));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_atan2f(0.0f, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, wn_sqrtf(9.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_sqrtf(-1.0f));
    TEST_ASSERT_EQUAL_INT(-2, wn_floordiv(-3, 2));
    TEST_ASSERT_EQUAL_INT(1, wn_floordiv(3, 2));
    TEST_ASSERT_EQUAL_INT(-1, wn_floordiv(1, -2));
    TEST_ASSERT_EQUAL_INT(1, wn_floormod(-3, 2));
    TEST_ASSERT_EQUAL_INT(-1, wn_floormod(3, -2));
    TEST_ASSERT_EQUAL_INT(-1, wn_floorf_i(-0.5f));
    TEST_ASSERT_EQUAL_INT(2, wn_floorf_i(2.0f));
    wn_vec3 n = wn_v3_norm(wn_v3(3, 0, 4));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.6f, n.x);
    wn_vec3 z = wn_v3_norm(wn_v3(0, 0, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, z.x);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, wn_v3_len(wn_v3(3, 4, 0)));
    wn_vec3 l = wn_v3_lerp(wn_v3(0, 0, 0), wn_v3(2, 4, 6), 0.5f);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, l.y);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, wn_lerpf(1.0f, 2.0f, 0.5f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wn_smoothstep(0, 1, -1));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, wn_smoothstep(0, 1, 2));
    TEST_ASSERT_EQUAL_FLOAT(0.5f, wn_smoothstep(0, 1, 0.5f));
    TEST_ASSERT_EQUAL_FLOAT(2.0f, wn_absf(-2.0f));
}

static void test_matrices(void)
{
    wn_mat4 t = wn_mat4_translate(wn_v3(1, 2, 3));
    wn_mat4 s = wn_mat4_scale(wn_v3(2, 2, 2));
    wn_vec3 p = wn_mat4_mul_point(wn_mat4_mul(t, s), wn_v3(1, 1, 1));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, p.x);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, p.z);
    wn_mat4 inv;
    TEST_ASSERT_TRUE(wn_mat4_invert(wn_mat4_mul(t, s), &inv));
    wn_vec3 q = wn_mat4_mul_point(inv, p);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, q.y);
    wn_mat4 zero = {{0}};
    TEST_ASSERT_FALSE(wn_mat4_invert(zero, &inv));
    wn_vec3 r = wn_mat4_mul_point(wn_mat4_rotate_y(WN_PI * 0.5f), wn_v3(1, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, r.z);
    wn_vec3 rx = wn_mat4_mul_point(wn_mat4_rotate_x(WN_PI * 0.5f), wn_v3(0, 1, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, rx.z);
    wn_mat4 view = wn_mat4_look_at(wn_v3(0, 0, 5), wn_v3(0, 0, 0), wn_v3(0, 1, 0));
    wn_vec3 vp = wn_mat4_mul_point(view, wn_v3(0, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -5.0f, vp.z);
    wn_mat4 proj = wn_mat4_perspective(1.0f, 1.0f, 0.1f, 100.0f);
    wn_vec3 pp = wn_mat4_mul_point(proj, wn_v3(0, 0, -100.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, pp.z);
    wn_mat4 o = wn_mat4_ortho(-1, 1, -1, 1, 0, 10);
    wn_vec3 op = wn_mat4_mul_point(o, wn_v3(1, 1, -10));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, op.z);
    wn_mat4 id = wn_mat4_from_quat(wn_quat_identity());
    TEST_ASSERT_EQUAL_FLOAT(1.0f, id.m[0]);
}

static void test_quaternions(void)
{
    wn_quat q = wn_quat_identity();
    /* Integrate a quarter turn around Y. */
    for (int i = 0; i < 1000; i++) q = wn_quat_integrate(q, wn_v3(0, WN_PI * 0.5f, 0), 0.001f);
    wn_vec3 v = wn_quat_rotate(q, wn_v3(1, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, -1.0f, v.z);
    wn_mat4 m = wn_mat4_from_quat(q);
    wn_vec3 mv = wn_mat4_mul_point(m, wn_v3(1, 0, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, v.z, mv.z);
    wn_quat c = wn_quat_mul(q, wn_quat_conj(q));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, c.w);
    wn_quat zero = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_FLOAT(1.0f, wn_quat_norm(zero).w);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rng_is_deterministic);
    RUN_TEST(test_rng_streams_differ);
    RUN_TEST(test_rng_ranges);
    RUN_TEST(test_hashing);
    RUN_TEST(test_clock_steps);
    RUN_TEST(test_clock_frame_rate_independent);
    RUN_TEST(test_logging);
    RUN_TEST(test_parser_records);
    RUN_TEST(test_parser_errors);
    RUN_TEST(test_read_file);
    RUN_TEST(test_math);
    RUN_TEST(test_matrices);
    RUN_TEST(test_quaternions);
    return UNITY_END();
}
