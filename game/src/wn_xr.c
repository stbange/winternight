#include "wn/wn_xr.h"

float wn_xr_swing_damage(float speed)
{
    if (speed < WN_XR_SWING_THRESHOLD) return 0.0f;
    float f = 0.6f + (speed - WN_XR_SWING_THRESHOLD) * 0.35f;
    return WN_MIN(f, 2.5f);
}

void wn_xr_turn_init(wn_xr_turn *t, bool snap)
{
    t->snap = snap;
    t->snap_angle = WN_PI / 6.0f;
    t->smooth_speed = 2.0f;
    t->armed = true;
}

float wn_xr_turn_step(wn_xr_turn *t, float stick_x, float dt)
{
    float a = wn_absf(stick_x);
    if (!t->snap) return a < 0.15f ? 0.0f : stick_x * t->smooth_speed * dt;
    if (a < 0.3f) {
        t->armed = true;
        return 0.0f;
    }
    if (a > 0.7f && t->armed) {
        t->armed = false;
        return stick_x > 0.0f ? t->snap_angle : -t->snap_angle;
    }
    return 0.0f;
}

float wn_xr_vignette(bool enabled, float move_amount, float turn_rate)
{
    if (!enabled) return 0.0f;
    float v = WN_CLAMP(move_amount, 0.0f, 1.0f) * 0.55f + WN_CLAMP(wn_absf(turn_rate) / 2.0f, 0.0f, 1.0f) * 0.45f;
    return WN_CLAMP(v, 0.0f, 0.8f);
}
