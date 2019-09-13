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

#define BTSTACK_FILE__ "avrcp_browsing_target.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "btstack.h"
#include "classic/avrcp.h"
#include "classic/avrcp_browsing_target.h"

#define PSM_AVCTP_BROWSING              0x001b

static void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context);
static void avrcp_browsing_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void avrcp_browsing_target_request_can_send_now(avrcp_browsing_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}

static int avrcp_browsing_target_handle_can_send_now(avrcp_browsing_connection_t * connection){
    int pos = 0; 
    // printf("avrcp_browsing_target_handle_can_send_now, cmd_operands_length %d\n", connection->cmd_operands_length);
    // printf_hexdump(connection->cmd_operands, connection->cmd_operands_length);

    // l2cap_reserve_packet_buffer();
    // uint8_t * packet = l2cap_get_outgoing_buffer();
    uint8_t packet[300];
    connection->packet_type = AVRCP_SINGLE_PACKET;

    packet[pos++] = (connection->transaction_label << 4) | (connection->packet_type << 2) | (AVRCP_RESPONSE_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    packet[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    memcpy(packet+pos, connection->cmd_operands, connection->cmd_operands_length);
    
    pos += connection->cmd_operands_length;
    connection->wait_to_send = 0;
    // return l2cap_send_prepared(connection->l2cap_browsing_cid, pos);
    return l2cap_send(connection->l2cap_browsing_cid, packet, pos);
}


static uint8_t avrcp_browsing_target_response_general_reject(avrcp_browsing_connection_t * connection, avrcp_status_code_t status){
    // AVRCP_CTYPE_RESPONSE_REJECTED
    int pos = 0;
    connection->cmd_operands[pos++] = AVRCP_PDU_ID_GENERAL_REJECT;
    // connection->cmd_operands[pos++] = 0;
    // param length
    big_endian_store_16(connection->cmd_operands, pos, 1);
    pos += 2;
    connection->cmd_operands[pos++] = status;
    connection->cmd_operands_length = 4;
    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_target_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

static void avrcp_browsing_target_emit_get_folder_items(btstack_packet_handler_t callback, uint16_t browsing_cid, avrcp_browsing_connection_t * connection){
    if (!callback) return;
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
    if (!callback) return;
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

static void avrcp_emit_browsing_connection_established(btstack_packet_handler_t callback, uint16_t browsing_cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_incoming_browsing_connection(btstack_packet_handler_t callback, uint16_t browsing_cid, bd_addr_t addr){
    if (!callback) return;
    uint8_t event[11];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_browsing_connection_closed(btstack_packet_handler_t callback, uint16_t browsing_cid){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static avrcp_browsing_connection_t * avrcp_browsing_create_connection(avrcp_connection_t * avrcp_connection){
    avrcp_browsing_connection_t * connection = btstack_memory_avrcp_browsing_connection_get();
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->transaction_label = 0xFF;
    avrcp_connection->avrcp_browsing_cid = avrcp_get_next_cid(avrcp_connection->role);
    avrcp_connection->browsing_connection = connection;
    return connection;
}

static uint8_t avrcp_browsing_connect(bd_addr_t remote_addr, avrcp_context_t * context, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_bd_addr(context->role, remote_addr);
    
    if (!avrcp_connection){
        log_error("avrcp: there is no previously established AVRCP controller connection.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (connection){
        log_error(" avrcp_browsing_connect connection exists.");
        return ERROR_CODE_SUCCESS;
    }
    
    connection = avrcp_browsing_create_connection(avrcp_connection);
    if (!connection){
        log_error("avrcp: could not allocate connection struct.");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    if (browsing_cid){
        *browsing_cid = avrcp_connection->avrcp_browsing_cid; 
    }
    
    connection->ertm_buffer = ertm_buffer;
    connection->ertm_buffer_size = size;
    avrcp_connection->browsing_connection = connection;

    memcpy(&connection->ertm_config, ertm_config, sizeof(l2cap_ertm_config_t));

    return l2cap_create_ertm_channel(avrcp_browsing_target_packet_handler, remote_addr, avrcp_connection->browsing_l2cap_psm, 
                    &connection->ertm_config, connection->ertm_buffer, connection->ertm_buffer_size, NULL);

}

static void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;
    avrcp_browsing_connection_t * browsing_connection = NULL;
    avrcp_connection_t * avrcp_connection = NULL;
    
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            avrcp_emit_browsing_connection_closed(context->browsing_avrcp_callback, 0);
            break;
        case L2CAP_EVENT_INCOMING_CONNECTION:
            l2cap_event_incoming_connection_get_address(packet, event_addr);
            local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
            avrcp_connection = get_avrcp_connection_for_bd_addr(context->role, event_addr);
            if (!avrcp_connection) {
                log_error("No previously created AVRCP controller connections");
                l2cap_decline_connection(local_cid);
                break;
            }
            browsing_connection = avrcp_browsing_create_connection(avrcp_connection);
            browsing_connection->l2cap_browsing_cid = local_cid;
            browsing_connection->state = AVCTP_CONNECTION_W4_ERTM_CONFIGURATION;
            log_info("Emit AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION browsing_cid 0x%02x, l2cap_signaling_cid 0x%02x\n", avrcp_connection->avrcp_browsing_cid, browsing_connection->l2cap_browsing_cid);
            avrcp_emit_incoming_browsing_connection(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr);
            break;
            
        case L2CAP_EVENT_CHANNEL_OPENED:
            l2cap_event_channel_opened_get_address(packet, event_addr);
            status = l2cap_event_channel_opened_get_status(packet);
            local_cid = l2cap_event_channel_opened_get_local_cid(packet);
            
            avrcp_connection = get_avrcp_connection_for_bd_addr(context->role, event_addr);
            if (!avrcp_connection){
                log_error("Failed to find AVRCP connection for bd_addr %s", bd_addr_to_str(event_addr));
                avrcp_emit_browsing_connection_established(context->browsing_avrcp_callback, local_cid, event_addr, L2CAP_LOCAL_CID_DOES_NOT_EXIST);
                l2cap_disconnect(local_cid, 0); // reason isn't used
                break;
            }

            browsing_connection = avrcp_connection->browsing_connection;
            if (status != ERROR_CODE_SUCCESS){
                log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                avrcp_emit_browsing_connection_established(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr, status);
                btstack_memory_avrcp_browsing_connection_free(browsing_connection);
                avrcp_connection->browsing_connection = NULL;
                break;
            }
            if (browsing_connection->state != AVCTP_CONNECTION_W4_L2CAP_CONNECTED) break;
            
            browsing_connection->l2cap_browsing_cid = local_cid;

            log_info("L2CAP_EVENT_CHANNEL_OPENED browsing cid 0x%02x, l2cap cid 0x%02x", avrcp_connection->avrcp_browsing_cid, browsing_connection->l2cap_browsing_cid);
            browsing_connection->state = AVCTP_CONNECTION_OPENED;
            avrcp_emit_browsing_connection_established(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid, event_addr, ERROR_CODE_SUCCESS);
            break;
        
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            local_cid = l2cap_event_channel_closed_get_local_cid(packet);
            avrcp_connection = get_avrcp_connection_for_browsing_l2cap_cid(context->role, local_cid);
            
            if (avrcp_connection && avrcp_connection->browsing_connection){
                avrcp_emit_browsing_connection_closed(context->browsing_avrcp_callback, avrcp_connection->avrcp_browsing_cid);
                // free connection
                btstack_memory_avrcp_browsing_connection_free(avrcp_connection->browsing_connection);
                avrcp_connection->browsing_connection = NULL;
                break;
            }
            break;
        default:
            break;
    }
}


static void avrcp_browsing_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_browsing_connection_t * browsing_connection;

    switch (packet_type) {
        case L2CAP_DATA_PACKET:{
            browsing_connection = get_avrcp_browsing_connection_for_l2cap_cid(AVRCP_TARGET, channel);
            if (!browsing_connection) break;
            // printf_hexdump(packet,size);
            int pos = 0;
            uint8_t transport_header = packet[pos++];
            // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
            browsing_connection->transaction_label = transport_header >> 4;
            avrcp_packet_type_t avctp_packet_type = (transport_header & 0x0F) >> 2;
            // printf("L2CAP_DATA_PACKET, transaction_label %d\n", browsing_connection->transaction_label);
            switch (avctp_packet_type){
                case AVRCP_SINGLE_PACKET:
                case AVRCP_START_PACKET:
                    // uint8_t frame_type = (transport_header & 0x03) >> 1;
                    // uint8_t ipid = transport_header & 0x01;
                    browsing_connection->subunit_type = packet[pos++] >> 2;
                    browsing_connection->subunit_id = 0;
                    browsing_connection->command_opcode = packet[pos++];
                    // printf("subunit_id")
                    // pos += 2;
                    browsing_connection->num_packets = 1;
                    if (avctp_packet_type == AVRCP_START_PACKET){
                        browsing_connection->num_packets = packet[pos++];
                    } 
                    browsing_connection->pdu_id = packet[pos++];
                    // uint16_t length = big_endian_read_16(packet, pos);
                    // pos += 2;
                    break;
                default:
                    break;
            }
            // printf("pdu id 0x%2x\n", browsing_connection->pdu_id);
            // uint32_t i;
            switch (avctp_packet_type){
                case AVRCP_SINGLE_PACKET:
                case AVRCP_END_PACKET:
                    switch(browsing_connection->pdu_id){
                        case AVRCP_PDU_ID_GET_FOLDER_ITEMS:
                            printf("\n");
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
                            // uint32_t num_items = big_endian_read_32(packet, pos);
                            // pos += 4;
                            break;
                        }
                        default:
                            // printf("send avrcp_browsing_target_response_general_reject\n");
                            avrcp_browsing_target_response_general_reject(browsing_connection, AVRCP_STATUS_INVALID_COMMAND);
                            log_info(" not parsed pdu ID 0x%02x", browsing_connection->pdu_id);
                            break;
                    }
                    browsing_connection->state = AVCTP_CONNECTION_OPENED;
                    // avrcp_browsing_target_emit_done_with_uid_counter(avrcp_target_context.browsing_avrcp_callback, channel, browsing_connection->uid_counter, browsing_connection->browsing_status, ERROR_CODE_SUCCESS);
                    break;
                default:
                    break;
            }
            // printf(" paket done\n");
            break;
        }
        
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:
                    browsing_connection = get_avrcp_browsing_connection_for_l2cap_cid(AVRCP_TARGET, channel);
                    if (!browsing_connection) break;
                    if (browsing_connection->state != AVCTP_W2_SEND_RESPONSE) return;
                    browsing_connection->state = AVCTP_CONNECTION_OPENED;
                    avrcp_browsing_target_handle_can_send_now(browsing_connection);
                    break;
                default:
                    avrcp_browser_packet_handler(packet_type, channel, packet, size, &avrcp_target_context);
                    break;
            }
            break;

        default:
            break;
    }
}

void avrcp_browsing_target_init(void){
    avrcp_target_context.browsing_packet_handler = avrcp_browsing_target_packet_handler;
    l2cap_register_service(&avrcp_browsing_target_packet_handler, PSM_AVCTP_BROWSING, 0xffff, LEVEL_2);
}

void avrcp_browsing_target_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avrcp_browsing_target_register_packet_handler called with NULL callback");
        return;
    }
    avrcp_target_context.browsing_avrcp_callback = callback;
}

uint8_t avrcp_browsing_target_connect(bd_addr_t bd_addr, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid){
    return avrcp_browsing_connect(bd_addr, &avrcp_target_context, ertm_buffer, size, ertm_config, avrcp_browsing_cid);
}

uint8_t avrcp_browsing_target_disconnect(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_target_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (avrcp_connection->browsing_connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    l2cap_disconnect(avrcp_connection->browsing_connection->l2cap_browsing_cid, 0);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_configure_incoming_connection(uint16_t avrcp_browsing_cid, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_decline_incoming_connection: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!avrcp_connection->browsing_connection){
        log_error("avrcp_browsing_decline_incoming_connection: no browsing connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 

    if (avrcp_connection->browsing_connection->state != AVCTP_CONNECTION_W4_ERTM_CONFIGURATION){
        log_error("avrcp_browsing_decline_incoming_connection: browsing connection in a wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    avrcp_connection->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    avrcp_connection->browsing_connection->ertm_buffer = ertm_buffer;
    avrcp_connection->browsing_connection->ertm_buffer_size = size;
    memcpy(&avrcp_connection->browsing_connection->ertm_config, ertm_config, sizeof(l2cap_ertm_config_t));
    l2cap_accept_ertm_connection(avrcp_connection->browsing_connection->l2cap_browsing_cid, &avrcp_connection->browsing_connection->ertm_config, avrcp_connection->browsing_connection->ertm_buffer, avrcp_connection->browsing_connection->ertm_buffer_size);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_target_decline_incoming_connection(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_decline_incoming_connection: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!avrcp_connection->browsing_connection) return ERROR_CODE_SUCCESS;
    if (avrcp_connection->browsing_connection->state > AVCTP_CONNECTION_W4_ERTM_CONFIGURATION) return ERROR_CODE_COMMAND_DISALLOWED;
    
    l2cap_decline_connection(avrcp_connection->browsing_connection->l2cap_browsing_cid);
    // free connection
    btstack_memory_avrcp_browsing_connection_free(avrcp_connection->browsing_connection);
    avrcp_connection->browsing_connection = NULL;
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_subevent_browsing_get_folder_items_response(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint8_t * attr_list, uint16_t attr_list_size){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (!connection){
        log_info("avrcp_subevent_browsing_get_folder_items_response: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    if (connection->state != AVCTP_CONNECTION_OPENED) {
        log_info("avrcp_subevent_browsing_get_folder_items_response: wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    int pos = 0;

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
    memcpy(&connection->cmd_operands[pos], attr_list, attr_list_size);
    pos += attr_list_size;
    connection->cmd_operands_length = pos;    
    // printf_hexdump(connection->cmd_operands, connection->cmd_operands_length);

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_target_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}


uint8_t avrcp_subevent_browsing_get_total_num_items_response(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint32_t total_num_items){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_TARGET, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (!connection){
        log_info("avrcp_subevent_browsing_get_folder_items_response: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    if (connection->state != AVCTP_CONNECTION_OPENED) {
        log_info("avrcp_subevent_browsing_get_folder_items_response: wrong state.");
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
    // printf_hexdump(connection->cmd_operands, connection->cmd_operands_length);

    connection->state = AVCTP_W2_SEND_RESPONSE;
    avrcp_browsing_target_request_can_send_now(connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

