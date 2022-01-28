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

#define BTSTACK_FILE__ "audio_stream_control_service_server.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/audio_stream_control_service_util.h"
#include "le-audio/gatt-service/audio_stream_control_service_server.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define ASCS_TASK_SEND_CODEC_CONFIGURATION_VALUE_CHANGED    0x01
#define ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE     0x02


static att_service_handler_t    audio_stream_control_service;
static btstack_packet_handler_t ascs_event_callback;

static ascs_streamendpoint_characteristic_t * ascs_streamendpoint_characteristics;
static uint8_t  ascs_streamendpoint_chr_num = 0;
static ascs_remote_client_t * ascs_clients;
static uint8_t ascs_clients_num = 0;
static uint8_t ascs_streamendpoint_characteristics_id_counter = 0;

// characteristic: ASE_CONTROL_POINT
static uint16_t ascs_ase_control_point_handle;
static uint16_t ascs_ase_control_point_client_configuration_handle;
// configration of conntrol point notifications is stored per client

#ifdef ENABLE_TESTING_SUPPORT
static void dump_streamendpoint(ascs_remote_client_t * client, ascs_streamendpoint_t * streamendpoint){
    printf("Streamendpoint (con_handle 0x%02x): \n", client->con_handle);
    printf("    - id              %d\n", streamendpoint->ase_characteristic->ase_id);
    printf("    - value handle 0x%02x\n", streamendpoint->ase_characteristic->ase_characteristic_value_handle);
    printf("    - ccc   handle 0x%02x\n", streamendpoint->ase_characteristic->ase_characteristic_client_configuration_handle);
    printf("    - config          %d\n", streamendpoint->ase_characteristic_client_configuration);
}
#endif

static uint8_t ascs_get_next_streamendpoint_chr_id(void){
    uint8_t next_streamendpoint_id;
    if (ascs_streamendpoint_characteristics_id_counter == 0xff) {
        next_streamendpoint_id = 1;
    } else {
        next_streamendpoint_id = ascs_streamendpoint_characteristics_id_counter + 1;
    }
    ascs_streamendpoint_characteristics_id_counter = next_streamendpoint_id;
    return next_streamendpoint_id;
}

static ascs_remote_client_t * ascs_get_remote_client_for_con_handle(hci_con_handle_t con_handle){
    if (con_handle == HCI_CON_HANDLE_INVALID){
        return NULL;
    }
    uint8_t i;
    for (i = 0; i < ascs_clients_num; i++){
        if (ascs_clients[i].con_handle == con_handle){
            return &ascs_clients[i];
        }
    }
    return NULL;
}

static ascs_remote_client_t * ascs_server_add_remote_client(hci_con_handle_t con_handle){
    uint8_t i;
    
    for (i = 0; i < ascs_clients_num; i++){
        if (ascs_clients[i].con_handle == HCI_CON_HANDLE_INVALID){
            ascs_clients[i].con_handle = con_handle;
            log_info("added client 0x%02x, index %d", con_handle, i);
            return &ascs_clients[i];
        } 
    }
    return NULL;
}

static ascs_streamendpoint_t * ascs_get_streamendpoint_for_ase_id(ascs_remote_client_t * client, uint8_t ase_id){
    uint8_t i;
    for (i = 0; i < ascs_streamendpoint_chr_num; i++){
        if (client->streamendpoints[i].ase_characteristic->ase_id == ase_id){
            return &client->streamendpoints[i];
        }
    }
    return NULL;
}

static bool ascs_streamendpoint_in_source_role(ascs_streamendpoint_t * streamendpoint){
    return streamendpoint->ase_characteristic->role == LE_AUDIO_ROLE_SOURCE;
}

static bool ascs_streamendpoint_can_transit_to_state(ascs_streamendpoint_t * streamendpoint, ascs_opcode_t opcode, ascs_state_t target_state){
    btstack_assert(streamendpoint != NULL);
    switch (streamendpoint->state){
        case ASCS_STATE_IDLE:
            switch (opcode) {
                case ASCS_OPCODE_CONFIG_CODEC:
                    return target_state == ASCS_STATE_CODEC_CONFIGURED;
                default:
                    break;
            }
            return false;

        case ASCS_STATE_CODEC_CONFIGURED:
            switch (opcode) {
                case ASCS_OPCODE_CONFIG_CODEC:
                    return target_state == ASCS_STATE_CODEC_CONFIGURED;
                case ASCS_OPCODE_CONFIG_QOS:
                    return target_state == ASCS_STATE_QOS_CONFIGURED;
                case ASCS_OPCODE_RELEASE:
                    return target_state == ASCS_STATE_RELEASING;
                default:
                    break;
            }
            return false;

        case ASCS_STATE_QOS_CONFIGURED:
            switch (opcode) {
                case ASCS_OPCODE_CONFIG_QOS:
                    return target_state == ASCS_STATE_QOS_CONFIGURED;
                case ASCS_OPCODE_ENABLE:
                    return target_state == ASCS_STATE_ENABLING;
                case ASCS_OPCODE_RELEASE:
                    return target_state == ASCS_STATE_RELEASING;
                default:
                    break;
            }
            return false;

        case ASCS_STATE_ENABLING:
            switch (opcode) {
                case ASCS_OPCODE_UPDATE_METADATA:
                    return target_state == ASCS_STATE_ENABLING;
                case ASCS_OPCODE_RELEASE:
                    return target_state == ASCS_STATE_RELEASING;
                case ASCS_OPCODE_RECEIVER_START_READY:
                    return target_state == ASCS_STATE_STREAMING;
                case ASCS_OPCODE_DISABLE:
                    if (ascs_streamendpoint_in_source_role(streamendpoint)) {
                        return target_state == ASCS_STATE_DISABLING;
                    }
                    return target_state == ASCS_STATE_QOS_CONFIGURED;
                default:
                    break;
            }
            return false;

        case ASCS_STATE_STREAMING:
            switch (opcode) {
                case ASCS_OPCODE_UPDATE_METADATA:
                    return target_state == ASCS_STATE_STREAMING;
                case ASCS_OPCODE_RELEASE:
                    return target_state == ASCS_STATE_RELEASING;
                case ASCS_OPCODE_DISABLE:
                    if (ascs_streamendpoint_in_source_role(streamendpoint)) {
                        return target_state == ASCS_STATE_DISABLING;
                    }
                    return target_state == ASCS_STATE_QOS_CONFIGURED;
                default:
                    break;
            }
            return false;

        case ASCS_STATE_DISABLING:
            if (ascs_streamendpoint_in_source_role(streamendpoint)) {
                switch (opcode) {
                    case ASCS_OPCODE_RELEASE:
                        return target_state == ASCS_STATE_RELEASING;
                    case ASCS_OPCODE_RECEIVER_STOP_READY:
                        return target_state == ASCS_STATE_QOS_CONFIGURED;
                    default:
                        break;
                }
            }
            return false;

        case ASCS_STATE_RELEASING:
            switch (opcode) {
                case ASCS_OPCODE_RELEASED:
                    // TODO: if (caching) return target_state == ASCS_STATE_CODEC_CONFIGURED;
                    return target_state == ASCS_STATE_IDLE;
                default:
                    break;
            }
            return false;

        default:
            btstack_assert(false);
            return false;
    }
}


static void ascs_client_reset_response(ascs_remote_client_t * client){
    client->response_opcode = ASCS_OPCODE_UNSUPPORTED;
    client->response_ases_num = 0;
    memset(client->response, 0, sizeof(ascs_control_point_operation_response_t) * ASCS_STREAMENDPOINTS_MAX_NUM);
}



static void ascs_client_reset_streamendpoints_for_client(ascs_remote_client_t * client){
    if (client == NULL){
        return;
    }
    uint8_t i;
    for (i = 0; i < ascs_streamendpoint_chr_num; i++){
        ascs_streamendpoint_t * streamendpoint = &client->streamendpoints[i];
        streamendpoint->state = ASCS_STATE_IDLE;
        memset(&streamendpoint->codec_configuration, 0, sizeof(ascs_codec_configuration_t));
        memset(&streamendpoint->qos_configuration, 0, sizeof(ascs_qos_configuration_t));
        memset(&streamendpoint->metadata, 0, sizeof(le_audio_metadata_t));
        streamendpoint->ase_characteristic_value_change_initiated_by_client = false;
        streamendpoint->ase_characteristic_value_changed_w2_notify = false;
    }
}

static void ascs_client_reset(ascs_remote_client_t * client){
    if (client == NULL){
        return;
    }
            
    client->scheduled_tasks = 0;
    client->con_handle = HCI_CON_HANDLE_INVALID;
    ascs_client_reset_response(client);
    ascs_client_reset_streamendpoints_for_client(client);
}

static bool ascs_client_request_successfully_processed(ascs_remote_client_t * client, uint8_t response_index){
    if (client->response[response_index].response_code != ASCS_ERROR_CODE_SUCCESS){
        return false;
    }

    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, client->response[response_index].ase_id);
    streamendpoint->ase_characteristic_value_change_initiated_by_client = true;
    return true;
}


static void ascs_server_emit_remote_client_disconnected(hci_con_handle_t con_handle){
    uint8_t event[5];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_REMOTE_CLIENT_DISCONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void ascs_server_emit_remote_client_connected(hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_REMOTE_CLIENT_CONNECTED;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = status;
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void ascs_emit_client_request(hci_con_handle_t con_handle, uint8_t ase_id, uint8_t subevent_id){
    uint8_t event[6];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = subevent_id;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = ase_id;
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}


void ascs_server_emit_codec_configuration_request(hci_con_handle_t con_handle, uint8_t ase_id, ascs_client_codec_configuration_request_t * codec_configuration_request){
    btstack_assert(ascs_event_callback != NULL);
    uint8_t event[23];
    
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_ASCS_CODEC_CONFIGURATION_REQUEST;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    event[pos++] = ase_id;
    event[pos++] = (uint8_t)codec_configuration_request->target_latency;
    event[pos++] = (uint8_t)codec_configuration_request->target_phy;
    event[pos++] = (uint8_t)codec_configuration_request->coding_format;
    little_endian_store_16(event, pos, codec_configuration_request->company_id);
    pos += 2;
    little_endian_store_16(event, pos, codec_configuration_request->vendor_specific_codec_id);
    pos += 2;

    pos += ascs_util_specific_codec_configuration_serialize(&codec_configuration_request->specific_codec_configuration,
                                                            &event[pos], sizeof(event) - pos);
    (*ascs_event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}


static uint16_t asce_server_ase_serialize(ascs_streamendpoint_t * streamendpoint, uint8_t * value, uint16_t value_size){
    UNUSED(value_size);
    uint16_t pos = 0;

    value[pos++] = streamendpoint->ase_characteristic->ase_id;
    value[pos++] = (uint8_t)streamendpoint->state;

    switch (streamendpoint->state){
        case ASCS_STATE_CODEC_CONFIGURED:
            pos += ascs_util_codec_configuration_serialize(&streamendpoint->codec_configuration, &value[pos],
                                                      value_size - pos);
            break;
        case ASCS_STATE_QOS_CONFIGURED:
            pos += ascs_util_qos_configuration_serialize(&streamendpoint->qos_configuration, &value[pos], value_size - pos);
            break;

        case ASCS_STATE_ENABLING:
        case ASCS_STATE_STREAMING:
        case ASCS_STATE_DISABLING:
            value[pos++] = streamendpoint->qos_configuration.cig_id;
            value[pos++] = streamendpoint->qos_configuration.cis_id;

            pos += asce_util_metadata_serialize(&streamendpoint->metadata, &value[pos], value_size - pos);
            break;
        default:
            break;           
    }
    return pos;
}

static uint16_t audio_stream_control_service_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    ascs_remote_client_t * client = ascs_get_remote_client_for_con_handle(con_handle);
    
    if (client == NULL){
        client = ascs_server_add_remote_client(con_handle);
        
        if (client == NULL){
            ascs_server_emit_remote_client_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d clients connected. No memory for new client.", ascs_clients_num);
            return 0;
        } else {
            ascs_server_emit_remote_client_connected(con_handle, ERROR_CODE_SUCCESS);
        }    
    }

    if (attribute_handle == ascs_ase_control_point_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(client->ase_control_point_client_configuration, offset, buffer, buffer_size);
    }

    uint8_t i;
    for (i = 0; i < ascs_streamendpoint_chr_num; i++){
        ascs_streamendpoint_t * streamendpoint = &client->streamendpoints[i];
        btstack_assert(streamendpoint != NULL);

        if (attribute_handle == streamendpoint->ase_characteristic->ase_characteristic_value_handle){
            uint8_t value[25 + LE_AUDIO_MAX_CODEC_CONFIG_SIZE]; 
            uint8_t value_size = asce_server_ase_serialize(streamendpoint, value, sizeof(value));
            return att_read_callback_handle_blob(value, value_size, offset, buffer, buffer_size);
        }

        if (attribute_handle == streamendpoint->ase_characteristic->ase_characteristic_client_configuration_handle){
            return att_read_callback_handle_little_endian_16(streamendpoint->ase_characteristic_client_configuration, offset, buffer, buffer_size);
        }
    }
    // reset client if no attribute handle was associated with it
    client->con_handle = HCI_CON_HANDLE_INVALID;
    return 0;
}

static void audio_stream_control_service_server_can_send_now(void * context){
    ascs_remote_client_t * client = (ascs_remote_client_t *) context;
    if (client->con_handle == HCI_CON_HANDLE_INVALID){
        ascs_client_reset(client);
        return;
    }
    
    if ((client->scheduled_tasks & ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE) != 0){
        client->scheduled_tasks &= ~ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE;

        uint8_t value[2+3*ASCS_STREAMENDPOINTS_MAX_NUM];
        uint8_t pos = 0;
        
        value[pos++] = client->response_opcode;
        value[pos++] = client->response_ases_num;

        if (client->response_ases_num != 0xFF){
            uint8_t i;
            for (i = 0; i < client->response_ases_num; i++){
                ascs_control_point_operation_response_t response = client->response[i];

                value[pos++] = response.ase_id;
                value[pos++] = response.response_code;
                value[pos++] = response.reason;
            }
        }
        att_server_notify(client->con_handle, ascs_ase_control_point_handle, &value[0], pos);

    } else if ((client->scheduled_tasks & ASCS_TASK_SEND_CODEC_CONFIGURATION_VALUE_CHANGED) != 0){
        client->scheduled_tasks &= ~ASCS_TASK_SEND_CODEC_CONFIGURATION_VALUE_CHANGED;

        bool notification_sent = false;
        uint8_t i;
        for (i = 0; i < ascs_streamendpoint_chr_num; i++){
            
            ascs_streamendpoint_t * streamendpoint = &client->streamendpoints[i];
            if (!streamendpoint->ase_characteristic_value_changed_w2_notify){
                continue;
            }

            if (!notification_sent){
                notification_sent = true;
                streamendpoint->ase_characteristic_value_changed_w2_notify = false;
                streamendpoint->ase_characteristic_value_change_initiated_by_client = false;

                uint8_t value[25 + LE_AUDIO_MAX_CODEC_CONFIG_SIZE]; 
                uint16_t value_size = asce_server_ase_serialize(streamendpoint, value, sizeof(value));
                att_server_notify(client->con_handle, streamendpoint->ase_characteristic->ase_characteristic_value_handle, &value[0], value_size);
            } else {
                client->scheduled_tasks |= ASCS_TASK_SEND_CODEC_CONFIGURATION_VALUE_CHANGED;
                break;
            }
        }
    }

    if (client->scheduled_tasks != 0){
        client->scheduled_tasks_callback.callback = &audio_stream_control_service_server_can_send_now;
        client->scheduled_tasks_callback.context  = (void*) client;
        att_server_register_can_send_now_callback(&client->scheduled_tasks_callback, client->con_handle);
    }
}

static void ascs_schedule_task(ascs_remote_client_t * client, uint8_t task){
    if (client->con_handle == HCI_CON_HANDLE_INVALID){
        ascs_client_reset(client);
        return;
    }

    if (task == ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE){
        // check if control point chr notification enabled
        if (client->ase_control_point_client_configuration == 0){
            return;
        }
    } 

    uint16_t scheduled_tasks = client->scheduled_tasks;
    client->scheduled_tasks |= task;

    if (scheduled_tasks == 0){
        client->scheduled_tasks_callback.callback = &audio_stream_control_service_server_can_send_now;
        client->scheduled_tasks_callback.context  = (void*) client;
        att_server_register_can_send_now_callback(&client->scheduled_tasks_callback, client->con_handle);
    }
}

static void ascs_update_control_point_operation_response(ascs_remote_client_t * client, uint8_t response_index, uint8_t response_code, uint8_t reason){
     client->response[response_index].response_code = response_code;
     client->response[response_index].reason = reason;
}

static bool ascs_control_point_operation_has_valid_length(ascs_opcode_t opcode, uint8_t ases_num, uint8_t *buffer, uint16_t buffer_size){
    uint16_t pos = 0;
    
    uint8_t  i;
    switch (opcode){
        case ASCS_OPCODE_CONFIG_CODEC:
            for (i = 0; i < ases_num; i++){
                if ((buffer_size - pos) < 9){
                    return false;
                }
                // ase_id(1), latency(1), phy(1), codec_id(5)
                pos += 8; 
                uint8_t codec_config_len = buffer[pos++];
            
                if ((buffer_size - pos) < codec_config_len){
                    return false;
                }
                pos += codec_config_len; 
            }
            break;

        case ASCS_OPCODE_CONFIG_QOS:
            for (i = 0; i < ases_num; i++){
                if ((buffer_size - pos) < 16){
                    return 0;
                }
                // ase_id(1), cig_id(1), cis_id(1), sdu_interval(3), framing(1), phy(1), max_sdu(2), 
                // retransmission_number(1), max_transport_latency(2), presentation_delay(3)
                pos += 16; 
            }
            break;

        case ASCS_OPCODE_ENABLE:
        case ASCS_OPCODE_UPDATE_METADATA:
            for (i = 0; i < ases_num; i++){
                if ((buffer_size - pos) < 2){
                    return 0;
                }
                // ase_id(1)
                pos++;
                uint8_t metadata_length = buffer[pos++];
                if ((buffer_size - pos) < metadata_length){
                    return false;
                }
                pos += metadata_length; 
            }
            break;
        
        case ASCS_OPCODE_RECEIVER_START_READY:
        case ASCS_OPCODE_DISABLE:
        case ASCS_OPCODE_RECEIVER_STOP_READY:
        case ASCS_OPCODE_RELEASE:
            // ases_num * ase_id(1)
            pos += ases_num;
            break;
        
        default:
            // allows for ASCS_ERROR_CODE_UNSUPPORTED_OPCODE error
            return true;
    }
    
    return (pos == buffer_size);
}

static void ascs_control_point_operation_prepare_response_for_codec_configuration(ascs_remote_client_t * client, uint8_t ase_index, uint8_t ase_id, ascs_client_codec_configuration_request_t codec_config){
    client->response[ase_index].ase_id = ase_id;

    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_ID, 0);
        return;
    }

    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, ASCS_OPCODE_CONFIG_CODEC, ASCS_STATE_CODEC_CONFIGURED)){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION, 0);
        return;
    }

    if (codec_config.target_latency >= LE_AUDIO_CLIENT_TARGET_LATENCY_RFU){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_MAX_TRANSPORT_LATENCY);
        return;
    }

    if (codec_config.target_phy >= LE_AUDIO_CLIENT_TARGET_PHY_RFU){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_PHY);
        return;
    }

    if (codec_config.coding_format >= HCI_AUDIO_CODING_FORMAT_RFU &&
        codec_config.coding_format !=  HCI_AUDIO_CODING_FORMAT_VENDOR_SPECIFIC){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_CODEC_ID);
        return;
    }

    switch (codec_config.coding_format){
        case HCI_AUDIO_CODING_FORMAT_LC3:
            if (codec_config.company_id != 0){
                ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_CODEC_ID);
                return;
            }
            if (codec_config.vendor_specific_codec_id != 0){
                ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_CODEC_ID);
                return;
            }
            break;
        default:
            ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_REJECTED_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_CODEC_SPECIFIC_CONFIGURATION);
            break;
    }

    uint8_t codec_config_type;
    uint8_t reject_reason = 0;
    ascs_specific_codec_configuration_t * specific_codec_config = &codec_config.specific_codec_configuration;

    for (codec_config_type = (uint8_t)LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY; (codec_config_type < (uint8_t) LE_AUDIO_CODEC_CONFIGURATION_TYPE_RFU) && (reject_reason==0); codec_config_type++){
        if ((specific_codec_config->codec_configuration_mask & (1 << codec_config_type) ) != 0 ){

            switch ((le_audio_codec_configuration_type_t)codec_config_type) {
                case LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY:
                    if ((specific_codec_config->sampling_frequency_index == LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_INVALID) ||
                        (specific_codec_config->sampling_frequency_index >= LE_AUDIO_CODEC_SAMPLING_FREQUENCY_INDEX_RFU)) {
                        reject_reason = ASCS_REJECT_REASON_CODEC_SPECIFIC_CONFIGURATION;
                        break;
                    }
                    break;
                case LE_AUDIO_CODEC_CONFIGURATION_TYPE_FRAME_DURATION:
                    if ((specific_codec_config->frame_duration_index == LE_AUDIO_CODEC_FRAME_DURATION_INDEX_INVALID) ||
                        (specific_codec_config->frame_duration_index >= LE_AUDIO_CODEC_FRAME_DURATION_INDEX_RFU)){
                        reject_reason = ASCS_REJECT_REASON_CODEC_SPECIFIC_CONFIGURATION;
                        break;
                    }
                    break;
                case LE_AUDIO_CODEC_CONFIGURATION_TYPE_AUDIO_CHANNEL_ALLOCATION:
                    if (specific_codec_config->audio_channel_allocation_mask >= LE_AUDIO_LOCATION_MASK_RFU){
                        reject_reason = ASCS_REJECT_REASON_CODEC_SPECIFIC_CONFIGURATION;
                        break;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    if (reject_reason != 0){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_CODEC_SPECIFIC_CONFIGURATION);
    }
}

static void ascs_control_point_operation_prepare_response_for_qos_configuration(ascs_remote_client_t * client, uint8_t ase_index, uint8_t ase_id, ascs_qos_configuration_t qos_config){
    client->response[ase_index].ase_id = ase_id;
    
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_ID, 0);
        return;
    }

    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, ASCS_OPCODE_CONFIG_QOS, ASCS_STATE_QOS_CONFIGURED)){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION, 0);
        return;
    }

    // If a client requests a Config QoS operation for an ASE that would result in more than one Sink ASE having
    // identical CIG_ID and CIS_ID parameter values for that client, 
    // or that would result in more than one Source ASE having identical CIG_ID and CIS_ID parameter values for that client, 
    // the server shall not accept the Config QoS operation for that ASE. 
    
    uint8_t i;
    for (i = 0; i < ascs_streamendpoint_chr_num; i++){
        if (streamendpoint->ase_characteristic->ase_id == client->streamendpoints[i].ase_characteristic->ase_id){
            continue;
        }
        if (streamendpoint->ase_characteristic->role != client->streamendpoints[i].ase_characteristic->role){
            continue;
        }

        if (streamendpoint->state == ASCS_STATE_CODEC_CONFIGURED){
            if (qos_config.cig_id == client->streamendpoints[i].qos_configuration.cig_id && 
                qos_config.cis_id == client->streamendpoints[i].qos_configuration.cis_id){
                ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_INVALID_ASE_CIS_MAPPING);
                return;
            } 
        }
    }

    if (qos_config.sdu_interval < 0x0000FF || qos_config.sdu_interval > 0x0FFFFF){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_SDU_INTERVAL);
        return;
    }

    if (qos_config.framing != streamendpoint->codec_configuration.framing){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_FRAMING);
        return;
    }
    
    if (qos_config.phy > LE_AUDIO_SERVER_PHY_MASK_CODED) {
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_PHY);
        return;
    }

    if (qos_config.max_sdu > 0x0FFF){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_MAXIMUM_SDU_SIZE);
        return;
    }

    if (qos_config.max_transport_latency > streamendpoint->codec_configuration.max_transport_latency_ms){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_MAX_TRANSPORT_LATENCY);
        return;
    }

    if ((qos_config.presentation_delay_us < streamendpoint->codec_configuration.presentation_delay_min_us) ||
        (qos_config.presentation_delay_us > streamendpoint->codec_configuration.presentation_delay_max_us)){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_CONFIGURATION_PARAMETER_VALUE, ASCS_REJECT_REASON_PRESENTATION_DELAY);
        return;
    }
}

static void ascs_control_point_operation_prepare_response_for_target_state(ascs_remote_client_t * client, uint8_t ase_index, uint8_t ase_id, ascs_state_t target_state){
    client->response[ase_index].ase_id = ase_id;
    
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_ID, 0);
        return;
    }

    if (target_state == ASCS_STATE_DISABLING && !ascs_streamendpoint_in_source_role(streamendpoint)) {
        target_state = ASCS_STATE_QOS_CONFIGURED;
    }

    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, client->response_opcode, target_state)){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION, 0);
        return;
    }
}

static void ascs_control_point_operation_prepare_response_for_metadata_update(ascs_remote_client_t * client, uint8_t ase_index, uint8_t ase_id, le_audio_metadata_t * metadata){
    client->response[ase_index].ase_id = ase_id;
    
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_ID, 0);
        return;
    }
    
    switch (streamendpoint->state){
        case ASCS_STATE_ENABLING:
        case ASCS_STATE_STREAMING:
            break;
        default:
            ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION, 0);
            return;
    }

    uint16_t metadata_type;
    uint8_t reject_code = 0;

    if ((metadata->metadata_mask & (1 << (uint16_t) LE_AUDIO_METADATA_TYPE_RFU) ) != 0 ){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_REJECTED_METADATA, 0);
        return;
    }

    for (metadata_type = (uint16_t)LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS; metadata_type < (uint16_t) LE_AUDIO_METADATA_TYPE_RFU; metadata_type++){
        if ((metadata->metadata_mask & (1 << metadata_type) ) != 0 ){

            switch ((le_audio_metadata_type_t)metadata_type){
                case LE_AUDIO_METADATA_TYPE_PREFERRED_AUDIO_CONTEXTS:
                    if (metadata->preferred_audio_contexts_mask >= LE_AUDIO_CONTEXT_MASK_RFU){
                        reject_code = ASCS_ERROR_CODE_INVALID_METADATA;
                    }
                    break;
                case LE_AUDIO_METADATA_TYPE_STREAMING_AUDIO_CONTEXTS:
                    if (metadata->streaming_audio_contexts_mask >= LE_AUDIO_CONTEXT_MASK_RFU){
                        reject_code = ASCS_ERROR_CODE_INVALID_METADATA;
                    }
                    break;
                case LE_AUDIO_METADATA_TYPE_PARENTAL_RATING:
                    if (metadata->parental_rating >= LE_AUDIO_PARENTAL_RATING_RFU){
                        reject_code = ASCS_ERROR_CODE_INVALID_METADATA;
                    }
                    break;
                default:
                    break;
            }
        }
    }
    if (reject_code != 0){
        ascs_update_control_point_operation_response(client, ase_index, reject_code, 0);
    }
}

static void ascs_control_point_operation_prepare_response_for_start_ready(ascs_remote_client_t * client, uint8_t ase_index, uint8_t ase_id){
    client->response[ase_index].ase_id = ase_id;
    
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_ID, 0);
        return;
    }

    if (streamendpoint->ase_characteristic->role == LE_AUDIO_ROLE_SINK){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_DIRECTION, 0);
        return;
    }

    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, ASCS_OPCODE_RECEIVER_START_READY, ASCS_STATE_STREAMING)){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION, 0);
        return;
    }
}

static void ascs_control_point_operation_prepare_response_for_stop_ready(ascs_remote_client_t * client, uint8_t ase_index, uint8_t ase_id){
    client->response[ase_index].ase_id = ase_id;
    
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_ID, 0);
        return;
    }

    if (streamendpoint->ase_characteristic->role == LE_AUDIO_ROLE_SINK){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_DIRECTION, 0);
        return;
    }

    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, ASCS_OPCODE_RECEIVER_STOP_READY, ASCS_STATE_QOS_CONFIGURED)){
        ascs_update_control_point_operation_response(client, ase_index, ASCS_ERROR_CODE_INVALID_ASE_STATE_MACHINE_TRANSITION, 0);
        return;
    }
}

static int audio_stream_control_service_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);
    
    if (attribute_handle == ascs_ase_control_point_handle){
        // write without response
        if (buffer_size < 1){
            return 0; // ASCS_ERROR_CODE_UNSUPPORTED_OPCODE;
        }

        ascs_remote_client_t * client = ascs_get_remote_client_for_con_handle(con_handle);
        if (client == NULL){
            return 0; 
        }
        ascs_client_reset_response(client);

        uint8_t pos = 0;
        client->response_opcode = (ascs_opcode_t)buffer[pos++];
        if (buffer_size < 2){
            // ASCS_ERROR_CODE_INVALID_LENGTH, set Number_of_ASEs to 0xFF
            client->response_ases_num = 0xFF;
            ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
            return 0;
        }

        client->response_ases_num = buffer[pos++];
        if ((client->response_ases_num == 0) || (client->response_ases_num > ascs_streamendpoint_chr_num)){
            // ASCS_ERROR_CODE_INVALID_LENGTH, set Number_of_ASEs to 0xFF
            client->response_ases_num = 0xFF;
            ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
            return 0;
        }

        if (!ascs_control_point_operation_has_valid_length(client->response_opcode, client->response_ases_num, &buffer[pos], buffer_size - pos)){
            // ASCS_ERROR_CODE_INVALID_LENGTH, set Number_of_ASEs to 0xFF
            client->response_ases_num = 0xFF;
            ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
            return 0;
        }

        // save offset for second buffer read:
        // - first read is used to form answer for control point operation - sent via notification
        // - second read is used to inform server on ASEs that changed values, which could in turn trigger 
        // notification on value changed
        ascs_client_codec_configuration_request_t codec_config_request;
        ascs_qos_configuration_t          qos_config;
        le_audio_metadata_t               metadata_config;
        memset(&metadata_config, 0, sizeof(le_audio_metadata_t));

        uint16_t data_offset = pos;
        uint8_t ase_id;
        uint8_t  i;

        // 1. as first schedule opcode operation answer via notification,
        // 2. then inform server on these (client) codec configuration recommendations via GATTSERVICE_SUBEVENT_ASCS_CLIENT_CODEC_CONFIGURATION_RECEIVED event 
        // 3. server should then call the API audio_stream_control_service_server_configure_codec to set the values
        // 4. and this should in return trigger notification of value change for each ASE changed separately.

        switch (client->response_opcode){
            case ASCS_OPCODE_CONFIG_CODEC:
                // first read
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    pos += ascs_util_codec_configuration_request_parse(&buffer[pos], buffer_size-pos, &codec_config_request);
                    ascs_control_point_operation_prepare_response_for_codec_configuration(client, i, ase_id, codec_config_request);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);

                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    data_offset += ascs_util_codec_configuration_request_parse(&buffer[data_offset], buffer_size-data_offset, &codec_config_request);
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_server_emit_codec_configuration_request(con_handle, ase_id, &codec_config_request);
                    }
                }
                break;

            case ASCS_OPCODE_CONFIG_QOS:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    pos += ascs_util_qos_configuration_parse(&buffer[pos], buffer_size - pos, &qos_config);
                    ascs_control_point_operation_prepare_response_for_qos_configuration(client, i, ase_id, qos_config);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
                
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    data_offset += ascs_util_qos_configuration_parse(&buffer[data_offset], buffer_size - data_offset, &qos_config);
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_util_emit_qos_configuration(ascs_event_callback, con_handle, ase_id, ASCS_STATE_RFU, &qos_config);
                    }
                }
                break;

            case ASCS_OPCODE_ENABLE:
                // The metadata values for an ASE can only be set or updated by the Initiator.
                // An Acceptor can update its Available_Audio_Contexts value at any point, but these do not appear in the ASE metadata. 
                // Nor do they result in the termination of a current stream. Any change in the Available_Audio_Contexts is only used for subsequent connection attempts.
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    pos += le_audio_util_metadata_parse(&buffer[pos], buffer_size-pos, &metadata_config);
                    ascs_control_point_operation_prepare_response_for_target_state(client, i, ase_id, ASCS_STATE_ENABLING);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
                
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    data_offset += le_audio_util_metadata_parse(&buffer[data_offset], buffer_size-data_offset, &metadata_config);
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_util_emit_metadata(ascs_event_callback, con_handle, ase_id, ASCS_STATE_RFU, &metadata_config);
                    }
                }
                break;

            case ASCS_OPCODE_RECEIVER_START_READY:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    ascs_control_point_operation_prepare_response_for_start_ready(client, i, ase_id);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);

                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_emit_client_request(con_handle, ase_id, GATTSERVICE_SUBEVENT_ASCS_CLIENT_START_READY);
                    }
                }
                break;

            case ASCS_OPCODE_DISABLE:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    ascs_control_point_operation_prepare_response_for_target_state(client, i, ase_id, ASCS_STATE_DISABLING);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);

                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_emit_client_request(con_handle, ase_id, GATTSERVICE_SUBEVENT_ASCS_CLIENT_DISABLING);
                    }
                }
                break;
            
            case ASCS_OPCODE_RECEIVER_STOP_READY:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    ascs_control_point_operation_prepare_response_for_stop_ready(client, i, ase_id);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);

                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_emit_client_request(con_handle, ase_id, GATTSERVICE_SUBEVENT_ASCS_CLIENT_STOP_READY);
                    }
                }
                break;
            
            case ASCS_OPCODE_UPDATE_METADATA:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    pos += le_audio_util_metadata_parse(&buffer[pos], buffer_size-pos, &metadata_config);
                    ascs_control_point_operation_prepare_response_for_metadata_update(client, i, ase_id, &metadata_config);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
                
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    data_offset += le_audio_util_metadata_parse(&buffer[data_offset], buffer_size-data_offset, &metadata_config);
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_util_emit_metadata(ascs_event_callback, con_handle, ase_id, ASCS_STATE_RFU, &metadata_config);
                    }
                }
                break;
            
            case ASCS_OPCODE_RELEASE:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    ascs_control_point_operation_prepare_response_for_target_state(client, i, ase_id, ASCS_STATE_RELEASING);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);

                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_emit_client_request(con_handle, ase_id, GATTSERVICE_SUBEVENT_ASCS_CLIENT_RELEASING);
                    }
                }
                break;
            case ASCS_OPCODE_RELEASED:
                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[pos++];
                    ascs_control_point_operation_prepare_response_for_target_state(client, i, ase_id, ASCS_STATE_IDLE);
                }
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);

                for (i = 0; i < client->response_ases_num; i++){
                    ase_id = buffer[data_offset++];
                    if (ascs_client_request_successfully_processed(client, i)){
                        ascs_emit_client_request(con_handle, ase_id, GATTSERVICE_SUBEVENT_ASCS_CLIENT_RELEASED);
                    }
                }
                break;

            default:
                // ASCS_ERROR_CODE_UNSUPPORTED_OPCODE, set Number_of_ASEs to 0xFF
                client->response_ases_num = 0xFF;
                ascs_schedule_task(client, ASCS_TASK_SEND_CONTROL_POINT_OPERATION_RESPONSE);
                return 0;
        }
        return 0;
    }

    ascs_remote_client_t * client = ascs_get_remote_client_for_con_handle(con_handle);
    if (client == NULL){
        client = ascs_server_add_remote_client(con_handle);
        
        if (client == NULL){
            ascs_server_emit_remote_client_connected(con_handle, ERROR_CODE_CONNECTION_LIMIT_EXCEEDED);
            log_info("There are already %d clients connected. No memory for new client.", ascs_clients_num);
            return 0;
        } else {
            client->con_handle = con_handle;
            ascs_server_emit_remote_client_connected(con_handle, ERROR_CODE_SUCCESS);
        }    
    }

    if (attribute_handle == ascs_ase_control_point_client_configuration_handle){
        client->ase_control_point_client_configuration = little_endian_read_16(buffer, 0);
        return 0;
    }

    uint8_t i;
    for (i = 0; i < ascs_streamendpoint_chr_num; i++){
        ascs_streamendpoint_t * streamendpoint = &client->streamendpoints[i];

        if (attribute_handle == streamendpoint->ase_characteristic->ase_characteristic_client_configuration_handle){
            streamendpoint->ase_characteristic_client_configuration = little_endian_read_16(buffer, 0);

#ifdef ENABLE_TESTING_SUPPORT
            printf("%s notification [index %d, con handle 0x%02X, ccc 0x%02x]\n", 
                streamendpoint->ase_characteristic_client_configuration == 0 ? "Unregistered" : "Registered",
                i, client->con_handle, streamendpoint->ase_characteristic->ase_characteristic_client_configuration_handle);
        dump_streamendpoint(client, streamendpoint);
#endif 
            return 0;
        }
    }

    return 0;
}

static void audio_stream_control_service_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    ascs_remote_client_t * client_connection;
    
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            
            client_connection = ascs_get_remote_client_for_con_handle(con_handle);
            if (client_connection != NULL){
                ascs_client_reset(client_connection);
                ascs_server_emit_remote_client_disconnected(con_handle);
            }
            break;
        default:
            break;
    }
}

static void ascs_streamenpoint_init(
    const uint8_t streamendpoint_characteristics_num, ascs_streamendpoint_characteristic_t * streamendpoint_characteristics, 
    uint16_t start_handle, uint16_t end_handle, le_audio_role_t role){

    uint16_t chr_uuid16 = ORG_BLUETOOTH_CHARACTERISTIC_SINK_ASE;
    if (role == LE_AUDIO_ROLE_SOURCE){
        chr_uuid16 = ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_ASE;
    }

    // search streamendpoints
    while ( (start_handle < end_handle) && (ascs_streamendpoint_chr_num < streamendpoint_characteristics_num)) {
        uint16_t chr_value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, chr_uuid16);
        uint16_t chr_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, chr_uuid16);
        
        if (chr_value_handle == 0){
            break;
        }

        btstack_assert(ascs_streamendpoint_chr_num < ASCS_STREAMENDPOINTS_MAX_NUM);
        ascs_streamendpoint_characteristics[ascs_streamendpoint_chr_num] = streamendpoint_characteristics[ascs_streamendpoint_chr_num];

        ascs_streamendpoint_characteristic_t * streamendpoint_chr = &ascs_streamendpoint_characteristics[ascs_streamendpoint_chr_num];
        memset(streamendpoint_chr, 0, sizeof(ascs_streamendpoint_characteristic_t));

        streamendpoint_chr->role = role;
        streamendpoint_chr->ase_id = ascs_get_next_streamendpoint_chr_id();

        streamendpoint_chr->ase_characteristic_value_handle = chr_value_handle;
        streamendpoint_chr->ase_characteristic_client_configuration_handle = chr_client_configuration_handle;
        
#ifdef ENABLE_TESTING_SUPPORT
        printf("    %s_streamendpoint_%d                 0x%02x \n", (role == LE_AUDIO_ROLE_SOURCE)? "SRC":"SNK", streamendpoint_chr->ase_id, streamendpoint_chr->ase_characteristic_value_handle);
        printf("    %s_streamendpoint_CCD_%d             0x%02x \n", (role == LE_AUDIO_ROLE_SOURCE)? "SRC":"SNK", streamendpoint_chr->ase_id, streamendpoint_chr->ase_characteristic_client_configuration_handle);
#endif
        
        start_handle = chr_client_configuration_handle + 1;
        ascs_streamendpoint_chr_num++;
    }
}

void audio_stream_control_service_server_init(
        const uint8_t streamendpoint_characteristics_num, ascs_streamendpoint_characteristic_t * streamendpoint_characteristics, 
        const uint8_t clients_num, ascs_remote_client_t * clients){

    btstack_assert(streamendpoint_characteristics_num != 0);
    btstack_assert(clients_num != 0);

    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_AUDIO_STREAM_CONTROL_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

#ifdef ENABLE_TESTING_SUPPORT
    printf("ASCS 0x%02x - 0x%02x \n", start_handle, end_handle);
#endif
    log_info("Found ASCS service 0x%02x-0x%02x", start_handle, end_handle);

    ascs_streamendpoint_chr_num = 0;
    ascs_streamendpoint_characteristics_id_counter = 0;
    ascs_streamendpoint_characteristics = streamendpoint_characteristics;

    ascs_streamenpoint_init(streamendpoint_characteristics_num, &streamendpoint_characteristics[0], 
        start_handle, end_handle, LE_AUDIO_ROLE_SINK);
    ascs_streamenpoint_init(streamendpoint_characteristics_num - ascs_streamendpoint_chr_num, &streamendpoint_characteristics[ascs_streamendpoint_chr_num], 
        start_handle, end_handle, LE_AUDIO_ROLE_SOURCE);

    ascs_clients_num = clients_num;
    ascs_clients = clients;
    memset(ascs_clients, 0, sizeof(ascs_remote_client_t) * ascs_clients_num);
    uint8_t i;
    for (i = 0; i < ascs_clients_num; i++){
        uint8_t j;
        for (j = 0; j < streamendpoint_characteristics_num; j++){
            ascs_clients[i].streamendpoints[j].state = ASCS_STATE_IDLE;
            ascs_clients[i].streamendpoints[j].ase_characteristic = &streamendpoint_characteristics[j];
            btstack_assert(ascs_clients[i].streamendpoints[j].ase_characteristic != NULL);
        }
        ascs_clients[i].con_handle = HCI_CON_HANDLE_INVALID;
    }

    ascs_ase_control_point_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ASE_CONTROL_POINT);;
    ascs_ase_control_point_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_ASE_CONTROL_POINT);
    
#ifdef ENABLE_TESTING_SUPPORT
    printf("    ase_control_point                    0x%02x \n", ascs_ase_control_point_handle);
    printf("    ase_control_point CCD                0x%02x \n", ascs_ase_control_point_client_configuration_handle);
#endif

    // register service with ATT Server
    audio_stream_control_service.start_handle   = start_handle;
    audio_stream_control_service.end_handle     = end_handle;
    audio_stream_control_service.read_callback  = &audio_stream_control_service_read_callback;
    audio_stream_control_service.write_callback = &audio_stream_control_service_write_callback;
    audio_stream_control_service.packet_handler = audio_stream_control_service_packet_handler;
    att_server_register_service_handler(&audio_stream_control_service);
}

void audio_stream_control_service_server_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    ascs_event_callback = callback;
}

static void ascs_streamendpoint_schedule_value_changed_task(ascs_remote_client_t * client, ascs_streamendpoint_t * streamendpoint){
    btstack_assert(streamendpoint != NULL);
    // skip if already scheduled
    if (streamendpoint->ase_characteristic_value_changed_w2_notify == true){
        return;
    }

    if (streamendpoint->ase_characteristic_client_configuration != 0){
        if (!streamendpoint->ase_characteristic_value_changed_w2_notify){
            streamendpoint->ase_characteristic_value_changed_w2_notify = true;
            ascs_schedule_task(client, ASCS_TASK_SEND_CODEC_CONFIGURATION_VALUE_CHANGED);
        }
    }
}

static bool ascs_streamendpoint_transit_to_state(hci_con_handle_t con_handle, uint8_t ase_id, ascs_opcode_t opcode, ascs_state_t target_state, ascs_remote_client_t ** out_client, ascs_streamendpoint_t ** out_streamendpoint){
    ascs_remote_client_t * client = ascs_get_remote_client_for_con_handle(con_handle);
    if (client == NULL){
        log_info("no client found for con_handle 0x%02x", con_handle);
        return false;
    }
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        log_info("no streamendpoint found %d for con_handle 0x%02x", ase_id, con_handle);
        return false;
    }
    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, opcode, target_state)){
        log_info("streamendpoint %d for con_handle 0x%02x in wrong state %d", ase_id, con_handle, streamendpoint->state);
        return false;
    }
    btstack_assert(streamendpoint != NULL);
    streamendpoint->state = target_state;
    *out_client = client;
    *out_streamendpoint = streamendpoint;
    return true;
}

void audio_stream_control_service_server_streamendpoint_configure_codec(hci_con_handle_t con_handle, uint8_t ase_id, ascs_codec_configuration_t codec_configuration){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;
    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_CONFIG_CODEC, ASCS_STATE_CODEC_CONFIGURED, &client, &streamendpoint)){
        return;
    }

    memcpy(&streamendpoint->codec_configuration, &codec_configuration, sizeof(ascs_codec_configuration_t));
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_streamendpoint_configure_qos(hci_con_handle_t con_handle, uint8_t ase_id, ascs_qos_configuration_t qos_configuration){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;

    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_CONFIG_QOS, ASCS_STATE_QOS_CONFIGURED, &client, &streamendpoint)){
        return;
    }
    memcpy(&streamendpoint->qos_configuration, &qos_configuration, sizeof(ascs_qos_configuration_t));
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_streamendpoint_enable(hci_con_handle_t con_handle, uint8_t ase_id){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;

    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_ENABLE, ASCS_STATE_ENABLING, &client, &streamendpoint)){
        return;
    }
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_streamendpoint_receiver_start_ready(hci_con_handle_t con_handle, uint8_t ase_id){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;

    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_RECEIVER_START_READY, ASCS_STATE_STREAMING, &client, &streamendpoint)){
        return;
    }
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}


void audio_stream_control_service_server_streamendpoint_disable(hci_con_handle_t con_handle, uint8_t ase_id){
    ascs_remote_client_t * client = ascs_get_remote_client_for_con_handle(con_handle);
    if (client == NULL){
        return;
    }
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        return;
    }

    ascs_state_t target_state;
    if (ascs_streamendpoint_in_source_role((streamendpoint))){
        target_state = ASCS_STATE_DISABLING;
    } else {
        target_state = ASCS_STATE_QOS_CONFIGURED;
    }

    if (!ascs_streamendpoint_can_transit_to_state(streamendpoint, ASCS_OPCODE_DISABLE, target_state)){
        return;
    }

    streamendpoint->state = target_state;
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_streamendpoint_receiver_stop_ready(hci_con_handle_t con_handle, uint8_t ase_id){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;

    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_RECEIVER_STOP_READY, ASCS_STATE_QOS_CONFIGURED, &client, &streamendpoint)){
        return;
    }
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_streamendpoint_release(hci_con_handle_t con_handle, uint8_t ase_id){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;

    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_RELEASE, ASCS_STATE_RELEASING, &client, &streamendpoint)){
        return;
    }
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_streamendpoint_released(hci_con_handle_t con_handle, uint8_t ase_id, bool caching){
    ascs_remote_client_t  * client;
    ascs_streamendpoint_t * streamendpoint;

    ascs_state_t target_state = ASCS_STATE_IDLE;
    if (caching){
        target_state = ASCS_STATE_QOS_CONFIGURED;
    }

    if (!ascs_streamendpoint_transit_to_state(con_handle, ase_id, ASCS_OPCODE_RELEASED, target_state, &client, &streamendpoint)){
        return;
    }
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
    // TODO reset values
}

void audio_stream_control_service_server_streamendpoint_metadata_update(hci_con_handle_t con_handle, uint8_t ase_id, le_audio_metadata_t metadata){
    ascs_remote_client_t * client = ascs_get_remote_client_for_con_handle(con_handle);
    if (client == NULL){
        return;
    }
    ascs_streamendpoint_t * streamendpoint = ascs_get_streamendpoint_for_ase_id(client, ase_id);
    if (streamendpoint == NULL){
        return;
    }
    switch (streamendpoint->state){
        case ASCS_STATE_ENABLING:
        case ASCS_STATE_STREAMING:
            memcpy(&streamendpoint->metadata, &metadata, sizeof(le_audio_metadata_t));
            ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
            break;
        default:
            return;
    }
    ascs_streamendpoint_schedule_value_changed_task(client, streamendpoint);
}

void audio_stream_control_service_server_deinit(void){
    ascs_event_callback = NULL;
}
