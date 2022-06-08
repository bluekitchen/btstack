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
#include "classic/goep_client.h"
#include "classic/opp_client.h"

// 796135f0-f0c5-11d8-0966- 0800200c9a66
static const uint8_t opp_uuid[] = { 0x79, 0x61, 0x35, 0xf0, 0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};

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
    // abort operation
    OPP_W4_ABORT_COMPLETE,

} opp_state_t;

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

typedef struct opp_client {
    opp_state_t state;
    uint16_t  cid;
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
    /* srm */
    obex_srm_t obex_srm;
    srm_state_t srm_state;
} opp_client_t;

static opp_client_t opp_client_singleton;
static opp_client_t * opp_client = &opp_client_singleton;

static void opp_client_emit_connected_event(opp_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    (void)memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static void opp_client_emit_connection_closed_event(opp_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("opp_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static void opp_client_emit_operation_complete_event(opp_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    pos++;  // skip len
    event[pos++] = OPP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("opp_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void obex_srm_init(obex_srm_t * obex_srm){
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
                case 0:
                    client->client_handler(OPP_DATA_PACKET, client->cid, (uint8_t *) data_buffer, data_len);
                    break;

                default:
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static void opp_client_prepare_srm_header(const opp_client_t * client){
    if (goep_client_version_20_or_higher(client->goep_cid)){
        goep_client_header_add_srm_enable(client->goep_cid);
        opp_client->srm_state = SRM_W4_CONFIRM;
    }
}

static void opp_client_prepare_get_operation(opp_client_t * client){
    obex_parser_init_for_response(&client->obex_parser, OBEX_OPCODE_GET, opp_client_parser_callback_get_operation, opp_client);
    obex_srm_init(&client->obex_srm);
    client->obex_parser_waiting_for_response = true;
}

static void opp_handle_can_send_now(void){
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
            goep_client_header_add_target(opp_client->goep_cid, opp_uuid, 16);
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
        default:
            break;
    }
}

static void opp_client_handle_srm_headers(opp_client_t *context) {
    const obex_srm_t * obex_srm = &opp_client->obex_srm;
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

static void opp_packet_handler_hci(uint8_t *packet, uint16_t size){
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
                    opp_handle_can_send_now();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void opp_packet_handler_goep(uint8_t *packet, uint16_t size){
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
                        break;
                }
                break;
            case OPP_W4_DISCONNECT_RESPONSE:
                goep_client_disconnect(opp_client->goep_cid);
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

static void opp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events

    switch (packet_type){
        case HCI_EVENT_PACKET:
            opp_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            opp_packet_handler_goep(packet, size);
            break;
        default:
            break;
    }
}

static void opp_client_reset_state(void) {
    opp_client->state = OPP_INIT;
    opp_client->cid = 1;
}

void opp_client_init(void){
    opp_client_reset_state();
}

void opp_client_deinit(void){
    opp_client_reset_state();
    memset(opp_client, 0, sizeof(opp_client_t));
}

uint8_t opp_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (opp_client->state != OPP_INIT){
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    opp_client->state = OPP_W4_GOEP_CONNECTION;
    opp_client->client_handler = handler;

    uint8_t err = goep_client_create_connection(&opp_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE, &opp_client->goep_cid);
    *out_cid = opp_client->cid;
    if (err) return err;
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_disconnect(uint16_t opp_cid){
    UNUSED(opp_cid);
    if (opp_client->state < OPP_CONNECTED){
        return BTSTACK_BUSY;
    }
    opp_client->state = OPP_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(opp_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_abort(uint16_t opp_cid){
    UNUSED(opp_cid);
    if ((opp_client->state < OPP_CONNECTED) || (opp_client->abort_operation != 0)){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    log_info("abort current operation, state 0x%02x", opp_client->state);
    opp_client->abort_operation = 1;
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_next_packet(uint16_t opp_cid){
    // log_info("opp_next_packet, state %x", opp_client->state);
    UNUSED(opp_cid);
    return ERROR_CODE_SUCCESS;
}

