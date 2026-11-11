/* Winternight game - the playable simulation: world, player, camera, combat,
 * snow, blood, debris, triggers (spec game-flow). Pure C, deterministic. */
#ifndef WN_GAME_H
#define WN_GAME_H

#include "wn/wn_edit.h"
#include "wn/wn_fx.h"
#include "wn/wn_input.h"
#include "wn/wn_model.h"
#include "wn/wn_render.h"
#include "wn/wn_scene.h"
#include "wn/wn_snow.h"

typedef struct wn_player {
    wn_vec3 pos;  /* feet centre */
    wn_vec3 vel;
    float yaw;    /* facing; 0 = +z */
    bool on_ground;
    float walk_phase;
    float step_dist; /* distance since the last footprint */
    int foot;        /* alternates left/right */
    float swing;     /* > 0 while attacking: time since the swing started */
    int swing_count;
    bool swing_hit_done;
    bool blocking;
    float brush_cooldown;
    float health;
    float swing_power; /* damage factor of the current swing (XR: swing speed) */
} wn_player;

typedef struct wn_cam_rig {
    float yaw, pitch;
    float distance;    /* desired distance behind the shoulder */
    float shoulder;    /* sideways offset */
    float actual_dist; /* after collision */
    bool first_person; /* XR */
    bool fixed;        /* free camera (tests, cut-scenes): uses fixed_pos and yaw/pitch */
    wn_vec3 fixed_pos;
} wn_cam_rig;

#define WN_MAX_MESSAGES 4

typedef struct wn_game {
    wn_materials mats;
    wn_prefab_lib prefabs;
    wn_scene scene;
    wn_world world;
    wn_debris_set debris;
    wn_fx fx;
    wn_snow snow;
    wn_atmos atmos;
    wn_player player;
    wn_player prev_player; /* for render interpolation */
    wn_cam_rig cam;
    wn_model warrior;
    wn_rng rng;
    uint64_t step;
    int view_radius;
    bool complete;
    uint32_t events_fired;
    char message[64];
    float message_time;
    float melt_accum[4];
    bool loaded;
} wn_game;

/* Loads materials, prefabs and the scene from the data directory (text
 * contents are passed in so tests and the web build can supply them). */
bool wn_game_init(wn_game *g, const char *materials_txt, const char *prefabs_txt, const char *scene_txt, uint64_t seed,
                  char *err, size_t errlen);
/* Convenience: reads the files from data_dir and calls wn_game_init. */
bool wn_game_init_from_dir(wn_game *g, const char *data_dir, const char *scene_name, uint64_t seed, char *err,
                           size_t errlen);
void wn_game_free(wn_game *g);

/* Streams the world around the player. max_columns limits work per call. */
int wn_game_stream(wn_game *g, int max_columns);
/* Loads everything within the view radius (start-up, tests). */
void wn_game_stream_all(wn_game *g);

/* One fixed simulation step. */
void wn_game_step(wn_game *g, const wn_input_frame *in);
uint32_t wn_game_checksum(wn_game *g);

/* Camera for rendering with interpolation factor alpha in [0,1]. */
wn_camera wn_game_camera(const wn_game *g, float alpha);
/* Poses the warrior model for rendering (alpha-interpolated). */
void wn_game_pose_warrior(wn_game *g, float alpha);

/* Builds the voxel model of the warrior (fur cloak, sword). */
void wn_build_warrior(wn_model *m, const wn_materials *mats);

/* Fires an event by name (also used by triggers). Returns actions run. */
int wn_game_fire_event(wn_game *g, const char *event);

/* XR: a tracked swing of the weapon hand at the given speed (m/s). Starts an
 * attack whose damage follows the speed (spec webxr-mode: Swing attack).
 * Returns false when the swing is too slow. */
bool wn_game_xr_swing(wn_game *g, float speed);

/* Test / debug helpers. */
void wn_game_teleport(wn_game *g, wn_vec3 pos, float yaw);
void wn_game_spill_blood(wn_game *g, wn_vec3 pos, float amount);

/* Player constants. */
#define WN_PLAYER_RADIUS 0.28f
#define WN_PLAYER_HEIGHT 1.8f
#define WN_PLAYER_WALK 2.4f
#define WN_PLAYER_RUN 4.4f
#define WN_PLAYER_JUMP 3.1f /* initial speed: about 0.5 m high */
#define WN_WADE_DEPTH 0.55f /* snow deeper than this carries the player */

#endif
