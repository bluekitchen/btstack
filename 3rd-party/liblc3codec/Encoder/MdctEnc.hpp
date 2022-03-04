/*
 * MdctEnc.hpp
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

#ifndef MDCT_ENC_H_
#define MDCT_ENC_H_

#include <cstdint>
#include "DctIV.hpp"
#include "Datapoints.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class MdctEnc
{
    public:
        MdctEnc(const Lc3Config& lc3Config_);
        virtual ~MdctEnc();

        void registerDatapoints(DatapointContainer* datapoints);

        void run(const int16_t* const x_s);

        const int* get_I_fs() const;

        const Lc3Config& lc3Config;

        double* X;
        double* E_B;
        uint8_t near_nyquist_flag;

    private:
        void MdctFastDbl(const double* const tw);

        DctIVDbl dctIVDbl;

        uint8_t skipMdct; // just for testing; find a better solution?
        int16_t* t;
        double* wN;
        const int* I_fs;
};

}//namespace Lc3Enc

#endif // MDCT_ENC_H_
