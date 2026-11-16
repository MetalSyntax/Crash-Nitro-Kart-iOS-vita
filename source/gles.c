/**
 * @file gles.c
 * @brief OpenGL ES 1.1 pipeline translation over vitaGL.
 * @details The game renders into a virtual 320x480 portrait renderbuffer created on iOS by EAGLView
 *          (with internal projection rotation to landscape). Here that renderbuffer is an FBO with a texture
 *          of the same size: viewport, scissor, and glCopyTexImage2D remain in iOS coordinate space untouched.
 *          During presentation (gles_present), the FBO is drawn rotated and scaled onto the native 960x544 screen.
 *
 *          All imported gl* functions take only integer/pointer arguments (*x variants): no SoftFP issues.
 *          GL_FIXED vertex arrays go straight to vitaGL, which supports them natively.
 */
#include <stdlib.h>
#include <string.h>
#include <vitaGL.h>

#include "gles.h"
#include "hle.h"
#include "imports.h"
#include "log.h"
#include "objc.h"

#define FB_W 320
#define FB_H 480
#define SCR_W 960
#define SCR_H 544
// Destination rectangle: 480x320 scaled to Vita screen height (1.7x) and centered horizontally.
#define DST_H SCR_H
#define DST_W (DST_H * FB_H / FB_W)
#define DST_X ((SCR_W - DST_W) / 2)
GLuint gles_overlay_tex; // menú de controles, a pantalla completa sobre el juego
int gles_dst_x = DST_X, gles_dst_w = DST_W; // 4:3-ish centrado; gles_set_stretch(1) -> 960 de ancho

#define GL_FRAMEBUFFER_COMPLETE_OES 0x8CD5
#define GL_RENDERBUFFER_WIDTH_OES 0x8D42
#define GL_RENDERBUFFER_HEIGHT_OES 0x8D43

static GLuint fbo, fbo_tex;

// Cached state overwritten during blit that glGet cannot fully query:
typedef struct {
    GLint size;
    GLenum type;
    GLsizei stride;
    const void *ptr;
} array_ptr;
static array_ptr vtx, tc[2];
static int client_unit; // glClientActiveTexture - GL_TEXTURE0
static GLint env_mode = GL_MODULATE;
static GLfixed color[4] = {0x10000, 0x10000, 0x10000, 0x10000};

// ---------------------------------------------------------------- client arrays
// vitaGL acepta GL_FIXED de forma nativa (el shader FFP lo divide por 65536), así que los
// punteros pasan tal cual. Solo se cachean para restaurarlos tras el blit de gles_present.

static void g_glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) {
    vtx = (array_ptr){size, type, stride, ptr};
    glVertexPointer(size, type, stride, ptr);
}

static void g_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *ptr) {
    tc[client_unit] = (array_ptr){size, type, stride, ptr};
    glTexCoordPointer(size, type, stride, ptr);
}

static void g_glClientActiveTexture(GLenum unit) {
    client_unit = (unit - GL_TEXTURE0) & 1;
    glClientActiveTexture(unit);
}

static uint32_t gles_draw_calls;

uint32_t gles_get_draw_calls(void) {
    return gles_draw_calls;
}

static void g_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    gles_draw_calls++;
    glDrawArrays(mode, first, count);
}

static void g_glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *idx) {
    gles_draw_calls++;
    glDrawElements(mode, count, type, idx);
}

// Diagnóstico de pantalla blanca: textura del FBO borrada por el juego / texturas del juego.
static void g_glDeleteTextures(GLsizei n, const GLuint *ids) {
    for (int i = 0; i < n; i++) {
        if (ids[i] == fbo_tex) {
            l_warn("glDeleteTextures: guest tried to delete FBO texture %u, ignored", ids[i]);
            continue;
        }
        glDeleteTextures(1, &ids[i]);
    }
}

static void g_glTexImage2D(GLenum target, GLint level, GLint ifmt, GLsizei w, GLsizei h, GLint border,
                           GLenum fmt, GLenum type, const void *px) {
    static int logged;
    if (logged < 16) {
        GLint bound;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
        l_info("glTexImage2D #%d tex=%d lvl=%d %dx%d ifmt=0x%X fmt=0x%X type=0x%X px=%p", ++logged, bound, level,
               w, h, ifmt, fmt, type, px);
    }
    glTexImage2D(target, level, ifmt, w, h, border, fmt, type, px);
}

static void g_glGetPointerv(GLenum pname, void **out) {
    if (pname == GL_VERTEX_ARRAY_POINTER)
        *out = (void *)vtx.ptr;
    else if (pname == GL_TEXTURE_COORD_ARRAY_POINTER)
        *out = (void *)tc[client_unit].ptr;
    else
        glGetPointerv(pname, out);
}

// ---------------------------------------------------------------- *x state

static void g_glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a) {
    color[0] = r, color[1] = g, color[2] = b, color[3] = a;
    glColor4x(r, g, b, a);
}

static void g_glTexEnvi(GLenum target, GLenum pname, GLint v) {
    if (pname == GL_TEXTURE_ENV_MODE)
        env_mode = v;
    glTexEnvi(target, pname, v);
}

static void g_glTexEnvxv(GLenum target, GLenum pname, const GLfixed *v) {
    if (pname == GL_TEXTURE_ENV_MODE) // enum value, not fixed point
        return g_glTexEnvi(target, pname, v[0]);
    glTexEnvxv(target, pname, (GLfixed *)v);
}

// Texture parameters used by the engine are enums (filters, wrap modes): pass unscaled.
static void g_glTexParameterx(GLenum target, GLenum pname, GLfixed v) { glTexParameteri(target, pname, v); }

static void g_glGetFixedv(GLenum pname, GLfixed *out) {
    float f[16];
    int n = 1;
    if (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX || pname == GL_TEXTURE_MATRIX)
        n = 16;
    else if (pname == GL_CURRENT_COLOR) {
        memcpy(out, color, sizeof(color));
        return;
    }
    glGetFloatv(pname, f);
    for (int i = 0; i < n; i++)
        out[i] = (GLfixed)(f[i] * 65536.0f);
}

static void g_glGetLightxv(GLenum light, GLenum pname, GLfixed *out) { memset(out, 0, 4 * sizeof(GLfixed)); }

// ---------------------------------------------------------------- EAGLView framebuffer emulation

static GLuint next_name = 1;
static void g_glGenNames(GLsizei n, GLuint *ids) {
    for (int i = 0; i < n; i++)
        ids[i] = next_name++;
}
static void g_glBindFramebufferOES(GLenum target, GLuint fb) { glBindFramebuffer(GL_FRAMEBUFFER, fbo); }
static void g_glNoop(void) {}
static GLenum g_glCheckFramebufferStatusOES(GLenum target) { return GL_FRAMEBUFFER_COMPLETE_OES; }
static void g_glGetRenderbufferParameterivOES(GLenum target, GLenum pname, GLint *v) {
    *v = pname == GL_RENDERBUFFER_WIDTH_OES ? FB_W : pname == GL_RENDERBUFFER_HEIGHT_OES ? FB_H : 0;
}

// ---------------------------------------------------------------- init / present

void gles_init(void) {
    vglUseTripleBuffering(GL_FALSE);
    vglInitExtended(0, SCR_W, SCR_H, 8 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    glGenTextures(1, &fbo_tex);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FB_W, FB_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glViewport(0, 0, FB_W, FB_H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    l_info("vitaGL: FBO %ux%u -> screen %ux%u at x=%d", FB_W, FB_H, DST_W, DST_H, DST_X);
}

void gles_set_stretch(int on) {
    gles_dst_w = on ? SCR_W : DST_W;
    gles_dst_x = (SCR_W - gles_dst_w) / 2;
    l_info("Screen: %dx%d at x=%d (%s)", gles_dst_w, DST_H, gles_dst_x, on ? "stretched" : "aspect");
}

void gles_present(void) {
    // El estado que deja el juego (culling, niebla, luces, matriz de textura) alteraría el blit.
    static const GLenum caps[] = {GL_BLEND,   GL_ALPHA_TEST, GL_SCISSOR_TEST, GL_DEPTH_TEST,
                                  GL_CULL_FACE, GL_FOG,      GL_LIGHTING,     GL_TEXTURE_2D};
#define NCAPS (sizeof(caps) / sizeof(caps[0]))
    GLboolean on[NCAPS], tca, va, ca, na, tex1;
    GLint tex, mode, active, vp[4];
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE1); // la 2ª unidad no debe modular el blit
    tex1 = glIsEnabled(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    for (int i = 0; i < NCAPS; i++)
        on[i] = glIsEnabled(caps[i]);
    glClientActiveTexture(GL_TEXTURE0);
    tca = glIsEnabled(GL_TEXTURE_COORD_ARRAY);
    va = glIsEnabled(GL_VERTEX_ARRAY);
    ca = glIsEnabled(GL_COLOR_ARRAY);
    na = glIsEnabled(GL_NORMAL_ARRAY);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    glGetIntegerv(GL_VIEWPORT, vp);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCR_W, SCR_H);
    for (int i = 0; i < NCAPS - 1; i++)
        glDisable(caps[i]);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glMatrixMode(GL_TEXTURE);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrthof(0, SCR_W, SCR_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glClear(GL_COLOR_BUFFER_BIT);
    // Screen corners (top-left, top-right, bottom-left, bottom-right) -> vertical FBO texture.
    // Assumed landscape-right rotation (top edge of FBO aligns to left edge of screen).
    // Adjust UV mapping table if target game renders mirrored or inverted.
    static const float uv[] = {1, 1, 1, 0, 0, 1, 0, 0};
    const float x0 = gles_dst_x, x1 = gles_dst_x + gles_dst_w;
    const float xy[] = {x0, 0, x1, 0, x0, SCR_H, x1, SCR_H};
    glVertexPointer(2, GL_FLOAT, 0, xy);
    glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (gles_overlay_tex) {
        static const float ouv[] = {0, 0, 1, 0, 0, 1, 1, 1};
        static const float oxy[] = {0, 0, SCR_W, 0, 0, SCR_H, SCR_W, SCR_H};
        glBindTexture(GL_TEXTURE_2D, gles_overlay_tex);
        glVertexPointer(2, GL_FLOAT, 0, oxy);
        glTexCoordPointer(2, GL_FLOAT, 0, ouv);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    vglSwapBuffers(ime_active()); // GL_TRUE while system dialog is active

    // Restore guest OpenGL state.
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_TEXTURE);
    glPopMatrix();
    glMatrixMode(mode);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env_mode);
    for (int i = 0; i < NCAPS; i++)
        on[i] ? glEnable(caps[i]) : glDisable(caps[i]);
    if (!va)
        glDisableClientState(GL_VERTEX_ARRAY);
    if (!tca)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (ca)
        glEnableClientState(GL_COLOR_ARRAY);
    if (na)
        glEnableClientState(GL_NORMAL_ARRAY);
    glColor4x(color[0], color[1], color[2], color[3]);
    if (vtx.size)
        glVertexPointer(vtx.size, vtx.type, vtx.stride, vtx.ptr);
    if (tc[0].size) // la unidad cliente 0 sigue activa desde el blit
        glTexCoordPointer(tc[0].size, tc[0].type, tc[0].stride, tc[0].ptr);
    glClientActiveTexture(GL_TEXTURE0 + client_unit);
    if (tex1) {
        glActiveTexture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
    }
    glActiveTexture(active);
}

// Cadenas de un iPhone con PowerVR MBX (GLES 1.1): el motor elige rutas según las extensiones.
static const GLubyte *g_glGetString(GLenum name) {
    switch (name) {
    case GL_VENDOR: return (const GLubyte *)"Imagination Technologies";
    case GL_RENDERER: return (const GLubyte *)"PowerVR MBX";
    case GL_VERSION: return (const GLubyte *)"OpenGL ES-CM 1.1";
    case GL_EXTENSIONS:
        return (const GLubyte *)"GL_OES_framebuffer_object GL_OES_matrix_palette GL_IMG_texture_compression_pvrtc "
                                "GL_OES_point_sprite GL_EXT_texture_filter_anisotropic";
    }
    return (const GLubyte *)"";
}

static GLenum g_glGetError(void) {
    GLenum e = glGetError();
    if (e)
        l_debug("glGetError -> 0x%04X", e);
    return e;
}

// ---------------------------------------------------------------- import table

#define F(name, fn) {"_" #name, (uintptr_t)(fn)}
#define D(name) {"_" #name, (uintptr_t)(name)}

const import_entry gles_imports[] = {
    D(glActiveTexture), D(glBindTexture), D(glBlendFunc), D(glClear), D(glCopyTexImage2D),
    D(glCopyTexSubImage2D), F(glDeleteTextures, g_glDeleteTextures), D(glDepthMask), D(glDisable), D(glDisableClientState),
    D(glEnable), D(glEnableClientState), D(glGenTextures), D(glGetIntegerv), D(glIsEnabled),
    D(glLoadIdentity), D(glLoadMatrixx), D(glMatrixMode), D(glOrthox), D(glPopMatrix), D(glPushMatrix),
    D(glRotatex), D(glScalex), D(glScissor), D(glShadeModel), F(glTexImage2D, g_glTexImage2D), D(glTexParameteri),
    D(glTexSubImage2D), D(glTranslatex), D(glViewport),
    D(glAlphaFuncx), D(glClearColorx), D(glColorPointer), D(glCullFace), D(glDepthRangex),
    F(glDrawElements, g_glDrawElements), D(glFrontFace), F(glGetError, g_glGetError), F(glGetString, g_glGetString), D(glColorMask),
    D(glCompressedTexImage2D), D(glDepthFunc), D(glFogx), D(glFogxv), D(glFrustumx), D(glMultMatrixx),
    D(glPixelStorei), D(glPolygonOffsetx), D(glHint), D(glMaterialx),
    D(glMaterialxv), D(glNormalPointer),
    F(glColor4x, g_glColor4x), F(glTexEnvi, g_glTexEnvi), F(glTexEnvxv, g_glTexEnvxv),
    F(glTexParameterx, g_glTexParameterx), F(glGetFixedv, g_glGetFixedv), F(glGetLightxv, g_glGetLightxv),
    F(glVertexPointer, g_glVertexPointer), F(glTexCoordPointer, g_glTexCoordPointer),
    F(glClientActiveTexture, g_glClientActiveTexture), F(glDrawArrays, g_glDrawArrays),
    F(glGetPointerv, g_glGetPointerv),
    F(glGenFramebuffersOES, g_glGenNames), F(glGenRenderbuffersOES, g_glGenNames),
    F(glBindFramebufferOES, g_glBindFramebufferOES), F(glBindRenderbufferOES, g_glNoop),
    F(glDeleteFramebuffersOES, g_glNoop), F(glDeleteRenderbuffersOES, g_glNoop),
    F(glRenderbufferStorageOES, g_glNoop), F(glFramebufferRenderbufferOES, g_glNoop),
    F(glCheckFramebufferStatusOES, g_glCheckFramebufferStatusOES),
    F(glGetRenderbufferParameterivOES, g_glGetRenderbufferParameterivOES),
};
const int gles_imports_count = sizeof(gles_imports) / sizeof(gles_imports[0]);
