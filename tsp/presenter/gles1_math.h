#ifndef TSPGL_GLES1_MATH_H
#define TSPGL_GLES1_MATH_H

#include <math.h>
#include <stdint.h>
#include <string.h>

static inline void es1_ident(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.f;
}

static inline void es1_copy(float *d, const float *s)
{
    memcpy(d, s, 16 * sizeof(float));
}

static inline void es1_mul(float *r, const float *a, const float *b)
{
    float t[16];
    int i, j;
    for (j = 0; j < 4; ++j) {
        for (i = 0; i < 4; ++i) {
            t[j * 4 + i] = a[i] * b[j * 4] + a[4 + i] * b[j * 4 + 1] +
                           a[8 + i] * b[j * 4 + 2] + a[12 + i] * b[j * 4 + 3];
        }
    }
    memcpy(r, t, sizeof(t));
}

static inline void es1_translate(float *m, float x, float y, float z)
{
    float t[16];
    es1_ident(t);
    t[12] = x;
    t[13] = y;
    t[14] = z;
    es1_mul(m, m, t);
}

static inline void es1_scale(float *m, float x, float y, float z)
{
    float t[16];
    es1_ident(t);
    t[0] = x;
    t[5] = y;
    t[10] = z;
    es1_mul(m, m, t);
}

static inline void es1_rotate(float *m, float deg, float x, float y, float z)
{
    float t[16], c, s, len, ic;
    float xx, yy, zz, xy, xz, yz, xs, ys, zs;
    len = sqrtf(x * x + y * y + z * z);
    if (len < 1e-8f)
        return;
    x /= len;
    y /= len;
    z /= len;
    deg *= 0.017453292519943295f;
    c = cosf(deg);
    s = sinf(deg);
    ic = 1.f - c;
    xx = x * x;
    yy = y * y;
    zz = z * z;
    xy = x * y;
    xz = x * z;
    yz = y * z;
    xs = x * s;
    ys = y * s;
    zs = z * s;
    es1_ident(t);
    t[0] = xx * ic + c;
    t[1] = xy * ic + zs;
    t[2] = xz * ic - ys;
    t[4] = xy * ic - zs;
    t[5] = yy * ic + c;
    t[6] = yz * ic + xs;
    t[8] = xz * ic + ys;
    t[9] = yz * ic - xs;
    t[10] = zz * ic + c;
    es1_mul(m, m, t);
}

static inline void es1_frustum(float *m, float l, float r, float b, float t,
                               float n, float f)
{
    float a[16];
    float rl = r - l, tb = t - b, fn = f - n;
    es1_ident(a);
    if (rl == 0.f || tb == 0.f || fn == 0.f || n == 0.f)
        return;
    a[0] = (2.f * n) / rl;
    a[5] = (2.f * n) / tb;
    a[8] = (r + l) / rl;
    a[9] = (t + b) / tb;
    a[10] = -(f + n) / fn;
    a[11] = -1.f;
    a[14] = -(2.f * f * n) / fn;
    a[15] = 0.f;
    es1_mul(m, m, a);
}

static inline void es1_ortho(float *m, float l, float r, float b, float t,
                             float n, float f)
{
    float a[16];
    float rl = r - l, tb = t - b, fn = f - n;
    es1_ident(a);
    if (rl == 0.f || tb == 0.f || fn == 0.f)
        return;
    a[0] = 2.f / rl;
    a[5] = 2.f / tb;
    a[10] = -2.f / fn;
    a[12] = -(r + l) / rl;
    a[13] = -(t + b) / tb;
    a[14] = -(f + n) / fn;
    es1_mul(m, m, a);
}

static inline void es1_normal_from_mv(float *n3, const float *mv)
{
    /* upper-left 3x3 inverse-transpose of affine modelview, no scale-safe. */
    n3[0] = mv[0];
    n3[1] = mv[1];
    n3[2] = mv[2];
    n3[3] = mv[4];
    n3[4] = mv[5];
    n3[5] = mv[6];
    n3[6] = mv[8];
    n3[7] = mv[9];
    n3[8] = mv[10];
}

static inline float es1_x2f(int32_t x)
{
    return (float)x / 65536.f;
}

#endif
