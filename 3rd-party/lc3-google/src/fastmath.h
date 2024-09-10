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

#ifndef __LC3_FASTMATH_H
#define __LC3_FASTMATH_H

#include <stdint.h>
#include <float.h>
#include <math.h>


/**
 * IEEE 754 Floating point representation
 */

#define LC3_IEEE754_SIGN_SHL   (31)
#define LC3_IEEE754_SIGN_MASK  (1 << LC3_IEEE754_SIGN_SHL)

#define LC3_IEEE754_EXP_SHL    (23)
#define LC3_IEEE754_EXP_MASK   (0xff << LC3_IEEE754_EXP_SHL)
#define LC3_IEEE754_EXP_BIAS   (127)


/**
 * Fast multiply floating-point number by integral power of 2
 * x               Operand, finite number
 * exp             Exponent such that 2^x is finite
 * return          2^exp
 */
static inline float lc3_ldexpf(float _x, int exp) {
    union { float f; int32_t s; } x = { .f = _x };

    if (x.s & LC3_IEEE754_EXP_MASK)
        x.s += exp << LC3_IEEE754_EXP_SHL;

    return x.f;
}

/**
 * Fast convert floating-point number to fractional and integral components
 * x               Operand, finite number
 * exp             Return the exponent part
 * return          The normalized fraction in [0.5:1[
 */
static inline float lc3_frexpf(float _x, int *exp) {
    union { float f; uint32_t u; } x = { .f = _x };

    int e = (x.u & LC3_IEEE754_EXP_MASK) >> LC3_IEEE754_EXP_SHL;
    *exp = e - (LC3_IEEE754_EXP_BIAS - 1);

    x.u = (x.u & ~LC3_IEEE754_EXP_MASK) |
        ((LC3_IEEE754_EXP_BIAS - 1) << LC3_IEEE754_EXP_SHL);

    return x.f;
}

/**
 * Fast 2^n approximation
 * x               Operand, range -100 to 100
 * return          2^x approximation (max relative error ~ 4e-7)
 */
static inline float lc3_exp2f(float x)
{
    /* --- 2^(i/8) for i from 0 to 7 --- */

    static const float e[] = {
        1.00000000e+00, 1.09050773e+00, 1.18920712e+00, 1.29683955e+00,
        1.41421356e+00, 1.54221083e+00, 1.68179283e+00, 1.83400809e+00 };

    /* --- Polynomial approx in range 0 to 1/8 --- */

    static const float p[] = {
        1.00448128e-02, 5.54563260e-02, 2.40228756e-01, 6.93147140e-01 };

    /* --- Split the operand ---
     *
     * Such as x = k/8 + y, with k an integer, and |y| < 0.5/8
     *
     * Note that `fast-math` compiler option leads to rounding error,
     * disable optimisation with `volatile`. */

    volatile union { float f; int32_t s; } v;

    v.f = x + 0x1.8p20f;
    int k = v.s;
    x -= v.f - 0x1.8p20f;

    /* --- Compute 2^x, with |x| < 1 ---
     * Perform polynomial approximation in range -0.5/8 to 0.5/8,
     * and muplity by precomputed value of 2^(i/8), i in [0:7] */

    union { float f; int32_t s; } y;

    y.f = (      p[0]) * x;
    y.f = (y.f + p[1]) * x;
    y.f = (y.f + p[2]) * x;
    y.f = (y.f + p[3]) * x;
    y.f = (y.f +  1.f) * e[k & 7];

    /* --- Add the exponent --- */

    y.s += (k >> 3) << LC3_IEEE754_EXP_SHL;

    return y.f;
}

/**
 * Fast log2(x) approximation
 * x               Operand, greater than 0
 * return          log2(x) approximation (max absolute error ~ 1e-4)
 */
static inline float lc3_log2f(float x)
{
    float y;
    int e;

    /* --- Polynomial approx in range 0.5 to 1 --- */

    static const float c[] = {
        -1.29479677, 5.11769018, -8.42295281, 8.10557963, -3.50567360 };

    x = lc3_frexpf(x, &e);

    y = (    c[0]) * x;
    y = (y + c[1]) * x;
    y = (y + c[2]) * x;
    y = (y + c[3]) * x;
    y = (y + c[4]);

    /* --- Add log2f(2^e) and return --- */

    return e + y;
}

/**
 * Fast log10(x) approximation
 * x               Operand, greater than 0
 * return          log10(x) approximation (max absolute error ~ 1e-4)
 */
static inline float lc3_log10f(float x)
{
    return log10f(2) * lc3_log2f(x);
}

/**
 * Fast `10 * log10(x)` (or dB) approximation in fixed Q16
 * x               Operand, in range 2^-63 to 2^63 (1e-19 to 1e19)
 * return          10 * log10(x) in fixed Q16 (-190 to 192 dB)
 *
 * - The 0 value is accepted and return the minimum value ~ -191dB
 * - This function assumed that float 32 bits is coded IEEE 754
 */
static inline int32_t lc3_db_q16(float x)
{
    /* --- Table in Q15 --- */

    static const uint16_t t[][2] = {

        /* [n][0] = 10 * log10(2) * log2(1 + n/32), with n = [0..15]     */
        /* [n][1] = [n+1][0] - [n][0] (while defining [16][0])           */

        {     0, 4379 }, {  4379, 4248 }, {  8627, 4125 }, { 12753, 4009 },
        { 16762, 3899 }, { 20661, 3795 }, { 24456, 3697 }, { 28153, 3603 },
        { 31755, 3514 }, { 35269, 3429 }, { 38699, 3349 }, { 42047, 3272 },
        { 45319, 3198 }, { 48517, 3128 }, { 51645, 3061 }, { 54705, 2996 },

        /* [n][0] = 10 * log10(2) * log2(1 + n/32) - 10 * log10(2) / 2,  */
        /*     with n = [16..31]                                         */
        /* [n][1] = [n+1][0] - [n][0] (while defining [32][0])           */

        {  8381, 2934 }, { 11315, 2875 }, { 14190, 2818 }, { 17008, 2763 },
        { 19772, 2711 }, { 22482, 2660 }, { 25142, 2611 }, { 27754, 2564 },
        { 30318, 2519 }, { 32837, 2475 }, { 35312, 2433 }, { 37744, 2392 },
        { 40136, 2352 }, { 42489, 2314 }, { 44803, 2277 }, { 47080, 2241 },

    };

    /* --- Approximation ---
     *
     *   10 * log10(x^2) = 10 * log10(2) * log2(x^2)
     *
     *   And log2(x^2) = 2 * log2( (1 + m) * 2^e )
     *                 = 2 * (e + log2(1 + m)) , with m in range [0..1]
     *
     * Split the float values in :
     *   e2  Double value of the exponent (2 * e + k)
     *   hi  High 5 bits of mantissa, for precalculated result `t[hi][0]`
     *   lo  Low 16 bits of mantissa, for linear interpolation `t[hi][1]`
     *
     * Two cases, from the range of the mantissa :
     *   0 to 0.5   `k = 0`, use 1st part of the table
     *   0.5 to 1   `k = 1`, use 2nd part of the table  */

    union { float f; uint32_t u; } x2 = { .f = x*x };

    int e2 = (int)(x2.u >> 22) - 2*127;
    int hi = (x2.u >> 18) & 0x1f;
    int lo = (x2.u >>  2) & 0xffff;

    return e2 * 49321 + t[hi][0] + ((t[hi][1] * lo) >> 16);
}


#endif /* __LC3_FASTMATH_H */
