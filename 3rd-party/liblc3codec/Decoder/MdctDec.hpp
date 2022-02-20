/*
 * MdctDec.hpp
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

#ifndef MDCT_DEC_H_
#define MDCT_DEC_H_

#include <cstdint>
#include "DctIV.hpp"
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Dec
{

class MdctDec
{
    public:
        MdctDec(const Lc3Config& lc3Config_);
        virtual ~MdctDec();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(const double* const X_hat);

        const Lc3Config& lc3Config;
        double* x_hat_mdct;

    private:
        void MdctInvFastDbl();

        DctIVDbl dctIVDbl;
        double* mem_ola_add;
        double* t_hat_mdct;
        double* wN;
};

}//namespace Lc3Dec

#endif // MDCT_DEC_H_
