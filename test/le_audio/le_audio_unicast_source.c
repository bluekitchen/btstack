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

#define BTSTACK_FILE__ "le_audio_unicast_source.c"

/*
 * LE Audio Unicast Source
 * Until GATT Services are available, we encode LC3 config in advertising
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <btstack_debug.h>

#include "bluetooth_data_types.h"
#include "bluetooth_company_id.h"
#include "btstack_stdin.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "btstack_lc3.h"
#include "btstack_lc3_ehima.h"

#include "hxcmod.h"
#include "mods/mod.h"

// max config
#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480

static uint8_t adv_data[] = {
        // Manufacturer Specific Data to indicate codec
        9,
        BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH & 0xff,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH >> 8,
        0, // subtype: LE Audio Connection Source
        0, // flags
        1, // num bis
        8, // sampling frequency in khz
        0, // frame duration
        26, // octets per frame
        // name
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e'
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

static bd_addr_t remote;
static hci_con_handle_t remote_handle;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static unsigned int     next_cis_index;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static uint16_t packet_sequence_numbers[MAX_CHANNELS];
static uint8_t framed_pdus;
static bool cis_can_send[MAX_CHANNELS];
static bool cis_has_data[MAX_CHANNELS];
static bool cis_request[MAX_CHANNELS];
static bool cis_established[MAX_CHANNELS];
static uint8_t iso_frame_counter;
static uint16_t frame_duration_us;
static uint8_t num_cis;

// time stamping
#ifdef COUNT_MODE
#define MAX_PACKET_INTERVAL_BINS_MS 50
static uint32_t send_time_bins[MAX_PACKET_INTERVAL_BINS_MS];
static uint32_t send_last_ms;
#endif

// time based sender
#ifdef GENERATE_AUDIO_WITH_TIMER
static uint32_t next_send_time_ms;
static uint32_t next_send_time_additional_us;
static btstack_timer_source_t send_timer;
#endif

// lc3 codec config
static uint32_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_channels = 1;

// lc3 encoder
static const btstack_lc3_encoder_t * lc3_encoder;
static lc3_encoder_ehima_t encoder_contexts[MAX_CHANNELS];
static int16_t pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];
static uint32_t time_generation_ms;

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// mod player
static int hxcmod_initialized;
static modcontext mod_context;
static tracker_buffer_state trkbuf;
static int16_t mod_pcm[MAX_CHANNELS * MAX_SAMPLES_PER_FRAME];

// sine generator
static uint8_t  sine_step;
static uint16_t sine_phases[MAX_CHANNELS];

// audio producer
static enum {
    AUDIO_SOURCE_SINE,
    AUDIO_SOURCE_MODPLAYER
} audio_source = AUDIO_SOURCE_MODPLAYER;

static enum {
    APP_W4_WORKING,
    APP_SET_HOST_FEATURES,
    APP_IDLE,
    APP_W4_CIS_COMPLETE,
    APP_SET_ISO_PATH,
    APP_STREAMING
} app_state = APP_W4_WORKING;

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

static void show_usage(void);

static void print_config(void) {
    printf("Config '%s_%u': %u, %s ms, %u octets - %s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_channels,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           audio_source == AUDIO_SOURCE_SINE ? "Sine" : "Modplayer");
}

static void setup_lc3_encoder(void){
    uint8_t channel;
    for (channel = 0 ; channel < num_channels ; channel++){
        lc3_encoder_ehima_t * context = &encoder_contexts[channel];
        lc3_encoder = lc3_encoder_ehima_init_instance(context);
        lc3_encoder->configure(context, sampling_frequency_hz, frame_duration);
    }
    number_samples_per_frame = lc3_encoder->get_number_samples_per_frame(&encoder_contexts[0]);
    btstack_assert(number_samples_per_frame <= MAX_SAMPLES_PER_FRAME);
    printf("LC3 Encoder config: %u hz, frame duration %s ms, num samples %u, num octets %u\n",
           sampling_frequency_hz, frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           number_samples_per_frame, octets_per_frame);
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
    uint32_t start_ms = btstack_run_loop_get_time_ms();
    uint16_t sample;
    switch (audio_source) {
        case AUDIO_SOURCE_SINE:
            // generate sine wave for all channels
            for (sample = 0 ; sample < number_samples_per_frame ; sample++){
                uint8_t channel;
                for (channel = 0; channel < num_channels; channel++) {
                    int16_t value = sine_int16[sine_phases[channel]] / 4;
                    pcm[channel * MAX_SAMPLES_PER_FRAME + sample] = value;
                    sine_phases[channel] += sine_step * (1+channel);    // second channel, double frequency
                    if (sine_phases[channel] >= (sizeof(sine_int16) / sizeof(int16_t))) {
                        sine_phases[channel] = 0;
                    }
                }
            }
            break;
        case AUDIO_SOURCE_MODPLAYER:
            // mod player configured for stereo
            hxcmod_fillbuffer(&mod_context, (unsigned short *) &mod_pcm[0], number_samples_per_frame, &trkbuf);
            uint16_t i;
            if (num_channels == 1){
                // stereo -> mono
                for (i=0;i<number_samples_per_frame;i++){
                    pcm[i] = (mod_pcm[2*i] / 2) + (mod_pcm[2*i+1] / 2);
                }
            } else {
                // sort interleaved samples
                for (i=0;i<number_samples_per_frame;i++){
                    pcm[i] = mod_pcm[2*i];
                    pcm[MAX_SAMPLES_PER_FRAME+i] = mod_pcm[2*i+1];
                }
            }
            break;
        default:
            btstack_unreachable();
            break;
    }
    time_generation_ms = btstack_run_loop_get_time_ms() - start_ms;
    iso_frame_counter++;
}

static void encode_and_send(uint8_t cis_index){

#ifdef COUNT_MODE
    if (bis_index == 0) {
        uint32_t now = btstack_run_loop_get_time_ms();
        if (send_last_ms != 0) {
            uint16_t send_interval_ms = now - send_last_ms;
            if (send_interval_ms >= MAX_PACKET_INTERVAL_BINS_MS) {
                printf("ERROR: send interval %u\n", send_interval_ms);
            } else {
                send_time_bins[send_interval_ms]++;
            }
        }
        send_last_ms = now;
    }
#endif
    bool ok = hci_reserve_packet_buffer();
    btstack_assert(ok);
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, cis_con_handles[cis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + num_channels * octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[cis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, num_channels * octets_per_frame);
#ifdef COUNT_MODE
    // test data: bis_index, counter
    buffer[8] = bis_index;
    memset(&buffer[9], iso_frame_counter, num_channels * octets_per_frame - 1);
#else
    // encode as lc3
    uint8_t channel;
    uint16_t offset = 8;
    for (channel = 0; channel < num_channels; channel++){
        lc3_encoder->encode(&encoder_contexts[channel], &pcm[channel * MAX_SAMPLES_PER_FRAME], &buffer[offset], octets_per_frame);
        offset += octets_per_frame;
    }
#endif
    // send
    hci_send_iso_packet_buffer(4 + 0 + 4 + num_channels * octets_per_frame);

    if (((packet_sequence_numbers[cis_index] & 0x7f) == 0) && (cis_index == 0)) {
        printf("Encoding time: %u\n", time_generation_ms);
    }
    if ((packet_sequence_numbers[cis_index] & 0x7c) == 0){
        printf("%04x %10u %u ", packet_sequence_numbers[cis_index], btstack_run_loop_get_time_ms(), cis_index);
        printf_hexdump(&buffer[8], octets_per_frame);
    }

    packet_sequence_numbers[cis_index]++;
}

static void try_send(void){
    bool all_can_send = true;
    uint8_t i;
    for (i=0; i < num_cis; i++) {
        all_can_send &= cis_can_send[i];
    }
#ifdef PTS_MODE
   static uint8_t next_sender;
    // PTS 8.2 sends a packet after the previous one was received -> it sends at half speed for stereo configuration
    if (all_can_send) {
        if (next_sender == 0) {
            generate_audio();
        }
        cis_can_send[next_sender] = false;
        encode_and_send(next_sender);
        next_sender = (num_cis - 1) - next_sender;
    }
#else
#ifdef GENERATE_AUDIO_WITH_TIMER
    for (i=0;i<num_cis;i++){
        if (hci_is_packet_buffer_reserved()) return;
        if (cis_has_data[i]){
            cis_can_send[i] = false;
            cis_has_data[i] = false;
            encode_and_send(i);
            return;
        }
    }
#else
    // check if next audio frame should be produced and send
    if (all_can_send){
        generate_audio();
        for (i=0; i < num_cis; i++) {
            cis_has_data[i] = true;
        }
    }

    for (i=0; i < num_cis; i++){
        if (hci_is_packet_buffer_reserved()) return;
        if (cis_can_send[i] && cis_has_data[i]){
            cis_can_send[i] = false;
            cis_has_data[i] = false;
            encode_and_send(i);
            return;
        }
    }
#endif
#endif
}

#ifdef GENERATE_AUDIO_WITH_TIMER
static void generate_audio_timer_handler(btstack_timer_source_t *ts){

    generate_audio();

    uint8_t i;
    for (i=0; i<num_cis;i++) {
        cis_has_data[i] = true;
    }

    // next send time based on frame_duration_us
    next_send_time_additional_us += frame_duration_us % 1000;
    if (next_send_time_additional_us > 1000){
        next_send_time_ms++;
        next_send_time_additional_us -= 1000;
    }
    next_send_time_ms += frame_duration_us / 1000;

    uint32_t now = btstack_run_loop_get_time_ms();
    btstack_run_loop_set_timer(&send_timer, next_send_time_ms - now);
    btstack_run_loop_add_timer(&send_timer);

    try_send();
}
#endif

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;

    bd_addr_t event_addr;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_SET_HOST_FEATURES;
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            switch (hci_event_command_complete_get_command_opcode(packet)){
                case HCI_OPCODE_HCI_LE_CREATE_CIS:
                    app_state = APP_SET_ISO_PATH;
                    printf("Set ISO Paths\n");
                    break;
                case HCI_OPCODE_HCI_LE_SETUP_ISO_DATA_PATH:
                    next_cis_index++;
                    if (next_cis_index == num_cis){
                        printf("%u ISO path(s) set up\n", num_channels);
                        // ready to send
                        uint8_t i;
                        for (i=0; i < num_cis; i++) {
                            cis_can_send[i] = true;
                        }
                        app_state = APP_STREAMING;
                        //
#ifdef GENERATE_AUDIO_WITH_TIMER
                        btstack_run_loop_set_timer_handler(&send_timer, &generate_audio_timer_handler);
                        uint32_t next_send_time_ms = btstack_run_loop_get_time_ms() + 10;
                        uint32_t now = btstack_run_loop_get_time_ms();
                        btstack_run_loop_set_timer(&send_timer, next_send_time_ms - now);
                        btstack_run_loop_add_timer(&send_timer);
#endif
                    }
                    break;
            }
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    next_cis_index = 0;
                    break;
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    cis_con_handles[next_cis_index] = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    cis_request[next_cis_index] = true;
                    next_cis_index++;
                    break;
                case HCI_SUBEVENT_LE_CIS_ESTABLISHED:
                {
                    // only look for cis handle
                    uint8_t i;
                    hci_con_handle_t cis_handle = hci_subevent_le_cis_established_get_connection_handle(packet);
                    for (i=0; i < num_cis; i++){
                        if (cis_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    if (complete) {
                        printf("All CIS Established\n");
                        next_cis_index = 0;
                        app_state = APP_SET_ISO_PATH;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            if (size >= 3){
                uint16_t num_handles = packet[2];
                if (size != (3u + num_handles * 4u)) break;
                uint16_t offset = 3;
                uint16_t i;
                for (i=0; i<num_handles;i++) {
                    hci_con_handle_t handle = little_endian_read_16(packet, offset) & 0x0fffu;
                    offset += 2u;
                    uint16_t num_packets = little_endian_read_16(packet, offset);
                    offset += 2u;
                    uint8_t j;
                    for (j=0 ; j < num_channels ; j++){
                        if (handle == cis_con_handles[j]){
                            // allow to send
                            cis_can_send[j] = true;
                        }
                    }
                }
            }
            break;
        default:
            break;
    }

    if (hci_can_send_command_packet_now()) {
        switch(app_state){
            case APP_SET_HOST_FEATURES:
                hci_send_cmd(&hci_le_set_host_feature, 32, 1);
                app_state = APP_IDLE;
                show_usage();
                printf("Please select sample frequency and variation, then start advertising\n");
                break;
            case APP_W4_CIS_COMPLETE:
            {
                uint8_t i;
                for (i=0; i < num_cis; i++){
                    if (cis_request[i]){
                        cis_request[i] = false;
                        printf("Accept CIS Request for conn handle %04x\n", cis_con_handles[i]);
                        hci_send_cmd(&hci_le_accept_cis_request, cis_con_handles[i]);
                        break;
                    }
                }
                break;
            }
            case APP_SET_ISO_PATH:
                hci_send_cmd(&hci_le_setup_iso_data_path, cis_con_handles[next_cis_index], 0, 0,  0, 0, 0,  0, 0, NULL);
                break;
            default:
                break;
        }
    }

    try_send();
}

static void show_usage(void){
    printf("\n--- LE Audio Unicast Source Test Console ---\n");
    print_config();
    printf("---\n");
    printf("c - toggle channels\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("t - toggle sine / modplayer\n");
    printf("s - start advertising\n");
    printf("x - shutdown\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 'c':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            num_channels = 3 - num_channels;
            print_config();
            break;
        case 'f':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
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
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            menu_variant++;
            if (menu_variant >= codec_configurations[menu_sampling_frequency].num_variants){
                menu_variant = 0;
            }
            print_config();
            break;
        case 'x':
#ifdef COUNT_MODE
            printf("Send statistic:\n");
            {
                uint16_t i;
                for (i=0;i<MAX_PACKET_INTERVAL_BINS_MS;i++){
                    printf("%2u: %5u\n", i, send_time_bins[i]);
                }
            }
#endif
            printf("Shutdown...\n");
            hci_power_control(HCI_POWER_OFF);
            break;
        case 's':
            if (app_state != APP_IDLE){
                printf("Cannot start advertising - not in idle state\n");
                break;
            }
            // use values from table
            sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
            octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
            frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

            // get num samples per frame
            setup_lc3_encoder();

            // setup mod player
            setup_mod_player();

            // setup sine generator
            if (sampling_frequency_hz == 44100){
                sine_step = 2;
            } else {
                sine_step = 96000 / sampling_frequency_hz;
            }

            // update adv / BASE
            adv_data[4] = 0; // subtype
            adv_data[5] = 0; // flags
            adv_data[6] = num_channels;
            adv_data[7] = sampling_frequency_hz / 1000;
            adv_data[8] = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 0 : 1;
            adv_data[9] = octets_per_frame;

            // setup advertisements
            uint16_t adv_int_min = 0x0030;
            uint16_t adv_int_max = 0x0030;
            uint8_t adv_type = 0;
            bd_addr_t null_addr;
            memset(null_addr, 0, 6);
            gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
            gap_advertisements_set_data(sizeof(adv_data), adv_data);
            gap_advertisements_enable(1);
            num_cis = 1;
            app_state = APP_W4_CIS_COMPLETE;
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
    
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
