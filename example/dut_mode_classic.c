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

#define BTSTACK_FILE__ "dut_mode_classic.c"

// *****************************************************************************
/* EXAMPLE_START(dut_mode_classic): Testing - Enable Device Under Test (DUT) Mode for Classic
 *
 * @text DUT mode can be used for production testing. This example just configures 
 * the Bluetooth Controller for DUT mode.
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
 
static btstack_packet_callback_registration_t hci_event_callback_registration;

/* @section Bluetooth Logic 
 *
 * @text When BTstack is up and running, send Enable Device Under Test Mode Command and
 * print its result.
 */ 

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    static int enable_dut_mode = 0;

    // only handle HCI Events
    if (packet_type != HCI_EVENT_PACKET) return;

    // wait for stack for complete startup
    switch(hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                enable_dut_mode = 1;
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            if (hci_event_command_complete_get_command_opcode(packet) == hci_enable_device_under_test_mode.opcode){
                uint8_t status = hci_event_command_complete_get_return_parameters(packet)[0];
                printf("Enable Device Under Test Mode: %s\n", (status != ERROR_CODE_SUCCESS) ? "Failed" : "OK");
            }
            break;
        default:
            break;
    }

    // enable DUT mode when ready
    if (enable_dut_mode && hci_can_send_command_packet_now()){
        enable_dut_mode = 0;
        hci_send_cmd(&hci_enable_device_under_test_mode);
    }
}

/* @text For more details on discovering remote devices, please see
 * Section on [GAP](../profiles/#sec:GAPdiscoverRemoteDevices).
 */


/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It registers the HCI packet handler and starts the Bluetooth stack.
 */

/* LISTING_START(MainConfiguration): Setup packet handler  */
int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;

    // disable Secure Simple Pairinng
    gap_ssp_set_enable(0);

    // make device connectable
    // @note: gap_connectable_control will be enabled when an L2CAP service 
    // (e.g. RFCOMM) is initialized). Therefore, it's not needed in regular applications
    gap_connectable_control(1);

    // make device discoverable
    gap_discoverable_control(1);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    return 0;
}
/* LISTING_END */
/* EXAMPLE_END */
