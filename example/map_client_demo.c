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

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

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
static const char * remote_addr_string = "001BDC08E25C";

static const char * folder_name = "inbox";
static map_message_handle_t message_handle = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static map_message_handle_t message_handles[MAP_MESSAGE_TYPE_IM+1] = { 0, };
static map_conversation_id_t conv_id = { 0x00, };

static const char * path = "telecom/msg";

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

#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth MAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    if (map_cid != 0){
        printf("MAP instance ID #%u, map cid %u\n", map_mas_instance_id, map_cid);
    }
    printf("\n");
    if (map_mas_0_cid == 0){
        printf("a - establish connection to MAS ID #0 - %s\n", bd_addr_to_str(remote_addr));
    } else {
        printf("a - select MAS ID #0 - %s\n", bd_addr_to_str(remote_addr));
        printf("A - disconnect from MAS ID 0 - %s\n", bd_addr_to_str(remote_addr));
    }
    if (map_mas_1_cid == 0){
        printf("b - establish connection to MAS ID #1 - %s\n", bd_addr_to_str(remote_addr));
    } else {
        printf("b - select MAS ID #1 - %s\n", bd_addr_to_str(remote_addr));
        printf("B - disconnect from MAS ID 1 - %s\n", bd_addr_to_str(remote_addr));
    }
    printf("U - request an update on the inbox\n");
    printf("p - set path \'%s\'\n", path);
    printf("f - get folder listing\n");
    printf("F - get message listing for folder \'%s\'\n", folder_name);
    printf("C - get conversation listing\n");
    printf("6 - Select last listed \"unknown\" message\n");
    printf("1 - Select last listed \"email\" message\n");
    printf("2 - Select last listed \"sms_gsm\" message\n");
    printf("3 - Select last listed \"sms_cdma\" message\n");
    printf("4 - Select last listed \"mms\" message\n");
    printf("5 - Select last listed \"im\" message\n");
    printf("g - Get selected message "); printf_hexdump(message_handle, sizeof(message_handle));
    printf("r - Mark selected messages as read\n");
    printf("u - Mark selected message as unread\n");
    printf("n - enable notifications for all MAS\n");
    printf("N - disable notifications for all MAS\n");
    printf("m - toggle notification filter for new messages\n");
    printf("i - get MAS Instance Information\n");
    printf("t - disconnect MNS Client\n");
    printf("\n");
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
        case 'a':
            if (map_mas_0_cid == 0){
                printf("[+] Connecting to MAS ID #0 of %s...\n", bd_addr_to_str(remote_addr));
#ifdef ENABLE_GOEP_L2CAP
                map_access_client_connect(&map_access_client_mas_0, &map_access_client_ertm_config,
                                          sizeof(map_access_client_ertm_buffer_mas_0),
                                          map_access_client_ertm_buffer_mas_0, &packet_handler, remote_addr, 0, &map_mas_0_cid);
#else
                map_access_client_connect(&map_access_client_mas_0, NULL, 0, NULL, &packet_handler, remote_addr, 0, &map_mas_0_cid);
#endif
            } else {
                printf("[+] Switching to MAP ID #0, map_cid %u\n", map_mas_0_cid);
            }
            mce_demo_select_mas_instance(0);
            break;
        case 'A':
            printf("[+] Disconnect MAP ID #0 from %s...\n", bd_addr_to_str(remote_addr));
            map_access_client_disconnect(map_mas_0_cid);
            map_mas_0_cid = 0;
            break;

        case 'b':
            if (map_mas_1_cid == 0) {
                printf("[+] Connecting to MAS ID #1 of %s...\n", bd_addr_to_str(remote_addr));
#ifdef ENABLE_GOEP_L2CAP
                map_access_client_connect(&map_access_client_mas_1, &map_access_client_ertm_config,
                                          sizeof(map_access_client_ertm_buffer_mas_1),
                                          map_access_client_ertm_buffer_mas_1, &packet_handler, remote_addr, 1,
                                          &map_mas_1_cid);
#else
                map_access_client_connect(&map_access_client_mas_1, NULL, 0, NULL, &packet_handler, remote_addr, 1, &map_mas_1_cid);
#endif
            } else {
                printf("[+] Switching to MAP ID #1, map_cid %u\n", map_mas_1_cid);
            }
            mce_demo_select_mas_instance(1);
            break;
        case 'B':
            printf("[+] Disconnect MAP ID #1 from %s...\n", bd_addr_to_str(remote_addr));
            map_access_client_disconnect(map_mas_1_cid);
            map_mas_1_cid = 0;
            break;

        case 'U':
            printf("[+] Requesting inbox update\n");
            map_access_client_update_inbox(map_cid);
            break;

        case 'p':
            printf("[+] Set path \'%s\'\n", path);
            map_access_client_set_path(map_cid, path);
            break;
        case 'f':
            printf("[+] Get folder listing\n");
            map_access_client_get_folder_listing(map_cid);
            break;
        case 'F':
            printf("[+] Get message listing for folder \'%s\'\n", folder_name);
            map_access_client_get_message_listing_for_folder(map_cid, folder_name);
            break;
        case 'C':
            printf("[+] Get conversation listing\n");
            map_access_client_get_conversation_listing(map_cid, 10, 0);
            break;
        case 'i':
            printf("[+] Get MAS instance info for default instance\n");
            map_access_client_get_mas_instance_info(map_cid, 0);
            break;
        case '0':
            printf("[+] Select last listed \"unknown\" message\n");
            memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_UNKNOWN], MAP_MESSAGE_HANDLE_SIZE);
            break;
        case '1':
            printf("[+] Select last listed \"email\" message\n");
            memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_EMAIL], MAP_MESSAGE_HANDLE_SIZE);
            break;
        case '2':
            printf("[+] Select last listed \"sms_gsm\" message\n");
            memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_SMS_GSM], MAP_MESSAGE_HANDLE_SIZE);
            break;
        case '3':
            printf("[+] Select last listed \"sms_cmda\" message\n");
            memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_SMS_CDMA], MAP_MESSAGE_HANDLE_SIZE);
            break;
        case '4':
            printf("[+] Select last listed \"mms\" message\n");
            memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_MMS], MAP_MESSAGE_HANDLE_SIZE);
            break;
        case '5':
            printf("[+] Select last listed \"im\" message\n");
            memcpy((uint8_t *) message_handle, message_handles[MAP_MESSAGE_TYPE_IM], MAP_MESSAGE_HANDLE_SIZE);
            break;
        case 'g':
            printf("[+] Get selected message\n");
            map_access_client_get_message_with_handle(map_cid, message_handle, 1);
            break;
        case 'r':
            printf("[+] Mark selected messages as read\n");
            map_access_client_set_message_status(map_cid, message_handle, 1);
            break;
        case 'u':
            printf("[+] Mark selected message as unread\n");
            map_access_client_set_message_status(map_cid, message_handle, 0);
            break;
        case 'n':
            // enable notifications for all/both mas instances
            mce_demo_select_mas_instance(0);
            map_mce_state = MCE_DEMO_NOTIFICATION_ENABLE;
            printf("[+] Enable notifications map_cid %u\n", map_cid);
            status = map_access_client_enable_notifications(map_cid);
            break;
        case 'N':
            // disable notifications for all/both mas instances
            mce_demo_select_mas_instance(0);
            map_mce_state = MCE_DEMO_NOTIFICATION_DISABLE;
            printf("[+] Disable notifications map_cid %u\n", map_cid);
            map_access_client_disable_notifications(map_cid);
            break;
        case 'm':
            notification_filter = !notification_filter;
            printf("[+] Notification filter for new messages %s\n", notification_filter ? "enabled" : "disabled");
            map_access_client_set_notification_filter(map_cid, notification_filter ? 0x01 : 0xffffffff);
            break;
        case 't':
            printf("[+] Disconnect MNS Client\n");
            map_notification_server_disconnect(mns_cid);
            break;
        default:
            show_usage();
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("ERROR CODE 0x%02x!\n", status);
    }
}

// packet handler for interactive console
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int i;
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
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    gap_pin_code_response(event_addr, "0000");
                    break;

                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            status = map_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                printf("[+] Connected map_cid %u\n", map_subevent_connection_opened_get_map_cid(packet));
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            cid = map_subevent_connection_closed_get_map_cid(packet);
                            if (cid == map_mas_0_cid){
                                printf("[+] Connection MAS #0 closed\n");
                                map_mas_0_cid = 0;
                            }
                            if (cid == map_mas_1_cid){
                                printf("[+] Connection MAS #1 closed\n");
                                map_mas_1_cid = 0;
                            }
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            printf("\n");
                            printf("[+] Operation complete, mas_cid %u\n", map_subevent_operation_completed_get_map_cid(packet));
                            // enable/disable notification for all/both MAS
                            switch (map_mas_instance_id){
                                case 0:
                                    switch (map_mce_state){
                                        case MCE_DEMO_NOTIFICATION_ENABLE:
                                            mce_demo_select_mas_instance(1);
                                            status = map_access_client_enable_notifications(map_cid);
                                            printf("[+] Enable notifications map_cid %u, status 0x%02x\n", map_cid, status);
                                            break;
                                        case MCE_DEMO_NOTIFICATION_DISABLE:
                                            mce_demo_select_mas_instance(1);
                                            status = map_access_client_disable_notifications(map_cid);
                                            printf("[+] Disable notifications map_cid %u, status 0x%02x\n", map_cid, status);
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
                            printf("Folder \'%s\'\n", value);
                            break;
                        case MAP_SUBEVENT_GET_MESSAGE_LISTING:
                            msg_type = map_subevent_message_listing_item_get_type (packet);
                            msg_status = map_subevent_message_listing_item_get_read (packet);
                            memcpy((uint8_t *) message_handle, map_subevent_message_listing_item_get_handle(packet), MAP_MESSAGE_HANDLE_SIZE);
                            memcpy((uint8_t *) message_handles[msg_type], message_handle, MAP_MESSAGE_HANDLE_SIZE);
                            printf("Message (%s / %s) handle: ",
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
                            printf ("Conversation: ");
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
            for (i=0;i<size;i++){
                printf("%c", packet[i]);
            }
            break;
        default:
            break;
    }
}
#endif

static void mns_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    int i;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            mns_cid = map_subevent_connection_opened_get_map_cid(packet);
                            printf("MNS Client connected, mns_cid 0x04%x\n", mns_cid);
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            printf("MNS Client disconnected, mns_cid 0x04%x\n", mns_cid);
                            mns_cid = 0;
                            break;
                        case MAP_SUBEVENT_NOTIFICATION_EVENT:
                            printf("Notification!\n");
                            break;
                        default:
                            printf ("unknown map meta event %d\n", hci_event_map_meta_get_subevent_code(packet));
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case MAP_DATA_PACKET:
            for (i=0;i<size;i++){
                printf("%c", packet[i]);
            }
            printf ("\n");
            break;
        default:
            printf ("unknown event of type %d\n", packet_type);
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
