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

#define  MAX_NUM_BIS 2
#define  MAX_CHANNELS 2
#define MAX_LC3_FRAME_BYTES 155

static btstack_packet_callback_registration_t hci_event_callback_registration;

static hci_con_handle_t bis_con_handles[MAX_NUM_BIS];
static uint16_t packet_sequence_numbers[MAX_NUM_BIS];
static uint8_t iso_payload[MAX_CHANNELS * MAX_LC3_FRAME_BYTES];
static uint16_t bis_sdu_size;
static uint8_t num_bis;

static void send_iso_packet(uint8_t cis_index){
    hci_reserve_packet_buffer();
    uint8_t * buffer = hci_get_outgoing_packet_buffer();
    // complete SDU, no TimeStamp
    little_endian_store_16(buffer, 0, bis_con_handles[cis_index] | (2 << 12));
    // len
    little_endian_store_16(buffer, 2, 0 + 4 + bis_sdu_size);
    // TimeStamp if TS flag is set
    // packet seq nr
    little_endian_store_16(buffer, 4, packet_sequence_numbers[cis_index]);
    // iso sdu len
    little_endian_store_16(buffer, 6, bis_sdu_size);
    // copy encoded payload
    uint8_t i;
    uint16_t offset = 8;
    memcpy(&buffer[offset], &iso_payload[i * MAX_LC3_FRAME_BYTES], bis_sdu_size);
    offset += bis_sdu_size;

    // send
    hci_send_iso_packet_buffer(offset);

    packet_sequence_numbers[cis_index]++;
}

// HCI Handler
static void hci_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    hci_con_handle_t con_handle;
    hci_con_handle_t cis_con_handle;
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
                            MESSAGE("HCI Disconnect for bis_con_handle 0x%04x, with index %u", cis_con_handle, i);
                            bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
                        }
                    }
                    break;
                case HCI_EVENT_BIS_CAN_SEND_NOW:
                    cis_con_handle = hci_event_cis_can_send_now_get_cis_con_handle(packet);
                    for (i = 0; i < num_bis; i++) {
                        if (cis_con_handle == bis_con_handles[i]){
                            send_iso_packet(i);
                            hci_request_cis_can_send_now_events(cis_con_handle);
                        }
                    }
                    break;
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)){
                            break;
                        default:
                            break;
                    }
                    break;
                case HCI_EVENT_META_GAP:
                    switch (hci_event_gap_meta_get_subevent_code(packet)) {
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

void btp_bap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_BAP;
    response_op = opcode;
    uint8_t ase_id;
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
                uint8_t payload_len = data[pos++];
                const uint8_t *const payload_data = &data[pos];
                uint8_t result = payload_len;
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, sizeof(result), &result);
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_SETUP:
            if (controller_index == 0){
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_SETUP");
                uint16_t pos = 0;
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
                uint8_t coding_format = data[pos++];
                uint16_t vid = little_endian_read_16(data, pos);
                pos += 2;
                uint16_t cid = little_endian_read_16(data, pos);
                pos += 2;
                uint8_t ltv_len = data[pos++];
                const uint8_t * const ltv_data = &data[pos];
                uint8_t result[7];
                little_endian_store_32(result, 0, btp_gap_current_settings());
                uint32_t broadcast_id = 1;
                little_endian_store_24(result, 4, broadcast_id);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, sizeof(result), result);
            }
            break;
        case BTP_BAP_BROADCAST_ADV_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_ADV_START");
                uint16_t pos = 0;
                uint32_t broadcast_id = little_endian_read_24(data, 0);
                btp_send(BTP_SERVICE_ID_BAP, opcode, controller_index, 0, NULL);
                break;
            }
            break;
        case BTP_BAP_BROADCAST_SOURCE_START:
            if (controller_index == 0) {
                MESSAGE("BTP_BAP_BROADCAST_SOURCE_START");
                uint16_t pos = 0;
                uint32_t broadcast_id = little_endian_read_24(data, 0);
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

    // BIS
    uint8_t i;
    for (i=0;i<MAX_NUM_BIS;i++){
        bis_con_handles[i] = HCI_CON_HANDLE_INVALID;
    }
}

