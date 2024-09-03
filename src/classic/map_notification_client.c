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

#define BTSTACK_FILE__ "map_notification_client.c"

#include "classic/map_notification_client.h"
#include "classic/map_access_server.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include <stdint.h>

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
#include "map_notification_client.h"

#define MNC_SET_STATE(new_state) do {mnc->state = new_state; log_info("mnc->state = %s", #new_state);} while (0)

// MAP 1.4.2, 6.3 - OBEX Header: map notification service bb582b41-420c-11db-b0de-0800200c9a66
static const uint8_t map_notification_client_service_uuid[] = { 0xbb, 0x58, 0x2b, 0x41, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66 };

static btstack_linked_list_t map_notification_clients;

static void map_notification_client_emit_connected_event(map_notification_client_t* context, uint8_t status) {
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event, pos, context->goep_client.cid);
    pos += 2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event, pos, context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    btstack_assert(pos == sizeof(event));
    context->client_handler(HCI_EVENT_PACKET, context->goep_client.cid, &event[0], pos);
}

static void map_notification_client_emit_connection_closed_event(map_notification_client_t* context) {
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event, pos, context->goep_client.cid);
    pos += 2;
    event[1] = pos - 2;
    btstack_assert(pos == sizeof(event));
    context->client_handler(HCI_EVENT_PACKET, context->goep_client.cid, &event[0], pos);
}

static void map_notification_client_emit_operation_complete_event(map_notification_client_t* context, uint8_t status) {
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event, pos, context->goep_client.cid);
    pos += 2;
    event[pos++] = status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->goep_client.cid, &event[0], pos);
}

//
// TODO: re-use the goep cid as map cid, so both refer to the same object
//       by this, we can remote the non-working linked_list_item_t from map_notification_client_t

static map_notification_client_t* map_notification_client_for_cid(uint16_t cid) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &map_notification_clients);
    while (btstack_linked_list_iterator_has_next(&it)) {
        map_notification_client_t* mnc = (map_notification_client_t*)btstack_linked_list_iterator_next(&it);
        if (mnc->cid == cid) {
            return mnc;
        }
    }
    return NULL;
}

map_notification_client_t* map_notification_client_for_mnc_cid(uint16_t mnc_cid) {
    return map_notification_client_for_cid(mnc_cid);
}

static map_notification_client_t* map_notification_client_for_goep_cid(uint16_t goep_cid) {
    return map_notification_client_for_cid(goep_cid);
}

static void map_notification_client_parser_callback_connect(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    map_notification_client_t* client = (map_notification_client_t*)user_data;
    switch (header_id) {
        case OBEX_HEADER_CONNECTION_ID:
            if (obex_parser_header_store(client->obex_header_buffer, sizeof(client->obex_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE) {
                goep_client_set_connection_id(client->goep_client.cid, big_endian_read_32(client->obex_header_buffer, 0));
            }
            break;
        default:
            break;
    }
}

static void map_notification_client_parser_callback_put_operation(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    map_notification_client_t* mnc = (map_notification_client_t*)user_data;
    switch (header_id) {
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch (mnc->state) {
                default:
                    printf("unexpected state: %d\n", mnc->state);
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static void map_notification_client_prepare_operation(map_notification_client_t* client, uint8_t op) {
    obex_parser_callback_t callback;

    switch (op) {
        case OBEX_OPCODE_PUT:
            callback = map_notification_client_parser_callback_put_operation;
            break;
        case OBEX_OPCODE_CONNECT:
            callback = map_notification_client_parser_callback_connect;
            break;
        default:
            callback = NULL;
            break;
    }

    obex_parser_init_for_response(&client->obex_parser, op, callback, client);
    client->obex_parser_waiting_for_response = true;
}

static void map_notification_client_handle_can_send_now(uint16_t goep_cid) {
   
    map_notification_client_t* mnc = map_notification_client_for_goep_cid(goep_cid);
    btstack_assert(mnc != NULL);

    switch (mnc->state) {
        case MNC_STATE_W2_SEND_CONNECT_REQUEST:
            goep_client_request_create_connect(mnc->goep_client.cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(mnc->goep_client.cid, map_notification_client_service_uuid, 16);

            // }
            MNC_SET_STATE(MNC_STATE_W4_CONNECT_RESPONSE);
            // prepare response
            map_notification_client_prepare_operation(mnc, OBEX_OPCODE_CONNECT);
            mnc->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(mnc->goep_client.cid);
            break;

        case MNC_STATE_W2_SEND_DISCONNECT_REQUEST:
            goep_client_request_create_disconnect(mnc->goep_client.cid);
            MNC_SET_STATE(MNC_STATE_W4_DISCONNECT_RESPONSE);
            // prepare response
            map_notification_client_prepare_operation(mnc, OBEX_OPCODE_DISCONNECT);
            // send packet
            goep_client_execute(mnc->goep_client.cid);
            break;

        case MNC_STATE_W2_PUT_SEND_EVENT: 
#if 0 //def ENABLE_GOEP_L2CAP
            log_error("With L2CAP enabled we've got only 150 bytes body size which is too small for our reports. Chunking to be implemented in the MNS client");
#else
            goep_client_request_create_put(mnc->goep_client.cid);
            goep_client_header_add_srm_enable(mnc->goep_client.cid);
            goep_client_header_add_type(mnc->goep_client.cid, "x-bt/MAP-event-report");
            goep_client_header_add_application_parameters(mnc->goep_client.cid, &mnc->app_params[0], (uint16_t)mnc->app_params_len);
            goep_client_body_add_static(mnc->goep_client.cid, mnc->body_buf, (uint32_t)mnc->body_buf_len);

            //goep_client_body_add_static(mnc->goep_client.cid, (uint8_t *) "0", 1);
            MNC_SET_STATE(MNC_STATE_W4_PUT_SEND_EVENT);
            map_notification_client_prepare_operation(mnc, OBEX_OPCODE_PUT);
            mnc->request_number++;
            goep_client_execute(mnc->goep_client.cid);
            log_error("goep_client_execute()");

#endif
            break;

        default:
            break;
    }
}

static void map_notification_client_handle_srm_headers(map_notification_client_t *context) {
    const map_access_client_obex_srm_t * obex_srm = &context->obex_srm;
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
static void map_notification_client_packet_handler_hci(uint8_t* packet, uint16_t size) {
    UNUSED(size);
    uint8_t status;
    map_notification_client_t* mnc;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)) {
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    mnc = map_notification_client_for_goep_cid(goep_subevent_connection_opened_get_goep_cid(packet));
                    btstack_assert(mnc != NULL);
                    status = goep_subevent_connection_opened_get_status(packet);
                    mnc->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                    mnc->incoming = goep_subevent_connection_opened_get_incoming(packet);
                    goep_subevent_connection_opened_get_bd_addr(packet, mnc->bd_addr);
                    if (status) {
                        log_info("map: connection failed %u", status);
                        MNC_SET_STATE(MNC_STATE_INIT);
                        map_notification_client_emit_connected_event(mnc, status);
                    }
                    else {
                        mnc->goep_client.cid = goep_subevent_connection_opened_get_goep_cid(packet);
                        log_info("map: connection established mnc->goep_client.cid:%u(0x%x)", mnc->goep_client.cid, mnc->goep_client.cid);
                        MNC_SET_STATE(MNC_STATE_W2_SEND_CONNECT_REQUEST);
                        goep_client_request_can_send_now(mnc->goep_client.cid);
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    mnc = map_notification_client_for_goep_cid(goep_subevent_connection_closed_get_goep_cid(packet));
                    btstack_assert(mnc != NULL);

                    if (mnc->state != MNC_STATE_CONNECTED) {
                        map_notification_client_emit_operation_complete_event(mnc, OBEX_DISCONNECTED);
                    }
                    MNC_SET_STATE(MNC_STATE_INIT);
                    btstack_linked_list_remove(&map_notification_clients, (btstack_linked_item_t*)mnc);
                    map_notification_client_emit_connection_closed_event(mnc);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    map_notification_client_handle_can_send_now(goep_subevent_can_send_now_get_goep_cid(packet));
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
map_notification_client_packet_handler_goep(uint16_t goep_cid, uint8_t* packet, uint16_t size) {
    map_notification_client_t* mnc = map_notification_client_for_goep_cid(goep_cid);
    btstack_assert(mnc != NULL);

    if (mnc->obex_parser_waiting_for_response == false) return;

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&mnc->obex_parser, packet, size);
    if (parser_state != OBEX_PARSER_OBJECT_STATE_COMPLETE) {
        return;
    }

    mnc->obex_parser_waiting_for_response = false;
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&mnc->obex_parser, &op_info);

    switch (mnc->state) {
        case MNC_STATE_W4_CONNECT_RESPONSE:
            switch (op_info.response_code) {
                case OBEX_RESP_SUCCESS:
                    MNC_SET_STATE(MNC_STATE_CONNECTED);
                    map_notification_client_emit_connected_event(mnc, ERROR_CODE_SUCCESS);
                    break;
                default:
                    log_info("map: obex connect failed, result 0x%02x", packet[0]);
                    MNC_SET_STATE(MNC_STATE_INIT);
                    map_notification_client_emit_connected_event(mnc, OBEX_CONNECT_FAILED);
                    break;
            }
            break;
        case MNC_STATE_W4_DISCONNECT_RESPONSE:
            MNC_SET_STATE(MNC_STATE_INIT);
            goep_client_disconnect(mnc->goep_client.cid);
            break;

        case MNC_STATE_W4_PUT_SEND_EVENT:
            switch (op_info.response_code) {
                case OBEX_RESP_CONTINUE:
                    map_notification_client_handle_srm_headers(mnc);
                    if (mnc->srm_state == SRM_ENABLED) {
                        map_notification_client_prepare_operation(mnc, OBEX_OPCODE_GET);
                        break;
                    }
                    MNC_SET_STATE(MNC_STATE_W2_PUT_SEND_EVENT);
                    goep_client_request_can_send_now(mnc->goep_client.cid);
                    break;
                case OBEX_RESP_SUCCESS:
                    MNC_SET_STATE(MNC_STATE_CONNECTED);
                    map_notification_client_emit_operation_complete_event(mnc, ERROR_CODE_SUCCESS);
                    break;
                case OBEX_RESP_NOT_IMPLEMENTED:
                    MNC_SET_STATE(MNC_STATE_CONNECTED);
                    map_notification_client_emit_operation_complete_event(mnc, OBEX_UNKNOWN_ERROR);
                    break;
                case OBEX_RESP_NOT_FOUND:
                    MNC_SET_STATE(MNC_STATE_CONNECTED);
                    map_notification_client_emit_operation_complete_event(mnc, OBEX_NOT_FOUND);
                    break;
                case OBEX_RESP_UNAUTHORIZED:
                case OBEX_RESP_FORBIDDEN:
                case OBEX_RESP_NOT_ACCEPTABLE:
                case OBEX_RESP_UNSUPPORTED_MEDIA_TYPE:
                case OBEX_RESP_ENTITY_TOO_LARGE:
                    MNC_SET_STATE(MNC_STATE_CONNECTED);
                    map_notification_client_emit_operation_complete_event(mnc, OBEX_NOT_ACCEPTABLE);
                    break;
                default:
                    log_info("unexpected obex response 0x%02x", op_info.response_code);
                    MNC_SET_STATE(MNC_STATE_CONNECTED);
                    map_notification_client_emit_operation_complete_event(mnc, OBEX_UNKNOWN_ERROR);
                    break;
            }
            break;
        default:
            btstack_unreachable();
            break;
    }
}

static void map_notification_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    UNUSED(channel);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            map_notification_client_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            map_notification_client_packet_handler_goep(channel, packet, size);
            break;
        default:
            break;
    }
}

// creates and requests to send MAP PUT "x-bt/MAP-event-report"
// First MASInstanceID should be 0 (BT SIG MAS SPEC, hard coded expactation in PTS MAP/MSE/MMN/BV-02-C)
uint8_t map_notification_client_send_event(uint16_t mnc_cid, uint8_t MASInstanceID, uint8_t* body_buf, size_t  body_buf_len){
    map_notification_client_t * mnc = map_notification_client_for_mnc_cid(mnc_cid);
    if (mnc == NULL) {
        RUN_AND_LOG_ACTION(return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;)
    }
    if (mnc->state != MNC_STATE_CONNECTED){
        RUN_AND_LOG_ACTION(return BTSTACK_BUSY;)
    }

    uint16_t pos = 0;

    mnc->app_params[pos++] = MAP_APP_PARAM_MASInstanceID;
    mnc->app_params[pos++] = 1;
    mnc->app_params[pos++] = MASInstanceID; 
    mnc->app_params_len = pos;

    MNC_SET_STATE(MNC_STATE_W2_PUT_SEND_EVENT);
    mnc->request_number = 0;
    mnc->body_buf = body_buf;
    mnc->body_buf_len = body_buf_len;
    log_debug("preapred sending goep_client_request_can_send_now");
    goep_client_request_can_send_now(mnc->goep_client.cid);
    return 0;
}

void map_notification_client_init(void) {
    map_notification_clients = NULL;
}

uint8_t map_notification_client_connect(map_notification_client_t* mnc, l2cap_ertm_config_t* l2cap_ertm_config,
                                        uint16_t l2cap_ertm_buffer_size, uint8_t* l2cap_ertm_buffer,
                                        btstack_packet_handler_t handler, bd_addr_t addr, uint8_t instance_id,
                                        uint16_t* out_cid) {

    memset(mnc, 0, sizeof(map_notification_client_t));
    MNC_SET_STATE(MNC_STATE_W4_GOEP_CONNECTION);
    mnc->client_handler = handler;
    uint8_t status = goep_client_connect((goep_client_t*)mnc, l2cap_ertm_config,
                                         l2cap_ertm_buffer, l2cap_ertm_buffer_size, &map_notification_client_packet_handler,
                                         addr, BLUETOOTH_SERVICE_CLASS_MESSAGE_NOTIFICATION_SERVER, instance_id,
                                         &mnc->cid);
    btstack_assert(status == ERROR_CODE_SUCCESS);
    btstack_linked_list_add(&map_notification_clients, (btstack_linked_item_t*)mnc);
    *out_cid = mnc->cid;
    log_debug("mnc->cid:0x%x mnc->goep_client.cid:0x%x mnc->goep_client.bearer_cid:0x%x", mnc->cid, mnc->goep_client.cid,mnc->goep_client.bearer_cid);
    return status;
}


uint8_t map_notification_client_disconnect(uint16_t mnc_cid) {
    map_notification_client_t* mnc = map_notification_client_for_mnc_cid(mnc_cid);
    if (mnc == NULL) {
        RUN_AND_LOG_ACTION(return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;)
    }
    if (mnc->state != MNC_STATE_CONNECTED) {
        return BTSTACK_BUSY;
    }

    MNC_SET_STATE(MNC_STATE_W2_SEND_DISCONNECT_REQUEST);
    goep_client_request_can_send_now(mnc->goep_client.cid);
    log_debug("mnc->goep_client.cid:0x%x", mnc->goep_client.cid);
    return 0;
}
