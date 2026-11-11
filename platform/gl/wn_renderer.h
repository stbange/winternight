/* Winternight platform - OpenGL renderer: draws the frame described by the
 * core (view columns, far scenery, models, debris, particles, overlay) with
 * the retro pipeline of design D4. */
#ifndef WN_RENDERER_H
#define WN_RENDERER_H

#include "wn/wn_edit.h"
#include "wn/wn_fx.h"
#include "wn/wn_model.h"
#include "wn/wn_overlay.h"
#include "wn/wn_render.h"
#include "wn/wn_scene.h"
#include "wn/wn_view.h"

typedef struct wn_renderer wn_renderer;

typedef struct wn_frame_desc {
    wn_mat4 view, proj;
    wn_vec3 eye;
    const wn_atmos *atmos;
    const wn_quality *quality;
    wn_view *view_cache; /* world columns */
    wn_world *world;
    wn_vec3 viewer;      /* centre of the loaded square (player) */
    int radius;
    wn_model *models[4];
    int nmodels;
    wn_debris_set *debris;
    const wn_fx *fx;
    wn_overlay *overlay;
    bool xr;             /* stereo eye: no vertex snapping */
    float vignette;      /* comfort vignette strength 0..1 */
    /* output */
    unsigned target_fbo;
    int out_x, out_y, out_w, out_h; /* destination viewport (window or eye) */
    int win_w, win_h;               /* full target size for clearing */
    bool clear_target;
} wn_frame_desc;

typedef struct wn_render_stats {
    uint32_t draw_calls, triangles;
} wn_render_stats;

wn_renderer *wn_renderer_create(const wn_materials *mats, const wn_palette *pal, char *err, size_t errlen);
void wn_renderer_destroy(wn_renderer *r);
/* Builds the far-scenery mesh (whole scene at coarse resolution). */
void wn_renderer_set_far(wn_renderer *r, const wn_far_grid *g, const wn_materials *mats);
void wn_renderer_draw(wn_renderer *r, const wn_frame_desc *f, wn_render_stats *stats);
/* Reads the last final internal-resolution image (RGBA8, top row first). */
bool wn_renderer_capture(wn_renderer *r, uint8_t **rgba, int *w, int *h);
void wn_renderer_internal_size(const wn_renderer *r, int *w, int *h);

#endif
