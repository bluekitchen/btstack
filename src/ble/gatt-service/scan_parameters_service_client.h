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
 * @title Scan Parameters Service Client
 * 
 */

/** 
 * @text The Scan Parameters Service Client allows to store its 
 * LE scan parameters on a remote device such that the remote device 
 * can utilize this information to optimize power consumption and/or 
 * reconnection latency.
 */

#ifndef SCAN_PARAMAETERS_SERVICE_CLIENT_H
#define SCAN_PARAMAETERS_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "ble/gatt_client.h"
#include "btstack_linked_list.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_IDLE,
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC,
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,
#ifdef ENABLE_TESTING_SUPPORT   
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_QUERY_CCC,
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_CCC,
#endif
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W2_CONFIGURE_NOTIFICATIONS,
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_W4_NOTIFICATIONS_CONFIGURED,
    
    SCAN_PARAMETERS_SERVICE_CLIENT_STATE_CONNECTED
} scan_parameters_service_client_state_t;


typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t con_handle;
    uint16_t cid;
    scan_parameters_service_client_state_t  state;
    btstack_packet_handler_t client_handler;

    // service
    uint16_t start_handle;
    uint16_t end_handle;

    // characteristic
    uint16_t scan_interval_window_value_handle;

    uint16_t scan_refresh_value_handle;
    uint16_t scan_refresh_end_handle;
    uint16_t scan_refresh_properties;

    bool     scan_interval_window_value_update;

    gatt_client_notification_t notification_listener;
} scan_parameters_service_client_t;

/* API_START */

/**
 * @brief Initialize Scan Parameters Service. 
 */
void scan_parameters_service_client_init(void);

/**
 * @brief Set Scan Parameters Service. It will update all connected devices.
 * @param scan_interval
 * @param scan_window
 */
void scan_parameters_service_client_set(uint16_t scan_interval, uint16_t scan_window);

/**
 * @brief Connect to Scan Parameters Service of remote device. 
 *
 * The GATTSERVICE_SUBEVENT_SCAN_PARAMETERS_SERVICE_CONNECTED event completes the request. 
 * Its status is set to ERROR_CODE_SUCCESS if remote service and SCAN_INTERVAL_WINDOW characteristic are found. 
 * Other status codes of this event:
 * - GATT_CLIENT_IN_WRONG_STATE: client in wrong state
 * - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE: service or characteristic not found
 * - ATT errors, see bluetooth.h
 *
 * @param con_handle
 * @param packet_handler
 * @param scan_parameters_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if client with con_handle not found
 */
uint8_t scan_parameters_service_client_connect(hci_con_handle_t con_handle,  btstack_packet_handler_t packet_handler, uint16_t * scan_parameters_cid);

/**
 * @brief Enable notifications
 * @param scan_parameters_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if client with con_handle is not found
 */
uint8_t scan_parameters_service_client_enable_notifications(uint16_t scan_parameters_cid);

/**
 * @brief Disconnect from Scan Parameters Service.
 * @param scan_parameters_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if client with con_handle is not found
 */
uint8_t scan_parameters_service_client_disconnect(uint16_t scan_parameters_cid); 


/**
 * @brief De-initialize Scan Parameters Service. 
 */
void scan_parameters_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
