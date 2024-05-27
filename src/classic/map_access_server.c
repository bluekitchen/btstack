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

#define BTSTACK_FILE__ "map_access_server.c"

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#include "classic/map.h"
#include "classic/map_access_server.h"

 // max app params for vcard listing and phonebook response:
 // - NewMissedCalls
 // - DatabaseIdentifier
 // - PrimaryFolderVersion,
 // - SecondaryFolderVersion
#define MAP_SERVER_MAX_APP_PARAMS_LEN ((4*2) + 2 + MAP_DATABASE_IDENTIFIER_LEN + (2*MAP_FOLDER_VERSION_LEN))

typedef enum {
    MAP_SERVER_DIR_ROOT,
    MAP_SERVER_DIR_TELECOM,
    MAP_SERVER_DIR_TELECOM_PHONEBOOK,
    MAP_SERVER_DIR_SIM,
    MAP_SERVER_DIR_SIM_TELECOM,
    MAP_SERVER_DIR_SIM_TELECOM_PHONEBOOK
} map_access_server_dir_t;

typedef enum {
    MAP_SERVER_STATE_W4_OPEN,
    MAP_SERVER_STATE_W4_CONNECT_OPCODE,
    MAP_SERVER_STATE_W4_CONNECT_REQUEST,
    MAP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR,
    MAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS,
    MAP_SERVER_STATE_CONNECTED,
    MAP_SERVER_STATE_W4_REQUEST,
    MAP_SERVER_STATE_W4_USER_DATA,
    MAP_SERVER_STATE_W4_GET_OPCODE,
    MAP_SERVER_STATE_W4_GET_REQUEST,
    MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE,
    MAP_SERVER_STATE_SEND_USER_RESPONSE,
    MAP_SERVER_STATE_SEND_PHONEBOOK_SIZE,
    MAP_SERVER_STATE_SEND_DISCONNECT_RESPONSE,
    MAP_SERVER_STATE_ABOUT_TO_SEND,
} map_access_server_state_t;

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

static  btstack_packet_handler_t map_access_server_user_packet_handler;

typedef struct {
    uint16_t map_cid;
    uint16_t goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    bool     incoming;
    map_access_server_state_t state;
    obex_parser_t obex_parser;
    uint16_t map_supported_features;
    map_access_server_dir_t map_access_server_dir;
    map_phonebook_t  map_phonebook;
    // SRM
    obex_srm_t  obex_srm;
    srm_state_t srm_state;
    // request
    struct {
        char name[MAP_SERVER_MAX_NAME_LEN];
        char type[MAP_SERVER_MAX_TYPE_LEN];
        map_object_type_t object_type; // parsed from type string
        uint32_t continuation;
        obex_app_param_parser_t app_param_parser;
        uint8_t app_param_buffer[8];
        struct {
            char search_value[MAP_SERVER_MAX_SEARCH_VALUE_LEN];    // has trailing zero
            uint32_t property_selector;
            uint32_t vcard_selector;
            map_format_vcard_t format;
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
        uint8_t database_identifier[MAP_DATABASE_IDENTIFIER_LEN];
        uint8_t primary_folder_version[MAP_FOLDER_VERSION_LEN];
        uint8_t secondary_folder_version[MAP_FOLDER_VERSION_LEN];
        uint16_t body_len;
        const uint8_t* body_data;
    } response;
} map_access_server_t;

static map_access_server_t map_access_server_singleton;

static struct {
    char* name;
    map_access_server_dir_t parent_dir;
    char* path;
} map_access_server_phonebooks[] = {
    {"cch",MAP_SERVER_DIR_TELECOM, "telecom/cch.vcf"},
    {"fav",MAP_SERVER_DIR_TELECOM, "telecom/fav.vcf"},
    {"ich",MAP_SERVER_DIR_TELECOM, "telecom/ich.vcf"},
    {"mch",MAP_SERVER_DIR_TELECOM, "telecom/mch.vcf"},
    {"och",MAP_SERVER_DIR_TELECOM, "telecom/och.vcf"},
    {"pb", MAP_SERVER_DIR_TELECOM, "telecom/pb.vcf"},
    {"spd",MAP_SERVER_DIR_TELECOM, "telecom/spd.vcf"},
    {"cch",MAP_SERVER_DIR_SIM_TELECOM, "SIM1/telecom/cch.vcf"},
    {"ich",MAP_SERVER_DIR_SIM_TELECOM, "SIM1/telecom/ich.vcf"},
    {"mch",MAP_SERVER_DIR_SIM_TELECOM, "SIM1/telecom/mch.vcf"},
    {"och",MAP_SERVER_DIR_SIM_TELECOM, "SIM1/telecom/och.vcf"},
    {"pb", MAP_SERVER_DIR_SIM_TELECOM, "SIM1/telecom/pb.vcf"}
};

static const char map_access_server_default_service_name[] = "MAP";

static const uint8_t map_uuid[] = { 0xbb, 0x58, 0x2b, 0x40, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66 };

// Prototypes
static void map_access_server_handle_get_request(map_access_server_t* map_access_server);
static void map_access_server_build_response(map_access_server_t* map_access_server);

static map_access_server_t* map_access_server_for_goep_cid(uint16_t goep_cid) {
    // TODO: check goep_cid after incoming connection -> accept/reject is implemented and state has been setup
    // return map_access_server_singleton.goep_cid == goep_cid ? &map_access_server_singleton : NULL;
    return &map_access_server_singleton;
}

static map_access_server_t* map_access_server_for_map_cid(uint16_t map_cid) {
    return (map_cid == map_access_server_singleton.map_cid) ? &map_access_server_singleton : NULL;
}

static void map_access_server_finalize_connection(map_access_server_t* map_access_server) {
    // minimal
    map_access_server->state = MAP_SERVER_STATE_W4_OPEN;
}

void map_access_server_create_sdp_record(uint8_t* service, uint32_t service_record_handle, uint8_t rfcomm_channel_nr,
    uint16_t l2cap_psm, const char* name, uint8_t supported_repositories,
    uint32_t map_supported_features) {
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);

        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm, DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);
            de_add_number(rfcomm, DE_UINT, DE_SIZE_8, rfcomm_channel_nr);
        }
        de_pop_sequence(attribute, rfcomm);

        uint8_t* obexProtocol = de_push_sequence(attribute);
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
        uint8_t* mapServerProfile = de_push_sequence(attribute);
        {
            de_add_number(mapServerProfile, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS);
            de_add_number(mapServerProfile, DE_UINT, DE_SIZE_16, 0x0102); // Verision 1.2
        }
        de_pop_sequence(attribute, mapServerProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    if (name == NULL) {
        name = map_access_server_default_service_name;
    }
    if (strlen(name) > 0) {
        de_add_number(service, DE_UINT, DE_SIZE_16, 0x0100);
        de_add_data(service, DE_STRING, (uint16_t)strlen(name), (uint8_t*)name);
    }

#ifdef ENABLE_GOEP_L2CAP
    // 0x0200 "GOEP L2CAP PSM"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_GOEP_L2CAP_PSM);
    de_add_number(service, DE_UINT, DE_SIZE_16, l2cap_psm);
#endif

    // 0x0314 "Supported Repositories"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SUPPORTED_REPOSITORIES);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_repositories);

    // 0x0317 "MAP Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_MAP_SUPPORTED_FEATURES);
    de_add_number(service, DE_UINT, DE_SIZE_16, map_supported_features);
}

static map_phonebook_t map_access_server_get_phonebook_by_path(const char* path) {
    uint16_t index;
    for (index = 0; index < (sizeof(map_access_server_phonebooks) / sizeof(map_access_server_phonebooks[0])); index++) {
        if (strcmp(path, map_access_server_phonebooks[index].path) == 0) {
            return MAP_PHONEBOOK_TELECOM_CCH + index;
        }
    }
    return MAP_PHONEBOOK_INVALID;
}

static map_phonebook_t map_access_server_get_phonebook_by_dir_and_name(map_access_server_dir_t parent_dir, const char* name) {
    uint16_t index;
    for (index = 0; index < (sizeof(map_access_server_phonebooks) / sizeof(map_access_server_phonebooks[0])); index++) {
        if ((parent_dir == map_access_server_phonebooks[index].parent_dir) && (strcmp(name, map_access_server_phonebooks[index].name) == 0)) {
            return MAP_PHONEBOOK_TELECOM_CCH + index;
        }
    }
    return MAP_PHONEBOOK_INVALID;
}

static void map_access_server_handle_set_path_request(map_access_server_t* map_access_server, uint8_t flags, const char* name) {
    uint16_t name_len = (uint16_t)strlen(name);
    uint8_t obex_result = OBEX_RESP_SUCCESS;
    if (name_len == 0) {
        if ((flags & 1) == 1) {
            switch (map_access_server->map_access_server_dir) {
            case MAP_SERVER_DIR_TELECOM:
            case MAP_SERVER_DIR_SIM:
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_ROOT;
                break;
            case MAP_SERVER_DIR_SIM_TELECOM:
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_SIM;
                break;
            case MAP_SERVER_DIR_TELECOM_PHONEBOOK:
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_TELECOM;
                break;
            case MAP_SERVER_DIR_SIM_TELECOM_PHONEBOOK:
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_SIM_TELECOM;
                break;
            default:
                obex_result = OBEX_RESP_NOT_FOUND;
                break;
            }
        }
        else {
            map_access_server->map_access_server_dir = MAP_SERVER_DIR_ROOT;
        };
    }
    else {
        switch (map_access_server->map_access_server_dir) {
        case MAP_SERVER_DIR_ROOT:
            if (strcmp("telecom", name) == 0) {
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_TELECOM;
            }
            else if (strcmp("SIM1", name) == 0) {
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_SIM;
            }
            else {
                obex_result = OBEX_RESP_NOT_FOUND;
            }
            break;
        case MAP_SERVER_DIR_SIM:
            if (strcmp("telecom", name) == 0) {
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_SIM_TELECOM;
            }
            else {
                obex_result = OBEX_RESP_NOT_FOUND;
            }
            break;
        case MAP_SERVER_DIR_TELECOM_PHONEBOOK:
            map_access_server->map_phonebook = map_access_server_get_phonebook_by_dir_and_name(map_access_server->map_access_server_dir, name);
            if (map_access_server->map_phonebook < 0) {
                obex_result = OBEX_RESP_NOT_FOUND;
            }
            else {
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_TELECOM_PHONEBOOK;
            }
            break;
        case MAP_SERVER_DIR_SIM_TELECOM_PHONEBOOK:
            map_access_server->map_phonebook = map_access_server_get_phonebook_by_dir_and_name(map_access_server->map_access_server_dir, name);
            if (map_access_server->map_phonebook < 0) {
                obex_result = OBEX_RESP_NOT_FOUND;
            }
            else {
                map_access_server->map_access_server_dir = MAP_SERVER_DIR_SIM_TELECOM_PHONEBOOK;
            }
            break;
        default:
            obex_result = OBEX_RESP_NOT_FOUND;
            break;
        }
    }
    map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
    map_access_server->response.code = obex_result;
    goep_server_request_can_send_now(map_access_server->goep_cid);
}

static map_object_type_t map_access_server_parse_object_type(const char* type_string) {
    if (strcmp("x-bt/MAP-msg-listing", type_string) == 0) {
        return MAP_OBJECT_TYPE_MSG_LISTING;
    }

    return MAP_OBJECT_TYPE_INVALID;
}

static void obex_srm_init(obex_srm_t* obex_srm) {
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

static void map_access_server_handle_srm_headers(map_access_server_t* map_access_server) {
    const obex_srm_t* obex_srm = &map_access_server->obex_srm;
    // Update SRM state based on SRM headers
    switch (map_access_server->srm_state) {
    case SRM_DISABLED:
        if (obex_srm->srm_value == OBEX_SRM_ENABLE) {
            if (obex_srm->srmp_value == OBEX_SRMP_WAIT) {
                map_access_server->srm_state = SRM_SEND_CONFIRM_WAIT;
            }
            else {
                map_access_server->srm_state = SRM_SEND_CONFIRM;
            }
        }
        break;
    case SRM_ENABLED_WAIT:
        if (obex_srm->srmp_value == OBEX_SRMP_NEXT) {
            map_access_server->srm_state = SRM_ENABLED;
        }
        break;
    default:
        break;
    }
}

static void map_access_server_add_srm_headers(map_access_server_t* map_access_server) {
    switch (map_access_server->srm_state) {
    case SRM_SEND_CONFIRM:
        goep_server_header_add_srm_enable(map_access_server->goep_cid);
        map_access_server->srm_state = SRM_ENABLED;
        break;
    case SRM_SEND_CONFIRM_WAIT:
        goep_server_header_add_srm_enable(map_access_server->goep_cid);
        map_access_server->srm_state = SRM_ENABLED_WAIT;
        break;
    default:
        break;
    }
}

static uint16_t map_access_server_application_params_add_uint16(uint8_t* application_parameters, uint8_t type, uint16_t value) {
    uint16_t pos = 0;
    application_parameters[pos++] = type;
    application_parameters[pos++] = 2;
    big_endian_store_16(application_parameters, 2, value);
    pos += 2;
    return pos;
}

static uint16_t map_access_server_application_params_add_uint128(uint8_t* application_parameters, uint8_t type, const uint8_t* value) {
    uint16_t pos = 0;
    application_parameters[pos++] = type;
    application_parameters[pos++] = 16;
    memcpy(&application_parameters[pos], value, 16);
    pos += 16;
    return pos;
}

//static uint16_t map_access_server_application_params_add_phonebook_size(uint8_t* application_parameters, uint16_t phonebook_size) {
//    return map_access_server_application_params_add_uint16(application_parameters, MAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE, phonebook_size);
//}
//
//static uint16_t map_access_server_application_params_add_new_missed_calls(uint8_t* application_parameters, uint16_t new_missed_calls) {
//    return map_access_server_application_params_add_uint16(application_parameters, MAP_APPLICATION_PARAMETER_NEW_MISSED_CALLS, new_missed_calls);
//}
//
//static uint16_t map_access_server_application_params_add_primary_folder_version(uint8_t* application_parameters, const uint8_t* primary_folder_version) {
//    return map_access_server_application_params_add_uint128(application_parameters, MAP_APPLICATION_PARAMETER_PRIMARY_VERSION_COUNTER, primary_folder_version);
//}
//
//static uint16_t map_access_server_application_params_add_secondary_folder_version(uint8_t* application_parameters, const uint8_t* secondary_folder_version) {
//    return map_access_server_application_params_add_uint128(application_parameters, MAP_APPLICATION_PARAMETER_SECONDARY_VERSION_COUNTER, secondary_folder_version);
//}
//
//static uint16_t map_access_server_application_params_add_database_identifier(uint8_t* application_parameters, const uint8_t* database_identifier) {
//    return map_access_server_application_params_add_uint128(application_parameters, MAP_APPLICATION_PARAMETER_DATABASE_IDENTIFIER, database_identifier);
//}

static void map_access_server_add_application_parameters(const map_access_server_t* map_access_server, uint8_t* application_parameters, uint16_t len) {
    if (len > 0) {
        goep_server_header_add_application_parameters(map_access_server->goep_cid, application_parameters, len);
    }
}

static void map_access_server_default_headers(map_access_server_t* map_access_server) {
    (void)memset(&map_access_server->request, 0, sizeof(map_access_server->request));
    map_access_server->request.app_params.max_list_count = 0xffffU;
    map_access_server->request.app_params.vcard_selector = 0xffffffffUL;
    map_access_server->request.app_params.property_selector = 0xffffffffUL;
}
static void map_access_server_reset_response(map_access_server_t* map_access_server) {
    (void)memset(&map_access_server->response, 0, sizeof(map_access_server->response));
}

static void map_access_server_operation_complete(map_access_server_t* map_access_server) {
    map_access_server->state = MAP_SERVER_STATE_CONNECTED;
    map_access_server->srm_state = SRM_DISABLED;
    map_access_server_default_headers(map_access_server);
    map_access_server_reset_response(map_access_server);
}

static void map_access_server_handle_can_send_now(map_access_server_t* map_access_server) {
    uint8_t response_code;
    switch (map_access_server->state) {
    case MAP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR:
        // prepare response
        goep_server_response_create_general(map_access_server->goep_cid);
        // next state
        map_access_server->state = MAP_SERVER_STATE_W4_CONNECT_OPCODE;
        // send packet
        goep_server_execute(map_access_server->goep_cid, OBEX_RESP_BAD_REQUEST);
        break;
    case MAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS:
        // prepare response
        goep_server_response_create_connect(map_access_server->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
        goep_server_header_add_who(map_access_server->goep_cid, map_uuid);
        // next state
        map_access_server->map_access_server_dir = MAP_SERVER_DIR_ROOT;
        map_access_server->map_phonebook = MAP_PHONEBOOK_INVALID;
        map_access_server_operation_complete(map_access_server);
        // send packet
        goep_server_execute(map_access_server->goep_cid, OBEX_RESP_SUCCESS);
        break;
    case MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE:
        // prepare response
        goep_server_response_create_general(map_access_server->goep_cid);
        // next state
        response_code = map_access_server->response.code;
        map_access_server_operation_complete(map_access_server);
        // send packet
        goep_server_execute(map_access_server->goep_cid, response_code);
        break;
    case MAP_SERVER_STATE_SEND_USER_RESPONSE:
        // prepare response
        map_access_server_build_response(map_access_server);
        if (map_access_server->response.body_len > 0) {
            goep_server_header_add_end_of_body(map_access_server->goep_cid,
                map_access_server->response.body_data,
                map_access_server->response.body_len);
        }
        // next state
        response_code = map_access_server->response.code;
        if (response_code == OBEX_RESP_CONTINUE) {
            map_access_server_reset_response(map_access_server);
            // next state
            map_access_server->state = (map_access_server->srm_state == SRM_ENABLED) ? MAP_SERVER_STATE_ABOUT_TO_SEND : MAP_SERVER_STATE_W4_GET_OPCODE;
        }
        else {
            map_access_server_operation_complete(map_access_server);
        }
        // send packet
        goep_server_execute(map_access_server->goep_cid, response_code);
        // trigger next user response in SRM
        if (map_access_server->srm_state == SRM_ENABLED) {
            map_access_server_handle_get_request(map_access_server);
        }
        break;
    case MAP_SERVER_STATE_SEND_DISCONNECT_RESPONSE:
    {
        // cache data
        uint16_t map_cid = map_access_server->map_cid;
        uint16_t goep_cid = map_access_server->goep_cid;

        // finalize connection
        map_access_server_finalize_connection(map_access_server);

        // respond to request
        goep_server_response_create_general(goep_cid);
        goep_server_execute(goep_cid, OBEX_RESP_SUCCESS);

        // emit event
        uint8_t event[2 + 3];
        uint16_t pos = 0;
        event[pos++] = HCI_EVENT_MAP_META;
        event[pos++] = 0;
        event[pos++] = MAP_SUBEVENT_CONNECTION_CLOSED;
        little_endian_store_16(event, pos, map_cid);
        pos += 2;
        (*map_access_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
        break;
    }
    default:
        break;
    }
}

static void map_access_server_packet_handler_hci(uint8_t* packet, uint16_t size) {
    UNUSED(size);
    uint8_t status;
    map_access_server_t* map_access_server;
    uint16_t goep_cid;
    switch (hci_event_packet_get_type(packet)) {
    case HCI_EVENT_GOEP_META:
        switch (hci_event_goep_meta_get_subevent_code(packet)) {
        case GOEP_SUBEVENT_INCOMING_CONNECTION:
            // TODO: check if resources available
            goep_server_accept_connection(goep_subevent_incoming_connection_get_goep_cid(packet));
            break;
        case GOEP_SUBEVENT_CONNECTION_OPENED:
            goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
            map_access_server = map_access_server_for_goep_cid(goep_cid);
            btstack_assert(map_access_server != NULL);
            status = goep_subevent_connection_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                // TODO: report failed outgoing connection
            }
            else {
                map_access_server->goep_cid = goep_cid;
                goep_subevent_connection_opened_get_bd_addr(packet, map_access_server->bd_addr);
                map_access_server->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                map_access_server->incoming = goep_subevent_connection_opened_get_incoming(packet);
                map_access_server->state = MAP_SERVER_STATE_W4_CONNECT_OPCODE;
            }
            break;
        case GOEP_SUBEVENT_CONNECTION_CLOSED:
            goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
            map_access_server = map_access_server_for_goep_cid(goep_cid);
            btstack_assert(map_access_server != NULL);
            break;
        case GOEP_SUBEVENT_CAN_SEND_NOW:
            goep_cid = goep_subevent_can_send_now_get_goep_cid(packet);
            map_access_server = map_access_server_for_goep_cid(goep_cid);
            btstack_assert(map_access_server != NULL);
            map_access_server_handle_can_send_now(map_access_server);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}
static void map_access_server_app_param_callback_connect(void* user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t* data_buffer, uint8_t data_len) {
    map_access_server_t* map_access_server = (map_access_server_t*)user_data;
    if (tag_id == MAP_APPLICATION_PARAMETER_MAP_SUPPORTED_FEATURES) {
        obex_app_param_parser_tag_state_t state = obex_app_param_parser_tag_store(map_access_server->request.app_param_buffer, sizeof(map_access_server->request.app_param_buffer), total_len, data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE) {
            map_access_server->map_supported_features = big_endian_read_32(map_access_server->request.app_param_buffer, 0);
        }
    }
}

static void map_access_server_parser_callback_connect(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    UNUSED(total_len);
    UNUSED(data_offset);

    map_access_server_t* map_access_server = (map_access_server_t*)user_data;

    switch (header_id) {
    case OBEX_HEADER_TARGET:
        // TODO: verify target
        break;
    case OBEX_HEADER_APPLICATION_PARAMETERS:
        if (data_offset == 0) {
            obex_app_param_parser_init(&map_access_server->request.app_param_parser,
                &map_access_server_app_param_callback_connect, total_len, map_access_server);
        }
        obex_app_param_parser_process_data(&map_access_server->request.app_param_parser, data_buffer, data_len);
        break;
    default:
        break;
    }
}

static void map_access_server_app_param_callback_get(void* user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t* data_buffer, uint8_t data_len) {
    map_access_server_t* map_access_server = (map_access_server_t*)user_data;
    obex_app_param_parser_tag_state_t state;
    switch (tag_id) {
    //case MAP_APPLICATION_PARAMETER_SEARCH_VALUE:
    //    state = obex_app_param_parser_tag_store((uint8_t*)map_access_server->request.app_params.search_value,
    //        MAP_SERVER_MAX_SEARCH_VALUE_LEN - 1, total_len,
    //        data_offset, data_buffer, data_len);
    //    // assert trailing zero
    //    map_access_server->request.app_params.search_value[MAP_SERVER_MAX_SEARCH_VALUE_LEN - 1] = 0;
    //    break;
    default:
        state = obex_app_param_parser_tag_store(map_access_server->request.app_param_buffer,
            sizeof(map_access_server->request.app_param_buffer), total_len,
            data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE) {
            switch (tag_id) {
            //case MAP_APPLICATION_PARAMETER_PROPERTY_SELECTOR:
            //    // read lower 32 bit
            //    map_access_server->request.app_params.property_selector = big_endian_read_32(
            //        map_access_server->request.app_param_buffer, 4);
            //    break;
            //case MAP_APPLICATION_PARAMETER_VCARD_SELECTOR:
            //    // read lower 32 bit
            //    map_access_server->request.app_params.vcard_selector = big_endian_read_32(
            //        map_access_server->request.app_param_buffer, 4);
            //    break;
            //case MAP_APPLICATION_PARAMETER_FORMAT:
            //    map_access_server->request.app_params.format = (map_format_vcard_t)map_access_server->request.app_param_buffer[0];
            //    break;
            case MAP_APPLICATION_PARAMETER_MAX_LIST_COUNT:
                map_access_server->request.app_params.max_list_count = big_endian_read_16(
                    map_access_server->request.app_param_buffer, 0);
                break;
            //case MAP_APPLICATION_PARAMETER_LIST_START_OFFSET:
            //    map_access_server->request.app_params.list_start_offset = big_endian_read_16(
            //        map_access_server->request.app_param_buffer, 0);
            //    break;
            //case MAP_APPLICATION_PARAMETER_RESET_NEW_MISSED_CALLS:
            //    map_access_server->request.app_params.reset_new_missed_calls = map_access_server->request.app_param_buffer[0];
            //    break;
            //case MAP_APPLICATION_PARAMETER_VCARD_SELECTOR_OPERATOR:
            //    map_access_server->request.app_params.vcard_selector_operator = map_access_server->request.app_param_buffer[0];
            //    break;
            //case MAP_APPLICATION_PARAMETER_ORDER:
            //    map_access_server->request.app_params.vcard_selector_operator = map_access_server->request.app_param_buffer[0];
            //    break;
            //case MAP_APPLICATION_PARAMETER_SEARCH_PROPERTY:
            //    map_access_server->request.app_params.search_property = map_access_server->request.app_param_buffer[0];
            //    break;
            default:
                break;
            }
        }
        break;
    }
}

static void map_access_server_parser_callback_get(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    map_access_server_t* map_access_server = (map_access_server_t*)user_data;

    switch (header_id) {
    case OBEX_HEADER_SINGLE_RESPONSE_MODE:
        obex_parser_header_store(&map_access_server->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
        break;
    case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
        obex_parser_header_store(&map_access_server->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
        break;
    case OBEX_HEADER_CONNECTION_ID:
        // TODO: verify connection id
        break;
    case OBEX_HEADER_NAME:
        // name is stored in big endian unicode-16
        if (total_len < (MAP_SERVER_MAX_NAME_LEN * 2)) {
            uint16_t i;
            for (i = 0; i < data_len; i++) {
                if (((data_offset + i) & 1) == 1) {
                    map_access_server->request.name[(data_offset + i) >> 1] = data_buffer[i];
                }
            }
            map_access_server->request.name[total_len / 2] = 0;
        }
        break;
    case OBEX_HEADER_TYPE:
        if (total_len < MAP_SERVER_MAX_TYPE_LEN) {
            memcpy(&map_access_server->request.type[data_offset], data_buffer, data_len);
            map_access_server->request.type[total_len] = 0;
        }
        break;
    case OBEX_HEADER_APPLICATION_PARAMETERS:
        if (data_offset == 0) {
            obex_app_param_parser_init(&map_access_server->request.app_param_parser,
                &map_access_server_app_param_callback_get, total_len, map_access_server);
        }
        obex_app_param_parser_process_data(&map_access_server->request.app_param_parser, data_buffer, data_len);
        break;
    default:
        break;
    }
}

static void map_access_server_handle_get_request(map_access_server_t* map_access_server) {
    map_access_server_handle_srm_headers(map_access_server);
    map_access_server->request.object_type = map_access_server_parse_object_type(map_access_server->request.type);
    map_phonebook_t phonebook = MAP_PHONEBOOK_INVALID;
    uint16_t name_len = (uint16_t)strlen(map_access_server->request.name);
    switch (map_access_server->request.object_type) {
    case MAP_OBJECT_TYPE_INVALID:
        // unknown object type
        map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
        map_access_server->response.code = OBEX_RESP_BAD_REQUEST;
        goep_server_request_can_send_now(map_access_server->goep_cid);
        return;
    //case MAP_OBJECT_TYPE_PHONEBOOOK:
    //    // lookup phonebook by absolute path
    //    phonebook = map_access_server_get_phonebook_by_path(map_access_server->request.name);
    //    break;
    //case MAP_OBJECT_TYPE_VCARD_LISTING:
    //    // lookup phonebook by relative name if name given
    //    if (name_len == 0) {
    //        phonebook = map_access_server->map_phonebook;
    //    }
    //    else {
    //        phonebook = map_access_server_get_phonebook_by_dir_and_name(map_access_server->map_access_server_dir, map_access_server->request.name);
    //    }
    //    break;
    //case MAP_OBJECT_TYPE_VCARD:
    //    phonebook = map_access_server->map_phonebook;
    //    break;
    default:
        btstack_unreachable();
        break;
    }

    if (phonebook == MAP_PHONEBOOK_INVALID) {
        map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
        map_access_server->response.code = OBEX_RESP_NOT_FOUND;
        goep_server_request_can_send_now(map_access_server->goep_cid);
        return;
    }

    //// ResetNewMissedCalls
    //if (map_access_server->request.app_params.reset_new_missed_calls == 1) {
    //    uint8_t event[6];
    //    uint16_t pos = 0;
    //    event[pos++] = HCI_EVENT_MAP_META;
    //    event[pos++] = sizeof(event) - 2;
    //    event[pos++] = MAP_SUBEVENT_RESET_MISSED_CALLS;
    //    little_endian_store_16(event, pos, map_access_server->map_cid);
    //    pos += 2;
    //    event[pos++] = phonebook;
    //    (*map_access_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
    //}

    // MaxListCount == 0 -> query size
    if (map_access_server->request.app_params.max_list_count == 0) {
        // not valid for vCard request
        if (map_access_server->request.object_type == MAP_OBJECT_TYPE_VCARD) {
            map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
            map_access_server->response.code = OBEX_RESP_BAD_REQUEST;
            goep_server_request_can_send_now(map_access_server->goep_cid);
        }
        //else {
        //    map_access_server->state = MAP_SERVER_STATE_W4_USER_DATA;
        //    uint8_t event[11];
        //    uint16_t pos = 0;
        //    event[pos++] = HCI_EVENT_MAP_META;
        //    event[pos++] = sizeof(event) - 2;
        //    event[pos++] = MAP_SUBEVENT_QUERY_PHONEBOOK_SIZE;
        //    little_endian_store_16(event, pos, map_access_server->map_cid);
        //    pos += 2;
        //    little_endian_store_32(event, pos, map_access_server->request.app_params.vcard_selector);
        //    pos += 4;
        //    event[pos++] = map_access_server->request.app_params.vcard_selector_operator;
        //    event[pos++] = phonebook;
        //    (*map_access_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
        //}
        return;
    }
    dbg_printf("partial implementation, shouldnt be reached\n");
    //// emit pull ( phonebook, vcard_listing, vcard)
    //uint8_t event[2 + 20 + MAP_SERVER_MAX_NAME_LEN + MAP_SERVER_MAX_SEARCH_VALUE_LEN];
    //uint16_t pos = 0;
    //uint16_t search_value_len;
    //event[pos++] = HCI_EVENT_MAP_META;
    //switch (map_access_server->request.object_type) {
    //case MAP_OBJECT_TYPE_PHONEBOOOK:
    //    event[pos++] = 22;
    //    event[pos++] = MAP_SUBEVENT_PULL_PHONEBOOK;
    //    little_endian_store_16(event, pos, map_access_server->map_cid);
    //    pos += 2;
    //    little_endian_store_32(event, pos, map_access_server->request.continuation);
    //    pos += 4;
    //    little_endian_store_32(event, pos, map_access_server->request.app_params.property_selector);
    //    pos += 4;
    //    event[pos++] = map_access_server->request.app_params.format;
    //    little_endian_store_16(event, pos, map_access_server->request.app_params.max_list_count);
    //    pos += 2;
    //    little_endian_store_16(event, pos, map_access_server->request.app_params.list_start_offset);
    //    pos += 2;
    //    little_endian_store_32(event, pos, map_access_server->request.app_params.vcard_selector);
    //    pos += 4;
    //    event[pos++] = map_access_server->request.app_params.vcard_selector_operator;
    //    event[pos++] = phonebook;
    //    break;
    //case MAP_OBJECT_TYPE_VCARD_LISTING:
    //    search_value_len = (uint16_t)strlen(map_access_server->request.app_params.search_value);
    //    event[pos++] = 20 + search_value_len + 1;
    //    event[pos++] = MAP_SUBEVENT_PULL_VCARD_LISTING;
    //    little_endian_store_16(event, pos, map_access_server->map_cid);
    //    pos += 2;
    //    little_endian_store_32(event, pos, map_access_server->request.continuation);
    //    pos += 4;
    //    event[pos++] = map_access_server->request.app_params.order;
    //    little_endian_store_16(event, pos, map_access_server->request.app_params.max_list_count);
    //    pos += 2;
    //    little_endian_store_16(event, pos, map_access_server->request.app_params.list_start_offset);
    //    pos += 2;
    //    little_endian_store_32(event, pos, map_access_server->request.app_params.vcard_selector);
    //    pos += 4;
    //    event[pos++] = map_access_server->request.app_params.vcard_selector_operator;
    //    event[pos++] = map_access_server->request.app_params.search_property;
    //    // search_value is zero terminated
    //    event[pos++] = search_value_len + 1;
    //    memcpy(&event[pos], (const uint8_t*)map_access_server->request.app_params.search_value, search_value_len + 1);
    //    pos += search_value_len + 1;
    //    event[pos++] = phonebook;
    //    break;
    //case MAP_OBJECT_TYPE_VCARD:
    //    event[pos++] = 13 + name_len + 1;
    //    event[pos++] = MAP_SUBEVENT_PULL_VCARD_ENTRY;
    //    little_endian_store_16(event, pos, map_access_server->map_cid);
    //    pos += 2;
    //    little_endian_store_32(event, pos, map_access_server->request.continuation);
    //    pos += 4;
    //    little_endian_store_32(event, pos, map_access_server->request.app_params.property_selector);
    //    pos += 4;
    //    event[pos++] = map_access_server->request.app_params.format;
    //    event[pos++] = phonebook;
    //    // name is zero terminated
    //    memcpy(&event[pos], map_access_server->request.name, name_len + 1);
    //    pos += name_len + 1;
    //    break;
    //default:
    //    btstack_unreachable();
    //    break;
    //}
    //map_access_server->state = MAP_SERVER_STATE_W4_USER_DATA;
    //(*map_access_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static void map_access_server_packet_handler_goep(map_access_server_t* map_access_server, uint8_t* packet, uint16_t size) {
    btstack_assert(size > 0);
    uint8_t opcode;
    obex_parser_object_state_t parser_state;

    switch (map_access_server->state) {
    case MAP_SERVER_STATE_W4_OPEN:
        btstack_unreachable();
        break;
    case MAP_SERVER_STATE_W4_CONNECT_OPCODE:
        map_access_server->state = MAP_SERVER_STATE_W4_CONNECT_REQUEST;
        map_access_server_default_headers(map_access_server);
        obex_parser_init_for_request(&map_access_server->obex_parser, &map_access_server_parser_callback_connect, (void*)map_access_server);

        /* fall through */

    case MAP_SERVER_STATE_W4_CONNECT_REQUEST:
        parser_state = obex_parser_process_data(&map_access_server->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&map_access_server->obex_parser, &op_info);
            bool ok = true;
            // check opcode
            if (op_info.opcode != OBEX_OPCODE_CONNECT) {
                ok = false;
            }
            // TODO: check Target
            if (ok == false) {
                // send bad request response
                map_access_server->state = MAP_SERVER_STATE_SEND_CONNECT_RESPONSE_ERROR;
            }
            else {
                // send connect response
                map_access_server->state = MAP_SERVER_STATE_SEND_CONNECT_RESPONSE_SUCCESS;
            }
            goep_server_request_can_send_now(map_access_server->goep_cid);
        }
        break;
    case MAP_SERVER_STATE_CONNECTED:
        opcode = packet[0];
        // default headers
        switch (opcode) {
        case OBEX_OPCODE_GET:
        case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
            map_access_server->state = MAP_SERVER_STATE_W4_REQUEST;
            obex_parser_init_for_request(&map_access_server->obex_parser, &map_access_server_parser_callback_get, (void*)map_access_server);
            break;
        case OBEX_OPCODE_SETPATH:
            map_access_server->state = MAP_SERVER_STATE_W4_REQUEST;
            obex_parser_init_for_request(&map_access_server->obex_parser, &map_access_server_parser_callback_get, (void*)map_access_server);
            break;
        case OBEX_OPCODE_DISCONNECT:
            map_access_server->state = MAP_SERVER_STATE_W4_REQUEST;
            obex_parser_init_for_request(&map_access_server->obex_parser, NULL, NULL);
            break;
        case OBEX_OPCODE_ACTION:
        default:
            map_access_server->state = MAP_SERVER_STATE_W4_REQUEST;
            obex_parser_init_for_request(&map_access_server->obex_parser, NULL, NULL);
            break;
        }

        /* fall through */

    case MAP_SERVER_STATE_W4_REQUEST:
        obex_srm_init(&map_access_server->obex_srm);
        parser_state = obex_parser_process_data(&map_access_server->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&map_access_server->obex_parser, &op_info);
            switch (op_info.opcode) {
            case OBEX_OPCODE_GET:
            case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
                map_access_server_handle_get_request(map_access_server);
                break;
            case OBEX_OPCODE_SETPATH:
                map_access_server_handle_set_path_request(map_access_server, op_info.flags, &map_access_server->request.name[0]);
                break;
            case OBEX_OPCODE_DISCONNECT:
                map_access_server->state = MAP_SERVER_STATE_SEND_DISCONNECT_RESPONSE;
                goep_server_request_can_send_now(map_access_server->goep_cid);
                break;
            case OBEX_OPCODE_ACTION:
            case (OBEX_OPCODE_ACTION | OBEX_OPCODE_FINAL_BIT_MASK):
            case OBEX_OPCODE_SESSION:
            default:
                // send bad request response
                map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                map_access_server->response.code = OBEX_RESP_BAD_REQUEST;
                goep_server_request_can_send_now(map_access_server->goep_cid);
                break;
            }
        }
        break;

    case MAP_SERVER_STATE_W4_GET_OPCODE:
        map_access_server->state = MAP_SERVER_STATE_W4_GET_REQUEST;
        obex_parser_init_for_request(&map_access_server->obex_parser, &map_access_server_parser_callback_get, (void*)map_access_server);

        /* fall through */

    case MAP_SERVER_STATE_W4_GET_REQUEST:
        obex_srm_init(&map_access_server->obex_srm);
        parser_state = obex_parser_process_data(&map_access_server->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&map_access_server->obex_parser, &op_info);
            switch ((op_info.opcode & 0x7f)) {
            case OBEX_OPCODE_GET:
                map_access_server_handle_get_request(map_access_server);
                break;
            case (OBEX_OPCODE_ABORT & 0x7f):
                map_access_server->response.code = OBEX_RESP_SUCCESS;
                map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                goep_server_request_can_send_now(map_access_server->goep_cid);
                break;
            default:
                // send bad request response
                map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
                map_access_server->response.code = OBEX_RESP_BAD_REQUEST;
                goep_server_request_can_send_now(map_access_server->goep_cid);
                break;
            }
        }
        break;
    default:
        break;
    }
}

static void map_access_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events
    map_access_server_t* map_access_server;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        map_access_server_packet_handler_hci(packet, size);
        break;
    case GOEP_DATA_PACKET:
        map_access_server = map_access_server_for_goep_cid(channel);
        btstack_assert(map_access_server != NULL);
        map_access_server_packet_handler_goep(map_access_server, packet, size);
        break;
    default:
        break;
    }
}

void map_access_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, uint16_t mtu) {
    //maximum_obex_packet_length = mtu;
    goep_server_register_service(&map_access_server_packet_handler, rfcomm_channel_nr, 0xFFFF, l2cap_psm, 0xFFFF, LEVEL_0);

    map_access_server_user_packet_handler = packet_handler;
}

void map_access_server_deinit(void) {
}

// note: common code for next four setters
static bool map_access_server_valid_header_for_request(map_access_server_t* map_access_server) {
    if (map_access_server->state != MAP_SERVER_STATE_W4_USER_DATA) {
        return false;
    }
    switch (map_access_server->request.object_type) {
    case MAP_OBJECT_TYPE_PHONEBOOOK:
    case MAP_OBJECT_TYPE_VCARD_LISTING:
        return true;
    default:
        return false;
    }

}
uint8_t map_access_server_set_new_missed_calls(uint16_t map_cid, uint16_t new_missed_calls) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_server_valid_header_for_request(map_access_server)) {
        map_access_server->response.new_missed_calls = new_missed_calls;
        map_access_server->response.new_missed_calls_set = true;
        return ERROR_CODE_SUCCESS;
    }
    else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t map_access_server_set_primary_folder_version(uint16_t map_cid, const uint8_t* primary_folder_version) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_server_valid_header_for_request(map_access_server)) {
        (void)memcpy(map_access_server->response.primary_folder_version, primary_folder_version, MAP_FOLDER_VERSION_LEN);
        map_access_server->response.primary_folder_version_set = true;
        return ERROR_CODE_SUCCESS;
    }
    else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t map_access_server_set_secondary_folder_version(uint16_t map_cid, const uint8_t* secondary_folder_version) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_server_valid_header_for_request(map_access_server)) {
        (void)memcpy(map_access_server->response.secondary_folder_version, secondary_folder_version, MAP_FOLDER_VERSION_LEN);
        map_access_server->response.secondary_folder_version_set = true;
        return ERROR_CODE_SUCCESS;
    }
    else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t map_access_server_set_database_identifier(uint16_t map_cid, const uint8_t* database_identifier) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_server->state != MAP_SERVER_STATE_W4_USER_DATA) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    btstack_assert(map_access_server->request.object_type != MAP_OBJECT_TYPE_INVALID);

    (void)memcpy(map_access_server->response.database_identifier, database_identifier, MAP_FOLDER_VERSION_LEN);
    map_access_server->response.database_identifier_set = true;
    return ERROR_CODE_SUCCESS;
};

static void map_access_server_build_response(map_access_server_t* map_access_server) {
    goep_server_response_create_general(map_access_server->goep_cid);
    map_access_server_add_srm_headers(map_access_server);
    // Application Params
    uint8_t app_params[MAP_SERVER_MAX_APP_PARAMS_LEN];
    uint16_t app_params_pos = 0;
    //if (map_access_server->response.phonebook_size_set) {
    //    map_access_server->response.phonebook_size_set = false;
    //    app_params_pos += map_access_server_application_params_add_phonebook_size(&app_params[app_params_pos], map_access_server->response.phonebook_size);
    //}
    //if (map_access_server->response.new_missed_calls_set) {
    //    map_access_server->response.new_missed_calls_set = false;
    //    app_params_pos += map_access_server_application_params_add_new_missed_calls(&app_params[app_params_pos], map_access_server->response.new_missed_calls);
    //}
    //if (map_access_server->response.primary_folder_version_set) {
    //    map_access_server->response.primary_folder_version_set = false;
    //    app_params_pos += map_access_server_application_params_add_primary_folder_version(&app_params[app_params_pos], map_access_server->response.primary_folder_version);
    //}
    //if (map_access_server->response.secondary_folder_version_set) {
    //    map_access_server->response.secondary_folder_version_set = false;
    //    app_params_pos += map_access_server_application_params_add_secondary_folder_version(&app_params[app_params_pos], map_access_server->response.secondary_folder_version);
    //}
    //if (map_access_server->response.database_identifier_set) {
    //    map_access_server->response.database_identifier_set = false;
    //    app_params_pos += map_access_server_application_params_add_database_identifier(&app_params[app_params_pos], map_access_server->response.database_identifier);
    //}
    map_access_server_add_application_parameters(map_access_server, app_params, app_params_pos);
}

uint16_t map_access_server_get_max_body_size(uint16_t map_cid) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return 0;
    }
    if (map_access_server->state != MAP_SERVER_STATE_W4_USER_DATA) {
        return 0;
    }
    btstack_assert(map_access_server->request.object_type != MAP_OBJECT_TYPE_INVALID);

    // calc max body size without reserving outgoing buffer: packet size - OBEX Header (3) - SRM Header (2) - Body Header (3)
    return goep_server_response_get_max_message_size(map_access_server->goep_cid) - (3 + 2 + 3);
}

uint8_t map_access_server_send_phonebook_size(uint16_t map_cid, uint8_t response_code, uint16_t phonebook_size) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // phonebook size valid for PHONEBOOK and VCARD_LIST query
    if (map_access_server_valid_header_for_request(map_access_server)) {
        map_access_server->response.phonebook_size = phonebook_size;
        map_access_server->response.phonebook_size_set = true;
        map_access_server->response.code = response_code;
        map_access_server->state = MAP_SERVER_STATE_SEND_USER_RESPONSE;
        return goep_server_request_can_send_now(map_access_server->goep_cid);
    }
    else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint16_t map_access_server_send_pull_response(uint16_t map_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t* body) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    // double check size

    // calc max body size without reserving outgoing buffer: packet size - OBEX Header (3) - SRM Header (2) - Body Header (3)
    uint16_t max_body_size = goep_server_response_get_max_message_size(map_access_server->goep_cid) - (3 + 2 + 3);
    if (body_len > max_body_size) {
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    // set data for response and trigger execute
    map_access_server->response.code = response_code;
    map_access_server->request.continuation = continuation;
    map_access_server->state = MAP_SERVER_STATE_SEND_USER_RESPONSE;
    map_access_server->response.body_data = body;
    map_access_server->response.body_len = body_len;
    return goep_server_request_can_send_now(map_access_server->goep_cid);
}

// suppress MSVC C4244: unchecked upper bound for enum phonebook used as index
#ifdef _MSC_VER
#pragma warning( disable : 33011 )
#endif

const char* map_access_server_get_phonebook_path(map_phonebook_t phonebook) {
    btstack_assert(phonebook >= MAP_PHONEBOOK_TELECOM_CCH);
    btstack_assert(phonebook <= MAP_PHONEBOOK_SIM_TELECOM_PB);
    return map_access_server_phonebooks[(uint16_t)phonebook].path;
}

const char* map_access_server_get_phonebook_name(map_phonebook_t phonebook) {
    btstack_assert(phonebook >= MAP_PHONEBOOK_TELECOM_CCH);
    btstack_assert(phonebook <= MAP_PHONEBOOK_SIM_TELECOM_PB);
    return map_access_server_phonebooks[(uint16_t)phonebook].name;
}

bool map_access_server_is_phonebook_on_sim1(map_phonebook_t phonebook) {
    return (phonebook >= MAP_PHONEBOOK_TELECOM_CCH) && (phonebook <= MAP_PHONEBOOK_TELECOM_SPD);
}
