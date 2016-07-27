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
 
// *****************************************************************************
//
// IOP Test providing all implemented profiles at once
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stdin_support.h"

#include "btstack.h"

#define NETWORK_TYPE_IPv4       0x0800
#define NETWORK_TYPE_ARP        0x0806

static void show_usage(void){
    printf("\n--- CLI for IOP Testing ---\n");
    printf("HSP AG\n");
    printf("HSP HF\n");
    printf("HFP AG\n");
    printf("HFP HF\n");
    printf("PAN-PANU\n");
    printf("SPP\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    char buffer;
    read(ds->fd, &buffer, 1);
    switch (buffer){
        default:
            show_usage();
            break;

    }
}

static uint8_t pan_service_buffer[200];
static uint8_t spp_service_buffer[200];     // rfcomm 1
static uint8_t hsp_ag_service_buffer[200];  // rfcomm 2
static uint8_t hsp_hs_service_buffer[200];  // rfcomm 3
static uint8_t hfp_ag_service_buffer[200];  // rfcomm 4
static uint8_t hfp_hf_service_buffer[200];  // rfcomm 5

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    l2cap_init();

    sdp_init();

    // Create and register SDP records
    
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

    // set CoD for all this
    gap_set_class_of_device(0x6A0000);  // Networking, Capturing, Audio, Telehpony / Misc

    gap_discoverable_control(1);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(&stdin_process);
    return 0;
}
