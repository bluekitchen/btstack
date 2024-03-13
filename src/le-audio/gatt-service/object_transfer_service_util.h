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
 * @title Media Control Service Server
 * 
 */

#ifndef OBJECT_TRANSFER_SERVICE_UTIL_H
#define OBJECT_TRANSFER_SERVICE_UTIL_H

#include <stdint.h>
#include "le-audio/le_audio.h"

#if defined __cplusplus
extern "C" {
#endif

#define OBJECT_TRANSFER_SERVICE_NUM_CHARACTERISTICS                       14

#define ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED                   0x80
#define ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED                      0x81
#define ATT_ERROR_RESPONSE_OTS_CONCURRENCY_LIMIT_EXCEEDED               0x82
#define ATT_ERROR_RESPONSE_OTS_OBJECT_NAME_ALREADY_EXISTS               0x83
#define ATT_ERROR_RESPONSE_ATT_ERROR_CCCD_IMPROPERLY_CONFIGURED         0xFD

// OACP (Object Action Control Point) feature masks
#define OACP_FEATURE_MASK_CREATE                                        0x0001
#define OACP_FEATURE_MASK_DELETE                                        0x0002
#define OACP_FEATURE_MASK_CALCULATE_CHECKSUM                            0x0004
#define OACP_FEATURE_MASK_EXECUTE                                       0x0008
#define OACP_FEATURE_MASK_READ                                          0x0010
#define OACP_FEATURE_MASK_WRITE                                         0x0020
#define OACP_FEATURE_MASK_APPENDING_ADDITIONAL_DATA_TO_OBJECTS          0x0040
#define OACP_FEATURE_MASK_TRUNCATION_OF_OBJECTS                         0x0080
#define OACP_FEATURE_MASK_PATCHING_OF_OBJECTS                           0x0100
#define OACP_FEATURE_MASK_ABORT                                         0x0200

// OLCP (Object List Control Point) feature masks
#define OLCP_FEATURE_MASK_GO_TO                                         0x0001
#define OLCP_FEATURE_MASK_ORDER                                         0x0002
#define OLCP_FEATURE_MASK_REQUEST_NUMBER_OF_OBJECTS                     0x0004
#define OLCP_FEATURE_MASK_CLEAR_MARKING                                 0x0008

// Object Properties
#define OBJECT_PROPERTY_MASK_DELETE                                     0x0001
#define OBJECT_PROPERTY_MASK_EXECUTE                                    0x0002
#define OBJECT_PROPERTY_MASK_READ                                       0x0004
#define OBJECT_PROPERTY_MASK_WRITE                                      0x0008
#define OBJECT_PROPERTY_MASK_APPEND                                     0x0010
#define OBJECT_PROPERTY_MASK_TRUNCATE                                   0x0020
#define OBJECT_PROPERTY_MASK_PATCH                                      0x0040
#define OBJECT_PROPERTY_MASK_MARK                                       0x0080


#define OTS_MAX_STRING_LENGHT         60
#define OTS_MAX_NUM_FILTERS          3
#define OTS_OBJECT_ID_LEN            6
/**
 * @brief Bluetooth address
 */
typedef uint8_t ots_object_id_t[OTS_OBJECT_ID_LEN];

/* API_START */

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hours;
    uint8_t  minutes;
    uint8_t  seconds;
} btstack_utc_t;

typedef enum {
    OTS_FILTER_TYPE_NO_FILTER = 0,              // 0
    OTS_FILTER_TYPE_NAME_STARTS_WITH,           // var
    OTS_FILTER_TYPE_NAME_ENDS_WITH,             // var
    OTS_FILTER_TYPE_NAME_CONTAINS,              // var
    OTS_FILTER_TYPE_NAME_IS_EXACTLY,            // var
    OTS_FILTER_TYPE_OBJECT_TYPE,                // 6
    OTS_FILTER_TYPE_CREATED_BETWEEN,            // 14
    OTS_FILTER_TYPE_MODIFIED_BETWEEN,           // 14
    OTS_FILTER_TYPE_CURRENT_SIZE_BETWEEN,       // 8
    OTS_FILTER_TYPE_ALLOCATED_SIZE_BETWEEN,     // 8
    OTS_FILTER_TYPE_MARKED_OBJECTS,             // 0
    OTS_FILTER_TYPE_RFU
} ots_filter_type_t;

typedef enum {
    OTS_OBJECT_CHANGED_FLAG_SOURCE_OF_CHANGE = 0, // 0 - Server, 1 - Client
    OTS_OBJECT_CHANGED_FLAG_OBJECT_CONTENTS_CHANGED,
    OTS_OBJECT_CHANGED_FLAG_OBJECT_METADATA_CHANGED,
    OTS_OBJECT_CHANGED_FLAG_OBJECT_CREATED,
    OTS_OBJECT_CHANGED_FLAG_OBJECT_DELETED,
    OTS_OBJECT_CHANGED_FLAG_RFU
} ots_object_changed_flag_t;

typedef enum {
    OACP_OPCODE_READY  = 0x00, 
    OACP_OPCODE_CREATE = 0x01,           
    OACP_OPCODE_DELETE,             
    OACP_OPCODE_CALCULATE_CHECKSUM,              
    OACP_OPCODE_EXECUTE,            
    OACP_OPCODE_READ,                
    OACP_OPCODE_WRITE,           
    OACP_OPCODE_ABORT,          
    OACP_OPCODE_RFU,
    OACP_OPCODE_RESPONSE_CODE = 0x60,
} oacp_opcode_t;

typedef enum {
    OACP_RESULT_CODE_SUCCESS = 0x01,
    OACP_RESULT_CODE_OP_CODE_NOT_SUPPORTED,
    OACP_RESULT_CODE_INVALID_PARAMETER,
    OACP_RESULT_CODE_INSUFFICIENT_RESOURCES,
    OACP_RESULT_CODE_INVALID_OBJECT,
    OACP_RESULT_CODE_CHANNEL_UNAVAILABLE,
    OACP_RESULT_CODE_UNSUPPORTED_TYPE,
    OACP_RESULT_CODE_PROCEDURE_NOT_PERMITTED,
    OACP_RESULT_CODE_OBJECT_LOCKED,
    OACP_RESULT_CODE_OPERATION_FAILED,
    OACP_RESULT_CODE_OPERATION_RFU
} oacp_result_code_t;

typedef enum {
    OLCP_OPCODE_READY = 0x00,
    OLCP_OPCODE_FIRST = 0x01,
    OLCP_OPCODE_LAST,             
    OLCP_OPCODE_PREVIOUS,              
    OLCP_OPCODE_NEXT,            
    OLCP_OPCODE_GOTO,
    OLCP_OPCODE_ORDER,
    OLCP_OPCODE_REQUEST_NUMBER_OF_OBJECTS,           
    OLCP_OPCODE_CLEAR_MARKING,          
    OLCP_OPCODE_RFU,
    OLCP_OPCODE_RESPONSE_CODE = 0x70
} olcp_opcode_t;

typedef enum {
    OLCP_RESULT_CODE_SUCCESS = 0x01,
    OLCP_RESULT_CODE_OP_CODE_NOT_SUPPORTED,
    OLCP_RESULT_CODE_INVALID_PARAMETER,
    OLCP_RESULT_CODE_OPERATION_FAILED,
    OLCP_RESULT_CODE_OUT_OF_BOUNDS,
    OLCP_RESULT_CODE_TOO_MANY_OBJECTS,
    OLCP_RESULT_CODE_NO_OBJECT,
    OLCP_RESULT_CODE_OBJECT_ID_NOT_FOUND,
    OLCP_RESULT_CODE_OPERATION_RFU
} olcp_result_code_t;

typedef enum {
    OLCP_LIST_SORT_ORDER_NONE = 0x00,
    OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_ASCENDING,
    OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_ASCENDING,
    OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_ASCENDING, 
    OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_ASCENDING,
    OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_ASCENDING,
    OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_DESCENDING = 0x11,
    OLCP_LIST_SORT_ORDER_BY_OBJECT_TYPE_DESCENDING,
    OLCP_LIST_SORT_ORDER_BY_OBJECT_CURRENT_SIZE_DESCENDING, 
    OLCP_LIST_SORT_ORDER_BY_FIRST_CREATED_DESCENDING,
    OLCP_LIST_SORT_ORDER_BY_LAST_CREATED_DESCENDING,
    OLCP_LIST_SORT_ORDER_RFU
} olcp_list_sort_order_t;

typedef enum {
    OTS_OBJECT_TYPE_DIRECTORY_LISTING = 0x2ACB,
    OTS_OBJECT_TYPE_MEDIA_PLAYER_ICON = 0x2BA9,
    OTS_OBJECT_TYPE_TRACK_SEGMENTS = 0x2BAA,
    OTS_OBJECT_TYPE_TRACK = 0x2BAB,
    OTS_OBJECT_TYPE_GROUP = 0x2BAC
} ots_object_type_t;

typedef enum {
    OTS_GROUP_OBJECT_TYPE_TRACK = 0,
    OTS_GROUP_OBJECT_TYPE_GROUP
} ots_group_object_type_t;

typedef struct {
    ots_filter_type_t type;
    uint8_t value_length;
    uint8_t value[OTS_MAX_STRING_LENGHT];
} ots_filter_t;

typedef struct {
    // metadata

    // Locally Unique Identifier: 0x000000000000 - Directory Listing Object, [0x000000000001, 0x0000000000FF] - RFU
    // luid >= 0x000000000100
    ots_object_id_t luid;

    uint32_t properties;

    char name[OTS_MAX_STRING_LENGHT];
    ots_object_type_t type;
    uint8_t  type_uuid128[16];

    // allocated_size = 0 if object not initialized
    uint32_t allocated_size;
    uint32_t current_size;

    btstack_utc_t first_created;
    btstack_utc_t last_modified;
} ots_object_t;


/* API_END */

#if defined __cplusplus
}
#endif

#endif

