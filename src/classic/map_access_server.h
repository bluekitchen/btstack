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
 *  @brief TODO
 */

#ifndef MAP_ACCESS_SERVER_H
#define MAP_ACCESS_SERVER_H

#if defined __cplusplus
extern "C" {
#endif
#include "btstack_config.h"
#include "btstack_defines.h"
// max len of phone number used for lookup in map_lookup_by_number
#define MAP_MAX_PHONE_NUMBER_LEN 32

// max len of name reported in MAP_SUBEVENT_CARD_RESULT
#define MAP_MAX_NAME_LEN   32

// max len of vcard handle reported in MAP_SUBEVENT_CARD_RESULT
#define MAP_MAX_HANDLE_LEN 16

// max name header len
#define MAP_SERVER_MAX_NAME_LEN 32

// max type header len
#define MAP_SERVER_MAX_TYPE_LEN 30

// max search value len
#define MAP_SERVER_MAX_SEARCH_VALUE_LEN 32

    /* API_START */

// MAP Supported Features

#define MAP_SUPPORTED_FEATURES_DOWNLOAD                        (1<<0)
#define MAP_SUPPORTED_FEATURES_BROWSING                        (1<<1)
#define MAP_SUPPORTED_FEATURES_DATABASE_IDENTIFIER             (1<<2)
#define MAP_SUPPORTED_FEATURES_FOLDER_VERSION_COUNTERS         (1<<3)
#define MAP_SUPPORTED_FEATURES_MSG_SELECTING                   (1<<4)
#define MAP_SUPPORTED_FEATURES_ENHANCED_MISSED_CALLS           (1<<5)
#define MAP_SUPPORTED_FEATURES_X_BT_UCI_MSG_PROPERTY           (1<<6)
#define MAP_SUPPORTED_FEATURES_X_BT_UID_MSG_PROPERTY           (1<<7)
#define MAP_SUPPORTED_FEATURES_CONTACT_REFERENCING             (1<<8)
#define MAP_SUPPORTED_FEATURES_DEFAULT_CONTACT_IMAGE_FORMAT    (1<<9)

// MAP Property Mask - also used for vCardSelector
#define MAP_PROPERTY_MASK_VERSION              (1<< 0) // vCard Version
#define MAP_PROPERTY_MASK_FN                   (1<< 1) // Formatted Name
#define MAP_PROPERTY_MASK_N                    (1<< 2) // Structured Presentation of Name
#define MAP_PROPERTY_MASK_PHOTO                (1<< 3) // Associated Image or Photo
#define MAP_PROPERTY_MASK_BDAY                 (1<< 4) // Birthday
#define MAP_PROPERTY_MASK_ADR                  (1<< 5) // Delivery Address
#define MAP_PROPERTY_MASK_LABEL                (1<< 6) // Delivery
#define MAP_PROPERTY_MASK_TEL                  (1<< 7) // Telephone Number
#define MAP_PROPERTY_MASK_EMAIL                (1<< 8) // Electronic Mail Address
#define MAP_PROPERTY_MASK_MAILER               (1<< 9) // Electronic Mail
#define MAP_PROPERTY_MASK_TZ                   (1<<10) // Time Zone
#define MAP_PROPERTY_MASK_GEO                  (1<<11) // Geographic Position
#define MAP_PROPERTY_MASK_TITLE                (1<<12) // Job
#define MAP_PROPERTY_MASK_ROLE                 (1<<13) // Role within the Organization
#define MAP_PROPERTY_MASK_LOGO                 (1<<14) // Organization Logo
#define MAP_PROPERTY_MASK_AGENT                (1<<15) // vCard of Person Representing
#define MAP_PROPERTY_MASK_ORG                  (1<<16) // Name of Organization
#define MAP_PROPERTY_MASK_NOTE                 (1<<17) // Comments
#define MAP_PROPERTY_MASK_REV                  (1<<18) // Revision
#define MAP_PROPERTY_MASK_SOUND                (1<<19) // Pronunciation of Name
#define MAP_PROPERTY_MASK_URL                  (1<<20) // Uniform Resource Locator
#define MAP_PROPERTY_MASK_UID                  (1<<21) // Unique ID
#define MAP_PROPERTY_MASK_KEY                  (1<<22) // Public Encryption Key
#define MAP_PROPERTY_MASK_NICKNAME             (1<<23) // Nickname
#define MAP_PROPERTY_MASK_CATEGORIES           (1<<24) // Categories
#define MAP_PROPERTY_MASK_PROID                (1<<25) // Product ID
#define MAP_PROPERTY_MASK_CLASS                (1<<26) // Class information
#define MAP_PROPERTY_MASK_SORT_STRING          (1<<27) // String used for sorting operations
#define MAP_PROPERTY_MASK_X_IRMC_CALL_DATETIME (1<<28) // Time stamp
#define MAP_PROPERTY_MASK_X_BT_SPEEDDIALKEY    (1<<29) // Speed-dial shortcut
#define MAP_PROPERTY_MASK_X_BT_UCI             (1<<30) // Uniform Caller Identifier
#define MAP_PROPERTY_MASK_X_BT_UID             (1<<31) // Bluetooth Contact Unique Identifier

// MAP vCardSelectorOperator
#define MAP_MSG_SELECTOR_OPERATOR_OR          0
#define MAP_MSG_SELECTOR_OPERATOR_AND         1

// MAP Format
typedef enum {
    MAP_FORMAT_MSG_10 = 0,
    MAP_FORMAT_MSG_11
} map_format_msg_t;

// MAP Object Types
typedef enum {
    MAP_OBJECT_TYPE_INVALID = 0,
    MAP_OBJECT_TYPE_MSG_LISTING,
    MAP_OBJECT_TYPE_MESSAGE,
} map_object_type_t;

// MAP Folders
typedef enum {
    MAS_FOLDER_MIN, MAS_FOLDER_INVALID = 0,
    MAS_FOLDER_ROOT,
    MAS_FOLDER_TELECOM,
    MAS_FOLDER_TELECOM_MSG, 
    MAS_FOLDER_TELECOM_MSG_INBOX,
    MAS_FOLDER_MAX,
} mas_folder_t;


// lengths
#define MAS_DATABASE_IDENTIFIER_LEN 16
#define MAS_FOLDER_VERSION_LEN 16

uint16_t map_access_server_send_get_response(uint16_t map_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t* body);
uint16_t map_access_server_get_max_body_size(uint16_t map_cid);
uint8_t map_access_server_set_new_messages(uint16_t map_cid, uint16_t new_messages);
uint8_t map_access_server_set_folder_version(uint16_t map_cid, const uint8_t* primary_folder_version);
uint8_t map_access_server_set_database_identifier(uint16_t map_cid, const uint8_t* database_identifier);
void map_access_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, uint16_t mtu);

#if defined __cplusplus
}
#endif
#endif // MAP_ACCESS_SERVER_H
