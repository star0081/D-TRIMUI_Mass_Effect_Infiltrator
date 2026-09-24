/* GLES 1.1 on GLES 3.0. Included from server.c after G is defined. */
#include "gles1_math.h"

#define ES1_MV 0
#define ES1_PR 1
#define ES1_TX 2
#define ES1_STACK 16
#define GL_LIGHT0 0x4000
#define GL_TEXTURE_2D 0x0DE1
#define GL_LIGHTING 0x0B50
#define GL_COLOR_MATERIAL 0x0B57
#define GL_FOG 0x0B60
#define GL_ALPHA_TEST 0x0BC0
#define GL_MATRIX_PALETTE_OES 0x8840
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
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_ENV_COLOR 0x2201
#define GL_MODULATE 0x2100
#define GL_REPLACE 0x1E01
#define GL_DECAL 0x2101
#define GL_BLEND 0x0BE2
#define GL_ADD 0x0104
#define GL_ONE 1
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_FOG_MODE 0x0B65
#define GL_FOG_DENSITY 0x0B62
#define GL_FOG_START 0x0B63
#define GL_FOG_END 0x0B64
#define GL_FOG_COLOR 0x0B66
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_FUNC 0x0B74
#define GL_LESS 0x0201
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS 0x2A00

struct es1_srv_light {
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float position[4];
    float spot_dir[3];
    float spot_exp;
    float spot_cut;
    float att[3];
};

struct es1_srv {
    int ready;
    uint32_t prog;
    int loc_mvp, loc_mv, loc_nmat, loc_palette, loc_pal_on;
    int loc_lighting, loc_tex_on, loc_tex_on1, loc_texenv, loc_texenv1, loc_texenv_c, loc_texmat, loc_texmat1;
    int loc_color, loc_normal, loc_color_arr, loc_norm_arr, loc_uvsz, loc_uvsz1;
    int loc_add_no_color, loc_env_mod;
    int loc_samp, loc_samp1;
    int loc_mat_a, loc_mat_d, loc_mat_s, loc_mat_e, loc_shine, loc_scene;
    int loc_l_on, loc_l_a, loc_l_d, loc_l_s, loc_l_p, loc_l_att;
    int loc_alpha_on, loc_alpha_fn, loc_alpha_ref;
    int loc_fog_on, loc_fog_mode, loc_fog_c, loc_fog_s, loc_fog_e, loc_fog_d;
    int loc_color_mat;
    int mode, pal_i, depth[3];
    float stack[3][ES1_STACK][16];
    float palette[16][16];
    float color[4];
    float normal[3];
    float texcoord[4];
    float mat_amb[4], mat_diff[4], mat_spec[4], mat_emis[4];
    float shininess;
    float scene_amb[4];
    struct es1_srv_light light[8];
    int cap_light[8];
    int cap_lighting, cap_tex2d[2], cap_color_mat, cap_fog, cap_alpha, cap_palette;
    uint32_t texenv_mode[2];
    float texenv_color[4];
    uint32_t alpha_func;
    float alpha_ref;
    uint32_t fog_mode;
    float fog_density, fog_start, fog_end, fog_color[4];
    int env_po_on, saved_dmask, saved_po_en, env_blend_on, saved_blend_on;
    int32_t saved_blend_src, saved_blend_dst;
    float saved_po_f, saved_po_u;
    int tex_unit, tx_depth[2];
    float tx_stack[2][ES1_STACK][16];
    uint32_t tex_bound[2];
};

static struct es1_srv E;

static float es1_f32(const uint8_t *p)
{
    float f;
    memcpy(&f, p, 4);
    return f;
}

static void es1_light_def(struct es1_srv_light *L, int i)
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

static uint32_t es1_compile(uint32_t type, const char *src)
{
    uint32_t sh;
    int32_t ok = 0;
    char log[512];
    int32_t n = 0;
    void (*srcfn)(uint32_t, int32_t, const char *const *, const int32_t *) =
        (void *)G.glShaderSource;
    void (*getiv)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetShaderiv;
    void (*getlog)(uint32_t, int32_t, int32_t *, char *) = (void *)G.glGetShaderInfoLog;
    sh = G.glCreateShader(type);
    if (!sh || !srcfn)
        return 0;
    srcfn(sh, 1, &src, NULL);
    G.glCompileShader(sh);
    if (getiv)
        getiv(sh, 0x8B81, &ok);
    if (!ok) {
        if (getlog)
            getlog(sh, (int32_t)sizeof(log) - 1, &n, log);
        log[n < 0 ? 0 : (n >= (int)sizeof(log) ? (int)sizeof(log) - 1 : n)] = 0;
        fprintf(stderr, "tspgl-es1: shader compile failed:\n%s\n", log);
        return 0;
    }
    return sh;
}

static int es1_loc(const char *n)
{
    int32_t (*fn)(uint32_t, const char *) = (void *)G.glGetUniformLocation;
    if (!fn)
        return -1;
    return fn(E.prog, n);
}

static const char *ES1_VS =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "layout(location=0) in vec4 a_pos;\n"
    "layout(location=1) in vec4 a_color;\n"
    "layout(location=2) in vec3 a_normal;\n"
    "layout(location=3) in vec4 a_uv;\n"
    "layout(location=4) in vec4 a_weight;\n"
    "layout(location=5) in vec4 a_midx;\n"
    "layout(location=6) in vec4 a_uv1;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_mv;\n"
    "uniform mat3 u_nmat;\n"
    "uniform mat4 u_texmat;\n"
    "uniform mat4 u_texmat1;\n"
    "uniform mat4 u_palette[16];\n"
    "uniform int u_palette_on;\n"
    "uniform int u_color_arr;\n"
    "uniform int u_norm_arr;\n"
    "uniform int u_uvsz;\n"
    "uniform int u_uvsz1;\n"
    "uniform vec4 u_color;\n"
    "uniform vec3 u_normal;\n"
    "out vec4 v_color;\n"
    "out vec2 v_uv;\n"
    "out vec2 v_uv1;\n"
    "out vec3 v_eye;\n"
    "out vec3 v_nrm;\n"
    "void main(){\n"
    "  vec4 pos = a_pos;\n"
    "  vec3 nrm = u_norm_arr != 0 ? a_normal : u_normal;\n"
    "  if (u_palette_on != 0) {\n"
    "    int i0 = int(clamp(a_midx.x + 0.5, 0.0, 15.0));\n"
    "    int i1 = int(clamp(a_midx.y + 0.5, 0.0, 15.0));\n"
    "    int i2 = int(clamp(a_midx.z + 0.5, 0.0, 15.0));\n"
    "    int i3 = int(clamp(a_midx.w + 0.5, 0.0, 15.0));\n"
    "    mat4 skin = a_weight.x * u_palette[i0] + a_weight.y * u_palette[i1]\n"
    "              + a_weight.z * u_palette[i2] + a_weight.w * u_palette[i3];\n"
    "    pos = skin * pos;\n"
    "    nrm = mat3(skin) * nrm;\n"
    "  }\n"
    "  gl_Position = u_mvp * pos;\n"
    "  vec4 e = u_mv * pos;\n"
    "  v_eye = e.xyz;\n"
    "  v_nrm = u_nmat * nrm;\n"
    "  v_color = u_color_arr != 0 ? a_color : u_color;\n"
    "  if (u_uvsz >= 3) {\n"
    "    vec3 n = a_uv.xyz;\n"
    "    float ls = dot(n, n);\n"
    "    if (ls > 1e-8) n *= inversesqrt(ls);\n"
    "    vec4 tc = u_texmat * vec4(n, 0.0);\n"
    "    v_uv = tc.xy + u_texmat[3].xy;\n"
    "  } else {\n"
    "    vec4 tc = u_texmat * vec4(a_uv.xy, 0.0, 1.0);\n"
    "    v_uv = abs(tc.w) > 1e-6 ? tc.xy / tc.w : tc.xy;\n"
    "  }\n"
    "  if (u_uvsz1 >= 3) {\n"
    "    vec3 n1 = a_uv1.xyz;\n"
    "    float ls1 = dot(n1, n1);\n"
    "    if (ls1 > 1e-8) n1 *= inversesqrt(ls1);\n"
    "    vec4 tc1 = u_texmat1 * vec4(n1, 0.0);\n"
    "    v_uv1 = tc1.xy + u_texmat1[3].xy;\n"
    "  } else {\n"
    "    vec4 tc1 = u_texmat1 * vec4(a_uv1.xy, 0.0, 1.0);\n"
    "    v_uv1 = abs(tc1.w) > 1e-6 ? tc1.xy / tc1.w : tc1.xy;\n"
    "  }\n"
    "}\n";

static const char *ES1_FS =
    "#version 300 es\n"
    "precision mediump float;\n"
    "precision highp int;\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "in vec2 v_uv1;\n"
    "in vec3 v_eye;\n"
    "in vec3 v_nrm;\n"
    "uniform int u_lighting;\n"
    "uniform int u_tex_on;\n"
    "uniform int u_tex_on1;\n"
    "uniform int u_texenv;\n"
    "uniform int u_texenv1;\n"
    "uniform vec4 u_texenv_c;\n"
    "uniform int u_add_no_color;\n"
    "uniform int u_env_mod;\n"
    "uniform int u_color_mat;\n"
    "uniform vec4 u_mat_a;\n"
    "uniform vec4 u_mat_d;\n"
    "uniform vec4 u_mat_s;\n"
    "uniform vec4 u_mat_e;\n"
    "uniform float u_shine;\n"
    "uniform vec4 u_scene;\n"
    "uniform int u_l_on[8];\n"
    "uniform vec4 u_l_a[8];\n"
    "uniform vec4 u_l_d[8];\n"
    "uniform vec4 u_l_s[8];\n"
    "uniform vec4 u_l_p[8];\n"
    "uniform vec3 u_l_att[8];\n"
    "uniform int u_alpha_on;\n"
    "uniform int u_alpha_fn;\n"
    "uniform float u_alpha_ref;\n"
    "uniform int u_fog_on;\n"
    "uniform int u_fog_mode;\n"
    "uniform vec4 u_fog_c;\n"
    "uniform float u_fog_s;\n"
    "uniform float u_fog_e;\n"
    "uniform float u_fog_d;\n"
    "uniform sampler2D u_samp;\n"
    "uniform sampler2D u_samp1;\n"
    "layout(location=0) out highp vec4 o_col;\n"
    "bool alpha_pass(float a){\n"
    "  if (u_alpha_on == 0) return true;\n"
    "  int fn = u_alpha_fn;\n"
    "  if (fn == 0x0200) return false;\n"
    "  if (fn == 0x0201) return a < u_alpha_ref;\n"
    "  if (fn == 0x0202) return abs(a - u_alpha_ref) < 0.002;\n"
    "  if (fn == 0x0203) return a <= u_alpha_ref;\n"
    "  if (fn == 0x0204) return a > u_alpha_ref;\n"
    "  if (fn == 0x0205) return abs(a - u_alpha_ref) >= 0.002;\n"
    "  if (fn == 0x0206) return a >= u_alpha_ref;\n"
    "  return true;\n"
    "}\n"
    "void main(){\n"
    "  vec4 col = v_color;\n"
    "  vec4 matd = u_mat_d;\n"
    "  vec4 mata = u_mat_a;\n"
    "  if (u_color_mat != 0) { matd = col; mata = col; }\n"
    "  vec3 N = v_nrm;\n"
    "  float nls = dot(N, N);\n"
    "  N = nls > 1e-8 ? N * inversesqrt(nls) : vec3(0.0, 0.0, 1.0);\n"
    "  if (u_lighting != 0) {\n"
    "    vec4 lit = u_mat_e + mata * u_scene;\n"
    "    vec3 V = normalize(-v_eye);\n"
    "    int i;\n"
    "    for (i = 0; i < 8; ++i) {\n"
    "      if (u_l_on[i] == 0) continue;\n"
    "      vec4 lp = u_l_p[i];\n"
    "      vec3 L; float att = 1.0;\n"
    "      if (abs(lp.w) < 1e-5) L = normalize(lp.xyz);\n"
    "      else {\n"
    "        vec3 d = lp.xyz - v_eye;\n"
    "        float dist = length(d);\n"
    "        L = d / max(dist, 1e-6);\n"
    "        att = 1.0 / max(u_l_att[i].x + u_l_att[i].y * dist + u_l_att[i].z * dist * dist, 1e-4);\n"
    "      }\n"
    "      float ndl = max(dot(N, L), 0.0);\n"
    "      vec3 H = normalize(L + V);\n"
    "      float spec = 0.0;\n"
    "      if (ndl > 0.0 && u_shine > 0.0)\n"
    "        spec = pow(max(dot(N, H), 0.0), max(u_shine, 0.001));\n"
    "      lit.rgb += att * (mata.rgb * u_l_a[i].rgb + matd.rgb * u_l_d[i].rgb * ndl\n"
    "                        + u_mat_s.rgb * u_l_s[i].rgb * spec);\n"
    "    }\n"
    "    lit.a = matd.a * col.a;\n"
    "    lit.rgb = max(lit.rgb, matd.rgb * 0.35 + mata.rgb * 0.15);\n"
    "    col = lit;\n"
    "    col.rgb = clamp(col.rgb, 0.0, 1.0);\n"
    "  }\n"
    "  if (u_tex_on != 0) {\n"
    "    vec4 tx = texture(u_samp, v_uv);\n"
    "    int m = u_texenv;\n"
    "    if (m == 0x1E01) col = tx;\n"
    "    else if (m == 0x2101) { col.rgb = mix(col.rgb, tx.rgb, tx.a); }\n"
    "    else if (m == 0x0BE2) { col.rgb = mix(col.rgb, u_texenv_c.rgb, tx.rgb); col.a = col.a * tx.a; }\n"
    "    else if (m == 0x0104) {\n"
    "      vec3 base = col.rgb;\n"
    "      if (base.r >= 0.95 && base.g >= 0.95 && base.b >= 0.95)\n"
    "        col.rgb = tx.rgb;\n"
    "      else\n"
    "        col.rgb = clamp(base + tx.rgb, 0.0, 1.0);\n"
    "      col.a = col.a * tx.a;\n"
    "    }\n"
    "    else col *= tx;\n"
    "  }\n"
    "  if (u_tex_on1 != 0) {\n"
    "    vec4 tx1 = texture(u_samp1, v_uv1);\n"
    "    int m1 = u_texenv1;\n"
    "    if (u_tex_on == 0 || m1 == 0x1E01) col = tx1;\n"
    "    else if (m1 == 0x0104) {\n"
    "      col.rgb = clamp(col.rgb + tx1.rgb, 0.0, 1.0);\n"
    "      col.a = col.a * tx1.a;\n"
    "    }\n"
    "    else col *= tx1;\n"
    "  }\n"
    "  if (!alpha_pass(col.a)) discard;\n"
    "  if (u_fog_on != 0) {\n"
    "    float z = abs(v_eye.z);\n"
    "    float f = 1.0;\n"
    "    if (u_fog_mode == 0x2601) {\n"
    "      float s = u_fog_e - u_fog_s;\n"
    "      f = (s == 0.0) ? 0.0 : clamp((u_fog_e - z) / s, 0.0, 1.0);\n"
    "    } else if (u_fog_mode == 0x0801) {\n"
    "      f = exp(-pow(u_fog_d * z, 2.0));\n"
    "    } else {\n"
    "      f = exp(-u_fog_d * z);\n"
    "    }\n"
    "    col.rgb = mix(u_fog_c.rgb, col.rgb, clamp(f, 0.0, 1.0));\n"
    "  }\n"
    "  col.rgb = clamp(col.rgb, 0.0, 1.0);\n"
    "  col.a = clamp(col.a, 0.0, 1.0);\n"
    "  o_col = col;\n"
    "}\n";

void tspgl_es1_init(void)
{
    uint32_t vs, fs, prog;
    int32_t ok = 0;
    char log[512];
    int32_t n = 0;
    int i;
    void (*getiv)(uint32_t, uint32_t, int32_t *) = (void *)G.glGetProgramiv;
    void (*getlog)(uint32_t, int32_t, int32_t *, char *) = (void *)G.glGetProgramInfoLog;
    if (E.ready && E.prog)
        return;
    memset(&E, 0, sizeof(E));
    E.mode = 0x1700;
    for (i = 0; i < 3; ++i)
        es1_ident(E.stack[i][0]);
    E.tex_unit = 0;
    E.tx_depth[0] = E.tx_depth[1] = 0;
    es1_ident(E.tx_stack[0][0]);
    es1_ident(E.tx_stack[1][0]);
    for (i = 0; i < 16; ++i)
        es1_ident(E.palette[i]);
    E.color[0] = E.color[1] = E.color[2] = E.color[3] = 1.f;
    E.normal[2] = 1.f;
    E.texcoord[3] = 1.f;
    E.mat_amb[0] = E.mat_amb[1] = E.mat_amb[2] = 0.2f;
    E.mat_amb[3] = 1.f;
    E.mat_diff[0] = E.mat_diff[1] = E.mat_diff[2] = 0.8f;
    E.mat_diff[3] = 1.f;
    E.mat_spec[3] = 1.f;
    E.mat_emis[3] = 1.f;
    E.scene_amb[0] = E.scene_amb[1] = E.scene_amb[2] = 0.2f;
    E.scene_amb[3] = 1.f;
    for (i = 0; i < 8; ++i)
        es1_light_def(&E.light[i], i);
    E.cap_light[0] = 1;
    E.texenv_mode[0] = E.texenv_mode[1] = GL_MODULATE;
    E.alpha_func = 0x0207;
    E.fog_mode = 0x0800;
    E.fog_density = 1.f;
    E.fog_end = 1.f;
    if (!G.glCreateShader || !G.glCreateProgram) {
        fprintf(stderr, "tspgl-es1: missing shader API\n");
        return;
    }
    vs = es1_compile(0x8B31, ES1_VS);
    fs = es1_compile(0x8B30, ES1_FS);
    if (!vs || !fs)
        return;
    prog = G.glCreateProgram();
    G.glAttachShader(prog, vs);
    G.glAttachShader(prog, fs);
    G.glLinkProgram(prog);
    if (getiv)
        getiv(prog, 0x8B82, &ok);
    if (!ok) {
        if (getlog)
            getlog(prog, (int32_t)sizeof(log) - 1, &n, log);
        log[n < 0 ? 0 : (n >= (int)sizeof(log) ? (int)sizeof(log) - 1 : n)] = 0;
        fprintf(stderr, "tspgl-es1: link failed:\n%s\n", log);
        return;
    }
    E.prog = prog;
    E.loc_mvp = es1_loc("u_mvp");
    E.loc_mv = es1_loc("u_mv");
    E.loc_nmat = es1_loc("u_nmat");
    E.loc_palette = es1_loc("u_palette[0]");
    if (E.loc_palette < 0)
        E.loc_palette = es1_loc("u_palette");
    E.loc_pal_on = es1_loc("u_palette_on");
    E.loc_lighting = es1_loc("u_lighting");
    E.loc_tex_on = es1_loc("u_tex_on");
    E.loc_tex_on1 = es1_loc("u_tex_on1");
    E.loc_texmat = es1_loc("u_texmat");
    E.loc_texmat1 = es1_loc("u_texmat1");
    E.loc_texenv = es1_loc("u_texenv");
    E.loc_texenv1 = es1_loc("u_texenv1");
    E.loc_texenv_c = es1_loc("u_texenv_c");
    E.loc_color = es1_loc("u_color");
    E.loc_normal = es1_loc("u_normal");
    E.loc_color_arr = es1_loc("u_color_arr");
    E.loc_norm_arr = es1_loc("u_norm_arr");
    E.loc_uvsz = es1_loc("u_uvsz");
    E.loc_uvsz1 = es1_loc("u_uvsz1");
    E.loc_add_no_color = es1_loc("u_add_no_color");
    E.loc_env_mod = es1_loc("u_env_mod");
    E.loc_mat_a = es1_loc("u_mat_a");
    E.loc_mat_d = es1_loc("u_mat_d");
    E.loc_mat_s = es1_loc("u_mat_s");
    E.loc_mat_e = es1_loc("u_mat_e");
    E.loc_shine = es1_loc("u_shine");
    E.loc_scene = es1_loc("u_scene");
    E.loc_l_on = es1_loc("u_l_on[0]");
    if (E.loc_l_on < 0)
        E.loc_l_on = es1_loc("u_l_on");
    E.loc_l_a = es1_loc("u_l_a[0]");
    if (E.loc_l_a < 0)
        E.loc_l_a = es1_loc("u_l_a");
    E.loc_l_d = es1_loc("u_l_d[0]");
    if (E.loc_l_d < 0)
        E.loc_l_d = es1_loc("u_l_d");
    E.loc_l_s = es1_loc("u_l_s[0]");
    if (E.loc_l_s < 0)
        E.loc_l_s = es1_loc("u_l_s");
    E.loc_l_p = es1_loc("u_l_p[0]");
    if (E.loc_l_p < 0)
        E.loc_l_p = es1_loc("u_l_p");
    E.loc_l_att = es1_loc("u_l_att[0]");
    if (E.loc_l_att < 0)
        E.loc_l_att = es1_loc("u_l_att");
    E.loc_alpha_on = es1_loc("u_alpha_on");
    E.loc_alpha_fn = es1_loc("u_alpha_fn");
    E.loc_alpha_ref = es1_loc("u_alpha_ref");
    E.loc_fog_on = es1_loc("u_fog_on");
    E.loc_fog_mode = es1_loc("u_fog_mode");
    E.loc_fog_c = es1_loc("u_fog_c");
    E.loc_fog_s = es1_loc("u_fog_s");
    E.loc_fog_e = es1_loc("u_fog_e");
    E.loc_fog_d = es1_loc("u_fog_d");
    E.loc_color_mat = es1_loc("u_color_mat");
    E.loc_samp = es1_loc("u_samp");
    E.loc_samp1 = es1_loc("u_samp1");
    E.ready = 1;
    fprintf(stderr, "tspgl-es1: GLES1 program ready prog=%u build=28\n", E.prog);
}

static float *es1_cur(void)
{
    int i = 0, d, u;
    if (E.mode == 0x1701)
        i = ES1_PR;
    else if (E.mode == 0x1702) {
        u = E.tex_unit;
        if (u < 0)
            u = 0;
        if (u > 1)
            u = 1;
        d = E.tx_depth[u];
        if (d < 0)
            d = 0;
        if (d >= ES1_STACK)
            d = ES1_STACK - 1;
        return E.tx_stack[u][d];
    }
    d = E.depth[i];
    if (d < 0)
        d = 0;
    if (d >= ES1_STACK)
        d = ES1_STACK - 1;
    return E.stack[i][d];
}

void tspgl_es1_bind_tex(uint32_t target, uint32_t id)
{
    if (target == GL_TEXTURE_2D)
        E.tex_bound[E.tex_unit < 2 ? E.tex_unit : 0] = id;
}

void tspgl_es1_set_active_tex(uint32_t texture)
{
    int u = (int)(texture >= 0x84C0u ? texture - 0x84C0u : 0);
    if (u < 0)
        u = 0;
    if (u > 1)
        u = 1;
    E.tex_unit = u;
}

static const float *es1_texmat_u(int u)
{
    int d;
    if (u < 0)
        u = 0;
    if (u > 1)
        u = 1;
    d = E.tx_depth[u];
    if (d < 0)
        d = 0;
    if (d >= ES1_STACK)
        d = ES1_STACK - 1;
    return E.tx_stack[u][d];
}

static const float *es1_texmat0(void)
{
    return es1_texmat_u(0);
}

static void es1_u1i(int loc, int v)
{
    if (loc >= 0 && G.glUniform1i)
        G.glUniform1i(loc, v);
}

static void es1_u1f(int loc, float v)
{
    void (*fn)(int32_t, float) = (void *)G.glUniform1f;
    if (loc >= 0 && fn)
        fn(loc, v);
}

static void es1_u4f(int loc, const float *v)
{
    void (*fn)(int32_t, int32_t, const float *) = (void *)G.glUniform4fv;
    if (loc >= 0 && fn)
        fn(loc, 1, v);
}

static void es1_u3f(int loc, const float *v)
{
    void (*fn)(int32_t, float, float, float) = (void *)G.glUniform3f;
    if (loc >= 0 && fn)
        fn(loc, v[0], v[1], v[2]);
}

static void es1_umat4(int loc, int n, const float *v)
{
    void (*fn)(int32_t, int32_t, uint32_t, const float *) = (void *)G.glUniformMatrix4fv;
    if (loc >= 0 && fn)
        fn(loc, n, 0, v);
}

static void es1_umat3(int loc, const float *v)
{
    void (*fn)(int32_t, int32_t, uint32_t, const float *) = (void *)G.glUniformMatrix3fv;
    if (loc >= 0 && fn)
        fn(loc, 1, 0, v);
}

static int es1_is_suit_atlas(int32_t texid);

static uint32_t es1_client_prog;
static int es1_gles2_seen;

int tspgl_es1_skip_draw(void)
{
    /* GLES2 titles call UseProgram(0) between passes. Feeding those
     * draws through the ES1 shader paints black over the world. */
    if (es1_gles2_seen && es1_client_prog == 0)
        return 1;
    return 0;
}

void tspgl_es1_note_client_program(uint32_t p)
{
    static int nlog;
    es1_client_prog = p;
    if (p && p != E.prog)
        es1_gles2_seen = 1;
    if (nlog < 12) {
        fprintf(stderr, "tspgl-es1: client UseProgram %u\n", p);
        nlog++;
    }
}

int tspgl_es1_gles2_active(void)
{
    return es1_client_prog != 0 && es1_client_prog != E.prog;
}

void tspgl_es1_bind_client_program(void)
{
    if (tspgl_es1_gles2_active() && G.glUseProgram)
        G.glUseProgram(es1_client_prog);
}

static int es1_is_env_mod(void)
{
    return (arr_en[3] && sattr[3].size >= 3 && E.texenv_mode[0] == GL_MODULATE) ||
           (arr_en[6] && sattr[6].size >= 3 && E.texenv_mode[1] == GL_MODULATE);
}

static int es1_is_suit_atlas(int32_t texid)
{
    const float *tm;
    if (!arr_en[3] || sattr[3].size != 2 || sattr[3].type != 0x1402)
        return 0;
    if (texid == 80)
        return 1;
    tm = es1_texmat0();
    return tm[0] > 1.51e-5f && tm[0] < 1.54e-5f &&
           tm[5] > 1.51e-5f && tm[5] < 1.54e-5f;
}

static void es1_layer_begin(int32_t texid)
{
    uint8_t on = 0;
    void (*getb)(uint32_t, uint8_t *) = (void *)G.glGetBooleanv;
    void (*getf)(uint32_t, float *) = (void *)G.glGetFloatv;
    void (*getiv)(uint32_t, int32_t *) = (void *)G.glGetIntegerv;
    if (E.env_po_on)
        return;
    E.env_blend_on = 0;
    if (es1_is_env_mod() || (arr_en[3] && sattr[3].size >= 3)) {
        /* Helmet/armor env: keep blend (HP must show through). Disable cull
         * so the helmet is visible from behind Isaac. */
        if (getb) {
            getb(GL_CULL_FACE, &on);
            E.saved_dmask = on;
        }
        if (G.glDisable)
            G.glDisable(GL_CULL_FACE);
        E.env_po_on = 1;
        /* Separate GL_ADD visor pass is drawn opaque; without framebuffer
         * add it replaces the albedo and can stick black in bright rooms. */
        if (E.texenv_mode[0] == GL_ADD) {
            uint8_t blend_on = 0;
            if (getb)
                getb(GL_BLEND, &blend_on);
            if (!blend_on) {
                E.saved_blend_on = 0;
                E.saved_blend_src = GL_ONE;
                E.saved_blend_dst = 0;
                if (getiv) {
                    getiv(GL_BLEND_SRC_RGB, &E.saved_blend_src);
                    getiv(GL_BLEND_DST_RGB, &E.saved_blend_dst);
                }
                if (G.glBlendFunc)
                    G.glBlendFunc(GL_ONE, GL_ONE);
                if (G.glEnable)
                    G.glEnable(GL_BLEND);
                E.env_blend_on = 1;
            }
        }
        return;
    }
    if (!es1_is_suit_atlas(texid))
        return;
    {
        uint8_t po = 0;
        float fu[2];
        fu[0] = fu[1] = 0.f;
        if (getb)
            getb(GL_POLYGON_OFFSET_FILL, &po);
        if (getf) {
            getf(GL_POLYGON_OFFSET_FACTOR, &fu[0]);
            getf(GL_POLYGON_OFFSET_UNITS, &fu[1]);
        }
        E.saved_po_en = po;
        E.saved_po_f = fu[0];
        E.saved_po_u = fu[1];
        if (G.glPolygonOffset)
            G.glPolygonOffset(-1.f, -4.f);
        if (G.glEnable)
            G.glEnable(GL_POLYGON_OFFSET_FILL);
        E.env_po_on = 2;
    }
}

void tspgl_es1_finish_draw(void)
{
    if (E.env_blend_on) {
        if (G.glBlendFunc)
            G.glBlendFunc(E.saved_blend_src, E.saved_blend_dst);
        if (E.saved_blend_on) {
            if (G.glEnable)
                G.glEnable(GL_BLEND);
        } else if (G.glDisable) {
            G.glDisable(GL_BLEND);
        }
        E.env_blend_on = 0;
    }
    if (E.env_po_on == 1) {
        if (E.saved_dmask) {
            if (G.glEnable)
                G.glEnable(GL_CULL_FACE);
        } else if (G.glDisable) {
            G.glDisable(GL_CULL_FACE);
        }
    } else if (E.env_po_on == 2) {
        if (G.glPolygonOffset)
            G.glPolygonOffset(E.saved_po_f, E.saved_po_u);
        if (E.saved_po_en) {
            if (G.glEnable)
                G.glEnable(GL_POLYGON_OFFSET_FILL);
        } else if (G.glDisable) {
            G.glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }
    E.env_po_on = 0;
}

void tspgl_es1_prepare_draw(void)
{
    float mvp[16], n3[9];
    int i, lon[8];
    float la[32], ld[32], ls[32], lp[32], att[24];
    void (*ena)(uint32_t) = (void *)G.glEnableVertexAttribArray;
    (void)ena;
    if (!E.ready)
        tspgl_es1_init();
    if (!E.ready || !G.glUseProgram)
        return;
    G.glUseProgram(E.prog);
    es1_mul(mvp, E.stack[ES1_PR][E.depth[ES1_PR]], E.stack[ES1_MV][E.depth[ES1_MV]]);
    es1_normal_from_mv(n3, E.stack[ES1_MV][E.depth[ES1_MV]]);
    es1_umat4(E.loc_mvp, 1, mvp);
    es1_umat4(E.loc_mv, 1, E.stack[ES1_MV][E.depth[ES1_MV]]);
    es1_umat3(E.loc_nmat, n3);
    es1_u1i(E.loc_pal_on, E.cap_palette);
    if (E.cap_palette)
        es1_umat4(E.loc_palette, 16, E.palette[0]);
    es1_u1i(E.loc_lighting, E.cap_lighting && arr_en[2]);
    es1_umat4(E.loc_texmat, 1, es1_texmat_u(0));
    es1_umat4(E.loc_texmat1, 1, es1_texmat_u(1));
    {
        int tex_on = 0, tex_on1 = 0;
        int32_t texid = (int32_t)E.tex_bound[0];
        int32_t texid1 = (int32_t)E.tex_bound[1];
        const float *tm = es1_texmat_u(0);
        tex_on = E.cap_tex2d[0];
        if (!tex_on && texid && arr_en[3])
            tex_on = 1;
        if (!texid)
            tex_on = 0;
        tex_on1 = E.cap_tex2d[1];
        if (!tex_on1 && texid1 && arr_en[6])
            tex_on1 = 1;
        if (!texid1)
            tex_on1 = 0;
        es1_u1i(E.loc_tex_on, tex_on);
        es1_u1i(E.loc_tex_on1, tex_on1);
        (void)tm;
    }
    es1_u1i(E.loc_texenv, (int)E.texenv_mode[0]);
    es1_u1i(E.loc_texenv1, (int)E.texenv_mode[1]);
    es1_u4f(E.loc_texenv_c, E.texenv_color);
    es1_u4f(E.loc_color, E.color);
    es1_u3f(E.loc_normal, E.normal);
    es1_u1i(E.loc_color_arr, arr_en[1] ? 1 : 0);
    es1_u1i(E.loc_norm_arr, arr_en[2] ? 1 : 0);
    es1_u1i(E.loc_uvsz, sattr[3].size);
    es1_u1i(E.loc_uvsz1, sattr[6].size);
    es1_u1i(E.loc_add_no_color, arr_en[1] ? 0 : 1);
    es1_u1i(E.loc_env_mod, es1_is_env_mod() ? 1 : 0);
    es1_u4f(E.loc_mat_a, E.mat_amb);
    es1_u4f(E.loc_mat_d, E.mat_diff);
    es1_u4f(E.loc_mat_s, E.mat_spec);
    es1_u4f(E.loc_mat_e, E.mat_emis);
    es1_u1f(E.loc_shine, E.shininess);
    es1_u4f(E.loc_scene, E.scene_amb);
    if (E.cap_lighting && arr_en[2]) {
        for (i = 0; i < 8; ++i) {
            lon[i] = E.cap_light[i];
            memcpy(la + i * 4, E.light[i].ambient, 16);
            memcpy(ld + i * 4, E.light[i].diffuse, 16);
            memcpy(ls + i * 4, E.light[i].specular, 16);
            memcpy(lp + i * 4, E.light[i].position, 16);
            memcpy(att + i * 3, E.light[i].att, 12);
        }
        {
            void (*u1iv)(int32_t, int32_t, const int32_t *) = (void *)G.glUniform1iv;
            void (*u4fv)(int32_t, int32_t, const float *) = (void *)G.glUniform4fv;
            void (*u3fv)(int32_t, int32_t, const float *) = (void *)G.glUniform3fv;
            if (u1iv && E.loc_l_on >= 0)
                u1iv(E.loc_l_on, 8, lon);
            if (u4fv) {
                if (E.loc_l_a >= 0)
                    u4fv(E.loc_l_a, 8, la);
                if (E.loc_l_d >= 0)
                    u4fv(E.loc_l_d, 8, ld);
                if (E.loc_l_s >= 0)
                    u4fv(E.loc_l_s, 8, ls);
                if (E.loc_l_p >= 0)
                    u4fv(E.loc_l_p, 8, lp);
            }
            if (u3fv && E.loc_l_att >= 0)
                u3fv(E.loc_l_att, 8, att);
        }
    }
    es1_u1i(E.loc_alpha_on, (arr_en[3] && sattr[3].size >= 3) ? 0 : E.cap_alpha);
    es1_u1i(E.loc_alpha_fn, (int)E.alpha_func);
    es1_u1f(E.loc_alpha_ref, E.alpha_ref);
    es1_u1i(E.loc_fog_on, E.cap_fog);
    if (E.cap_fog) {
        es1_u1i(E.loc_fog_mode, (int)E.fog_mode);
        es1_u4f(E.loc_fog_c, E.fog_color);
        es1_u1f(E.loc_fog_s, E.fog_start);
        es1_u1f(E.loc_fog_e, E.fog_end);
        es1_u1f(E.loc_fog_d, E.fog_density);
    }
    es1_u1i(E.loc_color_mat, E.cap_color_mat);
    {
        static int nlog;
        if (nlog < 12) {
            unsigned arr = 0;
            for (i = 0; i < 7; ++i)
                if (arr_en[i])
                    arr |= 1u << i;
            fprintf(stderr,
                    "tspgl-es1: draw arr=0x%x tex=%d bound=%u lighting=%d alpha=%d\n",
                    arr, E.cap_tex2d[0], E.tex_bound[0], E.cap_lighting,
                    E.cap_alpha);
            nlog++;
        }
    }
    {
        static int samp_set;
        if (!samp_set) {
            es1_u1i(E.loc_samp, 0);
            es1_u1i(E.loc_samp1, 1);
            samp_set = 1;
        }
    }
    if (G.glVertexAttrib4f) {
        if (!arr_en[1])
            G.glVertexAttrib4f(1, E.color[0], E.color[1], E.color[2], E.color[3]);
        if (!arr_en[2])
            G.glVertexAttrib3f(2, E.normal[0], E.normal[1], E.normal[2]);
        if (!arr_en[3])
            G.glVertexAttrib4f(3, E.texcoord[0], E.texcoord[1], 0.f, 1.f);
        if (!arr_en[4])
            G.glVertexAttrib4f(4, 1.f, 0.f, 0.f, 0.f);
        if (!arr_en[5])
            G.glVertexAttrib4f(5, 0.f, 0.f, 0.f, 0.f);
        if (!arr_en[6])
            G.glVertexAttrib4f(6, E.texcoord[0], E.texcoord[1], 0.f, 1.f);
    }
    if (G.glActiveTexture)
        G.glActiveTexture(0x84C0);
    es1_layer_begin((int32_t)E.tex_bound[0]);
}

static int es1_need(uint32_t n, uint32_t have)
{
    return have >= n;
}

static void es1_set_cap(uint32_t cap, int on)
{
    if (cap == GL_TEXTURE_2D)
        E.cap_tex2d[E.tex_unit < 2 ? E.tex_unit : 0] = on;
    else if (cap == GL_LIGHTING)
        E.cap_lighting = on;
    else if (cap == GL_COLOR_MATERIAL)
        E.cap_color_mat = on;
    else if (cap == GL_FOG)
        E.cap_fog = on;
    else if (cap == GL_ALPHA_TEST)
        E.cap_alpha = on;
    else if (cap == GL_MATRIX_PALETTE_OES)
        E.cap_palette = on;
    else if (cap >= GL_LIGHT0 && cap <= GL_LIGHT0 + 7)
        E.cap_light[cap - GL_LIGHT0] = on;
}

static void es1_drawtex(const float *u)
{
    /* window-space quad: x,y,z,w,h in pixels of the game FBO */
    float x = u[0], y = u[1], w = u[3], h = u[4];
    float gw = (float)(game_w > 0 ? game_w : 640);
    float gh = (float)(game_h > 0 ? game_h : 480);
    float x0 = (x / gw) * 2.f - 1.f;
    float y0 = (y / gh) * 2.f - 1.f;
    float x1 = ((x + w) / gw) * 2.f - 1.f;
    float y1 = ((y + h) / gh) * 2.f - 1.f;
    float verts[16] = {
        x0, y0, 0.f, 1.f, x1, y0, 0.f, 1.f, x0, y1, 0.f, 1.f, x1, y1, 0.f, 1.f
    };
    float uvs[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    uint32_t vbo = 0, tbo = 0;
    void (*gen)(int32_t, uint32_t *) = G.glGenBuffers;
    void (*bind)(uint32_t, uint32_t) = (void *)G.glBindBuffer;
    void (*data)(uint32_t, intptr_t, const void *, uint32_t) = (void *)G.glBufferData;
    void (*vap)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void *) =
        (void *)G.glVertexAttribPointer;
    void (*ena)(uint32_t) = (void *)G.glEnableVertexAttribArray;
    void (*dis)(uint32_t) = (void *)G.glDisableVertexAttribArray;
    void (*draw)(uint32_t, int32_t, int32_t) = (void *)G.glDrawArrays;
    int saved_tex = E.cap_tex2d[0];
    float ident[16];
    if (!gen || !bind || !data || !vap || !draw)
        return;
    es1_ident(ident);
    es1_copy(E.stack[ES1_MV][E.depth[ES1_MV]], ident);
    es1_copy(E.stack[ES1_PR][E.depth[ES1_PR]], ident);
    E.cap_tex2d[0] = 1;
    E.cap_lighting = 0;
    tspgl_es1_prepare_draw();
    gen(1, &vbo);
    gen(1, &tbo);
    bind(0x8892, vbo);
    data(0x8892, 16 * 4, verts, 0x88E0);
    if (ena)
        ena(0);
    vap(0, 4, 0x1406, 0, 0, (const void *)0);
    bind(0x8892, tbo);
    data(0x8892, 8 * 4, uvs, 0x88E0);
    if (ena)
        ena(3);
    vap(3, 2, 0x1406, 0, 0, (const void *)0);
    bind(0x8892, 0);
    draw(0x0005, 0, 4); /* triangle strip */
    tspgl_es1_finish_draw();
    if (dis) {
        dis(0);
        dis(3);
    }
    E.cap_tex2d[0] = saved_tex;
    (void)y0;
    (void)y1;
    (void)x0;
    (void)x1;
}

static int tspgl_dispatch_es1(uint32_t op, const uint8_t *in, uint32_t nbytes,
                              uint8_t *out, uint32_t *out_n)
{
    const uint32_t *u = (const uint32_t *)in;
    const float *f = (const float *)in;
    (void)out;
    if (out_n)
        *out_n = 0;
    if (op < OP_ES1_MATRIX_MODE || op >= OP_ES1_LAST)
        return -1;
    switch (op) {
    case OP_ES1_MATRIX_MODE:
        if (!es1_need(4, nbytes))
            return -1;
        E.mode = (int)u[0];
        return 0;
    case OP_ES1_LOAD_IDENTITY:
        es1_ident(es1_cur());
        return 0;
    case OP_ES1_LOAD_MATRIX:
        if (!es1_need(64, nbytes))
            return -1;
        memcpy(es1_cur(), f, 64);
        return 0;
    case OP_ES1_MULT_MATRIX:
        if (!es1_need(64, nbytes))
            return -1;
        es1_mul(es1_cur(), es1_cur(), f);
        return 0;
    case OP_ES1_PUSH: {
        int i = 0, d, u;
        if (E.mode == 0x1702) {
            u = E.tex_unit;
            if (u < 0)
                u = 0;
            if (u > 1)
                u = 1;
            d = E.tx_depth[u];
            if (d + 1 < ES1_STACK) {
                es1_copy(E.tx_stack[u][d + 1], E.tx_stack[u][d]);
                E.tx_depth[u] = d + 1;
            }
            return 0;
        }
        if (E.mode == 0x1701)
            i = ES1_PR;
        d = E.depth[i];
        if (d + 1 < ES1_STACK) {
            es1_copy(E.stack[i][d + 1], E.stack[i][d]);
            E.depth[i] = d + 1;
        }
        return 0;
    }
    case OP_ES1_POP: {
        int i = 0, u;
        if (E.mode == 0x1702) {
            u = E.tex_unit;
            if (u < 0)
                u = 0;
            if (u > 1)
                u = 1;
            if (E.tx_depth[u] > 0)
                E.tx_depth[u]--;
            return 0;
        }
        if (E.mode == 0x1701)
            i = ES1_PR;
        if (E.depth[i] > 0)
            E.depth[i]--;
        return 0;
    }
    case OP_ES1_TRANSLATE:
        if (!es1_need(12, nbytes))
            return -1;
        es1_translate(es1_cur(), f[0], f[1], f[2]);
        return 0;
    case OP_ES1_SCALE:
        if (!es1_need(12, nbytes))
            return -1;
        es1_scale(es1_cur(), f[0], f[1], f[2]);
        return 0;
    case OP_ES1_ROTATE:
        if (!es1_need(16, nbytes))
            return -1;
        es1_rotate(es1_cur(), f[0], f[1], f[2], f[3]);
        return 0;
    case OP_ES1_FRUSTUM:
        if (!es1_need(24, nbytes))
            return -1;
        es1_frustum(es1_cur(), f[0], f[1], f[2], f[3], f[4], f[5]);
        return 0;
    case OP_ES1_ORTHO:
        if (!es1_need(24, nbytes))
            return -1;
        es1_ortho(es1_cur(), f[0], f[1], f[2], f[3], f[4], f[5]);
        return 0;
    case OP_ES1_ENABLE:
        if (!es1_need(4, nbytes))
            return -1;
        es1_set_cap(u[0], 1);
        return 0;
    case OP_ES1_DISABLE:
        if (!es1_need(4, nbytes))
            return -1;
        es1_set_cap(u[0], 0);
        return 0;
    case OP_ES1_COLOR:
        if (!es1_need(16, nbytes))
            return -1;
        memcpy(E.color, f, 16);
        return 0;
    case OP_ES1_NORMAL:
        if (!es1_need(12, nbytes))
            return -1;
        memcpy(E.normal, f, 12);
        return 0;
    case OP_ES1_TEXCOORD:
        if (!es1_need(16, nbytes))
            return -1;
        memcpy(E.texcoord, f, 16);
        return 0;
    case OP_ES1_ALPHA_FUNC:
        if (!es1_need(8, nbytes))
            return -1;
        E.alpha_func = u[0];
        E.alpha_ref = es1_f32(in + 4);
        return 0;
    case OP_ES1_SHADE_MODEL:
        return 0;
    case OP_ES1_POINT_SIZE:
        if (es1_need(4, nbytes) && G.glLineWidth)
            ; /* glPointSize may exist */
        return 0;
    case OP_ES1_CLIENT_TEX:
        return 0;
    case OP_ES1_LIGHT: {
        int li;
        uint32_t pname;
        const float *p;
        if (!es1_need(8 + 16, nbytes))
            return -1;
        if (u[0] < GL_LIGHT0 || u[0] > GL_LIGHT0 + 7)
            return 0;
        li = (int)(u[0] - GL_LIGHT0);
        pname = u[1];
        p = (const float *)(in + 8);
        switch (pname) {
        case GL_AMBIENT:
            memcpy(E.light[li].ambient, p, 16);
            break;
        case GL_DIFFUSE:
            memcpy(E.light[li].diffuse, p, 16);
            break;
        case GL_SPECULAR:
            memcpy(E.light[li].specular, p, 16);
            break;
        case GL_POSITION: {
            float eye[4], *mv = E.stack[ES1_MV][E.depth[ES1_MV]];
            /* transform by current modelview into eye space */
            eye[0] = mv[0] * p[0] + mv[4] * p[1] + mv[8] * p[2] + mv[12] * p[3];
            eye[1] = mv[1] * p[0] + mv[5] * p[1] + mv[9] * p[2] + mv[13] * p[3];
            eye[2] = mv[2] * p[0] + mv[6] * p[1] + mv[10] * p[2] + mv[14] * p[3];
            eye[3] = mv[3] * p[0] + mv[7] * p[1] + mv[11] * p[2] + mv[15] * p[3];
            memcpy(E.light[li].position, eye, 16);
            break;
        }
        case GL_SPOT_DIRECTION:
            memcpy(E.light[li].spot_dir, p, 12);
            break;
        case GL_SPOT_EXPONENT:
            E.light[li].spot_exp = p[0];
            break;
        case GL_SPOT_CUTOFF:
            E.light[li].spot_cut = p[0];
            break;
        case GL_CONSTANT_ATTENUATION:
            E.light[li].att[0] = p[0];
            break;
        case GL_LINEAR_ATTENUATION:
            E.light[li].att[1] = p[0];
            break;
        case GL_QUADRATIC_ATTENUATION:
            E.light[li].att[2] = p[0];
            break;
        default:
            break;
        }
        return 0;
    }
    case OP_ES1_LIGHT_MODEL:
        if (!es1_need(4 + 16, nbytes))
            return -1;
        if (u[0] == GL_LIGHT_MODEL_AMBIENT)
            memcpy(E.scene_amb, in + 4, 16);
        return 0;
    case OP_ES1_MATERIAL: {
        uint32_t pname;
        const float *p;
        if (!es1_need(8 + 16, nbytes))
            return -1;
        pname = u[1];
        p = (const float *)(in + 8);
        if (pname == GL_SHININESS)
            E.shininess = p[0];
        else if (pname == GL_AMBIENT || pname == GL_AMBIENT_AND_DIFFUSE) {
            memcpy(E.mat_amb, p, 16);
            if (pname == GL_AMBIENT_AND_DIFFUSE)
                memcpy(E.mat_diff, p, 16);
        } else if (pname == GL_DIFFUSE)
            memcpy(E.mat_diff, p, 16);
        else if (pname == GL_SPECULAR)
            memcpy(E.mat_spec, p, 16);
        else if (pname == GL_EMISSION)
            memcpy(E.mat_emis, p, 16);
        return 0;
    }
    case OP_ES1_TEXENV: {
        uint32_t pname;
        const float *p;
        if (!es1_need(8 + 4, nbytes))
            return -1;
        pname = u[1];
        p = (const float *)(in + 8);
        if (pname == GL_TEXTURE_ENV_MODE)
            E.texenv_mode[E.tex_unit < 2 ? E.tex_unit : 0] = (uint32_t)p[0];
        else if (pname == GL_TEXTURE_ENV_COLOR && nbytes >= 8 + 16)
            memcpy(E.texenv_color, p, 16);
        return 0;
    }
    case OP_ES1_FOG:
        if (!es1_need(4 + 4, nbytes))
            return -1;
        if (u[0] == GL_FOG_MODE)
            E.fog_mode = (uint32_t)es1_f32(in + 4);
        else if (u[0] == GL_FOG_DENSITY)
            E.fog_density = es1_f32(in + 4);
        else if (u[0] == GL_FOG_START)
            E.fog_start = es1_f32(in + 4);
        else if (u[0] == GL_FOG_END)
            E.fog_end = es1_f32(in + 4);
        else if (u[0] == GL_FOG_COLOR && nbytes >= 4 + 16)
            memcpy(E.fog_color, in + 4, 16);
        return 0;
    case OP_ES1_PALETTE_CURRENT:
        if (!es1_need(4, nbytes))
            return -1;
        E.pal_i = (int)(u[0] < 16 ? u[0] : 15);
        return 0;
    case OP_ES1_PALETTE_LOAD:
        es1_copy(E.palette[E.pal_i], E.stack[ES1_MV][E.depth[ES1_MV]]);
        return 0;
    case OP_ES1_DRAWTEX:
        if (!es1_need(20, nbytes))
            return -1;
        es1_drawtex(f);
        return 0;
    default:
        return -1;
    }
}
