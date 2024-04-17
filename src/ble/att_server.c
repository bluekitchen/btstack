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

#define BTSTACK_FILE__ "att_server.c"


//
// ATT Server Globals
//

#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_config.h"

#include "ble/att_dispatch.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/core.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_tlv.h"
#ifdef ENABLE_LE_SIGNED_WRITE
#include "ble/sm.h"
#include "bluetooth_psm.h"

#endif

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#ifndef NVN_NUM_GATT_SERVER_CCC
#define NVN_NUM_GATT_SERVER_CCC 20
#endif

#define ATT_SERVICE_FLAGS_DELAYED_RESPONSE (1<<0u)

static void att_run_for_context(att_server_t * att_server, att_connection_t * att_connection);
static att_write_callback_t att_server_write_callback_for_handle(uint16_t handle);
static btstack_packet_handler_t att_server_packet_handler_for_handle(uint16_t handle);
static void att_server_handle_can_send_now(void);
static void att_server_persistent_ccc_restore(att_server_t * att_server, att_connection_t * att_connection);
static void att_server_persistent_ccc_clear(att_server_t * att_server);
static void att_server_handle_att_pdu(att_server_t * att_server, att_connection_t * att_connection, uint8_t * packet, uint16_t size);

typedef enum {
    ATT_SERVER_RUN_PHASE_1_REQUESTS = 0,
    ATT_SERVER_RUN_PHASE_2_INDICATIONS,
    ATT_SERVER_RUN_PHASE_3_NOTIFICATIONS,
} att_server_run_phase_t;

//
typedef struct {
    uint32_t seq_nr;
    uint16_t att_handle;
    uint8_t  value;
    uint8_t  device_index;
} persistent_ccc_entry_t;

// global
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static btstack_packet_handler_t               att_client_packet_handler;
static btstack_linked_list_t                  service_handlers;
static btstack_context_callback_registration_t att_client_waiting_for_can_send_registration;

static att_read_callback_t                    att_server_client_read_callback;
static att_write_callback_t                   att_server_client_write_callback;

// round robin
static hci_con_handle_t att_server_last_can_send_now = HCI_CON_HANDLE_INVALID;

static uint8_t att_server_flags;

#ifdef ENABLE_GATT_OVER_EATT
typedef struct {
    btstack_linked_item_t item;
    att_server_t     att_server;
    att_connection_t att_connection;
    uint8_t * receive_buffer;
    uint8_t * send_buffer;
} att_server_eatt_bearer_t;
static att_server_eatt_bearer_t * att_server_eatt_bearer_for_con_handle(hci_con_handle_t con_handle);
static btstack_linked_list_t att_server_eatt_bearer_pool;
static btstack_linked_list_t att_server_eatt_bearer_active;
#endif

#ifdef ENABLE_LE_SIGNED_WRITE
static bool att_server_connections_for_state(att_server_state_t state, att_server_t ** att_server_out, att_connection_t ** att_connection_out){
    btstack_linked_list_iterator_t it;
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * hci_connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if (hci_connection->att_server.state == state) {
            *att_server_out     = &hci_connection->att_server;
            *att_connection_out = &hci_connection->att_connection;
            return true;
        }
    }

#ifdef ENABLE_GATT_OVER_EATT
    btstack_linked_list_iterator_init(&it, &att_server_eatt_bearer_active);
    while(btstack_linked_list_iterator_has_next(&it)){
        att_server_eatt_bearer_t * eatt_bearer = (att_server_eatt_bearer_t *) btstack_linked_list_iterator_next(&it);
        if (eatt_bearer->att_server.state == state) {
            *att_server_out     = &eatt_bearer->att_server;
            *att_connection_out = &eatt_bearer->att_connection;
            return true;
        }
    }
#endif

    return false;
}
#endif

static void att_server_request_can_send_now(att_server_t * att_server, att_connection_t * att_connection ){
    switch (att_server->bearer_type){
        case ATT_BEARER_UNENHANCED_LE:
#ifdef ENABLE_GATT_OVER_CLASSIC
        case ATT_BEARER_UNENHANCED_CLASSIC:
            /* fall through */
#endif
            att_dispatch_server_request_can_send_now_event(att_connection->con_handle);
            break;
#ifdef ENABLE_GATT_OVER_EATT
        case ATT_BEARER_ENHANCED_LE:
            l2cap_request_can_send_now_event(att_server->l2cap_cid);
            break;
#endif
        default:
            btstack_unreachable();
            break;
    }
}

static bool att_server_can_send_packet(att_server_t * att_server, att_connection_t * att_connection){
    switch (att_server->bearer_type) {
        case ATT_BEARER_UNENHANCED_LE:
#ifdef ENABLE_GATT_OVER_CLASSIC
        case ATT_BEARER_UNENHANCED_CLASSIC:
            /* fall through */
#endif
            return att_dispatch_server_can_send_now(att_connection->con_handle);
#ifdef ENABLE_GATT_OVER_EATT
        case ATT_BEARER_ENHANCED_LE:
            return l2cap_can_send_packet_now(att_server->l2cap_cid);
#endif
        default:
            btstack_unreachable();
            break;
    }
    return false;
}

static void att_handle_value_indication_notify_client(uint8_t status, uint16_t client_handle, uint16_t attribute_handle){
    btstack_packet_handler_t packet_handler = att_server_packet_handler_for_handle(attribute_handle);
    if (!packet_handler) return;
    
    uint8_t event[7];
    int pos = 0;
    event[pos++] = ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE;
    event[pos++] = sizeof(event) - 2u;
    event[pos++] = status;
    little_endian_store_16(event, pos, client_handle);
    pos += 2;
    little_endian_store_16(event, pos, attribute_handle);
    (*packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_emit_event_to_all(const uint8_t * event, uint16_t size){
    // dispatch to app level handler
    if (att_client_packet_handler != NULL){
        (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, (uint8_t*) event, size);
    }

    // dispatch to service handlers
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &service_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        att_service_handler_t * handler = (att_service_handler_t*) btstack_linked_list_iterator_next(&it);
        if (!handler->packet_handler) continue;
        (*handler->packet_handler)(HCI_EVENT_PACKET, 0, (uint8_t*) event, size);
    }
}

static void att_emit_mtu_event(hci_con_handle_t con_handle, uint16_t mtu){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = ATT_EVENT_MTU_EXCHANGE_COMPLETE;
    event[pos++] = sizeof(event) - 2u;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, mtu);

    // also dispatch to GATT Clients
    att_dispatch_server_mtu_exchanged(con_handle, mtu);

    // dispatch to app level handler and service handlers
    att_emit_event_to_all(&event[0], sizeof(event));
}

static void att_emit_can_send_now_event(void * context){
    UNUSED(context);
    if (!att_client_packet_handler) return;

    uint8_t event[] = { ATT_EVENT_CAN_SEND_NOW, 0};
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_emit_connected_event(att_server_t * att_server,  att_connection_t * att_connection){
    uint8_t event[11];
    int pos = 0;
    event[pos++] = ATT_EVENT_CONNECTED;
    event[pos++] = sizeof(event) - 2u;
    event[pos++] = att_server->peer_addr_type;
    reverse_bd_addr(att_server->peer_address, &event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, att_connection->con_handle);
    pos += 2;

    // dispatch to app level handler and service handlers
    att_emit_event_to_all(&event[0], sizeof(event));
}


static void att_emit_disconnected_event(uint16_t con_handle){
    uint8_t event[4];
    int pos = 0;
    event[pos++] = ATT_EVENT_DISCONNECTED;
    event[pos++] = sizeof(event) - 2u;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;

    // dispatch to app level handler and service handlers
    att_emit_event_to_all(&event[0], sizeof(event));
}

static void att_handle_value_indication_timeout(btstack_timer_source_t *ts){
    void * context = btstack_run_loop_get_timer_context(ts);
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return;
    // @note: after a transaction timeout, no more requests shall be sent over this ATT Bearer
    // (that's why we don't reset the value_indication_handle)
    att_server_t * att_server = &hci_connection->att_server;
    uint16_t att_handle = att_server->value_indication_handle;
    att_connection_t * att_connection = &hci_connection->att_connection;
    att_handle_value_indication_notify_client((uint8_t)ATT_HANDLE_VALUE_INDICATION_TIMEOUT, att_connection->con_handle, att_handle);
}

static void att_server_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own l2cap events
    
    att_server_t * att_server;
    att_connection_t * att_connection;
    hci_con_handle_t con_handle;
    hci_connection_t * hci_connection;

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_META_GAP:
                    switch (hci_event_gap_meta_get_subevent_code(packet)) {
                        case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                            con_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
                            hci_connection = hci_connection_for_handle(con_handle);
                            if (!hci_connection) break;
                            att_server = &hci_connection->att_server;
                        	// store connection info
                        	att_server->peer_addr_type = gap_subevent_le_connection_complete_get_peer_address_type(packet);
                            gap_subevent_le_connection_complete_get_peer_address(packet, att_server->peer_address);
                            att_connection = &hci_connection->att_connection;
                            att_connection->con_handle = con_handle;
                            // reset connection properties
                            att_server->state = ATT_SERVER_IDLE;
                            att_server->bearer_type = ATT_BEARER_UNENHANCED_LE;
                            att_connection->mtu = ATT_DEFAULT_MTU;
                            att_connection->max_mtu = l2cap_max_le_mtu();
                            if (att_connection->max_mtu > ATT_REQUEST_BUFFER_SIZE){
                                att_connection->max_mtu = ATT_REQUEST_BUFFER_SIZE;
                            }
                            att_connection->encryption_key_size = 0u;
                            att_connection->authenticated = 0u;
		                	att_connection->authorized = 0u;
                            // workaround: identity resolving can already be complete, at least store result
                            att_server->ir_le_device_db_index = sm_le_device_index(con_handle);
                            att_server->ir_lookup_active = false;
                            att_server->pairing_active = false;
                            // notify all - new
                            att_emit_connected_event(att_server, att_connection);
                            break;
                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // forward LE Connection Complete event to keep backward compatibility
                            // deprecated: please register hci handler with hci_add_event_handler or handle ATT_EVENT_CONNECTED
                            att_emit_event_to_all(packet, size);
                            break;
                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE:
                case HCI_EVENT_ENCRYPTION_CHANGE_V2:
                case HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE:
                	// check handle
                    con_handle = little_endian_read_16(packet, 3);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    if (gap_get_connection_type(con_handle) != GAP_CONNECTION_LE) break;
                    // update security params
                    att_server = &hci_connection->att_server;
                    att_connection = &hci_connection->att_connection;
                    att_connection->encryption_key_size = gap_encryption_key_size(con_handle);
                    att_connection->authenticated = gap_authenticated(con_handle) ? 1 : 0;
                    att_connection->secure_connection = gap_secure_connection(con_handle) ? 1 : 0;
                    log_info("encrypted key size %u, authenticated %u, secure connection %u",
                        att_connection->encryption_key_size, att_connection->authenticated, att_connection->secure_connection);
                    if (hci_event_packet_get_type(packet) == HCI_EVENT_ENCRYPTION_CHANGE){
                        // restore CCC values when encrypted for LE Connections
                        if (hci_event_encryption_change_get_encryption_enabled(packet) != 0){
                            att_server_persistent_ccc_restore(att_server, att_connection);
                        } 
                    }
                    att_run_for_context(att_server, att_connection);
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // check handle
                    con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    att_server = &hci_connection->att_server;
                    att_connection = &hci_connection->att_connection;
                    att_clear_transaction_queue(att_connection);
                    att_connection->con_handle = 0;
                    att_server->pairing_active = false;
                    att_server->state = ATT_SERVER_IDLE;
                    if (att_server->value_indication_handle != 0u){
                        btstack_run_loop_remove_timer(&att_server->value_indication_timer);
                        uint16_t att_handle = att_server->value_indication_handle;
                        att_server->value_indication_handle = 0u; // reset error state
                        att_handle_value_indication_notify_client((uint8_t)ATT_HANDLE_VALUE_INDICATION_DISCONNECT, att_connection->con_handle, att_handle);
                    }
                    // notify all - new
                    att_emit_disconnected_event(con_handle);
                    // notify all - old
                    att_emit_event_to_all(packet, size);
                    break;
                    
                // Identity Resolving
                case SM_EVENT_IDENTITY_RESOLVING_STARTED:
                    con_handle = sm_event_identity_resolving_started_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    att_server = &hci_connection->att_server;
                    log_info("SM_EVENT_IDENTITY_RESOLVING_STARTED");
                    att_server->ir_lookup_active = true;
                    break;
                case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
                    con_handle = sm_event_identity_created_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) return;
                    att_connection = &hci_connection->att_connection;
                    att_server = &hci_connection->att_server;
                    att_server->ir_lookup_active = false;
                    att_server->ir_le_device_db_index = sm_event_identity_resolving_succeeded_get_index(packet);
                    log_info("SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED");
                    att_run_for_context(att_server, att_connection);
                    break;
                case SM_EVENT_IDENTITY_RESOLVING_FAILED:
                    con_handle = sm_event_identity_resolving_failed_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    att_connection = &hci_connection->att_connection;
                    att_server = &hci_connection->att_server;
                    log_info("SM_EVENT_IDENTITY_RESOLVING_FAILED");
                    att_server->ir_lookup_active = false;
                    att_server->ir_le_device_db_index = -1;
                    att_run_for_context(att_server, att_connection);
                    break;

                // Pairing started - delete stored CCC values
                // - assumes pairing indicates either new device or re-pairing, in both cases there should be no stored CCC values
                // - assumes that all events have the con handle as the first field
                case SM_EVENT_JUST_WORKS_REQUEST:
                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                case SM_EVENT_PASSKEY_INPUT_NUMBER:
                case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
                    con_handle = sm_event_just_works_request_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    att_server = &hci_connection->att_server;
                    log_info("SM Pairing started");
                    att_server->pairing_active = true;
                    if (att_server->ir_le_device_db_index < 0) break;
                    att_server_persistent_ccc_clear(att_server);
                    // index not valid anymore
                    att_server->ir_le_device_db_index = -1;
                    break;

                // Bonding completed
                case SM_EVENT_IDENTITY_CREATED:
                    con_handle = sm_event_identity_created_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) return;
                    att_connection = &hci_connection->att_connection;
                    att_server = &hci_connection->att_server;
                    att_server->pairing_active = false;
                    att_server->ir_le_device_db_index = sm_event_identity_created_get_index(packet);
                    att_run_for_context(att_server, att_connection);
                    break;

                // Pairing complete (with/without bonding=storing of pairing information)
                case SM_EVENT_PAIRING_COMPLETE:
                    con_handle = sm_event_pairing_complete_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) return;
                    att_connection = &hci_connection->att_connection;
                    att_server = &hci_connection->att_server;
                    att_server->pairing_active = false;
                    att_run_for_context(att_server, att_connection);
                    break;

                // Authorization
                case SM_EVENT_AUTHORIZATION_RESULT: {
                    con_handle = sm_event_authorization_result_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    att_connection = &hci_connection->att_connection;
                    att_server = &hci_connection->att_server;
                    att_connection->authorized = sm_event_authorization_result_get_authorization_result(packet);
                    att_server_request_can_send_now(att_server, att_connection);
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

static uint8_t
att_server_send_prepared(const att_server_t *att_server, const att_connection_t *att_connection, uint8_t *buffer,
                         uint16_t size) {
    UNUSED(buffer);
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (att_server->bearer_type) {
        case ATT_BEARER_UNENHANCED_LE:
            status = l2cap_send_prepared_connectionless(att_connection->con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, size);
            break;
#ifdef ENABLE_GATT_OVER_CLASSIC
        case ATT_BEARER_UNENHANCED_CLASSIC:
            status = l2cap_send_prepared(att_server->l2cap_cid, size);
            break;
#endif
#ifdef ENABLE_GATT_OVER_EATT
        case ATT_BEARER_ENHANCED_LE:
            btstack_assert(buffer != NULL);
            status = l2cap_send(att_server->l2cap_cid, buffer, size);
            break;
#endif
        default:
            btstack_unreachable();
            break;
    }
    return status;
}

#ifdef ENABLE_LE_SIGNED_WRITE
static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    att_server_t * att_server = NULL;
    att_connection_t * att_connection = NULL;
    bool found = att_server_connections_for_state(ATT_SERVER_W4_SIGNED_WRITE_VALIDATION, &att_server, &att_connection);

    if (found == false){
        return;
    }

    uint8_t hash_flipped[8];
    reverse_64(hash, hash_flipped);
    if (memcmp(hash_flipped, &att_server->request_buffer[att_server->request_size-8], 8) != 0){
        log_info("ATT Signed Write, invalid signature");
#ifdef ENABLE_TESTING_SUPPORT
        printf("ATT Signed Write, invalid signature\n");
#endif
        att_server->state = ATT_SERVER_IDLE;
        return;
    }
    log_info("ATT Signed Write, valid signature");
#ifdef ENABLE_TESTING_SUPPORT
    printf("ATT Signed Write, valid signature\n");
#endif

    // update sequence number
    uint32_t counter_packet = little_endian_read_32(att_server->request_buffer, att_server->request_size-12);
    le_device_db_remote_counter_set(att_server->ir_le_device_db_index, counter_packet+1);
    att_server->state = ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
    att_server_request_can_send_now(att_server, att_connection);
}
#endif

#ifdef ENABLE_ATT_DELAYED_RESPONSE
static void att_server_handle_response_pending(att_server_t *att_server, const att_connection_t *att_connection,
                                               const uint8_t *eatt_buffer,
                                               uint16_t att_response_size) {
    // free reserved buffer
    if (eatt_buffer == NULL){
        l2cap_release_packet_buffer();
    }

    // update state
    att_server->state = ATT_SERVER_RESPONSE_PENDING;

    // callback with handle ATT_READ_RESPONSE_PENDING for reads
    if (att_response_size == ATT_READ_RESPONSE_PENDING){
        // notify services that returned response pending
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &service_handlers);
        while (btstack_linked_list_iterator_has_next(&it)) {
            att_service_handler_t *handler = (att_service_handler_t *) btstack_linked_list_iterator_next(&it);
            if ((handler->flags & ATT_SERVICE_FLAGS_DELAYED_RESPONSE) != 0){
                handler->flags &= ~ATT_SERVICE_FLAGS_DELAYED_RESPONSE;
                handler->read_callback(att_connection->con_handle, ATT_READ_RESPONSE_PENDING, 0, NULL, 0);
            }
        }
        // notify main read callback if it returned response pending
        if ((att_server_flags & ATT_SERVICE_FLAGS_DELAYED_RESPONSE) != 0){
            // flag was set by read callback
            btstack_assert(att_server_client_read_callback != NULL);
            (*att_server_client_read_callback)(att_connection->con_handle, ATT_READ_RESPONSE_PENDING, 0, NULL, 0);
        }
    }
}
#endif

// pre: att_server->state == ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED
// pre: can send now
// uses l2cap outgoing buffer if no eatt_buffer provided
// returns: 1 if packet was sent
static int
att_server_process_validated_request(att_server_t *att_server, att_connection_t *att_connection, uint8_t *eatt_buffer) {

    uint8_t * att_response_buffer;
    if (eatt_buffer != NULL){
        att_response_buffer = eatt_buffer;
    } else {
        l2cap_reserve_packet_buffer();
        att_response_buffer = l2cap_get_outgoing_buffer();
    }

    uint16_t  att_response_size = 0;
    // Send Error Response for MTU Request over connection-oriented channel
    if ((att_server->bearer_type != ATT_BEARER_UNENHANCED_LE) && (att_server->request_buffer[0] == ATT_EXCHANGE_MTU_REQUEST)){
        att_response_size = 5;
        att_response_buffer[0] = ATT_ERROR_RESPONSE;
        att_response_buffer[1] = ATT_EXCHANGE_MTU_REQUEST;
        att_response_buffer[2] = 0;
        att_response_buffer[3] = 0;
        att_response_buffer[4] = ATT_ERROR_REQUEST_NOT_SUPPORTED;
    } else {
        att_response_size = att_handle_request(att_connection, att_server->request_buffer, att_server->request_size, att_response_buffer);
    }

#ifdef ENABLE_ATT_DELAYED_RESPONSE
    if ((att_response_size == ATT_READ_RESPONSE_PENDING) || (att_response_size == ATT_INTERNAL_WRITE_RESPONSE_PENDING)){
        att_server_handle_response_pending(att_server, att_connection, eatt_buffer, att_response_size);
        return 0;
    }
#endif

    // intercept "insufficient authorization" for authenticated connections to allow for user authorization
    if ((att_response_size     >= 4u)
    && (att_response_buffer[0] == ATT_ERROR_RESPONSE)
    && (att_response_buffer[4] == ATT_ERROR_INSUFFICIENT_AUTHORIZATION)
    && (att_connection->authenticated != 0u)){

        switch (gap_authorization_state(att_connection->con_handle)){
            case AUTHORIZATION_UNKNOWN:
                if (eatt_buffer == NULL){
                    l2cap_release_packet_buffer();
                }
                sm_request_pairing(att_connection->con_handle);
                return 0;
            case AUTHORIZATION_PENDING:
                if (eatt_buffer == NULL){
                    l2cap_release_packet_buffer();
                }
                return 0;
            default:
                break;
        }
    }

    att_server->state = ATT_SERVER_IDLE;
    if (att_response_size == 0u) {
        if (eatt_buffer == NULL){
            l2cap_release_packet_buffer();
        }
        return 0;
    }

    (void) att_server_send_prepared(att_server, att_connection, eatt_buffer, att_response_size);

    // notify client about MTU exchange result
    if (att_response_buffer[0] == ATT_EXCHANGE_MTU_RESPONSE){
        att_emit_mtu_event(att_connection->con_handle, att_connection->mtu);
    }
    return 1;
}

#ifdef ENABLE_ATT_DELAYED_RESPONSE
uint8_t att_server_response_ready(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    att_server_t * att_server = &hci_connection->att_server;
    att_connection_t * att_connection = &hci_connection->att_connection;
    if (att_server->state != ATT_SERVER_RESPONSE_PENDING)   return ERROR_CODE_COMMAND_DISALLOWED;

    att_server->state = ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
    att_server_request_can_send_now(att_server, att_connection);
    return ERROR_CODE_SUCCESS;
}
#endif

static void att_run_for_context(att_server_t * att_server, att_connection_t * att_connection){
    switch (att_server->state){
        case ATT_SERVER_REQUEST_RECEIVED:
            switch (att_server->bearer_type){
                case ATT_BEARER_UNENHANCED_LE:
                    // wait until re-encryption as central is complete
                    if (gap_reconnect_security_setup_active(att_connection->con_handle)) {
                        return;
                    };
                    break;
#if defined(ENABLE_GATT_OVER_CLASSIC) || defined (ENABLE_GATT_OVER_EATT)
#ifdef ENABLE_GATT_OVER_CLASSIC
                case ATT_BEARER_UNENHANCED_CLASSIC:
                    /* fall through */
#endif
#ifdef ENABLE_GATT_OVER_EATT
                case ATT_BEARER_ENHANCED_LE:
#endif
                    // ok
                    break;
#endif
                default:
                    btstack_unreachable();
                    break;
            }

            // wait until pairing is complete
            if (att_server->pairing_active) break;

#ifdef ENABLE_LE_SIGNED_WRITE
            if (att_server->request_buffer[0] == ATT_SIGNED_WRITE_COMMAND){
                log_info("ATT Signed Write!");
                if (!sm_cmac_ready()) {
                    log_info("ATT Signed Write, sm_cmac engine not ready. Abort");
                    att_server->state = ATT_SERVER_IDLE;
                    return;
                }  
                if (att_server->request_size < (3 + 12)) {
                    log_info("ATT Signed Write, request to short. Abort.");
                    att_server->state = ATT_SERVER_IDLE;
                    return;
                }
                if (att_server->ir_lookup_active){
                    return;
                }
                if (att_server->ir_le_device_db_index < 0){
                    log_info("ATT Signed Write, CSRK not available");
                    att_server->state = ATT_SERVER_IDLE;
                    return;
                }

                // check counter
                uint32_t counter_packet = little_endian_read_32(att_server->request_buffer, att_server->request_size-12);
                uint32_t counter_db     = le_device_db_remote_counter_get(att_server->ir_le_device_db_index);
                log_info("ATT Signed Write, DB counter %"PRIu32", packet counter %"PRIu32, counter_db, counter_packet);
                if (counter_packet < counter_db){
                    log_info("ATT Signed Write, db reports higher counter, abort");
                    att_server->state = ATT_SERVER_IDLE;
                    return;
                }

                // signature is { sequence counter, secure hash }
                sm_key_t csrk;
                le_device_db_remote_csrk_get(att_server->ir_le_device_db_index, csrk);
                att_server->state = ATT_SERVER_W4_SIGNED_WRITE_VALIDATION;
                log_info("Orig Signature: ");
                log_info_hexdump( &att_server->request_buffer[att_server->request_size-8], 8);
                uint16_t attribute_handle = little_endian_read_16(att_server->request_buffer, 1);
                sm_cmac_signed_write_start(csrk, att_server->request_buffer[0], attribute_handle, att_server->request_size - 15, &att_server->request_buffer[3], counter_packet, att_signed_write_handle_cmac_result);
                return;
            } 
#endif
            // move on
            att_server->state = ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
            att_server_request_can_send_now(att_server, att_connection);
            break;

        default:
            break;
    }   
}

static bool att_server_data_ready_for_phase(att_server_t * att_server,  att_server_run_phase_t phase){
    switch (phase){
        case ATT_SERVER_RUN_PHASE_1_REQUESTS:
            return att_server->state == ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
        case ATT_SERVER_RUN_PHASE_2_INDICATIONS:
             return (!btstack_linked_list_empty(&att_server->indication_requests) && (att_server->value_indication_handle == 0u));
        case ATT_SERVER_RUN_PHASE_3_NOTIFICATIONS:
            return (!btstack_linked_list_empty(&att_server->notification_requests));
        default:
            btstack_assert(false);
            return false;
    }
}

static void att_server_trigger_send_for_phase(att_server_t * att_server, att_connection_t * att_connection, att_server_run_phase_t phase){
    btstack_context_callback_registration_t * client;
    switch (phase){
        case ATT_SERVER_RUN_PHASE_1_REQUESTS:
            att_server_process_validated_request(att_server, att_connection, NULL);
            break;
        case ATT_SERVER_RUN_PHASE_2_INDICATIONS:
            client = (btstack_context_callback_registration_t*) att_server->indication_requests;
            btstack_linked_list_remove(&att_server->indication_requests, (btstack_linked_item_t *) client);
            client->callback(client->context);
            break;
       case ATT_SERVER_RUN_PHASE_3_NOTIFICATIONS:
            client = (btstack_context_callback_registration_t*) att_server->notification_requests;
            btstack_linked_list_remove(&att_server->notification_requests, (btstack_linked_item_t *) client);
            client->callback(client->context);
            break;
        default:
            btstack_assert(false);
            break;
    }
}

static void att_server_handle_can_send_now(void){

    hci_con_handle_t last_send_con_handle = HCI_CON_HANDLE_INVALID;
    hci_connection_t * request_hci_connection   = NULL;
    bool can_send_now = true;
    uint8_t phase_index;

    for (phase_index = ATT_SERVER_RUN_PHASE_1_REQUESTS; phase_index <= ATT_SERVER_RUN_PHASE_3_NOTIFICATIONS; phase_index++){
        att_server_run_phase_t phase = (att_server_run_phase_t) phase_index;
        hci_con_handle_t skip_connections_until = att_server_last_can_send_now;
        while (true){
            btstack_linked_list_iterator_t it;
            hci_connections_get_iterator(&it);
            while(btstack_linked_list_iterator_has_next(&it)){
                hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
                att_server_t * att_server = &connection->att_server;
                att_connection_t * att_connection = &connection->att_connection;

                bool data_ready = att_server_data_ready_for_phase(att_server, phase);

                // log_debug("phase %u, handle 0x%04x, skip until 0x%04x, data ready %u", phase, att_connection->con_handle, skip_connections_until, data_ready);

                // skip until last sender found (which is also skipped)
                if (skip_connections_until != HCI_CON_HANDLE_INVALID){
                    if (data_ready && (request_hci_connection == NULL)){
                        request_hci_connection = connection;
                    }
                    if (skip_connections_until == att_connection->con_handle){
                        skip_connections_until = HCI_CON_HANDLE_INVALID;
                    }
                    continue;
                };

                if (data_ready){
                    if (can_send_now){
                        att_server_trigger_send_for_phase(att_server, att_connection, phase);
                        last_send_con_handle = att_connection->con_handle;
                        can_send_now = att_server_can_send_packet(att_server, att_connection);
                        data_ready = att_server_data_ready_for_phase(att_server, phase);
                        if (data_ready && (request_hci_connection == NULL)){
                            request_hci_connection = connection;
                        }
                    } else {
                        request_hci_connection = connection;
                        break;
                    }
                }
            }

            // stop skipping (handles disconnect by last send connection)
            skip_connections_until = HCI_CON_HANDLE_INVALID;

            // Exit loop, if we cannot send
            if (!can_send_now) break;

            // Exit loop, if we can send but there are also no further request
            if (request_hci_connection == NULL) break;

            // Finally, if we still can send and there are requests, just try again
            request_hci_connection = NULL;
        }
        // update last send con handle for round robin
        if (last_send_con_handle != HCI_CON_HANDLE_INVALID){
            att_server_last_can_send_now = last_send_con_handle;
        }
    }

    if (request_hci_connection == NULL) return;

    att_server_t * att_server = &request_hci_connection->att_server;
    att_connection_t * att_connection = &request_hci_connection->att_connection;
    att_server_request_can_send_now(att_server, att_connection);
}

static void att_server_handle_att_pdu(att_server_t * att_server, att_connection_t * att_connection, uint8_t * packet, uint16_t size){

    uint8_t opcode  = packet[0u];
    uint8_t method  = opcode & 0x03fu;
    bool invalid = method > ATT_MULTIPLE_HANDLE_VALUE_NTF;
    bool command = (opcode & 0x40u) != 0u;

    // ignore invalid commands
    if (invalid && command){
        return;
    }

    // handle value indication confirms
    if ((opcode == ATT_HANDLE_VALUE_CONFIRMATION) && (att_server->value_indication_handle != 0u)){
        btstack_run_loop_remove_timer(&att_server->value_indication_timer);
        uint16_t att_handle = att_server->value_indication_handle;
        att_server->value_indication_handle = 0u;    
        att_handle_value_indication_notify_client(0u, att_connection->con_handle, att_handle);
        att_server_request_can_send_now(att_server, att_connection);
        return;
    }

    // directly process command
    // note: signed write cannot be handled directly as authentication needs to be verified
    if (opcode == ATT_WRITE_COMMAND){
        att_handle_request(att_connection, packet, size, NULL);
        return;
    }

    // check size
    if (size > sizeof(att_server->request_buffer)) {
        log_info("drop att pdu 0x%02x as size %u > att_server->request_buffer %u", packet[0], size, (int) sizeof(att_server->request_buffer));
        return;
    }

#ifdef ENABLE_LE_SIGNED_WRITE
    // abort signed write validation if a new request comes in (but finish previous signed write if possible)
    if (att_server->state == ATT_SERVER_W4_SIGNED_WRITE_VALIDATION){
        if (packet[0] == ATT_SIGNED_WRITE_COMMAND){
            log_info("skip new signed write request as previous is in validation");
            return;
        } else {
            log_info("abort signed write validation to process new request");
            att_server->state = ATT_SERVER_IDLE;
        }
    }
#endif
    // last request still in processing?
    if (att_server->state != ATT_SERVER_IDLE){
        log_info("skip att pdu 0x%02x as server not idle (state %u)", packet[0], att_server->state);
        return;
    }

    // store request
    att_server->state = ATT_SERVER_REQUEST_RECEIVED;
    att_server->request_size = size;
    (void)memcpy(att_server->request_buffer, packet, size);

    att_run_for_context(att_server, att_connection);
}

static void att_server_dispatch_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hci_connection_t * hci_connection;
    att_connection_t * att_connection;
    att_server_t     * att_server;
#ifdef ENABLE_GATT_OVER_CLASSIC
    hci_con_handle_t con_handle;
    bd_addr_t        address;
    btstack_linked_list_iterator_t it;
#endif

    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (packet[0]){
#ifdef ENABLE_GATT_OVER_CLASSIC
                case L2CAP_EVENT_CHANNEL_OPENED:
                    con_handle = l2cap_event_channel_opened_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    if (!hci_connection) break;
                    att_server = &hci_connection->att_server;
                    // store connection info
                    att_server->bearer_type = ATT_BEARER_UNENHANCED_CLASSIC;
                    att_server->peer_addr_type = BD_ADDR_TYPE_ACL;
                    l2cap_event_channel_opened_get_address(packet, att_server->peer_address);
                    att_connection = &hci_connection->att_connection;
                    att_connection->con_handle = con_handle;
                    // reset connection properties
                    att_server->state = ATT_SERVER_IDLE;
                    att_connection->mtu = l2cap_event_channel_opened_get_remote_mtu(packet);
                    att_connection->max_mtu = l2cap_max_mtu();
                    if (att_connection->max_mtu > ATT_REQUEST_BUFFER_SIZE){
                        att_connection->max_mtu = ATT_REQUEST_BUFFER_SIZE;
                    }

                    log_info("Connection opened %s, l2cap cid %04x, mtu %u", bd_addr_to_str(address), att_server->l2cap_cid, att_connection->mtu);

                    // update security params
                    att_connection->encryption_key_size = gap_encryption_key_size(con_handle);
                    att_connection->authenticated = gap_authenticated(con_handle);
                    att_connection->secure_connection = gap_secure_connection(con_handle);
                    log_info("encrypted key size %u, authenticated %u, secure connection %u",
                             att_connection->encryption_key_size, att_connection->authenticated, att_connection->secure_connection);

                    // notify connection opened
                    att_emit_connected_event(att_server, att_connection);

                    // restore persisten ccc if encrypted
                    if ( gap_security_level(con_handle) >= LEVEL_2){
                        att_server_persistent_ccc_restore(att_server, att_connection);
                    }
                    // TODO: what to do about le device db?
                    att_server->pairing_active = 0;
                    break;
#endif
                case L2CAP_EVENT_CAN_SEND_NOW:
                    att_server_handle_can_send_now();
                    break;
                case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
                    // GATT client has negotiated the mtu for this connection
                    hci_connection = hci_connection_for_handle(channel);
                    if (!hci_connection) break;
                    att_connection = &hci_connection->att_connection;
                    att_connection->mtu = little_endian_read_16(packet, 4);
                    break;
                default:
                    break;
            }
            break;

        case ATT_DATA_PACKET:
            log_debug("ATT Packet, handle 0x%04x", channel);
            hci_connection = hci_connection_for_handle(channel);
            if (!hci_connection) break;

            att_server = &hci_connection->att_server;
            att_connection = &hci_connection->att_connection;
            att_server_handle_att_pdu(att_server, att_connection, packet, size);
            break;

#ifdef ENABLE_GATT_OVER_CLASSIC
        case L2CAP_DATA_PACKET:
            hci_connections_get_iterator(&it);
            while(btstack_linked_list_iterator_has_next(&it)){
                hci_connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
                att_server = &hci_connection->att_server;
                att_connection = &hci_connection->att_connection;
                if (att_server->l2cap_cid == channel) {
                    att_server_handle_att_pdu(att_server, att_connection, packet, size);
                    break;
                }
            }
            break;
#endif

        default:
            break;
    }
}

// ---------------------
// persistent CCC writes
static uint32_t att_server_persistent_ccc_tag_for_index(uint8_t index){
    return ('B' << 24u) | ('T' << 16u) | ('C' << 8u) | index;
}

static void att_server_persistent_ccc_write(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t value){
    // lookup att_server instance
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return;
    att_server_t * att_server = &hci_connection->att_server;
    int le_device_index = att_server->ir_le_device_db_index;
    log_info("Store CCC value 0x%04x for handle 0x%04x of remote %s, le device id %d", value, att_handle, bd_addr_to_str(att_server->peer_address), le_device_index);

    // check if bonded
    if (le_device_index < 0) return;

    // get btstack_tlv
    const btstack_tlv_t * tlv_impl = NULL;
    void * tlv_context;
    btstack_tlv_get_instance(&tlv_impl, &tlv_context);
    if (!tlv_impl) return;

    // update ccc tag
    int index;
    uint32_t highest_seq_nr = 0;
    uint32_t lowest_seq_nr = 0;
    uint32_t tag_for_lowest_seq_nr = 0;
    uint32_t tag_for_empty = 0;
    persistent_ccc_entry_t entry;
    for (index=0; index<NVN_NUM_GATT_SERVER_CCC; index++){
        uint32_t tag = att_server_persistent_ccc_tag_for_index(index);
        int len = tlv_impl->get_tag(tlv_context, tag, (uint8_t *) &entry, sizeof(persistent_ccc_entry_t));

        // empty/invalid tag
        if (len != sizeof(persistent_ccc_entry_t)){
            tag_for_empty = tag;
            continue;
        }
        // update highest seq nr
        if (entry.seq_nr > highest_seq_nr){
            highest_seq_nr = entry.seq_nr;
        }
        // find entry with lowest seq nr
        if ((tag_for_lowest_seq_nr == 0u) || (entry.seq_nr < lowest_seq_nr)){
            tag_for_lowest_seq_nr = tag;
            lowest_seq_nr = entry.seq_nr;
        }

        if (entry.device_index != le_device_index) continue;
        if (entry.att_handle   != att_handle)      continue;

        // found matching entry
        if (value != 0){
            // update
            if (entry.value == value) {
                log_info("CCC Index %u: Up-to-date", index);
                return;
            }
            entry.value = (uint8_t) value;
            entry.seq_nr = highest_seq_nr + 1u;
            log_info("CCC Index %u: Store", index);
            int result = tlv_impl->store_tag(tlv_context, tag, (const uint8_t *) &entry, sizeof(persistent_ccc_entry_t));
            if (result != 0){
                log_error("Store tag index %u failed", index);
            }
        } else {
            // delete
            log_info("CCC Index %u: Delete", index);
            tlv_impl->delete_tag(tlv_context, tag);
        }
        return;
    }

    log_info("tag_for_empty %"PRIx32", tag_for_lowest_seq_nr %"PRIx32, tag_for_empty, tag_for_lowest_seq_nr);

    if (value == 0u){
        // done
        return;
    }

    uint32_t tag_to_use = 0u;
    if (tag_for_empty != 0u){
        tag_to_use = tag_for_empty;
    } else if (tag_for_lowest_seq_nr != 0){
        tag_to_use = tag_for_lowest_seq_nr;
    } else {
        // should not happen
        return;
    }
    // store ccc tag
    entry.seq_nr       = highest_seq_nr + 1u;
    entry.device_index = le_device_index;
    entry.att_handle   = att_handle;
    entry.value        = (uint8_t) value;
    int result = tlv_impl->store_tag(tlv_context, tag_to_use, (uint8_t *) &entry, sizeof(persistent_ccc_entry_t));
    if (result != 0){
        log_error("Store tag index %u failed", index);
    }
}

static void att_server_persistent_ccc_clear(att_server_t * att_server){
    int le_device_index = att_server->ir_le_device_db_index;
    log_info("Clear CCC values of remote %s, le device id %d", bd_addr_to_str(att_server->peer_address), le_device_index);
    // check if bonded
    if (le_device_index < 0) return;
    // get btstack_tlv
    const btstack_tlv_t * tlv_impl = NULL;
    void * tlv_context;
    btstack_tlv_get_instance(&tlv_impl, &tlv_context);
    if (!tlv_impl) return;
    // get all ccc tag
    int index;
    persistent_ccc_entry_t entry;
    for (index=0;index<NVN_NUM_GATT_SERVER_CCC;index++){
        uint32_t tag = att_server_persistent_ccc_tag_for_index(index);
        int len = tlv_impl->get_tag(tlv_context, tag, (uint8_t *) &entry, sizeof(persistent_ccc_entry_t));
        if (len != sizeof(persistent_ccc_entry_t)) continue;
        if (entry.device_index != le_device_index) continue;
        // delete entry
        log_info("CCC Index %u: Delete", index);
        tlv_impl->delete_tag(tlv_context, tag);
    }  
}

static void att_server_persistent_ccc_restore(att_server_t * att_server, att_connection_t * att_connection){
    int le_device_index = att_server->ir_le_device_db_index;
    log_info("Restore CCC values of remote %s, le device id %d", bd_addr_to_str(att_server->peer_address), le_device_index);
    // check if bonded
    if (le_device_index < 0) return;
    // get btstack_tlv
    const btstack_tlv_t * tlv_impl = NULL;
    void * tlv_context;
    btstack_tlv_get_instance(&tlv_impl, &tlv_context);
    if (!tlv_impl) return;
    // get all ccc tag
    int index;
    persistent_ccc_entry_t entry;
    for (index=0;index<NVN_NUM_GATT_SERVER_CCC;index++){
        uint32_t tag = att_server_persistent_ccc_tag_for_index(index);
        int len = tlv_impl->get_tag(tlv_context, tag, (uint8_t *) &entry, sizeof(persistent_ccc_entry_t));
        if (len != sizeof(persistent_ccc_entry_t)) continue;
        if (entry.device_index != le_device_index) continue;
        // simulate write callback
        uint16_t attribute_handle = entry.att_handle;
        uint8_t  value[2];
        little_endian_store_16(value, 0, entry.value);
        att_write_callback_t callback = att_server_write_callback_for_handle(attribute_handle);
        if (!callback) continue;
        log_info("CCC Index %u: Set Attribute handle 0x%04x to value 0x%04x", index, attribute_handle, entry.value );
        (*callback)(att_connection->con_handle, attribute_handle, ATT_TRANSACTION_MODE_NONE, 0, value, sizeof(value));
    }
}

// persistent CCC writes
// ---------------------

// gatt service management
static att_service_handler_t * att_service_handler_for_handle(uint16_t handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &service_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        att_service_handler_t * handler = (att_service_handler_t*) btstack_linked_list_iterator_next(&it);
        if (handler->start_handle > handle) continue;
        if (handler->end_handle   < handle) continue;
        return handler;
    }
    return NULL;
}

static att_write_callback_t att_server_write_callback_for_handle(uint16_t handle){
    att_service_handler_t * handler = att_service_handler_for_handle(handle);
    if (handler != NULL) return handler->write_callback;
    return att_server_client_write_callback;
}

static btstack_packet_handler_t att_server_packet_handler_for_handle(uint16_t handle){
    att_service_handler_t * handler = att_service_handler_for_handle(handle);
    if (handler != NULL) return handler->packet_handler;
    return att_client_packet_handler;
}

static void att_notify_write_callbacks(hci_con_handle_t con_handle, uint16_t transaction_mode){
    // notify all callbacks
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &service_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        att_service_handler_t * handler = (att_service_handler_t*) btstack_linked_list_iterator_next(&it);
        if (!handler->write_callback) continue;
        (*handler->write_callback)(con_handle, 0, transaction_mode, 0, NULL, 0);
    }
    if (!att_server_client_write_callback) return;
    (*att_server_client_write_callback)(con_handle, 0, transaction_mode, 0, NULL, 0);
}

// returns first reported error or 0
static uint8_t att_validate_prepared_write(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &service_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        att_service_handler_t * handler = (att_service_handler_t*) btstack_linked_list_iterator_next(&it);
        if (!handler->write_callback) continue;
        uint8_t error_code = (*handler->write_callback)(con_handle, 0, ATT_TRANSACTION_MODE_VALIDATE, 0, NULL, 0);
        if (error_code != 0u) return error_code;
    }
    if (!att_server_client_write_callback) return 0;
    return (*att_server_client_write_callback)(con_handle, 0, ATT_TRANSACTION_MODE_VALIDATE, 0, NULL, 0);
}

static uint16_t att_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    att_service_handler_t * service = att_service_handler_for_handle(attribute_handle);
    att_read_callback_t read_callback = (service != NULL) ? service->read_callback : att_server_client_read_callback;
    uint16_t result = 0;
    if (read_callback != NULL){
        result = (*read_callback)(con_handle, attribute_handle, offset, buffer, buffer_size);
#ifdef ENABLE_ATT_DELAYED_RESPONSE
        if (result == ATT_READ_RESPONSE_PENDING){
            if (service == NULL){
                att_server_flags |= ATT_SERVICE_FLAGS_DELAYED_RESPONSE;
            } else {
                service->flags   |= ATT_SERVICE_FLAGS_DELAYED_RESPONSE;
            }
        }
#endif
    }
    return result;
}

static int att_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    switch (transaction_mode){
        case ATT_TRANSACTION_MODE_VALIDATE:
            return att_validate_prepared_write(con_handle);
        case ATT_TRANSACTION_MODE_EXECUTE:
        case ATT_TRANSACTION_MODE_CANCEL:
            att_notify_write_callbacks(con_handle, transaction_mode);
            return 0;
        default:
            break;
    }

    // track CCC writes
    if (att_is_persistent_ccc(attribute_handle) && (offset == 0u) && (buffer_size == 2u)){
        att_server_persistent_ccc_write(con_handle, attribute_handle, little_endian_read_16(buffer, 0));
    }

    att_write_callback_t callback = att_server_write_callback_for_handle(attribute_handle);
    if (!callback) return 0;
    return (*callback)(con_handle, attribute_handle, transaction_mode, offset, buffer, buffer_size);
}

/**
 * @brief register read/write callbacks for specific handle range
 * @param att_service_handler_t
 */
void att_server_register_service_handler(att_service_handler_t * handler){
    bool att_server_registered = false;
    if (att_service_handler_for_handle(handler->start_handle) != NULL){
        att_server_registered = true;
    }

    if (att_service_handler_for_handle(handler->end_handle) != NULL){
        att_server_registered = true;
    }
    
    if (att_server_registered){
        log_error("handler for range 0x%04x-0x%04x already registered", handler->start_handle, handler->end_handle);
        return;
    }

    handler->flags = 0;
    btstack_linked_list_add(&service_handlers, (btstack_linked_item_t*) handler);
}

void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback){

    // store callbacks
    att_server_client_read_callback  = read_callback;
    att_server_client_write_callback = write_callback;

    // register for HCI Events
    hci_event_callback_registration.callback = &att_server_event_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &att_server_event_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // and L2CAP ATT Server PDUs
    att_dispatch_register_server(att_server_dispatch_packet_handler);

#ifdef ENABLE_GATT_OVER_CLASSIC
    // setup l2cap service
    att_dispatch_classic_register_service();
#endif

    att_set_db(db);
    att_set_read_callback(att_server_read_callback);
    att_set_write_callback(att_server_write_callback);
}

void att_server_register_packet_handler(btstack_packet_handler_t handler){
    att_client_packet_handler = handler;    
}


// to be deprecated
int  att_server_can_send_packet_now(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return 0;
    att_server_t * att_server = &hci_connection->att_server;
    att_connection_t * att_connection = &hci_connection->att_connection;
    return att_server_can_send_packet(att_server, att_connection);
}

uint8_t att_server_register_can_send_now_callback(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    return att_server_request_to_send_notification(callback_registration, con_handle);
}

void att_server_request_can_send_now_event(hci_con_handle_t con_handle){
    att_client_waiting_for_can_send_registration.callback = &att_emit_can_send_now_event;
    att_server_request_to_send_notification(&att_client_waiting_for_can_send_registration, con_handle);
}
// end of deprecated

uint8_t att_server_request_to_send_notification(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    att_server_t * att_server = &hci_connection->att_server;
    att_connection_t * att_connection = &hci_connection->att_connection;
    bool added = btstack_linked_list_add_tail(&att_server->notification_requests, (btstack_linked_item_t*) callback_registration);
    att_server_request_can_send_now(att_server, att_connection);
    if (added){
        return ERROR_CODE_SUCCESS;
    } else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

uint8_t att_server_request_to_send_indication(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    att_server_t * att_server = &hci_connection->att_server;
    att_connection_t * att_connection = &hci_connection->att_connection;
    bool added = btstack_linked_list_add_tail(&att_server->indication_requests, (btstack_linked_item_t*) callback_registration);
    att_server_request_can_send_now(att_server, att_connection);
    if (added){
        return ERROR_CODE_SUCCESS;
    } else {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

static uint8_t att_server_prepare_server_message(hci_con_handle_t con_handle, att_server_t ** out_att_server, att_connection_t ** out_att_connection, uint8_t ** out_packet_buffer){

    att_server_t *     att_server = NULL;
    att_connection_t * att_connection = NULL;
    uint8_t *          packet_buffer = NULL;

    // prefer enhanced bearer
#ifdef ENABLE_GATT_OVER_EATT
    att_server_eatt_bearer_t * eatt_bearer = att_server_eatt_bearer_for_con_handle(con_handle);
    if (eatt_bearer != NULL){
        att_server     = &eatt_bearer->att_server;
        att_connection = &eatt_bearer->att_connection;
        packet_buffer  = eatt_bearer->send_buffer;
    } else
#endif
    {
        hci_connection_t *hci_connection = hci_connection_for_handle(con_handle);
        if (hci_connection != NULL) {
            att_server     = &hci_connection->att_server;
            att_connection = &hci_connection->att_connection;
        }
    }

    if (att_server == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    if (!att_server_can_send_packet(att_server, att_connection)) return BTSTACK_ACL_BUFFERS_FULL;

    if (packet_buffer == NULL){
        l2cap_reserve_packet_buffer();
        packet_buffer = l2cap_get_outgoing_buffer();
    }

    *out_att_connection = att_connection;
    *out_att_server = att_server;
    *out_packet_buffer = packet_buffer;
    return ERROR_CODE_SUCCESS;
}

uint8_t att_server_notify(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t *value, uint16_t value_len){
    att_server_t * att_server = NULL;
    att_connection_t * att_connection = NULL;
    uint8_t * packet_buffer = NULL;

    uint8_t status = att_server_prepare_server_message(con_handle, &att_server, &att_connection, &packet_buffer);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    uint16_t size = att_prepare_handle_value_notification(att_connection, attribute_handle, value, value_len, packet_buffer);

    return att_server_send_prepared(att_server, att_connection, NULL, size);
}

/**
 * @brief notify client about multiple attribute value changes
 * @param con_handle
 * @param num_attributes
 * @param attribute_handles[]
 * @param values_data[]
 * @param values_len[]
 * @return 0 if ok, error otherwise
 */
uint8_t att_server_multiple_notify(hci_con_handle_t con_handle, uint8_t num_attributes,
                                   const uint16_t * attribute_handles, const uint8_t ** values_data, const uint16_t * values_len){

    att_server_t * att_server = NULL;
    att_connection_t * att_connection = NULL;
    uint8_t * packet_buffer = NULL;

    uint8_t status = att_server_prepare_server_message(con_handle, &att_server, &att_connection, &packet_buffer);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    uint16_t size = att_prepare_handle_value_multiple_notification(att_connection, num_attributes, attribute_handles, values_data, values_len, packet_buffer);

    return att_server_send_prepared(att_server, att_connection, packet_buffer, size);
}

uint8_t att_server_indicate(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t *value, uint16_t value_len){

    att_server_t * att_server = NULL;
    att_connection_t * att_connection = NULL;
    uint8_t * packet_buffer = NULL;

    uint8_t status = att_server_prepare_server_message(con_handle, &att_server, &att_connection, &packet_buffer);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    if (att_server->value_indication_handle != 0u) {
        // free reserved packet buffer
        if (att_server->bearer_type == ATT_BEARER_ENHANCED_LE){
            l2cap_release_packet_buffer();
        }
        return ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS;
    }

    // track indication
    att_server->value_indication_handle = attribute_handle;
    btstack_run_loop_set_timer_handler(&att_server->value_indication_timer, att_handle_value_indication_timeout);
    btstack_run_loop_set_timer(&att_server->value_indication_timer, ATT_TRANSACTION_TIMEOUT_MS);
    btstack_run_loop_add_timer(&att_server->value_indication_timer);

    uint16_t size = att_prepare_handle_value_indication(att_connection, attribute_handle, value, value_len, packet_buffer);

    return att_server_send_prepared(att_server, att_connection, NULL, size);
}

uint16_t att_server_get_mtu(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return 0;
    att_connection_t * att_connection = &hci_connection->att_connection;
    return att_connection->mtu;
}

void att_server_deinit(void){
    att_server_client_read_callback  = NULL;
    att_server_client_write_callback = NULL;
    att_client_packet_handler = NULL;
    service_handlers = NULL;
    att_server_flags = 0;
}

#ifdef ENABLE_GATT_OVER_EATT

#define MAX_NR_EATT_CHANNELS 5

static uint16_t att_server_eatt_receive_buffer_size;
static uint16_t att_server_eatt_send_buffer_size;

static att_server_eatt_bearer_t * att_server_eatt_bearer_for_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &att_server_eatt_bearer_active);
    while(btstack_linked_list_iterator_has_next(&it)){
        att_server_eatt_bearer_t * eatt_bearer = (att_server_eatt_bearer_t *) btstack_linked_list_iterator_next(&it);
        if (eatt_bearer->att_server.l2cap_cid == cid) {
            return eatt_bearer;
        }
    }
    return NULL;
}

static att_server_eatt_bearer_t * att_server_eatt_bearer_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &att_server_eatt_bearer_active);
    while(btstack_linked_list_iterator_has_next(&it)){
        att_server_eatt_bearer_t * eatt_bearer = (att_server_eatt_bearer_t *) btstack_linked_list_iterator_next(&it);
        if (eatt_bearer->att_connection.con_handle == con_handle) {
            return eatt_bearer;
        }
    }
    return NULL;
}

static void att_server_eatt_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint16_t cid;
    uint8_t status;
    uint16_t remote_mtu;

    uint8_t i;
    uint8_t num_requested_bearers;
    uint8_t num_accepted_bearers;
    uint16_t initial_credits = L2CAP_LE_AUTOMATIC_CREDITS;
    uint8_t * receive_buffers[MAX_NR_EATT_CHANNELS];
    uint16_t cids[MAX_NR_EATT_CHANNELS];
    att_server_eatt_bearer_t * eatt_bearers[MAX_NR_EATT_CHANNELS];
    att_server_eatt_bearer_t * eatt_bearer;
    att_server_t * att_server;
    att_connection_t * att_connection;
    hci_con_handle_t con_handle;
    hci_connection_t * hci_connection;

    switch (packet_type) {

        case L2CAP_DATA_PACKET:
            eatt_bearer = att_server_eatt_bearer_for_cid(channel);
            btstack_assert(eatt_bearer != NULL);
            att_server = &eatt_bearer->att_server;
            att_connection = &eatt_bearer->att_connection;
            att_server_handle_att_pdu(att_server, att_connection, packet, size);
            break;

        case HCI_EVENT_PACKET:
            switch (packet[0]) {

                case L2CAP_EVENT_CAN_SEND_NOW:
                    cid = l2cap_event_packet_sent_get_local_cid(packet);
                    eatt_bearer = att_server_eatt_bearer_for_cid(cid);
                    btstack_assert(eatt_bearer != NULL);
                    att_server = &eatt_bearer->att_server;
                    att_connection = &eatt_bearer->att_connection;
                    // only used for EATT request responses
                    btstack_assert(att_server->state == ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED);
                    att_server_process_validated_request(att_server, att_connection, eatt_bearer->send_buffer);
                    break;

                case L2CAP_EVENT_PACKET_SENT:
                    cid = l2cap_event_packet_sent_get_local_cid(packet);
                    eatt_bearer = att_server_eatt_bearer_for_cid(cid);
                    btstack_assert(eatt_bearer != NULL);
                    att_server = &eatt_bearer->att_server;
                    att_connection = &eatt_bearer->att_connection;
                    att_server_handle_att_pdu(att_server, att_connection, packet, size);
                    break;

                case L2CAP_EVENT_ECBM_INCOMING_CONNECTION:
                    cid = l2cap_event_ecbm_incoming_connection_get_local_cid(packet);

                    // reject if outgoing l2cap connection active, L2CAP/TIM/BV-01-C
                    con_handle = l2cap_event_ecbm_incoming_connection_get_handle(packet);
                    hci_connection = hci_connection_for_handle(con_handle);
                    btstack_assert(hci_connection != NULL);
                    if (hci_connection->att_server.eatt_outgoing_active) {
                        hci_connection->att_server.incoming_connection_request = true;
                        l2cap_ecbm_decline_channels(cid, L2CAP_ECBM_CONNECTION_RESULT_SOME_REFUSED_INSUFFICIENT_RESOURCES_AVAILABLE );
                        log_info("Decline incoming connection from %s", bd_addr_to_str(hci_connection->address));
                    } else {
                        num_requested_bearers = l2cap_event_ecbm_incoming_connection_get_num_channels(packet);
                        for (i = 0; i < num_requested_bearers; i++){
                            eatt_bearers[i] = (att_server_eatt_bearer_t *) btstack_linked_list_pop(&att_server_eatt_bearer_pool);
                            if (eatt_bearers[i] == NULL) {
                                break;
                            }
                            eatt_bearers[i]->att_connection.con_handle = l2cap_event_ecbm_incoming_connection_get_handle(packet);
                            eatt_bearers[i]->att_server.bearer_type = ATT_BEARER_ENHANCED_LE;
                            receive_buffers[i] = eatt_bearers[i]->receive_buffer;
                            btstack_linked_list_add(&att_server_eatt_bearer_active, (btstack_linked_item_t *) eatt_bearers[i]);
                        }
                        num_accepted_bearers = i;
                        status = l2cap_ecbm_accept_channels(cid, num_accepted_bearers, initial_credits, att_server_eatt_receive_buffer_size, receive_buffers, cids);
                        btstack_assert(status == ERROR_CODE_SUCCESS);
                        log_info("requested %u, accepted %u", num_requested_bearers, num_accepted_bearers);
                        for (i=0;i<num_accepted_bearers;i++){
                            log_info("eatt l2cap cid: 0x%04x", cids[i]);
                            eatt_bearers[i]->att_server.l2cap_cid = cids[i];
                        }
                    }
                    break;

                case L2CAP_EVENT_ECBM_CHANNEL_OPENED:
                    cid         = l2cap_event_ecbm_channel_opened_get_local_cid(packet);
                    status      = l2cap_event_ecbm_channel_opened_get_status(packet);
                    remote_mtu  = l2cap_event_ecbm_channel_opened_get_remote_mtu(packet);
                    eatt_bearer = att_server_eatt_bearer_for_cid(cid);
                    btstack_assert(eatt_bearer != NULL);
                    eatt_bearer->att_connection.mtu_exchanged = true;
                    eatt_bearer->att_connection.mtu = remote_mtu;
                    eatt_bearer->att_connection.max_mtu = remote_mtu;
                    log_info("L2CAP_EVENT_ECBM_CHANNEL_OPENED - cid 0x%04x mtu %u, status 0x%02x", cid, remote_mtu, status);
                    break;

                case L2CAP_EVENT_ECBM_RECONFIGURED:
                    break;

                case L2CAP_EVENT_CHANNEL_CLOSED:
                    eatt_bearer = att_server_eatt_bearer_for_cid(l2cap_event_channel_closed_get_local_cid(packet));
                    btstack_assert(eatt_bearers != NULL);

                    // TODO: finalize - abort queued writes

                    btstack_linked_list_remove(&att_server_eatt_bearer_active, (btstack_linked_item_t  *) eatt_bearer);
                    btstack_linked_list_add(&att_server_eatt_bearer_pool, (btstack_linked_item_t  *) eatt_bearer);
                    break;
                default:
                    break;
            }
            break;
        default:
            btstack_unreachable();
            break;
    }
}

// create eatt bearers
uint8_t att_server_eatt_init(uint8_t num_eatt_bearers, uint8_t * storage_buffer, uint16_t storage_size){
    uint16_t size_for_structs = num_eatt_bearers * sizeof(att_server_eatt_bearer_t);
    if (storage_size < size_for_structs) {
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    // TODO: The minimum ATT_MTU for an Enhanced ATT bearer is 64 octets.

    memset(storage_buffer, 0, storage_size);
    uint16_t buffer_size_per_bearer = ((storage_size - size_for_structs) / num_eatt_bearers);
    att_server_eatt_receive_buffer_size = buffer_size_per_bearer / 2;
    att_server_eatt_send_buffer_size    = buffer_size_per_bearer / 2;
    uint8_t * bearer_buffer = &storage_buffer[size_for_structs];
    uint8_t i;
    att_server_eatt_bearer_t * eatt_bearer = (att_server_eatt_bearer_t *) storage_buffer;
    log_info("%u EATT bearers with receive buffer size %u",
             num_eatt_bearers, att_server_eatt_receive_buffer_size);
    for (i=0;i<num_eatt_bearers;i++){
        eatt_bearer->att_connection.con_handle = HCI_CON_HANDLE_INVALID;
        eatt_bearer->receive_buffer = bearer_buffer;
        bearer_buffer += att_server_eatt_receive_buffer_size;
        eatt_bearer->send_buffer = bearer_buffer;
        bearer_buffer += att_server_eatt_send_buffer_size;
        btstack_linked_list_add(&att_server_eatt_bearer_pool, (btstack_linked_item_t *) eatt_bearer);
        eatt_bearer++;
    }
    // TODO: define minimum EATT MTU
    l2cap_ecbm_register_service(att_server_eatt_handler, BLUETOOTH_PSM_EATT, 64, 0, false);

    return 0;
}
#endif
