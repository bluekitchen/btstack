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

#define BTSTACK_FILE__ "avdtp.c"


#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "classic/avdtp.h"
#include "classic/avdtp_acceptor.h"
#include "classic/avdtp_initiator.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_client.h"
#include "classic/sdp_util.h"

#define CONFIGURATION_TIMEOUT_MS 300

static int record_id = -1;
static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

// typedef struct {
//     btstack_linked_list_t * avdtp_connections;
//     avdtp_connection_t * connection;
//     btstack_packet_handler_t avdtp_callback;
//     avdtp_sep_type_t query_role;
//     btstack_packet_handler_t packet_handler;
//     uint16_t avdtp_l2cap_psm;
//     uint16_t avdtp_version;
//     uint8_t  role_supported;
// } avdtp_sdp_query_context_t;

static avdtp_context_t * sdp_query_context;
static uint16_t avdtp_cid_counter = 0;

static void (*handle_media_data)(uint8_t local_seid, uint8_t *packet, uint16_t size);
static void avdtp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void avdtp_configuration_timeout_handler(btstack_timer_source_t * timer){
    avdtp_connection_t * connection = (avdtp_connection_t *) btstack_run_loop_get_timer_context(timer);
    if (!connection){
        log_error("Context of avdtp_configuration_timeout_handler is NULL");
        return;
    }
    avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t*) connection->active_stream_endpoint;
    if (!stream_endpoint) {
        log_error("avdtp_configuration_timeout_handler: no initiator stream endpoint for seid %d", connection->local_seid);
        return;
    }   
    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE) return;    
    connection->is_configuration_initiated_locally = 1;
    connection->is_initiator = 1;
    connection->initiator_transaction_label++;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_SET_CONFIGURATION;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);  
}

void avdtp_configuration_timer_start(avdtp_connection_t * connection){
    avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t*) connection->active_stream_endpoint;
    if (!stream_endpoint) {
        log_error("avdtp_configuration_timeout_handler: no initiator stream endpoint for seid %d", connection->local_seid);
        return;
    }   
    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE) return; 

    btstack_run_loop_remove_timer(&connection->configuration_timer);
    btstack_run_loop_set_timer_handler(&connection->configuration_timer, avdtp_configuration_timeout_handler);
    btstack_run_loop_set_timer_context(&connection->configuration_timer, connection);
    btstack_run_loop_set_timer(&connection->configuration_timer, CONFIGURATION_TIMEOUT_MS); 
    btstack_run_loop_add_timer(&connection->configuration_timer);
}

void avdtp_configuration_timer_stop(avdtp_connection_t * connection){
    btstack_run_loop_remove_timer(&connection->configuration_timer);
} 

static uint16_t avdtp_get_next_initiator_transaction_label(avdtp_context_t * context){
    context->initiator_transaction_id_counter++;
    if (context->initiator_transaction_id_counter == 0){
        context->initiator_transaction_id_counter = 1;
    }
    return context->initiator_transaction_id_counter;
}

static uint16_t avdtp_get_next_avdtp_cid(avdtp_context_t * context){
    do {
        if (avdtp_cid_counter == 0xffff) {
            avdtp_cid_counter = 1;
        } else {
            avdtp_cid_counter++;
        }
    } while (avdtp_connection_for_avdtp_cid(avdtp_cid_counter, context) !=  NULL) ;
    return avdtp_cid_counter;
}

static avdtp_stream_endpoint_t * avdtp_stream_endpoint_for_id(avdtp_context_t * context, uint16_t stream_endpoint_id) {
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) context->stream_endpoints; it ; it = it->next){
        avdtp_stream_endpoint_t * stream_endpoint = ((avdtp_stream_endpoint_t *) it);

        if (stream_endpoint->sep.seid == stream_endpoint_id) {
            return stream_endpoint;
        };
    }
    return NULL;
}

static uint16_t avdtp_get_next_local_seid(avdtp_context_t * context){
    uint16_t stream_endpoint_id = context->stream_endpoints_id_counter;
    do {
        if (stream_endpoint_id == 0xffff) {
            stream_endpoint_id = 1;
        } else {
            stream_endpoint_id++;
        }
    } while (avdtp_stream_endpoint_for_id(context, stream_endpoint_id) !=  NULL) ;
    return stream_endpoint_id;
}


uint8_t avdtp_connect(bd_addr_t remote, avdtp_sep_type_t query_role, avdtp_context_t * avdtp_context, uint16_t * avdtp_cid){
    sdp_query_context = avdtp_context;
    avdtp_connection_t * connection = avdtp_connection_for_bd_addr(remote, avdtp_context);
    if (!connection){
        connection = avdtp_create_connection(remote, avdtp_context);
        if (!connection){
            log_error("Not enough memory to create connection.");
            return BTSTACK_MEMORY_ALLOC_FAILED;
        }
    }
    
    *avdtp_cid = connection->avdtp_cid;
    if (!avdtp_cid) {
        return L2CAP_LOCAL_CID_DOES_NOT_EXIST;
    }
    avdtp_context->avdtp_cid = connection->avdtp_cid;
            
    uint8_t err;
    switch (connection->state){
        case AVDTP_SIGNALING_CONNECTION_IDLE:
            connection->state = AVDTP_SIGNALING_W4_SDP_QUERY_COMPLETE;
            connection->is_initiator = 1;
            sdp_query_context = avdtp_context;
            avdtp_context->avdtp_l2cap_psm = 0;
            avdtp_context->avdtp_version = 0;
            avdtp_context->query_role = query_role;
            err = sdp_client_query_uuid16(&avdtp_handle_sdp_client_query_result, remote, BLUETOOTH_PROTOCOL_AVDTP);
            if (err != ERROR_CODE_SUCCESS){
                connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
                btstack_linked_list_remove(&avdtp_context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avdtp_connection_free(connection);
            }
            return err;
        case AVDTP_SIGNALING_CONNECTION_OPENED:{
            avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_signaling_cid(connection->l2cap_signaling_cid, avdtp_context);
            if (stream_endpoint){
                avdtp_streaming_emit_connection_established(avdtp_context->avdtp_callback, connection->avdtp_cid, remote, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), 0);
                break;
            }
            avdtp_signaling_emit_connection_established(avdtp_context->avdtp_callback, connection->avdtp_cid, connection->remote_addr, ERROR_CODE_SUCCESS);
            break;
        }
        default:
            log_error("avdtp_connect: sink in wrong state");
            return AVDTP_CONNECTION_IN_WRONG_STATE;
            
    }
    return ERROR_CODE_SUCCESS;
}

void avdtp_register_media_transport_category(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MEDIA_TRANSPORT, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_register_reporting_category(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_REPORTING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_register_delay_reporting_category(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_DELAY_REPORTING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_register_recovery_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_RECOVERY, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.recovery.recovery_type = 0x01; // 0x01 = RFC2733
    stream_endpoint->sep.capabilities.recovery.maximum_recovery_window_size = maximum_recovery_window_size;
    stream_endpoint->sep.capabilities.recovery.maximum_number_media_packets = maximum_number_media_packets;
}

void avdtp_register_content_protection_category(avdtp_stream_endpoint_t * stream_endpoint, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_CONTENT_PROTECTION, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.content_protection.cp_type = cp_type;
    memcpy(stream_endpoint->sep.capabilities.content_protection.cp_type_value, cp_type_value, btstack_min(cp_type_value_len, AVDTP_MAX_CONTENT_PROTECTION_TYPE_VALUE_LEN));
    stream_endpoint->sep.capabilities.content_protection.cp_type_value_len = btstack_min(cp_type_value_len, AVDTP_MAX_CONTENT_PROTECTION_TYPE_VALUE_LEN);
}

void avdtp_register_header_compression_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t back_ch, uint8_t media, uint8_t recovery){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_HEADER_COMPRESSION, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.header_compression.back_ch = back_ch;
    stream_endpoint->sep.capabilities.header_compression.media = media;
    stream_endpoint->sep.capabilities.header_compression.recovery = recovery;
}

void avdtp_register_media_codec_category(avdtp_stream_endpoint_t * stream_endpoint, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, uint8_t * media_codec_info, uint16_t media_codec_info_len){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MEDIA_CODEC, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.media_codec.media_type = media_type;
    stream_endpoint->sep.capabilities.media_codec.media_codec_type = media_codec_type;
    stream_endpoint->sep.capabilities.media_codec.media_codec_information = media_codec_info;
    stream_endpoint->sep.capabilities.media_codec.media_codec_information_len = media_codec_info_len;
}

void avdtp_register_multiplexing_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t fragmentation){
    if (!stream_endpoint){
        log_error("Stream endpoint with given seid is not registered.");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MULTIPLEXING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.multiplexing_mode.fragmentation = fragmentation;
}


/* START: tracking can send now requests pro l2cap cid */
void avdtp_handle_can_send_now(avdtp_connection_t * connection, uint16_t l2cap_cid, avdtp_context_t * context){
    if (connection->wait_to_send_acceptor){
        connection->wait_to_send_acceptor = 0;
        avdtp_acceptor_stream_config_subsm_run(connection, context);
    } else if (connection->wait_to_send_initiator){
        connection->wait_to_send_initiator = 0;
        avdtp_initiator_stream_config_subsm_run(connection, context);
    } else if (connection->wait_to_send_self){
        connection->wait_to_send_self = 0;
        if (connection->disconnect){
            btstack_linked_list_iterator_t it;    
            btstack_linked_list_iterator_init(&it, &context->stream_endpoints);
            while (btstack_linked_list_iterator_has_next(&it)){
                avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
                if (stream_endpoint->connection == connection){
                    if (stream_endpoint->state >= AVDTP_STREAM_ENDPOINT_OPENED && stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_DISCONNECTED){
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_DISCONNECTED;
                        avdtp_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        l2cap_disconnect(stream_endpoint->l2cap_media_cid, 0);
                        return;
                    }
                }
            }
            connection->disconnect = 0;
            connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_DISCONNECTED;
            l2cap_disconnect(connection->l2cap_signaling_cid, 0);
            return;
        }
    }

    // re-register
    int more_to_send = connection->wait_to_send_acceptor || connection->wait_to_send_initiator || connection->wait_to_send_self;
    if (more_to_send){
        l2cap_request_can_send_now_event(l2cap_cid);
    }
}
/* END: tracking can send now requests pro l2cap cid */

avdtp_connection_t * avdtp_create_connection(bd_addr_t remote_addr, avdtp_context_t * context){
    avdtp_connection_t * connection = btstack_memory_avdtp_connection_get();
    if (!connection){
        log_error("Not enough memory to create connection");
        return NULL;
    }
    connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
    connection->initiator_transaction_label = avdtp_get_next_initiator_transaction_label(context);
    connection->avdtp_cid = avdtp_get_next_avdtp_cid(context);
    context->avdtp_cid = connection->avdtp_cid;
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&context->connections, (btstack_linked_item_t *) connection);
    return connection;
}

avdtp_stream_endpoint_t * avdtp_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type, avdtp_context_t * context){
    avdtp_stream_endpoint_t * stream_endpoint = btstack_memory_avdtp_stream_endpoint_get();
    if (!stream_endpoint){
        log_error("Not enough memory to create stream endpoint");
        return NULL;
    }
    stream_endpoint->sep.seid = avdtp_get_next_local_seid(context);
    stream_endpoint->sep.media_type = media_type;
    stream_endpoint->sep.type = sep_type;
    btstack_linked_list_add(&context->stream_endpoints, (btstack_linked_item_t *) stream_endpoint);
    return stream_endpoint;
}


static void handle_l2cap_data_packet_for_signaling_connection(avdtp_connection_t * connection, uint8_t *packet, uint16_t size, avdtp_context_t * context){
    int offset = avdtp_read_signaling_header(&connection->signaling_packet, packet, size);
    switch (connection->signaling_packet.message_type){
        case AVDTP_CMD_MSG:
            avdtp_acceptor_stream_config_subsm(connection, packet, size, offset, context);
            break;
        default:
            avdtp_initiator_stream_config_subsm(connection, packet, size, offset, context);
            break;
    }
}

static void avdtp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(sdp_query_context->avdtp_cid, sdp_query_context);
    if (!connection) {
        log_error("SDP query, connection with 0x%02x cid not found", sdp_query_context->avdtp_cid);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_W4_SDP_QUERY_COMPLETE) return;

    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    uint8_t status;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id) {
                record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                // log_info("SDP Record: Nr: %d", record_id);
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
                                    case BLUETOOTH_SERVICE_CLASS_AUDIO_SOURCE:
                                        if (sdp_query_context->query_role == AVDTP_SOURCE) {
                                            sdp_query_context->role_supported = 1;
                                            break;
                                        }
                                        // log_info("SDP Attribute 0x%04x: AVDTP SOURCE protocol UUID: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet), uuid);
                                        // avdtp_remote_uuid = uuid;
                                        break;
                                    case BLUETOOTH_SERVICE_CLASS_AUDIO_SINK:
                                        if (sdp_query_context->query_role == AVDTP_SINK) {
                                            sdp_query_context->role_supported = 1;
                                            break;
                                        }
                                        // log_info("SDP Attribute 0x%04x: AVDTP SINK protocol UUID: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet), uuid);
                                        // avdtp_remote_uuid = uuid;
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST: {
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
                                    des_iterator_next(&prot_it);
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avdtp_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVDTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &sdp_query_context->avdtp_version);
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
            if (connection->state != AVDTP_SIGNALING_W4_SDP_QUERY_COMPLETE){
                // bail out, we must have had an incoming connection in the meantime;
                break;
            }
            status = sdp_event_query_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                avdtp_signaling_emit_connection_established(sdp_query_context->avdtp_callback, sdp_query_context->avdtp_cid, connection->remote_addr, status);
                btstack_linked_list_remove(&sdp_query_context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avdtp_connection_free(connection);
                log_info("SDP query failed with status 0x%02x.", status);
                break;
            } 
            if (!sdp_query_context->role_supported){
                btstack_linked_list_remove(&sdp_query_context->connections, (btstack_linked_item_t*) connection); 
                btstack_memory_avdtp_connection_free(connection);
                avdtp_signaling_emit_connection_established(sdp_query_context->avdtp_callback, sdp_query_context->avdtp_cid, connection->remote_addr, SDP_SERVICE_NOT_FOUND);
                log_info("SDP query, remote device does not support required role.");
                break;
            }
            if (!sdp_query_context->avdtp_l2cap_psm) {
                btstack_linked_list_remove(&sdp_query_context->connections, (btstack_linked_item_t*)connection); 
                btstack_memory_avdtp_connection_free(connection);
                avdtp_signaling_emit_connection_established(sdp_query_context->avdtp_callback, sdp_query_context->avdtp_cid, connection->remote_addr, L2CAP_SERVICE_DOES_NOT_EXIST);
                log_info("SDP query, no l2cap psm found.");
                break;
            }
            connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
            l2cap_create_channel(sdp_query_context->packet_handler, connection->remote_addr, sdp_query_context->avdtp_l2cap_psm, l2cap_max_mtu(), NULL);
            break;
    }
}


void avdtp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avdtp_context_t * context){
    bd_addr_t event_addr;
    uint16_t psm;
    uint16_t local_cid;
    uint8_t  status;
    int accept_signaling_connection;
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    avdtp_connection_t * connection = NULL;
    btstack_linked_list_t * avdtp_connections = &context->connections;
    btstack_linked_list_t * stream_endpoints =  &context->stream_endpoints;
    handle_media_data = context->handle_media_data;
    // log_info("avdtp_packet_handler packet type %02x, event %02x ", packet_type, hci_event_packet_get_type(packet));
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = avdtp_connection_for_l2cap_signaling_cid(channel, context);
            if (connection){
                handle_l2cap_data_packet_for_signaling_connection(connection, packet, size, context);
                break;
            }
            
            stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(channel, context);
            if (!stream_endpoint){
                if (!connection) break;
                handle_l2cap_data_packet_for_signaling_connection(connection, packet, size, context);
                break;
            }
            
            if (stream_endpoint->connection){
                if (channel == stream_endpoint->connection->l2cap_signaling_cid){
                    int offset = avdtp_read_signaling_header(&stream_endpoint->connection->signaling_packet, packet, size);
                    if (stream_endpoint->connection->signaling_packet.message_type == AVDTP_CMD_MSG){
                        avdtp_acceptor_stream_config_subsm(stream_endpoint->connection, packet, size, offset, context);
                    } else {
                        avdtp_initiator_stream_config_subsm(stream_endpoint->connection, packet, size, offset, context);
                    } 
                    break;
                }
            }

            if (channel == stream_endpoint->l2cap_media_cid){
                if (handle_media_data){
                    (*handle_media_data)(avdtp_local_seid(stream_endpoint), packet, size);
                }               
                break;
            } 

            if (channel == stream_endpoint->l2cap_reporting_cid){
                // TODO
                log_info("L2CAP_DATA_PACKET for reporting: NOT IMPLEMENTED");
            } else if (channel == stream_endpoint->l2cap_recovery_cid){
                // TODO
                log_info("L2CAP_DATA_PACKET for recovery: NOT IMPLEMENTED");
            } else {
                log_error("avdtp packet handler L2CAP_DATA_PACKET: local cid 0x%02x not found", channel);
            }
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    connection = avdtp_connection_for_bd_addr(event_addr, context);
                    log_info("L2CAP_EVENT_INCOMING_CONNECTION, local cid 0x%02x ", local_cid);
                    if (!connection){
                        // create connection struct for regular inconming connection request
                        connection = avdtp_create_connection(event_addr, context);
                        if (!connection){
                            log_error("Could not create connection, reject");
                            l2cap_decline_connection(local_cid);
                            break;                            
                        }
                        connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
                    }

                    // connection exists, check states
                    log_info("Connection state %d", connection->state);
                    accept_signaling_connection = 0;
                    switch (connection->state){
                        case AVDTP_SIGNALING_CONNECTION_IDLE:
                            // regular incoming connection
                            accept_signaling_connection = 1;
                            break;
                        case AVDTP_SIGNALING_W4_SDP_QUERY_COMPLETE:
                            // incoming connection during sdp query, just accept it
                            accept_signaling_connection = 1;
                            break;
                        case AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED:
                            log_info("Reject incoming connection after creating outgoing");
                            l2cap_decline_connection(local_cid);
                            return;
                        case AVDTP_SIGNALING_CONNECTION_OPENED:
                            // handled below
                            break;
                        case AVDTP_SIGNALING_CONNECTION_W4_L2CAP_DISCONNECTED:
                            log_info("Reject incoming connection during disconnect");
                            l2cap_decline_connection(local_cid);
                            return;
                    }

                    if (accept_signaling_connection){
                        connection->is_initiator = 0;
                        connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
                        log_info("L2CAP_EVENT_INCOMING_CONNECTION: role is_initiator %d", connection->is_initiator);
                        log_info("L2CAP_EVENT_INCOMING_CONNECTION, connection %p, state connection %d, avdtp cid 0x%02x", connection, connection->state, connection->avdtp_cid);
                        l2cap_accept_connection(local_cid);
                        break;
                    }

                    // now, we're only dealing with media connections
                    stream_endpoint = avdtp_stream_endpoint_for_seid(connection->local_seid, context);
                    if (!stream_endpoint) {
                        log_info("L2CAP_EVENT_INCOMING_CONNECTION no streamendpoint found for seid %d", connection->local_seid);
                        l2cap_decline_connection(local_cid);
                        break;
                    }

                    log_info("Checking l2cap_media_cid %d, for local seid %d, state of stream endpoint %d, role is_initiator %d", stream_endpoint->l2cap_media_cid, connection->local_seid, stream_endpoint->state, connection->is_initiator);
                    if (stream_endpoint->l2cap_media_cid == 0){
                        if (connection->is_initiator){
                            l2cap_decline_connection(local_cid);
                            break;
                        } 
                        l2cap_accept_connection(local_cid);
                        break;
                    } 
                    l2cap_decline_connection(local_cid);
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    if (psm != BLUETOOTH_PSM_AVDTP){
                        log_info("Unexpected PSM - Not implemented yet, avdtp sink: L2CAP_EVENT_CHANNEL_OPENED ");
                        return;
                    }

                    status = l2cap_event_channel_opened_get_status(packet);
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    connection = avdtp_connection_for_bd_addr(event_addr, context);
                    
                    log_info("L2CAP_EVENT_CHANNEL_OPENED: status %d, cid 0x%02x , signaling connection %p ", status, local_cid, connection);
                    connection = avdtp_connection_for_bd_addr(event_addr, context);
                    if (!connection){
                        log_info("L2CAP_EVENT_CHANNEL_OPENED 2: status %d, signaling connection %p ", status, connection);
                        break;
                    } 
                        
                    if (connection->l2cap_signaling_cid == 0) {
                        if (status){
                            log_info("L2CAP connection to %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                            connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
                            connection->l2cap_signaling_cid = 0;
                            avdtp_signaling_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, l2cap_event_channel_opened_get_status(packet));
                            break;
                        }
                        if (connection->state != AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED) {
                            log_info("L2CAP connection to %s failed. status code 0x%02x", bd_addr_to_str(event_addr), status);
                            avdtp_signaling_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, AVDTP_CONNECTION_IN_WRONG_STATE);
                            break;
                        }                    
                        connection->l2cap_signaling_cid = local_cid;
                        connection->local_seid = 0;
                        connection->state = AVDTP_SIGNALING_CONNECTION_OPENED;
                        log_info("AVDTP_SIGNALING_CONNECTION_OPENED, connection %p, l2cap_signaling_cid 0x%02x, avdtp_cid 0x%02x", connection, local_cid, connection->avdtp_cid);
                        avdtp_signaling_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, 0);
                        break;
                    }
                    
                    stream_endpoint = avdtp_stream_endpoint_for_seid(connection->local_seid, context);
                    if (!stream_endpoint){
                        log_info("L2CAP_EVENT_CHANNEL_OPENED: stream_endpoint not found");
                        return;
                    }
                    
                    if (stream_endpoint->l2cap_media_cid == 0){
                        status = l2cap_event_channel_opened_get_status(packet);
                        if (status){
                            log_info("AVDTP_STREAM_ENDPOINT_OPENED failed with status %d, avdtp cid 0x%02x, l2cap_media_cid 0x%02x, local seid %d, remote seid %d", status, connection->avdtp_cid, stream_endpoint->l2cap_media_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint));
                            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_IDLE;
                            avdtp_streaming_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), status);
                            break;
                        }
                        if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED){
                            log_info("AVDTP_STREAM_ENDPOINT_OPENED failed - stream endpoint in wrong state %d, avdtp cid 0x%02x, l2cap_media_cid 0x%02x, local seid %d, remote seid %d", stream_endpoint->state, connection->avdtp_cid, stream_endpoint->l2cap_media_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint));
                            avdtp_streaming_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE);
                            break;
                        } 

                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                        stream_endpoint->connection = connection;
                        stream_endpoint->l2cap_media_cid = l2cap_event_channel_opened_get_local_cid(packet);
                        stream_endpoint->media_con_handle = l2cap_event_channel_opened_get_handle(packet);

                        log_info("AVDTP_STREAM_ENDPOINT_OPENED, avdtp cid 0x%02x, l2cap_media_cid 0x%02x, local seid %d, remote seid %d", connection->avdtp_cid, stream_endpoint->l2cap_media_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint));
                        avdtp_streaming_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), 0);
                        return;
                    }
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = avdtp_connection_for_l2cap_signaling_cid(local_cid, context);
                    stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(local_cid, context);
                    log_info("Received L2CAP_EVENT_CHANNEL_CLOSED, cid 0x%2x, connection %p, stream_endpoint %p", local_cid, connection, stream_endpoint);

                    if (stream_endpoint){
                        if (stream_endpoint->l2cap_media_cid == local_cid){
                            connection = stream_endpoint->connection;
                            if (connection) {
                                avdtp_streaming_emit_connection_released(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint));
                            }
                            avdtp_reset_stream_endpoint(stream_endpoint);
                            if (connection && connection->disconnect){
                                avdtp_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                            }
                            break;
                        }
                        if (stream_endpoint->l2cap_recovery_cid == local_cid){
                            log_info("L2CAP_EVENT_CHANNEL_CLOSED recovery cid 0x%0x", local_cid);
                            stream_endpoint->l2cap_recovery_cid = 0;
                            break;
                        }
                        
                        if (stream_endpoint->l2cap_reporting_cid == local_cid){
                            log_info("L2CAP_EVENT_CHANNEL_CLOSED reporting cid 0x%0x", local_cid);
                            stream_endpoint->l2cap_reporting_cid = 0;
                            break;
                        }
                    }

                    if (connection){
                        btstack_linked_list_iterator_t it;    
                        btstack_linked_list_iterator_init(&it, stream_endpoints);
                        while (btstack_linked_list_iterator_has_next(&it)){
                            avdtp_stream_endpoint_t * _stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
                            if (_stream_endpoint->connection == connection){
                                avdtp_reset_stream_endpoint(_stream_endpoint);
                            }
                        }
                        btstack_run_loop_remove_timer(&connection->configuration_timer);
                        avdtp_signaling_emit_connection_released(context->avdtp_callback, connection->avdtp_cid);
                        btstack_linked_list_remove(avdtp_connections, (btstack_linked_item_t*) connection); 
                        btstack_memory_avdtp_connection_free(connection);
                        break;
                    }

                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = avdtp_connection_for_l2cap_signaling_cid(channel, context);
                    if (!connection) {
                        stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(channel, context);
                        if (!stream_endpoint->connection) break;
                        connection = stream_endpoint->connection;
                    }
                    avdtp_handle_can_send_now(connection, channel, context);
                    break;
                default:
                    log_info("Unknown HCI event type %02x", hci_event_packet_get_type(packet));
                    break;
            }
            break;
            
        default:
            // other packet type
            break;
    }
}

uint8_t avdtp_disconnect(uint16_t avdtp_cid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection) return BTSTACK_MEMORY_ALLOC_FAILED;
    if (connection->state == AVDTP_SIGNALING_CONNECTION_IDLE){
        avdtp_signaling_emit_connection_released(context->avdtp_callback, connection->avdtp_cid);
        return ERROR_CODE_SUCCESS;
    } 
    if (connection->state == AVDTP_SIGNALING_CONNECTION_W4_L2CAP_DISCONNECTED) return ERROR_CODE_SUCCESS;
    
    connection->disconnect = 1;
    avdtp_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_open_stream(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_media_connect: no connection for signaling cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }

    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) {
        log_error("avdtp_media_connect: wrong connection state %d", connection->state);
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }

    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_with_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_media_connect: no stream_endpoint with seid %d found", local_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }

    if (stream_endpoint->remote_sep.seid != remote_seid){
        log_error("avdtp_media_connect: no remote sep with seid %d registered with the stream endpoint", remote_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }
    
    if (stream_endpoint->state < AVDTP_STREAM_ENDPOINT_CONFIGURED) return AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE;
    
    connection->initiator_transaction_label++;
    connection->remote_seid = remote_seid;
    connection->local_seid = local_seid;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_OPEN_STREAM;
    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W2_REQUEST_OPEN_STREAM;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_start_stream(uint16_t avdtp_cid, uint8_t local_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_start_stream: no connection for signaling cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }

    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_with_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_start_stream: no stream_endpoint with seid %d found", local_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("avdtp_start_stream: no media connection for stream_endpoint with seid %d found", local_seid);
        return AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST;
    }

    if (!is_avdtp_remote_seid_registered(stream_endpoint)){
        log_error("avdtp_media_connect: no remote sep registered with the stream endpoint");
        return AVDTP_SEID_DOES_NOT_EXIST;
    }

    stream_endpoint->start_stream = 1;
    connection->local_seid = local_seid;
    connection->remote_seid = stream_endpoint->remote_sep.seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_stop_stream(uint16_t avdtp_cid, uint8_t local_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_stop_stream: no connection for signaling cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }

    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_with_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_stop_stream: no stream_endpoint with seid %d found", local_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("avdtp_stop_stream: no media connection for stream_endpoint with seid %d found", local_seid);
        return AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST;
    }
    
    if (!is_avdtp_remote_seid_registered(stream_endpoint) || stream_endpoint->stop_stream){
        return ERROR_CODE_SUCCESS;
    }

    stream_endpoint->stop_stream = 1;
    connection->local_seid = local_seid;
    connection->remote_seid = stream_endpoint->remote_sep.seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_abort_stream(uint16_t avdtp_cid, uint8_t local_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_abort_stream: no connection for signaling cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }

    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_with_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_abort_stream: no stream_endpoint with seid %d found", local_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("avdtp_abort_stream: no media connection for stream_endpoint with seid %d found", local_seid);
        return AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST;
    }

    if (!is_avdtp_remote_seid_registered(stream_endpoint) || stream_endpoint->abort_stream){
        return ERROR_CODE_SUCCESS;
    }
    
    stream_endpoint->abort_stream = 1;
    connection->local_seid = local_seid;
    connection->remote_seid = stream_endpoint->remote_sep.seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_suspend_stream(uint16_t avdtp_cid, uint8_t local_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_suspend_stream: no connection for signaling cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_with_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_suspend_stream: no stream_endpoint with seid %d found", local_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("avdtp_suspend_stream: no media connection for stream_endpoint with seid %d found", local_seid);
        return AVDTP_MEDIA_CONNECTION_DOES_NOT_EXIST;
    }
    
    if (!is_avdtp_remote_seid_registered(stream_endpoint) || stream_endpoint->suspend_stream){
        return ERROR_CODE_SUCCESS;
    }

    stream_endpoint->suspend_stream = 1;
    connection->local_seid = local_seid;
    connection->remote_seid = stream_endpoint->remote_sep.seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t avdtp_discover_stream_endpoints(uint16_t avdtp_cid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_discover_stream_endpoints: no connection for signaling cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED ||
        connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) {
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }
    
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS;
    return avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}


uint8_t avdtp_get_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("No connection for AVDTP cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED || 
        connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) {
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }
    
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES;
    connection->remote_seid = remote_seid;
    return avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}


uint8_t avdtp_get_all_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("No connection for AVDTP cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED || 
        connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) {
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }
    
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES;
    connection->remote_seid = remote_seid;
    return avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

uint8_t avdtp_get_configuration(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("No connection for AVDTP cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED || 
        connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) {
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }

    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION;
    connection->remote_seid = remote_seid;
    return avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

uint8_t avdtp_set_configuration(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("No connection for AVDTP cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED || 
        connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) {
        log_error("connection in wrong state, %d, initiator state %d", connection->state, connection->initiator_connection_state);
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }
    
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("No initiator stream endpoint for seid %d", local_seid);
        return AVDTP_STREAM_ENDPOINT_DOES_NOT_EXIST;
    } 
    if (stream_endpoint->state >= AVDTP_STREAM_ENDPOINT_CONFIGURED){
        log_error("Stream endpoint seid %d in wrong state %d", local_seid, stream_endpoint->state);
        return AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE;
    }
    connection->active_stream_endpoint = (void*) stream_endpoint;    
    connection->is_configuration_initiated_locally = 1;
    connection->is_initiator = 1;
    
    connection->initiator_transaction_label++;
    connection->remote_seid = remote_seid;
    connection->local_seid = local_seid;
    stream_endpoint->remote_configuration_bitmap = configured_services_bitmap;
    stream_endpoint->remote_configuration = configuration;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_SET_CONFIGURATION;
    
    // cache media codec information for SBC
    stream_endpoint->media_codec_type = configuration.media_codec.media_codec_type;
    if (configuration.media_codec.media_codec_type == AVDTP_CODEC_SBC){
        stream_endpoint->media_type = configuration.media_codec.media_type;
        memcpy(stream_endpoint->media_codec_sbc_info, configuration.media_codec.media_codec_information, 4);
    }
    return avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

uint8_t avdtp_reconfigure(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("No connection for AVDTP cid 0x%02x found", avdtp_cid);
        return AVDTP_CONNECTION_DOES_NOT_EXIST;
    }
    //TODO: if opened only app capabilities, enable reconfigure for not opened
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED || 
        connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) {
        return AVDTP_CONNECTION_IN_WRONG_STATE;
    }
   
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_reconfigure: no initiator stream endpoint for seid %d", local_seid);
        return AVDTP_STREAM_ENDPOINT_DOES_NOT_EXIST;
    }  

    if (!is_avdtp_remote_seid_registered(stream_endpoint)){
        log_error("avdtp_reconfigure: no associated remote sep");
        return AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE;
    } 

    connection->initiator_transaction_label++;
    connection->remote_seid = remote_seid;
    connection->local_seid = local_seid;
    stream_endpoint->remote_configuration_bitmap = configured_services_bitmap;
    stream_endpoint->remote_configuration = configuration;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID;
    return avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

void    avdtp_set_preferred_sampling_frequeny(avdtp_stream_endpoint_t * stream_endpoint, uint32_t sampling_frequency){
    stream_endpoint->preferred_sampling_frequency = sampling_frequency;
}

uint8_t avdtp_choose_sbc_channel_mode(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_channel_mode_bitmap){
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    uint8_t channel_mode_bitmap = (media_codec[0] & 0x0F) & remote_channel_mode_bitmap;
    
    uint8_t channel_mode = AVDTP_SBC_STEREO;
    if (channel_mode_bitmap & AVDTP_SBC_JOINT_STEREO){
        channel_mode = AVDTP_SBC_JOINT_STEREO;
    } else if (channel_mode_bitmap & AVDTP_SBC_STEREO){
        channel_mode = AVDTP_SBC_STEREO;
    } else if (channel_mode_bitmap & AVDTP_SBC_DUAL_CHANNEL){
        channel_mode = AVDTP_SBC_DUAL_CHANNEL;
    } else if (channel_mode_bitmap & AVDTP_SBC_MONO){
        channel_mode = AVDTP_SBC_MONO;
    } 
    return channel_mode;
}

uint8_t avdtp_choose_sbc_allocation_method(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_allocation_method_bitmap){
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    uint8_t allocation_method_bitmap = (media_codec[1] & 0x03) & remote_allocation_method_bitmap;
    
    uint8_t allocation_method = AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS;
    if (allocation_method_bitmap & AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS){
        allocation_method = AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS;
    } else if (allocation_method_bitmap & AVDTP_SBC_ALLOCATION_METHOD_SNR){
        allocation_method = AVDTP_SBC_ALLOCATION_METHOD_SNR;
    }
    return allocation_method;
}

uint8_t avdtp_stream_endpoint_seid(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return 0;
    return stream_endpoint->sep.seid;
}
uint8_t avdtp_choose_sbc_subbands(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_subbands_bitmap){
    if (!stream_endpoint) return 0;
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    uint8_t subbands_bitmap = ((media_codec[1] >> 2) & 0x03) & remote_subbands_bitmap;
    
    uint8_t subbands = AVDTP_SBC_SUBBANDS_8;
    if (subbands_bitmap & AVDTP_SBC_SUBBANDS_8){
        subbands = AVDTP_SBC_SUBBANDS_8;
    } else if (subbands_bitmap & AVDTP_SBC_SUBBANDS_4){
        subbands = AVDTP_SBC_SUBBANDS_4;
    }
    return subbands;
}

uint8_t avdtp_choose_sbc_block_length(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_block_length_bitmap){
    if (!stream_endpoint) return 0;
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    uint8_t block_length_bitmap = (media_codec[1] >> 4) & remote_block_length_bitmap;
    
    uint8_t block_length = AVDTP_SBC_BLOCK_LENGTH_16;
    if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_16){
        block_length = AVDTP_SBC_BLOCK_LENGTH_16;
    } else if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_12){
        block_length = AVDTP_SBC_BLOCK_LENGTH_12;
    } else if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_8){
        block_length = AVDTP_SBC_BLOCK_LENGTH_8;
    } else if (block_length_bitmap & AVDTP_SBC_BLOCK_LENGTH_4){
        block_length = AVDTP_SBC_BLOCK_LENGTH_4;
    } 
    return block_length;
}

uint8_t avdtp_choose_sbc_sampling_frequency(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_sampling_frequency_bitmap){
    if (!stream_endpoint) return 0;
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    uint8_t supported_sampling_frequency_bitmap = (media_codec[0] >> 4) & remote_sampling_frequency_bitmap;
    uint8_t sampling_frequency = AVDTP_SBC_44100;   // some default

    // use preferred sampling frequency if possible
    if        ((stream_endpoint->preferred_sampling_frequency == 48000) && (supported_sampling_frequency_bitmap & AVDTP_SBC_48000)){
        sampling_frequency = AVDTP_SBC_48000;
    } else if ((stream_endpoint->preferred_sampling_frequency == 44100) && (supported_sampling_frequency_bitmap & AVDTP_SBC_44100)){
        sampling_frequency = AVDTP_SBC_44100;
    } else if ((stream_endpoint->preferred_sampling_frequency == 32000) && (supported_sampling_frequency_bitmap & AVDTP_SBC_32000)){
        sampling_frequency = AVDTP_SBC_32000;
    } else if ((stream_endpoint->preferred_sampling_frequency == 16000) && (supported_sampling_frequency_bitmap & AVDTP_SBC_16000)){
        sampling_frequency = AVDTP_SBC_16000;
    }
    // otherwise, use highest available
    else if (supported_sampling_frequency_bitmap & AVDTP_SBC_48000){
        sampling_frequency = AVDTP_SBC_48000;
    } else if (supported_sampling_frequency_bitmap & AVDTP_SBC_44100){
        sampling_frequency = AVDTP_SBC_44100;
    } else if (supported_sampling_frequency_bitmap & AVDTP_SBC_32000){
        sampling_frequency = AVDTP_SBC_32000;
    } else if (supported_sampling_frequency_bitmap & AVDTP_SBC_16000){
        sampling_frequency = AVDTP_SBC_16000;
    } 
    return sampling_frequency;
}

uint8_t avdtp_choose_sbc_max_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_max_bitpool_value){
    if (!stream_endpoint) return 0;
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    return btstack_min(media_codec[3], remote_max_bitpool_value);
}

uint8_t avdtp_choose_sbc_min_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_min_bitpool_value){
    if (!stream_endpoint) return 0;
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    return btstack_max(media_codec[2], remote_min_bitpool_value);
}

uint8_t is_avdtp_remote_seid_registered(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint) return 0;
    if (stream_endpoint->remote_sep.seid == 0) return 0;
    if (stream_endpoint->remote_sep.seid > 0x3E) return 0;
    return 1;
}
