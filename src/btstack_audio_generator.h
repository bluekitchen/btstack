/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

/**
 *  @brief TODO
 */

#ifndef BTSTACK_AUDIO_GENERATOR_H
#define BTSTACK_AUDIO_GENERATOR_H
#include <stdint.h>

#include "hxcmod.h"

#ifdef ENABLE_VORBIS
#define STB_VORBIS_HEADER_ONLY
#include "btstack_audio.h"
#include "stb_vorbis.c"
#endif

#if defined __cplusplus
extern "C" {
#endif

// Max Samplerate for all generators
#define MAX_SAMPLETRATE_HZ 48000

// Lowest sine wave
#define SINE_LOWEST_FREQUENCY 200
#define SINE_MAX_SAMPLES_AT_48KHZ (MAX_SAMPLETRATE_HZ / SINE_LOWEST_FREQUENCY)

/** API_START **/

/*
 * Abstract audio generator
 * Note: Initialize function must set function pointers
 */
typedef struct btstack_audio_generator btstack_audio_generator_t;
struct btstack_audio_generator {
    // methods
    void (*generate)(btstack_audio_generator_t * self, int16_t * pcm_buffer, uint16_t num_samples);
    void (*finalize)(btstack_audio_generator_t * self);
    // fields
    uint16_t samplerate_hz;
    uint8_t  channels;
};

/*
 * Silence Generator
 */
typedef btstack_audio_generator_t btstack_audio_generator_silence_t;

/**
 * @brief Initialize silence generator
 * @param self
 * @param samplerate_hz
 * @param channels
 */
void btstack_audio_generator_silence_init(btstack_audio_generator_silence_t * self, uint16_t samplerate_hz, uint8_t channels);

/*
 * Sine Wave Generator
 */
typedef struct {
    btstack_audio_generator_t base;
    int16_t  table[SINE_MAX_SAMPLES_AT_48KHZ];
    uint16_t phase;
    uint16_t num_samples;
} btstack_audio_generator_sine_t;

/**
 * @brief Initialize sine wave generator
 *
 * @param self
 * @param samplerate_hz
 * @param channels
 * @param frequency_hz >= SINE_LOWEST_FREQUENCY
 */
void btstack_audio_generator_sine_init(btstack_audio_generator_sine_t * self, uint16_t samplerate_hz, uint8_t channels,
                                       uint16_t frequency_hz);

/*
 * Mod Player Generator
 */
typedef struct {
    btstack_audio_generator_t base;
    modcontext context;
} btstack_audio_generator_mod_t;

/**
 * @brief Initialize mod player generator
 * @param self
 * @param samplerate_hz
 * @param channels
 * @param mod_data
 * @param mod_len
 */
void btstack_audio_generator_modplayer_init(btstack_audio_generator_mod_t * self, uint16_t samplerate_hz, uint8_t channels,
                                       const uint8_t * mod_data, uint32_t mod_len);
#ifdef ENABLE_VORBIS
/*
 * OGG Vorbis Generator
 * note: requires sample rate and channels to match file
 */
typedef struct {
    btstack_audio_generator_t base;
    stb_vorbis * context;
    uint8_t file_channels;
} btstack_audio_generator_vorbis_t;

/**
 * @brief Initialize OGG Vorbis Generator
 * @param self
 * @param samplerate_hz
 * @param channels
 * @param filename
 */
void btstack_audio_generator_vorbis_init(btstack_audio_generator_vorbis_t * self, uint16_t samplerate_hz, uint8_t channels,
                                         const char * filename);
#endif

/**
 * @brief Generate audio samples with the initialized audio generator
 * @param self
 * @param pcm_buffer
 * @param num_samples
 */
void btstack_audio_generator_generate(btstack_audio_generator_t * self, int16_t * pcm_buffer, uint16_t num_samples);

/**
 * @brief Finalize initialized audio generator
 * @param self
 */
void btstack_audio_generator_finalize(btstack_audio_generator_t * self);

/** API_END **/

#if defined __cplusplus
}
#endif
#endif // BTSTACK_AUDIO_GENERATOR_H
