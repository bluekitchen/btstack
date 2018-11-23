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

#define __BTSTACK_FILE__ "mesh_proxy_server.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble/mesh/gatt_bearer.h"
#include "ble/gatt-service/mesh_proxy_service_server.h"
#include "mesh_proxy_server.h"
#include "btstack_config.h"
#include "btstack.h"
#include "provisioning.h"
#include "provisioning_device.h"
#include "btstack_tlv.h"

static mesh_provisioning_data_t provisioning_data;
static const btstack_tlv_t * btstack_tlv_singleton_impl;
static void *                btstack_tlv_singleton_context;
static uint16_t con_handle;
static uint16_t adv_int_min = 0x0030;
static uint16_t adv_int_max = 0x0030;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06, 
    // 16-bit Service UUIDs
    0x03, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8,
    // Service Data (22)
    0x0C, BLUETOOTH_DATA_TYPE_SERVICE_DATA, ORG_BLUETOOTH_SERVICE_MESH_PROXY & 0xff, ORG_BLUETOOTH_SERVICE_MESH_PROXY >> 8, 
          // MESH_IDENTIFICATION_NETWORK_ID_TYPE
          0x00, 
          // Network ID
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t adv_data_len = sizeof(adv_data);


static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth Mesh Provisioning Device Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    switch (cmd){
        case 't':
            printf("disconnect \n");
            gap_disconnect(con_handle);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int prov_len;
    if (packet_type != HCI_EVENT_PACKET) return;
 
    switch (hci_event_packet_get_type(packet)){
        case BTSTACK_EVENT_STATE:{
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    // get tlv
                    btstack_tlv_get_instance(&btstack_tlv_singleton_impl, &btstack_tlv_singleton_context);
                    // load provisioning data
                    prov_len = btstack_tlv_singleton_impl->get_tag(btstack_tlv_singleton_context, 'PROV', (uint8_t *) &provisioning_data, sizeof(mesh_provisioning_data_t));
                    printf("Provisioning data available: %u\n", prov_len ? 1 : 0);
                    
                    if (!prov_len) break;

                    // dynamically store network ID into adv data 
                    uint8_t netid_flipped[8];
                    reverse_64(provisioning_data.network_id, netid_flipped);
                    memcpy(&adv_data[12], netid_flipped, sizeof(netid_flipped));
                    // setup advertisements
                    bd_addr_t null_addr;
                    memset(null_addr, 0, 6);
                    uint8_t adv_type = 0;   // AFV_IND
                    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
                    gap_advertisements_set_data(adv_data_len, (uint8_t*) adv_data);
                    gap_advertisements_enable(1);
                    break;
        }
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

int btstack_main(void);
int btstack_main(void){
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // crypto
    btstack_crypto_init();

    // l2cap
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();

    // setup ATT server
    att_server_init(profile_data, NULL, NULL);    

    // Setup GATT bearere
    gatt_bearer_init();
#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
/* EXAMPLE_END */
