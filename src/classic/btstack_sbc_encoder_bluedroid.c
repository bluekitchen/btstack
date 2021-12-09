/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_sbc_encoder_bluedroid.c"
 
// *****************************************************************************
//
// SBC encoder based on Bluedroid library 
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_sbc.h"
#include "btstack_sbc_plc.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include "sbc_encoder.h"

#define mSBC_SYNCWORD 0xad
#define SBC_SYNCWORD 0x9c
#define SBC_MAX_CHANNELS 2
// #define LOG_FRAME_STATUS


typedef struct {
    SBC_ENC_PARAMS context;
    int num_data_bytes;
    uint8_t sbc_packet[1000];
} bludroid_encoder_state_t;

static btstack_sbc_encoder_state_t * sbc_encoder_state_singleton = NULL;
static bludroid_encoder_state_t bd_encoder_state;


void btstack_sbc_encoder_init(btstack_sbc_encoder_state_t * state, btstack_sbc_mode_t mode, 
                        int blocks, int subbands, btstack_sbc_allocation_method_t allocation_method, 
                        int sample_rate, int bitpool, btstack_sbc_channel_mode_t channel_mode){

    if (sbc_encoder_state_singleton && (sbc_encoder_state_singleton != state) ){
        log_error("SBC encoder: different sbc decoder state is allready registered");
    } 
    
    sbc_encoder_state_singleton = state;

    if (!sbc_encoder_state_singleton){
        log_error("SBC encoder init: sbc state is NULL");
    }

    sbc_encoder_state_singleton->mode = mode;

    switch (sbc_encoder_state_singleton->mode){
        case SBC_MODE_STANDARD:
            bd_encoder_state.context.s16NumOfBlocks = blocks;                          
            bd_encoder_state.context.s16NumOfSubBands = subbands;                       
            bd_encoder_state.context.s16AllocationMethod = (uint8_t)allocation_method;                     
            bd_encoder_state.context.s16BitPool = bitpool;  
            bd_encoder_state.context.mSBCEnabled = 0;
            bd_encoder_state.context.s16ChannelMode = (uint8_t)channel_mode;
            bd_encoder_state.context.s16NumOfChannels = 2;
            if (bd_encoder_state.context.s16ChannelMode == SBC_MONO){
                bd_encoder_state.context.s16NumOfChannels = 1;
            }
            switch(sample_rate){
                case 16000: bd_encoder_state.context.s16SamplingFreq = SBC_sf16000; break;
                case 32000: bd_encoder_state.context.s16SamplingFreq = SBC_sf32000; break;
                case 44100: bd_encoder_state.context.s16SamplingFreq = SBC_sf44100; break;
                case 48000: bd_encoder_state.context.s16SamplingFreq = SBC_sf48000; break;
                default: bd_encoder_state.context.s16SamplingFreq = 0; break;
            }
            break;
        case SBC_MODE_mSBC:
            bd_encoder_state.context.s16NumOfBlocks    = 15;
            bd_encoder_state.context.s16NumOfSubBands  = 8;
            bd_encoder_state.context.s16AllocationMethod = SBC_LOUDNESS;
            bd_encoder_state.context.s16BitPool   = 26;
            bd_encoder_state.context.s16ChannelMode = SBC_MONO;
            bd_encoder_state.context.s16NumOfChannels = 1;
            bd_encoder_state.context.mSBCEnabled = 1;
            bd_encoder_state.context.s16SamplingFreq = SBC_sf16000;
            break;
        default:
            btstack_assert(false);
            break;
    }
    bd_encoder_state.context.pu8Packet = bd_encoder_state.sbc_packet;
    
    sbc_encoder_state_singleton->encoder_state = &bd_encoder_state;
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    SBC_Encoder_Init(context);
}


void btstack_sbc_encoder_process_data(int16_t * input_buffer){
    if (!sbc_encoder_state_singleton){
        log_error("SBC encoder: sbc state is NULL, call btstack_sbc_encoder_init to initialize it");
    }
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    context->ps16PcmBuffer = input_buffer;
    if (context->mSBCEnabled){
        context->pu8Packet[0] = 0xad;
    }
    SBC_Encoder(context);
}

int btstack_sbc_encoder_num_audio_frames(void){
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    return context->s16NumOfSubBands * context->s16NumOfBlocks;
}

uint8_t * btstack_sbc_encoder_sbc_buffer(void){
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    return context->pu8Packet;
}

uint16_t  btstack_sbc_encoder_sbc_buffer_length(void){
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    return context->u16PacketLength;
}
