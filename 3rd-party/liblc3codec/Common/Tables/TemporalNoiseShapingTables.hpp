/*
 * TemporalNoiseShapingTables.hpp
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
// Section 5.7.4 Temporal Noise Shaping

#ifndef TEMPORAL_NOISE_SHAPING_TABLES_H_
#define TEMPORAL_NOISE_SHAPING_TABLES_H_

// LC3 Specification d09r01.pdf; Page 109 of 177
extern short ac_tns_order_bits[2][8];
extern short ac_tns_order_freq[2][8];
extern short ac_tns_order_cumfreq[2][8];

extern short ac_tns_coef_bits[8][17];
extern short ac_tns_coef_freq[8][17];
extern short ac_tns_coef_cumfreq[8][17];

#endif // TEMPORAL_NOISE_SHAPING_TABLES_H_
