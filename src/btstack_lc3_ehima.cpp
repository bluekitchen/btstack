/*
 * Copyright (C) 2022 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define BTSTACK_FILE__ "btstack_lc3_ehima.c"

/**
 * @title LC3 EHIMA Wrapper
 */

#include "btstack_config.h"
#include "bluetooth.h"

#ifdef HAVE_LC3_EHIMA

#include "btstack_lc3_ehima.h"
#include <Lc3Config.hpp>
#include <Lc3Decoder.hpp>
#include <Lc3Encoder.hpp>

/* Decoder implementation */

static uint8_t lc3_decoder_ehima_configure(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration){
    lc3_decoder_ehima_t * instance = static_cast<lc3_decoder_ehima_t*>(context);

    // free decoder if it exists
    if (instance->decoder != NULL){
        delete instance;
        instance->decoder = NULL;
    }

    // store config
    instance->sample_rate = sample_rate;
    instance->frame_duration = frame_duration;

    // map frame duration
    Lc3Config::FrameDuration duration;
    switch (frame_duration) {
        case BTSTACK_LC3_FRAME_DURATION_7500US:
            duration = Lc3Config::FrameDuration::d7p5ms;
            break;
        case BTSTACK_LC3_FRAME_DURATION_10000US:
            duration = Lc3Config::FrameDuration::d10ms;
            break;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // create new decoder instance
    instance->decoder = new Lc3Decoder(instance->sample_rate, duration);
    return ERROR_CODE_SUCCESS;
}

static uint16_t lc3_decoder_ehima_get_number_octets_for_bitrate(void * context, uint32_t bitrate){
    lc3_decoder_ehima_t * instance = static_cast<lc3_decoder_ehima_t*>(context);
    Lc3Decoder * decoder = static_cast<Lc3Decoder*>(instance->decoder);
    return decoder->lc3Config.getByteCountFromBitrate(bitrate);
}

static uint16_t lc3_decoder_ehima_get_number_samples_per_frame(void * context){
    lc3_decoder_ehima_t * instance = static_cast<lc3_decoder_ehima_t*>(context);
    Lc3Decoder * decoder = static_cast<Lc3Decoder*>(instance->decoder);
    return decoder->lc3Config.NF;
}

static uint8_t lc3_decoder_ehima_decode(void * context, const uint8_t *bytes, uint16_t byte_count, uint8_t BFI, int16_t* pcm_out, uint16_t stride, uint8_t * BEC_detect){
    (void)stride;
    lc3_decoder_ehima_t * instance = static_cast<lc3_decoder_ehima_t*>(context);
    Lc3Decoder * decoder = static_cast<Lc3Decoder*>(instance->decoder);
    uint8_t res = decoder->run(bytes, byte_count, BFI, pcm_out, decoder->lc3Config.NF, *BEC_detect);
    return ERROR_CODE_SUCCESS;
}

static const btstack_lc3_decoder_t l3c_decoder_ehima_instance = {
    lc3_decoder_ehima_configure,
    lc3_decoder_ehima_get_number_octets_for_bitrate,
    lc3_decoder_ehima_get_number_samples_per_frame,
    lc3_decoder_ehima_decode
};

const btstack_lc3_decoder_t * lc3_decoder_ehima_init_instance(lc3_decoder_ehima_t * context){
    memset(context, 0, sizeof(lc3_decoder_ehima_t));
    return &l3c_decoder_ehima_instance;
}

/* Encoder implementation */

static uint8_t lc3_encoder_ehima_configure(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration){
    lc3_encoder_ehima_t * instance = static_cast<lc3_encoder_ehima_t*>(context);

    // free decoder if it exists
    if (instance->encoder != NULL){
        delete instance;
        instance->encoder = NULL;
    }

    // store config
    instance->sample_rate = sample_rate;
    instance->frame_duration = frame_duration;

    // map frame duration
    Lc3Config::FrameDuration duration;
    switch (frame_duration) {
        case BTSTACK_LC3_FRAME_DURATION_7500US:
            duration = Lc3Config::FrameDuration::d7p5ms;
            break;
        case BTSTACK_LC3_FRAME_DURATION_10000US:
            duration = Lc3Config::FrameDuration::d10ms;
            break;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // create config instance
    Lc3Config lc3_encoder_config(sample_rate, duration, 1);

    // create new decoder instance
    instance->encoder = new Lc3Encoder(lc3_encoder_config);
    return ERROR_CODE_SUCCESS;
}

static uint32_t lc3_encoder_ehima_get_bitrate_for_number_of_octets(void * context, uint16_t number_of_octets){
    lc3_encoder_ehima_t * instance = static_cast<lc3_encoder_ehima_t*>(context);
    Lc3Encoder * encoder = static_cast<Lc3Encoder*>(instance->encoder);
    return encoder->lc3Config.getBitrateFromByteCount(number_of_octets);
}

static uint16_t lc3_encoder_ehima_get_number_samples_per_frame(void * context){
    lc3_encoder_ehima_t * instance = static_cast<lc3_encoder_ehima_t*>(context);
    Lc3Encoder * encoder = static_cast<Lc3Encoder*>(instance->encoder);
    return encoder->lc3Config.NF;
}

static uint8_t lc3_encoder_ehima_encode(void * context, const int16_t* pcm_in, uint16_t stride, uint8_t *bytes, uint16_t byte_count){
    (void)stride;
    lc3_encoder_ehima_t * instance = static_cast<lc3_encoder_ehima_t*>(context);
    Lc3Encoder * encoder = static_cast<Lc3Encoder*>(instance->encoder);
    uint8_t res = encoder->run(pcm_in, byte_count, bytes);
    return ERROR_CODE_SUCCESS;
}

static const btstack_lc3_encoder_t l3c_encoder_ehima_instance = {
        lc3_encoder_ehima_configure,
        lc3_encoder_ehima_get_bitrate_for_number_of_octets,
        lc3_encoder_ehima_get_number_samples_per_frame,
        lc3_encoder_ehima_encode
};

const btstack_lc3_encoder_t * lc3_encoder_ehima_init_instance(lc3_encoder_ehima_t * context){
    memset(context, 0, sizeof(lc3_decoder_ehima_t));
    return &l3c_encoder_ehima_instance;
}

/* Datapoints implementation needed */

#include "Datapoints.hpp"
#include <iomanip>
#include <sstream>
#include <iostream>

class ContainerRealization
{
};

DatapointContainer::DatapointContainer() : m_pContainer(nullptr)
{
    // nothing stored yet; we may integrate a complete datapoint container when more debugging
    // is needed in the android context
    (void) m_pContainer;
}

DatapointContainer::~DatapointContainer()
{
}

void DatapointContainer::addDatapoint(const char* label, Datapoint* pDatapoint)
{
}

void DatapointContainer::addDatapoint(const char* label, void* pData, uint16_t sizeInBytes)
{
}

void DatapointContainer::addDatapoint(const char* label, const void* pData, uint16_t sizeInBytes)
{
}

void DatapointContainer::log(const char* label, const void* pData,  uint16_t sizeInBytes)
{
    // Note: this is just a first simple step to demonstrate the possibilities we have.
    //   In this case the datapoint label its size in bytes and its content as hex-byte-stream
    //   is send to cout.
    const uint8_t* pByteData = reinterpret_cast<const uint8_t*>(pData);
    std::string valueAsHexString("0x");
    std::ostringstream vStream;
    vStream << "0x";
    for (uint16_t byteNr=0; byteNr < sizeInBytes; byteNr++)
    {
        vStream << std::right
                << std::setw(2)
                << std::setfill('0')
                << std::hex
                << static_cast<int>(pByteData[byteNr]);
    }
    std::cout << label << "[" << sizeInBytes << "]:" << vStream.str();
}


uint16_t DatapointContainer::getDatapointSize(const char* label)
{
    return 0;
}

bool DatapointContainer::getDatapointValue(const char* label, void* pDataBuffer, uint16_t bufferSize)
{
    return false;
}

bool DatapointContainer::setDatapointValue(const char* label, const void* pDataBuffer, uint16_t bufferSize)
{
    return false;
}

#endif

