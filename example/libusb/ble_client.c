/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

//*****************************************************************************
//
// BLE Client
//
//*****************************************************************************


// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/run_loop.h>

#include "config.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "att.h"

// API

typedef struct gatt_client_event {
    uint8_t   type;
} gatt_client_event_t;

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;


void (*gatt_client_callback)(gatt_client_event_t * event);

void gatt_client_init();
void gatt_client_start_scan();
// creates one event per found peripheral device
// EVENT: type (8), addr_type (8), addr(48), rssi(8), ad_len(8), ad_data(ad_len*8)
void gatt_client_stop_scan();

/*

typedef struct service_uuid{
    uint8_t lenght;
    uint8_t * uuid;
} service_uuid_t;

typedef struct peripheral{

} peripheral_t;

// Advertising Data Parser

typedef struct ad_context {
     uint8_t * data;
     uint8_t   offset;
     uint8_t   length;
} ad_context_t;

// iterator
void ad_init(ad_context_t *context, uint8_t ad_len, uint8_t * ad_data);
int  ad_has_more(ad_context_t * context);
void ad_next(ad_context_t * context);

// access functions
uint8_t   ad_get_data_type(ad_context_t * context);
uint8_t   ad_get_data_len(ad_context_t * context);
uint8_t * ad_get_data(ad_context_t * context);

// convenience function on complete advertisements
int ad_data_contains_uuid16(uint8_t ad_len, uint8_t * ad_data, uint16_t uuid);
int ad_data_contains_uuid128(uint8_t ad_len, uint8_t * ad_data, uint8_t * uuid128);

// example use of Advertisment Data Parser
void test_ad_parser(){
    ad_context_t context;
    for (ad_init(&context, len, data) ; ad_has_more(&context) ; ad_next(&context)){
        uint8_t data_type = ad_get_data_type(&context);
        uint8_t data_len  = ad_get_data_len(&context);
        uint8_t * data    = ad_get_data(&context);
    }
}

// GATT Client API

void gatt_client_register_handler( btstack_packet_handler_t handler);

uint16_t gatt_client_connect(bt_addr_t *dev);
void gatt_client_cancel_connect(peripheral_id peripheral);

void get_services_for_peripheral(peripheral_id peripheral);
// EVENT: type (8), peripheral_id (16), service_id 

void get_characteristics_for_service(peripheral_id, service_id);
// EVENT: type (8), peripheral_id (16), service_id (16), ...
*/
// END API

// gatt client state
static uint8_t requested_scan_state = 0;
static uint8_t scan_state = 0;


void gatt_client_init(){
    requested_scan_state = 0;
    scan_state = 0;
}

static void gatt_client_run(){
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
    if (scan_state == requested_scan_state) return;
    
    if (requested_scan_state){
        printf("Starting scan...\n");
    } else {
        printf("Stopping scan...\n");                
    }

    hci_send_cmd(&hci_le_set_scan_enable, requested_scan_state, 0);
}

void gatt_client_start_scan(){
    requested_scan_state = 1;
    gatt_client_run();
}

void gatt_client_stop_scan(){
    requested_scan_state = 0;
    gatt_client_run();
}


static void hexdump2(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void dump_ad_event(ad_event_t e){
    printf("evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e.event_type, 
            e.address_type, bd_addr_to_str(e.address), e.rssi, e.length); 
    hexdump2( e.data, e.length);                            
}

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started
					if (packet[2] == HCI_STATE_WORKING) {
                        printf("Working!\n");
                        gatt_client_start_scan();
                    }
					break;
                
                case HCI_EVENT_COMMAND_COMPLETE:
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_enable)){
                        scan_state = requested_scan_state;
                    }
                    break;

                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_ADVERTISING_REPORT: {
                            int num_reports = packet[3];
                            int i;
                            int total_data_length = 0;
                            int data_offset = 0;

                            for (i=0; i<num_reports;i++){
                                total_data_length += packet[4+num_reports*8+i];  
                            }

                            for (i=0; i<num_reports;i++){
                                ad_event_t advertisement_event;
                                advertisement_event.event_type = packet[4+i];
                                advertisement_event.address_type = packet[4+num_reports+i];
                                bt_flip_addr(advertisement_event.address, &packet[4+num_reports*2+i*6]);
                                advertisement_event.length = packet[4+num_reports*8+i];
                                advertisement_event.data = &packet[4+num_reports*9+data_offset];
                                data_offset += advertisement_event.length;
                                advertisement_event.rssi = packet[4+num_reports*9+total_data_length + i];
                                
                                // TODO add register callback
                                // (*gatt_client_callback)((gatt_client_event_t*)&advertisement_event); 

                                dump_ad_event(advertisement_event);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    break;

                default:
                    break;
			}
	}

    gatt_client_run();
}


void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
        
    hci_init(transport, config, control, remote_db);

    l2cap_init();
    //l2cap_register_fixed_channel(packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    l2cap_register_packet_handler(packet_handler);
}

// main == setup
int main(void)
{
    setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}
