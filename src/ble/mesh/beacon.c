/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "adv_bearer.c"

#include <string.h>

#include "ble/mesh/beacon.h"
#include "ble/mesh/adv_bearer.h"
#include "ble/core.h"
#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "gap.h"

#define UNPROVISIONED_BEACON_INTERVAL_MS 5000

#define BEACON_TYPE_UNPROVISIONED_DEVICE 0
#define BEACON_TYPE_SECURE_NETWORK 1

static const uint8_t * beacon_device_uuid;
static       uint16_t  beacon_oob_information;
static       uint32_t  beacon_uri_hash;
static btstack_timer_source_t beacon_timer;
static btstack_packet_handler_t unprovisioned_device_beacon_handler;
static btstack_packet_handler_t secure_network_beacon_handler;

static void beacon_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t beacon[23];
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case HCI_EVENT_MESH_META:
                    switch(packet[2]){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            beacon[0] = BEACON_TYPE_UNPROVISIONED_DEVICE;
                            memcpy(&beacon[1], beacon_device_uuid, 16);
                            big_endian_store_16(beacon, 17, beacon_oob_information);
                            big_endian_store_32(beacon, 19, beacon_uri_hash);
                            adv_bearer_send_mesh_beacon(beacon, sizeof(beacon));
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case MESH_BEACON_PACKET:
            log_info("beacon type %u", packet[0]);
            switch (packet[0]){
                case BEACON_TYPE_UNPROVISIONED_DEVICE:
                    if (unprovisioned_device_beacon_handler){
                        (*unprovisioned_device_beacon_handler)(packet_type, channel, packet, size);
                    }
                    break;
                case BEACON_TYPE_SECURE_NETWORK:
                    if (secure_network_beacon_handler){
                        (*secure_network_beacon_handler)(packet_type, channel, packet, size);
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void beacon_timer_handler(btstack_timer_source_t * ts){
    // restart timer
    btstack_run_loop_set_timer(ts, UNPROVISIONED_BEACON_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);

    adv_bearer_request_can_send_now_for_mesh_beacon();
}

void beacon_init(void){
    adv_bearer_register_for_mesh_beacon(&beacon_packet_handler);
}

/**
 * Start Unprovisioned Device Beacon
 */
void beacon_unprovisioned_device_start(const uint8_t * device_uuid, uint16_t oob_information){
    beacon_oob_information = oob_information;
    if (device_uuid){
        beacon_device_uuid = device_uuid;
        beacon_timer.process = &beacon_timer_handler;
        beacon_timer_handler(&beacon_timer);
    }
}

/**
 * Stop Unprovisioned Device Beacon
 */
void beacon_unprovisioned_device_stop(void){
    btstack_run_loop_remove_timer(&beacon_timer);
}

// secure network beacons

static btstack_crypto_aes128_cmac_t mesh_secure_network_beacon_cmac_request;
static uint8_t mesh_secure_network_beacon[22];
static uint8_t mesh_secure_network_beacon_auth_value[16];

static void mesh_secure_network_beacon_send_adv_auth_value_calculated(void * arg){
    UNUSED(arg);
    memcpy(&mesh_secure_network_beacon[14], mesh_secure_network_beacon_auth_value, 8);
    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
    adv_bearer_send_mesh_beacon(mesh_secure_network_beacon, sizeof(mesh_secure_network_beacon));
}

static void mesh_secure_network_beacon_send_adv(const char * network_id, const char * beacon_key, int8_t mesh_flags){
    mesh_secure_network_beacon[0] = BEACON_TYPE_SECURE_NETWORK;
    mesh_secure_network_beacon[1] = mesh_flags;
    memcpy(&mesh_secure_network_beacon[2], network_id, 8);
    big_endian_store_32(mesh_secure_network_beacon, 10, mesh_get_iv_index());
    btstack_crypto_aes128_cmac_message(&mesh_secure_network_beacon_cmac_request, beacon_key, 13,
        &mesh_secure_network_beacon[1], mesh_secure_network_beacon_auth_value, &mesh_secure_network_beacon_send_adv_auth_value_calculated, NULL);
}

// crypto context 
static void beacon_secure_network_timer_handler(btstack_timer_source_t * ts){

}

void beacon_secure_network_start(mesh_network_key_t * mesh_network_key){
    // start with default interval
    mesh_network_key->beacon_interval_ms = 10 * 1000;  // 10 seconds
    // set timer for next one

    // 
}

// register handler
void beacon_register_for_unprovisioned_device_beacons(btstack_packet_handler_t packet_handler){
    unprovisioned_device_beacon_handler = packet_handler;
}

void beacon_register_for_secure_network_beacons(btstack_packet_handler_t packet_handler){
    secure_network_beacon_handler = packet_handler;
}
