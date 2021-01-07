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

#define __BTSTACK_FILE__ "hid_host_test.c"

/*
 * hid_host_test.c
 */



#include <inttypes.h>
#include <stdio.h>

#include "btstack_config.h"
#include "btstack.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300

static enum {
    APP_IDLE,
    APP_CONNECTED
} app_state = APP_IDLE;

static uint16_t hid_host_cid = 0;
static bool unplugged;
static bool boot_mode = false;
static bool send_through_interrupt_channel = false;

// SDP
static uint8_t            hid_descriptor[MAX_ATTRIBUTE_VALUE_SIZE];
static uint16_t           hid_descriptor_len;

// PTS
static const char * remote_addr_string = "00:1B:DC:08:E2:5C";
static bd_addr_t remote_addr;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void hid_host_setup(void){

    // Initialize L2CAP 
    l2cap_init();
    
    // Initialize HID Host    
    hid_host_init(hid_descriptor, hid_descriptor_len);
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
                
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BTstack up and running. \n");
                    }
                    break;

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
                            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            printf("HID Host connected..\n");
                            break;

                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            printf("HID Host disconnected..\n");
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
    printf("c      - start SDP scan and connect");
    printf("o      - get output report\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        
        case 'R':
            printf("Set Protocol mode\n");
            boot_mode = false;
            status = hid_host_send_set_protocol_mode(hid_host_cid, HID_PROTOCOL_MODE_REPORT);
            break;
        case 'b':
            printf("Set Boot mode\n");
            boot_mode = false;
            status = hid_host_send_set_protocol_mode(hid_host_cid, HID_PROTOCOL_MODE_BOOT);
            break;
        case 'B':
            printf("Set Boot mode\n");
            boot_mode = true;
            break;

        case 'c':
            if (unplugged){
                printf("Cannot connect, host is unplugged.\n");
                break;  
            } 
            printf("Start SDP scan and connect to %s.\n", bd_addr_to_str(remote_addr));
            
            if (boot_mode){
                status = hid_host_connect(remote_addr, HID_PROTOCOL_MODE_BOOT, &hid_host_cid);
            } else {
                status = hid_host_connect(remote_addr, HID_PROTOCOL_MODE_REPORT, &hid_host_cid);
            }
            break;

        case 'C':
            printf("Disconnect from  %s...\n", bd_addr_to_str(remote_addr));
            hid_host_disconnect(hid_host_cid);
            break;

        case 's':
            printf("Send \'Suspend\'\n");
            hid_host_send_suspend(hid_host_cid);
            break;
        case 'S':
            printf("Send \'Exit suspend\'\n");
            hid_host_send_exit_suspend(hid_host_cid);
            break;
        case 'u':
            printf("Send \'Unplug\'\n");
            unplugged = true;
            hid_host_send_virtual_cable_unplug(hid_host_cid);
            break;
        
        case '1':
            printf("Get feature report with id 0x03 from %s\n", remote_addr_string);
            status = hid_host_send_get_feature_report(hid_host_cid, 0x03);
            break;
        case '2':
            printf("Get output report with id 0x05 from %s\n", remote_addr_string);
            status = hid_host_send_get_output_report(hid_host_cid, 0x05);
            break;
        case '3':
            printf("Get input report from with id 0x02 %s\n", remote_addr_string);
            status = hid_host_send_get_input_report(hid_host_cid, 0x02);
            break;
        
        case '4':{
            uint8_t report[] = {0, 0};
            printf("Set output report with id 0x03\n");
            status = hid_host_send_set_output_report(hid_host_cid, 0x03, report, sizeof(report));
            break;
        }
        case '5':{
            uint8_t report[] = {0, 0, 0};
            printf("Set feature report with id 0x05\n");
            status = hid_host_send_set_feature_report(hid_host_cid, 0x05, report, sizeof(report));
            break;
        }

        case '6':{
            uint8_t report[] = {0, 0, 0, 0};
            printf("Set input report with id 0x02\n");
            status = hid_host_send_set_input_report(hid_host_cid, 0x02, report, sizeof(report));
            break;
        }
        case '7':{
            uint8_t report[] = {0,0,0, 0,0,0, 0,0};
            printf("Set output report with id 0x01\n");
            status = hid_host_send_set_output_report(hid_host_cid, 0x01, report, sizeof(report));
            break;
        }
        case 'p':
            printf("Get Protocol\n");
            status = hid_host_send_get_protocol(hid_host_cid);
            break;
        
        case 'X':
            printf("Set send through interrupt channel\n");
            send_through_interrupt_channel = true;
            break;
        case 'x':
            printf("Set send through control channel\n");
            send_through_interrupt_channel = true;
            break;

         case '8':{
            uint8_t report[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            printf("Send output report with id 0x03\n");
            status = hid_host_send_output_report(hid_host_cid, 0x03, report, sizeof(report));
            break;
        }
        case '9':{
            uint8_t report[] = {0, 0, 0, 0, 0, 0, 0, 0};
            printf("Set output report with id 0x01\n");
            status = hid_host_send_set_output_report(hid_host_cid, 0x01, report, sizeof(report));
            break;
        }
        case '0':{
            uint8_t report[] = {0, 0, 0, 0, 0, 0, 0, 0};
            printf("Send output report with id 0x01\n");
            status = hid_host_send_output_report(hid_host_cid, 0x01, report, sizeof(report));
            break;
        }

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

