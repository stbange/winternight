/* Winternight engine - device-independent input (spec input-controls).
 * Platform code feeds raw "sources"; game code only sees actions. */
#ifndef WN_INPUT_H
#define WN_INPUT_H

#include "wn_base.h"

typedef enum wn_action {
    WN_ACT_MOVE_X,  /* axis: strafe right positive */
    WN_ACT_MOVE_Y,  /* axis: forward positive */
    WN_ACT_LOOK_X,  /* axis delta: yaw right positive (radians per step) */
    WN_ACT_LOOK_Y,  /* axis delta: pitch up positive */
    WN_ACT_ATTACK,  /* button */
    WN_ACT_BLOCK,
    WN_ACT_JUMP,
    WN_ACT_INTERACT,
    WN_ACT_RUN,
    WN_ACT_MENU,
    WN_ACT_COUNT
} wn_action;

#define WN_FIRST_BUTTON_ACTION WN_ACT_ATTACK

typedef enum wn_source_type {
    WN_SRC_KEY,          /* code = SDL scancode number */
    WN_SRC_MOUSE_BUTTON, /* code = 1 left, 2 middle, 3 right */
    WN_SRC_MOUSE_MOTION, /* code = 0 x, 1 y; values are pixel deltas */
    WN_SRC_PAD_BUTTON,   /* code = SDL gamepad button number */
    WN_SRC_PAD_AXIS,     /* code = SDL gamepad axis number, value -1..1 */
    WN_SRC_TOUCH,        /* code = wn_touch_control */
    WN_SRC_XR,           /* code = wn_xr_control */
    WN_SRC_TYPE_COUNT
} wn_source_type;

typedef enum wn_device_class {
    WN_DEVICE_KEYBOARD_MOUSE,
    WN_DEVICE_GAMEPAD,
    WN_DEVICE_TOUCH,
    WN_DEVICE_XR
} wn_device_class;

typedef struct wn_binding {
    wn_source_type type;
    int32_t code;
    wn_action action;
    float scale; /* axis contribution, or 1 for buttons */
} wn_binding;

#define WN_MAX_BINDINGS 64
#define WN_MAX_SOURCE_CODES 512

typedef struct wn_input_frame {
    float axis[4];    /* MOVE_X, MOVE_Y, LOOK_X, LOOK_Y */
    uint32_t down;    /* bit per button action (bit = action index) */
    uint32_t pressed; /* went down this step */
} wn_input_frame;

typedef struct wn_input {
    wn_binding bindings[WN_MAX_BINDINGS];
    int nbindings;
    /* Current raw state, indexed [type][code]. Motion sources accumulate. */
    float state[WN_SRC_TYPE_COUNT][WN_MAX_SOURCE_CODES];
    uint32_t prev_down;
    wn_device_class last_device;
    float pad_deadzone;
} wn_input;

void wn_input_init(wn_input *in);
void wn_input_default_bindings(wn_input *in);
void wn_input_clear_bindings(wn_input *in);
bool wn_input_bind(wn_input *in, wn_source_type type, int32_t code, wn_action action, float scale);
/* Removes every binding of an action. Returns how many were removed. */
int wn_input_unbind_action(wn_input *in, wn_action action);
/* Loads bindings from text ("bind <key|mouse|pad|padaxis|touch> <code> <action> [scale]").
 * On error nothing changes and false is returned with a message in err. */
bool wn_input_load_bindings(wn_input *in, const char *text, char *err, size_t errlen);
bool wn_action_from_name(const char *name, wn_action *out);

/* Raw source update from the platform layer. */
void wn_input_set(wn_input *in, wn_source_type type, int32_t code, float value);
/* Adds to an accumulating source (mouse motion, look deltas). */
void wn_input_add(wn_input *in, wn_source_type type, int32_t code, float delta);
/* Builds the frame for one simulation step and consumes accumulated motion. */
wn_input_frame wn_input_poll(wn_input *in);
bool wn_frame_down(const wn_input_frame *f, wn_action a);
bool wn_frame_pressed(const wn_input_frame *f, wn_action a);

/* ---- Touch controls (spec input-controls: Automatic touch controls) ---- */

typedef enum wn_touch_control {
    WN_TOUCH_STICK_X,
    WN_TOUCH_STICK_Y,
    WN_TOUCH_LOOK_X,
    WN_TOUCH_LOOK_Y,
    WN_TOUCH_BTN_ATTACK,
    WN_TOUCH_BTN_BLOCK,
    WN_TOUCH_BTN_JUMP,
    WN_TOUCH_BTN_INTERACT,
    WN_TOUCH_CONTROL_COUNT
} wn_touch_control;

#define WN_TOUCH_MAX_FINGERS 10
#define WN_TOUCH_BUTTONS 4

typedef enum wn_finger_role { WN_FINGER_NONE, WN_FINGER_STICK, WN_FINGER_LOOK, WN_FINGER_BUTTON } wn_finger_role;

typedef struct wn_touch_button {
    float x, y, r; /* centre and radius in normalised screen units (height = 1) */
    wn_touch_control control;
} wn_touch_button;

typedef struct wn_touch {
    bool visible;
    float aspect; /* screen width / height */
    float stick_radius;
    float look_sensitivity;
    wn_touch_button buttons[WN_TOUCH_BUTTONS];
    struct {
        int64_t id;
        wn_finger_role role;
        int button;
        float ox, oy; /* origin (stick centre) */
        float x, y;   /* current position */
    } fingers[WN_TOUCH_MAX_FINGERS];
} wn_touch;

void wn_touch_init(wn_touch *t, float aspect, bool coarse_pointer);
void wn_touch_set_aspect(wn_touch *t, float aspect);
/* Positions are in pixels-independent units: x in [0, aspect], y in [0, 1], y down. */
void wn_touch_down(wn_touch *t, wn_input *in, int64_t finger, float x, float y);
void wn_touch_move(wn_touch *t, wn_input *in, int64_t finger, float x, float y);
void wn_touch_up(wn_touch *t, wn_input *in, int64_t finger);
/* Keyboard or gamepad use hides the touch controls. */
void wn_touch_notice_device(wn_touch *t, wn_device_class dev);
/* Stick deflection for a drag (dx, dy) with given radius: clamped to unit circle. */
void wn_touch_stick_value(float dx, float dy, float radius, float *out_x, float *out_y);

/* ---- Replay (spec input-controls: Input recording and replay) ---- */

typedef struct wn_replay {
    uint64_t seed;
    wn_input_frame *frames;
    uint32_t *checksums; /* optional per-step state checksum, 0 if unknown */
    uint32_t count;
    uint32_t cap;
    uint32_t cursor;
} wn_replay;

void wn_replay_init(wn_replay *r, uint64_t seed);
void wn_replay_free(wn_replay *r);
void wn_replay_push(wn_replay *r, const wn_input_frame *f, uint32_t checksum);
/* Returns false when the replay is exhausted. */
bool wn_replay_next(wn_replay *r, wn_input_frame *out, uint32_t *checksum);
/* Serialisation into a malloc'd buffer / from memory. */
uint8_t *wn_replay_serialize(const wn_replay *r, size_t *out_len);
bool wn_replay_deserialize(wn_replay *r, const uint8_t *data, size_t len, char *err, size_t errlen);

#endif
