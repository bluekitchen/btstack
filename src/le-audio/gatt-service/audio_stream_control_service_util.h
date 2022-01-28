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
 * @title Audio Stream Sontrol Service Util (ASCS)
 * 
 */

#ifndef AUDIO_STREAM_CONTROL_SERVICE_UTIL_H
#define AUDIO_STREAM_CONTROL_SERVICE_UTIL_H

#include <stdint.h>
#include "le-audio/le_audio.h"

#if defined __cplusplus
extern "C" {
#endif

#define ASCS_STREAMENDPOINTS_MAX_NUM 5
#define ASCS_CLIENTS_MAX_NUM 5
#define ASCS_STREAMENDPOINT_STATE_MAX_SIZE 5

#define ASCS_ERROR_CODE_SUCCESS                                                     0x00
#define ASCS_ERROR_CODE_UNSUPPORTED_OPCODE                                          0x01
#define ASCS_ERROR_CODE_INVALID_LENGTH                                              0x02
#define ASCS_ERROR_CODE_INVALID_ASE_ID                                              0x03
#define ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION                        0x04
#define ASCS_ERROR_CODE_INVALID_ASE_DIRECTION                                       0x05
#define ASCS_ERROR_CODE_UNSUPPORTED_AUDIO_CAPABILITIES                              0x06
#define ASCS_ERROR_CODE_UNSUPPORTED_CONFIGURATION_PARAMETER_VALUE                   0x07
#define ASCS_ERROR_CODE_REJECTED_CONFIGURATION_PARAMETER_VALUE                      0x08
#define ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE                       0x09
#define ASCS_ERROR_CODE_UNSUPPORTED_METADATA                                        0x0A
#define ASCS_ERROR_CODE_REJECTED_METADATA                                           0x0B
#define ASCS_ERROR_CODE_INVALID_METADATA                                            0x0C
#define ASCS_ERROR_CODE_INSUFFICIENT_RESOURCES                                      0x0D
#define ASCS_ERROR_CODE_UNSPECIFIED_ERROR                                           0x0E
#define ASCS_ERROR_CODE_RFU                                                         0x0D


#define ASCS_REJECT_REASON_CODEC_ID                                                 0x01
#define ASCS_REJECT_REASON_CODEC_SPECIFIC_CONFIGURATION                             0x02
#define ASCS_REJECT_REASON_SDU_INTERVAL                                             0x03
#define ASCS_REJECT_REASON_FRAMING                                                  0x04
#define ASCS_REJECT_REASON_PHY                                                      0x05
#define ASCS_REJECT_REASON_MAXIMUM_SDU_SIZE                                         0x06
#define ASCS_REJECT_REASON_RETRANSMISSION_NUMBER                                    0x07
#define ASCS_REJECT_REASON_MAX_TRANSPORT_LATENCY                                    0x08
#define ASCS_REJECT_REASON_PRESENTATION_DELAY                                       0x09
#define ASCS_REJECT_REASON_INVALID_ASE_CIS_MAPPING                                  0x0A
#define ASCS_REJECT_REASON_RFU                                                      0x0B

typedef enum {
    ASCS_OPCODE_UNSUPPORTED = 0x00,
    ASCS_OPCODE_CONFIG_CODEC = 0x01,
    ASCS_OPCODE_CONFIG_QOS,
    ASCS_OPCODE_ENABLE,
    ASCS_OPCODE_RECEIVER_START_READY,
    ASCS_OPCODE_DISABLE,
    ASCS_OPCODE_RECEIVER_STOP_READY, 
    ASCS_OPCODE_UPDATE_METADATA,
    ASCS_OPCODE_RELEASE,
    ASCS_OPCODE_RELEASED,
    ASCS_OPCODE_RFU
} ascs_opcode_t;

typedef enum {
    ASCS_STATE_IDLE = 0x00,
    ASCS_STATE_CODEC_CONFIGURED,
    ASCS_STATE_QOS_CONFIGURED,
    ASCS_STATE_ENABLING,
    ASCS_STATE_STREAMING,
    ASCS_STATE_DISABLING,
    ASCS_STATE_RELEASING,
    ASCS_STATE_RFU
} ascs_state_t;

typedef struct {
    uint8_t  codec_configuration_mask;

    le_audio_codec_sampling_frequency_index_t sampling_frequency_index; 
    le_audio_codec_frame_duration_index_t frame_duration_index;
    uint32_t audio_channel_allocation_mask;
    uint16_t octets_per_codec_frame;
    uint8_t  codec_frame_blocks_per_sdu;
} ascs_specific_codec_configuration_t;

typedef struct {
    uint8_t  framing;                                   // see LEA_UNFRAMED_ISOAL_PDUS_* in le_audio.h
    uint8_t  preferred_phy;                             // see LEA_PHY_* in le_audio.h
    uint8_t  preferred_retransmission_number;           // 0x00-0xFF
    uint16_t max_transport_latency_ms;                  // 0x0005–0x0FA0
    uint32_t presentation_delay_min_us;                 // 3 byte, microsec
    uint32_t presentation_delay_max_us;                 // 3 byte, microsec
    uint32_t preferred_presentation_delay_min_us;       // 3 byte, microsec, 0x00 - no preference
    uint32_t preferred_presentation_delay_max_us;       // 3 byte, microsec, 0x00 - no preference

    hci_audio_coding_format_t coding_format;
    uint16_t company_id;
    uint16_t vendor_specific_codec_id;

    ascs_specific_codec_configuration_t specific_codec_configuration;
} ascs_codec_configuration_t;

typedef struct {
    le_audio_client_target_latency_t target_latency;
    le_audio_client_target_phy_t target_phy;

    hci_audio_coding_format_t coding_format;
    uint16_t company_id;
    uint16_t vendor_specific_codec_id;

    ascs_specific_codec_configuration_t specific_codec_configuration;
} ascs_client_codec_configuration_request_t;

typedef struct {
    uint8_t  cig_id;
    uint8_t  cis_id;
    uint32_t sdu_interval;                              // 0x0000FF–0x0FFFFF
    uint8_t  framing;                                   // 0 - unframed, 1 - framed
    uint8_t  phy;                                       // see LEA_PHY_* in le_audio.h
    uint16_t max_sdu;                                   // 0x00–0xFFF
    uint8_t  retransmission_number;                     // 0x00–0xFF
    uint16_t max_transport_latency;                     // 0x0005–0x0FA0
    uint32_t presentation_delay_us;                     // 3 byte, microsec
} ascs_qos_configuration_t;


// The ASCS Server must expose at least one Audio Stream Endpoint (ASE) for every Audio Stream it is capable of supporting.
// For each ASE, the ASCS Server maintains an instance of a state machine for each connected client. The state of an ASE is
// controlled by each client, although in some cases the Server can autonomously change the state of an ASE.

typedef struct {
    uint8_t  ase_id;
    le_audio_role_t role;

    uint16_t ase_characteristic_value_handle;
    uint16_t ase_characteristic_end_handle;
    uint16_t ase_characteristic_client_configuration_handle;
    uint16_t ase_characteristic_properties;
} ascs_streamendpoint_characteristic_t;

typedef struct {
    ascs_streamendpoint_characteristic_t * ase_characteristic;
    uint16_t ase_characteristic_client_configuration;

    ascs_state_t state;

    // Codec Configuration
    ascs_codec_configuration_t    codec_configuration;
    // QoS Configuration
    ascs_qos_configuration_t      qos_configuration;
    // Enable Metadata
    le_audio_metadata_t metadata;

    // ASE value changed indicates that server 
    bool ase_characteristic_value_change_initiated_by_client;
    bool ase_characteristic_value_changed_w2_notify; 

    gatt_client_notification_t notification_listener;  
} ascs_streamendpoint_t;

typedef struct {
    uint8_t  ase_id;
    uint8_t  response_code;
    uint8_t  reason;
} ascs_control_point_operation_response_t;

uint16_t ascs_util_qos_configuration_parse(const uint8_t * buffer, uint8_t buffer_size, ascs_qos_configuration_t * qos_config);
uint16_t ascs_util_codec_configuration_parse(const uint8_t * buffer, uint8_t buffer_size, ascs_codec_configuration_t * codec_config);
uint16_t ascs_util_specific_codec_configuration_parse(const uint8_t * buffer, uint16_t buffer_size, ascs_specific_codec_configuration_t * codec_configuration);
uint16_t ascs_util_codec_configuration_request_parse(uint8_t * buffer, uint8_t buffer_size, ascs_client_codec_configuration_request_t * codec_config);

uint16_t ascs_util_specific_codec_configuration_serialize_using_mask(ascs_specific_codec_configuration_t * codec_configuration, uint8_t * tlv_buffer, uint16_t tlv_buffer_size);

uint16_t ascs_util_codec_configuration_serialize(ascs_codec_configuration_t * codec_configuration, uint8_t * event, uint16_t event_size);
uint16_t ascs_util_specific_codec_configuration_serialize(ascs_specific_codec_configuration_t * specific_codec_configuration, uint8_t * event, uint16_t event_size);
uint16_t ascs_util_qos_configuration_serialize(ascs_qos_configuration_t * qos_configuration, uint8_t * event, uint16_t event_size);
uint16_t asce_util_metadata_serialize(le_audio_metadata_t * metadata, uint8_t * value, uint16_t value_size);
    
uint16_t asce_util_codec_configuration_request_serialize(ascs_client_codec_configuration_request_t * codec_configuration, uint8_t * value, uint16_t value_size);

void ascs_util_emit_codec_configuration(btstack_packet_handler_t ascs_event_callback, hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state, ascs_codec_configuration_t * codec_configuration);
void ascs_util_emit_qos_configuration(btstack_packet_handler_t ascs_event_callback, hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state, ascs_qos_configuration_t * qos_configuration);
void ascs_util_emit_metadata(btstack_packet_handler_t ascs_event_callback, hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state, le_audio_metadata_t * metadata);

#if defined __cplusplus
}
#endif

#endif

