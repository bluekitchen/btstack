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

#define BTSTACK_FILE__ "map_access_client.c"

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hci_cmd.h"
#include "btstack_debug.h"
#include "hci.h"
#include "btstack_memory.h"
#include "bluetooth_sdp.h"
#include "btstack_event.h"

#include "classic/goep_client.h"
#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/sdp_client.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_util.h"
#include "classic/map_access_client.h"

#define MAP_MAX_NUM_ENTRIES 1024

// map access service bb582b40-420c-11db-b0de-0800200c9a66
static const uint8_t map_access_client_service_uuid[] = {0xbb, 0x58, 0x2b, 0x40, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66};
static uint32_t map_access_client_supported_features = 0x1F;

static btstack_linked_list_t map_access_clients;

static void map_access_client_emit_connected_event(map_access_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->goep_client.cid);
    pos+=2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_access_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->goep_client.cid, &event[0], pos);
}

static void map_access_client_emit_connection_closed_event(map_access_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->goep_client.cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_access_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->goep_client.cid, &event[0], pos);
}

static void map_access_client_emit_operation_complete_event(map_access_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->goep_client.cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->goep_client.cid, &event[0], pos);
}

// we re-use the goep cid as map cid, so both refer to the same object
static map_access_client_t * map_access_client_for_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &map_access_clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        map_access_client_t * map_access_client = (map_access_client_t *) btstack_linked_list_iterator_next(&it);
        if (map_access_client->cid == cid){
            return map_access_client;
        }
    }
    return NULL;
}

static map_access_client_t * map_access_client_for_map_cid(uint16_t map_cid){
    return map_access_client_for_cid(map_cid);
}

static map_access_client_t * map_access_client_for_goep_cid(uint16_t goep_cid){
    return map_access_client_for_cid(goep_cid);
}

static void map_access_client_message_handle_to_str(char * p, const map_message_handle_t msg_handle){
    int i;
    for (i = 0; i < MAP_MESSAGE_HANDLE_SIZE ; i++) {
        uint8_t byte = msg_handle[i];
        *p++ = char_for_nibble(byte >> 4);
        *p++ = char_for_nibble(byte & 0x0F);
    }
    *p = 0;
}

static void map_access_client_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    map_access_client_t * client = (map_access_client_t *) user_data;
    switch (header_id){
        case OBEX_HEADER_CONNECTION_ID:
            if (obex_parser_header_store(client->obex_header_buffer, sizeof(client->obex_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE){
                goep_client_set_connection_id(client->goep_client.cid, big_endian_read_32(client->obex_header_buffer, 0));
            }
            break;
        default:
            break;
    }
}

static void map_access_client_parser_callback_get_operation(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    map_access_client_t *map_access_client = (map_access_client_t *) user_data;
    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&map_access_client->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&map_access_client->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch(map_access_client->state){
                case MAP_W4_FOLDERS:
                case MAP_W4_MESSAGES_IN_FOLDER:
                case MAP_W4_CONVERSATION_LISTING:
                    map_util_xml_parser_parse (&map_access_client->mu_parser,
                                               (uint8_t *) data_buffer,
                                               data_len);
                    if (map_access_client->state == OBEX_HEADER_END_OF_BODY)
                        map_util_xml_parser_parse (&map_access_client->mu_parser,
                                                   NULL, 0);
                    break;
                case MAP_W4_SET_MESSAGE_STATUS:
                    break;
                case MAP_W4_MESSAGE:
                case MAP_W4_MAS_INSTANCE_INFO:
                    map_access_client->client_handler(MAP_DATA_PACKET, map_access_client->goep_client.cid, (uint8_t *) data_buffer, data_len);
                    break;

                default:
                    printf ("unexpected state: %d\n", map_access_client->state);
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static void map_access_client_prepare_srm_header(map_access_client_t * map_access_client){
    obex_srm_client_prepare_header(&map_access_client->obex_srm, map_access_client->goep_client.cid);
}

static void map_access_client_prepare_operation(map_access_client_t * client, uint8_t op){
    obex_parser_callback_t callback;

    switch (op) {
        case OBEX_OPCODE_GET:
        case OBEX_OPCODE_PUT:
        case OBEX_OPCODE_SETPATH:
            callback = map_access_client_parser_callback_get_operation;
            break;
        case OBEX_OPCODE_CONNECT:
            callback = map_access_client_parser_callback_connect;
            break;
        default:
            callback = NULL;
            break;
    }

    obex_parser_init_for_response(&client->obex_parser, op, callback, client);
    obex_srm_client_reset_fields(&client->obex_srm);
    client->obex_parser_waiting_for_response = true;
}

static void map_access_client_handle_can_send_now(uint16_t goep_cid) {
    uint8_t application_parameters[6];
    int pos = 0;
    char map_message_handle_to_str_buffer[MAP_MESSAGE_HANDLE_SIZE * 2 + 1];
    uint8_t  path_element[20];
    uint16_t path_element_start;
    uint16_t path_element_len;
    int done;

    map_access_client_t * map_access_client = map_access_client_for_goep_cid(goep_cid);
    btstack_assert (map_access_client != NULL);

    switch (map_access_client->state){
        case MAP_W2_SEND_CONNECT_REQUEST:
            goep_client_request_create_connect(map_access_client->goep_client.cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(map_access_client->goep_client.cid, map_access_client_service_uuid, 16);
            // Mandatory if the PSE advertises a PbapSupportedFeatures attribute in its SDP record, else excluded.
            if (goep_client_get_map_supported_features(map_access_client->goep_client.cid) != MAP_FEATURES_NOT_PRESENT){
                application_parameters[0] = 0x29; // MAP_APPLICATION_PARAMETER_MAP_SUPPORTED_FEATURES;
                application_parameters[1] = 4;
                big_endian_store_32(application_parameters, 2, map_access_client_supported_features);
                goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], 6);

            }
            map_access_client->state = MAP_W4_CONNECT_RESPONSE;
            // prepare response
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_CONNECT);
            obex_srm_client_init(&map_access_client->obex_srm);
            map_access_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_DISCONNECT_REQUEST:
            goep_client_request_create_disconnect(map_access_client->goep_client.cid);
            map_access_client->state = MAP_W4_DISCONNECT_RESPONSE;
            // prepare response
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_DISCONNECT);
            obex_srm_client_init(&map_access_client->obex_srm);
            // send packet
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_UPDATE_INBOX:
            goep_client_request_create_put(map_access_client->goep_client.cid);
            goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/MAP-messageUpdate");

            goep_client_body_add_static(map_access_client->goep_client.cid, (uint8_t *) "0", 1);
            map_access_client->state = MAP_W4_UPDATE_INBOX;
            // prepare response
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_PUT);
            obex_srm_client_init(&map_access_client->obex_srm);
            map_access_client->request_number++;
            // send packet
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SET_PATH_ROOT:
            goep_client_request_create_set_path(map_access_client->goep_client.cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(map_access_client->goep_client.cid, "");
            // state
            map_access_client->state = MAP_W4_SET_PATH_ROOT_COMPLETE;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_SETPATH);
            obex_srm_client_init(&map_access_client->obex_srm);
            // send packet
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;
        case MAP_W2_SET_PATH_ELEMENT:
            // find '/' or '\0'
            path_element_start = map_access_client->set_path_offset;
            while (map_access_client->current_folder[map_access_client->set_path_offset] != '\0' &&
                   map_access_client->current_folder[map_access_client->set_path_offset] != '/'){
                map_access_client->set_path_offset++;
            }
            path_element_len = map_access_client->set_path_offset - path_element_start;
            memcpy(path_element, &map_access_client->current_folder[path_element_start], path_element_len);
            path_element[path_element_len] = 0;

            // skip /
            if (map_access_client->current_folder[map_access_client->set_path_offset] == '/'){
                map_access_client->set_path_offset++;
            }

            // done?
            done = map_access_client->current_folder[map_access_client->set_path_offset] == '\0';

            // status
            log_info("Path element '%s', done %u", path_element, done);
            UNUSED(done);

            goep_client_request_create_set_path(map_access_client->goep_client.cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(map_access_client->goep_client.cid, (const char *) path_element); // next element
            // state
            map_access_client->state = MAP_W4_SET_PATH_ELEMENT_COMPLETE;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_SETPATH);
            obex_srm_client_init(&map_access_client->obex_srm);
            // send packet
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_GET_FOLDERS:
            goep_client_request_create_get(map_access_client->goep_client.cid);
            if (map_access_client->request_number == 0){
                obex_srm_client_init(&map_access_client->obex_srm);
                map_access_client_prepare_srm_header(map_access_client);
                goep_client_header_add_type(map_access_client->goep_client.cid, "x-obex/folder-listing");
            }
            map_access_client->state = MAP_W4_FOLDERS;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_GET);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER:
            goep_client_request_create_get(map_access_client->goep_client.cid);
            if (map_access_client->request_number == 0){
                obex_srm_client_init(&map_access_client->obex_srm);
                map_access_client_prepare_srm_header(map_access_client);
                goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/MAP-msg-listing");
                goep_client_header_add_name(map_access_client->goep_client.cid, map_access_client->folder_name);
            }
            map_access_client->state = MAP_W4_MESSAGES_IN_FOLDER;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_GET);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE:
            goep_client_request_create_get(map_access_client->goep_client.cid);

            if (map_access_client->request_number == 0){
                obex_srm_client_init(&map_access_client->obex_srm);
                map_access_client_prepare_srm_header(map_access_client);

                map_access_client_message_handle_to_str(map_message_handle_to_str_buffer, map_access_client->message_handle);
                goep_client_header_add_name(map_access_client->goep_client.cid, map_message_handle_to_str_buffer);

                goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/message");

                application_parameters[pos++] = 0x0A; // attachment
                application_parameters[pos++] = 1;
                application_parameters[pos++] = map_access_client->get_message_attachment;

                application_parameters[pos++] = 0x14; // Charset
                application_parameters[pos++] = 1;
                application_parameters[pos++] = 1;    // UTF-8
                goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], pos);
            }

            map_access_client->state = MAP_W4_MESSAGE;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_GET);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_GET_CONVERSATION_LISTING:
            goep_client_request_create_get(map_access_client->goep_client.cid);

            if (map_access_client->request_number == 0){
                obex_srm_client_init(&map_access_client->obex_srm);
                map_access_client_prepare_srm_header(map_access_client);

                goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/MAP-convo-listing");

                application_parameters[pos++] = 0x01; // MaxListCount
                application_parameters[pos++] = 2;
                big_endian_store_16(application_parameters,pos,map_access_client->max_list_count);
                pos += 2;

                application_parameters[pos++] = 0x02; // ListStartOffset
                application_parameters[pos++] = 2;
                big_endian_store_16(application_parameters,pos,map_access_client->list_start_offset);
                pos += 2;

                goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], pos);
            }

            map_access_client->state = MAP_W4_CONVERSATION_LISTING;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_GET);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_SET_MESSAGE_STATUS:
            goep_client_request_create_put(map_access_client->goep_client.cid);
            goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/messageStatus");

            map_access_client_message_handle_to_str(map_message_handle_to_str_buffer, map_access_client->message_handle);
            goep_client_header_add_name(map_access_client->goep_client.cid, map_message_handle_to_str_buffer);

            if (map_access_client->read_status >= 0 && map_access_client->read_status <= 1) {
                application_parameters[pos++] = 0x17;  // StatusIndicator
                application_parameters[pos++] = 1;
                application_parameters[pos++] = 0;     // readStatus

                application_parameters[pos++] = 0x18; // StatusValue
                application_parameters[pos++] = 1;
                application_parameters[pos++] = map_access_client->read_status;
                goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], pos);
            } else {
                obex_srm_client_init(&map_access_client->obex_srm);
            }

            goep_client_body_add_static(map_access_client->goep_client.cid, (uint8_t *) "0", 1);

            map_access_client->state = MAP_W4_SET_MESSAGE_STATUS;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_PUT);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SET_NOTIFICATION:
            goep_client_request_create_put(map_access_client->goep_client.cid);
            goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/MAP-NotificationRegistration");

            application_parameters[pos++] = 0x0E; // NotificationStatus
            application_parameters[pos++] = 1;
            application_parameters[pos++] = map_access_client->notifications_enabled;

            goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], pos);
            goep_client_body_add_static(map_access_client->goep_client.cid, (uint8_t *) "0", 1);
            map_access_client->state = MAP_W4_SET_NOTIFICATION;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_PUT);
            obex_srm_client_init(&map_access_client->obex_srm);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SET_NOTIFICATION_FILTER:
            goep_client_request_create_put(map_access_client->goep_client.cid);
            goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/MAP-notification-filter");

            application_parameters[pos++] = 0x25; // NotificationFilterMask
            application_parameters[pos++] = 4;
            application_parameters[pos++] = (map_access_client->notification_filter_mask >> 0) & 0xff;
            application_parameters[pos++] = (map_access_client->notification_filter_mask >> 8) & 0xff;
            application_parameters[pos++] = (map_access_client->notification_filter_mask >> 16) & 0xff;
            application_parameters[pos++] = (map_access_client->notification_filter_mask >> 24) & 0xff;

            goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], pos);
            goep_client_body_add_static(map_access_client->goep_client.cid, (uint8_t *) "0", 1);
            map_access_client->state = MAP_W4_SET_NOTIFICATION_FILTER;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_PUT);
            obex_srm_client_init(&map_access_client->obex_srm);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        case MAP_W2_SEND_GET_MAS_INSTANCE_INFO:
            goep_client_request_create_get(map_access_client->goep_client.cid);

            if (map_access_client->request_number == 0){
                obex_srm_client_init(&map_access_client->obex_srm);
                map_access_client_prepare_srm_header(map_access_client);

                goep_client_header_add_type(map_access_client->goep_client.cid, "x-bt/MASInstanceInformation");

                application_parameters[pos++] = 0x0F; // MASInstance
                application_parameters[pos++] = 1;
                application_parameters[pos++] = map_access_client->mas_instance_id;

                goep_client_header_add_application_parameters(map_access_client->goep_client.cid, &application_parameters[0], pos);
            }

            map_access_client->state = MAP_W4_MAS_INSTANCE_INFO;
            map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_GET);
            map_access_client->request_number++;
            goep_client_execute(map_access_client->goep_client.cid);
            break;

        default:
            break;
    }
}

static void map_access_client_packet_handler_hci(uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t status;
    map_access_client_t * map_access_client;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)){
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    map_access_client = map_access_client_for_goep_cid(goep_subevent_connection_opened_get_goep_cid(packet));
                    btstack_assert(map_access_client != NULL);
                    status = goep_subevent_connection_opened_get_status(packet);
                    map_access_client->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                    map_access_client->incoming = goep_subevent_connection_opened_get_incoming(packet);
                    goep_subevent_connection_opened_get_bd_addr(packet, map_access_client->bd_addr);
                    if (status){
                        log_info("map: connection failed %u", status);
                        map_access_client->state = MAP_INIT;
                        map_access_client_emit_connected_event(map_access_client, status);
                    } else {
                        log_info("map: connection established");
                        map_access_client->goep_client.cid = goep_subevent_connection_opened_get_goep_cid(packet);
                        map_access_client->state = MAP_W2_SEND_CONNECT_REQUEST;
                        goep_client_request_can_send_now(map_access_client->goep_client.cid);
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    map_access_client = map_access_client_for_goep_cid(goep_subevent_connection_closed_get_goep_cid(packet));
                    btstack_assert(map_access_client != NULL);

                    if (map_access_client->state != MAP_CONNECTED){
                        map_access_client_emit_operation_complete_event(map_access_client, OBEX_DISCONNECTED);
                    }
                    map_access_client->state = MAP_INIT;
                    btstack_linked_list_remove(&map_access_clients, (btstack_linked_item_t *) map_access_client);
                    map_access_client_emit_connection_closed_event(map_access_client);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    map_access_client_handle_can_send_now(goep_subevent_can_send_now_get_goep_cid(packet));
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
map_access_client_packet_handler_goep(uint16_t goep_cid, uint8_t *packet, uint16_t size) {
    map_access_client_t * map_access_client = map_access_client_for_goep_cid(goep_cid);
    btstack_assert(map_access_client != NULL);

    if (map_access_client->obex_parser_waiting_for_response == false) return;

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&map_access_client->obex_parser, packet, size);
    if (parser_state != OBEX_PARSER_OBJECT_STATE_COMPLETE){
        return;
    }

    map_access_client->obex_parser_waiting_for_response = false;
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&map_access_client->obex_parser, &op_info);

    switch (map_access_client->state){
        case MAP_W4_CONNECT_RESPONSE:
            switch (op_info.response_code){
                case OBEX_RESP_SUCCESS:
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_connected_event(map_access_client, ERROR_CODE_SUCCESS);
                    break;
                default:
                    log_info("map: obex connect failed, result 0x%02x", packet[0]);
                    map_access_client->state = MAP_INIT;
                    map_access_client_emit_connected_event(map_access_client, OBEX_CONNECT_FAILED);
                    break;
            }
            break;
        case MAP_W4_DISCONNECT_RESPONSE:
            map_access_client->state = MAP_INIT;
            goep_client_disconnect(map_access_client->goep_client.cid);
            break;

        case MAP_W4_SET_PATH_ROOT_COMPLETE:
        case MAP_W4_SET_PATH_ELEMENT_COMPLETE:
            if (op_info.response_code == OBEX_RESP_SUCCESS){
                // more path?
                if (map_access_client->current_folder[map_access_client->set_path_offset]){
                    map_access_client->state = MAP_W2_SET_PATH_ELEMENT;
                    goep_client_request_can_send_now(map_access_client->goep_client.cid);
                } else {
                    map_access_client->current_folder = NULL;
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_operation_complete_event(map_access_client, ERROR_CODE_SUCCESS);
                }
            } else if (packet[0] == OBEX_RESP_NOT_FOUND){
                map_access_client->state = MAP_CONNECTED;
                map_access_client_emit_operation_complete_event(map_access_client, OBEX_NOT_FOUND);
            } else {
                map_access_client->state = MAP_CONNECTED;
                map_access_client_emit_operation_complete_event(map_access_client, OBEX_UNKNOWN_ERROR);
            }
            break;

        case MAP_W4_FOLDERS:
        case MAP_W4_UPDATE_INBOX:
        case MAP_W4_MESSAGES_IN_FOLDER:
        case MAP_W4_MESSAGE:
        case MAP_W4_CONVERSATION_LISTING:
        case MAP_W4_SET_MESSAGE_STATUS:
        case MAP_W4_SET_NOTIFICATION:
        case MAP_W4_SET_NOTIFICATION_FILTER:
        case MAP_W4_MAS_INSTANCE_INFO:
            switch (op_info.response_code) {
                case OBEX_RESP_CONTINUE:
                    obex_srm_client_handle_headers(&map_access_client->obex_srm);
                    if (obex_srm_client_is_srm_active(&map_access_client->obex_srm)) {
                        map_access_client_prepare_operation(map_access_client, OBEX_OPCODE_GET);
                        break;
                    }
                    map_access_client->state =
                       (map_access_client->state == MAP_W4_FOLDERS ? MAP_W2_SEND_GET_FOLDERS :
                        map_access_client->state == MAP_W4_MESSAGES_IN_FOLDER ? MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER :
                        map_access_client->state == MAP_W4_CONVERSATION_LISTING ? MAP_W2_SEND_GET_CONVERSATION_LISTING :
                        map_access_client->state == MAP_W4_MESSAGE ? MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE :
                        MAP_CONNECTED);

                    goep_client_request_can_send_now(map_access_client->goep_client.cid);
                    break;
                case OBEX_RESP_SUCCESS:
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_operation_complete_event(map_access_client, ERROR_CODE_SUCCESS);
                    break;
                case OBEX_RESP_NOT_IMPLEMENTED:
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_operation_complete_event(map_access_client, OBEX_UNKNOWN_ERROR);
                    break;
                case OBEX_RESP_NOT_FOUND:
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_operation_complete_event(map_access_client, OBEX_NOT_FOUND);
                    break;
                case OBEX_RESP_UNAUTHORIZED:
                case OBEX_RESP_FORBIDDEN:
                case OBEX_RESP_NOT_ACCEPTABLE:
                case OBEX_RESP_UNSUPPORTED_MEDIA_TYPE:
                case OBEX_RESP_ENTITY_TOO_LARGE:
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_operation_complete_event(map_access_client, OBEX_NOT_ACCEPTABLE);
                    break;
                default:
                    log_info("unexpected obex response 0x%02x", op_info.response_code);
                    map_access_client->state = MAP_CONNECTED;
                    map_access_client_emit_operation_complete_event(map_access_client, OBEX_UNKNOWN_ERROR);
                    break;
            }
            break;

        default:
            btstack_unreachable();
            break;
    }
}

static void map_access_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);

    switch (packet_type){
        case HCI_EVENT_PACKET:
            map_access_client_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            map_access_client_packet_handler_goep(channel, packet, size);
            break;
        default:
            break;
    }
}

void map_access_client_init(void){
    map_access_clients = NULL;
}

uint8_t map_access_client_connect(map_access_client_t *map_access_client, l2cap_ertm_config_t *l2cap_ertm_config,
                                  uint16_t l2cap_ertm_buffer_size, uint8_t *l2cap_ertm_buffer,
                                  btstack_packet_handler_t handler, bd_addr_t addr, uint8_t instance_id,
                                  uint16_t *out_cid) {

    memset(map_access_client, 0, sizeof(map_access_client_t));
    map_access_client->state = MAP_W4_GOEP_CONNECTION;
    map_access_client->request_number = 0;
    map_access_client->client_handler = handler;
    uint8_t status = goep_client_connect(&map_access_client->goep_client, l2cap_ertm_config,
                                         l2cap_ertm_buffer, l2cap_ertm_buffer_size, &map_access_client_packet_handler,
                                         addr, BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER, instance_id,
                                         &map_access_client->cid);
    btstack_assert(status == ERROR_CODE_SUCCESS);
    btstack_linked_list_add(&map_access_clients, (btstack_linked_item_t *) map_access_client);
    *out_cid = map_access_client->cid;
    return status;
}

uint8_t map_access_client_disconnect(uint16_t map_cid){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_DISCONNECT_REQUEST;
    map_access_client->request_number = 0;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_update_inbox(uint16_t map_cid){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_UPDATE_INBOX;
    map_access_client->request_number = 0;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_get_folder_listing(uint16_t map_cid){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_GET_FOLDERS;
    map_access_client->request_number = 0;
    map_util_xml_parser_init (&map_access_client->mu_parser,
                              MAP_UTIL_PARSER_TYPE_FOLDER_LISTING,
                              map_access_client->client_handler,
                              map_cid);
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_get_message_listing_for_folder(uint16_t map_cid, const char * folder_name){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER;
    map_access_client->request_number = 0;
    map_access_client->folder_name = folder_name;
    map_util_xml_parser_init (&map_access_client->mu_parser,
                              MAP_UTIL_PARSER_TYPE_MESSAGE_LISTING,
                              map_access_client->client_handler,
                              map_cid);
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_get_conversation_listing(uint16_t map_cid, int max_list_count, int list_start_offset){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_GET_CONVERSATION_LISTING;
    map_access_client->request_number = 0;
    map_access_client->max_list_count = max_list_count;
    map_access_client->list_start_offset = list_start_offset;
    map_util_xml_parser_init (&map_access_client->mu_parser,
                              MAP_UTIL_PARSER_TYPE_CONVERSATION_LISTING,
                              map_access_client->client_handler,
                              map_cid);
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_get_message_with_handle(uint16_t map_cid, const map_message_handle_t map_message_handle, uint8_t with_attachment){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE;
    map_access_client->request_number = 0;
    memcpy(map_access_client->message_handle, map_message_handle, MAP_MESSAGE_HANDLE_SIZE);
    map_access_client->get_message_attachment = with_attachment;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_set_message_status(uint16_t map_cid, const map_message_handle_t map_message_handle, int read_status){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    map_access_client->state = MAP_W2_SEND_SET_MESSAGE_STATUS;
    map_access_client->request_number = 0;
    map_access_client->read_status = read_status;
    memcpy(map_access_client->message_handle, map_message_handle, MAP_MESSAGE_HANDLE_SIZE);
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_set_path(uint16_t map_cid, const char * path){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SET_PATH_ROOT;
    map_access_client->request_number = 0;
    map_access_client->current_folder = path;
    map_access_client->set_path_offset = 0;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_enable_notifications(uint16_t map_cid){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SET_NOTIFICATION;
    map_access_client->request_number = 0;
    map_access_client->notifications_enabled = 1;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_disable_notifications(uint16_t map_cid){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SET_NOTIFICATION;
    map_access_client->request_number = 0;
    map_access_client->notifications_enabled = 0;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_set_notification_filter(uint16_t map_cid, uint32_t filter_mask){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SET_NOTIFICATION_FILTER;
    map_access_client->request_number = 0;
    map_access_client->notification_filter_mask = filter_mask;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}

uint8_t map_access_client_get_mas_instance_info(uint16_t map_cid, uint8_t mas_instance_id){
    map_access_client_t * map_access_client = map_access_client_for_map_cid(map_cid);
    if (map_access_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_client->state != MAP_CONNECTED){
        return BTSTACK_BUSY;
    }

    map_access_client->state = MAP_W2_SEND_GET_MAS_INSTANCE_INFO;
    map_access_client->request_number = 0;
    map_access_client->mas_instance_id = mas_instance_id;
    goep_client_request_can_send_now(map_access_client->goep_client.cid);
    return 0;
}
