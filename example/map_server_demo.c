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
#include "classic/map_util.h"
#include "classic/obex.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#define PRINTF_TO_PKTLOG
#ifdef PRINTF_TO_PKTLOG
#define MAP_PRINTF(format, ...)  printf(format,  ## __VA_ARGS__); HCI_DUMP_LOG("PRINTF", HCI_DUMP_LOG_LEVEL_INFO, format,  ## __VA_ARGS__)
#else
#define MAP_PRINTF MAP_PRINTF
#endif

#define MAS_SERVER_RFCOMM_CHANNEL_NR 1
#define MAS_SERVER_GOEP_PSM 0x1001

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);


static uint16_t map_cid;
// singleton instance required to configure GOEP
static uint8_t service_buffer[150];
static uint8_t upload_buffer[1000];


#ifdef ENABLE_GOEP_L2CAP
static uint8_t map_notification_client_ertm_buffer_mas_0[4000];
static uint8_t map_notification_client_ertm_buffer_mas_1[4000];
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
#endif


static bd_addr_t    remote_addr;
static const char* remote_addr_string = "001BDC08E272";
static btstack_packet_callback_registration_t hci_event_callback_registration;

#define MSG_LISTING_HEADER   "<MAP-msg-listing version=\"1.0\">"
#define MSG_LISTING_FOOTER   "</MAP-msg-listing>"
/* Sample from BT SIG MAP Spec Page 54
<MAP-convo-listing version = "1.0">
<conversation id="E1E2E3E4F1F2F3F4A1A2A3A4B1B2B3B4" name="Beergarden
Connection" last_activity="20140612T105430+0100" read_status="no"
version_counter="A1A1B2B2C3C3D4D5E5E6F6F7A7A8B8B">
<participant uci="4986925814@s.whateverapp.net" display_name="Tien"
chat_state="3" last_activity="20140612T105430+0100"/>
<participant uci="4912345678@s.whateverapp.net" display_name="Jonas"
chat_state="5" last_activity="20140610T115130+0100"/>
</conversation>
</MAP-convo-listing>
*/
#define CONVO_LISTING_HEADER "<MAP-convo-listing version=\"1.0\">"
#define CONVO_LISTING_FOOTER "</MAP-convo-listing>"

struct objconfig_s {
    char* header;
    char* footer;
    // array of up to 3 function pointers for creation of body
    // last entry needs to be NULL
    void (*fbody[4])(char* msg_buffer, uint16_t index, int maxsize);

};

static void body_msg(char* msg_buffer, uint16_t index, int maxsize);
static void body_convo(char* msg_buffer, uint16_t index, int maxsize);



struct objconfig_s msg = {
    .header = MSG_LISTING_HEADER,
    .footer = MSG_LISTING_FOOTER,
    .fbody = {body_msg, NULL }
};

struct objconfig_s convo = {
    .header = CONVO_LISTING_HEADER,
    .footer = CONVO_LISTING_FOOTER,
    .fbody = {body_convo, NULL }

};

enum msg_status_read { no, yes };
static struct test_config_s
{
    int nr;
    char* descr;
    struct objconfig_s* type;
    int obj_count;
    char* objects[6]; // maximum 6-1 entries, last one is null
    enum msg_status_read msg_stati[6]; // maximum 6-1 entries, last one is null
} test_configs[] =
{
{.nr = 0, .descr = "MAP/MSE/MMB/BV-09-I 10 11 13 14" , .type = &msg,  .obj_count = 2, .objects = { "SMS_GSM","SMS_CDMA"                      }, },
{.nr = 1, .descr = "MAP/MSE/MMB/BV-12-I"             , .type = &msg,  .obj_count = 1, .objects = { "EMAIL", "SMS_GSM","SMS_CDMA"             }, },
{.nr = 2, .descr = "MAP/MSE/MMB/BV-15-I 18 20 22"    , .type = &msg,  .obj_count = 5, .objects = { "EMAIL","SMS_GSM","SMS_CDMA", "MMS", "IM" }, },
{.nr = 3, .descr = "MAP/MSE/MMB/BV-16-I 23"          , .type = &msg,  .obj_count = 1, .objects = { "EMAIL","EMAIL"                           }, },
{.nr = 4, .descr = "MAP/MSE/MMB/BV-24-I <a><OK>"     , .type = &convo,.obj_count = 0, .objects = { "",""                                     }, },
{.nr = 5, .descr = "MAP/MSE/MMB/BV-25-I <c><OK>"     , .type = &convo,.obj_count = 0, .objects = { "",""                                     }, },
{.nr = 6, .descr = "MAP/MSE/MMB/BV-34-I 38 39"       , .type = &convo,.obj_count = 1, .objects = { "",""                                     }, },
{.nr = 7, .descr = "MAP/MSE/MMB/BV-35-I 36 37"       , .type = &msg,  .obj_count = 1, .objects = { "EMAIL"                                   }, },
};

struct test_config_s* config = &test_configs[0];
static int current_msg_type = 0;
static int add_one_object = 0;
uint16_t ListingSize = 0;

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
// BT SIG Test Suite PTS is not acepting what BT SIG MAP spec describes as valid counters:
// "variable length (max. 32 bytes), 128 - bit value in hex string format":
// "00000000000000000000000000000001"
// "0000000000000000"
// "0000000000000001"
// "00000000"
// "00000001"
// { 0 } doesn't work neither
static mas_uint128hex_t ConversationListingVersionCounter = { 0 };
static mas_uint128hex_t ConversationID = { 0 };

static void increase_version_counter_by_1(mas_uint128hex_t counter) {
    counter[BT_UINT128_HEX_LEN_BYTES - 1]++;
    log_debug_hexdump(counter, BT_UINT128_HEX_LEN_BYTES);
}

static void set_test_config(int nr) {
    if (nr < ARRAYSIZE(test_configs))
        config = &test_configs[nr];
    else
        printf("couldn't set test setup nr %d\n", nr);
}


static void init_testcases(void) {
    current_msg_type = 0;
    add_one_object = 0;
    ListingSize = config->obj_count + add_one_object;
}

static void body_msg(char* msg_buffer, uint16_t index, int maxsize) {
    index = index % ARRAYSIZE(config->objects);
    snprintf(msg_buffer, maxsize, "<msg handle=\"ID%u\" subject=\"Hello\" type=\"%s\" read=\"%s\""
        //" conversation_id=\"E1\"" /* might be required for v1.1? */
        //" direction=\"incoming\"" /* might be required for v1.1? */
        "/>",
        index,
        config->objects[index],
        config->objects[index] ? "yes" : "no");
}

static void body_convo(char* msg_buffer, uint16_t index, int maxsize) {
    static int version = 0;
    // Implement the function to create a conversation
    snprintf(msg_buffer, maxsize,
        "<conversation id=\"DEAD%02X\" name=\"%s\""
        // optional fields and elements
        // "last_activity=\"20140612T10543%1u+0100\" read_status=\"%s\""       
        "version_counter=\"BEEF%02X\">"                                                                                 
        // "<participant uci=\"%u@bla.net\" display_name=\"%s\""                                                           
        // "chat_state=\"%u\" last_activity=\"20140612T10543%1u+0100\"/>"                                                  
        "</conversation>",
        index, // <conversation id = \"DEAD%02X\" 
        "Sbjct", // name="subject"
        // optional fields and elements
        // ,0, // last second of last_activity=\"20140612T10543%1u+0100\"
        // "no", // read_status = \"%s\"
        version++ // version_counter=\"BEEF%02X\"> 
        // 0, // <participant uci=\"%u@bla.net\"
        // "Eve", // display_name=\"%s\"
        // 3, // chat_state=\"%u\"
        // 0 // last_activity=\"20140612T10543%1u+0100\"/> 
    );
}

// TODO enable to send message larger as one OBEX/MAP packet
static uint16_t send_listing(uint16_t first, uint16_t last) {
    uint16_t max_body_size = map_access_server_get_max_body_size(map_cid);
    char listing_buffer[500];
    uint16_t pos = 0;
    bool done = false;

    log_debug("1 first:%d last:%d max_body_size:%d", first, last, max_body_size);

    if (first == 0){
        // add header
        uint16_t len = (uint16_t)strlen(config->type->header);
        memcpy(upload_buffer, config->type->header, len);
        pos += len;
        max_body_size -= len;
    }

    while ((max_body_size > 0) && (first < last)){
        log_debug("2 first:%d last:%d pos:%d", first, last, pos);
        // add entry
        for (int i = 0; config->type->fbody[i] != NULL; i++)
            config->type->fbody[i](listing_buffer, first, sizeof(listing_buffer));
        // get len
        uint16_t len = (uint16_t)strlen(listing_buffer);
        log_debug("2.5 first:%d last:%d pos:%d len:%d", first, last, pos, len);
        if (len > max_body_size){
            break;
        }
        memcpy(&upload_buffer[pos], (const uint8_t *) listing_buffer, len);
        pos += len;
        max_body_size -= len;
        first++;
        log_debug("3 first:%d last:%d pos:%d len:%d", first, last, pos, len);
        log_debug("4 %.*s", btstack_min(pos, sizeof(listing_buffer)),upload_buffer);
    }


    if (first >= last){
        uint16_t len = (uint16_t) strlen(config->type->footer);

        log_debug("5 first:%d last:%d pos:%d len:%d", first, last, pos, len);
        if (len < max_body_size){
            log_debug("6 first:%d last:%d pos:%d len:%d", first, last, pos, len);
            // add footer
            memcpy(&upload_buffer[pos], (const uint8_t *)config->type->footer, len);
            pos += len;
            max_body_size -= len;
            done = true;
        }
    }


    uint8_t response_code;
	uint32_t continuation;

    if (done) {
        response_code = OBEX_RESP_SUCCESS;
        continuation = 0x10000;
    }
    else {
        response_code = OBEX_RESP_CONTINUE;
        continuation = first;
    }
    log_debug("7 send response %.*s", btstack_min(pos, sizeof(listing_buffer)), upload_buffer);
    map_access_server_send_get_put_response(map_cid, response_code, continuation, pos, upload_buffer);
    return first;
}

static void send_get_listing_object(uint8_t* packet, uint16_t start_index, uint16_t max_list_count, uint32_t continuation) {

    uint16_t total_messages, num_msgs_selected, end_index;

    //if (max_list_count == 0) {
    //    MAP_PRINTF("[+] Start max_list_count == 0\n");
    //    send_listing(0, 0);
    //    return;
    //}

    // send messages listing
    total_messages = config->obj_count + add_one_object;

    num_msgs_selected = total_messages - start_index;
    max_list_count = map_subevent_get_message_listing_get_MaxListCount(packet);
    if (max_list_count < 0xffff) {
        num_msgs_selected = btstack_min(max_list_count, num_msgs_selected);
    }
    MAP_PRINTF("[+] get message listing - list offset %u, num messages %u max_list_count %u\n", start_index, num_msgs_selected, max_list_count);
    // consider already sent cards
    if (continuation > 0xffff) {
        // just missed the footer
        start_index = 0xffff;
        num_msgs_selected = 0;
    }
    else {
        num_msgs_selected -= continuation;
        start_index += continuation;
    }
    end_index = start_index + num_msgs_selected;
    MAP_PRINTF("[-] continuation %u, num messages %u, start index %u, end index %u\n", continuation, num_msgs_selected, start_index, end_index);
    send_listing(start_index, end_index);
}

static void print_current_test_config(void)
{
    MAP_PRINTF("config #%d:%s obj_count:%d hdr:%s\n", config->nr, config->descr, config->obj_count, config->type->header);
}

#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);
    struct test_config_s* cfg = &test_configs[0];

    MAP_PRINTF("\n--- Bluetooth MAP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    do {
        MAP_PRINTF("[%d] <%s>\n", cfg->nr, cfg->descr);
    } while (++cfg < &test_configs[ARRAYSIZE(test_configs)]);

    print_current_test_config();
    MAP_PRINTF("<n> switch to next test config\n");
    MAP_PRINTF("<0..9> select # 0..9\n");
    MAP_PRINTF("<f> FolderVersionCounter++\n");
    MAP_PRINTF("<c> ConversationListingVersionCounter++\n");
    MAP_PRINTF("<i> ConversationID++\n");
    MAP_PRINTF("<d> DatabaseIdentifier++\n");

    MAP_PRINTF("<r> reset current test case\n");


}

static void stdin_process(char c){
    
    switch (c){
        case 'n':
            // cycle throug all test cases
            if (++config >= &test_configs[ARRAYSIZE(test_configs)])
                config = &test_configs[0];
            // init MAP Access Server test cases
            init_testcases();
            print_current_test_config();
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            uint8_t tc = c - '0';
            if (tc < ARRAYSIZE(test_configs))
            {
                config = &test_configs[c - '0'];
                // init MAP Access Server test cases
                init_testcases();
                print_current_test_config();
            }
            else
                MAP_PRINTF("Error: coulnd't set config <%c>", c);
            break;

        case 'a':
            // BT SIG Test case MAP/MSE/MMB/BV-23-I asks for one more message after
// issuing a "update messages" request so we just simulate one
            add_one_object = 1;
            MAP_PRINTF("Added one object, press <OK> in PTS");
            break;

        case 'r':
            // reset current test cases
            init_testcases();
            print_current_test_config();
            break;

        case 'f':
            print_current_test_config();
            MAP_PRINTF("FolderVersionCounter:");
            increase_version_counter_by_1(FolderVersionCounter);
            break;

        case 'i':
            print_current_test_config();
            MAP_PRINTF("ConversationID:");
            increase_version_counter_by_1(ConversationID);
            break;

        case 'c':
            print_current_test_config();
            MAP_PRINTF("ConversationListingVersionCounter:");
            increase_version_counter_by_1(ConversationListingVersionCounter);
            break;

        case 'd':
            print_current_test_config();
            MAP_PRINTF("DatabaseIdentifier:");
            increase_version_counter_by_1(DatabaseIdentifier);
            break;

        default:
            show_usage();
            break;
    }
}


static handle_set_message_status(char *msg_handle_str, uint8_t StatusIndicator, uint8_t StatusValue) {
#define ERROR(str_descr) { error_msg = str_descr; goto error; }
    int msg_handle;
    const char* error_msg = "";
    
    if (StatusIndicator != 0)
        ERROR("we only handle valid change - requests of Status Read=yes/no");
    
    if (StatusValue > 1)
        ERROR("we only handle valid change-requests of Status Read=yes/no");
    
    if (msg_handle_str == NULL)
        ERROR("msg_handle_str invalid");

    if (strnlen_s(msg_handle_str,10) != 3)
        ERROR("String is not 3 digits long");
    
    if (msg_handle_str[0] != 'I' || msg_handle_str[1] != 'D')
        ERROR("no valid ID prefix");

    if (msg_handle_str[2] < '0' || msg_handle_str[2] > '9')
        ERROR("no valid digit");

    msg_handle = msg_handle_str[2] - '0';

    if (msg_handle > ARRAYSIZE(config->objects) - 1)
        ERROR("handle exceeds our nr of messages");

    if (StatusValue > 1)
        ERROR("invalid status");

    config->msg_stati[msg_handle] = StatusValue;

error:
    log_error("%s", error_msg);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
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
    uint8_t status;
    uint16_t pos, start_index, max_list_count = 0, dummy_map_cid;
    uint32_t continuation;
	
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
                            status = map_subevent_connection_opened_get_status(packet);
                            if (status){
                                MAP_PRINTF("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                map_cid = map_subevent_connection_opened_get_map_cid(packet);
                                MAP_PRINTF("[+] Connected map_cid 0x%04x\n", map_cid);
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            MAP_PRINTF("[+] Connection closed, re-init test case states\n");
                            init_testcases();
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            MAP_PRINTF("[+] Operation complete, status 0x%02x\n",
                                   map_subevent_operation_completed_get_status(packet));
                            break;
							
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_FolderVersionCounter, FolderVersionCounter);
                            MAP_PRINTF("[+] Get Folder listing\n");
                            send_listing(0, config->obj_count-1 + add_one_object);
                            break;
							
                        case MAP_SUBEVENT_GET_MESSAGE_LISTING:
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_16(packet, &pos, &start_index);
                            uint8_t tmp_NewMessage = 0;
                            char MSETime[] = "20140612T105430+0100";
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_NewMessage, &tmp_NewMessage);
                            // needs ro be present for MAP/MSE/MMB/BV-35-I
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_MSETime, &MSETime[0]);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_FolderVersionCounter, FolderVersionCounter);

                            // BT MAP Spec requires to skip the body if max_list_count == 0
                            if (max_list_count == 0)
                                map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            else
                                send_get_listing_object(packet, start_index, max_list_count, continuation);
                            break;

                        case MAP_SUBEVENT_GET_MESSAGE:
                            MAP_PRINTF("[+] Get Message\n");
                            send_listing(0, 1);
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_STATUS:
                            uint8_t StatusIndicator;
                            uint8_t StatusValue;
                            char request_name[32];
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            APP_READ_08(packet, &pos, &StatusIndicator);
                            APP_READ_08(packet, &pos, &StatusValue);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name);
                            MAP_PRINTF("[+] Put MessageStatus ObjectName:%s StatusIndicator:0x%02X StatusValue:0x%02X\n", request_name, (unsigned int) StatusIndicator, (unsigned int)StatusValue);
                            map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            handle_set_message_status(request_name, StatusIndicator, (unsigned int)StatusValue);
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_UPDATE:
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            MAP_PRINTF("[+] Put MessageUpdate\n");
                            map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            // BT SIG Test case MAP/MSE/MMB/BV-23-I asks for one more message after
                            // issuing a "update messages" request so we just simulate one
                            add_one_object = 1;
                            break;

                        case MAP_SUBEVENT_GET_CONVO_LISTING:
                            mas_UTCstmpoffstr_t FilterPeriodBegin[50];
                            mas_UTCstmpoffstr_t EndFilterPeriodEnd[50];
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            APP_READ_STR(packet, &pos, sizeof(FilterPeriodBegin),  (char*)FilterPeriodBegin);
                            APP_READ_STR(packet, &pos, sizeof(EndFilterPeriodEnd), (char*)EndFilterPeriodEnd);
                            APP_READ_STR(packet, &pos, sizeof(ConversationID), ConversationID);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_DatabaseIdentifier, DatabaseIdentifier);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_ConversationListingVersionCounter, ConversationListingVersionCounter);
                            map_access_server_set_response_app_param(map_cid, MAP_APP_PARAM_ListingSize, &ListingSize);
                            // BT MAP Spec requires to skip the body if max_list_count == 0
                            if (config->obj_count == 0 && add_one_object == 0)
                                map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            else
                                send_get_listing_object(packet, 0, 0xffff, continuation);
                            break;
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
            break;

        case MAP_DATA_PACKET:
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
            MAP_PRINTF("MAP_DATA_PACKET (%u bytes): ", size);
            for (int i=0;i<size;i++){
                MAP_PRINTF("%0x2 ", packet[i]);
            }
            MAP_PRINTF ("\n");
            break;
        default:
            MAP_PRINTF ("unknown event of type %d\n", packet_type);
            break;
    }
}

static uint8_t  map_message_access_service_buffer[150];
const char * name = "MAS";

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
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sscanf_bd_addr(remote_addr_string, remote_addr);

    // init MAP Access Server test cases
    init_testcases();

    // setup MAP Access Server
    uint8_t supported_message_types =   MAP_SUPPORTED_MESSAGE_TYPE_EMAIL
                                      | MAP_SUPPORTED_MESSAGE_TYPE_SMS_GSM
                                      | MAP_SUPPORTED_MESSAGE_TYPE_SMS_CDMA
                                      | MAP_SUPPORTED_MESSAGE_TYPE_MMS
                                      | MAP_SUPPORTED_MESSAGE_TYPE_IM;
    uint32_t supported_features = 0x1F;
    memset(map_message_access_service_buffer, 0, sizeof(map_message_access_service_buffer));
    map_util_create_access_service_sdp_record(map_message_access_service_buffer,
                                                    sdp_create_service_record_handle(),
                                                    1,
                                                    MAS_SERVER_RFCOMM_CHANNEL_NR,
                                                    MAS_SERVER_GOEP_PSM,
                                                    supported_message_types,
                                                    supported_features,
                                                    name);
    sdp_register_service(map_message_access_service_buffer);
    map_access_server_init(mas_packet_handler, MAS_SERVER_RFCOMM_CHANNEL_NR, MAS_SERVER_GOEP_PSM, 0xffff);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif    

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
