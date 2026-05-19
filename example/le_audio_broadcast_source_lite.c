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

#define BTSTACK_FILE__ "le_audio_broadcast_source_lite.c"

/*
 * LE Audio Broadcast Source
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <btstack_debug.h>
#include <inttypes.h>
#include <string.h>

#include "audio_player_demo_util.h"
#include "bluetooth_company_id.h"
#include "bluetooth_data_types.h"
#include "btstack_audio_player.h"
#include "btstack_stdin.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"

#include "mods/mod.h"

#include "le_audio_demo_util_source.h"

// max config
#define MAX_NUM_BIS 4
#define MAX_SAMPLES_PER_FRAME 480
#define MAX_LC3_FRAME_BYTES   155

#define ADVERTISING_SID  0

typedef enum {
    BROADCAST_SOURCE_PLAYER = 0,
    BROADCAST_SOURCE_COUNTER,
    BROADCAST_SOURCE_LOOPBACK,
} broadcast_source_t;
static broadcast_source_t broadcast_source = BROADCAST_SOURCE_PLAYER;

static btstack_audio_player_t audio_player[MAX_NUM_BIS];;

static int num_iso_generators;
static le_audio_demo_source_generator_t iso_gen[MAX_NUM_BIS];

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
        .advertising_sid = ADVERTISING_SID,
        .scan_request_notification_enable = 0,
};

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

enum {
    BROADCAST_LITE_ADV_FIELD_LEN = 9,
    BROADCAST_LITE_ADV_OFFSET_SUBTYPE = 11,
    BROADCAST_LITE_ADV_OFFSET_FLAGS,
    BROADCAST_LITE_ADV_OFFSET_NUM_BIS,
    BROADCAST_LITE_ADV_OFFSET_SAMPLING_FREQUENCY,
    BROADCAST_LITE_ADV_OFFSET_FRAME_DURATION,
    BROADCAST_LITE_ADV_OFFSET_OCTETS_PER_FRAME,
};

static uint8_t extended_adv_data[] = {
        // 16 bit service data, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE, Broadcast ID
        6, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x52, 0x18,
        BROADCAST_ID >> 16,
        (BROADCAST_ID >> 8) & 0xff,
        BROADCAST_ID & 0xff,
        // Manufacturer Specific Data to indicate codec
        BROADCAST_LITE_ADV_FIELD_LEN,
        BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH & 0xff,
        BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH >> 8,
        1, // subtype: LE Audio Broadcast Source Lite
        0, // flags
        1, // num bis
        8, // LC3 sampling frequency index
        0, // frame duration
        26, // octets per frame
        // name
         7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'o', 'u', 'r', 'c', 'e',
         7, BLUETOOTH_DATA_TYPE_BROADCAST_NAME,      'S', 'o', 'u', 'r', 'c', 'e',
};

static const le_periodic_advertising_parameters_t periodic_params = {
        .periodic_advertising_interval_min = 0x258, // 375 ms
        .periodic_advertising_interval_max = 0x258, // 375 ms
        .periodic_advertising_properties = 0
};

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t adv_handle = 0;
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static bool bis_read_tx_time[MAX_NUM_BIS];

static le_audio_big_t big_storage;
static le_audio_big_params_t big_params;
static bd_addr_t local_addr;
static uint8_t local_addr_type;

// time stamping
#ifdef COUNT_MODE
#define MAX_PACKET_INTERVAL_BINS_MS 50
static uint32_t send_time_bins[MAX_PACKET_INTERVAL_BINS_MS];
static uint32_t send_last_ms;
#endif

static uint16_t counter_packet_sequence_number;

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t number_samples_per_frame;
static uint16_t octets_per_frame;

static uint8_t num_stereo_streams = 0;
static uint8_t num_mono_streams   = 1;
static uint8_t num_bis            = 1;

// codec menu
static uint8_t menu_sampling_frequency;
static uint8_t menu_variant;

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };

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

static uint32_t measurement_disconnect_time_ms = 15000;
static btstack_timer_source_t measurement_disconnect_timer;
static btstack_timer_source_t loopback_single_packet_timer;
static void show_usage(void);

static void print_config(void) {
    printf("Stereo streams: %2d Mono streams: %2d\n", num_stereo_streams, num_mono_streams);
    static const char * generator[] = { "Audio Player", "Counter Mode (test)", "Loopback (test)"};
    printf("Config '%s_%u': %u, %s ms, %u octets - %s%s\n",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].name,
           num_bis,
           codec_configurations[menu_sampling_frequency].samplingrate_hz,
           codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? "7.5" : "10",
           codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame,
           generator[broadcast_source],
           encryption ? " (encrypted)" : "");
}

static void setup_advertising() {
    gap_le_get_own_address(&local_addr_type, local_addr);

    extended_params.own_address_type = local_addr_type;
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    if( local_addr_type == BD_ADDR_TYPE_LE_RANDOM ) {
        gap_extended_advertising_set_random_address( adv_handle, local_addr );
    }
    gap_extended_advertising_set_adv_data(adv_handle, sizeof(extended_adv_data), extended_adv_data);
    gap_periodic_advertising_set_params(adv_handle, &periodic_params);
    gap_periodic_advertising_set_data(adv_handle, 0, NULL);
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
    big_params.rtn = 1;
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

static void stop_broadcast(void) {
    printf("Terminate BIG\n");
    gap_big_terminate(big_params.big_handle);
}

static void measurement_disconnect_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    stop_broadcast();
}

static void update_demo_util_source(int i) {
    le_audio_demo_source_generator_t *gen = &iso_gen[i];

    switch (broadcast_source) {
        case BROADCAST_SOURCE_PLAYER:
            le_audio_demo_util_source_set_audio_generator(gen, btstack_audio_player_get_generator(&audio_player[i]));
            break;
        case BROADCAST_SOURCE_COUNTER:
            printf("BROADCAST_SOURCE_COUNTER not supported yet\n");
            btstack_unreachable();
            break;
        case BROADCAST_SOURCE_LOOPBACK:
            printf("BROADCAST_SOURCE_LOOPBACK not supported yet\n");
            btstack_unreachable();
            break;
        default:
            btstack_unreachable();
            break;
    };
    // prepare first frame
    le_audio_demo_util_source_generate_iso_frame(gen);
}

static void start_broadcast() {
    // use values from table
    sampling_frequency_hz = codec_configurations[menu_sampling_frequency].samplingrate_hz;
    octets_per_frame      = codec_configurations[menu_sampling_frequency].variants[menu_variant].octets_per_frame;
    frame_duration        = codec_configurations[menu_sampling_frequency].variants[menu_variant].frame_duration;

    extended_adv_data[BROADCAST_LITE_ADV_OFFSET_SUBTYPE] = 1; // LE Audio Broadcast Source Lite
    extended_adv_data[BROADCAST_LITE_ADV_OFFSET_FLAGS] = 0;
    extended_adv_data[BROADCAST_LITE_ADV_OFFSET_NUM_BIS] = num_bis;
    extended_adv_data[BROADCAST_LITE_ADV_OFFSET_SAMPLING_FREQUENCY] = codec_configurations[menu_sampling_frequency].samplingrate_index;
    extended_adv_data[BROADCAST_LITE_ADV_OFFSET_FRAME_DURATION] = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 0 : 1;
    extended_adv_data[BROADCAST_LITE_ADV_OFFSET_OCTETS_PER_FRAME] = octets_per_frame;

    number_samples_per_frame = btstack_lc3_samples_per_frame(sampling_frequency_hz, frame_duration);

    // generate playlist for audio player
    btstack_linked_list_t playlist = audio_player_demo_util_get_playlist_instance();

    num_iso_generators = 0;
    for( int i=0, bis_count=num_bis, stereo_stream=num_stereo_streams; bis_count>0; ++i ) {
        uint8_t channels = (stereo_stream>0)?2:1;
        printf("gen[%d]( %d ) ", i, channels);
        btstack_audio_player_init(&audio_player[i], sampling_frequency_hz, channels, playlist);
        btstack_audio_player_play(&audio_player[i]);

        le_audio_demo_source_generator_t *gen = &iso_gen[i];
        le_audio_demo_util_source_configure(gen, channels, 1, sampling_frequency_hz, frame_duration, octets_per_frame);
        num_iso_generators++;

        printf(" %" PRIu32 "Hz\n", gen->sampling_frequency_hz);

        // configure and prepare first iso frame
        update_demo_util_source(i);

        if( stereo_stream > 0 ) {
            --stereo_stream;
        }
        bis_count -= channels;
    }
    printf("Song: %s\n", btstack_audio_player_get_current_song(&audio_player[0])->title);


    // setup extended and periodic advertising
    setup_advertising();

    // setup big
    setup_big();

    // set timer for counter mode
    switch (broadcast_source) {
        case BROADCAST_SOURCE_COUNTER:
        case BROADCAST_SOURCE_LOOPBACK:
            btstack_run_loop_set_timer_handler(&measurement_disconnect_timer, measurement_disconnect_handler);
            btstack_run_loop_set_timer(&measurement_disconnect_timer, measurement_disconnect_time_ms);
            printf("Terminate BIG after %" PRIu32 " s\n", measurement_disconnect_time_ms / 1000);
            btstack_run_loop_add_timer(&measurement_disconnect_timer);
            break;
        default:
            break;
    }
}

static void loopback_source_count_send( hci_con_handle_t con_handle ) {
    hci_reserve_packet_buffer();

    uint16_t frame_len = octets_per_frame;

    //    printf("%s\n", __func__);
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, ((uint16_t) con_handle) | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + frame_len);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, counter_packet_sequence_number);
    // iso sdu len
    little_endian_store_16(buffer, 6, frame_len);
    uint16_t offset = 8;
    // copy encoded payload
    memset( &buffer[offset], (uint8_t) counter_packet_sequence_number, frame_len );
    offset += frame_len;
    counter_packet_sequence_number += 1;
    //    printf("iso_packet_size: %d\n", offset );
    // send
    hci_send_iso_packet_buffer(offset);
}

static void looback_single_packet_handler(btstack_timer_source_t * ts) {
    UNUSED(ts);
    hci_request_bis_can_send_now_events(big_params.big_handle);
}

static void handle_bis_can_send_now_event(const uint8_t * packet, uint16_t size) {
    UNUSED(size);
    uint8_t bis_index = hci_event_bis_can_send_now_get_bis_index(packet);
    le_audio_demo_source_generator_t *gen = NULL;
    int channels = 0;
    uint8_t stream_index = 0;
    for(int i=0;i<num_bis;++i) {
        gen = &iso_gen[i];
        // calculate stream index relative to generator
        stream_index = bis_index - channels;
        channels += gen->num_channels;
        if( bis_index < channels ) {
            break;
        }
    }
    switch (broadcast_source) {
        case BROADCAST_SOURCE_LOOPBACK:
            loopback_source_count_send(bis_con_handles[bis_index]);
            break;
        default:
            le_audio_demo_util_source_send(gen, stream_index, bis_con_handles[bis_index]);
            break;
    }
    if ((bis_index+1) == channels ){
        // HCI LE Read TX ISO Sync does not work on NCS 2.7 or 2.7
        // bis_read_tx_time[0] = true;
        // we use timer to send individual packets
		if (broadcast_source == BROADCAST_SOURCE_LOOPBACK) {
            btstack_run_loop_set_timer_handler(&loopback_single_packet_timer, &looback_single_packet_handler);
            btstack_run_loop_set_timer(&loopback_single_packet_timer, 50);
            btstack_run_loop_add_timer(&loopback_single_packet_timer);
        } else {
            le_audio_demo_util_source_generate_iso_frame(gen);
            hci_request_bis_can_send_now_events(big_params.big_handle);
        }
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t bis_index;
    uint8_t i;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    show_usage();
                    printf("Please select sample frequency and variation, then start broadcast\n");
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
            if ((hci_event_command_complete_get_command_opcode(packet)) == HCI_OPCODE_HCI_LE_READ_ISO_TX_SYNC) {
                const uint8_t * return_params = hci_event_command_complete_get_return_parameters(packet);
                hci_con_handle_t handle = little_endian_read_16(return_params, 1);
                uint16_t packet_sequence_number = little_endian_read_16(return_params, 3);
                // we sent READ ISO TX after sending a packet. We expect it to return the packet sequence number
                // of the previously sent packet, so we'll use this number + 1 as the new outgoing sequence number
                for (i=0;i<num_bis;i++) {
                    if (handle == bis_con_handles[i]) {
                        log_info("our seq %u, tx seq %u", counter_packet_sequence_number, packet_sequence_number);
                        counter_packet_sequence_number = packet_sequence_number + 1;
                    }
                }
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
                case GAP_SUBEVENT_BIG_TERMINATED:
                    printf("BIG TERMINATED\n");

                    switch (broadcast_source) {
                        case BROADCAST_SOURCE_COUNTER:
                        case BROADCAST_SOURCE_LOOPBACK:
                            // shut down
                            printf("EXIT\n");
                            btstack_run_loop_trigger_exit();
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_BIS_CAN_SEND_NOW:
            handle_bis_can_send_now_event(packet, size);
            break;
        default:
            break;
    }

    // Read TX ISO Time
    for (i=0;i<num_bis;i++) {
        if (bis_read_tx_time[i]) {
            if (hci_can_send_command_packet_now()) {
                bis_read_tx_time[i] = false;
                hci_send_cmd(&hci_le_read_iso_tx_sync, bis_con_handles[i]);
            }
        }
    }
}

static void show_usage(void){
    printf("\n--- LE Audio Broadcast Source Test Console ---\n");
    print_config();
    printf("---\n");
    printf("m - add mono subgroup\n");
    printf("r - add stereo subgroup (reset mono subgroups)\n");
    printf("e - toggle encryption\n");
    printf("f - next sampling frequency\n");
    printf("v - next codec variant\n");
    printf("x - next song\n");
    printf("s - start broadcast\n");
    printf("q - stop broadcast\n");
    printf("---\n");
}

static void update_num_bis(void) {
    num_bis = num_mono_streams + 2 * num_stereo_streams;
    if (num_bis > MAX_NUM_BIS) {
        num_mono_streams = 0;
        num_stereo_streams = 0;
        num_bis = 0;
    }
}

static void stdin_process(char c){
    switch (c){
        case 'm':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            num_mono_streams++;
            update_num_bis();
            print_config();
            break;
        case 'r':
            if (app_state != APP_IDLE){
                printf("Codec configuration can only be changed in idle state\n");
                break;
            }
            if (num_mono_streams) {
                printf("Reset Mono subgroups\n");
                num_mono_streams = 0;
            }
            num_stereo_streams++;
            update_num_bis();
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
            for (int i=0;i<num_iso_generators;i++) {
                btstack_audio_player_next_song(&audio_player[i]);
            }
            printf("Next song: %s\n", btstack_audio_player_get_current_song(&audio_player[0])->title);
            break;
        case 'q':
            stop_broadcast();
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

int btstack_main(int argc, char * argv[]);
int btstack_main(int argc, char * argv[]){
    (void) argc;
    (void) argv;

    // default config: 48_4_2
    menu_sampling_frequency = 5;
    menu_variant = 3;
    num_bis = 2;
    num_stereo_streams = 1;
    num_mono_streams = 0;

    print_config();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for BIS Can Send Now events
    hci_register_iso_packet_handler(&packet_handler);

    // setup audio processing
    for( int i=0; i<MAX_NUM_BIS; ++i) {
        le_audio_demo_source_generator_t *gen = &iso_gen[i];
        le_audio_demo_util_source_init(gen);
    }

    // queue upt to 2 iso packets
    hci_set_num_iso_packets_to_queue(2);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
