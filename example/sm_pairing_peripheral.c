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

#define BTSTACK_FILE__ "sm_pairing_peripheral.c"

// *****************************************************************************
/* EXAMPLE_START(sm_pairing_peripheral): LE Peripheral - Test Pairing Methods
 *
 * @text Depending on the Authentication requiremens and IO Capabilities,
 * the pairing process uses different short and long term key generation method.
 * This example helps explore the different options incl. LE Secure Connections.
 */
 // *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "sm_pairing_peripheral.h"
#include "btstack.h"

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager and configures the ATT Server with the pre-compiled
 * ATT Database generated from $sm_pairing_peripheral.gatt$. Finally, it configures the advertisements 
 * and boots the Bluetooth stack. 
 * In this example, the Advertisement contains the Flags attribute, the device name, and a 16-bit (test) service 0x1111
 * The flag 0x06 indicates: LE General Discoverable Mode and BR/EDR not supported.
 * Various examples for IO Capabilites and Authentication Requirements are given below.
 */
 
/* LISTING_START(MainConfiguration): Setup stack to advertise */
static btstack_packet_callback_registration_t sm_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

const uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // Name
    0x0b, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'S', 'M', ' ', 'P', 'a', 'i', 'r', 'i', 'n', 'g', 
    // Incomplete List of 16-bit Service Class UUIDs -- 1111 - only valid for testing!
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x11, 0x11,
};
const uint8_t adv_data_len = sizeof(adv_data);

static void sm_peripheral_setup(void){

    l2cap_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);

    // setup GATT Client
    gatt_client_init();

    // setup advertisements
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
    gap_advertisements_enable(1);

    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // register for ATT
    att_server_register_packet_handler(packet_handler);


    // Configuration

    // Enable mandatory authentication for GATT Client
    // - if un-encrypted connections are not supported, e.g. when connecting to own device, this enforces authentication
    // gatt_client_set_required_security_level(LEVEL_2);

    /**
     * Choose ONE of the following configurations
     * Bonding is disabled to allow for repeated testing. It can be enabled by or'ing
     * SM_AUTHREQ_BONDING to the authentication requirements like this:
     * sm_set_authentication_requirements( X | SM_AUTHREQ_BONDING)
     */

    // LE Legacy Pairing, Just Works
    // sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    // sm_set_authentication_requirements(0);

    // LE Legacy Pairing, Passkey entry initiator enter, responder (us) displays
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_MITM_PROTECTION);
    // sm_use_fixed_passkey_in_display_role(123456);

#ifdef ENABLE_LE_SECURE_CONNECTIONS

    // enable LE Secure Connections Only mode - disables Legacy pairing
    // sm_set_secure_connections_only_mode(true);

    // LE Secure Connections, Just Works
    // sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION);

    // LE Secure Connections, Numeric Comparison
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_YES_NO);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);

    // LE Secure Pairing, Passkey entry initiator enter, responder (us) displays
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);
    // sm_use_fixed_passkey_in_display_role(123456);

    // LE Secure Pairing, Passkey entry initiator displays, responder (us) enter
    // sm_set_io_capabilities(IO_CAPABILITY_KEYBOARD_ONLY);
    // sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION|SM_AUTHREQ_MITM_PROTECTION);
#endif
}

/* LISTING_END */

/* 
 * @section Packet Handler
 *
 * @text The packet handler is used to:
 *        - report connect/disconnect
 *        - handle Security Manager events
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t con_handle;
    bd_addr_t addr;
    bd_addr_type_t addr_type;
    uint8_t status;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    printf("Connection complete\n");
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    UNUSED(con_handle);

                    // for testing, choose one of the following actions

                    // manually start pairing
                    // sm_request_pairing(con_handle);

                    // gatt client request to authenticated characteristic in sm_pairing_central (short cut, uses hard-coded value handle)
                    // gatt_client_read_value_of_characteristic_using_value_handle(&packet_handler, con_handle, 0x0009);

                    // general gatt client request to trigger mandatory authentication
                    // gatt_client_discover_primary_services(&packet_handler, con_handle);
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            printf("Identity resolving failed\n");
            break;
        case SM_EVENT_PAIRING_STARTED:
            printf("Pairing started\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing failed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, authentication failure with reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;
        case SM_EVENT_REENCRYPTION_STARTED:
            sm_event_reencryption_complete_get_address(packet, addr);
            printf("Bonding information exists for addr type %u, identity addr %s -> re-encryption started\n",
                   sm_event_reencryption_started_get_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_REENCRYPTION_COMPLETE:
            switch (sm_event_reencryption_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Re-encryption complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Re-encryption failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Re-encryption failed, disconnected\n");
                    break;
                case ERROR_CODE_PIN_OR_KEY_MISSING:
                    printf("Re-encryption failed, bonding information missing\n\n");
                    printf("Assuming remote lost bonding information\n");
                    printf("Deleting local bonding information to allow for new pairing...\n");
                    sm_event_reencryption_complete_get_address(packet, addr);
                    addr_type = sm_event_reencryption_started_get_addr_type(packet);
                    gap_delete_bonding(addr_type, addr);
                    break;
                default:
                    break;
            }
            break;
        case GATT_EVENT_QUERY_COMPLETE:
            status = gatt_event_query_complete_get_att_status(packet);
            switch (status){
                case ATT_ERROR_INSUFFICIENT_ENCRYPTION:
                    printf("GATT Query failed, Insufficient Encryption\n");
                    break;
                case ATT_ERROR_INSUFFICIENT_AUTHENTICATION:
                    printf("GATT Query failed, Insufficient Authentication\n");
                    break;
                case ATT_ERROR_BONDING_INFORMATION_MISSING:
                    printf("GATT Query failed, Bonding Information Missing\n");
                    break;
                case ATT_ERROR_SUCCESS:
                    printf("GATT Query successful\n");
                    break;
                default:
                    printf("GATT Query failed, status 0x%02x\n", gatt_event_query_complete_get_att_status(packet));
                    break;
            }
            break;
        default:
            break;
    }
}
/* LISTING_END */

int btstack_main(void);
int btstack_main(void)
{
    sm_peripheral_setup();

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */
