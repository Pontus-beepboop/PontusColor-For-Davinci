/* ============================================================================
 *  PontusColor - shared color-science engine
 *  ---------------------------------------------------------------------------
 *  ONE source of truth for the look math. This same file is compiled by every
 *  backend (CUDA / OpenCL / Metal / CPU) through a thin macro layer so the
 *  image is identical on every GPU and on every OS.
 *
 *  Pipeline (per pixel):
 *     input encoding  -> scene-linear (Rec.709 primaries)
 *     white balance (temperature / tint, luminance preserving)
 *     exposure
 *     subtractive saturation (GCR / dye-density model)  <-- the "film" core
 *     luminance-weighted shadow / highlight desaturation
 *     filmic tone curve (log-pivot contrast + soft highlight shoulder + toe)
 *     print / stock color cast
 *     output encoding (Rec.709 2.4 by default)
 *     dry/wet look-intensity mix
 * ==========================================================================*/

#ifndef PONTUSCOLOR_KERNEL_H
#define PONTUSCOLOR_KERNEL_H

/* ----------------------------------------------------------------------------
 *  Cross-backend macro layer
 * --------------------------------------------------------------------------*/
#if defined(__CUDACC__)                 /* ---- CUDA ---- */
    #define PC_DEV   __device__ static inline
    #define PC_POW   powf
    #define PC_EXP   expf
    #define PC_LOG   logf
    #define PC_LOG10 log10f
    #define PC_ABS   fabsf
    #define PC_MAX   fmaxf
    #define PC_MIN   fminf
#elif defined(__OPENCL_VERSION__)       /* ---- OpenCL ---- */
    #define PC_DEV   static inline
    #define PC_POW   pow
    #define PC_EXP   exp
    #define PC_LOG   log
    #define PC_LOG10 log10
    #define PC_ABS   fabs
    #define PC_MAX   fmax
    #define PC_MIN   fmin
#elif defined(__METAL_VERSION__)        /* ---- Metal ---- */
    #include <metal_stdlib>
    using namespace metal;
    #define PC_DEV   inline
    #define PC_POW   pow
    #define PC_EXP   exp
    #define PC_LOG   log
    #define PC_LOG10 log10
    #define PC_ABS   fabs
    #define PC_MAX   fmax
    #define PC_MIN   fmin
#else                                   /* ---- CPU (C++) ---- */
    #include <math.h>
    #define PC_DEV   static inline
    #define PC_POW   powf
    #define PC_EXP   expf
    #define PC_LOG   logf
    #define PC_LOG10 log10f
    #define PC_ABS   fabsf
    #define PC_MAX   fmaxf
    #define PC_MIN   fminf
#endif

/* Plain 3-float container used everywhere (avoids float3 constructor diffs). */
typedef struct PCrgb_s { float r, g, b; } PCrgb;

/* All UI values, passed by value into the kernel.  Keep POD + 4-byte fields so
 * the identical struct lays out the same on host and every device. */
typedef struct PCparams_s {
    int   inputSpace;     /* see PC_IN_* below                         */
    int   outputSpace;    /* see PC_OUT_*                              */
    int   look;           /* base look preset                          */
    float exposure;       /* stops                                     */
    float temperature;    /* -1..+1  (warm positive)                   */
    float tint;           /* -1..+1  (magenta positive)                */
    float contrast;       /* 0..2, 1 = neutral                         */
    float pivot;          /* middle-grey pivot (linear), ~0.18         */
    float highlightRolloff;/* 0..1 filmic shoulder strength            */
    float toe;            /* 0..1 black softness / lift                */
    float subSaturation;  /* 0..2 subtractive saturation, 1 = neutral  */
    float saturation;     /* 0..2 final perceptual saturation          */
    float highlightDesat; /* 0..1                                      */
    float shadowDesat;    /* 0..1                                      */
    float filmWarmth;     /* -1..1 shadow/highlight split warmth       */
    float printDensity;   /* 0..2, 1 = neutral                         */
    float lookIntensity;  /* 0..1 dry/wet                              */
} PCparams;

/* Input encodings */
#define PC_IN_REC709        0
#define PC_IN_LINEAR        1
#define PC_IN_SLOG3_CINE    2
#define PC_IN_SLOG3         3
#define PC_IN_REDLOG3G10    4
#define PC_IN_LOGC3         5
#define PC_IN_LOGC4         6
#define PC_IN_VLOG          7
#define PC_IN_BMDFILM5      8
#define PC_IN_DLOG          9
#define PC_IN_CLOG3         10
#define PC_IN_FLOG2         11
#define PC_IN_CINEON        12

/* Output encodings */
#define PC_OUT_REC709       0
#define PC_OUT_SRGB         1
#define PC_OUT_LINEAR       2
#define PC_OUT_REC709_SCENE 3   /* passthrough: leave scene-linear (for a CST after) */

/* Base looks */
#define PC_LOOK_NEUTRAL     0
#define PC_LOOK_FILMIC      1
#define PC_LOOK_BLEACH      2
#define PC_LOOK_WARMPRINT   3
#define PC_LOOK_COOLMODERN  4
#define PC_LOOK_VINTAGE     5

/* ----------------------------------------------------------------------------
 *  Small helpers
 * --------------------------------------------------------------------------*/
PC_DEV float pc_clampf(float v, float lo, float hi){ return PC_MIN(PC_MAX(v, lo), hi); }
PC_DEV float pc_sat01(float v){ return pc_clampf(v, 0.0f, 1.0f); }
PC_DEV float pc_luma(PCrgb c){ return 0.2126f*c.r + 0.7152f*c.g + 0.0722f*c.b; }

PC_DEV PCrgb pc_mat3(PCrgb c, const float m[9]){
    PCrgb o;
    o.r = m[0]*c.r + m[1]*c.g + m[2]*c.b;
    o.g = m[3]*c.r + m[4]*c.g + m[5]*c.b;
    o.b = m[6]*c.r + m[7]*c.g + m[8]*c.b;
    return o;
}

/* signed power: keeps behaviour stable for small negatives (wide gamuts) */
PC_DEV float pc_spow(float x, float p){
    float s = (x < 0.0f) ? -1.0f : 1.0f;
    return s * PC_POW(PC_ABS(x), p);
}

/* ----------------------------------------------------------------------------
 *  Camera gamut -> Rec.709 (linear, D65) matrices, as macro literals so they
 *  drop into address-space-neutral local arrays (CUDA/OpenCL/Metal/CPU safe).
 *  Computed from published primaries (see tools/gamut.py). Match Resolve CST
 *  within rounding.
 * --------------------------------------------------------------------------*/
#define PC_MAT_SGAMUT3CINE 1.6269474f,-0.5401385f,-0.0868089f,-0.1785155f,1.4179409f,-0.2394254f,-0.0444361f,-0.1959200f,1.2403561f
#define PC_MAT_SGAMUT3     1.8779151f,-0.7941688f,-0.0837464f,-0.1768070f,1.3509996f,-0.1741926f,-0.0262011f,-0.1484223f,1.1746234f
#define PC_MAT_AWG3        1.6175234f,-0.5372866f,-0.0802368f,-0.0705727f,1.3346131f,-0.2640403f,-0.0211017f,-0.2269539f,1.2480556f
#define PC_MAT_AWG4        1.8931234f,-0.7808815f,-0.1122419f,-0.2057004f,1.3402575f,-0.1345571f,-0.0127057f,-0.1521849f,1.1648906f
#define PC_MAT_VGAMUT      1.8065759f,-0.6956973f,-0.1108786f,-0.1700903f,1.3059552f,-0.1358649f,-0.0252058f,-0.1544683f,1.1796741f
#define PC_MAT_REDWG       1.9819760f,-0.9004318f,-0.0815442f,-0.1781432f,1.5004684f,-0.3223252f,-0.1017960f,-0.5352635f,1.6370594f
#define PC_MAT_BMDWG5      1.5685504f,-0.5228361f,-0.0457143f,-0.0863793f,1.3449178f,-0.2585385f,-0.0520103f,-0.2491165f,1.3011268f
#define PC_MAT_DGAMUT      1.6747231f,-0.5797890f,-0.0949341f,-0.0980877f,1.3339837f,-0.2358960f,-0.0409671f,-0.2429838f,1.2839509f
#define PC_MAT_CINEMAGAMUT 1.9238613f,-0.7987607f,-0.1251006f,-0.2043108f,1.4958985f,-0.2915877f,-0.0236850f,-0.4201270f,1.4438120f
#define PC_MAT_REC2020     1.6604910f,-0.5876411f,-0.0728499f,-0.1245505f,1.1328999f,-0.0083494f,-0.0181508f,-0.1005789f,1.1187297f

/* ----------------------------------------------------------------------------
 *  Log/gamma decode curves  (signal 0..1  ->  scene-linear, 0.18 = mid grey)
 *  All formulas are the camera makers' published transfer functions.
 * --------------------------------------------------------------------------*/
PC_DEV float pc_dec_rec709(float v){ return pc_spow(PC_MAX(v,0.0f), 2.4f); }      /* display gamma */
PC_DEV float pc_dec_srgb(float v){
    return (v <= 0.04045f) ? v/12.92f : PC_POW((v+0.055f)/1.055f, 2.4f);
}
PC_DEV float pc_dec_slog3(float v){
    return (v >= 0.1673609920f)
        ? (PC_POW(10.0f, (v*1023.0f-420.0f)/261.5f)*0.19f - 0.01f)
        : (v*1023.0f-95.0f)*0.01125000f/(171.2102946929f-95.0f);
}
PC_DEV float pc_dec_redlog3g10(float v){
    const float a=0.224282f, b=155.975327f, c=0.01f;
    float x = v + 0.01f;                 /* black offset */
    return (PC_POW(10.0f, x/a) - 1.0f)/b - c;
}
PC_DEV float pc_dec_logc3(float v){      /* ARRI LogC EI800 */
    return (v > 0.1496582f)
        ? (PC_POW(10.0f,(v-0.385537f)/0.2471896f) - 0.052272f)/5.555556f
        : (v - 0.092809f)/5.367655f;
}
PC_DEV float pc_dec_logc4(float v){      /* ARRI LogC4 */
    const float a = (PC_POW(2.0f,18.0f)-16.0f)/117.45f;
    const float b = (1023.0f-95.0f)/1023.0f;
    const float c = 95.0f/1023.0f;
    const float s = (7.0f*PC_LOG(2.0f)*PC_POW(2.0f,7.0f-14.0f*c/b))/(a*b);
    const float t = (PC_POW(2.0f,14.0f*(-c/b)+6.0f)-64.0f)/a;
    float x = (v - c)/b;
    return (x < 0.0f) ? x*s + t
                      : (PC_POW(2.0f, 14.0f*x + 6.0f) - 64.0f)/a;
}
PC_DEV float pc_dec_vlog(float v){       /* Panasonic V-Log */
    const float b=0.00873f, c=0.241514f, d=0.598206f;
    return (v < 0.181f) ? (v - 0.125f)/5.6f
                        : PC_POW(10.0f,(v-d)/c) - b;
}
PC_DEV float pc_dec_bmdfilm5(float v){   /* Blackmagic Film Gen5 */
    const float A=0.08692876065491224f, B=0.005494072432257808f, C=0.5300133392291939f;
    const float D=8.283605932402494f,  E=0.09246575342465753f, cut=0.005f;
    return (v < (D*cut+E)) ? (v - E)/D : PC_EXP((v - C)/A) - B;
}
PC_DEV float pc_dec_dlog(float v){       /* DJI D-Log */
    return (v <= 0.14f) ? (v - 0.0929f)/6.025f
                        : (PC_POW(10.0f, 3.89616f*v - 2.27752f) - 0.0108f)/0.9892f;
}
PC_DEV float pc_dec_clog3(float v){      /* Canon Log3 */
    if (v < 0.097465473f)  return -(PC_POW(10.0f,(0.12783901f - v)/0.36726845f) - 1.0f)/14.98325f;
    if (v <= 0.15277891f)  return (v - 0.12512248f)/1.9754798f;
    return (PC_POW(10.0f,(v - 0.12240537f)/0.36726845f) - 1.0f)/14.98325f;
}
PC_DEV float pc_dec_flog2(float v){      /* Fujifilm F-Log2 */
    const float a=5.555556f,b=0.064829f,c=0.245281f,d=0.384316f,e=8.799461f,f=0.092864f;
    const float cut=0.100686685370811f;
    return (v < cut) ? (v - f)/e
                     : (PC_POW(10.0f,(v - d)/c) - b)/a;
}
PC_DEV float pc_dec_cineon(float v){     /* Kodak Cineon log */
    float bk = PC_POW(10.0f,(95.0f-685.0f)*0.002f/0.6f);
    float lin = PC_POW(10.0f,(v*1023.0f-685.0f)*0.002f/0.6f);
    return (lin - bk)/(1.0f - bk);
}

/* Decode signal -> scene-linear Rec.709 primaries */
PC_DEV PCrgb pc_to_scene_linear(PCrgb c, int space){
    PCrgb l = c;
    if (space == PC_IN_LINEAR) return c;
    if (space == PC_IN_REC709){
        l.r=pc_dec_rec709(c.r); l.g=pc_dec_rec709(c.g); l.b=pc_dec_rec709(c.b); return l;
    }
    if (space == PC_IN_CINEON){
        l.r=pc_dec_cineon(c.r); l.g=pc_dec_cineon(c.g); l.b=pc_dec_cineon(c.b); return l;
    }
    switch (space){
        case PC_IN_SLOG3_CINE:{
            const float m[9]={PC_MAT_SGAMUT3CINE};
            l.r=pc_dec_slog3(c.r); l.g=pc_dec_slog3(c.g); l.b=pc_dec_slog3(c.b);
            return pc_mat3(l, m); }
        case PC_IN_SLOG3:{
            const float m[9]={PC_MAT_SGAMUT3};
            l.r=pc_dec_slog3(c.r); l.g=pc_dec_slog3(c.g); l.b=pc_dec_slog3(c.b);
            return pc_mat3(l, m); }
        case PC_IN_REDLOG3G10:{
            const float m[9]={PC_MAT_REDWG};
            l.r=pc_dec_redlog3g10(c.r); l.g=pc_dec_redlog3g10(c.g); l.b=pc_dec_redlog3g10(c.b);
            return pc_mat3(l, m); }
        case PC_IN_LOGC3:{
            const float m[9]={PC_MAT_AWG3};
            l.r=pc_dec_logc3(c.r); l.g=pc_dec_logc3(c.g); l.b=pc_dec_logc3(c.b);
            return pc_mat3(l, m); }
        case PC_IN_LOGC4:{
            const float m[9]={PC_MAT_AWG4};
            l.r=pc_dec_logc4(c.r); l.g=pc_dec_logc4(c.g); l.b=pc_dec_logc4(c.b);
            return pc_mat3(l, m); }
        case PC_IN_VLOG:{
            const float m[9]={PC_MAT_VGAMUT};
            l.r=pc_dec_vlog(c.r); l.g=pc_dec_vlog(c.g); l.b=pc_dec_vlog(c.b);
            return pc_mat3(l, m); }
        case PC_IN_BMDFILM5:{
            const float m[9]={PC_MAT_BMDWG5};
            l.r=pc_dec_bmdfilm5(c.r); l.g=pc_dec_bmdfilm5(c.g); l.b=pc_dec_bmdfilm5(c.b);
            return pc_mat3(l, m); }
        case PC_IN_DLOG:{
            const float m[9]={PC_MAT_DGAMUT};
            l.r=pc_dec_dlog(c.r); l.g=pc_dec_dlog(c.g); l.b=pc_dec_dlog(c.b);
            return pc_mat3(l, m); }
        case PC_IN_CLOG3:{
            const float m[9]={PC_MAT_CINEMAGAMUT};
            l.r=pc_dec_clog3(c.r); l.g=pc_dec_clog3(c.g); l.b=pc_dec_clog3(c.b);
            return pc_mat3(l, m); }
        case PC_IN_FLOG2:{
            const float m[9]={PC_MAT_REC2020};
            l.r=pc_dec_flog2(c.r); l.g=pc_dec_flog2(c.g); l.b=pc_dec_flog2(c.b);
            return pc_mat3(l, m); }
        default:
            return c;
    }
}

/* ----------------------------------------------------------------------------
 *  Output encode  (scene-linear Rec.709  ->  signal)
 * --------------------------------------------------------------------------*/
PC_DEV float pc_enc_rec709(float v){ return pc_spow(PC_MAX(v,0.0f), 1.0f/2.4f); }
PC_DEV float pc_enc_srgb(float v){
    v = PC_MAX(v, 0.0f);
    return (v <= 0.0031308f) ? v*12.92f : 1.055f*PC_POW(v, 1.0f/2.4f) - 0.055f;
}
PC_DEV PCrgb pc_encode(PCrgb c, int space){
    PCrgb o = c;
    switch (space){
        case PC_OUT_REC709: o.r=pc_enc_rec709(c.r); o.g=pc_enc_rec709(c.g); o.b=pc_enc_rec709(c.b); return o;
        case PC_OUT_SRGB:   o.r=pc_enc_srgb(c.r);   o.g=pc_enc_srgb(c.g);   o.b=pc_enc_srgb(c.b);   return o;
        case PC_OUT_LINEAR:
        case PC_OUT_REC709_SCENE:
        default: return c;
    }
}

/* ----------------------------------------------------------------------------
 *  White balance  (temperature / tint) - luminance preserving channel gains.
 *  Operates in scene-linear. Warm (+temp) lifts R / drops B; +tint -> magenta.
 * --------------------------------------------------------------------------*/
PC_DEV PCrgb pc_white_balance(PCrgb c, float temp, float tint){
    float rg = 1.0f + 0.45f*temp;
    float bg = 1.0f - 0.45f*temp;
    float gg = 1.0f - 0.18f*tint;
    float rb = 1.0f + 0.09f*tint;
    PCrgb o;
    o.r = c.r * rg * rb;
    o.g = c.g * gg;
    o.b = c.b * bg * rb;
    /* preserve luminance so WB does not change overall brightness */
    float l0 = pc_luma(c);
    float l1 = pc_luma(o);
    float k  = (l1 > 1e-6f) ? (l0 / l1) : 1.0f;
    /* blend the normalisation slightly so deep tints still read */
    k = 0.85f*k + 0.15f;
    o.r *= k; o.g *= k; o.b *= k;
    return o;
}

/* ----------------------------------------------------------------------------
 *  Subtractive saturation - the film core.
 *  Works in the inverted "dye density" domain. The neutral (grey-component)
 *  density is held while the *pure* dye part is scaled, so colours deepen and
 *  shift like film dyes instead of going electric / clipping like RGB gain.
 * --------------------------------------------------------------------------*/
PC_DEV PCrgb pc_subtractive_sat(PCrgb c, float amount){
    /* map linear -> printing density (more dye = darker) */
    float dr = 1.0f - pc_sat01(c.r);
    float dg = 1.0f - pc_sat01(c.g);
    float db = 1.0f - pc_sat01(c.b);
    float k  = PC_MIN(dr, PC_MIN(dg, db));   /* grey-component (neutral) */
    /* scale only the pure-dye portion */
    dr = k + (dr - k)*amount;
    dg = k + (dg - k)*amount;
    db = k + (db - k)*amount;
    PCrgb o;
    o.r = 1.0f - dr; o.g = 1.0f - dg; o.b = 1.0f - db;
    /* keep HDR values above 1.0 that the sat() clipped untouched in ratio */
    o.r = (c.r > 1.0f) ? c.r : o.r;
    o.g = (c.g > 1.0f) ? c.g : o.g;
    o.b = (c.b > 1.0f) ? c.b : o.b;
    return o;
}

/* Perceptual saturation + tonal desaturation of shadows & highlights */
PC_DEV PCrgb pc_tonal_saturation(PCrgb c, float sat, float hiDesat, float loDesat){
    float l = pc_luma(c);
    /* shadow & highlight weights (0 in mids, ->1 at extremes) */
    float hw = pc_sat01((l - 0.5f) * 2.0f);          /* highlights */
    float lw = pc_sat01((0.25f - l) * 4.0f);         /* shadows    */
    float s  = sat * (1.0f - hiDesat*hw) * (1.0f - loDesat*lw);
    PCrgb o;
    o.r = l + (c.r - l)*s;
    o.g = l + (c.g - l)*s;
    o.b = l + (c.b - l)*s;
    return o;
}

/* ----------------------------------------------------------------------------
 *  Filmic tone curve: log-pivot contrast, soft highlight shoulder, toe lift.
 * --------------------------------------------------------------------------*/
PC_DEV float pc_tone_channel(float x, float contrast, float pivot, float rolloff, float toe){
    x = PC_MAX(x, 0.0f);
    /* log-domain S-curve contrast around the grey pivot */
    float eps = 1e-5f;
    float lx  = PC_LOG(x + eps);
    float lp  = PC_LOG(pivot + eps);
    lx = (lx - lp)*contrast + lp;
    x  = PC_EXP(lx) - eps;
    x  = PC_MAX(x, 0.0f);
    /* filmic highlight shoulder (extended Reinhard), blended by rolloff */
    float wp = 4.0f;                                  /* white point in stops-ish */
    float shoulder = x * (1.0f + x/(wp*wp)) / (1.0f + x);
    x = x*(1.0f - rolloff) + shoulder*rolloff;
    /* gentle toe: lift & soften the deepest blacks */
    float t = toe*0.02f;
    x = x + t*(1.0f - x/(x + 0.05f));
    return PC_MAX(x, 0.0f);
}
PC_DEV PCrgb pc_filmic_tone(PCrgb c, float contrast, float pivot, float rolloff, float toe){
    PCrgb o;
    o.r = pc_tone_channel(c.r, contrast, pivot, rolloff, toe);
    o.g = pc_tone_channel(c.g, contrast, pivot, rolloff, toe);
    o.b = pc_tone_channel(c.b, contrast, pivot, rolloff, toe);
    return o;
}

/* ----------------------------------------------------------------------------
 *  Print / stock colour cast - split warmth: warm shadows, cooler highlights
 *  (warmth>0) for a classic print feel; plus overall density.
 * --------------------------------------------------------------------------*/
PC_DEV PCrgb pc_print(PCrgb c, float warmth, float density){
    float l = pc_luma(c);
    float shadow = 1.0f - pc_sat01(l*1.6f);
    float hi     = pc_sat01((l - 0.5f)*2.0f);
    float w = warmth;
    PCrgb o = c;
    o.r += (0.025f*w)*shadow - (0.02f*w)*hi;
    o.b -= (0.022f*w)*shadow - (0.03f*w)*hi;
    o.g += (0.008f*w)*shadow;
    /* print density: subtle overall gamma toward richer blacks */
    float d = 1.0f + (density - 1.0f)*0.35f;
    o.r = pc_spow(PC_MAX(o.r,0.0f), d);
    o.g = pc_spow(PC_MAX(o.g,0.0f), d);
    o.b = pc_spow(PC_MAX(o.b,0.0f), d);
    return o;
}

/* ----------------------------------------------------------------------------
 *  Base-look preset offsets. Multiply onto user values so the user's sliders
 *  always layer on top of a chosen starting point.
 * --------------------------------------------------------------------------*/
PC_DEV void pc_apply_look(int look, PCparams* p){
    switch (look){
        case PC_LOOK_FILMIC:
            p->subSaturation *= 1.10f; p->saturation *= 0.92f;
            p->highlightDesat = PC_MAX(p->highlightDesat, 0.35f);
            p->highlightRolloff = PC_MAX(p->highlightRolloff, 0.6f);
            break;
        case PC_LOOK_BLEACH:
            p->subSaturation *= 0.55f; p->saturation *= 0.65f;
            p->contrast *= 1.25f; p->printDensity *= 1.15f;
            break;
        case PC_LOOK_WARMPRINT:
            p->filmWarmth += 0.5f; p->subSaturation *= 1.05f;
            p->highlightDesat = PC_MAX(p->highlightDesat, 0.3f);
            break;
        case PC_LOOK_COOLMODERN:
            p->filmWarmth -= 0.35f; p->contrast *= 1.1f;
            p->saturation *= 0.95f; p->shadowDesat = PC_MAX(p->shadowDesat,0.2f);
            break;
        case PC_LOOK_VINTAGE:
            p->subSaturation *= 0.8f; p->toe = PC_MAX(p->toe, 0.5f);
            p->filmWarmth += 0.3f; p->highlightDesat = PC_MAX(p->highlightDesat,0.45f);
            break;
        case PC_LOOK_NEUTRAL:
        default: break;
    }
}

/* ----------------------------------------------------------------------------
 *  The full per-pixel transform.
 * --------------------------------------------------------------------------*/
PC_DEV PCrgb PontusColorPixel(PCrgb src, PCparams p){
    pc_apply_look(p.look, &p);

    PCrgb lin = pc_to_scene_linear(src, p.inputSpace);

    /* exposure */
    float ev = PC_POW(2.0f, p.exposure);
    lin.r *= ev; lin.g *= ev; lin.b *= ev;

    /* white balance */
    lin = pc_white_balance(lin, p.temperature, p.tint);

    /* subtractive saturation (film dye model) */
    lin = pc_subtractive_sat(lin, p.subSaturation);

    /* tonal (shadow/highlight) desaturation + final saturation */
    lin = pc_tonal_saturation(lin, p.saturation, p.highlightDesat, p.shadowDesat);

    /* filmic tone curve */
    lin = pc_filmic_tone(lin, p.contrast, p.pivot, p.highlightRolloff, p.toe);

    /* print / stock cast */
    lin = pc_print(lin, p.filmWarmth, p.printDensity);

    /* output encode */
    PCrgb outc = pc_encode(lin, p.outputSpace);

    /* dry/wet mix against the ORIGINAL incoming signal */
    float m = pc_sat01(p.lookIntensity);
    PCrgb res;
    res.r = src.r + (outc.r - src.r)*m;
    res.g = src.g + (outc.g - src.g)*m;
    res.b = src.b + (outc.b - src.b)*m;
    return res;
}

#endif /* PONTUSCOLOR_KERNEL_H */
