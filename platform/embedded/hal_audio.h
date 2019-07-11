/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#include <stdint.h>

/*
 *  hal_audio.h
 *
 *  Hardware abstraction layer that provides circular audio playback and recording
 *
 *  Assumptions: 
 *  - num buffers n >= 2
 *  - after start, buffers are played in sequence 0, 1, ... , n-1, 0, 1, .. , n-1, ... 
 *  - after a buffer is played, the callback is called with tbe index of the played buffer
 */

/**
 * @brief Setup audio codec for playback using specified samplerate and number of channels
 * @param Channels
 * @param Sample rate
 * @param Buffer played callback
 */
void hal_audio_sink_init(uint8_t channels, 
                         uint32_t sample_rate,
                         void (*buffer_played_callback)(uint8_t buffer_index));

/**
 * @brief Get number of output buffers in HAL
 * @returns num buffers
 */
uint16_t hal_audio_sink_get_num_output_buffers(void);

/**
 * @brief Get size of single output buffer in HAL
 * @returns buffer size
 */
uint16_t hal_audio_sink_get_num_output_buffer_samples(void);

/**
 * @brief Get output buffer
 * @param buffer index
 * @returns buffer
 */
int16_t * hal_audio_sink_get_output_buffer(uint8_t buffer_index);

/**
 * @brief Start stream
 */
void hal_audio_sink_start(void);

/**
 * @brief Stop stream
 */
void hal_audio_sink_stop(void);

/**
 * @brief Close audio codec
 */
void hal_audio_sink_close(void);


/**
 * @brief Setup audio codec for recording using specified samplerate and number of channels
 * @param Channels
 * @param Sample rate
 * @param Buffer recorded callback
 */
void hal_audio_source_init(uint8_t channels, 
                           uint32_t sample_rate,
                           void (*buffer_recorded_callback)(const int16_t * buffer, uint16_t num_samples));

/**
 * @brief Start stream
 */
void hal_audio_source_start(void);

/**
 * @brief Stop stream
 */
void hal_audio_source_stop(void);

/**
 * @brief Close audio codec
 */
void hal_audio_source_close(void);

#endif
