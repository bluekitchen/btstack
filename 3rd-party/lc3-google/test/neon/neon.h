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

#if __ARM_NEON && __ARM_ARCH_ISA_A64

#include <arm_neon.h>

#else

#include <stdint.h>


/* ----------------------------------------------------------------------------
 *  Integer
 * -------------------------------------------------------------------------- */

typedef struct { int16_t e[4]; } int16x4_t;

typedef struct { int16_t e[8]; } int16x8_t;
typedef struct { int32_t e[4]; } int32x4_t;
typedef struct { int64_t e[2]; } int64x2_t;


/**
 * Load / Store
 */

__attribute__((unused))
static int16x4_t vld1_s16(const int16_t *p)
{
    return (int16x4_t){ { p[0], p[1], p[2], p[3] } };
}


/**
 * Arithmetic
 */

__attribute__((unused))
static int32x4_t vmull_s16(int16x4_t a, int16x4_t b)
{
    return (int32x4_t){ { a.e[0] * b.e[0], a.e[1] * b.e[1],
                          a.e[2] * b.e[2], a.e[3] * b.e[3]  } };
}

__attribute__((unused))
static int32x4_t vmlal_s16(int32x4_t r, int16x4_t a, int16x4_t b)
{
    return (int32x4_t){ {
        r.e[0] + a.e[0] * b.e[0], r.e[1] + a.e[1] * b.e[1],
        r.e[2] + a.e[2] * b.e[2], r.e[3] + a.e[3] * b.e[3] } };
}

__attribute__((unused))
static int64x2_t vpadalq_s32(int64x2_t a, int32x4_t b)
{
    int64x2_t r;

    r.e[0] = a.e[0] + ((int64_t)b.e[0] + b.e[1]);
    r.e[1] = a.e[1] + ((int64_t)b.e[2] + b.e[3]);

    return r;
}


/**
 * Reduce
 */

__attribute__((unused))
static int32_t vaddvq_s32(int32x4_t v)
{
    return v.e[0] + v.e[1] + v.e[2] + v.e[3];
}

__attribute__((unused))
static int64_t vaddvq_s64(int64x2_t v)
{
    return v.e[0] + v.e[1];
}


/**
 * Manipulation
 */

__attribute__((unused))
static int16x4_t vext_s16(int16x4_t a, int16x4_t b, const int n)
{
    int16_t x[] = { a.e[0], a.e[1], a.e[2], a.e[3],
                    b.e[0], b.e[1], b.e[2], b.e[3] };

    return (int16x4_t){ { x[n], x[n+1], x[n+2], x[n+3] } };
}

__attribute__((unused))
static int32x4_t vmovq_n_s32(uint32_t v)
{
    return (int32x4_t){ { v, v, v, v } };
}

__attribute__((unused))
static int64x2_t vmovq_n_s64(int64_t v)
{
    return (int64x2_t){ { v, v, } };
}



/* ----------------------------------------------------------------------------
 *  Floating Point
 * -------------------------------------------------------------------------- */

typedef struct { float e[2]; } float32x2_t;
typedef struct { float e[4]; } float32x4_t;

typedef struct { float32x2_t val[2]; } float32x2x2_t;
typedef struct { float32x4_t val[2]; } float32x4x2_t;


/**
 * Load / Store
 */

__attribute__((unused))
static float32x2_t vld1_f32(const float *p)
{
    return (float32x2_t){ { p[0], p[1] } };
}

__attribute__((unused))
static float32x4_t vld1q_f32(const float *p)
{
    return (float32x4_t){ { p[0], p[1], p[2], p[3] } };
}

__attribute__((unused))
static float32x4_t vld1q_dup_f32(const float *p)
{
    return (float32x4_t){ { p[0], p[0], p[0], p[0] } };
}

__attribute__((unused))
static float32x2x2_t vld2_f32(const float *p)
{
    return (float32x2x2_t){ .val[0] = { { p[0], p[2] } },
                            .val[1] = { { p[1], p[3] } } };
}

__attribute__((unused))
static float32x4x2_t vld2q_f32(const float *p)
{
    return (float32x4x2_t){ .val[0] = { { p[0], p[2], p[4], p[6] } },
                            .val[1] = { { p[1], p[3], p[5], p[7] } } };
}

__attribute__((unused))
static void vst1_f32(float *p, float32x2_t v)
{
    p[0] = v.e[0], p[1] = v.e[1];
}

__attribute__((unused))
static void vst1q_f32(float *p, float32x4_t v)
{
    p[0] = v.e[0], p[1] = v.e[1], p[2] = v.e[2], p[3] = v.e[3];
}

/**
 * Arithmetic
 */

__attribute__((unused))
static float32x2_t vneg_f32(float32x2_t a)
{
    return (float32x2_t){ { -a.e[0], -a.e[1] } };
}

__attribute__((unused))
static float32x4_t vnegq_f32(float32x4_t a)
{
    return (float32x4_t){ { -a.e[0], -a.e[1], -a.e[2], -a.e[3] } };
}

__attribute__((unused))
static float32x4_t vaddq_f32(float32x4_t a, float32x4_t b)
{
    return (float32x4_t){ { a.e[0] + b.e[0], a.e[1] + b.e[1],
                            a.e[2] + b.e[2], a.e[3] + b.e[3] } };
}

__attribute__((unused))
static float32x4_t vsubq_f32(float32x4_t a, float32x4_t b)
{
    return (float32x4_t){ { a.e[0] - b.e[0], a.e[1] - b.e[1],
                            a.e[2] - b.e[2], a.e[3] - b.e[3] } };
}

__attribute__((unused))
static float32x2_t vfma_f32(float32x2_t a, float32x2_t b, float32x2_t c)
{
    return (float32x2_t){ {
        a.e[0] + b.e[0] * c.e[0], a.e[1] + b.e[1] * c.e[1] } };
}

__attribute__((unused))
static float32x4_t vfmaq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{
    return (float32x4_t){ {
        a.e[0] + b.e[0] * c.e[0], a.e[1] + b.e[1] * c.e[1],
        a.e[2] + b.e[2] * c.e[2], a.e[3] + b.e[3] * c.e[3] } };
}

__attribute__((unused))
static float32x2_t vfms_f32(float32x2_t a, float32x2_t b, float32x2_t c)
{
    return (float32x2_t){ {
        a.e[0] - b.e[0] * c.e[0], a.e[1] - b.e[1] * c.e[1] } };
}

__attribute__((unused))
static float32x4_t vfmsq_f32(float32x4_t a, float32x4_t b, float32x4_t c)
{
    return (float32x4_t){ {
        a.e[0] - b.e[0] * c.e[0], a.e[1] - b.e[1] * c.e[1],
        a.e[2] - b.e[2] * c.e[2], a.e[3] - b.e[3] * c.e[3] } };
}


/**
 * Manipulation
 */

__attribute__((unused))
static float32x2_t vcreate_f32(uint64_t u)
{
    float *f = (float *)&u;
    return (float32x2_t){ { f[0] , f[1] } };
}

__attribute__((unused))
static float32x4_t vcombine_f32(float32x2_t a, float32x2_t b)
{
    return (float32x4_t){ { a.e[0], a.e[1], b.e[0], b.e[1] } };
}

__attribute__((unused))
static float32x2_t vget_low_f32(float32x4_t a)
{
    return (float32x2_t){ { a.e[0], a.e[1] } };
}

__attribute__((unused))
static float32x2_t vget_high_f32(float32x4_t a)
{
    return (float32x2_t){ { a.e[2], a.e[3] } };
}

__attribute__((unused))
static float32x4_t vmovq_n_f32(float v)
{
    return (float32x4_t){ { v, v, v, v } };
}

__attribute__((unused))
static float32x2_t vrev64_f32(float32x2_t v)
{
    return (float32x2_t){ { v.e[1], v.e[0] } };
}

__attribute__((unused))
static float32x4_t vrev64q_f32(float32x4_t v)
{
    return (float32x4_t){ { v.e[1], v.e[0], v.e[3], v.e[2] } };
}

__attribute__((unused))
static float32x2_t vtrn1_f32(float32x2_t a, float32x2_t b)
{
    return (float32x2_t){ { a.e[0], b.e[0] } };
}

__attribute__((unused))
static float32x2_t vtrn2_f32(float32x2_t a, float32x2_t b)
{
    return (float32x2_t){ { a.e[1], b.e[1] } };
}

__attribute__((unused))
static float32x4_t vtrn1q_f32(float32x4_t a, float32x4_t b)
{
    return (float32x4_t){ { a.e[0], b.e[0], a.e[2], b.e[2] } };
}

__attribute__((unused))
static float32x4_t vtrn2q_f32(float32x4_t a, float32x4_t b)
{
    return (float32x4_t){ { a.e[1], b.e[1], a.e[3], b.e[3] } };
}

__attribute__((unused))
static float32x4_t vzip1q_f32(float32x4_t a, float32x4_t b)
{
    return (float32x4_t){ { a.e[0], b.e[0], a.e[1], b.e[1] } };
}

__attribute__((unused))
static float32x4_t vzip2q_f32(float32x4_t a, float32x4_t b)
{
    return (float32x4_t){ { a.e[2], b.e[2], a.e[3], b.e[3] } };
}


#endif /* __ARM_NEON */
