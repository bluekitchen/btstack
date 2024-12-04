
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

/**
 * Supported use cases:
 * - single incoming connection: sep discovery starts and stream will get setup if remote sink sep with SBC is found
 * - single outgoing connection: see above
 * - outgoing and incoming connection to same device:
 *    - if outgoing is triggered first, incoming will get ignored.
 *    - if incoming starts first, start ougoing will fail, but incoming will succeed.
 * - outgoing and incoming connections to different devices:
 *    - if outgoing is first, incoming gets ignored.
 *    - if incoming starts first SEP discovery will get stopped and outgoing will succeed.
 */

#define BTSTACK_FILE__ "a2dp_source.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/a2dp.h"
#include "classic/a2dp_source.h"
#include "classic/avdtp_source.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_util.h"
#include "l2cap.h"
#include "a2dp.h"

static const char * a2dp_source_default_service_name = "BTstack A2DP Source Service";
static const char * a2dp_default_source_service_provider_name = "BTstack A2DP Source Service Provider";

static uint8_t (*a2dp_source_media_config_validator)(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size);

void a2dp_source_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    if (service_provider_name == NULL){
        service_provider_name = a2dp_default_source_service_provider_name;
    }
    if (service_name == NULL){
        service_name = a2dp_source_default_service_name;
    }
    a2dp_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_AUDIO_SOURCE,
                           supported_features, service_name, service_provider_name);
}

static void a2dp_source_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;

    switch (hci_event_avdtp_meta_get_subevent_code(packet)){

        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT:
            a2dp_replace_subevent_id_and_emit_source(packet, size, A2DP_SUBEVENT_SIGNALING_DELAY_REPORT);
            break;

        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
            a2dp_replace_subevent_id_and_emit_source(packet, size, A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW);
            break;
        
        default:
            // forward events to config process
            a2dp_config_process_avdtp_event_handler(AVDTP_ROLE_SOURCE, packet, size);
            break;
    }
}
void a2dp_source_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);

    avdtp_source_register_packet_handler(&a2dp_source_packet_handler_internal);
    a2dp_register_source_packet_handler(callback);
}

void a2dp_source_init(void){
    a2dp_init();
    avdtp_source_init();
}

void a2dp_source_deinit(void){
    a2dp_deinit();
    avdtp_source_deinit();
    a2dp_source_media_config_validator = NULL;
}

avdtp_stream_endpoint_t * a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type,
                                                             const uint8_t *codec_capabilities, uint16_t codec_capabilities_len,
                                                             uint8_t * codec_configuration, uint16_t codec_configuration_len){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_source_create_stream_endpoint(AVDTP_SOURCE, media_type);
    if (!stream_endpoint){
        return NULL;
    }
    avdtp_source_register_media_transport_category(avdtp_stream_endpoint_seid(stream_endpoint));
    avdtp_source_register_media_codec_category(avdtp_stream_endpoint_seid(stream_endpoint), media_type, media_codec_type,
                                               codec_capabilities, codec_capabilities_len);
	avdtp_source_register_delay_reporting_category(avdtp_stream_endpoint_seid(stream_endpoint));

	// store user codec configuration buffer
	stream_endpoint->media_codec_type = media_codec_type;
	stream_endpoint->media_codec_configuration_info = codec_configuration;
    stream_endpoint->media_codec_configuration_len  = codec_configuration_len;

    return stream_endpoint;
}

void a2dp_source_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint){
    avdtp_source_finalize_stream_endpoint(stream_endpoint);
}

uint8_t a2dp_source_establish_stream(bd_addr_t remote_addr, uint16_t *avdtp_cid) {

    uint16_t outgoing_cid;

    avdtp_connection_t * connection = avdtp_get_connection_for_bd_addr(remote_addr);
    if (connection == NULL){
        uint8_t status = avdtp_source_connect(remote_addr, &outgoing_cid);
        if (status != ERROR_CODE_SUCCESS) {
            // if there's already a connection for for remote addr, avdtp_source_connect fails,
            // but the stream will get set-up nevertheless
            return status;
        }
        connection = avdtp_get_connection_for_avdtp_cid(outgoing_cid);
        btstack_assert(connection != NULL);

        // setup state
        connection->a2dp_source_config_process.outgoing_active = true;
        connection->a2dp_source_config_process.state = A2DP_W4_CONNECTED;
        *avdtp_cid = outgoing_cid;

    } else {
        if (connection->a2dp_source_config_process.outgoing_active || connection->a2dp_source_config_process.stream_endpoint_configured) {
            return ERROR_CODE_COMMAND_DISALLOWED;
        }

        // check state
        switch (connection->a2dp_source_config_process.state){
            case A2DP_IDLE:
            case A2DP_CONNECTED:
                // restart process e.g. if there no suitable stream endpoints or they had been in use
                connection->a2dp_source_config_process.outgoing_active = true;
                *avdtp_cid = connection->avdtp_cid;
                a2dp_config_process_ready_for_sep_discovery(AVDTP_ROLE_SOURCE, connection);
                break;
            default:
                return ERROR_CODE_COMMAND_DISALLOWED;
        }
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t a2dp_source_disconnect(uint16_t avdtp_cid){
    return avdtp_disconnect(avdtp_cid);
}

uint8_t a2dp_source_start_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_start_stream(avdtp_cid, local_seid);
}

uint8_t a2dp_source_pause_stream(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_suspend_stream(avdtp_cid, local_seid);
}

void a2dp_source_stream_endpoint_request_can_send_now(uint16_t avdtp_cid, uint8_t local_seid){
    avdtp_source_stream_endpoint_request_can_send_now(avdtp_cid, local_seid);
}

int a2dp_max_media_payload_size(uint16_t avdtp_cid, uint8_t local_seid){
    return avdtp_max_media_payload_size(avdtp_cid, local_seid);
}

uint8_t
a2dp_source_stream_send_media_payload_rtp(uint16_t a2dp_cid, uint8_t local_seid, uint8_t marker, uint32_t timestamp,
                                          uint8_t *payload, uint16_t payload_size) {
    return avdtp_source_stream_send_media_payload_rtp(a2dp_cid, local_seid, marker, timestamp, payload, payload_size);
}

uint8_t	a2dp_source_stream_send_media_packet(uint16_t a2dp_cid, uint8_t local_seid, const uint8_t * packet, uint16_t size){
    return avdtp_source_stream_send_media_packet(a2dp_cid, local_seid, packet, size);
}


uint8_t a2dp_source_set_config_sbc(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_sbc_t * configuration){
    return a2dp_config_process_set_sbc(AVDTP_ROLE_SOURCE, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_source_set_config_mpeg_audio(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_mpeg_audio_t * configuration){
    return a2dp_config_process_set_mpeg_audio(AVDTP_ROLE_SOURCE, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_source_set_config_mpeg_aac(uint16_t a2dp_cid,  uint8_t local_seid,  uint8_t remote_seid, const avdtp_configuration_mpeg_aac_t * configuration){
    return a2dp_config_process_set_mpeg_aac(AVDTP_ROLE_SOURCE, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_source_set_config_atrac(uint16_t a2dp_cid, uint8_t local_seid, uint8_t remote_seid, const avdtp_configuration_atrac_t * configuration){
    return a2dp_config_process_set_atrac(AVDTP_ROLE_SOURCE, a2dp_cid, local_seid, remote_seid, configuration);
}

uint8_t a2dp_source_set_config_other(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid,
                                     const uint8_t * media_codec_information, uint8_t media_codec_information_len){
    return a2dp_config_process_set_other(AVDTP_ROLE_SOURCE, a2dp_cid, local_seid, remote_seid, media_codec_information, media_codec_information_len);
}

uint8_t a2dp_source_reconfigure_stream_sampling_frequency(uint16_t avdtp_cid, uint32_t sampling_frequency){
    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(avdtp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (connection->a2dp_source_config_process.state != A2DP_STREAMING_OPENED) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    btstack_assert(connection->a2dp_source_config_process.local_stream_endpoint != NULL);

    log_info("Reconfigure avdtp_cid 0x%02x", avdtp_cid);

    avdtp_media_codec_type_t codec_type = connection->a2dp_source_config_process.local_stream_endpoint->sep.capabilities.media_codec.media_codec_type;
    uint8_t codec_info_len;
    uint8_t status;
    switch (codec_type){
        case AVDTP_CODEC_SBC:
            codec_info_len = 4;
            (void)memcpy(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, connection->a2dp_source_config_process.local_stream_endpoint->remote_sep.configuration.media_codec.media_codec_information, codec_info_len);
            status = avdtp_config_sbc_set_sampling_frequency(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, sampling_frequency);
            break;
        case AVDTP_CODEC_MPEG_1_2_AUDIO:
            codec_info_len = 4;
            (void)memcpy(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, connection->a2dp_source_config_process.local_stream_endpoint->remote_sep.configuration.media_codec.media_codec_information, codec_info_len);
            status = avdtp_config_mpeg_audio_set_sampling_frequency(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, sampling_frequency);
            break;
        case AVDTP_CODEC_MPEG_2_4_AAC:
            codec_info_len = 6;
            (void)memcpy(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, connection->a2dp_source_config_process.local_stream_endpoint->remote_sep.configuration.media_codec.media_codec_information, codec_info_len);
            status = avdtp_config_mpeg_aac_set_sampling_frequency(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, sampling_frequency);
            break;
        case AVDTP_CODEC_ATRAC_FAMILY:
            codec_info_len = 7;
            (void)memcpy(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, connection->a2dp_source_config_process.local_stream_endpoint->remote_sep.configuration.media_codec.media_codec_information, codec_info_len);
            status = avdtp_config_atrac_set_sampling_frequency(connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info, sampling_frequency);
            break;
        default:
            return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }
    avdtp_capabilities_t new_configuration;
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type = codec_type;
    new_configuration.media_codec.media_codec_information_len = codec_info_len;
    new_configuration.media_codec.media_codec_information = connection->a2dp_source_config_process.local_stream_endpoint->media_codec_info;

    // start reconfigure
    connection->a2dp_source_config_process.state = A2DP_W2_RECONFIGURE_WITH_SEID;

    return avdtp_source_reconfigure(
            avdtp_cid,
            avdtp_stream_endpoint_seid(connection->a2dp_source_config_process.local_stream_endpoint),
            connection->a2dp_source_config_process.local_stream_endpoint->remote_sep.seid,
            1 << AVDTP_MEDIA_CODEC,
            new_configuration
    );
}

static uint8_t a2dp_source_media_config_validator_callback(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size){
    uint8_t error = 0;
    if (a2dp_source_media_config_validator != NULL) {
        // update subevent id and call validator
        uint8_t avdtp_subevent_id = event[2];
        uint8_t a2dp_subevent_id = a2dp_subevent_id_for_avdtp_subevent_id(avdtp_subevent_id);
        uint8_t * subevent_field = (uint8_t *) &event[2];
        *subevent_field = a2dp_subevent_id;
        error = (*a2dp_source_media_config_validator)(stream_endpoint, event, size);
        *subevent_field = avdtp_subevent_id;
    }
    return error;
}

void a2dp_source_register_media_config_validator(uint8_t (*callback)(const avdtp_stream_endpoint_t * stream_endpoint, const uint8_t * event, uint16_t size)){
    a2dp_source_media_config_validator = callback;
    avdtp_source_register_media_config_validator(&a2dp_source_media_config_validator_callback);
}

