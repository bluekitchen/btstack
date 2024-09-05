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

#include "bluetooth_sdp.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "hci_event_builder.h"
#include "l2cap.h"

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/obex_message_builder.h"
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
#define MAP_SERVER_MAX_APP_PARAMS_LEN 200
//#define MAP_SERVER_MAX_APP_PARAMS_LEN ((4*2) + 2 + BT_UINT128_LEN_BYTES + (2*BT_UINT128_LEN_BYTES))

typedef enum {
    MAS_STATE_W4_OPEN,
    MAS_STATE_W4_CONNECT_OPCODE,
    MAS_STATE_W4_CONNECT_REQUEST,
    MAS_STATE_SEND_CONNECT_RESPONSE_ERROR,
    MAS_STATE_SEND_CONNECT_RESPONSE_SUCCESS,
    MAS_STATE_CONNECTED,
    MAS_STATE_W4_REQUEST,
    MAS_STATE_W4_USER_DATA,
    MAS_STATE_W4_GET_OPCODE,
    MAS_STATE_W4_GET_REQUEST,
    MAS_STATE_SEND_INTERNAL_RESPONSE,
    MAS_STATE_SEND_RESPONSE_CONTINUE,
    MAS_STATE_SEND_USER_RESPONSE,
    MAS_STATE_SEND_DISCONNECT_RESPONSE,
    MAS_STATE_ABOUT_TO_SEND,
} map_server_state_t;

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

struct {
    bool PUT_continue_as_put;
} tweaks;

static  btstack_packet_handler_t map_server_user_packet_handler;

typedef struct {
// the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
// below generates app_params_compile_time_check struct members to compile-time check all PARAMs
#define PARAM_REQUST(name, tag, type, opts, descr) type PARAM_REQUST ## name;
#define PARAM_RESPON(name, tag, type, opts, descr) type PARAM_RESPON ## name;
#define PARAM_REQRSP(name, tag, type, opts, descr) type PARAM_REQRSP ## name;
#define PARAM_UNUSED(name, tag, type, opts, descr) type PARAM_UNUSED ## name;
#define ENUM(...)
#define DSCR(...)


    APP_PARAMS

#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR

} app_params_compile_time_check;

typedef struct {
    uint8_t entry;
    uint16_t goep_cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    bool     incoming;
    map_server_state_t state;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[MAP_SERVER_MAX_OBEX_HEADER_BUF];
    uint16_t map_supported_features;
    mas_folder_t map_server_dir;
    mas_folder_t  map_folder;
    // SRM
    obex_srm_t  obex_srm;
    uint8_t OBEX_opcode;
    srm_state_t srm_state;
    // request
    struct {
        char name[MAP_SERVER_MAX_NAME_LEN];
        char type[MAP_SERVER_MAX_TYPE_LEN];
        map_object_type_t object_type; // parsed from type string
        struct {
            uint32_t ConnectionID;
        } hdr;
        uint32_t continuation;
        obex_app_param_parser_t app_param_parser;
        uint8_t app_param_buffer[MAP_SERVER_MAX_APP_PARAM_BUFFER];
        struct {
// the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
// below generates request.app_param struct members
#define PARAM_REQUST(name, tag, type, opts, descr) type name;
#define PARAM_REQRSP PARAM_REQUST
#define PARAM_RESPON(...)
#define PARAM_UNUSED(...)
#define ENUM(...)
#define DSCR(...)

            APP_PARAMS

#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR

        } app_params;
    } request;
    // response
    struct {
        uint8_t code;
        char *hdr_name; // pointer to a name string for the response header provided by the application or NULL
        char *hdr_type; // pointer to a type string for the response header provided by the application or NULL
        uint8_t header_data[MAP_SERVER_MAX_APP_PARAMS_LEN];
        uint16_t header_pos;
        bool hdr_finalized; // all header data set?
        uint16_t body_len;
        const uint8_t* body_data;
        bool folder_version_set;
    } response;
} map_server_t;

static map_server_t map_server_connections[MAS_MAX_CONNECTIONS] = {
    [0] = { .entry = 0 },
    [1] = { .entry = 1 },
};

static struct {
    char* name;
    char* msglistingdir;
    char* msglistingsent;
    mas_folder_t dir;
    char* path;
} map_server_folders[] = {
    {"msg",    "incomming"    , "no",  MAS_FOLDER_TELECOM_MSG,        "telecom/msg.vcf"},
    {"inbox",  "incomming"    , "no",  MAS_FOLDER_TELECOM_MSG_INBOX,  "telecom/msg/inbox.vcf"},
    {"outbox", "outgoing"     , "no",  MAS_FOLDER_TELECOM_MSG_OUTBOX, "telecom/msg/outbox.vcf"},
    {"draft",  "outgoingdraft", "no",  MAS_FOLDER_TELECOM_MSG_DRAFT,  "telecom/msg/draft.vcf"},
    {"sent",   "outgoing"     , "yes", MAS_FOLDER_TELECOM_MSG_SENT,   "telecom/msg/sent.vcf"},
};

static const uint8_t map_uuid[] = { 0xbb, 0x58, 0x2b, 0x40, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66 };

// Prototypes
static void map_server_handle_get_or_put_request(map_server_t* mas);
static void map_server_build_response(map_server_t* mas);

static map_server_t* map_server_for_goep_cid(uint16_t goep_cid) {
    int i;

    for (i = 0; i< MAS_MAX_CONNECTIONS; i++)
        if (map_server_connections[i].goep_cid == 0 || map_server_connections[i].goep_cid == goep_cid) {
            log_debug("use map_server_connections[%d] for goep_cid:0x%x", i, goep_cid);
            return &map_server_connections[i];
        }

    log_error("cannot open more than MAS_MAX_CONNECTIONS (%d)", MAS_MAX_CONNECTIONS);
    return NULL;
}

/* only to be called if the GEOP connection is closed
* if only OBEX connection is closed we need to go to  MAS_STATE_W4_CONNECT_OPCODE
*/
static void map_server_finalize_connection(map_server_t* mas) {
    // minimal
    mas->state = MAS_STATE_W4_OPEN;
}

static int get_index_for_path(const char* path) {
    int idx = ARRAYSIZE(map_server_folders) - 1;
    while (idx >= 0 && 0 != strcasecmp(path, map_server_folders[idx].name)) {
        idx--;
    }
    log_debug("path:%s idx:%u msglistingdir:%s", path, idx, map_server_folders[idx].msglistingdir);
    return idx;
}

static mas_folder_t map_server_get_folder_by_path(char* path) {
    int idx = get_index_for_path(path);
    log_debug("path:%s idx:%u dir:%u", path, idx, map_server_folders[idx].dir);
    return map_server_folders[idx].dir;
}

const char* map_server_get_folder_MsgListingDir(char* path) {
    int idx = get_index_for_path(path);
    log_debug("path:%s idx:%u msglistingdir:%u", path, idx, map_server_folders[idx].msglistingdir);
    return map_server_folders[idx].msglistingdir;
}

const char* map_server_get_folder_MsgListingSent(char* path) {
    int idx = get_index_for_path(path);
    log_debug("path:%s idx:%u MsgListingSent:%u", path, idx, map_server_folders[idx].msglistingdir);
    return map_server_folders[idx].msglistingsent;
}


static void map_server_handle_set_path_request(map_server_t* mas, uint8_t flags, const char* name) {
    uint16_t name_len = (uint16_t)strlen(name);
    uint8_t obex_result = OBEX_RESP_SUCCESS;
    if (name_len == 0) {
        // no path name given, one dir up?
        if IS_BIT_SET(flags, OBEX_SP_BIT0_DIR_UP) {
            switch (mas->map_server_dir) {
            case MAS_FOLDER_TELECOM:
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_ROOT;)
                break;
            case MAS_FOLDER_TELECOM_MSG:
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM;)
                break;
            case MAS_FOLDER_TELECOM_MSG_INBOX:
            case MAS_FOLDER_TELECOM_MSG_OUTBOX:
            case MAS_FOLDER_TELECOM_MSG_DRAFT:
            case MAS_FOLDER_TELECOM_MSG_SENT:            
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM_MSG;)
                break;
            default:
                RUN_AND_LOG_ACTION(obex_result = OBEX_RESP_NOT_FOUND;)
                break;
            }
        }
        else {
            RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_ROOT;)
        };
    }
    else {
        switch (mas->map_server_dir) {
        case MAS_FOLDER_ROOT:
            if (strcasecmp("telecom", name) == 0) {
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM;)
            }
            else {
                RUN_AND_LOG_ACTION(obex_result = OBEX_RESP_NOT_FOUND;)
            }
            break;
        case MAS_FOLDER_TELECOM:
            if (strcasecmp("msg", name) == 0) {
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM_MSG;)
            }
            else {
                RUN_AND_LOG_ACTION(obex_result = OBEX_RESP_NOT_FOUND;)
            }
            break;
        case MAS_FOLDER_TELECOM_MSG:
            if      (strcasecmp("inbox", name) == 0) {
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM_MSG_INBOX;)
            } 
            else if (strcasecmp("outbox", name) == 0) {
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM_MSG_OUTBOX;)
            }
            else if (strcasecmp("draft", name) == 0) {
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM_MSG_DRAFT;)
            }
            else if (strcasecmp("sent", name) == 0) {
                RUN_AND_LOG_ACTION(mas->map_server_dir = MAS_FOLDER_TELECOM_MSG_SENT;)
            }
            else {
                RUN_AND_LOG_ACTION(obex_result = OBEX_RESP_NOT_FOUND;)
            }
            break;

        default:
            RUN_AND_LOG_ACTION(obex_result = OBEX_RESP_NOT_FOUND;)
            break;
        }
    }
    mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
    mas->response.code = obex_result;
    goep_server_request_can_send_now(mas->goep_cid);
}

static map_object_type_t map_server_parse_object_type(map_server_t* mas, const char* type_string) {
    
    if (strcmp("x-obex/folder-listing", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_GET_FOLDER_LISTING;)
    }
    
    if (strcmp("x-bt/MAP-msg-listing", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_GET_MSG_LISTING;)
    }

    if (strcmp("x-bt/MAP-convo-listing", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_GET_CONVO_LISTING;)
    }

    if (strcmp("x-bt/MASInstanceInformation", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_GET_MAS_INSTANCE_INFORMATION;)
    }

    if (strcmp("x-bt/messageStatus", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS;)
    }

    if (strcmp("x-bt/MAP-messageUpdate", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_MESSAGE_UPDATE;)
    }

    if (strcmp("x-bt/message", type_string) == 0) {
        if ((mas->OBEX_opcode & OBEX_OPCODE_GET) == OBEX_OPCODE_GET) {
            RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_GET_MESSAGE;)
        }
        else {
            if (mas->OBEX_opcode & OBEX_OPCODE_FINAL_BIT_MASK) {
                RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_MESSAGE;)
            }
            else {
                RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_MESSAGE_CONTINUE;)
            }
        }
    }

    if (strcmp("x-bt/MAP-NotificationRegistration", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_NOTIFICATION_REGISTRATION;)
    }

    if (strcmp("x-bt/MAP-notification-filter", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_SET_NOTIFICATION_FILTER;)
    }

    if (strcmp("x-bt/ownerStatus", type_string) == 0) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_OWNER_STATUS;)
    }
#ifdef MAP_PTS_BUG_TC_MAP_OLD_MAP_MSE_GOEP_SRM_BV_04
    if (mas->OBEX_opcode == (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK)) {
        RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_PUT_EMPTY;)
    }
#endif
    RUN_AND_LOG_ACTION(return MAP_OBJECT_TYPE_UNKNOWN;)
}

static void map_server_obex_srm_init(obex_srm_t* obex_srm) {
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

static void map_server_handle_srm_headers(map_server_t* mas) {
    const obex_srm_t* obex_srm = &mas->obex_srm;
    // Update SRM state based on SRM headers
    switch (mas->srm_state) {
    case SRM_DISABLED:
        if (obex_srm->srm_value == OBEX_SRM_ENABLE) {
            if (obex_srm->srmp_value == OBEX_SRMP_WAIT) {
                RUN_AND_LOG_ACTION(mas->srm_state = SRM_SEND_CONFIRM_WAIT;)
            }
            else {
                RUN_AND_LOG_ACTION(mas->srm_state = SRM_SEND_CONFIRM;)
            }
        }
        break;
    case SRM_ENABLED_WAIT:
        if (obex_srm->srmp_value == OBEX_SRMP_NEXT) {
            RUN_AND_LOG_ACTION(mas->srm_state = SRM_ENABLED;)
        }
        break;
    default:
        break;
    }
}

static void map_server_add_srm_headers(map_server_t* mas) {
    switch (mas->srm_state) {
    case SRM_SEND_CONFIRM:
        goep_server_header_add_srm_enable(mas->goep_cid);
        RUN_AND_LOG_ACTION(mas->srm_state = SRM_ENABLED;)
        break;
    case SRM_SEND_CONFIRM_WAIT:
        goep_server_header_add_srm_enable_wait(mas->goep_cid);
        RUN_AND_LOG_ACTION(mas->srm_state = SRM_ENABLED_WAIT;)
        break;
    default:
        break;
    }
}

static void map_server_add_name_header(map_server_t* mas) {

    if (mas->response.hdr_name != NULL) {

        goep_server_header_add_name(mas->goep_cid, mas->response.hdr_name);
    }
}

static void map_server_add_type_header(map_server_t* mas) {

    if (mas->response.hdr_type != NULL) {

        goep_server_header_add_type(mas->goep_cid, mas->response.hdr_type);
    }
}

static void map_server_default_headers(map_server_t* mas) {
    (void)memset(&mas->request, 0, sizeof(mas->request));
    mas->request.app_params.MaxListCount = 1024;
    log_debug("set default headers for request");
}
static void map_server_reset_response(map_server_t* mas) {
    (void)memset(&mas->response, 0, sizeof(mas->response));
}

static void map_server_operation_complete(map_server_t* mas) {
    RUN_AND_LOG_ACTION(mas->state = MAS_STATE_CONNECTED;)
    RUN_AND_LOG_ACTION(mas->srm_state = SRM_DISABLED;)
    map_server_default_headers(mas);
    map_server_reset_response(mas);
}

static void map_server_handle_can_send_now(map_server_t* mas) {
	// save mas->response.code before its reset by map_server_operation_complete()
    uint8_t response_code = mas->response.code;
    uint8_t event[30];
    uint16_t pos = 0;
    hci_event_builder_context_t context;
    switch (mas->state) {
    case MAS_STATE_SEND_INTERNAL_RESPONSE:
        log_debug("MAS_STATE_SEND_INTERNAL_RESPONSE");
        // prepare response
        goep_server_response_create_general(mas->goep_cid);
        map_server_add_srm_headers(mas);
        // next state
        map_server_operation_complete(mas);
        // send packet
        goep_server_execute(mas->goep_cid, response_code);
        break;
    case MAS_STATE_SEND_RESPONSE_CONTINUE:
        log_debug("MAS_STATE_SEND_RESPONSE_CONTINUE");
        // prepare response
        goep_server_response_create_general(mas->goep_cid);
        map_server_add_srm_headers(mas);
        // next state
        mas->state = MAS_STATE_CONNECTED;
        // send packet
        goep_server_execute(mas->goep_cid, response_code);
        break;
    case MAS_STATE_SEND_USER_RESPONSE:
        log_debug("MAS_STATE_SEND_USER_RESPONSE");
        // prepare response
        map_server_build_response(mas);
        if (mas->response.body_len > 0) {
            goep_server_header_add_end_of_body(mas->goep_cid,
                mas->response.body_data,
                mas->response.body_len);
        }
        // next state
        if (response_code == OBEX_RESP_CONTINUE) {
            map_server_reset_response(mas);
            // next state
            if (mas->srm_state == SRM_ENABLED) {
                RUN_AND_LOG_ACTION(mas->state = MAS_STATE_ABOUT_TO_SEND;)
            }
            else {
                RUN_AND_LOG_ACTION(mas->state = MAS_STATE_W4_GET_OPCODE;)
            }
        }
        else {
            map_server_operation_complete(mas);
        }
        // send packet
        goep_server_execute(mas->goep_cid, response_code);
        // trigger next user response in SRM
        if (mas->srm_state == SRM_ENABLED) {
            map_server_handle_get_or_put_request(mas);
        }
        break;
    case MAS_STATE_SEND_CONNECT_RESPONSE_ERROR:
        log_debug("MAS_STATE_SEND_CONNECT_RESPONSE_ERROR");
        // prepare response
        goep_server_response_create_general(mas->goep_cid);
        // next state
        RUN_AND_LOG_ACTION(mas->state = MAS_STATE_W4_CONNECT_OPCODE;)
        // send packet
        goep_server_execute(mas->goep_cid, OBEX_RESP_BAD_REQUEST);
        break;
    case MAS_STATE_SEND_CONNECT_RESPONSE_SUCCESS:
        log_debug("MAS_STATE_SEND_CONNECT_RESPONSE_SUCCESS");
        // prepare response
        goep_server_response_create_connect(mas->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
        goep_server_header_add_who(mas->goep_cid, map_uuid);
        // next state
        mas->map_server_dir = MAS_FOLDER_ROOT;
        mas->map_folder = MAS_FOLDER_INVALID;
        map_server_operation_complete(mas);
        // send packet
        goep_server_execute(mas->goep_cid, OBEX_RESP_SUCCESS);

        // emit event
        hci_event_builder_init(&context, event, sizeof(event), HCI_EVENT_MAP_META, MAP_SUBEVENT_CONNECTION_OPENED);
        hci_event_builder_add_16(&context, mas->goep_cid);
        hci_event_builder_add_08(&context, ERROR_CODE_SUCCESS);
        hci_event_builder_add_bd_addr(&context, mas->bd_addr);
        hci_event_builder_add_con_handle(&context, mas->con_handle);
        hci_event_builder_add_08(&context, 1);
        (*map_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, hci_event_builder_get_length(&context));
        break;

    case MAS_STATE_SEND_DISCONNECT_RESPONSE:
    {
        log_debug("MAS_STATE_SEND_DISCONNECT_RESPONSE");
        // cache data
        //uint16_t map_cid = mas->goep_cid;
        uint16_t goep_cid = mas->goep_cid;

        // reset MAP/OBEX connection state
        RUN_AND_LOG_ACTION(mas->state = MAS_STATE_W4_CONNECT_OPCODE;)

        // respond to request
        goep_server_response_create_general(goep_cid);
        goep_server_execute(goep_cid, OBEX_RESP_SUCCESS);

        // emit event
        APP_WRITE_08(event, &pos, HCI_EVENT_MAP_META);
        APP_WRITE_08(event, &pos, 0);
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_CONNECTION_CLOSED);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_LEN(event, pos);
        (*map_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
        break;
    }
    default:
        break;
    }
}

static void map_server_packet_handler_hci(uint8_t* packet, uint16_t size) {
    UNUSED(size);
    uint8_t status;
    map_server_t* mas;
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
            mas = map_server_for_goep_cid(goep_cid);
            btstack_assert(mas != NULL);
            status = goep_subevent_connection_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) {
                // TODO: report failed incomming connection
                log_error("TODO: report failed outgoing connection");
            }
            else {
                mas->goep_cid = goep_cid;
                goep_subevent_connection_opened_get_bd_addr(packet, mas->bd_addr);
                mas->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                mas->incoming = goep_subevent_connection_opened_get_incoming(packet);
                mas->state = MAS_STATE_W4_CONNECT_OPCODE;
            }
            break;
        case GOEP_SUBEVENT_CONNECTION_CLOSED:
            goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
            mas = map_server_for_goep_cid(goep_cid);
            log_debug("free up mas slot goep_cid <%u>(0x%04x) for another connect", goep_cid, goep_cid);
            mas->goep_cid = 0;
            btstack_assert(mas != NULL);
            break;
        case GOEP_SUBEVENT_CAN_SEND_NOW:
            goep_cid = goep_subevent_can_send_now_get_goep_cid(packet);
            mas = map_server_for_goep_cid(goep_cid);
            btstack_assert(mas != NULL);
            map_server_handle_can_send_now(mas);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}
static void map_server_app_param_callback_connect(void* user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t* data_buffer, uint8_t data_len) {
    map_server_t* mas = (map_server_t*)user_data;
    if (tag_id == MAP_APP_PARAM_MapSupportedFeatures) {
        obex_app_param_parser_tag_state_t state = obex_app_param_parser_tag_store(mas->request.app_param_buffer, sizeof(mas->request.app_param_buffer), total_len, data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE) {
            mas->map_supported_features = big_endian_read_32(mas->request.app_param_buffer, 0);
        }
    }
}

static void map_server_parser_callback_connect(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    UNUSED(total_len);
    UNUSED(data_offset);

    map_server_t* mas = (map_server_t*)user_data;

    switch (header_id) {
    case OBEX_HEADER_TARGET:
        // TODO: verify target
        break;
    case OBEX_HEADER_APPLICATION_PARAMETERS:
        if (data_offset == 0) {
            obex_app_param_parser_init(&mas->request.app_param_parser,
                &map_server_app_param_callback_connect, total_len, mas);
        }
        obex_app_param_parser_process_data(&mas->request.app_param_parser, data_buffer, data_len);
        break;
    default:
        break;
    }
}

static void map_server_app_param_callback_get(void* user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t* data_buffer, uint8_t data_len) {
    map_server_t* mas = (map_server_t*)user_data;
    obex_app_param_parser_tag_state_t state;
    uint16_t pos = data_offset;
    switch (tag_id) {

    default:
        state = obex_app_param_parser_tag_store(mas->request.app_param_buffer,
            sizeof(mas->request.app_param_buffer), total_len,
            data_offset, data_buffer, data_len);
        if (state == OBEX_APP_PARAM_PARSER_TAG_COMPLETE) {
            switch (tag_id) {

// the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
// automagically generates GETers for all APP Params in the form of:
//case MAP_APPLICATION_PARAMETER_MAX_LIST_COUNT:
//    mas->request.app_params.MaxListCount = big_endian_read_16(
//        mas->request.app_param_buffer, 0);
//    break;

#define PARAM_REQUST(name, tag, type, opts, descr) \
            case MAP_APP_PARAM_ ## name: \
                    app_param_read_ ## type (mas->request.app_param_buffer, &pos, &mas->request.app_params. name, sizeof(type)); \
                break;

#define PARAM_REQRSP PARAM_REQUST
#define PARAM_RESPON(...)
#define PARAM_UNUSED(...)
#define ENUM(...)
#define DSCR(...)

                APP_PARAMS

#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR

            default:
                break;
            }
        }
        break;
    }
}

static void map_server_parser_callback_get(void* user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t* data_buffer, uint16_t data_len) {
    map_server_t* mas = (map_server_t*)user_data;

    switch (header_id) {
    case OBEX_HEADER_SINGLE_RESPONSE_MODE:
        obex_parser_header_store(&mas->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
        break;
    case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
        obex_parser_header_store(&mas->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
        break;
    case OBEX_HEADER_CONNECTION_ID:
        log_debug("OBEX_HEADER_CONNECTION_ID: total_len:%u, data_offset:%u, data_len:%u", total_len, data_offset, data_len);
        if (obex_parser_header_store(mas->obex_header_buffer, sizeof(mas->obex_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE) {
            mas->request.hdr.ConnectionID = big_endian_read_32(mas->obex_header_buffer, 0);
            log_info("OBEX_HEADER_CONNECTION_ID:%x", mas->request.hdr.ConnectionID);
        }
        break;
    case OBEX_HEADER_NAME:
        // name is stored in big endian unicode-16
        if (total_len < (MAP_SERVER_MAX_NAME_LEN * 2)) {
            uint16_t i;
            for (i = 0; i < data_len; i++) {
                if (((data_offset + i) & 1) == 1) {
                    mas->request.name[(data_offset + i) >> 1] = data_buffer[i];
                }
            }
            mas->request.name[total_len / 2] = 0;
            log_debug("OBEX_HEADER_NAME:<%.*s>", data_len/2, mas->request.name);
        }
        break;
    case OBEX_HEADER_TYPE:
        if (total_len < MAP_SERVER_MAX_TYPE_LEN) {
            memcpy(&mas->request.type[data_offset], data_buffer, data_len);
            mas->request.type[total_len] = 0;
        }
        else
            log_error("(total_len < MAP_SERVER_MAX_TYPE_LEN) failed: <%s>", data_buffer);
        break;
    case OBEX_HEADER_APPLICATION_PARAMETERS:
        if (data_offset == 0) {
            obex_app_param_parser_init(&mas->request.app_param_parser,
                &map_server_app_param_callback_get, total_len, mas);
        }
        obex_app_param_parser_process_data(&mas->request.app_param_parser, data_buffer, data_len);
        break;
    default:
        break;
    }
}



// sends MAP_SUBEVENT_xyz messages to the application using serialized stack-internal app-parameters
static void map_server_handle_get_or_put_request(map_server_t* mas) {
    map_server_handle_srm_headers(mas);

    if (mas->srm_state == SRM_SEND_CONFIRM_WAIT) {
        RUN_AND_LOG_ACTION(mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;)
        RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_CONTINUE;)
        RUN_AND_LOG_ACTION(mas->obex_srm.srm_value = OBEX_SRM_ENABLE;)
        RUN_AND_LOG_ACTION(mas->srm_state = SRM_ENABLED_WAIT;)
        goep_server_request_can_send_now(mas->goep_cid);
        return;
    }

    mas->request.object_type = map_server_parse_object_type(mas, mas->request.type);
    mas_folder_t folder = mas->map_server_dir;
    //uint16_t name_len = (uint16_t)strlen(mas->request.name);
    switch (mas->request.object_type) {

    default:
    case MAP_OBJECT_TYPE_UNKNOWN:
    case MAP_OBJECT_TYPE_INVALID:
        mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
        RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_BAD_REQUEST;)
        goep_server_request_can_send_now(mas->goep_cid);
        return;
#ifdef MAP_PTS_BUG_TC_MAP_OLD_MAP_MSE_GOEP_SRM_BV_04
    case MAP_OBJECT_TYPE_PUT_EMPTY:
    {
        static uint8_t respcode[] = { 0xA0, 0xC0, 0xC1, 0xC3, 0xC4, 0xC6, 0xCF };
        static idx = 0;

        mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
        RUN_AND_LOG_ACTION(mas->response.code = respcode[idx++];)
        if (idx >= ARRAYSIZE(respcode))
            idx = 0;
        goep_server_request_can_send_now(mas->goep_cid);
        return;
    }
#endif

    case MAP_OBJECT_TYPE_GET_MSG_LISTING:
        folder = map_server_get_folder_by_path(mas->request.name);
        break;

    case MAP_OBJECT_TYPE_PUT_MESSAGE_CONTINUE:
#ifndef MAP_PTS_BUG_TC_MAP_OLD_MAP_MSE_GOEP_SRM_BV_04_PASS
            mas->state = MAS_STATE_SEND_RESPONSE_CONTINUE;
            RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_CONTINUE;)
            goep_server_request_can_send_now(mas->goep_cid);
            return;
#else
            mas->request.object_type = MAP_OBJECT_TYPE_PUT_MESSAGE;
            // fall trough to MAP_OBJECT_TYPE_PUT_MESSAGE
#endif


    case MAP_OBJECT_TYPE_GET_MESSAGE:
    case MAP_OBJECT_TYPE_GET_FOLDER_LISTING:
    case MAP_OBJECT_TYPE_GET_CONVO_LISTING:
    case MAP_OBJECT_TYPE_GET_MAS_INSTANCE_INFORMATION:
    case MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS:
    case MAP_OBJECT_TYPE_PUT_MESSAGE_UPDATE:
    case MAP_OBJECT_TYPE_PUT_MESSAGE:
    case MAP_OBJECT_TYPE_PUT_NOTIFICATION_REGISTRATION:
    case MAP_OBJECT_TYPE_PUT_SET_NOTIFICATION_FILTER:
    case MAP_OBJECT_TYPE_PUT_OWNER_STATUS:
        break;
    }

    if (folder == MAS_FOLDER_INVALID) {
        mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
        RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_NOT_FOUND;)
        goep_server_request_can_send_now(mas->goep_cid);
        return;
    }

    // emit get ( folder, msg_listing, msg)
    uint8_t event[2 + 20 + MAP_SERVER_MAX_NAME_LEN + MAP_SERVER_MAX_SEARCH_VALUE_LEN];
    uint16_t pos = 0;
    APP_WRITE_08(event, &pos, HCI_EVENT_MAP_META);
    pos = 2; // skip size header, its written at the end
    switch (mas->request.object_type) {

    case MAP_OBJECT_TYPE_GET_FOLDER_LISTING:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_FOLDER_LISTING_ITEM);
        APP_WRITE_32(event, &pos, mas->request.continuation);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_16(event, &pos, mas->request.app_params.MaxListCount);
        APP_WRITE_16(event, &pos, mas->request.app_params.ListStartOffset);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_GET_MSG_LISTING:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_GET_MESSAGE_LISTING);
        APP_WRITE_32(event, &pos, mas->request.continuation);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_16(event, &pos, mas->request.app_params.MaxListCount);
        APP_WRITE_16(event, &pos, mas->request.app_params.ListStartOffset);
        APP_WRITE_STR(event, &pos, sizeof(event) - pos, mas->request.name);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_GET_CONVO_LISTING:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_GET_CONVO_LISTING);
        APP_WRITE_32(event, &pos, mas->request.continuation);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_16(event, &pos, mas->request.app_params.MaxListCount);
        APP_WRITE_STR(event, &pos, sizeof(mas->request.app_params.FilterPeriodBegin), (char*)mas->request.app_params.FilterPeriodBegin);
        APP_WRITE_STR(event, &pos, sizeof(mas->request.app_params.EndFilterPeriodEnd), (char*)mas->request.app_params.EndFilterPeriodEnd);
        APP_WRITE_STR(event, &pos, sizeof(mas->request.app_params.EndFilterPeriodEnd), (char*)mas->request.app_params.FilterRecipient);
        APP_WRITE_STR(event, &pos, sizeof(mas->request.app_params.ConversationID),(char*)mas->request.app_params.ConversationID);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_GET_MAS_INSTANCE_INFORMATION:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_GET_MAS_INSTANCE_INFORMATION);
        APP_WRITE_32(event, &pos, mas->request.continuation);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_08(event, &pos, mas->request.app_params.MASInstanceID)
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_GET_MESSAGE:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_GET_MESSAGE);
        APP_WRITE_32(event, &pos, mas->request.continuation);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_STR(event, &pos, sizeof(event) - pos, mas->request.name);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_PUT_MESSAGE_STATUS);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_08(event, &pos, mas->request.app_params.StatusIndicator);
        APP_WRITE_08(event, &pos, mas->request.app_params.StatusValue);
        APP_WRITE_STR(event, &pos, sizeof(event) - pos, mas->request.name);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_PUT_MESSAGE:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_PUT_MESSAGE);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_08(event, &pos, mas->OBEX_opcode);
        APP_WRITE_08(event, &pos, mas->request.app_params.Charset);
        APP_WRITE_08(event, &pos, mas->request.app_params.Attachment);
        APP_WRITE_08(event, &pos, mas->request.app_params.ModifyText);
        APP_WRITE_STR(event, &pos, sizeof(mas->request.app_params.MessageHandle), (char*)mas->request.app_params.MessageHandle);
        APP_WRITE_STR(event, &pos, sizeof(event) - pos, mas->request.name);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_PUT_MESSAGE_UPDATE:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_PUT_MESSAGE_UPDATE);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_PUT_NOTIFICATION_REGISTRATION:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_PUT_SET_NOTIFICATION_REGISTRATION);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_32(event, &pos, mas->request.hdr.ConnectionID);
        APP_WRITE_08(event, &pos, mas->request.app_params.NotificationStatus);
        APP_WRITE_LEN(event, pos);
        break;
		

    case MAP_OBJECT_TYPE_PUT_SET_NOTIFICATION_FILTER:
        APP_WRITE_08(event, &pos, MAP_SUBEVENT_PUT_SET_NOTIFICATION_FILTER);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_32(event, &pos, mas->request.app_params.NotificationFilterMask);
        APP_WRITE_LEN(event, pos);
        break;

    case MAP_OBJECT_TYPE_PUT_OWNER_STATUS:
        APP_WRITE_08(event, &pos, MAP_OBJECT_TYPE_PUT_OWNER_STATUS);
        APP_WRITE_16(event, &pos, mas->goep_cid);
        APP_WRITE_08(event, &pos, mas->request.app_params.ChatState);
        APP_WRITE_STR(event, &pos, sizeof(event) - pos, (char*)mas->request.app_params.LastActivity);
        APP_WRITE_LEN(event, pos);
        break;

    default:
        btstack_unreachable();
        break;
    }
    mas->state = MAS_STATE_W4_USER_DATA;
    (*map_server_user_packet_handler)(HCI_EVENT_PACKET, 0, event, pos);
}

static void map_server_packet_handler_goep(map_server_t* mas, uint8_t* packet, uint16_t size) {
    btstack_assert(size > 0);
    uint8_t opcode;
    obex_parser_object_state_t parser_state;

    switch (mas->state) {
    case MAS_STATE_W4_OPEN:
        btstack_unreachable();
        break;
    case MAS_STATE_W4_CONNECT_OPCODE:
        mas->state = MAS_STATE_W4_CONNECT_REQUEST;
        map_server_default_headers(mas);
        obex_parser_init_for_request(&mas->obex_parser, &map_server_parser_callback_connect, (void*)mas);

        /* fall through */

    case MAS_STATE_W4_CONNECT_REQUEST:
        parser_state = obex_parser_process_data(&mas->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&mas->obex_parser, &op_info);
            bool ok = true;
            // check opcode
            if (op_info.opcode != OBEX_OPCODE_CONNECT) {
                ok = false;
            }
            // TODO: check Target
            if (ok == false) {
                // send bad request response
                mas->state = MAS_STATE_SEND_CONNECT_RESPONSE_ERROR;
            }
            else {
                // send connect response
                mas->state = MAS_STATE_SEND_CONNECT_RESPONSE_SUCCESS;
            }
            goep_server_request_can_send_now(mas->goep_cid);
        }
        break;
    case MAS_STATE_CONNECTED:
        opcode = packet[0];
        mas->OBEX_opcode = opcode;

        // default headers
        switch (opcode) {
        case OBEX_OPCODE_GET:
        case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
        case OBEX_OPCODE_PUT:
        case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
            log_debug("MAS_STATE_W4_REQUEST: OBEX_OPCODE_GET/PUT opcode:0x%02X", opcode);
            mas->state = MAS_STATE_W4_REQUEST;
            obex_parser_init_for_request(&mas->obex_parser, &map_server_parser_callback_get, (void*)mas);
            break;
        case OBEX_OPCODE_SETPATH:
            log_debug("MAS_STATE_W4_REQUEST: OBEX_OPCODE_SETPATH opcode:0x%02X", opcode);
            mas->state = MAS_STATE_W4_REQUEST;
            obex_parser_init_for_request(&mas->obex_parser, &map_server_parser_callback_get, (void*)mas);
            break;
        case OBEX_OPCODE_DISCONNECT:
            log_debug("MAS_STATE_W4_REQUEST: OBEX_OPCODE_DISCONNECT opcode:0x%02X", opcode);
            mas->state = MAS_STATE_W4_REQUEST;
            obex_parser_init_for_request(&mas->obex_parser, NULL, NULL);
            break;
        case OBEX_OPCODE_ACTION:
        default:
            log_debug("MAS_STATE_W4_REQUEST: OBEX_OPCODE_ACTION/default opcode:0x%02X", opcode);
            mas->state = MAS_STATE_W4_REQUEST;
            obex_parser_init_for_request(&mas->obex_parser, NULL, NULL);
            break;
        }

        /* fall through */

    case MAS_STATE_W4_REQUEST:
        map_server_obex_srm_init(&mas->obex_srm);
        parser_state = obex_parser_process_data(&mas->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&mas->obex_parser, &op_info);
            switch (op_info.opcode) {
            case OBEX_OPCODE_GET:
            case (OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK):
            case OBEX_OPCODE_PUT:
            case (OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK):
                map_server_handle_get_or_put_request(mas);
                break;
            case OBEX_OPCODE_SETPATH:
                map_server_handle_set_path_request(mas, op_info.flags, &mas->request.name[0]);
                break;
            case OBEX_OPCODE_DISCONNECT:
                mas->state = MAS_STATE_SEND_DISCONNECT_RESPONSE;
                goep_server_request_can_send_now(mas->goep_cid);
                break;
            case OBEX_OPCODE_ACTION:
            case (OBEX_OPCODE_ACTION | OBEX_OPCODE_FINAL_BIT_MASK):
            case OBEX_OPCODE_SESSION:
            default:
                // send bad request response
                mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
                RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_BAD_REQUEST;)
                goep_server_request_can_send_now(mas->goep_cid);
                break;
            }
        }
        break;

    case MAS_STATE_W4_GET_OPCODE:
        mas->state = MAS_STATE_W4_GET_REQUEST;
        obex_parser_init_for_request(&mas->obex_parser, &map_server_parser_callback_get, (void*)mas);

        /* fall through */

    case MAS_STATE_W4_GET_REQUEST:
        map_server_obex_srm_init(&mas->obex_srm);
        parser_state = obex_parser_process_data(&mas->obex_parser, packet, size);
        if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE) {
            obex_parser_operation_info_t op_info;
            obex_parser_get_operation_info(&mas->obex_parser, &op_info);
            switch ((op_info.opcode & 0x7f)) {
            case OBEX_OPCODE_GET:
                map_server_handle_get_or_put_request(mas);
                break;
            case (OBEX_OPCODE_ABORT & 0x7f):
                mas->response.code = OBEX_RESP_SUCCESS;
                mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
                goep_server_request_can_send_now(mas->goep_cid);
                break;
            default:
                // send bad request response
                mas->state = MAS_STATE_SEND_INTERNAL_RESPONSE;
                RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_BAD_REQUEST;)
                goep_server_request_can_send_now(mas->goep_cid);
                break;
            }
        }
        break;
    default:
        break;
    }
}

static void map_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events
    map_server_t* mas;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        map_server_packet_handler_hci(packet, size);
        break;
    case GOEP_DATA_PACKET:
        mas = map_server_for_goep_cid(channel);
        btstack_assert(mas != NULL);
        map_server_packet_handler_goep(mas, packet, size);
        break;
    default:
        break;
    }
}

void map_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, uint16_t mtu) {
    //maximum_obex_packet_length = mtu;
    goep_server_register_service(&map_server_packet_handler, rfcomm_channel_nr, 0xFFFF, l2cap_psm, 0xFFFF, LEVEL_0);

    map_server_user_packet_handler = packet_handler;
}

void map_server_deinit(void) {
}

// note: common code for next four setters
static bool map_server_valid_header_for_request(map_server_t* mas) {
    if (mas->state != MAS_STATE_W4_USER_DATA) {
        RUN_AND_LOG_ACTION(return false;)
    }
    switch (mas->request.object_type) {
    case MAP_OBJECT_TYPE_GET_MSG_LISTING:
    case MAP_OBJECT_TYPE_GET_CONVO_LISTING:
    case MAP_OBJECT_TYPE_GET_MAS_INSTANCE_INFORMATION:
        return true;
    default:
        RUN_AND_LOG_ACTION(return false;)
    }

}


int map_server_set_response_app_param(uint16_t map_cid, enum MAP_APP_PARAMS app_param, void* param) {
    map_server_t* mas = map_server_for_goep_cid(map_cid);
    size_t len;

    if (mas == NULL)
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;

    if (!map_server_valid_header_for_request(mas))
        RUN_AND_LOG_ACTION(return ERROR_CODE_COMMAND_DISALLOWED;)
    
    switch (app_param) {


// the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
// automagically generates SETers for all APP Params
#define PARAM_RESPON(name, tag, type, opts, descr) \
    case MAP_APP_PARAM_ ## name: \
        len = (MAP_APP_PARAMS_OPTIONS_ ## name & OPT_STR0) ? strlen(param):sizeof(type); \
        /* Type: 1 Byte  */BT_APP_PARAM_WRITE_08(mas->response.header_data, &mas->response.header_pos, MAP_APP_PARAM_ ## name, 1); \
        /* Size: 1 Byte  */BT_APP_PARAM_WRITE_08(mas->response.header_data, &mas->response.header_pos, (uint8_t)len, 1); \
        /* Data: N Bytes */app_param_write_ ## type (mas->response.header_data, &mas->response.header_pos, *((type*)param), (uint16_t) len); \
        return ERROR_CODE_SUCCESS;

#define PARAM_REQRSP PARAM_RESPON
#define PARAM_REQUST(...)
#define PARAM_UNUSED(...)
#define ENUM(...)
#define DSCR(...)

        APP_PARAMS

#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR


    default:
        btstack_unreachable();
        return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        break;
    } // end of switch
}

static void map_server_build_response(map_server_t* mas) {
    goep_server_response_create_general(mas->goep_cid);
    map_server_add_srm_headers(mas);
    map_server_add_name_header(mas);
    map_server_add_type_header(mas);

    // Application Params already in mas->response.header_data
    if (mas->response.header_pos > 0)
        goep_server_header_add_application_parameters(mas->goep_cid, mas->response.header_data, mas->response.header_pos);
}

#define OBEX_SIZE_HDR 3
#define OBEX_SIZE_SRM 2
#define OBEX_SIZE_SRM 2

// TODO: currently a lot of heuristics/hard coded to determine the available body space - should reflect the real packet created later
uint16_t map_server_get_max_body_size(uint16_t map_cid) {
    map_server_t* mas = map_server_for_goep_cid(map_cid);

    btstack_assert(mas->request.object_type != MAP_OBJECT_TYPE_INVALID);
    btstack_assert(mas->response.hdr_finalized);

    if (mas == NULL) {
        return 0;
    }
    if (mas->state != MAS_STATE_W4_USER_DATA) {
        return 0;
    }

    uint16_t hdr_name_len = obex_message_builder_get_header_name_len_from_strlen(mas->response.hdr_name ? (uint16_t)strlen(mas->response.hdr_name) : 0);
    uint16_t hdr_type_len = obex_message_builder_get_header_type_len(mas->response.hdr_type);
    
    uint16_t goep_max_message_size = goep_server_response_get_max_message_size(mas->goep_cid);
    log_debug("goep_max_message_size:%u response.header_pos:%u hdr_name:%u, hdr_type:%u", goep_max_message_size, mas->response.header_pos, hdr_name_len, hdr_type_len);
    // calc max body size without reserving outgoing buffer: packet size - OBEX Header (3) - SRM Header (2) - Body Header (3) - Unknown additional 3 bytes???
    return goep_max_message_size - 3 - 2 - 3 - 3 - mas->response.header_pos - hdr_name_len - hdr_type_len;
}

void map_server_set_response_type_and_name(uint16_t map_cid, char* hdr_name, char* type_name) {
    map_server_t* mas = map_server_for_goep_cid(map_cid);
    if (mas == NULL) {
        RUN_AND_LOG_ACTION(return;)
    }

    mas->response.hdr_name = hdr_name;
    mas->response.hdr_type = type_name;
    mas->response.hdr_finalized = true;
}

uint16_t map_server_send_response(uint16_t map_cid, uint8_t response_code, uint32_t continuation, size_t body_len, const uint8_t* body) {
    map_server_t* mas = map_server_for_goep_cid(map_cid);

    if (mas == NULL) {
        RUN_AND_LOG_ACTION(return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;)
    }

    log_debug("map_cid:%u, response_code:0x%02x, hdr_name:%s, continuation:%u, body_len:%u, body [%s]",
        map_cid, response_code, mas->response.hdr_name, continuation, body_len, body);

    // double check size

    // calc max body size without reserving outgoing buffer: packet size - OBEX Header (3) - SRM Header (2) - Body Header (3)
    uint16_t max_body_size = map_server_get_max_body_size(mas->goep_cid);
    if (body_len > max_body_size) {
        RUN_AND_LOG_ACTION(return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;)
    }
#if 0 // MAP/MSE/GOEP/SRMP/BV-03-C this one gets PTS to send a second PUT (more packets to follow)
    RUN_AND_LOG_ACTION(mas->srm_state = SRM_SEND_CONFIRM_WAIT;)
    RUN_AND_LOG_ACTION(mas->response.code = OBEX_RESP_CONTINUE;)
#else
    // set data for response and trigger execute
    mas->response.code = response_code;
#endif

    mas->request.continuation = continuation;
    mas->state = MAS_STATE_SEND_USER_RESPONSE;
    mas->response.body_data = body;
    mas->response.body_len = (uint16_t)body_len;
    RUN_AND_LOG_ACTION(return goep_server_request_can_send_now(mas->goep_cid);)
}

// suppress MSVC C4244: unchecked upper bound for enum folder used as index
#ifdef _MSC_VER
#pragma warning( disable : 33011 )
#endif



