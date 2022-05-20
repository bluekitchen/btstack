/*
 * LongTermPostfilter.hpp
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

#ifndef LONG_TERM_POSTFILTER_H_
#define LONG_TERM_POSTFILTER_H_

#include <cstdint>
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Dec
{

class LongTermPostfilter
{
    public:
        LongTermPostfilter(const Lc3Config& lc3Config_, uint16_t nbits);
        virtual ~LongTermPostfilter();

        void registerDatapoints(DatapointContainer* datapoints);

        LongTermPostfilter& operator= ( const LongTermPostfilter & );

        void setInputX(const double* const x_hat);
        void run(int16_t ltpf_active, int16_t pitch_index);

        const Lc3Config& lc3Config;
        const uint8_t numMemBlocks;
        double* x_hat_ltpf;

    private:
        void setGainParams(uint16_t nbits);
        void computeFilterCoeffs(uint16_t pitch_index);

        int16_t ltpf_active_prev;
        uint16_t blockStartIndex;
        double *c_num;
        double *c_den;
        double *c_num_mem;
        double *c_den_mem;
        double* x_hat_ltpfin;
        double* x_hat_mem;
        double* x_hat_ltpf_mem;

        uint8_t L_num;
        uint8_t L_den;

        double gain_ltpf;
        uint8_t gain_ind;

        uint16_t p_int;
        uint16_t p_fr;
        uint16_t p_int_mem;
        uint16_t p_fr_mem;

};

}//namespace Lc3Dec

#endif // LONG_TERM_POSTFILTER_H_
