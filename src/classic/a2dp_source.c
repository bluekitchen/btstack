
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

#define __BTSTACK_FILE__ "a2dp_source.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"
#include "avdtp_source.h"
#include "a2dp_source.h"

static const char * default_a2dp_source_service_name = "BTstack A2DP Source Service";
static const char * default_a2dp_source_service_provider_name = "BTstack A2DP Source Service Provider";
static avdtp_context_t a2dp_source_context;

typedef struct {
// to app
    uint32_t fill_audio_ring_buffer_timeout_ms;
    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t fill_audio_ring_buffer_timer;
    btstack_ring_buffer_t sbc_ring_buffer;
    btstack_sbc_encoder_state_t sbc_encoder_state;
    
    int reconfigure;
    int num_channels;
    int sampling_frequency;
    int channel_mode;
    int block_length;
    int subbands;
    int allocation_method;
    int min_bitpool_value;
    int max_bitpool_value;
    avdtp_stream_endpoint_t * local_stream_endpoint;
    avdtp_sep_t * active_remote_sep;
} avdtp_stream_endpoint_context_t;

static a2dp_state_t app_state = A2DP_IDLE;
static avdtp_stream_endpoint_context_t sc;
static uint16_t avdtp_cid = 0;
static int next_remote_sep_index_to_query = 0;

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
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AUDIO_SOURCE); 
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
            de_add_number(a2dProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_ADVANCED_AUDIO_DISTRIBUTION); 
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
        de_add_data(service,  DE_STRING, strlen(default_a2dp_source_service_name), (uint8_t *) default_a2dp_source_service_name);
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_a2dp_source_service_provider_name), (uint8_t *) default_a2dp_source_service_provider_name);
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

static void a2dp_streaming_emit_connection_established(btstack_packet_handler_t callback, uint16_t cid, uint8_t local_seid, uint8_t remote_seid, uint8_t status){
    if (!callback) return;
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_STREAM_ESTABLISHED;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = remote_seid;
    event[pos++] = status;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void a2dp_streaming_emit_can_send_media_packet_now(btstack_packet_handler_t callback, uint16_t cid, uint8_t seid){
    if (!callback) return;
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = seid;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t event_addr;
    uint8_t signal_identifier;
    uint8_t status;
    avdtp_sep_t sep;
    uint8_t int_seid;
    uint8_t acp_seid;

    switch (packet_type) {
 
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit test app
                    printf("\n --- a2dp source --- HCI_EVENT_DISCONNECTION_COMPLETE ---\n");

                    break;
                case HCI_EVENT_AVDTP_META:
                    switch (packet[2]){
                        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
                            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
                            status = avdtp_subevent_signaling_connection_established_get_status(packet);
                            if (status != 0){
                                printf(" --- a2dp source --- AVDTP_SUBEVENT_SIGNALING_CONNECTION could not be established, status %d ---\n", status);
                                break;
                            }
                            sc.active_remote_sep = NULL;
                            next_remote_sep_index_to_query = 0;
                            app_state = A2DP_W2_DISCOVER_SEPS;
                            printf(" --- a2dp source --- AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED, avdtp cid 0x%02x ---\n", avdtp_cid);
                            avdtp_source_discover_stream_endpoints(avdtp_cid);
                            break;
                        
                        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
                            status = avdtp_subevent_streaming_connection_established_get_status(packet);
                            avdtp_cid = avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet);
                            int_seid = avdtp_subevent_streaming_connection_established_get_int_seid(packet);
                            acp_seid = avdtp_subevent_streaming_connection_established_get_acp_seid(packet);

                            if (status != 0){
                                printf(" --- a2dp source --- AVDTP_SUBEVENT_STREAMING_CONNECTION could not be established, status %d ---\n", status);
                                break;
                            }
                            app_state = A2DP_STREAMING_OPENED;
                            a2dp_streaming_emit_connection_established(a2dp_source_context.a2dp_callback, avdtp_cid, int_seid, acp_seid, 0);
                            printf(" --- a2dp source --- AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED --- avdtp_cid 0x%02x, local seid %d, remote seid %d\n", avdtp_cid, int_seid, acp_seid);
                            break;

                        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
                            if (app_state != A2DP_W2_DISCOVER_SEPS) return;
                            sep.seid = avdtp_subevent_signaling_sep_found_get_seid(packet);
                            sep.in_use = avdtp_subevent_signaling_sep_found_get_in_use(packet);
                            sep.media_type = avdtp_subevent_signaling_sep_found_get_media_type(packet);
                            sep.type = avdtp_subevent_signaling_sep_found_get_sep_type(packet);
                            printf(" --- a2dp source --- Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", sep.seid, sep.in_use, sep.media_type, sep.type);
                            break;

                        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:{
                            printf(" AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY 0\n");
                            if (!sc.local_stream_endpoint) return;
                            printf(" AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY 1\n");
                            uint8_t sampling_frequency = avdtp_choose_sbc_sampling_frequency(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet));
                            uint8_t channel_mode = avdtp_choose_sbc_channel_mode(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet));
                            uint8_t block_length = avdtp_choose_sbc_block_length(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet));
                            uint8_t subbands = avdtp_choose_sbc_subbands(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet));
                            
                            uint8_t allocation_method = avdtp_choose_sbc_allocation_method(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet));
                            uint8_t max_bitpool_value = avdtp_choose_sbc_max_bitpool_value(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet));
                            uint8_t min_bitpool_value = avdtp_choose_sbc_min_bitpool_value(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet));

                            sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information[0] = (sampling_frequency << 4) | channel_mode;
                            sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information[1] = (block_length << 4) | (subbands << 2) | allocation_method;
                            sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information[2] = min_bitpool_value;
                            sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information[3] = max_bitpool_value;

                            sc.local_stream_endpoint->remote_configuration_bitmap = store_bit16(sc.local_stream_endpoint->remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
                            sc.local_stream_endpoint->remote_configuration.media_codec.media_type = AVDTP_AUDIO;
                            sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;

                            app_state = A2DP_W2_SET_CONFIGURATION;
                            break;
                        }
                        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
                            printf(" --- a2dp source ---  received non SBC codec. not implemented\n");
                            break;
                        
                        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
                            sc.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
                            sc.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
                            sc.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
                            // switch (avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet)){
                            //     case AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS:
                            //         sc.allocation_method = SBC_LOUDNESS;
                            //         break;
                            //     case AVDTP_SBC_ALLOCATION_METHOD_SNR:
                            //         sc.allocation_method = SBC_SNR;
                            //         break;
                            // }
                            sc.allocation_method = avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet) - 1;
                            sc.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
                            // TODO: deal with reconfigure: avdtp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
                            break;
                        }  
                        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW: 
                            a2dp_streaming_emit_can_send_media_packet_now(a2dp_source_context.a2dp_callback, avdtp_cid, 0);
                            break;
                        
                        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
                            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
                            status = avdtp_subevent_signaling_accept_get_status(packet);
                            printf(" --- a2dp source ---  Accepted %d\n", signal_identifier);
                            
                            switch (app_state){
                                case A2DP_W2_DISCOVER_SEPS:
                                    app_state = A2DP_W2_GET_ALL_CAPABILITIES;
                                    sc.active_remote_sep = avdtp_source_remote_sep(avdtp_cid, next_remote_sep_index_to_query++);
                                    printf(" --- a2dp source ---  Query get caps for seid %d\n", sc.active_remote_sep->seid);
                                    avdtp_source_get_capabilities(avdtp_cid, sc.active_remote_sep->seid);
                                    break;
                                case A2DP_W2_GET_CAPABILITIES:
                                case A2DP_W2_GET_ALL_CAPABILITIES:
                                    if (next_remote_sep_index_to_query < avdtp_source_remote_seps_num(avdtp_cid)){
                                        sc.active_remote_sep = avdtp_source_remote_sep(avdtp_cid, next_remote_sep_index_to_query++);
                                        printf(" --- a2dp source ---  Query get caps for seid %d\n", sc.active_remote_sep->seid);
                                        avdtp_source_get_capabilities(avdtp_cid, sc.active_remote_sep->seid);
                                    } else {
                                        printf(" --- a2dp source ---  No more remote seps found\n");
                                        app_state = A2DP_IDLE;
                                    }
                                    break;
                                case A2DP_W2_SET_CONFIGURATION:{
                                    if (!sc.local_stream_endpoint) return;
                                    app_state = A2DP_W2_GET_CONFIGURATION;
                                    avdtp_source_set_configuration(avdtp_cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.active_remote_sep->seid, sc.local_stream_endpoint->remote_configuration_bitmap, sc.local_stream_endpoint->remote_configuration);
                                    break;
                                }
                                case A2DP_W2_GET_CONFIGURATION:
                                    app_state = A2DP_W2_OPEN_STREAM_WITH_SEID;
                                    avdtp_source_get_configuration(avdtp_cid, sc.active_remote_sep->seid);
                                    break;
                                case A2DP_W2_OPEN_STREAM_WITH_SEID:{
                                    app_state = A2DP_W4_OPEN_STREAM_WITH_SEID;
                                    btstack_sbc_encoder_init(&sc.sbc_encoder_state, SBC_MODE_STANDARD, 
                                        sc.block_length, sc.subbands, 
                                        sc.allocation_method, sc.sampling_frequency, 
                                        sc.max_bitpool_value);
                                    avdtp_source_open_stream(avdtp_cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.active_remote_sep->seid);
                                    break;
                                }
                                case A2DP_STREAMING_OPENED:
                                    if (!a2dp_source_context.a2dp_callback) return;
                                    switch (signal_identifier){
                                        case  AVDTP_SI_START:{
                                            uint8_t event[6];
                                            int pos = 0;
                                            event[pos++] = HCI_EVENT_A2DP_META;
                                            event[pos++] = sizeof(event) - 2;
                                            event[pos++] = A2DP_SUBEVENT_STREAM_START_ACCEPTED;
                                            little_endian_store_16(event, pos, avdtp_cid);
                                            pos += 2;
                                            event[pos++] = avdtp_stream_endpoint_seid(sc.local_stream_endpoint);
                                            (*a2dp_source_context.a2dp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                                            break;
                                        }
                                        case AVDTP_SI_CLOSE:{
                                            uint8_t event[6];
                                            int pos = 0;
                                            event[pos++] = HCI_EVENT_A2DP_META;
                                            event[pos++] = sizeof(event) - 2;
                                            event[pos++] = A2DP_SUBEVENT_STREAM_RELEASED;
                                            little_endian_store_16(event, pos, avdtp_cid);
                                            pos += 2;
                                            printf("send A2DP_SUBEVENT_STREAM_RELEASED to app\n");
                                            event[pos++] = avdtp_stream_endpoint_seid(sc.local_stream_endpoint);
                                            (*a2dp_source_context.a2dp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                                            break;
                                        }
                                        case AVDTP_SI_SUSPEND:{
                                            uint8_t event[6];
                                            int pos = 0;
                                            event[pos++] = HCI_EVENT_A2DP_META;
                                            event[pos++] = sizeof(event) - 2;
                                            event[pos++] = A2DP_SUBEVENT_STREAM_SUSPENDED;
                                            little_endian_store_16(event, pos, avdtp_cid);
                                            pos += 2;
                                            event[pos++] = avdtp_stream_endpoint_seid(sc.local_stream_endpoint);
                                            (*a2dp_source_context.a2dp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
                                            break;
                                        }
                                        default:
                                            break;
                                    }
                                    break;
                                default:
                                    app_state = A2DP_IDLE;
                                    break;
                            }
                            
                            break;
                        case AVDTP_SUBEVENT_SIGNALING_REJECT:
                            app_state = A2DP_IDLE;
                            signal_identifier = avdtp_subevent_signaling_reject_get_signal_identifier(packet);
                            printf(" --- a2dp source ---  Rejected %d\n", signal_identifier);
                            break;
                        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
                            app_state = A2DP_IDLE;
                            signal_identifier = avdtp_subevent_signaling_general_reject_get_signal_identifier(packet);
                            printf(" --- a2dp source ---  Rejected %d\n", signal_identifier);
                            break;
                        default:
                            app_state = A2DP_IDLE;
                            printf(" --- a2dp source ---  not implemented\n");
                            break; 
                    }
                    break;   
                default:
                    break;
            }
            break;
        default:
            // other packet type
            break;
    }
}
void a2dp_source_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("a2dp_source_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_source_register_packet_handler(&packet_handler);   
    a2dp_source_context.a2dp_callback = callback;
}

void a2dp_source_init(void){
    avdtp_source_init(&a2dp_source_context);
    l2cap_register_service(&packet_handler, BLUETOOTH_PROTOCOL_AVDTP, 0xffff, LEVEL_0);
}

uint8_t a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, 
    uint8_t * codec_capabilities, uint16_t codec_capabilities_len,
    uint8_t * media_codec_info, uint16_t media_codec_info_len){
    avdtp_stream_endpoint_t * local_stream_endpoint = avdtp_source_create_stream_endpoint(AVDTP_SOURCE, media_type);
    avdtp_source_register_media_transport_category(avdtp_stream_endpoint_seid(local_stream_endpoint));
    avdtp_source_register_media_codec_category(avdtp_stream_endpoint_seid(local_stream_endpoint), media_type, media_codec_type, 
        codec_capabilities, codec_capabilities_len);
    local_stream_endpoint->remote_configuration.media_codec.media_codec_information     = media_codec_info;
    local_stream_endpoint->remote_configuration.media_codec.media_codec_information_len = media_codec_info_len;
                           
    return local_stream_endpoint->sep.seid;
}

void a2dp_source_establish_stream(bd_addr_t bd_addr, uint8_t local_seid){
    sc.local_stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
    if (!sc.local_stream_endpoint){
        printf(" no local_stream_endpoint for seid %d\n", local_seid);
    }
    avdtp_source_connect(bd_addr);

}

void a2dp_source_disconnect(uint16_t con_handle){
    avdtp_disconnect(con_handle, &a2dp_source_context);
}

void a2dp_source_start_stream(uint8_t int_seid){
    avdtp_start_stream(int_seid, &a2dp_source_context);
}

void a2dp_source_release_stream(uint8_t int_seid){
    avdtp_stop_stream(int_seid, &a2dp_source_context);
}

void a2dp_source_pause_stream(uint8_t int_seid){
    avdtp_suspend_stream(int_seid, &a2dp_source_context);
}

uint8_t a2dp_source_stream_endpoint_ready(uint8_t local_seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint");
        return 0;
    }
    return (stream_endpoint->state == AVDTP_STREAM_ENDPOINT_STREAMING);
}


static void a2dp_source_setup_media_header(uint8_t * media_packet, int size, int *offset, uint8_t marker, uint16_t sequence_number){
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

void a2dp_source_stream_endpoint_request_can_send_now(uint8_t local_seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint");
        return;
    }
    stream_endpoint->send_stream = 1;
    avdtp_request_can_send_now_initiator(stream_endpoint->connection, stream_endpoint->l2cap_media_cid);
}

int a2dp_max_media_payload_size(uint8_t int_seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(int_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for seid %d", int_seid);
        return 0;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        printf("no media cid found for seid %d", int_seid);
        return 0;
    }  
    int sbc_header_size = 12;
    return l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid) - sbc_header_size;
}

uint8_t a2dp_num_frames(uint8_t int_seid, int header_size, int num_bytes_in_frame, int bytes_in_storage){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(int_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for seid %d", int_seid);
        return 0;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        printf("no media cid found for seid %d", int_seid);
        return 0;
    }        

    int num_bytes_to_populate = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid) - header_size;
    int calc_num_frames = btstack_min(num_bytes_to_populate/num_bytes_in_frame, bytes_in_storage/num_bytes_in_frame);
    return (uint8_t)calc_num_frames;
}

static uint8_t sbc_storage[1030];

static void a2dp_source_copy_media_payload(uint8_t * media_packet, int size, int * offset, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames){
    if (size < 18){
        printf("small outgoing buffer\n");
        return;
    }
    
    int pos = *offset;
    media_packet[pos++] = num_frames; // (fragmentation << 7) | (starting_packet << 6) | (last_packet << 5) | num_frames;
    memcpy(media_packet + pos, storage, num_bytes_to_copy);
    pos += num_bytes_to_copy;
    *offset = pos;
}

int a2dp_source_stream_send_media_payload(uint8_t int_seid, btstack_ring_buffer_t * sbc_ring_buffer, uint16_t sbc_frame_bytes, uint8_t num_frames, uint8_t marker){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(int_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        printf("no stream_endpoint found for seid %d", int_seid);
        return 0;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        printf("no media cid found for seid %d", int_seid);
        return 0;
    }        

    int size = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);
    int offset = 0;

    l2cap_reserve_packet_buffer();
    uint8_t * media_packet = l2cap_get_outgoing_buffer();
    //int size = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);
    a2dp_source_setup_media_header(media_packet, size, &offset, marker, stream_endpoint->sequence_number);


    // payload
    int i;
    int storage_offset = 0;
    for (i = 0; i < num_frames; i++){
        uint8_t  sbc_frame_size = 0;
        uint32_t number_of_bytes_read = 0;
        btstack_ring_buffer_read(sbc_ring_buffer, &sbc_frame_size, 1, &number_of_bytes_read);
        btstack_ring_buffer_read(sbc_ring_buffer, sbc_storage + storage_offset, sbc_frame_bytes, &number_of_bytes_read);
        storage_offset += sbc_frame_size;
    }
    
    int num_bytes_to_copy = num_frames*sbc_frame_bytes;
    
    a2dp_source_copy_media_payload(media_packet, size, &offset, &sbc_storage[0], num_bytes_to_copy, num_frames);
    stream_endpoint->sequence_number++;
    l2cap_send_prepared(stream_endpoint->l2cap_media_cid, offset);
    return size;
}
