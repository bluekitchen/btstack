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

#include "ble/mesh/adv_bearer.h"
#include "ble/core.h"
#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "gap.h"

#define NUM_TYPES 3
#define ADV_TIMER_EXTRA_MS 10

typedef enum {
    MESH_NETWORK_ID,
    MESH_BEACON_ID,
    PB_ADV_ID,
    INVALID_ID,
} message_type_id_t;

//
static void adv_bearer_set_mesh_params(void);
static void adv_bearer_start_advertising(const uint8_t * data, uint16_t data_len, uint8_t type);
static void adv_gap_start_advertising(void);

static btstack_packet_callback_registration_t hci_event_callback_registration;

static btstack_packet_handler_t client_callbacks[NUM_TYPES];
static int request_can_send_now[NUM_TYPES];
static int last_sender;

static uint8_t adv_buffer[31];

static btstack_timer_source_t adv_timer;

static int adv_check_interval_ms;

static int adv_bearer_active;
static int adv_gap_active;

// gap advertisement
static uint8_t   gap_advertising_data_length;
static uint8_t * gap_advertising_data;
// set defaults
static uint16_t  gap_adv_int_min    = 0x30;
static uint16_t  gap_adv_int_max    = 0x30;
static uint8_t   gap_adv_type       = 0;
static uint8_t   gap_direct_address_typ;
static bd_addr_t gap_direct_address;
static uint8_t   gap_channel_map    = 0x07;
static uint8_t   gap_filter_policy  = 0;

// round-robin
static void adv_bearer_emit_can_send_now(void){
    if (adv_bearer_active) return;
    int countdown = NUM_TYPES;
    while (countdown--) {
        last_sender++;
        if (last_sender == NUM_TYPES) {
            last_sender = 0;
        }
        if (request_can_send_now[last_sender]){
            request_can_send_now[last_sender] = 0;
            // emit can send now
            log_info("can send now");
            uint8_t event[3];
            event[0] = HCI_EVENT_MESH_META;
            event[1] = 1;
            event[2] = MESH_SUBEVENT_CAN_SEND_NOW;
            (*client_callbacks[last_sender])(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
            return;
        }
    }
}

static void adv_bearer_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    const uint8_t * data;
    int data_len;
    message_type_id_t type_id;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case GAP_EVENT_ADVERTISING_REPORT:
                    // only non-connectable ind
                    if (gap_event_advertising_report_get_advertising_event_type(packet) != 0x03) break;
                    data = gap_event_advertising_report_get_data(packet);
                    data_len = gap_event_advertising_report_get_data_length(packet);

                    // log_info_hexdump(data, data_len);
                    switch(data[1]){
                        case BLUETOOTH_DATA_TYPE_MESH_MESSAGE:
                            type_id = MESH_NETWORK_ID;
                            break;
                        case BLUETOOTH_DATA_TYPE_MESH_BEACON:
                            type_id = MESH_BEACON_ID;
                            break;
                        case BLUETOOTH_DATA_TYPE_PB_ADV:
                            type_id = PB_ADV_ID;
                            break;
                        default:
                            return;
                    }
                    if (client_callbacks[type_id]){
                        switch (type_id){
                            case PB_ADV_ID:
                                (*client_callbacks[type_id])(packet_type, channel, packet, size);
                                break;
                            case MESH_NETWORK_ID:
                                (*client_callbacks[type_id])(MESH_NETWORK_PACKET, 0, (uint8_t*) &data[2], data_len-2);
                                break;
                            case MESH_BEACON_ID:
                                (*client_callbacks[type_id])(MESH_BEACON_PACKET, 0, (uint8_t*) &data[2], data_len-2);
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
}

static void adv_bearer_idle(void){
    gap_advertisements_enable(0);
}

static int adv_bearer_have_requests(void){
    int i;
    for (i=0;i<NUM_TYPES;i++){
        if (request_can_send_now[i]){
            return 1;
        }
    }
    return 0;
}

static void adv_bearer_timeout_handler(btstack_timer_source_t * ts){

    log_info("Timeout Adv Bearer");

    adv_bearer_active = 0;

    if (gap_advertising_data){
        // switch to gap adv
        adv_gap_start_advertising();
        return;
    }

    if (adv_bearer_have_requests()){
        // notify next in line
        adv_bearer_emit_can_send_now();
    }

    // done?
    if (adv_bearer_have_requests()) return;
    adv_bearer_idle();
}

static void adv_gap_timeout_handler(btstack_timer_source_t * ts){

    log_info("Timeout GAP Adv");

    if (adv_bearer_have_requests()){
        // switch to adv bearer
        adv_gap_active = 0;
        adv_bearer_set_mesh_params();
        adv_bearer_emit_can_send_now();
        return;
    }

    // gap active, no other requests, check back later
    btstack_run_loop_set_timer(&adv_timer, adv_check_interval_ms);
    btstack_run_loop_add_timer(&adv_timer);
}

static void adv_bearer_start_advertising(const uint8_t * data, uint16_t data_len, uint8_t type){

    log_info("Start Adv Bearer type %x", type);

    // prepare message
    adv_buffer[0] = data_len+1;
    adv_buffer[1] = type;
    memcpy(&adv_buffer[2], data, data_len);

    // set advertiseent data
    gap_advertisements_set_data(data_len+2, adv_buffer);

    // enable
    gap_advertisements_enable(1);

    // adv update
    adv_check_interval_ms = (100 * 3) + ADV_TIMER_EXTRA_MS;

    // set timer
    adv_timer.process = &adv_bearer_timeout_handler;
    btstack_run_loop_set_timer(&adv_timer, adv_check_interval_ms);
    btstack_run_loop_add_timer(&adv_timer);

    adv_bearer_active = 1;
}

static void adv_gap_start_advertising(void){

    log_info("Start GAP Adv");

    // prepare advertisement
    gap_advertisements_set_params(gap_adv_int_min, gap_adv_int_max, gap_adv_type, gap_direct_address_typ, gap_direct_address, gap_channel_map, gap_filter_policy);
    gap_advertisements_set_data(gap_advertising_data_length, gap_advertising_data);

    // enable
    gap_advertisements_enable(1);

    // adv update
    adv_check_interval_ms = (gap_adv_int_max * 3) + ADV_TIMER_EXTRA_MS;

    // set timer
    adv_timer.process = &adv_gap_timeout_handler;
    btstack_run_loop_set_timer(&adv_timer, adv_check_interval_ms);
    btstack_run_loop_add_timer(&adv_timer);

    adv_gap_active = 1;
}

static void adv_bearer_request(message_type_id_t type_id){
    log_info("request to send message type %u", (int) type_id);
    request_can_send_now[type_id] = 1;
    adv_bearer_emit_can_send_now();
}

static void adv_bearer_set_mesh_params(void){
    // configure LE advertisments: 100ms, non-conn ind, null addr type, null addr, all channels, no filter
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(0xa0, 0xa0, 3, 0, null_addr, 0x07, 0);
    adv_check_interval_ms = (3 * 100) + ADV_TIMER_EXTRA_MS; 
}


void adv_bearer_init(void){
    // register for HCI Events
    hci_event_callback_registration.callback = &adv_bearer_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    adv_bearer_set_mesh_params();
}

//

void adv_bearer_register_for_mesh_message(btstack_packet_handler_t packet_handler){
    client_callbacks[MESH_NETWORK_ID] = packet_handler;
}
void adv_bearer_register_for_mesh_beacon(btstack_packet_handler_t packet_handler){
    client_callbacks[MESH_BEACON_ID] = packet_handler;
}
void adv_bearer_register_for_pb_adv(btstack_packet_handler_t packet_handler){
    client_callbacks[PB_ADV_ID] = packet_handler;
}

//
void adv_bearer_request_can_send_now_for_mesh_message(void){
    adv_bearer_request(MESH_NETWORK_ID);
}
void adv_bearer_request_can_send_now_for_mesh_beacon(void){
    log_info("Adv Bearer request for mesh beacon");
    adv_bearer_request(MESH_BEACON_ID);
}
void adv_bearer_request_can_send_now_for_pb_adv(void){
    adv_bearer_request(PB_ADV_ID);
}

void adv_bearer_send_mesh_message(const uint8_t * data, uint16_t data_len){
    adv_bearer_start_advertising(data, data_len, BLUETOOTH_DATA_TYPE_MESH_MESSAGE);
}
void adv_bearer_send_mesh_beacon(const uint8_t * data, uint16_t data_len){
    adv_bearer_start_advertising(data, data_len, BLUETOOTH_DATA_TYPE_MESH_BEACON);
}
void adv_bearer_send_pb_adv(const uint8_t * data, uint16_t data_len){
    adv_bearer_start_advertising(data, data_len, BLUETOOTH_DATA_TYPE_PB_ADV);
}

void adv_bearer_advertisements_enable(int enabled){
    // TODO
}

void adv_bearer_advertisements_set_data(uint8_t advertising_data_length, uint8_t * advertising_data){
    gap_advertising_data_length = advertising_data_length;
    gap_advertising_data        = advertising_data;

    if (adv_bearer_active) return;
    if (adv_gap_active)    return;

    adv_gap_start_advertising();
}

void adv_bearer_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy){

    gap_adv_int_min        = adv_int_min;
    gap_adv_int_max        = adv_int_max;
    gap_adv_type           = adv_type;
    gap_direct_address_typ = direct_address_typ; 
    memcpy(gap_direct_address, &direct_address, 6);
    gap_channel_map        = channel_map; 
    gap_filter_policy      = filter_policy; 
}
