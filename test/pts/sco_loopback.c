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

#define BTSTACK_FILE__ "sco_loopback.c"

/*
 * sco_loopback.c : Minimal test sending / receiving SCO packets
 */ 

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hci_cmd.h"
#include "hci.h"
#include "btstack_debug.h"
#include "btstack_event.h"
  
static uint16_t  sco_handle = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void try_send_sco(void){
    if (!sco_handle) return;
    if (!hci_can_send_sco_packet_now()) {
        printf("try_send_sco, cannot send now\n");
        return;
    }
    const int frames_per_packet = 20;
    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
    // set handle + flags
    little_endian_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = frames_per_packet;
    int i;
    for (i=0;i<frames_per_packet;i++){
        sco_packet[3+i] = i;
    }
    hci_send_sco_packet_buffer(frames_per_packet+3);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t event_size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(event_size);
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            // request loopback mode
            hci_send_cmd(&hci_write_loopback_mode, 1);
            break;
        case HCI_EVENT_CONNECTION_COMPLETE:
            if (packet[11]) break;
            sco_handle = little_endian_read_16(packet, 3);
            printf("SCO handle 0x%04x\n", sco_handle);
            try_send_sco();
            break;
        default:
            try_send_sco();
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
