/*
 * SnsQuantizationTables.hpp
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

#ifndef SNS_QUANTIZATION_TABLES_H_
#define SNS_QUANTIZATION_TABLES_H_

// LC3 Specification d09r01.pdf
// Section 5.7.3 SNS Quantization

// LC3 Specification d09r04_*implementorComments*
// Section 3.7.4 SNS Quantization

extern double LFCB[32][8];
extern double HFCB[32][8];
extern double sns_vq_reg_adj_gains[2];
extern double sns_vq_reg_lf_adj_gains[4];
extern double sns_vq_near_adj_gains[4];
extern double sns_vq_far_adj_gains[8];
extern int sns_gainMSBbits[4];
extern int sns_gainLSBbits[4];
extern unsigned int MPVQ_offsets[16][1+10];
extern double D[16][16];


#endif // SNS_QUANTIZATION_TABLES_H_
