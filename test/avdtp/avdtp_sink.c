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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_sink.h"
#include "avdtp_util.h"
#include "avdtp_initiator.h"
#include "avdtp_acceptor.h"

static const char * default_avdtp_sink_service_name = "BTstack AVDTP Sink Service";
static const char * default_avdtp_sink_service_provider_name = "BTstack AVDTP Sink Service Provider";

// TODO list of devices
static btstack_linked_list_t avdtp_connections;
static uint16_t stream_endpoints_id_counter;

static btstack_linked_list_t stream_endpoints;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void (*handle_media_data)(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size);    

void a2dp_sink_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, AUDIO_SINK_GROUP); 
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, SDP_L2CAPProtocol);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, PSM_AVDTP);  
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* avProtocol = de_push_sequence(attribute);
        {
            de_add_number(avProtocol,  DE_UUID, DE_SIZE_16, PSM_AVDTP);  // avProtocol_service
            de_add_number(avProtocol,  DE_UINT, DE_SIZE_16,  0x0103);  // version
        }
        de_pop_sequence(attribute, avProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, SDP_PublicBrowseGroup);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t *a2dProfile = de_push_sequence(attribute);
        {
            de_add_number(a2dProfile,  DE_UUID, DE_SIZE_16, ADVANCED_AUDIO_DISTRIBUTION); 
            de_add_number(a2dProfile,  DE_UINT, DE_SIZE_16, 0x0103); 
        }
        de_pop_sequence(attribute, a2dProfile);
    }
    de_pop_sequence(service, attribute);


    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    if (service_name){
        de_add_data(service,  DE_STRING, strlen(service_name), (uint8_t *) service_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_avdtp_sink_service_name), (uint8_t *) default_avdtp_sink_service_name);
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_avdtp_sink_service_provider_name), (uint8_t *) default_avdtp_sink_service_provider_name);
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}


static avdtp_stream_endpoint_t * get_avdtp_stream_endpoint_for_seid(uint16_t seid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &stream_endpoints);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
        if (stream_endpoint->sep.seid == seid){
            return stream_endpoint;
        }
    }
    return NULL;
}

static avdtp_connection_t * avdtp_sink_create_connection(bd_addr_t remote_addr){
    avdtp_connection_t * connection = btstack_memory_avdtp_connection_get();
    memset(connection, 0, sizeof(avdtp_connection_t));
    connection->state = AVDTP_SIGNALING_CONNECTION_IDLE;
    connection->initiator_transaction_label++;
    memcpy(connection->remote_addr, remote_addr, 6);
    btstack_linked_list_add(&avdtp_connections, (btstack_linked_item_t *) connection);
    return connection;
}

uint8_t avdtp_sink_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type){
    avdtp_stream_endpoint_t * stream_endpoint = btstack_memory_avdtp_stream_endpoint_get();
    memset(stream_endpoint, 0, sizeof(avdtp_stream_endpoint_t));
    stream_endpoints_id_counter++;
    stream_endpoint->sep.seid = stream_endpoints_id_counter;
    stream_endpoint->sep.media_type = media_type;
    stream_endpoint->sep.type = sep_type;
    btstack_linked_list_add(&stream_endpoints, (btstack_linked_item_t *) stream_endpoint);
    return stream_endpoint->sep.seid;
}

void avdtp_sink_register_media_transport_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MEDIA_TRANSPORT, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    printf("registered services AVDTP_MEDIA_TRANSPORT(%d) %02x\n", AVDTP_MEDIA_TRANSPORT, stream_endpoint->sep.registered_service_categories);
}

void avdtp_sink_register_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_REPORTING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_sink_register_delay_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_DELAY_REPORTING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
}

void avdtp_sink_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_RECOVERY, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.recovery.recovery_type = 0x01; // 0x01 = RFC2733
    stream_endpoint->sep.capabilities.recovery.maximum_recovery_window_size = maximum_recovery_window_size;
    stream_endpoint->sep.capabilities.recovery.maximum_number_media_packets = maximum_number_media_packets;
}

void avdtp_sink_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_CONTENT_PROTECTION, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.content_protection.cp_type = cp_type;
    stream_endpoint->sep.capabilities.content_protection.cp_type_value = cp_type_value;
    stream_endpoint->sep.capabilities.content_protection.cp_type_value_len = cp_type_value_len;
}

void avdtp_sink_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_HEADER_COMPRESSION, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.header_compression.back_ch = back_ch;
    stream_endpoint->sep.capabilities.header_compression.media = media;
    stream_endpoint->sep.capabilities.header_compression.recovery = recovery;
}

void avdtp_sink_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t * media_codec_info, uint16_t media_codec_info_len){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MEDIA_CODEC, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    printf("registered services AVDTP_MEDIA_CODEC(%d) %02x\n", AVDTP_MEDIA_CODEC, stream_endpoint->sep.registered_service_categories);
    stream_endpoint->sep.capabilities.media_codec.media_type = media_type;
    stream_endpoint->sep.capabilities.media_codec.media_codec_type = media_codec_type;
    stream_endpoint->sep.capabilities.media_codec.media_codec_information = media_codec_info;
    stream_endpoint->sep.capabilities.media_codec.media_codec_information_len = media_codec_info_len;
}

void avdtp_sink_register_multiplexing_category(uint8_t seid, uint8_t fragmentation){
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint){
        log_error("avdtp_sink_register_media_transport_category: stream endpoint with seid %d is not registered", seid);
        return;
    }
    uint16_t bitmap = store_bit16(stream_endpoint->sep.registered_service_categories, AVDTP_MULTIPLEXING, 1);
    stream_endpoint->sep.registered_service_categories = bitmap;
    stream_endpoint->sep.capabilities.multiplexing_mode.fragmentation = fragmentation;
}

// // media, reporting. recovery
// void avdtp_sink_register_media_transport_identifier_for_multiplexing_category(uint8_t seid, uint8_t fragmentation){

// }


static avdtp_connection_t * get_avdtp_connection_for_bd_addr(bd_addr_t addr){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_connection_t * connection = (avdtp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(addr, connection->remote_addr, 6) != 0) continue;
        return connection;
    }
    return NULL;
}

static avdtp_connection_t * get_avdtp_connection_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_connection_t * connection = (avdtp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->con_handle != con_handle) continue;
        return connection;
    }
    return NULL;
}

static avdtp_stream_endpoint_t * get_avdtp_stream_endpoint_for_active_seid(uint8_t seid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &stream_endpoints);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
        if (stream_endpoint->sep.seid == seid){
            return stream_endpoint;
        }
    }
    return NULL;
}

static avdtp_connection_t * get_avdtp_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_connection_t * connection = (avdtp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_signaling_cid != l2cap_cid) continue;
        return connection;
    }
    return NULL;
}

static avdtp_stream_endpoint_t * get_avdtp_stream_endpoint_for_l2cap_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &stream_endpoints);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
        if (stream_endpoint->l2cap_media_cid == l2cap_cid){
            return stream_endpoint;
        }
        if (stream_endpoint->l2cap_reporting_cid == l2cap_cid){
            return stream_endpoint;
        }   
        if (stream_endpoint->l2cap_recovery_cid == l2cap_cid){
            return stream_endpoint;
        }  
    }
    return NULL;
}

static avdtp_stream_endpoint_t * get_avdtp_stream_endpoint_for_connection(avdtp_connection_t * connection){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &stream_endpoints);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
        if (stream_endpoint->connection != connection) continue;
        return stream_endpoint;
    }
    return NULL;
}

// static int avdtp_acceptor_send_accept_response(uint16_t cid,  avdtp_signal_identifier_t identifier, uint8_t transaction_label){
//     uint8_t command[2];
//     command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
//     command[1] = (uint8_t)identifier;
//     return l2cap_send(cid, command, sizeof(command));
// }

static int avdtp_send_seps_response(uint16_t cid, uint8_t transaction_label, avdtp_stream_endpoint_t * endpoints){
    uint8_t command[2+2*MAX_NUM_SEPS];
    int pos = 0;
    command[pos++] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[pos++] = (uint8_t)AVDTP_SI_DISCOVER;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) endpoints);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
        command[pos++] = (stream_endpoint->sep.seid << 2) | (stream_endpoint->sep.in_use<<1);
        command[pos++] = (stream_endpoint->sep.media_type << 4) | (stream_endpoint->sep.type << 3);
    }
    return l2cap_send(cid, command, pos);
}

static int avdtp_initiator_send_signaling_cmd(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_initiator_send_signaling_cmd_with_seid(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label, uint8_t sep_id){
    uint8_t command[3];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    command[2] = sep_id << 2;
    return l2cap_send(cid, command, sizeof(command));
}


static void avdtp_sink_request_can_send_now_acceptor(avdtp_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send_acceptor = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}
static void avdtp_sink_request_can_send_now_initiator(avdtp_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send_initiator = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}
static void avdtp_sink_request_can_send_now_self(avdtp_connection_t * connection, uint16_t l2cap_cid){
    connection->wait_to_send_self = 1;
    l2cap_request_can_send_now_event(l2cap_cid);
}

/* START: tracking can send now requests pro l2cap cid */
static void avdtp_sink_handle_can_send_now(avdtp_connection_t * connection, uint16_t l2cap_cid){
    if (connection->wait_to_send_acceptor){
        connection->wait_to_send_acceptor = 0;
        avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
        if (!stream_endpoint) return;
        int sent = avdtp_acceptor_stream_config_subsm_run(connection, stream_endpoint);
        
        // check fragmentation
        if (stream_endpoint->acceptor_config_state != AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE){
            avdtp_sink_request_can_send_now_acceptor(connection, l2cap_cid);
        }
        if (sent) return;
    } else if (connection->wait_to_send_initiator){
        connection->wait_to_send_initiator = 0;
        avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
        if (!stream_endpoint) return;

        if (avdtp_initiator_stream_config_subsm_run(connection, stream_endpoint)) return;

    } else if (connection->wait_to_send_self){
        connection->wait_to_send_self = 0;

        if (connection->disconnect){
            btstack_linked_list_iterator_t it;    
            btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &stream_endpoints);
            while (btstack_linked_list_iterator_has_next(&it)){
                avdtp_stream_endpoint_t * stream_endpoint = (avdtp_stream_endpoint_t *)btstack_linked_list_iterator_next(&it);
                if (stream_endpoint->connection == connection){
                    if (stream_endpoint->state >= AVDTP_STREAM_ENDPOINT_OPENED && stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_DISCONNECTED){
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_DISCONNECTED;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
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


        switch (connection->acceptor_connection_state){
            case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS:
                printf(" -> AVDTP_SIGNALING_CONNECTION_OPENED\n");
                connection->state = AVDTP_SIGNALING_CONNECTION_OPENED;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
                avdtp_send_seps_response(connection->l2cap_signaling_cid, connection->acceptor_transaction_label, (avdtp_stream_endpoint_t *)&stream_endpoints);
                return;
            case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE:
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
                avdtp_acceptor_send_response_reject_with_error_code(connection->l2cap_signaling_cid, connection->reject_signal_identifier, connection->error_code, connection->acceptor_transaction_label);
                return;
            case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE:
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
                avdtp_acceptor_send_response_reject_service_category(connection->l2cap_signaling_cid, connection->reject_signal_identifier, connection->reject_service_category, connection->error_code, connection->acceptor_transaction_label);
                return;
            case AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_GENERAL_REJECT_WITH_ERROR_CODE:
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE;
                avdtp_acceptor_send_response_general_reject(connection->l2cap_signaling_cid, connection->reject_signal_identifier, connection->acceptor_transaction_label);
            default:
                break;
        }
        switch (connection->initiator_connection_state){
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STOPED:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STOPED\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STOPED;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_CLOSE, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_ABORTED:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_ABORTED\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_ABORTED;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_ABORT, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STARTED:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STARTED\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_STREAMING_STARTED;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_START, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_MEDIA_CONNECTED:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_MEDIA_CONNECTED\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_L2CAP_FOR_MEDIA_CONNECTED;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_OPEN, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SEPS_DISCOVERED\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SEPS_DISCOVERED;
                avdtp_initiator_send_signaling_cmd(connection->l2cap_signaling_cid, AVDTP_SI_DISCOVER, connection->initiator_transaction_label);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_CAPABILITIES;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CAPABILITIES, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_GET_ALL_CAPABILITIES\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ALL_CAPABILITIES;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_ALL_CAPABILITIES, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_GET_CONFIGURATION\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_GET_CONFIGURATION;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_GET_CONFIGURATION, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SET_CAPABILITIES:{
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SET_CAPABILITIES bitmap %02x\n",  connection->remote_capabilities_bitmap);
                // printf_hexdump(  connection->remote_capabilities.media_codec.media_codec_information,  connection->remote_capabilities.media_codec.media_codec_information_len);
                connection->signaling_packet.acp_seid = connection->query_seid;
                connection->signaling_packet.int_seid = connection->int_seid;
                connection->signaling_packet.signal_identifier = AVDTP_SI_SET_CONFIGURATION;

                avdtp_prepare_capabilities(&connection->signaling_packet, connection->initiator_transaction_label, connection->remote_capabilities_bitmap, connection->remote_capabilities, AVDTP_SI_SET_CONFIGURATION);
                l2cap_reserve_packet_buffer();
                uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->signaling_packet, out_buffer);
                if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SET_CAPABILITIES;
                    printf("    ACP: fragmented\n");
                }
                l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
                return;
            }
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SUSPEND_STREAM_WITH_SEID:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SUSPEND_STREAM_WITH_SEID\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SUSPEND_STREAM_WITH_SEID;
                avdtp_initiator_send_signaling_cmd_with_seid(connection->l2cap_signaling_cid, AVDTP_SI_SUSPEND, connection->initiator_transaction_label, connection->query_seid);
                return;
            case AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID:
                printf(" -> AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_RECONFIGURE_STREAM_WITH_SEID\n");
                connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_RECONFIGURE_STREAM_WITH_SEID;                

                connection->signaling_packet.acp_seid = connection->query_seid;
                connection->signaling_packet.int_seid = 0;
                connection->signaling_packet.signal_identifier = AVDTP_SI_RECONFIGURE;

                avdtp_prepare_capabilities(&connection->signaling_packet, connection->initiator_transaction_label, connection->remote_capabilities_bitmap, connection->remote_capabilities, AVDTP_SI_RECONFIGURE);
                l2cap_reserve_packet_buffer();
                uint8_t * out_buffer = l2cap_get_outgoing_buffer();
                uint16_t pos = avdtp_signaling_create_fragment(connection->l2cap_signaling_cid, &connection->signaling_packet, out_buffer);
                if (connection->signaling_packet.packet_type != AVDTP_SINGLE_PACKET && connection->signaling_packet.packet_type != AVDTP_END_PACKET){
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_RECONFIGURE_STREAM_WITH_SEID;
                    printf("    ACP: fragmented\n");
                }
                l2cap_send_prepared(connection->l2cap_signaling_cid, pos);
                
                return;
            default:
                break;
        }
        printf("wait_to_send_self not handled \n");
    }

    // re-register
    int more_to_send = connection->wait_to_send_acceptor || connection->wait_to_send_initiator || connection->wait_to_send_self;
    if (more_to_send){
        l2cap_request_can_send_now_event(l2cap_cid);
    }
}



/* END: tracking can send now requests pro l2cap cid */
static int handle_l2cap_data_packet_for_stream_endpoint(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size, int offset){
    avdtp_read_signaling_header(&connection->signaling_packet, packet, size);

    if (connection->signaling_packet.message_type == AVDTP_CMD_MSG){
        if (avdtp_acceptor_stream_config_subsm(connection, stream_endpoint, packet, size, offset)){
            avdtp_sink_request_can_send_now_acceptor(connection, connection->l2cap_signaling_cid);
            return 1;
        }
    } else {
        if (avdtp_initiator_stream_config_subsm(connection, stream_endpoint, packet, size)){
            avdtp_sink_request_can_send_now_initiator(connection, connection->l2cap_signaling_cid);
            return 1;
        }
    } 
    return 0;  
}

static int handle_l2cap_data_packet_for_signaling_connection(avdtp_connection_t * connection, uint8_t *packet, uint16_t size){
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    int offset = avdtp_read_signaling_header(&connection->signaling_packet, packet, size);

    switch (connection->signaling_packet.message_type){
        case AVDTP_CMD_MSG:
            if (size < 2) {
                connection->error_code = BAD_LENGTH;
                connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                return 1;
            }
            connection->acceptor_transaction_label = connection->signaling_packet.transaction_label;
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_DISCOVER:
                    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return 0;
                    connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS;
                    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                    return 1;
                case AVDTP_SI_GET_CAPABILITIES:
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                case AVDTP_SI_SET_CONFIGURATION:
                case AVDTP_SI_GET_CONFIGURATION:
                case AVDTP_SI_START:
                case AVDTP_SI_CLOSE:
                case AVDTP_SI_ABORT:
                    if (size < 3) {
                        connection->error_code = BAD_LENGTH;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }
                    connection->query_seid  = packet[offset++] >> 2;
                    stream_endpoint = get_avdtp_stream_endpoint_for_active_seid(connection->query_seid);
                    if (!stream_endpoint){
                        printf("    ACP: cmd %d - RESPONSE REJECT BAD_ACP_SEID\n", connection->signaling_packet.signal_identifier);
                        connection->error_code = BAD_ACP_SEID;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }

                    return handle_l2cap_data_packet_for_stream_endpoint(connection, stream_endpoint, packet, size, offset);
                case AVDTP_SI_RECONFIGURE:
                    if (size < 3) {
                        connection->error_code = BAD_LENGTH;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }
                    connection->query_seid  = packet[offset++] >> 2;
                    stream_endpoint = get_avdtp_stream_endpoint_for_active_seid(connection->query_seid);
                    if (!stream_endpoint){
                        printf("    ACP: AVDTP_SI_RECONFIGURE - CATEGORY RESPONSE REJECT BAD_ACP_SEID\n");
                        connection->error_code = BAD_ACP_SEID;
                        connection->reject_service_category = 0;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }
                    return handle_l2cap_data_packet_for_stream_endpoint(connection, stream_endpoint, packet, size, offset);
                
                case AVDTP_SI_OPEN:
                    if (size < 3) {
                        connection->error_code = BAD_LENGTH;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }
                    connection->query_seid  = packet[offset++] >> 2;
                    stream_endpoint = get_avdtp_stream_endpoint_for_active_seid(connection->query_seid);
                    if (!stream_endpoint){
                        printf("    ACP: AVDTP_SI_OPEN - RESPONSE REJECT BAD_STATE\n");
                        connection->error_code = BAD_STATE;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }
                    return handle_l2cap_data_packet_for_stream_endpoint(connection, stream_endpoint, packet, size, offset);
                
                case AVDTP_SI_SUSPEND:{
                    int i;
                    int request_to_send = 0;
                    printf("    ACP: AVDTP_SI_SUSPEND seids: ");
                    connection->num_suspended_seids = 0;

                    for (i = offset; i < size; i++){
                        connection->suspended_seids[connection->num_suspended_seids] = packet[i] >> 2;
                        offset++;
                        printf("%d, \n", connection->suspended_seids[connection->num_suspended_seids]);
                        connection->num_suspended_seids++;
                    }
                    
                    if (connection->num_suspended_seids == 0) {
                        printf("    ACP: CATEGORY RESPONSE REJECT BAD_ACP_SEID\n");
                        connection->error_code = BAD_ACP_SEID;
                        connection->reject_service_category = connection->query_seid;
                        connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                        connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                        avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                        return 1;
                    }
                    // deal with first susspended seid 
                    for (i = 0; i < connection->num_suspended_seids; i++){
                        connection->query_seid = connection->suspended_seids[i];
                        stream_endpoint = get_avdtp_stream_endpoint_for_active_seid(connection->query_seid);
                        if (!stream_endpoint){
                            printf("    ACP: stream_endpoint not found, CATEGORY RESPONSE REJECT BAD_ACP_SEID\n");
                            connection->error_code = BAD_ACP_SEID;
                            connection->reject_service_category = connection->query_seid;
                            connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE;
                            connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                            avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                            connection->num_suspended_seids = 0;
                            return 1;
                        }
                        request_to_send = handle_l2cap_data_packet_for_stream_endpoint(connection, stream_endpoint, packet, size, offset);
                    }
                    return request_to_send;
                }
                default:
                    connection->acceptor_connection_state = AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_GENERAL_REJECT_WITH_ERROR_CODE;
                    connection->reject_signal_identifier = connection->signaling_packet.signal_identifier;
                    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
                    printf("AVDTP_CMD_MSG signal %d not implemented, general reject\n", connection->signaling_packet.signal_identifier);
                    return 1;
            }
            break;
        case AVDTP_RESPONSE_ACCEPT_MSG:
            printf("    AVDTP_RESPONSE_ACCEPT_MSG, identifier %d\n", connection->signaling_packet.signal_identifier);
            
            switch (connection->signaling_packet.signal_identifier){
                case AVDTP_SI_DISCOVER:{
                    if (connection->signaling_packet.transaction_label != connection->initiator_transaction_label){
                        printf("    unexpected transaction label, got %d, expected %d\n", connection->signaling_packet.transaction_label, connection->initiator_transaction_label);
                        return 0;
                    }
                    
                    if (size == 3){
                        printf("    ERROR code %02x\n", packet[offset]);
                        return 0;
                    }
                    
                    avdtp_sep_t sep;
                    int i;
                    for (i = offset; i < size; i += 2){
                        sep.seid = packet[i] >> 2;
                        offset++;
                        if (sep.seid < 0x01 || sep.seid > 0x3E){
                            printf("    invalid sep id\n");
                            return 0;
                        }
                        sep.in_use = (packet[i] >> 1) & 0x01;
                        sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                        sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                        avdtp_signaling_emit_sep(avdtp_sink_callback, connection->con_handle, sep);
                    }
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    connection->initiator_transaction_label++;
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    return 0;
                }
                
                case AVDTP_SI_GET_CAPABILITIES:
                case AVDTP_SI_GET_ALL_CAPABILITIES:
                case AVDTP_SI_GET_CONFIGURATION:{
                    avdtp_sep_t sep;
                    sep.registered_service_categories = avdtp_unpack_service_capabilities(connection, &sep.capabilities, packet+offset, size-offset);
                    printf_hexdump(packet, size);

                    if (get_bit16(sep.registered_service_categories, AVDTP_MEDIA_CODEC)){
                        switch (sep.capabilities.media_codec.media_codec_type){
                            case AVDTP_CODEC_SBC: 
                                avdtp_signaling_emit_media_codec_sbc(avdtp_sink_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                            default:
                                avdtp_signaling_emit_media_codec_other(avdtp_sink_callback, connection->con_handle, sep.capabilities.media_codec);
                                break;
                        }
                    }
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    //avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    return 0;
                }
                case AVDTP_SI_SUSPEND:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint) {
                        avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                        return 0;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    return 0;
                case AVDTP_SI_SET_CONFIGURATION:{
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->int_seid);
                    if (!stream_endpoint) {
                        avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 1);
                        return 0;
                    }
                    
                    avdtp_sep_t sep;
                    sep.seid = connection->signaling_packet.command[3] >> 2;
                    sep.registered_service_categories = avdtp_unpack_service_capabilities(connection, &sep.capabilities, connection->signaling_packet.command+4, connection->signaling_packet.size-4);
                    sep.in_use = 1;

                    // find or add sep
                    int i;
                    stream_endpoint->remote_sep_index = 0xFF;
                    for (i=0; i < stream_endpoint->remote_seps_num; i++){
                        if (stream_endpoint->remote_seps[i].seid == sep.seid){
                            stream_endpoint->remote_sep_index = i;
                            break;
                        }
                    }
                    
                    if (stream_endpoint->remote_sep_index != 0xFF){
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        printf("    INT: update seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    } else {
                        // add new
                        printf("    INT: seid %d not found in %p\n", sep.seid, stream_endpoint);
                        stream_endpoint->remote_sep_index = stream_endpoint->remote_seps_num;
                        stream_endpoint->remote_seps_num++;
                        stream_endpoint->remote_seps[stream_endpoint->remote_sep_index] = sep;
                        printf("    INT: add seid %d, to %p\n", stream_endpoint->remote_seps[stream_endpoint->remote_sep_index].seid, stream_endpoint);
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CONFIGURED;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    return 0;
                }
                case AVDTP_SI_START:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint) {
                        avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                        return 0;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    break;
                case AVDTP_SI_OPEN:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint) {
                        avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                        return 0;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    return 0;

                case AVDTP_SI_CLOSE:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint) {
                        avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                        return 0;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    return 0;
                
                case AVDTP_SI_ABORT:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint) {
                        avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                        return 0;
                    }
                    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    return 0;

                case AVDTP_SI_RECONFIGURE:
                    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 0);
                    return 0;
                default:
                    avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 1);
                    printf("    AVDTP_RESPONSE_ACCEPT_MSG, signal %d not implemented\n", connection->signaling_packet.signal_identifier);
                    return 0;
                }
            break;
        case AVDTP_RESPONSE_REJECT_MSG:
            printf("    AVDTP_RESPONSE_REJECT_MSG signal %d\n", connection->signaling_packet.signal_identifier);
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
            avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 1);
            return 0;
        case AVDTP_GENERAL_REJECT_MSG:
            printf("    AVDTP_GENERAL_REJECT_MSG signal %d\n", connection->signaling_packet.signal_identifier);
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE;
            avdtp_signaling_emit_done(avdtp_sink_callback, connection->con_handle, 1);
            return 0;
    }
    
    printf(" handle_l2cap_data_packet_for_signaling_connection, signal %d not handled\n", connection->signaling_packet.signal_identifier);
    return 0;
}

static void stream_endpoint_state_machine(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint, uint8_t packet_type, uint8_t event, uint8_t *packet, uint16_t size){
    uint16_t local_cid;
    switch (packet_type){
        case L2CAP_DATA_PACKET:
            handle_l2cap_data_packet_for_stream_endpoint(connection, stream_endpoint, packet, size, 0);
            break;
        case HCI_EVENT_PACKET:
            switch (event){
                case L2CAP_EVENT_CHANNEL_OPENED:
                    if (stream_endpoint->l2cap_media_cid == 0){
                        if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED) return;
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_OPENED;
                        stream_endpoint->connection = connection;
                        stream_endpoint->l2cap_media_cid = l2cap_event_channel_opened_get_local_cid(packet);;
                        printf(" -> AVDTP_STREAM_ENDPOINT_OPENED, stream endpoint %p, connection %p\n", stream_endpoint, connection);
                        break;
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    if (stream_endpoint->l2cap_media_cid == local_cid){
                        stream_endpoint->l2cap_media_cid = 0;
                        printf(" -> AVDTP_STREAM_ENDPOINT_IDLE\n");
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_IDLE;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
                        stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
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

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    hci_con_handle_t con_handle;
    uint16_t psm;
    uint16_t local_cid;
    avdtp_stream_endpoint_t * stream_endpoint = NULL;
    avdtp_connection_t * connection = NULL;
   
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avdtp_connection_for_l2cap_signaling_cid(channel);
            if (connection){
                handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
                break;
            }
            
            stream_endpoint = get_avdtp_stream_endpoint_for_l2cap_cid(channel);
            if (!stream_endpoint){
                if (!connection) break;
                handle_l2cap_data_packet_for_signaling_connection(connection, packet, size);
                break;
            }

            if (channel == stream_endpoint->connection->l2cap_signaling_cid){
                stream_endpoint_state_machine(stream_endpoint->connection, stream_endpoint, L2CAP_DATA_PACKET, 0, packet, size);
                break;
            }

            if (channel == stream_endpoint->l2cap_media_cid){
                (*handle_media_data)(stream_endpoint, packet, size);
                break;
            } 

            if (channel == stream_endpoint->l2cap_reporting_cid){
                // TODO
                printf("L2CAP_DATA_PACKET for reporting: NOT IMPLEMENTED\n");
            } else if (channel == stream_endpoint->l2cap_recovery_cid){
                // TODO
                printf("L2CAP_DATA_PACKET for recovery: NOT IMPLEMENTED\n");
            } else {
                log_error("avdtp packet handler L2CAP_DATA_PACKET: local cid 0x%02x not found", channel);
            }
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    local_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    
                    connection = get_avdtp_connection_for_bd_addr(event_addr);
                    if (!connection){
                        connection = avdtp_sink_create_connection(event_addr);
                        connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
                        l2cap_accept_connection(local_cid);
                        break;
                    }
                    
                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint) {
                        printf("L2CAP_EVENT_INCOMING_CONNECTION no streamendpoint found for seid %d\n", connection->query_seid);
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
                    
                    if (l2cap_event_channel_opened_get_status(packet)){
                        log_error("L2CAP connection to connection %s failed. status code 0x%02x", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    if (psm != PSM_AVDTP){
                        log_error("unexpected PSM - Not implemented yet, avdtp sink: L2CAP_EVENT_CHANNEL_OPENED");
                        return;
                    }
                    
                    con_handle = l2cap_event_channel_opened_get_handle(packet);
                    local_cid = l2cap_event_channel_opened_get_local_cid(packet);

                    // printf("L2CAP_EVENT_CHANNEL_OPENED: Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                    //        bd_addr_to_str(event_addr), con_handle, psm, local_cid,  l2cap_event_channel_opened_get_remote_cid(packet));

                    if (psm != PSM_AVDTP) break;
                    
                    connection = get_avdtp_connection_for_bd_addr(event_addr);
                    if (!connection) break;

                    if (connection->l2cap_signaling_cid == 0) {
                        if (connection->state != AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED) break;
                        connection->l2cap_signaling_cid = local_cid;
                        connection->con_handle = con_handle;
                        connection->query_seid = 0;
                        connection->state = AVDTP_SIGNALING_CONNECTION_OPENED;
                        printf(" -> AVDTP_SIGNALING_CONNECTION_OPENED, connection %p\n", connection);
                        avdtp_signaling_emit_connection_established(avdtp_sink_callback, con_handle, event_addr, 0);
                        break;
                    }

                    stream_endpoint = get_avdtp_stream_endpoint_for_seid(connection->query_seid);
                    if (!stream_endpoint){
                        printf("L2CAP_EVENT_CHANNEL_OPENED: stream_endpoint not found");
                        return;
                    }
                    stream_endpoint_state_machine(connection, stream_endpoint, HCI_EVENT_PACKET, L2CAP_EVENT_CHANNEL_OPENED, packet, size);
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avdtp_connection_for_l2cap_signaling_cid(local_cid);
                    printf(" -> L2CAP_EVENT_CHANNEL_CLOSED signaling cid 0x%0x\n", local_cid);
                    
                    if (connection){
                        printf(" -> AVDTP_STREAM_ENDPOINT_IDLE, connection closed\n");
                        stream_endpoint = get_avdtp_stream_endpoint_for_connection(connection);
                        btstack_linked_list_remove(&avdtp_connections, (btstack_linked_item_t*) connection); 
                        if (!stream_endpoint) break;
                        stream_endpoint->connection = NULL;
                        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_IDLE;
                        stream_endpoint->acceptor_config_state = AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE;
                        stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
                        stream_endpoint->remote_sep_index = 0;
                        stream_endpoint->media_disconnect = 0;
                        stream_endpoint->remote_seps_num = 0;
                        memset(stream_endpoint->remote_seps, 0, sizeof(stream_endpoint->remote_seps));
                        break;
                    }

                    stream_endpoint = get_avdtp_stream_endpoint_for_l2cap_cid(local_cid);
                    if (!stream_endpoint) return;
                    
                    stream_endpoint_state_machine(connection, stream_endpoint, HCI_EVENT_PACKET, L2CAP_EVENT_CHANNEL_CLOSED, packet, size);
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;

                case L2CAP_EVENT_CAN_SEND_NOW:
                    connection = get_avdtp_connection_for_l2cap_signaling_cid(channel);
                    if (!connection) {
                        stream_endpoint = get_avdtp_stream_endpoint_for_l2cap_cid(channel);
                        if (!stream_endpoint->connection) break;
                        connection = stream_endpoint->connection;
                    }
                    avdtp_sink_handle_can_send_now(connection, channel);
                    break;
                default:
                    printf("unknown HCI event type %02x\n", hci_event_packet_get_type(packet));
                    break;
            }
            break;
            
        default:
            // other packet type
            break;
    }
}

// TODO: find out which security level is needed, and replace LEVEL_0 in avdtp_sink_init
void avdtp_sink_init(void){
    stream_endpoints = NULL;
    avdtp_connections = NULL;
    stream_endpoints_id_counter = 0;
    l2cap_register_service(&packet_handler, PSM_AVDTP, 0xffff, LEVEL_0);
}

void avdtp_sink_register_media_handler(void (*callback)(avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size)){
    if (callback == NULL){
        log_error("avdtp_sink_register_media_handler called with NULL callback");
        return;
    }
    handle_media_data = callback;
}

void avdtp_sink_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avdtp_sink_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_sink_callback = callback;
}

void avdtp_sink_connect(bd_addr_t bd_addr){
    avdtp_connection_t * connection = get_avdtp_connection_for_bd_addr(bd_addr);
    if (!connection){
        connection = avdtp_sink_create_connection(bd_addr);
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_IDLE) return;
    connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, PSM_AVDTP, 0xffff, NULL);
}

void avdtp_sink_open_stream(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_media_connect: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) {
        printf("avdtp_sink_media_connect: wrong connection state %d\n", connection->state);
        return;
    }

    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint) {
        printf("avdtp_sink_media_connect: no stream_endpoint for seid %d found\n", seid);
        return;
    }
    printf(" AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_MEDIA_CONNECTED \n");
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_MEDIA_CONNECTED;
    connection->query_seid = seid;

    if (stream_endpoint->state < AVDTP_STREAM_ENDPOINT_CONFIGURED){
        return;
    } else {
        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED;
    }
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

void avdtp_sink_start_stream(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_media_connect: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) {
        printf("avdtp_sink_media_connect: wrong connection state %d\n", connection->state);
        return;
    }

    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint) {
        printf("avdtp_sink_media_connect: no stream_endpoint for seid %d found\n", seid);
        return;
    }
    printf(" AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STARTED \n");
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STARTED;
    connection->query_seid = seid;

    if (stream_endpoint->state < AVDTP_STREAM_ENDPOINT_OPENED) return;
    
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

void avdtp_sink_stop_stream(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_stop_stream: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) {
        printf("avdtp_sink_stop_stream: wrong connection state %d\n", connection->state);
        return;
    }

    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint) {
        printf("avdtp_sink_stop_stream: no stream_endpoint for seid %d found\n", seid);
        return;
    }
   
    switch (stream_endpoint->state){
        case AVDTP_STREAM_ENDPOINT_OPENED:
        case AVDTP_STREAM_ENDPOINT_STREAMING:
            printf(" AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STOPED \n");
            connection->initiator_transaction_label++;
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_STOPED;
            connection->query_seid = seid;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_CLOSING;
            avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
            break;
        default:
            break;
    }
}

void avdtp_sink_abort_stream(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_abort_stream: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) {
        printf("avdtp_sink_abort_stream: wrong connection state %d\n", connection->state);
        return;
    }

    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint) {
        printf("avdtp_sink_abort_stream: no stream_endpoint for seid %d found\n", seid);
        return;
    }
    switch (stream_endpoint->state){
        case AVDTP_STREAM_ENDPOINT_CONFIGURED:
        case AVDTP_STREAM_ENDPOINT_CLOSING:
        case AVDTP_STREAM_ENDPOINT_OPENED:
        case AVDTP_STREAM_ENDPOINT_STREAMING:
            printf(" AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_ABORTED \n");
            connection->initiator_transaction_label++;
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_L2CAP_FOR_STREAMING_ABORTED;
            connection->query_seid = seid;
            stream_endpoint->state = AVDTP_STREAM_ENDPOINT_ABORTING;
            avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
            break;
        default:
            break;
    }
}

void avdtp_sink_disconnect(uint16_t con_handle){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (connection->state == AVDTP_SIGNALING_CONNECTION_IDLE) return;
    if (connection->state == AVDTP_SIGNALING_CONNECTION_W4_L2CAP_DISCONNECTED) return;
    
    connection->disconnect = 1;
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

void avdtp_sink_discover_stream_endpoints(uint16_t con_handle){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_discover_stream_endpoints: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;

    switch (connection->initiator_connection_state){
        case AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE:
            connection->initiator_transaction_label++;
            connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS;
            avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
            break;
        default:
            printf("avdtp_sink_discover_stream_endpoints: wrong state\n");
            break;
    }
}


void avdtp_sink_get_capabilities(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_get_capabilities: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES;
    connection->query_seid = seid;
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}


void avdtp_sink_get_all_capabilities(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_get_all_capabilities: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES;
    connection->query_seid = seid;
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

void avdtp_sink_get_configuration(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_get_configuration: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION;
    connection->query_seid = seid;
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

void avdtp_sink_set_configuration(uint16_t con_handle, uint8_t acp_seid, uint8_t int_seid, uint16_t remote_capabilities_bitmap, avdtp_capabilities_t remote_capabilities){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_set_configuration: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SET_CAPABILITIES;
    connection->initiator_transaction_label++;
    connection->query_seid = acp_seid;
    connection->int_seid = int_seid;
    connection->remote_capabilities = remote_capabilities;
    connection->remote_capabilities_bitmap = remote_capabilities_bitmap;
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

void avdtp_sink_suspend(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_suspend: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SUSPEND_STREAM_WITH_SEID;
    connection->query_seid = seid;
    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}


void avdtp_sink_reconfigure(uint16_t con_handle, uint8_t seid){
    avdtp_connection_t * connection = get_avdtp_connection_for_con_handle(con_handle);
    if (!connection){
        printf("avdtp_sink_reconfigure: no connection for handle 0x%02x found\n", con_handle);
        return;
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) return;
    if (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE) return;
    avdtp_stream_endpoint_t * stream_endpoint = get_avdtp_stream_endpoint_for_seid(seid);
    if (!stream_endpoint) return;
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID;
    connection->query_seid = seid;

    avdtp_sink_request_can_send_now_self(connection, connection->l2cap_signaling_cid);
}

