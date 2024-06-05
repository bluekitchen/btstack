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

#ifndef MAP_UTIL_H
#define MAP_UTIL_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "btstack_config.h"
#include "classic/map.h"
#include "yxml.h"

/* API_START */


/**
 * @brief Create SDP record for Message Access Service
 * @param service
 * @param service_record_handle
 * @param instance_id
 * @param rfcomm_channel_nr
 * @param goep_l2cap_psm
 * @param supported_message_types
 * @param supported_features
 * @param name
 */

    void map_util_create_access_service_sdp_record(uint8_t* service, uint32_t service_record_handle, uint8_t instance_id,
        uint8_t rfcomm_channel_nr, uint16_t goep_l2cap_psm, uint8_t supported_message_types, uint32_t supported_features, const char* name);


/**
 * @brief Create SDP record for Message Notification Service
 * @param service
 * @param service_record_handle
 * @param instance_id
 * @param rfcomm_channel_nr
 * @param goep_l2cap_psm
 * @param supported_message_types
 * @param supported_features
 * @param name
 */

void map_util_create_notification_service_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t instance_id,
                                                     uint8_t rfcomm_channel_nr, uint16_t goep_l2cap_psm, map_message_type_t supported_message_types, uint32_t supported_features, const char * name);

/**
 * @brief XML parser for message listing
 */

typedef enum {
    MAP_UTIL_PARSER_TYPE_INVALID = 0,
    MAP_UTIL_PARSER_TYPE_FOLDER_LISTING,
    MAP_UTIL_PARSER_TYPE_MESSAGE_LISTING,
    MAP_UTIL_PARSER_TYPE_CONVERSATION_LISTING,
} map_util_xml_parser_type;

typedef struct {
    btstack_packet_handler_t callback;
    uint16_t cid;
    yxml_t   xml_parser;
    uint8_t  payload_type;
    uint8_t  xml_buffer[50];
    int      cur_attr;
    char     attr_val[MAP_MAX_VALUE_LEN];
    union {
        struct {
            int message_found;
            map_message_type_t   msg_type;
            map_message_status_t msg_status;
            map_message_handle_t msg_handle;
        } msg_listing;
        struct {
            int conv_found;
            map_conversation_id_t conv_id;
        } conv_listing;
        struct {
            int folder_found;
            int name_found;
            char name[MAP_MAX_VALUE_LEN];
        } folder_listing;
    };
} map_util_xml_parser;

void map_util_xml_parser_init (map_util_xml_parser      *mu_parser,
                               map_util_xml_parser_type  payload_type,
                               btstack_packet_handler_t  callback,
                               uint16_t cid);
void map_util_xml_parser_parse (map_util_xml_parser *mu_parser,
                                const uint8_t       *data,
                                uint16_t             data_len);

/* API_END */

// only for testing
    
void map_message_str_to_handle(const char * value, map_message_handle_t msg_handle);
void map_message_handle_to_str(char * p, const map_message_handle_t msg_handle);
void map_conversation_str_to_id(const char * value, map_conversation_id_t conv_id);

#if defined __cplusplus
}
#endif
#endif
