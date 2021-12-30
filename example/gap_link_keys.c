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

#define BTSTACK_FILE__ "gap_link_keys.c"

// *****************************************************************************
/* EXAMPLE_START(gap_link_keys): GAP Link Key Management (Classic)
 *
 * @text Shows how to iterate over the Classic Link Keys stored in NVS
 *       Link Keys are per device-device bonding. If the Bluetooth Controller can be swapped, 
 *       e.g. on desktop systems, a Link Key DB for each Controller is needed.
 *       We need to wait until the Bluetooth Stack has started up and selected 
 *       the correct Link Key DB based on the Controller's BD_ADDR.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;

/* @section GAP Link Key Logic 
 *
 * @text List stored link keys
 */ 
static void list_link_keys(void){
    bd_addr_t  addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;

    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        printf("Link key iterator not implemented\n");
        return;
    }
    printf("Stored link keys: \n");
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)){
        printf("%s - type %u, key: ", bd_addr_to_str(addr), (int) type);
        printf_hexdump(link_key, 16);
    }
    printf(".\n");
    gap_link_key_iterator_done(&it);
}

/* @section Bluetooth Logic 
 *
 * @text Wait for Bluetooth startup before listing the stored link keys
 */ 

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                list_link_keys();
                break;
            }
            break;
        default:
            break;
    }
}

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It registers the HCI packet handler and starts the Bluetooth stack.
 */

/* LISTING_START(MainConfiguration): Setup packet handler for GAP inquiry */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
