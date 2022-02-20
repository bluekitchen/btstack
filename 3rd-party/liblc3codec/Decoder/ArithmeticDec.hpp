/*
 * ArithmeticDec.hpp
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

#ifndef __ARITHMETIC_DEC_HPP_
#define __ARITHMETIC_DEC_HPP_

#include <cstdint>
#include "Datapoints.hpp"

namespace Lc3Dec
{

struct ac_dec_state {
    uint32_t low;
    uint32_t range;
};

class ArithmeticDec
{
    public:
        ArithmeticDec(uint16_t NF_, uint16_t NE_, uint16_t rateFlag_, uint8_t tns_lpc_weighting_);
        virtual ~ArithmeticDec();

        void registerDatapoints(DatapointContainer* datapoints);

        double rc_q(uint8_t k, uint8_t f);

        void run(
            const uint8_t* bytes,
            uint16_t& bp,
            uint16_t& bp_side,
            uint8_t& mask_side,
            int16_t& num_tns_filters,
            int16_t rc_order[],
            const uint8_t& lsbMode,
            const int16_t& lastnz,
            uint16_t nbits,
            uint8_t& BEC_detect
        );

        const uint16_t NF;
        const uint16_t NE;
        const uint16_t rateFlag;
        const uint8_t tns_lpc_weighting;

        // states & outputs
        int16_t* X_hat_q_ari;
        uint8_t* save_lev;
        int16_t nf_seed;
        int16_t rc_order_ari[2];
        int16_t rc_i[2*8]; // [max(rc_order[f])=8][max(num_tns_filters)=2]
        uint16_t nbits_residual;

    private:
        struct ac_dec_state st;
};

}//namespace Lc3Dec

#endif // __ARITHMETIC_DEC_HPP_
