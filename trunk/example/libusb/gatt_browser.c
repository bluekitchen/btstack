/*
 * Copyright (C) 2011-2013 by BlueKitchen GmbH
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include <btstack/sdp_util.h>

#include "btstack-config.h"

#include "ad_parser.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "att.h"
#include "gatt_client.h"

#ifdef HAVE_UART_CC2564
#include "bt_control_cc256x.h"
#endif

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;


typedef enum {
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,
    
    TC_W4_SERVICE_RESULT,
    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_DISCONNECT
} tc_state_t;


static gatt_client_t test_gatt_client_context;
static uint16_t test_gatt_client_handle;

static le_service_t services[40];
static int service_count = 0;
static int service_index = 0;
static uint8_t   test_device_addr_type = 0;
static tc_state_t tc_state = TC_IDLE;

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    if (uuid16){
        printf("%04x",uuid16);
    } else {
        printUUID128(uuid128);
    }
}

static void dump_ad_event(ad_event_t * e){
    printf("    * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type,
           e->address_type, bd_addr_to_str(e->address), e->rssi, e->length);
    hexdump(e->data, e->length);
    
}

static void dump_characteristic(le_characteristic_t * characteristic){
    printf("    * characteristic: [0x%04x-0x%04x-0x%04x], properties 0x%02x, uuid ",
                            characteristic->start_handle, characteristic->value_handle, characteristic->end_handle, characteristic->properties);
    printUUID(characteristic->uuid128, characteristic->uuid16);
    printf("\n");
}

static void dump_service(le_service_t * service){
    printf("    * service: [0x%04x-0x%04x], uuid ", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
    printf("\n");
}

void handle_gatt_client_event(le_event_t * event){
    le_service_t service;
    le_characteristic_t characteristic;
    switch(tc_state){
        case TC_W4_SERVICE_RESULT:
            switch(event->type){
                case GATT_SERVICE_QUERY_RESULT:
                    service = ((le_service_event_t *) event)->service;
                    dump_service(&service);
                    services[service_count++] = service;
                    break;
                case GATT_QUERY_COMPLETE:
                    tc_state = TC_W4_CHARACTERISTIC_RESULT;
                    service_index = 0;
                    printf("\ntest client - CHARACTERISTIC for SERVICE ");
                    printUUID128(service.uuid128); printf("\n");
                    
                    gatt_client_discover_characteristics_for_service(&test_gatt_client_context, &services[service_index]);
                    break;
                default:
                    break;
            }
            break;
            
        case TC_W4_CHARACTERISTIC_RESULT:
            switch(event->type){
                case GATT_CHARACTERISTIC_QUERY_RESULT:
                    characteristic = ((le_characteristic_event_t *) event)->characteristic;
                    dump_characteristic(&characteristic);
                    break;
                case GATT_QUERY_COMPLETE:
                    if (service_index < service_count) {
                        tc_state = TC_W4_CHARACTERISTIC_RESULT;
                        service = services[service_index++];
                        printf("\ntest client - CHARACTERISTIC for SERVICE ");
                        printUUID128(service.uuid128);
                        printf(", [0x%04x-0x%04x]\n", service.start_group_handle, service.end_group_handle);
                        
                        gatt_client_discover_characteristics_for_service(&test_gatt_client_context, &service);
                        break;
                    }
                    tc_state = TC_W4_DISCONNECT;
                    service_index = 0;
                    gap_disconnect(test_gatt_client_context.handle);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    
}

static void handle_hci_event(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch (packet[0]) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("\ntest client - DISCONNECTED\n");
            exit(0);
            break;
        case GAP_LE_ADVERTISING_REPORT:
            if (tc_state != TC_W4_SCAN_RESULT) return;
            printf("test client - SCAN ACTIVE\n");
            ad_event_t ad_event;
            int pos = 2;
            ad_event.event_type = packet[pos++];
            ad_event.address_type = packet[pos++];
            memcpy(ad_event.address, &packet[pos], 6);
            
            pos += 6;
            ad_event.rssi = packet[pos++];
            ad_event.length = packet[pos++];
            ad_event.data = &packet[pos];
            pos += ad_event.length;
            dump_ad_event(&ad_event);
            
            test_device_addr_type = ad_event.address_type;
            bd_addr_t found_device_addr;
            
            memcpy(found_device_addr, ad_event.address, 6);
            swapX(ad_event.address, found_device_addr, 6);
            
            tc_state = TC_W4_CONNECT;
            le_central_stop_scan();
            le_central_connect(&found_device_addr, test_device_addr_type);
    
            break;
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (packet[2] == HCI_STATE_WORKING) {
                printf("BTstack activated, get started!\n");
                tc_state = TC_W4_SCAN_RESULT;
                le_central_start_scan(); 
            }
            break;
        case HCI_EVENT_LE_META:
            switch (packet[2]) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                    if (tc_state != TC_W4_CONNECT) return;
                    tc_state = TC_W4_SERVICE_RESULT;
                    printf("\ntest client - CONNECTED, query primary services\n");
                    test_gatt_client_handle = READ_BT_16(packet, 4);
                    gatt_client_start(&test_gatt_client_context, test_gatt_client_handle);
                    
                    // let's start
                    gatt_client_discover_primary_services(&test_gatt_client_context);
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

static hci_uart_config_t  config;
void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/ble_client.pklg", HCI_DUMP_PACKETLOGGER);

  // init HCI
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
    bt_control_t       * control   = NULL;
#ifndef  HAVE_UART_CC2564
    hci_transport_t    * transport = hci_transport_usb_instance();
#else
    hci_transport_t    * transport = hci_transport_h4_instance();
    control   = bt_control_cc256x_instance();
    // config.device_name   = "/dev/tty.usbserial-A600eIDu";   // 5438
    config.device_name   = "/dev/tty.usbserial-A800cGd0";   // 5529
    config.baudrate_init = 115200;
    config.baudrate_main = 0;
    config.flowcontrol = 1;
#endif        
    hci_init(transport, &config, control, remote_db);
    l2cap_init();
    l2cap_register_packet_handler(&handle_hci_event);

    gatt_client_init();
    gatt_client_register_handler(&handle_gatt_client_event);
}

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




