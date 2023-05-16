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
#endif

#define MNS_SERVER_RFCOMM_CHANNEL_NR 1
#define MNS_SERVER_GOEP_PSM 0x1001

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// singleton instance
static map_access_client_t map_access_client;

#ifdef ENABLE_GOEP_L2CAP
// singleton instance
static uint8_t map_access_client_ertm_buffer[1000];
static l2cap_ertm_config_t map_access_client_ertm_config = {
        1,  // ertm mandatory
        2,  // max transmit, some tests require > 1
        2000,
        12000,
        512,    // l2cap ertm mtu
        2,
        2,
        1,      // 16-bit FCS
};
#endif


static bd_addr_t    remote_addr;
static uint16_t rfcomm_channel_id;
// MBP2016 "F4-0F-24-3B-1B-E1"
// Nexus 7 "30-85-A9-54-2E-78"
// iPhone SE "BC:EC:5D:E6:15:03"
// PTS "001BDC080AA5"
// iPhone 5 static  char * remote_addr_string = "6C:72:E7:10:22:EE";
// Android
static const char * remote_addr_string = "008098090B32";

static const char * folder_name = "inbox";
static map_message_handle_t message_handle = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static map_message_handle_t message_handles[MAP_MESSAGE_TYPE_IM+1] = { 0, };

static const char * path = "telecom/msg";

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t map_cid;
static uint8_t notification_filter = 0;

#ifdef HAVE_BTSTACK_STDIN
// Testig User Interface 
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth MAP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("a - establish MAP connection to %s\n", bd_addr_to_str(remote_addr));
    printf("A - disconnect from %s\n", bd_addr_to_str(remote_addr));
    printf("p - set path \'%s\'\n", path);
    printf("f - get folder listing\n");
    printf("F - get message listing for folder \'%s\'\n", folder_name);
    printf("0 - Get last listed \"unknown\" message\n");
    printf("1 - Get last listed \"email\" message\n");
    printf("2 - Get last listed \"sms_gsm\" message\n");
    printf("3 - Get last listed \"sms_cdma\" message\n");
    printf("4 - Get last listed \"mms\" message\n");
    printf("5 - Get last listed \"im\" message\n");
    printf("n - enable notifications\n");
    printf("N - disable notifications\n");
    printf("m - toggle notification filter for new messages\n");
    printf("i - get MAS Instance Information\n");

    printf("\n");
}

static void stdin_process(char c){
    switch (c){
        case 'a':
            printf("[+] Connecting to %s...\n", bd_addr_to_str(remote_addr));
#ifdef ENABLE_GOEP_L2CAP
            map_access_client_connect(&map_access_client, &map_access_client_ertm_config,
                                      sizeof(map_access_client_ertm_buffer),
                                      map_access_client_ertm_buffer, &packet_handler, remote_addr, 0, &map_cid);
#else
            map_access_client_connect(&map_access_client, NULL, 0, NULL, &packet_handler, remote_addr, 0, &map_cid);
#endif
            break;
        case 'A':
            printf("[+] Disconnect from %s...\n", bd_addr_to_str(remote_addr));
            map_access_client_disconnect(map_cid);
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
        case 'i':
            printf("[+] Get MAS instance info for default instance\n");
            map_access_client_get_mas_instance_info(map_cid, 0);
            break;
        case '0':
            printf("[+] Get last listed \"unknown\" message\n");
            map_access_client_get_message_with_handle(map_cid, message_handles[MAP_MESSAGE_TYPE_UNKNOWN], 1);
            break;
        case '1':
            printf("[+] Get last listed \"email\" message\n");
            map_access_client_get_message_with_handle(map_cid, message_handles[MAP_MESSAGE_TYPE_EMAIL], 1);
            break;
        case '2':
            printf("[+] Get last listed \"sms_gsm\" message\n");
            map_access_client_get_message_with_handle(map_cid, message_handles[MAP_MESSAGE_TYPE_SMS_GSM], 1);
            break;
        case '3':
            printf("[+] Get last listed \"sms_cmda\" message\n");
            map_access_client_get_message_with_handle(map_cid, message_handles[MAP_MESSAGE_TYPE_SMS_CDMA], 1);
            break;
        case '4':
            printf("[+] Get last listed \"mms\" message\n");
            map_access_client_get_message_with_handle(map_cid, message_handles[MAP_MESSAGE_TYPE_MMS], 1);
            break;
        case '5':
            printf("[+] Get last listed \"im\" message\n");
            map_access_client_get_message_with_handle(map_cid, message_handles[MAP_MESSAGE_TYPE_IM], 1);
            break;
        case 'n':
            printf("[+] Enable notifications\n");
            map_access_client_enable_notifications(map_cid);
            break;
        case 'N':
            printf("[+] Disable notifications\n");
            map_access_client_disable_notifications(map_cid);
            break;
        case 'm':
            notification_filter = !notification_filter;
            printf("[+] Notification filter for new messages %s\n", notification_filter ? "enabled" : "disabled");
            map_access_client_set_notification_filter(map_cid, notification_filter ? 0x01 : 0xffffffff);
            break;
        default:
            show_usage();
            break;
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

                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    printf("RFCOMM channel closed\n");
                    rfcomm_channel_id = 0;
                    break;

                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            status = map_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                printf("[+] Connected\n");
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            printf("[+] Connection closed\n");
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            printf("\n");
                            printf("[+] Operation complete\n");
                            break;
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            value_len = btstack_min(map_subevent_folder_listing_item_get_name_len(packet), MAP_MAX_VALUE_LEN);
                            memcpy(value, map_subevent_folder_listing_item_get_name(packet), value_len);
                            printf("Folder \'%s\'\n", value);
                            break;
                        case MAP_SUBEVENT_MESSAGE_LISTING_ITEM:
                            msg_type = map_subevent_message_listing_item_get_type (packet);
                            msg_status = map_subevent_message_listing_item_get_read (packet);
                            memcpy((uint8_t *) message_handle, map_subevent_message_listing_item_get_handle(packet), MAP_MESSAGE_HANDLE_SIZE);
                            memcpy((uint8_t *) message_handles[msg_type], message_handle, MAP_MESSAGE_HANDLE_SIZE);
                            printf("Message (%s, %s) handle: ",
                                   msg_type == MAP_MESSAGE_TYPE_EMAIL    ? "email" :
                                   msg_type == MAP_MESSAGE_TYPE_SMS_GSM  ? "sms_gsm" :
                                   msg_type == MAP_MESSAGE_TYPE_SMS_CDMA ? "sms_cdma" :
                                   msg_type == MAP_MESSAGE_TYPE_MMS      ? "mms" :
                                   msg_type == MAP_MESSAGE_TYPE_IM       ? "im" :
                                   "unknown type",
                                   msg_status == MAP_MESSAGE_STATUS_UNREAD ? "unread" :
                                   msg_status == MAP_MESSAGE_STATUS_READ   ? "read" :
                                   "unknown status");
                            printf_hexdump((uint8_t *) message_handle, MAP_MESSAGE_HANDLE_SIZE);
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
                        case MAP_SUBEVENT_NOTIFICATION_EVENT:
                            printf("Notification!\n");
                            break;
                        default:
                            printf ("unknown map meta event %d\n",
                                    hci_event_map_meta_get_subevent_code(packet));
                            break;
                    }
                    break;
                default:
                    printf ("unknown HCI event %d\n",
                            hci_event_packet_get_type(packet));
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

static uint8_t  map_message_notification_service_buffer[150];
const char * name = "MAP Service";

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

    // init MAP Access Client
    map_access_client_init();

    // setup MAP Notification Server
    map_message_type_t supported_message_types = MAP_MESSAGE_TYPE_SMS_GSM;
    uint32_t supported_features = 0x1F;
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
