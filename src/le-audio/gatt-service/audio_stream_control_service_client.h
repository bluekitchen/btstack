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
 * @title Audio Stream Control Service Client
 * 
 */

#ifndef AUDIO_STREAM_CONTROL_SERVICE_CLIENT_H
#define AUDIO_STREAM_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/audio_stream_control_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
typedef enum {
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_IDLE,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,

    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_CONNECTED,

    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT,

    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_READ,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_ASE_READ,

    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W2_ASE_WRITE,
    AUDIO_STREAM_CONTROL_SERVICE_SERVICE_CLIENT_STATE_W4_ASE_WRITTEN,
    
} audio_stream_control_service_service_client_state_t;

typedef struct {
    uint16_t value_handle;
    uint16_t ccc_handle; 
    uint16_t properties;
    uint16_t uuid16; 
    uint16_t end_handle;
} ascs_client_characteristic_t;

typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    uint16_t          mtu;
    audio_stream_control_service_service_client_state_t  state;
    
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    
    // used for memory capacity checking
    uint8_t  service_instances_num;
    uint8_t  streamendpoints_instances_num;
    
    uint8_t  max_streamendpoints_num;
    ascs_streamendpoint_t streamendpoints[ASCS_STREAMENDPOINTS_MAX_NUM];

    uint16_t control_point_value_handle;

    // used for notification registration and read/write requests
    uint8_t  streamendpoints_index;
    
    // used for write requests
    ascs_client_codec_configuration_request_t * codec_configuration;
    ascs_qos_configuration_t   * qos_configuration;
    le_audio_metadata_t        * metadata;
    ascs_opcode_t              command_opcode;

} ascs_client_connection_t;

/* API_START */

/**
 * @brief Init Audio Stream Control Service Client
 */
void audio_stream_control_service_client_init(btstack_packet_handler_t packet_handler);

uint8_t audio_stream_control_service_service_client_connect(ascs_client_connection_t * connection, 
    ascs_streamendpoint_characteristic_t * streamendpoint_characteristics, 
    uint8_t streamendpoint_characteristics_num,
    hci_con_handle_t con_handle, uint16_t * ascs_cid);

uint8_t audio_stream_control_service_service_client_read_streamendpoint(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Configure codec.
 * @param ascs_cid
 * @param ase_id
 * @param codec_configuration
 */
uint8_t audio_stream_control_service_client_streamendpoint_configure_codec(uint16_t ascs_cid, uint8_t streamendpoint_index, ascs_client_codec_configuration_request_t * codec_configuration);

/**
 * @brief Configure QoS.
 * @param ascs_cid
 * @param ase_id
 * @param qos_configuration
 */
uint8_t audio_stream_control_service_client_streamendpoint_configure_qos(uint16_t ascs_cid, uint8_t streamendpoint_index, ascs_qos_configuration_t * qos_configuration);

/**
 * @brief Enable streamendpoint.
 * @param ascs_cid
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_enable(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Start stream.
 * @param ascs_cid
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_receiver_start_ready(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Stop stream.
 * @param ascs_cid
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_receiver_stop_ready(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Release stream.
 * @param ascs_cid
 * @param ase_id
 * @param caching
 */
uint8_t audio_stream_control_service_client_streamendpoint_release(uint16_t ascs_cid, uint8_t streamendpoint_index, bool caching);

/**
 * @brief Disable streamendpoint.
 * @param ascs_cid
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_disable(uint16_t ascs_cid, uint8_t streamendpoint_index);


/**
 * @brief Disabled streamendpoint.
 * @param ascs_cid
 * @param ase_id
 */
uint8_t audio_stream_control_service_client_streamendpoint_released(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Update Metadata.
 * @param ascs_cid
 * @param ase_id
 * @param metadata_configuration
 */
uint8_t audio_stream_control_service_client_streamendpoint_metadata_update(uint16_t ascs_cid, uint8_t streamendpoint_index, le_audio_metadata_t * metadata);


/**
 * @brief Deinit Audio Stream Control Service Client
 */
void audio_stream_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

