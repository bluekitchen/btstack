/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "opp_client.c"

#include "btstack_config.h"

#include <stdint.h>
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

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/obex_iterator.h"
#include "classic/obex_srm_client.h"
#include "classic/goep_client.h"
#include "classic/opp_client.h"

// TODO remove include and printf
#include <stdio.h>

typedef enum {
    OPP_INIT = 0,
    OPP_W4_GOEP_CONNECTION,
    OPP_W2_SEND_CONNECT_REQUEST,
    OPP_W4_CONNECT_RESPONSE,
    OPP_CONNECT_RESPONSE_RECEIVED,
    OPP_CONNECTED,
    //
    OPP_W2_SEND_DISCONNECT_REQUEST,
    OPP_W4_DISCONNECT_RESPONSE,
    // pull default object
    OPP_W2_GET_DEFAULT_OBJECT,
    OPP_W4_GET_DEFAULT_OBJECT,
    // push object
    OPP_W2_PUT_OBJECT,
    OPP_W4_PUT_OBJECT,
    // abort operation
    OPP_W4_ABORT_COMPLETE,
} opp_state_t;

typedef struct opp_client {
    opp_state_t state;
    uint16_t  opp_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    uint16_t  goep_cid;
    btstack_packet_handler_t client_handler;
    int request_number;
    /* abort */
    uint8_t  abort_operation;
    /* obex parser */
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];
    /* object put */
    const char     *object_name;
    const char     *object_type;
    const uint8_t  *object_data;
    uint32_t  object_data_size;
    uint32_t  object_data_offset;
    uint32_t  object_total_size;
    uint32_t  object_total_pos;
    /* srm */
    obex_srm_client_t obex_srm;
} opp_client_t;

static opp_client_t opp_client_singleton;
static opp_client_t * opp_client = &opp_client_singleton;

static void opp_client_emit_connected_event(opp_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->opp_cid);
    pos+=2;
    event[pos++] = status;
    (void)memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("opp_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->opp_cid, &event[0], pos);
}

static void opp_client_emit_connection_closed_event(opp_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->opp_cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("opp_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->opp_cid, &event[0], pos);
}

static void opp_client_emit_push_object_data_event(opp_client_t * context,uint32_t cur_position, uint16_t buf_size){
    uint8_t event[11];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_PUSH_OBJECT_DATA;
    little_endian_store_16(event,pos,context->opp_cid);
    pos+=2;
    little_endian_store_32(event,pos,cur_position);
    pos+=4;
    little_endian_store_16(event,pos,buf_size);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("opp_client_push_object_data_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->opp_cid, &event[0], pos);
}

static void opp_client_emit_operation_complete_event(opp_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->opp_cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("opp_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->opp_cid, &event[0], pos);
}

static void obex_srm_init(obex_srm_client_t * obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

static void opp_client_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    opp_client_t * client = (opp_client_t *) user_data;
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

static void opp_client_parser_callback_get_operation(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    opp_client_t *client = (opp_client_t *) user_data;
    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&client->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&client->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch(opp_client->state){
                case OPP_W4_GET_DEFAULT_OBJECT:
                    client->client_handler(OPP_DATA_PACKET, client->opp_cid, (uint8_t *) data_buffer, data_len);
                    break;

                case OPP_W4_PUT_OBJECT:
                    break;

                default:
                    printf ("unexpected state: %d\n", opp_client->state);
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static void opp_client_prepare_srm_header(const opp_client_t * client) {
    obex_srm_client_prepare_header(&opp_client->obex_srm, opp_client->goep_cid);
}

static void opp_client_prepare_get_operation(opp_client_t * client){
    obex_parser_init_for_response(&client->obex_parser, OBEX_OPCODE_GET, opp_client_parser_callback_get_operation, opp_client);
    obex_srm_init(&client->obex_srm);
    client->obex_parser_waiting_for_response = true;
}

static void opp_client_prepare_put_operation(opp_client_t * client){
    obex_parser_init_for_response(&client->obex_parser, OBEX_OPCODE_PUT, opp_client_parser_callback_get_operation, opp_client);
    obex_srm_init(&client->obex_srm);
    client->obex_parser_waiting_for_response = true;
}

static void opp_client_handle_can_send_now(void){
    if (opp_client->abort_operation){
        opp_client->abort_operation = 0;
        // prepare request
        goep_client_request_create_abort(opp_client->goep_cid);
        // state
        opp_client->state = OPP_W4_ABORT_COMPLETE;
        // prepare response
        obex_parser_init_for_response(&opp_client->obex_parser, OBEX_OPCODE_ABORT, NULL, opp_client);
        obex_srm_init(&opp_client->obex_srm);
        opp_client->obex_parser_waiting_for_response = true;
        // send packet
        goep_client_execute(opp_client->goep_cid);
        return;
    }

    switch (opp_client->state){
        case OPP_W2_SEND_CONNECT_REQUEST:
            // prepare request
            goep_client_request_create_connect(opp_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            // state
            opp_client->state = OPP_W4_CONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&opp_client->obex_parser, OBEX_OPCODE_CONNECT, opp_client_parser_callback_connect, opp_client);
            obex_srm_init(&opp_client->obex_srm);
            opp_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(opp_client->goep_cid);
            break;

        case OPP_W2_SEND_DISCONNECT_REQUEST:
            // prepare request
            goep_client_request_create_disconnect(opp_client->goep_cid);
            // state
            opp_client->state = OPP_W4_DISCONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&opp_client->obex_parser, OBEX_OPCODE_DISCONNECT, NULL, opp_client);
            obex_srm_init(&opp_client->obex_srm);
            opp_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(opp_client->goep_cid);
            return;

        case OPP_W2_PUT_OBJECT:
            // prepare request
            goep_client_request_create_put(opp_client->goep_cid);

            // first PUT request adds info headers
            if (opp_client->request_number == 0){
                opp_client_prepare_srm_header(opp_client);
                goep_client_header_add_name(opp_client->goep_cid, opp_client->object_name);
                goep_client_header_add_type(opp_client->goep_cid, opp_client->object_type);
                goep_client_header_add_length(opp_client->goep_cid, opp_client->object_total_size);
            }

            uint32_t used_space = 0;
            uint32_t chunk_pos = opp_client->object_total_pos - opp_client->object_data_offset;

            goep_client_body_fillup_static (opp_client->goep_cid, opp_client->object_data + chunk_pos, opp_client->object_data_size - chunk_pos, &used_space);
            opp_client->object_total_pos += used_space;

            // state
            opp_client_prepare_put_operation(opp_client);
            opp_client->request_number++;
            if (opp_client->object_total_pos >= opp_client->object_total_size) {
                // if the object has been transferred completely
                // wait for the confirmation.
                opp_client->state = OPP_W4_PUT_OBJECT;
                goep_client_execute_with_final_bit(opp_client->goep_cid, true);
            } else {
                // there still is more data to transfer
                goep_client_execute_with_final_bit(opp_client->goep_cid, false);

                if (opp_client->obex_srm.srm_state == OBEX_SRM_CLIENT_STATE_ENABLED) {
                    opp_client->state = OPP_W2_PUT_OBJECT;
                } else {
                    opp_client->state = OPP_W4_PUT_OBJECT;
                }

                if (chunk_pos + used_space >= opp_client->object_data_size) {
                    // we've used up all the data of the current chunk,
                    // request the next one.
                    uint16_t buf_size;
                    opp_client->object_data = NULL;
                    buf_size = goep_client_body_get_outgoing_buffer_len(opp_client->goep_cid);
                    opp_client_emit_push_object_data_event(opp_client, opp_client->object_total_pos, buf_size);
                } else {
                    // we do have data readily available. If SRM is enabled
                    // we immediately can request the next can_send.
                    if (opp_client->obex_srm.srm_state == OBEX_SRM_CLIENT_STATE_ENABLED) {
                        goep_client_request_can_send_now(opp_client->goep_cid);
                    }
                }
            }
            break;

        case OPP_W2_GET_DEFAULT_OBJECT:
            // prepare request
            goep_client_request_create_get(opp_client->goep_cid);
            if (opp_client->request_number == 0){
                opp_client_prepare_srm_header(opp_client);
                // no name for the default object
                // type is fixed
                goep_client_header_add_type(opp_client->goep_cid, "text/x-vcard");
            }
            // state
            opp_client->state = OPP_W4_GET_DEFAULT_OBJECT;
            opp_client_prepare_get_operation(opp_client);
            // send packet
            opp_client->request_number++;
            goep_client_execute(opp_client->goep_cid);
            break;

        default:
            break;
    }
}

static void opp_client_packet_handler_hci(uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t status;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)){
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    status = goep_subevent_connection_opened_get_status(packet);
                    opp_client->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                    opp_client->incoming = goep_subevent_connection_opened_get_incoming(packet);
                    goep_subevent_connection_opened_get_bd_addr(packet, opp_client->bd_addr);
                    if (status){
                        log_info("opp: connection failed %u", status);
                        opp_client->state = OPP_INIT;
                        opp_client_emit_connected_event(opp_client, status);
                    } else {
                        log_info("opp: connection established");
                        opp_client->goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                        opp_client->state = OPP_W2_SEND_CONNECT_REQUEST;
                        goep_client_request_can_send_now(opp_client->goep_cid);
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    if (opp_client->state > OPP_CONNECTED){
                        opp_client_emit_operation_complete_event(opp_client, OBEX_DISCONNECTED);
                    }
                    opp_client->state = OPP_INIT;
                    opp_client_emit_connection_closed_event(opp_client);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    opp_client_handle_can_send_now();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void opp_client_packet_handler_goep(uint8_t *packet, uint16_t size){
    if (opp_client->obex_parser_waiting_for_response == false) return;

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&opp_client->obex_parser, packet, size);
    if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
        opp_client->obex_parser_waiting_for_response = false;
        obex_parser_operation_info_t op_info;
        obex_parser_get_operation_info(&opp_client->obex_parser, &op_info);
        switch (opp_client->state){
            case OPP_W4_CONNECT_RESPONSE:
                switch (op_info.response_code) {
                    case OBEX_RESP_SUCCESS:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_connected_event(opp_client, ERROR_CODE_SUCCESS);
                        break;
                    default:
                        log_info("opp: obex connect failed, result 0x%02x", packet[0]);
                        opp_client->state = OPP_INIT;
                        opp_client_emit_connected_event(opp_client, OBEX_CONNECT_FAILED);
                }
                break;
            case OPP_W4_DISCONNECT_RESPONSE:
                goep_client_disconnect(opp_client->goep_cid);
                break;
            case OPP_W4_PUT_OBJECT:
                switch (op_info.response_code) {
                    case OBEX_RESP_CONTINUE:
                        obex_srm_client_handle_headers(&opp_client->obex_srm);

                        if (opp_client->object_total_pos < opp_client->object_total_size) {
                            opp_client->state = OPP_W2_PUT_OBJECT;
                            if (opp_client->object_data) {
                                goep_client_request_can_send_now(opp_client->goep_cid);
                            } else {
                                uint16_t buf_size;

                                buf_size = goep_client_body_get_outgoing_buffer_len(opp_client->goep_cid);
                                opp_client_emit_push_object_data_event(opp_client, opp_client->object_total_pos, buf_size);
                            }
                        } else {
                            opp_client->state = OPP_CONNECTED;
                            opp_client_emit_operation_complete_event(opp_client, ERROR_CODE_SUCCESS);
                        }
                        break;
                    case OBEX_RESP_SUCCESS:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, ERROR_CODE_SUCCESS);
                        break;
                    case OBEX_RESP_NOT_IMPLEMENTED:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_UNKNOWN_ERROR);
                        break;
                    case OBEX_RESP_NOT_FOUND:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_NOT_FOUND);
                        break;
                    case OBEX_RESP_UNAUTHORIZED:
                    case OBEX_RESP_FORBIDDEN:
                    case OBEX_RESP_NOT_ACCEPTABLE:
                    case OBEX_RESP_UNSUPPORTED_MEDIA_TYPE:
                    case OBEX_RESP_ENTITY_TOO_LARGE:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_NOT_ACCEPTABLE);
                        break;
                    default:
                        log_info("unexpected obex response 0x%02x", op_info.response_code);
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_UNKNOWN_ERROR);
                        break;
                }
                break;

            case OPP_W4_GET_DEFAULT_OBJECT:
                switch (op_info.response_code) {
                    case OBEX_RESP_CONTINUE:
                        obex_srm_client_handle_headers(&opp_client->obex_srm);
                        if (opp_client->obex_srm.srm_state == OBEX_SRM_CLIENT_STATE_ENABLED) {
                            // prepare response
                            opp_client_prepare_get_operation(opp_client);
                            break;
                        }
                        opp_client->state = OPP_W2_GET_DEFAULT_OBJECT;
                        goep_client_request_can_send_now(opp_client->goep_cid);
                        break;
                    case OBEX_RESP_SUCCESS:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, ERROR_CODE_SUCCESS);
                        break;
                    case OBEX_RESP_NOT_IMPLEMENTED:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_UNKNOWN_ERROR);
                        break;
                    case OBEX_RESP_NOT_FOUND:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_NOT_FOUND);
                        break;
                    case OBEX_RESP_UNAUTHORIZED:
                    case OBEX_RESP_FORBIDDEN:
                    case OBEX_RESP_NOT_ACCEPTABLE:
                    case OBEX_RESP_UNSUPPORTED_MEDIA_TYPE:
                    case OBEX_RESP_ENTITY_TOO_LARGE:
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_NOT_ACCEPTABLE);
                        break;
                    default:
                        log_info("unexpected obex response 0x%02x", op_info.response_code);
                        opp_client->state = OPP_CONNECTED;
                        opp_client_emit_operation_complete_event(opp_client, OBEX_UNKNOWN_ERROR);
                        break;
                }
                break;
            case OPP_W4_ABORT_COMPLETE:
                opp_client->state = OPP_CONNECTED;
                opp_client_emit_operation_complete_event(opp_client, OBEX_ABORTED);
                break;
            default:
                btstack_unreachable();
                break;
        }
    }
}

static void opp_client_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events

    switch (packet_type){
        case HCI_EVENT_PACKET:
            opp_client_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            opp_client_packet_handler_goep(packet, size);
            break;
        default:
            break;
    }
}

static void opp_client_reset_state(void) {
    opp_client->state = OPP_INIT;
    opp_client->opp_cid = 1;
}

void opp_client_init(void){
    opp_client_reset_state();
}

void opp_client_deinit(void){
    opp_client_reset_state();
    memset(opp_client, 0, sizeof(opp_client_t));
}

uint8_t opp_client_create_connection(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (opp_client->state != OPP_INIT){
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    l2cap_ertm_config_t *l2cap_ertm_config = NULL;
    uint8_t *l2cap_ertm_buffer = NULL;
    uint16_t l2cap_ertm_buffer_size = 0;
    // singleton instance
    static goep_client_t opp_client_goep_singleton_goep_client;

#ifdef ENABLE_GOEP_L2CAP

    static uint8_t opp_client_singleton_ertm_buffer[1000];
    static l2cap_ertm_config_t opp_client_singleton_ertm_config = {
            1,  // ertm mandatory
            2,  // max transmit, some tests require > 1
            2000,
            12000,
            512,    // l2cap ertm mtu
            2,
            2,
            1,      // 16-bit FCS
    };

    l2cap_ertm_config = &opp_client_singleton_ertm_config;
    l2cap_ertm_buffer = opp_client_singleton_ertm_buffer;
    l2cap_ertm_buffer_size = sizeof(opp_client_singleton_ertm_buffer);
#endif

    opp_client->state = OPP_W4_GOEP_CONNECTION;
    opp_client->client_handler = handler;

    uint8_t err = goep_client_connect(&opp_client_goep_singleton_goep_client, l2cap_ertm_config, l2cap_ertm_buffer, l2cap_ertm_buffer_size,
                                      &opp_client_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_OBEX_OBJECT_PUSH, 0, &opp_client->goep_cid);
    *out_cid = opp_client->opp_cid;
    if (err) return err;
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_client_disconnect(uint16_t opp_cid){
    UNUSED(opp_cid);
    if (opp_client->state < OPP_CONNECTED){
        return BTSTACK_BUSY;
    }
    opp_client->state = OPP_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(opp_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_client_push_object(uint16_t        opp_cid,
                               const char     *name,
                               const char     *type,
                               const uint8_t  *data,
                               uint32_t        size)
{
    UNUSED(opp_cid);
    if (opp_client->state != OPP_CONNECTED){
        return BTSTACK_BUSY;
    }
    opp_client->object_name = name;
    opp_client->object_type = type;
    opp_client->object_data = data;
    opp_client->object_data_offset = 0;
    if (opp_client->object_data) {
        opp_client->object_data_size = size;
    } else {
        opp_client->object_data_size = 0;
    }
    opp_client->object_total_size = size;
    opp_client->object_total_pos  = 0;

    opp_client->state = OPP_W2_PUT_OBJECT;
    opp_client->request_number = 0;

    if (opp_client->object_data) {
        goep_client_request_can_send_now(opp_client->goep_cid);
    } else {
        uint16_t buf_size;

        buf_size = goep_client_body_get_outgoing_buffer_len(opp_client->goep_cid);
        opp_client_emit_push_object_data_event(opp_client, 0, buf_size);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_client_push_object_chunk(uint16_t       opp_cid,
                                     const uint8_t *chunk_data,
                                     uint32_t       chunk_offset,
                                     uint32_t       chunk_size)
{
    UNUSED(opp_cid);
    if (opp_client->state != OPP_W2_PUT_OBJECT && opp_client->state != OPP_W4_PUT_OBJECT){
        log_error("opp_client_push_object_chunk called without an ongoing PUT state");
        return BTSTACK_BUSY;
    }

    if (opp_client->object_data != NULL) {
        log_error("opp_client_push_object_chunk called while we still have outgoing data");
        return BTSTACK_BUSY;
    }

    opp_client->object_data = chunk_data;
    opp_client->object_data_offset = chunk_offset;
    opp_client->object_data_size = chunk_size;

    // if we're in W2_PUT (usually when SRM is enabled)
    // request the next can_send
    if (opp_client->state == OPP_W2_PUT_OBJECT) {
        goep_client_request_can_send_now(opp_client->goep_cid);
    }

    return ERROR_CODE_SUCCESS;
}

uint8_t opp_client_pull_default_object(uint16_t opp_cid){
    UNUSED(opp_cid);
    if (opp_client->state != OPP_CONNECTED){
        return BTSTACK_BUSY;
    }
    opp_client->state = OPP_W2_GET_DEFAULT_OBJECT;
    opp_client->request_number = 0;
    goep_client_request_can_send_now(opp_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_client_abort(uint16_t opp_cid){
    UNUSED(opp_cid);
    if ((opp_client->state < OPP_CONNECTED) || (opp_client->abort_operation != 0)){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    log_info("abort current operation, state 0x%02x", opp_client->state);
    opp_client->abort_operation = 1;
    return ERROR_CODE_SUCCESS;
}
