/*
 * LongTermPostfilterCoefficients.hpp
 *
 * Copyright 2019 HIMSA II K/S - www.himsa.com. Represented by EHIMA - www.ehima.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// LC3 Specification d09r01.pdf
// Section 5.7.5 Long Term Postfiltering

#ifndef LONG_TERM_POSTFILTER_COEFFICIENTS_H_
#define LONG_TERM_POSTFILTER_COEFFICIENTS_H_

extern double tab_resamp_filter[239];
extern double tab_ltpf_interp_R[31];
extern double tab_ltpf_interp_x12k8[15];
extern double tab_ltpf_num_8000[4][3];
extern double tab_ltpf_num_16000[4][3];
extern double tab_ltpf_num_24000[4][5];
extern double tab_ltpf_num_32000[4][7];
extern double tab_ltpf_num_48000[4][11];
extern double tab_ltpf_den_8000[4][5];
extern double tab_ltpf_den_16000[4][5];
extern double tab_ltpf_den_24000[4][7];
extern double tab_ltpf_den_32000[4][9];
extern double tab_ltpf_den_48000[4][13];

#endif // LONG_TERM_POSTFILTER_COEFFICIENTS_H_
