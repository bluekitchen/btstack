/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title Audio Stream Sontrol Service Server (ASCS)
 * 
 */

#ifndef AUDIO_STREAM_CONTROL_SERVICE_SERVER_H
#define AUDIO_STREAM_CONTROL_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/audio_stream_control_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

#ifndef ASCS_SERVER_WRITE_LONG_MAX
#define ASCS_SERVER_WRITE_LONG_MAX 100
#endif

/* API_START */

/**
 * @text The Audio Stream Sontrol Service Server exposes an interface fo Audio Stream Endpoints (ASEs). This enables clients
 * to discover, configure, establish and control the ASEs and their unicast Audio Streams.
 * 
 * To use with your application, add `#import <audio_stream_control_service.gatt>` to your .gatt file. 
 */


typedef struct {
    hci_con_handle_t con_handle;
    ascs_streamendpoint_t streamendpoints[ASCS_STREAMENDPOINTS_MAX_NUM];

    uint8_t  write_long_buffer[ASCS_SERVER_WRITE_LONG_MAX];
    uint16_t write_long_length;

    // High priority Control Point Operation responses - assumed that are handled serially
    // followed by lower priority characteristic value changes, handled asynchronously
    uint8_t scheduled_tasks;
    btstack_context_callback_registration_t scheduled_tasks_callback;  
    uint16_t ase_control_point_client_configuration;

    // store response for control point operation
    ascs_opcode_t response_opcode;
    uint8_t       response_ases_num;
    ascs_control_point_operation_response_t response[ASCS_STREAMENDPOINTS_MAX_NUM];
} ascs_server_connection_t;


/**
 * @brief Init Audio Stream Control Service Server with ATT DB
 */
void audio_stream_control_service_server_init(
        const uint8_t streamendpoint_characteristics_num, ascs_streamendpoint_characteristic_t * streamendpoint_characteristics,
        const uint8_t clients_num, ascs_server_connection_t * clients);

/**
 * @brief Register packet handler to receive events:
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_CONNECTED
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_DISCONNECTED
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_CODEC_CONFIGURATION
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_QOS_CONFIGURATION
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_METADATA
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_START_READY
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_DISABLE
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_STOP_READY
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASE
 * - LEAUDIO_SUBEVENT_ASCS_SERVER_RELEASED
 * @param packet_handler
 */
void audio_stream_control_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * @brief Configure codec.
 * @param con_handle
 * @param ase_id
 * @param codec_configuration
 */
void audio_stream_control_service_server_streamendpoint_configure_codec(hci_con_handle_t con_handle, uint8_t ase_id, const ascs_codec_configuration_t *codec_configuration);

/**
 * @brief Configure QoS.
 * @param con_handle
 * @param ase_id
 * @param qos_configuration
 */
void audio_stream_control_service_server_streamendpoint_configure_qos(hci_con_handle_t con_handle, uint8_t ase_id, const ascs_qos_configuration_t *qos_configuration);

/**
 * @brief Enable streamendpoint.
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_enable(hci_con_handle_t con_handle, uint8_t ase_id, const le_audio_metadata_t *metadata);

/**
 * @brief Start stream.
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_receiver_start_ready(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Stop stream.
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_receiver_stop_ready(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Release stream.
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_release(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Release stream.
 * @param con_handle
 * @param ase_id
 * @param caching
 */
void audio_stream_control_service_server_streamendpoint_released(hci_con_handle_t con_handle, uint8_t ase_id, bool caching);

/**
 * @brief Disable streamendpoint.
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_disable(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Update Metadata.
 * @param con_handle
 * @param ase_id
 * @param metadata_configuration
 */
void audio_stream_control_service_server_streamendpoint_metadata_update(hci_con_handle_t con_handle, uint8_t ase_id, const le_audio_metadata_t *metadata);

/**
 * @brief Release streamendpoint.
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_release(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Report Accepted CIS request
 * @param acl_handle
 * @param cis_handle
 * @param cig_id
 * @param cis_id
 */
void audio_stream_control_service_server_streamendpoint_cis_accepted(hci_con_handle_t acl_handle, hci_con_handle_t cis_handle, uint8_t cig_id, uint8_t cis_id);

/**
 * @brief Report CIS loss for a streamendpoint
 * @param con_handle
 * @param ase_id
 */
void audio_stream_control_service_server_streamendpoint_cis_lost(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Get CIS Connection Handle for streamendpoint
 * @param con_handle
 * @param ase_id
 * @return cis handle or HCI_CON_HANDLE_INVALID
 */
hci_con_handle_t audio_stream_control_service_server_streamendpoint_cis_get_handle(hci_con_handle_t con_handle, uint8_t ase_id);

/**
 * @brief Set available audio context mask. Used to verify Enable or Metadata update that specify audio contexts
 * @param available_sink_audio_contexts_mask
 * @param available_source_audio_contexts_mask
 */
void audio_stream_control_service_server_set_available_audio_contexts(
        uint16_t available_sink_audio_contexts_mask,
        uint16_t available_source_audio_contexts_mask);

/**
 * @brief Deinit Audio Stream Control Service Server
 */
void audio_stream_control_service_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
