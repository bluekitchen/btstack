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

#define BTSTACK_FILE__ "audio_duplex.c"

/*
 * Audio Duplex: forward audio from BTstack audio source to audio sink - test for audio interface
 *
 */

#include "btstack.h"


// uncomment to test start/stop of loopback / audio driver
// #define TEST_START_STOP_INTERVAL 5000

#ifdef TEST_START_STOP_INTERVAL
static void stop_loopback(btstack_timer_source_t * ts);
#endif

static btstack_timer_source_t start_stop_timer;

// samplerate
const uint32_t samplerate = 16000;

// ring buffer for audio
#define BUFFER_SAMPLES 1024
static uint16_t              audio_buffer_storage[BUFFER_SAMPLES];
static btstack_ring_buffer_t audio_buffer;

// mono buffer
#define MONO_BUFFER_LEN 128
static int16_t mono_buffer[MONO_BUFFER_LEN];

// playback starts after audio_buffer is half full
static int playback_started;

// sample couners
static int count_recording;
static int count_playback;

static void audio_recording(const int16_t * pcm_buffer, uint16_t num_samples_to_write){
    count_recording += num_samples_to_write;
    int err = btstack_ring_buffer_write(&audio_buffer, (uint8_t *) pcm_buffer, num_samples_to_write * 2);
    if (err){
        printf("Failed to store %u samples\n", num_samples_to_write);
    }
}

static void audio_playback(int16_t * pcm_buffer, uint16_t num_samples_to_write){
    int num_samples_in_buffer = btstack_ring_buffer_bytes_available(&audio_buffer) / 2;
    if (playback_started == 0){
        if ( num_samples_in_buffer < (BUFFER_SAMPLES / 2)) return;
        playback_started = 1;
    }
    count_playback += num_samples_to_write;
    while (num_samples_to_write){
        num_samples_in_buffer = btstack_ring_buffer_bytes_available(&audio_buffer) / 2;
        int num_samples_ready = btstack_min(num_samples_in_buffer, num_samples_to_write);
        // limit by mono_buffer
        int num_samples_from_buffer = btstack_min(num_samples_ready, MONO_BUFFER_LEN);
        if (!num_samples_from_buffer) break;
        uint32_t bytes_read;
        btstack_ring_buffer_read(&audio_buffer, (uint8_t *) mono_buffer, num_samples_from_buffer * 2, &bytes_read);
        // duplicate samples for stereo output
        int i;
        for (i=0; i < num_samples_from_buffer;i++){
            *pcm_buffer++ = mono_buffer[i];
            *pcm_buffer++ = mono_buffer[i];
            num_samples_to_write--;
        }
    }

    // warn about underrun
    if (num_samples_to_write){
        printf("Buffer underrun - recording %u, playback %u - delta %d!\n", count_recording, count_playback, count_recording - count_playback);
    }

    // fill rest with silence
    while (num_samples_to_write){
        *pcm_buffer++ = 0;
        *pcm_buffer++ = 0;
        num_samples_to_write--;
    }
}

static void start_loopback(btstack_timer_source_t * ts){
    const btstack_audio_sink_t   * audio_sink   = btstack_audio_sink_get_instance();
    const btstack_audio_source_t * audio_source = btstack_audio_source_get_instance();

    // prepare audio buffer
    btstack_ring_buffer_init(&audio_buffer, (uint8_t*) &audio_buffer_storage[0], sizeof(audio_buffer_storage));

    // setup audio: mono input -> stereo output
    audio_sink->init(2, samplerate, &audio_playback);
    audio_source->init(1, samplerate, &audio_recording);

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

    playback_started = 0;

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
