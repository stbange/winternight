/* Winternight engine - 2D overlay drawn at the internal resolution: touch
 * controls, messages and the frame-time overlay. Drawn on the CPU into an
 * RGBA buffer so it is identical on every target and part of golden images. */
#ifndef WN_OVERLAY_H
#define WN_OVERLAY_H

#include "wn_input.h"

typedef struct wn_overlay {
    uint8_t *rgba;
    int w, h;
    uint32_t hash; /* content hash of the last draw (skip uploads when equal) */
    bool changed;
} wn_overlay;

void wn_overlay_init(wn_overlay *o, int w, int h);
void wn_overlay_free(wn_overlay *o);
void wn_overlay_clear(wn_overlay *o);
/* Draws a filled or ring circle (pixels), colour 0xRRGGBB with alpha. */
void wn_overlay_circle(wn_overlay *o, float cx, float cy, float r, float thickness, uint32_t rgb, uint8_t alpha);
/* Draws text with a 1-pixel dark outline; returns the width. */
int wn_overlay_text(wn_overlay *o, int x, int y, const char *text, uint32_t rgb);
/* Draws the touch controls if visible. */
void wn_overlay_touch(wn_overlay *o, const wn_touch *t);
/* Finishes a frame: updates hash/changed. */
void wn_overlay_finish(wn_overlay *o);

#endif
