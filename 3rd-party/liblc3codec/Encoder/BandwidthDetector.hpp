/*
 * BandwidthDetector.hpp
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

#ifndef BANDWIDTH_DETECTOR_H_
#define BANDWIDTH_DETECTOR_H_

#include <cstdint>
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class BandwidthDetector
{
    public:
        BandwidthDetector(const Lc3Config& lc3Config_);
        virtual ~BandwidthDetector();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(const double* const E_B);  // Note: fs-dependent N_b not needed since processing occurs only for fs>8000

        static uint8_t calc_N_bw(uint8_t fs_ind);

        const Lc3Config& lc3Config;
        const uint8_t N_bw;

        uint8_t nbits_bw;
        uint8_t P_bw;

    private:
        const uint8_t* I_bw_start;
        const uint8_t* I_bw_stop;
        const uint8_t* L;
};

}//namespace Lc3Enc

#endif // BANDWIDTH_DETECTOR_H_
