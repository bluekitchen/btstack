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
/* EXAMPLE_START(sm_pairing_central): LE Peripheral - Test pairing combinations
 *
 * @text Depending on the Authentication requiremens and IO Capabilities,
 * the pairing process uses different short and long term key generation method.
 * This example helps explore the different options incl. LE Secure Connections.
 * It scans for advertisements and connects to the first device that lists a 
 * random service.
 */
 // *****************************************************************************


#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"


// We're looking for a remote device that lists this service in the advertisement
// LightBlue assigns 0x1111 as the UUID for a Blank service.
#define REMOTE_SERVICE 0x1111


static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

/* @section GAP LE setup for receiving advertisements
 *
 * @text GAP LE advertisements are received as custom HCI events of the 
 * GAP_EVENT_ADVERTISING_REPORT type. To receive them, you'll need to register
 * the HCI packet handler, as shown in Listing GAPLEAdvSetup.
 */

/* LISTING_START(GAPLEAdvSetup): Setting up GAP LE client for receiving advertisements */
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void sm_pairing_central_setup(void){
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    /**
     * Choose ONE of the following configurations
     */

    // LE Legacy Pairing, Just Works
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
    sm_set_authentication_requirements(0);

    // LE Legacy Pairing, Passkey entry initiator enter, responder (us) displays
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_MITM_PROTECTION);

#ifdef ENABLE_LE_SECURE_CONNECTIONS
    // LE Secure Connetions, Just Works
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);

    // LE Secure Connections, Numeric Comparison
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);

    // LE Legacy Pairing, Passkey entry initiator enter, responder (us) displays
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);
#endif
}

/* LISTING_END */

/* @section HCI packet handler
 * 
 * @text The HCI packet handler has to start the scanning, 
 * and to handle received advertisements. Advertisements are received 
 * as HCI event packets of the GAP_EVENT_ADVERTISING_REPORT type,
 * see Listing GAPLEAdvPacketHandler.  
 */

/* LISTING_START(GAPLEAdvPacketHandler): Scanning and receiving advertisements */

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    hci_con_handle_t con_handle;

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                printf("Start scaning!\n");
                gap_set_scan_parameters(1,0x0030, 0x0030);
                gap_start_scan(); 
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:{
            bd_addr_t address;
            gap_event_advertising_report_get_address(packet, address);
            uint8_t address_type = gap_event_advertising_report_get_address_type(packet);
            uint8_t length = gap_event_advertising_report_get_data_length(packet);
            const uint8_t * data = gap_event_advertising_report_get_data(packet);
            printf("Advertisement event: addr-type %u, addr %s, data[%u] ",
               address_type, bd_addr_to_str(address), length);
            printf_hexdump(data, length);
            if (!ad_data_contains_uuid16(length, (uint8_t *) data, REMOTE_SERVICE)) break;
            printf("Found remote with UUID %04x, connecting...\n", REMOTE_SERVICE);
            gap_stop_scan();
            gap_connect(address,address_type);
            break;
        }
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %u\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %u\n", sm_event_passkey_display_number_get_passkey(packet));
            break;

        case HCI_EVENT_LE_META:
            // wait for connection complete
            if (hci_event_le_meta_get_subevent_code(packet) != HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
            // start pairing
            sm_request_pairing(con_handle);
            break;
        default:
            break;
    }
}
/* LISTING_END */

int btstack_main(void);
int btstack_main(void)
{
    sm_pairing_central_setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
        
    return 0;
}

/* EXAMPLE_END */
