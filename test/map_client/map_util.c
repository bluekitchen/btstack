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


#define __BTSTACK_FILE__ "map_util.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_util.h"
#include "map_util.h"
#include "yxml.h"
#include "map.h"
#include "btstack_defines.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "btstack_event.h"

static yxml_t  xml_parser;
static uint8_t xml_buffer[50];

void map_message_str_to_handle(const char * value, map_message_handle_t msg_handle){
    int i;
    for (i = 0; i < MAP_MESSAGE_HANDLE_SIZE; i++) {
        uint8_t upper_nibble = nibble_for_char(*value++);
        uint8_t lower_nibble = nibble_for_char(*value++);
        msg_handle[i] = (upper_nibble << 4) | lower_nibble;
    }
}

void map_message_handle_to_str(char * p, const map_message_handle_t msg_handle){
    int i;
    for (i = 0; i < MAP_MESSAGE_HANDLE_SIZE ; i++) {
        uint8_t byte = msg_handle[i];
        *p++ = char_for_nibble(byte >> 4);
        *p++ = char_for_nibble(byte & 0x0F);
    }
    *p = 0;
}

static void map_client_emit_folder_listing_item_event(btstack_packet_handler_t callback, uint16_t cid, uint8_t * folder_name, uint16_t folder_name_len){
    uint8_t event[7 + MAP_MAX_VALUE_LEN];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_FOLDER_LISTING_ITEM;
    little_endian_store_16(event,pos,cid);
    pos+=2;
    uint16_t value_len = btstack_min(folder_name_len, sizeof(event) - pos);
    little_endian_store_16(event, pos, value_len);
    pos += 2;
    memcpy(event+pos, folder_name, value_len);
    pos += value_len;
    event[1] = pos - 2;
    if (pos > sizeof(event)) log_error("map_client_emit_folder_listing_item_event size %u", pos);
    (*callback)(HCI_EVENT_PACKET, cid, &event[0], pos);
}  

static void map_client_emit_message_listing_item_event(btstack_packet_handler_t callback, uint16_t cid, map_message_handle_t message_handle){
    uint8_t event[7 + MAP_MESSAGE_HANDLE_SIZE];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_MESSAGE_LISTING_ITEM;
    little_endian_store_16(event,pos,cid);
    pos+=2;
    
    memcpy(event+pos, message_handle, MAP_MESSAGE_HANDLE_SIZE);
    pos += MAP_MESSAGE_HANDLE_SIZE;
    
    event[1] = pos - 2;
    if (pos > sizeof(event)) log_error("map_client_emit_message_listing_item_event size %u", pos);
    (*callback)(HCI_EVENT_PACKET, cid, &event[0], pos);
} 

static void map_client_emit_parsing_done_event(btstack_packet_handler_t callback, uint16_t cid){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_PARSING_DONE;
    little_endian_store_16(event,pos,cid);
    pos += 2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("map_client_emit_parsing_done_event size %u", pos);
    (*callback)(HCI_EVENT_PACKET, cid, &event[0], pos);
}

void map_client_parse_folder_listing(btstack_packet_handler_t callback, uint16_t cid, const uint8_t * data, uint16_t data_len){
    int folder_found = 0;
    int name_found = 0;
    char name[MAP_MAX_VALUE_LEN];
    yxml_init(&xml_parser, xml_buffer, sizeof(xml_buffer));
    
    while (data_len > 0){
        data_len -= 1;
        yxml_ret_t r = yxml_parse(&xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                folder_found = strcmp("folder", xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (folder_found){
                    map_client_emit_folder_listing_item_event(callback, cid, (uint8_t *) name, strlen(name));
                }
                folder_found = 0;
                break;
            case YXML_ATTRSTART:
                if (folder_found == 0) break;
                if (strcmp("name", xml_parser.attr) == 0){
                    name_found = 1;
                    name[0]    = 0;
                    break;
                }
                break;
            case YXML_ATTRVAL:
                if (name_found == 1) {
                    // "In UTF-8, characters from the U+0000..U+10FFFF range (the UTF-16 accessible range) are encoded using sequences of 1 to 4 octets."
                    if (strlen(name) + 4 + 1 >= sizeof(name)) break;
                    strcat(name, xml_parser.data);
                    break;
                }
                break;
            case YXML_ATTREND:
                name_found = 0;
                break;
            default:
                break;
        }
    }
    map_client_emit_parsing_done_event(callback, cid);
}

void map_client_parse_message_listing(btstack_packet_handler_t callback, uint16_t cid, const uint8_t * data, uint16_t data_len){
    // now try parsing it
    btstack_assert(data_len < 65535);

    int message_found = 0;
    int handle_found = 0;
    char handle[MAP_MESSAGE_HANDLE_SIZE * 2 + 1];
    map_message_handle_t msg_handle;
    yxml_init(&xml_parser, xml_buffer, sizeof(xml_buffer));

    while (data_len > 0){
        data_len -= 1;
        yxml_ret_t r = yxml_parse(&xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                message_found = strcmp("msg", xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (message_found == 0) break;
                message_found = 0;
                if (strlen(handle) != MAP_MESSAGE_HANDLE_SIZE * 2){
                    log_info("message handle string length %u != %u", (unsigned int) strlen(handle), MAP_MESSAGE_HANDLE_SIZE*2);
                    break;
                }
                map_message_str_to_handle(handle, msg_handle);
                map_client_emit_message_listing_item_event(callback, cid, msg_handle);
                break;
            case YXML_ATTRSTART:
                if (message_found == 0) break;
                if (strcmp("handle", xml_parser.attr) == 0){
                    handle_found = 1;
                    handle[0]    = 0;
                    break;
                }
                break;
            case YXML_ATTRVAL:
                if (handle_found == 1) {
                    if (strlen(xml_parser.data) != 1) break;
                    if (strlen(handle) >= (MAP_MESSAGE_HANDLE_SIZE * 2)) break;
                    strcat(handle, xml_parser.data);
                    break;
                }
                break;
            case YXML_ATTREND:
                handle_found = 0;
                break;
            default:
                break;
        }
    }
    map_client_emit_parsing_done_event(callback, cid);
}