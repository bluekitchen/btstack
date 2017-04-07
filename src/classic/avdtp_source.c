
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

#define __BTSTACK_FILE__ "avdtp_source.c"


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"
#include "avdtp_source.h"

#include "btstack_ring_buffer.h"

static const char * default_avdtp_source_service_name = "BTstack AVDTP Source Service";
static const char * default_avdtp_source_service_provider_name = "BTstack AVDTP Source Service Provider";

static avdtp_context_t avdtp_source_context;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void a2dp_source_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, AUDIO_SOURCE_GROUP); 
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVDTP);  
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* avProtocol = de_push_sequence(attribute);
        {
            de_add_number(avProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_AVDTP);  // avProtocol_service
            de_add_number(avProtocol,  DE_UINT, DE_SIZE_16,  0x0103);  // version
        }
        de_pop_sequence(attribute, avProtocol);
    }
    de_pop_sequence(service, attribute);

    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
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
        de_add_data(service,  DE_STRING, strlen(default_avdtp_source_service_name), (uint8_t *) default_avdtp_source_service_name);
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_avdtp_source_service_provider_name), (uint8_t *) default_avdtp_source_service_provider_name);
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}


void avdtp_source_register_media_transport_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_media_transport_category(stream_endpoint);
}

void avdtp_source_register_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_reporting_category(stream_endpoint);
}

void avdtp_source_register_delay_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_delay_reporting_category(stream_endpoint);
}

void avdtp_source_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_recovery_category(stream_endpoint, maximum_recovery_window_size, maximum_number_media_packets);
}

void avdtp_source_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_content_protection_category(stream_endpoint, cp_type, cp_type_value, cp_type_value_len);
}

void avdtp_source_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_header_compression_category(stream_endpoint, back_ch, media, recovery);
}

void avdtp_source_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t * media_codec_info, uint16_t media_codec_info_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_media_codec_category(stream_endpoint, media_type, media_codec_type, media_codec_info, media_codec_info_len);
}

void avdtp_source_register_multiplexing_category(uint8_t seid, uint8_t fragmentation){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, &avdtp_source_context);
    avdtp_register_multiplexing_category(stream_endpoint, fragmentation);
}

avdtp_stream_endpoint_t * avdtp_source_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type){
    return avdtp_create_stream_endpoint(sep_type, media_type, &avdtp_source_context);
}

void avdtp_source_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avdtp_source_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_source_context.avdtp_callback = callback;
}

void avdtp_source_connect(bd_addr_t bd_addr){
    avdtp_connection_t * connection = avdtp_connection_for_bd_addr(bd_addr, &avdtp_source_context);
    if (!connection){
        connection = avdtp_create_connection(bd_addr, &avdtp_source_context);
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_IDLE) return;
    connection->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, BLUETOOTH_PROTOCOL_AVDTP, 0xffff, NULL);
}

void avdtp_source_disconnect(uint16_t con_handle){
    avdtp_disconnect(con_handle, &avdtp_source_context);
}

void avdtp_source_open_stream(uint16_t con_handle, uint8_t acp_seid){
    avdtp_open_stream(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_start_stream(uint16_t con_handle, uint8_t acp_seid){
    avdtp_start_stream(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_stop_stream(uint16_t con_handle, uint8_t acp_seid){
    avdtp_stop_stream(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_abort_stream(uint16_t con_handle, uint8_t acp_seid){
    avdtp_abort_stream(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_discover_stream_endpoints(uint16_t con_handle){
    avdtp_discover_stream_endpoints(con_handle, &avdtp_source_context);
}

void avdtp_source_get_capabilities(uint16_t con_handle, uint8_t acp_seid){
    avdtp_get_capabilities(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_get_all_capabilities(uint16_t con_handle, uint8_t acp_seid){
    avdtp_get_all_capabilities(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_get_configuration(uint16_t con_handle, uint8_t acp_seid){
    avdtp_get_configuration(con_handle, acp_seid, &avdtp_source_context);
}

void avdtp_source_set_configuration(uint16_t con_handle, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    avdtp_set_configuration(con_handle, int_seid, acp_seid, configured_services_bitmap, configuration, &avdtp_source_context);
}

void avdtp_source_reconfigure(uint16_t con_handle, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    avdtp_reconfigure(con_handle, acp_seid, configured_services_bitmap, configuration, &avdtp_source_context);
}

void avdtp_source_suspend(uint16_t con_handle, uint8_t acp_seid){
    avdtp_suspend(con_handle, acp_seid, &avdtp_source_context);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avdtp_packet_handler(packet_type, channel, packet, size, &avdtp_source_context);      
}

void avdtp_source_init(void){
    avdtp_source_context.stream_endpoints = NULL;
    avdtp_source_context.connections = NULL;
    avdtp_source_context.stream_endpoints_id_counter = 0;
    avdtp_source_context.packet_handler = packet_handler;

    l2cap_register_service(&packet_handler, BLUETOOTH_PROTOCOL_AVDTP, 0xffff, LEVEL_0);
}

int avdtp_source_streaming_endpoint_ready(uint16_t con_handle){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(con_handle, &avdtp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for 0x%02x", con_handle);
        return 0;
    }
    return (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING || stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING_W2_SEND);
}

void avdtp_source_request_can_send_now(uint16_t con_handle){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(con_handle, &avdtp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for 0x%02x", con_handle);
        return;
    }
    stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING_W2_SEND;
    avdtp_request_can_send_now_self(stream_endpoint->connection, stream_endpoint->l2cap_media_cid);
}


static void avdtp_source_setup_media_header(uint8_t * media_packet, int size, int *offset, uint8_t marker, uint16_t sequence_number){
    if (size < 12){
        printf("small outgoing buffer\n");
        return;
    }

    uint8_t  rtp_version = 2;
    uint8_t  padding = 0;
    uint8_t  extension = 0;
    uint8_t  csrc_count = 0;
    uint8_t  payload_type = 0x60;
    // uint16_t sequence_number = stream_endpoint->sequence_number;
    uint32_t timestamp = btstack_run_loop_get_time_ms();
    uint32_t ssrc = 0x11223344;

    // rtp header (min size 12B)
    int pos = 0;
    // int mtu = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);

    media_packet[pos++] = (rtp_version << 6) | (padding << 5) | (extension << 4) | csrc_count;
    media_packet[pos++] = (marker << 1) | payload_type;
    big_endian_store_16(media_packet, pos, sequence_number);
    pos += 2;
    big_endian_store_32(media_packet, pos, timestamp);
    pos += 4;
    big_endian_store_32(media_packet, pos, ssrc); // only used for multicast
    pos += 4;
    *offset = pos;
}

static void avdtp_source_copy_media_payload(uint8_t * media_packet, int size, int * offset, btstack_ring_buffer_t * sbc_ring_buffer){
    if (size < 18){
        printf("small outgoing buffer\n");
        return;
    }
    int pos = *offset;
    // media payload
    // sbc_header (size 1B)
    uint8_t sbc_header_index = pos;
    pos++;
    uint8_t fragmentation = 0;
    uint8_t starting_packet = 0; // set to 1 for the first packet of a fragmented SBC frame
    uint8_t last_packet = 0;     // set to 1 for the last packet of a fragmented SBC frame
    uint8_t num_frames = 0;

    uint32_t total_sbc_bytes_read = 0;
    uint8_t  sbc_frame_size = 0;
    // payload
    uint16_t sbc_frame_bytes = btstack_sbc_encoder_sbc_buffer_length();

    while (size - 13 - total_sbc_bytes_read >= sbc_frame_bytes && btstack_ring_buffer_bytes_available(sbc_ring_buffer)){
        uint32_t number_of_bytes_read = 0;
        btstack_ring_buffer_read(sbc_ring_buffer, &sbc_frame_size, 1, &number_of_bytes_read);
        btstack_ring_buffer_read(sbc_ring_buffer, media_packet + pos, sbc_frame_size, &number_of_bytes_read);
        pos += sbc_frame_size;
        total_sbc_bytes_read += sbc_frame_size;
        num_frames++;
        // printf("send sbc frame: timestamp %d, seq. nr %d\n", timestamp, stream_endpoint->sequence_number);
    }
    media_packet[sbc_header_index] =  (fragmentation << 7) | (starting_packet << 6) | (last_packet << 5) | num_frames;
    *offset = pos;
}

void avdtp_source_stream_send_media_payload(uint16_t l2cap_media_cid, btstack_ring_buffer_t * sbc_ring_buffer, uint8_t marker){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(l2cap_media_cid, &avdtp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for 0x%02x", l2cap_media_cid);
        return;
    }

    int size = l2cap_get_remote_mtu_for_local_cid(l2cap_media_cid);
    int offset = 0;

    l2cap_reserve_packet_buffer();
    uint8_t * media_packet = l2cap_get_outgoing_buffer();
    //int size = l2cap_get_remote_mtu_for_local_cid(l2cap_media_cid);
    avdtp_source_setup_media_header(media_packet, size, &offset, marker, stream_endpoint->sequence_number);
    avdtp_source_copy_media_payload(media_packet, size, &offset, sbc_ring_buffer);
    stream_endpoint->sequence_number++;
    l2cap_send_prepared(l2cap_media_cid, offset);
}

