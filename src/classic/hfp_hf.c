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

#define BTSTACK_FILE__ "hfp_hf.c"
 
// *****************************************************************************
//
// HFP Hands-Free (HF) unit
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "classic/core.h"
#include "classic/hfp.h"
#include "classic/hfp_hf.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"

static btstack_packet_callback_registration_t hci_event_callback_registration;

static const char default_hfp_hf_service_name[] = "Hands-Free unit";
static uint16_t hfp_supported_features = HFP_DEFAULT_HF_SUPPORTED_FEATURES;
static uint8_t hfp_codecs_nr = 0;
static uint8_t hfp_codecs[HFP_MAX_NUM_CODECS];

static uint8_t hfp_indicators_nr = 0;
static uint8_t hfp_indicators[HFP_MAX_NUM_HF_INDICATORS];
static uint32_t hfp_indicators_value[HFP_MAX_NUM_HF_INDICATORS];

static uint8_t hfp_hf_speaker_gain = 9;
static uint8_t hfp_hf_microphone_gain = 9;

static btstack_packet_handler_t hfp_hf_callback;

static hfp_call_status_t hfp_call_status;
static hfp_callsetup_status_t hfp_callsetup_status;
static hfp_callheld_status_t hfp_callheld_status;

static char phone_number[25]; 

static hfp_connection_t * get_hfp_hf_connection_context_for_acl_handle(uint16_t handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->acl_handle != handle)      continue;
        if (hfp_connection->local_role != HFP_ROLE_HF) continue;
        return hfp_connection;
    }
    return NULL;
}

void hfp_hf_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        log_error("hfp_hf_register_packet_handler called with NULL callback");
        return;
    }
    hfp_hf_callback = callback;
    hfp_set_hf_callback(callback); 
}

static void hfp_hf_emit_subscriber_information(btstack_packet_handler_t callback, uint8_t event_subtype, uint8_t status, uint8_t bnip_type, const char * bnip_number){
    if (!callback) return;
    uint8_t event[31];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    event[3] = status;
    event[4] = bnip_type;
    int size = (strlen(bnip_number) < sizeof(event) - 6) ? (int) strlen(bnip_number) : (int) sizeof(event) - 6;
    strncpy((char*)&event[5], bnip_number, size);
    event[5 + size] = 0;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void hfp_hf_emit_type_and_number(btstack_packet_handler_t callback, uint8_t event_subtype, uint8_t bnip_type, const char * bnip_number){
    if (!callback) return;
    uint8_t event[30];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    event[3] = bnip_type;
    int size = (strlen(bnip_number) < sizeof(event) - 5) ? (int) strlen(bnip_number) : (int) sizeof(event) - 5;
    strncpy((char*)&event[4], bnip_number, size);
    event[4 + size] = 0;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void hfp_hf_emit_enhanced_call_status(btstack_packet_handler_t callback, uint8_t clcc_idx, uint8_t clcc_dir,
                uint8_t clcc_status, uint8_t clcc_mode, uint8_t clcc_mpty, uint8_t bnip_type, const char * bnip_number){
    if (!callback) return;
    printf("hfp_hf_emit_enhanced_call_status: %s \n", bnip_number);
    uint8_t event[36];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_ENHANCED_CALL_STATUS;
    event[pos++] = clcc_idx;
    event[pos++] = clcc_dir;
    event[pos++] = clcc_status;
    event[pos++] = clcc_mode;
    event[pos++] = clcc_mpty;
    event[pos++] = bnip_type;
    int size = (strlen(bnip_number) < sizeof(event) - pos) ? (int) strlen(bnip_number) : (int) sizeof(event) - pos;
    strncpy((char*)&event[pos], bnip_number, size);
    pos += size;
    event[pos++] = 0;
    (*callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static int has_codec_negotiation_feature(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_supported_features, HFP_HFSF_CODEC_NEGOTIATION);
    int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_CODEC_NEGOTIATION);
    return hf && ag;
}

static int has_call_waiting_and_3way_calling_feature(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_supported_features, HFP_HFSF_THREE_WAY_CALLING);
    int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_THREE_WAY_CALLING);
    return hf && ag;
}


static int has_hf_indicators_feature(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_supported_features, HFP_HFSF_HF_INDICATORS);
    int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_HF_INDICATORS);
    return hf && ag;
}

void hfp_hf_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint16_t supported_features, int wide_band_speech){
    if (!name){
        name = default_hfp_hf_service_name;
    }
    hfp_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_HANDSFREE, rfcomm_channel_nr, name);

    // Construct SupportedFeatures for SDP bitmap:
    // 
    // "The values of the “SupportedFeatures” bitmap given in Table 5.4 shall be the same as the values
    //  of the Bits 0 to 4 of the unsolicited result code +BRSF"
    //
    // Wide band speech (bit 5) requires Codec negotiation
    //
    uint16_t sdp_features = supported_features & 0x1f;
    if (wide_band_speech && (supported_features & (1 << HFP_HFSF_CODEC_NEGOTIATION))){
        sdp_features |= 1 << 5;
    }
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);    // Hands-Free Profile - SupportedFeatures
    de_add_number(service, DE_UINT, DE_SIZE_16, sdp_features);
}


static inline int hfp_hf_send_cmd(uint16_t cid, const char * cmd){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "AT%s\r\n", cmd);
    return send_str_over_rfcomm(cid, buffer);
}

static inline int hfp_hf_send_cmd_with_mark(uint16_t cid, const char * cmd, const char * mark){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "AT%s%s\r\n", cmd, mark);
    return send_str_over_rfcomm(cid, buffer);
}

static inline int hfp_hf_send_cmd_with_int(uint16_t cid, const char * cmd, uint16_t value){
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "AT%s=%d\r\n", cmd, value);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_notify_on_codecs(uint16_t cid){
    char buffer[30];
    const int size = sizeof(buffer);
    int offset = snprintf(buffer, size, "AT%s=", HFP_AVAILABLE_CODECS);
    offset += join(buffer+offset, size-offset, hfp_codecs, hfp_codecs_nr);
    offset += snprintf(buffer+offset, size-offset, "\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_activate_status_update_for_ag_indicator(uint16_t cid, uint32_t indicators_status, int indicators_nr){
    char buffer[50];
    const int size = sizeof(buffer);
    int offset = snprintf(buffer, size, "AT%s=", HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS);
    offset += join_bitmap(buffer+offset, size-offset, indicators_status, indicators_nr);
    offset += snprintf(buffer+offset, size-offset, "\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_list_supported_generic_status_indicators(uint16_t cid){
    char buffer[30];
    const int size = sizeof(buffer);
    int offset = snprintf(buffer, size, "AT%s=", HFP_GENERIC_STATUS_INDICATOR);
    offset += join(buffer+offset, size-offset, hfp_indicators, hfp_indicators_nr);
    offset += snprintf(buffer+offset, size-offset, "\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_activate_status_update_for_all_ag_indicators(uint16_t cid, uint8_t activate){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "AT%s=3,0,0,%d\r\n", HFP_ENABLE_STATUS_UPDATE_FOR_AG_INDICATORS, activate);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_initiate_outgoing_call_cmd(uint16_t cid){
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%s%s;\r\n", HFP_CALL_PHONE_NUMBER, phone_number);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_send_memory_dial_cmd(uint16_t cid, int memory_id){
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%s>%d;\r\n", HFP_CALL_PHONE_NUMBER, memory_id);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_send_chld(uint16_t cid, unsigned int number){
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "AT%s=%u\r\n", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, number);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_send_dtmf(uint16_t cid, char code){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "AT%s=%c\r\n", HFP_TRANSMIT_DTMF_CODES, code);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_ata(uint16_t cid){
    return send_str_over_rfcomm(cid, (char *) "ATA\r\n");
}

static int hfp_hf_cmd_exchange_supported_features(uint16_t cid){
    return hfp_hf_send_cmd_with_int(cid, HFP_SUPPORTED_FEATURES, hfp_supported_features);
}

static int hfp_hf_cmd_retrieve_indicators(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_INDICATOR, "=?");
}

static int hfp_hf_cmd_retrieve_indicators_status(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_INDICATOR, "?");
}

static int hfp_hf_cmd_retrieve_can_hold_call(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, "=?");
}

static int hfp_hf_cmd_retrieve_supported_generic_status_indicators(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_GENERIC_STATUS_INDICATOR, "=?");
}

static int hfp_hf_cmd_list_initital_supported_generic_status_indicators(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_GENERIC_STATUS_INDICATOR, "?");
}

static int hfp_hf_cmd_query_operator_name_format(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_QUERY_OPERATOR_SELECTION, "=3,0");
}

static int hfp_hf_cmd_query_operator_name(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_QUERY_OPERATOR_SELECTION, "?");
}

static int hfp_hf_cmd_trigger_codec_connection_setup(uint16_t cid){
    return hfp_hf_send_cmd(cid, HFP_TRIGGER_CODEC_CONNECTION_SETUP);
}

static int hfp_hf_set_microphone_gain_cmd(uint16_t cid, int gain){
    return hfp_hf_send_cmd_with_int(cid, HFP_SET_MICROPHONE_GAIN, gain);
}

static int hfp_hf_set_speaker_gain_cmd(uint16_t cid, int gain){
    return hfp_hf_send_cmd_with_int(cid, HFP_SET_SPEAKER_GAIN, gain);
}

static int hfp_hf_set_calling_line_notification_cmd(uint16_t cid, uint8_t activate){
    return hfp_hf_send_cmd_with_int(cid, HFP_ENABLE_CLIP, activate);
}

static int hfp_hf_set_echo_canceling_and_noise_reduction_cmd(uint16_t cid, uint8_t activate){
    return hfp_hf_send_cmd_with_int(cid, HFP_TURN_OFF_EC_AND_NR, activate);
}

static int hfp_hf_set_voice_recognition_notification_cmd(uint16_t cid, uint8_t activate){
    return hfp_hf_send_cmd_with_int(cid, HFP_ACTIVATE_VOICE_RECOGNITION, activate);
}

static int hfp_hf_set_call_waiting_notification_cmd(uint16_t cid, uint8_t activate){
    return hfp_hf_send_cmd_with_int(cid, HFP_ENABLE_CALL_WAITING_NOTIFICATION, activate);
}

static int hfp_hf_cmd_confirm_codec(uint16_t cid, uint8_t codec){
    return hfp_hf_send_cmd_with_int(cid, HFP_CONFIRM_COMMON_CODEC, codec);
}

static int hfp_hf_cmd_enable_extended_audio_gateway_error_report(uint16_t cid, uint8_t enable){
    return hfp_hf_send_cmd_with_int(cid, HFP_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR, enable);
}

static int hfp_hf_send_redial_last_number_cmd(uint16_t cid){
    return hfp_hf_send_cmd(cid, HFP_REDIAL_LAST_NUMBER);
}

static int hfp_hf_send_chup(uint16_t cid){
    return hfp_hf_send_cmd(cid, HFP_HANG_UP_CALL);
}

static int hfp_hf_send_binp(uint16_t cid){
    return hfp_hf_send_cmd_with_mark(cid, HFP_PHONE_NUMBER_FOR_VOICE_TAG, "=1");
}

static int hfp_hf_send_clcc(uint16_t cid){
    return hfp_hf_send_cmd(cid, HFP_LIST_CURRENT_CALLS);
}

static void hfp_emit_ag_indicator_event(btstack_packet_handler_t callback, hfp_ag_indicator_t indicator){
    if (!callback) return;
    uint8_t event[10+HFP_MAX_INDICATOR_DESC_SIZE+1];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED;
    event[pos++] = indicator.index; 
    event[pos++] = indicator.status;
    event[pos++] = indicator.min_range;
    event[pos++] = indicator.max_range;
    event[pos++] = indicator.mandatory;
    event[pos++] = indicator.enabled;
    event[pos++] = indicator.status_changed;
    strncpy((char*)&event[pos], indicator.name, HFP_MAX_INDICATOR_DESC_SIZE);
    pos += HFP_MAX_INDICATOR_DESC_SIZE;
    event[pos] = 0;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void hfp_emit_network_operator_event(btstack_packet_handler_t callback, hfp_network_opearator_t network_operator){
    if (!callback) return;
    uint8_t event[5+HFP_MAX_NETWORK_OPERATOR_NAME_SIZE+1];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED;
    event[3] = network_operator.mode;
    event[4] = network_operator.format;
    strncpy((char*)&event[5], network_operator.name, HFP_MAX_NETWORK_OPERATOR_NAME_SIZE); 
    event[5+HFP_MAX_NETWORK_OPERATOR_NAME_SIZE] = 0;
    (*callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static int hfp_hf_run_for_context_service_level_connection(hfp_connection_t * hfp_connection){
    if (hfp_connection->state >= HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return 0;
    if (hfp_connection->ok_pending) return 0;
    int done = 1;
            
    switch (hfp_connection->state){
        case HFP_EXCHANGE_SUPPORTED_FEATURES:
            hfp_hf_drop_mSBC_if_eSCO_not_supported(hfp_codecs, &hfp_codecs_nr);
            hfp_connection->state = HFP_W4_EXCHANGE_SUPPORTED_FEATURES;
            hfp_hf_cmd_exchange_supported_features(hfp_connection->rfcomm_cid);
            break;
        case HFP_NOTIFY_ON_CODECS:
            hfp_connection->state = HFP_W4_NOTIFY_ON_CODECS;
            hfp_hf_cmd_notify_on_codecs(hfp_connection->rfcomm_cid);
            break;
        case HFP_RETRIEVE_INDICATORS:
            hfp_connection->state = HFP_W4_RETRIEVE_INDICATORS;
            hfp_hf_cmd_retrieve_indicators(hfp_connection->rfcomm_cid);
            break;
        case HFP_RETRIEVE_INDICATORS_STATUS:
            hfp_connection->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
            hfp_hf_cmd_retrieve_indicators_status(hfp_connection->rfcomm_cid);
            break;
        case HFP_ENABLE_INDICATORS_STATUS_UPDATE:
            hfp_connection->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
            hfp_hf_cmd_activate_status_update_for_all_ag_indicators(hfp_connection->rfcomm_cid, 1);
            break;
        case HFP_RETRIEVE_CAN_HOLD_CALL:
            hfp_connection->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
            hfp_hf_cmd_retrieve_can_hold_call(hfp_connection->rfcomm_cid);
            break;
        case HFP_LIST_GENERIC_STATUS_INDICATORS:
            hfp_connection->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            hfp_hf_cmd_list_supported_generic_status_indicators(hfp_connection->rfcomm_cid);
            break;
        case HFP_RETRIEVE_GENERIC_STATUS_INDICATORS:
            hfp_connection->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
            hfp_hf_cmd_retrieve_supported_generic_status_indicators(hfp_connection->rfcomm_cid);
            break;
        case HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            hfp_connection->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
            hfp_hf_cmd_list_initital_supported_generic_status_indicators(hfp_connection->rfcomm_cid);
            break;
        default:
            done = 0;
            break;
    }
    return done;
}


static int hfp_hf_run_for_context_service_level_connection_queries(hfp_connection_t * hfp_connection){
    if (hfp_connection->state != HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return 0;
    if (hfp_connection->ok_pending) return 0;
    
    int done = 0;
    if (hfp_connection->enable_status_update_for_ag_indicators != 0xFF){
        hfp_connection->ok_pending = 1;
        done = 1;
        hfp_hf_cmd_activate_status_update_for_all_ag_indicators(hfp_connection->rfcomm_cid, hfp_connection->enable_status_update_for_ag_indicators);
        return done;
    };
    if (hfp_connection->change_status_update_for_individual_ag_indicators){
        hfp_connection->ok_pending = 1;
        done = 1;
        hfp_hf_cmd_activate_status_update_for_ag_indicator(hfp_connection->rfcomm_cid, 
                hfp_connection->ag_indicators_status_update_bitmap,
                hfp_connection->ag_indicators_nr);
        return done;
    }

    switch (hfp_connection->hf_query_operator_state){
        case HFP_HF_QUERY_OPERATOR_SET_FORMAT:
            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_W4_SET_FORMAT_OK;
            hfp_connection->ok_pending = 1;
            hfp_hf_cmd_query_operator_name_format(hfp_connection->rfcomm_cid);
            return 1;            
        case HFP_HF_QUERY_OPERATOR_SEND_QUERY:
            hfp_connection->hf_query_operator_state = HPF_HF_QUERY_OPERATOR_W4_RESULT;
            hfp_connection->ok_pending = 1;
            hfp_hf_cmd_query_operator_name(hfp_connection->rfcomm_cid);
            return 1;
        default:
            break;         
    }

    if (hfp_connection->enable_extended_audio_gateway_error_report){
        hfp_connection->ok_pending = 1;
        done = 1;
        hfp_hf_cmd_enable_extended_audio_gateway_error_report(hfp_connection->rfcomm_cid, hfp_connection->enable_extended_audio_gateway_error_report);
        return done;   
    }

    return done;
}

static int codecs_exchange_state_machine(hfp_connection_t * hfp_connection){
    /* events ( == commands):
        HFP_CMD_AVAILABLE_CODECS == received AT+BAC with list of codecs
        HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP:
            hf_trigger_codec_connection_setup == received BCC
            ag_trigger_codec_connection_setup == received from AG to send BCS
        HFP_CMD_HF_CONFIRMED_CODEC == received AT+BCS
    */

    if (hfp_connection->ok_pending) return 0;
    
    switch (hfp_connection->command){
        case HFP_CMD_AVAILABLE_CODECS:
            if (hfp_connection->codecs_state == HFP_CODECS_W4_AG_COMMON_CODEC) return 0;
            
            hfp_connection->codecs_state = HFP_CODECS_W4_AG_COMMON_CODEC;
            hfp_connection->ok_pending = 1;
            hfp_hf_cmd_notify_on_codecs(hfp_connection->rfcomm_cid);
            return 1;
        case HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP:
            hfp_connection->codec_confirmed = 0;
            hfp_connection->suggested_codec = 0;
            hfp_connection->negotiated_codec = 0;

            hfp_connection->codecs_state = HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE;
            hfp_connection->ok_pending = 1;
            hfp_hf_cmd_trigger_codec_connection_setup(hfp_connection->rfcomm_cid);
            break;

         case HFP_CMD_AG_SUGGESTED_CODEC:{
            if (hfp_supports_codec(hfp_connection->suggested_codec, hfp_codecs_nr, hfp_codecs)){
                hfp_connection->codec_confirmed = hfp_connection->suggested_codec;
                hfp_connection->ok_pending = 1;
                hfp_connection->codecs_state = HFP_CODECS_HF_CONFIRMED_CODEC;
                hfp_connection->negotiated_codec = hfp_connection->suggested_codec;
                log_info("hfp: codec confirmed: %s", hfp_connection->negotiated_codec == HFP_CODEC_MSBC ? "mSBC" : "CVSD");
                hfp_hf_cmd_confirm_codec(hfp_connection->rfcomm_cid, hfp_connection->codec_confirmed);
            } else {
                hfp_connection->codec_confirmed = 0;
                hfp_connection->suggested_codec = 0;
                hfp_connection->negotiated_codec = 0;
                hfp_connection->codecs_state = HFP_CODECS_W4_AG_COMMON_CODEC;
                hfp_connection->ok_pending = 1;
                hfp_hf_cmd_notify_on_codecs(hfp_connection->rfcomm_cid);

            }
            break;
        }
        default:
            break;
    }
    return 0;
}

static int hfp_hf_run_for_audio_connection(hfp_connection_t * hfp_connection){
    if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED ||
        hfp_connection->state > HFP_W2_DISCONNECT_SCO) return 0;

    if (hfp_connection->release_audio_connection){
        hfp_connection->state = HFP_W4_SCO_DISCONNECTED;
        hfp_connection->release_audio_connection = 0;
        gap_disconnect(hfp_connection->sco_handle);
        return 1;
    }

    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;

    // run codecs exchange
    int done = codecs_exchange_state_machine(hfp_connection);
    if (done) return 1;
    
    if (hfp_connection->codecs_state != HFP_CODECS_EXCHANGED) return 0;
    if (hfp_connection->establish_audio_connection){
        hfp_connection->state = HFP_W4_SCO_CONNECTED;
        hfp_connection->establish_audio_connection = 0;
        hfp_setup_synchronous_connection(hfp_connection);
        return 1;
    }

    return 0;
}


static int call_setup_state_machine(hfp_connection_t * hfp_connection){
    if (hfp_connection->hf_answer_incoming_call){
        hfp_hf_cmd_ata(hfp_connection->rfcomm_cid);
        hfp_connection->hf_answer_incoming_call = 0;
        return 1;
    }
    return 0;
}

static void hfp_run_for_context(hfp_connection_t * hfp_connection){
    if (!hfp_connection) return;
    if (!hfp_connection->rfcomm_cid) return;

    if (hfp_connection->local_role != HFP_ROLE_HF) {
        log_info("HFP HF%p, wrong role %u", hfp_connection, hfp_connection->local_role);
        return;
    }

    if (hfp_connection->hf_accept_sco && hci_can_send_command_packet_now()){

        hfp_connection->hf_accept_sco = 0;

        // notify about codec selection if not done already
        if (hfp_connection->negotiated_codec == 0){
            hfp_connection->negotiated_codec = HFP_CODEC_CVSD;
        }

        // remote supported feature eSCO is set if link type is eSCO
        // eSCO: S4 - max latency == transmission interval = 0x000c == 12 ms, 
        uint16_t max_latency;
        uint8_t  retransmission_effort;
        uint16_t packet_types;
        
        if (hci_extended_sco_link_supported() && hci_remote_esco_supported(hfp_connection->acl_handle)){
            max_latency = 0x000c;
            retransmission_effort = 0x02;
            packet_types = 0x388;
        } else {
            max_latency = 0xffff;
            retransmission_effort = 0xff;
            packet_types = 0x003f;
        }
        
        uint16_t sco_voice_setting = hci_get_sco_voice_setting();
        if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
            sco_voice_setting = 0x0043; // Transparent data
        }
        
        log_info("HFP: sending hci_accept_connection_request, sco_voice_setting 0x%02x", sco_voice_setting);
        hci_send_cmd(&hci_accept_synchronous_connection, hfp_connection->remote_addr, 8000, 8000, max_latency, 
                        sco_voice_setting, retransmission_effort, packet_types);
        return;
    }

    if (!rfcomm_can_send_packet_now(hfp_connection->rfcomm_cid)) {
        rfcomm_request_can_send_now_event(hfp_connection->rfcomm_cid);
        return;
    }
    int done = hfp_hf_run_for_context_service_level_connection(hfp_connection);
    if (!done){
        done = hfp_hf_run_for_context_service_level_connection_queries(hfp_connection);
    }
    if (!done){
        done = hfp_hf_run_for_audio_connection(hfp_connection);
    }
    if (!done){
        done = call_setup_state_machine(hfp_connection);
    }

    // don't send a new command while ok still pending
    if (hfp_connection->ok_pending) return;

    if (hfp_connection->send_microphone_gain){
        hfp_connection->send_microphone_gain = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_microphone_gain_cmd(hfp_connection->rfcomm_cid, hfp_connection->microphone_gain);
        return;
    }

    if (hfp_connection->send_speaker_gain){
        hfp_connection->send_speaker_gain = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_speaker_gain_cmd(hfp_connection->rfcomm_cid, hfp_connection->speaker_gain);
        return;
    }
    
    if (hfp_connection->hf_deactivate_calling_line_notification){
        hfp_connection->hf_deactivate_calling_line_notification = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_calling_line_notification_cmd(hfp_connection->rfcomm_cid, 0);
        return;
    }

    if (hfp_connection->hf_activate_calling_line_notification){
        hfp_connection->hf_activate_calling_line_notification = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_calling_line_notification_cmd(hfp_connection->rfcomm_cid, 1);
        return;
    }

    if (hfp_connection->hf_deactivate_echo_canceling_and_noise_reduction){
        hfp_connection->hf_deactivate_echo_canceling_and_noise_reduction = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_echo_canceling_and_noise_reduction_cmd(hfp_connection->rfcomm_cid, 0);
        return;
    }

    if (hfp_connection->hf_activate_echo_canceling_and_noise_reduction){
        hfp_connection->hf_activate_echo_canceling_and_noise_reduction = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_echo_canceling_and_noise_reduction_cmd(hfp_connection->rfcomm_cid, 1);
        return;
    }

    if (hfp_connection->hf_deactivate_voice_recognition_notification){
        hfp_connection->hf_deactivate_voice_recognition_notification = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_voice_recognition_notification_cmd(hfp_connection->rfcomm_cid, 0);
        return;
    }

    if (hfp_connection->hf_activate_voice_recognition_notification){
        hfp_connection->hf_activate_voice_recognition_notification = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_voice_recognition_notification_cmd(hfp_connection->rfcomm_cid, 1);
        return;
    }


    if (hfp_connection->hf_deactivate_call_waiting_notification){
        hfp_connection->hf_deactivate_call_waiting_notification = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_call_waiting_notification_cmd(hfp_connection->rfcomm_cid, 0);
        return;
    }

    if (hfp_connection->hf_activate_call_waiting_notification){
        hfp_connection->hf_activate_call_waiting_notification = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_set_call_waiting_notification_cmd(hfp_connection->rfcomm_cid, 1);
        return;
    }

    if (hfp_connection->hf_initiate_outgoing_call){
        hfp_connection->hf_initiate_outgoing_call = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_initiate_outgoing_call_cmd(hfp_connection->rfcomm_cid);
        return;
    }
    
    if (hfp_connection->hf_initiate_memory_dialing){
        hfp_connection->hf_initiate_memory_dialing = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_memory_dial_cmd(hfp_connection->rfcomm_cid, hfp_connection->memory_id);
        return;
    }

    if (hfp_connection->hf_initiate_redial_last_number){
        hfp_connection->hf_initiate_redial_last_number = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_redial_last_number_cmd(hfp_connection->rfcomm_cid);
        return;
    }

    if (hfp_connection->hf_send_chup){
        hfp_connection->hf_send_chup = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chup(hfp_connection->rfcomm_cid);
        return;
    }

    if (hfp_connection->hf_send_chld_0){
        hfp_connection->hf_send_chld_0 = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chld(hfp_connection->rfcomm_cid, 0);
        return;
    }

    if (hfp_connection->hf_send_chld_1){
        hfp_connection->hf_send_chld_1 = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chld(hfp_connection->rfcomm_cid, 1);
        return;
    }

    if (hfp_connection->hf_send_chld_2){
        hfp_connection->hf_send_chld_2 = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chld(hfp_connection->rfcomm_cid, 2);
        return;
    }

    if (hfp_connection->hf_send_chld_3){
        hfp_connection->hf_send_chld_3 = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chld(hfp_connection->rfcomm_cid, 3);
        return;
    }

    if (hfp_connection->hf_send_chld_4){
        hfp_connection->hf_send_chld_4 = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chld(hfp_connection->rfcomm_cid, 4);
        return;
    }

    if (hfp_connection->hf_send_chld_x){
        hfp_connection->hf_send_chld_x = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_chld(hfp_connection->rfcomm_cid, hfp_connection->hf_send_chld_x_index);
        return;
    }

    if (hfp_connection->hf_send_dtmf_code){
        char code = hfp_connection->hf_send_dtmf_code;
        hfp_connection->hf_send_dtmf_code = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_dtmf(hfp_connection->rfcomm_cid, code);
        return;
    }

    if (hfp_connection->hf_send_binp){
        hfp_connection->hf_send_binp = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_binp(hfp_connection->rfcomm_cid);
        return;
    }
    
    if (hfp_connection->hf_send_clcc){
        hfp_connection->hf_send_clcc = 0;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_clcc(hfp_connection->rfcomm_cid);
        return;
    }

    if (hfp_connection->hf_send_rrh){
        hfp_connection->hf_send_rrh = 0;
        char buffer[20];
        switch (hfp_connection->hf_send_rrh_command){
            case '?':
                sprintf(buffer, "AT%s?\r\n", HFP_RESPONSE_AND_HOLD);
                send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
                return;
            case '0':
            case '1':
            case '2':
                sprintf(buffer, "AT%s=%c\r\n", HFP_RESPONSE_AND_HOLD, hfp_connection->hf_send_rrh_command);
                send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
                return;
            default:
                break;
        }
        return;
    }

    if (hfp_connection->hf_send_cnum){
        hfp_connection->hf_send_cnum = 0;
        char buffer[20];
        sprintf(buffer, "AT%s\r\n", HFP_SUBSCRIBER_NUMBER_INFORMATION);
        send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
        return;
    }

    // update HF indicators
    if (hfp_connection->generic_status_update_bitmap){
        int i;
        for (i=0;i<hfp_indicators_nr;i++){
            if (get_bit(hfp_connection->generic_status_update_bitmap, i)){
                if (hfp_connection->generic_status_indicators[i].state){
                    hfp_connection->ok_pending = 1;
                    hfp_connection->generic_status_update_bitmap = store_bit(hfp_connection->generic_status_update_bitmap, i, 0);
                    char buffer[30];
                    sprintf(buffer, "AT%s=%u,%u\r\n", HFP_TRANSFER_HF_INDICATOR_STATUS, hfp_indicators[i], (unsigned int) hfp_indicators_value[i]);
                    send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
                } else {
                    log_info("Not sending HF indicator %u as it is disabled", hfp_indicators[i]);
                }
                return;
            }
        }
    }

    if (done) return;
    // deal with disconnect
    switch (hfp_connection->state){ 
        case HFP_W2_DISCONNECT_RFCOMM:
            hfp_connection->state = HFP_W4_RFCOMM_DISCONNECTED;
            rfcomm_disconnect(hfp_connection->rfcomm_cid);
            break;

        default:
            break;
    }
}

static void hfp_ag_slc_established(hfp_connection_t * hfp_connection){
    hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;

    hfp_emit_slc_connection_event(hfp_connection, 0, hfp_connection->acl_handle, hfp_connection->remote_addr);

    // restore volume settings
    hfp_connection->speaker_gain = hfp_hf_speaker_gain;
    hfp_connection->send_speaker_gain = 1;
    hfp_emit_event(hfp_connection, HFP_SUBEVENT_SPEAKER_VOLUME, hfp_hf_speaker_gain);
    hfp_connection->microphone_gain = hfp_hf_microphone_gain;
    hfp_connection->send_microphone_gain = 1;
    hfp_emit_event(hfp_connection, HFP_SUBEVENT_MICROPHONE_VOLUME, hfp_hf_microphone_gain);
    // enable all indicators
    int i;
    for (i=0;i<hfp_indicators_nr;i++){
        hfp_connection->generic_status_indicators[i].uuid = hfp_indicators[i];
        hfp_connection->generic_status_indicators[i].state = 1;
    }
}

static void hfp_hf_switch_on_ok(hfp_connection_t *hfp_connection){
    hfp_connection->ok_pending = 0;
    switch (hfp_connection->state){
        case HFP_W4_EXCHANGE_SUPPORTED_FEATURES:
            if (has_codec_negotiation_feature(hfp_connection)){
                hfp_connection->state = HFP_NOTIFY_ON_CODECS;
                break;
            } 
            hfp_connection->state = HFP_RETRIEVE_INDICATORS;
            break;

        case HFP_W4_NOTIFY_ON_CODECS:
            hfp_connection->state = HFP_RETRIEVE_INDICATORS;
            break;   
        
        case HFP_W4_RETRIEVE_INDICATORS:
            hfp_connection->state = HFP_RETRIEVE_INDICATORS_STATUS; 
            break;
        
        case HFP_W4_RETRIEVE_INDICATORS_STATUS:
            hfp_connection->state = HFP_ENABLE_INDICATORS_STATUS_UPDATE;
            break;
            
        case HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE:
            if (has_call_waiting_and_3way_calling_feature(hfp_connection)){
                hfp_connection->state = HFP_RETRIEVE_CAN_HOLD_CALL;
                break;
            }
            if (has_hf_indicators_feature(hfp_connection)){
                hfp_connection->state = HFP_LIST_GENERIC_STATUS_INDICATORS;
                break;
            } 
            hfp_ag_slc_established(hfp_connection);
            break;
        
        case HFP_W4_RETRIEVE_CAN_HOLD_CALL:
            if (has_hf_indicators_feature(hfp_connection)){
                hfp_connection->state = HFP_LIST_GENERIC_STATUS_INDICATORS;
                break;
            } 
            hfp_ag_slc_established(hfp_connection);
            break;
        
        case HFP_W4_LIST_GENERIC_STATUS_INDICATORS:
            hfp_connection->state = HFP_RETRIEVE_GENERIC_STATUS_INDICATORS;
            break;

        case HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS:
            hfp_connection->state = HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
            break;
                    
        case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
            hfp_ag_slc_established(hfp_connection);
            break;
        case HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
            if (hfp_connection->enable_status_update_for_ag_indicators != 0xFF){
                hfp_connection->enable_status_update_for_ag_indicators = 0xFF;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_COMPLETE, 0);
                break;
            }

            if (hfp_connection->change_status_update_for_individual_ag_indicators == 1){
                hfp_connection->change_status_update_for_individual_ag_indicators = 0;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_COMPLETE, 0);
                break;
            }

            switch (hfp_connection->hf_query_operator_state){
                case HFP_HF_QUERY_OPERATOR_W4_SET_FORMAT_OK:
                    // printf("Format set, querying name\n");
                    hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_SEND_QUERY;
                    break;
                case HPF_HF_QUERY_OPERATOR_W4_RESULT:
                    hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_FORMAT_SET;
                    hfp_emit_network_operator_event(hfp_hf_callback, hfp_connection->network_operator);
                    break;
                default:
                    break;
            }

            if (hfp_connection->enable_extended_audio_gateway_error_report){
                hfp_connection->enable_extended_audio_gateway_error_report = 0;
                break;
            }
        
            switch (hfp_connection->codecs_state){
                case HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE:
                    hfp_connection->codecs_state = HFP_CODECS_W4_AG_COMMON_CODEC;
                    break;
                case HFP_CODECS_HF_CONFIRMED_CODEC:
                    hfp_connection->codecs_state = HFP_CODECS_EXCHANGED;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    // done
    hfp_connection->command = HFP_CMD_NONE;
}

static int hfp_parser_is_end_of_line(uint8_t byte){
    return byte == '\n' || byte == '\r';
}

static void hfp_hf_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);    // ok: only called with RFCOMM_DATA_PACKET
    // assertion: size >= 1 as rfcomm.c does not deliver empty packets
    if (size < 1) return;

    hfp_connection_t * hfp_connection = get_hfp_connection_context_for_rfcomm_cid(channel);
    if (!hfp_connection) return;

    hfp_log_rfcomm_message("HFP_HF_RX", packet, size);

    // process messages byte-wise
    int pos;
    for (pos = 0; pos < size; pos++){
        hfp_parse(hfp_connection, packet[pos], 1);

        // parse until end of line "\r\n"
        if (!hfp_parser_is_end_of_line(packet[pos])) continue;
        
        int value;
        int i;
        switch (hfp_connection->command){
            case HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_hf_emit_subscriber_information(hfp_hf_callback, HFP_SUBEVENT_SUBSCRIBER_NUMBER_INFORMATION, 0, hfp_connection->bnip_type, hfp_connection->bnip_number);
                break;
            case HFP_CMD_RESPONSE_AND_HOLD_STATUS:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_RESPONSE_AND_HOLD_STATUS, btstack_atoi((char *)&hfp_connection->line_buffer[0]));
                break;
            case HFP_CMD_LIST_CURRENT_CALLS:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_hf_emit_enhanced_call_status(hfp_hf_callback, hfp_connection->clcc_idx,
                    hfp_connection->clcc_dir, hfp_connection->clcc_status, hfp_connection->clcc_mode,
                    hfp_connection->clcc_mpty, hfp_connection->bnip_type, hfp_connection->bnip_number);
                break;
            case HFP_CMD_SET_SPEAKER_GAIN:
                hfp_connection->command = HFP_CMD_NONE;
                value = btstack_atoi((char*)hfp_connection->line_buffer);
                hfp_hf_speaker_gain = value;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_SPEAKER_VOLUME, value);
                break;
            case HFP_CMD_SET_MICROPHONE_GAIN:
                hfp_connection->command = HFP_CMD_NONE;
                value = btstack_atoi((char*)hfp_connection->line_buffer);
                hfp_hf_microphone_gain = value;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_MICROPHONE_VOLUME, value);
                break;
            case HFP_CMD_AG_SENT_PHONE_NUMBER:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG, hfp_connection->bnip_number);
                break;
            case HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_hf_emit_type_and_number(hfp_hf_callback, HFP_SUBEVENT_CALL_WAITING_NOTIFICATION, hfp_connection->bnip_type, hfp_connection->bnip_number);
                break;
            case HFP_CMD_AG_SENT_CLIP_INFORMATION:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_hf_emit_type_and_number(hfp_hf_callback, HFP_SUBEVENT_CALLING_LINE_IDENTIFICATION_NOTIFICATION, hfp_connection->bnip_type, hfp_connection->bnip_number);
                break;
            case HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR:
                hfp_connection->ok_pending = 0;
                hfp_connection->command = HFP_CMD_NONE;
                hfp_connection->extended_audio_gateway_error = 0;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR, hfp_connection->extended_audio_gateway_error_value);
                break;
            case HFP_CMD_ERROR:
                hfp_connection->ok_pending = 0;
                hfp_reset_context_flags(hfp_connection);
                hfp_connection->command = HFP_CMD_NONE;
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_COMPLETE, 1);
                break;
            case HFP_CMD_OK:
                hfp_hf_switch_on_ok(hfp_connection);
                break;
            case HFP_CMD_RING:
                hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_RING);
                break;
            case HFP_CMD_TRANSFER_AG_INDICATOR_STATUS:
                for (i = 0; i < hfp_connection->ag_indicators_nr; i++){
                    if (hfp_connection->ag_indicators[i].status_changed) {
                        if (strcmp(hfp_connection->ag_indicators[i].name, "callsetup") == 0){
                            hfp_callsetup_status = (hfp_callsetup_status_t) hfp_connection->ag_indicators[i].status;
                        } else if (strcmp(hfp_connection->ag_indicators[i].name, "callheld") == 0){
                            hfp_callheld_status = (hfp_callheld_status_t) hfp_connection->ag_indicators[i].status;
                            // avoid set but not used warning
                            (void) hfp_callheld_status;
                        } else if (strcmp(hfp_connection->ag_indicators[i].name, "call") == 0){
                            hfp_call_status = (hfp_call_status_t) hfp_connection->ag_indicators[i].status;
                        }
                        hfp_connection->ag_indicators[i].status_changed = 0;
                        hfp_emit_ag_indicator_event(hfp_hf_callback, hfp_connection->ag_indicators[i]);
                        break;
                    }
                }
                break;
            case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
                for (i = 0; i < hfp_connection->ag_indicators_nr; i++){
                    hfp_emit_ag_indicator_event(hfp_hf_callback, hfp_connection->ag_indicators[i]);
                }
                hfp_connection->command = HFP_CMD_NONE;
                break; 
            default:
                break;
        }
    }
    hfp_run_for_context(hfp_connection);
}

static void hfp_run(void){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_HF) continue;
        hfp_run_for_context(hfp_connection);
    }
}

static void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type){
        case RFCOMM_DATA_PACKET:
            hfp_hf_handle_rfcomm_event(packet_type, channel, packet, size);
            break;
        case HCI_EVENT_PACKET:
            if (packet[0] == RFCOMM_EVENT_CAN_SEND_NOW){
                uint16_t rfcomm_cid = rfcomm_event_can_send_now_get_rfcomm_cid(packet);
                hfp_run_for_context(get_hfp_connection_context_for_rfcomm_cid(rfcomm_cid));
                return;
            }
            hfp_handle_rfcomm_event(packet_type, channel, packet, size, HFP_ROLE_HF);
            break;
        default:
            break;
    }
    hfp_run();
}

static void hci_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_handle_hci_event(packet_type, channel, packet, size, HFP_ROLE_HF);
    
    // allow for sco established -> ring transition and sco retry
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE) return;
    hfp_run();
}

void hfp_hf_init(uint16_t rfcomm_channel_nr){
    hfp_init();

    hci_event_callback_registration.callback = &hci_event_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    rfcomm_register_service(rfcomm_packet_handler, rfcomm_channel_nr, 0xffff);  

    // used to set packet handler for outgoing rfcomm connections - could be handled by emitting an event to us
    hfp_set_hf_rfcomm_packet_handler(&rfcomm_packet_handler);

    hfp_set_hf_run_for_context(hfp_run_for_context);

    hfp_supported_features = HFP_DEFAULT_HF_SUPPORTED_FEATURES;
    hfp_codecs_nr = 0;
    hfp_indicators_nr = 0;
    hfp_hf_speaker_gain = 9;
    hfp_hf_microphone_gain = 9;
}

void hfp_hf_init_codecs(int codecs_nr, uint8_t * codecs){
    if (codecs_nr > HFP_MAX_NUM_CODECS){
        log_error("hfp_hf_init_codecs: codecs_nr (%d) > HFP_MAX_NUM_CODECS (%d)", codecs_nr, HFP_MAX_NUM_CODECS);
        return;
    }

    hfp_codecs_nr = codecs_nr;
    int i;
    for (i=0; i<codecs_nr; i++){
        hfp_codecs[i] = codecs[i];
    }
}

void hfp_hf_init_supported_features(uint32_t supported_features){
    hfp_supported_features = supported_features;
}

void hfp_hf_init_hf_indicators(int indicators_nr, uint16_t * indicators){
    hfp_indicators_nr = indicators_nr;
    int i;
    for (i = 0; i < hfp_indicators_nr ; i++){
        hfp_indicators[i] = indicators[i];
    }
}

void hfp_hf_establish_service_level_connection(bd_addr_t bd_addr){
    hfp_establish_service_level_connection(bd_addr, BLUETOOTH_SERVICE_CLASS_HANDSFREE_AUDIO_GATEWAY, HFP_ROLE_HF);
}

void hfp_hf_release_service_level_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_release_service_level_connection(hfp_connection);
    hfp_run_for_context(hfp_connection);
}

static void hfp_hf_set_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle, uint8_t enable){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->enable_status_update_for_ag_indicators = enable;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_enable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle){
    hfp_hf_set_status_update_for_all_ag_indicators(acl_handle, 1);
}

void hfp_hf_disable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle){
    hfp_hf_set_status_update_for_all_ag_indicators(acl_handle, 0);
}

// TODO: returned ERROR - wrong format
void hfp_hf_set_status_update_for_individual_ag_indicators(hci_con_handle_t acl_handle, uint32_t indicators_status_bitmap){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->change_status_update_for_individual_ag_indicators = 1;
    hfp_connection->ag_indicators_status_update_bitmap = indicators_status_bitmap; 
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_query_operator_selection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    switch (hfp_connection->hf_query_operator_state){
        case HFP_HF_QUERY_OPERATOR_FORMAT_NOT_SET:
            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_SET_FORMAT;
            break;
        case HFP_HF_QUERY_OPERATOR_FORMAT_SET:
            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_SEND_QUERY;
            break;
        default:
            break;
    }
    hfp_run_for_context(hfp_connection);
}

static void hfp_hf_set_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle, uint8_t enable){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->enable_extended_audio_gateway_error_report = enable;
    hfp_run_for_context(hfp_connection);
}


void hfp_hf_enable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle){
    hfp_hf_set_report_extended_audio_gateway_error_result_code(acl_handle, 1);
}

void hfp_hf_disable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle){
    hfp_hf_set_report_extended_audio_gateway_error_result_code(acl_handle, 0);
}

static uint8_t hfp_hf_esco_s4_supported(hfp_connection_t * hfp_connection){
    return (hfp_connection->remote_supported_features & (1<<HFP_AGSF_ESCO_S4)) &&  (hfp_supported_features  & (1<<HFP_HFSF_ESCO_S4));
}

void hfp_hf_establish_audio_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->establish_audio_connection = 0;

    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return;
    if (hfp_connection->state >= HFP_W2_DISCONNECT_SCO) return;

    hfp_connection->trigger_codec_exchange = 0;
    hfp_connection->establish_audio_connection = 1;
    if (!has_codec_negotiation_feature(hfp_connection)){
        log_info("hfp_ag_establish_audio_connection - no codec negotiation feature, using defaults");
        hfp_connection->codecs_state = HFP_CODECS_EXCHANGED;
        hfp_connection->suggested_codec = HFP_CODEC_CVSD;
        hfp_connection->codec_confirmed = hfp_connection->suggested_codec;
        hfp_connection->negotiated_codec = hfp_connection->suggested_codec;
        hfp_init_link_settings(hfp_connection, hfp_hf_esco_s4_supported(hfp_connection));
        hfp_connection->trigger_codec_exchange = 0;
        hfp_connection->state = HFP_W4_SCO_CONNECTED;
    } else {
        switch (hfp_connection->codecs_state){
            case HFP_CODECS_W4_AG_COMMON_CODEC:
                break;
            default:
                hfp_connection->trigger_codec_exchange = 1;
                hfp_connection->command = HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP;
                break;
        } 
    }

    hfp_run_for_context(hfp_connection);
}

void hfp_hf_release_audio_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_release_audio_connection(hfp_connection);
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_answer_incoming_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }

    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_connection->hf_answer_incoming_call = 1;
        hfp_run_for_context(hfp_connection);
    } else {
        log_error("HFP HF: answering incoming call with wrong callsetup status %u", hfp_callsetup_status);
    }
}

void hfp_hf_terminate_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_chup = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_reject_incoming_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_connection->hf_send_chup = 1;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_user_busy(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_connection->hf_send_chld_0 = 1;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_end_active_and_accept_other(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS ||
        hfp_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->hf_send_chld_1 = 1;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_swap_calls(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS ||
        hfp_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->hf_send_chld_2 = 1;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_join_held_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS ||
        hfp_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->hf_send_chld_3 = 1;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_connect_calls(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS ||
        hfp_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->hf_send_chld_4 = 1;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_release_call_with_index(hci_con_handle_t acl_handle, int index){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS ||
        hfp_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->hf_send_chld_x = 1;
        hfp_connection->hf_send_chld_x_index = 10 + index;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_private_consultation_with_call(hci_con_handle_t acl_handle, int index){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS ||
        hfp_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->hf_send_chld_x = 1;
        hfp_connection->hf_send_chld_x_index = 20 + index;
        hfp_run_for_context(hfp_connection);
    }
}

void hfp_hf_dial_number(hci_con_handle_t acl_handle, char * number){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_initiate_outgoing_call = 1;
    snprintf(phone_number, sizeof(phone_number), "%s", number);
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_dial_memory(hci_con_handle_t acl_handle, int memory_id){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_initiate_memory_dialing = 1;
    hfp_connection->memory_id = memory_id;

    hfp_run_for_context(hfp_connection);
}

void hfp_hf_redial_last_number(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_initiate_redial_last_number = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_activate_call_waiting_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_activate_call_waiting_notification = 1;
    hfp_run_for_context(hfp_connection);
}


void hfp_hf_deactivate_call_waiting_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_deactivate_call_waiting_notification = 1;
    hfp_run_for_context(hfp_connection);
}


void hfp_hf_activate_calling_line_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_activate_calling_line_notification = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_deactivate_calling_line_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_deactivate_calling_line_notification = 1;
    hfp_run_for_context(hfp_connection);
}


void hfp_hf_activate_echo_canceling_and_noise_reduction(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_activate_echo_canceling_and_noise_reduction = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_deactivate_echo_canceling_and_noise_reduction(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_deactivate_echo_canceling_and_noise_reduction = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_activate_voice_recognition_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_activate_voice_recognition_notification = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_deactivate_voice_recognition_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    hfp_connection->hf_deactivate_voice_recognition_notification = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_set_microphone_gain(hci_con_handle_t acl_handle, int gain){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_connection->microphone_gain == gain) return;
    if (gain < 0 || gain > 15){
        log_info("Valid range for a gain is [0..15]. Currently sent: %d", gain);
        return;
    }
    hfp_connection->microphone_gain = gain;
    hfp_connection->send_microphone_gain = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_set_speaker_gain(hci_con_handle_t acl_handle, int gain){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (hfp_connection->speaker_gain == gain) return;
    if (gain < 0 || gain > 15){
        log_info("Valid range for a gain is [0..15]. Currently sent: %d", gain);
        return;
    }
    hfp_connection->speaker_gain = gain;
    hfp_connection->send_speaker_gain = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_send_dtmf_code(hci_con_handle_t acl_handle, char code){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }

    hfp_connection->hf_send_dtmf_code = code;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_request_phone_number_for_voice_tag(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_binp = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_query_current_call_status(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_clcc = 1;
    hfp_run_for_context(hfp_connection);
}


void hfp_hf_rrh_query_status(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '?';
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_rrh_hold_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '0';
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_rrh_accept_held_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '1';
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_rrh_reject_held_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '2';
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_query_subscriber_number(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->hf_send_cnum = 1;
    hfp_run_for_context(hfp_connection);
}

void hfp_hf_set_hf_indicator(hci_con_handle_t acl_handle, int assigned_number, int value){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return;
    }
    // find index for assigned number
    int i;
    for (i = 0; i < hfp_indicators_nr ; i++){
        if (hfp_indicators[i] == assigned_number){
            // set value
            hfp_indicators_value[i] = value;
            // mark for update
            if (hfp_connection->state > HFP_LIST_GENERIC_STATUS_INDICATORS){
                hfp_connection->generic_status_update_bitmap |= (1<<i);
                // send update
                hfp_run_for_context(hfp_connection);
            }
            return;
        }
    }
}

int hfp_hf_in_band_ringtone_active(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        log_error("HFP HF: ACL handle 0x%2x is not found.", acl_handle);
        return 0;
    }
    return get_bit(hfp_connection->remote_supported_features, HFP_AGSF_IN_BAND_RING_TONE);
}
