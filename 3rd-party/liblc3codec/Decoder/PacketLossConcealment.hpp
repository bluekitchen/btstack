/*
 * PacketLossConcealment.hpp
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

#ifndef PACKET_LOSS_CONCEALMENT_H_
#define PACKET_LOSS_CONCEALMENT_H_

#include <cstdint>
#include "Datapoints.hpp"

namespace Lc3Dec
{

class PacketLossConcealment
{
    public:
        PacketLossConcealment(uint16_t NE_);
        virtual ~PacketLossConcealment();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(uint8_t BER_DETECT, double* X_hat, int16_t& ltpf_active);

        const uint16_t NE;

    private:
        uint16_t plc_seed;
        uint8_t nbLostCmpt;
        double alpha;
        double* X_hat_lastGood;
};

}//namespace Lc3Dec

#endif // PACKET_LOSS_CONCEALMENT_H_
