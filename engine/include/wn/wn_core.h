/* Winternight engine - core utilities: RNG, hashing, fixed-step clock, logging,
 * and the line-based text data parser used by every data file. */
#ifndef WN_CORE_H
#define WN_CORE_H

#include "wn_base.h"

/* ---- PCG32 random streams (design D7) ---- */
typedef struct wn_rng {
    uint64_t state;
    uint64_t inc;
} wn_rng;

void wn_rng_seed(wn_rng *r, uint64_t seed, uint64_t stream);
uint32_t wn_rng_u32(wn_rng *r);
/* Uniform integer in [0, bound). Returns 0 for bound 0. */
uint32_t wn_rng_range(wn_rng *r, uint32_t bound);
/* Uniform float in [0, 1). */
float wn_rng_f01(wn_rng *r);
/* Uniform float in [lo, hi). */
float wn_rng_frange(wn_rng *r, float lo, float hi);

/* ---- Hashing ---- */
uint32_t wn_hash32(const void *data, size_t len, uint32_t seed);
typedef struct wn_hasher {
    void *state; /* XXH32_state_t, opaque to callers */
} wn_hasher;
void wn_hasher_init(wn_hasher *h, uint32_t seed);
void wn_hasher_add(wn_hasher *h, const void *data, size_t len);
uint32_t wn_hasher_finish(wn_hasher *h); /* also frees the state */
/* Stable integer hash of a 3D position (colour variation, noise). */
uint32_t wn_hash_ivec3(int32_t x, int32_t y, int32_t z, uint32_t seed);

/* ---- Fixed-step clock (spec platform-targets: Fixed simulation step) ---- */
typedef struct wn_clock {
    double accumulator; /* seconds not yet simulated */
    double max_frame;   /* clamp of a single frame delta (spiral-of-death guard) */
    uint64_t step;      /* number of simulation steps executed */
} wn_clock;

void wn_clock_init(wn_clock *c);
/* Adds real time and returns how many fixed steps must be simulated now. */
int wn_clock_advance(wn_clock *c, double dt_seconds);
/* Interpolation factor between the last two simulation states, in [0,1). */
float wn_clock_alpha(const wn_clock *c);

/* ---- Logging ---- */
typedef enum wn_log_level { WN_LOG_DEBUG, WN_LOG_INFO, WN_LOG_WARN, WN_LOG_ERROR } wn_log_level;
typedef void (*wn_log_sink)(wn_log_level level, const char *msg, void *user);
void wn_log_set_sink(wn_log_sink sink, void *user);
void wn_log_set_level(wn_log_level min_level);
void wn_log(wn_log_level level, const char *fmt, ...);
/* The most recent error message (for start-up failure reporting). */
const char *wn_log_last_error(void);

/* ---- Text data parser ----
 * Format: one record per line, fields separated by whitespace, '#' starts a
 * comment, double quotes group a field containing spaces. */
#define WN_PARSE_MAX_FIELDS 24
#define WN_PARSE_MAX_LINE 512

typedef struct wn_parser {
    const char *src;
    size_t len;
    size_t pos;
    int line_no;
    char line[WN_PARSE_MAX_LINE];
    char *fields[WN_PARSE_MAX_FIELDS];
    int nfields;
    char error[160];
} wn_parser;

void wn_parser_init(wn_parser *p, const char *src, size_t len);
/* Advances to the next non-empty record. Returns false at end of input or on
 * error (check p->error[0]). */
bool wn_parser_next(wn_parser *p);
bool wn_parser_is(const wn_parser *p, int field, const char *text);
/* Field conversions; on failure they record an error and return false. */
bool wn_parser_int(wn_parser *p, int field, int32_t *out);
bool wn_parser_float(wn_parser *p, int field, float *out);
bool wn_parser_color(wn_parser *p, int field, uint32_t *rgb); /* "#rrggbb" */
bool wn_parser_expect_fields(wn_parser *p, int min_fields);
void wn_parser_fail(wn_parser *p, const char *msg);

/* Reads a whole file into a NUL-terminated heap buffer. */
char *wn_read_file(const char *path, size_t *out_len);

#endif
