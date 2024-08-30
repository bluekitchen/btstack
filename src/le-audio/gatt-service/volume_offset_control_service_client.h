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
 * @title Volume Offset Control Service Client
 * 
 */

#ifndef VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_H
#define VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/volume_offset_control_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Media Control Service Client 
 */
typedef enum {
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_IDLE = 0,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTED,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHANGE_COUNTER,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_CHANGE_COUNTER_RESULT,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_CHANGE_COUNTER_RESULT_READ_FAILED,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_READY,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    VOLUME_OFFSET_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT
} volume_offset_service_client_state_t;

typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    btstack_packet_handler_t packet_handler;

    volume_offset_service_client_state_t state;

    // Used for read characteristic queries
    uint8_t characteristic_index;

    // Used to store parameters for write characteristic
    union {
        uint8_t      data_bytes[4];
        const char * data_string;
    } data;
    uint8_t  change_counter;

    gatt_service_client_characteristic_t characteristics_storage[VOLUME_OFFSET_CONTROL_SERVICE_NUM_CHARACTERISTICS];
    
    btstack_context_callback_registration_t gatt_query_can_send_now;
} vocs_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Volume Offset Control Service. 
 */
void volume_offset_control_service_client_init(void);

 /**
 * @brief Connect to a Volume Offset Control Service instance of remote device. The client will automatically register for notifications.
 *   
 * Event LEAUDIO_SUBEVENT_VOCS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no audio input control service is found, or ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param packet_handler
 * @param service_start_handle
 * @param service_end_handle
 * @param service_index
 * @param connection
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t volume_offset_control_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    uint16_t service_start_handle, 
    uint16_t service_end_handle, 
    uint8_t service_index, 
    vocs_client_connection_t * connection);

uint8_t volume_offset_control_service_client_write_volume_offset(vocs_client_connection_t * connection, int16_t volume_offset);
uint8_t volume_offset_control_service_client_write_audio_location(vocs_client_connection_t * connection, uint32_t audio_location);

uint8_t volume_offset_control_service_client_read_offset_state(vocs_client_connection_t * connection);
uint8_t volume_offset_control_service_client_read_audio_location(vocs_client_connection_t * connection);
uint8_t volume_offset_control_service_client_read_audio_output_description(vocs_client_connection_t * connection);


/**
 * @brief Disconnect.
 * @param connection
 * @return status
 */
uint8_t volume_offset_control_service_client_disconnect(vocs_client_connection_t * connection);


/**
 * @brief De-initialize Media Control Service. 
 */
void volume_offset_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
