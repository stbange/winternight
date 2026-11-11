/* Winternight engine - base types, configuration and small math helpers.
 * Pure C11, no platform dependencies. */
#ifndef WN_BASE_H
#define WN_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Edge length of one voxel in metres. Provisional (design D15): every other
 * piece of code must derive sizes from this constant. */
#ifndef WN_VOXEL_SIZE
#define WN_VOXEL_SIZE 0.1f
#endif

#define WN_CHUNK_BITS 5
#define WN_CHUNK_SIZE (1 << WN_CHUNK_BITS) /* 32 voxels per edge */
#define WN_CHUNK_MASK (WN_CHUNK_SIZE - 1)
#define WN_CHUNK_VOLUME (WN_CHUNK_SIZE * WN_CHUNK_SIZE * WN_CHUNK_SIZE)

/* Fixed simulation step (spec platform-targets: Fixed simulation step). */
#define WN_SIM_HZ 60
#define WN_SIM_DT (1.0f / (float)WN_SIM_HZ)

#define WN_PI 3.14159265358979323846f

#define WN_MIN(a, b) ((a) < (b) ? (a) : (b))
#define WN_MAX(a, b) ((a) > (b) ? (a) : (b))
#define WN_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define WN_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef struct wn_vec3 {
    float x, y, z;
} wn_vec3;

typedef struct wn_ivec3 {
    int32_t x, y, z;
} wn_ivec3;

typedef struct wn_mat4 {
    float m[16]; /* column-major, OpenGL convention */
} wn_mat4;

typedef struct wn_quat {
    float x, y, z, w;
} wn_quat;

static inline wn_vec3 wn_v3(float x, float y, float z)
{
    wn_vec3 v = {x, y, z};
    return v;
}
static inline wn_ivec3 wn_iv3(int32_t x, int32_t y, int32_t z)
{
    wn_ivec3 v = {x, y, z};
    return v;
}
static inline wn_vec3 wn_v3_add(wn_vec3 a, wn_vec3 b) { return wn_v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline wn_vec3 wn_v3_sub(wn_vec3 a, wn_vec3 b) { return wn_v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline wn_vec3 wn_v3_scale(wn_vec3 a, float s) { return wn_v3(a.x * s, a.y * s, a.z * s); }
static inline float wn_v3_dot(wn_vec3 a, wn_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline wn_vec3 wn_v3_cross(wn_vec3 a, wn_vec3 b)
{
    return wn_v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline wn_vec3 wn_v3_lerp(wn_vec3 a, wn_vec3 b, float t)
{
    return wn_v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

/* Integer floor division / modulo that work for negative numbers. */
static inline int32_t wn_floordiv(int32_t a, int32_t b)
{
    int32_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}
static inline int32_t wn_floormod(int32_t a, int32_t b)
{
    int32_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
static inline int32_t wn_floorf_i(float f)
{
    int32_t i = (int32_t)f;
    return (f < (float)i) ? i - 1 : i;
}

/* Deterministic math (design D7): only IEEE-exact operations or our own
 * polynomial approximations are used in the simulation path. */
float wn_sqrtf(float x);
float wn_sinf(float x);
float wn_cosf(float x);
float wn_atan2f(float y, float x);
float wn_absf(float x);
float wn_v3_len(wn_vec3 v);
wn_vec3 wn_v3_norm(wn_vec3 v);
float wn_lerpf(float a, float b, float t);
float wn_smoothstep(float e0, float e1, float x);

/* Matrices (render path only). */
wn_mat4 wn_mat4_identity(void);
wn_mat4 wn_mat4_mul(wn_mat4 a, wn_mat4 b);
wn_mat4 wn_mat4_translate(wn_vec3 t);
wn_mat4 wn_mat4_scale(wn_vec3 s);
wn_mat4 wn_mat4_rotate_y(float a);
wn_mat4 wn_mat4_rotate_x(float a);
wn_mat4 wn_mat4_from_quat(wn_quat q);
wn_mat4 wn_mat4_perspective(float fovy, float aspect, float znear, float zfar);
wn_mat4 wn_mat4_ortho(float l, float r, float b, float t, float n, float f);
wn_mat4 wn_mat4_look_at(wn_vec3 eye, wn_vec3 target, wn_vec3 up);
wn_vec3 wn_mat4_mul_point(wn_mat4 m, wn_vec3 p);
bool wn_mat4_invert(wn_mat4 m, wn_mat4 *out);

/* Quaternions (debris rotation). */
wn_quat wn_quat_identity(void);
wn_quat wn_quat_mul(wn_quat a, wn_quat b);
wn_quat wn_quat_norm(wn_quat q);
wn_quat wn_quat_integrate(wn_quat q, wn_vec3 omega, float dt);
wn_vec3 wn_quat_rotate(wn_quat q, wn_vec3 v);
wn_quat wn_quat_conj(wn_quat q);

#endif
