/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * @title Microphone Control Service Client
 *
 */

#ifndef MICROPHONE_CONTROL_SERVICE_CLIENT_H
#define MICROPHONE_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Microphone Control Service Client connects to the Microphone Control Services of a remote device
 * and it can query or set mute value if mute value on the remote side is enabled. The Mute updates are received via notifications.
 */

typedef enum {
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_IDLE,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

#ifdef ENABLE_TESTING_SUPPORT
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
#endif

    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    MICROPHONE_CONTROL_SERVICE_CLIENT_STATE_CONNECTED,

#ifdef ENABLE_TESTING_SUPPORT
    MICROPHONE_CONTROL_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION,
    MICROPHONE_CONTROL_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT,
#endif
    MICROPHONE_CONTROL_SERVICE_CLIENT_W2_WRITE_MUTE,
    MICROPHONE_CONTROL_SERVICE_CLIENT_W4_WRITE_MUTE_RESULT
} microphone_service_client_state_t;


typedef struct {
    // service
    uint16_t start_handle;
    uint16_t end_handle;

    // characteristic
    uint16_t properties;
    uint16_t value_handle;

#ifdef ENABLE_TESTING_SUPPORT
    uint16_t ccc_handle;
#endif

    gatt_client_notification_t notification_listener;
} microphone_service_t;


typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t  con_handle;
    uint16_t          cid;
    microphone_service_client_state_t  state;
    btstack_packet_handler_t client_handler;

    // microphone_service_t service;
    // service
    uint16_t start_handle;
    uint16_t end_handle;

    // characteristic
    uint16_t properties;
    uint16_t mute_value_handle;

#ifdef ENABLE_TESTING_SUPPORT
    uint16_t ccc_handle;
#endif

    bool need_polling;
    uint16_t num_instances;
    uint8_t  requested_mute;

    gatt_client_notification_t notification_listener;
} microphone_control_service_client_t;

/* API_START */


/**
 * @brief Initialize Microphone Control Service.
 */
void microphone_control_service_client_init(void);

/**
 * @brief Connect to Microphone Control Services of remote device. The client will automatically register for notifications.
 * The mute state is received via GATTSERVICE_SUBEVENT_MICROPHONE_CONTROL_SERVICE_MUTE event.
 * The mute state can be 0 - off, 1 - on, 2 - disabeled and it is valid if the ATTT status is equal to ATT_ERROR_SUCCESS,
 * see ATT errors (see bluetooth.h) for other values.
 *
 * Event GATTSERVICE_SUBEVENT_MICROPHONE_CONTROL_SERVICE_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no microphone control service is found, or ATT errors (see bluetooth.h).
 *
 * @param con_handle
 * @param packet_handler
 * @param mics_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t microphone_control_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, uint16_t * mics_cid);

/**
 * @brief Read mute state. The mute state is received via GATTSERVICE_SUBEVENT_MICROPHONE_CONTROL_SERVICE_MUTE event.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_read_mute_state(uint16_t mics_cid);

/**
 * @brief Turn on mute.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_mute_turn_on(uint16_t mics_cid);

/**
 * @brief Turn off mute.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_mute_turn_off(uint16_t mics_cid);

/**
 * @brief Disconnect.
 * @param mics_cid
 * @return status
 */
uint8_t microphone_control_service_client_disconnect(uint16_t mics_cid);

/**
 * @brief De-initialize Microphone Control Service.
 */
void microphone_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
