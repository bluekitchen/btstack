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

#define __BTSTACK_FILE__ "map_client_test.c"

// *****************************************************************************
/* EXAMPLE_START(pbap_client_demo): Connect to Phonebook Server and get contacts.
 *
 * @text Note: The Bluetooth address of the remote Phonbook server is hardcoded. 
 * Change it before running example, then use the UI to connect to it, to set and 
 * query contacts.
 */
// *****************************************************************************


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluetooth_sdp.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "classic/goep_client.h"
#include "classic/obex.h"
#include "classic/rfcomm.h"
#include "classic/sdp_client.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "l2cap.h"
#include "map_client.h"
#include "map_server.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

static const uint8_t map_client_notification_service_uuid[] = {0xbb, 0x58, 0x2b, 0x41, 0x42, 0xc, 0x11, 0xdb, 0xb0, 0xde, 0x8, 0x0, 0x20, 0xc, 0x9a, 0x66};

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static bd_addr_t    remote_addr;
static uint16_t rfcomm_channel_id;
// MBP2016 "F4-0F-24-3B-1B-E1"
// Nexus 7 "30-85-A9-54-2E-78"
// iPhone SE "BC:EC:5D:E6:15:03"
// PTS "001BDC080AA5"
// iPhone 5 static  char * remote_addr_string = "6C:72:E7:10:22:EE";
// Android
static const char * remote_addr_string = "a0:28:ed:04:33:b0";
static const char * folder_name = "inbox";
static map_message_handle_t message_handle = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

static const char * path = "telecom/msg";

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t map_cid;

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
    printf("l - get message for last found handle\n");
    printf("n - enable notifications\n");
    
    printf("\n");
}

static void stdin_process(char c){
    switch (c){
        case 'a':
            printf("[+] Connecting to %s...\n", bd_addr_to_str(remote_addr));
            map_client_connect(&packet_handler, remote_addr, &map_cid);
            break;
        case 'A':
            printf("[+] Disconnect from %s...\n", bd_addr_to_str(remote_addr));
            map_client_disconnect(map_cid);
            break;

        case 'p':
            printf("[+] Set path \'%s\'\n", path);
            map_client_set_path(map_cid, path);
            break;
        case 'f':
            printf("[+] Get folder listing\n");
            map_client_get_folder_listing(map_cid);
            break;
        case 'F':
            printf("[+] Get message listing for folder \'%s\'\n", folder_name);
            map_client_get_message_listing_for_folder(map_cid, folder_name);
            break;
        case 'l':
            printf("[+] Get message for hardcoded handle\n");
            map_client_get_message_with_handle(map_cid, message_handle, 1);
            break;
        case 'n':
            printf("[+] Enable notifications\n");
            map_client_enable_notifications(map_cid);
            break;
        default:
            show_usage();
            break;
    }
}

static void obex_server_success_response(uint16_t rfcomm_cid){
    uint8_t event[30];
    int pos = 0;
    event[pos++] = OBEX_RESP_SUCCESS;
    // store len
    pos += 2;
    // obex version num
    event[pos++] = OBEX_VERSION;
    // flags
    // Bit 0 should be used by the receiving client to decide how to multiplex operations 
    // to the server (should it desire to do so). If the bit is 0 the client should serialize 
    // the operations over a single TTP connection. If the bit is set the client is free to 
    // establish multiple TTP connections to the server and concurrently exchange its objects.
    event[pos++] = 0;
    
    // Maximum OBEX packet length
    big_endian_store_16(event, pos, 0x0400);
    pos += 2;
    
    event[pos++] = OBEX_HEADER_CONNECTION_ID;
    big_endian_store_32(event, pos, 0x1234); 
    pos += 4;

    event[pos++] = OBEX_HEADER_WHO;
    big_endian_store_16(event, pos, 16 + 3);
    pos += 2;
    memcpy(event+pos, map_client_notification_service_uuid, 16);
    pos += 16;

    big_endian_store_16(event, 1, pos);
    rfcomm_send(rfcomm_cid, event, pos);
}

// packet handler for interactive console
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int i;
    uint8_t status;
    uint16_t  mtu;
    
    int value_len;
    char value[MAP_MAX_VALUE_LEN];
    memset(value, 0, MAP_MAX_VALUE_LEN);

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        show_usage();
                    }
                    break;

                 case RFCOMM_EVENT_INCOMING_CONNECTION:
                    rfcomm_channel_id = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
                    printf("RFCOMM connection opened\n");
                    rfcomm_accept_connection(rfcomm_channel_id);
                    break;
               
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    // data: event(8), len(8), status (8), address (48), server channel(8), rfcomm_cid(16), max frame size(16)
                    if (rfcomm_event_channel_opened_get_status(packet)) {
                        printf("RFCOMM channel open failed, status %u\n", rfcomm_event_channel_opened_get_status(packet));
                    } else {
                        rfcomm_channel_id = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                        mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                        printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_channel_id, mtu);
                    }
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
                            printf("[+] Operation complete\n");
                            break;
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            value_len = btstack_min(map_subevent_folder_listing_item_get_name_len(packet), MAP_MAX_VALUE_LEN);
                            memcpy(value, map_subevent_folder_listing_item_get_name(packet), value_len);
                            printf("Folder \'%s\'\n", value);
                            break;
                        case MAP_SUBEVENT_MESSAGE_LISTING_ITEM:
                            memcpy((uint8_t *) message_handle, map_subevent_message_listing_item_get_handle(packet), MAP_MESSAGE_HANDLE_SIZE);
                            printf("Message handle: ");
                            printf_hexdump((uint8_t *) message_handle, MAP_MESSAGE_HANDLE_SIZE);
                            printf("\n");
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
   
        case RFCOMM_DATA_PACKET:
            printf("RFCOMM data packet: '");
            for (i=0;i<size;i++){
                printf("%02x ", packet[i]);
            }
            printf("'\n"); 
            obex_server_success_response(rfcomm_channel_id);
            break;

        case MAP_DATA_PACKET:
            for (i=0;i<size;i++){
                printf("%c", packet[i]);
            }
            printf("\n");
            break;
        default:
            break;
    }
}
#endif

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

    // init GOEP Client
    goep_client_init();

    // init MAP Client
    map_client_init();
    // init MAP Server
    map_server_init();

    sdp_init();
    // setup AVDTP sink
    map_message_type_t supported_message_types = MAP_MESSAGE_TYPE_SMS_GSM;
    uint32_t supported_features = 0x1F;

    int rfcomm_channel_nr = 1;

    memset(map_message_notification_service_buffer, 0, sizeof(map_message_notification_service_buffer));
    map_message_notification_service_create_sdp_record(map_message_notification_service_buffer, 0x10001,
            1, rfcomm_channel_nr, 1, supported_message_types, supported_features, name);
    sdp_register_service(map_message_notification_service_buffer);

    rfcomm_register_service(&packet_handler, rfcomm_channel_nr, 0xFFFF);
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sscanf_bd_addr(remote_addr_string, remote_addr);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif    

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
