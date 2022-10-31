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

#define BTSTACK_FILE__ "le_audio_broadcast_source.c"

/*
 * LE Audio Broadcast Source
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
#include "le-audio/le_audio_base_builder.h"

#include "hxcmod.h"
#include "mods/mod.h"

// PTS mode
// #define PTS_MODE

// Count mode - send packet count as test data for manual analysis
// #define COUNT_MODE

// max config
#define MAX_NUM_BIS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

static const uint8_t adv_sid = 0;

static le_advertising_set_t le_advertising_set;

static const le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 0,
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 0,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

static const uint8_t extended_adv_data[] = {
        // 16 bit service data, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE, Broadcast ID
        6, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x52, 0x18,
        BROADCAST_ID >> 16,
        (BROADCAST_ID >> 8) & 0xff,
        BROADCAST_ID & 0xff,
        // name
#ifdef PTS_MODE
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P', 'T', 'S', '-', 'x', 'x'
#elif defined(COUNT_MODE)
        6, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'C', 'O', 'U', 'N', 'T'
#else
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e'
#endif
};

static const le_periodic_advertising_parameters_t periodic_params = {
        .periodic_advertising_interval_min = 0x258, // 375 ms
        .periodic_advertising_interval_max = 0x258, // 375 ms
        .periodic_advertising_properties = 0
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
static const char * remote_addr_string = "00:1B:DC:08:E2:72";

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t period_adv_data[255];
static uint16_t period_adv_data_len;

static uint8_t adv_handle = 0;
static unsigned int     next_bis_index;
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint16_t packet_sequence_numbers[MAX_NUM_BIS];
static uint8_t framed_pdus;
static bool bis_can_send[MAX_NUM_BIS];
static bool bis_has_data[MAX_NUM_BIS];
static uint8_t iso_frame_counter;
static uint16_t frame_duration_us;

static le_audio_big_t big_storage;
static le_audio_big_params_t big_params;

// time stamping
#ifdef COUNT_MODE
#define MAX_PACKET_INTERVAL_BINS_MS 50
static uint32_t send_time_bins[MAX_PACKET_INTERVAL_BINS_MS];
static uint32_t send_last_ms;
#endif

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;
static uint8_t  num_bis = 1;

// lc3 encoder
static const btstack_lc3_encoder_t * lc3_encoder;
static btstack_lc3_encoder_google_t encoder_contexts[MAX_NUM_BIS];
static int16_t pcm[MAX_NUM_BIS * MAX_SAMPLES_PER_FRAME];
static uint8_t iso_payload[MAX_NUM_BIS * MAX_LC3_FRAME_BYTES];
static uint32_t time_generation_ms;

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

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };

// audio producer
static enum {
    AUDIO_SOURCE_SINE,
    AUDIO_SOURCE_MODPLAYER
} audio_source = AUDIO_SOURCE_MODPLAYER;

static enum {
    APP_IDLE,
    APP_W4_CREATE_BIG_COMPLETE,
    APP_STREAMING,
} app_state = APP_IDLE;

// enumerate default codec configs
static struct {
    uint16_t samplingrate_hz;
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
    printf("Config '%s_%u': %u, %s ms, %u octets - %s%s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_bis,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           audio_source == AUDIO_SOURCE_SINE ? "Sine" : "Modplayer", encryption ? " (encrypted)" : "");
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
    time_generation_ms = btstack_run_loop_get_time_ms() - start_ms;
    iso_frame_counter++;
}

static void encode(uint8_t bis_index){
    // encode as lc3
    lc3_encoder->encode_signed_16(&encoder_contexts[bis_index], &pcm[bis_index], num_bis, &iso_payload[bis_index * MAX_LC3_FRAME_BYTES]);
}


static void send_iso_packet(uint8_t bis_index) {

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
    little_endian_store_16(buffer, 0, bis_con_handles[bis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + octets_per_frame);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[bis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, octets_per_frame);
#ifdef COUNT_MODE
    // test data: bis_index, counter
    buffer[8] = bis_index;
    memset(&buffer[9], iso_frame_counter, octets_per_frame - 1);
#else
    // copy encoded payload
    memcpy(&buffer[8], &iso_payload[bis_index * MAX_LC3_FRAME_BYTES], octets_per_frame);
#endif
    // send
    hci_send_iso_packet_buffer(4 + 0 + 4 + octets_per_frame);

#ifdef HAVE_POSIX_FILE_IO
    if (((packet_sequence_numbers[bis_index] & 0x7f) == 0) && (bis_index == 0)) {
        printf("Encoding time: %u\n", time_generation_ms);
    }
#endif

    packet_sequence_numbers[bis_index]++;
}

static void generate_audio_and_encode(void){
    uint8_t i;
    generate_audio();
    for (i = 0; i < num_bis; i++) {
        encode(i);
        bis_has_data[i] = true;
    }
}

static void setup_advertising() {
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(extended_adv_data), extended_adv_data);
    gap_periodic_advertising_set_params(adv_handle, &periodic_params);
    gap_periodic_advertising_set_data(adv_handle, period_adv_data_len, period_adv_data);
    gap_periodic_advertising_start(adv_handle, 0);
    gap_extended_advertising_start(adv_handle, 0, 0);
}

static void setup_big(void){
    // Create BIG
    big_params.big_handle = 0;
    big_params.advertising_handle = adv_handle;
    big_params.num_bis = num_bis;
    big_params.max_sdu = octets_per_frame;
    big_params.max_transport_latency_ms = 31;
    big_params.rtn = 2;
    big_params.phy = 2;
    big_params.packing = 0;
    big_params.encryption = encryption;
    if (encryption) {
        memcpy(big_params.broadcast_code, &broadcast_code[0], 16);
    } else {
        memset(big_params.broadcast_code, 0, 16);
    }
    if (sampling_frequency_hz == 44100){
        // same config as for 48k -> frame is longer by 48/44.1
        big_params.sdu_interval_us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 8163 : 10884;
        big_params.framing = 1;
    } else {
        big_params.sdu_interval_us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 7500 : 10000;
        big_params.framing = 0;
    }
    app_state = APP_W4_CREATE_BIG_COMPLETE;
    gap_big_create(&big_storage, &big_params);
}


static void start_broadcast() {// use values from table
    sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
    octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
    frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

    // get num samples per frame
    setup_lc3_encoder();

    // setup base
    uint8_t codec_id[] = { 0x06, 0x00, 0x00, 0x00, 0x00 };
    uint8_t subgroup_codec_specific_configuration[] = {
            0x02, 0x01, 0x01,
            0x02, 0x02, 0x01,
            0x03, 0x04, 0x1E, 0x00,
    };
    subgroup_codec_specific_configuration[2] = codec_configurations[menu_sampling_frequency].samplingrate_index;
    subgroup_codec_specific_configuration[5] =  (frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US) ? 0 : 1;;
    uint8_t subgroup_metadata[] = {
            0x03, 0x02, 0x04, 0x00, // Metadata[i]
    };
    little_endian_store_16(subgroup_codec_specific_configuration, 8, octets_per_frame);
    uint8_t bis_codec_specific_configuration_1[] = {
            0x05, 0x03, 0x01, 0x00, 0x00, 0x00
    };
    uint8_t bis_codec_specific_configuration_2[] = {
            0x05, 0x03, 0x02, 0x00, 0x00, 0x00
    };
    le_audio_base_builder_t builder;
    le_audio_base_builder_init(&builder, period_adv_data, sizeof(period_adv_data), 40);
    le_audio_base_builder_add_subgroup(&builder, codec_id,
                                       sizeof(subgroup_codec_specific_configuration),
                                       subgroup_codec_specific_configuration,
                                       sizeof(subgroup_metadata), subgroup_metadata);
    le_audio_base_builder_add_bis(&builder, 1, sizeof(bis_codec_specific_configuration_1),
                                  bis_codec_specific_configuration_1);
    if (num_bis == 2){
        le_audio_base_builder_add_bis(&builder, 2, sizeof(bis_codec_specific_configuration_2),
                                      bis_codec_specific_configuration_2);
    }
    period_adv_data_len = le_audio_base_builder_get_ad_data_size(&builder);

    // setup mod player
    setup_mod_player();

    // setup sine generator
    if (sampling_frequency_hz == 44100){
        sine_step = 2;
    } else {
        sine_step = 96000 / sampling_frequency_hz;
    }

    // setup extended and periodic advertising
    setup_advertising();

    // setup big
    setup_big();
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t bis_index;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
#ifdef ENABLE_DEMO_MODE
                    // start broadcast automatically, mod player, 48_5_1
                    num_bis = 1;
                    menu_sampling_frequency = 5;
                    menu_variant = 4;
                    start_broadcast();
#else
                    show_usage();
                    printf("Please select sample frequency and variation, then start broadcast\n");
#endif
                    break;
                case HCI_STATE_OFF:
                    printf("Goodbye\n");
                    exit(0);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_BIG_CREATED:
                    printf("BIG Created with BIS Connection handles: \n");
                    for (bis_index=0;bis_index<num_bis;bis_index++){
                        bis_con_handles[bis_index] = gap_subevent_big_created_get_bis_con_handles(packet, bis_index);
                        printf("0x%04x ", bis_con_handles[bis_index]);
                    }

                    app_state = APP_STREAMING;
                    printf("Start streaming\n");
                    generate_audio_and_encode();
                    hci_request_bis_can_send_now_events(big_params.big_handle);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_BIS_CAN_SEND_NOW:
            bis_index = hci_event_bis_can_send_now_get_bis_index(packet);
            send_iso_packet(bis_index);
            bis_index++;
            if (bis_index == num_bis){
                generate_audio_and_encode();
                hci_request_bis_can_send_now_events(big_params.big_handle);
            }
            break;
        default:
            break;
    }
}

static void show_usage(void){
    printf("\n--- LE Audio Broadcast Source Test Console ---\n");
    print_config();
    printf("---\n");
    printf("c - toggle channels\n");
    printf("e - toggle encryption\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("t - toggle sine / modplayer\n");
    printf("s - start broadcast\n");
    printf("---\n");
}
static void stdin_process(char c){
    switch (c){
        case 'c':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            num_bis = 3 - num_bis;
            print_config();
            break;
        case 'e':
            if (app_state != APP_IDLE){
                printf("Encryption can only be changed in idle state\n");
                break;
            }
            encryption = 1 - encryption;
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
        case 's':
            if (app_state != APP_IDLE){
                printf("Cannot start broadcast - not in idle state\n");
                break;
            }
            start_broadcast();
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
