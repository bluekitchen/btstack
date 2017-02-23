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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "stdin_support.h"
#include "avrcp.h"

#define AVRCP_BROWSING_ENABLED 0
static btstack_packet_callback_registration_t hci_event_callback_registration;

// mac 2011: static bd_addr_t device_addr = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
// pts: static bd_addr_t device_addr = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: static bd_addr_t device_addr = {0x84, 0x38, 0x35, 0x65, 0xd1, 0x15};
// phone: static bd_addr_t device_addr = {0xD8, 0xBB, 0x2C, 0xDF, 0xF1, 0x08};

// iPhone SE
// static const char * device_addr_string = "BC:EC:5D:E6:15:03";

static bd_addr_t device_addr;
static const char * device_addr_string = "BC:EC:5D:E6:15:03";

static uint16_t con_handle = 0;
static uint8_t sdp_avrcp_controller_service_buffer[150];

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit test app
                    printf("--- avrcp_test: HCI_EVENT_DISCONNECTION_COMPLETE\n");
                    break;
                case HCI_EVENT_AVRCP_META:
                    printf("app: ");

                    switch (packet[2]){
                        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED:
                            con_handle = avrcp_subevent_connection_established_get_con_handle(packet);
                            local_cid = avrcp_subevent_connection_established_get_local_cid(packet);
                            avrcp_subevent_connection_established_get_bd_addr(packet, event_addr);
                            printf("AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: Channel successfully opened: %s, handle 0x%02x, local cid 0x%02x\n", bd_addr_to_str(event_addr), con_handle, local_cid);
                            break;
                        case AVRCP_SUBEVENT_CONNECTION_CLOSED:
                            printf("AVRCP_SUBEVENT_CONNECTION_RELEASED: con_handle 0x%02x\n", avrcp_subevent_connection_closed_get_con_handle(packet));
                            con_handle = 0;
                            break;
                        // case AVRCP_SUBEVENT_PLAY_STATUS:
                        //     printf("AVRCP_SUBEVENT_PLAY_STATUS\n");
                        //     break;
                        default:
                            printf("--- avrcp_test: Not implemented\n");
                            break;
                    }    
                    break;   
                default:
                    break;
            }
            break;
        default:
            // other packet type
            break;
    }
}


static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVRCP Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("C      - disconnect\n");
    printf("i      - get unit info\n");
    printf("e      - get capabilities\n");
    printf("l      - play\n");
    printf("s      - stop\n");
    printf("p      - pause\n");
    printf("w      - fast_forward\n");
    printf("r      - start_rewind\n");
    printf("R      - stop_rewind\n");
    printf("f      - forward\n"); 
    printf("b      - backward\n");
    printf("S      - get play status\n");
    printf("n      - enable  notification, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED\n");
    printf("N      - disable notification, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED\n");
    printf("I      - get now playing info\n");
    printf("u      - volume up\n");
    printf("d      - volume down\n");
    printf("a      - absolute volume of %d percent\n", 5000/127);
    printf("m      - mute\n");
    printf("k      - skip\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    UNUSED(ds);
    UNUSED(callback_type);

    int cmd = btstack_stdin_read();
    printf("- execute command %c\n", cmd);
    switch (cmd){
        case 'c':
            avrcp_connect(device_addr);
            break;
        case 'i':
            avrcp_unit_info(con_handle);
            break;
        case 'e':
            avrcp_get_supported_events(con_handle);
            break;
        case 'l':
            avrcp_play(con_handle);
            break;
        case 's':
            avrcp_stop(con_handle);
            break;
        case 'p':
            avrcp_pause(con_handle);
            break;
        case 'w':
            avrcp_start_fast_forward(con_handle);
            break;
        case 'W':
            avrcp_stop_fast_forward(con_handle);
            break;
        case 'r':
            avrcp_start_rewind(con_handle);
            break;
        case 'R':
            avrcp_stop_rewind(con_handle);
            break;
        case 'f':
            avrcp_forward(con_handle); 
            break;
        case 'b':
            avrcp_backward(con_handle);
            break;
        case 'S':
            avrcp_get_play_status(con_handle);
            break;
        case 'n':
            //avrcp_enable_notification(con_handle, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED, 2);
            avrcp_enable_notification(con_handle, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED, 0);
            break;
        case 'N':
            //avrcp_disable_notification(con_handle, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_disable_notification(con_handle, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            break;
        case 'I':
            avrcp_get_now_playing_info(con_handle);
            break;
        case 'a':
            avrcp_set_absolute_volume(con_handle, 50);
            break;
        case 'u':
            avrcp_volume_up(con_handle);
            break;
        case 'd':
            avrcp_start_volume_down(con_handle);
            break;
        case 'm':
            avrcp_start_mute(con_handle);
            break;
        case 'k':
            avrcp_start_skip(con_handle);
            break;

        default:
            show_usage();
            break;

    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    
    // Initialize AVDTP Sink
    avrcp_init();
    avrcp_register_packet_handler(&packet_handler);

    // Initialize SDP 
    sdp_init();
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10001, AVRCP_BROWSING_ENABLED, 1, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);
    
    gap_set_local_name("BTstack AVRCP Test");
    gap_discoverable_control(1);
//     gap_set_class_of_device(0x200408);
    
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
