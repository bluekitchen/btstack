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

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_SOURCE_COUNTER,
    AUDIO_SOURCE_SINE,
    AUDIO_SOURCE_MODPLAYER,
    AUDIO_SOURCE_RECORDING,
    AUDIO_SOURCE_LOOPBACK,
} le_audio_demo_source_generator;

/**
 * @brief Init source functionality
 */
void le_audio_demo_util_source_init(void);

/**
 * @brief Configure audio source
 *        Supported num_streams x num_channels_per_stream configurations: 1 x 1, 1 x 2, 2 x 1
 * @param num_streams
 * @param num_channels_per_stream
 * @param sampling_frequency_hz
 * @param frame_duration
 * @param octets_per_frame
 */
void le_audio_demo_util_source_configure(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                         btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame);
/**
 * @brief Generate audio and encode as ISO frame
 * @param channel
 * @param generator
 */

void le_audio_demo_util_source_generate_iso_frame(le_audio_demo_source_generator generator);

/**
 * @brief Send prepared ISO packet
 * @param stream_index
 * @param con_handle
 */
void le_audio_demo_util_source_send(uint8_t stream_index, hci_con_handle_t con_handle);

/**
 * @brief Close source
 */
void le_audio_demo_util_source_close(void);

#if defined __cplusplus
}
#endif
#endif // LE_AUDIO_DEMO_UTIL_SOURCE_H
