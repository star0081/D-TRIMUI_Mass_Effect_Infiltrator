/* Qualcomm ATC → RGBA8888. Same block layout as AMD_compressed_ATC_texture.
 * Used so the 32-bit loader can pass compressed data instead of decoding
 * every mip on the ARMHF core and logging TRACE lines to the SD card. */

#define GL_ATC_RGB_AMD 0x8C92
#define GL_ATC_RGBA_EXPLICIT_ALPHA_AMD 0x8C93
#define GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD 0x87EE

static uint8_t atc_expand(uint8_t v, int bits)
{
    v = (uint8_t)(v << (8 - bits));
    return (uint8_t)(v | (v >> bits));
}

static uint32_t atc_pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) |
           ((uint32_t)a << 24);
}

static void atc_color_block(const uint8_t *src, uint32_t *dst)
{
    uint8_t c[16];
    uint32_t c0 = (uint32_t)src[0] | ((uint32_t)src[1] << 8);
    uint32_t c1 = (uint32_t)src[2] | ((uint32_t)src[3] << 8);
    uint32_t i, next;

    memset(c, 0, sizeof(c));
    if ((c0 & 0x8000u) == 0) {
        c[0] = atc_expand((uint8_t)((c0 >> 0) & 0x1f), 5);
        c[1] = atc_expand((uint8_t)((c0 >> 5) & 0x1f), 5);
        c[2] = atc_expand((uint8_t)((c0 >> 10) & 0x1f), 5);
        c[12] = atc_expand((uint8_t)((c1 >> 0) & 0x1f), 5);
        c[13] = atc_expand((uint8_t)((c1 >> 5) & 0x3f), 6);
        c[14] = atc_expand((uint8_t)((c1 >> 11) & 0x1f), 5);
        c[4] = (uint8_t)((5 * c[0] + 3 * c[12]) / 8);
        c[5] = (uint8_t)((5 * c[1] + 3 * c[13]) / 8);
        c[6] = (uint8_t)((5 * c[2] + 3 * c[14]) / 8);
        c[8] = (uint8_t)((3 * c[0] + 5 * c[12]) / 8);
        c[9] = (uint8_t)((3 * c[1] + 5 * c[13]) / 8);
        c[10] = (uint8_t)((3 * c[2] + 5 * c[14]) / 8);
    } else {
        c[8] = atc_expand((uint8_t)((c0 >> 0) & 0x1f), 5);
        c[9] = atc_expand((uint8_t)((c0 >> 5) & 0x1f), 5);
        c[10] = atc_expand((uint8_t)((c0 >> 10) & 0x1f), 5);
        c[12] = atc_expand((uint8_t)((c1 >> 0) & 0x1f), 5);
        c[13] = atc_expand((uint8_t)((c1 >> 5) & 0x3f), 6);
        c[14] = atc_expand((uint8_t)((c1 >> 11) & 0x1f), 5);
        c[4] = (uint8_t)(c[8] > c[12] / 4 ? c[8] - c[12] / 4 : 0);
        c[5] = (uint8_t)(c[9] > c[13] / 4 ? c[9] - c[13] / 4 : 0);
        c[6] = (uint8_t)(c[10] > c[14] / 4 ? c[10] - c[14] / 4 : 0);
    }
    for (i = 0, next = 8u * 4u; i < 16u; i++, next += 2u) {
        uint32_t idx = ((src[next >> 3] >> (next & 7u)) & 3u) * 4u;
        dst[i] = atc_pack(c[idx + 0], c[idx + 1], c[idx + 2], 255);
    }
}

static void atc_explicit_alpha(const uint8_t *src, uint32_t *dst)
{
    uint32_t i;
    for (i = 0; i < 16u; i++) {
        uint8_t a4 = (i & 1u) ? (uint8_t)(src[i / 2u] >> 4) : (uint8_t)(src[i / 2u] & 0x0f);
        uint8_t a = (uint8_t)((a4 << 4) | a4);
        dst[i] = (dst[i] & 0x00ffffffu) | ((uint32_t)a << 24);
    }
}

static void atc_interp_alpha(const uint8_t *src, uint32_t *dst)
{
    uint8_t a[8];
    uint64_t bits;
    uint32_t i;

    a[0] = src[0];
    a[1] = src[1];
    if (a[0] > a[1]) {
        a[2] = (uint8_t)((6 * a[0] + 1 * a[1]) / 7);
        a[3] = (uint8_t)((5 * a[0] + 2 * a[1]) / 7);
        a[4] = (uint8_t)((4 * a[0] + 3 * a[1]) / 7);
        a[5] = (uint8_t)((3 * a[0] + 4 * a[1]) / 7);
        a[6] = (uint8_t)((2 * a[0] + 5 * a[1]) / 7);
        a[7] = (uint8_t)((1 * a[0] + 6 * a[1]) / 7);
    } else {
        a[2] = (uint8_t)((4 * a[0] + 1 * a[1]) / 5);
        a[3] = (uint8_t)((3 * a[0] + 2 * a[1]) / 5);
        a[4] = (uint8_t)((2 * a[0] + 3 * a[1]) / 5);
        a[5] = (uint8_t)((1 * a[0] + 4 * a[1]) / 5);
        a[6] = 0;
        a[7] = 255;
    }
    bits = (uint64_t)src[2] | ((uint64_t)src[3] << 8) | ((uint64_t)src[4] << 16) |
           ((uint64_t)src[5] << 24) | ((uint64_t)src[6] << 32) |
           ((uint64_t)src[7] << 40);
    for (i = 0; i < 16u; i++) {
        uint8_t av = a[bits & 7u];
        bits >>= 3;
        dst[i] = (dst[i] & 0x00ffffffu) | ((uint32_t)av << 24);
    }
}

static void atc_copy_block(uint32_t bx, uint32_t by, int w, int h,
                           const uint32_t *block, uint32_t *image)
{
    uint32_t x, y;
    for (y = 0; y < 4u; y++) {
        uint32_t py = by * 4u + y;
        if ((int)py >= h)
            break;
        for (x = 0; x < 4u; x++) {
            uint32_t px = bx * 4u + x;
            if ((int)px >= w)
                break;
            image[py * (uint32_t)w + px] = block[y * 4u + x];
        }
    }
}

static int atc_decode_rgba(uint32_t ifmt, int w, int h, const void *src,
                           int src_n, uint32_t *dst)
{
    uint32_t bx, by, nx, ny;
    const uint8_t *p = src;
    int block;

    if (w < 1 || h < 1 || !src || !dst)
        return -1;
    nx = ((uint32_t)w + 3u) / 4u;
    ny = ((uint32_t)h + 3u) / 4u;
    if (ifmt == GL_ATC_RGB_AMD)
        block = 8;
    else
        block = 16;
    if (src_n < (int)(nx * ny * (uint32_t)block))
        return -1;
    for (by = 0; by < ny; by++) {
        for (bx = 0; bx < nx; bx++) {
            uint32_t pix[16];
            if (ifmt == GL_ATC_RGB_AMD) {
                atc_color_block(p, pix);
                p += 8;
            } else if (ifmt == GL_ATC_RGBA_EXPLICIT_ALPHA_AMD) {
                atc_color_block(p + 8, pix);
                atc_explicit_alpha(p, pix);
                p += 16;
            } else {
                atc_color_block(p + 8, pix);
                atc_interp_alpha(p, pix);
                p += 16;
            }
            atc_copy_block(bx, by, w, h, pix, dst);
        }
    }
    return 0;
}

static int atc_is_fmt(uint32_t ifmt)
{
    return ifmt == GL_ATC_RGB_AMD || ifmt == GL_ATC_RGBA_EXPLICIT_ALPHA_AMD ||
           ifmt == GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD;
}
