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

#define __BTSTACK_FILE__ "btstack_audio_embedded.c"

/*
 *  btstack_audio_embedded.c
 *
 *  Implementation of btstack_audio.h using hal_audio.h for embedded run loop
 *
 */

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_run_loop_embedded.h"
#include "hal_audio.h"

// allow to compile all files in embedded folder even if there's no audio support
#ifdef HAVE_HAL_AUDIO

#define DRIVER_POLL_INTERVAL_MS          5

// client
static void (*playback_callback)(int16_t * buffer, uint16_t num_samples);
static void (*recording_callback)(const int16_t * buffer, uint16_t num_samples);

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer;

static volatile unsigned int output_buffer_to_play;
static          unsigned int output_buffer_to_fill;
static          unsigned int output_buffer_count;
static          unsigned int output_buffer_samples;

static void btstack_audio_audio_played(uint8_t buffer_index){
    output_buffer_to_play = (buffer_index + 1) % output_buffer_count;
}

static void btstack_audio_audio_recorded(const int16_t * samples, uint16_t num_samples){
    UNUSED(samples);
    UNUSED(num_samples);
}

static void driver_timer_handler(btstack_timer_source_t * ts){

    // playback buffer ready to fill
    if (playback_callback && output_buffer_to_play != output_buffer_to_fill){
        // fill buffer
        int16_t * buffer = hal_audio_get_output_buffer(output_buffer_to_fill);
        (*playback_callback)(buffer, output_buffer_samples);

        // next
        output_buffer_to_fill = (output_buffer_to_fill + 1 ) % output_buffer_count;
    }

    // recording buffer ready to process
    if (recording_callback){
    }    

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_embedded_init(
    uint8_t channels,
    uint32_t samplerate, 
    void (*playback)(int16_t * buffer, uint16_t num_samples),
    void (*recording)(const int16_t * buffer, uint16_t num_samples)
){
    playback_callback  = playback;
    recording_callback = recording;

    void (*buffer_played_callback)  (uint8_t buffer_index)                         = NULL;
    void (*buffer_recorded_callback)(const int16_t * buffer, uint16_t num_samples) = NULL;
    if (playback) {
        buffer_played_callback = &btstack_audio_audio_played;
    }
    if (recording) {
        buffer_recorded_callback = &btstack_audio_audio_recorded;
    }
    hal_audio_init(channels, samplerate, buffer_played_callback, buffer_recorded_callback);

    return 0;
}

static void btstack_audio_embedded_start_stream(void){

    if (playback_callback){
    
        output_buffer_count   = hal_audio_get_num_output_buffers();
        output_buffer_samples = hal_audio_get_num_output_buffer_samples();

        // pre-fill HAL buffers
        uint16_t i;
        for (i=0;i<output_buffer_count;i++){
            int16_t * buffer = hal_audio_get_output_buffer(i);
            (*playback_callback)(buffer, output_buffer_samples);
        }

        output_buffer_to_play = 0;
        output_buffer_to_fill = 0;
    }

    // TODO: setup input

    // start playback
    hal_audio_start();

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer, &driver_timer_handler);
    btstack_run_loop_set_timer(&driver_timer, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer);
}

static void btstack_audio_embedded_close(void){
    // stop timer
    btstack_run_loop_remove_timer(&driver_timer);
    // close HAL
    hal_audio_close();
}

static const btstack_audio_t btstack_audio_embedded = {
    /* int (*init)(..);*/                                       &btstack_audio_embedded_init,
    /* void (*start_stream(void));*/                            &btstack_audio_embedded_start_stream,
    /* void (*close)(void); */                                  &btstack_audio_embedded_close
};

const btstack_audio_t * btstack_audio_embedded_get_instance(void){
    return &btstack_audio_embedded;
}

#endif 
