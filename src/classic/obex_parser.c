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

#define BTSTACK_FILE__ "obex_parser.c"

#include "btstack_config.h"

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include <string.h>
#include "btstack_debug.h"
#include "btstack_util.h"

static uint16_t obex_parser_param_size_for_request(uint8_t opcode){
    switch (opcode) {
        case OBEX_OPCODE_SETPATH:
            return 4;
        case OBEX_OPCODE_CONNECT:
            return 6;
        default:
            return 2;
    }
}

static uint16_t obex_parser_param_size_for_response(uint8_t opcode){
    switch (opcode) {
        case OBEX_OPCODE_CONNECT:
            return 6;
        default:
            return 2;
    }
}

static void obex_parser_init(obex_parser_t * obex_parser, obex_parser_callback_t obex_parser_callback, void * user_data){
    memset(obex_parser, 0, sizeof(obex_parser_t));
    obex_parser->packet_size = 3;
    obex_parser->user_data = user_data;
    obex_parser->callback = obex_parser_callback;
}

void obex_parser_init_for_request(obex_parser_t * obex_parser, obex_parser_callback_t obex_parser_callback, void * user_data){
    obex_parser_init(obex_parser, obex_parser_callback, user_data);
    obex_parser->state = OBEX_PARSER_STATE_W4_OPCODE;
}

void obex_parser_init_for_response(obex_parser_t * obex_parser, uint8_t opcode, obex_parser_callback_t obex_parser_callback, void * user_data){
    obex_parser_init(obex_parser, obex_parser_callback, user_data);
    obex_parser->state = OBEX_PARSER_STATE_W4_RESPONSE_CODE;
    obex_parser->opcode = opcode;
}

obex_parser_object_state_t obex_parser_process_data(obex_parser_t *obex_parser, const uint8_t *data_buffer, uint16_t data_len) {
    while (data_len){
        uint16_t bytes_to_consume = 1;
        uint8_t  header_type;
        switch (obex_parser->state){
            case OBEX_PARSER_STATE_W4_OPCODE:
                obex_parser->opcode = *data_buffer;
                obex_parser->state = OBEX_PARSER_STATE_W4_PACKET_LEN;
                obex_parser->item_len = obex_parser_param_size_for_request(obex_parser->opcode);
                obex_parser->packet_size = 1 + obex_parser->item_len;
                break;
            case OBEX_PARSER_STATE_W4_RESPONSE_CODE:
                obex_parser->response_code = *data_buffer;
                obex_parser->state = OBEX_PARSER_STATE_W4_PACKET_LEN;
                obex_parser->item_len = obex_parser_param_size_for_response(obex_parser->opcode);
                obex_parser->packet_size = 1 + obex_parser->item_len;
                break;
            case OBEX_PARSER_STATE_W4_PACKET_LEN:
                bytes_to_consume = btstack_min(2 - obex_parser->item_pos, data_len);
                memcpy(&obex_parser->params[obex_parser->item_pos], data_buffer, bytes_to_consume);
                obex_parser->item_pos += bytes_to_consume;
                if (obex_parser->item_pos == 2){
                    // validate packet large enough for header
                    obex_parser->packet_size = big_endian_read_16(obex_parser->params, 0);
                    if (obex_parser->packet_size < (obex_parser->item_len + 1)){
                        // packet size smaller than opcode + params
                        obex_parser->state = OBEX_PARSER_STATE_INVALID;
                        break;
                    }
                    // params already complete?
                    if (obex_parser->item_len == 2){
                        obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_ID;
                    } else {
                        obex_parser->state = OBEX_PARSER_STATE_W4_PARAMS;
                    }
                }
                break;
            case OBEX_PARSER_STATE_W4_PARAMS:
                bytes_to_consume = btstack_min(obex_parser->item_len - obex_parser->item_pos, data_len);
                memcpy(&obex_parser->params[obex_parser->item_pos], data_buffer, bytes_to_consume);
                obex_parser->item_pos += bytes_to_consume;
                if (obex_parser->item_pos == obex_parser->item_len){
                    obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_ID;
                }
                break;
            case OBEX_PARSER_STATE_W4_HEADER_ID:
                obex_parser->header_id = *data_buffer; // constants in obex.h encode type as well, so just use these as well
                header_type = *data_buffer >> 6;
                obex_parser->item_pos = 0;
                switch (header_type){
                    case 0:
                    case 1:
                        // 16-bit length info prefixed
                        obex_parser->item_len = 2;
                        obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_LEN_FIRST;
                        break;
                    case 2:
                        // 8-bit value
                        obex_parser->item_len = 1;
                        obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_VALUE;
                        break;
                    case 3:
                        // 32-bit value
                        obex_parser->item_len = 4;
                        obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_VALUE;
                        break;
                    default:
                        // avoid compiler warning about unused cases (encoding in [0..3])
                        break;
                }
                break;
            case OBEX_PARSER_STATE_W4_HEADER_LEN_FIRST:
                obex_parser->item_len = *data_buffer << 8;
                obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_LEN_SECOND;
                break;
            case OBEX_PARSER_STATE_W4_HEADER_LEN_SECOND:
                obex_parser->item_len = obex_parser->item_len + *data_buffer;
                if (obex_parser->item_len < 3){
                    // len to small to even cover header
                    obex_parser->state = OBEX_PARSER_STATE_INVALID;
                    break;
                };
                if (obex_parser->item_len == 3){
                    // borderline: empty value
                    obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_ID;
                    break;
                }
                obex_parser->item_len -= 3;
                obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_VALUE;
                break;
            case OBEX_PARSER_STATE_W4_HEADER_VALUE:
                bytes_to_consume = btstack_min(obex_parser->item_len - obex_parser->item_pos, data_len);
                if (*obex_parser->callback != NULL){
                    (*obex_parser->callback)(obex_parser->user_data, obex_parser->header_id, obex_parser->item_len, obex_parser->item_pos, data_buffer, bytes_to_consume);
                }
                obex_parser->item_pos += bytes_to_consume;
                if (obex_parser->item_pos == obex_parser->item_len){
                    obex_parser->state = OBEX_PARSER_STATE_W4_HEADER_ID;
                }
                break;
            case OBEX_PARSER_STATE_COMPLETE:
                obex_parser->state = OBEX_PARSER_STATE_OVERRUN;
                break;
            case OBEX_PARSER_STATE_INVALID:
                break;
            default:
                btstack_unreachable();
                break;
        }

        data_buffer += bytes_to_consume;
        data_len    -= bytes_to_consume;
        obex_parser->packet_pos += bytes_to_consume;

        // all bytes read? then check state
        if (obex_parser->packet_pos == obex_parser->packet_size){
            if (obex_parser->state == OBEX_PARSER_STATE_W4_HEADER_ID){
                obex_parser->state = OBEX_PARSER_STATE_COMPLETE;
            } else {
                obex_parser->state = OBEX_PARSER_STATE_INVALID;
            }
        }
    }

    switch (obex_parser->state){
        case OBEX_PARSER_STATE_COMPLETE:
            return OBEX_PARSER_OBJECT_STATE_COMPLETE;
        case OBEX_PARSER_STATE_OVERRUN:
            return OBEX_PARSER_OBJECT_STATE_OVERRUN;
        case OBEX_PARSER_STATE_INVALID:
            return OBEX_PARSER_OBJECT_STATE_INVALID;
        default:
            return OBEX_PARSER_OBJECT_STATE_INCOMPLETE;
    }
}

void obex_parser_get_operation_info(obex_parser_t * obex_parser, obex_parser_operation_info_t * obex_operation_info){
    memset(obex_operation_info, 0, sizeof(obex_parser_operation_info_t));
    obex_operation_info->opcode = obex_parser->opcode;
    obex_operation_info->response_code = obex_parser->response_code;
    switch (obex_parser->opcode){
        case OBEX_OPCODE_CONNECT:
            obex_operation_info->obex_version_number = obex_parser->params[2];
            obex_operation_info->flags = obex_parser->params[3];
            obex_operation_info->max_packet_length = big_endian_read_16(obex_parser->params, 4);
            break;
        case OBEX_OPCODE_SETPATH:
            obex_operation_info->flags = obex_parser->params[2];
            break;
        default:
            break;
    }
}
obex_parser_header_state_t obex_parser_header_store(uint8_t * header_buffer, uint16_t buffer_size, uint16_t total_len,
                                                    uint16_t data_offset,  const uint8_t * data_buffer, uint16_t data_len){
    uint16_t bytes_to_store = btstack_min(buffer_size - data_offset, data_len);
    memcpy(&header_buffer[data_offset], data_buffer, bytes_to_store);
    uint16_t new_offset = data_offset + bytes_to_store;
    if (new_offset > buffer_size){
        return OBEX_PARSER_HEADER_OVERRUN;
    } else if (new_offset == total_len) {
        return OBEX_PARSER_HEADER_COMPLETE;
    } else {
        return OBEX_PARSER_HEADER_INCOMPLETE;
    }
}


/* OBEX App Param Parser */

void obex_app_param_parser_init(obex_app_param_parser_t * parser, obex_app_param_parser_callback_t callback, uint8_t param_size, void * user_data){
    parser->state = OBEX_APP_PARAM_PARSER_STATE_W4_TYPE;
    parser->callback = callback;
    parser->user_data = user_data;
    parser->param_size = param_size;
    parser->param_pos = 0;
}

obex_app_param_parser_params_state_t obex_app_param_parser_process_data(obex_app_param_parser_t *parser, const uint8_t *data_buffer, uint16_t data_len){
    while ((data_len > 0) && (parser->param_pos < parser->param_size)){
        uint16_t bytes_to_consume = 1;
        switch(parser->state){
            case OBEX_APP_PARAM_PARSER_STATE_INVALID:
                return OBEX_APP_PARAM_PARSER_PARAMS_STATE_INVALID;
            case OBEX_APP_PARAM_PARSER_STATE_W4_TYPE:
                parser->tag_id = *data_buffer;
                parser->state = OBEX_APP_PARAM_PARSER_STATE_W4_LEN;
                break;
            case OBEX_APP_PARAM_PARSER_STATE_W4_LEN:
                parser->tag_len = *data_buffer;
                if ((parser->param_pos + parser->tag_len) > parser->param_size){
                    parser->state = OBEX_APP_PARAM_PARSER_STATE_INVALID;
                    return OBEX_APP_PARAM_PARSER_PARAMS_STATE_INVALID;
                }
                parser->tag_pos = 0;
                parser->state = OBEX_APP_PARAM_PARSER_STATE_W4_VALUE;
                break;
            case OBEX_APP_PARAM_PARSER_STATE_W4_VALUE:
                bytes_to_consume = btstack_min(parser->tag_len - parser->tag_pos, data_len);
				// param_size is uint8_t
                (*parser->callback)(parser->user_data, parser->tag_id, (uint8_t) parser->tag_len, (uint8_t) parser->tag_pos, data_buffer, (uint8_t) bytes_to_consume);
                parser->tag_pos   += bytes_to_consume;
                if (parser->tag_pos == parser->tag_len){
                    parser->state = OBEX_APP_PARAM_PARSER_STATE_W4_TYPE;
                }
                break;
            default:
                btstack_unreachable();
                break;
        }

        data_buffer += bytes_to_consume;
        data_len    -= bytes_to_consume;
        parser->param_pos += bytes_to_consume;

        // all bytes read? then check state
        if (parser->param_pos == parser->param_size){
            if (parser->state == OBEX_APP_PARAM_PARSER_STATE_W4_TYPE){
                parser->state = OBEX_APP_PARAM_PARSER_STATE_COMPLETE;
            } else {
                parser->state = OBEX_APP_PARAM_PARSER_STATE_INVALID;
            }
        }
    }

    if (data_len > 0){
        return OBEX_APP_PARAM_PARSER_PARAMS_STATE_OVERRUN;
    }

    switch (parser->state){
        case OBEX_APP_PARAM_PARSER_STATE_COMPLETE:
            return OBEX_APP_PARAM_PARSER_PARAMS_STATE_COMPLETE;
        case OBEX_APP_PARAM_PARSER_STATE_INVALID:
            return OBEX_APP_PARAM_PARSER_PARAMS_STATE_INVALID;
        default:
            return OBEX_APP_PARAM_PARSER_PARAMS_STATE_INCOMPLETE;
    }
}


obex_app_param_parser_tag_state_t obex_app_param_parser_tag_store(uint8_t * header_buffer, uint8_t buffer_size, uint8_t total_len,
                                                                         uint8_t data_offset, const uint8_t * data_buffer, uint8_t data_len){
    uint16_t bytes_to_store = btstack_min(buffer_size - data_offset, data_len);
    memcpy(&header_buffer[data_offset], data_buffer, bytes_to_store);
    uint16_t new_offset = data_offset + bytes_to_store;
    if (new_offset > buffer_size){
        return OBEX_APP_PARAM_PARSER_TAG_OVERRUN;
    } else if (new_offset == total_len) {
        return OBEX_APP_PARAM_PARSER_TAG_COMPLETE;
    } else {
        return OBEX_APP_PARAM_PARSER_TAG_INCOMPLETE;
    }
}
