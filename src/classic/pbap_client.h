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

#ifndef __GOEP_CLIENT_H

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
 * @brief Get size of phone book from PSE
 * @param pbap_cid
 * @return status
 */
uint8_t pbap_get_phonebook_size(uint16_t pbap_cid);

/**
 * @brief Pull phone book from PSE
 * @param pbap_cid
 * @return status
 */
uint8_t pbap_pull_phonebook(uint16_t pbap_cid);

/**
 * @brief Lookup contact(s) by phone number
 * @param pbap_cid
 * @param phone_number
 * @return status
 */
uint8_t pbap_lookup_by_number(uint16_t pbap_cid, const char * phone_number);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
