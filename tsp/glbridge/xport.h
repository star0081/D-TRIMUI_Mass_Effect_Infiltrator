#ifndef TSPGL_XPORT_H
#define TSPGL_XPORT_H

#include "protocol.h"
#include <stdint.h>
#include <string.h>

#define TSPGL_CMD_CAP (256u * 1024u)

/* One mmap for all copies of this .so (libEGL + libGLESv2 + SONAME
 * aliases). FAT32 has no symlinks, so the game loads four DSOs with
 * four static tspgl_fd's — four sockets into one GPU context, draws
 * beat texture uploads, road tiles stay black. */
struct tspgl_shared {
    uint32_t magic;
    int32_t pid;
    int lock;
    int sock;
    uint32_t seq;
    int fail_log;
    uint32_t cmd_used;
    uint8_t cmdbuf[TSPGL_CMD_CAP];
    unsigned active_tex;
    uint32_t tex_bind_2d[32];
    uint32_t tex_bind_cube[32];
    uint32_t bound_fb;
    uint32_t bound_read_fb;
    uint32_t bound_rb;
    uint32_t bound_array;
    uint32_t bound_element;
    uint32_t bound_vao;
    uint32_t cur_prog;
    int32_t st_viewport[4];
    int32_t st_scissor[4];
    int unpack_align;
    /* Vertex attribs must be shared: FAT32 duplicates this .so under
     * libGLESv2 / libGLESv1_CM / libmali. AttribPtr often lands in one
     * copy and DrawElements in another — local statics stay empty and
     * the 64-bit GPU draws zeros (black FBO, audio still runs). */
    struct {
        int32_t enabled;
        int32_t size;
        uint32_t type;
        int32_t normalized;
        int32_t stride;
        uint32_t ptr;
        int32_t is_offset;
    } attribs[16];
    /* Last GLES2 4x4 that looks like MVP/projection. DrawElements often
     * lives in another FAT32 copy of this .so than UniformMatrix4fv. */
    float gles2_mvp[16];
    int32_t gles2_mvp_valid;
};

static inline uint32_t tspgl_pack_f32(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static inline float tspgl_unpack_f32(uint32_t u)
{
    float f;
    memcpy(&f, &u, 4);
    return f;
}

struct tspgl_shared *tspgl_shared(void);
int tspgl_call(uint32_t op, const void *in, uint32_t in_len, void *out,
               uint32_t out_cap);

#endif
