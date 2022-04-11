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

#define BTSTACK_FILE__ "pbap_server.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "bluetooth_sdp.h"
#include "classic/sdp_client_rfcomm.h"
#include "btstack_event.h"
#include "md5.h"
#include "yxml.h"

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/goep_server.h"
#include "classic/sdp_util.h"
#include "classic/pbap.h"
#include "classic/pbap_server.h"

typedef enum {
    PBAP_SERVER_STATE_W4_OPEN,
    PBAP_SERVER_STATE_W4_CONNECT_OPCODE,
    PBAP_SERVER_STATE_W4_CONNECT_REQUEST,
    PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR,
    PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS,
    PBAP_SERVER_STATE_CONNECTED,
    PBAP_SERVER_STATE_W4_REQUEST,
    PBAP_SERVER_STATE_W4_GET_OPCODE,
    PBAP_SERVER_STATE_W4_GET_REQUEST,
    PBAP_SERVER_STATE_W4_PUT_OPCODE,
    PBAP_SERVER_STATE_W4_PUT_REQUEST,
    PBAP_SERVER_STATE_SEND_SUCCESS_RESPONSE,
    PBAP_SERVER_STATE_SEND_BAD_REQUEST_RESPONSE,
    PBAP_SERVER_STATE_SEND_EMPTY_RESPONSE,
    PBAP_SERVER_STATE_SEND_CONTINUE_DATA,
    PBAP_SERVER_STATE_SEND_FINAL_DATA,
} pbap_server_state_t;

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

typedef struct {
    uint16_t goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    bool     incoming;
    pbap_server_state_t state;
    obex_parser_t obex_parser;
    uint32_t connection_id;
    uint16_t pbap_supported_features;
    // SRM
    obex_srm_t  obex_srm;
    srm_state_t srm_state;
    // header fields
    struct {
        uint8_t name[PBAP_SERVER_MAX_NAME_LEN];
        uint8_t type[PBAP_SERVER_MAX_TYPE_LEN];
        obex_app_param_parser_t app_param_parser;
        uint8_t app_param_buffer[4];
        struct {
        } app_params;
    } headers;
} pbap_server_t;

static pbap_server_t pbap_server_singleton;

// 796135f0-f0c5-11d8-0966- 0800200c9a66
static const uint8_t pbap_uuid[] = { 0x79, 0x61, 0x35, 0xf0, 0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};

// dummy vcard
static const char dummy_vcard[] = "BEGIN:VCARD\n"
                                  "FN:Jean Dupont\n"
                                  "N:Dupont;Jean\n"
                                  "TEL;CELL;PREF:+1234 56789\n"
                                  "END:VCARD";

static uint8_t dummy_vacrd_counter;

static pbap_server_t * pbap_server_for_goep_cid(uint16_t goep_cid){
    // TODO: check goep_cid after incoming connection -> accept/reject is implemented and state has been setup
    // return pbap_server_singleton.goep_cid == goep_cid ? &pbap_server_singleton : NULL;
    return &pbap_server_singleton;
}

void pbap_server_create_sdp_record(uint8_t *service, uint32_t service_record_handle, uint8_t rfcomm_channel_nr,
                                   uint16_t l2cap_psm, const char *name, uint8_t supported_repositories,
                                   uint32_t pbap_supported_features) {
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);

        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);
            de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel_nr);
        }
        de_pop_sequence(attribute, rfcomm);

        uint8_t* obexProtocol = de_push_sequence(attribute);
        {
            de_add_number(obexProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_OBEX);
        }
        de_pop_sequence(attribute, obexProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *pbapServerProfile = de_push_sequence(attribute);
        {
            de_add_number(pbapServerProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS);
            de_add_number(pbapServerProfile,  DE_UINT, DE_SIZE_16, 0x0102); // Verision 1.2
        }
        de_pop_sequence(attribute, pbapServerProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);

#ifdef ENABLE_GOEP_L2CAP
    // 0x0200 "GOEP L2CAP PSM"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_GOEP_L2CAP_PSM);
    de_add_number(service, DE_UINT, DE_SIZE_16, l2cap_psm);
#endif

    // 0x0314 "Supported Repositories"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SUPPORTED_REPOSITORIES);
    de_add_number(service, DE_UINT, DE_SIZE_16, l2cap_psm);

    // 0x0317 "PBAP Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES);
    de_add_number(service, DE_UINT, DE_SIZE_16, l2cap_psm);
}

static void obex_srm_init(obex_srm_t * obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

static void pbap_server_handle_srm_headers(pbap_server_t *pbap_server) {
    const obex_srm_t *obex_srm = &pbap_server->obex_srm;
    // Update SRM state based on SRM headers
    printf("Process SRM %u / SRMP %u\n", obex_srm->srm_value, obex_srm->srmp_value);
    switch (pbap_server->srm_state) {
        case SRM_DISABLED:
            if (obex_srm->srm_value == OBEX_SRM_ENABLE) {
                if (obex_srm->srmp_value == OBEX_SRMP_WAIT){
                    printf("SRM_DISABLED -> SRM_SEND_CONFIRM_WAIT\n");
                    pbap_server->srm_state = SRM_SEND_CONFIRM_WAIT;
                } else {
                    printf("SRM_DISABLED -> SRM_SEND_CONFIRM\n");
                    pbap_server->srm_state = SRM_SEND_CONFIRM;
                }
            }
            break;
        case SRM_ENABLED_WAIT:
            if (obex_srm->srmp_value == OBEX_SRMP_NEXT){
                printf("SRM_ENABLED_WAIT -> SRM_ENABLED\n");
                pbap_server->srm_state = SRM_ENABLED;
            }
            break;
        default:
            break;
    }
}

static void pbap_server_add_srm_headers(pbap_server_t *pbap_server){
    switch (pbap_server->srm_state) {
        case SRM_SEND_CONFIRM:
            goep_server_header_add_srm_enable(pbap_server->goep_cid);
            printf("SRM_SEND_CONFIRM -> SRM_ENABLED\n");
            pbap_server->srm_state = SRM_ENABLED;
            break;
        case SRM_SEND_CONFIRM_WAIT:
            goep_server_header_add_srm_enable(pbap_server->goep_cid);
            printf("SRM_SEND_CONFIRM_WAIT -> SRM_ENABLED_WAIT\n");
            pbap_server->srm_state = SRM_ENABLED_WAIT;
            break;
        default:
            break;
    }
}

static void pbap_server_operation_complete(pbap_server_t * pbap_server){
    pbap_server->state = PBAP_SERVER_STATE_CONNECTED;
    pbap_server->srm_state = SRM_DISABLED;
    memset(&pbap_server->headers, 0, sizeof(pbap_server->headers));
}

static void pbap_server_handle_can_send_now(pbap_server_t * pbap_server){
    switch (pbap_server->state){
        case PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid, OBEX_RESP_BAD_REQUEST);
            // next state
            pbap_server->state = PBAP_SERVER_STATE_W4_CONNECT_OPCODE;
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            break;
        case PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS:
            // prepare response
            goep_server_response_create_connect(pbap_server->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_server_header_add_who(pbap_server->goep_cid, pbap_uuid);
            // next state
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            break;
        case PBAP_SERVER_STATE_SEND_EMPTY_RESPONSE:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid, OBEX_RESP_SUCCESS);
            goep_server_header_add_end_of_body(pbap_server->goep_cid, NULL, 0);
            // next state
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            break;
        case PBAP_SERVER_STATE_SEND_CONTINUE_DATA:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid, OBEX_RESP_CONTINUE);
            pbap_server_add_srm_headers(pbap_server);
            goep_server_header_add_end_of_body(pbap_server->goep_cid, (const uint8_t*) dummy_vcard, sizeof(dummy_vcard));
            printf("Send Continue Data: SRM State %u, card counter %u\n", pbap_server->srm_state, dummy_vacrd_counter);
            // next state
            if (pbap_server->srm_state != SRM_ENABLED){
                pbap_server->state = PBAP_SERVER_STATE_W4_GET_OPCODE;
            }
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            if (pbap_server->srm_state == SRM_ENABLED){
                if (dummy_vacrd_counter > 0){
                    dummy_vacrd_counter--;
                } else {
                    pbap_server->state = PBAP_SERVER_STATE_SEND_FINAL_DATA;
                }
                goep_server_request_can_send_now(pbap_server->goep_cid);
            }
            break;
        case PBAP_SERVER_STATE_SEND_FINAL_DATA:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid, OBEX_RESP_SUCCESS);
            goep_server_header_add_end_of_body(pbap_server->goep_cid, (const uint8_t*) dummy_vcard, sizeof(dummy_vcard));
            // next state
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            break;
        case PBAP_SERVER_STATE_SEND_SUCCESS_RESPONSE:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid, OBEX_RESP_SUCCESS);
            // next state
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            break;
        case PBAP_SERVER_STATE_SEND_BAD_REQUEST_RESPONSE:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid, OBEX_RESP_BAD_REQUEST);
            // next state
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid);
            break;
        default:
            break;
    }
}

static void pbap_server_packet_handler_hci(uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t status;
    pbap_server_t * pbap_server;
    uint16_t goep_cid;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)){
                case GOEP_SUBEVENT_INCOMING_CONNECTION:
                    // TODO: check if resources available
                    printf("Accept incoming connection\n");
                    goep_server_accept_connection(goep_subevent_incoming_connection_get_goep_cid(packet));
                    break;
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                    pbap_server = pbap_server_for_goep_cid(goep_cid);
                    btstack_assert(pbap_server != NULL);
                    status = goep_subevent_connection_opened_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        // TODO: report failed outgoing connection
                    } else {
                        pbap_server->goep_cid = goep_cid;
                        goep_subevent_connection_opened_get_bd_addr(packet, pbap_server->bd_addr);
                        pbap_server->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                        pbap_server->incoming = goep_subevent_connection_opened_get_incoming(packet);
                        pbap_server->state = PBAP_SERVER_STATE_W4_CONNECT_OPCODE;
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                    pbap_server = pbap_server_for_goep_cid(goep_cid);
                    btstack_assert(pbap_server != NULL);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    goep_cid = goep_subevent_can_send_now_get_goep_cid(packet);
                    pbap_server = pbap_server_for_goep_cid(goep_cid);
                    btstack_assert(pbap_server != NULL);
                    pbap_server_handle_can_send_now(pbap_server);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}
static void pbap_server_app_param_callback(void * user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len){
    pbap_server_t * pbap_server = (pbap_server_t *) user_data;
    if (tag_id == PBAP_APPLICATION_PARAMETER_PBAP_SUPPORTED_FEATURES){
        obex_app_param_parser_tag_state_t state = obex_app_param_parser_tag_store(pbap_server->headers.app_param_buffer, sizeof(pbap_server->headers.app_param_buffer), total_len, data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE){
            pbap_server->pbap_supported_features = big_endian_read_32(pbap_server->headers.app_param_buffer, 0);
            printf(" - PBAP Supported Features %04x\n", pbap_server->pbap_supported_features);
        }
    }
}

static void pbap_server_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    UNUSED(total_len);
    UNUSED(data_offset);

    printf("CONNECT Header: %02x - len %u - ", header_id, data_len);
    printf_hexdump(data_buffer, data_len);

    pbap_server_t * pbap_server = (pbap_server_t *) user_data;

    switch (header_id) {
        case OBEX_HEADER_TARGET:
            // TODO: verify target
            break;
        case OBEX_HEADER_APPLICATION_PARAMETERS:
            if (data_offset == 0){
                obex_app_param_parser_init(&pbap_server->headers.app_param_parser, &pbap_server_app_param_callback, total_len, pbap_server);
            }
            obex_app_param_parser_process_data(&pbap_server->headers.app_param_parser, data_buffer, data_len);
            break;
        default:
            break;
    }
}

static void pbap_server_parser_callback_get(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    printf("GET Header: %02x - len %u - ", header_id, data_len);
    printf_hexdump(data_buffer, data_len);

    pbap_server_t * pbap_server = (pbap_server_t *) user_data;

    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&pbap_server->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&pbap_server->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_CONNECTION_ID:
            printf("- todo: verify connection id\n");
            // TODO: verify connection id
            break;
        case OBEX_HEADER_NAME:
            // name is stored in big endian unicode-16
            if (total_len < (PBAP_SERVER_MAX_NAME_LEN * 2)){
                uint16_t i;
                for (i = 0; i < data_len ; i++){
                    if (((data_offset + i) & 1) == 1) {
                        pbap_server->headers.name[(data_offset + i) >> 1] = data_buffer[i];
                    }
                }
                pbap_server->headers.name[total_len/2] = 0;
                printf("- Name: '%s'\n", pbap_server->headers.name);
            }
            break;
        case OBEX_HEADER_TYPE:
            if (total_len < PBAP_SERVER_MAX_TYPE_LEN){
                memcpy(&pbap_server->headers.type[data_offset], data_buffer, data_len);
                pbap_server->headers.type[total_len] = 0;
                printf("- Type: '%s'\n", pbap_server->headers.type);
            }
            break;
        case OBEX_HEADER_APPLICATION_PARAMETERS:
            printf("- todo: parse app params\n");
            break;
        default:
            break;
    }
}

static void pbap_server_packet_handler_goep(pbap_server_t * pbap_server, uint8_t *packet, uint16_t size){
    btstack_assert(size > 0);
    uint8_t opcode;
    obex_parser_object_state_t parser_state;

    printf("GOEP Data: ");
    printf_hexdump(packet, size);
    switch (pbap_server->state){
        case PBAP_SERVER_STATE_W4_OPEN:
            btstack_unreachable();
            break;
        case PBAP_SERVER_STATE_W4_CONNECT_OPCODE:
            pbap_server->state = PBAP_SERVER_STATE_W4_CONNECT_REQUEST;
            obex_parser_init_for_request(&pbap_server->obex_parser, &pbap_server_parser_callback_connect, (void*) pbap_server);

            /* fall through */

        case PBAP_SERVER_STATE_W4_CONNECT_REQUEST:
            parser_state = obex_parser_process_data(&pbap_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&pbap_server->obex_parser, &op_info);
                bool ok = true;
                // check opcode
                if (op_info.opcode != OBEX_OPCODE_CONNECT){
                    ok = false;
                }
                // TODO: check Target
                if (ok == false){
                    // send bad request response
                    pbap_server->state = PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR;
                } else {
                    // send connect response
                    pbap_server->state = PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS;
                }
                goep_server_request_can_send_now(pbap_server->goep_cid);
            }
            break;
        case PBAP_SERVER_STATE_CONNECTED:
            opcode = packet[0];
            switch (opcode){
                case OBEX_OPCODE_GET:
                case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
                    pbap_server->state = PBAP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&pbap_server->obex_parser, &pbap_server_parser_callback_get, (void*) pbap_server);
                    break;
                case OBEX_OPCODE_SETPATH:
                    pbap_server->state = PBAP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&pbap_server->obex_parser, &pbap_server_parser_callback_get, (void*) pbap_server);
                    break;
                case OBEX_OPCODE_DISCONNECT:
                    pbap_server->state = PBAP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&pbap_server->obex_parser, NULL, NULL);
                    break;
                case OBEX_OPCODE_ACTION:
                default:
                    pbap_server->state = PBAP_SERVER_STATE_W4_REQUEST;
                    obex_parser_init_for_request(&pbap_server->obex_parser, NULL, NULL);
                    break;
            }

            /* fall through */

        case PBAP_SERVER_STATE_W4_REQUEST:
            obex_srm_init(&pbap_server->obex_srm);
            parser_state = obex_parser_process_data(&pbap_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&pbap_server->obex_parser, &op_info);
                switch (op_info.opcode){
                    case OBEX_OPCODE_GET:
                    case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
                        pbap_server_handle_srm_headers(pbap_server);
                        // test get request - return 5 empty vcards
                        dummy_vacrd_counter = 10;
                        pbap_server->state = PBAP_SERVER_STATE_SEND_CONTINUE_DATA;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                    case OBEX_OPCODE_SETPATH:
                        // test set path request - return ok
                        printf("- SetPath: flags %02x, name %s\n", op_info.flags, pbap_server->headers.name);
                        pbap_server->state = PBAP_SERVER_STATE_SEND_SUCCESS_RESPONSE;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                    case OBEX_OPCODE_DISCONNECT:
                        // todo: emit disconnect
                        break;
                    case OBEX_OPCODE_ACTION:
                    case (OBEX_OPCODE_ACTION | OBEX_OPCODE_FINAL_BIT_MASK):
                    case OBEX_OPCODE_SESSION:
                    default:
                        pbap_server->state = PBAP_SERVER_STATE_SEND_BAD_REQUEST_RESPONSE;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                }
            }
            break;

        case PBAP_SERVER_STATE_W4_GET_OPCODE:
            pbap_server->state = PBAP_SERVER_STATE_W4_CONNECT_REQUEST;
            obex_parser_init_for_request(&pbap_server->obex_parser, &pbap_server_parser_callback_get, (void*) pbap_server);

            /* fall through */

        case PBAP_SERVER_STATE_W4_GET_REQUEST:
            obex_srm_init(&pbap_server->obex_srm);
            parser_state = obex_parser_process_data(&pbap_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&pbap_server->obex_parser, &op_info);
                bool ok = true;
                // TODO: check opcode
                // TODO: check Target
                if (ok == false) {
                    // send bad request response
                    pbap_server->state = PBAP_SERVER_STATE_SEND_BAD_REQUEST_RESPONSE;
                } else {
                    pbap_server_handle_srm_headers(pbap_server);
                    // send next card
                    if (dummy_vacrd_counter > 0){
                        dummy_vacrd_counter--;
                        pbap_server->state = PBAP_SERVER_STATE_SEND_CONTINUE_DATA;
                    } else {
                        pbap_server->state = PBAP_SERVER_STATE_SEND_FINAL_DATA;
                    }
                }
                goep_server_request_can_send_now(pbap_server->goep_cid);
            }
            break;
        default:
            break;
    }
}

static void pbap_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events
    pbap_server_t * pbap_server;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            pbap_server_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            pbap_server = pbap_server_for_goep_cid(channel);
            btstack_assert(pbap_server != NULL);
            pbap_server_packet_handler_goep(pbap_server, packet, size);
            break;
        default:
            break;
    }
}

static void pbap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
}

uint8_t pbap_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, gap_security_level_t security_level){
    uint8_t status = goep_server_register_service(&pbap_server_packet_handler, rfcomm_channel_nr, 0xffff, l2cap_psm, 0xffff, security_level);
    return status;
}

void pbap_server_deinit(void){
}

uint8_t pbap_connect(bd_addr_t addr, uint16_t * out_cid){
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_disconnect(uint16_t pbap_cid){
    return ERROR_CODE_SUCCESS;
}

