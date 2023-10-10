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

#define BTSTACK_FILE__ "btstack_lc3_plus_fraunhofer.c"

/**
 * @title LC3 Plus Fraunhofer Adapter
 */

#include "btstack_config.h"
#include "bluetooth.h"

#include "btstack_lc3plus_fraunhofer.h"
#include "btstack_debug.h"
#include <string.h>

#ifdef HAVE_LC3PLUS

#define MAX_SAMPLES_PER_FRAME 480

static uint8_t lc3plus_farunhofer_scratch[LC3PLUS_DEC_MAX_SCRATCH_SIZE];

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

static uint8_t lc3plus_fraunhofer_decoder_configure(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame){
    btstack_lc3plus_fraunhofer_decoder_t * instance = (btstack_lc3plus_fraunhofer_decoder_t *) context;
    LC3PLUS_Dec * decoder = (LC3PLUS_Dec*) instance->decoder;

    // map frame duration
    uint16_t duration_us = lc3_frame_duration_in_us(frame_duration);
    if (duration_us == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // store config
    instance->sample_rate = sample_rate;
    instance->frame_duration = frame_duration;
    instance->octets_per_frame = octets_per_frame;
    instance->samples_per_frame = btstack_lc3_samples_per_frame(sample_rate, frame_duration);

    LC3PLUS_Error error;
#ifdef ENABLE_HR_MODE
    error = lc3plus_dec_init(decoder, sample_rate, 1, LC3PLUS_PLC_ADVANCED, 0);
#else
    error = lc3plus_dec_init(decoder, sample_rate, 1, LC3PLUS_PLC_ADVANCED);
#endif
    btstack_assert(error == LC3PLUS_OK);

    error = lc3plus_dec_set_frame_dms(decoder, duration_us / 100);
    btstack_assert(error == LC3PLUS_OK);

    return ERROR_CODE_SUCCESS;
}

static uint8_t lc3plus_fraunhofer_decoder_decode_signed_16(void * context, const uint8_t *bytes, uint8_t BFI, int16_t* pcm_out, uint16_t stride, uint8_t * BEC_detect) {
    btstack_lc3plus_fraunhofer_decoder_t *instance = (btstack_lc3plus_fraunhofer_decoder_t *) context;
    LC3PLUS_Dec *decoder = (LC3PLUS_Dec *) instance->decoder;

    // temporary output buffer to interleave samples for caller
    int16_t temp_out[MAX_SAMPLES_PER_FRAME];

    // output_samples: array of channel buffers. use temp_out if stride is used
    int16_t *output_samples[1];
    if (stride > 1) {
        output_samples[0] = temp_out;
    } else {
        output_samples[0] = pcm_out;
    }

    // trigger plc if BFI by passing 0 valid input bytes
    uint16_t byte_count = instance->octets_per_frame;
    if (BFI != 0){
        byte_count = 0;
    }

    LC3PLUS_Error error = lc3plus_dec16(decoder, (void*) bytes, byte_count, output_samples, lc3plus_farunhofer_scratch, BFI);

    // store samples
    if (stride > 1){
        uint16_t i;
        for (i = 0; i < instance->samples_per_frame; i++){
            // cppcheck-suppress uninitvar ; for stride > 1, output_samples[0] = temp_out, which is initialized by lc3plus_dec16
            pcm_out [i * stride] = temp_out[i];
        }
    }

    // map error
    switch (error){
        case LC3PLUS_OK:
            // success
            return ERROR_CODE_SUCCESS;
        case LC3PLUS_DECODE_ERROR:
            // PLC enagaged
            *BEC_detect = 1;
            return ERROR_CODE_SUCCESS;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
}

static uint8_t lc3plus_fraunhofer_decoder_decode_signed_24(void * context, const uint8_t *bytes, uint8_t BFI, int32_t* pcm_out, uint16_t stride, uint8_t * BEC_detect) {
    btstack_lc3plus_fraunhofer_decoder_t * instance = (btstack_lc3plus_fraunhofer_decoder_t *) context;
    LC3PLUS_Dec * decoder = (LC3PLUS_Dec*) instance->decoder;

    // output_samples: array of channel buffers.
    int32_t * output_samples[1];
    output_samples[0] = pcm_out;

    // trigger plc if BFI by passing 0 valid input bytes
    uint16_t byte_count = instance->octets_per_frame;
    if (BFI != 0){
        byte_count = 0;
    }

    LC3PLUS_Error error = lc3plus_dec24(decoder, (void *) bytes, byte_count, output_samples, lc3plus_farunhofer_scratch, BFI);

    // map error
    switch (error){
        case LC3PLUS_OK:
            // success
            return ERROR_CODE_SUCCESS;
        case LC3PLUS_DECODE_ERROR:
            // PLC enagaged
            *BEC_detect = 1;
            return ERROR_CODE_SUCCESS;
        default:
            return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
}

static const btstack_lc3_decoder_t btstack_l3cplus_fraunhofer_decoder_instance = {
        lc3plus_fraunhofer_decoder_configure,
        lc3plus_fraunhofer_decoder_decode_signed_16,
        lc3plus_fraunhofer_decoder_decode_signed_24
};

const btstack_lc3_decoder_t * btstack_lc3plus_fraunhofer_decoder_init_instance(btstack_lc3plus_fraunhofer_decoder_t * context){
    memset(context, 0, sizeof(btstack_lc3plus_fraunhofer_decoder_t));
    return &btstack_l3cplus_fraunhofer_decoder_instance;
}

/* Encoder implementation */

static uint8_t lc3plus_fraunhofer_encoder_configure(void * context, uint32_t sample_rate, btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame){
    btstack_lc3plus_fraunhofer_encoder_t * instance = (btstack_lc3plus_fraunhofer_encoder_t *) context;
    return ERROR_CODE_COMMAND_DISALLOWED;
}

static uint8_t lc3plus_fraunhofer_encoder_encode_signed_16(void * context, const int16_t* pcm_in, uint16_t stride, uint8_t *bytes){
    return ERROR_CODE_COMMAND_DISALLOWED;
}

static uint8_t lc3plus_fraunhofer_encoder_encode_signed_24(void * context, const int32_t* pcm_in, uint16_t stride, uint8_t *bytes) {
    return ERROR_CODE_COMMAND_DISALLOWED;
}

static const btstack_lc3_encoder_t btstack_l3cplus_fraunhofer_encoder_instance = {
        lc3plus_fraunhofer_encoder_configure,
        lc3plus_fraunhofer_encoder_encode_signed_16,
        lc3plus_fraunhofer_encoder_encode_signed_24
};

const btstack_lc3_encoder_t * btstack_lc3plus_fraunhofer_encoder_init_instance(btstack_lc3plus_fraunhofer_encoder_t * context){
    memset(context, 0, sizeof(btstack_lc3plus_fraunhofer_encoder_t));
    return &btstack_l3cplus_fraunhofer_encoder_instance;
}

#endif /* HAVE_LC3PLUS */
