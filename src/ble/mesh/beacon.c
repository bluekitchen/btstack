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
#include "mesh_keys.h"

#define UNPROVISIONED_BEACON_INTERVAL_MS 5000

#define BEACON_TYPE_UNPROVISIONED_DEVICE 0
#define BEACON_TYPE_SECURE_NETWORK 1

#define SECURE_BEACON_INTERVAL_MIN_MS 10000


// beacon
static uint8_t mesh_beacon_data[29];
static uint8_t mesh_beacon_len;
static btstack_timer_source_t   beacon_timer;

// unprovisioned device beacon
static const uint8_t * beacon_device_uuid;
static       uint16_t  beacon_oob_information;
static       uint32_t  beacon_uri_hash;

static btstack_packet_handler_t unprovisioned_device_beacon_handler;

// secure network beacon
static btstack_crypto_aes128_cmac_t        mesh_secure_network_beacon_cmac_request;
static uint8_t                             mesh_secure_network_beacon_auth_value[16];
static btstack_packet_handler_t            mesh_secure_network_beacon_handler;
static int                                 mesh_secure_network_beacon_active;

static void beacon_timer_handler(btstack_timer_source_t * ts){
    // restart timer
    btstack_run_loop_set_timer(ts, UNPROVISIONED_BEACON_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);

    // setup beacon
    mesh_beacon_len = 23;
    mesh_beacon_data[0] = BEACON_TYPE_UNPROVISIONED_DEVICE;
    memcpy(&mesh_beacon_data[1], beacon_device_uuid, 16);
    big_endian_store_16(mesh_beacon_data, 17, beacon_oob_information);
    big_endian_store_32(mesh_beacon_data, 19, beacon_uri_hash);

    // request to send
    adv_bearer_request_can_send_now_for_mesh_beacon();
}

static void mesh_secure_network_beacon_auth_value_calculated(void * arg){
    mesh_network_key_t * mesh_network_key = (mesh_network_key_t *) arg;

    memcpy(&mesh_beacon_data[14], mesh_secure_network_beacon_auth_value, 8);
    mesh_beacon_len = 22;

    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_beacon_data, sizeof(mesh_beacon_len));

    mesh_network_key->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_SEND_ADV;
    adv_bearer_request_can_send_now_for_mesh_beacon();
}

static void mesh_secure_network_beacon_setup(mesh_network_key_t * mesh_network_key){
    mesh_network_key->beacon_state  = MESH_SECURE_NETWORK_BEACON_W4_AUTH_VALUE;

    // calculate mesh flags
    uint8_t mesh_flags = 0;
    if (mesh_network_key->key_refresh != MESH_KEY_REFRESH_NOT_ACTIVE){
        mesh_flags |= 1;
    }

    // TODO: set bit 1 if IV Update is active

    mesh_beacon_data[0] = BEACON_TYPE_SECURE_NETWORK;
    mesh_beacon_data[1] = mesh_flags;
    memcpy(&mesh_beacon_data[2], mesh_network_key->network_id, 8);
    big_endian_store_32(mesh_beacon_data, 10, mesh_get_iv_index());
    btstack_crypto_aes128_cmac_message(&mesh_secure_network_beacon_cmac_request, mesh_network_key->beacon_key, 13,
        &mesh_beacon_data[1], mesh_secure_network_beacon_auth_value, &mesh_secure_network_beacon_auth_value_calculated, mesh_network_key);
}

static void mesh_secure_network_beacon_run(btstack_timer_source_t * ts){
    UNUSED(ts);

    uint32_t next_timeout_ms = 0;

    // iterate over all networks
    mesh_network_key_iterator_t it;
    mesh_network_key_iterator_init(&it);
    while (mesh_network_key_iterator_has_more(&it)){
        mesh_network_key_t * subnet = mesh_network_key_iterator_get_next(&it);
        switch (subnet->beacon_state){
            case MESH_SECURE_NETWORK_BEACON_W4_INTERVAL:
                // TODO: calculate beacon interval

                // send new beacon
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE;

                /** Explict Fall-through */

            case MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE:
                if (mesh_secure_network_beacon_active){
                    // just try again in 10 ms
                    next_timeout_ms = 10;
                    break;
                }
                mesh_secure_network_beacon_active = 1;
                mesh_secure_network_beacon_setup(subnet);
                break;

            case MESH_SECURE_NETWORK_BEACON_SENT:
                // now, start listening for beacons
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_W4_INTERVAL;
                // and request timeout
                if (next_timeout_ms == 0 || next_timeout_ms > subnet->beacon_interval_ms){
                    next_timeout_ms = subnet->beacon_interval_ms;
                }
                break;
            default:
                break;
        }
    }

    // setup next run
    if (next_timeout_ms == 0) return;

    btstack_run_loop_set_timer(&beacon_timer, 10);
    btstack_run_loop_set_timer_handler(&beacon_timer, mesh_secure_network_beacon_run);
    btstack_run_loop_add_timer(&beacon_timer);
}

static void beacon_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    mesh_network_key_iterator_t it;
    int secure_beacon_run = 0;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case HCI_EVENT_MESH_META:
                    switch(packet[2]){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            adv_bearer_send_mesh_beacon(mesh_beacon_data, mesh_beacon_len);
                            mesh_secure_network_beacon_active = 0;

                            // secure beacon state machine
                            mesh_network_key_iterator_init(&it);
                            while (mesh_network_key_iterator_has_more(&it)){
                                mesh_network_key_t * subnet = mesh_network_key_iterator_get_next(&it);
                                switch (subnet->beacon_state){
                                    case MESH_SECURE_NETWORK_BEACON_W2_SEND_ADV:
                                        // send 
                                        subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_SENT;
                                        secure_beacon_run = 1;
                                        break;
                                    default:
                                        break;
                                }
                            }

                            if (!secure_beacon_run) break;
                            mesh_secure_network_beacon_run(NULL);
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
                    if (mesh_secure_network_beacon_handler){
                        (*mesh_secure_network_beacon_handler)(packet_type, channel, packet, size);
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

void beacon_secure_network_start(mesh_network_key_t * mesh_network_key){
    // default interval
    mesh_network_key->beacon_interval_ms = SECURE_BEACON_INTERVAL_MIN_MS;
    mesh_network_key->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE;
    mesh_network_key->beacon_observation_start_ms = btstack_run_loop_get_time_ms();
    mesh_network_key->beacon_observation_counter = 0;

    // start sending
    mesh_secure_network_beacon_run(NULL);
}

// register handler
void beacon_register_for_unprovisioned_device_beacons(btstack_packet_handler_t packet_handler){
    unprovisioned_device_beacon_handler = packet_handler;
}

void beacon_register_for_secure_network_beacons(btstack_packet_handler_t packet_handler){
    mesh_secure_network_beacon_handler = packet_handler;
}
