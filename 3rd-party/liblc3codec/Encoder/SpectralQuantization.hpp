/*
 * SpectralQuantization.hpp
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

#ifndef SPECTRAL_QUANTIZATION_H_
#define SPECTRAL_QUANTIZATION_H_

#include <cstdint>
#include "Datapoints.hpp"

namespace Lc3Enc
{

class SpectralQuantization
{
    public:
        SpectralQuantization(uint16_t NE_, uint8_t fs_ind_);
        virtual ~SpectralQuantization();

        void registerDatapoints(DatapointContainer* datapoints_);

        void run(const double* const X_f, uint16_t nbits, uint16_t nbits_spec_local);

    private:
        void updateGlobalGainEstimationParameter(uint16_t nbits, uint16_t nbits_spec_local);
        void computeSpectralEnergy(double* E, const double* X_f);
        void globalGainEstimation(const double* E);
        bool globalGainLimitation(const double* const X_f);
        void quantizeSpectrum(const double* const X_f);
        void computeBitConsumption(
                    uint16_t nbits,
                    uint8_t& modeFlag);
        bool globalGainAdjustment();

    public:
        static const uint8_t N_b = 64;

        const uint16_t NE;
        const uint8_t fs_ind;

        int16_t* X_q;
        uint16_t lastnz;
        uint16_t nbits_lsb;
        uint16_t nbits_trunc;
        uint16_t rateFlag;
        uint8_t lsbMode;
        float gg;
        uint16_t lastnz_trunc;
        int16_t gg_ind;

    private:
        DatapointContainer* datapoints;
        int16_t gg_off;
        int16_t gg_min;
        float nbits_offset;
        uint16_t nbits_spec;
        uint16_t nbits_spec_adj;
        uint16_t nbits_est;

        // states
        bool reset_offset_old;
        float nbits_offset_old;
        uint16_t nbits_spec_old;
        uint16_t nbits_est_old;
};

}//namespace Lc3Enc

#endif // SPECTRAL_QUANTIZATION_H_
