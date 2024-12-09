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

#define BTSTACK_FILE__ "le_audio_demo_util_source.c"

#include "le_audio_demo_util_source.h"

#include <stdio.h>
#include <inttypes.h>
#include <math.h>

#include "btstack_config.h"
#include "btstack_bool.h"
#include "btstack_debug.h"
#include "btstack_ring_buffer.h"

#include "hci.h"
#include "btstack_audio.h"
#include "btstack_lc3_google.h"
#include "btstack_lc3plus_fraunhofer.h"

#include "hxcmod.h"
#include "mods/mod.h"

#define MAX_CHANNELS 5
#define MAX_SAMPLES_PER_10MS_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

// live recording
#define RECORDING_PREBUFFER__MS          20
#define RECORDING_BUFFER_MS              40

// SOURCE
static float sine_frequencies[MAX_CHANNELS] = {
    261.63, // C-4
    311.13, // Es-4
    369.99, // G-4
    493.88, // B-4
    587.33, // D-5
};

//  48000 / 261.6 = 183.46
#define SINE_MAX_SAMPLES_AT_48KHZ 185
#define PI 3.14159265358979323846

static int16_t sine_table[MAX_CHANNELS * SINE_MAX_SAMPLES_AT_48KHZ];

static uint16_t le_audio_demo_source_octets_per_frame;
static uint16_t le_audio_demo_source_packet_sequence_numbers[MAX_CHANNELS];
static uint8_t  le_audio_demo_source_iso_frame_counter;
static uint32_t le_audio_demo_source_sampling_frequency_hz;
static btstack_lc3_frame_duration_t le_audio_demo_source_frame_duration;
static uint8_t  le_audio_demo_source_num_channels;
static uint8_t  le_audio_demo_source_num_streams;
static uint8_t  le_audio_demo_source_num_channels_per_stream;
static uint16_t le_audio_demo_source_num_samples_per_frame;


static uint8_t le_audio_demo_source_iso_payload[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];

// lc3 encoder
static const btstack_lc3_encoder_t * le_audio_demo_source_lc3_encoder;
static btstack_lc3_encoder_google_t  le_audio_demo_source_encoder_contexts[MAX_CHANNELS];
static int16_t                       le_audio_demo_source_pcm[MAX_CHANNELS * MAX_SAMPLES_PER_10MS_FRAME];

// sine generator
static uint16_t le_audio_demo_source_sine_samples[MAX_CHANNELS];
static uint16_t le_audio_demo_source_sine_phases[MAX_CHANNELS];

// mod player
static bool                 le_audio_demo_source_hxcmod_initialized;
static modcontext           le_audio_demo_source_hxcmod_context;
static tracker_buffer_state le_audio_demo_source_hxcmod_trkbuf;

// recording / portaudio
#define SAMPLES_PER_CHANNEL_RECORDING (MAX_SAMPLES_PER_10MS_FRAME * RECORDING_BUFFER_MS / 10)
static uint8_t recording_storage[MAX_CHANNELS * 2 * SAMPLES_PER_CHANNEL_RECORDING];
static btstack_ring_buffer_t le_audio_demo_source_recording_buffer;
static uint16_t le_audio_demo_source_recording_stored_samples;
static uint16_t le_audio_demo_source_recording_prebuffer_samples;
static bool le_audio_demo_source_recording_streaming;
static btstack_audio_source_t const * le_audio_demo_audio_source;

// generation method
static le_audio_demo_source_generator le_audio_demo_util_source_generator = AUDIO_SOURCE_SINE;

// recording callback has channels interleaved, collect per channel
static void le_audio_util_source_recording_callback(const int16_t * buffer, uint16_t num_samples){
    log_info("store %u samples per channel", num_samples);
    uint32_t bytes_to_store = le_audio_demo_source_num_channels * num_samples * 2;
    if (bytes_to_store < btstack_ring_buffer_bytes_free(&le_audio_demo_source_recording_buffer)){
        btstack_ring_buffer_write(&le_audio_demo_source_recording_buffer, (uint8_t *)buffer, bytes_to_store);
        le_audio_demo_source_recording_stored_samples += num_samples;
    }
}

void le_audio_demo_util_source_init(void) {
    le_audio_demo_audio_source = btstack_audio_source_get_instance();
}

static bool le_audio_demo_util_source_recording_start(void){
    bool ok = false;
    if (le_audio_demo_audio_source != NULL){
        int init_ok = le_audio_demo_audio_source->init(le_audio_demo_source_num_channels, le_audio_demo_source_sampling_frequency_hz,
                                         &le_audio_util_source_recording_callback);
        log_info("recording initialized, ok %u", init_ok);
        if (init_ok){
            btstack_ring_buffer_init(&le_audio_demo_source_recording_buffer, recording_storage, sizeof(recording_storage));
            le_audio_demo_source_recording_prebuffer_samples = le_audio_demo_source_num_channels * le_audio_demo_source_sampling_frequency_hz * RECORDING_PREBUFFER__MS / 1000;
            le_audio_demo_source_recording_streaming = false;
            le_audio_demo_source_recording_stored_samples = 0;
            le_audio_demo_audio_source->start_stream();
            log_info("recording start, %u prebuffer samples per channel", le_audio_demo_source_recording_prebuffer_samples / le_audio_demo_source_num_channels);
            ok = true;
        }
    }
    return ok;
}

static void le_audio_demo_util_source_recording_stop(void) {
    le_audio_demo_audio_source->stop_stream();
}

static void le_audio_demo_source_setup_lc3_encoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < le_audio_demo_source_num_channels ; channel++){
        btstack_lc3_encoder_google_t * context = &le_audio_demo_source_encoder_contexts[channel];
        le_audio_demo_source_lc3_encoder = btstack_lc3_encoder_google_init_instance(context);
        le_audio_demo_source_lc3_encoder->configure(context, le_audio_demo_source_sampling_frequency_hz, le_audio_demo_source_frame_duration, le_audio_demo_source_octets_per_frame);
    }

    printf("LC3 Encoder config: %" PRIu32 " hz, frame duration %s ms, num samples %u, num octets %u\n",
           le_audio_demo_source_sampling_frequency_hz, le_audio_demo_source_frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           le_audio_demo_source_num_samples_per_frame, le_audio_demo_source_octets_per_frame);
}

static void le_audio_demo_source_setup_mod_player(void){
    if (!le_audio_demo_source_hxcmod_initialized) {
        le_audio_demo_source_hxcmod_initialized = hxcmod_init(&le_audio_demo_source_hxcmod_context);
        btstack_assert(le_audio_demo_source_hxcmod_initialized != 0);
    }
    hxcmod_unload(&le_audio_demo_source_hxcmod_context);
    hxcmod_setcfg(&le_audio_demo_source_hxcmod_context, le_audio_demo_source_sampling_frequency_hz, 1, 1);
    hxcmod_load(&le_audio_demo_source_hxcmod_context, (void *) &mod_data, mod_len);
}

static void le_audio_demo_source_setup_sine_generator(void){
    // pre-compute sine for all channels
    uint8_t channel;
    for (channel = 0; channel < MAX_CHANNELS ; channel++){
        float frequency_hz =  (float) sine_frequencies[channel];
        float samplerate_hz = (float) le_audio_demo_source_sampling_frequency_hz;
        float period_samples =samplerate_hz / frequency_hz;
        le_audio_demo_source_sine_samples[channel] = (uint16_t) period_samples;
        le_audio_demo_source_sine_phases[channel] = 0;
        uint16_t i;
        for (i=0;i<le_audio_demo_source_sine_samples[channel] ;i++){
            float sine_value = sin(2 * PI * i / period_samples);
            sine_table[channel*SINE_MAX_SAMPLES_AT_48KHZ + i] = (int16_t)(sine_value * 32767);
        }
    }
}

void le_audio_demo_util_source_configure(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                         btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame) {
    le_audio_demo_source_sampling_frequency_hz = sampling_frequency_hz;
    le_audio_demo_source_frame_duration = frame_duration;
    le_audio_demo_source_octets_per_frame = octets_per_frame;
    le_audio_demo_source_num_streams = num_streams;
    le_audio_demo_source_num_channels_per_stream = num_channels_per_stream;

    le_audio_demo_source_num_channels = num_streams * num_channels_per_stream;
    btstack_assert((le_audio_demo_source_num_channels > 0) || (le_audio_demo_source_num_channels <= MAX_CHANNELS));

    le_audio_demo_source_num_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(le_audio_demo_source_num_samples_per_frame <= MAX_SAMPLES_PER_10MS_FRAME);

    // setup encoder
    le_audio_demo_source_setup_lc3_encoder();

    // setup sine generator
    le_audio_demo_source_setup_sine_generator();

    // setup mod player
    le_audio_demo_source_setup_mod_player();
}

void le_audio_demo_util_source_generate_iso_frame(le_audio_demo_source_generator generator) {
    btstack_assert(le_audio_demo_source_octets_per_frame != 0);
    uint16_t sample;
    bool encode_pcm = true;

    // lazy init of btstack_audio, fallback to sine
    if ((generator == AUDIO_SOURCE_RECORDING) && (le_audio_demo_util_source_generator != AUDIO_SOURCE_RECORDING)){
        bool ok = le_audio_demo_util_source_recording_start();
        if (ok) {
            le_audio_demo_util_source_generator = generator;
        } else {
            generator = AUDIO_SOURCE_SINE;
        }
    }

    // stop recording
    if ((generator != AUDIO_SOURCE_RECORDING) && (le_audio_demo_util_source_generator == AUDIO_SOURCE_RECORDING)) {
        le_audio_demo_util_source_recording_stop();
    }

    switch (generator){
        case AUDIO_SOURCE_COUNTER:
            encode_pcm = false;
            memset(le_audio_demo_source_iso_payload, le_audio_demo_source_iso_frame_counter++, sizeof(le_audio_demo_source_iso_payload));
            break;
        case AUDIO_SOURCE_SINE:
            // use pre-computed sine for all channels
            for (sample = 0 ; sample < le_audio_demo_source_num_samples_per_frame ; sample++){
                uint8_t i;
                for (i = 0; i < le_audio_demo_source_num_channels; i++) {
                    int16_t value = sine_table[i*SINE_MAX_SAMPLES_AT_48KHZ + le_audio_demo_source_sine_phases[i]];
                    le_audio_demo_source_pcm[sample * le_audio_demo_source_num_channels + i] = value;
                    le_audio_demo_source_sine_phases[i] += 1;
                    if (le_audio_demo_source_sine_phases[i] >= le_audio_demo_source_sine_samples[i]) {
                        le_audio_demo_source_sine_phases[i] = 0;
                    }
                }
            }
            break;
        case AUDIO_SOURCE_MODPLAYER:
            // mod player configured for stereo
            hxcmod_fillbuffer(&le_audio_demo_source_hxcmod_context, le_audio_demo_source_pcm, le_audio_demo_source_num_samples_per_frame, &le_audio_demo_source_hxcmod_trkbuf);
            // stereo -> mono
            if (le_audio_demo_source_num_channels == 1) {
                uint16_t i;
                for (i=0;i<le_audio_demo_source_num_samples_per_frame;i++){
                    le_audio_demo_source_pcm[i] = (le_audio_demo_source_pcm[2*i] / 2) + (le_audio_demo_source_pcm[2*i+1] / 2);
                }
            }
            // duplicate stereo channels
            if (le_audio_demo_source_num_channels > 2) {
                int16_t i;
                for (i = le_audio_demo_source_num_samples_per_frame - 1; i >= 0; i--) {
                    uint16_t channel_dst;
                    for (channel_dst=0; channel_dst < le_audio_demo_source_num_channels; channel_dst++){
                        uint16_t channel_src = channel_dst & 1;
                        le_audio_demo_source_pcm[i * le_audio_demo_source_num_channels + channel_dst] =
                                le_audio_demo_source_pcm[i * 2 + channel_src];
                    }
                }
            }
            break;
        case AUDIO_SOURCE_RECORDING:
            if (le_audio_demo_source_recording_streaming == false){
                if (le_audio_demo_source_recording_stored_samples >= le_audio_demo_source_recording_prebuffer_samples){
                    // start streaming audio
                    le_audio_demo_source_recording_streaming = true;
                    log_info("Streaming started");

                }
            }
            if (le_audio_demo_source_recording_streaming){
                uint32_t bytes_needed = le_audio_demo_source_num_channels * le_audio_demo_source_num_samples_per_frame * 2;
                if (btstack_ring_buffer_bytes_available(&le_audio_demo_source_recording_buffer) >= bytes_needed){
                    uint32_t bytes_read;
                    btstack_ring_buffer_read(&le_audio_demo_source_recording_buffer, (uint8_t*) le_audio_demo_source_pcm, bytes_needed, &bytes_read);
                    btstack_assert(bytes_needed == bytes_needed);
                    log_info("Read %u samples per channel", le_audio_demo_source_num_samples_per_frame);
                    le_audio_demo_source_recording_stored_samples -= le_audio_demo_source_num_samples_per_frame;
                } else {
                    le_audio_demo_source_recording_streaming = false;
                    log_info("Streaming underrun");
                }
            } else {
                memset(le_audio_demo_source_pcm, 0, sizeof(le_audio_demo_source_pcm));
            }
            break;
        default:
            btstack_unreachable();
            break;
    }

    if (encode_pcm){
        uint8_t i;
        for (i=0;i<le_audio_demo_source_num_channels;i++){
            le_audio_demo_source_lc3_encoder->encode_signed_16(&le_audio_demo_source_encoder_contexts[i], &le_audio_demo_source_pcm[i], le_audio_demo_source_num_channels, &le_audio_demo_source_iso_payload[i * MAX_LC3_FRAME_BYTES]);
        }
    }

    le_audio_demo_util_source_generator = generator;
}

void le_audio_demo_util_source_send(uint8_t stream_index, hci_con_handle_t con_handle){
    btstack_assert(le_audio_demo_source_octets_per_frame != 0);

    hci_reserve_packet_buffer();

    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, ((uint16_t) con_handle) | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + le_audio_demo_source_num_channels_per_stream * le_audio_demo_source_octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, le_audio_demo_source_packet_sequence_numbers[stream_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, le_audio_demo_source_num_channels_per_stream * le_audio_demo_source_octets_per_frame);
    uint16_t offset = 8;
    // copy encoded payload
    uint8_t i;
    for (i=0; i<le_audio_demo_source_num_channels_per_stream;i++) {
        uint8_t effective_channel = (stream_index * le_audio_demo_source_num_channels_per_stream) + i;
        memcpy(&buffer[offset], &le_audio_demo_source_iso_payload[effective_channel * MAX_LC3_FRAME_BYTES], le_audio_demo_source_octets_per_frame);
        offset += le_audio_demo_source_octets_per_frame;
    }
    // send
    hci_send_iso_packet_buffer(offset);

    le_audio_demo_source_packet_sequence_numbers[stream_index]++;
}

void le_audio_demo_util_source_close(void){
    if (le_audio_demo_audio_source != NULL){
        le_audio_demo_audio_source->close();
    }
}
