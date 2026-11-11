/* Winternight engine - tiny 5x7 pixel font for HUD text and the frame-time
 * overlay (rendered into an alpha bitmap, drawn by the backend). */
#ifndef WN_FONT_INCLUDED
#define WN_FONT_INCLUDED

#include "wn_base.h"

#define WN_FONT_W 5
#define WN_FONT_H 7
#define WN_FONT_ADVANCE 6

/* Width in pixels of a text line. */
int wn_font_width(const char *text);
/* Rasterises text into an 8-bit alpha buffer (w x h, row-major, top row
 * first) at pixel (x, y). Unknown characters render as a box. */
void wn_font_draw(uint8_t *buf, int w, int h, int x, int y, const char *text);

#endif
