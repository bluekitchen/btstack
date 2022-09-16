/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "avrcp_browsing_target.c"

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "classic/avrcp.h"
#include "classic/avrcp_browsing.h"
#include "classic/avrcp_browsing_target.h"
#include "classic/avrcp_target.h"
#include "classic/avrcp_controller.h"

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"

static void avrcp_browsing_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static int avrcp_browsing_target_handle_can_send_now(avrcp_browsing_connection_t * connection){
    int pos = 0; 
    
    // l2cap_reserve_packet_buffer();
    // uint8_t * packet = l2cap_get_outgoing_buffer();
    uint8_t packet[300];
    connection->packet_type = AVRCP_SINGLE_PACKET;

    packet[pos++] = (connection->transaction_label << 4) | (connection->packet_type << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    (void)memcpy(packet + pos, connection->cmd_operands,
                 connection->cmd_operands_length);
    
    pos += connection->cmd_operands_length;
    connection->wait_to_send = false;
    // return l2cap_send_prepared(connection->l2cap_browsing_cid, pos);
    return l2cap_send(connection->l2cap_browsing_cid, packet, pos);
}


static uint8_t avrcp_browsing_target_response_general_reject(avrcp_browsing_connection_t * connection, avrcp_status_code_t status){
    // AVRCP_CTYPE_RESPONSE_REJECTED
    int pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GENERAL_REJECT;
    // connection->message_body[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = status;
    connection->cmd_operands_length = 4;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

static void avrcp_browsing_target_emit_get_folder_items(btstack_packet_handler_t callback, uint16_t browsing_cid, avrcp_browsing_connection_t * connection){
    btstack_assert(callback != NULL);

    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    event[pos++] = connection->scope;
    big_endian_store_32(event, pos, connection->attr_bitmap);
    pos += 4;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_browsing_target_emit_get_total_num_items(btstack_packet_handler_t callback, uint16_t browsing_cid, avrcp_browsing_connection_t * connection){
    btstack_assert(callback != NULL);

    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_GET_TOTAL_NUM_ITEMS;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    event[pos++] = connection->scope;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_browsing_target_emit_set_browsed_player(btstack_packet_handler_t callback, uint16_t browsing_cid, uint16_t browsed_player_id){
    btstack_assert(callback != NULL);

    uint8_t event[10];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_SET_BROWSED_PLAYER;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    little_endian_store_16(event, pos, browsed_player_id);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static void avrcp_browsing_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(size);
    avrcp_browsing_connection_t * browsing_connection;

    switch (packet_type) {
        case L2CAP_DATA_PACKET:{
            browsing_connection = avrcp_get_browsing_connection_for_l2cap_cid_for_role(AVRCP_TARGET, channel);
            if (!browsing_connection) break;
            int pos = 0;
            uint8_t transport_header = packet[pos++];
            // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
            browsing_connection->transaction_label = transport_header >> 4;
            avctp_packet_type_t avctp_packet_type = (avctp_packet_type_t)((transport_header & 0x0F) >> 2);
            switch (avctp_packet_type){
                case AVCTP_SINGLE_PACKET:
                case AVCTP_START_PACKET:
                    browsing_connection->subunit_type = packet[pos++] >> 2;
                    browsing_connection->subunit_id = 0;
                    browsing_connection->command_opcode = packet[pos++];
                    browsing_connection->num_packets = 1;
                    if (avctp_packet_type == AVCTP_START_PACKET){
                        browsing_connection->num_packets = packet[pos++];
                    } 
                    browsing_connection->pdu_id = packet[pos++];
                   
                    switch(browsing_connection->pdu_id){
                        case AVRCP_PDU_ID_GET_FOLDER_ITEMS:
                            browsing_connection->scope = packet[pos++];
                            browsing_connection->start_item = big_endian_read_32(packet, pos);
                            pos += 4;
                            browsing_connection->end_item = big_endian_read_32(packet, pos);
                            pos += 4;
                            uint8_t attr_count = packet[pos++];

                            while (attr_count){
                                uint32_t attr_id = big_endian_read_32(packet, pos);
                                pos += 4;
                                browsing_connection->attr_bitmap |= (1 << attr_id);
                                attr_count--;
                            }
                            avrcp_browsing_target_emit_get_folder_items(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection);
                            break;
                        case AVRCP_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS:{
                            // send total num items
                            browsing_connection->scope = packet[pos++];
                            avrcp_browsing_target_emit_get_total_num_items(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection);
                            break;
                        }
                        case AVRCP_PDU_ID_SET_BROWSED_PLAYER:
                            // param length (2), player_id (2)
                            if (big_endian_read_16(packet, pos) != 2){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }
                            avrcp_browsing_target_emit_set_browsed_player(avrcp_target_context.browsing_avrcp_callback, channel, big_endian_read_16(packet, pos+2));
                            break;
                        default:
                            avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                            log_info("not parsed pdu ID 0x%02x", browsing_connection->pdu_id);
                            break;
                    }
                    browsing_connection->state = AVCTP_CONNECTION_OPENED;
                    break;
                default:
                    break;
            }
            break;
        }
        
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:
                    browsing_connection = avrcp_get_browsing_connection_for_l2cap_cid_for_role(AVRCP_TARGET, channel);
                    if (browsing_connection->state != AVCTP_W2_SEND_RESPONSE) return;
                    browsing_connection->state = AVCTP_CONNECTION_OPENED;
                    avrcp_browsing_target_handle_can_send_now(browsing_connection);
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

void avrcp_browsing_target_init(void){
    avrcp_target_context.browsing_packet_handler = avrcp_browsing_target_packet_handler;
    avrcp_browsing_register_target_packet_handler(avrcp_browsing_target_packet_handler);
}

void avrcp_browsing_target_deinit(void){
    avrcp_controller_context.browsing_packet_handler = NULL;
}

void avrcp_browsing_target_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    avrcp_target_context.browsing_avrcp_callback = callback;
}

uint8_t avrcp_browsing_target_send_get_folder_items_response(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint8_t * attr_list, uint16_t attr_list_size){
    avrcp_connection_t * avrcp_connection = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("Could not find an AVRCP Target connection for browsing_cid 0x%02x.", avrcp_browsing_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (!connection){
        log_info("Could not find a browsing connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;

    // TODO: handle response to SetAddressedPlayer

    uint16_t pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_FOLDER_ITEMS;
    big_endian_store_16(connection->cmd_operands, pos, attr_list_size);
    pos += 2;
    
    connection->cmd_operands[pos++] = AVRCP_STATUS_SUCCESS;
    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;
    
    // TODO: fragmentation
    if (attr_list_size >  sizeof(connection->cmd_operands)){
        connection->attr_list = attr_list;
        connection->attr_list_size = attr_list_size;
        log_info(" todo: list too big, invoke fragmentation");
        return 1;
    }
    (void)memcpy(&connection->cmd_operands[pos], attr_list, attr_list_size);
    pos += attr_list_size;
	btstack_assert(pos <= 255);
    connection->cmd_operands_length = (uint8_t) pos;

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_send_accept_set_browsed_player(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint16_t browsed_player_id, uint8_t * response, uint16_t response_size){
    avrcp_connection_t * avrcp_connection = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("Could not find an AVRCP Target connection for browsing_cid 0x%02x.", avrcp_browsing_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (!connection){
        log_info("Could not find a browsing connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (connection->state != AVCTP_CONNECTION_OPENED) {
        log_info("Browsing connection wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    connection->browsed_player_id = browsed_player_id;

    uint16_t pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SET_BROWSED_PLAYER;
    big_endian_store_16(connection->cmd_operands, pos, response_size + 2 + 1); // uuid counter + status
    pos += 2;

    connection->cmd_operands[pos++] = AVRCP_STATUS_SUCCESS;
    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;

    // TODO: fragmentation
    if (response_size >  sizeof(connection->cmd_operands)){
        connection->attr_list = response;
        connection->attr_list_size = response_size;
        log_info(" todo: list too big, invoke fragmentation");
        return 1;
    }
    
    (void)memcpy(&connection->cmd_operands[pos], response, response_size);
    pos += response_size;
	btstack_assert(pos <= 255);
    connection->cmd_operands_length = (uint8_t) pos;

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_send_reject_set_browsed_player(uint16_t avrcp_browsing_cid, avrcp_status_code_t status){
    avrcp_connection_t * avrcp_connection = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("Could not find an AVRCP Target connection for browsing_cid 0x%02x.", avrcp_browsing_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (!connection){
        log_info("Could not find a browsing connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (connection->state != AVCTP_CONNECTION_OPENED) {
        log_info("Browsing connection wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    int pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SET_BROWSED_PLAYER;
    big_endian_store_16(connection->cmd_operands, pos, 1);
    connection->cmd_operands[pos++] = status;
    connection->cmd_operands_length = pos;    

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_send_get_total_num_items_response(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint32_t total_num_items){
    avrcp_connection_t * avrcp_connection = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("Could not find an AVRCP Target connection for browsing_cid 0x%02x.", avrcp_browsing_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (!connection){
        log_info("Could not find a browsing connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (connection->state != AVCTP_CONNECTION_OPENED) {
        log_info("Browsing connection wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    int pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS;
    big_endian_store_16(connection->cmd_operands, pos, 7);
    pos += 2;
    connection->cmd_operands[pos++] = AVRCP_STATUS_SUCCESS;
    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;
    big_endian_store_32(connection->cmd_operands, pos, total_num_items);
    pos += 4; 
    connection->cmd_operands_length = pos;    

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

