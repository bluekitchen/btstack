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

#define __BTSTACK_FILE__ "avdtp.c"


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"
#include "avdtp_acceptor.h"
#include "avdtp_initiator.h"

static int record_id = -1;
static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

typedef struct {
    avdtp_connection_t * connection;
    btstack_packet_handler_t avdtp_callback;
    avdtp_sep_type_t query_role;
    btstack_packet_handler_t packet_handler;
} avdtp_sdp_query_context_t;

static avdtp_sdp_query_context_t sdp_query_context;
static uint16_t avdtp_cid_counter = 0;

static void (*handle_media_data)(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size);
static void avdtp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint16_t avdtp_get_next_initiator_transaction_label(avdtp_context_t * context){
    context->initiator_transaction_id_counter++;
    if (context->initiator_transaction_id_counter == 0){
        context->initiator_transaction_id_counter = 1;
    }
    return context->initiator_transaction_id_counter;
}

static uint16_t avdtp_get_next_avdtp_cid(void){
    avdtp_cid_counter++;
    if (avdtp_cid_counter == 0){
        avdtp_cid_counter = 1;
    }
    return avdtp_cid_counter;
}

static uint16_t avdtp_get_next_local_seid(avdtp_context_t * context){
    context->stream_endpoints_id_counter++;
    if (context->stream_endpoints_id_counter == 0){
        context->stream_endpoints_id_counter = 1;
    }
    return context->stream_endpoints_id_counter;
}

uint8_t avdtp_connect(bd_addr_t remote, avdtp_sep_type_t query_role, avdtp_context_t * avdtp_context, uint16_t * avdtp_cid){
    sdp_query_context.connection = NULL;
    avdtp_connection_t * connection = avdtp_connection_for_bd_addr(remote, avdtp_context);
    if (!connection){
        connection = avdtp_create_connection(remote, avdtp_context);
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_IDLE){
        log_error("avdtp_connect: sink in wrong state,");
        return BTSTACK_MEMORY_ALLOC_FAILED;
    } 

    if (!avdtp_cid) return L2CAP_LOCAL_CID_DOES_NOT_EXIST;

    *avdtp_cid = connection->avdtp_cid;
    connection->state = AVDTP_SIGNALING_W4_SDP_QUERY_COMPLETE;
    sdp_query_context.connection = connection;
    sdp_query_context.query_role = query_role;
    sdp_query_context.avdtp_callback = avdtp_context->avdtp_callback;
    sdp_query_context.packet_handler = avdtp_context->packet_handler;
    
    sdp_client_query_uuid16(&avdtp_handle_sdp_client_query_result, remote, BLUETOOTH_PROTOCOL_AVDTP);
    return ERROR_CODE_SUCCESS;
}

void avdtp_register_media_transport_category(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint){
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MEDIA_TRANSPORT, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_register_reporting_category(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint){
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_REPORTING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_register_delay_reporting_category(avdtp_stream_endpoint_t * stream_endpoint){
    if (!stream_endpoint){
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_DELAY_REPORTING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_register_recovery_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    if (!stream_endpoint){
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
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
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_CONTENT_PROTECTION, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.content_protection.cp_type = cp_type;
    stream_endpoint->sep.capabilities.content_protection.cp_type_value = cp_type_value;
    stream_endpoint->sep.capabilities.content_protection.cp_type_value_len = cp_type_value_len;
}

void avdtp_register_header_compression_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t back_ch, uint8_t media, uint8_t recovery){
    if (!stream_endpoint){
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
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
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
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
        log_error("avdtp_register_media_transport_category: stream endpoint with given seid is not registered");
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
    memset(connection, 0, sizeof(avdtp_connection_t));
    connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
    connection->initiator_transaction_label = avdtp_get_next_initiator_transaction_label(context);
    connection->avdtp_cid = avdtp_get_next_avdtp_cid();
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&context->connections, (btstack_linked_item_t *) connection);
    return connection;
}

avdtp_stream_endpoint_t * avdtp_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type, avdtp_context_t * context){
    avdtp_stream_endpoint_t * stream_endpoint = btstack_memory_avdtp_stream_endpoint_get();
    memset(stream_endpoint, 0, sizeof(avdtp_stream_endpoint_t));
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

static void stream_endpoint_state_machine(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint, uint8_t packet_type, uint8_t event, uint8_t *packet, uint16_t size, avdtp_context_t * context){
    uint16_t local_cid;
    uint8_t  status;
    switch (packet_type){
        case L2CAP_DATA_PACKET:{
            int offset = avdtp_read_signaling_header(&connection->signaling_packet, packet, size);
            if (connection->signaling_packet.message_type == AVDTP_CMD_MSG){
                avdtp_acceptor_stream_config_subsm(connection, packet, size, offset, context);
            } else {
                avdtp_initiator_stream_config_subsm(connection, packet, size, offset, context);
            } 
            break;
        }
        case HCI_EVENT_PACKET:
            switch (event){
                case L2CAP_EVENT_CHANNEL_OPENED:
                    if (stream_endpoint->l2cap_media_cid == 0){
                        if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED){
                            log_info(" -> AVDTP_STREAM_ENDPOINT_OPENED failed - stream endpoint in wrong state %d, avdtp cid 0x%02x, l2cap_media_cid 0x%02x, local seid %d, remote seid %d", stream_endpoint->state, connection->avdtp_cid, stream_endpoint->l2cap_media_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint));
                            avdtp_streaming_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE);
                            break;
                        } 
                        status = l2cap_event_channel_opened_get_status(packet);
                        if (status){
                            log_info(" -> AVDTP_STREAM_ENDPOINT_OPENED failed with status %d, avdtp cid 0x%02x, l2cap_media_cid 0x%02x, local seid %d, remote seid %d", status, connection->avdtp_cid, stream_endpoint->l2cap_media_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint));
                            avdtp_streaming_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), status);
                            break;
                        }
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                        stream_endpoint->connection = connection;
                        stream_endpoint->l2cap_media_cid = l2cap_event_channel_opened_get_local_cid(packet);
                        stream_endpoint->media_con_handle = l2cap_event_channel_opened_get_handle(packet);

                        // log_info(" -> AVDTP_STREAM_ENDPOINT_OPENED, avdtp cid 0x%02x, l2cap_media_cid 0x%02x, local seid %d, remote seid %d", connection->avdtp_cid, stream_endpoint->l2cap_media_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint));
                        avdtp_streaming_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, avdtp_local_seid(stream_endpoint), avdtp_remote_seid(stream_endpoint), 0);
                        break;
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    if (stream_endpoint->l2cap_media_cid == local_cid){
                        avdtp_streaming_emit_connection_released(context->avdtp_callback, stream_endpoint->connection->avdtp_cid, avdtp_local_seid(stream_endpoint));
                        stream_endpoint->l2cap_media_cid = 0;
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_IDLE;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
                        stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
                        stream_endpoint->remote_sep_index = 0;
                        break;
                    }
                    if (stream_endpoint->l2cap_recovery_cid == local_cid){
                        log_info(" -> L2CAP_EVENT_CHANNEL_CLOSED recovery cid 0x%0x", local_cid);
                        stream_endpoint->l2cap_recovery_cid = 0;
                        break;
                    }
                    
                    if (stream_endpoint->l2cap_reporting_cid == local_cid){
                        log_info("L2CAP_EVENT_CHANNEL_CLOSED reporting cid 0x%0x", local_cid);
                        stream_endpoint->l2cap_reporting_cid = 0;
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

static void avdtp_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    uint16_t avdtp_l2cap_psm      = 0;
    uint16_t avdtp_version        = 0;
    // uint32_t avdtp_remote_uuid    = 0;
    
    if (!sdp_query_context.connection) return;

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
                                        if (sdp_query_context.query_role != AVDTP_SOURCE) {
                                            sdp_query_context.connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
                                            avdtp_signaling_emit_connection_established(sdp_query_context.avdtp_callback, sdp_query_context.connection->avdtp_cid, sdp_query_context.connection->remote_addr, SDP_SERVICE_NOT_FOUND);
                                            break;
                                        }
                                        // log_info("SDP Attribute 0x%04x: AVDTP SOURCE protocol UUID: 0x%04x", sdp_event_query_attribute_byte_get_attribute_id(packet), uuid);
                                        // avdtp_remote_uuid = uuid;
                                        break;
                                    case BLUETOOTH_SERVICE_CLASS_AUDIO_SINK:
                                        if (sdp_query_context.query_role != AVDTP_SINK) {
                                            sdp_query_context.connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
                                            avdtp_signaling_emit_connection_established(sdp_query_context.avdtp_callback, sdp_query_context.connection->avdtp_cid, sdp_query_context.connection->remote_addr, SDP_SERVICE_NOT_FOUND);
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
                                    switch (uuid){
                                        case BLUETOOTH_PROTOCOL_L2CAP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &avdtp_l2cap_psm);
                                            break;
                                        case BLUETOOTH_PROTOCOL_AVDTP:
                                            if (!des_iterator_has_more(&prot_it)) continue;
                                            des_iterator_next(&prot_it);
                                            de_element_get_uint16(des_iterator_get_element(&prot_it), &avdtp_version);
                                            break;
                                        default:
                                            break;
                                    }
                                }
                                if (!avdtp_l2cap_psm) {
                                    sdp_query_context.connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
                                    avdtp_signaling_emit_connection_established(sdp_query_context.avdtp_callback, sdp_query_context.connection->avdtp_cid, sdp_query_context.connection->remote_addr, L2CAP_SERVICE_DOES_NOT_EXIST);
                                    break;
                                }
                                sdp_query_context.connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
                                l2cap_create_channel(sdp_query_context.packet_handler, sdp_query_context.connection->remote_addr, avdtp_l2cap_psm, l2cap_max_mtu(), NULL);
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
            log_info("General query done with status %d.", sdp_event_query_complete_get_status(packet));
            break;
    }
}


void avdtp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, avdtp_context_t * context){
    bd_addr_t event_addr;
    uint16_t psm;
    uint16_t local_cid;
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
            
            if (channel == stream_endpoint->connection->l2cap_signaling_cid){
                stream_endpoint_state_machine(stream_endpoint->connection, stream_endpoint, L2CAP_DATA_PACKET, 0, packet, size, context);
                break;
            }

            if (channel == stream_endpoint->l2cap_media_cid){
                if (handle_media_data){
                    (*handle_media_data)(stream_endpoint, packet, size);
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
                    
                    if (!connection || connection->state == AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED){
                        connection = avdtp_create_connection(event_addr, context);
                        connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
                        log_info("L2CAP_EVENT_INCOMING_CONNECTION, connection %p, state connection %d", connection, connection->state);
                        l2cap_accept_connection(local_cid);
                        break;
                    }
                    
                    stream_endpoint = avdtp_stream_endpoint_for_seid(connection->local_seid, context);
                    if (!stream_endpoint) {
                        log_info("L2CAP_EVENT_INCOMING_CONNECTION no streamendpoint found for seid %d", connection->local_seid);
                        break;
                    }

                    if (stream_endpoint->l2cap_media_cid == 0){
                        if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED) break;
                        l2cap_accept_connection(local_cid);
                        break;
                    }
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);
                    if (l2cap_event_channel_opened_get_status(packet)){
                        avdtp_signaling_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, l2cap_event_channel_opened_get_status(packet));
                        log_error("L2CAP connection to connection %s failed. status code 0x%02x", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    if (psm != BLUETOOTH_PROTOCOL_AVDTP){
                        log_error("unexpected PSM - Not implemented yet, avdtp sink: L2CAP_EVENT_CHANNEL_OPENED");
                        return;
                    }
                    
                    // log_info("L2CAP_EVENT_CHANNEL_OPENED: Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x",
                           // bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_handle(packet), psm, local_cid, l2cap_event_channel_opened_get_remote_cid(packet));

                    if (psm != BLUETOOTH_PROTOCOL_AVDTP) break;
                    
                    connection = avdtp_connection_for_bd_addr(event_addr, context);
                    if (!connection) break;

                    if (connection->l2cap_signaling_cid == 0) {
                        if (connection->state != AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED) break;
                        connection->l2cap_signaling_cid = local_cid;
                        connection->local_seid = 0;
                        connection->state = AVDTP_SIGNALING_CONNECTION_OPENED;
                        log_info(" -> AVDTP_SIGNALING_CONNECTION_OPENED, connection %p, avdtp_cid 0x%02x", connection, connection->avdtp_cid);
                        avdtp_signaling_emit_connection_established(context->avdtp_callback, connection->avdtp_cid, event_addr, 0);
                        break;
                    }

                    stream_endpoint = avdtp_stream_endpoint_for_seid(connection->local_seid, context);
                    if (!stream_endpoint){
                        log_info("L2CAP_EVENT_CHANNEL_OPENED: stream_endpoint not found");
                        return;
                    }
                    stream_endpoint_state_machine(connection, stream_endpoint, HCI_EVENT_PACKET, L2CAP_EVENT_CHANNEL_OPENED, packet, size, context);
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = avdtp_connection_for_l2cap_signaling_cid(local_cid, context);
                    stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(local_cid, context);

                    if (stream_endpoint){ 
                        stream_endpoint_state_machine(connection, stream_endpoint, HCI_EVENT_PACKET, L2CAP_EVENT_CHANNEL_CLOSED, packet, size, context);
                        break;
                    }
                    
                    if (connection){
                        avdtp_signaling_emit_connection_released(context->avdtp_callback, connection->avdtp_cid);
                        btstack_linked_list_remove(avdtp_connections, (btstack_linked_item_t*) connection); 
                        btstack_linked_list_iterator_t it;    
                        btstack_linked_list_iterator_init(&it, stream_endpoints);
                        while (btstack_linked_list_iterator_has_next(&it)){
                            avdtp_stream_endpoint_t * _stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
                            
                            if (_stream_endpoint->connection == connection){
                                avdtp_initialize_stream_endpoint(_stream_endpoint);
                            }
                        }
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
                    log_info("unknown HCI event type %02x", hci_event_packet_get_type(packet));
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
    if (connection->state == AVDTP_SIGNALING_CONNECTION_IDLE) return AVDTP_CONNECTION_IN_WRONG_STATE;
    if (connection->state == AVDTP_SIGNALING_CONNECTION_W4_L2CAP_DISCONNECTED) return AVDTP_CONNECTION_IN_WRONG_STATE;
    
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
    if (avdtp_find_remote_sep(connection, remote_seid) == 0xFF){
        log_error("avdtp_media_connect: no remote sep for seid %d found", remote_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
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
    
    if (stream_endpoint->state < AVDTP_STREAM_ENDPOINT_CONFIGURED) return AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE;
    if (stream_endpoint->remote_sep_index == AVDTP_INVALID_SEP_INDEX) return AVDTP_SEID_DOES_NOT_EXIST;

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

    if (stream_endpoint->remote_sep_index == AVDTP_INVALID_SEP_INDEX || stream_endpoint->start_stream){
        return AVDTP_STREAM_ENDPOINT_IN_WRONG_STATE;
    }

    stream_endpoint->start_stream = 1;
    connection->local_seid = local_seid;
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
    if (stream_endpoint->remote_sep_index == 0xFF || stream_endpoint->stop_stream) return ERROR_CODE_SUCCESS;
    
    stream_endpoint->stop_stream = 1;
    connection->local_seid = local_seid;
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
    if (stream_endpoint->remote_sep_index == 0xFF || stream_endpoint->abort_stream) return ERROR_CODE_SUCCESS;
    
    stream_endpoint->abort_stream = 1;
    connection->local_seid = local_seid;
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
    if (stream_endpoint->remote_sep_index == 0xFF || stream_endpoint->suspend_stream) return ERROR_CODE_SUCCESS;
    
    stream_endpoint->suspend_stream = 1;
    connection->local_seid = local_seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
    return ERROR_CODE_SUCCESS;
}

void avdtp_discover_stream_endpoints(uint16_t avdtp_cid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_discover_stream_endpoints: no connection for signaling cid 0x%02x found", avdtp_cid);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;

    switch (connection->initiator_connection_state){
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE:
            connection->initiator_transaction_label++;
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS;
            avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
            break;
        default:
            log_error("avdtp_discover_stream_endpoints: wrong state");
            break;
    }
}


void avdtp_get_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_get_capabilities: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES;
    connection->remote_seid = remote_seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}


void avdtp_get_all_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_get_all_capabilities: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES;
    connection->remote_seid = remote_seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

void avdtp_get_configuration(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_get_configuration: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION;
    connection->remote_seid = remote_seid;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

void avdtp_set_configuration(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_set_configuration: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_set_configuration: no initiator stream endpoint for seid %d", local_seid);
        return;
    }        
    
    connection->initiator_transaction_label++;
    connection->remote_seid = remote_seid;
    connection->local_seid = local_seid;
    stream_endpoint->remote_configuration_bitmap = configured_services_bitmap;
    stream_endpoint->remote_configuration = configuration;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_SET_CONFIGURATION;
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

void avdtp_reconfigure(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_reconfigure: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return;
    }
    //TODO: if opened only app capabilities, enable reconfigure for not opened
    if (connection->state < AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
   
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, context);
    if (!stream_endpoint) {
        log_error("avdtp_reconfigure: no initiator stream endpoint for seid %d", local_seid);
        return;
    }  

    if (stream_endpoint->remote_sep_index == 0xFF){
        log_error("avdtp_reconfigure: no associated remote sep");
        return;
    } 

    connection->initiator_transaction_label++;
    connection->remote_seid = remote_seid;
    connection->local_seid = local_seid;
    stream_endpoint->remote_configuration_bitmap = configured_services_bitmap;
    stream_endpoint->remote_configuration = configuration;
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID;
    printf("AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID \n");
    avdtp_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
}

uint8_t avdtp_remote_seps_num(uint16_t avdtp_cid, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_suspend: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return 0;
    }
    return connection->remote_seps_num;
}

avdtp_sep_t * avdtp_remote_sep(uint16_t avdtp_cid, uint8_t index, avdtp_context_t * context){
    avdtp_connection_t * connection = avdtp_connection_for_avdtp_cid(avdtp_cid, context);
    if (!connection){
        log_error("avdtp_suspend: no connection for AVDTP cid 0x%02x found", avdtp_cid);
        return NULL;
    }
    return &connection->remote_seps[index];
}

void avdtp_initialize_sbc_configuration_storage(avdtp_stream_endpoint_t * stream_endpoint, uint8_t * config_storage, uint16_t storage_size, uint8_t * packet, uint16_t packet_size){
    UNUSED(packet_size);
    if (storage_size < 4) {
        log_error("storage must have 4 bytes");
        return;
    }
    uint8_t sampling_frequency = avdtp_choose_sbc_sampling_frequency(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet));
    uint8_t channel_mode = avdtp_choose_sbc_channel_mode(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet));
    uint8_t block_length = avdtp_choose_sbc_block_length(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet));
    uint8_t subbands = avdtp_choose_sbc_subbands(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet));
    
    uint8_t allocation_method = avdtp_choose_sbc_allocation_method(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet));
    uint8_t max_bitpool_value = avdtp_choose_sbc_max_bitpool_value(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet));
    uint8_t min_bitpool_value = avdtp_choose_sbc_min_bitpool_value(stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet));

    config_storage[0] = (sampling_frequency << 4) | channel_mode;
    config_storage[1] = (block_length << 4) | (subbands << 2) | allocation_method;
    config_storage[2] = min_bitpool_value;
    config_storage[3] = max_bitpool_value;

    stream_endpoint->remote_configuration_bitmap = store_bit16(stream_endpoint->remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
    stream_endpoint->remote_configuration.media_codec.media_type = AVDTP_AUDIO;
    stream_endpoint->remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
    stream_endpoint->remote_configuration.media_codec.media_codec_information_len = storage_size;
    stream_endpoint->remote_configuration.media_codec.media_codec_information = config_storage;
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
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    uint8_t sampling_frequency_bitmap = (media_codec[0] >> 4) & remote_sampling_frequency_bitmap;

    uint8_t sampling_frequency = AVDTP_SBC_44100;
    if (sampling_frequency_bitmap & AVDTP_SBC_48000){
        sampling_frequency = AVDTP_SBC_48000;
    } else if (sampling_frequency_bitmap & AVDTP_SBC_44100){
        sampling_frequency = AVDTP_SBC_44100;
    } else if (sampling_frequency_bitmap & AVDTP_SBC_32000){
        sampling_frequency = AVDTP_SBC_32000;
    } else if (sampling_frequency_bitmap & AVDTP_SBC_16000){
        sampling_frequency = AVDTP_SBC_16000;
    } 
    return sampling_frequency;
}

uint8_t avdtp_choose_sbc_max_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_max_bitpool_value){
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    return btstack_min(media_codec[3], remote_max_bitpool_value);
}

uint8_t avdtp_choose_sbc_min_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_min_bitpool_value){
    uint8_t * media_codec = stream_endpoint->sep.capabilities.media_codec.media_codec_information;
    return btstack_max(media_codec[2], remote_min_bitpool_value);
}
