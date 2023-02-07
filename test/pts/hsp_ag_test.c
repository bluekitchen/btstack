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
 
#define BTSTACK_FILE__ "hsp_ag_test.c"

/*
 * hsp_ag_test.c : Tool for testig HSP Audio Gateway with PTS
 */ 

#include <errno.h>
#include <fcntl.h>
#include <net/if_arp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "classic/hsp_ag.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_cmd.h"
#include "l2cap.h"
#include "btstack_stdin.h"
 
static uint32_t   hsp_service_buffer[150/4]; // implicit alignment to 4-byte memory address
static uint8_t    rfcomm_channel_nr = 1;

static char hsp_ag_service_name[] = "Audio Gateway Test";
static bd_addr_t bt_speaker_addr = {0x00, 0x21, 0x3C, 0xAC, 0xF7, 0x38};
static bd_addr_t pts_addr = {0x00,0x1A,0x7D,0xDA,0x71,0x0A};
// static bd_addr_t pts_addr = {0x00,0x1b,0xDC,0x07,0x32,0xEF};

static char hs_cmd_buffer[100];
// prototypes
static void show_usage(void);

static void show_usage(void){
    printf("\n--- Bluetooth HSP AudioGateway Test Console ---\n");
    printf("---\n");
    printf("p - establish service level connection to PTS module\n");
    printf("e - establish service level connection to Bluetooth Speaker\n");
    printf("a - release audio connection\n");
    printf("d - release audio connection\n");
    printf("m - set microphone gain 8\n");
    printf("M - set microphone gain 15\n");
    printf("o - set speaker gain 0\n");
    printf("s - set speaker gain 8\n");
    printf("S - set speaker gain 15\n");
    printf("r - start ringing\n");
    printf("t - stop ringing\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char buffer){
    switch (buffer){
        case 'p':
            printf("Establishing service level connection to PTS module %s...\n", bd_addr_to_str(pts_addr));
            hsp_ag_connect(pts_addr);
            break;
        case 'e':
            printf("Establishing service level connection to Bluetooth Speaker %s...\n", bd_addr_to_str(bt_speaker_addr));
            hsp_ag_connect(bt_speaker_addr);
            break;
        case 'a':
            printf("Establish audio connection\n");
            hsp_ag_establish_audio_connection();
            break;
        case 'd':
            printf("Releasing audio connection\n");
            hsp_ag_disconnect();
            break;
        case 'm':
            printf("Setting microphone gain 8\n");
            hsp_ag_set_microphone_gain(8);
            break;
        case 'M':
            printf("Setting microphone gain 15\n");
            hsp_ag_set_microphone_gain(15);
            break;
        case 'o':
            printf("Setting speaker gain 0\n");
            hsp_ag_set_speaker_gain(0);
            break;
        case 's':
            printf("Setting speaker gain 8\n");
            hsp_ag_set_speaker_gain(8);
            break;
        case 'S':
            printf("Setting speaker gain 15\n");
            hsp_ag_set_speaker_gain(15);
            break;
        case 'r':
            printf("Start ringing\n");
            hsp_ag_start_ringing();
            break;
        case 't':
            printf("Stop ringing\n");
            hsp_ag_stop_ringing();
            break;
        default:
            show_usage();
            break;

    }
}

// Audio Gateway routines 
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t event_size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(event_size);
    switch (event[2]) {
        case HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE:
            if (event[3] == 0){
                printf("Audio connection established.\n\n");
            } else {
                printf("Audio connection establishment failed with status %u\n", event[3]);
            }
            break;
        case HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE:
            if (event[3] == 0){
                printf("Audio connection released.\n\n");
            } else {
                printf("Audio connection releasing failed with status %u\n", event[3]);
            }
            break;
        case HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED:
            printf("Received microphone gain change %d\n", event[3]);
            break;
        case HSP_SUBEVENT_SPEAKER_GAIN_CHANGED:
            printf("Received speaker gain change %d\n", event[3]);
            break;
        case HSP_SUBEVENT_HS_COMMAND:{
            memset(hs_cmd_buffer, 0, sizeof(hs_cmd_buffer));
            int size = event[3] <= sizeof(hs_cmd_buffer)? event[3] : sizeof(hs_cmd_buffer); 
            memcpy(hs_cmd_buffer, &event[4], size - 1);
            printf("Received custom command: \"%s\". \nExit code or call hsp_ag_send_result.\n", hs_cmd_buffer);
            break;
        }
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void) argv;
    
    l2cap_init();
    rfcomm_init();
    hsp_ag_init(rfcomm_channel_nr);
    hsp_ag_register_packet_handler(packet_handler);
    
    // init SDP, create record for SPP and register with SDP
    sdp_init();
    memset((uint8_t *)hsp_service_buffer, 0, sizeof(hsp_service_buffer));
    hsp_ag_create_sdp_record((uint8_t *)hsp_service_buffer, 0x10003, rfcomm_channel_nr, hsp_ag_service_name);
    sdp_register_service((uint8_t *)hsp_service_buffer);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);

    return 0;
}
