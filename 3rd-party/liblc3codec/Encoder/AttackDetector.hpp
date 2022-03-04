/*
 * AttackDetector.hpp
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

#ifndef ATTACK_DETECTOR_H_
#define ATTACK_DETECTOR_H_

#include <cstdint>
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class AttackDetector
{
    public:
        AttackDetector(const Lc3Config& lc3Config_);
        virtual ~AttackDetector();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(const int16_t* const x_s, uint16_t nbytes);

        const Lc3Config& lc3Config;
        const uint8_t M_F;

        uint8_t F_att;

    private:
        double x_att_last[2];
        double E_att_last;
        double A_att_last;
        int8_t P_att_last;
};

}//namespace Lc3Enc

#endif // ATTACK_DETECTOR_H_
