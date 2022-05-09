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
 * @title PBAP Client
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

/**
 * Setup PhoneBook Access Client
 */
void pbap_client_init(void);

/**
 * @brief Create PBAP connection to a Phone Book Server (PSE) server on a remote device. 
 * If the server requires authentication, a PBAP_SUBEVENT_AUTHENTICATION_REQUEST is emitted, which
 * can be answered with pbap_authentication_password(..).
 * The status of PBAP connection establishment is reported via PBAP_SUBEVENT_CONNECTION_OPENED event, 
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 *    
 * @param handler 
 * @param addr
 * @param out_cid to use for further commands
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_MEMORY_ALLOC_FAILED if PBAP or GOEP connection already exists.  
 */
uint8_t pbap_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid);

/**
 * @brief Provide password for OBEX Authentication after receiving PBAP_SUBEVENT_AUTHENTICATION_REQUEST.
 * The status of PBAP connection establishment is reported via PBAP_SUBEVENT_CONNECTION_OPENED event, 
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 * 
 * @param pbap_cid
 * @param password (null terminated string) - not copied, needs to stay valid until connection completed
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_authentication_password(uint16_t pbap_cid, const char * password);

/** 
 * @brief Disconnects PBAP connection with given identifier.
 * Event PBAP_SUBEVENT_CONNECTION_CLOSED indicates that PBAP connection is closed.
 *
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_disconnect(uint16_t pbap_cid);

/** 
 * @brief Set current folder on PSE. The status is reported via PBAP_SUBEVENT_OPERATION_COMPLETED event. 
 * 
 * @param pbap_cid
 * @param path - note: path is not copied
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_phonebook(uint16_t pbap_cid, const char * path);

/**
 * @brief Set vCard Selector for get/pull phonebook. No event is emitted.
 * 
 * @param pbap_cid
 * @param vcard_selector - combination of PBAP_PROPERTY_MASK_* properties
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_vcard_selector(uint16_t pbap_cid, uint32_t vcard_selector);

/**
 * @brief Set vCard Selector for get/pull phonebook. No event is emitted.
 * 
 * @param pbap_cid
 * @param vcard_selector_operator - PBAP_VCARD_SELECTOR_OPERATOR_OR (default) or PBAP_VCARD_SELECTOR_OPERATOR_AND
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_vcard_selector_operator(uint16_t pbap_cid, int vcard_selector_operator);

/**
 * @brief Set Property Selector. No event is emitted.
 * @param pbap_cid
 * @param property_selector - combination of PBAP_PROPERTY_MASK_* properties
 * @return
 */
uint8_t pbap_set_property_selector(uint16_t pbap_cid, uint32_t property_selector);

/**
 * @brief Get size of phone book from PSE. The result is reported via PBAP_SUBEVENT_PHONEBOOK_SIZE event. 
 * 
 * @param pbap_cid
 * @param path - note: path is not copied, common path 'telecom/pb.vcf'
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_get_phonebook_size(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull phone book from PSE. The result is reported via registered packet handler (see pbap_connect function),
 * with packet type set to PBAP_DATA_PACKET. Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of the phone book. 
 * 
 * @param pbap_cid
 * @param path - note: path is not copied, common path 'telecom/pb.vcf'
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_pull_phonebook(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull vCard listing. vCard data is emitted via PBAP_SUBEVENT_CARD_RESULT event. 
 * Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of vCard listing.
 *
 * @param pbap_cid
 * @param path
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_pull_vcard_listing(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull vCard entry. The result is reported via callback (see pbap_connect function),
 * with packet type set to PBAP_DATA_PACKET.
 * Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of the vCard entry.
 * 
 * @param pbap_cid
 * @param path
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_pull_vcard_entry(uint16_t pbap_cid, const char * path);

/**
 * @brief Lookup contact(s) by phone number. vCard data is emitted via PBAP_SUBEVENT_CARD_RESULT event. 
 * Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of the lookup.
 *
 * @param pbap_cid
 * @param phone_number
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_lookup_by_number(uint16_t pbap_cid, const char * phone_number);

/**
 * @brief Abort current operation. No event is emitted.
 * 
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_abort(uint16_t pbap_cid);


/**
 * @brief Set flow control mode - default is off. No event is emitted.
 * @note When enabled, pbap_next_packet needs to be called after a packet was processed to receive the next one
 *
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_flow_control_mode(uint16_t pbap_cid, int enable);
    
/**
 * @brief Trigger next packet from PSE when Flow Control Mode is enabled.
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_next_packet(uint16_t pbap_cid);

/**
 * @brief De-Init PBAP Client
 */
void pbap_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
