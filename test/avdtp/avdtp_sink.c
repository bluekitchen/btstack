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

#include "btstack_sbc.h"
#include "wav_util.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#include "btstack_ring_buffer.h"
#endif

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100

#ifdef HAVE_POSIX_FILE_IO
#define STORE_SBC_TO_SBC_FILE 
#define STORE_SBC_TO_WAV_FILE 
#endif

#ifdef HAVE_PORTAUDIO
#define PA_SAMPLE_TYPE      paInt16
#define FRAMES_PER_BUFFER   128
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)
#define PREBUFFER_MS        150
#define PREBUFFER_BYTES     (PREBUFFER_MS*SAMPLE_RATE/1000*BYTES_PER_FRAME)
static uint8_t ring_buffer_storage[2*PREBUFFER_BYTES];
static btstack_ring_buffer_t ring_buffer;

static PaStream * stream;
static uint8_t pa_stream_started = 0;

static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
    uint16_t bytes_read = 0;
    int bytes_per_buffer = framesPerBuffer * BYTES_PER_FRAME;

    if (btstack_ring_buffer_bytes_available(&ring_buffer) >= bytes_per_buffer){
        btstack_ring_buffer_read(&ring_buffer, outputBuffer, bytes_per_buffer, &bytes_read);
    } else {
        printf("NOT ENOUGH DATA!\n");
        memset(outputBuffer, 0, bytes_per_buffer);
    }
    printf("bytes avail after read: %d\n", btstack_ring_buffer_bytes_available(&ring_buffer));
    return 0;
}
#endif

#ifdef STORE_SBC_TO_WAV_FILE    
// store sbc as wav:
static btstack_sbc_decoder_state_t state;
static int total_num_samples = 0;
static int frame_count = 0;
static char * wav_filename = "avdtp_sink.wav";
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    wav_writer_write_int16(num_samples*num_channels, data);
    total_num_samples+=num_samples*num_channels;
    frame_count++;

#ifdef HAVE_PORTAUDIO
    if (!pa_stream_started && btstack_ring_buffer_bytes_available(&ring_buffer) >= PREBUFFER_BYTES){
        /* -- start stream -- */
        PaError err = Pa_StartStream(stream);
        if (err != paNoError){
            printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
        pa_stream_started = 1; 
    }
    btstack_ring_buffer_write(&ring_buffer, (uint8_t *)data, num_samples*num_channels*2);
    printf("bytes avail after write: %d\n", btstack_ring_buffer_bytes_available(&ring_buffer));
#endif
}
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
static FILE * sbc_file;
static char * sbc_filename = "avdtp_sink.sbc";
#endif


static const char * default_avdtp_sink_service_name = "BTstack AVDTP Sink Service";
static const char * default_avdtp_sink_service_provider_name = "BTstack AVDTP Sink Service Provider";

static btstack_linked_list_t avdtp_sink_connections = NULL;

static avdtp_sep_t local_seps[MAX_NUM_SEPS];
static uint8_t local_seps_num = 0;

static btstack_packet_handler_t avdtp_sink_callback;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avdtp_sink_run_for_connection(avdtp_sink_connection_t *connection);


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

uint8_t avdtp_sink_register_stream_end_point(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type){
    if (local_seps_num >= MAX_NUM_SEPS){
        log_error("avdtp_sink_register_sep: excedeed max sep number %d", MAX_NUM_SEPS);
        return 255;
    }
    uint8_t seid = local_seps_num;
    avdtp_sep_t entry = {
        seid,
        0,
        media_type,
        sep_type
    };

    local_seps[local_seps_num] = entry;
    local_seps_num++;
    return seid;
}

static int get_bit16(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
}

static uint8_t store_bit16(uint16_t bitmap, int position, uint8_t value){
    if (value){
        bitmap |= 1 << position;
    } else {
        bitmap &= ~ (1 << position);
    }
    return bitmap;
}

void avdtp_sink_register_media_transport_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_MEDIA_TRANSPORT, 1);
    local_seps[seid].registered_service_categories = bitmap;
    printf("registered services AVDTP_MEDIA_TRANSPORT(%d) %02x\n", AVDTP_MEDIA_TRANSPORT, local_seps[seid].registered_service_categories);
}

void avdtp_sink_register_reporting_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_REPORTING, 1);
    local_seps[seid].registered_service_categories = bitmap;
}

void avdtp_sink_register_delay_reporting_category(uint8_t seid){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_DELAY_REPORTING, 1);
    local_seps[seid].registered_service_categories = bitmap;
}

void avdtp_sink_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_RECOVERY, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.recovery.recovery_type = 0x01; // 0x01 = RFC2733
    local_seps[seid].capabilities.recovery.maximum_recovery_window_size = maximum_recovery_window_size;
    local_seps[seid].capabilities.recovery.maximum_number_media_packets = maximum_number_media_packets;
}

void avdtp_sink_register_content_protection_category(uint8_t seid, uint8_t cp_type_lsb,  uint8_t cp_type_msb, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_CONTENT_PROTECTION, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.content_protection.cp_type_lsb = cp_type_lsb;
    local_seps[seid].capabilities.content_protection.cp_type_msb = cp_type_msb;
    local_seps[seid].capabilities.content_protection.cp_type_value = cp_type_value;
    local_seps[seid].capabilities.content_protection.cp_type_value_len = cp_type_value_len;
}

void avdtp_sink_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_HEADER_COMPRESSION, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.header_compression.back_ch = back_ch;
    local_seps[seid].capabilities.header_compression.media = media;
    local_seps[seid].capabilities.header_compression.recovery = recovery;
}

void avdtp_sink_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t * media_codec_info, uint16_t media_codec_info_len){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_MEDIA_CODEC, 1);
    local_seps[seid].registered_service_categories = bitmap;
    printf("registered services AVDTP_MEDIA_CODEC(%d) %02x\n", AVDTP_MEDIA_CODEC, local_seps[seid].registered_service_categories);
    local_seps[seid].capabilities.media_codec.media_type = media_type;
    local_seps[seid].capabilities.media_codec.media_codec_type = media_codec_type;
    local_seps[seid].capabilities.media_codec.media_codec_information = media_codec_info;
    local_seps[seid].capabilities.media_codec.media_codec_information_len = media_codec_info_len;
}

void avdtp_sink_register_multiplexing_category(uint8_t seid, uint8_t fragmentation){
    if (seid >= local_seps_num){
        log_error("invalid stream end point identifier");
        return;
    }
    uint16_t bitmap = store_bit16(local_seps[seid].registered_service_categories, AVDTP_MULTIPLEXING, 1);
    local_seps[seid].registered_service_categories = bitmap;
    local_seps[seid].capabilities.multiplexing_mode.fragmentation = fragmentation;
}
// // media, reporting. recovery
// void avdtp_sink_register_media_transport_identifier_for_multiplexing_category(uint8_t seid, uint8_t fragmentation){

// }

static void audio_sink_generate_next_transaction_label(avdtp_sink_connection_t * connection){
    connection->local_transaction_label++;
}

static avdtp_sink_connection_t * create_avdtp_sink_connection_context(bd_addr_t bd_addr){
    avdtp_sink_connection_t * avdtp_connection = btstack_memory_avdtp_sink_connection_get();
    if (!avdtp_connection) return NULL;
    memset(avdtp_connection,0, sizeof(avdtp_sink_connection_t));
    memcpy(avdtp_connection->remote_addr, bd_addr, 6);
    avdtp_connection->local_state = AVDTP_SINK_IDLE;

    btstack_linked_list_add(&avdtp_sink_connections, (btstack_linked_item_t*)avdtp_connection);
    return avdtp_connection;
}

static avdtp_sink_connection_t * get_avdtp_sink_connection_context_for_bd_addr(bd_addr_t bd_addr){
    btstack_linked_list_iterator_t it;  
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_sink_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_sink_connection_t * connection = (avdtp_sink_connection_t *)btstack_linked_list_iterator_next(&it);
        if (memcmp(connection->remote_addr, bd_addr, 6) == 0) {
            return connection;
        }
    }
    return NULL;
}

static avdtp_sink_connection_t * get_avdtp_sink_connection_context_for_l2cap_cid(uint16_t l2cap_cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, (btstack_linked_list_t *) &avdtp_sink_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_sink_connection_t * connection = (avdtp_sink_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->l2cap_signaling_cid == l2cap_cid){
            return connection;
        }
        if (connection->l2cap_media_cid == l2cap_cid){
            return connection;
        }
        if (connection->l2cap_reporting_cid == l2cap_cid){
            return connection;
        }   
    }
    return NULL;
}

static void avdtp_sink_remove_connection_context(avdtp_sink_connection_t * connection){
    btstack_linked_list_remove(&avdtp_sink_connections, (btstack_linked_item_t*) connection);   
}

static inline uint8_t avdtp_header(uint8_t tr_label, avdtp_packet_type_t packet_type, avdtp_message_type_t msg_type){
    return (tr_label<<4) | ((uint8_t)packet_type<<2) | (uint8_t)msg_type;
}

static int avdtp_sink_send_signaling_cmd(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_sink_send_seps_response(uint16_t cid, uint8_t transaction_label, avdtp_sep_t * seps, int seps_num){
    uint8_t command[2+2*MAX_NUM_SEPS];
    int pos = 0;
    command[pos++] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[pos++] = (uint8_t)AVDTP_DISCOVER;
    int i = 0;
    for (i=0; i<seps_num; i++){
        command[pos++] = (seps[i].seid << 2) | (seps[i].in_use<<1);
        command[pos++] = (seps[i].media_type << 4) | (seps[i].type << 3);
    }
    return l2cap_send(cid, command, pos);
}

static int avdtp_sink_send_accept_response(uint16_t cid,  avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}


// static int avdtp_sink_send_configuration_reject_response(uint16_t cid, uint8_t transaction_label){
//     uint8_t command[3];
//     command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_REJECT_MSG);
//     command[1] = (uint8_t)AVDTP_SET_CONFIGURATION;
//     command[2] = 0x00;
//     // TODO: add reason for reject AVDTP 8.9
//     return l2cap_send(cid, command, sizeof(command));
// }

static int avdtp_sink_send_get_capabilities_cmd(uint16_t cid, uint8_t sep_id){
    return 0;
}

static int avdtp_pack_service_capabilities(uint8_t * buffer, int size, avdtp_capabilities_t caps, avdtp_service_category_t category){
    int i;
    // pos = 0 reserved for length
    int pos = 1;
    switch(category){
        case AVDTP_MEDIA_TRANSPORT:
        case AVDTP_REPORTING:
        case AVDTP_DELAY_REPORTING:
            break;
        case AVDTP_RECOVERY:
            buffer[pos++] = caps.recovery.recovery_type; // 0x01=RFC2733
            buffer[pos++] = caps.recovery.maximum_recovery_window_size;
            buffer[pos++] = caps.recovery.maximum_number_media_packets;
            break;
        case AVDTP_CONTENT_PROTECTION:
            buffer[pos++] = caps.content_protection.cp_type_lsb;
            buffer[pos++] = caps.content_protection.cp_type_msb;
            for (i = 0; i<caps.content_protection.cp_type_value_len; i++){
                buffer[pos++] = caps.content_protection.cp_type_value[i];
            }
            break;
        case AVDTP_HEADER_COMPRESSION:
            buffer[pos++] = (caps.header_compression.back_ch << 7) | (caps.header_compression.media << 6) | (caps.header_compression.recovery << 5);
            break;
        case AVDTP_MULTIPLEXING:
            buffer[pos++] = caps.multiplexing_mode.fragmentation << 7;
            for (i=0; i<caps.multiplexing_mode.transport_identifiers_num; i++){
                buffer[pos++] = caps.multiplexing_mode.transport_session_identifiers[i] << 7;
                buffer[pos++] = caps.multiplexing_mode.tcid[i] << 7;
                // media, reporting. recovery
            }
            break;
        case AVDTP_MEDIA_CODEC:
            buffer[pos++] = ((uint8_t)caps.media_codec.media_type) << 4;
            buffer[pos++] = (uint8_t)caps.media_codec.media_codec_type;
            for (i = 0; i<caps.media_codec.media_codec_information_len; i++){
                buffer[pos++] = caps.media_codec.media_codec_information[i];
            }
            break;
    }
    buffer[0] = pos; // length
    return pos;
}

static uint16_t avdtp_unpack_service_capabilities(avdtp_capabilities_t * caps, uint8_t * packet, uint16_t size){
    int pos = 0;
    uint16_t registered_service_categories = 0;

    avdtp_service_category_t category = (avdtp_service_category_t)packet[pos++];
    uint8_t cap_len = packet[pos++];
    int i;
    while (pos < size){
        switch(category){
            case AVDTP_MEDIA_TRANSPORT:
            case AVDTP_REPORTING:
            case AVDTP_DELAY_REPORTING:
                pos++;
                break;
            case AVDTP_RECOVERY:
                caps->recovery.recovery_type = packet[pos++];
                caps->recovery.maximum_recovery_window_size = packet[pos++];
                caps->recovery.maximum_number_media_packets = packet[pos++];
                break;
            case AVDTP_CONTENT_PROTECTION:
                caps->content_protection.cp_type_lsb = packet[pos++];
                caps->content_protection.cp_type_msb = packet[pos++];
                caps->content_protection.cp_type_value_len = cap_len - 2;
                printf_hexdump(packet+pos, caps->content_protection.cp_type_value_len);
                pos += caps->content_protection.cp_type_value_len;
                break;
            case AVDTP_HEADER_COMPRESSION:
                caps->header_compression.back_ch  = packet[pos] >> 7; 
                caps->header_compression.media    = packet[pos] >> 6;
                caps->header_compression.recovery = packet[pos] >> 5;
                pos++;
                break;
            case AVDTP_MULTIPLEXING:
                caps->multiplexing_mode.fragmentation = packet[pos++] >> 7;
                for (i=0; i<caps->multiplexing_mode.transport_identifiers_num; i++){
                    caps->multiplexing_mode.transport_session_identifiers[i] = packet[pos++] >> 7;
                    caps->multiplexing_mode.tcid[i] = packet[pos++] >> 7;
                    // media, reporting. recovery
                }
                break;
            case AVDTP_MEDIA_CODEC:
                caps->media_codec.media_type = packet[pos++] >> 4;
                caps->media_codec.media_codec_type = packet[pos++];
                caps->media_codec.media_codec_information_len = cap_len - 2;
                printf_hexdump(packet+pos, caps->media_codec.media_codec_information_len);
                pos += caps->media_codec.media_codec_information_len;
                break;
        }
        registered_service_categories = store_bit16(registered_service_categories, category, 1);
    }
    return registered_service_categories;
}

static int avdtp_sink_send_capabilities_response(uint16_t cid, uint8_t transaction_label, avdtp_sep_t sep){
    uint8_t command[100];
    int pos = 0;
    command[pos++] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_RESPONSE_ACCEPT_MSG);
    command[pos++] = (uint8_t)AVDTP_GET_CAPABILITIES;
    int i = 0;
    for (i = 1; i < 9; i++){
        if (get_bit16(sep.registered_service_categories, i)){
            // service category
            command[pos++] = i;
            pos += avdtp_pack_service_capabilities(command+pos, sizeof(command)-pos, sep.capabilities, (avdtp_service_category_t)i);
        }
    }
    return l2cap_send(cid, command, pos);
}

static void handle_l2cap_media_data_packet(avdtp_sink_connection_t * connection, uint8_t *packet, uint16_t size){
    int pos = 0;
    
    avdtp_media_packet_header_t media_header;
    media_header.version = packet[pos] & 0x03;
    media_header.padding = get_bit16(packet[pos],2);
    media_header.extension = get_bit16(packet[pos],3);
    media_header.csrc_count = (packet[pos] >> 4) & 0x0F;

    pos++;

    media_header.marker = get_bit16(packet[pos],0);
    media_header.payload_type  = (packet[pos] >> 1) & 0x7F;
    pos++;

    media_header.sequence_number = big_endian_read_16(packet, pos);
    pos+=2;

    media_header.timestamp = big_endian_read_32(packet, pos);
    pos+=4;

    media_header.synchronization_source = big_endian_read_32(packet, pos);
    pos+=4;

    // TODO: read csrc list
    
    // printf_hexdump( packet, pos );
    // printf("MEDIA HEADER: %u timestamp, version %u, padding %u, extension %u, csrc_count %u\n", 
    //     media_header.timestamp, media_header.version, media_header.padding, media_header.extension, media_header.csrc_count);
    // printf("MEDIA HEADER: marker %02x, payload_type %02x, sequence_number %u, synchronization_source %u\n", 
    //     media_header.marker, media_header.payload_type, media_header.sequence_number, media_header.synchronization_source);
    
    avdtp_sbc_codec_header_t sbc_header;
    sbc_header.fragmentation = get_bit16(packet[pos], 7);
    sbc_header.starting_packet = get_bit16(packet[pos], 6);
    sbc_header.last_packet = get_bit16(packet[pos], 5);
    sbc_header.num_frames = packet[pos] & 0x0f;
    pos++;

    // printf("SBC HEADER: num_frames %u, fragmented %u, start %u, stop %u\n", sbc_header.num_frames, sbc_header.fragmentation, sbc_header.starting_packet, sbc_header.last_packet);
    // printf_hexdump( packet+pos, size-pos );
#ifdef STORE_SBC_TO_WAV_FILE
    btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
#endif

#ifdef STORE_SBC_TO_SBC_FILE
    fwrite(packet+pos, size-pos, 1, sbc_file);
#endif
}

static void handle_l2cap_signaling_data_packet(avdtp_sink_connection_t * connection, uint8_t *packet, uint16_t size){
    if (size < 2) {
        log_error("l2cap data packet too small");
        return;
    }
    avdtp_signaling_packet_header_t signaling_header;

    signaling_header.transaction_label = packet[0] >> 4;
    signaling_header.packet_type = (avdtp_packet_type_t)((packet[0] >> 2) & 0x03);
    signaling_header.message_type = (avdtp_message_type_t) (packet[0] & 0x03);
    signaling_header.signal_identifier = packet[1] & 0x3f;

    printf("SIGNALING HEADER: tr_label %d, packet_type %d, msg_type %d, signal_identifier %02x\n", 
        signaling_header.transaction_label, signaling_header.packet_type, signaling_header.message_type, signaling_header.signal_identifier);
    
    int i = 2;
    avdtp_sep_t sep;
    
    if (signaling_header.message_type == AVDTP_CMD_MSG){
        // remote
        printf("command from remote: ");
        connection->remote_transaction_label = signaling_header.transaction_label;
        switch (signaling_header.signal_identifier){
            case AVDTP_DISCOVER:
                if (connection->remote_state != AVDTP_REMOTE_IDLE) return;
                printf("AVDTP_DISCOVER\n");
                connection->remote_state = AVDTP_REMOTE_W2_ANSWER_DISCOVER_SEPS;
                l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                break;
            case AVDTP_GET_CAPABILITIES:
                if (connection->remote_state != AVDTP_REMOTE_IDLE) return;
                printf("AVDTP_GET_CAPABILITIES\n");
                connection->local_seid = packet[2] >> 2;
                connection->remote_state = AVDTP_REMOTE_W2_ANSWER_GET_CAPABILITIES;
                l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                break;
            case AVDTP_SET_CONFIGURATION:
                if (connection->remote_state != AVDTP_REMOTE_IDLE) return;
                printf("AVDTP_SET_CONFIGURATION\n");
                connection->local_seid  = packet[2] >> 2;
                sep.seid = packet[3] >> 2;
                // find or add sep
                for (i=0; i<connection->remote_seps_num; i++){
                    if (connection->remote_seps[i].seid == sep.seid){
                        connection->remote_sep_index = i;
                    }
                }
                sep.registered_service_categories = avdtp_unpack_service_capabilities(&sep.capabilities, packet+4, size-4);
                connection->remote_seps[connection->remote_sep_index] = sep;
                connection->remote_state = AVDTP_REMOTE_W2_ANSWER_SET_CONFIGURATION;
                l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                break;
            case AVDTP_OPEN:
                if (connection->remote_state != AVDTP_REMOTE_IDLE) return;
                printf("AVDTP_OPEN\n");
                connection->local_seid  = packet[2] >> 2;
                connection->remote_state = AVDTP_REMOTE_W2_ANSWER_OPEN_STREAM;
                l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                break;
            case AVDTP_START:
                if (connection->remote_state != AVDTP_REMOTE_OPEN) return;
                printf("AVDTP_START\n");
                connection->local_seid  = packet[2] >> 2;
                connection->remote_state = AVDTP_REMOTE_W2_ANSWER_START_SINGLE_STREAM;
                l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                break;
            case AVDTP_CLOSE:          

                break;

            default:{
                printf("NOT IMPLEMENTED signal_identifier %d\n", signaling_header.signal_identifier);
                printf_hexdump( packet, size );
           #ifdef STORE_SBC_TO_WAV_FILE 
                printf(" Close wav writer\n");                   
                wav_writer_close();
                int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

                printf("\nDecoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", total_frames_nr, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
                printf("Write %d frames to wav file: %s\n\n", frame_count, wav_filename);
#endif
#ifdef STORE_SBC_TO_SBC_FILE
                fclose(sbc_file);
#endif     

#ifdef HAVE_PORTAUDIO
                PaError err = Pa_StopStream(stream);
                if (err != paNoError){
                    printf("Error stopping the stream: \"%s\"\n",  Pa_GetErrorText(err));
                    return;
                } 
                pa_stream_started = 0;
                err = Pa_CloseStream(stream);
                if (err != paNoError){
                    printf("Error closing the stream: \"%s\"\n",  Pa_GetErrorText(err));
                    return;
                } 

                err = Pa_Terminate();
                if (err != paNoError){
                    printf("Error terminating portaudio: \"%s\"\n",  Pa_GetErrorText(err));
                    return;
                } 
#endif
                break;
            }
        }
        return;
    }

    printf("send to remote: ");
    switch (connection->local_state){
        case AVDTP_SINK_W4_SEPS_DISCOVERED:
            printf("AVDTP_SINK_W4_SEPS_DISCOVERED\n");
            if (signaling_header.transaction_label != connection->local_transaction_label){
                log_error("unexpected transaction label, got %d, expected %d", signaling_header.transaction_label, connection->local_transaction_label);
                return;
            }
            if (signaling_header.signal_identifier != AVDTP_DISCOVER) {
                log_error("unexpected signal identifier ...");
                return;
            }
            if (signaling_header.message_type != AVDTP_RESPONSE_ACCEPT_MSG){
                log_error("request rejected...");
                return;   
            }
            if (size == 3){
                printf("ERROR code %02x\n", packet[2]);
                return;
            }
            for (i = 2; i<size; i+=2){
                sep.seid = packet[i] >> 2;
                if (sep.seid < 0x01 || sep.seid > 0x3E){
                    log_error("invalid sep id");
                    return;
                }
                sep.in_use = (packet[i] >> 1) & 0x01;
                sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                connection->remote_seps[connection->remote_seps_num++] = sep;
                printf("found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", 
                    sep.seid, sep.in_use, sep.media_type, sep.type);
            }
            // connection->local_state = AVDTP_SINK_W2_GET_CAPABILITIES;
            // connection->local_transaction_label++;
            // l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
            break;
        default:
            printf("NOT IMPLEMENTED\n");
            printf_hexdump( packet, size );
            break;
    }
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    hci_con_handle_t con_handle;
    uint16_t psm;
    uint16_t remote_cid;
    uint16_t local_cid;
    avdtp_sink_connection_t * connection = NULL;
        
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            connection = get_avdtp_sink_connection_context_for_l2cap_cid(channel);
            if (!connection){
                log_error("avdtp packet handler L2CAP_DATA_PACKET: connection for local cid 0x%02x not found", channel);
                break;
            }
            if (channel == connection->l2cap_signaling_cid){
                handle_l2cap_signaling_data_packet(connection, packet, size);
            } else if (channel == connection->l2cap_media_cid){
                handle_l2cap_media_data_packet(connection, packet, size);
            } else if (channel == connection->l2cap_reporting_cid){
                // TODO
            } else {
                log_error("avdtp packet handler L2CAP_DATA_PACKET: local cid 0x%02x not found", channel);
            }
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    l2cap_event_incoming_connection_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (!connection){
                        log_error("avdtp packet handler L2CAP_EVENT_INCOMING_CONNECTION: connection for bd address %s not found", bd_addr_to_str(event_addr));
                        break;
                    }

                    connection = create_avdtp_sink_connection_context(event_addr);
                    if (!connection) return;

                    con_handle = l2cap_event_incoming_connection_get_handle(packet); 
                    psm        = l2cap_event_incoming_connection_get_psm(packet); 
                    local_cid  = l2cap_event_incoming_connection_get_local_cid(packet); 
                    remote_cid = l2cap_event_incoming_connection_get_remote_cid(packet); 
                    printf("L2CAP_EVENT_INCOMING_CONNECTION %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                        bd_addr_to_str(event_addr), con_handle, psm, local_cid, remote_cid);
                    
                    l2cap_accept_connection(local_cid);
                    break;
                    
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_addr);
                    connection = get_avdtp_sink_connection_context_for_bd_addr(event_addr);
                    if (!connection){
                        log_error("avdtp sink: L2CAP_EVENT_CHANNEL_OPENED: connection for bd addr %s cannot be found", bd_addr_to_str(event_addr));
                        return;
                    }

                    if (l2cap_event_channel_opened_get_status(packet)){
                        printf("L2CAP connection to device %s failed. status code 0x%02x\n", 
                            bd_addr_to_str(event_addr), l2cap_event_channel_opened_get_status(packet));
                        break;
                    }
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    if (psm != PSM_AVDTP){
                        log_error("unexpected PSM - Not implemented yet, avdtp sink: L2CAP_EVENT_CHANNEL_OPENED");
                        return;
                    }
                    printf("L2CAP_EVENT_CHANNEL_OPENED cid %02x, context %p, local state %d, remote state %d\n",  
                        l2cap_event_channel_opened_get_local_cid(packet), connection, connection->local_state, connection->remote_state);
                    if (connection->local_state == AVDTP_SINK_W4_L2CAP_CONNECTED){
                        connection->l2cap_signaling_cid = l2cap_event_channel_opened_get_local_cid(packet); 
                        connection->local_state = AVDTP_SINK_W2_DISCOVER_SEPS;
                        audio_sink_generate_next_transaction_label(connection);
                        l2cap_request_can_send_now_event(connection->l2cap_signaling_cid);
                        printf("L2CAP_EVENT_CHANNEL_OPENED: Signaling ");
                    } else if (connection->remote_state <= AVDTP_REMOTE_W4_STREAMING_CONNECTION_OPEN){
                        connection->l2cap_media_cid = l2cap_event_channel_opened_get_local_cid(packet);
                        connection->remote_state = AVDTP_REMOTE_STREAMING;
                        // l2cap_request_can_send_now_event(connection->l2cap_media_cid);
                        printf("L2CAP_EVENT_CHANNEL_OPENED: Media ");
                    } else {
                        printf("unexpected connection state: Not implemented yet remote state %d\n", connection->remote_state);
                        return;
                    }
                
                    con_handle = l2cap_event_channel_opened_get_handle(packet);
                    printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x, context %p\n",
                           bd_addr_to_str(event_addr), con_handle, psm, connection->l2cap_signaling_cid,  l2cap_event_channel_opened_get_remote_cid(packet),
                           connection);
                    break;
                
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // data: event (8), len(8), channel (16)
                    local_cid = l2cap_event_channel_closed_get_local_cid(packet);
                    connection = get_avdtp_sink_connection_context_for_l2cap_cid(local_cid);
                    if (!connection || connection->local_state != AVDTP_SINK_W4_L2CAP_DISCONNECTED) return;

                    log_info("L2CAP_EVENT_CHANNEL_CLOSED cid 0x%0x", local_cid);
                    avdtp_sink_remove_connection_context(connection);
                    connection->local_state = AVDTP_SINK_IDLE;
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:{

#ifdef STORE_SBC_TO_WAV_FILE 
                    printf(" Close wav writer\n");                   
                    wav_writer_close();
                    int total_frames_nr = state.good_frames_nr + state.bad_frames_nr + state.zero_frames_nr;

                    printf("\nDecoding done. Processed totaly %d frames:\n - %d good\n - %d bad\n - %d zero frames\n", total_frames_nr, state.good_frames_nr, state.bad_frames_nr, state.zero_frames_nr);
                    printf("Write %d frames to wav file: %s\n\n", frame_count, wav_filename);
#endif
#ifdef STORE_SBC_TO_SBC_FILE
                    fclose(sbc_file);
#endif
                }
                default:
                    printf("unknown packet %02x\n", hci_event_packet_get_type(packet));
                    break;
            }
            break;
            
        default:
            // other packet type
            break;
    }
    avdtp_sink_run_for_connection(connection);
}

// TODO: find out which security level is needed, and replace LEVEL_0 in avdtp_sink_init
void avdtp_sink_init(void){
#ifdef STORE_SBC_TO_WAV_FILE
    btstack_sbc_decoder_init(&state, mode, &handle_pcm_data, NULL);
    wav_writer_open(wav_filename, NUM_CHANNELS, SAMPLE_RATE);  
#endif

#ifdef STORE_SBC_TO_SBC_FILE    
    sbc_file = fopen(sbc_filename, "wb"); 
#endif

#ifdef HAVE_PORTAUDIO
    PaError err;
    PaStreamParameters outputParameters;

    /* -- initialize PortAudio -- */
    err = Pa_Initialize();
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    } 
    /* -- setup input and output -- */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL,                /* &inputParameters */
           &outputParameters,
           SAMPLE_RATE,
           FRAMES_PER_BUFFER,
           paClipOff,           /* we won't output out of range samples so don't bother clipping them */
           patestCallback,      /* use callback */
           NULL );   
    
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    pa_stream_started = 0;
#endif  
    l2cap_register_service(&packet_handler, PSM_AVDTP, 0xffff, LEVEL_0);
}

void avdtp_sink_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avdtp_sink_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_sink_callback = callback;
}


static void avdtp_sink_run_for_connection(avdtp_sink_connection_t *connection){
    if (!connection) return;
    if (connection->release_l2cap_connection){
        if (connection->local_state > AVDTP_SINK_W4_L2CAP_CONNECTED || 
            connection->local_state < AVDTP_SINK_W4_L2CAP_DISCONNECTED){
            
            connection->release_l2cap_connection = 0;
            connection->local_state = AVDTP_SINK_W4_L2CAP_DISCONNECTED;
            l2cap_disconnect(connection->l2cap_signaling_cid, 0);
            return;
        }
    }

    if (!l2cap_can_send_packet_now(connection->l2cap_signaling_cid)) {
        log_info("avdtp_sink_run_for_connection: request cannot send for 0x%02x", connection->l2cap_signaling_cid);
        return;
    }
    printf("avdtp_sink_run_for_connection: \n");
    
    int sent = 1;
    switch (connection->remote_state){
        case AVDTP_REMOTE_W2_ANSWER_DISCOVER_SEPS:
            connection->remote_state = AVDTP_REMOTE_IDLE;
            avdtp_sink_send_seps_response(connection->l2cap_signaling_cid, connection->remote_transaction_label, local_seps, local_seps_num);
            break;
        case AVDTP_REMOTE_W2_ANSWER_GET_CAPABILITIES:
            connection->remote_state = AVDTP_REMOTE_IDLE;
            avdtp_sink_send_capabilities_response(connection->l2cap_signaling_cid, connection->remote_transaction_label, local_seps[connection->local_seid]);
            break;
        case AVDTP_REMOTE_W2_ANSWER_SET_CONFIGURATION:
            connection->remote_state = AVDTP_REMOTE_IDLE;
            avdtp_sink_send_accept_response(connection->l2cap_signaling_cid, AVDTP_SET_CONFIGURATION, connection->remote_transaction_label);
            break;
        case AVDTP_REMOTE_W2_ANSWER_OPEN_STREAM:
            connection->remote_state = AVDTP_REMOTE_OPEN;
            avdtp_sink_send_accept_response(connection->l2cap_signaling_cid, AVDTP_OPEN, connection->remote_transaction_label);
            break;
        case AVDTP_REMOTE_W2_ANSWER_START_SINGLE_STREAM:
            connection->remote_state = AVDTP_REMOTE_W4_STREAMING_CONNECTION_OPEN;
            avdtp_sink_send_accept_response(connection->l2cap_signaling_cid, AVDTP_START, connection->remote_transaction_label);
            break;
        default:
            sent = 0;
            break;
    }

    if (sent == 1){
        printf("answered to remote\n");
        return;  
    } 

    sent = 1;
    switch (connection->local_state){
        case AVDTP_SINK_W2_DISCOVER_SEPS:
            connection->local_state = AVDTP_SINK_W4_SEPS_DISCOVERED;
            avdtp_sink_send_signaling_cmd(connection->l2cap_signaling_cid, AVDTP_DISCOVER, connection->local_transaction_label);
            break;
        case AVDTP_SINK_W2_GET_CAPABILITIES:
            connection->local_state = AVDTP_SINK_W4_CAPABILITIES;
            avdtp_sink_send_get_capabilities_cmd(connection->l2cap_signaling_cid, 0);
            break;
        default:
            sent = 0;
            break;
    }    

    if (sent){
        printf("sent request to remote\n");
        return;  
    } 
}

void avdtp_sink_connect(bd_addr_t bd_addr){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_bd_addr(bd_addr);
    if (connection) {
        log_error("avdtp_sink_connect: connection for bd address %s not found", bd_addr_to_str(bd_addr));
        return;
    }
    connection = create_avdtp_sink_connection_context(bd_addr);
    if (!connection){
        log_error("avdtp_sink_connect: cannot create connection for bd address %s", bd_addr_to_str(bd_addr));
        return;
    }

    connection->local_state = AVDTP_SINK_W4_L2CAP_CONNECTED;
    l2cap_create_channel(packet_handler, connection->remote_addr, PSM_AVDTP, 0xffff, NULL);
}

void avdtp_sink_disconnect(uint16_t l2cap_cid){
    avdtp_sink_connection_t * connection = get_avdtp_sink_connection_context_for_l2cap_cid(l2cap_cid);
    if (!connection) {
        log_error("avdtp_sink_disconnect: Connection with l2cap_cid %d not found", l2cap_cid);
        return;
    }

    if (connection->local_state == AVDTP_SINK_IDLE) return;
    if (connection->local_state == AVDTP_SINK_W4_L2CAP_DISCONNECTED) return;
    
    connection->release_l2cap_connection = 1;
    avdtp_sink_run_for_connection(connection);
}


