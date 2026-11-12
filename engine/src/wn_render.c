#include "wn/wn_render.h"

#include "wn/wn_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const wn_res wn_internal_res[WN_RES_COUNT] = {{320, 180}, {480, 270}, {640, 360}};

wn_viewport wn_upscale(wn_res in, int win_w, int win_h)
{
    wn_viewport v;
    int sx = win_w / in.w, sy = win_h / in.h;
    int s = WN_MIN(sx, sy);
    if (s >= 1) {
        v.scale = s;
        v.w = in.w * s;
        v.h = in.h * s;
    } else {
        /* Window smaller than the internal image: fit while keeping aspect. */
        v.scale = 0;
        float f = WN_MIN((float)win_w / (float)in.w, (float)win_h / (float)in.h);
        v.w = (int)((float)in.w * f);
        v.h = (int)((float)in.h * f);
    }
    v.x = (win_w - v.w) / 2;
    v.y = (win_h - v.h) / 2;
    return v;
}

/* ---- Presets ---- */

wn_quality wn_quality_preset(wn_preset_id id)
{
    wn_quality q;
    memset(&q, 0, sizeof q);
    q.preset = id;
    q.palette = true;
    switch (id) {
    case WN_PRESET_MOBILE:
        q.res_index = 1;
        q.view_radius = 5;
        q.shadow_size = 512;
        q.shadow_radius = 1;
        q.lod_near = 0;
        q.lod_mid = 2;
        q.particles = 2000;
        q.glow_divisor = 4;
        q.lod_shift = 5;
        break;
    case WN_PRESET_STANDARD:
        q.res_index = 1;
        q.view_radius = 7;
        q.shadow_size = 1024;
        q.shadow_radius = 3;
        q.lod_near = 1;
        q.lod_mid = 3;
        q.particles = 3000;
        q.glow_divisor = 4;
        q.lod_shift = 4;
        break;
    default:
        q.preset = WN_PRESET_HIGH;
        q.res_index = 2;
        q.view_radius = 10;
        q.shadow_size = 2048;
        q.shadow_radius = 5;
        q.lod_near = 2;
        q.lod_mid = 5;
        q.particles = 4096;
        q.glow_divisor = 2;
        q.lod_shift = 3;
        break;
    }
    return q;
}

const char *wn_preset_name(wn_preset_id id)
{
    static const char *n[] = {"Mobile", "Standard", "High"};
    return (id >= 0 && id < WN_PRESET_COUNT) ? n[id] : "?";
}

wn_preset_id wn_preset_for_device(bool coarse, int sw, int sh)
{
    int short_side = WN_MIN(sw, sh);
    if (coarse && short_side <= 1200) return WN_PRESET_MOBILE;
    if (coarse) return WN_PRESET_STANDARD; /* tablets */
    return WN_PRESET_STANDARD;
}

bool wn_quality_lower(wn_quality *q)
{
    if (q->res_index > 0) {
        q->res_index--;
        return true;
    }
    if (q->view_radius > 3) {
        q->view_radius--;
        return true;
    }
    if (q->particles > 500) {
        q->particles /= 2;
        return true;
    }
    if (q->glow_divisor) {
        q->glow_divisor = 0;
        return true;
    }
    return false;
}

bool wn_quality_raise(wn_quality *q, const wn_quality *ceil)
{
    if (!q->glow_divisor && ceil->glow_divisor) {
        q->glow_divisor = ceil->glow_divisor;
        return true;
    }
    if (q->particles < ceil->particles) {
        q->particles = WN_MIN(ceil->particles, q->particles * 2);
        return true;
    }
    if (q->view_radius < ceil->view_radius) {
        q->view_radius++;
        return true;
    }
    if (q->res_index < ceil->res_index) {
        q->res_index++;
        return true;
    }
    return false;
}

void wn_autoscale_init(wn_autoscale *a, float budget_ms)
{
    memset(a, 0, sizeof *a);
    a->budget_ms = budget_ms;
    a->enabled = true;
}

int wn_autoscale_frame(wn_autoscale *a, wn_quality *q, float ms, float dt)
{
    if (!a->enabled) return 0;
    a->window_ms[a->window_i] = ms;
    a->window_i = (a->window_i + 1) % 30;
    if (a->window_n < 30) a->window_n++;
    float avg = 0.0f;
    for (int i = 0; i < a->window_n; i++) avg += a->window_ms[i];
    avg /= (float)a->window_n;
    if (a->hold_time > 0.0f) a->hold_time -= dt;
    if (avg > a->budget_ms) {
        a->over_time += dt;
        a->under_time = 0.0f;
    } else {
        a->over_time = 0.0f;
        if (avg < a->budget_ms * 0.6f) a->under_time += dt;
        else a->under_time = 0.0f;
    }
    if (a->over_time >= 2.0f) {
        a->over_time = 0.0f;
        if (wn_quality_lower(q)) {
            a->steps_down++;
            a->hold_time = 10.0f;
            a->window_n = 0;
            return -1;
        }
    }
    if (a->steps_down > 0 && a->hold_time <= 0.0f && a->under_time >= 10.0f) {
        wn_quality ceil = wn_quality_preset(q->preset);
        a->under_time = 0.0f;
        if (wn_quality_raise(q, &ceil)) {
            a->steps_down--;
            a->hold_time = 10.0f;
            return 1;
        }
    }
    return 0;
}

/* ---- Palette ---- */

bool wn_palette_load(wn_palette *p, const char *text, char *err, size_t errlen)
{
    wn_palette tmp;
    memset(&tmp, 0, sizeof tmp);
    wn_parser ps;
    wn_parser_init(&ps, text, strlen(text));
    while (wn_parser_next(&ps)) {
        for (int i = 0; i < ps.nfields; i++) {
            if (tmp.count >= WN_PALETTE_SIZE) {
                wn_parser_fail(&ps, "more than 64 palette colours");
                break;
            }
            if (!wn_parser_color(&ps, i, &tmp.colors[tmp.count])) break;
            tmp.count++;
        }
        if (ps.error[0]) break;
    }
    if (!ps.error[0] && tmp.count == 0) wn_parser_fail(&ps, "palette is empty");
    if (ps.error[0]) {
        if (err) snprintf(err, errlen, "%s", ps.error);
        return false;
    }
    *p = tmp;
    return true;
}

int wn_palette_nearest(const wn_palette *p, uint32_t rgb)
{
    int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    int best = 0;
    long best_d = -1;
    for (int i = 0; i < p->count; i++) {
        int pr = (p->colors[i] >> 16) & 255, pg = (p->colors[i] >> 8) & 255, pb = p->colors[i] & 255;
        /* Perceptual-ish weights (green matters most). */
        long d = 3L * (r - pr) * (r - pr) + 4L * (g - pg) * (g - pg) + 2L * (b - pb) * (b - pb);
        if (best_d < 0 || d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

void wn_palette_build_lut(const wn_palette *p, uint8_t *lut)
{
    for (int b = 0; b < WN_LUT_SIZE; b++)
        for (int g = 0; g < WN_LUT_SIZE; g++)
            for (int r = 0; r < WN_LUT_SIZE; r++) {
                uint32_t rr = (uint32_t)(r * 255 / (WN_LUT_SIZE - 1));
                uint32_t gg = (uint32_t)(g * 255 / (WN_LUT_SIZE - 1));
                uint32_t bb = (uint32_t)(b * 255 / (WN_LUT_SIZE - 1));
                uint32_t c = p->colors[wn_palette_nearest(p, (rr << 16) | (gg << 8) | bb)];
                uint8_t *o = &lut[((b * WN_LUT_SIZE + g) * WN_LUT_SIZE + r) * 4];
                o[0] = (uint8_t)(c >> 16);
                o[1] = (uint8_t)(c >> 8);
                o[2] = (uint8_t)c;
                o[3] = 255;
            }
}

float wn_bayer4(int x, int y)
{
    static const int m[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    return (float)m[(y & 3) * 4 + (x & 3)] / 16.0f;
}

/* ---- Atmosphere ---- */

void wn_atmos_default(wn_atmos *a)
{
    memset(a, 0, sizeof *a);
    a->moon_dir = wn_v3_norm(wn_v3(-0.5f, 0.45f, 0.3f));
    a->moon_color = 0x9FB4E0;
    a->moon_intensity = 0.9f;
    a->sky_top = 0x05070F;
    a->sky_horizon = 0x16204A;
    a->fog_color = 0x0B1026;
    a->fog_density = 0.035f;
    a->fire_color = a->fire_target = 0xFFB050;
    a->ambient = 0.10f;
}

uint32_t wn_rgb_lerp(uint32_t a, uint32_t b, float t)
{
    t = WN_CLAMP(t, 0.0f, 1.0f);
    uint32_t out = 0;
    for (int s = 0; s < 24; s += 8) {
        float ca = (float)((a >> s) & 255), cb = (float)((b >> s) & 255);
        uint32_t c = (uint32_t)(ca + (cb - ca) * t + 0.5f);
        out |= (c & 255) << s;
    }
    return out;
}

wn_vec3 wn_rgb_to_vec(uint32_t rgb)
{
    return wn_v3((float)((rgb >> 16) & 255) / 255.0f, (float)((rgb >> 8) & 255) / 255.0f, (float)(rgb & 255) / 255.0f);
}

void wn_atmos_fade_fire(wn_atmos *a, uint32_t target, float seconds)
{
    a->fire_target = target;
    a->fire_change_time = seconds;
    if (seconds <= 0.0f) a->fire_color = target;
}

void wn_atmos_step(wn_atmos *a, float dt)
{
    a->time += dt;
    if (a->fire_change_time > 0.0f) {
        float t = dt >= a->fire_change_time ? 1.0f : dt / a->fire_change_time;
        a->fire_color = wn_rgb_lerp(a->fire_color, a->fire_target, t);
        a->fire_change_time -= dt;
        if (a->fire_change_time <= 0.0f) a->fire_color = a->fire_target;
    }
}

/* ---- Camera ---- */

wn_vec3 wn_camera_forward(const wn_camera *c)
{
    float cp = wn_cosf(c->pitch);
    return wn_v3(wn_sinf(c->yaw) * cp, wn_sinf(c->pitch), wn_cosf(c->yaw) * cp);
}

wn_mat4 wn_camera_view(const wn_camera *c)
{
    return wn_mat4_look_at(c->pos, wn_v3_add(c->pos, wn_camera_forward(c)), wn_v3(0, 1, 0));
}

wn_mat4 wn_camera_proj(const wn_camera *c, float aspect)
{
    return wn_mat4_perspective(c->fov_y, aspect, c->znear, c->zfar);
}

/* ---- Stats ---- */

void wn_stats_reset(wn_frame_stats *s) { memset(s, 0, sizeof *s); }

void wn_stats_push(wn_frame_stats *s, float ms, uint32_t draws, uint32_t tris)
{
    if (s->n < WN_STATS_MAX) s->ms[s->n++] = ms;
    s->draw_calls = draws;
    s->triangles = tris;
    s->max_draw_calls = WN_MAX(s->max_draw_calls, draws);
    s->max_triangles = WN_MAX(s->max_triangles, tris);
}

static int cmp_float(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

float wn_stats_percentile(const wn_frame_stats *s, float p)
{
    if (s->n == 0) return 0.0f;
    float *tmp = (float *)malloc(s->n * sizeof(float));
    memcpy(tmp, s->ms, s->n * sizeof(float));
    qsort(tmp, s->n, sizeof(float), cmp_float);
    float idx = p / 100.0f * (float)(s->n - 1);
    uint32_t i = (uint32_t)idx;
    float r = tmp[i];
    if (i + 1 < s->n) r += (tmp[i + 1] - tmp[i]) * (idx - (float)i);
    free(tmp);
    return r;
}
