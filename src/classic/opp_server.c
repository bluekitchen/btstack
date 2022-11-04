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

#define BTSTACK_FILE__ "opp_server.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "l2cap.h"
#include "bluetooth_sdp.h"
#include "btstack_event.h"

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/goep_server.h"
#include "classic/sdp_util.h"
#include "classic/opp_server.h"

typedef enum {
    OPP_SERVER_STATE_W4_OPEN,
    OPP_SERVER_STATE_W4_CONNECT_OPCODE,
    OPP_SERVER_STATE_W4_CONNECT_REQUEST,
    OPP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR,
    OPP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS,
    OPP_SERVER_STATE_CONNECTED,
    OPP_SERVER_STATE_W4_REQUEST,
    OPP_SERVER_STATE_W4_USER_DATA,
    OPP_SERVER_STATE_W4_GET_OPCODE,
    OPP_SERVER_STATE_W4_GET_REQUEST,
    OPP_SERVER_STATE_W4_PUT_OPCODE,
    OPP_SERVER_STATE_W4_PUT_REQUEST,
    OPP_SERVER_STATE_SEND_PUT_RESPONSE,
    OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE,
    OPP_SERVER_STATE_SEND_USER_RESPONSE,
    OPP_SERVER_STATE_SEND_DISCONNECT_RESPONSE,
    OPP_SERVER_STATE_ABOUT_TO_SEND,
} opp_server_state_t;

typedef struct {
    uint8_t srm_value;
    uint8_t srmp_value;
} obex_srm_t;

typedef enum {
    SRM_DISABLED,
    SRM_SEND_CONFIRM,
    SRM_SEND_CONFIRM_WAIT,
    SRM_ENABLED,
    SRM_ENABLED_WAIT,
} srm_state_t;

static  btstack_packet_handler_t opp_server_user_packet_handler;

typedef struct {
    uint16_t opp_cid;
    uint16_t goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    bool     incoming;
    opp_server_state_t state;
    obex_parser_t obex_parser;
    uint16_t opp_supported_features;
    // SRM
    obex_srm_t  obex_srm;
    srm_state_t srm_state;
    // request
    struct {
        char name[OPP_SERVER_MAX_NAME_LEN];
        char type[OPP_SERVER_MAX_TYPE_LEN];
        uint32_t continuation;
    } request;
    // response
    struct {
        uint8_t code;
    } response;
} opp_server_t;

static opp_server_t opp_server_singleton;

static void opp_server_handle_get_request(opp_server_t * opp_server);
static void opp_server_build_response(opp_server_t * opp_server);


static opp_server_t * opp_server_for_goep_cid(uint16_t goep_cid){
    // TODO: check goep_cid after incoming connection -> accept/reject is implemented and state has been setup
    // return opp_server_singleton.goep_cid == goep_cid ? &opp_server_singleton : NULL;
    return &opp_server_singleton;
}

static opp_server_t * opp_server_for_opp_cid(uint16_t opp_cid){
    return (opp_cid == opp_server_singleton.opp_cid) ? &opp_server_singleton : NULL;
}

static void opp_server_finalize_connection(opp_server_t * opp_server){
    // minimal
    opp_server->state = OPP_SERVER_STATE_W4_OPEN;
}

void opp_server_create_sdp_record(uint8_t *service, uint32_t service_record_handle, uint8_t rfcomm_channel_nr,
                                  uint16_t l2cap_psm, const char *name, uint8_t num_supported_formats,
                                  const uint8_t * supported_formats) {
    uint8_t *attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_OBEX_OBJECT_PUSH);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);

        uint8_t *rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);
            de_add_number(rfcomm, DE_UINT, DE_SIZE_8, rfcomm_channel_nr);
        }
        de_pop_sequence(attribute, rfcomm);

        uint8_t *obexProtocol = de_push_sequence(attribute);
        {
            de_add_number(obexProtocol, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_OBEX);
        }
        de_pop_sequence(attribute, obexProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *oppServerProfile = de_push_sequence(attribute);
        {
            de_add_number(oppServerProfile, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_OBEX_OBJECT_PUSH);
            de_add_number(oppServerProfile, DE_UINT, DE_SIZE_16, 0x0102); // Version 1.2
        }
        de_pop_sequence(attribute, oppServerProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service, DE_STRING, strlen(name), (uint8_t *) name);

#ifdef ENABLE_GOEP_L2CAP
    // 0x0200 "GOEP L2CAP PSM"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_GOEP_L2CAP_PSM);
    de_add_number(service, DE_UINT, DE_SIZE_16, l2cap_psm);
#endif

    // 0x0314 "Supported Formats List" - DES of Uint8
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SUPPORTED_FORMATS_LIST);
    attribute = de_push_sequence(service);{
        uint8_t i;
        for (i=0;i<num_supported_formats;i++){
            de_add_number(attribute, DE_UINT, DE_SIZE_8, supported_formats[i]);
        }
    }
    de_pop_sequence(service, attribute);
}

static void obex_srm_init(obex_srm_t * obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

static void opp_server_handle_srm_headers(opp_server_t *opp_server) {
    const obex_srm_t *obex_srm = &opp_server->obex_srm;
    // Update SRM state based on SRM headers
    switch (opp_server->srm_state) {
        case SRM_DISABLED:
            if (obex_srm->srm_value == OBEX_SRM_ENABLE) {
                if (obex_srm->srmp_value == OBEX_SRMP_WAIT){
                    opp_server->srm_state = SRM_SEND_CONFIRM_WAIT;
                } else {
                    opp_server->srm_state = SRM_SEND_CONFIRM;
                }
            }
            break;
        case SRM_ENABLED_WAIT:
            if (obex_srm->srmp_value == OBEX_SRMP_NEXT){
                opp_server->srm_state = SRM_ENABLED;
            }
            break;
        default:
            break;
    }
}

static void opp_server_add_srm_headers(opp_server_t *opp_server){
    switch (opp_server->srm_state) {
        case SRM_SEND_CONFIRM:
            goep_server_header_add_srm_enable(opp_server->goep_cid);
            opp_server->srm_state = SRM_ENABLED;
            break;
        case SRM_SEND_CONFIRM_WAIT:
            goep_server_header_add_srm_enable(opp_server->goep_cid);
            opp_server->srm_state = SRM_ENABLED_WAIT;
            break;
        default:
            break;
    }
}

static void opp_server_add_application_parameters(const opp_server_t * opp_server, uint8_t * application_parameters, uint16_t len){
    if (len > 0){
        goep_server_header_add_application_parameters(opp_server->goep_cid, application_parameters, len);
    }
}

static void opp_server_reset_response(opp_server_t * opp_server){
    (void) memset(&opp_server->response, 0, sizeof(opp_server->response));
}

static void opp_server_operation_complete(opp_server_t * opp_server){
    opp_server->state = OPP_SERVER_STATE_CONNECTED;
    opp_server->srm_state = SRM_DISABLED;
    opp_server_reset_response(opp_server);
}

static void opp_server_handle_can_send_now(opp_server_t * opp_server){
    uint8_t response_code;
    log_info ("opp_server->state: %d\n", opp_server->state);
    switch (opp_server->state){
        case OPP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR:
            // prepare response
            goep_server_response_create_general(opp_server->goep_cid);
            // next state
            opp_server->state = OPP_SERVER_STATE_W4_CONNECT_OPCODE;
            // send packet
            goep_server_execute(opp_server->goep_cid, OBEX_RESP_BAD_REQUEST);
            break;
        case OPP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS:
            log_info ("can_send: response success\n");
            // prepare response
            goep_server_response_create_connect(opp_server->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            // next state
            opp_server_operation_complete(opp_server);
            // send packet
            goep_server_execute(opp_server->goep_cid, OBEX_RESP_SUCCESS);
            break;
        case OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE:
            // prepare response
            goep_server_response_create_general(opp_server->goep_cid);
            // next state
            response_code = opp_server->response.code;
            opp_server_operation_complete(opp_server);
            // send packet
            goep_server_execute(opp_server->goep_cid, response_code);
            break;
        case OPP_SERVER_STATE_SEND_USER_RESPONSE:
            // next state
            response_code = opp_server->response.code;
            if (response_code == OBEX_RESP_CONTINUE){
                opp_server_reset_response(opp_server);
                // next state
                opp_server->state = (opp_server->srm_state == SRM_ENABLED) ?  OPP_SERVER_STATE_ABOUT_TO_SEND : OPP_SERVER_STATE_W4_GET_OPCODE;
            } else {
                opp_server_operation_complete(opp_server);
            }
            // send packet
            goep_server_execute(opp_server->goep_cid, response_code);
            // trigger next user response in SRM
            if (opp_server->srm_state == SRM_ENABLED){
                opp_server_handle_get_request(opp_server);
            }
            break;
        case OPP_SERVER_STATE_SEND_DISCONNECT_RESPONSE:
        {
            // cache data
            uint16_t opp_cid = opp_server->opp_cid;
            uint16_t goep_cid = opp_server->goep_cid;

            // finalize connection
            opp_server_finalize_connection(opp_server);

            // respond to request
            goep_server_response_create_general(goep_cid);
            goep_server_execute(goep_cid, OBEX_RESP_SUCCESS);

            // emit event
            uint8_t event[2+3];
            uint16_t pos = 0;
            event[pos++] = HCI_EVENT_OPP_META;
            event[pos++] = 0;
            event[pos++] = OPP_SUBEVENT_CONNECTION_CLOSED;
            little_endian_store_16(event, pos, opp_cid);
            pos += 2;
            (*opp_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
            break;
        }
        case OPP_SERVER_STATE_SEND_PUT_RESPONSE:
        {
            // next state
            response_code = opp_server->response.code;
            goep_server_response_create_general(opp_server->goep_cid);
            if (response_code == OBEX_RESP_CONTINUE){
                // next state
                opp_server->state = OPP_SERVER_STATE_W4_PUT_OPCODE;
            } else {
                opp_server_operation_complete(opp_server);
            }
            // send packet
            goep_server_execute(opp_server->goep_cid, response_code);
            break;
        }
        default:
            break;
    }
}

static void opp_server_packet_handler_hci(uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t status;
    opp_server_t * opp_server;
    uint16_t goep_cid;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)){
                case GOEP_SUBEVENT_INCOMING_CONNECTION:
                    // TODO: check if resources available
                    goep_cid = goep_subevent_incoming_connection_get_goep_cid(packet);
                    goep_server_accept_connection(goep_cid);
                    break;
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    log_info ("GOEP opened\n");
                    goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                    opp_server = opp_server_for_goep_cid(goep_cid);
                    btstack_assert(opp_server != NULL);
                    status = goep_subevent_connection_opened_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        log_info("opp: connection failed %u", status);
                    } else {
                        log_info("opp: connection established");
                        opp_server->goep_cid = goep_cid;
                        goep_subevent_connection_opened_get_bd_addr(packet, opp_server->bd_addr);
                        opp_server->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                        opp_server->incoming = goep_subevent_connection_opened_get_incoming(packet);
                        opp_server->state = OPP_SERVER_STATE_W4_CONNECT_OPCODE;
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                    opp_server = opp_server_for_goep_cid(goep_cid);
                    btstack_assert(opp_server != NULL);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    goep_cid = goep_subevent_can_send_now_get_goep_cid(packet);
                    opp_server = opp_server_for_goep_cid(goep_cid);
                    btstack_assert(opp_server != NULL);
                    opp_server_handle_can_send_now(opp_server);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void opp_server_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    UNUSED(total_len);
    UNUSED(data_offset);

    // opp_server_t * opp_server = (opp_server_t *) user_data;

    switch (header_id) {
        default:
            break;
    }
}

static void opp_server_parser_callback_get(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    opp_server_t * opp_server = (opp_server_t *) user_data;

    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&opp_server->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&opp_server->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_CONNECTION_ID:
            // TODO: verify connection id
            break;
        case OBEX_HEADER_NAME:
            // name is stored in big endian unicode-16
            if (total_len < (OPP_SERVER_MAX_NAME_LEN * 2)){
                uint16_t i;
                for (i = 0; i < data_len ; i++){
                    if (((data_offset + i) & 1) == 1) {
                        opp_server->request.name[(data_offset + i) >> 1] = data_buffer[i];
                    }
                }
                opp_server->request.name[total_len / 2] = 0;
                log_info ("received OPP header name: %s\n",
                          opp_server->request.name);
            }
            break;
        case OBEX_HEADER_TYPE:
            if (total_len < OPP_SERVER_MAX_TYPE_LEN){
                memcpy(&opp_server->request.type[data_offset], data_buffer, data_len);
                opp_server->request.type[total_len] = 0;
                log_info ("received OPP header type: %s\n",
                          opp_server->request.type);
            }
            break;
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            log_info ("received (END_OF_)BODY data: %d bytes\n", data_len);
            (*opp_server_user_packet_handler)(OPP_DATA_PACKET, opp_server->opp_cid, (uint8_t *) data_buffer, data_len);
            break;
        case OBEX_HEADER_LENGTH:
            log_info ("length of data: %d\n",
                      big_endian_read_32 (data_buffer, 0));
            break;
        default:
            log_info ("received unhandled OPP header type %d\n", header_id);
            break;
    }
}

static void opp_server_handle_get_request(opp_server_t * opp_server){
    opp_server_handle_srm_headers(opp_server);
    if (strcmp("text/x-vcard", opp_server->request.type) != 0 ||
        opp_server->request.name[0] != '\0') {
        // wrong default object request
        if (opp_server->request.name[0] != '\0') {
            printf ("forbidden\n");
            opp_server->response.code = OBEX_RESP_FORBIDDEN;
        } else {
            printf ("wrong type\n");
            opp_server->response.code = OBEX_RESP_BAD_REQUEST;
        }
        opp_server->state = OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
        goep_server_request_can_send_now(opp_server->goep_cid);
        return;
    }

    uint8_t event[2+3];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_OPP_META;
    event[pos++] = 0;
    event[pos++] = OPP_SUBEVENT_PULL_DEFAULT_OBJECT;
    little_endian_store_16(event, pos, opp_server->opp_cid);
    pos += 2;
    (*opp_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);

    opp_server->state = OPP_SERVER_STATE_W4_USER_DATA;
}

static void opp_server_handle_put_request(opp_server_t * opp_server, uint8_t opcode){
    opp_server_handle_srm_headers(opp_server);

    log_info ("handle put request");
    // emit received opp data
    opp_server->state = OPP_SERVER_STATE_SEND_PUT_RESPONSE;

    // (*opp_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);

    opp_server->response.code = opcode & OBEX_OPCODE_FINAL_BIT_MASK ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE;
    goep_server_request_can_send_now(opp_server->goep_cid);
}

static void opp_server_packet_handler_goep(opp_server_t * opp_server, uint8_t *packet, uint16_t size){
    btstack_assert(size > 0);
    uint8_t opcode;
    obex_parser_object_state_t parser_state;

    switch (opp_server->state){
        case OPP_SERVER_STATE_W4_OPEN:
            btstack_unreachable();
            break;
        case OPP_SERVER_STATE_W4_CONNECT_OPCODE:
            opp_server->state = OPP_SERVER_STATE_W4_CONNECT_REQUEST;
            obex_parser_init_for_request(&opp_server->obex_parser, &opp_server_parser_callback_connect, (void*) opp_server);

            /* fall through */

        case OPP_SERVER_STATE_W4_CONNECT_REQUEST:
            parser_state = obex_parser_process_data(&opp_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&opp_server->obex_parser, &op_info);
                log_info ("op_info.opcode: %d\n", op_info.opcode);
                bool ok = true;
                // check opcode
                if (op_info.opcode != OBEX_OPCODE_CONNECT){
                    ok = false;
                }
                log_info ("ok: %d\n", ok);
                // TODO: check Target
                if (ok == false){
                    log_info ("considered error\n");
                    // send bad request response
                    opp_server->state = OPP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR;
                } else {
                    log_info ("considered success\n");
                    // send connect response
                    opp_server->state = OPP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS;
                }
                log_info ("opp_server->state (a): %d\n", opp_server->state);
                goep_server_request_can_send_now(opp_server->goep_cid);
            }
            break;
        case OPP_SERVER_STATE_CONNECTED:
            opcode = packet[0];
            // default headers
            switch (opcode){
                case OBEX_OPCODE_GET:
                case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
                    log_info ("handling GET\n");
                    opp_server->state = OPP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&opp_server->obex_parser, &opp_server_parser_callback_get, (void*) opp_server);
                    break;
                case OBEX_OPCODE_PUT:
                case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
                    log_info ("handling PUT\n");
                    opp_server->state = OPP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&opp_server->obex_parser, &opp_server_parser_callback_get, (void*) opp_server);
                    break;
                case OBEX_OPCODE_DISCONNECT:
                    opp_server->state = OPP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&opp_server->obex_parser, NULL, NULL);
                    break;
                case OBEX_OPCODE_ACTION:
                default:
                    opp_server->state = OPP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&opp_server->obex_parser, NULL, NULL);
                    break;
            }

            /* fall through */

        case OPP_SERVER_STATE_W4_REQUEST:
            obex_srm_init(&opp_server->obex_srm);
            parser_state = obex_parser_process_data(&opp_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&opp_server->obex_parser, &op_info);
                log_info ("W4_Request: opcode: %d\n", op_info.opcode);
                switch (op_info.opcode){
                    case OBEX_OPCODE_GET:
                    case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
                        opp_server_handle_get_request(opp_server);
                        break;
                    case OBEX_OPCODE_PUT:
                    case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
                        opp_server_handle_put_request(opp_server, op_info.opcode);
                        break;
                    case OBEX_OPCODE_DISCONNECT:
                        opp_server->state = OPP_SERVER_STATE_SEND_DISCONNECT_RESPONSE;
                        goep_server_request_can_send_now(opp_server->goep_cid);
                        break;
                    case OBEX_OPCODE_ACTION:
                    case (OBEX_OPCODE_ACTION | OBEX_OPCODE_FINAL_BIT_MASK):
                    case OBEX_OPCODE_SESSION:
                    default:
                        // send bad request response
                        opp_server->state = OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                        opp_server->response.code = OBEX_RESP_BAD_REQUEST;
                        goep_server_request_can_send_now(opp_server->goep_cid);
                        break;
                }
            }
            break;

        case OPP_SERVER_STATE_W4_GET_OPCODE:
            opp_server->state = OPP_SERVER_STATE_W4_GET_REQUEST;
            obex_parser_init_for_request(&opp_server->obex_parser, &opp_server_parser_callback_get, (void*) opp_server);

            /* fall through */

        case OPP_SERVER_STATE_W4_GET_REQUEST:
            obex_srm_init(&opp_server->obex_srm);
            parser_state = obex_parser_process_data(&opp_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&opp_server->obex_parser, &op_info);
                switch((op_info.opcode & 0x7f)){
                    case OBEX_OPCODE_GET:
                        opp_server_handle_get_request(opp_server);
                        break;
                    case (OBEX_OPCODE_ABORT & 0x7f):
                        opp_server->response.code = OBEX_RESP_SUCCESS;
                        opp_server->state = OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                        goep_server_request_can_send_now(opp_server->goep_cid);
                        break;
                    default:
                        // send bad request response
                        opp_server->state = OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                        opp_server->response.code = OBEX_RESP_BAD_REQUEST;
                        goep_server_request_can_send_now(opp_server->goep_cid);
                        break;
                }
            }
            break;

        case OPP_SERVER_STATE_W4_PUT_OPCODE:
            opp_server->state = OPP_SERVER_STATE_W4_PUT_REQUEST;
            obex_parser_init_for_request(&opp_server->obex_parser, &opp_server_parser_callback_get, (void*) opp_server);

            /* fall through */

        case OPP_SERVER_STATE_W4_PUT_REQUEST:
            obex_srm_init(&opp_server->obex_srm);
            parser_state = obex_parser_process_data(&opp_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&opp_server->obex_parser, &op_info);
                switch((op_info.opcode & 0x7f)){
                    case OBEX_OPCODE_PUT:
                        opp_server_handle_put_request(opp_server, op_info.opcode);
                        break;
                    case (OBEX_OPCODE_ABORT & 0x7f):
                        opp_server->response.code = OBEX_RESP_SUCCESS;
                        opp_server->state = OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                        goep_server_request_can_send_now(opp_server->goep_cid);
                        break;
                    default:
                        // send bad request response
                        opp_server->state = OPP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                        opp_server->response.code = OBEX_RESP_BAD_REQUEST;
                        goep_server_request_can_send_now(opp_server->goep_cid);
                        break;
                }
            }
            break;
        default:
            break;
    }
}

static void opp_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events
    opp_server_t * opp_server;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            opp_server_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            opp_server = opp_server_for_goep_cid(channel);
            btstack_assert(opp_server != NULL);
            opp_server_packet_handler_goep(opp_server, packet, size);
            break;
        default:
            break;
    }
}

uint8_t opp_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, gap_security_level_t security_level){
    uint8_t status = goep_server_register_service(&opp_server_packet_handler, rfcomm_channel_nr, 0xffff, l2cap_psm, 0xffff, security_level);
    opp_server_user_packet_handler = packet_handler;
    return status;
}

void opp_server_deinit(void){
}

uint8_t opp_connect(bd_addr_t addr, uint16_t * out_cid){
    return ERROR_CODE_SUCCESS;
}

uint8_t opp_disconnect(uint16_t opp_cid){
    return ERROR_CODE_SUCCESS;
}

// note: common code for next four setters
static bool opp_server_valid_header_for_request(opp_server_t * opp_server){
    if (opp_server->state != OPP_SERVER_STATE_W4_USER_DATA) {
        return false;
    }
    return true;

}

static void opp_server_build_response(opp_server_t * opp_server){
    if (opp_server->response.code == 0){
        // set interim response code
        opp_server->response.code = OBEX_RESP_SUCCESS;
        goep_server_response_create_general(opp_server->goep_cid);
        opp_server_add_srm_headers(opp_server);
    }
}

uint16_t opp_server_get_max_body_size(uint16_t opp_cid){
    opp_server_t * opp_server = opp_server_for_opp_cid(opp_cid);
    if (opp_server == NULL){
        return 0;
    }
    if (opp_server->state != OPP_SERVER_STATE_W4_USER_DATA) {
        return 0;
    }
    opp_server_build_response(opp_server);
    return goep_server_response_get_max_body_size(opp_server->goep_cid);
}

uint16_t opp_server_send_pull_response(uint16_t opp_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t * body){
    opp_server_t * opp_server = opp_server_for_opp_cid(opp_cid);
    if (opp_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // double check body size
    opp_server_build_response(opp_server);
    if (body_len > goep_server_response_get_max_body_size(opp_server->goep_cid)){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }
    // append body and execute
    goep_server_header_add_end_of_body(opp_server->goep_cid, body, body_len);
    // set final response code
    opp_server->response.code = response_code;
    opp_server->request.continuation = continuation;
    opp_server->state = OPP_SERVER_STATE_SEND_USER_RESPONSE;
    return goep_server_request_can_send_now(opp_server->goep_cid);
}

