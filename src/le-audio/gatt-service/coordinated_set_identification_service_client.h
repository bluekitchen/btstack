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
 * @title Coordinated Set Identification Service Client
 * 
 */

#ifndef COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_H
#define COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_IDLE,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,

    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,
    
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_CONNECTED,

    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_CONFIGURATION,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_CONFIGURATION_RESULT,

    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_READ,

    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    COORDINATED_SET_IDENTIFICATION_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_VALUE_WRITTEN
    
} coordinated_set_identification_service_client_state_t;

typedef struct {
    uint16_t value_handle;
    uint16_t client_configuration_handle; 
    uint16_t properties;
    uint16_t uuid16; 
    uint16_t end_handle;
    gatt_client_notification_t notification_listener;
} csis_client_characteristic_t;

typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    uint16_t          mtu;
    coordinated_set_identification_service_client_state_t  state;
    
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    
    csis_client_characteristic_t characteristics[4];

    // used for memory capacity checking
    uint8_t  service_instances_num;
    uint8_t  characteristic_index;

    csis_sirk_calculation_state_t  remote_sirk_state;
    
    csis_member_lock_t coordinator_lock;
} csis_client_connection_t;

/* API_START */

/**
 * @brief Init Coordinated Set Identification Service (CSIS) Client and register packet handler to receive events:
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_SERVER_CONNECTED
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_SERVER_DISCONNECTED
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_LOCK_WRITE_COMPLETE
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_LOCK
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_COORDINATED_SET_SIZE
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_RANK
 * - GATTSERVICE_SUBEVENT_CSIS_REMOTE_SIRK
 * - GATTSERVICE_SUBEVENT_CSIS_RSI_MATCH
 *              
 * @param packet_handler
 */
void coordinated_set_identification_service_client_init(btstack_packet_handler_t packet_handler);

/**
 * @brief Connect to remote CSIS Server. 
 * @param  connection                            
 * @param  con_handle
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if the HCI connection with the given con_handle is found, otherwise
 *                ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER
 */
uint8_t coordinated_set_identification_service_client_connect(csis_client_connection_t * connection, hci_con_handle_t con_handle, uint16_t * csis_cid);

/**
 * @brief Read SIRK from remote CSIS server. The SIRK value is reported via the GATTSERVICE_SUBEVENT_CSIS_REMOTE_SIRK event. 
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if successful, otherwise
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if the HCI connection with the given con_handle is found
 *                - ERROR_CODE_COMMAND_DISALLOWED if the client is not connected
 *                - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no appropriate characteristic is found on the remote server
 */
uint8_t coordinated_set_identification_service_client_read_sirk(uint16_t ascs_cid);

/**
 * @brief Read lock from remote CSIS server. The value is reported via the GATTSERVICE_SUBEVENT_CSIS_REMOTE_LOCK event. 
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if succesful, otherwise
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if the HCI connection with the given con_handle is found
 *                - ERROR_CODE_COMMAND_DISALLOWED if the client is not connected
 *                - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no appropriate characteristic is found on the remote server
 */
uint8_t coordinated_set_identification_service_client_read_member_lock(uint16_t ascs_cid);

/**
 * @brief Write lock on remote CSIS server. The status of write is reported via the GATTSERVICE_SUBEVENT_CSIS_WRITE_LOCK_COMPLETE event. 
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if succesful, otherwise
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if the HCI connection with the given con_handle is found
 *                - ERROR_CODE_COMMAND_DISALLOWED if the client is not connected
 *                - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no appropriate characteristic is found on the remote server
 */
uint8_t coordinated_set_identification_service_client_write_member_lock(uint16_t ascs_cid, csis_member_lock_t lock);

/**
 * @brief Read coordinated set size from remote CSIS server. The value is reported via the GATTSERVICE_SUBEVENT_CSIS_REMOTE_COORDINATED_SET_SIZE event. 
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if succesful, otherwise
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if the HCI connection with the given con_handle is found
 *                - ERROR_CODE_COMMAND_DISALLOWED if the client is not connected
 *                - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no appropriate characteristic is found on the remote server
 */
uint8_t coordinated_set_identification_service_client_read_coordinated_set_size(uint16_t ascs_cid);

/**
 * @brief Read coordinator rank from remote CSIS server. The value is reported via the GATTSERVICE_SUBEVENT_CSIS_REMOTE_RANK event. 
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if succesful, otherwise
 *                - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if the HCI connection with the given con_handle is found
 *                - ERROR_CODE_COMMAND_DISALLOWED if the client is not connected
 *                - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no appropriate characteristic is found on the remote server
 */
uint8_t coordinated_set_identification_service_client_read_member_rank(uint16_t ascs_cid);

/**
 * @brief Check if the RSI matches the given SIRK value. The result is reported via the GATTSERVICE_SUBEVENT_CSIS_RSI_MATCH event. 
 * @param  ascs_cid
 * @return ERROR_CODE_SUCCESS if succesful, otherwise
 *                - ERROR_CODE_COMMAND_DISALLOWED if there is an ongoing RSI calculation
 */
uint8_t coordinated_set_identification_service_client_check_rsi(const uint8_t * rsi, const uint8_t * sirk);

/**
 * @brief Deinit CSIS Client
 */
void coordinated_set_identification_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

