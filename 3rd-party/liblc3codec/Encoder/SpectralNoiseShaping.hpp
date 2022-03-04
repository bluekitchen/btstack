/*
 * SpectralNoiseShaping.hpp
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

#ifndef SPECTRAL_NOISE_SHAPING_H_
#define SPECTRAL_NOISE_SHAPING_H_

#include <cstdint>
#include "Datapoints.hpp"
#include "SnsQuantization.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class SpectralNoiseShaping
{
    public:
        SpectralNoiseShaping(const Lc3Config& lc3Config_, const int* const I_fs_);
        virtual ~SpectralNoiseShaping();

        void registerDatapoints(DatapointContainer* datapoints_);

        void run(const double* const X, const double* E_B, bool F_att);

        uint8_t get_ind_LF();
        uint8_t get_ind_HF();
        uint8_t get_shape_j();
        uint8_t get_Gind();
        int32_t get_LS_indA();
        int32_t get_LS_indB();
        uint32_t get_index_joint_j();

        static const uint8_t N_b = 64;
        static const uint8_t Nscales = 16;
        static const uint8_t nbits_SNS = 38;

        const Lc3Config& lc3Config;
        const uint8_t g_tilt;
        double* X_S;

    private:
        const int* const I_fs;
        SnsQuantization snsQuantization;
        double scf[Nscales];
        double g_SNS[N_b];
        DatapointContainer* datapoints;
};

}//namespace Lc3Enc

#endif // SPECTRAL_NOISE_SHAPING_H_
