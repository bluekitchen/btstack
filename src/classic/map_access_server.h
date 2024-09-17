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

// max type header len
#define MAP_SERVER_MAX_TYPE_LEN 42

#define MAP_SERVER_MAX_OBEX_HEADER_BUF 20

// max APP_PARAM_BUFFER len
#define MAP_SERVER_MAX_APP_PARAM_BUFFER 200



// max search value len
#define MAP_SERVER_MAX_SEARCH_VALUE_LEN 32

// we asume our own MAS ConnectionIDs are in the range of 1..31 max
#define HIGHEST_MAS_CONNECTION_ID_VALUE 31

    /* API_START */

    // lengths
#define BT_UINT128_LEN_BYTES 16
// should be 32 - but - BT SIG PTS Test tool expects the version as a 16-bytes binary representation like BT SIG PBAP
// but doesnt accept the BT SIG MAP Spec's up to 32 bytes human-readable hex string
#define BT_UINT128_HEX_LEN_BYTES 16

typedef uint8_t mas_string_t[MAP_SERVER_MAX_TYPE_LEN];
typedef uint8_t mas_utf8_t[MAP_SERVER_MAX_TYPE_LEN];
typedef uint8_t mas_uint128hex_t[BT_UINT128_HEX_LEN_BYTES];
typedef uint8_t mas_uint64_t[8];
typedef uint8_t mas_uint64hex_t[16+1]; // \0 terminated?
typedef uint8_t mas_UTCstmpoffstr_t[20];


#define app_param_read_uint8_t              BT_APP_PARAM_READ_08
#define app_param_read_uint16_t             BT_APP_PARAM_READ_16
#define app_param_read_uint32_t             BT_APP_PARAM_READ_32
#define app_param_read_mas_string_t         BT_APP_PARAM_READ_ARR
#define app_param_read_mas_string_t         BT_APP_PARAM_READ_ARR
#define app_param_read_mas_UTCstmpoffstr_t  BT_APP_PARAM_READ_ARR
#define app_param_read_mas_uint64_t         BT_APP_PARAM_READ_ARR
#define app_param_read_mas_uint64hex_t      BT_APP_PARAM_READ_ARR
#define app_param_read_mas_uint128hex_t     BT_APP_PARAM_READ_ARR
                                            
#define app_param_write_uint8_t             BT_APP_PARAM_WRITE_08
#define app_param_write_uint16_t            BT_APP_PARAM_WRITE_16
#define app_param_write_uint32_t            BT_APP_PARAM_WRITE_32
#define app_param_write_mas_string_t        BT_APP_PARAM_WRITE_ARR
#define app_param_write_mas_UTCstmpoffstr_t BT_APP_PARAM_WRITE_ARR
#define app_param_write_mas_utf8_t          BT_APP_PARAM_WRITE_ARR 
#define app_param_write_mas_uint64_t        BT_APP_PARAM_WRITE_ARR 
#define app_param_write_mas_uint64hex_t     BT_APP_PARAM_WRITE_ARR 
#define app_param_write_mas_uint128hex_t    BT_APP_PARAM_WRITE_ARR 

// APP_PARAM options
#define NO_OPTS  0x00 // no option set
#define OPT_STR0 0x01 // \0 terminated sring with max size

    // Data extracted from "Message Access Profile"
    // Bluetooth  Profile Specification
    // *  Revision : v1.4.2
    // *  Revision Date : 2019 - 08 - 13
    // *  Group Prepared By : Audio, Telephony, and Automotive Working Group

    // the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
    // below is a compact storage of all BT SIG MAS supported ApplicationParameters
    // for both Requests and Responses. Unused parameters dont use any run-time ressources
    // but are declared and compile-time checked as well but
    // 
    //  PARAM_xyz( Parameter_Name                , Tag , Type               , OPTS(STR0), DSCR( free text description ... no coma ... multiple _backslash_no_space lines ... )
#define APP_PARAMS \
 PARAM_REQUST( MaxListCount                      , 0x01, uint16_t           , NO_OPTS   , DSCR( 0000 to 0xFFFF                                                                  ))\
 PARAM_REQUST( ListStartOffset                   , 0x02, uint16_t           , NO_OPTS   , DSCR( 0x0000 to 0xFFFF                                                                ))\
 PARAM_UNUSED( FilterMessageType                 , 0x03, uint8_t            , NO_OPTS   , DSCR( Bit mask: 0b000XXXX1 = "SMS_GSM"                                                  \
                                                                                                          0b000XXX1X = "SMS_CDMA"                                                 \
                                                                                                          0b000XX1XX = "EMAIL" 0b000X1XXX = "MMS" 0b0001XXXX = "IM"               \
                                                                                                          All other values : Reserved for Future Use                              \
                                                                                                          Where                                                                   \
                                                                                                          0 = "no filtering; get this type"                                       \
                                                                                                          1 = "filter out this type"                                            ))\
 PARAM_REQUST( FilterPeriodBegin                 , 0x04, mas_UTCstmpoffstr_t, NO_OPTS   , DSCR( with Begin of filter period.See Section 5.5.4                                   ))\
 PARAM_REQUST( EndFilterPeriodEnd                , 0x05, mas_UTCstmpoffstr_t, NO_OPTS   , DSCR( with End of filter period.See Section 5.5.4                                     ))\
 PARAM_UNUSED( FilterReadStatus                  , 0x06, uint8_t            , NO_OPTS   , DSCR( 1 byte Bit mask : 0b00000001 = get unread messages only                           \
                                                                                                0b00000010 = get read messages only                                               \
                                                                                                0b00000000 =                                                                      \
                                                                                                no - filtering; get both read and unread messages; all other values : undefined ))\
 PARAM_REQUST( FilterRecipient 	                 , 0x07, mas_string_t       , NO_OPTS   , DSCR( variable Text(UTF - 8) wildcards "*" may 	be used if required                 ))\
 PARAM_UNUSED( FilterOriginator                  , 0x08, mas_string_t       , NO_OPTS   , DSCR( variable Text(UTF - 8) wildcards "*" may be used if required                    ))\
 PARAM_UNUSED( FilterPriority                    , 0x09, uint8_t            , NO_OPTS   , DSCR( Bit mask: 0b00000000 = no - filtering                                             \
                                                                                                          0b00000001 = get high priority messages only                            \
                                                                                                          0b00000010 = get non - high priority messages only;                     \
                                                                                                          all other values : undefined                                          ))\
 PARAM_REQUST( Attachment                        , 0x0A, uint8_t            , NO_OPTS   , ENUM( 1 , ON )                                                                          \
                                                                                          ENUM( 0 , OFF)                                                                         )\
 PARAM_UNUSED( Transparent                       , 0x0B, uint8_t            , NO_OPTS   , DSCR( 0b1 = "ON"                                                                        \
                                                                                                0b0 = "OFF"                                                                     ))\
 PARAM_UNUSED( Retry                             , 0x0C, uint8_t            , NO_OPTS   , DSCR( 0b1 = "ON"                                                                        \
                                                                                                0b0 = "OFF"                                                                     ))\
 PARAM_RESPON( NewMessage                        , 0x0D, uint8_t            , NO_OPTS   , DSCR( 0b1 = "ON"                                                                        \
                                                                                                0b0 = "OFF"                                                                     ))\
 PARAM_REQUST( NotificationStatus                , 0x0E, uint8_t            , NO_OPTS   , DSCR( 0b1 = "ON"                                                                        \
                                                                                                0b0 = "OFF"                                                                     ))\
 PARAM_REQUST( MASInstanceID                     , 0x0F, uint8_t            , NO_OPTS   , DSCR( 0 to 255                                                                        ))\
 PARAM_UNUSED( ParameterMask                     , 0x10, uint32_t           , NO_OPTS   , DSCR( Bit mask; settings see Section 5.5.4                                            ))\
 PARAM_UNUSED( FolderListingSize                 , 0x11, uint16_t           , NO_OPTS   , DSCR( 0x0000 to 0xFFFF                                                                ))\
 PARAM_RESPON( ListingSize                       , 0x12, uint16_t           , NO_OPTS   , DSCR( 0x0000 to 0xFFFF                                                                ))\
 PARAM_UNUSED( SubjectLength                     , 0x13, uint8_t            , NO_OPTS   , DSCR( 1 to 255                                                                        ))\
 PARAM_REQUST( Charset                           , 0x14, uint8_t            , NO_OPTS   , ENUM( 0 , native )                                                                    \
                                                                                          ENUM( 1 , UTF_8  )                                                                    )\
 PARAM_UNUSED( FractionRequest                   , 0x15, uint8_t            , NO_OPTS   , DSCR( 0 = "first" 1 = "next"                                                          ))\
 PARAM_UNUSED( FractionDeliver                   , 0x16, uint8_t            , NO_OPTS   , DSCR( 0 = "more"                                                                        \
                                                                                                1 = "last"                                                                      ))\
 PARAM_REQUST( StatusIndicator                   , 0x17, uint8_t            , NO_OPTS   , ENUM( 0 , readStatus      )                                                             \
                                                                                          ENUM( 1 , deletedStatus   )                                                             \
                                                                                          ENUM( 2 , setExtendedData )                                                            )\
 PARAM_REQUST( StatusValue                       , 0x18, uint8_t            , NO_OPTS   , DSCR( 1 = "yes"                                                                         \
                                                                                                0 = "no"                                                                        ))\
 PARAM_RESPON( MSETime                           , 0x19, mas_UTCstmpoffstr_t, NO_OPTS   , DSCR( with current time basis and UTC - offset of the MSE.See Section 5.5.4           ))\
 PARAM_RESPON( DatabaseIdentifier                , 0x1A, mas_uint128hex_t   , NO_OPTS   , DSCR( (max 3uint16_t)    ;   128 - bit value in hex string format                     ))\
 PARAM_RESPON( ConversationListingVersionCounter , 0x1B, mas_uint128hex_t   , NO_OPTS   , DSCR( (max 3uint16_t)    ;   128 - bit value in hex string format                     ))\
 PARAM_UNUSED( PresenceAvailability              , 0x1C, uint8_t            , NO_OPTS   , DSCR( 0 to 255                                                                        ))\
 PARAM_UNUSED( PresenceText                      , 0x1D, mas_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_REQUST( LastActivity                      , 0x1E, mas_UTCstmpoffstr_t, NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_UNUSED( FilterLastActivityBegin           , 0x1F, mas_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_UNUSED( FilterLastActivityEnd             , 0x20, mas_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_REQUST( ChatState                         , 0x21, uint8_t            , NO_OPTS   , DSCR( 0 to 255                                                                        ))\
 PARAM_REQRSP( ConversationID                    , 0x22, mas_uint128hex_t   , NO_OPTS   , DSCR( (max 3uint16_t)    ;   128 - bit value in hex string format                     ))\
 PARAM_RESPON( FolderVersionCounter              , 0x23, mas_uint128hex_t   , NO_OPTS   , DSCR( (max 3uint16_t);   128 - bit value in hex string format                         ))\
 PARAM_UNUSED( FilterMessageHandle               , 0x24, mas_uint64_t       , NO_OPTS   , DSCR( 64 - bit value in hex string format                                             ))\
 PARAM_REQUST( NotificationFilterMask            , 0x25, uint32_t           , NO_OPTS   , DSCR( Bit mask settings; see Section 5.14.3.1 )                                         \
                                                                                          ENUM(      1 << 0  , NewMessage                   )                                     \
                                                                                          ENUM(      1 << 1  , MessageDeleted               )                                     \
                                                                                          ENUM(      1 << 2  , MessageShift                 )                                     \
                                                                                          ENUM(      1 << 3  , SendingSuccess               )                                     \
                                                                                          ENUM(      1 << 4  , SendingFailure               )                                     \
                                                                                          ENUM(      1 << 5  , DeliverySuccess              )                                     \
                                                                                          ENUM(      1 << 6  , DeliveryFailure              )                                     \
                                                                                          ENUM(      1 << 7  , MemoryFull                   )                                     \
                                                                                          ENUM(      1 << 8  , MemoryAvailable              )                                     \
                                                                                          ENUM(      1 << 9  , ReadStatusChanged            )                                     \
                                                                                          ENUM(      1 << 10 , ConversationChanged          )                                     \
                                                                                          ENUM(      1 << 11 , ParticipantPresenceChanged   )                                     \
                                                                                          ENUM(      1 << 12 , ParticipantChatStateChanged  )                                     \
                                                                                          ENUM(      1 << 13 , MessageExtendedDataChanged   )                                     \
                                                                                          ENUM(      1 << 14 , MessageRemoved               )                                     \
                                                                                          ENUM(   N_BITS(15) , AllNotifications             )                                     \
                                                                                          ENUM(0x1FFFF << 15 , Reserved_Mask                )                                    )\
 PARAM_UNUSED( ConvParameterMask                 , 0x26, uint32_t           , NO_OPTS   , DSCR( Bit mask settings; see Section 5.13.3.10                                        ))\
 PARAM_RESPON( OwnerUCI                          , 0x27, mas_utf8_t         , OPT_STR0  , DSCR( Text UTF - 8                                                                    ))\
 PARAM_UNUSED( ExtendedData                      , 0x28, mas_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_REQUST( MapSupportedFeatures              , 0x29, uint32_t           , NO_OPTS   , DSCR( Bit 0 = Notification Registration Feature                                         \
                                                                                                Bit 1 = Notification Feature                                                      \
                                                                                                Bit 2 = Browsing Feature                                                          \
                                                                                                Bit 3 = Uploading Feature                                                         \
                                                                                                Bit 4 = Delete Feature                                                            \
                                                                                                Bit 5 = Instance Information Feature                                              \
                                                                                                Bit 6 = Extended Event Report 1.1                                                 \
                                                                                                Bit 7 = Event Report Version 1.2                                                  \
                                                                                                Bit 8 = Message Format Version 1.1                                                \
                                                                                                Bit 9 = Messages - Listing Format Version 1.1                                     \
                                                                                                Bit 10 = Persistent Message Handles                                               \
                                                                                                Bit 11 = Database Identifier                                                      \
                                                                                                Bit 12 = Folder Version Counter                                                   \
                                                                                                Bit 13 = Conversation Version Counters                                            \
                                                                                                Bit 14 = Participant Presence Change Notification                                 \
                                                                                                Bit 15 = Participant Chat State Change Notification                               \
                                                                                                Bit 16 = PBAP Contact Cross Reference                                             \
                                                                                                Bit 17 = Notification Filtering                                                   \
                                                                                                Bit 18 = UTC Offset Timestamp Format                                              \
                                                                                                Bit 19 = Reserved                                                                 \
                                                                                                Bit 20 = Conversation listing                                                     \
                                                                                                Bit 21 = Owner status                                                             \
                                                                                                Bit 22 = Message Forwarding                                                       \
                                                                                                Bit 23 = Unacknowledged Message Indication                                        \
                                                                                                Bit 24 = Eventreport 1.3                                                          \
                                                                                                Bits 22 to 31 = Reserved for Future Use                                         ))\
 PARAM_REQUST( MessageHandle                     , 0x2A, mas_uint64hex_t    , NO_OPTS   , DSCR( 64 - bit value in hex string format                                             ))\
 PARAM_REQUST( ModifyText                        , 0x2B, uint8_t            , NO_OPTS   , ENUM( 0 , REPLACE)                                                                     )


    enum MAP_APP_PARAMS
    {
        // the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
        // below defines enum MAP_APP_PARAMS members MAP_APP_PARAM_xyz = tag with BT SPECs tag values
#define PARAM_REQUST(name, tag, type, opts, descr) MAP_APP_PARAM_ ## name = tag,
#define PARAM_RESPON PARAM_REQUST
#define PARAM_REQRSP PARAM_REQUST
#define PARAM_UNUSED PARAM_REQUST
#define ENUM(...)
#define DSCR(...)

        APP_PARAMS
#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR

    };

    enum MAP_APP_PARAMS_OPTIONS
    {
    // the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
    // below defines enum MAP_APP_PARAMS members MAP_APP_PARAM_xyz = tag with BT SPECs tag values
#define ENUM(...)
#define DSCR(...)
#define PARAM_REQUST(name, tag, type, opts, descr) MAP_APP_PARAMS_OPTIONS_ ## name = opts,
#define PARAM_RESPON PARAM_REQUST
#define PARAM_REQRSP PARAM_REQUST
#define PARAM_UNUSED PARAM_REQUST
    APP_PARAMS

#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR
    };


        // the following X-Macro (https://en.wikipedia.org/wiki/X_macro)
        // below defines enum MAP_APP_PARAMS members MAP_APP_PARAM_xyz = tag with BT SPECs tag values
#define ENUM(value, enumname) MAP_APP_PARAM_SUB_ ## enumname = value,
#define DSCR(...)

#define PARAM_REQUST(name, tag, type, opts, descr) enum MAP_APP_PARAM_SUB_ ## name { MAP_APP_PARAM_SUB_ ## name ## _min = 0, descr };
#define PARAM_RESPON PARAM_REQUST
#define PARAM_REQRSP PARAM_REQUST
#define PARAM_UNUSED PARAM_REQUST
        APP_PARAMS

#undef PARAM_REQUST
#undef PARAM_RESPON
#undef PARAM_REQRSP
#undef PARAM_UNUSED
#undef ENUM
#undef DSCR



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
