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
#include "ble/gatt-service/broadcast_audio_scan_service_util.h"
#include "btstack_defines.h"
#include "le_audio.h"

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
    uint8_t source_id;
    bool    in_use;
    le_audio_pa_sync_t pa_sync;

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
 */
void    broadcast_audio_scan_service_client_init(btstack_packet_handler_t packet_handler);

uint8_t broadcast_audio_scan_service_client_connect(bass_client_connection_t * connection, bass_client_source_t * sources, uint8_t num_sources, hci_con_handle_t con_handle, uint16_t * bass_cid);

uint8_t broadcast_audio_scan_service_client_scanning_started(uint16_t bass_cid);

uint8_t broadcast_audio_scan_service_client_scanning_stopped(uint16_t bass_cid);

uint8_t broadcast_audio_scan_service_client_add_source(uint16_t bass_cid, const bass_source_data_t * add_source_data);

uint8_t broadcast_audio_scan_service_client_modify_source(uint16_t bass_cid, uint8_t source_id, const bass_source_data_t * modify_source_data);

uint8_t broadcast_audio_scan_service_client_set_broadcast_code(uint16_t bass_cid, uint8_t source_id, const uint8_t * broadcast_code);

uint8_t broadcast_audio_scan_service_client_remove_source(uint16_t bass_cid, uint8_t source_id);


/**
 * @brief Deinit Broadcast Audio Scan Service Client
 */
void broadcast_audio_scan_service_client_deinit(uint16_t bass_cid);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

