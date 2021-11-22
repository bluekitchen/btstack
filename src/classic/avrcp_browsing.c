/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "avrcp_browsing.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"
#include "classic/avrcp_browsing.h"

typedef struct {
    uint16_t browsing_cid;
    uint16_t browsing_l2cap_psm;
    uint16_t browsing_version;
} avrcp_browsing_sdp_query_context_t; 

static void avrcp_browsing_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

// higher layer callbacks
static btstack_packet_handler_t           avrcp_browsing_callback;
static btstack_packet_handler_t avrcp_browsing_controller_packet_handler;
static btstack_packet_handler_t avrcp_browsing_target_packet_handler;

// sdp query
static bd_addr_t avrcp_browsing_sdp_addr;
static btstack_context_callback_registration_t avrcp_browsing_handle_sdp_client_query_request;
static avrcp_browsing_sdp_query_context_t avrcp_browsing_sdp_query_context;

static bool avrcp_browsing_l2cap_service_registered;


void avrcp_browsing_request_can_send_now(avrcp_browsing_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send = true;
    l2cap_request_can_send_now_event(l2cap_cid);
}

static void avrcp_retry_timer_timeout_handler(btstack_timer_source_t * timer){
    uint16_t avrcp_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
    avrcp_connection_t * connection_controller = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, avrcp_cid);
    if (connection_controller == NULL) return;
    avrcp_connection_t * connection_target = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, avrcp_cid);
    if (connection_target == NULL) return;

    if ((connection_controller->browsing_connection == NULL) || (connection_target->browsing_connection == NULL)) return;

    if (connection_controller->browsing_connection->state == AVCTP_CONNECTION_W2_L2CAP_RETRY){
        connection_controller->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
        connection_target->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;

        l2cap_ertm_create_channel(avrcp_browsing_packet_handler, connection_controller->remote_addr, connection_controller->browsing_l2cap_psm,
                &connection_controller->browsing_connection->ertm_config, 
                connection_controller->browsing_connection->ertm_buffer, 
                connection_controller->browsing_connection->ertm_buffer_size, NULL);
    }
}

static void avrcp_retry_timer_start(avrcp_connection_t * connection){
    btstack_run_loop_set_timer_handler(&connection->retry_timer, avrcp_retry_timer_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->retry_timer, (void *)(uintptr_t)connection->avrcp_cid);

    // add some jitter/randomness to reconnect delay
    uint32_t timeout = 100 + (btstack_run_loop_get_time_ms() & 0x7F);
    btstack_run_loop_set_timer(&connection->retry_timer, timeout); 
    
    btstack_run_loop_add_timer(&connection->retry_timer);
}

// AVRCP Browsing Service functions
static void avrcp_browsing_finalize_connection(avrcp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->retry_timer);
    btstack_memory_avrcp_browsing_connection_free(connection->browsing_connection);
    connection->browsing_connection = NULL;
}

static void avrcp_browsing_emit_connection_established(uint16_t browsing_cid, bd_addr_t addr, uint8_t status){
    btstack_assert(avrcp_browsing_callback != NULL);
    
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*avrcp_browsing_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_browsing_emit_incoming_connection(uint16_t browsing_cid, bd_addr_t addr){
    btstack_assert(avrcp_browsing_callback != NULL);
    
    uint8_t event[11];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*avrcp_browsing_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_browsing_emit_connection_closed(uint16_t browsing_cid){
    btstack_assert(avrcp_browsing_callback != NULL);
    
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, browsing_cid);
    pos += 2;
    (*avrcp_browsing_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


static avrcp_browsing_connection_t * avrcp_browsing_create_connection(avrcp_connection_t * avrcp_connection, uint16_t avrcp_browsing_cid){
    avrcp_browsing_connection_t * browsing_connection = btstack_memory_avrcp_browsing_connection_get();
    if (!browsing_connection){
        log_error("Not enough memory to create browsing connection");
        return NULL;
    }
    browsing_connection->state = AVCTP_CONNECTION_IDLE;
    browsing_connection->transaction_label = 0xFF;
    
    avrcp_connection->avrcp_browsing_cid = avrcp_browsing_cid;
    avrcp_connection->browsing_connection = browsing_connection;

    log_info("avrcp_browsing_create_connection, avrcp cid 0x%02x", avrcp_connection->avrcp_browsing_cid);
    return browsing_connection;
}

static void avrcp_browsing_configure_ertm(avrcp_browsing_connection_t * browsing_connection, uint8_t * ertm_buffer, uint32_t ertm_buffer_size, l2cap_ertm_config_t * ertm_config){
    browsing_connection->ertm_buffer = ertm_buffer;
    browsing_connection->ertm_buffer_size = ertm_buffer_size;
    
    if (ertm_buffer_size > 0) {
        (void)memcpy(&browsing_connection->ertm_config, ertm_config,
                 sizeof(l2cap_ertm_config_t));
        log_info("avrcp_browsing_configure_ertm");
    }
}

static avrcp_browsing_connection_t * avrcp_browsing_handle_incoming_connection(avrcp_connection_t * connection, uint16_t local_cid, uint16_t avrcp_browsing_cid){
    if (connection->browsing_connection == NULL){
        avrcp_browsing_create_connection(connection, avrcp_browsing_cid);
    }
    if (connection->browsing_connection) {
        connection->browsing_connection->l2cap_browsing_cid = local_cid;
        connection->browsing_connection->state = AVCTP_CONNECTION_W4_ERTM_CONFIGURATION;
        btstack_run_loop_remove_timer(&connection->retry_timer);
    } 
    return connection->browsing_connection;
}

static void avrcp_browsing_handle_open_connection_for_role(avrcp_connection_t * connection, uint16_t local_cid){
    connection->browsing_connection->l2cap_browsing_cid = local_cid;
    connection->browsing_connection->incoming_declined = false;
    connection->browsing_connection->state = AVCTP_CONNECTION_OPENED;
    log_info("L2CAP_EVENT_CHANNEL_OPENED browsing_avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x, role %d", connection->avrcp_cid, connection->l2cap_signaling_cid, connection->role);
}

static avrcp_frame_type_t avrcp_get_frame_type(uint8_t header){
    return (avrcp_frame_type_t)((header & 0x02) >> 1);
}

static void avrcp_browsing_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;
    bool decline_connection;
    bool outoing_active;

    avrcp_connection_t * connection_controller;
    avrcp_connection_t * connection_target;

    switch (packet_type){
        case L2CAP_DATA_PACKET:
            switch (avrcp_get_frame_type(packet[0])){
                case AVRCP_RESPONSE_FRAME:
                    (*avrcp_browsing_controller_packet_handler)(packet_type, channel, packet, size);
                    break;
                case AVRCP_COMMAND_FRAME:
                default:    // make compiler happy
                    (*avrcp_browsing_target_packet_handler)(packet_type, channel, packet, size);
                    break;
            }
            break;
        case HCI_EVENT_PACKET:
            btstack_assert(avrcp_browsing_controller_packet_handler != NULL);
            btstack_assert(avrcp_browsing_target_packet_handler != NULL);
  
            switch (hci_event_packet_get_type(packet)) {
                
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    btstack_assert(avrcp_browsing_controller_packet_handler != NULL);
                    btstack_assert(avrcp_browsing_target_packet_handler != NULL);

                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    outoing_active = false;

                    connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, event_addr);
                    connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, event_addr);
                    
                    if (connection_target == NULL || connection_controller == NULL) {
                        l2cap_decline_connection(local_cid);
                        return;
                    }

                    if (connection_target->browsing_connection != NULL){
                        if (connection_target->browsing_connection->state == AVCTP_CONNECTION_W4_L2CAP_CONNECTED){
                            outoing_active = true;
                            connection_target->browsing_connection->incoming_declined = true;
                        }
                    }
                    
                    if (connection_controller->browsing_connection != NULL){
                        if (connection_controller->browsing_connection->state == AVCTP_CONNECTION_W4_L2CAP_CONNECTED) {
                            outoing_active = true;
                            connection_controller->browsing_connection->incoming_declined = true;
                        }
                    }

                    decline_connection = outoing_active;
                    if (decline_connection == false){
                        uint16_t avrcp_browsing_cid;
                        if ((connection_controller->browsing_connection == NULL) || (connection_target->browsing_connection == NULL)){
                            avrcp_browsing_cid = avrcp_get_next_cid(AVRCP_CONTROLLER);
                        } else {
                            avrcp_browsing_cid = connection_controller->avrcp_browsing_cid;
                        }

                        // create two connection objects (both)
                        connection_target->browsing_connection     = avrcp_browsing_handle_incoming_connection(connection_target, local_cid, avrcp_browsing_cid);
                        connection_controller->browsing_connection = avrcp_browsing_handle_incoming_connection(connection_controller, local_cid, avrcp_browsing_cid);
                        
                        if ((connection_target->browsing_connection  == NULL) || (connection_controller->browsing_connection == NULL)){
                            decline_connection = true;
                            if (connection_target->browsing_connection) {
                                avrcp_browsing_finalize_connection(connection_target);
                            } 
                            if (connection_controller->browsing_connection) {
                                avrcp_browsing_finalize_connection(connection_controller);
                            }
                        }
                    }
                    if (decline_connection){
                        l2cap_decline_connection(local_cid);
                    } else {
                        log_info("AVRCP: L2CAP_EVENT_INCOMING_CONNECTION browsing_avrcp_cid 0x%02x", connection_controller->avrcp_browsing_cid);
                        avrcp_browsing_emit_incoming_connection(connection_controller->avrcp_browsing_cid, event_addr);
                    }
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    status = l2cap_event_channel_opened_get_status(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    
                    connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, event_addr);
                    connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, event_addr);

                    // incoming: structs are already created in L2CAP_EVENT_INCOMING_CONNECTION
                    // outgoing: structs are cteated in avrcp_connect() and avrcp_browsing_connect()
                    if ((connection_controller == NULL) || (connection_target == NULL)) {
                        break;
                    }
                    if ((connection_controller->browsing_connection == NULL) || (connection_target->browsing_connection == NULL)) {
                        break;
                    }
                    
                    switch (status){
                        case ERROR_CODE_SUCCESS:
                            avrcp_browsing_handle_open_connection_for_role(connection_target, local_cid);
                            avrcp_browsing_handle_open_connection_for_role(connection_controller, local_cid);
                            avrcp_browsing_emit_connection_established(connection_controller->avrcp_browsing_cid, event_addr, status);
                            return;
                        case L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES: 
                            if (connection_controller->browsing_connection->incoming_declined == true){
                                log_info("Incoming browsing connection was declined, and the outgoing failed");
                                connection_controller->browsing_connection->state = AVCTP_CONNECTION_W2_L2CAP_RETRY;
                                connection_controller->browsing_connection->incoming_declined = false;
                                connection_target->browsing_connection->state = AVCTP_CONNECTION_W2_L2CAP_RETRY;
                                connection_target->browsing_connection->incoming_declined = false;
                                avrcp_retry_timer_start(connection_controller);
                                return;
                            } 
                            break;
                        default:
                            break;
                    }
                    log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                    avrcp_browsing_emit_connection_established(connection_controller->avrcp_browsing_cid, event_addr, status);
                    avrcp_browsing_finalize_connection(connection_controller);
                    avrcp_browsing_finalize_connection(connection_target);
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
        
                    connection_controller = avrcp_get_connection_for_browsing_l2cap_cid_for_role(AVRCP_CONTROLLER, local_cid);
                    connection_target = avrcp_get_connection_for_browsing_l2cap_cid_for_role(AVRCP_TARGET, local_cid);
                    if ((connection_controller == NULL) || (connection_target == NULL)) {
                        break;
                    }
                    if ((connection_controller->browsing_connection == NULL) || (connection_target->browsing_connection == NULL)) {
                        break;
                    }
                    avrcp_browsing_emit_connection_closed(connection_controller->avrcp_browsing_cid);
                    avrcp_browsing_finalize_connection(connection_controller);
                    avrcp_browsing_finalize_connection(connection_target);
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    local_cid = l2cap_event_can_send_now_get_local_cid(packet);
                    connection_target = avrcp_get_connection_for_browsing_l2cap_cid_for_role(AVRCP_TARGET, local_cid);
                    if ((connection_target != NULL) && (connection_target->browsing_connection != NULL) && connection_target->browsing_connection->wait_to_send) {
                        connection_target->browsing_connection->wait_to_send = false;
                        (*avrcp_browsing_target_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
                        break;  
                    }
                    connection_controller = avrcp_get_connection_for_browsing_l2cap_cid_for_role(AVRCP_CONTROLLER, local_cid);
                    if ((connection_controller != NULL) && (connection_controller->browsing_connection != NULL) && connection_controller->browsing_connection->wait_to_send) {
                        connection_controller->browsing_connection->wait_to_send = false;
                        (*avrcp_browsing_controller_packet_handler)(HCI_EVENT_PACKET, channel, packet, size);
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

void avrcp_browsing_init(void){
    if (avrcp_browsing_l2cap_service_registered) return;
    int status = l2cap_register_service(&avrcp_browsing_packet_handler, PSM_AVCTP_BROWSING, 0xffff, LEVEL_2);

    if (status != ERROR_CODE_SUCCESS) return;
    avrcp_browsing_l2cap_service_registered = true;
}

void avrcp_browsing_deinit(void){
    avrcp_browsing_callback = NULL;
    avrcp_browsing_controller_packet_handler = NULL;
    avrcp_browsing_target_packet_handler = NULL;

    (void) memset(avrcp_browsing_sdp_addr, 0, 6);
    (void) memset(&avrcp_browsing_handle_sdp_client_query_request, 0, sizeof(avrcp_browsing_handle_sdp_client_query_request));
    (void) memset(&avrcp_browsing_sdp_query_context, 0, sizeof(avrcp_browsing_sdp_query_context_t));

    avrcp_browsing_l2cap_service_registered = false;
}

static void avrcp_browsing_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    avrcp_connection_t * avrcp_target_connection = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_sdp_query_context.browsing_cid);
    avrcp_connection_t * avrcp_controller_connection = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_CONTROLLER, avrcp_browsing_sdp_query_context.browsing_cid);

    if ((avrcp_target_connection == NULL) || (avrcp_target_connection->browsing_connection == NULL)) return;
    if (avrcp_target_connection->browsing_connection->state != AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE) return;

    if ((avrcp_controller_connection == NULL) || (avrcp_controller_connection->browsing_connection == NULL)) return;
    if (avrcp_controller_connection->browsing_connection->state != AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE) return;
    

    uint8_t status;
    uint16_t browsing_l2cap_psm;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            avrcp_handle_sdp_client_query_attribute_value(packet);
            break;

        case SDP_EVENT_QUERY_COMPLETE:
            status = sdp_event_query_complete_get_status(packet);

            if (status != ERROR_CODE_SUCCESS){
                avrcp_browsing_emit_connection_established(avrcp_target_connection->avrcp_browsing_cid, avrcp_browsing_sdp_addr, status);
                avrcp_browsing_finalize_connection(avrcp_target_connection);
                avrcp_browsing_finalize_connection(avrcp_controller_connection);
                break;
            }

            browsing_l2cap_psm = avrcp_sdp_query_browsing_l2cap_psm();
            if (!browsing_l2cap_psm){
                avrcp_browsing_emit_connection_established(avrcp_target_connection->avrcp_browsing_cid, avrcp_browsing_sdp_addr, SDP_SERVICE_NOT_FOUND);
                avrcp_browsing_finalize_connection(avrcp_target_connection);
                avrcp_browsing_finalize_connection(avrcp_controller_connection);
                break;
            }

            l2cap_ertm_create_channel(avrcp_browsing_packet_handler, avrcp_browsing_sdp_addr, browsing_l2cap_psm,
                                            &avrcp_controller_connection->browsing_connection->ertm_config,
                                            avrcp_controller_connection->browsing_connection->ertm_buffer,
                                            avrcp_controller_connection->browsing_connection->ertm_buffer_size, NULL);
            break;

        default:
            break;
    }
    // register the SDP Query request to check if there is another connection waiting for the query
    // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
    (void) sdp_client_register_query_callback(&avrcp_browsing_handle_sdp_client_query_request);
}

static void avrcp_browsing_handle_start_sdp_client_query(void * context){
    UNUSED(context);
    // TODO

    btstack_linked_list_t connections = avrcp_get_connections();
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_connection_t * connection = (avrcp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->browsing_connection == NULL) continue;
        if (connection->browsing_connection->state != AVCTP_CONNECTION_W2_SEND_SDP_QUERY) continue;
        connection->browsing_connection->state = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
        
        // prevent triggering SDP query twice (for each role once)
        avrcp_connection_t * connection_with_opposite_role;       
        switch (connection->role){
            case AVRCP_CONTROLLER:
                connection_with_opposite_role = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_TARGET, connection->avrcp_cid);
                break;
            case AVRCP_TARGET:
                connection_with_opposite_role = avrcp_get_connection_for_avrcp_cid_for_role(AVRCP_CONTROLLER, connection->avrcp_cid);
                break;
            default:    
                btstack_assert(false);
                return;
        }
        if (connection->browsing_connection != NULL){
            connection_with_opposite_role->browsing_connection->state = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
        }

        avrcp_browsing_sdp_query_context.browsing_l2cap_psm = 0;
        avrcp_browsing_sdp_query_context.browsing_version = 0;
        avrcp_browsing_sdp_query_context.browsing_cid = connection->avrcp_browsing_cid;
        
        sdp_client_query_uuid16(&avrcp_browsing_handle_sdp_client_query_result, (uint8_t *) connection->remote_addr, BLUETOOTH_PROTOCOL_AVCTP);
        return;
    }
}


uint8_t avrcp_browsing_connect(bd_addr_t remote_addr, uint8_t * ertm_buffer, uint32_t ertm_buffer_size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid){
    btstack_assert(avrcp_browsing_controller_packet_handler != NULL);
    btstack_assert(avrcp_browsing_target_packet_handler != NULL);

    avrcp_connection_t * connection_controller = avrcp_get_connection_for_bd_addr_for_role(AVRCP_CONTROLLER, remote_addr);
    if (!connection_controller){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    avrcp_connection_t * connection_target = avrcp_get_connection_for_bd_addr_for_role(AVRCP_TARGET, remote_addr);
    if (!connection_target){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (connection_controller->browsing_connection){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (connection_target->browsing_connection){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint16_t cid = avrcp_get_next_cid(AVRCP_CONTROLLER);

    connection_controller->browsing_connection = avrcp_browsing_create_connection(connection_controller, cid);
    if (!connection_controller->browsing_connection) return BTSTACK_MEMORY_ALLOC_FAILED;
    
    connection_target->browsing_connection = avrcp_browsing_create_connection(connection_target, cid);
    if (!connection_target->browsing_connection){
        avrcp_browsing_finalize_connection(connection_controller);
        return BTSTACK_MEMORY_ALLOC_FAILED;
    } 
    avrcp_browsing_configure_ertm(connection_controller->browsing_connection, ertm_buffer, ertm_buffer_size, ertm_config);
    avrcp_browsing_configure_ertm(connection_target->browsing_connection, ertm_buffer, ertm_buffer_size, ertm_config);

    if (avrcp_browsing_cid != NULL){
        *avrcp_browsing_cid = cid;
    }

    if (connection_controller->browsing_l2cap_psm == 0){
        memcpy(avrcp_browsing_sdp_addr, remote_addr, 6);
        connection_controller->browsing_connection->state = AVCTP_CONNECTION_W2_SEND_SDP_QUERY;
        connection_target->browsing_connection->state     = AVCTP_CONNECTION_W2_SEND_SDP_QUERY;
        avrcp_browsing_handle_sdp_client_query_request.callback = &avrcp_browsing_handle_start_sdp_client_query;

        // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
        (void) sdp_client_register_query_callback(&avrcp_browsing_handle_sdp_client_query_request);
        return ERROR_CODE_SUCCESS;
    } else {
        connection_controller->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
        connection_target->browsing_connection->state     = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;

        return l2cap_ertm_create_channel(avrcp_browsing_packet_handler, remote_addr, connection_controller->browsing_l2cap_psm,
                                         &connection_controller->browsing_connection->ertm_config,
                                         connection_controller->browsing_connection->ertm_buffer,
                                         connection_controller->browsing_connection->ertm_buffer_size, NULL);
    }
}

uint8_t avrcp_browsing_configure_incoming_connection(uint16_t avrcp_browsing_cid, uint8_t * ertm_buffer, uint32_t ertm_buffer_size, l2cap_ertm_config_t * ertm_config){
    avrcp_connection_t * connection_controller = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!connection_controller){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_connection_t * connection_target = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!connection_target){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (!connection_controller->browsing_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!connection_target->browsing_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection_controller->browsing_connection->state != AVCTP_CONNECTION_W4_ERTM_CONFIGURATION){
        return ERROR_CODE_COMMAND_DISALLOWED;
    } 

    avrcp_browsing_configure_ertm(connection_controller->browsing_connection, ertm_buffer, ertm_buffer_size, ertm_config);
    avrcp_browsing_configure_ertm(connection_target->browsing_connection, ertm_buffer, ertm_buffer_size, ertm_config);

    connection_controller->browsing_connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    connection_target->browsing_connection->state     = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
    
    l2cap_ertm_accept_connection(connection_controller->browsing_connection->l2cap_browsing_cid,
        &connection_controller->browsing_connection->ertm_config, 
        connection_controller->browsing_connection->ertm_buffer, 
        connection_controller->browsing_connection->ertm_buffer_size);
    return ERROR_CODE_SUCCESS;
}


uint8_t avrcp_browsing_decline_incoming_connection(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * connection_controller = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!connection_controller){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_connection_t * connection_target = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!connection_target){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (!connection_controller->browsing_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!connection_target->browsing_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection_controller->browsing_connection->state != AVCTP_CONNECTION_W4_ERTM_CONFIGURATION){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    l2cap_decline_connection(connection_controller->browsing_connection->l2cap_browsing_cid);

    avrcp_browsing_finalize_connection(connection_controller);
    avrcp_browsing_finalize_connection(connection_target);
    return ERROR_CODE_SUCCESS;
}

uint8_t avrcp_browsing_disconnect(uint16_t avrcp_browsing_cid){
    avrcp_connection_t * connection_controller = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_CONTROLLER, avrcp_browsing_cid);
    if (!connection_controller){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    avrcp_connection_t * connection_target = avrcp_get_connection_for_browsing_cid_for_role(AVRCP_TARGET, avrcp_browsing_cid);
    if (!connection_target){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (!connection_controller->browsing_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (!connection_target->browsing_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    l2cap_disconnect(connection_controller->browsing_connection->l2cap_browsing_cid);
    return ERROR_CODE_SUCCESS;
}

void avrcp_browsing_register_controller_packet_handler(btstack_packet_handler_t callback){
    avrcp_browsing_controller_packet_handler = callback;
}

void avrcp_browsing_register_target_packet_handler(btstack_packet_handler_t callback){
    avrcp_browsing_target_packet_handler = callback;
}

void avrcp_browsing_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    avrcp_browsing_callback = callback;
}
