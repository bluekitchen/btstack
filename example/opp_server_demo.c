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

#define BTSTACK_FILE__ "opp_server_demo.c"

// *****************************************************************************
/* EXAMPLE_START(pbap_server_demo): OPP Server - Demo OPP Server
 */
// *****************************************************************************


#define OPP_SERVER_L2CAP_PSM         0x1001
#define OPP_SERVER_RFCOMM_CHANNEL_NR 1

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint16_t opp_cid;

static uint8_t service_buffer[150];

// static uint32_t sdp_service_record_handle;

static uint32_t supported_features_default = 0;

// packet handler
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_OPP_META:
                    switch (hci_event_opp_meta_get_subevent_code(packet)){
                        case OPP_SUBEVENT_CONNECTION_OPENED:
                            status = opp_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                opp_cid = opp_subevent_connection_opened_get_opp_cid(packet);
                                printf("[+] Connected opp_cid 0x%04x\n", opp_cid);
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
        default:
            break;
    }
}


#ifdef HAVE_BTSTACK_STDIN

// Testing User Interface
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth OPP Server Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
}

static void stdin_process(char c) {
    log_info("stdin: %c", c);
    switch (c) {
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    gap_set_local_name("OPP Server Demo 00:00:00:00:00:00");

    // init L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // init RFCOM
    rfcomm_init();

    // init GOEP Server
    goep_server_init();

    // init OPP Server
    opp_server_init(&packet_handler, OPP_SERVER_RFCOMM_CHANNEL_NR, OPP_SERVER_L2CAP_PSM, LEVEL_2);

    // setup SDP Record
    sdp_init();
    uint32_t supported_features = supported_features_default;
    opp_server_create_sdp_record(service_buffer, sdp_create_service_record_handle(), OPP_SERVER_RFCOMM_CHANNEL_NR,
                                  OPP_SERVER_L2CAP_PSM, "OPP Server", 1, supported_features);
    sdp_register_service(service_buffer);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    return 0;
}
/* EXAMPLE_END */
