/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

/**
 *  @brief TODO
 */

#ifndef MAP_ACCESS_SERVER_H
#define MAP_ACCESS_SERVER_H

#if defined __cplusplus
extern "C" {
#endif
#include "btstack_config.h"
#include "btstack_defines.h"

// BT SIG Mapp Access Server Spec tests up to 2 open connections
#define MAS_MAX_CONNECTIONS 2

// max name header len
#define MAP_SERVER_MAX_NAME_LEN 32

#define MAP_SERVER_MAX_OBEX_HEADER_BUF 20

// max APP_PARAM_BUFFER len
#define MAP_SERVER_MAX_APP_PARAM_BUFFER 200



// max search value len
#define MAP_SERVER_MAX_SEARCH_VALUE_LEN 32

// we asume our own MAS ConnectionIDs are in the range of 1..31 max
#define HIGHEST_MAS_CONNECTION_ID_VALUE 31

    /* API_START */
#include "classic/map_access_app_params.h"

// MAP Supported Features
#define MAP_SUPPORTED_FEATURES_NOTIFICATION_REGISTRATION                    (1 <<  0) // Notification Registration                 
#define MAP_SUPPORTED_FEATURES_NOTIFICATION                                 (1 <<  1) // Notification                              
#define MAP_SUPPORTED_FEATURES_BROWSING                                     (1 <<  2) // Browsing                                  
#define MAP_SUPPORTED_FEATURES_UPLOADING                                    (1 <<  3) // Uploading                                 
#define MAP_SUPPORTED_FEATURES_DELETE                                       (1 <<  4) // Delete                                    
#define MAP_SUPPORTED_FEATURES_INSTANCE_INFORMATION                         (1 <<  5) // Instance Information                      
#define MAP_SUPPORTED_FEATURES_EXTENDED_EVENT_REPORT_1_1                    (1 <<  6) // Extended Event Report 1.1                 
#define MAP_SUPPORTED_FEATURES_EVENT_REPORT_VERSION_1_2                     (1 <<  7) // Event Report Version 1.2                  
#define MAP_SUPPORTED_FEATURES_MESSAGE_FORMAT_VERSION_1_1                   (1 <<  8) // Message Format Version 1.1                
#define MAP_SUPPORTED_FEATURES_MESSAGES_LISTING_FORMAT_VERSION_1_1          (1 <<  9) // Messages-Listing Format Version 1.1       
#define MAP_SUPPORTED_FEATURES_PERSISTENT_MESSAGE_HANDLES                   (1 << 10) // Persistent Message Handles                
#define MAP_SUPPORTED_FEATURES_DATABASE_IDENTIFIER                          (1 << 11) // Database Identifier                       
#define MAP_SUPPORTED_FEATURES_FOLDER_VERSION_COUNTER                       (1 << 12) // Folder Version Counter                    
#define MAP_SUPPORTED_FEATURES_CONVERSATION_VERSION_COUNTERS                (1 << 13) // Conversation Version Counters             
#define MAP_SUPPORTED_FEATURES_PARTICIPANT_PRESENCE_CHANGE_NOTIFICATION     (1 << 14) // Participant Presence Change Notification  
#define MAP_SUPPORTED_FEATURES_PARTICIPANT_CHAT_STATE_CHANGE_NOTIFICATION   (1 << 15) // Participant Chat State Change Notification
#define MAP_SUPPORTED_FEATURES_PBAP_CONTACT_CROSS_REFERENCE                 (1 << 16) // PBAP Contact Cross Reference              
#define MAP_SUPPORTED_FEATURES_NOTIFICATION_FILTERING                       (1 << 17) // Notification Filtering                    
#define MAP_SUPPORTED_FEATURES_UTC_OFFSET_TIMESTAMP_FORMAT                  (1 << 18) // UTC Offset Timestamp Format               
#define MAP_SUPPORTED_FEATURES_MAPSUPPORTEDFEATURES_IN_CONNECT_REQUEST      (1 << 19) // Reserved                                  
#define MAP_SUPPORTED_FEATURES_CONVERSATION_LISTING                         (1 << 20) // Conversation listing                      
#define MAP_SUPPORTED_FEATURES_OWNER_STATUS                                 (1 << 21) // Owner status
#define MAP_SUPPORTED_FEATURES_MESSAGE_FORWARDING                           (1 << 22) // Message Forwarding
#define MAP_SUPPORTED_FEATURES_UNACKNOWLEDGED_MESSAGE_INDICATION            (1 << 23) // PTS: UnacknowledgedMessageIndication???
#define MAP_SUPPORTED_FEATURES_EVENTREPORT_1_3                              (1 << 24) // PTS: EventReport_1_3 ???
#define MAP_SUPPORTED_FEATURES_ALL                                          0x1FFFFFF  // we support all features

// SupportedMessageTypes                                                    
#define MAP_SUPPORTED_MESSAGE_TYPE_EMAIL                                    (1 << 0) //  EMAIL   
#define MAP_SUPPORTED_MESSAGE_TYPE_SMS_GSM                                  (1 << 1) //  SMS_GSM 
#define MAP_SUPPORTED_MESSAGE_TYPE_SMS_CDMA                                 (1 << 2) //  SMS_CDMA
#define MAP_SUPPORTED_MESSAGE_TYPE_MMS                                      (1 << 3) //  MMS     
#define MAP_SUPPORTED_MESSAGE_TYPE_IM                                       (1 << 4) //  IM

// MAP vCardSelectorOperator
#define MAP_MSG_SELECTOR_OPERATOR_OR          0
#define MAP_MSG_SELECTOR_OPERATOR_AND         1

// MAP Format
typedef enum {
    MAP_FORMAT_MSG_10 = 0,
    MAP_FORMAT_MSG_11
} map_format_msg_t;

// MAP Object Types
typedef enum {
    MAP_OBJECT_TYPE_INVALID = 0,
    MAP_OBJECT_TYPE_UNKNOWN,
    MAP_OBJECT_TYPE_GET_FOLDER_LISTING,
    MAP_OBJECT_TYPE_GET_MSG_LISTING,
    MAP_OBJECT_TYPE_GET_CONVO_LISTING,
    MAP_OBJECT_TYPE_GET_MAS_INSTANCE_INFORMATION,
    MAP_OBJECT_TYPE_GET_MESSAGE,
    MAP_OBJECT_TYPE_PUT_MESSAGE_STATUS,
    MAP_OBJECT_TYPE_PUT_MESSAGE_UPDATE,
    MAP_OBJECT_TYPE_PUT_MESSAGE,
    MAP_OBJECT_TYPE_PUT_MESSAGE_CONTINUE,
#ifdef MAP_PTS_BUG_TC_MAP_OLD_MAP_MSE_GOEP_SRM_BV_04
    MAP_OBJECT_TYPE_PUT_EMPTY,
#endif
    MAP_OBJECT_TYPE_PUT_NOTIFICATION_REGISTRATION,
    MAP_OBJECT_TYPE_PUT_SET_NOTIFICATION_FILTER,
    MAP_OBJECT_TYPE_PUT_OWNER_STATUS
} map_object_type_t;

// MAP Folders
typedef enum {
    MAS_FOLDER_MIN, MAS_FOLDER_INVALID = 0,
    MAS_FOLDER_ROOT,
    MAS_FOLDER_TELECOM,
    MAS_FOLDER_TELECOM_MSG, 
    MAS_FOLDER_TELECOM_MSG_INBOX,
    MAS_FOLDER_TELECOM_MSG_OUTBOX,
    MAS_FOLDER_TELECOM_MSG_DRAFT,
    MAS_FOLDER_TELECOM_MSG_SENT,
    MAS_FOLDER_MAX,
} mas_folder_t;

int map_server_set_response_app_param(uint16_t map_cid, enum MAP_APP_PARAMS app_param, void* param);
uint16_t map_server_send_response_with_body(uint16_t map_cid, uint8_t response_code, uint32_t continuation, size_t body_len, const uint8_t* body);
uint16_t map_server_send_response(uint16_t map_cid, uint8_t response_code);
uint16_t map_server_get_max_body_size(uint16_t map_cid);
void map_server_set_response_type_and_name(uint16_t map_cid, char* hdr_name, char* type_name, bool SRMP_wait);
int map_server_set_response_app_param(uint16_t map_cid, enum MAP_APP_PARAMS app_param, void* param);
void map_server_init(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel_nr, uint16_t l2cap_psm, uint16_t mtu);


// function pointers to some test case management functions
struct test_set_config {
    void (*fp_init_test_cases)(struct test_set_config* cfg);
    void (*fp_next_test_case)(struct test_set_config* cfg);
    void (*fp_previous_test_case)(struct test_set_config* cfg);
    void (*fp_select_test_case_n)(struct test_set_config* cfg, uint8_t n);
    void (*fp_print_test_config)(struct test_set_config* cfg);
    void (*fp_print_test_cases)(struct test_set_config* cfg);
    void (*fp_print_test_case_help)(struct test_set_config* cfg);
};

enum msg_status_read { no, yes };

const char* map_server_get_folder_MsgListingDir(char* path);
const char* map_server_get_folder_MsgListingSent(char* path);


#if defined __cplusplus
}
#endif
#endif // MAP_ACCESS_SERVER_H
