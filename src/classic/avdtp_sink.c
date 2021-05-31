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

#define BTSTACK_FILE__ "avdtp_sink.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "l2cap.h"

#include "classic/avdtp.h"
#include "classic/avdtp_acceptor.h"
#include "classic/avdtp_initiator.h"
#include "classic/avdtp_sink.h"
#include "classic/avdtp_util.h"

void avdtp_sink_register_media_transport_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_media_transport_category(stream_endpoint);
}

void avdtp_sink_register_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_reporting_category(stream_endpoint);
}

void avdtp_sink_register_delay_reporting_category(uint8_t seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_delay_reporting_category(stream_endpoint);
}

void avdtp_sink_register_recovery_category(uint8_t seid, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_recovery_category(stream_endpoint, maximum_recovery_window_size, maximum_number_media_packets);
}

void avdtp_sink_register_content_protection_category(uint8_t seid, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_content_protection_category(stream_endpoint, cp_type, cp_type_value, cp_type_value_len);
}

void avdtp_sink_register_header_compression_category(uint8_t seid, uint8_t back_ch, uint8_t media, uint8_t recovery){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_header_compression_category(stream_endpoint, back_ch, media, recovery);
}

void avdtp_sink_register_media_codec_category(uint8_t seid, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t *media_codec_info, uint16_t media_codec_info_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_media_codec_category(stream_endpoint, media_type, media_codec_type, media_codec_info, media_codec_info_len);
}

void avdtp_sink_register_multiplexing_category(uint8_t seid, uint8_t fragmentation){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(seid);
    avdtp_register_multiplexing_category(stream_endpoint, fragmentation);
}
/* END: tracking can send now requests pro l2cap cid */

void avdtp_sink_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);

    avdtp_register_sink_packet_handler(callback);
}

void avdtp_sink_init(void) {
    avdtp_init();
}

void avdtp_sink_deinit(void){
    avdtp_deinit();
}

avdtp_stream_endpoint_t * avdtp_sink_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type){
    return avdtp_create_stream_endpoint(sep_type, media_type);
}

void avdtp_sink_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint){
    avdtp_finalize_stream_endpoint(stream_endpoint);
}

void avdtp_sink_register_media_handler(void (*callback)(uint8_t local_seid, uint8_t *packet, uint16_t size)){
    avdtp_register_media_handler(callback);
}

uint8_t avdtp_sink_connect(bd_addr_t remote, uint16_t * avdtp_cid){
    return avdtp_connect(remote, AVDTP_ROLE_SINK, avdtp_cid);
}

uint8_t avdtp_sink_disconnect(uint16_t avdtp_cid){
    return avdtp_disconnect(avdtp_cid);
}

uint8_t avdtp_sink_open_stream(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid){
    return avdtp_open_stream(avdtp_cid, local_seid, remote_seid);
}

uint8_t avdtp_sink_start_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_start_stream(avdtp_cid, local_seid);
}

uint8_t avdtp_sink_stop_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_stop_stream(avdtp_cid, local_seid);
}

uint8_t avdtp_sink_abort_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_abort_stream(avdtp_cid, local_seid);
}

uint8_t avdtp_sink_suspend(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_suspend_stream(avdtp_cid, local_seid);
}

uint8_t avdtp_sink_discover_stream_endpoints(uint16_t avdtp_cid){
    return avdtp_discover_stream_endpoints(avdtp_cid);
}

uint8_t avdtp_sink_get_capabilities(uint16_t avdtp_cid, uint8_t remote_seid){
    return avdtp_get_capabilities(avdtp_cid, remote_seid);
}

uint8_t avdtp_sink_get_all_capabilities(uint16_t avdtp_cid, uint8_t remote_seid){
    return avdtp_get_all_capabilities(avdtp_cid, remote_seid);
}

uint8_t avdtp_sink_get_configuration(uint16_t avdtp_cid, uint8_t remote_seid){
    return avdtp_get_configuration(avdtp_cid, remote_seid);
}

uint8_t avdtp_sink_set_configuration(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    return avdtp_set_configuration(avdtp_cid, local_seid, remote_seid, configured_services_bitmap, configuration);
}

uint8_t avdtp_sink_reconfigure(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration){
    return avdtp_reconfigure(avdtp_cid, local_seid, remote_seid, configured_services_bitmap, configuration);
}

uint8_t avdtp_sink_delay_report(uint16_t avdtp_cid, uint8_t local_seid, uint16_t delay_100us){
    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(avdtp_cid);
    if (!connection){
        log_error("delay_report: no connection for signaling cid 0x%02x found", avdtp_cid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((connection->state != AVDTP_SIGNALING_CONNECTION_OPENED) ||
        (connection->initiator_connection_state != AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE)) {
        log_error("delay_report: connection in wrong state, state %d, initiator state %d", connection->state, connection->initiator_connection_state);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(local_seid);
    if (!stream_endpoint) {
        log_error("delay_report: no stream_endpoint with seid %d found", local_seid);
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (stream_endpoint->state < AVDTP_STREAM_ENDPOINT_CONFIGURED){
        log_error("Stream endpoint seid %d in wrong state %d", local_seid, stream_endpoint->state);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    
    connection->initiator_transaction_label++;
    connection->initiator_connection_state = AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SEND_DELAY_REPORT;
    connection->delay_ms = delay_100us;
    connection->initiator_local_seid = local_seid;
    connection->initiator_remote_seid = stream_endpoint->remote_sep.seid;
	avdtp_request_can_send_now_initiator(connection);
    return ERROR_CODE_SUCCESS;
}

