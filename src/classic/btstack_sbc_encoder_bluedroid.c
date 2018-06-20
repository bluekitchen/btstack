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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define __BTSTACK_FILE__ "btstack_sbc_bludroid.c"
 
// *****************************************************************************
//
// SBC encoder based on Bluedroid library 
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_sbc.h"
#include "btstack_sbc_plc.h"

#include "sbc_encoder.h"
#include "btstack.h"

#define mSBC_SYNCWORD 0xad
#define SBC_SYNCWORD 0x9c
#define SBC_MAX_CHANNELS 2
// #define LOG_FRAME_STATUS







void btstack_sbc_encoder_init(btstack_sbc_encoder_state_t * state, btstack_sbc_mode_t mode, 
                        int blocks, int subbands, int allmethod, int sample_rate, int bitpool, int channel_mode){


    state->mode = mode;

    switch (state->mode){
        case SBC_MODE_STANDARD:
            (state->bd_encoder_state).context.s16NumOfBlocks = blocks;                          
            (state->bd_encoder_state).context.s16NumOfSubBands = subbands;                       
            (state->bd_encoder_state).context.s16AllocationMethod = allmethod;                     
            (state->bd_encoder_state).context.s16BitPool = bitpool;  
            (state->bd_encoder_state).context.mSBCEnabled = 0;
            (state->bd_encoder_state).context.s16ChannelMode = channel_mode;
            (state->bd_encoder_state).context.s16NumOfChannels = 2;
            if ((state->bd_encoder_state).context.s16ChannelMode == SBC_MONO){
                (state->bd_encoder_state).context.s16NumOfChannels = 1;
            }
            switch(sample_rate){
                case 16000: (state->bd_encoder_state).context.s16SamplingFreq = SBC_sf16000; break;
                case 32000: (state->bd_encoder_state).context.s16SamplingFreq = SBC_sf32000; break;
                case 44100: (state->bd_encoder_state).context.s16SamplingFreq = SBC_sf44100; break;
                case 48000: (state->bd_encoder_state).context.s16SamplingFreq = SBC_sf48000; break;
                default: (state->bd_encoder_state).context.s16SamplingFreq = 0; break;
            }
            break;
        case SBC_MODE_mSBC:
            (state->bd_encoder_state).context.s16NumOfBlocks    = 15;
            (state->bd_encoder_state).context.s16NumOfSubBands  = 8;
            (state->bd_encoder_state).context.s16AllocationMethod = SBC_LOUDNESS;
            (state->bd_encoder_state).context.s16BitPool   = 26;
            (state->bd_encoder_state).context.s16ChannelMode = SBC_MONO;
            (state->bd_encoder_state).context.s16NumOfChannels = 1;
            (state->bd_encoder_state).context.mSBCEnabled = 1;
            (state->bd_encoder_state).context.s16SamplingFreq = SBC_sf16000;
            break;
    }
    (state->bd_encoder_state).context.pu8Packet = (state->bd_encoder_state).sbc_packet;
    
    state->encoder_state = state;
    SBC_ENC_PARAMS * context = &((state->bd_encoder_state).context);
    SBC_Encoder_Init(context);
}


void btstack_sbc_encoder_process_data(btstack_sbc_encoder_state_t* state, int16_t * input_buffer){
    SBC_ENC_PARAMS * context = &((state->bd_encoder_state).context);
    context->ps16PcmBuffer = input_buffer;
    if (context->mSBCEnabled){
        context->pu8Packet[0] = 0xad;
    }
    SBC_Encoder(context);
}

int btstack_sbc_encoder_num_audio_frames(btstack_sbc_encoder_state_t* state){
    SBC_ENC_PARAMS * context = &((state->bd_encoder_state).context);
    return context->s16NumOfSubBands * context->s16NumOfBlocks;
}

uint8_t * btstack_sbc_encoder_sbc_buffer(btstack_sbc_encoder_state_t* state){
    SBC_ENC_PARAMS * context = &((state->bd_encoder_state).context);
    return context->pu8Packet;
}

uint16_t  btstack_sbc_encoder_sbc_buffer_length(btstack_sbc_encoder_state_t* state){
    SBC_ENC_PARAMS * context = &((state->bd_encoder_state).context);
    return context->u16PacketLength;
}
