/*
 * DctIV.hpp
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

#ifndef DCT_IV_H_
#define DCT_IV_H_

#include <cstdint>

class DctIVDbl
{
    public:
        DctIVDbl(uint16_t NF_);
        virtual ~DctIVDbl();

        void run();

        const uint16_t NF;
        double* in;
        double* out;
        void* dctIVconfig;
};

#endif // DCT_IV_H_
