/* GLES specials for the 64-bit PowerVR server. Included from server.c. */

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0

struct srv_attrib {
    int32_t size;
    uint32_t type;
    uint32_t normalized;
    int32_t stride;
    int is_offset;
};
static struct srv_attrib sattr[16];
static int arr_en[16];
static uint32_t scratch_vbo[16];
static uint32_t scratch_ibo;
static uint32_t srv_bound_array;
static uint32_t srv_bound_element;

/* Menu+Select. The stripe is a blend+depth mesh with count > 6.
 * Bar colour splits that class by index count. */
static int probe_mode;
static uint32_t probe_skin_sh[96];
static int probe_skin_sn;
static uint32_t probe_skin_pr[96];
static int probe_skin_pn;
static uint32_t probe_white;

static int probe_has(const uint32_t *t, int n, uint32_t id)
{
    int i;
    for (i = 0; i < n; ++i)
        if (t[i] == id)
            return 1;
    return 0;
}

static void probe_add(uint32_t *t, int *n, int cap, uint32_t id)
{
    if (!id || *n >= cap || probe_has(t, *n, id))
        return;
    t[(*n)++] = id;
}

static void probe_note_shader(uint32_t shader, const char *src)
{
    if (src && strstr(src, "g_BonePalette"))
        probe_add(probe_skin_sh, &probe_skin_sn, 96, shader);
}

void tspgl_probe_note_program(uint32_t prog)
{
    void (*get)(uint32_t, int32_t, int32_t *, uint32_t *) =
        (void *)G.glGetAttachedShaders;
    uint32_t sh[8];
    int32_t n = 0, i;
    if (!prog || !get)
        return;
    get(prog, 8, &n, sh);
    for (i = 0; i < n && i < 8; ++i) {
        if (probe_has(probe_skin_sh, probe_skin_sn, sh[i])) {
            probe_add(probe_skin_pr, &probe_skin_pn, 96, prog);
            return;
        }
    }
}

int tspgl_probe_mode(void)
{
    return probe_mode;
}

void tspgl_probe_cycle(void)
{
}

static void probe_white_bind(void)
{
    void (*active)(uint32_t) = (void *)G.glActiveTexture;
    void (*bind)(uint32_t, uint32_t) = (void *)G.glBindTexture;
    void (*gen)(int32_t, uint32_t *) = (void *)G.glGenTextures;
    void (*image)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                  uint32_t, uint32_t, const void *) = (void *)G.glTexImage2D;
    void (*param)(uint32_t, uint32_t, int32_t) = (void *)G.glTexParameteri;
    int u;
    if (!active || !bind)
        return;
    if (!probe_white && gen && image) {
        static const uint8_t px[4] = { 255, 255, 255, 255 };
        gen(1, &probe_white);
        bind(0x0DE1, probe_white);
        if (param) {
            param(0x0DE1, 0x2801, 0x2601);
            param(0x0DE1, 0x2800, 0x2601);
        }
        image(0x0DE1, 0, 0x8058, 1, 1, 0, 0x1908, 0x1401, px);
    }
    if (!probe_white)
        return;
    for (u = 0; u < 4; ++u) {
        active(0x84C0u + (uint32_t)u);
        bind(0x0DE1, probe_white);
    }
    active(0x84C0u);
}

/* GE8300 kicks the tile job at eglSwapBuffers, not at glDrawElements.
 * Deleting a scratch buffer in the same frame hands the GPU a dead name.
 * Hold the name for three swaps, which is how long the previous kick lives. */
#define SCRATCH_RETIRE_LAG 3
#define SCRATCH_RETIRE_MAX 192
static struct {
    uint32_t id;
    int frame;
} scratch_retire_q[SCRATCH_RETIRE_MAX];
static int scratch_retire_n;
static int scratch_retire_frame;

static void scratch_retire(uint32_t id)
{
    void (*delb)(int32_t, const uint32_t *) = G.glDeleteBuffers;
    if (!id || !delb)
        return;
    if (scratch_retire_n >= SCRATCH_RETIRE_MAX) {
        delb(1, &scratch_retire_q[0].id);
        memmove(&scratch_retire_q[0], &scratch_retire_q[1],
                (size_t)(scratch_retire_n - 1) * sizeof(scratch_retire_q[0]));
        scratch_retire_n--;
    }
    scratch_retire_q[scratch_retire_n].id = id;
    scratch_retire_q[scratch_retire_n].frame = scratch_retire_frame;
    scratch_retire_n++;
}

void tspgl_scratch_retire_frame(void)
{
    void (*delb)(int32_t, const uint32_t *) = G.glDeleteBuffers;
    int i, w;
    scratch_retire_frame++;
    if (!delb)
        return;
    w = 0;
    for (i = 0; i < scratch_retire_n; ++i) {
        if (scratch_retire_frame - scratch_retire_q[i].frame >= SCRATCH_RETIRE_LAG)
            delb(1, &scratch_retire_q[i].id);
        else
            scratch_retire_q[w++] = scratch_retire_q[i];
    }
    scratch_retire_n = w;
}

/* Rogue fetches every enabled attrib, including ones the current program
 * never reads. A skin index left on from the character stretches the next
 * mesh into a black fin, and the fin changes whenever the previous draw changes. */
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_ACTIVE_ATTRIBUTES 0x8B89
static uint32_t attrib_mask_prog;
static uint16_t attrib_mask_bits;

static uint16_t program_attrib_mask(uint32_t prog)
{
    void (*getp)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetProgramiv;
    void (*geta)(uint32_t, uint32_t, int32_t, int32_t *, int32_t *, uint32_t *,
                 char *) = (void *)G.glGetActiveAttrib;
    int32_t (*loc)(uint32_t, const char *) = (void *)G.glGetAttribLocation;
    int32_t n = 0, i;
    uint16_t bits = 0;
    if (prog == attrib_mask_prog && prog)
        return attrib_mask_bits;
    if (!getp || !geta || !loc || !prog)
        return 0xffffu;
    getp(prog, GL_ACTIVE_ATTRIBUTES, &n);
    for (i = 0; i < n && i < 32; ++i) {
        char name[64];
        int32_t len = 0, size = 0;
        uint32_t type = 0;
        int32_t at;
        name[0] = 0;
        geta(prog, (uint32_t)i, (int32_t)sizeof(name), &len, &size, &type, name);
        name[sizeof(name) - 1] = 0;
        at = loc(prog, name);
        if (at >= 0 && at < 16)
            bits = (uint16_t)(bits | (uint16_t)(1u << at));
    }
    attrib_mask_prog = prog;
    attrib_mask_bits = bits;
    return bits;
}

static uint16_t hide_stale_attribs(void)
{
    void (*getiv)(uint32_t, int32_t *) = (void *)G.glGetIntegerv;
    void (*dis)(uint32_t) = (void *)G.glDisableVertexAttribArray;
    void (*a4)(uint32_t, float, float, float, float) = (void *)G.glVertexAttrib4f;
    int32_t prog = 0;
    uint16_t used, hidden = 0;
    int i;
    if (!getiv || !dis)
        return 0;
    getiv(GL_CURRENT_PROGRAM, &prog);
    if (prog <= 0)
        return 0;
    used = program_attrib_mask((uint32_t)prog);
    if (!used)
        return 0;
    for (i = 0; i < 16; ++i) {
        if (!arr_en[i] || (used & (uint16_t)(1u << i)))
            continue;
        dis((uint32_t)i);
        if (a4)
            a4((uint32_t)i, 0.f, 0.f, 0.f, 1.f);
        hidden = (uint16_t)(hidden | (uint16_t)(1u << i));
    }
    if (hidden) {
        static int once;
        if (!once) {
            fprintf(stderr, "tspgl-srv: stale attribs masked\n");
            once = 1;
        }
    }
    return hidden;
}

static void restore_stale_attribs(uint16_t hidden)
{
    void (*en)(uint32_t) = (void *)G.glEnableVertexAttribArray;
    int i;
    if (!hidden || !en)
        return;
    for (i = 0; i < 16; ++i) {
        if (hidden & (uint16_t)(1u << i))
            en((uint32_t)i);
    }
}

static int need(uint32_t n, uint32_t have)
{
    return have >= n;
}

static const void *stage_blob(const uint8_t *in, uint32_t nbytes, uint32_t off)
{
    if (nbytes <= off)
        return NULL;
    return tspgl_stage(in + off, nbytes - off);
}

/* The black wedge is not the ground-shadow update. Log each distinct draw
 * state once so the polygon can be identified without changing the picture. */
#define SV_KEEP 0x1E00
#define SV_STENCIL_TEST 0x0B90
#define SV_BLEND 0x0BE2
#define SV_CULL_FACE 0x0B44
#define SV_DEPTH_TEST 0x0B71
static uint32_t sv_fail = SV_KEEP, sv_zfail = SV_KEEP, sv_zpass = SV_KEEP;
static int sv_depth_mask = 1;
static int sv_depth_test = 1;
static int sv_stencil_on;
static int sv_blend_on;
static int sv_cull_on;
static uint32_t sv_blend_src = 0x0302, sv_blend_dst = 0x0303;
static void (*sv_real_blend)(uint32_t, uint32_t);

static void effect_blend_begin(void)
{
    /* Shots, hits and shields are depth-tested blended billboards.
     * With the default alpha blend their black texels replace the scene.
     * Additive keeps the glow and adds nothing where the texel is black. */
    if (!(sv_blend_on && sv_depth_test) || !sv_real_blend)
        return;
    sv_real_blend(0x0302, 1u);
}

static void effect_blend_end(void)
{
    if (!(sv_blend_on && sv_depth_test) || !sv_real_blend)
        return;
    sv_real_blend(sv_blend_src, sv_blend_dst);
}

static int probe_additive(void)
{
    return sv_blend_dst == 1u || sv_blend_src == 1u;
}

static int probe_skip_gles2(int32_t count)
{
    /* Black stripes are the smaller alpha-blended depth-tested meshes.
     * Additive draws in the same size (shots, arrows) stay. */
    if (!(sv_blend_on && sv_depth_test && count >= 25 && count <= 48))
        return 0;
    return !probe_additive();
}

static void (*sv_real_stencil_op)(uint32_t, uint32_t, uint32_t);
static void (*sv_real_stencil_op_sep)(uint32_t, uint32_t, uint32_t, uint32_t);
static void (*sv_real_depth_mask)(uint32_t);
static void (*sv_real_enable)(uint32_t);
static void (*sv_real_disable)(uint32_t);

static void sv_hook_stencil_op(uint32_t a, uint32_t b, uint32_t c)
{
    sv_fail = a;
    sv_zfail = b;
    sv_zpass = c;
    if (sv_real_stencil_op)
        sv_real_stencil_op(a, b, c);
}

static void sv_hook_stencil_op_sep(uint32_t face, uint32_t a, uint32_t b, uint32_t c)
{
    (void)face;
    sv_fail = a;
    sv_zfail = b;
    sv_zpass = c;
    if (sv_real_stencil_op_sep)
        sv_real_stencil_op_sep(face, a, b, c);
}

static void sv_hook_depth_mask(uint32_t on)
{
    sv_depth_mask = on ? 1 : 0;
    if (sv_real_depth_mask)
        sv_real_depth_mask(on);
}

static void sv_note_cap(uint32_t cap, int on)
{
    if (cap == SV_STENCIL_TEST)
        sv_stencil_on = on;
    else if (cap == SV_BLEND)
        sv_blend_on = on;
    else if (cap == SV_DEPTH_TEST)
        sv_depth_test = on;
    else if (cap == SV_CULL_FACE)
        sv_cull_on = on;
}

static void sv_hook_enable(uint32_t cap)
{
    sv_note_cap(cap, 1);
    if (sv_real_enable)
        sv_real_enable(cap);
}

static void sv_hook_disable(uint32_t cap)
{
    sv_note_cap(cap, 0);
    if (sv_real_disable)
        sv_real_disable(cap);
}

static void sv_hook_blend(uint32_t src, uint32_t dst)
{
    sv_blend_src = src;
    sv_blend_dst = dst;
    if (sv_real_blend)
        sv_real_blend(src, dst);
}

void tspgl_hook_volume_mask(void)
{
    sv_real_stencil_op = G.glStencilOp;
    sv_real_stencil_op_sep = G.glStencilOpSeparate;
    sv_real_depth_mask = G.glDepthMask;
    sv_real_enable = G.glEnable;
    sv_real_disable = G.glDisable;
    sv_real_blend = G.glBlendFunc;
    G.glStencilOp = sv_hook_stencil_op;
    G.glStencilOpSeparate = sv_hook_stencil_op_sep;
    G.glDepthMask = sv_hook_depth_mask;
    G.glEnable = sv_hook_enable;
    G.glDisable = sv_hook_disable;
    G.glBlendFunc = sv_hook_blend;
}

static int sh_ident(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int sh_has(const char *a, const char *b, const char *n)
{
    size_t nt = strlen(n);
    for (; a + nt <= b; a++)
        if (strncmp(a, n, nt) == 0)
            return 1;
    return 0;
}

static int sh_line_kind(const char *a, const char *b)
{
    const char *eq;
    while (a < b && (*a == ' ' || *a == '\t'))
        a++;
    if (a >= b || *a == '\n' || *a == '\r')
        return 1;
    if (a + 1 < b && a[0] == '/' && a[1] == '/')
        return 1;
    if (*a == '#') {
        if (a + 8 <= b && strncmp(a, "#version", 8) == 0)
            return 2;
        if (a + 10 <= b && strncmp(a, "#extension", 10) == 0 &&
            sh_has(a, b, "framebuffer_fetch"))
            return 3;
        return 1;
    }
    if (a + 9 <= b && strncmp(a, "precision", 9) == 0 && sh_has(a, b, "float"))
        return 4;
    if (!sh_has(a, b, "gl_LastFragData"))
        return 0;
    for (eq = a; eq < b; eq++)
        if (*eq == '=')
            return 0;
    return 5;
}

static void sh_put(char **buf, size_t *len, size_t *cap, const char *s, size_t n)
{
    if (*len + n + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : 4096;
        char *nb;
        while (nc < *len + n + 1)
            nc *= 2;
        nb = realloc(*buf, nc);
        if (!nb)
            return;
        *buf = nb;
        *cap = nc;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = 0;
}

static int sh_tok(const char *s, const char *line, const char *end,
                  const char *tok, size_t nt)
{
    if ((size_t)(end - s) < nt)
        return 0;
    if (strncmp(s, tok, nt) != 0)
        return 0;
    if (s > line && sh_ident(s[-1]))
        return 0;
    if (s + nt < end && sh_ident(s[nt]))
        return 0;
    return 1;
}

static char *rewrite_gles2_shader(const char *src, size_t slen, int *out_len)
{
    int is_frag, is_vert, use_fetch, to_es3;
    const char *p, *end;
    char *out = NULL;
    size_t len = 0, cap = 0;
    static int dump;
    const char *last_rep;
    size_t last_n;

    if (!src)
        return NULL;
    end = src + slen;
    is_frag = strstr(src, "gl_FragColor") || strstr(src, "gl_LastFragData") ||
              strstr(src, "gl_FragData");
    is_vert = strstr(src, "gl_Position") != NULL;
    use_fetch = strstr(src, "gl_LastFragData") != NULL ||
                strstr(src, "framebuffer_fetch") != NULL;
    if (strstr(src, "#version 300")) {
        *out_len = (int)slen;
        return NULL;
    }
    to_es3 = (have_fb_fetch || have_fb_fetch_nc) && (is_frag || is_vert);

    last_rep = (have_fb_fetch || have_fb_fetch_nc) ? "tspgl_fb" : "vec4(1.0)";
    last_n = strlen(last_rep);

    if (to_es3) {
        sh_put(&out, &len, &cap, "#version 300 es\n", 16);
        if (is_frag && use_fetch) {
            if (have_fb_fetch)
                sh_put(&out, &len, &cap,
                       "#extension GL_EXT_shader_framebuffer_fetch : require\n",
                       53);
            else
                sh_put(&out, &len, &cap,
                       "#extension GL_EXT_shader_framebuffer_fetch_non_coherent : require\n",
                       66);
            sh_put(&out, &len, &cap, "precision highp float;\n", 23);
            if (have_fb_fetch)
                sh_put(&out, &len, &cap,
                       "layout(location=0) inout highp vec4 tspgl_fb;\n", 45);
            else
                sh_put(&out, &len, &cap,
                       "layout(noncoherent, location=0) inout highp vec4 tspgl_fb;\n",
                       58);
        } else if (is_frag) {
            sh_put(&out, &len, &cap, "precision highp float;\n", 23);
            sh_put(&out, &len, &cap,
                   "layout(location=0) out highp vec4 tspgl_fb;\n", 43);
        } else {
            sh_put(&out, &len, &cap, "precision highp float;\n", 23);
        }
    } else if (use_fetch && !(have_fb_fetch || have_fb_fetch_nc)) {
        static int once;
        if (!once) {
            fprintf(stderr,
                    "tspgl-srv: no FB fetch on GPU, LastFragData -> vec4(1.0)\n");
            once = 1;
        }
    } else if (!use_fetch) {
        *out_len = (int)slen;
        return NULL;
    }

    p = src;
    while (p < end) {
        const char *eol = p;
        int kind;
        while (eol < end && *eol != '\n')
            eol++;
        kind = sh_line_kind(p, eol);
        if (to_es3 && kind >= 2 && kind <= 5) {
            p = (eol < end) ? eol + 1 : eol;
            continue;
        }
        if (!to_es3 && (kind == 3 || kind == 5)) {
            p = (eol < end) ? eol + 1 : eol;
            continue;
        }
        {
            const char *q = p;
            while (q < eol) {
                size_t left = (size_t)(eol - q);
                if (to_es3 && left >= 19 &&
                    sh_tok(q, p, eol, "texture2DProjLodEXT", 19)) {
                    sh_put(&out, &len, &cap, "textureProjLod", 14);
                    q += 19;
                    continue;
                }
                if (to_es3 && left >= 16 && sh_tok(q, p, eol, "texture2DProjLod", 16)) {
                    sh_put(&out, &len, &cap, "textureProjLod", 14);
                    q += 16;
                    continue;
                }
                if (to_es3 && left >= 15 && sh_tok(q, p, eol, "texture2DLodEXT", 15)) {
                    sh_put(&out, &len, &cap, "textureLod", 10);
                    q += 15;
                    continue;
                }
                if (to_es3 && left >= 12 && sh_tok(q, p, eol, "texture2DLod", 12)) {
                    sh_put(&out, &len, &cap, "textureLod", 10);
                    q += 12;
                    continue;
                }
                if (to_es3 && left >= 13 && sh_tok(q, p, eol, "texture2DProj", 13)) {
                    sh_put(&out, &len, &cap, "textureProj", 11);
                    q += 13;
                    continue;
                }
                if (to_es3 && left >= 17 && sh_tok(q, p, eol, "textureCubeLodEXT", 17)) {
                    sh_put(&out, &len, &cap, "textureLod", 10);
                    q += 17;
                    continue;
                }
                if (to_es3 && left >= 9 && sh_tok(q, p, eol, "texture2D", 9)) {
                    sh_put(&out, &len, &cap, "texture", 7);
                    q += 9;
                    continue;
                }
                if (to_es3 && left >= 11 && sh_tok(q, p, eol, "textureCube", 11)) {
                    sh_put(&out, &len, &cap, "texture", 7);
                    q += 11;
                    continue;
                }
                if (left >= 15 && sh_tok(q, p, eol, "gl_LastFragData", 15)) {
                    sh_put(&out, &len, &cap, last_rep, last_n);
                    q += 15;
                    if (q < eol && *q == '[') {
                        int depth = 1;
                        q++;
                        while (q < eol && depth) {
                            if (*q == '[')
                                depth++;
                            else if (*q == ']')
                                depth--;
                            q++;
                        }
                    }
                    continue;
                }
                if (to_es3 && is_frag && left >= 12 &&
                    sh_tok(q, p, eol, "gl_FragColor", 12)) {
                    sh_put(&out, &len, &cap, "tspgl_fb", 8);
                    q += 12;
                    continue;
                }
                if (to_es3 && is_frag && left >= 11 &&
                    sh_tok(q, p, eol, "gl_FragData", 11)) {
                    sh_put(&out, &len, &cap, "tspgl_fb", 8);
                    q += 11;
                    if (q < eol && *q == '[') {
                        int depth = 1;
                        q++;
                        while (q < eol && depth) {
                            if (*q == '[')
                                depth++;
                            else if (*q == ']')
                                depth--;
                            q++;
                        }
                    }
                    continue;
                }
                if (to_es3 && left >= 9 && sh_tok(q, p, eol, "attribute", 9)) {
                    sh_put(&out, &len, &cap, "in", 2);
                    q += 9;
                    continue;
                }
                if (to_es3 && left >= 7 && sh_tok(q, p, eol, "varying", 7)) {
                    sh_put(&out, &len, &cap, is_frag ? "in" : "out",
                           is_frag ? 2 : 3);
                    q += 7;
                    continue;
                }
                sh_put(&out, &len, &cap, q, 1);
                q++;
            }
        }
        if (eol < end) {
            sh_put(&out, &len, &cap, "\n", 1);
            p = eol + 1;
        } else
            p = eol;
    }

    /* Character rig: for(i<n) indexes[i] into g_BonePalette. Rogue does not
     * keep a loop index as a uniform-array index, so some bones read garbage
     * and those vertices stick out behind the mesh. Unroll to .xyzw. */
    if (is_vert && out && strstr(out, "g_BonePalette")) {
        char *loop = strstr(out, "for(int i=0;i<n;i++)");
        char *brace = loop ? strchr(loop, '{') : NULL;
        if (brace) {
            int depth = 0;
            char *q;
            for (q = brace; *q; ++q) {
                if (*q == '{')
                    depth++;
                else if (*q == '}' && --depth == 0)
                    break;
            }
            if (*q == '}' && depth == 0) {
                static const char body[] =
                    "if(n>0){row1+=g_BonePalette[int(indexes.x)*3]*weights.x;"
                    "row2+=g_BonePalette[int(indexes.x)*3+1]*weights.x;"
                    "row3+=g_BonePalette[int(indexes.x)*3+2]*weights.x;}"
                    "if(n>1){row1+=g_BonePalette[int(indexes.y)*3]*weights.y;"
                    "row2+=g_BonePalette[int(indexes.y)*3+1]*weights.y;"
                    "row3+=g_BonePalette[int(indexes.y)*3+2]*weights.y;}"
                    "if(n>2){row1+=g_BonePalette[int(indexes.z)*3]*weights.z;"
                    "row2+=g_BonePalette[int(indexes.z)*3+1]*weights.z;"
                    "row3+=g_BonePalette[int(indexes.z)*3+2]*weights.z;}"
                    "if(n>3){row1+=g_BonePalette[int(indexes.w)*3]*weights.w;"
                    "row2+=g_BonePalette[int(indexes.w)*3+1]*weights.w;"
                    "row3+=g_BonePalette[int(indexes.w)*3+2]*weights.w;}";
                size_t glen = sizeof(body) - 1;
                size_t old_n;
                q++;
                old_n = (size_t)(q - loop);
                {
                    char *nbuf = malloc(len - old_n + glen + 1);
                    if (nbuf) {
                        size_t at = (size_t)(loop - out);
                        memcpy(nbuf, out, at);
                        memcpy(nbuf + at, body, glen);
                        memcpy(nbuf + at + glen, q, len - (at + old_n) + 1);
                        free(out);
                        out = nbuf;
                        len = len - old_n + glen;
                        {
                            static int once;
                            if (!once) {
                                fprintf(stderr, "tspgl-srv: skin loop unrolled\n");
                                once = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    /* GE8300 fills a triangle that crosses the near plane with a black
     * wedge. The straight edge is the clip, the jagged edge is the mesh.
     * Pin those vertices to the near plane so the triangle is not clipped. */
    if (is_vert && out && strstr(out, "gl_Position")) {
        static const char guard[] =
            "if(gl_Position.w>0.0&&gl_Position.z<-gl_Position.w)"
            "gl_Position.z=-gl_Position.w;\n";
        char *main_fn = strstr(out, "void main");
        char *brace = main_fn ? strchr(main_fn, '{') : NULL;
        if (brace) {
            int depth = 0;
            char *q;
            for (q = brace; *q; ++q) {
                if (*q == '{')
                    depth++;
                else if (*q == '}' && --depth == 0)
                    break;
            }
            if (*q == '}' && depth == 0) {
                size_t at = (size_t)(q - out);
                size_t glen = sizeof(guard) - 1;
                char *nbuf = malloc(len + glen + 1);
                if (nbuf) {
                    memcpy(nbuf, out, at);
                    memcpy(nbuf + at, guard, glen);
                    memcpy(nbuf + at + glen, out + at, len - at + 1);
                    free(out);
                    out = nbuf;
                    len += glen;
                    {
                        static int once;
                        if (!once) {
                            fprintf(stderr, "tspgl-srv: near-plane clamp\n");
                            once = 1;
                        }
                    }
                }
            }
        }
    }
    if (use_fetch && dump < 3 && out) {
        fprintf(stderr,
                "tspgl-srv: shader rewrite fetch=%d es3=%d bytes=%u\n%.360s\n",
                use_fetch, to_es3, (unsigned)len, out);
        dump++;
    }
    *out_len = (int)len;
    return out;
}

static int tspgl_dispatch_special(uint32_t op, const uint8_t *in, uint32_t nbytes,
                                  uint8_t *out, uint32_t *out_n)
{
    const uint32_t *u = (const uint32_t *)in;
    *out_n = 0;

    switch (op) {
    case OP_glGetString: {
        const uint8_t *(*fn)(uint32_t) = (void *)G.glGetString;
        const uint8_t *s;
        uint32_t n;
        if (!need(4, nbytes) || !fn)
            return -1;
        s = fn(u[0]);
        if (!s)
            s = (const uint8_t *)"";
        n = (uint32_t)strlen((const char *)s) + 1u;
        if (n > 4095)
            n = 4095;
        memcpy(out, s, n);
        out[n] = 0;
        *out_n = n + 1;
        return 0;
    }
    case OP_glGetIntegerv: {
        void (*fn)(uint32_t, int32_t *) = (void *)G.glGetIntegerv;
        if (!need(4, nbytes) || !fn)
            return -1;
        memset(out, 0, 64);
        fn(u[0], (int32_t *)out);
        *out_n = 64;
        return 0;
    }
    case OP_glGetFloatv: {
        void (*fn)(uint32_t, float *) = (void *)G.glGetFloatv;
        if (!need(4, nbytes) || !fn)
            return -1;
        memset(out, 0, 64);
        fn(u[0], (float *)out);
        *out_n = 64;
        return 0;
    }
    case OP_glGetBooleanv: {
        void (*fn)(uint32_t, uint8_t *) = (void *)G.glGetBooleanv;
        if (!need(4, nbytes) || !fn)
            return -1;
        memset(out, 0, 16);
        fn(u[0], out);
        *out_n = 16;
        return 0;
    }
    case OP_glGetShaderiv: {
        void (*fn)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetShaderiv;
        int32_t v = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], &v);
        memcpy(out, &v, 4);
        *out_n = 4;
        return 0;
    }
    case OP_glGetProgramiv: {
        void (*fn)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetProgramiv;
        int32_t v = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], &v);
        memcpy(out, &v, 4);
        *out_n = 4;
        return 0;
    }
    case OP_glGetShaderInfoLog:
    case OP_glGetProgramInfoLog: {
        void (*fn)(uint32_t, int32_t, int32_t *, char *) =
            op == OP_glGetShaderInfoLog ? (void *)G.glGetShaderInfoLog
                                        : (void *)G.glGetProgramInfoLog;
        int32_t cap, len = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        cap = (int32_t)u[1];
        if (cap < 1)
            cap = 1;
        if ((uint32_t)cap + 4u > TSPGL_MAX_BLOB)
            cap = (int32_t)(TSPGL_MAX_BLOB - 4);
        fn(u[0], cap, &len, (char *)(out + 4));
        if (len < 0)
            len = 0;
        memcpy(out, &len, 4);
        *out_n = 4u + (uint32_t)len;
        return 0;
    }
    case OP_glShaderSource: {
        void (*fn)(uint32_t, int32_t, const char *const *, const int32_t *) =
            (void *)G.glShaderSource;
        uint32_t shader, count, i;
        const uint8_t *p;
        size_t total = 0;
        char *src;
        const char *one;
        int32_t one_len;
        static const char prefix[] = "#version 100\n";
        int have_ver = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        shader = u[0];
        count = u[1];
        if (count > 64)
            count = 64;
        p = in + 8;
        for (i = 0; i < count; ++i) {
            int32_t len;
            if ((size_t)(p - in) + 4 > nbytes)
                break;
            memcpy(&len, p, 4);
            p += 4;
            if (len < 0)
                len = 0;
            if ((size_t)(p - in) + (size_t)len > nbytes)
                len = (int32_t)(nbytes - (uint32_t)(p - in));
            total += (size_t)len;
            p += (uint32_t)len;
        }
        src = malloc(total + sizeof(prefix) + 4);
        if (!src)
            return -1;
        p = in + 8;
        total = 0;
        for (i = 0; i < count; ++i) {
            int32_t len;
            if ((size_t)(p - in) + 4 > nbytes)
                break;
            memcpy(&len, p, 4);
            p += 4;
            if (len < 0)
                len = 0;
            if ((size_t)(p - in) + (size_t)len > nbytes)
                len = (int32_t)(nbytes - (uint32_t)(p - in));
            memcpy(src + total, p, (size_t)len);
            total += (size_t)len;
            p += (uint32_t)len;
        }
        src[total] = 0;
        probe_note_shader(shader, src);
        {
            const char *q = src;
            while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n')
                q++;
            if (q[0] == '#' && strncmp(q, "#version", 8) == 0)
                have_ver = 1;
        }
        if (!have_ver) {
            memmove(src + sizeof(prefix) - 1, src, total + 1);
            memcpy(src, prefix, sizeof(prefix) - 1);
            total += sizeof(prefix) - 1;
        }
        {
            int nlen = 0;
            char *rw = rewrite_gles2_shader(src, total, &nlen);
            if (rw) {
                free(src);
                src = rw;
                total = (size_t)nlen;
            }
        }
        one = src;
        one_len = (int32_t)total;
        fn(shader, 1, &one, &one_len);
        free(src);
        return 0;
    }
    case OP_glBindAttribLocation: {
        void (*fn)(uint32_t, uint32_t, const char *) = (void *)G.glBindAttribLocation;
        if (!need(9, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], (const char *)(in + 8));
        return 0;
    }
    case OP_glGetAttribLocation:
    case OP_glGetUniformLocation: {
        int32_t (*fn)(uint32_t, const char *) =
            op == OP_glGetAttribLocation ? (void *)G.glGetAttribLocation
                                         : (void *)G.glGetUniformLocation;
        int32_t loc;
        if (!need(5, nbytes) || !fn)
            return -1;
        loc = fn(u[0], (const char *)(in + 4));
        memcpy(out, &loc, 4);
        *out_n = 4;
        return 0;
    }
    case OP_glBufferData: {
        void (*fn)(uint32_t, intptr_t, const void *, uint32_t) =
            (void *)G.glBufferData;
        const void *data;
        int32_t size;
        if (!need(12, nbytes) || !fn)
            return -1;
        size = (int32_t)u[1];
        data = (nbytes > 12) ? stage_blob(in, nbytes, 12) : NULL;
        fn(u[0], (intptr_t)size, data, u[2]);
        return 0;
    }
    case OP_glBufferSubData: {
        void (*fn)(uint32_t, intptr_t, intptr_t, const void *) =
            (void *)G.glBufferSubData;
        int32_t size;
        if (!need(12, nbytes) || !fn)
            return -1;
        size = (int32_t)u[2];
        fn(u[0], (intptr_t)(int32_t)u[1], (intptr_t)size,
           nbytes > 12 ? stage_blob(in, nbytes, 12) : NULL);
        return 0;
    }
    case OP_glTexImage2D: {
        void (*fn)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                   uint32_t, uint32_t, const void *) = (void *)G.glTexImage2D;
        if (!need(32, nbytes) || !fn)
            return -1;
        fn(u[0], (int32_t)u[1], (int32_t)u[2], (int32_t)u[3], (int32_t)u[4],
           (int32_t)u[5], u[6], u[7], stage_blob(in, nbytes, 32));
        return 0;
    }
    case OP_glTexSubImage2D: {
        void (*fn)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                   uint32_t, uint32_t, const void *) = (void *)G.glTexSubImage2D;
        if (!need(32, nbytes) || !fn)
            return -1;
        fn(u[0], (int32_t)u[1], (int32_t)u[2], (int32_t)u[3], (int32_t)u[4],
           (int32_t)u[5], u[6], u[7], stage_blob(in, nbytes, 32));
        return 0;
    }
    case OP_glCompressedTexImage2D: {
        void (*fn)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t,
                   int32_t, const void *) = (void *)G.glCompressedTexImage2D;
        if (!need(28, nbytes) || !fn)
            return -1;
        fn(u[0], (int32_t)u[1], u[2], (int32_t)u[3], (int32_t)u[4],
           (int32_t)u[5], (int32_t)u[6], stage_blob(in, nbytes, 28));
        return 0;
    }
    case OP_glCompressedTexSubImage2D: {
        void (*fn)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
                   uint32_t, int32_t, const void *) =
            (void *)G.glCompressedTexSubImage2D;
        if (!need(32, nbytes) || !fn)
            return -1;
        fn(u[0], (int32_t)u[1], (int32_t)u[2], (int32_t)u[3], (int32_t)u[4],
           (int32_t)u[5], u[6], (int32_t)u[7], stage_blob(in, nbytes, 32));
        return 0;
    }
    case OP_glReadPixels: {
        void (*fn)(int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t,
                   void *) = (void *)G.glReadPixels;
        int32_t w, h;
        uint32_t nb;
        if (!need(24, nbytes) || !fn)
            return -1;
        w = (int32_t)u[2];
        h = (int32_t)u[3];
        if (w < 0)
            w = 0;
        if (h < 0)
            h = 0;
        nb = (uint32_t)w * (uint32_t)h * 4u;
        if (nb > TSPGL_MAX_BLOB)
            nb = TSPGL_MAX_BLOB;
        fn((int32_t)u[0], (int32_t)u[1], w, h, u[4], u[5], out);
        *out_n = nb;
        return 0;
    }
    case OP_glVertexAttribPointer: {
        void (*fn)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void *) =
            (void *)G.glVertexAttribPointer;
        uint32_t index;
        if (!need(24, nbytes) || !fn)
            return -1;
        index = u[0];
        if (index >= 16)
            return 0;
        sattr[index].size = (int32_t)u[1];
        sattr[index].type = u[2];
        sattr[index].normalized = u[3];
        sattr[index].stride = (int32_t)u[4];
        sattr[index].is_offset = (u[5] != 0xffffffffu);
        if (sattr[index].is_offset)
            fn(index, sattr[index].size, sattr[index].type,
               (uint8_t)sattr[index].normalized, sattr[index].stride,
               (const void *)(uintptr_t)u[5]);
        return 0;
    }
    case OP_glUploadAttrib: {
        void (*bind)(uint32_t, uint32_t) = (void *)G.glBindBuffer;
        void (*data)(uint32_t, intptr_t, const void *, uint32_t) =
            (void *)G.glBufferData;
        void (*vap)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void *) =
            (void *)G.glVertexAttribPointer;
        void (*gen)(int32_t, uint32_t *) = G.glGenBuffers;
        uint32_t index, nb;
        if (!need(16, nbytes) || !bind || !data || !vap || !gen)
            return -1;
        index = u[0];
        nb = u[3];
        if (index >= 16)
            return 0;
        if (nb > nbytes - 16)
            nb = nbytes - 16;
        {
            /* Previous mesh stays live until the tile kick, three swaps later. */
            uint32_t old = scratch_vbo[index];
            uint32_t neu = 0;
            gen(1, &neu);
            if (!neu)
                return 0;
            scratch_vbo[index] = neu;
            bind(GL_ARRAY_BUFFER, neu);
            data(GL_ARRAY_BUFFER, (intptr_t)nb, stage_blob(in, nbytes, 16),
                 GL_STREAM_DRAW);
            vap(index, sattr[index].size ? sattr[index].size : 4,
                sattr[index].type, (uint8_t)sattr[index].normalized,
                (int32_t)u[1], (const void *)0);
            if (old)
                scratch_retire(old);
        }
        bind(GL_ARRAY_BUFFER, srv_bound_array);
        return 0;
    }
    case OP_glDrawArrays: {
        void (*fn)(uint32_t, int32_t, int32_t) = (void *)G.glDrawArrays;
        if (!need(12, nbytes) || !fn)
            return -1;
        if (tspgl_es1_skip_draw())
            return 0;
        if (tspgl_es1_gles2_active()) {
            uint16_t hidden;
            tspgl_es1_bind_client_program();
            if (probe_skip_gles2((int32_t)u[2]))
                return 0;
            hidden = hide_stale_attribs();
            fn(u[0], (int32_t)u[1], (int32_t)u[2]);
            restore_stale_attribs(hidden);
            return 0;
        }
        tspgl_es1_prepare_draw();
        fn(u[0], (int32_t)u[1], (int32_t)u[2]);
        tspgl_es1_finish_draw();
        return 0;
    }
    case OP_glDrawElements: {
        void (*fn)(uint32_t, int32_t, uint32_t, const void *) =
            (void *)G.glDrawElements;
        void (*bind)(uint32_t, uint32_t) = (void *)G.glBindBuffer;
        void (*data)(uint32_t, intptr_t, const void *, uint32_t) =
            (void *)G.glBufferData;
        void (*gen)(int32_t, uint32_t *) = G.glGenBuffers;
        uint32_t nb;
        if (!need(16, nbytes) || !fn)
            return -1;
        if (u[3] == 0xffffffffu) {
            nb = nbytes - 16;
            if (gen)
                gen(1, &scratch_ibo);
            if (bind && data && scratch_ibo) {
                if (tspgl_es1_skip_draw())
                    return 0;
                if (tspgl_es1_gles2_active()) {
                    uint16_t hidden;
                    uint32_t ibo = scratch_ibo;
                    tspgl_es1_bind_client_program();
                    if (probe_skip_gles2((int32_t)u[1])) {
                        scratch_retire(ibo);
                        scratch_ibo = 0;
                        return 0;
                    }
                    bind(GL_ELEMENT_ARRAY_BUFFER, ibo);
                    data(GL_ELEMENT_ARRAY_BUFFER, (intptr_t)nb,
                         stage_blob(in, nbytes, 16), GL_STREAM_DRAW);
                    hidden = hide_stale_attribs();
                    fn(u[0], (int32_t)u[1], u[2], (const void *)0);
                    restore_stale_attribs(hidden);
                    bind(GL_ELEMENT_ARRAY_BUFFER, srv_bound_element);
                    scratch_retire(ibo);
                    scratch_ibo = 0;
                    return 0;
                }
                tspgl_es1_prepare_draw();
                bind(GL_ELEMENT_ARRAY_BUFFER, scratch_ibo);
                data(GL_ELEMENT_ARRAY_BUFFER, (intptr_t)nb,
                     stage_blob(in, nbytes, 16), GL_STREAM_DRAW);
                fn(u[0], (int32_t)u[1], u[2], (const void *)0);
                tspgl_es1_finish_draw();
                bind(GL_ELEMENT_ARRAY_BUFFER, srv_bound_element);
                scratch_retire(scratch_ibo);
                scratch_ibo = 0;
                return 0;
            }
        }
        if (tspgl_es1_skip_draw())
            return 0;
        if (tspgl_es1_gles2_active()) {
            uint16_t hidden;
            tspgl_es1_bind_client_program();
            if (probe_skip_gles2((int32_t)u[1]))
                return 0;
            hidden = hide_stale_attribs();
            fn(u[0], (int32_t)u[1], u[2], (const void *)(uintptr_t)u[3]);
            restore_stale_attribs(hidden);
            return 0;
        }
        tspgl_es1_prepare_draw();
        fn(u[0], (int32_t)u[1], u[2], (const void *)(uintptr_t)u[3]);
        tspgl_es1_finish_draw();
        return 0;
    }
    case OP_glUniform1fv:
    case OP_glUniform2fv:
    case OP_glUniform3fv:
    case OP_glUniform4fv: {
        void (*fn)(int32_t, int32_t, const float *) =
            op == OP_glUniform1fv   ? (void *)G.glUniform1fv
            : op == OP_glUniform2fv ? (void *)G.glUniform2fv
            : op == OP_glUniform3fv ? (void *)G.glUniform3fv
                                    : (void *)G.glUniform4fv;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn((int32_t)u[0], (int32_t)u[1], (const float *)tspgl_stage(in + 8, nbytes - 8));
        return 0;
    }
    case OP_glUniform1iv:
    case OP_glUniform2iv:
    case OP_glUniform3iv:
    case OP_glUniform4iv: {
        void (*fn)(int32_t, int32_t, const int32_t *) =
            op == OP_glUniform1iv   ? (void *)G.glUniform1iv
            : op == OP_glUniform2iv ? (void *)G.glUniform2iv
            : op == OP_glUniform3iv ? (void *)G.glUniform3iv
                                    : (void *)G.glUniform4iv;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn((int32_t)u[0], (int32_t)u[1], (const int32_t *)tspgl_stage(in + 8, nbytes - 8));
        return 0;
    }
    case OP_glUniformMatrix2fv:
    case OP_glUniformMatrix3fv:
    case OP_glUniformMatrix4fv: {
        void (*fn)(int32_t, int32_t, uint8_t, const float *) =
            op == OP_glUniformMatrix2fv   ? (void *)G.glUniformMatrix2fv
            : op == OP_glUniformMatrix3fv ? (void *)G.glUniformMatrix3fv
                                          : (void *)G.glUniformMatrix4fv;
        if (!need(12, nbytes) || !fn)
            return -1;
        fn((int32_t)u[0], (int32_t)u[1], (uint8_t)u[2],
           (const float *)tspgl_stage(in + 12, nbytes - 12));
        return 0;
    }
    case OP_glGetBufferParameteriv: {
        void (*fn)(uint32_t, uint32_t, int32_t *) =
            (void *)G.glGetBufferParameteriv;
        int32_t v = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], &v);
        memcpy(out, &v, 4);
        *out_n = 4;
        return 0;
    }
    case OP_glBindBuffer: {
        void (*fn)(uint32_t, uint32_t) = (void *)G.glBindBuffer;
        if (!need(8, nbytes) || !fn)
            return -1;
        if (u[0] == GL_ARRAY_BUFFER)
            srv_bound_array = u[1];
        if (u[0] == GL_ELEMENT_ARRAY_BUFFER)
            srv_bound_element = u[1];
        fn(u[0], u[1]);
        return 0;
    }
    case OP_glEnableVertexAttribArray: {
        void (*fn)(uint32_t) = (void *)G.glEnableVertexAttribArray;
        if (!need(4, nbytes) || !fn)
            return -1;
        if (u[0] < 16)
            arr_en[u[0]] = 1;
        fn(u[0]);
        return 0;
    }
    case OP_glDisableVertexAttribArray: {
        void (*fn)(uint32_t) = (void *)G.glDisableVertexAttribArray;
        if (!need(4, nbytes) || !fn)
            return -1;
        if (u[0] < 16)
            arr_en[u[0]] = 0;
        fn(u[0]);
        return 0;
    }
    case OP_glPixelStorei: {
        void (*fn)(uint32_t, int32_t) = (void *)G.glPixelStorei;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], (int32_t)u[1]);
        return 0;
    }
    case OP_glTexParameterfv: {
        void (*fn)(uint32_t, uint32_t, const float *) = (void *)G.glTexParameterfv;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], (const float *)(in + 8));
        return 0;
    }
    case OP_glTexParameteriv: {
        void (*fn)(uint32_t, uint32_t, const int32_t *) =
            (void *)G.glTexParameteriv;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], (const int32_t *)(in + 8));
        return 0;
    }
    case OP_glVertexAttrib1fv:
    case OP_glVertexAttrib2fv:
    case OP_glVertexAttrib3fv:
    case OP_glVertexAttrib4fv: {
        void (*fn)(uint32_t, const float *) =
            op == OP_glVertexAttrib1fv   ? (void *)G.glVertexAttrib1fv
            : op == OP_glVertexAttrib2fv ? (void *)G.glVertexAttrib2fv
            : op == OP_glVertexAttrib3fv ? (void *)G.glVertexAttrib3fv
                                         : (void *)G.glVertexAttrib4fv;
        if (!need(4, nbytes) || !fn)
            return -1;
        fn(u[0], (const float *)(in + 4));
        return 0;
    }
    case OP_glGetActiveAttrib:
    case OP_glGetActiveUniform: {
        void (*fn)(uint32_t, uint32_t, int32_t, int32_t *, int32_t *, uint32_t *,
                   char *) = op == OP_glGetActiveAttrib
                                 ? (void *)G.glGetActiveAttrib
                                 : (void *)G.glGetActiveUniform;
        int32_t bufSize, length = 0, size = 0;
        uint32_t type = 0;
        char *name;
        if (!need(12, nbytes) || !fn)
            return -1;
        bufSize = (int32_t)u[2];
        if (bufSize < 1)
            bufSize = 1;
        if ((uint32_t)bufSize + 12u > TSPGL_MAX_BLOB)
            bufSize = (int32_t)(TSPGL_MAX_BLOB - 12);
        name = (char *)(out + 12);
        memset(name, 0, (uint32_t)bufSize);
        fn(u[0], u[1], bufSize, &length, &size, &type, name);
        memcpy(out, &length, 4);
        memcpy(out + 4, &size, 4);
        memcpy(out + 8, &type, 4);
        *out_n = 12u + (uint32_t)bufSize;
        return 0;
    }
    case OP_glGetAttachedShaders: {
        void (*fn)(uint32_t, int32_t, int32_t *, uint32_t *) =
            (void *)G.glGetAttachedShaders;
        int32_t maxc, count = 0;
        uint32_t shaders[64];
        if (!need(8, nbytes) || !fn)
            return -1;
        maxc = (int32_t)u[1];
        if (maxc > 64)
            maxc = 64;
        if (maxc < 0)
            maxc = 0;
        fn(u[0], maxc, &count, shaders);
        memcpy(out, &count, 4);
        if (count > 0)
            memcpy(out + 4, shaders, (uint32_t)count * 4u);
        *out_n = 4u + (uint32_t)count * 4u;
        return 0;
    }
    case OP_glGetFramebufferAttachmentParameteriv: {
        void (*fn)(uint32_t, uint32_t, uint32_t, int32_t *) =
            (void *)G.glGetFramebufferAttachmentParameteriv;
        int32_t v = 0;
        if (!need(12, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], u[2], &v);
        memcpy(out, &v, 4);
        *out_n = 4;
        return 0;
    }
    case OP_glGetRenderbufferParameteriv: {
        void (*fn)(uint32_t, uint32_t, int32_t *) =
            (void *)G.glGetRenderbufferParameteriv;
        int32_t v = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], &v);
        memcpy(out, &v, 4);
        *out_n = 4;
        return 0;
    }
    case OP_glGetShaderPrecisionFormat: {
        void (*fn)(uint32_t, uint32_t, int32_t *, int32_t *) =
            (void *)G.glGetShaderPrecisionFormat;
        int32_t range[2] = { 0, 0 };
        int32_t prec = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        fn(u[0], u[1], range, &prec);
        memcpy(out, range, 8);
        memcpy(out + 8, &prec, 4);
        *out_n = 12;
        return 0;
    }
    case OP_glGetShaderSource: {
        void (*fn)(uint32_t, int32_t, int32_t *, char *) =
            (void *)G.glGetShaderSource;
        int32_t cap, len = 0;
        if (!need(8, nbytes) || !fn)
            return -1;
        cap = (int32_t)u[1];
        if (cap < 1)
            cap = 1;
        if ((uint32_t)cap + 4u > TSPGL_MAX_BLOB)
            cap = (int32_t)(TSPGL_MAX_BLOB - 4);
        fn(u[0], cap, &len, (char *)(out + 4));
        memcpy(out, &len, 4);
        *out_n = 4u + (uint32_t)(len > 0 ? len : 0);
        return 0;
    }
    case OP_glGetTexParameterfv: {
        void (*fn)(uint32_t, uint32_t, float *) = (void *)G.glGetTexParameterfv;
        if (!need(8, nbytes) || !fn)
            return -1;
        memset(out, 0, 16);
        fn(u[0], u[1], (float *)out);
        *out_n = 16;
        return 0;
    }
    case OP_glGetTexParameteriv: {
        void (*fn)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetTexParameteriv;
        if (!need(8, nbytes) || !fn)
            return -1;
        memset(out, 0, 16);
        fn(u[0], u[1], (int32_t *)out);
        *out_n = 16;
        return 0;
    }
    case OP_glGetUniformfv: {
        void (*fn)(uint32_t, int32_t, float *) = (void *)G.glGetUniformfv;
        if (!need(8, nbytes) || !fn)
            return -1;
        memset(out, 0, 64);
        fn(u[0], (int32_t)u[1], (float *)out);
        *out_n = 64;
        return 0;
    }
    case OP_glGetUniformiv: {
        void (*fn)(uint32_t, int32_t, int32_t *) = (void *)G.glGetUniformiv;
        if (!need(8, nbytes) || !fn)
            return -1;
        memset(out, 0, 64);
        fn(u[0], (int32_t)u[1], (int32_t *)out);
        *out_n = 64;
        return 0;
    }
    case OP_glGetVertexAttribfv: {
        void (*fn)(uint32_t, uint32_t, float *) = (void *)G.glGetVertexAttribfv;
        if (!need(8, nbytes) || !fn)
            return -1;
        memset(out, 0, 16);
        fn(u[0], u[1], (float *)out);
        *out_n = 16;
        return 0;
    }
    case OP_glGetVertexAttribiv: {
        void (*fn)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetVertexAttribiv;
        if (!need(8, nbytes) || !fn)
            return -1;
        memset(out, 0, 16);
        fn(u[0], u[1], (int32_t *)out);
        *out_n = 16;
        return 0;
    }
    case OP_glBindVertexArray: {
        void (*fn)(uint32_t) = (void *)G.glBindVertexArray;
        if (!need(4, nbytes) || !fn)
            return -1;
        fn(u[0]);
        return 0;
    }
    case OP_glIsVertexArray: {
        uint32_t (*fn)(uint32_t) = (void *)G.glIsVertexArray;
        uint32_t v = 0;
        if (!need(4, nbytes))
            return -1;
        if (fn)
            v = fn(u[0]);
        memcpy(out, &v, 4);
        *out_n = 4;
        return 0;
    }
    default:
        return -1;
    }
}
