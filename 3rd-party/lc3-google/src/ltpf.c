/******************************************************************************
 *
 *  Copyright 2021 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "ltpf.h"
#include "tables.h"


/* ----------------------------------------------------------------------------
 *  Resampling
 * -------------------------------------------------------------------------- */

/**
 * Resample to 12.8 KHz (cf. 3.3.9.3-4) Template
 * sr              Samplerate source of the frame
 * hp50            State of the High-Pass 50 Hz filter
 * x               [-d..-1] Previous, [0..ns-1] Current samples
 * y, n            [0..n-1] Output `n` processed samples
 *
 * The number of previous samples `d` accessed on `x` is :
 *   d: { 10, 20, 30, 40, 60 } - 1 for samplerates from 8KHz to 48KHz
 */
static inline void resample_12k8_template(const enum lc3_srate sr,
    struct lc3_ltpf_hp50_state *hp50, const float *x, float *y, int n)
{
    /* --- Parameters  ---
     * p: Resampling factor, from 4 to 24
     * w: Half width of polyphase filter
     *
     * bn, an: High-Pass Biquad coefficients,
     * with `bn` support of rescaling resampling factor.
     * Note that it's an High-Pass filter, so we have `b0 = b2`,
     * in the following steps we use `b0` as `b2`. */

    const int p = 192 / LC3_SRATE_KHZ(sr);
    const int w = 5 * LC3_SRATE_KHZ(sr) / 8;

    const int b_scale = p >> (sr == LC3_SRATE_8K);
    const float a1 = -1.965293373, b1 = -1.965589417 * b_scale;
    const float a2 =  0.965885461, b2 =  0.982794708 * b_scale;

    /* --- Resampling ---
     * The value `15*8 * n` is divisible by all resampling factors `p`,
     * integer and fractionnal position can be determined at compilation
     * time while unrolling the loops by 8 samples.
     * The biquad filter implementation chosen in the `Direct Form 2`. */

    const float *h = lc3_ltpf_h12k8 + 119;
    x -= w;

    for (int i = 0; i < n; i += 8, x += 120/p)
        for (int j = 0; j < 15*8; j += 15) {
            float un, yn;
            int e, f, k;

            e = j / p, f = j % p;
            for (un = 0, k = 1-w; k <= w; k++)
                un += x[e+k] * h[k*p - f];

            yn = b2 * un + hp50->s1;
            hp50->s1 = b1 * un - a1 * yn + hp50->s2;
            hp50->s2 = b2 * un - a2 * yn;
            *(y++) = yn;
        }
}

/**
 * LTPF Resample to 12.8 KHz implementations for each samplerates
 */

static void resample_8k_12k8(
    struct lc3_ltpf_hp50_state *hp50, const float *x, float *y, int n)
{
    resample_12k8_template(LC3_SRATE_8K, hp50, x, y, n);
}

static void resample_16k_12k8(
    struct lc3_ltpf_hp50_state *hp50, const float *x, float *y, int n)
{
    resample_12k8_template(LC3_SRATE_16K, hp50, x, y, n);
}

static void resample_24k_12k8(
    struct lc3_ltpf_hp50_state *hp50, const float *x, float *y, int n)
{
    resample_12k8_template(LC3_SRATE_24K, hp50, x, y, n);
}

static void resample_32k_12k8(
    struct lc3_ltpf_hp50_state *hp50, const float *x, float *y, int n)
{
    resample_12k8_template(LC3_SRATE_32K, hp50, x, y, n);
}

static void resample_48k_12k8(
    struct lc3_ltpf_hp50_state *hp50, const float *x, float *y, int n)
{
    resample_12k8_template(LC3_SRATE_48K, hp50, x, y, n);
}

static void (* const resample_12k8[])
    (struct lc3_ltpf_hp50_state *, const float *, float *, int ) =
{
    [LC3_SRATE_8K ] = resample_8k_12k8,
    [LC3_SRATE_16K] = resample_16k_12k8,
    [LC3_SRATE_24K] = resample_24k_12k8,
    [LC3_SRATE_32K] = resample_32k_12k8,
    [LC3_SRATE_48K] = resample_48k_12k8,
};

/**
 * Resample to 6.4 KHz (cf. 3.3.9.3-4)
 * x               [-3..-1] Previous, [0..n-1] Current samples
 * y, n            [0..n-1] Output `n` processed samples
 */
static void resample_6k4(const float *x, float *y, int n)
{
    static const float h[] = { 0.2819382921, 0.2353512128, 0.1236796411 };
    float xn2 = x[-3], xn1 = x[-2], x0 = x[-1], x1, x2;

    for (const float *ye = y + n; y < ye; xn2 = x0, xn1 = x1, x0 = x2) {
        x1 = *(x++); x2 = *(x++);

        *(y++) = x0 * h[0] + (xn1 + x1) * h[1] + (xn2 + x2) * h[2];
    }
}


/* ----------------------------------------------------------------------------
 *  Analysis
 * -------------------------------------------------------------------------- */

/**
 * Return dot product of 2 vectors
 * a, b, n         The 2 vectors of size `n`
 * return          sum( a[i] * b[i] ), i = [0..n-1]
 */
static inline float dot(const float *a, const float *b, int n)
{
    float v = 0;

    while (n--)
        v += *(a++) * *(b++);

    return v;
}

/**
 * Return vector of correlations
 * a, b, n         The 2 vector of size `n` to correlate
 * y, nc           Output the correlation vector of size `nc`
 *
 * The size `n` of input vectors must be multiple of 16
 */
static void correlate(
    const float *a, const float *b, int n, float *y, int nc)
{
    for (const float *ye = y + nc; y < ye; )
        *(y++) = dot(a, b--, n);
}

/**
 * Search the maximum value and returns its argument
 * x, n            The input vector of size `n`
 * x_max           Return the maximum value
 * return          Return the argument of the maximum
 */
static int argmax(const float *x, int n, float *x_max)
{
    int arg = 0;

    *x_max = x[arg = 0];
    for (int i = 1; i < n; i++)
        if (*x_max < x[i])
            *x_max = x[arg = i];

    return arg;
}

/**
 * Search the maximum weithed value and returns its argument
 * x, n            The input vector of size `n`
 * w_incr          Increment of the weight
 * x_max, xw_max   Return the maximum not weighted value
 * return          Return the argument of the weigthed maximum
 */
static int argmax_weighted(
    const float *x, int n, float w_incr, float *x_max)
{
    int arg;

    float xw_max = (*x_max = x[arg = 0]);
    float w = 1 + w_incr;

    for (int i = 1; i < n; i++, w += w_incr)
        if (xw_max < x[i] * w)
            xw_max = (*x_max = x[arg = i]) * w;

    return arg;
}

/**
 * Interpolate from pitch detected value (3.3.9.8)
 * x, n            [-2..-1] Previous, [0..n] Current input
 * d               The phase of interpolation (0 to 3)
 * return          The interpolated vector
 *
 * The size `n` of vectors must be multiple of 4
 */
static void interpolate(const float *x, int n, int d, float *y)
{
    static const float h4[][8] = {
        { 2.09880463e-01, 5.83527575e-01, 2.09880463e-01                 },
        { 1.06999186e-01, 5.50075002e-01, 3.35690625e-01, 6.69885837e-03 },
        { 3.96711478e-02, 4.59220930e-01, 4.59220930e-01, 3.96711478e-02 },
        { 6.69885837e-03, 3.35690625e-01, 5.50075002e-01, 1.06999186e-01 },
    };

    const float *h = h4[d];
    float x3 = x[-2], x2 = x[-1], x1, x0;

    x1 = (*x++);
    for (const float *ye = y + n; y < ye; ) {
        *(y++) = (x0 = *(x++)) * h[0] + x1 * h[1] + x2 * h[2] + x3 * h[3];
        *(y++) = (x3 = *(x++)) * h[0] + x0 * h[1] + x1 * h[2] + x2 * h[3];
        *(y++) = (x2 = *(x++)) * h[0] + x3 * h[1] + x0 * h[2] + x1 * h[3];
        *(y++) = (x1 = *(x++)) * h[0] + x2 * h[1] + x3 * h[2] + x0 * h[3];
    }
}

/**
 * Interpolate autocorrelation (3.3.9.7)
 * x               [-4..-1] Previous, [0..4] Current input
 * d               The phase of interpolation (-3 to 3)
 * return          The interpolated value
 */
static float interpolate_4(const float *x, int d)
{
    static const float h4[][8] = {
        {  1.53572770e-02, -4.72963246e-02,  8.35788573e-02,  8.98638285e-01,
           8.35788573e-02, -4.72963246e-02,  1.53572770e-02,                 },
        {  2.74547165e-03,  4.59833449e-03, -7.54404636e-02,  8.17488686e-01,
           3.30182571e-01, -1.05835916e-01,  2.86823405e-02, -2.87456116e-03 },
        { -3.00125103e-03,  2.95038503e-02, -1.30305021e-01,  6.03297008e-01,
           6.03297008e-01, -1.30305021e-01,  2.95038503e-02, -3.00125103e-03 },
        { -2.87456116e-03,  2.86823405e-02, -1.05835916e-01,  3.30182571e-01,
           8.17488686e-01, -7.54404636e-02,  4.59833449e-03,  2.74547165e-03 },
    };

    const float *h = h4[(4+d) % 4];

    float y = d < 0 ? x[-4] * *(h++) :
              d > 0 ? x[ 4] * *(h+7) : 0;

    y += x[-3] * h[0] + x[-2] * h[1] + x[-1] * h[2] + x[0] * h[3] +
         x[ 1] * h[4] + x[ 2] * h[5] + x[ 3] * h[6];

    return y;
}

/**
 * Pitch detection algorithm (3.3.9.5-6)
 * ltpf            Context of analysis
 * x, n            [-114..-17] Previous, [0..n-1] Current 6.4KHz samples
 * tc              Return the pitch-lag estimation
 * return          True when pitch present
 */
static bool detect_pitch(
    struct lc3_ltpf_analysis *ltpf, const float *x, int n, int *tc)
{
    float rm1, rm2;
    float r[98];

    const int r0 = 17, nr = 98;
    int k0 = LC3_MAX(   0, ltpf->tc-4);
    int nk = LC3_MIN(nr-1, ltpf->tc+4) - k0 + 1;

    correlate(x, x - r0, n, r, nr);

    int t1 = argmax_weighted(r, nr, -.5/(nr-1), &rm1);
    int t2 = k0 + argmax(r + k0, nk, &rm2);

    const float *x1 = x - (r0 + t1);
    const float *x2 = x - (r0 + t2);

    float nc1 = rm1 <= 0 ? 0 :
        rm1 / sqrtf(dot(x, x, n) * dot(x1, x1, n));

    float nc2 = rm2 <= 0 ? 0 :
        rm2 / sqrtf(dot(x, x, n) * dot(x2, x2, n));

    int t1sel = nc2 <= 0.85 * nc1;
    ltpf->tc = (t1sel ? t1 : t2);

    *tc = r0 + ltpf->tc;
    return (t1sel ? nc1 : nc2) > 0.6;
}

/**
 * Pitch-lag parameter (3.3.9.7)
 * x, n            [-232..-28] Previous, [0..n-1] Current 12.8KHz samples
 * tc              Pitch-lag estimation
 * pitch           The pitch value, in fixed .4
 * return          The bitstream pitch index value
 */
static int refine_pitch(const float *x, int n, int tc, int *pitch)
{
    float r[17], rm;
    int e, f;

    int r0 = LC3_MAX( 32, 2*tc - 4);
    int nr = LC3_MIN(228, 2*tc + 4) - r0 + 1;

    correlate(x, x - (r0 - 4), n, r, nr + 8);

    e = r0 + argmax(r + 4, nr, &rm);
    const float *re = r + (e - (r0 - 4));

    float dm = interpolate_4(re, f = 0);
    for (int i = 1; i <= 3; i++) {
        float d;

        if (e >= 127 && ((i & 1) | (e >= 157)))
            continue;

        if ((d = interpolate_4(re, i)) > dm)
            dm = d, f = i;

        if (e > 32 && (d = interpolate_4(re, -i)) > dm)
            dm = d, f = -i;
    }

    e -=   (f < 0);
    f += 4*(f < 0);

    *pitch = 4*e + f;
    return e < 127 ? 4*e +  f       - 128 :
           e < 157 ? 2*e + (f >> 1) + 126 : e + 283;
}

/**
 * LTPF Analysis
 */
bool lc3_ltpf_analyse(enum lc3_dt dt, enum lc3_srate sr,
    struct lc3_ltpf_analysis *ltpf, const float *x, struct lc3_ltpf_data *data)
{
    /* --- Resampling to 12.8 KHz --- */

    int z_12k8 = sizeof(ltpf->x_12k8) / sizeof(float);
    int n_12k8 = dt == LC3_DT_7M5 ? 96 : 128;

    memmove(ltpf->x_12k8, ltpf->x_12k8 + n_12k8,
        (z_12k8 - n_12k8) * sizeof(float));

    float *x_12k8 = ltpf->x_12k8 + (z_12k8 - n_12k8);
    resample_12k8[sr](&ltpf->hp50, x, x_12k8, n_12k8);

    x_12k8 -= (dt == LC3_DT_7M5 ? 44 :  24);

    /* --- Resampling to 6.4 KHz --- */

    int z_6k4 = sizeof(ltpf->x_6k4) / sizeof(float);
    int n_6k4 = n_12k8 >> 1;

    memmove(ltpf->x_6k4, ltpf->x_6k4 + n_6k4,
        (z_6k4 - n_6k4) * sizeof(float));

    float *x_6k4 = ltpf->x_6k4 + (z_6k4 - n_6k4);
    resample_6k4(x_12k8, x_6k4, n_6k4);

    /* --- Pitch detection --- */

    int tc, pitch = 0;
    float nc = 0;

    bool pitch_present = detect_pitch(ltpf, x_6k4, n_6k4, &tc);

    if (pitch_present) {
        float u[n_12k8], v[n_12k8];

        data->pitch_index = refine_pitch(x_12k8, n_12k8, tc, &pitch);

        interpolate(x_12k8, n_12k8, 0, u);
        interpolate(x_12k8 - (pitch >> 2), n_12k8, pitch & 3, v);

        nc = dot(u, v, n_12k8) / sqrtf(dot(u, u, n_12k8) * dot(v, v, n_12k8));
    }

    /* --- Activation --- */

     if (ltpf->active) {
        int pitch_diff =
            LC3_MAX(pitch, ltpf->pitch) - LC3_MIN(pitch, ltpf->pitch);
        float nc_diff = nc - ltpf->nc[0];

        data->active = pitch_present &&
            ((nc > 0.9) || (nc > 0.84 && pitch_diff < 8 && nc_diff > -0.1));

     } else {
         data->active = pitch_present &&
            ( (dt == LC3_DT_10M || ltpf->nc[1] > 0.94) &&
              (ltpf->nc[0] > 0.94 && nc > 0.94) );
     }

     ltpf->active = data->active;
     ltpf->pitch = pitch;
     ltpf->nc[1] = ltpf->nc[0];
     ltpf->nc[0] = nc;

     return pitch_present;
}


/* ----------------------------------------------------------------------------
 *  Synthesis
 * -------------------------------------------------------------------------- */

/**
 * Synthesis filter template
 * ym              [-w/2..0] Previous, [0..w-1] Current pitch samples
 * xm              w-1 previous input samples
 * x, n            Current samples as input, filtered as output
 * c, w            Coefficients by pair (num, den), and count of pairs
 * fade            Fading mode of filter  -1: Out  1: In  0: None
 */
static inline void synthesize_template(const float *ym, const float *xm,
    float *x, int n, const float (*c)[2], const int w, int fade)
{
    float g = (float)(fade <= 0);
    float g_incr = (float)((fade > 0) - (fade < 0)) / n;
    float u[w];
    int i;

    ym -= (w >> 1);

    /* --- Load previous samples --- */

    for (i = 1-w; i < 0; i++) {
        float xi = *(xm++), yi = *(ym++);

        u[i + w-1] = 0;
        for (int k = w-1; k+i >= 0; k--)
            u[i+k] += xi * c[k][0] - yi * c[k][1];
    }

    u[w-1] = 0;

    /* --- Process --- */

    for (; i < n; i += w) {

        for (int j = 0; j < w; j++, g += g_incr) {
            float xi = *x, yi = *(ym++);

            for (int k = w-1; k >= 0; k--)
                u[(j+k)%w] += xi * c[k][0] - yi * c[k][1];

            *(x++) = xi - g * u[j];
            u[j] = 0;
        }
    }
}

/**
 * Synthesis filter for each samplerates (width of filter)
 */

static void synthesize_4(const float *ym, const float *xm,
    float *x, int n, const float (*c)[2], int fade)
{
    synthesize_template(ym, xm, x, n, c, 4, fade);
}

static void synthesize_6(const float *ym, const float *xm,
    float *x, int n, const float (*c)[2], int fade)
{
    synthesize_template(ym, xm, x, n, c, 6, fade);
}

static void synthesize_8(const float *ym, const float *xm,
    float *x, int n, const float (*c)[2], int fade)
{
    synthesize_template(ym, xm, x, n, c, 8, fade);
}

static void synthesize_12(const float *ym, const float *xm,
    float *x, int n, const float (*c)[2], int fade)
{
    synthesize_template(ym, xm, x, n, c, 12, fade);
}

static void (* const synthesize[])(
    const float *, const float *, float *, int, const float (*)[2], int) =
{
    [LC3_SRATE_8K ] = synthesize_4,
    [LC3_SRATE_16K] = synthesize_4,
    [LC3_SRATE_24K] = synthesize_6,
    [LC3_SRATE_32K] = synthesize_8,
    [LC3_SRATE_48K] = synthesize_12,
};

/**
 * LTPF Synthesis
 */
void lc3_ltpf_synthesize(enum lc3_dt dt, enum lc3_srate sr,
    int nbytes, struct lc3_ltpf_synthesis *ltpf,
    const struct lc3_ltpf_data *data, float *x)
{
    int dt_us = LC3_DT_US(dt);

    /* --- Filter parameters --- */

    int p_idx = data ? data->pitch_index : 0;
    int pitch =
        p_idx >= 440 ? (((p_idx     ) - 283) << 2)  :
        p_idx >= 380 ? (((p_idx >> 1) -  63) << 2) + (((p_idx  & 1)) << 1) :
                       (((p_idx >> 2) +  32) << 2) + (((p_idx  & 3)) << 0)  ;

    pitch = (pitch * LC3_SRATE_KHZ(sr) * 10 + 64) / 128;

    int nbits = (nbytes*8 * 10000 + (dt_us/2)) / dt_us;
    int g_idx = LC3_MAX(nbits / 80, 3 + (int)sr) - (3 + sr);
    bool active = data && data->active && g_idx < 4;

    int w = LC3_MAX(4, LC3_SRATE_KHZ(sr) / 4);
    float c[w][2];

    for (int i = 0; i < w; i++) {
        float g = active ? 0.4f - 0.05f * g_idx : 0;

        c[i][0] = active ? 0.85f * g * lc3_ltpf_cnum[sr][g_idx][i] : 0;
        c[i][1] = active ? g * lc3_ltpf_cden[sr][pitch & 3][i] : 0;
    }

    /* --- Transition handling --- */

    int ns = LC3_NS(dt, sr);
    int nt = ns / (4 - (dt == LC3_DT_7M5));
    float xm[12];

    if (active)
        memcpy(xm, x + nt-(w-1), (w-1) * sizeof(float));

    if (!ltpf->active && active)
        synthesize[sr](x - pitch/4, ltpf->x, x, nt, c, 1);
    else if (ltpf->active && !active)
        synthesize[sr](x - ltpf->pitch/4, ltpf->x, x, nt, ltpf->c, -1);
    else if (ltpf->active && active && ltpf->pitch == pitch)
        synthesize[sr](x - pitch/4, ltpf->x, x, nt, c, 0);
    else if (ltpf->active && active) {
        synthesize[sr](x - ltpf->pitch/4, ltpf->x, x, nt, ltpf->c, -1);
        synthesize[sr](x - pitch/4, x - (w-1), x, nt, c, 1);
    }

    /* --- Remainder --- */

    memcpy(ltpf->x, x + ns-(w-1), (w-1) * sizeof(float));

    if (active)
        synthesize[sr](x - pitch/4 + nt, xm, x + nt, ns-nt, c, 0);

    /* --- Update state --- */

    ltpf->active = active;
    ltpf->pitch = pitch;
    memcpy(ltpf->c, c, w * sizeof(ltpf->c[0]));
}


/* ----------------------------------------------------------------------------
 *  Bitstream data
 * -------------------------------------------------------------------------- */

/**
 * LTPF disable
 */
void lc3_ltpf_disable(struct lc3_ltpf_data *data)
{
    data->active = false;
}

/**
 * Return number of bits coding the bitstream data
 */
int lc3_ltpf_get_nbits(bool pitch)
{
    return 1 + 10 * pitch;
}

/**
 * Put bitstream data
 */
void lc3_ltpf_put_data(lc3_bits_t *bits,
    const struct lc3_ltpf_data *data)
{
    lc3_put_bit(bits, data->active);
    lc3_put_bits(bits, data->pitch_index, 9);
}

/**
 * Get bitstream data
 */
void lc3_ltpf_get_data(lc3_bits_t *bits, struct lc3_ltpf_data *data)
{
    data->active = lc3_get_bit(bits);
    data->pitch_index = lc3_get_bits(bits, 9);
}
