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

#define __BTSTACK_FILE__ "att_server.c"


//
// ATT Server Globals
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_config.h"

#include "att_dispatch.h"
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

static void att_run_for_context(att_server_t * att_server);

// global
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static btstack_packet_handler_t               att_client_packet_handler = NULL;
static btstack_linked_list_t                  can_send_now_clients;
static uint8_t                                att_client_waiting_for_can_send;

static att_server_t * att_server_for_handle(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (!hci_connection) return NULL;
    return &hci_connection->att_server;
}

#ifdef ENABLE_LE_SIGNED_WRITE
static att_server_t * att_server_for_state(att_server_state_t state){
    btstack_linked_list_iterator_t it;
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        att_server_t * att_server = &connection->att_server;
        if (att_server->state == state) return att_server;
    }
    return NULL;
}
#endif

static void att_handle_value_indication_notify_client(uint8_t status, uint16_t client_handle, uint16_t attribute_handle){
    if (!att_client_packet_handler) return;
    
    uint8_t event[7];
    int pos = 0;
    event[pos++] = ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    little_endian_store_16(event, pos, client_handle);
    pos += 2;
    little_endian_store_16(event, pos, attribute_handle);
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_emit_mtu_event(hci_con_handle_t con_handle, uint16_t mtu){
    if (!att_client_packet_handler) return;

    uint8_t event[6];
    int pos = 0;
    event[pos++] = ATT_EVENT_MTU_EXCHANGE_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, mtu);
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_emit_can_send_now_event(void){
    if (!att_client_packet_handler) return;

    uint8_t event[] = { ATT_EVENT_CAN_SEND_NOW, 0};
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_handle_value_indication_timeout(btstack_timer_source_t *ts){
    void * context = btstack_run_loop_get_timer_context(ts);
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) context;
    att_server_t * att_server = att_server_for_handle(con_handle);
    if (!att_server) return;
    uint16_t att_handle = att_server->value_indication_handle;
    att_handle_value_indication_notify_client(ATT_HANDLE_VALUE_INDICATION_TIMEOUT, att_server->connection.con_handle, att_handle);
}

static void att_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    UNUSED(channel);
    UNUSED(size);
    
    att_server_t * att_server;
    hci_con_handle_t con_handle;

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            con_handle = little_endian_read_16(packet, 4);
                            att_server = att_server_for_handle(con_handle);
                            if (!att_server) break;
                        	// store connection info 
                        	att_server->peer_addr_type = packet[7];
                            reverse_bd_addr(&packet[8], att_server->peer_address);
                            att_server->connection.con_handle = con_handle;
                            // reset connection properties
                            att_server->state = ATT_SERVER_IDLE;
                            att_server->connection.mtu = ATT_DEFAULT_MTU;
                            att_server->connection.max_mtu = l2cap_max_le_mtu();
                            if (att_server->connection.max_mtu > ATT_REQUEST_BUFFER_SIZE){
                                att_server->connection.max_mtu = ATT_REQUEST_BUFFER_SIZE;
                            }
                            att_server->connection.encryption_key_size = 0;
                            att_server->connection.authenticated = 0;
		                	att_server->connection.authorized = 0;
                            att_server->ir_le_device_db_index = -1;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                case HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE: 
                	// check handle
                    con_handle = little_endian_read_16(packet, 3);
                    att_server = att_server_for_handle(con_handle);
                    if (!att_server) break;
                	att_server->connection.encryption_key_size = sm_encryption_key_size(con_handle);
                	att_server->connection.authenticated = sm_authenticated(con_handle);
                	break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // check handle
                    con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
                    att_server = att_server_for_handle(con_handle);
                    if (!att_server) break;
                    att_clear_transaction_queue(&att_server->connection);
                    att_server->connection.con_handle = 0;
                    att_server->value_indication_handle = 0; // reset error state
                    att_server->state = ATT_SERVER_IDLE;
                    break;
                    
                case SM_EVENT_IDENTITY_RESOLVING_STARTED:
                    con_handle = sm_event_identity_resolving_started_get_handle(packet);
                    att_server = att_server_for_handle(con_handle);
                    if (!att_server) break;
                    log_info("SM_EVENT_IDENTITY_RESOLVING_STARTED");
                    att_server->ir_lookup_active = 1;
                    break;
                case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
                    con_handle = sm_event_identity_resolving_succeeded_get_handle(packet);
                    att_server = att_server_for_handle(con_handle);
                    if (!att_server) break;
                    att_server->ir_lookup_active = 0;
                    att_server->ir_le_device_db_index = sm_event_identity_resolving_succeeded_get_index_internal(packet);
                    log_info("SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED id %u", att_server->ir_le_device_db_index);
                    att_run_for_context(att_server);
                    break;
                case SM_EVENT_IDENTITY_RESOLVING_FAILED:
                    con_handle = sm_event_identity_resolving_failed_get_handle(packet);
                    att_server = att_server_for_handle(con_handle);
                    if (!att_server) break;
                    log_info("SM_EVENT_IDENTITY_RESOLVING_FAILED");
                    att_server->ir_lookup_active = 0;
                    att_server->ir_le_device_db_index = -1;
                    att_run_for_context(att_server);
                    break;
                case SM_EVENT_AUTHORIZATION_RESULT: {
                    con_handle = sm_event_authorization_result_get_handle(packet);
                    att_server = att_server_for_handle(con_handle);
                    if (!att_server) break;
                    att_server->connection.authorized = sm_event_authorization_result_get_authorization_result(packet);
                    att_dispatch_server_request_can_send_now_event(con_handle);
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

#ifdef ENABLE_LE_SIGNED_WRITE
static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    
    att_server_t * att_server = att_server_for_state(ATT_SERVER_W4_SIGNED_WRITE_VALIDATION);
    if (!att_server) return;

    uint8_t hash_flipped[8];
    reverse_64(hash, hash_flipped);
    if (memcmp(hash_flipped, &att_server->request_buffer[att_server->request_size-8], 8)){
        log_info("ATT Signed Write, invalid signature");
        att_server->state = ATT_SERVER_IDLE;
        return;
    }
    log_info("ATT Signed Write, valid signature");

    // update sequence number
    uint32_t counter_packet = little_endian_read_32(att_server->request_buffer, att_server->request_size-12);
    le_device_db_remote_counter_set(att_server->ir_le_device_db_index, counter_packet+1);
    att_server->state = ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
    att_dispatch_server_request_can_send_now_event(att_server->connection.con_handle);
}
#endif

// pre: att_server->state == ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED
// pre: can send now
// returns: 1 if packet was sent
static int att_server_process_validated_request(att_server_t * att_server){

    l2cap_reserve_packet_buffer();
    uint8_t * att_response_buffer = l2cap_get_outgoing_buffer();
    uint16_t  att_response_size   = att_handle_request(&att_server->connection, att_server->request_buffer, att_server->request_size, att_response_buffer);

    // intercept "insufficient authorization" for authenticated connections to allow for user authorization
    if ((att_response_size     >= 4)
    && (att_response_buffer[0] == ATT_ERROR_RESPONSE)
    && (att_response_buffer[4] == ATT_ERROR_INSUFFICIENT_AUTHORIZATION)
    && (att_server->connection.authenticated)){

        switch (sm_authorization_state(att_server->connection.con_handle)){
            case AUTHORIZATION_UNKNOWN:
                l2cap_release_packet_buffer();
                sm_request_pairing(att_server->connection.con_handle);
                return 0;
            case AUTHORIZATION_PENDING:
                l2cap_release_packet_buffer();
                return 0;
            default:
                break;
        }
    }

    att_server->state = ATT_SERVER_IDLE;
    if (att_response_size == 0) {
        l2cap_release_packet_buffer();
        return 0;
    }

    l2cap_send_prepared_connectionless(att_server->connection.con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_size);

    // notify client about MTU exchange result
    if (att_response_buffer[0] == ATT_EXCHANGE_MTU_RESPONSE){
        att_emit_mtu_event(att_server->connection.con_handle, att_server->connection.mtu);
    }
    return 1;
}

static void att_run_for_context(att_server_t * att_server){
    switch (att_server->state){
        case ATT_SERVER_REQUEST_RECEIVED:
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
            att_dispatch_server_request_can_send_now_event(att_server->connection.con_handle);
            break;

        default:
            break;
    }   
}

static void att_server_handle_can_send_now(void){

    // NOTE: we get l2cap fixed channel instead of con_handle 

    btstack_linked_list_iterator_t it;
    hci_connections_get_iterator(&it);
    while(btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        att_server_t * att_server = &connection->att_server;
        if (att_server->state == ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED){
            int sent = att_server_process_validated_request(att_server);
            if (sent && (att_client_waiting_for_can_send || !btstack_linked_list_empty(&can_send_now_clients))){
                att_dispatch_server_request_can_send_now_event(att_server->connection.con_handle);
                return;
            }
        }
    }

    while (!btstack_linked_list_empty(&can_send_now_clients)){
        // handle first client
        btstack_context_callback_registration_t * client = (btstack_context_callback_registration_t*) can_send_now_clients;
        hci_con_handle_t con_handle = (uintptr_t) client->context;
        btstack_linked_list_remove(&can_send_now_clients, (btstack_linked_item_t *) client);
        client->callback(client->context);

        // request again if needed
        if (!att_dispatch_server_can_send_now(con_handle)){
            if (!btstack_linked_list_empty(&can_send_now_clients) || att_client_waiting_for_can_send){
                att_dispatch_server_request_can_send_now_event(con_handle);
            }
            return;
        }
    }

    if (att_client_waiting_for_can_send){
        att_client_waiting_for_can_send = 0;
        att_emit_can_send_now_event();
    }
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    att_server_t * att_server;

    switch (packet_type){

        case HCI_EVENT_PACKET:
            if (packet[0] != L2CAP_EVENT_CAN_SEND_NOW) break;
            att_server_handle_can_send_now();
            break;

        case ATT_DATA_PACKET:
            log_info("ATT Packet, handle 0x%04x", handle);
            att_server = att_server_for_handle(handle);
            if (!att_server) break;

            // handle value indication confirms
            if (packet[0] == ATT_HANDLE_VALUE_CONFIRMATION && att_server->value_indication_handle){
                btstack_run_loop_remove_timer(&att_server->value_indication_timer);
                uint16_t att_handle = att_server->value_indication_handle;
                att_server->value_indication_handle = 0;    
                att_handle_value_indication_notify_client(0, att_server->connection.con_handle, att_handle);
                return;
            }

            // directly process command
            // note: signed write cannot be handled directly as authentication needs to be verified
            if (packet[0] == ATT_WRITE_COMMAND){
                att_handle_request(&att_server->connection, packet, size, 0);
                return;
            }

            // check size
            if (size > sizeof(att_server->request_buffer)) {
                log_info("att_packet_handler: dropping att pdu 0x%02x as size %u > att_server->request_buffer %u", packet[0], size, (int) sizeof(att_server->request_buffer));
                return;
            }

            // last request still in processing?
            if (att_server->state != ATT_SERVER_IDLE){
                log_info("att_packet_handler: skipping att pdu 0x%02x as server not idle (state %u)", packet[0], att_server->state);
                return;
            }

            // store request
            att_server->state = ATT_SERVER_REQUEST_RECEIVED;
            att_server->request_size = size;
            memcpy(att_server->request_buffer, packet, size);
        
            att_run_for_context(att_server);
            break;
    }
}

void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback){

    // register for HCI Events
    hci_event_callback_registration.callback = &att_event_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &att_event_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // and L2CAP ATT Server PDUs
    att_dispatch_register_server(att_packet_handler);

    att_set_db(db);
    att_set_read_callback(read_callback);
    att_set_write_callback(write_callback);

}

void att_server_register_packet_handler(btstack_packet_handler_t handler){
    att_client_packet_handler = handler;    
}

int  att_server_can_send_packet_now(hci_con_handle_t con_handle){
	return att_dispatch_server_can_send_now(con_handle);
}

void att_server_request_can_send_now_event(hci_con_handle_t con_handle){
    log_debug("att_server_request_can_send_now_event 0x%04x", con_handle);
    att_client_waiting_for_can_send = 1;
    att_dispatch_server_request_can_send_now_event(con_handle);
}

void att_server_register_can_send_now_callback(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle){
    // check if can send already
    if (att_dispatch_server_can_send_now(con_handle)){
        callback_registration->callback(callback_registration->context);
        return;
    }
    callback_registration->context = (void*)(uintptr_t) con_handle;
    btstack_linked_list_add_tail(&can_send_now_clients, (btstack_linked_item_t*) callback_registration);
    att_dispatch_server_request_can_send_now_event(con_handle);
}


int att_server_notify(hci_con_handle_t con_handle, uint16_t attribute_handle, uint8_t *value, uint16_t value_len){
    att_server_t * att_server = att_server_for_handle(con_handle);
    if (!att_server) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;

    if (!att_dispatch_server_can_send_now(con_handle)) return BTSTACK_ACL_BUFFERS_FULL;

    l2cap_reserve_packet_buffer();
    uint8_t * packet_buffer = l2cap_get_outgoing_buffer();
    uint16_t size = att_prepare_handle_value_notification(&att_server->connection, attribute_handle, value, value_len, packet_buffer);
	return l2cap_send_prepared_connectionless(att_server->connection.con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, size);
}

int att_server_indicate(hci_con_handle_t con_handle, uint16_t attribute_handle, uint8_t *value, uint16_t value_len){
    att_server_t * att_server = att_server_for_handle(con_handle);
    if (!att_server) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;

    if (att_server->value_indication_handle) return ATT_HANDLE_VALUE_INDICATION_IN_PROGRESS;
    if (!att_dispatch_server_can_send_now(con_handle)) return BTSTACK_ACL_BUFFERS_FULL;

    // track indication
    att_server->value_indication_handle = attribute_handle;
    btstack_run_loop_set_timer_handler(&att_server->value_indication_timer, att_handle_value_indication_timeout);
    btstack_run_loop_set_timer(&att_server->value_indication_timer, ATT_TRANSACTION_TIMEOUT_MS);
    btstack_run_loop_add_timer(&att_server->value_indication_timer);

    l2cap_reserve_packet_buffer();
    uint8_t * packet_buffer = l2cap_get_outgoing_buffer();
    uint16_t size = att_prepare_handle_value_indication(&att_server->connection, attribute_handle, value, value_len, packet_buffer);
	l2cap_send_prepared_connectionless(att_server->connection.con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, size);
    return 0;
}
