
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

#define BTSTACK_FILE__ "a2dp_source.c"

#include <stdint.h>
#include <string.h>

#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/a2dp_source.h"
#include "classic/avdtp_source.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_util.h"
#include "l2cap.h"

#define AVDTP_MAX_SEP_NUM 10
#define AVDTP_MEDIA_PAYLOAD_HEADER_SIZE 12

static const char * default_a2dp_source_service_name = "BTstack A2DP Source Service";
static const char * default_a2dp_source_service_provider_name = "BTstack A2DP Source Service Provider";
static avdtp_context_t a2dp_source_context;

static a2dp_state_t app_state = A2DP_IDLE;
static avdtp_stream_endpoint_context_t sc;
static avdtp_sep_t remote_seps[AVDTP_MAX_SEP_NUM];
static int remote_seps_index = 0;

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

static inline void a2dp_signaling_emit_delay_report_capability(btstack_packet_handler_t callback, uint8_t * event, uint16_t event_size){
    if (!callback) return;
    event[0] = HCI_EVENT_A2DP_META;
    event[2] = A2DP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY;
    (*callback)(HCI_EVENT_PACKET, 0, event, event_size);
}

static inline void a2dp_signaling_emit_capabilities_done(btstack_packet_handler_t callback, uint8_t * event, uint16_t event_size){
    if (!callback) return;
    event[0] = HCI_EVENT_A2DP_META;
    event[2] = A2DP_SUBEVENT_SIGNALING_CAPABILITIES_DONE;
    (*callback)(HCI_EVENT_PACKET, 0, event, event_size);
}

static inline void a2dp_signaling_emit_delay_report(btstack_packet_handler_t callback, uint8_t * event, uint16_t event_size){
    if (!callback) return;
    event[0] = HCI_EVENT_A2DP_META;
    event[2] = A2DP_SUBEVENT_SIGNALING_DELAY_REPORT;
    (*callback)(HCI_EVENT_PACKET, 0, event, event_size);
}

static inline void a2dp_signaling_emit_media_codec_sbc(btstack_packet_handler_t callback, uint8_t * event, uint16_t event_size){
    if (!callback) return;
    if (event_size < 18) return;
    event[0] = HCI_EVENT_A2DP_META;
    event[2] = A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION;
    (*callback)(HCI_EVENT_PACKET, 0, event, event_size);
}

static inline void a2dp_signaling_emit_reject_cmd(btstack_packet_handler_t callback, uint8_t * event, uint16_t event_size){
    if (!callback) return;
    if (event_size < 18) return;
    event[0] = HCI_EVENT_A2DP_META;
    event[2] = A2DP_SUBEVENT_COMMAND_REJECTED;
    (*callback)(HCI_EVENT_PACKET, 0, event, event_size);
}

static void a2dp_signaling_emit_connection_established(btstack_packet_handler_t callback, uint16_t cid, bd_addr_t addr, uint8_t status){
    if (!callback) return;
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    event[pos++] = status;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void a2dp_signaling_emit_control_command(btstack_packet_handler_t callback, uint16_t cid, uint8_t local_seid, uint8_t cmd){
    if (!callback) return;
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = cmd;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = local_seid;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void a2dp_signaling_emit_reconfigured(btstack_packet_handler_t callback, uint16_t cid, uint8_t local_seid, uint8_t status){
    if (!callback) return;
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_STREAM_RECONFIGURED;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = status;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint8_t signal_identifier;
    uint8_t status;
    uint8_t local_seid;
    uint8_t remote_seid;
    uint16_t cid;
    bd_addr_t address;
    
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;
    
    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:{
            avdtp_subevent_signaling_connection_established_get_bd_addr(packet, sc.remote_addr);
            cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status = avdtp_subevent_signaling_connection_established_get_status(packet);
            
            if (status != 0){
                log_info("AVDTP_SUBEVENT_SIGNALING_CONNECTION failed status %d ---", status);
                app_state = A2DP_IDLE;
                a2dp_signaling_emit_connection_established(a2dp_source_context.a2dp_callback, cid, sc.remote_addr, status);
                break;
            }
            log_info("A2DP_SUBEVENT_SIGNALING_CONNECTION established avdtp_cid 0x%02x ---", a2dp_source_context.avdtp_cid);
            sc.active_remote_sep = NULL;
            sc.active_remote_sep_index = 0;
            app_state = A2DP_W2_DISCOVER_SEPS;
            remote_seps_index = 0;
            memset(remote_seps, 0, sizeof(avdtp_sep_t) * AVDTP_MAX_SEP_NUM);
            a2dp_signaling_emit_connection_established(a2dp_source_context.a2dp_callback, cid, sc.remote_addr, status);
            avdtp_source_discover_stream_endpoints(cid);
            break;
        }

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:{
            log_info("A2DP received SBC capability.");
            if (!sc.local_stream_endpoint) {
                log_error("invalid local seid %d", avdtp_subevent_signaling_media_codec_sbc_capability_get_local_seid(packet));
                return;
            }
            log_info("A2DP received SBC capability.");
            
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
            log_info("received non SBC codec. not implemented");
            break;
        
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
            log_info("received, but not forwarded: AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY, remote seid %d", avdtp_subevent_signaling_media_transport_capability_get_remote_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
            log_info("received, but not forwarded: AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY, remote seid %d", avdtp_subevent_signaling_reporting_capability_get_remote_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
            log_info("received, but not forwarded: AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY, remote seid %d", avdtp_subevent_signaling_recovery_capability_get_remote_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
            log_info("received, but not forwarded: AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY, remote seid %d", avdtp_subevent_signaling_content_protection_capability_get_remote_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
            log_info("received, but not forwarded: AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY, remote seid %d", avdtp_subevent_signaling_header_compression_capability_get_remote_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
            log_info("received, but not forwarded: AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY, remote seid %d", avdtp_subevent_signaling_multiplexing_capability_get_remote_seid(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            a2dp_signaling_emit_delay_report_capability(a2dp_source_context.a2dp_callback, packet, size);
            break;
        case AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE:
            a2dp_signaling_emit_capabilities_done(a2dp_source_context.a2dp_callback, packet, size);
            break;

        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT:
            // forward packet:
            a2dp_signaling_emit_delay_report(a2dp_source_context.a2dp_callback, packet, size);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            // TODO check cid
            sc.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sc.channel_mode = avdtp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            sc.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sc.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sc.allocation_method = avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            sc.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            sc.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            // TODO: deal with reconfigure: avdtp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            log_info("A2DP received SBC Config: sample rate %u, max bitpool %u.", sc.sampling_frequency, sc.max_bitpool_value);
            log_info("A2DP received SBC Config: sample rate %u, max bitpool %u.", sc.sampling_frequency, sc.max_bitpool_value);
            app_state = A2DP_W2_OPEN_STREAM_WITH_SEID;
            a2dp_signaling_emit_media_codec_sbc(a2dp_source_context.a2dp_callback, packet, size);
            break;
        }  
       
        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW: 
            cid = avdtp_subevent_streaming_can_send_media_packet_now_get_avdtp_cid(packet);
            a2dp_streaming_emit_can_send_media_packet_now(a2dp_source_context.a2dp_callback, cid, 0);
            break;
        
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            avdtp_subevent_streaming_connection_established_get_bd_addr(packet, address);
            status = avdtp_subevent_streaming_connection_established_get_status(packet);
            cid = avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet);
            remote_seid = avdtp_subevent_streaming_connection_established_get_remote_seid(packet);
            local_seid  = avdtp_subevent_streaming_connection_established_get_local_seid(packet);
            if (status != 0){
                log_info("AVDTP_SUBEVENT_STREAMING_CONNECTION could not be established, avdtp_cid 0x%02x, status 0x%02x ---", cid, status);
                a2dp_streaming_emit_connection_established(a2dp_source_context.a2dp_callback, cid, address, local_seid, remote_seid, status);
                break;
            }
            log_info("AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED --- avdtp_cid 0x%02x, local seid %d, remote seid %d", cid, local_seid, remote_seid);
            app_state = A2DP_STREAMING_OPENED;
            a2dp_streaming_emit_connection_established(a2dp_source_context.a2dp_callback, cid, address, local_seid, remote_seid, 0);
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:{
            avdtp_sep_t sep;
            sep.seid = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);;
            sep.in_use = avdtp_subevent_signaling_sep_found_get_in_use(packet);
            sep.media_type = (avdtp_media_type_t) avdtp_subevent_signaling_sep_found_get_media_type(packet);
            sep.type = (avdtp_sep_type_t) avdtp_subevent_signaling_sep_found_get_sep_type(packet);
            log_info("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)", sep.seid, sep.in_use, sep.media_type, sep.type);
            remote_seps[remote_seps_index++] = sep;
            break;
        }
        case AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE:
            app_state = A2DP_W2_GET_CAPABILITIES;
            sc.active_remote_sep_index = 0;
            break;

        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
            // TODO check cid
            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
            cid = avdtp_subevent_signaling_accept_get_avdtp_cid(packet);
            log_info("A2DP Accepted %d, state %d", signal_identifier, app_state);
            
            switch (app_state){
                case A2DP_W2_GET_CAPABILITIES:
                    if (sc.active_remote_sep_index < remote_seps_index){
                        sc.active_remote_sep = &remote_seps[sc.active_remote_sep_index++];
                        avdtp_source_get_capabilities(cid, sc.active_remote_sep->seid);
                    }
                    break;
                case A2DP_W2_SET_CONFIGURATION:{
                    if (!sc.local_stream_endpoint) return;
                    log_info("A2DP initiate set configuration locally and wait for response ... ");
                    app_state = A2DP_IDLE;
                    avdtp_source_set_configuration(cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.active_remote_sep->seid, sc.local_stream_endpoint->remote_configuration_bitmap, sc.local_stream_endpoint->remote_configuration);
                    break;
                }
                case A2DP_W2_RECONFIGURE_WITH_SEID:
                    log_info("A2DP reconfigured");
                    a2dp_signaling_emit_reconfigured(a2dp_source_context.a2dp_callback, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), 0);
                    app_state = A2DP_STREAMING_OPENED;
                    break;
                case A2DP_W2_OPEN_STREAM_WITH_SEID:{
                    log_info("A2DP open stream ");
                    app_state = A2DP_W4_OPEN_STREAM_WITH_SEID;
                    avdtp_source_open_stream(cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.active_remote_sep->seid);
                    break;
                }
                case A2DP_STREAMING_OPENED:
                    if (!a2dp_source_context.a2dp_callback) return;
                    switch (signal_identifier){
                        case  AVDTP_SI_START:
                            a2dp_signaling_emit_control_command(a2dp_source_context.a2dp_callback, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), A2DP_SUBEVENT_STREAM_STARTED);
                            break;
                        case AVDTP_SI_SUSPEND:
                            a2dp_signaling_emit_control_command(a2dp_source_context.a2dp_callback, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), A2DP_SUBEVENT_STREAM_SUSPENDED);
                            break;
                        case AVDTP_SI_ABORT:
                        case AVDTP_SI_CLOSE:
                            a2dp_signaling_emit_control_command(a2dp_source_context.a2dp_callback, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), A2DP_SUBEVENT_STREAM_STOPPED);
                            break;
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
        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            app_state = A2DP_IDLE;
            a2dp_signaling_emit_reject_cmd(a2dp_source_context.a2dp_callback, packet, size);
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:{
            app_state = A2DP_IDLE;
            uint8_t event[6];
            int pos = 0;
            event[pos++] = HCI_EVENT_A2DP_META;
            event[pos++] = sizeof(event) - 2;
            event[pos++] = A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED;
            little_endian_store_16(event, pos, avdtp_subevent_streaming_connection_released_get_avdtp_cid(packet));
            pos += 2;
            event[pos++] = avdtp_subevent_streaming_connection_released_get_local_seid(packet);
            (*a2dp_source_context.a2dp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:{
            app_state = A2DP_IDLE;
            uint8_t event[6];
            int pos = 0;
            event[pos++] = HCI_EVENT_A2DP_META;
            event[pos++] = sizeof(event) - 2;
            event[pos++] = A2DP_SUBEVENT_STREAM_RELEASED;
            little_endian_store_16(event, pos, avdtp_subevent_streaming_connection_released_get_avdtp_cid(packet));
            pos += 2;
            event[pos++] = avdtp_subevent_streaming_connection_released_get_local_seid(packet);
            (*a2dp_source_context.a2dp_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        default:
            app_state = A2DP_IDLE;
            log_info("not implemented");
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
}

avdtp_stream_endpoint_t * a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, 
    uint8_t * codec_capabilities, uint16_t codec_capabilities_len,
    uint8_t * media_codec_info, uint16_t media_codec_info_len){
    avdtp_stream_endpoint_t * local_stream_endpoint = avdtp_source_create_stream_endpoint(AVDTP_SOURCE, media_type);
    if (!local_stream_endpoint){
        return NULL;
    }
    avdtp_source_register_media_transport_category(avdtp_stream_endpoint_seid(local_stream_endpoint));
    avdtp_source_register_media_codec_category(avdtp_stream_endpoint_seid(local_stream_endpoint), media_type, media_codec_type, 
        codec_capabilities, codec_capabilities_len);
    
    local_stream_endpoint->remote_configuration.media_codec.media_codec_information     = media_codec_info;
    local_stream_endpoint->remote_configuration.media_codec.media_codec_information_len = media_codec_info_len;
    sc.local_stream_endpoint = local_stream_endpoint;                     
    avdtp_source_register_delay_reporting_category(avdtp_stream_endpoint_seid(local_stream_endpoint));
    return local_stream_endpoint;
}

uint8_t a2dp_source_establish_stream(bd_addr_t remote_addr, uint8_t loc_seid, uint16_t * a2dp_cid){
    sc.local_stream_endpoint = avdtp_stream_endpoint_for_seid(loc_seid, &a2dp_source_context);
    if (!sc.local_stream_endpoint){
        log_error(" no local_stream_endpoint for seid %d", loc_seid);
        return AVDTP_SEID_DOES_NOT_EXIST;
    }
    memcpy(sc.remote_addr, remote_addr, 6);
    return avdtp_source_connect(remote_addr, a2dp_cid);
}

uint8_t a2dp_source_disconnect(uint16_t a2dp_cid){
    return avdtp_disconnect(a2dp_cid, &a2dp_source_context);
}

uint8_t a2dp_source_reconfigure_stream_sampling_frequency(uint16_t a2dp_cid, uint32_t sampling_frequency){
    // UNUSED(sampling_frequency);

    log_info("a2dp_source_reconfigure_stream");

    memcpy(sc.local_stream_endpoint->reconfigure_media_codec_sbc_info, sc.local_stream_endpoint->remote_sep.configuration.media_codec.media_codec_information, 4);

    // update sampling frequency
    uint8_t config = sc.local_stream_endpoint->reconfigure_media_codec_sbc_info[0] & 0x0f;
    switch (sampling_frequency){
        case 48000:
            config |= (AVDTP_SBC_48000 << 4);
            break;
        case 44100:
            config |= (AVDTP_SBC_44100 << 4);
            break;
        case 32000:
            config |= (AVDTP_SBC_32000 << 4);
            break;
        case 16000:
            config |= (AVDTP_SBC_16000 << 4);
            break;
        default:
            log_error("Unsupported sampling frequency %u", sampling_frequency);
            return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    sc.local_stream_endpoint->reconfigure_media_codec_sbc_info[0] = config;

    avdtp_capabilities_t new_configuration;
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
    new_configuration.media_codec.media_codec_information_len = 4;
    new_configuration.media_codec.media_codec_information = sc.local_stream_endpoint->reconfigure_media_codec_sbc_info;

    // sttart reconfigure
    app_state = A2DP_W2_RECONFIGURE_WITH_SEID;
    return avdtp_source_reconfigure(
        a2dp_cid,
        avdtp_stream_endpoint_seid(sc.local_stream_endpoint),
        sc.active_remote_sep->seid,
        1 << AVDTP_MEDIA_CODEC,
        new_configuration
        );
}

uint8_t a2dp_source_start_stream(uint16_t a2dp_cid, uint8_t local_seid){
    return avdtp_start_stream(a2dp_cid, local_seid, &a2dp_source_context);
}

uint8_t a2dp_source_pause_stream(uint16_t a2dp_cid, uint8_t local_seid){
    return avdtp_suspend_stream(a2dp_cid, local_seid, &a2dp_source_context);
}

static void a2dp_source_setup_media_header(uint8_t * media_packet, int size, int *offset, uint8_t marker, uint16_t sequence_number){
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

void a2dp_source_stream_endpoint_request_can_send_now(uint16_t a2dp_cid, uint8_t local_seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        log_error("A2DP source: no stream_endpoint with seid %d", local_seid);
        return;
    }
    if (a2dp_source_context.avdtp_cid != a2dp_cid){
        log_error("A2DP source: a2dp cid 0x%02x not known, expected 0x%02x", a2dp_cid, a2dp_source_context.avdtp_cid);
        return;
    }
    stream_endpoint->send_stream = 1;
    avdtp_request_can_send_now_initiator(stream_endpoint->connection, stream_endpoint->l2cap_media_cid);
}

int a2dp_max_media_payload_size(uint16_t a2dp_cid, uint8_t local_seid){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        log_error("A2DP source: no stream_endpoint with seid %d", local_seid);
        return 0;
    }
    if (a2dp_source_context.avdtp_cid != a2dp_cid){
        log_error("A2DP source: a2dp cid 0x%02x not known, expected 0x%02x", a2dp_cid, a2dp_source_context.avdtp_cid);
        return 0;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("A2DP source: no media connection for seid %d", local_seid);
        return 0;
    }  
    return l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid) - AVDTP_MEDIA_PAYLOAD_HEADER_SIZE;
}

static void a2dp_source_copy_media_payload(uint8_t * media_packet, int size, int * offset, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames){
    if (size < num_bytes_to_copy + 1){
        log_error("small outgoing buffer: buffer size %u, but need %u", size, num_bytes_to_copy + 1);
        return;
    }
    
    int pos = *offset;
    media_packet[pos++] = num_frames; // (fragmentation << 7) | (starting_packet << 6) | (last_packet << 5) | num_frames;
    memcpy(media_packet + pos, storage, num_bytes_to_copy);
    pos += num_bytes_to_copy;
    *offset = pos;
}

int a2dp_source_stream_send_media_payload(uint16_t a2dp_cid, uint8_t local_seid, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames, uint8_t marker){
    avdtp_stream_endpoint_t * stream_endpoint = avdtp_stream_endpoint_for_seid(local_seid, &a2dp_source_context);
    if (!stream_endpoint) {
        log_error("A2DP source: no stream_endpoint with seid %d", local_seid);
        return 0;
    }
    if (a2dp_source_context.avdtp_cid != a2dp_cid){
        log_error("A2DP source: a2dp cid 0x%02x not known, expected 0x%02x", a2dp_cid, a2dp_source_context.avdtp_cid);
        return 0;
    }

    if (stream_endpoint->l2cap_media_cid == 0){
        log_error("A2DP source: no media connection for seid %d", local_seid);
        return 0;
    } 

    int size = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);
    int offset = 0;

    l2cap_reserve_packet_buffer();
    uint8_t * media_packet = l2cap_get_outgoing_buffer();
    //int size = l2cap_get_remote_mtu_for_local_cid(stream_endpoint->l2cap_media_cid);
    a2dp_source_setup_media_header(media_packet, size, &offset, marker, stream_endpoint->sequence_number);
    a2dp_source_copy_media_payload(media_packet, size, &offset, storage, num_bytes_to_copy, num_frames);
    stream_endpoint->sequence_number++;
    l2cap_send_prepared(stream_endpoint->l2cap_media_cid, offset);
    return size;
}
