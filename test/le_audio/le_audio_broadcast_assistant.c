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

#define BTSTACK_FILE__ "le_audio_broadcast_assistant.c"

/*
 * LE Audio Broadcast Assistant
 */


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "ad_parser.h"
#include "ble/sm.h"
#include "bluetooth_data_types.h"
#include "bluetooth_gatt.h"
#include "btstack_audio.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_lc3.h"
#include "btstack_run_loop.h"
#include "btstack_stdin.h"
#include "btstack_util.h"
#include "gap.h"
#include "hci.h"
#include "l2cap.h"
#include "le-audio/gatt-service/broadcast_audio_scan_service_client.h"
#include "le-audio/le_audio_util.h"
#include "le-audio/le_audio_base_parser.h"

static void show_usage(void);

static enum {
    APP_W4_WORKING,
    APP_IDLE,
    APP_W4_SCAN_DELEGATOR_CONNECTION,
    APP_CONNECTED,
    APP_W4_SCAN_BROADCAST_SOURCES,
    APP_W4_PA_AND_BIG_INFO,
    APP_W4_BIG_SYNC_ESTABLISHED,
} app_state = APP_W4_WORKING;

//
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static bool have_scan_delegator;
static bool have_broadcast_source;
static bool have_base;
static bool have_big_info;
static bool scan_for_scan_delegator;
static bool scan_for_broadcast_source;
static uint16_t broadcast_source_scan_duration_s = 10;

// broadcast sink info
#define NUM_BROADCAST_SOURCES 5
#define BROADCAST_SOURCE_NAME_LEN 20
struct {
    char name[BROADCAST_SOURCE_NAME_LEN];
    bd_addr_t addr;
    bd_addr_type_t addr_type;
    uint8_t sid;
    uint32_t broadcast_id;
    uint16_t pa_interval;
} broadcast_sources[NUM_BROADCAST_SOURCES];
static uint8_t broadcast_source_current;
static bool broadcast_source_selected;
static uint8_t broadcast_source_count = 0;
static btstack_timer_source_t broadcast_source_scan_timer;
static bool broadcast_source_synced;

// broadcast info
static hci_con_handle_t sync_handle = HCI_CON_HANDLE_INVALID;
static uint8_t          encryption;
static uint8_t          broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };
static uint8_t          num_bis;
static uint32_t         bis_sync_mask;
static uint32_t         sampling_frequency_hz;
static btstack_lc3_frame_duration_t frame_duration;
static uint16_t octets_per_frame;

// scan delegator info
static char scan_delegator_name[20];
static bd_addr_t scan_delegator;
static bd_addr_type_t scan_delegator_type;
static hci_con_handle_t scan_delegator_handle;

// BASS
#define BASS_CLIENT_NUM_SOURCES 1
static bass_client_connection_t bass_connection;
static bass_client_source_t bass_sources[BASS_CLIENT_NUM_SOURCES];
static bass_source_data_t bass_source_data;
static uint16_t bass_cid;
static uint8_t  bass_source_id;

/* Scanning */

static void start_scanning() {
    have_base = false;
    have_big_info = false;
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();
    printf("Start scan..\n");
}

static void stop_scanning() {
    gap_stop_scan();
    app_state = APP_CONNECTED;
}

static void broadcast_source_scan_timeout_handler(btstack_timer_source_t * ts) {
    stop_scanning();
    printf("Scanning done, found %u Broadcast Sources\n", broadcast_source_count);
    show_usage();
}

/* Scan Delegator */

static void connect_to_scan_delegator(void) {
    printf("Connect to Scan Delegator %s type %u - %s\n", bd_addr_to_str(scan_delegator), scan_delegator_type, scan_delegator_name);
    app_state = APP_W4_SCAN_DELEGATOR_CONNECTION;
    gap_connect(scan_delegator, scan_delegator_type);
}

static void
le_audio_broadcast_assistant_found_scan_delegator(bd_addr_type_t *addr_type, const uint8_t *addr,
                                                  const char *adv_name) {
    memcpy(scan_delegator, addr, 6);
    scan_delegator_type = (*addr_type);
    btstack_strcpy(scan_delegator_name,  sizeof(scan_delegator_name), (const char *) adv_name);
    printf("Broadcast sink found, addr %s, name: '%s'\n", bd_addr_to_str(scan_delegator), scan_delegator_name);
    gap_whitelist_add(scan_delegator_type, scan_delegator);

    stop_scanning();
    connect_to_scan_delegator();
}

/* Broadcast Sources */

static void le_audio_broadcast_assistant_start_periodic_sync() {
    printf("Start Periodic Advertising Sync\n");

    have_base = false;
    have_big_info = false;

    // ignore other advertisements
    gap_set_scan_params(1, 0x30, 0x30, 1);
    // sync to PA
    gap_periodic_advertiser_list_clear();
    gap_periodic_advertiser_list_add(broadcast_sources[broadcast_source_current].addr_type, broadcast_sources[broadcast_source_current].addr, broadcast_sources[broadcast_source_current].sid);
    app_state = APP_W4_PA_AND_BIG_INFO;
    gap_periodic_advertising_create_sync(0x01, broadcast_sources[broadcast_source_current].sid, broadcast_sources[broadcast_source_current].addr_type, broadcast_sources[broadcast_source_current].addr, 0, 1000, 0);
    gap_start_scan();
}

static void handle_periodic_advertisement(const uint8_t * packet, uint16_t size){

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
    bass_source_data.subgroups_num = num_subgroups;
    printf("- num subgroups: %u\n", num_subgroups);
    uint8_t i;
    for (i=0;i<num_subgroups;i++){

        // Cache in new source struct
        bass_source_data.subgroups[i].bis_sync = 0;

        // Level 2: Subgroup Level
        num_bis = le_audio_base_parser_subgroup_get_num_bis(&parser);
        printf("  - num bis[%u]: %u\n", i, num_bis);
        uint8_t codec_specific_configuration_length = le_audio_base_parser_bis_get_codec_specific_configuration_length(&parser);
        const uint8_t * codec_specific_configuration = le_audio_base_parser_subgroup_get_codec_specific_configuration(&parser);
        printf("  - codec specific config[%u]: ", i);
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
                    printf("    - sampling frequency[%u]: %u\n", i, sampling_frequency_hz);
                    break;
                case 0x02: // 0 = 7.5, 1 = 10 ms
                    frame_duration_index =  codec_specific_configuration[codec_offset+1];
                    frame_duration = le_audio_util_get_btstack_lc3_frame_duration(frame_duration_index);
                    printf("    - frame duration[%u]: %u us\n", i, le_audio_get_frame_duration_us(frame_duration_index));
                    break;
                case 0x04:  // octets per coding frame
                    octets_per_frame = little_endian_read_16(codec_specific_configuration, codec_offset+1);
                    printf("    - octets per codec frame[%u]: %u\n", i, octets_per_frame);
                    break;
                default:
                    break;
            }
            codec_offset += ltv_len;
        }
        uint8_t metadata_length = le_audio_base_parser_subgroup_get_metadata_length(&parser);
        const uint8_t * meta_data = le_audio_base_subgroup_parser_get_metadata(&parser);
        printf("  - meta data[%u]: ", i);
        printf_hexdump(meta_data, metadata_length);
        uint8_t k;
        for (k=0;k<num_bis;k++){
            // Level 3: BIS Level
            uint8_t bis_index = le_audio_base_parser_bis_get_index(&parser);

            // check value
            // double check message

            // Cache in new source struct
            bis_sync_mask |= 1 << (bis_index-1);

            bass_source_data.subgroups[i].metadata_length = 0;
            memset(&bass_source_data.subgroups[i].metadata, 0, sizeof(le_audio_metadata_t));

            printf("    - bis index[%u][%u]: %u\n", i, k, bis_index);
            codec_specific_configuration_length = le_audio_base_parser_bis_get_codec_specific_configuration_length(&parser);
            codec_specific_configuration = le_audio_base_bis_parser_get_codec_specific_configuration(&parser);
            printf("    - codec specific config[%u][%u]: ", i, k);
            printf_hexdump(codec_specific_configuration, codec_specific_configuration_length);

            // parse next BIS
            le_audio_base_parser_bis_next(&parser);
        }

        // parse next subgroup
        le_audio_base_parser_subgroup_next(&parser);
    }
}

static void have_base_and_big_info(void){
    stop_scanning();
    broadcast_source_synced = true;
    show_usage();
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

static void add_source() {
    // setup bass source info
    printf("BASS Client: add source with BIS Sync 0x%04x\n", bass_source_data.subgroups[0].bis_sync_state);
    bass_source_data.address_type = broadcast_sources[broadcast_source_current].addr_type;
    memcpy(bass_source_data.address, broadcast_sources[broadcast_source_current].addr, 6);
    bass_source_data.adv_sid = broadcast_sources[broadcast_source_current].sid;
    bass_source_data.broadcast_id = broadcast_sources[broadcast_source_current].broadcast_id;
    bass_source_data.pa_sync = broadcast_source_synced ? LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE : LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE;
    bass_source_data.pa_interval = broadcast_sources[broadcast_source_current].pa_interval;
    // bass_source_new.subgroups_num set in BASE parser
    // bass_source_new.subgroup[i].* set in BASE parser

    // add bass source
    broadcast_audio_scan_service_client_add_source(bass_cid, &bass_source_data);
}

/* Packet Handlers */

static void bass_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    const bass_source_data_t * source_data;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_BASS_CLIENT_CONNECTED:
            if (leaudio_subevent_bass_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("BASS client connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x\n",
                       bass_cid, scan_delegator_handle,
                       leaudio_subevent_bass_client_connected_get_status(packet));
                return;
            }
            printf("BASS client connected, cid 0x%02x\n", bass_cid);
            app_state = APP_CONNECTED;
            break;
        case LEAUDIO_SUBEVENT_BASS_CLIENT_SOURCE_OPERATION_COMPLETE:
            if (leaudio_subevent_bass_client_source_operation_complete_get_status(packet) != ERROR_CODE_SUCCESS){
                printf("BASS client source operation failed, status 0x%02x\n", leaudio_subevent_bass_client_source_operation_complete_get_status(packet));
                break;
            }

            if ( leaudio_subevent_bass_client_source_operation_complete_get_opcode(packet) == (uint8_t)BASS_OPCODE_ADD_SOURCE ){
                // TODO: set state to 'wait for source_id"
                printf("BASS client add source operation completed, wait for source_id\n");
            }
            break;
        case LEAUDIO_SUBEVENT_BASS_CLIENT_NOTIFICATION_COMPLETE:
            // store source_id
            bass_source_id = leaudio_subevent_bass_client_notification_complete_get_source_id(packet);
            printf("BASS client notification, source_id = 0x%02x\n", bass_source_id);
            source_data = broadcast_audio_scan_service_client_get_source_data(bass_cid, bass_source_id);
            btstack_assert(source_data != NULL);

            switch (source_data->pa_sync_state){
                case LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST:
                    // start pa sync transfer
                    printf("LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST -> Start PAST\n");
                     // TODO: unclear why this needs to be shifted for PAST with TS to get test green
                    uint16_t service_data = bass_source_id << 8;
                    gap_periodic_advertising_sync_transfer_send(scan_delegator_handle, service_data, sync_handle);
                    break;
                default:
                    break;
            }
            break;
        case LEAUDIO_SUBEVENT_BASS_CLIENT_DISCONNECTED:
            break;
        default:
            break;
    }
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t adv_size;
    const uint8_t * adv_data;
    bool found_scan_delegator = false;
    bool found_broadcast_source = false;
    bool add_broadcast_source = false;
    char adv_name[31];
    uint16_t uuid;
    ad_context_t context;

    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            switch (btstack_event_state_get_state(packet)) {
                case HCI_STATE_WORKING:
                    app_state = APP_IDLE;
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

        case GAP_EVENT_ADVERTISING_REPORT:
            // TODO: contains partial copy of GAP_EVENT_EXTENDED_ADVERTISING_REPORT below
            adv_size = gap_event_advertising_report_get_data_length(packet);
            adv_data = gap_event_advertising_report_get_data(packet);
            adv_name[0] = 0;

            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        switch (uuid){
                            case ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE:
                                if (scan_for_broadcast_source) {
                                    broadcast_sources[broadcast_source_count].broadcast_id = little_endian_read_24(data, 2);
                                    found_broadcast_source = true;
                                }
                                break;
                            case ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE:
                                if (scan_for_scan_delegator) {
                                    found_scan_delegator = true;
                                }
                                break;
                            default:
                                break;
                        }
                        break;
                    case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                    case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                        size = btstack_min(sizeof(adv_name) - 1, size);
                        memcpy(adv_name, data, size);
                        adv_name[size] = 0;
                        break;
                    default:
                        break;
                }
            }

            if ((have_scan_delegator == false) && found_scan_delegator){
                have_scan_delegator = true;
                bd_addr_t addr;
                gap_event_extended_advertising_report_get_address(packet, addr);
                bd_addr_type_t addr_type = (bd_addr_type_t) gap_event_extended_advertising_report_get_address_type(packet);
                le_audio_broadcast_assistant_found_scan_delegator(&addr_type, addr, adv_name);
            }
            break;

        case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
            adv_size = gap_event_extended_advertising_report_get_data_length(packet);
            adv_data = gap_event_extended_advertising_report_get_data(packet);
            adv_name[0] = 0;

            for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
                uint8_t data_type = ad_iterator_get_data_type(&context);
                uint8_t size = ad_iterator_get_data_len(&context);
                const uint8_t *data = ad_iterator_get_data(&context);
                switch (data_type){
                    case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                        uuid = little_endian_read_16(data, 0);
                        switch (uuid){
                            case ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE:
                                if (scan_for_broadcast_source) {
                                    broadcast_sources[broadcast_source_count].broadcast_id = little_endian_read_24(data, 2);
                                    found_broadcast_source = true;
                                }
                                break;
                            case ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE:
                                if (scan_for_scan_delegator) {
                                    found_scan_delegator = true;
                                }
                                break;
                            default:
                                break;
                        }
                        break;
                    case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                    case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                        size = btstack_min(sizeof(adv_name) - 1, size);
                        memcpy(adv_name, data, size);
                        adv_name[size] = 0;
                        break;
                    default:
                        break;
                }
            }

            if ((have_scan_delegator == false) && found_scan_delegator){
                have_scan_delegator = true;
                bd_addr_t addr;
                gap_event_extended_advertising_report_get_address(packet, addr);
                bd_addr_type_t addr_type = (bd_addr_type_t) gap_event_extended_advertising_report_get_address_type(packet);
                le_audio_broadcast_assistant_found_scan_delegator(&addr_type, addr, adv_name);
            }
            if (found_broadcast_source){
                have_broadcast_source = true;
                bd_addr_t addr;
                gap_event_extended_advertising_report_get_address(packet, addr);
                bd_addr_type_t type = gap_event_extended_advertising_report_get_address_type(packet);
                uint8_t i;
                add_broadcast_source = true;
                for (i=0;i<broadcast_source_count;i++) {
                    if (memcmp(addr, broadcast_sources[i].addr, 6) != 0) continue;
                    if (type != broadcast_sources[i].addr_type) continue;
                    add_broadcast_source = false;
                }
            }
            if (add_broadcast_source && (broadcast_source_count < NUM_BROADCAST_SOURCES)) {
                gap_event_extended_advertising_report_get_address(packet, broadcast_sources[broadcast_source_count].addr);
                broadcast_sources[broadcast_source_count].addr_type = gap_event_extended_advertising_report_get_address_type(packet);
                broadcast_sources[broadcast_source_count].sid = gap_event_extended_advertising_report_get_advertising_sid(packet);
                btstack_strcpy(broadcast_sources[broadcast_source_count].name,  BROADCAST_SOURCE_NAME_LEN, (const char *) adv_name);
                printf("Broadcast source found, addr %s, name: '%s'\n", bd_addr_to_str(broadcast_sources[broadcast_source_count].addr), broadcast_sources[broadcast_source_count].name);
                gap_whitelist_add(broadcast_sources[broadcast_source_count].addr_type, broadcast_sources[broadcast_source_count].addr);
                broadcast_source_count++;
            }
            break;

        case HCI_EVENT_LE_META:
            switch(hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                    sync_handle = hci_subevent_le_periodic_advertising_sync_establishment_get_sync_handle(packet);
                    broadcast_sources[broadcast_source_current].pa_interval = hci_subevent_le_periodic_advertising_sync_establishment_get_periodic_advertising_interval(packet);
                    printf("Periodic advertising sync with handle 0x%04x established\n", sync_handle);
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_REPORT:
                    if (have_base) break;
                    handle_periodic_advertisement(packet, size);
                    if (have_base & have_big_info){
                        have_base_and_big_info();
                    }
                    break;
                case HCI_SUBEVENT_LE_BIGINFO_ADVERTISING_REPORT:
                    if (have_big_info) break;
                    handle_big_info(packet, size);
                    if (have_base & have_big_info){
                        have_base_and_big_info();
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
                    start_scanning();
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)){
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    if (gap_subevent_le_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS) break;
                    scan_delegator_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("Connection complete, handle 0x%04x\n", scan_delegator_handle);
                    broadcast_audio_scan_service_client_connect(&bass_connection,
                                                                bass_sources,
                                                                BASS_CLIENT_NUM_SOURCES,
                                                                scan_delegator_handle,
                                                                &bass_cid);
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

/* UI */

static void show_usage(void){
    printf("\n--- LE Audio Broadcast Assistant Test Console ---\n");
    switch (app_state) {
        case APP_IDLE:
            // printf("s - setup LE Broadcast Sink with Broadcast Source via Scan Delegator\n");
            printf("c - scan and connect to Scan Delegator\n");
            break;
        case APP_CONNECTED:
            if (broadcast_source_selected) {
                printf("Selected Broadcast source: addr %s, type %u, name: %s, synced %u\n",
                    bd_addr_to_str(broadcast_sources[broadcast_source_current].addr),
                    broadcast_sources[broadcast_source_current].addr_type,
                    broadcast_sources[broadcast_source_current].name,
                    broadcast_source_synced);
            }
            printf("d - scan for Broadcast sources (%u s)\n", broadcast_source_scan_duration_s);
            if (broadcast_source_count > 0) {
                printf("1..%u select one of the Broadcast Sources:\n", broadcast_source_count);
                uint8_t i;
                for (i=0;i<broadcast_source_count;i++) {
                    printf("%u. addr  %s, type %u, name: %s\n", i+1,
                        bd_addr_to_str(broadcast_sources[broadcast_source_current].addr),
                        broadcast_sources[broadcast_source_current].addr_type,
                        broadcast_sources[broadcast_source_current].name);
                }
                if (broadcast_source_synced) {
                    printf("a - add source with BIS Sync 0x%08x\n", bis_sync_mask);
                    printf("A - add source with BIS Sync 0x00000000 (do not sync)\n");
                    printf("m - modify source to PA Sync = 0, bis sync = 0x00000000\n");
                    printf("M - modify source to PA Sync = 2, bis sync = 0x%08x\n", bis_sync_mask);
                    printf("b - send Broadcast Code: ");
                    printf_hexdump(broadcast_code, sizeof(broadcast_code));
                    printf("r - remove source\n");
                }
            }
            break;
        default:
            printf("[LE Audio Broadcast Assistant busy]\n");
            break;
    }
    printf("---\n");
}

static void stdin_process(char c){
    switch (app_state) {
        case APP_IDLE:
            switch (c) {
                case 'c':
                    scan_for_scan_delegator = true;
                    scan_for_broadcast_source = false;
                    start_scanning();
                    app_state = APP_W4_SCAN_DELEGATOR_CONNECTION;
                    break;
                default:
                    break;
            }
            break;
        case APP_CONNECTED:
            if ((c >= '1') && (c <= '9')){
                uint8_t digit = c - '0';
                if (digit <= broadcast_source_count) {
                    if (sync_handle != HCI_CON_HANDLE_INVALID) {
                        gap_periodic_advertising_terminate_sync(sync_handle);
                    }
                    broadcast_source_synced   = false;
                    broadcast_source_selected = true;
                    broadcast_source_current = digit-1;
                    printf("Selected  addr  %s, type %u, name: %s\n",
                        bd_addr_to_str(broadcast_sources[broadcast_source_current].addr),
                        broadcast_sources[broadcast_source_current].addr_type,
                        broadcast_sources[broadcast_source_current].name);
                    le_audio_broadcast_assistant_start_periodic_sync();
                }
                break;
            }
            switch (c) {
                case 'd':
                    scan_for_scan_delegator = false;
                    scan_for_broadcast_source = true;
                    start_scanning();
                    btstack_run_loop_set_timer(&broadcast_source_scan_timer, broadcast_source_scan_duration_s * 1000);
                    btstack_run_loop_set_timer_handler(&broadcast_source_scan_timer, broadcast_source_scan_timeout_handler);
                    btstack_run_loop_add_timer(&broadcast_source_scan_timer);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    switch (c){
        case 'a':
            bass_source_data.subgroups[0].bis_sync = bis_sync_mask;
            add_source();
            break;
        case 'A':
            bass_source_data.subgroups[0].bis_sync = 0;
            add_source();
            break;
        case 'm':
            bass_source_data.pa_sync = LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA;
            bass_source_data.subgroups[0].bis_sync_state = 0;
            broadcast_audio_scan_service_client_modify_source(bass_cid, bass_source_id, &bass_source_data);
            break;
        case 'M':
            bass_source_data.pa_sync = LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE;
            bass_source_data.subgroups[0].bis_sync_state = bis_sync_mask;
            broadcast_audio_scan_service_client_modify_source(bass_cid, bass_source_id, &bass_source_data);
            break;
        case 'r':
            broadcast_audio_scan_service_client_remove_source(bass_cid, bass_source_id);
            break;
        case 'b':
            broadcast_audio_scan_service_client_set_broadcast_code(bass_cid, bass_source_id, broadcast_code);
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

    l2cap_init();
    sm_init();
    gatt_client_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    broadcast_audio_scan_service_client_init(&bass_packet_handler);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
