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

#define BTSTACK_FILE__ "avrcp_browsing_controller.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "btstack.h"
#include "classic/avrcp.h"
#include "classic/avrcp_browsing_controller.h"

#define PSM_AVCTP_BROWSING              0x001b

static void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context);
static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

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

static uint8_t avrcp_browsing_connect(bd_addr_t remote_addr, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_bd_addr(AVRCP_CONTROLLER, remote_addr);
    
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
    avrcp_connection->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    memcpy(&connection->ertm_config, ertm_config, sizeof(l2cap_ertm_config_t));

    return l2cap_create_ertm_channel(avrcp_browsing_controller_packet_handler, remote_addr, avrcp_connection->browsing_l2cap_psm, 
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
            avrcp_connection = get_avrcp_connection_for_bd_addr(AVRCP_CONTROLLER, event_addr);
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
            
            avrcp_connection = get_avrcp_connection_for_bd_addr(AVRCP_CONTROLLER, event_addr);
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

static int avrcp_browsing_controller_send_get_folder_items_cmd(uint16_t cid, avrcp_browsing_connection_t * connection){
    uint8_t command[100];
    int pos = 0; 

    // // if (connection->cmd_operands[3] == AVRCP_PDU_ID_GET_CAPABILITIES){
    //     printf("cheet\n");
    //     uint8_t buffer[] = {0xB0, 0x11, 0x0E, 0xFF, 0x00, 0x02, 0xFF, 0xFF};
    //     memcpy(command, buffer, sizeof(buffer));
    //     pos =  sizeof(buffer);
    //     return l2cap_send(cid, command, pos);
    // // }

    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_PDU_ID_GET_FOLDER_ITEMS;

    uint32_t attribute_count = 0;
    uint32_t attributes_to_copy = 0;

    switch (connection->attr_bitmap){
        case AVRCP_MEDIA_ATTR_NONE:
            attribute_count = AVRCP_MEDIA_ATTR_NONE; // 0xFFFFFFFF
            break;
        case AVRCP_MEDIA_ATTR_ALL:
            attribute_count = AVRCP_MEDIA_ATTR_ALL;  // 0
            break;
        default:
            attribute_count    = count_set_bits_uint32(connection->attr_bitmap & 0xff);
            attributes_to_copy = attribute_count;
            break;
    }
    big_endian_store_16(command, pos, 9 + 1 + attribute_count*4);
    pos += 2;
    
    command[pos++] = connection->scope;
    big_endian_store_32(command, pos, connection->start_item);
    pos += 4;
    big_endian_store_32(command, pos, connection->end_item);
    pos += 4;
    command[pos++] = attribute_count;
    
    int bit_position = 1;
    while (attributes_to_copy){
        if (connection->attr_bitmap & (1 << bit_position)){
            big_endian_store_32(command, pos, bit_position);
            pos += 4;
            attributes_to_copy--;
        }
        bit_position++;
    }
    return l2cap_send(cid, command, pos);
}


static int avrcp_browsing_controller_send_get_item_attributes_cmd(uint16_t cid, avrcp_browsing_connection_t * connection){
    uint8_t command[100];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_PDU_ID_GET_ITEM_ATTRIBUTES;

    uint32_t attribute_count;
    uint32_t attributes_to_copy = 0;

    switch (connection->attr_bitmap){
        case AVRCP_MEDIA_ATTR_NONE:
        case AVRCP_MEDIA_ATTR_ALL:
            attribute_count = 0;
            break;
        default:
            attribute_count = count_set_bits_uint32(connection->attr_bitmap & 0xff);
            attributes_to_copy = attribute_count;
            break;
    }
    
    big_endian_store_16(command, pos, 12 + attribute_count*4);
    pos += 2;

    command[pos++] = connection->scope;
    memcpy(command+pos, connection->folder_uid, 8);
    pos += 8;
    big_endian_store_16(command, pos, connection->uid_counter);
    pos += 2;
    command[pos++] = attribute_count;
    
    int bit_position = 1;
    while (attributes_to_copy){
        if (connection->attr_bitmap & (1 << bit_position)){
            big_endian_store_32(command, pos, bit_position);
            pos += 4;
            attributes_to_copy--;
        }
        bit_position++;
    }
    
    return l2cap_send(cid, command, pos);
}


static int avrcp_browsing_controller_send_change_path_cmd(uint16_t cid, avrcp_browsing_connection_t * connection){
    uint8_t command[100];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_PDU_ID_CHANGE_PATH;

    big_endian_store_16(command, pos, 11);
    pos += 2;
    pos += 2;
    command[pos++] = connection->direction;
    memcpy(command+pos, connection->folder_uid, 8);
    pos += 8;
    return l2cap_send(cid, command, pos);
}

static int avrcp_browsing_controller_send_search_cmd(uint16_t cid, avrcp_browsing_connection_t * connection){
    uint8_t command[100];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_PDU_ID_SEARCH;

    big_endian_store_16(command, pos, 4 + connection->search_str_len);
    pos += 2;

    big_endian_store_16(command, pos, 0x006A);
    pos += 2;
    big_endian_store_16(command, pos, connection->search_str_len);
    pos += 2;

    memcpy(command+pos,connection->search_str, connection->search_str_len);
    pos += connection->search_str_len;
    return l2cap_send(cid, command, pos);
}

static int avrcp_browsing_controller_send_set_browsed_player_cmd(uint16_t cid, avrcp_browsing_connection_t * connection){
    uint8_t command[100];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_PDU_ID_SET_BROWSED_PLAYER;

    big_endian_store_16(command, pos, 2);
    pos += 2;
    big_endian_store_16(command, pos, connection->browsed_player_id);
    pos += 2;
    return l2cap_send(cid, command, pos);
}

static int avrcp_browsing_controller_send_get_total_nr_items_cmd(uint16_t cid, avrcp_browsing_connection_t * connection){
    uint8_t command[7];
    int pos = 0; 
    // transport header
    // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
    command[pos++] = (connection->transaction_label << 4) | (AVRCP_SINGLE_PACKET << 2) | (AVRCP_COMMAND_FRAME << 1) | 0;
    // Profile IDentifier (PID)
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL >> 8;
    command[pos++] = BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL & 0x00FF;
    command[pos++] = AVRCP_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS;

    big_endian_store_16(command, pos, 1);
    pos += 2;
    command[pos++] = connection->get_total_nr_items_scope;
    return l2cap_send(cid, command, pos);
}

static void avrcp_browsing_controller_handle_can_send_now(avrcp_browsing_connection_t * connection){
    switch (connection->state){
        case AVCTP_CONNECTION_OPENED:
            if (connection->set_browsed_player_id){
                connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                connection->set_browsed_player_id = 0;
                avrcp_browsing_controller_send_set_browsed_player_cmd(connection->l2cap_browsing_cid, connection);
                break;
            }            

            if (connection->get_total_nr_items){
                connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                connection->get_total_nr_items = 0;
                avrcp_browsing_controller_send_get_total_nr_items_cmd(connection->l2cap_browsing_cid, connection);
                break;
            }

            if (connection->get_folder_items){
                connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                connection->get_folder_items = 0;
                avrcp_browsing_controller_send_get_folder_items_cmd(connection->l2cap_browsing_cid, connection);
                break;
            }

            if (connection->get_item_attributes){
                connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                connection->get_item_attributes = 0;
                avrcp_browsing_controller_send_get_item_attributes_cmd(connection->l2cap_browsing_cid, connection);
                break;
            }
            
            if (connection->change_path){
                connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                connection->change_path = 0;
                avrcp_browsing_controller_send_change_path_cmd(connection->l2cap_browsing_cid, connection);
                break;
            }

            if (connection->search){
                connection->state = AVCTP_W2_RECEIVE_RESPONSE;
                connection->search = 0;
                avrcp_browsing_controller_send_search_cmd(connection->l2cap_browsing_cid, connection);
                break;   
            }
            break;
        default:
            return;
    }
}


static void avrcp_browsing_controller_emit_done_with_uid_counter(btstack_packet_handler_t callback, uint16_t browsing_cid, uint16_t uid_counter, uint8_t browsing_status, uint8_t bluetooth_status){
    if (!callback) return;
    uint8_t event[9];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_DONE;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    little_endian_store_16(event, pos, uid_counter);
    pos += 2;
    event[pos++] = browsing_status;
    event[pos++] = bluetooth_status;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_parser_reset(avrcp_browsing_connection_t * connection){
    connection->parser_attribute_header_pos = 0;
    connection->parsed_attribute_value_offset = 0;
    connection->parsed_num_attributes = 0;
    connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_HEADER;
}


static void avrcp_browsing_parser_process_byte(uint8_t byte, avrcp_browsing_connection_t * connection){
    uint8_t prepended_header_size = 1;
    switch(connection->parser_state){
        case AVRCP_PARSER_GET_ATTRIBUTE_HEADER:{
            connection->parser_attribute_header[connection->parser_attribute_header_pos++] = byte;
            if (connection->parser_attribute_header_pos < AVRCP_BROWSING_ITEM_HEADER_LEN) break;

            uint16_t attribute_total_value_len = big_endian_read_16(connection->parser_attribute_header, 1);
            connection->parsed_attribute_value[connection->parsed_attribute_value_offset++] = connection->parser_attribute_header[0];   // prepend with item type
            connection->parsed_attribute_value_len = btstack_min(attribute_total_value_len, AVRCP_MAX_ATTRIBUTTE_SIZE - prepended_header_size);                 // reduce AVRCP_MAX_ATTRIBUTTE_SIZE for the size ot item type
            connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_VALUE;
            // printf("AVRCP_PARSER_GET_ATTRIBUTE_HEADER value len %d, to parse %d, offset %d\n", attribute_total_value_len, connection->parsed_attribute_value_len, connection->parsed_attribute_value_offset);
            break;
        }
        case AVRCP_PARSER_GET_ATTRIBUTE_VALUE:{
            connection->parsed_attribute_value[connection->parsed_attribute_value_offset++] = byte;
            if (connection->parsed_attribute_value_offset < connection->parsed_attribute_value_len + prepended_header_size){
                break;
            }
            if (connection->parsed_attribute_value_offset < big_endian_read_16(connection->parser_attribute_header, 1)){
                connection->parser_state = AVRCP_PARSER_IGNORE_REST_OF_ATTRIBUTE_VALUE;
                break;
            }
            connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_HEADER;
            (*avrcp_controller_context.browsing_avrcp_callback)(AVRCP_BROWSING_DATA_PACKET, connection->l2cap_browsing_cid, &connection->parsed_attribute_value[0], connection->parsed_attribute_value_offset);
            connection->parsed_num_attributes++;
            connection->parsed_attribute_value_offset = 0;
            connection->parser_attribute_header_pos = 0;
                
            if (connection->parsed_num_attributes == connection->num_items){
                avrcp_parser_reset(connection);
                break;
            }
            break;
        }
        case AVRCP_PARSER_IGNORE_REST_OF_ATTRIBUTE_VALUE:
            connection->parsed_attribute_value_offset++;
            if (connection->parsed_attribute_value_offset < big_endian_read_16(connection->parser_attribute_header, 1) + prepended_header_size){
                break;
            }
            connection->parser_state = AVRCP_PARSER_GET_ATTRIBUTE_HEADER;
            (*avrcp_controller_context.browsing_avrcp_callback)(AVRCP_BROWSING_DATA_PACKET, connection->l2cap_browsing_cid, &connection->parsed_attribute_value[0], connection->parsed_attribute_value_offset);
            connection->parsed_num_attributes++;
            connection->parsed_attribute_value_offset = 0;
            connection->parser_attribute_header_pos = 0;
                
            if (connection->parsed_num_attributes == connection->num_items){
                avrcp_parser_reset(connection);
                break;
            }
            break;
        default:
            break;
    }
}

static void avrcp_browsing_parse_and_emit_element_attrs(uint8_t * packet, uint16_t num_bytes_to_read, avrcp_browsing_connection_t * connection){
    int i;
    for (i=0;i<num_bytes_to_read;i++){
        avrcp_browsing_parser_process_byte(packet[i], connection);
    }
}

static void avrcp_browsing_controller_emit_failed(btstack_packet_handler_t callback, uint16_t browsing_cid, uint8_t browsing_status, uint8_t bluetooth_status){
    avrcp_browsing_controller_emit_done_with_uid_counter(callback, browsing_cid, 0, browsing_status, bluetooth_status);
}


static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_browsing_connection_t * browsing_connection;
            
    switch (packet_type) {
        case L2CAP_DATA_PACKET:{                            
            browsing_connection = get_avrcp_browsing_connection_for_l2cap_cid(AVRCP_CONTROLLER, channel);
            if (!browsing_connection) break;
            // printf("received \n");
            // printf_hexdump(packet,size);
            int pos = 0;
            uint8_t transport_header = packet[pos++];
            // Transaction label | Packet_type | C/R | IPID (1 == invalid profile identifier)
            browsing_connection->transaction_label = transport_header >> 4;
            avrcp_packet_type_t avctp_packet_type = (transport_header & 0x0F) >> 2;
            // printf("L2CAP_DATA_PACKET, packet type %d\n", avctp_packet_type);
            switch (avctp_packet_type){
                case AVRCP_SINGLE_PACKET:
                case AVRCP_START_PACKET:
                    // uint8_t frame_type = (transport_header & 0x03) >> 1;
                    // uint8_t ipid = transport_header & 0x01;
                    pos += 2;
                    browsing_connection->num_packets = 1;
                    if (avctp_packet_type == AVRCP_START_PACKET){
                        browsing_connection->num_packets = packet[pos++];
                    } 
                    if (pos + 4 > size){
                        browsing_connection->state = AVCTP_CONNECTION_OPENED;
                        avrcp_browsing_controller_emit_failed(avrcp_controller_context.browsing_avrcp_callback, channel, AVRCP_BROWSING_ERROR_CODE_INVALID_COMMAND, ERROR_CODE_SUCCESS);
                        return;  
                    }
                    browsing_connection->pdu_id = packet[pos++];
                    // uint16_t length = big_endian_read_16(packet, pos);
                    pos += 2;
                    browsing_connection->browsing_status = packet[pos++]; 
                    if (browsing_connection->browsing_status != AVRCP_BROWSING_ERROR_CODE_SUCCESS){
                        browsing_connection->state = AVCTP_CONNECTION_OPENED;
                        avrcp_browsing_controller_emit_failed(avrcp_controller_context.browsing_avrcp_callback, channel, browsing_connection->browsing_status, ERROR_CODE_SUCCESS);
                        return;        
                    }
                    break;
                default:
                    break;
            }

            uint32_t i;
            switch(browsing_connection->pdu_id){
                case AVRCP_PDU_ID_CHANGE_PATH:
                    // printf("AVRCP_PDU_ID_CHANGE_PATH \n");
                    break;
                case AVRCP_PDU_ID_SET_ADDRESSED_PLAYER:
                    // printf("AVRCP_PDU_ID_SET_ADDRESSED_PLAYER \n");
                    break;
                case AVRCP_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS:{
                    // uint32_t num_items = big_endian_read_32(packet, pos);
                    // pos += 4;
                    // printf("TODO: send event, uid_counter %d, num_items %d\n", browsing_connection->uid_counter, num_items);
                    break;
                }
                case AVRCP_PDU_ID_SET_BROWSED_PLAYER:{
                    // printf("AVRCP_PDU_ID_SET_BROWSED_PLAYER \n");
                    browsing_connection->uid_counter =  big_endian_read_16(packet, pos);
                    pos += 2;
                    // uint32_t num_items = big_endian_read_32(packet, pos);
                    pos += 4;
                    // uint16_t charset = big_endian_read_16(packet, pos);
                    pos += 2;
                    uint8_t folder_depth = packet[pos++];

                    for (i = 0; i < folder_depth; i++){
                        uint16_t folder_name_length = big_endian_read_16(packet, pos);
                        pos += 2;
                        // reuse packet and add data type as a header
                        packet[pos-1] = AVRCP_BROWSING_MEDIA_ROOT_FOLDER;
                        (*avrcp_controller_context.browsing_avrcp_callback)(AVRCP_BROWSING_DATA_PACKET, channel, packet+pos-1, folder_name_length+1);
                        pos += folder_name_length;
                    }
                    break;
                }
                case AVRCP_PDU_ID_GET_FOLDER_ITEMS:{
                    // printf("AVRCP_PDU_ID_GET_FOLDER_ITEMS \n");
                    switch (avctp_packet_type){
                        case AVRCP_SINGLE_PACKET:
                        case AVRCP_START_PACKET:
                            avrcp_parser_reset(browsing_connection);
                            browsing_connection->uid_counter =  big_endian_read_16(packet, pos);
                            pos += 2;
                            browsing_connection->num_items = big_endian_read_16(packet, pos); //num_items
                            pos += 2;
                            avrcp_browsing_parse_and_emit_element_attrs(packet+pos, size-pos, browsing_connection);
                            break;
                        
                        case AVRCP_CONTINUE_PACKET:
                            avrcp_browsing_parse_and_emit_element_attrs(packet+pos, size-pos, browsing_connection);
                            break;
                        
                        case AVRCP_END_PACKET:
                            avrcp_browsing_parse_and_emit_element_attrs(packet+pos, size-pos, browsing_connection);
                            avrcp_parser_reset(browsing_connection);
                            break;
                    }
                    break;
                }                
                case AVRCP_PDU_ID_SEARCH:{
                    browsing_connection->uid_counter =  big_endian_read_16(packet, pos);
                    pos += 2;
                    // uint32_t num_items = big_endian_read_32(packet, pos);
                    // printf("TODO: send as event, search found %d items\n", num_items);
                    break;
                }
                case AVRCP_PDU_ID_GET_ITEM_ATTRIBUTES:
                    packet[pos-1] = AVRCP_BROWSING_MEDIA_ELEMENT_ITEM_ATTRIBUTE;
                    (*avrcp_controller_context.browsing_avrcp_callback)(AVRCP_BROWSING_DATA_PACKET, channel, packet+pos-1, size - pos + 1);
                    break;
                default:
                    log_info(" not parsed pdu ID 0x%02x", browsing_connection->pdu_id);
                    break;
            }

            switch (avctp_packet_type){
                case AVRCP_SINGLE_PACKET:
                case AVRCP_END_PACKET:
                    // printf("reset browsing connection state to OPENED\n");
                    browsing_connection->state = AVCTP_CONNECTION_OPENED;
                    avrcp_browsing_controller_emit_done_with_uid_counter(avrcp_controller_context.browsing_avrcp_callback, channel, browsing_connection->uid_counter, browsing_connection->browsing_status, ERROR_CODE_SUCCESS);
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
                    browsing_connection = get_avrcp_browsing_connection_for_l2cap_cid(AVRCP_CONTROLLER,channel);
                    if (!browsing_connection) break;
                    avrcp_browsing_controller_handle_can_send_now(browsing_connection);
                    break;
                default:
                    avrcp_browser_packet_handler(packet_type, channel, packet, size, &avrcp_controller_context);
                    break;
            }
            break;

        default:
            break;
    }
}

void avrcp_browsing_controller_init(void){
    avrcp_controller_context.browsing_packet_handler = avrcp_browsing_controller_packet_handler;
    l2cap_register_service(&avrcp_browsing_controller_packet_handler, PSM_AVCTP_BROWSING, 0xffff, LEVEL_2);
}

void avrcp_browsing_controller_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avrcp_browsing_controller_register_packet_handler called with NULL callback");
        return;
    }
    avrcp_controller_context.browsing_avrcp_callback = callback;
}

uint8_t avrcp_browsing_controller_connect(bd_addr_t bd_addr, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid){
    return avrcp_browsing_connect(bd_addr, ertm_buffer, size, ertm_config, avrcp_browsing_cid);
}

uint8_t avrcp_browsing_controller_disconnect(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (avrcp_connection->browsing_connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    
    l2cap_disconnect(avrcp_connection->browsing_connection->l2cap_browsing_cid, 0);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_controller_configure_incoming_connection(uint16_t avrcp_browsing_cid, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_decline_incoming_connection: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!avrcp_connection->browsing_connection){
        log_error("avrcp_browsing_controller_decline_incoming_connection: no browsing connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 

    if (avrcp_connection->browsing_connection->state != AVCTP_CONNECTION_W4_ERTM_CONFIGURATION){
        log_error("avrcp_browsing_controller_decline_incoming_connection: browsing connection in a wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    avrcp_connection->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    avrcp_connection->browsing_connection->ertm_buffer = ertm_buffer;
    avrcp_connection->browsing_connection->ertm_buffer_size = size;
    memcpy(&avrcp_connection->browsing_connection->ertm_config, ertm_config, sizeof(l2cap_ertm_config_t));
    l2cap_accept_ertm_connection(avrcp_connection->browsing_connection->l2cap_browsing_cid, &avrcp_connection->browsing_connection->ertm_config, avrcp_connection->browsing_connection->ertm_buffer, avrcp_connection->browsing_connection->ertm_buffer_size);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_controller_decline_incoming_connection(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_decline_incoming_connection: could not find a connection.");
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

uint8_t avrcp_browsing_controller_get_item_attributes_for_scope(uint16_t avrcp_browsing_cid, uint8_t * uid, uint16_t uid_counter, uint32_t attr_bitmap, avrcp_browsing_scope_t scope){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_get_item_attributes: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("avrcp_browsing_controller_get_item_attributes: connection in wrong state %d, expected %d.", connection->state, AVCTP_CONNECTION_OPENED);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->get_item_attributes = 1;
    connection->scope = scope;
    memcpy(connection->folder_uid, uid, 8);
    connection->uid_counter = uid_counter;
    connection->attr_bitmap = attr_bitmap;

    avrcp_request_can_send_now(avrcp_connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Retrieve a listing of the contents of a folder.
 * @param scope    0-player list, 1-virtual file system, 2-search, 3-now playing  
 * @param start_item
 * @param end_item
 * @param attribute_count
 * @param attribute_list
 **/
static uint8_t avrcp_browsing_controller_get_folder_items(uint16_t avrcp_browsing_cid, avrcp_browsing_scope_t scope, uint32_t start_item, uint32_t end_item, uint32_t attr_bitmap){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (connection->state != AVCTP_CONNECTION_OPENED) {
        log_error("avrcp_browsing_controller_get_folder_items: connection in wrong state %d, expected %d.", connection->state, AVCTP_CONNECTION_OPENED);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    connection->get_folder_items = 1;
    connection->scope = scope;
    connection->start_item = start_item;
    connection->end_item = end_item;
    connection->attr_bitmap = attr_bitmap;

    avrcp_request_can_send_now(avrcp_connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_controller_get_media_players(uint16_t avrcp_browsing_cid, uint32_t start_item, uint32_t end_item, uint32_t attr_bitmap){
    return avrcp_browsing_controller_get_folder_items(avrcp_browsing_cid, AVRCP_BROWSING_MEDIA_PLAYER_LIST, start_item, end_item, attr_bitmap);
}

uint8_t avrcp_browsing_controller_browse_file_system(uint16_t avrcp_browsing_cid, uint32_t start_item, uint32_t end_item, uint32_t attr_bitmap){
    // return avrcp_browsing_controller_get_folder_items(avrcp_browsing_cid, 1, 0, 0xFFFFFFFF, attr_bitmap);
    return avrcp_browsing_controller_get_folder_items(avrcp_browsing_cid, AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM, start_item, end_item, attr_bitmap);
}

uint8_t avrcp_browsing_controller_browse_media(uint16_t avrcp_browsing_cid, uint32_t start_item, uint32_t end_item, uint32_t attr_bitmap){
    // return avrcp_browsing_controller_get_folder_items(avrcp_browsing_cid, 2, 0, 0xFFFFFFFF, 0, NULL);
    return avrcp_browsing_controller_get_folder_items(avrcp_browsing_cid, AVRCP_BROWSING_SEARCH, start_item, end_item, attr_bitmap);
}

uint8_t avrcp_browsing_controller_browse_now_playing_list(uint16_t avrcp_browsing_cid, uint32_t start_item, uint32_t end_item, uint32_t attr_bitmap){
    return avrcp_browsing_controller_get_folder_items(avrcp_browsing_cid, AVRCP_BROWSING_NOW_PLAYING, start_item, end_item, attr_bitmap);
}


uint8_t avrcp_browsing_controller_set_browsed_player(uint16_t avrcp_browsing_cid, uint16_t browsed_player_id){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_change_path: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    if (connection->state != AVCTP_CONNECTION_OPENED){
        log_error("avrcp_browsing_controller_change_path: connection in wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->set_browsed_player_id = 1;
    connection->browsed_player_id = browsed_player_id;
    avrcp_request_can_send_now(avrcp_connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Retrieve a listing of the contents of a folder.
 * @param direction     0-folder up, 1-folder down    
 * @param folder_uid    8 bytes long
 **/
uint8_t avrcp_browsing_controller_change_path(uint16_t avrcp_browsing_cid, uint8_t direction, uint8_t * folder_uid){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_change_path: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    
    if (!connection || connection->state != AVCTP_CONNECTION_OPENED){
        log_error("avrcp_browsing_controller_change_path: connection in wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    if (!connection->browsed_player_id){
        log_error("avrcp_browsing_controller_change_path: no browsed player set.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->change_path = 1;
    connection->direction = direction;
    memset(connection->folder_uid, 0, 8);
    if (folder_uid){
        memcpy(connection->folder_uid, folder_uid, 8);
    }
    
    avrcp_request_can_send_now(avrcp_connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_controller_go_up_one_level(uint16_t avrcp_browsing_cid){
    return avrcp_browsing_controller_change_path(avrcp_browsing_cid, 0, NULL);
}

uint8_t avrcp_browsing_controller_go_down_one_level(uint16_t avrcp_browsing_cid, uint8_t * folder_uid){
    return avrcp_browsing_controller_change_path(avrcp_browsing_cid, 1, folder_uid);
}

uint8_t avrcp_browsing_controller_search(uint16_t avrcp_browsing_cid, uint16_t search_str_len, char * search_str){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_change_path: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    
    if (!connection || connection->state != AVCTP_CONNECTION_OPENED){
        log_error("avrcp_browsing_controller_change_path: connection in wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    if (!connection->browsed_player_id){
        log_error("avrcp_browsing_controller_change_path: no browsed player set.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (!search_str || search_str_len == 0){
        return AVRCP_BROWSING_ERROR_CODE_INVALID_COMMAND;
    }

    connection->search = 1;
  
    connection->search_str_len = btstack_min(search_str_len, sizeof(connection->search_str)-1);
    memset(connection->search_str, 0, sizeof(connection->search_str));
    memcpy(connection->search_str, search_str, connection->search_str_len);
    avrcp_request_can_send_now(avrcp_connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_controller_get_total_nr_items_for_scope(uint16_t avrcp_browsing_cid, avrcp_browsing_scope_t scope){
    avrcp_connection_t * avrcp_connection = get_avrcp_connection_for_browsing_cid(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!avrcp_connection){
        log_error("avrcp_browsing_controller_change_path: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    avrcp_browsing_connection_t * connection = avrcp_connection->browsing_connection;
    
    if (!connection || connection->state != AVCTP_CONNECTION_OPENED){
        log_error("avrcp_browsing_controller_change_path: connection in wrong state.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    if (!connection->browsed_player_id){
        log_error("avrcp_browsing_controller_change_path: no browsed player set.");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection->get_total_nr_items = 1;
    connection->get_total_nr_items_scope = scope;
    avrcp_request_can_send_now(avrcp_connection, connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}
