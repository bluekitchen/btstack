/*
 * SpectralDataTables.hpp
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
// Section 5.7.6 Spectral data

#ifndef SPECTRAL_DATA_TABLES_H_
#define SPECTRAL_DATA_TABLES_H_

extern unsigned char ac_spec_lookup[4096];
extern short ac_spec_cumfreq[64][17];
extern short ac_spec_freq[64][17];
extern short ac_spec_bits[64][17];

#endif // SPECTRAL_DATA_TABLES_H_
