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
// ANCS Client Demo
//
// TODO: query full text upon notification using control point
// TODO: present notifications in human readable form
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>

#include "debug.h"
#include "btstack_memory.h"

#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "att.h"
#include "att_server.h"
#include "le_device_db.h"
#include "gap_le.h"
#include "gatt_client.h"
#include "sm.h"
#include "ancs_client_lib.h"

// ancs client profile
#include "ancs_client.h"

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x05, 0x09, 'A', 'N', 'C', 'S', 
    // Service Solicitation, 128-bit UUIDs - ANCS (little endian)
    0x11,0x15,0xD0,0x00,0x2D,0x12,0x1E,0x4B,0x0F,0xA4,0x99,0x4E,0xCE,0xB5,0x31,0xF4,0x05,0x79
};
uint8_t adv_data_len = sizeof(adv_data);

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    sm_event_t * sm_event;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                case BTSTACK_EVENT_STATE:
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("ANCS Client Demo ready.\n");
                    }
                    break;
                case SM_JUST_WORKS_REQUEST:
                    sm_event = (sm_event_t *) packet;
                    sm_just_works_confirm(sm_event->addr_type, sm_event->address);
                    printf("Just Works Confirmed.\n");
                    break;
                case SM_PASSKEY_DISPLAY_NUMBER:
                    sm_event = (sm_event_t *) packet;
                    printf("Passkey display: %06u.\n", sm_event->passkey);
                    break;
            }
            break;
    }
}

static void ancs_callback(ancs_event_t * event){
    const char * attribute_name;
    switch (event->type){
        case ANCS_CLIENT_CONNECTED:
            printf("ANCS Client: Connected\n");
            break;
        case ANCS_CLIENT_DISCONNECTED:
            printf("ANCS Client: Disconnected\n");
            break;
        case ANCS_CLIENT_NOTIFICATION:
            attribute_name = ancs_client_attribute_name_for_id(event->attribute_id);
            if (!attribute_name) break;
            printf("Notification: %s - %s\n", attribute_name, event->text);
            break;
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    
    printf("BTstack ANCS Client starting up...\n");

    // set up l2cap_le
    l2cap_init();
    
    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING ); 

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    
    att_server_register_packet_handler(app_packet_handler);

    // setup GATT client
    gatt_client_init();

    // setup ANCS Client
    ancs_client_init();
    ancs_client_register_callback(&ancs_callback);

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}
