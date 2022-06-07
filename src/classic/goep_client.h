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
 * @title GOEP Client 
 *
 * Communicate with remote OBEX server - General Object Exchange
 *
 */

#ifndef GOEP_CLIENT_H
#define GOEP_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif
 
#include <stdlib.h>
#include <string.h>

#include "btstack_defines.h"

/* API_START */

// remote does not expose PBAP features in SDP record
#define PBAP_FEATURES_NOT_PRESENT ((uint32_t) -1)

/**
 * Setup GOEP Client
 */
void goep_client_init(void);

/*
 * @brief Create GOEP connection to a GEOP server with specified UUID on a remote deivce.
 * @param handler 
 * @param addr
 * @param uuid
 * @param out_cid to use for further commands
 * @result status
*/
uint8_t goep_client_create_connection(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t uuid, uint16_t * out_cid);

/** 
 * @brief Disconnects GOEP connection with given identifier.
 * @param goep_cid
 * @return status
 */
uint8_t goep_client_disconnect(uint16_t goep_cid);

/** 
 * @brief Request emission of GOEP_SUBEVENT_CAN_SEND_NOW as soon as possible
 * @note GOEP_SUBEVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param goep_cid
 */
void goep_client_request_can_send_now(uint16_t goep_cid);

/**
 * @brief Get Opcode from last created request, needed for parsing of OBEX response packet
 * @param goep_cid
 * @return opcode
 */
uint8_t goep_client_get_request_opcode(uint16_t goep_cid);

/**
 * @brief Get PBAP Supported Features found in SDP record during connect
 */
uint32_t goep_client_get_pbap_supported_features(uint16_t goep_cid); 

/**
 * @brief Check if GOEP 2.0 or higher features can be used
 * @return true if GOEP Version 2.0 or higher
 */
bool goep_client_version_20_or_higher(uint16_t goep_cid);

/**
 * @brief Set Connection ID used for newly created requests
 * @param goep_cid
 */
void goep_client_set_connection_id(uint16_t goep_cid, uint32_t connection_id);

/**
 * @brief Start Connect request
 * @param goep_cid
 * @param obex_version_number
 * @param flags
 * @param maximum_obex_packet_length
 */
void goep_client_request_create_connect(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length);

/**
 * @brief Start Disconnect request
 * @param goep_cid
 */
void goep_client_request_create_disconnect(uint16_t goep_cid);

/**
 * @brief Create Get request
 * @param goep_cid
 */
void goep_client_request_create_get(uint16_t goep_cid);

/**
 * @brief Create Abort request
 * @param goep_cid
 */
void goep_client_request_create_abort(uint16_t goep_cid);

/**
 * @brief Start Set Path request
 * @param goep_cid
 */
void goep_client_request_create_set_path(uint16_t goep_cid, uint8_t flags);

/**
 * @brief Create Put request
 * @param goep_cid
 */
void goep_client_request_create_put(uint16_t goep_cid);

/**
 * @brief Get max size of body data that can be added to current response with goep_client_body_add_static
 * @param goep_cid
 * @param data
 * @param length
 * @return size in bytes or 0
 */
uint16_t goep_client_request_get_max_body_size(uint16_t goep_cid);

/**
 * @brief Add SRM Enable
 * @param goep_cid
 */
void goep_client_header_add_srm_enable(uint16_t goep_cid);

/**
 * @brief Add header with single byte value (8 bit)
 * @param goep_cid
 * @param header_type
 * @param value
 */
void goep_client_header_add_byte(uint16_t goep_cid, uint8_t header_type, uint8_t value);

/**
 * @brief Add header with word value (32 bit)
 * @param goep_cid
 * @param header_type
 * @param value
 */
void goep_client_header_add_word(uint16_t goep_cid, uint8_t header_type, uint32_t value);

/**
 * @brief Add header with variable size
 * @param goep_cid
 * @param header_type
 * @param header_data
 * @param header_data_length
 */
void goep_client_header_add_variable(uint16_t goep_cid, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length);

/**
 * @brief Add name header to current request
 * @param goep_cid
 * @param name
 */
void goep_client_header_add_name(uint16_t goep_cid, const char * name);

/**
 * @brief Add name header to current request
 * @param goep_cid
 * @param name
 * @param name_len
 */
void goep_client_header_add_name_prefix(uint16_t goep_cid, const char * name, uint16_t name_len);

/**
 * @brief Add target header to current request
 * @param goep_cid
 * @param target
 * @param length of target
 */
void goep_client_header_add_target(uint16_t goep_cid, const uint8_t * target, uint16_t length);

/**
 * @brief Add type header to current request
 * @param goep_cid
 * @param type
 */
void goep_client_header_add_type(uint16_t goep_cid, const char * type);

/**
 * @brief Add count header to current request
 * @param goep_cid
 * @param count
 */
void goep_client_header_add_count(uint16_t goep_cid, uint32_t count);

/**
 * @brief Add application parameters header to current request
 * @param goep_cid
 * @param data 
 * @param lenght of application parameters
 */
void goep_client_header_add_application_parameters(uint16_t goep_cid, const uint8_t * data, uint16_t length);

/**
 * @brief Add application parameters header to current request
 * @param goep_cid
 * @param data
 * @param lenght of challenge response
 */
void goep_client_header_add_challenge_response(uint16_t goep_cid, const uint8_t * data, uint16_t length);

/**
 * @brief Add body
 * @param goep_cid
 * @param data
 * @param lenght 
 */
void goep_client_body_add_static(uint16_t goep_cid, const uint8_t * data, uint32_t length);

/**
 * @brief Execute prepared request
 * @param goep_cid
 * @param daa 
 */
int goep_client_execute(uint16_t goep_cid);

/**
 * @brief De-Init GOEP Client
 */
void goep_client_deinit(void);

/* API_END */

// int goep_client_body_add_dynamic(uint16_t goep_cid, uint32_t length, void (*data_callback)(uint32_t offset, uint8_t * buffer, uint32_t len));

#if defined __cplusplus
}
#endif
#endif

