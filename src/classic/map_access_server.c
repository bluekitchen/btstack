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

 // TODO: copied from PBAP_server, to be adapted to MAS server
 // max app params for vcard listing and folder response:
 // - NewMissedCalls
 // - DatabaseIdentifier
 // - PrimaryFolderVersion,
 // - SecondaryFolderVersion
#define MAP_SERVER_MAX_APP_PARAMS_LEN ((4*2) + 2 + MAS_DATABASE_IDENTIFIER_LEN + (2*MAS_FOLDER_VERSION_LEN))


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


//typedef char    variable_string_t[64];
//typedef char    variable_utf8[64];
//typedef uint8_t variable_uint128[16];
//typedef uint8_t variable_uint64[8];

typedef uint32_t variable_string_t;
typedef uint32_t variable_utf8;
typedef uint32_t variable_uint128;
typedef uint32_t variable_uint64;


#define app_param_read_uint8_t           big_endian_read_08
#define app_param_read_uint16_t          big_endian_read_16
#define app_param_read_uint32_t          big_endian_read_32
#define app_param_read_variable_string_t big_endian_read_32 // TODO: dummy
#define app_param_read_variable_utf8     big_endian_read_32 // TODO: dummy
#define app_param_read_variable_uint64   big_endian_read_32 // TODO: dummy
#define app_param_read_variable_uint128  big_endian_read_32 // TODO: dummy

// Data extracted from "Message Access Profile"
// Bluetooth  Profile Specification
// *  Revision : v1.4.2
// *  Revision Date : 2019 - 08 - 13
// *  Group Prepared By : Audio, Telephony, and Automotive Working Group

// compact storage of supported ApplicationParameters
// X-Macro below provides enumeration and mapping table
// 
//X(Parameter Name         , Tag , Type               , free text description ... no coma ... multiple _backslash_no_space lines ... )
#define APP_PARAMS                                                                                                                        \
X(MaxListCount             , 0x01, uint16_t           , 0000 to 0xFFFF                                                                   )\
X(ListStartOffset          , 0x02, uint16_t           , 0x0000 to 0xFFFF                                                                 )\
X(FilterMessageType        , 0x03, uint8_t            , Bit mask: 0b000XXXX1 = "SMS_GSM"                                                  \
                                                                  0b000XXX1X = "SMS_CDMA"                                                 \
                                                                  0b000XX1XX = "EMAIL" 0b000X1XXX = "MMS" 0b0001XXXX = "IM"               \
                                                                  All other values : Reserved for Future Use                              \
                                                                  Where                                                                   \
                                                                  0 = "no filtering; get this type"                                       \
                                                                  1 = "filter out this type"                                             )\
 X(FilterPeriodBegin       , 0x04, variable_string_t  , with Begin of filter period.See Section 5.5.4                                    )\
 X(EndFilterPeriodEnd      , 0x05, variable_string_t  , with End of filter period.See Section 5.5.4                                      )\
 X(FilterReadStatus        , 0x06, uint8_t            , 1 byte Bit mask : 0b00000001 = get unread messages only                           \
                                                        0b00000010 = get read messages only                                               \
                                                        0b00000000 =                                                                      \
                                                        no - filtering; get both read and unread messages; all other values : undefined  )\
 X(FilterRecipient 	       , 0x07, variable_string_t  , variable Text(UTF - 8) wildcards "*" may 	be used if required                  )\
 X(FilterOriginator        , 0x08, variable_string_t  , variable Text(UTF - 8) wildcards "*" may be used if required                     )\
 X(FilterPriority          , 0x09, uint8_t            , Bit mask: 0b00000000 = no - filtering                                             \
                                                                  0b00000001 = get high priority messages only                            \
                                                                  0b00000010 = get non - high priority messages only;                     \
                                                                  all other values : undefined                                           )\
 X(Attachment              , 0x0A, uint8_t            , 0b1 = "ON"                                                                        \
                                                        0b0 = "OFF"                                                                      )\
 X(Transparent             , 0x0B, uint8_t            , 0b1 = "ON"                                                                        \
                                                        0b0 = "OFF"                                                                      )\
 X(Retry                   , 0x0C, uint8_t            , 0b1 = "ON"                                                                        \
                                                        0b0 = "OFF"                                                                      )\
 X(NewMessage              , 0x0D, uint8_t            , 0b1 = "ON"                                                                        \
                                                        0b0 = "OFF"                                                                      )\
 X(NotificationStatus      , 0x0E, uint8_t            , 0b1 = "ON"                                                                        \
                                                        0b0 = "OFF"                                                                      )\
 X(MASInstanceID           , 0x0F, uint8_t            , 0 to 255                                                                         )\
 X(ParameterMask           , 0x10, uint32_t           , Bit mask; settings see Section 5.5.4                                             )\
 X(FolderListingSize       , 0x11, uint16_t           , 0x0000 to 0xFFFF                                                                 )\
 X(ListingSize             , 0x12, uint16_t           , 0x0000 to 0xFFFF                                                                 )\
 X(SubjectLength           , 0x13, uint8_t            , 1 to 255                                                                         )\
 X(Charset                 , 0x14, uint8_t            , 0 = "native"                                                                      \
                                                        1 = "UTF-8"                                                                      )\
 X(FractionRequest         , 0x15, uint8_t            , 0 = "first" 1 = "next"                                                           )\
 X(FractionDeliver         , 0x16, uint8_t            , 0 = "more"                                                                        \
                                                        1 = "last"                                                                       )\
 X(StatusIndicator         , 0x17, uint8_t            , 0 = "readStatus"                                                                  \
                                                        1 = "deletedStatus"                                                               \
                                                        2 = “setExtendedData”                                                            )\
 X(StatusValue             , 0x18, uint8_t            , 1 = "yes"                                                                         \
                                                        0 = "no"                                                                         )\
 X(MSETime                 , 0x19, variable_string_t  , with current time basis and UTC - offset of the MSE.See Section 5.5.4            )\
 X(DatabaseIdentifier      , 0x1A, variable_uint128   , (max 3uint16_t)    ;   128 - bit value in hex string format                      )\
 X(ListingVersionCounter   , 0x1B, variable_uint128   , (max 3uint16_t)    ;   128 - bit value in hex string format                      )\
 X(PresenceAvailability    , 0x1C, uint8_t            , 0 to 255                                                                         )\
 X(PresenceText            , 0x1D, variable_utf8      , Text UTF - 8                                                                     )\
 X(LastActivity            , 0x1E, variable_utf8      , Text UTF - 8                                                                     )\
 X(FilterLastActivityBegin , 0x1F, variable_utf8      , Text UTF - 8                                                                     )\
 X(FilterLastActivityEnd   , 0x20, variable_utf8      , Text UTF - 8                                                                     )\
 X(ChatState               , 0x21, uint8_t            , 0 to 255                                                                         )\
 X(ConversationID          , 0x22, variable_uint128   , (max 3uint16_t)    ;   128 - bit value in hex string format                      )\
 X(FolderVersionCounter    , 0x23, variable_uint128   , (max 3uint16_t);   128 - bit value in hex string format                          )\
 X(FilterMessageHandle     , 0x24, variable_uint64    , 64 - bit value in hex string format                                              )\
 X(NotificationFilterMask  , 0x25, uint32_t           , Bit mask settings; see Section 5.14.3.1                                          )\
 X(ConvParameterMask       , 0x26, uint32_t           , Bit mask settings; see Section 5.13.3.10                                         )\
 X(OwnerUCI                , 0x27, variable_utf8      , Text UTF - 8                                                                     )\
 X(ExtendedData            , 0x28, variable_utf8      , Text UTF - 8                                                                     )\
 X(MapSupportedFeatures    , 0x29, uint32_t           , Bit 0 = Notification Registration Feature                                         \
                                                        Bit 1 = Notification Feature                                                      \
                                                        Bit 2 = Browsing Feature                                                          \
                                                        Bit 3 = Uploading Feature                                                         \
                                                        Bit 4 = Delete Feature                                                            \
                                                        Bit 5 = Instance Information Feature                                              \
                                                        Bit 6 = Extended Event Report 1.1                                                 \
                                                        Bit 7 = Event Report Version 1.2                                                  \
                                                        Bit 8 = Message Format Version 1.1                                                \
                                                        Bit 9 = Messages - Listing Format Version 1.1                                     \
                                                        Bit 10 = Persistent Message Handles                                               \
                                                        Bit 11 = Database Identifier                                                      \
                                                        Bit 12 = Folder Version Counter                                                   \
                                                        Bit 13 = Conversation Version Counters                                            \
                                                        Bit 14 = Participant Presence Change Notification                                 \
                                                        Bit 15 = Participant Chat State Change Notification                               \
                                                        Bit 16 = PBAP Contact Cross Reference                                             \
                                                        Bit 17 = Notification Filtering                                                   \
                                                        Bit 18 = UTC Offset Timestamp Format                                              \
                                                        Bit 19 = Reserved                                                                 \
                                                        Bit 20 = Conversation listing                                                     \
                                                        Bit 21 = Owner status                                                             \
                                                        Bits 22 to 31 = Reserved for Future Use0F                                        )\
 X(MessageHandle           , 0x2A, variable_uint64    , 64 - bit value in hex string format                                              )\
 X(ModifyText              , 0x2B, uint8_t            , 0 = "REPLACE"                                                                    )


enum MAP_APP_PARAMS
{
#define X(name, tag, type, descr) MAP_APP_PARAM_ ## name = tag,
    APP_PARAMS
#undef X
};

typedef struct {
    uint16_t map_cid;
    uint16_t goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    bool     incoming;
    map_access_server_state_t state;
    obex_parser_t obex_parser;
    uint16_t map_supported_features;
    mas_folder_t map_access_server_dir;
    mas_folder_t  map_folder;
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

#define X(name, tag, type, descr) type name;
        APP_PARAMS
    
#undef X
        } app_params;
    } request;
    // response
    struct {
        uint8_t code;
        uint16_t new_messages_count;
        bool new_messages;
        bool folder_size_set;
        bool database_identifier_set;
        bool folder_version_set;
        uint16_t folder_size;
        uint8_t database_identifier[MAS_DATABASE_IDENTIFIER_LEN];
        uint8_t folder_version[MAS_FOLDER_VERSION_LEN];
        uint16_t body_len;
        const uint8_t* body_data;
    } response;
} map_access_server_t;

static map_access_server_t map_access_server_singleton;

static struct {
    char* name;
    mas_folder_t parent_dir;
    char* path;
} map_access_server_folders[] = {
    {"msg",     MAS_FOLDER_TELECOM_MSG, "telecom/msg.vcf"},
    {"inbox",   MAS_FOLDER_TELECOM_MSG_INBOX, "telecom/msg/inbox.vcf"},
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

/* only to be called if the GEOP connection is closed
* if only OBEX connection is closed we need to go to  MAP_SERVER_STATE_W4_CONNECT_OPCODE
*/
static void map_access_server_finalize_connection(map_access_server_t* map_access_server) {
    // minimal
    map_access_server->state = MAP_SERVER_STATE_W4_OPEN;
}

static mas_folder_t map_access_server_get_folder_by_path(const char* path) {
    return MAS_FOLDER_TELECOM_MSG;
}

static mas_folder_t map_access_server_get_folder_by_dir_and_name(mas_folder_t parent_dir, const char* name) {
    uint16_t index;
    for (index = 0; index < (sizeof(map_access_server_folders) / sizeof(map_access_server_folders[0])); index++) {
        if ((parent_dir == map_access_server_folders[index].parent_dir) && (strcmp(name, map_access_server_folders[index].name) == 0)) {
            return MAS_FOLDER_TELECOM_MSG + index;
        }
    }
    return MAS_FOLDER_INVALID;
}

static void map_access_server_handle_set_path_request(map_access_server_t* map_access_server, uint8_t flags, const char* name) {
    uint16_t name_len = (uint16_t)strlen(name);
    uint8_t obex_result = OBEX_RESP_SUCCESS;
    if (name_len == 0) {
        // no path name given, one dir up?
        if IS_BIT_SET(flags, OBEX_SP_BIT0_DIR_UP) {
            switch (map_access_server->map_access_server_dir) {
            case MAS_FOLDER_TELECOM:
                map_access_server->map_access_server_dir = MAS_FOLDER_ROOT;
                break;
            case MAS_FOLDER_TELECOM_MSG:
                map_access_server->map_access_server_dir = MAS_FOLDER_TELECOM;
                break;
            case MAS_FOLDER_TELECOM_MSG_INBOX:
                map_access_server->map_access_server_dir = MAS_FOLDER_TELECOM_MSG;
                break;
            default:
                obex_result = OBEX_RESP_NOT_FOUND;
                break;
            }
        }
        else {
            map_access_server->map_access_server_dir = MAS_FOLDER_ROOT;
        };
    }
    else {
        switch (map_access_server->map_access_server_dir) {
        case MAS_FOLDER_ROOT:
            if (strcmp("telecom", name) == 0) {
                map_access_server->map_access_server_dir = MAS_FOLDER_TELECOM;
            }
            else {
                obex_result = OBEX_RESP_NOT_FOUND;
            }
            break;
        case MAS_FOLDER_TELECOM:
            if (strcmp("msg", name) == 0) {
                map_access_server->map_access_server_dir = MAS_FOLDER_TELECOM_MSG;
            }
            else {
                obex_result = OBEX_RESP_NOT_FOUND;
            }
            break;
        case MAS_FOLDER_TELECOM_MSG:
            if (strcmp("inbox", name) == 0) {
                map_access_server->map_access_server_dir = MAS_FOLDER_TELECOM_MSG_INBOX;
            }
            else {
                obex_result = OBEX_RESP_NOT_FOUND;
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
    
    if (strcmp("x-obex/folder-listing", type_string) == 0) {
        return MAP_OBJECT_TYPE_GET_FOLDER_LISTING;
    }
    
    if (strcmp("x-bt/MAP-msg-listing", type_string) == 0) {
        return MAP_OBJECT_TYPE_GET_MSG_LISTING;
    }

    if (strcmp("x-bt/message", type_string) == 0) {
        return MAP_OBJECT_TYPE_GET_MESSAGE;
    }

    if (strcmp("x-bt/messageStatus", type_string) == 0) {
        return MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS;
    }


    return MAP_OBJECT_TYPE_INVALID;
}

static void map_access_server_obex_srm_init(obex_srm_t* obex_srm) {
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

static void map_access_server_add_application_parameters(const map_access_server_t* map_access_server, uint8_t* application_parameters, uint16_t len) {
    if (len > 0) {
        goep_server_header_add_application_parameters(map_access_server->goep_cid, application_parameters, len);
    }
}

static void map_access_server_default_headers(map_access_server_t* map_access_server) {
    (void)memset(&map_access_server->request, 0, sizeof(map_access_server->request));
    map_access_server->request.app_params.MaxListCount = 0xffffU;
    //map_access_server->request.app_params.msg_selector = 0xffffffffUL;
    //map_access_server->request.app_params.property_selector = 0xffffffffUL;
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
        map_access_server->map_access_server_dir = MAS_FOLDER_ROOT;
        map_access_server->map_folder = MAS_FOLDER_INVALID;
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

        // reset MAP/OBEX connection state
        map_access_server->state = MAP_SERVER_STATE_W4_CONNECT_OPCODE;

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
                // TODO: report failed incomming connection
                log_error("TODO: report failed outgoing connection");
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

    default:
        state = obex_app_param_parser_tag_store(map_access_server->request.app_param_buffer,
            sizeof(map_access_server->request.app_param_buffer), total_len,
            data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE) {
            switch (tag_id) {

// X-Macro automagically generates GETers for all APP Params
//case MAP_APPLICATION_PARAMETER_MAX_LIST_COUNT:
//    map_access_server->request.app_params.MaxListCount = big_endian_read_16(
//        map_access_server->request.app_param_buffer, 0);
//    break;

#define X(name, tag, type, descr) \
            case MAP_APP_PARAM_ ## name: \
                map_access_server->request.app_params. name = \
                    app_param_read_ ## type (map_access_server->request.app_param_buffer, 0); \
                log_debug("APP PARAM <%s> value <%08x>", #name, map_access_server->request.app_params. name); \
                break;
                APP_PARAMS
#undef X
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
    mas_folder_t folder = map_access_server->map_access_server_dir;
    uint16_t name_len = (uint16_t)strlen(map_access_server->request.name);
    switch (map_access_server->request.object_type) {
    case MAP_OBJECT_TYPE_INVALID:
        // unknown object type
        map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
        map_access_server->response.code = OBEX_RESP_BAD_REQUEST;
        goep_server_request_can_send_now(map_access_server->goep_cid);
        return;

    case MAP_OBJECT_TYPE_GET_MSG_LISTING:
        folder = map_access_server_get_folder_by_path(map_access_server->request.name);
        break;

    case MAP_OBJECT_TYPE_GET_MESSAGE:
    case MAP_OBJECT_TYPE_GET_FOLDER_LISTING:
    case MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS:
        break;

    default:
        btstack_unreachable();
        break;
    }

    if (folder == MAS_FOLDER_INVALID) {
        map_access_server->state = MAP_SERVER_STATE_SEND_INTERNAL_RESPONSE;
        map_access_server->response.code = OBEX_RESP_NOT_FOUND;
        goep_server_request_can_send_now(map_access_server->goep_cid);
        return;
    }

    // emit get ( folder, msg_listing, msg)
    uint8_t event[2 + 20 + MAP_SERVER_MAX_NAME_LEN + MAP_SERVER_MAX_SEARCH_VALUE_LEN];
    uint16_t pos = 0;
    uint16_t search_value_len;
    app_write_08(event, &pos, HCI_EVENT_MAP_META);
    pos++; // skip size header, its written at the end
    switch (map_access_server->request.object_type) {

    case MAP_OBJECT_TYPE_GET_FOLDER_LISTING:
        app_write_08(event, &pos, MAP_SUBEVENT_FOLDER_LISTING_ITEM);
        app_write_16(event, &pos, map_access_server->map_cid);

        // write message len (after 2 header bytes) into 2nd byte
        event[1] = 25;//pos - 2;
        break;

    case MAP_OBJECT_TYPE_GET_MSG_LISTING:
        app_write_08(event, &pos, MAP_SUBEVENT_GET_MESSAGE_LISTING);
        app_write_16(event, &pos, map_access_server->map_cid);

        // write message len (after 2 header bytes) into 2nd byte
        event[1] = 25;//pos - 2;
        break;

    case MAP_OBJECT_TYPE_GET_MESSAGE:
        * @param continuation - value provided by caller of map_server_send_pull_response
        * @param MaxListCount
        * @param ListStartOffset
        app_write_08(event, &pos, MAP_SUBEVENT_GET_MESSAGE);
        app_write_16(event, &pos, map_access_server->map_cid);
        app_write_16(event, &pos, map_access_server->request.app_params.MaxListCount);
        app_write_16(event, &pos, map_access_server->request.app_params.ListStartOffset);

        // write message len (after 2 header bytes) into 2nd byte
        event[1] = 25;//pos - 2;
        break;

    case MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS:
        app_write_08(event, &pos, MAP_SUBEVENT_PUT_MESSAGE_STATUS);
        app_write_16(event, &pos, map_access_server->map_cid);

        // write message len (after 2 header bytes) into 2nd byte
        event[1] = 25;//pos - 2;
        break;

    default:
        btstack_unreachable();
        break;
    }
    map_access_server->state = MAP_SERVER_STATE_W4_USER_DATA;
    (*map_access_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
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
        case OBEX_OPCODE_PUT:
        case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
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
        map_access_server_obex_srm_init(&map_access_server->obex_srm);
        parser_state = obex_parser_process_data(&map_access_server->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&map_access_server->obex_parser, &op_info);
            switch (op_info.opcode) {
            case OBEX_OPCODE_GET:
            case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
            case OBEX_OPCODE_PUT:
            case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
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
        map_access_server_obex_srm_init(&map_access_server->obex_srm);
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
    case MAP_OBJECT_TYPE_GET_MSG_LISTING:
        return true;
    default:
        return false;
    }

}
uint8_t map_access_server_set_new_messages(uint16_t map_cid, uint16_t new_messages) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_server_valid_header_for_request(map_access_server)) {
        map_access_server->response.new_messages_count = new_messages;
        map_access_server->response.new_messages = true;
        return ERROR_CODE_SUCCESS;
    }
    else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t map_access_server_set_folder_version(uint16_t map_cid, const uint8_t* primary_folder_version) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_access_server_valid_header_for_request(map_access_server)) {
        (void)memcpy(map_access_server->response.folder_version, primary_folder_version, MAS_FOLDER_VERSION_LEN);
        map_access_server->response.folder_version_set = true;
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

    (void)memcpy(map_access_server->response.database_identifier, database_identifier, MAS_FOLDER_VERSION_LEN);
    map_access_server->response.database_identifier_set = true;
    return ERROR_CODE_SUCCESS;
};

static void map_access_server_build_response(map_access_server_t* map_access_server) {
    goep_server_response_create_general(map_access_server->goep_cid);
    map_access_server_add_srm_headers(map_access_server);
    // Application Params
    uint8_t app_params[MAP_SERVER_MAX_APP_PARAMS_LEN];
    uint16_t app_params_pos = 0;
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

uint8_t map_access_server_send_folder_size(uint16_t map_cid, uint8_t response_code, uint16_t folder_size) {
    map_access_server_t* map_access_server = map_access_server_for_map_cid(map_cid);
    if (map_access_server == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // folder size valid for PHONEBOOK and VCARD_LIST query
    if (map_access_server_valid_header_for_request(map_access_server)) {
        map_access_server->response.folder_size = folder_size;
        map_access_server->response.folder_size_set = true;
        map_access_server->response.code = response_code;
        map_access_server->state = MAP_SERVER_STATE_SEND_USER_RESPONSE;
        return goep_server_request_can_send_now(map_access_server->goep_cid);
    }
    else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint16_t map_access_server_send_get_put_response(uint16_t map_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t* body) {
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

uint16_t map_access_server_send_put_response(uint16_t map_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t* body) {
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

// suppress MSVC C4244: unchecked upper bound for enum folder used as index
#ifdef _MSC_VER
#pragma warning( disable : 33011 )
#endif

const char* map_access_server_get_folder_path(mas_folder_t folder) {
    btstack_assert(folder > MAS_FOLDER_MAX);
    btstack_assert(folder < MAS_FOLDER_MAX);
    return map_access_server_folders[(uint16_t)folder].path;
}

const char* map_access_server_get_folder_name(mas_folder_t folder) {
    btstack_assert(folder > MAS_FOLDER_MAX);
    btstack_assert(folder < MAS_FOLDER_MAX);
    return map_access_server_folders[(uint16_t)folder].name;
}
