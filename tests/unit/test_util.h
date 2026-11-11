/* Shared helpers for unit tests. */
#ifndef WN_TEST_UTIL_H
#define WN_TEST_UTIL_H

#include "wn/wn_voxel.h"

#include <stdio.h>
#include <stdlib.h>

/* Minimal material table that defines every built-in material. */
static const char *k_test_materials =
    "material bedrock solid 1000 3 0 a #202020\n"
    "material earth solid 6 1.6 0 - #3a2e28 #4a3a30\n"
    "material stone solid 12 2.5 0 - #454555 #6a6a7a\n"
    "material snow granular 1 0.3 0 - #c9d6f2 #9fb0d8 #b8c6e8\n"
    "material ice solid 8 0.9 0 - #9fd0f0\n"
    "material water liquid 0 1 0 - #20406a\n"
    "material blood liquid 0 1 0 - #8c1016\n"
    "material frozen-blood solid 2 1 0 - #5a0a0c\n"
    "material pine-wood solid 4 0.6 0 f #3a2618 #2e1e12\n"
    "material pine-needles solid 1.5 0.2 0 f #173328 #0e1f1a #24493a\n"
    "material castle-stone solid 14 2.6 0 - #2a2a35 #454555\n"
    "material window-glow solid 6 1 14 - #ffb050\n"
    "material fire gas 0 0 15 - #ff6a1a\n"
    "material bone solid 3 1.5 0 - #d8cfb4 #a99c7a\n";

static inline wn_materials *test_materials(void)
{
    static wn_materials mats;
    static int loaded = 0;
    if (!loaded) {
        char err[256];
        if (!wn_materials_load(&mats, k_test_materials, err, sizeof err)) {
            fprintf(stderr, "test materials failed: %s\n", err);
            abort();
        }
        loaded = 1;
    }
    return &mats;
}

/* Generator: everything below y=0 is stone, air above. */
static inline void test_gen_flat(void *user, wn_chunk *c)
{
    (void)user;
    if (c->pos.y < 0) c->uniform = wn_vox_make(WN_MAT_STONE, 0);
    else c->uniform = wn_vox_set_sky(0, 15);
}

#endif
