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

#define BTSTACK_FILE__ "microphone_control_service_client_test.c"


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#include "microphone_control_service_client_test.h"

typedef struct advertising_report {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    const uint8_t * data;
} advertising_report_t;

static enum {
    APP_STATE_IDLE,
    APP_STATE_W4_SCAN_RESULT,
    APP_STATE_W4_CONNECT,
    APP_STATE_CONNECTED
} app_state;

static int blacklist_index = 0;
static bd_addr_t blacklist[20];
static advertising_report_t report;

static hci_con_handle_t connection_handle;
static uint16_t mips_cid;

static bd_addr_t cmdline_addr;
static int cmdline_addr_found = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;


static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void microphone_control_service_client_setup(void){
    // Init L2CAP
    l2cap_init();

    // Setup ATT server - only needed if LE Peripheral does ATT queries on its own, e.g. Android phones
    att_server_init(profile_data, NULL, NULL);    

    // GATT Client setup
    gatt_client_init();
    
    microphone_control_service_client_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);
} 

static int blacklist_size(void){
    return sizeof(blacklist) / sizeof(bd_addr_t);
}

static int blacklist_contains(bd_addr_t addr){
    int i;
    for (i=0; i<blacklist_size(); i++){
        if (bd_addr_cmp(addr, blacklist[i]) == 0) return 1;
    }
    return 0;
}

static void add_to_blacklist(bd_addr_t addr){
    printf("%s added to blacklist (no battery service found).\n", bd_addr_to_str(addr));
    bd_addr_copy(blacklist[blacklist_index], addr);
    blacklist_index = (blacklist_index + 1) % blacklist_size();
}

static void dump_advertising_report(uint8_t *packet){
    bd_addr_t address;
    gap_event_advertising_report_get_address(packet, address);

    printf("    * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", 
        gap_event_advertising_report_get_advertising_event_type(packet),
        gap_event_advertising_report_get_address_type(packet), 
        bd_addr_to_str(address), 
        gap_event_advertising_report_get_rssi(packet), 
        gap_event_advertising_report_get_data_length(packet));
    printf_hexdump(gap_event_advertising_report_get_data(packet), gap_event_advertising_report_get_data_length(packet));
    
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t address;

    if (packet_type != HCI_EVENT_PACKET){
        return;  
    } 

    switch (hci_event_packet_get_type(packet)) {
        
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            if (cmdline_addr_found){
                printf("Connect to %s\n", bd_addr_to_str(cmdline_addr));
                app_state = APP_STATE_W4_CONNECT;
                gap_connect(cmdline_addr, 0);
                break;
            }
            printf("Start scanning!\n");
            app_state = APP_STATE_W4_SCAN_RESULT;
            gap_set_scan_parameters(0,0x0030, 0x0030);
            gap_start_scan();
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (app_state != APP_STATE_W4_SCAN_RESULT) return;

            gap_event_advertising_report_get_address(packet, address);
            if (blacklist_contains(address)) {
                break;
            }
            dump_advertising_report(packet);

            // stop scanning, and connect to the device
            app_state = APP_STATE_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(report.address));
            gap_connect(report.address,report.address_type);
            break;

        case HCI_EVENT_LE_META:
            // Wait for connection complete
            if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            
            if (app_state != APP_STATE_W4_CONNECT) return;
            
            // Get connection handle from event
            connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
            
            (void) microphone_control_service_client_connect(connection_handle, gatt_client_event_handler, &mips_cid);

            app_state = APP_STATE_CONNECTED;
            printf("Microphone Control service connected.\n");
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            connection_handle = HCI_CON_HANDLE_INVALID;
            // Disconnect battery service
            microphone_control_service_client_disconnect(mips_cid);
            
            if (cmdline_addr_found){
                printf("Disconnected %s\n", bd_addr_to_str(cmdline_addr));
                return;
            }

            printf("Disconnected %s\n", bd_addr_to_str(report.address));
            printf("Restart scan.\n");
            app_state = APP_STATE_W4_SCAN_RESULT;
            gap_start_scan();
            break;
        default:
            break;
    }
}

static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;
    uint8_t att_status;

    if (hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META){
        return;
    }
    
    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case GATTSERVICE_SUBEVENT_MICS_CONNECTED:
            status = gattservice_subevent_mics_connected_get_status(packet);
            switch (status){
                case ERROR_CODE_SUCCESS:
                    printf("Microphone Control service client connected\n");
                    break;
                default:
                    printf("Microphone Control service client connection failed, err 0x%02x.\n", status);
                    add_to_blacklist(report.address);
                    gap_disconnect(connection_handle);
                    break;
            }
            break;

        case GATTSERVICE_SUBEVENT_REMOTE_MICS_MUTE:
            att_status = gattservice_subevent_remote_mics_mute_get_status(packet);
            if (att_status != ATT_ERROR_SUCCESS){
                printf("Mute status read failed, ATT Error 0x%02x\n", att_status);
            } else {
                printf("Mute status: %d\n", gattservice_subevent_remote_mics_mute_get_state(packet));
            }
            break;

        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    // parse address if command line arguments are provided
    int arg = 1;
    cmdline_addr_found = 0;
    
    while (arg < argc) {
        if(!strcmp(argv[arg], "-a") || !strcmp(argv[arg], "--address")){
            arg++;
            cmdline_addr_found = sscanf_bd_addr(argv[arg], cmdline_addr);
            arg++;
            if (!cmdline_addr_found) exit(1);
            continue;
        }
        fprintf(stderr, "\nUsage: %s [-a|--address aa:bb:cc:dd:ee:ff]\n", argv[0]);
        fprintf(stderr, "If no argument is provided, GATT browser will start scanning and connect to the first found device.\nTo connect to a specific device use argument [-a].\n\n");
        return 0;
    }
    (void)argv;

    microphone_control_service_client_setup();

    app_state = APP_STATE_IDLE;

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}



