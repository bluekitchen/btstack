/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "map_client_demo.c"

// *****************************************************************************
/* EXAMPLE_START(map_client_demo): Connect to MAP Messaging Server Equipment (MSE)
 *
 * @text Note: The Bluetooth address of the remote Phonbook server is hardcoded. 
 * Change it before running example, then use the UI to connect to it, to set and 
 * query contacts.
 */
// *****************************************************************************


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack_event.h"
#include "yxml.h"
#include "classic/goep_client.h"
#include "classic/goep_server.h"
#include "classic/map.h"
#include "classic/map_access_client.h"
#include "classic/map_notification_server.h"
#include "classic/map_util.h"
#include "classic/obex.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#include "btstack_debug.h"

#endif

#define MNS_SERVER_RFCOMM_CHANNEL_NR 1
#define MNS_SERVER_GOEP_PSM 0x1001

// forward declarations
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void stdin_process(char c);

// map access clients
static map_access_client_t map_access_client_mas_0;
static map_access_client_t map_access_client_mas_1;

#ifdef ENABLE_GOEP_L2CAP
// singleton instance
static uint8_t map_access_client_ertm_buffer_mas_0[4000];
static uint8_t map_access_client_ertm_buffer_mas_1[4000];
static l2cap_ertm_config_t map_access_client_ertm_config = {
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

// msn client connection
static uint16_t mns_cid;

static bd_addr_t    remote_addr;
static const char * remote_addr_string = "001BDC0732EF";

static const char* folders[] = { "inbox", "outbox", "draft", "delete", "", NULL};
static const char * *folder_name = folders;
static map_message_handle_t message_handle = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static map_message_handle_t message_handles[MAP_MESSAGE_TYPE_IM+1] = { 0, };
static map_conversation_id_t conv_id = { 0x00, };

//static const char * path = "telecom/msg/draft";
static const char* path = "telecom/msg";

static const char bmsg_email[] =
"BEGIN:BMSG\r\n"
"VERSION:1.0\r\n"
"STATUS:UNREAD\r\n"
"TYPE:EMAIL\r\n"
"FOLDER:TELECOM/MSG/draft\r\n"
"BEGIN:VCARD\r\n"
"VERSION:2.1\r\n"
"N:IUT\r\n"
"EMAIL:member@iut.com\r\n"
"END:VCARD\r\n"
"BEGIN:BENV\r\n"
"BEGIN:VCARD\r\n"
"VERSION:2.1\r\n"
"N:PTS\r\n"
"EMAIL:Iut_emailaddress\r\n"
"END:VCARD\r\n"
"BEGIN:BENV\r\n"
"BEGIN:VCARD\r\n"
"VERSION:2.1\r\n"
"N:PTS1\r\n"
"EMAIL:PTS1@bluetooth.com\r\n"
"END:VCARD\r\n"
"BEGIN:BBODY\r\n"
"ENCODING:8BIT\r\n"
"LENGTH:4975\r\n"
"BEGIN:MSG\r\n"
"Date:Mon, 14 Mar 2014 09:09:09\r\n"
"Subject:PTS testing\r\n"
"From:PTS@bluetooth.com\r\n"
"To:IUT@bluetooth.com\r\n"
"\r\n"
"PTS EMAIL message \r\n"
"\r\n"
"\r\n"
"END:MSG\r\n"
"END:BBODY\r\n"
"END:BENV\r\n"
"END:BENV\r\n"
"END:BMSG\r\n";

//"BEGIN:BMSG\r\n"
//"VERSION:1.0\r\n"
//"STATUS:UNREAD\r\n"
//"TYPE:EMAIL\r\n"
//"FOLDER:\r\n"
//"BEGIN:VCARD\r\n"
//"VERSION:2.1\r\n"
//"N:IUT\r\n"
//"EMAIL:member@iut.com\r\n"
//"END:VCARD\r\n"
//"BEGIN:BENV\r\n"
//"BEGIN:BBODY\r\n"
//"ENCODING:UTF-8\r\n"
//"LENGTH:4975\r\n"
//"BEGIN:MSG\r\n"
//"Date:20 Jun 96\r\n"
//"Subject:BTStack testing\r\n"
//"From:PTS@bluetooth.com\r\n"
//"To:IUT@bluetooth.com\r\n"
//"\r\n"
//"Hello World!\r\n"
//"\r\n"
//"END:MSG\r\n"
//"END:BBODY\r\n"
//"END:BENV\r\n"
//"END:BMSG\r\n";

static const char bmsg_IM[] =
"BEGIN:BMSG\r\n"
"VERSION:1.0\r\n"
"STATUS:UNREAD\r\n"
"TYPE:IM\r\n"
"FOLDER:TELECOM/MSG/DRAFT\r\n"
"BEGIN:VCARD\r\n"
"VERSION:2.1\r\n"
"N:IUT\r\n"
"EMAIL:member@iut.com\r\n"
"END:VCARD\r\n"
"BEGIN:BENV\r\n"
"BEGIN:BBODY\r\n"
"ENCODING:8BIT\r\n"
"LENGTH:4975\r\n"
"BEGIN:MSG\r\n"
"Date:20 Jun 96\r\n"
"Subject:BTStack testing\r\n"
"From:PTS@bluetooth.com\r\n"
"To:IUT@bluetooth.com\r\n"
"\r\n"
"Hello World!\r\n"
"\r\n"
"END:MSG\r\n"
"END:BBODY\r\n"
"END:BENV\r\n"
"END:BMSG\r\n";
;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint16_t map_mas_0_cid;
static uint16_t map_mas_1_cid;
static uint8_t notification_filter = 0;

// MNS SDP Record
static uint8_t  map_message_notification_service_buffer[150];
const char * name = "MAP Service";

// UI
static uint8_t map_mas_instance_id = 0;
static uint16_t map_cid;
static enum {
    MCE_DEMO_IDLE,
    MCE_DEMO_NOTIFICATION_ENABLE,
    MCE_DEMO_NOTIFICATION_DISABLE,
} map_mce_state = MCE_DEMO_IDLE;

 static struct test_case_s {
    int nr;
    const char *descr;
    const char *keysequ;
 } test_cases[] = {
    {.nr = 1, .descr = "MAP/MCE/MMD/BV-01-C", .keysequ = "apF1d2d4d5dAb3dB"},
    {.nr = 2, .descr = "MAP/MCE/MMD/BV-03-C", .keysequ = "anpF5d"},
    {.nr = 3, .descr = "MAP/MCE/MMU/BV-01-C", .keysequ = "apuA"},
};
static struct test_case_s *ptc = test_cases;
int keysequ_idx = 0;

static btstack_timer_source_t keypress_timer;
static void keypress_timer_cb(btstack_timer_source_t* ts) {
    char key = ptc->keysequ[keysequ_idx];
    // we wait 1s after each keypress. 3s for a connect attempts
    uint32_t timeout_ms = (key == 'a' || key == 'b' || key == 'n') ? 3000 : 1000;

    log_debug("Timer keypress <%c> %u ms", key, timeout_ms);
    stdin_process(key);
    
    // last key in sequence?
    if (ptc->keysequ[++keysequ_idx] == '\0') {
        keysequ_idx = 0;
        btstack_run_loop_remove_timer(ts);
        log_debug("Timer removed");
    // progress with next key in sequence
    } else {
        // Re-Arm Timer
        log_debug("Timer re-armed");
        btstack_run_loop_set_timer(ts, timeout_ms);
        btstack_run_loop_add_timer(ts);
    }
}
static init_keypress_timer(void) {
    btstack_run_loop_set_timer_handler(&keypress_timer, keypress_timer_cb);
    btstack_run_loop_set_timer(&keypress_timer, 10);
    btstack_run_loop_add_timer(&keypress_timer);
    log_debug("Timer set");
}

static void next_test_case(void) {
    // cycle throug all test cases
    if (++ptc >= &test_cases[ARRAYSIZE(test_cases)]) {
        ptc = &test_cases[0];
    }
    // init test cases
    keysequ_idx = 0;
}

static void previous_test_case(void) {
    // cycle throug all test cases
    if (--ptc < &test_cases[0]) {
        ptc = &test_cases[ARRAYSIZE(test_cases) - 1];
    }
    // init test cases
    keysequ_idx = 0;
}

void client_connect(int mas_id)
{
    uint16_t* pmap_mas_x_cid = mas_id == 0 ? &map_mas_0_cid : &map_mas_1_cid;
    map_access_client_t* pmap_access_client_mas_x = mas_id == 0 ? &map_access_client_mas_0 : &map_access_client_mas_1;
#ifdef ENABLE_GOEP_L2CAP
    uint8_t* pmap_access_client_ertm_buffer_mas_x = mas_id == 0 ? map_access_client_ertm_buffer_mas_0 : map_access_client_ertm_buffer_mas_1;
#endif
    if (*pmap_mas_x_cid == 0) {
        btprintf("[+] Connecting to MAS ID #%u of %s...\n", mas_id, bd_addr_to_str(remote_addr));
#ifdef ENABLE_GOEP_L2CAP
        map_access_client_connect(pmap_access_client_mas_x, &map_access_client_ertm_config,
            sizeof(map_access_client_ertm_buffer_mas_0),
            pmap_access_client_ertm_buffer_mas_x, &packet_handler, remote_addr, mas_id, pmap_mas_x_cid);
#else
        map_access_client_connect(pmap_access_client_mas_x, NULL, 0, NULL, &packet_handler, remote_addr, 0, pmap_mas_x_cid);
#endif
    }
    else {
        btprintf("[+] Switching to MAP ID #0, map_cid %u\n", *pmap_mas_x_cid);
    }
    map_mas_instance_id = mas_id;
    map_cid = *pmap_mas_x_cid;
}

#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    btprintf("\n--- Bluetooth MAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    if (map_cid != 0){
        btprintf("MAP instance ID #%u, map cid %u\n", map_mas_instance_id, map_cid);
    }
    btprintf("\n");
    if (map_mas_0_cid == 0){
        btprintf("a - establish connection to MAS ID #0 - %s\n", bd_addr_to_str(remote_addr));
    } else {
        btprintf("a - select MAS ID #0 - %s\n", bd_addr_to_str(remote_addr));
        btprintf("A - disconnect from MAS ID 0 - %s\n", bd_addr_to_str(remote_addr));
    }
    if (map_mas_1_cid == 0){
        btprintf("b - establish connection to MAS ID #1 - %s\n", bd_addr_to_str(remote_addr));
    } else {
        btprintf("b - select MAS ID #1 - %s\n", bd_addr_to_str(remote_addr));
        btprintf("B - disconnect from MAS ID 1 - %s\n", bd_addr_to_str(remote_addr));
    }
    btprintf("X - eXecute current test case key sequence (%d:%s %s)\n", ptc->nr, ptc->descr, ptc->keysequ);
    btprintf("x - cycle through test cases\n");
    btprintf("U - request an update on the inbox\n");
    btprintf("p - set path \'%s\'\n", path);
    btprintf("f - get folder listing\n");
    btprintf("F - get message listing for folder \'%s\'\n", *folder_name);
    btprintf("C - get conversation listing\n");
    btprintf("6 - Select last listed \"unknown\" message\n");
    btprintf("1 - Select last listed \"email\" message\n");
    btprintf("2 - Select last listed \"sms_gsm\" message\n");
    btprintf("3 - Select last listed \"sms_cdma\" message\n");
    btprintf("4 - Select last listed \"mms\" message\n");
    btprintf("5 - Select last listed \"im\" message\n");
    btprintf("g - Get selected message "); printf_hexdump(message_handle, sizeof(message_handle));
    btprintf("u - Upload (Push) message <%.20s>\n", bmsg_email);
    btprintf("r - Mark selected messages as read\n");
    btprintf("R - Mark selected messagee as unread\n");
    btprintf("d - Mark selected messagee as deleted\n");
    btprintf("n - enable notifications for all MAS\n");
    btprintf("N - disable notifications for all MAS\n");
    btprintf("m - toggle notification filter for new messages\n");
    btprintf("i - get MAS Instance Information\n");
    btprintf("t - disconnect MNS Client\n");
    btprintf("\n");
}

static void mce_demo_select_mas_instance(uint8_t instance){
    switch (instance){
        case 0:
            map_mas_instance_id = 0;
            map_cid = map_mas_0_cid;
            break;
        case 1:
            map_mas_instance_id = 1;
            map_cid = map_mas_1_cid;
            break;
        default:
            btstack_unreachable();
            break;
    }
}
static void stdin_process(char c){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (c){
    case 'x':
        next_test_case();
        show_usage();
        break;
    case 'X':
        init_keypress_timer();
        btprintf("[+] eXecute keypress sequence <%s>\n", ptc->keysequ);
        break;
    case 'a':            
        client_connect(0);
        mce_demo_select_mas_instance(0);
        break;
    case 'A':
        btprintf("[+] Disconnect MAP ID #0 from %s...\n", bd_addr_to_str(remote_addr));
        map_access_client_disconnect(map_mas_0_cid);
        map_mas_0_cid = 0;
        break;
    case 'b':
        client_connect(1);
        mce_demo_select_mas_instance(1);
        break;
    case 'B':
        btprintf("[+] Disconnect MAP ID #1 from %s...\n", bd_addr_to_str(remote_addr));
        map_access_client_disconnect(map_mas_1_cid);
        map_mas_1_cid = 0;
        break;
    case 'U':
        btprintf("[+] Requesting inbox update\n");
        map_access_client_update_inbox(map_cid);
        break;
    case 'p':
        btprintf("[+] Set path \'%s\'\n", path);
        map_access_client_set_path(map_cid, path);
        break;
    case 'P':
        if (*(++folder_name) == NULL)
            folder_name = folders;
        btprintf("[+] Set folder_name to \'%s\'\n", *folder_name);
        break;
    case 'f':
        btprintf("[+] Get folder listing\n");
        map_access_client_get_folder_listing(map_cid);
        break;
    case 'F':
        btprintf("[+] Get message listing for folder \'%s\'\n", *folder_name);
        map_access_client_get_message_listing_for_folder(map_cid, *folder_name);
        break;
    case 'C':
        btprintf("[+] Get conversation listing\n");
        map_access_client_get_conversation_listing(map_cid, 10, 0);
        break;
    case 'i':
        btprintf("[+] Get MAS instance info for default instance\n");
        map_access_client_get_mas_instance_info(map_cid, 0);
        break;
    case '0':
        btprintf("[+] Select last listed \"unknown\" message\n");
        memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_UNKNOWN], MAP_MESSAGE_HANDLE_SIZE);
        break;
    case '1':
        btprintf("[+] Select last listed \"email\" message\n");
        memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_EMAIL], MAP_MESSAGE_HANDLE_SIZE);
        break;
    case '2':
        btprintf("[+] Select last listed \"sms_gsm\" message\n");
        memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_SMS_GSM], MAP_MESSAGE_HANDLE_SIZE);
        break;
    case '3':
        btprintf("[+] Select last listed \"sms_cmda\" message\n");
        memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_SMS_CDMA], MAP_MESSAGE_HANDLE_SIZE);
        break;
    case '4':
        btprintf("[+] Select last listed \"mms\" message\n");
        memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_MMS], MAP_MESSAGE_HANDLE_SIZE);
        break;
    case '5':
        btprintf("[+] Select last listed \"im\" message\n");
        memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_IM], MAP_MESSAGE_HANDLE_SIZE);
        break;
    case 'g':
        btprintf("[+] Get selected message\n");
        map_access_client_get_message_with_handle(map_cid, message_handle, 1);
        break;
    case 'u':
        btprintf("[+] Upload (PUT/PUSH) message\n");
        map_access_client_push_message(map_cid, *folder_name, bmsg_email, (uint16_t)strlen(bmsg_email));
        break;
    case 'r':
        btprintf("[+] Mark selected messages as read\n");
        map_access_client_set_message_status(map_cid, message_handle, readStatus, yes);
        break;
    case 'R':
        btprintf("[+] Mark selected message as unread\n");
        map_access_client_set_message_status(map_cid, message_handle, readStatus, no);
        break;
    case 'd':
        btprintf("[+] Mark selected message as deleted\n");
        map_access_client_set_message_status(map_cid, message_handle, deletedStatus, yes);
        break;
    case 'n':
        // enable notifications for all/both mas instances
        mce_demo_select_mas_instance(0);
        map_mce_state = MCE_DEMO_NOTIFICATION_ENABLE;
        btprintf("[+] Enable notifications map_cid %u\n", map_cid);
        status = map_access_client_enable_notifications(map_cid);
        break;
    case 'N':
        // disable notifications for all/both mas instances
        mce_demo_select_mas_instance(0);
        map_mce_state = MCE_DEMO_NOTIFICATION_DISABLE;
        btprintf("[+] Disable notifications map_cid %u\n", map_cid);
        map_access_client_disable_notifications(map_cid);
        break;
    case 'm':
        notification_filter = !notification_filter;
        btprintf("[+] Notification filter for new messages %s\n", notification_filter ? "enabled" : "disabled");
        map_access_client_set_notification_filter(map_cid, notification_filter ? 0x01 : 0xffffffff);
        break;
    case 't':
        btprintf("[+] Disconnect MNS Client\n");
        map_notification_server_disconnect(mns_cid);
        break;
    default:
        show_usage();
        break;
    }
    if (status != ERROR_CODE_SUCCESS){
        btprintf("ERROR CODE 0x%02x!\n", status);
    }
}

// packet handler for interactive console
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    map_message_type_t msg_type;
    map_message_status_t msg_status;
    uint16_t cid;

    int value_len;
    char value[MAP_MAX_VALUE_LEN];
    memset(value, 0, MAP_MAX_VALUE_LEN);
    bd_addr_t event_addr;

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
                    btprintf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            status = map_subevent_connection_opened_get_status(packet);
                            if (status){
                                btprintf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                btprintf("[+] Connected map_cid %u\n", map_subevent_connection_opened_get_map_cid(packet));
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            cid = map_subevent_connection_closed_get_map_cid(packet);
                            if (cid == map_mas_0_cid){
                                btprintf("[+] Connection MAS #0 closed\n");
                                map_mas_0_cid = 0;
                            }
                            if (cid == map_mas_1_cid){
                                btprintf("[+] Connection MAS #1 closed\n");
                                map_mas_1_cid = 0;
                            }
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            btprintf("\n");
                            btprintf("[+] Operation complete, mas_cid %u\n", map_subevent_operation_completed_get_map_cid(packet));
                            // enable/disable notification for all/both MAS
                            switch (map_mas_instance_id){
                                case 0:
                                    switch (map_mce_state){
                                        case MCE_DEMO_NOTIFICATION_ENABLE:
                                            //mce_demo_select_mas_instance(0); // this enabled kills our active OBEX map_cid, disabled we send endless PUT requests for notification
                                            status = map_access_client_enable_notifications(map_cid);
                                            btprintf("[+] Enable notifications map_cid %u, status 0x%02x\n", map_cid, status);
                                            break;
                                        case MCE_DEMO_NOTIFICATION_DISABLE:
                                            //mce_demo_select_mas_instance(0); // this enabled kills our active OBEX map_cid, disabled we send endless PUT requests for notification
                                            status = map_access_client_disable_notifications(map_cid);
                                            btprintf("[+] Disable notifications map_cid %u, status 0x%02x\n", map_cid, status);
                                            break;
                                        default:
                                            break;
                                    }
                                    break;
                                case 1:
                                    map_mce_state = MCE_DEMO_IDLE;
                                    break;
                                default:
                                    btstack_unreachable();
                                    break;
                            }
                            break;
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            value_len = btstack_min(map_subevent_folder_listing_item_get_name_len(packet), MAP_MAX_VALUE_LEN);
                            memcpy(value, map_subevent_folder_listing_item_get_name(packet), value_len);
                            btprintf("Folder \'%s\'\n", value);
                            break;
                        case MAP_SUBEVENT_MESSAGE_LISTING_ITEM:
                            msg_type = map_subevent_message_listing_item_get_type (packet);
                            msg_status = map_subevent_message_listing_item_get_read (packet);
                            memcpy((uint8_t *) message_handle, map_subevent_message_listing_item_get_handle(packet), MAP_MESSAGE_HANDLE_SIZE);
                            memcpy((uint8_t *) message_handles[msg_type], message_handle, MAP_MESSAGE_HANDLE_SIZE);
                            btprintf("Message (%s / %s) handle: ",
                                   msg_type == MAP_MESSAGE_TYPE_EMAIL    ? "email   " :
                                   msg_type == MAP_MESSAGE_TYPE_SMS_GSM  ? "sms_gsm " :
                                   msg_type == MAP_MESSAGE_TYPE_SMS_CDMA ? "sms_cdma" :
                                   msg_type == MAP_MESSAGE_TYPE_MMS      ? "mms     " :
                                   msg_type == MAP_MESSAGE_TYPE_IM       ? "im      " :
                                   "unknown type",
                                   msg_status == MAP_MESSAGE_STATUS_UNREAD ? "unread" :
                                   msg_status == MAP_MESSAGE_STATUS_READ   ? "read  " :
                                   "unknown status");
                            printf_hexdump((uint8_t *) message_handle, MAP_MESSAGE_HANDLE_SIZE);
                            break;
                        case MAP_SUBEVENT_CONVERSATION_LISTING_ITEM:
                            memcpy((uint8_t *) conv_id, map_subevent_conversation_listing_item_get_id(packet), MAP_CONVERSATION_ID_SIZE);
                            btprintf ("Conversation: ");
                            printf_hexdump((uint8_t *) conv_id, MAP_CONVERSATION_ID_SIZE);
                            break;
                        case MAP_SUBEVENT_PARSING_DONE:
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case MAP_DATA_PACKET:
            char buf[100]; int i;
            for (i = 0; i < size; i++) {
                snprintf(&buf[3 * i], 4, "%02x ", packet[i]);
            }
            log_debug("MAP_DATA_PACKET (%u bytes): %s", size, buf);
            break;
        default:
            break;
    }
}
#endif

static void mns_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            mns_cid = map_subevent_connection_opened_get_map_cid(packet);
                            btprintf("MNS Client connected, mns_cid 0x04%x\n", mns_cid);
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            btprintf("MNS Client disconnected, mns_cid 0x04%x\n", mns_cid);
                            mns_cid = 0;
                            break;
                        case MAP_SUBEVENT_NOTIFICATION_EVENT:
                            btprintf("Notification!\n");
                            break;
                        default:
                            btprintf ("unknown map meta event %d\n", hci_event_map_meta_get_subevent_code(packet));
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case MAP_DATA_PACKET:
            char buf[100]; int i;
            for (i = 0; i < size; i++) {
                snprintf(&buf[3 * i], 4, "%02x ", packet[i]);
            }
            log_debug("MAP_DATA_PACKET (%u bytes): %s", size, buf);

            break;
        default:
            btprintf ("unknown event of type %d\n", packet_type);
            break;
    }
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
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sscanf_bd_addr(remote_addr_string, remote_addr);

    // setup MAP Notification Server

    // init MAP Access Client
    map_access_client_init();
    map_message_type_t supported_message_types = MAP_MESSAGE_TYPE_SMS_GSM;
    uint32_t supported_features = 0x7FFFFF;
    memset(map_message_notification_service_buffer, 0, sizeof(map_message_notification_service_buffer));
    map_util_create_notification_service_sdp_record(map_message_notification_service_buffer,
                                                    sdp_create_service_record_handle(),
                                                    1,
                                                    MNS_SERVER_RFCOMM_CHANNEL_NR,
                                                    MNS_SERVER_GOEP_PSM,
                                                    supported_message_types,
                                                    supported_features,
                                                    name);
    sdp_register_service(map_message_notification_service_buffer);
    map_notification_server_init(mns_packet_handler, MNS_SERVER_RFCOMM_CHANNEL_NR,  MNS_SERVER_GOEP_PSM, 0xffff);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif    

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
