/*
 * Copyright (C) 2023 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "btstack_sbc_bluedroid.c"

#include "btstack_sbc_bluedroid.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include <stdint.h>
#include "bluetooth.h"

static uint8_t btstack_sbc_encoder_bluedroid_configure(void * context, btstack_sbc_mode_t mode,
                     uint8_t blocks, uint8_t subbands, btstack_sbc_allocation_method_t allocation_method,
                     uint16_t sample_rate, uint8_t bitpool, btstack_sbc_channel_mode_t channel_mode){

    btstack_sbc_encoder_bluedroid_t * instance = (btstack_sbc_encoder_bluedroid_t *) context;

    instance->mode = mode;

    switch (instance->mode){
        case SBC_MODE_STANDARD:
            instance->params.s16NumOfBlocks = blocks;
            instance->params.s16NumOfSubBands = subbands;
            instance->params.s16AllocationMethod = (uint8_t)allocation_method;
            instance->params.s16BitPool = bitpool;
            instance->params.mSBCEnabled = 0;
            instance->params.s16ChannelMode = (uint8_t)channel_mode;
            instance->params.s16NumOfChannels = 2;
            if (instance->params.s16ChannelMode == SBC_MONO){
                instance->params.s16NumOfChannels = 1;
            }
            switch(sample_rate){
                case 16000: instance->params.s16SamplingFreq = SBC_sf16000; break;
                case 32000: instance->params.s16SamplingFreq = SBC_sf32000; break;
                case 44100: instance->params.s16SamplingFreq = SBC_sf44100; break;
                case 48000: instance->params.s16SamplingFreq = SBC_sf48000; break;
                default: instance->params.s16SamplingFreq = 0; break;
            }
            break;
        case SBC_MODE_mSBC:
            instance->params.s16NumOfBlocks    = 15;
            instance->params.s16NumOfSubBands  = 8;
            instance->params.s16AllocationMethod = SBC_LOUDNESS;
            instance->params.s16BitPool   = 26;
            instance->params.s16ChannelMode = SBC_MONO;
            instance->params.s16NumOfChannels = 1;
            instance->params.mSBCEnabled = 1;
            instance->params.s16SamplingFreq = SBC_sf16000;
            break;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    SBC_Encoder_Init(&instance->params);

    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Return number of audio frames required for one SBC packet
 * @param context
 * @note  each audio frame contains 2 sample values in stereo modes
 */
static uint16_t btstack_sbc_encoder_bluedroid_num_audio_frames(void * context){
    btstack_sbc_encoder_bluedroid_t * instance = (btstack_sbc_encoder_bluedroid_t *) context;
    return instance->params.s16NumOfSubBands * instance->params.s16NumOfBlocks;
}

static uint16_t btstack_sbc_encoder_bluedroid_sbc_buffer_length(void * context){
    btstack_sbc_encoder_bluedroid_t * instance = (btstack_sbc_encoder_bluedroid_t *) context;
    return instance->params.u16PacketLength;
}

/**
 * @brief Encode PCM data
 * @param context
 * @param pcm_in with samples in host endianess
 * @param sbc_out
 * @return status
 */
static uint8_t btstack_sbc_encoder_bluedroid_encode_signed_16(void * context, const int16_t* pcm_in, uint8_t * sbc_out){
    btstack_sbc_encoder_bluedroid_t * instance = (btstack_sbc_encoder_bluedroid_t *) context;

    instance->params.ps16PcmBuffer = (int16_t *) pcm_in;
    instance->params.pu8Packet =  sbc_out;
    if (instance->params.mSBCEnabled){
        instance->params.pu8Packet[0] = 0xad;
    }
    SBC_Encoder(&instance->params);
    return ERROR_CODE_SUCCESS;
}

static const btstack_sbc_encoder_t btstack_sbc_encoder_bluedroid = {
    .configure         = btstack_sbc_encoder_bluedroid_configure,
    .sbc_buffer_length = btstack_sbc_encoder_bluedroid_sbc_buffer_length,
    .num_audio_frames  = btstack_sbc_encoder_bluedroid_num_audio_frames,
    .encode_signed_16  = btstack_sbc_encoder_bluedroid_encode_signed_16
};

const btstack_sbc_encoder_t * btstack_sbc_encoder_bluedroid_init_instance(btstack_sbc_encoder_bluedroid_t * context){
    return &btstack_sbc_encoder_bluedroid;
}
