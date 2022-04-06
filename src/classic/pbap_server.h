/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title PBAP Server
 *
 */

#ifndef PBAP_SERVER_H
#define PBAP_SERVER_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include <stdint.h>

// max len of phone number used for lookup in pbap_lookup_by_number
#define PBAP_MAX_PHONE_NUMBER_LEN 32

// max len of name reported in PBAP_SUBEVENT_CARD_RESULT
#define PBAP_MAX_NAME_LEN   32

// max len of vcard handle reported in PBAP_SUBEVENT_CARD_RESULT
#define PBAP_MAX_HANDLE_LEN 16

// max name header len
#define PBAP_SERVER_MAX_NAME_LEN 32

// max type header len
#define PBAP_SERVER_MAX_TYPE_LEN 20

    /* API_START */

// PBAP Supported Features

#define PBAP_SUPPORTED_FEATURES_DOWNLOAD                        (1<<0)
#define PBAP_SUPPORTED_FEATURES_BROWSING                        (1<<1)
#define PBAP_SUPPORTED_FEATURES_DATABASE_IDENTIFIER             (1<<2)
#define PBAP_SUPPORTED_FEATURES_FOLDER_VERSION_COUNTERS         (1<<3)
#define PBAP_SUPPORTED_FEATURES_VCARD_SELECTING                 (1<<4)
#define PBAP_SUPPORTED_FEATURES_ENHANCED_MISSED_CALLS           (1<<5)
#define PBAP_SUPPORTED_FEATURES_X_BT_UCI_VCARD_PROPERTY         (1<<6)
#define PBAP_SUPPORTED_FEATURES_X_BT_UID_VCARD_PROPERTY         (1<<7)
#define PBAP_SUPPORTED_FEATURES_CONTACT_REFERENCING             (1<<8)
#define PBAP_SUPPORTED_FEATURES_DEFAULT_CONTACT_IMAGE_FORMAT    (1<<9)

// PBAP Property Mask - also used for vCardSelector
#define PBAP_PROPERTY_MASK_VERSION              (1<< 0) // vCard Version
#define PBAP_PROPERTY_MASK_FN                   (1<< 1) // Formatted Name
#define PBAP_PROPERTY_MASK_N                    (1<< 2) // Structured Presentation of Name
#define PBAP_PROPERTY_MASK_PHOTO                (1<< 3) // Associated Image or Photo
#define PBAP_PROPERTY_MASK_BDAY                 (1<< 4) // Birthday
#define PBAP_PROPERTY_MASK_ADR                  (1<< 5) // Delivery Address
#define PBAP_PROPERTY_MASK_LABEL                (1<< 6) // Delivery
#define PBAP_PROPERTY_MASK_TEL                  (1<< 7) // Telephone Number
#define PBAP_PROPERTY_MASK_EMAIL                (1<< 8) // Electronic Mail Address
#define PBAP_PROPERTY_MASK_MAILER               (1<< 9) // Electronic Mail
#define PBAP_PROPERTY_MASK_TZ                   (1<<10) // Time Zone
#define PBAP_PROPERTY_MASK_GEO                  (1<<11) // Geographic Position
#define PBAP_PROPERTY_MASK_TITLE                (1<<12) // Job
#define PBAP_PROPERTY_MASK_ROLE                 (1<<13) // Role within the Organization
#define PBAP_PROPERTY_MASK_LOGO                 (1<<14) // Organization Logo
#define PBAP_PROPERTY_MASK_AGENT                (1<<15) // vCard of Person Representing
#define PBAP_PROPERTY_MASK_ORG                  (1<<16) // Name of Organization
#define PBAP_PROPERTY_MASK_NOTE                 (1<<17) // Comments
#define PBAP_PROPERTY_MASK_REV                  (1<<18) // Revision
#define PBAP_PROPERTY_MASK_SOUND                (1<<19) // Pronunciation of Name
#define PBAP_PROPERTY_MASK_URL                  (1<<20) // Uniform Resource Locator
#define PBAP_PROPERTY_MASK_UID                  (1<<21) // Unique ID
#define PBAP_PROPERTY_MASK_KEY                  (1<<22) // Public Encryption Key
#define PBAP_PROPERTY_MASK_NICKNAME             (1<<23) // Nickname
#define PBAP_PROPERTY_MASK_CATEGORIES           (1<<24) // Categories
#define PBAP_PROPERTY_MASK_PROID                (1<<25) // Product ID
#define PBAP_PROPERTY_MASK_CLASS                (1<<26) // Class information
#define PBAP_PROPERTY_MASK_SORT_STRING          (1<<27) // String used for sorting operations
#define PBAP_PROPERTY_MASK_X_IRMC_CALL_DATETIME (1<<28) // Time stamp
#define PBAP_PROPERTY_MASK_X_BT_SPEEDDIALKEY    (1<<29) // Speed-dial shortcut
#define PBAP_PROPERTY_MASK_X_BT_UCI             (1<<30) // Uniform Caller Identifier
#define PBAP_PROPERTY_MASK_X_BT_UID             (1<<31) // Bluetooth Contact Unique Identifier

// PBAP vCardSelectorOperator
#define PBAP_VCARD_SELECTOR_OPERATOR_OR          0
#define PBAP_VCARD_SELECTOR_OPERATOR_AND         1

/**
 * Create SDP Record for Phonebook Access Server
 * @param service
 * @param service_record_handle
 * @param rfcomm_channel_nr
 * @param l2cap_psm
 * @param name
 */
void pbap_server_create_sdp_record(uint8_t *service, uint32_t service_record_handle, uint8_t rfcomm_channel_nr,
                                   uint16_t l2cap_psm, const char *name, uint8_t supported_repositories,
                                   uint32_t pbap_supported_features);

/**
 * Setup PhoneBook Access Server
 * @param packet_handler
 * @param rfcomm_channel_nr
 * @param l2cap_psm
 * @return status
 */
uint8_t pbap_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, gap_security_level_t security_level);

/**
 * @brief Create PBAP connection to a Phone Book Client (PCE) server on a remote device.
 * If the server requires authentication, a PBAP_SUBEVENT_AUTHENTICATION_REQUEST is emitted, which
 * can be answered with pbap_authentication_password(..).
 * The status of PBAP connection establishment is reported via PBAP_SUBEVENT_CONNECTION_OPENED event, 
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 *    
 * @param addr
 * @param out_cid to use for further commands
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_MEMORY_ALLOC_FAILED if PBAP or GOEP connection already exists.  
 */
uint8_t pbap_server_connect( bd_addr_t addr, uint16_t * out_cid);

/**
 * @brief Disconnects PBAP connection with given identifier.
 * Event PBAP_SUBEVENT_CONNECTION_CLOSED indicates that PBAP connection is closed.
 *
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_server_disconnect(uint16_t pbap_cid);

/**
 * @brief De-Init PBAP Client
 */
void pbap_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
