/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 * @title Volume Control Service Client
 * 
 */

#ifndef VOLUME_CONTROL_SERVICE_CLIENT_H
#define VOLUME_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/volume_control_service_util.h"
#include "le-audio/gatt-service/audio_input_control_service_client.h"
#include "le-audio/gatt-service/volume_offset_control_service_client.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Volume Control Service Client 
 */
typedef enum {
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_IDLE = 0,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_RESULT_READ_FAILED,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_AICS_SERVICES_CONNECTED,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_VOCS_SERVICES_CONNECTED,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_READY,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    VOLUME_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT
} volume_control_service_client_state_t;


typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    btstack_packet_handler_t packet_handler;

    volume_control_service_client_state_t state;

    // btstack_packet_handler_t client_handler;
    // gatt_client_notification_t notification_listener;

    // Used for read characteristic queries
    uint8_t characteristic_index;
    // Used to store parameters for write characteristic
    union {
        uint8_t  data_8;
        uint16_t data_16;
        uint32_t data_32;
        uint8_t  data_bytes[4];
        const char * data_string;
    } data;

    uint8_t write_buffer[1];
    uint8_t change_counter;

    gatt_service_client_characteristic_t characteristics_storage[VOLUME_CONTROL_SERVICE_NUM_CHARACTERISTICS];

    aics_client_connection_t  * aics_connections_storage;
    uint8_t aics_connections_max_num;
    uint8_t aics_connections_num;
    uint8_t aics_connections_connected;
    uint8_t aics_connections_index;

    vocs_client_connection_t  * vocs_connections_storage;
    uint8_t vocs_connections_max_num;
    uint8_t vocs_connections_num;
    uint8_t vocs_connections_connected;
    uint8_t vocs_connections_index;

} vcs_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Volume Control Service. 
 */
void volume_control_service_client_init(void);

 /**
 * @brief Connect to a Volume Control Service instance of remote device. The client will automatically register for notifications.
 * Event LEAUDIO_SUBEVENT_VCS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no audio input control service is found, or ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param packet_handler
 * @param vcs_connection
 * @param aics_connections
 * @param vocs_connections
 * @param out_vcs_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t volume_control_service_client_connect(
         hci_con_handle_t con_handle,
         btstack_packet_handler_t packet_handler,
         vcs_client_connection_t * vcs_connection,
         aics_client_connection_t * aics_connections, uint8_t aics_connections_num,
         vocs_client_connection_t * vocs_connections, uint8_t vocs_connections_num,
         uint16_t * out_vcs_cid
);


/**
 * @brief Read volume state. The volume state is received via LEAUDIO_SUBEVENT_VCS_CLIENT_VOLUME_STATE event.
 * @param vcs_cid
 * @return status
 */
uint8_t volume_control_service_client_read_volume_state(uint16_t vcs_cid);
uint8_t volume_control_service_client_read_volume_flags(uint16_t vcs_cid);

/**
 * @brief Turn on mute.
 * @param vcs_cid
 * @return status
 */
uint8_t volume_control_service_client_mute(uint16_t vcs_cid);
/**
 * @brief Turn off mute.
 * @param vcs_cid
 * @return status
 */
uint8_t volume_control_service_client_unmute(uint16_t vcs_cid);


// VCS Control Point procedures

uint8_t volume_control_service_client_relative_volume_up(uint16_t vcs_cid);
uint8_t volume_control_service_client_relative_volume_down(uint16_t vcs_cid);
uint8_t volume_control_service_client_unmute_relative_volume_up(uint16_t vcs_cid);
uint8_t volume_control_service_client_unmute_relative_volume_down(uint16_t vcs_cid);
uint8_t volume_control_service_client_set_absolute_volume(uint16_t vcs_cid, uint8_t abs_volume);

// Wrap AICS service API
uint8_t volume_control_service_client_write_gain_setting(uint16_t vcs_cid, uint8_t aics_index, int8_t gain_setting);
uint8_t volume_control_service_client_write_manual_gain_mode(uint16_t vcs_cid, uint8_t aics_index);
uint8_t volume_control_service_client_write_automatic_gain_mode(uint16_t vcs_cid, uint8_t aics_index);

uint8_t volume_control_service_client_read_input_description(uint16_t vcs_cid, uint8_t aics_index);
uint8_t volume_control_service_client_write_input_description(uint16_t vcs_cid, uint8_t aics_index, const char * audio_input_description);

uint8_t volume_control_service_client_read_input_state(uint16_t vcs_cid, uint8_t aics_index);
uint8_t volume_control_service_client_read_gain_setting_properties(uint16_t vcs_cid, uint8_t aics_index);
uint8_t volume_control_service_client_read_input_type(uint16_t vcs_cid, uint8_t aics_index);
uint8_t volume_control_service_client_read_input_status(uint16_t vcs_cid, uint8_t aics_index);

uint8_t volume_control_service_client_write_mute(uint16_t vcs_cid, uint8_t aics_index);
uint8_t volume_control_service_client_write_unmute(uint16_t vcs_cid, uint8_t aics_index);

// Wrap VOCS service API
uint8_t volume_control_service_client_write_volume_offset(uint16_t vcs_cid, uint8_t vocs_index, int16_t volume_offset);
uint8_t volume_control_service_client_write_audio_location(uint16_t vcs_cid, uint8_t vocs_index, uint32_t audio_location);
uint8_t volume_control_service_client_read_offset_state(uint16_t vcs_cid, uint8_t vocs_index);
uint8_t volume_control_service_client_read_audio_location(uint16_t vcs_cid, uint8_t vocs_index);
uint8_t volume_control_service_client_read_audio_output_description(uint16_t vcs_cid, uint8_t vocs_index);


/**
 * @brief Disconnect.
 * @param vcs_cid
 * @return status
 */
uint8_t volume_control_service_client_disconnect(uint16_t vcs_cid);

/**
 * @brief Disconnect.
 * @param aics_cid
 * @return status
 */
uint8_t volume_control_service_client_disconnect(uint16_t vcs_cid);

/**
 * @brief De-initialize Media Control Service. 
 */
void volume_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
