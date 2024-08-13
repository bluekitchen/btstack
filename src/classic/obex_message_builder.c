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

#define BTSTACK_FILE__ "obex_message_builder.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <stdlib.h>

#include "btstack_util.h"
#include "btstack_debug.h"
#include "classic/obex.h"
#include "classic/obex_message_builder.h"

#if defined(ENABLE_LOG_DEBUG) || defined(ENABLE_LOG_ERROR)
#define LUT(which) [which] = #which
const char* lut_type[0xFF] = {
LUT(OBEX_HEADER_NAME),
LUT(OBEX_HEADER_DESCRIPTION),
LUT(OBEX_HEADER_IMG_HANDLE),
LUT(OBEX_HEADER_TYPE),
LUT(OBEX_HEADER_TIME_ISO_8601),
LUT(OBEX_HEADER_TARGET),
LUT(OBEX_HEADER_HTTP),
LUT(OBEX_HEADER_BODY),
LUT(OBEX_HEADER_END_OF_BODY),
LUT(OBEX_HEADER_WHO),
LUT(OBEX_HEADER_APPLICATION_PARAMETERS),
LUT(OBEX_HEADER_AUTHENTICATION_CHALLENGE),
LUT(OBEX_HEADER_AUTHENTICATION_RESPONSE),
LUT(OBEX_HEADER_OBJECT_CLASS),
LUT(OBEX_HEADER_IMG_DESCRIPTOR),
LUT(OBEX_HEADER_SINGLE_RESPONSE_MODE),
LUT(OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER),
LUT(OBEX_HEADER_COUNT),
LUT(OBEX_HEADER_LENGTH),
LUT(OBEX_HEADER_TIME_4_BYTE),
LUT(OBEX_HEADER_CONNECTION_ID),
};
#endif

static uint8_t obex_message_builder_packet_init(uint8_t * buffer, uint16_t buffer_len, uint8_t opcode_or_response_code){
    if (buffer_len < 3) return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    buffer[0] = opcode_or_response_code;
    big_endian_store_16(buffer, 1, 3);
    return ERROR_CODE_SUCCESS;
}

static uint8_t obex_message_builder_packet_append(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint16_t len){
    uint16_t pos = big_endian_read_16(buffer, 1);
    
    log_debug("add type:0x%02x(%s) buffer_len:%u pos+len:%u pos:%u len:%u", data[0], lut_type[data[0]], buffer_len, pos + len, pos, len);
    
    if (buffer_len < pos + len) {
        log_error("ERROR_CODE_MEMORY_CAPACITY_EXCEEDED type:0x%02x(%s) buffer_len:%u size:%u pos:%u len:%u", buffer[0], lut_type[buffer[0]], buffer_len, pos + len, pos, len);
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }
    
    (void)memcpy(&buffer[pos], data, len);
    pos += len;
    big_endian_store_16(buffer, 1, pos);
     return ERROR_CODE_SUCCESS;
}

uint16_t obex_message_builder_get_message_length(uint8_t * buffer){
    return big_endian_read_16(buffer, 1);
}

uint8_t obex_message_builder_header_add_byte(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, uint8_t value){
    uint8_t header[2];
    header[0] = header_type;
    header[1] = value;
    return obex_message_builder_packet_append(buffer, buffer_len, &header[0], sizeof(header));
}

uint8_t obex_message_builder_header_add_word(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, uint32_t value){
    uint8_t header[5];
    header[0] = header_type;
    big_endian_store_32(header, 1, value);
    return obex_message_builder_packet_append(buffer, buffer_len, &header[0], sizeof(header));
}

uint8_t obex_message_builder_header_add_variable(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length){
    uint8_t header[3];
    header[0] = header_type;
    big_endian_store_16(header, 1, sizeof(header) + header_data_length);
    
    uint8_t status = obex_message_builder_packet_append(buffer, buffer_len, &header[0], sizeof(header));
    if (status != ERROR_CODE_SUCCESS) return status;

    return obex_message_builder_packet_append(buffer, buffer_len, header_data, header_data_length);        
}

uint8_t obex_message_builder_header_fillup_variable(uint8_t * buffer, uint16_t buffer_len, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length, uint32_t * ret_length){
    uint8_t header[3];
    header[0] = header_type;
    uint16_t pos = big_endian_read_16(buffer, 1);

    if (sizeof (header) + header_data_length + pos > buffer_len)
        header_data_length = buffer_len - pos - sizeof (header);

    big_endian_store_16(header, 1, sizeof(header) + header_data_length);
    *ret_length = 0;
    uint8_t status = obex_message_builder_packet_append(buffer, buffer_len, &header[0], sizeof(header));
    if (status != ERROR_CODE_SUCCESS) return status;

    *ret_length = header_data_length;
    return obex_message_builder_packet_append(buffer, buffer_len, header_data, header_data_length);
}

static uint8_t obex_message_builder_header_add_connection_id(uint8_t * buffer, uint16_t buffer_len, uint32_t obex_connection_id){
    // add connection_id header if set, must be first header if used
    if (obex_connection_id == OBEX_CONNECTION_ID_INVALID) return ERROR_CODE_PARAMETER_OUT_OF_MANDATORY_RANGE;
    return obex_message_builder_header_add_word(buffer, buffer_len, OBEX_HEADER_CONNECTION_ID, obex_connection_id);
}

static inline uint8_t obex_message_builder_create_connect(uint8_t * buffer, uint16_t buffer_len, uint8_t opcode,
    uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length){
    uint8_t status = obex_message_builder_packet_init(buffer, buffer_len, opcode);
    if (status != ERROR_CODE_SUCCESS) return status;

    uint8_t fields[4];
    fields[0] = obex_version_number;
    fields[1] = flags;
    big_endian_store_16(fields, 2, maximum_obex_packet_length);
    return obex_message_builder_packet_append(buffer, buffer_len, &fields[0], sizeof(fields));
}

uint8_t obex_message_builder_request_create_connect(uint8_t * buffer, uint16_t buffer_len, 
    uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length){

    return obex_message_builder_create_connect(buffer, buffer_len, OBEX_OPCODE_CONNECT, obex_version_number, flags, maximum_obex_packet_length);
}

uint8_t obex_message_builder_response_create_connect(uint8_t * buffer, uint16_t buffer_len, uint8_t obex_version_number, 
    uint8_t flags, uint16_t maximum_obex_packet_length, uint32_t obex_connection_id){

    uint8_t status = obex_message_builder_create_connect(buffer, buffer_len, OBEX_RESP_SUCCESS, obex_version_number, flags, maximum_obex_packet_length);
    if (status != ERROR_CODE_SUCCESS) return status;
    return obex_message_builder_header_add_connection_id(buffer, buffer_len, obex_connection_id);
}

uint8_t obex_message_builder_response_create_general(uint8_t * buffer, uint16_t buffer_len, uint8_t response_code){
    return obex_message_builder_packet_init(buffer, buffer_len, response_code);
}

uint8_t obex_message_builder_response_update_code(uint8_t * buffer, uint16_t buffer_len, uint8_t response_code){
    if (buffer_len < 3) return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    buffer[0] = response_code;
    return ERROR_CODE_SUCCESS;
}

uint8_t obex_message_builder_request_create_get(uint8_t * buffer, uint16_t buffer_len, uint32_t obex_connection_id){
    uint8_t status = obex_message_builder_packet_init(buffer, buffer_len, OBEX_OPCODE_GET | OBEX_OPCODE_FINAL_BIT_MASK);
    if (status != ERROR_CODE_SUCCESS) return status;
    return obex_message_builder_header_add_connection_id(buffer, buffer_len, obex_connection_id);
}

uint8_t obex_message_builder_request_create_put(uint8_t * buffer, uint16_t buffer_len, uint32_t obex_connection_id){
    uint8_t status = obex_message_builder_packet_init(buffer, buffer_len, OBEX_OPCODE_PUT | OBEX_OPCODE_FINAL_BIT_MASK);
    if (status != ERROR_CODE_SUCCESS) return status;

    return obex_message_builder_header_add_connection_id(buffer, buffer_len, obex_connection_id);
}

uint8_t obex_message_builder_request_create_set_path(uint8_t * buffer, uint16_t buffer_len, uint8_t flags, uint32_t obex_connection_id){
    uint8_t status = obex_message_builder_packet_init(buffer, buffer_len, OBEX_OPCODE_SETPATH);
    if (status != ERROR_CODE_SUCCESS) return status;

    uint8_t fields[2];
    fields[0] = flags;
    fields[1] = 0;  // reserved
    status = obex_message_builder_packet_append(buffer, buffer_len, &fields[0], sizeof(fields));
    if (status != ERROR_CODE_SUCCESS) return status;
    return obex_message_builder_header_add_connection_id(buffer, buffer_len, obex_connection_id);
}

uint8_t obex_message_builder_request_create_abort(uint8_t * buffer, uint16_t buffer_len, uint32_t obex_connection_id){
    uint8_t status = obex_message_builder_packet_init(buffer, buffer_len, OBEX_OPCODE_ABORT);
    if (status != ERROR_CODE_SUCCESS) return status;
    return obex_message_builder_header_add_connection_id(buffer, buffer_len, obex_connection_id);
}

uint8_t obex_message_builder_request_create_disconnect(uint8_t * buffer, uint16_t buffer_len, uint32_t obex_connection_id){
    uint8_t status = obex_message_builder_packet_init(buffer, buffer_len, OBEX_OPCODE_DISCONNECT);
    if (status != ERROR_CODE_SUCCESS) return status;
    return obex_message_builder_header_add_connection_id(buffer, buffer_len, obex_connection_id);
}

uint8_t obex_message_builder_set_final_bit (uint8_t * buffer, uint16_t buffer_len, bool final){
    if (buffer_len < 1) return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    if (buffer[0] == OBEX_OPCODE_CONNECT ||
        buffer[0] == OBEX_OPCODE_DISCONNECT ||
        buffer[0] == OBEX_OPCODE_SETPATH ||
        buffer[0] == OBEX_OPCODE_SESSION ||
        buffer[0] == OBEX_OPCODE_ABORT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    buffer[0] &= ~OBEX_OPCODE_FINAL_BIT_MASK;
    buffer[0] |= (final ? OBEX_OPCODE_FINAL_BIT_MASK : 0);
    return ERROR_CODE_SUCCESS;
}

uint8_t obex_message_builder_header_add_srm_enable(uint8_t * buffer, uint16_t buffer_len){
    log_debug("SRM header enabled");
    return obex_message_builder_header_add_byte(buffer, buffer_len, OBEX_HEADER_SINGLE_RESPONSE_MODE, OBEX_SRM_ENABLE);
}

uint8_t obex_message_builder_header_add_srmp_wait(uint8_t* buffer, uint16_t buffer_len) {
    return obex_message_builder_header_add_byte(buffer, buffer_len, OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER, OBEX_SRMP_WAIT);
}

uint8_t obex_message_builder_header_add_target(uint8_t * buffer, uint16_t buffer_len, const uint8_t * target, uint16_t length){
    return obex_message_builder_header_add_variable(buffer, buffer_len, OBEX_HEADER_TARGET, target, length);
}

uint8_t obex_message_builder_header_add_application_parameters(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint16_t length){
    return obex_message_builder_header_add_variable(buffer, buffer_len, OBEX_HEADER_APPLICATION_PARAMETERS, data, length);
}

uint8_t obex_message_builder_header_add_challenge_response(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint16_t length){
    return obex_message_builder_header_add_variable(buffer, buffer_len, OBEX_HEADER_AUTHENTICATION_RESPONSE, data, length);
}

uint8_t obex_message_builder_header_add_who(uint8_t * buffer, uint16_t buffer_len, const uint8_t * who){
    return obex_message_builder_header_add_variable(buffer, buffer_len, OBEX_HEADER_WHO, who, 16);
}

uint8_t obex_message_builder_body_add_static(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint32_t length){
    return obex_message_builder_header_add_variable(buffer, buffer_len, OBEX_HEADER_END_OF_BODY, data, length);
}

uint8_t obex_message_builder_body_fillup_static(uint8_t * buffer, uint16_t buffer_len, const uint8_t * data, uint32_t length, uint32_t *ret_length){
    return obex_message_builder_header_fillup_variable(buffer, buffer_len, OBEX_HEADER_END_OF_BODY, data, length, ret_length);
}

uint8_t obex_message_builder_get_header_name_len_from_strlen(uint16_t name_len) {
    
    // non-empty string have trailing \0
    bool add_trailing_zero = name_len > 0;
    return 1 + 2 + (name_len * 2) + (add_trailing_zero ? 2 : 0);
}

uint8_t obex_message_builder_header_add_unicode_prefix(uint8_t * buffer, uint16_t buffer_len, uint8_t header_id, const char * name, uint16_t name_len){
    // non-empty string have trailing \0
    bool add_trailing_zero = name_len > 0;

    uint16_t header_len = obex_message_builder_get_header_name_len_from_strlen(name_len);
    if (buffer_len < header_len) return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;

    uint16_t pos = big_endian_read_16(buffer, 1);
    buffer[pos++] = header_id;
    big_endian_store_16(buffer, pos, header_len);
    pos += 2;
    int i;
    // @note name[len] == 0
    for (i = 0 ; i < name_len ; i++){
        buffer[pos++] = 0;
        buffer[pos++] = *name++;
    }
    if (add_trailing_zero){
        buffer[pos++] = 0;
        buffer[pos++] = 0;
    }
    big_endian_store_16(buffer, 1, pos);
    return ERROR_CODE_SUCCESS;
}

uint8_t obex_message_builder_header_add_name_prefix(uint8_t * buffer, uint16_t buffer_len, const char * name, uint16_t name_len){
    return obex_message_builder_header_add_unicode_prefix(buffer, buffer_len, OBEX_HEADER_NAME, name, name_len);
}

uint8_t obex_message_builder_header_add_name(uint8_t * buffer, uint16_t buffer_len, const char * name){
    uint16_t name_len = (uint16_t) strlen(name);
    return obex_message_builder_header_add_unicode_prefix(buffer, buffer_len, OBEX_HEADER_NAME, name, name_len);
}

uint8_t obex_message_builder_get_header_type_len(char * type) {
    if (type == NULL)
        return 0;                // type_header is ommited

    return (uint8_t) ( 1            // Header Encoding + ID
                     + 2            // Length
                     + strlen(type) // length of string in bytes
                     + 1);           // trailing \0
}

uint8_t obex_message_builder_header_add_type(uint8_t * buffer, uint16_t buffer_len, const char * type){
    uint8_t header[3];
    header[0] = OBEX_HEADER_TYPE;
    int len_incl_zero = (uint16_t) strlen(type) + 1;
    big_endian_store_16(header, 1, 1 + 2 + len_incl_zero);

    uint8_t status = obex_message_builder_packet_append(buffer, buffer_len, &header[0], sizeof(header));
    if (status != ERROR_CODE_SUCCESS) return status;
    return obex_message_builder_packet_append(buffer, buffer_len, (const uint8_t*)type, len_incl_zero);
}

uint8_t obex_message_builder_header_add_length(uint8_t * buffer, uint16_t buffer_len, uint32_t length){
    return obex_message_builder_header_add_word(buffer, buffer_len, OBEX_HEADER_LENGTH, length);
}
