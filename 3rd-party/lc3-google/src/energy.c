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

#include "energy.h"
#include "tables.h"


/**
 * Energy estimation per band
 */
bool lc3_energy_compute(
    enum lc3_dt dt, enum lc3_srate sr, const float *x, float *e)
{
    int nb = lc3_num_bands[dt][sr];
    const int *lim = lc3_band_lim[dt][sr];

    /* Mean the square of coefficients within each band */

    float e_sum[2] = { 0, 0 };
    int iband_h = nb - (const int []){
        [LC3_DT_2M5] = 2, [LC3_DT_5M ] = 3,
        [LC3_DT_7M5] = 4, [LC3_DT_10M] = 2 }[dt];

    for (int iband = 0, i = lim[iband]; iband < nb; iband++) {
        int ie = lim[iband+1];
        int n = ie - i;

        float sx2 = x[i] * x[i];
        for (i++; i < ie; i++)
            sx2 += x[i] * x[i];

        *e = sx2 / n;
        e_sum[iband >= iband_h] += *(e++);
    }

    /* Return the near nyquist flag */

    return e_sum[1] > 30 * e_sum[0];
}
