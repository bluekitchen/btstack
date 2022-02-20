/*
 * Lc3Decoder.cpp
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

#include "Lc3Decoder.hpp"
#include "DecoderTop.hpp"
#include <cstring>

using namespace Lc3Dec;

Lc3Decoder::Lc3Decoder(uint16_t Fs, Lc3Config::FrameDuration frameDuration)
    :
    Lc3Decoder(Lc3Config(Fs,frameDuration, 1))
{
}

Lc3Decoder::Lc3Decoder(Lc3Config lc3Config_, uint8_t bits_per_audio_sample_dec_, uint16_t byte_count_max_dec_, void* datapoints)
    :
    lc3Config(lc3Config_),
    bits_per_audio_sample_dec(bits_per_audio_sample_dec_),
    byte_count_max_dec( (byte_count_max_dec_ < 400) ? byte_count_max_dec_ : 400 ),
    decoderList(lc3Config.Nc)
{
    // proceed only with valid configuration
    if ( lc3Config.isValid() )
    {
        for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
        {
            decoderList[channelNr] = new DecoderTop(lc3Config, reinterpret_cast<DatapointContainer*>(datapoints) );
        }
    }
}

Lc3Decoder::~Lc3Decoder()
{
    for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
    {
        DecoderTop* decoderTop = decoderList[channelNr];
        if (nullptr != decoderTop)
        {
            delete decoderTop;
        }
    }
}

uint8_t Lc3Decoder::run(const uint8_t *bytes, uint16_t byte_count, uint8_t BFI,
        int16_t* x_out, uint16_t x_out_size, uint8_t& BEC_detect, uint8_t channelNr)
{
    if (!lc3Config.isValid())
    {
        return INVALID_CONFIGURATION;
    }
    if ( (byte_count < 20) || (byte_count > byte_count_max_dec) )
    {
        return INVALID_BYTE_COUNT;
    }
    if ( lc3Config.NF != x_out_size )
    {
        return INVALID_X_OUT_SIZE;
    }
    if ( bits_per_audio_sample_dec != 16 )
    {
        return INVALID_BITS_PER_AUDIO_SAMPLE;
    }
    if (nullptr==decoderList[channelNr])
    {
        return DECODER_ALLOCATION_ERROR;
    }

    decoderList[channelNr]->run<int16_t>(
            bytes, byte_count,
            BFI,
            bits_per_audio_sample_dec, x_out,
            BEC_detect);
    return ERROR_FREE;
}

uint8_t Lc3Decoder::run(const uint8_t *bytes, uint16_t byte_count, uint8_t BFI,
        int32_t* x_out, uint16_t x_out_size, uint8_t& BEC_detect, uint8_t channelNr)
{
    if (!lc3Config.isValid())
    {
        return INVALID_CONFIGURATION;
    }
    if ( (byte_count < 20) || (byte_count > byte_count_max_dec) )
    {
        return INVALID_BYTE_COUNT;
    }
    if ( lc3Config.NF != x_out_size )
    {
        return INVALID_X_OUT_SIZE;
    }
    if ( ( bits_per_audio_sample_dec != 16 ) && ( bits_per_audio_sample_dec != 24 ) && ( bits_per_audio_sample_dec != 32 ) )
    {
        return INVALID_BITS_PER_AUDIO_SAMPLE;
    }
    if (nullptr==decoderList[channelNr])
    {
        return DECODER_ALLOCATION_ERROR;
    }

    decoderList[channelNr]->run<int32_t>(
            bytes, byte_count,
            BFI,
            bits_per_audio_sample_dec, x_out,
            BEC_detect);
    return ERROR_FREE;
}

uint8_t Lc3Decoder::run(const uint8_t* bytes, const uint16_t* byte_count_per_channel, const uint8_t* BFI_per_channel,
        int16_t* x_out, uint32_t x_out_size, uint8_t* BEC_detect_per_channel)
{
    if (!lc3Config.isValid())
    {
        return INVALID_CONFIGURATION;
    }
    if ( lc3Config.NF * lc3Config.Nc != x_out_size )
    {
        return INVALID_X_OUT_SIZE;
    }

    uint8_t returnCode = ERROR_FREE;
    uint32_t byteOffset = 0;
    for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
    {
        // Note: bitwise or of the single channel return code will not allow uniquely to decode
        //       the given error. The idea is to catch any error. This decision makes the API
        //       more simple. However, when the precise error code is needed, the single channel call
        //       has to be made separately.
        returnCode |= run(&bytes[byteOffset], byte_count_per_channel[channelNr], BFI_per_channel[channelNr],
                          &x_out[channelNr*lc3Config.NF], lc3Config.NF, BEC_detect_per_channel[channelNr], channelNr);
        byteOffset += byte_count_per_channel[channelNr];
    }
    return returnCode;
}

uint8_t Lc3Decoder::run(const uint8_t* bytes, const uint16_t* byte_count_per_channel, const uint8_t* BFI_per_channel,
        int32_t* x_out, uint32_t x_out_size, uint8_t* BEC_detect_per_channel)
{
    if (!lc3Config.isValid())
    {
        return INVALID_CONFIGURATION;
    }
    if ( lc3Config.NF * lc3Config.Nc != x_out_size )
    {
        return INVALID_X_OUT_SIZE;
    }

    uint8_t returnCode = ERROR_FREE;
    uint32_t byteOffset = 0;
    for (uint8_t channelNr=0; channelNr < lc3Config.Nc; channelNr++)
    {
        // Note: bitwise or of the single channel return code will not allow uniquely to decode
        //       the given error. The idea is to catch any error. This decision makes the API
        //       more simple. However, when the precise error code is needed, the single channel call
        //       has to be made separately.
        returnCode |= run(&bytes[byteOffset], byte_count_per_channel[channelNr], BFI_per_channel[channelNr],
                          &x_out[channelNr*lc3Config.NF], lc3Config.NF, BEC_detect_per_channel[channelNr], channelNr);
        byteOffset += byte_count_per_channel[channelNr];
    }
    return returnCode;
}
