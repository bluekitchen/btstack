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

#define BTSTACK_FILE__ "a2dp_sink.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/a2dp_sink.h"
#include "classic/avdtp_sink.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_util.h"

static const char * default_a2dp_sink_service_name = "BTstack A2DP Sink Service";
static const char * default_a2dp_sink_service_provider_name = "BTstack A2DP Sink Service Provider";

static bool outgoing_active = false;
static uint16_t a2dp_sink_cid;
static bool stream_endpoint_configured = false;

static btstack_packet_handler_t a2dp_sink_packet_handler_user;

static void a2dp_sink_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void a2dp_sink_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_AUDIO_SINK); 
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
            de_add_number(l2cpProtocol,  DE_UINT, DE_SIZE_16, BLUETOOTH_PSM_AVDTP);  
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
        de_add_data(service,  DE_STRING, strlen(default_a2dp_sink_service_name), (uint8_t *) default_a2dp_sink_service_name);
    }

    // 0x0100 "Provider Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0102);
    if (service_provider_name){
        de_add_data(service,  DE_STRING, strlen(service_provider_name), (uint8_t *) service_provider_name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_a2dp_sink_service_provider_name), (uint8_t *) default_a2dp_sink_service_provider_name);
    }

    // 0x0311 "Supported Features"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);
    de_add_number(service, DE_UINT, DE_SIZE_16, supported_features);
}

void a2dp_sink_register_packet_handler(btstack_packet_handler_t callback){
    // avdtp_sink_register_packet_handler(callback);
    // return;
    if (callback == NULL){
        log_error("a2dp_sink_register_packet_handler called with NULL callback");
        return;
    }
    avdtp_sink_register_packet_handler(&a2dp_sink_packet_handler_internal);
    a2dp_sink_packet_handler_user = callback;
}

void a2dp_sink_register_media_handler(void (*callback)(uint8_t local_seid, uint8_t *packet, uint16_t size)){
    if (callback == NULL){
        log_error("a2dp_sink_register_media_handler called with NULL callback");
        return;
    }
    avdtp_sink_register_media_handler(callback);   
}

void a2dp_sink_init(void){
    avdtp_sink_init();
}

void a2dp_sink_deinit(void){
    avdtp_sink_deinit();

    outgoing_active = false;
    a2dp_sink_cid = 0;
    stream_endpoint_configured = false;
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
    // remember to tell client
    outgoing_active = true;
    return avdtp_sink_connect(bd_addr, avdtp_cid);
}

void a2dp_sink_disconnect(uint16_t a2dp_cid){
    avdtp_disconnect(a2dp_cid);
}

static void a2dp_sink_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;
    uint8_t local_seid;
    uint8_t signal_identifier;
    bool reconfigure;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            if (stream_endpoint_configured) return;

            status = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                // only care for outgoing connections
                if (!outgoing_active) break;
                outgoing_active = false;
                a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED);
                log_info("A2DP sink signaling connection failed status %d", status);
                break;
            }

            a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED);
            log_info("A2DP sink signaling connection established avdtp_cid 0x%02x", avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet));
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION:
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION:
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION:
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            reconfigure = avdtp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet) != 0;
            // accept configure if not configured and reconfigure if already configured
            if (stream_endpoint_configured != reconfigure) break;
            stream_endpoint_configured = true;
            a2dp_sink_cid = avdtp_subevent_signaling_media_codec_other_capability_get_avdtp_cid(packet);
            switch (packet[2]){
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:
                    a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION);
                    break;
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION:
                    a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION);
                    break;
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION:
                    a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION);
                    break;
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION:
                    a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION);
                    break;
                case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
                    a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION);
                    break;
                default:
                    btstack_assert(false);
                    break;
            }
            break;

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            if (stream_endpoint_configured == false) break;
            if (a2dp_sink_cid != avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet)) break;
            
            // about to notify client
            outgoing_active = false;
            status = avdtp_subevent_streaming_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                log_info("A2DP sink streaming connection could not be established, avdtp_cid 0x%02x, status 0x%02x ---", a2dp_sink_cid, status);
                a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAM_ESTABLISHED);
                break;
            }

            log_info("A2DP streaming connection established --- avdtp_cid 0x%02x, local seid %d, remote seid %d", a2dp_sink_cid, 
                avdtp_subevent_streaming_connection_established_get_local_seid(packet), 
                avdtp_subevent_streaming_connection_established_get_remote_seid(packet));

            a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAM_ESTABLISHED);
            break;

        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
            if (stream_endpoint_configured == false) break;
            if (a2dp_sink_cid != avdtp_subevent_signaling_accept_get_avdtp_cid(packet)) break;

            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
            local_seid = avdtp_subevent_signaling_accept_get_local_seid(packet);

            switch (signal_identifier){
                case  AVDTP_SI_START:
                    a2dp_emit_stream_event(a2dp_sink_packet_handler_user, a2dp_sink_cid, local_seid, A2DP_SUBEVENT_STREAM_STARTED);
                    break;
                case AVDTP_SI_SUSPEND:
                    a2dp_emit_stream_event(a2dp_sink_packet_handler_user, a2dp_sink_cid, local_seid, A2DP_SUBEVENT_STREAM_SUSPENDED);
                    break;
                case AVDTP_SI_ABORT:
                case AVDTP_SI_CLOSE:
                    a2dp_emit_stream_event(a2dp_sink_packet_handler_user, a2dp_sink_cid, local_seid, A2DP_SUBEVENT_STREAM_STOPPED);
                    break;
                default:
                    break;
            }
            break;
        
        case AVDTP_SUBEVENT_SIGNALING_REJECT:
            if (stream_endpoint_configured == false) break;
            if (a2dp_sink_cid != avdtp_subevent_signaling_reject_get_avdtp_cid(packet)) break;

            a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_COMMAND_REJECTED);
            break;

        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            if (stream_endpoint_configured == false) break;
            if (a2dp_sink_cid != avdtp_subevent_signaling_general_reject_get_avdtp_cid(packet)) break;

            a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_COMMAND_REJECTED);
            break;

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
            if (stream_endpoint_configured == false) break;
            if (a2dp_sink_cid != avdtp_subevent_streaming_connection_released_get_avdtp_cid(packet)) break;

            stream_endpoint_configured = false;

            a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAM_RELEASED);
            break;
        
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            if (a2dp_sink_cid != avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet)) break;

            stream_endpoint_configured = false;
            outgoing_active = false;

            a2dp_replace_subevent_id_and_emit_cmd(a2dp_sink_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED);
            break;
        default:
            break;
    }

}

