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

#define BTSTACK_FILE__ "btstack_lc3_google.c"

/**
 * @title LC3 Google Adapter
 */

#include "btstack_config.h"
#include "bluetooth.h"

#include "btstack_lc3_google.h"
#include "btstack_debug.h"
#include <string.h>

static uint16_t lc3_frame_duration_in_us(btstack_lc3_frame_duration_t frame_duration){
    switch (frame_duration) {
        case BTSTACK_LC3_FRAME_DURATION_7500US:
            return 7500;
        case BTSTACK_LC3_FRAME_DURATION_10000US:
            return 10000;
        default:
            return 0;
    }
}

/* Decoder implementation */

static uint8_t lc3_decoder_google_configure(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration){
    btstack_lc3_decoder_google_t * instance = (btstack_lc3_decoder_google_t *) context;

    // map frame duration
    uint16_t duration_us = lc3_frame_duration_in_us(frame_duration);
    if (duration_us == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // store config
    instance->sample_rate = sample_rate;
    instance->frame_duration = frame_duration;

    // config decoder
    instance->decoder = lc3_setup_decoder(duration_us, sample_rate, 0, &instance->decoder_mem);

    if (instance->decoder == NULL) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    return ERROR_CODE_SUCCESS;
}

static uint16_t lc3_decoder_google_get_number_octets_for_bitrate(void * context, uint32_t bitrate){
    btstack_lc3_decoder_google_t * instance = (btstack_lc3_decoder_google_t *) context;
    uint16_t duration_us = lc3_frame_duration_in_us(instance->frame_duration);
    if (duration_us == 0){
        return 0;
    }
    return lc3_frame_bytes(duration_us, bitrate);
}

static uint16_t lc3_decoder_google_get_number_samples_per_frame(void * context){
    btstack_lc3_decoder_google_t * instance = (btstack_lc3_decoder_google_t *) context;
    uint16_t duration_us = lc3_frame_duration_in_us(instance->frame_duration);
    if (duration_us == 0){
        return 0;
    }
    return lc3_frame_samples(duration_us, instance->sample_rate);
}

static uint8_t lc3_decoder_google_decode(void * context, const uint8_t *bytes, uint16_t byte_count, uint8_t BFI,  enum lc3_pcm_format fmt, void * pcm_out, uint16_t stride, uint8_t * BEC_detect){
    btstack_lc3_decoder_google_t * instance = (btstack_lc3_decoder_google_t *) context;

    if (BFI){
        // bit errors, trigger PLC by passing NULL as buffer
        *BEC_detect = 1;
        bytes = NULL;
    } else {
        // no bit errors, regular processing
        *BEC_detect = 0;
    }

    int result = lc3_decode(instance->decoder, (const void *) bytes, byte_count, fmt, pcm_out, stride);
    switch (result){
        case 0: // success
            return ERROR_CODE_SUCCESS;
        case -1: // PLC engaged
            *BEC_detect = 1;
            return ERROR_CODE_SUCCESS;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
}

static uint8_t lc3_decoder_google_decode_signed_16(void * context, const uint8_t *bytes, uint16_t byte_count, uint8_t BFI, int16_t* pcm_out, uint16_t stride, uint8_t * BEC_detect){
    return lc3_decoder_google_decode(context, bytes, byte_count, BFI, LC3_PCM_FORMAT_S16, (void *) pcm_out, stride, BEC_detect);
}

static uint8_t lc3_decoder_google_decode_signed_24(void * context, const uint8_t *bytes, uint16_t byte_count, uint8_t BFI, int32_t* pcm_out, uint16_t stride, uint8_t * BEC_detect) {
    return lc3_decoder_google_decode(context, bytes, byte_count, BFI, LC3_PCM_FORMAT_S24, (void *) pcm_out, stride, BEC_detect);
}

static const btstack_lc3_decoder_t btstack_l3c_decoder_google_instance = {
    lc3_decoder_google_configure,
    lc3_decoder_google_get_number_octets_for_bitrate,
    lc3_decoder_google_get_number_samples_per_frame,
    lc3_decoder_google_decode_signed_16,
    lc3_decoder_google_decode_signed_24
};

const btstack_lc3_decoder_t * btstack_lc3_decoder_google_init_instance(btstack_lc3_decoder_google_t * context){
    memset(context, 0, sizeof(btstack_lc3_decoder_google_t));
    return &btstack_l3c_decoder_google_instance;
}

/* Encoder implementation */

static uint8_t lc3_encoder_google_configure(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration){
    btstack_lc3_encoder_google_t * instance = (btstack_lc3_encoder_google_t *) context;

    // map frame duration
    uint16_t duration_us = lc3_frame_duration_in_us(frame_duration);
    if (duration_us == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // store config
    instance->sample_rate = sample_rate;
    instance->frame_duration = frame_duration;

    // config encoder
    instance->encoder = lc3_setup_encoder(duration_us, sample_rate, 0, &instance->encoder_mem);

    if (instance->encoder == NULL) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    return ERROR_CODE_SUCCESS;
}

static uint32_t lc3_encoder_google_get_bitrate_for_number_of_octets(void * context, uint16_t number_of_octets){
    btstack_lc3_encoder_google_t * instance = (btstack_lc3_encoder_google_t *) context;
    // map frame duration
    uint16_t duration_us = lc3_frame_duration_in_us(instance->frame_duration);
    if (duration_us == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    return lc3_resolve_bitrate(duration_us, number_of_octets);
}

static uint16_t lc3_encoder_google_get_number_samples_per_frame(void * context){
    btstack_lc3_encoder_google_t * instance = (btstack_lc3_encoder_google_t *) context;
    // map frame duration
    uint16_t duration_us = lc3_frame_duration_in_us(instance->frame_duration);
    if (duration_us == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    return lc3_frame_samples(duration_us, instance->sample_rate);
}
static uint8_t lc3_encoder_google_encode_signed(void * context, enum lc3_pcm_format fmt, const void *pcm_in, uint16_t stride, uint8_t *bytes, uint16_t byte_count) {
    btstack_lc3_encoder_google_t * instance = (btstack_lc3_encoder_google_t *) context;
    int result = lc3_encode(instance->encoder, fmt, pcm_in, stride, byte_count, (void*) bytes);
    switch (result){
        case 0:
            return ERROR_CODE_SUCCESS;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
}

static uint8_t lc3_encoder_google_encode_signed_16(void * context, const int16_t* pcm_in, uint16_t stride, uint8_t *bytes, uint16_t byte_count){
    return lc3_encoder_google_encode_signed(context, LC3_PCM_FORMAT_S16, (const void *) pcm_in, stride, bytes, byte_count);
}

static uint8_t lc3_encoder_google_encode_signed_24(void * context, const int32_t* pcm_in, uint16_t stride, uint8_t *bytes, uint16_t byte_count){
    return lc3_encoder_google_encode_signed(context, LC3_PCM_FORMAT_S24, (const void *) pcm_in, stride, bytes, byte_count);
}

static const btstack_lc3_encoder_t btstack_l3c_encoder_google_instance = {
        lc3_encoder_google_configure,
        lc3_encoder_google_get_bitrate_for_number_of_octets,
        lc3_encoder_google_get_number_samples_per_frame,
        lc3_encoder_google_encode_signed_16,
        lc3_encoder_google_encode_signed_24
};

const btstack_lc3_encoder_t * btstack_lc3_encoder_google_init_instance(btstack_lc3_encoder_google_t * context){
    memset(context, 0, sizeof(btstack_lc3_encoder_google_t));
    return &btstack_l3c_encoder_google_instance;
}

