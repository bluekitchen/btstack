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

#define PRINTF_TO_PKTLOG
#ifdef PRINTF_TO_PKTLOG
#define MAP_PRINTF(format, ...)  printf(format,  ## __VA_ARGS__); HCI_DUMP_LOG("PRINTF", HCI_DUMP_LOG_LEVEL_INFO, format,  ## __VA_ARGS__)
#else
#define MAP_PRINTF MAP_PRINTF
#endif

// MAP 1.4.2, 6.3 - OBEX Header: map notification service bb582b41-420c-11db-b0de-0800200c9a66
static const uint8_t map_notification_client_service_uuid[] = { 0xbb, 0x58, 0x2b, 0x41, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66 };

static btstack_linked_list_t map_notification_clients;

struct objconfig_s {
    char* header;
    char* body;
    char* footer;
};

// copied from PTS v8.6.0B6 file EMAIL_NewMessage_event_report_1_0
struct objconfig_s nm_v1_0 = {
    .header = "<MAP-event-report version=\"1.0\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type=\"NewMessage\" handle=\"0123456789000003\" folder=\"TELECOM/MSG/INBOX\" msg_type=\"%s\" read_status=\"yes\" acknowledged_status=\"no\"/>"
};

// copied from PTS v8.6.0B6 file GSM_NewMessage_event_report_1_1
struct objconfig_s nm_v1_1 = {
    .header = "<MAP-event-report version=\"1.1\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type = \"NewMessage\" handle=\"0123456789000001\" folder=\"TELECOM/MSG/OUTBOX\" msg_type=\"%s\" subject=\"Subject\" datetime=\"20130121T130510\" sender_name=\"Xyz\" priority=\"no\"/>"
};

// copied and adapted from PTS v8.6.0B6 file GSM_NewMessage_event_report_1_1
struct objconfig_s nm_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body = "<event type = \"NewMessage\" handle=\"0123456789000001\" folder=\"TELECOM/MSG/OUTBOX\" msg_type=\"%s\" subject=\"Subject\" datetime=\"20130121T130510\" sender_name=\"Xyz\" priority=\"no\"/>"
};

// copied from PTS v8.6.0B6 file ExtendedDataChanged_event_report_1_2
struct objconfig_s ed_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body = "<event type=\"MessageExtendedDataChanged\" msg_type=\"%s\" handle=\"0123456789000002\" folder=\"TELECOM/MSG/INBOX\" sender_name=\"PTS\""
            " datatime=\"20160925T133700\" extended_data=\"0:54;\""
            " conversation_id=\"3909231965\""
            " participant_uci=\"skype:beastmode2\""
            "/>"
};

// copied from PTS v8.6.0B6 file ParticipantPresenceChanged_event_report_1_2
struct objconfig_s pp_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type = \"ParticipantPresenceChanged\" sender_name=\"PTS\" conversation_id=”3909231965” presence_availability=\"1\" last_activity=\"20160625T133700\" participant_uci=”skype:beastmode2”/>"
};

// copied from PTS v8.6.0B6 file ParticipantChatStateChanged_event_report_1_2
struct objconfig_s pc_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type = \"ParticipantChatStateChanged\" sender_name=\"PTS\" conversation_id=”3909231965” last_activity=\"20160625T133700\" chat_state=\"1\" participant_uci=”skype:beastmode2”/>"
};

// copied from PTS v8.6.0B6 file ConvrsationChanged_event_report_1_2
struct objconfig_s cc_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type=\"ConversationChanged\" msg_type=\"IM\" sender_name=\"PTS\""
              " conversation_id=”3909231965”  presence_availability=\"1\"  chat_state=\"1\" last_activity=\"20160625T133700\""
              " participant_uci=”skype:beastmode2”/>"
};

// copied from PTS v8.6.0B6 file IM_MessageRemoved_event_report_1_2
struct objconfig_s mr_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type = \"MessageRemoved\" handle=\"0123456789001000\" folder=\"TELECOM/MSG/INBOX\"  msg_type=\"IM\"/>"
};


#define MAX_TC_OBJECTS 10 // maximum MAX_TC_OBJECTS-1 entries, last one is null
static struct test_config_s
{
    int nr;
    char* descr;
    struct objconfig_s* type;
    int obj_count;
    char* msg_types[MAX_TC_OBJECTS]; 
    enum msg_status_read msg_stati[MAX_TC_OBJECTS]; 
    bool msg_deleted[MAX_TC_OBJECTS];
} test_configs[] =
{
    {.nr = 0, .descr = "MAP/MSE/MMN/BV-02-C"    , .type = &nm_v1_0   ,.obj_count = 1, .msg_types = { "EMAIL", "SMS_GSM", "SMS_CDMA", "MMS", "IM"},},
    {.nr = 1, .descr = "MAP/MSE/MMN/BV-04-C 06" , .type = &nm_v1_1   ,.obj_count = 1, .msg_types = { "EMAIL", "SMS_GSM", "SMS_CDMA", "MMS", "IM"},},
    {.nr = 2, .descr = "MAP/MSE/MMN/BV-07-C"    , .type = &nm_v1_2   ,.obj_count = 1, .msg_types = { "EMAIL"},},
    {.nr = 3, .descr = "MAP/MSE/MMN/BV-08-C 09" , .type = &ed_v1_2   ,.obj_count = 1, .msg_types = { "IM"},},
    {.nr = 4, .descr = "MAP/MSE/MMN/BV-10-C 15" , .type = &pp_v1_2   ,.obj_count = 1, .msg_types = { ""},},
    {.nr = 5, .descr = "MAP/MSE/MMN/BV-11-C"    , .type = &pc_v1_2   ,.obj_count = 1, .msg_types = { ""},},
    {.nr = 6, .descr = "MAP/MSE/MMN/BV-12-C 13" , .type = &cc_v1_2   ,.obj_count = 1, .msg_types = { ""},},
    {.nr = 7, .descr = "MAP/MSE/MMN/BV-14-C"    , .type = &mr_v1_2   ,.obj_count = 1, .msg_types = { ""},},
};

static struct test_config_s* mac_cfg = &test_configs[0];
static int cfg_start_index = 0;
static int curent_event_type = 0;

static void mac_print_test_config(struct test_set_config* cfg)
{
    MAP_PRINTF("mac_cfg #%d:%s obj_count:%d hdr:%s\n", mac_cfg->nr, mac_cfg->descr, mac_cfg->obj_count, mac_cfg->type->header);
}

static void mac_init_test_cases(struct test_set_config* cfg) {
    cfg_start_index = 0;
    curent_event_type = 0;
    cfg->fp_print_test_config(cfg);
}

void mac_next_test_case(struct test_set_config* cfg) {
    // cycle throug all test cases
    if (++mac_cfg >= &test_configs[ARRAYSIZE(test_configs)])
        mac_cfg = &test_configs[0];
    // init MAP Access Server test cases
    cfg->fp_init_test_cases(cfg);
}

void mac_select_test_case_n(struct test_set_config* cfg, uint8_t n) {
    if (n < ARRAYSIZE(test_configs))
    {
        mac_cfg = &test_configs[n];
        // init MAP Access Server test cases
        cfg->fp_init_test_cases(cfg);
    }
    else
        MAP_PRINTF("Error: coulnd't set config <%d>", n);
}

static void mac_print_test_cases(struct test_set_config* cfg)
{
    struct test_config_s* tc = &test_configs[0];
    
    do {
        MAP_PRINTF("[%d] <%s>\n", tc->nr, tc->descr);
    } while (++tc < &test_configs[ARRAYSIZE(test_configs)]);
}
struct test_set_config mac_test_set =
{
    .fp_init_test_cases    = mac_init_test_cases,
    .fp_next_test_case     = mac_next_test_case,
    .fp_select_test_case_n = mac_select_test_case_n,
    .fp_print_test_config  = mac_print_test_config,
    .fp_print_test_cases   = mac_print_test_cases,
};

static int gen_event_report(char* buf, int maxsize, int index)
{
    index = index % ARRAYSIZE(mac_cfg->msg_types);
    int pos = 0;
    
    pos += snprintf(&buf[pos], maxsize - pos, mac_cfg->type->header);
    pos += snprintf(&buf[pos], maxsize - pos, mac_cfg->type->body, mac_cfg->msg_types[index]);
    pos += snprintf(&buf[pos], maxsize - pos, mac_cfg->type->footer);

    return pos;
}

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
        map_notification_client_t* map_notification_client = (map_notification_client_t*)btstack_linked_list_iterator_next(&it);
        if (map_notification_client->cid == cid) {
            return map_notification_client;
        }
    }
    return NULL;
}

static map_notification_client_t* map_notification_client_for_map_cid(uint16_t map_cid) {
    return map_notification_client_for_cid(map_cid);
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
    map_notification_client_t* map_notification_client = (map_notification_client_t*)user_data;
    switch (header_id) {
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch (map_notification_client->state) {
                default:
                    printf("unexpected state: %d\n", map_notification_client->state);
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
   
    map_notification_client_t* map_notification_client = map_notification_client_for_goep_cid(goep_cid);
    btstack_assert(map_notification_client != NULL);

    switch (map_notification_client->state) {
        case MNC_STATE_W2_SEND_CONNECT_REQUEST:
            goep_client_request_create_connect(map_notification_client->goep_client.cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(map_notification_client->goep_client.cid, map_notification_client_service_uuid, 16);

            // }
            map_notification_client->state = MNC_STATE_W4_CONNECT_RESPONSE;
            // prepare response
            map_notification_client_prepare_operation(map_notification_client, OBEX_OPCODE_CONNECT);
            map_notification_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(map_notification_client->goep_client.cid);
            break;

        case MNC_STATE_W2_SEND_DISCONNECT_REQUEST:
            goep_client_request_create_disconnect(map_notification_client->goep_client.cid);
            map_notification_client->state = MNC_STATE_W4_DISCONNECT_RESPONSE;
            // prepare response
            map_notification_client_prepare_operation(map_notification_client, OBEX_OPCODE_DISCONNECT);
            // send packet
            goep_client_execute(map_notification_client->goep_client.cid);
            break;

        case MNC_STATE_W2_PUT_SEND_EVENT: 
#ifdef ENABLE_GOEP_L2CAP
            log_error("With L2CAP in MAP_OLD MAP/MSE/MMN/BV-04-C PTS 8.6.0B6 sets maximum packet length suddendly to only 150 bytes but minimum size reports are 200+")
#else
            {
                uint8_t application_parameters[20];
                uint16_t pos = 0;

                char dummy_report[300];
                gen_event_report(dummy_report, sizeof(dummy_report), curent_event_type);

                goep_client_request_create_put(map_notification_client->goep_client.cid);
                goep_client_header_add_srm_enable(map_notification_client->goep_client.cid);
                goep_client_header_add_type(map_notification_client->goep_client.cid, "x-bt/MAP-event-report");

                application_parameters[pos++] = MAP_APP_PARAM_MASInstanceID;
                application_parameters[pos++] = 1;
                application_parameters[pos++] = 0; // First MASInstanceID should be 0 (BT SIG MAS SPEC, hard coded expactation in PTS MAP/MSE/MMN/BV-02-C)
                goep_client_header_add_application_parameters(map_notification_client->goep_client.cid, &application_parameters[0], pos);

                goep_client_body_add_static(map_notification_client->goep_client.cid, dummy_report, (uint32_t)strlen(dummy_report));

                // cycle through message types
                if (++curent_event_type >= ARRAYSIZE(dummy_report))
                    curent_event_type = 0;

                //goep_client_body_add_static(map_notification_client->goep_client.cid, (uint8_t *) "0", 1);
                map_notification_client->state = MNC_STATE_W4_PUT_SEND_EVENT;
                map_notification_client_prepare_operation(map_notification_client, OBEX_OPCODE_PUT);
                map_notification_client->request_number++;
                goep_client_execute(map_notification_client->goep_client.cid);
            }

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
    map_notification_client_t* map_notification_client;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_GOEP_META:
            switch (hci_event_goep_meta_get_subevent_code(packet)) {
                case GOEP_SUBEVENT_CONNECTION_OPENED:
                    map_notification_client = map_notification_client_for_goep_cid(goep_subevent_connection_opened_get_goep_cid(packet));
                    btstack_assert(map_notification_client != NULL);
                    status = goep_subevent_connection_opened_get_status(packet);
                    map_notification_client->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                    map_notification_client->incoming = goep_subevent_connection_opened_get_incoming(packet);
                    goep_subevent_connection_opened_get_bd_addr(packet, map_notification_client->bd_addr);
                    if (status) {
                        log_info("map: connection failed %u", status);
                        map_notification_client->state = MNC_STATE_INIT;
                        map_notification_client_emit_connected_event(map_notification_client, status);
                    }
                    else {
                        log_info("map: connection established");
                        map_notification_client->goep_client.cid = goep_subevent_connection_opened_get_goep_cid(packet);
                        map_notification_client->state = MNC_STATE_W2_SEND_CONNECT_REQUEST;
                        goep_client_request_can_send_now(map_notification_client->goep_client.cid);
                    }
                    break;
                case GOEP_SUBEVENT_CONNECTION_CLOSED:
                    map_notification_client = map_notification_client_for_goep_cid(goep_subevent_connection_closed_get_goep_cid(packet));
                    btstack_assert(map_notification_client != NULL);

                    if (map_notification_client->state != MNC_STATE_CONNECTED) {
                        map_notification_client_emit_operation_complete_event(map_notification_client, OBEX_DISCONNECTED);
                    }
                    map_notification_client->state = MNC_STATE_INIT;
                    btstack_linked_list_remove(&map_notification_clients, (btstack_linked_item_t*)map_notification_client);
                    map_notification_client_emit_connection_closed_event(map_notification_client);
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
    map_notification_client_t* map_notification_client = map_notification_client_for_goep_cid(goep_cid);
    btstack_assert(map_notification_client != NULL);

    if (map_notification_client->obex_parser_waiting_for_response == false) return;

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&map_notification_client->obex_parser, packet, size);
    if (parser_state != OBEX_PARSER_OBJECT_STATE_COMPLETE) {
        return;
    }

    map_notification_client->obex_parser_waiting_for_response = false;
    obex_parser_operation_info_t op_info;
    obex_parser_get_operation_info(&map_notification_client->obex_parser, &op_info);

    switch (map_notification_client->state) {
        case MNC_STATE_W4_CONNECT_RESPONSE:
            switch (op_info.response_code) {
                case OBEX_RESP_SUCCESS:
                    map_notification_client->state = MNC_STATE_CONNECTED;
                    map_notification_client_emit_connected_event(map_notification_client, ERROR_CODE_SUCCESS);
                    break;
                default:
                    log_info("map: obex connect failed, result 0x%02x", packet[0]);
                    map_notification_client->state = MNC_STATE_INIT;
                    map_notification_client_emit_connected_event(map_notification_client, OBEX_CONNECT_FAILED);
                    break;
            }
            break;
        case MNC_STATE_W4_DISCONNECT_RESPONSE:
            map_notification_client->state = MNC_STATE_INIT;
            goep_client_disconnect(map_notification_client->goep_client.cid);
            break;

        case MNC_STATE_W4_PUT_SEND_EVENT:
            switch (op_info.response_code) {
                case OBEX_RESP_CONTINUE:
                    map_notification_client_handle_srm_headers(map_notification_client);
                    if (map_notification_client->srm_state == SRM_ENABLED) {
                        map_notification_client_prepare_operation(map_notification_client, OBEX_OPCODE_GET);
                        break;
                    }
                    map_notification_client->state = MNC_STATE_W2_PUT_SEND_EVENT;
                    goep_client_request_can_send_now(map_notification_client->goep_client.cid);
                    break;
                case OBEX_RESP_SUCCESS:
                    map_notification_client->state = MNC_STATE_CONNECTED;
                    map_notification_client_emit_operation_complete_event(map_notification_client, ERROR_CODE_SUCCESS);
                    break;
                case OBEX_RESP_NOT_IMPLEMENTED:
                    map_notification_client->state = MNC_STATE_CONNECTED;
                    map_notification_client_emit_operation_complete_event(map_notification_client, OBEX_UNKNOWN_ERROR);
                    break;
                case OBEX_RESP_NOT_FOUND:
                    map_notification_client->state = MNC_STATE_CONNECTED;
                    map_notification_client_emit_operation_complete_event(map_notification_client, OBEX_NOT_FOUND);
                    break;
                case OBEX_RESP_UNAUTHORIZED:
                case OBEX_RESP_FORBIDDEN:
                case OBEX_RESP_NOT_ACCEPTABLE:
                case OBEX_RESP_UNSUPPORTED_MEDIA_TYPE:
                case OBEX_RESP_ENTITY_TOO_LARGE:
                    map_notification_client->state = MNC_STATE_CONNECTED;
                    map_notification_client_emit_operation_complete_event(map_notification_client, OBEX_NOT_ACCEPTABLE);
                    break;
                default:
                    log_info("unexpected obex response 0x%02x", op_info.response_code);
                    map_notification_client->state = MNC_STATE_CONNECTED;
                    map_notification_client_emit_operation_complete_event(map_notification_client, OBEX_UNKNOWN_ERROR);
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

uint8_t map_notification_client_put_send_event(uint16_t map_cid){
    map_notification_client_t * map_notification_client = map_notification_client_for_map_cid(map_cid);
    if (map_notification_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_notification_client->state != MNC_STATE_CONNECTED){
        return BTSTACK_BUSY;
    }
    map_notification_client->state = MNC_STATE_W2_PUT_SEND_EVENT;
    map_notification_client->request_number = 0;
    goep_client_request_can_send_now(map_notification_client->goep_client.cid);
    return 0;
}

void map_notification_client_init(void) {
    map_notification_clients = NULL;
}

uint8_t map_notification_client_connect(map_notification_client_t* map_notification_client, l2cap_ertm_config_t* l2cap_ertm_config,
                                        uint16_t l2cap_ertm_buffer_size, uint8_t* l2cap_ertm_buffer,
                                        btstack_packet_handler_t handler, bd_addr_t addr, uint8_t instance_id,
                                        uint16_t* out_cid) {

    memset(map_notification_client, 0, sizeof(map_notification_client_t));
    map_notification_client->state = MNC_STATE_W4_GOEP_CONNECTION;
    map_notification_client->client_handler = handler;
    uint8_t status = goep_client_connect((goep_client_t*)map_notification_client, l2cap_ertm_config,
                                         l2cap_ertm_buffer, l2cap_ertm_buffer_size, &map_notification_client_packet_handler,
                                         addr, BLUETOOTH_SERVICE_CLASS_MESSAGE_NOTIFICATION_SERVER, instance_id,
                                         &map_notification_client->cid);
    btstack_assert(status == ERROR_CODE_SUCCESS);
    btstack_linked_list_add(&map_notification_clients, (btstack_linked_item_t*)map_notification_client);
    *out_cid = map_notification_client->cid;
    return status;
}


uint8_t map_notification_client_disconnect(uint16_t map_cid) {
    map_notification_client_t* map_notification_client = map_notification_client_for_map_cid(map_cid);
    if (map_notification_client == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (map_notification_client->state != MNC_STATE_CONNECTED) {
        return BTSTACK_BUSY;
    }

    map_notification_client->state = MNC_STATE_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(map_notification_client->goep_client.cid);
    return 0;
}
