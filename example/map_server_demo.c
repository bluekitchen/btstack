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

#define BTSTACK_FILE__ "map_server_demo.c"

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
#include "yxml.h"
#include "classic/goep_client.h"
#include "classic/goep_server.h"
#include "classic/map.h"
#include "classic/map_access_server.h"
#include "classic/map_notification_client.h"
#include "classic/map_server_client_demo.h"
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

static uint8_t upload_buffer[1000];
// we create the full obex body object and send it in chunks via continuations
static uint8_t OBEX_body_object[4000];

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
STATIC_ASSERT(sizeof(mnc.active_notifications)*CHAR_BIT >= HIGHEST_MAS_CONNECTION_ID_VALUE, active_notifications_bit_mask_too_small);

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

static size_t body_msg(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t body_msg_short(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t body_msg_email_1_1(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t body_convo(char* msg_buffer, uint16_t index, size_t maxsize);
static size_t PRINT_SMS_native_vcard(char* msg_buffer, uint16_t index, size_t maxsize);

static void MAP_MSE_MMD_BV_02_I_disc(void);
static void MAP_MSE_MMD_BV_02_I_getMsgListng(void);
static void MAP_MSE_MMB_BV_23_inc_VersCnt(void);
static void MAP_MSE_MMB_BV_24_inc_ConvCnt(void);
static void MAP_MSE_MMB_BV_25_inc_ConvListCnt(void);
static void MAP_MSE_MMU_BV_02_I_PutMsg(void);

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

struct objconfig_s vcard = {
    .header = "",
    .footer = "",
    .fbody = PRINT_SMS_native_vcard
};

#define TC_NORM(data, ...)  data,  ## __VA_ARGS__
#ifdef ENABLE_GOEP_L2CAP
#define TC_RFCOM(...)       .descr = " Please disable ENABLE_GOEP_L2CAP", .disabled = true },
#define TC_L2CAP            TC_NORM
#else
#define TC_RFCOM            TC_NORM
#define TC_L2CAP(...)       .descr = " Please enable ENABLE_GOEP_L2CAP", .disabled = true },
#endif

#define MAX_TC_OBJECTS 10
static struct test_config_s
{
    int nr;
    char* descr;
    bool disabled;
    struct objconfig_s* type;
    void (*fdiscon)(void); // optional handler for OBEX disconnect
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
{TC_NORM( .descr = "MAP/MSE/MSM/*../MNR/*"             ,.type = &msgshrt,.obj_count = 1, .objects = { "EMAIL"                                           }                                                      ,.helpstr = "PTS IXIT set 'TSPX_secure_simple_pairing_pass_key_confirmation' to 'true'"                                                                                                                                                                                                                                                      },)
{TC_NORM( .descr = "MAP/MSE/SGSIT/ATTR/BV-01"          ,.type = &msgshrt,.obj_count = 1, .objects = { "EMAIL"                                           }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/GOEP/SRM/BI-05"            ,.type = &msgshrt,.obj_count = 1, .objects = { "EMAIL"                                           }                                                      ,.helpstr = "PTS needs both L2CAP with 214 bytes short PDU and working messages(continue) so we need to create short messages to pass"                                                                                                                                                                                                                                                      },)
{TC_NORM( .descr = "MAP/MSE/MFMH/BV-01..05"            ,.type = &msg,    .obj_count = 1, .objects = { "EMAIL"                                           }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MFB/BV-02 05 07"           ,.type = &msg,    .obj_count = 0, .objects = { ""                                                }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMU/BV-03"                 ,.type = &msg,    .obj_count = 0, .objects = { "", "EMAIL", "MMS"                                }, .fPutMsg = MAP_MSE_MMU_BV_02_I_PutMsg               ,.helpstr = "WIP add OBEX NAME Header (Handle) PTS [50] Enter Test Step TS_MTC_OBEX_extract_handle_name ( , (lt)Not Defined Value(gt)  ) Test case error in 'MAP/MSE/MMU/BV-03'. The value 'headers_received' (-1) is not fully defined. See Screenshots in MAP_MSE_MMU_BV_03_I_2024_06_28_12_02_17"                                                                                      },)
{TC_NORM( .descr = "MAP/MSE/MMU/BV-02"                 ,.type = &msgshrt,.obj_count = 0, .objects = { "", "EMAIL", "MMS" , "EMAIL", "EMAIL"             }, .fPutMsg = MAP_MSE_MMU_BV_02_I_PutMsg               ,.helpstr = "WIP: PTS accepts the EMAIL but not the MMS. No idea why..."                                                                                                                                                                                                                                                                                                                    },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-09 10"              ,.type = &msg,    .obj_count = 2, .objects = { "SMS_GSM","SMS_CDMA"                              }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_L2CAP(.descr = "MAP/MSE/MMB/BV-11 13 14 42 46"     ,.type = &msg,    .obj_count = 2, .objects = { "SMS_GSM","SMS_CDMA"                              }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-12"                 ,.type = &msg,    .obj_count = 1, .objects = { "EMAIL", "EMAIL",                                 }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-15 18 20 22"        ,.type = &msg,    .obj_count = 5, .objects = { "EMAIL","SMS_GSM","SMS_CDMA", "MMS", "IM"         }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-16"                 ,.type = &msg,    .obj_count = 1, .objects = { "EMAIL","EMAIL"                                   }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-23"                 ,.type = &msg,    .obj_count = 1, .objects = { "EMAIL","EMAIL"                                   }, .fGetMsgListng = MAP_MSE_MMB_BV_23_inc_VersCnt      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-24"                 ,.type = &convo,  .obj_count = 0, .objects = { "",""                                             }, .fGetConvoListng = MAP_MSE_MMB_BV_24_inc_ConvCnt    ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-25"                 ,.type = &convo,  .obj_count = 0, .objects = { "",""                                             }, .fGetConvoListng = MAP_MSE_MMB_BV_25_inc_ConvListCnt,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-34 38 39 40 41 44"  ,.type = &convo,  .obj_count = 1, .objects = { "",""                                             }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-35 36 37"           ,.type = &msg,    .obj_count = 1, .objects = { "EMAIL"                                           }                                                      ,                                                                                                                                                                                                                                                                                                                                                                                           },)
{TC_NORM( .descr = "MAP/MSE/MMB/BV-47"                 ,.type = &msg,    .obj_count = 1, .objects = { "IM","IM"                                         }                                                      ,.helpstr = "PTS.EXE fails with '- MTC INCONC: no email message in message listing' but expects type=\"IM\""                                                                                                                                                                                                                                                                                },)
{TC_NORM( .descr = "MAP/MSE/MMD/BV-02"                 ,.type = &msg,    .obj_count = 1, .objects = { "EMAIL","MMS", "SMS_GSM","SMS_CDMA", "IM", "dummy"}, .fGetMsgListng = MAP_MSE_MMD_BV_02_I_getMsgListng   ,
                                                                                                                                                                           .fdiscon = MAP_MSE_MMD_BV_02_I_disc           ,.helpstr = "PTS 8.5.4 Build 6 issue: sends a sequence of OBEX connect, GetMessageListing (expects 1 EMAIL, nothing else, no more messages), PUT MessageStatus (Delete Message), GetMessageListing (expects empty listing), OBEX discoonect, repeat (MMS, SMS_GSM, SMS_CDMA) - the last repeat for IM misses the disconnect and fails on the Get because the list is still empty"           },)
};

static struct test_config_s* mas_cfg = &test_configs[0];
static int cfg_start_index = 0;
static int cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter = 0;
static int one_object_more_or_less = 0;
static uint16_t ListingSize = 0;

extern void mac_select_test_set(struct test_set_config* cfg);

static void mas_init_test_cases(struct test_set_config* cfg) {
    
    int i;
    for (i = 0; i < ARRAYSIZE(test_configs); i++)
        test_configs[i].nr = i;

    cfg_start_index = 0;
    one_object_more_or_less = 0;

    ListingSize = mas_cfg->obj_count + one_object_more_or_less;
    cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter = 0;
    cfg->fp_print_test_config(cfg);
}

static void mas_next_test_case(struct test_set_config* cfg) {
    // cycle throug all test cases
    if (++mas_cfg >= &test_configs[ARRAYSIZE(test_configs)])
        mas_cfg = &test_configs[0];
    // init MAP Access Server test cases
    cfg->fp_init_test_cases(cfg);
}

static void mas_select_test_case_n(struct test_set_config* cfg, uint8_t n) {
    if (n < ARRAYSIZE(test_configs))
    {
        mas_cfg = &test_configs[n];
        // init MAP Access Server test cases
        cfg->fp_init_test_cases(cfg);
    }
    else
        MAP_PRINTF("Error: coulnd't set mas_cfg <%d>", n);
}

static void mas_print_test_config(struct test_set_config* cfg) {
    MAP_PRINTF("mas_cfg #%d:%s obj_count:%d hdr:%s\n", mas_cfg->nr, mas_cfg->descr, mas_cfg->obj_count, mas_cfg->type?mas_cfg->type->header:"");
}

static void mas_print_test_case_help(struct test_set_config* cfg) {
    struct test_config_s* tc = &test_configs[0];
    MAP_PRINTF("[%d] <%s> help/hint:%s\n", tc->nr, tc->descr, tc->helpstr? tc->helpstr:"no help/hint available");
}

static void mas_print_test_cases(struct test_set_config* cfg)
{
    struct test_config_s* tc = &test_configs[0];
    do {
        MAP_PRINTF("[%d]%s <%s>\n", tc->nr, tc->helpstr ? "<h>" : "", tc->descr);
    } while (++tc < &test_configs[ARRAYSIZE(test_configs)]);
}

struct test_set_config mas_test_set = 
{
    .fp_init_test_cases      = mas_init_test_cases,
    .fp_next_test_case       = mas_next_test_case,
    .fp_select_test_case_n   = mas_select_test_case_n,
    .fp_print_test_config    = mas_print_test_config,
    .fp_print_test_cases     = mas_print_test_cases,
    .fp_print_test_case_help = mas_print_test_case_help,
};

struct test_set_config* test_set = &mas_test_set;

// declaration of config struct in mnc.mnc.c
struct test_set_config mac_test_set;
static select_test_set(char c) {
    if (c == 'S') {
        test_set = &mas_test_set;
    }
    else if (c == 'C') {
        test_set = &mac_test_set;
    }
    test_set->fp_print_test_cases(test_set);
    test_set->fp_init_test_cases(test_set);
}

static mas_uint128hex_t DatabaseIdentifier = { 0 };
// BT SIG Test Suite PTS is not acepting what BT SIG MAP spec describes as valid counters:
// "variable length (max. 32 bytes), 128 - bit value in hex string format":
// "00112233445566778899AABBCCDDEEFF"
// "0000000000000000"
// "0000000000000001"
// "00000000"
// "00000001"
// { 0 } works
static mas_uint128hex_t FolderVersionCounter = { 0 };
static mas_uint128hex_t ConversationListingVersionCounter = { 0 };
static mas_uint128hex_t ConversationID = { 0 };

static void increase_version_counter_by_1(char* name, mas_uint128hex_t counter) {
    counter[BT_UINT128_HEX_LEN_BYTES - 1]++;
    log_debug("%s:", name)
    log_debug_hexdump(counter, BT_UINT128_HEX_LEN_BYTES);
}


/*
* Call-backs which update/change the state or behaviour of a test case, e.g. add one more message after each GetMessageListing
*/
static void MAP_MSE_MMD_BV_02_I_disc(void) {
    log_debug("disabled default discconect behaviour");
    //cfg_start_index++;
    //one_object_more_or_less++;
    //log_debug("counter:%d cfg_start_index:%d one_object_more_or_less:%d", cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter, cfg_start_index, one_object_more_or_less);
    //cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter++;
}

static void MAP_MSE_MMD_BV_02_I_getMsgListng(void) {
    if ((cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter & 0x1) == 1) {
        cfg_start_index++;
        one_object_more_or_less++;
    }
    log_debug("counter:%d cfg_start_index:%d one_object_more_or_less:%d", cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter, cfg_start_index, one_object_more_or_less);
    cfg_MAP_MSE_MMD_BV_02_I_getMsgListng_counter++;
}

static void MAP_MSE_MMB_BV_23_inc_VersCnt(void) {
    increase_version_counter_by_1("FolderVersionCounter", FolderVersionCounter);
}

static void MAP_MSE_MMB_BV_24_inc_ConvCnt(void) {
    //increase_version_counter_by_1("ConversationListingVersionCounter",ConversationListingVersionCounter);
    cfg_start_index++;
    one_object_more_or_less++;
}

static void MAP_MSE_MMB_BV_25_inc_ConvListCnt(void) {
    increase_version_counter_by_1("FolderVersionCounter", ConversationListingVersionCounter);
}

static void MAP_MSE_MMU_BV_02_I_PutMsg(void) {
    one_object_more_or_less++;
    log_debug("one_object_more_or_less:%d", one_object_more_or_less);
}

static size_t PRINT_SMS_native_vcard(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index])
        size = snprintf(msg_buffer, maxsize,
            "BEGIN:BMSG\r\n"
            "VERSION:1.0\r\n"
            "STATUS:UNREAD\r\n"
            "TYPE:SMS_GSM\r\n"
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
            "0681\r\n"
            "END:MSG\r\n"
            "END:BBODY\r\n"
            "END:BENV\r\n"
            "END:BMSG\r\n"

        );
    return size;
}

static size_t body_msg(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index])
        size = snprintf(msg_buffer, maxsize,
            "<msg handle=\"A%X\""
            " type=\"%s\""
            " subject=\"Sbjct\""
            " datetime=\"20140705T092200+0100\" sender_name=\"Jonas\""
            " sender_addressing=\"1@bla.net\" recipient_addressing=\"\""
            " size=\"512\" attachment_size=\"123\" priority=\"no\" read=\"%s\" sent=\"yes\" protected=\"no\""
            " conversation_id=\"E1E2E3E4\"" // "E1" is to short for PTS but happy with "E1E2E3E4\"
            " direction=\"incoming\""
            " attachment_mime_types=\"video/mpeg\"/>" // PTS wants this in MAP/MSE/MMD/BV-02, otherwise "no EMAIL message in message listing"
            ,
            index,
            mas_cfg->objects[index],
            mas_cfg->msg_stati[index] ? "yes" : "no"
        );
    return size;
}


// TODO: PTS MAP/MSE/MMU/BV-02 fails to handle multi-segment chunked responses so we had to send 2 message in a single rfcom/goep/obex response packet.
static size_t body_msg_short(char* msg_buffer, uint16_t index, size_t maxsize) {
    index = index % ARRAYSIZE(mas_cfg->objects);
    int size = 0;
    if (!mas_cfg->msg_deleted[index])
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
            //" attachment_mime_types=\"video/mpeg\"/>" // PTS wants this in MAP/MSE/MMD/BV-02, otherwise "no EMAIL message in message listing"
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
    if (!mas_cfg->msg_deleted[index])
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
static size_t create_obex_body(uint16_t first, uint16_t last) {
    size_t pos = 0;
    size_t len = 0;
    size_t size = sizeof(OBEX_body_object);

    log_debug("1 first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);

    // clear OBEX_body_object
    OBEX_body_object[0] = 0;
    
    // header
    len = snprintf(&OBEX_body_object[pos], size, mas_cfg->type->header);
    pos += len; size -= len;
    log_debug("first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);
    
    while ((size > 0) && (first < last)){        
        // add entry
        len = mas_cfg->type->fbody(&OBEX_body_object[pos], first, size);
        pos += len; size -= len;
        first++;
        log_debug("first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);
    }

    // footer
    len = snprintf(&OBEX_body_object[pos], size, mas_cfg->type->footer);
    pos += len; size -= len;

    log_debug("first:%d last:%d pos:%d size:%d <%s>", first, last, pos, size, OBEX_body_object);

    return pos;
}

static void send_obex_object(uint16_t map_cid, uint16_t start_index, uint16_t end_index, uint32_t pos) {

    static size_t object_size = 0;
    size_t start = pos, len, body_size = map_access_server_get_max_body_size(map_cid);
    uint8_t response_code;

    if (pos == 0) {
        uint16_t total_messages, num_msgs_selected, end_index;

        // send messages listing
        total_messages = mas_cfg->obj_count + one_object_more_or_less;

        num_msgs_selected = total_messages - start_index;
        end_index = start_index + num_msgs_selected;
        object_size = create_obex_body(start_index, end_index);

        log_debug("obj_count:%u one_object_more_or_less:%d start_index:%u end_index:%u num_msgs_selected:%u\n", 
            mas_cfg->obj_count, one_object_more_or_less, start_index, end_index, num_msgs_selected);
    }

    // copy first part of OBEX body object into upload_buffer, limit len to space, buf size and packet size
    len = object_size - pos; log_debug("len:%d", len);
    len = min3(len, sizeof(upload_buffer), body_size);
    pos += (uint32_t)len;
    log_debug("len:%d pos:%d upload_buffer [%.*s]", len, pos, len, &OBEX_body_object[start]);

    if (pos == object_size) {
        response_code = OBEX_RESP_SUCCESS;
        object_size = 0;
    } else {
        response_code = OBEX_RESP_CONTINUE;
    }
    map_access_server_send_response(map_cid, response_code, pos, len, &OBEX_body_object[start]);
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
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);
    
    MAP_PRINTF("\n--- Bluetooth MAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));

    test_set->fp_print_test_cases(test_set);
    test_set->fp_print_test_config(test_set);
    MAP_PRINTF("<S> switch to Map Server test cases\n");
    MAP_PRINTF("<C> switch to Map Client test cases\n");
    MAP_PRINTF("<n> switch to next test mas_cfg\n");
    MAP_PRINTF("<h> show help/hint for the current test case\n");
    MAP_PRINTF("<0..9> select # 0..9\n");
    MAP_PRINTF("<f> FolderVersionCounter++\n");
    MAP_PRINTF("<c> ConversationListingVersionCounter++\n");
    MAP_PRINTF("<i> ConversationID++\n");
    MAP_PRINTF("<a> add one object\n");
    MAP_PRINTF("<d> delete one object\n");
    MAP_PRINTF("<r> reset current test case\n");
    MAP_PRINTF("<e> send Event report PUT Notification to MCE Server\n");
}

static void stdin_process(char c){
    switch (c){
        case 'n':
            test_set->fp_next_test_case(test_set);
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            test_set->fp_select_test_case_n(test_set, c - '0');
            break;

        case 'S': case 'C':
            // switch to Server test cases
            MAP_PRINTF("\nSelected %s Test Set\n", c == 'S' ? "Server" : "Client");
            select_test_set(c);
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
            map_notification_client_send_event(mnc.cid, 0, body, strlen(body));
            MAP_PRINTF("map_notification_client_send_event mnc.cid:%04x", mnc.cid);
            log_debug("sent event report:%s", body);
            break;
        }
        default:
            show_usage();
            break;
    }
}


static void handle_set_message_status(char *msg_handle_str, uint8_t StatusIndicator, uint8_t StatusValue) {
#define ERROR(str_descr) { error_msg = str_descr; goto error; }
    int msg_handle;
    const char* error_msg = "";
    
    
    if (StatusValue > 1)
        ERROR("we only handle valid change-requests of Status Read=yes/no / deletedStatus");
    
    if (msg_handle_str == NULL)
        ERROR("msg_handle_str invalid");

    if (strnlen(msg_handle_str,10) != 2)
        ERROR("String is not 2 digits long");

    if (msg_handle_str[1] < '0' || msg_handle_str[1] > '9')
        ERROR("no valid digit");

    msg_handle = msg_handle_str[1] - '0';

    if (msg_handle > ARRAYSIZE(mas_cfg->objects) - 1)
        ERROR("handle exceeds our nr of messages");


    switch (StatusIndicator) {
    
    case MAP_APP_PARAM_SUB_readStatus:
        mas_cfg->msg_stati[msg_handle] = StatusValue;
        break;
    
    case MAP_APP_PARAM_SUB_deletedStatus:
        mas_cfg->msg_deleted[msg_handle] = StatusValue;
        break;
    
    default:
        ERROR("we only handle valid change - requests of Status Read=yes/no or deletedStatus");
        break;
    }

    return;

error:
    log_error("%s", error_msg);
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
                            APP_READ_16(packet, &pos, &current_map_cid);
                            MAP_PRINTF("[+] Connected current_map_cid 0x%04x\n", current_map_cid);
                            map_subevent_connection_opened_get_bd_addr(packet, remote_addr);
                            break;

                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            status = disconnect_map_notification_client(current_map_cid);
                            MAP_PRINTF("[+] Connection closed current_map_cid:0x%04x client disconnect status %u, %s\n", current_map_cid, status, mas_cfg->fdiscon?"disconnect handler":"re-init test case states");
                            if (mas_cfg->fdiscon != NULL)
                                mas_cfg->fdiscon();
                            else
                                test_set->fp_init_test_cases(test_set);
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
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            map_access_server_set_response_type_and_name(current_map_cid, "", "x-obex/folder-listing");
                            MAP_PRINTF("[+] Get Folder listing\n");
                            send_obex_object(current_map_cid, 0, mas_cfg->obj_count - 1 + one_object_more_or_less, continuation);
                            break;
							
                        case MAP_SUBEVENT_GET_MESSAGE_LISTING:
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_16(packet, &pos, &start_index);
                            uint8_t tmp_NewMessage = 0;
                            char MSETime[] = "20140612T105430+0100";
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_NewMessage, &tmp_NewMessage);
                            // needs ro be present for MAP/MSE/MMB/BV-35
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_MSETime, &MSETime[0]);
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_FolderVersionCounter, FolderVersionCounter);
                            map_access_server_set_response_type_and_name(current_map_cid, "", "x-bt/MAP-msg-listing");

                            if (cfg_start_index != 0) {
                                start_index = cfg_start_index;
                            }

                            MAP_PRINTF("[+] Get Message Listing cfg_start_index:%d\n", cfg_start_index);

                            // BT MAP Spec requires to skip the body if max_list_count == 0
                            if (max_list_count == 0)
                                map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            else
                                send_obex_object(current_map_cid, start_index, max_list_count, continuation);

                            // some PTS tests require strange behaviour so we need a handler to modify default behaviour for GetMessageListing
                            if (mas_cfg->fGetMsgListng != NULL)
                                mas_cfg->fGetMsgListng();

                            break;

                        case MAP_SUBEVENT_GET_MESSAGE:
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            MAP_PRINTF("[+] Get Message\n");
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            send_obex_object(current_map_cid, 0, 1, continuation);
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_STATUS: {
                            uint8_t StatusIndicator;
                            uint8_t StatusValue;
                            char request_name[32];
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_08(packet, &pos, &StatusIndicator);
                            APP_READ_08(packet, &pos, &StatusValue);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);
                            MAP_PRINTF("[+] Put MessageStatus ObjectName:%s StatusIndicator:0x%02X StatusValue:0x%02X\n", request_name, (unsigned int)StatusIndicator, (unsigned int)StatusValue);
                            map_access_server_set_response_type_and_name(current_map_cid, "", "x-bt/messageStatus");
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            handle_set_message_status(request_name, StatusIndicator, (unsigned int)StatusValue);
                            break;
                        }
                        case MAP_SUBEVENT_PUT_MESSAGE_UPDATE:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            MAP_PRINTF("[+] Put MessageUpdate\n");
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS,  0, 0, NULL);
                            // BT SIG Test case MAP/MSE/MMB/BV-23 asks for one more message after
                            // issuing a "update messages" request so we just simulate one
                            one_object_more_or_less = 1;
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE: {
                            char request_name[32];
                            //char* obex_name_hdr_new_msg_handle = "A1A2A3A4" // crrently not used
                            uint8_t Attachment;
                            uint8_t Charset;
                            uint8_t ModifyText;
                            mas_uint64hex_t MessageHandle;
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_08(packet, &pos, &Charset);
                            APP_READ_08(packet, &pos, &Attachment);
                            APP_READ_08(packet, &pos, &ModifyText);
                            APP_READ_STR(packet, &pos, sizeof(MessageHandle), (char *) MessageHandle);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);

                            MAP_PRINTF("[+] Put Message Charset:%u Attachment:%u MessageHandle:%s\n", Charset, Attachment, MessageHandle);
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);

                            if (mas_cfg->fPutMsg != NULL)
                                mas_cfg->fPutMsg();
                            break;
                        }

                        case MAP_SUBEVENT_PUT_SET_NOTIFICATION_REGISTRATION:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_32(packet, &pos, &ConnectionID);
                            APP_READ_08(packet, &pos, &NotificationStatus);
                            MAP_PRINTF("[+] Put NotificationRegistration \n");
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
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
                            break;

                        case MAP_SUBEVENT_PUT_SET_NOTIFICATION_FILTER:
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_32(packet, &pos, &NotificationFilterMask);
                            MAP_PRINTF("[+] Put SetNotificationFilter NotificationFilterMask old:0x%08x new:0x%08x changed:0x%08x ReadStatusChanged:%s\n",
                                NotificationFilterMask, newNotificationFilterMask, NotificationFilterMask ^ newNotificationFilterMask, NotificationFilterMask & MAP_APP_PARAM_SUB_ReadStatusChanged ? "ON":"OFF");
                            newNotificationFilterMask = NotificationFilterMask;
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            break;

                        case MAP_SUBEVENT_PUT_OWNER_STATUS: {
                            mas_UTCstmpoffstr_t LastActivity;
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_STR(packet, &pos, sizeof(LastActivity), (char*)LastActivity);
                            APP_READ_08(packet, &pos, &ChatState);
                            MAP_PRINTF("[+] Put OwnerStatus\n");
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            // BT SIG Test case MAP/MSE/MMB/BV-23 asks for one more message after
                            // issuing a "update messages" request so we just simulate one
                            one_object_more_or_less = 1;
                            break;
                        }

                        case MAP_SUBEVENT_GET_CONVO_LISTING: {
                            mas_UTCstmpoffstr_t FilterPeriodBegin;
                            mas_UTCstmpoffstr_t EndFilterPeriodEnd;
                            mas_string_t FilterRecipient;
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_STR(packet, &pos, sizeof(FilterPeriodBegin), (char*)FilterPeriodBegin);
                            APP_READ_STR(packet, &pos, sizeof(EndFilterPeriodEnd), (char*)EndFilterPeriodEnd);
                            APP_READ_STR(packet, &pos, sizeof(EndFilterPeriodEnd), (char*)FilterRecipient);
                            APP_READ_STR(packet, &pos, sizeof(ConversationID), (char *) ConversationID);
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            if (max_list_count == 0) {
                                map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_ConversationListingVersionCounter, ConversationListingVersionCounter);
                            }
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            // BT MAP Spec requires to skip the body if max_list_count == 0
                            if (mas_cfg->obj_count == 0 && one_object_more_or_less == 0)
                                map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            else
                                send_obex_object(current_map_cid, 0, 0xffff, continuation);

                            // some PTS tests require strange behaviour so we need a handler to modify default behaviour for GetConvoListing
                            if (mas_cfg->fGetConvoListng != NULL)
                                mas_cfg->fGetConvoListng();

                            break;
                        }
                        case MAP_SUBEVENT_GET_MAS_INSTANCE_INFORMATION: {
                            uint8_t MASInstanceID;
                            const char MAS_INSTANCE_INFORMATION[] = "BTstack MAS INSTANCE";
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &current_map_cid);
                            APP_READ_08(packet, &pos, &MASInstanceID);
                            map_access_server_set_response_app_param(current_map_cid, MAP_APP_PARAM_OwnerUCI, "BTstack OwnerUCI");
                            map_access_server_set_response_type_and_name(current_map_cid, NULL, NULL);
                            map_access_server_send_response(current_map_cid, OBEX_RESP_SUCCESS, 0, (uint16_t)sizeof(MAS_INSTANCE_INFORMATION), (uint8_t *) MAS_INSTANCE_INFORMATION);
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
#if 0 // set to 0 to only print data
            switch (map_da) {
            case HCI_EVENT_MAP_META:
                switch (hci_event_map_meta_get_subevent_code(packet)) {
                case MAP_SUBEVENT_NOTIFICATION_EVENT:
                    log_info("Notification!");
                    break;
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
#endif

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
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                        MAP_PRINTF("[+] Notification Connection closed\n");
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

    // init test setups    
    test_set->fp_init_test_cases(test_set);

    memset(map_message_access_service_buffer, 0, sizeof(map_message_access_service_buffer));
    
    register_map_access_server(0); // First MASInstanceID should be 0 (BT SIG MAS SPEC, hard coded expactation in PTS MAP/MSE/MMN/BV-02-C)
    register_map_access_server(1);

    map_access_server_init(mas_packet_handler, MAS_SERVER_RFCOMM_CHANNEL_NR, MAS_SERVER_GOEP_PSM, 0xffff);

    // setup MAP Notfication Client
    map_notification_client_init();


#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif    

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
