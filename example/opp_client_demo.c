/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "opp_client_demo.c"

// *****************************************************************************
/* EXAMPLE_START(opp_client_demo): OPP Client - Push objects to OPP server
 *
 * @text Note: The Bluetooth address of the remote OPP server is hardcoded. 
 * Change it before running example, then use the UI to connect to it and send
 * a sample file.
 */
// *****************************************************************************


#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static bd_addr_t    remote_addr;
// MBP2016 "F4-0F-24-3B-1B-E1"
// Nexus 7 "30-85-A9-54-2E-78"
// iPhone SE "BC:EC:5D:E6:15:03"
// PTS "001BDC080AA5"
static  char * remote_addr_string = "58:d9:c3:2b:fb:a7";

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t opp_cid;

#ifdef HAVE_BTSTACK_STDIN

// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth OPP Client Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("a - establish OPP connection to %s\n", bd_addr_to_str(remote_addr));
    printf("t - disconnect\n");
    printf("x - abort operation\n");
    printf("\n");
}

static void stdin_process(char c){
    switch (c){
        case '\n':
        case '\r':
            break;
        case 'a':
            printf("[+] Connecting to %s...\n", bd_addr_to_str(remote_addr));
            opp_connect(&packet_handler, remote_addr, &opp_cid);
            break;
        case 'x':
            printf("[+] Abort\n");
            opp_abort(opp_cid);
            break;
        case 't':
            opp_disconnect(opp_cid);
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
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        show_usage();
                    }
                    break;
                case HCI_EVENT_OPP_META:
                    switch (hci_event_opp_meta_get_subevent_code(packet)){
                        case OPP_SUBEVENT_CONNECTION_OPENED:
                            status = opp_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                printf("[+] Connected\n");
                            }
                            break;
                        case OPP_SUBEVENT_CONNECTION_CLOSED:
                            printf("[+] Connection closed\n");
                            break;
                        case OPP_SUBEVENT_OPERATION_COMPLETED:
                            printf("[+] Operation complete, status 0x%02x\n",
                                   opp_subevent_operation_completed_get_status(packet));
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case OPP_DATA_PACKET:
            for (i=0;i<size;i++){
                printf("%c", packet[i]);
            }
            break;
        default:
            break;
    }
}
#else

// packet handler for emdded system with fixed operation sequence
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int i;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("[+] Connect to Object Push Server on %s\n", bd_addr_to_str(remote_addr));
                        opp_connect(&packet_handler, remote_addr, &opp_cid);
                    }
                    break;
                case HCI_EVENT_OPP_META:
                    switch (hci_event_opp_meta_get_subevent_code(packet)){
                        case OPP_SUBEVENT_CONNECTION_OPENED:
                            printf("[+] Connected\n");
                            break;
                        case OPP_SUBEVENT_CONNECTION_CLOSED:
                            printf("[+] Connection closed\n");
                            break;
                        case OPP_SUBEVENT_OPERATION_COMPLETED:
                            printf("[+] Operation complete\n");
                            opp_disconnect(opp_cid);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case OPP_DATA_PACKET:
            for (i=0;i<size;i++){
                printf("%c", packet[i]);
            }
            break;
        default:
            break;
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // init L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // init RFCOM
    rfcomm_init();

    // init GOEP Client
    goep_client_init();

    // init OPP Client
    opp_client_init();

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
