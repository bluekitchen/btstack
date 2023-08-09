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
 * HFP Hands-Free (HF) unit and Audio Gateway Commons
 *
 */

#ifndef BTSTACK_HFP_H
#define BTSTACK_HFP_H

#include "hci.h"
#include "classic/sdp_client_rfcomm.h"

#if defined __cplusplus
extern "C" {
#endif

// period AG will send RING messages
#define HFP_RING_PERIOD_MS 2000

/* HF Supported Features: 
0: EC and/or NR function
1: Three-way calling
2: CLI presentation capability
3: Voice recognition activation
4: Remote volume control
5: Enhanced call status
6: Enhanced call control
7: Codec negotiation
8: HF Indicators
9: eSCO S4 (and T2) Settings Supported
10-31: Reserved for future definition
*/
#define HFP_HFSF_EC_NR_FUNCTION                          0
#define HFP_HFSF_THREE_WAY_CALLING                       1
#define HFP_HFSF_CLI_PRESENTATION_CAPABILITY             2
#define HFP_HFSF_VOICE_RECOGNITION_FUNCTION              3 
#define HFP_HFSF_REMOTE_VOLUME_CONTROL                   4
#define HFP_HFSF_ENHANCED_CALL_STATUS                    5  
#define HFP_HFSF_ENHANCED_CALL_CONTROL                   6 
#define HFP_HFSF_CODEC_NEGOTIATION                       7  
#define HFP_HFSF_HF_INDICATORS                           8
#define HFP_HFSF_ESCO_S4                                 9
#define HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS      10
#define HFP_HFSF_VOICE_RECOGNITION_TEXT                 11

/* AG Supported Features:
0: Three-way calling
1: EC and/or NR function
2: Voice recognition function
3: In-band ring tone capability
4: Attach a number to a voice tag
5: Ability to reject a call
6: Enhanced call status
7: Enhanced call control
8: Extended Error Result Codes
9: Codec negotiation
10: HF Indicators
11: eSCO S4 (and T2) Settings Supported
12-31: Reserved for future definition
*/
#define HFP_AGSF_THREE_WAY_CALLING                       0
#define HFP_AGSF_EC_NR_FUNCTION                          1
#define HFP_AGSF_VOICE_RECOGNITION_FUNCTION              2
#define HFP_AGSF_IN_BAND_RING_TONE                       3
#define HFP_AGSF_ATTACH_A_NUMBER_TO_A_VOICE_TAG          4
#define HFP_AGSF_ABILITY_TO_REJECT_A_CALL                5
#define HFP_AGSF_ENHANCED_CALL_STATUS                    6
#define HFP_AGSF_ENHANCED_CALL_CONTROL                   7
#define HFP_AGSF_EXTENDED_ERROR_RESULT_CODES             8
#define HFP_AGSF_CODEC_NEGOTIATION                       9
#define HFP_AGSF_HF_INDICATORS                          10
#define HFP_AGSF_ESCO_S4                                11
#define HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS      12
#define HFP_AGSF_VOICE_RECOGNITION_TEXT                 13

#define HFP_DEFAULT_HF_SUPPORTED_FEATURES 0x0000
#define HFP_DEFAULT_AG_SUPPORTED_FEATURES 0x0009

#define HFP_MAX_NUM_INDICATORS                  10
#define HFP_MAX_NUM_CALL_SERVICES               20
#define HFP_CALL_SERVICE_SIZE                    3
#define HFP_MAX_NUM_CODECS                      10
#define HFP_BNEP_NUM_MAX_SIZE                   25
#define HFP_MAX_INDICATOR_DESC_SIZE             20
#define HFP_MAX_VR_TEXT_SIZE                   100
#define HFP_VR_TEXT_HEADER_SIZE                 27      // bytes needed for sending +BVRA message including quotes but excluding string length: 
                                                        // \r\n+BVRA: <vrect>,<vrecstate>,<textID>,<textType>,<textOperation>,"<string>"\r\n

#define HFP_MAX_NETWORK_OPERATOR_NAME_SIZE 17   

#define HFP_HF_INDICATOR_UUID_ENHANCED_SAFETY   0x0001  // 0 - disabled, 1 - enabled
#define HFP_HF_INDICATOR_UUID_BATTERY_LEVEL     0X0002  // 0-100 remaining level of battery
    
#define HFP_SUPPORTED_FEATURES "+BRSF"
#define HFP_AVAILABLE_CODECS "+BAC"
#define HFP_INDICATOR "+CIND"
#define HFP_ENABLE_STATUS_UPDATE_FOR_AG_INDICATORS "+CMER"
#define HFP_ENABLE_CLIP "+CLIP"
#define HFP_ENABLE_CALL_WAITING_NOTIFICATION "+CCWA"
#define HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS "+BIA" // +BIA:<enabled>,,<enabled>,,,<enabled>
#define HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES "+CHLD"
#define HFP_GENERIC_STATUS_INDICATOR "+BIND"
#define HFP_TRANSFER_AG_INDICATOR_STATUS "+CIEV" // +CIEV: <index>,<value>
#define HFP_TRANSFER_HF_INDICATOR_STATUS "+BIEV" // +BIEC: <index>,<value>
#define HFP_QUERY_OPERATOR_SELECTION "+COPS"     // +COPS: <mode>,0,<opearator>
#define HFP_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR "+CMEE"
#define HFP_EXTENDED_AUDIO_GATEWAY_ERROR "+CME ERROR"
#define HFP_TRIGGER_CODEC_CONNECTION_SETUP "+BCC"
#define HFP_CONFIRM_COMMON_CODEC "+BCS"
#define HFP_ANSWER_CALL "ATA"
#define HFP_HANG_UP_CALL "+CHUP"
#define HFP_CHANGE_IN_BAND_RING_TONE_SETTING "+BSIR"
#define HFP_CALL_PHONE_NUMBER "ATD"
#define HFP_REDIAL_LAST_NUMBER "+BLDN"
#define HFP_TURN_OFF_EC_AND_NR "+NREC" // EC (Echo CAnceling), NR (Noise Reduction)
#define HFP_ACTIVATE_VOICE_RECOGNITION "+BVRA" // Voice Recognition
#define HFP_SET_MICROPHONE_GAIN  "+VGM"
#define HFP_SET_SPEAKER_GAIN     "+VGS"
#define HFP_PHONE_NUMBER_FOR_VOICE_TAG "+BINP"
#define HFP_TRANSMIT_DTMF_CODES  "+VTS"
#define HFP_SUBSCRIBER_NUMBER_INFORMATION "+CNUM"
#define HFP_LIST_CURRENT_CALLS "+CLCC"
#define HFP_RESPONSE_AND_HOLD "+BTRH"

#define HFP_OK "OK"
#define HFP_ERROR "ERROR"
#define HFP_RING "RING"

// Codecs 
#define HFP_CODEC_CVSD    0x01
#define HFP_CODEC_MSBC    0x02
#define HFP_CODEC_LC3_SWB 0x03

typedef enum {
    HFP_ROLE_INVALID = 0,
    HFP_ROLE_AG,
    HFP_ROLE_HF,
} hfp_role_t;

typedef enum {
    HFP_CMD_NONE = 0,
    HFP_CMD_ERROR,
    HFP_CMD_UNKNOWN,
    HFP_CMD_OK,
    HFP_CMD_RING,
    HFP_CMD_SUPPORTED_FEATURES,                             // 5
    HFP_CMD_AVAILABLE_CODECS,
    HFP_CMD_RETRIEVE_AG_INDICATORS_GENERIC,
    HFP_CMD_RETRIEVE_AG_INDICATORS,
    HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS, 
    HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE,                 // 10
    HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE,
    HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES,
    HFP_CMD_ENABLE_CLIP,
    HFP_CMD_AG_SENT_CLIP_INFORMATION,
    HFP_CMD_ENABLE_CALL_WAITING_NOTIFICATION,               // 15
    HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE,
    HFP_CMD_LIST_GENERIC_STATUS_INDICATORS,
    HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS,
    HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE,
    HFP_CMD_SET_GENERIC_STATUS_INDICATOR_STATUS,            // 20
    HFP_CMD_TRANSFER_AG_INDICATOR_STATUS,
    HFP_CMD_QUERY_OPERATOR_SELECTION_NAME,
    HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT,
    HFP_CMD_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR,
    HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR,                   // 25
    HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP,
    HFP_CMD_AG_SEND_COMMON_CODEC,
    HFP_CMD_AG_SUGGESTED_CODEC,
    HFP_CMD_HF_CONFIRMED_CODEC,                             
    HFP_CMD_CALL_ANSWERED,                                  // 30
    HFP_CMD_CALL_HOLD,
    HFP_CMD_HANG_UP_CALL,
    HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING,
    HFP_CMD_CALL_PHONE_NUMBER,
    HFP_CMD_REDIAL_LAST_NUMBER,                             // 35            
    HFP_CMD_TURN_OFF_EC_AND_NR,     
    HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION,                  // 37
    HFP_CMD_HF_ACTIVATE_VOICE_RECOGNITION,
    HFP_CMD_HF_REQUEST_PHONE_NUMBER,
    HFP_CMD_AG_SENT_PHONE_NUMBER,
    HFP_CMD_TRANSMIT_DTMF_CODES,
    HFP_CMD_SET_MICROPHONE_GAIN,
    HFP_CMD_SET_SPEAKER_GAIN,
    HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION,
    HFP_CMD_LIST_CURRENT_CALLS,
    HFP_CMD_RESPONSE_AND_HOLD_QUERY,
    HFP_CMD_RESPONSE_AND_HOLD_COMMAND,
    HFP_CMD_RESPONSE_AND_HOLD_STATUS,
    HFP_CMD_HF_INDICATOR_STATUS,
    HFP_CMD_CUSTOM_MESSAGE
} hfp_command_t;
 

typedef enum {
    HFP_CME_ERROR_AG_FAILURE = 0, 
    HFP_CME_ERROR_NO_CONNECTION_TO_PHONE,
    HFP_CME_ERROR_2,
    HFP_CME_ERROR_OPERATION_NOT_ALLOWED, 
    HFP_CME_ERROR_OPERATION_NOT_SUPPORTED,
    HFP_CME_ERROR_PH_SIM_PIN_REQUIRED,
    HFP_CME_ERROR_6,
    HFP_CME_ERROR_7,
    HFP_CME_ERROR_8,
    HFP_CME_ERROR_9,
    HFP_CME_ERROR_SIM_NOT_INSERTED,
    HFP_CME_ERROR_SIM_PIN_REQUIRED,
    HFP_CME_ERROR_SIM_PUK_REQUIRED,
    HFP_CME_ERROR_SIM_FAILURE,
    HFP_CME_ERROR_SIM_BUSY,
    HFP_CME_ERROR_15,
    HFP_CME_ERROR_INCORRECT_PASSWORD, 
    HFP_CME_ERROR_SIM_PIN2_REQUIRED,
    HFP_CME_ERROR_SIM_PUK2_REQUIRED,
    HFP_CME_ERROR_19,
    HFP_CME_ERROR_MEMORY_FULL,
    HFP_CME_ERROR_INVALID_INDEX,
    HFP_CME_ERROR_22,
    HFP_CME_ERROR_MEMORY_FAILURE,
    HFP_CME_ERROR_TEXT_STRING_TOO_LONG,
    HFP_CME_ERROR_INVALID_CHARACTERS_IN_TEXT_STRING,
    HFP_CME_ERROR_DIAL_STRING_TOO_LONG,
    HFP_CME_ERROR_INVALID_CHARACTERS_IN_DIAL_STRING,
    HFP_CME_ERROR_28,
    HFP_CME_ERROR_29,
    HFP_CME_ERROR_NO_NETWORK_SERVICE,
    HFP_CME_ERROR_NETWORK_TIMEOUT,
    HFP_CME_ERROR_NETWORK_NOT_ALLOWED_EMERGENCY_CALLS_ONLY
} hfp_cme_error_t;

typedef enum {
    HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS = 0,
    HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT
} hfp_call_status_t;

typedef enum {  
    HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS = 0, 
    HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS,
    HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE,
    HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE
} hfp_callsetup_status_t;

typedef enum {
    HFP_CALLHELD_STATUS_NO_CALLS_HELD = 0,
    HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED,
    HFP_CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS 
} hfp_callheld_status_t;


typedef enum {
    HFP_AG_INCOMING_CALL,
    HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG,
    HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF,
    HFP_AG_AUDIO_CONNECTION_ESTABLISHED,
    HFP_AG_OUTGOING_CALL_INITIATED_BY_AG,
    HFP_AG_OUTGOING_CALL_INITIATED_BY_HF,
    HFP_AG_OUTGOING_CALL_REJECTED,
    HFP_AG_OUTGOING_CALL_ACCEPTED,
    HFP_AG_OUTGOING_CALL_RINGING,
    HFP_AG_OUTGOING_CALL_ESTABLISHED,
    HFP_AG_OUTGOING_REDIAL_INITIATED,
    HFP_AG_HELD_CALL_JOINED_BY_AG,
    HFP_AG_TERMINATE_CALL_BY_AG,
    HFP_AG_TERMINATE_CALL_BY_HF,
    HFP_AG_CALL_DROPPED,
    HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG,
    HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG,
    HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG,
    HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF,
    HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF,
    HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF,
    HFP_AG_CALL_HOLD_USER_BUSY,
    HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL,
    HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL,
    HFP_AG_CALL_HOLD_ADD_HELD_CALL,
    HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS,
    HFP_AG_SET_CLIP
} hfp_ag_call_event_t;


typedef enum {
    HFP_PARSER_CMD_HEADER = 0,
    HFP_PARSER_CMD_SEQUENCE,
    HFP_PARSER_SECOND_ITEM,
    HFP_PARSER_THIRD_ITEM,
    HFP_PARSER_CUSTOM_COMMAND
} hfp_parser_state_t;

typedef enum {
    HFP_VOICE_RECOGNITION_STATE_AG_READY = 0,
    HFP_VOICE_RECOGNITION_STATE_AG_READY_TO_ACCEPT_AUDIO_INPUT = 1,
    HFP_VOICE_RECOGNITION_STATE_AG_IS_STARTING_SOUND = 2,
    HFP_VOICE_RECOGNITION_STATE_AG_IS_PROCESSING_AUDIO_INPUT = 4
} hfp_voice_recognition_state_t;

typedef enum {
    HFP_TEXT_TYPE_RECOGNISED_FROM_HF_AUDIO = 0,
    HFP_TEXT_TYPE_MESSAGE_FROM_AG,
    HFP_TEXT_TYPE_QUESTION_FROM_AG,
    HFP_TEXT_TYPE_ERROR_FROM_AG
} hfp_text_type_t;

typedef enum {
    HFP_TEXT_OPERATION_NEW_TEXT = 1,
    HFP_TEXT_OPERATION_REPLACE,
    HFP_TEXT_OPERATION_APPEND
} hfp_text_operation_t;

typedef enum {
    HFP_IDLE = 0, //0
    HFP_SDP_QUERY_RFCOMM_CHANNEL,
    HFP_W2_SEND_SDP_QUERY,
    HFP_W4_SDP_QUERY_COMPLETE,
    HFP_W4_RFCOMM_CONNECTED,
    
    HFP_EXCHANGE_SUPPORTED_FEATURES,   // 5
    HFP_W4_EXCHANGE_SUPPORTED_FEATURES, 
    
    HFP_NOTIFY_ON_CODECS,
    HFP_W4_NOTIFY_ON_CODECS,
    
    HFP_RETRIEVE_INDICATORS, 
    HFP_W4_RETRIEVE_INDICATORS,        // 10
    
    HFP_RETRIEVE_INDICATORS_STATUS, 
    HFP_W4_RETRIEVE_INDICATORS_STATUS,
    
    HFP_ENABLE_INDICATORS_STATUS_UPDATE,
    HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE,
    
    HFP_RETRIEVE_CAN_HOLD_CALL,        // 15
    HFP_W4_RETRIEVE_CAN_HOLD_CALL, 
    
    HFP_LIST_GENERIC_STATUS_INDICATORS,
    HFP_W4_LIST_GENERIC_STATUS_INDICATORS,
    
    HFP_RETRIEVE_GENERIC_STATUS_INDICATORS,
    HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS,  //20
    
    HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS, 
    HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS,
    
    HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED,  //23
    
    HFP_W2_CONNECT_SCO,
    HFP_W4_SCO_CONNECTED,
    
    HFP_AUDIO_CONNECTION_ESTABLISHED, 
    

    HFP_W2_DISCONNECT_SCO,
    HFP_W4_SCO_DISCONNECTED,
    HFP_W4_SCO_DISCONNECTED_TO_SHUTDOWN,
    HFP_W4_WBS_SHUTDOWN,

    HFP_W2_DISCONNECT_RFCOMM,
    HFP_W4_RFCOMM_DISCONNECTED, 
    HFP_W4_RFCOMM_DISCONNECTED_AND_RESTART, 
    HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN
} hfp_state_t;


typedef enum {
    // shared between normal voice recognition and enhanced one
    HFP_VRA_VOICE_RECOGNITION_OFF,

    HFP_VRA_W2_SEND_VOICE_RECOGNITION_OFF,
    HFP_VRA_W4_VOICE_RECOGNITION_OFF,
    
    HFP_VRA_W2_SEND_VOICE_RECOGNITION_ACTIVATED,
    HFP_VRA_W4_VOICE_RECOGNITION_ACTIVATED,
    HFP_VRA_VOICE_RECOGNITION_ACTIVATED,
    
    HFP_VRA_W2_SEND_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO,
    HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO,
    HFP_VRA_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO,

    HFP_VRA_W2_SEND_ENHANCED_VOICE_RECOGNITION_STATUS,
    HFP_VRA_W2_SEND_ENHANCED_VOICE_RECOGNITION_MSG
} hfp_voice_recognition_activation_status_t;

typedef struct {
    uint16_t text_id;
    hfp_text_type_t text_type;
    hfp_text_operation_t text_operation;
    const char * text;
} hfp_voice_recognition_message_t;

typedef enum {
    HFP_CODECS_IDLE,
    HFP_CODECS_RECEIVED_LIST,
    HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE,
    HFP_CODECS_W4_AG_COMMON_CODEC,
    HFP_CODECS_AG_SENT_COMMON_CODEC,
    HFP_CODECS_AG_RESEND_COMMON_CODEC,
    HFP_CODECS_HF_CONFIRMED_CODEC,
    HFP_CODECS_EXCHANGED,
    HFP_CODECS_ERROR
} hfp_codecs_state_t;

typedef enum {
    HFP_CALL_IDLE,
    HFP_CALL_TRIGGER_AUDIO_CONNECTION,
    HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING,
    HFP_CALL_INCOMING_RINGING,
    HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE,
    HFP_CALL_ACTIVE,
    HFP_CALL_W2_SEND_CALL_WAITING,
    HFP_CALL_W4_CHLD,
    HFP_CALL_OUTGOING_INITIATED,
    HFP_CALL_OUTGOING_DIALING,
    HFP_CALL_OUTGOING_RINGING
} hfp_call_state_t;

typedef enum{
    HFP_ENHANCED_CALL_DIR_OUTGOING,
    HFP_ENHANCED_CALL_DIR_INCOMING
} hfp_enhanced_call_dir_t;

typedef enum{
    HFP_ENHANCED_CALL_STATUS_ACTIVE,
    HFP_ENHANCED_CALL_STATUS_HELD,
    HFP_ENHANCED_CALL_STATUS_OUTGOING_DIALING,
    HFP_ENHANCED_CALL_STATUS_OUTGOING_ALERTING,
    HFP_ENHANCED_CALL_STATUS_INCOMING,
    HFP_ENHANCED_CALL_STATUS_INCOMING_WAITING,
    HFP_ENHANCED_CALL_STATUS_CALL_HELD_BY_RESPONSE_AND_HOLD
} hfp_enhanced_call_status_t;

typedef enum{
    HFP_ENHANCED_CALL_MODE_VOICE,
    HFP_ENHANCED_CALL_MODE_DATA,
    HFP_ENHANCED_CALL_MODE_FAX
} hfp_enhanced_call_mode_t;

typedef enum{
    HFP_ENHANCED_CALL_MPTY_NOT_A_CONFERENCE_CALL,
    HFP_ENHANCED_CALL_MPTY_CONFERENCE_CALL
} hfp_enhanced_call_mpty_t;

typedef enum {
    HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD = 0,
    HFP_RESPONSE_AND_HOLD_HELD_INCOMING_ACCEPTED,
    HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED
} hfp_response_and_hold_state_t;

typedef enum {
    HFP_HF_QUERY_OPERATOR_FORMAT_NOT_SET = 0,
    HFP_HF_QUERY_OPERATOR_SET_FORMAT,
    HFP_HF_QUERY_OPERATOR_W4_SET_FORMAT_OK,
    HFP_HF_QUERY_OPERATOR_FORMAT_SET,
    HFP_HF_QUERY_OPERATOR_SEND_QUERY,
    HPF_HF_QUERY_OPERATOR_W4_RESULT
} hfp_hf_query_operator_state_t;

typedef enum {
    HFP_LINK_SETTINGS_D0 = 0,
    HFP_LINK_SETTINGS_D1,
    HFP_LINK_SETTINGS_S1,
    HFP_LINK_SETTINGS_S2,
    HFP_LINK_SETTINGS_S3,
    HFP_LINK_SETTINGS_S4,
    HFP_LINK_SETTINGS_T1,
    HFP_LINK_SETTINGS_T2,
    HFP_LINK_SETTINGS_NONE,
} hfp_link_settings_t;

typedef enum{
    HFP_NONE_SM,
    HFP_SLC_SM,
    HFP_SLC_QUERIES_SM,
    HFP_CODECS_CONNECTION_SM,
    HFP_AUDIO_CONNECTION_SM,
    HFP_CALL_SM
} hfp_state_machine_t;

typedef struct{
    uint16_t uuid;
    uint8_t state; // enabled
} hfp_generic_status_indicator_t;

typedef struct{
    uint8_t index;
    char name[HFP_MAX_INDICATOR_DESC_SIZE];
    uint8_t min_range;
    uint8_t max_range;
    uint8_t status;
    uint8_t mandatory;
    uint8_t enabled;
    uint8_t status_changed;
} hfp_ag_indicator_t;

typedef struct{
    char name[HFP_CALL_SERVICE_SIZE];
} hfp_call_service_t;


typedef struct{
    uint8_t mode;
    uint8_t format;
    char name[HFP_MAX_NETWORK_OPERATOR_NAME_SIZE]; // enabled
} hfp_network_opearator_t;

    
typedef struct hfp_connection {
    btstack_linked_item_t    item;
    
    // local role: HF or AG
    hfp_role_t local_role;

    bd_addr_t remote_addr;
    hci_con_handle_t acl_handle;
    hci_con_handle_t sco_handle;
    uint16_t packet_types;
    uint8_t rfcomm_channel_nr;
    uint16_t rfcomm_cid;
    uint16_t rfcomm_mtu;
    
    hfp_state_machine_t state_machine;
    hfp_call_state_t call_state;
    hfp_state_t state;
    hfp_codecs_state_t codecs_state;
    
    // needed for reestablishing connection - service uuid of the remote
    uint16_t service_uuid;

    // used during service level connection establishment
    hfp_command_t command;
    hfp_parser_state_t parser_state;
    int      parser_item_index;
    int      parser_indicator_index;
    uint32_t parser_indicator_value;
    bool     parser_quoted;

    // line buffer is always \0 terminated
    uint8_t  line_buffer[HFP_MAX_VR_TEXT_SIZE];
    int      line_size;
    
    uint32_t remote_supported_features;

    uint16_t remote_codecs_nr;
    uint8_t  remote_codecs[HFP_MAX_NUM_CODECS];

    uint16_t ag_indicators_nr;
    hfp_ag_indicator_t ag_indicators[HFP_MAX_NUM_INDICATORS];
    uint32_t ag_indicators_status_update_bitmap;
    uint8_t  enable_status_update_for_ag_indicators;

    uint16_t remote_call_services_index;
    hfp_call_service_t remote_call_services[HFP_MAX_NUM_CALL_SERVICES];
    
    // TODO: use bitmap.
    uint16_t generic_status_indicators_nr;
    uint32_t generic_status_update_bitmap;
    hfp_generic_status_indicator_t generic_status_indicators[HFP_MAX_NUM_INDICATORS];

    hfp_network_opearator_t network_operator;
    
    // Retrieved during service level connection establishment, not used yet
    uint8_t  negotiated_codec;

    // HF -> AG configuration
    uint8_t clip_enabled;
    uint8_t call_waiting_notification_enabled;

    // TODO: put these bit flags in a bitmap
    uint8_t ok_pending;
    uint8_t send_error;

    bool found_equal_sign;
    uint8_t ignore_value;

    uint8_t change_status_update_for_individual_ag_indicators; 
    uint8_t operator_name_changed;      

    uint8_t enable_extended_audio_gateway_error_report;
    uint8_t extended_audio_gateway_error_value;
    uint8_t extended_audio_gateway_error;

    // establish codecs connection
    uint8_t suggested_codec;
    uint8_t codec_confirmed;
    uint8_t sco_for_msbc_failed;
    uint8_t trigger_codec_exchange;

    // establish audio connection
    hfp_link_settings_t link_setting;
    uint8_t accept_sco; // 1 = SCO, 2 = eSCO

    uint8_t establish_audio_connection;
    uint8_t release_audio_connection; 
    uint8_t release_slc_connection; 

    uint8_t microphone_gain;
    uint8_t send_microphone_gain;

    uint8_t speaker_gain;
    uint8_t send_speaker_gain;

    uint8_t send_phone_number_for_voice_tag;
    uint8_t send_ag_status_indicators;
    uint8_t send_ag_indicators_segment;
    uint8_t send_response_and_hold_status;  // 0 - don't send. BRTH:0 == 1, ..

    // HF: AT Command, AG: Unsolicited Result Code
    const char * send_custom_message;

    // HF:  Unsolicited Result Code, AG:  AT Command
    uint16_t custom_at_command_id;

    bool emit_vra_enabled_after_audio_established;
    // AG only
    uint8_t change_in_band_ring_tone_setting;
    uint8_t ag_ring;
    uint8_t ag_send_clip;
    uint8_t ag_echo_and_noise_reduction;
    // used by AG: HFP parser stores here the activation value issued by HF
    uint8_t ag_activate_voice_recognition_value;
    bool    ag_audio_connection_opened_before_vra;

    uint8_t ag_notify_incoming_call_waiting;
    uint8_t send_subscriber_number;
    uint8_t next_subscriber_number_to_send;
    uint8_t ag_call_hold_action;
    uint8_t ag_response_and_hold_action;
    uint8_t ag_dtmf_code;
    bool    ag_in_band_ring_tone_active;
    bool    ag_send_no_carrier;
    bool    ag_vra_send_command;
    bool    ag_send_in_band_ring_tone_setting;
    bool    ag_send_common_codec;
    bool    ag_vra_requested_by_hf;

    int send_status_of_current_calls;
    int next_call_index;

    // HF only
    // HF: track command for which ok/error response need to be received
    hfp_command_t response_pending_for_command;

    hfp_hf_query_operator_state_t hf_query_operator_state;
    uint8_t hf_answer_incoming_call;
    uint8_t hf_initiate_outgoing_call;
    uint8_t hf_initiate_memory_dialing;
    uint8_t hf_initiate_redial_last_number;
    bool    hf_send_codec_confirm;
    bool    hf_send_supported_codecs;

    int memory_id;
    
    uint8_t hf_send_clip_enable;
    uint8_t hf_send_chup;
    uint8_t hf_send_chld_0;
    uint8_t hf_send_chld_1;
    uint8_t hf_send_chld_2;
    uint8_t hf_send_chld_3;
    uint8_t hf_send_chld_4;
    uint8_t hf_send_chld_x;
    uint8_t hf_send_chld_x_index;
    char    hf_send_dtmf_code; 
    uint8_t hf_send_binp;
    uint8_t hf_send_clcc;
    uint8_t hf_send_rrh;
    char    hf_send_rrh_command;
    uint8_t hf_send_cnum;

    uint8_t hf_activate_call_waiting_notification;
    uint8_t hf_deactivate_call_waiting_notification;
    
    uint8_t hf_activate_calling_line_notification;
    uint8_t hf_deactivate_calling_line_notification;
    uint8_t hf_deactivate_echo_canceling_and_noise_reduction;

    hfp_voice_recognition_activation_status_t vra_state;
    hfp_voice_recognition_activation_status_t vra_state_requested;
    bool deactivate_voice_recognition;
    bool activate_voice_recognition;
    bool enhanced_voice_recognition_enabled;

    // ih HF, used by parser, in AG used for commands
    uint8_t ag_vra_status;
    hfp_voice_recognition_state_t ag_vra_state;
    hfp_voice_recognition_message_t ag_msg;
    uint16_t ag_vra_msg_length;

    uint8_t clcc_idx;
    uint8_t clcc_dir;
    uint8_t clcc_status;
    uint8_t clcc_mode;
    uint8_t clcc_mpty;

    uint8_t call_index;
    // also used for CLCC, CCWA, CLIP if set
    uint8_t bnip_type;       // 0 == not set
    char    bnip_number[HFP_BNEP_NUM_MAX_SIZE]; //
    bool    clip_have_alpha;

#ifdef ENABLE_CC256X_ASSISTED_HFP
    bool cc256x_send_write_codec_config;
    bool cc256x_send_wbs_associate;
    bool cc256x_send_wbs_disassociate;
#endif
#ifdef ENABLE_BCM_PCM_WBS
    bool bcm_send_enable_wbs;
    bool bcm_send_disable_wbs;
    bool bcm_send_write_i2spcm_interface_param;
#endif
#ifdef ENABLE_RTK_PCM_WBS
    bool rtk_send_sco_config;
#endif
} hfp_connection_t;

/**
 * @brief Struct to register custom AT Command.
 */
typedef struct {
    btstack_linked_item_t * next;
    const char *            command;
    uint16_t                command_id;
} hfp_custom_at_command_t;

// UTILS_START : TODO move to utils
int send_str_over_rfcomm(uint16_t cid, const char * command);
int join(char * buffer, int buffer_size, uint8_t * values, int values_nr);
int join_bitmap(char * buffer, int buffer_size, uint32_t values, int values_nr);
int get_bit(uint16_t bitmap, int position);
int store_bit(uint32_t bitmap, int position, uint8_t value);
// UTILS_END

#define HFP_H2_SYNC_FRAME_SIZE 60
// HFP H2 Synchronization
typedef struct {
    // callback returns true if data was valid
    bool        (*callback)(bool bad_frame, const uint8_t * frame_data, uint16_t frame_len);
    uint8_t     frame_data[HFP_H2_SYNC_FRAME_SIZE];
    uint16_t    frame_len;
    uint16_t    dropped_bytes;
} hfp_h2_sync_t;

/**
 * @brief Init HFP H2 Sync state
 * @param hfp_h2_sync
 * @param callback
 */
void hfp_h2_sync_init(hfp_h2_sync_t * hfp_h2_sync,
                      bool (*callback)(bool bad_frame, const uint8_t * frame_data, uint16_t frame_len));
/**
 * @brief Process H2 data and execute callback for frames with valid H2 header
 * @param hfp_h2_sync
 * @param bad_frame
 * @param frame_data
 * @param frame_len
 */
void hfp_h2_sync_process(hfp_h2_sync_t *hfp_h2_sync, bool bad_frame, const uint8_t *frame_data, uint16_t frame_len);


// other

void hfp_finalize_connection_context(hfp_connection_t * hfp_connection);
void hfp_emit_sco_connection_established(hfp_connection_t *hfp_connection, uint8_t status, uint8_t negotiated_codec,
                                         uint16_t rx_packet_length, uint16_t tx_packet_length);

void hfp_set_ag_callback(btstack_packet_handler_t callback);
void hfp_set_ag_rfcomm_packet_handler(btstack_packet_handler_t handler);

void hfp_set_hf_callback(btstack_packet_handler_t callback);
void hfp_set_hf_rfcomm_packet_handler(btstack_packet_handler_t handler);

void hfp_init(void);
void hfp_deinit(void);

void hfp_register_custom_ag_command(hfp_custom_at_command_t * at_command);
void hfp_register_custom_hf_command(hfp_custom_at_command_t * at_command);

void hfp_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t service_uuid, int rfcomm_channel_nr, const char * name);
void hfp_handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, hfp_role_t local_role);
void hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, hfp_role_t local_role);
void hfp_emit_event(hfp_connection_t * hfp_connection, uint8_t event_subtype, uint8_t value);
void hfp_emit_simple_event(hfp_connection_t * hfp_connection, uint8_t event_subtype);
void hfp_emit_string_event(hfp_connection_t * hfp_connection, uint8_t event_subtype, const char * value);
void hfp_emit_slc_connection_event(hfp_role_t local_role, uint8_t status, hci_con_handle_t con_handle, bd_addr_t addr);

/**
 * @brief Emit HFP_SUBEVENT_VOICE_RECOGNITION_ENABLED event
 * @param hfp_connection
 * @param status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_COMMAND_DISALLOWED
 */
void hfp_emit_voice_recognition_enabled(hfp_connection_t * hfp_connection, uint8_t status);

/**
 * @brief Emit HFP_SUBEVENT_VOICE_RECOGNITION_DISABLED event
 * @param hfp_connection
 * @param status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_COMMAND_DISALLOWED
 */
void hfp_emit_voice_recognition_disabled(hfp_connection_t * hfp_connection, uint8_t status);

void hfp_emit_enhanced_voice_recognition_hf_ready_for_audio_event(hfp_connection_t * hfp_connection, uint8_t status);
void hfp_emit_enhanced_voice_recognition_state_event(hfp_connection_t * hfp_connection, uint8_t status);

hfp_connection_t * get_hfp_connection_context_for_rfcomm_cid(uint16_t cid);
hfp_connection_t * get_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr, hfp_role_t hfp_role);
hfp_connection_t * get_hfp_connection_context_for_sco_handle(uint16_t handle, hfp_role_t hfp_role);
hfp_connection_t * get_hfp_connection_context_for_acl_handle(uint16_t handle, hfp_role_t hfp_role);

btstack_linked_list_t * hfp_get_connections(void);
void hfp_parse(hfp_connection_t * connection, uint8_t byte, int isHandsFree);
void hfp_parser_reset_line_buffer(hfp_connection_t *hfp_connection);

/**
 * @brief Establish RFCOMM connection, and perform service level connection agreement:
 * @param bd_addr
 * @param service_uuid
 * @param local_role
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *                  - ERROR_CODE_COMMAND_DISALLOWED if connection already exists, or
 *                  - BTSTACK_MEMORY_ALLOC_FAILED 
 */
uint8_t hfp_establish_service_level_connection(bd_addr_t bd_addr, uint16_t service_uuid, hfp_role_t local_role);

/**
 * @brief Prepare connection for audio and SLC connections release
 * @param hfp_connection
 */
void hfp_trigger_release_service_level_connection(hfp_connection_t * hfp_connection);

/**
 * @brief Prepare connection for audio connection release
 * @param hfp_connection
 */
uint8_t hfp_trigger_release_audio_connection(hfp_connection_t * hfp_connection);

void hfp_reset_context_flags(hfp_connection_t * hfp_connection);

void hfp_setup_synchronous_connection(hfp_connection_t * hfp_connection);
void hfp_accept_synchronous_connection(hfp_connection_t * hfp_connection, bool incoming_eSCO);
int hfp_supports_codec(uint8_t codec, int codecs_nr, uint8_t * codecs);
void hfp_hf_drop_mSBC_if_eSCO_not_supported(uint8_t * codecs, uint8_t * codecs_nr);
void hfp_init_link_settings(hfp_connection_t * hfp_connection, uint8_t eSCO_S4_supported);
hfp_link_settings_t hfp_next_link_setting(hfp_link_settings_t current_setting, uint16_t local_sco_packet_types,
                                          uint16_t remote_sco_packet_types, bool eSCO_s4_supported,
                                          uint8_t negotiated_codec);

const char * hfp_hf_feature(int index);
const char * hfp_ag_feature(int index);

void hfp_log_rfcomm_message(const char * tag, uint8_t * packet, uint16_t size);

const char * hfp_enhanced_call_dir2str(uint16_t index);
const char * hfp_enhanced_call_status2str(uint16_t index);
const char * hfp_enhanced_call_mode2str(uint16_t index);
const char * hfp_enhanced_call_mpty2str(uint16_t index);

#ifdef ENABLE_CC256X_ASSISTED_HFP
void hfp_cc256x_prepare_for_sco(hfp_connection_t * hfp_connection);
void hfp_cc256x_write_codec_config(hfp_connection_t * hfp_connection);
#endif
#ifdef ENABLE_BCM_PCM_WBS
void hfp_bcm_prepare_for_sco(hfp_connection_t * hfp_connection);
void hfp_bcm_write_i2spcm_interface_param (hfp_connection_t * hfp_connection);
#endif

/**
 * @brief Set packet types for SCO connections
 * @param common single packet_types: HFP_SCO_PACKET_TYPES_*
 */
void hfp_set_sco_packet_types(uint16_t packet_types);

/**
 * @brief Get packet types for SCO connections
 * @return packet_types
 */
uint16_t hfp_get_sco_packet_types(void);

#if defined __cplusplus
}
#endif

#endif // BTSTACK_HFP_H
