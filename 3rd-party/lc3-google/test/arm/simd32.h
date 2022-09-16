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

#if __ARM_FEATURE_SIMD32

#include <arm_acle.h>

#else
#define __ARM_FEATURE_SIMD32 1

#include <stdint.h>

typedef int32_t int16x2_t;

__attribute__((unused))
static int16x2_t __pkhbt(int16x2_t a, int16x2_t b)
{
    uint32_t a_bot = (uint32_t)a & 0x0000ffffu;
    uint32_t b_top = (uint32_t)b & 0xffff0000u;

    return (int16x2_t)(a_bot | b_top);
}

__attribute__((unused))
static int32_t __smlad(int16x2_t a, int16x2_t b, int32_t u)
{
    int16_t a_hi = a >> 16, a_lo = a & 0xffff;
    int16_t b_hi = b >> 16, b_lo = b & 0xffff;

    return u + (a_hi * b_hi) + (a_lo * b_lo);
}

__attribute__((unused))
static int64_t __smlald(int16x2_t a, int16x2_t b, int64_t u)
{
    int16_t a_hi = a >> 16, a_lo = a & 0xffff;
    int16_t b_hi = b >> 16, b_lo = b & 0xffff;
    return u + (a_hi * b_hi) + (a_lo * b_lo);
}

__attribute__((unused))
static int64_t __smlaldx(int16x2_t a, int16x2_t b, int64_t u)
{
    int16_t a_hi = a >> 16, a_lo = a & 0xffff;
    int16_t b_hi = b >> 16, b_lo = b & 0xffff;
    return u + (a_hi * b_lo) + (a_lo * b_hi);
}

#endif /* __ARM_FEATURE_SIMD32 */
