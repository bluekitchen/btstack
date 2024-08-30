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
 * @title Audio Input Control Service Client
 * 
 */

#ifndef MICROPHONE_CONTROL_SERVICE_CLIENT_H
#define MICROPHONE_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/audio_input_control_service_client.h"

#if defined __cplusplus
extern "C" {
#endif

// number of characteristics in Media Control Service
#define MICROPHONE_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS 1

/** 
 * @text The Media Control Service Client 
 */
typedef enum {
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_IDLE = 0,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CONNECTION,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_INCLUDED_SERVICES,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICES_RESULT,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_INCLUDED_SERVICE_CONNECTED,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_READY,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT
} microphone_service_client_state_t;

typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    btstack_packet_handler_t packet_handler;

    microphone_service_client_state_t state;

    // Used for read characteristic queries
    uint8_t characteristic_index;
    // Used to store parameters for write characteristic
    union {
        uint8_t  data_8;
        uint16_t data_16;
        uint32_t data_32;
        const char * data_string;
    } data;

    uint8_t write_buffer[1];

    aics_client_connection_t  * aics_connections_storage;
    uint8_t aics_connections_max_num;
    uint8_t aics_connections_num;
    uint8_t aics_connections_connected;
    uint8_t aics_connections_index;
    gatt_service_client_characteristic_t * aics_characteristics_storage;
    uint8_t aics_characteristics_max_num;

    btstack_context_callback_registration_t gatt_query_can_send_now_request;

} mics_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Media Control Service. 
 */
void microphone_control_service_client_init(void);

 /**
 * @brief Connect to a Media Control Service instance of remote device. The client will automatically register for notifications.
 * The mute state is received via LEAUDIO_SUBEVENT_MCS_CLIENT_MUTE event.
 * The mute state can be 0 - off, 1 - on, 2 - disabeled and it is valid if the ATT status is equal to ATT_ERROR_SUCCESS,
 * see ATT errors (see bluetooth.h) for other values.
 *   
 * Event LEAUDIO_SUBEVENT_MCS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no audio input control service is found, or ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param packet_handler
 * @param mics_connection
 * @param mics_characteristics_storage           storage for characteristics
 * @param mics_characteristics_num           >= MEDIA_CONTROL_SERVICE_CLIENT_NUM_CHARACTERISTICS
 * @param aics_client
 * @param aics_connections
 * @param aics_connections_num
 * @param aics_characteristics_storage
 * @param aics_characteristics_num
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t microphone_control_service_client_connect(
         hci_con_handle_t con_handle,
         btstack_packet_handler_t packet_handler,
         mics_client_connection_t * mics_connection,
         gatt_service_client_characteristic_t * mics_characteristics_storage, uint8_t mics_characteristics_num,
         aics_client_connection_t * aics_connections, uint8_t aics_connections_num,
         gatt_service_client_characteristic_t * aics_characteristics_storage, uint8_t aics_characteristics_num, // for all aics connections
         uint16_t * mics_cid
);

//uint8_t microphone_control_service_client_connect_aics_services(uint16_t mics_cid);

/**
 * @brief Read mute state. The mute state is received via LEAUDIO_SUBEVENT_MICS_CLIENT_MUTE event.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_read_mute_state(uint16_t mics_cid);

/**
 * @brief Turn on mute.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_mute(uint16_t mics_cid);

uint8_t microphone_control_service_client_write_gain_setting(uint16_t mics_cid, uint8_t aics_index, int8_t gain_setting);
uint8_t microphone_control_service_client_write_manual_gain_mode(uint16_t mics_cid, uint8_t aics_index);
uint8_t microphone_control_service_client_write_automatic_gain_mode(uint16_t mics_cid, uint8_t aics_index);

uint8_t microphone_control_service_client_read_input_description(uint16_t mics_cid, uint8_t aics_index);
uint8_t microphone_control_service_client_write_input_description(uint16_t mics_cid, uint8_t aics_index, const char * audio_input_description);

uint8_t microphone_control_service_client_read_input_state(uint16_t mics_cid, uint8_t aics_index);
uint8_t microphone_control_service_client_read_gain_setting_properties(uint16_t mics_cid, uint8_t aics_index);
uint8_t microphone_control_service_client_read_input_type(uint16_t mics_cid, uint8_t aics_index);
uint8_t microphone_control_service_client_read_input_status(uint16_t mics_cid, uint8_t aics_index);

uint8_t microphone_control_service_client_write_mute(uint16_t mics_cid, uint8_t aics_index);
uint8_t microphone_control_service_client_write_unmute(uint16_t mics_cid, uint8_t aics_index);
/**
 * @brief Turn off mute.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_unmute(uint16_t mics_cid);

/**
 * @brief Disconnect.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_disconnect(uint16_t mics_cid);

/**
 * @brief Disconnect.
 * @param aics_cid
 * @return status
 */
uint8_t microphone_control_service_client_disconnect(uint16_t mics_cid);

/**
 * @brief De-initialize Media Control Service. 
 */
void microphone_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
