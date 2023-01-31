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
 * @title Media Control Service Client
 * 
 */

#ifndef MEDIA_CONTROL_SERVICE_CLIENT_H
#define MEDIA_CONTROL_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Media Control Service Client connects to the Media Control Services of a remote device 
 * and it can query or set mute value if mute value on the remote side is enabled. The Mute updates are received via notifications.
 */

#define LE_AUDIO_SERVICE_CHARACTERISTICS_MAX_NUM 15

typedef enum {
    LE_AUDIO_SERVICE_CLIENT_STATE_IDLE,
    LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    LE_AUDIO_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

#ifdef ENABLE_TESTING_SUPPORT
    LE_AUDIO_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    LE_AUDIO_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
#endif

    LE_AUDIO_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    LE_AUDIO_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    LE_AUDIO_SERVICE_CLIENT_STATE_CONNECTED,

#ifdef ENABLE_TESTING_SUPPORT
    LE_AUDIO_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION,
    LE_AUDIO_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT
#endif
} le_audio_service_client_state_t;

typedef struct {
    uint16_t value_handle;
    uint16_t client_configuration_handle; 
    uint16_t properties;
    uint16_t uuid16; 
    uint16_t end_handle;
    gatt_client_notification_t notification_listener;
} le_audio_service_characteristic_t;

typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t  con_handle;
    uint16_t          cid;
    le_audio_service_client_state_t  state;

    // service
    uint16_t num_instances;
    uint16_t start_handle;
    uint16_t end_handle;
    
    le_audio_service_characteristic_t characteristics[LE_AUDIO_SERVICE_CHARACTERISTICS_MAX_NUM];
    uint8_t num_characteristics;
    
    btstack_packet_handler_t event_callback;
} le_audio_service_client_connection_t;


typedef struct {
    btstack_linked_item_t item;

    btstack_linked_list_t connections;
    uint16_t cid_counter;
    
    uint8_t  connect_subevent;
    uint8_t  disconnect_subevent;
    
    // service
    uint16_t service_uuid;

    btstack_packet_callback_registration_t hci_event_callback_registration;
} le_audio_service_client_t;

typedef enum {
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_IDLE,
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

// #ifdef ENABLE_TESTING_SUPPORT
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
// #endif

//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
//     MEDIA_CONTROL_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    MEDIA_CONTROL_SERVICE_CLIENT_STATE_CONNECTED,

// #ifdef ENABLE_TESTING_SUPPORT
//     MEDIA_CONTROL_SERVICE_CLIENT_W2_READ_CHARACTERISTIC_CONFIGURATION,
//     MEDIA_CONTROL_SERVICE_CLIENT_W4_CHARACTERISTIC_CONFIGURATION_RESULT,
// #endif
} media_service_client_state_t;


typedef struct {
    btstack_linked_item_t item;
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    le_audio_service_client_state_t  state;

    media_service_client_state_t  client_state;
    btstack_packet_handler_t client_handler;
 
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

    
    uint8_t  requested_mute;

    gatt_client_notification_t notification_listener;
} mcs_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Media Control Service. 
 */
void media_control_service_client_init(void);

/**
 * @brief Connect to Media Control Services of remote device. The client will automatically register for notifications. 
 * The mute state is received via GATTSERVICE_SUBEVENT_MCS_CLIENT_MUTE event.
 * The mute state can be 0 - off, 1 - on, 2 - disabeled and it is valid if the ATTT status is equal to ATT_ERROR_SUCCESS, 
 * see ATT errors (see bluetooth.h) for other values.
 *   
 * Event GATTSERVICE_SUBEVENT_MCS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
 * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no media control service is found, or ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param packet_handler
 * @param mcs_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED 
 */
uint8_t media_control_service_client_connect(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler, uint16_t * mcs_cid);


/**
 * @brief Disconnect.
 * @param mcs_cid
 * @return status
 */
uint8_t media_control_service_client_disconnect(uint16_t mcs_cid);

/**
 * @brief De-initialize Media Control Service. 
 */
void media_control_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
