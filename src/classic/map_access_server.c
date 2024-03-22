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

#define BTSTACK_FILE__ "map_access_server.c"

#include "classic/map_access_server.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "bluetooth.h"
#include "classic/obex.h"
#include "classic/obex_srm.h"
#include "classic/obex_parser.h"
#include "classic/goep_server.h"

#include <stdio.h>
#include <stdint.h>

#define MAP_MAX_NUM_ENTRIES 1024

 // map access service bb582b40-420c-11db-b0de-0800200c9a66
static const uint8_t map_access_service_uuid[] = { 0xbb, 0x58, 0x2b, 0x40, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66 };

static uint16_t maximum_obex_packet_length;

typedef enum {
    MAP_INIT = 0,
    MAP_W2_SEND_CONNECT_RESPONSE,
    MAP_CONNECTED,
    MAP_W2_SEND_DISCONNECT_RESPONSE,
    MAP_W4_REQUEST,
    MAP_SEND_REQUEST_RESPONSE,
} map_access_server_state_t;


typedef struct {
    uint16_t  mns_cid;
    uint16_t  goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    map_access_server_state_t state;
    obex_parser_t obex_parser;
    obex_srm_t obex_srm;
    // request
    struct {
        bool is_event_report;
        uint32_t length;
        uint8_t* payload_data;
        uint32_t payload_len;
        uint32_t payload_position;
        uint8_t  abort_response;
        obex_app_param_parser_t app_param_parser;
        struct {
            uint8_t  mas_instance_id;
        } app_params;
    } request;
    uint8_t response_code;
    uint16_t maximum_obex_packet_length;
    uint8_t  flags;
    // operation status
    bool    operation_complete_send;
    uint8_t operation_complete_status;
} map_access_server_t;

#define ENTER_STATE(mas, s) \
    do { log_info ("entering state %s", #s); mas->state = s; } while (0)

static btstack_packet_handler_t map_access_server_user_packet_handler;

static map_access_server_t  map_access_server_singleton;

// dummy lookup
static map_access_server_t* map_access_server_for_goep_cid(uint16_t goep_cid) {
    return &map_access_server_singleton;
}

static void map_access_send_connect_response(uint16_t goep_cid) {
    goep_server_response_create_connect(goep_cid, OBEX_VERSION, 0, maximum_obex_packet_length);
    goep_server_header_add_who(goep_cid, map_access_service_uuid);
    goep_server_execute(goep_cid, OBEX_RESP_SUCCESS);
}

static void map_access_send_general_response(uint16_t goep_cid, uint8_t response_code) {
    goep_server_response_create_general(goep_cid);
    goep_server_execute(goep_cid, response_code);
}

static uint8_t goep_data_packet_get_opcode(uint8_t* packet) {
    return packet[0];
}

static void map_access_server_reset_request(map_access_server_t* mns) {
    (void)memset(&mns->request, 0, sizeof(mns->request));
}

/* OBEX related functions */

static void map_access_server_app_param_callback(void* user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t* data_buffer, uint8_t data_len) {
    map_access_server_t* mns = (map_access_server_t*)user_data;

    switch (tag_id) {
    case MAP_APPLICATION_PARAMETER_MAS_INSTANCE_ID:
        mns->request.app_params.mas_instance_id = data_buffer[0];
        log_info("MAS instance ID %02d\n", mns->request.app_params.mas_instance_id);
        break;
    default:
        log_info("unhandled application parameter %02x\n", tag_id);
        break;
    }
}

static void map_access_server_obex_parser_callback(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    map_access_server_t* mns = (map_access_server_t*)user_data;

    switch (header_id) {
    case OBEX_HEADER_SINGLE_RESPONSE_MODE:
    case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
        obex_srm_header_store(&mns->obex_srm, header_id,
            total_len, data_offset, data_buffer, data_len);
        break;
    case OBEX_HEADER_CONNECTION_ID:
        // TODO: verify connection id
        break;
    case OBEX_HEADER_TYPE:
        /* we only deal with <x-bt/MAP-event-report> */
        if (data_len == strlen("x-bt/MAP-event-report") &&
            strncmp("x-bt/MAP-event-report", (const char*)data_buffer, data_len) == 0) {
            mns->request.is_event_report = 1;
        }
        break;
    case OBEX_HEADER_BODY:
    case OBEX_HEADER_END_OF_BODY:
        log_info("received (END_OF_)BODY data: %d bytes\n", data_len);
        mns->request.payload_data = (uint8_t*)data_buffer;
        mns->request.payload_len = data_len;
        break;
    case OBEX_HEADER_LENGTH:
        mns->request.length = big_endian_read_32(data_buffer, 0);
        log_info("length of data: %d\n", mns->request.length);
        break;
    case OBEX_HEADER_APPLICATION_PARAMETERS:
        log_info("application parameters: %d bytes\n", data_len);
        if (data_offset == 0) {
            obex_app_param_parser_init(&mns->request.app_param_parser,
                &map_access_server_app_param_callback, total_len, mns);
        }
        obex_app_param_parser_process_data(&mns->request.app_param_parser, data_buffer, data_len);
        break;

    default:
        log_info("received unhandled MNS header type %d\n", header_id);
        break;
    }
}


/* btstack packet handlers */

static void map_access_server_handle_can_send_now(map_access_server_t* mas) {
    switch (mas->state) {
    case MAP_W2_SEND_CONNECT_RESPONSE:
        if (mas->response_code == OBEX_RESP_SUCCESS) {
            ENTER_STATE(mas, MAP_CONNECTED);
            map_access_send_connect_response(mas->goep_cid);
        }
        else {
            ENTER_STATE(mas, MAP_INIT);
            map_access_send_general_response(mas->goep_cid, mas->response_code);
        }
        break;
    case MAP_W2_SEND_DISCONNECT_RESPONSE:
        // TODO: should we disconnect after sending the disconnect response?
        ENTER_STATE(mas, MAP_INIT);
        map_access_send_general_response(mas->goep_cid, OBEX_RESP_SUCCESS);
        break;
    case MAP_SEND_REQUEST_RESPONSE:
        ENTER_STATE(mas, MAP_CONNECTED);
        map_access_send_general_response(mas->goep_cid, mas->response_code);
        break;
    default:
        break;
    }
}

static void map_access_server_packet_handler_hci(uint8_t* packet, uint16_t size) {
    UNUSED(size);
    uint8_t status;
    map_access_server_t* mas;
    uint16_t goep_cid;

    switch (hci_event_packet_get_type(packet)) {
    case HCI_EVENT_GOEP_META:
        goep_cid = goep_subevent_incoming_connection_get_goep_cid(packet);
        switch (hci_event_goep_meta_get_subevent_code(packet)) {
        case GOEP_SUBEVENT_INCOMING_CONNECTION:
            status = goep_server_accept_connection(goep_cid);
            btstack_assert(status == ERROR_CODE_SUCCESS);
            break;
        case GOEP_SUBEVENT_CONNECTION_OPENED:
            mas = map_access_server_for_goep_cid(goep_cid);
            btstack_assert(mas != NULL);
            status = goep_subevent_connection_opened_get_status(packet);
            log_info("MAS GOEP Connection opened 0x%02x\n", status);
            if (status == ERROR_CODE_SUCCESS) {
                log_info("mas: connection established");
                mas->goep_cid = goep_cid;
                goep_subevent_connection_opened_get_bd_addr(packet, mas->bd_addr);
                mas->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                ENTER_STATE(mas, MAP_INIT);
                // TODO: emit connection success event
                // map_emit_connected_event(mas, status);
                obex_parser_init_for_request(&mas->obex_parser, NULL, NULL);
            }
            else {
                log_info("MAP access server: connection failed %u", status);
                // TODO: emit connection failed event
                // map_emit_connected_event(&map_server.connection, status);
            }
            break;
        case GOEP_SUBEVENT_CONNECTION_CLOSED:
            goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
            mas = map_access_server_for_goep_cid(goep_cid);
            btstack_assert(mas != NULL);
            log_info("MAS: connection closed");
            ENTER_STATE(mas, MAP_INIT);
            // TODO: emit connection closed event
            // map_emit_connection_closed_event(&map_server.connection);
            break;
        case GOEP_SUBEVENT_CAN_SEND_NOW:
            goep_cid = goep_subevent_can_send_now_get_goep_cid(packet);
            mas = map_access_server_for_goep_cid(goep_cid);
            btstack_assert(mas != NULL);
            map_access_server_handle_can_send_now(mas);
            break;
        default:
            break;
        }
    default:
        break;
    }
}

static void map_access_server_packet_handler_goep(map_access_server_t* mas, uint8_t* packet, uint16_t size) {
    obex_parser_object_state_t parser_state;
    uint8_t opcode;
    int i;

    btstack_assert(size > 0);

    switch (mas->state) {
    
    case MAP_INIT:
        parser_state = obex_parser_process_data(&mas->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&mas->obex_parser, &op_info);
            switch (op_info.opcode) {
            case OBEX_OPCODE_CONNECT:
                ENTER_STATE(mas, MAP_W2_SEND_CONNECT_RESPONSE);
                mas->flags = packet[2];
                mas->maximum_obex_packet_length = btstack_min(maximum_obex_packet_length, big_endian_read_16(packet, 3));
                mas->response_code = OBEX_RESP_SUCCESS;
                goep_server_request_can_send_now(mas->goep_cid);
                break;

            default:
                ENTER_STATE(mas, MAP_W2_SEND_CONNECT_RESPONSE);
                mas->response_code = OBEX_RESP_BAD_REQUEST;
                goep_server_request_can_send_now(mas->goep_cid);
                break;
            }
        }
        break;

    case MAP_CONNECTED:
        opcode = goep_data_packet_get_opcode(packet);
        // default headers
        switch (opcode) {
        case OBEX_OPCODE_PUT:
        case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
            log_info("handling PUT\n");
            map_access_server_reset_request(mas);
            ENTER_STATE(mas, MAP_W4_REQUEST);
            obex_parser_init_for_request(&mas->obex_parser, &map_access_server_obex_parser_callback, (void*)mas);
            break;
        case OBEX_OPCODE_DISCONNECT:
            ENTER_STATE(mas, MAP_W4_REQUEST);
            obex_parser_init_for_request(&mas->obex_parser, NULL, NULL);
            break;
        default:
            ENTER_STATE(mas, MAP_W4_REQUEST);
            obex_parser_init_for_request(&mas->obex_parser, NULL, NULL);
            break;
        }

        /* fall through */
#if 0
    case MAP_W4_REQUEST:
        obex_srm_init(&mas->obex_srm);
        parser_state = obex_parser_process_data(&mas->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&mas->obex_parser, &op_info);
            switch (op_info.opcode) {
            case OBEX_OPCODE_PUT:
            case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
                mas->request.abort_response = 0;
                map_access_server_handle_put_request(mas, op_info.opcode, false);
                if (mas->request.abort_response == 0) {
                    (*map_access_server_user_packet_handler)(MAP_DATA_PACKET, mas->mns_cid, (uint8_t*)mas->request.payload_data, mas->request.payload_len);
                }
                break;
            case OBEX_OPCODE_DISCONNECT:
                ENTER_STATE(mas, MAP_W2_SEND_DISCONNECT_RESPONSE);
                goep_server_request_can_send_now(mas->goep_cid);
                break;

            default:
                ENTER_STATE(mas, MAP_SEND_REQUEST_RESPONSE);
                mas->response_code = OBEX_RESP_BAD_REQUEST;
                goep_server_request_can_send_now(mas->goep_cid);
                break;
            }
        }
        break;
#endif
    default:
        printf("MAP server: GOEP data packet'");
        for (i = 0;i < size;i++) {
            printf("%02x ", packet[i]);
        }
        printf("'\n");
        return;
    }
}

static void map_access_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    map_access_server_t* mas;
    switch (packet_type) {
    // meta events
    case HCI_EVENT_PACKET:
        map_access_server_packet_handler_hci(packet, size);
        break;
    // data
    case GOEP_DATA_PACKET:
        mas = map_access_server_for_goep_cid(channel);
        btstack_assert(mas != NULL);
        map_access_server_packet_handler_goep(mas, packet, size);
        break;
    default:
        break;
    }
}

void map_access_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, uint16_t mtu) {
    maximum_obex_packet_length = mtu;
    goep_server_register_service(&map_access_server_packet_handler, rfcomm_channel_nr, 0xFFFF, l2cap_psm, 0xFFFF, LEVEL_0);

    map_access_server_user_packet_handler = packet_handler;
}
