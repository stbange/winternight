#include "wn/wn_input.h"

#include "wn/wn_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SDL3 scancode numbers (stable USB HID usage ids) so the core needs no SDL. */
enum {
    SC_A = 4, SC_D = 7, SC_E = 8, SC_F = 9, SC_S = 22, SC_W = 26,
    SC_ESC = 41, SC_SPACE = 44, SC_RIGHT = 79, SC_LEFT = 80, SC_DOWN = 81, SC_UP = 82,
    SC_LCTRL = 224, SC_LSHIFT = 225
};
/* SDL3 gamepad buttons / axes. */
enum { PAD_SOUTH = 0, PAD_EAST = 1, PAD_WEST = 2, PAD_NORTH = 3, PAD_START = 6, PAD_LSTICK = 7 };
enum { AX_LX = 0, AX_LY = 1, AX_RX = 2, AX_RY = 3, AX_LT = 4, AX_RT = 5 };

static const char *k_action_names[WN_ACT_COUNT] = {
    "move_x", "move_y", "look_x", "look_y", "attack", "block", "jump", "interact", "run", "menu"};

bool wn_action_from_name(const char *name, wn_action *out)
{
    for (int i = 0; i < WN_ACT_COUNT; i++) {
        if (strcmp(name, k_action_names[i]) == 0) {
            *out = (wn_action)i;
            return true;
        }
    }
    return false;
}

void wn_input_init(wn_input *in)
{
    memset(in, 0, sizeof *in);
    in->pad_deadzone = 0.18f;
    in->last_device = WN_DEVICE_KEYBOARD_MOUSE;
    wn_input_default_bindings(in);
}

void wn_input_clear_bindings(wn_input *in) { in->nbindings = 0; }

bool wn_input_bind(wn_input *in, wn_source_type type, int32_t code, wn_action action, float scale)
{
    if (in->nbindings >= WN_MAX_BINDINGS || code < 0 || code >= WN_MAX_SOURCE_CODES) return false;
    wn_binding *b = &in->bindings[in->nbindings++];
    b->type = type;
    b->code = code;
    b->action = action;
    b->scale = scale;
    return true;
}

int wn_input_unbind_action(wn_input *in, wn_action action)
{
    int w = 0, removed = 0;
    for (int i = 0; i < in->nbindings; i++) {
        if (in->bindings[i].action == action) {
            removed++;
            continue;
        }
        in->bindings[w++] = in->bindings[i];
    }
    in->nbindings = w;
    return removed;
}

void wn_input_default_bindings(wn_input *in)
{
    wn_input_clear_bindings(in);
    /* keyboard + mouse */
    wn_input_bind(in, WN_SRC_KEY, SC_W, WN_ACT_MOVE_Y, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_UP, WN_ACT_MOVE_Y, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_S, WN_ACT_MOVE_Y, -1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_DOWN, WN_ACT_MOVE_Y, -1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_D, WN_ACT_MOVE_X, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_A, WN_ACT_MOVE_X, -1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_RIGHT, WN_ACT_LOOK_X, 0.04f);
    wn_input_bind(in, WN_SRC_KEY, SC_LEFT, WN_ACT_LOOK_X, -0.04f);
    wn_input_bind(in, WN_SRC_MOUSE_MOTION, 0, WN_ACT_LOOK_X, 0.0035f);
    wn_input_bind(in, WN_SRC_MOUSE_MOTION, 1, WN_ACT_LOOK_Y, -0.0035f);
    wn_input_bind(in, WN_SRC_MOUSE_BUTTON, 1, WN_ACT_ATTACK, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_F, WN_ACT_ATTACK, 1.0f);
    wn_input_bind(in, WN_SRC_MOUSE_BUTTON, 3, WN_ACT_BLOCK, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_LCTRL, WN_ACT_BLOCK, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_SPACE, WN_ACT_JUMP, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_E, WN_ACT_INTERACT, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_LSHIFT, WN_ACT_RUN, 1.0f);
    wn_input_bind(in, WN_SRC_KEY, SC_ESC, WN_ACT_MENU, 1.0f);
    /* gamepad */
    wn_input_bind(in, WN_SRC_PAD_AXIS, AX_LX, WN_ACT_MOVE_X, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_AXIS, AX_LY, WN_ACT_MOVE_Y, -1.0f);
    wn_input_bind(in, WN_SRC_PAD_AXIS, AX_RX, WN_ACT_LOOK_X, 0.05f);
    wn_input_bind(in, WN_SRC_PAD_AXIS, AX_RY, WN_ACT_LOOK_Y, -0.04f);
    wn_input_bind(in, WN_SRC_PAD_AXIS, AX_RT, WN_ACT_ATTACK, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_BUTTON, PAD_WEST, WN_ACT_ATTACK, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_AXIS, AX_LT, WN_ACT_BLOCK, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_BUTTON, PAD_SOUTH, WN_ACT_JUMP, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_BUTTON, PAD_NORTH, WN_ACT_INTERACT, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_BUTTON, PAD_LSTICK, WN_ACT_RUN, 1.0f);
    wn_input_bind(in, WN_SRC_PAD_BUTTON, PAD_START, WN_ACT_MENU, 1.0f);
    /* touch */
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_STICK_X, WN_ACT_MOVE_X, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_STICK_Y, WN_ACT_MOVE_Y, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_LOOK_X, WN_ACT_LOOK_X, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_LOOK_Y, WN_ACT_LOOK_Y, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_BTN_ATTACK, WN_ACT_ATTACK, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_BTN_BLOCK, WN_ACT_BLOCK, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_BTN_JUMP, WN_ACT_JUMP, 1.0f);
    wn_input_bind(in, WN_SRC_TOUCH, WN_TOUCH_BTN_INTERACT, WN_ACT_INTERACT, 1.0f);
}

static bool source_from_name(const char *name, wn_source_type *out)
{
    static const struct {
        const char *n;
        wn_source_type t;
    } map[] = {{"key", WN_SRC_KEY},         {"mouse", WN_SRC_MOUSE_BUTTON}, {"motion", WN_SRC_MOUSE_MOTION},
               {"pad", WN_SRC_PAD_BUTTON},  {"padaxis", WN_SRC_PAD_AXIS},   {"touch", WN_SRC_TOUCH},
               {"xr", WN_SRC_XR}};
    for (size_t i = 0; i < WN_ARRAY_LEN(map); i++) {
        if (strcmp(map[i].n, name) == 0) {
            *out = map[i].t;
            return true;
        }
    }
    return false;
}

bool wn_input_load_bindings(wn_input *in, const char *text, char *err, size_t errlen)
{
    wn_input tmp = *in;
    wn_input_clear_bindings(&tmp);
    wn_parser p;
    wn_parser_init(&p, text, strlen(text));
    while (wn_parser_next(&p)) {
        if (!wn_parser_is(&p, 0, "bind")) {
            wn_parser_fail(&p, "expected 'bind'");
            break;
        }
        if (!wn_parser_expect_fields(&p, 4)) break;
        wn_source_type t;
        wn_action a;
        int32_t code;
        float scale = 1.0f;
        if (!source_from_name(p.fields[1], &t)) {
            wn_parser_fail(&p, "unknown source type");
            break;
        }
        if (!wn_parser_int(&p, 2, &code)) break;
        if (!wn_action_from_name(p.fields[3], &a)) {
            wn_parser_fail(&p, "unknown action");
            break;
        }
        if (p.nfields > 4 && !wn_parser_float(&p, 4, &scale)) break;
        if (!wn_input_bind(&tmp, t, code, a, scale)) {
            wn_parser_fail(&p, "binding rejected (code out of range or too many bindings)");
            break;
        }
    }
    if (p.error[0]) {
        if (err) snprintf(err, errlen, "%s", p.error);
        return false;
    }
    memcpy(in->bindings, tmp.bindings, sizeof tmp.bindings);
    in->nbindings = tmp.nbindings;
    return true;
}

static wn_device_class device_of(wn_source_type t)
{
    switch (t) {
    case WN_SRC_PAD_BUTTON:
    case WN_SRC_PAD_AXIS: return WN_DEVICE_GAMEPAD;
    case WN_SRC_TOUCH: return WN_DEVICE_TOUCH;
    case WN_SRC_XR: return WN_DEVICE_XR;
    default: return WN_DEVICE_KEYBOARD_MOUSE;
    }
}

void wn_input_set(wn_input *in, wn_source_type type, int32_t code, float value)
{
    if (code < 0 || code >= WN_MAX_SOURCE_CODES) return;
    if (type == WN_SRC_PAD_AXIS && wn_absf(value) < in->pad_deadzone) value = 0.0f;
    if (value != 0.0f) in->last_device = device_of(type);
    in->state[type][code] = value;
}

void wn_input_add(wn_input *in, wn_source_type type, int32_t code, float delta)
{
    if (code < 0 || code >= WN_MAX_SOURCE_CODES) return;
    if (delta != 0.0f) in->last_device = device_of(type);
    in->state[type][code] += delta;
}

static bool is_accumulating(const wn_binding *b)
{
    return b->type == WN_SRC_MOUSE_MOTION ||
           (b->type == WN_SRC_TOUCH && (b->code == WN_TOUCH_LOOK_X || b->code == WN_TOUCH_LOOK_Y));
}

wn_input_frame wn_input_poll(wn_input *in)
{
    wn_input_frame f;
    memset(&f, 0, sizeof f);
    for (int i = 0; i < in->nbindings; i++) {
        const wn_binding *b = &in->bindings[i];
        float v = in->state[b->type][b->code];
        if (b->action < WN_FIRST_BUTTON_ACTION) {
            f.axis[b->action] += v * b->scale;
        } else if (v > 0.5f) {
            f.down |= 1u << b->action;
        }
    }
    /* Clamp movement to the unit range so stacked bindings don't speed up. */
    f.axis[WN_ACT_MOVE_X] = WN_CLAMP(f.axis[WN_ACT_MOVE_X], -1.0f, 1.0f);
    f.axis[WN_ACT_MOVE_Y] = WN_CLAMP(f.axis[WN_ACT_MOVE_Y], -1.0f, 1.0f);
    for (int i = 0; i < in->nbindings; i++) {
        const wn_binding *b = &in->bindings[i];
        if (is_accumulating(b)) in->state[b->type][b->code] = 0.0f;
    }
    f.pressed = f.down & ~in->prev_down;
    in->prev_down = f.down;
    return f;
}

bool wn_frame_down(const wn_input_frame *f, wn_action a) { return (f->down >> a) & 1u; }
bool wn_frame_pressed(const wn_input_frame *f, wn_action a) { return (f->pressed >> a) & 1u; }

/* ---------------- Touch ---------------- */

void wn_touch_set_aspect(wn_touch *t, float aspect)
{
    t->aspect = aspect;
    /* Buttons hug the bottom-right corner, sized relative to screen height. */
    float r = 0.075f;
    t->buttons[0] = (wn_touch_button){aspect - 0.14f, 0.80f, r * 1.25f, WN_TOUCH_BTN_ATTACK};
    t->buttons[1] = (wn_touch_button){aspect - 0.33f, 0.86f, r, WN_TOUCH_BTN_BLOCK};
    t->buttons[2] = (wn_touch_button){aspect - 0.12f, 0.58f, r, WN_TOUCH_BTN_JUMP};
    t->buttons[3] = (wn_touch_button){aspect - 0.30f, 0.66f, r, WN_TOUCH_BTN_INTERACT};
}

void wn_touch_init(wn_touch *t, float aspect, bool coarse_pointer)
{
    memset(t, 0, sizeof *t);
    t->visible = coarse_pointer;
    t->stick_radius = 0.12f;
    t->look_sensitivity = 3.0f;
    for (int i = 0; i < WN_TOUCH_MAX_FINGERS; i++) t->fingers[i].id = -1;
    wn_touch_set_aspect(t, aspect);
}

void wn_touch_stick_value(float dx, float dy, float radius, float *ox, float *oy)
{
    float x = dx / radius, y = dy / radius;
    float l = wn_sqrtf(x * x + y * y);
    if (l > 1.0f) {
        x /= l;
        y /= l;
    }
    *ox = x;
    *oy = y;
}

static int finger_slot(wn_touch *t, int64_t id, bool alloc)
{
    for (int i = 0; i < WN_TOUCH_MAX_FINGERS; i++)
        if (t->fingers[i].id == id) return i;
    if (!alloc) return -1;
    for (int i = 0; i < WN_TOUCH_MAX_FINGERS; i++)
        if (t->fingers[i].id < 0) return i;
    return -1;
}

void wn_touch_down(wn_touch *t, wn_input *in, int64_t id, float x, float y)
{
    t->visible = true;
    in->last_device = WN_DEVICE_TOUCH;
    int s = finger_slot(t, id, true);
    if (s < 0) return;
    t->fingers[s].id = id;
    t->fingers[s].ox = t->fingers[s].x = x;
    t->fingers[s].oy = t->fingers[s].y = y;
    for (int b = 0; b < WN_TOUCH_BUTTONS; b++) {
        float dx = x - t->buttons[b].x, dy = y - t->buttons[b].y;
        if (dx * dx + dy * dy <= t->buttons[b].r * t->buttons[b].r) {
            t->fingers[s].role = WN_FINGER_BUTTON;
            t->fingers[s].button = b;
            wn_input_set(in, WN_SRC_TOUCH, t->buttons[b].control, 1.0f);
            return;
        }
    }
    t->fingers[s].role = (x < t->aspect * 0.5f) ? WN_FINGER_STICK : WN_FINGER_LOOK;
}

void wn_touch_move(wn_touch *t, wn_input *in, int64_t id, float x, float y)
{
    int s = finger_slot(t, id, false);
    if (s < 0) return;
    float px = t->fingers[s].x, py = t->fingers[s].y;
    t->fingers[s].x = x;
    t->fingers[s].y = y;
    if (t->fingers[s].role == WN_FINGER_STICK) {
        float sx, sy;
        wn_touch_stick_value(x - t->fingers[s].ox, y - t->fingers[s].oy, t->stick_radius, &sx, &sy);
        wn_input_set(in, WN_SRC_TOUCH, WN_TOUCH_STICK_X, sx);
        wn_input_set(in, WN_SRC_TOUCH, WN_TOUCH_STICK_Y, -sy); /* screen y is down */
    } else if (t->fingers[s].role == WN_FINGER_LOOK) {
        wn_input_add(in, WN_SRC_TOUCH, WN_TOUCH_LOOK_X, (x - px) * t->look_sensitivity);
        wn_input_add(in, WN_SRC_TOUCH, WN_TOUCH_LOOK_Y, -(y - py) * t->look_sensitivity);
    }
}

void wn_touch_up(wn_touch *t, wn_input *in, int64_t id)
{
    int s = finger_slot(t, id, false);
    if (s < 0) return;
    if (t->fingers[s].role == WN_FINGER_STICK) {
        wn_input_set(in, WN_SRC_TOUCH, WN_TOUCH_STICK_X, 0.0f);
        wn_input_set(in, WN_SRC_TOUCH, WN_TOUCH_STICK_Y, 0.0f);
    } else if (t->fingers[s].role == WN_FINGER_BUTTON) {
        wn_input_set(in, WN_SRC_TOUCH, t->buttons[t->fingers[s].button].control, 0.0f);
    }
    t->fingers[s].id = -1;
    t->fingers[s].role = WN_FINGER_NONE;
}

void wn_touch_notice_device(wn_touch *t, wn_device_class dev)
{
    if (dev == WN_DEVICE_KEYBOARD_MOUSE || dev == WN_DEVICE_GAMEPAD) t->visible = false;
}

/* ---------------- Replay ---------------- */

#define REPLAY_MAGIC "WNREC1\0\0"

void wn_replay_init(wn_replay *r, uint64_t seed)
{
    memset(r, 0, sizeof *r);
    r->seed = seed;
}

void wn_replay_free(wn_replay *r)
{
    free(r->frames);
    free(r->checksums);
    memset(r, 0, sizeof *r);
}

void wn_replay_push(wn_replay *r, const wn_input_frame *f, uint32_t checksum)
{
    if (r->count == r->cap) {
        r->cap = r->cap ? r->cap * 2 : 256;
        r->frames = (wn_input_frame *)realloc(r->frames, r->cap * sizeof *r->frames);
        r->checksums = (uint32_t *)realloc(r->checksums, r->cap * sizeof *r->checksums);
    }
    r->frames[r->count] = *f;
    r->checksums[r->count] = checksum;
    r->count++;
}

bool wn_replay_next(wn_replay *r, wn_input_frame *out, uint32_t *checksum)
{
    if (r->cursor >= r->count) return false;
    *out = r->frames[r->cursor];
    if (checksum) *checksum = r->checksums[r->cursor];
    r->cursor++;
    return true;
}

/* Little-endian writer helpers so files are portable. */
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint32_t get_u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put_f32(uint8_t *p, float f)
{
    uint32_t v;
    memcpy(&v, &f, 4);
    put_u32(p, v);
}
static float get_f32(const uint8_t *p)
{
    uint32_t v = get_u32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

#define REPLAY_HEADER 24
#define REPLAY_RECORD 28 /* 4 axes + down + pressed + checksum */

uint8_t *wn_replay_serialize(const wn_replay *r, size_t *out_len)
{
    size_t n = REPLAY_HEADER + (size_t)r->count * REPLAY_RECORD;
    uint8_t *buf = (uint8_t *)malloc(n);
    memcpy(buf, REPLAY_MAGIC, 8);
    put_u32(buf + 8, (uint32_t)r->seed);
    put_u32(buf + 12, (uint32_t)(r->seed >> 32));
    put_u32(buf + 16, r->count);
    put_u32(buf + 20, 1u); /* format version */
    uint8_t *p = buf + REPLAY_HEADER;
    for (uint32_t i = 0; i < r->count; i++, p += REPLAY_RECORD) {
        for (int a = 0; a < 4; a++) put_f32(p + a * 4, r->frames[i].axis[a]);
        put_u32(p + 16, r->frames[i].down);
        put_u32(p + 20, r->frames[i].pressed);
        put_u32(p + 24, r->checksums[i]);
    }
    *out_len = n;
    return buf;
}

bool wn_replay_deserialize(wn_replay *r, const uint8_t *data, size_t len, char *err, size_t errlen)
{
    if (len < REPLAY_HEADER || memcmp(data, REPLAY_MAGIC, 8) != 0) {
        snprintf(err, errlen, "not a Winternight replay file");
        return false;
    }
    uint32_t count = get_u32(data + 16);
    if ((len - REPLAY_HEADER) / REPLAY_RECORD < count || len != REPLAY_HEADER + (size_t)count * REPLAY_RECORD) {
        snprintf(err, errlen, "replay file is truncated or corrupted");
        return false;
    }
    uint64_t seed = (uint64_t)get_u32(data + 8) | ((uint64_t)get_u32(data + 12) << 32);
    wn_replay_init(r, seed);
    const uint8_t *p = data + REPLAY_HEADER;
    for (uint32_t i = 0; i < count; i++, p += REPLAY_RECORD) {
        wn_input_frame f;
        for (int a = 0; a < 4; a++) f.axis[a] = get_f32(p + a * 4);
        f.down = get_u32(p + 16);
        f.pressed = get_u32(p + 20);
        wn_replay_push(r, &f, get_u32(p + 24));
    }
    return true;
}
