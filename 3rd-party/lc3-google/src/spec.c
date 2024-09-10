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

#include "spec.h"
#include "bits.h"
#include "tables.h"


/* ----------------------------------------------------------------------------
 *  Global Gain / Quantization
 * -------------------------------------------------------------------------- */

/**
 * Resolve quantized gain index offset
 * sr, nbytes      Samplerate and size of the frame
 * return          Gain index offset
 */
static int resolve_gain_offset(enum lc3_srate sr, int nbytes)
{
    int sr_ind = lc3_hr(sr) ? 4 + (sr - LC3_SRATE_48K_HR) : sr;

    int g_off = (nbytes * 8) / (10 * (1 + sr_ind));
    return LC3_MIN(sr >= LC3_SRATE_96K_HR ? 181 : 255,
        105 + 5*(1 + sr_ind) + LC3_MIN(g_off, 115));
}

/**
 * Unquantize gain
 * g_int           Quantization gain value
 * return          Unquantized gain value
 */
static float unquantize_gain(int g_int)
{
    /* Unquantization gain table :
     * G[i] = 10 ^ (i / 28) , i = [0..27] */

    static const float iq_table[] = {
        1.00000000e+00, 1.08571112e+00, 1.17876863e+00, 1.27980221e+00,
        1.38949549e+00, 1.50859071e+00, 1.63789371e+00, 1.77827941e+00,
        1.93069773e+00, 2.09617999e+00, 2.27584593e+00, 2.47091123e+00,
        2.68269580e+00, 2.91263265e+00, 3.16227766e+00, 3.43332002e+00,
        3.72759372e+00, 4.04708995e+00, 4.39397056e+00, 4.77058270e+00,
        5.17947468e+00, 5.62341325e+00, 6.10540230e+00, 6.62870316e+00,
        7.19685673e+00, 7.81370738e+00, 8.48342898e+00, 9.21055318e+00
    };

    float g = 1.f;

    for ( ; g_int <   0; g_int += 28, g *= 0.1f);
    for ( ; g_int >= 28; g_int -= 28, g *= 10.f);

    return g * iq_table[g_int];
}

/**
 * Global Gain Estimation
 * dt, sr          Duration and samplerate of the frame
 * x               Spectral coefficients
 * nbytes          Size of the frame
 * nbits_budget    Number of bits available coding the spectrum
 * nbits_off       Offset on the available bits, temporarily smoothed
 * g_off           Gain index offset
 * reset_off       Return True when the nbits_off must be reset
 * g_min           Return lower bound of quantized gain value
 * return          The quantized gain value
 */
LC3_HOT static int estimate_gain(
    enum lc3_dt dt, enum lc3_srate sr, const float *x,
    int nbytes, int nbits_budget, float nbits_off, int g_off,
    bool *reset_off, int *g_min)
{
    int n4 = lc3_ne(dt, sr) / 4;
    union { float f; int32_t q16; } e[LC3_MAX_NE / 4];

    /* --- Signal adaptative noise floor --- */

    int reg_bits = 0;
    float low_bits = 0;

    if (lc3_hr(sr)) {
        int reg_c = (const int [LC3_NUM_DT][LC3_NUM_SRATE - LC3_SRATE_48K_HR]){
            [LC3_DT_2M5] = { -6, -6 },
            [LC3_DT_5M ] = {  0,  0 },
            [LC3_DT_10M] = {  2,  5 }
        }[dt][sr - LC3_SRATE_48K_HR];

        reg_bits = (8*nbytes * 4) / (125 * (1 + dt));
        reg_bits = LC3_CLIP(reg_bits + reg_c, 6, 23);

        float m0 = 1e-5f, m1 = 1e-5f, k = 0;

        for (int i = 0; i < n4; i++) {
            m0 += fabsf(x[4*i + 0]),  m1 += fabsf(x[4*i + 0]) * k++;
            m0 += fabsf(x[4*i + 1]),  m1 += fabsf(x[4*i + 1]) * k++;
            m0 += fabsf(x[4*i + 2]),  m1 += fabsf(x[4*i + 2]) * k++;
            m0 += fabsf(x[4*i + 3]),  m1 += fabsf(x[4*i + 3]) * k++;
        }

        int m = roundf((1.6f * m0) / ((1 + dt) * m1));
        low_bits = 8 - LC3_MIN(m, 8);
    }

    /* --- Energy (dB) by 4 MDCT blocks --- */

    float x2_max = 0;

    for (int i = 0; i < n4; i++) {
        float x0 = x[4*i + 0] * x[4*i + 0];
        float x1 = x[4*i + 1] * x[4*i + 1];
        float x2 = x[4*i + 2] * x[4*i + 2];
        float x3 = x[4*i + 3] * x[4*i + 3];

        x2_max = fmaxf(x2_max, x0);
        x2_max = fmaxf(x2_max, x1);
        x2_max = fmaxf(x2_max, x2);
        x2_max = fmaxf(x2_max, x3);

        e[i].f = x0 + x1 + x2 + x3;
    }

    float x_max = sqrtf(x2_max);
    float nf = lc3_hr(sr) ?
        lc3_ldexpf(x_max, -reg_bits) * lc3_exp2f(-low_bits) : 0;

    for (int i = 0; i < n4; i++)
        e[i].q16 = lc3_db_q16(fmaxf(e[i].f + nf, 1e-10f));

    /* --- Determine gain index --- */

    int nbits = nbits_budget + nbits_off + 0.5f;
    int g_int = 255 - g_off;

    const int k_20_28 = 20.f/28 * 0x1p16f + 0.5f;
    const int k_2u7 = 2.7f * 0x1p16f + 0.5f;
    const int k_1u4 = 1.4f * 0x1p16f + 0.5f;

    for (int i = 128, j, j0 = n4-1, j1 ; i > 0; i >>= 1) {
        int gn = (g_int - i) * k_20_28;
        int v = 0;

        for (j = j0; j >= 0 && e[j].q16 < gn; j--);

        for (j1 = j; j >= 0; j--) {
            int e_diff = e[j].q16 - gn;

            v += e_diff < 0 ? k_2u7 :
                 e_diff < 43 << 16 ?   e_diff + ( 7 << 16)
                                   : 2*e_diff - (36 << 16);
        }

        if (v > nbits * k_1u4)
            j0 = j1;
        else
            g_int = g_int - i;
    }

    /* --- Limit gain index --- */

    float x_lim = lc3_hr(sr) ? 0x7fffp8f : 0x7fffp0f;

    *g_min = 255 - g_off;
    for (int i = 128 ; i > 0; i >>= 1)
        if (x_lim * unquantize_gain(*g_min - i) > x_max)
            *g_min -= i;

    *reset_off = g_int < *g_min || x_max == 0;
    if (*reset_off)
        g_int = *g_min;

    return g_int;
}

/**
 * Global Gain Adjustment
 * dt, sr          Duration and samplerate of the frame
 * g_idx           The estimated quantized gain index
 * nbits           Computed number of bits coding the spectrum
 * nbits_budget    Number of bits available for coding the spectrum
 * g_idx_min       Minimum gain index value
 * return          Gain adjust value (-1 to 2)
 */
LC3_HOT static int adjust_gain(
    enum lc3_dt dt, enum lc3_srate sr,
    int g_idx, int nbits, int nbits_budget, int g_idx_min)
{
    /* --- Compute delta threshold --- */

    const int *t = (const int [LC3_NUM_SRATE][3]){
        {  80,  500,  850 }, { 230, 1025, 1700 }, { 380, 1550, 2550 },
        { 530, 2075, 3400 }, { 680, 2600, 4250 },
        { 680, 2600, 4250 }, { 830, 3125, 5100 }
    }[sr];

    int delta, den = 48;

    if (nbits < t[0]) {
        delta = 3*(nbits + 48);

    } else if (nbits < t[1]) {
        int n0 = 3*(t[0] + 48), range = t[1] - t[0];
        delta = n0 * range + (nbits - t[0]) * (t[1] - n0);
        den *= range;

    } else {
        delta = LC3_MIN(nbits, t[2]);
    }

    delta = (delta + den/2) / den;

    /* --- Adjust gain --- */

    if (lc3_hr(sr) && nbits > nbits_budget) {
        int factor = 1 + (dt <= LC3_DT_5M) +
            (dt <= LC3_DT_2M5) * (1 + (nbits >= 520));

        int g_incr = factor + (factor * (nbits - nbits_budget)) / delta;
        return LC3_MIN(g_idx + g_incr, 255) - g_idx;
    }

    if (!lc3_hr(sr) && nbits < nbits_budget - (delta + 2))
        return -(g_idx > g_idx_min);

    if (!lc3_hr(sr) && nbits > nbits_budget)
        return (g_idx < 255) + (g_idx < 254 && nbits >= nbits_budget + delta);

    return 0;
}

/**
 * Spectrum quantization
 * dt, sr          Duration and samplerate of the frame
 * g_int           Quantization gain value
 * x               Spectral coefficients, scaled as output
 * n               Return count of significants
 */
LC3_HOT static void quantize(
    enum lc3_dt dt, enum lc3_srate sr, int g_int, float *x, int *n)
{
    float g_inv = unquantize_gain(-g_int);
    int ne = lc3_ne(dt, sr);

    *n = ne;

    for (int i = 0; i < ne; i += 2) {
        float xq_min = lc3_hr(sr) ? 0.5f : 10.f/16;

        x[i+0] *= g_inv;
        x[i+1] *= g_inv;

        *n = fabsf(x[i+0]) >= xq_min ||
             fabsf(x[i+1]) >= xq_min   ? ne : *n - 2;
    }
}

/**
 * Spectrum quantization inverse
 * dt, sr          Duration and samplerate of the frame
 * g_int           Quantization gain value
 * x, nq           Spectral quantized, and count of significants
 * return          Unquantized gain value
 */
LC3_HOT static float unquantize(
    enum lc3_dt dt, enum lc3_srate sr,
    int g_int, float *x, int nq)
{
    float g = unquantize_gain(g_int);
    int i, ne = lc3_ne(dt, sr);

    for (i = 0; i < nq; i++)
        x[i] = x[i] * g;

    for ( ; i < ne; i++)
        x[i] = 0;

    return g;
}


/* ----------------------------------------------------------------------------
 *  Spectrum coding
 * -------------------------------------------------------------------------- */

/**
 * Resolve High-bitrate and LSB modes according size of the frame
 * sr, nbytes      Samplerate and size of the frame
 * p_lsb_mode      True when LSB mode allowed, when not NULL
 * return          True when High-Rate mode enabled
 */
static bool resolve_modes(enum lc3_srate sr, int nbytes, bool *p_lsb_mode)
{
    int sr_ind = lc3_hr(sr) ? 4 + (sr - LC3_SRATE_48K_HR) : sr;

    if (p_lsb_mode)
      *p_lsb_mode = (nbytes >= 20 * (3 + sr_ind)) && (sr < LC3_SRATE_96K_HR);

    return (nbytes > 20 * (1 + sr_ind)) && (sr < LC3_SRATE_96K_HR);
}

/**
 * Bit consumption
 * dt, sr, nbytes  Duration, samplerate and size of the frame
 * x               Spectral quantized coefficients
 * n               Count of significant coefficients, updated on truncation
 * nbits_budget    Truncate to stay in budget, when not zero
 * p_lsb_mode      Return True when LSB's are not AC coded, or NULL
 * return          The number of bits coding the spectrum
 */
LC3_HOT static int compute_nbits(
    enum lc3_dt dt, enum lc3_srate sr, int nbytes,
    const float *x, int *n, int nbits_budget, bool *p_lsb_mode)
{
    bool lsb_mode, high_rate = resolve_modes(sr, nbytes, &lsb_mode);
    int ne = lc3_ne(dt, sr);

    /* --- Loop on quantized coefficients --- */

    int nbits = 0, nbits_lsb = 0;
    uint8_t state = 0;

    int nbits_end = 0;
    int n_end = 0;

    nbits_budget = nbits_budget ? nbits_budget * 2048 : INT_MAX;

    for (int i = 0, h = 0; h < 2; h++) {
        const uint8_t (*lut_coeff)[4] = lc3_spectrum_lookup[high_rate][h];

        for ( ; i < LC3_MIN(*n, (ne + 2) >> (1 - h))
                && nbits <= nbits_budget; i += 2) {

            float xq_off = lc3_hr(sr) ? 0.5f : 6.f/16;
            uint32_t a = fabsf(x[i+0]) + xq_off;
            uint32_t b = fabsf(x[i+1]) + xq_off;

            const uint8_t *lut = lut_coeff[state];

            /* --- Sign values --- */

            int s = (a != 0) + (b != 0);
            nbits += s * 2048;

            /* --- LSB values Reduce to 2*2 bits MSB values ---
             * Reduce to 2x2 bits MSB values. The LSB's pair are arithmetic
             * coded with an escape code followed by 1 bit for each values.
             * The LSB mode does not arthmetic code the first LSB,
             * add the sign of the LSB when one of pair was at value 1 */

            uint32_t m = (a | b) >> 2;
            unsigned k = 0;

            if (m) {

                if (lsb_mode) {
                    nbits += lc3_spectrum_bits[lut[k++]][16] - 2*2048;
                    nbits_lsb += 2 + (a == 1) + (b == 1);
                }

                for (m >>= lsb_mode; m; m >>= 1, k++)
                    nbits += lc3_spectrum_bits[lut[LC3_MIN(k, 3)]][16];

                nbits += k * 2*2048;
                a >>= k;
                b >>= k;

                k = LC3_MIN(k, 3);
            }

            /* --- MSB values --- */

            nbits += lc3_spectrum_bits[lut[k]][a + 4*b];

            /* --- Update state --- */

            if (s && nbits <= nbits_budget) {
                n_end = i + 2;
                nbits_end = nbits;
            }

            state = (state << 4) + (k > 1 ? 12 + k : 1 + (a + b) * (k + 1));
        }
    }

    /* --- Return --- */

    *n = n_end;

    if (p_lsb_mode)
        *p_lsb_mode = lsb_mode &&
            nbits_end + nbits_lsb * 2048 > nbits_budget;

    if (nbits_budget >= INT_MAX)
        nbits_end += nbits_lsb * 2048;

    return (nbits_end + 2047) / 2048;
}

/**
 * Put quantized spectrum
 * bits            Bitstream context
 * dt, sr, nbytes  Duration, samplerate and size of the frame
 * x               Spectral quantized coefficients
 * nq, lsb_mode    Count of significants, and LSB discard indication
 */
LC3_HOT static void put_quantized(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_srate sr, int nbytes,
    const float *x, int nq, bool lsb_mode)
{
    bool high_rate = resolve_modes(sr, nbytes, NULL);
    int ne = lc3_ne(dt, sr);

    /* --- Loop on quantized coefficients --- */

    uint8_t state = 0;

    for (int i = 0, h = 0; h < 2; h++) {
        const uint8_t (*lut_coeff)[4] = lc3_spectrum_lookup[high_rate][h];

        for ( ; i < LC3_MIN(nq, (ne + 2) >> (1 - h)); i += 2) {

            float xq_off = lc3_hr(sr) ? 0.5f : 6.f/16;
            uint32_t a = fabsf(x[i+0]) + xq_off;
            uint32_t b = fabsf(x[i+1]) + xq_off;

            const uint8_t *lut = lut_coeff[state];

            /* --- LSB values Reduce to 2*2 bits MSB values ---
             * Reduce to 2x2 bits MSB values. The LSB's pair are arithmetic
             * coded with an escape code and 1 bits for each values.
             * The LSB mode discard the first LSB (at this step) */

            uint32_t m = (a | b) >> 2;
            unsigned k = 0, shr = 0;

            if (m) {

                if (lsb_mode)
                    lc3_put_symbol(bits,
                        lc3_spectrum_models + lut[k++], 16);

                for (m >>= lsb_mode; m; m >>= 1, k++) {
                    lc3_put_bit(bits, (a >> k) & 1);
                    lc3_put_bit(bits, (b >> k) & 1);
                    lc3_put_symbol(bits,
                        lc3_spectrum_models + lut[LC3_MIN(k, 3)], 16);
                }

                a >>= lsb_mode;
                b >>= lsb_mode;

                shr = k - lsb_mode;
                k = LC3_MIN(k, 3);
            }

            /* --- Sign values --- */

            if (a) lc3_put_bit(bits, x[i+0] < 0);
            if (b) lc3_put_bit(bits, x[i+1] < 0);

            /* --- MSB values --- */

            a >>= shr;
            b >>= shr;

            lc3_put_symbol(bits, lc3_spectrum_models + lut[k], a + 4*b);

            /* --- Update state --- */

            state = (state << 4) + (k > 1 ? 12 + k : 1 + (a + b) * (k + 1));
        }
    }
}

/**
 * Get quantized spectrum
 * bits            Bitstream context
 * dt, sr, nbytes  Duration, samplerate and size of the frame
 * nq, lsb_mode    Count of significants, and LSB discard indication
 * x               Return `nq` spectral quantized coefficients
 * nf_seed         Return the noise factor seed associated
 * return          0: Ok  -1: Invalid bitstream data
 */
LC3_HOT static int get_quantized(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_srate sr, int nbytes,
    int nq, bool lsb_mode, float *x, uint16_t *nf_seed)
{
    bool high_rate = resolve_modes(sr, nbytes, NULL);
    int ne = lc3_ne(dt, sr);

    *nf_seed = 0;

    /* --- Loop on quantized coefficients --- */

    uint8_t state = 0;

    for (int i = 0, h = 0; h < 2; h++) {
        const uint8_t (*lut_coeff)[4] = lc3_spectrum_lookup[high_rate][h];

        for ( ; i < LC3_MIN(nq, (ne + 2) >> (1 - h)); i += 2) {

            const uint8_t *lut = lut_coeff[state];
            int max_shl = lc3_hr(sr) ? 22 : 14;

            /* --- LSB values ---
             * Until the symbol read indicates the escape value 16,
             * read an LSB bit for each values.
             * The LSB mode discard the first LSB (at this step) */

            int u = 0, v = 0;
            int k = 0, shl = 0;

            unsigned s = lc3_get_symbol(bits, lc3_spectrum_models + lut[k]);

            if (lsb_mode && s >= 16) {
                s = lc3_get_symbol(bits, lc3_spectrum_models + lut[++k]);
                shl++;
            }

            for ( ; s >= 16 && shl < max_shl; shl++) {
                u |= lc3_get_bit(bits) << shl;
                v |= lc3_get_bit(bits) << shl;

                k += (k < 3);
                s  = lc3_get_symbol(bits, lc3_spectrum_models + lut[k]);
            }

            if (s >= 16)
                return -1;

            /* --- MSB & sign values --- */

            int a = s % 4;
            int b = s / 4;

            u |= a << shl;
            v |= b << shl;

            x[i+0] = u && lc3_get_bit(bits) ? -u : u;
            x[i+1] = v && lc3_get_bit(bits) ? -v : v;

            *nf_seed = (*nf_seed + (u & 0x7fff) * (i  )
                                 + (v & 0x7fff) * (i+1)) & 0xffff;

            /* --- Update state --- */

            state = (state << 4) + (k > 1 ? 12 + k : 1 + (a + b) * (k + 1));
        }
    }

    return 0;
}

/**
 * Put residual bits of quantization
 * bits            Bitstream context
 * nbits           Maximum number of bits to output
 * hrmode          High-Resolution mode
 * x, n            Spectral quantized, and count of significants
 */
LC3_HOT static void put_residual(lc3_bits_t *bits,
    int nbits, bool hrmode, float *x, int n)
{
    float xq_lim = hrmode ? 0.5f : 10.f/16;
    float xq_off = xq_lim / 2;

    for (int iter = 0; iter < (hrmode ? 20 : 1) && nbits > 0; iter++) {
        for (int i = 0; i < n && nbits > 0; i++) {

            float xr = fabsf(x[i]);
            if (xr < xq_lim)
                continue;

            bool b = (xr - truncf(xr) < xq_lim) ^ (x[i] < 0);
            lc3_put_bit(bits, b);
            nbits--;

            x[i] += b ? -xq_off : xq_off;
        }

        xq_off *= xq_lim;
    }
}

/**
 * Get residual bits of quantization
 * bits            Bitstream context
 * nbits           Maximum number of bits to output
 * hrmode          High-Resolution mode
 * x, nq           Spectral quantized, and count of significants
 */
LC3_HOT static void get_residual(lc3_bits_t *bits,
    int nbits, bool hrmode, float *x, int n)
{
    float xq_off_1 = hrmode ? 0.25f : 5.f/16;
    float xq_off_2 = hrmode ? 0.25f : 3.f/16;

    for (int iter = 0; iter < (hrmode ? 20 : 1) && nbits > 0; iter++) {
        for (int i = 0; i < n && nbits > 0; i++) {

            if (x[i] == 0)
              continue;

            if (lc3_get_bit(bits) == 0)
                x[i] -= x[i] < 0 ? xq_off_1 : xq_off_2;
            else
                x[i] += x[i] > 0 ? xq_off_1 : xq_off_2;

            nbits--;
        }

        xq_off_1 *= 0.5f;
        xq_off_2 *= 0.5f;
    }
}

/**
 * Put LSB values of quantized spectrum values
 * bits            Bitstream context
 * nbits           Maximum number of bits to output
 * hrmode          High-Resolution mode
 * x, n            Spectral quantized, and count of significants
 */
LC3_HOT static void put_lsb(lc3_bits_t *bits,
    int nbits, bool hrmode, const float *x, int n)
{
    for (int i = 0; i < n && nbits > 0; i += 2) {

        float xq_off = hrmode ? 0.5f : 6.f/16;
        uint32_t a = fabsf(x[i+0]) + xq_off;
        uint32_t b = fabsf(x[i+1]) + xq_off;

        if ((a | b) >> 2 == 0)
            continue;

        if (nbits-- > 0)
            lc3_put_bit(bits, a & 1);

        if (a == 1 && nbits-- > 0)
            lc3_put_bit(bits, x[i+0] < 0);

        if (nbits-- > 0)
            lc3_put_bit(bits, b & 1);

        if (b == 1 && nbits-- > 0)
            lc3_put_bit(bits, x[i+1] < 0);
    }
}

/**
 * Get LSB values of quantized spectrum values
 * bits            Bitstream context
 * nbits           Maximum number of bits to output
 * x, nq           Spectral quantized, and count of significants
 * nf_seed         Update the noise factor seed according
 */
LC3_HOT static void get_lsb(lc3_bits_t *bits,
    int nbits, float *x, int nq, uint16_t *nf_seed)
{
    for (int i = 0; i < nq && nbits > 0; i += 2) {

        float a = fabsf(x[i]), b = fabsf(x[i+1]);

        if (fmaxf(a, b) < 4)
            continue;

        if (nbits-- > 0 && lc3_get_bit(bits)) {
            if (a) {
                x[i] += x[i] < 0 ? -1 : 1;
                *nf_seed = (*nf_seed + i) & 0xffff;
            } else if (nbits-- > 0) {
                x[i] = lc3_get_bit(bits) ? -1 : 1;
                *nf_seed = (*nf_seed + i) & 0xffff;
            }
        }

        if (nbits-- > 0 && lc3_get_bit(bits)) {
            if (b) {
                x[i+1] += x[i+1] < 0 ? -1 : 1;
                *nf_seed = (*nf_seed + i+1) & 0xffff;
            } else if (nbits-- > 0) {
                x[i+1] = lc3_get_bit(bits) ? -1 : 1;
                *nf_seed = (*nf_seed + i+1) & 0xffff;
            }
        }
    }
}


/* ----------------------------------------------------------------------------
 *  Noise coding
 * -------------------------------------------------------------------------- */

/**
 * Estimate noise level
 * dt, bw          Duration and bandwidth of the frame
 * hrmode          High-Resolution mode
 * x, n            Spectral quantized, and count of significants
 * return          Noise factor (0 to 7)
 */
LC3_HOT static int estimate_noise(
    enum lc3_dt dt, enum lc3_bandwidth bw, bool hrmode, const float *x, int n)
{
    int bw_stop = lc3_ne(dt, (enum lc3_srate)LC3_MIN(bw, LC3_BANDWIDTH_FB));
    int w = 1 + (dt >= LC3_DT_7M5) + (dt>= LC3_DT_10M);

    float xq_lim = hrmode ? 0.5f : 10.f/16;
    float sum = 0;
    int i, ns = 0, z = 0;

    for (i = 6 * (1 + dt) - w; i < LC3_MIN(n, bw_stop); i++) {
        z = fabsf(x[i]) < xq_lim ? z + 1 : 0;
        if (z > 2*w)
            sum += fabsf(x[i - w]), ns++;
    }

    for ( ; i < bw_stop + w; i++)
        if (++z > 2*w)
            sum += fabsf(x[i - w]), ns++;

    int nf = ns ? 8 - (int)((16 * sum) / ns + 0.5f) : 8;

    return LC3_CLIP(nf, 0, 7);
}

/**
 * Noise filling
 * dt, bw          Duration and bandwidth of the frame
 * nf, nf_seed     The noise factor and pseudo-random seed
 * g               Quantization gain
 * x, nq           Spectral quantized, and count of significants
 */
LC3_HOT static void fill_noise(enum lc3_dt dt, enum lc3_bandwidth bw,
    int nf, uint16_t nf_seed, float g, float *x, int nq)
{
    int bw_stop = lc3_ne(dt, (enum lc3_srate)LC3_MIN(bw, LC3_BANDWIDTH_FB));
    int w = 1 + (dt >= LC3_DT_7M5) + (dt>= LC3_DT_10M);

    float s = g * (float)(8 - nf) / 16;
    int i, z = 0;

    for (i = 6 * (1 + dt) - w; i < LC3_MIN(nq, bw_stop); i++) {
        z = x[i] ? 0 : z + 1;
        if (z > 2*w) {
            nf_seed = (13849 + nf_seed*31821) & 0xffff;
            x[i - w] = nf_seed & 0x8000 ? -s : s;
        }
    }

    for ( ; i < bw_stop + w; i++)
        if (++z > 2*w) {
            nf_seed = (13849 + nf_seed*31821) & 0xffff;
            x[i - w] = nf_seed & 0x8000 ? -s : s;
        }
}

/**
 * Put noise factor
 * bits            Bitstream context
 * nf              Noise factor (0 to 7)
 */
static void put_noise_factor(lc3_bits_t *bits, int nf)
{
    lc3_put_bits(bits, nf, 3);
}

/**
 * Get noise factor
 * bits            Bitstream context
 * return          Noise factor (0 to 7)
 */
static int get_noise_factor(lc3_bits_t *bits)
{
    return lc3_get_bits(bits, 3);
}


/* ----------------------------------------------------------------------------
 *  Encoding
 * -------------------------------------------------------------------------- */

/**
 * Bit consumption of the number of coded coefficients
 * dt, sr, nbytes  Duration, samplerate and size of the frame
 * return          Bit consumpution of the number of coded coefficients
 */
static int get_nbits_nq(enum lc3_dt dt, enum lc3_srate sr)
{
    int ne = lc3_ne(dt, sr);
    return 4 + (ne > 32) + (ne > 64) + (ne > 128) + (ne > 256) + (ne > 512);
}

/**
 * Bit consumption of the arithmetic coder
 * dt, sr, nbytes  Duration, samplerate and size of the frame
 * return          Bit consumption of bitstream data
 */
static int get_nbits_ac(enum lc3_dt dt, enum lc3_srate sr, int nbytes)
{
    return get_nbits_nq(dt, sr) +
        3 + lc3_hr(sr) + LC3_MIN((nbytes-1) / 160, 2);
}

/**
 * Spectrum analysis
 */
void lc3_spec_analyze(
    enum lc3_dt dt, enum lc3_srate sr, int nbytes,
    bool pitch, const lc3_tns_data_t *tns,
    struct lc3_spec_analysis *spec,
    float *x, struct lc3_spec_side *side)
{
    bool reset_off;

    /* --- Bit budget --- */

    const int nbits_gain = 8;
    const int nbits_nf = 3;

    int nbits_budget = 8*nbytes - get_nbits_ac(dt, sr, nbytes) -
        lc3_bwdet_get_nbits(sr) - lc3_ltpf_get_nbits(pitch) -
        lc3_sns_get_nbits() - lc3_tns_get_nbits(tns) - nbits_gain - nbits_nf;

    /* --- Global gain --- */

    float nbits_off = spec->nbits_off + spec->nbits_spare;
    nbits_off = fminf(fmaxf(nbits_off, -40), 40);
    nbits_off = 0.8f * spec->nbits_off + 0.2f * nbits_off;

    int g_off = resolve_gain_offset(sr, nbytes);

    int g_min, g_int = estimate_gain(dt, sr,
        x, nbytes, nbits_budget, nbits_off, g_off, &reset_off, &g_min);

    /* --- Quantization --- */

    quantize(dt, sr, g_int, x, &side->nq);

    int nbits = compute_nbits(dt, sr, nbytes, x, &side->nq, 0, NULL);

    spec->nbits_off = reset_off ? 0 : nbits_off;
    spec->nbits_spare = reset_off ? 0 : nbits_budget - nbits;

    /* --- Adjust gain and requantize --- */

    int g_adj = adjust_gain(dt, sr,
        g_off + g_int, nbits, nbits_budget, g_off + g_min);

    if (g_adj)
        quantize(dt, sr, g_adj, x, &side->nq);

    side->g_idx = g_int + g_adj + g_off;
    nbits = compute_nbits(dt, sr, nbytes,
        x, &side->nq, nbits_budget, &side->lsb_mode);
}

/**
 * Put spectral quantization side data
 */
void lc3_spec_put_side(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_srate sr,
    const struct lc3_spec_side *side)
{
    int nbits_nq = get_nbits_nq(dt, sr);

    lc3_put_bits(bits, LC3_MAX(side->nq >> 1, 1) - 1, nbits_nq);
    lc3_put_bits(bits, side->lsb_mode, 1);
    lc3_put_bits(bits, side->g_idx, 8);
}

/**
 * Encode spectral coefficients
 */
void lc3_spec_encode(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_srate sr, enum lc3_bandwidth bw,
    int nbytes, const lc3_spec_side_t *side, float *x)
{
    bool lsb_mode = side->lsb_mode;
    int nq = side->nq;

    put_noise_factor(bits, estimate_noise(dt, bw, lc3_hr(sr), x, nq));

    put_quantized(bits, dt, sr, nbytes, x, nq, lsb_mode);

    int nbits_left = lc3_get_bits_left(bits);

    if (lsb_mode)
        put_lsb(bits, nbits_left, lc3_hr(sr), x, nq);
    else
        put_residual(bits, nbits_left, lc3_hr(sr), x, nq);
}


/* ----------------------------------------------------------------------------
 *  Decoding
 * -------------------------------------------------------------------------- */

/**
 * Get spectral quantization side data
 */
int lc3_spec_get_side(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_srate sr, struct lc3_spec_side *side)
{
    int nbits_nq = get_nbits_nq(dt, sr);
    int ne = lc3_ne(dt, sr);

    side->nq = (lc3_get_bits(bits, nbits_nq) + 1) << 1;
    side->lsb_mode = lc3_get_bit(bits);
    side->g_idx = lc3_get_bits(bits, 8);

    return side->nq > ne ? (side->nq = ne), -1 : 0;
}

/**
 * Decode spectral coefficients
 */
int lc3_spec_decode(lc3_bits_t *bits,
    enum lc3_dt dt, enum lc3_srate sr, enum lc3_bandwidth bw,
    int nbytes, const lc3_spec_side_t *side, float *x)
{
    bool lsb_mode = side->lsb_mode;
    int nq = side->nq;
    int ret = 0;

    int nf = get_noise_factor(bits);
    uint16_t nf_seed;

    if ((ret = get_quantized(bits, dt, sr, nbytes,
                    nq, lsb_mode, x, &nf_seed)) < 0)
        return ret;

    int nbits_left = lc3_get_bits_left(bits);

    if (lsb_mode)
        get_lsb(bits, nbits_left, x, nq, &nf_seed);
    else
        get_residual(bits, nbits_left, lc3_hr(sr), x, nq);

    int g_int = side->g_idx - resolve_gain_offset(sr, nbytes);
    float g = unquantize(dt, sr, g_int, x, nq);

    if (nq > 2 || x[0] || x[1] || side->g_idx > 0 || nf < 7)
        fill_noise(dt, bw, nf, nf_seed, g, x, nq);

    return 0;
}
