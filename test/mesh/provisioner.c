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

#define BTSTACK_FILE__ "provisioner.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mesh/adv_bearer.h"
#include "mesh/pb_adv.h"
#include "mesh/beacon.h"
#include "provisioning.h"
#include "provisioning_provisioner.h"
#include "btstack.h"
#include "btstack_tlv.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static mesh_provisioning_data_t provisioning_data;

// pin entry
static int ui_chars_for_pin; 
static uint8_t ui_pin[17];
static int ui_pin_offset;

static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;

static uint16_t pb_adv_cid;

// PTS defaults
static uint8_t      prov_static_oob_data[16];
static const char * prov_static_oob_string = "00000000000000000102030405060708";

static uint8_t      prov_public_key_data[64];
static const char * prov_public_key_string = "F465E43FF23D3F1B9DC7DFC04DA8758184DBC966204796ECCF0D6CF5E16500CC0201D048BCBBD899EEEFC424164E33C201C2B010CA6B4D43A8A155CAD8ECB279";

static int scan_hex_byte(const char * byte_string){
    int upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(*byte_string);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

static int btstack_parse_hex(const char * string, uint16_t len, uint8_t * buffer){
    int i;
    for (i = 0; i < len; i++) {
        int single_byte = scan_hex_byte(string);
        if (single_byte < 0) return 0;
        string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check seperator after last byte
        if (i == len - 1) {
            return 1;
        }
        // optional seperator
        char separator = *string;
        if (separator == ':' && separator == '-' && separator == ' ') {
            string++;
        }
    }
    return 1;
}

static void btstack_print_hex(const uint8_t * data, uint16_t len, char separator){
    int i;
    for (i=0;i<len;i++){
        printf("%02x", data[i]);
        if (separator){
            printf("%c", separator);
        }
    }
    printf("\n");
}

static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t addr;
    int i;
    int prov_len;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    // dump bd_addr in pts format
                    gap_local_bd_addr(addr);
                    printf("Local addr: %s - ", bd_addr_to_str(addr));
                    for (i=0;i<6;i++) {
                        printf("%02x", addr[i]);
                    }
                    printf("\n");
                    // get tlv
                    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
                    // load provisioning data
                    prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
                    printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
                    // setup scanning
                    gap_set_scan_parameters(0, 0x300, 0x300);
                    gap_start_scan();
                    break;
                default:
                    break;
            }
            break;
    }
}

static void mesh_message_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t public_oob  = 0;
    uint8_t auth_method = 0;
    uint8_t auth_action = 0;
    uint8_t auth_size   = 0;

    uint8_t  auth_static_oob = 0;
    uint16_t auth_output_oob_action;
    uint16_t auth_input_oob_action;

    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN:
                    printf("Provisioner link opened");
                    break;
                case MESH_SUBEVENT_PB_PROV_CAPABILITIES:
                    printf("// Provisioner capabilities\n");
                    public_oob = mesh_subevent_pb_prov_capabilities_get_public_key(packet);
                    if (public_oob){
                        printf("PTS supports Public OOB, select Public OOB\n");
                    } else {
                        printf("PTS does not supports Public OOB, select No Public OOB\n");
                    }
                    auth_static_oob        = mesh_subevent_pb_prov_capabilities_get_static_oob_type(packet);
                    auth_input_oob_action  = mesh_subevent_pb_prov_capabilities_get_input_oob_action(packet);   
                    auth_output_oob_action = mesh_subevent_pb_prov_capabilities_get_output_oob_action(packet);
                    if (auth_output_oob_action){
                        auth_method = 0x02; // Output OOB
                        // find output action
                        int i;
                        for (i=0;i<5;i++){
                            if (auth_output_oob_action & (1<<i)){
                                auth_action = i;
                                auth_size   = mesh_subevent_pb_prov_capabilities_get_output_oob_size(packet);
                                printf("// - Pick Output OOB Action with index %u, size %u\n", i, auth_size);
                                break;
                            }
                        }
                    } else if(auth_input_oob_action){
                        auth_method = 0x03; // Input OOB
                        // find output action
                        int i;
                        for (i=0;i<5;i++){
                            if (auth_input_oob_action & (1<<i)){
                                auth_action = i;
                                auth_size   = mesh_subevent_pb_prov_capabilities_get_input_oob_size(packet);
                                printf("// - Pick Input OOB Action with index %u, size %u\n", i, auth_size);
                                break;
                            }
                        }
                    } else if (auth_static_oob & 1){
                        // set static oob - used for PTS
                        btstack_parse_hex(prov_static_oob_string, 16, prov_static_oob_data);
                        provisioning_provisioner_set_static_oob(pb_adv_cid, 16, prov_static_oob_data);
                        // pick Static OOB method
                        printf("// - Pick Static OOB\n");
                        auth_method = 0x01;
                    }
                    provisioning_provisioner_select_authentication_method(1, 0, public_oob, auth_method, auth_action, auth_size);
                    break;
                case MESH_SUBEVENT_PB_PROV_START_RECEIVE_PUBLIC_KEY_OOB:
                    printf("Simulate Read Public Key OOB\n");
                    btstack_parse_hex(prov_public_key_string, 64, prov_public_key_data);
                    provisioning_provisioner_public_key_oob_received(pb_adv_cid, prov_public_key_data);
                    break;
                case MESH_SUBEVENT_PB_PROV_OUTPUT_OOB_REQUEST:
                    printf("Enter passphrase: ");
                    fflush(stdout);
                    ui_chars_for_pin = 1;
                    ui_pin_offset = 0;
                    break;
                case MESH_SUBEVENT_PB_PROV_START_EMIT_INPUT_OOB:
                    break;
                case MESH_SUBEVENT_PB_PROV_COMPLETE:
                    printf("Provisioning complete\n");
                    break;
                default:
                    break;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            printf("received mesh message\n");
            break;
        default:
            break;
    }
}

static uint8_t device_uuid[16];

static void mesh_unprovisioned_beacon_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != MESH_BEACON_PACKET) return;

    uint16_t oob;
    memcpy(device_uuid, &packet[1], 16);
    oob = big_endian_read_16(packet, 17);
    printf("received unprovisioned device beacon, oob data %x, device uuid: ", oob);
    printf_hexdump(device_uuid, 16);
    pb_adv_create_link(device_uuid);
}

uint8_t      pts_device_uuid[16];
const char * pts_device_uuid_string = "001BDC0810210B0E0A0C000B0E0A0C00";

static void stdin_process(char cmd){
    if (ui_chars_for_pin){
        printf("%c", cmd);
        fflush(stdout);
        if (cmd == '\n'){
            ui_pin[ui_pin_offset] = 0;
            printf("\nSending Pin '%s'\n", ui_pin);
            // provisioning_provisioner_input_oob_complete_alphanumeric(1, ui_pin, ui_pin_offset);
            provisioning_provisioner_input_oob_complete_numeric(1, btstack_atoi((char*)ui_pin));
            ui_chars_for_pin = 0;
        } else {
            ui_pin[ui_pin_offset++] = cmd;
        }
        return;
    }
    switch (cmd){
        default:
            printf("Command: '%c'\n", cmd);
            break;
    }
}

int btstack_main(void);
int btstack_main(void)
{
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // console
    btstack_stdin_setup(stdin_process);

    // crypto
    btstack_crypto_init();

    // mesh
    adv_bearer_init();
    adv_bearer_register_for_network_pdu(&mesh_message_handler);

    beacon_init();
    beacon_register_for_unprovisioned_device_beacons(&mesh_unprovisioned_beacon_handler);
    
    // Provisioning in device role
    provisioning_provisioner_init();
    provisioning_provisioner_register_packet_handler(&mesh_message_handler);

    //
    btstack_parse_hex(pts_device_uuid_string, 16, pts_device_uuid);
    btstack_print_hex(pts_device_uuid, 16, 0);

    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */
