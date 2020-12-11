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

#define BTSTACK_FILE__ "sdp_client_rfcomm.c"

/*
 *  sdp_rfcomm_query.c
 */

#include <stdint.h>
#include <string.h>

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/core.h"
#include "classic/sdp_client.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_util.h"
#include "hci_cmd.h"

#if (SDP_SERVICE_NAME_LEN < 16)
#error "SDP_SERVICE_NAME_LEN must be at least 16 bytes"
#endif

// called by test/sdp_client
void sdp_client_query_rfcomm_init(void);

static enum {
    GET_PROTOCOL_LIST_LENGTH = 1,
    GET_PROTOCOL_LENGTH,
    GET_PROTOCOL_ID_HEADER_LENGTH,
    GET_PROTOCOL_ID,
    GET_PROTOCOL_VALUE_LENGTH,
    GET_PROTOCOL_VALUE
} protokoll_descriptor_list_state;

static enum {
    GET_SERVICE_LIST_LENGTH = 1,
    GET_SERVICE_LIST_ITEM_GET_UUID_TYPE,
    GET_SERVICE_LIST_ITEM,
    GET_SERVICE_LIST_ITEM_SHORT,
    GET_SERVICE_LIST_ITEM_LONG,
    GET_SERVICE_INVALID,
} service_class_id_list_state;

// higher layer query - get rfcomm channel and name

// All attributes: 0x0001 - 0x0100
static const uint8_t des_attributeIDList[]    = { 0x35, 0x05, 0x0A, 0x00, 0x01, 0x01, 0x00};  

static uint8_t  sdp_service_name[SDP_SERVICE_NAME_LEN+1];
static uint8_t  sdp_service_name_len = 0;
static uint8_t  sdp_rfcomm_channel_nr = 0;

static uint8_t  sdp_service_name_header_size;

static bool     sdp_client_rfcomm_serviceclass_matched;
static bool     sdp_client_rfcomm_match_serviceclass;
static uint16_t sdp_client_rfcomm_uuid16;

static int      protocol_value_bytes_received = 0;
static int      protocol_value_size;
static int      protocol_offset;
static int      protocol_size;
static int      protocol_id_bytes_to_read;
static uint32_t protocol_id = 0;

static de_state_t de_header_state;
static btstack_packet_handler_t sdp_app_callback;
//

static void sdp_rfcomm_query_prepare(void){
    sdp_rfcomm_channel_nr = 0;
    sdp_service_name[0] = 0;
    sdp_client_rfcomm_serviceclass_matched = false;
}

static void sdp_rfcomm_query_emit_service(void){
    uint8_t event[3+SDP_SERVICE_NAME_LEN+1];
    event[0] = SDP_EVENT_QUERY_RFCOMM_SERVICE;
    event[1] = sdp_service_name_len + 1;
    event[2] = sdp_rfcomm_channel_nr;
    (void)memcpy(&event[3], sdp_service_name, sdp_service_name_len);
    event[3+sdp_service_name_len] = 0;
    (*sdp_app_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void sdp_client_query_rfcomm_handle_record_parsed(void){
    if (sdp_rfcomm_channel_nr == 0) return;
    if (sdp_client_rfcomm_match_serviceclass && (sdp_client_rfcomm_serviceclass_matched == false)) return;
    sdp_rfcomm_query_emit_service();
    sdp_rfcomm_query_prepare();
}

// Format: DE Sequence of UUIDs
static void sdp_client_query_rfcomm_handle_service_class_list_data(uint32_t attribute_value_length, uint32_t data_offset, uint8_t data){
    UNUSED(attribute_value_length);

    // init state on first byte
    if (data_offset == 0){
        service_class_id_list_state = GET_SERVICE_LIST_LENGTH;
        de_state_init(&de_header_state);
    }

    // process data
    switch(service_class_id_list_state){

        case GET_SERVICE_LIST_LENGTH:
            // read DES sequence header
            if (!de_state_size(data, &de_header_state)) break;
            service_class_id_list_state = GET_SERVICE_LIST_ITEM_GET_UUID_TYPE;
            break;

        case GET_SERVICE_LIST_ITEM_GET_UUID_TYPE:
            protocol_id = 0;
            protocol_offset = 0;
            // validate UUID type
            if (de_get_element_type(&data) != DE_UUID) {
                service_class_id_list_state = GET_SERVICE_INVALID;
                break;
            }
            // get UUID length
            protocol_id_bytes_to_read = de_get_data_size(&data);
            if (protocol_id_bytes_to_read > 16) {
                service_class_id_list_state = GET_SERVICE_INVALID;
                break;
            }
            service_class_id_list_state = GET_SERVICE_LIST_ITEM;
            break;

        case GET_SERVICE_LIST_ITEM:
            sdp_service_name[protocol_offset++] = data;
            protocol_id_bytes_to_read--;
            if (protocol_id_bytes_to_read > 0) break;
            // parse 2/4/16 bytes UUID
            switch (protocol_offset){
                case 2:
                    protocol_id = big_endian_read_16(sdp_service_name, 0);
                    break;
                case 4:
                    protocol_id = big_endian_read_32(sdp_service_name, 0);
                    break;
                case 16:
                    if (uuid_has_bluetooth_prefix(sdp_service_name)){
                        protocol_id = big_endian_read_32(sdp_service_name, 0);
                    }
                    break;
                default:
                    break;
            }
            if (protocol_id == sdp_client_rfcomm_uuid16){
                sdp_client_rfcomm_serviceclass_matched = true;
            }
            service_class_id_list_state = GET_SERVICE_LIST_ITEM_GET_UUID_TYPE;
            break;

        default:
            break;
    }
}

static void sdp_client_query_rfcomm_handle_protocol_descriptor_list_data(uint32_t attribute_value_length, uint32_t data_offset, uint8_t data){
    UNUSED(attribute_value_length);
    
    // init state on first byte
    if (data_offset == 0){
        de_state_init(&de_header_state);
        protokoll_descriptor_list_state = GET_PROTOCOL_LIST_LENGTH;
    }

    switch(protokoll_descriptor_list_state){
        
        case GET_PROTOCOL_LIST_LENGTH:
            if (!de_state_size(data, &de_header_state)) break;

            protokoll_descriptor_list_state = GET_PROTOCOL_LENGTH;
            break;
        
        case GET_PROTOCOL_LENGTH:
            // check size
            if (!de_state_size(data, &de_header_state)) break;
            
            // cache protocol info
            protocol_offset = de_header_state.de_offset;
            protocol_size   = de_header_state.de_size;

            protokoll_descriptor_list_state = GET_PROTOCOL_ID_HEADER_LENGTH;
            break;
        
       case GET_PROTOCOL_ID_HEADER_LENGTH:
            protocol_offset++;
            if (!de_state_size(data, &de_header_state)) break;
            
            protocol_id = 0;
            protocol_id_bytes_to_read = de_header_state.de_size;
            protokoll_descriptor_list_state = GET_PROTOCOL_ID;
            
            break;
        
        case GET_PROTOCOL_ID:
            protocol_offset++;

            protocol_id = (protocol_id << 8) | data;
            protocol_id_bytes_to_read--;
            if (protocol_id_bytes_to_read > 0) break;


            if (protocol_offset >= protocol_size){
                protokoll_descriptor_list_state = GET_PROTOCOL_LENGTH;
                break;
            }

            protokoll_descriptor_list_state = GET_PROTOCOL_VALUE_LENGTH;
            protocol_value_bytes_received = 0;
            break;
        
        case GET_PROTOCOL_VALUE_LENGTH:
            protocol_offset++;

            if (!de_state_size(data, &de_header_state)) break;

            protocol_value_size = de_header_state.de_size;
            protokoll_descriptor_list_state = GET_PROTOCOL_VALUE;
            sdp_rfcomm_channel_nr = 0;
            break;
        
        case GET_PROTOCOL_VALUE:
            protocol_offset++;
            protocol_value_bytes_received++;

            if (protocol_value_bytes_received < protocol_value_size) break;

            if (protocol_id == BLUETOOTH_PROTOCOL_RFCOMM){
                //  log_info("\n\n *******  Data ***** %02x\n\n", data);
                sdp_rfcomm_channel_nr = data;
            }

            if (protocol_offset >= protocol_size) {
                protokoll_descriptor_list_state = GET_PROTOCOL_LENGTH;
                break;

            }
            protokoll_descriptor_list_state = GET_PROTOCOL_ID_HEADER_LENGTH;
            break;
        default:
            break;
    }
}

static void sdp_client_query_rfcomm_handle_service_name_data(uint32_t attribute_value_length, uint32_t data_offset, uint8_t data){

    // Get Header Len
    if (data_offset == 0){
        de_state_init(&de_header_state);
        de_state_size(data, &de_header_state);
        sdp_service_name_header_size = de_header_state.addon_header_bytes + 1;
        return;
    }

    // Get Header
    if (data_offset < sdp_service_name_header_size){
        de_state_size(data, &de_header_state);
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
            sdp_service_name_len = name_pos;            
        } 

        // terminate if buffer full
        if (name_pos == SDP_SERVICE_NAME_LEN){
            sdp_service_name[name_pos] = 0;            
            sdp_service_name_len = name_pos;            
        }
    }

    // notify on last char
    if ((data_offset == (attribute_value_length - 1)) && (sdp_rfcomm_channel_nr!=0)){
        sdp_client_query_rfcomm_handle_record_parsed();
    }
}

static void sdp_client_query_rfcomm_handle_sdp_parser_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    UNUSED(channel);

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_SERVICE_RECORD_HANDLE:
            sdp_client_query_rfcomm_handle_record_parsed();

            // prepare for new record
            sdp_rfcomm_query_prepare();
            break;
        case SDP_EVENT_QUERY_ATTRIBUTE_VALUE:
            switch (sdp_event_query_attribute_byte_get_attribute_id(packet)){
                case BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST:
                    if (sdp_client_rfcomm_match_serviceclass){
                        sdp_client_query_rfcomm_handle_service_class_list_data(sdp_event_query_attribute_byte_get_attribute_length(packet),
                                                                               sdp_event_query_attribute_byte_get_data_offset(packet),
                                                                               sdp_event_query_attribute_byte_get_data(packet));
                    }
                    break;
                case BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST:
                    // find rfcomm channel
                    sdp_client_query_rfcomm_handle_protocol_descriptor_list_data(sdp_event_query_attribute_byte_get_attribute_length(packet),
                        sdp_event_query_attribute_byte_get_data_offset(packet),
                        sdp_event_query_attribute_byte_get_data(packet));
                    break;
                case 0x0100:
                    // get service name
                    sdp_client_query_rfcomm_handle_service_name_data(sdp_event_query_attribute_byte_get_attribute_length(packet),
                        sdp_event_query_attribute_byte_get_data_offset(packet),
                        sdp_event_query_attribute_byte_get_data(packet));
                    break;
                default:
                    // give up
                    return;
            }
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            sdp_client_query_rfcomm_handle_record_parsed();
            (*sdp_app_callback)(HCI_EVENT_PACKET, 0, packet, size);
            break;
        default:
            break;
    }
}

void sdp_client_query_rfcomm_init(void){
    // init
    protokoll_descriptor_list_state = GET_PROTOCOL_LIST_LENGTH;
    protocol_offset = 0;
    sdp_rfcomm_channel_nr = 0;
    sdp_service_name[0] = 0;
}

static uint8_t sdp_client_query_rfcomm(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t * service_search_pattern){
    sdp_app_callback = callback;
    sdp_client_query_rfcomm_init();
    return sdp_client_query(&sdp_client_query_rfcomm_handle_sdp_parser_event, remote, service_search_pattern, (uint8_t*)&des_attributeIDList[0]);
}

// Public API

uint8_t sdp_client_query_rfcomm_channel_and_name_for_uuid(btstack_packet_handler_t callback, bd_addr_t remote, uint16_t uuid16){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;
    sdp_client_rfcomm_match_serviceclass = false;
    return sdp_client_query_rfcomm(callback, remote, sdp_service_search_pattern_for_uuid16(uuid16));
}

uint8_t sdp_client_query_rfcomm_channel_and_name_for_service_class_uuid(btstack_packet_handler_t callback, bd_addr_t remote, uint16_t uuid16){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;
    sdp_client_rfcomm_match_serviceclass = true;
    sdp_client_rfcomm_uuid16 = uuid16;
    return sdp_client_query_rfcomm(callback, remote, sdp_service_search_pattern_for_uuid16(uuid16));
}

uint8_t sdp_client_query_rfcomm_channel_and_name_for_uuid128(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t * uuid128){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;
    sdp_client_rfcomm_match_serviceclass = false;
    return sdp_client_query_rfcomm(callback, remote, sdp_service_search_pattern_for_uuid128(uuid128));
}

uint8_t sdp_client_query_rfcomm_channel_and_name_for_search_pattern(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t * service_search_pattern){
    if (!sdp_client_ready()) return SDP_QUERY_BUSY;
    sdp_client_rfcomm_match_serviceclass = false;
    return sdp_client_query_rfcomm(callback, remote, service_search_pattern);
}
