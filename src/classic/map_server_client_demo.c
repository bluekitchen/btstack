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
#include "map_util.h"

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

// copied from PTS v8.6.0B6 file IM_MessageRemoved_event_report_1_2
struct objconfig_s mr_v1_2_A0 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body   = "<event type = \"MessageRemoved\" handle=\"A0\" folder=\"TELECOM/MSG/INBOX\"  msg_type=\"IM\"/>"
};

#define TC_NORM(data, ...)  data,  ## __VA_ARGS__
#ifdef ENABLE_GOEP_L2CAP
#define TC_RFCOM(...)       .descr = " Please disable ENABLE_GOEP_L2CAP", .disabled = true },
#define TC_L2CAP            TC_NORM
#else
#define TC_RFCOM            TC_NORM
#define TC_L2CAP(...)       .descr = " Please enable ENABLE_GOEP_L2CAP", .disabled = true },
#endif


#define MAX_TC_OBJECTS 10 // maximum MAX_TC_OBJECTS-1 entries, last one is null
static struct test_config_s
{
    int nr;
    char* descr;
    bool disabled;
    struct objconfig_s* type;
    int obj_count;
    char* msg_types[MAX_TC_OBJECTS]; 
    enum msg_status_read msg_stati[MAX_TC_OBJECTS]; 
    bool msg_deleted[MAX_TC_OBJECTS];
    char* helpstr; // additional help for setup, environment etc.
} test_configs[] =
{
    {TC_NORM( .descr = "MAP/MSE/MMD/BV-05"                  , .type = &mr_v1_2_A0,.obj_count = 1, .msg_types = { "IM"},                                       },)  
    {TC_NORM( .descr = "MAP/MSE/MMB/BV-43 AUTO"             , .type = &cc_v1_2   ,.obj_count = 1, .msg_types = { "IM"},                                       },) 
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-02-C <e><e><e>.."    , .type = &nm_v1_0   ,.obj_count = 1, .msg_types = { "EMAIL", "SMS_GSM", "SMS_CDMA", "MMS", "IM"},},)
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-04..06"              , .type = &nm_v1_1   ,.obj_count = 1, .msg_types = { "EMAIL", "SMS_GSM", "SMS_CDMA", "MMS", "IM"},},)
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-07"                  , .type = &nm_v1_2   ,.obj_count = 1, .msg_types = { "EMAIL"},                                    },)                         
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-08..09"              , .type = &ed_v1_2   ,.obj_count = 1, .msg_types = { "IM"},                                       },)                         
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-10, 15"              , .type = &pp_v1_2   ,.obj_count = 1, .msg_types = { ""},                                         },)                         
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-11, 16"              , .type = &pc_v1_2   ,.obj_count = 1, .msg_types = { ""},                                         },)                         
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-12..13"              , .type = &cc_v1_2   ,.obj_count = 1, .msg_types = { ""},                                         },)                         
    {TC_NORM( .descr = "MAP/MSE/MMN/BV-14"                  , .type = &mr_v1_2   ,.obj_count = 1, .msg_types = { ""},                                         },)                         
    
};                                                                                                                                                  

static char event_report_body_object[300];
static struct test_config_s* mac_cfg = &test_configs[0];
static int cfg_start_index = 0;
static int curent_event_type = 0;

static void mac_print_test_config(struct test_set_config* cfg)
{
    MAP_PRINTF("mac_cfg #%d:%s obj_count:%d hdr:%s\n", mac_cfg->nr, mac_cfg->descr, mac_cfg->obj_count, mac_cfg->type ? mac_cfg->type->header : "");
}

static void mac_init_test_cases(struct test_set_config* cfg) {
    int i;
    for (i = 0; i < ARRAYSIZE(test_configs); i++)
        test_configs[i].nr = i;

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
    if (mac_cfg->msg_types[index] == NULL)
        return 0;

    int pos = 0;
    
    pos += snprintf(&buf[pos], maxsize - pos, mac_cfg->type->header);
    pos += snprintf(&buf[pos], maxsize - pos, mac_cfg->type->body, mac_cfg->msg_types[index]);
    pos += snprintf(&buf[pos], maxsize - pos, mac_cfg->type->footer);

    return pos;
}

void mac_select_tc_MAP_MSE_MMD_BV_0x(int tc) {
    mac_select_test_case_n(&mac_test_set, tc);
}

char* create_next_mnc_event_report_body_object(void) {
    gen_event_report(event_report_body_object, sizeof(event_report_body_object), curent_event_type);

    // cycle through message types
    curent_event_type++;
    if (mac_cfg->msg_types[curent_event_type] == NULL)
        curent_event_type = 0;

    return event_report_body_object;
}

