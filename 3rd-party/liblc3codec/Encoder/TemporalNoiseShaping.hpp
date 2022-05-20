/*
 * TemporalNoiseShaping.hpp
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

#ifndef TEMPORAL_NOISE_SHAPING_H_
#define TEMPORAL_NOISE_SHAPING_H_

#include <cstdint>
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class TemporalNoiseShaping
{
    public:
        TemporalNoiseShaping(const Lc3Config& lc3Config_);
        virtual ~TemporalNoiseShaping();

        void registerDatapoints(DatapointContainer* datapoints_);

        void run(const double* const X_S, uint8_t P_BW, uint16_t nbits, uint8_t near_nyquist_flag);

        static int8_t nint(double x);

        const Lc3Config& lc3Config;

        uint16_t nbits_TNS;
        double* X_f;
        uint8_t tns_lpc_weighting;
        uint8_t num_tns_filters;
        uint8_t rc_order[2];
        uint8_t rc_i[2*8];

    private:
        double rc_q[2*8];
        DatapointContainer* datapoints;
};

}//namespace Lc3Enc

#endif // TEMPORAL_NOISE_SHAPING_H_
