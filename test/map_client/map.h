/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#ifndef MAP_H
#define MAP_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include <stdint.h>

/* API_START */
#define MAP_MAX_VALUE_LEN    32
#define MAP_MESSAGE_HANDLE_SIZE 8
/**
 * @brief MAP Message handle
 */
typedef uint8_t map_message_handle_t[MAP_MESSAGE_HANDLE_SIZE];

typedef enum {
    MAP_MESSAGE_TYPE_EMAIL = 0,
    MAP_MESSAGE_TYPE_SMS_GSM,
    MAP_MESSAGE_TYPE_SMS_CDMA,
    MAP_MESSAGE_TYPE_MMS,
    MAP_MESSAGE_TYPE_IM
} map_message_type_t;

typedef enum {
    MAP_RECEPTION_STATUS_COMPLETE,    // Complete message has been received by the MSE
    MAP_RECEPTION_STATUS_FRACTIONED,  // Only a part of the message has been received by the MSE (e.g., fractioned email of push-service)
    MAP_RECEPTION_STATUS_NOTIFICATION // Only a notification of the message has been received by the MSE
} map_reception_status_t;

typedef enum {
    MAP_FEATURE_NOTIFICATION_REGISTRATION = 0,
    MAP_FEATURE_NOTIFICATION,
    MAP_FEATURE_BROWSING,
    MAP_FEATURE_UPLOADING,
    MAP_FEATURE_DELETE,
    MAP_FEATURE_INSTANCE_INFORMATION,
    MAP_FEATURE_EXTENDED_EVENT_REPORT_1_1,
    MAP_FEATURE_EVENT_REPORT_VERSION_1_2,
    MAP_FEATURE_MESSAGE_FORMAT_VERSION_1_1,
    MAP_FEATURE_MESSAGES_LISTING_FORMAT_VERSION_1_1,
    MAP_FEATURE_PERSISTENT_MESSAGE_HANDLES,
    MAP_FEATURE_DATABASE_IDENTIFIER,
    MAP_FEATURE_FOLDER_VERSION_COUNTER,
    MAP_FEATURE_CONVERSATION_VERSION_COUNTERS,
    MAP_FEATURE_PARTICIPANT_PRESENCE_CHANGE_NOTIFICATION,
    MAP_FEATURE_PARTICIPANT_CHAT_STATE_CHANGE_NOTIFICATION,
    MAP_FEATURE_PBAP_CONTACT_CROSS_REFERENCE,
    MAP_FEATURE_NOTIFICATION_FILTERING,
    MAP_FEATURE_UTC_OFFSET_TIMESTAMP_FORMAT,
    MAP_FEATURE_MAPSUPPORTEDFEATURES_IN_CONNECT_REQUEST,
    MAP_FEATURE_CONVERSATION_LISTING,
    MAP_FEATURE_OWNER_STATUS_BIT,
    MAP_FEATURE_MESSAGE_FORWARDING
} map_feature_t;

typedef struct {
    char handle[MAP_MESSAGE_HANDLE_SIZE];
    char datetime[MAP_MAX_VALUE_LEN];
    
    char subject[MAP_MAX_VALUE_LEN];
    char sender_name[MAP_MAX_VALUE_LEN];
    char sender_addressing[MAP_MAX_VALUE_LEN];
    
    map_message_type_t type;
    
    uint16_t attachment_size;
    uint8_t flags; // 0 - read, 1 - text
} map_message_t;

/* API_END */

#if defined __cplusplus
}
#endif
#endif
