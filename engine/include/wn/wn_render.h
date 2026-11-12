/* Winternight engine - platform-independent render logic: quality presets,
 * internal resolution and upscaling, automatic quality scaling, palette
 * quantisation tables, atmosphere parameters and frame statistics
 * (spec retro-rendering, spec platform-targets, design D4/D14). The OpenGL
 * backend in platform/gl turns this data into draw calls. */
#ifndef WN_RENDER_H
#define WN_RENDER_H

#include "wn_base.h"

/* ---- Internal resolutions ---- */

typedef struct wn_res {
    int w, h;
} wn_res;

#define WN_RES_COUNT 3
extern const wn_res wn_internal_res[WN_RES_COUNT]; /* 320x180, 480x270, 640x360 */

typedef struct wn_viewport {
    int x, y, w, h; /* destination rectangle in the window */
    int scale;      /* integer scale factor (0 = fractional fit, window too small) */
} wn_viewport;

/* Largest integer scale that fits; centred with letterboxing
 * (spec: Integer upscale on 1080p, Letterboxing on odd window size). */
wn_viewport wn_upscale(wn_res internal, int win_w, int win_h);

/* ---- Quality presets (spec platform-targets: Quality presets) ---- */

typedef enum wn_preset_id { WN_PRESET_MOBILE, WN_PRESET_STANDARD, WN_PRESET_HIGH, WN_PRESET_COUNT } wn_preset_id;

typedef struct wn_quality {
    wn_preset_id preset;
    int res_index;      /* into wn_internal_res */
    int view_radius;    /* chunk columns */
    int shadow_size;    /* shadow map edge */
    int shadow_radius;  /* chunk rings that cast moon shadows (design D14) */
    int lod_near;       /* chunk rings meshed at full detail */
    int lod_mid;        /* rings at half detail; beyond: quarter detail */
    uint32_t particles; /* particle cap */
    int glow_divisor;   /* glow buffer = internal / divisor, 0 = off */
    int lod_shift;      /* far scenery cell = 2^shift voxels */
    bool palette;       /* 64-colour quantisation + dithering */
    bool vertex_snap;   /* PS1-style jitter (never in XR) */
} wn_quality;

wn_quality wn_quality_preset(wn_preset_id id);
const char *wn_preset_name(wn_preset_id id);
/* Default preset for a device (phones start on Mobile). */
wn_preset_id wn_preset_for_device(bool coarse_pointer, int screen_w, int screen_h);

/* ---- Automatic quality scaling ---- */

typedef struct wn_autoscale {
    float budget_ms;     /* 16.7 */
    float over_time;     /* seconds continuously over budget */
    float hold_time;     /* seconds left before a step may be undone */
    float under_time;    /* seconds comfortably under budget */
    int steps_down;      /* how many steps were taken (for raising again) */
    float window_ms[30];
    int window_n, window_i;
    bool enabled;
} wn_autoscale;

void wn_autoscale_init(wn_autoscale *a, float budget_ms);
/* Feeds one frame time. Returns -1 if the quality was lowered by one step,
 * +1 if raised, 0 otherwise. q is modified in place. */
int wn_autoscale_frame(wn_autoscale *a, wn_quality *q, float frame_ms, float dt);
/* Lowers q by one step (resolution, then radius, then particles). Returns
 * false if nothing can be lowered any more. */
bool wn_quality_lower(wn_quality *q);
bool wn_quality_raise(wn_quality *q, const wn_quality *ceiling);

/* ---- Palette ---- */

#define WN_PALETTE_SIZE 64
#define WN_LUT_SIZE 32

typedef struct wn_palette {
    uint32_t colors[WN_PALETTE_SIZE];
    int count;
} wn_palette;

/* Loads "#rrggbb" entries (any layout, '#' comments allowed). */
bool wn_palette_load(wn_palette *p, const char *text, char *err, size_t errlen);
int wn_palette_nearest(const wn_palette *p, uint32_t rgb);
/* Builds an RGBA8 3D lookup table (WN_LUT_SIZE^3 * 4 bytes) mapping colours to
 * the nearest palette entry. */
void wn_palette_build_lut(const wn_palette *p, uint8_t *lut);
/* 4x4 ordered (Bayer) dither threshold in [0,1). */
float wn_bayer4(int x, int y);

/* ---- Atmosphere (spec night-lighting) ---- */

typedef struct wn_atmos {
    wn_vec3 moon_dir; /* towards the moon, normalised */
    uint32_t moon_color;
    float moon_intensity;
    uint32_t sky_top, sky_horizon;
    uint32_t fog_color;
    float fog_density;
    uint32_t fire_color;       /* colour of block light (warm / necromantic) */
    uint32_t fire_target;      /* colour the fire is changing towards */
    float fire_change_time;    /* seconds left for the change */
    float ambient;             /* minimum light so the night still reads */
    bool aurora;
    float time;                /* seconds since scene start (stars twinkle) */
} wn_atmos;

void wn_atmos_default(wn_atmos *a);
/* Starts a gradual light-colour change (spec: Fire turns necromantic). */
void wn_atmos_fade_fire(wn_atmos *a, uint32_t target, float seconds);
void wn_atmos_step(wn_atmos *a, float dt);
uint32_t wn_rgb_lerp(uint32_t a, uint32_t b, float t);
wn_vec3 wn_rgb_to_vec(uint32_t rgb);

/* ---- Camera ---- */

typedef struct wn_camera {
    wn_vec3 pos;
    float yaw, pitch;
    float fov_y; /* radians */
    float znear, zfar;
} wn_camera;

wn_vec3 wn_camera_forward(const wn_camera *c);
wn_mat4 wn_camera_view(const wn_camera *c);
wn_mat4 wn_camera_proj(const wn_camera *c, float aspect);

/* ---- Frame statistics (spec: Performance budgets) ---- */

#define WN_STATS_MAX 8192
typedef struct wn_frame_stats {
    float ms[WN_STATS_MAX];
    uint32_t n;
    uint32_t draw_calls, triangles; /* last frame */
    uint32_t max_draw_calls, max_triangles;
} wn_frame_stats;

void wn_stats_reset(wn_frame_stats *s);
void wn_stats_push(wn_frame_stats *s, float ms, uint32_t draws, uint32_t tris);
/* Percentile p in [0,100] of recorded frame times. */
float wn_stats_percentile(const wn_frame_stats *s, float p);

#endif
