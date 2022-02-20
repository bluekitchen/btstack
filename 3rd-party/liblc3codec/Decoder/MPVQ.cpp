/*
 * MPVQ.cpp
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

#include "MPVQ.hpp"
#include <cstdbool>

#include "SnsQuantizationTables.hpp"

namespace Lc3Dec
{

// declare local helper functions
void mind2vec_tab ( short dim_in, /* i: dimension */
               short k_max_local, /* i: nb unit pulses */
                short leading_sign, /* i: leading sign */
                unsigned int ind, /* i: MPVQ-index */
                short *vec_out, /* o: pulse train */
                unsigned int MPVQ_offsets [][11] /* i: offset matrix */
            );

void mind2vec_one( short k_val_in, /* i: nb unit pulses */
                short leading_sign, /* i: leading sign -1, 1 */
                short *vec_out /* o: updated pulse train */
            );


short setval_update_sign (
                        short k_delta, /* i */
                        short k_max_local_in, /* i */
                        short *leading_sign, /* i/o */
                        unsigned int *ind_in, /* i/o */
                        short *vec_out /* i/o */
                    );

short get_lead_sign(unsigned int *ind_in );


//
// Implementation
//

void MPVQdeenum(uint8_t dim_in, /* i : dimension of vec_out */
                uint8_t k_val_in, /* i : number of unit pulses */
                int16_t LS_ind, /* i : leading sign index */
                int32_t MPVQ_ind, /* i : MPVQ shape index */
                int16_t *vec_out /* o : PVQ integer pulse train */
                )
{
    for (uint8_t i=0; i < dim_in; i++)
    {
        vec_out[i] = 0;
    }
    short leading_sign = 1;
    if ( LS_ind != 0 ){
        leading_sign = -1;
    }
    mind2vec_tab ( dim_in,
                    k_val_in,
                    leading_sign,
                    MPVQ_ind,
                    vec_out,
                    MPVQ_offsets );
}

void mind2vec_tab ( short dim_in, /* i: dimension */
                short k_max_local, /* i: nb unit pulses */
                short leading_sign, /* i: leading sign */
                unsigned int ind, /* i: MPVQ-index */
                short *vec_out, /* o: pulse train */
                unsigned int MPVQ_offsets [][11] /* i: offset matrix */
                )
{
    /* init */
    unsigned int* h_row_ptr = &(MPVQ_offsets[(dim_in-1)][0]);
    short k_acc = k_max_local;
    /* loop over positions */
    for (uint8_t pos = 0; pos < dim_in; pos++)
    {
        short k_delta;
        if (ind != 0)
        {
            k_acc = k_max_local;;
            unsigned int UL_tmp_offset = h_row_ptr[k_acc];
            bool wrap_flag = (ind < UL_tmp_offset ) ;
            unsigned int UL_diff=0;
            if (!wrap_flag)
            {
                // Note: due to android build using a integer-overflow sanitizer, we have to avoid
                //       computing the following difference when ind < UL_tmp_offset
                UL_diff = ind - UL_tmp_offset;
            }
            while (wrap_flag)
            {
                k_acc--;
                wrap_flag = (ind < h_row_ptr[k_acc]);
                if (!wrap_flag)
                {
                    // Note: due to android build using a integer-overflow sanitizer, we have to avoid
                    //       computing the following difference when ind < UL_tmp_offset
                    UL_diff = ind - h_row_ptr[k_acc];
                }
            }
            ind = UL_diff;
            k_delta = k_max_local - k_acc;
        }
        else
        {
            mind2vec_one(k_max_local, leading_sign, &vec_out[pos]);
            break;
        }
        k_max_local = setval_update_sign(
                            k_delta,
                            k_max_local,
                            &leading_sign,
                            &ind,
                            &vec_out[pos]);
        h_row_ptr -= 11; /* reduce dimension in MPVQ_offsets table */
    }
}


void mind2vec_one( short k_val_in, /* i: nb unit pulses */
                short leading_sign, /* i: leading sign -1, 1 */
                short *vec_out /* o: updated pulse train */
            )
{
    short amp = k_val_in;
    if ( leading_sign < 0 )
    {
        amp = -k_val_in ;
    }
    *vec_out = amp;
}

short setval_update_sign (
                        short k_delta, /* i */
                        short k_max_local_in, /* i */
                        short *leading_sign, /* i/o */
                        unsigned int *ind_in, /* i/o; needed to change type compared to spec */
                        short *vec_out /* i/o */
                        )
{
    short k_max_local_out = k_max_local_in;
    if (k_delta != 0)
    {
        mind2vec_one(k_delta, *leading_sign, vec_out);
        *leading_sign = get_lead_sign( ind_in );
        k_max_local_out -= k_delta ;
    }
    return k_max_local_out;
}

short get_lead_sign(unsigned int *ind ) // renamed "ind_in" from spec to just "ind" (as already found by yao.wang 28.06.2019)
{
    short leading_sign = +1;
    if ( ((*ind)&0x1 ) != 0 )
    {
        leading_sign = -1;
    }
    (*ind) = (*ind >> 1);
    return leading_sign;
}

}//namespace Lc3Dec
