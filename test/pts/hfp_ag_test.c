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
// Minimal test for HSP Audio Gateway (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************

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
#include "hfp_ag.h"
#include "stdin_support.h"
 

const uint32_t   hfp_service_buffer[150/4]; // implicit alignment to 4-byte memory address
const uint8_t    rfcomm_channel_nr = 1;
const char hfp_ag_service_name[] = "BTstack HFP AG Test";

static bd_addr_t device_addr;
static bd_addr_t pts_addr = {0x00,0x1b,0xDC,0x07,0x32,0xEF};
static bd_addr_t speaker_addr = {0x00, 0x21, 0x3C, 0xAC, 0xF7, 0x38};
static uint8_t codecs[1] = {HFP_CODEC_CVSD};
static uint16_t handle = -1;

static int ag_indicators_nr = 7;
static hfp_ag_indicator_t ag_indicators[] = {
    // index, name, min range, max range, status, mandatory, enabled, status changed
    {1, "service",   0, 1, 1, 0, 0, 0},
    {2, "call",      0, 1, 0, 1, 1, 0},
    {3, "callsetup", 0, 3, 0, 1, 1, 0},
    {4, "battchg",   0, 5, 3, 0, 0, 0},
    {5, "signal",    0, 5, 5, 0, 0, 0},
    {6, "roam",      0, 1, 0, 0, 0, 0},
    {7, "callheld",  0, 2, 0, 1, 1, 0}
};

static int call_hold_services_nr = 5;
static const char* call_hold_services[] = {"1", "1x", "2", "2x", "3"};

static int hf_indicators_nr = 2;
static hfp_generic_status_indicator_t hf_indicators[] = {
    {1, 1},
    {2, 1},
};

char cmd;
// prototypes
static void show_usage();

// Testig User Interface 
static void show_usage(void){
    printf("\n--- Bluetooth HFP Hands-Free (HF) unit Test Console ---\n");
    printf("---\n");
    
    printf("a - establish HFP connection to PTS module\n");
    // printf("A - release HFP connection to PTS module\n");
    
    printf("z - establish HFP connection to speaker\n");
    // printf("Z - release HFP connection to speaker\n");
    
    printf("b - establish AUDIO connection\n");
    printf("B - release AUDIO connection\n");
    
    printf("c - simulate incoming call\n");
    printf("C - simulate call dropped\n");

    printf("d - report AG failure\n");

    printf("e - answer call on AG\n");

    printf("r - disable in-band ring tone\n");
    printf("R - enable in-band ring tone\n");

    printf("f - Disable cellular network\n");
    printf("F - Enable cellular network\n");

    printf("g - Set signal strength to 0\n");
    printf("G - Set signal strength to 5\n");

    printf("h - Disable roaming\n");
    printf("H - Enable roaming\n");

    printf("i - Set battery level to 3\n");
    printf("I - Set battery level to 5\n");

    printf("t - terminate connection\n");

    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static int stdin_process(struct data_source *ds){
    read(ds->fd, &cmd, 1);
    switch (cmd){
        case 'a':
            memcpy(device_addr, pts_addr, 6);
            printf("Establish HFP service level connection to PTS module %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_service_level_connection(device_addr);
            break;
        case 'A':
            printf("Release HFP service level connection.\n");
            hfp_ag_release_service_level_connection(device_addr);
            break;
        case 'z':
            memcpy(device_addr, speaker_addr, 6);
            printf("Establish HFP service level connection to %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_service_level_connection(device_addr);
            break;
        case 'Z':
            printf("Release HFP service level connection to %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_release_service_level_connection(device_addr);
            break;
        case 'b':
            printf("Establish Audio connection %s...\n", bd_addr_to_str(device_addr));
            hfp_ag_establish_audio_connection(device_addr);
            break;
        case 'B':
            printf("Release Audio connection.\n");
            hfp_ag_release_audio_connection(device_addr);
            break;
        case 'c':
            printf("Simulate incoming call\n");
            hfp_ag_incoming_call();
            break;
        case 'C':
            printf("Simulate terminate call\n");
            hfp_ag_call_dropped();
            break;
        case 'd':
            printf("Report AG failure\n");
            hfp_ag_report_extended_audio_gateway_error_result_code(device_addr, HFP_CME_ERROR_AG_FAILURE);
            break;
        case 'e':
            printf("Answer call on AG\n");
            hfp_ag_answer_incoming_call();
            break;
        case 'f':
            printf("Disable cellular network\n");
            hfp_ag_set_registration_status(0);
            break;
        case 'F':
            printf("Enable cellular network\n");
            hfp_ag_set_registration_status(1);
            break;
        case 'g':
            printf("Set signal strength to 0\n");
            hfp_ag_set_signal_strength(0);
            break;
        case 'G':
            printf("Set signal strength to 5\n");
            hfp_ag_set_signal_strength(5);
            break;
        case 'h':
            printf("Disable roaming\n");
            hfp_ag_set_roaming_status(0);
            break;
        case 'H':
            printf("Enable roaming\n");
            hfp_ag_set_roaming_status(1);
            break;
        case 'i':
            printf("Set battery level to 3\n");
            hfp_ag_set_battery_level(3);
            break;
        case 'I':
            printf("Set battery level to 5\n");
            hfp_ag_set_battery_level(5);
            break;
        case 'r':
            printf("Disable in-band ring tone\n");
            hfp_ag_set_use_in_band_ring_tone(0);
            break;
        case 'R':
            printf("Enable in-band ring tone\n");
            hfp_ag_set_use_in_band_ring_tone(1);
            break;
        case 't':
            printf("Terminate HCI connection.\n");
            gap_disconnect(handle);
        default:
            show_usage();
            break;
    }

    return 0;
}


static void packet_handler(uint8_t * event, uint16_t event_size){

    if (event[0] == RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE){
        handle = READ_BT_16(event, 9);
        printf("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE received for handle 0x%04x\n", handle);
        return;
    }

    if (event[0] != HCI_EVENT_HFP_META) return;
    if (event[3]){
        printf("ERROR, status: %u\n", event[3]);
        return;
    }

    switch (event[2]) {   
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            printf("Service level connection established.\n\n");
            break;
        case HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED:
            printf("Service level connection released.\n\n");
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED:
            printf("\n** Audio connection established **\n\n");
            break;
        case HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED:
            printf("\n** Audio connection released **\n\n");
            break;
        case HFP_SUBEVENT_START_RINGINIG:
            printf("\n** Start Ringing **\n\n");
            break;        
        case HFP_SUBEVENT_STOP_RINGINIG:
            printf("\n** Stop Ringing **\n\n");
            break;        
        default:
            // printf("event not handled %u\n", event[2]);
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    // init L2CAP
    l2cap_init();
    rfcomm_init();
    
    hfp_ag_init(rfcomm_channel_nr, 0x3ef | (1<<HFP_AGSF_HF_INDICATORS), codecs, sizeof(codecs), 
        ag_indicators, ag_indicators_nr, 
        hf_indicators, hf_indicators_nr, 
        call_hold_services, call_hold_services_nr);

    hfp_ag_register_packet_handler(packet_handler);

    sdp_init();
    // init SDP, create record for SPP and register with SDP
    memset((uint8_t *)hfp_service_buffer, 0, sizeof(hfp_service_buffer));
    hfp_ag_create_sdp_record((uint8_t *)hfp_service_buffer, rfcomm_channel_nr, hfp_ag_service_name, 0, 0);

    sdp_register_service_internal(NULL, (uint8_t *)hfp_service_buffer);
    

    // pre-select pts
    memcpy(device_addr, pts_addr, 6);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    btstack_stdin_setup(stdin_process);
    return 0;
}
