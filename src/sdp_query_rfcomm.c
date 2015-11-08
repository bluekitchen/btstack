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
 *  sdp_rfcomm_query.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/sdp_util.h>

#include "sdp_client.h"
#include "sdp_query_rfcomm.h"

// called by test/sdp_client
void sdp_query_rfcomm_init(void);

static void dummy_notify_app(sdp_query_event_t* event, void * context);

typedef enum { 
    GET_PROTOCOL_LIST_LENGTH = 1,
    GET_PROTOCOL_LENGTH,
    GET_PROTOCOL_ID_HEADER_LENGTH,
    GET_PROTOCOL_ID,
    GET_PROTOCOL_VALUE_LENGTH,
    GET_PROTOCOL_VALUE
} pdl_state_t;


// higher layer query - get rfcomm channel and name

const uint8_t des_attributeIDList[]    = { 0x35, 0x05, 0x0A, 0x00, 0x01, 0x01, 0x00};  // Arribute: 0x0001 - 0x0100

static uint8_t des_serviceSearchPattern[5] = {0x35, 0x03, 0x19, 0x00, 0x00};

static uint8_t sdp_service_name[SDP_SERVICE_NAME_LEN+1];
static uint8_t sdp_rfcomm_channel_nr = 0;
static uint8_t sdp_service_name_header_size;

static pdl_state_t pdl_state = GET_PROTOCOL_LIST_LENGTH;
static int protocol_value_bytes_received = 0;
static uint16_t protocol_id = 0;
static int protocol_offset;
static int protocol_size;
static int protocol_id_bytes_to_read;
static int protocol_value_size;
static de_state_t de_header_state;
static de_state_t sn_de_header_state;

static void *sdp_app_context;
static void (*sdp_app_callback)(sdp_query_event_t * event, void * context) = dummy_notify_app;

//

static void dummy_notify_app(sdp_query_event_t* event, void * context){}

static void emit_service(void){
    sdp_query_rfcomm_service_event_t value_event = {
        SDP_QUERY_RFCOMM_SERVICE, 
        sdp_rfcomm_channel_nr,
        (uint8_t *) sdp_service_name
    };
    (*sdp_app_callback)((sdp_query_event_t*)&value_event, sdp_app_context);
    sdp_rfcomm_channel_nr = 0;
}

void sdp_query_rfcomm_register_callback(void (*sdp_callback)(sdp_query_event_t* event, void * context), void * context){
    sdp_app_callback = dummy_notify_app;
    if (sdp_callback != NULL){
        sdp_app_callback = sdp_callback;
    } 
    sdp_app_context = context;
}

static void handleProtocolDescriptorListData(uint32_t attribute_value_length, uint32_t data_offset, uint8_t data){
    // init state on first byte
    if (data_offset == 0){
        pdl_state = GET_PROTOCOL_LIST_LENGTH;
    }

    // log_info("handleProtocolDescriptorListData (%u,%u) %02x", attribute_value_length, data_offset, data);

    switch(pdl_state){
        
        case GET_PROTOCOL_LIST_LENGTH:
            if (!de_state_size(data, &de_header_state)) break;
            // log_info("   query: PD List payload is %d bytes.", de_header_state.de_size);
            // log_info("   query: PD List offset %u, list size %u", de_header_state.de_offset, de_header_state.de_size);

            pdl_state = GET_PROTOCOL_LENGTH;
            break;
        
        case GET_PROTOCOL_LENGTH:
            // check size
            if (!de_state_size(data, &de_header_state)) break;
            // log_info("   query: PD Record payload is %d bytes.", de_header_state.de_size);
            
            // cache protocol info
            protocol_offset = de_header_state.de_offset;
            protocol_size   = de_header_state.de_size;

            pdl_state = GET_PROTOCOL_ID_HEADER_LENGTH;
            break;
        
       case GET_PROTOCOL_ID_HEADER_LENGTH:
            protocol_offset++;
            if (!de_state_size(data, &de_header_state)) break;
            
            protocol_id = 0;
            protocol_id_bytes_to_read = de_header_state.de_size;
            // log_info("   query: ID data is stored in %d bytes.", protocol_id_bytes_to_read);
            pdl_state = GET_PROTOCOL_ID;
            
            break;
        
        case GET_PROTOCOL_ID:
            protocol_offset++;

            protocol_id = (protocol_id << 8) | data;
            protocol_id_bytes_to_read--;
            if (protocol_id_bytes_to_read > 0) break;

            // log_info("   query: Protocol ID: %04x.", protocol_id);

            if (protocol_offset >= protocol_size){
                pdl_state = GET_PROTOCOL_LENGTH;
                // log_info("   query: Get next protocol");
                break;
            } 
            
            pdl_state = GET_PROTOCOL_VALUE_LENGTH;
            protocol_value_bytes_received = 0;
            break;
        
        case GET_PROTOCOL_VALUE_LENGTH:
            protocol_offset++;

            if (!de_state_size(data, &de_header_state)) break;

            protocol_value_size = de_header_state.de_size;
            pdl_state = GET_PROTOCOL_VALUE;
            sdp_rfcomm_channel_nr = 0;
            break;
        
        case GET_PROTOCOL_VALUE:
            protocol_offset++;
            protocol_value_bytes_received++;
           
            // log_info("   query: protocol_value_bytes_received %u, protocol_value_size %u", protocol_value_bytes_received, protocol_value_size);

            if (protocol_value_bytes_received < protocol_value_size) break;

            if (protocol_id == 0x0003){
                //  log_info("\n\n *******  Data ***** %02x\n\n", data);
                sdp_rfcomm_channel_nr = data;
            }

            // log_info("   query: protocol done");
            // log_info("   query: Protocol offset %u, protocol size %u", protocol_offset, protocol_size);

            if (protocol_offset >= protocol_size) {
                pdl_state = GET_PROTOCOL_LENGTH;
                break;

            }
            pdl_state = GET_PROTOCOL_ID_HEADER_LENGTH;
            // log_info("   query: Get next protocol");
            break;
        default:
            break;
    }
}

static void handleServiceNameData(uint32_t attribute_value_length, uint32_t data_offset, uint8_t data){

    // Get Header Len
    if (data_offset == 0){
        de_state_size(data, &sn_de_header_state);
        sdp_service_name_header_size = sn_de_header_state.addon_header_bytes + 1;
        return;
    }

    // Get Header
    if (data_offset < sdp_service_name_header_size){
        de_state_size(data, &sn_de_header_state);
        return;
    }

    // Process payload
    int name_len = attribute_value_length - sdp_service_name_header_size;
    int name_pos = data_offset - sdp_service_name_header_size;

    if (name_pos < SDP_SERVICE_NAME_LEN){
        sdp_service_name[name_pos] = data;
        name_pos++;

        // terminate if name complete
        if (name_pos >= name_len){
            sdp_service_name[name_pos] = 0;            
        } 

        // terminate if buffer full
        if (name_pos == SDP_SERVICE_NAME_LEN){
            sdp_service_name[name_pos] = 0;            
        }
    }

    // notify on last char
    if (data_offset == attribute_value_length - 1 && sdp_rfcomm_channel_nr!=0){
        emit_service();
    }
}

static void handle_sdp_parser_event(sdp_query_event_t * event){
    sdp_query_attribute_value_event_t * ve;
    switch (event->type){
        case SDP_QUERY_SERVICE_RECORD_HANDLE:
            // handle service without a name
            if (sdp_rfcomm_channel_nr){
                emit_service();
            }

            // prepare for new record
            sdp_rfcomm_channel_nr = 0;
            sdp_service_name[0] = 0;
            break;
        case SDP_QUERY_ATTRIBUTE_VALUE:
            ve = (sdp_query_attribute_value_event_t*) event;
           // log_info("handle_sdp_parser_event [ AID, ALen, DOff, Data] : [%x, %u, %u] BYTE %02x", 
           //          ve->attribute_id, ve->attribute_length, ve->data_offset, ve->data);
            
            switch (ve->attribute_id){
                case SDP_ProtocolDescriptorList:
                    // find rfcomm channel
                    handleProtocolDescriptorListData(ve->attribute_length, ve->data_offset, ve->data);
                    break;
                case 0x0100:
                    // get service name
                    handleServiceNameData(ve->attribute_length, ve->data_offset, ve->data);
                    break;
                default:
                    // give up
                    return;
            }
            break;
        case SDP_QUERY_COMPLETE:
            // handle service without a name
            if (sdp_rfcomm_channel_nr){
                emit_service();
            }
            (*sdp_app_callback)(event, sdp_app_context);
            break;
    }
    // insert higher level code HERE
}

void sdp_query_rfcomm_init(void){
    // init
    de_state_init(&de_header_state);
    de_state_init(&sn_de_header_state);
    pdl_state = GET_PROTOCOL_LIST_LENGTH;
    protocol_offset = 0;
    sdp_parser_register_callback(handle_sdp_parser_event);
    sdp_rfcomm_channel_nr = 0;
    sdp_service_name[0] = 0;
}


void sdp_query_rfcomm_channel_and_name_for_search_pattern(bd_addr_t remote, uint8_t * serviceSearchPattern){
    sdp_parser_init();
    sdp_query_rfcomm_init();
    sdp_client_query(remote, serviceSearchPattern, (uint8_t*)&des_attributeIDList[0]);
}

void sdp_query_rfcomm_channel_and_name_for_uuid(bd_addr_t remote, uint16_t uuid){
    net_store_16(des_serviceSearchPattern, 3, uuid);
    sdp_query_rfcomm_channel_and_name_for_search_pattern(remote, (uint8_t*)des_serviceSearchPattern);
}

void sdp_query_rfcomm_deregister_callback(void){
    sdp_query_rfcomm_init();
    sdp_app_callback = dummy_notify_app; 
    sdp_app_context = NULL;
}
