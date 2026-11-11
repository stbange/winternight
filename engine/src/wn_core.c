#include "wn/wn_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XXH_INLINE_ALL
#include "xxhash.h"

/* ---------------- RNG ---------------- */

void wn_rng_seed(wn_rng *r, uint64_t seed, uint64_t stream)
{
    r->state = 0u;
    r->inc = (stream << 1u) | 1u;
    (void)wn_rng_u32(r);
    r->state += seed;
    (void)wn_rng_u32(r);
}

uint32_t wn_rng_u32(wn_rng *r)
{
    uint64_t old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

uint32_t wn_rng_range(wn_rng *r, uint32_t bound)
{
    if (bound == 0) return 0;
    uint32_t threshold = (0u - bound) % bound;
    for (;;) {
        uint32_t v = wn_rng_u32(r);
        if (v >= threshold) return v % bound;
    }
}

float wn_rng_f01(wn_rng *r) { return (float)(wn_rng_u32(r) >> 8) * (1.0f / 16777216.0f); }

float wn_rng_frange(wn_rng *r, float lo, float hi) { return lo + (hi - lo) * wn_rng_f01(r); }

/* ---------------- Hashing ---------------- */

uint32_t wn_hash32(const void *data, size_t len, uint32_t seed) { return XXH32(data, len, seed); }

void wn_hasher_init(wn_hasher *h, uint32_t seed)
{
    XXH32_state_t *s = XXH32_createState();
    XXH32_reset(s, seed);
    h->state = s;
}

void wn_hasher_add(wn_hasher *h, const void *data, size_t len) { XXH32_update((XXH32_state_t *)h->state, data, len); }

uint32_t wn_hasher_finish(wn_hasher *h)
{
    uint32_t v = XXH32_digest((XXH32_state_t *)h->state);
    XXH32_freeState((XXH32_state_t *)h->state);
    h->state = NULL;
    return v;
}

uint32_t wn_hash_ivec3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    uint32_t h = seed ^ 0x9E3779B9u;
    h ^= (uint32_t)x * 0x85EBCA6Bu;
    h = (h << 13) | (h >> 19);
    h ^= (uint32_t)y * 0xC2B2AE35u;
    h = (h << 17) | (h >> 15);
    h ^= (uint32_t)z * 0x27D4EB2Fu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

/* ---------------- Clock ---------------- */

void wn_clock_init(wn_clock *c)
{
    c->accumulator = 0.0;
    c->max_frame = 0.25;
    c->step = 0;
}

int wn_clock_advance(wn_clock *c, double dt)
{
    if (dt < 0.0) dt = 0.0;
    if (dt > c->max_frame) dt = c->max_frame;
    c->accumulator += dt;
    const double step = 1.0 / (double)WN_SIM_HZ;
    int n = 0;
    while (c->accumulator >= step) {
        c->accumulator -= step;
        n++;
    }
    c->step += (uint64_t)n;
    return n;
}

float wn_clock_alpha(const wn_clock *c) { return (float)(c->accumulator * (double)WN_SIM_HZ); }

/* ---------------- Logging ---------------- */

static wn_log_sink g_sink = NULL;
static void *g_sink_user = NULL;
static wn_log_level g_min_level = WN_LOG_INFO;
static char g_last_error[256];

void wn_log_set_sink(wn_log_sink sink, void *user)
{
    g_sink = sink;
    g_sink_user = user;
}

void wn_log_set_level(wn_log_level lvl) { g_min_level = lvl; }

const char *wn_log_last_error(void) { return g_last_error; }

void wn_log(wn_log_level level, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (level == WN_LOG_ERROR) {
        memcpy(g_last_error, buf, sizeof g_last_error - 1);
        g_last_error[sizeof g_last_error - 1] = '\0';
    }
    if (level < g_min_level) return;
    if (g_sink) {
        g_sink(level, buf, g_sink_user);
    } else {
        static const char *names[] = {"debug", "info", "warn", "error"};
        fprintf(stderr, "[%s] %s\n", names[level], buf);
    }
}

/* ---------------- Parser ---------------- */

void wn_parser_init(wn_parser *p, const char *src, size_t len)
{
    memset(p, 0, sizeof *p);
    p->src = src;
    p->len = len;
}

void wn_parser_fail(wn_parser *p, const char *msg)
{
    if (p->error[0]) return; /* keep the first error */
    snprintf(p->error, sizeof p->error, "line %d: %s", p->line_no, msg);
}

static bool split_fields(wn_parser *p)
{
    char *s = p->line;
    p->nfields = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\r') s++;
        /* '#' starts a comment at the start of a line or when followed by
         * whitespace; otherwise it is part of a value such as "#rrggbb". */
        if (!*s) break;
        if (*s == '#' && (p->nfields == 0 || s[1] == '\0' || s[1] == ' ' || s[1] == '\t' || s[1] == '\r')) break;
        if (p->nfields == WN_PARSE_MAX_FIELDS) {
            wn_parser_fail(p, "too many fields");
            return false;
        }
        if (*s == '"') {
            s++;
            p->fields[p->nfields++] = s;
            while (*s && *s != '"') s++;
            if (*s != '"') {
                wn_parser_fail(p, "unterminated quote");
                return false;
            }
            *s++ = '\0';
        } else {
            p->fields[p->nfields++] = s;
            while (*s && *s != ' ' && *s != '\t' && *s != '\r') s++;
            if (*s) *s++ = '\0';
        }
    }
    return true;
}

bool wn_parser_next(wn_parser *p)
{
    while (p->pos < p->len && !p->error[0]) {
        size_t start = p->pos;
        while (p->pos < p->len && p->src[p->pos] != '\n') p->pos++;
        size_t n = p->pos - start;
        if (p->pos < p->len) p->pos++; /* skip newline */
        p->line_no++;
        if (n >= WN_PARSE_MAX_LINE) {
            wn_parser_fail(p, "line too long");
            return false;
        }
        memcpy(p->line, p->src + start, n);
        p->line[n] = '\0';
        if (!split_fields(p)) return false;
        if (p->nfields > 0) return true;
    }
    p->nfields = 0;
    return false;
}

bool wn_parser_is(const wn_parser *p, int field, const char *text)
{
    return field < p->nfields && strcmp(p->fields[field], text) == 0;
}

bool wn_parser_expect_fields(wn_parser *p, int min_fields)
{
    if (p->nfields >= min_fields) return true;
    char msg[96];
    snprintf(msg, sizeof msg, "'%s' needs %d values", p->fields[0], min_fields - 1);
    wn_parser_fail(p, msg);
    return false;
}

bool wn_parser_int(wn_parser *p, int field, int32_t *out)
{
    if (field >= p->nfields) {
        wn_parser_fail(p, "missing integer");
        return false;
    }
    char *end = NULL;
    long v = strtol(p->fields[field], &end, 10);
    if (end == p->fields[field] || *end) {
        char msg[96];
        snprintf(msg, sizeof msg, "'%.40s' is not an integer", p->fields[field]);
        wn_parser_fail(p, msg);
        return false;
    }
    *out = (int32_t)v;
    return true;
}

bool wn_parser_float(wn_parser *p, int field, float *out)
{
    if (field >= p->nfields) {
        wn_parser_fail(p, "missing number");
        return false;
    }
    char *end = NULL;
    float v = strtof(p->fields[field], &end);
    if (end == p->fields[field] || *end) {
        char msg[96];
        snprintf(msg, sizeof msg, "'%.40s' is not a number", p->fields[field]);
        wn_parser_fail(p, msg);
        return false;
    }
    *out = v;
    return true;
}

bool wn_parser_color(wn_parser *p, int field, uint32_t *rgb)
{
    if (field >= p->nfields) {
        wn_parser_fail(p, "missing colour");
        return false;
    }
    const char *s = p->fields[field];
    if (s[0] != '#' || strlen(s) != 7) {
        wn_parser_fail(p, "colour must look like #rrggbb");
        return false;
    }
    char *end = NULL;
    unsigned long v = strtoul(s + 1, &end, 16);
    if (*end) {
        wn_parser_fail(p, "colour must look like #rrggbb");
        return false;
    }
    *rgb = (uint32_t)v;
    return true;
}

char *wn_read_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}
