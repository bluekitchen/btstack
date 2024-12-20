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

#define BTSTACK_FILE__ "gatt_client.c"

#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "btstack_config.h"

#include "ble/att_dispatch.h"
#include "ble/att_db.h"
#include "ble/gatt_client.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "bluetooth_psm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_event_builder.h"
#include "l2cap.h"
#include "classic/sdp_client.h"
#include "bluetooth_gatt.h"
#include "bluetooth_sdp.h"
#include "classic/sdp_util.h"

#if defined(ENABLE_GATT_OVER_EATT) && !defined(ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE)
#error "GATT Over EATT requires support for L2CAP Enhanced CoC. Please enable ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE"
#endif

// L2CAP Test Spec p35 defines a minimum of 100 ms, but PTS might indicate an error if we sent after 100 ms
#define GATT_CLIENT_COLLISION_BACKOFF_MS 150

static btstack_linked_list_t gatt_client_connections;
static btstack_linked_list_t gatt_client_value_listeners;
static btstack_linked_list_t gatt_client_service_value_listeners;
#ifdef ENABLE_GATT_CLIENT_SERVICE_CHANGED
static btstack_linked_list_t gatt_client_service_changed_handler;
#endif
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static btstack_context_callback_registration_t gatt_client_deferred_event_emit;

// GATT Client Configuration
static bool                 gatt_client_mtu_exchange_enabled;
static gap_security_level_t gatt_client_required_security_level;

static void gatt_client_att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size);
static void gatt_client_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void gatt_client_report_error_if_pending(gatt_client_t *gatt_client, uint8_t att_error_code);

#ifdef ENABLE_LE_SIGNED_WRITE
static void att_signed_write_handle_cmac_result(uint8_t hash[8]);
#endif

#ifdef ENABLE_GATT_OVER_CLASSIC
static gatt_client_t * gatt_client_get_context_for_l2cap_cid(uint16_t l2cap_cid);
static void gatt_client_classic_handle_connected(gatt_client_t * gatt_client, uint8_t status);
static void gatt_client_classic_handle_disconnected(gatt_client_t * gatt_client);
static void gatt_client_classic_retry(btstack_timer_source_t * ts);
#endif

#ifdef ENABLE_GATT_OVER_EATT
static bool gatt_client_eatt_enabled;
static bool gatt_client_le_enhanced_handle_can_send_query(gatt_client_t * gatt_client);
static void gatt_client_le_enhanced_retry(btstack_timer_source_t * ts);
#endif

void gatt_client_init(void){
    gatt_client_connections = NULL;
    gatt_client_value_listeners = NULL;
    gatt_client_service_value_listeners = NULL;
#ifdef ENABLE_GATT_CLIENT_SERVICE_CHANGED
    gatt_client_service_changed_handler = NULL;
#endif
    // default configuration
    gatt_client_mtu_exchange_enabled    = true;
    gatt_client_required_security_level = LEVEL_0;

    // register for HCI Events
    hci_event_callback_registration.callback = &gatt_client_event_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM Events
    sm_event_callback_registration.callback = &gatt_client_event_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // and ATT Client PDUs
    att_dispatch_register_client(gatt_client_att_packet_handler);

#ifdef ENABLE_GATT_OVER_EATT
    gatt_client_eatt_enabled = true;
#endif
}

void gatt_client_set_required_security_level(gap_security_level_t level){
    gatt_client_required_security_level = level;
}

static gatt_client_t * gatt_client_for_timer(btstack_timer_source_t * ts){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &gatt_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_client_t * gatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        if (&gatt_client->gc_timeout == ts) {
            return gatt_client;
        }
    }
    return NULL;
}

static void gatt_client_timeout_handler(btstack_timer_source_t * timer){
    gatt_client_t * gatt_client = gatt_client_for_timer(timer);
    if (gatt_client == NULL) return;
    log_info("GATT client timeout handle, handle 0x%02x", gatt_client->con_handle);
    gatt_client_report_error_if_pending(gatt_client, ATT_ERROR_TIMEOUT);
}

static void gatt_client_timeout_start(gatt_client_t * gatt_client){
    log_debug("GATT client timeout start, handle 0x%02x", gatt_client->con_handle);
    btstack_run_loop_remove_timer(&gatt_client->gc_timeout);
    btstack_run_loop_set_timer_handler(&gatt_client->gc_timeout, gatt_client_timeout_handler);
    btstack_run_loop_set_timer(&gatt_client->gc_timeout, 30000); // 30 seconds sm timeout
    btstack_run_loop_add_timer(&gatt_client->gc_timeout);
}

static void gatt_client_timeout_stop(gatt_client_t * gatt_client){
    log_debug("GATT client timeout stop, handle 0x%02x", gatt_client->con_handle);
    btstack_run_loop_remove_timer(&gatt_client->gc_timeout);
}

static gap_security_level_t gatt_client_le_security_level_for_connection(hci_con_handle_t con_handle){
    uint8_t encryption_key_size = gap_encryption_key_size(con_handle);
    if (encryption_key_size == 0) return LEVEL_0;

    bool authenticated = gap_authenticated(con_handle);
    if (!authenticated) return LEVEL_2;

    return encryption_key_size == 16 ? LEVEL_4 : LEVEL_3;
}

static gatt_client_t * gatt_client_get_context_for_handle(uint16_t handle){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) gatt_client_connections; it != NULL; it = it->next){
        gatt_client_t * gatt_client = (gatt_client_t *) it;
        if (gatt_client->con_handle == handle){
            return gatt_client;
        }
    }
    return NULL;
}


// @return gatt_client context
// returns existing one, or tries to setup new one
static uint8_t gatt_client_provide_context_for_handle(hci_con_handle_t con_handle, gatt_client_t ** out_gatt_client){
    gatt_client_t * gatt_client = gatt_client_get_context_for_handle(con_handle);

    if (gatt_client != NULL){
        *out_gatt_client = gatt_client;
        return ERROR_CODE_SUCCESS;
    }

    // bail if no such hci connection
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection == NULL){
        log_error("No connection for handle 0x%04x", con_handle);
        *out_gatt_client = NULL;
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    gatt_client = btstack_memory_gatt_client_get();
    if (gatt_client == NULL){
        *out_gatt_client = NULL;
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    } 
    // init state
    gatt_client->bearer_type = ATT_BEARER_UNENHANCED_LE;
    gatt_client->con_handle = con_handle;
    gatt_client->mtu = ATT_DEFAULT_MTU;
    gatt_client->security_level = gatt_client_le_security_level_for_connection(con_handle);
    if (gatt_client_mtu_exchange_enabled){
        gatt_client->mtu_state = SEND_MTU_EXCHANGE;
    } else {
        gatt_client->mtu_state = MTU_AUTO_EXCHANGE_DISABLED;
    }
    gatt_client->state = P_READY;
    gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DISCOVER_W2_SEND;
#ifdef ENABLE_GATT_OVER_EATT
    gatt_client->eatt_state = GATT_CLIENT_EATT_IDLE;
#endif
    btstack_linked_list_add(&gatt_client_connections, (btstack_linked_item_t*)gatt_client);

    // get unenhanced att bearer state
    if (hci_connection->att_connection.mtu_exchanged){
        gatt_client->mtu = hci_connection->att_connection.mtu;
        gatt_client->mtu_state = MTU_EXCHANGED;
    }
    *out_gatt_client = gatt_client;
    return ERROR_CODE_SUCCESS;
}

static bool is_ready(gatt_client_t * gatt_client){
    return gatt_client->state == P_READY;
}

static uint8_t gatt_client_provide_context_for_request(hci_con_handle_t con_handle, gatt_client_t ** out_gatt_client){
    gatt_client_t * gatt_client = NULL;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

#ifdef ENABLE_GATT_OVER_EATT
    if ((gatt_client->eatt_state == GATT_CLIENT_EATT_READY) && gatt_client_eatt_enabled){
        btstack_linked_list_iterator_t it;
        gatt_client_t * eatt_client = NULL;
        // find free eatt client
        btstack_linked_list_iterator_init(&it, &gatt_client->eatt_clients);
        while (btstack_linked_list_iterator_has_next(&it)){
            gatt_client_t * client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
            if (client->state == P_READY){
                eatt_client = client;
                break;
            }
        }
        if (eatt_client == NULL){
            return ERROR_CODE_COMMAND_DISALLOWED;
        }
        gatt_client = eatt_client;
    }
#endif

    if (is_ready(gatt_client) == false){
        return GATT_CLIENT_IN_WRONG_STATE;
    }

    gatt_client_timeout_start(gatt_client);

    *out_gatt_client = gatt_client;

    return status;
}

int gatt_client_is_ready(hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return 0;
    }
    return is_ready(gatt_client) ? 1 : 0;
}

void gatt_client_mtu_enable_auto_negotiation(uint8_t enabled){
    gatt_client_mtu_exchange_enabled = enabled != 0;
}

uint8_t gatt_client_get_mtu(hci_con_handle_t con_handle, uint16_t * mtu){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        *mtu = 0;
        return status;
    }

    if ((gatt_client->mtu_state == MTU_EXCHANGED) || (gatt_client->mtu_state == MTU_AUTO_EXCHANGE_DISABLED)){
        *mtu = gatt_client->mtu;
        return ERROR_CODE_SUCCESS;
    } 
    *mtu = ATT_DEFAULT_MTU;
    return GATT_CLIENT_IN_WRONG_STATE;
}

static uint8_t *gatt_client_reserve_request_buffer(gatt_client_t *gatt_client) {
    switch (gatt_client->bearer_type){
#ifdef ENABLE_GATT_OVER_CLASSIC
        case ATT_BEARER_UNENHANCED_CLASSIC:
#endif
        case ATT_BEARER_UNENHANCED_LE:
            l2cap_reserve_packet_buffer();
            return l2cap_get_outgoing_buffer();
#ifdef ENABLE_GATT_OVER_EATT
        case ATT_BEARER_ENHANCED_LE:
            return gatt_client->eatt_storage_buffer;
#endif
        default:
            btstack_unreachable();
            break;
    }
    return NULL;
}

// precondition: can_send_packet_now == TRUE
static uint8_t gatt_client_send(gatt_client_t * gatt_client, uint16_t len){
    switch (gatt_client->bearer_type){
        case ATT_BEARER_UNENHANCED_LE:
            return l2cap_send_prepared_connectionless(gatt_client->con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, len);
#ifdef ENABLE_GATT_OVER_CLASSIC
        case ATT_BEARER_UNENHANCED_CLASSIC:
            return l2cap_send_prepared(gatt_client->l2cap_cid, len);
#endif
#ifdef ENABLE_GATT_OVER_EATT
        case ATT_BEARER_ENHANCED_LE:
            return l2cap_send(gatt_client->l2cap_cid, gatt_client->eatt_storage_buffer, len);
#endif
        default:
            btstack_unreachable();
            return ERROR_CODE_HARDWARE_FAILURE;
    }
}

// precondition: can_send_packet_now == TRUE
static uint8_t att_confirmation(gatt_client_t * gatt_client) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = ATT_HANDLE_VALUE_CONFIRMATION;

    return gatt_client_send(gatt_client, 1);
}

// precondition: can_send_packet_now == TRUE
static uint8_t att_find_information_request(gatt_client_t *gatt_client, uint8_t request_type, uint16_t start_handle,
                                            uint16_t end_handle) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);

    return gatt_client_send(gatt_client, 5);
}

// precondition: can_send_packet_now == TRUE
static uint8_t
att_find_by_type_value_request(gatt_client_t *gatt_client, uint8_t request_type, uint16_t attribute_group_type,
                               uint16_t start_handle, uint16_t end_handle, uint8_t *value, uint16_t value_size) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);
    request[0] = request_type;

    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    little_endian_store_16(request, 5, attribute_group_type);
    (void)memcpy(&request[7], value, value_size);
    
    return gatt_client_send(gatt_client, 7u + value_size);
}

// precondition: can_send_packet_now == TRUE
static uint8_t
att_read_by_type_or_group_request_for_uuid16(gatt_client_t *gatt_client, uint8_t request_type, uint16_t uuid16,
                                             uint16_t start_handle, uint16_t end_handle) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    little_endian_store_16(request, 5, uuid16);
    
    return gatt_client_send(gatt_client, 7);
}

// precondition: can_send_packet_now == TRUE
static uint8_t
att_read_by_type_or_group_request_for_uuid128(gatt_client_t *gatt_client, uint8_t request_type, const uint8_t *uuid128,
                                              uint16_t start_handle, uint16_t end_handle) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, start_handle);
    little_endian_store_16(request, 3, end_handle);
    reverse_128(uuid128, &request[5]);
    
    return gatt_client_send(gatt_client, 21);
}

// precondition: can_send_packet_now == TRUE
static uint8_t att_read_request(gatt_client_t *gatt_client, uint8_t request_type, uint16_t attribute_handle) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    
    return gatt_client_send(gatt_client, 3);
}

// precondition: can_send_packet_now == TRUE
static uint8_t att_read_blob_request(gatt_client_t *gatt_client, uint8_t request_type, uint16_t attribute_handle,
                                     uint16_t value_offset) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    little_endian_store_16(request, 3, value_offset);
    
    return gatt_client_send(gatt_client, 5);
}

static uint8_t
att_read_multiple_request_with_opcode(gatt_client_t *gatt_client, uint16_t num_value_handles, uint16_t *value_handles, uint8_t opcode) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = opcode;
    uint16_t i;
    uint16_t offset = 1;
    for (i=0;i<num_value_handles;i++){
        little_endian_store_16(request, offset, value_handles[i]);
        offset += 2;
    }

    return gatt_client_send(gatt_client, offset);
}

static uint8_t
att_read_multiple_request(gatt_client_t *gatt_client, uint16_t num_value_handles, uint16_t *value_handles) {
    return att_read_multiple_request_with_opcode(gatt_client, num_value_handles, value_handles, ATT_READ_MULTIPLE_REQUEST);
}

#ifdef ENABLE_GATT_OVER_EATT
static uint8_t
att_read_multiple_variable_request(gatt_client_t *gatt_client, uint16_t num_value_handles, uint16_t *value_handles) {
    return att_read_multiple_request_with_opcode(gatt_client, num_value_handles, value_handles, ATT_READ_MULTIPLE_VARIABLE_REQ);
}
#endif

#ifdef ENABLE_LE_SIGNED_WRITE
// precondition: can_send_packet_now == TRUE
static uint8_t att_signed_write_request(gatt_client_t *gatt_client, uint16_t request_type, uint16_t attribute_handle,
                                        uint16_t value_length, uint8_t *value, uint32_t sign_counter, uint8_t sgn[8]) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    (void)memcpy(&request[3], value, value_length);
    little_endian_store_32(request, 3 + value_length, sign_counter);
    reverse_64(sgn, &request[3 + value_length + 4]);
    
    return gatt_client_send(gatt_client, 3 + value_length + 12);
}
#endif

// precondition: can_send_packet_now == TRUE
static uint8_t
att_write_request(gatt_client_t *gatt_client, uint8_t request_type, uint16_t attribute_handle, uint16_t value_length,
                  uint8_t *value) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    (void)memcpy(&request[3], value, value_length);
    
    return gatt_client_send(gatt_client, 3u + value_length);
}

// precondition: can_send_packet_now == TRUE
static uint8_t att_execute_write_request(gatt_client_t *gatt_client, uint8_t request_type, uint8_t execute_write) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    request[1] = execute_write;
    
    return gatt_client_send(gatt_client, 2);
}

// precondition: can_send_packet_now == TRUE
static uint8_t att_prepare_write_request(gatt_client_t *gatt_client, uint8_t request_type, uint16_t attribute_handle,
                                         uint16_t value_offset, uint16_t blob_length, uint8_t *value) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = request_type;
    little_endian_store_16(request, 1, attribute_handle);
    little_endian_store_16(request, 3, value_offset);
    (void)memcpy(&request[5], &value[value_offset], blob_length);
    
    return gatt_client_send(gatt_client,  5u + blob_length);
}

static uint8_t att_exchange_mtu_request(gatt_client_t *gatt_client) {
    uint8_t *request = gatt_client_reserve_request_buffer(gatt_client);

    request[0] = ATT_EXCHANGE_MTU_REQUEST;
    uint16_t mtu = l2cap_max_le_mtu();
    little_endian_store_16(request, 1, mtu);
    
    return gatt_client_send(gatt_client, 3);
}

static uint16_t write_blob_length(gatt_client_t * gatt_client){
    uint16_t max_blob_length = gatt_client->mtu - 5u;
    if (gatt_client->attribute_offset >= gatt_client->attribute_length) {
        return 0;
    }
    uint16_t rest_length = gatt_client->attribute_length - gatt_client->attribute_offset;
    if (max_blob_length > rest_length){
        return rest_length;
    }
    return max_blob_length;
}

static void send_gatt_services_request(gatt_client_t *gatt_client){
    att_read_by_type_or_group_request_for_uuid16(gatt_client, ATT_READ_BY_GROUP_TYPE_REQUEST,
                                                 gatt_client->uuid16, gatt_client->start_group_handle,
                                                 gatt_client->end_group_handle);
}

static void send_gatt_by_uuid_request(gatt_client_t *gatt_client, uint16_t attribute_group_type){
    if (gatt_client->uuid16 != 0u){
        uint8_t uuid16[2];
        little_endian_store_16(uuid16, 0, gatt_client->uuid16);
        att_find_by_type_value_request(gatt_client, ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type,
                                       gatt_client->start_group_handle, gatt_client->end_group_handle, uuid16, 2);
        return;
    }
    uint8_t uuid128[16];
    reverse_128(gatt_client->uuid128, uuid128);
    att_find_by_type_value_request(gatt_client, ATT_FIND_BY_TYPE_VALUE_REQUEST, attribute_group_type,
                                   gatt_client->start_group_handle, gatt_client->end_group_handle, uuid128, 16);
}

static void send_gatt_services_by_uuid_request(gatt_client_t *gatt_client){
    send_gatt_by_uuid_request(gatt_client, GATT_PRIMARY_SERVICE_UUID);
}

static void send_gatt_included_service_uuid_request(gatt_client_t *gatt_client){
    att_read_request(gatt_client, ATT_READ_REQUEST, gatt_client->query_start_handle);
}

static void send_gatt_included_service_request(gatt_client_t *gatt_client){
    att_read_by_type_or_group_request_for_uuid16(gatt_client, ATT_READ_BY_TYPE_REQUEST,
                                                 GATT_INCLUDE_SERVICE_UUID, gatt_client->start_group_handle,
                                                 gatt_client->end_group_handle);
}

static void send_gatt_characteristic_request(gatt_client_t *gatt_client){
    att_read_by_type_or_group_request_for_uuid16(gatt_client, ATT_READ_BY_TYPE_REQUEST,
                                                 GATT_CHARACTERISTICS_UUID, gatt_client->start_group_handle,
                                                 gatt_client->end_group_handle);
}

static void send_gatt_characteristic_descriptor_request(gatt_client_t *gatt_client){
    att_find_information_request(gatt_client, ATT_FIND_INFORMATION_REQUEST, gatt_client->start_group_handle,
                                 gatt_client->end_group_handle);
}

static void send_gatt_read_characteristic_value_request(gatt_client_t *gatt_client){
    att_read_request(gatt_client, ATT_READ_REQUEST, gatt_client->attribute_handle);
}

static void send_gatt_read_by_type_request(gatt_client_t * gatt_client){
    if (gatt_client->uuid16 != 0u){
        att_read_by_type_or_group_request_for_uuid16(gatt_client, ATT_READ_BY_TYPE_REQUEST,
                                                     gatt_client->uuid16, gatt_client->start_group_handle,
                                                     gatt_client->end_group_handle);
    } else {
        att_read_by_type_or_group_request_for_uuid128(gatt_client, ATT_READ_BY_TYPE_REQUEST,
                                                      gatt_client->uuid128, gatt_client->start_group_handle,
                                                      gatt_client->end_group_handle);
    }
}

static void send_gatt_read_blob_request(gatt_client_t *gatt_client){
    if (gatt_client->attribute_offset == 0){
        att_read_request(gatt_client, ATT_READ_REQUEST, gatt_client->attribute_handle);
    } else {
        att_read_blob_request(gatt_client, ATT_READ_BLOB_REQUEST, gatt_client->attribute_handle,
                              gatt_client->attribute_offset);
    }
}

static void send_gatt_read_multiple_request(gatt_client_t * gatt_client){
    att_read_multiple_request(gatt_client, gatt_client->read_multiple_handle_count, gatt_client->read_multiple_handles);
}

#ifdef ENABLE_GATT_OVER_EATT
static void send_gatt_read_multiple_variable_request(gatt_client_t * gatt_client){
    att_read_multiple_variable_request(gatt_client, gatt_client->read_multiple_handle_count, gatt_client->read_multiple_handles);
}
#endif

static void send_gatt_write_attribute_value_request(gatt_client_t * gatt_client){
    att_write_request(gatt_client, ATT_WRITE_REQUEST, gatt_client->attribute_handle, gatt_client->attribute_length,
                      gatt_client->attribute_value);
}

static void send_gatt_write_client_characteristic_configuration_request(gatt_client_t * gatt_client){
    att_write_request(gatt_client, ATT_WRITE_REQUEST, gatt_client->client_characteristic_configuration_handle, 2,
                      gatt_client->client_characteristic_configuration_value);
}

static void send_gatt_prepare_write_request(gatt_client_t * gatt_client){
    att_prepare_write_request(gatt_client, ATT_PREPARE_WRITE_REQUEST, gatt_client->attribute_handle,
                              gatt_client->attribute_offset, write_blob_length(gatt_client),
                              gatt_client->attribute_value);
}

static void send_gatt_execute_write_request(gatt_client_t * gatt_client){
    att_execute_write_request(gatt_client, ATT_EXECUTE_WRITE_REQUEST, 1);
}

static void send_gatt_cancel_prepared_write_request(gatt_client_t * gatt_client){
    att_execute_write_request(gatt_client, ATT_EXECUTE_WRITE_REQUEST, 0);
}

#ifndef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
static void send_gatt_read_client_characteristic_configuration_request(gatt_client_t * gatt_client){
    att_read_by_type_or_group_request_for_uuid16(gatt_client, ATT_READ_BY_TYPE_REQUEST,
                                                 GATT_CLIENT_CHARACTERISTICS_CONFIGURATION,
                                                 gatt_client->start_group_handle, gatt_client->end_group_handle);
}
#endif

static void send_gatt_read_characteristic_descriptor_request(gatt_client_t * gatt_client){
    att_read_request(gatt_client, ATT_READ_REQUEST, gatt_client->attribute_handle);
}

#ifdef ENABLE_LE_SIGNED_WRITE
static void send_gatt_signed_write_request(gatt_client_t * gatt_client, uint32_t sign_counter){
    att_signed_write_request(gatt_client, ATT_SIGNED_WRITE_COMMAND, gatt_client->attribute_handle,
                             gatt_client->attribute_length, gatt_client->attribute_value, sign_counter,
                             gatt_client->cmac);
}
#endif

static uint16_t get_last_result_handle_from_service_list(uint8_t * packet, uint16_t size){
    if (size < 2) return 0xffff;
    uint8_t attr_length = packet[1];
    if ((2 + attr_length) > size) return 0xffff;
    return little_endian_read_16(packet, size - attr_length + 2u);
}

static uint16_t get_last_result_handle_from_characteristics_list(uint8_t * packet, uint16_t size){
    if (size < 2) return 0xffff;
    uint8_t attr_length = packet[1];
    if ((2 + attr_length) > size) return 0xffff;
    return little_endian_read_16(packet, size - attr_length + 3u);
}

static uint16_t get_last_result_handle_from_included_services_list(uint8_t * packet, uint16_t size){
    if (size < 2) return 0xffff;
    uint8_t attr_length = packet[1];
    if ((2 + attr_length) > size) return 0xffff;
    return little_endian_read_16(packet, size - attr_length);
}

#ifdef ENABLE_GATT_CLIENT_SERVICE_CHANGED
static void gatt_client_service_emit_event(gatt_client_t * gatt_client, uint8_t * event, uint16_t size){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client_service_changed_handler);
    while (btstack_linked_list_iterator_has_next(&it)) {
        btstack_packet_callback_registration_t *callback = (btstack_packet_callback_registration_t *) btstack_linked_list_iterator_next(&it);
        (*callback->callback)(HCI_EVENT_PACKET, (uint16_t) gatt_client->con_handle, event, size);
    }
}

static void
gatt_client_service_emit_database_hash(gatt_client_t *gatt_client, const uint8_t *value, uint16_t value_len) {
    if (value_len == 16){
        uint8_t event[21];
        hci_event_builder_context_t context;
        hci_event_builder_init(&context, event, sizeof(event), HCI_EVENT_GATTSERVICE_META, GATTSERVICE_SUBEVENT_GATT_DATABASE_HASH);
        hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
        hci_event_builder_add_bytes(&context, value, 16);
        gatt_client_service_emit_event(gatt_client, event, hci_event_builder_get_length(&context));
    }
}

static void
gatt_client_service_emit_service_changed(gatt_client_t *gatt_client, const uint8_t *value, uint16_t value_len) {
    if (value_len == 4){
        uint8_t event[9];
        hci_event_builder_context_t context;
        hci_event_builder_init(&context, event, sizeof(event), HCI_EVENT_GATTSERVICE_META, GATTSERVICE_SUBEVENT_GATT_SERVICE_CHANGED);
        hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
        hci_event_builder_add_bytes(&context, value, 4);
        gatt_client_service_emit_event(gatt_client, event, hci_event_builder_get_length(&context));
    }
}

static void gatt_client_service_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);  // ok: handling own l2cap events
    UNUSED(size);     // ok: there is no channel

    hci_con_handle_t con_handle;
    gatt_client_t *gatt_client;
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    con_handle = gatt_event_service_query_result_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(gatt_client->gatt_service_state == GATT_CLIENT_SERVICE_DISCOVER_W4_DONE);
                    gatt_event_service_query_result_get_service(packet, &service);
                    gatt_client->gatt_service_start_group_handle = service.start_group_handle;
                    gatt_client->gatt_service_end_group_handle = service.end_group_handle;
                    break;
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    con_handle = gatt_event_characteristic_query_result_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(gatt_client->gatt_service_state == GATT_CLIENT_SERVICE_DISCOVER_CHARACTERISTICS_W4_DONE);
                    gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
                    switch (characteristic.uuid16){
                        case ORG_BLUETOOTH_CHARACTERISTIC_GATT_SERVICE_CHANGED:
                            gatt_client->gatt_service_changed_value_handle = characteristic.value_handle;
                            gatt_client->gatt_service_changed_end_handle = characteristic.end_handle;
                            break;
                        case ORG_BLUETOOTH_CHARACTERISTIC_DATABASE_HASH:
                            gatt_client->gatt_service_database_hash_value_handle = characteristic.value_handle;
                            gatt_client->gatt_service_database_hash_end_handle = characteristic.end_handle;
                            break;
                        default:
                            break;
                    }
                    break;
                case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
                    con_handle = gatt_event_characteristic_value_query_result_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(gatt_client->gatt_service_state == GATT_CLIENT_SERVICE_DATABASE_HASH_READ_W4_DONE);
                        gatt_client_service_emit_database_hash(gatt_client,
                                                               gatt_event_characteristic_value_query_result_get_value(packet),
                                                               gatt_event_characteristic_value_query_result_get_value_length(packet));
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    con_handle = gatt_event_query_complete_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    switch (gatt_client->gatt_service_state) {
                        case GATT_CLIENT_SERVICE_DISCOVER_W4_DONE:
                            gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DISCOVER_CHARACTERISTICS_W2_SEND;
                            break;
                        case GATT_CLIENT_SERVICE_DISCOVER_CHARACTERISTICS_W4_DONE:
                            gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_SERVICE_CHANGED_WRITE_CCCD_W2_SEND;
                            break;
                        case GATT_CLIENT_SERVICE_SERVICE_CHANGED_WRITE_CCCD_W4_DONE:
                            gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DATABASE_HASH_READ_W2_SEND;
                            break;
                        case GATT_CLIENT_SERVICE_DATABASE_HASH_READ_W4_DONE:
                            gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DATABASE_HASH_WRITE_CCCD_W2_SEND;
                            break;
                        case GATT_CLIENT_SERVICE_DATABASE_HASH_WRITE_CCCD_W4_DONE:
                            gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DONE;
                            break;
                        default:
                            btstack_unreachable();
                            break;
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
#endif

static void gatt_client_notify_can_send_query(gatt_client_t * gatt_client){

#ifdef ENABLE_GATT_OVER_EATT
    // if eatt is ready, notify all clients that can send a query
    if (gatt_client->eatt_state == GATT_CLIENT_EATT_READY){
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &gatt_client->eatt_clients);
        while (btstack_linked_list_iterator_has_next(&it)){
            gatt_client_t * client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
            if (client->state == P_READY){
                // call callback
                btstack_context_callback_registration_t * callback = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&gatt_client->query_requests);
                if (callback == NULL) {
                    return;
                }
                (*callback->callback)(callback->context);
            }
        }
        return;
    }
#endif

    while (gatt_client->state == P_READY){
        bool query_sent = false;
        UNUSED(query_sent);

#ifdef ENABLE_GATT_CLIENT_SERVICE_CHANGED
        uint8_t status = ERROR_CODE_SUCCESS;
        gatt_client_service_t gatt_service;
        gatt_client_characteristic_t characteristic;
        switch (gatt_client->gatt_service_state){
            case GATT_CLIENT_SERVICE_DISCOVER_W2_SEND:
                gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DISCOVER_W4_DONE;
                status = gatt_client_discover_primary_services_by_uuid16(&gatt_client_service_packet_handler,
                                                                                         gatt_client->con_handle,
                                                                                         ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE);
                query_sent = true;
                break;
            case GATT_CLIENT_SERVICE_DISCOVER_CHARACTERISTICS_W2_SEND:
                if (gatt_client->gatt_service_start_group_handle != 0){
                    gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DISCOVER_CHARACTERISTICS_W4_DONE;
                    gatt_service.start_group_handle = gatt_client->gatt_service_start_group_handle;
                    gatt_service.end_group_handle   = gatt_client->gatt_service_end_group_handle;
                    status = gatt_client_discover_characteristics_for_service(&gatt_client_service_packet_handler, gatt_client->con_handle, &gatt_service);
                    query_sent = true;
                    break;
                }

                /* fall through */

            case GATT_CLIENT_SERVICE_SERVICE_CHANGED_WRITE_CCCD_W2_SEND:
                if (gatt_client->gatt_service_changed_value_handle != 0){
                    gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_SERVICE_CHANGED_WRITE_CCCD_W4_DONE;
                    characteristic.value_handle = gatt_client->gatt_service_changed_value_handle;
                    characteristic.end_handle   = gatt_client->gatt_service_changed_end_handle;
                    // we assume good case. We cannot do much otherwise
                    characteristic.properties = ATT_PROPERTY_INDICATE;
                    status = gatt_client_write_client_characteristic_configuration(&gatt_client_service_packet_handler,
                                                                                   gatt_client->con_handle, &characteristic,
                                                                                   GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
                    query_sent = true;
                    break;
                }

                /* fall through */

            case GATT_CLIENT_SERVICE_DATABASE_HASH_READ_W2_SEND:
                if (gatt_client->gatt_service_database_hash_value_handle != 0){
                    gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DATABASE_HASH_READ_W4_DONE;
                    status = gatt_client_read_value_of_characteristics_by_uuid16(&gatt_client_service_packet_handler,
                                                                                         gatt_client->con_handle,
                                                                                         0x0001, 0xffff, ORG_BLUETOOTH_CHARACTERISTIC_DATABASE_HASH);
                    query_sent = true;
                    break;
                }

                /* fall through */

            case GATT_CLIENT_SERVICE_DATABASE_HASH_WRITE_CCCD_W2_SEND:
                if (gatt_client->gatt_service_database_hash_value_handle != 0) {
                    gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DATABASE_HASH_WRITE_CCCD_W4_DONE;
                    characteristic.value_handle = gatt_client->gatt_service_database_hash_value_handle;
                    characteristic.end_handle = gatt_client->gatt_service_database_hash_end_handle;
                    // we assume good case. We cannot do much otherwise
                    characteristic.properties = ATT_PROPERTY_INDICATE;
                    status = gatt_client_write_client_characteristic_configuration(&gatt_client_service_packet_handler,
                                                                                   gatt_client->con_handle,
                                                                                   &characteristic,
                                                                                   GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION);
                    query_sent = true;
                    break;
                }

                // DONE
                gatt_client->gatt_service_state = GATT_CLIENT_SERVICE_DONE;
                break;
            default:
                break;
        }
        btstack_assert(status == ERROR_CODE_SUCCESS);
        UNUSED(status);
        if (query_sent){
            continue;
        }
#endif

#ifdef ENABLE_GATT_OVER_EATT
        query_sent = gatt_client_le_enhanced_handle_can_send_query(gatt_client);
        if (query_sent){
            continue;
        }
#endif
        btstack_context_callback_registration_t * callback = (btstack_context_callback_registration_t *) btstack_linked_list_pop(&gatt_client->query_requests);
        if (callback == NULL) {
            return;
        }
        (*callback->callback)(callback->context);
    }
}

// test if notification/indication should be delivered to application (BLESA)
static bool gatt_client_accept_server_message(gatt_client_t *gatt_client) {
    // ignore messages until re-encryption is complete
    if (gap_reconnect_security_setup_active(gatt_client->con_handle)) return false;

	// after that ignore if bonded but not encrypted
	return !gap_bonded(gatt_client->con_handle) || (gap_encryption_key_size(gatt_client->con_handle) > 0);
}

static void emit_event_new(btstack_packet_handler_t callback, uint8_t * packet, uint16_t size){
    if (!callback) return;
    hci_dump_btstack_event(packet, size);
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

static void emit_gatt_complete_event(gatt_client_t * gatt_client, uint8_t att_status){
    // @format H122
    uint8_t packet[9];
    hci_event_builder_context_t context;
    hci_event_builder_init(&context, packet, sizeof(packet), GATT_EVENT_QUERY_COMPLETE, 0);
    hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
    hci_event_builder_add_16(&context, gatt_client->service_id);
    hci_event_builder_add_16(&context, gatt_client->connection_id);
    hci_event_builder_add_08(&context, att_status);
    emit_event_new(gatt_client->callback, packet, hci_event_builder_get_length(&context));
}

static void emit_gatt_service_query_result_event(gatt_client_t * gatt_client, uint16_t start_group_handle, uint16_t end_group_handle, const uint8_t * uuid128){
    // @format H22X
    uint8_t packet[28];
    hci_event_builder_context_t context;
    hci_event_builder_init(&context, packet, sizeof(packet), GATT_EVENT_SERVICE_QUERY_RESULT, 0);
    hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
    hci_event_builder_add_16(&context, gatt_client->service_id);
    hci_event_builder_add_16(&context, gatt_client->connection_id);
    hci_event_builder_add_16(&context, start_group_handle);
    hci_event_builder_add_16(&context, end_group_handle);
    hci_event_builder_add_128(&context, uuid128);
    emit_event_new(gatt_client->callback, packet, hci_event_builder_get_length(&context));
}

static void emit_gatt_included_service_query_result_event(gatt_client_t * gatt_client, uint16_t include_handle, uint16_t start_group_handle, uint16_t end_group_handle, const uint8_t * uuid128){
    // @format H22X
    uint8_t packet[30];
    hci_event_builder_context_t context;
    hci_event_builder_init(&context, packet, sizeof(packet), GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT, 0);
    hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
    hci_event_builder_add_16(&context, gatt_client->service_id);
    hci_event_builder_add_16(&context, gatt_client->connection_id);
    hci_event_builder_add_16(&context, include_handle);
    hci_event_builder_add_16(&context, start_group_handle);
    hci_event_builder_add_16(&context, end_group_handle);
    hci_event_builder_add_128(&context, uuid128);
    emit_event_new(gatt_client->callback, packet, hci_event_builder_get_length(&context));
}

static void emit_gatt_characteristic_query_result_event(gatt_client_t * gatt_client, uint16_t start_handle, uint16_t value_handle, uint16_t end_handle,
                                                        uint16_t properties, const uint8_t * uuid128){
    // @format H22Y
    uint8_t packet[32];
    hci_event_builder_context_t context;
    hci_event_builder_init(&context, packet, sizeof(packet), GATT_EVENT_CHARACTERISTIC_QUERY_RESULT, 0);
    hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
    hci_event_builder_add_16(&context, gatt_client->service_id);
    hci_event_builder_add_16(&context, gatt_client->connection_id);
    hci_event_builder_add_16(&context, start_handle);
    hci_event_builder_add_16(&context, value_handle);
    hci_event_builder_add_16(&context, end_handle);
    hci_event_builder_add_16(&context, properties);
    hci_event_builder_add_128(&context, uuid128);
    emit_event_new(gatt_client->callback, packet, hci_event_builder_get_length(&context));
}

static void emit_gatt_all_characteristic_descriptors_result_event(
        gatt_client_t * gatt_client, uint16_t descriptor_handle, const uint8_t * uuid128){
    // @format H22Z
    uint8_t packet[26];
    hci_event_builder_context_t context;
    hci_event_builder_init(&context, packet, sizeof(packet), GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT, 0);
    hci_event_builder_add_con_handle(&context, gatt_client->con_handle);
    hci_event_builder_add_16(&context, gatt_client->service_id);
    hci_event_builder_add_16(&context, gatt_client->connection_id);
    hci_event_builder_add_16(&context, descriptor_handle);
    hci_event_builder_add_128(&context, uuid128);
    emit_event_new(gatt_client->callback, packet, hci_event_builder_get_length(&context));
}

static void emit_gatt_mtu_exchanged_result_event(gatt_client_t * gatt_client, uint16_t new_mtu){
    // @format H2
    uint8_t packet[6];
    packet[0] = GATT_EVENT_MTU;
    packet[1] = sizeof(packet) - 2u;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    little_endian_store_16(packet, 4, new_mtu);
    att_dispatch_client_mtu_exchanged(gatt_client->con_handle, new_mtu);
    emit_event_new(gatt_client->callback, packet, sizeof(packet));
}

// helper
static void gatt_client_handle_transaction_complete(gatt_client_t *gatt_client, uint8_t att_status) {
    gatt_client->state = P_READY;
    gatt_client_timeout_stop(gatt_client);
    emit_gatt_complete_event(gatt_client, att_status);
    gatt_client_notify_can_send_query(gatt_client);
}

// @return packet pointer
// @note assume that value is part of an l2cap buffer - overwrite HCI + L2CAP packet headers + 4 pre_buffer bytes
#define CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE 12
static uint8_t *
setup_characteristic_value_packet(const gatt_client_t *gatt_client, uint8_t type, uint16_t attribute_handle,
                                  uint8_t *value, uint16_t length, uint16_t service_id, uint16_t connection_id) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // copy value into test packet for testing
    static uint8_t packet[1000];
    memcpy(&packet[CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE], value, length);
#else
    // before the value inside the ATT PDU
    uint8_t * packet = value - CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE;
#endif
    packet[0] = type;
    packet[1] = CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE - 2 + length;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    little_endian_store_16(packet, 4, service_id);
    little_endian_store_16(packet, 6, connection_id);
    little_endian_store_16(packet, 8, attribute_handle);
    little_endian_store_16(packet, 10, length);
    return packet;
}

// @return packet pointer
// @note assume that value is part of an l2cap buffer - overwrite HCI + L2CAP packet headers + 6 pre_buffer bytes
#define LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE 14

// L2CAP Header (4) + ACL Header (4) => 8 bytes
#if !defined(HCI_INCOMING_PRE_BUFFER_SIZE) || ((HCI_INCOMING_PRE_BUFFER_SIZE < LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE - 8))
#error "Long Characteristic reads requires HCI_INCOMING_PRE_BUFFER_SIZE >= 6"
#endif

static uint8_t *
setup_long_characteristic_value_packet(const gatt_client_t *gatt_client, uint8_t type, uint16_t attribute_handle,
                                       uint16_t offset, uint8_t *value, uint16_t length) {
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // avoid using pre ATT headers.
    // copy value into test packet for testing
    static uint8_t packet[1000];
    memcpy(&packet[LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE], value, length);
#else
    // before the value inside the ATT PDU
    uint8_t * packet = value - LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE;
#endif
    packet[0] = type;
    packet[1] = LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE - 2 + length;
    little_endian_store_16(packet, 2, gatt_client->con_handle);
    little_endian_store_16(packet, 4, gatt_client->service_id);
    little_endian_store_16(packet, 6, gatt_client->connection_id);
    little_endian_store_16(packet, 8, attribute_handle);
    little_endian_store_16(packet, 10, offset);
    little_endian_store_16(packet, 12, length);
    return packet;
}

#if (LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE > CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE)
#define REPORT_PREBUFFER_HEADER LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE
#else
#define REPORT_PREBUFFER_HEADER CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE
#endif

///
static void report_gatt_services(gatt_client_t * gatt_client, uint8_t * packet, uint16_t size){
    if (size < 2) return;
    uint8_t attr_length = packet[1];
    uint8_t uuid_length = attr_length - 4u;

    int i;
    for (i = 2; (i+attr_length) <= size; i += attr_length){
        uint16_t start_group_handle = little_endian_read_16(packet,i);
        uint16_t end_group_handle   = little_endian_read_16(packet,i+2);
        uint8_t  uuid128[16];
        uint16_t uuid16 = 0;

        if (uuid_length == 2u){
            uuid16 = little_endian_read_16(packet, i+4);
            uuid_add_bluetooth_prefix((uint8_t*) &uuid128, uuid16);
        } else if (uuid_length == 16u) {
            reverse_128(&packet[i+4], uuid128);
        } else {
            return;
        }
        emit_gatt_service_query_result_event(gatt_client, start_group_handle, end_group_handle, uuid128);
    }
}

static void report_gatt_characteristic_start_found(gatt_client_t * gatt_client, uint16_t start_handle, uint8_t properties, uint16_t value_handle, uint8_t * uuid, uint16_t uuid_length){
    uint8_t uuid128[16];
    uint16_t uuid16 = 0;
    if (uuid_length == 2u){
        uuid16 = little_endian_read_16(uuid, 0);
        uuid_add_bluetooth_prefix((uint8_t*) uuid128, uuid16);
    } else if (uuid_length == 16u){
        reverse_128(uuid, uuid128);
    } else {
        return;
    }

    if (gatt_client->filter_with_uuid && (memcmp(gatt_client->uuid128, uuid128, 16) != 0)) return;

    gatt_client->characteristic_properties = properties;
    gatt_client->characteristic_start_handle = start_handle;
    gatt_client->attribute_handle = value_handle;

    if (gatt_client->filter_with_uuid) return;

    gatt_client->uuid16 = uuid16;
    (void)memcpy(gatt_client->uuid128, uuid128, 16);
}

static void report_gatt_characteristic_end_found(gatt_client_t * gatt_client, uint16_t end_handle){
    // TODO: stop searching if filter and uuid found

    if (!gatt_client->characteristic_start_handle) return;

    emit_gatt_characteristic_query_result_event(gatt_client, gatt_client->characteristic_start_handle, gatt_client->attribute_handle,
                                                end_handle, gatt_client->characteristic_properties, gatt_client->uuid128);

    gatt_client->characteristic_start_handle = 0;
}


static void report_gatt_characteristics(gatt_client_t * gatt_client, uint8_t * packet, uint16_t size){
    if (size < 2u) return;
    uint8_t attr_length = packet[1];
    if ((attr_length != 7u) && (attr_length != 21u)) return;
    uint8_t uuid_length = attr_length - 5u;
    int i;
    for (i = 2u; (i + attr_length) <= size; i += attr_length){
        uint16_t start_handle = little_endian_read_16(packet, i);
        uint8_t  properties = packet[i+2];
        uint16_t value_handle = little_endian_read_16(packet, i+3);
        report_gatt_characteristic_end_found(gatt_client, start_handle - 1u);
        report_gatt_characteristic_start_found(gatt_client, start_handle, properties, value_handle, &packet[i + 5],
                                               uuid_length);
    }
}

static void report_gatt_included_service_uuid16(gatt_client_t * gatt_client, uint16_t include_handle, uint16_t uuid16){
    uint8_t normalized_uuid128[16];
    uuid_add_bluetooth_prefix(normalized_uuid128, uuid16);
    emit_gatt_included_service_query_result_event(gatt_client, include_handle, gatt_client->query_start_handle,
                                                  gatt_client->query_end_handle, normalized_uuid128);
}

static void report_gatt_included_service_uuid128(gatt_client_t * gatt_client, uint16_t include_handle, const uint8_t * uuid128){
    emit_gatt_included_service_query_result_event(gatt_client, include_handle, gatt_client->query_start_handle,
                                                  gatt_client->query_end_handle, uuid128);
}

static void report_gatt_characteristic_value_change(gatt_client_t *gatt_client, uint8_t event_type, uint16_t value_handle, uint8_t *value, int length) {
    uint8_t * packet;

    // Single Characteristic listener, setup packet with service + connection id = 0
    packet = setup_characteristic_value_packet(gatt_client, event_type, value_handle, value, length, 0, 0);
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client_value_listeners);
    while (btstack_linked_list_iterator_has_next(&it)) {
        gatt_client_notification_t *notification = (gatt_client_notification_t *) btstack_linked_list_iterator_next(&it);
        if ((notification->con_handle != GATT_CLIENT_ANY_CONNECTION) && (notification->con_handle != gatt_client->con_handle))    continue;
        if ((notification->attribute_handle != GATT_CLIENT_ANY_VALUE_HANDLE) && (notification->attribute_handle != value_handle)) continue;

        (*notification->callback)(HCI_EVENT_PACKET, 0, packet, CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE + length);
    }

    // Service characteristics
    btstack_linked_list_iterator_init(&it, &gatt_client_service_value_listeners);
    while (btstack_linked_list_iterator_has_next(&it)){
        const gatt_client_service_notification_t * notification = (gatt_client_service_notification_t*) btstack_linked_list_iterator_next(&it);
        if (notification->con_handle         != gatt_client->con_handle) continue;
        if (notification->start_group_handle  > value_handle) continue;
        if (notification->end_group_handle    < value_handle) continue;
        // (re)setup value packet with service and connection id (to avoid patching event later)
        packet = setup_characteristic_value_packet(gatt_client, event_type, value_handle, value, length, notification->service_id, notification->connection_id);
        (*notification->callback)(HCI_EVENT_PACKET, 0, packet, CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE + length);
    }
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_notification(gatt_client_t *gatt_client, uint16_t value_handle, uint8_t *value, int length) {
    if (!gatt_client_accept_server_message(gatt_client)) return;
    report_gatt_characteristic_value_change(gatt_client, GATT_EVENT_NOTIFICATION, value_handle, value, length);
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_indication(gatt_client_t *gatt_client, uint16_t value_handle, uint8_t *value, int length) {
	if (!gatt_client_accept_server_message(gatt_client)) return;
#ifdef ENABLE_GATT_CLIENT_SERVICE_CHANGED
    // Directly Handle GATT Service Changed and Database Hash indications
    if (value_handle == gatt_client->gatt_service_database_hash_value_handle){
        gatt_client_service_emit_database_hash(gatt_client, value, length);
    }
    if (value_handle == gatt_client->gatt_service_changed_value_handle){
        gatt_client_service_emit_service_changed(gatt_client, value, length);
    }
#endif
    report_gatt_characteristic_value_change(gatt_client, GATT_EVENT_INDICATION, value_handle, value, length);
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_characteristic_value(gatt_client_t * gatt_client, uint16_t attribute_handle, uint8_t * value, uint16_t length){
    uint8_t * packet = setup_characteristic_value_packet(
            gatt_client, GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT, attribute_handle, value, length, gatt_client->service_id, gatt_client->connection_id);
    emit_event_new(gatt_client->callback, packet, CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE + length);
}

// @note assume that value is part of an l2cap buffer - overwrite parts of the HCI/L2CAP/ATT packet (4/4/3) bytes 
static void report_gatt_long_characteristic_value_blob(gatt_client_t * gatt_client, uint16_t attribute_handle, uint8_t * blob, uint16_t blob_length, int value_offset){
    uint8_t * packet = setup_long_characteristic_value_packet(gatt_client,
                                                              GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT,
                                                              attribute_handle, value_offset,
                                                              blob, blob_length);
    emit_event_new(gatt_client->callback, packet, blob_length + LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE);
}

static void report_gatt_characteristic_descriptor(gatt_client_t * gatt_client, uint16_t descriptor_handle, uint8_t *value, uint16_t value_length, uint16_t value_offset){
    UNUSED(value_offset);
    uint8_t * packet = setup_characteristic_value_packet(gatt_client, GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT,
                                                         descriptor_handle, value,
                                                         value_length, gatt_client->service_id, gatt_client->connection_id);
    emit_event_new(gatt_client->callback, packet, value_length + 8u);
}

static void report_gatt_long_characteristic_descriptor(gatt_client_t * gatt_client, uint16_t descriptor_handle, uint8_t *blob, uint16_t blob_length, uint16_t value_offset){
    uint8_t * packet = setup_long_characteristic_value_packet(gatt_client,
                                                              GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT,
                                                              descriptor_handle, value_offset,
                                                              blob, blob_length);
    emit_event_new(gatt_client->callback, packet, blob_length + LONG_CHARACTERISTIC_VALUE_EVENT_HEADER_SIZE);
}

static void report_gatt_all_characteristic_descriptors(gatt_client_t * gatt_client, uint8_t * packet, uint16_t size, uint16_t pair_size){
    int i;
    for (i = 0u; (i + pair_size) <= size; i += pair_size){
        uint16_t descriptor_handle = little_endian_read_16(packet,i);
        uint8_t uuid128[16];
        uint16_t uuid16 = 0;
        if (pair_size == 4u){
            uuid16 = little_endian_read_16(packet,i+2);
            uuid_add_bluetooth_prefix(uuid128, uuid16);
        } else {
            reverse_128(&packet[i+2], uuid128);
        }        
        emit_gatt_all_characteristic_descriptors_result_event(gatt_client, descriptor_handle, uuid128);
    }
    
}

static bool is_query_done(gatt_client_t * gatt_client, uint16_t last_result_handle){
    return last_result_handle >= gatt_client->end_group_handle;
}

static void trigger_next_query(gatt_client_t * gatt_client, uint16_t last_result_handle, gatt_client_state_t next_query_state){
    if (is_query_done(gatt_client, last_result_handle)){
        gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
        return;
    }
    // next
    gatt_client->start_group_handle = last_result_handle + 1u;
    gatt_client->state = next_query_state;
}

static void trigger_next_included_service_query(gatt_client_t * gatt_client, uint16_t last_result_handle){
    trigger_next_query(gatt_client, last_result_handle, P_W2_SEND_INCLUDED_SERVICE_QUERY);
}

static void trigger_next_service_query(gatt_client_t * gatt_client, uint16_t last_result_handle){
    trigger_next_query(gatt_client, last_result_handle, P_W2_SEND_SERVICE_QUERY);
}

static void trigger_next_service_by_uuid_query(gatt_client_t * gatt_client, uint16_t last_result_handle){
    trigger_next_query(gatt_client, last_result_handle, P_W2_SEND_SERVICE_WITH_UUID_QUERY);
}

static void trigger_next_characteristic_query(gatt_client_t * gatt_client, uint16_t last_result_handle){
    if (is_query_done(gatt_client, last_result_handle)){
        // report last characteristic
        report_gatt_characteristic_end_found(gatt_client, gatt_client->end_group_handle);
    }
    trigger_next_query(gatt_client, last_result_handle, P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY);
}

static void trigger_next_characteristic_descriptor_query(gatt_client_t * gatt_client, uint16_t last_result_handle){
    trigger_next_query(gatt_client, last_result_handle, P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY);
}

static void trigger_next_read_by_type_query(gatt_client_t * gatt_client, uint16_t last_result_handle){
    trigger_next_query(gatt_client, last_result_handle, P_W2_SEND_READ_BY_TYPE_REQUEST);
}

static void trigger_next_prepare_write_query(gatt_client_t * gatt_client, gatt_client_state_t next_query_state, gatt_client_state_t done_state){
    gatt_client->attribute_offset += write_blob_length(gatt_client);
    uint16_t next_blob_length =  write_blob_length(gatt_client);
    
    if (next_blob_length == 0u){
        gatt_client->state = done_state;
        return;
    }
    gatt_client->state = next_query_state;
}

static void trigger_next_blob_query(gatt_client_t * gatt_client, gatt_client_state_t next_query_state, uint16_t received_blob_length){

    uint16_t max_blob_length = gatt_client->mtu - 1u;
    if (received_blob_length < max_blob_length){
        gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
        return;
    }

    gatt_client->attribute_offset += received_blob_length;
    gatt_client->state = next_query_state;
}

void gatt_client_listen_for_characteristic_value_updates(gatt_client_notification_t * notification, btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    notification->callback = callback;
    notification->con_handle = con_handle;
    if (characteristic == NULL){
        notification->attribute_handle = GATT_CLIENT_ANY_VALUE_HANDLE;
    } else {
        notification->attribute_handle = characteristic->value_handle;
    }
    btstack_linked_list_add(&gatt_client_value_listeners, (btstack_linked_item_t*) notification);
}

void gatt_client_stop_listening_for_characteristic_value_updates(gatt_client_notification_t * notification){
    btstack_linked_list_remove(&gatt_client_value_listeners, (btstack_linked_item_t*) notification);
}

void gatt_client_listen_for_service_characteristic_value_updates(gatt_client_service_notification_t * notification,
                                                                 btstack_packet_handler_t callback,
                                                                 hci_con_handle_t con_handle,
                                                                 gatt_client_service_t * service,
                                                                 uint16_t service_id,
                                                                 uint16_t connection_id){
    notification->callback = callback;
    notification->con_handle = con_handle;
    notification->start_group_handle = service->start_group_handle;
    notification->end_group_handle = service->end_group_handle;
    notification->service_id = service_id;
    notification->connection_id = connection_id;
    btstack_linked_list_add(&gatt_client_service_value_listeners, (btstack_linked_item_t*) notification);
}

/**
 * @brief Stop listening to characteristic value updates for registered service with
 * the gatt_client_listen_for_characteristic_value_updates function.
 * @param notification struct used in gatt_client_listen_for_characteristic_value_updates
 */
void gatt_client_stop_listening_for_service_characteristic_value_updates(gatt_client_service_notification_t * notification){
    btstack_linked_list_remove(&gatt_client_service_value_listeners, (btstack_linked_item_t*) notification);
}

static bool is_value_valid(gatt_client_t *gatt_client, uint8_t *packet, uint16_t size){
    uint16_t attribute_handle = little_endian_read_16(packet, 1);
    uint16_t value_offset = little_endian_read_16(packet, 3);
    
    if (gatt_client->attribute_handle != attribute_handle) return false;
    if (gatt_client->attribute_offset != value_offset) return false;
    return memcmp(&gatt_client->attribute_value[gatt_client->attribute_offset], &packet[5], size - 5u) == 0u;
}

#ifdef ENABLE_LE_SIGNED_WRITE
static void gatt_client_run_for_client_start_signed_write(gatt_client_t *gatt_client) {
    sm_key_t csrk;
    le_device_db_local_csrk_get(gatt_client->le_device_index, csrk);
    uint32_t sign_counter = le_device_db_local_counter_get(gatt_client->le_device_index);
    gatt_client->state = P_W4_CMAC_RESULT;
    sm_cmac_signed_write_start(csrk, ATT_SIGNED_WRITE_COMMAND, gatt_client->attribute_handle, gatt_client->attribute_length, gatt_client->attribute_value, sign_counter, att_signed_write_handle_cmac_result);
}
#endif

// returns true if packet was sent
static bool gatt_client_run_for_gatt_client(gatt_client_t * gatt_client){

    // wait until re-encryption is complete
    if (gap_reconnect_security_setup_active(gatt_client->con_handle)) return false;

    // wait until re-encryption is complete
    if (gatt_client->reencryption_active) return false;

    // wait until pairing complete (either reactive authentication or due to required security level)
    if (gatt_client->wait_for_authentication_complete) return false;

    bool client_request_pending = gatt_client->state != P_READY;

    // verify security level for Mandatory Authentication over LE
    bool check_security;
    switch (gatt_client->bearer_type){
        case ATT_BEARER_UNENHANCED_LE:
            check_security = true;
            break;
        default:
            check_security = false;
            break;
    }
    if (client_request_pending && (gatt_client_required_security_level > gatt_client->security_level) && check_security){
        log_info("Trigger pairing, current security level %u, required %u\n", gatt_client->security_level, gatt_client_required_security_level);
        gatt_client->wait_for_authentication_complete = true;
        // set att error code for pairing failure based on required level
        switch (gatt_client_required_security_level){
            case LEVEL_4:
            case LEVEL_3:
                gatt_client->pending_error_code = ATT_ERROR_INSUFFICIENT_AUTHENTICATION;
                break;
            default:
                gatt_client->pending_error_code = ATT_ERROR_INSUFFICIENT_ENCRYPTION;
                break;
        }
        sm_request_pairing(gatt_client->con_handle);
        // sm probably just sent a pdu
        return true;
    }

    switch (gatt_client->mtu_state) {
        case SEND_MTU_EXCHANGE:
            gatt_client->mtu_state = SENT_MTU_EXCHANGE;
            att_exchange_mtu_request(gatt_client);
            return true;
        case SENT_MTU_EXCHANGE:
            return false;
        default:
            break;
    }

    if (gatt_client->send_confirmation){
        gatt_client->send_confirmation = false;
        att_confirmation(gatt_client);
        return true;
    }

    // check MTU for writes
    switch (gatt_client->state){
        case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
        case P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR:
            if (gatt_client->attribute_length <= (gatt_client->mtu - 3u)) break;
            log_error("gatt_client_run: value len %u > MTU %u - 3\n", gatt_client->attribute_length,gatt_client->mtu);
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH);
            return false;
        default:
            break;
    }

    bool packet_sent = true;
    bool done = true;
    switch (gatt_client->state){
        case P_W2_SEND_SERVICE_QUERY:
            gatt_client->state = P_W4_SERVICE_QUERY_RESULT;
            send_gatt_services_request(gatt_client);
            break;

        case P_W2_SEND_SERVICE_WITH_UUID_QUERY:
            gatt_client->state = P_W4_SERVICE_WITH_UUID_RESULT;
            send_gatt_services_by_uuid_request(gatt_client);
            break;

        case P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY:
            gatt_client->state = P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT;
            send_gatt_characteristic_request(gatt_client);
            break;

        case P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY:
            gatt_client->state = P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT;
            send_gatt_characteristic_request(gatt_client);
            break;

        case P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY:
            gatt_client->state = P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT;
            send_gatt_characteristic_descriptor_request(gatt_client);
            break;

        case P_W2_SEND_INCLUDED_SERVICE_QUERY:
            gatt_client->state = P_W4_INCLUDED_SERVICE_QUERY_RESULT;
            send_gatt_included_service_request(gatt_client);
            break;

        case P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY:
            gatt_client->state = P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT;
            send_gatt_included_service_uuid_request(gatt_client);
            break;

        case P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY:
            gatt_client->state = P_W4_READ_CHARACTERISTIC_VALUE_RESULT;
            send_gatt_read_characteristic_value_request(gatt_client);
            break;
            
        case P_W2_SEND_READ_BLOB_QUERY:
            gatt_client->state = P_W4_READ_BLOB_RESULT;
            send_gatt_read_blob_request(gatt_client);
            break;

        case P_W2_SEND_READ_BY_TYPE_REQUEST:
            gatt_client->state = P_W4_READ_BY_TYPE_RESPONSE;
            send_gatt_read_by_type_request(gatt_client);
            break;

        case P_W2_SEND_READ_MULTIPLE_REQUEST:
            gatt_client->state = P_W4_READ_MULTIPLE_RESPONSE;
            send_gatt_read_multiple_request(gatt_client);
            break;

#ifdef ENABLE_GATT_OVER_EATT
        case P_W2_SEND_READ_MULTIPLE_VARIABLE_REQUEST:
            gatt_client->state = P_W4_READ_MULTIPLE_VARIABLE_RESPONSE;
            send_gatt_read_multiple_variable_request(gatt_client);
            break;
#endif

        case P_W2_SEND_WRITE_CHARACTERISTIC_VALUE:
            gatt_client->state = P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT;
            send_gatt_write_attribute_value_request(gatt_client);
            break;

        case P_W2_PREPARE_WRITE:
            gatt_client->state = P_W4_PREPARE_WRITE_RESULT;
            send_gatt_prepare_write_request(gatt_client);
            break;

        case P_W2_PREPARE_WRITE_SINGLE:
            gatt_client->state = P_W4_PREPARE_WRITE_SINGLE_RESULT;
            send_gatt_prepare_write_request(gatt_client);
            break;

        case P_W2_PREPARE_RELIABLE_WRITE:
            gatt_client->state = P_W4_PREPARE_RELIABLE_WRITE_RESULT;
            send_gatt_prepare_write_request(gatt_client);
            break;

        case P_W2_EXECUTE_PREPARED_WRITE:
            gatt_client->state = P_W4_EXECUTE_PREPARED_WRITE_RESULT;
            send_gatt_execute_write_request(gatt_client);
            break;

        case P_W2_CANCEL_PREPARED_WRITE:
            gatt_client->state = P_W4_CANCEL_PREPARED_WRITE_RESULT;
            send_gatt_cancel_prepared_write_request(gatt_client);
            break;

        case P_W2_CANCEL_PREPARED_WRITE_DATA_MISMATCH:
            gatt_client->state = P_W4_CANCEL_PREPARED_WRITE_DATA_MISMATCH_RESULT;
            send_gatt_cancel_prepared_write_request(gatt_client);
            break;

#ifdef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
        case P_W2_SEND_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY:    
            // use Find Information
            gatt_client->state = P_W4_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT;
            send_gatt_characteristic_descriptor_request(gatt_client);
#else
        case P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY:
            // Use Read By Type
            gatt_client->state = P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT;
            send_gatt_read_client_characteristic_configuration_request(gatt_client);
#endif
            break;

        case P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY:
            gatt_client->state = P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT;
            send_gatt_read_characteristic_descriptor_request(gatt_client);
            break;

        case P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY:
            gatt_client->state = P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT;
            send_gatt_read_blob_request(gatt_client);
            break;

        case P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR:
            gatt_client->state = P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
            send_gatt_write_attribute_value_request(gatt_client);
            break;

        case P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:
            gatt_client->state = P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT;
            send_gatt_write_client_characteristic_configuration_request(gatt_client);
            break;

        case P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR:
            gatt_client->state = P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
            send_gatt_prepare_write_request(gatt_client);
            break;

        case P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR:
            gatt_client->state = P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT;
            send_gatt_execute_write_request(gatt_client);
            break;

#ifdef ENABLE_LE_SIGNED_WRITE
        case P_W4_IDENTITY_RESOLVING:
            log_info("P_W4_IDENTITY_RESOLVING - state %x", sm_identity_resolving_state(gatt_client->con_handle));
            switch (sm_identity_resolving_state(gatt_client->con_handle)){
                case IRK_LOOKUP_SUCCEEDED:
                    gatt_client->le_device_index = sm_le_device_index(gatt_client->con_handle);
                    gatt_client->state = P_W4_CMAC_READY;
                    if (sm_cmac_ready()){
                        gatt_client_run_for_client_start_signed_write(gatt_client);
                    }
                    break;
                case IRK_LOOKUP_FAILED:
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_BONDING_INFORMATION_MISSING);
                    break;
                default:
                    break;
            }
            packet_sent = false;
            break;

        case P_W4_CMAC_READY:
            if (sm_cmac_ready()){
                gatt_client_run_for_client_start_signed_write(gatt_client);
            }
            packet_sent = false;
            break;

        case P_W2_SEND_SIGNED_WRITE: {
            gatt_client->state = P_W4_SEND_SIGNED_WRITE_DONE;
            // bump local signing counter
            uint32_t sign_counter = le_device_db_local_counter_get(gatt_client->le_device_index);
            le_device_db_local_counter_set(gatt_client->le_device_index, sign_counter + 1);
            // send signed write command
            send_gatt_signed_write_request(gatt_client, sign_counter);
            // finally, notifiy client that write is complete
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
            break;
        }
#endif
        default:
            done = false;
            break;
    }

    if (done){
        return packet_sent;
    }

    // write without response callback
    btstack_context_callback_registration_t * callback =
            (btstack_context_callback_registration_t *) btstack_linked_list_pop(&gatt_client->write_without_response_requests);
    if (callback != NULL){
        (*callback->callback)(callback->context);
        return true;
    }

    // requested can send now old
    if (gatt_client->write_without_response_callback != NULL){
        btstack_packet_handler_t packet_handler = gatt_client->write_without_response_callback;
        gatt_client->write_without_response_callback = NULL;
        uint8_t event[4];
        event[0] = GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE;
        event[1] = sizeof(event) - 2u;
        little_endian_store_16(event, 2, gatt_client->con_handle);
        packet_handler(HCI_EVENT_PACKET, gatt_client->con_handle, event, sizeof(event));
        return true; // to trigger requeueing (even if higher layer didn't sent)
    }

    return false;
}

static void gatt_client_run(void){
    btstack_linked_item_t *it;
    bool packet_sent;
#ifdef ENABLE_GATT_OVER_EATT
    btstack_linked_list_iterator_t it_eatt;
#endif
    for (it = (btstack_linked_item_t *) gatt_client_connections; it != NULL; it = it->next){
        gatt_client_t * gatt_client = (gatt_client_t *) it;
        switch (gatt_client->bearer_type){
            case ATT_BEARER_UNENHANCED_LE:
#ifdef ENABLE_GATT_OVER_EATT
                btstack_linked_list_iterator_init(&it_eatt, &gatt_client->eatt_clients);
                while (btstack_linked_list_iterator_has_next(&it_eatt)) {
                    gatt_client_t * eatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it_eatt);
                    if (eatt_client->state != P_READY){
                        if (att_dispatch_client_can_send_now(gatt_client->con_handle)){
                            gatt_client_run_for_gatt_client(eatt_client);
                        }
                    }
                }
#endif
                if (!att_dispatch_client_can_send_now(gatt_client->con_handle)) {
                    att_dispatch_client_request_can_send_now_event(gatt_client->con_handle);
                    return;
                }
                packet_sent = gatt_client_run_for_gatt_client(gatt_client);
                if (packet_sent){
                    // request new permission
                    att_dispatch_client_request_can_send_now_event(gatt_client->con_handle);
                    // requeue client for fairness and exit
                    // note: iterator has become invalid
                    btstack_linked_list_remove(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
                    btstack_linked_list_add_tail(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
                    return;
                }
                break;
#ifdef ENABLE_GATT_OVER_CLASSIC
            case ATT_BEARER_UNENHANCED_CLASSIC:
                if (gatt_client->con_handle == HCI_CON_HANDLE_INVALID) {
                    continue;
                }

                // handle GATT over BR/EDR
                if (att_dispatch_client_can_send_now(gatt_client->con_handle) == false) {
                    att_dispatch_client_request_can_send_now_event(gatt_client->con_handle);
                    return;
                }
                packet_sent = gatt_client_run_for_gatt_client(gatt_client);
                if (packet_sent){
                    // request new permission
                    att_dispatch_client_request_can_send_now_event(gatt_client->con_handle);
                    // requeue client for fairness and exit
                    // note: iterator has become invalid
                    btstack_linked_list_remove(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
                    btstack_linked_list_add_tail(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
                    return;
                }
                break;
#endif
            default:
                btstack_unreachable();
                break;
        }


    }
}

// emit complete event, used to avoid emitting event from API call
static void gatt_client_emit_events(void * context){
    UNUSED(context);
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) gatt_client_connections; it != NULL; it = it->next) {
        gatt_client_t *gatt_client = (gatt_client_t *) it;
        if (gatt_client->state == P_W2_EMIT_QUERY_COMPLETE_EVENT){
            gatt_client->state = P_READY;
            emit_gatt_complete_event(gatt_client, ATT_ERROR_SUCCESS);
        }
    }
}

static void gatt_client_report_error_if_pending(gatt_client_t *gatt_client, uint8_t att_error_code) {
    if (is_ready(gatt_client)) return;
    gatt_client_handle_transaction_complete(gatt_client, att_error_code);
}

static void gatt_client_handle_reencryption_complete(const uint8_t * packet){
    hci_con_handle_t con_handle = sm_event_reencryption_complete_get_handle(packet);
    gatt_client_t * gatt_client = gatt_client_get_context_for_handle(con_handle);
    if (gatt_client == NULL) return;

    // update security level
    gatt_client->security_level = gatt_client_le_security_level_for_connection(con_handle);

    gatt_client->reencryption_result = sm_event_reencryption_complete_get_status(packet);
    gatt_client->reencryption_active = false;
    gatt_client->wait_for_authentication_complete = false;

    if (gatt_client->state == P_READY) return;

    switch (sm_event_reencryption_complete_get_status(packet)){
        case ERROR_CODE_SUCCESS:
            log_info("re-encryption success, retry operation");
            break;
        case ERROR_CODE_AUTHENTICATION_FAILURE:
        case ERROR_CODE_PIN_OR_KEY_MISSING:
#if defined(ENABLE_GATT_CLIENT_PAIRING) && !defined(ENABLE_LE_PROACTIVE_AUTHENTICATION)
            if (gatt_client_required_security_level == LEVEL_0) {
                // re-encryption failed for reactive authentication with pairing and we have a pending client request
                // => try to resolve it by deleting bonding information if we started pairing before
                // delete bonding information
                int le_device_db_index = sm_le_device_index(gatt_client->con_handle);
                btstack_assert(le_device_db_index >= 0);
                log_info("reactive auth with pairing: delete bonding and start pairing");
#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
                hci_remove_le_device_db_entry_from_resolving_list((uint16_t) le_device_db_index);
#endif
                le_device_db_remove(le_device_db_index);
                // trigger pairing again
                sm_request_pairing(gatt_client->con_handle);
                break;
            }
#endif
            // report bonding information missing
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_BONDING_INFORMATION_MISSING);
            break;
        default:
            // report bonding information missing
            gatt_client_handle_transaction_complete(gatt_client, gatt_client->pending_error_code);
            break;
    }
}

static void gatt_client_handle_disconnection_complete(const uint8_t * packet){
    log_info("GATT Client: HCI_EVENT_DISCONNECTION_COMPLETE");
    hci_con_handle_t con_handle = little_endian_read_16(packet,3);
    gatt_client_t * gatt_client = gatt_client_get_context_for_handle(con_handle);
    if (gatt_client == NULL) return;

    gatt_client_report_error_if_pending(gatt_client, ATT_ERROR_HCI_DISCONNECT_RECEIVED);
    gatt_client_timeout_stop(gatt_client);
    btstack_linked_list_remove(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
    btstack_memory_gatt_client_free(gatt_client);
}

static void gatt_client_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);    // ok: handling own l2cap events
    UNUSED(size);       // ok: there is no channel
    
    if (packet_type != HCI_EVENT_PACKET) return;

    hci_con_handle_t con_handle;
    gatt_client_t * gatt_client;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            gatt_client_handle_disconnection_complete(packet);
            break;

        // Pairing complete (with/without bonding=storing of pairing information)
        case SM_EVENT_PAIRING_COMPLETE:
            con_handle = sm_event_pairing_complete_get_handle(packet);
            gatt_client = gatt_client_get_context_for_handle(con_handle);
            if (gatt_client == NULL) break;

            // update security level
            gatt_client->security_level = gatt_client_le_security_level_for_connection(con_handle);

            if (gatt_client->wait_for_authentication_complete){
                gatt_client->wait_for_authentication_complete = false;
                if (sm_event_pairing_complete_get_status(packet) != ERROR_CODE_SUCCESS){
                    log_info("pairing failed, report previous error 0x%x", gatt_client->pending_error_code);
                    gatt_client_report_error_if_pending(gatt_client, gatt_client->pending_error_code);
                } else {
                    log_info("pairing success, retry operation");
                }
            }
            break;

#ifdef ENABLE_LE_SIGNED_WRITE
        // Identity Resolving completed (no code, gatt_client_run will continue)
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            break;
#endif

        // re-encryption started
        case SM_EVENT_REENCRYPTION_STARTED:
            con_handle = sm_event_reencryption_complete_get_handle(packet);
            gatt_client = gatt_client_get_context_for_handle(con_handle);
            if (gatt_client == NULL) break;

            gatt_client->reencryption_active = true;
            gatt_client->reencryption_result = ERROR_CODE_SUCCESS;
            break;

        // re-encryption complete
        case SM_EVENT_REENCRYPTION_COMPLETE:
            gatt_client_handle_reencryption_complete(packet);
            break;
        default:
            break;
    }

    gatt_client_run();
}

static void gatt_client_handle_att_read_response(gatt_client_t *gatt_client, uint8_t *packet, uint16_t size) {
    switch (gatt_client->state) {
        case P_W4_INCLUDED_SERVICE_UUID_WITH_QUERY_RESULT:
            if (size >= 17) {
                uint8_t uuid128[16];
                reverse_128(&packet[1], uuid128);
                report_gatt_included_service_uuid128(gatt_client, gatt_client->start_group_handle, uuid128);
            }
            trigger_next_included_service_query(gatt_client, gatt_client->start_group_handle);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;

        case P_W4_READ_CHARACTERISTIC_VALUE_RESULT:
            report_gatt_characteristic_value(gatt_client, gatt_client->attribute_handle, &packet[1], size - 1u);
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
            break;

        case P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT:
            report_gatt_characteristic_descriptor(gatt_client, gatt_client->attribute_handle, &packet[1],
                                                  size - 1u, 0u);
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
            break;

            // Use ATT_READ_REQUEST for first blob of Read Long Characteristic
        case P_W4_READ_BLOB_RESULT:
            report_gatt_long_characteristic_value_blob(gatt_client, gatt_client->attribute_handle, &packet[1],
                                                       size - 1u, gatt_client->attribute_offset);
            trigger_next_blob_query(gatt_client, P_W2_SEND_READ_BLOB_QUERY, size - 1u);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;

            // Use ATT_READ_REQUEST for first blob of Read Long Characteristic Descriptor
        case P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT:
            report_gatt_long_characteristic_descriptor(gatt_client, gatt_client->attribute_handle, &packet[1],
                                                       size - 1u, gatt_client->attribute_offset);
            trigger_next_blob_query(gatt_client, P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY,
                                    size - 1u);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;

        default:
            break;
    }
}

static void gatt_client_handle_att_read_by_type_response(gatt_client_t *gatt_client, uint8_t *packet, uint16_t size) {
    switch (gatt_client->state) {
        case P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT:
            report_gatt_characteristics(gatt_client, packet, size);
            trigger_next_characteristic_query(gatt_client,
                                              get_last_result_handle_from_characteristics_list(packet, size));
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done, or by ATT_ERROR
            break;
        case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:
            report_gatt_characteristics(gatt_client, packet, size);
            trigger_next_characteristic_query(gatt_client,
                                              get_last_result_handle_from_characteristics_list(packet, size));
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done, or by ATT_ERROR
            break;
        case P_W4_INCLUDED_SERVICE_QUERY_RESULT: {
            if (size < 2u) break;
            uint16_t uuid16 = 0;
            uint16_t pair_size = packet[1];

            if (pair_size == 6u) {
                if (size < 8u) break;
                // UUIDs not available, query first included service
                gatt_client->start_group_handle = little_endian_read_16(packet, 2); // ready for next query
                gatt_client->query_start_handle = little_endian_read_16(packet, 4);
                gatt_client->query_end_handle = little_endian_read_16(packet, 6);
                gatt_client->state = P_W2_SEND_INCLUDED_SERVICE_WITH_UUID_QUERY;
                break;
            }

            if (pair_size != 8u) break;

            // UUIDs included, report all of them
            uint16_t offset;
            for (offset = 2u; (offset + 8u) <= size; offset += pair_size) {
                uint16_t include_handle = little_endian_read_16(packet, offset);
                gatt_client->query_start_handle = little_endian_read_16(packet, offset + 2u);
                gatt_client->query_end_handle = little_endian_read_16(packet, offset + 4u);
                uuid16 = little_endian_read_16(packet, offset + 6u);
                report_gatt_included_service_uuid16(gatt_client, include_handle, uuid16);
            }

            trigger_next_included_service_query(gatt_client,
                                                get_last_result_handle_from_included_services_list(packet,
                                                                                                   size));
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;
        }
#ifndef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
        case P_W4_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT:
            gatt_client->client_characteristic_configuration_handle = little_endian_read_16(packet, 2);
            gatt_client->state = P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
            break;
#endif
        case P_W4_READ_BY_TYPE_RESPONSE: {
            uint16_t pair_size = packet[1];
            // set last result handle to last valid handle, only used if pair_size invalid
            uint16_t last_result_handle = 0xffff;
            if (pair_size > 2) {
                uint16_t offset;
                for (offset = 2; offset < size; offset += pair_size) {
                    uint16_t value_handle = little_endian_read_16(packet, offset);
                    report_gatt_characteristic_value(gatt_client, value_handle, &packet[offset + 2u],
                                                     pair_size - 2u);
                    last_result_handle = value_handle;
                }
            }
            trigger_next_read_by_type_query(gatt_client, last_result_handle);
            break;
        }
        default:
            break;
    }
}

static void gatt_client_handle_att_write_response(gatt_client_t *gatt_client) {
    switch (gatt_client->state) {
        case P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
            break;
        case P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT:
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
            break;
        case P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
            break;
        default:
            break;
    }
}

static void gatt_client_handle_att_response(gatt_client_t * gatt_client, uint8_t * packet, uint16_t size) {
    uint8_t att_status;
    switch (packet[0]) {
        case ATT_EXCHANGE_MTU_RESPONSE: {
            if (size < 3u) break;
            bool update_gatt_server_att_mtu = false;
            uint16_t remote_rx_mtu = little_endian_read_16(packet, 1);
            uint16_t local_rx_mtu = l2cap_max_le_mtu();
            switch (gatt_client->bearer_type){
                case ATT_BEARER_UNENHANCED_LE:
                    update_gatt_server_att_mtu = true;
                    break;
#ifdef ENABLE_GATT_OVER_CLASSIC
                case ATT_BEARER_UNENHANCED_CLASSIC:
                    local_rx_mtu = gatt_client->mtu;
                    break;
#endif
                default:
                    btstack_unreachable();
                    break;
            }

            uint16_t mtu = (remote_rx_mtu < local_rx_mtu) ? remote_rx_mtu : local_rx_mtu;

            // set gatt client mtu
            gatt_client->mtu = mtu;
            gatt_client->mtu_state = MTU_EXCHANGED;

            if (update_gatt_server_att_mtu){
                // set per connection mtu state - for fixed channel
                hci_connection_t *hci_connection = hci_connection_for_handle(gatt_client->con_handle);
                hci_connection->att_connection.mtu = gatt_client->mtu;
                hci_connection->att_connection.mtu_exchanged = true;
            }
            emit_gatt_mtu_exchanged_result_event(gatt_client, gatt_client->mtu);
            break;
        }
        case ATT_READ_BY_GROUP_TYPE_RESPONSE:
            switch (gatt_client->state) {
                case P_W4_SERVICE_QUERY_RESULT:
                    report_gatt_services(gatt_client, packet, size);
                    trigger_next_service_query(gatt_client, get_last_result_handle_from_service_list(packet, size));
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                default:
                    break;
            }
            break;
        case ATT_HANDLE_VALUE_NOTIFICATION:
            if (size < 3u) return;
            report_gatt_notification(gatt_client, little_endian_read_16(packet, 1u), &packet[3], size - 3u);
            return;
#ifdef ENABLE_GATT_OVER_EATT
        case ATT_MULTIPLE_HANDLE_VALUE_NTF:
            if (size >= 5u) {
                uint16_t offset = 1;
                while (true){
                    uint16_t value_handle = little_endian_read_16(packet, offset);
                    offset += 2;
                    uint16_t value_length = little_endian_read_16(packet, offset);
                    offset += 2;
                    if ((offset + value_length) > size) break;
                    report_gatt_notification(gatt_client, value_handle, &packet[offset], value_length);
                    offset += value_length;
                }
            }
            return;
#endif
        case ATT_HANDLE_VALUE_INDICATION:
            if (size < 3u) break;
            report_gatt_indication(gatt_client, little_endian_read_16(packet, 1u), &packet[3], size - 3u);
            gatt_client->send_confirmation = true;
            break;
        case ATT_READ_BY_TYPE_RESPONSE:
            gatt_client_handle_att_read_by_type_response(gatt_client, packet, size);
            break;
        case ATT_READ_RESPONSE:
            gatt_client_handle_att_read_response(gatt_client, packet, size);
            break;
        case ATT_FIND_BY_TYPE_VALUE_RESPONSE: {
            uint8_t pair_size = 4;
            int i;
            uint16_t start_group_handle;
            uint16_t end_group_handle = 0xffff; // asserts GATT_EVENT_QUERY_COMPLETE is emitted if no results
            for (i = 1u; (i + pair_size) <= size; i += pair_size) {
                start_group_handle = little_endian_read_16(packet, i);
                end_group_handle = little_endian_read_16(packet, i + 2);
                emit_gatt_service_query_result_event(gatt_client, start_group_handle, end_group_handle,
                                                     gatt_client->uuid128);
            }
            trigger_next_service_by_uuid_query(gatt_client, end_group_handle);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;
        }
        case ATT_FIND_INFORMATION_REPLY: {
            if (size < 2u) break;

            uint8_t pair_size = 4;
            if (packet[1u] == 2u) {
                pair_size = 18;
            }
            uint16_t offset = 2;

            if (size < (pair_size + offset)) break;
            uint16_t last_descriptor_handle = little_endian_read_16(packet, size - pair_size);

#ifdef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
            log_info("ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY, state %x", gatt_client->state);
            if (gatt_client->state == P_W4_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY_RESULT){
                // iterate over descriptors looking for CCC
                if (pair_size == 4){
                    while ((offset + 4) <= size){
                        uint16_t uuid16 = little_endian_read_16(packet, offset + 2);
                        if (uuid16 == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION){
                            gatt_client->client_characteristic_configuration_handle = little_endian_read_16(packet, offset);
                            gatt_client->state = P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
                            log_info("CCC found %x", gatt_client->client_characteristic_configuration_handle);
                            break;
                        }
                        offset += pair_size;
                    }
                }
                if (is_query_done(gatt_client, last_descriptor_handle)){

                } else {
                    // next
                    gatt_client->start_group_handle = last_descriptor_handle + 1;
                    gatt_client->state = P_W2_SEND_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY;
                }
                break;
            }
#endif
            report_gatt_all_characteristic_descriptors(gatt_client, &packet[2], size - 2u, pair_size);
            trigger_next_characteristic_descriptor_query(gatt_client, last_descriptor_handle);
            // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
            break;
        }

        case ATT_WRITE_RESPONSE:
            gatt_client_handle_att_write_response(gatt_client);
            break;

        case ATT_READ_BLOB_RESPONSE: {
            uint16_t received_blob_length = size - 1u;
            switch (gatt_client->state) {
                case P_W4_READ_BLOB_RESULT:
                    report_gatt_long_characteristic_value_blob(gatt_client, gatt_client->attribute_handle, &packet[1],
                                                               received_blob_length, gatt_client->attribute_offset);
                    trigger_next_blob_query(gatt_client, P_W2_SEND_READ_BLOB_QUERY, received_blob_length);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                case P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    report_gatt_long_characteristic_descriptor(gatt_client, gatt_client->attribute_handle,
                                                               &packet[1], received_blob_length,
                                                               gatt_client->attribute_offset);
                    trigger_next_blob_query(gatt_client, P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY,
                                            received_blob_length);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                default:
                    break;
            }
            break;
        }
        case ATT_PREPARE_WRITE_RESPONSE:
            switch (gatt_client->state) {
                case P_W4_PREPARE_WRITE_SINGLE_RESULT:
                    if (is_value_valid(gatt_client, packet, size)) {
                        att_status = ATT_ERROR_SUCCESS;
                    } else {
                        att_status = ATT_ERROR_DATA_MISMATCH;
                    }
                    gatt_client_handle_transaction_complete(gatt_client, att_status);
                    break;

                case P_W4_PREPARE_WRITE_RESULT: {
                    gatt_client->attribute_offset = little_endian_read_16(packet, 3);
                    trigger_next_prepare_write_query(gatt_client, P_W2_PREPARE_WRITE, P_W2_EXECUTE_PREPARED_WRITE);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT: {
                    gatt_client->attribute_offset = little_endian_read_16(packet, 3);
                    trigger_next_prepare_write_query(gatt_client, P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR,
                                                     P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR);
                    // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                    break;
                }
                case P_W4_PREPARE_RELIABLE_WRITE_RESULT: {
                    if (is_value_valid(gatt_client, packet, size)) {
                        gatt_client->attribute_offset = little_endian_read_16(packet, 3);
                        trigger_next_prepare_write_query(gatt_client, P_W2_PREPARE_RELIABLE_WRITE,
                                                         P_W2_EXECUTE_PREPARED_WRITE);
                        // GATT_EVENT_QUERY_COMPLETE is emitted by trigger_next_xxx when done
                        break;
                    }
                    gatt_client->state = P_W2_CANCEL_PREPARED_WRITE_DATA_MISMATCH;
                    break;
                }
                default:
                    break;
            }
            break;

        case ATT_EXECUTE_WRITE_RESPONSE:
            switch (gatt_client->state) {
                case P_W4_EXECUTE_PREPARED_WRITE_RESULT:
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                    break;
                case P_W4_CANCEL_PREPARED_WRITE_RESULT:
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                    break;
                case P_W4_CANCEL_PREPARED_WRITE_DATA_MISMATCH_RESULT:
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_DATA_MISMATCH);
                    break;
                case P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                    break;
                default:
                    break;

            }
            break;

        case ATT_READ_MULTIPLE_RESPONSE:
            switch (gatt_client->state) {
                case P_W4_READ_MULTIPLE_RESPONSE:
                    report_gatt_characteristic_value(gatt_client, 0u, &packet[1], size - 1u);
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                    break;
                default:
                    break;
            }
            break;

#ifdef ENABLE_GATT_OVER_EATT
        case ATT_READ_MULTIPLE_VARIABLE_RSP:
            switch (gatt_client->state) {
                case P_W4_READ_MULTIPLE_VARIABLE_RESPONSE:
                    report_gatt_characteristic_value(gatt_client, 0u, &packet[1], size - 1u);
                    gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                    break;
                default:
                    break;
            }
            break;
#endif

        case ATT_ERROR_RESPONSE:
            if (size < 5u) return;
            att_status = packet[4];
            switch (att_status) {
                case ATT_ERROR_ATTRIBUTE_NOT_FOUND: {
                    switch (gatt_client->state) {
                        case P_W4_SERVICE_QUERY_RESULT:
                        case P_W4_SERVICE_WITH_UUID_RESULT:
                        case P_W4_INCLUDED_SERVICE_QUERY_RESULT:
                        case P_W4_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
                            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                            break;
                        case P_W4_ALL_CHARACTERISTICS_OF_SERVICE_QUERY_RESULT:
                        case P_W4_CHARACTERISTIC_WITH_UUID_QUERY_RESULT:
                            report_gatt_characteristic_end_found(gatt_client, gatt_client->end_group_handle);
                            gatt_client_handle_transaction_complete(gatt_client, ATT_ERROR_SUCCESS);
                            break;
                        case P_W4_READ_BY_TYPE_RESPONSE:
                            if (gatt_client->start_group_handle == gatt_client->query_start_handle) {
                                att_status = ATT_ERROR_ATTRIBUTE_NOT_FOUND;
                            } else {
                                att_status = ATT_ERROR_SUCCESS;
                            }
                            gatt_client_handle_transaction_complete(gatt_client, att_status);
                            break;
                        default:
                            gatt_client_report_error_if_pending(gatt_client, att_status);
                            break;
                    }
                    break;
                }

#ifdef ENABLE_GATT_CLIENT_PAIRING

                    case ATT_ERROR_INSUFFICIENT_AUTHENTICATION:
                    case ATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE:
                    case ATT_ERROR_INSUFFICIENT_ENCRYPTION: {

                        // security too low
                        if (gatt_client->security_counter > 0) {
                            gatt_client_report_error_if_pending(gatt_client, att_status);
                            break;
                        }
                        // start security
                        gatt_client->security_counter++;

                        // setup action
                        int retry = 1;
                        switch (gatt_client->state){
                            case P_W4_READ_CHARACTERISTIC_VALUE_RESULT:
                                gatt_client->state = P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY ;
                                break;
                            case P_W4_READ_BLOB_RESULT:
                                gatt_client->state = P_W2_SEND_READ_BLOB_QUERY;
                                break;
                            case P_W4_READ_BY_TYPE_RESPONSE:
                                gatt_client->state = P_W2_SEND_READ_BY_TYPE_REQUEST;
                                break;
                            case P_W4_READ_MULTIPLE_RESPONSE:
                                gatt_client->state = P_W2_SEND_READ_MULTIPLE_REQUEST;
                                break;
                            case P_W4_READ_MULTIPLE_VARIABLE_RESPONSE:
                                gatt_client->state = P_W2_SEND_READ_MULTIPLE_VARIABLE_REQUEST;
                                break;
                            case P_W4_WRITE_CHARACTERISTIC_VALUE_RESULT:
                                gatt_client->state = P_W2_SEND_WRITE_CHARACTERISTIC_VALUE;
                                break;
                            case P_W4_PREPARE_WRITE_RESULT:
                                gatt_client->state = P_W2_PREPARE_WRITE;
                                break;
                            case P_W4_PREPARE_WRITE_SINGLE_RESULT:
                                gatt_client->state = P_W2_PREPARE_WRITE_SINGLE;
                                break;
                            case P_W4_PREPARE_RELIABLE_WRITE_RESULT:
                                gatt_client->state = P_W2_PREPARE_RELIABLE_WRITE;
                                break;
                            case P_W4_EXECUTE_PREPARED_WRITE_RESULT:
                                gatt_client->state = P_W2_EXECUTE_PREPARED_WRITE;
                                break;
                            case P_W4_CANCEL_PREPARED_WRITE_RESULT:
                                gatt_client->state = P_W2_CANCEL_PREPARED_WRITE;
                                break;
                            case P_W4_CANCEL_PREPARED_WRITE_DATA_MISMATCH_RESULT:
                                gatt_client->state = P_W2_CANCEL_PREPARED_WRITE_DATA_MISMATCH;
                                break;
                            case P_W4_READ_CHARACTERISTIC_DESCRIPTOR_RESULT:
                                gatt_client->state = P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY;
                                break;
                            case P_W4_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_RESULT:
                                gatt_client->state = P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY;
                                break;
                            case P_W4_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                                gatt_client->state = P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR;
                                break;
                            case P_W4_CLIENT_CHARACTERISTIC_CONFIGURATION_RESULT:
                                gatt_client->state = P_W2_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION;
                                break;
                            case P_W4_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                                gatt_client->state = P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR;
                                break;
                            case P_W4_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR_RESULT:
                                gatt_client->state = P_W2_EXECUTE_PREPARED_WRITE_CHARACTERISTIC_DESCRIPTOR;
                                break;
#ifdef ENABLE_LE_SIGNED_WRITE
                            case P_W4_SEND_SIGNED_WRITE_DONE:
                                gatt_client->state = P_W2_SEND_SIGNED_WRITE;
                                break;
#endif
                            default:
                                log_info("retry not supported for state %x", gatt_client->state);
                                retry = 0;
                                break;
                        }

                        if (!retry) {
                            gatt_client_report_error_if_pending(gatt_client, att_status);
                            break;
                        }

                        log_info("security error, start pairing");

                        // start pairing for higher security level
                        gatt_client->wait_for_authentication_complete = true;
                        gatt_client->pending_error_code = att_status;
                        sm_request_pairing(gatt_client->con_handle);
                        break;
                    }
#endif

                    // nothing we can do about that
                case ATT_ERROR_INSUFFICIENT_AUTHORIZATION:
                default:
                    gatt_client_report_error_if_pending(gatt_client, att_status);
                    break;
            }
            break;

        default:
            log_info("ATT Handler, unhandled response type 0x%02x", packet[0]);
            break;
    }
}

static void gatt_client_att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size) {
    gatt_client_t *gatt_client;
#ifdef ENABLE_GATT_OVER_CLASSIC
    uint8_t status;
    hci_connection_t * hci_connection;
    hci_con_handle_t con_handle;
#endif

    if (size < 1u) return;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
#ifdef ENABLE_GATT_OVER_CLASSIC
                case L2CAP_EVENT_CHANNEL_OPENED:
                    status = l2cap_event_channel_opened_get_status(packet);
                    gatt_client = gatt_client_get_context_for_l2cap_cid(l2cap_event_channel_opened_get_local_cid(packet));
                    if (gatt_client != NULL){
                        con_handle = l2cap_event_channel_opened_get_handle(packet);
                        hci_connection = hci_connection_for_handle(con_handle);
                        if (status == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES){
                            if ((hci_connection != NULL) && hci_connection->att_server.incoming_connection_request) {
                                log_info("Collision, retry in 100ms");
                                gatt_client->state = P_W2_L2CAP_CONNECT;
                                // set timer for retry
                                btstack_run_loop_set_timer(&gatt_client->gc_timeout, GATT_CLIENT_COLLISION_BACKOFF_MS);
                                btstack_run_loop_set_timer_handler(&gatt_client->gc_timeout, gatt_client_classic_retry);
                                btstack_run_loop_add_timer(&gatt_client->gc_timeout);
                                break;
                            }
                        }
                        // if status != 0, gatt_client will be discarded
                        gatt_client->state = P_READY;
                        gatt_client->con_handle = l2cap_event_channel_opened_get_handle(packet);
                        gatt_client->mtu = l2cap_event_channel_opened_get_remote_mtu(packet);
                        gatt_client_classic_handle_connected(gatt_client, status);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    gatt_client = gatt_client_get_context_for_l2cap_cid(l2cap_event_channel_closed_get_local_cid(packet));
                    if (gatt_client != NULL){
                        // discard gatt client object
                        gatt_client_classic_handle_disconnected(gatt_client);
                    }
                    break;
#endif
                case L2CAP_EVENT_CAN_SEND_NOW:
                    gatt_client_run();
                    break;
                    // att_server has negotiated the mtu for this connection, cache if context exists
                case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
                    if (size < 6u) break;
                    gatt_client = gatt_client_get_context_for_handle(handle);
                    if (gatt_client != NULL) {
                        gatt_client->mtu = little_endian_read_16(packet, 4);
                    }
                    break;
                default:
                    break;
            }
            break;

        case ATT_DATA_PACKET:
            // special cases: notifications & indications motivate creating context
            switch (packet[0]) {
                case ATT_HANDLE_VALUE_NOTIFICATION:
                case ATT_HANDLE_VALUE_INDICATION:
                    gatt_client_provide_context_for_handle(handle, &gatt_client);
                    break;
                default:
                    gatt_client = gatt_client_get_context_for_handle(handle);
                    break;
            }

            if (gatt_client != NULL) {
                gatt_client_handle_att_response(gatt_client, packet, size);
                gatt_client_run();
            }
            break;

#ifdef ENABLE_GATT_OVER_CLASSIC
        case L2CAP_DATA_PACKET:
            gatt_client = gatt_client_get_context_for_l2cap_cid(handle);
            if (gatt_client != NULL){
                gatt_client_handle_att_response(gatt_client, packet, size);
                gatt_client_run();
            }
            break;
#endif

        default:
            break;
    }
}

#ifdef ENABLE_LE_SIGNED_WRITE
static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_client_t * gatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        if (gatt_client->state == P_W4_CMAC_RESULT){
            // store result
            (void)memcpy(gatt_client->cmac, hash, 8);
            // reverse_64(hash, gatt_client->cmac);
            gatt_client->state = P_W2_SEND_SIGNED_WRITE;
            gatt_client_run();
            return;
        }
    }
}

uint8_t gatt_client_signed_write_without_response(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t message_len, uint8_t * message){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    if (is_ready(gatt_client) == 0){
        return GATT_CLIENT_IN_WRONG_STATE;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = value_handle;
    gatt_client->attribute_length = message_len;
    gatt_client->attribute_value = message;
    gatt_client->state = P_W4_IDENTITY_RESOLVING;
    gatt_client_run();
    return ERROR_CODE_SUCCESS; 
}
#endif

uint8_t gatt_client_discover_primary_services(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = 0x0001;
    gatt_client->end_group_handle   = 0xffff;
    gatt_client->state = P_W2_SEND_SERVICE_QUERY;
    gatt_client->uuid16 = GATT_PRIMARY_SERVICE_UUID;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_secondary_services(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = 0x0001;
    gatt_client->end_group_handle   = 0xffff;
    gatt_client->state = P_W2_SEND_SERVICE_QUERY;
    gatt_client->uuid16 = GATT_SECONDARY_SERVICE_UUID;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_primary_services_by_uuid16_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle,
                                                                     uint16_t uuid16, uint16_t service_id, uint16_t connection_id){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    gatt_client->start_group_handle = 0x0001;
    gatt_client->end_group_handle   = 0xffff;
    gatt_client->state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    gatt_client->uuid16 = uuid16;
    uuid_add_bluetooth_prefix((uint8_t*) &(gatt_client->uuid128), gatt_client->uuid16);
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_primary_services_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t uuid16){
    return gatt_client_discover_primary_services_by_uuid16_with_context(callback, con_handle, uuid16, 0, 0);
}

uint8_t gatt_client_discover_primary_services_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, const uint8_t * uuid128){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = 0x0001;
    gatt_client->end_group_handle   = 0xffff;
    gatt_client->uuid16 = 0;
    (void)memcpy(gatt_client->uuid128, uuid128, 16);
    gatt_client->state = P_W2_SEND_SERVICE_WITH_UUID_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_service_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service,
                                                                      uint16_t service_id, uint16_t connection_id){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    gatt_client->start_group_handle = service->start_group_handle;
    gatt_client->end_group_handle   = service->end_group_handle;
    gatt_client->filter_with_uuid = false;
    gatt_client->characteristic_start_handle = 0;
    gatt_client->state = P_W2_SEND_ALL_CHARACTERISTICS_OF_SERVICE_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service){
    return gatt_client_discover_characteristics_for_service_with_context(callback, con_handle, service, 0, 0);
}

uint8_t gatt_client_find_included_services_for_service_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle,
                                                                    gatt_client_service_t * service, uint16_t service_id, uint16_t connection_id){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    gatt_client->start_group_handle = service->start_group_handle;
    gatt_client->end_group_handle   = service->end_group_handle;
    gatt_client->state = P_W2_SEND_INCLUDED_SERVICE_QUERY;
    
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_find_included_services_for_service(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service) {
    return gatt_client_find_included_services_for_service_with_context(callback, con_handle, service, 0, 0);
}

uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = start_handle;
    gatt_client->end_group_handle   = end_handle;
    gatt_client->filter_with_uuid = true;
    gatt_client->uuid16 = uuid16;
    uuid_add_bluetooth_prefix((uint8_t*) &(gatt_client->uuid128), uuid16);
    gatt_client->characteristic_start_handle = 0;
    gatt_client->state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristics_for_handle_range_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, const uint8_t * uuid128){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = start_handle;
    gatt_client->end_group_handle   = end_handle;
    gatt_client->filter_with_uuid = true;
    gatt_client->uuid16 = 0;
    (void)memcpy(gatt_client->uuid128, uuid128, 16);
    gatt_client->characteristic_start_handle = 0;
    gatt_client->state = P_W2_SEND_CHARACTERISTIC_WITH_UUID_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}


uint8_t gatt_client_discover_characteristics_for_service_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service, uint16_t uuid16){
    return gatt_client_discover_characteristics_for_handle_range_by_uuid16(callback, con_handle, service->start_group_handle, service->end_group_handle, uuid16);
}

uint8_t gatt_client_discover_characteristics_for_service_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_service_t * service, const uint8_t * uuid128){
    return gatt_client_discover_characteristics_for_handle_range_by_uuid128(callback, con_handle, service->start_group_handle, service->end_group_handle, uuid128);
}

uint8_t gatt_client_discover_characteristic_descriptors_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle,
                                                                     gatt_client_characteristic_t * characteristic,  uint16_t service_id, uint16_t connection_id){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;

    // check if there is space for characteristics descriptors
    if (characteristic->end_handle > characteristic->value_handle){
        gatt_client->callback = callback;
        gatt_client->start_group_handle = characteristic->value_handle + 1u;
        gatt_client->end_group_handle   = characteristic->end_handle;
        gatt_client->state = P_W2_SEND_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY;
        gatt_client_run();
    } else {
        // schedule gatt complete event on next run loop iteration otherwise
        gatt_client->state = P_W2_EMIT_QUERY_COMPLETE_EVENT;
        gatt_client_deferred_event_emit.callback = gatt_client_emit_events;
        btstack_run_loop_execute_on_main_thread(&gatt_client_deferred_event_emit);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_discover_characteristic_descriptors(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    return gatt_client_discover_characteristic_descriptors_with_context(callback, con_handle, characteristic, 0, 0);
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle_with_context(btstack_packet_handler_t callback,
                                                                                 hci_con_handle_t con_handle,
                                                                                 uint16_t value_handle,
                                                                                 uint16_t service_id,
                                                                                 uint16_t connection_id) {
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    gatt_client->attribute_handle = value_handle;
    gatt_client->attribute_offset = 0;
    gatt_client->state = P_W2_SEND_READ_CHARACTERISTIC_VALUE_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    return gatt_client_read_value_of_characteristic_using_value_handle_with_context(callback, con_handle, value_handle, 0, 0);

}

uint8_t gatt_client_read_value_of_characteristics_by_uuid16(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, uint16_t uuid16){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = start_handle;
    gatt_client->end_group_handle = end_handle;
    gatt_client->query_start_handle = start_handle;
    gatt_client->query_end_handle = end_handle;
    gatt_client->uuid16 = uuid16;
    uuid_add_bluetooth_prefix((uint8_t*) &(gatt_client->uuid128), uuid16);
    gatt_client->state = P_W2_SEND_READ_BY_TYPE_REQUEST;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_value_of_characteristics_by_uuid128(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t start_handle, uint16_t end_handle, const uint8_t * uuid128){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->start_group_handle = start_handle;
    gatt_client->end_group_handle = end_handle;
    gatt_client->query_start_handle = start_handle;
    gatt_client->query_end_handle = end_handle;
    gatt_client->uuid16 = 0;
    (void)memcpy(gatt_client->uuid128, uuid128, 16);
    gatt_client->state = P_W2_SEND_READ_BY_TYPE_REQUEST;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}


uint8_t gatt_client_read_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    return gatt_client_read_value_of_characteristic_using_value_handle(callback, con_handle, characteristic->value_handle);
}

uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t offset){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = value_handle;
    gatt_client->attribute_offset = offset;
    gatt_client->state = P_W2_SEND_READ_BLOB_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}
uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle_with_context(btstack_packet_handler_t callback,
                                                                                      hci_con_handle_t con_handle, uint16_t value_handle,
                                                                                      uint16_t service_id, uint16_t connection_id){
    // TODO: move into gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset once
    //       gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset_and_context exists
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    return gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(callback, con_handle, value_handle, 0);
}

uint8_t gatt_client_read_long_value_of_characteristic_using_value_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle){
    return gatt_client_read_long_value_of_characteristic_using_value_handle_with_context(callback, con_handle, value_handle, 0, 0);
}

uint8_t gatt_client_read_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic){
    return gatt_client_read_long_value_of_characteristic_using_value_handle(callback, con_handle, characteristic->value_handle);
}

static uint8_t gatt_client_read_multiple_characteristic_values_with_state(btstack_packet_handler_t callback, hci_con_handle_t con_handle, int num_value_handles, uint16_t * value_handles, gatt_client_state_t state){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->read_multiple_handle_count = num_value_handles;
    gatt_client->read_multiple_handles = value_handles;
    gatt_client->state = state;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_multiple_characteristic_values(btstack_packet_handler_t callback, hci_con_handle_t con_handle, int num_value_handles, uint16_t * value_handles){
    return gatt_client_read_multiple_characteristic_values_with_state(callback, con_handle, num_value_handles, value_handles, P_W2_SEND_READ_MULTIPLE_REQUEST);
}

#ifdef ENABLE_GATT_OVER_EATT
uint8_t gatt_client_read_multiple_variable_characteristic_values(btstack_packet_handler_t callback, hci_con_handle_t con_handle, int num_value_handles, uint16_t * value_handles){
    return gatt_client_read_multiple_characteristic_values_with_state(callback, con_handle, num_value_handles, value_handles, P_W2_SEND_READ_MULTIPLE_VARIABLE_REQUEST);
}
#endif

uint8_t gatt_client_write_value_of_characteristic_without_response(hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (value_length > (gatt_client->mtu - 3u)) return GATT_CLIENT_VALUE_TOO_LONG;
    if (!att_dispatch_client_can_send_now(gatt_client->con_handle)) return GATT_CLIENT_BUSY;

    return att_write_request(gatt_client, ATT_WRITE_COMMAND, value_handle, value_length, value);
}
uint8_t gatt_client_write_value_of_characteristic_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle,
                                                               uint16_t value_length, uint8_t * value, uint16_t service_id, uint16_t connection_id){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    gatt_client->attribute_handle = value_handle;
    gatt_client->attribute_length = value_length;
    gatt_client->attribute_value = value;
    gatt_client->state = P_W2_SEND_WRITE_CHARACTERISTIC_VALUE;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}
uint8_t gatt_client_write_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value) {
    return gatt_client_write_value_of_characteristic_with_context(callback, con_handle, value_handle, value_length, value, 0, 0);
}

uint8_t gatt_client_write_long_value_of_characteristic_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t offset, uint16_t value_length, uint8_t * value){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = value_handle;
    gatt_client->attribute_length = value_length;
    gatt_client->attribute_offset = offset;
    gatt_client->attribute_value = value;
    gatt_client->state = P_W2_PREPARE_WRITE;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_long_value_of_characteristic_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value, uint16_t service_id, uint16_t connection_id){
    // TODO: move into gatt_client_write_long_value_of_characteristic_with_offset once gatt_client_write_long_value_of_characteristic_with_offset_with_context exists
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    return gatt_client_write_long_value_of_characteristic_with_offset(callback, con_handle, value_handle, 0, value_length, value);
}

uint8_t gatt_client_write_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    return gatt_client_write_long_value_of_characteristic_with_context(callback, con_handle, value_handle, value_length, value, 0, 0);
}

uint8_t gatt_client_reliable_write_long_value_of_characteristic(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t value_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = value_handle;
    gatt_client->attribute_length = value_length;
    gatt_client->attribute_offset = 0;
    gatt_client->attribute_value = value;
    gatt_client->state = P_W2_PREPARE_RELIABLE_WRITE;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_client_characteristic_configuration_with_context(btstack_packet_handler_t callback, hci_con_handle_t con_handle,
                                                              gatt_client_characteristic_t * characteristic, uint16_t configuration, uint16_t service_id, uint16_t connection_id){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (configuration > 3){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    
    if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) &&
        ((characteristic->properties & ATT_PROPERTY_NOTIFY) == 0u)) {
        log_info("gatt_client_write_client_characteristic_configuration: GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED");
        return GATT_CLIENT_CHARACTERISTIC_NOTIFICATION_NOT_SUPPORTED;
    } else if ( (configuration & GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION) &&
               ((characteristic->properties & ATT_PROPERTY_INDICATE) == 0u)){
        log_info("gatt_client_write_client_characteristic_configuration: GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED");
        return GATT_CLIENT_CHARACTERISTIC_INDICATION_NOT_SUPPORTED;
    }

    gatt_client->callback = callback;
    gatt_client->service_id = service_id;
    gatt_client->connection_id = connection_id;
    gatt_client->start_group_handle = characteristic->value_handle;
    gatt_client->end_group_handle = characteristic->end_handle;
    little_endian_store_16(gatt_client->client_characteristic_configuration_value, 0, configuration);
    
#ifdef ENABLE_GATT_FIND_INFORMATION_FOR_CCC_DISCOVERY
    gatt_client->state = P_W2_SEND_FIND_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY;
#else
    gatt_client->state = P_W2_SEND_READ_CLIENT_CHARACTERISTIC_CONFIGURATION_QUERY;
#endif
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_client_characteristic_configuration(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_t * characteristic, uint16_t configuration){
    return gatt_client_write_client_characteristic_configuration_with_context(callback, con_handle, characteristic, configuration, 0, 0);
}

uint8_t gatt_client_read_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = descriptor_handle;

    gatt_client->state = P_W2_SEND_READ_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor){
    return gatt_client_read_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle);
}

uint8_t gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t offset){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = descriptor_handle;
    gatt_client->attribute_offset = offset;
    gatt_client->state = P_W2_SEND_READ_BLOB_CHARACTERISTIC_DESCRIPTOR_QUERY;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle){
    return gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(callback, con_handle, descriptor_handle, 0);
}

uint8_t gatt_client_read_long_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor){
    return gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle);
}

uint8_t gatt_client_write_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t value_length, uint8_t * value){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = descriptor_handle;
    gatt_client->attribute_length = value_length;
    gatt_client->attribute_offset = 0;
    gatt_client->attribute_value = value;
    gatt_client->state = P_W2_SEND_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor, uint16_t value_length, uint8_t * value){
    return gatt_client_write_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle, value_length, value);
}

uint8_t gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t offset, uint16_t value_length, uint8_t * value){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = descriptor_handle;
    gatt_client->attribute_length = value_length;
    gatt_client->attribute_offset = offset;
    gatt_client->attribute_value = value;
    gatt_client->state = P_W2_PREPARE_WRITE_CHARACTERISTIC_DESCRIPTOR;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t descriptor_handle, uint16_t value_length, uint8_t * value){
    return gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(callback, con_handle, descriptor_handle, 0, value_length, value);
}

uint8_t gatt_client_write_long_characteristic_descriptor(btstack_packet_handler_t callback, hci_con_handle_t con_handle, gatt_client_characteristic_descriptor_t * descriptor, uint16_t value_length, uint8_t * value){
    return gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(callback, con_handle, descriptor->handle, value_length, value);
}

/**
 * @brief -> gatt complete event
 */
uint8_t gatt_client_prepare_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint16_t value_length, uint8_t * value){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->attribute_handle = attribute_handle;
    gatt_client->attribute_length = value_length;
    gatt_client->attribute_offset = offset;
    gatt_client->attribute_value = value;
    gatt_client->state = P_W2_PREPARE_WRITE_SINGLE;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief -> gatt complete event
 */
uint8_t gatt_client_execute_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->state = P_W2_EXECUTE_PREPARED_WRITE;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief -> gatt complete event
 */
uint8_t gatt_client_cancel_write(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_request(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    gatt_client->callback = callback;
    gatt_client->state = P_W2_CANCEL_PREPARED_WRITE;
    gatt_client_run();
    return ERROR_CODE_SUCCESS;    
}

void gatt_client_deserialize_service(const uint8_t *packet, int offset, gatt_client_service_t * service){
    service->start_group_handle = little_endian_read_16(packet, offset);
    service->end_group_handle = little_endian_read_16(packet, offset + 2);
    reverse_128(&packet[offset + 4], service->uuid128);
    if (uuid_has_bluetooth_prefix(service->uuid128)){
        service->uuid16 = big_endian_read_32(service->uuid128, 0);
    } else {
        service->uuid16 = 0;
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
    } else {
        characteristic->uuid16 = 0;
    }
}

void gatt_client_deserialize_characteristic_descriptor(const uint8_t * packet, int offset, gatt_client_characteristic_descriptor_t * descriptor){
    descriptor->handle = little_endian_read_16(packet, offset);
    reverse_128(&packet[offset+2], descriptor->uuid128);
    if (uuid_has_bluetooth_prefix(descriptor->uuid128)){
        descriptor->uuid16 = big_endian_read_32(descriptor->uuid128, 0);
    } else {
        descriptor->uuid16 = 0;
    }
}

void gatt_client_send_mtu_negotiation(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return;
    }    
    if (gatt_client->mtu_state == MTU_AUTO_EXCHANGE_DISABLED){
        gatt_client->callback = callback;
        gatt_client->mtu_state = SEND_MTU_EXCHANGE;
        gatt_client_run();
    }
}

uint8_t gatt_client_request_to_write_without_response(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    bool added = btstack_linked_list_add_tail(&gatt_client->write_without_response_requests, (btstack_linked_item_t*) callback_registration);
    if (added == false){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } else {
        att_dispatch_client_request_can_send_now_event(gatt_client->con_handle);
        return ERROR_CODE_SUCCESS;
    }
}

uint8_t gatt_client_request_to_send_gatt_query(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    bool added = btstack_linked_list_add_tail(&gatt_client->query_requests, (btstack_linked_item_t*) callback_registration);
    if (added == false){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } else {
        gatt_client_notify_can_send_query(gatt_client);
        return ERROR_CODE_SUCCESS;
    }
}

uint8_t gatt_client_remove_gatt_query(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    (void)btstack_linked_list_remove(&gatt_client->query_requests, (btstack_linked_item_t*) callback_registration);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_request_can_write_without_response_event(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }    
    if (gatt_client->write_without_response_callback != NULL){
        return GATT_CLIENT_IN_WRONG_STATE;
    } 
    gatt_client->write_without_response_callback = callback;
    att_dispatch_client_request_can_send_now_event(gatt_client->con_handle);
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_att_status_to_error_code(uint8_t att_error_code){
    switch (att_error_code){
        case ATT_ERROR_SUCCESS:
            return ERROR_CODE_SUCCESS;
        case ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH:
            return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
        default:
            log_info("ATT ERROR 0x%02x mapped to ERROR_CODE_UNSPECIFIED_ERROR", att_error_code);
            return ERROR_CODE_UNSPECIFIED_ERROR;
    }
}

#ifdef ENABLE_GATT_CLIENT_SERVICE_CHANGED
void gatt_client_add_service_changed_handler(btstack_packet_callback_registration_t * callback) {
    btstack_linked_list_add_tail(&gatt_client_service_changed_handler, (btstack_linked_item_t*) callback);
}

void gatt_client_remove_service_changed_handler(btstack_packet_callback_registration_t * callback){
    btstack_linked_list_remove(&gatt_client_service_changed_handler, (btstack_linked_item_t*) callback);
}
#endif

#if defined(ENABLE_GATT_OVER_CLASSIC) || defined(ENABLE_GATT_OVER_EATT)

#include "hci_event.h"

static const hci_event_t gatt_client_connected = {
        GATT_EVENT_CONNECTED, 0, "11BH"
};

static const hci_event_t gatt_client_disconnected = {
        GATT_EVENT_DISCONNECTED, 0, "H"
};

static void
gatt_client_emit_connected(btstack_packet_handler_t callback, uint8_t status, bd_addr_type_t addr_type, bd_addr_t addr,
                           hci_con_handle_t con_handle) {
    uint8_t buffer[20];
    uint16_t len = hci_event_create_from_template_and_arguments(buffer, sizeof(buffer), &gatt_client_connected, status, addr_type, addr, con_handle);
    (*callback)(HCI_EVENT_PACKET, 0, buffer, len);
}

#endif

#ifdef ENABLE_GATT_OVER_CLASSIC

#include "bluetooth_psm.h"

// single active SDP query
static gatt_client_t * gatt_client_classic_active_sdp_query;

// macos protocol descriptor list requires 16 bytes
static uint8_t gatt_client_classic_sdp_buffer[32];


static gatt_client_t * gatt_client_get_context_for_classic_addr(bd_addr_t addr){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) gatt_client_connections; it != NULL; it = it->next){
        gatt_client_t * gatt_client = (gatt_client_t *) it;
        if (memcmp(gatt_client->addr, addr, 6) == 0){
            return gatt_client;
        }
    }
    return NULL;
}

static gatt_client_t * gatt_client_get_context_for_l2cap_cid(uint16_t l2cap_cid){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) gatt_client_connections; it != NULL; it = it->next){
        gatt_client_t * gatt_client = (gatt_client_t *) it;
        if (gatt_client->l2cap_cid == l2cap_cid){
            return gatt_client;
        }
    }
    return NULL;
}

static void gatt_client_classic_handle_connected(gatt_client_t * gatt_client, uint8_t status){
    // cache peer information
    bd_addr_t addr;
    // cppcheck-suppress uninitvar ; addr is reported as uninitialized although it's the destination of the memcpy
    memcpy(addr, gatt_client->addr, 6);
    bd_addr_type_t addr_type = gatt_client->addr_type;
    gatt_client->addr_type = BD_ADDR_TYPE_ACL;
    hci_con_handle_t con_handle = gatt_client->con_handle;
    btstack_packet_handler_t callback = gatt_client->callback;

    if (status != ERROR_CODE_SUCCESS){
        btstack_linked_list_remove(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
        btstack_memory_gatt_client_free(gatt_client);
    }

    gatt_client_emit_connected(callback, status, addr_type, addr, con_handle);
}

static void gatt_client_classic_retry(btstack_timer_source_t * ts){
    gatt_client_t * gatt_client = gatt_client_for_timer(ts);
    if (gatt_client != NULL){
        gatt_client->state = P_W4_L2CAP_CONNECTION;
        att_dispatch_classic_connect(gatt_client->addr, gatt_client->l2cap_psm, &gatt_client->l2cap_cid);
    }
}

static void gatt_client_classic_handle_disconnected(gatt_client_t * gatt_client){

    gatt_client_report_error_if_pending(gatt_client, ATT_ERROR_HCI_DISCONNECT_RECEIVED);
    gatt_client_timeout_stop(gatt_client);

    hci_con_handle_t con_handle = gatt_client->con_handle;
    btstack_packet_handler_t callback = gatt_client->callback;
    btstack_linked_list_remove(&gatt_client_connections, (btstack_linked_item_t *) gatt_client);
    btstack_memory_gatt_client_free(gatt_client);

    uint8_t buffer[20];
    uint16_t len = hci_event_create_from_template_and_arguments(buffer, sizeof(buffer), &gatt_client_disconnected, con_handle);
    (*callback)(HCI_EVENT_PACKET, 0, buffer, len);
}

static void gatt_client_handle_sdp_client_query_attribute_value(gatt_client_t * connection, uint8_t *packet){
    des_iterator_t des_list_it;
    des_iterator_t prot_it;

    if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= sizeof(gatt_client_classic_sdp_buffer)) {
        gatt_client_classic_sdp_buffer[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
        if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
            switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                    for (des_iterator_init(&des_list_it, gatt_client_classic_sdp_buffer); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                        uint8_t       *des_element;
                        uint8_t       *element;
                        uint32_t       uuid;

                        if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                        des_element = des_iterator_get_element(&des_list_it);
                        des_iterator_init(&prot_it, des_element);
                        element = des_iterator_get_element(&prot_it);

                        if (de_get_element_type(element) != DE_UUID) continue;

                        uuid = de_get_uuid32(element);
                        des_iterator_next(&prot_it);
                        // we assume that the even if there are both roles supported, remote device uses the same psm and avdtp version for both
                        switch (uuid){
                            case BLUETOOTH_PROTOCOL_L2CAP:
                                if (!des_iterator_has_more(&prot_it)) continue;
                                de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->l2cap_psm);
                                break;
                            default:
                                break;
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

static void gatt_client_classic_sdp_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    gatt_client_t * gatt_client = gatt_client_classic_active_sdp_query;
    btstack_assert(gatt_client != NULL);
    uint8_t status;

    // TODO: handle sdp events, get l2cap psm
    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            gatt_client_handle_sdp_client_query_attribute_value(gatt_client, packet);
            // TODO:
            return;
        case SDP_EVENT_QUERY_COMPLETE:
            status = sdp_event_query_complete_get_status(packet);
            gatt_client_classic_active_sdp_query = NULL;
            log_info("l2cap psm: %0x, status %02x", gatt_client->l2cap_psm, status);
            if (status != ERROR_CODE_SUCCESS) break;
            if (gatt_client->l2cap_psm == 0) {
                status = SDP_SERVICE_NOT_FOUND;
                break;
            }
            break;
        default:
            btstack_assert(false);
            return;
    }

    // done
    if (status == ERROR_CODE_SUCCESS){
        gatt_client->state = P_W4_L2CAP_CONNECTION;
        status = att_dispatch_classic_connect(gatt_client->addr, gatt_client->l2cap_psm, &gatt_client->l2cap_cid);
    }
    if (status != ERROR_CODE_SUCCESS) {
        gatt_client_classic_handle_connected(gatt_client, status);
    }
}

static void gatt_client_classic_sdp_start(void * context){
    gatt_client_classic_active_sdp_query = (gatt_client_t *) context;
    gatt_client_classic_active_sdp_query->state = P_W4_SDP_QUERY;
    sdp_client_query_uuid16(gatt_client_classic_sdp_handler, gatt_client_classic_active_sdp_query->addr, ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE);
}

static void gatt_client_classic_emit_connected(void * context){
    gatt_client_t * gatt_client = (gatt_client_t *) context;
    gatt_client->state = P_READY;
    gatt_client_emit_connected(gatt_client->callback, ERROR_CODE_SUCCESS, gatt_client->addr_type, gatt_client->addr, gatt_client->con_handle);
}

uint8_t gatt_client_classic_connect(btstack_packet_handler_t callback, bd_addr_t addr){
    gatt_client_t * gatt_client = gatt_client_get_context_for_classic_addr(addr);
    if (gatt_client != NULL){
        return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
    }
    gatt_client = btstack_memory_gatt_client_get();
    if (gatt_client == NULL){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }
    // init state
    gatt_client->bearer_type = ATT_BEARER_UNENHANCED_CLASSIC;
    gatt_client->con_handle = HCI_CON_HANDLE_INVALID;
    memcpy(gatt_client->addr, addr, 6);
    gatt_client->addr_type = BD_ADDR_TYPE_ACL;
    gatt_client->mtu = ATT_DEFAULT_MTU;
    gatt_client->security_level = LEVEL_0;
    gatt_client->mtu_state = MTU_AUTO_EXCHANGE_DISABLED;
    gatt_client->callback = callback;
#ifdef ENABLE_GATT_OVER_EATT
    gatt_client->eatt_state = GATT_CLIENT_EATT_IDLE;
#endif
    btstack_linked_list_add(&gatt_client_connections, (btstack_linked_item_t*)gatt_client);

    // schedule emitted event if already connected, otherwise
    bool already_connected = false;
    hci_connection_t * hci_connection = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
    if (hci_connection != NULL){
        if (hci_connection->att_server.l2cap_cid != 0){
            already_connected = true;
        }
    }
    gatt_client->callback_request.context = gatt_client;
    if (already_connected){
        gatt_client->con_handle = hci_connection->con_handle;
        gatt_client->callback_request.callback = &gatt_client_classic_emit_connected;
        gatt_client->state = P_W2_EMIT_CONNECTED;
        btstack_run_loop_execute_on_main_thread(&gatt_client->callback_request);
    } else {
        gatt_client->callback_request.callback = &gatt_client_classic_sdp_start;
        gatt_client->state = P_W2_SDP_QUERY;
        sdp_client_register_query_callback(&gatt_client->callback_request);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t gatt_client_classic_disconnect(btstack_packet_handler_t callback, hci_con_handle_t con_handle){
    gatt_client_t * gatt_client = gatt_client_get_context_for_handle(con_handle);
    if (gatt_client == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    gatt_client->callback = callback;
    return l2cap_disconnect(gatt_client->l2cap_cid);
}
#endif

#ifdef ENABLE_GATT_OVER_EATT

#define MAX_NR_EATT_CHANNELS 5

static void gatt_client_le_enhanced_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint8_t gatt_client_le_enhanced_num_eatt_clients_in_state(gatt_client_t * gatt_client, gatt_client_state_t state){
    uint8_t num_clients = 0;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client->eatt_clients);
    while (btstack_linked_list_iterator_has_next(&it)){
        gatt_client_t * eatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        if (eatt_client->state == state){
            num_clients++;
        }
    }
    return num_clients;
}

static void gatt_client_eatt_finalize(gatt_client_t * gatt_client) {
    // free eatt clients
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)) {
        gatt_client_t *eatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        btstack_linked_list_iterator_remove(&it);
        btstack_memory_gatt_client_free(eatt_client);
    }
}

// all channels connected
static void gatt_client_le_enhanced_handle_connected(gatt_client_t * gatt_client, uint8_t status) {
    if (status == ERROR_CODE_SUCCESS){
        uint8_t num_ready = gatt_client_le_enhanced_num_eatt_clients_in_state(gatt_client, P_READY);
        if (num_ready > 0){
            gatt_client->eatt_state = GATT_CLIENT_EATT_READY;
            // free unused channels
            btstack_linked_list_iterator_t it;
            btstack_linked_list_iterator_init(&it, &gatt_client_connections);
            while (btstack_linked_list_iterator_has_next(&it)) {
                gatt_client_t *eatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
                if (eatt_client->state == P_L2CAP_CLOSED){
                    btstack_linked_list_iterator_remove(&it);
                    btstack_memory_gatt_client_free(eatt_client);
                }
            }
        } else {
            hci_connection_t * hci_connection = hci_connection_for_handle(gatt_client->con_handle);
            btstack_assert(hci_connection != NULL);
            if (hci_connection->att_server.incoming_connection_request){
                hci_connection->att_server.incoming_connection_request = false;
                log_info("Collision, retry in 100ms");
                gatt_client->state = P_W2_L2CAP_CONNECT;
                // set timer for retry
                btstack_run_loop_set_timer(&gatt_client->gc_timeout, GATT_CLIENT_COLLISION_BACKOFF_MS);
                btstack_run_loop_set_timer_handler(&gatt_client->gc_timeout, gatt_client_le_enhanced_retry);
                btstack_run_loop_add_timer(&gatt_client->gc_timeout);
                return;
            } else {
                gatt_client->eatt_state = GATT_CLIENT_EATT_IDLE;
                status = ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES;
            }
        }
    } else {
        gatt_client_eatt_finalize(gatt_client);
        gatt_client->eatt_state = GATT_CLIENT_EATT_IDLE;
    }

    gatt_client_emit_connected(gatt_client->callback, status, gatt_client->addr_type, gatt_client->addr, gatt_client->con_handle);
}

// single channel disconnected
static void gatt_client_le_enhanced_handle_ecbm_disconnected(gatt_client_t * gatt_client, gatt_client_t * eatt_client) {

    // report error
    gatt_client_report_error_if_pending(eatt_client, ATT_ERROR_HCI_DISCONNECT_RECEIVED);

    // free memory
    btstack_linked_list_remove(&gatt_client->eatt_clients, (btstack_linked_item_t *) eatt_client);
    btstack_memory_gatt_client_free(eatt_client);

    // last channel
    if (btstack_linked_list_empty(&gatt_client->eatt_clients)){
        hci_connection_t * hci_connection = hci_connection_for_handle(gatt_client->con_handle);
        hci_connection->att_server.eatt_outgoing_active = false;

        if (gatt_client->eatt_state == GATT_CLIENT_EATT_READY) {
            // report disconnected if last channel closed
            uint8_t buffer[20];
            uint16_t len = hci_event_create_from_template_and_arguments(buffer, sizeof(buffer), &gatt_client_disconnected, gatt_client->con_handle);
            (*gatt_client->callback)(HCI_EVENT_PACKET, 0, buffer, len);
        }
    }
}

static gatt_client_t * gatt_client_le_enhanced_get_context_for_l2cap_cid(uint16_t l2cap_cid, gatt_client_t ** out_eatt_client){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &gatt_client_connections);
    while (btstack_linked_list_iterator_has_next(&it)) {
        gatt_client_t * gatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);
        btstack_linked_list_iterator_t it2;
        btstack_linked_list_iterator_init(&it2, &gatt_client->eatt_clients);
        while (btstack_linked_list_iterator_has_next(&it2)) {
            gatt_client_t * eatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it2);
            if (eatt_client->l2cap_cid == l2cap_cid){
                *out_eatt_client = eatt_client;
                return gatt_client;
            }
        }
    }
    return NULL;
}

static void gatt_client_le_enhanced_setup_l2cap_channel(gatt_client_t * gatt_client){
    uint8_t num_channels = gatt_client->eatt_num_clients;

    // setup channels
    uint16_t buffer_size_per_client = gatt_client->eatt_storage_size / num_channels;
    uint16_t max_mtu = (buffer_size_per_client - REPORT_PREBUFFER_HEADER) / 2;
    uint8_t * receive_buffers[MAX_NR_EATT_CHANNELS];
    uint16_t  new_cids[MAX_NR_EATT_CHANNELS];
    memset(gatt_client->eatt_storage_buffer, 0, gatt_client->eatt_storage_size);
    uint8_t i;
    for (i=0;i<gatt_client->eatt_num_clients; i++){
        receive_buffers[i] = &gatt_client->eatt_storage_buffer[REPORT_PREBUFFER_HEADER];
        gatt_client->eatt_storage_buffer += REPORT_PREBUFFER_HEADER + max_mtu;
    }

    log_info("%u EATT clients with receive buffer size %u", gatt_client->eatt_num_clients, buffer_size_per_client);

    uint8_t status = l2cap_ecbm_create_channels(&gatt_client_le_enhanced_packet_handler,
                                                gatt_client->con_handle,
                                                gatt_client->security_level,
                                                BLUETOOTH_PSM_EATT, num_channels,
                                                L2CAP_LE_AUTOMATIC_CREDITS,
                                                buffer_size_per_client,
                                                receive_buffers,
                                                new_cids);

    if (status == ERROR_CODE_SUCCESS){
        i = 0;
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &gatt_client->eatt_clients);
        while (btstack_linked_list_iterator_has_next(&it)) {
            gatt_client_t *new_eatt_client = (gatt_client_t *) btstack_linked_list_iterator_next(&it);

            // init state with new cid and transmit buffer
            new_eatt_client->bearer_type = ATT_BEARER_ENHANCED_LE;
            new_eatt_client->con_handle = gatt_client->con_handle;
            new_eatt_client->mtu = 64;
            new_eatt_client->security_level = LEVEL_0;
            new_eatt_client->mtu_state = MTU_AUTO_EXCHANGE_DISABLED;
            new_eatt_client->state = P_W4_L2CAP_CONNECTION;
            new_eatt_client->l2cap_cid = new_cids[i];
            new_eatt_client->eatt_storage_buffer = gatt_client->eatt_storage_buffer;
            gatt_client->eatt_storage_buffer += max_mtu;
            i++;
        }
        gatt_client->eatt_state = GATT_CLIENT_EATT_L2CAP_SETUP;
    } else {
        gatt_client_le_enhanced_handle_connected(gatt_client, status);
    }
}

static void gatt_client_le_enhanced_retry(btstack_timer_source_t * ts){
    gatt_client_t * gatt_client = gatt_client_for_timer(ts);
    if (gatt_client != NULL){
        gatt_client->state = P_W4_L2CAP_CONNECTION;
        gatt_client_le_enhanced_setup_l2cap_channel(gatt_client);
    }
}

static void gatt_client_le_enhanced_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    gatt_client_t *gatt_client;
    gatt_client_t *eatt_client;
    hci_con_handle_t con_handle;
    uint16_t l2cap_cid;
    uint8_t status;
    gatt_client_characteristic_t characteristic;
    gatt_client_service_t service;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case GATT_EVENT_SERVICE_QUERY_RESULT:
                    con_handle = gatt_event_service_query_result_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(gatt_client->eatt_state == GATT_CLIENT_EATT_DISCOVER_GATT_SERVICE_W4_DONE);
                    gatt_event_service_query_result_get_service(packet, &service);
                    gatt_client->gatt_service_start_group_handle = service.start_group_handle;
                    gatt_client->gatt_service_end_group_handle = service.end_group_handle;
                    break;
                case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
                    con_handle = gatt_event_characteristic_value_query_result_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(gatt_client->eatt_state == GATT_CLIENT_EATT_READ_SERVER_SUPPORTED_FEATURES_W4_DONE);
                    if (gatt_event_characteristic_value_query_result_get_value_length(packet) >= 1) {
                        gatt_client->gatt_server_supported_features = gatt_event_characteristic_value_query_result_get_value(packet)[0];
                    }
                    break;
                case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
                    con_handle = gatt_event_characteristic_query_result_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(gatt_client->eatt_state == GATT_CLIENT_EATT_FIND_CLIENT_SUPPORTED_FEATURES_W4_DONE);
                    gatt_event_characteristic_query_result_get_characteristic(packet, &characteristic);
                    gatt_client->gatt_client_supported_features_handle = characteristic.value_handle;
                    break;
                case GATT_EVENT_QUERY_COMPLETE:
                    con_handle = gatt_event_query_complete_get_handle(packet);
                    gatt_client = gatt_client_get_context_for_handle(con_handle);
                    btstack_assert(gatt_client != NULL);
                    switch (gatt_client->eatt_state){
                        case GATT_CLIENT_EATT_DISCOVER_GATT_SERVICE_W4_DONE:
                            if (gatt_client->gatt_service_start_group_handle == 0){
                                gatt_client_le_enhanced_handle_connected(gatt_client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                            } else {
                                gatt_client->eatt_state = GATT_CLIENT_EATT_READ_SERVER_SUPPORTED_FEATURES_W2_SEND;
                            }
                            break;
                        case GATT_CLIENT_EATT_READ_SERVER_SUPPORTED_FEATURES_W4_DONE:
                            if ((gatt_client->gatt_server_supported_features & 1) == 0) {
                                gatt_client_le_enhanced_handle_connected(gatt_client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                            } else {
                                gatt_client->eatt_state = GATT_CLIENT_EATT_FIND_CLIENT_SUPPORTED_FEATURES_W2_SEND;
                            }
                            break;
                        case GATT_CLIENT_EATT_FIND_CLIENT_SUPPORTED_FEATURES_W4_DONE:
                            if (gatt_client->gatt_client_supported_features_handle == 0){
                                gatt_client_le_enhanced_handle_connected(gatt_client, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE);
                            } else {
                                gatt_client->eatt_state = GATT_CLIENT_EATT_WRITE_ClIENT_SUPPORTED_FEATURES_W2_SEND;
                            }
                            break;
                        case GATT_CLIENT_EATT_WRITE_ClIENT_SUPPORTED_FEATURES_W4_DONE:
                            gatt_client_le_enhanced_setup_l2cap_channel(gatt_client);
                            break;
                        default:
                            break;
                    }
                    break;
                case L2CAP_EVENT_ECBM_CHANNEL_OPENED:
                    l2cap_cid = l2cap_event_ecbm_channel_opened_get_local_cid(packet);
                    gatt_client = gatt_client_le_enhanced_get_context_for_l2cap_cid(l2cap_cid, &eatt_client);

                    btstack_assert(gatt_client != NULL);
                    btstack_assert(eatt_client != NULL);
                    btstack_assert(eatt_client->state == P_W4_L2CAP_CONNECTION);

                    status = l2cap_event_channel_opened_get_status(packet);
                    if (status == ERROR_CODE_SUCCESS){
                        eatt_client->state = P_READY;
                        eatt_client->mtu = l2cap_event_channel_opened_get_remote_mtu(packet);
                    } else {
                        eatt_client->state = P_L2CAP_CLOSED;
                    }
                    // connected if opened event for all channels received
                    if (gatt_client_le_enhanced_num_eatt_clients_in_state(gatt_client, P_W4_L2CAP_CONNECTION) == 0){
                        gatt_client_le_enhanced_handle_connected(gatt_client, ERROR_CODE_SUCCESS);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    l2cap_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    gatt_client = gatt_client_le_enhanced_get_context_for_l2cap_cid(l2cap_cid, &eatt_client);
                    btstack_assert(gatt_client != NULL);
                    btstack_assert(eatt_client != NULL);
                    gatt_client_le_enhanced_handle_ecbm_disconnected(gatt_client, eatt_client);
                    break;
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            gatt_client = gatt_client_le_enhanced_get_context_for_l2cap_cid(channel, &eatt_client);
            btstack_assert(gatt_client != NULL);
            btstack_assert(eatt_client != NULL);
            gatt_client_handle_att_response(eatt_client, packet, size);
            gatt_client_run();
            break;
        default:
            break;
    }
}

static bool gatt_client_le_enhanced_handle_can_send_query(gatt_client_t * gatt_client){
    uint8_t status = ERROR_CODE_SUCCESS;
    uint8_t gatt_client_supported_features = 0x06; // eatt + multiple value notifications
    switch (gatt_client->eatt_state){
        case GATT_CLIENT_EATT_DISCOVER_GATT_SERVICE_W2_SEND:
            gatt_client->gatt_service_start_group_handle = 0;
            gatt_client->eatt_state = GATT_CLIENT_EATT_DISCOVER_GATT_SERVICE_W4_DONE;
            status = gatt_client_discover_primary_services_by_uuid16(&gatt_client_le_enhanced_packet_handler,
                                                                     gatt_client->con_handle,
                                                                     ORG_BLUETOOTH_SERVICE_GENERIC_ATTRIBUTE);
            break;
        case GATT_CLIENT_EATT_READ_SERVER_SUPPORTED_FEATURES_W2_SEND:
            gatt_client->gatt_server_supported_features = 0;
            gatt_client->eatt_state = GATT_CLIENT_EATT_READ_SERVER_SUPPORTED_FEATURES_W4_DONE;
            status = gatt_client_read_value_of_characteristics_by_uuid16(&gatt_client_le_enhanced_packet_handler,
                                                                         gatt_client->con_handle,
                                                                         gatt_client->gatt_service_start_group_handle,
                                                                         gatt_client->gatt_service_end_group_handle,
                                                                         ORG_BLUETOOTH_CHARACTERISTIC_SERVER_SUPPORTED_FEATURES);
            return true;
        case GATT_CLIENT_EATT_FIND_CLIENT_SUPPORTED_FEATURES_W2_SEND:
            gatt_client->gatt_client_supported_features_handle = 0;
            gatt_client->eatt_state = GATT_CLIENT_EATT_FIND_CLIENT_SUPPORTED_FEATURES_W4_DONE;
            status = gatt_client_discover_characteristics_for_handle_range_by_uuid16(&gatt_client_le_enhanced_packet_handler,
                                                                                     gatt_client->con_handle,
                                                                                     gatt_client->gatt_service_start_group_handle,
                                                                                     gatt_client->gatt_service_end_group_handle,
                                                                                     ORG_BLUETOOTH_CHARACTERISTIC_CLIENT_SUPPORTED_FEATURES);
            return true;
        case GATT_CLIENT_EATT_WRITE_ClIENT_SUPPORTED_FEATURES_W2_SEND:
            gatt_client->eatt_state = GATT_CLIENT_EATT_WRITE_ClIENT_SUPPORTED_FEATURES_W4_DONE;
            status = gatt_client_write_value_of_characteristic(&gatt_client_le_enhanced_packet_handler, gatt_client->con_handle,
                                                               gatt_client->gatt_client_supported_features_handle, 1,
                                                               &gatt_client_supported_features);
            return true;
        default:
            break;
    }
    btstack_assert(status == ERROR_CODE_SUCCESS);
    UNUSED(status);
    return false;
}

uint8_t gatt_client_le_enhanced_connect(btstack_packet_handler_t callback, hci_con_handle_t con_handle, uint8_t num_channels, uint8_t * storage_buffer, uint16_t storage_size) {
    gatt_client_t * gatt_client;
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, &gatt_client);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (gatt_client->eatt_state != GATT_CLIENT_EATT_IDLE){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // need one buffer for sending and one for receiving. Receiving includes pre-buffer for reports
    uint16_t buffer_size_per_client = storage_size / num_channels;
    uint16_t max_mtu = (buffer_size_per_client - REPORT_PREBUFFER_HEADER) / 2;
    if (max_mtu < 64) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    if ((num_channels == 0) || (num_channels > MAX_NR_EATT_CHANNELS)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // create max num_channel eatt clients
    uint8_t i;
    btstack_linked_list_t eatt_clients = NULL;
    for (i=0;i<num_channels;i++) {
        gatt_client_t * new_gatt_client = btstack_memory_gatt_client_get();
        if (new_gatt_client == NULL) {
            break;
        }
        btstack_linked_list_add(&eatt_clients, (btstack_linked_item_t*)new_gatt_client);
    }

    if (i != num_channels){
        while (true){
            gatt_client = (gatt_client_t *) btstack_linked_list_pop(&eatt_clients);
            if (gatt_client == NULL) {
                break;
            }
            btstack_memory_gatt_client_free(gatt_client);
        }
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    hci_connection->att_server.eatt_outgoing_active = true;

    gatt_client->callback = callback;
    gatt_client->eatt_num_clients   = num_channels;
    gatt_client->eatt_storage_buffer = storage_buffer;
    gatt_client->eatt_storage_size   = storage_size;
    gatt_client->eatt_clients = eatt_clients;
    gatt_client->eatt_state = GATT_CLIENT_EATT_DISCOVER_GATT_SERVICE_W2_SEND;
    gatt_client_notify_can_send_query(gatt_client);

    return ERROR_CODE_SUCCESS;
}

void gatt_client_le_enhanced_enable(bool enable){
    gatt_client_eatt_enabled = enable;
}


#endif

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
void gatt_client_att_packet_handler_fuzz(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    gatt_client_att_packet_handler(packet_type, handle, packet, size);
}

uint8_t gatt_client_get_client(hci_con_handle_t con_handle, gatt_client_t ** out_gatt_client){
    uint8_t status = gatt_client_provide_context_for_handle(con_handle, out_gatt_client);
    return status;
}
#endif
