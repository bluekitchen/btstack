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
 * @title Telephone Bearer Service Client
 * 
 */

#ifndef TELEPHONE_BEARER_SERVICE_CLIENT_H
#define TELEPHONE_BEARER_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"
#include "ble/gatt_service_client.h"
#include "le-audio/gatt-service/telephone_bearer_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_IDLE = 0,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_CONNECTED,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_READY,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_READ_CHARACTERISTIC_VALUE,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_READ_CHARACTERISTIC_VALUE_RESULT,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE_WITHOUT_RESPONSE,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W2_WRITE_CHARACTERISTIC_VALUE,
    TELEPHONE_BEARER_SERVICE_CLIENT_STATE_W4_WRITE_CHARACTERISTIC_VALUE_RESULT   
} telephone_bearer_service_client_state_t;

#define TBS_CLIENT_SERIALISATION_BUFFER_SIZE 255

typedef struct {
    btstack_linked_item_t item;

    gatt_service_client_connection_t basic_connection;
    btstack_packet_handler_t packet_handler;

    telephone_bearer_service_client_state_t state;
    
    // Used for read characteristic queries
    uint8_t characteristic_index;
    // Used to store parameters for write characteristic
    uint8_t serialisation_buffer[TBS_CLIENT_SERIALISATION_BUFFER_SIZE];
    uint8_t data_size;

    gatt_service_client_characteristic_t characteristics_storage[TBS_CHARACTERISTICS_NUM];
    btstack_context_callback_registration_t gatt_query_can_send_now;
} tbs_client_connection_t;

/* API_START */

    
/**
 * @brief Initialize Telephone Bearer Service. 
 */
void telephone_bearer_service_client_init(void);

 /**
  * @brief Connect to Telephony Bearer Service of remote device. The client will automatically register for notifications.
  *
  * Event GATTSERVICE_SUBEVENT_TBS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
  * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no media control service is found, or ATT errors (see bluetooth.h).
  *
 * @param con_handle
 * @param packet_handler
 * @param connection
 * @param service_index
 * @param tbs_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
 */
 uint8_t telephone_bearer_service_client_connect(
    hci_con_handle_t con_handle,
    btstack_packet_handler_t packet_handler,
    tbs_client_connection_t * connection,
    uint8_t  service_index,
    uint16_t * tbs_cid);

 /**
   * @brief Connect to Generic Telephony Bearer Service of remote device. The client will automatically register for notifications.
   *
   * Event GATTSERVICE_SUBEVENT_TBS_CLIENT_CONNECTED is emitted with status ERROR_CODE_SUCCESS on success, otherwise
   * GATT_CLIENT_IN_WRONG_STATE, ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if no media control service is found, or ATT errors (see bluetooth.h).
   *
  * @param con_handle
  * @param packet_handler
  * @param connection
  * @param service_index
  * @param tbs_cid
  * @return status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED if there is already a client associated with con_handle, or BTSTACK_MEMORY_ALLOC_FAILED
  */
  uint8_t telephone_generic_bearer_service_client_connect(
     hci_con_handle_t con_handle,
     btstack_packet_handler_t packet_handler,
     tbs_client_connection_t * connection,
     uint8_t  service_index,
     uint16_t * tbs_cid);

/**
 * @brief Set signal strength reporting interval
 *
 * @param cid
 * @param reporting_interval_s
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_set_strength_reporting_interval(uint16_t cid, uint8_t reporting_interval_s);

/**
 * @brief Issues the ACCEPT command, with given call id, to the Call Control Point characteristic, command status is received via Call Control Point Notifications
 *
 * @param cid
 * @param call_id
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_call_accept(uint16_t cid, uint8_t call_id);

/**
 * @brief Issues the TERMINATE command, with given call id, to the Call Control Point characteristic, command status is received via Call Control Point Notifications
 * @param cid
 * @param call_id
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_call_terminate(uint16_t cid, uint8_t call_id);

/**
 * @brief Issues the LOCAL_HOLD command, with given call id, to the Call Control Point characteristic, command status is received via Call Control Point Notifications
 * @param cid
 * @param call_id
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_call_hold(uint16_t cid, uint8_t call_id);

/**
 * @brief Issues the LOCAL_RETRIEVE command, with given call id, to the Call Control Point characteristic, command status is received via Call Control Point Notifications
 * @param cid
 * @param call_id
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_call_retrieve(uint16_t cid, uint8_t call_id);

/**
 * @brief Issues the ORIGINATE command, with given call id, to the Call Control Point characteristic, command status is received via Call Control Point Notifications
 * @param cid
 * @param call_id
 * @param uri
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_call_originate(uint16_t cid, uint8_t call_id, const char *uri);

/**
 * @brief Issues the JOIN command, with given call id, to the Call Control Point characteristic, command status is received via Call Control Point Notifications
 * @param cid
 * @param call_id
 * @param call_index_list
 * @param size
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_call_join(uint16_t cid, const uint8_t *call_index_list, uint16_t size);

/**
 * @brief Query the provider name
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_provider_name(uint16_t cid);

/**
 * @brief Query the list of current calls
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_list_current_calls(uint16_t cid);

/**
 * @brief Query the signal strength
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_signal_strength(uint16_t cid);

/**
 * @brief Query the signal strength reporting interval
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_signal_strength_reporting_interval(uint16_t cid);

/**
 * @brief Query the bearer technology
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_technology(uint16_t cid);

/**
 * @brief Query the universal caller identifier
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_uci(uint16_t cid);

/**
 * @brief Query bearer supported schemes list
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_schemes_supported_list(uint16_t cid);

/**
 * @brief Query call control point optional opcodes
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_call_control_point_optional_opcodes(uint16_t cid);

/**
 * @brief Query call friendly name
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_call_friendly_name(uint16_t cid);

/**
 * @brief Query call state
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_call_state(uint16_t cid);

/**
 * @brief Query content control id
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_content_control_id(uint16_t cid);

/**
 * @brief Query incoming call
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_incoming_call(uint16_t cid);

/**
 * @brief Query incoming call target bearer URI
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_incoming_call_target_bearer_uri(uint16_t cid);

/**
 * @brief Query status flags
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_status_flags(uint16_t cid);

/**
 * @brief Query termination reason
 * @param cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_get_termination_reason(uint16_t cid);

/**
 * @brief Disconnect.
 * @param tbs_cid
 * @return status ERROR_CODE_SUCCESS on success
 */
uint8_t telephone_bearer_service_client_disconnect(uint16_t tbs_cid);

/**
 * @brief De-initialize Telephone Bearer Service.
 */
void telephone_bearer_service_client_deinit(void);


/* API_END */

#if defined __cplusplus
}
#endif

#endif
