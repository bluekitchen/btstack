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

#define BTSTACK_FILE__ "le_audio_unicast_headset.c"

/*
 * LE Audio Unicast Sink
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ad_parser.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_audio.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_ring_buffer.h"
#include "btstack_stdin.h"
#include "btstack_util.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "btstack_lc3.h"
#include "btstack_lc3_google.h"

#include "le_audio_demo_util_sink.h"
#include "le_audio_demo_util_source.h"

#include "le_audio_unicast_headset.h"

#include "le-audio/gatt-service/audio_stream_control_service_server.h"
#include "ble/att_server.h"
#include "ble/sm.h"
#include "l2cap.h"
#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_server.h"
#include "le-audio/gatt-service/volume_control_service_server.h"
#include "le-audio/gatt-service/media_control_service_client.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_server.h"

#define ENABLE_MCS_CLIENT
#define ENABLE_MICROPHONE
#define ENABLE_CSIS_SERVER

//#define DEBUG_PLC
#ifdef DEBUG_PLC
#define printf_plc(...) printf(__VA_ARGS__)
#else
#define printf_plc(...)  (void)(0);
#endif

// max config
#define MAX_CHANNELS 2
#define MAX_SAMPLES_PER_FRAME 480

// playback
#define MAX_NUM_LC3_FRAMES   15
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_BYTES_PER_SAMPLE)

// analysis
#define PACKET_PREFIX_LEN 10

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static void show_usage(void);

static const char * channel_names[] =  { "Stereo", "Mono Front Left", "Mono Front Right", "Mono Front Center", "Mono Back Left", "Mono Back Right" };
static uint8_t app_config;

static enum {
    APP_W4_WORKING,
    APP_W4_CIG_COMPLETE,
    APP_W4_CIS_CREATED,
    APP_STREAMING,
    APP_IDLE,
} app_state = APP_W4_WORKING;

#define APP_AD_FLAGS 0x06
static uint8_t adv_data[] = {
    // offset 0 - Flags general discoverable
    2, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
#ifdef ENABLE_CSIS_SERVER
    // offset 3 - RSI
    7, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0, 0, 0, 0, 0, 0,
#endif
    // Service Class List (ASCS)
    3, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE & 0xff, ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE >> 8,
    // Service Data (ASCS)
    9, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE & 0xff, ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE >> 8,  0x00, 0xff, 0x0f, 0x03, 0x00, 0x00,
    // Service Data (CAP)
    4, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, ORG_BLUETOOTH_SERVICE_COMMON_AUDIO_SERVICE & 0xff, ORG_BLUETOOTH_SERVICE_COMMON_AUDIO_SERVICE >> 8,  0x00,
    // name
    8, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'H', 'e', 'a', 'd', 's', 'e', 't'
};

static pacs_record_t sink_pac_records[] = {
    // sink_record_0
    {
        // codec ID
        {HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000},
        // capabilities
        {
            0x3E,
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_8000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_24000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_32000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ,
            LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
            LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2,
            26,
            155,
            2
        },
        // metadata length
        45,
        // metadata
        {
            // all metadata set
            0x0FFE,
            // (2) preferred_audio_contexts_mask
            LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
            // (2) streaming_audio_contexts_mask
            LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
        }
    }
};

#ifdef ENABLE_MICROPHONE
static pacs_record_t source_pac_records[] = {
    {
        // codec ID
        {HCI_AUDIO_CODING_FORMAT_LC3, 0x0000, 0x0000},
        // capabilities
        {
            0x3E,
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_8000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_24000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_32000_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ |
            LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ,
            LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US | LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US,
            LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1,
            26,
            155,
            1
        },
        // metadata length
        45,
        // metadata
        {
            // all metadata set
            0x0FFE,
            // (2) preferred_audio_contexts_mask
            LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
            // (2) streaming_audio_contexts_mask
            LE_AUDIO_CONTEXT_MASK_UNSPECIFIED,
        }
    }
};
#endif

//
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

// remote info
static hci_con_handle_t remote_handle;

// iso info
static uint8_t num_cis;
static hci_con_handle_t cis_con_handles[MAX_CHANNELS];
static bool cis_established[MAX_CHANNELS];
static uint16_t iso_interval_1250us;
static uint8_t  burst_number;
static uint8_t  flush_timeout;

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t octets_per_frame;
static uint8_t  num_channels;

// playback - volume in 0..255 to match VCS
static uint8_t  playback_volume = 255;
static bool     playback_active;

// PACS Server
static pacs_streamendpoint_t sink_node;
#ifdef ENABLE_MICROPHONE
static pacs_streamendpoint_t source_node;
#endif

// CSIS Server
#ifdef ENABLE_CSIS_SERVER
#define CSIS_NUM_CLIENTS 1
static csis_server_connection_t csis_coordinators[CSIS_NUM_CLIENTS];
// TODO: random SIRK used for BTstack examples, each consumer device need a unique SIRK
static uint8_t sirk[] = {
    0x83, 0x8E, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A,
    0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8
};
static bool rsi_ready;
static uint8_t rsi[6];
#endif

// ASCS Server
#define ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS 5
#define ASCS_NUM_CLIENTS 3

static ascs_streamendpoint_characteristic_t ascs_streamendpoint_characteristics[ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS];
static ascs_server_connection_t ascs_clients[ASCS_NUM_CLIENTS];
static btstack_timer_source_t  ascs_server_released_timer;
static le_audio_metadata_t     ascs_server_audio_metadata;

static hci_con_handle_t ascs_server_current_client_con_handle;
static uint8_t ascs_server_current_ase_id;

#ifdef ENABLE_MCS_CLIENT
// MCS Client Handler
#define MCS_CHARACTERISTICS_COUNT 30
static uint16_t mcs_cid;
static mcs_client_connection_t mcs_client_connection;
static gatt_service_client_characteristic_t mcs_client_characteristics[MEDIA_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS];
#endif

static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 1,  // connectable
        .primary_advertising_interval_min = 160,    // 100 ms
        .primary_advertising_interval_max = 192,    // 120 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = BD_ADDR_TYPE_LE_PUBLIC,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = 0,
        .scan_request_notification_enable = 0,
};
static le_advertising_set_t le_advertising_set;
static uint8_t adv_handle = 0;

static bool ui_wait_for_configuration;

static void start_advertising(void) {

    static bool adv_initialized = false;

    if (adv_initialized == false){
        adv_initialized = true;

#ifdef ENABLE_CSIS_SERVER
        if (rsi_ready == false) return;
        uint8_t offset = 3 + 2;
        printf("RSI in advertisements at offset %u: ", offset);
        printf_hexdump(rsi, 6);
        reverse_48(rsi, &adv_data[offset]);
#endif

        bd_addr_t local_addr;
        gap_local_bd_addr(local_addr);
        bool local_address_invalid = btstack_is_null_bd_addr( local_addr );
        if (local_address_invalid) {
            extended_params.own_address_type = BD_ADDR_TYPE_LE_RANDOM;
        }
        gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
        if (local_address_invalid) {
            // assume nRF5340 in which case main already set a random address
            uint8_t local_addr_type;
            gap_le_get_own_address(&local_addr_type, local_addr);
            gap_extended_advertising_set_random_address( adv_handle, local_addr );
        }
        gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    }

    gap_extended_advertising_start(adv_handle, 0, 0);

    printf("Start Advertising, waiting for connection\n");
}

void setup_pacs(uint8_t audio_location_mask) {
    // PACS Server
    // - sinks
    sink_node.records_num = 1;
    sink_node.records = &sink_pac_records[0];
    sink_node.audio_locations_mask = audio_location_mask;
    sink_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    sink_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    uint8_t channel_count_mask = LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1;
    if (audio_location_mask == (LE_AUDIO_LOCATION_MASK_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_FRONT_RIGHT)){
        channel_count_mask = LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2;
    }
    sink_pac_records[0].codec_capability.supported_audio_channel_counts_mask = channel_count_mask;
    // - sources
    pacs_streamendpoint_t * sources = NULL;
#ifdef ENABLE_MICROPHONE
    sources = &source_node;
    source_node.records_num = 1;
    source_node.records = &source_pac_records[0];
    source_node.audio_locations_mask = 1;
    source_node.available_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
    source_node.supported_audio_contexts_mask = LE_AUDIO_CONTEXT_MASK_UNSPECIFIED;
#endif
    published_audio_capabilities_service_server_init(&sink_node, sources);
    published_audio_capabilities_service_server_register_packet_handler(&pacs_server_packet_handler);
}

static void update_playback_volume(void){
    const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
    if (sink != NULL){
        sink->set_volume(playback_volume / 2);
    }
}

static void enter_streaming(void){
    // init sink
    le_audio_demo_util_sink_configure_unicast(1, num_channels, sampling_frequency_hz,
                                              frame_duration, octets_per_frame, iso_interval_1250us, flush_timeout);
    playback_active = true;
}

static void stop_streaming(void){
    le_audio_demo_util_sink_close();
    playback_active = false;
}

#define MAX_SET_SIZE 5
static struct config {
    uint8_t set_size;
    uint8_t channel_id;
} configurations[] = {
    { 1, 0 },
    { 1, 1 },
    { 2, 1 },
    { 2, 2 },
    { 3, 1 },
    { 3, 2 },
    { 3, 3 },
    { 4, 1 },
    { 4, 2 },
    { 4, 3 },
    { 4, 4 },
    { 5, 1 },
    { 5, 2 },
    { 5, 3 },
    { 5, 4 },
    { 5, 5 },
};
static const uint8_t configuration_count = sizeof(configurations) / sizeof(struct config);

static void list_configurations(void){
    printf("\n");
    printf("Please select speaker configuration:\n");
    uint8_t option;
    for (option = 0 ; option < configuration_count; option++) {
        printf("%c => %u speakers - %s\n", 'a' + option, configurations[option].set_size,
               channel_names[configurations[option].channel_id]);
    }
    printf("\n");
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    bd_addr_t event_addr;
    hci_con_handle_t cis_handle;
    hci_con_handle_t acl_handle;
    unsigned int i;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    list_configurations();
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (hci_event_disconnection_complete_get_connection_handle(packet) == remote_handle){
                printf("Disconnected, back to advertising\n");
                stop_streaming();
                start_advertising();
            }
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    cis_con_handles[0] = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                    gap_cis_accept(cis_con_handles[0]);
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    gap_subevent_le_connection_complete_get_peer_address(packet, event_addr);
                    remote_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("Connected, remote %s, handle %04x\n", bd_addr_to_str(event_addr), remote_handle);
                    break;
                case GAP_SUBEVENT_CIS_CREATED:
                    // only look for cis handle
                    cis_handle = gap_subevent_cis_created_get_cis_con_handle(packet);
                    acl_handle = gap_subevent_cis_created_get_acl_con_handle(packet);
                    for (i=0; i < num_cis; i++){
                        if (cis_handle == cis_con_handles[i]){
                            cis_established[i] = true;
                            // cache cis info
                            iso_interval_1250us = gap_subevent_cis_created_get_iso_interval_1250us(packet);
                            burst_number        = gap_subevent_cis_created_get_burst_number_c_to_p(packet);
                            flush_timeout       = gap_subevent_cis_created_get_flush_timeout_c_to_p(packet);
                        }
                    }
                    // check for complete
                    bool complete = true;
                    for (i=0; i < num_cis; i++) {
                        complete &= cis_established[i];
                    }
                    if (complete) {
                        printf("All CIS Established\n");
                        printf("- ISO Interval:  %u\n", iso_interval_1250us * 1250);
                        printf("- Burst NUmber:  %u\n", burst_number);
                        printf("- Flush Timeout: %u\n", flush_timeout);

                        enter_streaming();
                        audio_stream_control_service_server_streamendpoint_receiver_start_ready(acl_handle,
                                                                                                ascs_server_current_ase_id);
                        app_state = APP_STREAMING;
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t a_channel, uint8_t *packet, uint16_t size){

    if (playback_active == false) return;

    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;

    // infer stream from con handle - only works for up to 2 channels
    uint8_t stream_index = (con_handle == cis_con_handles[0]) ? 0 : 1;

    le_audio_demo_util_sink_receive(stream_index, packet, size);
}

// PACS Server Handler
static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_PACS_SERVER_AUDIO_LOCATIONS:
            printf("PACS: audio location received\n");
            break;

        default:
            break;
    }
}

#ifdef ENABLE_CSIS_SERVER
// CSIS Server Handler
static void csis_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_CSIS_SERVER_CONNECTED:
            printf("CSIS: LEAUDIO_SUBEVENT_CSIS_SERVER_CONNECTED\n");
            break;
        case LEAUDIO_SUBEVENT_CSIS_SERVER_MEMBER_LOCK:
            printf("CSIS: LEAUDIO_SUBEVENT_CSIS_SERVER_MEMBER_LOCK\n");
        break;
        case LEAUDIO_SUBEVENT_CSIS_SERVER_COORDINATED_SET_SIZE:
            printf("CSIS: LEAUDIO_SUBEVENT_CSIS_SERVER_COORDINATED_SET_SIZE\n");
            break;
        case LEAUDIO_SUBEVENT_CSIS_SERVER_RSI:
            leaudio_subevent_csis_server_rsi_get_rsi(packet, rsi);
            rsi_ready = true;
            break;
        default:
            break;
    }
}
#endif

// VCS Server Handler
static void vcs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_VCS_SERVER_VOLUME_STATE:
            playback_volume = gattservice_subevent_vcs_server_volume_state_get_volume_setting(packet);
            update_playback_volume();
            printf("VCS Server: set volume %3u\n", playback_volume);
            break;
        default:
            break;
    }
}

#ifdef ENABLE_MCS_CLIENT
static void mcs_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    printf("MCS Client: ");
    printf_hexdump(packet, size);

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_CONNECTED:
            printf("MCS Client: connected, status 0x%02x, mcs id %u\n",
                   gattservice_subevent_mcs_client_connected_get_status(packet),
                   gattservice_subevent_client_connected_get_cid(packet));
            break;
        case GATTSERVICE_SUBEVENT_MCS_CLIENT_DISCONNECTED:
            printf("MCS Client: disconnected\n");
            mcs_cid = 0;
            break;
        default:
            break;
    }
}

void mcs_client_connect(hci_con_handle_t con_handle){
    if (mcs_cid == 0) {
        printf("MCS Client: connect\n");
        media_control_service_client_connect_generic_player(con_handle, mcs_client_packet_handler,
            &mcs_client_connection, mcs_client_characteristics, MCS_CHARACTERISTICS_COUNT, &mcs_cid);
    }
}

void mcs_client_disconnect(hci_con_handle_t con_handle) {
    if (mcs_cid != 0) {
        printf("MCS Client: disconnect\n");
        media_control_service_client_disconnect(mcs_cid);
    }
}
#endif

// ANCS Server Handler
static void ascs_server_released_timer_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    printf("ASCS Server Released triggered by timer\n");
    audio_stream_control_service_server_streamendpoint_released(ascs_server_current_client_con_handle, ascs_server_current_ase_id, true);
}

static void ascs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;
    hci_con_handle_t con_handle;
    uint8_t status;

    ascs_codec_configuration_t codec_configuration;
    ascs_qos_configuration_t   qos_configuration;
    uint8_t ase_id;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_ASCS_SERVER_CONNECTED:
            con_handle = leaudio_subevent_ascs_server_connected_get_con_handle(packet);
            status =     leaudio_subevent_ascs_server_connected_get_status(packet);
            printf("ASCS Server: connected, con_handle 0x%04x\n, status 0x%02x", con_handle, status);
#ifdef ENABLE_MCS_CLIENT
            if (configurations[app_config].channel_id < 2){
                printf("Only left/stereo speaker connect to MCS Server -> don't connecty to MCS\n");
            } else {
                mcs_client_connect(con_handle);
            }
#endif
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_DISCONNECTED:
            con_handle = leaudio_subevent_ascs_server_disconnected_get_con_handle(packet);
            printf("ASCS Server: disconnected, con_handle 0x%04xn\n", con_handle);
#ifdef ENABLE_MCS_CLIENT
            mcs_client_disconnect(con_handle);
#endif
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_CODEC_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_server_codec_configuration_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_codec_configuration_get_con_handle(packet);

            // use framing for 441
            codec_configuration.framing = leaudio_subevent_ascs_server_codec_configuration_get_sampling_frequency_index(packet) == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ ? 1 : 0;

            codec_configuration.preferred_phy = LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE;
            codec_configuration.preferred_retransmission_number = 0;
            codec_configuration.max_transport_latency_ms = 0x0FA0;
            codec_configuration.presentation_delay_min_us = 0x0000;
            codec_configuration.presentation_delay_max_us = 0xFFAA;
            codec_configuration.preferred_presentation_delay_min_us = 0;
            codec_configuration.preferred_presentation_delay_max_us = 0;

            // codec id:
            codec_configuration.coding_format =  leaudio_subevent_ascs_server_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = leaudio_subevent_ascs_server_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = leaudio_subevent_ascs_server_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = leaudio_subevent_ascs_server_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = leaudio_subevent_ascs_server_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = leaudio_subevent_ascs_server_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = leaudio_subevent_ascs_server_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = leaudio_subevent_ascs_server_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = leaudio_subevent_ascs_server_codec_configuration_get_frame_blocks_per_sdu(packet);

            // store in existing state
            num_cis = 1;
            num_channels = 1;
            octets_per_frame = codec_configuration.specific_codec_configuration.octets_per_codec_frame;
            sampling_frequency_hz = le_audio_get_sampling_frequency_hz(codec_configuration.specific_codec_configuration.sampling_frequency_index );
            frame_duration = le_audio_util_get_btstack_lc3_frame_duration(codec_configuration.specific_codec_configuration.frame_duration_index);

            printf("ASCS: CODEC_CONFIGURATION_RECEIVED ase_id %d, codec_configuration_mask %02x, con_handle 0x%04x\n", ase_id,
                   codec_configuration.specific_codec_configuration.codec_configuration_mask, con_handle);
            if (codec_configuration.specific_codec_configuration.codec_configuration_mask & (1 <<LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION)){
                if (codec_configuration.specific_codec_configuration.audio_channel_allocation_mask == 3){
                    num_channels = 2;
                }
                printf("- channel allocation: 0x%02x -> %u channels\n", codec_configuration.specific_codec_configuration.audio_channel_allocation_mask, num_channels);
            }
            if (codec_configuration.specific_codec_configuration.codec_configuration_mask & (1 <<LE_AUDIO_CODEC_CONFIGURATION_TYPE_OCTETS_PER_CODEC_FRAME)){
                printf("- octets per codec frame: %3u\n", codec_configuration.specific_codec_configuration.octets_per_codec_frame);
            }
            audio_stream_control_service_server_streamendpoint_configure_codec(con_handle, ase_id, &codec_configuration);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_QOS_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_server_qos_configuration_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_qos_configuration_get_con_handle(packet);

            qos_configuration.cig_id = leaudio_subevent_ascs_server_qos_configuration_get_cig_id(packet);
            qos_configuration.cis_id = leaudio_subevent_ascs_server_qos_configuration_get_cis_id(packet);
            qos_configuration.sdu_interval = leaudio_subevent_ascs_server_qos_configuration_get_sdu_interval(packet);
            qos_configuration.framing = leaudio_subevent_ascs_server_qos_configuration_get_framing(packet);
            qos_configuration.phy = leaudio_subevent_ascs_server_qos_configuration_get_phy(packet);
            qos_configuration.max_sdu = leaudio_subevent_ascs_server_qos_configuration_get_max_sdu(packet);
            qos_configuration.retransmission_number = leaudio_subevent_ascs_server_qos_configuration_get_retransmission_number(packet);
            qos_configuration.max_transport_latency_ms = leaudio_subevent_ascs_server_qos_configuration_get_max_transport_latency(packet);
            qos_configuration.presentation_delay_us = leaudio_subevent_ascs_server_qos_configuration_get_presentation_delay_us(packet);

            printf("ASCS: QOS_CONFIGURATION_RECEIVED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_configure_qos(con_handle, ase_id, &qos_configuration);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_ENABLE:
            ascs_server_current_ase_id = leaudio_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_disable_get_con_handle(packet);
            printf("ASCS: ENABLE ase_id %d\n", ascs_server_current_ase_id);

            audio_stream_control_service_server_streamendpoint_enable(con_handle, ascs_server_current_ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_METADATA:
            con_handle = leaudio_subevent_ascs_server_metadata_get_con_handle(packet);
            ase_id = leaudio_subevent_ascs_server_metadata_get_ase_id(packet);
            printf("ASCS: METADATA_RECEIVED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_metadata_update(con_handle, ase_id, &ascs_server_audio_metadata);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_START_READY:
            ase_id = leaudio_subevent_ascs_server_start_ready_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_start_ready_get_con_handle(packet);
            printf("ASCS: START_READY ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_start_ready(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_STOP_READY:
            ase_id = leaudio_subevent_ascs_server_stop_ready_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_stop_ready_get_con_handle(packet);
            printf("ASCS: STOP_READY ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_receiver_stop_ready(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_DISABLE:
            ase_id = leaudio_subevent_ascs_server_disable_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_disable_get_con_handle(packet);
            printf("ASCS: DISABLING ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_disable(con_handle, ase_id);
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASE:
            ascs_server_current_ase_id            = leaudio_subevent_ascs_server_release_get_ase_id(packet);
            ascs_server_current_client_con_handle = leaudio_subevent_ascs_server_release_get_con_handle(packet);
            printf("ASCS: RELEASE ase_id %d\n", ascs_server_current_ase_id);
            audio_stream_control_service_server_streamendpoint_release(ascs_server_current_client_con_handle, ascs_server_current_ase_id);

            stop_streaming();

            // Client request: Release. Accept (enter Releasing State), and trigger Releasing
            // TODO: find better approach
            btstack_run_loop_remove_timer(&ascs_server_released_timer);
            btstack_run_loop_set_timer_handler(&ascs_server_released_timer, &ascs_server_released_timer_handler);
            btstack_run_loop_set_timer(&ascs_server_released_timer, 100);
            btstack_run_loop_add_timer(&ascs_server_released_timer);
            // stop playback
            playback_active = false;
            break;
        case LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASED:
            ase_id = leaudio_subevent_ascs_server_released_get_ase_id(packet);
            con_handle = leaudio_subevent_ascs_server_released_get_con_handle(packet);
            printf("ASCS: RELEASED ase_id %d\n", ase_id);
            audio_stream_control_service_server_streamendpoint_released(con_handle, ase_id, true);
            break;
        default:
            break;
    }
}

static void app_configure(uint8_t option) {

    app_config = option;
    uint8_t set_size = configurations[app_config].set_size;
    uint8_t channel_id = configurations[app_config].channel_id;

    printf("Configuration: set size %u, channel %s\n", set_size, channel_names[channel_id]);

    const char * filenames[] = { "stereo", "front_left", "front_right", "front_center", "back_left", "back_right"};
    static char filename[128];

    snprintf(filename, sizeof(filename), "headset_%s.wav", filenames[channel_id]);
    printf("Store WAV in %s\n", filename);

    uint8_t set_rank;
    uint8_t audio_location_mask;
    switch (channel_id){
        case 0:
            audio_location_mask = LE_AUDIO_LOCATION_MASK_FRONT_LEFT | LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;
            set_rank = 1;
            break;
        case 1:
            audio_location_mask = LE_AUDIO_LOCATION_MASK_FRONT_LEFT;
            set_rank = 1;
            break;
        case 2:
            audio_location_mask = LE_AUDIO_LOCATION_MASK_FRONT_RIGHT;
            set_rank = 2;
            break;
        case 3:
            audio_location_mask = LE_AUDIO_LOCATION_MASK_FRONT_CENTER;
            set_rank = 3;
            break;
        case 4:
            audio_location_mask = LE_AUDIO_LOCATION_MASK_BACK_LEFT;
            set_rank = 4;
            break;
        case 5:
            audio_location_mask = LE_AUDIO_LOCATION_MASK_BACK_RIGHT;
            set_rank = 5;
            break;
        default:
            btstack_unreachable();
            break;
    }
    setup_pacs(audio_location_mask);
    le_audio_demo_util_sink_init(filename);
    start_advertising();

#ifdef ENABLE_CSIS_SERVER
    coordinated_set_identification_service_server_set_size(set_size);
    coordinated_set_identification_service_server_set_rank(set_rank);
#endif
}

static void show_usage(void){
    printf("\n--- LE Audio Unicast Headset Console ---\n");
    uint8_t set_size = configurations[app_config].set_size;
    uint8_t channel_id = configurations[app_config].channel_id;
    printf("Configuration: set size %u, channel %s\n", set_size, channel_names[channel_id]);
#ifdef ENABLE_MCS_CLIENT
    if (mcs_cid != 0){
        printf(" [ - volume down\n");
        printf(" ] - volume up\n");
        printf(" k - play\n");
        printf(" K - stop (only works in paused mode)\n");
        printf(" L - pause\n");
        printf(" i - next track\n");
        printf(" I - previous track\n");
    }
#endif
    printf("---\n");
}

static void stdin_process(char c){

    printf("STDIN: %c\n", c);

    // handle config selection
    if (ui_wait_for_configuration){
        if (c >= 'a'){
            uint8_t option = c - 'a';
            if (option < configuration_count){
                ui_wait_for_configuration = false;
                app_configure(option);
            } else {
                list_configurations();
            }
        }
    }

    uint8_t volume_step = 10;
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (c){
        case '[':
            if (playback_volume > volume_step){
                playback_volume -= volume_step;
            } else {
                playback_volume = 0;
            }
            volume_control_service_server_set_volume_state(playback_volume, VCS_MUTE_OFF);
            update_playback_volume();
            break;
        case ']':
            if (playback_volume < (255 - volume_step)){
                playback_volume += volume_step;
            } else {
                playback_volume = 255;
            }
            volume_control_service_server_set_volume_state(playback_volume, VCS_MUTE_OFF);
            if (btstack_audio_sink_get_instance() != NULL){
                btstack_audio_sink_get_instance()->set_volume(playback_volume / 2);
            }
            update_playback_volume();
            break;
#ifdef ENABLE_MCS_CLIENT
        case 'k':
            status = media_control_service_client_command_play(mcs_cid);
            printf("MCS Client: play (0x%02x)\n", status);
            break;
        case 'K':
            status = media_control_service_client_command_stop(mcs_cid);
            printf("MCS Client: stop (0x%02x)\n", status);
            break;
        case 'L':
            status = media_control_service_client_command_pause(mcs_cid);
            printf("MCS Client: pause (0x%02x)\n", status);
            break;
        case 'i':
            status = media_control_service_client_command_next_track(mcs_cid);
            printf("MCS Client: next track (0x%02x)\n", status);
            break;
        case 'I':
            status = media_control_service_client_command_previous_track(mcs_cid);
            printf("MCS Client: previous track (0x%02x)\n", status);
            break;
#endif
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

    // setup l2cap
    l2cap_init();

    // LE Secure Connections, Just Works
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // gatt client
    gatt_client_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // ASCS Server
    audio_stream_control_service_server_init(ASCS_NUM_STREAMENDPOINT_CHARACTERISTICS, ascs_streamendpoint_characteristics, ASCS_NUM_CLIENTS, ascs_clients);
    audio_stream_control_service_server_register_packet_handler(&ascs_server_packet_handler);

    // VCS Server
    volume_control_service_server_init(255, VCS_MUTE_OFF, 0, NULL, 0, NULL);
    volume_control_service_server_register_packet_handler(vcs_server_packet_handler);

#ifdef ENABLE_CSIS_SERVER
    // Coordinated Set Server
    coordinated_set_identification_service_server_init(CSIS_NUM_CLIENTS, csis_coordinators, 1, 1);
    coordinated_set_identification_service_server_register_packet_handler(&csis_packet_handler);
    coordinated_set_identification_service_server_set_sirk(CSIS_SIRK_TYPE_PUBLIC, sirk, false);
    coordinated_set_identification_service_server_generate_rsi();
#endif

#ifdef ENABLE_MCS_CLIENT
    // MCS Client
    media_control_service_client_init();
#endif

    // setup audio processing
    le_audio_demo_util_source_init();

    // turn on!
    hci_power_control(HCI_POWER_ON);

    ui_wait_for_configuration = true;
    btstack_stdin_setup(stdin_process);
    return 0;
}
