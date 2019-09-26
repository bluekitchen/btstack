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


#ifndef ATT_DB_H
#define ATT_DB_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "btstack_defines.h"

#if defined __cplusplus
extern "C" {
#endif

// MARK: Attribute PDU Opcodes 
#define ATT_ERROR_RESPONSE              0x01

#define ATT_EXCHANGE_MTU_REQUEST        0x02
#define ATT_EXCHANGE_MTU_RESPONSE       0x03

#define ATT_FIND_INFORMATION_REQUEST    0x04
#define ATT_FIND_INFORMATION_REPLY      0x05
#define ATT_FIND_BY_TYPE_VALUE_REQUEST  0x06
#define ATT_FIND_BY_TYPE_VALUE_RESPONSE 0x07

#define ATT_READ_BY_TYPE_REQUEST        0x08
#define ATT_READ_BY_TYPE_RESPONSE       0x09
#define ATT_READ_REQUEST                0x0a
#define ATT_READ_RESPONSE               0x0b
#define ATT_READ_BLOB_REQUEST           0x0c
#define ATT_READ_BLOB_RESPONSE          0x0d
#define ATT_READ_MULTIPLE_REQUEST       0x0e
#define ATT_READ_MULTIPLE_RESPONSE      0x0f
#define ATT_READ_BY_GROUP_TYPE_REQUEST  0x10
#define ATT_READ_BY_GROUP_TYPE_RESPONSE 0x11

#define ATT_WRITE_REQUEST               0x12
#define ATT_WRITE_RESPONSE              0x13

#define ATT_PREPARE_WRITE_REQUEST       0x16
#define ATT_PREPARE_WRITE_RESPONSE      0x17
#define ATT_EXECUTE_WRITE_REQUEST       0x18
#define ATT_EXECUTE_WRITE_RESPONSE      0x19

#define ATT_HANDLE_VALUE_NOTIFICATION   0x1b
#define ATT_HANDLE_VALUE_INDICATION     0x1d
#define ATT_HANDLE_VALUE_CONFIRMATION   0x1e


#define ATT_WRITE_COMMAND                0x52
#define ATT_SIGNED_WRITE_COMMAND         0xD2

// custom BTstack ATT Response Pending for att_read_callback
#define ATT_READ_RESPONSE_PENDING                 0xffff

// internally used to signal write response pending
#define ATT_INTERNAL_WRITE_RESPONSE_PENDING       0xfffe

// internal additions
// 128 bit UUID used
#define ATT_PROPERTY_UUID128             0x200
// Read/Write Permission bits
#define ATT_PROPERTY_READ_PERMISSION_BIT_0  0x0400
#define ATT_PROPERTY_READ_PERMISSION_BIT_1  0x0800
#define ATT_PROPERTY_WRITE_PERMISSION_BIT_0 0x0001
#define ATT_PROPERTY_WRITE_PERMISSION_BIT_1 0x0010
#define ATT_PROPERTY_READ_PERMISSION_SC     0x0020
#define ATT_PROPERTY_WRITE_PERMISSION_SC    0x0080


typedef struct att_connection {
    hci_con_handle_t con_handle;
    uint16_t mtu;       // initialized to ATT_DEFAULT_MTU (23), negotiated during MTU exchange
    uint16_t max_mtu;   // local maximal L2CAP_MTU, set to l2cap_max_le_mtu()
    uint8_t  encryption_key_size;
    uint8_t  authenticated;
    uint8_t  authorized;
    uint8_t  secure_connection;
} att_connection_t;

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// If ENABLE_ATT_DELAYED_READ_RESPONSE is defined, you may return ATT_READ_RESPONSE_PENDING if data isn't available yet
// @param con_handle of hci le connection
// @param attribute_handle to be read
// @param offset defines start of attribute value
// @param buffer 
// @param buffer_size
typedef uint16_t (*att_read_callback_t)(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);

// ATT Client Write Callback for Dynamic Data
// @param con_handle of hci le connection
// @param attribute_handle to be written
// @param transaction - ATT_TRANSACTION_MODE_NONE for regular writes. For prepared writes: ATT_TRANSACTION_MODE_ACTIVE, ATT_TRANSACTION_MODE_VALIDATE, ATT_TRANSACTION_MODE_EXECUTE, ATT_TRANSACTION_MODE_CANCEL
// @param offset into the value - used for queued writes and long attributes
// @param buffer 
// @param buffer_size
// @param signature used for signed write commmands
// @returns 0 if write was ok, ATT_ERROR_PREPARE_QUEUE_FULL if no space in queue, ATT_ERROR_INVALID_OFFSET if offset is larger than max buffer
//
// Each Prepared Write Request triggers a callback with transaction mode ATT_TRANSACTION_MODE_ACTIVE.
// On Execute Write, the callback will be called with ATT_TRANSACTION_MODE_VALIDATE and allows to validate all queued writes and return an application error.
// If none of the registered callbacks return an error for ATT_TRANSACTION_MODE_VALIDATE and the callback will be called with ATT_TRANSACTION_MODE_EXECUTE.
// Otherwise, all callbacks will be called with ATT_TRANSACTION_MODE_CANCEL.
//
// If the additional validation step is not needed, just return 0 for all callbacks with transaction mode ATT_TRANSACTION_MODE_VALIDATE.
//
typedef int (*att_write_callback_t)(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

// Read & Write Callbacks for handle range
typedef struct att_service_handler {
    btstack_linked_item_t * item;
    uint16_t start_handle;
    uint16_t end_handle;
    att_read_callback_t read_callback;
    att_write_callback_t write_callback;
    btstack_packet_handler_t packet_handler;
} att_service_handler_t;

// MARK: ATT Operations

/*
 * @brief setup ATT database
 */
void att_set_db(uint8_t const * db);

/*
 * @brief set callback for read of dynamic attributes
 * @param callback
 */
void att_set_read_callback(att_read_callback_t callback);

/*
 * @brief set callback for write of dynamic attributes
 * @param callback
 */
void att_set_write_callback(att_write_callback_t callback);

/*
 * @brief debug helper, dump ATT database to stdout using log_info
 */
void att_dump_attributes(void);

/*
 * @brief process ATT request against database and put response into response buffer
 * @param att_connection used for mtu and security properties
 * @param request_buffer, request_len: ATT request from clinet
 * @param response_buffer for result
 * @returns len of data in response buffer. 0 = no response, 
 *          ATT_READ_RESPONSE_PENDING if it was returned at least once for dynamic data (requires ENABLE_ATT_DELAYED_READ_RESPONSE)
 */
uint16_t att_handle_request(att_connection_t * att_connection,
                            uint8_t * request_buffer,
                            uint16_t request_len,
                            uint8_t * response_buffer);

/*
 * @brief setup value notification in response buffer for a given handle and value
 * @param att_connection
 * @param attribute_handle
 * @param value
 * @param value_len
 * @param response_buffer for notification
 */
uint16_t att_prepare_handle_value_notification(att_connection_t * att_connection,
                                               uint16_t attribute_handle,
                                               const uint8_t *value,
                                               uint16_t value_len, 
                                               uint8_t * response_buffer);

/*
 * @brief setup value indication in response buffer for a given handle and value
 * @param att_connection
 * @param attribute_handle
 * @param value
 * @param value_len
 * @param response_buffer for indication
 */
uint16_t att_prepare_handle_value_indication(att_connection_t * att_connection,
                                             uint16_t attribute_handle,
                                             const uint8_t *value,
                                             uint16_t value_len, 
                                             uint8_t * response_buffer);

/*
 * @brief transcation queue of prepared writes, e.g., after disconnect
 */
void att_clear_transaction_queue(att_connection_t * att_connection);

// att_read_callback helpers for a various data types

/*
 * @brief Handle read of blob like data for att_read_callback
 * @param blob of data
 * @param blob_size of blob
 * @param offset from att_read_callback
 * @param buffer from att_read_callback
 * @param buffer_size from att_read_callback
 * @returns value size for buffer == 0 and num bytes copied otherwise
 */
uint16_t att_read_callback_handle_blob(const uint8_t * blob, uint16_t blob_size, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);

/*
 * @brief Handle read of little endian unsigned 32 bit value for att_read_callback
 * @param value
 * @param offset from att_read_callback
 * @param buffer from att_read_callback
 * @param buffer_size from att_read_callback
 * @returns value size for buffer == 0 and num bytes copied otherwise
 */
uint16_t att_read_callback_handle_little_endian_32(uint32_t value, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);

/*
 * @brief Handle read of little endian unsigned 16 bit value for att_read_callback
 * @param value
 * @param offset from att_read_callback
 * @param buffer from att_read_callback
 * @param buffer_size from att_read_callback
 * @returns value size for buffer == 0 and num bytes copied otherwise
 */
uint16_t att_read_callback_handle_little_endian_16(uint16_t value, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);

/*
 * @brief Handle read of single byte for att_read_callback
 * @param blob of data
 * @param blob_size of blob
 * @param offset from att_read_callback
 * @param buffer from att_read_callback
 * @param buffer_size from att_read_callback
 * @returns value size for buffer == 0 and num bytes copied otherwise
 */
uint16_t att_read_callback_handle_byte(uint8_t value, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);


// experimental client API
uint16_t att_uuid_for_handle(uint16_t attribute_handle);


// experimental GATT Server API

// returns 1 if service found. only primary service.
int gatt_server_get_get_handle_range_for_service_with_uuid16(uint16_t uuid16, uint16_t * start_handle, uint16_t * end_handle);

// returns 0 if not found
uint16_t gatt_server_get_value_handle_for_characteristic_with_uuid16(uint16_t start_handle, uint16_t end_handle, uint16_t uuid16);

// returns 0 if not found
uint16_t gatt_server_get_descriptor_handle_for_characteristic_with_uuid16(uint16_t start_handle, uint16_t end_handle, uint16_t characteristic_uuid16, uint16_t descriptor_uuid16);
uint16_t gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(uint16_t start_handle, uint16_t end_handle, uint16_t characteristic_uuid16);
uint16_t gatt_server_get_server_configuration_handle_for_characteristic_with_uuid16(uint16_t start_handle, uint16_t end_handle, uint16_t characteristic_uuid16);


// returns 1 if service found. only primary service.
int gatt_server_get_get_handle_range_for_service_with_uuid128(const uint8_t * uuid128, uint16_t * start_handle, uint16_t * end_handle);

// returns 0 if not found
uint16_t gatt_server_get_value_handle_for_characteristic_with_uuid128(uint16_t start_handle, uint16_t end_handle, const uint8_t * uuid128);

// returns 0 if not found
uint16_t gatt_server_get_client_configuration_handle_for_characteristic_with_uuid128(uint16_t start_handle, uint16_t end_handle, const uint8_t * uuid128);

// non-user functionality for att_server

/*
 * @brief Check if writes to handle should be persistent
 * @param handle
 * @returns 1 if persistent
 */
int att_is_persistent_ccc(uint16_t handle);


#if defined __cplusplus
}
#endif

#endif // ATT_H
