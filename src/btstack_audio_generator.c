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

#define BTSTACK_FILE__ "btstack_audio_generator.c"

#include "btstack_audio_generator.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

// Implementation of simple audio generators: silence, sine, mod player, vorbis, and recording
// Minimal implementation focused on producing interleaved 16-bit PCM at requested sample rate/channels.

#define PI 3.14159265358979323846f

// ---- Helpers ----
static void generator_base_init(btstack_audio_generator_t * base, uint16_t samplerate_hz, uint8_t channels,
                                void (*generate)(btstack_audio_generator_t *, int16_t *, uint16_t),
                                void (*finalize)(btstack_audio_generator_t *)){
    base->samplerate_hz = samplerate_hz;
    base->channels = channels;
    base->generate = generate;
    base->finalize = finalize;
}

// duplicate audio channels in a round-robin fashion
static void duplicate_audio_channels(int16_t* pcm_buffer, uint16_t num_samples, uint8_t channels_have, uint8_t channels_need) {
    for (int16_t i = num_samples - 1; i >= 0; i--) {
        for (uint16_t channel_dst=0; channel_dst < channels_need; channel_dst++){
            uint16_t channel_src = channel_dst % channels_have;
            pcm_buffer[i * channels_need + channel_dst] = pcm_buffer[i * channels_have + channel_src];
        }
    }
}

// ---- Silence ----
static void silence_generate(btstack_audio_generator_t * self, int16_t * pcm_buffer, uint16_t num_samples){
    UNUSED(self);
    memset(pcm_buffer, 0, (size_t)num_samples * self->channels * sizeof(int16_t));
}

static void silence_finalize(btstack_audio_generator_t * self){
    (void)self;
}

void btstack_audio_generator_silence_init(btstack_audio_generator_silence_t * self, uint16_t samplerate_hz, uint8_t channels){
    generator_base_init(self, samplerate_hz, channels, silence_generate, silence_finalize);
}

// ---- Sine Wave ----

static void sine_generate(btstack_audio_generator_t * base, int16_t * pcm_buffer, uint16_t num_samples){
    btstack_audio_generator_sine_t * self = (btstack_audio_generator_sine_t*) base;
    uint16_t pos = 0;
    for (uint16_t i = 0; i < num_samples; i++) {
        for (uint8_t ch = 0; ch < base->channels; ch++) {
            int16_t value = self->table[self->phase];
            pcm_buffer[pos++] = value;
            self->phase += 1;
            if (self->phase >= self->num_samples) {
                self->phase = 0;
            }
        }
    }
}

static void sine_finalize(btstack_audio_generator_t * base){
    UNUSED(base);
}

void btstack_audio_generator_sine_init(btstack_audio_generator_sine_t * self, uint16_t samplerate_hz, uint8_t channels,
                                       uint16_t frequency_hz){
    btstack_assert(samplerate_hz <= MAX_SAMPLETRATE_HZ);
    // calc number of samples per period
    uint16_t num_samples = samplerate_hz / frequency_hz;
    if (num_samples > SINE_MAX_SAMPLES_AT_48KHZ) {
        num_samples = SINE_MAX_SAMPLES_AT_48KHZ;
    }
    self->num_samples = num_samples;
    // fill table
    uint16_t i;
    for (i=0;i<self->num_samples;i++){
        float sine_value = sin(2 * PI * i / num_samples);
        self->table[i] = (int16_t)(sine_value * 32767);
    }
    // init context
    self->phase = 0;
    generator_base_init(&self->base, samplerate_hz, channels, sine_generate, sine_finalize);
}

// ---- MOD Player ----
static void mod_generate(btstack_audio_generator_t * base, int16_t * pcm_buffer, uint16_t num_samples){
    btstack_audio_generator_mod_t * self = (btstack_audio_generator_mod_t *) base;
    uint8_t channels = self->base.channels;
    if (channels == 1) {
        // use small buffer to fill stereo output and then convert to mono into pcm_buffer
        #define MOD_MONO_BUFFER_SAMPLES 32
        int16_t mod_buffer[MOD_MONO_BUFFER_SAMPLES * 2];
        uint16_t samples_to_do = num_samples;
        uint16_t pos = 0;
        while (samples_to_do > 0) {
            uint16_t samples_now = btstack_min(samples_to_do, MOD_MONO_BUFFER_SAMPLES);
            hxcmod_fillbuffer(&self->context, (msample*)mod_buffer, samples_now, NULL);
            // stereo -> mono
            for (uint16_t i=0;i<samples_now;i++){
                int32_t left  = mod_buffer[2*i + 0];
                int32_t right = mod_buffer[2*i + 1];
                pcm_buffer[pos++] = (int16_t)((left + right) / 2);
            }
            samples_to_do -= samples_now;
        }
    } else {
        hxcmod_fillbuffer(&self->context, (msample*)pcm_buffer, num_samples, NULL);
        // duplicate stereo channels
        if (channels > 2) {
            duplicate_audio_channels(pcm_buffer, num_samples, 2, channels);
        }
    }
}

static void mod_finalize(btstack_audio_generator_t * base){
    btstack_audio_generator_mod_t * self = (btstack_audio_generator_mod_t *) base;
    hxcmod_unload(&self->context);
}

void btstack_audio_generator_modplayer_init(btstack_audio_generator_mod_t * self, uint16_t samplerate_hz, uint8_t channels,
                                       const uint8_t * mod_data, uint32_t mod_len){
    int ok = hxcmod_init(&self->context);
    btstack_assert(ok != 0);
    hxcmod_setcfg(&self->context, samplerate_hz, 1, 1);
    hxcmod_load(&self->context, (void*)mod_data, (int)mod_len);
    generator_base_init(&self->base, samplerate_hz, channels, mod_generate, mod_finalize);
}

// ---- OGG Vorbis ----
// @TODO: support different sample rates and stereo -> mono conversion
static void vorbis_generate(btstack_audio_generator_t * base, int16_t * pcm_buffer, uint16_t num_samples){
    btstack_audio_generator_vorbis_t * self = (btstack_audio_generator_vorbis_t *) base;
    uint16_t need_samples = num_samples;
    uint8_t channels_needed = self->base.channels;
    uint8_t channels_have = self->file_channels;
    while (need_samples > 0){
        int n = stb_vorbis_get_samples_short_interleaved(self->context, base->channels, pcm_buffer, need_samples * channels_needed);
        if (n <= 0){
            // loop: rewind to start
            stb_vorbis_seek_start(self->context);
            continue;
        }
        // stb returns frames per channel
        pcm_buffer  += n * channels_needed;
        need_samples -= n;
    }
    if (channels_needed > channels_have) {
        duplicate_audio_channels(pcm_buffer, num_samples, channels_have, channels_needed);
    }
}

static void vorbis_finalize(btstack_audio_generator_t * base){
    btstack_audio_generator_vorbis_t * self = (btstack_audio_generator_vorbis_t *) base;
    if (self->context){
        stb_vorbis_close(self->context);
        self->context = NULL;
    }
}

void btstack_audio_generator_vorbis_init(btstack_audio_generator_vorbis_t * self, uint16_t samplerate_hz, uint8_t channels,
                                         const char * filename){
    int error = 0;
    stb_vorbis * v = stb_vorbis_open_filename(filename, &error, NULL);
    if (v == NULL){
        log_error("vorbis open failed %d for %s", error, filename);
    } else {
        stb_vorbis_info info = stb_vorbis_get_info(v);
        self->file_channels = info.channels;
        btstack_assert( info.sample_rate == samplerate_hz );
        btstack_assert( info.channels <= channels );
    }
    self->context = v;
    generator_base_init(&self->base, samplerate_hz, channels, vorbis_generate, vorbis_finalize);
}

