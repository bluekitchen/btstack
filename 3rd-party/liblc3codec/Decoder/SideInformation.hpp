/*
 * SideInformation.hpp
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

#ifndef SIDE_INFORMATION_H_
#define SIDE_INFORMATION_H_

#include <cstdint>
#include "Datapoints.hpp"

namespace Lc3Dec
{

class SideInformation
{
    public:
        SideInformation(uint16_t NF_, uint16_t NE_, uint8_t fs_ind_);

        virtual ~SideInformation();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(
            const uint8_t* bytes,
            uint16_t& bp_side,
            uint8_t& mask_side,
            int16_t& P_BW,
            int16_t& lastnz,
            uint8_t& lsbMode,
            int16_t& gg_ind,
            int16_t& num_tns_filters,
            int16_t* rc_order,
            uint8_t& pitch_present,
            int16_t& pitch_index,
            int16_t& ltpf_active,
            int16_t& F_NF,
            int16_t& ind_LF,
            int16_t& ind_HF,
            int16_t& Gind,
            int16_t& LS_indA,
            int16_t& LS_indB,
            int32_t& idxA,
            int16_t& idxB,

            uint8_t& BEC_detect
        );

        const uint16_t NF;
        const uint16_t NE;
        const uint8_t fs_ind;
        const uint8_t nbits_bw;

        int16_t submodeMSB;
        int16_t submodeLSB;

    private:
        uint8_t nbits_lastnz;

        void dec_split_st2VQ_CW(
            uint32_t cwRx, uint32_t szA, uint32_t szB,
            uint8_t& BEC_detect, int16_t& submodeLSB,
            int32_t& idxA, int32_t& idxBorGainLSB );


};

}//namespace Lc3Dec

#endif // SIDE_INFORMATION_H_
