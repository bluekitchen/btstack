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

#include "btstack_bool.h"
#include "btstack_config.h"
#include <btstack_debug.h>
#include <stdio.h>

#include "hci.h"
#include "btstack_audio.h"
#include "btstack_lc3_google.h"
#include "btstack_lc3plus_fraunhofer.h"

#include "hxcmod.h"
#include "mods/mod.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#include "btstack_ring_buffer.h"

#endif

//#define DEBUG_PLC
#ifdef DEBUG_PLC
#define printf_plc(...) printf(__VA_ARGS__)
#else
#define printf_plc(...)  (void)(0);
#endif

#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

// playback
#define MAX_NUM_LC3_FRAMES   15
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_BYTES_PER_SAMPLE)
#define PLAYBACK_START_MS (MAX_NUM_LC3_FRAMES * 20 / 3)

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// analysis
#define PACKET_PREFIX_LEN 10

// SOURCE

// input signal: pre-computed int16 sine wave, 96000 Hz at 300 Hz
static const int16_t sine_int16[] = {
        0,    643,   1286,   1929,   2571,   3212,   3851,   4489,   5126,   5760,
        6393,   7022,   7649,   8273,   8894,   9512,  10126,  10735,  11341,  11943,
        12539,  13131,  13718,  14300,  14876,  15446,  16011,  16569,  17121,  17666,
        18204,  18736,  19260,  19777,  20286,  20787,  21280,  21766,  22242,  22710,
        23170,  23620,  24062,  24494,  24916,  25329,  25732,  26126,  26509,  26882,
        27245,  27597,  27938,  28269,  28589,  28898,  29196,  29482,  29757,  30021,
        30273,  30513,  30742,  30958,  31163,  31356,  31537,  31705,  31862,  32006,
        32137,  32257,  32364,  32458,  32540,  32609,  32666,  32710,  32742,  32761,
        32767,  32761,  32742,  32710,  32666,  32609,  32540,  32458,  32364,  32257,
        32137,  32006,  31862,  31705,  31537,  31356,  31163,  30958,  30742,  30513,
        30273,  30021,  29757,  29482,  29196,  28898,  28589,  28269,  27938,  27597,
        27245,  26882,  26509,  26126,  25732,  25329,  24916,  24494,  24062,  23620,
        23170,  22710,  22242,  21766,  21280,  20787,  20286,  19777,  19260,  18736,
        18204,  17666,  17121,  16569,  16011,  15446,  14876,  14300,  13718,  13131,
        12539,  11943,  11341,  10735,  10126,   9512,   8894,   8273,   7649,   7022,
        6393,   5760,   5126,   4489,   3851,   3212,   2571,   1929,   1286,    643,
        0,   -643,  -1286,  -1929,  -2571,  -3212,  -3851,  -4489,  -5126,  -5760,
        -6393,  -7022,  -7649,  -8273,  -8894,  -9512, -10126, -10735, -11341, -11943,
        -12539, -13131, -13718, -14300, -14876, -15446, -16011, -16569, -17121, -17666,
        -18204, -18736, -19260, -19777, -20286, -20787, -21280, -21766, -22242, -22710,
        -23170, -23620, -24062, -24494, -24916, -25329, -25732, -26126, -26509, -26882,
        -27245, -27597, -27938, -28269, -28589, -28898, -29196, -29482, -29757, -30021,
        -30273, -30513, -30742, -30958, -31163, -31356, -31537, -31705, -31862, -32006,
        -32137, -32257, -32364, -32458, -32540, -32609, -32666, -32710, -32742, -32761,
        -32767, -32761, -32742, -32710, -32666, -32609, -32540, -32458, -32364, -32257,
        -32137, -32006, -31862, -31705, -31537, -31356, -31163, -30958, -30742, -30513,
        -30273, -30021, -29757, -29482, -29196, -28898, -28589, -28269, -27938, -27597,
        -27245, -26882, -26509, -26126, -25732, -25329, -24916, -24494, -24062, -23620,
        -23170, -22710, -22242, -21766, -21280, -20787, -20286, -19777, -19260, -18736,
        -18204, -17666, -17121, -16569, -16011, -15446, -14876, -14300, -13718, -13131,
        -12539, -11943, -11341, -10735, -10126,  -9512,  -8894,  -8273,  -7649,  -7022,
        -6393,  -5760,  -5126,  -4489,  -3851,  -3212,  -2571,  -1929,  -1286,   -643,
};

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
static int16_t                       le_audio_demo_source_pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];

// sine generator
static uint8_t  le_audio_demo_source_sine_step;
static uint16_t le_audio_demo_source_sine_phases[MAX_CHANNELS];

// mod player
static bool                 le_audio_demo_source_hxcmod_initialized;
static modcontext           le_audio_demo_source_hxcmod_context;
static tracker_buffer_state le_audio_demo_source_hxcmod_trkbuf;

void le_audio_demo_util_source_init(void){
}

static void le_audio_demo_source_setup_lc3_encoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < le_audio_demo_source_num_channels ; channel++){
        btstack_lc3_encoder_google_t * context = &le_audio_demo_source_encoder_contexts[channel];
        le_audio_demo_source_lc3_encoder = btstack_lc3_encoder_google_init_instance(context);
        le_audio_demo_source_lc3_encoder->configure(context, le_audio_demo_source_sampling_frequency_hz, le_audio_demo_source_frame_duration, le_audio_demo_source_octets_per_frame);
    }

    printf("LC3 Encoder config: %u hz, frame duration %s ms, num samples %u, num octets %u\n",
           le_audio_demo_source_sampling_frequency_hz, le_audio_demo_source_frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           le_audio_demo_source_num_samples_per_frame, le_audio_demo_source_octets_per_frame);
}

static void le_audio_demo_source_setup_mod_player(void){
    if (!le_audio_demo_source_hxcmod_initialized) {
        le_audio_demo_source_hxcmod_initialized = hxcmod_init(&le_audio_demo_source_hxcmod_context);
        btstack_assert(le_audio_demo_source_hxcmod_initialized != 0);
    }
    hxcmod_unload(&le_audio_demo_source_hxcmod_context);
    hxcmod_setcfg(&le_audio_demo_source_hxcmod_context, le_audio_demo_source_sampling_frequency_hz, 16, 1, 1, 1);
    hxcmod_load(&le_audio_demo_source_hxcmod_context, (void *) &mod_data, mod_len);
}

void le_audio_demo_util_source_configure(uint8_t num_streams, uint8_t num_channels_per_stream, uint32_t sampling_frequency_hz,
                                         btstack_lc3_frame_duration_t frame_duration, uint16_t octets_per_frame) {
    le_audio_demo_source_sampling_frequency_hz = sampling_frequency_hz;
    le_audio_demo_source_frame_duration = frame_duration;
    le_audio_demo_source_octets_per_frame = octets_per_frame;
    le_audio_demo_source_num_streams = num_streams;
    le_audio_demo_source_num_channels_per_stream = num_channels_per_stream;

    le_audio_demo_source_num_channels = num_streams * num_channels_per_stream;
    btstack_assert((le_audio_demo_source_num_channels == 1) || (le_audio_demo_source_num_channels == 2));

    le_audio_demo_source_num_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(le_audio_demo_source_num_samples_per_frame <= MAX_SAMPLES_PER_FRAME);

    // setup encoder
    le_audio_demo_source_setup_lc3_encoder();

    // setup sine generator
    if (sampling_frequency_hz == 44100){
        le_audio_demo_source_sine_step = 2;
    } else {
        le_audio_demo_source_sine_step = 96000 / sampling_frequency_hz;
    }

    // setup mod player
    le_audio_demo_source_setup_mod_player();
};

void le_audio_demo_util_source_generate_iso_frame(le_audio_demo_source_generator generator) {
    btstack_assert(le_audio_demo_source_octets_per_frame != 0);
    uint16_t sample;
    bool encode_pcm = true;
    switch (generator){
        case AUDIO_SOURCE_COUNTER:
            encode_pcm = false;
            memset(le_audio_demo_source_iso_payload, le_audio_demo_source_iso_frame_counter++, sizeof(le_audio_demo_source_iso_payload));
            break;
        case AUDIO_SOURCE_SINE:
            // generate sine wave for all channels
            for (sample = 0 ; sample < le_audio_demo_source_num_samples_per_frame ; sample++){
                uint8_t i;
                for (i = 0; i < le_audio_demo_source_num_channels; i++) {
                    int16_t value = sine_int16[le_audio_demo_source_sine_phases[i]] / 4;
                    le_audio_demo_source_pcm[sample * le_audio_demo_source_num_channels + i] = value;
                    le_audio_demo_source_sine_phases[i] += le_audio_demo_source_sine_step * (1 + i);    // second channel, double frequency
                    if (le_audio_demo_source_sine_phases[i] >= (sizeof(sine_int16) / sizeof(int16_t))) {
                        le_audio_demo_source_sine_phases[i] = 0;
                    }
                }
            }
            break;
        case AUDIO_SOURCE_MODPLAYER:
            // mod player configured for stereo
            hxcmod_fillbuffer(&le_audio_demo_source_hxcmod_context, (unsigned short *) le_audio_demo_source_pcm, le_audio_demo_source_num_samples_per_frame, &le_audio_demo_source_hxcmod_trkbuf);
            if (le_audio_demo_source_num_channels == 1) {
                // stereo -> mono
                uint16_t i;
                for (i=0;i<le_audio_demo_source_num_samples_per_frame;i++){
                    le_audio_demo_source_pcm[i] = (le_audio_demo_source_pcm[2*i] / 2) + (le_audio_demo_source_pcm[2*i+1] / 2);
                }
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
};

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
}
