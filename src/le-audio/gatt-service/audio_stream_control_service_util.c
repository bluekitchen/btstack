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

#define BTSTACK_FILE__ "audio_stream_control_service_util.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/audio_stream_control_service_util.h"

static char * ase_state_name[] = {
    "IDLE",
    "CODEC_CONFIGURED",
    "QOS_CONFIGURED",
    "ENABLING",
    "STREAMING",
    "DISABLING",
    "RELEASING",
    "RFU"
};

char * ascs_util_ase_state2str(ascs_state_t state){
    uint8_t state_index = (uint8_t)state;

    if (state_index < 7){
        return ase_state_name[state_index];
    }
    return ase_state_name[7];
}

static uint16_t ascs_util_get_value_size_for_codec_configuration_type(le_audio_codec_configuration_type_t codec_config_type){
    switch (codec_config_type){
        case LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY:
        case LE_AUDIO_CODEC_CONFIGURATION_TYPE_FRAME_DURATION:
        case LE_AUDIO_CODEC_CONFIGURATION_TYPE_CODEC_FRAME_BLOCKS_PER_SDU:
            return 1;
        case LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION:
            return 4;
        case LE_AUDIO_CODEC_CONFIGURATION_TYPE_OCTETS_PER_CODEC_FRAME:
            return 2;
        default:
            break;
    }
    return 0;
}

uint16_t ascs_util_qos_configuration_parse(const uint8_t * buffer, uint8_t buffer_size, ascs_qos_configuration_t * qos_config){
    UNUSED(buffer_size);
    uint16_t offset = 0;
    qos_config->cig_id = buffer[offset++];
    qos_config->cis_id = buffer[offset++];
    qos_config->sdu_interval = little_endian_read_24(buffer, offset);
    offset += 3;
    qos_config->framing = buffer[offset++];
    qos_config->phy = buffer[offset++];
    qos_config->max_sdu = little_endian_read_16(buffer, offset);
    offset += 2;
    qos_config->retransmission_number = buffer[offset++];                  
    qos_config->max_transport_latency_ms = little_endian_read_16(buffer, offset);
    offset += 2;
    qos_config->presentation_delay_us = little_endian_read_24(buffer, offset);
    offset += 3;
    return offset;
}


uint16_t ascs_util_specific_codec_configuration_parse(const uint8_t * buffer, uint16_t buffer_size, ascs_specific_codec_configuration_t * codec_configuration){
    btstack_assert(buffer_size > 0);

    uint16_t pos = 0;
    uint16_t codec_config_length = buffer[pos++];
    
    if ( (codec_config_length == 0) || (codec_config_length > (buffer_size - pos)) ){
        return pos;
    }

    uint16_t remaining_bytes = codec_config_length;
    codec_configuration->codec_configuration_mask = 0;

    while (remaining_bytes > 1) {
        uint8_t payload_length = buffer[pos++];
        remaining_bytes -= 1;
        
        if (payload_length > remaining_bytes){
            return (pos - 1);
        }

        le_audio_codec_configuration_type_t codec_config_type = (le_audio_codec_configuration_type_t)buffer[pos];
        if ((codec_config_type == LE_AUDIO_CODEC_CONFIGURATION_TYPE_UNDEFINED) || (codec_config_type >= LE_AUDIO_CODEC_CONFIGURATION_TYPE_RFU)){
            return (pos - 2);
        }
        if (payload_length != (ascs_util_get_value_size_for_codec_configuration_type(codec_config_type) + 1)){
            return (pos - 2);
        }

        codec_configuration->codec_configuration_mask |= (1 << codec_config_type);

        switch (codec_config_type){
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY:
                codec_configuration->sampling_frequency_index = buffer[pos];
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_FRAME_DURATION:
                codec_configuration->frame_duration_index = buffer[pos];
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION:
                codec_configuration->audio_channel_allocation_mask = little_endian_read_32(buffer, pos);
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_OCTETS_PER_CODEC_FRAME:
                codec_configuration->octets_per_codec_frame = little_endian_read_16(buffer, pos);
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_CODEC_FRAME_BLOCKS_PER_SDU:
                codec_configuration->codec_frame_blocks_per_sdu = buffer[pos];
                break;
            default:
                break;
        }

        pos += payload_length;
        remaining_bytes -= payload_length;
    }
    return pos;
}

uint16_t ascs_util_codec_configuration_parse(const uint8_t * buffer, uint8_t buffer_size, ascs_codec_configuration_t * codec_configuration){
    btstack_assert(buffer_size > 22);

    uint16_t pos = 0;
    codec_configuration->framing = buffer[pos++];
    codec_configuration->preferred_phy = buffer[pos++];
    codec_configuration->preferred_retransmission_number = buffer[pos++];
    codec_configuration->max_transport_latency_ms = little_endian_read_16(buffer, pos);
    pos += 2;
    codec_configuration->presentation_delay_min_us = little_endian_read_24(buffer, pos);
    pos += 3;
    codec_configuration->presentation_delay_max_us = little_endian_read_24(buffer, pos);
    pos += 3;
    codec_configuration->preferred_presentation_delay_min_us = little_endian_read_24(buffer, pos);
    pos += 3;
    codec_configuration->preferred_presentation_delay_max_us = little_endian_read_24(buffer, pos);
    pos += 3;

    codec_configuration->coding_format = buffer[pos++];
    codec_configuration->company_id = little_endian_read_16(buffer, pos);
    pos += 2;
    codec_configuration->vendor_specific_codec_id = little_endian_read_16(buffer, pos);
    pos += 2;

    
    pos += ascs_util_specific_codec_configuration_parse(&buffer[pos], buffer_size - pos,
                                                        &codec_configuration->specific_codec_configuration);
    return pos;
}

uint16_t ascs_util_codec_configuration_request_parse(uint8_t * buffer, uint8_t buffer_size, ascs_client_codec_configuration_request_t * codec_config_request){
    btstack_assert(buffer_size > 8);

    uint16_t offset = 0;
    memset(codec_config_request, 0, sizeof(ascs_client_codec_configuration_request_t));
    codec_config_request->target_latency = (le_audio_client_target_latency_t)buffer[offset++];
    codec_config_request->target_phy = (le_audio_client_target_phy_t)buffer[offset++];
    codec_config_request->coding_format = (hci_audio_coding_format_t)buffer[offset++];
    codec_config_request->company_id = little_endian_read_16(buffer, offset);
    offset += 2;
    codec_config_request->vendor_specific_codec_id = little_endian_read_16(buffer, offset);
    offset += 2;
    
    offset += ascs_util_specific_codec_configuration_parse(&buffer[offset], buffer_size - offset,
                                                           &codec_config_request->specific_codec_configuration);
    return offset;
}


uint16_t ascs_util_specific_codec_configuration_serialize_using_mask(ascs_specific_codec_configuration_t * codec_configuration, uint8_t * tlv_buffer, uint16_t tlv_buffer_size){
    btstack_assert(tlv_buffer_size > 18);
    uint16_t pos = 0;
    uint16_t remaining_bytes = tlv_buffer_size;

    uint8_t  codec_config_type;
    for (codec_config_type = (uint8_t)LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY; codec_config_type < (uint8_t) LE_AUDIO_CODEC_CONFIGURATION_TYPE_RFU; codec_config_type++){
        if ((codec_configuration->codec_configuration_mask & (1 << codec_config_type) ) == 0 ){
            continue;
        }

        uint8_t payload_length = ascs_util_get_value_size_for_codec_configuration_type(codec_config_type);
        // ensure that there is enough space in TLV to store length (1), type(1) and payload
        if (remaining_bytes < (2 + payload_length)){
            return pos;
        }

        tlv_buffer[pos++] = 1 + payload_length;
        tlv_buffer[pos++] = codec_config_type;
        
        switch ((le_audio_codec_configuration_type_t)codec_config_type){
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY:
                tlv_buffer[pos] = codec_configuration->sampling_frequency_index;
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_FRAME_DURATION:
                tlv_buffer[pos] = codec_configuration->frame_duration_index;
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION:
                little_endian_store_32(tlv_buffer, pos, codec_configuration->audio_channel_allocation_mask);
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_OCTETS_PER_CODEC_FRAME:
                little_endian_store_16(tlv_buffer, pos, codec_configuration->octets_per_codec_frame);
                break;
            case LE_AUDIO_CODEC_CONFIGURATION_TYPE_CODEC_FRAME_BLOCKS_PER_SDU:
                tlv_buffer[pos] = codec_configuration->codec_frame_blocks_per_sdu;
                break;
            default:
                break;
        } 
        pos += payload_length;
        remaining_bytes -= (payload_length + 2);
    }
    return pos;
}

uint16_t ascs_util_specific_codec_configuration_serialize(ascs_specific_codec_configuration_t * specific_codec_configuration, uint8_t * event, uint16_t event_size){
    btstack_assert(event_size > 9);
    uint8_t pos = 0;

    event[pos++] = (uint8_t)specific_codec_configuration->codec_configuration_mask;
    event[pos++] = (uint8_t)specific_codec_configuration->sampling_frequency_index;
    event[pos++] = (uint8_t)specific_codec_configuration->frame_duration_index;
    little_endian_store_32(event, pos, specific_codec_configuration->audio_channel_allocation_mask);
    pos += 4;
    little_endian_store_16(event, pos, specific_codec_configuration->octets_per_codec_frame);
    pos += 2;
    event[pos++] = (uint8_t)specific_codec_configuration->codec_frame_blocks_per_sdu;
    return pos;
}

uint16_t ascs_util_codec_configuration_serialize(ascs_codec_configuration_t * codec_configuration, uint8_t * event, uint16_t event_size){
    btstack_assert(event_size > 22);
    uint16_t pos = 0;

    event[pos++] = (uint8_t)codec_configuration->framing;
    event[pos++] = (uint8_t)codec_configuration->preferred_phy;
    event[pos++] = (uint8_t)codec_configuration->preferred_retransmission_number;

    little_endian_store_16(event, pos, codec_configuration->max_transport_latency_ms);
    pos += 2;
    
    little_endian_store_24(event, pos, codec_configuration->presentation_delay_min_us);
    pos += 3;
    little_endian_store_24(event, pos, codec_configuration->presentation_delay_max_us);
    pos += 3;
    little_endian_store_24(event, pos, codec_configuration->preferred_presentation_delay_min_us);
    pos += 3;
    little_endian_store_24(event, pos, codec_configuration->preferred_presentation_delay_max_us);
    pos += 3;

    event[pos++] = (uint8_t)codec_configuration->coding_format;
    little_endian_store_16(event, pos, codec_configuration->company_id);
    pos += 2;
    little_endian_store_16(event, pos, codec_configuration->vendor_specific_codec_id);
    pos += 2;

    // reserve place for config length
    uint8_t codec_config_length_pos = pos;
    pos++;
    uint8_t codec_config_length = ascs_util_specific_codec_configuration_serialize(&codec_configuration->specific_codec_configuration,
                                                            &event[pos],
                                                            event_size - pos);
    event[codec_config_length_pos] = codec_config_length;
    pos += codec_config_length;
    return pos;
}

uint16_t ascs_util_qos_configuration_serialize(ascs_qos_configuration_t * qos_configuration, uint8_t * event, uint16_t event_size){
    btstack_assert(event_size > 14);
    uint16_t pos = 0;

    event[pos++] = qos_configuration->cig_id;
    event[pos++] = qos_configuration->cis_id;
    little_endian_store_24(event, pos, qos_configuration->sdu_interval);
    pos += 3;
    event[pos++] = qos_configuration->framing;
    event[pos++] = qos_configuration->phy;
    little_endian_store_16(event, pos, qos_configuration->max_sdu);
    pos += 2;
    event[pos++] = qos_configuration->retransmission_number;
    little_endian_store_16(event, pos, qos_configuration->max_transport_latency_ms);
    pos += 2;
    little_endian_store_24(event, pos, qos_configuration->presentation_delay_us);
    pos += 3;
    return pos;
}

uint16_t asce_util_metadata_serialize(le_audio_metadata_t * metadata, uint8_t * value, uint16_t value_size){
    btstack_assert(value_size > 0);
    uint16_t pos = 0;

    if (metadata == NULL){
        value[pos++] = 0;
        return pos;
    }

    uint16_t meatadata_length_pos = pos;
    pos++;

    uint16_t metadata_length = le_audio_util_metadata_serialize_using_mask(metadata, &value[pos], value_size - pos);
    pos += metadata_length;
    value[meatadata_length_pos] = metadata_length;
    return pos;
}

uint16_t asce_util_codec_configuration_request_serialize(ascs_client_codec_configuration_request_t * codec_configuration, uint8_t * value, uint16_t value_size){
    btstack_assert(value_size > 6);

    uint16_t pos = 0;

    value[pos++] = (uint8_t)codec_configuration->target_latency;
    value[pos++] = (uint8_t)codec_configuration->target_phy;
    
    value[pos++] = (uint8_t)codec_configuration->coding_format;
    little_endian_store_16(value, pos, codec_configuration->company_id);
    pos += 2;
    little_endian_store_16(value, pos, codec_configuration->vendor_specific_codec_id);
    pos += 2;

    // reserve place for config length
    uint16_t codec_config_length_pos = pos;
    pos++;

    uint8_t codec_config_length = ascs_util_specific_codec_configuration_serialize_using_mask(
            &codec_configuration->specific_codec_configuration, &value[pos], value_size - pos);
    value[codec_config_length_pos] = codec_config_length;
    pos += codec_config_length;
    return pos;
}

void ascs_util_emit_codec_configuration(btstack_packet_handler_t ascs_event_callback, hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state, ascs_codec_configuration_t * codec_configuration){
    btstack_assert(ascs_event_callback != NULL);
    
    uint8_t event[39];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = ase_id;
    pos += ascs_util_codec_configuration_serialize(codec_configuration, &event[pos], sizeof(event) - pos);
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

void ascs_util_emit_qos_configuration(btstack_packet_handler_t ascs_event_callback, hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state, ascs_qos_configuration_t * qos_configuration){
    btstack_assert(ascs_event_callback != NULL);
    
    uint8_t event[21];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_QOS_CONFIGURATION;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = ase_id;
    pos += ascs_util_qos_configuration_serialize(qos_configuration, &event[pos], sizeof(event) - pos);
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


void ascs_util_emit_metadata(btstack_packet_handler_t ascs_event_callback, hci_con_handle_t con_handle, uint8_t ase_id, ascs_state_t state, le_audio_metadata_t * metadata){
    btstack_assert(ascs_event_callback != NULL);

    uint8_t event[25 + LE_AUDIO_PROGRAM_INFO_MAX_LENGTH
            + LE_AUDIO_PROGRAM_INFO_URI_MAX_LENGTH
            + LE_AUDIO_EXTENDED_METADATA_MAX_LENGHT
            + LE_AUDIO_VENDOR_SPECIFIC_METADATA_MAX_LENGTH
            + LE_CCIDS_MAX_NUM];

    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = (uint8_t)sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_METADATA;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = ase_id;
    uint16_t metadata_length = le_audio_util_metadata_serialize(metadata, &event[pos], sizeof(event) - pos);
    pos += metadata_length;
    
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}
