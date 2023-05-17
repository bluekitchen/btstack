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


#define BTSTACK_FILE__ "map_util.c"
 
#include "btstack_config.h"

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"
#include "classic/sdp_util.h"
#include "map.h"
#include "yxml.h"
#include "map_util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void map_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t service_uuid, uint8_t instance_id,
                                  uint8_t rfcomm_channel_nr, uint16_t goep_l2cap_psm, map_message_type_t supported_message_types, uint32_t supported_features, const char * name){
    UNUSED(goep_l2cap_psm);
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        //  "UUID for Service"
        de_add_number(attribute, DE_UUID, DE_SIZE_16, service_uuid);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);

        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);  // rfcomm_service
            de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel_nr);  // rfcomm channel
        }
        de_pop_sequence(attribute, rfcomm);

        uint8_t* obexProtocol = de_push_sequence(attribute);
        {
            de_add_number(obexProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_OBEX);
        }
        de_pop_sequence(attribute, obexProtocol);

    }
    de_pop_sequence(service, attribute);


    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *profile = de_push_sequence(attribute);
        {
            de_add_number(profile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_PROFILE);
            de_add_number(profile,  DE_UINT, DE_SIZE_16, 0x0103); // Verision 1.7
        }
        de_pop_sequence(attribute, profile);
    }
    de_pop_sequence(service, attribute);

    // "Service Name"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,   DE_STRING, strlen(name), (uint8_t *) name);

#ifdef ENABLE_GOEP_L2CAP
    // 0x0200 "GoepL2CapPsm"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0200);
    de_add_number(service, DE_UINT, DE_SIZE_16, goep_l2cap_psm);
#endif

    // 0x0315 "MASInstanceID"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0315);
    de_add_number(service, DE_UINT, DE_SIZE_8, instance_id);

    // 0x0316 "SupportedMessageTypes"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0316);
    de_add_number(service, DE_UINT, DE_SIZE_8, supported_message_types);

    // 0x0317 "MapSupportedFeatures"
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0317);
    de_add_number(service, DE_UINT, DE_SIZE_32, supported_features);
}

void map_util_create_access_service_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t instance_id,
                                               uint8_t rfcomm_channel_nr, uint16_t goep_l2cap_psm, map_message_type_t supported_message_types, uint32_t supported_features, const char * name){
    map_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_MESSAGE_ACCESS_SERVER, instance_id, rfcomm_channel_nr,
                          goep_l2cap_psm, supported_message_types, supported_features, name);
}

void map_util_create_notification_service_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t instance_id,
                                                     uint8_t rfcomm_channel_nr, uint16_t goep_l2cap_psm, map_message_type_t supported_message_types, uint32_t supported_features, const char * name){
    map_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_MESSAGE_NOTIFICATION_SERVER, instance_id, rfcomm_channel_nr,
                          goep_l2cap_psm, supported_message_types, supported_features, name);
}

void map_message_str_to_handle(const char * value, map_message_handle_t msg_handle){
    uint64_t handle = 0x00;
    int nibble;
    int i;

    while (*value) {
        nibble = nibble_for_char(*value++);
        if (nibble < 0)
            return;
        handle = (handle << 4) | nibble;
    }

    for (i = 0; i < MAP_MESSAGE_HANDLE_SIZE; i++) {
        msg_handle[MAP_MESSAGE_HANDLE_SIZE-1-i] = (handle >> (i*8)) & 0xff;
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

static void map_client_emit_message_listing_item_event(btstack_packet_handler_t callback, uint16_t cid, map_message_handle_t message_handle, map_message_type_t msg_type, map_message_status_t msg_status){
    uint8_t event[7 + MAP_MESSAGE_HANDLE_SIZE];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_MAP_META;
    pos++;  // skip len
    event[pos++] = MAP_SUBEVENT_MESSAGE_LISTING_ITEM;
    little_endian_store_16(event,pos,cid);
    pos+=2;
    
    memcpy(event+pos, message_handle, MAP_MESSAGE_HANDLE_SIZE);
    pos += MAP_MESSAGE_HANDLE_SIZE;
    
    event[pos++] = msg_type;
    event[pos++] = msg_status;

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

static void map_client_parse_folder_listing(map_util_xml_parser *mu_parser, const uint8_t * data, uint16_t data_len){
    while (data_len > 0){
        data_len -= 1;
        yxml_ret_t r = yxml_parse(&mu_parser->xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                mu_parser->folder_listing.folder_found = strcmp("folder", mu_parser->xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (mu_parser->folder_listing.folder_found){
                    map_client_emit_folder_listing_item_event(mu_parser->callback, mu_parser->cid, (uint8_t *) mu_parser->folder_listing.name, strlen(mu_parser->folder_listing.name));
                }
                mu_parser->folder_listing.folder_found = 0;
                break;
            case YXML_ATTRSTART:
                if (mu_parser->folder_listing.folder_found == 0) break;
                if (strcmp("name", mu_parser->xml_parser.attr) == 0){
                    mu_parser->folder_listing.name_found = 1;
                    mu_parser->folder_listing.name[0]    = 0;
                    break;
                }
                break;
            case YXML_ATTRVAL:
                if (mu_parser->folder_listing.name_found == 1) {
                    // "In UTF-8, characters from the U+0000..U+10FFFF range (the UTF-16 accessible range) are encoded using sequences of 1 to 4 octets."
                    if (strlen(mu_parser->folder_listing.name) + 4 + 1 >= sizeof(mu_parser->folder_listing.name)) break;
                    strcat(mu_parser->folder_listing.name, mu_parser->xml_parser.data);
                    break;
                }
                break;
            case YXML_ATTREND:
                mu_parser->folder_listing.name_found = 0;
                break;
            default:
                break;
        }
    }
}

enum {
    MAP_MSG_ATTR_INVALID = 0,
    MAP_MSG_ATTR_HANDLE,
    MAP_MSG_ATTR_TYPE,
    MAP_MSG_ATTR_READ,
};

void map_client_parse_message_listing(map_util_xml_parser *mu_parser, const uint8_t * data, uint16_t data_len){
    while (data_len > 0){
        data_len -= 1;
        yxml_ret_t r = yxml_parse(&mu_parser->xml_parser, *data++);
        switch (r){
            case YXML_ELEMSTART:
                mu_parser->msg_listing.message_found = strcmp("msg", mu_parser->xml_parser.elem) == 0;
                mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_UNKNOWN;
                mu_parser->msg_listing.msg_status = MAP_MESSAGE_STATUS_UNREAD;  /* defaults to "no" = unread */
                break;
            case YXML_ELEMEND:
                if (mu_parser->msg_listing.message_found == 0) break;
                mu_parser->msg_listing.message_found = 0;
                map_client_emit_message_listing_item_event(mu_parser->callback, mu_parser->cid, mu_parser->msg_listing.msg_handle,
                                                           mu_parser->msg_listing.msg_type, mu_parser->msg_listing.msg_status);
                break;
            case YXML_ATTRSTART:
                mu_parser->msg_listing.cur_attr = MAP_MSG_ATTR_INVALID;
                if (mu_parser->msg_listing.message_found == 0) break;

                if (strcmp("handle", mu_parser->xml_parser.attr) == 0){
                    mu_parser->msg_listing.cur_attr = MAP_MSG_ATTR_HANDLE;
                } else if (strcmp("type", mu_parser->xml_parser.attr) == 0){
                    mu_parser->msg_listing.cur_attr = MAP_MSG_ATTR_TYPE;
                } else if (strcmp("read", mu_parser->xml_parser.attr) == 0){
                    mu_parser->msg_listing.cur_attr = MAP_MSG_ATTR_READ;
                }
                mu_parser->msg_listing.attr_val[0] = 0;
                break;
            case YXML_ATTRVAL:
                if (mu_parser->msg_listing.cur_attr == MAP_MSG_ATTR_INVALID) break;
                if (strlen(mu_parser->xml_parser.data) != 1) break;
                if (strlen(mu_parser->msg_listing.attr_val) >= MAP_MAX_VALUE_LEN) break;
                strcat(mu_parser->msg_listing.attr_val, mu_parser->xml_parser.data);
                break;
            case YXML_ATTREND:
                switch (mu_parser->msg_listing.cur_attr){
                    case MAP_MSG_ATTR_HANDLE:
                        if (strlen(mu_parser->msg_listing.attr_val) > MAP_MESSAGE_HANDLE_SIZE * 2){
                            log_info("message handle string length %u > %u", (unsigned int) strlen(mu_parser->msg_listing.attr_val), MAP_MESSAGE_HANDLE_SIZE*2);
                            /* discard message */
                            mu_parser->msg_listing.message_found = 0;
                            break;
                        }
                        map_message_str_to_handle(mu_parser->msg_listing.attr_val, mu_parser->msg_listing.msg_handle);
                        break;
                    case MAP_MSG_ATTR_TYPE:
                        if (strcmp (mu_parser->msg_listing.attr_val, "EMAIL") == 0) {
                            mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_EMAIL;
                        } else if (strcmp (mu_parser->msg_listing.attr_val, "SMS_GSM") == 0) {
                            mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_SMS_GSM;
                        } else if (strcmp (mu_parser->msg_listing.attr_val, "SMS_CDMA") == 0) {
                            mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_SMS_CDMA;
                        } else if (strcmp (mu_parser->msg_listing.attr_val, "MMS") == 0) {
                            mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_MMS;
                        } else if (strcmp (mu_parser->msg_listing.attr_val, "IM") == 0) {
                            mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_IM;
                        } else {
                            mu_parser->msg_listing.msg_type = MAP_MESSAGE_TYPE_UNKNOWN;
                        }
                        break;
                    case MAP_MSG_ATTR_READ:
                        if (strcmp (mu_parser->msg_listing.attr_val, "yes") == 0) {
                            mu_parser->msg_listing.msg_status = MAP_MESSAGE_STATUS_READ;
                        } else if (strcmp (mu_parser->msg_listing.attr_val, "no") == 0) {
                            mu_parser->msg_listing.msg_status = MAP_MESSAGE_STATUS_UNREAD;
                        } else {
                            mu_parser->msg_listing.msg_status = MAP_MESSAGE_STATUS_UNKNOWN;
                        }
                        break;
                    default:
                        break;
                }
                mu_parser->msg_listing.cur_attr = MAP_MSG_ATTR_INVALID;
                break;
            default:
                break;
        }
    }
}

void
map_util_message_listing_parser_init (map_util_xml_parser      *mu_parser,
                                      map_util_xml_parser_type  payload_type,
                                      btstack_packet_handler_t  callback,
                                      uint16_t cid){
    memset (mu_parser, 0, sizeof (map_util_xml_parser));
    yxml_init(&mu_parser->xml_parser, mu_parser->xml_buffer, sizeof(mu_parser->xml_buffer));
    mu_parser->callback = callback;
    mu_parser->cid = cid;
    mu_parser->payload_type = payload_type;
}

void
map_util_xml_parser_parse (map_util_xml_parser *mu_parser,
                           const uint8_t       *data,
                           uint16_t             data_len){
    btstack_assert(data_len < 65535);

    if (!data){
        map_client_emit_parsing_done_event(mu_parser->callback, mu_parser->cid);
        /* mark the parser as invalid */
        mu_parser->payload_type = MAP_UTIL_PARSER_TYPE_INVALID;
        return;
    }

    switch (mu_parser->payload_type) {
        case MAP_UTIL_PARSER_TYPE_FOLDER_LISTING:
            map_client_parse_folder_listing (mu_parser, data, data_len);
            break;

        case MAP_UTIL_PARSER_TYPE_MESSAGE_LISTING:
            map_client_parse_message_listing (mu_parser, data, data_len);
            break;

        default:
            btstack_assert (false);  /* must not be reached */
            break;
    }
}
