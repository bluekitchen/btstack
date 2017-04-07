
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


/* streaming part */
#define NUM_CHANNELS        2
#define SAMPLE_RATE         44100
#define BYTES_PER_AUDIO_SAMPLE   (2*NUM_CHANNELS)
#define LATENCY             300 // ms

#ifndef M_PI
#define M_PI  3.14159265
#endif
#define TABLE_SIZE_441HZ   100

typedef struct {
    int16_t source[TABLE_SIZE_441HZ];
    int left_phase;
    int right_phase;
} paTestData;

static uint32_t fill_audio_ring_buffer_timeout = 50; //ms
static paTestData sin_data;

static void fill_audio_ring_buffer(void *userData, int num_samples_to_write, avdtp_stream_endpoint_t * stream_endpoint){
    paTestData *data = (paTestData*)userData;
    int count = 0;
    while (btstack_ring_buffer_bytes_free(&stream_endpoint->audio_ring_buffer) >= BYTES_PER_AUDIO_SAMPLE && count < num_samples_to_write){
        uint8_t write_data[BYTES_PER_AUDIO_SAMPLE];
        *(int16_t*)&write_data[0] = data->source[data->left_phase];
        *(int16_t*)&write_data[2] = data->source[data->right_phase];
        
        btstack_ring_buffer_write(&stream_endpoint->audio_ring_buffer, write_data, BYTES_PER_AUDIO_SAMPLE);
        count++;

        data->left_phase += 1;
        if (data->left_phase >= TABLE_SIZE_441HZ){
            data->left_phase -= TABLE_SIZE_441HZ;
        }
        data->right_phase += 1; 
        if (data->right_phase >= TABLE_SIZE_441HZ){
            data->right_phase -= TABLE_SIZE_441HZ;
        } 
    }
}

static void fill_sbc_ring_buffer(uint8_t * sbc_frame, int sbc_frame_size, avdtp_stream_endpoint_t * stream_endpoint){
    if (btstack_ring_buffer_bytes_free(&stream_endpoint->sbc_ring_buffer) >= sbc_frame_size ){
        // printf("    fill_sbc_ring_buffer\n");
        uint8_t size_buffer = sbc_frame_size;
        btstack_ring_buffer_write(&stream_endpoint->sbc_ring_buffer, &size_buffer, 1);
        btstack_ring_buffer_write(&stream_endpoint->sbc_ring_buffer, sbc_frame, sbc_frame_size);
    } else {
        printf("No space in sbc buffer\n");
    }
}


static void avdtp_source_stream_endpoint_run(avdtp_stream_endpoint_t * stream_endpoint){
    // performe sbc encoding
    int total_num_bytes_read = 0;
    int num_audio_samples_to_read = btstack_sbc_encoder_num_audio_frames();
    int audio_bytes_to_read = num_audio_samples_to_read * BYTES_PER_AUDIO_SAMPLE; 

    // printf("run: audio_bytes_to_read:        %d\n", audio_bytes_to_read);
    // printf("     audio buf, bytes available: %d\n", btstack_ring_buffer_bytes_available(&stream_endpoint->audio_ring_buffer));
    // printf("     sbc buf,   bytes free:      %d\n", btstack_ring_buffer_bytes_free(&stream_endpoint->sbc_ring_buffer));

    while (btstack_ring_buffer_bytes_available(&stream_endpoint->audio_ring_buffer) >= audio_bytes_to_read
        && btstack_ring_buffer_bytes_free(&stream_endpoint->sbc_ring_buffer) >= 120){ // TODO use real value
        
        uint32_t number_of_bytes_read = 0;
        uint8_t pcm_frame[256*BYTES_PER_AUDIO_SAMPLE];
        btstack_ring_buffer_read(&stream_endpoint->audio_ring_buffer, pcm_frame, audio_bytes_to_read, &number_of_bytes_read); 
        // printf("     num audio bytes read %d\n", number_of_bytes_read);
        btstack_sbc_encoder_process_data((int16_t *) pcm_frame);
        
        uint16_t sbc_frame_bytes = 119; //btstack_sbc_encoder_sbc_buffer_length();

        total_num_bytes_read += number_of_bytes_read;
        fill_sbc_ring_buffer(btstack_sbc_encoder_sbc_buffer(), sbc_frame_bytes, stream_endpoint);
    }
    // schedule sending
    if (total_num_bytes_read != 0){
        stream_endpoint->state = AVDTP_STREAM_ENDPOINT_STREAMING_W2_SEND;
        avdtp_request_can_send_now_self(stream_endpoint->connection, stream_endpoint->l2cap_media_cid);
    }
}

static void test_fill_audio_ring_buffer_timeout_handler(btstack_timer_source_t * timer){
    avdtp_stream_endpoint_t * stream_endpoint = btstack_run_loop_get_timer_context(timer);
    btstack_run_loop_set_timer(&stream_endpoint->fill_audio_ring_buffer_timer, fill_audio_ring_buffer_timeout); // 2 seconds timeout
    btstack_run_loop_add_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
    uint32_t now = btstack_run_loop_get_time_ms();

    uint32_t update_period_ms = fill_audio_ring_buffer_timeout;
    if (stream_endpoint->time_audio_data_sent > 0){
        update_period_ms = now - stream_endpoint->time_audio_data_sent;
    } 
    uint32_t num_samples = (update_period_ms * 44100) / 1000;
    stream_endpoint->acc_num_missed_samples += (update_period_ms * 44100) % 1000;

    if (stream_endpoint->acc_num_missed_samples >= 1000){
        num_samples++;
        stream_endpoint->acc_num_missed_samples -= 1000;
    }

    fill_audio_ring_buffer(&sin_data, num_samples, stream_endpoint);
    stream_endpoint->time_audio_data_sent = now;

    avdtp_source_stream_endpoint_run(stream_endpoint);
    // 
}

static void test_fill_audio_ring_buffer_timer_start(avdtp_stream_endpoint_t * stream_endpoint){
    btstack_run_loop_remove_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
    btstack_run_loop_set_timer_handler(&stream_endpoint->fill_audio_ring_buffer_timer, test_fill_audio_ring_buffer_timeout_handler);
    btstack_run_loop_set_timer_context(&stream_endpoint->fill_audio_ring_buffer_timer, stream_endpoint);
    btstack_run_loop_set_timer(&stream_endpoint->fill_audio_ring_buffer_timer, fill_audio_ring_buffer_timeout); // 50 ms timeout
    btstack_run_loop_add_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
}

static void test_fill_audio_ring_buffer_timer_stop(avdtp_stream_endpoint_t * stream_endpoint){
    btstack_run_loop_remove_timer(&stream_endpoint->fill_audio_ring_buffer_timer);
} 

void avdtp_source_stream_data_start(uint16_t con_handle){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(con_handle, &avdtp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for 0x%02x", con_handle);
        return;
    }
    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_STREAMING){
        printf("stream_endpoint in wrong state %d\n", stream_endpoint->state);
        return;
    } 

    test_fill_audio_ring_buffer_timer_start(stream_endpoint);
}

void avdtp_source_stream_data_stop(uint16_t con_handle){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_l2cap_cid(con_handle, &avdtp_source_context);
    if (!stream_endpoint) {
        log_error("no stream_endpoint found");
        return;
    }
    if (stream_endpoint->state != AVDTP_STREAM_ENDPOINT_STREAMING) return;
    // TODO: initialize randomly sequence number
    stream_endpoint->sequence_number = 0;
    test_fill_audio_ring_buffer_timer_stop(stream_endpoint);
}

void avdtp_source_init(void){
    avdtp_source_context.stream_endpoints = NULL;
    avdtp_source_context.connections = NULL;
    avdtp_source_context.stream_endpoints_id_counter = 0;
    avdtp_source_context.packet_handler = packet_handler;

    /* initialise sinusoidal wavetable */
    int i;
    for (i=0; i<TABLE_SIZE_441HZ; i++){
        sin_data.source[i] = sin(((double)i/(double)TABLE_SIZE_441HZ) * M_PI * 2.)*32767;
    }
    sin_data.left_phase = sin_data.right_phase = 0;

    l2cap_register_service(&packet_handler, BLUETOOTH_PROTOCOL_AVDTP, 0xffff, LEVEL_0);
}
