#include "gl/wn_renderer.h"

#include "gl/wn_gl.h"
#include "gl/wn_shaders.h"
#include "wn/wn_core.h"
#include "wn/wn_mesh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gpu_mesh {
    GLuint vao, vbo, ibo;
    GLsizei count;
} gpu_mesh;

typedef struct model_entry {
    const void *owner;
    int part;
    uint32_t rev;
    gpu_mesh mesh;
    bool used;
    bool seen;
} model_entry;

#define MAX_MODEL_ENTRIES 128
#define MAX_COLUMN_HANDLES 4096

typedef struct prog_voxel {
    GLuint p;
    GLint viewproj, model, shadow_mat, snap, res, mat, shadow_map, hash_origin, cam, moon_dir, moon_col, fire_col,
        fog_col, amb_col, fog_density, ambient, use_shadow, shadow_texel, cut, cut_on, time, rim;
} prog_voxel;

struct wn_renderer {
    bool gles;
    prog_voxel vox;
    GLuint p_shadow, p_sky, p_particle, p_glow_down, p_blur, p_post, p_blit;
    GLint sh_viewproj, sh_model;
    GLuint empty_vao;
    GLuint mat_tex, lut_tex;
    /* internal resolution targets */
    int iw, ih;
    GLuint scene_fbo, scene_tex, scene_depth;
    GLuint final_fbo, final_tex;
    int gw, gh;
    GLuint glow_fbo[2], glow_tex[2];
    int shadow_size;
    GLuint shadow_fbo, shadow_tex, shadow_color;
    GLuint overlay_tex;
    int ow, oh;
    /* geometry */
    gpu_mesh columns[MAX_COLUMN_HANDLES];
    uint32_t next_handle;
    gpu_mesh far_mesh;
    wn_ivec3 far_origin;
    int far_shift;
    float far_cell;
    model_entry models[MAX_MODEL_ENTRIES];
    GLuint part_vao, part_vbo;
    int part_cap;
    wn_mesh scratch;
};

/* ---------------- GL helpers ---------------- */

static GLuint compile(GLenum type, const char *src, bool gles, char *err, size_t errlen)
{
    const char *header = gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler3D;\n"
                              : "#version 330 core\n";
    const char *srcs[2] = {header, src};
    GLuint s = glCreateShader(type);
    glShaderSource(s, 2, srcs, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        snprintf(err, errlen, "shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint program(const char *vs, const char *fs, bool gles, char *err, size_t errlen)
{
    GLuint v = compile(GL_VERTEX_SHADER, vs, gles, err, errlen);
    if (!v) return 0;
    GLuint f = compile(GL_FRAGMENT_SHADER, fs, gles, err, errlen);
    if (!f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        snprintf(err, errlen, "program link failed: %s", log);
        return 0;
    }
    return p;
}

static GLuint make_tex2d(int w, int h, GLint ifmt, GLenum fmt, GLenum type, GLint filter, const void *data)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static GLuint make_fbo(GLuint color, GLuint depth_tex, GLuint depth_rb)
{
    GLuint f;
    glGenFramebuffers(1, &f);
    glBindFramebuffer(GL_FRAMEBUFFER, f);
    if (color) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    if (depth_tex) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex, 0);
    if (depth_rb) glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) wn_log(WN_LOG_ERROR, "framebuffer incomplete");
    return f;
}

static void vertex_layout(void)
{
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(wn_vertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_SHORT, sizeof(wn_vertex), (void *)offsetof(wn_vertex, surface));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 4, GL_UNSIGNED_BYTE, sizeof(wn_vertex), (void *)offsetof(wn_vertex, nao));
}

static void upload_mesh(gpu_mesh *g, const wn_mesh *m)
{
    if (!g->vao) {
        glGenVertexArrays(1, &g->vao);
        glGenBuffers(1, &g->vbo);
        glGenBuffers(1, &g->ibo);
        glBindVertexArray(g->vao);
        glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
        vertex_layout();
    } else {
        glBindVertexArray(g->vao);
        glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    }
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m->nverts * sizeof(wn_vertex)), m->verts, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m->nindices * sizeof(uint32_t)), m->indices, GL_STATIC_DRAW);
    g->count = (GLsizei)m->nindices;
    glBindVertexArray(0);
}

static void free_mesh(gpu_mesh *g)
{
    if (!g->vao) return;
    glDeleteVertexArrays(1, &g->vao);
    glDeleteBuffers(1, &g->vbo);
    glDeleteBuffers(1, &g->ibo);
    memset(g, 0, sizeof *g);
}

/* ---------------- Targets ---------------- */

static void destroy_targets(wn_renderer *r)
{
    GLuint texs[] = {r->scene_tex, r->final_tex, r->glow_tex[0], r->glow_tex[1]};
    GLuint fbos[] = {r->scene_fbo, r->final_fbo, r->glow_fbo[0], r->glow_fbo[1]};
    for (int i = 0; i < 4; i++) {
        if (texs[i]) glDeleteTextures(1, &texs[i]);
        if (fbos[i]) glDeleteFramebuffers(1, &fbos[i]);
    }
    if (r->scene_depth) glDeleteRenderbuffers(1, &r->scene_depth);
    r->scene_tex = r->final_tex = r->glow_tex[0] = r->glow_tex[1] = 0;
    r->scene_fbo = r->final_fbo = r->glow_fbo[0] = r->glow_fbo[1] = 0;
    r->scene_depth = 0;
}

static void ensure_targets(wn_renderer *r, int w, int h, int glow_div)
{
    int gw = glow_div ? WN_MAX(1, w / glow_div) : 1, gh = glow_div ? WN_MAX(1, h / glow_div) : 1;
    if (r->iw == w && r->ih == h && r->gw == gw && r->gh == gh) return;
    destroy_targets(r);
    r->iw = w;
    r->ih = h;
    r->gw = gw;
    r->gh = gh;
    r->scene_tex = make_tex2d(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, NULL);
    glGenRenderbuffers(1, &r->scene_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, r->scene_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    r->scene_fbo = make_fbo(r->scene_tex, 0, r->scene_depth);
    r->final_tex = make_tex2d(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, NULL);
    r->final_fbo = make_fbo(r->final_tex, 0, 0);
    for (int i = 0; i < 2; i++) {
        r->glow_tex[i] = make_tex2d(gw, gh, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR, NULL);
        r->glow_fbo[i] = make_fbo(r->glow_tex[i], 0, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void ensure_shadow(wn_renderer *r, int size)
{
    if (r->shadow_size == size) return;
    if (r->shadow_tex) {
        glDeleteTextures(1, &r->shadow_tex);
        glDeleteTextures(1, &r->shadow_color);
        glDeleteFramebuffers(1, &r->shadow_fbo);
    }
    r->shadow_size = size;
    r->shadow_tex = make_tex2d(size, size, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, GL_NEAREST, NULL);
    /* A colour attachment keeps the FBO complete on every implementation. */
    r->shadow_color = make_tex2d(size, size, GL_R8, GL_RED, GL_UNSIGNED_BYTE, GL_NEAREST, NULL);
    r->shadow_fbo = make_fbo(r->shadow_color, r->shadow_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* ---------------- Creation ---------------- */

static void upload_materials(wn_renderer *r, const wn_materials *mats)
{
    /* 256 x 5 texels: rows 0-3 ramp colours, row 4: ramp length, emit. */
    uint8_t *px = (uint8_t *)calloc(256 * 5 * 4, 1);
    for (int m = 0; m < 256; m++) {
        const wn_material *mt = &mats->m[m];
        for (int k = 0; k < 4; k++) {
            uint32_t c = mt->ramp_n ? mt->ramp[k < mt->ramp_n ? k : mt->ramp_n - 1] : 0xFF00FF;
            uint8_t *o = &px[(k * 256 + m) * 4];
            o[0] = (uint8_t)(c >> 16);
            o[1] = (uint8_t)(c >> 8);
            o[2] = (uint8_t)c;
            o[3] = 255;
        }
        uint8_t *info = &px[(4 * 256 + m) * 4];
        info[0] = (uint8_t)mt->ramp_n;
        info[1] = mt->emit;
        info[3] = 255;
    }
    r->mat_tex = make_tex2d(256, 5, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, px);
    free(px);
}

static void upload_lut(wn_renderer *r, const wn_palette *pal)
{
    uint8_t *lut = (uint8_t *)malloc(WN_LUT_SIZE * WN_LUT_SIZE * WN_LUT_SIZE * 4);
    wn_palette_build_lut(pal, lut);
    glGenTextures(1, &r->lut_tex);
    glBindTexture(GL_TEXTURE_3D, r->lut_tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, WN_LUT_SIZE, WN_LUT_SIZE, WN_LUT_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, lut);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    free(lut);
}

#define U(field, name) r->vox.field = glGetUniformLocation(r->vox.p, name)

wn_renderer *wn_renderer_create(const wn_materials *mats, const wn_palette *pal, char *err, size_t errlen)
{
    wn_renderer *r = (wn_renderer *)calloc(1, sizeof *r);
    r->gles = WN_GLES;
    r->next_handle = 1;
    bool g = r->gles;
    if (!(r->vox.p = program(k_voxel_vs, k_voxel_fs, g, err, errlen)) ||
        !(r->p_shadow = program(k_shadow_vs, k_shadow_fs, g, err, errlen)) ||
        !(r->p_sky = program(k_fullscreen_vs, k_sky_fs, g, err, errlen)) ||
        !(r->p_particle = program(k_particle_vs, k_particle_fs, g, err, errlen)) ||
        !(r->p_glow_down = program(k_fullscreen_vs, k_glow_down_fs, g, err, errlen)) ||
        !(r->p_blur = program(k_fullscreen_vs, k_blur_fs, g, err, errlen)) ||
        !(r->p_post = program(k_fullscreen_vs, k_post_fs, g, err, errlen)) ||
        !(r->p_blit = program(k_fullscreen_vs, k_blit_fs, g, err, errlen))) {
        free(r);
        return NULL;
    }
    U(viewproj, "u_viewproj");
    U(model, "u_model");
    U(shadow_mat, "u_shadow_mat");
    U(snap, "u_snap");
    U(res, "u_res");
    U(mat, "u_mat");
    U(shadow_map, "u_shadow_map");
    U(hash_origin, "u_hash_origin");
    U(cam, "u_cam");
    U(moon_dir, "u_moon_dir");
    U(moon_col, "u_moon_col");
    U(fire_col, "u_fire_col");
    U(fog_col, "u_fog_col");
    U(amb_col, "u_amb_col");
    U(fog_density, "u_fog_density");
    U(ambient, "u_ambient");
    U(use_shadow, "u_use_shadow");
    U(shadow_texel, "u_shadow_texel");
    U(cut, "u_cut");
    U(cut_on, "u_cut_on");
    U(time, "u_time");
    U(rim, "u_rim");
    r->sh_viewproj = glGetUniformLocation(r->p_shadow, "u_viewproj");
    r->sh_model = glGetUniformLocation(r->p_shadow, "u_model");
    glGenVertexArrays(1, &r->empty_vao);
    upload_materials(r, mats);
    upload_lut(r, pal);
    glGenVertexArrays(1, &r->part_vao);
    glGenBuffers(1, &r->part_vbo);
    wn_mesh_init(&r->scratch);
    if (!r->gles) glEnable(GL_PROGRAM_POINT_SIZE);
    return r;
}

void wn_renderer_destroy(wn_renderer *r)
{
    if (!r) return;
    destroy_targets(r);
    for (int i = 0; i < MAX_COLUMN_HANDLES; i++) free_mesh(&r->columns[i]);
    for (int i = 0; i < MAX_MODEL_ENTRIES; i++) free_mesh(&r->models[i].mesh);
    free_mesh(&r->far_mesh);
    wn_mesh_free(&r->scratch);
    free(r);
}

void wn_renderer_internal_size(const wn_renderer *r, int *w, int *h)
{
    *w = r->iw;
    *h = r->ih;
}

void wn_renderer_set_far(wn_renderer *r, const wn_far_grid *g, const wn_materials *mats)
{
    wn_mesh m;
    wn_mesh_init(&m);
    wn_mesh_opts o;
    memset(&o, 0, sizeof o);
    o.full_light = 0xF0;
    o.scale_shift = (uint8_t)g->shift;
    o.hash_origin = g->origin;
    wn_mesh_grid(&m, g->cells, g->sx, g->sy, g->sz, mats, &o);
    upload_mesh(&r->far_mesh, &m);
    wn_mesh_free(&m);
    r->far_origin = g->origin;
    r->far_shift = g->shift;
    r->far_cell = (float)(1 << g->shift) * WN_VOXEL_SIZE;
}

/* ---------------- Drawing ---------------- */

static void set_common_uniforms(wn_renderer *r, const wn_frame_desc *f, wn_mat4 viewproj, wn_mat4 shadow_mat, bool shadow)
{
    const wn_atmos *a = f->atmos;
    glUseProgram(r->vox.p);
    glUniformMatrix4fv(r->vox.viewproj, 1, GL_FALSE, viewproj.m);
    glUniformMatrix4fv(r->vox.shadow_mat, 1, GL_FALSE, shadow_mat.m);
    glUniform1f(r->vox.snap, (f->quality->vertex_snap && !f->xr) ? 1.0f : 0.0f);
    glUniform2f(r->vox.res, (float)r->iw, (float)r->ih);
    glUniform1i(r->vox.mat, 0);
    glUniform1i(r->vox.shadow_map, 1);
    glUniform3f(r->vox.cam, f->eye.x, f->eye.y, f->eye.z);
    glUniform3f(r->vox.moon_dir, a->moon_dir.x, a->moon_dir.y, a->moon_dir.z);
    wn_vec3 mc = wn_v3_scale(wn_rgb_to_vec(a->moon_color), a->moon_intensity);
    glUniform3f(r->vox.moon_col, mc.x, mc.y, mc.z);
    wn_vec3 fc = wn_rgb_to_vec(a->fire_color);
    glUniform3f(r->vox.fire_col, fc.x, fc.y, fc.z);
    wn_vec3 fog = wn_rgb_to_vec(a->fog_color);
    glUniform3f(r->vox.fog_col, fog.x, fog.y, fog.z);
    wn_vec3 amb = wn_v3_add(wn_v3_scale(wn_rgb_to_vec(a->sky_horizon), 2.0f), wn_v3_scale(wn_rgb_to_vec(a->moon_color), 0.8f));
    glUniform3f(r->vox.amb_col, amb.x, amb.y, amb.z);
    glUniform1f(r->vox.fog_density, a->fog_density);
    glUniform1f(r->vox.ambient, a->ambient * 5.0f);
    glUniform1f(r->vox.use_shadow, shadow ? 1.0f : 0.0f);
    glUniform1f(r->vox.shadow_texel, r->shadow_size ? 1.0f / (float)r->shadow_size : 0.0f);
    glUniform1f(r->vox.time, a->time);
    glUniform1f(r->vox.cut_on, 0.0f);
    glUniform1f(r->vox.rim, 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->mat_tex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, r->shadow_tex);
    glActiveTexture(GL_TEXTURE0);
}

static wn_mat4 column_matrix(const wn_world *w, const wn_view_column *c, wn_ivec3 *hash_origin)
{
    float cell = (float)(1 << c->lod) * WN_VOXEL_SIZE;
    wn_vec3 origin = wn_voxel_to_world(wn_iv3(c->cx * WN_CHUNK_SIZE, w->y_min * WN_CHUNK_SIZE, c->cz * WN_CHUNK_SIZE));
    if (hash_origin)
        *hash_origin = wn_iv3((c->cx * WN_CHUNK_SIZE) >> c->lod, (w->y_min * WN_CHUNK_SIZE) >> c->lod,
                              (c->cz * WN_CHUNK_SIZE) >> c->lod);
    return wn_mat4_mul(wn_mat4_translate(origin), wn_mat4_scale(wn_v3(cell, cell, cell)));
}

static void sync_columns(wn_renderer *r, wn_view *v)
{
    uint32_t freed[256];
    uint32_t n;
    while ((n = wn_view_take_freed(v, freed, 256)) > 0)
        for (uint32_t i = 0; i < n; i++)
            if (freed[i] < MAX_COLUMN_HANDLES) free_mesh(&r->columns[freed[i]]);
    for (uint32_t i = 0; i < v->cap; i++) {
        wn_view_column *c = &v->cols[i];
        if (!c->used || !c->gpu_dirty) continue;
        if (!c->gpu) {
            /* find a free handle */
            for (uint32_t k = 0; k < MAX_COLUMN_HANDLES; k++) {
                uint32_t h = (r->next_handle + k) % MAX_COLUMN_HANDLES;
                if (h == 0 || r->columns[h].vao) continue;
                c->gpu = h;
                r->next_handle = h + 1;
                break;
            }
            if (!c->gpu) continue;
        }
        upload_mesh(&r->columns[c->gpu], &c->mesh);
        c->gpu_dirty = false;
    }
}

static gpu_mesh *model_mesh(wn_renderer *r, const void *owner, int part, uint32_t rev, const wn_voxel *grid, int sx, int sy,
                            int sz, const wn_materials *mats)
{
    model_entry *free_slot = NULL;
    for (int i = 0; i < MAX_MODEL_ENTRIES; i++) {
        model_entry *e = &r->models[i];
        if (e->used && e->owner == owner && e->part == part) {
            e->seen = true;
            if (e->rev != rev) {
                wn_mesh_opts o;
                memset(&o, 0, sizeof o);
                o.full_light = 0xF0;
                wn_mesh_clear(&r->scratch);
                wn_mesh_grid(&r->scratch, grid, sx, sy, sz, mats, &o);
                upload_mesh(&e->mesh, &r->scratch);
                e->rev = rev;
            }
            return &e->mesh;
        }
        if (!e->used && !free_slot) free_slot = e;
    }
    if (!free_slot) return NULL;
    free_slot->used = true;
    free_slot->owner = owner;
    free_slot->part = part;
    free_slot->rev = rev + 1; /* force build */
    return model_mesh(r, owner, part, rev, grid, sx, sy, sz, mats);
}

static void draw_mesh(gpu_mesh *g, wn_render_stats *st)
{
    if (!g || !g->count) return;
    glBindVertexArray(g->vao);
    glDrawElements(GL_TRIANGLES, g->count, GL_UNSIGNED_INT, 0);
    st->draw_calls++;
    st->triangles += (uint32_t)g->count / 3;
}

typedef struct draw_ctx {
    wn_renderer *r;
    const wn_frame_desc *f;
    wn_render_stats *st;
    bool shadow_pass;
    GLint model_loc;
    const wn_materials *mats;
} draw_ctx;

static void set_model(draw_ctx *d, wn_mat4 m, wn_ivec3 hash)
{
    glUniformMatrix4fv(d->model_loc, 1, GL_FALSE, m.m);
    if (!d->shadow_pass) glUniform3i(d->r->vox.hash_origin, hash.x, hash.y, hash.z);
}

static void draw_world(draw_ctx *d, const wn_frustum *fr)
{
    wn_renderer *r = d->r;
    const wn_frame_desc *f = d->f;
    wn_view *v = f->view_cache;
    for (uint32_t i = 0; i < v->cap; i++) {
        wn_view_column *c = &v->cols[i];
        if (!c->used || !c->gpu || c->lod < 0) continue;
        wn_vec3 mn, mx;
        wn_view_column_bounds(f->world, c->cx, c->cz, &mn, &mx);
        if (fr && !wn_frustum_box(fr, mn, mx)) continue;
        wn_ivec3 h;
        wn_mat4 m = column_matrix(f->world, c, &h);
        set_model(d, m, h);
        draw_mesh(&r->columns[c->gpu], d->st);
    }
}

static void draw_models(draw_ctx *d)
{
    const wn_frame_desc *f = d->f;
    for (int i = 0; i < f->nmodels; i++) {
        wn_model *m = f->models[i];
        for (int p = 0; p < m->nparts; p++) {
            wn_model_part *part = &m->parts[p];
            if (part->severed) continue;
            gpu_mesh *g = model_mesh(d->r, m, p, part->mesh_rev, part->grid, part->sx, part->sy, part->sz, d->mats);
            set_model(d, wn_model_part_matrix(m, p), wn_iv3(p * 64, 0, 0));
            draw_mesh(g, d->st);
        }
    }
    if (f->debris) {
        for (int i = 0; i < WN_MAX_DEBRIS; i++) {
            wn_debris *db = &f->debris->d[i];
            if (!db->active) continue;
            gpu_mesh *g = model_mesh(d->r, f->debris, (int)db->id, db->mesh_rev, db->grid, db->sx, db->sy, db->sz, d->mats);
            set_model(d, wn_debris_matrix(db), wn_iv3(0, 0, 0));
            draw_mesh(g, d->st);
        }
    }
}

static void gc_models(wn_renderer *r)
{
    for (int i = 0; i < MAX_MODEL_ENTRIES; i++) {
        model_entry *e = &r->models[i];
        if (!e->used) continue;
        if (!e->seen) {
            free_mesh(&e->mesh);
            memset(e, 0, sizeof *e);
        } else {
            e->seen = false;
        }
    }
}

static wn_mat4 shadow_matrix(const wn_frame_desc *f, float extent, int size)
{
    wn_vec3 md = f->atmos->moon_dir;
    wn_vec3 center = f->viewer;
    wn_vec3 eye = wn_v3_add(center, wn_v3_scale(md, 80.0f));
    wn_mat4 view = wn_mat4_look_at(eye, center, wn_v3(0, 1, 0));
    /* Snap to shadow texels so the shadows do not shimmer while moving. */
    float texel = 2.0f * extent / (float)size;
    wn_vec3 c = wn_mat4_mul_point(view, center);
    float sx = (float)wn_floorf_i(c.x / texel) * texel - c.x, sy = (float)wn_floorf_i(c.y / texel) * texel - c.y;
    wn_mat4 proj = wn_mat4_ortho(-extent + sx, extent + sx, -extent + sy, extent + sy, 1.0f, 200.0f);
    return wn_mat4_mul(proj, view);
}

static void draw_particles(wn_renderer *r, const wn_frame_desc *f, wn_mat4 viewproj, wn_render_stats *st)
{
    const wn_fx *fx = f->fx;
    if (!fx || fx->count == 0) return;
    typedef struct pv {
        float x, y, z;
        uint8_t c[4];
        float size;
    } pv;
    if ((int)fx->count > r->part_cap) {
        r->part_cap = (int)fx->count * 2;
        glBindVertexArray(r->part_vao);
        glBindBuffer(GL_ARRAY_BUFFER, r->part_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(r->part_cap * (int)sizeof(pv)), NULL, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(pv), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(pv), (void *)12);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(pv), (void *)16);
    }
    pv *buf = (pv *)malloc(fx->count * sizeof(pv));
    float scale = (float)r->ih / 270.0f;
    for (uint32_t i = 0; i < fx->count; i++) {
        const wn_particle *p = &fx->p[i];
        buf[i].x = p->pos.x;
        buf[i].y = p->pos.y;
        buf[i].z = p->pos.z;
        buf[i].c[0] = (uint8_t)(p->color >> 16);
        buf[i].c[1] = (uint8_t)(p->color >> 8);
        buf[i].c[2] = (uint8_t)p->color;
        buf[i].c[3] = 255;
        buf[i].size = (float)p->size * WN_MAX(1.0f, scale);
    }
    glBindVertexArray(r->part_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->part_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(fx->count * sizeof(pv)), buf, GL_STREAM_DRAW);
    free(buf);
    glUseProgram(r->p_particle);
    glUniformMatrix4fv(glGetUniformLocation(r->p_particle, "u_viewproj"), 1, GL_FALSE, viewproj.m);
    glUniform3f(glGetUniformLocation(r->p_particle, "u_cam"), f->eye.x, f->eye.y, f->eye.z);
    glUniform1f(glGetUniformLocation(r->p_particle, "u_fog_density"), f->atmos->fog_density);
    wn_vec3 fog = wn_rgb_to_vec(f->atmos->fog_color);
    glUniform3f(glGetUniformLocation(r->p_particle, "u_fog_col"), fog.x, fog.y, fog.z);
    wn_vec3 light = wn_v3_add(wn_v3_scale(wn_rgb_to_vec(f->atmos->moon_color), 0.8f), wn_v3(0.15f, 0.15f, 0.2f));
    glUniform3f(glGetUniformLocation(r->p_particle, "u_light"), light.x, light.y, light.z);
    glDrawArrays(GL_POINTS, 0, (GLsizei)fx->count);
    st->draw_calls++;
}

static void fullscreen(wn_renderer *r)
{
    glBindVertexArray(r->empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void wn_renderer_draw(wn_renderer *r, const wn_frame_desc *f, wn_render_stats *st)
{
    memset(st, 0, sizeof *st);
    const wn_quality *q = f->quality;
    wn_res res = wn_internal_res[WN_CLAMP(q->res_index, 0, WN_RES_COUNT - 1)];
    if (f->xr) {
        /* XR eyes: keep the aspect of the eye viewport at a moderate resolution. */
        res.w = WN_CLAMP(f->out_w / 2, 320, 1024);
        res.h = WN_CLAMP(f->out_h / 2, 180, 1024);
    }
    ensure_targets(r, res.w, res.h, q->glow_divisor);
    sync_columns(r, f->view_cache);
    wn_mat4 viewproj = wn_mat4_mul(f->proj, f->view);
    draw_ctx d = {r, f, st, false, r->vox.model, NULL};
    d.mats = f->world->mats;

    /* 1. Moon shadow map. */
    bool shadows = q->shadow_size > 0 && !getenv("WN_DEBUG_NO_SHADOWS");
    float extent = (float)(f->radius + 1) * WN_CHUNK_SIZE * WN_VOXEL_SIZE;
    wn_mat4 shadow_vp = shadow_matrix(f, extent, q->shadow_size ? q->shadow_size : 512);
    if (shadows) {
        ensure_shadow(r, q->shadow_size);
        glBindFramebuffer(GL_FRAMEBUFFER, r->shadow_fbo);
        glViewport(0, 0, r->shadow_size, r->shadow_size);
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE); /* thin geometry (needles) must cast shadows from both sides */
        glUseProgram(r->p_shadow);
        glUniformMatrix4fv(r->sh_viewproj, 1, GL_FALSE, shadow_vp.m);
        draw_ctx sd = d;
        sd.shadow_pass = true;
        sd.model_loc = r->sh_model;
        draw_world(&sd, NULL);
        draw_models(&sd);
    }
    /* Shadow lookup matrix: clip space of the shadow map. */
    wn_mat4 shadow_lookup = shadow_vp;

    /* 2. Scene at internal resolution. */
    glBindFramebuffer(GL_FRAMEBUFFER, r->scene_fbo);
    glViewport(0, 0, r->iw, r->ih);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Sky: rotation-only view so the stars stay put (spec: Stars stay in place). */
    {
        wn_mat4 rot = f->view;
        rot.m[12] = rot.m[13] = rot.m[14] = 0.0f;
        wn_mat4 inv;
        wn_mat4_invert(wn_mat4_mul(f->proj, rot), &inv);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(r->p_sky);
        glUniformMatrix4fv(glGetUniformLocation(r->p_sky, "u_inv_viewproj"), 1, GL_FALSE, inv.m);
        const wn_atmos *a = f->atmos;
        wn_vec3 c;
        c = wn_rgb_to_vec(a->sky_top);
        glUniform3f(glGetUniformLocation(r->p_sky, "u_sky_top"), c.x, c.y, c.z);
        c = wn_rgb_to_vec(a->sky_horizon);
        glUniform3f(glGetUniformLocation(r->p_sky, "u_sky_horizon"), c.x, c.y, c.z);
        glUniform3f(glGetUniformLocation(r->p_sky, "u_moon_dir"), a->moon_dir.x, a->moon_dir.y, a->moon_dir.z);
        c = wn_rgb_to_vec(a->moon_color);
        glUniform3f(glGetUniformLocation(r->p_sky, "u_moon_col"), c.x, c.y, c.z);
        c = wn_rgb_to_vec(a->fog_color);
        glUniform3f(glGetUniformLocation(r->p_sky, "u_fog_col"), c.x, c.y, c.z);
        glUniform1f(glGetUniformLocation(r->p_sky, "u_time"), a->time);
        glUniform1f(glGetUniformLocation(r->p_sky, "u_aurora"), a->aurora ? 1.0f : 0.0f);
        fullscreen(r);
        st->draw_calls++;
    }
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    set_common_uniforms(r, f, viewproj, shadow_lookup, shadows);
    wn_frustum fr = wn_frustum_from_matrix(viewproj);
    draw_world(&d, &fr);
    /* Far scenery outside the loaded square. */
    if (r->far_mesh.count) {
        float s = WN_CHUNK_SIZE * WN_VOXEL_SIZE;
        wn_ivec3 pv = wn_world_to_voxel(f->viewer);
        int pcx = pv.x >> WN_CHUNK_BITS, pcz = pv.z >> WN_CHUNK_BITS;
        float inner = (float)f->radius - 0.25f;
        glUniform4f(r->vox.cut, ((float)pcx - inner) * s, ((float)pcz - inner) * s, ((float)pcx + 1.0f + inner) * s,
                    ((float)pcz + 1.0f + inner) * s);
        glUniform1f(r->vox.cut_on, 1.0f);
        wn_mat4 m = wn_mat4_mul(wn_mat4_translate(wn_voxel_to_world(r->far_origin)),
                                wn_mat4_scale(wn_v3(r->far_cell, r->far_cell, r->far_cell)));
        set_model(&d, m, wn_iv3(r->far_origin.x >> r->far_shift, r->far_origin.y >> r->far_shift, r->far_origin.z >> r->far_shift));
        draw_mesh(&r->far_mesh, st);
        glUniform1f(r->vox.cut_on, 0.0f);
    }
    glDisable(GL_CULL_FACE); /* models are thin and mirrored by rotation */
    glUniform1f(r->vox.rim, 1.0f); /* characters get a moonlit rim so they read at night */
    draw_models(&d);
    glUniform1f(r->vox.rim, 0.0f);
    gc_models(r);
    draw_particles(r, f, viewproj, st);
    glDisable(GL_DEPTH_TEST);

    /* 3. Glow. */
    bool glow = q->glow_divisor > 0;
    if (glow) {
        glBindFramebuffer(GL_FRAMEBUFFER, r->glow_fbo[0]);
        glViewport(0, 0, r->gw, r->gh);
        glUseProgram(r->p_glow_down);
        glBindTexture(GL_TEXTURE_2D, r->scene_tex);
        glUniform1i(glGetUniformLocation(r->p_glow_down, "u_src"), 0);
        glUniform2f(glGetUniformLocation(r->p_glow_down, "u_texel"), 1.0f / (float)r->iw, 1.0f / (float)r->ih);
        fullscreen(r);
        glUseProgram(r->p_blur);
        glUniform1i(glGetUniformLocation(r->p_blur, "u_src"), 0);
        for (int pass = 0; pass < 2; pass++) {
            glBindFramebuffer(GL_FRAMEBUFFER, r->glow_fbo[1 - pass]);
            glBindTexture(GL_TEXTURE_2D, r->glow_tex[pass]);
            glUniform2f(glGetUniformLocation(r->p_blur, "u_dir"), pass == 0 ? 1.0f / (float)r->gw : 0.0f,
                        pass == 0 ? 0.0f : 1.0f / (float)r->gh);
            fullscreen(r);
        }
        st->draw_calls += 3;
    }

    /* 4. Post: palette + dither + overlay into the final internal image. */
    if (f->overlay && (f->overlay->changed || !r->overlay_tex || r->ow != f->overlay->w || r->oh != f->overlay->h)) {
        if (!r->overlay_tex || r->ow != f->overlay->w || r->oh != f->overlay->h) {
            if (r->overlay_tex) glDeleteTextures(1, &r->overlay_tex);
            r->overlay_tex = make_tex2d(f->overlay->w, f->overlay->h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST,
                                        f->overlay->rgba);
            r->ow = f->overlay->w;
            r->oh = f->overlay->h;
        } else {
            glBindTexture(GL_TEXTURE_2D, r->overlay_tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, r->ow, r->oh, GL_RGBA, GL_UNSIGNED_BYTE, f->overlay->rgba);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, r->final_fbo);
    glViewport(0, 0, r->iw, r->ih);
    glUseProgram(r->p_post);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->scene_tex);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, r->glow_tex[0]);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, r->overlay_tex);
    glActiveTexture(GL_TEXTURE0 + 3);
    glBindTexture(GL_TEXTURE_3D, r->lut_tex);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(r->p_post, "u_scene"), 0);
    glUniform1i(glGetUniformLocation(r->p_post, "u_glow"), 1);
    glUniform1i(glGetUniformLocation(r->p_post, "u_overlay"), 2);
    glUniform1i(glGetUniformLocation(r->p_post, "u_lut"), 3);
    glUniform1f(glGetUniformLocation(r->p_post, "u_glow_on"), glow ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(r->p_post, "u_palette_on"), q->palette ? 1.0f : 0.0f);
    glUniform1f(glGetUniformLocation(r->p_post, "u_vignette"), f->vignette);
    fullscreen(r);
    st->draw_calls++;

    /* 5. Upscale (nearest, integer when possible) into the target. */
    glBindFramebuffer(GL_FRAMEBUFFER, f->target_fbo);
    if (f->clear_target) {
        glViewport(0, 0, f->win_w, f->win_h);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glViewport(f->out_x, f->out_y, f->out_w, f->out_h);
    glUseProgram(r->p_blit);
    glBindTexture(GL_TEXTURE_2D, r->final_tex);
    glUniform1i(glGetUniformLocation(r->p_blit, "u_src"), 0);
    fullscreen(r);
    st->draw_calls++;
    glBindVertexArray(0);
}

bool wn_renderer_capture(wn_renderer *r, uint8_t **rgba, int *w, int *h)
{
    if (!r->final_fbo) return false;
    *w = r->iw;
    *h = r->ih;
    uint8_t *buf = (uint8_t *)malloc((size_t)r->iw * r->ih * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, r->final_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, r->iw, r->ih, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* flip to top row first */
    uint8_t *out = (uint8_t *)malloc((size_t)r->iw * r->ih * 4);
    for (int y = 0; y < r->ih; y++) memcpy(out + (size_t)y * r->iw * 4, buf + (size_t)(r->ih - 1 - y) * r->iw * 4, (size_t)r->iw * 4);
    free(buf);
    *rgba = out;
    return true;
}
