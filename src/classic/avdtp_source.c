
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

#define BTSTACK_FILE__ "avdtp_source.c"


#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "l2cap.h"

#include "classic/avdtp.h"
#include "classic/avdtp_util.h"
#include "classic/avdtp_source.h"

static avdtp_context_t * avdtp_source_context;
#define AVDTP_MEDIA_PAYLOAD_HEADER_SIZE 12

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

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

uint8_t avdtp_source_connect(bd_addr_t remote, uint16_t * avdtp_cid){
    return avdtp_connect(remote, AVDTP_SINK, avdtp_source_context, avdtp_cid);
}

uint8_t avdtp_source_disconnect(uint16_t avdtp_cid){
    return avdtp_disconnect(avdtp_cid, avdtp_source_context);
}

uint8_t avdtp_source_open_stream(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid){
    return avdtp_open_stream(avdtp_cid, local_seid, remote_seid, avdtp_source_context);
}

uint8_t avdtp_source_start_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_start_stream(avdtp_cid, local_seid, avdtp_source_context);
}

uint8_t avdtp_source_stop_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_stop_stream(avdtp_cid, local_seid, avdtp_source_context);
}

uint8_t avdtp_source_abort_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_abort_stream(avdtp_cid, local_seid, avdtp_source_context);
}

uint8_t avdtp_source_suspend(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_suspend_stream(avdtp_cid, local_seid, avdtp_source_context);
}

uint8_t avdtp_source_discover_stream_endpoints(uint16_t avdtp_cid){
    return avdtp_discover_stream_endpoints(avdtp_cid, avdtp_source_context);
}

uint8_t avdtp_source_get_capabilities(uint16_t avdtp_cid, uint8_t remote_seid){
    return avdtp_get_capabilities(avdtp_cid, remote_seid, avdtp_source_context);
}

uint8_t avdtp_source_get_all_capabilities(uint16_t avdtp_cid, uint8_t remote_seid){
    return avdtp_get_all_capabilities(avdtp_cid, remote_seid, avdtp_source_context);
}

uint8_t avdtp_source_get_configuration(uint16_t avdtp_cid, uint8_t remote_seid){
    return avdtp_get_configuration(avdtp_cid, remote_seid, avdtp_source_context);
}

uint8_t avdtp_source_set_configuration(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    return avdtp_set_configuration(avdtp_cid, local_seid, remote_seid, configured_services_bitmap, configuration, avdtp_source_context);
}

uint8_t avdtp_source_reconfigure(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    return avdtp_reconfigure(avdtp_cid, local_seid, remote_seid, configured_services_bitmap, configuration, avdtp_source_context);
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

    l2cap_register_service(&packet_handler, BLUETOOTH_PSM_AVDTP, 0xffff, LEVEL_2);
}


static void avdtp_source_setup_media_header(uint8_t * media_packet, int size, int *offset, uint8_t marker, uint16_t sequence_number){
    if (size < AVDTP_MEDIA_PAYLOAD_HEADER_SIZE){
        log_error("small outgoing buffer");
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

static void avdtp_source_copy_media_payload(uint8_t * media_packet, int size, int * offset, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames){
    if (size < (num_bytes_to_copy + 1)){
        log_error("small outgoing buffer: buffer size %u, but need %u", size, num_bytes_to_copy + 1);
        return;
    }
    
    int pos = *offset;
    media_packet[pos++] = num_frames; // (fragmentation << 7) | (starting_packet << 6) | (last_packet << 5) | num_frames;
    (void)memcpy(media_packet + pos, storage, num_bytes_to_copy);
    pos += num_bytes_to_copy;
    *offset = pos;
}

int avdtp_source_stream_send_media_payload(uint16_t avdtp_cid, uint8_t local_seid, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames, uint8_t marker){
    if (avdtp_source_context->avdtp_cid != avdtp_cid){
        log_error("avdtp source: avdtp cid 0x%02x not known, expected 0x%02x", avdtp_cid, avdtp_source_context->avdtp_cid);
        return 0;
    }
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, avdtp_source_context);
    if (!stream_endpoint) {
        log_error("avdtp source: no stream_endpoint with seid %d", local_seid);
        return 0;
    }
    
    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("avdtp source: no media connection for seid %d", local_seid);
        return 0;
    } 

    int size = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);
    int offset = 0;

    l2cap_reserve_packet_buffer();
    uint8_t * media_packet = l2cap_get_outgoing_buffer();

    avdtp_source_setup_media_header(media_packet, size, &offset, marker, stream_endpoint->sequence_number);
    avdtp_source_copy_media_payload(media_packet, size, &offset, storage, num_bytes_to_copy, num_frames);
    stream_endpoint->sequence_number++;
    l2cap_send_prepared(stream_endpoint->l2cap_media_cid, offset);
    return size;
}

void avdtp_source_stream_endpoint_request_can_send_now(uint16_t avdtp_cid, uint8_t local_seid){
    if (avdtp_source_context->avdtp_cid != avdtp_cid){
        log_error("AVDTP source: avdtp cid 0x%02x not known, expected 0x%02x", avdtp_cid, avdtp_source_context->avdtp_cid);
        return;
    }
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, avdtp_source_context);
    if (!stream_endpoint) {
        log_error("AVDTP source: no stream_endpoint with seid %d", local_seid);
        return;
    }
    stream_endpoint->send_stream = 1;
    avdtp_request_can_send_now_initiator(stream_endpoint->connection, stream_endpoint->l2cap_media_cid);
}

int avdtp_max_media_payload_size(uint16_t avdtp_cid, uint8_t local_seid){
    if (avdtp_source_context->avdtp_cid != avdtp_cid){
        log_error("A2DP source: a2dp cid 0x%02x not known, expected 0x%02x", avdtp_cid, avdtp_source_context->avdtp_cid);
        return 0;
    }
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, avdtp_source_context);
    if (!stream_endpoint) {
        log_error("A2DP source: no stream_endpoint with seid %d", local_seid);
        return 0;
    }
    
    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("A2DP source: no media connection for seid %d", local_seid);
        return 0;
    }  
    return l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid) - AVDTP_MEDIA_PAYLOAD_HEADER_SIZE;
}
