#include "wn/wn_gametest.h"

#include "wn/wn_core.h"
#include "wn/wn_snow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_REDEFINE
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb_image.h"
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void wn_gametest_fail(wn_gametest *t, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    t->failures++;
    size_t used = strlen(t->log);
    if (used + strlen(buf) + 2 < sizeof t->log) {
        memcpy(t->log + used, buf, strlen(buf));
        t->log[used + strlen(buf)] = '\n';
        t->log[used + strlen(buf) + 1] = '\0';
    }
}

static bool parse_op(const char *s, wn_gt_op *op)
{
    static const char *names[] = {"<", "<=", ">", ">=", "==", "!="};
    for (int i = 0; i < 6; i++)
        if (strcmp(s, names[i]) == 0) {
            *op = (wn_gt_op)i;
            return true;
        }
    return false;
}

static int cmp_cmd(const void *a, const void *b)
{
    const wn_gt_cmd *x = (const wn_gt_cmd *)a, *y = (const wn_gt_cmd *)b;
    if (x->step != y->step) return x->step < y->step ? -1 : 1;
    return x->line - y->line; /* stable: file order */
}

bool wn_gametest_load(wn_gametest *t, const char *text, char *err, size_t errlen)
{
    memset(t, 0, sizeof *t);
    t->preset = -1;
    t->seed = 0;
    snprintf(t->scene, sizeof t->scene, "prologue");
    t->end_step = 600;
    wn_parser p;
    wn_parser_init(&p, text, strlen(text));
    while (wn_parser_next(&p)) {
        int32_t iv;
        if (wn_parser_is(&p, 0, "scene")) {
            if (!wn_parser_expect_fields(&p, 2)) break;
            snprintf(t->scene, sizeof t->scene, "%s", p.fields[1]);
        } else if (wn_parser_is(&p, 0, "seed")) {
            if (!wn_parser_int(&p, 1, &iv)) break;
            t->seed = (uint64_t)(uint32_t)iv;
        } else if (wn_parser_is(&p, 0, "preset")) {
            if (!wn_parser_expect_fields(&p, 2)) break;
            if (wn_parser_is(&p, 1, "mobile")) t->preset = WN_PRESET_MOBILE;
            else if (wn_parser_is(&p, 1, "standard")) t->preset = WN_PRESET_STANDARD;
            else if (wn_parser_is(&p, 1, "high")) t->preset = WN_PRESET_HIGH;
            else {
                wn_parser_fail(&p, "unknown preset");
                break;
            }
        } else if (wn_parser_is(&p, 0, "replay")) {
            if (!wn_parser_expect_fields(&p, 2)) break;
            snprintf(t->replay, sizeof t->replay, "%s", p.fields[1]);
        } else if (wn_parser_is(&p, 0, "end")) {
            if (!wn_parser_int(&p, 1, &iv)) break;
            t->end_step = (uint32_t)iv;
        } else if (wn_parser_is(&p, 0, "at")) {
            if (!wn_parser_expect_fields(&p, 3) || !wn_parser_int(&p, 1, &iv)) break;
            if (t->ncmds >= WN_GT_MAX_CMDS) {
                wn_parser_fail(&p, "too many commands");
                break;
            }
            wn_gt_cmd *c = &t->cmds[t->ncmds];
            memset(c, 0, sizeof *c);
            c->step = (uint32_t)iv;
            c->line = p.line_no;
            const char *k = p.fields[2];
            if (strcmp(k, "input") == 0 || strcmp(k, "press") == 0 || strcmp(k, "release") == 0) {
                wn_action a;
                if (!wn_parser_expect_fields(&p, 4) || !wn_action_from_name(p.fields[3], &a)) {
                    wn_parser_fail(&p, "unknown action");
                    break;
                }
                c->kind = WN_GT_INPUT;
                c->action = (int)a;
                c->v[0] = strcmp(k, "release") == 0 ? 0.0f : 1.0f;
                if (strcmp(k, "input") == 0 && !wn_parser_float(&p, 4, &c->v[0])) break;
            } else if (strcmp(k, "teleport") == 0) {
                c->kind = WN_GT_TELEPORT;
                if (!wn_parser_expect_fields(&p, 7)) break;
                if (!wn_parser_float(&p, 3, &c->v[0])) break;
                if (wn_parser_is(&p, 4, "ground")) c->v[1] = -1e9f;
                else if (!wn_parser_float(&p, 4, &c->v[1])) break;
                if (!wn_parser_float(&p, 5, &c->v[2]) || !wn_parser_float(&p, 6, &c->v[3])) break;
            } else if (strcmp(k, "camera") == 0) {
                c->kind = WN_GT_CAMERA;
                if (!wn_parser_expect_fields(&p, 8)) break;
                for (int i = 0; i < 5; i++)
                    if (!wn_parser_float(&p, 3 + i, &c->v[i])) break;
                if (p.error[0]) break;
            } else if (strcmp(k, "autopilot") == 0) {
                c->kind = WN_GT_AUTOPILOT;
                if (!wn_parser_expect_fields(&p, 4)) break;
                c->v[0] = wn_parser_is(&p, 3, "run") ? 2.0f : (wn_parser_is(&p, 3, "walk") ? 1.0f : 0.0f);
                if (p.nfields > 5 && wn_parser_is(&p, 4, "chop") && !wn_parser_float(&p, 5, &c->v[1])) break;
            } else if (strcmp(k, "follow") == 0) {
                c->kind = WN_GT_CAMERA;
                c->v[4] = 1e9f; /* back to the follow camera */
            } else if (strcmp(k, "look") == 0) {
                c->kind = WN_GT_LOOK;
                if (!wn_parser_expect_fields(&p, 5) || !wn_parser_float(&p, 3, &c->v[0]) || !wn_parser_float(&p, 4, &c->v[1]))
                    break;
            } else if (strcmp(k, "expect") == 0) {
                c->kind = WN_GT_EXPECT;
                if (!wn_parser_expect_fields(&p, 4)) break;
                snprintf(c->name, sizeof c->name, "%s", p.fields[3]);
                if (p.nfields == 4) {
                    c->op = WN_OP_NE; /* "expect complete" -> complete != 0 */
                    c->v[0] = 0.0f;
                } else {
                    if (!wn_parser_expect_fields(&p, 6) || !parse_op(p.fields[4], &c->op)) {
                        wn_parser_fail(&p, "expect needs: <var> <op> <value>");
                        break;
                    }
                    if (!wn_parser_float(&p, 5, &c->v[0])) break;
                }
            } else if (strcmp(k, "capture") == 0) {
                c->kind = WN_GT_CAPTURE;
                if (!wn_parser_expect_fields(&p, 4)) break;
                snprintf(c->name, sizeof c->name, "%s", p.fields[3]);
                c->tolerance = 8;
                c->maxdiff = 0.01f;
                for (int i = 4; i + 1 < p.nfields; i += 2) {
                    float v;
                    if (!wn_parser_float(&p, i + 1, &v)) break;
                    if (wn_parser_is(&p, i, "tolerance")) c->tolerance = (int)v;
                    else if (wn_parser_is(&p, i, "maxdiff")) c->maxdiff = v / 100.0f;
                    else wn_parser_fail(&p, "unknown capture option");
                }
                if (p.error[0]) break;
            } else if (strcmp(k, "blood") == 0) {
                c->kind = WN_GT_BLOOD;
                if (!wn_parser_expect_fields(&p, 7)) break;
                for (int i = 0; i < 4; i++)
                    if (!wn_parser_float(&p, 3 + i, &c->v[i])) break;
                if (p.error[0]) break;
            } else if (strcmp(k, "event") == 0) {
                c->kind = WN_GT_EVENT;
                if (!wn_parser_expect_fields(&p, 4)) break;
                snprintf(c->name, sizeof c->name, "%s", p.fields[3]);
            } else if (strcmp(k, "stream") == 0) {
                c->kind = WN_GT_STREAM;
            } else {
                wn_parser_fail(&p, "unknown test command");
                break;
            }
            t->ncmds++;
        } else {
            wn_parser_fail(&p, "unknown test keyword");
            break;
        }
    }
    if (p.error[0]) {
        if (err) snprintf(err, errlen, "%s", p.error);
        return false;
    }
    qsort(t->cmds, (size_t)t->ncmds, sizeof t->cmds[0], cmp_cmd);
    return true;
}

void wn_gametest_steer(wn_gametest *t, wn_game *g)
{
    if (!t->autopilot) return;
    float z = g->player.pos.z + 3.0f;
    float x = wn_scene_road_x(&g->scene, z);
    g->cam.yaw = wn_atan2f(x - g->player.pos.x, z - g->player.pos.z);
    t->held[WN_ACT_MOVE_Y] = 1.0f;
    t->held[WN_ACT_RUN] = t->autopilot == 2 ? 1.0f : 0.0f;
}

wn_input_frame wn_gametest_input(wn_gametest *t, uint32_t step)
{
    if (t->chop_period > 0) t->held[WN_ACT_ATTACK] = (step % (uint32_t)t->chop_period) < 2 ? 1.0f : 0.0f;
    /* Apply input commands due at this step first. */
    for (int i = 0; i < t->ncmds; i++)
        if (t->cmds[i].kind == WN_GT_INPUT && t->cmds[i].step == step) t->held[t->cmds[i].action] = t->cmds[i].v[0];
    wn_input_frame f;
    memset(&f, 0, sizeof f);
    for (int a = 0; a < WN_FIRST_BUTTON_ACTION; a++) f.axis[a] = t->held[a];
    for (int a = WN_FIRST_BUTTON_ACTION; a < WN_ACT_COUNT; a++)
        if (t->held[a] > 0.5f) f.down |= 1u << a;
    f.pressed = f.down & ~t->prev_down;
    t->prev_down = f.down;
    return f;
}

bool wn_gametest_var(wn_game *g, const char *name, float *out)
{
    if (strcmp(name, "player.x") == 0) *out = g->player.pos.x;
    else if (strcmp(name, "player.y") == 0) *out = g->player.pos.y;
    else if (strcmp(name, "player.z") == 0) *out = g->player.pos.z;
    else if (strcmp(name, "complete") == 0) *out = g->complete ? 1.0f : 0.0f;
    else if (strcmp(name, "events") == 0) *out = (float)g->events_fired;
    else if (strcmp(name, "debris") == 0) *out = (float)wn_debris_count(&g->debris);
    else if (strcmp(name, "stains") == 0) *out = (float)g->fx.stains_written;
    else if (strcmp(name, "camera.dist") == 0) *out = g->cam.actual_dist;
    else if (strcmp(name, "snow_depth") == 0) *out = wn_snow_depth_at(&g->world, wn_v3_add(g->player.pos, wn_v3(0, 0.3f, 0)));
    else if (strcmp(name, "step") == 0) *out = (float)g->step;
    else return false;
    return true;
}

static bool compare(wn_gt_op op, float a, float b)
{
    switch (op) {
    case WN_OP_LT: return a < b;
    case WN_OP_LE: return a <= b;
    case WN_OP_GT: return a > b;
    case WN_OP_GE: return a >= b;
    case WN_OP_EQ: return a == b;
    default: return a != b;
    }
}

int wn_gametest_run(wn_gametest *t, wn_game *g, uint32_t step, wn_gt_cmd *captures, int max)
{
    int ncap = 0;
    while (t->next < t->ncmds && t->cmds[t->next].step <= step) {
        wn_gt_cmd *c = &t->cmds[t->next++];
        switch (c->kind) {
        case WN_GT_TELEPORT: {
            float y = c->v[1];
            if (y < -1e8f) y = wn_scene_terrain_height(&g->scene, c->v[0], c->v[2]);
            wn_game_teleport(g, wn_v3(c->v[0], y, c->v[2]), c->v[3]);
            break;
        }
        case WN_GT_AUTOPILOT:
            t->autopilot = (int)c->v[0];
            t->chop_period = (int)c->v[1];
            if (!t->autopilot) {
                t->held[WN_ACT_MOVE_Y] = 0.0f;
                t->held[WN_ACT_RUN] = 0.0f;
                t->held[WN_ACT_ATTACK] = 0.0f;
            }
            break;
        case WN_GT_CAMERA:
            if (c->v[4] > 1e8f) {
                g->cam.fixed = false;
            } else {
                g->cam.fixed = true;
                g->cam.fixed_pos = wn_v3(c->v[0], c->v[1], c->v[2]);
                g->cam.yaw = c->v[3];
                g->cam.pitch = c->v[4];
            }
            break;
        case WN_GT_LOOK:
            g->cam.yaw = c->v[0];
            g->cam.pitch = c->v[1];
            break;
        case WN_GT_EXPECT: {
            float v;
            if (!wn_gametest_var(g, c->name, &v)) {
                wn_gametest_fail(t, "line %d: unknown variable '%s'", c->line, c->name);
            } else if (!compare(c->op, v, c->v[0])) {
                wn_gametest_fail(t, "line %d (step %u): expected %s compared with %g, got %g", c->line, step, c->name,
                                 (double)c->v[0], (double)v);
            }
            break;
        }
        case WN_GT_CAPTURE:
            if (ncap < max) captures[ncap++] = *c;
            break;
        case WN_GT_BLOOD: wn_game_spill_blood(g, wn_v3(c->v[0], c->v[1], c->v[2]), c->v[3]); break;
        case WN_GT_EVENT: wn_game_fire_event(g, c->name); break;
        case WN_GT_STREAM: wn_game_stream_all(g); break;
        default: break; /* input commands are handled by wn_gametest_input */
        }
    }
    return ncap;
}

/* ---- Images ---- */

wn_image_diff wn_image_compare(const uint8_t *a, const uint8_t *b, int w, int h, int tol, uint8_t *diff)
{
    wn_image_diff r;
    memset(&r, 0, sizeof r);
    for (int i = 0; i < w * h; i++) {
        int d = 0;
        for (int c = 0; c < 3; c++) {
            int x = abs((int)a[i * 4 + c] - (int)b[i * 4 + c]);
            if (x > d) d = x;
        }
        if (d > r.max_delta) r.max_delta = d;
        bool bad = d > tol;
        if (bad) r.differing++;
        if (diff) {
            diff[i * 4 + 0] = bad ? 255 : (uint8_t)(a[i * 4] / 4);
            diff[i * 4 + 1] = bad ? 0 : (uint8_t)(a[i * 4 + 1] / 4);
            diff[i * 4 + 2] = bad ? 0 : (uint8_t)(a[i * 4 + 2] / 4);
            diff[i * 4 + 3] = 255;
        }
    }
    r.fraction = (w > 0 && h > 0) ? (float)r.differing / (float)(w * h) : 0.0f;
    return r;
}

bool wn_png_write(const char *path, const uint8_t *rgba, int w, int h)
{
    return stbi_write_png(path, w, h, 4, rgba, w * 4) != 0;
}

uint8_t *wn_png_read(const char *path, int *w, int *h)
{
    int n;
    return stbi_load(path, w, h, &n, 4);
}
