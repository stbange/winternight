/* Winternight engine - voxel cells, materials, chunks and the world grid
 * (spec voxel-world, design D2). */
#ifndef WN_VOXEL_H
#define WN_VOXEL_H

#include "wn_base.h"

/* ---- 32-bit voxel cell ----
 * bits  0-7  material   bits  8-11 fill (0 = full)   bits 12-15 damage
 * bits 16-17 stain type bits 18-23 stain amount
 * bits 24-27 sky light  bits 28-31 block light */
typedef uint32_t wn_voxel;

typedef enum wn_stain {
    WN_STAIN_NONE = 0,
    WN_STAIN_BLOOD = 1,       /* fresh red */
    WN_STAIN_BLOOD_DRIED = 2, /* dark, aged */
    WN_STAIN_ICHOR = 3        /* undead / soot */
} wn_stain;

static inline uint32_t wn_vox_mat(wn_voxel v) { return v & 0xFFu; }
static inline uint32_t wn_vox_fill(wn_voxel v) { return (v >> 8) & 0xFu; }
static inline uint32_t wn_vox_damage(wn_voxel v) { return (v >> 12) & 0xFu; }
static inline uint32_t wn_vox_stain(wn_voxel v) { return (v >> 16) & 0x3u; }
static inline uint32_t wn_vox_stain_amount(wn_voxel v) { return (v >> 18) & 0x3Fu; }
static inline uint32_t wn_vox_sky(wn_voxel v) { return (v >> 24) & 0xFu; }
static inline uint32_t wn_vox_block(wn_voxel v) { return (v >> 28) & 0xFu; }

static inline wn_voxel wn_vox_set_mat(wn_voxel v, uint32_t m) { return (v & ~0xFFu) | (m & 0xFFu); }
static inline wn_voxel wn_vox_set_fill(wn_voxel v, uint32_t f) { return (v & ~0xF00u) | ((f & 0xFu) << 8); }
static inline wn_voxel wn_vox_set_damage(wn_voxel v, uint32_t d) { return (v & ~0xF000u) | ((d & 0xFu) << 12); }
static inline wn_voxel wn_vox_set_stain(wn_voxel v, uint32_t type, uint32_t amount)
{
    return (v & ~0xFF0000u) | ((type & 0x3u) << 16) | ((amount & 0x3Fu) << 18);
}
static inline wn_voxel wn_vox_set_sky(wn_voxel v, uint32_t s) { return (v & ~0x0F000000u) | ((s & 0xFu) << 24); }
static inline wn_voxel wn_vox_set_block(wn_voxel v, uint32_t b) { return (v & ~0xF0000000u) | ((b & 0xFu) << 28); }
/* Material + fill only (the "shape" of a voxel, ignoring light/stain/damage). */
static inline wn_voxel wn_vox_make(uint32_t mat, uint32_t fill) { return (mat & 0xFFu) | ((fill & 0xFu) << 8); }
/* Height of a voxel's top surface in 1/16 voxel units (16 = full). */
static inline uint32_t wn_vox_height16(wn_voxel v) { uint32_t f = wn_vox_fill(v); return f == 0 ? 16u : f; }

/* ---- Materials ---- */

typedef enum wn_mat_kind { WN_KIND_GAS, WN_KIND_SOLID, WN_KIND_GRANULAR, WN_KIND_LIQUID } wn_mat_kind;

/* Materials the engine itself refers to; the data file defines their looks. */
enum {
    WN_MAT_AIR = 0,
    WN_MAT_BEDROCK,
    WN_MAT_EARTH,
    WN_MAT_STONE,
    WN_MAT_SNOW,
    WN_MAT_ICE,
    WN_MAT_WATER,
    WN_MAT_BLOOD,
    WN_MAT_FROZEN_BLOOD,
    WN_MAT_PINE_WOOD,
    WN_MAT_PINE_NEEDLES,
    WN_MAT_CASTLE_STONE,
    WN_MAT_WINDOW_GLOW,
    WN_MAT_FIRE,
    WN_MAT_BUILTIN_COUNT
};

#define WN_MAX_MATERIALS 256
#define WN_RAMP_MAX 4

typedef struct wn_material {
    char name[24];
    wn_mat_kind kind;
    float hardness;   /* hit points of one voxel */
    float density;    /* kg per voxel-litre, used by debris mass */
    bool flammable;
    uint8_t emit;     /* block light level 0-15 */
    bool anchor;      /* never falls (bedrock, foundations) */
    uint32_t ramp[WN_RAMP_MAX];
    int ramp_n;
    bool defined;
} wn_material;

typedef struct wn_materials {
    wn_material m[WN_MAX_MATERIALS];
    int count; /* highest defined id + 1 */
} wn_materials;

/* Loads the material table. Format per line:
 *   material <name> <solid|granular|liquid|gas> <hardness> <density> <emit> <flags> #rrggbb [#rrggbb...]
 * flags: '-' or a combination of 'f' (flammable) and 'a' (anchor).
 * Built-in names map to their fixed ids; other names get the next free id.
 * On error the output table is left untouched and false is returned. */
bool wn_materials_load(wn_materials *out, const char *text, char *err, size_t errlen);
int wn_materials_find(const wn_materials *mats, const char *name);
const char *wn_material_builtin_name(int id);
static inline bool wn_mat_is_solid(const wn_materials *t, uint32_t m)
{
    return t->m[m].kind == WN_KIND_SOLID || t->m[m].kind == WN_KIND_GRANULAR;
}
/* Blocks light and sky (solid or granular). */
static inline bool wn_mat_is_opaque(const wn_materials *t, uint32_t m) { return wn_mat_is_solid(t, m); }

/* ---- Chunks ---- */

enum {
    WN_CHUNK_DIRTY_MESH = 1u << 0,
    WN_CHUNK_MODIFIED = 1u << 1, /* changed after generation: must be kept */
};

typedef struct wn_chunk {
    wn_ivec3 pos;
    wn_voxel uniform; /* value of every voxel when vox == NULL */
    wn_voxel *vox;    /* WN_CHUNK_VOLUME cells or NULL when uniform */
    uint32_t flags;
    uint32_t mesh_rev; /* incremented on every change that needs remeshing */
    struct wn_chunk *next;
} wn_chunk;

static inline int wn_chunk_index(int x, int y, int z) { return (y * WN_CHUNK_SIZE + z) * WN_CHUNK_SIZE + x; }
wn_chunk *wn_chunk_new(wn_ivec3 pos, wn_voxel fill);
void wn_chunk_free(wn_chunk *c);
static inline wn_voxel wn_chunk_get(const wn_chunk *c, int x, int y, int z)
{
    return c->vox ? c->vox[wn_chunk_index(x, y, z)] : c->uniform;
}
void wn_chunk_set(wn_chunk *c, int x, int y, int z, wn_voxel v);
void wn_chunk_expand(wn_chunk *c);
/* Returns true when the chunk was turned back into the compact form. */
bool wn_chunk_compact(wn_chunk *c);
bool wn_chunk_is_uniform(const wn_chunk *c);

/* ---- World ---- */

typedef struct wn_world wn_world;
/* Fills a freshly created chunk (generation). */
typedef void (*wn_chunk_gen_fn)(void *user, wn_chunk *chunk);
/* Called after a chunk column has been loaded (lighting etc). */
typedef void (*wn_column_loaded_fn)(void *user, wn_world *w, int cx, int cz);

struct wn_world {
    wn_chunk **buckets;
    uint32_t nbuckets;
    uint32_t nchunks;
    /* Chunks changed by the player that were streamed out (spec: Player
     * changes survive unloading). Same hashing scheme. */
    wn_chunk **saved;
    uint32_t nsaved;
    const wn_materials *mats;
    wn_chunk_gen_fn gen;
    void *gen_user;
    wn_column_loaded_fn on_column;
    void *column_user;
    int y_min, y_max; /* vertical chunk range (inclusive) */
    wn_chunk *cache;  /* last looked-up chunk */
    uint32_t edit_serial;
};

void wn_world_init(wn_world *w, const wn_materials *mats, int y_min, int y_max);
void wn_world_free(wn_world *w);
void wn_world_set_generator(wn_world *w, wn_chunk_gen_fn gen, void *user);

/* Coordinate helpers. */
wn_ivec3 wn_world_to_voxel(wn_vec3 p);
wn_vec3 wn_voxel_to_world(wn_ivec3 v); /* corner of the voxel */
wn_vec3 wn_voxel_center(wn_ivec3 v);
static inline wn_ivec3 wn_voxel_to_chunk(wn_ivec3 v)
{
    return wn_iv3(v.x >> WN_CHUNK_BITS, v.y >> WN_CHUNK_BITS, v.z >> WN_CHUNK_BITS);
}

wn_chunk *wn_world_chunk(wn_world *w, wn_ivec3 cpos);
/* Creates (generates or restores) the chunk if missing. */
wn_chunk *wn_world_load_chunk(wn_world *w, wn_ivec3 cpos);
void wn_world_unload_chunk(wn_world *w, wn_ivec3 cpos);
/* Loads every chunk of a column (y_min..y_max) and calls on_column. */
void wn_world_load_column(wn_world *w, int cx, int cz);
bool wn_world_column_loaded(wn_world *w, int cx, int cz);

/* Voxel access. Reads from unloaded chunks return air with full sky light. */
wn_voxel wn_world_get(wn_world *w, int32_t x, int32_t y, int32_t z);
/* Gameplay write: replaces material/fill/damage/stain but keeps the voxel's
 * current light bits, marks the chunk (and touching neighbours) for remeshing
 * and as modified. Call wn_light_voxel_changed afterwards (the edit API does). */
void wn_world_set(wn_world *w, int32_t x, int32_t y, int32_t z, wn_voxel v);
/* Full write including light bits, not marked as modified (lighting, generation). */
void wn_world_set_quiet(wn_world *w, int32_t x, int32_t y, int32_t z, wn_voxel v);
bool wn_world_is_solid(wn_world *w, int32_t x, int32_t y, int32_t z);

/* Streaming (spec voxel-world: Streaming around the viewer). Loads at most
 * max_loads columns per call, nearest first, and unloads columns further than
 * radius + 1. Returns the number of columns loaded. */
int wn_world_stream(wn_world *w, wn_vec3 viewer, int radius, int max_loads);

/* Checksum of all loaded and saved chunks (shape + stain; light excluded). */
uint32_t wn_world_checksum(wn_world *w);

/* ---- Queries ---- */

typedef struct wn_ray_hit {
    bool hit;
    wn_ivec3 voxel;
    wn_ivec3 normal;
    float distance;
} wn_ray_hit;

/* Casts a ray against solid voxels (fill heights are respected). */
wn_ray_hit wn_world_raycast(wn_world *w, wn_vec3 origin, wn_vec3 dir, float max_dist);
/* True if any solid voxel overlaps the box [mn, mx] (world units). */
bool wn_world_box_overlaps(wn_world *w, wn_vec3 mn, wn_vec3 mx);
/* Iterates chunks: calls fn for every loaded chunk. */
void wn_world_foreach_chunk(wn_world *w, void (*fn)(void *user, wn_chunk *c), void *user);

#endif
