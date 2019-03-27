/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "map_client.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "bluetooth_sdp.h"
#include "classic/sdp_client_rfcomm.h"
#include "btstack_event.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"

#include "classic/obex.h"
#include "classic/obex_iterator.h"
#include "classic/goep_client.h"
#include "map_client.h"

#define MAP_MAX_NUM_ENTRIES 1024

// map access service bb582b40-420c-11db-b0de-0800200c9a66
static const uint8_t map_client_access_service_uuid[] = {0xbb, 0x58, 0x2b, 0x40, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66};
static uint32_t map_supported_features = 0x1F;

typedef enum {
    MAP_INIT = 0,
    MAP_W4_GOEP_CONNECTION,
    MAP_W2_SEND_CONNECT_REQUEST,
    MAP_W4_CONNECT_RESPONSE,
    MAP_CONNECT_RESPONSE_RECEIVED,
    MAP_CONNECTED,
    
    MAP_W2_SET_PATH_ROOT,
    MAP_W4_SET_PATH_ROOT_COMPLETE,
    MAP_W2_SET_PATH_ELEMENT,
    MAP_W4_SET_PATH_ELEMENT_COMPLETE,

    MAP_W2_SEND_GET_FOLDERS,
    MAP_W4_FOLDERS,
    MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER,
    MAP_W4_MESSAGES_IN_FOLDER,
    MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE,
    MAP_W4_MESSAGE,
    MAP_W2_SET_NOTIFICATION,
    MAP_W4_SET_NOTIFICATION,
    
    MAP_W2_SEND_DISCONNECT_REQUEST,
    MAP_W4_DISCONNECT_RESPONSE,
} map_state_t;

typedef struct map_client {
    map_state_t state;
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    uint16_t  goep_cid;
    btstack_packet_handler_t client_handler;

    const char * folder_name;
    const char * current_folder;
    uint16_t set_path_offset;
    uint8_t  notifications_enabled;
    
    map_message_handle_t message_handle; 
    uint8_t get_message_attachment;
} map_client_t;

static map_client_t _map_client;
static map_client_t * map_client = &_map_client;

static void map_client_emit_connected_event(map_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}  

static void map_client_emit_connection_closed_event(map_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static void map_client_emit_operation_complete_event(map_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void map_message_handle_to_str(char * p, const map_message_handle_t msg_handle){
    int i;
    for (i = 0; i < MAP_MESSAGE_HANDLE_SIZE ; i++) {
        uint8_t byte = msg_handle[i];
        *p++ = char_for_nibble(byte >> 4);
        *p++ = char_for_nibble(byte & 0x0F);
    }
    *p = 0;
}

static void map_handle_can_send_now(void){
    uint8_t application_parameters[6];
    int pos = 0;
    char map_message_handle_to_str_buffer[MAP_MESSAGE_HANDLE_SIZE * 2 + 1]; 
    uint8_t  path_element[20];
    uint16_t path_element_start;
    uint16_t path_element_len;
    int done;

    switch (map_client->state){
        case MAP_W2_SEND_CONNECT_REQUEST:
            goep_client_request_create_connect(map_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(map_client->goep_cid, map_client_access_service_uuid, 16);
            // Mandatory if the PSE advertises a PbapSupportedFeatures attribute in its SDP record, else excluded.
            // if (goep_client_get_map_supported_features(map_client->goep_cid) != MAP_FEATURES_NOT_PRESENT){
                application_parameters[0] = 0x29; // MAP_APPLICATION_PARAMETER_MAP_SUPPORTED_FEATURES;
                application_parameters[1] = 4;
                big_endian_store_32(application_parameters, 2, map_supported_features);
                goep_client_header_add_application_parameters(map_client->goep_cid, &application_parameters[0], 6);

            // }
            map_client->state = MAP_W4_CONNECT_RESPONSE;
            goep_client_execute(map_client->goep_cid);
            break;
        
        case MAP_W2_SEND_DISCONNECT_REQUEST:
            goep_client_request_create_disconnect(map_client->goep_cid);
            map_client->state = MAP_W4_DISCONNECT_RESPONSE;
            goep_client_execute(map_client->goep_cid);
            break;
        
        case MAP_W2_SET_PATH_ROOT:
            goep_client_request_create_set_path(map_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(map_client->goep_cid, "");
            // state
            map_client->state = MAP_W4_SET_PATH_ROOT_COMPLETE;
            // send packet
            goep_client_execute(map_client->goep_cid);
            break;
        case MAP_W2_SET_PATH_ELEMENT:
            // find '/' or '\0'
            path_element_start = map_client->set_path_offset;
            while (map_client->current_folder[map_client->set_path_offset] != '\0' &&
                map_client->current_folder[map_client->set_path_offset] != '/'){
                map_client->set_path_offset++;              
            }
            path_element_len = map_client->set_path_offset-path_element_start;
            memcpy(path_element, &map_client->current_folder[path_element_start], path_element_len);
            path_element[path_element_len] = 0;

            // skip /
            if (map_client->current_folder[map_client->set_path_offset] == '/'){
                map_client->set_path_offset++;  
            }

            // done?
            done = map_client->current_folder[map_client->set_path_offset] == '\0';

            // status
            log_info("Path element '%s', done %u", path_element, done);

            goep_client_request_create_set_path(map_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(map_client->goep_cid, (const char *) path_element); // next element
            // state
            map_client->state = MAP_W4_SET_PATH_ELEMENT_COMPLETE;
            // send packet
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SEND_GET_FOLDERS:
            goep_client_request_create_get(map_client->goep_cid);
            goep_client_header_add_type(map_client->goep_cid, "x-obex/folder-listing");
            map_client->state = MAP_W4_FOLDERS;
            goep_client_execute(map_client->goep_cid);
            break;
        
        case MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER:
            goep_client_request_create_get(map_client->goep_cid);
            goep_client_header_add_type(map_client->goep_cid, "x-bt/MAP-msg-listing");
            goep_client_header_add_name(map_client->goep_cid, map_client->folder_name);
            map_client->state = MAP_W4_MESSAGES_IN_FOLDER;
            goep_client_execute(map_client->goep_cid);
            break;
        
        case MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE:
            goep_client_request_create_get(map_client->goep_cid);
            
            map_message_handle_to_str(map_message_handle_to_str_buffer, map_client->message_handle);
            goep_client_header_add_name(map_client->goep_cid, map_message_handle_to_str_buffer);
            
            goep_client_header_add_type(map_client->goep_cid, "x-bt/message");
            
            application_parameters[pos++] = 0x0A; // attachment
            application_parameters[pos++] = 1;
            application_parameters[pos++] = map_client->get_message_attachment;
            
            application_parameters[pos++] = 0x14; // Charset
            application_parameters[pos++] = 1;
            application_parameters[pos++] = 1;    // UTF-8
            goep_client_header_add_application_parameters(map_client->goep_cid, &application_parameters[0], 6);

            map_client->state = MAP_W4_MESSAGE;
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SET_NOTIFICATION:
            goep_client_request_create_put(map_client->goep_cid);
            goep_client_header_add_type(map_client->goep_cid, "x-bt/MAP-NotificationRegistration");

            application_parameters[pos++] = 0x0E; // NotificationStatus
            application_parameters[pos++] = 1;
            application_parameters[pos++] = map_client->notifications_enabled;

            goep_client_header_add_application_parameters(map_client->goep_cid, &application_parameters[0], pos);
            goep_client_body_add_static(map_client->goep_cid, (uint8_t *) "0", 1);
            map_client->state = MAP_W4_MESSAGE;
            goep_client_execute(map_client->goep_cid);
            break;
        default:
            break;
    }
}

static void map_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); 
    UNUSED(size);    
    // printf("packet_type 0x%02x, event type 0x%02x, subevent 0x%02x\n", packet_type, hci_event_packet_get_type(packet), hci_event_goep_meta_get_subevent_code(packet));
    obex_iterator_t it;
    uint8_t status;
    // int i;
    // int wait_for_user = 0;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_GOEP_META:
                    switch (hci_event_goep_meta_get_subevent_code(packet)){
                        case GOEP_SUBEVENT_CONNECTION_OPENED:
                            status = goep_subevent_connection_opened_get_status(packet);
                            map_client->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                            map_client->incoming = goep_subevent_connection_opened_get_incoming(packet);
                            goep_subevent_connection_opened_get_bd_addr(packet, map_client->bd_addr); 
                            if (status){
                                log_info("map: connection failed %u", status);
                                map_client->state = MAP_INIT;
                                map_client_emit_connected_event(map_client, status);
                            } else {
                                log_info("map: connection established");
                                map_client->goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                                map_client->state = MAP_W2_SEND_CONNECT_REQUEST;
                                goep_client_request_can_send_now(map_client->goep_cid);
                            }
                            break;
                        case GOEP_SUBEVENT_CONNECTION_CLOSED:
                            if (map_client->state != MAP_CONNECTED){
                                // map_client_emit_operation_complete_event(map_client, OBEX_DISCONNECTED);
                            }
                            map_client->state = MAP_INIT;
                            map_client_emit_connection_closed_event(map_client);
                            break;
                        case GOEP_SUBEVENT_CAN_SEND_NOW:
                            map_handle_can_send_now();
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case GOEP_DATA_PACKET:
            switch (map_client->state){
                case MAP_W4_CONNECT_RESPONSE:
                    switch (packet[0]){
                        case OBEX_RESP_SUCCESS:
                            for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(map_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                                uint8_t hi = obex_iterator_get_hi(&it);
                                if (hi == OBEX_HEADER_CONNECTION_ID){
                                    goep_client_set_connection_id(map_client->goep_cid, obex_iterator_get_data_32(&it));
                                }
                            }
                            map_client->state = MAP_CONNECTED;
                            // map_client->vcard_selector_supported = map_supported_features & goep_client_get_map_supported_features(map_client->goep_cid) & MAP_SUPPORTED_FEATURES_VCARD_SELECTING;
                            map_client_emit_connected_event(map_client, 0);
                            break;
                        default:
                            log_info("map: obex connect failed, result 0x%02x", packet[0]);
                            map_client->state = MAP_INIT;
                            map_client_emit_connected_event(map_client, OBEX_CONNECT_FAILED);
                            break;                            
                    }
                    break;
                case MAP_W4_DISCONNECT_RESPONSE:
                    map_client->state = MAP_INIT;
                    goep_client_disconnect(map_client->goep_cid);
                    break;
                
                case MAP_W4_SET_PATH_ROOT_COMPLETE:
                case MAP_W4_SET_PATH_ELEMENT_COMPLETE:
                    if (packet[0] == OBEX_RESP_SUCCESS){
                        // more path?
                        if (map_client->current_folder[map_client->set_path_offset]){
                            map_client->state = MAP_W2_SET_PATH_ELEMENT;
                            goep_client_request_can_send_now(map_client->goep_cid);
                        } else {
                            map_client->current_folder = NULL;   
                            map_client->state = MAP_CONNECTED;
                            map_client_emit_operation_complete_event(map_client, 0);
                        }
                    } else if (packet[0] == OBEX_RESP_NOT_FOUND){
                        map_client->state = MAP_CONNECTED;
                        map_client_emit_operation_complete_event(map_client, OBEX_NOT_FOUND);
                    } else {
                        map_client->state = MAP_CONNECTED;
                        map_client_emit_operation_complete_event(map_client, OBEX_UNKNOWN_ERROR);
                    }
                    break;

                case MAP_W4_FOLDERS:
                    map_client->state = MAP_CONNECTED;
                    printf("Folders:\n");
                    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(map_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                        uint8_t hi = obex_iterator_get_hi(&it);
                        // uint16_t     data_len = obex_iterator_get_data_len(&it);
                        const uint8_t  * data = obex_iterator_get_data(&it);
                        switch (hi){
                            case OBEX_HEADER_BODY:
                            case OBEX_HEADER_END_OF_BODY:
                                printf("%s\n", data);
                                break;
                            default:
                                break;
                        }
                    }
                    printf("\n");
                    break;
                case MAP_W4_MESSAGES_IN_FOLDER:
                    map_client->state = MAP_CONNECTED;
                    printf("Messages:\n");
                    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(map_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                        uint8_t hi = obex_iterator_get_hi(&it);
                        // uint16_t     data_len = obex_iterator_get_data_len(&it);
                        const uint8_t  * data = obex_iterator_get_data(&it);
                        switch (hi){
                            case OBEX_HEADER_BODY:
                            case OBEX_HEADER_END_OF_BODY:
                                printf("%s\n", data);
                                break;
                            default:
                                break;
                        }
                    }
                    printf("\n");
                    break;
                case MAP_W4_MESSAGE:
                    map_client->state = MAP_CONNECTED;
                    printf("Message :\n");
                    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(map_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                        uint8_t hi = obex_iterator_get_hi(&it);
                        // uint16_t     data_len = obex_iterator_get_data_len(&it);
                        const uint8_t  * data = obex_iterator_get_data(&it);
                        switch (hi){
                            case OBEX_HEADER_BODY:
                            case OBEX_HEADER_END_OF_BODY:
                                printf("%s\n", data);
                                break;
                            default:
                                break;
                        }
                    }
                    printf("\n");
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void map_client_init(void){
    memset(map_client, 0, sizeof(map_client_t));
    map_client->state = MAP_INIT;
    map_client->cid = 1;
}

uint8_t map_client_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (map_client->state != MAP_INIT) return BTSTACK_MEMORY_ALLOC_FAILED;

    map_client->state = MAP_W4_GOEP_CONNECTION;
    map_client->client_handler = handler;
    
    uint8_t err = goep_client_create_connection(&map_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER, &map_client->goep_cid);
    *out_cid = map_client->cid;
    if (err) return err;
    return 0;
}

uint8_t map_client_disconnect(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_client_get_folder_listing(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_GET_FOLDERS;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_client_get_message_listing_for_folder(uint16_t map_cid, const char * folder_name){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER;
    map_client->folder_name = folder_name;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;    
}

uint8_t map_client_get_message_with_handle(uint16_t map_cid, const map_message_handle_t message_handle, uint8_t with_attachment){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE;
    memcpy(map_client->message_handle, message_handle, MAP_MESSAGE_HANDLE_SIZE);
    map_client->get_message_attachment = with_attachment;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;    
}

uint8_t map_client_set_path(uint16_t map_cid, const char * path){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SET_PATH_ROOT;
    map_client->current_folder = path;
    map_client->set_path_offset = 0;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;    
}

uint8_t map_client_enable_notifications(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SET_NOTIFICATION;
    map_client->notifications_enabled = 1;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;    
}

uint8_t map_client_disable_notifications(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SET_NOTIFICATION;
    map_client->notifications_enabled = 0;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;    
}