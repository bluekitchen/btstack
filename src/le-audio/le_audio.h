/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title Volume Offset Control Service Server
 * 
 */

#ifndef LE_AUDIO_H
#define LE_AUDIO_H

#include <stdint.h>

#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

#define LE_AUDIO_MAX_CODEC_CONFIG_SIZE                          19
#define LE_CCIDS_MAX_NUM                                        10

#define LE_AUDIO_PROGRAM_INFO_MAX_LENGTH                        20
#define LE_AUDIO_PROGRAM_INFO_URI_MAX_LENGTH                    20
#define LE_AUDIO_EXTENDED_METADATA_MAX_LENGHT                   20
#define LE_AUDIO_VENDOR_SPECIFIC_METADATA_MAX_LENGTH            20

// Generic Audio/Audio Location Definitions/Bitmap
#define LE_AUDIO_LOCATION_MASK_NOT_ALLOWED                   0x00000000
#define LE_AUDIO_LOCATION_MASK_FRONT_LEFT                    0x00000001
#define LE_AUDIO_LOCATION_MASK_FRONT_RIGHT                   0x00000002
#define LE_AUDIO_LOCATION_MASK_FRONT_CENTER                  0x00000004
#define LE_AUDIO_LOCATION_MASK_LOW_FREQUENCY_EFFECTS1        0x00000008
#define LE_AUDIO_LOCATION_MASK_BACK_LEFT                     0x00000010
#define LE_AUDIO_LOCATION_MASK_BACK_RIGHT                    0x00000020
#define LE_AUDIO_LOCATION_MASK_FRONT_LEFT_OF_CENTER          0x00000040
#define LE_AUDIO_LOCATION_MASK_FRONT_RIGHT_OF_CENTER         0x00000080
#define LE_AUDIO_LOCATION_MASK_BACK_CENTER                   0x00000100
#define LE_AUDIO_LOCATION_MASK_LOW_FREQUENCY_EFFECTS2        0x00000200
#define LE_AUDIO_LOCATION_MASK_SIDE_LEFT                     0x00000400
#define LE_AUDIO_LOCATION_MASK_SIDE_RIGHT                    0x00000800
#define LE_AUDIO_LOCATION_MASK_TOP_FRONT_LEFT                0x00001000
#define LE_AUDIO_LOCATION_MASK_TOP_FRONT_RIGHT               0x00002000
#define LE_AUDIO_LOCATION_MASK_TOP_FRONT_CENTER              0x00004000
#define LE_AUDIO_LOCATION_MASK_TOP_CENTER                    0x00008000
#define LE_AUDIO_LOCATION_MASK_TOP_BACK_LEFT                 0x00010000
#define LE_AUDIO_LOCATION_MASK_TOP_BACK_RIGHT                0x00020000
#define LE_AUDIO_LOCATION_MASK_TOP_SIDE_LEFT                 0x00040000
#define LE_AUDIO_LOCATION_MASK_TOP_SIDE_RIGHT                0x00080000
#define LE_AUDIO_LOCATION_MASK_TOP_BACK_CENTER               0x00100000
#define LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_CENTER           0x00200000
#define LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_LEFT             0x00400000
#define LE_AUDIO_LOCATION_MASK_BOTTOM_FRONT_RIGHT            0x00800000
#define LE_AUDIO_LOCATION_MASK_FRONT_LEFT_WIDE               0x01000000
#define LE_AUDIO_LOCATION_MASK_FRONT_RIGHT_WIDE              0x02000000
#define LE_AUDIO_LOCATION_MASK_LEFT_SURROUND                 0x04000000
#define LE_AUDIO_LOCATION_MASK_RIGHT_SURROUND                0x08000000
#define LE_AUDIO_LOCATION_MASK_RFU                           0xF0000000

// Generic Audio/Context Mask
#define LE_AUDIO_CONTEXT_MASK_PROHIBITED                0x0000
#define LE_AUDIO_CONTEXT_MASK_UNSPECIFIED               0x0001
#define LE_AUDIO_CONTEXT_MASK_CONVERSATIONAL            0x0002 // Conversation between humans, for example, in telephony or video calls, including traditional cellular as well as VoIP and Push-to-Talk
#define LE_AUDIO_CONTEXT_MASK_MEDIA                     0x0004 // Media, for example, music playback, radio, podcast or movie soundtrack, or tv audio
#define LE_AUDIO_CONTEXT_MASK_GAME                      0x0008 // Audio associated with video gaming, for example gaming media; gaming effects; music and in-game voice chat between participants; or a mix of all the above
#define LE_AUDIO_CONTEXT_MASK_INSTRUCTIONAL             0x0010 // Instructional audio, for example, in navigation, announcements, or user guidance
#define LE_AUDIO_CONTEXT_MASK_VOICE_ASSISTANTS          0x0020 // Man-machine communication, for example, with voice recognition or virtual assistants
#define LE_AUDIO_CONTEXT_MASK_LIVE                      0x0040 // Live audio, for example, from a microphone where audio is perceived both through a direct acoustic path and through an LE Audio Stream
#define LE_AUDIO_CONTEXT_MASK_SOUND_EFFECTS             0x0080 // Sound effects including keyboard and touch feedback; menu and user interface sounds; and other system sounds
#define LE_AUDIO_CONTEXT_MASK_NOTIFICATIONS             0x0100 // Notification and reminder sounds; attention-seeking audio, for example, in beeps signaling the arrival of a message
#define LE_AUDIO_CONTEXT_MASK_RINGTONE                  0x0200 // Alerts the user to an incoming call, for example, an incoming telephony or video call, including traditional cellular as well as VoIP and Push-to-Talk
#define LE_AUDIO_CONTEXT_MASK_ALERTS                    0x0400 // Alarms and timers; immediate alerts, for example, in a critical battery alarm, timer expiry or alarm clock, toaster, cooker, kettle, microwave, etc.
#define LE_AUDIO_CONTEXT_MASK_EMERGENCY_ALARM           0x0800 //Emergency alarm Emergency sounds, for example, fire alarms or other urgent alerts
#define LE_AUDIO_CONTEXT_MASK_RFU                       0xF000

#define LE_AUDIO_OTC_MIN_OBJECT_ID_VALUE                0x000000000100 
#define LE_AUDIO_OTC_MAX_OBJECT_ID_VALUE                0xFFFFFFFFFFFF 

// ASCS: Framing for Codec Configured State 
#define LE_AUDIO_UNFRAMED_ISOAL_MASK_PDUS_SUPPORTED             0x00
#define LE_AUDIO_UNFRAMED_ISOAL_MASK_PDUS_NOT_SUPPORTED         0x01

// ASCS: Server responds with bitmap values: PHY Bitmap for Codec Configured State 
#define LE_AUDIO_SERVER_PHY_MASK_NO_PREFERENCE                         0x00
#define LE_AUDIO_SERVER_PHY_MASK_1M                                    0x01
#define LE_AUDIO_SERVER_PHY_MASK_2M                                    0x02
#define LE_AUDIO_SERVER_PHY_MASK_CODED                                 0x04

// ASCS: Latency for Codec Configured State 
#define LE_AUDIO_SERVER_LATENCY_MASK_NO_PREFERENCE                     0x00
#define LE_AUDIO_SERVER_LATENCY_MASK_LOW                               0x01
#define LE_AUDIO_SERVER_LATENCY_MASK_BALANCED                          0x02
#define LE_AUDIO_SERVER_LATENCY_MASK_HIGH                              0x04

typedef enum {
    LE_AUDIO_ROLE_SINK = 0,
    LE_AUDIO_ROLE_SOURCE,
    LE_AUDIO_ROLE_INVALID
} le_audio_role_t;

// GA Codec_Specific_Configuration LTV structures
typedef enum {
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_UNDEFINED = 0x00,
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY = 0x01,
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_FRAME_DURATION,
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION,
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_OCTETS_PER_CODEC_FRAME,
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_CODEC_FRAME_BLOCKS_PER_SDU,
    LE_AUDIO_CODEC_CONFIGURATION_TYPE_RFU
} le_audio_codec_configuration_type_t;

typedef enum {
    LE_AUDIO_CODEC_CAPABILITY_TYPE_UNDEFINED = 0x00,
    LE_AUDIO_CODEC_CAPABILITY_TYPE_SAMPLING_FREQUENCY = 0x01,
    LE_AUDIO_CODEC_CAPABILITY_TYPE_FRAME_DURATION,
    LE_AUDIO_CODEC_CAPABILITY_TYPE_SUPPORTED_AUDIO_CHANNEL_COUNTS,
    LE_AUDIO_CODEC_CAPABILITY_TYPE_OCTETS_PER_CODEC_FRAME,
    LE_AUDIO_CODEC_CAPABILITY_TYPE_CODEC_FRAME_BLOCKS_PER_SDU,
    LE_AUDIO_CODEC_CAPABILITY_TYPE_RFU
} le_audio_codec_capability_type_t;

#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_8000_HZ      0x0001
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_11025_HZ     0x0002
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_16000_HZ     0x0004
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_22050_HZ     0x0008
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_24000_HZ     0x0010
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_32000_HZ     0x0020
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_44100_HZ     0x0040
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_48000_HZ     0x0080
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_88200_HZ     0x0100
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_96000_HZ     0x0200
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_176400_HZ    0x0400
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_192000_HZ    0x0800
#define LE_AUDIO_CODEC_SAMPLING_FREQUENCY_MASK_384000_HZ    0x1000

typedef enum {
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_INVALID = 0x00,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_8000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_11025_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_16000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_22050_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_24000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_32000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_44100_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_48000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_88200_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_96000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_176400_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_192000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_384000_HZ,
    LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_RFU
} le_audio_codec_sampling_frequency_index_t;

#define LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US               0x00
#define LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US              0x01
#define LE_AUDIO_CODEC_FRAME_DURATION_MASK_7500US_PREFERRED     0x08
#define LE_AUDIO_CODEC_FRAME_DURATION_MASK_10000US_PREFERRED    0x10

typedef enum {
    LE_AUDIO_CODEC_FRAME_DURATION_INDEX_7500US = 0,
    LE_AUDIO_CODEC_FRAME_DURATION_INDEX_10000US,
    LE_AUDIO_CODEC_FRAME_DURATION_INDEX_RFU
} le_audio_codec_frame_duration_index_t;

typedef enum {
    LE_AUDIO_QUALITY_LOW = 0x00,
    LE_AUDIO_QUALITY_MEDIUM,
    LE_AUDIO_QUALITY_HIGH
} le_audio_quality_t;

#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_1            0x01
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_2            0x02
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_3            0x04
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_4            0x08
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_5            0x10
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_6            0x20
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_7            0x40
#define LE_AUDIO_CODEC_AUDIO_CHANNEL_COUNT_MASK_8            0x80


// GA Metadata LTV structures

typedef enum {
    LE_AUDIO_METADATA_TYPE_INVALID = 0x00,
    LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS = 0x01,
    LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS,
    LE_AUDIO_METADATA_TYPE_PROGRAM_INFO,
    LE_AUDIO_METADATA_TYPE_LANGUAGE,
    LE_AUDIO_METADATA_TYPE_CCID_LIST,
    LE_AUDIO_METADATA_TYPE_PARENTAL_RATING,
    LE_AUDIO_METADATA_TYPE_PROGRAM_INFO_URI,
    LE_AUDIO_METADATA_TYPE_MAPPED_EXTENDED_METADATA_BIT_POSITION,
    LE_AUDIO_METADATA_TYPE_MAPPED_VENDOR_SPECIFIC_METADATA_BIT_POSITION,
    // also used to indicate invalid packet type
    LE_AUDIO_METADATA_TYPE_RFU,
    LE_AUDIO_METADATA_TYPE_EXTENDED_METADATA = 0xFE, // mapped on RFU
    LE_AUDIO_METADATA_TYPE_VENDOR_SPECIFIC_METADATA = 0xFF    // mapped on RFU+1
} le_audio_metadata_type_t;

typedef enum {
    LE_AUDIO_PARENTAL_RATING_NO_RATING = 0,
    LE_AUDIO_PARENTAL_RATING_ANY_AGE,
    LE_AUDIO_PARENTAL_RATING_AGE_2,
    LE_AUDIO_PARENTAL_RATING_AGE_3,
    LE_AUDIO_PARENTAL_RATING_AGE_4,
    LE_AUDIO_PARENTAL_RATING_AGE_6,
    LE_AUDIO_PARENTAL_RATING_AGE_7,
    LE_AUDIO_PARENTAL_RATING_AGE_8,
    LE_AUDIO_PARENTAL_RATING_AGE_9,
    LE_AUDIO_PARENTAL_RATING_AGE_10,
    LE_AUDIO_PARENTAL_RATING_RFU = 0x08
} le_audio_parental_rating_t;


typedef enum {
    LE_AUDIO_CLIENT_TARGET_LATENCY_NO_PREFERENCE = 0,
    LE_AUDIO_CLIENT_TARGET_LATENCY_LOW_LATENCY,
    LE_AUDIO_CLIENT_TARGET_LATENCY_BALANCED_LATENCY_AND_RELIABILITY,
    LE_AUDIO_CLIENT_TARGET_LATENCY_HIGH_RELIABILITY,
    LE_AUDIO_CLIENT_TARGET_LATENCY_RFU,
} le_audio_client_target_latency_t;
    
typedef enum {
    LE_AUDIO_CLIENT_TARGET_PHY_NO_PREFERENCE = 0,
    LE_AUDIO_CLIENT_TARGET_PHY_LOW,
    LE_AUDIO_CLIENT_TARGET_PHY_BALANCED,
    LE_AUDIO_CLIENT_TARGET_PHY_HIGH,
    LE_AUDIO_CLIENT_TARGET_PHY_RFU
} le_audio_client_target_phy_t;

// struct for codec id
typedef struct {
    hci_audio_coding_format_t coding_format;
    uint16_t company_id;
    uint16_t vendor_specific_codec_id;
} le_audio_codec_id_t;


typedef enum {
    LE_AUDIO_PA_SYNC_DO_NOT_SYNCHRONIZE_TO_PA = 0x00,
    LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_AVAILABLE,
    LE_AUDIO_PA_SYNC_SYNCHRONIZE_TO_PA_PAST_NOT_AVAILABLE,
    LE_AUDIO_PA_SYNC_RFU
} le_audio_pa_sync_t;

typedef enum {
    LE_AUDIO_PA_SYNC_STATE_NOT_SYNCHRONIZED_TO_PA = 0x00,
    LE_AUDIO_PA_SYNC_STATE_SYNCINFO_REQUEST,
    LE_AUDIO_PA_SYNC_STATE_SYNCHRONIZED_TO_PA,
    LE_AUDIO_PA_SYNC_STATE_FAILED_TO_SYNCHRONIZE_TO_PA,
    LE_AUDIO_PA_SYNC_STATE_NO_PAST,
    LE_AUDIO_PA_SYNC_STATE_RFU
} le_audio_pa_sync_state_t;

typedef enum {
    LE_AUDIO_BIG_ENCRYPTION_NOT_ENCRYPTED = 0x00,
    LE_AUDIO_BIG_ENCRYPTION_BROADCAST_CODE_REQUIRED,
    LE_AUDIO_BIG_ENCRYPTION_DECRYPTING,
    LE_AUDIO_BIG_ENCRYPTION_BAD_CODE,
    LE_AUDIO_BIG_ENCRYPTION_RFU
} le_audio_big_encryption_t;


typedef struct {
    uint16_t metadata_mask;

    uint16_t preferred_audio_contexts_mask;
    uint16_t streaming_audio_contexts_mask;

    uint8_t  program_info_length;                              
    uint8_t  program_info[LE_AUDIO_PROGRAM_INFO_MAX_LENGTH];

    uint32_t language_code; // 3-byte, lower case language code as defined in ISO 639-3

    uint8_t  ccids_num;
    uint8_t  ccids[LE_CCIDS_MAX_NUM];

    le_audio_parental_rating_t parental_rating; 
    
    uint8_t  program_info_uri_length;                              
    uint8_t  program_info_uri[LE_AUDIO_PROGRAM_INFO_URI_MAX_LENGTH];

    uint16_t extended_metadata_type;
    uint8_t  extended_metadata_length;                              
    uint8_t  extended_metadata[LE_AUDIO_EXTENDED_METADATA_MAX_LENGHT];

    uint16_t vendor_specific_company_id;
    uint8_t  vendor_specific_metadata_length;                              
    uint8_t  vendor_specific_metadata[LE_AUDIO_VENDOR_SPECIFIC_METADATA_MAX_LENGTH];
} le_audio_metadata_t;

typedef struct {
    const char * name;
    le_audio_codec_sampling_frequency_index_t sampling_frequency_index;
    le_audio_codec_frame_duration_index_t     frame_duration_index;
    uint16_t octets_per_frame;
} le_audio_codec_configuration_t;

typedef struct {
    const char * name;
    uint16_t sdu_interval_us;
    uint8_t  framing;
    uint16_t max_sdu_size;
    uint8_t  retransmission_number;                  
    uint16_t max_transport_latency_ms; 
} le_audio_qos_configuration_t;

#if defined __cplusplus
}
#endif

#endif

