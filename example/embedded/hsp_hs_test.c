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
// Requires HAVE_SCO and HAVE_SCO_OVER_HCI to be defined
//
// Tested working setups: 
// - Ubuntu 14 64-bit, CC2564B connected via FTDI USB-2-UART adapter, 921600 baud
// - Ubuntu 14 64-bit, CSR dongle
// - OS X 10.11, CSR dongle

// Broken setups:
// - OS X 10.11, CC2564B connected via FDTI USB-2-UART adapter, 921600 baud
//   - select(..) blocks > 400 ms -> num completed is received to late -> gaps between audio
//   - looks like bug in select->FTDI driver as it works correct on Linux
// 
// SCO not routed over HCI (yet)
// - CSR UART dongle 
// - Broadcom USB dongle
// - Broadcom UART chipset
// - ..
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>
#include <btstack/sdp_util.h>

#include "sdp.h"
#include "hsp_hs.h"

#include "hci.h"
#include "l2cap.h"
#include "debug.h"

static uint32_t      hsp_service_buffer[150/4]; // implicit alignment to 4-byte memory address
static const uint8_t rfcomm_channel_nr = 1;
static const char    hsp_hs_service_name[] = "Headset Test";
static uint16_t      sco_handle = 0;

static char hs_cmd_buffer[100];

static int phase = 0;

// input signal: pre-computed sine wave, 160 Hz
static const uint8_t sine[] = {
      0,  15,  31,  46,  61,  74,  86,  97, 107, 114,
    120, 124, 126, 126, 124, 120, 114, 107,  97,  86,
     74,  61,  46,  31,  15,   0, 241, 225, 210, 195,
    182, 170, 159, 149, 142, 136, 132, 130, 130, 132,
    136, 142, 149, 159, 170, 182, 195, 210, 225, 241,
};

#if 0
//
// re-compute sine table for input signal
//
#include <math.h>
#define TABLE_SIZE 50
static uint8_t sine[TABLE_SIZE];
static void compute_signal(void){
    #define TABLE_SIZE    (50)
    // create sine wave table
    int i;
    printf("static const uint8_t sine[] = {");
    for( i=0; i<TABLE_SIZE; i++ ) {
        if ((i % 10) == 0) printf("\n    ");
        sine[i] = (int8_t) (127.0 * sin( ((double)i/(double)TABLE_SIZE) * M_PI * 2. ));
        printf("%3u, ", sine[i]);
    }
    printf("\n};\n");
}
#endif

static void try_send_sco(void){
    if (!sco_handle) return;
    if (!hci_can_send_sco_packet_now(sco_handle)) {
        // printf("try_send_sco, cannot send now\n");
        return;
    }

    const int sco_packet_length = hci_get_sco_packet_length();
    const int sco_payload_length = sco_packet_length - 3;
    const int frames_per_packet = sco_payload_length;    // for 8-bit data. for 16-bit data it's /2

    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
    // set handle + flags
    bt_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = sco_payload_length;
    int i;
    for (i=0;i<frames_per_packet;i++){
        sco_packet[3+i] = sine[phase];
        phase++;
        if (phase >= sizeof(sine)) phase = 0;
    }
    hci_send_sco_packet_buffer(sco_packet_length);
    static int count = 0;
    count++;
    if ((count & 15) == 0) printf("Sent %u\n", count);
}

static void sco_packet_handler(uint8_t packet_type, uint8_t * packet, uint16_t size){
    static int count = 0;
    count++;
    if ((count & 15)) return;
    printf("SCO packets %u\n", count);
    // hexdumpf(packet, size);
}

static void packet_handler(uint8_t * event, uint16_t event_size){

    // printf("Packet handler event 0x%02x\n", event[0]);
    
    try_send_sco();
    
    switch (event[0]) {
        case BTSTACK_EVENT_STATE:
            if (event[2] != HCI_STATE_WORKING) break;
            printf("Working!\n");
            break;
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
            // printf("HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS\n");
            // try_send_sco();
            break;
        case DAEMON_EVENT_HCI_PACKET_SENT:
            // printf("DAEMON_EVENT_HCI_PACKET_SENT\n");
            // try_send_sco();
            break;
        case HCI_EVENT_HSP_META:
            switch (event[2]) { 
                case HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE:
                    if (event[3] == 0){
                        sco_handle = READ_BT_16(event, 4);
                        printf("Audio connection established with SCO handle 0x%04x.\n", sco_handle);
                        // try_send_sco();
                    } else {
                        printf("Audio connection establishment failed with status %u\n", event[3]);
                        sco_handle = 0;
                    }
                    break;
                case HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE:
                    if (event[3] == 0){
                        printf("Audio connection released.\n\n");
                        sco_handle = 0;
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
                case HSP_SUBEVENT_RING:
                    printf("HS: RING RING!\n");
                    break;
                case HSP_SUBEVENT_AG_INDICATION:
                    memset(hs_cmd_buffer, 0, sizeof(hs_cmd_buffer));
                    int size = event_size <= sizeof(hs_cmd_buffer)? event_size : sizeof(hs_cmd_buffer); 
                    memcpy(hs_cmd_buffer, &event[3], size - 1);
                    printf("Received custom indication: \"%s\". \nExit code or call hsp_hs_send_result.\n", hs_cmd_buffer);
                    break;
                default:
                    printf("event not handled %u\n", event[2]);
                    break;
            }
            break;
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

#ifdef TABLE_SIZE
    compute_signal();
#endif

    // 8-bit, 2's complement (== int8_t)
    // 16-bit samples probably required for USB: 
    // hci_set_sco_voice_setting(0x0060);

    hci_register_sco_packet_handler(&sco_packet_handler);

    hci_discoverable_control(1);
    hci_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);
    gap_set_local_name("BTstack HSP HS");

    hsp_hs_init(rfcomm_channel_nr);
    hsp_hs_register_packet_handler(packet_handler);
    
    sdp_init();
	memset((uint8_t *)hsp_service_buffer, 0, sizeof(hsp_service_buffer));
    hsp_hs_create_service((uint8_t *)hsp_service_buffer, rfcomm_channel_nr, hsp_hs_service_name, 0);

    sdp_register_service_internal(NULL, (uint8_t *)hsp_service_buffer);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}
