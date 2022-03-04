/*
 * EncoderFrame.hpp
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

#ifndef __ENCODER_FRAME_HPP_
#define __ENCODER_FRAME_HPP_

#include <cstdint>
#include "Datapoints.hpp"
#include "MdctEnc.hpp"
#include "BandwidthDetector.hpp"
#include "AttackDetector.hpp"
#include "SpectralNoiseShaping.hpp"
#include "TemporalNoiseShaping.hpp"
#include "LongTermPostfilter.hpp"
#include "SpectralQuantization.hpp"
#include "NoiseLevelEstimation.hpp"
#include "BitstreamEncoding.hpp"
#include "Lc3Config.hpp"

namespace Lc3Enc
{

class EncoderFrame
{
    public:
        EncoderFrame(
            MdctEnc& mdctEnc_,
            AttackDetector& attackDetector_,
            SpectralNoiseShaping& spectralNoiseShaping_,
            TemporalNoiseShaping& temporalNoiseShaping_,
            SpectralQuantization& spectralQuantization_,
            NoiseLevelEstimation& noiseLevelEstimation_,
            const Lc3Config& lc3Config_,
            uint16_t nbytes_);
        virtual ~EncoderFrame();

        void registerDatapoints(DatapointContainer* datapoints_);

        void run(const int16_t* x_s, uint8_t* bytes);

        void linkPreviousFrame(EncoderFrame* previousFrame);

        // input parameter
        const Lc3Config& lc3Config;
        const uint16_t nbytes;

    private:
        DatapointContainer* datapoints;
        MdctEnc& mdctEnc;
        BandwidthDetector bandwidthDetector;
        AttackDetector& attackDetector;
        SpectralNoiseShaping& spectralNoiseShaping;
        TemporalNoiseShaping& temporalNoiseShaping;
        LongTermPostfilter longTermPostfilter;
        SpectralQuantization& spectralQuantization;
        NoiseLevelEstimation& noiseLevelEstimation;
        BitstreamEncoding bitstreamEncoding;


    private:
        // states & outputs
        int16_t frameN;
};

}//namespace Lc3Enc

#endif // __ENCODER_FRAME_HPP_
