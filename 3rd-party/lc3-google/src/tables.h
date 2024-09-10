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

#ifndef __LC3_TABLES_H
#define __LC3_TABLES_H

#include "common.h"
#include "bits.h"


/**
 * Characteristics
 *
 *   ns   Number of temporal samples / frequency coefficients within a frame
 *
 *   ne   Number of encoded frequency coefficients
 *
 *   nd   Number of MDCT delayed samples, sum of half a frame and an ovelap
 *        of future by 1.25 ms (2.5ms, 5ms and 10ms frame durations),
 *        or 2 ms (7.5ms frame duration).
 *
 *   nh   Number of 18 ms samples of the history buffer, aligned on a frame
 *
 *   nt   Number of 1.25 ms previous samples
 */

extern const int lc3_ns_2m5[LC3_NUM_SRATE];
extern const int lc3_ne_2m5[LC3_NUM_SRATE];
extern const int lc3_ns_4m [LC3_NUM_SRATE];

static inline int lc3_ns(enum lc3_dt dt, enum lc3_srate sr) {
    return lc3_ns_2m5[sr] * (1 + dt);
}

static inline int lc3_ne(enum lc3_dt dt, enum lc3_srate sr) {
    return lc3_ne_2m5[sr] * (1 + dt);
}

static inline int lc3_nd(enum lc3_dt dt, enum lc3_srate sr) {
    return ( lc3_ns(dt, sr) +
        (dt == LC3_DT_7M5 ? lc3_ns_4m[sr] : lc3_ns_2m5[sr]) ) >> 1;
}

static inline int lc3_nh(enum lc3_dt dt, enum lc3_srate sr) {
    return sr > LC3_SRATE_48K_HR ? 0 :
        (8 + (dt == LC3_DT_7M5)) * lc3_ns_2m5[sr];
}

static inline int lc3_nt(enum lc3_srate sr) {
    return lc3_ns_2m5[sr] >> 1;
}

#define LC3_MAX_SRATE_HZ  ( LC3_PLUS_HR ? 96000 : 48000 )

#define LC3_MAX_NS  ( LC3_NS(10000, LC3_MAX_SRATE_HZ) )
#define LC3_MAX_NE  ( LC3_PLUS_HR ? LC3_MAX_NS : LC3_NS(10000, 40000) )


/**
 * Limits on size of frame
 */

extern const int lc3_frame_bytes_hr_lim
        [LC3_NUM_DT][LC3_NUM_SRATE - LC3_SRATE_48K_HR][2];

static inline int lc3_min_frame_bytes(enum lc3_dt dt, enum lc3_srate sr) {
    return !lc3_hr(sr) ? LC3_MIN_FRAME_BYTES :
        lc3_frame_bytes_hr_lim[dt][sr - LC3_SRATE_48K_HR][0];
}

static inline int lc3_max_frame_bytes(enum lc3_dt dt, enum lc3_srate sr) {
    return !lc3_hr(sr) ? LC3_MAX_FRAME_BYTES :
        lc3_frame_bytes_hr_lim[dt][sr - LC3_SRATE_48K_HR][1];
}


/**
 * MDCT Twiddles and window coefficients
 */

struct lc3_fft_bf3_twiddles { int n3; const struct lc3_complex (*t)[2]; };
struct lc3_fft_bf2_twiddles { int n2; const struct lc3_complex *t; };
struct lc3_mdct_rot_def { int n4; const struct lc3_complex *w; };

extern const struct lc3_fft_bf3_twiddles *lc3_fft_twiddles_bf3[];
extern const struct lc3_fft_bf2_twiddles *lc3_fft_twiddles_bf2[][3];
extern const struct lc3_mdct_rot_def *lc3_mdct_rot[LC3_NUM_DT][LC3_NUM_SRATE];

extern const float *lc3_mdct_win[LC3_NUM_DT][LC3_NUM_SRATE];


/**
 * Limits of bands
 */

#define LC3_MAX_BANDS  64

extern const int lc3_num_bands[LC3_NUM_DT][LC3_NUM_SRATE];
extern const int *lc3_band_lim[LC3_NUM_DT][LC3_NUM_SRATE];


/**
 * SNS Quantization
 */

extern const float lc3_sns_lfcb[32][8];
extern const float lc3_sns_hfcb[32][8];

struct lc3_sns_vq_gains {
    int count; const float *v;
};

extern const struct lc3_sns_vq_gains lc3_sns_vq_gains[4];

extern const int32_t lc3_sns_mpvq_offsets[][11];


/**
 * TNS Arithmetic Coding
 */

extern const struct lc3_ac_model lc3_tns_order_models[];
extern const uint16_t lc3_tns_order_bits[][8];

extern const struct lc3_ac_model lc3_tns_coeffs_models[];
extern const uint16_t lc3_tns_coeffs_bits[][17];


/**
 * Long Term Postfilter
 */

extern const float *lc3_ltpf_cnum[LC3_NUM_SRATE][4];
extern const float *lc3_ltpf_cden[LC3_NUM_SRATE][4];


/**
 * Spectral Data Arithmetic Coding
 */

extern const uint8_t lc3_spectrum_lookup[2][2][256][4];
extern const struct lc3_ac_model lc3_spectrum_models[];
extern const uint16_t lc3_spectrum_bits[][17];


#endif /* __LC3_TABLES_H */
