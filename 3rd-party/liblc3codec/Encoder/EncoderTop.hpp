/*
 * EncoderTop.hpp
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

#ifndef __ENCODER_TOP_HPP_
#define __ENCODER_TOP_HPP_

#include <cstdint>
#include "Lc3Config.hpp"
#include "EncoderFrame.hpp"
#include "MdctEnc.hpp"
#include "AttackDetector.hpp"
#include "SpectralNoiseShaping.hpp"
#include "TemporalNoiseShaping.hpp"
#include "SpectralQuantization.hpp"
#include "NoiseLevelEstimation.hpp"
#include "Datapoints.hpp"

namespace Lc3Enc
{

class EncoderTop
{
    public:
        EncoderTop(uint16_t fs_, DatapointContainer* datapoints_=nullptr);
        EncoderTop(Lc3Config lc3Config_, DatapointContainer* datapoints_=nullptr);
        virtual ~EncoderTop();

        void run(const int16_t* x_s, uint16_t byte_count, uint8_t* bytes);

        const Lc3Config lc3Config;

    private:
        DatapointContainer* datapoints;
        EncoderFrame* encoderCurrentFrame;
        EncoderFrame* encoderPreviousFrame;
        MdctEnc mdctEnc;
        AttackDetector attackDetector;
        SpectralNoiseShaping spectralNoiseShaping;
        TemporalNoiseShaping temporalNoiseShaping;
        SpectralQuantization spectralQuantization;
        NoiseLevelEstimation noiseLevelEstimation;
        uint32_t frameCount;
};

}//namespace Lc3Enc

#endif // __ENCODER_TOP_HPP_
