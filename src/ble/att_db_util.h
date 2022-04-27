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
 * @title Runtine ATT Database Setup
 *
 * Helper to construct ATT DB at runtime (BTstack GATT Compiler is not used).
 *
 */

#ifndef ATT_DB_UTIL_H
#define ATT_DB_UTIL_H

#include "btstack_config.h"
#include "btstack_crypto.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Init ATT DB storage
 */
void att_db_util_init(void);

/**
 * @brief Add primary service for 16-bit UUID
 * @param uuid16
 * @return attribute handle for the new service definition
 */
uint16_t att_db_util_add_service_uuid16(uint16_t uuid16);

/**
 * @brief Add primary service for 128-bit UUID
 * @param uuid128
 * @return attribute handle for the new service definition
 */
uint16_t att_db_util_add_service_uuid128(const uint8_t * uuid128);

/**
 * @brief Add secondary service for 16-bit UUID
 * @param uuid16
 * @return attribute handle for the new service definition
 */
uint16_t att_db_util_add_secondary_service_uuid16(uint16_t uuid16);

/**
 * @brief Add secondary service for 128-bit UUID
 * @param uuid128
 * @return attribute handle for the new service definition
 */
uint16_t att_db_util_add_secondary_service_uuid128(const uint8_t * uuid128);

/**
 * @brief Add included service with 16-bit UUID
 * @param start_group_handle
 * @param end_group_handle
 * @param uuid16
 * @return attribute handle for the new service definition
 */
uint16_t att_db_util_add_included_service_uuid16(uint16_t start_group_handle, uint16_t  end_group_handle, uint16_t uuid16);

/**
 * @brief Add Characteristic with 16-bit UUID, properties, and data
 * @param uuid16
 * @param properties        - see ATT_PROPERTY_* in src/bluetooth.h
 * @param read_permissions  - see ATT_SECURITY_* in src/bluetooth.h
 * @param write_permissions - see ATT_SECURITY_* in src/bluetooth.h
 * @param data returned in read operations if ATT_PROPERTY_DYNAMIC is not specified
 * @param data_len
 * @return attribute handle of the new characteristic value declaration
 * @note If properties contains ATT_PROPERTY_NOTIFY or ATT_PROPERTY_INDICATE flags, a Client Configuration Characteristic Descriptor (CCCD)
 *       is created as well. The attribute value handle of the CCCD is the attribute value handle plus 1
 */
uint16_t att_db_util_add_characteristic_uuid16(uint16_t uuid16, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len);

/**
 * @brief Add Characteristic with 128-bit UUID, properties, and data
 * @param uuid128
 * @param properties        - see ATT_PROPERTY_* in src/bluetooth.h
 * @param read_permissions  - see ATT_SECURITY_* in src/bluetooth.h
 * @param write_permissions - see ATT_SECURITY_* in src/bluetooth.h
 * @param data returned in read operations if ATT_PROPERTY_DYNAMIC is not specified
 * @param data_len
 * @return attribute handle of the new characteristic value declaration
 * @note If properties contains ATT_PROPERTY_NOTIFY or ATT_PROPERTY_INDICATE flags, a Client Configuration Characteristic Descriptor (CCCD)
 *       is created as well. The attribute value handle of the CCCD is the attribute value handle plus 1
 */
uint16_t att_db_util_add_characteristic_uuid128(const uint8_t * uuid128, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len);

/**
* @brief Add descriptor with 16-bit UUID, properties, and data
* @param uuid16
* @param properties        - see ATT_PROPERTY_* in src/bluetooth.h
* @param read_permissions  - see ATT_SECURITY_* in src/bluetooth.h
* @param write_permissions - see ATT_SECURITY_* in src/bluetooth.h
* @param data returned in read operations if ATT_PROPERTY_DYNAMIC is not specified
* @param data_len
* @return attribute handle of the new characteristic descriptor declaration
*/
uint16_t att_db_util_add_descriptor_uuid16(uint16_t uuid16, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len);

/**
* @brief Add descriptor with 128-bit UUID, properties, and data
* @param uuid128
* @param properties        - see ATT_PROPERTY_* in src/bluetooth.h
* @param read_permissions  - see ATT_SECURITY_* in src/bluetooth.h
* @param write_permissions - see ATT_SECURITY_* in src/bluetooth.h
* @param data returned in read operations if ATT_PROPERTY_DYNAMIC is not specified
* @param data_len
* @return attribute handle of the new characteristic descriptor declaration
*/
uint16_t att_db_util_add_descriptor_uuid128(const uint8_t * uuid128, uint16_t properties, uint8_t read_permission, uint8_t write_permission, uint8_t * data, uint16_t data_len);

/**
 * @brief Get address of constructed ATT DB
 */
uint8_t * att_db_util_get_address(void);

/**
 * @brief Get size of constructed ATT DB
 */
uint16_t att_db_util_get_size(void);

/**
 * @brief Get number of bytes that are included in GATT Database Hash
 */
uint16_t att_db_util_hash_len(void);

/**
 * @brief init generator for GATT Database Hash
 */
void att_db_util_hash_init(void);

/**
 * @brief get next byte from generator for GATT Database Hash
 */
uint8_t att_db_util_hash_get_next(void);

/**
 * @brief Calculate GATT Database Hash using crypto engine
 * @param request
 * @param db_hash
 * @param callback
 * @param callback_arg
 */
void att_db_util_hash_calc(btstack_crypto_aes128_cmac_t * request, uint8_t * db_hash, void (* callback)(void * arg), void * callback_arg);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
