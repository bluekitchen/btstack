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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "a2dp_sink.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/a2dp.h"
#include "classic/a2dp_sink.h"
#include "classic/avdtp_sink.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_util.h"

static const char * a2dp_sink_default_service_name = "BTstack A2DP Sink Service";
static const char * a2dp_sink_default_service_provider_name = "BTstack A2DP Sink Service Provider";

static uint16_t a2dp_sink_cid;
static bool a2dp_sink_stream_endpoint_configured = false;

static uint8_t (*a2dp_sink_media_config_validator)(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size);

static void a2dp_sink_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void a2dp_sink_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    if (service_provider_name == NULL){
        service_provider_name = a2dp_sink_default_service_provider_name;
    }
    if (service_name == NULL){
        service_name = a2dp_sink_default_service_name;
    }
    a2dp_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_AUDIO_SINK,
                           supported_features, service_name, service_provider_name);
}

void a2dp_sink_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback);

    avdtp_sink_register_packet_handler(&a2dp_sink_packet_handler_internal);
    a2dp_register_sink_packet_handler(callback);
}

void a2dp_sink_register_media_handler(void (*callback)(uint8_t local_seid, uint8_t *packet, uint16_t size)){
    if (callback == NULL){
        log_error("a2dp_sink_register_media_handler called with NULL callback");
        return;
    }
    avdtp_sink_register_media_handler(callback);   
}

void a2dp_sink_init(void){
    a2dp_init();
    avdtp_sink_init();
}

void a2dp_sink_deinit(void){
    a2dp_deinit();
    avdtp_sink_deinit();

    a2dp_sink_cid = 0;
    a2dp_sink_media_config_validator = NULL;
    a2dp_sink_stream_endpoint_configured = false;
}

avdtp_stream_endpoint_t * a2dp_sink_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type,
                                                           const uint8_t *codec_capabilities, uint16_t codec_capabilities_len,
                                                           uint8_t * codec_configuration, uint16_t codec_configuration_len){
    avdtp_stream_endpoint_t * local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, media_type);
    if (!local_stream_endpoint){
        return NULL;
    }
    avdtp_sink_register_media_transport_category(avdtp_stream_endpoint_seid(local_stream_endpoint));
    avdtp_sink_register_media_codec_category(avdtp_stream_endpoint_seid(local_stream_endpoint), media_type, media_codec_type, 
        codec_capabilities, codec_capabilities_len);
	avdtp_sink_register_delay_reporting_category(avdtp_stream_endpoint_seid(local_stream_endpoint));

	// store user codec configuration buffer
	local_stream_endpoint->media_codec_configuration_info = codec_configuration;
	local_stream_endpoint->media_codec_configuration_len  = codec_configuration_len;

    return local_stream_endpoint;
}

void a2dp_sink_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint){
    avdtp_sink_finalize_stream_endpoint(stream_endpoint);
}

uint8_t a2dp_sink_establish_stream(bd_addr_t bd_addr, uint8_t local_seid, uint16_t * avdtp_cid){
	avdtp_stream_endpoint_t * stream_endpoint = avdtp_get_stream_endpoint_for_seid(local_seid);
    if (stream_endpoint == NULL){
        log_info("No local_stream_endpoint for seid %d", local_seid);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    uint16_t outgoing_cid;

    uint8_t status = avdtp_sink_connect(bd_addr, &outgoing_cid);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(outgoing_cid);
    btstack_assert(connection != NULL);

    // setup state
    connection->a2dp_sink_config_process.outgoing_active = true;
    *avdtp_cid = outgoing_cid;

    return ERROR_CODE_SUCCESS;
}

#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
uint8_t a2dp_sink_start_stream_accept(uint16_t a2dp_cid, uint8_t local_seid){
    return avdtp_start_stream_accept(a2dp_cid, local_seid);
}

uint8_t a2dp_sink_start_stream_reject(uint16_t a2dp_cid, uint8_t local_seid){
    return avdtp_start_stream_reject(a2dp_cid, local_seid);
}
#endif

void a2dp_sink_disconnect(uint16_t a2dp_cid){
    avdtp_disconnect(a2dp_cid);
}

static void a2dp_sink_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;

    a2dp_config_process_avdtp_event_handler(AVDTP_ROLE_SINK, packet, size);
}

static uint8_t a2dp_sink_media_config_validator_callback(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size){
    uint8_t error = 0;
    if (a2dp_sink_media_config_validator != NULL) {
        // update subevent id and call validator
        uint8_t avdtp_subevent_id = event[2];
        uint8_t a2dp_subevent_id = a2dp_subevent_id_for_avdtp_subevent_id(avdtp_subevent_id);
        uint8_t * subevent_field = (uint8_t *) &event[2];
        *subevent_field = a2dp_subevent_id;
        error = (*a2dp_sink_media_config_validator)(stream_endpoint, event, size);
        *subevent_field = avdtp_subevent_id;
    }
    return error;
}

void a2dp_sink_register_media_config_validator(uint8_t (*callback)(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size)){
    a2dp_sink_media_config_validator = callback;
    avdtp_sink_register_media_config_validator(&a2dp_sink_media_config_validator_callback);
}


uint8_t a2dp_sink_set_config_sbc(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_sbc_t * configuration){
    return a2dp_config_process_set_sbc(AVDTP_ROLE_SINK, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_sink_set_config_mpeg_audio(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_mpeg_audio_t * configuration){
    return a2dp_config_process_set_mpeg_audio(AVDTP_ROLE_SINK, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_sink_set_config_mpeg_aac(uint16_t a2dp_cid,  uint8_t local_seid,  uint8_t remote_seid, const avdtp_configuration_mpeg_aac_t * configuration){
    return a2dp_config_process_set_mpeg_aac(AVDTP_ROLE_SINK, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_sink_set_config_atrac(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_atrac_t * configuration){
    return a2dp_config_process_set_atrac(AVDTP_ROLE_SINK, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_sink_set_config_other(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid,
                                     const uint8_t * media_codec_information, uint8_t media_codec_information_len){
    return a2dp_config_process_set_other(AVDTP_ROLE_SINK, a2dp_cid, local_seid, remote_seid, media_codec_information, media_codec_information_len);
}
