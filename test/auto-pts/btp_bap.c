/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btp_bap.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btpclient.h"
#include "btp.h"
#include "btp_bap.h"
#include "btp_aics.h"
#include "btp_mcp.h"
#include "btp_mcs.h"
#include "btp_micp.h"
#include "btp_mics.h"
#include "btp_pacs.h"
#include "btp_vcp.h"
#include "btp_vcs.h"
#include "btp_vocs.h"
#include "btp_server.h"
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"
#include "le-audio/le_audio_base_parser.h"
#include "btp_tmap.h"
#include "btp_tbs.h"

#define  MAX_NUM_BIS 2
#define  MAX_CHANNELS 2
#define MAX_LC3_FRAME_BYTES 155

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_handler_t btp_bap_higher_layer_handler;

// send iso packets
static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint16_t packet_sequence_numbers[MAX_NUM_BIS];
static uint8_t  iso_payload_data[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];
static btstack_timer_source_t iso_timer;

static uint8_t adv_handle;

static le_advertising_set_t le_advertising_set;

static uint8_t period_adv_data[255];
static uint16_t period_adv_data_len;

static le_audio_big_t big_storage;
le_audio_big_params_t btp_bap_big_params;

static const uint8_t adv_sid = 0;

// Scan
static bool scan_for_scan_delegator;
static bool scan_for_broadcast_audio_announcement;
static bool have_base;
static bool have_big_info;
static uint32_t       periodic_advertising_sync_broadcast_id;
static bd_addr_t      periodic_advertising_sync_address;
static bd_addr_type_t periodic_advertising_sync_addr_type;
static bass_source_data_t bass_source_data;
static hci_con_handle_t sync_handle = HCI_CON_HANDLE_INVALID;

// BIG Sync
static const uint8_t              big_handle = 1;
static le_audio_big_sync_t        big_sync_storage;
static le_audio_big_sync_params_t big_sync_params;

// Advertising etc
static const le_periodic_advertising_parameters_t periodic_params = {
        .periodic_advertising_interval_min = 0x258, // 375 ms
        .periodic_advertising_interval_max = 0x258, // 375 ms
        .periodic_advertising_properties = 0
};

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

static uint8_t extended_adv_data[] = {
        // 16 bit service data, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE, Broadcast ID
        6, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x52, 0x18, 0, 0, 0,
        // name
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P', 'T', 'S', '-', 'x', 'x',
        7, BLUETOOTH_DATA_TYPE_BROADCAST_NAME ,     'P', 'T', 'S', '-', 'x', 'x',
};

// TBS
#define TBS_BEARERS_MAX_NUM 2
#define CALL_POOL_SIZE  (10)
typedef struct {
    telephone_bearer_service_server_t bearer;
    uint16_t id;
    char *scheme;
} my_bearer_t;
static my_bearer_t tbs_bearers[TBS_BEARERS_MAX_NUM];
static uint8_t tbs_bearer_index;
static tbs_call_data_t calls[CALL_POOL_SIZE];
static uint16_t call_ids[CALL_POOL_SIZE];

static void btp_bap_send_response_if_pending(void){
    if ((response_service_id == BTP_SERVICE_ID_BAP) && (response_op != 0)){
        btp_send(BTP_SERVICE_ID_BAP, response_op, 0, 0, NULL);
        response_op = 0;
    }
}

static void btp_trigger_iso(btstack_timer_source_t * ts){
    hci_request_bis_can_send_now_events(btp_bap_big_params.big_handle);
}

static void handle_extended_advertisement(const uint8_t * packet, uint16_t size){
    ad_context_t context;
    uint16_t uuid;
    uint16_t adv_size = gap_event_extended_advertising_report_get_data_length(packet);
    const uint8_t * const adv_data = gap_event_extended_advertising_report_get_data(packet);
    for (ad_iterator_init(&context, adv_size, adv_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        const uint8_t *data = ad_iterator_get_data(&context);
        switch (data_type){
            case BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID:
                uuid = little_endian_read_16(data, 0);
                switch (uuid){
                    case ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_ANNOUNCEMENT_SERVICE:
                        if (scan_for_broadcast_audio_announcement) {
                            uint32_t broadcast_id = little_endian_read_24(data, 2);
                            uint8_t buffer[13];
                            uint8_t pos = 0;
                            buffer[pos++] = gap_event_extended_advertising_report_get_address_type(packet);
                            bd_addr_t addr;
                            gap_event_extended_advertising_report_get_address(packet, addr);
                            reverse_bd_addr(addr, &buffer[pos]);
                            pos += 6;
                            little_endian_store_24(buffer, pos, broadcast_id);
                            pos += 3;
                            buffer[pos++] = gap_event_extended_advertising_report_get_advertising_sid(packet);
                            uint16_t pa_interval = gap_event_extended_advertising_report_get_periodic_advertising_interval(packet);
                            little_endian_store_16(buffer, pos, pa_interval);
                            pos += 2;
                            btstack_assert(pos == sizeof(buffer));
                            btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_BAA_FOUND, 0, sizeof(buffer), buffer);
                        }
                        break;
                    case ORG_BLUETOOTH_SERVICE_BROADCAST_AUDIO_SCAN_SERVICE:
                        if (scan_for_scan_delegator) {
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
    uint32_t bis_sync_mask = 0;
    for (i=0;i<num_subgroups;i++){

        // Cache in new source struct
        bass_source_data.subgroups[i].bis_sync = 0;

        // Level 2: Subgroup Level
        uint8_t num_bis = le_audio_base_parser_subgroup_get_num_bis(&parser);
        const uint8_t * const codec_id = le_audio_base_parser_subgroup_get_codec_id(&parser);
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
            uint32_t sampling_frequency_hz;
            uint8_t frame_duration_index;
            uint16_t frame_duration;
            uint16_t octets_per_frame;
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

            // emit BIS Found event
            uint8_t buffer[255];
            uint16_t pos = 0;
            buffer[pos++] = periodic_advertising_sync_addr_type;
            reverse_bd_addr(periodic_advertising_sync_address, &buffer[pos]);
            pos += 6;
            little_endian_store_24(buffer, pos, periodic_advertising_sync_broadcast_id);
            pos += 3;
            little_endian_store_16(buffer, pos, presentation_delay);
            pos += 2;
            buffer[pos++] = i;
            buffer[pos++] = k;
            memcpy(&buffer[pos], codec_id, 5);
            pos += 5;
            buffer[pos++] = codec_specific_configuration_length;
            memcpy(&buffer[pos], codec_specific_configuration, codec_specific_configuration_length);
            pos += codec_specific_configuration_length;
            btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_BIS_FOUND, 0, pos, buffer);

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
        MESSAGE("Stream is encrypted\n");
    }
    have_big_info = true;
}

static void have_base_and_big_info(void){
    MESSAGE("Broadcast Sink Synced -> Stop Scanning to reduce resources");
    gap_stop_scan();
}

static void handle_big_sync_created(const uint8_t * packet, uint16_t size){
    printf("BIG Sync created with BIS Connection handles: ");
    uint8_t i;
    uint8_t num_bis = gap_subevent_big_sync_created_get_num_bis(packet);
    for (i=0;i<num_bis;i++){
        bis_con_handles[i] = gap_subevent_big_sync_created_get_bis_con_handles(packet, i);
        printf("0x%04x ", bis_con_handles[i]);

        // emit BIS Synced event
        uint8_t buffer[255];
        uint16_t pos = 0;
        buffer[pos++] = periodic_advertising_sync_addr_type;
        reverse_bd_addr(periodic_advertising_sync_address, &buffer[pos]);
        pos += 6;
        little_endian_store_24(buffer, pos, periodic_advertising_sync_broadcast_id);
        pos += 3;
        buffer[pos++] = big_sync_params.bis_indices[i];
        btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_BIS_SYNCED, 0, pos, buffer);
    }
    printf("\n");

    printf("Terminate PA Sync\n");
    gap_periodic_advertising_terminate_sync(sync_handle);
}

// HCI Handler
static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    hci_con_handle_t con_handle;
    hci_con_handle_t bis_index;
    uint8_t cig_id;
    uint8_t cis_id;
    uint8_t i;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    for (i=0; i < MAX_NUM_BIS ; i++){
                        con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
                        if (bis_con_handles[i] == con_handle){
                            MESSAGE("HCI Disconnect for bis_con_handle 0x%04x, with index %u", bis_index, i);
                            bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
                        }
                    }
                    break;
                case HCI_EVENT_BIS_CAN_SEND_NOW:
                    bis_index = hci_event_bis_can_send_now_get_bis_index(packet);
                    le_audio_demo_util_source_send(bis_index, bis_con_handles[bis_index]);
                    bis_index++;
                    if (bis_index == btp_bap_big_params.num_bis){
                        le_audio_demo_util_source_generate_iso_frame(AUDIO_SOURCE_SINE);
                        btstack_run_loop_set_timer(&iso_timer, 100);
                        btstack_run_loop_set_timer_handler(&iso_timer, &btp_trigger_iso);
                        btstack_run_loop_add_timer(&iso_timer);
                    }
                    break;
                case GAP_EVENT_EXTENDED_ADVERTISING_REPORT:
                    handle_extended_advertisement(packet, size);
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)){
                        case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                            sync_handle = hci_subevent_le_periodic_advertising_sync_establishment_get_sync_handle(packet);
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
                        default:
                            break;
                    }
                    break;
                case HCI_EVENT_META_GAP:
                    switch (hci_event_gap_meta_get_subevent_code(packet)) {
                        case GAP_SUBEVENT_BIG_CREATED:
                            for (i=0; i < btp_bap_big_params.num_bis; i++){
                                bis_con_handles[i] = gap_subevent_big_created_get_bis_con_handles(packet, i);
                            }
                            btp_bap_send_response_if_pending();
                            hci_request_bis_can_send_now_events(btp_bap_big_params.big_handle);
                            break;
                        case GAP_SUBEVENT_BIG_TERMINATED:
                            btp_bap_send_response_if_pending();
                            break;

                        case GAP_SUBEVENT_BIG_SYNC_CREATED:
                            handle_big_sync_created(packet, size);
                            break;
                        default:
                            break;
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

static void bass_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    const bass_source_data_t * source_data;
    uint16_t bass_cid;
    server_t * server;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_BASS_CLIENT_CONNECTED:
			bass_cid = leaudio_subevent_bass_client_connected_get_bass_cid(packet);
            if (leaudio_subevent_bass_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                MESSAGE("BASS client connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x",
                       bass_cid, remote_handle,
                       leaudio_subevent_bass_client_connected_get_status(packet));
                return;
            }
            MESSAGE("BASS client connected, cid 0x%02x\n", bass_cid);

            // inform btp
            if ((response_service_id == BTP_SERVICE_ID_BAP) && (response_op != 0)){
                server = btp_server_for_bass_cid(bass_cid);
                btstack_assert(server != NULL);
                response_op = 0;
                uint8_t data[7];
                data[0] = server->address_type;
                reverse_bd_addr(server->address, &data[1]);
                btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_SCAN_DELEGATOR_FOUND, 0, sizeof(data), data);
            }
            break;
        case LEAUDIO_SUBEVENT_BASS_CLIENT_SOURCE_OPERATION_COMPLETE:
            if (leaudio_subevent_bass_client_source_operation_complete_get_status(packet) != ERROR_CODE_SUCCESS){
                MESSAGE("BASS client source operation failed, status 0x%02x", leaudio_subevent_bass_client_source_operation_complete_get_status(packet));
                break;
            }

            // inform btp
            if ((response_service_id == BTP_SERVICE_ID_BAP) && (response_op == BTP_BAP_ADD_BROADCAST_SRC)) {
                if (leaudio_subevent_bass_client_source_operation_complete_get_opcode(packet) == (uint8_t) BASS_OPCODE_ADD_SOURCE) {
                    MESSAGE("BTP_BAP_ADD_BROADCAST_SRC completed");
                    btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_ADD_BROADCAST_SRC, 0, 0, NULL);
                }
            }
            else if ((response_service_id == BTP_SERVICE_ID_BAP) && (response_op == BTP_BAP_REMOVE_BROADCAST_SRC)) {
                if (leaudio_subevent_bass_client_source_operation_complete_get_opcode(packet) == (uint8_t) BASS_OPCODE_REMOVE_SOURCE) {
                    MESSAGE("BTP_BAP_REMOVE_BROADCAST_SRC completed");
                    btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_REMOVE_BROADCAST_SRC, 0, 0, NULL);
                }
            }
            else if ((response_service_id == BTP_SERVICE_ID_BAP) && (response_op == BTP_BAP_MODIFY_BROADCAST_SRC)) {
                if (leaudio_subevent_bass_client_source_operation_complete_get_opcode(packet) == (uint8_t) BASS_OPCODE_MODIFY_SOURCE) {
                    MESSAGE("BTP_BAP_MODIFY_BROADCAST_SRC completed");
                    btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_MODIFY_BROADCAST_SRC, 0, 0, NULL);
                }
            }
            else {
                MESSAGE("BTP_BAP_OPERATION %u completed", leaudio_subevent_bass_client_source_operation_complete_get_opcode(packet));
            }
            break;
        case LEAUDIO_SUBEVENT_BASS_CLIENT_NOTIFICATION_COMPLETE:
            // store source_id
            bass_cid = leaudio_subevent_bass_client_notification_complete_get_bass_cid(packet);
            server = btp_server_for_bass_cid(bass_cid);
            btstack_assert(server != NULL);

            server->bass_source_id = leaudio_subevent_bass_client_notification_complete_get_source_id(packet);
            MESSAGE("BASS client notification, source_id = 0x%02x", server->bass_source_id);
            source_data = broadcast_audio_scan_service_client_get_source_data(bass_cid, server->bass_source_id);
            btstack_assert(source_data != NULL);

            // inform btp
            {
                /*  <B6s B B6s B3sBBB
                    Address_Type (1 octet)
					Address (6 octets)
					Source ID (1 octet)
					Broadcaster Address Type (1 octet)
					Broadcaster Address (6 octets)
					Advertiser SID (1 octet)
					Broadcast ID (3 octets)
					PA Sync State (1 octet)
					BIG Encryption (1 octet)
					Num Subgroups (1 octet)
					Subgroups (varies)
                 */
                response_len = 0;
                btp_append_uint8(server->address_type);
                reverse_bd_addr(server->address, &response_buffer[response_len]);
                response_len += 6;
                btp_append_uint8(server->bass_source_id);
                btp_append_uint8(source_data->address_type);
                reverse_bd_addr(source_data->address, &response_buffer[response_len]);
                response_len += 6;
                btp_append_uint8(source_data->adv_sid);
                btp_append_uint24(source_data->broadcast_id);
                btp_append_uint8(source_data->pa_sync_state);
                btp_append_uint8(source_data->big_encryption);
                btp_append_uint8(0); // no subgroups for now
                btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_BROADCAST_RECEIVE_STATE, 0, response_len,  response_buffer);
            }

            switch (source_data->pa_sync_state){
                case LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST:
                    // start pa sync transfer
                    printf("LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST -> Start PAST\n");
                    // TODO: unclear why this needs to be shifted for PAST with TS to get test green
                    uint16_t service_data = server->bass_source_id << 8;
                    gap_periodic_advertising_sync_transfer_send(remote_handle, service_data, sync_handle);
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

    if (btp_bap_higher_layer_handler != NULL){
        (*btp_bap_higher_layer_handler)(packet_type, channel, packet, size);
    }
}

static void expect_status_no_error(uint8_t status){
    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("STATUS = 0x%02x -> ABORT", status);
    }
    btstack_assert(status == ERROR_CODE_SUCCESS);
}

void btp_bap_start_advertising(uint32_t broadcast_id) {
    if (adv_handle == 0xff){
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
    }

    // set broadcast id
    little_endian_store_24(extended_adv_data, 4, broadcast_id);

    uint8_t status;
    status = gap_extended_advertising_set_adv_data(adv_handle, sizeof(extended_adv_data), extended_adv_data);
    if (status != 0) MESSAGE("Status 0x%02x", status);
    status = gap_periodic_advertising_set_params(adv_handle, &periodic_params);
    if (status != 0) MESSAGE("Status 0x%02x", status);
    status = gap_periodic_advertising_set_data(adv_handle, period_adv_data_len, period_adv_data);
    if (status != 0) MESSAGE("Status 0x%02x", status);
    status = gap_periodic_advertising_start(adv_handle, 0);
    if (status != 0) MESSAGE("Status 0x%02x", status);
    status = gap_extended_advertising_start(adv_handle, 0, 0);
    if (status != 0) MESSAGE("Status 0x%02x", status);
}

void btp_bap_stop_advertising(void) {
    gap_periodic_advertising_stop(adv_handle);
    gap_extended_advertising_stop(adv_handle);
}

void btp_bap_setup_big(void){
    // Create BIG. From BTP_BAP_BROADCAST_SOURCE_SETUP
    // - num_bis
    // - max sdu
    // - max_transport_latency_ms
    // - rtn
    // - sdu interval us
    // - framing
    btp_bap_big_params.big_handle = 0;
    btp_bap_big_params.advertising_handle = adv_handle;
    btp_bap_big_params.phy = 2;
    btp_bap_big_params.packing = 0;
    btp_bap_big_params.encryption = encryption;
    if (encryption) {
        memcpy(btp_bap_big_params.broadcast_code, &broadcast_code[0], 16);
    } else {
        memset(btp_bap_big_params.broadcast_code, 0, 16);
    }
    gap_big_create(&big_storage, &btp_bap_big_params);
}

void btp_bap_release_big(void){
    uint8_t status = gap_big_terminate(btp_bap_big_params.big_handle);
    MESSAGE("gap_big_terminate, status 0x%x", status);
}

void btp_bap_setup_periodic_advertising_data(uint8_t num_bis, uint32_t presentation_delay_us, const uint8_t *const codec_id,
                                             uint8_t subgroup_codec_ltv_len, const uint8_t *const subgroup_codec_ltv_bytes,
                                             uint8_t subgroup_metadata_ltv_len, const uint8_t *const subgroup_metadata_ltv_bytes,
                                             uint8_t stream_codec_ltv_len, const uint8_t *const stream_codec_ltv_bytes) {

    le_audio_base_builder_t builder;
    le_audio_base_builder_init(&builder, period_adv_data, sizeof(period_adv_data), presentation_delay_us);
    le_audio_base_builder_add_subgroup(&builder, codec_id, subgroup_codec_ltv_len,
                                       subgroup_codec_ltv_bytes,
                                       subgroup_metadata_ltv_len, subgroup_metadata_ltv_bytes);
    uint8_t i;
    for (i=1;i<= num_bis;i++){
        le_audio_base_builder_add_bis(&builder, i, stream_codec_ltv_len, stream_codec_ltv_bytes);
    }
    period_adv_data_len = le_audio_base_builder_get_ad_data_size(&builder);
}

static void start_scanning() {
    have_base = false;
    have_big_info = false;
    gap_set_scan_params(1, 0x30, 0x30, 0);
    gap_start_scan();
    printf("Start scan..\n");
}

static void stop_scanning() {
    gap_stop_scan();
}

static void
btp_bap_start_periodic_sync(bd_addr_type_t addr_type, bd_addr_t addr, uint32_t broadcast_id, uint8_t sid, uint16_t skip,
                            uint16_t sync_timeout) {
    MESSAGE("Start Periodic Advertising Sync with %s, broadcast id %06x", bd_addr_to_str(addr), broadcast_id);

    have_base = false;
    have_big_info = false;

    // cache for btp
    memcpy(periodic_advertising_sync_address, addr, 6);
    periodic_advertising_sync_addr_type = addr_type;
    periodic_advertising_sync_broadcast_id = broadcast_id;

    // ignore other advertisements
    gap_set_scan_params(1, 0x30, 0x30, 1);
    // sync to PA
    gap_periodic_advertiser_list_clear();
    gap_periodic_advertiser_list_add(addr_type, addr, sid);
    gap_periodic_advertising_create_sync(0x01, sid, addr_type, addr, skip, sync_timeout, 0);
    gap_start_scan();
}

static void enter_create_big_sync(uint32_t requested_bis_sync){
    // num bis from requested_bis_sync for 1 or 2 bis
    uint8_t num_bis = 0;
    if ((requested_bis_sync & 1) != 0){
        num_bis++;
    }
    if ((requested_bis_sync & 2) != 0){
        num_bis++;
    }

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
    gap_big_sync_create(&big_sync_storage, &big_sync_params);
}

static void iso_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {

    // get stream_index from con_handle
    uint16_t header = little_endian_read_16(packet, 0);
    hci_con_handle_t con_handle = header & 0x0fff;
    uint8_t i;

    for (i=0;i<big_sync_params.num_bis;i++){
        if (bis_con_handles[i] == con_handle) {
            // emit BIS Synced event
            uint8_t buffer[400];
            uint16_t pos = 0;
            buffer[pos++] = periodic_advertising_sync_addr_type;
            reverse_bd_addr(periodic_advertising_sync_address, &buffer[pos]);
            pos += 6;
            little_endian_store_24(buffer, pos, periodic_advertising_sync_broadcast_id);
            pos += 3;
            buffer[pos++] = big_sync_params.bis_indices[i];
            buffer[pos++] = size;
            memcpy(&buffer[pos], packet, size);
            pos += size;
            btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_BIS_STREAM_RECEIVED, 0, pos, buffer);
        }
    }
}

// BAP Discovery

typedef enum {
    BTP_BAP_STATE_IDLE = 0,
    BTP_BAP_PACS_STATE_W4_CONNECTED,
    BTP_BAP_PACS_STATE_W4_SINK_PACS,
    BTP_BAP_PACS_STATE_W4_SOURCE_PACS,
    BTP_BAP_ASCS_STATE_W4_CONNECTED,
    BTP_BAP_STATE_DONE
} btp_bap_state_t;

static void btp_bap_send_discovery_complete(server_t * server) {
    struct btp_bap_discovery_completed_ev discovery_completed_ev;
    discovery_completed_ev.addr_type = server->address_type;
    reverse_bd_addr(server->address, discovery_completed_ev.address);
    MESSAGE("BTP_BAP_EV_DISCOVERY_COMPLETED, con handle 0x%04x - DONE", server->acl_con_handle);
    btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_DISCOVERY_COMPLETED, 0, sizeof(discovery_completed_ev), (const uint8_t *) &discovery_completed_ev);
}

static void btp_bap_discovery_next(server_t * server){
    MESSAGE("BTP BAP Discovery %s, state %u (%p)", bd_addr_to_str(server->address), server->bap_state, server);
    uint8_t status = ERROR_CODE_SUCCESS;
    switch ((btp_bap_state_t) server->bap_state){
        case BTP_BAP_STATE_IDLE:
            server->bap_state = (uint8_t) BTP_BAP_PACS_STATE_W4_CONNECTED;
            status = published_audio_capabilities_service_client_connect(&server->pacs_connection, server->acl_con_handle, &server->pacs_cid);
            MESSAGE("BTP PACS: connect 0x%04x, PACS CID %u", server->acl_con_handle, server->csis_cid);
            break;
        case BTP_BAP_PACS_STATE_W4_CONNECTED:
            server->bap_state = (uint8_t) BTP_BAP_PACS_STATE_W4_SINK_PACS;
            status = published_audio_capabilities_service_client_get_sink_pacs(server->pacs_cid);
            MESSAGE("BTP PACS Client %u: Get Sink PAC records", server->server_id);
            if (status == ERROR_CODE_SUCCESS){
                break;
            }

            // fall through //

        case BTP_BAP_PACS_STATE_W4_SINK_PACS:
            server->bap_state = (uint8_t) BTP_BAP_PACS_STATE_W4_SOURCE_PACS;
            status = published_audio_capabilities_service_client_get_source_pacs(server->pacs_cid);
            MESSAGE("BTP PACS Client %u: Get Source PAC records", server->server_id);
            if (status == ERROR_CODE_SUCCESS){
                break;
            }

            // fall through //

        case BTP_BAP_PACS_STATE_W4_SOURCE_PACS:
            MESSAGE("BTP PACS Client %u: PAC Discovery Done", server->server_id);

            server->bap_state = (uint8_t) BTP_BAP_ASCS_STATE_W4_CONNECTED;
            status = audio_stream_control_service_client_connect(&server->ascs_connection,
                                                        server->streamendpoint_characteristics,
                                                        ASCS_CLIENT_NUM_STREAMENDPOINTS,
                                                        server->acl_con_handle,
                                                        &server->ascs_cid);
            MESSAGE("BTP ASCS: connect 0x%04x, ASCS CID %u", server->acl_con_handle, server->ascs_cid);
            break;
        case BTP_BAP_ASCS_STATE_W4_CONNECTED:
            server->bap_state = (uint8_t) BTP_BAP_STATE_DONE;
            btp_bap_send_discovery_complete(server);
            break;
        default:
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("BTP BAP Discovery status 0x%02x abort", status);
        btstack_assert(false);
    }
}

// ASCS

static void btp_bap_send_ase(server_t * server, uint8_t direction, uint8_t num_ases, const uint8_t * ase_ids){
    uint8_t i;
    for (i=0;i<num_ases;i++){
        uint8_t buffer[100];
        uint16_t pos = 0;
        buffer[pos++] = server->address_type;
        reverse_bd_addr( server->address, &buffer[pos]);
        pos += 6;
        buffer[pos++] = direction;
        buffer[pos++] = ase_ids[i];
        MESSAGE("BTP_BAP_EV_ASE_FOUND %s", bd_addr_to_str(server->address));
        btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_ASE_FOUND, 0, pos, buffer);
    }
}

static void btp_ascs_client_report_ases(server_t *server, uint8_t *packet){
    uint8_t sink_ase_num = leaudio_subevent_ascs_client_connected_get_sink_ase_num(packet);
    const uint8_t * sink_ase_ids = leaudio_subevent_ascs_client_connected_get_sink_ase_ids(packet);
    btp_bap_send_ase(server, BTP_AUDIO_DIR_SINK, sink_ase_num, sink_ase_ids);
    uint8_t source_ase_num = leaudio_subevent_ascs_client_connected_get_source_ase_num(packet);
    const uint8_t * source_ase_ids = leaudio_subevent_ascs_client_connected_get_source_ase_ids(packet);
    btp_bap_send_ase(server, BTP_AUDIO_DIR_SOURCE, source_ase_num, source_ase_ids);
}

static void btp_bap_ascs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    ascs_codec_configuration_t codec_configuration;
    hci_con_handle_t con_handle;
    ascs_state_t ase_state;
    uint8_t response_code;
    uint8_t reason;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    uint8_t ase_id;
    uint8_t num_ases;
    uint8_t i;
    uint8_t opcode;
    hci_connection_t * hci_connection;
    server_t * server;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_ASCS_CLIENT_CONNECTED:
            ascs_cid = leaudio_subevent_ascs_client_connected_get_ascs_cid(packet);
            con_handle = leaudio_subevent_ascs_client_connected_get_con_handle(packet);
            server = btp_server_for_acl_con_handle(con_handle);
            if (leaudio_subevent_ascs_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("BTP ASCS Client: connection failed, cid 0x%02x, con_handle 0x%04x, status 0x%02x", ascs_cid, con_handle,
                        leaudio_subevent_ascs_client_connected_get_status(packet));
                server->ascs_cid = 0;
            } else {
                MESSAGE("BTP ASCS Client %u: connected, cid %u", server->server_id, server->ascs_cid);
                btp_ascs_client_report_ases(server, packet);
            }
            btp_bap_discovery_next(server);
            break;
        case LEAUDIO_SUBEVENT_ASCS_CLIENT_DISCONNECTED:
            MESSAGE("BTP ASCS Client: disconnected, cid 0x%02x", leaudio_subevent_ascs_client_disconnected_get_ascs_cid(packet));
            ascs_cid = leaudio_subevent_ascs_client_disconnected_get_ascs_cid(packet);
            server = btp_server_for_ascs_cid(con_handle);
            if (server != NULL){
                server->ascs_cid = 0;
            }
            break;
#if 0
        case LEAUDIO_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION:
            ase_id = leaudio_subevent_ascs_client_codec_configuration_get_ase_id(packet);
            ascs_cid = leaudio_subevent_ascs_client_codec_configuration_get_ascs_cid(packet);

            // codec id:
            codec_configuration.coding_format =  leaudio_subevent_ascs_client_codec_configuration_get_coding_format(packet);;
            codec_configuration.company_id = leaudio_subevent_ascs_client_codec_configuration_get_company_id(packet);
            codec_configuration.vendor_specific_codec_id = leaudio_subevent_ascs_client_codec_configuration_get_vendor_specific_codec_id(packet);

            codec_configuration.specific_codec_configuration.codec_configuration_mask = leaudio_subevent_ascs_client_codec_configuration_get_specific_codec_configuration_mask(packet);
            codec_configuration.specific_codec_configuration.sampling_frequency_index = leaudio_subevent_ascs_client_codec_configuration_get_sampling_frequency_index(packet);
            codec_configuration.specific_codec_configuration.frame_duration_index = leaudio_subevent_ascs_client_codec_configuration_get_frame_duration_index(packet);
            codec_configuration.specific_codec_configuration.audio_channel_allocation_mask = leaudio_subevent_ascs_client_codec_configuration_get_audio_channel_allocation_mask(packet);
            codec_configuration.specific_codec_configuration.octets_per_codec_frame = leaudio_subevent_ascs_client_codec_configuration_get_octets_per_frame(packet);
            codec_configuration.specific_codec_configuration.codec_frame_blocks_per_sdu = leaudio_subevent_ascs_client_codec_configuration_get_frame_blocks_per_sdu(packet);

            MESSAGE("ASCS Client: CODEC CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            response_len = 0;
            btp_append_uint24(leaudio_subevent_ascs_client_codec_configuration_get_presentation_delay_min(packet));
            btp_append_uint24(leaudio_subevent_ascs_client_codec_configuration_get_presentation_delay_max(packet));
            btp_send(BTP_SERVICE_ID_LE_AUDIO, BTP_LE_AUDIO_OP_ASCS_CONFIGURE_CODEC, 0, response_len, response_buffer);
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_QOS_CONFIGURATION:
            ase_id   = leaudio_subevent_ascs_client_qos_configuration_get_ase_id(packet);
            ascs_cid = leaudio_subevent_ascs_client_qos_configuration_get_con_handle(packet);

            MESSAGE("ASCS Client: QOS CONFIGURATION - ase_id %d, ascs_cid 0x%02x", ase_id, ascs_cid);
            if (response_op != 0){
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_METADATA:
            ase_id  = leaudio_subevent_ascs_client_metadata_get_ase_id(packet);
            ascs_cid = leaudio_subevent_ascs_client_metadata_get_ascs_cid(packet);

            printf("ASCS Client: METADATA UPDATE - ase_id %d, ascs_cid 0x%02x\n", ase_id, ascs_cid);
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_CONTROL_POINT_OPERATION_RESPONSE:
            ase_id        = leaudio_subevent_ascs_client_control_point_operation_response_get_ase_id(packet);
            ascs_cid      = leaudio_subevent_ascs_client_control_point_operation_response_get_ascs_cid(packet);
            response_code = leaudio_subevent_ascs_client_control_point_operation_response_get_response_code(packet);
            reason        = leaudio_subevent_ascs_client_control_point_operation_response_get_reason(packet);
            opcode        = leaudio_subevent_ascs_client_control_point_operation_response_get_opcode(packet);
            printf("ASCS Client: Operation complete - ase_id %d, opcode %u, response [0x%02x, 0x%02x], ascs_cid 0x%02x\n", ase_id, opcode, response_code, reason, ascs_cid);
            if ((opcode == ASCS_OPCODE_RECEIVER_START_READY) && (response_op != 0)){
                MESSAGE("Receiver Start Ready completed");
                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                response_op = 0;
            }
            break;

        case LEAUDIO_SUBEVENT_ASCS_CLIENT_STREAMENDPOINT_STATE:
            log_info("LEAUDIO_SUBEVENT_ASCS_STREAMENDPOINT_STATE");
            ascs_cid   = leaudio_subevent_ascs_client_streamendpoint_state_get_ascs_cid(packet);
            ase_id     = leaudio_subevent_ascs_client_streamendpoint_state_get_ase_id(packet);
            ase_state  = leaudio_subevent_ascs_client_streamendpoint_state_get_state(packet);
            log_info("ASCS Client: ASE STATE (%s) - ase_id %d, ascs_cid 0x%02x, role %s", ascs_util_ase_state2str(ase_state), ase_id, ascs_cid,
                     (audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SOURCE) ? "SOURCE" : "SINK" );

            // stop streaming if remote is a sink
            if ((ase_state == ASCS_STATE_RELEASING) && audio_stream_control_service_client_get_ase_role(ascs_cid, ase_id) == LE_AUDIO_ROLE_SINK){
                log_info("Releasing on Sink Stream Endpoint -> stop sending on all CIS");
                for (i=0;i<MAX_CHANNELS;i++){
                    cis_con_handles[i] = HCI_CON_HANDLE_INVALID;
                }
            }

            // send done
            if (response_service_id == BTP_SERVICE_ID_LE_AUDIO){
                switch (response_op){
                    case BTP_LE_AUDIO_OP_ASCS_ENABLE:
                        switch (ase_state){
                            case ASCS_STATE_ENABLING:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_START_READY:
                        // moved to control operation complete
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RECEIVER_STOP_READY:
                        switch (ase_state){
                            case ASCS_STATE_IDLE:
                            case ASCS_STATE_QOS_CONFIGURED:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_DISABLE:
                        switch (ase_state){
                            case ASCS_STATE_DISABLING:
                            case ASCS_STATE_QOS_CONFIGURED:
                                btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                                response_op = 0;
                                break;
                            default:
                                break;
                        }
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_RELEASE:
                        btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                        response_op = 0;
                        break;
                    case BTP_LE_AUDIO_OP_ASCS_UPDATE_METADATA:
                        btp_send(BTP_SERVICE_ID_LE_AUDIO, response_op, 0, 0, NULL);
                        response_op = 0;
                        break;
                    default:
                        break;
                }
            }
            break;
#endif
        default:
            break;
    }

    if (btp_bap_higher_layer_handler != NULL){
        (*btp_bap_higher_layer_handler)(packet_type, channel, packet, size);
    }
}

// -- PACS

static void btp_bap_pacs_client_report_pacs(uint8_t *packet, uint16_t size){
    uint16_t pacs_cid = leaudio_subevent_pacs_client_operation_done_get_pacs_cid(packet);
    server_t * server = btp_server_for_pacs_cid(pacs_cid);
    btstack_assert(server != NULL);

    uint8_t buffer[100];
    uint16_t pos = 0;
    buffer[pos++] = server->address_type;
    reverse_bd_addr( server->address, &buffer[pos]);
    pos += 6;
    buffer[pos++] = leaudio_subevent_pacs_client_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? BTP_AUDIO_DIR_SINK : BTP_AUDIO_DIR_SOURCE;
    buffer[pos++] = leaudio_subevent_pacs_client_pack_record_get_coding_format(packet);
    uint16_t supported_frequencies = leaudio_subevent_pacs_client_pack_record_get_supported_sampling_frequencies_mask(packet);
    little_endian_store_16(buffer, pos, supported_frequencies);
    pos += 2;
    buffer[pos++] = leaudio_subevent_pacs_client_pack_record_get_supported_frame_durations_mask(packet);
    uint32_t octets_per_frame = leaudio_subevent_pacs_client_pack_record_get_supported_octets_per_frame_max_num(packet);
    little_endian_store_32(buffer, pos, octets_per_frame);
    pos += 4;
    buffer[pos++] = leaudio_subevent_pacs_client_pack_record_get_supported_audio_channel_counts_mask(packet);

    MESSAGE("BTP_BAP_EV_CODEC_CAP_FOUND %s", bd_addr_to_str(server->address));

    btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_CODEC_CAP_FOUND, 0, pos, buffer);

    MESSAGE("BTP PACS Client: %s PAC Record\n", leaudio_subevent_pacs_client_pack_record_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
}

static void btp_bap_pacs_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    uint32_t audio_allocation_mask;
    uint16_t ascs_cid;
    uint16_t pacs_cid;
    hci_con_handle_t con_handle;
    uint8_t i;
    uint8_t status;
    server_t * server;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_PACS_CLIENT_CONNECTED:
            pacs_cid = leaudio_subevent_pacs_client_connected_get_pacs_cid(packet);
            con_handle = leaudio_subevent_pacs_client_connected_get_con_handle(packet);
            status = leaudio_subevent_pacs_client_connected_get_status(packet);
            server = btp_server_for_pacs_cid(pacs_cid);
            btstack_assert(server != NULL);
            if (status != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("BTP PACS Client %u: connection failed, status 0x%02x", server->server_id, status);
            } else {
                MESSAGE("BTP PACS Client %u: connected", server->server_id);
                btp_bap_discovery_next(server);
            }
            break;
        case LEAUDIO_SUBEVENT_PACS_CLIENT_PACK_RECORD:
            btp_bap_pacs_client_report_pacs(packet, size);
            break;
        case LEAUDIO_SUBEVENT_PACS_CLIENT_PACK_RECORD_DONE:
            pacs_cid = leaudio_subevent_pacs_client_pack_record_done_get_pacs_cid(packet);
            server = btp_server_for_pacs_cid(pacs_cid);
            btstack_assert(server != NULL);
            MESSAGE("BTP PACS Client %u: %s PAC Record DONE\n", server->server_id, leaudio_subevent_pacs_client_pack_record_done_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source");
            break;
        case LEAUDIO_SUBEVENT_PACS_CLIENT_OPERATION_DONE:
            pacs_cid = leaudio_subevent_pacs_client_operation_done_get_pacs_cid(packet);
            server = btp_server_for_pacs_cid(pacs_cid);
            btstack_assert(server != NULL);
            if (leaudio_subevent_pacs_client_operation_done_get_status(packet) == ERROR_CODE_SUCCESS){
                MESSAGE("BTP PACS Client %u: Operation successful", server->server_id);
            } else {
                MESSAGE("BTP PACS Client %u: Operation failed with status 0x%02X", server->server_id, leaudio_subevent_pacs_client_operation_done_get_status(packet));
            }
            btp_bap_discovery_next(server);
            break;
        case LEAUDIO_SUBEVENT_PACS_CLIENT_DISCONNECTED:
            MESSAGE("PACS Client: disconnected\n");
            break;
#if 0
        case LEAUDIO_SUBEVENT_PACS_CLIENT_AUDIO_LOCATIONS:
            audio_allocation_mask = leaudio_subevent_pacs_client_audio_locations_get_audio_locations_mask(packet);
            MESSAGE("PACS Client: %s Audio Locations 0x%04x",
                    leaudio_subevent_pacs_client_audio_locations_get_le_audio_role(packet) == LE_AUDIO_ROLE_SINK ? "Sink" : "Source", audio_allocation_mask);
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_AVAILABLE_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Available Audio Contexts");
            MESSAGE("      Sink   0x%02X", leaudio_subevent_pacs_client_available_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X", leaudio_subevent_pacs_client_available_audio_contexts_get_source_mask(packet));
            break;

        case LEAUDIO_SUBEVENT_PACS_CLIENT_SUPPORTED_AUDIO_CONTEXTS:
            MESSAGE("PACS Client: Supported Audio Contexts\n");
            MESSAGE("      Sink   0x%02X\n", leaudio_subevent_pacs_client_supported_audio_contexts_get_sink_mask(packet));
            MESSAGE("      Source 0x%02X\n", leaudio_subevent_pacs_client_supported_audio_contexts_get_source_mask(packet));
            break;
#endif
        default:
            break;
    }

    if (btp_bap_higher_layer_handler != NULL){
        (*btp_bap_higher_layer_handler)(packet_type, channel, packet, size);
    }
}

// TBS
static void tbs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
#if 0
        case LEAUDIO_SUBEVENT_TBS_SERVER_CALL_CONTROL_POINT_NOTIFICATION_TASK: {
//            hci_con_handle_t con_handle = little_endian_read_16(packet, 3);
            uint16_t bearer_id = little_endian_read_16(packet, 5);
            uint8_t opcode = packet[7];

            telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
            my_bearer_t *bearer = tbs_to_my_bearer( tbs_bearer );
            uint8_t call_id = packet[8];
            uint8_t *data = &packet[8];
            uint16_t data_size = size - 8;
            tbs_call_data_t *tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
            my_call_data_t *call = tbs_call_to_my_call( tbs_call );
#ifdef DEBUG
            for( int i=0; i<size; ++i ) {
                printf("%#04x, ", packet[i]);
            }
            printf("\n");
            printf("opcode: %d\ncall_id: %d\n", opcode, call_id);
            printf("bearer_id: %d\n", bearer_id);
#endif
            switch( opcode ) {
                case TBS_CONTROL_POINT_OPCODE_ACCEPT: {
                    printf("%s( %d )\n", opcode_to_string[opcode], call_id);
                    tbs_private_public_event_t sig_accept = { .data.sig=ACCEPT_SIG, .private = false };
                    tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };
                    if( call == NULL ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                        break;
                    }
                    btstack_hsm_dispatch( &call->super, &sig_accept.data );

                    // for all other call's of this bearer
                    for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                        if(( calls[i].id > 0 ) && (calls[i].id != call_id) && (calls[i].bearer == bearer)) {
                            tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                            switch( state ) {
                                case TBS_CALL_STATE_ACTIVE:
                                    btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                                    break;
                                case TBS_CALL_STATE_REMOTELY_HELD:
                                    btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
                }
                case TBS_CONTROL_POINT_OPCODE_JOIN: {
                    printf("%s( ", opcode_to_string[opcode]);
                    for(int i=0; i<data_size; ++i) {
                        printf("%d ", data[i]);
                    }
                    printf(")\n");
                    if( !join_operation ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, 0, opcode, TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE);
                        break;
                    }
                    tbs_private_public_event_t sig_retrive = { .data.sig=LOCAL_RETRIEVE_SIG, .private = true };
                    tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };

                    if( data_size < 2 ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, 0, opcode, TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE);
                        break;
                    }
                    // if any call in the list is incoming, or invalid reject operation
                    for( int i=0; i<data_size; ++i ) {
                        call_id = data[i];
                        tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
                        if( tbs_call == NULL ) {
                            telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                            return;
                        }
                        tbs_call_state_t state = telephone_bearer_service_server_get_call_state(tbs_call);
                        switch( state ) {
                            case TBS_CALL_STATE_INCOMMING:
                                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE);
                                return;
                            default:
                                break;
                        }
                    }

                    call_id = data[0];
                    tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
                    tbs_call_state_t state = telephone_bearer_service_server_get_call_state(tbs_call);
                    telephone_bearer_service_server_set_call_state( bearer_id, call_id, state );

                    // retrieve locally held calls
                    for( int i=0; i<data_size; ++i ) {
                        tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, data[i] );
                        call = tbs_call_to_my_call( tbs_call );
                        state = telephone_bearer_service_server_get_call_state(tbs_call);
                        switch( state ) {
                            case TBS_CALL_STATE_LOCALLY_HELD:
                            case TBS_CALL_STATE_LOCALLY_AND_REMOTELY_HELD:
                                btstack_hsm_dispatch( &call->super, &sig_retrive.data );
                                break;
                            default:
                                break;
                        }
                    }
                    // for all other call's of this bearer
                    for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                        call = &calls[i];
                        // skip unused call
                        if( call->id == 0 ) {
                            continue;
                        }
                        // skip call's not belonging to us
                        if( call->bearer != bearer ) {
                            continue;
                        }
                        // skip calls in the join list
                        int j;
                        for( j=0; j<data_size; ++j ) {
                            if( data[i] == call->id ) {
                                break;
                            }
                        }
                        if(j < data_size ) {
                            continue;
                        }

                        tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                        switch( state ) {
                            case TBS_CALL_STATE_ACTIVE:
                                btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                                break;
                            default:
                                break;
                        }
                    }

                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
                    break;
                }
                case TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD: {
                    printf("%s( %d )\n", opcode_to_string[opcode], call_id);
                    tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = false };
                    if( call == NULL ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                        break;
                    }
                    btstack_hsm_dispatch( &call->super, &sig_local_hold.data );
                    break;
                }
                case TBS_CONTROL_POINT_OPCODE_LOCAL_RETRIEVE: {
                    printf("%s( %d )\n", opcode_to_string[opcode], call_id);

                    tbs_private_public_event_t sig_retrive = { .data.sig=LOCAL_RETRIEVE_SIG, .private = false };
                    tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };
                    if( call == NULL ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                        break;
                    }
                    btstack_hsm_dispatch( &call->super, &sig_retrive.data );

                    // for all other call's of this bearer
                    for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                        if(( calls[i].id > 0 ) && (calls[i].id != call_id) && (calls[i].bearer == bearer)) {
                            tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                            switch( state ) {
                                case TBS_CALL_STATE_ACTIVE:
                                    btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                                    break;
                                case TBS_CALL_STATE_REMOTELY_HELD:
                                    btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
                }
                case TBS_CONTROL_POINT_OPCODE_ORIGINATE: {
                    printf("%s( %s )\n", opcode_to_string[opcode], data);
                    my_call_data_t *call = find_unused_call();
                    if( call == NULL ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_LACK_OF_RESOURCES);
                        break;
                    }
                    btstack_assert( bearer != NULL );

                    tbs_state_constructor( call );
                    call->bearer = bearer;
                    tbs_call_event_t e = {
                            .data.sig = ORIGINATE_SIG,
                            .caller_id = "5551234",
                            .friendly_name = "all mighty",
                            .target_uri = (char *)data,
                    };
                    telephone_bearer_service_server_register_call( bearer_id, &call->data, &call->id );
                    call_id = call->id;
                    btstack_hsm_init( &call->super, &e.data);
                    break;
                }
                case TBS_CONTROL_POINT_OPCODE_TERMINATE: {
                    printf("%s( %d )\n", opcode_to_string[opcode], call_id);
                    btstack_hsm_event_t e = { .sig=TERMINATE_SIG };
                    if( call == NULL ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                        break;
                    }
                    btstack_hsm_dispatch( &call->super, &e );
                    break;
                }
                default:
                    printf("unknown opcode\n");
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_OPCODE_NOT_SUPPORTED);
                    break;
            }
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_SERVER_CALL_DEREGISTER_DONE: {
//            hci_con_handle_t con_handle = little_endian_read_16(packet, 3);
//            uint16_t bearer_id = little_endian_read_16(packet, 5);
            uint8_t call_id = packet[7];

            my_call_data_t *call = find_call_by_id(call_id);
            btstack_assert( call != NULL );
            call->id = 0;
            printf("de-register call_id - %d\n", call_id);
            break;
        }
#endif
        default:
            break;
    }
}

// BTP

void btp_bap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_BAP;
    response_op = opcode;
    uint8_t ase_id;
    uint8_t i;
    server_t * server;
    switch (opcode) {
        case BTP_BAP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_BAP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_BAP_DISCOVER:
            if (controller_index == 0) {
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                MESSAGE("BTP_BAP_DISCOVER %s", bd_addr_to_str(address));
                server = btp_server_for_address(addr_type, address);
                server->bap_state = (uint8_t) BTP_BAP_STATE_IDLE;
                btp_bap_discovery_next(server);

                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_BAP_SEND:
            if (controller_index == 0) {
                uint16_t pos = 0;
                uint8_t addr_type = data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                uint8_t ase_id = data[pos++];
                uint8_t payload_len = data[pos++];
                const uint8_t * const payload_data = &data[pos];
                // confirm sent
                if (response_op != 0){
                    uint8_t num_bytes_sent = payload_len;
                    btp_send(BTP_SERVICE_ID_BAP, response_op, 0, 1, &num_bytes_sent);
                    response_op = 0;
                }
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_SETUP:
            if (controller_index == 0){
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_SETUP");
                uint16_t pos = 0;
                uint8_t streams_per_subgroup = data[pos++];
                uint8_t subgroups = data[pos++];
                uint32_t sdu_interval = little_endian_read_24(data, pos);
                pos += 3;
                uint8_t framing = data[pos++];
                uint16_t max_sdu = little_endian_read_16(data, pos);
                pos += 2;
                uint8_t retransmission_num = data[pos++];
                uint16_t max_transport_latency = little_endian_read_16(data, pos);
                pos += 2;
                uint32_t presentation_delay = little_endian_read_24(data, pos);
                pos += 3;
                // coding format(1), vendor id (2), codec id (2)
                const uint8_t * const codec_id = &data[pos];
                pos += 5;
                uint8_t ltv_len = data[pos++];
                const uint8_t * const ltv_data = &data[pos];

                // setup BIG params
                btp_bap_big_params.num_bis = streams_per_subgroup;
                btp_bap_big_params.framing = framing;
                btp_bap_big_params.max_sdu = max_sdu;
                btp_bap_big_params.rtn = retransmission_num;
                btp_bap_big_params.max_transport_latency_ms = max_transport_latency;
                btp_bap_big_params.sdu_interval_us = sdu_interval;

                // setup BASE
                // setup base
                uint8_t subgroup_metadata[] = {
                        0x03, 0x02, 0x04, 0x00, // Metadata[i]
                };
                btp_bap_setup_periodic_advertising_data(subgroups, presentation_delay, codec_id, ltv_len,
                                                        ltv_data, sizeof(subgroup_metadata), subgroup_metadata, 0,
                                                        NULL);

                // parse LTV and configure codec
                uint32_t sampling_frequency_hz = 0;
                pos = 0;
                while (pos < ltv_len){
                    uint8_t ltv_entry_len = ltv_data[pos++];
                    uint8_t codec_config_type = ltv_data[pos];
                    if (codec_config_type == LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY){
                        sampling_frequency_hz = le_audio_get_sampling_frequency_hz(ltv_data[pos]);
                    }
                    pos += ltv_entry_len;
                }
                btstack_lc3_frame_duration_t frame_duration =
                        btp_bap_big_params.sdu_interval_us == 7500 ? BTSTACK_LC3_FRAME_DURATION_7500US : BTSTACK_LC3_FRAME_DURATION_10000US;
                le_audio_demo_util_source_configure(btp_bap_big_params.num_bis, 1, sampling_frequency_hz, frame_duration, btp_bap_big_params.max_sdu);
                MESSAGE("LC3 Config: streams %u, channels per stream %u, sampling frequency %u, frame duration %u, max sdu %u", btp_bap_big_params.num_bis, 1, sampling_frequency_hz, btp_bap_big_params.sdu_interval_us, btp_bap_big_params.max_sdu);
                btstack_assert(sampling_frequency_hz != 0);

                uint8_t result[7];
                little_endian_store_32(result, 0, btp_gap_current_settings());
                uint32_t broadcast_id = BROADCAST_ID;
                little_endian_store_24(result, 4, broadcast_id);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, sizeof(result), result);
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_RELEASE:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_RELEASE");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_BAP_BROADCAST_ADV_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ADV_START");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                btp_bap_start_advertising(broadcast_id);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_ADV_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ADV_STOP");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                btp_bap_stop_advertising();
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_START");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                btp_bap_setup_big();
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_STOP");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                btp_bap_release_big();
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SINK_SETUP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SINK_SETUP");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SINK_RELEASE:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SINK_RELEASE");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SCAN_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SCAN_START");
                scan_for_broadcast_audio_announcement = true;
                scan_for_scan_delegator = false;
                start_scanning();
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SCAN_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SCAN_STOP");
                stop_scanning();
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SINK_SYNC:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SINK_SYNC");
                uint16_t pos = 0;
                uint8_t addr_type = data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                uint32_t broadcast_id = little_endian_read_24(data, pos);
                pos += 3;
                uint8_t advertiser_sid = data[pos++];
                uint16_t skip = little_endian_read_16(data, pos);
                pos += 2;
                uint16_t sync_timeout = little_endian_read_16(data, pos);
                pos += 2;
                uint8_t past_available = data[pos++];
                btp_bap_start_periodic_sync(addr_type, address, broadcast_id, advertiser_sid, skip, sync_timeout);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SINK_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SINK_STOP");
                gap_periodic_advertising_terminate_sync(sync_handle);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SINK_BIS_SYNC:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SINK_BIS_SYNC");
                uint16_t pos = 0;
                uint8_t addr_type = data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                uint32_t broadcast_id = little_endian_read_24(data, pos);
                pos += 3;
                uint32_t requested_bis_sync = little_endian_read_32(data, pos);
                enter_create_big_sync(requested_bis_sync);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_DISCOVER_SCAN_DELEGATORS:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_DISCOVER_SCAN_DELEGATOR");
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                MESSAGE("BTP_BAP_DISCOVER_SCAN_DELEGATORS %s", bd_addr_to_str(address));
                server = btp_server_for_address(addr_type, address);

                broadcast_audio_scan_service_client_connect(&server->bass_connection,
                                                            server->bass_sources,
                                                            BASS_CLIENT_NUM_SOURCES,
                                                            server->acl_con_handle,
                                                            &server->bass_cid);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_ASSISTANT_SCAN_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ASSISTANT_SCAN_START");
                scan_for_broadcast_audio_announcement = false;
                scan_for_scan_delegator = true;
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_ASSISTANT_SCAN_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ASSISTANT_SCAN_STOP");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_ADD_BROADCAST_SRC:
            if (controller_index == 0) {
                // parse command into bass_source_data

                uint16_t pos = 0;
                bd_addr_type_t addr_type = (bd_addr_type_t) data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                server = btp_server_for_address(addr_type, address);

                MESSAGE("BTP_BAP_ADD_BROADCAST_SRC on %s", bd_addr_to_str(server->address));
                memset(&bass_source_data, 0, sizeof(bass_source_data));
                bass_source_data.address_type = data[pos++];
                reverse_bd_addr(&data[pos], bass_source_data.address);
                pos += 6;
                bass_source_data.adv_sid = data[pos++];
                bass_source_data.broadcast_id = little_endian_read_24(data, pos);
                pos += 3;
                bass_source_data.pa_sync = data[pos++];
                bass_source_data.pa_interval = little_endian_read_16(data, pos);
                pos += 2;
                bass_source_data.subgroups_num = data[pos++];
                uint8_t subgroup;
                for (subgroup=0;subgroup < bass_source_data.subgroups_num;subgroup++){
                    bass_source_data.subgroups[subgroup].bis_sync = little_endian_read_32(data, pos);
                    pos += 4;
                    bass_source_data.subgroups[subgroup].metadata_length = data[pos++];
                    // skip metadata
                    pos += bass_source_data.subgroups[subgroup].metadata_length;
                }

                uint8_t status = broadcast_audio_scan_service_client_add_source(server->bass_cid, &bass_source_data);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                break;
            }
            break;
        case BTP_BAP_REMOVE_BROADCAST_SRC:
            if (controller_index == 0) {

                uint16_t pos = 0;
                bd_addr_type_t addr_type = (bd_addr_type_t) data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                server = btp_server_for_address(addr_type, address);

                MESSAGE("BTP_BAP_REMOVE_BROADCAST_SRC on %s", bd_addr_to_str(server->address));

                uint8_t source_id = data[pos];
                uint8_t status = broadcast_audio_scan_service_client_remove_source(server->bass_cid, source_id);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                break;
            }
            break;
        case BTP_BAP_MODIFY_BROADCAST_SRC:
            if (controller_index == 0) {
                // parse command into bass_source_data

                /*
		            Address_Type (1 octet)
					Address  (6 octets)
					Source ID (1 octet)
					PA_Sync (1 octet)
					PA_Interval (2 octets)
					Num_Subgroups (1 octet
                */
                uint16_t pos = 0;
                bd_addr_type_t addr_type = (bd_addr_type_t) data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                server = btp_server_for_address(addr_type, address);

                MESSAGE("BTP_BAP_MODIFY_BROADCAST_SRC on %s", bd_addr_to_str(server->address));

                uint8_t source_id = data[pos++];

                memset(&bass_source_data, 0, sizeof(bass_source_data));
                bass_source_data.pa_sync = data[pos++];
                bass_source_data.pa_interval = little_endian_read_16(data, pos);
                pos += 2;
                bass_source_data.subgroups_num = data[pos++];
                uint8_t subgroup;
                for (subgroup=0;subgroup < bass_source_data.subgroups_num;subgroup++){
                    bass_source_data.subgroups[subgroup].bis_sync = little_endian_read_32(data, pos);
                    pos += 4;
                    bass_source_data.subgroups[subgroup].metadata_length = data[pos++];
                    // skip metadata
                    pos += bass_source_data.subgroups[subgroup].metadata_length;
                }

                uint8_t status = broadcast_audio_scan_service_client_modify_source(server->bass_cid, source_id,  &bass_source_data);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                break;
            }
            break;
        case BTP_BAP_SET_BROADCAST_CODE:
            if (controller_index == 0) {
                /*
                    Address_Type (1 octet)
                    Address  (6 octets)
                    Source ID (1 octet)
                    Broadcast Code (16 octets)
                 */
                uint16_t pos = 0;
                bd_addr_type_t addr_type = (bd_addr_type_t) data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                server = btp_server_for_address(addr_type, address);
                uint8_t source_id = data[pos++];
                MESSAGE("BTP_BAP_SET_BROADCAST_CODE");
                uint8_t status = broadcast_audio_scan_service_client_set_broadcast_code(server->bass_cid, source_id, &data[pos]);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_SEND_PAST:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_SEND_PAST");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        default:
            break;
    }
}

void btp_bap_init(void){
    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup audio processing
    le_audio_demo_util_source_init();

    // register for ISO Packet
    hci_register_iso_packet_handler(&iso_packet_handler);

    // BIS
    uint8_t i;
    for (i=0;i<MAX_NUM_BIS;i++){
        bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
    }

    adv_handle = 0xff;

    // -- Clients --

    // BASS Client
    broadcast_audio_scan_service_client_init(&bass_packet_handler);

    // PACS Client
    published_audio_capabilities_service_client_init(&btp_bap_pacs_client_event_handler);

    // ASCS Client
    audio_stream_control_service_client_init(&btp_bap_ascs_client_event_handler);

    // AICS Client, used by VCS Client and MICS
    audio_input_control_service_client_init();

    // VOCS Client, used by VCS Client
    volume_offset_control_service_client_init();

    // VCP = VOCS Client
    volume_control_service_client_init();

    // MICP = MICS Client
    microphone_control_service_client_init();

    // -- Servers --

    // GTBS Server
    telephone_bearer_service_server_init();
    (void) telephone_bearer_service_server_register_generic_bearer(
            &tbs_bearers[tbs_bearer_index].bearer,
            &tbs_server_packet_handler,
            TBS_OPCODE_MASK_LOCAL_HOLD | TBS_OPCODE_MASK_JOIN,
            &tbs_bearers[tbs_bearer_index].id);


    // VCS Server without AICS or VOCS
    volume_control_service_server_init(128, VCS_MUTE_OFF, 0, NULL, 0, NULL);
}

void btp_bap_register_higher_layer(btstack_packet_handler_t handler){
    btp_bap_higher_layer_handler = handler;
}

void btp_bap_bass_discover(server_t * server){
    broadcast_audio_scan_service_client_connect(&server->bass_connection,
                                                server->bass_sources,
                                                BASS_CLIENT_NUM_SOURCES,
                                                server->acl_con_handle,
                                                &server->bass_cid);
}

void btp_vcp_init(void){
}

static void btp_bap_vcp_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    server_t *server;
    uint16_t vcs_cid;
    uint8_t status;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_VCS_CLIENT_CONNECTED:
            vcs_cid = leaudio_subevent_vcs_client_connected_get_vcs_cid(packet);
            status = leaudio_subevent_vcs_client_connected_get_att_status(packet);
            server = btp_server_for_vcs_cid(vcs_cid);
            btstack_assert(server != NULL);
            if (status != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("BTP VCS Client %u: connection failed, status 0x%02x", server->server_id, status);
            } else {
                MESSAGE("BTP VCS Client %u: connected, VCS Client cid %02x", server->server_id, server->vcs_cid);
                struct btp_vcp_discovered_ev event;
                memset(&event, 0, sizeof(event));
                uint16_t offset = 0;
                event.address.address_type = server->address_type;
                reverse_bd_addr(server->address,event.address.address);
                event.att_status = 0;
                btp_send(BTP_SERVICE_ID_VCP, BTP_VCP_DISCOVERED_EV, 0, sizeof(event), (uint8_t *) &event);
            }
            break;
        case LEAUDIO_SUBEVENT_VCS_CLIENT_DISCONNECTED:
            vcs_cid = leaudio_subevent_vcs_client_disconnected_get_vcs_cid(packet);
            server = btp_server_for_vcs_cid(vcs_cid);
            MESSAGE("BTP VCS Client %u: disconnected, cid %02x", server->server_id, vcs_cid);
            btstack_assert(server != NULL);
            server->vcs_cid = 0;
            break;
        default:
            break;
    }
}

void btp_vcp_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_VCP;
    response_op = opcode;
    server_t * server;
    switch (opcode) {
        case BTP_VCP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_VCP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_VCP_VOL_CTLR_DISCOVER:
            if (controller_index == 0) {
                /**
                    bt_addr_le_t address;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                server = btp_server_for_address(addr_type, address);
                uint8_t status = volume_control_service_client_connect(server->acl_con_handle,
                                                                       &btp_bap_vcp_event_handler, &server->vcs_connection,
                                                                       NULL, 0,
                                                                       NULL, 0,
                                                                       &server->vcs_cid);
                MESSAGE("BTP_VCP_VOL_CTLR_DISCOVER %s, VCS Client cid %02x", bd_addr_to_str(address), server->vcs_cid);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_VCP_VOL_CTLR_SET_VOL:
            if (controller_index == 0){
                /**
                    bt_addr_le_t address;
                    uint8_t volume;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                uint8_t volume = data[7];
                MESSAGE("BTP_VCP_VOL_CTLR_SET_VOL %s -> %u", bd_addr_to_str(address), volume);
                server = btp_server_for_address(addr_type, address);
                uint8_t status = volume_control_service_client_set_absolute_volume(server->vcs_cid, volume);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_VCP_VOL_CTLR_MUTE:
            if (controller_index == 0){
                /**
                    bt_addr_le_t address;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                MESSAGE("BTP_VCP_VOL_CTLR_MUTE %s", bd_addr_to_str(address));
                server = btp_server_for_address(addr_type, address);
                uint8_t status = volume_control_service_client_mute(server->vcs_cid);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_VCP_VOL_CTLR_UNMUTE:
            if (controller_index == 0){
                /**
                    bt_addr_le_t address;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                MESSAGE("BTP_VCP_VOL_CTLR_UNMUTE %s", bd_addr_to_str(address));
                server = btp_server_for_address(addr_type, address);
                uint8_t status = volume_control_service_client_unmute(server->vcs_cid);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            MESSAGE("BTP VCP Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
};

void btp_micp_init(void){
}

static void btp_bap_micp_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    server_t *server;
    uint16_t mics_cid;
    uint8_t status;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_MICS_CLIENT_CONNECTED:
            mics_cid = leaudio_subevent_mics_client_connected_get_mics_cid(packet);
            status = leaudio_subevent_mics_client_connected_get_att_status(packet);
            server = btp_server_for_mics_cid(mics_cid);
            btstack_assert(server != NULL);
            if (status != ERROR_CODE_SUCCESS){
                // TODO send error response
                MESSAGE("BTP MICP %u: connection failed, status 0x%02x", server->server_id, status);
            } else {
                MESSAGE("BTP MICP %u: connected, MICS Client cid %02x", server->server_id, server->mics_cid);
                struct btp_micp_discovered_ev event;
                memset(&event, 0, sizeof(event));
                uint16_t offset = 0;
                event.address.address_type = server->address_type;
                reverse_bd_addr(server->address,event.address.address);
                event.att_status = 0;
                btp_send(BTP_SERVICE_ID_MICP, BTP_MICP_DISCOVERED_EV, 0, sizeof(event), (uint8_t *) &event);
            }
            break;
        case LEAUDIO_SUBEVENT_MICS_CLIENT_DISCONNECTED:
            mics_cid = leaudio_subevent_mics_client_disconnected_get_mics_cid(packet);
            server = btp_server_for_mics_cid(mics_cid);
            MESSAGE("BTP MICP Client %u: disconnected, cid %02x", server->server_id, mics_cid);
            btstack_assert(server != NULL);
            server->mics_cid = 0;
            break;
        default:
            break;
    }
}

void btp_micp_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_MICP;
    response_op = opcode;
    server_t * server;
    switch (opcode) {
        case BTP_MICP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_MICP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_MICP_CTLR_DISCOVER:
            if (controller_index == 0) {
                /**
                    bt_addr_le_t address;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                server = btp_server_for_address(addr_type, address);
                uint8_t status = microphone_control_service_client_connect(server->acl_con_handle,
                                                                       &btp_bap_micp_event_handler,
                                                                       &server->mics_connection,
                                                                       server->mics_characteristics, MICROPHONE_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS,
                                                                       server->aics_connections, 2,
                                                                       server->aics_characteristics, 2 * AUDIO_INPUT_CONTROL_SERVICE_NUM_CHARACTERISTICS,
                                                                       &server->mics_cid);
                MESSAGE("BTP_MICP_CTLR_DISCOVER %s, MICS Client cid %02x", bd_addr_to_str(address), server->mics_cid);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_MICP_CTLR_MUTE:
            if (controller_index == 0){
                /**
                    bt_addr_le_t address;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                MESSAGE("BTP_MICP_CTLR_MUTE %s", bd_addr_to_str(address));
                server = btp_server_for_address(addr_type, address);
                MESSAGE("BTP_MICP_CTLR_MUTE server %p", server);
                uint8_t status = microphone_control_service_client_mute(server->mics_cid);
                MESSAGE("BTP_MICP_CTLR_MUTE status 0x%02x", status);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            MESSAGE("BTP MICP Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
};

void btp_aics_init(void){
}

static void btp_bap_aics_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        default:
            break;
    }
}

void btp_aics_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_AICS;
    response_op = opcode;
    server_t * server;
    switch (opcode) {
        case BTP_AICS_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_AICS_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_AICS_SET_GAIN:
            if (controller_index == 0) {
                /**
                    bt_addr_le_t address;
                 */
                // get server struct
                bd_addr_type_t addr_type = (bd_addr_type_t) data[0];
                bd_addr_t address;
                reverse_bd_addr(&data[1], address);
                server = btp_server_for_address(addr_type, address);
                int8_t gain = (int8_t) data[7];
                uint8_t status = microphone_control_service_client_write_gain_setting(server->mics_cid, 0, gain);
                MESSAGE("BTP_AICS_SET_GAIN %s, MICS Client cid %02x", bd_addr_to_str(address), server->mics_cid);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;

        default:
            MESSAGE("BTP MICP Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
};

void btp_tmap_init(void){
}

static void btp_bap_tmap_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        default:
            break;
    }
}

void btp_tmap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_TMAP;
    response_op = opcode;
    server_t * server;
    switch (opcode) {
        case BTP_TMAP_READ_SUPPORTED_COMMANDS:
        MESSAGE("BTP_TMAP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        default:
            MESSAGE("BTP TMAP Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
};


void btp_tbs_init(void){
}

void btp_tbs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_TBS;
    response_op = opcode;
    server_t * server;
    switch (opcode) {
        case BTP_TBS_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_TBS_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_TBS_REMOTE_INCOMING:
            /**
             * 	uint8_t index;
                uint8_t recv_len;
                uint8_t caller_len;
                uint8_t fn_len;
                uint8_t data_len;
                uint8_t data[0];
             */
            if (controller_index == 0) {
                uint16_t offset = 0;
                uint8_t index = data[offset++];
                uint8_t recv_len = data[offset++];
                uint8_t caller_len = data[offset++];
                uint8_t fn_len = data[offset++];
                uint8_t data_len = data[offset++];

                char receiver_uri[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
                memcpy(receiver_uri, &data[offset], recv_len);
                receiver_uri[recv_len] = 0;
                offset += recv_len;

                char caller_uri[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
                memcpy(caller_uri, &data[offset], caller_len);
                caller_uri[caller_len] = 0;

                char friendly_name[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
                memcpy(friendly_name, &data[offset], caller_len);
                friendly_name[caller_len] = 0;

                uint8_t bearer_id = tbs_bearers[tbs_bearer_index].id;
                btp_send(response_service_id, opcode, controller_index, 0, NULL);

                MESSAGE("BTP_TBS_REMOTE_INCOMING, index %u, bearer id %u", index, bearer_id);
                MESSAGE("BTP_TBS_REMOTE_INCOMING, receiver %s", receiver_uri);
                MESSAGE("BTP_TBS_REMOTE_INCOMING, caller %s", caller_uri);
                MESSAGE("BTP_TBS_REMOTE_INCOMING, friendly name %s", friendly_name);

                uint8_t status;
                status = telephone_bearer_service_server_register_call(bearer_id, &calls[index], &call_ids[index]);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                uint16_t call_id = call_ids[index];
                status = telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_INCOMMING);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                // TODO: might need to get flipped
                status = telephone_bearer_service_server_call_uri(bearer_id, call_id, caller_uri);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                status = telephone_bearer_service_server_incoming_call_target_bearer_uri(bearer_id, call_id, receiver_uri);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                status = telephone_bearer_service_server_call_friendly_name(bearer_id, call_id, friendly_name);
                btstack_assert(status == ERROR_CODE_SUCCESS);
            }
            break;
        case BTP_TBS_TERMINATE:
            /**
             * 	uint8_t index;
             */
            if (controller_index == 0) {
                uint16_t offset = 0;
                uint8_t index = data[offset++];
                uint8_t bearer_id = tbs_bearers[tbs_bearer_index].id;
                btp_send(response_service_id, opcode, controller_index, 0, NULL);

                uint16_t call_id = call_ids[index];
                MESSAGE("BTP_TBS_HANGUP, index %u, bearer id %u, call_id %u", index, bearer_id, call_id);
                uint8_t status;
                status = telephone_bearer_service_server_termination_reason(bearer_id, call_id, TBS_CALL_TERMINATION_REASON_CLIENT_ENDED_CALL);
                btstack_assert(status == ERROR_CODE_SUCCESS);
                telephone_bearer_service_server_deregister_call(bearer_id, call_id);
                btstack_assert(status == ERROR_CODE_SUCCESS);
            }
            break;
        default:
            MESSAGE("BTP TBS Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
};
