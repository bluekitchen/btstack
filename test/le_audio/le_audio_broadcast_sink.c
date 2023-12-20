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

#define BTSTACK_FILE__ "le_audio_broadcast_sink.c"

/*
 * LE Audio Broadcast Sink
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "ad_parser.h"
#include "ble/att_server.h"
#include "ble/sm.h"
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
#include "btstack_lc3plus_fraunhofer.h"
#include "l2cap.h"
#include "le-audio/le_audio_util.h"
#include "le-audio/le_audio_base_parser.h"
#include "le-audio/gatt-service/broadcast_audio_scan_service_server.h"

#include "le_audio_broadcast_sink.h"
#include "le_audio_demo_util_sink.h"

// max config
#define MAX_NUM_BIS 2
#define MAX_SAMPLES_PER_FRAME 480

// playback
#define MAX_NUM_LC3_FRAMES   5
#define MAX_BYTES_PER_SAMPLE 4
#define PLAYBACK_BUFFER_SIZE (MAX_NUM_LC3_FRAMES * MAX_SAMPLES_PER_FRAME * MAX_BYTES_PER_SAMPLE)

static void show_usage(void);

static enum {
    APP_W4_WORKING,
    APP_W4_BROADCAST_ADV,
    APP_W4_PA_AND_BIG_INFO,
    APP_W4_BROADCAST_CODE,
    APP_W4_BIG_SYNC_ESTABLISHED,
    APP_STREAMING,
    APP_IDLE
} app_state = APP_W4_WORKING;

static const uint8_t adv_sid = 0;
static le_advertising_set_t le_advertising_set;
static uint8_t adv_handle = 0;

static le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 1,  // connectable
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

static const uint8_t extended_adv_data[] = {
        // 16 bit service data, ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE,
        3, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID,
            ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE & 0xff, ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE >> 8,
       // name
        5, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'i', 'n', 'k'
};

#define BASS_NUM_CLIENTS 1
#define BASS_NUM_SOURCES 1
static bass_source_data_t       bass_source_new;
static bass_server_source_t     bass_sources[BASS_NUM_SOURCES];
static bass_server_connection_t bass_clients[BASS_NUM_CLIENTS];

//
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static bool have_base;
static bool have_big_info;
static bool have_broadcast_code;
static bool standalone_mode;
static uint32_t bis_sync_mask;

uint16_t samples_received;
uint16_t samples_dropped;

// remote info
static char remote_name[20];
static bd_addr_t remote;
static bd_addr_type_t remote_type;
static uint8_t remote_sid;
static bool count_mode;
static bool pts_mode;
static bool nrf5340_audio_demo;

// broadcast assistant
static hci_con_handle_t commander_acl_handle;
static bd_addr_t        commander_address;
static bd_addr_type_t   commander_type;

// broadcast info
static const uint8_t    big_handle = 1;
static hci_con_handle_t sync_handle;
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint8_t          encryption;
static uint8_t          broadcast_code[16];

static btstack_timer_source_t broadcast_sink_pa_sync_timer;

// BIG Sync
static le_audio_big_sync_t        big_sync_storage;
static le_audio_big_sync_params_t big_sync_params;

// lc3 codec config
static uint16_t sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t octets_per_frame;
static uint8_t  num_bis;

static void handle_periodic_advertisement(const uint8_t * packet, uint16_t size){
    // nRF534_audio quirk - no BASE in periodic advertisement
    if (nrf5340_audio_demo){
        // hard coded config LC3
        // default: mono bitrate 96000, 10 ms with USB audio source, 120 octets per frame
        count_mode = 0;
        pts_mode   = 0;
        num_bis    = 1;
        sampling_frequency_hz = 48000;
        frame_duration = BTSTACK_LC3_FRAME_DURATION_10000US;
        octets_per_frame = 120;
        have_base = true;
        return;
    }

    // periodic advertisement contains the BASE
    // TODO: BASE might be split across multiple advertisements
    const uint8_t * adv_data = hci_subevent_le_periodic_advertising_report_get_data(packet);
    uint16_t adv_size = hci_subevent_le_periodic_advertising_report_get_data_length(packet);
    uint8_t adv_status = hci_subevent_le_periodic_advertising_report_get_data_status(packet);

    if (adv_status != 0) {
        printf("Periodic Advertisement (status %u): ", adv_status);
        printf_hexdump(adv_data, adv_size);
        return;
    }

    le_audio_base_parser_t parser;
    bool ok = le_audio_base_parser_init(&parser, adv_data, adv_size);
    if (ok == false){
        return;
    }
    have_base = true;

    printf("BASE:\n");
    uint32_t presentation_delay = le_audio_base_parser_get_presentation_delay(&parser);
    printf("- presentation delay: %"PRIu32" us\n", presentation_delay);
    uint8_t num_subgroups = le_audio_base_parser_get_num_subgroups(&parser);
    // Cache in new source struct
    bass_source_new.subgroups_num = num_subgroups;
    printf("- num subgroups: %u\n", num_subgroups);
    uint8_t i;
    for (i=0;i<num_subgroups;i++){
        // Level 2: Subgroup Level
        printf("  - Subgroup %u\n", i);
        num_bis = le_audio_base_parser_subgroup_get_num_bis(&parser);
        printf("    - num bis[%u]: %u\n", i, num_bis);
        uint8_t codec_specific_configuration_length = le_audio_base_parser_subgroup_get_codec_specific_configuration_length(&parser);
        const uint8_t * codec_specific_configuration = le_audio_base_parser_subgroup_get_codec_specific_configuration(&parser);
        printf("    - codec specific config[%u]: ", i);
        printf_hexdump(codec_specific_configuration, codec_specific_configuration_length);
        // parse config to get sampling frequency and frame duration
        uint8_t codec_offset = 0;
        while ((codec_offset + 1) < codec_specific_configuration_length){
            uint8_t ltv_len = codec_specific_configuration[codec_offset++];
            uint8_t ltv_type = codec_specific_configuration[codec_offset];
            const uint32_t sampling_frequency_map[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 384000 };
            uint8_t sampling_frequency_index;
            uint8_t frame_duration_index;
            switch (ltv_type){
                case 0x01: // sampling frequency
                    sampling_frequency_index = codec_specific_configuration[codec_offset+1];
                    // TODO: check range
                    sampling_frequency_hz = sampling_frequency_map[sampling_frequency_index - 1];
                    printf("      - sampling frequency[%u]: %u\n", i, sampling_frequency_hz);
                    break;
                case 0x02: // 0 = 7.5, 1 = 10 ms
                    frame_duration_index =  codec_specific_configuration[codec_offset+1];
                    frame_duration = le_audio_util_get_btstack_lc3_frame_duration(frame_duration_index);
                    printf("      - frame duration[%u]: %u us\n", i, le_audio_get_frame_duration_us(frame_duration_index));
                    break;
                case 0x04:  // octets per coding frame
                    octets_per_frame = little_endian_read_16(codec_specific_configuration, codec_offset+1);
                    printf("      - octets per codec frame[%u]: %u\n", i, octets_per_frame);
                    break;
                default:
                    break;
            }
            codec_offset += ltv_len;
        }
        uint8_t metadata_length = le_audio_base_parser_subgroup_get_metadata_length(&parser);
        const uint8_t * meta_data = le_audio_base_subgroup_parser_get_metadata(&parser);
        printf("    - meta data[%u]: ", i);
        printf_hexdump(meta_data, metadata_length);
        uint8_t k;
        for (k=0;k<num_bis;k++){
            printf("      - BIS %u\n", k);
            // Level 3: BIS Level
            uint8_t bis_index =  le_audio_base_parser_bis_get_index(&parser);
            if ((bis_index == 0) || (bis_index > 30)){
                continue;
            }

            // collect bis sync mask
            bis_sync_mask |=  1 << (bis_index - 1);

            bass_source_new.subgroups[i].metadata_length = 0;
            memset(&bass_source_new.subgroups[i].metadata, 0, sizeof(le_audio_metadata_t));

            printf("        - bis index[%u][%u]: %u\n", i, k, bis_index);
            codec_specific_configuration_length = le_audio_base_parser_bis_get_codec_specific_configuration_length(&parser);
            codec_specific_configuration = le_audio_base_bis_parser_get_codec_specific_configuration(&parser);
            printf("        - codec specific config[%u][%u]: ", i, k);
            printf_hexdump(codec_specific_configuration, codec_specific_configuration_length);
            // parse next BIS
            le_audio_base_parser_bis_next(&parser);
        }
        
        // parse next subgroup
        le_audio_base_parser_subgroup_next(&parser);
    }
}

static void handle_big_info(const uint8_t * packet, uint16_t size){
    printf("BIG Info advertising report\n");
    sync_handle = hci_subevent_le_biginfo_advertising_report_get_sync_handle(packet);
    encryption = hci_subevent_le_biginfo_advertising_report_get_encryption(packet);
    if (encryption) {
        printf("Stream is encrypted\n");
    }
    have_big_info = true;
}

static void enter_create_big_sync(void){
    // stop scanning
    gap_stop_scan();

    // init sink
    uint16_t iso_interval_1250us = frame_duration == BTSTACK_LC3_FRAME_DURATION_7500US ? 6 : 8;
    uint8_t  pre_transmission_offset = 1;
    le_audio_demo_util_sink_configure_broadcast(num_bis, 1, sampling_frequency_hz, frame_duration, octets_per_frame,
                                              iso_interval_1250us, pre_transmission_offset);

    big_sync_params.big_handle = big_handle;
    big_sync_params.sync_handle = sync_handle;
    big_sync_params.encryption = encryption;
    if (encryption) {
        memcpy(big_sync_params.broadcast_code, &broadcast_code[0], 16);
    } else {
        memset(big_sync_params.broadcast_code, 0, 16);
    }
    big_sync_params.mse = 0;
    big_sync_params.big_sync_timeout_10ms = 100;
    big_sync_params.num_bis = num_bis;
    uint8_t i;
    printf("BIG Create Sync for BIS: ");
    for (i=0;i<num_bis;i++){
        big_sync_params.bis_indices[i] = i + 1;
        printf("%u ", big_sync_params.bis_indices[i]);
    }
    printf("\n");
    app_state = APP_W4_BIG_SYNC_ESTABLISHED;
    gap_big_sync_create(&big_sync_storage, &big_sync_params);
}

static void start_scanning() {
    app_state = APP_W4_BROADCAST_ADV;
    have_base = false;
    have_big_info = false;
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();
    printf("Start scan..\n");
}

static void start_advertising(void) {
    gap_extended_advertising_start(adv_handle, 0, 0);
}

static void setup_advertising(void) {
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
    start_advertising();
}

static void got_base_and_big_info() {
    // add source
    if (standalone_mode) {
        printf("BASS: add Broadcast Source\n");
        // add source
        uint8_t source_index = 0;
        broadcast_audio_scan_service_server_add_source(&bass_source_new, &source_index);
        broadcast_audio_scan_service_server_set_pa_sync_state(0, LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA);
    }

    // update num subgroups from BASE
    bass_sources[0].data.subgroups_num = bass_source_new.subgroups_num;

    if ((encryption == false) || have_broadcast_code){
        enter_create_big_sync();
    } else {
        printf("BASS: request Broadcast Code\n");
        bass_sources[0].big_encryption = LE_AUDIO_BIG_ENCRYPTION_BROADCAST_CODE_REQUIRED;
        broadcast_audio_scan_service_server_set_pa_sync_state(0, LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA);
        app_state = APP_W4_BROADCAST_CODE;
    }
}

static void start_pa_sync(bd_addr_type_t source_type, bd_addr_t source_addr) {
    // ignore other advertisements
    gap_whitelist_add(source_type, source_addr);
    gap_set_scan_params(1, 0x30, 0x30, 1);
    // sync to PA
    gap_periodic_advertiser_list_clear();
    gap_periodic_advertiser_list_add(source_type, source_addr, remote_sid);
    app_state = APP_W4_PA_AND_BIG_INFO;
    printf("Start Periodic Advertising Sync\n");
    gap_periodic_advertising_create_sync(0x01, remote_sid, source_type, source_addr, 0, 1000, 0);
    gap_start_scan();
}

static void pa_sync_established() {
    have_base = false;
    have_big_info = false;
    app_state = APP_W4_PA_AND_BIG_INFO;
}

static void stop_pa_sync(void) {
    app_state = APP_IDLE;
    le_audio_demo_util_sink_close();
    printf("Terminate BIG SYNC\n");
    gap_big_sync_terminate(big_handle);
    int i;
    for (i=0;i<num_bis;i++){
        if (bis_con_handles[i] != HCI_CON_HANDLE_INVALID) {
            gap_disconnect(bis_con_handles[i]);
        }
    }

}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch(btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_IDLE;
                    // setup advertising and allow to receive periodic advertising sync transfers
                    setup_advertising();
                    gap_periodic_advertising_sync_transfer_set_default_parameters(2, 0, 0x2000, 0);

#ifdef ENABLE_DEMO_MODE
                    start_scanning();
#else
                    show_usage();
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
        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
        {
            if (app_state != APP_W4_BROADCAST_ADV) break;

            gap_event_extended_advertising_report_get_address(packet, remote);
            uint8_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            const uint8_t * adv_data = gap_event_extended_advertising_report_get_data(packet);

            ad_context_t context;
            bool found = false;
            remote_name[0] = '\0';
            uint16_t uuid;
            uint32_t broadcast_id;
            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                const uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t data_size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        if (uuid == ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE){
                            broadcast_id = little_endian_read_24(data, 2);
                            found = true;
                        }
                        break;
                    case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                    case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                        data_size = btstack_min(sizeof(remote_name) - 1, data_size);
                        memcpy(remote_name, data, data_size);
                        remote_name[data_size] = 0;
                        // support for nRF5340 Audio DK
                        if (strncmp("NRF5340", remote_name, 7) == 0){
                            nrf5340_audio_demo = true;
                            found = true;
                        }
                        break;
                    default:
                        break;
                }
            }
            if (!found) break;
            remote_type = gap_event_extended_advertising_report_get_address_type(packet);
            remote_sid = gap_event_extended_advertising_report_get_advertising_sid(packet);
            pts_mode = strncmp("PTS-", remote_name, 4) == 0;
            count_mode = strncmp("COUNT", remote_name, 5) == 0;
            printf("Broadcast sink found, addr %s, name: '%s' (pts-mode: %u, count: %u), Broadcast_ID 0%06x\n", bd_addr_to_str(remote), remote_name, pts_mode, count_mode, broadcast_id);

            // setup bass source info
            bass_source_new.address_type = remote_type;
            memcpy(bass_source_new.address, remote, 6);
            bass_source_new.adv_sid = remote_sid;
            bass_source_new.broadcast_id = broadcast_id;
            bass_source_new.pa_sync = LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE;
            bass_source_new.pa_interval = gap_event_extended_advertising_report_get_periodic_advertising_interval(packet);

            // start pa sync
            start_pa_sync(remote_type, remote);
            break;
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // restart advertisements on disconnect
            if (hci_event_disconnection_complete_get_connection_handle(packet) == commander_acl_handle) {
                printf("Broadcast Assistant disconnected\n");
                commander_acl_handle = HCI_CON_HANDLE_INVALID;
                start_advertising();
            }
            break;
        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_TRANSFER_RECEIVED:
                    // PAST implies broadcast source has been added by client
                    sync_handle = hci_subevent_le_periodic_advertising_sync_transfer_received_get_sync_handle(packet);
                    broadcast_audio_scan_service_server_set_pa_sync_state(0, LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA);
                    btstack_run_loop_remove_timer(&broadcast_sink_pa_sync_timer);
                    printf("Periodic advertising sync transfer with handle 0x%04x received\n", sync_handle);
                    pa_sync_established();
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                    sync_handle = hci_subevent_le_periodic_advertising_sync_establishment_get_sync_handle(packet);
                    printf("Periodic advertising sync with handle 0x%04x established\n", sync_handle);
                    pa_sync_established();
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT:
                    if (app_state != APP_W4_PA_AND_BIG_INFO) break;
                    if (have_base) break;
                    handle_periodic_advertisement(packet, size);
                    if (have_base && have_big_info){
                        got_base_and_big_info();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIGINFO_ADVERTISING_REPORT:
                    if (app_state != APP_W4_PA_AND_BIG_INFO) break;
                    if (have_big_info) break;
                    handle_big_info(packet, size);
                    if (have_base && have_big_info){
                        got_base_and_big_info();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIG_SYNC_LOST:
                    printf("BIG Sync Lost\n");
                    {
                        const btstack_audio_sink_t * sink = btstack_audio_sink_get_instance();
                        if (sink != NULL) {
                            sink->stop_stream();
                            sink->close();
                        }
                    }
                    // start over
                    if (!standalone_mode) break;
                    start_scanning();
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    commander_acl_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    commander_type = gap_subevent_le_connection_complete_get_peer_address_type(packet);
                    gap_subevent_le_connection_complete_get_peer_address(packet, commander_address);
                    printf("Broadcast Assistant connected, handle 0x%04x - %s type %u\n", commander_acl_handle, bd_addr_to_str(commander_address), commander_type);
                    break;
                case GAP_SUBEVENT_BIG_SYNC_CREATED: {
                    printf("BIG Sync created with BIS Connection handles: ");
                    uint8_t i;
                    for (i=0;i<num_bis;i++){
                        bis_con_handles[i] = gap_subevent_big_sync_created_get_bis_con_handles(packet, i);
                        printf("0x%04x ", bis_con_handles[i]);
                    }
                    printf("\n");
                    app_state = APP_STREAMING;
                    printf("Start receiving\n");

                    printf("Terminate PA Sync\n");
                    gap_periodic_advertising_terminate_sync(sync_handle);

                    // update BIS Sync state
                    bass_sources[0].data.subgroups[0].bis_sync_state = bis_sync_mask;
                    broadcast_audio_scan_service_server_set_pa_sync_state(0, LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA);
                    break;
                }
                case GAP_SUBEVENT_BIG_SYNC_STOPPED:
                    printf("BIG Sync stopped, big_handle 0x%02x\n", gap_subevent_big_sync_stopped_get_big_handle(packet));
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        default:
            break;
    }
}

static void show_usage(void){
    printf("\n--- LE Audio Broadcast Sink Test Console ---\n");
    printf("s - start scanning\n");
#ifdef HAVE_LC3PLUS
    printf("q - use LC3plus decoder if 10 ms ISO interval is used\n");
#endif
    printf("e - set broadcast code to 0x11111111111111111111111111111111\n");
    printf("t - terminate BIS streams\n");
    printf("---\n");
}

static void stdin_process(char c){
    switch (c){
        case 's':
            if (app_state != APP_IDLE) break;
            standalone_mode = true;
            start_scanning();
            break;
        case 'e':
            printf("Set broadcast code to 0x11111111111111111111111111111111\n");
            memset(broadcast_code, 0x11, 16);
            have_broadcast_code = true;
            break;
#ifdef HAVE_LC3PLUS
        case 'q':
            printf("Use LC3plus decoder for 10 ms ISO interval...\n");
            le_audio_demo_util_sink_enable_lc3plus(true);
            break;
#endif
        case 't':
            switch (app_state){
                case APP_STREAMING:
                case APP_W4_BIG_SYNC_ESTABLISHED:
                    stop_pa_sync();
                    break;
                default:
                    break;
            }
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    void (*packet_receive)(uint8_t stream_index, uint8_t *packet, uint16_t size) = le_audio_demo_util_sink_receive;

    // get stream_index from con_handle
    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    uint8_t i;

    if (count_mode){
        packet_receive = le_audio_demo_util_sink_count;
    }

    for (i=0;i<num_bis;i++){
        if (bis_con_handles[i] == con_handle) {
            packet_receive(i, packet, size);
        }
    }
}

static void broadcast_sync_pa_sync_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    printf("PAST Timeout -> LE_AUDIO_PA_SYNC_STATE_NO_PAST\n");
    broadcast_audio_scan_service_server_set_pa_sync_state(0, LE_AUDIO_PA_SYNC_STATE_NO_PAST);
}

static void handle_new_pa_sync(uint8_t source_id, le_audio_pa_sync_t pa_sync) {
    switch (pa_sync){
        case LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA:
            printf("Stop PA Sync\n");
            stop_pa_sync();
            bass_sources[0].data.subgroups[0].bis_sync_state = 0;
            broadcast_audio_scan_service_server_set_pa_sync_state(0, LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA);
            break;
        case LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE:
            printf("LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE -> Request SyncInfo\n");
            broadcast_audio_scan_service_server_set_pa_sync_state(source_id, LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST);
            // start timeout
            btstack_run_loop_set_timer_handler(&broadcast_sink_pa_sync_timer, broadcast_sync_pa_sync_timeout_handler);
            btstack_run_loop_set_timer(&broadcast_sink_pa_sync_timer, 10000);   // 10 seconds
            btstack_run_loop_add_timer(&broadcast_sink_pa_sync_timer);
            break;
        case LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE:
            printf("LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE -> Start Periodic Advertising Sync\n");
            start_pa_sync(bass_sources[source_id].data.address_type, bass_sources[source_id].data.address);
            break;
        default:
            break;
    }
}

static void bass_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    btstack_assert (packet_type == HCI_EVENT_PACKET);
    btstack_assert(hci_event_packet_get_type(packet) == HCI_EVENT_GATTSERVICE_META);
    uint8_t source_id;
    printf("BASS Event 0x%02x: ", hci_event_gattservice_meta_get_subevent_code(packet));
    printf_hexdump(packet, size);
    uint8_t pa_sync;
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_ADDED:
            pa_sync = gattservice_subevent_bass_server_source_added_get_pa_sync(packet);
            source_id = gattservice_subevent_bass_server_source_added_get_source_id(packet);
            printf("GATTSERVICE_SUBEVENT_BASS_SOURCE_ADDED, source_id 0x%04x, pa_sync %u\n", source_id, pa_sync);
            handle_new_pa_sync(source_id, pa_sync);
            break;
        case GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_MODIFIED:
            pa_sync = gattservice_subevent_bass_server_source_added_get_pa_sync(packet);
            source_id = gattservice_subevent_bass_server_source_added_get_source_id(packet);
            printf("GATTSERVICE_SUBEVENT_BASS_SOURCE_MODIFIED, source_id 0x%04x, pa_sync %u\n", source_id, pa_sync);
            handle_new_pa_sync(source_id, pa_sync);
            break;
        case GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_DELETED:
            printf("GATTSERVICE_SUBEVENT_BASS_SERVER_SOURCE_DELETED, source_id 0x%04x\n",
                   gattservice_subevent_bass_server_source_deleted_get_source_id(packet));
            break;
        case GATTSERVICE_SUBEVENT_BASS_SERVER_BROADCAST_CODE:
            gattservice_subevent_bass_server_broadcast_code_get_broadcast_code(packet, broadcast_code);
            printf("GATTSERVICE_SUBEVENT_BASS_BROADCAST_CODE received: ");
            printf_hexdump(broadcast_code, 16);
            bass_sources[0].big_encryption = LE_AUDIO_BIG_ENCRYPTION_DECRYPTING;
            if (have_base && have_big_info){
                enter_create_big_sync();
            }
            break;
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void) argv;
    (void) argc;

    l2cap_init();
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // setup BASS Server
    broadcast_audio_scan_service_server_init(BASS_NUM_SOURCES, bass_sources, BASS_NUM_CLIENTS, bass_clients);
    broadcast_audio_scan_service_server_register_packet_handler(&bass_packet_handler);

    // setup audio processing
    le_audio_demo_util_sink_init("le_audio_broadcast_sink.wav");

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
