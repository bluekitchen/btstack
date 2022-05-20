/*
 * DecoderTop.cpp
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

#include "DecoderTop.hpp"

namespace Lc3Dec
{

DecoderTop::DecoderTop(uint16_t fs_, DatapointContainer* datapoints_)
    :
    DecoderTop(Lc3Config(fs_,Lc3Config::FrameDuration::d10ms,1), datapoints_)
{
}

DecoderTop::DecoderTop(Lc3Config lc3Config_, DatapointContainer* datapoints_)
    :
    lc3Config(lc3Config_),
    datapoints(datapoints_),
    decoderCurrentFrame(nullptr),
    decoderPreviousFrame(nullptr),
    residualSpectrum(lc3Config_.NE),
    spectralNoiseShaping(lc3Config),
    packetLossConcealment(lc3Config_.NE),
    mdctDec(lc3Config)
{
    if (nullptr != datapoints)
    {
        residualSpectrum.registerDatapoints( datapoints );
        spectralNoiseShaping.registerDatapoints( datapoints );
        packetLossConcealment.registerDatapoints( datapoints );
        mdctDec.registerDatapoints( datapoints );
    }
}

DecoderTop::~DecoderTop()
{
    if (nullptr != decoderPreviousFrame)
    {
        delete decoderPreviousFrame;
    }
    if (nullptr != decoderCurrentFrame)
    {
        delete decoderCurrentFrame;
    }
}


}//namespace Lc3Dec
