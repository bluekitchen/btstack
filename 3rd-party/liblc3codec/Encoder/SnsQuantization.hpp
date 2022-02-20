/*
 * SnsQuantization.hpp
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

#ifndef SNS_QUANTIZATION_H_
#define SNS_QUANTIZATION_H_

#include <cstdint>
#include "Datapoints.hpp"

namespace Lc3Enc
{

class SnsQuantization
{
    public: // methods
        SnsQuantization();
        virtual ~SnsQuantization();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(const double* const scf);

    private: // methods
        void normalizeCandidate(int8_t* y, double* XQ, uint8_t N);
        void MPVQenum (
            int32_t& index,
            int32_t& lead_sign_ind,
            uint8_t dim_in, /* i : dimension of vec_in */
            int8_t vec_in[] /* i : PVQ integer pulse train */
            );
        uint32_t encPushSign(
            int8_t val,
            int32_t& next_sign_ind_in,
            int32_t index_in
            );

    public: // members
        static const uint8_t Nscales = 16;

        double scfQ[Nscales];
        uint8_t ind_LF;
        uint8_t ind_HF;
        uint8_t shape_j;
        uint8_t Gind;
        int32_t LS_indA;
        int32_t LS_indB;
        uint32_t index_joint_j;

    private: // members
        int32_t idxA;
        int32_t idxB;
        double t2rot[Nscales];
        int8_t sns_Y0[16];
        int8_t sns_Y1[10];
        int8_t sns_Y2[16];
        int8_t sns_Y3[16];
        double sns_XQ0[Nscales];
        double sns_XQ1[Nscales]; // do not minimize to 10 -> last 6 values are all zeros which is important for the inverse D-transform
        double sns_XQ2[Nscales];
        double sns_XQ3[Nscales];
        DatapointContainer* datapoints;
};

}//namespace Lc3Enc

#endif // SNS_QUANTIZATION_H_
