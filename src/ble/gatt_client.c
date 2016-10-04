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

#include "btstack_config.h"

#include "att_dispatch.h"
#include "ad_parser.h"
#include "ble/att_db.h"
#include "ble/core.h"
#include "ble/gatt_client.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"

static btstack_linked_list_t gatt_client_connections;
static btstack_linked_list_t gatt_client_value_listeners;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t  pts_suppress_mtu_exchange;

static void gatt_client_att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size);
static void gatt_client_hci_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void gatt_client_report_error_if_pending(gatt_client_t *peripheral, uint8_t error_code);
static void att_signed_write_handle_cmac_result(uint8_t hash[8]);

static uint16_t peripheral_mtu(gatt_client_t *peripheral){
    if (peripheral->mtu > l2cap_max_le_mtu()){
        log_error("Peripheral mtu is not initialized");
        return l2cap_max_le_mtu();
    }
    return peripheral->mtu;
}

void gatt_client_init(void){
    gatt_client_connections = NULL;
    pts_suppress_mtu_exchange = 0;

    // regsister for HCI Events
    hci_event_callback_registration.callback = &gatt_client_hci_event_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // and ATT Client PDUs
    att_dispatch_register_client(gatt_client_att_packet_handler);
}

static gatt_client_t * gatt_client_for_timer(btstack_timer_source_t * ts){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &gatt_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_client_t * peripheral = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        if ( &peripheral->gc_timeout == ts) {
            return peripheral;
        }
    }
    return NULL;
}

static void gatt_client_timeout_handler(btstack_timer_source_t * timer){
    gatt_client_t * peripheral = gatt_client_for_timer(timer);
    if (!peripheral) return;
    log_info("GATT client timeout handle, handle 0x%02x", peripheral->con_handle);
    gatt_client_report_error_if_pending(peripheral, ATT_ERROR_TIMEOUT);           
}

static void gatt_client_timeout_start(gatt_client_t * peripheral){
    log_info("GATT client timeout start, handle 0x%02x", peripheral->con_handle);
    btstack_run_loop_remove_timer(&peripheral->gc_timeout);
    btstack_run_loop_set_timer_handler(&peripheral->gc_timeout, gatt_client_timeout_handler);
    btstack_run_loop_set_timer(&peripheral->gc_timeout, 30000); // 30 seconds sm timeout
    btstack_run_loop_add_timer(&peripheral->gc_timeout);
}

static void gatt_client_timeout_stop(gatt_client_t * peripheral){
    log_info("GATT client timeout stop, handle 0x%02x", peripheral->con_handle);
    btstack_run_loop_remove_timer(&peripheral->gc_timeout);
}

static gatt_client_t * get_gatt_client_context_for_handle(uint16_t handle){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) gatt_client_connections; it ; it = it->next){
        gatt_client_t * peripheral = (gatt_client_t *) it;
        if (peripheral->con_handle == handle){
            return peripheral;
        }
    }
    return NULL;
}


// @returns context
// returns existing one, or tries to setup new one
static gatt_client_t * provide_context_for_conn_handle(hci_con_handle_t con_handle){
    gatt_client_t * context = get_gatt_client_context_for_handle(con_handle);
    if (context) return  context;

    context = btstack_memory_gatt_client_get();
    if (!context) return NULL;
    // init state
    memset(context, 0, sizeof(gatt_client_t));
    context->con_handle = con_handle;
    context->mtu = ATT_DEFAULT_MTU;
    context->mtu_state = SEND_MTU_EXCHANGE;
    context->gatt_client_state = P_READY;
    btstack_linked_list_add(&gatt_client_connections, (btstack_linked_item_t*)context);

    // skip mtu exchange for testing sm with pts
    if (pts_suppress_mtu_exchange){
         context->mtu_state = MTU_EXCHANGED;
    }
    return context;
}

static gatt_client_t * provide_context_for_conn_handle_and_start_timer(hci_con_handle_t con_handle){
    gatt_client_t * context = provide_context_for_conn_handle(con_handle);
    if (!context) return NULL;
    gatt_client_timeout_start(context);
    return context;
}

static int is_ready(gatt_client_t * context){
    return context->gatt_client_state == P_READY;
}

int gatt_client_is_ready(hci_con_handle_t con_handle){
    gatt_client_t * context = provide_context_for_conn_handle(con_handle);
    if (!context) return 0;
    return is_ready(context);
}

uint8_t gatt_client_get_mtu(hci_con_handle_t con_handle, uint16_t * mtu){
    gatt_client_t * context = provide_context_for_conn_handle(con_handle);
    if (context && context->mtu_state == MTU_EXCHANGED){
        *mtu = context->mtu;
        return 0;
    } 
    *mtu = ATT_DEFAULT_MTU;
    return GATT_CLIENT_IN_WRONG_STATE;
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
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
}

// precondition: can_send_packet_now == TRUE
static void att_find_by_type_value_request(uint16_t request_type, uint16_t attribute_group_type, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle, uint8_t * value, uint16_t value_size){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    
    request[0] = request_type;
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    little_endian_store_16(request, 5, attribute_group_type);
    memcpy(&request[7], value, value_size);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 7+value_size);
}

// precondition: can_send_packet_now == TRUE
static void att_read_by_type_or_group_request_for_uuid16(uint16_t request_type, uint16_t uuid16, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    little_endian_store_16(request, 5, uuid16);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 7);
}

// precondition: can_send_packet_now == TRUE
static void att_read_by_type_or_group_request_for_uuid128(uint16_t request_type, uint8_t * uuid128, uint16_t peripheral_handle, uint16_t start_handle, uint16_t end_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    reverse_128(uuid128, &request[5]);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 21);
}

// precondition: can_send_packet_now == TRUE
static void att_read_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3);
}

// precondition: can_send_packet_now == TRUE
static void att_read_blob_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_offset){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    little_endian_store_16(request, 3, value_offset);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5);
}

static void att_read_multiple_request(uint16_t peripheral_handle, uint16_t num_value_handles, uint16_t * value_handles){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = ATT_READ_MULTIPLE_REQUEST;
    int i;
    int offset = 1;
    for (i=0;i<num_value_handles;i++){
        little_endian_store_16(request, offset, value_handles[i]);
        offset += 2;
    }
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, offset);
}

// precondition: can_send_packet_now == TRUE
static void att_signed_write_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_length, uint8_t * value, uint32_t sign_counter, uint8_t sgn[8]){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    memcpy(&request[3], value, value_length);
    little_endian_store_32(request, 3 + value_length, sign_counter);
    reverse_64(sgn, &request[3 + value_length + 4]);
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 3 + value_length + 12);
}

// precondition: can_send_packet_now == TRUE
static void att_write_request(uint16_t request_type, uint16_t peripheral_handle, uint16_t attribute_handle, uint16_t value_length, uint8_t * value){
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
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
    little_endian_store_16(request, 1, attribute_handle);
    little_endian_store_16(request, 3, value_offset);
    memcpy(&request[5], &value[value_offset], blob_length);
    
    l2cap_send_prepared_connectionless(peripheral_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, 5+blob_length);
}

static void att_exchange_mtu_request(uint16_t peripheral_handle){
    uint16_t mtu = l2cap_max_le_mtu();
    l2cap_reserve_packet_buffer();
    uint8_t * request = l2cap_get_outgoing_buffer();
    request[0] = ATT_EXCHANGE_MTU_REQUEST;
    little_endian_store_16(request, 1, mtu);
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
    att_read_by_type_or_group_request_for_uuid16(ATT_READ_BY_GROUP_TYPE_REQUEST, GATT_PRIMARY_SERVICE_UUID, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_by_uuid_request(gatt_client_t *peripheral, uint16_t attribute_group_type){
    if (peripheral->uuid16){
        uint8_t uuid16[2];
        little_endian_store_16(uuid16, 0, peripheral->uuid16);
        att_find_by_type_value_request(ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle, uuid16, 2);
        return;
    }
    uint8_t uuid128[16];
    reverse_128(peripheral->uuid128, uuid128);
    att_find_by_type_value_request(ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle, uuid128, 16);
}

static void send_gatt_services_by_uuid_request(gatt_client_t *peripheral){
    send_gatt_by_uuid_request(peripheral, GATT_PRIMARY_SERVICE_UUID);
}

static void send_gatt_included_service_uuid_request(gatt_client_t *peripheral){
    att_read_request(ATT_READ_REQUEST, peripheral->con_handle, peripheral->query_start_handle);
}

static void send_gatt_included_service_request(gatt_client_t *peripheral){
    att_read_by_type_or_group_request_for_uuid16(ATT_READ_BY_TYPE_REQUEST, GATT_INCLUDE_SERVICE_UUID, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_characteristic_request(gatt_client_t *peripheral){
    att_read_by_type_or_group_request_for_uuid16(ATT_READ_BY_TYPE_REQUEST, GATT_CHARACTERISTICS_UUID, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_characteristic_descriptor_request(gatt_client_t *peripheral){
    att_find_information_request(ATT_FIND_INFORMATION_REQUEST, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_read_characteristic_value_request(gatt_client_t *peripheral){
    att_read_request(ATT_READ_REQUEST, peripheral->con_handle, peripheral->attribute_handle);
}

static void send_gatt_read_by_type_request(gatt_client_t * peripheral){
    if (peripheral->uuid16){
        att_read_by_type_or_group_request_for_uuid16(ATT_READ_BY_TYPE_REQUEST, peripheral->uuid16, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
    } else {
        att_read_by_type_or_group_request_for_uuid128(ATT_READ_BY_TYPE_REQUEST, peripheral->uuid128, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
    }
}

static void send_gatt_read_blob_request(gatt_client_t *peripheral){
    att_read_blob_request(ATT_READ_BLOB_REQUEST, peripheral->con_handle, peripheral->attribute_handle, peripheral->attribute_offset);
}

static void send_gatt_read_multiple_request(gatt_client_t * peripheral){
    att_read_multiple_request(peripheral->con_handle, peripheral->read_multiple_handle_count, peripheral->read_multiple_handles);
}

static void send_gatt_write_attribute_value_request(gatt_client_t * peripheral){
    att_write_request(ATT_WRITE_REQUEST, peripheral->con_handle, peripheral->attribute_handle, peripheral->attribute_length, peripheral->attribute_value);
}

static void send_gatt_write_client_characteristic_configuration_request(gatt_client_t * peripheral){
    att_write_request(ATT_WRITE_REQUEST, peripheral->con_handle, peripheral->client_characteristic_configuration_handle, 2, peripheral->client_characteristic_configuration_value);
}

static void send_gatt_prepare_write_request(gatt_client_t * peripheral){
    att_prepare_write_request(ATT_PREPARE_WRITE_REQUEST, peripheral->con_handle, peripheral->attribute_handle, peripheral->attribute_offset, write_blob_length(peripheral), peripheral->attribute_value);
}

static void send_gatt_execute_write_request(gatt_client_t * peripheral){
    att_execute_write_request(ATT_EXECUTE_WRITE_REQUEST, peripheral->con_handle, 1);
}

static void send_gatt_cancel_prepared_write_request(gatt_client_t * peripheral){
    att_execute_write_request(ATT_EXECUTE_WRITE_REQUEST, peripheral->con_handle, 0);
}

static void send_gatt_read_client_characteristic_configuration_request(gatt_client_t * peripheral){
    att_read_by_type_or_group_request_for_uuid16(ATT_READ_BY_TYPE_REQUEST, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION, peripheral->con_handle, peripheral->start_group_handle, peripheral->end_group_handle);
}

static void send_gatt_read_characteristic_descriptor_request(gatt_client_t * peripheral){
    att_read_request(ATT_READ_REQUEST, peripheral->con_handle, peripheral->attribute_handle);
}

static void send_gatt_signed_write_request(gatt_client_t * peripheral, uint32_t sign_counter){
    att_signed_write_request(ATT_SIGNED_WRITE_COMMAND, peripheral->con_handle, peripheral->attribute_handle, peripheral->attribute_length, peripheral->attribute_value, sign_counter, peripheral->cmac);
}

static uint16_t get_last_result_handle_from_service_list(uint8_t * packet, uint16_t size){
    uint8_t attr_length = packet[1];
    return little_endian_read_16(packet, size - attr_length + 2);
}

static uint16_t get_last_result_handle_from_characteristics_list(uint8_t * packet, uint16_t size){
    uint8_t attr_length = packet[1];
    return little_endian_read_16(packet, size - attr_length + 3);
}

static uint16_t get_last_result_handle_from_included_services_list(uint8_t * packet, uint16_t size){
    uint8_t attr_length = packet[1];
    return little_endian_read_16(packet, size - attr_length);
}

static void gatt_client_handle_transaction_complete(gatt_client_t * peripheral){
    peripheral->gatt_client_state = P_READY;
    gatt_client_timeout_stop(peripheral);
}

static void emit_event_new(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size){
    if (!callback) return;
    hci_dump_packet(HCI_EVENT_PACKET, 0, packet, size);
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

void gatt_client_listen_for_characteristic_value_updates(gatt_client_notification_t * notification, btstack_packet_handler_t packet_handler, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    notification->callback = packet_handler;
    notification->con_handle = con_handle;
    notification->attribute_handle = characteristic->value_handle;
    btstack_linked_list_add(&gatt_client_value_listeners, (btstack_linked_item_t*) notification);
}

static void emit_event_to_registered_listeners(hci_con_handle_t con_handle, uint16_t attribute_handle, uint8_t * packet, uint16_t size){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &gatt_client_value_listeners);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_client_notification_t * notification = (gatt_client_notification_t*) btstack_linked_list_iterator_next(&it);
        if (notification->con_handle != con_handle) continue;
        if (notification->attribute_handle != attribute_handle) continue;
        (*notification->callback)(HCI_EVENT_PACKET, 0, packet, size);
    } 
}

static void emit_gatt_complete_event(gatt_client_t * peripheral, uint8_t status){
    // @format H1
    uint8_t packet[5];
    packet[0] = GATT_EVENT_QUERY_COMPLETE;
    packet[1] = 3;
    little_endian_store_16(packet, 2, peripheral->con_handle);
    packet[4] = status;
    emit_event_new(peripheral->callback, packet, sizeof(packet));
}

static void emit_gatt_service_query_result_event(gatt_client_t * peripheral, uint16_t start_group_handle, uint16_t end_group_handle, uint8_t * uuid128){
    // @format HX
    uint8_t packet[24];
    packet[0] = GATT_EVENT_SERVICE_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2;
    little_endian_store_16(packet, 2, peripheral->con_handle);
    ///
    little_endian_store_16(packet, 4, start_group_handle);
    little_endian_store_16(packet, 6, end_group_handle);
    reverse_128(uuid128, &packet[8]);
    emit_event_new(peripheral->callback, packet, sizeof(packet));
}

static void emit_gatt_included_service_query_result_event(gatt_client_t * peripheral, uint16_t include_handle, uint16_t start_group_handle, uint16_t end_group_handle, uint8_t * uuid128){
    // @format HX
    uint8_t packet[26];
    packet[0] = GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2;
    little_endian_store_16(packet, 2, peripheral->con_handle);
    ///
    little_endian_store_16(packet, 4, include_handle);
    //
    little_endian_store_16(packet, 6, start_group_handle);
    little_endian_store_16(packet, 8, end_group_handle);
    reverse_128(uuid128, &packet[10]);
    emit_event_new(peripheral->callback, packet, sizeof(packet));
}

static void emit_gatt_characteristic_query_result_event(gatt_client_t * peripheral, uint16_t start_handle, uint16_t value_handle, uint16_t end_handle,
        uint16_t properties, uint8_t * uuid128){
    // @format HY
    uint8_t packet[28];
    packet[0] = GATT_EVENT_CHARACTERISTIC_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2;
    little_endian_store_16(packet, 2, peripheral->con_handle);
    ///
    little_endian_store_16(packet, 4,  start_handle);
    little_endian_store_16(packet, 6,  value_handle);
    little_endian_store_16(packet, 8,  end_handle);
    little_endian_store_16(packet, 10, properties);
    reverse_128(uuid128, &packet[12]);
    emit_event_new(peripheral->callback, packet, sizeof(packet));
}

static void emit_gatt_all_characteristic_descriptors_result_event(
    gatt_client_t * peripheral, uint16_t descriptor_handle, uint8_t * uuid128){
    // @format HZ
    uint8_t packet[22];
    packet[0] = GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT;
    packet[1] = sizeof(packet) - 2;
    little_endian_store_16(packet, 2, peripheral->con_handle);
    ///
    little_endian_store_16(packet, 4,  descriptor_handle);
    reverse_128(uuid128, &packet[6]);
    emit_event_new(peripheral->callback, packet, sizeof(packet));
}
///

static void report_gatt_services(gatt_client_t * peripheral, uint8_t * packet,  uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 4;
    
    int i;
    for (i = 2; i < size; i += attr_length){
        uint16_t start_group_handle = little_endian_read_16(packet,i);
        uint16_t end_group_handle   = little_endian_read_16(packet,i+2);
        uint8_t  uuid128[16];
        uint16_t uuid16 = 0;

        if (uuid_length == 2){
            uuid16 = little_endian_read_16(packet, i+4);
            uuid_add_bluetooth_prefix((uint8_t*) &uuid128, uuid16);
        } else {
            reverse_128(&packet[i+4], uuid128);
        }
        emit_gatt_service_query_result_event(peripheral, start_group_handle, end_group_handle, uuid128);
    }
    // log_info("report_gatt_services for %02X done", peripheral->con_handle);
}

// helper
static void characteristic_start_found(gatt_client_t * peripheral, uint16_t start_handle, uint8_t properties, uint16_t value_handle, uint8_t * uuid, uint16_t uuid_length){
    uint8_t uuid128[16];
    uint16_t uuid16 = 0;
    if (uuid_length == 2){
        uuid16 = little_endian_read_16(uuid, 0);
        uuid_add_bluetooth_prefix((uint8_t*) uuid128, uuid16);
    } else {
        reverse_128(uuid, uuid128);
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

    emit_gatt_characteristic_query_result_event(peripheral, peripheral->characteristic_start_handle, peripheral->attribute_handle,
        end_handle, peripheral->characteristic_properties, peripheral->uuid128);    

    peripheral->characteristic_start_handle = 0;
}

static void report_gatt_characteristics(gatt_client_t * peripheral, uint8_t * packet,  uint16_t size){
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 5;
    int i;
    for (i = 2; i < size; i += attr_length){
        uint16_t start_handle = little_endian_read_16(packet, i);
        uint8_t  properties = packet[i+2];
        uint16_t value_handle = little_endian_read_16(packet, i+3);
        characteristic_end_found(peripheral, start_handle-1);
        characteristic_start_found(peripheral, start_handle, properties, value_handle, &packet[i+5], uuid_length);
    }
}

static void report_gatt_included_service_uuid16(gatt_client_t * peripheral, uint16_t include_handle, uint16_t uuid16){
    uint8_t normalized_uuid128[16];
    uuid_add_bluetooth_prefix(normalized_uuid128, uuid16);
    emit_gatt_included_service_query_result_event(peripheral, include_handle, peripheral->query_start_handle,
        peripheral->query_end_handle, normalized_uuid128);
}

static void report_gatt_included_service_uuid128(gatt_client_t * peripheral, uint16_t include_handle, uint8_t *uuid128){
    emit_gatt_included_service_query_result_event(peripheral, include_handle, peripheral->query_start_handle,
        peripheral->query_end_handle, uuid128);
}

// @returns packet pointer
// @note assume that value is part of an l2cap buffer - overwrite HCI + L2CAP packet headers
static const int characteristic_value_event_header_size = 8;
static uint8_t * setup_characteristic_value_packet(uint8_t type, hci_con_handle_t con_handle, uint16_t attribute_handle, uint8_t * value, uint16_t length){
    // before the value inside the ATT PDU
    uint8_t * packet = value - characteristic_value_event_header_size;
    packet[0] = type;
    packet[1] = characteristic_value_event_header_size - 2 + length;
    little_endian_store_16(packet, 2, con_handle);
    little_endian_store_16(packet, 4, attribute_handle);
    little_endian_store_16(packet, 6, length);
    return packet;
}

// @returns packet pointer
// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static const int long_characteristic_value_event_header_size = 10;
static uint8_t * setup_long_characteristic_value_packet(uint8_t type, hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * value, uint16_t length){
#if defined(HCI_INCOMING_PRE_BUFFER_SIZE) && (HCI_INCOMING_PRE_BUFFER_SIZE >= 10 - 8) // L2CAP Header (4) - ACL Header (4)
    // before the value inside the ATT PDU
    uint8_t * packet = value - long_characteristic_value_event_header_size;
    packet[0] = type;
    packet[1] = long_characteristic_value_event_header_size - 2 + length;
    little_endian_store_16(packet, 2, con_handle);
    little_endian_store_16(packet, 4, attribute_handle);
    little_endian_store_16(packet, 6, offset);
    little_endian_store_16(packet, 8, length);
    return packet;
#else 
    log_error("HCI_INCOMING_PRE_BUFFER_SIZE >= 2 required for long characteristic reads");
    return NULL;
#endif
}


// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_notification(hci_con_handle_t con_handle, uint16_t value_handle, uint8_t * value, int length){
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_NOTIFICATION, con_handle, value_handle, value, length);
    emit_event_to_registered_listeners(con_handle, value_handle, packet, characteristic_value_event_header_size + length);
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_indication(hci_con_handle_t con_handle, uint16_t value_handle, uint8_t * value, int length){
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_INDICATION, con_handle, value_handle, value, length);
    emit_event_to_registered_listeners(con_handle, value_handle, packet, characteristic_value_event_header_size + length);
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_characteristic_value(gatt_client_t * peripheral, uint16_t attribute_handle, uint8_t * value, uint16_t length){
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT, peripheral->con_handle, attribute_handle, value, length);
    emit_event_new(peripheral->callback, packet, characteristic_value_event_header_size + length);
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_long_characteristic_value_blob(gatt_client_t * peripheral, uint16_t attribute_handle, uint8_t * blob, uint16_t blob_length, int value_offset){
    uint8_t * packet = setup_long_characteristic_value_packet(GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT, peripheral->con_handle, attribute_handle, value_offset, blob, blob_length);
    if (!packet) return;
    emit_event_new(peripheral->callback, packet, blob_length + long_characteristic_value_event_header_size);
}

static void report_gatt_characteristic_descriptor(gatt_client_t * peripheral, uint16_t descriptor_handle, uint8_t *value, uint16_t value_length, uint16_t value_offset){
    uint8_t * packet = setup_characteristic_value_packet(GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT, peripheral->con_handle, descriptor_handle, value, value_length);
    emit_event_new(peripheral->callback, packet, value_length + 8);
}

static void report_gatt_long_characteristic_descriptor(gatt_client_t * peripheral, uint16_t descriptor_handle, uint8_t *blob, uint16_t blob_length, uint16_t value_offset){
    uint8_t * packet = setup_long_characteristic_value_packet(GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT, peripheral->con_handle, descriptor_handle, value_offset, blob, blob_length);
    if (!packet) return;
    emit_event_new(peripheral->callback, packet, blob_length + long_characteristic_value_event_header_size);
}

static void report_gatt_all_characteristic_descriptors(gatt_client_t * peripheral, uint8_t * packet, uint16_t size, uint16_t pair_size){
    int i;
    for (i = 0; i<size; i+=pair_size){
        uint16_t descriptor_handle = little_endian_read_16(packet,i);
        uint8_t uuid128[16];
        uint16_t uuid16 = 0;
        if (pair_size == 4){
            uuid16 = little_endian_read_16(packet,i+2);
            uuid_add_bluetooth_prefix(uuid128, uuid16);
        } else {
            reverse_128(&packet[i+2], uuid128);
        }        
        emit_gatt_all_characteristic_descriptors_result_event(peripheral, descriptor_handle, uuid128);
    }
    
}

static int is_query_done(gatt_client_t * peripheral, uint16_t last_result_handle){
    return last_result_handle >= peripheral->end_group_handle;
}

static void trigger_next_query(gatt_client_t * peripheral, uint16_t last_result_handle, gatt_client_state_t next_query_state){
    if (is_query_done(peripheral, last_result_handle)){
        gatt_client_handle_transaction_complete(peripheral);
        emit_gatt_complete_event(peripheral, 0);
        return;
    }
    // next
    peripheral->start_group_handle = last_result_handle + 1;
    peripheral->gatt_client_state = next_query_state;
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
    if (is_query_done(peripheral, last_result_handle)){
        // report last characteristic
        characteristic_end_found(peripheral, peripheral->end_group_handle);
    }
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY);
}

static inline void trigger_next_characteristic_descriptor_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY);
}

static inline void trigger_next_read_by_type_query(gatt_client_t * peripheral, uint16_t last_result_handle){
    trigger_next_query(peripheral, last_result_handle, P_W2_SEND_READ_BY_TYPE_REQUEST);
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
    uint16_t attribute_handle = little_endian_read_16(packet, 1);
    uint16_t value_offset = little_endian_read_16(packet, 3);
    
    if (peripheral->attribute_handle != attribute_handle) return 0;
    if (peripheral->attribute_offset != value_offset) return 0;
    return memcmp(&peripheral->attribute_value[peripheral->attribute_offset], &packet[5], size-5) == 0;
}


static void gatt_client_run(void){

    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) gatt_client_connections; it ; it = it->next){

        gatt_client_t * peripheral = (gatt_client_t *) it;

        if (!att_dispatch_client_can_send_now(peripheral->con_handle)) {
            att_dispatch_client_request_can_send_now_event(peripheral->con_handle);
            return;
        }

        // log_info("- handle_peripheral_list, mtu state %u, client state %u", peripheral->mtu_state, peripheral->gatt_client_state);
        
        switch (peripheral->mtu_state) {
            case SEND_MTU_EXCHANGE:{
                peripheral->mtu_state = SENT_MTU_EXCHANGE;
                att_exchange_mtu_request(peripheral->con_handle);
                return;
            }
            case SENT_MTU_EXCHANGE:
                return;
            default:
                break;
        }
        
        if (peripheral->send_confirmation){
            peripheral->send_confirmation = 0;
            att_confirmation(peripheral->con_handle);
            return;
        }
        
        // check MTU for writes
        switch (peripheral->gatt_client_state){
            case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
            case P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR:
                if (peripheral->attribute_length <= peripheral_mtu(peripheral) - 3) break;
                log_error("gatt_client_run: value len %u > MTU %u - 3\n", peripheral->attribute_length, peripheral_mtu(peripheral));
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
                
            case P_W2_SEND_READ_BY_TYPE_REQUEST:
                peripheral->gatt_client_state = P_W4_READ_BY_TYPE_RESPONSE;
                send_gatt_read_by_type_request(peripheral);
                break;

            case P_W2_SEND_READ_MULTIPLE_REQUEST:
                peripheral->gatt_client_state = P_W4_READ_MULTIPLE_RESPONSE;
                send_gatt_read_multiple_request(peripheral);
                break;

            case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
                peripheral->gatt_client_state = P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;
                send_gatt_write_attribute_value_request(peripheral);
                return;
                
            case P_W2_PREPARE_WRITE:
                peripheral->gatt_client_state = P_W4_PREPARE_WRITE_RESULT;
                send_gatt_prepare_write_request(peripheral);
                return;
                
            case P_W2_PREPARE_WRITE_SINGLE:
                peripheral->gatt_client_state = P_W4_PREPARE_WRITE_SINGLE_RESULT;
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
                
            case P_W2_CANCEL_PREPARED_WRITE_DATA_MISMATCH:
                peripheral->gatt_client_state = P_W4_CANCEL_PREPARED_WRITE_DATA_MISMATCH_RESULT;
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
                    le_device_db_local_csrk_get(peripheral->le_device_index, csrk);
                    uint32_t sign_counter = le_device_db_local_counter_get(peripheral->le_device_index); 
                    peripheral->gatt_client_state = P_W4_CMAC_RESULT;
                    sm_cmac_signed_write_start(csrk, ATT_SIGNED_WRITE_COMMAND, peripheral->attribute_handle, peripheral->attribute_length, peripheral->attribute_value, sign_counter, att_signed_write_handle_cmac_result);
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

static void gatt_client_report_error_if_pending(gatt_client_t *peripheral, uint8_t error_code) {
    if (is_ready(peripheral)) return;
    gatt_client_handle_transaction_complete(peripheral);
    emit_gatt_complete_event(peripheral, error_code);
}

static void gatt_client_hci_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
        {
            log_info("GATT Client: HCI_EVENT_DISCONNECTION_COMPLETE");
            hci_con_handle_t con_handle = little_endian_read_16(packet,3);
            gatt_client_t * peripheral = get_gatt_client_context_for_handle(con_handle);
            if (!peripheral) break;
            gatt_client_report_error_if_pending(peripheral, ATT_ERROR_HCI_DISCONNECT_RECEIVED);
            
            btstack_linked_list_remove(&gatt_client_connections, (btstack_linked_item_t *) peripheral);
            btstack_memory_gatt_client_free(peripheral);
            break;
        }
        default:
            break;
    }

    gatt_client_run();
}

static void gatt_client_att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type == HCI_EVENT_PACKET && packet[0] == L2CAP_EVENT_CAN_SEND_NOW){
        gatt_client_run();
    }

    if (packet_type != ATT_DATA_PACKET) return;

    // special cases: notifications don't need a context while indications motivate creating one
    gatt_client_t * peripheral;
    switch (packet[0]){
        case ATT_HANDLE_VALUE_NOTIFICATION:
            report_gatt_notification(handle, little_endian_read_16(packet,1), &packet[3], size-3);
            return;                
        case ATT_HANDLE_VALUE_INDICATION:
            peripheral = provide_context_for_conn_handle(handle);
            break;
        default:
            peripheral = get_gatt_client_context_for_handle(handle);
            break;
    }

    if (!peripheral) return;
    
    switch (packet[0]){
        case ATT_EXCHANGE_MTU_RESPONSE:
        {
            uint16_t remote_rx_mtu = little_endian_read_16(packet, 1);
            uint16_t local_rx_mtu = l2cap_max_le_mtu();
            peripheral->mtu = remote_rx_mtu < local_rx_mtu ? remote_rx_mtu : local_rx_mtu;
            peripheral->mtu_state = MTU_EXCHANGED;

            break;
        }
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            switch(peripheral->gatt_client_state){
                case P_W4_SERVICE_QUERY_RESULT:
                    report_gatt_services(peripheral, packet, size);
                    trigger_next_service_query(peripheral, get_last_result_handle_from_service_list(packet, size));
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                default:
                    break;
            }
            break;
        case ATT_HANDLE_VALUE_INDICATION:
            report_gatt_indication(handle, little_endian_read_16(packet,1), &packet[3], size-3);
            peripheral->send_confirmation = 1;
            break;
            
        case ATT_READ_BY_TYPE_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT:
                    report_gatt_characteristics(peripheral, packet, size);
                    trigger_next_characteristic_query(peripheral, get_last_result_handle_from_characteristics_list(packet, size));
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done, or by ATT_ERROR
                    break;
                case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:
                    report_gatt_characteristics(peripheral, packet, size);
                    trigger_next_characteristic_query(peripheral, get_last_result_handle_from_characteristics_list(packet, size));
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done, or by ATT_ERROR
                    break;
                case P_W4_INCLUDED_SERVICE_QUERY_RESULT:
                {
                    uint16_t uuid16 = 0;
                    uint16_t pair_size = packet[1];
                    
                    if (pair_size < 7){
                        // UUIDs not available, query first included service
                        peripheral->start_group_handle = little_endian_read_16(packet, 2); // ready for next query
                        peripheral->query_start_handle = little_endian_read_16(packet, 4);
                        peripheral->query_end_handle = little_endian_read_16(packet,6);
                        peripheral->gatt_client_state = P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY;
                        break;
                    }
                    
                    uint16_t offset;
                    for (offset = 2; offset < size; offset += pair_size){
                        uint16_t include_handle = little_endian_read_16(packet, offset);
                        peripheral->query_start_handle = little_endian_read_16(packet,offset+2);
                        peripheral->query_end_handle = little_endian_read_16(packet,offset+4);
                        uuid16 = little_endian_read_16(packet, offset+6);
                        report_gatt_included_service_uuid16(peripheral, include_handle, uuid16);
                    }
                    
                    trigger_next_included_service_query(peripheral, get_last_result_handle_from_included_services_list(packet, size));
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT:
                    peripheral->client_characteristic_configuration_handle = little_endian_read_16(packet, 2);
                    peripheral->gatt_client_state = P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
                    break;
                case P_W4_READ_BY_TYPE_RESPONSE: {
                    uint16_t pair_size = packet[1];
                    uint16_t offset;
                    uint16_t last_result_handle = 0;
                    for (offset = 2; offset < size ; offset += pair_size){
                        uint16_t value_handle = little_endian_read_16(packet, offset);
                        report_gatt_characteristic_value(peripheral, value_handle, &packet[offset+2], pair_size-2);
                        last_result_handle = value_handle;
                    }
                    trigger_next_read_by_type_query(peripheral, last_result_handle);
                    break;
                }
                default:
                    break;
            }
            break;
        case ATT_READ_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT: {
                    uint8_t uuid128[16];
                    reverse_128(&packet[1], uuid128);
                    report_gatt_included_service_uuid128(peripheral, peripheral->start_group_handle, uuid128);
                    trigger_next_included_service_query(peripheral, peripheral->start_group_handle);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    report_gatt_characteristic_value(peripheral, peripheral->attribute_handle, &packet[1], size-1);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                    
                case P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT:{
                    gatt_client_handle_transaction_complete(peripheral);
                    report_gatt_characteristic_descriptor(peripheral, peripheral->attribute_handle, &packet[1], size-1, 0);
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
            int i;
            uint16_t start_group_handle;
            uint16_t   end_group_handle= 0xffff; // asserts GATT_EVENT_QUERY_COMPLETE is emitted if no results 
            for (i = 1; i<size; i+=pair_size){
                start_group_handle = little_endian_read_16(packet,i);
                end_group_handle = little_endian_read_16(packet,i+2);
                emit_gatt_service_query_result_event(peripheral, start_group_handle, end_group_handle, peripheral->uuid128);
            }
            trigger_next_service_by_uuid_query(peripheral, end_group_handle);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;
        }
        case ATT_FIND_INFORMATION_REPLY:
        {
            uint8_t pair_size = 4;
            if (packet[1] == 2){
                pair_size = 18;
            }
            uint16_t last_descriptor_handle = little_endian_read_16(packet, size - pair_size);
            
            report_gatt_all_characteristic_descriptors(peripheral, &packet[2], size-2, pair_size);
            trigger_next_characteristic_descriptor_query(peripheral, last_descriptor_handle);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
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
                    report_gatt_long_characteristic_value_blob(peripheral, peripheral->attribute_handle, &packet[1], received_blob_length, peripheral->attribute_offset);
                    trigger_next_blob_query(peripheral, P_W2_SEND_READ_BLOB_QUERY, received_blob_length);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                case P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    report_gatt_long_characteristic_descriptor(peripheral, peripheral->attribute_handle,
                                                          &packet[1], received_blob_length,
                                                          peripheral->attribute_offset);
                    trigger_next_blob_query(peripheral, P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY, received_blob_length);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                default:
                    break;
            }
            break;
        }
        case ATT_PREPARE_WRITE_RESPONSE:
            switch (peripheral->gatt_client_state){
                case P_W4_PREPARE_WRITE_SINGLE_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    if (is_value_valid(peripheral, packet, size)){
                        emit_gatt_complete_event(peripheral, 0);
                    } else {
                        emit_gatt_complete_event(peripheral, ATT_ERROR_DATA_MISMATCH);
                    }
                    break;

                case P_W4_PREPARE_WRITE_RESULT:{
                    peripheral->attribute_offset = little_endian_read_16(packet, 3);
                    trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:{
                    peripheral->attribute_offset = little_endian_read_16(packet, 3);
                    trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR, P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_PREPARE_RELIABLE_WRITE_RESULT:{
                    if (is_value_valid(peripheral, packet, size)){
                        peripheral->attribute_offset = little_endian_read_16(packet, 3);
                        trigger_next_prepare_write_query(peripheral, P_W2_PREPARE_RELIABLE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                        // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                        break;
                    }
                    peripheral->gatt_client_state = P_W2_CANCEL_PREPARED_WRITE_DATA_MISMATCH;
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
                case P_W4_CANCEL_PREPARED_WRITE_DATA_MISMATCH_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, ATT_ERROR_DATA_MISMATCH);
                    break;
                case P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    gatt_client_handle_transaction_complete(peripheral);
                    emit_gatt_complete_event(peripheral, 0);
                    break;
                default:
                    break;
                    
            }
            break;

        case ATT_READ_MULTIPLE_RESPONSE:
            switch(peripheral->gatt_client_state){
                case P_W4_READ_MULTIPLE_RESPONSE:
                    report_gatt_characteristic_value(peripheral, 0, &packet[1], size-1);
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
                        case P_W4_READ_BY_TYPE_RESPONSE:
                            gatt_client_handle_transaction_complete(peripheral);
                            if (peripheral->start_group_handle == peripheral->query_start_handle){
                                emit_gatt_complete_event(peripheral, ATT_ERROR_ATTRIBUTE_NOT_FOUND);
                            } else {
                                emit_gatt_complete_event(peripheral, 0);
                            }
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
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_client_t * peripheral = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        if (peripheral->gatt_client_state == P_W4_CMAC_RESULT){
            // store result
            memcpy(peripheral->cmac, hash, 8);
            // reverse_64(hash, peripheral->cmac);
            peripheral->gatt_client_state = P_W2_SEND_SIGNED_WRITE;
            gatt_client_run();
            return;
        }
    }
}

uint8_t gatt_client_signed_write_without_response(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t handle, uint16_t message_len, uint8_t * message){
    gatt_client_t * peripheral = provide_context_for_conn_handle(con_handle);
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    peripheral->le_device_index = sm_le_device_index(con_handle);
    if (peripheral->le_device_index < 0) return GATT_CLIENT_IN_WRONG_STATE; // device lookup not done / no stored bonding information

    peripheral->callback = callback;
    peripheral->attribute_handle = handle;
    peripheral->attribute_length = message_len;
    peripheral->attribute_value = message;
    peripheral->gatt_client_state = P_W4_CMAC_READY;

    gatt_client_run();
    return 0; 
}

uint8_t gatt_client_discover_primary_services(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;

    peripheral->callback = callback;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->gatt_client_state = P_W2_SEND_SERVICE_QUERY;
    peripheral->uuid16 = 0;
    gatt_client_run();
    return 0;
}


uint8_t gatt_client_discover_primary_services_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t uuid16){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;

    peripheral->callback = callback;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->gatt_client_state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    peripheral->uuid16 = uuid16;
    uuid_add_bluetooth_prefix((uint8_t*) &(peripheral->uuid128), peripheral->uuid16);
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_discover_primary_services_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, const uint8_t * uuid128){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;

    peripheral->callback = callback;
    peripheral->start_group_handle = 0x0001;
    peripheral->end_group_handle   = 0xffff;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->gatt_client_state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_discover_characteristics_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t *service){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;

    peripheral->callback = callback;
    peripheral->start_group_handle = service->start_group_handle;
    peripheral->end_group_handle   = service->end_group_handle;
    peripheral->filter_with_uuid = 0;
    peripheral->characteristic_start_handle = 0;
    peripheral->gatt_client_state = P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_find_included_services_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t *service){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->start_group_handle = service->start_group_handle;
    peripheral->end_group_handle   = service->end_group_handle;
    peripheral->gatt_client_state = P_W2_SEND_INCLUDED_SERVICE_QUERY;
    
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle   = end_handle;
    peripheral->filter_with_uuid = 1;
    peripheral->uuid16 = uuid16;
    uuid_add_bluetooth_prefix((uint8_t*) &(peripheral->uuid128), uuid16);
    peripheral->characteristic_start_handle = 0;
    peripheral->gatt_client_state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint8_t * uuid128){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle   = end_handle;
    peripheral->filter_with_uuid = 1;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->characteristic_start_handle = 0;
    peripheral->gatt_client_state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    
    gatt_client_run();
    return 0;
}


uint8_t gatt_client_discover_characteristics_for_service_by_uuid16(btstack_packet_handler_t callback, uint16_t handle, gatt_client_service_t *service, uint16_t  uuid16){
    return gatt_client_discover_characteristics_for_handle_range_by_uuid16(callback, handle, service->start_group_handle, service->end_group_handle, uuid16);
}

uint8_t gatt_client_discover_characteristics_for_service_by_uuid128(btstack_packet_handler_t callback, uint16_t handle, gatt_client_service_t *service, uint8_t * uuid128){
    return gatt_client_discover_characteristics_for_handle_range_by_uuid128(callback, handle, service->start_group_handle, service->end_group_handle, uuid128);
}

uint8_t gatt_client_discover_characteristic_descriptors(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t *characteristic){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    if (characteristic->value_handle == characteristic->end_handle){
        emit_gatt_complete_event(peripheral, 0);
        return 0;
    }
    peripheral->callback = callback;
    peripheral->start_group_handle = characteristic->value_handle + 1;
    peripheral->end_group_handle   = characteristic->end_handle;
    peripheral->gatt_client_state = P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY;
    
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_offset = 0;
    peripheral->gatt_client_state = P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_value_of_characteristics_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle = end_handle;
    peripheral->query_start_handle = start_handle;
    peripheral->query_end_handle = end_handle;
    peripheral->uuid16 = uuid16;
    uuid_add_bluetooth_prefix((uint8_t*) &(peripheral->uuid128), uuid16);
    peripheral->gatt_client_state = P_W2_SEND_READ_BY_TYPE_REQUEST;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_value_of_characteristics_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint8_t * uuid128){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->start_group_handle = start_handle;
    peripheral->end_group_handle = end_handle;
    peripheral->query_start_handle = start_handle;
    peripheral->query_end_handle = end_handle;
    peripheral->uuid16 = 0;
    memcpy(peripheral->uuid128, uuid128, 16);
    peripheral->gatt_client_state = P_W2_SEND_READ_BY_TYPE_REQUEST;
    gatt_client_run();
    return 0;
}


uint8_t gatt_client_read_value_of_characteristic(btstack_packet_handler_t callback, uint16_t handle, gatt_client_characteristic_t *characteristic){
    return gatt_client_read_value_of_characteristic_using_value_handle(callback, handle, characteristic->value_handle);
}

uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t characteristic_value_handle, uint16_t offset){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = characteristic_value_handle;
    peripheral->attribute_offset = offset;
    peripheral->gatt_client_state = P_W2_SEND_READ_BLOB_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t characteristic_value_handle){
    return gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(callback, con_handle, characteristic_value_handle, 0);
}

uint8_t gatt_client_read_long_value_of_characteristic(btstack_packet_handler_t callback, uint16_t handle, gatt_client_characteristic_t *characteristic){
    return gatt_client_read_long_value_of_characteristic_using_value_handle(callback, handle, characteristic->value_handle);
}

uint8_t gatt_client_read_multiple_characteristic_values(btstack_packet_handler_t callback, hci_con_handle_t con_handle, int num_value_handles, uint16_t * value_handles){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->read_multiple_handle_count = num_value_handles;
    peripheral->read_multiple_handles = value_handles;
    peripheral->gatt_client_state = P_W2_SEND_READ_MULTIPLE_REQUEST;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_write_value_of_characteristic_without_response(hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    if (value_length > peripheral_mtu(peripheral) - 3) return GATT_CLIENT_VALUE_TOO_LONG;
    if (!att_dispatch_client_can_send_now(peripheral->con_handle)) return GATT_CLIENT_BUSY;

    att_write_request(ATT_WRITE_COMMAND, peripheral->con_handle, value_handle, value_length, value);
    return 0;
}

uint8_t gatt_client_write_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * data){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_value = data;
    peripheral->gatt_client_state = P_W2_SEND_WRITE_CHARACTERISTIC_VALUE;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_write_long_value_of_characteristic_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t offset, uint16_t value_length, uint8_t  * data){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_offset = offset;
    peripheral->attribute_value = data;
    peripheral->gatt_client_state = P_W2_PREPARE_WRITE;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_write_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    return gatt_client_write_long_value_of_characteristic_with_offset(callback, con_handle, value_handle, 0, value_length, value);    
}

uint8_t gatt_client_reliable_write_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = value_handle;
    peripheral->attribute_length = value_length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = value;
    peripheral->gatt_client_state = P_W2_PREPARE_RELIABLE_WRITE;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_write_client_characteristic_configuration(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic, uint16_t configuration){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) &&
        (characteristic->properties & ATT_PROPERTY_NOTIFY) == 0) {
        log_info("gatt_client_write_client_characteristic_configuration: GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED");
        return GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED;
    } else if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION) &&
               (characteristic->properties & ATT_PROPERTY_INDICATE) == 0){
        log_info("gatt_client_write_client_characteristic_configuration: GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED");
        return GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED;
    }
    
    peripheral->callback = callback;
    peripheral->start_group_handle = characteristic->value_handle;
    peripheral->end_group_handle = characteristic->end_handle;
    little_endian_store_16(peripheral->client_characteristic_configuration_value, 0, configuration);
    
    peripheral->gatt_client_state = P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = descriptor_handle;
    
    peripheral->gatt_client_state = P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor){
    return gatt_client_read_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle);
}

uint8_t gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t offset){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = descriptor_handle;
    peripheral->attribute_offset = offset;
    peripheral->gatt_client_state = P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle){
    return gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(callback, con_handle, descriptor_handle, 0);
}

uint8_t gatt_client_read_long_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor){
    return gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle);
}

uint8_t gatt_client_write_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t length, uint8_t  * data){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = descriptor_handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = 0;
    peripheral->attribute_value = data;
    peripheral->gatt_client_state = P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_write_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * value){
    return gatt_client_write_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle, length, value);
}

uint8_t gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t offset, uint16_t length, uint8_t  * data){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = descriptor_handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = offset;
    peripheral->attribute_value = data;
    peripheral->gatt_client_state = P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return 0;
}

uint8_t gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t length, uint8_t * data){
    return gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(callback, con_handle, descriptor_handle, 0, length, data );
}

uint8_t gatt_client_write_long_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor, uint16_t length, uint8_t * value){
    return gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle, length, value);
}

/**
 * @brief -> gatt complete event
 */
uint8_t gatt_client_prepare_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint16_t length, uint8_t * data){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->attribute_handle = attribute_handle;
    peripheral->attribute_length = length;
    peripheral->attribute_offset = offset;
    peripheral->attribute_value = data;
    peripheral->gatt_client_state = P_W2_PREPARE_WRITE_SINGLE;
    gatt_client_run();
    return 0;
}

/**
 * @brief -> gatt complete event
 */
uint8_t gatt_client_execute_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);

    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->gatt_client_state = P_W2_EXECUTE_PREPARED_WRITE;
    gatt_client_run();
    return 0;
}

/**
 * @brief -> gatt complete event
 */
uint8_t gatt_client_cancel_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * peripheral = provide_context_for_conn_handle_and_start_timer(con_handle);
    
    if (!peripheral) return BTSTACK_MEMORY_ALLOC_FAILED; 
    if (!is_ready(peripheral)) return GATT_CLIENT_IN_WRONG_STATE;
    
    peripheral->callback = callback;
    peripheral->gatt_client_state = P_W2_CANCEL_PREPARED_WRITE;
    gatt_client_run();
    return 0;    
}

void gatt_client_pts_suppress_mtu_exchange(void){
    pts_suppress_mtu_exchange = 1;
}

void gatt_client_deserialize_service(const uint8_t *packet, int offset, gatt_client_service_t *service){
    service->start_group_handle = little_endian_read_16(packet, offset);
    service->end_group_handle = little_endian_read_16(packet, offset + 2);
    reverse_128(&packet[offset + 4], service->uuid128);
    if (uuid_has_bluetooth_prefix(service->uuid128)){
        service->uuid16 = big_endian_read_32(service->uuid128, 0);
    }
}

void gatt_client_deserialize_characteristic(const uint8_t * packet, int offset, gatt_client_characteristic_t * characteristic){
    characteristic->start_handle = little_endian_read_16(packet, offset);
    characteristic->value_handle = little_endian_read_16(packet, offset + 2);
    characteristic->end_handle = little_endian_read_16(packet, offset + 4);
    characteristic->properties = little_endian_read_16(packet, offset + 6);
    reverse_128(&packet[offset+8], characteristic->uuid128);
    if (uuid_has_bluetooth_prefix(characteristic->uuid128)){
        characteristic->uuid16 = big_endian_read_32(characteristic->uuid128, 0);
    }
}

void gatt_client_deserialize_characteristic_descriptor(const uint8_t * packet, int offset, gatt_client_characteristic_descriptor_t * descriptor){
    descriptor->handle = little_endian_read_16(packet, offset);
    reverse_128(&packet[offset+2], descriptor->uuid128);
}
