/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#ifndef BTSTACK_AUDIO_H
#define BTSTACK_AUDIO_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/*
 *  btstack_audio.h
 *
 *  Abstraction layer for 16-bit audio playback and recording within BTstack
 */

typedef struct {

    /**
     * @brief Setup audio codec for specified samplerate and number of channels
     * @param Channels (1=mono, 2=stereo)
     * @param Sample rate
     * @param Playback callback
     * @return 1 on success
     */
    int (*init)(uint8_t channels,
                uint32_t samplerate, 
                void (*playback) (int16_t * buffer, uint16_t num_samples));

    /** 
     * @brief Start stream
     */
    void (*start_stream)(void);

    /** 
     * @brief Stop stream
     */
    void (*stop_stream)(void);

    /**
     * @brief Close audio codec
     */
    void (*close)(void);

} btstack_audio_sink_t;


typedef struct {

    /**
     * @brief Setup audio codec for specified samplerate and number of channels
     * @param Channels (1=mono, 2=stereo)
     * @param Sample rate
     * @param Recording callback
     * @return 1 on success
     */
    int (*init)(uint8_t channels,
                uint32_t samplerate, 
                void (*recording)(const int16_t * buffer, uint16_t num_samples));

    /** 
     * @brief Start stream
     */
    void (*start_stream)(void);

    /** 
     * @brief Stop stream
     */
    void (*stop_stream)(void);

    /**
     * @brief Close audio codec
     */
    void (*close)(void);

} btstack_audio_source_t;


/**
 * @brief Get BTstack Audio Sink Instance
 * @returns btstack_audio_sink implementation
 */
const btstack_audio_sink_t * btstack_audio_sink_get_instance(void);

/**
 * @brief Get BTstack Audio Source Instance
 * @returns btstack_audio_source implementation
 */
const btstack_audio_source_t * btstack_audio_source_get_instance(void);


/**
 * @brief Get BTstack Audio Sink Instance
 * @param btstack_audio_sink implementation
 */
void btstack_audio_sink_set_instance(const btstack_audio_sink_t * audio_sink_impl);

/**
 * @brief Get BTstack Audio Source Instance
 * @param btstack_audio_source implementation
 */
void btstack_audio_source_set_instance(const btstack_audio_source_t * audio_source_impl);


// common implementations
const btstack_audio_sink_t *   btstack_audio_portaudio_sink_get_instance(void);
const btstack_audio_source_t * btstack_audio_portaudio_source_get_instance(void);

const btstack_audio_sink_t *   btstack_audio_embedded_sink_get_instance(void);
const btstack_audio_source_t * btstack_audio_embedded_source_get_instance(void);

const btstack_audio_sink_t *   btstack_audio_esp32_sink_get_instance(void);
// const btstack_audio_source_t *   btstack_audio_esp32_source_get_instance(void);

#if defined __cplusplus
}
#endif

#endif
