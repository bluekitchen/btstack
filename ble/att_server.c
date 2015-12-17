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


//
// ATT Server Globals
//

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_dispatch.h"
#include "gap_le.h"
#include "le_device_db.h"

#include "att_server.h"

static void att_run(void);

// max ATT request matches L2CAP PDU -- allow to use smaller buffer
#ifndef ATT_REQUEST_BUFFER_SIZE
#define ATT_REQUEST_BUFFER_SIZE HCI_ACL_PAYLOAD_SIZE
#endif

typedef enum {
    ATT_SERVER_IDLE,
    ATT_SERVER_REQUEST_RECEIVED,
    ATT_SERVER_W4_SIGNED_WRITE_VALIDATION,
    ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED,
} att_server_state_t;

static att_connection_t att_connection;
static att_server_state_t att_server_state;

static uint8_t   att_client_addr_type;
static bd_addr_t att_client_address;
static uint16_t  att_request_size   = 0;
static uint8_t   att_request_buffer[ATT_REQUEST_BUFFER_SIZE];

static int       att_ir_le_device_db_index = -1;
static int       att_ir_lookup_active = 0;

static int       att_handle_value_indication_handle = 0;    
static timer_source_t att_handle_value_indication_timer;

static btstack_packet_handler_t att_client_packet_handler = NULL;

static void att_handle_value_indication_notify_client(uint8_t status, uint16_t client_handle, uint16_t attribute_handle){
    
    if (!att_client_packet_handler) return;
    
    uint8_t event[7];
    int pos = 0;
    event[pos++] = ATT_HANDLE_VALUE_INDICATION_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    bt_store_16(event, pos, client_handle);
    pos += 2;
    bt_store_16(event, pos, attribute_handle);
    pos += 2;
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_emit_mtu_event(uint16_t handle, uint16_t mtu){

    if (!att_client_packet_handler) return;

    uint8_t event[6];
    int pos = 0;
    event[pos++] = ATT_MTU_EXCHANGE_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    bt_store_16(event, pos, handle);
    pos += 2;
    bt_store_16(event, pos, mtu);
    pos += 2;
    (*att_client_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

static void att_handle_value_indication_timeout(timer_source_t *ts){
    uint16_t att_handle = att_handle_value_indication_handle;
    att_handle_value_indication_notify_client(ATT_HANDLE_VALUE_INDICATION_TIMEOUT, att_connection.con_handle, att_handle);
}

static void att_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    att_run();
                    break;
                    
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                        	// store connection info 
                        	att_client_addr_type = packet[7];
                            bt_flip_addr(att_client_address, &packet[8]);
                            // reset connection properties
                            att_connection.con_handle = READ_BT_16(packet, 4);
                            att_connection.mtu = ATT_DEFAULT_MTU;
                            att_connection.max_mtu = l2cap_max_le_mtu();
                            if (att_connection.max_mtu > ATT_REQUEST_BUFFER_SIZE){
                                att_connection.max_mtu = ATT_REQUEST_BUFFER_SIZE;
                            }
                            att_connection.encryption_key_size = 0;
                            att_connection.authenticated = 0;
		                	att_connection.authorized = 0;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                case HCI_EVENT_ENCRYPTION_KEY_REFRESH_COMPLETE: 
                	// check handle
                	if (att_connection.con_handle != READ_BT_16(packet, 3)) break;
                	att_connection.encryption_key_size = sm_encryption_key_size(att_client_addr_type, att_client_address);
                	att_connection.authenticated = sm_authenticated(att_client_addr_type, att_client_address);
                	break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    att_clear_transaction_queue(&att_connection);
                    att_connection.con_handle = 0;
                    att_handle_value_indication_handle = 0; // reset error state
                    // restart advertising if we have been connected before
                    // -> avoid sending advertise enable a second time before command complete was received 
                    att_server_state = ATT_SERVER_IDLE;
                    break;
                    
                case SM_IDENTITY_RESOLVING_STARTED:
                    log_info("SM_IDENTITY_RESOLVING_STARTED");
                    att_ir_lookup_active = 1;
                    break;
                case SM_IDENTITY_RESOLVING_SUCCEEDED:
                    att_ir_lookup_active = 0;
                    att_ir_le_device_db_index = ((sm_event_t*) packet)->le_device_db_index;
                    log_info("SM_IDENTITY_RESOLVING_SUCCEEDED id %u", att_ir_le_device_db_index);
                    att_run();
                    break;
                case SM_IDENTITY_RESOLVING_FAILED:
                    log_info("SM_IDENTITY_RESOLVING_FAILED");
                    att_ir_lookup_active = 0;
                    att_ir_le_device_db_index = -1;
                    att_run();
                    break;

                case SM_AUTHORIZATION_RESULT: {
                    sm_event_t * event = (sm_event_t *) packet;
                    if (event->addr_type != att_client_addr_type) break;
                    if (memcmp(event->address, att_client_address, 6) != 0) break;
                    att_connection.authorized = event->authorization_result;
                    att_run();
                	break;
                }

                default:
                    break;
            }
    }
    if (att_client_packet_handler){
        att_client_packet_handler(packet_type, channel, packet, size);
    }
}

static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    
    if (att_server_state != ATT_SERVER_W4_SIGNED_WRITE_VALIDATION) return;

    uint8_t hash_flipped[8];
    swap64(hash, hash_flipped);
    if (memcmp(hash_flipped, &att_request_buffer[att_request_size-8], 8)){
        log_info("ATT Signed Write, invalid signature");
        att_server_state = ATT_SERVER_IDLE;
        return;
    }
    log_info("ATT Signed Write, valid signature");

    // update sequence number
    uint32_t counter_packet = READ_BT_32(att_request_buffer, att_request_size-12);
    le_device_db_remote_counter_set(att_ir_le_device_db_index, counter_packet+1);
    att_server_state = ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED;
    att_run();
}

static void att_run(void){
    switch (att_server_state){
        case ATT_SERVER_IDLE:
        case ATT_SERVER_W4_SIGNED_WRITE_VALIDATION:
            return;
        case ATT_SERVER_REQUEST_RECEIVED:
            if (att_request_buffer[0] == ATT_SIGNED_WRITE_COMMAND){
                log_info("ATT Signed Write!");
                if (!sm_cmac_ready()) {
                    log_info("ATT Signed Write, sm_cmac engine not ready. Abort");
                    att_server_state = ATT_SERVER_IDLE;
                     return;
                }  
                if (att_request_size < (3 + 12)) {
                    log_info("ATT Signed Write, request to short. Abort.");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }
                if (att_ir_lookup_active){
                    return;
                }
                if (att_ir_le_device_db_index < 0){
                    log_info("ATT Signed Write, CSRK not available");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }

                // check counter
                uint32_t counter_packet = READ_BT_32(att_request_buffer, att_request_size-12);
                uint32_t counter_db     = le_device_db_remote_counter_get(att_ir_le_device_db_index);
                log_info("ATT Signed Write, DB counter %"PRIu32", packet counter %"PRIu32, counter_db, counter_packet);
                if (counter_packet < counter_db){
                    log_info("ATT Signed Write, db reports higher counter, abort");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }

                // signature is { sequence counter, secure hash }
                sm_key_t csrk;
                le_device_db_remote_csrk_get(att_ir_le_device_db_index, csrk);
                att_server_state = ATT_SERVER_W4_SIGNED_WRITE_VALIDATION;
                log_info("Orig Signature: ");
                hexdump( &att_request_buffer[att_request_size-8], 8);
                uint16_t attribute_handle = READ_BT_16(att_request_buffer, 1);
                sm_cmac_start(csrk, att_request_buffer[0], attribute_handle, att_request_size - 15, &att_request_buffer[3], counter_packet, att_signed_write_handle_cmac_result);
                return;
            } 
            // NOTE: fall through for regular commands

        case ATT_SERVER_REQUEST_RECEIVED_AND_VALIDATED:
            if (!l2cap_can_send_fixed_channel_packet_now(att_connection.con_handle)) return;

            l2cap_reserve_packet_buffer();
            uint8_t * att_response_buffer = l2cap_get_outgoing_buffer();
            uint16_t  att_response_size   = att_handle_request(&att_connection, att_request_buffer, att_request_size, att_response_buffer);

            // intercept "insufficient authorization" for authenticated connections to allow for user authorization
            if ((att_response_size     >= 4)
            && (att_response_buffer[0] == ATT_ERROR_RESPONSE)
            && (att_response_buffer[4] == ATT_ERROR_INSUFFICIENT_AUTHORIZATION)
            && (att_connection.authenticated)){

            	switch (sm_authorization_state(att_client_addr_type, att_client_address)){
            		case AUTHORIZATION_UNKNOWN:
                        l2cap_release_packet_buffer();
		             	sm_request_pairing(att_client_addr_type, att_client_address);
	    		        return;
	    		    case AUTHORIZATION_PENDING:
                        l2cap_release_packet_buffer();
	    		    	return;
	    		    default:
	    		    	break;
            	}
            }

            att_server_state = ATT_SERVER_IDLE;
            if (att_response_size == 0) {
                l2cap_release_packet_buffer();
                return;
            }

            l2cap_send_prepared_connectionless(att_connection.con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_size);

            // notify client about MTU exchange result
            if (att_response_buffer[0] == ATT_EXCHANGE_MTU_RESPONSE){
                att_emit_mtu_event(att_connection.con_handle, att_connection.mtu);
            }

            break;
    }
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;

    // handle value indication confirms
    if (packet[0] == ATT_HANDLE_VALUE_CONFIRMATION && att_handle_value_indication_handle){
        run_loop_remove_timer(&att_handle_value_indication_timer);
        uint16_t att_handle = att_handle_value_indication_handle;
        att_handle_value_indication_handle = 0;    
        att_handle_value_indication_notify_client(0, att_connection.con_handle, att_handle);
        return;
    }

    // check size
    if (size > sizeof(att_request_buffer)) return;

    // last request still in processing?
    if (att_server_state != ATT_SERVER_IDLE) return;

    // store request
    att_server_state = ATT_SERVER_REQUEST_RECEIVED;
    att_request_size = size;
    memcpy(att_request_buffer, packet, size);

    att_run();
}

void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback){

    sm_register_packet_handler(att_event_packet_handler);

    att_dispatch_register_server(att_packet_handler);

    att_server_state = ATT_SERVER_IDLE;
    att_set_db(db);
    att_set_read_callback(read_callback);
    att_set_write_callback(write_callback);

}

void att_server_register_packet_handler(btstack_packet_handler_t handler){
    att_client_packet_handler = handler;    
}

int  att_server_can_send(void){
	if (att_connection.con_handle == 0) return 0;
	return l2cap_can_send_fixed_channel_packet_now(att_connection.con_handle);
}

int att_server_notify(uint16_t handle, uint8_t *value, uint16_t value_len){
    if (!l2cap_can_send_fixed_channel_packet_now(att_connection.con_handle)) return BTSTACK_ACL_BUFFERS_FULL;

    l2cap_reserve_packet_buffer();
    uint8_t * packet_buffer = l2cap_get_outgoing_buffer();
    uint16_t size = att_prepare_handle_value_notification(&att_connection, handle, value, value_len, packet_buffer);
	return l2cap_send_prepared_connectionless(att_connection.con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, size);
}

int att_server_indicate(uint16_t handle, uint8_t *value, uint16_t value_len){
    if (att_handle_value_indication_handle) return ATT_HANDLE_VALUE_INDICATION_IN_PORGRESS;
    if (!l2cap_can_send_fixed_channel_packet_now(att_connection.con_handle)) return BTSTACK_ACL_BUFFERS_FULL;

    // track indication
    att_handle_value_indication_handle = handle;
    run_loop_set_timer_handler(&att_handle_value_indication_timer, att_handle_value_indication_timeout);
    run_loop_set_timer(&att_handle_value_indication_timer, ATT_TRANSACTION_TIMEOUT_MS);
    run_loop_add_timer(&att_handle_value_indication_timer);

    l2cap_reserve_packet_buffer();
    uint8_t * packet_buffer = l2cap_get_outgoing_buffer();
    uint16_t size = att_prepare_handle_value_indication(&att_connection, handle, value, value_len, packet_buffer);
	l2cap_send_prepared_connectionless(att_connection.con_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, size);
    return 0;
}
