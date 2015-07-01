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

/*
 *  sdp_parser.h
 */

#ifndef __SDP_PARSER_H
#define __SDP_PARSER_H

#include "btstack-config.h"
 
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/sdp_util.h>
#include <btstack/utils.h>

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

/* API_START */

// SDP Parser
// Basic SDP Query event type
typedef struct sdp_query_event {
    uint8_t type;
} sdp_query_event_t;

// SDP Query event to indicate that query/parser is complete.
typedef struct sdp_query_complete_event {
    uint8_t type;
    uint8_t status; // 0 == OK
} sdp_query_complete_event_t;

// SDP Parser event to deliver an attribute value byte by byte
typedef struct sdp_query_attribute_value_event {
    uint8_t type;
    int record_id;
    uint16_t attribute_id;
    uint32_t attribute_length;
    int data_offset;
    uint8_t data;
} sdp_query_attribute_value_event_t;


#ifdef HAVE_SDP_EXTRA_QUERIES
typedef struct sdp_query_service_record_handle_event {
    uint8_t type;
    uint16_t total_count;
    uint16_t current_count;
    uint32_t record_handle;
} sdp_query_service_record_handle_event_t;
#endif

/*
 * @brief
 */
void sdp_parser_init(void);

/*
 * @brief
 */
void sdp_parser_handle_chunk(uint8_t * data, uint16_t size);

#ifdef HAVE_SDP_EXTRA_QUERIES

/*
 * @brief
 */
void sdp_parser_init_service_attribute_search(void);

/*
 * @brief
 */
void sdp_parser_init_service_search(void);

/*
 * @brief
 */
void sdp_parser_handle_service_search(uint8_t * data, uint16_t total_count, uint16_t record_handle_count);
#endif

void sdp_parser_handle_done(uint8_t status);

/*
 * @brief Registers a callback to receive attribute value data and parse complete event.
 */
void sdp_parser_register_callback(void (*sdp_callback)(sdp_query_event_t * event));

/* API_END */

#if defined __cplusplus
}
#endif

#endif // __SDP_PARSER_H
