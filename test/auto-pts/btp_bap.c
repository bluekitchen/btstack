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

#define BTSTACK_FILE__ "btp_btp.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btpclient.h"
#include "btp.h"
#include "btp_bap.h"
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"
#include "le-audio/le_audio_base_parser.h"

#define  MAX_NUM_BIS 2
#define  MAX_CHANNELS 2
#define MAX_LC3_FRAME_BYTES 155

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };

static btstack_packet_callback_registration_t hci_event_callback_registration;

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
static le_audio_big_params_t big_params;

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

// BASS
#define BASS_CLIENT_NUM_SOURCES 1
static bd_addr_type_t bass_addr_type;
static bd_addr_t      bass_address;
static bass_client_connection_t bass_connection;
static bass_client_source_t bass_sources[BASS_CLIENT_NUM_SOURCES];
static bass_source_data_t bass_source_data;
static uint16_t bass_cid;
static uint8_t  bass_source_id;

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

static const uint8_t extended_adv_data[] = {
        // 16 bit service data, ORG_BLUETOOTH_SERVICE_BASIC_AUDIO_ANNOUNCEMENT_SERVICE, Broadcast ID
        6, BLUETOOTH_DATA_TYPE_SERVICE_DATA_16_BIT_UUID, 0x52, 0x18,
        BROADCAST_ID >> 16,
        (BROADCAST_ID >> 8) & 0xff,
        BROADCAST_ID & 0xff,
        // name
        7, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'P', 'T', 'S', '-', 'x', 'x',
        7, BLUETOOTH_DATA_TYPE_BROADCAST_NAME ,     'P', 'T', 'S', '-', 'x', 'x',
};

static void btp_bap_send_response_if_pending(void){
    if (response_op != 0){
        btp_send(BTP_SERVICE_ID_BAP, response_op, 0, 0, NULL);
        response_op = 0;
    }
}

static void btp_trigger_iso(btstack_timer_source_t * ts){
    hci_request_bis_can_send_now_events(big_params.big_handle);
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
    MESSAGE("Broadcast Sink Synced");
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
                    if (bis_index == big_params.num_bis){
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
                            for (i=0;i<big_params.num_bis;i++){
                                bis_con_handles[i] = gap_subevent_big_created_get_bis_con_handles(packet, i);
                            }
                            btp_bap_send_response_if_pending();
                            hci_request_bis_can_send_now_events(big_params.big_handle);
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
    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) return;

    const bass_source_data_t * source_data;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_BASS_CLIENT_CONNECTED:
            if (gattservice_subevent_bass_client_connected_get_status(packet) != ERROR_CODE_SUCCESS){
                MESSAGE("BASS client connection failed, cid 0x%02x, con_handle 0x%02x, status 0x%02x",
                       bass_cid, remote_handle,
                       gattservice_subevent_bass_client_connected_get_status(packet));
                return;
            }
            MESSAGE("BASS client connected, cid 0x%02x\n", bass_cid);

            // inform btp
            uint8_t data[7];
            data[0] = bass_addr_type;
            reverse_bd_addr(bass_address, &data[1]);
            btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_SCAN_DELEGATOR_FOUND, 0, sizeof(data), data);
            break;
        case GATTSERVICE_SUBEVENT_BASS_CLIENT_SOURCE_OPERATION_COMPLETE:
            if (gattservice_subevent_bass_client_source_operation_complete_get_status(packet) != ERROR_CODE_SUCCESS){
                MESSAGE("BASS client source operation failed, status 0x%02x", gattservice_subevent_bass_client_source_operation_complete_get_status(packet));
                break;
            }

            if ( gattservice_subevent_bass_client_source_operation_complete_get_opcode(packet) == (uint8_t)BASS_OPCODE_ADD_SOURCE ){
                // TODO: set state to 'wait for source_id"
                printf("BASS client add source operation completed, wait for source_id\n");
            }
            break;
        case GATTSERVICE_SUBEVENT_BASS_CLIENT_NOTIFICATION_COMPLETE:
            // store source_id
            bass_source_id = gattservice_subevent_bass_client_notification_complete_get_source_id(packet);
            MESSAGE("BASS client notification, source_id = 0x%02x", bass_source_id);
            source_data = broadcast_audio_scan_service_client_get_source_data(bass_cid, bass_source_id);
            btstack_assert(source_data != NULL);

            switch (source_data->pa_sync_state){
                case LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST:
                    // start pa sync transfer
                    printf("LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST -> Start PAST\n");
                    // TODO: unclear why this needs to be shifted for PAST with TS to get test green
                    uint16_t service_data = bass_source_id << 8;
                    gap_periodic_advertising_sync_transfer_send(remote_handle, service_data, sync_handle);
                    break;
                default:
                    break;
            }
            break;
        case GATTSERVICE_SUBEVENT_BASS_CLIENT_DISCONNECTED:
            break;
        default:
            break;
    }
}

static void expect_status_no_error(uint8_t status){
    if (status != ERROR_CODE_SUCCESS){
        MESSAGE("STATUS = 0x%02x -> ABORT", status);
    }
    btstack_assert(status == ERROR_CODE_SUCCESS);
}

static void start_advertising() {
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

static void stop_advertising(){
    gap_periodic_advertising_stop(adv_handle);
    gap_extended_advertising_stop(adv_handle);
}

static void setup_big(void){
    // Create BIG. From BTP_BAP_BROADCAST_SOURCE_SETUP
    // - num_bis
    // - max sdu
    // - max_transport_latency_ms
    // - rtn
    // - sdu interval us
    // - framing
    big_params.big_handle = 0;
    big_params.advertising_handle = adv_handle;
    big_params.phy = 2;
    big_params.packing = 0;
    big_params.encryption = encryption;
    if (encryption) {
        memcpy(big_params.broadcast_code, &broadcast_code[0], 16);
    } else {
        memset(big_params.broadcast_code, 0, 16);
    }
    gap_big_create(&big_storage, &big_params);
}

static void release_big(void){
    uint8_t status = gap_big_terminate(big_params.big_handle);
    MESSAGE("gap_big_terminate, status 0x%x", status);
}

static void setup_periodic_adv(uint8_t num_bis, uint32_t presentation_delay_us, const uint8_t * const codec_id, uint8_t codec_ltv_len, const uint8_t * const codec_ltv_bytes ) {

    // setup base
    uint8_t subgroup_metadata[] = {
            0x03, 0x02, 0x04, 0x00, // Metadata[i]
    };

    le_audio_base_builder_t builder;
    le_audio_base_builder_init(&builder, period_adv_data, sizeof(period_adv_data), presentation_delay_us);
    le_audio_base_builder_add_subgroup(&builder, codec_id,
                                       codec_ltv_len,
                                       codec_ltv_bytes,
                                       sizeof(subgroup_metadata), subgroup_metadata);
    uint8_t i;
    for (i=1;i<= num_bis;i++){
        le_audio_base_builder_add_bis(&builder, i, 0, NULL);
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

void btp_bap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_BAP;
    response_op = opcode;
    uint8_t ase_id;
    uint8_t i;
    switch (opcode) {
        case BTP_BAP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_BAP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_BAP_DISCOVER:
            MESSAGE("BTP_BAP_DISCOVER");
            if (controller_index == 0) {
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                // TODO: connect to ASCS and PACS Service
                // simulate done
                uint8_t buffer[8];
                memcpy(buffer, data, 7);
                buffer[7] = 0;
                btp_send(BTP_SERVICE_ID_BAP, BTP_BAP_EV_DISCOVERY_COMPLETED, controller_index, sizeof(buffer), buffer);
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
                big_params.num_bis = streams_per_subgroup;
                big_params.framing = framing;
                big_params.max_sdu = max_sdu;
                big_params.rtn = retransmission_num;
                big_params.max_transport_latency_ms = max_transport_latency;
                big_params.sdu_interval_us = sdu_interval;

                // setup BASE
                setup_periodic_adv(subgroups, presentation_delay, codec_id, ltv_len, ltv_data);

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
                        big_params.sdu_interval_us == 7500 ? BTSTACK_LC3_FRAME_DURATION_7500US : BTSTACK_LC3_FRAME_DURATION_10000US;
                le_audio_demo_util_source_configure(big_params.num_bis, 1, sampling_frequency_hz, frame_duration, big_params.max_sdu);
                MESSAGE("LC3 Config: streams %u, channels per stream %u, sampling frequency %u, frame duration %u, max sdu %u", big_params.num_bis, 1, sampling_frequency_hz, big_params.sdu_interval_us, big_params.max_sdu);
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
                start_advertising();
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_ADV_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ADV_STOP");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                stop_advertising();
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_START");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                setup_big();
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_STOP:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_STOP");
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                release_big();
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
        case BTP_BAP_DISCOVER_SCAN_DELEGATOR:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_DISCOVER_SCAN_DELEGATOR");
                bass_addr_type = data[0];
                reverse_bd_addr(&data[1], bass_address);
                broadcast_audio_scan_service_client_connect(&bass_connection,
                                                            bass_sources,
                                                            BASS_CLIENT_NUM_SOURCES,
                                                            remote_handle,
                                                            &bass_cid);
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
                MESSAGE("BTP_BAP_ADD_BROADCAST_SRC");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_REMOVE_BROADCAST_SRC:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_REMOVE_BROADCAST_SRC");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_MODIFY_BROADCAST_SRC:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_MODIFY_BROADCAST_SRC");
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_SET_BROADCAST_CODE:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_SET_BROADCAST_CODE");
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

    // BASS
    broadcast_audio_scan_service_client_init(&bass_packet_handler);

    // BIS
    uint8_t i;
    for (i=0;i<MAX_NUM_BIS;i++){
        bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
    }

    adv_handle = 0xff;
}
