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
#include "btstack.h"
#include "le-audio/le_audio_base_builder.h"
#include "le_audio_demo_util_source.h"

#define  MAX_NUM_BIS 2
#define  MAX_CHANNELS 2
#define MAX_LC3_FRAME_BYTES 155

// Random Broadcast ID, valid for lifetime of BIG
#define BROADCAST_ID (0x112233u)

// encryption
static uint8_t encryption = 0;
static uint8_t broadcast_code [] = {0x01, 0x02, 0x68, 0x05, 0x53, 0xF1, 0x41, 0x5A, 0xA2, 0x65, 0xBB, 0xAF, 0xC6, 0xEA, 0x03, 0xB8, };

static btstack_packet_callback_registration_t hci_event_callback_registration;

static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint16_t packet_sequence_numbers[MAX_NUM_BIS];
static uint8_t  iso_payload_data[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];

static uint8_t adv_handle;

static le_advertising_set_t le_advertising_set;

static uint8_t period_adv_data[255];
static uint16_t period_adv_data_len;

static le_audio_big_t big_storage;
static le_audio_big_params_t big_params;

static const uint8_t adv_sid = 0;

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
                    le_audio_demo_util_source_generate_iso_frame(AUDIO_SOURCE_SINE);
                    le_audio_demo_util_source_send(bis_index, bis_con_handles[bis_index]);
                    // confirm sent
                    if (response_op != 0){
                        uint8_t num_bytes_sent = big_params.max_sdu ;
                        btp_send(BTP_SERVICE_ID_BAP, response_op, 0, 1, &num_bytes_sent);
                        response_op = 0;
                    }
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)){
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
                            break;
                        case GAP_SUBEVENT_BIG_TERMINATED:
                            btp_bap_send_response_if_pending();
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
        case BTP_BAP_SEND:
            if (controller_index == 0) {
                uint16_t pos = 0;
                uint8_t addr_type = data[pos++];
                bd_addr_t address;
                reverse_bd_addr(&data[pos], address);
                pos += 6;
                uint8_t ase_id = data[pos++];
                uint8_t payload_len = data[pos++];  // ignored
                memcpy(iso_payload_data,  &data[pos], payload_len);
                hci_request_bis_can_send_now_events(big_params.big_handle);
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

                btstack_lc3_frame_duration_t frame_duration = big_params.sdu_interval_us == 7500 ? BTSTACK_LC3_FRAME_DURATION_7500US : BTSTACK_LC3_FRAME_DURATION_10000US;
                le_audio_demo_util_source_configure(1, big_params.num_bis, 48000, frame_duration, big_params.max_sdu);

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

    // BIS
    uint8_t i;
    for (i=0;i<MAX_NUM_BIS;i++){
        bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
    }

    adv_handle = 0xff;
}

