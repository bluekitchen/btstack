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
#include "le_audio_demo_util_source.h"

// Interoperability with Nordic LE Audio demo
//#define NRF5340_BROADCAST_MODE

// max config
#define MAX_NUM_BIS 2
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

static const uint8_t adv_sid = 0;

static le_advertising_set_t le_advertising_set;

static le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 0,
        .primary_advertising_interval_min = 0x4b0, // 750 ms
        .primary_advertising_interval_max = 0x4b0, // 750 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = BD_ADDR_TYPE_LE_PUBLIC,
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
#if defined(NRF5340_BROADCAST_MODE)
        20, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'N','R','F','5','3','4','0','_','B','R','O','A','D','C','A','S','T','E','R',
        20, BLUETOOTH_DATA_TYPE_BROADCAST_NAME,      'N','R','F','5','3','4','0','_','B','R','O','A','D','C','A','S','T','E','R',
#else
         7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e',
         7, BLUETOOTH_DATA_TYPE_BROADCAST_NAME,      'S', 'o', 'u', 'r', 'c', 'e',
#endif
};

static const le_periodic_advertising_parameters_t periodic_params = {
        .periodic_advertising_interval_min = 0x258, // 375 ms
        .periodic_advertising_interval_max = 0x258, // 375 ms
        .periodic_advertising_properties = 0
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t period_adv_data[255];
static uint16_t period_adv_data_len;

static uint8_t adv_handle = 0;
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];

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

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };

// audio producer
#ifdef COUNT_MODE
static le_audio_demo_source_generator audio_source = AUDIO_SOURCE_COUNTER;
#else
static le_audio_demo_source_generator audio_source = AUDIO_SOURCE_MODPLAYER;
#endif

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
    static const char * generator[] = { "Sine", "Modplayer", "Recording"};
    printf("Config '%s_%u': %u, %s ms, %u octets - %s%s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_bis,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           generator[audio_source - AUDIO_SOURCE_SINE],
           encryption ? " (encrypted)" : "");
}

static void setup_advertising() {
    bd_addr_t local_addr;
    gap_local_bd_addr(local_addr);
    bool local_address_invalid = btstack_is_null_bd_addr( local_addr );
    if( local_address_invalid ) {
        extended_params.own_address_type = BD_ADDR_TYPE_LE_RANDOM;
    }
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    if( local_address_invalid ) {
        bd_addr_t random_address = { 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01 };
        gap_extended_advertising_set_random_address( adv_handle, random_address );
    }
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

    number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);

    le_audio_demo_util_source_configure(num_bis, 1, sampling_frequency_hz, frame_duration, octets_per_frame);
    le_audio_demo_util_source_generate_iso_frame(audio_source);

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
    le_audio_base_builder_init(&builder, period_adv_data, sizeof(period_adv_data), 20000);
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
#elif defined( NRF5340_BROADCAST_MODE )
                    num_bis = 1;
                    menu_sampling_frequency = 5;
                    menu_variant = 1;
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
                    hci_request_bis_can_send_now_events(big_params.big_handle);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_BIS_CAN_SEND_NOW:
            bis_index = hci_event_bis_can_send_now_get_bis_index(packet);
            le_audio_demo_util_source_send(bis_index, bis_con_handles[bis_index]);
            bis_index++;
            if (bis_index == num_bis){
                le_audio_demo_util_source_generate_iso_frame(audio_source);
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
    printf("x - toggle sine / modplayer / recording\n");
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
        case 'x':
            switch (audio_source){
                case AUDIO_SOURCE_MODPLAYER:
                    audio_source = AUDIO_SOURCE_SINE;
                    break;
                case AUDIO_SOURCE_SINE:
                    audio_source = AUDIO_SOURCE_RECORDING;
                    break;
                case AUDIO_SOURCE_RECORDING:
                    audio_source = AUDIO_SOURCE_MODPLAYER;
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
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

    // setup audio processing
    le_audio_demo_util_source_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
