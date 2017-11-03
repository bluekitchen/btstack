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

#define __BTSTACK_FILE__ "avrcp_browsing_controller.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "classic/avrcp.h"
#include "classic/avrcp_browsing_controller.h"

static avrcp_context_t avrcp_browsing_controller_context;
static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t  avrcp_cid_counter = 0;

static avrcp_context_t * sdp_query_context;
static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context);

static uint16_t avrcp_get_next_cid(void){
    avrcp_cid_counter++;
    if (avrcp_cid_counter == 0){
        avrcp_cid_counter = 1;
    }
    return avrcp_cid_counter;
}

static avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_l2cap_cid(uint16_t l2cap_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_browsing_connection_t * connection = (avrcp_browsing_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_browsing_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

static avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_cid(uint16_t avrcp_cid, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *)  &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_browsing_connection_t * connection = (avrcp_browsing_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->browsing_cid != avrcp_cid) continue;
        return connection;
    }
    return NULL;
}

static avrcp_browsing_connection_t * get_avrcp_browsing_connection_for_bd_addr(bd_addr_t addr, avrcp_context_t * context){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &context->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avrcp_browsing_connection_t * connection = (avrcp_browsing_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

static void avrcp_emit_browsing_connection_established(btstack_packet_handler_t callback, uint16_t avrcp_cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED;
    event[pos++] = status;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_emit_browsing_connection_closed(btstack_packet_handler_t callback, uint16_t avrcp_cid){
    if (!callback) return;
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_AVRCP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, avrcp_cid);
    pos += 2;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void avrcp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_browsing_connection_t * connection = get_avrcp_browsing_connection_for_cid(sdp_query_context->avrcp_cid, sdp_query_context);
    if (!connection) return;
    if (connection->state != AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE) return;
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    // uint32_t avdtp_remote_uuid    = 0;
    
    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != sdp_query_context->record_id) {
                sdp_query_context->record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                sdp_query_context->parse_sdp_record = 0;
                printf("SDP Record: Nr: %d\n", sdp_query_context->record_id);
            }

            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
                
                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                            if (de_get_element_type(attribute_value) != DE_DES) break;
                            for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {
                                uint8_t * element = des_iterator_get_element(&des_list_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uint32_t uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_TARGET:
                                        if (sdp_query_context->role == AVRCP_CONTROLLER) {
                                            sdp_query_context->parse_sdp_record = 1;
                                            printf(" Controller \n");
                                            break;
                                        }
                                        break;
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL:
                                    case BLUETOOTH_SERVICE_CLASS_AV_REMOTE_CONTROL_CONTROLLER:
                                        if (sdp_query_context->role == AVRCP_TARGET) {
                                        	printf(" Target \n");
                                            sdp_query_context->parse_sdp_record = 1;
                                            break;
                                        }
                                        break;
                                    default:
                                    	printf(" not found\n");
                                        break;
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST: {
                                if (!sdp_query_context->parse_sdp_record) break;
                                // log_info("SDP Attribute: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet));
                                for (des_iterator_init(&des_list_it, attribute_value); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avrcp_l2cap_psm);
                                            printf(" found signaling PSM: 0x%02x\n", sdp_query_context->avrcp_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVCTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avrcp_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS: {
                                // log_info("SDP Attribute: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet));
                                if (!sdp_query_context->parse_sdp_record) break;
                                if (de_get_element_type(attribute_value) != DE_DES) break;

                                des_iterator_t des_list_0_it;
                                uint8_t       *element_0; 

                                des_iterator_init(&des_list_0_it, attribute_value);
                                element_0 = des_iterator_get_element(&des_list_0_it);

                                for (des_iterator_init(&des_list_it, element_0); des_iterator_has_more(&des_list_it); des_iterator_next(&des_list_it)) {                                    
                                    uint8_t       *des_element;
                                    uint8_t       *element;
                                    uint32_t       uuid;

                                    if (des_iterator_get_type(&des_list_it) != DE_DES) continue;

                                    des_element = des_iterator_get_element(&des_list_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);

                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avrcp_browsing_l2cap_psm);
                                            printf(" found browsing PSM: 0x%02x\n", sdp_query_context->avrcp_browsing_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVCTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avrcp_browsing_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
            } else {
                log_error("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            status = sdp_event_query_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                avrcp_emit_browsing_connection_established(sdp_query_context->avrcp_callback, connection->browsing_cid, connection->remote_addr, status);
                btstack_linked_list_remove(&sdp_query_context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avrcp_browsing_connection_free(connection);
                log_info("AVRCP: SDP query failed with status 0x%02x.", status);
                break;
            }

            if (!sdp_query_context->parse_sdp_record || !sdp_query_context->avrcp_browsing_l2cap_psm){
                connection->state = AVCTP_CONNECTION_IDLE;
                avrcp_emit_browsing_connection_established(sdp_query_context->avrcp_callback, connection->browsing_cid, connection->remote_addr, SDP_SERVICE_NOT_FOUND);
                btstack_linked_list_remove(&sdp_query_context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avrcp_browsing_connection_free(connection);
                break;                
            } 
            // log_info("AVRCP Control PSM 0x%02x, Browsing PSM 0x%02x", sdp_query_context->avrcp_l2cap_psm, sdp_query_context->avrcp_browsing_l2cap_psm);
            connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            
            l2cap_create_ertm_channel(sdp_query_context->packet_handler, connection->remote_addr, sdp_query_context->avrcp_browsing_l2cap_psm, 
            	&connection->ertm_config, connection->ertm_buffer, connection->ertm_buffer_size, NULL);

            // l2cap_create_channel(sdp_query_context->packet_handler, connection->remote_addr, sdp_query_context->avrcp_l2cap_psm, l2cap_max_mtu(), NULL);
            break;
    }
}


static avrcp_browsing_connection_t * avrcp_browsing_create_connection(bd_addr_t remote_addr, avrcp_context_t * context){
    avrcp_browsing_connection_t * connection = btstack_memory_avrcp_browsing_connection_get();
    if (!connection){
        log_error("avrcp: not enough memory to create connection");
        return NULL;
    }
    memset(connection, 0, sizeof(avrcp_browsing_connection_t));
    connection->state = AVCTP_CONNECTION_IDLE;
    connection->transaction_label = 0xFF;
    connection->browsing_cid = avrcp_get_next_cid();
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&context->connections, (btstack_linked_item_t *) connection);
    return connection;
}

static uint8_t avrcp_browsing_connect(bd_addr_t bd_addr, avrcp_context_t * context, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_cid){
    avrcp_browsing_connection_t * connection = get_avrcp_browsing_connection_for_bd_addr(bd_addr, context);
    if (connection){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    connection = avrcp_browsing_create_connection(bd_addr, context);
    if (!connection){
        printf("avrcp: could not allocate connection struct.");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }
    
    // if (!avrcp_cid) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    
    *avrcp_cid = connection->browsing_cid; 
    connection->state = AVCTP_CONNECTION_W4_SDP_QUERY_COMPLETE;
    connection->ertm_buffer = ertm_buffer;
    connection->ertm_buffer_size = size;
    memcpy(&connection->ertm_config, ertm_config, sizeof(l2cap_ertm_config_t));

    context->parse_sdp_record = 0;
    context->record_id = 0;
    context->avrcp_l2cap_psm = 0;
    context->avrcp_version = 0;
    context->avrcp_browsing_l2cap_psm = 0;
    context->avrcp_browsing_version = 0;

    context->avrcp_cid = connection->browsing_cid;
    sdp_query_context = context;
    printf(" start SDP query\n");
    return sdp_client_query_uuid16(&avrcp_handle_sdp_client_query_result, bd_addr, BLUETOOTH_PROTOCOL_AVCTP);
}

void avrcp_browser_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avrcp_context_t * context){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;
    avrcp_browsing_connection_t * connection = NULL;
    
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            avrcp_emit_browsing_connection_closed(context->avrcp_callback, 0);
            break;
        case L2CAP_EVENT_INCOMING_CONNECTION:
            l2cap_event_incoming_connection_get_address(packet, event_addr);
            local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
            connection = avrcp_browsing_create_connection(event_addr, context);
            if (!connection) {
                log_error("Failed to alloc connection structure");
                l2cap_decline_connection(local_cid);
                break;
            }
            connection->state = AVCTP_CONNECTION_W4_L2CAP_CONNECTED;
            connection->l2cap_browsing_cid = local_cid;
            log_info("L2CAP_EVENT_INCOMING_CONNECTION avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x", connection->browsing_cid, connection->l2cap_browsing_cid);
            // l2cap_accept_connection(local_cid);
            printf("L2CAP Accepting incoming connection request in ERTM\n"); 
            l2cap_accept_ertm_connection(local_cid, &connection->ertm_config, connection->ertm_buffer, connection->ertm_buffer_size);
            break;
            
        case L2CAP_EVENT_CHANNEL_OPENED:
            l2cap_event_channel_opened_get_address(packet, event_addr);
            status = l2cap_event_channel_opened_get_status(packet);
            local_cid = l2cap_event_channel_opened_get_local_cid(packet);
            
            connection = get_avrcp_browsing_connection_for_bd_addr(event_addr, context);
            if (!connection){
                log_error("Failed to alloc AVRCP connection structure");
                avrcp_emit_browsing_connection_established(context->avrcp_callback, connection->browsing_cid, event_addr, BTSTACK_MEMORY_ALLOC_FAILED);
                l2cap_disconnect(local_cid, 0); // reason isn't used
                break;
            }
            if (status != ERROR_CODE_SUCCESS){
                log_info("L2CAP connection to connection %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                avrcp_emit_browsing_connection_established(context->avrcp_callback, connection->browsing_cid, event_addr, status);
                btstack_linked_list_remove(&context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avrcp_browsing_connection_free(connection);
                break;
            }
            connection->l2cap_browsing_cid = local_cid;

            log_info("L2CAP_EVENT_CHANNEL_OPENED avrcp_cid 0x%02x, l2cap_signaling_cid 0x%02x", connection->browsing_cid, connection->l2cap_browsing_cid);
            connection->state = AVCTP_CONNECTION_OPENED;
            avrcp_emit_browsing_connection_established(context->avrcp_callback, connection->browsing_cid, event_addr, ERROR_CODE_SUCCESS);
            break;
        
        case L2CAP_EVENT_CHANNEL_CLOSED:
            // data: event (8), len(8), channel (16)
            local_cid = l2cap_event_channel_closed_get_local_cid(packet);
            connection = get_avrcp_browsing_connection_for_l2cap_cid(local_cid, context);
            if (connection){
                avrcp_emit_browsing_connection_closed(context->avrcp_callback, connection->browsing_cid);
                // free connection
                btstack_linked_list_remove(&context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avrcp_browsing_connection_free(connection);
                break;
            }
            break;
        default:
            break;
    }
}

// static void avrcp_handle_l2cap_data_packet_for_browsing_connection(avrcp_browsing_connection_t * connection, uint8_t *packet, uint16_t size){

// }

// static void avrcp_browsing_controller_handle_can_send_now(avrcp_browsing_connection_t * connection){
//     int i;
//     switch (connection->state){
//         case AVCTP_CONNECTION_OPENED:
//             return;
//         default:
//             return;
//     }
// }

static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avrcp_browsing_connection_t * connection;

    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avrcp_browsing_connection_for_l2cap_cid(channel, &avrcp_browsing_controller_context);
            if (!connection) break;
            // avrcp_handle_l2cap_data_packet_for_browsing_connection(connection, packet, size);
            break;
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)){
                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = get_avrcp_browsing_connection_for_l2cap_cid(channel, &avrcp_browsing_controller_context);
                    if (!connection) break;
                    // avrcp_browsing_controller_handle_can_send_now(connection);
                    break;
            default:
                avrcp_browser_packet_handler(packet_type, channel, packet, size, &avrcp_browsing_controller_context);
                break;
        }
        default:
            break;
    }
}

void avrcp_browsing_controller_init(void){
    avrcp_browsing_controller_context.role = AVRCP_CONTROLLER;
    avrcp_browsing_controller_context.connections = NULL;
    avrcp_browsing_controller_context.packet_handler = avrcp_browsing_controller_packet_handler;
    l2cap_register_service(&avrcp_browsing_controller_packet_handler, BLUETOOTH_PROTOCOL_AVCTP, 0xffff, LEVEL_0);
}

void avrcp_browsing_controller_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avrcp_browsing_controller_register_packet_handler called with NULL callback");
        return;
    }
    avrcp_browsing_controller_context.avrcp_callback = callback;
}

uint8_t avrcp_browsing_controller_connect(bd_addr_t bd_addr, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid){
    return avrcp_browsing_connect(bd_addr, &avrcp_browsing_controller_context, ertm_buffer, size, ertm_config, avrcp_browsing_cid);
}

uint8_t avrcp_browsing_controller_disconnect(uint16_t avrcp_browsing_cid){
    avrcp_browsing_connection_t * connection = get_avrcp_browsing_connection_for_cid(avrcp_browsing_cid, &avrcp_browsing_controller_context);
    if (!connection){
        log_error("avrcp_browsing_controller_disconnect: could not find a connection.");
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (connection->state != AVCTP_CONNECTION_OPENED) return ERROR_CODE_COMMAND_DISALLOWED;
    l2cap_disconnect(connection->l2cap_browsing_cid, 0);
    return ERROR_CODE_SUCCESS;
}
