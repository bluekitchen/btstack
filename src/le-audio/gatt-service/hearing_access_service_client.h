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
 * @title Hearing Access Service Client
 * 
 */

#ifndef HEARING_ACCESS_SERVICE_CLIENT_H
#define HEARING_ACCESS_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/hearing_access_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Hearing Access Service Client 
 */
typedef enum {
    HEARING_ACCESS_SERVICE_CLIENT_STATE_IDLE = 0,
    HEARING_ACCESS_SERVICE_CLIENT_STATE_W4_CONNECTION,
    HEARING_ACCESS_SERVICE_CLIENT_STATE_READY,
    HEARING_ACCESS_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    HEARING_ACCESS_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    HEARING_ACCESS_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    HEARING_ACCESS_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT
} hearing_access_service_client_state_t;


typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    hearing_access_service_client_state_t state;

    // btstack_packet_handler_t client_handler;
    // gatt_client_notification_t notification_listener;

    // Used for read characteristic queries
    uint8_t characteristic_index;
    // Used to store parameters for write characteristic
    union {
        uint8_t  data_bytes[3];
        const char * data_string;
    } data;

    uint8_t write_buffer[1];
    
    gatt_service_client_characteristic_t characteristics_storage[HEARING_ACCESS_SERVICE_NUM_CHARACTERISTICS];

    // Application packet handler
    btstack_packet_handler_t events_packet_handler;
} has_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Hearing Access Service. 
 */
void hearing_access_service_client_init(void);

 /**
 * @brief Connect to a Hearing Access Service instance of remote device. The client will automatically register for notifications.
 * Event LEAUDIO_SUBEVENT_HAS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no audio input control service is found, or ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param packet_handler
 * @param connection
 * @param out_has_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t hearing_access_service_client_connect(
         hci_con_handle_t con_handle,
         btstack_packet_handler_t packet_handler,
         has_client_connection_t * connection,
         uint16_t * out_has_cid
);


/**
 * @brief Read hearing aid features. The volume state is received via LEAUDIO_SUBEVENT_HAS_CLIENT_HEARING_AID_FEATURES event.
 * @param has_cid
 * @return status
 */
uint8_t hearing_access_service_client_read_hearing_aid_features(uint16_t has_cid);

/**
 * @brief Read hearing aid features. The volume state is received via LEAUDIO_SUBEVENT_HAS_CLIENT_HEARING_AID_FEATURES event.
 * @param has_cid
 * @return status
 */
uint8_t hearing_access_service_client_read_active_preset_index(uint16_t has_cid);

/**
 * @brief Request to read num_presets presets from the Server, starting with the first preset record with an Index greater than or equal to start_index.
 * @param has_cid
 * @param start_index
 * @param num_presets
 * @return status
 */
uint8_t hearing_access_service_client_read_presets_request(uint16_t has_cid, uint8_t start_index, uint8_t num_presets);


/**
 * @brief Write the Name field of a writable preset record identified by the Index field.
 * @param has_cid
 * @param index
 * @param name
 * @return status
 */
uint8_t hearing_access_service_client_write_preset_name(uint16_t has_cid, uint8_t index, const char * name);

/**
 * @brief Set the preset record identified by the Index field as the active preset.
 * @param has_cid
 * @param index
 * @param synchronized_locally  if true, then the server must relay this change to the other member of the Binaural Hearing Aid Set
 * @return status
 */
uint8_t hearing_access_service_client_set_active_preset(uint16_t has_cid, uint8_t index, bool synchronized_locally);

/**
 * @brief Set the next preset record in the server’s list of preset records as the active preset.
 * @param has_cid
 * @param index
 * @param synchronized_locally  if true, then the server must relay this change to the other member of the Binaural Hearing Aid Set
 * @return status
 */
uint8_t hearing_access_service_client_set_next_preset(uint16_t has_cid, uint8_t index, bool synchronized_locally);

/**
 * @brief Set the previous preset record in the server’s list of preset records as the active preset.
 * @param has_cid
 * @param index
 * @param synchronized_locally if true, then the server must relay this change to the other member of the Binaural Hearing Aid Set
 * @return status
 */
uint8_t hearing_access_service_client_set_previous_preset(uint16_t has_cid, uint8_t index, bool synchronized_locally);


/**
 * @brief Disconnect.
 * @param has_cid
 * @return status
 */
uint8_t hearing_access_service_client_disconnect(uint16_t has_cid);

/**
 * @brief De-initialize Media Control Service. 
 */
void hearing_access_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
