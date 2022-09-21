/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "lc3_test.c"

/*
 * Measure LC3 encoding load
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <btstack_debug.h>

#include "bluetooth_data_types.h"
#include "btstack_stdin.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"

#include "hxcmod.h"
#include "mods/mod.h"


#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

#include "btstack_lc3plus_fraunhofer.h"

// max config
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_NUM_BIS 2

// lc3 codec config
static uint32_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_bis = 1;

// lc3 encoder
static const btstack_lc3_encoder_t * lc3_encoder;
static btstack_lc3_encoder_google_t encoder_contexts[MAX_NUM_BIS];
static int16_t pcm[MAX_NUM_BIS * MAX_SAMPLES_PER_FRAME];

// lc3 decoder
static bool use_lc3plus_decoder = false;
static const btstack_lc3_decoder_t * lc3_decoder;
static int16_t pcm[MAX_NUM_BIS * MAX_SAMPLES_PER_FRAME];

static btstack_lc3_decoder_google_t google_decoder_contexts[MAX_NUM_BIS];
#ifdef HAVE_LC3PLUS
static btstack_lc3plus_fraunhofer_decoder_t fraunhofer_decoder_contexts[MAX_NUM_BIS];
#endif
static void * decoder_contexts[MAX_NR_BIS];

// PLC
static uint16_t plc_frame_counter;
static uint16_t plc_dopped_frame_interval;

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// mod player
static int hxcmod_initialized;
static modcontext mod_context;
static tracker_buffer_state trkbuf;

// sine generator
static uint8_t  sine_step;
static uint16_t sine_phases[MAX_NUM_BIS];

// audio producer
static enum {
    AUDIO_SOURCE_SINE,
    AUDIO_SOURCE_MODPLAYER
} audio_source = AUDIO_SOURCE_MODPLAYER;

// enumerate default codec configs
static struct {
    uint32_t samplingrate_hz;
    uint8_t  samplingrate_index;
    uint8_t  num_variants;
    struct {
        const char * name;
        btstack_lc3_frame_duration_t frame_duration;
        uint16_t octets_per_frame;
    } variants[6];
} codec_configurations[] = {
    {
        8000, 0x01, 2,
        {
            {  "8_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 26},
            {  "8_2", BTSTACK_LC3_FRAME_DURATION_10000US, 30}
        }
    },
    {
       16000, 0x03, 2,
       {
            {  "16_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 30},
            {  "16_2", BTSTACK_LC3_FRAME_DURATION_10000US, 40}
       }
    },
    {
        24000, 0x05, 2,
        {
            {  "24_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 45},
            {  "24_2", BTSTACK_LC3_FRAME_DURATION_10000US, 60}
       }
    },
    {
        32000, 0x06, 2,
        {
            {  "32_1",  BTSTACK_LC3_FRAME_DURATION_7500US, 60},
            {  "32_2", BTSTACK_LC3_FRAME_DURATION_10000US, 80}
        }
    },
    {
        44100, 0x07, 2,
        {
            { "441_1",  BTSTACK_LC3_FRAME_DURATION_7500US,  97},
            { "441_2", BTSTACK_LC3_FRAME_DURATION_10000US, 130}
        }
    },
    {
        48000, 0x08, 6,
        {
            {  "48_1", BTSTACK_LC3_FRAME_DURATION_7500US, 75},
            {  "48_2", BTSTACK_LC3_FRAME_DURATION_10000US, 100},
            {  "48_3", BTSTACK_LC3_FRAME_DURATION_7500US, 90},
            {  "48_4", BTSTACK_LC3_FRAME_DURATION_10000US, 120},
            {  "48_5", BTSTACK_LC3_FRAME_DURATION_7500US, 117},
            {  "48_6", BTSTACK_LC3_FRAME_DURATION_10000US, 155}
        }
    },
};


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

static void show_usage(void);

static void print_config(void) {
    printf("Config '%s_%u': %u, %s ms, %u octets - %s, drop frame interval %u, decoder %s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_bis,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           audio_source == AUDIO_SOURCE_SINE ? "Sine" : "Modplayer",
           plc_dopped_frame_interval,
           use_lc3plus_decoder ? "LC3plus" : "LC3");
}

static void setup_lc3_encoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < num_bis ; channel++){
        btstack_lc3_encoder_google_t * context = &encoder_contexts[channel];
        lc3_encoder = btstack_lc3_encoder_google_init_instance(context);
        lc3_encoder->configure(context, sampling_frequency_hz, frame_duration, octets_per_frame);
    }
    number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(number_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
    printf("LC3 Encoder config: %u hz, frame duration %s ms, num samples %u, num octets %u\n",
           sampling_frequency_hz, frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           number_samples_per_frame, octets_per_frame);
}

static void setup_lc3_decoder(void){
    uint8_t channel;
        for (channel = 0 ; channel < num_bis ; channel++){
            // pick decoder
            void * decoder_context = NULL;
#ifdef HAVE_LC3PLUS
             if (use_lc3plus_decoder){
                decoder_context = &fraunhofer_decoder_contexts[channel];
                lc3_decoder = btstack_lc3plus_fraunhofer_decoder_init_instance(decoder_context);
            }
            else
#endif
            {
                decoder_context = &google_decoder_contexts[channel];
                lc3_decoder = btstack_lc3_decoder_google_init_instance(decoder_context);
            }
            decoder_contexts[channel] = decoder_context;
            lc3_decoder->configure(decoder_context, sampling_frequency_hz, frame_duration, octets_per_frame);
        }
    number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);
    btstack_assert(number_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
}

static void setup_mod_player(void){
    if (!hxcmod_initialized) {
        hxcmod_initialized = hxcmod_init(&mod_context);
        btstack_assert(hxcmod_initialized != 0);
    }
    hxcmod_unload(&mod_context);
    hxcmod_setcfg(&mod_context, sampling_frequency_hz, 16, 1, 1, 1);
    hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
}

static void generate_audio(void){
    uint16_t sample;
    switch (audio_source) {
        case AUDIO_SOURCE_SINE:
            // generate sine wave for all channels
            for (sample = 0 ; sample < number_samples_per_frame ; sample++){
                uint8_t channel;
                for (channel = 0; channel < num_bis; channel++) {
                    int16_t value = sine_int16[sine_phases[channel]] / 4;
                    pcm[sample * num_bis + channel] = value;
                    sine_phases[channel] += sine_step * (1+channel);    // second channel, double frequency
                    if (sine_phases[channel] >= (sizeof(sine_int16) / sizeof(int16_t))) {
                        sine_phases[channel] = 0;
                    }
                }
            }
            break;
        case AUDIO_SOURCE_MODPLAYER:
            // mod player configured for stereo
            hxcmod_fillbuffer(&mod_context, (unsigned short *) pcm, number_samples_per_frame, &trkbuf);
            if (num_bis == 1) {
                // stereo -> mono
                uint16_t i;
                for (i=0;i<number_samples_per_frame;i++){
                    pcm[i] = (pcm[2*i] / 2) + (pcm[2*i+1] / 2);
                }
            }
            break;
        default:
            btstack_unreachable();
            break;
    }
}

static void test_encoder(){
#ifdef HAVE_POSIX_FILE_IO
    wav_writer_open("lc3_test.wav", 1, sampling_frequency_hz);
#endif

    // encode 10 seconds of music
    uint32_t audio_duration_in_seconds = 10;
    uint32_t total_samples = sampling_frequency_hz * audio_duration_in_seconds;
    uint32_t generated_samples = 0;
    uint32_t player_ms = 0;
    uint32_t encoder_ms = 0;
    uint32_t decoder_ms = 0;
    plc_frame_counter = 0;

    printf("Encoding and decoding %u seconds of audio...\n", audio_duration_in_seconds);
    while (generated_samples < total_samples){

        // generate audio
        uint32_t block_start_ms = btstack_run_loop_get_time_ms();
        generate_audio();
        uint32_t block_generated_ms  = btstack_run_loop_get_time_ms();

        // encode frame
        uint8_t buffer[200];
        lc3_encoder->encode_signed_16(&encoder_contexts[0], pcm, 1, buffer);
        generated_samples += number_samples_per_frame;
        uint32_t block_encoded_ms  = btstack_run_loop_get_time_ms();
        plc_frame_counter++;

        uint8_t BFI = 0;
        // simulate dropped packets
        if ((plc_dopped_frame_interval != 0) && (plc_frame_counter == plc_dopped_frame_interval)){
            plc_frame_counter = 0;
            BFI = 1;
        }

        // decode codec frame
        uint8_t tmp_BEC_detect;
        (void) lc3_decoder->decode_signed_16(decoder_contexts[0], buffer, BFI, pcm, 1, &tmp_BEC_detect);

        uint32_t block_decoded_ms  = btstack_run_loop_get_time_ms();

#ifdef HAVE_POSIX_FILE_IO
        wav_writer_write_int16(number_samples_per_frame, pcm);
#endif

        // summary
        player_ms  += block_generated_ms - block_start_ms;
        encoder_ms += block_encoded_ms   - block_generated_ms;
        decoder_ms += block_decoded_ms   - block_encoded_ms;
    }
    printf("Player:  time %5u ms, duty cycle %3u %%\n", player_ms,  player_ms  / audio_duration_in_seconds / 10);
    printf("Encoder: time %5u ms, duty cycle %3u %%\n", encoder_ms, encoder_ms / audio_duration_in_seconds / 10);
    printf("Decoder: time %5u ms, duty cycle %3u %%\n", decoder_ms, decoder_ms / audio_duration_in_seconds / 10);

#ifdef HAVE_POSIX_FILE_IO
    wav_writer_close();
#endif
}

static void show_usage(void){
    printf("\n--- LC3 Encoder Test Console ---\n");
    print_config();
    printf("---\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("t - toggle sine / modplayer\n");
    printf("p - simulated dropped frames\n");
#ifdef HAVE_LC3PLUS
    printf("q - use LC3plus\n");
#endif
    printf("s - start test\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'p':
            plc_dopped_frame_interval = 16 - plc_dopped_frame_interval;
            print_config();
            break;
#ifdef HAVE_LC3PLUS
        case 'q':
            use_lc3plus_decoder = true;
            // enforce 10ms as 7.5ms is not supported
            if ((menu_variant & 1) == 0){
                menu_variant++;
            }
            print_config();
            break;
#endif
        case 'f':
            menu_sampling_frequency++;
            if (menu_sampling_frequency >= 6){
                menu_sampling_frequency = 0;
            }
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 'v':
            menu_variant++;
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 's':
            // use values from table
            sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
            octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
            frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

            // get num samples per frame
            setup_lc3_encoder();
            setup_lc3_decoder();

            // setup mod player
            setup_mod_player();

            // setup sine generator
            if (sampling_frequency_hz == 44100){
                sine_step = 2;
            } else {
                sine_step = 96000 / sampling_frequency_hz;
            }
            test_encoder();
            break;
        case 't':
            audio_source = 1 - audio_source;
            print_config();
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argv;
    (void) argc;
    btstack_stdin_setup(stdin_process);
    // default config
    menu_sampling_frequency = 5;
    menu_variant = 4;
    audio_source = AUDIO_SOURCE_SINE;
    show_usage();
    return 0;
}
