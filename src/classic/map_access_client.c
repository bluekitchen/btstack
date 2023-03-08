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

#define BTSTACK_FILE__ "map_client.c"

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
#include "classic/obex_parser.h"
#include "classic/goep_client.h"
#include "map_access_client.h"

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

typedef enum {
    SRM_DISABLED,
    SRM_W4_CONFIRM,
    SRM_ENABLED_BUT_WAITING,
    SRM_ENABLED
} srm_state_t;

typedef struct {
    uint8_t srm_value;
    uint8_t srmp_value;
} obex_srm_t;

typedef struct map_client {
    map_state_t state;
    uint16_t  map_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    uint16_t  goep_cid;
    btstack_packet_handler_t client_handler;

    int request_number;
    /* obex parser */
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];
    /* srm */
    obex_srm_t obex_srm;
    srm_state_t srm_state;

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
    little_endian_store_16(event,pos,context->map_cid);
    pos+=2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->map_cid, &event[0], pos);
}

static void map_client_emit_connection_closed_event(map_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->map_cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->map_cid, &event[0], pos);
}

static void map_client_emit_operation_complete_event(map_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->map_cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->map_cid, &event[0], pos);
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

static void obex_srm_init(obex_srm_t * obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

static void map_client_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    map_client_t * client = (map_client_t *) user_data;
    switch (header_id){
        case OBEX_HEADER_CONNECTION_ID:
            if (obex_parser_header_store(client->obex_header_buffer, sizeof(client->obex_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE){
                goep_client_set_connection_id(client->goep_cid, big_endian_read_32(client->obex_header_buffer, 0));
            }
            break;
        default:
            break;
    }
}

static void map_client_parser_callback_get_operation(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    map_client_t *client = (map_client_t *) user_data;
    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&client->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&client->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch(map_client->state){
                case MAP_W4_FOLDERS:
                case MAP_W4_MESSAGES_IN_FOLDER:
                case MAP_W4_MESSAGE:
                    client->client_handler(MAP_DATA_PACKET, client->map_cid, (uint8_t *) data_buffer, data_len);
                    break;

                default:
                    printf ("unexpected state: %d\n", map_client->state);
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static void map_client_prepare_srm_header(const map_client_t * client){
    if (goep_client_version_20_or_higher(client->goep_cid)){
        goep_client_header_add_srm_enable(client->goep_cid);
        map_client->srm_state = SRM_W4_CONFIRM;
    }
}

static void map_client_prepare_operation(map_client_t * client, uint8_t op){
    obex_parser_callback_t callback;

    switch (op) {
        case OBEX_OPCODE_GET:
        case OBEX_OPCODE_PUT:
        case OBEX_OPCODE_SETPATH:
            callback = map_client_parser_callback_get_operation;
            break;
        case OBEX_OPCODE_CONNECT:
            callback = map_client_parser_callback_connect;
            break;
        default:
            callback = NULL;
            break;
    }

    obex_parser_init_for_response(&client->obex_parser, op, callback, client);
    obex_srm_init(&client->obex_srm);
    client->obex_parser_waiting_for_response = true;
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
            // prepare response
            map_client_prepare_operation(map_client, OBEX_OPCODE_CONNECT);
            obex_srm_init(&map_client->obex_srm);
            map_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SEND_DISCONNECT_REQUEST:
            goep_client_request_create_disconnect(map_client->goep_cid);
            map_client->state = MAP_W4_DISCONNECT_RESPONSE;
            // prepare response
            map_client_prepare_operation(map_client, OBEX_OPCODE_DISCONNECT);
            // send packet
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SET_PATH_ROOT:
            goep_client_request_create_set_path(map_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(map_client->goep_cid, "");
            // state
            map_client->state = MAP_W4_SET_PATH_ROOT_COMPLETE;
            map_client_prepare_operation(map_client, OBEX_OPCODE_SETPATH);
            // send packet
            map_client->request_number++;
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
            map_client_prepare_operation(map_client, OBEX_OPCODE_SETPATH);
            // send packet
            map_client->request_number++;
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SEND_GET_FOLDERS:
            goep_client_request_create_get(map_client->goep_cid);
            if (map_client->request_number == 0){
                map_client_prepare_srm_header(map_client);
                goep_client_header_add_type(map_client->goep_cid, "x-obex/folder-listing");
            }
            map_client->state = MAP_W4_FOLDERS;
            map_client_prepare_operation(map_client, OBEX_OPCODE_GET);
            map_client->request_number++;
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER:
            goep_client_request_create_get(map_client->goep_cid);
            if (map_client->request_number == 0){
                map_client_prepare_srm_header(map_client);
                goep_client_header_add_type(map_client->goep_cid, "x-bt/MAP-msg-listing");
                goep_client_header_add_name(map_client->goep_cid, map_client->folder_name);
            }
            map_client->state = MAP_W4_MESSAGES_IN_FOLDER;
            map_client_prepare_operation(map_client, OBEX_OPCODE_GET);
            map_client->request_number++;
            goep_client_execute(map_client->goep_cid);
            break;

        case MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE:
            goep_client_request_create_get(map_client->goep_cid);

            if (map_client->request_number == 0){
                map_client_prepare_srm_header(map_client);

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
            }

            map_client->state = MAP_W4_MESSAGE;
            map_client_prepare_operation(map_client, OBEX_OPCODE_GET);
            map_client->request_number++;
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
            map_client->state = MAP_W4_SET_NOTIFICATION;
            map_client_prepare_operation(map_client, OBEX_OPCODE_PUT);
            map_client->request_number++;
            goep_client_execute(map_client->goep_cid);
            break;
        default:
            break;
    }
}


static void map_client_handle_srm_headers(map_client_t *context) {
    const obex_srm_t * obex_srm = &map_client->obex_srm;
    // Update SRM state based on SRM headers
    switch (context->srm_state){
        case SRM_W4_CONFIRM:
            switch (obex_srm->srm_value){
                case OBEX_SRM_ENABLE:
                    switch (obex_srm->srmp_value){
                        case OBEX_SRMP_WAIT:
                            context->srm_state = SRM_ENABLED_BUT_WAITING;
                            break;
                        default:
                            context->srm_state = SRM_ENABLED;
                            break;
                    }
                    break;
                default:
                    context->srm_state = SRM_DISABLED;
                    break;
            }
            break;
        case SRM_ENABLED_BUT_WAITING:
            switch (obex_srm->srmp_value){
                case OBEX_SRMP_WAIT:
                    context->srm_state = SRM_ENABLED_BUT_WAITING;
                    break;
                default:
                    context->srm_state = SRM_ENABLED;
                    break;
            }
            break;
        default:
            break;
    }
    log_info("SRM state %u", context->srm_state);
}


static void map_client_packet_handler_hci(uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t status;

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
                        map_client_emit_operation_complete_event(map_client, OBEX_DISCONNECTED);
                    }
                    map_client->state = MAP_INIT;
                    map_client_emit_connection_closed_event(map_client);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    map_handle_can_send_now();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void
map_client_packet_handler_goep (uint8_t *packet, uint16_t size){
    if (map_client->obex_parser_waiting_for_response == false) return;

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&map_client->obex_parser, packet, size);
    if (parser_state != OBEX_PARSER_OBJECT_STATE_COMPLETE){
        return;
    }

    map_client->obex_parser_waiting_for_response = false;
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&map_client->obex_parser, &op_info);

    switch (map_client->state){
        case MAP_W4_CONNECT_RESPONSE:
            switch (op_info.response_code){
                case OBEX_RESP_SUCCESS:
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_connected_event(map_client, ERROR_CODE_SUCCESS);
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
            if (op_info.response_code == OBEX_RESP_SUCCESS){
                // more path?
                if (map_client->current_folder[map_client->set_path_offset]){
                    map_client->state = MAP_W2_SET_PATH_ELEMENT;
                    goep_client_request_can_send_now(map_client->goep_cid);
                } else {
                    map_client->current_folder = NULL;
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_operation_complete_event(map_client, ERROR_CODE_SUCCESS);
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
        case MAP_W4_MESSAGES_IN_FOLDER:
        case MAP_W4_MESSAGE:
        case MAP_W4_SET_NOTIFICATION:
            switch (op_info.response_code) {
                case OBEX_RESP_CONTINUE:
                    map_client_handle_srm_headers(map_client);
                    if (map_client->srm_state == SRM_ENABLED) {
                        map_client_prepare_operation(map_client, OBEX_OPCODE_GET);
                        break;
                    }
                    map_client->state =
                       (map_client->state == MAP_W4_FOLDERS            ? MAP_W2_SEND_GET_FOLDERS :
                        map_client->state == MAP_W4_MESSAGES_IN_FOLDER ? MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER :
                        map_client->state == MAP_W4_MESSAGE            ? MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE :
                        MAP_CONNECTED);

                    goep_client_request_can_send_now(map_client->goep_cid);
                    break;
                case OBEX_RESP_SUCCESS:
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_operation_complete_event(map_client, ERROR_CODE_SUCCESS);
                    break;
                case OBEX_RESP_NOT_IMPLEMENTED:
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_operation_complete_event(map_client, OBEX_UNKNOWN_ERROR);
                    break;
                case OBEX_RESP_NOT_FOUND:
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_operation_complete_event(map_client, OBEX_NOT_FOUND);
                    break;
                case OBEX_RESP_UNAUTHORIZED:
                case OBEX_RESP_FORBIDDEN:
                case OBEX_RESP_NOT_ACCEPTABLE:
                case OBEX_RESP_UNSUPPORTED_MEDIA_TYPE:
                case OBEX_RESP_ENTITY_TOO_LARGE:
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_operation_complete_event(map_client, OBEX_NOT_ACCEPTABLE);
                    break;
                default:
                    log_info("unexpected obex response 0x%02x", op_info.response_code);
                    map_client->state = MAP_CONNECTED;
                    map_client_emit_operation_complete_event(map_client, OBEX_UNKNOWN_ERROR);
                    break;
            }
            break;

        default:
            btstack_unreachable();
            break;
    }
}

static void map_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    switch (packet_type){
        case HCI_EVENT_PACKET:
            map_client_packet_handler_hci (packet, size);
            break;
        case GOEP_DATA_PACKET:
            map_client_packet_handler_goep (packet, size);
            break;
        default:
            break;
    }
}

void map_access_client_init(void){
    memset(map_client, 0, sizeof(map_client_t));
    map_client->state = MAP_INIT;
    map_client->map_cid = 1;
}

uint8_t map_access_client_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (map_client->state != MAP_INIT) return BTSTACK_MEMORY_ALLOC_FAILED;

    map_client->state = MAP_W4_GOEP_CONNECTION;
    map_client->request_number = 0;
    map_client->client_handler = handler;

    uint8_t err = goep_client_create_connection(&map_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER, &map_client->goep_cid);
    *out_cid = map_client->map_cid;
    if (err) return err;
    return 0;
}

uint8_t map_access_client_disconnect(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_DISCONNECT_REQUEST;
    map_client->request_number = 0;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_access_client_get_folder_listing(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_GET_FOLDERS;
    map_client->request_number = 0;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_access_client_get_message_listing_for_folder(uint16_t map_cid, const char * folder_name){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER;
    map_client->request_number = 0;
    map_client->folder_name = folder_name;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_access_client_get_message_with_handle(uint16_t map_cid, const map_message_handle_t map_message_handle, uint8_t with_attachment){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE;
    map_client->request_number = 0;
    memcpy(map_client->message_handle, map_message_handle, MAP_MESSAGE_HANDLE_SIZE);
    map_client->get_message_attachment = with_attachment;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_access_client_set_path(uint16_t map_cid, const char * path){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SET_PATH_ROOT;
    map_client->request_number = 0;
    map_client->current_folder = path;
    map_client->set_path_offset = 0;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_access_client_enable_notifications(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SET_NOTIFICATION;
    map_client->request_number = 0;
    map_client->notifications_enabled = 1;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}

uint8_t map_access_client_disable_notifications(uint16_t map_cid){
    UNUSED(map_cid);
    if (map_client->state != MAP_CONNECTED) return BTSTACK_BUSY;
    map_client->state = MAP_W2_SET_NOTIFICATION;
    map_client->request_number = 0;
    map_client->notifications_enabled = 0;
    goep_client_request_can_send_now(map_client->goep_cid);
    return 0;
}
