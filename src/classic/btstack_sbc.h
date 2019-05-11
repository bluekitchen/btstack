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

/*
 * btstack_sbc.h
 *
 */

#ifndef BTSTACK_SBC_H
#define BTSTACK_SBC_H

#include <stdint.h>
#include "btstack_sbc_plc.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum{
    SBC_MODE_STANDARD,
    SBC_MODE_mSBC
} btstack_sbc_mode_t;

typedef struct {
    void * context;
    void (*handle_pcm_data)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context);
    // private
    void * decoder_state;
    btstack_sbc_plc_state_t plc_state;
    btstack_sbc_mode_t mode;

    // summary of processed good, bad and zero frames
    int good_frames_nr;
    int bad_frames_nr;
    int zero_frames_nr;
} btstack_sbc_decoder_state_t;

typedef struct {
    // private
    void * encoder_state;
    btstack_sbc_mode_t mode;
} btstack_sbc_encoder_state_t;

/* API_START */

/* BTstack SBC decoder */
/**
 * @brief Init SBC decoder
 * @param state
 * @param mode
 * @param callback for decoded PCM data in host endianess
 * @param context provided in callback
 */

void btstack_sbc_decoder_init(btstack_sbc_decoder_state_t * state, btstack_sbc_mode_t mode, void (*callback)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context), void * context);

/**
 * @brief Process received SBC data
 * @param state
 * @param packet_status_flag from SCO packet: 0 = OK, 1 = possibly invalid data, 2 = no data received, 3 = data partially lost
 * @param buffer
 * @param size
 */
void btstack_sbc_decoder_process_data(btstack_sbc_decoder_state_t * state, int packet_status_flag, uint8_t * buffer, int size);

/**
 * @brief Get number of samples per SBC frame
 */
int btstack_sbc_decoder_num_samples_per_frame(btstack_sbc_decoder_state_t * state);

/*
 * @brief Get number of channels
 */
int btstack_sbc_decoder_num_channels(btstack_sbc_decoder_state_t * state);

/*
 * @brief Get sample rate in hz
 */
int btstack_sbc_decoder_sample_rate(btstack_sbc_decoder_state_t * state);


/* BTstack SBC Encoder */
/**
 * @brief Init SBC encoder
 * @param state
 * @param mode 
 * @param blocks
 * @param subbands
 * @param allocation_method
 * @param sample_rate
 * @param bitpool
 * @param channel_mode
 */
void btstack_sbc_encoder_init(btstack_sbc_encoder_state_t * state, btstack_sbc_mode_t mode, 
                        int blocks, int subbands, int allocation_method, int sample_rate, int bitpool, int channel_mode);

/**
 * @brief Encode PCM data
 * @param buffer with samples in host endianess
 */
void btstack_sbc_encoder_process_data(int16_t * input_buffer);

/**
 * @brief Return SBC frame
 */
uint8_t * btstack_sbc_encoder_sbc_buffer(void);

/**
 * @brief Return SBC frame length
 */
uint16_t  btstack_sbc_encoder_sbc_buffer_length(void);

/**
 * @brief Return number of audio frames required for one SBC packet
 * @note  each audio frame contains 2 sample values in stereo modes
 */
int  btstack_sbc_encoder_num_audio_frames(void);

/* API_END */

// testing only
void btstack_sbc_decoder_test_set_plc_enabled(int plc_enabled);
void btstack_sbc_decoder_test_simulate_corrupt_frames(int period);

#if defined __cplusplus
}
#endif

#endif // BTSTACK_SBC_H
