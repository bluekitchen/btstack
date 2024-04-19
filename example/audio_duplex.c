/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "audio_duplex.c"

/* EXAMPLE_START(audio_duplex): Audio Driver - Forward Audio from Source to Sink
 *
 */

#include "btstack.h"
#include <stdio.h>

// uncomment to test start/stop of loopback / audio driver
// #define TEST_START_STOP_INTERVAL 5000

// change to 1 for mono input and 2 for stereo input
#define NUM_INPUT_CHANNELS 1

#define BYTES_PER_SAMPLE (NUM_INPUT_CHANNELS * 2)

#ifdef TEST_START_STOP_INTERVAL
static void stop_loopback(btstack_timer_source_t * ts);
#endif

static btstack_timer_source_t start_stop_timer;

// samplerate
const uint32_t samplerate = 16000;

// ring buffer for audio
#define BUFFER_SAMPLES 2048
static uint16_t              audio_buffer_storage[BUFFER_SAMPLES * NUM_INPUT_CHANNELS];
static btstack_ring_buffer_t audio_buffer;

// transfer buffer
#define TRANSFER_SAMPLES 128
static int16_t transfer_buffer[TRANSFER_SAMPLES * NUM_INPUT_CHANNELS];

// playback starts after audio_buffer is half full
static bool playback_started;

// sample couners
static int count_recording;
static int count_playback;

static void audio_recording(const int16_t * pcm_buffer, uint16_t num_samples_to_write){
    count_recording += num_samples_to_write;
    int err = btstack_ring_buffer_write(&audio_buffer, (uint8_t *) pcm_buffer, num_samples_to_write * BYTES_PER_SAMPLE);
    if (err){
        printf("Failed to store %u samples\n", num_samples_to_write);
    }
}

static void audio_playback(int16_t * pcm_buffer, uint16_t num_samples_to_write){
    int num_samples_in_buffer = btstack_ring_buffer_bytes_available(&audio_buffer) / BYTES_PER_SAMPLE;
    if (playback_started == false){
        if ( num_samples_in_buffer >= (BUFFER_SAMPLES / 2)){
            playback_started = true;
        }
    }
    count_playback += num_samples_to_write;

    if (playback_started){

        while (num_samples_to_write){

            num_samples_in_buffer = btstack_ring_buffer_bytes_available(&audio_buffer) / BYTES_PER_SAMPLE;
            int num_samples_ready = btstack_min(num_samples_in_buffer, num_samples_to_write);
            // limit by transfer_buffer
            int num_samples_from_buffer = btstack_min(num_samples_ready, TRANSFER_SAMPLES);
            if (!num_samples_from_buffer) break;
            uint32_t bytes_read;
            btstack_ring_buffer_read(&audio_buffer, (uint8_t *) transfer_buffer, num_samples_from_buffer * BYTES_PER_SAMPLE, &bytes_read);

#if (NUM_INPUT_CHANNELS == 1)
            // duplicate samples for stereo output
            int i;
            for (i=0; i < num_samples_from_buffer;i++) {
                *pcm_buffer++ = transfer_buffer[i];
                *pcm_buffer++ = transfer_buffer[i];
                num_samples_to_write--;
            }
#endif

#if (NUM_INPUT_CHANNELS == 2)
            // copy samples
            int i;
            int j = 0;
            for (i=0; i < num_samples_from_buffer;i++) {
                *pcm_buffer++ = transfer_buffer[j++];
                *pcm_buffer++ = transfer_buffer[j++];
                num_samples_to_write--;
            }
#endif

        }

        // warn about underrun
        if (num_samples_to_write){
            printf("Buffer underrun - recording %u, playback %u - delta %d!\n", count_recording, count_playback, count_recording - count_playback);
        }

    }

    // fill rest with silence
    memset(pcm_buffer, 0, num_samples_to_write * 4);
}

static void start_loopback(btstack_timer_source_t * ts){
    const btstack_audio_sink_t   * audio_sink   = btstack_audio_sink_get_instance();
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();

    // prepare audio buffer
    btstack_ring_buffer_init(&audio_buffer, (uint8_t*) &audio_buffer_storage[0], sizeof(audio_buffer_storage));

    // setup audio: mono input -> stereo output
    audio_sink->init(2, samplerate, &audio_playback);
    audio_source->init(NUM_INPUT_CHANNELS, samplerate, &audio_recording);

    // start duplex
    audio_sink->start_stream();
    audio_source->start_stream();

    printf("Start Audio Loopback\n");

#ifdef TEST_START_STOP_INTERVAL
    // schedule stop
    btstack_run_loop_set_timer_handler(ts, &stop_loopback);
    btstack_run_loop_set_timer(ts, TEST_START_STOP_INTERVAL);
    btstack_run_loop_add_timer(ts);
#else
    UNUSED(ts);
#endif
}

#ifdef TEST_START_STOP_INTERVAL
static void stop_loopback(btstack_timer_source_t * ts){
    const btstack_audio_sink_t   * audio_sink   = btstack_audio_sink_get_instance();
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();

    // stop streams
    audio_sink->stop_stream();
    audio_source->stop_stream();

    // close audio
    audio_sink->close();
    audio_source->close();

    playback_started = false;

    printf("Stop Audio Loopback\n");

    // schedule stop
    btstack_run_loop_set_timer_handler(ts, &start_loopback);
    btstack_run_loop_set_timer(ts, TEST_START_STOP_INTERVAL);
    btstack_run_loop_add_timer(ts);
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // check audio interface
    const btstack_audio_sink_t * audio_sink   = btstack_audio_sink_get_instance();
    if (!audio_sink){
        printf("BTstack Audio Sink not setup\n");
        return 10;
    }

    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();
    if (!audio_source){
        printf("BTstack Audio Source not setup\n");
        return 10;
    }

    start_loopback(&start_stop_timer);

    return 0;
}

/* EXAMPLE_END */
