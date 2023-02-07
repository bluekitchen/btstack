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

#define BTSTACK_FILE__ "iopt.c"

/*
 * iopt.c : IOP Test providing all implemented profiles at once
 */ 


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "btstack_stdin.h"

#include "btstack.h"

//#define AVRCP_BROWSING_ENABLED

#define NETWORK_TYPE_IPv4       0x0800
#define NETWORK_TYPE_ARP        0x0806

static void show_usage(void){
    printf("\n--- CLI for IOP Testing ---\n");
    printf("A2DP Sink\n");
    printf("A2DP Source\n");
    printf("AVRCP Controller\n");
    printf("AVRCP Target\n");
    printf("HSP AG\n");
    printf("HSP HF\n");
    printf("HFP AG\n");
    printf("HFP HF\n");
    printf("PAN-PANU\n");
    printf("SPP\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char buffer){
    switch (buffer){
        default:
            show_usage();
            break;

    }
}

static uint8_t device_id_sdp_service_buffer[100];
static uint8_t pan_service_buffer[200];
static uint8_t spp_service_buffer[200];     // rfcomm 1
static uint8_t hsp_ag_service_buffer[200];  // rfcomm 2
static uint8_t hsp_hs_service_buffer[200];  // rfcomm 3
static uint8_t hfp_ag_service_buffer[200];  // rfcomm 4
static uint8_t hfp_hf_service_buffer[200];  // rfcomm 5
static uint8_t a2dp_sink_service_buffer[150];
static uint8_t a2dp_source_service_buffer[150];
static uint8_t avrcp_controller_service_buffer[200];
static uint8_t avrcp_target_service_buffer[200];


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argv;
    (void)argc;

    l2cap_init();

    sdp_init();

    // Create and register SDP records

    // See https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers if you don't have a USB Vendor ID and need a Bluetooth Vendor ID
    // device info: BlueKitchen GmbH, product 1, version 1
    device_id_create_sdp_record(device_id_sdp_service_buffer, 0x10000, DEVICE_ID_VENDOR_ID_SOURCE_BLUETOOTH, BLUETOOTH_COMPANY_ID_BLUEKITCHEN_GMBH, 1, 1);
    sdp_register_service(device_id_sdp_service_buffer);
    
    spp_create_sdp_record((uint8_t*) spp_service_buffer, 0x10001, 1, "SPP");
    sdp_register_service((uint8_t*)spp_service_buffer);

    uint16_t network_packet_types[] = { NETWORK_TYPE_IPv4, NETWORK_TYPE_ARP, 0};    // 0 as end of list
    pan_create_panu_sdp_record(pan_service_buffer, 0x10002, network_packet_types, NULL, NULL, BNEP_SECURITY_NONE);
    sdp_register_service((uint8_t*)pan_service_buffer);

    hsp_ag_create_sdp_record((uint8_t *)hsp_ag_service_buffer, 0x10003,  2, "HSP AG");
    sdp_register_service((uint8_t *)hsp_ag_service_buffer);

    hsp_hs_create_sdp_record((uint8_t *)hsp_hs_service_buffer, 0x10004, 3, "HSP HS", 0);
    sdp_register_service((uint8_t *)hsp_hs_service_buffer);

    hfp_ag_create_sdp_record((uint8_t *)hfp_ag_service_buffer, 0x10005, 4, "HFP AG", 0, 0, 1);
    sdp_register_service((uint8_t *)hfp_ag_service_buffer);
    
    hfp_hf_create_sdp_record((uint8_t *)hfp_hf_service_buffer, 0x10006, 5, "HFP HS", 0, 1);
    sdp_register_service((uint8_t *)hfp_hf_service_buffer);    

    a2dp_sink_create_sdp_record(a2dp_sink_service_buffer, 0x10007, 1, NULL, NULL);
    sdp_register_service(a2dp_sink_service_buffer);
    
    uint16_t controller_supported_features = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
#ifdef AVRCP_BROWSING_ENABLED
    controller_supported_features |= AVRCP_FEATURE_MASK_BROWSING;
#endif
    avrcp_controller_create_sdp_record(avrcp_controller_service_buffer, 0x10008, controller_supported_features, NULL, NULL);
    sdp_register_service(avrcp_controller_service_buffer);

    a2dp_source_create_sdp_record(a2dp_source_service_buffer, 0x10009, 1, NULL, NULL);
    sdp_register_service(a2dp_source_service_buffer);

    uint16_t target_supported_features = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
#ifdef AVRCP_BROWSING_ENABLED
    target_supported_features |= AVRCP_FEATURE_MASK_BROWSING;
#endif
    avrcp_target_create_sdp_record(avrcp_target_service_buffer, 0x1000a, target_supported_features, NULL, NULL);
    sdp_register_service(avrcp_target_service_buffer);

    // set CoD for all this
    gap_set_class_of_device(0x6E0000);  // Networking, Rendering (Spekaer), Capturing (Microphone), Audio, Telehpony / Misc

    gap_discoverable_control(1);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(&stdin_process);
    return 0;
}
