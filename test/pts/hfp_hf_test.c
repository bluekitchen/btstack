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
// Minimal test for HSP Headset (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "hci.h"
#include "l2cap.h"
#include "rfcomm.h"
#include "sdp.h"
#include "debug.h"
#include "hfp_hf.h"
#include "stdin_support.h"

const uint32_t   hfp_service_buffer[150/4]; // implicit alignment to 4-byte memory address
const uint8_t    rfcomm_channel_nr = 1;
const char hfp_hf_service_name[] = "BTstack HF Test";

static bd_addr_t pts_addr = {0x00,0x1b,0xDC,0x07,0x32,0xEF};
static bd_addr_t local_mac = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
static bd_addr_t phone = {0xD8,0xBb,0x2C,0xDf,0xF1,0x08};
// prototypes
static void show_usage();


// Testig User Interface 
static void show_usage(void){
    printf("\n--- Bluetooth HFP Hands-Free (HF) unit Test Console ---\n");
    printf("---\n");
    printf("p - establish HFP connection to PTS module\n");
    printf("e - establish HFP connection to local mac\n");
    printf("d - release HFP connection\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static int stdin_process(struct data_source *ds){
    char buffer;
    read(ds->fd, &buffer, 1);

    switch (buffer){
        case 'p':
            printf("Establishing HFP connection to PTS module %s...\n", bd_addr_to_str(pts_addr));
            hfp_hf_connect(pts_addr);
            break;
        case 'e':
            printf("Establishing HFP connection to %s...\n", bd_addr_to_str(phone));
            hfp_hf_connect(phone);
            break;
        case 'd':
            printf("Releasing HFP connection.\n");
            hfp_hf_disconnect(pts_addr);
            break;
        default:
            show_usage();
            break;
    }

    return 0;
}

void packet_handler(uint8_t * event, uint16_t event_size){
    switch (event[2]) {   
        default:
            printf("event not handled %u\n", event[2]);
            break;
    }
}

int btstack_main(int argc, const char * argv[]){
    // init L2CAP
    uint8_t codecs[1] = {HFP_Codec_CVSD};
    uint16_t indicators[1] = {0x01};

    l2cap_init();
    rfcomm_init();
    
    // hfp_hf_init(rfcomm_channel_nr, HFP_Default_HF_Supported_Features, codecs, sizeof(codecs), indicators, sizeof(indicators)/sizeof(uint16_t), 1);
    hfp_hf_init(rfcomm_channel_nr, 0x0009, codecs, sizeof(codecs), indicators, sizeof(indicators)/sizeof(uint16_t), 1);

    hfp_register_packet_handler(packet_handler);

    sdp_init();
    // init SDP, create record for SPP and register with SDP
    memset((uint8_t *)hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_hf_create_service((uint8_t *)hfp_service_buffer, rfcomm_channel_nr, hfp_hf_service_name, 0);
    sdp_register_service_internal(NULL, (uint8_t *)hfp_service_buffer);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    btstack_stdin_setup(stdin_process);

    return 0;
}
