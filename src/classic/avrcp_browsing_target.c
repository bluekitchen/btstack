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
    uint8_t packet[400];
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

    uint8_t event[18];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    event[pos++] = connection->scope;
    little_endian_store_32(event, pos, connection->start_item);
    pos += 4;
    little_endian_store_32(event, pos, connection->end_item);
    pos += 4;
    little_endian_store_32(event, pos, connection->attr_bitmap);
    pos += 4;
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void avrcp_browsing_target_emit_search(btstack_packet_handler_t callback, uint16_t browsing_cid, avrcp_browsing_connection_t * connection){
    btstack_assert(callback != NULL);

    uint8_t event[11 + AVRCP_SEARCH_STRING_MAX_LENGTH];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_SEARCH;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    little_endian_store_16(event, pos, connection->target_search_characterset);
    pos += 2;
    little_endian_store_16(event, pos, connection->target_search_str_len);
    pos += 2;
    uint16_t target_search_str_len = btstack_min(AVRCP_SEARCH_STRING_MAX_LENGTH, strlen(connection->target_search_str));
    little_endian_store_16(event, pos, target_search_str_len);
    pos += 2;
    if (target_search_str_len > 0){
        memcpy(&event[pos], connection->target_search_str, target_search_str_len);
        connection->target_search_str[target_search_str_len - 1] = 0;
        pos += target_search_str_len;
    }
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
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
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
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
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void avrcp_browsing_target_emit_change_path(btstack_packet_handler_t callback, uint16_t browsing_cid, uint16_t uid_counter, avrcp_browsing_direction_t direction, uint8_t * item_id){
    btstack_assert(callback != NULL);

    uint8_t event[19];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CHANGE_PATH;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    little_endian_store_16(event, pos, uid_counter);
    pos += 2;
    event[pos++] = direction;
    memcpy(&event[pos], item_id, 8);
    pos += 8;
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void avrcp_browsing_target_emit_get_item_attributes(btstack_packet_handler_t callback, uint16_t browsing_cid, uint16_t uid_counter, uint8_t scope, uint8_t * item_id, uint8_t attr_num, uint8_t * attr_list){
    btstack_assert(callback != NULL);
    btstack_assert(attr_num <= AVRCP_MEDIA_ATTR_NUM);

    uint8_t event[19 + 4 * AVRCP_MEDIA_ATTR_NUM];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_GET_ITEM_ATTRIBUTES;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    little_endian_store_16(event, pos, uid_counter);
    pos += 2;
    event[pos++] = scope;
    memcpy(&event[pos], item_id, 8);
    pos += 8;
    uint16_t attr_len = attr_num * 4;
    little_endian_store_16(event, pos, attr_len);
    pos += 2;

    memcpy(&event[pos], attr_list, attr_len);
    pos += attr_len;
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
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
                    uint16_t parameter_length = big_endian_read_16(packet, pos);
                    pos += 2;

                    switch(browsing_connection->pdu_id){
                        case AVRCP_PDU_ID_SEARCH:{
                            if (parameter_length < 4){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }
                            browsing_connection->target_search_characterset = big_endian_read_16(packet, pos);
                            pos += 2;
                            browsing_connection->target_search_str_len = big_endian_read_16(packet, pos);
                            pos += 2;
                            browsing_connection->target_search_str = (char *) &packet[pos];

                            if (parameter_length < (4 + browsing_connection->target_search_str_len)){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }

                            uint16_t string_len = strlen(browsing_connection->target_search_str);
                            if ((browsing_connection->target_search_str_len != string_len) || (browsing_connection->target_search_str_len > (size-pos))){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_PARAMETER);
                                break;
                            }
                            avrcp_browsing_target_emit_search(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection);
                            break;
                        }
                        case AVRCP_PDU_ID_GET_FOLDER_ITEMS:
                            if (parameter_length < 10){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }

                            browsing_connection->scope = packet[pos++];
                            browsing_connection->start_item = big_endian_read_32(packet, pos);
                            pos += 4;
                            browsing_connection->end_item = big_endian_read_32(packet, pos);
                            pos += 4;
                            uint8_t attr_count = packet[pos++];
                            browsing_connection->attr_bitmap = 0;

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
                            if (parameter_length != 1){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_SCOPE);
                                break;
                            }

                            browsing_connection->scope = packet[pos++];
                            avrcp_browsing_target_emit_get_total_num_items(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection);
                            break;
                        }
                        case AVRCP_PDU_ID_SET_BROWSED_PLAYER:
                            // player_id (2)
                            if (parameter_length != 2){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }
                            if ( (pos + 2) > size ){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_PLAYER_ID);
                                break;
                            }
                            avrcp_browsing_target_emit_set_browsed_player(avrcp_target_context.browsing_avrcp_callback, channel, big_endian_read_16(packet, pos));
                            break;

                        case AVRCP_PDU_ID_CHANGE_PATH:
                            // one level up or down in the virtual filesystem
                            if (parameter_length != 11){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }
                            browsing_connection->uid_counter = big_endian_read_16(packet, pos);
                            pos += 2;
                            browsing_connection->direction = (avrcp_browsing_direction_t)packet[pos++];

                            if (browsing_connection->direction > AVRCP_BROWSING_DIRECTION_FOLDER_RFU){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_DIRECTION);
                                break;
                            }
                            memcpy(browsing_connection->item_uid, &packet[pos], 8);
                            avrcp_browsing_target_emit_change_path(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection->uid_counter, browsing_connection->direction, browsing_connection->item_uid);
                            break;

                        case AVRCP_PDU_ID_GET_ITEM_ATTRIBUTES:{
                            if (parameter_length < 12){
                                avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                                break;
                            }

                            browsing_connection->scope = packet[pos++];
                            memcpy(browsing_connection->item_uid, &packet[pos], 8);
                            pos += 8;
                            browsing_connection->uid_counter = big_endian_read_16(packet, pos);
                            pos += 2;
                            browsing_connection->attr_list_size = packet[pos++];
                            browsing_connection->attr_list = &packet[pos];
                            
                            avrcp_browsing_target_emit_get_item_attributes(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection->uid_counter,
                                                                           browsing_connection->scope, browsing_connection->item_uid, browsing_connection->attr_list_size, browsing_connection->attr_list);
                            break;
                        }

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

uint8_t avrcp_browsing_target_send_get_folder_items_response(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint8_t * attr_list, uint16_t attr_list_size, uint16_t num_items){
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
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // TODO: handle response to SetAddressedPlayer

    uint16_t pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_FOLDER_ITEMS;
    uint8_t param_length = 5;
    uint8_t param_length_pos = pos;
    pos += 2;

    avrcp_status_code_t status = AVRCP_STATUS_SUCCESS;
    uint8_t status_pos = pos;
    pos++;

    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;
    
    // TODO: fragmentation
    if (attr_list_size >  sizeof(connection->cmd_operands)){
        connection->attr_list = attr_list;
        connection->attr_list_size = attr_list_size;
        log_info(" todo: list too big, invoke fragmentation");
        return 1;
    }

    uint16_t items_byte_len = 0;
    if (connection->start_item < num_items) {
        if (connection->end_item < num_items) {
            items_byte_len =  connection->end_item - connection->start_item + 1;
        } else {
            items_byte_len = num_items - connection->start_item;
        }

    } else {
        big_endian_store_16(connection->cmd_operands, pos, 0);
        pos += 2;
    }
    big_endian_store_16(connection->cmd_operands, pos, items_byte_len);
    pos += 2;
    param_length += items_byte_len;

    if (items_byte_len > 0){
        (void)memcpy(&connection->cmd_operands[pos], attr_list, attr_list_size);
        pos += attr_list_size;
        connection->cmd_operands_length = pos;
    } else {
        status = AVRCP_STATUS_RANGE_OUT_OF_BOUNDS;
        param_length = 1;
        connection->cmd_operands_length = status_pos + 1;
    }

    big_endian_store_16(connection->cmd_operands, param_length_pos, param_length);
    connection->cmd_operands[status_pos] = status;

    btstack_assert(pos <= 400);


    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_send_change_path_response(uint16_t avrcp_browsing_cid, avrcp_status_code_t status, uint32_t num_items){
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
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t pos = 0;
    uint16_t param_length = (status == AVRCP_STATUS_SUCCESS) ? 5 : 1;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_CHANGE_PATH;

    // Param length
    big_endian_store_16(connection->cmd_operands, pos, param_length);
    pos += 2;
    connection->cmd_operands[pos++] = status;

    if (status == AVRCP_STATUS_SUCCESS){
        big_endian_store_32(connection->cmd_operands, pos, num_items);
        pos += 4;
    }

    connection->cmd_operands_length = pos;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_send_get_item_attributes_response(uint16_t avrcp_browsing_cid, avrcp_status_code_t status, uint8_t * attr_list, uint16_t attr_list_size, uint8_t num_items){
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
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // TODO: fragmentation
    if (attr_list_size >  (sizeof(connection->cmd_operands) - 5)){
        connection->attr_list = attr_list;
        connection->attr_list_size = attr_list_size;
        log_info(" todo: list too big, invoke fragmentation");
        return 1;
    }

    uint16_t pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GET_ITEM_ATTRIBUTES;

    uint8_t param_length_pos = pos;
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = status;

    if (status != AVRCP_STATUS_SUCCESS){
        connection->cmd_operands_length = pos;
        connection->state = AVCTP_W2_SEND_RESPONSE;
        avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
        return ERROR_CODE_SUCCESS;
    }

    connection->cmd_operands[pos++] = num_items;
    (void)memcpy(&connection->cmd_operands[pos], attr_list, attr_list_size);
    pos += attr_list_size;

    big_endian_store_16(connection->cmd_operands, param_length_pos, pos - 3);
    connection->cmd_operands_length = pos;
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
    pos += 2;
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

uint8_t avrcp_browsing_target_send_search_response(uint16_t avrcp_browsing_cid, avrcp_status_code_t status, uint16_t uid_counter, uint32_t num_items){
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
    if (connection->state != AVCTP_CONNECTION_OPENED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_SEARCH;
    // param len
    big_endian_store_16(connection->cmd_operands, pos, 7);
    pos += 2;
    connection->cmd_operands[pos++] = status;
    big_endian_store_16(connection->cmd_operands, pos, uid_counter);
    pos += 2;

//    if (status != AVRCP_STATUS_SUCCESS){
//        connection->cmd_operands_length = pos;
//        connection->state = AVCTP_W2_SEND_RESPONSE;
//        avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
//        return ERROR_CODE_SUCCESS;
//    }

    big_endian_store_32(connection->cmd_operands, pos, num_items);
    pos += 4;
    connection->cmd_operands_length = pos;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}