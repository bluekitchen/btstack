/*
 * Copyright (C) 2016 BlueKitchen GmbH
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
 * Audio/Video Distribution Transport Protocol (AVDTP)
 *
 * This protocol defines A/V stream negotiation, establishment, and transmission 
 * procedures. Also specified are the message formats that are exchanged between 
 * such devices to transport their A/V streams in A/V distribution applications.
 *
 * Media packets are unidirectional, they travel downstream from AVDTP Source to AVDTP Sink. 
 */

#ifndef AVDTP_H
#define AVDTP_H

#include <stdint.h>
#include "hci.h"
#include "btstack_ring_buffer.h"

#if defined __cplusplus
extern "C" {
#endif

// Limit L2CAP MTU to fit largest packet for BR/EDR
#define AVDTP_L2CAP_MTU (HCI_ACL_3DH5_SIZE - L2CAP_HEADER_SIZE)

#define AVDTP_MAX_NUM_SEPS 10
#define AVDTP_MAX_CSRC_NUM 15
#define AVDTP_MAX_CONTENT_PROTECTION_TYPE_VALUE_LEN 10

// Supported Features
#define AVDTP_SOURCE_FEATURE_MASK_PLAYER        0x0001u
#define AVDTP_SOURCE_FEATURE_MASK_MICROPHONE    0x0002u
#define AVDTP_SOURCE_FEATURE_MASK_TUNER         0x0004u
#define AVDTP_SOURCE_FEATURE_MASK_MIXER         0x0008u

#define AVDTP_SINK_FEATURE_MASK_HEADPHONE       0x0001u
#define AVDTP_SINK_FEATURE_MASK_SPEAKER         0x0002u
#define AVDTP_SINK_FEATURE_MASK_RECORDER        0x0004u
#define AVDTP_SINK_FEATURE_MASK_AMPLIFIER       0x0008u

// ACP to INT, Signal Response Header Error Codes
#define AVDTP_ERROR_CODE_BAD_HEADER_FORMAT     0x01

// ACP to INT, Signal Response Payload Format Error Codes
#define AVDTP_ERROR_CODE_BAD_LENGTH                 0x11
#define AVDTP_ERROR_CODE_BAD_ACP_SEID               0x12
#define AVDTP_ERROR_CODE_SEP_IN_USE                 0x13
#define AVDTP_ERROR_CODE_SEP_NOT_IN_USE             0x14
#define AVDTP_ERROR_CODE_BAD_SERV_CATEGORY          0x17
#define AVDTP_ERROR_CODE_BAD_PAYLOAD_FORMAT         0x18
#define AVDTP_ERROR_CODE_NOT_SUPPORTED_COMMAND      0x19
#define AVDTP_ERROR_CODE_INVALID_CAPABILITIES       0x1A

// ACP to INT, Signal Response Transport Service Capabilities Error Codes
#define AVDTP_ERROR_CODE_BAD_RECOVERY_TYPE          0x22
#define AVDTP_ERROR_CODE_BAD_MEDIA_TRANSPORT_FORMAT 0x23
#define AVDTP_ERROR_CODE_BAD_RECOVERY_FORMAT        0x25
#define AVDTP_ERROR_CODE_BAD_ROHC_FORMAT            0x26
#define AVDTP_ERROR_CODE_BAD_CP_FORMAT              0x27
#define AVDTP_ERROR_CODE_BAD_MULTIPLEXING_FORMAT    0x28
#define AVDTP_ERROR_CODE_UNSUPPORTED_CONFIGURATION  0x29

// ACP to INT, Procedure Error Codes
#define AVDTP_ERROR_CODE_BAD_STATE                  0x31

// Internal Error Codes
#define AVDTP_INVALID_SEP_SEID                      0xFF


// Signal Identifier fields
typedef enum {
    AVDTP_SI_NONE = 0x00,
    AVDTP_SI_DISCOVER = 0x01,
    AVDTP_SI_GET_CAPABILITIES,
    AVDTP_SI_SET_CONFIGURATION,
    AVDTP_SI_GET_CONFIGURATION,
    AVDTP_SI_RECONFIGURE, //5
    AVDTP_SI_OPEN,  //6
    AVDTP_SI_START, //7
    AVDTP_SI_CLOSE,
    AVDTP_SI_SUSPEND,
    AVDTP_SI_ABORT, //10
    AVDTP_SI_SECURITY_CONTROL,
    AVDTP_SI_GET_ALL_CAPABILITIES, //12
    AVDTP_SI_DELAYREPORT,
#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
    AVDTP_SI_ACCEPT_START
#endif
} avdtp_signal_identifier_t;

typedef enum {
    AVDTP_SINGLE_PACKET = 0,
    AVDTP_START_PACKET    ,
    AVDTP_CONTINUE_PACKET ,
    AVDTP_END_PACKET
} avdtp_packet_type_t;

typedef enum {
    AVDTP_CMD_MSG = 0,
    AVDTP_GENERAL_REJECT_MSG   ,
    AVDTP_RESPONSE_ACCEPT_MSG ,
    AVDTP_RESPONSE_REJECT_MSG
} avdtp_message_type_t;

typedef enum {
    AVDTP_AUDIO = 0,
    AVDTP_VIDEO,
    AVDTP_MULTIMEDIA 
} avdtp_media_type_t;

typedef enum {
    AVDTP_CODEC_SBC             = 0x00,
    AVDTP_CODEC_MPEG_1_2_AUDIO  = 0x01, 
    AVDTP_CODEC_MPEG_2_4_AAC    = 0x02,
    AVDTP_CODEC_ATRAC_FAMILY    = 0x04,
    AVDTP_CODEC_NON_A2DP        = 0xFF
} avdtp_media_codec_type_t;

typedef enum {
    AVDTP_CONTENT_PROTECTION_DTCP = 0x0001,
    AVDTP_CONTENT_PROTECTION_SCMS_T = 0x0002
} avdtp_content_protection_type_t;

typedef enum {
    AVDTP_SOURCE = 0,
    AVDTP_SINK
} avdtp_sep_type_t;

typedef enum {
    AVDTP_ROLE_SOURCE = 0,
    AVDTP_ROLE_SINK
} avdtp_role_t;

typedef enum {
    AVDTP_SERVICE_CATEGORY_INVALID_0 = 0x00,
    AVDTP_MEDIA_TRANSPORT = 0X01,
    AVDTP_REPORTING,
    AVDTP_RECOVERY,
    AVDTP_CONTENT_PROTECTION, //4
    AVDTP_HEADER_COMPRESSION, //5
    AVDTP_MULTIPLEXING,       //6
    AVDTP_MEDIA_CODEC,        //7
    AVDTP_DELAY_REPORTING,    //8
    AVDTP_SERVICE_CATEGORY_INVALID_FF = 0xFF
} avdtp_service_category_t;

typedef struct {
    uint8_t recovery_type;                  // 0x01 = RFC2733
    uint8_t maximum_recovery_window_size;   // 0x01 to 0x18, for a Transport Packet
    uint8_t maximum_number_media_packets;   // 0x01 to 0x18, The maximum number of media packets a specific parity code covers
} avdtp_recovery_capabilities_t;            

typedef struct {
    avdtp_media_type_t       media_type;                     
    avdtp_media_codec_type_t media_codec_type; 
    uint16_t  media_codec_information_len;
    uint8_t * media_codec_information;
} adtvp_media_codec_capabilities_t;


typedef struct {
    uint16_t cp_type;
    uint16_t cp_type_value_len;
    uint8_t cp_type_value[AVDTP_MAX_CONTENT_PROTECTION_TYPE_VALUE_LEN];
} adtvp_content_protection_t;

typedef struct{
    uint8_t back_ch;  // byte0 - bit 8; 0=Not Available/Not Used; 1=Available/In Use
    uint8_t media;    // byte0 - bit 7
    uint8_t recovery; // byte0 - bit 6
} avdtp_header_compression_capabilities_t;

typedef struct{
    uint8_t fragmentation; // byte0 - bit 8, Allow Adaptation Layer Fragmentation, 0 no, 1 yes
    // Request/indicate value of the Transport Session Identifier for a media, reporting, or recovery transport sessions, respectively
    uint8_t transport_identifiers_num;
    uint8_t transport_session_identifiers[3];   // byte1, upper 5bits, 0x01 to 0x1E
    // Request/indicate value for TCID for a media, reporting, or transport session
    uint8_t tcid[3];         // byte2 0x01 to 0x1E 
} avdtp_multiplexing_mode_capabilities_t;

typedef struct{
    avdtp_recovery_capabilities_t recovery;
    adtvp_media_codec_capabilities_t media_codec;
    adtvp_content_protection_t content_protection;
    avdtp_header_compression_capabilities_t header_compression;
    avdtp_multiplexing_mode_capabilities_t multiplexing_mode;
} avdtp_capabilities_t;

typedef enum{
    AVDTP_SBC_48000 = 1,
    AVDTP_SBC_44100 = 2,
    AVDTP_SBC_32000 = 4,
    AVDTP_SBC_16000 = 8
} avdtp_sbc_sampling_frequency_t;

typedef enum{
    AVDTP_SBC_JOINT_STEREO  = 1,
    AVDTP_SBC_STEREO        = 2,
    AVDTP_SBC_DUAL_CHANNEL  = 4,
    AVDTP_SBC_MONO          = 8
} avdtp_sbc_channel_mode_t;

typedef enum{
    AVDTP_SBC_BLOCK_LENGTH_16 = 1,
    AVDTP_SBC_BLOCK_LENGTH_12 = 2,
    AVDTP_SBC_BLOCK_LENGTH_8  = 4,
    AVDTP_SBC_BLOCK_LENGTH_4  = 8
} avdtp_sbc_block_length_t;

typedef enum{
    AVDTP_SBC_SUBBANDS_8 = 1,
    AVDTP_SBC_SUBBANDS_4 = 2
} avdtp_sbc_subbands_t;

typedef enum{
    AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS = 1,
    AVDTP_SBC_ALLOCATION_METHOD_SNR      = 2
} avdtp_sbc_allocation_method_t;

typedef struct {
    uint8_t fragmentation;
    uint8_t starting_packet; // of fragmented SBC frame
    uint8_t last_packet;     // of fragmented SBC frame
    uint8_t num_frames;
} avdtp_sbc_codec_header_t;

typedef enum {
    AVDTP_MPEG_LAYER_1 = 1,
    AVDTP_MPEG_LAYER_2,
    AVDTP_MPEG_LAYER_3,
} avdtp_mpeg_layer_t;


typedef enum {
    AVDTP_AAC_MPEG2_LC = 1,
    AVDTP_AAC_MPEG4_LC,
    AVDTP_AAC_MPEG4_LTP,
    AVDTP_AAC_MPEG4_SCALABLE
} avdtp_aac_object_type_t;

typedef enum {
    AVDTP_ATRAC_VERSION_1 = 1,
    AVDTP_ATRAC_VERSION_2,
    AVDTP_ATRAC_VERSION_3
} avdtp_atrac_version_t;

// used for MPEG1/2 Audio, ATRAC (no stereo mode)
typedef enum {
    AVDTP_CHANNEL_MODE_MONO = 1,
    AVDTP_CHANNEL_MODE_DUAL_CHANNEL,
    AVDTP_CHANNEL_MODE_STEREO,
    AVDTP_CHANNEL_MODE_JOINT_STEREO,
} avdtp_channel_mode_t;

typedef struct {
    uint16_t                        sampling_frequency;
    avdtp_channel_mode_t            channel_mode;
    uint8_t                         block_length;
    uint8_t                         subbands;
    avdtp_sbc_allocation_method_t   allocation_method;
    uint8_t                         min_bitpool_value;
    uint8_t                         max_bitpool_value;
} avdtp_configuration_sbc_t;

typedef struct {
    avdtp_mpeg_layer_t      layer;
    uint8_t                 crc;
    avdtp_channel_mode_t    channel_mode;
    uint8_t                 media_payload_format;
    uint16_t                sampling_frequency;
    uint8_t                 vbr;
    uint8_t                 bit_rate_index;
} avdtp_configuration_mpeg_audio_t;

typedef struct {
    avdtp_aac_object_type_t object_type;
    uint32_t                sampling_frequency;
    uint8_t                 channels;
    uint32_t                bit_rate;
    uint8_t                 vbr;
} avdtp_configuration_mpeg_aac_t;

typedef struct {
    avdtp_atrac_version_t   version;
    avdtp_channel_mode_t    channel_mode;
    uint16_t                sampling_frequency;
    uint8_t                 vbr;
    uint8_t                 bit_rate_index;
    uint16_t                maximum_sul;
} avdtp_configuration_atrac_t;



typedef struct {
    uint8_t version;
    uint8_t padding;
    uint8_t extension;
    uint8_t csrc_count;
    uint8_t marker;
    uint8_t payload_type;

    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t synchronization_source;

    uint32_t csrc_list[AVDTP_MAX_CSRC_NUM];
} avdtp_media_packet_header_t;

typedef enum {
    AVDTP_BASIC_SERVICE_MODE = 0,
    AVDTP_MULTIPLEXING_SERVICE_MODE
} avdtp_service_mode_t;

typedef enum {
    AVDTP_STREAM_ENDPOINT_IDLE = 0,
    AVDTP_STREAM_ENDPOINT_CONFIGURATION_SUBSTATEMACHINE,
    AVDTP_STREAM_ENDPOINT_CONFIGURED,

    AVDTP_STREAM_ENDPOINT_W2_REQUEST_OPEN_STREAM,
    AVDTP_STREAM_ENDPOINT_W4_ACCEPT_OPEN_STREAM,
    AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_CONNECTED,

    AVDTP_STREAM_ENDPOINT_OPENED, 
    AVDTP_STREAM_ENDPOINT_STREAMING, 

    AVDTP_STREAM_ENDPOINT_CLOSING,
    AVDTP_STREAM_ENDPOINT_ABORTING,
    AVDTP_STREAM_ENDPOINT_W4_L2CAP_FOR_MEDIA_DISCONNECTED
} avdtp_stream_endpoint_state_t;

typedef enum {
    AVDTP_INITIATOR_STREAM_CONFIG_IDLE = 0,
    AVDTP_INITIATOR_W2_SET_CONFIGURATION,
    AVDTP_INITIATOR_W2_SUSPEND_STREAM_WITH_SEID,
    AVDTP_INITIATOR_W2_RECONFIGURE_STREAM_WITH_SEID,
    
    AVDTP_INITIATOR_W2_OPEN_STREAM,

    AVDTP_INITIATOR_W2_STREAMING_ABORT,
    AVDTP_INITIATOR_FRAGMENTATED_COMMAND,
    AVDTP_INITIATOR_W4_ANSWER
} avdtp_initiator_stream_endpoint_state_t;

typedef enum {
    AVDTP_ACCEPTOR_STREAM_CONFIG_IDLE = 0,
    AVDTP_ACCEPTOR_W2_ACCEPT_GET_CAPABILITIES,
    AVDTP_ACCEPTOR_W2_ACCEPT_GET_ALL_CAPABILITIES,
    AVDTP_ACCEPTOR_W2_ACCEPT_DELAY_REPORT,
    AVDTP_ACCEPTOR_W2_ACCEPT_SET_CONFIGURATION,
    AVDTP_ACCEPTOR_W2_ACCEPT_RECONFIGURE,
    AVDTP_ACCEPTOR_W2_ACCEPT_GET_CONFIGURATION,
    AVDTP_ACCEPTOR_W2_ACCEPT_OPEN_STREAM,
#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
    AVDTP_ACCEPTOR_W4_USER_CONFIRM_START_STREAM,
    AVDTP_ACCEPTOR_W2_REJECT_START_STREAM,
#endif
    AVDTP_ACCEPTOR_W2_ACCEPT_START_STREAM,
    AVDTP_ACCEPTOR_W2_ACCEPT_CLOSE_STREAM,
    AVDTP_ACCEPTOR_W2_ACCEPT_ABORT_STREAM,
    AVDTP_ACCEPTOR_W2_ACCEPT_SUSPEND_STREAM,
    AVDTP_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE,
    AVDTP_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE,
    AVDTP_ACCEPTOR_W2_REJECT_UNKNOWN_CMD
} avdtp_acceptor_stream_endpoint_state_t;

typedef struct {
    uint8_t seid;           // 0x01 – 0x3E, 6bit
    uint8_t in_use;         // 1 bit, 0 - not in use, 1 - in use
    avdtp_media_type_t media_type;     // 4 bit
    avdtp_sep_type_t   type;       // 1 bit, 0 - SRC, 1 - SNK

    uint16_t registered_service_categories;
    avdtp_capabilities_t capabilities;

    uint16_t configured_service_categories;
    avdtp_capabilities_t configuration;
} avdtp_sep_t;


typedef enum {
    AVDTP_SIGNALING_CONNECTION_IDLE = 0,
    AVDTP_SIGNALING_W2_SEND_SDP_QUERY_FOR_REMOTE_SINK,
    AVDTP_SIGNALING_W4_SDP_QUERY_FOR_REMOTE_SINK_COMPLETE,
    AVDTP_SIGNALING_W2_SEND_SDP_QUERY_FOR_REMOTE_SOURCE,
    AVDTP_SIGNALING_W4_SDP_QUERY_FOR_REMOTE_SOURCE_COMPLETE,
    AVDTP_SIGNALING_CONNECTION_W4_L2CAP_CONNECTED,
    AVDTP_SIGNALING_CONNECTION_W2_L2CAP_RETRY,
    AVDTP_SIGNALING_CONNECTION_OPENED,
    AVDTP_SIGNALING_CONNECTION_W4_L2CAP_DISCONNECTED
} avdtp_connection_state_t;

typedef enum {
    AVDTP_SIGNALING_CONNECTION_ACCEPTOR_IDLE = 0,
    AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_ANSWER_DISCOVER_SEPS,
    AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_WITH_ERROR_CODE,
    AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_REJECT_CATEGORY_WITH_ERROR_CODE,
    AVDTP_SIGNALING_CONNECTION_ACCEPTOR_W2_GENERAL_REJECT_WITH_ERROR_CODE
} avdtp_acceptor_connection_state_t;

typedef enum {
    AVDTP_SIGNALING_CONNECTION_INITIATOR_IDLE = 0,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_DISCOVER_SEPS,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SEND_SDP_QUERY_THEN_GET_ALL_CAPABILITIES_FROM_REMOTE_SINK,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SEND_SDP_QUERY_THEN_GET_ALL_CAPABILITIES_FROM_REMOTE_SOURCE,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_SDP_QUERY_COMPLETE_THEN_GET_ALL_CAPABILITIES,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CAPABILITIES,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_ALL_CAPABILITIES,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_GET_CONFIGURATION,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W2_SEND_DELAY_REPORT,
    AVDTP_SIGNALING_CONNECTION_INITIATOR_W4_ANSWER
} avdtp_initiator_connection_state_t;

typedef struct {
    uint8_t  command[200];
    uint16_t size;
    uint16_t offset;
    uint8_t  acp_seid;
    uint8_t  int_seid;
    uint8_t transaction_label;
    uint16_t num_packets;
    avdtp_signal_identifier_t   signal_identifier;
    avdtp_message_type_t        message_type;
    avdtp_packet_type_t         packet_type;
} avdtp_signaling_packet_t;

typedef enum {
    AVDTP_CONFIGURATION_STATE_IDLE = 0,
    AVDTP_CONFIGURATION_STATE_LOCAL_INITIATED,
    AVDTP_CONFIGURATION_STATE_LOCAL_CONFIGURED,
    AVDTP_CONFIGURATION_STATE_REMOTE_INITIATED,
    AVDTP_CONFIGURATION_STATE_REMOTE_CONFIGURED,
} avtdp_configuration_state_t;

typedef enum {
    A2DP_IDLE = 0,
    A2DP_W4_CONNECTED,
    A2DP_CONNECTED,
    A2DP_DISCOVER_SEPS,
    A2DP_GET_CAPABILITIES,
    A2DP_W2_GET_ALL_CAPABILITIES, //5
    A2DP_DISCOVERY_DONE,
    A2DP_SET_CONFIGURATION,      
    A2DP_W4_GET_CONFIGURATION,
    A2DP_W4_SET_CONFIGURATION,
    A2DP_CONFIGURED,
    A2DP_W2_SUSPEND_STREAM_WITH_SEID, //10
    A2DP_W2_RECONFIGURE_WITH_SEID,
    A2DP_W2_OPEN_STREAM_WITH_SEID,   
    A2DP_W4_OPEN_STREAM_WITH_SEID,
    A2DP_W2_START_STREAM_WITH_SEID,  
    A2DP_W2_ABORT_STREAM_WITH_SEID,   //15
    A2DP_W2_STOP_STREAM_WITH_SEID,
    A2DP_STREAMING_OPENED
} a2dp_state_t;

typedef struct {
    bool         discover_seps;
    bool         outgoing_active;
    bool         have_config;
    bool         stream_endpoint_configured;
    a2dp_state_t state;
    struct avdtp_stream_endpoint * local_stream_endpoint;
} a2dp_config_process_t;

typedef struct {
    btstack_linked_item_t    item;
    bd_addr_t remote_addr;
    
    uint16_t avdtp_cid;
    hci_con_handle_t con_handle;
    
    // SDP results
    uint16_t avdtp_l2cap_psm;
    uint16_t avdtp_version;
    bool     sink_supported;
    bool     source_supported;

    uint16_t l2cap_signaling_cid;
    uint16_t l2cap_mtu;
    
    avdtp_connection_state_t state;
    avdtp_acceptor_connection_state_t  acceptor_connection_state;
    avdtp_initiator_connection_state_t initiator_connection_state;

    // used to reassemble fragmented commands
    avdtp_signaling_packet_t acceptor_signaling_packet;

    // used to prepare outgoing signaling packets
    avdtp_signaling_packet_t initiator_signaling_packet;

    uint8_t initiator_local_seid;
    uint8_t initiator_remote_seid;

    uint8_t acceptor_local_seid;

    uint16_t delay_ms;

    // for repeating the set_configuration 
    void * active_stream_endpoint;

    uint8_t initiator_transaction_label;
    uint8_t acceptor_transaction_label;
    bool    wait_to_send_acceptor;
	bool    wait_to_send_initiator;

    uint8_t suspended_seids[AVDTP_MAX_NUM_SEPS];
    uint8_t num_suspended_seids;

    uint8_t reject_service_category;
    avdtp_signal_identifier_t reject_signal_identifier;
    uint8_t error_code;

    // configuration state machine
    avtdp_configuration_state_t configuration_state;

    bool incoming_declined;
    btstack_timer_source_t retry_timer;

    // A2DP SOURCE
    a2dp_config_process_t a2dp_source_config_process;

    // A2DP SINK
    a2dp_config_process_t a2dp_sink_config_process;

} avdtp_connection_t;


typedef struct avdtp_stream_endpoint {
    btstack_linked_item_t    item;
    
    // original capabilities configured via avdtp_register_x_category
    avdtp_sep_t sep;

    // media codec configuration - provided by user
    uint16_t  media_codec_configuration_len;
    uint8_t * media_codec_configuration_info;

    avdtp_sep_t remote_sep;
    hci_con_handle_t media_con_handle;
    uint16_t l2cap_media_cid;
    uint16_t l2cap_reporting_cid;
    uint16_t l2cap_recovery_cid;

    avdtp_stream_endpoint_state_t  state;
    avdtp_acceptor_stream_endpoint_state_t  acceptor_config_state;
    avdtp_initiator_stream_endpoint_state_t initiator_config_state;
    a2dp_state_t a2dp_state;
    // active connection
    avdtp_connection_t * connection;
    
    // currently active remote seid
    avdtp_capabilities_t remote_capabilities;
    uint16_t remote_capabilities_bitmap;
    
    uint16_t remote_configuration_bitmap;
    avdtp_capabilities_t remote_configuration;  

    // temporary codec config used by A2DP Source
    uint8_t set_config_remote_seid;
    avdtp_media_codec_type_t media_codec_type;
    uint8_t media_codec_info[8];

    // preferred SBC codec settings
    uint32_t preferred_sampling_frequency; 
    uint8_t  preferred_channel_mode;

    // register request for media L2cap connection release
    uint8_t media_disconnect;
    uint8_t media_connect;
    uint8_t start_stream;
    uint8_t close_stream;
    bool  request_can_send_now;
    uint8_t abort_stream;
    uint8_t suspend_stream;
    uint16_t sequence_number;
} avdtp_stream_endpoint_t;

void avdtp_init(void);
void avdtp_deinit(void);

avdtp_connection_t * avdtp_get_connection_for_bd_addr(bd_addr_t addr);
avdtp_connection_t * avdtp_get_connection_for_avdtp_cid(uint16_t avdtp_cid);
avdtp_connection_t * avdtp_get_connection_for_l2cap_signaling_cid(uint16_t l2cap_cid);
btstack_linked_list_t * avdtp_get_connections(void);
btstack_linked_list_t * avdtp_get_stream_endpoints(void);

avdtp_stream_endpoint_t * avdtp_get_stream_endpoint_for_seid(uint16_t seid);
avdtp_stream_endpoint_t * avdtp_get_source_stream_endpoint_for_media_codec(avdtp_media_codec_type_t codec_type);
avdtp_stream_endpoint_t * avdtp_get_source_stream_endpoint_for_media_codec_other(uint32_t vendor_id, uint16_t codec_id);
avdtp_stream_endpoint_t * avdtp_get_source_stream_endpoint_for_media_codec_and_type(avdtp_media_codec_type_t codec_type, avdtp_sep_type_t sep_type);

btstack_packet_handler_t avdtp_packet_handler_for_stream_endpoint(const avdtp_stream_endpoint_t *stream_endpoint);
void avdtp_emit_sink_and_source(uint8_t * packet, uint16_t size);
void avdtp_emit_source(uint8_t * packet, uint16_t size);

void avdtp_register_media_transport_category(avdtp_stream_endpoint_t * stream_endpoint);
void avdtp_register_reporting_category(avdtp_stream_endpoint_t * stream_endpoint);
void avdtp_register_delay_reporting_category(avdtp_stream_endpoint_t * stream_endpoint);
void avdtp_register_recovery_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t maximum_recovery_window_size, uint8_t maximum_number_media_packets);
void avdtp_register_content_protection_category(avdtp_stream_endpoint_t * stream_endpoint, uint16_t cp_type, const uint8_t * cp_type_value, uint8_t cp_type_value_len);
void avdtp_register_header_compression_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t back_ch, uint8_t media, uint8_t recovery);
void avdtp_register_media_codec_category(avdtp_stream_endpoint_t * stream_endpoint, avdtp_media_type_t media_type, avdtp_media_codec_type_t media_codec_type, const uint8_t *media_codec_info, uint16_t media_codec_info_len);
void avdtp_register_multiplexing_category(avdtp_stream_endpoint_t * stream_endpoint, uint8_t fragmentation);

// sink only
void avdtp_register_media_handler(void (*callback)(uint8_t local_seid, uint8_t *packet, uint16_t size));

void avdtp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
avdtp_stream_endpoint_t * avdtp_create_stream_endpoint(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type);
void avdtp_finalize_stream_endpoint(avdtp_stream_endpoint_t * stream_endpoint);

uint8_t avdtp_connect(bd_addr_t remote, avdtp_role_t role, uint16_t * avdtp_cid);
uint8_t avdtp_disconnect(uint16_t avdtp_cid);
void    avdtp_register_sink_packet_handler(btstack_packet_handler_t callback);
void    avdtp_register_source_packet_handler(btstack_packet_handler_t callback);

uint8_t avdtp_open_stream(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid);
uint8_t avdtp_start_stream(uint16_t avdtp_cid, uint8_t local_seid);
uint8_t avdtp_stop_stream (uint16_t avdtp_cid, uint8_t local_seid);
uint8_t avdtp_abort_stream(uint16_t avdtp_cid, uint8_t local_seid);
uint8_t avdtp_suspend_stream(uint16_t avdtp_cid, uint8_t local_seid);

uint8_t avdtp_discover_stream_endpoints(uint16_t avdtp_cid);
uint8_t avdtp_get_capabilities(uint16_t avdtp_cid, uint8_t remote_seid);
uint8_t avdtp_get_all_capabilities(uint16_t avdtp_cid, uint8_t remote_seid, avdtp_role_t role);
uint8_t avdtp_get_configuration(uint16_t avdtp_cid, uint8_t remote_seid);
uint8_t avdtp_set_configuration(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);
uint8_t avdtp_reconfigure(uint16_t avdtp_cid, uint8_t local_seid, uint8_t remote_seid, uint16_t configured_services_bitmap, avdtp_capabilities_t configuration);
uint8_t avdtp_validate_media_configuration(const avdtp_stream_endpoint_t *stream_endpoint, uint16_t avdtp_cid,
                                           uint8_t reconfigure, const adtvp_media_codec_capabilities_t *media_codec);

// frequency will be used by avdtp_choose_sbc_sampling_frequency (if supported by both endpoints)
void    avdtp_set_preferred_sampling_frequency(avdtp_stream_endpoint_t * stream_endpoint, uint32_t sampling_frequency);

// channel_mode will be used by avdtp_choose_sbc_channel_mode (if supported by both endpoints)
void    avdtp_set_preferred_channel_mode(avdtp_stream_endpoint_t * stream_endpoint, uint8_t channel_mode);

void    avdtp_set_preferred_sbc_channel_mode(avdtp_stream_endpoint_t * stream_endpoint, uint32_t sampling_frequency);

/**
 * @brief Get highest sampling frequency
 * @param sampling_frequency_bitmap
 * @return highest frequency or 0
 */
uint16_t avdtp_get_highest_sampling_frequency(uint8_t sampling_frequency_bitmap);

avdtp_channel_mode_t avdtp_choose_sbc_channel_mode(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_channel_mode_bitmap);
avdtp_sbc_allocation_method_t avdtp_choose_sbc_allocation_method(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_allocation_method_bitmap);
uint16_t avdtp_choose_sbc_sampling_frequency(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_sampling_frequency_bitmap);
uint8_t avdtp_choose_sbc_subbands(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_subbands_bitmap);
uint8_t avdtp_choose_sbc_block_length(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_block_length_bitmap);
uint8_t avdtp_choose_sbc_max_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_max_bitpool_value);
uint8_t avdtp_choose_sbc_min_bitpool_value(avdtp_stream_endpoint_t * stream_endpoint, uint8_t remote_min_bitpool_value);

uint8_t avdtp_stream_endpoint_seid(avdtp_stream_endpoint_t * stream_endpoint);

uint8_t is_avdtp_remote_seid_registered(avdtp_stream_endpoint_t * stream_endpoint);

uint8_t avdtp_get_next_transaction_label(void);

#ifdef ENABLE_AVDTP_ACCEPTOR_EXPLICIT_START_STREAM_CONFIRMATION
uint8_t avdtp_start_stream_accept(uint16_t avdtp_cid, uint8_t local_seid);
uint8_t avdtp_start_stream_reject(uint16_t avdtp_cid, uint8_t local_seid);
#endif

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
void avdtp_init_fuzz(void);
void avdtp_packet_handler_fuzz(uint8_t *packet, uint16_t size);
#endif

#if defined __cplusplus
}
#endif

#endif // AVDTP_H
