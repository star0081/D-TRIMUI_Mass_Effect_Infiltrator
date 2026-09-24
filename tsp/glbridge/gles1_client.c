#define _GNU_SOURCE
#include "gles1.h"
#include "gles1_math.h"
#include "ops.h"
#include "xport.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t GLenum;
typedef uint32_t GLuint;
typedef int32_t GLint;
typedef int32_t GLsizei;
typedef uint8_t GLboolean;
typedef float GLfloat;
typedef uint8_t GLubyte;
typedef int16_t GLshort;
typedef uint16_t GLushort;
typedef int32_t GLfixed;
typedef uint32_t GLbitfield;
typedef void GLvoid;

#define GL_MODELVIEW 0x1700
#define GL_PROJECTION 0x1701
#define GL_TEXTURE 0x1702
#define GL_MATRIX_PALETTE_OES 0x8840
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_COLOR_ARRAY 0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_MATRIX_INDEX_ARRAY_OES 0x8844
#define GL_WEIGHT_ARRAY_OES 0x86AD
#define GL_POINT_SIZE_ARRAY_OES 0x8B9C
#define GL_LIGHTING 0x0B50
#define GL_LIGHT0 0x4000
#define GL_TEXTURE_2D 0x0DE1
#define GL_COLOR_MATERIAL 0x0B57
#define GL_NORMALIZE 0x0BA1
#define GL_RESCALE_NORMAL 0x803A
#define GL_FOG 0x0B60
#define GL_ALPHA_TEST 0x0BC0
#define GL_CLIP_PLANE0 0x3000
#define GL_POINT_SPRITE_OES 0x8861
#define GL_COLOR_LOGIC_OP 0x0BF2
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_AMBIENT 0x1200
#define GL_DIFFUSE 0x1201
#define GL_SPECULAR 0x1202
#define GL_POSITION 0x1203
#define GL_SPOT_DIRECTION 0x1204
#define GL_SPOT_EXPONENT 0x1205
#define GL_SPOT_CUTOFF 0x1206
#define GL_CONSTANT_ATTENUATION 0x1207
#define GL_LINEAR_ATTENUATION 0x1208
#define GL_QUADRATIC_ATTENUATION 0x1209
#define GL_EMISSION 0x1600
#define GL_SHININESS 0x1601
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_LIGHT_MODEL_TWO_SIDE 0x0B52
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_ENV_COLOR 0x2201
#define GL_MODULATE 0x2100
#define GL_FOG_MODE 0x0B65
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_COLOR 0x0B66
#define GL_EXP 0x0800

#define ES1_MV 0
#define ES1_PR 1
#define ES1_TX 2
#define ES1_STACK 16

struct es1_light {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float position[4];
    float spot_dir[3];
    float spot_exp;
    float spot_cut;
    float att[3];
};

struct es1_state {
    int mode;
    int pal_i;
    int depth[3];
    float stack[3][ES1_STACK][16];
    float palette[16][16];
    float color[4];
    float normal[3];
    float texcoord[4];
    float mat_amb[4];
    float mat_diff[4];
    float mat_spec[4];
    float mat_emis[4];
    float shininess;
    float scene_amb[4];
    int two_side;
    struct es1_light light[8];
    int cap_light[8];
    int cap_lighting;
    int cap_tex2d;
    int cap_color_mat;
    int cap_normalize;
    int cap_fog;
    int cap_alpha;
    int cap_palette;
    int cap_clip[6];
    uint32_t texenv_mode;
    float texenv_color[4];
    uint32_t alpha_func;
    float alpha_ref;
    uint32_t fog_mode;
    float fog_density;
    float fog_start;
    float fog_end;
    float fog_color[4];
    uint32_t shade;
    int client_tex;
    void *ptr_vertex;
    void *ptr_color;
    void *ptr_normal;
    void *ptr_tex;
    void *ptr_weight;
    void *ptr_midx;
};

static struct es1_state S;
static int S_init;

static void light_reset(struct es1_light *L, int i)
{
    memset(L, 0, sizeof(*L));
    L->ambient[3] = 1.f;
    if (i == 0) {
        L->diffuse[0] = L->diffuse[1] = L->diffuse[2] = L->diffuse[3] = 1.f;
        L->specular[0] = L->specular[1] = L->specular[2] = L->specular[3] = 1.f;
    } else {
        L->diffuse[3] = 1.f;
        L->specular[3] = 1.f;
    }
    L->position[2] = 1.f;
    L->spot_dir[2] = -1.f;
    L->spot_cut = 180.f;
    L->att[0] = 1.f;
}

static void es1_ensure(void)
{
    int i;
    if (S_init)
        return;
    memset(&S, 0, sizeof(S));
    S.mode = GL_MODELVIEW;
    for (i = 0; i < 3; ++i)
        es1_ident(S.stack[i][0]);
    for (i = 0; i < 16; ++i)
        es1_ident(S.palette[i]);
    S.color[0] = S.color[1] = S.color[2] = S.color[3] = 1.f;
    S.normal[2] = 1.f;
    S.texcoord[3] = 1.f;
    S.mat_amb[0] = S.mat_amb[1] = S.mat_amb[2] = 0.2f;
    S.mat_amb[3] = 1.f;
    S.mat_diff[0] = S.mat_diff[1] = S.mat_diff[2] = 0.8f;
    S.mat_diff[3] = 1.f;
    S.mat_spec[3] = 1.f;
    S.mat_emis[3] = 1.f;
    S.scene_amb[0] = S.scene_amb[1] = S.scene_amb[2] = 0.2f;
    S.scene_amb[3] = 1.f;
    for (i = 0; i < 8; ++i)
        light_reset(&S.light[i], i);
    S.cap_light[0] = 1;
    S.texenv_mode = GL_MODULATE;
    S.alpha_func = 0x0207; /* ALWAYS */
    S.fog_mode = GL_EXP;
    S.fog_density = 1.f;
    S.fog_end = 1.f;
    S.shade = 0x1D01; /* SMOOTH */
    S_init = 1;
}

static int mode_i(uint32_t mode)
{
    if (mode == GL_PROJECTION)
        return ES1_PR;
    if (mode == GL_TEXTURE)
        return ES1_TX;
    return ES1_MV;
}

static float *cur_mat(void)
{
    int i = mode_i((uint32_t)S.mode);
    int d = S.depth[i];
    if (d < 0)
        d = 0;
    if (d >= ES1_STACK)
        d = ES1_STACK - 1;
    return S.stack[i][d];
}

static void send_u32(uint32_t op, uint32_t a)
{
    tspgl_call(op, &a, 4, NULL, 0);
}

static void send_f(uint32_t op, const float *f, unsigned n)
{
    tspgl_call(op, f, n * 4, NULL, 0);
}

static void send_u32_f(uint32_t op, uint32_t a, const float *f, unsigned n)
{
    uint8_t buf[4 + 64];
    memcpy(buf, &a, 4);
    memcpy(buf + 4, f, n * 4);
    tspgl_call(op, buf, 4 + n * 4, NULL, 0);
}

static void send_u32_2_f(uint32_t op, uint32_t a, uint32_t b, const float *f,
                         unsigned n)
{
    uint8_t buf[8 + 64];
    memcpy(buf, &a, 4);
    memcpy(buf + 4, &b, 4);
    memcpy(buf + 8, f, n * 4);
    tspgl_call(op, buf, 8 + n * 4, NULL, 0);
}

int tspgl_es1_cap(uint32_t cap)
{
    if (cap == GL_TEXTURE_2D || cap == GL_LIGHTING || cap == GL_COLOR_MATERIAL ||
        cap == GL_NORMALIZE || cap == GL_RESCALE_NORMAL || cap == GL_FOG ||
        cap == GL_ALPHA_TEST || cap == GL_MATRIX_PALETTE_OES ||
        cap == GL_POINT_SPRITE_OES || cap == GL_COLOR_LOGIC_OP)
        return 1;
    if (cap >= GL_LIGHT0 && cap <= GL_LIGHT0 + 7)
        return 1;
    if (cap >= GL_CLIP_PLANE0 && cap <= GL_CLIP_PLANE0 + 5)
        return 1;
    return 0;
}

int tspgl_es1_set_enable(uint32_t cap, int on)
{
    es1_ensure();
    if (!tspgl_es1_cap(cap))
        return 0;
    if (cap == GL_TEXTURE_2D)
        S.cap_tex2d = on;
    else if (cap == GL_LIGHTING)
        S.cap_lighting = on;
    else if (cap == GL_COLOR_MATERIAL)
        S.cap_color_mat = on;
    else if (cap == GL_NORMALIZE || cap == GL_RESCALE_NORMAL)
        S.cap_normalize = on;
    else if (cap == GL_FOG)
        S.cap_fog = on;
    else if (cap == GL_ALPHA_TEST)
        S.cap_alpha = on;
    else if (cap == GL_MATRIX_PALETTE_OES)
        S.cap_palette = on;
    else if (cap >= GL_LIGHT0 && cap <= GL_LIGHT0 + 7)
        S.cap_light[cap - GL_LIGHT0] = on;
    else if (cap >= GL_CLIP_PLANE0 && cap <= GL_CLIP_PLANE0 + 5)
        S.cap_clip[cap - GL_CLIP_PLANE0] = on;
    send_u32(on ? OP_ES1_ENABLE : OP_ES1_DISABLE, cap);
    return 1;
}

int tspgl_es1_is_enabled(uint32_t cap)
{
    es1_ensure();
    if (!tspgl_es1_cap(cap))
        return -1;
    if (cap == GL_TEXTURE_2D)
        return S.cap_tex2d;
    if (cap == GL_LIGHTING)
        return S.cap_lighting;
    if (cap == GL_COLOR_MATERIAL)
        return S.cap_color_mat;
    if (cap == GL_NORMALIZE || cap == GL_RESCALE_NORMAL)
        return S.cap_normalize;
    if (cap == GL_FOG)
        return S.cap_fog;
    if (cap == GL_ALPHA_TEST)
        return S.cap_alpha;
    if (cap == GL_MATRIX_PALETTE_OES)
        return S.cap_palette;
    if (cap >= GL_LIGHT0 && cap <= GL_LIGHT0 + 7)
        return S.cap_light[cap - GL_LIGHT0];
    if (cap >= GL_CLIP_PLANE0 && cap <= GL_CLIP_PLANE0 + 5)
        return S.cap_clip[cap - GL_CLIP_PLANE0];
    return 0;
}

int tspgl_es1_get_integerv(uint32_t pname, int32_t *params)
{
    es1_ensure();
    if (!params)
        return 0;
    switch (pname) {
    case 0x0D31: /* MAX_LIGHTS */
        params[0] = 8;
        return 1;
    case 0x0D32: /* MAX_CLIP_PLANES */
        params[0] = 0;
        return 1;
    case 0x0D36: /* MAX_MODELVIEW_STACK_DEPTH */
        params[0] = ES1_STACK;
        return 1;
    case 0x0D38: /* MAX_PROJECTION_STACK_DEPTH */
        params[0] = 4;
        return 1;
    case 0x0D39: /* MAX_TEXTURE_STACK_DEPTH */
        params[0] = 4;
        return 1;
    case 0x0D3B: /* MAX_TEXTURE_UNITS (ES1) */
    case 0x84E2: /* MAX_TEXTURE_UNITS / MAX_TEXTURE_IMAGE_UNITS */
        params[0] = 2;
        return 1;
    case 0x8842: /* MAX_PALETTE_MATRICES_OES */
        params[0] = 16;
        return 1;
    case 0x86A4: /* MAX_VERTEX_UNITS_OES */
        params[0] = 4;
        return 1;
    case 0x0BA0: /* MATRIX_MODE */
        params[0] = S.mode;
        return 1;
    case 0x84E1: /* CLIENT_ACTIVE_TEXTURE */
        params[0] = (int32_t)(0x84C0 + S.client_tex);
        return 1;
    case 0x0BA3: /* MODELVIEW_STACK_DEPTH */
        params[0] = S.depth[ES1_MV] + 1;
        return 1;
    case 0x0BA4: /* PROJECTION_STACK_DEPTH */
        params[0] = S.depth[ES1_PR] + 1;
        return 1;
    case 0x0BA5: /* TEXTURE_STACK_DEPTH */
        params[0] = S.depth[ES1_TX] + 1;
        return 1;
    case 0x0BC1: /* ALPHA_TEST_FUNC */
        params[0] = (int32_t)S.alpha_func;
        return 1;
    case 0x0B54: /* SHADE_MODEL */
        params[0] = (int32_t)S.shade;
        return 1;
    case 0x2200:
        params[0] = (int32_t)S.texenv_mode;
        return 1;
    default:
        return 0;
    }
}

int tspgl_es1_get_floatv(uint32_t pname, float *params)
{
    int i;
    es1_ensure();
    if (!params)
        return 0;
    switch (pname) {
    case 0x0BA6: /* MODELVIEW_MATRIX */
        memcpy(params, S.stack[ES1_MV][S.depth[ES1_MV]], 64);
        return 1;
    case 0x0BA7: /* PROJECTION_MATRIX */
        memcpy(params, S.stack[ES1_PR][S.depth[ES1_PR]], 64);
        return 1;
    case 0x0BA8: /* TEXTURE_MATRIX */
        memcpy(params, S.stack[ES1_TX][S.depth[ES1_TX]], 64);
        return 1;
    case 0x0B00: /* CURRENT_COLOR */
        memcpy(params, S.color, 16);
        return 1;
    case 0x0B02: /* CURRENT_NORMAL */
        memcpy(params, S.normal, 12);
        return 1;
    case 0x0BC2: /* ALPHA_TEST_REF */
        params[0] = S.alpha_ref;
        return 1;
    case GL_LIGHT_MODEL_AMBIENT:
        memcpy(params, S.scene_amb, 16);
        return 1;
    case GL_FOG_COLOR:
        memcpy(params, S.fog_color, 16);
        return 1;
    case GL_FOG_DENSITY:
        params[0] = S.fog_density;
        return 1;
    case GL_FOG_START:
        params[0] = S.fog_start;
        return 1;
    case GL_FOG_END:
        params[0] = S.fog_end;
        return 1;
    case 0x0B12: /* POINT_SIZE */
        params[0] = 1.f;
        return 1;
    default:
        (void)i;
        return 0;
    }
}

int tspgl_es1_get_booleanv(uint32_t pname, uint8_t *params)
{
    int v = tspgl_es1_is_enabled(pname);
    if (v < 0)
        return 0;
    if (params)
        params[0] = (uint8_t)v;
    return 1;
}

int tspgl_es1_get_fixedv(uint32_t pname, int32_t *params)
{
    float f[16];
    int i, n = 1;
    if (!tspgl_es1_get_floatv(pname, f))
        return 0;
    if (pname == 0x0BA6 || pname == 0x0BA7 || pname == 0x0BA8)
        n = 16;
    else if (pname == 0x0B00 || pname == GL_LIGHT_MODEL_AMBIENT ||
             pname == GL_FOG_COLOR)
        n = 4;
    else if (pname == 0x0B02)
        n = 3;
    if (params)
        for (i = 0; i < n; ++i)
            params[i] = (int32_t)(f[i] * 65536.f);
    return 1;
}

void glMatrixMode(GLenum mode)
{
    es1_ensure();
    S.mode = (int)mode;
    send_u32(OP_ES1_MATRIX_MODE, mode);
}

void glLoadIdentity(void)
{
    es1_ensure();
    es1_ident(cur_mat());
    tspgl_call(OP_ES1_LOAD_IDENTITY, NULL, 0, NULL, 0);
}

void glLoadMatrixf(const GLfloat *m)
{
    es1_ensure();
    if (!m)
        return;
    es1_copy(cur_mat(), m);
    send_f(OP_ES1_LOAD_MATRIX, m, 16);
}

void glLoadMatrixx(const GLfixed *m)
{
    float f[16];
    int i;
    if (!m)
        return;
    for (i = 0; i < 16; ++i)
        f[i] = es1_x2f(m[i]);
    glLoadMatrixf(f);
}

void glMultMatrixf(const GLfloat *m)
{
    es1_ensure();
    if (!m)
        return;
    es1_mul(cur_mat(), cur_mat(), m);
    send_f(OP_ES1_MULT_MATRIX, m, 16);
}

void glMultMatrixx(const GLfixed *m)
{
    float f[16];
    int i;
    if (!m)
        return;
    for (i = 0; i < 16; ++i)
        f[i] = es1_x2f(m[i]);
    glMultMatrixf(f);
}

void glPushMatrix(void)
{
    int i, d;
    es1_ensure();
    i = mode_i((uint32_t)S.mode);
    d = S.depth[i];
    if (d + 1 < ES1_STACK) {
        es1_copy(S.stack[i][d + 1], S.stack[i][d]);
        S.depth[i] = d + 1;
    }
    tspgl_call(OP_ES1_PUSH, NULL, 0, NULL, 0);
}

void glPopMatrix(void)
{
    int i;
    es1_ensure();
    i = mode_i((uint32_t)S.mode);
    if (S.depth[i] > 0)
        S.depth[i]--;
    tspgl_call(OP_ES1_POP, NULL, 0, NULL, 0);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    float u[3] = { x, y, z };
    es1_ensure();
    es1_translate(cur_mat(), x, y, z);
    send_f(OP_ES1_TRANSLATE, u, 3);
}

void glTranslatex(GLfixed x, GLfixed y, GLfixed z)
{
    glTranslatef(es1_x2f(x), es1_x2f(y), es1_x2f(z));
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    float u[3] = { x, y, z };
    es1_ensure();
    es1_scale(cur_mat(), x, y, z);
    send_f(OP_ES1_SCALE, u, 3);
}

void glScalex(GLfixed x, GLfixed y, GLfixed z)
{
    glScalef(es1_x2f(x), es1_x2f(y), es1_x2f(z));
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    float u[4] = { angle, x, y, z };
    es1_ensure();
    es1_rotate(cur_mat(), angle, x, y, z);
    send_f(OP_ES1_ROTATE, u, 4);
}

void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
{
    glRotatef(es1_x2f(angle), es1_x2f(x), es1_x2f(y), es1_x2f(z));
}

void glFrustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    float u[6] = { l, r, b, t, n, f };
    es1_ensure();
    es1_frustum(cur_mat(), l, r, b, t, n, f);
    send_f(OP_ES1_FRUSTUM, u, 6);
}

void glFrustumx(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    glFrustumf(es1_x2f(l), es1_x2f(r), es1_x2f(b), es1_x2f(t), es1_x2f(n),
               es1_x2f(f));
}

void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
    float u[6] = { l, r, b, t, n, f };
    static int nlog;
    es1_ensure();
    if (nlog < 6) {
        fprintf(stderr, "tspgl: Ortho %g,%g %g,%g z=%g,%g\n", (double)l,
                (double)r, (double)b, (double)t, (double)n, (double)f);
        fflush(stderr);
        nlog++;
    }
    es1_ortho(cur_mat(), l, r, b, t, n, f);
    send_f(OP_ES1_ORTHO, u, 6);
}

void glOrthox(GLfixed l, GLfixed r, GLfixed b, GLfixed t, GLfixed n, GLfixed f)
{
    glOrthof(es1_x2f(l), es1_x2f(r), es1_x2f(b), es1_x2f(t), es1_x2f(n),
             es1_x2f(f));
}

void glClipPlanef(GLenum p, const GLfloat *eqn)
{
    (void)p;
    (void)eqn;
}

void glClipPlanex(GLenum plane, const GLfixed *equation)
{
    (void)plane;
    (void)equation;
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    float u[4] = { r, g, b, a };
    es1_ensure();
    memcpy(S.color, u, 16);
    send_f(OP_ES1_COLOR, u, 4);
}

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    glColor4f(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}

void glColor4x(GLfixed r, GLfixed g, GLfixed b, GLfixed a)
{
    glColor4f(es1_x2f(r), es1_x2f(g), es1_x2f(b), es1_x2f(a));
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    float u[3] = { nx, ny, nz };
    es1_ensure();
    memcpy(S.normal, u, 12);
    send_f(OP_ES1_NORMAL, u, 3);
}

void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
{
    glNormal3f(es1_x2f(nx), es1_x2f(ny), es1_x2f(nz));
}

void glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    float u[4] = { s, t, r, q };
    (void)target;
    es1_ensure();
    memcpy(S.texcoord, u, 16);
    send_f(OP_ES1_TEXCOORD, u, 4);
}

void glMultiTexCoord4x(GLenum texture, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
{
    glMultiTexCoord4f(texture, es1_x2f(s), es1_x2f(t), es1_x2f(r), es1_x2f(q));
}

void glClientActiveTexture(GLenum texture)
{
    es1_ensure();
    S.client_tex = (int)(texture >= 0x84C0 ? texture - 0x84C0 : 0);
    send_u32(OP_ES1_CLIENT_TEX, (uint32_t)S.client_tex);
}

void tspgl_es1_set_active_tex(uint32_t texture)
{
    /* Server tracks the unit; client only needs ClientActiveTexture for arrays. */
    (void)texture;
}

static unsigned array_index(GLenum array)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        return 0;
    case GL_COLOR_ARRAY:
        return 1;
    case GL_NORMAL_ARRAY:
        return 2;
    case GL_TEXTURE_COORD_ARRAY:
        return S.client_tex == 1 ? 6 : 3;
    case GL_WEIGHT_ARRAY_OES:
        return 4;
    case GL_MATRIX_INDEX_ARRAY_OES:
        return 5;
    default:
        return 16;
    }
}

void glEnableClientState(GLenum array)
{
    unsigned i = array_index(array);
    es1_ensure();
    if (i < 16)
        tspgl_es1_enable_array(i, 1);
}

void glDisableClientState(GLenum array)
{
    unsigned i = array_index(array);
    es1_ensure();
    if (i < 16)
        tspgl_es1_enable_array(i, 0);
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    static int nlog;
    es1_ensure();
    if (nlog < 6) {
        fprintf(stderr, "tspgl: VertexPointer size=%d type=0x%x stride=%d ptr=%p\n",
                (int)size, type, (int)stride, pointer);
        fflush(stderr);
        nlog++;
    }
    S.ptr_vertex = (void *)pointer;
    tspgl_es1_set_pointer(0, size, type, 0, stride, pointer);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    int norm = (type != GL_FLOAT);
    es1_ensure();
    S.ptr_color = (void *)pointer;
    tspgl_es1_set_pointer(1, size, type, norm, stride, pointer);
}

void glNormalPointer(GLenum type, GLsizei stride, const void *pointer)
{
    int norm = (type == 0x1400 || type == 0x1402); /* BYTE / SHORT */
    es1_ensure();
    S.ptr_normal = (void *)pointer;
    tspgl_es1_set_pointer(2, 3, type, norm, stride, pointer);
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    unsigned idx;
    es1_ensure();
    S.ptr_tex = (void *)pointer;
    idx = (S.client_tex == 1) ? 6u : 3u;
    tspgl_es1_set_pointer(idx, size, type, 0, stride, pointer);
}

void glWeightPointerOES(GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    int norm = (type == GL_UNSIGNED_BYTE);
    es1_ensure();
    S.ptr_weight = (void *)pointer;
    tspgl_es1_set_pointer(4, size, type, norm, stride, pointer);
}

void glMatrixIndexPointerOES(GLint size, GLenum type, GLsizei stride,
                             const void *pointer)
{
    es1_ensure();
    S.ptr_midx = (void *)pointer;
    tspgl_es1_set_pointer(5, size, type, 0, stride, pointer);
}

void glGetPointerv(GLenum pname, void **params)
{
    es1_ensure();
    if (!params)
        return;
    switch (pname) {
    case 0x808E:
        *params = S.ptr_vertex;
        break;
    case 0x8090:
        *params = S.ptr_normal;
        break;
    case 0x8092:
        *params = S.ptr_color;
        break;
    case 0x8092 + 2:
    case 0x8096:
        *params = S.ptr_tex;
        break;
    default:
        *params = NULL;
        break;
    }
}

void glAlphaFunc(GLenum func, GLfloat ref)
{
    float u[2];
    es1_ensure();
    S.alpha_func = func;
    S.alpha_ref = ref;
    u[0] = tspgl_unpack_f32(func);
    (void)u;
    send_u32_f(OP_ES1_ALPHA_FUNC, func, &ref, 1);
}

void glAlphaFuncx(GLenum func, GLfixed ref)
{
    glAlphaFunc(func, es1_x2f(ref));
}

void glShadeModel(GLenum mode)
{
    es1_ensure();
    S.shade = mode;
    send_u32(OP_ES1_SHADE_MODEL, mode);
}

void glPointSize(GLfloat size)
{
    send_f(OP_ES1_POINT_SIZE, &size, 1);
}

void glPointSizex(GLfixed size)
{
    glPointSize(es1_x2f(size));
}

void glPointSizePointerOES(GLenum type, GLsizei stride, const void *pointer)
{
    (void)type;
    (void)stride;
    (void)pointer;
}

void glPointParameterf(GLenum pname, GLfloat param)
{
    (void)pname;
    (void)param;
}

void glPointParameterfv(GLenum pname, const GLfloat *params)
{
    (void)pname;
    (void)params;
}

void glPointParameterx(GLenum pname, GLfixed param)
{
    (void)pname;
    (void)param;
}

void glPointParameterxv(GLenum pname, const GLfixed *params)
{
    (void)pname;
    (void)params;
}

static void light_apply(struct es1_light *L, GLenum pname, const float *p, int n)
{
    switch (pname) {
    case GL_AMBIENT:
        memcpy(L->ambient, p, 16);
        break;
    case GL_DIFFUSE:
        memcpy(L->diffuse, p, 16);
        break;
    case GL_SPECULAR:
        memcpy(L->specular, p, 16);
        break;
    case GL_POSITION:
        memcpy(L->position, p, 16);
        break;
    case GL_SPOT_DIRECTION:
        memcpy(L->spot_dir, p, 12);
        break;
    case GL_SPOT_EXPONENT:
        L->spot_exp = p[0];
        break;
    case GL_SPOT_CUTOFF:
        L->spot_cut = p[0];
        break;
    case GL_CONSTANT_ATTENUATION:
        L->att[0] = p[0];
        break;
    case GL_LINEAR_ATTENUATION:
        L->att[1] = p[0];
        break;
    case GL_QUADRATIC_ATTENUATION:
        L->att[2] = p[0];
        break;
    default:
        break;
    }
    (void)n;
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    float p[4] = { 0, 0, 0, 1 };
    int i;
    es1_ensure();
    if (!params || light < GL_LIGHT0 || light > GL_LIGHT0 + 7)
        return;
    i = (int)(light - GL_LIGHT0);
    if (pname == GL_SPOT_DIRECTION)
        memcpy(p, params, 12);
    else if (pname == GL_AMBIENT || pname == GL_DIFFUSE || pname == GL_SPECULAR ||
             pname == GL_POSITION)
        memcpy(p, params, 16);
    else
        p[0] = params[0];
    light_apply(&S.light[i], pname, p, 4);
    send_u32_2_f(OP_ES1_LIGHT, light, pname, p, 4);
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    glLightfv(light, pname, &param);
}

void glLightxv(GLenum light, GLenum pname, const GLfixed *params)
{
    float p[4];
    int i;
    if (!params)
        return;
    for (i = 0; i < 4; ++i)
        p[i] = es1_x2f(params[i]);
    glLightfv(light, pname, p);
}

void glLightx(GLenum light, GLenum pname, GLfixed param)
{
    glLightf(light, pname, es1_x2f(param));
}

void glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
{
    struct es1_light *L;
    es1_ensure();
    if (!params || light < GL_LIGHT0 || light > GL_LIGHT0 + 7)
        return;
    L = &S.light[light - GL_LIGHT0];
    switch (pname) {
    case GL_AMBIENT:
        memcpy(params, L->ambient, 16);
        break;
    case GL_DIFFUSE:
        memcpy(params, L->diffuse, 16);
        break;
    case GL_SPECULAR:
        memcpy(params, L->specular, 16);
        break;
    case GL_POSITION:
        memcpy(params, L->position, 16);
        break;
    case GL_SPOT_DIRECTION:
        memcpy(params, L->spot_dir, 12);
        break;
    case GL_SPOT_EXPONENT:
        params[0] = L->spot_exp;
        break;
    case GL_SPOT_CUTOFF:
        params[0] = L->spot_cut;
        break;
    case GL_CONSTANT_ATTENUATION:
        params[0] = L->att[0];
        break;
    case GL_LINEAR_ATTENUATION:
        params[0] = L->att[1];
        break;
    case GL_QUADRATIC_ATTENUATION:
        params[0] = L->att[2];
        break;
    default:
        params[0] = 0;
        break;
    }
}

void glGetLightxv(GLenum light, GLenum pname, GLfixed *params)
{
    float f[4];
    int i;
    glGetLightfv(light, pname, f);
    if (!params)
        return;
    for (i = 0; i < 4; ++i)
        params[i] = (GLfixed)(f[i] * 65536.f);
}

void glLightModelfv(GLenum pname, const GLfloat *params)
{
    float p[4] = { 0, 0, 0, 1 };
    es1_ensure();
    if (!params)
        return;
    if (pname == GL_LIGHT_MODEL_AMBIENT) {
        memcpy(S.scene_amb, params, 16);
        memcpy(p, params, 16);
    } else if (pname == GL_LIGHT_MODEL_TWO_SIDE) {
        S.two_side = params[0] != 0.f;
        p[0] = params[0];
    }
    send_u32_f(OP_ES1_LIGHT_MODEL, pname, p, 4);
}

void glLightModelf(GLenum pname, GLfloat param)
{
    glLightModelfv(pname, &param);
}

void glLightModelxv(GLenum pname, const GLfixed *param)
{
    float p[4];
    int i;
    if (!param)
        return;
    for (i = 0; i < 4; ++i)
        p[i] = es1_x2f(param[i]);
    glLightModelfv(pname, p);
}

void glLightModelx(GLenum pname, GLfixed param)
{
    glLightModelf(pname, es1_x2f(param));
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    float p[4] = { 0, 0, 0, 1 };
    es1_ensure();
    if (!params)
        return;
    if (pname == GL_SHININESS) {
        S.shininess = params[0];
        p[0] = params[0];
    } else if (pname == GL_AMBIENT || pname == GL_AMBIENT_AND_DIFFUSE) {
        memcpy(S.mat_amb, params, 16);
        memcpy(p, params, 16);
        if (pname == GL_AMBIENT_AND_DIFFUSE)
            memcpy(S.mat_diff, params, 16);
    } else if (pname == GL_DIFFUSE) {
        memcpy(S.mat_diff, params, 16);
        memcpy(p, params, 16);
    } else if (pname == GL_SPECULAR) {
        memcpy(S.mat_spec, params, 16);
        memcpy(p, params, 16);
    } else if (pname == GL_EMISSION) {
        memcpy(S.mat_emis, params, 16);
        memcpy(p, params, 16);
    }
    send_u32_2_f(OP_ES1_MATERIAL, face, pname, p, 4);
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    glMaterialfv(face, pname, &param);
}

void glMaterialxv(GLenum face, GLenum pname, const GLfixed *param)
{
    float p[4];
    int i;
    if (!param)
        return;
    for (i = 0; i < 4; ++i)
        p[i] = es1_x2f(param[i]);
    glMaterialfv(face, pname, p);
}

void glMaterialx(GLenum face, GLenum pname, GLfixed param)
{
    glMaterialf(face, pname, es1_x2f(param));
}

void glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
    (void)face;
    es1_ensure();
    if (!params)
        return;
    switch (pname) {
    case GL_AMBIENT:
        memcpy(params, S.mat_amb, 16);
        break;
    case GL_DIFFUSE:
        memcpy(params, S.mat_diff, 16);
        break;
    case GL_SPECULAR:
        memcpy(params, S.mat_spec, 16);
        break;
    case GL_EMISSION:
        memcpy(params, S.mat_emis, 16);
        break;
    case GL_SHININESS:
        params[0] = S.shininess;
        break;
    default:
        params[0] = 0;
        break;
    }
}

void glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params)
{
    float f[4];
    int i;
    glGetMaterialfv(face, pname, f);
    if (!params)
        return;
    for (i = 0; i < 4; ++i)
        params[i] = (GLfixed)(f[i] * 65536.f);
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    float p = (float)param;
    es1_ensure();
    if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE)
        S.texenv_mode = (uint32_t)param;
    send_u32_2_f(OP_ES1_TEXENV, target, pname, &p, 1);
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    if (pname == GL_TEXTURE_ENV_MODE)
        glTexEnvi(target, pname, (GLint)param);
    else {
        es1_ensure();
        send_u32_2_f(OP_ES1_TEXENV, target, pname, &param, 1);
    }
}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
    es1_ensure();
    if (!params)
        return;
    if (pname == GL_TEXTURE_ENV_COLOR)
        memcpy(S.texenv_color, params, 16);
    else if (pname == GL_TEXTURE_ENV_MODE)
        S.texenv_mode = (uint32_t)params[0];
    send_u32_2_f(OP_ES1_TEXENV, target, pname, params, 4);
}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
    if (params)
        glTexEnvi(target, pname, params[0]);
}

void glTexEnvx(GLenum target, GLenum pname, GLfixed param)
{
    if (pname == GL_TEXTURE_ENV_MODE)
        glTexEnvi(target, pname, param);
    else
        glTexEnvf(target, pname, es1_x2f(param));
}

void glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params)
{
    float p[4];
    int i;
    if (!params)
        return;
    for (i = 0; i < 4; ++i)
        p[i] = es1_x2f(params[i]);
    glTexEnvfv(target, pname, p);
}

void glGetTexEnviv(GLenum target, GLenum pname, GLint *params)
{
    (void)target;
    es1_ensure();
    if (!params)
        return;
    if (pname == GL_TEXTURE_ENV_MODE)
        params[0] = (GLint)S.texenv_mode;
    else
        params[0] = 0;
}

void glGetTexEnvfv(GLenum target, GLenum pname, GLfloat *params)
{
    (void)target;
    es1_ensure();
    if (!params)
        return;
    if (pname == GL_TEXTURE_ENV_COLOR)
        memcpy(params, S.texenv_color, 16);
    else if (pname == GL_TEXTURE_ENV_MODE)
        params[0] = (float)S.texenv_mode;
}

void glGetTexEnvxv(GLenum target, GLenum pname, GLfixed *params)
{
    float f[4];
    int i;
    glGetTexEnvfv(target, pname, f);
    if (!params)
        return;
    for (i = 0; i < 4; ++i)
        params[i] = (GLfixed)(f[i] * 65536.f);
}

void glFogf(GLenum pname, GLfloat param)
{
    es1_ensure();
    if (pname == GL_FOG_MODE)
        S.fog_mode = (uint32_t)param;
    else if (pname == GL_FOG_DENSITY)
        S.fog_density = param;
    else if (pname == GL_FOG_START)
        S.fog_start = param;
    else if (pname == GL_FOG_END)
        S.fog_end = param;
    send_u32_f(OP_ES1_FOG, pname, &param, 1);
}

void glFogfv(GLenum pname, const GLfloat *params)
{
    es1_ensure();
    if (!params)
        return;
    if (pname == GL_FOG_COLOR)
        memcpy(S.fog_color, params, 16);
    else
        glFogf(pname, params[0]);
    if (pname == GL_FOG_COLOR)
        send_u32_f(OP_ES1_FOG, pname, params, 4);
}

void glFogx(GLenum pname, GLfixed param)
{
    if (pname == GL_FOG_MODE)
        glFogf(pname, (float)param);
    else
        glFogf(pname, es1_x2f(param));
}

void glFogxv(GLenum pname, const GLfixed *param)
{
    float p[4];
    int i;
    if (!param)
        return;
    for (i = 0; i < 4; ++i)
        p[i] = es1_x2f(param[i]);
    if (pname == GL_FOG_MODE)
        glFogf(pname, (float)param[0]);
    else
        glFogfv(pname, p);
}

void glCurrentPaletteMatrixOES(GLuint index)
{
    es1_ensure();
    S.pal_i = (int)(index < 16 ? index : 15);
    send_u32(OP_ES1_PALETTE_CURRENT, index);
}

void glLoadPaletteFromModelViewMatrixOES(void)
{
    es1_ensure();
    es1_copy(S.palette[S.pal_i], S.stack[ES1_MV][S.depth[ES1_MV]]);
    tspgl_call(OP_ES1_PALETTE_LOAD, NULL, 0, NULL, 0);
}

void glLogicOp(GLenum opcode)
{
    (void)opcode;
}

GLbitfield glQueryMatrixxOES(GLfixed *mantissa, GLint *exponent)
{
    float m[16];
    int i;
    es1_ensure();
    memcpy(m, cur_mat(), 64);
    if (mantissa)
        for (i = 0; i < 16; ++i)
            mantissa[i] = (GLfixed)(m[i] * 65536.f);
    if (exponent)
        for (i = 0; i < 16; ++i)
            exponent[i] = 0;
    return 0;
}

void glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat h)
{
    float u[5] = { x, y, z, w, h };
    static int nlog;
    if (nlog < 8) {
        fprintf(stderr, "tspgl: DrawTex %g,%g,%g %gx%g\n", (double)x, (double)y,
                (double)z, (double)w, (double)h);
        fflush(stderr);
        nlog++;
    }
    send_f(OP_ES1_DRAWTEX, u, 5);
}

void glDrawTexfvOES(const GLfloat *coords)
{
    if (coords)
        glDrawTexfOES(coords[0], coords[1], coords[2], coords[3], coords[4]);
}

void glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height)
{
    glDrawTexfOES((float)x, (float)y, (float)z, (float)width, (float)height);
}

void glDrawTexivOES(const GLint *coords)
{
    if (coords)
        glDrawTexiOES(coords[0], coords[1], coords[2], coords[3], coords[4]);
}

void glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height)
{
    glDrawTexfOES((float)x, (float)y, (float)z, (float)width, (float)height);
}

void glDrawTexsvOES(const GLshort *coords)
{
    if (coords)
        glDrawTexsOES(coords[0], coords[1], coords[2], coords[3], coords[4]);
}

void glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
    glDrawTexfOES(es1_x2f(x), es1_x2f(y), es1_x2f(z), es1_x2f(width),
                  es1_x2f(height));
}

void glDrawTexxvOES(const GLfixed *coords)
{
    if (coords)
        glDrawTexxOES(coords[0], coords[1], coords[2], coords[3], coords[4]);
}

/* ---- GLES2 aliases / fixed-point wrappers that the .so already exports ---- */
extern void glBindFramebuffer(GLenum, GLuint);
extern void glBindRenderbuffer(GLenum, GLuint);
extern void glBlendEquation(GLenum);
extern void glBlendEquationSeparate(GLenum, GLenum);
extern void glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum);
extern GLenum glCheckFramebufferStatus(GLenum);
extern void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
extern void glClearDepthf(GLfloat);
extern void glDepthRangef(GLfloat, GLfloat);
extern void glDeleteFramebuffers(GLsizei, const GLuint *);
extern void glDeleteRenderbuffers(GLsizei, const GLuint *);
extern void glFramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint);
extern void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint);
extern void glGenFramebuffers(GLsizei, GLuint *);
extern void glGenRenderbuffers(GLsizei, GLuint *);
extern void glGenerateMipmap(GLenum);
extern void glGetFramebufferAttachmentParameteriv(GLenum, GLenum, GLenum, GLint *);
extern void glGetRenderbufferParameteriv(GLenum, GLenum, GLint *);
extern GLboolean glIsFramebuffer(GLuint);
extern GLboolean glIsRenderbuffer(GLuint);
extern void glLineWidth(GLfloat);
extern void glPolygonOffset(GLfloat, GLfloat);
extern void glRenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei);
extern void glSampleCoverage(GLfloat, GLboolean);
extern void glTexParameterf(GLenum, GLenum, GLfloat);
extern void glTexParameterfv(GLenum, GLenum, const GLfloat *);
extern void glGetTexParameterfv(GLenum, GLenum, GLfloat *);
extern void glGetTexParameteriv(GLenum, GLenum, GLint *);

void glBindFramebufferOES(GLenum t, GLuint fb) { glBindFramebuffer(t, fb); }
void glBindRenderbufferOES(GLenum t, GLuint rb) { glBindRenderbuffer(t, rb); }
void glBlendEquationOES(GLenum m) { glBlendEquation(m); }
void glBlendEquationSeparateOES(GLenum a, GLenum b) { glBlendEquationSeparate(a, b); }
void glBlendFuncSeparateOES(GLenum a, GLenum b, GLenum c, GLenum d)
{
    glBlendFuncSeparate(a, b, c, d);
}
GLenum glCheckFramebufferStatusOES(GLenum t) { return glCheckFramebufferStatus(t); }
void glClearColorx(GLfixed r, GLfixed g, GLfixed b, GLfixed a)
{
    glClearColor(es1_x2f(r), es1_x2f(g), es1_x2f(b), es1_x2f(a));
}
void glClearDepthx(GLfixed d) { glClearDepthf(es1_x2f(d)); }
void glDepthRangex(GLfixed n, GLfixed f) { glDepthRangef(es1_x2f(n), es1_x2f(f)); }
void glDeleteFramebuffersOES(GLsizei n, const GLuint *ids)
{
    glDeleteFramebuffers(n, ids);
}
void glDeleteRenderbuffersOES(GLsizei n, const GLuint *ids)
{
    glDeleteRenderbuffers(n, ids);
}
void glFramebufferRenderbufferOES(GLenum a, GLenum b, GLenum c, GLuint d)
{
    glFramebufferRenderbuffer(a, b, c, d);
}
void glFramebufferTexture2DOES(GLenum a, GLenum b, GLenum c, GLuint d, GLint e)
{
    glFramebufferTexture2D(a, b, c, d, e);
}
void glGenFramebuffersOES(GLsizei n, GLuint *ids) { glGenFramebuffers(n, ids); }
void glGenRenderbuffersOES(GLsizei n, GLuint *ids) { glGenRenderbuffers(n, ids); }
void glGenerateMipmapOES(GLenum t) { glGenerateMipmap(t); }
void glGetFramebufferAttachmentParameterivOES(GLenum a, GLenum b, GLenum c,
                                              GLint *d)
{
    glGetFramebufferAttachmentParameteriv(a, b, c, d);
}
void glGetRenderbufferParameterivOES(GLenum a, GLenum b, GLint *c)
{
    glGetRenderbufferParameteriv(a, b, c);
}
GLboolean glIsFramebufferOES(GLuint x) { return glIsFramebuffer(x); }
GLboolean glIsRenderbufferOES(GLuint x) { return glIsRenderbuffer(x); }
void glLineWidthx(GLfixed w) { glLineWidth(es1_x2f(w)); }
void glPolygonOffsetx(GLfixed a, GLfixed b)
{
    glPolygonOffset(es1_x2f(a), es1_x2f(b));
}
void glRenderbufferStorageOES(GLenum a, GLenum b, GLsizei c, GLsizei d)
{
    glRenderbufferStorage(a, b, c, d);
}
void glSampleCoveragex(GLfixed v, GLboolean inv)
{
    glSampleCoverage(es1_x2f(v), inv);
}
void glTexParameterx(GLenum t, GLenum p, GLfixed v)
{
    glTexParameterf(t, p, es1_x2f(v));
}
void glTexParameterxv(GLenum t, GLenum p, const GLfixed *v)
{
    float f[4] = { 0, 0, 0, 0 };
    int i;
    if (v)
        for (i = 0; i < 4; ++i)
            f[i] = es1_x2f(v[i]);
    glTexParameterfv(t, p, f);
}
void glGetTexParameterxv(GLenum t, GLenum p, GLfixed *params)
{
    float f[4];
    int i;
    glGetTexParameterfv(t, p, f);
    if (params)
        for (i = 0; i < 4; ++i)
            params[i] = (GLfixed)(f[i] * 65536.f);
}

void glEGLImageTargetRenderbufferStorageOES(GLenum target, void *image)
{
    (void)target;
    (void)image;
}
void glEGLImageTargetTexture2DOES(GLenum target, void *image)
{
    (void)target;
    (void)image;
}

void glTexGenfOES(GLenum a, GLenum b, GLfloat c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glTexGenfvOES(GLenum a, GLenum b, const GLfloat *c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glTexGeniOES(GLenum a, GLenum b, GLint c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glTexGenivOES(GLenum a, GLenum b, const GLint *c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glTexGenxOES(GLenum a, GLenum b, GLfixed c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glTexGenxvOES(GLenum a, GLenum b, const GLfixed *c)
{
    (void)a;
    (void)b;
    (void)c;
}
void glGetTexGenfvOES(GLenum a, GLenum b, GLfloat *c)
{
    (void)a;
    (void)b;
    if (c)
        c[0] = 0;
}
void glGetTexGenivOES(GLenum a, GLenum b, GLint *c)
{
    (void)a;
    (void)b;
    if (c)
        c[0] = 0;
}
void glGetTexGenxvOES(GLenum a, GLenum b, GLfixed *c)
{
    (void)a;
    (void)b;
    if (c)
        c[0] = 0;
}

void glGetClipPlanef(GLenum p, GLfloat *e)
{
    (void)p;
    if (e) {
        e[0] = e[1] = e[2] = 0;
        e[3] = 0;
    }
}
void glGetClipPlanex(GLenum p, GLfixed *e)
{
    (void)p;
    if (e)
        e[0] = e[1] = e[2] = e[3] = 0;
}

/* libMassEffect.so (v1.0.58) ELF-imports OES_texture_3D. Dead Space did not.
 * Export no-op symbols so the loader can link. Log if the game actually calls. */
static void es1_stub3d(const char *name)
{
    fprintf(stderr, "tspgl: stub %s (OES_texture_3D)\n", name);
}

void glTexImage3DOES(GLenum target, GLint level, GLenum internalformat,
                     GLsizei width, GLsizei height, GLsizei depth, GLint border,
                     GLenum format, GLenum type, const void *pixels)
{
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)format;
    (void)type;
    (void)pixels;
    es1_stub3d("glTexImage3DOES");
}
void glTexImage3D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum format, GLenum type, const void *pixels)
{
    glTexImage3DOES(target, level, (GLenum)internalformat, width, height, depth,
                    border, format, type, pixels);
}
void glTexSubImage3DOES(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                        GLint zoffset, GLsizei width, GLsizei height,
                        GLsizei depth, GLenum format, GLenum type,
                        const void *pixels)
{
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    (void)width;
    (void)height;
    (void)depth;
    (void)format;
    (void)type;
    (void)pixels;
    es1_stub3d("glTexSubImage3DOES");
}
void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                     GLenum format, GLenum type, const void *pixels)
{
    glTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, width, height,
                       depth, format, type, pixels);
}
void glCompressedTexImage3DOES(GLenum target, GLint level, GLenum internalformat,
                               GLsizei width, GLsizei height, GLsizei depth,
                               GLint border, GLsizei imageSize, const void *data)
{
    (void)target;
    (void)level;
    (void)internalformat;
    (void)width;
    (void)height;
    (void)depth;
    (void)border;
    (void)imageSize;
    (void)data;
    es1_stub3d("glCompressedTexImage3DOES");
}
void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                            GLsizei width, GLsizei height, GLsizei depth,
                            GLint border, GLsizei imageSize, const void *data)
{
    glCompressedTexImage3DOES(target, level, internalformat, width, height, depth,
                              border, imageSize, data);
}
void glCompressedTexSubImage3DOES(GLenum target, GLint level, GLint xoffset,
                                  GLint yoffset, GLint zoffset, GLsizei width,
                                  GLsizei height, GLsizei depth, GLenum format,
                                  GLsizei imageSize, const void *data)
{
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    (void)width;
    (void)height;
    (void)depth;
    (void)format;
    (void)imageSize;
    (void)data;
    es1_stub3d("glCompressedTexSubImage3DOES");
}
void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                               GLint yoffset, GLint zoffset, GLsizei width,
                               GLsizei height, GLsizei depth, GLenum format,
                               GLsizei imageSize, const void *data)
{
    glCompressedTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, width,
                                 height, depth, format, imageSize, data);
}
void glCopyTexSubImage3DOES(GLenum target, GLint level, GLint xoffset,
                            GLint yoffset, GLint zoffset, GLint x, GLint y,
                            GLsizei width, GLsizei height)
{
    (void)target;
    (void)level;
    (void)xoffset;
    (void)yoffset;
    (void)zoffset;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    es1_stub3d("glCopyTexSubImage3DOES");
}
void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                         GLint zoffset, GLint x, GLint y, GLsizei width,
                         GLsizei height)
{
    glCopyTexSubImage3DOES(target, level, xoffset, yoffset, zoffset, x, y, width,
                           height);
}
void glFramebufferTexture3DOES(GLenum target, GLenum attachment, GLenum textarget,
                               GLuint texture, GLint level, GLint zoffset)
{
    (void)target;
    (void)attachment;
    (void)textarget;
    (void)texture;
    (void)level;
    (void)zoffset;
    es1_stub3d("glFramebufferTexture3DOES");
}

void glGetFixedv(GLenum pname, GLfixed *params);

#define FN(n) { #n, (void *)n }
static const struct {
    const char *name;
    void *fn;
} es1_procs[] = {
    FN(glAlphaFunc), FN(glAlphaFuncx), FN(glBindFramebufferOES),
    FN(glBindRenderbufferOES), FN(glBlendEquationOES),
    FN(glBlendEquationSeparateOES), FN(glBlendFuncSeparateOES),
    FN(glCheckFramebufferStatusOES), FN(glClearColorx), FN(glClearDepthx),
    FN(glClientActiveTexture), FN(glClipPlanef), FN(glClipPlanex), FN(glColor4f),
    FN(glColor4ub), FN(glColor4x), FN(glColorPointer),
    FN(glCurrentPaletteMatrixOES), FN(glDeleteFramebuffersOES),
    FN(glDeleteRenderbuffersOES), FN(glDepthRangex), FN(glDisableClientState),
    FN(glDrawTexfOES), FN(glDrawTexfvOES), FN(glDrawTexiOES), FN(glDrawTexivOES),
    FN(glDrawTexsOES), FN(glDrawTexsvOES), FN(glDrawTexxOES), FN(glDrawTexxvOES),
    FN(glEGLImageTargetRenderbufferStorageOES), FN(glEGLImageTargetTexture2DOES),
    FN(glEnableClientState), FN(glFogf), FN(glFogfv), FN(glFogx), FN(glFogxv),
    FN(glFramebufferRenderbufferOES), FN(glFramebufferTexture2DOES),
    FN(glFramebufferTexture3DOES), FN(glTexImage3D), FN(glTexImage3DOES),
    FN(glTexSubImage3D), FN(glTexSubImage3DOES),
    FN(glCompressedTexImage3D), FN(glCompressedTexImage3DOES),
    FN(glCompressedTexSubImage3D), FN(glCompressedTexSubImage3DOES),
    FN(glCopyTexSubImage3D), FN(glCopyTexSubImage3DOES),
    FN(glFrustumf), FN(glFrustumx), FN(glGenFramebuffersOES),
    FN(glGenRenderbuffersOES), FN(glGenerateMipmapOES), FN(glGetClipPlanef),
    FN(glGetClipPlanex), FN(glGetFixedv), FN(glGetLightfv), FN(glGetLightxv),
    FN(glGetMaterialfv), FN(glGetMaterialxv), FN(glGetPointerv),
    FN(glGetFramebufferAttachmentParameterivOES),
    FN(glGetRenderbufferParameterivOES), FN(glGetTexEnvfv), FN(glGetTexEnviv),
    FN(glGetTexEnvxv), FN(glGetTexGenfvOES), FN(glGetTexGenivOES),
    FN(glGetTexGenxvOES), FN(glGetTexParameterxv), FN(glIsFramebufferOES),
    FN(glIsRenderbufferOES), FN(glLightModelf), FN(glLightModelfv),
    FN(glLightModelx), FN(glLightModelxv), FN(glLightf), FN(glLightfv),
    FN(glLightx), FN(glLightxv), FN(glLineWidthx), FN(glLoadIdentity),
    FN(glLoadMatrixf), FN(glLoadMatrixx), FN(glLoadPaletteFromModelViewMatrixOES),
    FN(glLogicOp), FN(glMaterialf), FN(glMaterialfv), FN(glMaterialx),
    FN(glMaterialxv), FN(glMatrixIndexPointerOES), FN(glMatrixMode),
    FN(glMultMatrixf), FN(glMultMatrixx), FN(glMultiTexCoord4f),
    FN(glMultiTexCoord4x), FN(glNormal3f), FN(glNormal3x), FN(glNormalPointer),
    FN(glOrthof), FN(glOrthox), FN(glPointParameterf), FN(glPointParameterfv),
    FN(glPointParameterx), FN(glPointParameterxv), FN(glPointSize),
    FN(glPointSizePointerOES), FN(glPointSizex), FN(glPolygonOffsetx),
    FN(glPopMatrix), FN(glPushMatrix), FN(glQueryMatrixxOES),
    FN(glRenderbufferStorageOES), FN(glRotatef), FN(glRotatex),
    FN(glSampleCoveragex), FN(glScalef), FN(glScalex), FN(glShadeModel),
    FN(glTexCoordPointer), FN(glTexEnvf), FN(glTexEnvfv), FN(glTexEnvi),
    FN(glTexEnviv), FN(glTexEnvx), FN(glTexEnvxv), FN(glTexGenfOES),
    FN(glTexGenfvOES), FN(glTexGeniOES), FN(glTexGenivOES), FN(glTexGenxOES),
    FN(glTexGenxvOES), FN(glTexParameterx), FN(glTexParameterxv), FN(glTranslatef),
    FN(glTranslatex), FN(glVertexPointer), FN(glWeightPointerOES),
    { NULL, NULL }
};

void glGetFixedv(GLenum pname, GLfixed *params)
{
    tspgl_es1_get_fixedv(pname, params);
}

void *tspgl_es1_get_proc(const char *name)
{
    size_t i;
    if (!name)
        return NULL;
    for (i = 0; es1_procs[i].name; ++i) {
        if (strcmp(es1_procs[i].name, name) == 0)
            return es1_procs[i].fn;
    }
    return NULL;
}
