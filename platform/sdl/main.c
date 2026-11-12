/* Winternight platform - application entry point for desktop (Windows,
 * Linux) and the web (Emscripten, WebGL2, WebXR). Glue only: all game logic
 * lives in the core (design D1). */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "gl/wn_gl.h"
#include "gl/wn_renderer.h"
#include "wn/wn_core.h"
#include "wn/wn_game.h"
#include "wn/wn_gametest.h"
#include "wn/wn_overlay.h"
#include "wn/wn_render.h"
#include "wn/wn_view.h"
#include "wn/wn_xr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#ifndef WN_DATA_DIR
#define WN_DATA_DIR "game/data"
#endif

typedef struct app {
    SDL_Window *win;
    SDL_GLContext gl;
    wn_renderer *r;
    wn_game game;
    wn_view view;
    wn_overlay overlay;
    wn_input input;
    wn_touch touch;
    wn_quality quality;
    wn_autoscale autoscale;
    wn_clock clock;
    wn_frame_stats stats;
    wn_render_stats rstats;
    wn_palette palette;
    wn_far_grid far;
    bool running;
    bool show_stats;
    bool coarse;
    int win_w, win_h;
    uint64_t last_counter;
    uint64_t budget_deadline;
    float last_frame_ms;
    char data_dir[256];
    char scene[32];
    uint64_t seed;
    int preset;
    /* recording / replay */
    char record_path[256];
    wn_replay record;
    wn_replay play;
    bool playing;
    /* automated game test */
    bool test_mode;
    wn_gametest test;
    char golden_dir[256];
    char out_dir[256];
    bool update_golden;
    int exit_code;
    /* benchmark */
    bool bench;
    uint32_t bench_frames;
    /* XR */
    bool paused; /* tab hidden (spec platform-targets: Hidden tab pauses) */
    bool xr_active;
    wn_xr_turn xr_turn;
    uint32_t xr_frames, xr_views;
    float vignette;
    char status[512];
} app;

static app *g_app;

static void copy_str(char *dst, size_t n, const char *src)
{
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = '\0';
}

/* ---------------- helpers ---------------- */

static bool budget_ok(void *user)
{
    app *a = (app *)user;
    return SDL_GetPerformanceCounter() < a->budget_deadline;
}

static void set_budget_ms(app *a, double ms)
{
    a->budget_deadline = SDL_GetPerformanceCounter() + (uint64_t)(ms * (double)SDL_GetPerformanceFrequency() / 1000.0);
}

/* Streams and meshes everything around the player (start-up, test captures). */
static void settle_world(app *a)
{
    wn_game_stream_all(&a->game);
    do {
        wn_view_update(&a->view, &a->game.world, a->game.player.pos, a->game.view_radius, NULL, NULL);
    } while (a->view.pending_last > 0);
}

static void apply_quality(app *a)
{
    a->game.view_radius = a->quality.view_radius;
    a->game.fx.capacity = a->quality.particles;
    a->view.lod_near = a->quality.lod_near;
    a->view.lod_mid = a->quality.lod_mid;
}

static void draw_overlay(app *a)
{
    int iw = wn_internal_res[a->quality.res_index].w, ih = wn_internal_res[a->quality.res_index].h;
    if (a->overlay.w != iw || a->overlay.h != ih) {
        wn_overlay_free(&a->overlay);
        wn_overlay_init(&a->overlay, iw, ih);
    }
    wn_overlay_clear(&a->overlay);
    wn_overlay_touch(&a->overlay, &a->touch);
    if (a->game.message_time > 0.0f) {
        int w = (int)strlen(a->game.message) * 6;
        wn_overlay_text(&a->overlay, (iw - w) / 2, ih / 5, a->game.message, 0xFFE2A0);
    }
    if (a->show_stats) {
        char line[96];
        snprintf(line, sizeof line, "%.1f MS P95 %.1f DC %u TRI %uK %s", a->last_frame_ms,
                 wn_stats_percentile(&a->stats, 95.0f), a->rstats.draw_calls, a->rstats.triangles / 1000,
                 wn_preset_name(a->quality.preset));
        wn_overlay_text(&a->overlay, 4, 4, line, 0xC9D6F2);
    }
    wn_overlay_finish(&a->overlay);
}

static void render(app *a, float alpha, unsigned fbo, int x, int y, int w, int h, bool clear, const float *view,
                   const float *proj, bool xr)
{
    wn_camera cam = wn_game_camera(&a->game, alpha);
    wn_game_pose_warrior(&a->game, alpha);
    wn_frame_desc f;
    memset(&f, 0, sizeof f);
    float aspect = (float)wn_internal_res[a->quality.res_index].w / (float)wn_internal_res[a->quality.res_index].h;
    if (view && proj) {
        memcpy(f.view.m, view, sizeof f.view.m);
        memcpy(f.proj.m, proj, sizeof f.proj.m);
        wn_mat4 inv;
        wn_mat4_invert(f.view, &inv);
        f.eye = wn_v3(inv.m[12], inv.m[13], inv.m[14]);
    } else {
        f.view = wn_camera_view(&cam);
        f.proj = wn_camera_proj(&cam, aspect);
        f.eye = cam.pos;
    }
    f.atmos = &a->game.atmos;
    f.quality = &a->quality;
    f.view_cache = &a->view;
    f.world = &a->game.world;
    f.viewer = a->game.player.pos;
    f.radius = a->game.view_radius;
    if (!xr) f.models[f.nmodels++] = &a->game.warrior;
    f.debris = &a->game.debris;
    f.fx = &a->game.fx;
    f.overlay = xr ? NULL : &a->overlay;
    f.xr = xr;
    f.vignette = a->vignette;
    f.target_fbo = fbo;
    f.out_x = x;
    f.out_y = y;
    f.out_w = w;
    f.out_h = h;
    f.win_w = a->win_w;
    f.win_h = a->win_h;
    f.clear_target = clear;
    wn_renderer_draw(a->r, &f, &a->rstats);
}

/* ---------------- events ---------------- */

static void handle_event(app *a, const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_QUIT: a->running = false; break;
    case SDL_EVENT_KEY_DOWN:
        wn_touch_notice_device(&a->touch, WN_DEVICE_KEYBOARD_MOUSE);
        if (e->key.scancode == SDL_SCANCODE_F3 && !e->key.repeat) a->show_stats = !a->show_stats;
        if (e->key.scancode == SDL_SCANCODE_F11 && !e->key.repeat)
            SDL_SetWindowFullscreen(a->win, !(SDL_GetWindowFlags(a->win) & SDL_WINDOW_FULLSCREEN));
        if (e->key.scancode == SDL_SCANCODE_ESCAPE) SDL_SetWindowRelativeMouseMode(a->win, false);
        wn_input_set(&a->input, WN_SRC_KEY, (int32_t)e->key.scancode, 1.0f);
        break;
    case SDL_EVENT_KEY_UP: wn_input_set(&a->input, WN_SRC_KEY, (int32_t)e->key.scancode, 0.0f); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (!SDL_GetWindowRelativeMouseMode(a->win) && !a->touch.visible) SDL_SetWindowRelativeMouseMode(a->win, true);
        wn_input_set(&a->input, WN_SRC_MOUSE_BUTTON, e->button.button, 1.0f);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP: wn_input_set(&a->input, WN_SRC_MOUSE_BUTTON, e->button.button, 0.0f); break;
    case SDL_EVENT_MOUSE_MOTION:
        if (SDL_GetWindowRelativeMouseMode(a->win)) {
            wn_input_add(&a->input, WN_SRC_MOUSE_MOTION, 0, e->motion.xrel);
            wn_input_add(&a->input, WN_SRC_MOUSE_MOTION, 1, e->motion.yrel);
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED: SDL_OpenGamepad(e->gdevice.which); break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        wn_input_set(&a->input, WN_SRC_PAD_AXIS, e->gaxis.axis, (float)e->gaxis.value / 32767.0f);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        wn_touch_notice_device(&a->touch, WN_DEVICE_GAMEPAD);
        wn_input_set(&a->input, WN_SRC_PAD_BUTTON, e->gbutton.button, 1.0f);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP: wn_input_set(&a->input, WN_SRC_PAD_BUTTON, e->gbutton.button, 0.0f); break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP: {
        float aspect = a->win_h > 0 ? (float)a->win_w / (float)a->win_h : 1.0f;
        wn_touch_set_aspect(&a->touch, aspect);
        float x = e->tfinger.x * aspect, y = e->tfinger.y;
        int64_t id = (int64_t)e->tfinger.fingerID;
        if (e->type == SDL_EVENT_FINGER_DOWN) wn_touch_down(&a->touch, &a->input, id, x, y);
        else if (e->type == SDL_EVENT_FINGER_MOTION) wn_touch_move(&a->touch, &a->input, id, x, y);
        else wn_touch_up(&a->touch, &a->input, id);
        break;
    }
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        a->win_w = e->window.data1;
        a->win_h = e->window.data2;
        break;
    default: break;
    }
}

/* ---------------- game test mode ---------------- */

static void test_capture(app *a, const wn_gt_cmd *c)
{
    settle_world(a);
    draw_overlay(a);
    wn_viewport vp = wn_upscale(wn_internal_res[a->quality.res_index], a->win_w, a->win_h);
    render(a, 1.0f, 0, vp.x, vp.y, vp.w, vp.h, true, NULL, NULL, false);
    uint8_t *img;
    int w, h;
    if (!wn_renderer_capture(a->r, &img, &w, &h)) {
        wn_gametest_fail(&a->test, "capture '%s': no image", c->name);
        return;
    }
    char path[512];
    if (c->palette) {
        int bx = 0, by = 0;
        if (!wn_image_in_palette(img, w, h, a->palette.colors, a->palette.count, &bx, &by))
            wn_gametest_fail(&a->test, "capture '%s': pixel %d,%d is not a palette colour", c->name, bx, by);
    }
    if (!a->golden_dir[0]) { /* no golden images on this target (web): checks only */
        free(img);
        return;
    }
    snprintf(path, sizeof path, "%.255s/%.47s.png", a->out_dir, c->name);
    wn_png_write(path, img, w, h);
    snprintf(path, sizeof path, "%.255s/%.47s.png", a->golden_dir, c->name);
    if (a->update_golden) {
        wn_png_write(path, img, w, h);
        wn_log(WN_LOG_INFO, "golden image updated: %s", path);
    } else {
        int gw, gh;
        uint8_t *gold = wn_png_read(path, &gw, &gh);
        if (!gold) {
            wn_gametest_fail(&a->test, "capture '%s': golden image %s missing (run with --update-golden)", c->name, path);
        } else if (gw != w || gh != h) {
            wn_gametest_fail(&a->test, "capture '%s': size %dx%d differs from golden %dx%d", c->name, w, h, gw, gh);
        } else {
            uint8_t *diff = (uint8_t *)malloc((size_t)w * h * 4);
            wn_image_diff d = wn_image_compare(img, gold, w, h, c->tolerance, diff);
            if (d.fraction > c->maxdiff) {
                char dp[512];
                snprintf(dp, sizeof dp, "%.255s/%.47s.diff.png", a->out_dir, c->name);
                wn_png_write(dp, diff, w, h);
                wn_gametest_fail(&a->test, "capture '%s': %.2f%% pixels differ (max %.2f%%), diff saved to %s", c->name,
                                 (double)(d.fraction * 100.0f), (double)(c->maxdiff * 100.0f), dp);
            } else {
                wn_log(WN_LOG_INFO, "capture '%s' matches golden (%.3f%% differ)", c->name, (double)(d.fraction * 100.0f));
            }
            free(diff);
            free(gold);
        }
    }
    free(img);
}

static void test_frame(app *a)
{
    uint32_t step = (uint32_t)a->game.step;
    wn_gt_cmd caps[8];
    int n = wn_gametest_run(&a->test, &a->game, step, caps, 8);
    for (int i = 0; i < n; i++) test_capture(a, &caps[i]);
    if (step >= a->test.end_step) {
        if (a->test.failures) {
            fprintf(stderr, "GAME TEST FAILED (%d):\n%s", a->test.failures, a->test.log);
            a->exit_code = 1;
        } else {
            printf("GAME TEST PASSED (%u steps, checksum %08x)\n", step, wn_game_checksum(&a->game));
        }
        a->running = false;
        return;
    }
    wn_gametest_steer(&a->test, &a->game);
    wn_input_frame f = wn_gametest_input(&a->test, step);
    if (a->test.trace > 0 && step % (uint32_t)a->test.trace == 0) {
        uint32_t px, py, pz;
        memcpy(&px, &a->game.player.pos.x, 4);
        memcpy(&py, &a->game.player.pos.y, 4);
        memcpy(&pz, &a->game.player.pos.z, 4);
        printf("TRACE %u %08x pos %08x %08x %08x edits %u debris %08x fx %08x n %u g %u st %u\n", step,
               wn_game_checksum(&a->game), px, py, pz, a->game.world.edit_serial, wn_debris_checksum(&a->game.debris),
               wn_fx_checksum(&a->game.fx), a->game.fx.ncosmetic, a->game.fx.count, a->game.fx.stains_written);
        if (a->test.trace_particles)
            for (uint32_t i = 0; i < a->game.fx.count; i++) {
                const wn_particle *pp = &a->game.fx.p[i];
                uint32_t q[6];
                memcpy(q, &pp->pos, 12);
                memcpy(q + 3, &pp->vel, 12);
                printf("  P %u type %u %08x %08x %08x v %08x %08x %08x\n", i, pp->type, q[0], q[1], q[2], q[3], q[4], q[5]);
            }
    }
    wn_game_step(&a->game, &f);
    /* Deterministic streaming in tests: everything is loaded before each step. */
    wn_game_stream(&a->game, 1 << 20);
}

/* Tests render only every 30th step (and at captures) so long runs stay fast. */
static bool test_should_render(const app *a) { return a->game.step % 30 == 0; }

static void update_status(app *a)
{
    snprintf(a->status, sizeof a->status,
             "{\"step\":%llu,\"checksum\":%u,\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"touch\":%s,\"preset\":\"%s\","
             "\"xr\":%s,\"xr_frames\":%u,\"xr_views\":%u,\"complete\":%s,\"chunks\":%u,\"columns\":%u,\"ms\":%.2f}",
             (unsigned long long)a->game.step, wn_game_checksum(&a->game), (double)a->game.player.pos.x,
             (double)a->game.player.pos.y, (double)a->game.player.pos.z, a->touch.visible ? "true" : "false",
             wn_preset_name(a->quality.preset), a->xr_active ? "true" : "false", a->xr_frames, a->xr_views,
             a->game.complete ? "true" : "false", a->game.world.nchunks, a->view.ncols, (double)a->last_frame_ms);
}

/* ---------------- main frame ---------------- */

static void frame(void *user)
{
    app *a = (app *)user;
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(a, &e);
    if (a->xr_active) return; /* XR frames are driven by the XR session */
    if (a->paused) {
        a->last_counter = SDL_GetPerformanceCounter();
        return;
    }
    SDL_GetWindowSizeInPixels(a->win, &a->win_w, &a->win_h);
    uint64_t now = SDL_GetPerformanceCounter();
    double dt = (double)(now - a->last_counter) / (double)SDL_GetPerformanceFrequency();
    a->last_counter = now;

    if (a->test_mode) {
        test_frame(a);
        if (!a->running) return;
        if (!test_should_render(a)) return;
        wn_view_update(&a->view, &a->game.world, a->game.player.pos, a->game.view_radius, NULL, NULL);
    } else {
        int steps = a->bench ? 1 : wn_clock_advance(&a->clock, dt);
        for (int i = 0; i < steps; i++) {
            wn_input_frame f;
            if (a->playing) {
                if (!wn_replay_next(&a->play, &f, NULL)) {
                    a->playing = false;
                    memset(&f, 0, sizeof f);
                    printf("replay finished, checksum %08x\n", wn_game_checksum(&a->game));
                }
            } else {
                f = wn_input_poll(&a->input);
            }
            if (a->record_path[0]) wn_replay_push(&a->record, &f, wn_game_checksum(&a->game));
            wn_game_step(&a->game, &f);
        }
        /* Streaming and meshing within a per-frame budget (design D14). */
        int chunk_budget = a->quality.preset == WN_PRESET_MOBILE ? 3 : 8;
        wn_game_stream(&a->game, chunk_budget);
        set_budget_ms(a, a->quality.preset == WN_PRESET_MOBILE ? 3.0 : 5.0);
        wn_view_update(&a->view, &a->game.world, a->game.player.pos, a->game.view_radius, budget_ok, a);
    }
    draw_overlay(a);
    wn_viewport vp = wn_upscale(wn_internal_res[a->quality.res_index], a->win_w, a->win_h);
    render(a, a->test_mode || a->bench ? 1.0f : wn_clock_alpha(&a->clock), 0, vp.x, vp.y, vp.w, vp.h, true, NULL, NULL,
           false);
    SDL_GL_SwapWindow(a->win);
    float ms = (float)((double)(SDL_GetPerformanceCounter() - now) * 1000.0 / (double)SDL_GetPerformanceFrequency());
    a->last_frame_ms = (float)(dt * 1000.0);
    wn_stats_push(&a->stats, a->bench ? ms : (float)(dt * 1000.0), a->rstats.draw_calls, a->rstats.triangles);
    if (!a->test_mode && !a->bench && wn_autoscale_frame(&a->autoscale, &a->quality, (float)(dt * 1000.0), (float)dt))
        apply_quality(a);
    if (a->bench && ++a->bench_frames >= 900) {
        printf("BENCHMARK %s: frames %u p50 %.2f ms p95 %.2f ms p99 %.2f ms max draw calls %u max triangles %u\n",
               wn_preset_name(a->quality.preset), a->stats.n, (double)wn_stats_percentile(&a->stats, 50),
               (double)wn_stats_percentile(&a->stats, 95), (double)wn_stats_percentile(&a->stats, 99),
               a->stats.max_draw_calls, a->stats.max_triangles);
        a->running = false;
    }
    update_status(a);
#ifdef __EMSCRIPTEN__
    if (!a->running) emscripten_cancel_main_loop();
#endif
}

/* ---------------- web exports ---------------- */

#ifdef __EMSCRIPTEN__
EM_JS(int, wn_js_coarse_pointer, (void), {
    return (typeof window !== 'undefined' && window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0;
});
extern void wn_web_xr_init(void); /* platform/web/wn_web.js */

static bool on_visibility(int type, const EmscriptenVisibilityChangeEvent *e, void *user)
{
    (void)type;
    app *a = (app *)user;
    a->paused = e->hidden != 0;
    a->last_counter = SDL_GetPerformanceCounter(); /* no catch-up after the pause */
    return false;
}
EM_JS(void, wn_js_ready, (void), {
    if (typeof Module !== 'undefined' && Module.onWinternightReady) Module.onWinternightReady();
});
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* Status for browser tests (JSON). */
const char *wn_web_status(void) { return g_app ? g_app->status : "{}"; }

void wn_xr_session_started(void)
{
    if (!g_app) return;
    g_app->xr_active = true;
    g_app->game.cam.first_person = true;
    wn_xr_turn_init(&g_app->xr_turn, true);
}

void wn_xr_session_ended(void)
{
    if (!g_app) return;
    g_app->xr_active = false;
    g_app->game.cam.first_person = false;
    g_app->vignette = 0.0f;
    g_app->last_counter = SDL_GetPerformanceCounter();
    update_status(g_app);
}

/* Called by the WebXR bridge for every XR frame. views: count * 32 floats
 * (view matrix, projection matrix), viewports: count * 4 ints. inputs: 8
 * floats (move x, move y, turn x, trigger, grip, button a, swing speed, unused). */
void wn_xr_frame(const float *views, const int *viewports, int count, unsigned fbo, int fb_w, int fb_h,
                 const float *inputs, double dt)
{
    app *a = g_app;
    if (!a) return;
    a->xr_frames++;
    /* Controllers -> actions. */
    wn_input_frame f;
    memset(&f, 0, sizeof f);
    f.axis[WN_ACT_MOVE_X] = inputs[0];
    f.axis[WN_ACT_MOVE_Y] = -inputs[1];
    if (inputs[4] > 0.5f) f.down |= 1u << WN_ACT_BLOCK;
    if (inputs[5] > 0.5f) f.down |= 1u << WN_ACT_JUMP;
    int steps = wn_clock_advance(&a->clock, dt);
    for (int i = 0; i < steps; i++) {
        float turn = wn_xr_turn_step(&a->xr_turn, inputs[2], WN_SIM_DT);
        a->game.cam.yaw += turn;
        if (inputs[6] > 0.0f) wn_game_xr_swing(&a->game, inputs[6]);
        else if (inputs[3] > 0.5f && i == 0) f.pressed |= 1u << WN_ACT_ATTACK, f.down |= 1u << WN_ACT_ATTACK;
        wn_game_step(&a->game, &f);
        a->vignette = wn_xr_vignette(true, wn_sqrtf(inputs[0] * inputs[0] + inputs[1] * inputs[1]), turn / WN_SIM_DT);
    }
    wn_game_stream(&a->game, 4);
    set_budget_ms(a, 3.0);
    wn_view_update(&a->view, &a->game.world, a->game.player.pos, a->game.view_radius, budget_ok, a);
    for (int v = 0; v < count; v++) {
        const int *vp = &viewports[v * 4];
        /* XR view matrices are relative to the local reference space: place it at the player's feet. */
        wn_mat4 view, proj;
        memcpy(view.m, views + v * 32, sizeof view.m);
        memcpy(proj.m, views + v * 32 + 16, sizeof proj.m);
        wn_mat4 origin = wn_mat4_mul(wn_mat4_rotate_y(-a->game.cam.yaw + WN_PI), wn_mat4_translate(wn_v3_scale(a->game.player.pos, -1.0f)));
        wn_mat4 v2 = wn_mat4_mul(view, origin);
        render(a, 1.0f, fbo, vp[0], vp[1], vp[2], vp[3], v == 0, v2.m, proj.m, true);
        a->xr_views++;
    }
    update_status(a);
    (void)fb_w;
    (void)fb_h;
}
#ifdef __cplusplus
}
#endif

/* ---------------- start-up ---------------- */

static void usage(void)
{
    printf("winternight [--scene NAME] [--seed N] [--preset mobile|standard|high] [--data DIR]\n"
           "            [--record FILE] [--replay FILE] [--stats] [--benchmark]\n"
           "            [--test SCRIPT --golden DIR --out DIR [--update-golden]] [--window WxH]\n");
}

static bool load_text(const char *dir, const char *name, char **out)
{
    char path[512];
    snprintf(path, sizeof path, "%.255s/%.200s", dir, name);
    *out = wn_read_file(path, NULL);
    return *out != NULL;
}

static void fatal(app *a, const char *msg)
{
    fprintf(stderr, "winternight: %s\n", msg);
    /* Readable error for players; automated runs must never block on a dialog. */
    if (!a->test_mode && !a->bench && !SDL_getenv("WN_NO_DIALOGS"))
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Winternight", msg, a->win);
}

int main(int argc, char **argv)
{
    static app A;
    app *a = &A;
    g_app = a;
    memset(a, 0, sizeof *a);
    copy_str(a->data_dir, sizeof a->data_dir, WN_DATA_DIR);
    snprintf(a->scene, sizeof a->scene, "prologue");
    snprintf(a->golden_dir, sizeof a->golden_dir, "tests/game/golden");
    snprintf(a->out_dir, sizeof a->out_dir, ".");
    a->preset = -1;
    int win_w = 1280, win_h = 720;
    const char *test_script = NULL, *replay = NULL;
    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(s, "--scene") && v) copy_str(a->scene, sizeof a->scene, argv[++i]);
        else if (!strcmp(s, "--seed") && v) a->seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(s, "--data") && v) copy_str(a->data_dir, sizeof a->data_dir, argv[++i]);
        else if (!strcmp(s, "--preset") && v) {
            ++i;
            a->preset = !strcmp(v, "mobile") ? WN_PRESET_MOBILE : (!strcmp(v, "high") ? WN_PRESET_HIGH : WN_PRESET_STANDARD);
        } else if (!strcmp(s, "--record") && v) copy_str(a->record_path, sizeof a->record_path, argv[++i]);
        else if (!strcmp(s, "--replay") && v) replay = argv[++i];
        else if (!strcmp(s, "--test") && v) test_script = argv[++i];
        else if (!strcmp(s, "--golden") && v) copy_str(a->golden_dir, sizeof a->golden_dir, argv[++i]);
        else if (!strcmp(s, "--out") && v) copy_str(a->out_dir, sizeof a->out_dir, argv[++i]);
        else if (!strcmp(s, "--update-golden")) a->update_golden = true;
        else if (!strcmp(s, "--stats")) a->show_stats = true;
        else if (!strcmp(s, "--benchmark")) a->bench = true;
        else if (!strcmp(s, "--window") && v) sscanf(argv[++i], "%dx%d", &win_w, &win_h);
        else if (!strcmp(s, "--help")) {
            usage();
            return 0;
        }
    }

    char err[512];
    if (test_script) {
        char *txt = wn_read_file(test_script, NULL);
        if (!txt || !wn_gametest_load(&a->test, txt, err, sizeof err)) {
            fprintf(stderr, "cannot load test script %s: %s\n", test_script, txt ? err : "file not found");
            free(txt);
            return 2;
        }
        free(txt);
        a->test_mode = true;
        copy_str(a->scene, sizeof a->scene, a->test.scene);
        if (a->test.seed) a->seed = a->test.seed;
        if (a->test.preset >= 0) a->preset = a->test.preset;
    }

    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        fatal(a, SDL_GetError());
        return 1;
    }
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    a->coarse = wn_js_coarse_pointer() != 0;
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    a->win = SDL_CreateWindow("Winternight", win_w, win_h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!a->win) {
        snprintf(err, sizeof err, "Could not create a window with OpenGL 3.3 / WebGL2 support: %s", SDL_GetError());
        fatal(a, err);
        return 1;
    }
    a->gl = SDL_GL_CreateContext(a->win);
    if (!a->gl) {
        snprintf(err, sizeof err, "OpenGL 3.3 (or WebGL2) is required: %s", SDL_GetError());
        fatal(a, err);
        return 1;
    }
    SDL_GL_MakeCurrent(a->win, a->gl);
    SDL_GL_SetSwapInterval(a->test_mode || a->bench ? 0 : 1);
    const char *missing = wn_gl_load((wn_gl_getproc)SDL_GL_GetProcAddress);
    if (missing) {
        snprintf(err, sizeof err, "OpenGL function %s is missing (OpenGL 3.3 required)", missing);
        fatal(a, err);
        return 1;
    }
    wn_log(WN_LOG_INFO, "GL %s on %s", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));

    /* Data. */
    char *pal_txt = NULL;
    if (!load_text(a->data_dir, "palette.txt", &pal_txt) || !wn_palette_load(&a->palette, pal_txt, err, sizeof err)) {
        fatal(a, "cannot load palette.txt");
        return 1;
    }
    free(pal_txt);
    if (!wn_game_init_from_dir(&a->game, a->data_dir, a->scene, a->seed, err, sizeof err)) {
        fatal(a, err);
        return 1;
    }
    a->r = wn_renderer_create(&a->game.mats, &a->palette, err, sizeof err);
    if (!a->r) {
        fatal(a, err);
        return 1;
    }
    int sw = win_w, sh = win_h;
    SDL_GetWindowSizeInPixels(a->win, &sw, &sh);
    wn_preset_id pid = a->preset >= 0 ? (wn_preset_id)a->preset : wn_preset_for_device(a->coarse, sw, sh);
    a->quality = wn_quality_preset(pid);
    apply_quality(a);
    wn_autoscale_init(&a->autoscale, 16.7f);
    wn_clock_init(&a->clock);
    wn_input_init(&a->input);
    wn_touch_init(&a->touch, (float)sw / (float)WN_MAX(sh, 1), a->coarse);
    wn_view_init(&a->view);
    apply_quality(a);
    wn_overlay_init(&a->overlay, wn_internal_res[a->quality.res_index].w, wn_internal_res[a->quality.res_index].h);
    wn_stats_reset(&a->stats);
    wn_scene_build_far(&a->game.scene, a->quality.lod_shift, &a->far);
    wn_renderer_set_far(a->r, &a->far, &a->game.mats);
    if (replay) {
        size_t n;
        char *data = wn_read_file(replay, &n);
        if (!data || !wn_replay_deserialize(&a->play, (const uint8_t *)data, n, err, sizeof err)) {
            fprintf(stderr, "cannot load replay %s\n", replay);
            free(data);
            return 2;
        }
        free(data);
        a->playing = true;
    }
    if (a->record_path[0]) wn_replay_init(&a->record, a->game.scene.seed);
    settle_world(a);
    a->running = true;
    a->last_counter = SDL_GetPerformanceCounter();
    wn_log(WN_LOG_INFO, "scene '%s' ready: %u chunks, preset %s", a->scene, a->game.world.nchunks, wn_preset_name(pid));
#ifdef __EMSCRIPTEN__
    wn_web_xr_init();
    emscripten_set_visibilitychange_callback(a, false, on_visibility);
    wn_js_ready();
    emscripten_set_main_loop_arg(frame, a, 0, false);
    return 0;
#else
    while (a->running) frame(a);
    if (a->record_path[0]) {
        size_t n;
        uint8_t *buf = wn_replay_serialize(&a->record, &n);
        FILE *fp = fopen(a->record_path, "wb");
        if (fp) {
            fwrite(buf, 1, n, fp);
            fclose(fp);
            printf("recorded %u steps to %s (final checksum %08x)\n", a->record.count, a->record_path,
                   wn_game_checksum(&a->game));
        }
        free(buf);
    }
    if (a->playing || replay) printf("final checksum %08x\n", wn_game_checksum(&a->game));
    wn_renderer_destroy(a->r);
    wn_view_free(&a->view);
    wn_overlay_free(&a->overlay);
    wn_far_grid_free(&a->far);
    wn_game_free(&a->game);
    SDL_GL_DestroyContext(a->gl);
    SDL_DestroyWindow(a->win);
    SDL_Quit();
    return a->exit_code;
#endif
}
