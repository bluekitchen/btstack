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
 * @title OPP Server
 *
 */

#ifndef OPP_SERVER_H
#define OPP_SERVER_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include <stdint.h>

// max name header len
#define OPP_SERVER_MAX_NAME_LEN 32

// max type header len
#define OPP_SERVER_MAX_TYPE_LEN 20

// max search value len
#define OPP_SERVER_MAX_SEARCH_VALUE_LEN 32

    /* API_START */

// PBAP Supported Features

#define PBAP_SUPPORTED_FEATURES_DOWNLOAD                        (1<<0)
#define PBAP_SUPPORTED_FEATURES_BROWSING                        (1<<1)

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

/**
 * Create SDP Record for OBEX Push Profile server
 * @param service
 * @param service_record_handle
 * @param rfcomm_channel_nr
 * @param l2cap_psm
 * @param name
 * @param num_supported_formats
 * @param supported_formats
 */
void opp_server_create_sdp_record(uint8_t *service, uint32_t service_record_handle, uint8_t rfcomm_channel_nr,
                                   uint16_t l2cap_psm, const char *name, uint8_t num_supported_formats,
                                   const uint8_t * supported_formats);

/**
 * Setup OPP Server
 * @param packet_handler
 * @param rfcomm_channel_nr
 * @param l2cap_psm
 * @return status
 */
uint8_t opp_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, gap_security_level_t security_level);

/**
 * @brief Create OPP connection to a OPP server on a remote device.
 * If the server requires authentication, a OPP_SUBEVENT_AUTHENTICATION_REQUEST is emitted, which
 * can be answered with opp_authentication_password(..).
 * The status of OPP connection establishment is reported via OPP_SUBEVENT_CONNECTION_OPENED event, 
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 *    
 * @param addr
 * @param out_cid to use for further commands
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_MEMORY_ALLOC_FAILED if PBAP or GOEP connection already exists.  
 */
uint8_t opp_server_connect( bd_addr_t addr, uint16_t * out_cid);

/**
 * @brief Disconnects OPP connection with given identifier.
 * Event OPP_SUBEVENT_CONNECTION_CLOSED indicates that OPP connection is closed.
 *
 * @param opp_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t opp_server_disconnect(uint16_t opp_cid);

/**
 * @brief Get max opp size for current response, given already set headers
 * @param opp_cid
 * @param response_code, see obex.h
 * @return max body size or zero if connection invalid
 */
uint16_t opp_server_get_max_body_size(uint16_t opp_cid);

/**
 * @brief Send response to OPP_SUBEVENT_PULL including optional headers
 * @param opp_cid
 * @param response_code, see obex.h
 * @param continuation - 0 if request complete, returned in next pull event
 * @param body_len
 * @param body
 * @return max body size or zero if connection invalid
 */
uint16_t opp_server_send_pull_response(uint16_t opp_cid, uint8_t response_code, uint32_t continuation, uint16_t body_len, const uint8_t * body);

/**
 * @brief De-Init OPP server
 */
void opp_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
