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

#ifndef BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_H
#define BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_H

#include <stdint.h>
#include "ble/gatt_client.h"
#include "btstack_defines.h"
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/broadcast_audio_scan_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

#define BASS_CLIENT_MAX_ATT_BUFFER_SIZE             512
/* API_START */

typedef enum {
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_IDLE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,

    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_STATE_CONNECTED,

    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT,

    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_START_SCAN,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_START_SCAN,
    
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_STOP_SCAN,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_STOP_SCAN,
    
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_ADD_SOURCE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_ADD_SOURCE,
    
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_MODIFY_SOURCE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_MODIFY_SOURCE,
    
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_SET_BROADCAST_CODE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_SET_BROADCAST_CODE,
    
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_WRITE_CONTROL_POINT_REMOVE_SOURCE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_WRITE_CONTROL_POINT_REMOVE_SOURCE,

    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W2_READE_RECEIVE_STATE,
    BROADCAST_AUDIO_SCAN_SERVICE_CLIENT_W4_READE_RECEIVE_STATE,

} broadcast_audio_scan_service_client_state_t;

typedef struct {
    // used for add source command
    bass_source_data_t data;

    // received via notification
    bool    in_use;
    uint8_t source_id;
    le_audio_big_encryption_t big_encryption;
    uint8_t  bad_code[16];

    // characteristic
    uint16_t receive_state_value_handle;
    uint16_t receive_state_ccc_handle;
    uint16_t receive_state_properties;
} bass_client_source_t;


typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    uint16_t          mtu;
    broadcast_audio_scan_service_client_state_t  state;
    
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t control_point_value_handle; 
    
    // used for memory capacity checking
    uint8_t  service_instances_num;
    uint8_t  receive_states_instances_num;
    // used for notification registration
    uint8_t  receive_states_index;
    
    uint8_t max_receive_states_num;
    bass_client_source_t * receive_states;

    // used for write segmentation
    uint8_t  buffer[BASS_CLIENT_MAX_ATT_BUFFER_SIZE];
    uint16_t buffer_offset;
    uint16_t data_size;

    gatt_client_notification_t notification_listener;
    
    // used for adding and modifying source
    const bass_source_data_t * control_point_operation_data;
    uint8_t control_point_operation_source_id;
    // used for setting the broadcast code
    const uint8_t * broadcast_code;
} bass_client_connection_t;

/**
 * @brief Init Broadcast Audio Scan Service Client
 * @param packet_handler for events
 */
void    broadcast_audio_scan_service_client_init(btstack_packet_handler_t packet_handler);

/**
 * @brief Connect to BASS Service on remote device
 * @note GATTSERVICE_SUBEVENT_BASS_CONNECTED will be emitted
 * @param connection struct provided by user, needs to stay valid until disconnect event is received
 * @param sources buffer to store information on Broadcast Sources on the service
 * @param num_sources
 * @param con_handle to connect to
 * @param bass_cid connection id for this connection for other functions
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_connect(bass_client_connection_t * connection, bass_client_source_t * sources, uint8_t num_sources, hci_con_handle_t con_handle, uint16_t * bass_cid);

/**
 * @brief Notify BASS Service that scanning has started
 * @param bass_cid
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_scanning_started(uint16_t bass_cid);

/**
 * @brief Notify BASS Service that scanning has stopped
 * @note emits GATTSERVICE_SUBEVENT_BASS_SOURCE_OPERATION_COMPLETE
 * @param bass_cid
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_scanning_stopped(uint16_t bass_cid);

/**
 * @brief Add Broadcast Source on service
 * @note GATTSERVICE_SUBEVENT_BASS_NOTIFICATION_COMPLETE will contain source_id for other functions
 * @param bass_cid
 * @param add_source_data data to add, needs to stay valid until GATTSERVICE_SUBEVENT_BASS_SOURCE_OPERATION_COMPLETE
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_add_source(uint16_t bass_cid, const bass_source_data_t * add_source_data);

/**
 * @brief Modify information about Broadcast Source on service
 * @param bass_cid
 * @param source_id
 * @param modify_source_data data to modify, needs to stay valid until GATTSERVICE_SUBEVENT_BASS_SOURCE_OPERATION_COMPLETE
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_modify_source(uint16_t bass_cid, uint8_t source_id, const bass_source_data_t * modify_source_data);

/**
 * @brief Set Broadcast Code for a Broadcast Source to allow remote do decrypt audio stream
 * @param bass_cid
 * @param source_id
 * @param broadcast_code
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_set_broadcast_code(uint16_t bass_cid, uint8_t source_id, const uint8_t * broadcast_code);

/**
 * @brief Remove information about Broadcast Source
 * @param bass_cid
 * @param source_id
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_remove_source(uint16_t bass_cid, uint8_t source_id);

/**
 * @param Provide read-only access to Broadcast Receive State of given Broadcast Source on service
 * @param bass_cid
 * @param source_id
 * @return pointer to source data or NULL, if source_id not found
 */
const bass_source_data_t * broadcast_audio_scan_service_client_get_source_data(uint16_t bass_cid, uint8_t source_id);

/**
 * @param Get BIG Encryption and Bad Code from Broadcast Receive State of given Broadcast Source on service
 * @param bass_cid
 * @param source_id
 * @param out_big_encryption
 * @param out_bad_code 16-byte buffer
 * @return status
 */
uint8_t broadcast_audio_scan_service_client_get_encryption_state(uint16_t bass_cid, uint8_t source_id,
                                                                 le_audio_big_encryption_t * out_big_encryption, uint8_t * out_bad_code);

/**
 * @brief Deinit Broadcast Audio Scan Service Client
 */
void broadcast_audio_scan_service_client_deinit(uint16_t bass_cid);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

