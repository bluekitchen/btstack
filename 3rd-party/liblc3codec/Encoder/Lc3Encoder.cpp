/*
 * Lc3Encoder.cpp
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

#include "Lc3Encoder.hpp"
#include "EncoderTop.hpp"
#include <cstring>

using namespace Lc3Enc;

Lc3Encoder::Lc3Encoder(uint16_t Fs)
    :
    Lc3Encoder(Lc3Config(Fs,Lc3Config::FrameDuration::d10ms, 1))
{

}

Lc3Encoder::Lc3Encoder(Lc3Config lc3Config_, uint8_t bits_per_audio_sample_enc_, void* datapoints)
    :
    lc3Config(lc3Config_),
    bits_per_audio_sample_enc(bits_per_audio_sample_enc_),
    encoderList(lc3Config.Nc)
{
    // proceed only with valid configuration
    if ( lc3Config.isValid() && (16==bits_per_audio_sample_enc) )
    {
        for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
        {
            encoderList[channelNr] = new EncoderTop(lc3Config, reinterpret_cast<DatapointContainer*>(datapoints) );
        }
    }
}

Lc3Encoder::~Lc3Encoder()
{
    for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
    {
        EncoderTop* encoderTop = encoderList[channelNr];
        if (nullptr != encoderTop)
        {
            delete encoderTop;
        }
    }
}

uint8_t Lc3Encoder::run(const int16_t* x_s, uint16_t byte_count, uint8_t* bytes, uint8_t channelNr)
{
    if (!lc3Config.isValid())
    {
        return INVALID_CONFIGURATION;
    }
    if ( (byte_count < 20) || (byte_count>400) )
    {
        return INVALID_BYTE_COUNT;
    }
    if ( bits_per_audio_sample_enc != 16 )
    {
        return INVALID_BITS_PER_AUDIO_SAMPLE;
    }
    if (nullptr==encoderList[channelNr])
    {
        return ENCODER_ALLOCATION_ERROR;
    }

    encoderList[channelNr]->run(x_s, byte_count, bytes);
    return ERROR_FREE;
}

uint8_t Lc3Encoder::run(const int16_t* x_s, const uint16_t* byte_count_per_channel, uint8_t* bytes)
{
    if (!lc3Config.isValid())
    {
        return INVALID_CONFIGURATION;
    }

    uint8_t returnCode = ERROR_FREE;
    uint32_t byteOffset = 0;
    for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
    {
        // Note: bitwise or of the single channel return code will not allow uniquely to decode
        //       the given error. The idea is to catch any error. This decision makes the API
        //       more simple. However, when the precise error code is needed, the single channel call
        //       has to be made separately.
        returnCode |= run(&x_s[channelNr*lc3Config.NF], byte_count_per_channel[channelNr], &bytes[byteOffset], channelNr);
        byteOffset += byte_count_per_channel[channelNr];
    }
    return returnCode;
}

