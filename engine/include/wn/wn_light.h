/* Winternight engine - flood-fill sky and block light (spec night-lighting,
 * design D5). Light is stored in the voxel cells. */
#ifndef WN_LIGHT_H
#define WN_LIGHT_H

#include "wn_voxel.h"

typedef enum wn_light_channel { WN_LIGHT_SKY = 0, WN_LIGHT_BLOCK = 1 } wn_light_channel;

/* Computes light for a freshly loaded column and exchanges light with the
 * already-loaded neighbour columns. Suitable as wn_world.on_column. */
void wn_light_column_loaded(void *user, wn_world *w, int cx, int cz);

/* Re-lights the world after the voxel at (x,y,z) has been replaced. Call after
 * the new shape has been written. */
void wn_light_voxel_changed(wn_world *w, int32_t x, int32_t y, int32_t z);

/* Light value accessor helpers. */
uint32_t wn_light_get(wn_world *w, wn_light_channel ch, int32_t x, int32_t y, int32_t z);

/* Number of voxels touched by the last light operation (profiling/tests). */
uint32_t wn_light_last_work(void);

#endif
