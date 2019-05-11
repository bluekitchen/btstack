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

#ifndef PBAP_CLIENT_H
#define PBAP_CLIENT_H

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
 * Setup PhoneBook Access Client
 */
void pbap_client_init(void);

/**
 * @brief Create PBAP connection to a Phone Book Server (PSE) server on a remote deivce.
 * @param handler 
 * @param addr
 * @param out_cid to use for further commands
 * @result status
*/
uint8_t pbap_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid);

/**
 * @brief Provide password for OBEX Authentication after receiving PBAP_SUBEVENT_AUTHENTICATION_REQUEST
 * @param pbap_cid
 * @param password (null terminated string) - not copied, needs to stay valid until connection completed
*/
uint8_t pbap_authentication_password(uint16_t pbap_cid, const char * password);

/** 
 * @brief Disconnects PBAP connection with given identifier.
 * @param pbap_cid
 * @return status
 */
uint8_t pbap_disconnect(uint16_t pbap_cid);

/** 
 * @brief Set current folder on PSE
 * @param pbap_cid
 * @param path - note: path is not copied
 * @return status
 */
uint8_t pbap_set_phonebook(uint16_t pbap_cid, const char * path);

/**
 * @brief Set vCard Selector for get/pull phonebook
 * @param pbap_cid
 * @param vcard_selector - combination of PBAP_PROPERTY_MASK_* properties
 * @return status
 */
uint8_t pbap_set_vcard_selector(uint16_t pbap_cid, uint32_t vcard_selector);

/**
 * @brief Set vCard Selector for get/pull phonebook
 * @param pbap_cid
 * @param vcard_selector_operator - PBAP_VCARD_SELECTOR_OPERATOR_OR (default) or PBAP_VCARD_SELECTOR_OPERATOR_AND
 * @return status
 */
uint8_t pbap_set_vcard_selector_operator(uint16_t pbap_cid, int vcard_selector_operator);

/**
 * @brief Get size of phone book from PSE
 * @param pbap_cid
 * @param path - note: path is not copied, common path 'telecom/pb.vcf'
 * @return status
 */
uint8_t pbap_get_phonebook_size(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull phone book from PSE
 * @param pbap_cid
 * @param path - note: path is not copied, common path 'telecom/pb.vcf'
 * @return status
 */
uint8_t pbap_pull_phonebook(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull vCard listing
 * @param pbap_cid
 * @param path
 * @return status
 */
uint8_t pbap_pull_vcard_listing(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull vCard entry
 * @param pbap_cid
 * @param path
 * @return status
 */
uint8_t pbap_pull_vcard_entry(uint16_t pbap_cid, const char * path);

/**
 * @brief Lookup contact(s) by phone number
 * @param pbap_cid
 * @param phone_number
 * @return status
 */
uint8_t pbap_lookup_by_number(uint16_t pbap_cid, const char * phone_number);

/**
 * @brief Abort current operation
 * @param pbap_cid
 * @return status
 */
uint8_t pbap_abort(uint16_t pbap_cid);


/**
 * @brief Set flow control mode - default is off
 * @note When enabled, pbap_next_packet needs to be called after a packet was processed to receive the next one
 * @param pbap_cid
 * @return status
 */
uint8_t pbap_set_flow_control_mode(uint16_t pbap_cid, int enable);
    
/**
 * @brief Prigger next packet from PSE when Flow Control Mode is enabled
 * @param pbap_cid
 * @return status
 */
uint8_t pbap_next_packet(uint16_t pbap_cid);


/* API_END */

#if defined __cplusplus
}
#endif
#endif
