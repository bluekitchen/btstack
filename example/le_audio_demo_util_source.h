/*
 * Copyright (C) {copyright_year} BlueKitchen GmbH
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

/**
 *  @brief Send/receive isochronous audio, used by le_audio_* demos
 */

#ifndef LE_AUDIO_DEMO_UTIL_SOURCE_H
#define LE_AUDIO_DEMO_UTIL_SOURCE_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_bool.h"
#include "btstack_lc3.h"
#include "btstack_audio.h"
#include "btstack_audio_generator.h"

#include "btstack_lc3_google.h"
#include "btstack_lc3plus_fraunhofer.h"


#if defined __cplusplus
extern "C" {
#endif

#define LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS 2
#define DEMO_SINE_MAX_SAMPLES_AT_48KHZ 185
#define MAX_SAMPLES_PER_10MS_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

typedef struct {
    const btstack_lc3_encoder_t * encoder;
    btstack_lc3_encoder_google_t  contexts[LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS];
} lc3_codec_t;

/**
 * @brief Callback to generate (test) data for ISO packets
 * @param context provided via le_audio_demo_util_source_set_data_generator
 * @param iso_payload pointer to buffer
 * @param iso_length buffer size
 * @param channel
 * @param packet_sequence_number
 */
typedef void (*le_audio_demo_util_source_data_generator_t)(void *context,
        uint8_t * iso_payload, uint16_t iso_length, uint8_t channel, uint16_t packet_sequence_number);


typedef struct {
    btstack_audio_source_t const *audio_source;
    lc3_codec_t codec;
    bool initialized;

    // Audio Generator
    btstack_audio_generator_t * audio_generator;

    // Data Generator
    le_audio_demo_util_source_data_generator_t * data_generator;
    void * data_generator_context;

    uint16_t octets_per_frame;
    uint16_t packet_sequence_numbers[LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS];
    uint8_t  iso_payload[LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS * MAX_LC3_FRAME_BYTES];
    int16_t  pcm[LE_AUDIO_DEMO_SOURCE_MAX_CHANNELS * MAX_SAMPLES_PER_10MS_FRAME];

    uint32_t sampling_frequency_hz;
    btstack_lc3_frame_duration_t frame_duration;
    uint8_t  num_channels;
    uint8_t  num_streams;
    uint8_t  num_channels_per_stream;
    uint16_t num_samples_per_frame;
} le_audio_demo_source_generator_t;

/**
 * @brief Init source functionality
 * @param gen generator context to initialize
 */
void le_audio_demo_util_source_init(le_audio_demo_source_generator_t *gen);

/**
 * @brief Configure audio source
 *        Supported num_streams x num_channels_per_stream configurations: 1 x 1, 1 x 2, 2 x 1
 * @param gen generator context
 * @param num_streams
 * @param num_channels_per_stream
 * @param sampling_frequency_hz
 * @param frame_duration
 * @param octets_per_frame
 */
void le_audio_demo_util_source_configure(le_audio_demo_source_generator_t *gen, uint8_t num_streams, uint8_t num_channels_per_stream,
                                         uint32_t sampling_frequency_hz, btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame);

/**
 * @brief Sets the audio generator for the LE Audio demo source.
 * @param gen Pointer to the LE Audio demo source generator structure.
 * @param audio_generator Pointer to the audio generator to be used.
 */
void le_audio_demo_util_source_set_audio_generator(le_audio_demo_source_generator_t *gen,
    btstack_audio_generator_t *audio_generator);

/**
 * @brief Set the data generator for the LE Audio demo source
 * @param gen Pointer to the LE Audio demo source generator structure.
 * @param data_generator Pointer to ISO Data generator
 * @param context Pointer to context provided in data generator call
 */
void le_audio_demo_util_source_set_data_generator(le_audio_demo_source_generator_t *gen,
    le_audio_demo_util_source_data_generator_t * data_generator, void * context);

/**
 * @brief Generate audio and encode as ISO frame
 * @param gen generator context
 * @param channel
 */
void le_audio_demo_util_source_generate_iso_frame(le_audio_demo_source_generator_t* gen);

/**
 * @brief Send prepared ISO packet
 * @param gen generator context
 * @param stream_index
 * @param con_handle
 */
void le_audio_demo_util_source_send(le_audio_demo_source_generator_t* gen, uint8_t stream_index, hci_con_handle_t con_handle);

/**
 * @brief Close source
 * @param gen generator context
 */
void le_audio_demo_util_source_close(le_audio_demo_source_generator_t *gen);

#if defined __cplusplus
}
#endif
#endif // LE_AUDIO_DEMO_UTIL_SOURCE_H
