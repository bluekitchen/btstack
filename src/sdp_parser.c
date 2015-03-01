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
 *  sdp_parser.c
 */
#include <btstack/hci_cmds.h>
#include "sdp_parser.h"
#include "debug.h"

typedef enum { 
    GET_LIST_LENGTH = 1,
    GET_RECORD_LENGTH,
    GET_ATTRIBUTE_ID_HEADER_LENGTH,
    GET_ATTRIBUTE_ID,
    GET_ATTRIBUTE_VALUE_LENGTH,
    GET_ATTRIBUTE_VALUE
} state_t;

static state_t state = GET_LIST_LENGTH;
static uint16_t attribute_id = 0;
static uint16_t attribute_bytes_received = 0;
static uint16_t attribute_bytes_delivered = 0;
static uint16_t list_offset = 0;
static uint16_t list_size;
static uint16_t record_offset = 0;
static uint16_t record_size;
static uint16_t attribute_value_size;
static int record_counter = 0;

#ifdef HAVE_SDP_EXTRA_QUERIES
static uint32_t record_handle;
#endif

static void (*sdp_query_callback)(sdp_query_event_t * event);

// Low level parser
static de_state_t de_header_state;


void de_state_init(de_state_t * state){
    state->in_state_GET_DE_HEADER_LENGTH = 1;
    state->addon_header_bytes = 0;
    state->de_size = 0;
    state->de_offset = 0;
}

int de_state_size(uint8_t eventByte, de_state_t *de_state){
    if (de_state->in_state_GET_DE_HEADER_LENGTH){
        de_state->addon_header_bytes = de_get_header_size(&eventByte) - 1;
        de_state->de_size = 0;
        de_state->de_offset = 0;

        if (de_state->addon_header_bytes == 0){
            de_state->de_size = de_get_data_size(&eventByte);
            if (de_state->de_size == 0) {
                log_error("  ERROR: ID size is zero");
            }
            // log_info("Data element payload is %d bytes.", de_state->de_size);
            return 1;
        }
        de_state->in_state_GET_DE_HEADER_LENGTH = 0;
        return 0;
    }
   
    if (de_state->addon_header_bytes > 0){
        de_state->de_size = (de_state->de_size << 8) | eventByte;
        de_state->addon_header_bytes--;
    } 
    if (de_state->addon_header_bytes > 0) return 0;
    // log_info("Data element payload is %d bytes.", de_state->de_size);
    de_state->in_state_GET_DE_HEADER_LENGTH = 1;
    return 1;
}

void dummy_notify(sdp_query_event_t* event){}

void sdp_parser_register_callback(void (*sdp_callback)(sdp_query_event_t* event)){
    sdp_query_callback = dummy_notify;
    if (sdp_callback != NULL){
        sdp_query_callback = sdp_callback;
    } 
}

void parse(uint8_t eventByte){
    // count all bytes
    list_offset++;
    record_offset++;

    // log_info(" parse BYTE_RECEIVED %02x", eventByte);
    switch(state){
        case GET_LIST_LENGTH:
            if (!de_state_size(eventByte, &de_header_state)) break;
            list_offset = de_header_state.de_offset;
            list_size = de_header_state.de_size;
            // log_info("parser: List offset %u, list size %u", list_offset, list_size);
            
            record_counter = 0;
            state = GET_RECORD_LENGTH;
            break;

        case GET_RECORD_LENGTH:
            // check size
            if (!de_state_size(eventByte, &de_header_state)) break;
            // log_info("parser: Record payload is %d bytes.", de_header_state.de_size);
            record_offset = de_header_state.de_offset;
            record_size = de_header_state.de_size;
            state = GET_ATTRIBUTE_ID_HEADER_LENGTH;
            break;

        case GET_ATTRIBUTE_ID_HEADER_LENGTH:
            if (!de_state_size(eventByte, &de_header_state)) break;
            attribute_id = 0;
            log_info("ID data is stored in %d bytes.", de_header_state.de_size);
            state = GET_ATTRIBUTE_ID;
            break;
        
        case GET_ATTRIBUTE_ID:
            attribute_id = (attribute_id << 8) | eventByte;
            de_header_state.de_size--;
            if (de_header_state.de_size > 0) break;
            log_info("parser: Attribute ID: %04x.", attribute_id);

            state = GET_ATTRIBUTE_VALUE_LENGTH;
            attribute_bytes_received  = 0;
            attribute_bytes_delivered = 0;
            attribute_value_size      = 0;
            de_state_init(&de_header_state);
            break;
        
        case GET_ATTRIBUTE_VALUE_LENGTH:
            attribute_bytes_received++;
            {
            sdp_query_attribute_value_event_t attribute_value_event = {
                SDP_QUERY_ATTRIBUTE_VALUE, 
                record_counter, 
                attribute_id, 
                attribute_value_size,
                attribute_bytes_delivered++,
                eventByte
            };
            (*sdp_query_callback)((sdp_query_event_t*)&attribute_value_event);
            }
           if (!de_state_size(eventByte, &de_header_state)) break;

            attribute_value_size = de_header_state.de_size + attribute_bytes_received;

            state = GET_ATTRIBUTE_VALUE;
            break;
        
        case GET_ATTRIBUTE_VALUE: 
            attribute_bytes_received++;
            {
            sdp_query_attribute_value_event_t attribute_value_event = {
                SDP_QUERY_ATTRIBUTE_VALUE, 
                record_counter, 
                attribute_id, 
                attribute_value_size,
                attribute_bytes_delivered++,
                eventByte
            };

            (*sdp_query_callback)((sdp_query_event_t*)&attribute_value_event);
            }
            // log_info("paser: attribute_bytes_received %u, attribute_value_size %u", attribute_bytes_received, attribute_value_size);

            if (attribute_bytes_received < attribute_value_size) break;
            // log_info("parser: Record offset %u, record size %u", record_offset, record_size);
            if (record_offset != record_size){
                state = GET_ATTRIBUTE_ID_HEADER_LENGTH;
                // log_info("Get next attribute");
                break;
            } 
            record_offset = 0;
            // log_info("parser: List offset %u, list size %u", list_offset, list_size);
            
            if (list_size > 0 && list_offset != list_size){
                record_counter++;
                state = GET_RECORD_LENGTH;
                log_info("parser: END_OF_RECORD");
                break;
            }
            list_offset = 0;
            de_state_init(&de_header_state);
            state = GET_LIST_LENGTH;
            record_counter = 0;
            log_info("parser: END_OF_RECORD & DONE");
            break;
        default:
            break;
    }
}

void sdp_parser_init(void){
    // init
    de_state_init(&de_header_state);
    state = GET_LIST_LENGTH;
    list_offset = 0;
    record_offset = 0;
    record_counter = 0;
}

void sdp_parser_handle_chunk(uint8_t * data, uint16_t size){
    int i;
    for (i=0;i<size;i++){
        parse(data[i]);
    }
}

#ifdef HAVE_SDP_EXTRA_QUERIES
void sdp_parser_init_service_attribute_search(void){
    // init
    de_state_init(&de_header_state);
    state = GET_RECORD_LENGTH;
    list_offset = 0;
    record_offset = 0;
    record_counter = 0;
}

void sdp_parser_init_service_search(void){
    record_offset = 0;
}

void sdp_parser_handle_service_search(uint8_t * data, uint16_t total_count, uint16_t record_handle_count){
    int i;
    for (i=0;i<record_handle_count;i++){
        record_handle = READ_NET_32(data, i*4);
        record_counter++;
        sdp_query_service_record_handle_event_t service_record_handle_event = {
            SDP_QUERY_SERVICE_RECORD_HANDLE, 
            total_count, 
            record_counter, 
            record_handle
        };
        (*sdp_query_callback)((sdp_query_event_t*)&service_record_handle_event);       
    }        
}
#endif

void sdp_parser_handle_done(uint8_t status){
    sdp_query_complete_event_t complete_event = {
        SDP_QUERY_COMPLETE, 
        status
    };
    (*sdp_query_callback)((sdp_query_event_t*)&complete_event);
}
