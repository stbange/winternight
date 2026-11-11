/* Winternight game - automated game tests (spec quality-gates: Automated game
 * tests, Golden-image checks). A script drives the real game step by step;
 * the platform layer renders and captures frames when asked. */
#ifndef WN_GAMETEST_H
#define WN_GAMETEST_H

#include "wn/wn_game.h"
#include "wn/wn_input.h"

typedef enum wn_gt_kind {
    WN_GT_INPUT,    /* hold an action at a value */
    WN_GT_TELEPORT,
    WN_GT_LOOK,
    WN_GT_EXPECT,
    WN_GT_CAPTURE,
    WN_GT_BLOOD,
    WN_GT_EVENT,
    WN_GT_STREAM,   /* load everything around the player now */
    WN_GT_CAMERA,   /* fixed free camera (x y z yaw pitch) or back to follow */
    WN_GT_AUTOPILOT /* walk along the road (v0: 0 off, 1 walk, 2 run), v1: chop period in steps */
} wn_gt_kind;

typedef enum wn_gt_op { WN_OP_LT, WN_OP_LE, WN_OP_GT, WN_OP_GE, WN_OP_EQ, WN_OP_NE } wn_gt_op;

typedef struct wn_gt_cmd {
    uint32_t step;
    wn_gt_kind kind;
    int action;
    float v[5];
    char name[48];
    wn_gt_op op;
    int tolerance;
    float maxdiff; /* fraction of pixels allowed to differ */
    int line;
} wn_gt_cmd;

#define WN_GT_MAX_CMDS 256

typedef struct wn_gametest {
    char scene[32];
    uint64_t seed;
    int preset; /* wn_preset_id, -1 = default */
    char replay[128];
    uint32_t end_step;
    wn_gt_cmd cmds[WN_GT_MAX_CMDS];
    int ncmds;
    int next; /* next command to run */
    float held[WN_ACT_COUNT];
    uint32_t prev_down;
    int failures;
    char log[2048];
    int autopilot;      /* 0 off, 1 walk, 2 run */
    int chop_period;    /* > 0: press attack every N steps */
} wn_gametest;

bool wn_gametest_load(wn_gametest *t, const char *text, char *err, size_t errlen);
/* Input for the given step (scripted holds, autopilot). */
wn_input_frame wn_gametest_input(wn_gametest *t, uint32_t step);
/* Autopilot steering: points the camera along the road ahead of the player. */
void wn_gametest_steer(wn_gametest *t, wn_game *g);
/* Runs all non-capture commands scheduled at or before step. Capture commands
 * due at this step are copied to captures (up to max) for the platform. */
int wn_gametest_run(wn_gametest *t, wn_game *g, uint32_t step, wn_gt_cmd *captures, int max);
/* Value of a named game variable (player.x, complete, snow_depth, ...). */
bool wn_gametest_var(wn_game *g, const char *name, float *out);
void wn_gametest_fail(wn_gametest *t, const char *fmt, ...);

/* ---- Image comparison ---- */
typedef struct wn_image_diff {
    int differing;  /* pixels whose max channel difference exceeds tolerance */
    float fraction;
    int max_delta;
} wn_image_diff;

/* Compares two RGBA8 images. diff_out (optional, RGBA8) marks differing pixels red. */
wn_image_diff wn_image_compare(const uint8_t *a, const uint8_t *b, int w, int h, int tolerance, uint8_t *diff_out);
bool wn_png_write(const char *path, const uint8_t *rgba, int w, int h);
/* Loads an RGBA8 PNG (caller frees). */
uint8_t *wn_png_read(const char *path, int *w, int *h);

#endif
