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

static const char * msg_listing_header = "<MAP-msg-listing version=\"1.0\">";
static const char * msg_listing_msg   = "<msg handle=\"ID%u\" subject=\"Hello\" type=\"%s\" read=\"%s\"/>";
static const char * msg_listing_footer = "</MAP-msg-listing>";

enum msg_status_read { no, yes };
static struct test_config_s
{
    int nr;
    char* descr;
    int msg_count;
    char* msg_types[6]; // maximum 6-1 entries, last one is null
    enum msg_status_read msg_stati[6]; // maximum 6-1 entries, last one is null
} test_configs[] =
{
{.nr = 0, .descr = "MAP/MSE/MMB/BV-09-I 10 11 13 14" , .msg_count = 2, .msg_types = { "SMS_GSM","SMS_CDMA"                      }, },
{.nr = 1, .descr = "MAP/MSE/MMB/BV-12-I"             , .msg_count = 1, .msg_types = { "EMAIL", "SMS_GSM","SMS_CDMA"             }, },
{.nr = 2, .descr = "MAP/MSE/MMB/BV-15-I 18 20"       , .msg_count = 5, .msg_types = { "EMAIL","SMS_GSM","SMS_CDMA", "MMS", "IM" }, },
{.nr = 3, .descr = "MAP/MSE/MMB/BV-16-I"             , .msg_count = 1, .msg_types = { "EMAIL","EMAIL"},},
};

struct test_config_s* config = &test_configs[0];
static int current_msg_type = 0;
static int send_one_more_message = 0;

static void set_test_config(int nr) {
    if (nr < ARRAYSIZE(test_configs))
        config = &test_configs[nr];
    else
        printf("couldn't set test setup nr %d\n", nr);
}


static void init_testcases(void) {
    current_msg_type = 0;
    send_one_more_message = 0;
}


static void create_msg(char * msg_buffer, uint16_t index, int maxsize){
    index = index % ARRAYSIZE(config->msg_types);
    sprintf_s(msg_buffer, maxsize, msg_listing_msg,
        index,
        config->msg_types[index],
        config->msg_stati[index]?"yes":"no");
}

// TODO enable to send message larger as one OBEX/MAP packet
static uint16_t send_listing(uint16_t first, uint16_t last) {
    uint16_t max_body_size = map_access_server_get_max_body_size(map_cid);
    char listing_buffer[500];
    uint16_t pos = 0;
    bool done = false;

    log_debug("1 max_body_size:%d", max_body_size);

    if (first == 0){
        // add header
        uint16_t len = (uint16_t)strlen(msg_listing_header);
        memcpy(upload_buffer, msg_listing_header, len);
        pos += len;
        max_body_size -= len;
    }
    while ((max_body_size > 0) && (first <= last)){
        log_debug("2 first:%d last:%d pos:%d", first, last, pos);
        // add entry
        create_msg(listing_buffer, first, sizeof(listing_buffer));
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

    if (first > last){
        uint16_t len = (uint16_t) strlen(msg_listing_footer);
        log_debug("5 first:%d last:%d pos:%d len:%d", first, last, pos, len);
        if (len < max_body_size){
            log_debug("6 first:%d last:%d pos:%d len:%d", first, last, pos, len);
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
    log_debug("7 send response %.*s", btstack_min(pos, sizeof(listing_buffer)), upload_buffer);
    map_access_server_send_get_put_response(map_cid, response_code, continuation, pos, upload_buffer);
    return first;
}

static void print_current_test_config(void)
{
    MAP_PRINTF("new test config is <%d: %s>\n", config->nr, config->descr);
}

#ifdef HAVE_BTSTACK_STDIN
// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    MAP_PRINTF("\n--- Bluetooth MAP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    print_current_test_config();
    MAP_PRINTF("<n> switch to next test config\n");
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

        case 'r':
            // reset current test cases
            init_testcases();
            print_current_test_config();
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

    if (msg_handle > ARRAYSIZE(config->msg_types) - 1)
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
    uint16_t pos, total_messages, start_index, end_index, num_msgs_selected, max_list_count, dummy_map_cid;
    uint32_t continuation;
	
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
                            MAP_PRINTF("[+] Connection closed, re-init test case states\n");
                            init_testcases();
                            break;
                        case MAP_SUBEVENT_OPERATION_COMPLETED:
                            MAP_PRINTF("[+] Operation complete, status 0x%02x\n",
                                   map_subevent_operation_completed_get_status(packet));
                            break;
							
                        case MAP_SUBEVENT_FOLDER_LISTING_ITEM:
                            map_access_server_set_database_identifier(map_cid, database_identifier);
                            map_access_server_set_folder_version(map_cid, folder_version);
                            MAP_PRINTF("[+] Get Folder listing\n");
                            send_listing(0, config->msg_count-1 + send_one_more_message);
                            break;
							
                        case MAP_SUBEVENT_GET_MESSAGE_LISTING:
                            pos = 3; // we skip directly to continuation;
                            APP_READ_32(packet, &pos, &continuation);
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            APP_READ_16(packet, &pos, &max_list_count);
                            APP_READ_16(packet, &pos, &start_index);
                            

                            if (continuation == 0) {
                                MAP_PRINTF("[+] Start MAP_SUBEVENT_GET_MESSAGE_LISTING continuation == 0\n");
                                map_access_server_set_database_identifier(map_cid, database_identifier);
                                map_access_server_set_folder_version(map_cid, folder_version);
                            }
                            
                            // send messages listing
                            total_messages = config->msg_count + send_one_more_message;

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
                            break;

                        case MAP_SUBEVENT_GET_MESSAGE:
                            MAP_PRINTF("[+] Get Message\n");
                            send_listing(0, 1);
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_STATUS:
                            pos = 3; // we skip directly to the first value;
                            uint8_t StatusIndicator;
                            uint8_t StatusValue;
                            char request_name[32];
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            APP_READ_08(packet, &pos, &StatusIndicator);
                            APP_READ_08(packet, &pos, &StatusValue);
                            APP_READ_STR(packet, &pos, sizeof(request_name), request_name, "request_name");
                            MAP_PRINTF("[+] Put MessageStatus ObjectName:%s StatusIndicator:0x%02X StatusValue:0x%02X\n", request_name, (unsigned int) StatusIndicator, (unsigned int)StatusValue);
                            map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            handle_set_message_status(request_name, StatusIndicator, (unsigned int)StatusValue);
                            break;

                        case MAP_SUBEVENT_PUT_MESSAGE_UPDATE:
                            pos = 3; // we skip directly to the first value;
                            APP_READ_16(packet, &pos, &dummy_map_cid);
                            MAP_PRINTF("[+] Put MessageUpdate\n");
                            map_access_server_send_get_put_response(map_cid, OBEX_RESP_SUCCESS, 0, 0, NULL);
                            send_one_more_message = 1;
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
