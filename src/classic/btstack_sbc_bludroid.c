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
 
// *****************************************************************************
//
// SBC decoder and encoder based on Bludroid library 
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "btstack_sbc.h"
#include "btstack_sbc_plc.h"

#include "oi_codec_sbc.h"
#include "oi_assert.h"

#include "sbc_encoder.h"

#include "btstack.h"

#define mSBC_SYNCWORD 0xad
#define SBC_SYNCWORD 0x9c
#define SBC_MAX_CHANNELS 2
// #define LOG_FRAME_STATUS

// *****************************************************************************
// SBC decoder start
#define DECODER_DATA_SIZE (SBC_MAX_CHANNELS*SBC_MAX_BLOCKS*SBC_MAX_BANDS * 2 + SBC_CODEC_MIN_FILTER_BUFFERS*SBC_MAX_BANDS*SBC_MAX_CHANNELS * 2)

typedef struct {
    OI_UINT32 bytes_in_frame_buffer;
    OI_CODEC_SBC_DECODER_CONTEXT decoder_context;
    
    uint8_t frame_buffer[SBC_MAX_FRAME_LEN];
    int16_t pcm_plc_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    int16_t pcm_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    uint32_t pcm_bytes;
    OI_UINT32 decoder_data[(DECODER_DATA_SIZE+3)/4]; 
    int h2_sequence_nr;
    int search_new_sync_word;
    int sync_word_found;
    int first_good_frame_found; 
} bludroid_decoder_state_t;

static btstack_sbc_decoder_state_t * sbc_decoder_state_singleton = NULL;
static bludroid_decoder_state_t bd_decoder_state;

// Testing only - START
static int plc_enabled = 1;
static int corrupt_frame_period = -1;
// Testing - STOP

// SBC decoder end 
// *****************************************************************************


// *****************************************************************************
// SBC encoder start

typedef struct {
    SBC_ENC_PARAMS context;
    int num_data_bytes;
    uint8_t sbc_packet[1000];
} bludroid_encoder_state_t;

static btstack_sbc_encoder_state_t * sbc_encoder_state_singleton = NULL;
static bludroid_encoder_state_t bd_encoder_state;

// SBC encoder start
// *****************************************************************************



// *****************************************************************************
//
// SBC decoder based on Bludroid library 
//
// *****************************************************************************

void btstack_sbc_decoder_test_disable_plc(void){
    plc_enabled = 0;
}

void btstack_sbc_decoder_test_simulate_corrupt_frames(int period){
    corrupt_frame_period = period;
}

static int find_sequence_of_zeros(const OI_BYTE *frame_data, OI_UINT32 frame_bytes, int seq_length){
    int zero_seq_count = 0;
    int i;
    for (i=0; i<frame_bytes; i++){
        if (frame_data[i] == 0) {
            zero_seq_count++;
            if (zero_seq_count >= seq_length) return zero_seq_count;
        } else {
            zero_seq_count = 0;
        }
    }
    return 0;
}

static int find_h2_syncword(const OI_BYTE *frame_data, OI_UINT32 frame_bytes, btstack_sbc_mode_t mode){
    if (mode != SBC_MODE_mSBC) return -1;
    int syncword = mSBC_SYNCWORD;
    uint8_t h2_first_byte = 0;
    uint8_t h2_second_byte = 0;
    
    int i;
    for (i=0; i<frame_bytes; i++){
        if (frame_data[i] == syncword) {
            break;
        }
        h2_first_byte = h2_second_byte;
        h2_second_byte = frame_data[i];
    }
    if (h2_first_byte != 1) return -1;

    // check if upper nibble of second byte is 0x08
    uint8_t ln = h2_second_byte & 0x0F;
    if (ln != 8) return -1;

    // check that bits 0+2 == bits 1+3
    uint8_t hn = h2_second_byte >> 4; 
    if  ( ((hn>>1) & 0x05) != (hn & 0x05) ) return -1;

    return ((hn & 0x04) >> 1) | (hn & 0x01);
}

int btstack_sbc_decoder_num_samples_per_frame(btstack_sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.nrof_blocks * decoder_state->decoder_context.common.frameInfo.nrof_subbands;
}

int btstack_sbc_decoder_num_channels(btstack_sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.nrof_channels;
}

int btstack_sbc_decoder_sample_rate(btstack_sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.frequency;
}


#ifdef OI_DEBUG
void OI_AssertFail(char* file, int line, char* reason){
    printf("AssertFail file %s, line %d, reason %s\n", file, line, reason);
}
#endif

void btstack_sbc_decoder_init(btstack_sbc_decoder_state_t * state, btstack_sbc_mode_t mode, void (*callback)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context), void * context){
    if (sbc_decoder_state_singleton && sbc_decoder_state_singleton != state ){
        log_error("SBC decoder: different sbc decoder state is allready registered");
    } 
    OI_STATUS status;
    switch (mode){
        case SBC_MODE_STANDARD:
            status = OI_CODEC_SBC_DecoderReset(&(bd_decoder_state.decoder_context), bd_decoder_state.decoder_data, sizeof(bd_decoder_state.decoder_data), 2, 1, FALSE);
            break;
        case SBC_MODE_mSBC:
            status = OI_CODEC_mSBC_DecoderReset(&(bd_decoder_state.decoder_context), bd_decoder_state.decoder_data, sizeof(bd_decoder_state.decoder_data));
            break;
    }

    if (status != 0){
        log_error("SBC decoder: error during reset %d\n", status);
    }
    
    sbc_decoder_state_singleton = state;
    
    bd_decoder_state.bytes_in_frame_buffer = 0;
    bd_decoder_state.pcm_bytes = sizeof(bd_decoder_state.pcm_data);
    bd_decoder_state.h2_sequence_nr = -1;
    bd_decoder_state.sync_word_found = 0;
    bd_decoder_state.search_new_sync_word = 0;
    if (mode == SBC_MODE_mSBC){
        bd_decoder_state.search_new_sync_word = 1;
    }
    bd_decoder_state.first_good_frame_found = 0;

    memset(state, 0, sizeof(btstack_sbc_decoder_state_t));
    state->handle_pcm_data = callback;
    state->mode = mode;
    state->context = context;
    state->decoder_state = &bd_decoder_state;
    btstack_sbc_plc_init(&state->plc_state);
}

static void append_received_sbc_data(bludroid_decoder_state_t * state, uint8_t * buffer, int size){
    int numFreeBytes = sizeof(state->frame_buffer) - state->bytes_in_frame_buffer;

    if (size > numFreeBytes){
        log_error("SBC data: more bytes read %u than free bytes in buffer %u", size, numFreeBytes);
    }

    memcpy(state->frame_buffer + state->bytes_in_frame_buffer, buffer, size);
    state->bytes_in_frame_buffer += size;
}

void btstack_sbc_decoder_process_data(btstack_sbc_decoder_state_t * state, int packet_status_flag, uint8_t * buffer, int size){

    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t*)state->decoder_state;
    int bytes_to_process = size;
    int msbc_frame_size = 57; 

    // printf("<<-- enter -->>\n");
    // printf("Process data: in buffer %u, new %u\n", decoder_state->bytes_in_frame_buffer, size);

    while (bytes_to_process > 0){

        int bytes_missing_for_complete_msbc_frame = msbc_frame_size - decoder_state->bytes_in_frame_buffer;
        int bytes_to_append = btstack_min(bytes_to_process, bytes_missing_for_complete_msbc_frame);
        
        append_received_sbc_data(decoder_state, buffer, bytes_to_append);
        // printf("Append %u bytes, now %u in buffer \n", bytes_to_append, decoder_state->bytes_in_frame_buffer);
        buffer           += bytes_to_append;
        bytes_to_process -= bytes_to_append;
        
        if (decoder_state->bytes_in_frame_buffer < msbc_frame_size){
            // printf("not enough data %d > %d\n", msbc_frame_size, decoder_state->bytes_in_frame_buffer);
            if (bytes_to_process){
                printf("SHOULD NOT HAPPEN... not enough bytes, but bytes left to process\n");
            }
            break;
        }
        
        uint16_t bytes_in_buffer_before = decoder_state->bytes_in_frame_buffer;
        uint16_t bytes_processed = 0;
        const OI_BYTE *frame_data = decoder_state->frame_buffer;

        static int frame_count = 0;
        if (corrupt_frame_period > 0){
           frame_count++;

            if (frame_count % corrupt_frame_period == 0){
                *(uint8_t*)&frame_data[5] = 0;
                frame_count = 0;
            }
        }

        OI_STATUS status = 0;
        int bad_frame = 0;
        int zero_seq_found = 0;

        if (decoder_state->first_good_frame_found){
            zero_seq_found = find_sequence_of_zeros(frame_data, decoder_state->bytes_in_frame_buffer, 20);
            bad_frame = zero_seq_found || packet_status_flag;
        } 

        if (bad_frame){
            status = OI_CODEC_SBC_CHECKSUM_MISMATCH;
            decoder_state->bytes_in_frame_buffer = 0;
        } else {
            if (decoder_state->search_new_sync_word && !decoder_state->sync_word_found){
                int h2_syncword = find_h2_syncword(frame_data, decoder_state->bytes_in_frame_buffer, state->mode);
            
                if (h2_syncword != -1){
                    decoder_state->sync_word_found = 1;
                    decoder_state->h2_sequence_nr = h2_syncword;
                }
            }
            status = OI_CODEC_SBC_DecodeFrame(&(decoder_state->decoder_context), 
                                                &frame_data, 
                                                &(decoder_state->bytes_in_frame_buffer), 
                                                decoder_state->pcm_plc_data, 
                                                &(decoder_state->pcm_bytes));
        }        
    
        bytes_processed = bytes_in_buffer_before - decoder_state->bytes_in_frame_buffer;

        switch(status){
            case 0:
#ifdef LOG_FRAME_STATUS                    
                printf("%d : OK\n", decoder_state->h2_sequence_nr);
                if (decoder_state->h2_sequence_nr == 3) printf("\n");
#endif
                decoder_state->first_good_frame_found = 1;
                
                if (state->mode == SBC_MODE_mSBC){
                    decoder_state->search_new_sync_word = 1;
                    decoder_state->sync_word_found = 0;
                }
                
                btstack_sbc_plc_good_frame(&state->plc_state, decoder_state->pcm_plc_data, decoder_state->pcm_data);
                state->handle_pcm_data(decoder_state->pcm_data, 
                                    btstack_sbc_decoder_num_samples_per_frame(state), 
                                    btstack_sbc_decoder_num_channels(state), 
                                    btstack_sbc_decoder_sample_rate(state), state->context);
                state->good_frames_nr++;
                continue;
            case OI_CODEC_SBC_NOT_ENOUGH_HEADER_DATA:
            case OI_CODEC_SBC_NOT_ENOUGH_BODY_DATA:
                // printf("    NOT_ENOUGH_DATA\n");
                if (decoder_state->sync_word_found){
                    decoder_state->search_new_sync_word = 0;
                }
                break;
            case OI_CODEC_SBC_NO_SYNCWORD:
            case OI_CODEC_SBC_CHECKSUM_MISMATCH:
                // printf("NO_SYNCWORD or CHECKSUM_MISMATCH\n");
                decoder_state->bytes_in_frame_buffer = 0;
                if (!decoder_state->first_good_frame_found) break;

                if (state->mode == SBC_MODE_mSBC){
                    if (!decoder_state->sync_word_found){
                        decoder_state->h2_sequence_nr = (decoder_state->h2_sequence_nr + 1)%4;
                    }
                    decoder_state->search_new_sync_word = 1;
                    decoder_state->sync_word_found = 0;
                }

                if (zero_seq_found){
                    state->zero_frames_nr++;
                } else {
                    state->bad_frames_nr++;
                }

#ifdef LOG_FRAME_STATUS 
                if (zero_seq_found){
                    printf("%d : ZERO FRAME\n", decoder_state->h2_sequence_nr);
                } else {
                    printf("%d : BAD FRAME\n", decoder_state->h2_sequence_nr);
                }
                if (decoder_state->h2_sequence_nr == 3) printf("\n");
#endif
                if (!plc_enabled) break;
                
                frame_data = btstack_sbc_plc_zero_signal_frame();
                OI_UINT32 bytes_in_frame_buffer = msbc_frame_size;
                
                status = OI_CODEC_SBC_DecodeFrame(&(decoder_state->decoder_context), 
                                                    &frame_data, 
                                                    &bytes_in_frame_buffer, 
                                                    decoder_state->pcm_plc_data, 
                                                    &(decoder_state->pcm_bytes));
                // printf("after bad frame, new status: %d, bytes in frame %d\n", status, bytes_in_frame_buffer);

                if (status != 0) exit(10);
                btstack_sbc_plc_bad_frame(&state->plc_state, decoder_state->pcm_plc_data, decoder_state->pcm_data);
                state->handle_pcm_data(decoder_state->pcm_data, 
                                    btstack_sbc_decoder_num_samples_per_frame(state), 
                                    btstack_sbc_decoder_num_channels(state), 
                                    btstack_sbc_decoder_sample_rate(state), state->context);

                
                break;
            default:
                printf("Frame decode error: %d\n", status);
                break;
        }

        memmove(decoder_state->frame_buffer, decoder_state->frame_buffer + bytes_processed, decoder_state->bytes_in_frame_buffer);
    }
    // printf ("<<-- exit -->>\n");
}


// *****************************************************************************
//
// SBC encoder based on Bludroid library 
//
// *****************************************************************************

void btstack_sbc_encoder_init(btstack_sbc_encoder_state_t * state, btstack_sbc_mode_t mode, 
                        int blocks, int subbands, int allmethod, int sample_rate, int bitpool){

    if (sbc_encoder_state_singleton && sbc_encoder_state_singleton != state ){
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
            bd_encoder_state.context.s16AllocationMethod = allmethod;                     
            bd_encoder_state.context.s16BitPool = 31;  
            bd_encoder_state.context.mSBCEnabled = 0;
            
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

int btstack_sbc_encoder_num_audio_samples(void){
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    return context->s16NumOfSubBands * context->s16NumOfBlocks * context->s16NumOfChannels;
}

uint8_t * btstack_sbc_encoder_sbc_buffer(void){
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    return context->pu8Packet;
}

uint16_t  btstack_sbc_encoder_sbc_buffer_length(void){
    SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    return context->u16PacketLength;
}

// static void btstack_sbc_encoder_dump_context(void){
//     SBC_ENC_PARAMS * context = &((bludroid_encoder_state_t *)sbc_encoder_state_singleton->encoder_state)->context;
    
//     printf("Blocks %d\n", context->s16NumOfBlocks);
//     printf("SubBands %d\n", context->s16NumOfSubBands);
//     printf("Allocation Method %d\n", context->s16AllocationMethod);
//     printf("BitPool %d\n", context->s16BitPool);
//     printf("Channel Mode %d\n", context->s16ChannelMode);

//     printf("Sample Rate ");
//     switch (context->s16SamplingFreq){
//         case 0: printf("16000\n"); break;
//         case 1: printf("32000\n"); break;
//         case 2: printf("44100\n"); break;
//         case 3: printf("48000\n"); break;
//         default: printf("not defined\n"); break;   
//     }
//     printf("mSBC Enabled %d\n", context->mSBCEnabled);
//     printf("\n");
// }
