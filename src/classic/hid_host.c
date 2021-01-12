/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hid_host.c"

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_hid_parser.h"
#include "btstack_memory.h"
#include "classic/hid.h"
#include "classic/hid_host.h"
#include "classic/sdp_util.h"
#include "classic/sdp_client.h"
#include "l2cap.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300

#define CONTROL_MESSAGE_BITMASK_SUSPEND             1
#define CONTROL_MESSAGE_BITMASK_EXIT_SUSPEND        2
#define CONTROL_MESSAGE_BITMASK_VIRTUAL_CABLE_UNPLUG 4

// Simplified US Keyboard with Shift modifier

#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      27
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   0x7f
#define NUM_KEYS 6

// English (US)
//
static const uint8_t keytable_us_none [] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             //   0-3  
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',                   //  4-13  
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',                   // 14-23  
    'u', 'v', 'w', 'x', 'y', 'z',                                       // 24-29  
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',                   // 30-39  
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            // 40-44  
    '-', '=', '[', ']', '\\', CHAR_ILLEGAL, ';', '\'', 0x60, ',',       // 45-54  
    '.', '/', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   // 55-60  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 61-64  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 65-68  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 69-72  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 73-76  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 77-80  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 81-84  
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       // 85-97  
    '6', '7', '8', '9', '0', '.', 0xa7,                                 // 97-100 
}; 

static const uint8_t keytable_us_shift[] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             //  0-3   
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',                   //  4-13  
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',                   // 14-23  
    'U', 'V', 'W', 'X', 'Y', 'Z',                                       // 24-29  
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',                   // 30-39  
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            // 40-44  
    '_', '+', '{', '}', '|', CHAR_ILLEGAL, ':', '"', 0x7E, '<',         // 45-54  
    '>', '?', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   // 55-60  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 61-64  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 65-68  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 69-72  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 73-76  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 77-80  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 81-84  
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       // 85-97  
    '6', '7', '8', '9', '0', '.', 0xb1,                                 // 97-100 
}; 


static uint8_t last_keys[NUM_KEYS];

static uint8_t * hid_host_descriptor_storage;
static uint16_t hid_host_descriptor_storage_len;

// SDP
static uint8_t            attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;
static uint16_t           sdp_query_context_hid_host_control_cid = 0;

static btstack_linked_list_t connections;
static uint16_t hid_host_cid_counter = 0;

static btstack_packet_handler_t hid_callback;
static btstack_context_callback_registration_t hid_host_handle_sdp_client_query_request;
static void hid_host_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t hid_descriptor_storage_get_available_space(void){
    // assumes all descriptors are back to back
    uint16_t free_space = hid_host_descriptor_storage_len;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        free_space -= connection->hid_descriptor_len;
    }
    return free_space;
}

static void hid_descriptor_storage_init(hid_host_connection_t * connection){
    connection->hid_descriptor_len = 0;
    connection->hid_descriptor_max_len = hid_descriptor_storage_get_available_space();
    connection->hid_descriptor_offset = hid_host_descriptor_storage_len - connection->hid_descriptor_max_len;
}

static bool hid_descriptor_storage_store(hid_host_connection_t * connection, uint8_t byte){
    if (connection->hid_descriptor_len >= connection->hid_descriptor_max_len) return false;

    hid_host_descriptor_storage[connection->hid_descriptor_offset + connection->hid_descriptor_len] = byte;
    connection->hid_descriptor_len++;
    return true;
}

static void hid_descriptor_storage_delete(hid_host_connection_t * connection){
    uint16_t next_offset = connection->hid_descriptor_offset + connection->hid_descriptor_len;

    memmove(&hid_host_descriptor_storage[connection->hid_descriptor_offset], 
            &hid_host_descriptor_storage[next_offset],
            hid_host_descriptor_storage_len - next_offset);

    connection->hid_descriptor_len = 0;
    connection->hid_descriptor_offset = 0;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * conn = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if (conn->hid_descriptor_offset >= next_offset){
            conn->hid_descriptor_offset = next_offset;
            next_offset += conn->hid_descriptor_len;
        }
    }
}

static const uint8_t * hid_descriptor_storage_get_descriptor_data(hid_host_connection_t * connection){
    return &hid_host_descriptor_storage[connection->hid_descriptor_offset];
}

static const uint16_t hid_descriptor_storage_get_descriptor_len(hid_host_connection_t * connection){
    return connection->hid_descriptor_len;
}


// HID Util
static void hid_emit_connected_event(hid_host_connection_t * context, uint8_t status){
    uint8_t event[15];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = HID_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event, pos, context->hid_cid);
    pos+=2;
    event[pos++] = status;
    reverse_bd_addr(context->remote_addr, &event[pos]);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    hid_callback(HCI_EVENT_PACKET, context->hid_cid, &event[0], pos);
}   

static void hid_emit_event(hid_host_connection_t * context, uint8_t subevent_type){
    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = subevent_type;
    little_endian_store_16(event,pos,context->hid_cid);
    pos += 2;
    event[1] = pos - 2;
    hid_callback(HCI_EVENT_PACKET, context->hid_cid, &event[0], pos);
}

static void hid_emit_incoming_connection_event(hid_host_connection_t * context){
    uint8_t event[13];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = HID_SUBEVENT_INCOMING_CONNECTION;
    little_endian_store_16(event, pos, context->hid_cid);
    pos += 2;
    reverse_bd_addr(context->remote_addr, &event[pos]);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[1] = pos - 2;
    hid_callback(HCI_EVENT_PACKET, context->hid_cid, &event[0], pos);
}   

// setup get report response event - potentially in-place of original l2cap packet
static void hid_setup_get_report_event(hid_host_connection_t * context, hid_handshake_param_type_t status, uint8_t *buffer, uint16_t report_len){
    uint16_t pos = 0;
    buffer[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    buffer[pos++] = HID_SUBEVENT_GET_REPORT_RESPONSE;
    little_endian_store_16(buffer, pos, context->hid_cid);
    pos += 2;
    buffer[pos++] = (uint8_t) status;
    little_endian_store_16(buffer, pos, report_len);
    pos += 2;
    buffer[1] = pos + report_len - 2;
}

// HID Host

static uint16_t hid_host_get_next_cid(void){
    if (hid_host_cid_counter == 0xffff) {
        hid_host_cid_counter = 1;
    } else {
        hid_host_cid_counter++;
    }
    return hid_host_cid_counter;
}

static hid_host_connection_t * hid_host_create_connection(bd_addr_t remote_addr){
    hid_host_connection_t * connection = btstack_memory_hid_host_connection_get();
    if (!connection){
        log_error("Not enough memory to create connection");
        return NULL;
    }
    connection->state = HID_HOST_IDLE;
    connection->hid_cid = hid_host_get_next_cid();
    connection->control_cid = 0;
    connection->control_psm = 0;
    connection->interrupt_cid = 0;
    connection->interrupt_psm = 0;
    connection->con_handle = HCI_CON_HANDLE_INVALID;

    (void)memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&connections, (btstack_linked_item_t *) connection);
    return connection;
}

static hid_host_connection_t * hid_host_get_connection_for_bd_addr(bd_addr_t addr){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

static hid_host_connection_t * hid_host_get_connection_for_hid_cid(uint16_t hid_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->hid_cid != hid_cid) continue;
        return connection;
    }
    return NULL;
}

static hid_host_connection_t * hid_host_get_connection_for_l2cap_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if ((connection->interrupt_cid != l2cap_cid) && (connection->control_cid != l2cap_cid)) continue;
        return connection;
    }
    return NULL;
}

static void hid_host_finalize_connection(hid_host_connection_t * connection){
    btstack_linked_list_remove(&connections, (btstack_linked_item_t*) connection); 
    btstack_memory_hid_host_connection_free(connection);
}

static void hid_host_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    des_iterator_t prot_it;
    uint8_t       *des_element;
    uint8_t       *element;
    uint32_t       uuid;
    uint8_t        status = ERROR_CODE_SUCCESS;
    
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(sdp_query_context_hid_host_control_cid);
    if (!connection) {
        log_error("SDP query, connection with 0x%02x cid not found", sdp_query_context_hid_host_control_cid);
        return;
    }
 
    if (connection->state != HID_HOST_W4_SDP_QUERY_RESULT) return;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:

            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);

                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                des_iterator_init(&prot_it, des_element);
                                element = des_iterator_get_element(&prot_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        des_iterator_next(&prot_it);
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->control_psm);
                                        log_info("HID Control PSM: 0x%04x", (int) &connection->control_psm);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_DES) continue;
                                    des_element = des_iterator_get_element(&additional_des_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->interrupt_psm);
                                            log_info("HID Interrupt PSM: 0x%04x", (int) &connection->interrupt_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_STRING) continue;
                                    element = des_iterator_get_element(&additional_des_it);
                                    
                                    const uint8_t * descriptor = de_get_string(element);
                                    uint16_t descriptor_len = de_get_data_size(element);
                                    
                                    uint16_t i;
                                    for (i = 0; i < descriptor_len; i++){
                                        hid_descriptor_storage_store(connection, descriptor[i]);
                                    }

                                    // printf("Stored Descriptor %d\n", hid_descriptor_storage_get_descriptor_len(connection));
                                    // printf_hexdump(hid_descriptor_storage_get_descriptor_data(connection), hid_descriptor_storage_get_descriptor_len(connection));
                                }
                            }                        
                            break;
                        default:
                            break;
                    }
                }
            } else {
                fprintf(stderr, "SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            if (!connection->control_psm) {
                status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
                break;
            }
            if (!connection->interrupt_psm) {
                status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
                break;
            }
            
            connection->state = HID_HOST_W4_CONTROL_CONNECTION_ESTABLISHED;
            status = l2cap_create_channel(hid_host_packet_handler, connection->remote_addr, connection->control_psm, 48, &connection->control_cid);
            break;

        default:
            // bail out, we must have had an incoming connection in the meantime; just trigger next sdp query on complete
            if (hci_event_packet_get_type(packet) == SDP_EVENT_QUERY_COMPLETE){
                (void) sdp_client_register_query_callback(&hid_host_handle_sdp_client_query_request);
            }
            return;
    }

    if (status != ERROR_CODE_SUCCESS){
        hid_emit_connected_event(connection, status);
        hid_host_finalize_connection(connection);
        sdp_query_context_hid_host_control_cid = 0;
    }
}

static void hid_host_handle_interrupt_report(hid_host_connection_t * connection, const uint8_t * report, uint16_t report_len){
    // check if HID Input Report
    if (report_len < 1) return;
    if (*report != 0xa1) return; 
    report++;
    report_len--;
    
    printf("hid_host_handle_interrupt_report len %d\n", hid_descriptor_storage_get_descriptor_len(connection));
    printf_hexdump(hid_descriptor_storage_get_descriptor_data(connection), hid_descriptor_storage_get_descriptor_len(connection));

    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, 
        hid_descriptor_storage_get_descriptor_data(connection), 
        hid_descriptor_storage_get_descriptor_len(connection), 
        HID_REPORT_TYPE_INPUT, report, report_len);
    
    int shift = 0;
    uint8_t new_keys[NUM_KEYS];
    memset(new_keys, 0, sizeof(new_keys));
    int     new_keys_count = 0;
    while (btstack_hid_parser_has_more(&parser)){
        uint16_t usage_page;
        uint16_t usage;
        int32_t  value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);
        if (usage_page != 0x07) continue;   
        switch (usage){
            case 0xe1:
            case 0xe6:
                if (value){
                    shift = 1;
                }
                continue;
            case 0x00:
                continue;
            default:
                break;
        }
        if (usage >= sizeof(keytable_us_none)) continue;

        // store new keys
        new_keys[new_keys_count++] = usage;

        // check if usage was used last time (and ignore in that case)
        int i;
        for (i=0;i<NUM_KEYS;i++){
            if (usage == last_keys[i]){
                usage = 0;
            }
        }
        if (usage == 0) continue;

        uint8_t key;
        if (shift){
            key = keytable_us_shift[usage];
        } else {
            key = keytable_us_none[usage];
        }
        if (key == CHAR_ILLEGAL) continue;
        if (key == CHAR_BACKSPACE){ 
            printf("\b \b");    // go back one char, print space, go back one char again
            continue;
        }
        printf("%c", key);
    }
    memcpy(last_keys, new_keys, NUM_KEYS);
}


static void hid_host_handle_control_packet(hid_host_connection_t * connection, uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t param;
    hid_message_type_t         message_type;
    hid_handshake_param_type_t message_status;
    // hid_report_type_t          report_type;
    uint8_t * in_place_event;
    uint8_t status;

    message_type   = (hid_message_type_t)(packet[0] >> 4);
    if (message_type == HID_MESSAGE_TYPE_HID_CONTROL){
        param = packet[0] & 0x0F;
        switch ((hid_control_param_t)param){
            case HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG:
                hid_emit_event(connection, HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG);
                hid_host_disconnect(connection->hid_cid);
                return;
            default:
                break;
        }
    }

    switch (connection->state){
        case HID_HOST_W4_GET_REPORT_RESPONSE:
            switch (message_type){
                case HID_MESSAGE_TYPE_HANDSHAKE:{
                    uint8_t event[8];
                    message_status = (hid_handshake_param_type_t)(packet[0] & 0x0F);
                    hid_setup_get_report_event(connection, message_status, event, 0);
                    hid_callback(HCI_EVENT_PACKET, connection->hid_cid, event, sizeof(event));
                    break;
                }
                case HID_MESSAGE_TYPE_DATA:
                    // reuse hci+l2cap header - max 8 byte (7 bytes + 1 bytes overwriting hid header)
                    in_place_event = packet - 7;
                    hid_setup_get_report_event(connection, HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL, in_place_event, size-1);
                    hid_callback(HCI_EVENT_PACKET, connection->hid_cid, in_place_event, size + 7);
                    break;
                default:
                    break;
            }
            break;
        case HID_HOST_W4_SET_REPORT_RESPONSE:
            printf("HID_HOST_W4_SET_REPORT_RESPONSE \n");
            break;
        case HID_HOST_W4_GET_PROTOCOL_RESPONSE:
            printf("HID_HOST_W4_GET_PROTOCOL_RESPONSE \n");
            break;
        case HID_HOST_W4_SEND_REPORT_RESPONSE:
            printf("HID_HOST_W4_SEND_REPORT_RESPONSE\n");
            break;
        case HID_HOST_W4_SET_PROTOCOL_RESPONSE:
            printf("HID_HOST_W4_SET_PROTOCOL_RESPONSE \n");
            message_status = (hid_handshake_param_type_t)(packet[0] & 0x0F);
            switch (message_status){
                case HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL:
                    switch(connection->protocol_mode){
                        case HID_PROTOCOL_MODE_BOOT:{
                            status = l2cap_create_channel(hid_host_packet_handler, connection->remote_addr, connection->interrupt_psm, 48, &connection->interrupt_cid);
                            if (status){
                                printf("Connecting to HID Control failed: 0x%02x\n", status);
                                connection->state = HID_HOST_CONTROL_CONNECTION_ESTABLISHED;
                                break;
                            }
                            connection->state = HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED;
                            break;
                        }
                        case HID_PROTOCOL_MODE_REPORT:
                            break;
                        default:
                            break;
                    }
                default:
                    break;
            }
            break;
        default:
            printf("HID_MESSAGE_TYPE_DATA ???\n");
            break;
    }
    connection->state =  HID_HOST_CONNECTION_ESTABLISHED;
}


static void hid_host_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint8_t   event;
    bd_addr_t address;
    uint8_t   status;
    uint16_t  l2cap_cid;
    hid_host_connection_t * connection;

    switch (packet_type) {

        case L2CAP_DATA_PACKET:
            connection = hid_host_get_connection_for_l2cap_cid(channel);
            if (!connection) break;

            if (channel == connection->interrupt_cid){
                hid_host_handle_interrupt_report(connection, packet, size);
                break;
            }
            if (channel == connection->control_cid){
                hid_host_handle_control_packet(connection, packet, size);
                break;
            }
            break;

        case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, address); 
                    connection = hid_host_get_connection_for_bd_addr(address);
                    
                    switch (l2cap_event_incoming_connection_get_psm(packet)){
                        case PSM_HID_CONTROL:
                            if (connection){
                                l2cap_decline_connection(channel);
                                break; 
                            }
                        
                            connection = hid_host_create_connection(address);
                            if (!connection){
                                log_error("Cannot create connection for %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }

                            connection->state = HID_HOST_W4_CONTROL_CONNECTION_ESTABLISHED;
                            connection->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                            connection->control_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                            connection->incoming = true;
                            
                            hid_emit_incoming_connection_event(connection);
                            break;
                            
                        case PSM_HID_INTERRUPT:
                            if (!connection || (connection->interrupt_cid != 0) || (l2cap_event_incoming_connection_get_handle(packet) != connection->con_handle)){
                                log_error("Decline connection for %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }

                            connection->state = HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED;
                            connection->interrupt_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                            log_info("Accept connection on Interrupt channel %s", bd_addr_to_str(address));
                            l2cap_accept_connection(channel);
                            break;
                        
                        default:
                            log_info("Decline connection for %s", bd_addr_to_str(address));
                            l2cap_decline_connection(channel);
                            break;
                    }
                    break;

                case L2CAP_EVENT_CHANNEL_OPENED: 
                    l2cap_event_channel_opened_get_address(packet, address); 

                    status = l2cap_event_channel_opened_get_status(packet); 
                    if (status){
                        log_info("L2CAP connection %s failed: 0x%02xn", bd_addr_to_str(address), status);
                        break;
                    }

                    connection = hid_host_get_connection_for_bd_addr(address);
                    if (!connection){
                        log_error("Connection does not exist %s", bd_addr_to_str(address));
                        break;
                    }

                    // l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet);

                    switch (l2cap_event_channel_opened_get_psm(packet)){
                        case PSM_HID_CONTROL:
                            if (connection->state != HID_HOST_W4_CONTROL_CONNECTION_ESTABLISHED) break;
                            connection->state = HID_HOST_CONTROL_CONNECTION_ESTABLISHED;

                            if (connection->boot_mode){
                                 break;
                            } 

                            if (!connection->incoming){
                                status = l2cap_create_channel(hid_host_packet_handler, address, connection->interrupt_psm, 48, &connection->interrupt_cid);
                                if (status){
                                    log_info("Connecting to HID Interrupt failed: 0x%02x", status);
                                    break;
                                }
                                connection->con_handle = l2cap_event_channel_opened_get_handle(packet); 
                                connection->state = HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED;
                            }
                            break;

                        case PSM_HID_INTERRUPT:
                            if (connection->state != HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED) break;
                            if (connection->con_handle != l2cap_event_channel_opened_get_handle(packet)) break;
                            connection->state = HID_HOST_CONNECTION_ESTABLISHED;
                            log_info("HID host connection established, cids: control 0x%02x, interrupt 0x%02x interrupt, hid 0x%02x", 
                                connection->control_cid, connection->interrupt_cid, connection->hid_cid);
                            hid_emit_connected_event(connection, ERROR_CODE_SUCCESS);
                            break;

                        default:
                            break;
                    }
                    // disconnect?                    
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    l2cap_cid  = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = hid_host_get_connection_for_l2cap_cid(l2cap_cid);
                    if (!connection) return;
                    
                    if (l2cap_cid == connection->interrupt_cid){
                        connection->interrupt_cid = 0;
                        if (connection->state == HID_HOST_W4_INTERRUPT_CONNECTION_DISCONNECTED){
                            connection->state = HID_HOST_W4_CONTROL_CONNECTION_DISCONNECTED;
                            l2cap_disconnect(connection->control_cid, 0);
                        }
                        break;
                    }

                    if (l2cap_cid == connection->control_cid){
                        connection->control_cid = 0;
                        hid_emit_event(connection, HID_SUBEVENT_CONNECTION_CLOSED);
                        hid_descriptor_storage_delete(connection);
                        hid_host_finalize_connection(connection);
                        break;
                    }
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    l2cap_cid  = l2cap_event_can_send_now_get_local_cid(packet);
                    connection = hid_host_get_connection_for_l2cap_cid(l2cap_cid);
                    if (!connection) return;

                    if (connection->control_cid == l2cap_cid){
                        switch(connection->state){
                            case HID_HOST_CONNECTION_ESTABLISHED:
                                if ((connection->control_tasks & CONTROL_MESSAGE_BITMASK_SUSPEND) != 0){
                                    connection->control_tasks &= ~CONTROL_MESSAGE_BITMASK_SUSPEND;
                                    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_SUSPEND };
                                    l2cap_send(connection->control_cid, (uint8_t*) report, 1);
                                    break;
                                }
                                if ((connection->control_tasks & CONTROL_MESSAGE_BITMASK_EXIT_SUSPEND) != 0){
                                    connection->control_tasks &= ~CONTROL_MESSAGE_BITMASK_EXIT_SUSPEND;
                                    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_EXIT_SUSPEND };
                                    l2cap_send(connection->control_cid, (uint8_t*) report, 1);
                                    break;
                                }
                                if ((connection->control_tasks & CONTROL_MESSAGE_BITMASK_VIRTUAL_CABLE_UNPLUG) != 0){
                                    connection->control_tasks &= ~CONTROL_MESSAGE_BITMASK_VIRTUAL_CABLE_UNPLUG;
                                    uint8_t report[] = { (HID_MESSAGE_TYPE_HID_CONTROL << 4) | HID_CONTROL_PARAM_VIRTUAL_CABLE_UNPLUG };
                                    l2cap_send(connection->control_cid, (uint8_t*) report, 1);
                                    break;
                                }
                                break;

                            case HID_HOST_W2_SEND_GET_REPORT:{
                                uint8_t header = (HID_MESSAGE_TYPE_GET_REPORT << 4) | connection->report_type;
                                uint8_t report[] = {header, connection->report_id};
                                
                                connection->state = HID_HOST_W4_GET_REPORT_RESPONSE;
                                l2cap_send(connection->control_cid, (uint8_t*) report, sizeof(report));
                                break;
                            }
                            case HID_HOST_W2_SEND_SET_REPORT:{
                                uint8_t header = (HID_MESSAGE_TYPE_SET_REPORT << 4) | connection->report_type;
                                connection->state = HID_HOST_W4_SET_REPORT_RESPONSE;

                                l2cap_reserve_packet_buffer();
                                uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                                out_buffer[0] = header;
                                out_buffer[1] = connection->report_id;
                                (void)memcpy(out_buffer + 2, connection->report, connection->report_len);
                                if (connection->boot_mode){
                                    l2cap_send_prepared(connection->interrupt_cid, connection->report_len + 2);
                                } else {
                                    l2cap_send_prepared(connection->control_cid, connection->report_len + 2);
                                }
                                break;
                            }
                            case HID_HOST_W2_SEND_GET_PROTOCOL:{
                                uint8_t header = (HID_MESSAGE_TYPE_GET_PROTOCOL << 4);
                                uint8_t report[] = {header};
                                connection->state = HID_HOST_W4_GET_PROTOCOL_RESPONSE;
                                l2cap_send(connection->control_cid, (uint8_t*) report, sizeof(report));
                                break;   
                            }
                            case HID_HOST_W2_SEND_SET_PROTOCOL:{
                                uint8_t header = (HID_MESSAGE_TYPE_SET_PROTOCOL << 4) | connection->protocol_mode;
                                uint8_t report[] = {header};

                                connection->state = HID_HOST_W4_SET_PROTOCOL_RESPONSE;
                                l2cap_send(connection->control_cid, (uint8_t*) report, sizeof(report));
                                break;   
                            }
                            default:
                                break;
                        }
                    }

                    // printf("L2CAP_EVENT_CAN_SEND_NOW, hid_host.state = %d\n", connection->state);
                    switch(connection->state){
                        
                        case HID_HOST_W2_SEND_SET_REPORT:{
                            uint8_t header = (HID_MESSAGE_TYPE_SET_REPORT << 4) | connection->report_type;
                            connection->state = HID_HOST_W4_SET_REPORT_RESPONSE;

                            l2cap_reserve_packet_buffer();
                            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                            out_buffer[0] = header;
                            out_buffer[1] = connection->report_id;
                            (void)memcpy(out_buffer + 2, connection->report, connection->report_len);
                            if (connection->boot_mode){
                                l2cap_send_prepared(connection->interrupt_cid, connection->report_len + 2);
                            } else {
                                l2cap_send_prepared(connection->control_cid, connection->report_len + 2);
                            }
                            break;
                        }
                       
                        case HID_HOST_W2_SEND_REPORT:{
                            connection->state = HID_HOST_W4_SEND_REPORT_RESPONSE;
                            uint8_t header = (HID_MESSAGE_TYPE_DATA << 4) | connection->report_type;

                            l2cap_reserve_packet_buffer();
                            uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                            out_buffer[0] = header;
                            out_buffer[1] = connection->report_id;
                            (void)memcpy(out_buffer + 2, connection->report, connection->report_len);
                            l2cap_send_prepared(connection->interrupt_cid, connection->report_len + 2);
                        }
                        default:
                            break;
                    }

                    if (connection->control_tasks != 0){
                        l2cap_request_can_send_now_event(connection->control_cid);
                    }
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}


void hid_host_init(uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len){
    hid_host_descriptor_storage = hid_descriptor_storage;
    hid_host_descriptor_storage_len = hid_descriptor_storage_len;

    // register L2CAP Services for reconnections
    l2cap_register_service(hid_host_packet_handler, PSM_HID_INTERRUPT, 0xffff, gap_get_security_level());
    l2cap_register_service(hid_host_packet_handler, PSM_HID_CONTROL, 0xffff, gap_get_security_level());
}

void hid_host_register_packet_handler(btstack_packet_handler_t callback){
    hid_callback = callback;
}


static void hid_host_handle_start_sdp_client_query(void * context){
    UNUSED(context);

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);

        switch (connection->state){
             case HID_HOST_W2_SEND_SDP_QUERY:
                connection->state = HID_HOST_W4_SDP_QUERY_RESULT;
                break;
            default:
                continue;
        }
        hid_descriptor_storage_init(connection);
        sdp_query_context_hid_host_control_cid = connection->hid_cid;
        sdp_client_query_uuid16(&hid_host_handle_sdp_client_query_result, (uint8_t *) connection->remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
        return;
    }
}

uint8_t hid_host_accept_connection(uint16_t hid_cid){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection) return ERROR_CODE_COMMAND_DISALLOWED;
    l2cap_accept_connection(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_decline_connection(uint16_t hid_cid){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection) return ERROR_CODE_COMMAND_DISALLOWED;
    l2cap_decline_connection(connection->control_cid);
    hid_host_finalize_connection(connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_connect(bd_addr_t remote_addr, hid_protocol_mode_t protocol_mode, uint16_t * hid_cid){
    if (hid_cid == NULL) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    hid_host_connection_t * connection = hid_host_get_connection_for_bd_addr(remote_addr);
    if (connection){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection = hid_host_create_connection(remote_addr);
    if (!connection) return BTSTACK_MEMORY_ALLOC_FAILED;

    *hid_cid = connection->hid_cid;
    
    connection->state = HID_HOST_W2_SEND_SDP_QUERY;
    connection->incoming = false;
    connection->protocol_mode = protocol_mode;

    hid_host_handle_sdp_client_query_request.callback = &hid_host_handle_start_sdp_client_query;
    // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
    (void) sdp_client_register_query_callback(&hid_host_handle_sdp_client_query_request);
    return ERROR_CODE_SUCCESS;
}


void hid_host_disconnect(uint16_t hid_cid){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection) return;

    switch (connection->state){
        case HID_HOST_IDLE:
        case HID_HOST_W4_CONTROL_CONNECTION_DISCONNECTED:
        case HID_HOST_W4_INTERRUPT_CONNECTION_DISCONNECTED:
            return;
        default:
            break;
    }
    
    if (connection->interrupt_cid){
        connection->state = HID_HOST_W4_INTERRUPT_CONNECTION_DISCONNECTED;
        l2cap_disconnect(connection->interrupt_cid, 0);  // reason isn't used
        return;
    }

    if (connection->control_cid){
        connection->state = HID_HOST_W4_CONTROL_CONNECTION_DISCONNECTED;
        l2cap_disconnect(connection->control_cid, 0);  // reason isn't used
        return;
    } 
}


static inline uint8_t hid_host_send_control_message(uint16_t hid_cid, uint8_t control_message_bitmask){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection || !connection->control_cid) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;

    if (connection->state < HID_HOST_CONTROL_CONNECTION_ESTABLISHED) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (connection->state >= HID_HOST_W4_INTERRUPT_CONNECTION_DISCONNECTED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->control_tasks |= control_message_bitmask;
    l2cap_request_can_send_now_event(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_send_suspend(uint16_t hid_cid){
    return hid_host_send_control_message(hid_cid, CONTROL_MESSAGE_BITMASK_SUSPEND);
}

uint8_t hid_host_send_exit_suspend(uint16_t hid_cid){
    return hid_host_send_control_message(hid_cid, CONTROL_MESSAGE_BITMASK_EXIT_SUSPEND);
}

uint8_t hid_host_send_virtual_cable_unplug(uint16_t hid_cid){
    return hid_host_send_control_message(hid_cid, CONTROL_MESSAGE_BITMASK_VIRTUAL_CABLE_UNPLUG);
}


void hid_host_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    UNUSED(message);
    UNUSED(message_len);
}

static uint8_t hid_host_send_get_report(uint16_t hid_cid,  hid_report_type_t report_type, uint8_t report_id){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);

    if (!connection || !connection->control_cid){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 
    if (connection->state != HID_HOST_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->state = HID_HOST_W2_SEND_GET_REPORT;
    connection->report_type = report_type;
    connection->report_id = report_id;

    l2cap_request_can_send_now_event(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_send_get_output_report(uint16_t hid_cid, uint8_t report_id){
    return hid_host_send_get_report(hid_cid, HID_REPORT_TYPE_OUTPUT, report_id);
}

uint8_t hid_host_send_get_feature_report(uint16_t hid_cid, uint8_t report_id){
    return hid_host_send_get_report(hid_cid, HID_REPORT_TYPE_FEATURE, report_id);
}

uint8_t hid_host_send_get_input_report(uint16_t hid_cid, uint8_t report_id){
    return hid_host_send_get_report(hid_cid, HID_REPORT_TYPE_INPUT, report_id);
}

static uint8_t hid_host_send_set_report(uint16_t hid_cid, hid_report_type_t report_type, uint8_t report_id, uint8_t * report, uint8_t report_len){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);

    if (!connection || !connection->control_cid){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 

    if (connection->state != HID_HOST_CONNECTION_ESTABLISHED){
        printf("hid_host_send_set_report: unexpected state 0%02x\n", HID_HOST_CONNECTION_ESTABLISHED);
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->state = HID_HOST_W2_SEND_SET_REPORT;
    connection->report_type = report_type;
    connection->report_id = report_id;
    connection->report = report;
    connection->report_len = report_len;

    l2cap_request_can_send_now_event(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_send_set_output_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_set_report(hid_cid, HID_REPORT_TYPE_OUTPUT, report_id, report, report_len);
}

uint8_t hid_host_send_set_feature_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_set_report(hid_cid, HID_REPORT_TYPE_FEATURE, report_id, report, report_len);
}

uint8_t hid_host_send_set_input_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_set_report(hid_cid, HID_REPORT_TYPE_INPUT, report_id, report, report_len);
}

uint8_t hid_host_send_get_protocol(uint16_t hid_cid){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection || !connection->control_cid) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    if (connection->state != HID_HOST_CONNECTION_ESTABLISHED) return ERROR_CODE_COMMAND_DISALLOWED;

    connection->state = HID_HOST_W2_SEND_GET_PROTOCOL;

    l2cap_request_can_send_now_event(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_send_set_protocol_mode(uint16_t hid_cid, hid_protocol_mode_t protocol_mode){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection || !connection->control_cid) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    if (connection->state != HID_HOST_CONTROL_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->state = HID_HOST_W2_SEND_SET_PROTOCOL;
    connection->protocol_mode = protocol_mode;

    l2cap_request_can_send_now_event(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}


static uint8_t hid_host_send_report(uint16_t hid_cid,  hid_report_type_t report_type, uint8_t report_id, uint8_t * report, uint8_t report_len){
    hid_host_connection_t * connection = hid_host_get_connection_for_hid_cid(hid_cid);
    if (!connection || !connection->control_cid){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 
    if (connection->state != HID_HOST_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    connection->state = HID_HOST_W2_SEND_REPORT;
    connection->report_type = report_type;
    connection->report_id = report_id;
    connection->report = report;
    connection->report_len = report_len;

    l2cap_request_can_send_now_event(connection->control_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t hid_host_send_output_report(uint16_t hid_cid, uint8_t report_id, uint8_t * report, uint8_t report_len){
    return hid_host_send_report(hid_cid, HID_REPORT_TYPE_OUTPUT, report_id, report, report_len);
}

