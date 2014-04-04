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

#include "ble_client.h"
#include "ad_parser.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "att.h"

#ifdef HAVE_UART_CC2564
#include "bt_control_cc256x.h"
#endif

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
static uint16_t att_client_start_handle = 0x0001;
    
void (*le_central_callback)(le_central_event_t * event);
void (*le_central_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = NULL;

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size);
static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);


void le_central_init(){
    state = W4_ON;
    le_connections = NULL;
    att_client_start_handle = 0x0000;
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    l2cap_register_packet_handler(packet_handler);
}

static void dummy_notify(le_central_event_t* event){}

void le_central_register_handler(void (*le_callback)(le_central_event_t* event)){
    le_central_callback = dummy_notify;
    if (le_callback != NULL){
        le_central_callback = le_callback;
    } 
}

void le_central_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    le_central_packet_handler = handler;
}

static void gatt_client_run();

// START Helper Functions - to be sorted
static uint16_t l2cap_max_mtu_for_handle(uint16_t handle){
    return l2cap_max_mtu();
}

// END Helper Functions

static le_command_status_t att_confirmation(uint16_t peripheral_handle){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;

    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = ATT_HANDLE_VALUE_CONFIRMATION;
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 1);
    return BLE_PERIPHERAL_OK;
}

static le_command_status_t att_find_information_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;
    
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, start_handle);
    bt_store_16(request, 3, end_handle);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
    return BLE_PERIPHERAL_OK;
}


static le_command_status_t att_find_by_type_value_request(uint16_t request_type, uint16_t attribute_group_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle, uint8_t * value, uint16_t value_size){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;
    
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();

    request[0] = request_type;
    bt_store_16(request, 1, start_handle);
    bt_store_16(request, 3, end_handle);
    bt_store_16(request, 5, attribute_group_type);
    memcpy(&request[7], value, value_size);

    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 7+value_size);
    return BLE_PERIPHERAL_OK;
}

static le_command_status_t att_read_by_type_or_group_request(uint16_t request_type, uint16_t attribute_group_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;
    
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, start_handle);
    bt_store_16(request, 3, end_handle);
    bt_store_16(request, 5, attribute_group_type);
   
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 7);
    return BLE_PERIPHERAL_OK;
}

static le_command_status_t att_read_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;
    
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3);
    return BLE_PERIPHERAL_OK;
}

static le_command_status_t att_read_blob_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_offset){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;

    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    bt_store_16(request, 3, value_offset);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
    return BLE_PERIPHERAL_OK;
}


static le_command_status_t att_write_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_length, uint8_t * value){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;

    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    memcpy(&request[3], value, value_length);

    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3 + value_length);
    return BLE_PERIPHERAL_OK;
}

static le_command_status_t att_execute_write_request(uint16_t request_type, uint16_t peripheral_handle, uint8_t execute_write){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;

    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    request[1] = execute_write;
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 2);
    return BLE_PERIPHERAL_OK;
}


static le_command_status_t att_prepare_write_request(uint16_t request_type, uint16_t peripheral_handle,  uint16_t attribute_handle, uint16_t value_offset, uint16_t blob_length, uint8_t * value){
    if (!l2cap_can_send_connectionless_packet_now()) return BLE_PERIPHERAL_BUSY;

    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    bt_store_16(request, 3, value_offset);
    memcpy(&request[5], &value[value_offset], blob_length);

    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5+blob_length);
    return BLE_PERIPHERAL_OK;
}

static uint16_t write_blob_length(le_peripheral_t * peripheral){
    uint16_t max_blob_length = peripheral->mtu - 5;
    if (peripheral->attribute_offset >= peripheral->attribute_length) {
        return 0;
    }
    uint16_t rest_length = peripheral->attribute_length - peripheral->attribute_offset;
    if (max_blob_length > rest_length){
        return rest_length;
    }
    return max_blob_length;
}


static le_command_status_t send_gatt_services_request(le_peripheral_t *peripheral){
    return att_read_by_type_or_group_request(ATT_READ_BY_GROUP_TYPE_REQUEST, GATT_PRIMARY_SERVICE_UUID, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static le_command_status_t send_gatt_by_uuid_request(le_peripheral_t *peripheral, uint16_t attribute_group_type){
    if (peripheral->uuid16){
        uint8_t uuid16[2];
        bt_store_16(uuid16, 0, peripheral->uuid16);
        return att_find_by_type_value_request(ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle, uuid16, 2);
    }
    uint8_t uuid128[16];
    swap128(peripheral->uuid128, uuid128);
    return att_find_by_type_value_request(ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle, uuid128, 16);
}

static le_command_status_t send_gatt_services_by_uuid_request(le_peripheral_t *peripheral){
    return send_gatt_by_uuid_request(peripheral, GATT_PRIMARY_SERVICE_UUID);
}

static le_command_status_t send_gatt_included_service_uuid_request(le_peripheral_t *peripheral){
    return att_read_request(ATT_READ_REQUEST, peripheral->handle, peripheral->query_start_handle);
}

static le_command_status_t send_gatt_included_service_request(le_peripheral_t *peripheral){
    return att_read_by_type_or_group_request(ATT_READ_BY_TYPE_REQUEST, GATT_INCLUDE_SERVICE_UUID, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static le_command_status_t send_gatt_characteristic_request(le_peripheral_t *peripheral){
    return att_read_by_type_or_group_request(ATT_READ_BY_TYPE_REQUEST, GATT_CHARACTERISTICS_UUID, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static le_command_status_t send_gatt_characteristic_descriptor_request(le_peripheral_t *peripheral){
    return att_find_information_request(ATT_FIND_INFORMATION_REQUEST, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static le_command_status_t send_gatt_read_characteristic_value_request(le_peripheral_t *peripheral){
    return att_read_request(ATT_READ_REQUEST, peripheral->handle, peripheral->attribute_handle);
}

static le_command_status_t send_gatt_read_blob_request(le_peripheral_t *peripheral){
    return att_read_blob_request(ATT_READ_BLOB_REQUEST, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_offset);
}

static le_command_status_t send_gatt_write_attribute_value_request(le_peripheral_t * peripheral){
    return att_write_request(ATT_WRITE_REQUEST, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_length, peripheral->attribute_value);
}

static le_command_status_t send_gatt_write_client_characteristic_configuration_request(le_peripheral_t * peripheral){
    return att_write_request(ATT_WRITE_REQUEST, peripheral->handle, peripheral->client_characteristic_configuration_handle, 2, peripheral->client_characteristic_configuration_value);
}

static le_command_status_t send_gatt_prepare_write_request(le_peripheral_t * peripheral){
    return att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_offset, write_blob_length(peripheral), peripheral->attribute_value);
}

static le_command_status_t send_gatt_execute_write_request(le_peripheral_t * peripheral){
    return att_execute_write_request(ATT_EXECUTE_WRITE_REQUEST, peripheral->handle, 1);
}

static le_command_status_t send_gatt_cancel_prepared_write_request(le_peripheral_t * peripheral){
    return att_execute_write_request(ATT_EXECUTE_WRITE_REQUEST, peripheral->handle, 0);
}

static le_command_status_t send_gatt_read_client_characteristic_configuration_request(le_peripheral_t * peripheral){
    return att_read_by_type_or_group_request(ATT_READ_BY_TYPE_REQUEST, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static le_command_status_t send_gatt_read_characteristic_descriptor_request(le_peripheral_t * peripheral){
    return att_read_request(ATT_READ_REQUEST, peripheral->handle, peripheral->attribute_handle);
}

static inline void send_gatt_complete_event(le_peripheral_t * peripheral, uint8_t type, uint8_t status){
    le_peripheral_event_t event;
    event.type = type;
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
    // printf("handle_peripheral_list 1\n");
    // only one connect is allowed, wait for result
    if (get_peripheral_w4_connected()) return;
    // printf("handle_peripheral_list 2\n");
    // only one cancel connect is allowed, wait for result
    if (get_peripheral_w4_connect_cancelled()) return;
    // printf("handle_peripheral_list 3\n");

    if (!hci_can_send_packet_now_using_packet_buffer(HCI_COMMAND_DATA_PACKET)) return;
    // printf("handle_peripheral_list 4\n");
    if (!l2cap_can_send_connectionless_packet_now()) return;
    // printf("handle_peripheral_list 5\n");
    
    // printf("handle_peripheral_list empty %u\n", linked_list_empty(&le_connections));
    le_command_status_t status;
    linked_item_t *it;
    for (it = (linked_item_t *) le_connections; it ; it = it->next){
        le_peripheral_t * peripheral = (le_peripheral_t *) it;
        // printf("handle_peripheral_list, status %u\n", peripheral->state);

        if (peripheral->send_confirmation){
            peripheral->send_confirmation = 0;
            att_confirmation(peripheral->handle);
            return;
        }

        switch (peripheral->state){
            case P_W2_CONNECT:
                peripheral->state = P_W4_CONNECTED;
                hci_send_cmd(&hci_le_create_connection, 
                     1000,      // scan interval: 625 ms
                     1000,      // scan interval: 625 ms
                     0,         // don't use whitelist
                     peripheral->address_type, // peer address type
                     peripheral->address,       // peer bd addr
                     0,         // our addr type: public
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

                // TODO: extract as att_exchange_mtu_request
                l2cap_reserve_packet_buffer();
                uint8_t * request = l2cap_get_outgoing_buffer();
                request[0] = ATT_EXCHANGE_MTU_REQUEST;
                bt_store_16(request, 1, mtu);
                l2cap_send_prepared_connectionless(peripheral->handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3);
                return;
            }
            case P_W2_SEND_SERVICE_QUERY:
                status = send_gatt_services_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_SERVICE_QUERY_RESULT;
                break;

            case P_W2_SEND_SERVICE_WITH_UUID_QUERY:
                status = send_gatt_services_by_uuid_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_SERVICE_WITH_UUID_RESULT;
                break;

            case P_W2_SEND_CHARACTERISTIC_QUERY:
                status = send_gatt_characteristic_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_CHARACTERISTIC_QUERY_RESULT;
                break;

            case P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY:
                status = send_gatt_characteristic_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT;
                break;

            case P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY:
                status = send_gatt_characteristic_descriptor_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT;
                break;
            
            case P_W2_SEND_INCLUDED_SERVICE_QUERY:
                status = send_gatt_included_service_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_INCLUDED_SERVICE_QUERY_RESULT;
                break;

            case P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY:
                status = send_gatt_included_service_uuid_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT;
                break;
            
            case P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY:
                status = send_gatt_read_characteristic_value_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_READ_CHARACTERISTIC_VALUE_RESULT;
                break;

            case P_W2_SEND_READ_BLOB_QUERY:
                status = send_gatt_read_blob_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_READ_BLOB_RESULT;
                break;
            
            case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
                status = send_gatt_write_attribute_value_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;
                break;
            
            case P_W2_PREPARE_WRITE:
                status = send_gatt_prepare_write_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_PREPARE_WRITE_RESULT;
                break;

            case P_W2_PREPARE_RELIABLE_WRITE:
                status = send_gatt_prepare_write_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_PREPARE_RELIABLE_WRITE_RESULT;
                break;
            
            case P_W2_EXECUTE_PREPARED_WRITE:
                status = send_gatt_execute_write_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_EXECUTE_PREPARED_WRITE_RESULT;
                break;
            
            case P_W2_CANCEL_PREPARED_WRITE:
                status = send_gatt_cancel_prepared_write_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_CANCEL_PREPARED_WRITE_RESULT;
                break;    

            case P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY:
                status = send_gatt_read_client_characteristic_configuration_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT;
                break;

            case P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY:
                status = send_gatt_read_characteristic_descriptor_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT;
                break;
                
            case P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY:
                status = send_gatt_read_blob_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT;
                break;

            case P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR:
                status = send_gatt_write_attribute_value_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
                break;
                
            case P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:
                status = send_gatt_write_client_characteristic_configuration_request(peripheral);
                if (status != BLE_PERIPHERAL_OK) break;
                peripheral->state = P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT;
                break;
                
            case P_W2_DISCONNECT:
                peripheral->state = P_W4_DISCONNECTED;
                hci_send_cmd(&hci_disconnect, peripheral->handle,0x13);
                return;

            default:
                break;
        }
        
    }
         
}


le_command_status_t le_central_start_scan(){
    if (state != IDLE) return BLE_PERIPHERAL_IN_WRONG_STATE; 
    state = START_SCAN;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_stop_scan(){
    if (state != SCANNING) return BLE_PERIPHERAL_IN_WRONG_STATE;
    state = STOP_SCAN;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

static void le_peripheral_init(le_peripheral_t *context, uint8_t addr_type, bd_addr_t addr){
    memset(context, 0, sizeof(le_peripheral_t));
    context->address_type = addr_type;
    memcpy (context->address, addr, 6);
}

le_command_status_t le_central_connect(le_peripheral_t *context, uint8_t addr_type, bd_addr_t addr){
    //TODO: align with hci connection list capacity
    le_peripheral_t * peripheral = get_peripheral_with_address(addr_type, addr);
    if (!peripheral) {
        le_peripheral_init(context, addr_type, addr);
        context->state = P_W2_CONNECT;
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
    le_peripheral_t * peripheral = get_peripheral_with_address(context->address_type, context->address);
    if (!peripheral || (peripheral && peripheral != context)){
        return BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS;
    }

    switch(context->state){
        case P_W2_CONNECT:
            linked_list_remove(&le_connections, (linked_item_t *) context);
            send_gatt_complete_event(peripheral, GATT_CONNECTION_COMPLETE, 0);
            break; 
        case P_W4_CONNECTED:
        case P_W2_CANCEL_CONNECT:
            // trigger cancel connect
            context->state = P_W2_CANCEL_CONNECT;
            break;
        case P_W4_DISCONNECTED:
        case P_W4_CONNECT_CANCELLED: 
            return BLE_PERIPHERAL_IN_WRONG_STATE;
        default:
            context->state = P_W2_DISCONNECT;
            break;
    }  
    gatt_client_run();    
    return BLE_PERIPHERAL_OK;      
}

le_command_status_t le_central_discover_primary_services(le_peripheral_t *peripheral){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->state = P_W2_SEND_SERVICE_QUERY;

    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}


le_command_status_t le_central_discover_primary_services_by_uuid16(le_peripheral_t *peripheral, uint16_t uuid16){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    peripheral->uuid16 = uuid16;

    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_discover_primary_services_by_uuid128(le_peripheral_t *peripheral, uint8_t * uuid128){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_discover_characteristics_for_service(le_peripheral_t *peripheral, le_service_t *service){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = service->start_group_handle;
    peripheral->end_group_handle   = service->end_group_handle;
    peripheral->filter_with_uuid = 0;
    peripheral->characteristic_start_handle = 0;
    peripheral->state = P_W2_SEND_CHARACTERISTIC_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_find_included_services_for_service(le_peripheral_t *peripheral, le_service_t *service){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = service->start_group_handle;
    peripheral->end_group_handle   = service->end_group_handle;
    peripheral->state = P_W2_SEND_INCLUDED_SERVICE_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_discover_characteristics_for_handle_range_by_uuid16(le_peripheral_t *peripheral, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle   = end_handle;
    peripheral->filter_with_uuid = 1;
    peripheral->uuid16 = uuid16;
    sdp_normalize_uuid((uint8_t*) &(peripheral->uuid128), peripheral->uuid16);
    peripheral->characteristic_start_handle = 0;
    peripheral->state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_discover_characteristics_for_handle_range_by_uuid128(le_peripheral_t *peripheral, uint16_t start_handle, uint16_t end_handle, uint8_t * uuid128){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle   = end_handle;
    peripheral->filter_with_uuid = 1;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->characteristic_start_handle = 0;
    peripheral->state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;

    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_discover_characteristics_for_service_by_uuid16 (le_peripheral_t *peripheral, le_service_t *service, uint16_t  uuid16){
    return le_central_discover_characteristics_for_handle_range_by_uuid16(peripheral, service->start_group_handle, service->end_group_handle, uuid16);
}

le_command_status_t le_central_discover_characteristics_for_service_by_uuid128(le_peripheral_t *peripheral, le_service_t *service, uint8_t * uuid128){
    return le_central_discover_characteristics_for_handle_range_by_uuid128(peripheral, service->start_group_handle, service->end_group_handle, uuid128);
}

le_command_status_t le_central_discover_characteristic_descriptors(le_peripheral_t *peripheral, le_characteristic_t *characteristic){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    if (characteristic->value_handle == characteristic->end_handle){
        send_gatt_complete_event(peripheral, GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE, 0);
        return BLE_PERIPHERAL_OK;
    }
    peripheral->start_group_handle = characteristic->value_handle + 1;
    peripheral->end_group_handle   = characteristic->end_handle;
    peripheral->state = P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_read_value_of_characteristic_using_value_handle(le_peripheral_t *peripheral, uint16_t value_handle){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_offset = 0;
    peripheral->state = P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_read_value_of_characteristic(le_peripheral_t *peripheral, le_characteristic_t *characteristic){
    return le_central_read_value_of_characteristic_using_value_handle(peripheral, characteristic->value_handle);
}


le_command_status_t le_central_read_long_value_of_characteristic_using_value_handle(le_peripheral_t *peripheral, uint16_t value_handle){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_offset = 0;
    peripheral->state = P_W2_SEND_READ_BLOB_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_read_long_value_of_characteristic(le_peripheral_t *peripheral, le_characteristic_t *characteristic){
    return le_central_read_long_value_of_characteristic_using_value_handle(peripheral, characteristic->value_handle);
}

static int is_connected(le_peripheral_t * peripheral){
    return peripheral->state >= P_CONNECTED && peripheral->state < P_W2_CANCEL_CONNECT;
}

le_command_status_t le_central_write_value_of_characteristic_without_response(le_peripheral_t *peripheral, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    if (value_length >= peripheral->mtu - 3) return BLE_VALUE_TOO_LONG;
    if (!is_connected(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    return att_write_request(ATT_WRITE_COMMAND, peripheral->handle, value_handle, value_length, value);
}

le_command_status_t le_central_write_value_of_characteristic(le_peripheral_t *peripheral, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    if (value_length >= peripheral->mtu - 3) return BLE_VALUE_TOO_LONG;
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_value = value;
    peripheral->state = P_W2_SEND_WRITE_CHARACTERISTIC_VALUE;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}
 
le_command_status_t le_central_write_long_value_of_characteristic(le_peripheral_t *peripheral, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->state = P_W2_PREPARE_WRITE;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_reliable_write_long_value_of_characteristic(le_peripheral_t *peripheral, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->state = P_W2_PREPARE_RELIABLE_WRITE;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_write_client_characteristic_configuration(le_peripheral_t *peripheral, le_characteristic_t * characteristic, uint16_t configuration){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) &&
         (characteristic->properties & ATT_PROPERTY_NOTIFY) == 0) {
        log_info("le_central_write_client_characteristic_configuration: BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED");
        return BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED;   
    } else if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION) &&
                (characteristic->properties & ATT_PROPERTY_INDICATE) == 0){
        log_info("le_central_write_client_characteristic_configuration: BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED\n");
        return BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED;   
    }

    peripheral->start_group_handle = characteristic->value_handle;
    peripheral->end_group_handle = characteristic->end_handle;
    bt_store_16(peripheral->client_characteristic_configuration_value, 0, configuration);

    peripheral->state = P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_read_characteristic_descriptor(le_peripheral_t *peripheral, le_characteristic_descriptor_t * descriptor){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->attribute_handle = descriptor->handle;
    
    peripheral->uuid16 = descriptor->uuid16;
    if (!descriptor->uuid16){
        memcpy(peripheral->uuid128, descriptor->uuid128, 16);
    }

    peripheral->state = P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_read_long_characteristic_descriptor(le_peripheral_t *peripheral, le_characteristic_descriptor_t * descriptor){
    if (peripheral->state != P_CONNECTED) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->attribute_handle = descriptor->handle;
    peripheral->attribute_offset = 0;
    peripheral->state = P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_write_characteristic_descriptor(le_peripheral_t *peripheral, le_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * value){
    
    peripheral->attribute_handle = descriptor->handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    
    peripheral->state = P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t le_central_write_long_characteristic_descriptor(le_peripheral_t *peripheral, le_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * value){
    peripheral->attribute_handle = descriptor->handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    
    peripheral->state = P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}


static void gatt_client_run(){
    if (state == W4_ON) return;
    
    handle_peripheral_list();

    // check if command is send 
    if (!hci_can_send_packet_now_using_packet_buffer(HCI_COMMAND_DATA_PACKET)) return;
    if (!l2cap_can_send_connectionless_packet_now()) return;
    
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


static void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    if (packet_type != HCI_EVENT_PACKET) return;
    
	switch (packet[0]) {
		case BTSTACK_EVENT_STATE:
			// BTstack activated, get started
			if (packet[2] == HCI_STATE_WORKING) {
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
                break;
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
            send_gatt_complete_event(peripheral, GATT_CONNECTION_COMPLETE, packet[5]);
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
                        break;
                    } 
                    // cancel success?
                    peripheral = get_peripheral_w4_connect_cancelled();
                    if (!peripheral) break;
                            
                    linked_list_remove(&le_connections, (linked_item_t *) peripheral);
                    send_gatt_complete_event(peripheral, GATT_CONNECTION_COMPLETE, packet[3]);
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

    // forward to app
    if (!le_central_packet_handler) return;
    le_central_packet_handler(packet_type, packet, size);
}

static char * att_errors[] = {
    "OK",
    "Invalid Handle",
    "Read Not Permitted",
    "Write Not Permitted",
    "Invalid PDU",
    "Insufficient Authentication",
    "Request Not Supported",
    "Invalid Offset",
    "Insufficient Authorization",
    "Prepare Queue Full",
    "Attribute Not Found",
    "Attribute Not Long",
    "Insufficient Encryption Key Size",
    "Invalid Attribute Value Length",
    "Unlikely Error",
    "Insufficient Encryption",
    "Unsupported Group Type",
    "Insufficient Resources"
};

static void att_client_report_error(uint8_t * packet, uint16_t size){
    
    uint8_t error_code = packet[4];
    char * error = "Unknown";
    if (error_code <= 0x11){
        error = att_errors[error_code];
    }
    uint16_t handle = READ_BT_16(packet, 2);
    printf("ATT_ERROR_REPORT handle 0x%04x, error: %u - %s\n", handle, error_code, error);
}

static uint16_t get_last_result_handle(uint8_t * packet, uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t handle_offset = 0;
    switch (packet[0]){
        case ATT_READ_BY_TYPE_RESPONSE:
            handle_offset = 3;
            break;
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            handle_offset = 2;
            break;
    }
    return READ_BT_16(packet, size - attr_length + handle_offset);
}

static void report_gatt_services(le_peripheral_t * peripheral, uint8_t * packet,  uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 4;
    
    le_service_t service;
    le_service_event_t event;
    event.type = GATT_SERVICE_QUERY_RESULT;
    
    int i;
    for (i = 2; i < size; i += attr_length){
        service.start_group_handle = READ_BT_16(packet,i);
        service.end_group_handle = READ_BT_16(packet,i+2);
        
        if (uuid_length == 2){
            service.uuid16 = READ_BT_16(packet, i+4);
            sdp_normalize_uuid((uint8_t*) &service.uuid128, service.uuid16);
        } else {
            swap128(&packet[i+4], service.uuid128);
        }
        event.service = service;
        // printf(" report_gatt_services 0x%02x : 0x%02x-0x%02x\n", service.uuid16, service.start_group_handle, service.end_group_handle);

        (*le_central_callback)((le_central_event_t*)&event);
    }
    // printf("report_gatt_services for %02X done\n", peripheral->handle);
}

// helper
static void characteristic_start_found(le_peripheral_t * peripheral, uint16_t start_handle, uint8_t properties, uint16_t value_handle, uint8_t * uuid, uint16_t uuid_length){
   uint8_t uuid128[16];
    uint16_t uuid16;
    if (uuid_length == 2){
        uuid16 = READ_BT_16(uuid, 0);
        sdp_normalize_uuid((uint8_t*) uuid128, peripheral->uuid16);
    } else {
        swap128(uuid, uuid128);
    }

    if (peripheral->filter_with_uuid && memcmp(peripheral->uuid128, uuid128, 16) != 0) return;

    peripheral->characteristic_properties = properties;
    peripheral->characteristic_start_handle = start_handle;
    peripheral->attribute_handle = value_handle;

    if (peripheral->filter_with_uuid) return;

    peripheral->uuid16 = uuid16;
    memcpy(peripheral->uuid128, uuid128, 16);
}

static void characteristic_end_found(le_peripheral_t * peripheral, uint16_t end_handle){
    // TODO: stop searching if filter and uuid found
    
    if (!peripheral->characteristic_start_handle) return;

    le_characteristic_event_t event;
    event.type = GATT_CHARACTERISTIC_QUERY_RESULT;
    event.characteristic.start_handle = peripheral->characteristic_start_handle;
    event.characteristic.value_handle = peripheral->attribute_handle;
    event.characteristic.end_handle   = end_handle;
    event.characteristic.properties   = peripheral->characteristic_properties;
    event.characteristic.uuid16       = peripheral->uuid16;
    memcpy(event.characteristic.uuid128, peripheral->uuid128, 16);
    (*le_central_callback)((le_central_event_t*)&event);

    peripheral->characteristic_start_handle = 0;
}

static void report_gatt_characteristics(le_peripheral_t * peripheral, uint8_t * packet,  uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 5;
    int i;        
    for (i = 2; i < size; i += attr_length){
        uint16_t start_handle = READ_BT_16(packet, i);
        uint8_t  properties = packet[i+2];
        uint16_t value_handle = READ_BT_16(packet, i+3);
        characteristic_end_found(peripheral, start_handle-1);
        characteristic_start_found(peripheral, start_handle, properties, value_handle, &packet[i+5], uuid_length);
    }
}


static void report_gatt_included_service(le_peripheral_t * peripheral, uint8_t *uuid128, uint16_t uuid16){
    le_service_t service;
    service.uuid16 = uuid16;
    if (service.uuid16){
        sdp_normalize_uuid((uint8_t*) &service.uuid128, service.uuid16);
    }
    if (uuid128){
        memcpy(service.uuid128, uuid128, 16);
    }

    service.start_group_handle = peripheral->query_start_handle;
    service.end_group_handle   = peripheral->query_end_handle;
    
    le_service_event_t event;
    event.type = GATT_INCLUDED_SERVICE_QUERY_RESULT;
    event.service = service;
    (*le_central_callback)((le_central_event_t*)&event);
}

static void send_characteristic_value_event(le_peripheral_t * peripheral, uint16_t handle, uint8_t * value, uint16_t length, uint16_t offset, uint8_t event_type){
    le_characteristic_value_event_t event;
    event.type = event_type;
    event.characteristic_value_handle = handle;
    event.characteristic_value_blob_length = length; 
    event.characteristic_value = value;
    event.characteristic_value_offset = offset;
    (*le_central_callback)((le_central_event_t*)&event);
}

static void report_gatt_long_characteristic_value_blob(le_peripheral_t * peripheral, uint8_t * value, uint16_t blob_length, int value_offset){
    send_characteristic_value_event(peripheral, peripheral->attribute_handle, value, blob_length, value_offset, GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT);
}

static void report_gatt_notification(le_peripheral_t * peripheral, uint16_t handle, uint8_t * value, int length){
    send_characteristic_value_event(peripheral, handle, value, length, 0, GATT_NOTIFICATION);
}

static void report_gatt_indication(le_peripheral_t * peripheral, uint16_t handle, uint8_t * value, int length){
    send_characteristic_value_event(peripheral, handle, value, length, 0, GATT_INDICATION);
}

static void report_gatt_characteristic_value(le_peripheral_t * peripheral, uint16_t handle, uint8_t * value, int length){
    send_characteristic_value_event(peripheral, handle, value, length, 0, GATT_CHARACTERISTIC_VALUE_QUERY_RESULT);
}

static void report_gatt_characteristic_descriptor(le_peripheral_t * peripheral, uint16_t handle, uint8_t * uuid128, uint16_t uuid16, uint8_t *value, uint16_t value_length, uint16_t value_offset, uint8_t event_type){
    le_characteristic_descriptor_event_t event;
    event.type = event_type; // GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT;

    le_characteristic_descriptor_t descriptor;
    descriptor.handle = handle;
    descriptor.uuid16 = uuid16;
    descriptor.value_offset = value_offset;
    if (uuid128){
        memcpy(descriptor.uuid128, uuid128, 16);
    } else {
        sdp_normalize_uuid((uint8_t*) &descriptor.uuid128, descriptor.uuid16);
    }

    descriptor.value = value;
    descriptor.value_length = value_length;
    event.characteristic_descriptor = descriptor;
    (*le_central_callback)((le_central_event_t*)&event);
}

static void report_gatt_all_characteristic_descriptors(le_peripheral_t * peripheral, uint8_t * packet, uint16_t size, uint16_t pair_size){
    le_characteristic_descriptor_t descriptor;
    le_characteristic_descriptor_event_t event;
    event.type = GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT;
    
    int i;
    for (i = 0; i<size; i+=pair_size){
        descriptor.handle = READ_BT_16(packet,i);
        if (pair_size == 4){
            descriptor.uuid16 = READ_BT_16(packet,i+2);
            sdp_normalize_uuid((uint8_t*) &descriptor.uuid128, descriptor.uuid16);
        } else {
            descriptor.uuid16 = 0;
            swap128(&packet[i+2], descriptor.uuid128);
        }
        descriptor.value_length = 0;

        event.characteristic_descriptor = descriptor;
        (*le_central_callback)((le_central_event_t*)&event);
    }

}

static void trigger_next_query(le_peripheral_t * peripheral, uint16_t last_result_handle, peripheral_state_t next_query_state, uint8_t complete_event_type){
    if (last_result_handle < peripheral->end_group_handle){
        peripheral->start_group_handle = last_result_handle + 1;
        peripheral->state = next_query_state;
        return;
    }
    // DONE
    peripheral->state = P_CONNECTED;
    send_gatt_complete_event(peripheral, complete_event_type, 0); 
}


static inline void trigger_next_included_service_query(le_peripheral_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_INCLUDED_SERVICE_QUERY, GATT_INCLUDED_SERVICE_QUERY_COMPLETE);
}

static inline void trigger_next_service_query(le_peripheral_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_SERVICE_QUERY, GATT_SERVICE_QUERY_COMPLETE);
}

static inline void trigger_next_service_by_uuid_query(le_peripheral_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_SERVICE_WITH_UUID_QUERY, GATT_SERVICE_QUERY_COMPLETE);
}

static inline void trigger_next_characteristic_query(le_peripheral_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_CHARACTERISTIC_QUERY, GATT_CHARACTERISTIC_QUERY_COMPLETE);
}

static inline void trigger_next_characteristic_by_uuid_query(le_peripheral_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY, GATT_CHARACTERISTIC_QUERY_COMPLETE);
}

static inline void trigger_next_characteristic_descriptor_query(le_peripheral_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY, GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE);
}

static inline void trigger_next_prepare_write_query(le_peripheral_t * peripheral, peripheral_state_t next_query_state, peripheral_state_t done_state){
    peripheral->attribute_offset += write_blob_length(peripheral);
    uint16_t next_blob_length =  write_blob_length(peripheral);
    
    if (next_blob_length == 0){
        peripheral->state = done_state;
        return;
    }
    peripheral->state = next_query_state;
}

static inline void trigger_next_blob_query(le_peripheral_t * peripheral, peripheral_state_t next_query_state, uint8_t done_event, uint16_t received_blob_length){
    
    uint16_t max_blob_length = peripheral->mtu - 1;
    if (received_blob_length < max_blob_length){
        peripheral->state = P_CONNECTED;
        send_gatt_complete_event(peripheral, done_event, 0);
        return;
    }
    
    peripheral->attribute_offset += received_blob_length;
    peripheral->state = next_query_state;
}

static int is_value_valid(le_peripheral_t *peripheral, uint8_t *packet, uint16_t size){
    uint16_t attribute_handle = READ_BT_16(packet, 1);
    uint16_t value_offset = READ_BT_16(packet, 3);

    if (peripheral->attribute_handle != attribute_handle) return 0;
    if (peripheral->attribute_offset != value_offset) return 0;
    return memcmp(&peripheral->attribute_value[peripheral->attribute_offset], &packet[5], size-5) == 0;
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;
    le_peripheral_t * peripheral = get_peripheral_for_handle(handle);
    if (!peripheral) return;
    switch (packet[0]){
        case ATT_EXCHANGE_MTU_RESPONSE:
        {
            uint16_t remote_rx_mtu = READ_BT_16(packet, 1);
            uint16_t local_rx_mtu = l2cap_max_mtu_for_handle(handle);
            peripheral->mtu = remote_rx_mtu < local_rx_mtu ? remote_rx_mtu : local_rx_mtu;

            peripheral->state = P_CONNECTED; 
            send_gatt_complete_event(peripheral, GATT_CONNECTION_COMPLETE, 0);
            break;
        }
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            switch(peripheral->state){
                case P_W4_SERVICE_QUERY_RESULT:
                    report_gatt_services(peripheral, packet, size);
                    trigger_next_service_query(peripheral, get_last_result_handle(packet, size));
                    break;
                default:
                    printf("ATT_READ_BY_GROUP_TYPE_RESPONSE");
                    break;
            }
            break;
        case ATT_HANDLE_VALUE_NOTIFICATION:
            report_gatt_notification(peripheral, READ_BT_16(packet,1), &packet[3], size-3);
            break;
        
        case ATT_HANDLE_VALUE_INDICATION:
            report_gatt_indication(peripheral, READ_BT_16(packet,1), &packet[3], size-3);
            peripheral->send_confirmation = 1; 
            break;

        case ATT_READ_BY_TYPE_RESPONSE:
            switch (peripheral->state){
                case P_W4_CHARACTERISTIC_QUERY_RESULT:
                    report_gatt_characteristics(peripheral, packet, size);
                    trigger_next_characteristic_query(peripheral, get_last_result_handle(packet, size));
                    break;
                case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:

                    report_gatt_characteristics(peripheral, packet, size);
                    trigger_next_characteristic_query(peripheral, get_last_result_handle(packet, size));
                    break;
                case P_W4_INCLUDED_SERVICE_QUERY_RESULT:
                {
                    uint16_t uuid16 = 0;
                    uint16_t pair_size = packet[1];

                    if (pair_size < 7){
                        // UUIDs not available, query first included service
                        peripheral->start_group_handle = READ_BT_16(packet, 2); // ready for next query
                        peripheral->query_start_handle = READ_BT_16(packet, 4);
                        peripheral->query_end_handle = READ_BT_16(packet,6);
                        peripheral->state = P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY;
                        break;
                    }

                    uint16_t offset;
                    for (offset = 2; offset < size; offset += pair_size){
                        peripheral->query_start_handle = READ_BT_16(packet,offset+2);
                        peripheral->query_end_handle = READ_BT_16(packet,offset+4);
                        uuid16 = READ_BT_16(packet, offset+6);
                        report_gatt_included_service(peripheral, NULL, uuid16);
                    }
                    
                    trigger_next_included_service_query(peripheral, get_last_result_handle(packet, size));
                    break;
                }
                case P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT:
                    peripheral->client_characteristic_configuration_handle = READ_BT_16(packet, 2); 
                    peripheral->state = P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
                    break;
                default:
                    break;
            }
            break;
        case ATT_READ_RESPONSE: 
            switch (peripheral->state){
                case P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT: {
                    uint8_t uuid128[16];
                    swap128(&packet[1], uuid128);
                    report_gatt_included_service(peripheral, uuid128, 0);
                    trigger_next_included_service_query(peripheral, peripheral->start_group_handle);
                    break;
                }
                case P_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    peripheral->state = P_CONNECTED;
                    report_gatt_characteristic_value(peripheral, peripheral->attribute_handle, &packet[1], size-1);
                    break;

                case P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT:{
                    peripheral->state = P_CONNECTED;
                    report_gatt_characteristic_descriptor(peripheral, peripheral->attribute_handle, peripheral->uuid128, peripheral->uuid16, &packet[1], size-1, 0, GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
                    break;
                }
                default:
                    break;                
            }
            break;
        
        case ATT_FIND_BY_TYPE_VALUE_RESPONSE:
        {
            uint8_t pair_size = 4;
            le_service_t service;
            le_service_event_t event;
            event.type = GATT_SERVICE_QUERY_RESULT;
                
            int i;
            for (i = 1; i<size; i+=pair_size){
                service.start_group_handle = READ_BT_16(packet,i);
                service.end_group_handle = READ_BT_16(packet,i+2);
                memcpy(service.uuid128,  peripheral->uuid128, 16);

                event.service = service;
                (*le_central_callback)((le_central_event_t*)&event);
            }

            trigger_next_service_by_uuid_query(peripheral, service.end_group_handle);
            break;
        }
        case ATT_FIND_INFORMATION_REPLY:
        {
            uint8_t pair_size = 4;
            if (packet[1] == 2){
               pair_size = 18;     
            }
            uint16_t last_descriptor_handle = READ_BT_16(packet, size - pair_size);

            report_gatt_all_characteristic_descriptors(peripheral, &packet[2], size-2, pair_size);
            trigger_next_characteristic_descriptor_query(peripheral, last_descriptor_handle);

            break;
        }

        case ATT_WRITE_RESPONSE:
            switch (peripheral->state){
                case P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    peripheral->state = P_CONNECTED;
                    send_gatt_complete_event(peripheral, GATT_CHARACTERISTIC_VALUE_WRITE_RESPONSE, 0);
                    break;
                case P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT:
                    peripheral->state = P_CONNECTED;
                    send_gatt_complete_event(peripheral, GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_COMPLETE, 0);
                    break;
                case P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    send_gatt_complete_event(peripheral, GATT_CHARACTERISTIC_DESCRIPTOR_WRITE_RESPONSE, 0);
                    break;
                default:
                    break;
            }
            break;

        case ATT_READ_BLOB_RESPONSE:{
            uint16_t received_blob_length = size-1;
            
            switch(peripheral->state){
                case P_W4_READ_BLOB_RESULT:
                    report_gatt_long_characteristic_value_blob(peripheral, &packet[1], received_blob_length, peripheral->attribute_offset);
                    trigger_next_blob_query(peripheral, P_W2_SEND_READ_BLOB_QUERY, GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE, received_blob_length);
                    break;
                case P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    report_gatt_characteristic_descriptor(peripheral, peripheral->attribute_handle,
                        peripheral->uuid128, peripheral->uuid16, &packet[1], received_blob_length,
                        peripheral->attribute_offset, GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
                    trigger_next_blob_query(peripheral, P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY, GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_COMPLETE, received_blob_length);
                    break;
                default:
                    break;
            }
            break;
        }
        case ATT_PREPARE_WRITE_RESPONSE:
            switch (peripheral->state){
                case P_W4_PREPARE_WRITE_RESULT:{
                    peripheral->attribute_offset = READ_BT_16(packet, 3);
                    trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                    break;
                }
                case P_W4_PREPARE_RELIABLE_WRITE_RESULT:{
                    if (is_value_valid(peripheral, packet, size)){
                        peripheral->attribute_offset = READ_BT_16(packet, 3);
                        trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_RELIABLE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                        break;
                    }
                    peripheral->state = P_W2_CANCEL_PREPARED_WRITE;
                    break;
                }
                default:
                    break;
            } 
            break;
    
        case ATT_EXECUTE_WRITE_RESPONSE:
            switch (peripheral->state){
                case P_W4_EXECUTE_PREPARED_WRITE_RESULT:
                    peripheral->state = P_CONNECTED;
                    send_gatt_complete_event(peripheral, GATT_LONG_CHARACTERISTIC_VALUE_WRITE_COMPLETE, 0);
                    break;
                case P_W4_CANCEL_PREPARED_WRITE_RESULT:
                    peripheral->state = P_CONNECTED;
                    send_gatt_complete_event(peripheral, GATT_LONG_CHARACTERISTIC_VALUE_WRITE_COMPLETE, 1);
                    break;
                default:
                    break;

            }
            break;

        case ATT_ERROR_RESPONSE:
            // printf("ATT_ERROR_RESPONSE error %u, state %u\n", packet[4], peripheral->state);
            switch (packet[4]){
                case ATT_ERROR_ATTRIBUTE_NOT_FOUND: {
                    switch(peripheral->state){
                        case P_W4_SERVICE_QUERY_RESULT:
                        case P_W4_SERVICE_WITH_UUID_RESULT:
                            peripheral->state = P_CONNECTED;
                            send_gatt_complete_event(peripheral, GATT_SERVICE_QUERY_COMPLETE, 0);
                            break;

                        case P_W4_CHARACTERISTIC_QUERY_RESULT:
                        case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:
                            characteristic_end_found(peripheral, peripheral->end_group_handle);
                            peripheral->state = P_CONNECTED;
                            send_gatt_complete_event(peripheral, GATT_CHARACTERISTIC_QUERY_COMPLETE, 0);
                            break;

                        case P_W4_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
                            peripheral->state = P_CONNECTED;
                            send_gatt_complete_event(peripheral, GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE, 0);
                            break;
                        case P_W4_INCLUDED_SERVICE_QUERY_RESULT:
                            peripheral->state = P_CONNECTED;
                            send_gatt_complete_event(peripheral, GATT_INCLUDED_SERVICE_QUERY_COMPLETE, 0);
                            break;
                        case P_W4_READ_BLOB_RESULT:
                            peripheral->state = P_CONNECTED;
                            send_gatt_complete_event(peripheral, GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE, 0);
                            break;
                        case P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT:
                            peripheral->state = P_CONNECTED;
                            send_gatt_complete_event(peripheral, GATT_CLIENT_CHARACTERISTIC_CONFIGURATION_COMPLETE, 0);
                            break;
                        default:
                            printf("ATT_ERROR_ATTRIBUTE_NOT_FOUND in %d\n", peripheral->state);
                            return;
                    }

                    break;
                }
                default:                
                    att_client_report_error(packet, size);
                    break;
            }
            break;

        default:
            printf("ATT Handler, unhandled response type 0x%02x\n", packet[0]);
            break;
    }
    gatt_client_run();
}

// TEST CODE


static void hexdump2(void *data, int size){
    if (size <= 0) return;
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    // printf("\n");
}

static void printUUID(uint8_t * uuid128, uint16_t uuid16){
    printf(", uuid ");
    if (uuid16){
        printf(" 0x%02x",uuid16);
    } else {
        printUUID128(uuid128);
    }
    printf("\n");
}

static void dump_characteristic(le_characteristic_t * characteristic){
    printf("    *** characteristic *** properties %x, start handle 0x%02x, value handle 0x%02x, end handle 0x%02x", 
                            characteristic->properties, characteristic->start_handle, characteristic->value_handle, characteristic->end_handle);
    printUUID(characteristic->uuid128, characteristic->uuid16);
}

static void dump_ad_event(ad_event_t * e){
    printf("    *** adv. event *** evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", e->event_type, 
                            e->address_type, bd_addr_to_str(e->address), e->rssi, e->length); 
    hexdump2( e->data, e->length);                            
}

static void dump_service(le_service_t * service){
    printf("    *** service *** start group handle %02x, end group handle %02x", service->start_group_handle, service->end_group_handle);
    printUUID(service->uuid128, service->uuid16);
}


static void dump_descriptor(le_characteristic_descriptor_t * descriptor){
    printf("    *** descriptor *** handle 0x%02x, value ", descriptor->handle);
    hexdump2(descriptor->value, descriptor->value_length);
    printUUID(descriptor->uuid128, descriptor->uuid16);
}

static void dump_characteristic_value(le_characteristic_value_event_t * event){
    printf("    *** characteristic value of length %d *** ", event->characteristic_value_blob_length);
    hexdump2(event->characteristic_value, event->characteristic_value_blob_length);
    printf("\n");
}


le_peripheral_t test_device;
le_service_t services[40];
le_characteristic_t characteristics[100];
le_service_t service;
le_characteristic_t enable_characteristic;
le_characteristic_t config_characteristic;
le_characteristic_descriptor_t descriptor;

int service_count = 0;
int service_index = 0;
int characteristic_count = 0;
int characteristic_index = 0;

uint8_t acc_service_uuid[16] =           {0xf0, 0x00, 0xaa, 0x10, 0x04, 0x51, 0x40, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t acc_chr_client_config_uuid[16] = {0xf0, 0x00, 0xaa, 0x11, 0x04, 0x51, 0x40, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t acc_chr_enable_uuid[16] =        {0xf0, 0x00, 0xaa, 0x12, 0x04, 0x51, 0x40, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t  acc_enable[1]  = {0x01};
uint8_t  acc_disable[1] = {0x00};


static uint8_t   test_device_addr_type = 0;
// pick one:
// static bd_addr_t test_device_addr = {0x1c, 0xba, 0x8c, 0x20, 0xc7, 0xf6};
// static bd_addr_t test_device_addr = {0x00, 0x1b, 0xdc, 0x05, 0xb5, 0xdc}; // SensorTag 1
static bd_addr_t test_device_addr = {0x34, 0xb1, 0xf7, 0xd1, 0x77, 0x9b}; // SensorTag 2
// static bd_addr_t test_device_addr = {0x00, 0x02, 0x72, 0xdc, 0x31, 0xc1}; // delock40 

typedef enum {
    TC_IDLE,
    TC_W4_SCAN_RESULT,
    TC_W4_CONNECT,

    TC_W4_SERVICE_RESULT,
    TC_W4_SERVICE_WITH_UUID_RESULT,

    TC_W4_CHARACTERISTIC_RESULT,
    TC_W4_CHARACTERISTIC_WITH_UUID_RESULT,
    TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT,

    TC_W4_INCLUDED_SERVICE_RESULT,
    
    TC_W4_READ_RESULT,
    TC_W4_READ_LONG_RESULT,

    TC_W2_WRITE_WITHOUT_RESPONSE,
    TC_W4_WRITE_WITHOUT_RESPONSE_SENT,

    TC_W4_WRITE_RESULT,
    TC_W4_LONG_WRITE_RESULT,
    TC_W4_RELIABLE_WRITE_RESULT,

    TC_W4_ACC_ENABLE,
    TC_W4_ACC_CLIENT_CONFIG_CHARACTERISTIC_RESULT,
    TC_W4_ACC_SUBSCRIBE,
    TC_W4_ACC_DATA,

    TC_W4_DISCONNECT,
    TC_DISCONNECTED

} tc_state_t;

tc_state_t tc_state = TC_IDLE;
le_characteristic_t characteristic;

uint8_t chr_long_value[26] = {
    0x76, 0x75, 0x74, 0x73, 0x72, 
    0x71, 0x70, 0x6f, 0x6e, 0x6d, 
    0x6c, 0x6b, 0x6a, 0x69, 0x68, 
    0x67, 0x66, 0x65, 0x64, 0x63, 
    0x62, 0x61, 0x60, 0x5f, 0x5e,
    0x5d};
uint8_t chr_short_value[1] = {0x86};

// void sm_test_basic_cmds(le_central_event_t * event){
//     le_service_t service;
//     le_characteristic_descriptor_t descriptor;
    
//     switch(tc_state){
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_SERVICE_RESULT;
//             printf("\n test client - CONNECTED, query services now\n");
//             le_central_discover_primary_services(&test_device);
//             break;
        
//         case TC_W4_SERVICE_RESULT:
//             switch(event->type){
//                 case GATT_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     dump_service(&service);
//                     services[service_count++] = service;
//                     break;
                
//                 case GATT_SERVICE_QUERY_COMPLETE:
//                     tc_state = TC_W4_SERVICE_WITH_UUID_RESULT;
//                     printf("\n test client - SERVICE by SERVICE UUID QUERY\n"); 
//                     le_central_discover_primary_services_by_uuid16(&test_device, 0xffff);
//                     break;
                
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_SERVICE_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     dump_service(&service);
//                     break;
                
//                 case GATT_SERVICE_QUERY_COMPLETE:
//                     tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                     service_index = 0;
//                     printf("\n test client - CHARACTERISTIC for SERVICE 0x%02x QUERY\n", services[service_index].uuid16);
//                     le_central_discover_characteristics_for_service(&test_device, &services[service_index]);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_CHARACTERISTIC_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     characteristics[characteristic_count++] = characteristic;
//                     break;

//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     if (service_index < service_count) {
//                         tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                         service = services[service_index++];
//                         printf("\n test client - CHARACTERISTIC for SERVICE 0x%02x QUERY\n", service.uuid16);
//                         le_central_discover_characteristics_for_service(&test_device, &service);
//                         break;
//                     }
//                     tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT; 
//                     service_index = 0;
//                     characteristic_index = 0;
//                     printf("\n test client - CHARACTERISTIC for SERVICE 0x%02x QUERY with UUID 0x%02x\n", 
//                             services[0].uuid16, characteristics[0].uuid16);
//                     le_central_discover_characteristics_for_handle_range_by_uuid16(&test_device, 
//                         services[0].start_group_handle, services[0].end_group_handle, characteristics[0].uuid16);
//                     break;
//                 default:
//                     break;
//             }
//             break;
        
//         case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     tc_state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT;
//                     printf("\n test client - DESCRIPTOR for CHARACTERISTIC \n");
//                     dump_characteristic(&characteristics[0]);
//                     le_central_discover_characteristic_descriptors(&test_device, &characteristics[0]);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
//             switch(event->type){
//                 case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
//                     descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
//                     dump_descriptor(&descriptor);
//                     break;
//                 case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE:
//                     service_index = 0;
//                     service = services[service_index];
//                     tc_state = TC_W4_INCLUDED_SERVICE_RESULT;
//                     printf("\n test client - INCLUDED SERVICE QUERY, for service %02x\n", service.uuid16);
//                     le_central_find_included_services_for_service(&test_device, &service);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_INCLUDED_SERVICE_RESULT:
//             switch(event->type){
//                 case GATT_INCLUDED_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     printf("    *** found included service *** start group handle %02x, end group handle %02x \n", service.start_group_handle, service.end_group_handle);
//                     break;
//                 case GATT_INCLUDED_SERVICE_QUERY_COMPLETE:
//                     if (service_index < service_count) {
//                         service = services[service_index++];
//                         printf("\n test client - INCLUDED SERVICE QUERY, for service %02x\n", service.uuid16);
//                         le_central_find_included_services_for_service(&test_device, &service);
//                         break;
//                     }
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     break;
//             }
//             break;
//         default:
//             break;
//     }
// }


// void sm_test_reads(le_central_event_t * event){
//     switch(tc_state){
//         // 59 5a 5d f200
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT;
//             printf("\n test client - CONNECTED, query characteristic now\n");
//             le_central_discover_characteristics_for_handle_range_by_uuid16(&test_device, 0x59, 0x5d, 0xf200);
//             break;

//          case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     tc_state = TC_W4_READ_LONG_RESULT;
//                     printf("\n\n test client - LONG VALUE for CHARACTERISTIC \n");
//                     le_central_write_value_of_characteristic_without_response(&test_device, characteristic.value_handle, 1, chr_short_value);
//                     break;
//                 default:
//                     break;
//             }
//             break;   

//          case TC_W4_READ_LONG_RESULT:
//             switch (event->type){
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
//                     dump_characteristic_value((le_characteristic_value_event_t *)event);      
//                     break;
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE:
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     printf("TC_W4_READ_LONG_RESULT\n");
//                     break;
//             } 
//             break;

//         default:
//             break;
//         }
// }

// void sm_test_write_no_response(le_central_event_t * event){
//     switch(tc_state){
//         case TC_W4_SCAN_RESULT: {
//             if (event->type != GATT_ADVERTISEMENT) break;
//             printf("test client - SCAN ACTIVE\n");
//             ad_event_t * ad_event = (ad_event_t*) event;
//             dump_ad_event(ad_event);
//             // copy found addr
//             test_device_addr_type = ad_event->address_type;
//             bd_addr_t found_device_addr;
//             memcpy(found_device_addr, ad_event->address, 6);

//             if (memcmp(&found_device_addr, &test_device_addr, 6) != 0) break;
//             // memcpy(test_device_addr, ad_event->address, 6);

//             tc_state = TC_W4_CONNECT;
//             le_central_stop_scan();
//             le_central_connect(&test_device, test_device_addr_type, test_device_addr);
//             break;
//         }
//         // 59 5a 5d f200
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT;
//             printf("\n test client - CONNECTED, query characteristic now\n");
//             le_central_discover_characteristics_for_handle_range_by_uuid16(&test_device, 0x59, 0x5d, 0xf200);
//             break;

//         case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:{
//                     printf("\n\n test client - Write VALUE for CHARACTERISTIC \n");
//                     tc_state = TC_W2_WRITE_WITHOUT_RESPONSE;
//                     le_command_status_t  status = le_central_write_value_of_characteristic_without_response(&test_device, characteristic.value_handle, 1, chr_short_value);
//                     if (status != BLE_PERIPHERAL_OK) break;
                    
//                     tc_state = TC_W4_READ_LONG_RESULT;
//                     le_central_read_long_value_of_characteristic(&test_device, &characteristic);
//                     break;
//                 }
//                 default:
//                     break;
//             }
//             break;   

//         case TC_W4_READ_LONG_RESULT:
//             switch (event->type){
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
//                     dump_characteristic_value((le_characteristic_value_event_t *)event);      
//                     break;
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE:
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     printf("TC_W4_READ_LONG_RESULT\n");
//                     break;
//             } 
//             break;
        
//         default:
//             break;
//         }
// }

// void sm_test_long_writes(le_central_event_t * event){
//     switch(tc_state){
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_CHARACTERISTIC_WITH_UUID_RESULT;
//             printf("\n test client - CONNECTED, query characteristic now\n");
//             le_central_discover_characteristics_for_handle_range_by_uuid16(&test_device, 0x59, 0x5d, 0xf200);
//             break;

//          case TC_W4_CHARACTERISTIC_WITH_UUID_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:{
//                     printf("\n\n test client - Write LONG VALUE for CHARACTERISTIC \n");
//                     tc_state = TC_W4_WRITE_RESULT;
//                     // le_central_write_value_of_characteristic(&test_device, characteristic.value_handle, 1, chr_short_value);
//                     // le_central_write_long_value_of_characteristic(&test_device, characteristic.value_handle, 26, chr_long_value);
//                     le_central_reliable_write_long_value_of_characteristic(&test_device, characteristic.value_handle, 26, chr_long_value);
//                     break;
//                 }
//                 default:
//                     break;
//             }
//             break;   

//          case TC_W4_WRITE_RESULT:
//             printf("\n\n test client - Write LONG VALUE for CHARACTERISTIC \n");
//             tc_state = TC_W4_READ_LONG_RESULT;
//             le_central_read_long_value_of_characteristic(&test_device, &characteristic);
//             break;

//          case TC_W4_READ_LONG_RESULT:
//             switch (event->type){
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
//                     dump_characteristic_value((le_characteristic_value_event_t *)event);      
//                     break;
//                 case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_COMPLETE:
//                     tc_state = TC_W4_DISCONNECT;
//                     printf("\n\n test client - DISCONNECT ");
//                     le_central_disconnect(&test_device);
//                     break;
//                 default:
//                     printf("TC_W4_READ_LONG_RESULT\n");
//                     break;
//             } 
//             break;

//         default:
//             printf("Client, unhandled state %d\n", tc_state);
//             break;
//         }
// }
// void sm_test_write_characteristic_descriptor(le_central_event_t * event){
//     switch(tc_state){
//         case TC_W4_CONNECT:
//             if (event->type != GATT_CONNECTION_COMPLETE) break;
//             tc_state = TC_W4_SERVICE_RESULT;
//             printf("\n test client - CONNECTED, query ACC service\n");
//             le_central_discover_primary_services_by_uuid128(&test_device, acc_service_uuid);
//             break;
        
//         case TC_W4_SERVICE_RESULT:
//             switch(event->type){
//                 case GATT_SERVICE_QUERY_RESULT:
//                     service = ((le_service_event_t *) event)->service;
//                     dump_service(&service);
//                     break;
//                 case GATT_SERVICE_QUERY_COMPLETE:
//                     tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                     printf("\n test client - ENABLE CHARACTERISTIC for SERVICE QUERY : \n"); 
//                     dump_service(&service);
//                     le_central_discover_characteristics_for_service_by_uuid128(&test_device, &service, acc_chr_enable_uuid);
//                     break;
//                 default:
//                     break;
//             }
//             break;

//         case TC_W4_CHARACTERISTIC_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     enable_characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&enable_characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     tc_state = TC_W4_ACC_ENABLE; 
//                     printf("\n test client - ACC ENABLE\n");
//                     le_central_write_value_of_characteristic(&test_device, enable_characteristic.value_handle, 1, acc_enable);
//                     break;
//                 default:
//                     break;
//             }
//             break;
        
//         case TC_W4_ACC_ENABLE:
//             tc_state = TC_W4_ACC_CLIENT_CONFIG_CHARACTERISTIC_RESULT; 
//             printf("\n test client - CLIENT CONFIG CHARACTERISTIC for SERVICE QUERY with UUID"); 
//             printUUID128(service.uuid128);
//             printf("\n");
//             le_central_discover_characteristics_for_service_by_uuid128(&test_device, &service, acc_chr_client_config_uuid); 
//             break;
//         case TC_W4_ACC_CLIENT_CONFIG_CHARACTERISTIC_RESULT:
//             switch(event->type){
//                 case GATT_CHARACTERISTIC_QUERY_RESULT:
//                     config_characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                     dump_characteristic(&config_characteristic);
//                     break;
//                 case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                     tc_state = TC_W4_ACC_DATA; 
//                     printf("\n test client - ACC Client Configuration\n");
//                     le_central_write_client_characteristic_configuration(&test_device, &config_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
//                     // le_central_write_client_characteristic_configuration(&test_device, &config_characteristic, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
//                     break;
//                 default:
//                     break;
//             }
//             break;
//         case TC_W4_ACC_DATA:
//             printf("ACC Client Data: ");
//             if ( event->type != GATT_NOTIFICATION && event->type != GATT_INDICATION ) break;
//             dump_characteristic_value((le_characteristic_value_event_t *) event);
//             break;
//         default:
//             printf("Client, unhandled state %d\n", tc_state);
//             break;
//     }
// }
//
//void sm_test_read_characteristic_descriptor(le_central_event_t * event){
//    switch(tc_state){
//        case TC_W4_CONNECT:
//            if (event->type != GATT_CONNECTION_COMPLETE) break;
//            tc_state = TC_W4_SERVICE_RESULT;
//            printf("\n test client - CONNECTED, query ACC service\n");
//            le_central_discover_primary_services_by_uuid128(&test_device, acc_service_uuid);
//            break;
//            
//        case TC_W4_SERVICE_RESULT:
//            switch(event->type){
//                case GATT_SERVICE_QUERY_RESULT:
//                    service = ((le_service_event_t *) event)->service;
//                    dump_service(&service);
//                    break;
//                case GATT_SERVICE_QUERY_COMPLETE:
//                    tc_state = TC_W4_CHARACTERISTIC_RESULT;
//                    printf("\n test client - FIND ENABLE CHARACTERISTIC for SERVICE QUERY : \n");
//                    dump_service(&service);
//                    le_central_discover_characteristics_for_service_by_uuid128(&test_device, &service, acc_chr_enable_uuid);
//                    break;
//                default:
//                    break;
//            }
//            break;
//            
//        case TC_W4_CHARACTERISTIC_RESULT:
//            switch(event->type){
//                case GATT_CHARACTERISTIC_QUERY_RESULT:
//                    enable_characteristic = ((le_characteristic_event_t *) event)->characteristic;
//                    dump_characteristic(&enable_characteristic);
//                    break;
//                case GATT_CHARACTERISTIC_QUERY_COMPLETE:
//                    tc_state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT;
//                    printf("\n test client - ACC ENABLE\n");
//                    le_central_discover_characteristic_descriptors(&test_device, &enable_characteristic);
//                    break;
//                default:
//                    break;
//            }
//            break;
//            
//        case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
//            switch(event->type){
//                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
//                    descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
//                    dump_descriptor(&descriptor);
//                    break;
//                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE:
//                    le_central_read_characteristic_descriptor(&test_device, &descriptor);
//                    break;
//                case GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:{
//                    descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
//                    dump_descriptor(&descriptor);
//                    tc_state = TC_W4_DISCONNECT;
//                    printf("\n\n test client - DISCONNECT ");
//                    le_central_disconnect(&test_device);
//                    break;
//                }
//                default:
//                    break;
//            }
//            break;
//            
//        default:
//            printf("Client, unhandled state %d\n", tc_state);
//            break;
//    }
//
//}

static void handle_scan_and_connect(le_central_event_t * event){
    if (tc_state != TC_W4_SCAN_RESULT) return;
    if (event->type != GATT_ADVERTISEMENT) return;
    printf("test client - SCAN ACTIVE\n");
    ad_event_t * ad_event = (ad_event_t*) event;
    dump_ad_event(ad_event);
    // copy found addr
    test_device_addr_type = ad_event->address_type;
    bd_addr_t found_device_addr;
    memcpy(found_device_addr, ad_event->address, 6);

    if (memcmp(&found_device_addr, &test_device_addr, 6) != 0) return;
    // memcpy(test_device_addr, ad_event->address, 6);

    tc_state = TC_W4_CONNECT;
    le_central_stop_scan();
    le_central_connect(&test_device, test_device_addr_type, test_device_addr);
}

static void handle_disconnect(le_central_event_t * event){
    if (tc_state != TC_W4_DISCONNECT) return;
    if (event->type != GATT_CONNECTION_COMPLETE ) return;
    printf("  DONE\n");
}

static void handle_le_central_event(le_central_event_t * event){
    handle_scan_and_connect(event);
    handle_disconnect(event);
    
    switch(tc_state){
        case TC_W4_CONNECT:
            if (event->type != GATT_CONNECTION_COMPLETE) break;
            tc_state = TC_W4_SERVICE_RESULT;
            printf("\n test client - CONNECTED, query ACC service\n");
            le_central_discover_primary_services_by_uuid128(&test_device, acc_service_uuid);
            break;
        
        case TC_W4_SERVICE_RESULT:
            switch(event->type){
                case GATT_SERVICE_QUERY_RESULT:
                    service = ((le_service_event_t *) event)->service;
                    dump_service(&service);
                    break;
                case GATT_SERVICE_QUERY_COMPLETE:
                    tc_state = TC_W4_CHARACTERISTIC_RESULT;
                    printf("\n test client - FIND ENABLE CHARACTERISTIC for SERVICE QUERY : \n"); 
                    dump_service(&service);
                    le_central_discover_characteristics_for_service_by_uuid128(&test_device, &service, acc_chr_enable_uuid);
                    break;
                default:
                    break;
            }
            break;

        case TC_W4_CHARACTERISTIC_RESULT:
            switch(event->type){
                case GATT_CHARACTERISTIC_QUERY_RESULT:
                    enable_characteristic = ((le_characteristic_event_t *) event)->characteristic;
                    dump_characteristic(&enable_characteristic);
                    break;
                case GATT_CHARACTERISTIC_QUERY_COMPLETE:
                    tc_state = TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT; 
                    printf("\n test client - ACC ENABLE\n");
                    le_central_discover_characteristic_descriptors(&test_device, &enable_characteristic);
                    break;
                default:
                    break;
            }
            break;
        
        case TC_W4_CHARACTERISTIC_DESCRIPTOR_RESULT:
            switch(event->type){
                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
                    descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
                    dump_descriptor(&descriptor);
                    break;
                case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_COMPLETE:
                    le_central_read_long_characteristic_descriptor(&test_device, &descriptor);
                    break;
                case GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:{
                    descriptor = ((le_characteristic_descriptor_event_t *) event)->characteristic_descriptor;
                    dump_descriptor(&descriptor);
                    break;
                }
                case GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_COMPLETE:
                    printf("DONE");
                    tc_state = TC_W4_DISCONNECT;
                    printf("\n\n test client - DISCONNECT ");
                    le_central_disconnect(&test_device);
                    break;
                    
                default:
                    break;
            }
            break;

        default:
            printf("Client, unhandled state %d\n", tc_state);
            break;
        }


}

static void handle_hci_event(uint8_t packet_type, uint8_t *packet, uint16_t size){
    le_command_status_t status;
    
    if (packet_type != HCI_EVENT_PACKET) return;
    
    switch (packet[0]) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (packet[2] == HCI_STATE_WORKING) {
                printf("BTstack activated, get started!\n");
                tc_state = TC_W4_SCAN_RESULT;
                le_central_start_scan(); 
            }
            break;
        case DAEMON_EVENT_HCI_PACKET_SENT:
            switch(tc_state){
                case TC_W2_WRITE_WITHOUT_RESPONSE:
                    status = le_central_write_value_of_characteristic_without_response(&test_device, characteristic.value_handle, 1, chr_short_value);
                    if (status != BLE_PERIPHERAL_OK) break;
                    tc_state = TC_W4_READ_LONG_RESULT;
                    le_central_read_long_value_of_characteristic(&test_device, &characteristic);
                default:
                    break;
            } 

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
    le_central_init();
    le_central_register_handler(handle_le_central_event);
    le_central_register_packet_handler(handle_hci_event);
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





