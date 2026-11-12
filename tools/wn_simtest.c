/* Winternight - headless simulation test runner. Runs a game-test script
 * through the core simulation without a window or GPU (captures are skipped).
 * Used for cross-platform determinism checks on CI machines without OpenGL
 * (spec platform-targets: Cross-platform determinism). */
#include "wn/wn_core.h"
#include "wn/wn_game.h"
#include "wn/wn_gametest.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef WN_DATA_DIR
#define WN_DATA_DIR "game/data"
#endif

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: wn_simtest SCRIPT [DATA_DIR]\n");
        return 2;
    }
    const char *data = argc > 2 ? argv[2] : WN_DATA_DIR;
    char err[512];
    char *txt = wn_read_file(argv[1], NULL);
    static wn_gametest t;
    if (!txt || !wn_gametest_load(&t, txt, err, sizeof err)) {
        fprintf(stderr, "cannot load %s: %s\n", argv[1], txt ? err : "file not found");
        free(txt);
        return 2;
    }
    free(txt);
    static wn_game g;
    if (!wn_game_init_from_dir(&g, data, t.scene, t.seed, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 2;
    }
    wn_quality q = wn_quality_preset(t.preset >= 0 ? (wn_preset_id)t.preset : WN_PRESET_MOBILE);
    g.view_radius = q.view_radius;
    g.fx.capacity = q.particles;
    wn_game_stream_all(&g);
    for (;;) {
        uint32_t step = (uint32_t)g.step;
        wn_gt_cmd caps[8];
        wn_gametest_run(&t, &g, step, caps, 8); /* captures need a renderer: skipped */
        if (step >= t.end_step) break;
        wn_gametest_steer(&t, &g);
        wn_input_frame f = wn_gametest_input(&t, step);
        wn_game_step(&g, &f);
        wn_game_stream(&g, 1 << 20);
    }
    int rc = 0;
    if (t.failures) {
        fprintf(stderr, "SIM TEST FAILED (%d):\n%s", t.failures, t.log);
        rc = 1;
    } else {
        printf("SIM TEST PASSED (%llu steps, checksum %08x)\n", (unsigned long long)g.step, wn_game_checksum(&g));
    }
    wn_game_free(&g);
    return rc;
}
