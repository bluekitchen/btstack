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

#ifndef __BLE_CLIENT_H
#define __BLE_CLIENT_H

#include "btstack-config.h"

#include <btstack/utils.h>

#if defined __cplusplus
extern "C" {
#endif

#define LE_CENTRAL_MAX_INCLUDE_DEPTH 3

//*************** le client
    
typedef struct le_central_event {
    uint8_t   type;
} le_central_event_t;

typedef struct ad_event {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    uint8_t * data;
} ad_event_t;

    
//*************** gatt client
    
typedef enum{
    SEND_MTU_EXCHANGE,
    SENT_MTU_EXCHANGE,
    MTU_EXCHANGED
} gatt_client_mtu_t;
    

typedef enum{
    P_IDLE,
    P_W2_CONNECT,
    P_W4_CONNECTED,
    P_CONNECTED,
    P_W2_CANCEL_CONNECT,
    P_W4_CONNECT_CANCELLED,
    P_W2_DISCONNECT,
    P_W4_DISCONNECTED
} le_central_state_t;

typedef enum {
    P_READY,
    P_W2_SEND_SERVICE_QUERY,
    P_W4_SERVICE_QUERY_RESULT,
    P_W2_SEND_SERVICE_WITH_UUID_QUERY,
    P_W4_SERVICE_WITH_UUID_RESULT,
    
    P_W2_SEND_CHARACTERISTIC_QUERY,
    P_W4_CHARACTERISTIC_QUERY_RESULT,
    P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY,
    P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT,
    
    P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY,
    P_W4_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT,

    P_W2_SEND_INCLUDED_SERVICE_QUERY,
    P_W4_INCLUDED_SERVICE_QUERY_RESULT,
    P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY,
    P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT,
    
    P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY,
    P_W4_READ_CHARACTERISTIC_VALUE_RESULT,

    P_W2_SEND_READ_BLOB_QUERY,
    P_W4_READ_BLOB_RESULT,

    P_W2_SEND_WRITE_CHARACTERISTIC_VALUE,
    P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT,
    
    P_W2_PREPARE_WRITE,
    P_W4_PREPARE_WRITE_RESULT,
    P_W2_PREPARE_RELIABLE_WRITE,
    P_W4_PREPARE_RELIABLE_WRITE_RESULT,

    P_W2_EXECUTE_PREPARED_WRITE,
    P_W4_EXECUTE_PREPARED_WRITE_RESULT,
    P_W2_CANCEL_PREPARED_WRITE,
    P_W4_CANCEL_PREPARED_WRITE_RESULT,

    P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY,
    P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT,
    P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION,
    P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT,

    P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY,
    P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT,
    
    P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY,
    P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT,
    
    P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR,
    P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT,
    
    P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR,
    P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT,
    P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR,
    P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT
} gatt_client_state_t;

typedef enum {
    BLE_PERIPHERAL_OK = 0,
    BLE_PERIPHERAL_IN_WRONG_STATE,
    BLE_PERIPHERAL_DIFFERENT_CONTEXT_FOR_ADDRESS_ALREADY_EXISTS,
    BLE_PERIPHERAL_NOT_CONNECTED,
    BLE_VALUE_TOO_LONG,
    BLE_PERIPHERAL_BUSY, 
    BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED,
    BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED
} le_command_status_t;


typedef struct le_central{
    linked_item_t    item;
    le_central_state_t le_central_state;

    uint8_t   address_type;
    bd_addr_t address;
    uint16_t handle;
} le_central_t;


typedef struct gatt_client{
    linked_item_t    item;
    gatt_client_state_t gatt_client_state;
    // le_central_state_t le_central_state;
    
    uint16_t handle;
    
    uint8_t   address_type;
    bd_addr_t address;
    uint16_t mtu;
    gatt_client_mtu_t mtu_state;
    
    uint16_t uuid16;
    uint8_t  uuid128[16];
    
    uint16_t start_group_handle;
    uint16_t end_group_handle;
    
    uint16_t query_start_handle;
    uint16_t query_end_handle;
    
    uint8_t  characteristic_properties;
    uint16_t characteristic_start_handle;
    
    uint16_t attribute_handle;
    uint16_t attribute_offset;
    uint16_t attribute_length;
    uint8_t* attribute_value;
    
    uint16_t client_characteristic_configuration_handle;
    uint8_t client_characteristic_configuration_value[2];
    
    uint8_t  filter_with_uuid;
    uint8_t  send_confirmation;
    
} gatt_client_t;

typedef struct le_central_connection_complete_event{
    uint8_t   type;
    le_central_t * device;
    uint8_t status;
} le_central_connection_complete_event_t;

typedef struct gatt_complete_event{
    uint8_t   type;
    gatt_client_t * device;
    uint8_t status;
} gatt_complete_event_t;

typedef struct le_service{
    uint16_t start_group_handle;
    uint16_t end_group_handle;
    uint16_t uuid16; 
    uint8_t  uuid128[16]; 
} le_service_t;

typedef struct le_service_event{
    uint8_t  type;
    le_service_t service; 
} le_service_event_t;

typedef struct le_characteristic{
    uint8_t  properties;
    uint16_t start_handle;
    uint16_t value_handle;
    uint16_t end_handle;
    uint16_t uuid16;
    uint8_t  uuid128[16];
} le_characteristic_t;

typedef struct le_characteristic_event{
    uint8_t  type;
    le_characteristic_t characteristic; 
} le_characteristic_event_t;

typedef struct le_characteristic_descriptor{
    uint16_t handle;
    uint16_t uuid16;
    uint8_t  uuid128[16];
    uint16_t value_length;
    uint16_t value_offset;
    uint8_t * value;
} le_characteristic_descriptor_t;

typedef struct le_characteristic_descriptor_event{
    uint8_t  type;
    le_characteristic_descriptor_t characteristic_descriptor; 
} le_characteristic_descriptor_event_t;

typedef struct le_characteristic_value{
    
} le_characteristic_value_t;

typedef struct le_characteristic_value_event{
    uint8_t  type;
    uint16_t characteristic_value_handle;
    uint16_t characteristic_value_blob_length;
    uint16_t characteristic_value_offset;
    uint8_t * characteristic_value; 
} le_characteristic_value_event_t;


void ble_client_init();
void le_central_register_handler(void (*le_callback)(le_central_event_t * event));

le_command_status_t le_central_start_scan();
// creates one event per found peripheral device
// { type (8), addr_type (8), addr(48), rssi(8), ad_len(8), ad_data(ad_len*8) }
le_command_status_t le_central_stop_scan();

le_command_status_t  le_central_connect(le_central_t *context, uint8_t addr_type, bd_addr_t addr);
le_command_status_t  le_central_disconnect(le_central_t *context);

// start/stop gatt client
void gatt_client_start(gatt_client_t *context, uint16_t handle);
void gatt_client_stop(gatt_client_t *context);
    
// returns primary services
le_command_status_t gatt_client_discover_primary_services(gatt_client_t *context);
// { type (8), gatt_client_t *context, le_service * }


//TODO: define uuid type
le_command_status_t gatt_client_discover_primary_services_by_uuid16(gatt_client_t *context, uint16_t uuid16);
le_command_status_t gatt_client_discover_primary_services_by_uuid128(gatt_client_t *context, const uint8_t * uuid);

// Returns included services.
// Information about service type (primary/secondary) can be retrieved either by sending an ATT find query or 
// by comparing the service to the list of primary services obtained by calling le_central_get_services.
le_command_status_t gatt_client_find_included_services_for_service(gatt_client_t *context, le_service_t *service);
// { type (8), gatt_client_t *context, le_service * }

// returns characteristics, no included services
le_command_status_t gatt_client_discover_characteristics_for_service(gatt_client_t *context, le_service_t *service);
// { type (8), gatt_client_t *context, service_handle, le_characteristic *}

// gets all characteristics in handle range, and returns those that match the given UUID.
le_command_status_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(gatt_client_t *context, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16);
// { type (8), gatt_client_t *context, service_handle, le_characteristic *}
le_command_status_t gatt_client_discover_characteristics_for_handle_range_by_uuid128(gatt_client_t *context, uint16_t start_handle, uint16_t end_handle, uint8_t * uuid);
// { type (8), gatt_client_t *context, service_handle, le_characteristic *}

// more convenience
le_command_status_t gatt_client_discover_characteristics_for_service_by_uuid16 (gatt_client_t *context, le_service_t *service, uint16_t  uuid16);
le_command_status_t gatt_client_discover_characteristics_for_service_by_uuid128(gatt_client_t *context, le_service_t *service, uint8_t * uuid128);

// returns handle and uuid16 of a descriptor
le_command_status_t gatt_client_discover_characteristic_descriptors(gatt_client_t *context, le_characteristic_t *characteristic);

// Reads value of characteristic using characteristic value handle
le_command_status_t gatt_client_read_value_of_characteristic(gatt_client_t *context, le_characteristic_t *characteristic);
le_command_status_t gatt_client_read_value_of_characteristic_using_value_handle(gatt_client_t *context, uint16_t characteristic_value_handle);

// Reads long caharacteristic value.
le_command_status_t gatt_client_read_long_value_of_characteristic(gatt_client_t *context, le_characteristic_t *characteristic);
le_command_status_t gatt_client_read_long_value_of_characteristic_using_value_handle(gatt_client_t *context, uint16_t characteristic_value_handle);


le_command_status_t gatt_client_write_value_of_characteristic_without_response(gatt_client_t *context, uint16_t characteristic_value_handle, uint16_t length, uint8_t * data);
le_command_status_t gatt_client_write_value_of_characteristic(gatt_client_t *context, uint16_t characteristic_value_handle, uint16_t length, uint8_t * data);

le_command_status_t gatt_client_write_long_value_of_characteristic(gatt_client_t *context, uint16_t characteristic_value_handle, uint16_t length, uint8_t * data);
le_command_status_t gatt_client_reliable_write_long_value_of_characteristic(gatt_client_t *context, uint16_t characteristic_value_handle, uint16_t length, uint8_t * data);


le_command_status_t gatt_client_read_characteristic_descriptor(gatt_client_t *context, le_characteristic_descriptor_t * descriptor);
le_command_status_t gatt_client_read_long_characteristic_descriptor(gatt_client_t *context, le_characteristic_descriptor_t * descriptor);

le_command_status_t gatt_client_write_characteristic_descriptor(gatt_client_t *context, le_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * data);
le_command_status_t gatt_client_write_long_characteristic_descriptor(gatt_client_t *context, le_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * data);

le_command_status_t gatt_client_write_client_characteristic_configuration(gatt_client_t *context, le_characteristic_t * characteristic, uint16_t configuration);

// { read/write/subscribe/unsubscribe confirm/result}

// { type, le_peripheral *, characteristic handle, int len, uint8_t data[]?}



#if defined __cplusplus
}
#endif
#endif // __BLE_CLIENT_H
