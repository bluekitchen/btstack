
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
#include "classic/a2dp_source.h"
#include "classic/avdtp_source.h"
#include "classic/avdtp_util.h"
#include "classic/sdp_util.h"
#include "l2cap.h"

#define AVDTP_MAX_SEP_NUM 10
#define A2DP_SET_CONFIG_DELAY_MS 150

static const char * default_a2dp_source_service_name = "BTstack A2DP Source Service";
static const char * default_a2dp_source_service_provider_name = "BTstack A2DP Source Service Provider";
static bool sep_found_w2_set_configuration = false;
static btstack_timer_source_t a2dp_source_set_config_timer;

//
static bool     outgoing_active;

// discover remote seps
static a2dp_state_t a2dp_source_state = A2DP_IDLE;
static uint16_t     a2dp_source_cid;
static uint16_t     num_remote_seps = 0;
static avdtp_sep_t  remote_seps[AVDTP_MAX_SEP_NUM];

static bool stream_endpoint_configured = false;

static avdtp_stream_endpoint_context_t sc;
static btstack_packet_handler_t a2dp_source_packet_handler_user;

static void a2dp_source_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void a2dp_discover_seps_with_next_waiting_connection(void);

static void a2dp_source_streaming_emit_connection_failed(avdtp_connection_t * connection, uint8_t local_seid, uint8_t status) {
    uint8_t event[14];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_STREAM_ESTABLISHED;
    little_endian_store_16(event, pos, connection->avdtp_cid);
    pos += 2;
    reverse_bd_addr(connection->remote_addr, &event[pos]);
    pos += 6;
    event[pos++] = local_seid;
    event[pos++] = 0;
    event[pos++] = status;

    (*a2dp_source_packet_handler_user)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

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

static void a2dp_signaling_emit_reconfigured(uint16_t cid, uint8_t local_seid, uint8_t status){
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_A2DP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = A2DP_SUBEVENT_STREAM_RECONFIGURED;
    little_endian_store_16(event, pos, cid);
    pos += 2;
    event[pos++] = local_seid;
    event[pos++] = status;
    (*a2dp_source_packet_handler_user)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void a2dp_source_discover_stream_endpoints(uint16_t avdtp_cid){
    a2dp_source_cid = avdtp_cid;
    avdtp_source_discover_stream_endpoints(avdtp_cid);
}

static void a2dp_source_set_config_timer_handler(btstack_timer_source_t * timer){
    if (stream_endpoint_configured) return;

    uint16_t avdtp_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(timer);
	avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(avdtp_cid);
	btstack_run_loop_set_timer_context(&a2dp_source_set_config_timer, NULL);

    if (connection == NULL) {
        log_info("a2dp_discover_seps_with_next_waiting_connection");
        a2dp_discover_seps_with_next_waiting_connection();
        return;
    }

    a2dp_source_discover_stream_endpoints(avdtp_cid);
}

static void a2dp_source_set_config_timer_start(uint16_t avdtp_cid){
    log_info("a2dp_source_set_config_timer_start cid 0%02x", avdtp_cid);
    btstack_run_loop_remove_timer(&a2dp_source_set_config_timer);
    btstack_run_loop_set_timer_handler(&a2dp_source_set_config_timer,a2dp_source_set_config_timer_handler);
    btstack_run_loop_set_timer(&a2dp_source_set_config_timer, A2DP_SET_CONFIG_DELAY_MS);
    btstack_run_loop_set_timer_context(&a2dp_source_set_config_timer, (void *)(uintptr_t)avdtp_cid);
    btstack_run_loop_add_timer(&a2dp_source_set_config_timer);
}

static void a2dp_source_set_config_timer_stop(void){
    log_info("a2dp_source_set_config_timer_stop");
    btstack_run_loop_remove_timer(&a2dp_source_set_config_timer);
	btstack_run_loop_set_timer_context(&a2dp_source_set_config_timer, NULL);
}

static void a2dp_start_discovering_seps(avdtp_connection_t * connection){
    a2dp_source_state = A2DP_DISCOVER_SEPS;
    sc.active_remote_sep_index = 0;
    num_remote_seps = 0;
    memset(remote_seps, 0, sizeof(avdtp_sep_t) * AVDTP_MAX_SEP_NUM);
    connection->a2dp_source_discover_seps = false;

    // if we initiated the connection, start config right away, else wait a bit to give remote a chance to do it first
    if (outgoing_active && (a2dp_source_cid == connection->avdtp_cid)){
        log_info("discover seps");
        a2dp_source_discover_stream_endpoints(connection->avdtp_cid);
    } else {
        log_info("wait a bit, then discover seps");
        a2dp_source_set_config_timer_start(connection->avdtp_cid);
    }
}

static void a2dp_discover_seps_with_next_waiting_connection(void){
    a2dp_source_state = A2DP_IDLE;
                    
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, avdtp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        avdtp_connection_t * next_connection = (avdtp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (!next_connection->a2dp_source_discover_seps) continue;
        a2dp_start_discovering_seps(next_connection);
    }
}

static void a2dp_source_packet_handler_internal(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint16_t cid;
    avdtp_connection_t * connection;

    uint8_t signal_identifier;
    uint8_t status;
    uint8_t remote_seid;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            if (sc.local_stream_endpoint == NULL) return;

            cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            connection = avdtp_get_connection_for_avdtp_cid(cid);
            btstack_assert(connection != NULL);

            status = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                // notify about connection error only if we're initiator
                if (outgoing_active && (a2dp_source_cid == cid)){
                    log_info("A2DP source signaling connection failed status 0x%02x", status);
                    outgoing_active = false;
                    a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED);
                }
                break;
            }
            log_info("A2DP source signaling connection established avdtp_cid 0x%02x", cid);

            // notify app
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED);

            // we already have a valid setup
            if (stream_endpoint_configured) return;

            // start discover seps now if:
            // - outgoing active: signaling for outgoing connection
            // - outgoing not active: incoming connection and no sep discover ongoing

            log_info("outgoing_active %d, current avdtp cid 0x%02x, a2dp_source_state %d", outgoing_active, cid, a2dp_source_state);
            if ((outgoing_active && (a2dp_source_cid == cid)) || (!outgoing_active && (a2dp_source_state == A2DP_IDLE))){
                a2dp_start_discovering_seps(connection);
            } else {
                // post-pone sep discovery
                connection->a2dp_source_discover_seps = true;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
            cid = avdtp_subevent_signaling_sep_found_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state == A2DP_DISCOVER_SEPS) {
                avdtp_sep_t sep;
                sep.seid       = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);;
                sep.in_use     = avdtp_subevent_signaling_sep_found_get_in_use(packet);
                sep.media_type = (avdtp_media_type_t) avdtp_subevent_signaling_sep_found_get_media_type(packet);
                sep.type       = (avdtp_sep_type_t) avdtp_subevent_signaling_sep_found_get_sep_type(packet);
                log_info("A2DP Found sep: remote seid 0x%02x, in_use %d, media type %d, sep type %s, index %d",
                         sep.seid, sep.in_use, sep.media_type, sep.type == AVDTP_SOURCE ? "source" : "sink",
                         num_remote_seps);
                if (sep.type == AVDTP_SINK) {
                    remote_seps[num_remote_seps++] = sep;
                }
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE:
            cid = avdtp_subevent_signaling_sep_dicovery_done_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_DISCOVER_SEPS) break;

            connection = avdtp_get_connection_for_avdtp_cid(cid);
            btstack_assert(connection != NULL);

            if (num_remote_seps > 0){
                a2dp_source_state = A2DP_GET_CAPABILITIES;
                sc.active_remote_sep_index = 0;
            } else {
                if (outgoing_active){
                    outgoing_active = false;
                    connection = avdtp_get_connection_for_avdtp_cid(cid);
                    btstack_assert(connection != NULL);
                    a2dp_source_streaming_emit_connection_failed(connection, sc.local_stream_endpoint->sep.seid, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_NO_SUITABLE_CHANNEL_FOUND);
                }

                // continue
                a2dp_discover_seps_with_next_waiting_connection();
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:
            cid = avdtp_subevent_signaling_media_codec_sbc_capability_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state == A2DP_GET_CAPABILITIES) {

                log_info("A2DP received SBC capability, received: remote seid 0x%02x", avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet));

                // choose SBC config params
                uint8_t sampling_frequency = avdtp_choose_sbc_sampling_frequency(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet));
                uint8_t channel_mode       = avdtp_choose_sbc_channel_mode(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet));
                uint8_t block_length       = avdtp_choose_sbc_block_length(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet));
                uint8_t subbands           = avdtp_choose_sbc_subbands(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet));
                uint8_t allocation_method  = avdtp_choose_sbc_allocation_method(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet));
                uint8_t max_bitpool_value  = avdtp_choose_sbc_max_bitpool_value(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet));
                uint8_t min_bitpool_value  = avdtp_choose_sbc_min_bitpool_value(sc.local_stream_endpoint, avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet));

                // and pre-select this (safe) endpoint
                uint8_t local_seid = avdtp_stream_endpoint_seid(sc.local_stream_endpoint);
                remote_seid = avdtp_subevent_signaling_media_codec_sbc_capability_get_remote_seid(packet);
                a2dp_source_set_config_sbc(cid, local_seid, remote_seid, sampling_frequency, channel_mode, block_length, subbands, allocation_method, min_bitpool_value, max_bitpool_value);

                // forward codec capability
                a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY);
            }
            break;

        // forward codec capability
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY:
            cid = avdtp_subevent_signaling_media_codec_mpeg_audio_capability_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_GET_CAPABILITIES) break;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CAPABILITY);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY:
            cid = avdtp_subevent_signaling_media_codec_mpeg_aac_capability_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_GET_CAPABILITIES) break;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY:
            cid = avdtp_subevent_signaling_media_codec_atrac_capability_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_GET_CAPABILITIES) break;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CAPABILITY);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:
            cid = avdtp_subevent_signaling_media_codec_other_capability_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_GET_CAPABILITIES) break;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY);
            break;

        // not forwarded
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
        case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
        case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
        case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
        case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
        case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
            break;

        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            cid = avdtp_subevent_signaling_delay_reporting_capability_get_avdtp_cid(packet);
            log_info("received AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY, cid 0x%02x, state %d", cid, a2dp_source_state);
            
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_GET_CAPABILITIES) break;

            sc.local_stream_endpoint->remote_configuration_bitmap = store_bit16(sc.local_stream_endpoint->remote_configuration_bitmap, AVDTP_DELAY_REPORTING, 1);
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY);
            break;

        case AVDTP_SUBEVENT_SIGNALING_CAPABILITIES_DONE:
            cid = avdtp_subevent_signaling_capabilities_done_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_GET_CAPABILITIES) break;

            // forward capabilities done for endpoint
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CAPABILITIES_DONE);

            // endpoint was not suitable, check next one
            sc.active_remote_sep_index++;
            if (sc.active_remote_sep_index >= num_remote_seps){

                // emit 'all capabilities for all seps reported'
                uint8_t event[6];
                uint8_t pos = 0;
                event[pos++] = HCI_EVENT_A2DP_META;
                event[pos++] = sizeof(event) - 2;
                event[pos++] = A2DP_SUBEVENT_SIGNALING_CAPABILITIES_COMPLETE;
                little_endian_store_16(event, pos, cid);
                (*a2dp_source_packet_handler_user)(HCI_EVENT_PACKET, 0, event, sizeof(event));

                // do we have a valid config?
                if (sep_found_w2_set_configuration){
                    a2dp_source_state = A2DP_SET_CONFIGURATION;
                    sep_found_w2_set_configuration = false;
                    break;
                }

                // we didn't find a suitable SBC stream endpoint, sorry.
                if (outgoing_active){
                    outgoing_active = false;
                    connection = avdtp_get_connection_for_avdtp_cid(cid);
                    btstack_assert(connection != NULL);
                    a2dp_source_streaming_emit_connection_failed(connection, sc.local_stream_endpoint->sep.seid, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_NO_SUITABLE_CHANNEL_FOUND);
                }
                a2dp_discover_seps_with_next_waiting_connection();
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORT:
            cid = avdtp_subevent_signaling_delay_report_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_DELAY_REPORT);
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:
            cid = avdtp_subevent_signaling_media_codec_sbc_configuration_get_avdtp_cid(packet);

            if ((a2dp_source_cid == cid) && (a2dp_source_state == A2DP_W4_SET_CONFIGURATION)){
				// outgoing: discovery and config of remote sink sep successful, trigger stream open
				a2dp_source_state = A2DP_W2_OPEN_STREAM_WITH_SEID;
            } else {
				// incoming: accept cid and wait for stream open
				a2dp_source_cid = cid;
				a2dp_source_state = A2DP_W4_OPEN_STREAM_WITH_SEID;
            }

            // config set: stop timer
			a2dp_source_set_config_timer_stop();
            stream_endpoint_configured = true;

            sc.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sc.channel_mode = avdtp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            sc.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sc.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sc.allocation_method = avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            sc.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            sc.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            
            log_info("A2DP received SBC Config: sample rate %u, max bitpool %u., remote seid 0x%02x", sc.sampling_frequency, sc.max_bitpool_value, avdtp_subevent_signaling_media_codec_sbc_configuration_get_remote_seid(packet));
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION);
            break;

        // forward codec configuration
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION:
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AUDIO_CONFIGURATION);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION:
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION:
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_ATRAC_CONFIGURATION);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION);
            break;

        case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW: 
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW);
            break;
        
        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            cid = avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (a2dp_source_state != A2DP_W4_OPEN_STREAM_WITH_SEID) break;

			outgoing_active = false;
            status = avdtp_subevent_streaming_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                log_info("A2DP source streaming connection could not be established, avdtp_cid 0x%02x, status 0x%02x ---", cid, status);
                a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAM_ESTABLISHED);
                break;
            }

            log_info("A2DP source streaming connection established --- avdtp_cid 0x%02x, local seid 0x%02x, remote seid 0x%02x", cid, 
                avdtp_subevent_streaming_connection_established_get_local_seid(packet),
                avdtp_subevent_streaming_connection_established_get_remote_seid(packet));
            a2dp_source_state = A2DP_STREAMING_OPENED;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAM_ESTABLISHED);
            break;

        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:
            cid = avdtp_subevent_signaling_accept_get_avdtp_cid(packet);

			// reset discovery timer while remote is active
			if (avdtp_subevent_signaling_accept_get_is_initiator(packet) == 0) {
				uint16_t avdtp_cid = (uint16_t)(uintptr_t) btstack_run_loop_get_timer_context(&a2dp_source_set_config_timer);
				if ((avdtp_cid == 0) || (avdtp_cid != cid)) break;
				log_info("Reset discovery timer");
				a2dp_source_set_config_timer_start(avdtp_cid);
				break;
			}

            if (a2dp_source_cid != cid) break;

            signal_identifier = avdtp_subevent_signaling_accept_get_signal_identifier(packet);
            
            log_info("A2DP cmd %s accepted, global state %d, cid 0x%02x", avdtp_si2str(signal_identifier), a2dp_source_state, cid);

            switch (a2dp_source_state){
                case A2DP_GET_CAPABILITIES:
                    remote_seid = remote_seps[sc.active_remote_sep_index].seid;
                    log_info("A2DP get capabilities for remote seid 0x%02x", remote_seid);
                    avdtp_source_get_all_capabilities(cid, remote_seid);
                    return;

                case A2DP_SET_CONFIGURATION:
                    remote_seid = sc.local_stream_endpoint->set_config_remote_seid;
                    log_info("A2DP initiate set configuration locally and wait for response ... local seid 0x%02x, remote seid 0x%02x", avdtp_stream_endpoint_seid(sc.local_stream_endpoint), remote_seid);
                    a2dp_source_state = A2DP_W4_SET_CONFIGURATION;
                    avdtp_source_set_configuration(cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), remote_seid, sc.local_stream_endpoint->remote_configuration_bitmap, sc.local_stream_endpoint->remote_configuration);
                    return;

                case A2DP_W2_OPEN_STREAM_WITH_SEID:
                    log_info("A2DP open stream ... local seid 0x%02x, active remote seid 0x%02x", avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.local_stream_endpoint->remote_sep.seid);
                    a2dp_source_state = A2DP_W4_OPEN_STREAM_WITH_SEID;
                    avdtp_source_open_stream(cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.local_stream_endpoint->remote_sep.seid);
                    break;

                case A2DP_W2_RECONFIGURE_WITH_SEID:
                    log_info("A2DP reconfigured ... local seid 0x%02x, active remote seid 0x%02x", avdtp_stream_endpoint_seid(sc.local_stream_endpoint), sc.local_stream_endpoint->remote_sep.seid);
                    a2dp_signaling_emit_reconfigured(cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), ERROR_CODE_SUCCESS);
                    a2dp_source_state = A2DP_STREAMING_OPENED;
                    break;

                case A2DP_STREAMING_OPENED:
                    switch (signal_identifier){
                        case  AVDTP_SI_START:
                            a2dp_emit_stream_event(a2dp_source_packet_handler_user, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), A2DP_SUBEVENT_STREAM_STARTED);
                            break;
                        case AVDTP_SI_SUSPEND:
                            a2dp_emit_stream_event(a2dp_source_packet_handler_user, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), A2DP_SUBEVENT_STREAM_SUSPENDED);
                            break;
                        case AVDTP_SI_ABORT:
                        case AVDTP_SI_CLOSE:
                            a2dp_emit_stream_event(a2dp_source_packet_handler_user, cid, avdtp_stream_endpoint_seid(sc.local_stream_endpoint), A2DP_SUBEVENT_STREAM_STOPPED);
                            break;
                        default:
                            break;
                    }
                    break;

                default:
                    break;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_REJECT:
            cid = avdtp_subevent_signaling_reject_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (avdtp_subevent_signaling_reject_get_is_initiator(packet) == 0) break;

            a2dp_source_state = A2DP_CONNECTED;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_COMMAND_REJECTED);
            break;

        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            cid = avdtp_subevent_signaling_general_reject_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;
            if (avdtp_subevent_signaling_general_reject_get_is_initiator(packet) == 0) break;

            a2dp_source_state = A2DP_CONNECTED;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_COMMAND_REJECTED);
            break;

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
            cid = avdtp_subevent_streaming_connection_released_get_avdtp_cid(packet);
            if (a2dp_source_cid != cid) break;

            a2dp_source_state = A2DP_CONFIGURED;
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_STREAM_RELEASED);
            break;

        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            cid = avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet);
            // connect/release are passed on to app
            if (a2dp_source_cid == cid){
                stream_endpoint_configured = false;
                a2dp_source_state = A2DP_IDLE;
                a2dp_source_cid = 0;
            }
            a2dp_replace_subevent_id_and_emit_cmd(a2dp_source_packet_handler_user, packet, size, A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED);
            break;

        default:
            break;
    }
}
void a2dp_source_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);

    avdtp_source_register_packet_handler(&a2dp_source_packet_handler_internal);
    a2dp_source_packet_handler_user = callback;
}

void a2dp_source_init(void){
    avdtp_source_init();
}

avdtp_stream_endpoint_t * a2dp_source_create_stream_endpoint(avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type,
															 uint8_t * codec_capabilities, uint16_t codec_capabilities_len,
															 uint8_t * codec_configuration, uint16_t codec_configuration_len){
    avdtp_stream_endpoint_t * local_stream_endpoint = avdtp_source_create_stream_endpoint(AVDTP_SOURCE, media_type);
    if (!local_stream_endpoint){
        return NULL;
    }
    avdtp_source_register_media_transport_category(avdtp_stream_endpoint_seid(local_stream_endpoint));
    avdtp_source_register_media_codec_category(avdtp_stream_endpoint_seid(local_stream_endpoint), media_type, media_codec_type, 
        codec_capabilities, codec_capabilities_len);
	avdtp_source_register_delay_reporting_category(avdtp_stream_endpoint_seid(local_stream_endpoint));

	// store user codec configuration buffer
	local_stream_endpoint->media_codec_configuration_info = codec_configuration;
	local_stream_endpoint->media_codec_configuration_len  = codec_configuration_len;

    sc.local_stream_endpoint = local_stream_endpoint;
    return local_stream_endpoint;
}

void a2dp_source_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint){
    avdtp_source_finalize_stream_endpoint(stream_endpoint);
}

uint8_t a2dp_source_establish_stream(bd_addr_t remote_addr, uint8_t loc_seid, uint16_t * avdtp_cid){
    if (outgoing_active || stream_endpoint_configured) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    sc.local_stream_endpoint = avdtp_get_stream_endpoint_for_seid(loc_seid);
    if (!sc.local_stream_endpoint){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint16_t outgoing_cid;

    outgoing_active = true;
    uint8_t status = avdtp_source_connect(remote_addr, &outgoing_cid);
    if (status != ERROR_CODE_SUCCESS) {
        // if there's already a connection for for remote addr, avdtp_source_connect fails,
        // but the stream will get set-up nevertheless
        outgoing_active = false;
        return status;
    }

    // stop sep discovery for other
    if (a2dp_source_state != A2DP_IDLE){
        avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(a2dp_source_cid);
        if (connection != NULL){
            // sdp discovery has started: post-pone action, reserve sep discovery mechanism
            connection->a2dp_source_discover_seps = true;
        }
    }

    // setup state
    a2dp_source_state = A2DP_W4_CONNECTED;
    a2dp_source_cid   = outgoing_cid;
    *avdtp_cid = outgoing_cid;

    return ERROR_CODE_SUCCESS;
}

uint8_t a2dp_source_disconnect(uint16_t avdtp_cid){
    return avdtp_disconnect(avdtp_cid);
}

uint8_t a2dp_source_reconfigure_stream_sampling_frequency(uint16_t avdtp_cid, uint32_t sampling_frequency){
    btstack_assert(sc.local_stream_endpoint != NULL);

    if (a2dp_source_cid != avdtp_cid){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(avdtp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    } 

    if (a2dp_source_state != A2DP_STREAMING_OPENED) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    log_info("Reconfigure avdtp_cid 0x%02x", avdtp_cid);

    (void)memcpy(sc.local_stream_endpoint->media_codec_info,
                 sc.local_stream_endpoint->remote_sep.configuration.media_codec.media_codec_information,
                 4);

    // update sampling frequency
    uint8_t config = sc.local_stream_endpoint->media_codec_info[0] & 0x0f;
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
            log_error("Unsupported sampling frequency %u", (int) sampling_frequency);
            return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    sc.local_stream_endpoint->media_codec_info[0] = config;

    avdtp_capabilities_t new_configuration;
    new_configuration.media_codec.media_type = AVDTP_AUDIO;
    new_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
    new_configuration.media_codec.media_codec_information_len = 4;
    new_configuration.media_codec.media_codec_information = sc.local_stream_endpoint->media_codec_info;

    // start reconfigure
    a2dp_source_state = A2DP_W2_RECONFIGURE_WITH_SEID;

    return avdtp_source_reconfigure(
        avdtp_cid,
        avdtp_stream_endpoint_seid(sc.local_stream_endpoint),
        sc.local_stream_endpoint->remote_sep.seid,
        1 << AVDTP_MEDIA_CODEC,
        new_configuration
    );
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

int a2dp_source_stream_send_media_payload(uint16_t avdtp_cid, uint8_t local_seid, uint8_t * storage, int num_bytes_to_copy, uint8_t num_frames, uint8_t marker){
    return avdtp_source_stream_send_media_payload(avdtp_cid, local_seid, storage, num_bytes_to_copy, num_frames, marker);
}

uint8_t	a2dp_source_stream_send_media_packet(uint16_t a2dp_cid, uint8_t local_seid, const uint8_t * packet, uint16_t size){
    return avdtp_source_stream_send_media_packet(a2dp_cid, local_seid, packet, size);
}

static void avdtp_config_sbc_store(uint8_t * config, uint16_t sampling_frequency, avdtp_sbc_channel_mode_t channel_mode, uint8_t block_length, uint8_t subbands,
                                  avdtp_sbc_allocation_method_t  allocation_method, uint8_t min_bitpool_value, uint8_t max_bitpool_value) {
    config[0] = (sampling_frequency << 4) | channel_mode;
    config[1] = (block_length << 4) | (subbands << 2) | allocation_method;
    config[2] = min_bitpool_value;
    config[3] = max_bitpool_value;
}

static void avdtp_config_mpeg_audio_store(uint8_t * config, avdtp_mpeg_layer_t layer, uint8_t crc, avdtp_channel_mode_t channel_mode, uint8_t media_payload_format,
                                          uint16_t sampling_frequency, uint8_t vbr, uint8_t bit_rate_index){

    config[0] = (((uint8_t) layer) << 5) | ((crc & 0x01) << 4) | (1 << (channel_mode - AVDTP_CHANNEL_MODE_MONO));
    uint8_t sampling_frequency_index = 0;
    switch (sampling_frequency){
        case 16000:
            sampling_frequency_index = 5;
            break;
        case 22040:
            sampling_frequency_index = 4;
            break;
        case 24000:
            sampling_frequency_index = 3;
            break;
        case 32000:
            sampling_frequency_index = 2;
            break;
        case 44100:
            sampling_frequency_index = 1;
            break;
        case 48000:
            break;
    }
    config[1] = ((media_payload_format & 0x01) << 6) | (1 << sampling_frequency_index);
    uint16_t bit_rate_mask = 1 << bit_rate_index;
    config[2] = ((vbr & 0x01) << 7) | ((bit_rate_mask >> 8) & 0x3f);
    config[3] = bit_rate_mask & 0xff;
}

static void avdtp_config_mpeg_aac_store(uint8_t * config,  avdtp_aac_object_type_t object_type, uint32_t sampling_frequency, uint8_t channels, uint32_t bit_rate, uint8_t vbr) {
    config[0] = 1 << ((object_type - AVDTP_AAC_MPEG4_SCALABLE) + 4);
    uint16_t sampling_frequency_bitmap = 0;
    uint8_t i;
    const uint32_t aac_sampling_frequency_table[] = {
            96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000
    };
    for (i=0;i<12;i++){
        if (sampling_frequency == aac_sampling_frequency_table[i]){
            sampling_frequency_bitmap = 1 << i;
            break;
        }
    }
    config[1] = sampling_frequency_bitmap >> 4;
    uint8_t channels_bitmap = 0;
    switch (channels){
        case 1:
            channels_bitmap = 0x02;
            break;
        case 2:
            channels_bitmap = 0x01;
            break;
        default:
            break;
    }
    config[2] = ((sampling_frequency_bitmap & 0x0f) << 4) | (channels_bitmap << 2);
    config[3] = ((vbr & 0x01) << 7) | ((bit_rate >> 16) & 0x7f);
    config[4] = (bit_rate >> 8) & 0xff;
    config[5] =  bit_rate & 0xff;
}

static void avdtp_config_atrac_store(uint8_t * config, avdtp_atrac_version_t version, avdtp_channel_mode_t channel_mode, uint16_t sampling_frequency, uint8_t vbr,
                                     uint8_t bit_rate_index, uint16_t maximum_sul) {
    uint8_t channel_mode_bitamp = 0;
    switch (channel_mode){
        case AVDTP_CHANNEL_MODE_MONO:
            channel_mode_bitamp = 4;
            break;
        case AVDTP_CHANNEL_MODE_DUAL_CHANNEL:
            channel_mode_bitamp = 2;
            break;
        case AVDTP_CHANNEL_MODE_JOINT_STEREO:
            channel_mode_bitamp = 1;
            break;
        default:
            break;
    }
    config[0] = ((version - AVDTP_ATRAC_VERSION_1 + 1) << 5) | (channel_mode_bitamp << 2);
    uint8_t fs_bitmap = 0;
    switch (sampling_frequency){
        case 44100:
            fs_bitmap = 2;
            break;
        case 48000:
            fs_bitmap = 1;
            break;
        default:
            break;
    }
    uint32_t bit_rate_bitmap = 1 << (0x18 - bit_rate_index);
    config[1] = (fs_bitmap << 4) | ((vbr & 0x01) << 3) | ((bit_rate_bitmap >> 16) & 0x07);
    config[2] = (bit_rate_bitmap >> 8) & 0xff;
    config[3] = bit_rate_bitmap & 0xff;
    config[4] = maximum_sul >> 8;
    config[5] = maximum_sul & 0xff;
    config[6] = 0;
}

static void a2dp_source_config_init(uint8_t remote_seid, avdtp_media_codec_type_t codec_type, const uint8_t * codec_info, uint8_t codec_info_len){
    // set media configuration
    sc.local_stream_endpoint->remote_configuration_bitmap = store_bit16(sc.local_stream_endpoint->remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
    sc.local_stream_endpoint->remote_configuration.media_codec.media_type = AVDTP_AUDIO;
    sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
    // select reserved config buffer
    sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information = (uint8_t *) codec_info;
    sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information_len = codec_info_len;
    // store SBC configuration in reserved field
    sc.local_stream_endpoint->set_config_remote_seid = remote_seid;
    // suitable Sink SEP found, configure SEP
    sep_found_w2_set_configuration = true;
}

uint8_t a2dp_source_set_config_sbc(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, uint16_t sampling_frequency, avdtp_sbc_channel_mode_t channel_mode,
                                   uint8_t block_length, uint8_t subbands, avdtp_sbc_allocation_method_t  allocation_method, uint8_t min_bitpool_value, uint8_t max_bitpool_value){

    UNUSED(local_seid);

    if (a2dp_source_cid != a2dp_cid){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(a2dp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    a2dp_source_config_init(remote_seid, AVDTP_CODEC_SBC, sc.local_stream_endpoint->media_codec_info, 4);
    avdtp_config_sbc_store( sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information,
                            sampling_frequency, channel_mode, block_length, subbands, allocation_method, min_bitpool_value, max_bitpool_value);

    return ERROR_CODE_SUCCESS;
}

uint8_t a2dp_source_set_config_mpeg_audio(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, avdtp_mpeg_layer_t layer, uint8_t crc,
                                          avdtp_channel_mode_t channel_mode, uint8_t media_payload_format,
                                          uint16_t sampling_frequency, uint8_t vbr, uint8_t bit_rate_index){
    UNUSED(local_seid);

    if (a2dp_source_cid != a2dp_cid){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(a2dp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    a2dp_source_config_init(remote_seid, AVDTP_CODEC_MPEG_1_2_AUDIO, sc.local_stream_endpoint->media_codec_info, 4);
    avdtp_config_mpeg_audio_store( sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information,
                                   layer, crc, channel_mode, media_payload_format, sampling_frequency, vbr, bit_rate_index);

    return ERROR_CODE_SUCCESS;
}

uint8_t a2dp_source_set_config_mpeg_aac(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, avdtp_aac_object_type_t object_type,
                                        uint32_t sampling_frequency, uint8_t channels, uint32_t bit_rate, uint8_t vbr){
    UNUSED(local_seid);

    if (a2dp_source_cid != a2dp_cid){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(a2dp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    a2dp_source_config_init(remote_seid, AVDTP_CODEC_MPEG_2_4_AAC, sc.local_stream_endpoint->media_codec_info, 6);
    avdtp_config_mpeg_aac_store( sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information,
                                   object_type, sampling_frequency, channels, bit_rate, vbr);

    return ERROR_CODE_SUCCESS;
}

uint8_t a2dp_source_set_config_atrac(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid, avdtp_atrac_version_t version,
                                     avdtp_channel_mode_t channel_mode, uint16_t sampling_frequency, uint8_t vbr,
                                     uint8_t bit_rate_index, uint16_t maximum_sul){
    UNUSED(local_seid);

    if (a2dp_source_cid != a2dp_cid){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(a2dp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    a2dp_source_config_init(remote_seid, AVDTP_CODEC_ATRAC_FAMILY, sc.local_stream_endpoint->media_codec_info, 7);
    avdtp_config_atrac_store( sc.local_stream_endpoint->remote_configuration.media_codec.media_codec_information,
                                 version, channel_mode, sampling_frequency, vbr, bit_rate_index, maximum_sul);

    return ERROR_CODE_SUCCESS;
}

uint8_t a2dp_source_set_config_other(uint16_t a2dp_cid,  uint8_t local_seid, uint8_t remote_seid,
                                     const uint8_t * media_codec_information, uint8_t media_codec_information_len){
    if (a2dp_source_cid != a2dp_cid){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    avdtp_connection_t * connection = avdtp_get_connection_for_avdtp_cid(a2dp_cid);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    a2dp_source_config_init(remote_seid, AVDTP_CODEC_NON_A2DP, media_codec_information, media_codec_information_len);

    return ERROR_CODE_SUCCESS;
}
