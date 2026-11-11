/* Winternight platform - minimal OpenGL 3.3 core / OpenGL ES 3.0 (WebGL2)
 * binding. Only the common subset is used (design D4). On desktop the
 * functions are loaded through SDL; on the web they are linked directly. */
#ifndef WN_GL_H
#define WN_GL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define WN_GLES 1
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#else
#define WN_GLES 0

#if defined(_WIN32)
#define WN_APIENTRY __stdcall
#else
#define WN_APIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned char GLubyte;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_POINTS 0x0000
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_ALWAYS 0x0207
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_BACK 0x0405
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RED 0x1903
#define GL_RGBA 0x1908
#define GL_DEPTH_COMPONENT 0x1902
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_RGBA8 0x8058
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_R8 0x8229
#define GL_TEXTURE0 0x84C0
#define GL_PROGRAM_POINT_SIZE 0x8642
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41

#define WN_GL_FUNCS(X)                                                                                               \
    X(void, glActiveTexture, (GLenum texture))                                                                      \
    X(void, glAttachShader, (GLuint program, GLuint shader))                                                        \
    X(void, glBindBuffer, (GLenum target, GLuint buffer))                                                           \
    X(void, glBindFramebuffer, (GLenum target, GLuint fb))                                                          \
    X(void, glBindRenderbuffer, (GLenum target, GLuint rb))                                                         \
    X(void, glBindTexture, (GLenum target, GLuint texture))                                                         \
    X(void, glBindVertexArray, (GLuint array))                                                                      \
    X(void, glBlendFunc, (GLenum s, GLenum d))                                                                      \
    X(void, glBufferData, (GLenum target, GLsizeiptr size, const void *data, GLenum usage))                        \
    X(GLenum, glCheckFramebufferStatus, (GLenum target))                                                            \
    X(void, glClear, (GLbitfield mask))                                                                             \
    X(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                                             \
    X(void, glCompileShader, (GLuint shader))                                                                       \
    X(GLuint, glCreateProgram, (void))                                                                              \
    X(GLuint, glCreateShader, (GLenum type))                                                                        \
    X(void, glCullFace, (GLenum mode))                                                                              \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint *b))                                                          \
    X(void, glDeleteFramebuffers, (GLsizei n, const GLuint *f))                                                     \
    X(void, glDeleteProgram, (GLuint p))                                                                            \
    X(void, glDeleteRenderbuffers, (GLsizei n, const GLuint *r))                                                    \
    X(void, glDeleteShader, (GLuint s))                                                                             \
    X(void, glDeleteTextures, (GLsizei n, const GLuint *t))                                                         \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint *a))                                                     \
    X(void, glDepthFunc, (GLenum f))                                                                                \
    X(void, glDepthMask, (GLboolean f))                                                                             \
    X(void, glDisable, (GLenum cap))                                                                                \
    X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))                                                \
    X(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void *indices))                        \
    X(void, glEnable, (GLenum cap))                                                                                 \
    X(void, glEnableVertexAttribArray, (GLuint index))                                                              \
    X(void, glFramebufferRenderbuffer, (GLenum t, GLenum a, GLenum rt, GLuint rb))                                 \
    X(void, glFramebufferTexture2D, (GLenum t, GLenum a, GLenum tt, GLuint tex, GLint level))                      \
    X(void, glGenBuffers, (GLsizei n, GLuint *b))                                                                   \
    X(void, glGenFramebuffers, (GLsizei n, GLuint *f))                                                              \
    X(void, glGenRenderbuffers, (GLsizei n, GLuint *r))                                                             \
    X(void, glGenTextures, (GLsizei n, GLuint *t))                                                                  \
    X(void, glGenVertexArrays, (GLsizei n, GLuint *a))                                                              \
    X(GLenum, glGetError, (void))                                                                                   \
    X(void, glGetIntegerv, (GLenum p, GLint *v))                                                                    \
    X(void, glGetProgramInfoLog, (GLuint p, GLsizei n, GLsizei *len, GLchar *log))                                 \
    X(void, glGetProgramiv, (GLuint p, GLenum name, GLint *v))                                                      \
    X(void, glGetShaderInfoLog, (GLuint s, GLsizei n, GLsizei *len, GLchar *log))                                  \
    X(void, glGetShaderiv, (GLuint s, GLenum name, GLint *v))                                                       \
    X(const GLubyte *, glGetString, (GLenum name))                                                                  \
    X(GLint, glGetUniformLocation, (GLuint p, const GLchar *name))                                                  \
    X(void, glLinkProgram, (GLuint p))                                                                              \
    X(void, glPixelStorei, (GLenum p, GLint v))                                                                     \
    X(void, glPolygonOffset, (GLfloat f, GLfloat u))                                                                \
    X(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *d))                   \
    X(void, glRenderbufferStorage, (GLenum t, GLenum f, GLsizei w, GLsizei h))                                     \
    X(void, glScissor, (GLint x, GLint y, GLsizei w, GLsizei h))                                                    \
    X(void, glShaderSource, (GLuint s, GLsizei n, const GLchar *const *src, const GLint *len))                     \
    X(void, glTexImage2D,                                                                                          \
      (GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void *d))         \
    X(void, glTexImage3D,                                                                                          \
      (GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLsizei d, GLint b, GLenum f, GLenum ty, const void *p)) \
    X(void, glTexSubImage2D,                                                                                       \
      (GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const void *d))            \
    X(void, glTexParameteri, (GLenum t, GLenum p, GLint v))                                                         \
    X(void, glUniform1f, (GLint l, GLfloat v))                                                                      \
    X(void, glUniform1i, (GLint l, GLint v))                                                                        \
    X(void, glUniform2f, (GLint l, GLfloat a, GLfloat b))                                                           \
    X(void, glUniform3f, (GLint l, GLfloat a, GLfloat b, GLfloat c))                                                \
    X(void, glUniform3i, (GLint l, GLint a, GLint b, GLint c))                                                      \
    X(void, glUniform4f, (GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                                     \
    X(void, glUniformMatrix4fv, (GLint l, GLsizei n, GLboolean t, const GLfloat *v))                                \
    X(void, glUseProgram, (GLuint p))                                                                               \
    X(void, glVertexAttribIPointer, (GLuint i, GLint size, GLenum type, GLsizei stride, const void *p))            \
    X(void, glVertexAttribPointer, (GLuint i, GLint size, GLenum type, GLboolean n, GLsizei stride, const void *p)) \
    X(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h))

#define WN_GL_DECLARE(ret, name, params)                                                                             \
    typedef ret(WN_APIENTRY *PFN_##name) params;                                                                    \
    extern PFN_##name wn_##name;
WN_GL_FUNCS(WN_GL_DECLARE)
#undef WN_GL_DECLARE

#define glActiveTexture wn_glActiveTexture
#define glAttachShader wn_glAttachShader
#define glBindBuffer wn_glBindBuffer
#define glBindFramebuffer wn_glBindFramebuffer
#define glBindRenderbuffer wn_glBindRenderbuffer
#define glBindTexture wn_glBindTexture
#define glBindVertexArray wn_glBindVertexArray
#define glBlendFunc wn_glBlendFunc
#define glBufferData wn_glBufferData
#define glCheckFramebufferStatus wn_glCheckFramebufferStatus
#define glClear wn_glClear
#define glClearColor wn_glClearColor
#define glCompileShader wn_glCompileShader
#define glCreateProgram wn_glCreateProgram
#define glCreateShader wn_glCreateShader
#define glCullFace wn_glCullFace
#define glDeleteBuffers wn_glDeleteBuffers
#define glDeleteFramebuffers wn_glDeleteFramebuffers
#define glDeleteProgram wn_glDeleteProgram
#define glDeleteRenderbuffers wn_glDeleteRenderbuffers
#define glDeleteShader wn_glDeleteShader
#define glDeleteTextures wn_glDeleteTextures
#define glDeleteVertexArrays wn_glDeleteVertexArrays
#define glDepthFunc wn_glDepthFunc
#define glDepthMask wn_glDepthMask
#define glDisable wn_glDisable
#define glDrawArrays wn_glDrawArrays
#define glDrawElements wn_glDrawElements
#define glEnable wn_glEnable
#define glEnableVertexAttribArray wn_glEnableVertexAttribArray
#define glFramebufferRenderbuffer wn_glFramebufferRenderbuffer
#define glFramebufferTexture2D wn_glFramebufferTexture2D
#define glGenBuffers wn_glGenBuffers
#define glGenFramebuffers wn_glGenFramebuffers
#define glGenRenderbuffers wn_glGenRenderbuffers
#define glGenTextures wn_glGenTextures
#define glGenVertexArrays wn_glGenVertexArrays
#define glGetError wn_glGetError
#define glGetIntegerv wn_glGetIntegerv
#define glGetProgramInfoLog wn_glGetProgramInfoLog
#define glGetProgramiv wn_glGetProgramiv
#define glGetShaderInfoLog wn_glGetShaderInfoLog
#define glGetShaderiv wn_glGetShaderiv
#define glGetString wn_glGetString
#define glGetUniformLocation wn_glGetUniformLocation
#define glLinkProgram wn_glLinkProgram
#define glPixelStorei wn_glPixelStorei
#define glPolygonOffset wn_glPolygonOffset
#define glReadPixels wn_glReadPixels
#define glRenderbufferStorage wn_glRenderbufferStorage
#define glScissor wn_glScissor
#define glShaderSource wn_glShaderSource
#define glTexImage2D wn_glTexImage2D
#define glTexImage3D wn_glTexImage3D
#define glTexSubImage2D wn_glTexSubImage2D
#define glTexParameteri wn_glTexParameteri
#define glUniform1f wn_glUniform1f
#define glUniform1i wn_glUniform1i
#define glUniform2f wn_glUniform2f
#define glUniform3f wn_glUniform3f
#define glUniform3i wn_glUniform3i
#define glUniform4f wn_glUniform4f
#define glUniformMatrix4fv wn_glUniformMatrix4fv
#define glUseProgram wn_glUseProgram
#define glVertexAttribIPointer wn_glVertexAttribIPointer
#define glVertexAttribPointer wn_glVertexAttribPointer
#define glViewport wn_glViewport
#endif

/* Loads all functions; returns the name of the first missing one or NULL. */
typedef void *(*wn_gl_getproc)(const char *name);
const char *wn_gl_load(wn_gl_getproc getproc);

#endif
