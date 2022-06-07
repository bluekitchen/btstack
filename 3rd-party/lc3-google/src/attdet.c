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

#include "attdet.h"


/**
 * Time domain attack detector
 */
bool lc3_attdet_run(enum lc3_dt dt, enum lc3_srate sr,
    int nbytes, struct lc3_attdet_analysis *attdet, const float *x)
{
    /* --- Check enabling --- */

    const int nbytes_ranges[LC3_NUM_DT][LC3_NUM_SRATE - LC3_SRATE_32K][2] = {
            [LC3_DT_7M5] = { { 61,     149 }, {  75,     149 } },
            [LC3_DT_10M] = { { 81, INT_MAX }, { 100, INT_MAX } },
    };

    if (sr < LC3_SRATE_32K ||
            nbytes < nbytes_ranges[dt][sr - LC3_SRATE_32K][0] ||
            nbytes > nbytes_ranges[dt][sr - LC3_SRATE_32K][1]   )
        return 0;

    /* --- Filtering & Energy calculation --- */

    int nblk = 4 - (dt == LC3_DT_7M5);
    float e[4];

    for (int i = 0; i < nblk; i++) {
        e[i] = 0;

        if (sr == LC3_SRATE_32K) {
            float xn2 = x[-4] + x[-3];
            float xn1 = x[-2] + x[-1];
            float xn, xf;

            for (int j = 0; j < 40; j++, x += 2, xn2 = xn1, xn1 = xn) {
                xn = x[0] + x[1];
                xf = 0.375 * xn - 0.5 * xn1 + 0.125 * xn2;
                e[i] += xf * xf;
            }
        }

        else {
            float xn2 = x[-6] + x[-5] + x[-4];
            float xn1 = x[-3] + x[-2] + x[-1];
            float xn, xf;

            for (int j = 0; j < 40; j++, x += 3, xn2 = xn1, xn1 = xn) {
                xn = x[0] + x[1] + x[2];
                xf = 0.375 * xn - 0.5 * xn1 + 0.125 * xn2;
                e[i] += xf * xf;
            }
        }
    }

    /* --- Attack detection ---
     * The attack block `p_att` is defined as the normative value + 1,
     * in such way, it will be initialized to 0 */

    int p_att = 0;
    float a[4];

    for (int i = 0; i < nblk; i++) {
        a[i] = fmaxf(0.25 * attdet->an1, attdet->en1);
        attdet->en1 = e[i], attdet->an1 = a[i];

        if (e[i] > 8.5 * a[i])
            p_att = i + 1;
    }

    int att = attdet->p_att >= 1 + (nblk >> 1) || p_att > 0;
    attdet->p_att = p_att;

    return att;
}
