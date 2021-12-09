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

#define BTSTACK_FILE__ "beacon.c"

#include "mesh/beacon.h"

#include <stdio.h>
#include <string.h>

#include "ble/core.h"
#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "gap.h"

#include "mesh/adv_bearer.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_keys.h"

#define BEACON_TYPE_UNPROVISIONED_DEVICE 0
#define BEACON_TYPE_SECURE_NETWORK 1

#define UNPROVISIONED_BEACON_INTERVAL_MS 5000
#define UNPROVISIONED_BEACON_LEN      23

#define SECURE_NETWORK_BEACON_INTERVAL_MIN_MS  10000
#define SECURE_NETWORK_BEACON_INTERVAL_MAX_MS 600000
#define SECURE_NETWORK_BEACON_LEN                 22

// prototypes
static void mesh_secure_network_beacon_run(btstack_timer_source_t * ts);

// bearers
#ifdef ENABLE_MESH_GATT_BEARER
static hci_con_handle_t gatt_bearer_con_handle;
#endif

// beacon
static uint8_t mesh_beacon_data[29];
static uint8_t mesh_beacon_len;
static btstack_timer_source_t   beacon_timer;
static int                      beacon_timer_active;

// unprovisioned device beacon
#ifdef ENABLE_MESH_ADV_BEARER
static const uint8_t * beacon_device_uuid;
static       uint16_t  beacon_oob_information;
static       uint32_t  beacon_uri_hash;
static int             beacon_send_device_beacon;
#endif

static btstack_packet_handler_t unprovisioned_device_beacon_handler;

// secure network beacon
static btstack_crypto_aes128_cmac_t        mesh_secure_network_beacon_cmac_request;
static uint8_t                             mesh_secure_network_beacon_auth_value[16];
static btstack_packet_handler_t            mesh_secure_network_beacon_handler;
static int                                 mesh_secure_network_beacon_active;
#ifdef ENABLE_MESH_ADV_BEARER
static uint8_t                             mesh_secure_network_beacon_validate_buffer[SECURE_NETWORK_BEACON_LEN];
#endif

#ifdef ENABLE_MESH_ADV_BEARER
static void beacon_timer_handler(btstack_timer_source_t * ts){
    // restart timer
    btstack_run_loop_set_timer(ts, UNPROVISIONED_BEACON_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
    beacon_timer_active = 1;
    
    // setup beacon
    mesh_beacon_len = UNPROVISIONED_BEACON_LEN;
    mesh_beacon_data[0] = BEACON_TYPE_UNPROVISIONED_DEVICE;
    (void)memcpy(&mesh_beacon_data[1], beacon_device_uuid, 16);
    big_endian_store_16(mesh_beacon_data, 17, beacon_oob_information);
    big_endian_store_32(mesh_beacon_data, 19, beacon_uri_hash);

    // request to send
    beacon_send_device_beacon = 1;
    adv_bearer_request_can_send_now_for_beacon();
}
#endif

static void mesh_secure_network_beacon_auth_value_calculated(void * arg){
    mesh_subnet_t * mesh_subnet = (mesh_subnet_t *) arg;

    (void)memcpy(&mesh_beacon_data[14],
                 mesh_secure_network_beacon_auth_value, 8);
    mesh_beacon_len = SECURE_NETWORK_BEACON_LEN;

    printf("Secure Network Beacon\n");
    printf("- ");
    printf_hexdump(mesh_beacon_data, mesh_beacon_len);

    mesh_secure_network_beacon_active = 0;
    mesh_subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_AUTH_VALUE;

    mesh_secure_network_beacon_run(NULL);
}

static uint8_t mesh_secure_network_beacon_get_flags(mesh_subnet_t * mesh_subnet){
    uint8_t mesh_flags = 0;
    if (mesh_subnet->key_refresh != MESH_KEY_REFRESH_NOT_ACTIVE){
        mesh_flags |= 1;
    }
    if (mesh_iv_update_active()){
        mesh_flags |= 2;
    }

    return mesh_flags;    
}

static void mesh_secure_network_beacon_setup(mesh_subnet_t * mesh_subnet){
    mesh_beacon_data[0] = BEACON_TYPE_SECURE_NETWORK;
    mesh_beacon_data[1] = mesh_secure_network_beacon_get_flags(mesh_subnet);
    // TODO: pick correct key based on key refresh phase

    (void)memcpy(&mesh_beacon_data[2], mesh_subnet->old_key->network_id, 8);
    big_endian_store_32(mesh_beacon_data, 10, mesh_get_iv_index());
    mesh_network_key_t * network_key = mesh_subnet_get_outgoing_network_key(mesh_subnet);
    btstack_crypto_aes128_cmac_message(&mesh_secure_network_beacon_cmac_request, network_key->beacon_key, 13,
        &mesh_beacon_data[1], mesh_secure_network_beacon_auth_value, &mesh_secure_network_beacon_auth_value_calculated, mesh_subnet);
}

static void mesh_secure_network_beacon_update_interval(mesh_subnet_t * subnet){
    uint32_t min_observation_period_ms = 2 * subnet->beacon_interval_ms;
    uint32_t actual_observation_period = btstack_time_delta(btstack_run_loop_get_time_ms(), subnet->beacon_observation_start_ms);
    
    // The Observation Period in seconds should typically be double the typical Beacon Interval.
    if (actual_observation_period < min_observation_period_ms) return;

    // Expected Number of Beacons (1 beacon per 10 seconds)
    uint16_t expected_number_of_beacons = actual_observation_period / SECURE_NETWORK_BEACON_INTERVAL_MIN_MS;

    // Beacon Interval = Observation Period * (Observed Number of Beacons + 1) / Expected Number of Beacons
    uint32_t new_beacon_interval  =  actual_observation_period * (subnet->beacon_observation_counter + 1) / expected_number_of_beacons;

    if (new_beacon_interval > SECURE_NETWORK_BEACON_INTERVAL_MAX_MS){
        new_beacon_interval = SECURE_NETWORK_BEACON_INTERVAL_MAX_MS;
    }
    else if (new_beacon_interval < SECURE_NETWORK_BEACON_INTERVAL_MIN_MS){
        new_beacon_interval = SECURE_NETWORK_BEACON_INTERVAL_MAX_MS;
    }
    subnet->beacon_interval_ms = new_beacon_interval;
    log_info("New beacon interval %u seconds", (int) (subnet->beacon_interval_ms / 1000));
}

static void mesh_secure_network_beacon_run(btstack_timer_source_t * ts){
    UNUSED(ts);

    uint32_t next_timeout_ms = 0;

    // iterate over all networks
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * subnet = mesh_subnet_iterator_get_next(&it);
        switch (subnet->beacon_state){
            case MESH_SECURE_NETWORK_BEACON_W4_INTERVAL:
                // update beacon interval
                mesh_secure_network_beacon_update_interval(subnet);

                if (mesh_foundation_beacon_get() == 0){
                    // beacon off, continue observing
                    if (next_timeout_ms == 0 || next_timeout_ms > subnet->beacon_interval_ms){
                        next_timeout_ms = subnet->beacon_interval_ms;
                    }
                    break;
                }

                // send new beacon
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE;

                /* fall through */

            case MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE:
                if (mesh_secure_network_beacon_active){
                    // just try again in 10 ms
                    next_timeout_ms = 10;
                    break;
                }
                subnet->beacon_state  = MESH_SECURE_NETWORK_BEACON_W4_AUTH_VALUE;
                mesh_secure_network_beacon_active = 1;
                mesh_secure_network_beacon_setup(subnet);
                break;

            case MESH_SECURE_NETWORK_BEACON_AUTH_VALUE:

#ifdef ENABLE_MESH_ADV_BEARER
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_SEND_ADV;
                adv_bearer_request_can_send_now_for_beacon();
                break;
#endif
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_ADV_SENT;

                /* fall through */

            case MESH_SECURE_NETWORK_BEACON_ADV_SENT:

#ifdef ENABLE_MESH_GATT_BEARER
                if (gatt_bearer_con_handle != HCI_CON_HANDLE_INVALID && mesh_foundation_gatt_proxy_get() != 0){
                    subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_SEND_GATT;
                    gatt_bearer_request_can_send_now_for_beacon();
                    break;
                }
#endif 
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_GATT_SENT;

                /* fall through */

            case MESH_SECURE_NETWORK_BEACON_GATT_SENT:
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

    if (beacon_timer_active){
        btstack_run_loop_remove_timer(&beacon_timer);
        beacon_timer_active = 0;
    }
    
    // setup next run
    if (next_timeout_ms == 0) return;

    btstack_run_loop_set_timer(&beacon_timer, next_timeout_ms);
    btstack_run_loop_set_timer_handler(&beacon_timer, mesh_secure_network_beacon_run);
    btstack_run_loop_add_timer(&beacon_timer);
    beacon_timer_active = 1;
}

#ifdef ENABLE_MESH_ADV_BEARER
static void beacon_handle_secure_beacon_auth_value_calculated(void * arg){
    UNUSED(arg);

    // pass on, if auth value checks out
    if (memcmp(&mesh_secure_network_beacon_validate_buffer[14], mesh_secure_network_beacon_auth_value, 8) == 0) {
        if (mesh_secure_network_beacon_handler){
            (*mesh_secure_network_beacon_handler)(MESH_BEACON_PACKET, 0, mesh_secure_network_beacon_validate_buffer, SECURE_NETWORK_BEACON_LEN);
        }
    }

    // done
    mesh_secure_network_beacon_active = 0;
    mesh_secure_network_beacon_run(NULL);
}

static void beacon_handle_secure_beacon(uint8_t * packet, uint16_t size){
    if (size != SECURE_NETWORK_BEACON_LEN) return;
    
    // lookup subnet and netkey by network id
    uint8_t * beacon_network_id = &packet[2];
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    mesh_subnet_t * subnet = NULL;
    mesh_network_key_t * network_key = NULL;
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * item = mesh_subnet_iterator_get_next(&it);
        if (memcmp(item->old_key->network_id, beacon_network_id, 8) == 0 ) {
            subnet = item;
            network_key = item->old_key;
        }
        if (item->new_key != NULL && memcmp(item->new_key->network_id, beacon_network_id, 8) == 0 ) {
            subnet = item;
            network_key = item->new_key;
        }
        break;
    }
    if (subnet == NULL) return;

    // count beacon
    subnet->beacon_observation_counter++;

    // check if new flags are set
    uint8_t current_flags = mesh_secure_network_beacon_get_flags(subnet);
    uint8_t new_flags = packet[1] & (~current_flags);

    if (new_flags == 0) return;

    // validate beacon - if crytpo ready
    if (mesh_secure_network_beacon_active) return;

    mesh_secure_network_beacon_active = 1;
    (void)memcpy(mesh_secure_network_beacon_validate_buffer, &packet[0],
                 SECURE_NETWORK_BEACON_LEN);

    btstack_crypto_aes128_cmac_message(&mesh_secure_network_beacon_cmac_request, network_key->beacon_key, 13,
        &mesh_secure_network_beacon_validate_buffer[1], mesh_secure_network_beacon_auth_value, &beacon_handle_secure_beacon_auth_value_calculated, subnet);
}                    

static void beacon_handle_beacon_packet(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    log_info("beacon type %u", packet[0]);
    switch (packet[0]){
        case BEACON_TYPE_UNPROVISIONED_DEVICE:
            if (unprovisioned_device_beacon_handler){
                (*unprovisioned_device_beacon_handler)(packet_type, channel, packet, size);
            }
            break;
        case BEACON_TYPE_SECURE_NETWORK:
            beacon_handle_secure_beacon(packet, size);
            break;
        default:
            break;
    }
}

static void beacon_adv_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    mesh_subnet_iterator_t it;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case HCI_EVENT_MESH_META:
                    switch(packet[2]){
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            if (beacon_send_device_beacon){
                                beacon_send_device_beacon = 0;
                                adv_bearer_send_beacon(mesh_beacon_data, mesh_beacon_len);
                                break;
                            }
                            // secure beacon state machine
                            mesh_subnet_iterator_init(&it);
                            while (mesh_subnet_iterator_has_more(&it)){
                                mesh_subnet_t * subnet = mesh_subnet_iterator_get_next(&it);
                                switch (subnet->beacon_state){
                                    case MESH_SECURE_NETWORK_BEACON_W2_SEND_ADV:
                                        adv_bearer_send_beacon(mesh_beacon_data, mesh_beacon_len);
                                        subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_ADV_SENT;
                                        mesh_secure_network_beacon_run(NULL);
                                        break;
                                    default:
                                        break;
                                }
                            }
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
            beacon_handle_beacon_packet(packet_type, channel, packet, size);
            break;
        default:
            break;
    }
}
#endif

#ifdef ENABLE_MESH_GATT_BEARER
// handle MESH_SUBEVENT_PROXY_DISCONNECTED and MESH_SUBEVENT_CAN_SEND_NOW
static void beacon_gatt_handle_mesh_event(uint8_t mesh_subevent){
    mesh_subnet_iterator_t it;
    mesh_subnet_iterator_init(&it);
    while (mesh_subnet_iterator_has_more(&it)){
        mesh_subnet_t * subnet = mesh_subnet_iterator_get_next(&it);
        switch (subnet->beacon_state){
            case MESH_SECURE_NETWORK_BEACON_W2_SEND_GATT:
                // skip send on MESH_SUBEVENT_PROXY_DISCONNECTED 
                if (mesh_subevent == MESH_SUBEVENT_CAN_SEND_NOW){
                    gatt_bearer_send_beacon(mesh_beacon_data, mesh_beacon_len);
                }
                subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_GATT_SENT;
                mesh_secure_network_beacon_run(NULL);
                break;
            default:
                break;
        }
    }

}

static void beacon_gatt_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t mesh_subevent;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case HCI_EVENT_MESH_META:
                    mesh_subevent = packet[2];
                    switch(mesh_subevent){
                        case MESH_SUBEVENT_PROXY_CONNECTED:
                            gatt_bearer_con_handle = mesh_subevent_proxy_connected_get_con_handle(packet);
                            break;
                        case MESH_SUBEVENT_PROXY_DISCONNECTED:
                            gatt_bearer_con_handle = HCI_CON_HANDLE_INVALID;
                            beacon_gatt_handle_mesh_event(mesh_subevent);
                            break;
                        case MESH_SUBEVENT_CAN_SEND_NOW:
                            beacon_gatt_handle_mesh_event(mesh_subevent);
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
            beacon_handle_beacon_packet(packet_type, channel, packet, size);
            break;
        default:
            break;
    }
}
#endif

void beacon_init(void){
#ifdef ENABLE_MESH_ADV_BEARER
    adv_bearer_register_for_beacon(&beacon_adv_packet_handler);
#endif
#ifdef ENABLE_MESH_GATT_BEARER    
    gatt_bearer_con_handle = HCI_CON_HANDLE_INVALID;
    gatt_bearer_register_for_beacon(&beacon_gatt_packet_handler);
#endif
}

/**
 * Start Unprovisioned Device Beacon
 */
void beacon_unprovisioned_device_start(const uint8_t * device_uuid, uint16_t oob_information){
#ifdef ENABLE_MESH_ADV_BEARER
    beacon_oob_information = oob_information;
    if (device_uuid){
        beacon_device_uuid = device_uuid;
        beacon_timer.process = &beacon_timer_handler;
        btstack_run_loop_remove_timer(&beacon_timer);
        beacon_timer_handler(&beacon_timer);
    }
#endif
}

/**
 * Stop Unprovisioned Device Beacon
 */
void beacon_unprovisioned_device_stop(void){
#ifdef ENABLE_MESH_ADV_BEARER
    btstack_run_loop_remove_timer(&beacon_timer);
    beacon_timer_active = 0;
#endif
}

// secure network beacons

void beacon_secure_network_start(mesh_subnet_t * mesh_subnet){
    // default interval
    mesh_subnet->beacon_interval_ms = SECURE_NETWORK_BEACON_INTERVAL_MIN_MS;
    mesh_subnet->beacon_observation_start_ms = btstack_run_loop_get_time_ms();
    mesh_subnet->beacon_observation_counter = 0;
    if (mesh_foundation_beacon_get()){
        mesh_subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_W2_AUTH_VALUE;        
    } else {
        mesh_subnet->beacon_state = MESH_SECURE_NETWORK_BEACON_GATT_SENT;        
    }

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
