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

#define BTSTACK_FILE__ "hfp_hf.c"
 
// *****************************************************************************
//
// HFP Hands-Free (HF) unit
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
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

// const
static const char hfp_hf_default_service_name[] = "Hands-Free unit";

// globals

// higher layer callbacks
static btstack_packet_handler_t hfp_hf_callback;

static btstack_packet_callback_registration_t hfp_hf_hci_event_callback_registration;

static uint16_t hfp_hf_supported_features;
static uint8_t  hfp_hf_codecs_nr;
static uint8_t  hfp_hf_codecs[HFP_MAX_NUM_CODECS];

static uint8_t  hfp_hf_indicators_nr;
static uint8_t  hfp_hf_indicators[HFP_MAX_NUM_INDICATORS];
static uint32_t hfp_hf_indicators_value[HFP_MAX_NUM_INDICATORS];

static uint8_t  hfp_hf_speaker_gain;
static uint8_t  hfp_hf_microphone_gain;
static char hfp_hf_phone_number[25];

// Apple Accessory Information
static uint16_t hfp_hf_apple_vendor_id;
static uint16_t hfp_hf_apple_product_id;
static const char * hfp_hf_apple_version;
static uint8_t  hfp_hf_apple_features;
static int8_t hfp_hf_apple_battery_level;
static int8_t hfp_hf_apple_docked;


static int has_codec_negotiation_feature(hfp_connection_t * hfp_connection){
	int hf = get_bit(hfp_hf_supported_features, HFP_HFSF_CODEC_NEGOTIATION);
	int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_CODEC_NEGOTIATION);
	return hf && ag;
}

static int has_call_waiting_and_3way_calling_feature(hfp_connection_t * hfp_connection){
	int hf = get_bit(hfp_hf_supported_features, HFP_HFSF_THREE_WAY_CALLING);
	int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_THREE_WAY_CALLING);
	return hf && ag;
}


static int has_hf_indicators_feature(hfp_connection_t * hfp_connection){
	int hf = get_bit(hfp_hf_supported_features, HFP_HFSF_HF_INDICATORS);
	int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_HF_INDICATORS);
	return hf && ag;
}

static bool hfp_hf_vra_flag_supported(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_hf_supported_features, HFP_HFSF_VOICE_RECOGNITION_FUNCTION);
    int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION);
    return hf && ag;
}

static bool hfp_hf_enhanced_vra_flag_supported(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_hf_supported_features, HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS);
    int ag = get_bit(hfp_connection->remote_supported_features, HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS);
    return hf && ag;
}

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

/* emit functions */

static void hfp_hf_emit_subscriber_information(const hfp_connection_t * hfp_connection, uint8_t status){
    if (hfp_hf_callback == NULL) return;
    uint16_t bnip_number_len = btstack_min((uint16_t) strlen(hfp_connection->bnip_number), sizeof(hfp_connection->bnip_number)-1);
    uint8_t event[7 + sizeof(hfp_connection->bnip_number)];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = 6 + bnip_number_len;
    event[2] = HFP_SUBEVENT_SUBSCRIBER_NUMBER_INFORMATION;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
    event[5] = status;
    event[6] = hfp_connection->bnip_type;
    memcpy(&event[7], hfp_connection->bnip_number, bnip_number_len);
    event[7 + bnip_number_len] = 0;
    (*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, 8 + bnip_number_len);
}

static void hfp_hf_emit_type_number_alpha(const hfp_connection_t * hfp_connection, uint8_t event_subtype){
    if (hfp_hf_callback == NULL) return;
    uint16_t bnip_number_len = btstack_min((uint16_t) strlen(hfp_connection->bnip_number), sizeof(hfp_connection->bnip_number)-1);
    // 10 fixed - 1 (bnip_number_len <= sizeof(hfp_connection->bnip_number)-1) + 1 (trailing \0 for line buffer)
    uint8_t event[10 + sizeof(hfp_connection->bnip_number) + sizeof(hfp_connection->line_buffer)];
    uint8_t alpha_len = hfp_connection->clip_have_alpha ? (uint16_t) strlen((const char *) hfp_connection->line_buffer) : 0;
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = 8 + bnip_number_len + alpha_len;
    event[pos++] = event_subtype;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
    pos += 2;
    event[pos++] = hfp_connection->bnip_type;
    event[pos++] = bnip_number_len + 1;
    memcpy(&event[7], hfp_connection->bnip_number, bnip_number_len);
    pos += bnip_number_len;
    event[pos++] = 0;
    event[pos++] = alpha_len + 1;
    memcpy(&event[pos], hfp_connection->line_buffer, alpha_len);
    pos += alpha_len;
    event[pos++] = 0;
    (*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void hfp_hf_emit_enhanced_call_status(const hfp_connection_t * hfp_connection){
    if (hfp_hf_callback == NULL) return;
    
    uint16_t bnip_number_len = (uint16_t) strlen((const char *) hfp_connection->bnip_number);
    uint8_t event[11 + HFP_BNEP_NUM_MAX_SIZE];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = 10 + bnip_number_len + 1;
    event[2] = HFP_SUBEVENT_ENHANCED_CALL_STATUS;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
    event[5] = hfp_connection->clcc_idx;
    event[6] = hfp_connection->clcc_dir;
    event[7] = hfp_connection->clcc_status;
    event[8] = hfp_connection->clcc_mode;
    event[9] = hfp_connection->clcc_mpty;
    event[10] = hfp_connection->bnip_type;
    memcpy(&event[11], hfp_connection->bnip_number, bnip_number_len + 1);
    
    (*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, 11 + bnip_number_len + 1);
}

static void hfp_emit_ag_indicator_mapping_event(const hfp_connection_t * hfp_connection, const hfp_ag_indicator_t * indicator){
    if (hfp_hf_callback == NULL) return;
    uint8_t event[8 + HFP_MAX_INDICATOR_DESC_SIZE];
    uint16_t indicator_len = btstack_min((uint16_t) strlen(indicator->name), HFP_MAX_INDICATOR_DESC_SIZE-1);
    event[0] = HCI_EVENT_HFP_META;
    event[1] = 7 + indicator_len;
    event[2] = HFP_SUBEVENT_AG_INDICATOR_MAPPING;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
    event[5] = indicator->index;
    event[6] = indicator->min_range;
    event[7] = indicator->max_range;
    memcpy(&event[8], indicator->name, indicator_len);
    event[8+indicator_len] = 0;
    (*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, 9 + indicator_len);
}

static void hfp_emit_ag_indicator_status_event(const hfp_connection_t * hfp_connection, const hfp_ag_indicator_t * indicator){
	if (hfp_hf_callback == NULL) return;
	uint8_t event[12+HFP_MAX_INDICATOR_DESC_SIZE];
    uint16_t indicator_len = btstack_min((uint16_t) strlen(indicator->name), HFP_MAX_INDICATOR_DESC_SIZE-1);
	event[0] = HCI_EVENT_HFP_META;
	event[1] = 11 + indicator_len;
	event[2] = HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
	event[5] = indicator->index;
	event[6] = indicator->status;
	event[7] = indicator->min_range;
	event[8] = indicator->max_range;
	event[9] = indicator->mandatory;
	event[10] = indicator->enabled;
	event[11] = indicator->status_changed;
	memcpy(&event[12], indicator->name, indicator_len);
	event[12+indicator_len] = 0;
	(*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, 13 + indicator_len);
}

static void hfp_emit_network_operator_event(const hfp_connection_t * hfp_connection){
    if (hfp_hf_callback == NULL) return;
    uint16_t operator_len = btstack_min((uint16_t) strlen(hfp_connection->network_operator.name), HFP_MAX_NETWORK_OPERATOR_NAME_SIZE-1);
	uint8_t event[7+HFP_MAX_NETWORK_OPERATOR_NAME_SIZE];
	event[0] = HCI_EVENT_HFP_META;
	event[1] = sizeof(event) - 2;
    event[2] = HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
	event[5] = hfp_connection->network_operator.mode;
	event[6] = hfp_connection->network_operator.format;
	memcpy(&event[7], hfp_connection->network_operator.name, operator_len);
	event[7+operator_len] = 0;
	(*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, 8 + operator_len);
}


static void hfp_hf_emit_enhanced_voice_recognition_text(hfp_connection_t * hfp_connection){
    btstack_assert(hfp_connection != NULL);
    uint8_t event[HFP_MAX_VR_TEXT_SIZE + 11];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_ENHANCED_VOICE_RECOGNITION_AG_MESSAGE;
    little_endian_store_16(event, pos, hfp_connection->acl_handle);
    pos += 2;
    little_endian_store_16(event, pos, hfp_connection->ag_msg.text_id);
    pos += 2;
    event[pos++] = hfp_connection->ag_msg.text_type;
    event[pos++] = hfp_connection->ag_msg.text_operation;
    
    // length, zero ending is already in message
    uint8_t * value = &hfp_connection->line_buffer[0];
    uint16_t  value_length = hfp_connection->ag_vra_msg_length;

    little_endian_store_16(event, pos, value_length);
    pos += 2; 
    memcpy(&event[pos], value, value_length);
    pos += value_length; 

    (*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void hfp_hf_emit_custom_command_event(hfp_connection_t * hfp_connection){
    btstack_assert(sizeof(hfp_connection->line_buffer) < (255-5));

    uint16_t line_len = (uint16_t) strlen((const char*)hfp_connection->line_buffer) + 1;
    uint8_t event[7 + sizeof(hfp_connection->line_buffer)];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = 5 + line_len;
    event[2] = HFP_SUBEVENT_CUSTOM_AT_COMMAND;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
    little_endian_store_16(event, 5, hfp_connection->custom_at_command_id);
    memcpy(&event[7], hfp_connection->line_buffer, line_len);
    (*hfp_hf_callback)(HCI_EVENT_PACKET, 0, event, 7 + line_len);
}

/* send commands */

static inline int hfp_hf_send_cmd(uint16_t cid, const char * cmd){
    char buffer[20];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s\r", cmd);
    return send_str_over_rfcomm(cid, buffer);
}

static inline int hfp_hf_send_cmd_with_mark(uint16_t cid, const char * cmd, const char * mark){
    char buffer[20];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s%s\r", cmd, mark);
    return send_str_over_rfcomm(cid, buffer);
}

static inline int hfp_hf_send_cmd_with_int(uint16_t cid, const char * cmd, uint16_t value){
    char buffer[40];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%d\r", cmd, value);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_notify_on_codecs(uint16_t cid){
    char buffer[30];
    const int size = sizeof(buffer);
    int offset = btstack_snprintf_assert_complete(buffer, size, "AT%s=", HFP_AVAILABLE_CODECS);
    offset += join(buffer+offset, size-offset, hfp_hf_codecs, hfp_hf_codecs_nr);
    offset += btstack_snprintf_assert_complete(buffer+offset, size-offset, "\r");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_activate_status_update_for_ag_indicator(uint16_t cid, uint32_t indicators_status, int indicators_nr){
    char buffer[50];
    const int size = sizeof(buffer);
    int offset = btstack_snprintf_assert_complete(buffer, size, "AT%s=", HFP_UPDATE_ENABLE_STATUS_FOR_INDIVIDUAL_AG_INDICATORS);
    offset += join_bitmap(buffer+offset, size-offset, indicators_status, indicators_nr);
    offset += btstack_snprintf_assert_complete(buffer+offset, size-offset, "\r");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_list_supported_generic_status_indicators(uint16_t cid){
    char buffer[30];
    const int size = sizeof(buffer);
    int offset = btstack_snprintf_assert_complete(buffer, size, "AT%s=", HFP_GENERIC_STATUS_INDICATOR);
    offset += join(buffer+offset, size-offset, hfp_hf_indicators, hfp_hf_indicators_nr);
    offset += btstack_snprintf_assert_complete(buffer+offset, size-offset, "\r");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_activate_status_update_for_all_ag_indicators(uint16_t cid, uint8_t activate){
    char buffer[20];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=3,0,0,%d\r", HFP_ENABLE_STATUS_UPDATE_FOR_AG_INDICATORS, activate);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_initiate_outgoing_call_cmd(uint16_t cid){
    char buffer[40];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "%s%s;\r", HFP_CALL_PHONE_NUMBER, hfp_hf_phone_number);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_send_memory_dial_cmd(uint16_t cid, int memory_id){
    char buffer[40];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "%s>%d;\r", HFP_CALL_PHONE_NUMBER, memory_id);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_send_chld(uint16_t cid, unsigned int number){
    char buffer[40];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%u\r", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES, number);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_send_dtmf(uint16_t cid, char code){
    char buffer[20];
    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%c\r", HFP_TRANSMIT_DTMF_CODES, code);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_hf_cmd_ata(uint16_t cid){
    return send_str_over_rfcomm(cid, (char *) "ATA\r");
}

static int hfp_hf_cmd_exchange_supported_features(uint16_t cid){
    return hfp_hf_send_cmd_with_int(cid, HFP_SUPPORTED_FEATURES, hfp_hf_supported_features);
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

/* state machines */

static bool hfp_hf_run_for_context_service_level_connection(hfp_connection_t * hfp_connection){
    if (hfp_connection->state >= HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return false;
    if (hfp_connection->ok_pending) return false;

    log_info("hfp_hf_run_for_context_service_level_connection state %d\n", hfp_connection->state);
    switch (hfp_connection->state){
        case HFP_EXCHANGE_SUPPORTED_FEATURES:
            hfp_hf_drop_mSBC_if_eSCO_not_supported(hfp_hf_codecs, &hfp_hf_codecs_nr);
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
            return false;
    }
    return true;
}


static bool hfp_hf_run_for_context_service_level_connection_queries(hfp_connection_t * hfp_connection){
    if (hfp_connection->state != HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return false;
    if (hfp_connection->ok_pending){
        return false;
    }

    if (hfp_connection->enable_status_update_for_ag_indicators != 0xFF){
        hfp_connection->ok_pending = 1;
        hfp_hf_cmd_activate_status_update_for_all_ag_indicators(hfp_connection->rfcomm_cid, hfp_connection->enable_status_update_for_ag_indicators);
        return true;
    };

    if (hfp_connection->change_status_update_for_individual_ag_indicators){
        hfp_connection->ok_pending = 1;
        hfp_hf_cmd_activate_status_update_for_ag_indicator(hfp_connection->rfcomm_cid,
                hfp_connection->ag_indicators_status_update_bitmap,
                hfp_connection->ag_indicators_nr);
        return true;
    }

    switch (hfp_connection->hf_query_operator_state){
        case HFP_HF_QUERY_OPERATOR_SET_FORMAT:
            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_W4_SET_FORMAT_OK;
            hfp_connection->ok_pending = 1;
            hfp_hf_cmd_query_operator_name_format(hfp_connection->rfcomm_cid);
            return true;
        case HFP_HF_QUERY_OPERATOR_SEND_QUERY:
            hfp_connection->hf_query_operator_state = HPF_HF_QUERY_OPERATOR_W4_RESULT;
            hfp_connection->ok_pending = 1;
            hfp_hf_cmd_query_operator_name(hfp_connection->rfcomm_cid);
            return true;
        default:
            break;         
    }

    if (hfp_connection->enable_extended_audio_gateway_error_report){
        hfp_connection->ok_pending = 1;
        hfp_hf_cmd_enable_extended_audio_gateway_error_report(hfp_connection->rfcomm_cid, hfp_connection->enable_extended_audio_gateway_error_report);
        return true;
    }

    return false;
}

static bool hfp_hf_voice_recognition_state_machine(hfp_connection_t * hfp_connection){
    if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) {
        return false;
    }

    if (hfp_connection->ok_pending == 1){
        return false;
    }
    // voice recognition activated from AG
    if (hfp_connection->command == HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION){
        switch(hfp_connection->vra_state_requested){
            case HFP_VRA_W4_VOICE_RECOGNITION_ACTIVATED:
            case HFP_VRA_W4_VOICE_RECOGNITION_OFF:
            case HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
                // ignore AG command, continue to wait for OK
                return false;
            
            default:
                if (hfp_connection->ag_vra_msg_length > 0){
                    hfp_hf_emit_enhanced_voice_recognition_text(hfp_connection);
                    hfp_connection->ag_vra_msg_length = 0;
                    break;
                }
                switch(hfp_connection->ag_vra_state){
                    case HFP_VOICE_RECOGNITION_STATE_AG_READY:
                        switch (hfp_connection->ag_vra_status){
                            case 0:
                                hfp_connection->vra_state_requested = HFP_VRA_W4_VOICE_RECOGNITION_OFF;
                                break;
                            case 1:
                                hfp_connection->vra_state_requested = HFP_VRA_W4_VOICE_RECOGNITION_ACTIVATED;
                                break;
                            case 2:
                                hfp_connection->vra_state_requested = HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO;
                                break;
                            default:
                                break;
                        }
                        break;
                    default:
                        // state messages from AG
                        hfp_emit_enhanced_voice_recognition_state_event(hfp_connection, ERROR_CODE_SUCCESS);
                        hfp_connection->ag_vra_state = HFP_VOICE_RECOGNITION_STATE_AG_READY;
                        break;
                }
                break;
        }
        hfp_connection->command = HFP_CMD_NONE;
    }

    switch (hfp_connection->vra_state_requested){
        case HFP_VRA_W2_SEND_VOICE_RECOGNITION_OFF:
            hfp_connection->vra_state_requested = HFP_VRA_W4_VOICE_RECOGNITION_OFF;
            hfp_connection->ok_pending = 1;
            hfp_hf_set_voice_recognition_notification_cmd(hfp_connection->rfcomm_cid, 0);
            return true;

        case HFP_VRA_W2_SEND_VOICE_RECOGNITION_ACTIVATED:
            hfp_connection->vra_state_requested = HFP_VRA_W4_VOICE_RECOGNITION_ACTIVATED;
            hfp_connection->ok_pending = 1;
            hfp_hf_set_voice_recognition_notification_cmd(hfp_connection->rfcomm_cid, 1);
            return true;

        case HFP_VRA_W2_SEND_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
            hfp_connection->vra_state_requested = HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO;
            hfp_connection->ok_pending = 1;
            hfp_hf_set_voice_recognition_notification_cmd(hfp_connection->rfcomm_cid, 2);
            return true;

        case HFP_VRA_W4_VOICE_RECOGNITION_OFF:
            hfp_connection->vra_state = HFP_VRA_VOICE_RECOGNITION_OFF;
            hfp_connection->vra_state_requested = hfp_connection->vra_state;
            hfp_connection->activate_voice_recognition = false; 
            if (hfp_connection->activate_voice_recognition){
                hfp_connection->enhanced_voice_recognition_enabled = hfp_hf_enhanced_vra_flag_supported(hfp_connection);
                hfp_hf_activate_voice_recognition(hfp_connection->acl_handle);
            } else {
                hfp_emit_voice_recognition_disabled(hfp_connection, ERROR_CODE_SUCCESS);
            }
            break;

        case HFP_VRA_W4_VOICE_RECOGNITION_ACTIVATED:
            hfp_connection->vra_state = HFP_VRA_VOICE_RECOGNITION_ACTIVATED;
            hfp_connection->vra_state_requested = hfp_connection->vra_state;
            hfp_connection->activate_voice_recognition = false;
            if (hfp_connection->deactivate_voice_recognition){
                hfp_hf_deactivate_voice_recognition(hfp_connection->acl_handle);
            } else {
                hfp_connection->enhanced_voice_recognition_enabled = hfp_hf_enhanced_vra_flag_supported(hfp_connection);
                if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED){
                    hfp_emit_voice_recognition_enabled(hfp_connection, ERROR_CODE_SUCCESS);
                } else {
                    // postpone VRA event to simplify application logic
                    hfp_connection->emit_vra_enabled_after_audio_established = true;
                }
            }
            break;

        case HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
            hfp_connection->vra_state = HFP_VRA_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO;
            hfp_connection->vra_state_requested = hfp_connection->vra_state;
            hfp_connection->activate_voice_recognition = false;
            if (hfp_connection->deactivate_voice_recognition){
                hfp_hf_deactivate_voice_recognition(hfp_connection->acl_handle);
            } else {
                hfp_emit_enhanced_voice_recognition_hf_ready_for_audio_event(hfp_connection, ERROR_CODE_SUCCESS);
            }
            break;

        default:
            break;
    }
    return false;
}


static bool codecs_exchange_state_machine(hfp_connection_t * hfp_connection){
    if (hfp_connection->ok_pending) return false;

    if (hfp_connection->trigger_codec_exchange){
		hfp_connection->trigger_codec_exchange = false;

		hfp_connection->ok_pending = 1;
		hfp_hf_cmd_trigger_codec_connection_setup(hfp_connection->rfcomm_cid);
		return true;
    }

    if (hfp_connection->hf_send_codec_confirm){
		hfp_connection->hf_send_codec_confirm = false;

		hfp_connection->ok_pending = 1;
		hfp_hf_cmd_confirm_codec(hfp_connection->rfcomm_cid, hfp_connection->codec_confirmed);
		return true;
    }

    if (hfp_connection->hf_send_supported_codecs){
		hfp_connection->hf_send_supported_codecs = false;

		hfp_connection->ok_pending = 1;
		hfp_hf_cmd_notify_on_codecs(hfp_connection->rfcomm_cid);
		return true;
    }

    return false;
}

static bool hfp_hf_run_for_audio_connection(hfp_connection_t * hfp_connection){
    if ((hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) ||
        (hfp_connection->state > HFP_W2_DISCONNECT_SCO)) return false;

    if (hfp_connection->release_audio_connection){
        hfp_connection->state = HFP_W4_SCO_DISCONNECTED;
        hfp_connection->release_audio_connection = 0;
        gap_disconnect(hfp_connection->sco_handle);
        return true;
    }

    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return false;

    // run codecs exchange
    bool done = codecs_exchange_state_machine(hfp_connection);
    if (done) return true;
    
    if (hfp_connection->codecs_state != HFP_CODECS_EXCHANGED) return false;
    if (hfp_sco_setup_active()) return false;
    if (hfp_connection->establish_audio_connection){
        hfp_connection->state = HFP_W4_SCO_CONNECTED;
        hfp_connection->establish_audio_connection = 0;
        hfp_setup_synchronous_connection(hfp_connection);
        return true;
    }
    return false;
}


static bool call_setup_state_machine(hfp_connection_t * hfp_connection){

	if (hfp_connection->ok_pending) return false;

    if (hfp_connection->hf_answer_incoming_call){
        hfp_connection->hf_answer_incoming_call = 0;
        hfp_hf_cmd_ata(hfp_connection->rfcomm_cid);
        return true;
    }
    return false;
}

static void hfp_hf_run_for_context(hfp_connection_t * hfp_connection){

	btstack_assert(hfp_connection != NULL);
	btstack_assert(hfp_connection->local_role == HFP_ROLE_HF);

	// during SDP query, RFCOMM CID is not set
	if (hfp_connection->rfcomm_cid == 0) return;

    // emit postponed VRA event
    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED && hfp_connection->emit_vra_enabled_after_audio_established){
        hfp_connection->emit_vra_enabled_after_audio_established = false;
        hfp_emit_voice_recognition_enabled(hfp_connection, ERROR_CODE_SUCCESS);
    }

	// assert command could be sent
	if (hci_can_send_command_packet_now() == 0) return;

#ifdef ENABLE_CC256X_ASSISTED_HFP
    // WBS Disassociate
    if (hfp_connection->cc256x_send_wbs_disassociate){
        hfp_connection->cc256x_send_wbs_disassociate = false;
        hci_send_cmd(&hci_ti_wbs_disassociate);
        return;
    }
    // Write Codec Config
    if (hfp_connection->cc256x_send_write_codec_config){
        hfp_connection->cc256x_send_write_codec_config = false;
        hfp_cc256x_write_codec_config(hfp_connection);
        return;
    }
    // WBS Associate
    if (hfp_connection->cc256x_send_wbs_associate){
        hfp_connection->cc256x_send_wbs_associate = false;
        hci_send_cmd(&hci_ti_wbs_associate, hfp_connection->acl_handle);
        return;
    }
#endif
#ifdef ENABLE_BCM_PCM_WBS
    // Enable WBS
    if (hfp_connection->bcm_send_enable_wbs){
        hfp_connection->bcm_send_enable_wbs = false;
        hci_send_cmd(&hci_bcm_enable_wbs, 1, 2);
        return;
    }
    // Write I2S/PCM params
    if (hfp_connection->bcm_send_write_i2spcm_interface_param){
        hfp_connection->bcm_send_write_i2spcm_interface_param = false;
        hfp_bcm_write_i2spcm_interface_param(hfp_connection);
        return;
    }
    // Disable WBS
    if (hfp_connection->bcm_send_disable_wbs){
        hfp_connection->bcm_send_disable_wbs = false;
        hci_send_cmd(&hci_bcm_enable_wbs, 0, 2);
        return;
    }
#endif
#ifdef ENABLE_RTK_PCM_WBS
    if (hfp_connection->rtk_send_sco_config){
        hfp_connection->rtk_send_sco_config = false;
        if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
            log_info("RTK SCO: 16k + mSBC");
            hci_send_cmd(&hci_rtk_configure_sco_routing, 0x81, 0x90, 0x00, 0x00, 0x1a, 0x0c, 0x00, 0x00, 0x41);
        } else {
            log_info("RTK SCO: 16k + CVSD");
            hci_send_cmd(&hci_rtk_configure_sco_routing, 0x81, 0x90, 0x00, 0x00, 0x1a, 0x0c, 0x0c, 0x00, 0x01);
        }
        return;
    }
#endif
#ifdef ENABLE_NXP_PCM_WBS
    if (hfp_connection->nxp_start_audio_handle != HCI_CON_HANDLE_INVALID){
        hci_con_handle_t sco_handle = hfp_connection->nxp_start_audio_handle;
        hfp_connection->nxp_start_audio_handle = HCI_CON_HANDLE_INVALID;
        hci_send_cmd(&hci_nxp_host_pcm_i2s_audio_config, 0, 0, sco_handle, 0);
        return;
    }
    if (hfp_connection->nxp_stop_audio_handle != HCI_CON_HANDLE_INVALID){
        hci_con_handle_t sco_handle = hfp_connection->nxp_stop_audio_handle;
        hfp_connection->nxp_stop_audio_handle = HCI_CON_HANDLE_INVALID;
        hci_send_cmd(&hci_nxp_host_pcm_i2s_audio_config, 1, 0, sco_handle, 0);
        return;
    }
#endif
#if defined (ENABLE_CC256X_ASSISTED_HFP) || defined (ENABLE_BCM_PCM_WBS)
    if (hfp_connection->state == HFP_W4_WBS_SHUTDOWN){
        hfp_finalize_connection_context(hfp_connection);
        return;
    }
#endif

    if (hfp_connection->accept_sco && (hfp_sco_setup_active() == false)){
        bool incoming_eSCO = hfp_connection->accept_sco == 2;
        // notify about codec selection if not done already
        if (hfp_connection->negotiated_codec == 0){
            hfp_connection->negotiated_codec = HFP_CODEC_CVSD;
        }
        hfp_accept_synchronous_connection(hfp_connection, incoming_eSCO);
        return;
    }

    if (!rfcomm_can_send_packet_now(hfp_connection->rfcomm_cid)) {
        rfcomm_request_can_send_now_event(hfp_connection->rfcomm_cid);
        return;
    }

    // we can send at least an RFCOMM packet or a HCI Command now

    bool done = hfp_hf_run_for_context_service_level_connection(hfp_connection);
    if (!done){
        done = hfp_hf_run_for_context_service_level_connection_queries(hfp_connection);
    }
    if (!done){
        done = hfp_hf_run_for_audio_connection(hfp_connection);
    }
    if (!done){
        done = hfp_hf_voice_recognition_state_machine(hfp_connection);
    }
    if (!done){
        done = call_setup_state_machine(hfp_connection);
    }

    // don't send a new command while ok still pending or SLC is not established
    if (hfp_connection->ok_pending || (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED)){
        return;
    }
    
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
        hfp_connection->response_pending_for_command = HFP_CMD_TURN_OFF_EC_AND_NR;
        hfp_connection->ok_pending = 1;
        hfp_hf_send_cmd_with_int(hfp_connection->rfcomm_cid, HFP_TURN_OFF_EC_AND_NR, 0);
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
        // notify client that dtmf was sent
        char buffer[2];
        buffer[0] = code;
        buffer[1] = 0;
        hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_TRANSMIT_DTMF_CODES, buffer);
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
                btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s?\r",
                         HFP_RESPONSE_AND_HOLD);
                buffer[sizeof(buffer) - 1] = 0;
                send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
                return;
            case '0':
            case '1':
            case '2':
                btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%c\r",
                         HFP_RESPONSE_AND_HOLD,
                         hfp_connection->hf_send_rrh_command);
                buffer[sizeof(buffer) - 1] = 0;
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
        btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s\r",
                 HFP_SUBSCRIBER_NUMBER_INFORMATION);
        buffer[sizeof(buffer) - 1] = 0;
        send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
        return;
    }

    // update HF indicators
    if (hfp_connection->generic_status_update_bitmap){
        int i;
        for (i=0; i < hfp_hf_indicators_nr; i++){
            if (get_bit(hfp_connection->generic_status_update_bitmap, i)){
                hfp_connection->generic_status_update_bitmap = store_bit(hfp_connection->generic_status_update_bitmap, i, 0);
                if (hfp_connection->generic_status_indicators[i].state){
                    hfp_connection->ok_pending = 1;
                    char buffer[30];
                    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%u,%u\r",
                             HFP_TRANSFER_HF_INDICATOR_STATUS,
                             hfp_hf_indicators[i],
                             (unsigned int)hfp_hf_indicators_value[i]);
                    buffer[sizeof(buffer) - 1] = 0;
                    send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
                } else {
                    log_info("Not sending HF indicator %u as it is disabled", hfp_hf_indicators[i]);
                }
                return;
            }
        }
    }

    if (hfp_connection->send_apple_information){
        hfp_connection->send_apple_information = false;
        hfp_connection->ok_pending = 1;
        hfp_connection->response_pending_for_command = HFP_CMD_APPLE_ACCESSORY_INFORMATION;
        char buffer[40];
        btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=%04x-%04x-%s,%u\r", HFP_APPLE_ACCESSORY_INFORMATION,
                 hfp_hf_apple_vendor_id, hfp_hf_apple_product_id, hfp_hf_apple_version, hfp_hf_apple_features);
        (void) send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
        return;
    }

    if (hfp_connection->apple_accessory_commands_supported){
        uint8_t num_apple_values = 0;
        uint8_t first_key = 0;
        uint8_t first_value = 0;
        if (hfp_connection->apple_accessory_battery_level >= 0){
            num_apple_values++;
            first_key = 1;
            first_value = hfp_connection->apple_accessory_battery_level;
        }
        if (hfp_connection->apple_accessory_docked >= 0){
            num_apple_values++;
            first_key = 2;
            first_value = hfp_connection->apple_accessory_docked;
        }
        if (num_apple_values > 0){
            char buffer[40];
            switch (num_apple_values){
                case 1:
                    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=1,%u,%u\r", HFP_APPLE_ACCESSORY_STATE,
                             first_key, first_value);
                    break;
                case 2:
                    btstack_snprintf_assert_complete(buffer, sizeof(buffer), "AT%s=2,1,%u,2,%u\r", HFP_APPLE_ACCESSORY_STATE,
                             hfp_connection->apple_accessory_battery_level, hfp_connection->apple_accessory_docked);
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
            // clear
            hfp_connection->apple_accessory_battery_level = -1;
            hfp_connection->apple_accessory_docked = -1;
            // construct
            hfp_connection->ok_pending = 1;
            hfp_connection->response_pending_for_command = HFP_CMD_APPLE_ACCESSORY_STATE;
            (void) send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
            return;
        }
    }

    if (hfp_connection->send_custom_message != NULL){
        const char * message = hfp_connection->send_custom_message;
        hfp_connection->send_custom_message = NULL;
        hfp_connection->ok_pending = 1;
        hfp_connection->response_pending_for_command = HFP_CMD_CUSTOM_MESSAGE;
        send_str_over_rfcomm(hfp_connection->rfcomm_cid, message);
        return;
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


static void hfp_hf_apple_trigger_send(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role == HFP_ROLE_HF) {
            hfp_connection->apple_accessory_battery_level = hfp_hf_apple_battery_level;
            hfp_connection->apple_accessory_docked = hfp_hf_apple_docked;
        }
    }
}

static void hfp_hf_slc_established(hfp_connection_t * hfp_connection){
    hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;

    hfp_emit_slc_connection_event(hfp_connection->local_role, 0, hfp_connection->acl_handle, hfp_connection->remote_addr);

    uint8_t i;
    for (i = 0; i < hfp_connection->ag_indicators_nr; i++){
        hfp_emit_ag_indicator_mapping_event(hfp_connection, &hfp_connection->ag_indicators[i]);
    }

    for (i = 0; i < hfp_connection->ag_indicators_nr; i++){
        hfp_connection->ag_indicators[i].status_changed = 0;
        hfp_emit_ag_indicator_status_event(hfp_connection, &hfp_connection->ag_indicators[i]);
    }
    
    hfp_connection->apple_accessory_commands_supported = false;
    hfp_connection->send_apple_information = hfp_hf_apple_vendor_id != 0;

    // restore volume settings
    hfp_connection->speaker_gain = hfp_hf_speaker_gain;
    hfp_connection->send_speaker_gain = 1;
    hfp_emit_event(hfp_connection, HFP_SUBEVENT_SPEAKER_VOLUME, hfp_hf_speaker_gain);
    hfp_connection->microphone_gain = hfp_hf_microphone_gain;
    hfp_connection->send_microphone_gain = 1;
    hfp_emit_event(hfp_connection, HFP_SUBEVENT_MICROPHONE_VOLUME, hfp_hf_microphone_gain);
}

static void hfp_hf_handle_suggested_codec(hfp_connection_t * hfp_connection){
	if (hfp_supports_codec(hfp_connection->suggested_codec, hfp_hf_codecs_nr, hfp_hf_codecs)){
		// Codec supported, confirm
		hfp_connection->negotiated_codec = hfp_connection->suggested_codec;
		hfp_connection->codec_confirmed = hfp_connection->suggested_codec;
		log_info("hfp: codec confirmed: %s", (hfp_connection->negotiated_codec == HFP_CODEC_MSBC) ? "mSBC" : "CVSD");
		hfp_connection->codecs_state = HFP_CODECS_HF_CONFIRMED_CODEC;

		hfp_connection->hf_send_codec_confirm = true;
	} else {
		// Codec not supported, send supported codecs
		hfp_connection->codec_confirmed = 0;
		hfp_connection->suggested_codec = 0;
		hfp_connection->negotiated_codec = 0;
		hfp_connection->codecs_state = HFP_CODECS_W4_AG_COMMON_CODEC;

		hfp_connection->hf_send_supported_codecs = true;
	}
}
static void hfp_hf_apple_extension_supported(hfp_connection_t * hfp_connection, bool supported){
    hfp_connection->apple_accessory_commands_supported = supported;
    log_info("Apple Extension supported: %u", supported);
    hfp_emit_event(hfp_connection, HFP_SUBEVENT_APPLE_EXTENSION_SUPPORTED, hfp_hf_microphone_gain);
}

static bool hfp_hf_switch_on_ok_pending(hfp_connection_t *hfp_connection, uint8_t status){
    bool event_emited = true;

    // cache state and reset
    hfp_command_t response_pending_for_command = hfp_connection->response_pending_for_command;
    hfp_connection->response_pending_for_command = HFP_CMD_NONE;
    hfp_connection->command = HFP_CMD_NONE;
    hfp_connection->ok_pending = 0;

    switch (response_pending_for_command){
        case HFP_CMD_TURN_OFF_EC_AND_NR:
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_ECHO_CANCELING_AND_NOISE_REDUCTION_DEACTIVATE, status);
            break;
        case HFP_CMD_CUSTOM_MESSAGE:
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_CUSTOM_AT_MESSAGE_SENT, status);
            break;
        case HFP_CMD_APPLE_ACCESSORY_INFORMATION:
            hfp_hf_apple_extension_supported(hfp_connection, true);
            break;
        default:
            event_emited = false;
    
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
                    hfp_hf_slc_established(hfp_connection);
                    break;
                
                case HFP_W4_RETRIEVE_CAN_HOLD_CALL:
                    if (has_hf_indicators_feature(hfp_connection)){
                        hfp_connection->state = HFP_LIST_GENERIC_STATUS_INDICATORS;
                        break;
                    }
                    hfp_hf_slc_established(hfp_connection);
                    break;
                
                case HFP_W4_LIST_GENERIC_STATUS_INDICATORS:
                    hfp_connection->state = HFP_RETRIEVE_GENERIC_STATUS_INDICATORS;
                    break;

                case HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS:
                    hfp_connection->state = HFP_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS;
                    break;
                            
                case HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS:
                    hfp_hf_slc_established(hfp_connection);
                    break;
                case HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
                    if (hfp_connection->enable_status_update_for_ag_indicators != 0xFF){
                        hfp_connection->enable_status_update_for_ag_indicators = 0xFF;
                        hfp_emit_event(hfp_connection, HFP_SUBEVENT_COMPLETE, ERROR_CODE_SUCCESS);
                        break;
                    }

                    if (hfp_connection->change_status_update_for_individual_ag_indicators == 1){
                        hfp_connection->change_status_update_for_individual_ag_indicators = 0;
                        hfp_emit_event(hfp_connection, HFP_SUBEVENT_COMPLETE, ERROR_CODE_SUCCESS);
                        break;
                    }

                    switch (hfp_connection->hf_query_operator_state){
                        case HFP_HF_QUERY_OPERATOR_W4_SET_FORMAT_OK:
                            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_SEND_QUERY;
                            break;
                        case HPF_HF_QUERY_OPERATOR_W4_RESULT:
                            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_FORMAT_SET;
                            hfp_emit_network_operator_event(hfp_connection);
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
                    hfp_hf_voice_recognition_state_machine(hfp_connection);
                    break;
                case HFP_AUDIO_CONNECTION_ESTABLISHED:
                    hfp_hf_voice_recognition_state_machine(hfp_connection);
                    break;
                default:
                    break;
            }
            break;
    }

    return event_emited;
}   

static bool hfp_is_ringing(hfp_callsetup_status_t callsetup_status){
    switch (callsetup_status){
        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE:
            return true;
        default:
            return false;
   }
}

static void hfp_hf_handle_transfer_ag_indicator_status(hfp_connection_t * hfp_connection) {
    uint16_t i;

    for (i = 0; i < hfp_connection->ag_indicators_nr; i++){
        if (hfp_connection->ag_indicators[i].status_changed) {
            if (strcmp(hfp_connection->ag_indicators[i].name, "callsetup") == 0){
                hfp_callsetup_status_t new_hf_callsetup_status = (hfp_callsetup_status_t) hfp_connection->ag_indicators[i].status;
                bool ringing_old = hfp_is_ringing(hfp_connection->hf_callsetup_status);
                bool ringing_new = hfp_is_ringing(new_hf_callsetup_status);
                if (ringing_old != ringing_new){
                    if (ringing_new){
                        hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_START_RINGING);
                    } else {
                        hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_STOP_RINGING);
                    } 
                }
                hfp_connection->hf_callsetup_status = new_hf_callsetup_status;
            } else if (strcmp(hfp_connection->ag_indicators[i].name, "callheld") == 0){
                hfp_connection->hf_callheld_status = (hfp_callheld_status_t) hfp_connection->ag_indicators[i].status;
                // avoid set but not used warning
                (void) hfp_connection->hf_callheld_status;
            } else if (strcmp(hfp_connection->ag_indicators[i].name, "call") == 0){
                hfp_call_status_t new_hf_call_status = (hfp_call_status_t) hfp_connection->ag_indicators[i].status;
                if (hfp_connection->hf_call_status != new_hf_call_status){
                    if (new_hf_call_status == HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS){
                        hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_CALL_TERMINATED);
                    } else {
                        hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_CALL_ANSWERED);
                    }
                }
                hfp_connection->hf_call_status = new_hf_call_status;
            }
            hfp_connection->ag_indicators[i].status_changed = 0;
            hfp_emit_ag_indicator_status_event(hfp_connection, &hfp_connection->ag_indicators[i]);
            break;
        }
    }
}

static void hfp_hf_handle_rfcomm_command(hfp_connection_t * hfp_connection){
    int value;
    int i;
    bool event_emited;

    // last argument is still in line_buffer

    switch (hfp_connection->command){
        case HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_hf_emit_subscriber_information(hfp_connection, 0);
            break;
        case HFP_CMD_RESPONSE_AND_HOLD_STATUS:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_RESPONSE_AND_HOLD_STATUS, btstack_atoi((char *)&hfp_connection->line_buffer[0]));
            break;
        case HFP_CMD_LIST_CURRENT_CALLS:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_hf_emit_enhanced_call_status(hfp_connection);
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
            hfp_hf_emit_type_number_alpha(hfp_connection, HFP_SUBEVENT_CALL_WAITING_NOTIFICATION);
            break;
        case HFP_CMD_AG_SENT_CLIP_INFORMATION:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_hf_emit_type_number_alpha(hfp_connection, HFP_SUBEVENT_CALLING_LINE_IDENTIFICATION_NOTIFICATION);
            break;
        case HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_connection->ok_pending = 0;
            hfp_connection->extended_audio_gateway_error = 0;
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR, hfp_connection->extended_audio_gateway_error_value);
            break;
        case HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION:
            break;
        case HFP_CMD_ERROR:
            switch (hfp_connection->state){
                case HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED:
                    switch (hfp_connection->codecs_state){
                        case HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE:
                            hfp_reset_context_flags(hfp_connection);
                            hfp_emit_sco_connection_established(hfp_connection, HFP_REMOTE_REJECTS_AUDIO_CONNECTION,
                                                                hfp_connection->negotiated_codec, 0, 0);
                            return;
                        default:
                            break;
                    }
                    break;
                default: 
                    break;
            }           
            
            switch (hfp_connection->response_pending_for_command){
                case HFP_CMD_APPLE_ACCESSORY_INFORMATION:
                    hfp_connection->response_pending_for_command = HFP_CMD_NONE;
                    hfp_hf_apple_extension_supported(hfp_connection, false);
                    return;
                default:
                    break;
            }

            // handle error response for voice activation (HF initiated)
            switch(hfp_connection->vra_state_requested){
                case HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
                    hfp_emit_enhanced_voice_recognition_hf_ready_for_audio_event(hfp_connection, ERROR_CODE_UNSPECIFIED_ERROR);
                    break;
                default:
                    if (hfp_connection->vra_state_requested == hfp_connection->vra_state){
                        break;
                    }
                    hfp_connection->vra_state_requested = hfp_connection->vra_state;
                    hfp_emit_voice_recognition_enabled(hfp_connection, ERROR_CODE_UNSPECIFIED_ERROR);
                    hfp_reset_context_flags(hfp_connection);
                    return;
            }
            event_emited = hfp_hf_switch_on_ok_pending(hfp_connection, ERROR_CODE_UNSPECIFIED_ERROR);
            if (!event_emited){
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_COMPLETE, ERROR_CODE_UNSPECIFIED_ERROR);
            }
            hfp_reset_context_flags(hfp_connection);
            break;

        case HFP_CMD_OK:
            hfp_hf_switch_on_ok_pending(hfp_connection, ERROR_CODE_SUCCESS);
            break;
        case HFP_CMD_RING:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_RING);
            break;
        case HFP_CMD_TRANSFER_AG_INDICATOR_STATUS:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_hf_handle_transfer_ag_indicator_status(hfp_connection);
            break;
        case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
            hfp_connection->command = HFP_CMD_NONE;
            // report status after SLC established
            if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED){
                break;
            }
            for (i = 0; i < hfp_connection->ag_indicators_nr; i++){
                hfp_emit_ag_indicator_mapping_event(hfp_connection, &hfp_connection->ag_indicators[i]);
            }
            break;
    	case HFP_CMD_AG_SUGGESTED_CODEC:
            hfp_connection->command = HFP_CMD_NONE;
    		hfp_hf_handle_suggested_codec(hfp_connection);
			break;
        case HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING:
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_IN_BAND_RING_TONE, get_bit(hfp_connection->remote_supported_features, HFP_AGSF_IN_BAND_RING_TONE));
            break;
        case HFP_CMD_CUSTOM_MESSAGE:
            hfp_connection->command = HFP_CMD_NONE;
            hfp_parser_reset_line_buffer(hfp_connection);
            log_info("Custom AT Command ID 0x%04x", hfp_connection->custom_at_command_id);
            hfp_hf_emit_custom_command_event(hfp_connection);
            break;
        default:
            break;
    }
}

static int hfp_parser_is_end_of_line(uint8_t byte){
	return (byte == '\n') || (byte == '\r');
}

static void hfp_hf_handle_rfcomm_data(hfp_connection_t * hfp_connection, uint8_t *packet, uint16_t size){
    // assertion: size >= 1 as rfcomm.c does not deliver empty packets
    if (size < 1) return;
    
    hfp_log_rfcomm_message("HFP_HF_RX", packet, size);
#ifdef ENABLE_HFP_AT_MESSAGES
    hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_AT_MESSAGE_RECEIVED, (char *) packet);
#endif

    // process messages byte-wise
    uint8_t pos;
    for (pos = 0; pos < size; pos++){
        hfp_parse(hfp_connection, packet[pos], 1);
        // parse until end of line "\r" or "\n"
        if (!hfp_parser_is_end_of_line(packet[pos])) continue;
        hfp_hf_handle_rfcomm_command(hfp_connection);   
    }
}

static void hfp_hf_run(void){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_HF) continue;
        hfp_hf_run_for_context(hfp_connection);
    }
}

static void hfp_hf_rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_connection_t * hfp_connection;
    switch (packet_type){
        case RFCOMM_DATA_PACKET:
            hfp_connection = get_hfp_connection_context_for_rfcomm_cid(channel);
            if (!hfp_connection) return;
            hfp_hf_handle_rfcomm_data(hfp_connection, packet, size);
            hfp_hf_run_for_context(hfp_connection);
            return;
        case HCI_EVENT_PACKET:
            if (packet[0] == RFCOMM_EVENT_CAN_SEND_NOW){
                uint16_t rfcomm_cid = rfcomm_event_can_send_now_get_rfcomm_cid(packet);
                hfp_hf_run_for_context(get_hfp_connection_context_for_rfcomm_cid(rfcomm_cid));
                return;
            }
            hfp_handle_rfcomm_event(packet_type, channel, packet, size, HFP_ROLE_HF);
            break;
        default:
            break;
    }
    hfp_hf_run();
}

static void hfp_hf_hci_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_handle_hci_event(packet_type, channel, packet, size, HFP_ROLE_HF);
    hfp_hf_run();
}

static void hfp_hf_set_defaults(void){
    hfp_hf_supported_features = HFP_DEFAULT_HF_SUPPORTED_FEATURES;
    hfp_hf_codecs_nr = 0;
    hfp_hf_speaker_gain = 9;
    hfp_hf_microphone_gain = 9;
    hfp_hf_indicators_nr = 0;
    // Apple extension
    hfp_hf_apple_docked = -1;
    hfp_hf_apple_battery_level = -1;
}

uint8_t hfp_hf_set_default_microphone_gain(uint8_t gain){
    if (gain > 15){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    hfp_hf_microphone_gain = gain;
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_set_default_speaker_gain(uint8_t gain){
    if (gain > 15){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    hfp_hf_speaker_gain = gain;
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_init(uint8_t rfcomm_channel_nr){
    uint8_t status = rfcomm_register_service(hfp_hf_rfcomm_packet_handler, rfcomm_channel_nr, 0xffff);
    if (status != ERROR_CODE_SUCCESS){
        return status;
    }

    hfp_init();
    hfp_hf_set_defaults();

    hfp_hf_hci_event_callback_registration.callback = &hfp_hf_hci_event_packet_handler;
    hci_add_event_handler(&hfp_hf_hci_event_callback_registration);

    // used to set packet handler for outgoing rfcomm connections - could be handled by emitting an event to us
    hfp_set_hf_rfcomm_packet_handler(&hfp_hf_rfcomm_packet_handler);
    return ERROR_CODE_SUCCESS;
}

void hfp_hf_deinit(void){
    hfp_deinit();
    hfp_hf_set_defaults();

    hfp_hf_callback = NULL;
    hfp_hf_apple_vendor_id = 0;
    (void) memset(&hfp_hf_hci_event_callback_registration, 0, sizeof(btstack_packet_callback_registration_t));
    (void) memset(hfp_hf_phone_number, 0, sizeof(hfp_hf_phone_number));
}

void hfp_hf_init_codecs(uint8_t codecs_nr, const uint8_t * codecs){
    btstack_assert(codecs_nr <= HFP_MAX_NUM_CODECS);
    if (codecs_nr > HFP_MAX_NUM_CODECS) return;

    hfp_hf_codecs_nr = codecs_nr;
    uint8_t i;
    for (i=0; i<codecs_nr; i++){
        hfp_hf_codecs[i] = codecs[i];
    }
}

void hfp_hf_init_supported_features(uint32_t supported_features){
    hfp_hf_supported_features = supported_features;
}

void hfp_hf_init_hf_indicators(int indicators_nr, const uint16_t * indicators){
    btstack_assert(hfp_hf_indicators_nr < HFP_MAX_NUM_INDICATORS);
    if (hfp_hf_indicators_nr > HFP_MAX_NUM_INDICATORS) return;

    hfp_hf_indicators_nr = indicators_nr;
    int i;
    for (i = 0; i < hfp_hf_indicators_nr ; i++){
        hfp_hf_indicators[i] = (uint8_t) indicators[i];
    }

    // store copy in hfp to setup generic_status_indicators during SLC
    hfp_set_hf_indicators(indicators_nr, hfp_hf_indicators);
}

uint8_t hfp_hf_establish_service_level_connection(bd_addr_t bd_addr){
    return hfp_establish_service_level_connection(bd_addr, BLUETOOTH_SERVICE_CLASS_HANDSFREE_AUDIO_GATEWAY, HFP_ROLE_HF);
}

uint8_t hfp_hf_release_service_level_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_trigger_release_service_level_connection(hfp_connection);
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hfp_hf_set_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle, uint8_t enable){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->enable_status_update_for_ag_indicators = enable;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_enable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle){
    return hfp_hf_set_status_update_for_all_ag_indicators(acl_handle, 1);
}

uint8_t hfp_hf_disable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle){
    return hfp_hf_set_status_update_for_all_ag_indicators(acl_handle, 0);
}

// TODO: returned ERROR - wrong format
uint8_t hfp_hf_set_status_update_for_individual_ag_indicators(hci_con_handle_t acl_handle, uint32_t indicators_status_bitmap){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->change_status_update_for_individual_ag_indicators = 1;
    hfp_connection->ag_indicators_status_update_bitmap = indicators_status_bitmap;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_query_operator_selection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    switch (hfp_connection->hf_query_operator_state){
        case HFP_HF_QUERY_OPERATOR_FORMAT_NOT_SET:
            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_SET_FORMAT;
            break;
        case HFP_HF_QUERY_OPERATOR_FORMAT_SET:
            hfp_connection->hf_query_operator_state = HFP_HF_QUERY_OPERATOR_SEND_QUERY;
            break;
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hfp_hf_set_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle, uint8_t enable){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->enable_extended_audio_gateway_error_report = enable;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}


uint8_t hfp_hf_enable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle){
    return hfp_hf_set_report_extended_audio_gateway_error_result_code(acl_handle, 1);
}

uint8_t hfp_hf_disable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle){
    return hfp_hf_set_report_extended_audio_gateway_error_result_code(acl_handle, 0);
}

static uint8_t hfp_hf_esco_s4_supported(hfp_connection_t * hfp_connection){
    return (hfp_connection->remote_supported_features & (1<<HFP_AGSF_ESCO_S4)) &&  (hfp_hf_supported_features & (1 << HFP_HFSF_ESCO_S4));
}

uint8_t hfp_hf_establish_audio_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (hfp_connection->state >= HFP_W2_DISCONNECT_SCO){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    if (has_codec_negotiation_feature(hfp_connection)) {
        switch (hfp_connection->codecs_state) {
            case HFP_CODECS_W4_AG_COMMON_CODEC:
                break;
            case HFP_CODECS_EXCHANGED:
                hfp_connection->trigger_codec_exchange = 1;
                break;
            default:
                hfp_connection->codec_confirmed = 0;
                hfp_connection->suggested_codec = 0;
                hfp_connection->negotiated_codec = 0;
                hfp_connection->codecs_state = HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE;
                hfp_connection->trigger_codec_exchange = 1;
                break;
        }
    } else {
        log_info("no codec negotiation feature, use CVSD");
        hfp_connection->codecs_state = HFP_CODECS_EXCHANGED;
        hfp_connection->suggested_codec = HFP_CODEC_CVSD;
        hfp_connection->codec_confirmed = hfp_connection->suggested_codec;
        hfp_connection->negotiated_codec = hfp_connection->suggested_codec;
        hfp_init_link_settings(hfp_connection, hfp_hf_esco_s4_supported(hfp_connection));
        hfp_connection->establish_audio_connection = 1;
        hfp_connection->state = HFP_W4_SCO_CONNECTED;
    }

    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_release_audio_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (hfp_connection->vra_state == HFP_VRA_VOICE_RECOGNITION_ACTIVATED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    uint8_t status = hfp_trigger_release_audio_connection(hfp_connection);
    if (status == ERROR_CODE_SUCCESS){
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_answer_incoming_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_connection->hf_answer_incoming_call = 1;
        hfp_hf_run_for_context(hfp_connection);
    } else {
        log_error("HFP HF: answering incoming call with wrong callsetup status %u", hfp_connection->hf_callsetup_status);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_terminate_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_chup = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_reject_incoming_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_connection->hf_send_chup = 1;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_user_busy(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
        hfp_connection->hf_send_chld_0 = 1;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_terminate_held_calls(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    hfp_connection->hf_send_chld_0 = 1;
    hfp_hf_run_for_context(hfp_connection);

    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_end_active_and_accept_other(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) ||
        (hfp_connection->hf_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){
        hfp_connection->hf_send_chld_1 = 1;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_swap_calls(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) ||
        (hfp_connection->hf_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){
        hfp_connection->hf_send_chld_2 = 1;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_join_held_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) ||
        (hfp_connection->hf_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){
        hfp_connection->hf_send_chld_3 = 1;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_connect_calls(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) ||
        (hfp_connection->hf_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){
        hfp_connection->hf_send_chld_4 = 1;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_release_call_with_index(hci_con_handle_t acl_handle, int index){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) ||
        (hfp_connection->hf_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){
        hfp_connection->hf_send_chld_x = 1;
        hfp_connection->hf_send_chld_x_index = 10 + index;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_private_consultation_with_call(hci_con_handle_t acl_handle, int index){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if ((hfp_connection->hf_callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) ||
        (hfp_connection->hf_call_status == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){
        hfp_connection->hf_send_chld_x = 1;
        hfp_connection->hf_send_chld_x_index = 20 + index;
        hfp_hf_run_for_context(hfp_connection);
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_dial_number(hci_con_handle_t acl_handle, char * number){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_initiate_outgoing_call = 1;
    btstack_snprintf_assert_complete(hfp_hf_phone_number, sizeof(hfp_hf_phone_number), "%s", number);
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_dial_memory(hci_con_handle_t acl_handle, int memory_id){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_initiate_memory_dialing = 1;
    hfp_connection->memory_id = memory_id;

    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_redial_last_number(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_initiate_redial_last_number = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_activate_call_waiting_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_activate_call_waiting_notification = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}


uint8_t hfp_hf_deactivate_call_waiting_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_deactivate_call_waiting_notification = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}


uint8_t hfp_hf_activate_calling_line_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_activate_calling_line_notification = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_deactivate_calling_line_notification(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    hfp_connection->hf_deactivate_calling_line_notification = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_deactivate_echo_canceling_and_noise_reduction(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (get_bit(hfp_connection->remote_supported_features, HFP_AGSF_EC_NR_FUNCTION) == 0){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hfp_connection->hf_deactivate_echo_canceling_and_noise_reduction = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_activate_voice_recognition(hci_con_handle_t acl_handle){
     hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED || hfp_connection->state > HFP_AUDIO_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    bool enhanced_vra_supported = hfp_hf_enhanced_vra_flag_supported(hfp_connection);
    bool legacy_vra_supported   = hfp_hf_vra_flag_supported(hfp_connection);
    if (!enhanced_vra_supported && !legacy_vra_supported){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    switch (hfp_connection->vra_state){
        case HFP_VRA_VOICE_RECOGNITION_OFF:
        case HFP_VRA_W2_SEND_VOICE_RECOGNITION_OFF:
            hfp_connection->vra_state_requested = HFP_VRA_W2_SEND_VOICE_RECOGNITION_ACTIVATED;
            hfp_connection->enhanced_voice_recognition_enabled = enhanced_vra_supported;
            break;
        case HFP_VRA_W4_VOICE_RECOGNITION_OFF:
            hfp_connection->activate_voice_recognition = true;
            break;
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_enhanced_voice_recognition_report_ready_for_audio(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (hfp_connection->emit_vra_enabled_after_audio_established){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (hfp_connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    bool enhanced_vra_supported = hfp_hf_enhanced_vra_flag_supported(hfp_connection);
    if (!enhanced_vra_supported){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }

    switch (hfp_connection->vra_state){
        case HFP_VRA_VOICE_RECOGNITION_ACTIVATED:
        case HFP_VRA_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
            hfp_connection->vra_state_requested = HFP_VRA_W2_SEND_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO;
            break;
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}


uint8_t hfp_hf_deactivate_voice_recognition(hci_con_handle_t acl_handle){
    // return deactivate_voice_recognition(acl_handle, false);
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (hfp_connection->emit_vra_enabled_after_audio_established){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED || 
        hfp_connection->state > HFP_AUDIO_CONNECTION_ESTABLISHED){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    bool enhanced_vra_supported = hfp_hf_enhanced_vra_flag_supported(hfp_connection);
    bool legacy_vra_supported   = hfp_hf_vra_flag_supported(hfp_connection);
    if (!enhanced_vra_supported && !legacy_vra_supported){
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
    
    switch (hfp_connection->vra_state){
        case HFP_VRA_W2_SEND_VOICE_RECOGNITION_ACTIVATED:
        case HFP_VRA_VOICE_RECOGNITION_ACTIVATED:
        case HFP_VRA_W2_SEND_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
        case HFP_VRA_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
            hfp_connection->vra_state_requested = HFP_VRA_W2_SEND_VOICE_RECOGNITION_OFF;
            break;

        case HFP_VRA_W4_VOICE_RECOGNITION_ACTIVATED:
        case HFP_VRA_W4_ENHANCED_VOICE_RECOGNITION_READY_FOR_AUDIO:
            hfp_connection->deactivate_voice_recognition = true;
            break;
        
        case HFP_VRA_VOICE_RECOGNITION_OFF:
        case HFP_VRA_W2_SEND_VOICE_RECOGNITION_OFF:
        case HFP_VRA_W4_VOICE_RECOGNITION_OFF:
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_set_microphone_gain(hci_con_handle_t acl_handle, int gain){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (hfp_connection->microphone_gain == gain) {
        return ERROR_CODE_SUCCESS;
    }

    if ((gain < 0) || (gain > 15)){
        log_info("Valid range for a gain is [0..15]. Currently sent: %d", gain);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hfp_connection->microphone_gain = gain;
    hfp_connection->send_microphone_gain = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_set_speaker_gain(hci_con_handle_t acl_handle, int gain){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    
    if (hfp_connection->speaker_gain == gain){
        return ERROR_CODE_SUCCESS;
    }

    if ((gain < 0) || (gain > 15)){
        log_info("Valid range for a gain is [0..15]. Currently sent: %d", gain);
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hfp_connection->speaker_gain = gain;
    hfp_connection->send_speaker_gain = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_send_dtmf_code(hci_con_handle_t acl_handle, char code){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_dtmf_code = code;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_request_phone_number_for_voice_tag(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_binp = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_query_current_call_status(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_clcc = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}


uint8_t hfp_hf_rrh_query_status(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '?';
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_rrh_hold_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '0';
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_rrh_accept_held_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '1';
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_rrh_reject_held_call(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_rrh = 1;
    hfp_connection->hf_send_rrh_command = '2';
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_query_subscriber_number(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hfp_connection->hf_send_cnum = 1;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_set_hf_indicator(hci_con_handle_t acl_handle, int assigned_number, int value){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    // find index for assigned number
    uint8_t i;
    for (i = 0; i < hfp_hf_indicators_nr ; i++){
        if (hfp_hf_indicators[i] == assigned_number){
            // check if connection ready and indicator enabled
            if (hfp_connection->state > HFP_LIST_GENERIC_STATUS_INDICATORS){
                if (hfp_connection->generic_status_indicators[i].state != 0) {
                    // set value
                    hfp_hf_indicators_value[i] = value;
                    // mark for update
                    hfp_connection->generic_status_update_bitmap |= (1 << i);
                    // send update
                    hfp_hf_run_for_context(hfp_connection);
                    break;
                }
            }
        }
    }
    if  (i == hfp_hf_indicators_nr){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    return ERROR_CODE_SUCCESS;
}

void hfp_hf_apple_set_identification(uint16_t vendor_id, uint16_t product_id, const char * version, uint8_t features){
    hfp_hf_apple_vendor_id  = vendor_id;
    hfp_hf_apple_product_id = product_id;
    hfp_hf_apple_version    = version;
    hfp_hf_apple_features   = features;
}

uint8_t hfp_hf_apple_set_battery_level(uint8_t battery_level){
    if (battery_level > 9) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    hfp_hf_apple_battery_level = (int8_t) battery_level;
    hfp_hf_apple_trigger_send();
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_apple_set_docked_state(uint8_t docked){
    if (docked > 1) {
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    hfp_hf_apple_docked = (int8_t) docked;
    hfp_hf_apple_trigger_send();
    return ERROR_CODE_SUCCESS;
}

uint8_t hfp_hf_send_at_command(hci_con_handle_t acl_handle, const char * at_command){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (hfp_connection->send_custom_message != NULL){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    hfp_connection->send_custom_message = at_command;
    hfp_hf_run_for_context(hfp_connection);
    return ERROR_CODE_SUCCESS;
}

int hfp_hf_in_band_ringtone_active(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_hf_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection) {
        return 0;
    }
    return get_bit(hfp_connection->remote_supported_features, HFP_AGSF_IN_BAND_RING_TONE);
}

void hfp_hf_create_sdp_record_with_codecs(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr,
                                          const char * name, uint16_t supported_features, uint8_t codecs_nr, const uint8_t * codecs){
    if (!name){
        name = hfp_hf_default_service_name;
    }
    hfp_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_HANDSFREE, rfcomm_channel_nr, name);

    // Construct SupportedFeatures for SDP bitmap:
    //
    // "The values of the “SupportedFeatures” bitmap given in Table 5.4 shall be the same as the values
    //  of the Bits 0 to 4 of the unsolicited result code +BRSF"
    //
    // Wide band speech (bit 5) and LC3-SWB (bit 8) require Codec negotiation
    //
    uint16_t sdp_features = supported_features & 0x1f;

    if (supported_features & (1 << HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS)){
        sdp_features |= 1 << 6;
    }

    if (supported_features & (1 << HFP_HFSF_VOICE_RECOGNITION_TEXT)){
        sdp_features |= 1 << 7;
    }

    // codecs
    if ((supported_features & (1 << HFP_HFSF_CODEC_NEGOTIATION)) != 0){
        uint8_t i;
        for (i=0;i<codecs_nr;i++){
            switch (codecs[i]){
                case HFP_CODEC_MSBC:
                    sdp_features |= 1 << 5;
                    break;
                case HFP_CODEC_LC3_SWB:
                    sdp_features |= 1 << 8;
                    break;
                default:
                    break;
            }
        }
    }

    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SUPPORTED_FEATURES);
    de_add_number(service, DE_UINT, DE_SIZE_16, sdp_features);
}

// @deprecated, call new API
void hfp_hf_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint16_t supported_features, int wide_band_speech){
    uint8_t codecs_nr;
    const uint8_t * codecs;
    const uint8_t wide_band_codecs[] = { HFP_CODEC_MSBC };
    if (wide_band_speech == 0){
        codecs_nr = 0;
        codecs = NULL;
    } else {
        codecs_nr = 1;
        codecs = wide_band_codecs;
    }
    hfp_hf_create_sdp_record_with_codecs(service, service_record_handle, rfcomm_channel_nr, name, supported_features, codecs_nr, codecs);
}

void hfp_hf_register_custom_at_command(hfp_custom_at_command_t * custom_at_command){
    hfp_register_custom_hf_command(custom_at_command);
}

void hfp_hf_register_packet_handler(btstack_packet_handler_t callback){
	btstack_assert(callback != NULL);
    
	hfp_hf_callback = callback;
	hfp_set_hf_callback(callback);
}
