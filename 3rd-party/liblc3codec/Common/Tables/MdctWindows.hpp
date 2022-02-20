/*
 * MdctWindows.cpp
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


#ifndef MDCT_WINDOWS_H_
#define MDCT_WINDOWS_H_

// LC3 Specification d09r01.pdf
// Section 5.7.2

// LC3 Specification d09r04_*implementorComments*
// Section 3.7.3 Low delay MDCT windows
// Section 3.7.3.1 10 ms Frame Duration

extern double w_N80[160];
extern double w_N160[320];
extern double w_N240[480];
extern double w_N320[640];
extern double w_N480[960];

// Section 3.7.3.2 7.5 ms Frame Duration

extern double w_N60_7p5ms[120];
extern double w_N120_7p5ms[240];
extern double w_N180_7p5ms[360];
extern double w_N240_7p5ms[480];
extern double w_N360_7p5ms[720];


#endif // MDCT_WINDOWS_H_
