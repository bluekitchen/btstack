/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>
#include <btstack/utils.h>
#include <btstack/sdp_util.h>

#include "btstack-config.h"

#include "gatt_client.h"
#include "ad_parser.h"

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "att.h"
#include "att_dispatch.h"
#include "sm.h"
#include "le_device_db.h"

static linked_list_t gatt_client_connections = NULL;
static linked_list_t gatt_subclients = NULL;
static uint16_t gatt_client_id = 0;

static void gatt_client_att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size);
static void gatt_client_report_error_if_pending(gatt_client_t *peripheral, uint8_t error_code);
static void att_signed_write_handle_cmac_result(uint8_t hash[8]);

static uint16_t peripheral_mtu(gatt_client_t *peripheral){
    if (peripheral->mtu > l2cap_max_le_mtu()){
        log_error("Peripheral mtu is not initialized");
        return l2cap_max_le_mtu();
    }
    return peripheral->mtu;
}

static uint16_t gatt_client_next_id(void){
    if (gatt_client_id < 0xFFFF) {
        gatt_client_id++;
    } else {
        gatt_client_id = 1;
    }
    return gatt_client_id;
}

static gatt_client_callback_t gatt_client_callback_for_id(uint16_t id){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &gatt_subclients);
    while (linked_list_iterator_has_next(&it)){
        gatt_subclient_t * item = (gatt_subclient_t*) linked_list_iterator_next(&it);
        if ( item->id != id) continue;
        return item->callback;
    } 
    return NULL;
}

uint16_t gatt_client_register_packet_handler(gatt_client_callback_t gatt_callback){
    if (gatt_callback == NULL){
        log_error("gatt_client_register_packet_handler called with NULL callback");
        return 0;
    }

    gatt_subclient_t * subclient = btstack_memory_gatt_subclient_get();
    if (!subclient) {
        log_error("gatt_client_register_packet_handler failed (no memory)");
        return 0;
    } 

    subclient->id = gatt_client_next_id();
    subclient->callback = gatt_callback;
    linked_list_add(&gatt_subclients, (linked_item_t *) subclient);
    log_info("gatt_client_register_packet_handler with new id %u", subclient->id);

    return subclient->id;
}

void gatt_client_unregister_packet_handler(uint16_t gatt_client_id){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &gatt_subclients);
    while (linked_list_iterator_has_next(&it)){
        gatt_subclient_t * subclient = (gatt_subclient_t*) linked_list_iterator_next(&it);
        if ( subclient->id != gatt_client_id) continue;
        linked_list_remove(&gatt_subclients, (linked_item_t *) subclient);
        btstack_memory_gatt_subclient_free(subclient);
    } 
}

void gatt_client_init(void){
    gatt_client_connections = NULL;
    att_dispatch_register_client(gatt_client_att_packet_handler);
}

static gatt_client_t * gatt_client_for_timer(timer_source_t * ts){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &gatt_client_connections);
    while (linked_list_iterator_has_next(&it)){
        gatt_client_t * peripheral = (gatt_client_t *) linked_list_iterator_next(&it);
        if ( &peripheral->gc_timeout == ts) {
            return peripheral;
        }
    }
    return NULL;
}

static void gatt_client_timeout_handler(timer_source_t * timer){
    gatt_client_t * peripheral = gatt_client_for_timer(timer);
    if (!peripheral) return;
    log_info("GATT client timeout handle, handle 0x%02x", peripheral->handle);
    gatt_client_report_error_if_pending(peripheral, ATT_ERROR_TIMEOUT);           
}

static void gatt_client_timeout_start(gatt_client_t * peripheral){
    log_info("GATT client timeout start, handle 0x%02x", peripheral->handle);
    run_loop_remove_timer(&peripheral->gc_timeout);
    run_loop_set_timer_handler(&peripheral->gc_timeout, gatt_client_timeout_handler);
    run_loop_set_timer(&peripheral->gc_timeout, 30000); // 30 seconds sm timeout
    run_loop_add_timer(&peripheral->gc_timeout);
}

static void gatt_client_timeout_stop(gatt_client_t * peripheral){
    log_info("GATT client timeout stop, handle 0x%02x", peripheral->handle);
    run_loop_remove_timer(&peripheral->gc_timeout);
}

static gatt_client_t * get_gatt_client_context_for_handle(uint16_t handle){
    linked_item_t *it;
    for (it = (linked_item_t *) gatt_client_connections; it ; it = it->next){
        gatt_client_t * peripheral = (gatt_client_t *) it;
        if (peripheral->handle == handle){
            return peripheral;
        }
    }
    return NULL;
}


// @returns context
// returns existing one, or tries to setup new one
static gatt_client_t * provide_context_for_conn_handle(uint16_t con_handle){
    gatt_client_t * context = get_gatt_client_context_for_handle(con_handle);
    if (context) return  context;

    context = btstack_memory_gatt_client_get();
    if (!context) return NULL;
    // init state
    context->handle = con_handle;
    context->mtu = ATT_DEFAULT_MTU;
    context->mtu_state = SEND_MTU_EXCHANGE;
    context->gatt_client_state = P_READY;
    linked_list_add(&gatt_client_connections, (linked_item_t*)context);
    return context;
}

static gatt_client_t * provide_context_for_conn_handle_and_start_timer(uint16_t con_handle){
    gatt_client_t * context = provide_context_for_conn_handle(con_handle);
    if (!context) return NULL;
    gatt_client_timeout_start(context);
    return context;
}

static int is_ready(gatt_client_t * context){
    return context->gatt_client_state == P_READY;
}

int gatt_client_is_ready(uint16_t handle){
    gatt_client_t * context = provide_context_for_conn_handle(handle);
    if (!context) return 0;
    return is_ready(context);
}

le_command_status_t gatt_client_get_mtu(uint16_t handle, uint16_t * mtu){
    gatt_client_t * context = provide_context_for_conn_handle(handle);
    if (context && context->mtu_state == MTU_EXCHANGED){
        *mtu = context->mtu;
        return BLE_PERIPHERAL_OK;
    } 
    *mtu = ATT_DEFAULT_MTU;
    return BLE_PERIPHERAL_IN_WRONG_STATE;
}

// precondition: can_send_packet_now == TRUE
static void att_confirmation(uint16_t peripheral_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = ATT_HANDLE_VALUE_CONFIRMATION;
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 1);
}

// precondition: can_send_packet_now == TRUE
static void att_find_information_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, start_handle);
    bt_store_16(request, 3, end_handle);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
}

// precondition: can_send_packet_now == TRUE
static void att_find_by_type_value_request(uint16_t request_type, uint16_t attribute_group_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle, uint8_t * value, uint16_t value_size){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    
    request[0] = request_type;
    bt_store_16(request, 1, start_handle);
    bt_store_16(request, 3, end_handle);
    bt_store_16(request, 5, attribute_group_type);
    memcpy(&request[7], value, value_size);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 7+value_size);
}

// precondition: can_send_packet_now == TRUE
static void att_read_by_type_or_group_request(uint16_t request_type, uint16_t attribute_group_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, start_handle);
    bt_store_16(request, 3, end_handle);
    bt_store_16(request, 5, attribute_group_type);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 7);
}

// precondition: can_send_packet_now == TRUE
static void att_read_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3);
}

// precondition: can_send_packet_now == TRUE
static void att_read_blob_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_offset){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    bt_store_16(request, 3, value_offset);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
}

// precondition: can_send_packet_now == TRUE
static void att_signed_write_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_length, uint8_t * value, uint32_t sign_counter, uint8_t sgn[8]){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    memcpy(&request[3], value, value_length);
    bt_store_32(request, 3 + value_length, sign_counter);
    memcpy(&request[3 + value_length+4], sgn, 8);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3 + value_length + 12);
}

// precondition: can_send_packet_now == TRUE
static void att_write_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_length, uint8_t * value){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    memcpy(&request[3], value, value_length);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3 + value_length);
}

// precondition: can_send_packet_now == TRUE
static void att_execute_write_request(uint16_t request_type, uint16_t peripheral_handle, uint8_t execute_write){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    request[1] = execute_write;
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 2);
}

// precondition: can_send_packet_now == TRUE
static void att_prepare_write_request(uint16_t request_type, uint16_t peripheral_handle,  uint16_t attribute_handle, uint16_t value_offset, uint16_t blob_length, uint8_t * value){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    bt_store_16(request, 1, attribute_handle);
    bt_store_16(request, 3, value_offset);
    memcpy(&request[5], &value[value_offset], blob_length);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5+blob_length);
}

static void att_exchange_mtu_request(uint16_t peripheral_handle){
    uint16_t mtu = l2cap_max_le_mtu();
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = ATT_EXCHANGE_MTU_REQUEST;
    bt_store_16(request, 1, mtu);
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3);
}

static uint16_t write_blob_length(gatt_client_t * peripheral){
    uint16_t max_blob_length = peripheral_mtu(peripheral) - 5;
    if (peripheral->attribute_offset >= peripheral->attribute_length) {
        return 0;
    }
    uint16_t rest_length = peripheral->attribute_length - peripheral->attribute_offset;
    if (max_blob_length > rest_length){
        return rest_length;
    }
    return max_blob_length;
}

static void send_gatt_services_request(gatt_client_t *peripheral){
    att_read_by_type_or_group_request(ATT_READ_BY_GROUP_TYPE_REQUEST, GATT_PRIMARY_SERVICE_UUID, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_by_uuid_request(gatt_client_t *peripheral, uint16_t attribute_group_type){
    if (peripheral->uuid16){
        uint8_t uuid16[2];
        bt_store_16(uuid16, 0, peripheral->uuid16);
        att_find_by_type_value_request(ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle, uuid16, 2);
        return;
    }
    uint8_t uuid128[16];
    swap128(peripheral->uuid128, uuid128);
    att_find_by_type_value_request(ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle, uuid128, 16);
}

static void send_gatt_services_by_uuid_request(gatt_client_t *peripheral){
    send_gatt_by_uuid_request(peripheral, GATT_PRIMARY_SERVICE_UUID);
}

static void send_gatt_included_service_uuid_request(gatt_client_t *peripheral){
    att_read_request(ATT_READ_REQUEST, peripheral->handle, peripheral->query_start_handle);
}

static void send_gatt_included_service_request(gatt_client_t *peripheral){
    att_read_by_type_or_group_request(ATT_READ_BY_TYPE_REQUEST, GATT_INCLUDE_SERVICE_UUID, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_characteristic_request(gatt_client_t *peripheral){
    att_read_by_type_or_group_request(ATT_READ_BY_TYPE_REQUEST, GATT_CHARACTERISTICS_UUID, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_characteristic_descriptor_request(gatt_client_t *peripheral){
    att_find_information_request(ATT_FIND_INFORMATION_REQUEST, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_read_characteristic_value_request(gatt_client_t *peripheral){
    att_read_request(ATT_READ_REQUEST, peripheral->handle, peripheral->attribute_handle);
}

static void send_gatt_read_blob_request(gatt_client_t *peripheral){
    att_read_blob_request(ATT_READ_BLOB_REQUEST, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_offset);
}

static void send_gatt_write_attribute_value_request(gatt_client_t * peripheral){
    att_write_request(ATT_WRITE_REQUEST, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_length, peripheral->attribute_value);
}

static void send_gatt_write_client_characteristic_configuration_request(gatt_client_t * peripheral){
    att_write_request(ATT_WRITE_REQUEST, peripheral->handle, peripheral->client_characteristic_configuration_handle, 2, peripheral->client_characteristic_configuration_value);
}

static void send_gatt_prepare_write_request(gatt_client_t * peripheral){
    att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_offset, write_blob_length(peripheral), peripheral->attribute_value);
}

static void send_gatt_execute_write_request(gatt_client_t * peripheral){
    att_execute_write_request(ATT_EXECUTE_WRITE_REQUEST, peripheral->handle, 1);
}

static void send_gatt_cancel_prepared_write_request(gatt_client_t * peripheral){
    att_execute_write_request(ATT_EXECUTE_WRITE_REQUEST, peripheral->handle, 0);
}

static void send_gatt_read_client_characteristic_configuration_request(gatt_client_t * peripheral){
    att_read_by_type_or_group_request(ATT_READ_BY_TYPE_REQUEST, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION, peripheral->handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_read_characteristic_descriptor_request(gatt_client_t * peripheral){
    att_read_request(ATT_READ_REQUEST, peripheral->handle, peripheral->attribute_handle);
}

static void send_gatt_signed_write_request(gatt_client_t * peripheral, uint32_t sign_counter){
    att_signed_write_request(ATT_SIGNED_WRITE_COMMAND, peripheral->handle, peripheral->attribute_handle, peripheral->attribute_length, peripheral->attribute_value, sign_counter, peripheral->cmac);
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

static void gatt_client_handle_transaction_complete(gatt_client_t * peripheral){
    peripheral->gatt_client_state = P_READY;
    gatt_client_timeout_stop(peripheral);
}

static void emit_event(uint16_t gatt_client_id, le_event_t* event){
    gatt_client_callback_t gatt_client_callback = gatt_client_callback_for_id(gatt_client_id); 
    if (!gatt_client_callback) return;
    (*gatt_client_callback)(event);
}

static void emit_gatt_complete_event(gatt_client_t * peripheral, uint8_t status){
    gatt_complete_event_t event;
    event.type = GATT_QUERY_COMPLETE;
    event.handle = peripheral->handle;
    event.attribute_handle = peripheral->attribute_handle;
    event.status = status;

    emit_event(peripheral->subclient_id, (le_event_t*)&event);
}

static void report_gatt_services(gatt_client_t * peripheral, uint8_t * packet,  uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 4;
    
    le_service_t service;
    le_service_event_t event;
    event.type = GATT_SERVICE_QUERY_RESULT;
    event.handle = peripheral->handle;

    int i;
    for (i = 2; i < size; i += attr_length){
        service.start_group_handle = READ_BT_16(packet,i);
        service.end_group_handle = READ_BT_16(packet,i+2);
        
        if (uuid_length == 2){
            service.uuid16 = READ_BT_16(packet, i+4);
            sdp_normalize_uuid((uint8_t*) &service.uuid128, service.uuid16);
        } else {
            service.uuid16 = 0;
            swap128(&packet[i+4], service.uuid128);
        }
        event.service = service;
        // log_info(" report_gatt_services 0x%02x : 0x%02x-0x%02x", service.uuid16, service.start_group_handle, service.end_group_handle);
        
        emit_event(peripheral->subclient_id, (le_event_t*)&event);
    }
    // log_info("report_gatt_services for %02X done", peripheral->handle);
}

// helper
static void characteristic_start_found(gatt_client_t * peripheral, uint16_t start_handle, uint8_t properties, uint16_t value_handle, uint8_t * uuid, uint16_t uuid_length){
    uint8_t uuid128[16];
    uint16_t uuid16 = 0;
    if (uuid_length == 2){
        uuid16 = READ_BT_16(uuid, 0);
        sdp_normalize_uuid((uint8_t*) uuid128, uuid16);
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

static void characteristic_end_found(gatt_client_t * peripheral, uint16_t end_handle){
    // TODO: stop searching if filter and uuid found
    
    if (!peripheral->characteristic_start_handle) return;
    
    le_characteristic_event_t event;
    event.type = GATT_CHARACTERISTIC_QUERY_RESULT;
    event.handle = peripheral->handle;
    event.characteristic.start_handle = peripheral->characteristic_start_handle;
    event.characteristic.value_handle = peripheral->attribute_handle;
    event.characteristic.end_handle   = end_handle;
    event.characteristic.properties   = peripheral->characteristic_properties;
    event.characteristic.uuid16       = peripheral->uuid16;
    memcpy(event.characteristic.uuid128, peripheral->uuid128, 16);
    
    emit_event(peripheral->subclient_id, (le_event_t*)&event);
    
    peripheral->characteristic_start_handle = 0;
}

static void report_gatt_characteristics(gatt_client_t * peripheral, uint8_t * packet,  uint16_t size){
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


static void report_gatt_included_service(gatt_client_t * peripheral, uint8_t *uuid128, uint16_t uuid16){
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
    event.handle = peripheral->handle;
    emit_event(peripheral->subclient_id, (le_event_t*)&event);
}

static void send_characteristic_value_event(gatt_client_t * peripheral, uint16_t value_handle, uint8_t * value, uint16_t length, uint16_t offset, uint8_t event_type){
    le_characteristic_value_event_t event;
    event.type = event_type;
    event.handle = peripheral->handle;
    event.value_handle = value_handle;
    event.value_offset = offset;
    event.blob_length = length;
    event.blob = value;
    emit_event(peripheral->subclient_id, (le_event_t*)&event);
}

static void report_gatt_long_characteristic_value_blob(gatt_client_t * peripheral, uint8_t * value, uint16_t blob_length, int value_offset){
    send_characteristic_value_event(peripheral, peripheral->attribute_handle, value, blob_length, value_offset, GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT);
}

static void report_gatt_notification(gatt_client_t * peripheral, uint16_t handle, uint8_t * value, int length){
    send_characteristic_value_event(peripheral, handle, value, length, 0, GATT_NOTIFICATION);
}

static void report_gatt_indication(gatt_client_t * peripheral, uint16_t handle, uint8_t * value, int length){
    send_characteristic_value_event(peripheral, handle, value, length, 0, GATT_INDICATION);
}

static void report_gatt_characteristic_value(gatt_client_t * peripheral, uint16_t handle, uint8_t * value, int length){
    send_characteristic_value_event(peripheral, handle, value, length, 0, GATT_CHARACTERISTIC_VALUE_QUERY_RESULT);
}

static void report_gatt_characteristic_descriptor(gatt_client_t * peripheral, uint16_t handle, uint8_t *value, uint16_t value_length, uint16_t value_offset, uint8_t event_type){
    le_characteristic_descriptor_event_t event;
    event.type = event_type;
    event.handle = peripheral->handle;
    
    le_characteristic_descriptor_t descriptor;
    descriptor.handle = handle;
    descriptor.uuid16 = peripheral->uuid16;
    memcpy(descriptor.uuid128, peripheral->uuid128, 16);
    event.value_offset = value_offset;
    
    event.value = value;
    event.value_length = value_length;
    event.characteristic_descriptor = descriptor;
    emit_event(peripheral->subclient_id, (le_event_t*)&event);
}

static void report_gatt_all_characteristic_descriptors(gatt_client_t * peripheral, uint8_t * packet, uint16_t size, uint16_t pair_size){
    le_characteristic_descriptor_t descriptor;
    le_characteristic_descriptor_event_t event;
    event.type = GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT;
    event.handle = peripheral->handle;

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
        event.value_length = 0;
        
        event.characteristic_descriptor = descriptor;
        emit_event(peripheral->subclient_id, (le_event_t*)&event);
    }
    
}

static void trigger_next_query(gatt_client_t * peripheral, uint16_t last_result_handle, gatt_client_state_t next_query_state){
    if (last_result_handle < peripheral->end_group_handle){
        peripheral->start_group_handle = last_result_handle + 1;
        peripheral->gatt_client_state = next_query_state;
        return;
    }
    // DONE
    gatt_client_handle_transaction_complete(peripheral);
    emit_gatt_complete_event(peripheral, 0);
}


static inline void trigger_next_included_service_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_INCLUDED_SERVICE_QUERY);
}

static inline void trigger_next_service_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_SERVICE_QUERY);
}

static inline void trigger_next_service_by_uuid_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_SERVICE_WITH_UUID_QUERY);
}

static inline void trigger_next_characteristic_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY);
}

static inline void trigger_next_characteristic_descriptor_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY);
}

static inline void trigger_next_prepare_write_query(gatt_client_t * peripheral, gatt_client_state_t next_query_state, gatt_client_state_t done_state){
    peripheral->attribute_offset += write_blob_length(peripheral);
    uint16_t next_blob_length =  write_blob_length(peripheral);
    
    if (next_blob_length == 0){
        peripheral->gatt_client_state = done_state;
        return;
    }
    peripheral->gatt_client_state = next_query_state;
}

static inline void trigger_next_blob_query(gatt_client_t * peripheral, gatt_client_state_t next_query_state, uint16_t received_blob_length){
    
    uint16_t max_blob_length = peripheral_mtu(peripheral) - 1;
    if (received_blob_length < max_blob_length){
        gatt_client_handle_transaction_complete(peripheral);
        emit_gatt_complete_event(peripheral, 0);
        return;
    }
    
    peripheral->attribute_offset += received_blob_length;
    peripheral->gatt_client_state = next_query_state;
}

static int is_value_valid(gatt_client_t *peripheral, uint8_t *packet, uint16_t size){
    uint16_t attribute_handle = READ_BT_16(packet, 1);
    uint16_t value_offset = READ_BT_16(packet, 3);
    
    if (peripheral->attribute_handle != attribute_handle) return 0;
    if (peripheral->attribute_offset != value_offset) return 0;
    return memcmp(&peripheral->attribute_value[peripheral->attribute_offset], &packet[5], size-5) == 0;
}


static void gatt_client_run(void){

    linked_item_t *it;
    for (it = (linked_item_t *) gatt_client_connections; it ; it = it->next){

        gatt_client_t * peripheral = (gatt_client_t *) it;

        if (!l2cap_can_send_fixed_channel_packet_now(peripheral->handle)) return;

        // log_info("- handle_peripheral_list, mtu state %u, client state %u", peripheral->mtu_state, peripheral->gatt_client_state);
        
        switch (peripheral->mtu_state) {
            case SEND_MTU_EXCHANGE:{
                peripheral->mtu_state = SENT_MTU_EXCHANGE;
                att_exchange_mtu_request(peripheral->handle);
                return;
            }
            case SENT_MTU_EXCHANGE:
                return;
            default:
                break;
        }
        
        if (peripheral->send_confirmation){
            peripheral->send_confirmation = 0;
            att_confirmation(peripheral->handle);
            return;
        }
        
        // check MTU for writes
        switch (peripheral->gatt_client_state){
            case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
            case P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR:

                if (peripheral->attribute_length < peripheral_mtu(peripheral) - 3) break;
                printf(".. ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH %u > %u\n", peripheral->attribute_length, peripheral_mtu(peripheral));
                gatt_client_handle_transaction_complete(peripheral);
                emit_gatt_complete_event(peripheral, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
                return;
            default:
                break;
        }

        // log_info("gatt_client_state %u", peripheral->gatt_client_state);
        switch (peripheral->gatt_client_state){
            case P_W2_SEND_SERVICE_QUERY:
                peripheral->gatt_client_state = P_W4_SERVICE_QUERY_RESULT;
                send_gatt_services_request(peripheral);
                return;
                
            case P_W2_SEND_SERVICE_WITH_UUID_QUERY:
                peripheral->gatt_client_state = P_W4_SERVICE_WITH_UUID_RESULT;
                send_gatt_services_by_uuid_request(peripheral);
                return;
                
            case P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY:
                peripheral->gatt_client_state = P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT;
                send_gatt_characteristic_request(peripheral);
                return;
                
            case P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY:
                peripheral->gatt_client_state = P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT;
                send_gatt_characteristic_request(peripheral);
                return;
                
            case P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY:
                peripheral->gatt_client_state = P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT;
                send_gatt_characteristic_descriptor_request(peripheral);
                return;
                
            case P_W2_SEND_INCLUDED_SERVICE_QUERY:
                peripheral->gatt_client_state = P_W4_INCLUDED_SERVICE_QUERY_RESULT;
                send_gatt_included_service_request(peripheral);
                return;
                
            case P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY:
                peripheral->gatt_client_state = P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT;
                send_gatt_included_service_uuid_request(peripheral);
                return;
                
            case P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY:
                peripheral->gatt_client_state = P_W4_READ_CHARACTERISTIC_VALUE_RESULT;
                send_gatt_read_characteristic_value_request(peripheral);
                return;
                
            case P_W2_SEND_READ_BLOB_QUERY:
                peripheral->gatt_client_state = P_W4_READ_BLOB_RESULT;
                send_gatt_read_blob_request(peripheral);
                return;
                
            case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
                peripheral->gatt_client_state = P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;
                send_gatt_write_attribute_value_request(peripheral);
                return;
                
            case P_W2_PREPARE_WRITE:
                peripheral->gatt_client_state = P_W4_PREPARE_WRITE_RESULT;
                send_gatt_prepare_write_request(peripheral);
                return;
                
            case P_W2_PREPARE_RELIABLE_WRITE:
                peripheral->gatt_client_state = P_W4_PREPARE_RELIABLE_WRITE_RESULT;
                send_gatt_prepare_write_request(peripheral);
                return;
                
            case P_W2_EXECUTE_PREPARED_WRITE:
                peripheral->gatt_client_state = P_W4_EXECUTE_PREPARED_WRITE_RESULT;
                send_gatt_execute_write_request(peripheral);
                return;
                
            case P_W2_CANCEL_PREPARED_WRITE:
                peripheral->gatt_client_state = P_W4_CANCEL_PREPARED_WRITE_RESULT;
                send_gatt_cancel_prepared_write_request(peripheral);
                return;
                
            case P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY:
                peripheral->gatt_client_state = P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT;
                send_gatt_read_client_characteristic_configuration_request(peripheral);
                return;
                
            case P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY:
                peripheral->gatt_client_state = P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT;
                send_gatt_read_characteristic_descriptor_request(peripheral);
                return;
                
            case P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY:
                peripheral->gatt_client_state = P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT;
                send_gatt_read_blob_request(peripheral);
                return;
                
            case P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR:
                peripheral->gatt_client_state = P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
                send_gatt_write_attribute_value_request(peripheral);
                return;
                
            case P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:
                peripheral->gatt_client_state = P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT;
                send_gatt_write_client_characteristic_configuration_request(peripheral);
                return;
                
            case P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR:
                peripheral->gatt_client_state = P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
                send_gatt_prepare_write_request(peripheral);
                return;
                
            case P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR:
                peripheral->gatt_client_state = P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
                send_gatt_execute_write_request(peripheral);
                return;

            case P_W4_CMAC_READY:
                if (sm_cmac_ready()){
                    sm_key_t csrk;
                    le_device_db_csrk_get(peripheral->le_device_index, csrk);
                    uint32_t sign_counter = le_device_db_local_counter_get(peripheral->le_device_index); 
                    peripheral->gatt_client_state = P_W4_CMAC_RESULT;
                    sm_cmac_start(csrk, peripheral->attribute_length, peripheral->attribute_value, sign_counter, att_signed_write_handle_cmac_result);
                }
                return;

            case P_W2_SEND_SIGNED_WRITE: {
                peripheral->gatt_client_state = P_W4_SEND_SINGED_WRITE_DONE;
                // bump local signing counter
                uint32_t sign_counter = le_device_db_local_counter_get(peripheral->le_device_index);
                le_device_db_local_counter_set(peripheral->le_device_index, sign_counter + 1);

                send_gatt_signed_write_request(peripheral, sign_counter);
                peripheral->gatt_client_state = P_READY;
                // finally, notifiy client that write is complete
                gatt_client_handle_transaction_complete(peripheral);
                return;
            }

            default:
                break;
        }
    }
    
}

static void emit_event_to_all_subclients(le_event_t * event){
    linked_list_iterator_t it;    
    linked_list_iterator_init(&it, &gatt_subclients);
    while (linked_list_iterator_has_next(&it)){
        gatt_subclient_t * subclient = (gatt_subclient_t*) linked_list_iterator_next(&it);
        (*subclient->callback)(event);
    } 
}

static void gatt_client_report_error_if_pending(gatt_client_t *peripheral, uint8_t error_code) {
    if (is_ready(peripheral)) return;
    gatt_client_handle_transaction_complete(peripheral);
    emit_gatt_complete_event(peripheral, error_code);
}

static void gatt_client_hci_event_packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    switch (packet[0]) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
        {
            log_info("GATT Client: HCI_EVENT_DISCONNECTION_COMPLETE");
            uint16_t con_handle = READ_BT_16(packet,3);
            gatt_client_t * peripheral = get_gatt_client_context_for_handle(con_handle);
            if (!peripheral) break;
            gatt_client_report_error_if_pending(peripheral, ATT_ERROR_HCI_DISCONNECT_RECEIVED);
            
            linked_list_remove(&gatt_client_connections, (linked_item_t *) peripheral);
            btstack_memory_gatt_client_free(peripheral);

            // Forward event to all subclients
            emit_event_to_all_subclients((le_event_t *) packet);
            break;
        }
        default:
            break;
    }

    // forward all hci events
    emit_event_to_all_subclients((le_event_t *) packet);
}

static void gatt_client_att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type == HCI_EVENT_PACKET) {
        gatt_client_hci_event_packet_handler(packet_type, packet, size);
        gatt_client_run();
        return;
    }

    if (packet_type != ATT_DATA_PACKET) return;
    
    gatt_client_t * peripheral = get_gatt_client_context_for_handle(handle);
    if (!peripheral) return;
    
    switch (packet[0]){
        case ATT_EXCHANGE_MTU_RESPONSE:
        {
            uint16_t remote_rx_mtu = READ_BT_16(packet, 1);
            uint16_t local_rx_mtu = l2cap_max_le_mtu();
            peripheral->mtu = remote_rx_mtu < local_rx_mtu ? remote_rx_mtu : local_rx_mtu;
            peripheral->mtu_state = MTU_EXCHANGED;

            break;
        }
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            switch(peripheral->gatt_client_state){
                case P_W4_SERVICE_QUERY_RESULT:
                    report_gatt_services(peripheral, packet, size);
                    trigger_next_service_query(peripheral, get_last_result_handle(packet, size));
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                default:
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
            switch (peripheral->gatt_client_state){
                case P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT:
                    report_gatt_characteristics(peripheral, packet, size);
                    trigger_next_characteristic_query(peripheral, get_last_result_handle(packet, size));
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:
                    report_gatt_characteristics(peripheral, packet, size);
                    trigger_next_characteristic_query(peripheral, get_last_result_handle(packet, size));
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
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
                        peripheral->gatt_client_state = P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY;
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
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT:
                    peripheral->client_characteristic_configuration_handle = READ_BT_16(packet, 2);
                    peripheral->gatt_client_state = P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
                    break;
                default:
                    break;
            }
            break;
        case ATT_READ_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT: {
                    uint8_t uuid128[16];
                    swap128(&packet[1], uuid128);
                    report_gatt_included_service(peripheral, uuid128, 0);
                    trigger_next_included_service_query(peripheral, peripheral->start_group_handle);
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    report_gatt_characteristic_value(peripheral, peripheral->attribute_handle, &packet[1], size-1);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                    
                case P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT:{
                    gatt_client_handle_transaction_complete(peripheral);
                    report_gatt_characteristic_descriptor(peripheral, peripheral->attribute_handle, &packet[1], size-1, 0, GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
                    emit_gatt_complete_event(peripheral, 0);
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
                service.uuid16 = peripheral->uuid16;
                event.service = service;
                event.handle  = peripheral->handle;
                emit_event(peripheral->subclient_id, (le_event_t*)&event);
            }
            
            trigger_next_service_by_uuid_query(peripheral, service.end_group_handle);
            // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
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
            // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;
        }
            
        case ATT_WRITE_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                case P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                case P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                default:
                    break;
            }
            break;
            
        case ATT_READ_BLOB_RESPONSE:{
            uint16_t received_blob_length = size-1;
            
            switch(peripheral->gatt_client_state){
                case P_W4_READ_BLOB_RESULT:
                    report_gatt_long_characteristic_value_blob(peripheral, &packet[1], received_blob_length, peripheral->attribute_offset);
                    trigger_next_blob_query(peripheral, P_W2_SEND_READ_BLOB_QUERY, received_blob_length);
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                case P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    report_gatt_characteristic_descriptor(peripheral, peripheral->attribute_handle,
                                                          &packet[1], received_blob_length,
                                                          peripheral->attribute_offset, GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
                    trigger_next_blob_query(peripheral, P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY, received_blob_length);
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                default:
                    break;
            }
            break;
        }
        case ATT_PREPARE_WRITE_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_PREPARE_WRITE_RESULT:{
                    peripheral->attribute_offset = READ_BT_16(packet, 3);
                    trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:{
                    peripheral->attribute_offset = READ_BT_16(packet, 3);
                    trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR, P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR);
                    // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_PREPARE_RELIABLE_WRITE_RESULT:{
                    if (is_value_valid(peripheral, packet, size)){
                        peripheral->attribute_offset = READ_BT_16(packet, 3);
                        trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_RELIABLE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                        // GATT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                        break;
                    }
                    peripheral->gatt_client_state = P_W2_CANCEL_PREPARED_WRITE;
                    break;
                }
                default:
                    break;
            }
            break;
            
        case ATT_EXECUTE_WRITE_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_EXECUTE_PREPARED_WRITE_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                case P_W4_CANCEL_PREPARED_WRITE_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                case P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                default:
                    break;
                    
            }
            break;

        case ATT_ERROR_RESPONSE:

            switch (packet[4]){
                case ATT_ERROR_ATTRIBUTE_NOT_FOUND: {
                    switch(peripheral->gatt_client_state){
                        case P_W4_SERVICE_QUERY_RESULT:
                        case P_W4_SERVICE_WITH_UUID_RESULT:
                        case P_W4_INCLUDED_SERVICE_QUERY_RESULT:
                        case P_W4_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
                            gatt_client_handle_transaction_complete(peripheral);
                            emit_gatt_complete_event(peripheral, 0);
                            break;
                        case P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT:
                        case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:
                            characteristic_end_found(peripheral, peripheral->end_group_handle);
                            gatt_client_handle_transaction_complete(peripheral);
                            emit_gatt_complete_event(peripheral, 0);
                            break;
                        default:
                            gatt_client_report_error_if_pending(peripheral, packet[4]);
                            break;
                    }
                    break;
                }
                default:                
                    gatt_client_report_error_if_pending(peripheral, packet[4]);
                    break;
            }
            break;
            
        default:
            log_info("ATT Handler, unhandled response type 0x%02x", packet[0]);
            break;
    }
    gatt_client_run();
}

static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    linked_list_iterator_t it;
    linked_list_iterator_init(&it, &gatt_client_connections);
    while (linked_list_iterator_has_next(&it)){
        gatt_client_t * peripheral = (gatt_client_t *) linked_list_iterator_next(&it);
        if (peripheral->gatt_client_state == P_W4_CMAC_RESULT){
            // store result
            memcpy(peripheral->cmac, hash, 8);
            peripheral->gatt_client_state = P_W2_SEND_SIGNED_WRITE;
            gatt_client_run();
            return;
        }
    }
}

le_command_status_t gatt_client_signed_write_without_response(uint16_t gatt_client_id, uint16_t con_handle, uint16_t handle, uint16_t message_len, uint8_t * message){
    gatt_client_t * peripheral = provide_context_for_conn_handle(con_handle);
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    peripheral->le_device_index = sm_le_device_index(con_handle);
    if (peripheral->le_device_index < 0) return BLE_PERIPHERAL_IN_WRONG_STATE; // device lookup not done / no stored bonding information

    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = handle;
    peripheral->attribute_length = message_len;
    peripheral->attribute_value = message;
    peripheral->gatt_client_state = P_W4_CMAC_READY;

    gatt_client_run();
    return BLE_PERIPHERAL_OK; 
}

le_command_status_t gatt_client_discover_primary_services(uint16_t gatt_client_id, uint16_t con_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;

    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->gatt_client_state = P_W2_SEND_SERVICE_QUERY;
    peripheral->uuid16 = 0;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}


le_command_status_t gatt_client_discover_primary_services_by_uuid16(uint16_t gatt_client_id, uint16_t con_handle, uint16_t uuid16){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;

    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->gatt_client_state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    peripheral->uuid16 = uuid16;
    sdp_normalize_uuid((uint8_t*) &(peripheral->uuid128), peripheral->uuid16);
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_discover_primary_services_by_uuid128(uint16_t gatt_client_id, uint16_t con_handle, const uint8_t * uuid128){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;

    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->gatt_client_state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_discover_characteristics_for_service(uint16_t gatt_client_id, uint16_t con_handle, le_service_t *service){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;

    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = service->start_group_handle;
    peripheral->end_group_handle   = service->end_group_handle;
    peripheral->filter_with_uuid = 0;
    peripheral->characteristic_start_handle = 0;
    peripheral->gatt_client_state = P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_find_included_services_for_service(uint16_t gatt_client_id, uint16_t con_handle, le_service_t *service){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = service->start_group_handle;
    peripheral->end_group_handle   = service->end_group_handle;
    peripheral->gatt_client_state = P_W2_SEND_INCLUDED_SERVICE_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(uint16_t gatt_client_id, uint16_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle   = end_handle;
    peripheral->filter_with_uuid = 1;
    peripheral->uuid16 = uuid16;
    sdp_normalize_uuid((uint8_t*) &(peripheral->uuid128), uuid16);
    peripheral->characteristic_start_handle = 0;
    peripheral->gatt_client_state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_discover_characteristics_for_handle_range_by_uuid128(uint16_t gatt_client_id, uint16_t con_handle, uint16_t start_handle, uint16_t end_handle, uint8_t * uuid128){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle   = end_handle;
    peripheral->filter_with_uuid = 1;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->characteristic_start_handle = 0;
    peripheral->gatt_client_state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}


le_command_status_t gatt_client_discover_characteristics_for_service_by_uuid16(uint16_t gatt_client_id, uint16_t handle, le_service_t *service, uint16_t  uuid16){
    return gatt_client_discover_characteristics_for_handle_range_by_uuid16(gatt_client_id, handle, service->start_group_handle, service->end_group_handle, uuid16);
}

le_command_status_t gatt_client_discover_characteristics_for_service_by_uuid128(uint16_t gatt_client_id, uint16_t handle, le_service_t *service, uint8_t * uuid128){
    return gatt_client_discover_characteristics_for_handle_range_by_uuid128(gatt_client_id, handle, service->start_group_handle, service->end_group_handle, uuid128);
}

le_command_status_t gatt_client_discover_characteristic_descriptors(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_t *characteristic){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    if (characteristic->value_handle == characteristic->end_handle){
        emit_gatt_complete_event(peripheral, 0);
        return BLE_PERIPHERAL_OK;
    }
    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = characteristic->value_handle + 1;
    peripheral->end_group_handle   = characteristic->end_handle;
    peripheral->gatt_client_state = P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY;
    
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_read_value_of_characteristic_using_value_handle(uint16_t gatt_client_id, uint16_t con_handle, uint16_t value_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_offset = 0;
    peripheral->gatt_client_state = P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_read_value_of_characteristic(uint16_t gatt_client_id, uint16_t handle, le_characteristic_t *characteristic){
    return gatt_client_read_value_of_characteristic_using_value_handle(gatt_client_id, handle, characteristic->value_handle);
}


le_command_status_t gatt_client_read_long_value_of_characteristic_using_value_handle(uint16_t gatt_client_id, uint16_t con_handle, uint16_t value_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_offset = 0;
    peripheral->gatt_client_state = P_W2_SEND_READ_BLOB_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_read_long_value_of_characteristic(uint16_t gatt_client_id, uint16_t handle, le_characteristic_t *characteristic){
    return gatt_client_read_long_value_of_characteristic_using_value_handle(gatt_client_id, handle, characteristic->value_handle);
}

le_command_status_t gatt_client_write_value_of_characteristic_without_response(uint16_t gatt_client_id, uint16_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    if (value_length >= peripheral_mtu(peripheral) - 3) return BLE_VALUE_TOO_LONG;
    if (!l2cap_can_send_fixed_channel_packet_now(peripheral->handle)) return BLE_PERIPHERAL_BUSY;

    peripheral->subclient_id = gatt_client_id;
    att_write_request(ATT_WRITE_COMMAND, peripheral->handle, value_handle, value_length, value);
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_write_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_value = value;
    peripheral->gatt_client_state = P_W2_SEND_WRITE_CHARACTERISTIC_VALUE;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_write_long_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->gatt_client_state = P_W2_PREPARE_WRITE;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_reliable_write_long_value_of_characteristic(uint16_t gatt_client_id, uint16_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->gatt_client_state = P_W2_PREPARE_RELIABLE_WRITE;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_write_client_characteristic_configuration(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_t * characteristic, uint16_t configuration){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) &&
        (characteristic->properties & ATT_PROPERTY_NOTIFY) == 0) {
        log_info("le_central_write_client_characteristic_configuration: BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED");
        return BLE_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED;
    } else if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION) &&
               (characteristic->properties & ATT_PROPERTY_INDICATE) == 0){
        log_info("le_central_write_client_characteristic_configuration: BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED");
        return BLE_CHARACTERISTIC_INDICATION_NOT_SUPPORTED;
    }
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->start_group_handle = characteristic->value_handle;
    peripheral->end_group_handle = characteristic->end_handle;
    bt_store_16(peripheral->client_characteristic_configuration_value, 0, configuration);
    
    peripheral->gatt_client_state = P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_read_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t * descriptor){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = descriptor->handle;
    peripheral->uuid16 = descriptor->uuid16;
    if (!descriptor->uuid16){
        memcpy(peripheral->uuid128, descriptor->uuid128, 16);
    }
    
    peripheral->gatt_client_state = P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_read_long_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t * descriptor){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = descriptor->handle;
    peripheral->attribute_offset = 0;
    peripheral->gatt_client_state = P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_write_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = descriptor->handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->gatt_client_state = P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}

le_command_status_t gatt_client_write_long_characteristic_descriptor(uint16_t gatt_client_id, uint16_t con_handle, le_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return (le_command_status_t) BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return BLE_PERIPHERAL_IN_WRONG_STATE;
    
    peripheral->subclient_id = gatt_client_id;
    peripheral->attribute_handle = descriptor->handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->gatt_client_state = P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return BLE_PERIPHERAL_OK;
}


