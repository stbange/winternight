#include "wn/wn_base.h"

#include <math.h>

float wn_sqrtf(float x)
{
    /* IEEE 754 requires sqrt to be correctly rounded, so this is deterministic. */
    return x <= 0.0f ? 0.0f : sqrtf(x);
}

float wn_absf(float x) { return x < 0.0f ? -x : x; }

/* Range-reduced polynomial sine. Accurate to ~1e-6 and identical on every
 * platform because it only uses + - * and comparisons. */
float wn_sinf(float x)
{
    const float two_pi = 2.0f * WN_PI;
    float k = (float)wn_floorf_i(x / two_pi + 0.5f);
    x = x - k * two_pi; /* now in [-pi, pi] */
    float sign = 1.0f;
    if (x < 0.0f) {
        x = -x;
        sign = -1.0f;
    }
    if (x > WN_PI * 0.5f) x = WN_PI - x; /* [0, pi/2] */
    float x2 = x * x;
    float r = x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f + x2 * (1.0f / 362880.0f)))));
    return sign * r;
}

float wn_cosf(float x) { return wn_sinf(x + WN_PI * 0.5f); }

float wn_atan2f(float y, float x)
{
    if (x == 0.0f && y == 0.0f) return 0.0f;
    float ax = wn_absf(x), ay = wn_absf(y);
    float mn = ax < ay ? ax : ay, mx = ax < ay ? ay : ax;
    float a = mn / mx;
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = WN_PI * 0.5f - r;
    if (x < 0.0f) r = WN_PI - r;
    if (y < 0.0f) r = -r;
    return r;
}

float wn_v3_len(wn_vec3 v) { return wn_sqrtf(wn_v3_dot(v, v)); }

wn_vec3 wn_v3_norm(wn_vec3 v)
{
    float l = wn_v3_len(v);
    if (l <= 1e-12f) return wn_v3(0.0f, 0.0f, 0.0f);
    return wn_v3_scale(v, 1.0f / l);
}

float wn_lerpf(float a, float b, float t) { return a + (b - a) * t; }

float wn_smoothstep(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    t = WN_CLAMP(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

wn_mat4 wn_mat4_identity(void)
{
    wn_mat4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

wn_mat4 wn_mat4_mul(wn_mat4 a, wn_mat4 b)
{
    wn_mat4 r;
    for (int c = 0; c < 4; c++) {
        for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    }
    return r;
}

wn_mat4 wn_mat4_translate(wn_vec3 t)
{
    wn_mat4 r = wn_mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

wn_mat4 wn_mat4_scale(wn_vec3 s)
{
    wn_mat4 r = wn_mat4_identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

wn_mat4 wn_mat4_rotate_y(float a)
{
    wn_mat4 r = wn_mat4_identity();
    float c = wn_cosf(a), s = wn_sinf(a);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

wn_mat4 wn_mat4_rotate_x(float a)
{
    wn_mat4 r = wn_mat4_identity();
    float c = wn_cosf(a), s = wn_sinf(a);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

wn_mat4 wn_mat4_from_quat(wn_quat q)
{
    wn_mat4 r = wn_mat4_identity();
    float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    r.m[0] = 1.0f - 2.0f * (yy + zz);
    r.m[1] = 2.0f * (xy + wz);
    r.m[2] = 2.0f * (xz - wy);
    r.m[4] = 2.0f * (xy - wz);
    r.m[5] = 1.0f - 2.0f * (xx + zz);
    r.m[6] = 2.0f * (yz + wx);
    r.m[8] = 2.0f * (xz + wy);
    r.m[9] = 2.0f * (yz - wx);
    r.m[10] = 1.0f - 2.0f * (xx + yy);
    return r;
}

wn_mat4 wn_mat4_perspective(float fovy, float aspect, float zn, float zf)
{
    wn_mat4 r = {{0}};
    float f = 1.0f / (wn_sinf(fovy * 0.5f) / wn_cosf(fovy * 0.5f));
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zf + zn) / (zn - zf);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zf * zn) / (zn - zf);
    return r;
}

wn_mat4 wn_mat4_ortho(float l, float r_, float b, float t, float n, float f)
{
    wn_mat4 r = wn_mat4_identity();
    r.m[0] = 2.0f / (r_ - l);
    r.m[5] = 2.0f / (t - b);
    r.m[10] = -2.0f / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l);
    r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(f + n) / (f - n);
    return r;
}

wn_mat4 wn_mat4_look_at(wn_vec3 eye, wn_vec3 target, wn_vec3 up)
{
    wn_vec3 f = wn_v3_norm(wn_v3_sub(target, eye));
    wn_vec3 s = wn_v3_norm(wn_v3_cross(f, up));
    wn_vec3 u = wn_v3_cross(s, f);
    wn_mat4 r = wn_mat4_identity();
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -wn_v3_dot(s, eye);
    r.m[13] = -wn_v3_dot(u, eye);
    r.m[14] = wn_v3_dot(f, eye);
    return r;
}

wn_vec3 wn_mat4_mul_point(wn_mat4 m, wn_vec3 p)
{
    float x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    float y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    float z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    float w = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    if (w != 0.0f && w != 1.0f) {
        x /= w;
        y /= w;
        z /= w;
    }
    return wn_v3(x, y, z);
}

bool wn_mat4_invert(wn_mat4 a, wn_mat4 *out)
{
    const float *m = a.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0.0f) return false;
    det = 1.0f / det;
    for (int i = 0; i < 16; i++) out->m[i] = inv[i] * det;
    return true;
}

wn_quat wn_quat_identity(void)
{
    wn_quat q = {0.0f, 0.0f, 0.0f, 1.0f};
    return q;
}

wn_quat wn_quat_mul(wn_quat a, wn_quat b)
{
    wn_quat r;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}

wn_quat wn_quat_norm(wn_quat q)
{
    float l = wn_sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l <= 1e-12f) return wn_quat_identity();
    wn_quat r = {q.x / l, q.y / l, q.z / l, q.w / l};
    return r;
}

wn_quat wn_quat_integrate(wn_quat q, wn_vec3 w, float dt)
{
    wn_quat o = {w.x * dt * 0.5f, w.y * dt * 0.5f, w.z * dt * 0.5f, 0.0f};
    wn_quat d = wn_quat_mul(o, q);
    wn_quat r = {q.x + d.x, q.y + d.y, q.z + d.z, q.w + d.w};
    return wn_quat_norm(r);
}

wn_quat wn_quat_conj(wn_quat q)
{
    wn_quat r = {-q.x, -q.y, -q.z, q.w};
    return r;
}

wn_vec3 wn_quat_rotate(wn_quat q, wn_vec3 v)
{
    wn_vec3 u = wn_v3(q.x, q.y, q.z);
    wn_vec3 t = wn_v3_scale(wn_v3_cross(u, v), 2.0f);
    return wn_v3_add(wn_v3_add(v, wn_v3_scale(t, q.w)), wn_v3_cross(u, t));
}
