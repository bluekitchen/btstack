/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * OBEX Parser
 * Parser incoming arbitrarily chunked OBEX object
 */

#ifndef OBEX_PARSER_H
#define OBEX_PARSER_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    OBEX_PARSER_STATE_W4_OPCODE,
    OBEX_PARSER_STATE_W4_RESPONSE_CODE,
    OBEX_PARSER_STATE_W4_PACKET_LEN,
    OBEX_PARSER_STATE_W4_PARAMS,
    OBEX_PARSER_STATE_W4_HEADER_ID,
    OBEX_PARSER_STATE_W4_HEADER_LEN_FIRST,
    OBEX_PARSER_STATE_W4_HEADER_LEN_SECOND,
    OBEX_PARSER_STATE_W4_HEADER_VALUE,
    OBEX_PARSER_STATE_COMPLETE,
    OBEX_PARSER_STATE_OVERRUN,
    OBEX_PARSER_STATE_INVALID,
} obex_parser_state_t;

/* API_START */

/**
 * Callback to process chunked data
 * @param user_data provided in obex_parser_init
 * @param header_id current OBEX header ID
 * @param total_len of header
 * @param data_offset
 * @param data_len
 * @param data_buffer
 */
typedef void (*obex_parser_callback_t)(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len);

typedef enum {
    OBEX_PARSER_OBJECT_STATE_INCOMPLETE,
    OBEX_PARSER_OBJECT_STATE_COMPLETE,
    OBEX_PARSER_OBJECT_STATE_OVERRUN,
    OBEX_PARSER_OBJECT_STATE_INVALID,
} obex_parser_object_state_t;

typedef enum {
    OBEX_PARSER_HEADER_INCOMPLETE,
    OBEX_PARSER_HEADER_COMPLETE,
    OBEX_PARSER_HEADER_OVERRUN,
} obex_parser_header_state_t;

typedef struct {
    uint8_t  opcode;
    uint8_t  response_code;
    // Connect only
    uint8_t  obex_version_number;
    uint16_t max_packet_length;
    // Connect + Set Path only
    uint8_t  flags;
} obex_parser_operation_info_t;

typedef struct {
    obex_parser_callback_t callback;
    void * user_data;
    uint16_t packet_size;
    uint16_t packet_pos;
    obex_parser_state_t state;
    uint8_t  opcode;
    uint8_t  response_code;
    uint16_t item_len;
    uint16_t item_pos;
    uint8_t  params[6]; // for connect and set path
    uint8_t  header_id;
} obex_parser_t;

/**
 * Callback to process chunked data
 * @param user_data provided in obex_parser_init
 * @param header_id current OBEX header ID
 * @param total_len of header
 * @param data_offset
 * @param data_len
 * @param data_buffer
 */
typedef void (*obex_app_param_parser_callback_t)(void * user_data, uint8_t tag_id, uint8_t total_len, uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len);

typedef enum {
    OBEX_APP_PARAM_PARSER_PARAMS_STATE_INCOMPLETE,
    OBEX_APP_PARAM_PARSER_PARAMS_STATE_COMPLETE,
    OBEX_APP_PARAM_PARSER_PARAMS_STATE_OVERRUN,
    OBEX_APP_PARAM_PARSER_PARAMS_STATE_INVALID,
} obex_app_param_parser_params_state_t;

typedef enum {
    OBEX_APP_PARAM_PARSER_TAG_INCOMPLETE,
    OBEX_APP_PARAM_PARSER_TAG_COMPLETE,
    OBEX_APP_PARAM_PARSER_TAG_OVERRUN,
} obex_app_param_parser_tag_state_t;

typedef enum {
    OBEX_APP_PARAM_PARSER_STATE_W4_TYPE = 0,
    OBEX_APP_PARAM_PARSER_STATE_W4_LEN,
    OBEX_APP_PARAM_PARSER_STATE_W4_VALUE,
    OBEX_APP_PARAM_PARSER_STATE_COMPLETE,
    OBEX_APP_PARAM_PARSER_STATE_INVALID,
    OBEX_APP_PARAM_PARSER_STATE_OVERRUN,
} obex_app_param_parser_state_t;

typedef struct {
    obex_app_param_parser_callback_t callback;
    obex_app_param_parser_state_t state;
    void * user_data;
    uint16_t param_size;
    uint16_t param_pos;
    uint16_t tag_len;
    uint16_t tag_pos;
    uint8_t  tag_id;
} obex_app_param_parser_t;

/**
 * Initialize OBEX Parser for next OBEX request
 * @param obex_parser
 * @param function to call for field data
 * @param user_data provided to callback function
 */
void obex_parser_init_for_request(obex_parser_t * obex_parser, obex_parser_callback_t obex_parser_callback, void * user_data);

/**
 * Initialize OBEX Parser for next OBEX response
 * @param obex_parser
 * @param opcode of request - needed as responses with additional fields like connect and set path
 * @param function to call for field data
 * @param user_data provided to callback function
 */
void obex_parser_init_for_response(obex_parser_t * obex_parser, uint8_t opcode, obex_parser_callback_t obex_parser_callback, void * user_data);

/**
 * Process OBEX data
 * @param obex_parser
 * @param data_len
 * @param data_buffer
 * @return OBEX_PARSER_OBJECT_STATE_COMPLETE if packet has been completely parsed
 */
obex_parser_object_state_t obex_parser_process_data(obex_parser_t *obex_parser, const uint8_t *data_buffer, uint16_t data_len);

/**
 * Get operation info for request/response packets
 * @param obex_parser
 * @return
 */
void obex_parser_get_operation_info(obex_parser_t * obex_parser, obex_parser_operation_info_t * obex_operation_info);

/**
 * Helper to collect header chunks in fixed-size header buffer
 * @param header_buffer
 * @param buffer_size of header_buffer
 * @param total_len of header value
 * @param data_offset of chunk to store
 * @param data_buffer
 * @param data_len chunk length
 * @return OBEX_PARSER_HEADER_COMPLETE when header value complete
 */
obex_parser_header_state_t obex_parser_header_store(uint8_t * header_buffer, uint16_t buffer_size, uint16_t total_len,
                                                    uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len);

/**
 * Initialize OBEX Application Param Parser
 * @param parser
 * @param function to call for tag data
 * @param param_size of OBEX_HEADER_APPLICATION_PARAMETERS header
 * @param user_data provided to callback function
 */
void obex_app_param_parser_init(obex_app_param_parser_t * parser, obex_app_param_parser_callback_t callback, uint16_t param_size, void * user_data);

/**
 * Process OBEX App Param data
 * @param parser
 * @param data_len
 * @param data_buffer
 * @return OBEX_APP_PARAM_PARSER_PARAMS_STATE_COMPLETE if packet has been completely parsed
 */
obex_app_param_parser_params_state_t obex_app_param_parser_process_data(obex_app_param_parser_t *parser, const uint8_t *data_buffer, uint16_t data_len);

/**
 * Helper to collect tag chunks in fixed-size data buffer
 * @param tag_buffer
 * @param buffer_size of data buffer
 * @param total_len of tag value
 * @param data_offset of chunk to store
 * @param data_buffer
 * @param data_len chunk length
 * @return OBEX_APP_PARAM_PARSER_TAG_COMPLETE when tag complete
 */

obex_app_param_parser_tag_state_t obex_app_param_parser_tag_store(uint8_t * tag_buffer, uint8_t buffer_size, uint8_t total_len,
                                                                  uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len);


/* API_END */

#if defined __cplusplus
}
#endif
#endif
