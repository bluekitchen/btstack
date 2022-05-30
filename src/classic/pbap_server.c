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

// max app params for vcard listing and phonebook response:
// - NewMissedCalls
// - DatabaseIdentifier
// - PrimaryFolderVersion,
// - SecondaryFolderVersion
#define PBAP_SERVER_MAX_APP_PARAMS_LEN ((4*2) + 2 + PBAP_DATABASE_IDENTIFIER_LEN + (2*PBAP_FOLDER_VERSION_LEN))

typedef enum {
    PBAP_SERVER_STATE_W4_OPEN,
    PBAP_SERVER_STATE_W4_CONNECT_OPCODE,
    PBAP_SERVER_STATE_W4_CONNECT_REQUEST,
    PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST,
    PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS,
    PBAP_SERVER_STATE_CONNECTED,
    PBAP_SERVER_STATE_W4_REQUEST,
    PBAP_SERVER_STATE_W4_USER_DATA,
    PBAP_SERVER_STATE_W4_GET_OPCODE,
    PBAP_SERVER_STATE_W4_GET_REQUEST,
    PBAP_SERVER_STATE_W4_SET_PATH_RESPONSE,
    PBAP_SERVER_STATE_SEND_RESPONSE,
    PBAP_SERVER_STATE_SEND_PREPARED_RESPONSE,
    PBAP_SERVER_STATE_SEND_DISCONNECT_RESPONSE,
    PBAP_SERVER_STATE_ABOUT_TO_SEND,
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

static  btstack_packet_handler_t pbap_server_user_packet_handler;

typedef struct {
    uint16_t pbap_cid;
    uint16_t goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    bool     incoming;
    pbap_server_state_t state;
    obex_parser_t obex_parser;
    uint16_t pbap_supported_features;
    // SRM
    obex_srm_t  obex_srm;
    srm_state_t srm_state;
    // request
    struct {
        char name[PBAP_SERVER_MAX_NAME_LEN];
        char type[PBAP_SERVER_MAX_TYPE_LEN];
        pbap_object_type_t object_type; // parsed from type string
        uint32_t continuation;
        obex_app_param_parser_t app_param_parser;
        uint8_t app_param_buffer[8];
        struct {
            char search_value[PBAP_SERVER_MAX_SEARCH_VALUE_LEN];    // has trailing zero
            uint32_t property_selector;
            uint32_t vcard_selector;
            pbap_format_vcard_t format;
            uint16_t max_list_count;
            uint16_t list_start_offset;
            uint8_t reset_new_missed_calls;
            uint8_t vcard_selector_operator;
            uint8_t order;
            uint8_t search_property;
        } app_params;
    } request;
    // response
    struct {
        uint8_t code;
        bool new_missed_calls_set;
        bool phonebook_size_set;
        bool database_identifier_set;
        bool primary_folder_version_set;
        bool secondary_folder_version_set;
        uint16_t new_missed_calls;
        uint16_t phonebook_size;
        uint8_t database_identifier[PBAP_DATABASE_IDENTIFIER_LEN];
        uint8_t primary_folder_version[PBAP_FOLDER_VERSION_LEN];
        uint8_t secondary_folder_version[PBAP_FOLDER_VERSION_LEN];
    } response;
} pbap_server_t;

static pbap_server_t pbap_server_singleton;

// 796135f0-f0c5-11d8-0966- 0800200c9a66
static const uint8_t pbap_uuid[] = { 0x79, 0x61, 0x35, 0xf0, 0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};

static void pbap_server_handle_get_request(pbap_server_t * pbap_server);

static pbap_server_t * pbap_server_for_goep_cid(uint16_t goep_cid){
    // TODO: check goep_cid after incoming connection -> accept/reject is implemented and state has been setup
    // return pbap_server_singleton.goep_cid == goep_cid ? &pbap_server_singleton : NULL;
    return &pbap_server_singleton;
}

static pbap_server_t * pbap_server_for_pbap_cid(uint16_t pbap_cid){
    return (pbap_cid == pbap_server_singleton.pbap_cid) ? &pbap_server_singleton : NULL;
}

static void pbap_server_finalize_connection(pbap_server_t * pbap_server){
    // minimal
    pbap_server->state = PBAP_SERVER_STATE_W4_OPEN;
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
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_repositories);

    // 0x0317 "PBAP Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PBAP_SUPPORTED_FEATURES);
    de_add_number(service, DE_UINT, DE_SIZE_16, pbap_supported_features);
}

static void pbap_server_emit_set_path_event(pbap_server_t *server, uint8_t flags, const char * name) {
    uint8_t event[5+PBAP_MAX_NAME_LEN];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;

    uint16_t name_len = strlen(name);
    if (name_len == 0){
        event[pos++] = 1;
        if ((flags & 1) == 1){
            event[pos++] = PBAP_SUBEVENT_SET_PHONEBOOK_UP;
        } else {
            event[pos++] = PBAP_SUBEVENT_SET_PHONEBOOK_ROOT;
        };
        little_endian_store_16(event, pos, server->pbap_cid);
        pos += 2;
    } else {
        event[pos++] = name_len + 1;
        event[pos++] = PBAP_SUBEVENT_SET_PHONEBOOK_DOWN;
        little_endian_store_16(event, pos, server->pbap_cid);
        pos += 2;
        memcpy(&event[pos], name, name_len+1);
        pos += name_len + 1;
    }
    (*pbap_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static pbap_object_type_t pbap_server_parse_object_type(const char * type_string){
    if (strcmp("x-bt/phonebook", type_string) == 0) {
        return PBAP_OBJECT_TYPE_PHONEBOOOK;
    }
    if (strcmp("x-bt/vcard-listing", type_string) == 0) {
        return PBAP_OBJECT_TYPE_VCARD_LISTING;
    }
    if (strcmp("x-bt/vcard", type_string) == 0) {
        return PBAP_OBJECT_TYPE_VCARD;
    }
    return PBAP_OBJECT_TYPE_INVALID;
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

static uint16_t pbap_server_application_params_add_uint16(uint8_t * application_parameters, uint8_t type, uint16_t value){
    uint16_t pos = 0;
    application_parameters[pos++] = type;
    application_parameters[pos++] = 2;
    big_endian_store_16(application_parameters, 2, value);
    pos += 2;
    return pos;
}

static uint16_t pbap_server_application_params_add_uint128(uint8_t * application_parameters, uint8_t type, const uint8_t * value){
    uint16_t pos = 0;
    application_parameters[pos++] = type;
    application_parameters[pos++] = 16;
    memcpy(&application_parameters[pos], value, 16);
    pos += 16;
    return pos;
}

static uint16_t pbap_server_application_params_add_phonebook_size(uint8_t * application_parameters, uint16_t phonebook_size){
    return pbap_server_application_params_add_uint16(application_parameters, PBAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE, phonebook_size);
}

static uint16_t pbap_server_application_params_add_new_missed_calls(uint8_t * application_parameters, uint16_t new_missed_calls){
    return pbap_server_application_params_add_uint16(application_parameters, PBAP_APPLICATION_PARAMETER_NEW_MISSED_CALLS, new_missed_calls);
}

static uint16_t pbap_server_application_params_add_primary_folder_version(uint8_t * application_parameters, const uint8_t * primary_folder_version){
    return pbap_server_application_params_add_uint128(application_parameters, PBAP_APPLICATION_PARAMETER_PRIMARY_VERSION_COUNTER, primary_folder_version);
}

static uint16_t pbap_server_application_params_add_secondary_folder_version(uint8_t * application_parameters, const uint8_t * secondary_folder_version){
    return pbap_server_application_params_add_uint128(application_parameters, PBAP_APPLICATION_PARAMETER_SECONDARY_VERSION_COUNTER, secondary_folder_version);
}

static uint16_t pbap_server_application_params_add_database_identifier(uint8_t * application_parameters, const uint8_t * database_identifier){
    return pbap_server_application_params_add_uint128(application_parameters, PBAP_APPLICATION_PARAMETER_DATABASE_IDENTIFIER, database_identifier);
}

static void pbap_server_add_application_parameters(const pbap_server_t * pbap_server, uint8_t * application_parameters, uint16_t len){
    if (len > 0){
        goep_server_header_add_application_parameters(pbap_server->goep_cid, application_parameters, len);
    }
}

static void pbap_server_default_headers(pbap_server_t * pbap_server){
    (void) memset(&pbap_server->request, 0, sizeof(pbap_server->request));
    pbap_server->request.app_params.max_list_count    = 0xffffU;
    pbap_server->request.app_params.vcard_selector    = 0xffffffffUL;
    pbap_server->request.app_params.property_selector = 0xffffffffUL;
}
static void pbap_server_reset_response(pbap_server_t * pbap_server){
    (void) memset(&pbap_server->response, 0, sizeof(pbap_server->response));
}

static void pbap_server_operation_complete(pbap_server_t * pbap_server){
    pbap_server->state = PBAP_SERVER_STATE_CONNECTED;
    pbap_server->srm_state = SRM_DISABLED;
    pbap_server_default_headers(pbap_server);
    pbap_server_reset_response(pbap_server);
}

static void pbap_server_handle_can_send_now(pbap_server_t * pbap_server){
    uint8_t app_params[PBAP_SERVER_MAX_APP_PARAMS_LEN];
    uint16_t app_params_pos = 0;
    uint8_t response_code;
    switch (pbap_server->state){
        case PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid);
            // next state
            pbap_server->state = PBAP_SERVER_STATE_W4_CONNECT_OPCODE;
            // send packet
            goep_server_execute(pbap_server->goep_cid, OBEX_RESP_BAD_REQUEST);
            break;
        case PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS:
            // prepare response
            goep_server_response_create_connect(pbap_server->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_server_header_add_who(pbap_server->goep_cid, pbap_uuid);
            // next state
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid, OBEX_RESP_SUCCESS);
            break;
        case PBAP_SERVER_STATE_SEND_RESPONSE:
            // prepare response
            goep_server_response_create_general(pbap_server->goep_cid);
            if (pbap_server->response.phonebook_size_set){
                app_params_pos = pbap_server_application_params_add_phonebook_size(app_params, pbap_server->response.phonebook_size);
            }
            pbap_server_add_application_parameters(pbap_server, app_params, app_params_pos);
            // next state
            response_code = pbap_server->response.code;
            pbap_server_operation_complete(pbap_server);
            // send packet
            goep_server_execute(pbap_server->goep_cid, response_code);
            break;
        case PBAP_SERVER_STATE_SEND_PREPARED_RESPONSE:
            // next state
            response_code = pbap_server->response.code;
            if (response_code == OBEX_RESP_CONTINUE){
                pbap_server_reset_response(pbap_server);
                // next state
                pbap_server->state = (pbap_server->srm_state == SRM_ENABLED) ?  PBAP_SERVER_STATE_ABOUT_TO_SEND : PBAP_SERVER_STATE_W4_GET_OPCODE;
            } else {
                pbap_server_operation_complete(pbap_server);
            }
            // send packet
            goep_server_execute(pbap_server->goep_cid, response_code);
            // trigger next user response in SRM
            if (pbap_server->srm_state == SRM_ENABLED){
                pbap_server_handle_get_request(pbap_server);
            }
            break;
        case PBAP_SERVER_STATE_SEND_DISCONNECT_RESPONSE:
        {
            // cache data
            uint16_t pbap_cid = pbap_server->pbap_cid;
            uint16_t goep_cid = pbap_server->goep_cid;

            // finalize connection
            pbap_server_finalize_connection(pbap_server);

            // respond to request
            goep_server_response_create_general(goep_cid);
            goep_server_execute(goep_cid, OBEX_RESP_SUCCESS);

            // emit event
            uint8_t event[2+3];
            uint16_t pos = 0;
            event[pos++] = HCI_EVENT_PBAP_META;
            event[pos++] = 0;
            event[pos++] = PBAP_SUBEVENT_CONNECTION_CLOSED;
            little_endian_store_16(event, pos, pbap_cid);
            pos += 2;
            (*pbap_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
            break;
        }
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
static void pbap_server_app_param_callback_connect(void * user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len){
    pbap_server_t * pbap_server = (pbap_server_t *) user_data;
    if (tag_id == PBAP_APPLICATION_PARAMETER_PBAP_SUPPORTED_FEATURES){
        obex_app_param_parser_tag_state_t state = obex_app_param_parser_tag_store(pbap_server->request.app_param_buffer, sizeof(pbap_server->request.app_param_buffer), total_len, data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE){
            pbap_server->pbap_supported_features = big_endian_read_32(pbap_server->request.app_param_buffer, 0);
        }
    }
}

static void pbap_server_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    UNUSED(total_len);
    UNUSED(data_offset);

    pbap_server_t * pbap_server = (pbap_server_t *) user_data;

    switch (header_id) {
        case OBEX_HEADER_TARGET:
            // TODO: verify target
            break;
        case OBEX_HEADER_APPLICATION_PARAMETERS:
            if (data_offset == 0){
                obex_app_param_parser_init(&pbap_server->request.app_param_parser,
                                           &pbap_server_app_param_callback_connect, total_len, pbap_server);
            }
            obex_app_param_parser_process_data(&pbap_server->request.app_param_parser, data_buffer, data_len);
            break;
        default:
            break;
    }
}

static void pbap_server_app_param_callback_get(void * user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len){
    pbap_server_t * pbap_server = (pbap_server_t *) user_data;
    obex_app_param_parser_tag_state_t state;
    switch (tag_id) {
        case PBAP_APPLICATION_PARAMETER_SEARCH_VALUE:
            state = obex_app_param_parser_tag_store((uint8_t *) pbap_server->request.app_params.search_value,
                                                    PBAP_SERVER_MAX_SEARCH_VALUE_LEN-1, total_len,
                                                    data_offset, data_buffer, data_len);
            // assert trailing zero
            pbap_server->request.app_params.search_value[PBAP_SERVER_MAX_SEARCH_VALUE_LEN - 1] = 0;
            break;
        default:
            state = obex_app_param_parser_tag_store(pbap_server->request.app_param_buffer,
                                                    sizeof(pbap_server->request.app_param_buffer), total_len,
                                                    data_offset, data_buffer, data_len);
            if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE) {
                switch (tag_id) {
                    case PBAP_APPLICATION_PARAMETER_PROPERTY_SELECTOR:
                        // read lower 32 bit
                        pbap_server->request.app_params.property_selector = big_endian_read_32(
                                pbap_server->request.app_param_buffer, 4);
                        break;
                    case PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR:
                        // read lower 32 bit
                        pbap_server->request.app_params.vcard_selector = big_endian_read_32(
                                pbap_server->request.app_param_buffer, 4);
                        break;
                    case PBAP_APPLICATION_PARAMETER_FORMAT:
                        pbap_server->request.app_params.format = (pbap_format_vcard_t) pbap_server->request.app_param_buffer[0];
                        break;
                    case PBAP_APPLICATION_PARAMETER_MAX_LIST_COUNT:
                        pbap_server->request.app_params.max_list_count = big_endian_read_16(
                                pbap_server->request.app_param_buffer, 0);
                        break;
                    case PBAP_APPLICATION_PARAMETER_LIST_START_OFFSET:
                        pbap_server->request.app_params.list_start_offset = big_endian_read_16(
                                pbap_server->request.app_param_buffer, 0);
                        break;
                    case PBAP_APPLICATION_PARAMETER_RESET_NEW_MISSED_CALLS:
                        pbap_server->request.app_params.reset_new_missed_calls = pbap_server->request.app_param_buffer[0];
                        break;
                    case PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR_OPERATOR:
                        pbap_server->request.app_params.vcard_selector_operator = pbap_server->request.app_param_buffer[0];
                        break;
                    case PBAP_APPLICATION_PARAMETER_ORDER:
                        pbap_server->request.app_params.vcard_selector_operator = pbap_server->request.app_param_buffer[0];
                        break;
                    case PBAP_APPLICATION_PARAMETER_SEARCH_PROPERTY:
                        pbap_server->request.app_params.search_property = pbap_server->request.app_param_buffer[0];
                        break;
                    default:
                        break;
                }
            }
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
            // TODO: verify connection id
            break;
        case OBEX_HEADER_NAME:
            // name is stored in big endian unicode-16
            if (total_len < (PBAP_SERVER_MAX_NAME_LEN * 2)){
                uint16_t i;
                for (i = 0; i < data_len ; i++){
                    if (((data_offset + i) & 1) == 1) {
                        pbap_server->request.name[(data_offset + i) >> 1] = data_buffer[i];
                    }
                }
                pbap_server->request.name[total_len / 2] = 0;
                printf("- Name: '%s'\n", pbap_server->request.name);
            }
            break;
        case OBEX_HEADER_TYPE:
            if (total_len < PBAP_SERVER_MAX_TYPE_LEN){
                memcpy(&pbap_server->request.type[data_offset], data_buffer, data_len);
                pbap_server->request.type[total_len] = 0;
                printf("- Type: '%s'\n", pbap_server->request.type);
            }
            break;
        case OBEX_HEADER_APPLICATION_PARAMETERS:
            if (data_offset == 0){
                obex_app_param_parser_init(&pbap_server->request.app_param_parser,
                                           &pbap_server_app_param_callback_get, total_len, pbap_server);
            }
            obex_app_param_parser_process_data(&pbap_server->request.app_param_parser, data_buffer, data_len);
            break;
        default:
            break;
    }
}

static void pbap_server_handle_get_request(pbap_server_t * pbap_server){
    pbap_server_handle_srm_headers(pbap_server);
    pbap_server->request.object_type = pbap_server_parse_object_type(pbap_server->request.type);
    if (pbap_server->request.object_type == PBAP_OBJECT_TYPE_INVALID){
        // unknown object type
        pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST;
        goep_server_request_can_send_now(pbap_server->goep_cid);
    } else {
        // ResetNewMissedCalls
        if (pbap_server->request.app_params.reset_new_missed_calls == 1){
            uint8_t event[5 + PBAP_MAX_NAME_LEN];
            uint16_t pos = 0;
            uint16_t name_len = strlen(pbap_server->request.name);
            event[pos++] = HCI_EVENT_PBAP_META;
            event[pos++] = 1 + 2 + name_len;
            event[pos++] = PBAP_SUBEVENT_RESET_MISSED_CALLS;
            little_endian_store_16(event, pos, pbap_server->pbap_cid);
            pos += 2;
            // name is zero terminated
            memcpy((char *) &event[pos], pbap_server->request.name, name_len + 1);
            pos += name_len + 1;
            (*pbap_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
        }
        // MaxListCount == 0 -> query size
        if (pbap_server->request.app_params.max_list_count == 0){
            // not valid for vCard request
            if (pbap_server->request.object_type == PBAP_OBJECT_TYPE_VCARD){
                pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST;
                goep_server_request_can_send_now(pbap_server->goep_cid);
            } else {
                pbap_server->state = PBAP_SERVER_STATE_W4_USER_DATA;
                uint8_t event[10 + PBAP_MAX_NAME_LEN];
                uint16_t pos = 0;
                uint16_t name_len = strlen(pbap_server->request.name);
                event[pos++] = HCI_EVENT_PBAP_META;
                event[pos++] = 1 + 6 + name_len;
                event[pos++] = PBAP_SUBEVENT_QUERY_PHONEBOOK_SIZE;
                little_endian_store_16(event, pos, pbap_server->pbap_cid);
                pos += 2;
                little_endian_store_32(event, pos, pbap_server->request.app_params.vcard_selector);
                pos += 4;
                event[pos++] = pbap_server->request.app_params.vcard_selector_operator;
                // name is zero terminated
                memcpy((char *) &event[pos], pbap_server->request.name, name_len + 1);
                pos += name_len + 1;
                (*pbap_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
            }
        } else {
            // emit pull ( phonebook, vcard_listing, vcard)
            uint8_t event[2 + 20 + PBAP_SERVER_MAX_NAME_LEN + PBAP_SERVER_MAX_SEARCH_VALUE_LEN];
            uint16_t pos = 0;
            uint16_t name_len = (uint16_t) strlen(pbap_server->request.name);
            uint16_t search_value_len;
            event[pos++] = HCI_EVENT_PBAP_META;
            switch (pbap_server->request.object_type){
                case PBAP_OBJECT_TYPE_PHONEBOOOK:
                    event[pos++] = 20 + name_len + 1;
                    event[pos++] = PBAP_SUBEVENT_PULL_PHONEBOOK;
                    little_endian_store_16(event, pos, pbap_server->pbap_cid);
                    pos += 2;
                    little_endian_store_32(event, pos, pbap_server->request.continuation);
                    pos += 4;
                    little_endian_store_32(event, pos, pbap_server->request.app_params.property_selector);
                    pos += 4;
                    event[pos++] = pbap_server->request.app_params.format;
                    little_endian_store_16(event, pos, pbap_server->request.app_params.max_list_count);
                    pos += 2;
                    little_endian_store_16(event, pos, pbap_server->request.app_params.list_start_offset);
                    pos += 2;
                    little_endian_store_32(event, pos, pbap_server->request.app_params.vcard_selector);
                    pos += 4;
                    event[pos++] = pbap_server->request.app_params.vcard_selector_operator;
                    // name is zero terminated
                    memcpy(&event[pos], pbap_server->request.name, name_len + 1);
                    pos += name_len + 1;
                    break;
                case PBAP_OBJECT_TYPE_VCARD_LISTING:
                    search_value_len = (uint16_t) strlen(pbap_server->request.app_params.search_value);
                    event[pos++] = 20 + name_len + 1 + search_value_len + 1;
                    event[pos++] = PBAP_SUBEVENT_PULL_VCARD_LISTING;
                    little_endian_store_16(event, pos, pbap_server->pbap_cid);
                    pos += 2;
                    little_endian_store_32(event, pos, pbap_server->request.continuation);
                    pos += 4;
                    event[pos++] = pbap_server->request.app_params.order;
                    little_endian_store_16(event, pos, pbap_server->request.app_params.max_list_count);
                    pos += 2;
                    little_endian_store_16(event, pos, pbap_server->request.app_params.list_start_offset);
                    pos += 2;
                    little_endian_store_32(event, pos, pbap_server->request.app_params.vcard_selector);
                    pos += 4;
                    event[pos++] = pbap_server->request.app_params.vcard_selector_operator;
                    event[pos++] = pbap_server->request.app_params.search_property;
                    // search_value is zero terminated
                    event[pos++] = search_value_len + 1;
                    memcpy(&event[pos], (const uint8_t *) pbap_server->request.app_params.search_value, search_value_len + 1);
                    pos += search_value_len + 1;
                    // name is zero terminated
                    memcpy(&event[pos], pbap_server->request.name, name_len + 1);
                    pos += name_len + 1;
                    break;
                case PBAP_OBJECT_TYPE_VCARD:
                    event[pos++] = 8 + name_len + 1;
                    event[pos++] = PBAP_SUBEVENT_PULL_VCARD_ENTRY;
                    little_endian_store_16(event, pos, pbap_server->pbap_cid);
                    pos += 2;
                    little_endian_store_32(event, pos, pbap_server->request.app_params.property_selector);
                    pos += 4;
                    event[pos++] = pbap_server->request.app_params.format;
                    // name is zero terminated
                    memcpy(&event[pos], pbap_server->request.name, name_len + 1);
                    pos += name_len + 1;
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
            pbap_server->state = PBAP_SERVER_STATE_W4_USER_DATA;
            (*pbap_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
        }
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
            pbap_server_default_headers(pbap_server);
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
                    pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST;
                } else {
                    // send connect response
                    pbap_server->state = PBAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS;
                }
                goep_server_request_can_send_now(pbap_server->goep_cid);
            }
            break;
        case PBAP_SERVER_STATE_CONNECTED:
            opcode = packet[0];
            // default headers
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
                        pbap_server_handle_get_request(pbap_server);
                        break;
                    case OBEX_OPCODE_SETPATH:
                        pbap_server->state = PBAP_SERVER_STATE_W4_SET_PATH_RESPONSE;
                        pbap_server_emit_set_path_event(pbap_server, op_info.flags, &pbap_server->request.name[0]);
                        break;
                    case OBEX_OPCODE_DISCONNECT:
                        pbap_server->state = PBAP_SERVER_STATE_SEND_DISCONNECT_RESPONSE;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                    case OBEX_OPCODE_ACTION:
                    case (OBEX_OPCODE_ACTION | OBEX_OPCODE_FINAL_BIT_MASK):
                    case OBEX_OPCODE_SESSION:
                    default:
                        // send bad request response
                        pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                }
            }
            break;

        case PBAP_SERVER_STATE_W4_GET_OPCODE:
            pbap_server->state = PBAP_SERVER_STATE_W4_GET_REQUEST;
            obex_parser_init_for_request(&pbap_server->obex_parser, &pbap_server_parser_callback_get, (void*) pbap_server);

            /* fall through */

        case PBAP_SERVER_STATE_W4_GET_REQUEST:
            obex_srm_init(&pbap_server->obex_srm);
            parser_state = obex_parser_process_data(&pbap_server->obex_parser, packet, size);
            if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
                obex_parser_operation_info_t op_info;
                obex_parser_get_operation_info(&pbap_server->obex_parser, &op_info);
                switch((op_info.opcode & 0x7f)){
                    case OBEX_OPCODE_GET:
                        pbap_server_handle_get_request(pbap_server);
                        break;
                    case (OBEX_OPCODE_ABORT & 0x7f):
                        pbap_server->response.code = OBEX_RESP_SUCCESS;
                        pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                    default:
                        // send bad request response
                        pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE_BAD_REQUEST;
                        goep_server_request_can_send_now(pbap_server->goep_cid);
                        break;
                }
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

uint8_t pbap_server_send_set_phonebook_result(uint16_t pbap_cid, uint8_t response_code){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (pbap_server->state != PBAP_SERVER_STATE_W4_SET_PATH_RESPONSE) {
        ERROR_CODE_COMMAND_DISALLOWED;
    }
    pbap_server->response.code = response_code;
    pbap_server->state = PBAP_SERVER_STATE_SEND_RESPONSE;
    return goep_server_request_can_send_now(pbap_server->goep_cid);
}

uint8_t pbap_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, gap_security_level_t security_level){
    uint8_t status = goep_server_register_service(&pbap_server_packet_handler, rfcomm_channel_nr, 0xffff, l2cap_psm, 0xffff, security_level);
    pbap_server_user_packet_handler = packet_handler;
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

// note: common code for next four setters
static bool pbap_server_valid_header_for_request(pbap_server_t * pbap_server){
    if (pbap_server->state != PBAP_SERVER_STATE_W4_USER_DATA) {
        return false;
    }
    switch (pbap_server->request.object_type){
        case PBAP_OBJECT_TYPE_PHONEBOOOK:
        case PBAP_OBJECT_TYPE_VCARD_LISTING:
            return true;
        default:
            return false;
    }

}
uint8_t pbap_server_set_new_missed_calls(uint16_t pbap_cid, uint16_t new_missed_calls){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (pbap_server_valid_header_for_request(pbap_server)){
        pbap_server->response.new_missed_calls = new_missed_calls;
        pbap_server->response.new_missed_calls_set = true;
        return ERROR_CODE_SUCCESS;
    } else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t pbap_server_set_primary_folder_version(uint16_t pbap_cid, const uint8_t * primary_folder_version){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (pbap_server_valid_header_for_request(pbap_server)){
        (void) memcpy(pbap_server->response.primary_folder_version, primary_folder_version, PBAP_FOLDER_VERSION_LEN);
        pbap_server->response.primary_folder_version_set = true;
        return ERROR_CODE_SUCCESS;
    } else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t pbap_server_set_secondary_folder_version(uint16_t pbap_cid, const uint8_t * secondary_folder_version){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (pbap_server_valid_header_for_request(pbap_server)){
        (void) memcpy(pbap_server->response.secondary_folder_version, secondary_folder_version, PBAP_FOLDER_VERSION_LEN);
        pbap_server->response.secondary_folder_version_set = true;
        return ERROR_CODE_SUCCESS;
    } else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t pbap_server_set_database_identifier(uint16_t pbap_cid, const uint8_t * database_identifier){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (pbap_server->state != PBAP_SERVER_STATE_W4_USER_DATA) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    btstack_assert(pbap_server->request.object_type != PBAP_OBJECT_TYPE_INVALID);

    (void) memcpy(pbap_server->response.database_identifier, database_identifier, PBAP_FOLDER_VERSION_LEN);
    pbap_server->response.database_identifier_set = true;
    return ERROR_CODE_SUCCESS;
};

void pbap_server_build_response(pbap_server_t * pbap_server){
    if (pbap_server->response.code == 0){
        // set interim response code
        pbap_server->response.code = OBEX_RESP_SUCCESS;
        goep_server_response_create_general(pbap_server->goep_cid);
        pbap_server_add_srm_headers(pbap_server);
        // Application Params
        uint8_t app_params[PBAP_SERVER_MAX_APP_PARAMS_LEN];
        uint16_t app_params_pos = 0;
        if (pbap_server->response.phonebook_size_set){
            pbap_server->response.phonebook_size_set = false;
            app_params_pos += pbap_server_application_params_add_phonebook_size(&app_params[app_params_pos], pbap_server->response.phonebook_size);
        }
        if (pbap_server->response.new_missed_calls_set){
            pbap_server->response.new_missed_calls_set = false;
            app_params_pos += pbap_server_application_params_add_new_missed_calls(&app_params[app_params_pos], pbap_server->response.new_missed_calls);
        }
        if (pbap_server->response.primary_folder_version_set){
            pbap_server->response.primary_folder_version_set = false;
            app_params_pos += pbap_server_application_params_add_primary_folder_version(&app_params[app_params_pos], pbap_server->response.primary_folder_version);
        }
        if (pbap_server->response.secondary_folder_version_set){
            pbap_server->response.secondary_folder_version_set = false;
            app_params_pos += pbap_server_application_params_add_secondary_folder_version(&app_params[app_params_pos], pbap_server->response.secondary_folder_version);
        }
        if (pbap_server->response.database_identifier_set){
            pbap_server->response.database_identifier_set = false;
            app_params_pos += pbap_server_application_params_add_database_identifier(&app_params[app_params_pos], pbap_server->response.database_identifier);
        }
        pbap_server_add_application_parameters(pbap_server, app_params, app_params_pos);
    }
}

uint16_t pbap_server_get_max_body_size(uint16_t pbap_cid){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return 0;
    }
    if (pbap_server->state != PBAP_SERVER_STATE_W4_USER_DATA) {
        return 0;
    }
    btstack_assert(pbap_server->request.object_type != PBAP_OBJECT_TYPE_INVALID);

    pbap_server_build_response(pbap_server);
    return goep_server_response_get_max_body_size(pbap_server->goep_cid);
}

uint8_t pbap_server_send_phonebook_size(uint16_t pbap_cid, uint8_t response_code, uint16_t phonebook_size){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // phonebook size valid for PHONEBOOK and VCARD_LIST query
    if (pbap_server_valid_header_for_request(pbap_server)){
        pbap_server->response.phonebook_size = phonebook_size;
        pbap_server->response.phonebook_size_set = true;
        pbap_server_build_response(pbap_server);
        pbap_server->response.code = response_code;
        pbap_server->state = PBAP_SERVER_STATE_SEND_PREPARED_RESPONSE;
        return goep_server_request_can_send_now(pbap_server->goep_cid);
    } else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint16_t pbap_server_send_pull_response(uint16_t pbap_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t * body){
    pbap_server_t * pbap_server = pbap_server_for_pbap_cid(pbap_cid);
    if (pbap_server == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // double check body size
    pbap_server_build_response(pbap_server);
    if (body_len > goep_server_response_get_max_body_size(pbap_server->goep_cid)){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }
    // append body and execute
    goep_server_header_add_end_of_body(pbap_server->goep_cid, body, body_len);
    // set final response code
    pbap_server->response.code = response_code;
    pbap_server->request.continuation = continuation;
    pbap_server->state = PBAP_SERVER_STATE_SEND_PREPARED_RESPONSE;
    return goep_server_request_can_send_now(pbap_server->goep_cid);
}
