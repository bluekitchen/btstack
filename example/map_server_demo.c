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
#include "classic/map_access_client.h"
#include "classic/map_notification_server.h"
#include "classic/map_util.h"
#include "classic/obex.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
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

static const char * msg_listing_header = "<MAP-msg-listing version = \"1.0\">";
static const char * msg_listing_msg   = "<msg handle = \"20000100001\" subject = \"Hello\" datetime = \"20071213T130510\" sender_name = \"Jamie\" \
sender_addressing = \"+1-987-6543210\" recipient_addressing = \"+1-0123-456789\" type = \"SMS_GSM\"  \
size = \"256\" attachment_size = \"0\" priority = \"no\" read = \"yes\" sent = \"no\" protected = \"no\"/>";
static const char * msg_listing_footer = "</MAP-msg-listing>";

static void create_msg(char * msg_buffer, uint16_t index){
    sprintf(msg_buffer, msg_listing_msg);
}


// send msgs first-last, returns index of next card
static uint16_t send_msgs(uint16_t phonebook_index, uint16_t first, uint16_t last){
    uint16_t max_body_size = map_server_get_max_body_size(map_cid);
    char msg_buffer[200];
    uint16_t pos = 0;
    while ((max_body_size > 0) && (first <= last)){
        // create msg
        create_msg(msg_buffer, first);
        // get len
        uint16_t len = strlen(msg_buffer);
        if (len > max_body_size){
            break;
        }
        memcpy(&upload_buffer[pos], (const uint8_t *) msg_buffer, len);
        pos += len;
        max_body_size -= len;
        first++;
    }
    uint8_t response_code = (first > last) ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE;
    map_server_send_pull_response(map_cid, response_code, first, pos, upload_buffer);
    return first;
}

static uint16_t send_listing(uint16_t phonebook_index, bool header, uint16_t first, uint16_t last) {
    uint16_t max_body_size = map_server_get_max_body_size(map_cid);
    char listing_buffer[200];
    uint16_t pos = 0;
    bool done = false;
    // add header
    if (first == 0){
        uint16_t len = strlen(msg_listing_header);
        memcpy(upload_buffer, msg_listing_header, len);
        pos += len;
        max_body_size -= len;
    }
    while ((max_body_size > 0) && (first <= last)){
        // add entry
        create_msg(listing_buffer, first);
        // get len
        uint16_t len = strlen(listing_buffer);
        if (len > max_body_size){
            break;
        }
        memcpy(&upload_buffer[pos], (const uint8_t *) listing_buffer, len);
        pos += len;
        max_body_size -= len;
        first++;
    }

    if (first > last){
        uint16_t len = (uint16_t) strlen(msg_listing_footer);
        if (len < max_body_size){
            memcpy(&upload_buffer[pos], (const uint8_t *) msg_listing_footer, len);
            pos += len;
            max_body_size -= len;
            done = true;
        }
    }

    uint8_t response_code = done ? OBEX_RESP_SUCCESS : OBEX_RESP_CONTINUE;
    uint32_t continuation = done ? 0x10000 : first;
    map_server_send_pull_response(map_cid, response_code, continuation, pos, upload_buffer);
    return first;

}

static btstack_packet_callback_registration_t hci_event_callback_registration;

const static char* test_message =
"BEGIN:BMSG\n"
"VERSION:1.0\n"
"STATUS:UNREAD\n"
"TYPE:SMS_GSM\n"
"FOLDER:telecom/msg/INBOX\n"
"BEGIN:VCARD\n"
"VERSION:3.0\n"
"FN:\n"
"N:\n"
"TEL:Swisscom\n"
"END:VCARD\n"
"BEGIN:BENV\n"
"BEGIN:BBODY\n"
"CHARSET:UTF-8\n"
"LENGTH:172\n"
"BEGIN:MSG\n"
"This is a test message!\n"
"END:MSG\n"
"END:BBODY\n"
"END:BENV\n"
"END:BMSG\n";


#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth MAP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c: open GEOP connection to MNS (in PTS)\n");
    printf("Command list goes here... try press 'a'\n");
    printf("\n");
}

static void stdin_process(char c){
    switch (c){
        case 'a':
            printf("a was pressed\n");
            break;
        case 'c':
            printf("a was pressed\n");
            break;
        default:
            show_usage();
            break;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int i;
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

static void mas_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    int  phonebook_index;
    const char * handle;
    uint32_t continuation;
    uint16_t total_msgs;
    uint16_t start_index;
    uint16_t num_msgs_selected;
    uint16_t max_list_count;
	
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_MAP_META:
                    switch (hci_event_map_meta_get_subevent_code(packet)){
                        case MAP_SUBEVENT_NOTIFICATION_EVENT:
                            log_info("Notification!");
                            break;
                        case MAP_SUBEVENT_CONNECTION_OPENED:
                            status = map_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                map_cid = map_subevent_connection_opened_get_map_cid(packet);
                                printf("[+] Connected map_cid 0x%04x\n", map_cid);
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            printf("[+] Connection closed\n");
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            printf("[+] Operation complete, status 0x%02x\n",
                                   map_subevent_operation_completed_get_status(packet));
                            break;
                        case PBAP_SUBEVENT_PULL_PHONEBOOK:
                            // get start index
                            map_phonebook = map_subevent_pull_phonebook_get_phonebook(packet);
                            phonebook_index = map_phonebook - PBAP_PHONEBOOK_TELECOM_CCH;
                            continuation = (uint16_t) map_subevent_pull_phonebook_get_continuation(packet);
                            if (continuation == 0){
                                // setup headers
                                map_server_set_database_identifier(map_cid, database_identifier);
                                map_server_set_primary_folder_version(map_cid, primary_folder_version);
                                map_server_set_secondary_folder_version(map_cid, secondary_folder_version);
                                // set new missed calls
                                if (phonebooks[phonebook_index].missed_calls || (phonebooks[phonebook_index].enhanced_missed_calls && enhanced_missed_calls_supported)){
                                    printf("[-] new missed calls = %u\n", number_missed_calls);
                                    map_server_set_new_missed_calls(map_cid, number_missed_calls);
                                }
                            }
                            // send vcard
                            total_cards = phonebooks[phonebook_index].num_cards;
                            start_index = map_subevent_pull_phonebook_get_list_start_offset(packet);
                            num_cards_selected = total_cards - start_index;
                            max_list_count = map_subevent_pull_phonebook_get_max_list_count(packet);
                            if (max_list_count < 0xffff){
                                num_cards_selected = btstack_min(max_list_count, num_cards_selected);
                            }
                            printf("[+] Pull phonebook %d - list offset %u, num cards %u\n", phonebook_index, start_index, num_cards_selected);
                            // consider already sent cards
                            num_cards_selected -= continuation;
                            start_index += continuation;
                            uint16_t end_index = start_index + num_cards_selected - 1;
                            printf("[-] continuation %u, num cards %u, start index %u, end index %u\n", continuation, num_cards_selected, start_index, end_index);
                            send_vcards(phonebook_index, start_index, end_index);
                            break;
							
                        case MAP_SUBEVENT_MESSAGE_LISTING_ITEM:
                            map_phonebook = map_subevent_pull_msg_listing_get_phonebook(packet);
                            phonebook_index = map_phonebook - MAP_PHONEBOOK_TELECOM_CCH;
                            continuation = (uint16_t) map_subevent_pull_msg_listing_get_continuation(packet);
                            if (continuation == 0){
                                // setup headers on first response
                                map_server_set_database_identifier(map_cid, database_identifier);
                                map_server_set_primary_folder_version(map_cid, primary_folder_version);
                                map_server_set_secondary_folder_version(map_cid, secondary_folder_version);
                                // set new missed calls
                                if (phonebooks[phonebook_index].missed_calls || (phonebooks[phonebook_index].enhanced_missed_calls && enhanced_missed_calls_supported)){
                                    printf("[-] new missed calls = %u\n", number_missed_calls);
                                    map_server_set_new_missed_calls(map_cid, number_missed_calls);
                                }
                            }
                            // send msg listing
                            total_msgs = phonebooks[phonebook_index].num_msgs;
                            start_index = map_subevent_pull_msg_listing_get_list_start_offset(packet);
                            num_msgs_selected = total_msgs - start_index;
                            max_list_count = map_subevent_pull_msg_listing_get_max_list_count(packet);
                            if (max_list_count < 0xffff){
                                num_msgs_selected = btstack_min(max_list_count, num_msgs_selected);
                            }
                            printf("[+] Pull vCard listing %d - list offset %u, num cards %u\n", phonebook_index, start_index, num_msgs_selected);
                            // consider already sent cards
                            if (continuation > 0xffff){
                                // just missed the footer
                                start_index = 0xffff;
                                num_msgs_selected  = 0;
                            } else {
                                num_msgs_selected -= continuation;
                                start_index += continuation;
                            }
                            end_index = start_index + num_msgs_selected - 1;
                            printf("[-] continuation %u, num cards %u, start index %u, end index %u\n", continuation, num_msgs_selected, start_index, end_index);
                            send_listing(phonebook_index, continuation == 0, start_index, end_index);
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
            printf("MAP_DATA_PACKET (%u bytes): ", size);
            for (i=0;i<size;i++){
                printf("%0x2 ", packet[i]);
            }
            printf ("\n");
            break;
        default:
            printf ("unknown event of type %d\n", packet_type);
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

    // init MAP Notification Client
    // map_notification_client_init();

    // setup MAP Access Server
    map_message_type_t supported_message_types = MAP_MESSAGE_TYPE_SMS_GSM;
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
