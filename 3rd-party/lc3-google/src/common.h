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

/**
 * LC3 - Common constants and types
 */

#ifndef __LC3_COMMON_H
#define __LC3_COMMON_H

#include <lc3.h>

#include <limits.h>
#include <string.h>
#include <math.h>


/**
 * Macros
 * MIN/MAX  Minimum and maximum between 2 values
 * CLIP     Clip a value between low and high limits
 * ABS      Return the absolute value
 */

#define LC3_MIN(a, b)  ( (a) < (b) ? (a) : (b) )
#define LC3_MAX(a, b)  ( (a) > (b) ? (a) : (b) )
#define LC3_CLIP(v, min, max)  LC3_MIN(LC3_MAX(v, min), max)

#define LC3_ABS(n)  ( (n) < 0 ? -(n) : (n) )


/**
 * Convert `dt` in us and `sr` in KHz
 */

#define LC3_DT_US(dt) \
    ( (3 + (dt)) * 2500 )

#define LC3_SRATE_KHZ(sr) \
    ( (1 + (sr) + ((sr) == LC3_SRATE_48K)) * 8 )


/**
 * Return number of samples, delayed samples and
 * encoded spectrum coefficients within a frame
 * For decoding, add number of samples of 18 ms history
 */

#define LC3_NS(dt, sr) \
    ( 20 * (3 + (dt)) * (1 + (sr) + ((sr) == LC3_SRATE_48K)) )

#define LC3_ND(dt, sr) \
    ( (dt) == LC3_DT_7M5 ? 23 * LC3_NS(dt, sr) / 30 \
                         :  5 * LC3_NS(dt, sr) /  8 )
#define LC3_NE(dt, sr) \
    ( 20 * (3 + (dt)) * (1 + (sr)) )

#define LC3_MAX_NE \
    LC3_NE(LC3_DT_10M, LC3_SRATE_48K)

#define LC3_NH(sr) \
    (18 * LC3_SRATE_KHZ(sr))


/**
 * Bandwidth, mapped to Nyquist frequency of samplerates
 */

enum lc3_bandwidth {
    LC3_BANDWIDTH_NB = LC3_SRATE_8K,
    LC3_BANDWIDTH_WB = LC3_SRATE_16K,
    LC3_BANDWIDTH_SSWB = LC3_SRATE_24K,
    LC3_BANDWIDTH_SWB = LC3_SRATE_32K,
    LC3_BANDWIDTH_FB = LC3_SRATE_48K,

    LC3_NUM_BANDWIDTH,
};


/**
 * Complex floating point number
 */

struct lc3_complex
{
    float re, im;
};


#endif /* __LC3_COMMON_H */
