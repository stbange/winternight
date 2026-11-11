#include "wn/wn_scene.h"

#include "wn/wn_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================= Prefabs ======================= */

static bool parse_prim(wn_parser *p, int f, wn_prim *pr)
{
    static const struct {
        const char *name;
        wn_prim_kind kind;
        int nargs;
    } kinds[] = {{"box", WN_PRIM_BOX, 6},       {"cyl", WN_PRIM_CYL, 5},       {"hcylx", WN_PRIM_HCYL_X, 5},
                 {"hcylz", WN_PRIM_HCYL_Z, 5},  {"cone", WN_PRIM_CONE, 5},     {"sphere", WN_PRIM_SPHERE, 4},
                 {"gablex", WN_PRIM_GABLE_X, 6}, {"gablez", WN_PRIM_GABLE_Z, 6}, {"crenel", WN_PRIM_CRENEL, 7}};
    int k = -1;
    for (size_t i = 0; i < WN_ARRAY_LEN(kinds); i++)
        if (wn_parser_is(p, f, kinds[i].name)) k = (int)i;
    if (k < 0) {
        wn_parser_fail(p, "unknown primitive");
        return false;
    }
    pr->kind = kinds[k].kind;
    int first = f + (pr->carve ? 1 : 2);
    if (!wn_parser_expect_fields(p, first + kinds[k].nargs)) return false;
    for (int i = 0; i < kinds[k].nargs; i++)
        if (!wn_parser_float(p, first + i, &pr->a[i])) return false;
    const float *a = pr->a;
    switch (pr->kind) {
    case WN_PRIM_BOX:
    case WN_PRIM_GABLE_X:
    case WN_PRIM_GABLE_Z:
    case WN_PRIM_CRENEL:
        pr->mn = wn_v3(WN_MIN(a[0], a[3]), WN_MIN(a[1], a[4]), WN_MIN(a[2], a[5]));
        pr->mx = wn_v3(WN_MAX(a[0], a[3]), WN_MAX(a[1], a[4]), WN_MAX(a[2], a[5]));
        break;
    case WN_PRIM_CYL:
    case WN_PRIM_CONE: /* cx cz y0 y1 r */
        pr->mn = wn_v3(a[0] - a[4], WN_MIN(a[2], a[3]), a[1] - a[4]);
        pr->mx = wn_v3(a[0] + a[4], WN_MAX(a[2], a[3]), a[1] + a[4]);
        break;
    case WN_PRIM_HCYL_X: /* cy cz x0 x1 r */
        pr->mn = wn_v3(WN_MIN(a[2], a[3]), a[0] - a[4], a[1] - a[4]);
        pr->mx = wn_v3(WN_MAX(a[2], a[3]), a[0] + a[4], a[1] + a[4]);
        break;
    case WN_PRIM_HCYL_Z: /* cx cy z0 z1 r */
        pr->mn = wn_v3(a[0] - a[4], a[1] - a[4], WN_MIN(a[2], a[3]));
        pr->mx = wn_v3(a[0] + a[4], a[1] + a[4], WN_MAX(a[2], a[3]));
        break;
    default: /* sphere: cx cy cz r */
        pr->mn = wn_v3(a[0] - a[3], a[1] - a[3], a[2] - a[3]);
        pr->mx = wn_v3(a[0] + a[3], a[1] + a[3], a[2] + a[3]);
        break;
    }
    return true;
}

bool wn_prefabs_load(wn_prefab_lib *out, const wn_materials *mats, const char *text, char *err, size_t errlen)
{
    wn_prefab_lib *lib = (wn_prefab_lib *)calloc(1, sizeof *lib);
    wn_parser p;
    wn_parser_init(&p, text, strlen(text));
    wn_prefab *cur = NULL;
    while (wn_parser_next(&p)) {
        if (wn_parser_is(&p, 0, "prefab")) {
            if (cur) {
                wn_parser_fail(&p, "missing 'end' before new prefab");
                break;
            }
            if (!wn_parser_expect_fields(&p, 2)) break;
            if (lib->count >= WN_MAX_PREFABS) {
                wn_parser_fail(&p, "too many prefabs");
                break;
            }
            cur = &lib->p[lib->count++];
            snprintf(cur->name, sizeof cur->name, "%s", p.fields[1]);
            cur->snow_cap = true;
            cur->mn = wn_v3(1e9f, 1e9f, 1e9f);
            cur->mx = wn_v3(-1e9f, -1e9f, -1e9f);
            continue;
        }
        if (!cur) {
            wn_parser_fail(&p, "primitive outside a prefab");
            break;
        }
        if (wn_parser_is(&p, 0, "end")) {
            if (cur->nprims == 0) {
                wn_parser_fail(&p, "empty prefab");
                break;
            }
            cur = NULL;
            continue;
        }
        if (wn_parser_is(&p, 0, "nosnow")) {
            cur->snow_cap = false;
            continue;
        }
        if (cur->nprims >= WN_MAX_PRIMS) {
            wn_parser_fail(&p, "too many primitives");
            break;
        }
        wn_prim pr;
        memset(&pr, 0, sizeof pr);
        int f = 0;
        if (wn_parser_is(&p, 0, "carve")) {
            pr.carve = true;
            f = 1;
        }
        if (!pr.carve) {
            if (!wn_parser_expect_fields(&p, 2)) break;
            int m = wn_materials_find(mats, p.fields[1]);
            if (m <= 0) {
                char msg[96];
                snprintf(msg, sizeof msg, "unknown material '%.40s'", p.fields[1]);
                wn_parser_fail(&p, msg);
                break;
            }
            pr.mat = (uint8_t)m;
        }
        if (!parse_prim(&p, f, &pr)) break;
        cur->prims[cur->nprims++] = pr;
        if (!pr.carve) {
            cur->mn = wn_v3(WN_MIN(cur->mn.x, pr.mn.x), WN_MIN(cur->mn.y, pr.mn.y), WN_MIN(cur->mn.z, pr.mn.z));
            cur->mx = wn_v3(WN_MAX(cur->mx.x, pr.mx.x), WN_MAX(cur->mx.y, pr.mx.y), WN_MAX(cur->mx.z, pr.mx.z));
        }
    }
    if (!p.error[0] && cur) snprintf(p.error, sizeof p.error, "prefab '%s' is missing 'end'", cur->name);
    if (p.error[0]) {
        if (err) snprintf(err, errlen, "%s", p.error);
        free(lib);
        return false;
    }
    *out = *lib;
    free(lib);
    return true;
}

const wn_prefab *wn_prefab_find(const wn_prefab_lib *lib, const char *name)
{
    for (int i = 0; i < lib->count; i++)
        if (strcmp(lib->p[i].name, name) == 0) return &lib->p[i];
    return NULL;
}

static bool prim_contains(const wn_prim *pr, wn_vec3 q)
{
    if (q.x < pr->mn.x || q.y < pr->mn.y || q.z < pr->mn.z || q.x >= pr->mx.x || q.y >= pr->mx.y || q.z >= pr->mx.z)
        return false;
    const float *a = pr->a;
    switch (pr->kind) {
    case WN_PRIM_BOX: return true;
    case WN_PRIM_CYL: {
        float dx = q.x - a[0], dz = q.z - a[1];
        return dx * dx + dz * dz < a[4] * a[4];
    }
    case WN_PRIM_CONE: {
        float t = (q.y - a[2]) / (a[3] - a[2]);
        float r = a[4] * (1.0f - t);
        float dx = q.x - a[0], dz = q.z - a[1];
        return dx * dx + dz * dz < r * r;
    }
    case WN_PRIM_HCYL_X: {
        float dy = q.y - a[0], dz = q.z - a[1];
        return dy * dy + dz * dz < a[4] * a[4];
    }
    case WN_PRIM_HCYL_Z: {
        float dx = q.x - a[0], dy = q.y - a[1];
        return dx * dx + dy * dy < a[4] * a[4];
    }
    case WN_PRIM_SPHERE: {
        wn_vec3 d = wn_v3(q.x - a[0], q.y - a[1], q.z - a[2]);
        return wn_v3_dot(d, d) < a[3] * a[3];
    }
    case WN_PRIM_GABLE_X:
    case WN_PRIM_GABLE_Z: {
        bool alongx = pr->kind == WN_PRIM_GABLE_X;
        float c = alongx ? (pr->mn.z + pr->mx.z) * 0.5f : (pr->mn.x + pr->mx.x) * 0.5f;
        float half = alongx ? (pr->mx.z - pr->mn.z) * 0.5f : (pr->mx.x - pr->mn.x) * 0.5f;
        float off = wn_absf((alongx ? q.z : q.x) - c) / half;
        return q.y < pr->mn.y + (pr->mx.y - pr->mn.y) * (1.0f - off);
    }
    default: { /* crenel: merlons of width a[6] */
        float period = a[6] > 0.0f ? a[6] : 1.0f;
        int k = wn_floorf_i((q.x + q.z) / period);
        return (k & 1) == 0;
    }
    }
}

int wn_prefab_sample(const wn_prefab *p, wn_vec3 q)
{
    int result = -1;
    for (int i = 0; i < p->nprims; i++) {
        const wn_prim *pr = &p->prims[i];
        if (!prim_contains(pr, q)) continue;
        result = pr->carve ? 0 : pr->mat; /* later primitives win */
    }
    return result;
}

/* ======================= Scene file ======================= */

static bool add_action(wn_scene *s, wn_parser *p)
{
    if (!wn_parser_expect_fields(p, 3)) return false;
    if (s->nactions >= WN_MAX_EVENTS) {
        wn_parser_fail(p, "too many event actions");
        return false;
    }
    wn_event_action *a = &s->actions[s->nactions];
    memset(a, 0, sizeof *a);
    snprintf(a->event, sizeof a->event, "%s", p->fields[1]);
    if (wn_parser_is(p, 2, "complete")) {
        a->kind = WN_EVT_COMPLETE;
    } else if (wn_parser_is(p, 2, "fire")) {
        a->kind = WN_EVT_FIRE_COLOR;
        if (!wn_parser_expect_fields(p, 5) || !wn_parser_color(p, 3, &a->color) || !wn_parser_float(p, 4, &a->value))
            return false;
    } else if (wn_parser_is(p, 2, "snowfall")) {
        a->kind = WN_EVT_SNOWFALL;
        if (!wn_parser_expect_fields(p, 4) || !wn_parser_float(p, 3, &a->value)) return false;
    } else if (wn_parser_is(p, 2, "message")) {
        a->kind = WN_EVT_MESSAGE;
        if (!wn_parser_expect_fields(p, 4)) return false;
        snprintf(a->text, sizeof a->text, "%s", p->fields[3]);
    } else {
        wn_parser_fail(p, "unknown event action");
        return false;
    }
    s->nactions++;
    return true;
}

static bool floats(wn_parser *p, int first, int n, float *out)
{
    if (!wn_parser_expect_fields(p, first + n)) return false;
    for (int i = 0; i < n; i++)
        if (!wn_parser_float(p, first + i, &out[i])) return false;
    return true;
}

static void place_bounds(wn_placement *pl)
{
    wn_vec3 a = pl->prefab->mn, b = pl->prefab->mx;
    /* Rotate the local bounds by quarter turns. */
    wn_vec3 c[2] = {a, b};
    wn_vec3 mn = wn_v3(1e9f, 1e9f, 1e9f), mx = wn_v3(-1e9f, -1e9f, -1e9f);
    for (int i = 0; i < 4; i++) {
        float x = c[i & 1].x, z = c[i >> 1].z, rx = x, rz = z;
        for (int r = 0; r < pl->rot; r++) {
            float t = rx;
            rx = rz;
            rz = -t;
        }
        mn.x = WN_MIN(mn.x, rx);
        mx.x = WN_MAX(mx.x, rx);
        mn.z = WN_MIN(mn.z, rz);
        mx.z = WN_MAX(mx.z, rz);
    }
    pl->mn = wn_v3(pl->pos.x + mn.x, pl->pos.y + a.y, pl->pos.z + mn.z);
    pl->mx = wn_v3(pl->pos.x + mx.x, pl->pos.y + b.y, pl->pos.z + mx.z);
}

bool wn_scene_load(wn_scene *out, const wn_materials *mats, const wn_prefab_lib *lib, const char *text, char *err,
                   size_t errlen)
{
    wn_scene *s = (wn_scene *)calloc(1, sizeof *s);
    s->mats = mats;
    s->seed = 1;
    s->chunk_y_min = -1;
    s->chunk_y_max = 4;
    s->bounds_mn = wn_v3(-50, -5, -50);
    s->bounds_mx = wn_v3(50, 40, 50);
    s->terrain_scale = 20.0f;
    s->forest_cell = 3.0f;
    s->road_z0 = s->road_z1 = 0.0f;
    wn_atmos_default(&s->atmos);
    /* Placements need the terrain, which needs the terrain lines first:
     * remember placement lines and resolve them at the end. */
    struct {
        char prefab[32];
        float x, z, yoff;
        bool on_road;
        int rot, line;
    } pend[WN_MAX_PLACEMENTS];
    int npend = 0;
    wn_parser p;
    wn_parser_init(&p, text, strlen(text));
    bool have_scene = false;
    while (wn_parser_next(&p)) {
        float v[8];
        if (wn_parser_is(&p, 0, "scene")) {
            if (!wn_parser_expect_fields(&p, 2)) break;
            snprintf(s->name, sizeof s->name, "%s", p.fields[1]);
            have_scene = true;
        } else if (wn_parser_is(&p, 0, "seed")) {
            int32_t sd;
            if (!wn_parser_int(&p, 1, &sd)) break;
            s->seed = (uint64_t)(uint32_t)sd;
        } else if (wn_parser_is(&p, 0, "bounds")) {
            if (!floats(&p, 1, 6, v)) break;
            s->bounds_mn = wn_v3(v[0], v[1], v[2]);
            s->bounds_mx = wn_v3(v[3], v[4], v[5]);
        } else if (wn_parser_is(&p, 0, "chunks_y")) {
            int32_t a, b;
            if (!wn_parser_int(&p, 1, &a) || !wn_parser_int(&p, 2, &b)) break;
            s->chunk_y_min = a;
            s->chunk_y_max = b;
        } else if (wn_parser_is(&p, 0, "terrain")) {
            if (!floats(&p, 1, 3, v)) break;
            s->terrain_base = v[0];
            s->terrain_amp = v[1];
            s->terrain_scale = v[2];
        } else if (wn_parser_is(&p, 0, "snow")) {
            if (!floats(&p, 1, 2, v)) break;
            s->snow_depth = v[0];
            s->snow_var = v[1];
        } else if (wn_parser_is(&p, 0, "hill")) {
            if (!floats(&p, 1, 4, v)) break;
            if (s->nhills >= WN_MAX_HILLS) {
                wn_parser_fail(&p, "too many hills");
                break;
            }
            s->hills[s->nhills].x = v[0];
            s->hills[s->nhills].z = v[1];
            s->hills[s->nhills].radius = v[2];
            s->hills[s->nhills].height = v[3];
            s->nhills++;
        } else if (wn_parser_is(&p, 0, "road")) {
            if (!floats(&p, 1, 5, v)) break;
            s->road_width = v[0];
            s->road_amp = v[1];
            s->road_freq = v[2];
            s->road_z0 = v[3];
            s->road_z1 = v[4];
        } else if (wn_parser_is(&p, 0, "forest")) {
            if (!floats(&p, 1, 3, v)) break;
            s->forest_cell = v[0];
            s->forest_prob = v[1];
            s->forest_clear = v[2];
        } else if (wn_parser_is(&p, 0, "tracks")) {
            if (!floats(&p, 1, 4, v)) break;
            if (s->ntracks >= WN_MAX_TRACKS) {
                wn_parser_fail(&p, "too many tracks");
                break;
            }
            s->tracks[s->ntracks].x0 = v[0];
            s->tracks[s->ntracks].z0 = v[1];
            s->tracks[s->ntracks].x1 = v[2];
            s->tracks[s->ntracks].z1 = v[3];
            s->ntracks++;
        } else if (wn_parser_is(&p, 0, "place")) {
            if (!wn_parser_expect_fields(&p, 4)) break;
            if (npend >= WN_MAX_PLACEMENTS) {
                wn_parser_fail(&p, "too many placements");
                break;
            }
            snprintf(pend[npend].prefab, sizeof pend[npend].prefab, "%s", p.fields[1]);
            pend[npend].on_road = wn_parser_is(&p, 2, "road");
            pend[npend].x = 0.0f;
            if (!pend[npend].on_road && !wn_parser_float(&p, 2, &pend[npend].x)) break;
            if (!wn_parser_float(&p, 3, &pend[npend].z)) break;
            int32_t rot = 0;
            float yoff = 0.0f;
            if (p.nfields > 4 && !wn_parser_int(&p, 4, &rot)) break;
            if (p.nfields > 5 && !wn_parser_float(&p, 5, &yoff)) break;
            pend[npend].rot = ((rot % 4) + 4) % 4;
            pend[npend].yoff = yoff;
            pend[npend].line = p.line_no;
            npend++;
        } else if (wn_parser_is(&p, 0, "moon")) {
            if (!floats(&p, 1, 3, v) || !wn_parser_color(&p, 4, &s->atmos.moon_color) ||
                !wn_parser_float(&p, 5, &s->atmos.moon_intensity))
                break;
            s->atmos.moon_dir = wn_v3_norm(wn_v3(v[0], v[1], v[2]));
        } else if (wn_parser_is(&p, 0, "sky")) {
            if (!wn_parser_color(&p, 1, &s->atmos.sky_top) || !wn_parser_color(&p, 2, &s->atmos.sky_horizon)) break;
        } else if (wn_parser_is(&p, 0, "fog")) {
            if (!wn_parser_color(&p, 1, &s->atmos.fog_color) || !wn_parser_float(&p, 2, &s->atmos.fog_density)) break;
        } else if (wn_parser_is(&p, 0, "fire")) {
            if (!wn_parser_color(&p, 1, &s->atmos.fire_color)) break;
            s->atmos.fire_target = s->atmos.fire_color;
        } else if (wn_parser_is(&p, 0, "ambient")) {
            if (!wn_parser_float(&p, 1, &s->atmos.ambient)) break;
        } else if (wn_parser_is(&p, 0, "aurora")) {
            s->atmos.aurora = true;
        } else if (wn_parser_is(&p, 0, "snowfall")) {
            if (!floats(&p, 1, 2, v)) break;
            s->snow_rate = v[0];
            s->snowfall_flakes = v[1];
        } else if (wn_parser_is(&p, 0, "spawn")) {
            if (!floats(&p, 1, 3, v)) break;
            s->spawn = wn_v3(v[0], 0.0f, v[1]);
            s->spawn_yaw = v[2];
        } else if (wn_parser_is(&p, 0, "trigger")) {
            if (!wn_parser_expect_fields(&p, 9) || !floats(&p, 2, 6, v)) break;
            if (s->ntriggers >= WN_MAX_TRIGGERS) {
                wn_parser_fail(&p, "too many triggers");
                break;
            }
            wn_trigger *t = &s->triggers[s->ntriggers++];
            snprintf(t->name, sizeof t->name, "%s", p.fields[1]);
            t->mn = wn_v3(WN_MIN(v[0], v[3]), WN_MIN(v[1], v[4]), WN_MIN(v[2], v[5]));
            t->mx = wn_v3(WN_MAX(v[0], v[3]), WN_MAX(v[1], v[4]), WN_MAX(v[2], v[5]));
            snprintf(t->event, sizeof t->event, "%s", p.fields[8]);
        } else if (wn_parser_is(&p, 0, "event")) {
            if (!add_action(s, &p)) break;
        } else {
            char msg[96];
            snprintf(msg, sizeof msg, "unknown scene keyword '%.40s'", p.fields[0]);
            wn_parser_fail(&p, msg);
            break;
        }
    }
    if (!p.error[0] && !have_scene) snprintf(p.error, sizeof p.error, "scene file has no 'scene' line");
    for (int i = 0; i < npend && !p.error[0]; i++) {
        const wn_prefab *pf = wn_prefab_find(lib, pend[i].prefab);
        if (!pf) {
            snprintf(p.error, sizeof p.error, "line %d: prefab '%.31s' does not exist", pend[i].line, pend[i].prefab);
            break;
        }
        wn_placement *pl = &s->place[s->nplace++];
        pl->prefab = pf;
        float x = pend[i].on_road ? wn_scene_road_x(s, pend[i].z) : pend[i].x;
        pl->pos = wn_v3(x, wn_scene_terrain_height(s, x, pend[i].z) + pend[i].yoff, pend[i].z);
        pl->rot = pend[i].rot;
        place_bounds(pl);
    }
    if (!p.error[0]) {
        float sx = s->spawn.x;
        if (sx == 0.0f && s->road_width > 0.0f) sx = wn_scene_road_x(s, s->spawn.z);
        s->spawn = wn_v3(sx, wn_scene_terrain_height(s, sx, s->spawn.z), s->spawn.z);
    }
    if (p.error[0]) {
        if (err) snprintf(err, errlen, "%s", p.error);
        free(s);
        return false;
    }
    *out = *s;
    free(s);
    return true;
}

/* ======================= Generation ======================= */

static float hash01(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    return (float)(wn_hash_ivec3(x, y, z, seed) >> 8) * (1.0f / 16777216.0f);
}

static float value_noise(float x, float z, uint32_t seed)
{
    int32_t ix = wn_floorf_i(x), iz = wn_floorf_i(z);
    float fx = x - (float)ix, fz = z - (float)iz;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fz = fz * fz * (3.0f - 2.0f * fz);
    float a = hash01(ix, 0, iz, seed), b = hash01(ix + 1, 0, iz, seed);
    float c = hash01(ix, 0, iz + 1, seed), d = hash01(ix + 1, 0, iz + 1, seed);
    return wn_lerpf(wn_lerpf(a, b, fx), wn_lerpf(c, d, fx), fz);
}

static float fbm(float x, float z, uint32_t seed)
{
    return value_noise(x, z, seed) * 0.6f + value_noise(x * 2.1f, z * 2.1f, seed + 1) * 0.28f +
           value_noise(x * 4.3f, z * 4.3f, seed + 2) * 0.12f;
}

float wn_scene_road_x(const wn_scene *s, float z)
{
    return s->road_amp * wn_sinf(z * s->road_freq * 2.0f * WN_PI) +
           s->road_amp * 0.35f * wn_sinf(z * s->road_freq * 4.7f + 1.3f);
}

/* 0 outside the road band, 1 on the road. */
static float road_mask(const wn_scene *s, float x, float z, float extra)
{
    if (s->road_width <= 0.0f || z < s->road_z0 || z > s->road_z1) return 0.0f;
    float d = wn_absf(x - wn_scene_road_x(s, z));
    float half = s->road_width * 0.5f;
    return 1.0f - wn_smoothstep(half, half + extra, d);
}

static float raw_terrain(const wn_scene *s, float x, float z)
{
    float h = s->terrain_base + s->terrain_amp * (fbm(x / s->terrain_scale, z / s->terrain_scale, (uint32_t)s->seed) - 0.5f) * 2.0f;
    for (int i = 0; i < s->nhills; i++) {
        float dx = x - s->hills[i].x, dz = z - s->hills[i].z;
        float d = wn_sqrtf(dx * dx + dz * dz) / s->hills[i].radius;
        if (d < 1.0f) h += s->hills[i].height * (1.0f - wn_smoothstep(0.0f, 1.0f, d));
    }
    return h;
}

float wn_scene_terrain_height(const wn_scene *s, float x, float z)
{
    float h = raw_terrain(s, x, z);
    float m = road_mask(s, x, z, 3.0f);
    if (m > 0.0f) {
        float road_h = raw_terrain(s, wn_scene_road_x(s, z), z) - 0.15f;
        h = wn_lerpf(h, road_h, m);
    }
    return h;
}

static float track_dent(const wn_scene *s, float x, float z)
{
    for (int i = 0; i < s->ntracks; i++) {
        float ax = s->tracks[i].x0, az = s->tracks[i].z0, bx = s->tracks[i].x1, bz = s->tracks[i].z1;
        float dx = bx - ax, dz = bz - az;
        float len = wn_sqrtf(dx * dx + dz * dz);
        if (len <= 0.0f) continue;
        float ux = dx / len, uz = dz / len;
        float t = (x - ax) * ux + (z - az) * uz;
        if (t < 0.0f || t > len) continue;
        float side = -(x - ax) * uz + (z - az) * ux;
        int k = (int)(t / 0.42f + 0.5f);
        float ft = (float)k * 0.42f;
        float fs = (k & 1) ? 0.11f : -0.11f;
        float ex = t - ft, es = side - fs;
        if (ex * ex + es * es < 0.065f * 0.065f) return 0.6f;
    }
    return 0.0f;
}

float wn_scene_snow_height(const wn_scene *s, float x, float z)
{
    float g = wn_scene_terrain_height(s, x, z);
    float depth = s->snow_depth + s->snow_var * (value_noise(x * 0.35f, z * 0.35f, (uint32_t)s->seed + 9) - 0.5f) * 2.0f;
    float m = road_mask(s, x, z, 0.8f);
    depth = wn_lerpf(depth, s->snow_depth * 0.3f, m); /* trodden road */
    depth *= 1.0f - track_dent(s, x, z);
    return g + WN_MAX(depth, 0.0f);
}

/* ---- Trees ---- */

static bool in_any_placement(const wn_scene *s, float x, float z, float margin)
{
    for (int i = 0; i < s->nplace; i++) {
        const wn_placement *pl = &s->place[i];
        if (x > pl->mn.x - margin && x < pl->mx.x + margin && z > pl->mn.z - margin && z < pl->mx.z + margin) return true;
    }
    return false;
}

bool wn_scene_tree_in_cell(const wn_scene *s, int cx, int cz, wn_tree *t)
{
    if (s->forest_prob <= 0.0f) return false;
    uint32_t h = wn_hash_ivec3(cx, 77, cz, (uint32_t)s->seed);
    if ((float)(h >> 8) * (1.0f / 16777216.0f) >= s->forest_prob) return false;
    float jx = hash01(cx, 1, cz, (uint32_t)s->seed), jz = hash01(cx, 2, cz, (uint32_t)s->seed);
    t->x = ((float)cx + 0.15f + 0.7f * jx) * s->forest_cell;
    t->z = ((float)cz + 0.15f + 0.7f * jz) * s->forest_cell;
    if (t->x < s->bounds_mn.x - 30.0f || t->x > s->bounds_mx.x + 30.0f || t->z < s->bounds_mn.z - 30.0f ||
        t->z > s->bounds_mx.z + 30.0f)
        return false;
    if (road_mask(s, t->x, t->z, s->forest_clear) > 0.0f) return false;
    if (in_any_placement(s, t->x, t->z, 2.0f)) return false;
    float r = hash01(cx, 3, cz, (uint32_t)s->seed);
    t->height = 7.0f + 7.0f * r;
    t->trunk_r = 0.18f + 0.14f * hash01(cx, 4, cz, (uint32_t)s->seed);
    t->crown_r = 1.5f + 0.18f * t->height * (0.7f + 0.3f * hash01(cx, 5, cz, (uint32_t)s->seed));
    t->tiers = 4 + (int)(hash01(cx, 6, cz, (uint32_t)s->seed) * 3.0f);
    t->base = wn_scene_terrain_height(s, t->x, t->z) - 0.3f;
    t->hash = h;
    return true;
}

/* Material of a pine at a point relative to the trunk, or 0. */
static uint32_t pine_sample(const wn_tree *t, float dx, float h, float dz)
{
    if (h < 0.0f || h > t->height + 0.2f) return 0;
    float d2 = dx * dx + dz * dz;
    if (h < t->height * 0.9f && d2 < t->trunk_r * t->trunk_r) return WN_MAT_PINE_WOOD;
    float crown0 = t->height * 0.2f;
    if (h < crown0 || d2 > t->crown_r * t->crown_r * 1.3f) return 0;
    /* Jagged branch outline from the angle around the trunk. */
    float ang = wn_atan2f(dz, dx);
    int bucket = (int)((ang + WN_PI) * 2.6f);
    float jag = 0.78f + 0.3f * hash01(bucket, (int32_t)(h * 2.5f), 0, t->hash);
    for (int pass = 0; pass < 2; pass++) {
        float hh = pass == 0 ? h : h - 0.13f; /* pass 1: just above a tier surface */
        if (hh < crown0) continue;
        float u = (hh - crown0) / (t->height - crown0);
        float tf = u * (float)t->tiers;
        float frac = tf - (float)(int)tf;
        float env = t->crown_r * (1.0f - u * 0.92f);
        float r = env * (0.42f + 0.58f * (1.0f - frac)) * jag;
        if (d2 < r * r) return pass == 0 ? WN_MAT_PINE_NEEDLES : WN_MAT_SNOW;
    }
    return 0;
}

/* Local point for a placement (inverse quarter-turn rotation). */
static wn_vec3 to_local(const wn_placement *pl, wn_vec3 w)
{
    float x = w.x - pl->pos.x, z = w.z - pl->pos.z;
    for (int r = 0; r < pl->rot; r++) {
        float t = x;
        x = -z;
        z = t;
    }
    return wn_v3(x, w.y - pl->pos.y, z);
}

typedef struct gen_cache {
    wn_tree trees[64];
    int ntrees;
    const wn_placement *places[WN_MAX_PLACEMENTS];
    int nplaces;
} gen_cache;

static void gather(const wn_scene *s, float x0, float z0, float x1, float z1, gen_cache *gc)
{
    gc->ntrees = 0;
    gc->nplaces = 0;
    float reach = 6.0f;
    int cx0 = wn_floorf_i((x0 - reach) / s->forest_cell), cx1 = wn_floorf_i((x1 + reach) / s->forest_cell);
    int cz0 = wn_floorf_i((z0 - reach) / s->forest_cell), cz1 = wn_floorf_i((z1 + reach) / s->forest_cell);
    for (int cz = cz0; cz <= cz1; cz++)
        for (int cx = cx0; cx <= cx1 && gc->ntrees < 64; cx++) {
            wn_tree t;
            if (!wn_scene_tree_in_cell(s, cx, cz, &t)) continue;
            float r = t.crown_r * 1.2f;
            if (t.x + r < x0 || t.x - r > x1 || t.z + r < z0 || t.z - r > z1) continue;
            gc->trees[gc->ntrees++] = t;
        }
    for (int i = 0; i < s->nplace; i++) {
        const wn_placement *pl = &s->place[i];
        if (pl->mx.x < x0 - 1 || pl->mn.x > x1 + 1 || pl->mx.z < z0 - 1 || pl->mn.z > z1 + 1) continue;
        gc->places[gc->nplaces++] = pl;
    }
}

/* Classifies one voxel centre. ground/snow heights are precomputed per column. */
static wn_voxel classify(const wn_scene *s, const gen_cache *gc, wn_vec3 p, float ground, float snow, int32_t vy,
                         int32_t floor_vy)
{
    if (vy <= floor_vy) return wn_vox_make(WN_MAT_BEDROCK, 0);
    float half = WN_VOXEL_SIZE * 0.5f;
    /* Prefabs override terrain. */
    for (int i = 0; i < gc->nplaces; i++) {
        const wn_placement *pl = gc->places[i];
        if (p.x < pl->mn.x || p.x > pl->mx.x || p.z < pl->mn.z || p.z > pl->mx.z || p.y < pl->mn.y || p.y > pl->mx.y + 0.3f)
            continue;
        wn_vec3 l = to_local(pl, p);
        int m = wn_prefab_sample(pl->prefab, l);
        if (m > 0) return wn_vox_make((uint32_t)m, 0);
        if (m == 0 && p.y > ground) return 0; /* carved, but never into the ground */
        if (pl->prefab->snow_cap && p.y > snow - half) {
            int below = wn_prefab_sample(pl->prefab, wn_v3(l.x, l.y - WN_VOXEL_SIZE, l.z));
            if (below > 0 && below != WN_MAT_WINDOW_GLOW) return wn_vox_make(WN_MAT_SNOW, 9);
        }
    }
    if (p.y < ground) {
        return wn_vox_make(p.y < ground - 1.6f ? WN_MAT_STONE : WN_MAT_EARTH, 0);
    }
    if (p.y - half < snow) {
        float top = snow - (p.y - half);
        if (top >= WN_VOXEL_SIZE) return wn_vox_make(WN_MAT_SNOW, 0);
        /* Quarter-voxel steps: a smooth drift that still merges into big quads. */
        uint32_t fill = ((uint32_t)(top / WN_VOXEL_SIZE * 16.0f)) & ~3u;
        if (fill == 0) return 0;
        wn_voxel v = wn_vox_make(WN_MAT_SNOW, fill);
        if (road_mask(s, p.x, p.z, 0.4f) > 0.5f) v = wn_vox_set_damage(v, 4); /* trodden */
        return v;
    }
    for (int i = 0; i < gc->ntrees; i++) {
        const wn_tree *t = &gc->trees[i];
        uint32_t m = pine_sample(t, p.x - t->x, p.y - t->base, p.z - t->z);
        if (m) return wn_vox_make(m, m == WN_MAT_SNOW ? 10u : 0u);
    }
    return 0;
}

wn_voxel wn_scene_voxel(const wn_scene *s, int32_t x, int32_t y, int32_t z)
{
    wn_vec3 p = wn_voxel_center(wn_iv3(x, y, z));
    gen_cache *gc = (gen_cache *)malloc(sizeof *gc);
    gather(s, p.x, p.z, p.x, p.z, gc);
    wn_voxel v = classify(s, gc, p, wn_scene_terrain_height(s, p.x, p.z), wn_scene_snow_height(s, p.x, p.z), y,
                          s->chunk_y_min * WN_CHUNK_SIZE);
    free(gc);
    return v;
}

void wn_scene_generate_chunk(void *user, wn_chunk *c)
{
    const wn_scene *s = (const wn_scene *)user;
    const int N = WN_CHUNK_SIZE;
    int32_t bx = c->pos.x * N, by = c->pos.y * N, bz = c->pos.z * N;
    float ground[WN_CHUNK_SIZE * WN_CHUNK_SIZE], snow[WN_CHUNK_SIZE * WN_CHUNK_SIZE];
    float gmin = 1e9f, smax = -1e9f;
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            wn_vec3 p = wn_voxel_center(wn_iv3(bx + x, 0, bz + z));
            float g = wn_scene_terrain_height(s, p.x, p.z), sn = wn_scene_snow_height(s, p.x, p.z);
            ground[z * N + x] = g;
            snow[z * N + x] = sn;
            gmin = WN_MIN(gmin, g);
            smax = WN_MAX(smax, sn);
        }
    gen_cache *gc = (gen_cache *)malloc(sizeof *gc);
    wn_vec3 c0 = wn_voxel_to_world(wn_iv3(bx, by, bz)), c1 = wn_voxel_to_world(wn_iv3(bx + N, by + N, bz + N));
    gather(s, c0.x, c0.z, c1.x, c1.z, gc);
    /* Only trees reaching into this chunk's height range matter. */
    int keep = 0;
    for (int i = 0; i < gc->ntrees; i++) {
        const wn_tree *t = &gc->trees[i];
        if (t->base + t->height + 0.3f < c0.y || t->base > c1.y) continue;
        gc->trees[keep++] = *t;
    }
    gc->ntrees = keep;
    float top = smax;
    for (int i = 0; i < gc->ntrees; i++) top = WN_MAX(top, gc->trees[i].base + gc->trees[i].height + 0.3f);
    for (int i = 0; i < gc->nplaces; i++) top = WN_MAX(top, gc->places[i]->mx.y + 0.3f);
    float bottom = gmin - 1.7f;
    for (int i = 0; i < gc->nplaces; i++) bottom = WN_MIN(bottom, gc->places[i]->mn.y - 0.2f);
    int32_t floor_vy = s->chunk_y_min * N;
    if (c0.y > top) {
        c->uniform = wn_vox_set_sky(0, 15);
    } else if (c1.y < bottom && by > floor_vy) {
        c->uniform = wn_vox_make(WN_MAT_STONE, 0);
    } else {
        wn_chunk_expand(c);
        for (int y = 0; y < N; y++)
            for (int z = 0; z < N; z++)
                for (int x = 0; x < N; x++) {
                    wn_vec3 p = wn_voxel_center(wn_iv3(bx + x, by + y, bz + z));
                    c->vox[wn_chunk_index(x, y, z)] =
                        classify(s, gc, p, ground[z * N + x], snow[z * N + x], by + y, floor_vy);
                }
        wn_chunk_compact(c);
    }
    free(gc);
}

/* ---- Far scenery ---- */

void wn_scene_build_far(const wn_scene *s, int shift, wn_far_grid *g)
{
    int step = 1 << shift;
    wn_ivec3 a = wn_world_to_voxel(wn_v3(s->bounds_mn.x - 40.0f, s->bounds_mn.y, s->bounds_mn.z - 40.0f));
    wn_ivec3 b = wn_world_to_voxel(wn_v3(s->bounds_mx.x + 40.0f, s->bounds_mx.y, s->bounds_mx.z + 40.0f));
    a = wn_iv3(wn_floordiv(a.x, step) * step, wn_floordiv(a.y, step) * step, wn_floordiv(a.z, step) * step);
    int nx = (b.x - a.x) / step + 1, ny = (b.y - a.y) / step + 1, nz = (b.z - a.z) / step + 1;
    g->shift = shift;
    g->origin = a;
    g->sx = nx + 2;
    g->sy = ny + 2;
    g->sz = nz + 2;
    g->cells = (wn_voxel *)calloc((size_t)g->sx * g->sy * g->sz, sizeof(wn_voxel));
    gen_cache *gc = (gen_cache *)malloc(sizeof *gc);
    float cell_m = (float)step * WN_VOXEL_SIZE;
    for (int z = 0; z < nz; z++) {
        for (int x = 0; x < nx; x++) {
            int32_t vx = a.x + x * step + step / 2, vz = a.z + z * step + step / 2;
            wn_vec3 p = wn_voxel_center(wn_iv3(vx, 0, vz));
            float ground = wn_scene_terrain_height(s, p.x, p.z), snow = wn_scene_snow_height(s, p.x, p.z);
            gather(s, p.x - cell_m * 0.5f, p.z - cell_m * 0.5f, p.x + cell_m * 0.5f, p.z + cell_m * 0.5f, gc);
            for (int y = 0; y < ny; y++) {
                int32_t vy = a.y + y * step + step / 2;
                p.y = ((float)vy + 0.5f) * WN_VOXEL_SIZE;
                wn_voxel v = classify(s, gc, p, ground, snow, vy, s->chunk_y_min * WN_CHUNK_SIZE);
                /* A coarse cell is snow-covered ground if the snow surface lies inside it. */
                if (!wn_vox_mat(v) && p.y - cell_m * 0.5f < snow && p.y > snow - cell_m) v = wn_vox_make(WN_MAT_SNOW, 0);
                v = wn_vox_set_fill(v, 0);
                g->cells[((size_t)(y + 1) * g->sz + (size_t)(z + 1)) * g->sx + (size_t)(x + 1)] = v;
            }
        }
    }
    free(gc);
}

void wn_far_grid_free(wn_far_grid *g)
{
    free(g->cells);
    memset(g, 0, sizeof *g);
}
