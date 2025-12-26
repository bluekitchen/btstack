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

#define BTSTACK_FILE__ "btstack_audio_sdl2.c"

#ifdef HAVE_SDL2

#include <stdint.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

// SDL2 on desktop systems defines HAVE_MALLOC but we do this as well
#undef HAVE_MALLOC

#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop.h"

#define NUM_FRAMES_PER_AUDIO_BUFFER    512
#define NUM_OUTPUT_BUFFERS               5
#define NUM_INPUT_BUFFERS                5
#define DRIVER_POLL_INTERVAL_MS          5

#ifndef MAX_NR_AUDIO_CHANNELS
#define MAX_NR_AUDIO_CHANNELS 2
#endif

// config
static int  num_channels_sink;
static int  num_channels_source;
static int  num_bytes_per_sample_sink;
static int  num_bytes_per_sample_source;

// state
static int source_initialized;
static int sink_initialized;
static int source_active;
static int sink_active;

static uint8_t sink_volume;

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context);
static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context);

// output buffer
static int16_t               output_buffer_storage[NUM_OUTPUT_BUFFERS * NUM_FRAMES_PER_AUDIO_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int16_t             * output_buffers[NUM_OUTPUT_BUFFERS];
static int                   output_buffer_to_play;
static int                   output_buffer_to_fill;

// input buffer
static int16_t               input_buffer_storage[NUM_INPUT_BUFFERS * NUM_FRAMES_PER_AUDIO_BUFFER * MAX_NR_AUDIO_CHANNELS];
static int16_t             * input_buffers[NUM_INPUT_BUFFERS];
static int                   input_buffer_to_record;
static int                   input_buffer_to_fill;


// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;
static btstack_timer_source_t  driver_timer_source;

// SDL2 specific
static SDL_AudioSpec output_obtained_spec;
static SDL_AudioSpec input_obtained_spec;
static SDL_AudioDeviceID output_device;
static SDL_AudioDeviceID input_device;

SDL_AudioSpec obtainedSpec;
static void sdl2_callback_sink(void *userdata, Uint8 *stream, int len) {

    /** sdl_callback_sink is called from different thread, don't use hci_dump / log_info here without additional checks */

    UNUSED(userdata);

    // simplified volume control
    uint16_t index;
    int16_t * from_buffer = output_buffers[output_buffer_to_play];
    int16_t * to_buffer = (int16_t *) stream;
    int samples = len / (sizeof(int16_t) * num_channels_sink);
    btstack_assert(samples == NUM_FRAMES_PER_AUDIO_BUFFER);
    UNUSED(samples);

#if 0
    // up to 8 right shifts
    int right_shift = 8 - btstack_min(8, ((sink_volume + 15) / 16));
    for (index = 0; index < (NUM_FRAMES_PER_PA_BUFFER * num_channels_sink); index++){
        *to_buffer++ = (*from_buffer++) >> right_shift;
    }
#else
    // multiply with volume ^ 4
    int16_t x2 = sink_volume * sink_volume;
    int16_t x4 = (x2 * x2) >> 14;
    for (index = 0; index < (NUM_FRAMES_PER_AUDIO_BUFFER * num_channels_sink); index++){
        *to_buffer++ = ((*from_buffer++) * x4) >> 14;
    }
#endif

    // next
    output_buffer_to_play = (output_buffer_to_play + 1 ) % NUM_OUTPUT_BUFFERS;
}

static void sdl2_callback_source(void *userdata, Uint8 *stream, int len) {

    /** sdl2_callback_source is called from different thread, don't use hci_dump / log_info here without additional checks */

    UNUSED(userdata);

    int samples = len / (sizeof(int16_t) * num_channels_source);
    int16_t * to_buffer = input_buffers[input_buffer_to_fill];
    int16_t * from_buffer = (int16_t *) stream;
    btstack_assert(samples == NUM_FRAMES_PER_AUDIO_BUFFER);
    UNUSED(samples);

    // store in one of our buffers
    memcpy(to_buffer, from_buffer, NUM_FRAMES_PER_AUDIO_BUFFER * num_bytes_per_sample_source);

    // next
    input_buffer_to_fill = (input_buffer_to_fill + 1 ) % NUM_INPUT_BUFFERS;
}

static void driver_timer_handler_sink(btstack_timer_source_t * ts){

    // playback buffer ready to fill
    while (output_buffer_to_play != output_buffer_to_fill){
        (*playback_callback)(output_buffers[output_buffer_to_fill], NUM_FRAMES_PER_AUDIO_BUFFER, 0);

        // next
        output_buffer_to_fill = (output_buffer_to_fill + 1 ) % NUM_OUTPUT_BUFFERS;
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static void driver_timer_handler_source(btstack_timer_source_t * ts){

    // recording buffer ready to process
    if (input_buffer_to_record != input_buffer_to_fill){

        (*recording_callback)(input_buffers[input_buffer_to_record], NUM_FRAMES_PER_AUDIO_BUFFER, 0);

        // next
        input_buffer_to_record = (input_buffer_to_record + 1 ) % NUM_INPUT_BUFFERS;
    }    

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_sdl2_init(void) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        log_error("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    return 0;
}

static int btstack_audio_sdl2_sink_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context)
){
    btstack_assert(channels <= MAX_NR_AUDIO_CHANNELS);
    btstack_assert(playback != NULL);

    num_channels_sink = channels;
    num_bytes_per_sample_sink = 2 * channels;

    for (int i=0;i<NUM_OUTPUT_BUFFERS;i++){
        output_buffers[i] = &output_buffer_storage[i * NUM_FRAMES_PER_AUDIO_BUFFER * MAX_NR_AUDIO_CHANNELS];
    }

    // init sdl2 audio
    int error = btstack_audio_sdl2_init();
    if (error) return error;

    // open output device
    SDL_AudioSpec desiredSpec;
    SDL_zero(desiredSpec);
    desiredSpec.freq = samplerate;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = 2;
    desiredSpec.samples = NUM_FRAMES_PER_AUDIO_BUFFER;
    desiredSpec.callback = sdl2_callback_sink;

    output_device = SDL_OpenAudioDevice(
        NULL,      // NULL = default device
        0,         // 0 = playback
        &desiredSpec,
        &output_obtained_spec,
        0          // 0 = allow SDL to adjust spec if necessary
    );

    if (output_device == 0) {
        log_error("Could not open audio: %s\n", SDL_GetError());
        return 1;
    }

    playback_callback  = playback;

    sink_initialized = 1;
    sink_volume = 127;

    return 0;
}

static int btstack_audio_sdl2_source_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*recording)(const int16_t * buffer, uint16_t num_samples, const btstack_audio_context_t * context)
){
    btstack_assert(channels <= MAX_NR_AUDIO_CHANNELS);
    btstack_assert(recording != NULL);

    num_channels_source = channels;
    num_bytes_per_sample_source = 2 * channels;

    for (int i=0;i<NUM_INPUT_BUFFERS;i++){
        input_buffers[i] = &input_buffer_storage[i * NUM_FRAMES_PER_AUDIO_BUFFER * MAX_NR_AUDIO_CHANNELS];
    }

    // init sdl2 audio
    int error = btstack_audio_sdl2_init();
    if (error) return error;

    // open output device
    SDL_AudioSpec desiredSpec;
    SDL_zero(desiredSpec);
    desiredSpec.freq = samplerate;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = 2;
    desiredSpec.samples = NUM_FRAMES_PER_AUDIO_BUFFER;
    desiredSpec.callback = sdl2_callback_source;

    output_device = SDL_OpenAudioDevice(
        NULL,      // NULL = default device
        0,         // 1 = recording
        &desiredSpec,
        &input_obtained_spec,
        0          // 0 = allow SDL to adjust spec if necessary
    );

    if (output_device == 0) {
        log_error("Could not open audio: %s\n", SDL_GetError());
        return 1;
    }

    recording_callback = recording;

    source_initialized = 1;

    return 0;
}

static uint32_t btstack_audio_sdl2_sink_get_samplerate(void) {
    return output_obtained_spec.freq;
}

static uint32_t btstack_audio_sdl2_source_get_samplerate(void) {
    return input_obtained_spec.freq;
}

static void btstack_audio_sdl2_sink_set_volume(uint8_t volume){
    sink_volume = volume;
}

static void btstack_audio_sdl2_source_set_gain(uint8_t gain){
    UNUSED(gain);
}

static void btstack_audio_sdl2_sink_start_stream(void){

    if (!playback_callback) return;

    // fill buffers once
    uint8_t i;
    for (i=0;i<NUM_OUTPUT_BUFFERS-1;i++){
        (*playback_callback)(&output_buffer_storage[i * NUM_FRAMES_PER_AUDIO_BUFFER * MAX_NR_AUDIO_CHANNELS], NUM_FRAMES_PER_AUDIO_BUFFER, 0);
    }
    output_buffer_to_play = 0;
    output_buffer_to_fill = NUM_OUTPUT_BUFFERS-1;

    // start stream
    SDL_PauseAudioDevice(output_device, 0);  // start playing

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

    sink_active = 1;
}

static void btstack_audio_sdl2_source_start_stream(void){

    if (!recording_callback) return;

    // start stream
    SDL_PauseAudioDevice(input_device, 0);  // start playing

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_source, &driver_timer_handler_source);
    btstack_run_loop_set_timer(&driver_timer_source, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_source);

    source_active = 1;
}

static void btstack_audio_sdl2_sink_stop_stream(void){

    if (!playback_callback) return;
    if (!sink_active)       return;

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);

    // start stream
    SDL_PauseAudioDevice(output_device, 1);   // start capturing

    sink_active = 0;
}

static void btstack_audio_sdl2_source_stop_stream(void){

    if (!recording_callback) return;
    if (!source_active)      return;

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_source);

    // start stream
    SDL_PauseAudioDevice(input_device, 1);   // start capturing

    source_active = 0;
}

static void btstack_audio_sdl2_sink_close(void){

    if (!playback_callback) return;

    if (sink_active){
        btstack_audio_sdl2_sink_stop_stream();
    }

    SDL_CloseAudioDevice(output_device);

    sink_initialized = 0;
}

static void btstack_audio_sdl2_source_close(void){

    if (!recording_callback) return;

    if (source_active){
        btstack_audio_sdl2_source_stop_stream();
    }

    SDL_CloseAudioDevice(input_device);

    source_initialized = 0;
}

static const btstack_audio_sink_t btstack_audio_sdl2_sink = {
    /* int (*init)(..);*/                                       &btstack_audio_sdl2_sink_init,
    /* uint32_t (*get_samplerate)() */                          &btstack_audio_sdl2_sink_get_samplerate,
    /* void (*set_volume)(uint8_t volume); */                   &btstack_audio_sdl2_sink_set_volume,
    /* void (*start_stream(void));*/                            &btstack_audio_sdl2_sink_start_stream,
    /* void (*stop_stream)(void)  */                            &btstack_audio_sdl2_sink_stop_stream,
    /* void (*close)(void); */                                  &btstack_audio_sdl2_sink_close
};

static const btstack_audio_source_t btstack_audio_sdl2_source = {
    /* int (*init)(..);*/                                       &btstack_audio_sdl2_source_init,
    /* uint32_t (*get_samplerate)() */                          &btstack_audio_sdl2_source_get_samplerate,
    /* void (*set_gain)(uint8_t gain); */                       &btstack_audio_sdl2_source_set_gain,
    /* void (*start_stream(void));*/                            &btstack_audio_sdl2_source_start_stream,
    /* void (*stop_stream)(void)  */                            &btstack_audio_sdl2_source_stop_stream,
    /* void (*close)(void); */                                  &btstack_audio_sdl2_source_close
};

const btstack_audio_sink_t * btstack_audio_sdl2_sink_get_instance(void){
    return &btstack_audio_sdl2_sink;
}

const btstack_audio_source_t * btstack_audio_sdl2_source_get_instance(void){
    return &btstack_audio_sdl2_source;
}

#endif
