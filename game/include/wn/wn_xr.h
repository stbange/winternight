/* Winternight game - WebXR play rules that do not depend on the browser
 * (spec webxr-mode): swing attacks, turning and the comfort vignette. */
#ifndef WN_XR_H
#define WN_XR_H

#include "wn/wn_base.h"

#define WN_XR_SWING_THRESHOLD 2.2f /* m/s of the weapon hand */

/* Damage factor of a controller swing (0 below the threshold, grows with speed). */
float wn_xr_swing_damage(float speed);

typedef struct wn_xr_turn {
    bool snap;          /* snap turning instead of smooth */
    float snap_angle;   /* radians per snap */
    float smooth_speed; /* radians per second */
    bool armed;         /* stick returned to centre since the last snap */
} wn_xr_turn;

void wn_xr_turn_init(wn_xr_turn *t, bool snap);
/* Returns the yaw change for this step from the turn stick (-1..1). */
float wn_xr_turn_step(wn_xr_turn *t, float stick_x, float dt);
/* Comfort vignette strength for the current movement (0 when still). */
float wn_xr_vignette(bool enabled, float move_amount, float turn_rate);

#endif
