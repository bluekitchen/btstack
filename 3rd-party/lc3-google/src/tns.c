/******************************************************************************
 *
 *  Copyright 2022 Google LLC
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

#include "tns.h"
#include "tables.h"


/* ----------------------------------------------------------------------------
 *  Filter Coefficients
 * -------------------------------------------------------------------------- */

/**
 * Resolve LPC Weighting indication according bitrate
 * dt, nbytes      Duration and size of the frame
 * return          True when LPC Weighting enabled
 */
static bool resolve_lpc_weighting(enum lc3_dt dt, int nbytes)
{
    return nbytes * 8 < 120 * (int)(1 + dt);
}

/**
 * Return dot product of 2 vectors
 * a, b, n         The 2 vectors of size `n`
 * return          sum( a[i] * b[i] ), i = [0..n-1]
 */
LC3_HOT static inline float dot(const float *a, const float *b, int n)
{
    float v = 0;

    while (n--)
        v += *(a++) * *(b++);

    return v;
}

/**
 * LPC Coefficients
 * dt, bw          Duration and bandwidth of the frame
 * maxorder        Maximum order of filter
 * x               Spectral coefficients
 * gain, a         Output the prediction gains and LPC coefficients
 */
LC3_HOT static void compute_lpc_coeffs(
    enum lc3_dt dt, enum lc3_bandwidth bw, int maxorder,
    const float *x, float *gain, float (*a)[9])
{

#if LC3_PLUS

    static const int sub_2m5_nb[]   = {  3,  10,  20 };
    static const int sub_2m5_wb[]   = {  3,  20,  40 };
    static const int sub_2m5_sswb[] = {  3,  30,  60 };
    static const int sub_2m5_swb[]  = {  3,  40,  80 };
    static const int sub_2m5_fb[]   = {  3,  51, 100 };

    static const int sub_5m_nb[]    = {  6,  23,  40 };
    static const int sub_5m_wb[]    = {  6,  43,  80 };
    static const int sub_5m_sswb[]  = {  6,  63, 120 };
    static const int sub_5m_swb[]   = {  6,  43,  80, 120, 160 };
    static const int sub_5m_fb[]    = {  6,  53, 100, 150, 200 };

#endif /* LC3_PLUS */

    static const int sub_7m5_nb[]   = {  9,  26,  43,  60 };
    static const int sub_7m5_wb[]   = {  9,  46,  83, 120 };
    static const int sub_7m5_sswb[] = {  9,  66, 123, 180 };
    static const int sub_7m5_swb[]  = {  9,  46,  82, 120, 159, 200, 240 };
    static const int sub_7m5_fb[]   = {  9,  56, 103, 150, 200, 250, 300 };

    static const int sub_10m_nb[]   = { 12,  34,  57,  80 };
    static const int sub_10m_wb[]   = { 12,  61, 110, 160 };
    static const int sub_10m_sswb[] = { 12,  88, 164, 240 };
    static const int sub_10m_swb[]  = { 12,  61, 110, 160, 213, 266, 320 };
    static const int sub_10m_fb[]   = { 12,  74, 137, 200, 266, 333, 400 };

    static const float lag_window[] = {
        1.00000000e+00, 9.98028026e-01, 9.92135406e-01, 9.82391584e-01,
        9.68910791e-01, 9.51849807e-01, 9.31404933e-01, 9.07808230e-01,
        8.81323137e-01
    };

    const int *sub = (const int * const [LC3_NUM_DT][LC3_NUM_BANDWIDTH]){

#if LC3_PLUS

        [LC3_DT_2M5] = {
          sub_2m5_nb, sub_2m5_wb, sub_2m5_sswb, sub_2m5_swb,
          sub_2m5_fb, sub_2m5_fb, sub_2m5_fb                },

        [LC3_DT_5M] = {
            sub_5m_nb , sub_5m_wb , sub_5m_sswb , sub_5m_swb ,
            sub_5m_fb , sub_5m_fb , sub_5m_fb                 },

#endif /* LC3_PLUS */

        [LC3_DT_7M5] = {
            sub_7m5_nb, sub_7m5_wb, sub_7m5_sswb, sub_7m5_swb,
            sub_7m5_fb                                        },

        [LC3_DT_10M] = {
            sub_10m_nb, sub_10m_wb, sub_10m_sswb, sub_10m_swb,
            sub_10m_fb, sub_10m_fb, sub_10m_fb                },

    }[dt][bw];

    /* --- Normalized autocorrelation --- */

    int nfilters = 1 + (dt >= LC3_DT_5M && bw >= LC3_BANDWIDTH_SWB);
    int nsubdivisions = 2 + (dt >= LC3_DT_7M5);

    const float *xs, *xe = x + *sub;
    float r[2][9];

    for (int f = 0; f < nfilters; f++) {
        float c[9][3] = { 0 };

        for (int s = 0; s < nsubdivisions; s++) {
            xs = xe, xe = x + *(++sub);

            for (int k = 0; k <= maxorder; k++)
                c[k][s] = dot(xs, xs + k, (xe - xs) - k);
        }

        r[f][0] = nsubdivisions;
        if (nsubdivisions == 2) {
            float e0 = c[0][0], e1 = c[0][1];
            for (int k = 1; k <= maxorder; k++)
                r[f][k] = e0 == 0 || e1 == 0 ? 0 :
                  (c[k][0]/e0 + c[k][1]/e1) * lag_window[k];

        } else {
            float e0 = c[0][0], e1 = c[0][1], e2 = c[0][2];
            for (int k = 1; k <= maxorder; k++)
                r[f][k] = e0 == 0 || e1 == 0 || e2 == 0 ? 0 :
                  (c[k][0]/e0 + c[k][1]/e1 + c[k][2]/e2) * lag_window[k];
        }
    }

    /* --- Levinson-Durbin recursion --- */

    for (int f = 0; f < nfilters; f++) {
        float *a0 = a[f], a1[9];
        float err = r[f][0], rc;

        gain[f] = err;

        a0[0] = 1;
        for (int k = 1; k <= maxorder; ) {

            rc = -r[f][k];
            for (int i = 1; i < k; i++)
                rc -= a0[i] * r[f][k-i];

            rc /= err;
            err *= 1 - rc * rc;

            for (int i = 1; i < k; i++)
                a1[i] = a0[i] + rc * a0[k-i];
            a1[k++] = rc;

            rc = -r[f][k];
            for (int i = 1; i < k; i++)
                rc -= a1[i] * r[f][k-i];

            rc /= err;
            err *= 1 - rc * rc;

            for (int i = 1; i < k; i++)
                a0[i] = a1[i] + rc * a1[k-i];
            a0[k++] = rc;
        }

        gain[f] /= err;
    }
}

/**
 * LPC Weighting
 * gain, a         Prediction gain and LPC coefficients, weighted as output
 */
LC3_HOT static void lpc_weighting(float pred_gain, float *a)
{
    float gamma = 1.f - (1.f - 0.85f) * (2.f - pred_gain) / (2.f - 1.5f);
    float g = 1.f;

    for (int i = 1; i < 9; i++)
        a[i] *= (g *= gamma);
}

/**
 * LPC reflection
 * a, maxorder     LPC coefficients, and maximum order (4 or 8)
 * rc              Output refelection coefficients
 */
LC3_HOT static void lpc_reflection(
    const float *a, int maxorder, float *rc)
{
    float e, b[2][7], *b0, *b1;

    rc[maxorder-1] = a[maxorder];
    e = 1 - rc[maxorder-1] * rc[maxorder-1];

    b1 = b[1];
    for (int i = 0; i < maxorder-1; i++)
        b1[i] = (a[1+i] - rc[maxorder-1] * a[(maxorder-1)-i]) / e;

    for (int k = maxorder-2; k > 0; k--) {
        b0 = b1, b1 = b[k & 1];

        rc[k] = b0[k];
        e = 1 - rc[k] * rc[k];

        for (int i = 0; i < k; i++)
            b1[i] = (b0[i] - rc[k] * b0[k-1-i]) / e;
    }

    rc[0] = b1[0];
}

/**
 * Quantization of RC coefficients
 * rc, maxorder    Refelection coefficients, and maximum order (4 or 8)
 * order           Return order of coefficients
 * rc_i            Return quantized coefficients
 */
static void quantize_rc(const float *rc, int maxorder, int *order, int *rc_q)
{
    /* Quantization table, sin(delta * (i + 0.5)), delta = Pi / 17,
     * rounded to fixed point Q15 value (LC3-Plus HR requirements). */

    static float q_thr[] = {
        0x0bcfp-15, 0x2307p-15, 0x390ep-15, 0x4d23p-15,
        0x5e98p-15, 0x6cd4p-15, 0x775bp-15, 0x7dd2p-15,
    };

    *order = maxorder;

    for (int i = 0; i < maxorder; i++) {
        float rc_m = fabsf(rc[i]);

        rc_q[i] = 4 * (rc_m >= q_thr[4]);
        for (int j = 0; j < 4 && rc_m >= q_thr[rc_q[i]]; j++, rc_q[i]++);

        if (rc[i] < 0)
            rc_q[i] = -rc_q[i];

        *order = rc_q[i] != 0 ? maxorder : *order - 1;
    }
}

/**
 * Unquantization of RC coefficients
 * rc_q, order     Quantized coefficients, and order
 * rc              Return refelection coefficients
 */
static void unquantize_rc(const int *rc_q, int order, float rc[8])
{
    /* Quantization table, sin(delta * i), delta = Pi / 17,
     * rounded to fixed point Q15 value (LC3-Plus HR requirements). */

    static float q_inv[] = {
        0x0000p-15, 0x1785p-15, 0x2e3dp-15, 0x4362p-15,
        0x563cp-15, 0x6625p-15, 0x7295p-15, 0x7b1dp-15, 0x7f74p-15,
    };

    int i;

    for (i = 0; i < order; i++) {
        float rc_m = q_inv[LC3_ABS(rc_q[i])];
        rc[i] = rc_q[i] < 0 ? -rc_m : rc_m;
    }
}


/* ----------------------------------------------------------------------------
 *  Filtering
 * -------------------------------------------------------------------------- */

/**
 * Forward filtering
 * dt, bw          Duration and bandwidth of the frame
 * rc_order, rc    Order of coefficients, and coefficients
 * x               Spectral coefficients, filtered as output
 */
LC3_HOT static void forward_filtering(
    enum lc3_dt dt, enum lc3_bandwidth bw,
    const int rc_order[2], float (* const rc)[8], float *x)
{
    int nfilters = 1 + (dt >= LC3_DT_5M && bw >= LC3_BANDWIDTH_SWB);
    int nf = lc3_ne(dt, (enum lc3_srate)LC3_MIN(bw, LC3_BANDWIDTH_FB))
                >> (nfilters - 1);
    int i0, ie = 3*(1 + dt);

    float s[8] = { 0 };

    for (int f = 0; f < nfilters; f++) {

        i0 = ie;
        ie = nf * (1 + f);

        if (!rc_order[f])
            continue;

        for (int i = i0; i < ie; i++) {
            float xi = x[i];
            float s0, s1 = xi;

            for (int k = 0; k < rc_order[f]; k++) {
                s0 = s[k];
                s[k] = s1;

                s1  = rc[f][k] * xi + s0;
                xi += rc[f][k] * s0;
            }

            x[i] = xi;
        }
    }
}

/**
 * Inverse filtering
 * dt, bw          Duration and bandwidth of the frame
 * rc_order, rc    Order of coefficients, and unquantized coefficients
 * x               Spectral coefficients, filtered as output
 */
LC3_HOT static void inverse_filtering(
    enum lc3_dt dt, enum lc3_bandwidth bw,
    const int rc_order[2], float (* const rc)[8], float *x)
{
    int nfilters = 1 + (dt >= LC3_DT_5M && bw >= LC3_BANDWIDTH_SWB);
    int nf = lc3_ne(dt, (enum lc3_srate)LC3_MIN(bw, LC3_BANDWIDTH_FB))
                >> (nfilters - 1);
    int i0, ie = 3*(1 + dt);

    float s[8] = { 0 };

    for (int f = 0; f < nfilters; f++) {

        i0 = ie;
        ie = nf * (1 + f);

        if (!rc_order[f])
            continue;

        for (int i = i0; i < ie; i++) {
            float xi = x[i];

            xi -= s[7] * rc[f][7];
            for (int k = 6; k >= 0; k--) {
                xi -= s[k] * rc[f][k];
                s[k+1] = s[k] + rc[f][k] * xi;
            }
            s[0] = xi;
            x[i] = xi;
        }

        for (int k = 7; k >= rc_order[f]; k--)
            s[k] = 0;
    }
}


/* ----------------------------------------------------------------------------
 *  Interface
 * -------------------------------------------------------------------------- */

/**
 * TNS analysis
 */
void lc3_tns_analyze(enum lc3_dt dt, enum lc3_bandwidth bw,
    bool nn_flag, int nbytes, struct lc3_tns_data *data, float *x)
{
    /* Processing steps :
     * - Determine the LPC (Linear Predictive Coding) Coefficients
     * - Check is the filtering is disabled
     * - The coefficients are weighted on low bitrates and predicition gain
     * - Convert to reflection coefficients and quantize
     * - Finally filter the spectral coefficients */

    float pred_gain[2], a[2][9];
    float rc[2][8];

    data->lpc_weighting = resolve_lpc_weighting(dt, nbytes);
    data->nfilters = 1 + (dt >= LC3_DT_5M && bw >= LC3_BANDWIDTH_SWB);
    int maxorder = dt <= LC3_DT_5M ? 4 : 8;

    compute_lpc_coeffs(dt, bw, maxorder, x, pred_gain, a);

    for (int f = 0; f < data->nfilters; f++) {

        data->rc_order[f] = 0;
        if (nn_flag || pred_gain[f] <= 1.5f)
            continue;

        if (data->lpc_weighting && pred_gain[f] < 2.f)
            lpc_weighting(pred_gain[f], a[f]);

        lpc_reflection(a[f], maxorder, rc[f]);

        quantize_rc(rc[f], maxorder, &data->rc_order[f], data->rc[f]);
        unquantize_rc(data->rc[f], data->rc_order[f], rc[f]);
    }

    forward_filtering(dt, bw, data->rc_order, rc, x);
}

/**
 * TNS synthesis
 */
void lc3_tns_synthesize(enum lc3_dt dt, enum lc3_bandwidth bw,
    const struct lc3_tns_data *data, float *x)
{
    float rc[2][8] = { 0 };

    for (int f = 0; f < data->nfilters; f++)
        if (data->rc_order[f])
            unquantize_rc(data->rc[f], data->rc_order[f], rc[f]);

    inverse_filtering(dt, bw, data->rc_order, rc, x);
}

/**
 * Bit consumption of bitstream data
 */
int lc3_tns_get_nbits(const struct lc3_tns_data *data)
{
    int nbits = 0;

    for (int f = 0; f < data->nfilters; f++) {

        int nbits_2048 = 2048;
        int rc_order = data->rc_order[f];

        nbits_2048 += rc_order > 0 ? lc3_tns_order_bits
            [data->lpc_weighting][rc_order-1] : 0;

        for (int i = 0; i < rc_order; i++)
            nbits_2048 += lc3_tns_coeffs_bits[i][8 + data->rc[f][i]];

        nbits += (nbits_2048 + (1 << 11) - 1) >> 11;
    }

    return nbits;
}

/**
 * Put bitstream data
 */
void lc3_tns_put_data(lc3_bits_t *bits, const struct lc3_tns_data *data)
{
    for (int f = 0; f < data->nfilters; f++) {
        int rc_order = data->rc_order[f];

        lc3_put_bits(bits, rc_order > 0, 1);
        if (rc_order <= 0)
            continue;

        lc3_put_symbol(bits,
            lc3_tns_order_models + data->lpc_weighting, rc_order-1);

        for (int i = 0; i < rc_order; i++)
            lc3_put_symbol(bits,
                lc3_tns_coeffs_models + i, 8 + data->rc[f][i]);
    }
}

/**
 * Get bitstream data
 */
int lc3_tns_get_data(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_bandwidth bw, int nbytes, lc3_tns_data_t *data)
{
    data->nfilters = 1 + (dt >= LC3_DT_5M && bw >= LC3_BANDWIDTH_SWB);
    data->lpc_weighting = resolve_lpc_weighting(dt, nbytes);

    for (int f = 0; f < data->nfilters; f++) {

        data->rc_order[f] = lc3_get_bit(bits);
        if (!data->rc_order[f])
            continue;

        data->rc_order[f] += lc3_get_symbol(bits,
            lc3_tns_order_models + data->lpc_weighting);
        if (dt <= LC3_DT_5M && data->rc_order[f] > 4)
            return -1;

        for (int i = 0; i < data->rc_order[f]; i++)
            data->rc[f][i] = (int)lc3_get_symbol(bits,
                lc3_tns_coeffs_models + i) - 8;
    }

    return 0;
}
