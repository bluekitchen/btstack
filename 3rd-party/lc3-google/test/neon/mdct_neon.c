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

#include "neon.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------- */

#define TEST_NEON
#include <mdct.c>

/* -------------------------------------------------------------------------- */

static int check_fft(void)
{
    struct lc3_complex x[240];
    struct lc3_complex y[240], y_neon[240];

    for (int i = 0; i < 240; i++) {
          x[i].re = (double)rand() / RAND_MAX;
          x[i].im = (double)rand() / RAND_MAX;
    }

    fft_5(x, y, 240/5);
    neon_fft_5(x, y_neon, 240/5);
    for (int i = 0; i < 240; i++)
        if (fabsf(y[i].re - y_neon[i].re) > 1e-6f ||
            fabsf(y[i].im - y_neon[i].im) > 1e-6f   )
            return -1;

    fft_bf3(lc3_fft_twiddles_bf3[0], x, y, 240/15);
    neon_fft_bf3(lc3_fft_twiddles_bf3[0], x, y_neon, 240/15);
    for (int i = 0; i < 240; i++)
        if (fabsf(y[i].re - y_neon[i].re) > 1e-6f ||
            fabsf(y[i].im - y_neon[i].im) > 1e-6f   )
            return -1;

    fft_bf2(lc3_fft_twiddles_bf2[0][1], x, y, 240/30);
    neon_fft_bf2(lc3_fft_twiddles_bf2[0][1], x, y_neon, 240/30);
    for (int i = 0; i < 240; i++)
        if (fabsf(y[i].re - y_neon[i].re) > 1e-6f ||
            fabsf(y[i].im - y_neon[i].im) > 1e-6f   )
            return -1;

    return 0;
}

int check_mdct(void)
{
    int ret;

    if ((ret = check_fft()) < 0)
        return ret;

    return 0;
}
