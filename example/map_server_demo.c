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

static uint8_t database_identifier[MAS_DATABASE_IDENTIFIER_LEN];
static uint8_t folder_version[MAS_FOLDER_VERSION_LEN];

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

static const char * msg_listing_header = "<MAP-msg-listing version = \"1.0\">";
static const char * msg_listing_msg   = "<msg handle = \"20000100001\" subject=\"Hello\" type=\"%s\" read=\"%s\"/>";
static const char * msg_listing_footer = "</MAP-msg-listing>";

static struct test_config_s
{
    char* descr;
    int msg_count;
    int cycle_type_first;
    char* msg_types[6]; // maximum 4-1 entries, last one is null
    char* msg_stati[3]; // maximum 3-1 entries, last one is null
} test_configs[] =
{
    
{.descr = "MAP/MSE/MMB/BV-09-I 10 11 13 14" , .msg_count = 2, .msg_types = { "SMS_GSM","SMS_CDMA"},                          .msg_stati = { "no"}      ,.cycle_type_first = 0},
{.descr = "MAP/MSE/MMB/BV-12-I"             , .msg_count = 1, .msg_types = { "EMAIL", "SMS_GSM","SMS_CDMA"},                 .msg_stati = { "no","yes"},.cycle_type_first = 0},
{.descr = "MAP/MSE/MMB/BV-15-I"             , .msg_count = 3, .msg_types = { "EMAIL","SMS_GSM","SMS_CDMA"/*, "MMS", "IM"*/}, .msg_stati = {"no","yes"} ,.cycle_type_first = 1},
{.descr = "MAP/MSE/MMB/BV-15-I MMS only"    , .msg_count = 1, .msg_types = { "MMS"},                                         .msg_stati = { "no","yes"},.cycle_type_first = 0},
{.descr = "MAP/MSE/MMB/BV-15-I IM only"     , .msg_count = 1, .msg_types = { "IM"},                                          .msg_stati = { "no","yes"},.cycle_type_first = 0},
};

static int current_test_config = 0;
static int current_msg_type = 0;
static int msg_status = 0;

static void init_testcases(void) {
    current_msg_type = 0;
    msg_status = 0;
}

// activate next msg_status
// return true on overflow
static int cycle_msg_status(void) {
    if (test_configs[current_test_config].msg_stati[msg_status + 1] != NULL)
        ++msg_status;
    else
        msg_status = 0;

    return msg_status == 0;
}

// // activate next msg_type
// return true on overflow
static int cycle_msg_type(void) {
    if (test_configs[current_test_config].msg_types[current_msg_type + 1] != NULL)
        ++current_msg_type;
    else
        current_msg_type = 0;

    return current_msg_type == 0;
}

static void create_msg(char * msg_buffer, uint16_t index, int maxsize){
    sprintf_s(msg_buffer, maxsize, msg_listing_msg,
        test_configs[current_test_config].msg_types[current_msg_type], test_configs[current_test_config].msg_stati[msg_status]);

    if (test_configs[current_test_config].cycle_type_first) {
        if (cycle_msg_type())
            cycle_msg_status();
    } else {
        if (cycle_msg_status())
            cycle_msg_type();
    }
}

// TODO enable to send message larger as one OBEX/MAP packet
static uint16_t send_listing(uint16_t first, uint16_t last) {
    uint16_t max_body_size = map_access_server_get_max_body_size(map_cid);
    char listing_buffer[200];
    uint16_t pos = 0;
    bool done = false;
    
    if (first == 0){
        // add header
        uint16_t len = (uint16_t)strlen(msg_listing_header);
        memcpy(upload_buffer, msg_listing_header, len);
        pos += len;
        max_body_size -= len;
    }
    while ((max_body_size > 0) && (first <= last)){
        // add entry
        create_msg(listing_buffer, first, sizeof(listing_buffer));
        // get len
        uint16_t len = (uint16_t)strlen(listing_buffer);
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
            // add footer
            memcpy(&upload_buffer[pos], (const uint8_t *) msg_listing_footer, len);
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
    map_access_server_send_get_put_response(map_cid, response_code, continuation, pos, upload_buffer);
    return first;
}

static void print_current_test_config(void)
{
    MAP_PRINTF("new test config is <%d: %s>\n", current_test_config, test_configs[current_test_config].descr);
}

#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    MAP_PRINTF("\n--- Bluetooth MAP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    print_current_test_config();
    MAP_PRINTF("'n' switch to next test config\n");
    MAP_PRINTF("\n");
}

static void stdin_process(char c){
    switch (c){
        case 'n':
            current_test_config = (current_test_config + 1) % ARRAYSIZE(test_configs);
            // init MAP Access Server test cases
            init_testcases();
            print_current_test_config();
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
    uint16_t continuation, total_messages, start_index, end_index, num_msgs_selected, max_list_count;
	
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
                                MAP_PRINTF("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                map_cid = map_subevent_connection_opened_get_map_cid(packet);
                                MAP_PRINTF("[+] Connected map_cid 0x%04x\n", map_cid);
                            }
                            break;
                        case MAP_SUBEVENT_CONNECTION_CLOSED:
                            MAP_PRINTF("[+] Connection closed\n");
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            MAP_PRINTF("[+] Operation complete, status 0x%02x\n",
                                   map_subevent_operation_completed_get_status(packet));
                            break;
							
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            map_access_server_set_database_identifier(map_cid, database_identifier);
                            map_access_server_set_folder_version(map_cid, folder_version);
                            MAP_PRINTF("[+] Get Folder listing\n");
                            send_listing(0, test_configs[current_test_config].msg_count-1);
                            break;
							
                        case MAP_SUBEVENT_GET_MESSAGE_LISTING:
                            uint16_t pos = 3; // we skip directly to continuation;
                            //continuation = (uint16_t)map_subevent_get_message_listing_get_continuation(packet);
                            APP_READ_16(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &start_index);

                            if (continuation == 0) {
                                MAP_PRINTF("[+] Start MAP_SUBEVENT_GET_MESSAGE_LISTING continuation == 0\n");
                                map_access_server_set_database_identifier(map_cid, database_identifier);
                                map_access_server_set_folder_version(map_cid, folder_version);
                            }
                            
                            // send messages listing
                            total_messages = test_configs[current_test_config].msg_count;
                            //start_index = map_subevent_get_message_listing_get_ListStartOffset(packet);

                            num_msgs_selected = total_messages - start_index;
                            max_list_count = map_subevent_get_message_listing_get_MaxListCount(packet);
                            if (max_list_count < 0xffff){
                                num_msgs_selected = btstack_min(max_list_count, num_msgs_selected);
                            }
                            MAP_PRINTF("[+] get message listing - list offset %u, num messages %u\n", start_index, num_msgs_selected);
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
                            MAP_PRINTF("[-] continuation %u, num messages %u, start index %u, end index %u\n", continuation, num_msgs_selected, start_index, end_index);
                            send_listing(start_index, end_index);
                            //send_listing(test_configs[current_test_config].msg_count-1);
                            break;

                        case MAP_SUBEVENT_GET_MESSAGE:
                            MAP_PRINTF("[+] Get Message\n");
                            send_listing(0, 1);
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_STATUS:
                            MAP_PRINTF("[+] Put MessageStatus\n");
                            map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
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
