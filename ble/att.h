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


#ifndef __ATT_H
#define __ATT_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// Minimum/default MTU
#define ATT_DEFAULT_MTU               23

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

// MARK: ATT Error Codes
#define ATT_ERROR_INVALID_HANDLE                   0x01
#define ATT_ERROR_READ_NOT_PERMITTED               0x02
#define ATT_ERROR_WRITE_NOT_PERMITTED              0x03
#define ATT_ERROR_INVALID_PDU                      0x04  
#define ATT_ERROR_INSUFFICIENT_AUTHENTICATION      0x05
#define ATT_ERROR_REQUEST_NOT_SUPPORTED            0x06
#define ATT_ERROR_INVALID_OFFSET                   0x07
#define ATT_ERROR_INSUFFICIENT_AUTHORIZATION       0x08
#define ATT_ERROR_PREPARE_QUEUE_FULL               0x09
#define ATT_ERROR_ATTRIBUTE_NOT_FOUND              0x0a
#define ATT_ERROR_ATTRIBUTE_NOT_LONG               0x0b
#define ATT_ERROR_INSUFFICIENT_ENCRYPTION_KEY_SIZE 0x0c
#define ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH   0x0d
#define ATT_ERROR_UNLIKELY_ERROR                   0x0e
#define ATT_ERROR_INSUFFICIENT_ENCRYPTION          0x0f
#define ATT_ERROR_UNSUPPORTED_GROUP_TYPE           0x10
#define ATT_ERROR_INSUFFICIENT_RESOURCES           0x11
// custom BTstack ATT error coders
#define ATT_ERROR_DATA_MISMATCH                    0x7e
#define ATT_ERROR_TIMEOUT                          0x7F

    
// custom BTstack error codes
#define ATT_ERROR_HCI_DISCONNECT_RECEIVED          0x1f
// MARK: Attribute Property Flags
#define ATT_PROPERTY_BROADCAST           0x01
#define ATT_PROPERTY_READ                0x02
#define ATT_PROPERTY_WRITE_WITHOUT_RESPONSE 0x04
#define ATT_PROPERTY_WRITE               0x08
#define ATT_PROPERTY_NOTIFY              0x10
#define ATT_PROPERTY_INDICATE            0x20
#define ATT_PROPERTY_AUTHENTICATED_SIGNED_WRITE 0x40
#define ATT_PROPERTY_EXTENDED_PROPERTIES 0x80

// MARK: Attribute Property Flag, BTstack extension
// value is asked from client
#define ATT_PROPERTY_DYNAMIC             0x100
// 128 bit UUID used
#define ATT_PROPERTY_UUID128             0x200
// Authentication required
#define ATT_PROPERTY_AUTHENTICATION_REQUIRED 0x400
// Authorization from user required
#define ATT_PROPERTY_AUTHORIZATION_REQUIRED  0x800
// Encryption key size stored in upper 4 bits, 0 == no encryption, encryption key size - 1 otherwise

// ATT Transaction Timeout of 30 seconds for Command/Response or Incidationc/Confirmation
#define ATT_TRANSACTION_TIMEOUT_MS     30000

#define ATT_TRANSACTION_MODE_NONE      0x0
#define ATT_TRANSACTION_MODE_ACTIVE    0x1
#define ATT_TRANSACTION_MODE_EXECUTE   0x2
#define ATT_TRANSACTION_MODE_CANCEL    0x3

// MARK: GATT UUIDs
#define GATT_PRIMARY_SERVICE_UUID                   0x2800
#define GATT_SECONDARY_SERVICE_UUID                 0x2801
#define GATT_INCLUDE_SERVICE_UUID                   0x2802
#define GATT_CHARACTERISTICS_UUID                   0x2803
#define GATT_CHARACTERISTIC_EXTENDED_PROPERTIES     0x2900
#define GATT_CHARACTERISTIC_USER_DESCRIPTION        0x2901
#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION   0x2902
#define GATT_SERVER_CHARACTERISTICS_CONFIGURATION   0x2903
#define GATT_CHARACTERISTIC_PRESENTATION_FORMAT     0x2904
#define GATT_CHARACTERISTIC_AGGREGATE_FORMAT        0x2905

#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NONE          0
#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION  1
#define GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_INDICATION    2

// GAP Service and Characteristics
#define GAP_SERVICE_UUID               0x1800
#define GAP_DEVICE_NAME_UUID           0x2a00
#define GAP_APPEARANCE_UUID            0x2a01
#define GAP_PERIPHERAL_PRIVACY_FLAG    0x2a02
#define GAP_RECONNECTION_ADDRESS_UUID  0x2a03
#define GAP_PERIPHERAL_PREFERRED_CONNECTION_PARAMETERS_UUID 0x2a04


typedef struct att_connection {
    uint16_t con_handle;
    uint16_t mtu;       // initialized to ATT_DEFAULT_MTU (23), negotiated during MTU exchange
    uint16_t max_mtu;   // local maximal L2CAP_MTU, set to l2cap_max_le_mtu()
    uint8_t  encryption_key_size;
    uint8_t  authenticated;
    uint8_t  authorized;
} att_connection_t;

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param con_handle of hci le connection
// @param attribute_handle to be read
// @param offset defines start of attribute value
// @param buffer 
// @param buffer_size
typedef uint16_t (*att_read_callback_t)(uint16_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);

// ATT Client Write Callback for Dynamic Data
// @param con_handle of hci le connection
// @param attribute_handle to be written
// @param transaction - ATT_TRANSACTION_MODE_NONE for regular writes, ATT_TRANSACTION_MODE_ACTIVE for prepared writes and ATT_TRANSACTION_MODE_EXECUTE
// @param offset into the value - used for queued writes and long attributes
// @param buffer 
// @param buffer_size
// @param signature used for signed write commmands
// @returns 0 if write was ok, ATT_ERROR_PREPARE_QUEUE_FULL if no space in queue, ATT_ERROR_INVALID_OFFSET if offset is larger than max buffer
typedef int (*att_write_callback_t)(uint16_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

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
 * @returns len of data in response buffer. 0 = no response
 */
uint16_t att_handle_request(att_connection_t * att_connection,
                            uint8_t * request_buffer,
                            uint16_t request_len,
                            uint8_t * response_buffer);

/*
 * @brief setup value notification in response buffer for a given handle and value
 * @param att_connection
 * @param value, value_len: new attribute value
 * @param response_buffer for notification
 */
uint16_t att_prepare_handle_value_notification(att_connection_t * att_connection,
                                               uint16_t handle,
                                               uint8_t *value,
                                               uint16_t value_len, 
                                               uint8_t * response_buffer);

/*
 * @brief setup value indication in response buffer for a given handle and value
 * @param att_connection
 * @param value, value_len: new attribute value
 * @param response_buffer for indication
 */
uint16_t att_prepare_handle_value_indication(att_connection_t * att_connection,
                                             uint16_t handle,
                                             uint8_t *value,
                                             uint16_t value_len, 
                                             uint8_t * response_buffer);

/*
 * @brief transcation queue of prepared writes, e.g., after disconnect
 */
void att_clear_transaction_queue(att_connection_t * att_connection);

// experimental client API
uint16_t att_uuid_for_handle(uint16_t handle);

#if defined __cplusplus
}
#endif

#endif // __ATT_H
