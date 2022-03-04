/*
 * EncoderTop.cpp
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

#include "EncoderTop.hpp"

namespace Lc3Enc
{

EncoderTop::EncoderTop(uint16_t fs_, DatapointContainer* datapoints_)
    :
    EncoderTop(Lc3Config(fs_,Lc3Config::FrameDuration::d10ms,1), datapoints_)
{
}

EncoderTop::EncoderTop(Lc3Config lc3Config_, DatapointContainer* datapoints_)
    :
    lc3Config(lc3Config_),
    datapoints(datapoints_),
    encoderCurrentFrame(nullptr),
    encoderPreviousFrame(nullptr),
    mdctEnc(lc3Config),
    attackDetector(lc3Config),
    spectralNoiseShaping(lc3Config, mdctEnc.get_I_fs() ),
    temporalNoiseShaping(lc3Config),
    spectralQuantization(lc3Config_.NE, lc3Config_.Fs_ind),
    noiseLevelEstimation(lc3Config),
    frameCount(0)
{
    if (nullptr != datapoints)
    {
        datapoints->addDatapoint( "NF", &lc3Config.NF, sizeof(lc3Config.NF) );
        mdctEnc.registerDatapoints( datapoints );
        attackDetector.registerDatapoints( datapoints );
        spectralNoiseShaping.registerDatapoints( datapoints );
        temporalNoiseShaping.registerDatapoints( datapoints );
        spectralQuantization.registerDatapoints( datapoints );
        noiseLevelEstimation.registerDatapoints( datapoints );
    }
}


EncoderTop::~EncoderTop()
{
    if (nullptr != encoderPreviousFrame)
    {
        delete encoderPreviousFrame;
        encoderPreviousFrame = nullptr;
    }
    if (nullptr != encoderCurrentFrame)
    {
        delete encoderCurrentFrame;
        encoderCurrentFrame = nullptr;
    }
}

void EncoderTop::run(const int16_t* x_s, uint16_t byte_count, uint8_t* bytes)
{
    frameCount++;
    if ( (nullptr == encoderCurrentFrame) || (encoderCurrentFrame->nbytes != byte_count) )
    {
        // We need to instantiate a new EncoderFrame only
        // when byte_count has been changed. Thus, frequent
        // dynamic memory allocation can be constrained to
        // rate optimized operation.
        if (nullptr != encoderPreviousFrame)
        {
            delete encoderPreviousFrame;
        }
        encoderPreviousFrame = encoderCurrentFrame;
        encoderCurrentFrame = new EncoderFrame(
            mdctEnc,
            attackDetector,
            spectralNoiseShaping,
            temporalNoiseShaping,
            spectralQuantization,
            noiseLevelEstimation,
            lc3Config, byte_count);
        encoderCurrentFrame->linkPreviousFrame(encoderPreviousFrame);
        if (nullptr != datapoints)
        {
            // Another way to make to call optional would be to split run() into
            //  init(), registerDatapoints() & remaining run() -> we preferred the simpler
            // API and thus designed this registration being dependent on the availability of datapoints.
            encoderCurrentFrame->registerDatapoints( datapoints );
        }
    }
    encoderCurrentFrame->run(x_s, bytes);
}

}//namespace Lc3Enc
