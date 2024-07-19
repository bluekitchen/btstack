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
 * OBEX Message Builder
 *
 */

#ifndef OBEX_MESSAGE_BUILDER_H
#define OBEX_MESSAGE_BUILDER_H

#if defined __cplusplus
extern "C" {
#endif
 
#include <stdint.h>
#include <string.h>

#include "btstack_defines.h"

//------------------------------------------------------------------------------------------------------------
// obex_message_builder.h
// 
// Functions to incrementaly construct an OBEX message. The current length of the message is stored at 
// offset 1 in the buffer. All functions return status code ERROR_CODE_MEMORY_CAPACITY_EXCEEDED id the 
// buffer is too small.

/* API_START */
/**
 * @brief Get message lenght
 * @param buffer
 * @return buffer_length
 */
uint16_t obex_message_builder_get_message_length(uint8_t * buffer);

/**
 * @brief Start Connect request
 * @param buffer
 * @param buffer_len
 * @param obex_version_number
 * @param flags
 * @param maximum_obex_packet_length
 * @return status
 */
uint8_t obex_message_builder_request_create_connect(uint8_t * buffer, uint16_t buffer_len, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length);

/**
 * @brief Start Connect response
 * @param buffer
 * @param buffer_len
 * @param obex_version_number
 * @param flags
 * @param maximum_obex_packet_length
 * @param connection_id
 * @return status
 */
uint8_t obex_message_builder_response_create_connect(uint8_t * buffer, uint16_t buffer_len, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length, uint32_t connection_id);

/**
 * @brief Start General response with given response code
 * @param buffer
 * @param buffer_len
 * @param response_code
 * @return status
 */
 uint8_t obex_message_builder_response_create_general(uint8_t * buffer, uint16_t buffer_len, uint8_t response_code);

/**
 * @brief Update response code
 * @param buffer
 * @param buffer_len
 * @param response_code
 * @return status
 */
uint8_t obex_message_builder_response_update_code(uint8_t * buffer, uint16_t buffer_len, uint8_t response_code);

/**
 * @brief Start Disconnect request
 * @param buffer
 * @param buffer_len
 * @param connection_id
 * @return status
 */
uint8_t obex_message_builder_request_create_disconnect(uint8_t * buffer, uint16_t buffer_len, uint32_t connection_id);

/**
 * @brief Create Get request
 * @param buffer
 * @param buffer_len
 * @param connection_id
 * @return status
 */
uint8_t obex_message_builder_request_create_get(uint8_t * buffer, uint16_t buffer_len, uint32_t connection_id);

/**
 * @brief Create Put request
 * @param buffer
 * @param buffer_len
 * @param connection_id
 * @return status
 */
uint8_t obex_message_builder_request_create_put(uint8_t * buffer, uint16_t buffer_len, uint32_t connection_id);


/**
 * @brief Create Abort request
 * @param buffer
 * @param buffer_len
 * @param connection_id
 * @return status
 */
uint8_t obex_message_builder_request_create_abort(uint8_t * buffer, uint16_t buffer_len, uint32_t connection_id);

/**
 * @brief Start Set Path request
 * @param buffer
 * @param buffer_len
 * @param connection_id
 * @return status
 */
uint8_t obex_message_builder_request_create_set_path(uint8_t * buffer, uint16_t buffer_len, uint8_t flags, uint32_t connection_id);

/**
 * @brief Control the "final" bit
 * @param buffer
 * @param buffer_len
 * @param final
 * @return status
 */
uint8_t obex_message_builder_set_final_bit (uint8_t * buffer, uint16_t buffer_len, bool final);

/**
 * @brief Add SRM Enable
 * @param buffer
 * @param buffer_len
 * @return status
 */
uint8_t obex_message_builder_header_add_srm_enable(uint8_t * buffer, uint16_t buffer_len);

/**
 * @brief Add SRMP wait header
 * @param buffer
 * @param buffer_len
 * @return status
 */
uint8_t obex_message_builder_header_add_srmp_wait(uint8_t* buffer, uint16_t buffer_len);


/**
 * @brief Add header with single byte value (8 bit)
 * @param buffer
 * @param buffer_len
 * @param header_type
 * @param value
 * @return status
 */
uint8_t obex_message_builder_header_add_byte(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, uint8_t value);

/**
 * @brief Add header with word value (32 bit)
 * @param buffer
 * @param buffer_len
 * @param header_type
 * @param value
 * @return status
 */
uint8_t obex_message_builder_header_add_word(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, uint32_t value);

/**
 * @brief Add header with variable size
 * @param buffer
 * @param buffer_len
 * @param header_type
 * @param header_data
 * @param header_data_length
 * @return status
 */
uint8_t obex_message_builder_header_add_variable(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length);

/**
 * @brief Add (partial) header with variable size
 * @param buffer
 * @param buffer_len
 * @param header_type
 * @param header_data
 * @param header_data_length
 * @param ret_length
 * @return status
 */
uint8_t obex_message_builder_header_fillup_variable(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length, uint32_t * ret_length);


/**
 * @brief returns name header size in bytes from its string len
 * @param name_len - use value returned by strlen
 * @return header size in bytes
 */
uint8_t obex_message_builder_get_header_name_len_from_strlen(uint16_t name_len);

/**
 * @brief Add name header to current request
 * @param buffer
 * @param buffer_len
 * @param name with trailing '\0'
 * @return status
 */
uint8_t obex_message_builder_header_add_name(uint8_t * buffer, uint16_t buffer_len, const char * name);

/**
 * @brief Add name header to current request
 * @param buffer
 * @param buffer_len
 * @param name
 * @param name_len
 * @return status
 */
uint8_t obex_message_builder_header_add_name_prefix(uint8_t * buffer, uint16_t buffer_len, const char * name, uint16_t name_len);

/**
 * @brief Add string encoded in unicode to current request
 * @param buffer
 * @param buffer_len
 * @param header_id
 * @param string
 * @param string_len
 * @return status
 */
uint8_t obex_message_builder_header_add_unicode_prefix(uint8_t * buffer, uint16_t buffer_len, uint8_t header_id, const char * name, uint16_t name_len);

/**
 * @brief Add target header to current request
 * @param buffer
 * @param buffer_len
 * @param target
 * @param length of target
 * @return status
 */
uint8_t obex_message_builder_header_add_target(uint8_t * buffer, uint16_t buffer_len, const uint8_t * target, uint16_t length);

/**
 * @brief Add target header to current request
 * @param buffer
 * @param buffer_len
 * @param who  fixed len of 16 bytes
 * @return status
 */
uint8_t obex_message_builder_header_add_who(uint8_t * buffer, uint16_t buffer_len, const uint8_t * who);

/**
 * @brief returns the length of the type header for string <type>
 * @param type
 * @return header len in bytes
 */
uint8_t obex_message_builder_get_header_type_len(char* type);


/**
 * @brief Add type header to current request
 * @param buffer
 * @param buffer_len
 * @param type
 * @return status
 */
uint8_t obex_message_builder_header_add_type(uint8_t * buffer, uint16_t buffer_len, const char * type);

/**
 * @brief Add count header to current request
 * @param buffer
 * @param buffer_len
 * @param count
 * @return status
 */
uint8_t obex_message_builder_header_add_count(uint8_t * buffer, uint16_t buffer_len, uint32_t count);

/**
 * @brief Add length header to current request
 * @param buffer
 * @param buffer_len
 * @param lenght
 * @return status
 */
uint8_t obex_message_builder_header_add_length(uint8_t * buffer, uint16_t buffer_len, uint32_t length);

/**
 * @brief Add application parameters header to current request
 * @param buffer
 * @param buffer_len
 * @param data 
 * @param length of application parameters
 * @return status
 */
uint8_t obex_message_builder_header_add_application_parameters(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint16_t length);

/**
 * @brief Add application parameters header to current request
 * @param buffer
 * @param buffer_len
 * @param data
 * @param length of challenge response
 * @return status
 */
uint8_t obex_message_builder_header_add_challenge_response(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint16_t length);

/**
 * @brief Add body
 * @param buffer
 * @param buffer_len
 * @param data
 * @param length 
 * @return status
 */
uint8_t obex_message_builder_body_add_static(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint32_t length);

/**
 * @brief Add body
 * @param buffer
 * @param buffer_len
 * @param data
 * @param length
 * @param ret_length
 * @return status
 */
uint8_t obex_message_builder_body_fillup_static(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint32_t length, uint32_t *ret_length);

/* API_END */

// int  obex_message_builder_body_add_dynamic(uint8_t * buffer, uint16_t buffer_len, uint32_t length, void (*data_callback)(uint32_t offset, uint8_t * buffer, uint32_t len));

#if defined __cplusplus
}
#endif
#endif

