/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
 * @title ATT Server
 *
 */

#ifndef ATT_SERVER_H
#define ATT_SERVER_H

#include <stdint.h>
#include "ble/att_db.h"
#include "btstack_defines.h"
#include "btstack_config.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
/*
 * @brief setup ATT server
 * @param db attribute database created by compile-gatt.ph
 * @param read_callback, see att_db.h, can be NULL
 * @param write_callback, see attl.h, can be NULL
 */
void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback);

/*
 * @brief register packet handler for ATT server events:
 *        - ATT_EVENT_CAN_SEND_NOW
 *        - ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE
 *        - ATT_EVENT_MTU_EXCHANGE_COMPLETE 
 * @param handler
 */
void att_server_register_packet_handler(btstack_packet_handler_t handler);

/**
 * @brief register read/write callbacks for specific handle range
 * @param att_service_handler_t
 */
void att_server_register_service_handler(att_service_handler_t * handler);

/**
 * @brief Request callback when sending is possible
 * @note callback might happend during call to this function
 * @param callback_registration to point to callback function and context information
 * @param con_handle
 * @return 0 if ok, error otherwise
 */
int att_server_register_can_send_now_callback(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle);

/**
 * @brief Return ATT MTU
 * @param con_handle
 * @return mtu if ok, 0 otherwise
 */
uint16_t att_server_get_mtu(hci_con_handle_t con_handle);

/**
 * @brief Request callback when sending notifcation is possible
 * @note callback might happend during call to this function
 * @param callback_registration to point to callback function and context information
 * @param con_handle
 * @return ERROR_CODE_SUCCESS if ok, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if handle unknown, and ERROR_CODE_COMMAND_DISALLOWED if callback already registered
 */
int att_server_request_to_send_notification(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle);

/**
 * @brief Request callback when sending indication is possible
 * @note callback might happend during call to this function
 * @param callback_registration to point to callback function and context information
 * @param con_handle
 * @return ERROR_CODE_SUCCESS if ok, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if handle unknown, and ERROR_CODE_COMMAND_DISALLOWED if callback already registered
 */
int att_server_request_to_send_indication(btstack_context_callback_registration_t * callback_registration, hci_con_handle_t con_handle);

/*
 * @brief notify client about attribute value change
 * @param con_handle
 * @param attribute_handle
 * @param value
 * @param value_len
 * @return 0 if ok, error otherwise
 */
int att_server_notify(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t *value, uint16_t value_len);

/*
 * @brief indicate value change to client. client is supposed to reply with an indication_response
 * @param con_handle
 * @param attribute_handle
 * @param value
 * @param value_len
 * @return 0 if ok, error otherwise
 */
int att_server_indicate(hci_con_handle_t con_handle, uint16_t attribute_handle, const uint8_t *value, uint16_t value_len);

#ifdef ENABLE_ATT_DELAYED_RESPONSE
/*
 * @brief response ready - called after returning ATT_READ__RESPONSE_PENDING in an att_read_callback or
 * ATT_ERROR_WRITE_REQUEST_PENDING IN att_write_callback before to trigger callback again and complete the transaction
 * @nore The ATT Server will retry handling the current ATT request
 * @param con_handle
 * @return 0 if ok, error otherwise
 */
int att_server_response_ready(hci_con_handle_t con_handle);
#endif

// the following functions will be removed soon

/*
 * @brief tests if a notification or indication can be send right now
 * @param con_handle
 * @return 1, if packet can be sent
 */
int  att_server_can_send_packet_now(hci_con_handle_t con_handle);

/**
 * @brief Request emission of ATT_EVENT_CAN_SEND_NOW as soon as possible
 * @note ATT_EVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param con_handle
 */
void att_server_request_can_send_now_event(hci_con_handle_t con_handle);
// end of deprecated functions

/* API_END */

#if defined __cplusplus
}
#endif

#endif // ATT_SERVER_H
