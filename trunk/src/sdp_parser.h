/*
 * Copyright (C) 2009-2013 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  sdp_parser.h
 */

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/sdp_util.h>

#if defined __cplusplus
extern "C" {
#endif

/* SDP Parser */

// Data Element stream parser helper
typedef struct de_state {
    uint8_t  in_state_GET_DE_HEADER_LENGTH ;
    uint32_t addon_header_bytes;
    uint32_t de_size;
    uint32_t de_offset;
} de_state_t;
void de_state_init(de_state_t * state);
int  de_state_size(uint8_t eventByte, de_state_t *de_state);

// SDP Parser

// Basic SDP Parser event type
typedef enum sdp_parser_event_type {
    SDP_PARSER_ATTRIBUTE_VALUE = 1,
    SDP_PARSER_COMPLETE,
} sdp_parser_event_type_t;

typedef struct sdp_parser_event {
    uint8_t type;
} sdp_parser_event_t;

// SDP Parser event to deliver an attribute value byte by byte
typedef struct sdp_parser_attribute_value_event {
    uint8_t type;
    int record_id;
    uint16_t attribute_id;
    uint32_t attribute_length;
    int data_offset;
    uint8_t data;
} sdp_parser_attribute_value_event_t;

// SDP Parser event to indicate that parsing is complete.
typedef struct sdp_parser_complete_event {
    uint8_t type;
    uint8_t status; // 0 == OK
} sdp_parser_complete_event_t;


void sdp_parser_init(void);
void sdp_parser_handle_chunk(uint8_t * data, uint16_t size);
void sdp_parser_handle_done(uint8_t status);

// Registers a callback to receive attribute value data and parse complete event.
void sdp_parser_register_callback(void (*sdp_callback)(sdp_parser_event_t* event));


#if defined __cplusplus
}
#endif

