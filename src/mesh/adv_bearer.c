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

#define ENABLE_LOG_DEBUG

#include <string.h>

#include "mesh/adv_bearer.h"
#include "ble/core.h"
#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_run_loop.h"
#include "btstack_event.h"
#include "gap.h"

// issue: gap adv control in hci might be slow to update advertisements fast enough. for now add 10 ms extra to ADVERTISING_INTERVAL_CONNECTABLE_MIN_MS
// todo: track adv enable/disable events before next step

// min advertising interval 20 ms for connectable advertisements
#define ADVERTISING_INTERVAL_CONNECTABLE_MIN 0x30
#define ADVERTISING_INTERVAL_CONNECTABLE_MIN_MS (ADVERTISING_INTERVAL_CONNECTABLE_MIN * 625 / 1000)

// min advertising interval 100 ms for non-connectable advertisements (pre 5.0 controllers)
#define ADVERTISING_INTERVAL_NONCONNECTABLE_MIN 0xa0
#define ADVERTISING_INTERVAL_NONCONNECTABLE_MIN_MS (ADVERTISING_INTERVAL_NONCONNECTABLE_MIN * 625 / 1000)

// num adv bearer message types
#define NUM_TYPES 3

typedef enum {
    MESH_NETWORK_ID,
    MESH_BEACON_ID,
    PB_ADV_ID,
    INVALID_ID,
} message_type_id_t;

typedef enum {
    STATE_IDLE,
    STATE_BEARER,
    STATE_GAP,
} state_t;


// prototypes
static void adv_bearer_run(void);

// globals

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_timer_source_t adv_timer;
static bd_addr_t null_addr;

static btstack_packet_handler_t client_callbacks[NUM_TYPES];
static int request_can_send_now[NUM_TYPES];
static int last_sender;

// scheduler
static state_t    adv_bearer_state;
static uint32_t   gap_adv_next_ms;

// adv bearer packets
static uint8_t   adv_bearer_buffer[31];
static uint8_t   adv_bearer_buffer_length;
static uint8_t   adv_bearer_count;
static uint16_t  adv_bearer_interval;

// gap advertising
static int       gap_advertising_enabled;
static uint16_t  gap_adv_int_min    = 0x30;
static uint16_t  gap_adv_int_ms;
static uint16_t  gap_adv_int_max    = 0x30;
static uint8_t   gap_adv_type       = 0;
static uint8_t   gap_direct_address_typ;
static bd_addr_t gap_direct_address;
static uint8_t   gap_channel_map    = 0x07;
static uint8_t   gap_filter_policy  = 0;

static btstack_linked_list_t gap_connectable_advertisements;

// dispatch advertising events
static void adv_bearer_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    const uint8_t * data;
    int data_len;
    message_type_id_t type_id;

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch(packet[0]){
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
                    adv_bearer_run();
                    break;
                case GAP_EVENT_ADVERTISING_REPORT:
                    // only non-connectable ind
                    if (gap_event_advertising_report_get_advertising_event_type(packet) != 0x03) break;
                    data = gap_event_advertising_report_get_data(packet);
                    data_len = gap_event_advertising_report_get_data_length(packet);

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

// round-robin
static void adv_bearer_emit_can_send_now(void){

    if (adv_bearer_count > 0) return;

    int countdown = NUM_TYPES;
    while (countdown--) {
        last_sender++;
        if (last_sender == NUM_TYPES) {
            last_sender = 0;
        }
        if (request_can_send_now[last_sender]){
            request_can_send_now[last_sender] = 0;
            // emit can send now
            log_debug("can send now");
            uint8_t event[3];
            event[0] = HCI_EVENT_MESH_META;
            event[1] = 1;
            event[2] = MESH_SUBEVENT_CAN_SEND_NOW;
            (*client_callbacks[last_sender])(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
            return;
        }
    }
}

static void adv_bearer_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    uint32_t now = btstack_run_loop_get_time_ms();
    switch (adv_bearer_state){
        case STATE_GAP:
            log_debug("Timeout (state gap)");
            gap_advertisements_enable(0);
            if (gap_advertising_enabled){
                gap_adv_next_ms = now + gap_adv_int_ms - ADVERTISING_INTERVAL_CONNECTABLE_MIN_MS;
                log_debug("Next adv: %u", gap_adv_next_ms);
            }
            adv_bearer_state = STATE_IDLE;
            break;
        case STATE_BEARER:
            log_debug("Timeout (state bearer)");
            gap_advertisements_enable(0);
            adv_bearer_count--;
            if (adv_bearer_count == 0){
                adv_bearer_emit_can_send_now();
            }
            adv_bearer_state = STATE_IDLE;
            break;
        default:
            break;
    }
    adv_bearer_run();
}

static void adv_bearer_set_timeout(uint32_t time_ms){
    btstack_run_loop_set_timer_handler(&adv_timer, &adv_bearer_timeout_handler);
    btstack_run_loop_set_timer(&adv_timer, time_ms);    // compile time constants
    btstack_run_loop_add_timer(&adv_timer);
}

// scheduler
static void adv_bearer_run(void){

    if (hci_get_state() != HCI_STATE_WORKING) return;

    uint32_t now = btstack_run_loop_get_time_ms();
    switch (adv_bearer_state){
        case STATE_IDLE:
            if (gap_advertising_enabled){
                if ((int32_t)(now - gap_adv_next_ms) >= 0){
                    adv_bearer_connectable_advertisement_data_item_t * item = (adv_bearer_connectable_advertisement_data_item_t *) btstack_linked_list_pop(&gap_connectable_advertisements);
                    if (item != NULL){
                        // queue again
                        btstack_linked_list_add_tail(&gap_connectable_advertisements, (void*) item);                        
                        // time to advertise again
                        log_debug("Start GAP ADV, %p", item);
                        gap_advertisements_set_params(ADVERTISING_INTERVAL_CONNECTABLE_MIN, ADVERTISING_INTERVAL_CONNECTABLE_MIN, gap_adv_type, gap_direct_address_typ, gap_direct_address, gap_channel_map, gap_filter_policy);
                        gap_advertisements_set_data(item->adv_length, item->adv_data);
                        gap_advertisements_enable(1);
                        adv_bearer_set_timeout(ADVERTISING_INTERVAL_CONNECTABLE_MIN_MS);
                        adv_bearer_state = STATE_GAP;
                    }
                    break;
                }
            }
            if (adv_bearer_count > 0){
                // schedule adv bearer message if enough time
                // if ((gap_advertising_enabled) == 0 || ((int32_t)(gap_adv_next_ms - now) >= ADVERTISING_INTERVAL_NONCONNECTABLE_MIN_MS)){
                log_debug("Send ADV Bearer message");
                // configure LE advertisments: non-conn ind
                gap_advertisements_set_params(ADVERTISING_INTERVAL_NONCONNECTABLE_MIN, ADVERTISING_INTERVAL_NONCONNECTABLE_MIN, 3, 0, null_addr, 0x07, 0);
                gap_advertisements_set_data(adv_bearer_buffer_length, adv_bearer_buffer);
                gap_advertisements_enable(1);
                adv_bearer_set_timeout(ADVERTISING_INTERVAL_NONCONNECTABLE_MIN_MS);
                adv_bearer_state = STATE_BEARER;
                break;
                // }
            }
            if (gap_advertising_enabled){
                // use timer to wait for next adv
                adv_bearer_set_timeout(gap_adv_next_ms - now);
            }
            break;
        default:
            break;
    }
}

//
static void adv_bearer_prepare_message(const uint8_t * data, uint16_t data_len, uint8_t type, uint8_t count, uint16_t interval){
    log_debug("adv bearer message, type 0x%x\n", type);
    // prepare message
    adv_bearer_buffer[0] = data_len+1;
    adv_bearer_buffer[1] = type;
    memcpy(&adv_bearer_buffer[2], data, data_len);
    adv_bearer_buffer_length = data_len + 2;

    // setup trasmission schedule
    adv_bearer_count    = count;
    adv_bearer_interval = interval;
}

//////

static void adv_bearer_request(message_type_id_t type_id){
    request_can_send_now[type_id] = 1;
    adv_bearer_emit_can_send_now();
}

void adv_bearer_init(void){
    // register for HCI Events
    hci_event_callback_registration.callback = &adv_bearer_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    // idle
    adv_bearer_state = STATE_IDLE; 
    memset(null_addr, 0, 6);
}

// adv bearer packet handler regisration

void adv_bearer_register_for_network_pdu(btstack_packet_handler_t packet_handler){
    client_callbacks[MESH_NETWORK_ID] = packet_handler;
}
void adv_bearer_register_for_beacon(btstack_packet_handler_t packet_handler){
    client_callbacks[MESH_BEACON_ID] = packet_handler;
}
void adv_bearer_register_for_provisioning_pdu(btstack_packet_handler_t packet_handler){
    client_callbacks[PB_ADV_ID] = packet_handler;
}

// adv bearer request to send

void adv_bearer_request_can_send_now_for_network_pdu(void){
    adv_bearer_request(MESH_NETWORK_ID);
}
void adv_bearer_request_can_send_now_for_beacon(void){
    adv_bearer_request(MESH_BEACON_ID);
}
void adv_bearer_request_can_send_now_for_provisioning_pdu(void){
    adv_bearer_request(PB_ADV_ID);
}

// adv bearer send message

void adv_bearer_send_network_pdu(const uint8_t * data, uint16_t data_len, uint8_t count, uint16_t interval){
    adv_bearer_prepare_message(data, data_len, BLUETOOTH_DATA_TYPE_MESH_MESSAGE, count, interval);
    adv_bearer_run();
}
void adv_bearer_send_beacon(const uint8_t * data, uint16_t data_len){
    adv_bearer_prepare_message(data, data_len, BLUETOOTH_DATA_TYPE_MESH_BEACON, 3, 100);
    adv_bearer_run();
}
void adv_bearer_send_provisioning_pdu(const uint8_t * data, uint16_t data_len){
    adv_bearer_prepare_message(data, data_len, BLUETOOTH_DATA_TYPE_PB_ADV, 3, 100);
    adv_bearer_run();
}

// gap advertising

void adv_bearer_advertisements_enable(int enabled){
    gap_advertising_enabled = enabled;
    if (!gap_advertising_enabled) return;

    // start right away
    gap_adv_next_ms = btstack_run_loop_get_time_ms();
    adv_bearer_run();
}

void adv_bearer_advertisements_add_item(adv_bearer_connectable_advertisement_data_item_t * item){
    btstack_linked_list_add(&gap_connectable_advertisements, (void*) item);
}

void adv_bearer_advertisements_remove_item(adv_bearer_connectable_advertisement_data_item_t * item){
    btstack_linked_list_remove(&gap_connectable_advertisements, (void*) item);
}

void adv_bearer_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy){

    // assert adv_int_min/max >= 2 * ADVERTISING_INTERVAL_MIN

    gap_adv_int_min        = btstack_max(adv_int_min, 2 * ADVERTISING_INTERVAL_CONNECTABLE_MIN);
    gap_adv_int_max        = btstack_max(adv_int_max, 2 * ADVERTISING_INTERVAL_CONNECTABLE_MIN);
    gap_adv_int_ms         = gap_adv_int_min * 625 / 1000;
    gap_adv_type           = adv_type;
    gap_direct_address_typ = direct_address_typ; 
    memcpy(gap_direct_address, &direct_address, 6);
    gap_channel_map        = channel_map; 
    gap_filter_policy      = filter_policy; 

    log_info("GAP Adv interval %u ms", gap_adv_int_ms);
}
