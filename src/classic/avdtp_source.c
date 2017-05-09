
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

static avdtp_context_t * avdtp_source_context;
static avdtp_connection_t * avtdp_connection_doing_sdp_query = NULL;
static int record_id = -1;
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static uint8_t   attribute_value[1000];
static const unsigned int attribute_value_buffer_size = sizeof(attribute_value);

void avdtp_source_register_media_transport_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_media_transport_category(stream_endpoint);
}

void avdtp_source_register_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_reporting_category(stream_endpoint);
}

void avdtp_source_register_delay_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_delay_reporting_category(stream_endpoint);
}

void avdtp_source_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_recovery_category(stream_endpoint, maximum_recovery_window_size, maximum_number_media_packets);
}

void avdtp_source_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_content_protection_category(stream_endpoint, cp_type, cp_type_value, cp_type_value_len);
}

void avdtp_source_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_header_compression_category(stream_endpoint, back_ch, media, recovery);
}

void avdtp_source_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, uint8_t * media_codec_info, uint16_t media_codec_info_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_media_codec_category(stream_endpoint, media_type, media_codec_type, media_codec_info, media_codec_info_len);
}

void avdtp_source_register_multiplexing_category(uint8_t seid, uint8_t fragmentation){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(seid, avdtp_source_context);
    avdtp_register_multiplexing_category(stream_endpoint, fragmentation);
}

avdtp_stream_endpoint_t * avdtp_source_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type){
    return avdtp_create_stream_endpoint(sep_type, media_type, avdtp_source_context);
}

void avdtp_source_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("avdtp_source_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_source_context->avdtp_callback = callback;
}

static void avdtp_source_handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    des_iterator_t des_list_it;
    des_iterator_t prot_it;
    uint32_t avdtp_remote_uuid    = 0;
    uint16_t avdtp_l2cap_psm      = 0;
    uint16_t avdtp_version        = 0;
    
    if (!avtdp_connection_doing_sdp_query) return;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            // Handle new SDP record 
            if (sdp_event_query_attribute_byte_get_record_id(packet) != record_id) {
                record_id = sdp_event_query_attribute_byte_get_record_id(packet);
                printf("SDP Record: Nr: %d\n", record_id);
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
                                        printf("SDP Attribute 0x%04x: AVDTP SOURCE protocol UUID: 0x%04x\n", sdp_event_query_attribute_byte_get_attribute_id(packet), uuid);
                                        avdtp_remote_uuid = uuid;
                                        break;
                                    case BLUETOOTH_SERVICE_CLASS_AUDIO_SINK:
                                        printf("SDP Attribute 0x%04x: AVDTP SINK protocol UUID: 0x%04x\n", sdp_event_query_attribute_byte_get_attribute_id(packet), uuid);
                                        avdtp_remote_uuid = uuid;
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                        
                        case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST: {
                                printf("SDP Attribute: 0x%04x\n", sdp_event_query_attribute_byte_get_attribute_id(packet));

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
                                printf("l2cap_psm 0x%04x, avdtp_version 0x%04x\n", avdtp_l2cap_psm, avdtp_version);

                                /* Create AVDTP connection */
                                avtdp_connection_doing_sdp_query->state = AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED;
                                l2cap_create_channel(packet_handler, avtdp_connection_doing_sdp_query->remote_addr, avdtp_l2cap_psm, l2cap_max_mtu(), NULL);
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
            fprintf(stderr, "General query done with status %d.\n", sdp_event_query_complete_get_status(packet));
            break;
    }
}

void avdtp_source_connect(bd_addr_t remote){
    avtdp_connection_doing_sdp_query = NULL;
    avdtp_connection_t * connection = avdtp_connection_for_bd_addr(remote, avdtp_source_context);
    if (!connection){
        connection = avdtp_create_connection(remote, avdtp_source_context);
    }
    if (connection->state != AVDTP_SIGNALING_CONNECTION_IDLE) return;
    avtdp_connection_doing_sdp_query = connection;
    connection->state = AVDTP_SIGNALING_W4_SDP_QUERY_COMPLETE;
    sdp_client_query_uuid16(&avdtp_source_handle_sdp_client_query_result, remote, BLUETOOTH_PROTOCOL_AVDTP);
}

void avdtp_source_disconnect(uint16_t con_handle){
    avdtp_disconnect(con_handle, avdtp_source_context);
}

void avdtp_source_open_stream(uint16_t con_handle, uint8_t int_seid, uint8_t acp_seid){
    avdtp_open_stream(con_handle, int_seid, acp_seid, avdtp_source_context);
}

void avdtp_source_start_stream(uint8_t int_seid){
    avdtp_start_stream(int_seid, avdtp_source_context);
}

void avdtp_source_stop_stream(uint8_t int_seid){
    avdtp_stop_stream(int_seid, avdtp_source_context);
}

void avdtp_source_abort_stream(uint8_t int_seid){
    avdtp_abort_stream(int_seid, avdtp_source_context);
}

void avdtp_source_suspend(uint8_t int_seid){
    avdtp_suspend_stream(int_seid, avdtp_source_context);
}

void avdtp_source_discover_stream_endpoints(uint16_t con_handle){
    avdtp_discover_stream_endpoints(con_handle, avdtp_source_context);
}

void avdtp_source_get_capabilities(uint16_t con_handle, uint8_t acp_seid){
    avdtp_get_capabilities(con_handle, acp_seid, avdtp_source_context);
}

void avdtp_source_get_all_capabilities(uint16_t con_handle, uint8_t acp_seid){
    avdtp_get_all_capabilities(con_handle, acp_seid, avdtp_source_context);
}

void avdtp_source_get_configuration(uint16_t con_handle, uint8_t acp_seid){
    avdtp_get_configuration(con_handle, acp_seid, avdtp_source_context);
}

void avdtp_source_set_configuration(uint16_t con_handle, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    avdtp_set_configuration(con_handle, int_seid, acp_seid, configured_services_bitmap, configuration, avdtp_source_context);
}

void avdtp_source_reconfigure(uint16_t con_handle, uint8_t int_seid, uint8_t acp_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    avdtp_reconfigure(con_handle, int_seid, acp_seid, configured_services_bitmap, configuration, avdtp_source_context);
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    avdtp_packet_handler(packet_type, channel, packet, size, avdtp_source_context);      
}

void avdtp_source_init(avdtp_context_t * avdtp_context){
    if (!avdtp_context){
        log_error("avdtp_source_context is NULL");
        return;
    }
    avdtp_source_context = avdtp_context;
    avdtp_source_context->stream_endpoints = NULL;
    avdtp_source_context->connections = NULL;
    avdtp_source_context->stream_endpoints_id_counter = 0;
    avdtp_source_context->packet_handler = packet_handler;

    l2cap_register_service(&packet_handler, BLUETOOTH_PROTOCOL_AVDTP, 0xffff, LEVEL_0);
}

uint8_t avdtp_source_remote_seps_num(uint16_t avdtp_cid){
    return avdtp_remote_seps_num(avdtp_cid, avdtp_source_context);
}

avdtp_sep_t * avdtp_source_remote_sep(uint16_t avdtp_cid, uint8_t index){
    return avdtp_remote_sep(avdtp_cid, index, avdtp_source_context);
}
