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
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_IDLE,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_CONTROL_POINT_QUERY_CHARACTERISTIC_DESCRIPTOR,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_CHARACTERISTIC_DESCRIPTOR_RESULT,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_CONTROL_POINT_REGISTER_NOTIFICATION,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_CONTROL_POINT_NOTIFICATION_REGISTERED,

    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_ASE_ID_READ,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_ASE_ID_READ,

    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_CONNECTED,

    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT,

    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_ASE_READ,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_ASE_READ,

    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W2_ASE_WRITE,
    AUDIO_STREAM_CONTROL_SERVICE_CLIENT_STATE_W4_ASE_WRITTEN,
    
} audio_stream_control_service_client_state_t;

typedef struct {
    uint16_t value_handle;
    uint16_t client_configuration_handle; 
    uint16_t properties;
    uint16_t uuid16; 
    uint16_t end_handle;
    gatt_client_notification_t notification_listener;
} ascs_client_characteristic_t;

typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    uint16_t          mtu;
    audio_stream_control_service_client_state_t  state;
    
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    
    // used for memory capacity checking
    uint8_t  service_instances_num;
    uint8_t  streamendpoints_instances_num;
    
    uint8_t  max_streamendpoints_num;
    ascs_streamendpoint_t streamendpoints[ASCS_STREAMENDPOINTS_MAX_NUM];

    ascs_client_characteristic_t control_point;

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
 * @brief Init Audio Stream Control Service Client and register callback for events.
 * @param packet_handler
 */
void audio_stream_control_service_client_init(btstack_packet_handler_t packet_handler);

/**
 * @brief Connect to remote ASCS server and subscribe for notifications on audio stream endpoints (ASEs). 
 * The GATTSERVICE_SUBEVENT_ASCS_REMOTE_CLIENT_CONNECTED is emitted on complete.
 * If the status field of the  event is equal to ATT_ERROR_SUCCESS, the connection was successfully established. 
 * 
 * The memory storage for the connection struct and the array for storing found 
 * stream endpoint characteristics must be provided by the application. 
 * NOTE: The number of actual stream endpoint characteristics will be limited by the size of the streamendpoint_characteristics
 * array, i.e. by streamendpoint_characteristics_num input parameter.
 * 
 * @param  connection                            ascs_client_connection_t struct provided by application
 * @param  streamendpoint_characteristics        ascs_streamendpoint_characteristic_t array provided by application
 * @param  streamendpoint_characteristics_num    size of the streamendpoint_characteristics array
 * @param  con_handle
 * @param  ascs_cid
 * @return status ERROR_CODE_SUCCESS if the HCI connection with the given con_handle is found, otherwise
 *                ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER
 */
uint8_t audio_stream_control_service_client_connect(ascs_client_connection_t * connection, 
    ascs_streamendpoint_characteristic_t * streamendpoint_characteristics, 
    uint8_t streamendpoint_characteristics_num,
    hci_con_handle_t con_handle, uint16_t * ascs_cid);


/**
 * @brief Get audio streamendpoint (ASE) ID.
 * NOTE:  Code will assert if index is invalid.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index 
 * @return ASCS_ASE_ID_INVALID if no HCI connection with the given ascs_cid is found or ASE index is invalid, othewise streamendpoint ID
 */
uint8_t audio_stream_control_service_client_get_ase_id(uint16_t ascs_cid, uint8_t ase_index);

/**
 * @brief Get ASE role.
 * NOTE:  Code will assert if ID is invalid.
 * 
 * @param  ascs_cid
 * @param  ase_id 
 * @return LE_AUDIO_ROLE_INVALID if no HCI connection with the given ascs_cid is found or ase_id is invalid, otherwise see le_audio_role_t  
 */
le_audio_role_t audio_stream_control_service_client_get_ase_role(uint16_t ascs_cid, uint8_t ase_id);


/**
 * @brief Read the current state of the stream endpoint and get the corresponding stream endpoint state values.
 * The current state is received via the GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event.
 * The corresponding stream endpoint state values are subsequently received via the following events:
 * - GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION for the stream endpoint in the ASCS_STATE_CODEC_CONFIGURED state 
 * - GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION   for the stream endpoint in the ASCS_STATE_QOS_CONFIGURED state 
 * - GATTSERVICE_SUBEVENT_ASCS_METADATA            for the stream endpoint in the ASCS_STATE_ENABLING/STREAMING/DISABLING state 
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index 
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_read_streamendpoint(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Request a codec configuration with the server. 
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * Codec configuration notifications are received via the GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION event.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @param  codec_configuration
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_configure_codec(uint16_t ascs_cid, uint8_t streamendpoint_index, ascs_client_codec_configuration_request_t * codec_configuration);

/**
 * @brief Request a CIS configuration preference with the server and assign identifiers to the CIS. 
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * Quality of Service notifications are received via the GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION event.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @param  qos_configuration
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_configure_qos(uint16_t ascs_cid, uint8_t streamendpoint_index, ascs_qos_configuration_t * qos_configuration);

/**
 * @brief Request the server to enable an ASE and to provide any Metadata applicable for that ASE. 
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * Notifications on Metadata updates are received via the GATTSERVICE_SUBEVENT_ASCS_METADATA event.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_enable(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Inform the server acting as Audio Source that the client is ready to consume audio data transmitted by the server.
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
* The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 *
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_receiver_start_ready(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Inform the server acting as Audio Source that the client is ready to stop consuming audio data transmitted by the server.
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_receiver_stop_ready(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Request the server to release an ASE.
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @param  caching
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_release(uint16_t ascs_cid, uint8_t streamendpoint_index, bool caching);

/**
 * @brief Request the server to disable an ASE.
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 * 
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_disable(uint16_t ascs_cid, uint8_t streamendpoint_index);


/**
 * @brief Inform the server that ASE is released.
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 *  
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
 */
uint8_t audio_stream_control_service_client_streamendpoint_released(uint16_t ascs_cid, uint8_t streamendpoint_index);

/**
 * @brief Provide the server with Metadata to be applied to an ASE. 
 * The GATTSERVICE_SUBEVENT_ASCS_CONTROL_POINT_OPERATION_RESPONSE event informs on the status of the operation.
 * Notifications on Metadata updates are received via the GATTSERVICE_SUBEVENT_ASCS_METADATA event.
 * The GATTSERVICE_SUBEVENT_ASCS_STREAMENDPOINT_STATE event provides the current state of the stream endpoint.
 *  
 * @param  ascs_cid
 * @param  streamendpoint_index
 * @param  metadata
 * @return status ERROR_CODE_SUCCESS if the request is successfully registered, otherwise:
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER  if no HCI connection with the given ascs_cid is found 
 *                - ERROR_CODE_COMMAND_DISALLOWED if the connection is in wrong state
 *                - ERROR_CODE_CONTROLLER_BUSY    if there is ongoing write or read
 *                - ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE if stream endpoint index excesses the number of stored elements
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

