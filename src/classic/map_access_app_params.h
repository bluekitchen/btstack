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

 // lengths
// max type header len
#define MAP_ACCESS_MAX_TYPE_LEN 42
#define BT_UINT128_LEN_BYTES 16
// should be 32 - but - BT SIG PTS Test tool expects the version as a 16-bytes binary representation like BT SIG PBAP
// but doesnt accept the BT SIG MAP Spec's up to 32 bytes human-readable hex string
#define BT_UINT128_LEN_BYTES 16
#define BT_UINT128hex_LEN_BYTES 32

typedef uint8_t map_string_t[MAP_ACCESS_MAX_TYPE_LEN];
typedef uint8_t map_utf8_t[MAP_ACCESS_MAX_TYPE_LEN];
typedef uint8_t map_uint128_t[BT_UINT128_LEN_BYTES];
typedef uint8_t map_uint128hex_t[BT_UINT128hex_LEN_BYTES];
typedef uint8_t map_uint64_t[8];
typedef uint8_t map_uint64hex_t[16 + 1]; // \0 terminated?
typedef uint8_t map_UTCstmpoffstr_t[20];


#define app_param_read_uint8_t              BT_APP_PARAM_READ_08
#define app_param_read_uint16_t             BT_APP_PARAM_READ_16
#define app_param_read_uint32_t             BT_APP_PARAM_READ_32
#define app_param_read_map_string_t         BT_APP_PARAM_READ_ARR
#define app_param_read_map_string_t         BT_APP_PARAM_READ_ARR
#define app_param_read_map_UTCstmpoffstr_t  BT_APP_PARAM_READ_ARR
#define app_param_read_map_uint64_t         BT_APP_PARAM_READ_ARR
#define app_param_read_map_uint64hex_t      BT_APP_PARAM_READ_ARR
#define app_param_read_map_uint128_t        BT_APP_PARAM_READ_ARR
#define app_param_read_map_uint128hex_t     BT_APP_PARAM_READ_ARR

#define app_param_write_uint8_t             BT_APP_PARAM_WRITE_08
#define app_param_write_uint16_t            BT_APP_PARAM_WRITE_16
#define app_param_write_uint32_t            BT_APP_PARAM_WRITE_32
#define app_param_write_map_string_t        BT_APP_PARAM_WRITE_ARR
#define app_param_write_map_UTCstmpoffstr_t BT_APP_PARAM_WRITE_ARR
#define app_param_write_map_utf8_t          BT_APP_PARAM_WRITE_ARR 
#define app_param_write_map_uint64_t        BT_APP_PARAM_WRITE_ARR 
#define app_param_write_map_uint64hex_t     BT_APP_PARAM_WRITE_ARR 
#define app_param_write_map_uint128_t       BT_APP_PARAM_WRITE_ARR 
#define app_param_write_map_uint128hex_t    BT_APP_PARAM_WRITE_ARR 


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
 PARAM_REQUST( FilterPeriodBegin                 , 0x04, map_UTCstmpoffstr_t, NO_OPTS   , DSCR( with Begin of filter period.See Section 5.5.4                                   ))\
 PARAM_REQUST( EndFilterPeriodEnd                , 0x05, map_UTCstmpoffstr_t, NO_OPTS   , DSCR( with End of filter period.See Section 5.5.4                                     ))\
 PARAM_UNUSED( FilterReadStatus                  , 0x06, uint8_t            , NO_OPTS   , DSCR( 1 byte Bit mask : 0b00000001 = get unread messages only                           \
                                                                                                0b00000010 = get read messages only                                               \
                                                                                                0b00000000 =                                                                      \
                                                                                                no - filtering; get both read and unread messages; all other values : undefined ))\
 PARAM_REQUST( FilterRecipient 	                 , 0x07, map_string_t       , NO_OPTS   , DSCR( variable Text(UTF - 8) wildcards "*" may 	be used if required                 ))\
 PARAM_UNUSED( FilterOriginator                  , 0x08, map_string_t       , NO_OPTS   , DSCR( variable Text(UTF - 8) wildcards "*" may be used if required                    ))\
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
 PARAM_RESPON( MSETime                           , 0x19, map_UTCstmpoffstr_t, NO_OPTS   , DSCR( with current time basis and UTC - offset of the MSE.See Section 5.5.4           ))\
 PARAM_RESPON( DatabaseIdentifier                , 0x1A, map_uint128_t   , NO_OPTS   , DSCR( (max 3uint16_t)    ;   128 - bit value in hex string format                     ))\
 PARAM_RESPON( ConversationListingVersionCounter , 0x1B, map_uint128_t   , NO_OPTS   , DSCR( (max 3uint16_t)    ;   128 - bit value in hex string format                     ))\
 PARAM_UNUSED( PresenceAvailability              , 0x1C, uint8_t            , NO_OPTS   , DSCR( 0 to 255                                                                        ))\
 PARAM_UNUSED( PresenceText                      , 0x1D, map_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_REQUST( LastActivity                      , 0x1E, map_UTCstmpoffstr_t, NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_UNUSED( FilterLastActivityBegin           , 0x1F, map_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_UNUSED( FilterLastActivityEnd             , 0x20, map_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
 PARAM_REQUST( ChatState                         , 0x21, uint8_t            , NO_OPTS   , DSCR( 0 to 255                                                                        ))\
 PARAM_REQRSP( ConversationID                    , 0x22, map_uint128_t   , NO_OPTS   , DSCR( (max 3uint16_t)    ;   128 - bit value in hex string format                     ))\
 PARAM_RESPON( FolderVersionCounter              , 0x23, map_uint128_t   , NO_OPTS   , DSCR( (max 3uint16_t);   128 - bit value in hex string format                         ))\
 PARAM_UNUSED( FilterMessageHandle               , 0x24, map_uint64_t       , NO_OPTS   , DSCR( 64 - bit value in hex string format                                             ))\
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
 PARAM_RESPON( OwnerUCI                          , 0x27, map_utf8_t         , OPT_STR0  , DSCR( Text UTF - 8                                                                    ))\
 PARAM_UNUSED( ExtendedData                      , 0x28, map_utf8_t         , NO_OPTS   , DSCR( Text UTF - 8                                                                    ))\
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
 PARAM_REQUST( MessageHandle                     , 0x2A, map_uint64hex_t    , NO_OPTS   , DSCR( 64 - bit value in hex string format                                             ))\
 PARAM_REQUST( ModifyText                        , 0x2B, uint8_t            , NO_OPTS   , ENUM( 0 , REPLACE)                                                                     )

