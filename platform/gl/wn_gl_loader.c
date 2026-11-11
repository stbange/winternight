#include "gl/wn_gl.h"

#ifdef __EMSCRIPTEN__
const char *wn_gl_load(wn_gl_getproc getproc)
{
    (void)getproc;
    return 0; /* WebGL2 functions are linked directly */
}
#else
#define WN_GL_DEFINE(ret, name, params) PFN_##name wn_##name;
WN_GL_FUNCS(WN_GL_DEFINE)
#undef WN_GL_DEFINE

const char *wn_gl_load(wn_gl_getproc getproc)
{
#define WN_GL_LOAD(ret, name, params)                                                                                \
    wn_##name = (PFN_##name)getproc(#name);                                                                          \
    if (!wn_##name) return #name;
    WN_GL_FUNCS(WN_GL_LOAD)
#undef WN_GL_LOAD
    return 0;
}
#endif
