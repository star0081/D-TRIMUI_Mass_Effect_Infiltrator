#define _GNU_SOURCE
#include "frame.h"
#include "gl_api.h"
#include "ops.h"
#include "protocol.h"
#include "xport.h"

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

/* stb_image – PNG only, single-header vendor */
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "atc_decode.c"

struct tspgl_api G;

#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_GAMECONTROLLER 0x00002000u
#define SDL_WINDOW_FULLSCREEN 0x00000001u
#define SDL_WINDOW_OPENGL 0x00000002u
#define SDL_WINDOW_SHOWN 0x00000004u
#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000u
#define SDL_GL_RED_SIZE 0
#define SDL_GL_GREEN_SIZE 1
#define SDL_GL_BLUE_SIZE 2
#define SDL_GL_ALPHA_SIZE 3
#define SDL_GL_DOUBLEBUFFER 5
#define SDL_GL_DEPTH_SIZE 6
#define SDL_GL_STENCIL_SIZE 7
#define SDL_GL_CONTEXT_MAJOR_VERSION 17
#define SDL_GL_CONTEXT_MINOR_VERSION 18
#define SDL_GL_CONTEXT_PROFILE_MASK 21
#define SDL_GL_CONTEXT_PROFILE_ES 4
#define SDL_CONTROLLER_BUTTON_BACK 4
#define SDL_CONTROLLER_BUTTON_GUIDE 5
#define SDL_CONTROLLER_BUTTON_START 6
#define SDL_CONTROLLER_BUTTON_MISC1 15

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_RGBA 0x1908
#define GL_ALPHA 0x1906
#define GL_LUMINANCE 0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_RED 0x1903
#define GL_GREEN 0x1904
#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_RG 0x8227
#define GL_ONE 1
#define GL_ZERO 0
#define GL_TEXTURE_SWIZZLE_R 0x8E42
#define GL_TEXTURE_SWIZZLE_G 0x8E43
#define GL_TEXTURE_SWIZZLE_B 0x8E44
#define GL_TEXTURE_SWIZZLE_A 0x8E45
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_STENCIL_BUFFER_BIT 0x0400
#define GL_NEAREST 0x2600
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_STENCIL 0x84F9
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#define GL_NONE 0
#define GL_SCISSOR_TEST 0x0C11
#define GL_VIEWPORT 0x0BA2
#define GL_SCISSOR_BOX 0x0C10
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_COMPONENT32 0x81A7
#define GL_DEPTH32F_STENCIL8 0x8CAD
#define GL_DEPTH_COMPONENT32F 0x8CAC

typedef struct sdl_event {
    uint32_t type;
    uint8_t pad[124];
} sdl_event;

struct sdl_api {
    void *lib;
    int (*init)(uint32_t);
    void (*quit)(void);
    const char *(*get_error)(void);
    int (*set_hint)(const char *, const char *);
    void *(*create_window)(const char *, int, int, int, int, uint32_t);
    void (*destroy_window)(void *);
    int (*gl_set_attr)(int, int);
    void *(*gl_create_context)(void *);
    void (*gl_delete_context)(void *);
    int (*gl_make_current)(void *, void *);
    void (*gl_swap)(void *);
    void *(*gl_get_proc)(const char *);
    int (*gl_set_swap)(int);
    int (*show_cursor)(int);
    int (*poll_event)(sdl_event *);
    void (*delay)(uint32_t);
    int (*num_joysticks)(void);
    int (*is_game_controller)(int);
    void *(*controller_open)(int);
    const char *(*controller_name)(void *);
    uint8_t (*controller_get_button)(void *, int);
    int16_t (*controller_get_axis)(void *, int);
};

static struct sdl_api sdl;
static void *window;
static void *glctx;
static void *pad;
static uint8_t *frame_map;
static int frame_fd = -1;
static int listen_fd = -1;

int game_w = 640;
int game_h = 480;
int win_w = 1280;
int win_h = 720;
static int present_letterbox;
uint32_t game_fbo;
uint32_t game_color;
uint32_t game_depth;
static uint32_t client_draw_fb;
static uint32_t srv_draw_fb;
static void (*real_bind_fb)(uint32_t, uint32_t);
static void (*real_get_integerv)(uint32_t, int32_t *);
static uint32_t (*real_check_fb)(uint32_t);
static void (*real_draw_buffers)(int32_t, const uint32_t *);
static void (*real_tex_image)(uint32_t, int32_t, int32_t, int32_t, int32_t,
                              int32_t, uint32_t, uint32_t, const void *);
static void (*real_tex_sub)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                            uint32_t, uint32_t, const void *);
static void (*real_tex_parami)(uint32_t, uint32_t, int32_t);
static void (*real_compressed)(uint32_t, int32_t, uint32_t, int32_t, int32_t,
                               int32_t, int32_t, const void *);
static void (*real_gen_mipmap)(uint32_t);
static void (*real_delete_tex)(int32_t, const uint32_t *);
static uint8_t tex_npot[4096];
static uint8_t tex_legacy[4096]; /* 1=ALPHA 2=LUMINANCE 3=LUMINANCE_ALPHA */
static int input_soon;
static int have_fb_fetch;
static int have_fb_fetch_nc;
static void (*real_tex_paramf)(uint32_t, uint32_t, float);
static void (*real_clear)(uint32_t);
static void (*real_scissor)(int32_t, int32_t, int32_t, int32_t);
static void (*real_bind_tex)(uint32_t, uint32_t);
static uint32_t *atc_rgba;
static size_t atc_rgba_cap;
static uint32_t fps_fbo, fps_color;
static unsigned fps_baked = 0xffffffffu;
#define FPS_W 140
#define FPS_H 32
static int overlay_cursor;
static int cur_sc_x, cur_sc_y, cur_sc_w, cur_sc_h;
static int cursor_hits, cursor_minx, cursor_miny, cursor_maxx, cursor_maxy;
static int request_quit;
static unsigned fps_shown = 60;
static unsigned mem_total_mib;
static unsigned mem_used_mib;
static unsigned mem_baked_total = 0xffffffffu;
static unsigned mem_baked_used = 0xffffffffu;
static int probe_baked = -1;
static int stick_lx, stick_ly;
static int stick_baked_x = 999, stick_baked_y = 999;

/* boot splash state */
static uint32_t splash_tex;
static uint32_t splash_fbo;
static int      splash_active;
static int      splash_hide_pending; /* counts down; reaches 0 → hide */
static uint32_t first_client_t;      /* now_ms() when first client connected */

static uint32_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)ts.tv_sec * 1000u + (uint32_t)(ts.tv_nsec / 1000000u);
}

static void wrap_bind_fb(uint32_t target, uint32_t fb);
static void wrap_get_integerv(uint32_t pname, int32_t *params);
static uint32_t wrap_check_fb(uint32_t target);

#define TSPGL_STAGE (4u * 1024u * 1024u)
/* Pixel memory must outlive glTexImage2D. On GE8300 the upload DMA is still
 * running when the next 16 ms frame recycles the buffer, which paints
 * flickering black bands. Four slots ≈ 3 frames of delay. */
#define TSPGL_STAGE_SLOTS 4
static uint8_t tspgl_stage_mem[TSPGL_STAGE_SLOTS][TSPGL_STAGE];
static int tspgl_stage_cur;
static uint32_t tspgl_stage_used;
static void **tspgl_stage_extra[TSPGL_STAGE_SLOTS];
static int tspgl_stage_en[TSPGL_STAGE_SLOTS];
static int tspgl_stage_cap[TSPGL_STAGE_SLOTS];

static const void *tspgl_stage(const void *src, uint32_t n)
{
    uint32_t pad;
    uint8_t *base;
    int cur;
    void *p;
    static int logged_big;

    if (!src || n == 0)
        return src;
    pad = (n + 15u) & ~15u;
    cur = tspgl_stage_cur;
    base = tspgl_stage_mem[cur];
    if (tspgl_stage_used + pad <= TSPGL_STAGE) {
        memcpy(base + tspgl_stage_used, src, n);
        src = base + tspgl_stage_used;
        tspgl_stage_used += pad;
        return src;
    }
    /* Never return `src`: that pointer dies when handle_client frees the
     * socket buffer, while PowerVR may still DMA. Arena is 4MB; chicago
     * lightmaps are ~16MB RGBA, so they always take this path. */
    if (tspgl_stage_en[cur] == tspgl_stage_cap[cur]) {
        int nc = tspgl_stage_cap[cur] ? tspgl_stage_cap[cur] * 2 : 64;
        void **nb = realloc(tspgl_stage_extra[cur], (size_t)nc * sizeof(void *));
        if (!nb) {
            fprintf(stderr, "tspgl-srv: stage realloc fail n=%u extra=%d\n", n,
                    tspgl_stage_en[cur]);
            return src;
        }
        tspgl_stage_extra[cur] = nb;
        tspgl_stage_cap[cur] = nc;
    }
    p = malloc(n);
    if (!p) {
        fprintf(stderr, "tspgl-srv: stage malloc %u fail\n", n);
        return src;
    }
    memcpy(p, src, n);
    tspgl_stage_extra[cur][tspgl_stage_en[cur]++] = p;
    if (!logged_big && n > TSPGL_STAGE) {
        fprintf(stderr, "tspgl-srv: stage extra %u bytes (arena full, extras=%d)\n",
                n, tspgl_stage_en[cur]);
        logged_big = 1;
    }
    return p;
}

static void tspgl_stage_flip(void)
{
    int next = (tspgl_stage_cur + 1) % TSPGL_STAGE_SLOTS;
    int i;
    for (i = 0; i < tspgl_stage_en[next]; ++i)
        free(tspgl_stage_extra[next][i]);
    tspgl_stage_en[next] = 0;
    tspgl_stage_cur = next;
    tspgl_stage_used = 0;
}

void tspgl_es1_init(void);
void tspgl_es1_prepare_draw(void);
int tspgl_es1_skip_draw(void);
void tspgl_es1_finish_draw(void);
void tspgl_es1_set_active_tex(uint32_t texture);
void tspgl_es1_bind_tex(uint32_t target, uint32_t id);
void tspgl_es1_note_client_program(uint32_t p);
int tspgl_es1_gles2_active(void);
void tspgl_es1_bind_client_program(void);
static int tspgl_dispatch_es1(uint32_t op, const uint8_t *in, uint32_t nbytes,
                              uint8_t *out, uint32_t *out_n);
#include "server_gen.c"
#include "server_gl.c"
#include "gles1_server.c"

static int load_sdl(void)
{
    const char *candidates[] = {
        "/usr/trimui/lib/libSDL2-2.0.so.0",
        "libSDL2-2.0.so.0",
        "libSDL2.so",
    };
    size_t i;

    memset(&sdl, 0, sizeof(sdl));
    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        sdl.lib = dlopen(candidates[i], RTLD_NOW | RTLD_GLOBAL);
        if (sdl.lib)
            break;
    }
    if (!sdl.lib) {
        fprintf(stderr, "tspgl-srv: dlopen SDL2: %s\n", dlerror());
        return -1;
    }
#define LOAD(member, name)                                                     \
    do {                                                                       \
        sdl.member = dlsym(sdl.lib, name);                                     \
        if (!sdl.member) {                                                     \
            fprintf(stderr, "tspgl-srv: missing %s\n", name);                  \
            return -1;                                                         \
        }                                                                      \
    } while (0)
#define LOAD_OPT(member, name) sdl.member = dlsym(sdl.lib, name)
    LOAD(init, "SDL_Init");
    LOAD(quit, "SDL_Quit");
    LOAD(get_error, "SDL_GetError");
    LOAD(set_hint, "SDL_SetHint");
    LOAD(create_window, "SDL_CreateWindow");
    LOAD(destroy_window, "SDL_DestroyWindow");
    LOAD(gl_set_attr, "SDL_GL_SetAttribute");
    LOAD(gl_create_context, "SDL_GL_CreateContext");
    LOAD(gl_delete_context, "SDL_GL_DeleteContext");
    LOAD(gl_make_current, "SDL_GL_MakeCurrent");
    LOAD(gl_swap, "SDL_GL_SwapWindow");
    LOAD(gl_get_proc, "SDL_GL_GetProcAddress");
    LOAD_OPT(gl_set_swap, "SDL_GL_SetSwapInterval");
    LOAD(show_cursor, "SDL_ShowCursor");
    LOAD(poll_event, "SDL_PollEvent");
    LOAD(delay, "SDL_Delay");
    LOAD_OPT(num_joysticks, "SDL_NumJoysticks");
    LOAD_OPT(is_game_controller, "SDL_IsGameController");
    LOAD_OPT(controller_open, "SDL_GameControllerOpen");
    LOAD_OPT(controller_name, "SDL_GameControllerName");
    LOAD_OPT(controller_get_button, "SDL_GameControllerGetButton");
    LOAD_OPT(controller_get_axis, "SDL_GameControllerGetAxis");
#undef LOAD
#undef LOAD_OPT
    return 0;
}

static void *gl_get(const char *name)
{
    void *p = NULL;
    if (sdl.gl_get_proc)
        p = sdl.gl_get_proc(name);
    if (!p)
        p = dlsym(RTLD_DEFAULT, name);
    return p;
}

static uint8_t *open_frame(void)
{
    uint8_t *map;
    int fd = open(NFSMW_FRAME_PATH, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        fprintf(stderr, "tspgl-srv: open frame errno=%d\n", errno);
        return NULL;
    }
    if (ftruncate(fd, (off_t)NFSMW_FRAME_FILE_SIZE) != 0) {
        fprintf(stderr, "tspgl-srv: ftruncate errno=%d\n", errno);
        close(fd);
        return NULL;
    }
    map = mmap(NULL, NFSMW_FRAME_FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
               fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "tspgl-srv: mmap errno=%d\n", errno);
        close(fd);
        return NULL;
    }
    frame_fd = fd;
    return map;
}

static void *open_pad(void)
{
    int n, i;
    void *p;
    if (!sdl.num_joysticks || !sdl.is_game_controller || !sdl.controller_open)
        return NULL;
    n = sdl.num_joysticks();
    for (i = 0; i < n; ++i) {
        if (sdl.is_game_controller(i) == 0)
            continue;
        p = sdl.controller_open(i);
        if (p) {
            const char *name = sdl.controller_name ? sdl.controller_name(p) : NULL;
            fprintf(stderr, "tspgl-srv: pad=%s index=%d joysticks=%d\n",
                    name ? name : "?", i, n);
            return p;
        }
    }
    fprintf(stderr, "tspgl-srv: no game controller (joysticks=%d)\n", n);
    return NULL;
}

static int16_t axis_deadzone(int32_t v, int dz)
{
    if (v > -dz && v < dz)
        return 0;
    if (v > 32767)
        v = 32767;
    if (v < -32768)
        v = -32768;
    return (int16_t)v;
}

/* Left stick: a short push already reads as full tilt, so cover-peek
 * does not need the stick buried. full_at is the raw value that saturates. */
static int16_t axis_shape(int32_t v, int dz, int full_at)
{
    int32_t mag, t;
    int neg;

    if (v > -dz && v < dz)
        return 0;
    if (v > 32767)
        v = 32767;
    if (v < -32768)
        v = -32768;
    neg = v < 0;
    mag = neg ? -v : v;
    if (full_at <= dz)
        full_at = dz + 1;
    t = mag - dz;
    if (t >= full_at - dz)
        return neg ? (int16_t)-32768 : (int16_t)32767;
    t = (t * 32767) / (full_at - dz);
    return neg ? (int16_t)(-t) : (int16_t)t;
}

static int axis_pct(int32_t v)
{
    if (v > 32767)
        v = 32767;
    if (v < -32767)
        v = -32767;
    v = (int)((v * 100) / 32767);
    if (v >= 85)
        return 100;
    if (v <= -85)
        return -100;
    return v;
}

static void publish_input(uint32_t *hdr)
{
    uint32_t buttons = 0;
    int i;

    if (!pad || !sdl.controller_get_button || !sdl.controller_get_axis)
        return;
    for (i = 0; i < 21; ++i) {
        if (sdl.controller_get_button(pad, i))
            buttons |= 1u << i;
    }
    /* Menu+Select cycles the stripe probe. Start stays the game's pause. */
    {
        uint32_t select_on = (buttons & (1u << SDL_CONTROLLER_BUTTON_BACK)) != 0;
        uint32_t menu_on = (buttons & (1u << SDL_CONTROLLER_BUTTON_GUIDE)) != 0;
        static uint32_t chord_was;
        uint32_t chord = menu_on && select_on;
        if (chord && !chord_was)
            tspgl_probe_cycle();
        chord_was = chord;
        if (chord)
            buttons &= ~((1u << SDL_CONTROLLER_BUTTON_BACK) |
                         (1u << SDL_CONTROLLER_BUTTON_GUIDE));
    }
    /* Select+Start, held briefly, leaves the port. Start alone stays pause. */
    {
        uint32_t select_on = (buttons & (1u << SDL_CONTROLLER_BUTTON_BACK)) != 0;
        uint32_t start_on = (buttons & (1u << SDL_CONTROLLER_BUTTON_START)) != 0;
        static uint32_t hold;
        if (select_on && start_on) {
            if (++hold == 30)
                request_quit = 1;
        } else {
            hold = 0;
        }
    }
    hdr[NFSMW_HDR_BUTTONS] = buttons;
    for (i = 0; i < 6; ++i) {
        int32_t v = (int32_t)sdl.controller_get_axis(pad, i);
        int16_t a = i < 2 ? axis_shape(v, 2800, 14000)
                          : axis_deadzone(v, i < 4 ? 10000 : 4000);
        if (i == 0)
            stick_lx = axis_pct(v);
        else if (i == 1)
            stick_ly = axis_pct(v);
        hdr[NFSMW_HDR_AXIS0 + i] = (uint32_t)(int32_t)a;
    }
    hdr[NFSMW_HDR_PAD_SEQ] = hdr[NFSMW_HDR_PAD_SEQ] + 1u;
}

static int create_game_fbo(void)
{
    void (*gen_tex)(int32_t, uint32_t *) = G.glGenTextures;
    void (*bind_tex)(uint32_t, uint32_t) = G.glBindTexture;
    void (*tex_parami)(uint32_t, uint32_t, int32_t) = G.glTexParameteri;
    void (*tex_image)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                      uint32_t, uint32_t, const void *);
    void (*gen_rb)(int32_t, uint32_t *) = G.glGenRenderbuffers;
    void (*bind_rb)(uint32_t, uint32_t) = G.glBindRenderbuffer;
    void (*rb_store)(uint32_t, uint32_t, int32_t, int32_t) =
        G.glRenderbufferStorage;
    void (*gen_fb)(int32_t, uint32_t *) = G.glGenFramebuffers;
    void (*bind_fb)(uint32_t, uint32_t) = G.glBindFramebuffer;
    void (*fb_tex)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t) =
        G.glFramebufferTexture2D;
    void (*fb_rb)(uint32_t, uint32_t, uint32_t, uint32_t) =
        G.glFramebufferRenderbuffer;
    uint32_t (*check)(uint32_t) = G.glCheckFramebufferStatus;
    uint32_t status;

    tex_image = (void *)gl_get("glTexImage2D");
    if (!gen_tex || !bind_tex || !tex_parami || !tex_image || !gen_rb ||
        !bind_rb || !rb_store || !gen_fb || !bind_fb || !fb_tex || !fb_rb ||
        !check) {
        fprintf(stderr, "tspgl-srv: missing FBO entry points\n");
        return -1;
    }

    gen_tex(1, &game_color);
    bind_tex(GL_TEXTURE_2D, game_color);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int32_t)GL_NEAREST);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int32_t)GL_NEAREST);
    tex_parami(GL_TEXTURE_2D, 0x2802, (int32_t)GL_CLAMP_TO_EDGE);
    tex_parami(GL_TEXTURE_2D, 0x2803, (int32_t)GL_CLAMP_TO_EDGE);
    tex_image(GL_TEXTURE_2D, 0, (int32_t)GL_RGBA, game_w, game_h, 0, GL_RGBA,
              GL_UNSIGNED_BYTE, NULL);

    gen_rb(1, &game_depth);
    bind_rb(GL_RENDERBUFFER, game_depth);
    rb_store(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, game_w, game_h);
    if (G.glGetError && G.glGetError() != 0) {
        rb_store(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, game_w, game_h);
        fprintf(stderr, "tspgl-srv: depth fallback DEPTH_COMPONENT16\n");
    }

    gen_fb(1, &game_fbo);
    bind_fb(GL_FRAMEBUFFER, game_fbo);
    fb_tex(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, game_color, 0);
    fb_rb(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
          game_depth);
    status = check(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fb_rb(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, game_depth);
        status = check(GL_FRAMEBUFFER);
    }
    fprintf(stderr, "tspgl-srv: game FBO %u color=%u depth=%u %dx%d status=0x%x\n",
            game_fbo, game_color, game_depth, game_w, game_h, status);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return -1;
    srv_draw_fb = game_fbo;
    client_draw_fb = 0;
    return 0;
}

static uint32_t wrap_check_fb(uint32_t target)
{
    uint32_t st;
    if (!real_check_fb)
        return 0;
    st = real_check_fb(target);
    if (st == GL_FRAMEBUFFER_COMPLETE)
        return st;
    if (real_draw_buffers) {
        void (*get_att)(uint32_t, uint32_t, uint32_t, int32_t *) =
            (void *)G.glGetFramebufferAttachmentParameteriv;
        int32_t color_type = 0;
        uint32_t none = GL_NONE;
        uint32_t color = GL_COLOR_ATTACHMENT0;

        /* A color attachment that is still incomplete must not get
         * DrawBuffers(NONE): the shadow/reflection texture stays black and
         * is projected onto the road. */
        if (get_att) {
            get_att(target, GL_COLOR_ATTACHMENT0,
                    GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &color_type);
            if (color_type != (int32_t)GL_NONE)
                return st;
        }
        real_draw_buffers(1, &none);
        st = real_check_fb(target);
        if (st == GL_FRAMEBUFFER_COMPLETE) {
            static int once;
            if (!once) {
                fprintf(stderr,
                        "tspgl-srv: depth-only FBO via DrawBuffers(NONE)\n");
                once = 1;
            }
            return st;
        }
        real_draw_buffers(1, &color);
        st = real_check_fb(target);
    }
    return st;
}

static void wrap_bind_fb(uint32_t target, uint32_t fb)
{
    uint32_t orig = fb;
    if (fb == 0)
        fb = game_fbo;
    if (real_bind_fb)
        real_bind_fb(target, fb);
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
        client_draw_fb = orig;
        srv_draw_fb = fb;
    }
}

static void wrap_get_integerv(uint32_t pname, int32_t *params)
{
    if (real_get_integerv)
        real_get_integerv(pname, params);
    if (params && pname == GL_FRAMEBUFFER_BINDING &&
        (uint32_t)params[0] == game_fbo)
        params[0] = 0;
}

static int is_depth_internal(int32_t fmt)
{
    switch ((uint32_t)fmt) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH_STENCIL:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH32F_STENCIL8:
        return 1;
    default:
        return 0;
    }
}

/* GLES3 defaults depth textures to COMPARE_REF_TO_TEXTURE. A GLES2 title
 * samples them with sampler2D and gets 0 → pitch-black shadows/puddles. */
static int is_pot(int32_t n)
{
    return n > 0 && (n & (n - 1)) == 0;
}

static int is_mip_filter(int32_t p)
{
    return p == (int32_t)GL_NEAREST_MIPMAP_NEAREST ||
           p == (int32_t)GL_LINEAR_MIPMAP_NEAREST ||
           p == (int32_t)GL_NEAREST_MIPMAP_LINEAR ||
           p == (int32_t)GL_LINEAR_MIPMAP_LINEAR;
}

static uint32_t param_target(uint32_t target)
{
    if (target == GL_TEXTURE_CUBE_MAP ||
        (target >= 0x8515u && target <= 0x851Au))
        return GL_TEXTURE_CUBE_MAP;
    return GL_TEXTURE_2D;
}

static uint32_t bound_tex(uint32_t ptarget)
{
    int32_t id = 0;
    if (!real_get_integerv)
        return 0;
    real_get_integerv(ptarget == GL_TEXTURE_CUBE_MAP ?
                      GL_TEXTURE_BINDING_CUBE_MAP : GL_TEXTURE_BINDING_2D,
                      &id);
    return id > 0 ? (uint32_t)id : 0;
}

static int tex_is_npot(uint32_t id)
{
    return id && id < 4096u && tex_npot[id];
}

static void mark_npot(uint32_t id, int npot)
{
    if (id && id < 4096u)
        tex_npot[id] = npot ? 1 : 0;
}

static void sanitize_npot(uint32_t ptarget)
{
    if (!real_tex_parami)
        return;
    real_tex_parami(ptarget, GL_TEXTURE_MIN_FILTER, (int32_t)GL_LINEAR);
    real_tex_parami(ptarget, GL_TEXTURE_MAG_FILTER, (int32_t)GL_LINEAR);
    real_tex_parami(ptarget, GL_TEXTURE_WRAP_S, (int32_t)GL_CLAMP_TO_EDGE);
    real_tex_parami(ptarget, GL_TEXTURE_WRAP_T, (int32_t)GL_CLAMP_TO_EDGE);
    if (ptarget == GL_TEXTURE_CUBE_MAP)
        real_tex_parami(ptarget, GL_TEXTURE_WRAP_R, (int32_t)GL_CLAMP_TO_EDGE);
    real_tex_parami(ptarget, GL_TEXTURE_BASE_LEVEL, 0);
    real_tex_parami(ptarget, GL_TEXTURE_MAX_LEVEL, 0);
}

static void sanitize_cube(uint32_t ptarget)
{
    if (ptarget != GL_TEXTURE_CUBE_MAP || !real_tex_parami)
        return;
    real_tex_parami(ptarget, GL_TEXTURE_WRAP_S, (int32_t)GL_CLAMP_TO_EDGE);
    real_tex_parami(ptarget, GL_TEXTURE_WRAP_T, (int32_t)GL_CLAMP_TO_EDGE);
    real_tex_parami(ptarget, GL_TEXTURE_WRAP_R, (int32_t)GL_CLAMP_TO_EDGE);
    real_tex_parami(ptarget, GL_TEXTURE_MIN_FILTER, (int32_t)GL_LINEAR);
    real_tex_parami(ptarget, GL_TEXTURE_BASE_LEVEL, 0);
    real_tex_parami(ptarget, GL_TEXTURE_MAX_LEVEL, 0);
}

static int32_t sized_ifmt(int32_t ifmt)
{
    if (ifmt == (int32_t)GL_RGBA || ifmt == 4)
        return 0x8058; /* GL_RGBA8 */
    if (ifmt == 0x1907 || ifmt == 3) /* GL_RGB */
        return 0x8051; /* GL_RGB8 */
    if (ifmt == (int32_t)0x80E1) /* BGRA_EXT */
        return 0x8058;
    if (ifmt == (int32_t)GL_ALPHA || ifmt == (int32_t)GL_LUMINANCE)
        return (int32_t)GL_R8;
    if (ifmt == (int32_t)GL_LUMINANCE_ALPHA)
        return (int32_t)GL_RG8;
    return ifmt;
}

/* GLES 3.2 dropped ALPHA/LUMINANCE. Fonts are GL_ALPHA + MODULATE; without
 * a (1,1,1,A) swizzle PowerVR samples (A,0,0,1) and Russian text goes black. */
static uint8_t legacy_from_fmt(int32_t ifmt, uint32_t format)
{
    if (ifmt == (int32_t)GL_ALPHA || format == GL_ALPHA)
        return 1;
    if (ifmt == (int32_t)GL_LUMINANCE || format == GL_LUMINANCE)
        return 2;
    if (ifmt == (int32_t)GL_LUMINANCE_ALPHA || format == GL_LUMINANCE_ALPHA)
        return 3;
    return 0;
}

static uint32_t es3_unsized_format(uint32_t format)
{
    if (format == GL_ALPHA || format == GL_LUMINANCE)
        return GL_RED;
    if (format == GL_LUMINANCE_ALPHA)
        return GL_RG;
    return format;
}

static void mark_legacy(uint32_t id, uint8_t mode)
{
    if (id && id < 4096u)
        tex_legacy[id] = mode;
}

static uint8_t tex_is_legacy(uint32_t id)
{
    return (id && id < 4096u) ? tex_legacy[id] : 0;
}

static void apply_legacy_swizzle(uint32_t ptarget, uint8_t mode)
{
    if (!real_tex_parami || !mode)
        return;
    if (mode == 1) {
        /* (A,A,A,A): fonts stay readable with white glColor, 3D ALPHA
         * masks no longer paint an opaque white film over albedo. */
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_R, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_G, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_B, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_A, (int32_t)GL_RED);
    } else if (mode == 2) {
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_R, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_G, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_B, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_A, (int32_t)GL_ONE);
    } else if (mode == 3) {
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_R, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_G, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_B, (int32_t)GL_RED);
        real_tex_parami(ptarget, GL_TEXTURE_SWIZZLE_A, (int32_t)GL_GREEN);
    }
}

static void wrap_tex_image2d(uint32_t target, int32_t level, int32_t ifmt,
                             int32_t w, int32_t h, int32_t border,
                             uint32_t format, uint32_t type,
                             const void *pixels)
{
    uint32_t ptarget = param_target(target);
    uint32_t id;
    int npot;
    uint8_t legacy = legacy_from_fmt(ifmt, format);

    /* Mip chain is discarded (MAX_LEVEL 0). Skip the extra uploads. */
    if (level != 0)
        return;

    ifmt = sized_ifmt(ifmt);
    format = es3_unsized_format(format);
    if (real_tex_image)
        real_tex_image(target, level, ifmt, w, h, border, format, type, pixels);
    /* GLES1 often sets LINEAR_MIPMAP_* after a single level-0 upload.
     * Incomplete mip chain samples as black on PowerVR. Drop mips. */
    if (real_tex_parami && level == 0) {
        real_tex_parami(ptarget, GL_TEXTURE_MIN_FILTER, (int32_t)GL_LINEAR);
        real_tex_parami(ptarget, GL_TEXTURE_BASE_LEVEL, 0);
        real_tex_parami(ptarget, GL_TEXTURE_MAX_LEVEL, 0);
    }
    if (is_depth_internal(ifmt) && real_tex_parami)
        real_tex_parami(ptarget, GL_TEXTURE_COMPARE_MODE, (int32_t)GL_NONE);
    if (level != 0)
        return;
    id = bound_tex(ptarget);
    npot = !is_pot(w) || !is_pot(h);
    mark_npot(id, npot);
    mark_legacy(id, legacy);
    if (legacy) {
        static int once;
        apply_legacy_swizzle(ptarget, legacy);
        if (!once) {
            fprintf(stderr, "tspgl-srv: GLES1 ALPHA/LUM format=%u -> R/RG + swizzle\n",
                    (unsigned)legacy);
            once = 1;
        }
    }
    if (npot) {
        static int once;
        sanitize_npot(ptarget);
        if (!once) {
            fprintf(stderr, "tspgl-srv: NPOT %dx%d id=%u -> LINEAR+CLAMP\n",
                    (int)w, (int)h, id);
            once = 1;
        }
    }
    sanitize_cube(ptarget);
    if (pixels && G.glFlush && (w >= 512 || h >= 512))
        G.glFlush();
    /* Boot splash: when game uploads a large texture the real splash/menu is
     * about to render.  Allow 3 more present_swap (= 2 extra splash frames
     * before hiding) so the texture has time to land in the game FBO. */
    if (splash_active && !splash_hide_pending && level == 0 &&
        (w >= 512 || h >= 512)) {
        fprintf(stderr, "tspgl-srv: boot splash hide tex=%dx%d\n",
                (int)w, (int)h);
        splash_hide_pending = 3;
    }
}

static void wrap_tex_sub_image2d(uint32_t target, int32_t level, int32_t xoff,
                                 int32_t yoff, int32_t w, int32_t h,
                                 uint32_t format, uint32_t type,
                                 const void *pixels)
{
    uint32_t id = bound_tex(param_target(target));
    uint8_t legacy = tex_is_legacy(id);

    if (legacy || format == GL_ALPHA || format == GL_LUMINANCE ||
        format == GL_LUMINANCE_ALPHA)
        format = es3_unsized_format(format);
    if (real_tex_sub)
        real_tex_sub(target, level, xoff, yoff, w, h, format, type, pixels);
}

static void wrap_compressed(uint32_t target, int32_t level, uint32_t ifmt,
                            int32_t w, int32_t h, int32_t border,
                            int32_t imageSize, const void *data)
{
    uint32_t ptarget = param_target(target);
    uint32_t id;
    int npot;

    (void)border;
    if (level != 0)
        return;
    if (atc_is_fmt(ifmt) && data && w > 0 && h > 0) {
        size_t need = (size_t)w * (size_t)h;
        if (need > atc_rgba_cap) {
            uint32_t *nb = realloc(atc_rgba, need * 4u);
            if (nb) {
                atc_rgba = nb;
                atc_rgba_cap = need;
            }
        }
        if (atc_rgba && need <= atc_rgba_cap &&
            atc_decode_rgba(ifmt, w, h, data, imageSize, atc_rgba) == 0) {
            if (real_tex_image)
                real_tex_image(target, 0, (int32_t)0x8058, w, h, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, atc_rgba);
            id = bound_tex(ptarget);
            npot = !is_pot(w) || !is_pot(h);
            mark_npot(id, npot);
            if (npot)
                sanitize_npot(ptarget);
            else if (real_tex_parami) {
                real_tex_parami(ptarget, GL_TEXTURE_MIN_FILTER, (int32_t)GL_LINEAR);
                real_tex_parami(ptarget, GL_TEXTURE_MAX_LEVEL, 0);
            }
            sanitize_cube(ptarget);
            return;
        }
    }
    if (real_compressed)
        real_compressed(target, level, ifmt, w, h, border, imageSize, data);
    id = bound_tex(ptarget);
    npot = !is_pot(w) || !is_pot(h);
    mark_npot(id, npot);
    if (npot)
        sanitize_npot(ptarget);
    sanitize_cube(ptarget);
}

static void wrap_bind_texture(uint32_t target, uint32_t id)
{
    if (real_bind_tex)
        real_bind_tex(target, id);
    tspgl_es1_bind_tex(target, id);
}

static int npot_fix_param(uint32_t id, uint32_t pname, int32_t *param)
{
    if (!tex_is_npot(id))
        return 0;
    if (pname == GL_TEXTURE_MIN_FILTER && is_mip_filter(*param)) {
        *param = (int32_t)GL_LINEAR;
        return 1;
    }
    if (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T ||
        pname == GL_TEXTURE_WRAP_R) {
        *param = (int32_t)GL_CLAMP_TO_EDGE;
        return 1;
    }
    if (pname == GL_TEXTURE_MAX_LEVEL) {
        *param = 0;
        return 1;
    }
    return 0;
}

static void wrap_tex_parami(uint32_t target, uint32_t pname, int32_t param)
{
    uint32_t ptarget = param_target(target);
    uint32_t id = bound_tex(ptarget);

    npot_fix_param(id, pname, &param);
    if (pname == GL_TEXTURE_MIN_FILTER && is_mip_filter(param))
        param = (int32_t)GL_LINEAR;
    if (pname == GL_TEXTURE_MAX_LEVEL && param != 0)
        param = 0;
    if (pname == GL_TEXTURE_COMPARE_MODE)
        param = (int32_t)GL_NONE;
    if (ptarget == GL_TEXTURE_CUBE_MAP &&
        (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T ||
         pname == GL_TEXTURE_WRAP_R))
        param = (int32_t)GL_CLAMP_TO_EDGE;
    if (real_tex_parami)
        real_tex_parami(target, pname, param);
}

static void wrap_tex_paramf(uint32_t target, uint32_t pname, float param)
{
    uint32_t ptarget = param_target(target);
    uint32_t id = bound_tex(ptarget);
    int32_t ip = (int32_t)param;

    if (npot_fix_param(id, pname, &ip))
        param = (float)ip;
    if (pname == GL_TEXTURE_MIN_FILTER && is_mip_filter(ip))
        param = (float)GL_LINEAR;
    if (pname == GL_TEXTURE_COMPARE_MODE)
        param = 0.f;
    if (ptarget == GL_TEXTURE_CUBE_MAP &&
        (pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T ||
         pname == GL_TEXTURE_WRAP_R))
        param = (float)GL_CLAMP_TO_EDGE;
    if (real_tex_paramf)
        real_tex_paramf(target, pname, param);
}

static void wrap_gen_mipmap(uint32_t target)
{
    uint32_t ptarget = param_target(target);
    if (tex_is_npot(bound_tex(ptarget)))
        return;
    if (real_gen_mipmap)
        real_gen_mipmap(target);
}

static void wrap_delete_tex(int32_t n, const uint32_t *ids)
{
    int32_t i;
    if (real_delete_tex)
        real_delete_tex(n, ids);
    if (!ids)
        return;
    for (i = 0; i < n; ++i)
        if (ids[i] && ids[i] < 4096u) {
            tex_npot[ids[i]] = 0;
            tex_legacy[ids[i]] = 0;
        }
}

static void wrap_scissor(int32_t x, int32_t y, int32_t w, int32_t h)
{
    cur_sc_x = x;
    cur_sc_y = y;
    cur_sc_w = w;
    cur_sc_h = h;
    if (real_scissor)
        real_scissor(x, y, w, h);
}

static void wrap_clear(uint32_t mask)
{
    uint32_t scissor_on = 0;
    int mw, Mw;

    if (overlay_cursor) {
        if (real_clear)
            real_clear(mask);
        return;
    }
    if (G.glIsEnabled)
        scissor_on = G.glIsEnabled(GL_SCISSOR_TEST);
    mw = cur_sc_w < cur_sc_h ? cur_sc_w : cur_sc_h;
    Mw = cur_sc_w > cur_sc_h ? cur_sc_w : cur_sc_h;
    /* Loader cursor is GLES1 scissor runs (1-3px thick). Drop them and
     * redraw a DS-cyan cross at present so the OS mouse is gone. */
    if (scissor_on && (mask & GL_COLOR_BUFFER_BIT) && mw > 0 && mw <= 3 &&
        Mw <= 64) {
        int x0 = cur_sc_x, y0 = cur_sc_y;
        int x1 = cur_sc_x + cur_sc_w, y1 = cur_sc_y + cur_sc_h;
        if (!cursor_hits) {
            cursor_minx = x0;
            cursor_miny = y0;
            cursor_maxx = x1;
            cursor_maxy = y1;
        } else {
            if (x0 < cursor_minx)
                cursor_minx = x0;
            if (y0 < cursor_miny)
                cursor_miny = y0;
            if (x1 > cursor_maxx)
                cursor_maxx = x1;
            if (y1 > cursor_maxy)
                cursor_maxy = y1;
        }
        cursor_hits++;
        return;
    }
    /* PowerVR tiles keep undefined depth if only COLOR is cleared — but
     * only on the presented game FBO. Extra depth clears on offscreen
     * colour RTs / shadow maps wipe those passes (world blinks black). */
    if (!scissor_on && (mask & GL_COLOR_BUFFER_BIT) &&
        srv_draw_fb == game_fbo)
        mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    if (real_clear)
        real_clear(mask);
}

static void mem_refresh(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    char line[128];
    unsigned long total_kb = 0, avail_kb = 0, free_kb = 0;

    if (!f)
        return;
    while (fgets(line, (int)sizeof(line), f)) {
        if (!total_kb && sscanf(line, "MemTotal: %lu kB", &total_kb) == 1)
            continue;
        if (!avail_kb && sscanf(line, "MemAvailable: %lu kB", &avail_kb) == 1)
            continue;
        if (!free_kb && sscanf(line, "MemFree: %lu kB", &free_kb) == 1)
            continue;
    }
    fclose(f);
    if (!avail_kb)
        avail_kb = free_kb;
    if (avail_kb > total_kb)
        avail_kb = total_kb;
    mem_total_mib = (unsigned)(total_kb / 1024ul);
    mem_used_mib = (unsigned)((total_kb - avail_kb) / 1024ul);
    if (mem_total_mib > 999u)
        mem_total_mib = 999u;
    if (mem_used_mib > 999u)
        mem_used_mib = 999u;
}

static void fps_tick(void)
{
    static uint32_t t0;
    static unsigned frames;
    uint32_t t = now_ms();

    frames++;
    if (!t0)
        t0 = t;
    if (t - t0 >= 500u) {
        unsigned dt = t - t0;
        if (dt < 1u)
            dt = 1u;
        fps_shown = frames * 1000u / dt;
        if (fps_shown > 999u)
            fps_shown = 999u;
        frames = 0;
        t0 = t;
        mem_refresh();
    }
}

/* 5-wide bitmaps, bit 4 = leftmost column. */
static const uint8_t FONT5_DIGIT[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
};
static const uint8_t FONT5_F[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t FONT5_P[7] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
static const uint8_t FONT5_S[7] = {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E};
static const uint8_t FONT5_VE[7] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
static const uint8_t FONT5_I_CYR[7] = {0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x11};
static const uint8_t FONT5_SLASH[7] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};
static const uint8_t FONT5_R[7] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
static const uint8_t FONT5_X[7] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
static const uint8_t FONT5_Y[7] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
static const uint8_t FONT5_PLUS[7] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
static const uint8_t FONT5_MINUS[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};

static void overlay_fill(int x, int y, int w, int h,
                         void (*scissor_fn)(int32_t, int32_t, int32_t, int32_t),
                         void (*clear_fn)(uint32_t))
{
    int gl_y;

    if (w < 1 || h < 1)
        return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= win_w || y >= win_h)
        return;
    if (x + w > win_w)
        w = win_w - x;
    if (y + h > win_h)
        h = win_h - y;
    gl_y = win_h - (y + h);
    if (gl_y < 0)
        gl_y = 0;
    scissor_fn(x, gl_y, w, h);
    clear_fn(GL_COLOR_BUFFER_BIT);
}

static void overlay_glyph(int x, int y, int s, const uint8_t rows[7],
                          void (*scissor_fn)(int32_t, int32_t, int32_t, int32_t),
                          void (*clear_fn)(uint32_t))
{
    int r, c;

    if (s < 1)
        s = 1;
    for (r = 0; r < 7; ++r) {
        uint8_t bits = rows[r];
        for (c = 0; c < 5; ++c) {
            if (bits & (0x10u >> c))
                overlay_fill(x + c * s, y + r * s, s, s, scissor_fn, clear_fn);
        }
    }
}

static void fps_bake(void)
{
    void (*clear_fn)(uint32_t) = real_clear ? real_clear : G.glClear;
    void (*scissor_fn)(int32_t, int32_t, int32_t, int32_t) =
        real_scissor ? real_scissor : G.glScissor;
    const int s = 3;
    const int oy = 6;
    const int gw = 5 * s + s;
    const int ox = 6;
    int n = (int)fps_shown;
    int d2, d1, d0;
    int saved_w = win_w, saved_h = win_h;

    if (!fps_fbo || !clear_fn || !scissor_fn || !G.glClearColor)
        return;
    if (n < 0)
        n = 0;
    if (n > 999)
        n = 999;
    d2 = n / 100;
    d1 = (n / 10) % 10;
    d0 = n % 10;
    if (real_bind_fb)
        real_bind_fb(GL_FRAMEBUFFER, fps_fbo);
    win_w = FPS_W;
    win_h = FPS_H;
    if (G.glViewport)
        G.glViewport(0, 0, FPS_W, FPS_H);
    overlay_cursor = 1;
    if (G.glEnable)
        G.glEnable(GL_SCISSOR_TEST);
    if (G.glColorMask)
        G.glColorMask(1u, 1u, 1u, 1u);
    G.glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    overlay_fill(0, 0, FPS_W, FPS_H, scissor_fn, clear_fn);
    G.glClearColor(1.0f, 0.92f, 0.22f, 1.0f);
    overlay_glyph(ox, oy, s, FONT5_F, scissor_fn, clear_fn);
    overlay_glyph(ox + gw, oy, s, FONT5_P, scissor_fn, clear_fn);
    overlay_glyph(ox + gw * 2, oy, s, FONT5_S, scissor_fn, clear_fn);
    overlay_glyph(ox + gw * 4, oy, s, FONT5_DIGIT[d2], scissor_fn, clear_fn);
    overlay_glyph(ox + gw * 5, oy, s, FONT5_DIGIT[d1], scissor_fn, clear_fn);
    overlay_glyph(ox + gw * 6, oy, s, FONT5_DIGIT[d0], scissor_fn, clear_fn);
    overlay_cursor = 0;
    if (G.glDisable)
        G.glDisable(GL_SCISSOR_TEST);
    win_w = saved_w;
    win_h = saved_h;
    /* Never leave the window FB or a 1280×720 viewport bound: splash/menu
     * often never re-bind and would then draw 640×480 into the corner. */
    if (real_bind_fb)
        real_bind_fb(GL_FRAMEBUFFER, game_fbo);
    if (G.glViewport)
        G.glViewport(0, 0, game_w, game_h);
    fps_baked = fps_shown;
    mem_baked_total = mem_total_mib;
    mem_baked_used = mem_used_mib;
    stick_baked_x = stick_lx;
    stick_baked_y = stick_ly;
    probe_baked = tspgl_probe_mode();
}

static int create_fps_overlay(void)
{
    void (*gen_tex)(int32_t, uint32_t *) = G.glGenTextures;
    void (*tex_parami)(uint32_t, uint32_t, int32_t) = real_tex_parami;
    void (*gen_fb)(int32_t, uint32_t *) = G.glGenFramebuffers;
    void (*fb_tex)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t) =
        G.glFramebufferTexture2D;

    if (!gen_tex || !real_tex_image || !tex_parami || !gen_fb || !real_bind_fb ||
        !fb_tex || !real_bind_tex)
        return -1;
    gen_tex(1, &fps_color);
    real_bind_tex(GL_TEXTURE_2D, fps_color);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int32_t)GL_NEAREST);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int32_t)GL_NEAREST);
    tex_parami(GL_TEXTURE_2D, 0x2802, (int32_t)GL_CLAMP_TO_EDGE);
    tex_parami(GL_TEXTURE_2D, 0x2803, (int32_t)GL_CLAMP_TO_EDGE);
    real_tex_image(GL_TEXTURE_2D, 0, (int32_t)0x8058, FPS_W, FPS_H, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, NULL);
    gen_fb(1, &fps_fbo);
    real_bind_fb(GL_FRAMEBUFFER, fps_fbo);
    fb_tex(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fps_color, 0);
    real_bind_tex(GL_TEXTURE_2D, 0);
    mem_refresh();
    fps_bake();
    if (real_bind_fb)
        real_bind_fb(GL_FRAMEBUFFER, game_fbo);
    if (G.glViewport)
        G.glViewport(0, 0, game_w, game_h);
    return 0;
}

static void present_draw_fps(void)
{
    const int s = 3;
    const int gw = 5 * s + s;
    const int oy = 14;
    int ox = win_w - FPS_W - 2;
    int n = (int)fps_shown;
    int d2, d1, d0;

    if (!real_bind_fb || !G.glClearColor || !real_scissor || !G.glClear)
        return;
    if (n < 0)
        n = 0;
    if (n > 999)
        n = 999;
    d2 = n / 100;
    d1 = (n / 10) % 10;
    d0 = n % 10;
    if (ox < 0)
        ox = 0;
    real_bind_fb(GL_FRAMEBUFFER, 0);
    if (G.glViewport)
        G.glViewport(0, 0, win_w, win_h);
    if (G.glColorMask)
        G.glColorMask(1u, 1u, 1u, 1u);
    if (G.glEnable)
        G.glEnable(GL_SCISSOR_TEST);
    G.glClearColor(1.0f, 0.92f, 0.22f, 1.0f);
    overlay_glyph(ox, oy, s, FONT5_F, real_scissor, G.glClear);
    overlay_glyph(ox + gw, oy, s, FONT5_P, real_scissor, G.glClear);
    overlay_glyph(ox + gw * 2, oy, s, FONT5_S, real_scissor, G.glClear);
    overlay_glyph(ox + gw * 4, oy, s, FONT5_DIGIT[d2], real_scissor, G.glClear);
    overlay_glyph(ox + gw * 5, oy, s, FONT5_DIGIT[d1], real_scissor, G.glClear);
    overlay_glyph(ox + gw * 6, oy, s, FONT5_DIGIT[d0], real_scissor, G.glClear);
    if (G.glDisable)
        G.glDisable(GL_SCISSOR_TEST);
    real_bind_fb(GL_FRAMEBUFFER, game_fbo);
    if (G.glViewport)
        G.glViewport(0, 0, game_w, game_h);
}

static int cmdline_has(const char *buf, size_t n, const char *needle)
{
    size_t nl = strlen(needle);
    size_t i;

    if (nl == 0 || n < nl)
        return 0;
    for (i = 0; i + nl <= n; ++i) {
        if (memcmp(buf + i, needle, nl) == 0)
            return 1;
    }
    return 0;
}

static int cmdline_arg_basename(const char *cmd, size_t n, const char *want)
{
    size_t i = 0;

    while (i < n) {
        const char *a = cmd + i;
        const char *base;
        size_t L = 0;
        while (i + L < n && a[L] != 0)
            L++;
        if (L == 0) {
            i++;
            continue;
        }
        base = a;
        {
            size_t k;
            for (k = 0; k < L; ++k)
                if (a[k] == '/')
                    base = a + k + 1;
        }
        if (strcmp(base, want) == 0)
            return 1;
        i += L + 1;
    }
    return 0;
}

static void kill_port_siblings(void)
{
    DIR *d;
    struct dirent *e;
    pid_t self = getpid();
    pid_t parent = getppid();

    /* Never kill(0): on stock TrimUI the port shares a process group with
     * MainUI, and signalling the group reboots the shell. */
    d = opendir("/proc");
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        char path[64];
        char comm[32];
        char cmd[512];
        FILE *f;
        pid_t pid;
        size_t n;
        int hit = 0;

        if (!isdigit((unsigned char)e->d_name[0]))
            continue;
        pid = (pid_t)atoi(e->d_name);
        if (pid <= 1 || pid == self || pid == parent)
            continue;
        snprintf(path, sizeof(path), "/proc/%s/comm", e->d_name);
        f = fopen(path, "r");
        comm[0] = 0;
        if (f) {
            if (fgets(comm, sizeof(comm), f)) {
                size_t L = strlen(comm);
                if (L && comm[L - 1] == '\n')
                    comm[L - 1] = 0;
            }
            fclose(f);
        }
        if (strncmp(comm, "deadspace_p", 11) == 0)
            continue;
        snprintf(path, sizeof(path), "/proc/%s/cmdline", e->d_name);
        f = fopen(path, "r");
        n = 0;
        cmd[0] = 0;
        if (f) {
            n = fread(cmd, 1, sizeof(cmd) - 1, f);
            cmd[n] = 0;
            fclose(f);
        }
        if (cmdline_has(cmd, n, "deadspace_present"))
            continue;
        if (strcmp(comm, "gptokeyb") == 0 ||
            cmdline_arg_basename(cmd, n, "gptokeyb"))
            hit = 1;
        else if (strcmp(comm, "deadspace") == 0 ||
                 cmdline_arg_basename(cmd, n, "deadspace"))
            hit = 1;
        else if (strcmp(comm, "masseffect") == 0 ||
                 cmdline_arg_basename(cmd, n, "masseffect"))
            hit = 1;
        if (!hit)
            continue;
        fprintf(stderr, "tspgl-srv: term pid=%d comm=%s\n", (int)pid, comm);
        kill(pid, SIGTERM);
    }
    closedir(d);
}

static void present_draw_cursor(int dx, int dy, int dw, int dh)
{
    int gx, gy, sx, sy, gl_y;
    int arm;
    int gh = game_h > 0 ? game_h : 480;
    int gw = game_w > 0 ? game_w : 640;
    void (*clear_fn)(uint32_t) = real_clear ? real_clear : G.glClear;
    void (*scissor_fn)(int32_t, int32_t, int32_t, int32_t) =
        real_scissor ? real_scissor : G.glScissor;

    if (!cursor_hits || !clear_fn || !scissor_fn || !G.glClearColor)
        return;
    gx = (cursor_minx + cursor_maxx) / 2;
    gy = gh - 1 - (cursor_miny + cursor_maxy) / 2;
    cursor_hits = 0;
    if (dw < 1)
        dw = win_w;
    if (dh < 1)
        dh = win_h;
    sx = dx + gx * dw / gw;
    sy = dy + gy * dh / gh;
    gl_y = win_h - 1 - sy;
    arm = 14;
    overlay_cursor = 1;
    if (G.glEnable)
        G.glEnable(GL_SCISSOR_TEST);
    if (G.glColorMask)
        G.glColorMask(1u, 1u, 1u, 1u);
    /* Dead Space menu cyan (locator / button glow), not the OS mouse. */
    G.glClearColor(0.35f, 0.90f, 0.95f, 1.0f);
    scissor_fn(sx - arm, gl_y - 1, arm * 2 + 1, 3);
    clear_fn(GL_COLOR_BUFFER_BIT);
    scissor_fn(sx - 1, gl_y - arm, 3, arm * 2 + 1);
    clear_fn(GL_COLOR_BUFFER_BIT);
    overlay_cursor = 0;
}

/* ---------------------------------------------------------------------------
 * Boot splash – shown until the game uploads a real (>=512px) texture, or
 * until 12 seconds have elapsed from the first client connection.
 * --------------------------------------------------------------------------*/
static void load_boot_splash(void)
{
    void (*gen_tex)(int32_t, uint32_t *) = G.glGenTextures;
    void (*bind_tex)(uint32_t, uint32_t) = G.glBindTexture;
    void (*tex_parami)(uint32_t, uint32_t, int32_t) = G.glTexParameteri;
    void (*tex_image)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                      uint32_t, uint32_t, const void *);
    void (*gen_fb)(int32_t, uint32_t *) = G.glGenFramebuffers;
    void (*fb_tex)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t) =
        G.glFramebufferTexture2D;
    void (*blit)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                 int32_t, uint32_t, uint32_t) = G.glBlitFramebuffer;
    char path[512];
    const char *env;
    int w = 0, h = 0, comp = 0;
    unsigned char *pixels = NULL;

    tex_image = (void *)gl_get("glTexImage2D");
    if (!gen_tex || !bind_tex || !tex_parami || !tex_image ||
        !gen_fb || !fb_tex || !blit || !real_bind_fb) {
        fprintf(stderr, "tspgl-srv: boot splash missing GL, skip\n");
        return;
    }

    /* resolve image path */
    path[0] = '\0';
    env = getenv("DEADSPACE_BOOT_SPLASH");
    if (env && env[0]) {
        snprintf(path, sizeof(path), "%s", env);
    }
    if (!path[0] || access(path, R_OK) != 0) {
        if (access("./boot_splash.png", R_OK) == 0)
            snprintf(path, sizeof(path), "%s", "./boot_splash.png");
    }
    if (!path[0] || access(path, R_OK) != 0) {
        /* fall back to directory of the running executable */
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 20);
        if (len > 0) {
            path[len] = '\0';
            char *sl = strrchr(path, '/');
            if (sl)
                strcpy(sl + 1, "boot_splash.png");
        }
    }
    if (!path[0] || access(path, R_OK) != 0) {
        fprintf(stderr, "tspgl-srv: boot splash missing\n");
        return;
    }

    /* load PNG, flip vertically for GL (bottom-left origin) */
    stbi_set_flip_vertically_on_load(1);
    pixels = stbi_load(path, &w, &h, &comp, 4);
    if (!pixels) {
        fprintf(stderr, "tspgl-srv: boot splash stbi_load fail: %s\n",
                stbi_failure_reason());
        return;
    }
    fprintf(stderr, "tspgl-srv: boot splash loaded %dx%d\n", w, h);

    /* upload texture */
    gen_tex(1, &splash_tex);
    bind_tex(GL_TEXTURE_2D, splash_tex);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (int32_t)GL_NEAREST);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (int32_t)GL_NEAREST);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (int32_t)GL_CLAMP_TO_EDGE);
    tex_parami(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (int32_t)GL_CLAMP_TO_EDGE);
    tex_image(GL_TEXTURE_2D, 0, (int32_t)GL_RGBA, w, h, 0,
              GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    /* attach texture to a dedicated read FBO */
    gen_fb(1, &splash_fbo);
    real_bind_fb(GL_FRAMEBUFFER, splash_fbo);
    fb_tex(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, splash_tex, 0);

    /* restore game FBO */
    if (game_fbo)
        real_bind_fb(GL_FRAMEBUFFER, game_fbo);
    else
        real_bind_fb(GL_FRAMEBUFFER, 0);

    splash_active = 1;

    /* draw immediately so the window is never garbage before the first swap */
    if (G.glViewport)
        G.glViewport(0, 0, win_w, win_h);
    real_bind_fb(GL_READ_FRAMEBUFFER, splash_fbo);
    real_bind_fb(GL_DRAW_FRAMEBUFFER, 0);
    blit(0, 0, w, h, 0, 0, win_w, win_h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    sdl.gl_swap(window);

    /* restore bindings */
    if (game_fbo) {
        real_bind_fb(GL_FRAMEBUFFER, game_fbo);
        if (G.glViewport)
            G.glViewport(0, 0, game_w, game_h);
    }
}

static void present_swap(void)
{
    void (*blit)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                 int32_t, uint32_t, uint32_t) = G.glBlitFramebuffer;
    int dw, dh, dx, dy;
    static unsigned shown;

    /* --- boot splash: cover window until game has uploaded real textures --- */
    if (splash_active && splash_fbo && blit) {
        int32_t vp[4] = {0, 0, game_w, game_h};
        int32_t sc[4] = {0, 0, game_w, game_h};
        uint32_t scissor_on = 0;

        /* advance hide countdown (started by wrap_tex_image2d on large tex) */
        if (first_client_t && now_ms() - first_client_t >= 12000u) {
            fprintf(stderr, "tspgl-srv: boot splash hide (timeout)\n");
            splash_active = 0;
        } else if (splash_hide_pending > 0 && --splash_hide_pending == 0) {
            splash_active = 0;
        }

        if (splash_active) {
            /* save game FBO state */
            if (real_get_integerv) {
                real_get_integerv(GL_VIEWPORT, vp);
                real_get_integerv(GL_SCISSOR_BOX, sc);
            }
            if (G.glIsEnabled)
                scissor_on = G.glIsEnabled(GL_SCISSOR_TEST);
            if (scissor_on && G.glDisable)
                G.glDisable(GL_SCISSOR_TEST);

            /* blit splash → window (full 1280×720) */
            real_bind_fb(GL_READ_FRAMEBUFFER, splash_fbo);
            real_bind_fb(GL_DRAW_FRAMEBUFFER, 0);
            if (G.glViewport)
                G.glViewport(0, 0, win_w, win_h);
            blit(0, 0, win_w, win_h, 0, 0, win_w, win_h,
                 GL_COLOR_BUFFER_BIT, GL_NEAREST);
            present_draw_cursor(0, 0, win_w, win_h);
            fps_tick();
            present_draw_fps();
            sdl.gl_swap(window);
            tspgl_stage_flip();
            input_soon = 1;
            ++shown;

            /* restore game FBO + viewport/scissor */
            if (real_bind_fb)
                real_bind_fb(GL_FRAMEBUFFER, game_fbo);
            if (G.glViewport)
                G.glViewport(vp[0], vp[1], vp[2], vp[3]);
            if (G.glScissor)
                G.glScissor(sc[0], sc[1], sc[2], sc[3]);
            if (scissor_on && G.glEnable)
                G.glEnable(GL_SCISSOR_TEST);
            return;
        }
    }
    /* --- end boot splash --- */

    if (!game_fbo) {
        present_draw_cursor(0, 0, win_w, win_h);
        fps_tick();
        present_draw_fps();
        sdl.gl_swap(window);
        tspgl_stage_flip();
        input_soon = 1;
        ++shown;
        if (shown == 1)
            fprintf(stderr, "tspgl-srv: swap %u (default FB) fps=%u\n", shown,
                    fps_shown);
        return;
    }
    int32_t vp[4];
    int32_t sc[4];
    uint32_t scissor_on = 0;

    vp[0] = 0;
    vp[1] = 0;
    vp[2] = game_w;
    vp[3] = game_h;
    sc[0] = 0;
    sc[1] = 0;
    sc[2] = game_w;
    sc[3] = game_h;

    if (present_letterbox) {
        if ((long)win_w * (long)game_h <= (long)win_h * (long)game_w) {
            dw = win_w;
            dh = (int)((long)game_h * win_w / game_w);
        } else {
            dh = win_h;
            dw = (int)((long)game_w * win_h / game_h);
        }
        if (dw < 1)
            dw = 1;
        if (dh < 1)
            dh = 1;
        dx = (win_w - dw) / 2;
        dy = (win_h - dh) / 2;
    } else {
        dw = win_w;
        dh = win_h;
        dx = 0;
        dy = 0;
    }

    if (real_get_integerv) {
        memset(vp, 0, sizeof(vp));
        memset(sc, 0, sizeof(sc));
        vp[2] = game_w;
        vp[3] = game_h;
        sc[2] = game_w;
        sc[3] = game_h;
        real_get_integerv(GL_VIEWPORT, vp);
        real_get_integerv(GL_SCISSOR_BOX, sc);
    }
    if (G.glIsEnabled)
        scissor_on = G.glIsEnabled(GL_SCISSOR_TEST);
    if (scissor_on && G.glDisable)
        G.glDisable(GL_SCISSOR_TEST);

    if (game_fbo && client_draw_fb != 0) {
        static int nskip;
        if (nskip < 8) {
            fprintf(stderr, "tspgl-srv: skip present (offscreen fb=%u)\n",
                    client_draw_fb);
            nskip++;
        }
        tspgl_stage_flip();
        input_soon = 1;
        return;
    }

    if (real_bind_fb) {
        real_bind_fb(GL_READ_FRAMEBUFFER, game_fbo);
        real_bind_fb(GL_DRAW_FRAMEBUFFER, 0);
    }
    if (G.glViewport)
        G.glViewport(0, 0, win_w, win_h);
    if (blit)
        blit(0, 0, game_w, game_h, dx, dy, dx + dw, dy + dh, GL_COLOR_BUFFER_BIT,
             GL_NEAREST);
    present_draw_cursor(dx, dy, dw, dh);
    fps_tick();
    present_draw_fps();
    sdl.gl_swap(window);
    tspgl_scratch_retire_frame();
    tspgl_stage_flip();
    input_soon = 1;
    if (real_bind_fb)
        real_bind_fb(GL_FRAMEBUFFER,
                     client_draw_fb ? client_draw_fb : game_fbo);
    if (G.glViewport)
        G.glViewport(vp[0], vp[1], vp[2], vp[3]);
    if (G.glScissor)
        G.glScissor(sc[0], sc[1], sc[2], sc[3]);
    if (scissor_on && G.glEnable)
        G.glEnable(GL_SCISSOR_TEST);
    ++shown;
    if (shown == 1)
        fprintf(stderr, "tspgl-srv: swap %u blit %dx%d -> %dx%d+%d+%d fps=%u\n",
                shown, game_w, game_h, dw, dh, dx, dy, fps_shown);
}

static int full_write(int fd, const void *buf, size_t n)
{
    const uint8_t *p = buf;
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += (size_t)w;
        n -= (size_t)w;
    }
    return 0;
}

static int full_read(int fd, void *buf, size_t n)
{
    uint8_t *p = buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (r == 0)
            return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

static uint8_t inbuf_s[512 * 1024];
static uint8_t outbuf_s[2 * 1024 * 1024];

static int drain_bytes(int fd, uint32_t n)
{
    uint8_t junk[4096];
    while (n) {
        uint32_t c = n > sizeof(junk) ? (uint32_t)sizeof(junk) : n;
        if (full_read(fd, junk, c) != 0)
            return -1;
        n -= c;
    }
    return 0;
}

static int handle_client(int fd)
{
    struct tspgl_hdr h, r;
    uint32_t out_n = 0;
    int rc;
    uint8_t *in = inbuf_s;
    uint8_t *out = outbuf_s;
    uint8_t *in_h = NULL;
    int ret = -1;

    if (full_read(fd, &h, sizeof(h)) != 0)
        return -1;
    r.magic = TSPGL_MAGIC;
    r.seq = h.seq;
    r.op = TSPGL_OK;
    r.len = 0;
    if (h.magic != TSPGL_MAGIC) {
        fprintf(stderr, "tspgl-srv: bad magic 0x%x fd=%d\n", h.magic, fd);
        return -1;
    }
    if (h.len > TSPGL_MAX_BLOB) {
        fprintf(stderr, "tspgl-srv: blob %u too big op=%u\n", h.len, h.op);
        if (drain_bytes(fd, h.len) != 0)
            return -1;
        if (tspgl_needs_reply(h.op)) {
            r.op = TSPGL_ERR;
            if (full_write(fd, &r, sizeof(r)) != 0)
                return -1;
        }
        return 0;
    }
    if (h.len > sizeof(inbuf_s)) {
        in_h = malloc(h.len);
        if (!in_h) {
            fprintf(stderr, "tspgl-srv: OOM len=%u op=%u\n", h.len, h.op);
            drain_bytes(fd, h.len);
            return -1;
        }
        in = in_h;
    }
    if (h.len && full_read(fd, in, h.len) != 0)
        goto out_free;

    if (h.op == OP_PING) {
        rc = 0;
    } else if (h.op == OP_EGL_SWAP) {
        present_swap();
        rc = 0;
    } else {
        rc = tspgl_dispatch_es1(h.op, in, h.len, out, &out_n);
        if (rc != 0)
            rc = tspgl_dispatch_simple(h.op, (const uint32_t *)in, h.len,
                                       (uint32_t *)out, &out_n);
        if (rc != 0)
            rc = tspgl_dispatch_special(h.op, in, h.len, out, &out_n);
        if (h.op == OP_glActiveTexture && h.len >= 4)
            tspgl_es1_set_active_tex(*(const uint32_t *)in);
        if (h.op == OP_glFinish)
            tspgl_stage_flip();
        if (rc != 0) {
            fprintf(stderr, "tspgl-srv: unknown op %u len=%u\n", h.op, h.len);
            r.op = TSPGL_ERR;
            out_n = 0;
        } else if (h.op == OP_glCompileShader && h.len >= 4 && G.glGetShaderiv) {
            uint32_t shader;
            int32_t ok = 1;
            char slog[1024];
            int32_t slen = 0;
            void (*getiv)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetShaderiv;
            void (*getlog)(uint32_t, int32_t, int32_t *, char *) =
                (void *)G.glGetShaderInfoLog;
            memcpy(&shader, in, 4);
            getiv(shader, 0x8B81, &ok);
            if (!ok) {
                memset(slog, 0, sizeof(slog));
                if (getlog)
                    getlog(shader, (int32_t)sizeof(slog) - 1, &slen, slog);
                fprintf(stderr, "tspgl-srv: COMPILE FAIL id=%u: %s\n", shader,
                        slog);
            }
        } else if (h.op == OP_glLinkProgram && h.len >= 4 && G.glGetProgramiv) {
            uint32_t prog;
            int32_t ok = 1;
            char plog[1024];
            int32_t plen = 0;
            void (*getiv)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetProgramiv;
            void (*getlog)(uint32_t, int32_t, int32_t *, char *) =
                (void *)G.glGetProgramInfoLog;
            memcpy(&prog, in, 4);
            tspgl_probe_note_program(prog);
            getiv(prog, 0x8B82, &ok);
            if (!ok) {
                memset(plog, 0, sizeof(plog));
                if (getlog)
                    getlog(prog, (int32_t)sizeof(plog) - 1, &plen, plog);
                fprintf(stderr, "tspgl-srv: LINK FAIL id=%u: %s\n", prog, plog);
            }
        }
    }
    (void)rc;
    if (!tspgl_needs_reply(h.op)) {
        ret = 0;
        goto out_free;
    }
    if (out_n > sizeof(outbuf_s)) {
        fprintf(stderr, "tspgl-srv: out %u truncated\n", out_n);
        out_n = (uint32_t)sizeof(outbuf_s);
    }
    r.len = out_n;
    if (full_write(fd, &r, sizeof(r)) != 0)
        goto out_free;
    if (r.len && full_write(fd, out, r.len) != 0)
        goto out_free;
    ret = 0;
out_free:
    free(in_h);
    return ret;
}

static int setup_listen(void)
{
    struct sockaddr_un addr;
    int fd;
    unlink(TSPGL_SOCK);
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "tspgl-srv: socket errno=%d\n", errno);
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TSPGL_SOCK, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "tspgl-srv: bind %s errno=%d\n", TSPGL_SOCK, errno);
        close(fd);
        return -1;
    }
    if (listen(fd, 8) != 0) {
        fprintf(stderr, "tspgl-srv: listen errno=%d\n", errno);
        close(fd);
        return -1;
    }
    listen_fd = fd;
    fprintf(stderr, "tspgl-srv: listen %s\n", TSPGL_SOCK);
    return 0;
}

int main(void)
{
    const char *ew, *eh;
    uint32_t *hdr;
    FILE *ready;
    struct pollfd pf[9];
    int ncli = 0;
    int cli[8];
    unsigned wait_pad = 0;
    int i;

    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    fprintf(stderr, "tspgl-srv: start (32->64 GLES1 bridge) build=61\n");

    ew = getenv("TSPGL_WIDTH");
    eh = getenv("TSPGL_HEIGHT");
    if (ew)
        game_w = atoi(ew);
    if (eh)
        game_h = atoi(eh);
    if (game_w < 1)
        game_w = 640;
    if (game_h < 1)
        game_h = 480;
    {
        const char *mode = getenv("TSPGL_PRESENT");
#ifdef TSPGL_DEFAULT_LETTERBOX
        present_letterbox = 1;
#endif
        if (mode != NULL) {
            if (strcmp(mode, "letterbox") == 0 || strcmp(mode, "fit") == 0)
                present_letterbox = 1;
            else if (strcmp(mode, "stretch") == 0 || strcmp(mode, "fill") == 0)
                present_letterbox = 0;
        }
        fprintf(stderr, "tspgl-srv: present %s %dx%d -> %dx%d\n",
                present_letterbox ? "letterbox" : "stretch", game_w, game_h,
                win_w, win_h);
    }

    frame_map = open_frame();
    if (!frame_map)
        return 1;
    hdr = (uint32_t *)frame_map;
    hdr[NFSMW_HDR_MAGIC] = NFSMW_FRAME_MAGIC;

    if (load_sdl() != 0)
        return 1;
    if (sdl.set_hint) {
        sdl.set_hint("SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS", "0");
        sdl.set_hint("SDL_HINT_RENDER_VSYNC", "0");
        sdl.set_hint("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS", "1");
        sdl.set_hint("SDL_JOYSTICK_HIDAPI", "0");
    }
    if (sdl.init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) !=
        0) {
        fprintf(stderr, "tspgl-srv: SDL_Init+pad failed (%s), video only\n",
                sdl.get_error());
        if (sdl.init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "tspgl-srv: SDL_Init: %s\n", sdl.get_error());
            return 1;
        }
    }
    sdl.gl_set_attr(SDL_GL_RED_SIZE, 8);
    sdl.gl_set_attr(SDL_GL_GREEN_SIZE, 8);
    sdl.gl_set_attr(SDL_GL_BLUE_SIZE, 8);
    sdl.gl_set_attr(SDL_GL_ALPHA_SIZE, 8);
    sdl.gl_set_attr(SDL_GL_DOUBLEBUFFER, 1);
    sdl.gl_set_attr(SDL_GL_DEPTH_SIZE, 24);
    sdl.gl_set_attr(SDL_GL_STENCIL_SIZE, 8);
    sdl.gl_set_attr(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    sdl.gl_set_attr(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    sdl.gl_set_attr(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    window = sdl.create_window("deadspace-glbridge", SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED, win_w, win_h,
                               SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN |
                                   SDL_WINDOW_OPENGL);
    if (!window) {
        fprintf(stderr, "tspgl-srv: CreateWindow: %s\n", sdl.get_error());
        return 1;
    }
    glctx = sdl.gl_create_context(window);
    if (!glctx) {
        fprintf(stderr, "tspgl-srv: GLES3 context failed (%s), try ES2\n",
                sdl.get_error());
        sdl.gl_set_attr(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        sdl.gl_set_attr(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        glctx = sdl.gl_create_context(window);
    }
    if (!glctx) {
        fprintf(stderr, "tspgl-srv: CreateContext: %s\n", sdl.get_error());
        return 1;
    }
    if (sdl.gl_make_current(window, glctx) != 0) {
        fprintf(stderr, "tspgl-srv: MakeCurrent: %s\n", sdl.get_error());
        return 1;
    }
    if (sdl.gl_set_swap)
        sdl.gl_set_swap(0);
    sdl.show_cursor(0);
    fprintf(stderr, "tspgl-srv: window %dx%d GL context ok\n", win_w, win_h);

    memset(&G, 0, sizeof(G));
    tspgl_load_simple(gl_get);
    tspgl_load_specials(gl_get);
    tspgl_hook_volume_mask();
    if (!G.glBlitFramebuffer)
        G.glBlitFramebuffer = (void *)gl_get("glBlitFramebuffer");
    if (!G.glGenVertexArrays)
        G.glGenVertexArrays = (void *)gl_get("glGenVertexArraysOES");
    if (!G.glDeleteVertexArrays)
        G.glDeleteVertexArrays = (void *)gl_get("glDeleteVertexArraysOES");
    if (!G.glBindVertexArray)
        G.glBindVertexArray = gl_get("glBindVertexArrayOES");
    if (!G.glIsVertexArray)
        G.glIsVertexArray = gl_get("glIsVertexArrayOES");
    {
        const uint8_t *(*gs)(uint32_t) = (void *)G.glGetString;
        const char *ven = "", *ren = "", *ver = "", *ext = "";
        if (gs) {
            if (gs(0x1F00))
                ven = (const char *)gs(0x1F00);
            if (gs(0x1F01))
                ren = (const char *)gs(0x1F01);
            if (gs(0x1F02))
                ver = (const char *)gs(0x1F02);
            if (gs(0x1F03))
                ext = (const char *)gs(0x1F03);
        }
        fprintf(stderr, "tspgl-srv: GL vendor=%s renderer=%s version=%s\n", ven,
                ren, ver);
        fprintf(stderr, "tspgl-srv: GLext %s\n", ext);
        have_fb_fetch = 0;
        have_fb_fetch_nc = 0;
        if (ext) {
            const char *e = ext;
            while (*e) {
                const char *n = e;
                size_t nl;
                while (*e && *e != ' ')
                    e++;
                nl = (size_t)(e - n);
                if (nl == sizeof("GL_EXT_shader_framebuffer_fetch") - 1 &&
                    strncmp(n, "GL_EXT_shader_framebuffer_fetch", nl) == 0)
                    have_fb_fetch = 1;
                if (nl == sizeof("GL_EXT_shader_framebuffer_fetch_non_coherent") - 1 &&
                    strncmp(n, "GL_EXT_shader_framebuffer_fetch_non_coherent", nl) == 0)
                    have_fb_fetch_nc = 1;
                while (*e == ' ')
                    e++;
            }
        }
        fprintf(stderr, "tspgl-srv: fb_fetch=%d noncoherent=%d\n", have_fb_fetch,
                have_fb_fetch_nc);
    }

    if (create_game_fbo() != 0) {
        fprintf(stderr, "tspgl-srv: FBO failed, default framebuffer\n");
        game_fbo = 0;
    }
    real_bind_fb = G.glBindFramebuffer;
    if (game_fbo)
        G.glBindFramebuffer = wrap_bind_fb;
    real_get_integerv = (void (*)(uint32_t, int32_t *))(uintptr_t)G.glGetIntegerv;
    G.glGetIntegerv = (void *)(uintptr_t)wrap_get_integerv;
    real_check_fb = G.glCheckFramebufferStatus;
    G.glCheckFramebufferStatus = wrap_check_fb;
    real_draw_buffers = (void (*)(int32_t, const uint32_t *))(uintptr_t)gl_get(
        "glDrawBuffers");
    real_tex_image = (void *)G.glTexImage2D;
    if (real_tex_image)
        G.glTexImage2D = (void *)(uintptr_t)wrap_tex_image2d;
    real_tex_sub = (void *)G.glTexSubImage2D;
    if (real_tex_sub)
        G.glTexSubImage2D = (void *)(uintptr_t)wrap_tex_sub_image2d;
    real_tex_parami = G.glTexParameteri;
    if (real_tex_parami)
        G.glTexParameteri = wrap_tex_parami;
    real_tex_paramf = G.glTexParameterf;
    if (real_tex_paramf)
        G.glTexParameterf = wrap_tex_paramf;
    real_bind_tex = G.glBindTexture;
    if (real_bind_tex)
        G.glBindTexture = wrap_bind_texture;
    real_compressed = (void *)G.glCompressedTexImage2D;
    if (real_compressed)
        G.glCompressedTexImage2D = (void *)(uintptr_t)wrap_compressed;
    real_gen_mipmap = G.glGenerateMipmap;
    if (real_gen_mipmap)
        G.glGenerateMipmap = wrap_gen_mipmap;
    real_delete_tex = G.glDeleteTextures;
    if (real_delete_tex)
        G.glDeleteTextures = wrap_delete_tex;
    real_clear = G.glClear;
    if (real_clear)
        G.glClear = wrap_clear;
    real_scissor = G.glScissor;
    if (real_scissor)
        G.glScissor = wrap_scissor;
    if (real_bind_fb && game_fbo)
        real_bind_fb(GL_FRAMEBUFFER, game_fbo);
    if (G.glViewport)
        G.glViewport(0, 0, game_w, game_h);
    tspgl_es1_init();
    create_fps_overlay();
    load_boot_splash();

    if (setup_listen() != 0)
        return 1;

    pad = open_pad();
    hdr[NFSMW_HDR_READY] = 1;
    ready = fopen("/tmp/deadspace.present.ready", "w");
    if (ready) {
        fputs("ok\n", ready);
        fclose(ready);
    }
    fprintf(stderr, "tspgl-srv: ready\n");

    memset(cli, -1, sizeof(cli));
    {
        uint32_t last_sdl = 0;
        for (;;) {
            sdl_event ev;
            int nfds;
            int ret;
            uint32_t t = now_ms();

            if (!ncli || input_soon || (t - last_sdl) >= 8u) {
                while (sdl.poll_event(&ev)) {
                    if (ev.type == 0x100) {
                        request_quit = 1;
                        goto done;
                    }
                }
                if (!pad) {
                    ++wait_pad;
                    if ((wait_pad % 30u) == 0u)
                        pad = open_pad();
                }
                publish_input(hdr);
                last_sdl = t;
                input_soon = 0;
                if (request_quit)
                    goto done;
            }

            pf[0].fd = listen_fd;
            pf[0].events = POLLIN;
            pf[0].revents = 0;
            nfds = 1;
            for (i = 0; i < ncli; ++i) {
                pf[nfds].fd = cli[i];
                pf[nfds].events = POLLIN;
                pf[nfds].revents = 0;
                ++nfds;
            }
            ret = poll(pf, (nfds_t)nfds, ncli ? 8 : 50);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "tspgl-srv: poll errno=%d\n", errno);
            break;
        }
        {
            int nwas = ncli;
            if (pf[0].revents & POLLIN) {
                int cfd = accept(listen_fd, NULL, NULL);
                if (cfd >= 0) {
                    if (ncli < 8) {
                        int buf = 1024 * 1024;
                        setsockopt(cfd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
                        setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
                        cli[ncli++] = cfd;
                        fprintf(stderr, "tspgl-srv: client fd=%d n=%d\n", cfd,
                                ncli);
                        if (ncli == 1 && splash_active)
                            first_client_t = now_ms();
                    } else {
                        close(cfd);
                    }
                }
            }
            for (i = 0; i < nwas; ++i) {
                if (!(pf[i + 1].revents & (POLLIN | POLLHUP | POLLERR)))
                    continue;
                if (handle_client(cli[i]) != 0) {
                    fprintf(stderr, "tspgl-srv: client fd=%d drop\n",
                            cli[i]);
                    close(cli[i]);
                    cli[i] = -1;
                }
            }
            {
                int spin;
                for (spin = 0; spin < 64; ++spin) {
                    int more_work = 0;
                    for (i = 0; i < nwas; ++i) {
                        struct pollfd more;
                        if (cli[i] < 0)
                            continue;
                        more.fd = cli[i];
                        more.events = POLLIN;
                        more.revents = 0;
                        if (poll(&more, 1, 0) <= 0 ||
                            !(more.revents & POLLIN))
                            continue;
                        if (handle_client(cli[i]) != 0) {
                            fprintf(stderr, "tspgl-srv: client fd=%d drop\n",
                                    cli[i]);
                            close(cli[i]);
                            cli[i] = -1;
                        } else
                            more_work = 1;
                    }
                    if (!more_work)
                        break;
                }
            }
            {
                int w = 0;
                for (i = 0; i < ncli; ++i) {
                    if (cli[i] >= 0)
                        cli[w++] = cli[i];
                }
                ncli = w;
            }
        }
        }
    }

done:
    unlink("/tmp/deadspace.present.ready");
    unlink(TSPGL_SOCK);
    if (glctx)
        sdl.gl_delete_context(glctx);
    if (window)
        sdl.destroy_window(window);
    sdl.quit();
    if (request_quit)
        kill_port_siblings();
    fprintf(stderr, "tspgl-srv: exit\n");
    return 0;
}
