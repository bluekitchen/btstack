/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hid_host_test.c"

/*
 * hid_host_test.c : Tool for testig HID host with PTS, see hid_host_test.md for PTS tests command sequences
 */


#include <inttypes.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300
#define NUM_KEYS 6

// Simplified US Keyboard with Shift modifier

#define CHAR_ILLEGAL     0xff
#define CHAR_RETURN     '\n'
#define CHAR_ESCAPE      27
#define CHAR_TAB         '\t'
#define CHAR_BACKSPACE   0x7f


// English (US)
//
static const uint8_t keytable_us_none [] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             //   0-3  
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',                   //  4-13  
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',                   // 14-23  
    'u', 'v', 'w', 'x', 'y', 'z',                                       // 24-29  
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',                   // 30-39  
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            // 40-44  
    '-', '=', '[', ']', '\\', CHAR_ILLEGAL, ';', '\'', 0x60, ',',       // 45-54  
    '.', '/', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   // 55-60  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 61-64  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 65-68  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 69-72  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 73-76  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 77-80  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 81-84  
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       // 85-97  
    '6', '7', '8', '9', '0', '.', 0xa7,                                 // 97-100 
}; 

static const uint8_t keytable_us_shift[] = {
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             //  0-3   
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',                   //  4-13  
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',                   // 14-23  
    'U', 'V', 'W', 'X', 'Y', 'Z',                                       // 24-29  
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',                   // 30-39  
    CHAR_RETURN, CHAR_ESCAPE, CHAR_BACKSPACE, CHAR_TAB, ' ',            // 40-44  
    '_', '+', '{', '}', '|', CHAR_ILLEGAL, ':', '"', 0x7E, '<',         // 45-54  
    '>', '?', CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,   // 55-60  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 61-64  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 65-68  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 69-72  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 73-76  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 77-80  
    CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL, CHAR_ILLEGAL,             // 81-84  
    '*', '-', '+', '\n', '1', '2', '3', '4', '5',                       // 85-97  
    '6', '7', '8', '9', '0', '.', 0xb1,                                 // 97-100 
}; 


static uint8_t last_keys[NUM_KEYS];

static enum {
    APP_IDLE,
    APP_CONNECTED
} app_state = APP_IDLE;

static bool     unplugged    = false;
static uint16_t hid_host_cid = 0;
static hci_con_handle_t hid_host_con_handle = HCI_CON_HANDLE_INVALID;
static bool hid_host_descriptor_available = false;

static hid_protocol_mode_t hid_host_protocol_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;

// SDP
static uint8_t hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];

// Logitec static const char * remote_addr_string = "00:1F:20:86:DF:52";
// PTS 
static const char * remote_addr_string = "00:1B:DC:08:E2:5C";

static bd_addr_t remote_addr;
static uint16_t host_max_latency;
static uint16_t host_min_timeout;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // Initialize L2CAP 
    l2cap_init();
    
    // Initialize HID Host    
    hid_host_init(hid_descriptor, sizeof(hid_descriptor));
    hid_host_register_packet_handler(packet_handler);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);


    // Disable stdout buffering
    setbuf(stdout, NULL);
}

static void hid_host_handle_interrupt_report(const uint8_t * report, uint16_t report_len){
    // check if HID Input Report
    if (report_len < 1){
        printf("ignore report: len 0");
        return;
    } 

    if (*report != 0xa1) {
        printf("ignore report: header 0x%02x", report[0]);
        return;
    } 
    printf_hexdump(report, report_len);
    
    report++;
    report_len--;
    
    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, 
        hid_descriptor_storage_get_descriptor_data(hid_host_cid), 
        hid_descriptor_storage_get_descriptor_len(hid_host_cid), 
        HID_REPORT_TYPE_INPUT, report, report_len);
    
    int shift = 0;
    uint8_t new_keys[NUM_KEYS];
    memset(new_keys, 0, sizeof(new_keys));
    int     new_keys_count = 0;
    while (btstack_hid_parser_has_more(&parser)){
        uint16_t usage_page;
        uint16_t usage;
        int32_t  value;
        btstack_hid_parser_get_field(&parser, &usage_page, &usage, &value);
        if (usage_page != 0x07) continue;   
        switch (usage){
            case 0xe1:
            case 0xe6:
                if (value){
                    shift = 1;
                }
                continue;
            case 0x00:
                continue;
            default:
                break;
        }
        if (usage >= sizeof(keytable_us_none)) continue;

        // store new keys
        new_keys[new_keys_count++] = usage;

        // check if usage was used last time (and ignore in that case)
        int i;
        for (i=0;i<NUM_KEYS;i++){
            if (usage == last_keys[i]){
                usage = 0;
            }
        }
        if (usage == 0) continue;

        uint8_t key;
        if (shift){
            key = keytable_us_shift[usage];
        } else {
            key = keytable_us_none[usage];
        }
        if (key == CHAR_ILLEGAL) continue;
        if (key == CHAR_BACKSPACE){ 
            printf("\b \b");    // go back one char, print space, go back one char again
            continue;
        }
        printf("%c", key);
    }
    memcpy(last_keys, new_keys, NUM_KEYS);
}


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint8_t   event;
    bd_addr_t address;
    uint8_t   status;

    switch (packet_type) {
		case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);

            switch (event) {            
                case HCI_EVENT_PIN_CODE_REQUEST:
					// inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, address);
                    gap_pin_code_response(address, "0000");
					break;

                case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                    // inform about user confirmation request
                    printf("SSP User Confirmation Request with numeric value '%"PRIu32"'\n", little_endian_read_32(packet, 8));
                    printf("SSP User Confirmation Auto accept\n");
                    break;

                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)){
                        case HID_SUBEVENT_INCOMING_CONNECTION:
                            if (unplugged){
                                hid_host_decline_connection(hid_subevent_incoming_connection_get_hid_cid(packet));
                                break;
                            }
                            hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet), hid_host_protocol_mode);
                            break;
                        
                        case HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG:
                            if (hid_host_cid != hid_subevent_virtual_cable_unplug_get_hid_cid(packet)) return;
                            printf("Received UNPLUG event, disconnecting...\n");
                            unplugged = true;
                            break;

                        case HID_SUBEVENT_CONNECTION_OPENED:
                            status = hid_subevent_connection_opened_get_status(packet);
                            if (status) {
                                // outgoing connection failed
                                printf("Connection failed, status 0x%x\n", status);
                                app_state = APP_IDLE;
                                hid_host_cid = 0;
                                return;
                            }
                            app_state = APP_CONNECTED;
                            hid_host_con_handle = hid_subevent_connection_opened_get_con_handle(packet);
                            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            printf("HID Host connected...\n");
                            break;
                        
                        case HID_SUBEVENT_SNIFF_SUBRATING_PARAMS:
                            host_max_latency = hid_subevent_sniff_subrating_params_get_host_max_latency(packet);
                            host_min_timeout = hid_subevent_sniff_subrating_params_get_host_min_timeout(packet);
                            break;

                        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                            status = hid_subevent_descriptor_available_get_status(packet);
                            if (status == ERROR_CODE_SUCCESS){
                                hid_host_descriptor_available = true;
                                printf("HID Descriptor available\n");
                            } else {
                                printf("Cannot handle input report, HID Descriptor is not available.\n");
                            }
                            break;

                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            hid_host_cid = 0;
                            hid_host_con_handle = HCI_CON_HANDLE_INVALID;
                            printf("HID Host disconnected..\n");
                            break;
                        
                        case HID_SUBEVENT_GET_REPORT_RESPONSE:
                            status = hid_subevent_get_report_response_get_handshake_status(packet);
                            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                printf("Error get report, status 0x%02x\n", status);
                                break;
                            }
                            printf("Received report[%d]: ", hid_subevent_get_report_response_get_report_len(packet));
                            printf_hexdump(hid_subevent_get_report_response_get_report(packet), hid_subevent_get_report_response_get_report_len(packet));
                            printf("\n");
                            break;

                        case HID_SUBEVENT_SET_REPORT_RESPONSE:
                            status = hid_subevent_set_report_response_get_handshake_status(packet);
                            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                printf("Error set report, status 0x%02x\n", status);
                                break;
                            }
                            printf("Report set.\n");
                            break;

                        case HID_SUBEVENT_GET_PROTOCOL_RESPONSE:
                            status = hid_subevent_get_protocol_response_get_handshake_status(packet);
                            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                printf("Error get report, status 0x%02x\n", status);
                                break;
                            }
                            switch ((hid_protocol_mode_t)hid_subevent_get_protocol_response_get_protocol_mode(packet)){
                                case HID_PROTOCOL_MODE_BOOT:
                                    printf("HID device is in BOOT mode.\n");
                                    break;
                                case HID_PROTOCOL_MODE_REPORT:
                                    printf("HID device is in REPORT mode.\n");
                                    break;
                                default:
                                    break;
                            }
                            break;

                        case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
                            status = hid_subevent_set_report_response_get_handshake_status(packet);
                            if (status != HID_HANDSHAKE_PARAM_TYPE_SUCCESSFUL){
                                printf("Error set protocol, status 0x%02x\n", status);
                                break;
                            }
                            printf("Protocol set.\n");
                            break;

                        case HID_SUBEVENT_REPORT:
                            if (hid_host_descriptor_available){
                                hid_host_handle_interrupt_report(hid_subevent_report_get_report(packet), hid_subevent_report_get_report_len(packet));
                            } else {
                                printf_hexdump(hid_subevent_report_get_report(packet), hid_subevent_report_get_report_len(packet));
                            }
                            break;

                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }

}
/* LISTING_END */


static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth HID Host Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("a      - start SDP scan and connect to %s in BOOT mode\n", remote_addr_string);
    printf("c      - start SDP scan and connect to %s in REPORT mode\n", remote_addr_string);
    printf("C      - disconnect from %s\n", remote_addr_string);
    printf("\n");
    printf("s      - suspend\n");
    printf("S      - exit suspend\n");
    printf("U      - unplug\n");
    printf("u      - reset unplug (for the next PTS test)\n");
    
    printf("\n");
    printf("1      - Get report with id 0x05\n");
    printf("2      - Get report with id 0x03\n");
    printf("3      - Get report with id 0x02\n");

    printf("\n");
    printf("4      - Set report with id 0x03\n");
    printf("5      - Set report with id 0x05\n");
    printf("6      - Set report with id 0x02\n");
    printf("7      - Set report with id 0x01\n");

    printf("\n");
    printf("t      - Send report with id 0x03\n");
    printf("o      - Send report with id 0x01\n");
    
    printf("\n");
    printf("p      - Get protocol\n");
    printf("r      - Set protocol in REPORT mode\n");
    printf("b      - Set protocol in BOOT mode\n");

    printf("\n");
    printf("y      - set link supervision timeout to 0\n");
    printf("w      - send sniff subrating cmd\n");

    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'a':
            if (unplugged){
                printf("Cannot connect, host is unplugged.\n");
                break;  
            } 
            printf("Start SDP scan and connect to %s in boot mode.\n", remote_addr_string);
            status = hid_host_connect(remote_addr, HID_PROTOCOL_MODE_BOOT, &hid_host_cid);
            break;

        case 'c':
            if (unplugged){
                printf("Cannot connect, host is unplugged.\n");
                break;  
            } 
            printf("Start SDP scan and connect to %s in report mode.\n", remote_addr_string);
            status = hid_host_connect(remote_addr, HID_PROTOCOL_MODE_REPORT, &hid_host_cid);
            break;

        case 'C':
            printf("Disconnect from  %s...\n", remote_addr_string);
            hid_host_disconnect(hid_host_cid);
            break;


        case 'p':
            printf("Get protocol\n");
            status = hid_host_send_get_protocol(hid_host_cid);
            break;
        case 'r':
            printf("Set protocol in REPORT mode\n");
            status = hid_host_send_set_protocol_mode(hid_host_cid, HID_PROTOCOL_MODE_REPORT);
            break;
        case 'b':
            printf("Set protocol in BOOT mode\n");
            status = hid_host_send_set_protocol_mode(hid_host_cid, HID_PROTOCOL_MODE_BOOT);
            break;

        
        case 's':
            printf("Send \'Suspend\'\n");
            hid_host_send_suspend(hid_host_cid);
            break;
        case 'S':
            printf("Send \'Exit suspend\'\n");
            hid_host_send_exit_suspend(hid_host_cid);
            break;
        case 'U':
            printf("Send \'Unplug\'\n");
            hid_host_send_virtual_cable_unplug(hid_host_cid);
            break;
        case 'u':
            printf("Reset unplug (for the next PTS test)\n");
            unplugged = false;
            break;
        

        case '1':
            printf("Get report with id 0x05\n");
            status = hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, 0x05);
            break;
        case '2':
            printf("Get report with id 0x03\n");
            status = hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_OUTPUT, 0x03);
            break;
        case '3':
            printf("Get report from with id 0x02\n");
            status = hid_host_send_get_report(hid_host_cid, HID_REPORT_TYPE_INPUT, 0x02);
            break;


        case '4':{
            printf("Set output report with id 0x03\n");
            uint8_t report[] = {0, 0};
            status = hid_host_send_set_report(hid_host_cid, HID_REPORT_TYPE_OUTPUT, 0x03, report, sizeof(report));
            break;
        }

        case '5':{
            printf("Set feature report with id 0x05\n");
            uint8_t report[] = {0, 0, 0};
            status = hid_host_send_set_report(hid_host_cid, HID_REPORT_TYPE_FEATURE, 0x05, report, sizeof(report));
            break;
        }

        case '6':{
            printf("Set input report with id 0x02\n");
            uint8_t report[] = {0, 0, 0, 0};
            status = hid_host_send_set_report(hid_host_cid, HID_REPORT_TYPE_INPUT, 0x02, report, sizeof(report));
            break;
        }

        case '7':{
            uint8_t report[] = {0, 0, 0, 0, 0, 0, 0, 0};
            printf("Set input report with id 0x01\n");
            status = hid_host_send_set_report(hid_host_cid, HID_REPORT_TYPE_INPUT, 0x01, report, sizeof(report));
            break;
        }

        case 't':{
            printf("Send report with id 0x03\n");
            uint8_t report[] = {0, 0};
            status = hid_host_send_report(hid_host_cid, 0x03, report, sizeof(report));
            break;
        }

        case 'o':{
            uint8_t report[] = {0, 0, 0, 0, 0, 0, 0, 0};
            printf("Send output report with id 0x01\n");
            status = hid_host_send_report(hid_host_cid, 0x01, report, sizeof(report));
            break;
        }

        case 'y':
            printf("Set link supervision timeout to 0 \n");
            hci_send_cmd(&hci_write_link_supervision_timeout, hid_host_con_handle, 0);
            break;
        
        case 'w':
            printf("Configure sniff subrating\n");
            gap_sniff_subrating_configure(hid_host_con_handle, host_max_latency, host_min_timeout, 0);
            break;
        
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("HID host cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    hid_host_setup();

    // parse human readable Bluetooth address
    sscanf_bd_addr(remote_addr_string, remote_addr);
    btstack_stdin_setup(stdin_process);
    // Turn on the device 
    hci_power_control(HCI_POWER_ON);
    return 0;
}

