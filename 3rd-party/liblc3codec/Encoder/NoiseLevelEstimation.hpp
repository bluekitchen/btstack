/*
 * NoiseLevelEstimation.hpp
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

#ifndef NOISE_LEVEL_ESTIMATION_H_
#define NOISE_LEVEL_ESTIMATION_H_

#include <cstdint>
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class NoiseLevelEstimation
{
    public:
        NoiseLevelEstimation(const Lc3Config& lc3Config_);
        virtual ~NoiseLevelEstimation();

        void registerDatapoints(DatapointContainer* datapoints_);

        void run(
                const double* const X_f,
                const int16_t* const X_q,
                uint8_t P_bw,
                double gg);

        const Lc3Config& lc3Config;
        uint8_t F_NF;

    private:
        DatapointContainer* datapoints;
};

}//namespace Lc3Enc

#endif // NOISE_LEVEL_ESTIMATION_H_
