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
#include "Lc3Config.hpp"

namespace Lc3Dec
{

class SpectralNoiseShaping
{
    public:
        SpectralNoiseShaping(const Lc3Config& lc3Config_);

        virtual ~SpectralNoiseShaping();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(
            const double* const X_s_tns,
            double* const X_hat_ss,
            int16_t ind_LF,
            int16_t ind_HF,
            int16_t submodeMSB,
            int16_t submodeLSB,
            int16_t Gind,
            int16_t LS_indA,
            int16_t LS_indB,
            int32_t idxA,
            int16_t idxB
        );

        const Lc3Config& lc3Config;

    private:
        int* I_fs;

};

}//namespace Lc3Dec

#endif // SPECTRAL_NOISE_SHAPING_H_
