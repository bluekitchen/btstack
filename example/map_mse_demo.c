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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "map_mse_demo.c"

// *****************************************************************************
/* EXAMPLE_START(map_server_demo): Provide MAP Messaging Server Equipment (MSE)
 *
 */
// *****************************************************************************


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack_event.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "yxml.h"
#include "classic/goep_client.h"
#include "classic/goep_server.h"
#include "classic/map.h"
#include "classic/map_access_server.h"
#include "classic/map_notification_client.h"
#include "classic/map_util.h"
#include "classic/obex.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#define MAS_SERVER_RFCOMM_CHANNEL_NR 1
#define MAS_SERVER_GOEP_PSM 0x1001

static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void mns_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static char* create_next_mnc_event_report_body_object(void);

static uint8_t upload_buffer[1000];
// we create the full obex body object and send it in chunks via continuations
static uint8_t OBEX_body_object[40000];

// MAP Notification Client - MAP allows only 1 client connection
// which can be used from multiple map access server connections by using the MASInstanceID
static struct {
    // bitmask of open notifications
    // we asume our own MAS ConnectionIDs are in the range of 1..HIGHEST_MAS_CONNECTION_ID_VALUE
    uint32_t active_notifications;
    map_notification_client_t mnc;
    uint16_t cid;
} mnc = {0};

// we asume our own MAS ConnectionIDs are in the range of 1..HIGHEST_MAS_CONNECTION_ID_VALUE
STATIC_ASSERT(sizeof(mnc.active_notifications)*8 >= HIGHEST_MAS_CONNECTION_ID_VALUE, active_notifications_bit_mask_too_small);

#ifdef ENABLE_GOEP_L2CAP
// singleton instance
static uint8_t mnc_ertm_buffer[4000];
static l2cap_ertm_config_t map_notification_client_ertm_config = {
    1,  // ertm mandatory
    2,  // max transmit, some tests require > 1
    2000,
    12000,
    512,    // l2cap ertm mtu
    4,
    4,
    1,      // 16-bit FCS
};
static l2cap_ertm_config_t *p_mnc_ertm_cfg = &map_notification_client_ertm_config;
#else
static uint8_t mnc_ertm_buffer[1];
static l2cap_ertm_config_t *p_mnc_ertm_cfg = NULL;
#endif

static bd_addr_t    remote_addr;
static const char* remote_addr_string = "001bdc0732ef"; // BDADDR PTS New MW  "008098090C1D"; // BDADDR PTS Old MW //"001BDC08E272";
static btstack_packet_callback_registration_t hci_event_callback_registration;



struct objconfig_s {
    char* header;
    char* footer;
    const char* body;
    // array of up to 3 function pointers for creation of body
    // last entry needs to be NULL
    size_t(*fbody)(char* msg_buffer, uint16_t index, size_t maxsize);

};

// forward declared functions
static size_t body_msg(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t body_msg_short(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t body_msg_email_1_1(char* msg_buffer, uint16_t index, size_t maxsize);
//static size_t body_msg_email_1_1_del(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t body_convo(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t PRINT_bmessage(char* msg_buffer, uint16_t index, size_t maxsize);
static void select_event_report_n(int er);
static void MSE_MMU_BV_02_ClConn_Timer(void);
static void PUT_IN_APP(void);
static void PUT_IN_STACK(void);


static int cfg_start_index = 0;
static int cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter = 0;
static int one_object_more_or_less = 0;
static uint16_t ListingSize = 0;
static bool folder_msg_deleted = false;
static char* request_path = "msg";

static map_uint128_t DatabaseIdentifier = { 0 };
// BT SIG Test Suite PTS is not acepting what BT SIG MAP spec describes as valid counters:
// "variable length (max. 32 bytes), 128 - bit value in hex string format":
// "00112233445566778899AABBCCDDEEFF"
// "0000000000000000"
// "0000000000000001"
// "00000000"
// "00000001"
// { 0 } works
static map_uint128_t FolderVersionCounter = { 0 };
static map_uint128_t ConversationListingVersionCounter = { 0 };
static map_uint128_t ConversationID = { 0 };

static void increase_version_counter_by_1(char* name, map_uint128_t counter) {
    counter[BT_UINT128_LEN_BYTES - 1]++;
    log_debug("%s:", name);
    log_debug_hexdump(counter, BT_UINT128_LEN_BYTES);
}

// temp min3
uint32_t btstack_min3(uint32_t a, uint32_t b, uint32_t c){
    return btstack_min(btstack_min(a,b), c);
}
/*
* Call-backs which update/change the state or behaviour of a test case, e.g. add one more message after each GetMessageListing
*/
static void MAP_MSE_MMB_BV_23_inc_VersCnt(void) {
    increase_version_counter_by_1("FolderVersionCounter", FolderVersionCounter);
}

static void MAP_MSE_MMB_BV_24_inc_ConvCnt(void) {
    increase_version_counter_by_1("ConversationListingVersionCounter",ConversationListingVersionCounter);
    cfg_start_index++;
    one_object_more_or_less++;
}

static void MAP_MSE_MMB_BV_25_inc_ConvListCnt(void) {
    increase_version_counter_by_1("FolderVersionCounter", ConversationListingVersionCounter);
}

static void emit_report(void) {
    //select_event_report_n(9);
    char* body = create_next_mnc_event_report_body_object();
    map_notification_client_send_event(mnc.cid, 0, (uint8_t*)body, strlen(body));
    MAP_PRINTF("map_notification_client_send_event mnc.cid:%04x [%s]\n", mnc.cid, body);
    log_debug("sent the EventReport");
}

static btstack_timer_source_t event_report_timer;
static int count;
static void gen_event_report_timer_handler(btstack_timer_source_t* ts) {
    log_debug("Timer count:%d", count);
    if (count-- > 0) {
        // Re-Arm Timer
        btstack_run_loop_set_timer(ts, 1000);
        btstack_run_loop_add_timer(ts);
        emit_report();
    }
    else {
        btstack_run_loop_remove_timer(ts);
        log_debug("Timer removed");
    }
}

static void MAP_MSE_MMD_BV_05_PutMsg(void) {
    select_event_report_n(0);
    char* body = create_next_mnc_event_report_body_object();
    map_notification_client_send_event(mnc.cid, 0, (uint8_t *) body, strlen(body));
    MAP_PRINTF("map_notification_client_send_event mnc.cid:%04x [%s]\n", mnc.cid, body);
    log_debug("sent a RemovedMessage notification");
}

static void MAP_MSE_MMB_BV_43_getConvoListng(void) {
    select_event_report_n(1);
    increase_version_counter_by_1("ConversationListingVersionCounter", ConversationListingVersionCounter);
    char* body = create_next_mnc_event_report_body_object();
    map_notification_client_send_event(mnc.cid, 0, (uint8_t *) body, strlen(body));
    MAP_PRINTF("map_notification_client_send_event mnc.cid:%04x [%s]\n", mnc.cid, body);
    log_debug("sent a ConversationChanged notification");
}

static void MAP_MSE_MMU_BV_02_I_PutMsg(void) {
    one_object_more_or_less++;
    //if (one_object_more_or_less > 1) {
    //    cfg_start_index++;
    //}
    log_debug("one_object_more_or_less:%d ", one_object_more_or_less);
}

#define MSG_LISTING_HEADER   "<MAP-msg-listing version=\"1.1\">"
#define MSG_LISTING_FOOTER   "</MAP-msg-listing>"

#define CONVO_LISTING_HEADER "<MAP-convo-listing version=\"1.0\">"
#define CONVO_LISTING_FOOTER "</MAP-convo-listing>"

struct objconfig_s msg = {
    .header = MSG_LISTING_HEADER,
    .footer = MSG_LISTING_FOOTER,
    .fbody = body_msg
};

struct objconfig_s msgshrt = {
    .header = MSG_LISTING_HEADER,
    .footer = MSG_LISTING_FOOTER,
    .fbody = body_msg_short
};

struct objconfig_s msg1_1 = {
    .header = MSG_LISTING_HEADER,
    .footer = MSG_LISTING_FOOTER,
    .fbody = body_msg_email_1_1
};

struct objconfig_s convo = {
    .header = CONVO_LISTING_HEADER,
    .footer = CONVO_LISTING_FOOTER,
    .fbody = body_convo
};

struct objconfig_s s_bmsg = {
    .header = "",
    .footer = "",
    .fbody = PRINT_bmessage
};

// copied from PTS v8.6.0B6 file EMAIL_NewMessage_event_report_1_0
struct objconfig_s nm_v1_0 = {
    .header = "<MAP-event-report version=\"1.0\">",
    .footer = "</MAP-event-report>",
    .body = "<event type=\"NewMessage\" handle=\"0123456789000003\" folder=\"TELECOM/MSG/INBOX\" msg_type=\"%s\" read_status=\"yes\" acknowledged_status=\"no\"/>"
};

// copied from PTS v8.6.0B6 file GSM_NewMessage_event_report_1_1
struct objconfig_s nm_v1_1 = {
    .header = "<MAP-event-report version=\"1.1\">",
    .footer = "</MAP-event-report>",
    .body = "<event type = \"NewMessage\" handle=\"0123456789000001\" folder=\"TELECOM/MSG/OUTBOX\" msg_type=\"%s\" subject=\"Subject\" datetime=\"20130121T130510\" sender_name=\"Xyz\" priority=\"no\"/>"
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
    .body = "<event type = \"ParticipantPresenceChanged\" sender_name=\"PTS\" conversation_id=\"3909231965\" presence_availability=\"1\" last_activity=\"20160625T133700\" participant_uci=\"skype:beastmode2\"/>"
};

// copied from PTS v8.6.0B6 file ParticipantChatStateChanged_event_report_1_2
struct objconfig_s pc_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body = "<event type = \"ParticipantChatStateChanged\" sender_name=\"PTS\" conversation_id=\"3909231965\" last_activity=\"20160625T133700\" chat_state=\"1\" participant_uci=\"skype:beastmode2\"/>"
};

// copied from PTS v8.6.0B6 file ConvrsationChanged_event_report_1_2
struct objconfig_s cc_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body = "<event type=\"ConversationChanged\" msg_type=\"IM\" sender_name=\"PTS\""
              " conversation_id=\"3909231965\"  presence_availability=\"1\"  chat_state=\"1\" last_activity=\"20160625T133700\""
              " participant_uci=\"skype:beastmode2\"/>"
};

// copied from PTS v8.6.0B6 file IM_MessageRemoved_event_report_1_2
struct objconfig_s mr_v1_2 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body = "<event type = \"MessageRemoved\" handle=\"0123456789001000\" folder=\"TELECOM/MSG/INBOX\"  msg_type=\"IM\"/>"
};

// copied from PTS v8.6.0B6 file IM_MessageRemoved_event_report_1_2
struct objconfig_s mr_v1_2_A0 = {
    .header = "<MAP-event-report version=\"1.2\">",
    .footer = "</MAP-event-report>",
    .body = "<event type = \"MessageRemoved\" handle=\"A0\" folder=\"TELECOM/MSG/INBOX\"  msg_type=\"IM\"/>"
};

#define TC_NORM(data, ...)  {data,  ## __VA_ARGS__},
#ifdef ENABLE_GOEP_L2CAP
#define TC_RFCOM(...)       {.descr = "ENABLE_GOEP_L2CAP is set", .disabled = true },
#define TC_L2CAP            TC_NORM
#else
#define TC_RFCOM            TC_NORM
#define TC_L2CAP(...)       {.descr = "ENABLE_GOEP_L2CAP is disabled", .disabled = true },
#endif
#define TC_HIDE(data, ...)  {.hide = true, data,  ## __VA_ARGS__},
#define TC_DISA(...) 

#define MAX_TC_OBJECTS 10
static struct test_config_s
{
    int nr;
    char* descr;
    bool disabled; // test case cannot be run
    bool hide;     // test case is not printed in the menu - for automatic reports
    bool SRMP_wait; // Test case wants us to send the SRMP header to pass
    struct objconfig_s* type;
    void (*fcon)(void); // optional handler for OBEX connect
    void (*fdiscon)(void); // optional handler for OBEX disconnect
    void (*fClConn)(void); // optional handler for MAP Notification Server Client Connect OK
    void (*fClOpCompl)(void); // optional handler for MAP Notification Client Operation Complete
    void (*fGetMsgListng)(void); // optional handler for OBEX getMessageListing
    void (*fGetConvoListng)(void); // optional handler for OBEX getConvoListing
    void (*fPutMsg)(void); // optional handler for OBEX PutMessage
    int obj_count;
    char* objects[MAX_TC_OBJECTS]; // maximum 6-1 entries, last one is null
    enum msg_status_read msg_stati[MAX_TC_OBJECTS]; // maximum 6-1 entries, last one is null
    bool msg_deleted[MAX_TC_OBJECTS]; // maximum 6-1 entries, last one is null
    char* helpstr; // additional help for setup, environment etc.
} test_configs[] =
{
TC_NORM( .descr = "MAP/MSE/MSM/,MNR/,MMB/,MFB/,MMI/,GEOP/BC,ROB,CON"     ,.obj_count = 5, .objects = { "EMAIL","SMS_GSM","SMS_CDMA", "MMS", "IM"  }                                                      ,.helpstr = "PTS IXIT set 'TSPX_secure_simple_pairing_pass_key_confirmation' to 'true'"                                                     )
TC_NORM( .descr = "MAP/MSE/MMB/BV-23"                                    ,.obj_count = 1, .objects = { "EMAIL","EMAIL"                            }, .fGetMsgListng   = MAP_MSE_MMB_BV_23_inc_VersCnt    ,.helpstr = "PTS wants wants to see an updated Folder version counter on the 2nd GET message listing"                                       )
TC_NORM( .descr = "MAP/MSE/MMB/BV-24"                                    ,.obj_count = 0, .objects = { "",""                                      }, .fGetConvoListng = MAP_MSE_MMB_BV_24_inc_ConvCnt    ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMB/BV-25"                                    ,.obj_count = 0, .objects = { "",""                                      }, .fGetConvoListng = MAP_MSE_MMB_BV_25_inc_ConvListCnt,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMB/BV-43"                                    ,.obj_count = 0, .objects = { "",""                                      }, .fGetConvoListng = MAP_MSE_MMB_BV_43_getConvoListng ,                                                                                                                                           )                                                                                                                     
TC_NORM( .descr = "MAP/MSE/MMD/BV-02,05"                                 ,.obj_count = 5, .objects = { "EMAIL","SMS_GSM", "SMS_CDMA", "MMS", "IM" }, .fPutMsg         = MAP_MSE_MMD_BV_05_PutMsg         ,                                                                                                                                           )
TC_NORM(.descr = "-MAP/MSE/MMU/BV-02"                                    ,.obj_count = 1, .objects = { "SMS_CDMA", "EMAIL", "SMS_GSM", "MMS", "IM"}, .fPutMsg         = MAP_MSE_MMU_BV_02_I_PutMsg       ,.helpstr = "WIP: PTS 8.6.1 MAP_NEW L2CAP bug: claims to send a PUT but doesnt. could be consolidated into tc #0 otherwise"                 )
TC_DISA( .descr = "MAP/MSE/MMU/BV-02 test"         , .type = &msgshrt    ,.obj_count = 0, .objects = { "", "", "", "" , ""                        }, .fPutMsg         = MAP_MSE_MMU_BV_02_I_PutMsg       ,.helpstr = "WIP: finding out what PTS 8.7.0 expects"                                                                                       )
TC_NORM(.descr = "MAP/MSE/MMU/BV-03"                                     ,.obj_count = 0, .objects = { "EMAIL",                                   }, .fPutMsg         = MAP_MSE_MMU_BV_02_I_PutMsg       ,.helpstr = "WIP: PTS 8.6.1 MAP_NEW L2CAP bug: claims to send a PUT but doesnt. could be consolidated into tc #0 otherwise"                 )
TC_NORM( .descr = "MAP/MSE/SGSIT/ATTR/BV-01"                             ,.obj_count = 1, .objects = { "EMAIL"                                    }                                                      ,                                                                                                                                           )
TC_NORM(.descr = "-MAP/MSE/MFMH/BV-01..05"                               ,.obj_count = 1, .objects = { "EMAIL"                                    }, .fcon = PUT_IN_APP, .fdiscon = PUT_IN_STACK         ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MFB/BV-02 05 07"                              ,.obj_count = 0, .objects = { ""                                         }                                                      ,                                                                                                                                           )
TC_L2CAP(.descr = "MAP/MSE/GOEP/SRM/BI-02..08"                           ,.obj_count = 5, .objects = { "EMAIL","SMS_GSM","SMS_CDMA", "MMS", ""    }                                                      ,.helpstr = "WIP: MAP_OLD only. PTS 8.6.1 MAP_NEW & L2CAP bug: claims to send a PUT but doesnt. could be consolidated into tc #0 otherwise" )
TC_L2CAP(.descr = "MAP/MSE/GOEP/SRMP/BV-02,BI-02"                        ,.obj_count = 9, .objects = { "IM"                                       }                                                      ,                                                                                                                                           )
TC_L2CAP(.descr = "MAP/MSE/GOEP/SRMP/BV-03"         , .SRMP_wait = true  ,.obj_count = 9, .objects = { "IM"                                       }, .fcon = PUT_IN_APP, .fdiscon = PUT_IN_STACK         ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-02"               , .type = &nm_v1_0   ,.obj_count = 5, .objects = { "EMAIL", "SMS_GSM", "MMS", "IM", "SMS_CDMA"}, .fClConn = MSE_MMU_BV_02_ClConn_Timer               ,.helpstr = "Generate the requested event reports by pressing <e> when PTS asks for"                                                        )
TC_NORM( .descr = "MAP/MSE/MMN/BV-04,06"            , .type = &nm_v1_1   ,.obj_count = 5, .objects = { "EMAIL", "SMS_GSM", "MMS", "IM", "SMS_CDMA"}, .fClConn = MSE_MMU_BV_02_ClConn_Timer               ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-07"               , .type = &nm_v1_2   ,.obj_count = 1, .objects = { "EMAIL"                                    }, .fClConn = emit_report                              ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-08..09"           , .type = &ed_v1_2   ,.obj_count = 1, .objects = { "IM"                                       }, .fClConn = emit_report                              ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-10,15"            , .type = &pp_v1_2   ,.obj_count = 1, .objects = { ""                                         }, .fClConn = emit_report                              ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-11,16"            , .type = &pc_v1_2   ,.obj_count = 1, .objects = { ""                                         }, .fClConn = emit_report                              ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-12..13"           , .type = &cc_v1_2   ,.obj_count = 1, .objects = { ""                                         }, .fClConn = emit_report                              ,                                                                                                                                           )
TC_NORM( .descr = "MAP/MSE/MMN/BV-14"               , .type = &mr_v1_2   ,.obj_count = 1, .objects = { ""                                         }, .fClConn = emit_report                              ,                                                                                                                                           )
TC_HIDE( .descr = "MAP/MSE/MMB/BV-43 AUTO"          , .type = &cc_v1_2   ,.obj_count = 1, .objects = { "IM"                                       }                                                      ,                                                                                                                                           )
TC_HIDE( .descr = "MAP/MSE/MMD/BV-05 AUTO"          , .type = &mr_v1_2_A0,.obj_count = 1, .objects = { "IM"                                       }                                                      ,                                                                                                                                           )  
};
static struct test_config_s* mas_cfg = &test_configs[0];
static char event_report_body_object[300];
static struct test_config_s* evt_cfg = &test_configs[0];
static int evtcfg_start_index = 0;
static int curent_event_type = 0;

static void PUT_IN_APP(void) {
    map_server_handle_PUT_in_APP(true);
    log_debug("now PUT is handled by APP");
}

static void PUT_IN_STACK(void) {
    map_server_handle_PUT_in_APP(false);
    log_debug("now PUT is handled by the stack");
}

static void MSE_MMU_BV_02_ClConn_Timer(void) {
    count = evt_cfg->obj_count;
    btstack_run_loop_set_timer_handler(&event_report_timer, gen_event_report_timer_handler);
    btstack_run_loop_set_timer(&event_report_timer, 1000);
    btstack_run_loop_add_timer(&event_report_timer);
    log_debug("Timer set count:%d times", count);
}

static void mas_init_event_report(void) {
    evtcfg_start_index = 0;
    curent_event_type = 0;
}

static void mas_init_test_cases(struct test_set_config* cfg) {
    
    int i;
    for (i = 0; i < ARRAYSIZE(test_configs); i++) {
        test_configs[i].nr = i;
        memset(test_configs[i].msg_stati, 0, sizeof(test_configs[i].msg_stati));
        memset(test_configs[i].msg_deleted, 0, sizeof(test_configs[i].msg_deleted));
    }

    cfg_start_index = 0;
    one_object_more_or_less = 0;

    cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter = 0;
//    cfg->fp_print_test_config(cfg);
}

static void mas_next_test_case(struct test_set_config* cfg) {
    // cycle throug all test cases
    if (++mas_cfg >= &test_configs[ARRAYSIZE(test_configs)]) {
        mas_cfg = &test_configs[0];
    }
    evt_cfg = mas_cfg;
    // init MAP Access Server test cases
    cfg->fp_init_test_cases(cfg);
    cfg->fp_print_test_config(cfg);
}

static void mas_previous_test_case(struct test_set_config* cfg) {
    // cycle throug all test cases
    if (--mas_cfg < &test_configs[0]) {
        mas_cfg = &test_configs[ARRAYSIZE(test_configs)-1];
    }
    evt_cfg = mas_cfg;
    // init MAP Access Server test cases
    cfg->fp_init_test_cases(cfg);
    cfg->fp_print_test_config(cfg);
}

static void mas_select_test_case_n(struct test_set_config* cfg, uint8_t n) {
    if (n < ARRAYSIZE(test_configs))
    {
        mas_cfg = &test_configs[n];
        evt_cfg = mas_cfg;
        // init MAP Access Server test cases
        cfg->fp_init_test_cases(cfg);
        cfg->fp_print_test_config(cfg);
    }
    else
        MAP_PRINTF("Error: coulnd't set mas_cfg <%d>", n);
}

static void mas_print_test_config(struct test_set_config* cfg) {
    MAP_PRINTF("[%2u]:%s obj_count:%d %s\n", mas_cfg->nr, mas_cfg->descr, mas_cfg->obj_count, mas_cfg->helpstr ? mas_cfg->helpstr : "");
}

static void mas_print_test_case_help(struct test_set_config* cfg) {
    MAP_PRINTF("[%2u] <%s> help/hint:%s\n", mas_cfg->nr, mas_cfg->descr, mas_cfg->helpstr? mas_cfg->helpstr:"no help/hint available");
}

static void mas_print_test_cases(struct test_set_config* cfg)
{
    struct test_config_s* tc = &test_configs[0];
    do {
        if (!tc->hide) {
            bool selected = tc == mas_cfg;
            MAP_PRINTF("%c%2u%c <%s>\n", selected ? '[' : ' ',  tc->nr, selected ? ']' : ' ', tc->descr);
        }
    } while (++tc < &test_configs[ARRAYSIZE(test_configs)]);
}

struct test_set_config mas_test_set = 
{
    .fp_init_test_cases      = mas_init_test_cases,
    .fp_next_test_case       = mas_next_test_case,
    .fp_previous_test_case   = mas_previous_test_case,
    .fp_select_test_case_n   = mas_select_test_case_n,
    .fp_print_test_config    = mas_print_test_config,
    .fp_print_test_cases     = mas_print_test_cases,
    .fp_print_test_case_help = mas_print_test_case_help,
};

struct test_set_config* test_set = &mas_test_set;

struct test_set_config mac_test_set;


static int gen_event_report(char* buf, int maxsize, int index)
{
    if (evt_cfg->objects[index] == NULL)
        return 0;

    int pos = 0;

    pos += snprintf(&buf[pos], maxsize - pos, "%s", evt_cfg->type->header);
    pos += snprintf(&buf[pos], maxsize - pos, evt_cfg->type->body, evt_cfg->objects[index]);
    pos += snprintf(&buf[pos], maxsize - pos, "%s", evt_cfg->type->footer);

    return pos;
}

// internal event report selection indexes test_configs[] from the end with negativ indexes etc.
static void select_event_report_n(int er) {
    er = ARRAYSIZE(test_configs) - 1 - er;
    btstack_assert(er < ARRAYSIZE(test_configs));
    evt_cfg = &test_configs[er];
}

static char* create_next_mnc_event_report_body_object(void) {
    gen_event_report(event_report_body_object, sizeof(event_report_body_object), curent_event_type);

    // cycle through message types
    curent_event_type++;
    if (evt_cfg->objects[curent_event_type] == NULL)
        curent_event_type = 0;

    return event_report_body_object;
}

static size_t PRINT_bmessage(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index] || folder_msg_deleted)
        size = snprintf(msg_buffer, maxsize,
            "BEGIN:BMSG\r\n"
            "VERSION:1.0\r\n"
            "STATUS:%s\r\n"
            "TYPE:%s\r\n"
            "FOLDER:TELECOM/MSG/INBOX\r\n"
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "N:IUT. make the name 256 bytes long.\r\n"
            "TEL:1234567898\r\n"
            "END:VCARD\r\n"
            "BEGIN:BENV\r\n"
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "N:PTS. make the name 256 bytes long.\r\n"
            "TEL:14256913524\r\n"
            "END:VCARD\r\n"
            "BEGIN:BBODY\r\n"
            "ENCODING:G-7BIT\r\n"
            "CHARSET:native\r\n"
            "LENGTH:290\r\n"
            "BEGIN:MSG\r\n"
            "Index:A%u\r\n"
            "END:MSG\r\n"
            "END:BBODY\r\n"
            "END:BENV\r\n"
            "END:BMSG\r\n"
            ,
            mas_cfg->msg_stati[index] ? "READ" : "UNREAD",
            mas_cfg->objects[index],
            index
        );
    return size;
}

static size_t body_msg(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index] || folder_msg_deleted)
        size = snprintf(msg_buffer, maxsize,
            "<msg handle=\"A%X\""
            " type=\"%s\""
            " read=\"%s\""
            " direction=\"%s\""
            " sent=\"%s\""
            " subject=\"Sbjct\""
            " datetime=\"20140705T092200+0100\" sender_name=\"Jonas\""
            " sender_addressing=\"1@bla.net\" recipient_addressing=\"\""
            " size=\"512\" attachment_size=\"123\" priority=\"no\""
            " protected=\"no\""
            " conversation_id=\"E1E2E3E4\"" // "E1" is to short for PTS but happy with "E1E2E3E4\"

            //" attachment_mime_types=\"video/mpeg\"" // PTS wants this in MAP/MSE/MMD/BV-02, otherwise "no EMAIL message in message listing"
            "/>"
            ,
            index,
            mas_cfg->objects[index],
            mas_cfg->msg_stati[index] ? "yes" : "no",
            map_server_get_folder_MsgListingDir(request_path),
            map_server_get_folder_MsgListingSent(request_path)
        );
    return size;
}

// TODO: PTS MAP/MSE/MMU/BV-02 fails to handle multi-segment chunked responses so we had to send 2 message in a single rfcom/goep/obex response packet.
static size_t body_msg_short(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index] || folder_msg_deleted)
        size = snprintf(msg_buffer, maxsize,
            "<msg handle=\"A%X\""
            " type=\"%s\""
            " subject=\"Sbjct\""
            //" datetime=\"20140705T092200+0100\" sender_name=\"Jonas\""
            //" sender_addressing=\"1@bla.net\" recipient_addressing=\"\""
            //" size=\"512\" attachment_size=\"123\" priority=\"no\""
            " read=\"%s\""
            //" sent=\"yes\" protected=\"no\""
            //" conversation_id=\"E1E2E3E4\"" // "E1" is to short for PTS but happy with "E1E2E3E4\"
            //" direction=\"incoming\""
            //" attachment_mime_types=\"video/mpeg\"" // PTS wants this in MAP/MSE/MMD/BV-02, otherwise "no EMAIL message in message listing"
            "/>"
            ,
            index,
            mas_cfg->objects[index],
            mas_cfg->msg_stati[index] ? "yes" : "no"
        );
    return size;
}

static size_t body_msg_email_1_1(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index] || folder_msg_deleted)
        size = snprintf(msg_buffer, maxsize,
            "<event type = \"NewMessage\" handle=\"0123456789000001\" folder=\"TELECOM/MSG/OUTBOX\" msg_type=\"%s\" subject=\"Subject\" datetime=\"20130121T130510\" sender_name=\"Xyz\" priority=\"no\"/>",
            mas_cfg->objects[index]
        );
    return size;
}

static size_t body_convo(char* msg_buffer, uint16_t index, size_t maxsize) {
    static int version = 0, size = 0;
    // Implement the function to create a conversation
    size = snprintf(msg_buffer, maxsize,
        "<?xml version=\"1.0\" encoding= \"UTF-8\"?>"
        "<conversation id=\"E1E2E3E4F1F2F3F4A1A2A3A4B1B2B3%02X\" name=\"%s\""
        // optional fields and elements
        " last_activity=\"20140612T105430+0100\""
        " read_status=\"%s\""       
        " version_counter=\"BEEF%02X\">"                                                                                 
            "\n<participant uci=\"0@bla.net\""
            " display_name=\"Eve\""                                                           
            " chat_state=\"3\""
            " last_activity=\"20140612T105430+0100\"/>"                                                  
        "\n</conversation>",
        index, // <conversation id = \"DEAD%02X\" 
        "Sbjct", // name="subject"
        // optional fields and elements
        // ,0, // last second of last_activity=\"20140612T10543%1u+0100\"
        "yes", // read_status = \"%s\"
        version++ // version_counter=\"BEEF%02X\"> 
        // 0, // <participant uci=\"%u@bla.net\"
        // "Eve", // display_name=\"%s\"
        // 3, // chat_state=\"%u\"
        // 0 // last_activity=\"20140612T10543%1u+0100\"/> 
    );
    return size;
}

// we create a full obex body message in a local buffer
static size_t create_obex_body(struct objconfig_s* type, uint16_t first, uint16_t last) {
    size_t pos = 0;
    size_t len = 0;
    size_t size = sizeof(OBEX_body_object);

    log_debug("1 first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);

    // clear OBEX_body_object
    OBEX_body_object[0] = 0;
    
    // header
    len = snprintf((char *) &OBEX_body_object[pos], size, "%s", type->header);
    pos += len; size -= len;
    
    while ((size > 0) && (first < last)){        
        log_debug("2 first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);
        // add entry
        len = type->fbody((char*) &OBEX_body_object[pos], first, size);
        pos += len; size -= len;
        first++;
    }

    log_debug("4 first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);

    // footer
    len = snprintf((char*) &OBEX_body_object[pos], size, "%s",type->footer);

    pos += len; size -= len;

    log_debug("5 first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);

    return pos;
}

enum body_object { o_msg, o_convo, o_bmsg };

static void send_obex_object(enum body_object obj, uint16_t map_cid, uint16_t start_index, uint16_t end_index, uint32_t continuation) {

    static size_t object_size = 0;
    size_t start = continuation, len, body_size = map_server_get_max_body_size(map_cid);
    uint8_t response_code;
    struct objconfig_s* type;

    if (mas_cfg->type != NULL) {
        // override automatic config of response body object
        type = mas_cfg->type;
    } else {
        switch (obj) {
        case o_msg:   type = &msg; break;
        case o_convo: type = &convo; break;
        case o_bmsg:  type = &s_bmsg; break;
        }
    }

    if (continuation == 0) {
        end_index = btstack_min(end_index, mas_cfg->obj_count + one_object_more_or_less);
        object_size = create_obex_body(type, start_index, end_index);

        log_debug("obj_count:%u start_index:%u end_index:%u one_object_more_or_less:%u\n", 
            mas_cfg->obj_count, start_index, end_index, one_object_more_or_less);
    }

    // copy first part of OBEX body object into upload_buffer, limit len to space, buf size and packet size
    len = object_size - continuation; log_debug("len:%d", len);
    len = btstack_min3((uint32_t)len, (uint32_t)body_size, (uint32_t) sizeof(upload_buffer));
    continuation += (uint32_t)len;
    log_debug("len:%d pos:%d upload_buffer [%.*s]", len, continuation, len, &OBEX_body_object[start]);

    if (continuation == object_size) {
        response_code = OBEX_RESP_SUCCESS;
        object_size = 0;
    } else {
        response_code = OBEX_RESP_CONTINUE;
    }
    map_server_send_response_with_body(map_cid, response_code, continuation, len, &OBEX_body_object[start]);
}

static uint8_t connect_map_notification_client(int connection_id) {
    uint8_t status = 0;
    btstack_assert(connection_id < HIGHEST_MAS_CONNECTION_ID_VALUE);
    log_debug("mnc.cid:0x%x mnc.active_notifications:0x%02x connection_id:%u", mnc.cid, mnc.active_notifications, connection_id);
    
    if (mnc.active_notifications == 0) {
        status = map_notification_client_connect(&mnc.mnc, p_mnc_ertm_cfg,
            sizeof(mnc_ertm_buffer), mnc_ertm_buffer,
            mns_packet_handler, remote_addr, 0, &mnc.cid);
    } else {
        log_info("we have already 0x%02x open client notifications, mnc.cid:0x%x connection_id:%u", mnc.active_notifications, mnc.cid, connection_id);
    }

    // set the corresponding bit in the active notifications bitmask
    mnc.active_notifications |= 1 << connection_id;
    log_debug("mnc.cid:0x%x mnc.active_notifications:0x%02x connection_id:%u", mnc.cid, mnc.active_notifications, connection_id);

    return status;
}

static uint8_t disconnect_map_notification_client(int connection_id) {
    uint8_t status = 0;
    btstack_assert(connection_id < HIGHEST_MAS_CONNECTION_ID_VALUE);
    log_debug("mnc.cid:0x%x mnc.active_notifications:0x%02x connection_id:%u", mnc.cid, mnc.active_notifications, connection_id);

    if (mnc.active_notifications != 0) {
        // reset the corresponding bit in the active notifications bitmask
        mnc.active_notifications &= ~(1 << connection_id);
        
        if (mnc.active_notifications == 0) {
            status = map_notification_client_disconnect(mnc.cid);
            mnc.cid = 0;
        } else {
            log_debug("not closed, still mnc.active_notifications:0x%02x mnc.cid:0x%x connection_id:%u", mnc.active_notifications, mnc.cid, connection_id);
        }

    } else {
        log_info("client connection already closed, mnc.cid:0x%x connection_id:%u", mnc.cid, connection_id);
    }

    return status;
}

#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    MAP_PRINTF("\nBTstack Demo MAP_Access_Server to support semi-automated testing using BT SIG PTS.EXE\n");
    MAP_PRINTF("Usage: Select the correct test setup, for most PTS tests [0] should work\n");
    MAP_PRINTF("       then just run the PTS test. Mostly you can just run one PTS test after each other.\n");
    MAP_PRINTF("       If the next test is failing, try resetting the setup pressing <r> or restart this app.\n");
    MAP_PRINTF("<RTRN> Show the full list of test setups.\n");
    MAP_PRINTF("<0..9> select test setups nummber 0..9\n");
    MAP_PRINTF("<n>    switch to next test setup (e.g. to got to numbers from 10 up)\n");
    MAP_PRINTF("<p>    switch to previous test setup\n");
    MAP_PRINTF("<h>    show help/hint for the current test case\n");
    MAP_PRINTF("-------manual commands for debug purpose");
    MAP_PRINTF("<r>    reset current test setup\n");
    MAP_PRINTF("<e>    send Event report PUT Notification to MCE Server\n\n");
    MAP_PRINTF("<f>    FolderVersionCounter++\n");
    MAP_PRINTF("<c>    ConversationListingVersionCounter++\n");
    MAP_PRINTF("<i>    ConversationID++\n");
    MAP_PRINTF("<a>    add one object\n");
    MAP_PRINTF("<d>    delete one object\n");
    test_set->fp_print_test_cases(test_set);
    test_set->fp_print_test_config(test_set);
}

static void stdin_process(char c){
    switch (c){
        case 'n':
            test_set->fp_next_test_case(test_set);
            break;

        case 'p':
            test_set->fp_previous_test_case(test_set);
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            test_set->fp_select_test_case_n(test_set, c - '0');
            break;

        case 'a':
            one_object_more_or_less = 1;
            MAP_PRINTF("Added one object, press <OK> in PTS");
            break;

        case 'd':
            one_object_more_or_less = -1;
            MAP_PRINTF("removed one object, press <OK> in PTS");
            break;

        case 'r':
            // reset current test cases
            test_set->fp_init_test_cases(test_set);
            break;

        case 'f':
            test_set->fp_print_test_config(test_set);
            MAP_PRINTF("FolderVersionCounter:");
            increase_version_counter_by_1("FolderVersionCounter", FolderVersionCounter);
            break;

        case 'i':
            test_set->fp_print_test_config(test_set);
            MAP_PRINTF("ConversationID:");
            increase_version_counter_by_1("ConversationID", ConversationID);
            break;

        case 'h':
            test_set->fp_print_test_case_help(test_set);
            break;

        case 'c':
            test_set->fp_print_test_config(test_set);
            MAP_PRINTF("ConversationListingVersionCounter:");
            increase_version_counter_by_1("ConversationListingVersionCounter", ConversationListingVersionCounter);
            break;

        case 'e': {
            char* body = create_next_mnc_event_report_body_object();
            map_notification_client_send_event(mnc.cid, 0, (uint8_t*) body, strlen(body));
            MAP_PRINTF("map_notification_client_send_event mnc.cid:%04x [%s]\n", mnc.cid, body);
            break;
        }
        default:
            show_usage();
            break;
    }
}


static void handle_set_message_status(char *msg_handle_str, uint8_t StatusIndicator, uint8_t StatusValue) {
#define ERROR(str_descr) do { log_error(str_descr); return; } while (0)
    int msg_handle;
    
    if (StatusValue > 1)
        ERROR("we only handle valid change-requests of Status Read=yes/no / deletedStatus");
    
    if (msg_handle_str == NULL)
        ERROR("msg_handle_str invalid");

    if (strnlen(msg_handle_str,10) != 2)
        ERROR("String is not 2 digits long");

    if (msg_handle_str[0] != 'A')
        ERROR("Message not in Format A[0...9]");

    if (msg_handle_str[1] < '0' || msg_handle_str[1] > '9')
        ERROR("no valid digit");

    msg_handle = msg_handle_str[1] - '0';

    if (msg_handle > ARRAYSIZE(mas_cfg->objects) - 1)
        ERROR("handle exceeds our nr of messages");

    log_debug("msg_handle_str:%s msg_handle:%u StatusIndicator:%u StatusValue:%u", msg_handle_str, msg_handle, StatusIndicator, StatusValue);

    switch (StatusIndicator) {
    
    case MAP_APP_PARAM_SUB_readStatus:
        mas_cfg->msg_stati[msg_handle] = StatusValue;
        log_info("MAP_APP_PARAM_SUB_readStatus msg_stati[%u] = StatusValue:%u", msg_handle, StatusValue);
        return;
    
    case MAP_APP_PARAM_SUB_deletedStatus:
        mas_cfg->msg_deleted[msg_handle] = StatusValue;
        log_info("MAP_APP_PARAM_SUB_deletedStatus msg_deleted[%u] = StatusValue:%u", msg_handle, StatusValue);
        return;
    
    default:
        ERROR("we only handle valid change - requests of Status StatusIndicator=0: Read=yes/no or =1:deletedStatus");
    }
}

static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int i;
    char value[MAP_MAX_VALUE_LEN];
    memset(value, 0, MAP_MAX_VALUE_LEN);
    bd_addr_t event_addr;

    //log_debug("packet_type:%u", packet_type);
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        show_usage();
                    }
                    break;

                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    MAP_PRINTF("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;
                default:
                    break;
            }
            break;

        case MAP_DATA_PACKET:
            for (i=0;i<size;i++){
                MAP_PRINTF("%c", packet[i]);
            }
            break;
        default:
            break;
    }
}
#endif

static void mas_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status, NotificationStatus, ChatState;
    uint16_t pos, start_index, max_list_count = 0, current_map_cid;
    uint32_t continuation, ConnectionID,
           NotificationFilterMask = MAP_APP_PARAM_SUB_AllNotifications,
        newNotificationFilterMask = MAP_APP_PARAM_SUB_AllNotifications;
    char request_name[32] = "";

    if (mas_cfg->disabled) {
        MAP_PRINTF("TEST CASE DISABLED, %s\n", mas_cfg->descr);
        return;
    }

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MAP_META:
                    // set app message read to the first app parameter;
                    pos = 3;
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_NOTIFICATION_EVENT:
                            log_info("Notification!");
                            break;

                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            current_map_cid = map_subevent_connection_opened_get_map_cid(packet);
                            map_subevent_connection_opened_get_bd_addr(packet, remote_addr);
                            MAP_PRINTF("[+] Connected %s current_map_cid 0x%04x\n", bd_addr_to_str(remote_addr), current_map_cid);
                            if (mas_cfg->fcon != NULL) {
                               RUN_AND_LOG_ACTION(mas_cfg->fcon();)
                            }
                            break;

                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            current_map_cid = map_subevent_connection_opened_get_map_cid(packet);
                            status = disconnect_map_notification_client(current_map_cid);
                            MAP_PRINTF("[+] Connection closed current_map_cid:0x%04x client disconnect status %u, %s\n", current_map_cid, status, mas_cfg->fdiscon?"disconnect handler":"re-init test case states");
                            if (mas_cfg->fdiscon != NULL) {
                                RUN_AND_LOG_ACTION(mas_cfg->fdiscon();)
                            }
                            else {
                                RUN_AND_LOG_ACTION(test_set->fp_init_test_cases(test_set);)
                            }
                            break;

                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            MAP_PRINTF("[+] Operation complete, status 0x%02x\n",
                                   map_subevent_operation_completed_get_status(packet));
                            break;
							
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_16(packet, &pos, &start_index);
                            ListingSize = mas_cfg->obj_count + one_object_more_or_less;
                            map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            map_server_set_response_type_and_name(current_map_cid, "", "x-obex/folder-listing", false);
                            MAP_PRINTF("[+] Get Folder listing\n");                            send_obex_object(o_msg, current_map_cid, 0, mas_cfg->obj_count - 1 + one_object_more_or_less, continuation);
                            break;
							
                        case MAP_SUBEVENT_GET_MESSAGE_LISTING:
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_16(packet, &pos, &start_index);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);

                            request_path = request_name;
                            folder_msg_deleted = (0 == strcmp(request_name, "deleted"))?true:false;
                            uint8_t tmp_NewMessage = 0;
                            char MSETime[] = "20140612T105430+0100";
                            if (continuation == 0) {
                                ListingSize = mas_cfg->obj_count + one_object_more_or_less;
                                map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_NewMessage, &tmp_NewMessage);
                                // needs ro be present for MAP/MSE/MMB/BV-35
                                map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_MSETime, &MSETime[0]);
                                map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                                map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                                map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_FolderVersionCounter, FolderVersionCounter);
                            }
                            map_server_set_response_type_and_name(current_map_cid, "", "x-bt/MAP-msg-listing", false);
                            
                            if (cfg_start_index != 0) {
                                start_index = cfg_start_index;
                            }

                            MAP_PRINTF("[+] Get Message Listing folder:%s folder_msg_deleted:%u cfg_start_index:%d\n", request_name, folder_msg_deleted, cfg_start_index);

                            // BT MAP Spec requires to skip the body if max_list_count == 0
                            if (max_list_count == 0) {
                                map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);
                            }
                            else {
                                send_obex_object(o_msg, current_map_cid, start_index, max_list_count, continuation);
                            }

                            // some PTS tests require strange behaviour so we need a handler to modify default behaviour for GetMessageListing
                            if (   mas_cfg->fGetMsgListng != NULL
                                // we only want to call the helpers once per GET request, not on each L2CAP chunk
                                && continuation == 0)
                                mas_cfg->fGetMsgListng();

                            break;

                        case MAP_SUBEVENT_GET_MESSAGE:
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);

                            MAP_PRINTF("[+] Get Message <%s>\n", request_name);

                            if (request_name[0] != 'A' || request_name[1] < '0' || request_name[1] > '9') {
                                map_server_send_response(current_map_cid, OBEX_RESP_NOT_FOUND);
                                log_error("Invalid request MAP_SUBEVENT_GET_MESSAGE <%s>", request_name);
                            }
                            else {
                                uint16_t index = request_name[1] - '0';
                                map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                                send_obex_object(o_bmsg, current_map_cid, index, index + 1, continuation);
                            }
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_STATUS: {
                            uint8_t StatusIndicator;
                            uint8_t StatusValue;
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_08(packet, &pos, &StatusIndicator);
                            APP_READ_08(packet, &pos, &StatusValue);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);
                            MAP_PRINTF("[+] Put MessageStatus ObjectName:%s StatusIndicator:0x%02X StatusValue:0x%02X\n", request_name, (unsigned int)StatusIndicator, (unsigned int)StatusValue);
                            map_server_set_response_type_and_name(current_map_cid, "", "x-bt/messageStatus", false);
                            map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);
                            handle_set_message_status(request_name, StatusIndicator, (unsigned int)StatusValue);

                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;
                        }
                        case MAP_SUBEVENT_PUT_MESSAGE_UPDATE:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            MAP_PRINTF("[+] Put MessageUpdate\n");
                            map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                            map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);
                            // BT SIG Test case MAP/MSE/MMB/BV-23 asks for one more message after
                            // issuing a "update messages" request so we just simulate one
                            one_object_more_or_less = 1;

                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE: {
                            uint8_t OBEX_opcode;
                            char request_name[32];
                            //char* obex_name_hdr_new_msg_handle = "A1A2A3A4" // crrently not used
                            uint8_t Attachment;
                            uint8_t Charset;
                            uint8_t ModifyText;
                            map_uint64hex_t MessageHandle;
                            char new_handle[3]; // A0\0
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_08(packet, &pos, &OBEX_opcode);
                            APP_READ_08(packet, &pos, &Charset);
                            APP_READ_08(packet, &pos, &Attachment);
                            APP_READ_08(packet, &pos, &ModifyText);
                            APP_READ_STR(packet, &pos, sizeof(MessageHandle), (char *) MessageHandle);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);

                            MAP_PRINTF("[+] Put Message Charset:%u Attachment:%u MessageHandle:%s\n", Charset, Attachment, MessageHandle);
                            snprintf(new_handle, sizeof(new_handle), "A%u", mas_cfg->obj_count + one_object_more_or_less - cfg_start_index);
                            map_server_set_response_type_and_name(current_map_cid, new_handle, NULL, mas_cfg->SRMP_wait);
                            map_server_send_response(current_map_cid, OBEX_opcode & OBEX_OPCODE_FINAL_BIT_MASK ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE);

                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;
                        }

                        case MAP_SUBEVENT_PUT_SET_NOTIFICATION_REGISTRATION:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_32(packet, &pos, &ConnectionID);
                            APP_READ_08(packet, &pos, &NotificationStatus);
                            MAP_PRINTF("[+] Put NotificationRegistration \n");
                            map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                            map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);
                            if (NotificationStatus == 1) {
                                status = connect_map_notification_client(ConnectionID);
                                MAP_PRINTF("[-] Connect back to PTS MAP-MNS mnc.cid: <%u>(0x%04u), status:%u\n", mnc.cid, mnc.cid, status);
                            }
                            else if (NotificationStatus == 0) {
                                status = disconnect_map_notification_client(ConnectionID);
                                MAP_PRINTF("[-] Disable Notifications for MAP-MNS current_map_cid: <%u>(0x%04u), status:%u\n", mnc.cid, mnc.cid, status);
                            }
                            else {
                                btstack_assert(NotificationStatus < 2);
                            }
                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;

                        case MAP_SUBEVENT_PUT_SET_NOTIFICATION_FILTER:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_32(packet, &pos, &NotificationFilterMask);
                            MAP_PRINTF("[+] Put SetNotificationFilter NotificationFilterMask old:0x%08x new:0x%08x changed:0x%08x ReadStatusChanged:%s\n",
                                NotificationFilterMask, newNotificationFilterMask, NotificationFilterMask ^ newNotificationFilterMask, NotificationFilterMask & MAP_APP_PARAM_SUB_ReadStatusChanged ? "ON":"OFF");
                            newNotificationFilterMask = NotificationFilterMask;
                            map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                            map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);

                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;

                        case MAP_SUBEVENT_PUT_OWNER_STATUS: {
                            map_UTCstmpoffstr_t LastActivity;
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_STR(packet, &pos, sizeof(LastActivity), (char*)LastActivity);
                            APP_READ_08(packet, &pos, &ChatState);
                            MAP_PRINTF("[+] Put OwnerStatus\n");
                            map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                            map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);
                            // BT SIG Test case MAP/MSE/MMB/BV-23 asks for one more message after
                            // issuing a "update messages" request so we just simulate one
                            one_object_more_or_less = 1;

                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;
                        }

                        case MAP_SUBEVENT_GET_CONVO_LISTING: {
                            map_UTCstmpoffstr_t FilterPeriodBegin;
                            map_UTCstmpoffstr_t EndFilterPeriodEnd;
                            map_string_t FilterRecipient;
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_STR(packet, &pos, sizeof(FilterPeriodBegin), (char*)FilterPeriodBegin);
                            APP_READ_STR(packet, &pos, sizeof(EndFilterPeriodEnd), (char*)EndFilterPeriodEnd);
                            APP_READ_STR(packet, &pos, sizeof(EndFilterPeriodEnd), (char*)FilterRecipient);
                            APP_READ_STR(packet, &pos, sizeof(ConversationID), (char *) ConversationID);
                            ListingSize = mas_cfg->obj_count + one_object_more_or_less;
                            map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                            map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            if (max_list_count == 0) {
                                map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ConversationListingVersionCounter, ConversationListingVersionCounter);
                            }
                            map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                            // BT MAP Spec requires to skip the body if max_list_count == 0 TODO: replace with max_list_count and re-test
                            if (mas_cfg->obj_count == 0 && one_object_more_or_less == 0)
                                map_server_send_response(current_map_cid, OBEX_RESP_SUCCESS);
                            else
                                send_obex_object(o_convo, current_map_cid, 0, 0xffff, continuation);

                            // some PTS tests require strange behaviour so we need a handler to modify default behaviour for GetConvoListing
                            if (mas_cfg->fGetConvoListng != NULL)
                                mas_cfg->fGetConvoListng();

                            break;
                        }
                        case MAP_SUBEVENT_GET_MAS_INSTANCE_INFORMATION: {                            // some PTS tests require strange behaviour so we need a handler to modify default behaviour for GetConvoListing
                            if (mas_cfg->fGetConvoListng != NULL)
                                mas_cfg->fGetConvoListng();
                            uint8_t MASInstanceID;
                            const char MAS_INSTANCE_INFORMATION[] = "BTstack MAS INSTANCE";
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_08(packet, &pos, &MASInstanceID);
                            map_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_OwnerUCI, "BTstack OwnerUCI");
                            map_server_set_response_type_and_name(current_map_cid, NULL, NULL, false);
                            map_server_send_response_with_body(current_map_cid, OBEX_RESP_SUCCESS, 0, (uint16_t)sizeof(MAS_INSTANCE_INFORMATION), (uint8_t *) MAS_INSTANCE_INFORMATION);
                            break;
                        }
                        default:
                            log_info("unknown map meta event %d\n", hci_event_map_meta_get_subevent_code(packet));
                            break;
                    }
                    break;
                default:
                    log_info("unknown HCI event %d\n",
                            hci_event_packet_get_type(packet));
                    break;
            }

        case MAP_DATA_PACKET: {
            char buf[100]; int i;
            for (i = 0; i < size; i++) {
                snprintf(&buf[3 * i], 4, "%02x ", packet[i]);
            }
            //buf[3 * i] = 0;
            log_debug("MAP_DATA_PACKET (%u bytes): %s", size, buf);
            break;
        }
        default:
            log_debug("unknown event of type %d\n", packet_type);
            break;
    }
}

static void mns_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)) {
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            status = map_subevent_connection_opened_get_status(packet);
                            if (status) {
                                MAP_PRINTF("[!] Notification Connection failed, status 0x%02x\n", status);
                            } else {
                                mnc.cid = map_subevent_connection_opened_get_map_cid(packet);
                                MAP_PRINTF("[+] Connected mnc.cid 0x%04x\n",
                                           mnc.cid);
                                // call test setup sepcific client connect handler
                                if (mas_cfg->fClConn != NULL) {
                                    log_info("run mas_cfg->fClConn:%p", mas_cfg->fClConn);
                                    mas_cfg->fClConn();
                                }
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            MAP_PRINTF("[+] Notification Connection closed\n");
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            MAP_PRINTF("[+] Notification Operation Completed\n");
                            // call test setup sepcific client connect handler
                            if (mas_cfg->fClOpCompl != NULL) {
                                log_info("run mas_cfg->fClOpCompl:%p", mas_cfg->fClOpCompl);
                                mas_cfg->fClConn();
                            }
                            break;
                        default:
                        log_debug ("unknown MAP subevent of type %d\n", packet_type);
                            break;
                    }
                    break;
                default:
                    log_debug("unknown event of type %d\n", hci_event_map_meta_get_subevent_code(packet));
                    break;
            }
            break;
        default:
            log_debug("unknown packet type %d\n", packet_type);
            break;
    }
}

static uint8_t  map_message_access_service_buffer[150];
const char * name = "MAS";
const uint8_t supported_message_types =
  MAP_SUPPORTED_MESSAGE_TYPE_EMAIL
| MAP_SUPPORTED_MESSAGE_TYPE_SMS_GSM
| MAP_SUPPORTED_MESSAGE_TYPE_SMS_CDMA
| MAP_SUPPORTED_MESSAGE_TYPE_MMS
| MAP_SUPPORTED_MESSAGE_TYPE_IM;

static void register_map_access_server(uint8_t instance_id) {
    map_util_create_access_service_sdp_record(map_message_access_service_buffer,
        sdp_create_service_record_handle(),
        1,
        MAS_SERVER_RFCOMM_CHANNEL_NR,
        MAS_SERVER_GOEP_PSM,
        supported_message_types,
        MAP_SUPPORTED_FEATURES_ALL,
        name);
    sdp_register_service(map_message_access_service_buffer);
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    // init L2CAP
    l2cap_init();

    // init RFCOM
    rfcomm_init();

    // init SDP Server
    sdp_init();

    // init GOEP Client
    goep_client_init();

    // init GOEP Server
    goep_server_init();

    // register for HCI events
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sscanf_bd_addr(remote_addr_string, remote_addr);

    memset(map_message_access_service_buffer, 0, sizeof(map_message_access_service_buffer));
    
    register_map_access_server(0); // First MASInstanceID should be 0 (BT SIG MAS SPEC, hard coded expactation in PTS MAP/MSE/MMN/BV-02-C)
    register_map_access_server(1);

    map_server_init(mas_packet_handler, MAS_SERVER_RFCOMM_CHANNEL_NR, MAS_SERVER_GOEP_PSM, 0xffff);

    // setup MAP Notfication Client
    map_notification_client_init();

    // init test setups    
    test_set->fp_init_test_cases(test_set);


#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif    

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
