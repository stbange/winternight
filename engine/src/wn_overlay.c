#include "wn/wn_overlay.h"

#include "wn/wn_core.h"
#include "wn/wn_font.h"

#include <stdlib.h>
#include <string.h>

void wn_overlay_init(wn_overlay *o, int w, int h)
{
    memset(o, 0, sizeof *o);
    o->w = w;
    o->h = h;
    o->rgba = (uint8_t *)calloc((size_t)w * h * 4, 1);
    o->changed = true;
}

void wn_overlay_free(wn_overlay *o)
{
    free(o->rgba);
    memset(o, 0, sizeof *o);
}

void wn_overlay_clear(wn_overlay *o) { memset(o->rgba, 0, (size_t)o->w * o->h * 4); }

static void blend(wn_overlay *o, int x, int y, uint32_t rgb, uint8_t a)
{
    if (x < 0 || y < 0 || x >= o->w || y >= o->h) return;
    uint8_t *p = &o->rgba[((size_t)y * o->w + x) * 4];
    if (a >= p[3]) {
        p[0] = (uint8_t)(rgb >> 16);
        p[1] = (uint8_t)(rgb >> 8);
        p[2] = (uint8_t)rgb;
        p[3] = a;
    }
}

void wn_overlay_circle(wn_overlay *o, float cx, float cy, float r, float thickness, uint32_t rgb, uint8_t alpha)
{
    int x0 = (int)(cx - r - 1), x1 = (int)(cx + r + 1), y0 = (int)(cy - r - 1), y1 = (int)(cy + r + 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            float d = wn_sqrtf(dx * dx + dy * dy);
            if (d > r) continue;
            if (thickness > 0.0f && d < r - thickness) continue;
            blend(o, x, y, rgb, alpha);
        }
}

int wn_overlay_text(wn_overlay *o, int x, int y, const char *text, uint32_t rgb)
{
    int tw = wn_font_width(text);
    uint8_t *mask = (uint8_t *)calloc((size_t)(tw + 2) * (WN_FONT_H + 2), 1);
    wn_font_draw(mask, tw + 2, WN_FONT_H + 2, 1, 1, text);
    int mw = tw + 2, mh = WN_FONT_H + 2;
    for (int py = 0; py < mh; py++)
        for (int px = 0; px < mw; px++) {
            if (mask[py * mw + px]) {
                blend(o, x + px - 1, y + py - 1, rgb, 255);
                continue;
            }
            bool near = false;
            for (int dy = -1; dy <= 1 && !near; dy++)
                for (int dx = -1; dx <= 1 && !near; dx++) {
                    int qx = px + dx, qy = py + dy;
                    near = qx >= 0 && qy >= 0 && qx < mw && qy < mh && mask[qy * mw + qx];
                }
            if (near) blend(o, x + px - 1, y + py - 1, 0x05070F, 200);
        }
    free(mask);
    return tw;
}

void wn_overlay_touch(wn_overlay *o, const wn_touch *t)
{
    if (!t->visible) return;
    float s = (float)o->h; /* touch coordinates use screen height = 1 */
    /* Move stick: the base appears where the thumb went down, or a hint. */
    bool stick = false;
    for (int i = 0; i < WN_TOUCH_MAX_FINGERS; i++) {
        if (t->fingers[i].role != WN_FINGER_STICK) continue;
        stick = true;
        float bx = t->fingers[i].ox * s, by = t->fingers[i].oy * s;
        float sx, sy;
        wn_touch_stick_value(t->fingers[i].x - t->fingers[i].ox, t->fingers[i].y - t->fingers[i].oy, t->stick_radius, &sx, &sy);
        wn_overlay_circle(o, bx, by, t->stick_radius * s, 2.0f, 0xC9D6F2, 150);
        wn_overlay_circle(o, bx + sx * t->stick_radius * s, by + sy * t->stick_radius * s, t->stick_radius * s * 0.45f, 0.0f,
                          0xC9D6F2, 170);
    }
    if (!stick) wn_overlay_circle(o, 0.22f * s, 0.75f * s, t->stick_radius * s, 2.0f, 0x9FB0D8, 90);
    static const uint32_t colors[WN_TOUCH_BUTTONS] = {0xFFB050, 0x9FB0D8, 0xC9D6F2, 0x7DFFC0};
    static const char *labels[WN_TOUCH_BUTTONS] = {"ATK", "BLK", "JMP", "USE"};
    for (int b = 0; b < WN_TOUCH_BUTTONS; b++) {
        const wn_touch_button *btn = &t->buttons[b];
        bool held = false;
        for (int i = 0; i < WN_TOUCH_MAX_FINGERS; i++)
            held |= t->fingers[i].role == WN_FINGER_BUTTON && t->fingers[i].button == b;
        wn_overlay_circle(o, btn->x * s, btn->y * s, btn->r * s, held ? 0.0f : 2.0f, colors[b], held ? 170 : 130);
        int tw = wn_font_width(labels[b]);
        wn_overlay_text(o, (int)(btn->x * s) - tw / 2, (int)(btn->y * s) - 3, labels[b], colors[b]);
    }
}

void wn_overlay_finish(wn_overlay *o)
{
    uint32_t h = wn_hash32(o->rgba, (size_t)o->w * o->h * 4, 5u);
    o->changed = h != o->hash;
    o->hash = h;
}
