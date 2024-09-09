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
 * @title Telephone Bearer Service Util
 * 
 */

#ifndef TELEPHONE_BEARER_SERVICE_UTIL_H
#define TELEPHONE_BEARER_SERVICE_UTIL_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

#define TBS_NAME_ATT_ERROR_RESPONSE_VALUE_CHANGED_DURING_READ_LONG  0x80

#define TBS_OPCODE_MASK_LOCAL_HOLD       0x01
#define TBS_OPCODE_MASK_JOIN             0x02

// this enumeration represents the order in which notifications are processed, if multiple are pending, from highest to lowest.
// So to guarantee that the CALL_STATE gets notified before other characteristics including the call_id
// the CALL_STATE needs to have a higher value than the others, so come after them in this enumeration.
typedef enum {
    TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME = 0,
    TBS_CHARACTERISTIC_INDEX_BEARER_UCI,
    TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY,
    TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST,
    TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH,
    TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL,
    TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS,
    TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID,
    TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS,
    TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI,
    TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT,
    TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES,
    TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON,
    TBS_CHARACTERISTIC_INDEX_INCOMING_CALL,
    TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME,
    TBS_CHARACTERISTIC_INDEX_CALL_STATE,
    TBS_CHARACTERISTICS_NUM,
} tbs_characteristic_index_t;

typedef enum {
    TBS_CONTROL_POINT_OPCODE_ACCEPT = 0,
    TBS_CONTROL_POINT_OPCODE_TERMINATE,
    TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD,
    TBS_CONTROL_POINT_OPCODE_LOCAL_RETRIEVE,
    TBS_CONTROL_POINT_OPCODE_ORIGINATE,
    TBS_CONTROL_POINT_OPCODE_JOIN,
    TBS_CONTROL_POINT_OPCODE_RFU
} tbs_control_point_opcode_t;

typedef enum {
    TBS_CONTROL_POINT_RESULT_SUCCESS = 0,
    TBS_CONTROL_POINT_RESULT_OPCODE_NOT_SUPPORTED,
    TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE,
    TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX,
    TBS_CONTROL_POINT_RESULT_STATE_MISMATCH,
    TBS_CONTROL_POINT_RESULT_LACK_OF_RESOURCES,
    TBS_CONTROL_POINT_RESULT_INVALID_OUTGOING_URI,
    TBS_CONTROL_POINT_RESULT_RFU
} tbs_control_point_notification_result_codes_t;

typedef enum {
    TBS_CALL_TERMINATION_REASON_MALFORMED_URI,
    TBS_CALL_TERMINATION_REASON_CALL_FAILED,
    TBS_CALL_TERMINATION_REASON_REMOTE_ENDED_CALL,
    TBS_CALL_TERMINATION_REASON_SERVER_ENDED_CALL,
    TBS_CALL_TERMINATION_REASON_LINE_BUSY,
    TBS_CALL_TERMINATION_REASON_NETWORK_CONGESTION,
    TBS_CALL_TERMINATION_REASON_CLIENT_ENDED_CALL,
    TBS_CALL_TERMINATION_REASON_NO_SERVICE,
    TBS_CALL_TERMINATION_REASON_NO_ANSWER,
    TBS_CALL_TERMINATION_REASON_UNSPECIFIED,
    TBS_CALL_TERMINATION_REASON_RFU,
} tbs_call_termination_reason_t;

typedef enum {
    TBS_TECHNOLOGY_3G = 1,
    TBS_TECHNOLOGY_4G,
    TBS_TECHNOLOGY_LTE,
    TBS_TECHNOLOGY_WI_FI,
    TBS_TECHNOLOGY_5G,
    TBS_TECHNOLOGY_GSM,
    TBS_TECHNOLOGY_CDMA,
    TBS_TECHNOLOGY_WCDMA,
} tbs_technology_t;

extern const uint16_t tbs_characteristic_uuids[TBS_CHARACTERISTICS_NUM];

#ifdef ENABLE_TESTING_SUPPORT
/**
 * Converts a characteristic specified by a given index into a readable name
 *
 * @param index
 * @return name of given characteristic index
 */
const char *tbs_characteristic_index_to_name( tbs_characteristic_index_t index );

/**
 * Return list of characteristics names
 *
 * @return names of all characteristics
 */
const char ** tbs_get_characteristic_names(void);
#endif

/**
 * Given a characteristic by index returns the corresponding UUID
 *
 * @param index
 * @return uuid16 of given characteristic index
 */
uint16_t tbs_characteristic_index_to_uuid( tbs_characteristic_index_t index );

/**
 * Returns the sub-event code of a given characteristic
 *
 * @param index
 * @return sub-event code
 */
uint8_t tbs_characteristic_index_to_subevent( tbs_characteristic_index_t index );

/**
 * Returns if the Characteristic provides per call information
 * @param index
 * @return
 */
bool tbs_characteristic_index_uses_call_index( tbs_characteristic_index_t index );

#if defined __cplusplus
}
#endif

#endif
