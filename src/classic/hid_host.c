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

#define BTSTACK_FILE__ "hid_host.c"

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_hid_parser.h"
#include "btstack_memory.h"
#include "classic/hid.h"
#include "classic/hid_host.h"
#include "classic/sdp_util.h"
#include "classic/sdp_client.h"
#include "l2cap.h"

#define MAX_ATTRIBUTE_VALUE_SIZE 300

static btstack_packet_handler_t hid_callback;

static uint8_t * hid_host_descriptor_storage;
static uint16_t hid_host_descriptor_storage_len;

static btstack_linked_list_t connections;
static uint16_t hid_host_cid_counter = 0;

// SDP
static uint8_t            attribute_value[MAX_ATTRIBUTE_VALUE_SIZE];
static const unsigned int attribute_value_buffer_size = MAX_ATTRIBUTE_VALUE_SIZE;

static uint16_t           sdp_query_context_hid_host_control_cid = 0;

static btstack_context_callback_registration_t hid_host_handle_sdp_client_query_request;
static void hid_host_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t hid_descriptor_storage_get_available_space(void){
    // assumes all descriptors are back to back
    uint16_t free_space = hid_host_descriptor_storage_len;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        free_space -= connection->hid_descriptor_len;
    }
    return free_space;
}

static void hid_descriptor_storage_init(hid_host_connection_t * connection){
    connection->hid_descriptor_len = 0;
    connection->hid_descriptor_max_len = hid_descriptor_storage_get_available_space();
    connection->hid_descriptor_offset = hid_host_descriptor_storage_len - connection->hid_descriptor_max_len;
}

static bool hid_descriptor_storage_store(hid_host_connection_t * connection, uint8_t byte){
    if (connection->hid_descriptor_len >= connection->hid_descriptor_max_len) return false;

    hid_host_descriptor_storage[connection->hid_descriptor_offset + connection->hid_descriptor_len] = byte;
    connection->hid_descriptor_len++;
    return true;
}


static void hid_descriptor_storage_delete(hid_host_connection_t * connection){
    uint16_t next_offset = connection->hid_descriptor_offset + connection->hid_descriptor_len;

    memmove(&hid_host_descriptor_storage[connection->hid_descriptor_offset], 
            &hid_host_descriptor_storage[next_offset],
            hid_host_descriptor_storage_len - next_offset);

    connection->hid_descriptor_len = 0;
    connection->hid_descriptor_offset = 0;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * conn = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if (conn->hid_descriptor_offset >= next_offset){
            conn->hid_descriptor_offset = next_offset;
            next_offset += conn->hid_descriptor_len;
        }
    }
}

// HID Util
static inline void hid_emit_connected_event(hid_host_connection_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_HID_META;
    pos++;  // skip len
    event[pos++] = HID_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event, pos, context->hid_cid);
    pos+=2;
    event[pos++] = status;
    reverse_bd_addr(context->remote_addr, &event[pos]);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    hid_callback(HCI_EVENT_PACKET, context->hid_cid, &event[0], pos);
}   

// HID Host

static uint16_t hid_host_get_next_cid(void){
    if (hid_host_cid_counter == 0xffff) {
        hid_host_cid_counter = 1;
    } else {
        hid_host_cid_counter++;
    }
    return hid_host_cid_counter;
}

static hid_host_connection_t * hid_host_create_connection(bd_addr_t remote_addr){
    hid_host_connection_t * connection = btstack_memory_hid_host_connection_get();
    if (!connection){
        log_error("Not enough memory to create connection");
        return NULL;
    }
    connection->state = HID_HOST_IDLE;
    connection->hid_cid = hid_host_get_next_cid();
    (void)memcpy(connection->remote_addr, remote_addr, 6);
    printf("hid_host_create_connection, cid 0x%02x, %s \n", connection->hid_cid, bd_addr_to_str(connection->remote_addr));

    btstack_linked_list_add(&connections, (btstack_linked_item_t *) connection);
    return connection;
}

static hid_host_connection_t * hid_host_get_connection_for_bd_addr(bd_addr_t addr){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

static hid_host_connection_t * hid_host_connection_for_hid_cid(uint16_t hid_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->hid_cid != hid_cid) continue;
        return connection;
    }
    return NULL;
}

static void hid_host_finalize_connection(hid_host_connection_t * connection){
    btstack_linked_list_remove(&connections, (btstack_linked_item_t*) connection); 
    btstack_memory_hid_host_connection_free(connection);
}

static void hid_host_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t attribute_list_it;
    des_iterator_t additional_des_it;
    des_iterator_t prot_it;
    uint8_t       *des_element;
    uint8_t       *element;
    uint32_t       uuid;
    uint8_t        status = ERROR_CODE_SUCCESS;
    
    hid_host_connection_t * connection = hid_host_connection_for_hid_cid(sdp_query_context_hid_host_control_cid);
    if (!connection) {
        log_error("SDP query, connection with 0x%02x cid not found", sdp_query_context_hid_host_control_cid);
        return;
    }

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:

            if (sdp_event_query_attribute_byte_get_attribute_length(packet) <= attribute_value_buffer_size) {
                
                attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);

                if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)) {
                    switch(sdp_event_query_attribute_byte_get_attribute_id(packet)) {
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                des_iterator_init(&prot_it, des_element);
                                element = des_iterator_get_element(&prot_it);
                                if (de_get_element_type(element) != DE_UUID) continue;
                                uuid = de_get_uuid32(element);
                                switch (uuid){
                                    case BLUETOOTH_PROTOCOL_L2CAP:
                                        if (!des_iterator_has_more(&prot_it)) continue;
                                        des_iterator_next(&prot_it);
                                        de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->control_psm);
                                        log_info("HID Control PSM: 0x%04x", (int) &connection->control_psm);
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        case BLUETOOTH_ATTRIBUTE_ADDITIONAL_PROTOCOL_DESCRIPTOR_LISTS:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {                                    
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_DES) continue;
                                    des_element = des_iterator_get_element(&additional_des_it);
                                    des_iterator_init(&prot_it, des_element);
                                    element = des_iterator_get_element(&prot_it);
                                    if (de_get_element_type(element) != DE_UUID) continue;
                                    uuid = de_get_uuid32(element);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &connection->interrupt_psm);
                                            log_info("HID Interrupt PSM: 0x%04x", (int) &connection->interrupt_psm);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_HID_DESCRIPTOR_LIST:
                            for (des_iterator_init(&attribute_list_it, attribute_value); des_iterator_has_more(&attribute_list_it); des_iterator_next(&attribute_list_it)) {
                                if (des_iterator_get_type(&attribute_list_it) != DE_DES) continue;
                                des_element = des_iterator_get_element(&attribute_list_it);
                                for (des_iterator_init(&additional_des_it, des_element); des_iterator_has_more(&additional_des_it); des_iterator_next(&additional_des_it)) {                                    
                                    if (des_iterator_get_type(&additional_des_it) != DE_STRING) continue;
                                    element = des_iterator_get_element(&additional_des_it);
                                    
                                    const uint8_t * descriptor = de_get_string(element);
                                    uint16_t descriptor_len = de_get_data_size(element);
                                    
                                    uint16_t i;
                                    for (i = 0; i < descriptor_len; i++){
                                        hid_descriptor_storage_store(connection, descriptor[i]);
                                    }
                                    printf("HID Descriptor:\n");
                                    printf_hexdump(descriptor, descriptor_len);
                                }
                            }                        
                            break;
                        default:
                            break;
                    }
                }
            } else {
                fprintf(stderr, "SDP attribute value buffer size exceeded: available %d, required %d\n", attribute_value_buffer_size, sdp_event_query_attribute_byte_get_attribute_length(packet));
            }
            break;
            
        case SDP_EVENT_QUERY_COMPLETE:
            if (!connection->control_psm) {
                printf("HID Control PSM missing\n");
                status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
                break;
            }
            if (!connection->interrupt_psm) {
                printf("HID Interrupt PSM missing\n");
                status = ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
                break;
            }
            
            printf("Setup HID\n");
            status = l2cap_create_channel(hid_host_packet_handler, connection->remote_addr, connection->control_psm, 48, &connection->control_cid);
            if (status){
                printf("Connecting to HID Control failed: 0x%02x\n", status);
            }
            break;

        default:
            // bail out, we must have had an incoming connection in the meantime; just trigger next sdp query on complete
            if (hci_event_packet_get_type(packet) == SDP_EVENT_QUERY_COMPLETE){
                (void) sdp_client_register_query_callback(&hid_host_handle_sdp_client_query_request);
            }
            return;
    }

    if (status != ERROR_CODE_SUCCESS){
        hid_emit_connected_event(connection, status);
        hid_host_finalize_connection(connection);
        sdp_query_context_hid_host_control_cid = 0;
    }
}

static void hid_host_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint8_t   event;
    bd_addr_t address;
    uint8_t   status;
    uint16_t  cid;
    hid_host_connection_t * connection;

    // uint8_t param;
    // hid_message_type_t         message_type;
    // hid_handshake_param_type_t message_status;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, address); 
                    connection = hid_host_get_connection_for_bd_addr(address);
                    
                    if (connection && connection->unplugged){
                        log_info("Decline connection for %s, host is unplugged", bd_addr_to_str(address));
                        l2cap_decline_connection(channel);
                        break;
                    }
                    
                    switch (l2cap_event_incoming_connection_get_psm(packet)){
                        case PSM_HID_CONTROL:
                            if (connection){
                                log_error("Connection already exists %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }

                            connection = hid_host_create_connection(address);
                            if (!connection) {
                                log_error("Cannot create connection for %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }

                            connection->state = HID_HOST_W4_CONTROL_CONNECTION_ESTABLISHED;
                            connection->con_handle = l2cap_event_incoming_connection_get_handle(packet);
                            connection->control_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                            connection->incoming = true;
                            log_info("Accept connection on Control channel %s", bd_addr_to_str(address));
                            l2cap_accept_connection(channel);
                            break;

                        case PSM_HID_INTERRUPT:
                            if (!connection || (connection->interrupt_cid != 0) || (l2cap_event_incoming_connection_get_handle(packet) != connection->con_handle)){
                                log_error("Decline connection for %s", bd_addr_to_str(address));
                                l2cap_decline_connection(channel);
                                break;
                            }

                            connection->state = HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED;
                            connection->interrupt_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                            log_info("Accept connection on Interrupt channel %s", bd_addr_to_str(address));
                            l2cap_accept_connection(channel);
                            break;
                        
                        default:
                            log_info("Decline connection for %s", bd_addr_to_str(address));
                            l2cap_decline_connection(channel);
                            break;
                    }
                    break;

                case L2CAP_EVENT_CHANNEL_OPENED: 
                    l2cap_event_channel_opened_get_address(packet, address); 

                    status = l2cap_event_channel_opened_get_status(packet); 
                    if (status){
                        log_info("L2CAP connection %s failed: 0x%02xn", bd_addr_to_str(address), status);
                        break;
                    }

                    connection = hid_host_get_connection_for_bd_addr(address);
                    if (!connection){
                        log_error("Connection does not exist %s", bd_addr_to_str(address));
                        break;
                    }

                    cid = l2cap_event_channel_opened_get_local_cid(packet);
                    
                    switch (l2cap_event_channel_opened_get_psm(packet)){
                        case PSM_HID_CONTROL:
                            if (connection->state != HID_HOST_W4_CONTROL_CONNECTION_ESTABLISHED) break;
                            connection->state = HID_HOST_CONTROL_CONNECTION_ESTABLISHED;

                            if (connection->boot_mode){
                                 break;
                            } 

                            if (!connection->incoming){
                                status = l2cap_create_channel(hid_host_packet_handler, address, connection->interrupt_psm, 48, &connection->interrupt_cid);
                                if (status){
                                    log_info("Connecting to HID Interrupt failed: 0x%02x", status);
                                    break;
                                }
                                connection->con_handle = l2cap_event_channel_opened_get_handle(packet); 
                                connection->state = HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED;
                            }
                            break;

                        case PSM_HID_INTERRUPT:
                            if (connection->state != HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED) break;
                            if (connection->con_handle != l2cap_event_channel_opened_get_handle(packet)) break;

                            connection->state = HID_HOST_CONNECTION_ESTABLISHED;
                            hid_emit_connected_event(connection, ERROR_CODE_SUCCESS);

                            log_info("Connection on interrupt channel established, interrupt_cid 0x%02x", connection->interrupt_cid);
                            break;

                        default:
                            break;
                    }
                    // disconnect?                    
                    break;
                
                default:
                    break;
            }
        default:
            break;
    }
}


void hid_host_init(uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len){
    hid_host_descriptor_storage = hid_descriptor_storage;
    hid_host_descriptor_storage_len = hid_descriptor_storage_len;

    // register L2CAP Services for reconnections
    l2cap_register_service(hid_host_packet_handler, PSM_HID_INTERRUPT, 0xffff, gap_get_security_level());
    l2cap_register_service(hid_host_packet_handler, PSM_HID_CONTROL, 0xffff, gap_get_security_level());
}

void hid_host_register_packet_handler(btstack_packet_handler_t callback){
    hid_callback = callback;
}


static void hid_host_handle_start_sdp_client_query(void * context){
    UNUSED(context);

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hid_host_connection_t * connection = (hid_host_connection_t *)btstack_linked_list_iterator_next(&it);

        switch (connection->state){
             case HID_HOST_W2_SEND_SDP_QUERY:
                connection->state = HID_HOST_W4_SDP_QUERY_RESULT;
                break;
            default:
                continue;
        }
        printf("hid_descriptor_storage_init, start query, cid 0x%02x, %s \n", connection->hid_cid, bd_addr_to_str(connection->remote_addr));
        hid_descriptor_storage_init(connection);
        sdp_query_context_hid_host_control_cid = connection->hid_cid;
        sdp_client_query_uuid16(&hid_host_handle_sdp_client_query_result, (uint8_t *) connection->remote_addr, BLUETOOTH_SERVICE_CLASS_HUMAN_INTERFACE_DEVICE_SERVICE);
        return;
    }
}

uint8_t hid_host_connect(bd_addr_t remote_addr, hid_protocol_mode_t protocol_mode, uint16_t * hid_cid){
    UNUSED(protocol_mode);

    if (hid_cid == NULL) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    hid_host_connection_t * connection = hid_host_get_connection_for_bd_addr(remote_addr);
    if (connection){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    connection = hid_host_create_connection(remote_addr);
    if (!connection) return BTSTACK_MEMORY_ALLOC_FAILED;

    
    *hid_cid = connection->hid_cid;
    
    connection->state = HID_HOST_W2_SEND_SDP_QUERY;
    connection->incoming = false;
    connection->control_cid = 0;
    connection->control_psm = 0;
    connection->interrupt_cid = 0;
    connection->interrupt_psm = 0;
    printf("hid_host_connect, cid 0x%02x, %s \n", connection->hid_cid, bd_addr_to_str(connection->remote_addr));
        
    hid_host_handle_sdp_client_query_request.callback = &hid_host_handle_start_sdp_client_query;
    // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
    (void) sdp_client_register_query_callback(&hid_host_handle_sdp_client_query_request);

    return ERROR_CODE_SUCCESS;
}


void hid_host_disconnect(uint16_t hid_cid){
    UNUSED(hid_cid);
}

void hid_host_request_can_send_now_event(uint16_t hid_cid){
    UNUSED(hid_cid);
}

void hid_host_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    UNUSED(message);
    UNUSED(message_len);
}

void hid_host_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    UNUSED(message);
    UNUSED(message_len);
}