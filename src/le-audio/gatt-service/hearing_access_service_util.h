/*
 * Copyright (C) 2023 BlueKitchen GmbH
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
 * @title Hearing Access Service Util
 * 
 */

#ifndef HEARING_ACCESS_SERVICE_UTIL_H
#define HEARING_ACCESS_SERVICE_UTIL_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

#define HEARING_ACCESS_SERVICE_MIN_ATT_MTU                                                           49
#define HEARING_ACCESS_SERVICE_NUM_CHARACTERISTICS                                            3
#define HAS_PRESET_RECORD_NAME_MAX_LENGTH                                                    40

#define HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_OPCODE                                0x80
#define HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_WRITE_NAME_NOT_ALLOWED                        0x81
#define HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_SYNCHRONIZATION_NOT_SUPPORTED          0x82
#define HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_PRESET_OPERATION_NOT_POSSIBLE                 0x83
#define HAS_CONTROL_POINT_ATT_ERROR_RESPONSE_INVALID_PARAMETERS_LENGTH                     0x84

// HEARING_AID_FEATURE_MASK_HEARING_AID_TYPE 0-1
#define HEARING_AID_FEATURE_MASK_PRESET_SYNCHRONIZATION_SUPPORT      0x00000004
#define HEARING_AID_FEATURE_MASK_INDEPENDENT_PRESETS                 0x00000008
#define HEARING_AID_FEATURE_MASK_DYNAMIC_PRESETS                     0x00000010
#define HEARING_AID_FEATURE_MASK_WRITABLE_PRESETS_SUPPORT            0x00000020
                    
#define HEARING_AID_FEATURE_MASK_BINAURAL_HEARING_AID                0x00000001
#define HEARING_AID_FEATURE_MASK_MONAURAL_HEARING_AID                0x00000002
#define HEARING_AID_FEATURE_MASK_BANDED_HEARING_AID                  0x00000004

#define HEARING_AID_PRESET_PROPERTIES_MASK_WRITABLE                 0x00000001
#define HEARING_AID_PRESET_PROPERTIES_MASK_AVAILABLE                0x00000002

typedef enum {
    HAS_HEARING_AID_TYPE_BINAURAL = 0,
    HAS_HEARING_AID_TYPE_MONAURAL,
    HAS_HEARING_AID_TYPE_BANDED,
    HAS_HEARING_AID_TYPE_RFU 
} has_hearing_aid_type_t;

typedef enum {
    HAS_PRESET_SYNCHRONIZATION_NOT_SUPPORTED = 0,
    HAS_PRESET_SYNCHRONIZATION_SUPPORTED,
} has_preset_synchronization_support_type_t;

typedef enum {
    HAS_INDEPENDENT_PRESETS_IDENTICAL_ACROSS_COORDINATED_SET = 0,   // The list of preset records on this server is identical to the list of preset records in the other server of the Coordinated Set
    HAS_INDEPENDENT_PRESETS_NOT_IDENTICAL_ACROSS_COORDINATED_SET    // The list of preset records on this server may be different from the list of preset records in the other server of the Coordinated Set
} has_independent_presets_type_t;

typedef enum {
    HAS_DYNAMIC_PRESETS_IDENTICAL_UNCHANGEABLE = 0,     // The list of preset records does not change
    HAS_DYNAMIC_PRESETS_IDENTICAL_CHANGEABLE            // The list of preset records may change
} has_dynamic_presets_type_t;

typedef enum {
    HAS_WRITABLE_PRESETS_NOT_SUPPORTED = 0,             // The server does not support writable preset records
    HAS_WRITABLE_PRESETS_SUPPORTED                      //  The server supports writable preset records
} has_writable_presets_support_type_t;

typedef enum {
    HAS_OPCODE_RFU_0 = 0x00,
    HAS_OPCODE_READ_PRESETS_REQUEST = 0x01,
    HAS_OPCODE_READ_PRESET_RESPONSE,
    HAS_OPCODE_PRESET_CHANGED,
    HAS_OPCODE_WRITE_PRESET_NAME,
    HAS_OPCODE_SET_ACTIVE_PRESET,
    HAS_OPCODE_SET_NEXT_PRESET,
    HAS_OPCODE_SET_PREVIOUS_PRESET,
    HAS_OPCODE_SET_ACTIVE_PRESET_SYNCHRONIZED_LOCALLY,
    HAS_OPCODE_SET_NEXT_PRESET_SYNCHRONIZED_LOCALLY,
    HAS_OPCODE_SET_PREVIOUS_PRESET_SYNCHRONIZED_LOCALLY,
    HAS_OPCODE_RFU_1
} has_opcode_t;

typedef enum {
    HAS_CHANGEID_GENERIC_UPDATE = 0x00,
    HAS_CHANGEID_PRESET_RECORD_DELETED,
    HAS_CHANGEID_PRESET_RECORD_AVAILABLE,
    HAS_CHANGEID_PRESET_RECORD_UNAVAILABLE,
    HAS_CHANGEID_RFU
} has_changeid_t;

typedef struct {
    uint8_t index;
    uint8_t properties;
    char    name[HAS_PRESET_RECORD_NAME_MAX_LENGTH];
    bool    active;

    // used to buffer the position or record index, until all clients are notified of preset record change
    uint8_t calculated_position;

    // current preset change task, used in queue
    uint8_t scheduled_task;

    // marks the client that should receive notification on control point operation result
    hci_con_handle_t con_handle;
} has_preset_record_t;

#if defined __cplusplus
}
#endif

#endif
