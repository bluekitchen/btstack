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


// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>

#include "config.h"

#include "ble_client.h"
#include "ad_parser.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "att.h"

// gatt client state
typedef enum {
    W4_ON,
    IDLE,
    //
    START_SCAN,
    W4_SCANNING,
    //
    SCANNING,
    //
    STOP_SCAN,
    W4_SCAN_STOPPED
} state_t;


static state_t state = W4_ON;
static linked_list_t le_connections = NULL;

void le_central_init(){
    state = W4_ON;
    le_connections = NULL;
}

void (*le_central_callback)(le_central_event_t * event);

static void gatt_client_run();


// START Helper Functions - to be sorted
static int l2cap_can_send_conectionless_packet_now(){
    return hci_can_send_packet_now(HCI_ACL_DATA_PACKET);
}

static uint16_t l2cap_max_mtu_for_handle(uint16_t handle){
    return l2cap_max_mtu();
}
// END Helper Functions

static void dummy_notify(le_central_event_t* event){}

void le_central_register_handler(void (*le_callback)(le_central_event_t* event)){
    le_central_callback = dummy_notify;
    if (le_callback != NULL){
        le_central_callback = le_callback;
    } 
}



static void send_gatt_connection_complete_event(le_peripheral_t * peripheral, uint8_t status){
    le_peripheral_event_t event;
    event.type = GATT_CONNECTION_COMPLETE;
    event.device = peripheral;
    event.status = status;
    (*le_central_callback)((le_central_event_t*)&event); 
}

static le_peripheral_t * get_peripheral_for_handle(uint16_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) le_connections; it ; it = it->next){
        le_peripheral_t * peripheral = (le_peripheral_t *) it;
        if (peripheral->handle == handle){
            return peripheral;
        }
    }
    return NULL;
}

static le_peripheral_t * get_peripheral_with_state(peripheral_state_t p_state){
    linked_item_t *it;
    for (it = (linked_item_t *) le_connections; it ; it = it->next){
        le_peripheral_t * peripheral = (le_peripheral_t *) it;
        if (peripheral->state == p_state){
            return peripheral;
        }
    }
    return NULL;
}

static inline le_peripheral_t * get_peripheral_w4_disconnected(){
    return get_peripheral_with_state(P_W4_DISCONNECTED);
}

static inline le_peripheral_t * get_peripheral_w4_connect_cancelled(){
    return get_peripheral_with_state(P_W4_CONNECT_CANCELLED);
}

static inline le_peripheral_t * get_peripheral_w2_connect(){
    return get_peripheral_with_state(P_W2_CONNECT);
}

static inline le_peripheral_t * get_peripheral_w4_connected(){
    return get_peripheral_with_state(P_W4_CONNECTED);
}

static inline le_peripheral_t * get_peripheral_w2_exchange_MTU(){
    return get_peripheral_with_state(P_W2_EXCHANGE_MTU);
}

static le_peripheral_t * get_peripheral_with_address(uint8_t addr_type, bd_addr_t addr){
    linked_item_t *it;
    for (it = (linked_item_t *) le_connections; it ; it = it->next){
        le_peripheral_t * peripheral = (le_peripheral_t *) it;
        if (BD_ADDR_CMP(addr, peripheral->address) == 0 && peripheral->address_type == addr_type){
            return peripheral;
        }
    }
    return 0;
}

static void handle_advertising_packet(uint8_t *packet){
    int num_reports = packet[3];
    int i;
    int total_data_length = 0;
    int data_offset = 0;

    for (i=0; i<num_reports;i++){
        total_data_length += packet[4+num_reports*8+i];  
    }

    for (i=0; i<num_reports;i++){
        ad_event_t advertisement_event;
        advertisement_event.type = GATT_ADVERTISEMENT;
        advertisement_event.event_type = packet[4+i];
        advertisement_event.address_type = packet[4+num_reports+i];
        bt_flip_addr(advertisement_event.address, &packet[4+num_reports*2+i*6]);
        advertisement_event.length = packet[4+num_reports*8+i];
        advertisement_event.data = &packet[4+num_reports*9+data_offset];
        data_offset += advertisement_event.length;
        advertisement_event.rssi = packet[4+num_reports*9+total_data_length + i];
        
        (*le_central_callback)((le_central_event_t*)&advertisement_event); 
    }
}

static void handle_peripheral_list(){
    // only one connect is allowed, wait for result
    if (get_peripheral_w4_connected()) return;
    // only one cancel connect is allowed, wait for result
    if (get_peripheral_w4_connect_cancelled()) return;

    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
    if (!l2cap_can_send_conectionless_packet_now()) return;
    
    linked_item_t *it;
    for (it = (linked_item_t *) le_connections; it ; it = it->next){
        le_peripheral_t * peripheral = (le_peripheral_t *) it;
        
        switch (peripheral->state){
            case P_W2_CONNECT:
                peripheral->state = P_W4_CONNECTED;
                hci_send_cmd(&hci_le_create_connection, 
                     1000,      // scan interval: 625 ms
                     1000,      // scan interval: 625 ms
                     0,         // don't use whitelist
                     0,         // peer address type: public
                     peripheral->address,      // remote bd addr
                     peripheral->address_type, // random or public
                     80,        // conn interval min
                     80,        // conn interval max (3200 * 0.625)
                     0,         // conn latency
                     2000,      // supervision timeout
                     0,         // min ce length
                     1000       // max ce length
                );
                return;

            case P_W2_CANCEL_CONNECT: 
                peripheral->state = P_W4_CONNECT_CANCELLED;
                hci_send_cmd(&hci_le_create_connection_cancel);
                return;
            case P_W2_EXCHANGE_MTU:
            {
                peripheral->state = P_W4_EXCHANGE_MTU;
                uint16_t mtu = l2cap_max_mtu_for_handle(peripheral->handle);
                uint8_t request[3];
                request[0] = ATT_EXCHANGE_MTU_REQUEST;
                bt_store_16(request, 1, mtu);
                l2cap_send_connectionless(peripheral->handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, request, sizeof(request));
                return;
            }

            case P_W2_DISCONNECT:
                peripheral->state = P_W4_DISCONNECTED;
                hci_send_cmd(&hci_disconnect, peripheral->handle,0x13);
                return;

            default:
                break;
        }
        
    }
         
}


void le_central_start_scan(){
    if (state != IDLE) return; 
    state = START_SCAN;
    gatt_client_run();
}

void le_central_stop_scan(){
    if (state != SCANNING) return;
    state = STOP_SCAN;
    gatt_client_run();
}

static void le_peripheral_init(le_peripheral_t *context, uint8_t addr_type, bd_addr_t addr){
    memset(context, 0, sizeof(le_peripheral_t));
    context->state = P_W2_CONNECT;
    context->address_type = addr_type;
    memcpy (context->address, addr, 6);
}

le_command_status_t le_central_connect(le_peripheral_t *context, uint8_t addr_type, bd_addr_t addr){
    //TODO: align with hci connection list capacity
    le_peripheral_t * peripheral = get_peripheral_with_address(addr_type, addr);
    if (!peripheral) {
        le_peripheral_init(context, addr_type, addr);
        linked_list_add(&le_connections, (linked_item_t *) context);
    } else if (peripheral == context) {
        if (context->state != P_W2_CONNECT) return BLE_PERIPHERAL_IN_WRONG_STATE;
    } else {
        return BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS;
    }

    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_disconnect(le_peripheral_t *context){
    // printf("*** le_central_disconnect::CALLED DISCONNECT \n");

    le_peripheral_t * peripheral = get_peripheral_with_address(context->address_type, context->address);
    if (!peripheral || (peripheral && peripheral != context)){
        return BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS;
    }

    switch(context->state){
        case P_W2_CONNECT:
            linked_list_remove(&le_connections, (linked_item_t *) context);
            send_gatt_connection_complete_event(context, 0);
            break; 
        case P_W4_CONNECTED:
        case P_W2_CANCEL_CONNECT:
            // trigger cancel connect
            context->state = P_W2_CANCEL_CONNECT;
            break;
        case P_W2_EXCHANGE_MTU:
        case P_W4_EXCHANGE_MTU:
        case P_CONNECTED:
        case P_W2_DISCONNECT:
            // trigger disconnect
            context->state = P_W2_DISCONNECT;
            break;
        case P_W4_DISCONNECTED:
        case P_W4_CONNECT_CANCELLED: 
            return BLE_PERIPHERAL_IN_WRONG_STATE;
    }  
    gatt_client_run();    
    return BLE_PERIPHERAL_OK;      
}


static void gatt_client_run(){
    if (state == W4_ON) return;
    
    handle_peripheral_list();

    // check if command is send 
    if (!hci_can_send_packet_now(HCI_COMMAND_DATA_PACKET)) return;
    if (!l2cap_can_send_conectionless_packet_now()) return;
    
    switch(state){
        case START_SCAN:
            state = W4_SCANNING;
            hci_send_cmd(&hci_le_set_scan_enable, 1, 0);
            return;

        case STOP_SCAN:
            state = W4_SCAN_STOPPED;
            hci_send_cmd(&hci_le_set_scan_enable, 0, 0);
            return;

        default:
            break;
    }
}


static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    if (packet_type != HCI_EVENT_PACKET) return;
	switch (packet[0]) {
		case BTSTACK_EVENT_STATE:
			// BTstack activated, get started
			if (packet[2] == HCI_STATE_WORKING) {
                printf("BTstack activated, get started!\n");
                state = IDLE;
            }
			break;

        case HCI_EVENT_COMMAND_COMPLETE:
            if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_enable)){
                switch(state){
                    case W4_SCANNING:
                        state = SCANNING;
                        break;
                    case W4_SCAN_STOPPED:
                        state = IDLE;
                        break;
                    default:
                        break;
                }
                return;
            }

            if (COMMAND_COMPLETE_EVENT(packet, hci_le_create_connection_cancel)){
                // printf("packet_handler:: hci_le_create_connection_cancel: cancel connect\n");
                if (packet[3] != 0x0B) break;

                // cancel connection failed, as connection already established
                le_peripheral_t * peripheral = get_peripheral_w4_connect_cancelled();
                peripheral->state = P_W2_DISCONNECT;
                break;
            }
            break;  

        case HCI_EVENT_DISCONNECTION_COMPLETE:
        {
            uint16_t handle = READ_BT_16(packet,3);
            le_peripheral_t * peripheral = get_peripheral_for_handle(handle);
            if (!peripheral) break;

            peripheral->state = P_W2_CONNECT;
            linked_list_remove(&le_connections, (linked_item_t *) peripheral);
            send_gatt_connection_complete_event(peripheral, packet[5]);
            // printf("Peripheral disconnected, and removed from list\n");
            break;
        }

        case HCI_EVENT_LE_META:
            switch (packet[2]) {
                case HCI_SUBEVENT_LE_ADVERTISING_REPORT: 
                    if (state != SCANNING) break;
                    handle_advertising_packet(packet);
                    break;
                
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                    // deal with conn cancel, conn fail, conn success
                    le_peripheral_t * peripheral;

                    // conn success/error?
                    peripheral = get_peripheral_w4_connected();
                    if (peripheral){
                        if (packet[3]){
                            // error
                            linked_list_remove(&le_connections, (linked_item_t *) peripheral);
                        } else {
                            // success
                            peripheral->state = P_W2_EXCHANGE_MTU;
                            peripheral->handle = READ_BT_16(packet, 4);
                        }
                        send_gatt_connection_complete_event(peripheral, packet[3]);
                        break;
                    } 

                    // cancel success?
                    peripheral = get_peripheral_w4_connect_cancelled();
                    if (!peripheral) break;
                            
                    linked_list_remove(&le_connections, (linked_item_t *) peripheral);
                    send_gatt_connection_complete_event(peripheral, packet[3]);
                    break;
                }
                default:
                    break;
            }
            break;
        
        default:
            break;
    }
    gatt_client_run();
}


static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;

    switch (packet[0]){
        case ATT_EXCHANGE_MTU_RESPONSE:
        {
            le_peripheral_t * peripheral = get_peripheral_for_handle(handle);
            if (!peripheral) return;

            uint16_t remote_rx_mtu = READ_BT_16(packet, 1);
            uint16_t local_rx_mtu = l2cap_max_mtu_for_handle(handle);
            peripheral->mtu = remote_rx_mtu < local_rx_mtu ? remote_rx_mtu : local_rx_mtu;

            send_gatt_connection_complete_event(peripheral, 0);
            
            peripheral->state = P_CONNECTED; 
            break;
        }
        default:
            break;
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
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    l2cap_register_packet_handler(packet_handler);
}


// main == setup
int main(void)
{
    setup();
    
   /* test
    le_central_init();
    le_central_register_handler(handle_le_central_event);
   */
   
    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}

/* test 

static void dump_peripheral_state(peripheral_state_t p_state){
    switch(p_state) {
        case P_W2_CONNECT: printf("P_W2_CONNECT"); break;
        case P_W4_CONNECTED: printf("P_W4_CONNECTED"); break;
        case P_W2_EXCHANGE_MTU: printf("P_W2_EXCHANGE_MTU"); break;
        case P_W4_EXCHANGE_MTU: printf("P_W4_EXCHANGE_MTU"); break;
        case P_CONNECTED: printf("P_CONNECTED"); break;
        case P_W2_CANCEL_CONNECT: printf("P_W2_CANCEL_CONNECT"); break;
        case P_W4_CONNECT_CANCELLED: printf("P_W4_CONNECT_CANCELLED"); break;
        case P_W2_DISCONNECT: printf("P_W2_DISCONNECT"); break;
        case P_W4_DISCONNECTED: printf("P_W4_DISCONNECTED"); break;
    };
    printf("\n");
}

static void dump_state(){
    switch(state){
        case W4_ON: printf("W4_ON"); break;
        case IDLE: printf("IDLE"); break;
        case START_SCAN: printf("START_SCAN"); break;
        case W4_SCANNING: printf("W4_SCANNING"); break;
        case SCANNING: printf("SCANNING"); break;
        case STOP_SCAN: printf("STOP_SCAN"); break;
        case W4_SCAN_STOPPED: printf("W4_SCAN_STOPPED"); break;
    };
    printf("\n");
}

static void hexdump2(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void dump_ad_event(ad_event_t* e){
    printf("evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type, 
            e->address_type, bd_addr_to_str(e->address), e->rssi, e->length); 
    hexdump2( e->data, e->length);                            
}

le_peripheral_t test_device;

static bd_addr_t test_device_addr = {0x1c, 0xba, 0x8c, 0x20, 0xc7, 0xf6};


void test_client(){
    static int i = 0;
    
    switch(i){
        case 0:
            le_central_start_scan();
            printf("--- test_client::calling start scan \n");
            break;
        case 20:
            printf("--- test_client::calling connect peripheral: status %d\n", le_central_connect(&test_device, 0, test_device_addr));
            break;
        case 40:
            i=-1;
            // le_central_stop_scan();
            printf("--- test_client::calling dissconnect peripheral: status %d\n", le_central_disconnect(&test_device));
            break;
        default:
            break;
    }
    i++;
}

static void handle_le_central_event(le_central_event_t * event){
    ad_event_t * advertisement_event;
    le_peripheral_event_t * peripheral_event;
    switch (event->type){
        case GATT_ADVERTISEMENT:
            advertisement_event = (ad_event_t*) event;
            dump_ad_event(advertisement_event);
            break;
        case GATT_CONNECTION_COMPLETE:
            peripheral_event = (le_peripheral_event_t *) event;
            if (peripheral_event->status == 0){
                printf("handle_le_central_event::device is connected\n");
            } else {
                printf("handle_le_central_event::disconnected with status %02x \n", peripheral_event->status);
            }
        default:
            break;
    }
}*/


