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

#define BTSTACK_FILE__ "telephone_bearer_service_util.c"


#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/telephone_bearer_service_util.h"

const uint16_t tbs_characteristic_uuids[TBS_CHARACTERISTICS_NUM] = {
    [TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS]                 = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_LIST_CURRENT_CALLS,
    [TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME]                      = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_PROVIDER_NAME,
    [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL] = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL,
    [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH]                    = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_SIGNAL_STRENGTH,
    [TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY]                         = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_TECHNOLOGY,
    [TBS_CHARACTERISTIC_INDEX_BEARER_UCI]                                = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_UCI,
    [TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST]         = ORG_BLUETOOTH_CHARACTERISTIC_BEARER_URI_SCHEMES_SUPPORTED_LIST,
    [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES]       = ORG_BLUETOOTH_CHARACTERISTIC_CALL_CONTROL_POINT_OPTIONAL_OPCODES,
    [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT]                        = ORG_BLUETOOTH_CHARACTERISTIC_CALL_CONTROL_POINT,
    [TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME]                        = ORG_BLUETOOTH_CHARACTERISTIC_CALL_FRIENDLY_NAME,
    [TBS_CHARACTERISTIC_INDEX_CALL_STATE]                                = ORG_BLUETOOTH_CHARACTERISTIC_CALL_STATE,
    [TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID]                        = ORG_BLUETOOTH_CHARACTERISTIC_CONTENT_CONTROL_ID,
    [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI]           = ORG_BLUETOOTH_CHARACTERISTIC_INCOMING_CALL_TARGET_BEARER_URI,
    [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL]                             = ORG_BLUETOOTH_CHARACTERISTIC_INCOMING_CALL,
    [TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS]                              = ORG_BLUETOOTH_CHARACTERISTIC_STATUS_FLAGS,
    [TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON]                        = ORG_BLUETOOTH_CHARACTERISTIC_TERMINATION_REASON,
};

const uint8_t tbs_characteristic_meta_subevent[TBS_CHARACTERISTICS_NUM] = {
    [TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS]                 = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_LIST_CURRENT_CALLS,
    [TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME]                      = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_PROVIDER_NAME,
    [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL] = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL,
    [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH]                    = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_SIGNAL_STRENGTH,
    [TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY]                         = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_TECHNOLOGY,
    [TBS_CHARACTERISTIC_INDEX_BEARER_UCI]                                = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_UCI,
    [TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST]         = LEAUDIO_SUBEVENT_TBS_CLIENT_BEARER_URI_SCHEMES_SUPPORTED_LIST,
    [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES]       = LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_CONTROL_POINT_OPTIONAL_OPCODES,
    [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT]                        = LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_CONTROL_POINT,
    [TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME]                        = LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_FRIENDLY_NAME,
    [TBS_CHARACTERISTIC_INDEX_CALL_STATE]                                = LEAUDIO_SUBEVENT_TBS_CLIENT_CALL_STATE,
    [TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID]                        = LEAUDIO_SUBEVENT_TBS_CLIENT_CONTENT_CONTROL_ID,
    [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI]           = LEAUDIO_SUBEVENT_TBS_CLIENT_INCOMING_CALL_TARGET_BEARER_URI,
    [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL]                             = LEAUDIO_SUBEVENT_TBS_CLIENT_INCOMING_CALL,
    [TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS]                              = LEAUDIO_SUBEVENT_TBS_CLIENT_STATUS_FLAGS,
    [TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON]                        = LEAUDIO_SUBEVENT_TBS_CLIENT_TERMINATION_REASON,
};

#ifdef ENABLE_TESTING_SUPPORT
static const char * tbs_characteristic_names[TBS_CHARACTERISTICS_NUM] = {
    [TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS]                 = "Bearer List Current Calls",
    [TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME]                      = "Bearer Provider Name",
    [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL] = "Bearer Signal Strength Reporting Interval",
    [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH]                    = "Bearer Signal Strength",
    [TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY]                         = "Bearer Technology",
    [TBS_CHARACTERISTIC_INDEX_BEARER_UCI]                                = "Bearer UCI",
    [TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST]         = "Bearer URI Schemes Supported List",
    [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES]       = "Call Control Point Optional Opcodes",
    [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT]                        = "Call Control Point",
    [TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME]                        = "Call Friendly Name",
    [TBS_CHARACTERISTIC_INDEX_CALL_STATE]                                = "Call State",
    [TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID]                        = "Content Control ID",
    [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI]           = "Incoming Call Target Bearer URI",
    [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL]                             = "Incoming Call",
    [TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS]                              = "Status Flags",
    [TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON]                        = "Termination Reason",
//    "RFU"
};

const char ** tbs_get_characteristic_names(void){
    return tbs_characteristic_names;
}

const char *tbs_characteristic_index_to_name( tbs_characteristic_index_t index ) {
    btstack_assert( index < TBS_CHARACTERISTICS_NUM );
    return tbs_characteristic_names[index];
}
#endif

uint16_t tbs_characteristic_index_to_uuid( tbs_characteristic_index_t index ) {
    btstack_assert( index < TBS_CHARACTERISTICS_NUM );
    return tbs_characteristic_uuids[index];
}

uint8_t tbs_characteristic_index_to_subevent( tbs_characteristic_index_t index ) {
    btstack_assert( index < TBS_CHARACTERISTICS_NUM );
    return tbs_characteristic_meta_subevent[index];
}

bool tbs_characteristic_index_uses_call_index( tbs_characteristic_index_t index ){
    switch (index){
        case TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME:
        case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI:
        case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL:
        case TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON:
            return true;
        default:
            return false;
    }
}
