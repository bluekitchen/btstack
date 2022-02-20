/*
 * BandIndexTables.hpp
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

#ifndef BAND_INDEX_TABLES_H_
#define BAND_INDEX_TABLES_H_

// LC3 Specification d09r01.pdf
// Section 5.7.1 Band Tables Index I_fs

// LC3 Specification d09r04_*implementorComments*
// Section 3.7.1 Band tables index I_fs for 10 ms frame duration

extern int I_8000[65];
extern int I_16000[65];
extern int I_24000[65];
extern int I_32000[65];
extern int I_48000[65];

// LC3 Specification d09r04_*implementorComments*
// Section 3.7.2 Band tables index I_fs for 7.5 ms frame duration

extern int I_8000_7p5ms[61];
extern int I_16000_7p5ms [65];
extern int I_24000_7p5ms [65];
extern int I_32000_7p5ms [65];
extern int I_48000_7p5ms [65];

#endif // BAND_INDEX_TABLES_H_
