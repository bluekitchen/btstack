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

#define BTSTACK_FILE__ "hfp_ag.c"
 
// *****************************************************************************
//
// HFP Audio Gateway (AG) unit
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>

#include "hci_cmd.h"
#include "btstack_run_loop.h"

#include "bluetooth_sdp.h"
#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "classic/core.h"
#include "classic/hfp.h"
#include "classic/hfp_ag.h"
#include "classic/hfp_gsm_model.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"

// private prototypes
static void hfp_ag_run_for_context(hfp_connection_t *hfp_connection);
static void hfp_ag_hf_start_ringing(hfp_connection_t * hfp_connection);
static void hfp_ag_setup_audio_connection(hfp_connection_t * hfp_connection);

// public prototypes
hfp_generic_status_indicator_t * get_hfp_generic_status_indicators(void);
int get_hfp_generic_status_indicators_nr(void);
void set_hfp_generic_status_indicators(hfp_generic_status_indicator_t * indicators, int indicator_nr);
void set_hfp_ag_indicators(hfp_ag_indicator_t * indicators, int indicator_nr);
int get_hfp_ag_indicators_nr(hfp_connection_t * context);
hfp_ag_indicator_t * get_hfp_ag_indicators(hfp_connection_t * context);


// const
static const char default_hfp_ag_service_name[] = "Voice gateway";

// globals
static btstack_packet_callback_registration_t hfp_ag_hci_event_callback_registration;

static uint16_t hfp_supported_features;

static uint8_t hfp_codecs_nr;
static uint8_t hfp_codecs[HFP_MAX_NUM_CODECS];

static int  hfp_ag_indicators_nr;
static hfp_ag_indicator_t hfp_ag_indicators[HFP_MAX_NUM_INDICATORS];

static int hfp_generic_status_indicators_nr;
static hfp_generic_status_indicator_t hfp_generic_status_indicators[HFP_MAX_NUM_INDICATORS];

static int  hfp_ag_call_hold_services_nr;
static char *hfp_ag_call_hold_services[6];
static btstack_packet_handler_t hfp_ag_callback;

static hfp_response_and_hold_state_t hfp_ag_response_and_hold_state;
static int hfp_ag_response_and_hold_active;

// Subscriber information entries
static hfp_phone_number_t * subscriber_numbers;
static int subscriber_numbers_count;

// code
static int hfp_ag_get_ag_indicators_nr(hfp_connection_t * hfp_connection){
    if (hfp_connection->ag_indicators_nr != hfp_ag_indicators_nr){
        hfp_connection->ag_indicators_nr = hfp_ag_indicators_nr;
        (void)memcpy(hfp_connection->ag_indicators, hfp_ag_indicators,
                     hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    }
    return hfp_connection->ag_indicators_nr;
}

hfp_ag_indicator_t * hfp_ag_get_ag_indicators(hfp_connection_t * hfp_connection){
    // TODO: save only value, and value changed in the hfp_connection?
    if (hfp_connection->ag_indicators_nr != hfp_ag_indicators_nr){
        hfp_connection->ag_indicators_nr = hfp_ag_indicators_nr;
        (void)memcpy(hfp_connection->ag_indicators, hfp_ag_indicators,
                     hfp_ag_indicators_nr * sizeof(hfp_ag_indicator_t));
    }
    return (hfp_ag_indicator_t *)&(hfp_connection->ag_indicators);
}

static hfp_ag_indicator_t * get_ag_indicator_for_name(const char * name){
    int i;
    for (i = 0; i < hfp_ag_indicators_nr; i++){
        if (strcmp(hfp_ag_indicators[i].name, name) == 0){
            return &hfp_ag_indicators[i];
        }
    }
    return NULL;
}

static int get_ag_indicator_index_for_name(const char * name){
    int i;
    for (i = 0; i < hfp_ag_indicators_nr; i++){
        if (strcmp(hfp_ag_indicators[i].name, name) == 0){
            return i;
        }
    }
    return -1;
}

static hfp_connection_t * get_hfp_ag_connection_context_for_acl_handle(uint16_t handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->acl_handle != handle)      continue;
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        return hfp_connection;
    }
    return NULL;
}

static int use_in_band_tone(void){
    return get_bit(hfp_supported_features, HFP_AGSF_IN_BAND_RING_TONE);
}

static int has_codec_negotiation_feature(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_connection->remote_supported_features, HFP_HFSF_CODEC_NEGOTIATION);
    int ag = get_bit(hfp_supported_features, HFP_AGSF_CODEC_NEGOTIATION);
    return hf && ag;
}

static int has_call_waiting_and_3way_calling_feature(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_connection->remote_supported_features, HFP_HFSF_THREE_WAY_CALLING);
    int ag = get_bit(hfp_supported_features, HFP_AGSF_THREE_WAY_CALLING);
    return hf && ag;
}

static int has_hf_indicators_feature(hfp_connection_t * hfp_connection){
    int hf = get_bit(hfp_connection->remote_supported_features, HFP_HFSF_HF_INDICATORS);
    int ag = get_bit(hfp_supported_features, HFP_AGSF_HF_INDICATORS);
    return hf && ag;
}

/* unsolicited responses */

static int hfp_ag_send_change_in_band_ring_tone_setting_cmd(uint16_t cid){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "\r\n%s: %d\r\n",
             HFP_CHANGE_IN_BAND_RING_TONE_SETTING, use_in_band_tone());
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_exchange_supported_features_cmd(uint16_t cid){
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "\r\n%s:%d\r\n\r\nOK\r\n",
             HFP_SUPPORTED_FEATURES, hfp_supported_features);
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_ok(uint16_t cid){
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "\r\nOK\r\n");
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_ring(uint16_t cid){
    return send_str_over_rfcomm(cid, (char *) "\r\nRING\r\n");
}

static int hfp_ag_send_no_carrier(uint16_t cid){
    char buffer[15];
    snprintf(buffer, sizeof(buffer), "\r\nNO CARRIER\r\n");
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_clip(uint16_t cid){
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "\r\n%s: \"%s\",%u\r\n", HFP_ENABLE_CLIP,
             hfp_gsm_clip_number(), hfp_gsm_clip_type());
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_send_subscriber_number_cmd(uint16_t cid, uint8_t type, const char * number){
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "\r\n%s: ,\"%s\",%u, , \r\n",
             HFP_SUBSCRIBER_NUMBER_INFORMATION, number, type);
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}
        
static int hfp_ag_send_phone_number_for_voice_tag_cmd(uint16_t cid){
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "\r\n%s: %s\r\n",
             HFP_PHONE_NUMBER_FOR_VOICE_TAG, hfp_gsm_clip_number());
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_call_waiting_notification(uint16_t cid){
    char buffer[50];
    snprintf(buffer, sizeof(buffer), "\r\n%s: \"%s\",%u\r\n",
             HFP_ENABLE_CALL_WAITING_NOTIFICATION, hfp_gsm_clip_number(),
             hfp_gsm_clip_type());
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_error(uint16_t cid){
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "\r\nERROR\r\n");
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_report_extended_audio_gateway_error(uint16_t cid, uint8_t error){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "\r\n%s=%d\r\n",
             HFP_EXTENDED_AUDIO_GATEWAY_ERROR, error);
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

// get size for indicator string
static int hfp_ag_indicators_string_size(hfp_connection_t * hfp_connection, int i){
    // template: ("$NAME",($MIN,$MAX))
    return 8 + (int) strlen(hfp_ag_get_ag_indicators(hfp_connection)[i].name)
         + string_len_for_uint32(hfp_ag_get_ag_indicators(hfp_connection)[i].min_range)
         + string_len_for_uint32(hfp_ag_get_ag_indicators(hfp_connection)[i].min_range); 
}

// store indicator
static void hfp_ag_indicators_string_store(hfp_connection_t * hfp_connection, int i, uint8_t * buffer, uint16_t buffer_size){
    snprintf((char *)buffer, buffer_size, "(\"%s\",(%d,%d)),",
             hfp_ag_get_ag_indicators(hfp_connection)[i].name,
             hfp_ag_get_ag_indicators(hfp_connection)[i].min_range,
             hfp_ag_get_ag_indicators(hfp_connection)[i].max_range);
    ((char *)buffer)[buffer_size - 1] = 0;
}

// structure: header [indicator [comma indicator]] footer
static int hfp_ag_indicators_cmd_generator_num_segments(hfp_connection_t * hfp_connection){
    int num_indicators = hfp_ag_get_ag_indicators_nr(hfp_connection);
    if (!num_indicators) return 2;
    return 3 + ((num_indicators-1) * 2);
}

// get size of individual segment for hfp_ag_retrieve_indicators_cmd
static int hfp_ag_indicators_cmd_generator_get_segment_len(hfp_connection_t * hfp_connection, int index){
    if (index == 0) {
        return strlen(HFP_INDICATOR) + 3;   // "\n\r%s:""
    }
    index--;
    int num_indicators = hfp_ag_get_ag_indicators_nr(hfp_connection);
    int indicator_index = index >> 1;
    if ((index & 1) == 0){
        return hfp_ag_indicators_string_size(hfp_connection, indicator_index);
    }
    if (indicator_index == (num_indicators - 1)){
        return 8; // "\r\n\r\nOK\r\n"
    }
    return 1; // comma
}

static void hfp_ag_indicators_cmd_generator_store_segment(hfp_connection_t * hfp_connection, int index, uint8_t * buffer, uint16_t buffer_size){
    if (index == 0){
        *buffer++ = '\r';
        *buffer++ = '\n';
        int len = strlen(HFP_INDICATOR);
        (void)memcpy(buffer, HFP_INDICATOR, len);
        buffer += len;
        *buffer++ = ':';
        return;
    }
    index--;
    int num_indicators = hfp_ag_get_ag_indicators_nr(hfp_connection);
    int indicator_index = index >> 1;
    if ((index & 1) == 0){
        hfp_ag_indicators_string_store(hfp_connection, indicator_index, buffer, buffer_size);
        return;
    }
    if (indicator_index == (num_indicators-1)){
        (void)memcpy(buffer, "\r\n\r\nOK\r\n", 8);
        return;
    }
    *buffer = ',';
}

static int hfp_hf_indicators_join(char * buffer, int buffer_size){
    if (buffer_size < (hfp_ag_indicators_nr * 3)) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < (hfp_generic_status_indicators_nr-1); i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "%d,", hfp_generic_status_indicators[i].uuid);
    }
    if (i < hfp_generic_status_indicators_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", hfp_generic_status_indicators[i].uuid);
    }
    return offset;
}

static int hfp_hf_indicators_initial_status_join(char * buffer, int buffer_size){
    if (buffer_size < (hfp_generic_status_indicators_nr * 3)) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < hfp_generic_status_indicators_nr; i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "\r\n%s:%d,%d\r\n", HFP_GENERIC_STATUS_INDICATOR, hfp_generic_status_indicators[i].uuid, hfp_generic_status_indicators[i].state);
    }
    return offset;
}

static int hfp_ag_indicators_status_join(char * buffer, int buffer_size){
    if (buffer_size < (hfp_ag_indicators_nr * 3)) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < (hfp_ag_indicators_nr-1); i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "%d,", hfp_ag_indicators[i].status); 
    }
    if (i<hfp_ag_indicators_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", hfp_ag_indicators[i].status);
    }
    return offset;
}

static int hfp_ag_call_services_join(char * buffer, int buffer_size){
    if (buffer_size < (hfp_ag_call_hold_services_nr * 3)) return 0;
    int i;
    int offset = snprintf(buffer, buffer_size, "("); 
    for (i = 0; i < (hfp_ag_call_hold_services_nr-1); i++) {
        offset += snprintf(buffer+offset, buffer_size-offset, "%s,", hfp_ag_call_hold_services[i]); 
    }
    if (i<hfp_ag_call_hold_services_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%s)", hfp_ag_call_hold_services[i]);
    }
    return offset;
}

static int hfp_ag_send_cmd_via_generator(uint16_t cid, hfp_connection_t * hfp_connection,
    int start_segment, int num_segments,
    int (*get_segment_len)(hfp_connection_t * hfp_connection, int segment),
    void (*store_segment) (hfp_connection_t * hfp_connection, int segment, uint8_t * buffer, uint16_t buffer_size)){

    // assumes: can send now == true
    // assumes: num segments > 0
    // assumes: individual segments are smaller than MTU
    rfcomm_reserve_packet_buffer();
    int mtu = rfcomm_get_max_frame_size(cid);
    uint8_t * data = rfcomm_get_outgoing_buffer();
    int offset = 0;
    int segment = start_segment;
    while (segment < num_segments){
        int segment_len = get_segment_len(hfp_connection, segment);
        if ((offset + segment_len + 1) > mtu) break;
        // append segment. As it appends a '\0', we provide a buffer one byte larger
        store_segment(hfp_connection, segment, data+offset, segment_len + 1);
        offset += segment_len;
        segment++;
    }
    rfcomm_send_prepared(cid, offset);
#ifdef ENABLE_HFP_AT_MESSAGES
    hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_AT_MESSAGE_SENT, (char *) data);
#endif
    return segment;
}

// returns next segment to store
static int hfp_ag_send_retrieve_indicators_cmd_via_generator(uint16_t cid, hfp_connection_t * hfp_connection, int start_segment){
    int num_segments = hfp_ag_indicators_cmd_generator_num_segments(hfp_connection);
    return hfp_ag_send_cmd_via_generator(cid, hfp_connection, start_segment, num_segments,
                                         hfp_ag_indicators_cmd_generator_get_segment_len,
                                         hfp_ag_indicators_cmd_generator_store_segment);
}

static int hfp_ag_send_retrieve_indicators_status_cmd(uint16_t cid){
    char buffer[40];
    const int size = sizeof(buffer);
    int offset = snprintf(buffer, size, "\r\n%s:", HFP_INDICATOR);
    offset += hfp_ag_indicators_status_join(buffer+offset, size-offset-9);
    offset += snprintf(buffer+offset, size-offset, "\r\n\r\nOK\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_retrieve_can_hold_call_cmd(uint16_t cid){
    char buffer[40];
    const int size = sizeof(buffer);
    int offset = snprintf(buffer, size, "\r\n%s:", HFP_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES);
    offset += hfp_ag_call_services_join(buffer+offset, size-offset-9);
    offset += snprintf(buffer+offset, size-offset, "\r\n\r\nOK\r\n");
    return send_str_over_rfcomm(cid, buffer);
}


static int hfp_ag_send_list_supported_generic_status_indicators_cmd(uint16_t cid){
    return hfp_ag_send_ok(cid);
}

static int hfp_ag_send_retrieve_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[40];
    const int size = sizeof(buffer);
    int offset = snprintf(buffer, size, "\r\n%s:(", HFP_GENERIC_STATUS_INDICATOR);
    offset += hfp_hf_indicators_join(buffer+offset, size-offset-10);
    offset += snprintf(buffer+offset, size-offset, ")\r\n\r\nOK\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_retrieve_initital_supported_generic_status_indicators_cmd(uint16_t cid){
    char buffer[40];
    int offset = hfp_hf_indicators_initial_status_join(buffer, sizeof(buffer) - 7);
    snprintf(buffer+offset, sizeof(buffer)-offset, "\r\nOK\r\n");
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_transfer_ag_indicators_status_cmd(uint16_t cid, hfp_ag_indicator_t * indicator){
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "\r\n%s:%d,%d\r\n",
             HFP_TRANSFER_AG_INDICATOR_STATUS, indicator->index,
             indicator->status);
    buffer[sizeof(buffer) - 1] = 0;
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_report_network_operator_name_cmd(uint16_t cid, hfp_network_opearator_t op){
    char buffer[41];
    if (strlen(op.name) == 0){
        snprintf(buffer, sizeof(buffer), "\r\n%s:%d,,\r\n\r\nOK\r\n", HFP_QUERY_OPERATOR_SELECTION, op.mode);
    } else {
        int offset = snprintf(buffer,  sizeof(buffer), "\r\n%s:%d,%d,", HFP_QUERY_OPERATOR_SELECTION, op.mode, op.format);
        offset += snprintf(buffer+offset, 16, "%s", op.name);
        snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n\r\nOK\r\n");
    }
    return send_str_over_rfcomm(cid, buffer);
}

static inline int hfp_ag_send_cmd_with_space_and_int(uint16_t cid, const char * cmd, uint8_t value){
    char buffer[30];
    snprintf(buffer, sizeof(buffer), "\r\n%s: %d\r\n", cmd, value);
    return send_str_over_rfcomm(cid, buffer);
}


static inline int hfp_ag_send_cmd_with_int(uint16_t cid, const char * cmd, uint8_t value){
    char buffer[30];
    snprintf(buffer, sizeof(buffer), "\r\n%s:%d\r\n", cmd, value);
    return send_str_over_rfcomm(cid, buffer);
}

static int hfp_ag_send_suggest_codec_cmd(uint16_t cid, uint8_t codec){
    return hfp_ag_send_cmd_with_int(cid, HFP_CONFIRM_COMMON_CODEC, codec);
}

static int hfp_ag_send_activate_voice_recognition_cmd(uint16_t cid, uint8_t activate_voice_recognition){
    return hfp_ag_send_cmd_with_space_and_int(cid, HFP_ACTIVATE_VOICE_RECOGNITION, activate_voice_recognition);
}

static int hfp_ag_send_set_speaker_gain_cmd(uint16_t cid, uint8_t gain){
    return hfp_ag_send_cmd_with_int(cid, HFP_SET_SPEAKER_GAIN, gain);
}

static int hfp_ag_send_set_microphone_gain_cmd(uint16_t cid, uint8_t gain){
    return hfp_ag_send_cmd_with_int(cid, HFP_SET_MICROPHONE_GAIN, gain);
}
            
static int hfp_ag_send_set_response_and_hold(uint16_t cid, int state){
    return hfp_ag_send_cmd_with_space_and_int(cid, HFP_RESPONSE_AND_HOLD, state);
}

static uint8_t hfp_ag_suggest_codec(hfp_connection_t *hfp_connection){
    if (hfp_connection->sco_for_msbc_failed) return HFP_CODEC_CVSD;

    if (hfp_supports_codec(HFP_CODEC_MSBC, hfp_codecs_nr, hfp_codecs)){
        if (hfp_supports_codec(HFP_CODEC_MSBC, hfp_connection->remote_codecs_nr, hfp_connection->remote_codecs)){
            return HFP_CODEC_MSBC;
        }
    }
    return HFP_CODEC_CVSD;
}

/* state machines */

static uint8_t hfp_ag_esco_s4_supported(hfp_connection_t * hfp_connection){
    return (hfp_connection->remote_supported_features & (1<<HFP_HFSF_ESCO_S4)) &&  (hfp_supported_features  & (1<<HFP_AGSF_ESCO_S4));
}

static int codecs_exchange_state_machine(hfp_connection_t * hfp_connection){
    /* events ( == commands):
        HFP_CMD_AVAILABLE_CODECS == received AT+BAC with list of codecs
        HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP:
            hf_trigger_codec_connection_setup == received BCC
            ag_trigger_codec_connection_setup == received from AG to send BCS
        HFP_CMD_HF_CONFIRMED_CODEC == received AT+BCS
    */
    switch (hfp_connection->codecs_state){
        case HFP_CODECS_EXCHANGED:
            if (hfp_connection->command == HFP_CMD_AVAILABLE_CODECS){
                hfp_ag_send_ok(hfp_connection->rfcomm_cid);
                return 1;
            }
            break;

        case HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE:
            hfp_connection->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            break;
        case HFP_CODECS_AG_RESEND_COMMON_CODEC:
            hfp_connection->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            break;
        default:
            break;
    }

    switch (hfp_connection->command){
        case HFP_CMD_AVAILABLE_CODECS:
            if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED){
                hfp_connection->codecs_state = HFP_CODECS_RECEIVED_LIST;
                hfp_ag_send_ok(hfp_connection->rfcomm_cid);
                return 1;    
            }

            switch (hfp_connection->codecs_state){
                case HFP_CODECS_AG_SENT_COMMON_CODEC:
                    hfp_connection->codecs_state = HFP_CODECS_AG_RESEND_COMMON_CODEC;
                    break;
                default:
                    break;
            }
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            return 1;
        
        case HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP:
            hfp_connection->codecs_state = HFP_CODECS_RECEIVED_TRIGGER_CODEC_EXCHANGE;
            hfp_connection->establish_audio_connection = 1;
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            return 1;
        
        case HFP_CMD_AG_SEND_COMMON_CODEC:
            hfp_connection->codecs_state = HFP_CODECS_AG_SENT_COMMON_CODEC;
            hfp_connection->suggested_codec = hfp_ag_suggest_codec(hfp_connection);
            hfp_ag_send_suggest_codec_cmd(hfp_connection->rfcomm_cid, hfp_connection->suggested_codec);
            return 1;

        case HFP_CMD_HF_CONFIRMED_CODEC:
            if (hfp_connection->codec_confirmed != hfp_connection->suggested_codec){
                hfp_connection->codecs_state = HFP_CODECS_ERROR;
                hfp_ag_send_error(hfp_connection->rfcomm_cid);
                return 1;
            } 
            hfp_connection->negotiated_codec = hfp_connection->codec_confirmed;
            hfp_connection->codecs_state = HFP_CODECS_EXCHANGED;
            log_info("hfp: codec confirmed: %s", (hfp_connection->negotiated_codec == HFP_CODEC_MSBC) ? "mSBC" : "CVSD");
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);           
            // now, pick link settings
            hfp_init_link_settings(hfp_connection, hfp_ag_esco_s4_supported(hfp_connection));
#ifdef ENABLE_CC256X_ASSISTED_HFP
            hfp_cc256x_prepare_for_sco(hfp_connection);
#endif
            return 1;
        default:
            break;
    }
    return 0;
}

static void hfp_ag_slc_established(hfp_connection_t * hfp_connection){
    hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
    hfp_emit_slc_connection_event(hfp_connection, 0, hfp_connection->acl_handle, hfp_connection->remote_addr);
    
    // if active call exist, set per-hfp_connection state active, too (when audio is on)
    if (hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
        hfp_connection->call_state = HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE;
    }
    // if AG is ringing, also start ringing on the HF
    if ((hfp_gsm_call_status() == HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS) &&
        (hfp_gsm_callsetup_status() == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS)){
        hfp_ag_hf_start_ringing(hfp_connection);
    }
}

static int hfp_ag_run_for_context_service_level_connection(hfp_connection_t * hfp_connection){
    // log_info("hfp_ag_run_for_context_service_level_connection state %u, command %u", hfp_connection->state, hfp_connection->command);
    if (hfp_connection->state >= HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) return 0;
    int sent = 0;
    switch(hfp_connection->command){
        case HFP_CMD_SUPPORTED_FEATURES:
            switch(hfp_connection->state){
                case HFP_W4_EXCHANGE_SUPPORTED_FEATURES:
                case HFP_EXCHANGE_SUPPORTED_FEATURES:
                    hfp_hf_drop_mSBC_if_eSCO_not_supported(hfp_codecs, &hfp_codecs_nr);
                    if (has_codec_negotiation_feature(hfp_connection)){
                        hfp_connection->state = HFP_W4_NOTIFY_ON_CODECS;
                    } else {
                        hfp_connection->state = HFP_W4_RETRIEVE_INDICATORS;
                    }
                    hfp_ag_exchange_supported_features_cmd(hfp_connection->rfcomm_cid);
                    return 1;
                default:
                    break;
            }
            break;
        case HFP_CMD_AVAILABLE_CODECS:
            sent = codecs_exchange_state_machine(hfp_connection);
            if (hfp_connection->codecs_state == HFP_CODECS_RECEIVED_LIST){
                hfp_connection->state = HFP_W4_RETRIEVE_INDICATORS;
            }
            return sent;

        case HFP_CMD_RETRIEVE_AG_INDICATORS:
            if (hfp_connection->state == HFP_W4_RETRIEVE_INDICATORS) {
                // HF requested AG Indicators and we did expect it
                hfp_connection->send_ag_indicators_segment = 0;
                hfp_connection->state = HFP_RETRIEVE_INDICATORS;
                // continue below in state switch
            }
            break;
        
        case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
            if (hfp_connection->state != HFP_W4_RETRIEVE_INDICATORS_STATUS) break;
            hfp_connection->state = HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE;
            hfp_ag_send_retrieve_indicators_status_cmd(hfp_connection->rfcomm_cid);
            return 1;

        case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
            if (hfp_connection->state != HFP_W4_ENABLE_INDICATORS_STATUS_UPDATE) break;
            if (has_call_waiting_and_3way_calling_feature(hfp_connection)){
                hfp_connection->state = HFP_W4_RETRIEVE_CAN_HOLD_CALL;
            } else if (has_hf_indicators_feature(hfp_connection)){
                hfp_connection->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            } else {
                hfp_ag_slc_established(hfp_connection);
            }
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            return 1;
                
        case HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES:
            if (hfp_connection->state != HFP_W4_RETRIEVE_CAN_HOLD_CALL) break;
            if (has_hf_indicators_feature(hfp_connection)){
                hfp_connection->state = HFP_W4_LIST_GENERIC_STATUS_INDICATORS;
            }
            hfp_ag_send_retrieve_can_hold_call_cmd(hfp_connection->rfcomm_cid);
            if (!has_hf_indicators_feature(hfp_connection)){
                hfp_ag_slc_established(hfp_connection);
            }
            return 1;
        
        case HFP_CMD_LIST_GENERIC_STATUS_INDICATORS:
            if (hfp_connection->state != HFP_W4_LIST_GENERIC_STATUS_INDICATORS) break;
            hfp_connection->state = HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS;
            hfp_ag_send_list_supported_generic_status_indicators_cmd(hfp_connection->rfcomm_cid);
            return 1;

        case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS:
            if (hfp_connection->state != HFP_W4_RETRIEVE_GENERIC_STATUS_INDICATORS) break;
            hfp_connection->state = HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS; 
            hfp_ag_send_retrieve_supported_generic_status_indicators_cmd(hfp_connection->rfcomm_cid);
            return 1;

        case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE:
            if (hfp_connection->state != HFP_W4_RETRIEVE_INITITAL_STATE_GENERIC_STATUS_INDICATORS) break;
            hfp_ag_slc_established(hfp_connection);
            hfp_ag_send_retrieve_initital_supported_generic_status_indicators_cmd(hfp_connection->rfcomm_cid);
            return 1;
        default:
            break;
    }

    switch (hfp_connection->state){
        case HFP_RETRIEVE_INDICATORS: {
            int next_segment = hfp_ag_send_retrieve_indicators_cmd_via_generator(hfp_connection->rfcomm_cid, hfp_connection, hfp_connection->send_ag_indicators_segment);
            int num_segments = hfp_ag_indicators_cmd_generator_num_segments(hfp_connection);
            log_info("HFP_CMD_RETRIEVE_AG_INDICATORS next segment %u, num_segments %u", next_segment, num_segments);
            if (next_segment < num_segments){
                // prepare sending of next segment
                hfp_connection->send_ag_indicators_segment = next_segment;
                log_info("HFP_CMD_RETRIEVE_AG_INDICATORS more. command %u, next seg %u", hfp_connection->command, next_segment);
            } else {
                // done, go to next state
                hfp_connection->send_ag_indicators_segment = 0;
                hfp_connection->state = HFP_W4_RETRIEVE_INDICATORS_STATUS;
            }
            return 1;
        }
        default:
            break;
    }
    return 0;
}

static int hfp_ag_run_for_context_service_level_connection_queries(hfp_connection_t * hfp_connection){
    int sent = codecs_exchange_state_machine(hfp_connection);
    if (sent) return 1;
   
    switch(hfp_connection->command){
        case HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION:
            hfp_ag_send_activate_voice_recognition_cmd(hfp_connection->rfcomm_cid, hfp_connection->ag_activate_voice_recognition);
            if (hfp_connection->ag_activate_voice_recognition > 0){
                hfp_ag_setup_audio_connection(hfp_connection);
            } else {
                hfp_release_audio_connection(hfp_connection);
            }
            return 1;
        case HFP_CMD_HF_ACTIVATE_VOICE_RECOGNITION:
            if (get_bit(hfp_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION)){
                hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            } else {
                hfp_ag_send_error(hfp_connection->rfcomm_cid);
            }
            return 1;
        case HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING:
            hfp_ag_send_change_in_band_ring_tone_setting_cmd(hfp_connection->rfcomm_cid);
            return 1;
        case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME:
            hfp_ag_send_report_network_operator_name_cmd(hfp_connection->rfcomm_cid, hfp_connection->network_operator);
            return 1;
        case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT:
            if (hfp_connection->network_operator.format != 0){
                hfp_ag_send_error(hfp_connection->rfcomm_cid);
            } else {
                hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            }
            return 1;
        case HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE:
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            return 1;
        case HFP_CMD_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR:
            if (hfp_connection->extended_audio_gateway_error){
                hfp_connection->extended_audio_gateway_error = 0;
                hfp_ag_send_report_extended_audio_gateway_error(hfp_connection->rfcomm_cid, hfp_connection->extended_audio_gateway_error_value);
                return 1;
            }
            break;
        case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);
            return 1;
        default:
            break;
    }
    return 0;
}

static int hfp_ag_run_for_audio_connection(hfp_connection_t * hfp_connection){
    if ((hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) ||
        (hfp_connection->state > HFP_W2_DISCONNECT_SCO)) return 0;

    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;

    // accept incoming audio connection (codec negotiation is not used)
    if (hfp_connection->accept_sco){
        // notify about codec selection if not done already
        if (hfp_connection->negotiated_codec == 0){
            hfp_connection->negotiated_codec = HFP_CODEC_CVSD;
        }
        //
        bool incoming_eSCO = hfp_connection->accept_sco == 2;
        hfp_connection->accept_sco = 0;
        hfp_connection->state = HFP_W4_SCO_CONNECTED;
        hfp_accept_synchronous_connection(hfp_connection, incoming_eSCO);
        return 1;
    }

    // run codecs exchange
    int sent = codecs_exchange_state_machine(hfp_connection);
    if (sent) return 1;

    if (hfp_connection->codecs_state != HFP_CODECS_EXCHANGED) return 0;
    if (hfp_connection->establish_audio_connection){
        hfp_connection->state = HFP_W4_SCO_CONNECTED;
        hfp_connection->establish_audio_connection = 0;
        hfp_setup_synchronous_connection(hfp_connection);
        return 1;
    }
    return 0;
}

static hfp_connection_t * hfp_ag_context_for_timer(btstack_timer_source_t * ts){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());

    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if ( & hfp_connection->hfp_timeout == ts) {
            return hfp_connection;
        }
    }
    return NULL;
}

static void hfp_timeout_handler(btstack_timer_source_t * timer){
    hfp_connection_t * hfp_connection = hfp_ag_context_for_timer(timer);
    if (!hfp_connection) return;

    log_info("HFP start ring timeout, con handle 0x%02x", hfp_connection->acl_handle);
    hfp_connection->ag_ring = 1;
    hfp_connection->ag_send_clip = hfp_gsm_clip_type() && hfp_connection->clip_enabled;

    btstack_run_loop_set_timer(& hfp_connection->hfp_timeout, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(& hfp_connection->hfp_timeout);

    hfp_ag_run_for_context(hfp_connection);
}

static void hfp_timeout_start(hfp_connection_t * hfp_connection){
    btstack_run_loop_remove_timer(& hfp_connection->hfp_timeout);
    btstack_run_loop_set_timer_handler(& hfp_connection->hfp_timeout, hfp_timeout_handler);
    btstack_run_loop_set_timer(& hfp_connection->hfp_timeout, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(& hfp_connection->hfp_timeout);
}

static void hfp_timeout_stop(hfp_connection_t * hfp_connection){
    log_info("HFP stop ring timeout, con handle 0x%02x", hfp_connection->acl_handle);
    btstack_run_loop_remove_timer(& hfp_connection->hfp_timeout);
} 

static void hfp_ag_set_callsetup_indicator(void){
    hfp_ag_indicator_t * indicator = get_ag_indicator_for_name("callsetup");
    if (!indicator){
        log_error("hfp_ag_set_callsetup_indicator: callsetup indicator is missing");
        return;
    };
    indicator->status = hfp_gsm_callsetup_status();
}

static void hfp_ag_set_callheld_indicator(void){
    hfp_ag_indicator_t * indicator = get_ag_indicator_for_name("callheld");
    if (!indicator){
        log_error("hfp_ag_set_callheld_state: callheld indicator is missing");
        return;
    };
    indicator->status = hfp_gsm_callheld_status();
}

//
// transitition implementations for hfp_ag_call_state_machine
//

static void hfp_ag_hf_start_ringing(hfp_connection_t * hfp_connection){
    if (use_in_band_tone()){
        hfp_connection->call_state = HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING;
        hfp_ag_establish_audio_connection(hfp_connection->acl_handle);
    } else {
        hfp_timeout_start(hfp_connection);
        hfp_connection->ag_ring = 1;
        hfp_connection->ag_send_clip = hfp_gsm_clip_type() && hfp_connection->clip_enabled;
        hfp_connection->call_state = HFP_CALL_RINGING;
        hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_START_RINGINIG);
    }
}

static void hfp_ag_hf_stop_ringing(hfp_connection_t * hfp_connection){
    hfp_connection->ag_ring = 0;
    hfp_connection->ag_send_clip = 0;
    hfp_timeout_stop(hfp_connection);
    hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_STOP_RINGINIG);
}

static void hfp_ag_trigger_incoming_call(void){
    int indicator_index = get_ag_indicator_index_for_name("callsetup");
    if (indicator_index < 0) return;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_ag_establish_service_level_connection(hfp_connection->remote_addr);
        if (hfp_connection->call_state == HFP_CALL_IDLE){
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, indicator_index, 1);
            hfp_ag_hf_start_ringing(hfp_connection);
        }
        if (hfp_connection->call_state == HFP_CALL_ACTIVE){
            hfp_connection->call_state = HFP_CALL_W2_SEND_CALL_WAITING;
        }
        hfp_ag_run_for_context(hfp_connection);
    }
}

static void hfp_ag_trigger_outgoing_call(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_ag_establish_service_level_connection(hfp_connection->remote_addr);
        if (hfp_connection->call_state == HFP_CALL_IDLE){
            hfp_connection->call_state = HFP_CALL_OUTGOING_INITIATED;
        }
    }
}

static void hfp_ag_handle_outgoing_call_accepted(hfp_connection_t * hfp_connection){
    hfp_connection->call_state = HFP_CALL_OUTGOING_DIALING;

    // trigger callsetup to be
    int put_call_on_hold = hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT;
    hfp_gsm_handler(HFP_AG_OUTGOING_CALL_ACCEPTED, 0, 0, NULL);

    int indicator_index ;

    hfp_ag_set_callsetup_indicator();
    indicator_index = get_ag_indicator_index_for_name("callsetup");
    hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, indicator_index, 1);

    // put current call on hold if active
    if (put_call_on_hold){
        log_info("AG putting current call on hold for new outgoing call");
        hfp_ag_set_callheld_indicator();
        indicator_index = get_ag_indicator_index_for_name("callheld");
        hfp_ag_send_transfer_ag_indicators_status_cmd(hfp_connection->rfcomm_cid, &hfp_ag_indicators[indicator_index]);
    }

    // start audio if needed
    hfp_ag_establish_audio_connection(hfp_connection->acl_handle);
}

static void hfp_ag_transfer_indicator(const char * name){
    int indicator_index = get_ag_indicator_index_for_name(name);
    if (indicator_index < 0) return;

    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_ag_establish_service_level_connection(hfp_connection->remote_addr);
        hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, indicator_index, 1);
        hfp_ag_run_for_context(hfp_connection);
    }
}

static void hfp_ag_transfer_callsetup_state(void){
    hfp_ag_transfer_indicator("callsetup");
}

static void hfp_ag_transfer_call_state(void){
    hfp_ag_transfer_indicator("call");
}

static void hfp_ag_transfer_callheld_state(void){
    hfp_ag_transfer_indicator("callheld");
}

static void hfp_ag_hf_accept_call(hfp_connection_t * source){
    int call_indicator_index = get_ag_indicator_index_for_name("call");
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        if ((hfp_connection->call_state != HFP_CALL_RINGING) &&
            (hfp_connection->call_state != HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING)) continue;

        hfp_ag_hf_stop_ringing(hfp_connection);
        if (hfp_connection == source){

            if (use_in_band_tone()){
                hfp_connection->call_state = HFP_CALL_ACTIVE;
            } else {
                hfp_connection->call_state = HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE;
                hfp_ag_establish_audio_connection(hfp_connection->acl_handle);
            }

            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);

        } else {
            hfp_connection->call_state = HFP_CALL_IDLE;
        }
        hfp_ag_run_for_context(hfp_connection);
    }    
}

static void hfp_ag_ag_accept_call(void){
    int call_indicator_index = get_ag_indicator_index_for_name("call");
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        if (hfp_connection->call_state != HFP_CALL_RINGING) continue;

        hfp_ag_hf_stop_ringing(hfp_connection);
        hfp_connection->call_state = HFP_CALL_TRIGGER_AUDIO_CONNECTION;

        hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
        hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);

        hfp_ag_run_for_context(hfp_connection);
        break;  // only single 
    }    
}

static void hfp_ag_trigger_reject_call(void){
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (connection->local_role != HFP_ROLE_AG) continue;
        if ((connection->call_state != HFP_CALL_RINGING) &&
            (connection->call_state != HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING)) continue;
        hfp_ag_hf_stop_ringing(connection);
        connection->ag_indicators_status_update_bitmap = store_bit(connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
        connection->call_state = HFP_CALL_IDLE;
        hfp_ag_run_for_context(connection);
    }    
}

static void hfp_ag_trigger_terminate_call(void){
    int call_indicator_index = get_ag_indicator_index_for_name("call");

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_ag_establish_service_level_connection(hfp_connection->remote_addr);
        if (hfp_connection->call_state == HFP_CALL_IDLE) continue;
        hfp_connection->call_state = HFP_CALL_IDLE;
        hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
        if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED){
            hfp_connection->release_audio_connection = 1;
        }
        hfp_ag_run_for_context(hfp_connection);
    }
    hfp_emit_simple_event(NULL, HFP_SUBEVENT_CALL_TERMINATED);
}

static void hfp_ag_set_call_indicator(void){
    hfp_ag_indicator_t * indicator = get_ag_indicator_for_name("call");
    if (!indicator){
        log_error("hfp_ag_set_call_state: call indicator is missing");
        return;
    };
    indicator->status = hfp_gsm_call_status();
}

static void hfp_ag_stop_ringing(void){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        if ((hfp_connection->call_state != HFP_CALL_RINGING) &&
            (hfp_connection->call_state != HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING)) continue;
        hfp_ag_hf_stop_ringing(hfp_connection);
    }    
}

static hfp_connection_t * hfp_ag_connection_for_call_state(hfp_call_state_t call_state){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        if (hfp_connection->call_state == call_state) return hfp_connection;
    }
    return NULL;
}

static void hfp_ag_send_response_and_hold_state(hfp_response_and_hold_state_t state){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_connection->send_response_and_hold_status = state + 1;
    }
}

static int call_setup_state_machine(hfp_connection_t * hfp_connection){
    int indicator_index;
    switch (hfp_connection->call_state){
        case HFP_CALL_W4_AUDIO_CONNECTION_FOR_IN_BAND_RING:
            if (hfp_connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;
            // we got event: audio hfp_connection established
            hfp_timeout_start(hfp_connection);
            hfp_connection->ag_ring = 1;
            hfp_connection->ag_send_clip = hfp_gsm_clip_type() && hfp_connection->clip_enabled;
            hfp_connection->call_state = HFP_CALL_RINGING;
            hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_START_RINGINIG);
            break;        
        case HFP_CALL_W4_AUDIO_CONNECTION_FOR_ACTIVE:
            if (hfp_connection->state != HFP_AUDIO_CONNECTION_ESTABLISHED) return 0;
            // we got event: audio hfp_connection established
            hfp_connection->call_state = HFP_CALL_ACTIVE;
            break;    
        case HFP_CALL_W2_SEND_CALL_WAITING:
            hfp_connection->call_state = HFP_CALL_W4_CHLD;
            hfp_ag_send_call_waiting_notification(hfp_connection->rfcomm_cid);
            indicator_index = get_ag_indicator_index_for_name("callsetup");
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, indicator_index, 1);
            break;
        default:
            break;
    }
    return 0;
}

// functions extracted from hfp_ag_call_sm below
static void hfp_ag_handle_reject_outgoing_call(void){
    hfp_connection_t * hfp_connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_INITIATED);
    if (!hfp_connection){
        log_info("hfp_ag_call_sm: did not find outgoing hfp_connection in initiated state");
        return;
    }

    hfp_gsm_handler(HFP_AG_OUTGOING_CALL_REJECTED, 0, 0, NULL);
    hfp_connection->call_state = HFP_CALL_IDLE;
    hfp_connection->send_error = 1;
    hfp_ag_run_for_context(hfp_connection);
}

// hfp_connection is used to identify originating HF
static void hfp_ag_call_sm(hfp_ag_call_event_t event, hfp_connection_t * hfp_connection){
    int callsetup_indicator_index = get_ag_indicator_index_for_name("callsetup");
    int callheld_indicator_index = get_ag_indicator_index_for_name("callheld");
    int call_indicator_index = get_ag_indicator_index_for_name("call");
    
    switch (event){
        case HFP_AG_INCOMING_CALL:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_INCOMING_CALL, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_trigger_incoming_call();
                            log_info("AG rings");
                            break;
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_INCOMING_CALL, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_trigger_incoming_call();
                            log_info("AG call waiting");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG, 0, 0, NULL);
                            hfp_ag_set_call_indicator();
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_ag_accept_call();
                            log_info("AG answers call, accept call by GSM");
                            break;
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            log_info("AG: current call is placed on hold, incoming call gets active");
                            hfp_gsm_handler(HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_set_callheld_indicator();
                            hfp_ag_transfer_callsetup_state();
                            hfp_ag_transfer_callheld_state();
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        
        case HFP_AG_HELD_CALL_JOINED_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    switch (hfp_gsm_callheld_status()){
                        case HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED:
                            log_info("AG: joining held call with active call");
                            hfp_gsm_handler(HFP_AG_HELD_CALL_JOINED_BY_AG, 0, 0, NULL);
                            hfp_ag_set_callheld_indicator();
                            hfp_ag_transfer_callheld_state();
                            hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_CONFERENCE_CALL);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_set_call_indicator();
                            hfp_ag_hf_accept_call(hfp_connection);
                            hfp_connection->ok_pending = 1;
                            log_info("HF answers call, accept call by GSM");
                            hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_CALL_ANSWERED);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG, 0, 0, NULL);
                            hfp_ag_response_and_hold_active = 1;
                            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD;
                            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
                            // as with regualr call
                            hfp_ag_set_call_indicator();
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_ag_accept_call();
                            log_info("AG response and hold - hold by AG");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF, 0, 0, NULL);
                            hfp_ag_response_and_hold_active = 1;
                            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD;
                            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
                            // as with regualr call
                            hfp_ag_set_call_indicator();
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_hf_accept_call(hfp_connection);
                            log_info("AG response and hold - hold by HF");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF:
            if (!hfp_ag_response_and_hold_active) break;
            if (hfp_ag_response_and_hold_state != HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD) break;
            hfp_gsm_handler(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG, 0, 0, NULL);
            hfp_ag_response_and_hold_active = 0;
            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_HELD_INCOMING_ACCEPTED;
            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
            log_info("Held Call accepted and active");
            break;

        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF:
            if (!hfp_ag_response_and_hold_active) break;
            if (hfp_ag_response_and_hold_state != HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD) break;
            hfp_gsm_handler(HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG, 0, 0, NULL);
            hfp_ag_response_and_hold_active = 0;
            hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED;
            hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
            // from terminate by ag
            hfp_ag_set_call_indicator();
            hfp_ag_trigger_terminate_call();
            break;

        case HFP_AG_TERMINATE_CALL_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_TERMINATE_CALL_BY_HF, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_transfer_callsetup_state();
                            hfp_ag_trigger_reject_call();
                            log_info("HF Rejected Incoming call, AG terminate call");
                            break;
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE:
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE:
                            hfp_gsm_handler(HFP_AG_TERMINATE_CALL_BY_HF, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_transfer_callsetup_state();
                            log_info("AG terminate outgoing call process"); 
                            break;              
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    hfp_gsm_handler(HFP_AG_TERMINATE_CALL_BY_HF, 0, 0, NULL);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_set_call_indicator();
                    hfp_ag_trigger_terminate_call();
                    log_info("AG terminate call");
                    break;
                default:
                    break;
            }
            hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_CALL_TERMINATED);
            break;

        case HFP_AG_TERMINATE_CALL_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_gsm_handler(HFP_AG_TERMINATE_CALL_BY_AG, 0, 0, NULL);
                            hfp_ag_set_callsetup_indicator();
                            hfp_ag_trigger_reject_call();
                            log_info("AG Rejected Incoming call, AG terminate call");
                            break;
                        default:
                            break;
                    }
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    hfp_gsm_handler(HFP_AG_TERMINATE_CALL_BY_AG, 0, 0, NULL);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_set_call_indicator();
                    hfp_ag_trigger_terminate_call();
                    log_info("AG terminate call");
                    break;
                default:
                    break;
            }
            break;
        case HFP_AG_CALL_DROPPED:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()){
                        case HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS:
                            hfp_ag_stop_ringing();
                            log_info("Incoming call interrupted");
                            break;
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE:
                        case HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE:
                            log_info("Outgoing call interrupted\n");
                            log_info("AG notify call dropped\n");
                            break;
                        default:
                            break;
                    }
                    hfp_gsm_handler(HFP_AG_CALL_DROPPED, 0, 0, NULL);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_transfer_callsetup_state();
                    hfp_ag_trigger_terminate_call();
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    if (hfp_ag_response_and_hold_active) {
                        hfp_gsm_handler(HFP_AG_CALL_DROPPED, 0, 0, NULL);
                        hfp_ag_response_and_hold_state = HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED;
                        hfp_ag_send_response_and_hold_state(hfp_ag_response_and_hold_state);
                    }
                    hfp_gsm_handler(HFP_AG_CALL_DROPPED, 0, 0, NULL);
                    hfp_ag_set_callsetup_indicator();
                    hfp_ag_set_call_indicator();
                    hfp_ag_trigger_terminate_call();
                    log_info("AG notify call dropped");
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_OUTGOING_CALL_INITIATED_BY_HF:
            // directly reject call if number of free slots is exceeded
            if (!hfp_gsm_call_possible()){
                hfp_connection->send_error = 1;
                hfp_ag_run_for_context(hfp_connection);  
                break;
            }

            // note: number not used currently
            hfp_gsm_handler(HFP_AG_OUTGOING_CALL_INITIATED_BY_HF, 0, 0, (const char *) &hfp_connection->line_buffer[3]);

            hfp_connection->call_state = HFP_CALL_OUTGOING_INITIATED;

            hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER, (const char *) &hfp_connection->line_buffer[3]);
            break;

        case HFP_AG_OUTGOING_CALL_INITIATED_BY_AG:
            // directly reject call if number of free slots is exceeded
            if (!hfp_gsm_call_possible()) {
                // TODO: no error for user
                break;
            }
            switch (hfp_gsm_call_status()) {
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    switch (hfp_gsm_callsetup_status()) {
                        case HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS:
                            // note: number not used currently
                            hfp_gsm_handler(HFP_AG_OUTGOING_CALL_INITIATED_BY_AG, 0, 0, "1234567");
                            // init call state for all connections
                            hfp_ag_trigger_outgoing_call();
                            // get first one and accept call
                            hfp_connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_INITIATED);
                            if (hfp_connection) {
                                hfp_ag_handle_outgoing_call_accepted(hfp_connection);
                            }
                            break;
                        default:
                            // TODO: no error for user
                            break;
                    }
                    break;
                default:
                    // TODO: no error for user
                    break;
            }
            break;
        case HFP_AG_OUTGOING_REDIAL_INITIATED:{
            // directly reject call if number of free slots is exceeded
            if (!hfp_gsm_call_possible()){
                hfp_connection->send_error = 1;
                hfp_ag_run_for_context(hfp_connection);  
                break;
            }

            hfp_gsm_handler(HFP_AG_OUTGOING_REDIAL_INITIATED, 0, 0, NULL);
            hfp_connection->call_state = HFP_CALL_OUTGOING_INITIATED;

            log_info("Redial last number");
            char * last_dialed_number = hfp_gsm_last_dialed_number();
            
            if (strlen(last_dialed_number) > 0){
                log_info("Last number exists: accept call");
                hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_PLACE_CALL_WITH_NUMBER, last_dialed_number);
            } else {
                log_info("log_infoLast number missing: reject call");
                hfp_ag_handle_reject_outgoing_call();
            }
            break;
        }
        case HFP_AG_OUTGOING_CALL_REJECTED:
            hfp_ag_handle_reject_outgoing_call();
            break;

        case HFP_AG_OUTGOING_CALL_ACCEPTED:{
            hfp_connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_INITIATED);
            if (!hfp_connection){
                log_info("hfp_ag_call_sm: did not find outgoing hfp_connection in initiated state");
                break;
            }
            
            hfp_connection->ok_pending = 1;
            hfp_ag_handle_outgoing_call_accepted(hfp_connection);
            break;
        }
        case HFP_AG_OUTGOING_CALL_RINGING:
            hfp_connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_DIALING);
            if (!hfp_connection){
                log_info("hfp_ag_call_sm: did not find outgoing hfp_connection in dialing state");
                break;
            }

            hfp_gsm_handler(HFP_AG_OUTGOING_CALL_RINGING, 0, 0, NULL);
            hfp_connection->call_state = HFP_CALL_OUTGOING_RINGING;
            hfp_ag_set_callsetup_indicator();
            hfp_ag_transfer_callsetup_state();
            break;

        case HFP_AG_OUTGOING_CALL_ESTABLISHED:{
            // get outgoing call
            hfp_connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_RINGING);
            if (!hfp_connection){
                hfp_connection = hfp_ag_connection_for_call_state(HFP_CALL_OUTGOING_DIALING);
            }
            if (!hfp_connection){
                log_info("hfp_ag_call_sm: did not find outgoing hfp_connection");
                break;
            }

            int CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS = hfp_gsm_callheld_status() == HFP_CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS;
            hfp_gsm_handler(HFP_AG_OUTGOING_CALL_ESTABLISHED, 0, 0, NULL);
            hfp_connection->call_state = HFP_CALL_ACTIVE;
            hfp_ag_set_callsetup_indicator();
            hfp_ag_set_call_indicator();
            hfp_ag_transfer_call_state();
            hfp_ag_transfer_callsetup_state();
            if (CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS){
                hfp_ag_set_callheld_indicator();
                hfp_ag_transfer_callheld_state();
            }
            break;
        }

        case HFP_AG_CALL_HOLD_USER_BUSY:
            hfp_gsm_handler(HFP_AG_CALL_HOLD_USER_BUSY, 0, 0, NULL);
            hfp_ag_set_callsetup_indicator();
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
            hfp_connection->call_state = HFP_CALL_ACTIVE;
            log_info("AG: Call Waiting, User Busy");
            break;
        
        case HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL:{
            int call_setup_in_progress = hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            int call_held = hfp_gsm_callheld_status() != HFP_CALLHELD_STATUS_NO_CALLS_HELD;
            
            // Releases all active calls (if any exist) and accepts the other (held or waiting) call.
            if (call_held || call_setup_in_progress){
                hfp_gsm_handler(HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, hfp_connection->call_index,
                                0, NULL);

            }

            if (call_setup_in_progress){
                log_info("AG: Call Dropped, Accept new call");
                hfp_ag_set_callsetup_indicator();
                hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
            } else {
                log_info("AG: Call Dropped, Resume held call");
            }
            if (call_held){
                hfp_ag_set_callheld_indicator();
                hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
            }
            
            hfp_connection->call_state = HFP_CALL_ACTIVE;
            break;
        }

        case HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL:{
            // Places all active calls (if any exist) on hold and accepts the other (held or waiting) call.
            // only update if callsetup changed
            int call_setup_in_progress = hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            hfp_gsm_handler(HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, hfp_connection->call_index, 0,
                            NULL);

            if (call_setup_in_progress){
                log_info("AG: Call on Hold, Accept new call");
                hfp_ag_set_callsetup_indicator();
                hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callsetup_indicator_index, 1);
            } else {
                log_info("AG: Swap calls");
            }

            hfp_ag_set_callheld_indicator();
            // hfp_ag_set_callheld_state(HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED);
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
            hfp_connection->call_state = HFP_CALL_ACTIVE;
            break;
        }

        case HFP_AG_CALL_HOLD_ADD_HELD_CALL:
            // Adds a held call to the conversation.
            if (hfp_gsm_callheld_status() != HFP_CALLHELD_STATUS_NO_CALLS_HELD){
                log_info("AG: Join 3-way-call");
                hfp_gsm_handler(HFP_AG_CALL_HOLD_ADD_HELD_CALL, 0, 0, NULL);
                hfp_ag_set_callheld_indicator();
                hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
                hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_CONFERENCE_CALL);
            }
            hfp_connection->call_state = HFP_CALL_ACTIVE;
            break;
        case HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS:
            // Connects the two calls and disconnects the subscriber from both calls (Explicit Call Transfer)
            hfp_gsm_handler(HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS, 0, 0, NULL);
            log_info("AG: Transfer call -> Connect two calls and disconnect");
            hfp_ag_set_call_indicator();
            hfp_ag_set_callheld_indicator();
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, call_indicator_index, 1);
            hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, callheld_indicator_index, 1);
            hfp_connection->call_state = HFP_CALL_IDLE;
            break;
        
        default:
            break;
    }
   

}


static void hfp_ag_send_call_status(hfp_connection_t * hfp_connection, int call_index){
    hfp_gsm_call_t * active_call = hfp_gsm_call(call_index);
    if (!active_call) return;

    int idx = active_call->index;
    hfp_enhanced_call_dir_t dir = active_call->direction;
    hfp_enhanced_call_status_t status = active_call->enhanced_status;
    hfp_enhanced_call_mode_t mode = active_call->mode;
    hfp_enhanced_call_mpty_t mpty = active_call->mpty;
    uint8_t type = active_call->clip_type;
    char * number = active_call->clip_number;

    char buffer[100];
    // TODO: check length of a buffer, to fit the MTU
    int offset = snprintf(buffer, sizeof(buffer), "\r\n%s: %d,%d,%d,%d,%d", HFP_LIST_CURRENT_CALLS, idx, dir, status, mode, mpty);
    if (number){
        offset += snprintf(buffer+offset, sizeof(buffer)-offset-3, ", \"%s\",%u", number, type);
    } 
    snprintf(buffer+offset, sizeof(buffer)-offset, "\r\n");
    log_info("hfp_ag_send_current_call_status 000 index %d, dir %d, status %d, mode %d, mpty %d, type %d, number %s", idx, dir, status,
       mode, mpty, type, number);
    send_str_over_rfcomm(hfp_connection->rfcomm_cid, buffer);
}


// sends pending command, returns if command was sent
static int hfp_ag_send_commands(hfp_connection_t *hfp_connection){
    
    if (hfp_connection->send_status_of_current_calls){
        hfp_connection->ok_pending = 0; 
        if (hfp_connection->next_call_index < hfp_gsm_get_number_of_calls()){
            hfp_connection->next_call_index++;
            hfp_ag_send_call_status(hfp_connection, hfp_connection->next_call_index);
            return 1;
        } else {
            // TODO: this code should be removed. check a) before setting send_status_of_current_calls, or b) right before hfp_ag_send_call_status above
            hfp_connection->next_call_index = 0;
            hfp_connection->ok_pending = 1;
            hfp_connection->send_status_of_current_calls = 0;
        }
    } 

    if (hfp_connection->ag_notify_incoming_call_waiting){
        hfp_connection->ag_notify_incoming_call_waiting = 0;
        hfp_ag_send_call_waiting_notification(hfp_connection->rfcomm_cid);
        return 1;
    }

    if (hfp_connection->command == HFP_CMD_UNKNOWN){
        hfp_connection->ok_pending = 0;
        hfp_connection->send_error = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_error(hfp_connection->rfcomm_cid);
        return 1;
    }

    if (hfp_connection->send_error){
        hfp_connection->send_error = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_error(hfp_connection->rfcomm_cid); 
        return 1;
    }

    // note: before update AG indicators and ok_pending 
    if (hfp_connection->send_response_and_hold_status){
        int status = hfp_connection->send_response_and_hold_status - 1;
        hfp_connection->send_response_and_hold_status = 0;
        hfp_ag_send_set_response_and_hold(hfp_connection->rfcomm_cid, status);
        return 1;
    }

    if (hfp_connection->ok_pending){
        hfp_connection->ok_pending = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_ok(hfp_connection->rfcomm_cid);
        return 1;
    }

    // update AG indicators
    if (hfp_connection->ag_indicators_status_update_bitmap){
        int i;
        for (i=0;i<hfp_connection->ag_indicators_nr;i++){
            if (get_bit(hfp_connection->ag_indicators_status_update_bitmap, i)){
                hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, i, 0);
                if (!hfp_connection->enable_status_update_for_ag_indicators) {
                    log_info("+CMER:3,0,0,0 - not sending update for '%s'", hfp_ag_indicators[i].name);
                    break;
                }
                hfp_ag_send_transfer_ag_indicators_status_cmd(hfp_connection->rfcomm_cid, &hfp_ag_indicators[i]);
                return 1;
            }
        }
    }

    if (hfp_connection->ag_send_no_carrier){
        hfp_connection->ag_send_no_carrier = false;
        hfp_ag_send_no_carrier(hfp_connection->rfcomm_cid);
        return 1;
    }

    if (hfp_connection->ag_ring){
        hfp_connection->ag_ring = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_ring(hfp_connection->rfcomm_cid);
        return 1;
    }

    if (hfp_connection->ag_send_clip){
        hfp_connection->ag_send_clip = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_clip(hfp_connection->rfcomm_cid);
        return 1;
    }
    
    if (hfp_connection->send_phone_number_for_voice_tag){
        hfp_connection->send_phone_number_for_voice_tag = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_connection->ok_pending = 1;
        hfp_ag_send_phone_number_for_voice_tag_cmd(hfp_connection->rfcomm_cid);
        return 1;
    }

    if (hfp_connection->send_subscriber_number){
        if (hfp_connection->next_subscriber_number_to_send < subscriber_numbers_count){
            hfp_phone_number_t phone = subscriber_numbers[hfp_connection->next_subscriber_number_to_send++];
            hfp_send_subscriber_number_cmd(hfp_connection->rfcomm_cid, phone.type, phone.number);
        } else {
            hfp_connection->send_subscriber_number = 0;
            hfp_connection->next_subscriber_number_to_send = 0;
            hfp_ag_send_ok(hfp_connection->rfcomm_cid);
        }
        hfp_connection->command = HFP_CMD_NONE;
        return 1;
    }

    if (hfp_connection->send_microphone_gain){
        hfp_connection->send_microphone_gain = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_set_microphone_gain_cmd(hfp_connection->rfcomm_cid, hfp_connection->microphone_gain);
        return 1;
    }
    
    if (hfp_connection->send_speaker_gain){
        hfp_connection->send_speaker_gain = 0;
        hfp_connection->command = HFP_CMD_NONE;
        hfp_ag_send_set_speaker_gain_cmd(hfp_connection->rfcomm_cid, hfp_connection->speaker_gain);
        return 1;
    }
    
    if (hfp_connection->send_ag_status_indicators){
        hfp_connection->send_ag_status_indicators = 0;
        hfp_ag_send_retrieve_indicators_status_cmd(hfp_connection->rfcomm_cid);
        return 1;
    }

    return 0;
}

static void hfp_ag_run_for_context(hfp_connection_t *hfp_connection){

	btstack_assert(hfp_connection != NULL);
	btstack_assert(hfp_connection->local_role == HFP_ROLE_AG);

	// during SDP query, RFCOMM CID is not set
	if (hfp_connection->rfcomm_cid == 0) return;

    // assert command could be sent
    if (hci_can_send_command_packet_now() == 0) return;
    
    if ((hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) && hfp_connection->release_audio_connection){
        hfp_connection->state = HFP_W4_SCO_DISCONNECTED;
        hfp_connection->release_audio_connection = 0;
        gap_disconnect(hfp_connection->sco_handle);
        return;
    }
    
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
#if defined (ENABLE_CC256X_ASSISTED_HFP) || defined (ENABLE_BCM_PCM_WBS)
    if (hfp_connection->state == HFP_W4_WBS_SHUTDOWN){
        hfp_finalize_connection_context(hfp_connection);
        return;
    }
#endif

    if (!rfcomm_can_send_packet_now(hfp_connection->rfcomm_cid)) {
        log_info("hfp_ag_run_for_context: request can send for 0x%02x", hfp_connection->rfcomm_cid);
        rfcomm_request_can_send_now_event(hfp_connection->rfcomm_cid);
        return;
    }

    int cmd_sent = hfp_ag_send_commands(hfp_connection);

    if (!cmd_sent){
        cmd_sent = hfp_ag_run_for_context_service_level_connection(hfp_connection);
    }

    if (!cmd_sent){
        cmd_sent = hfp_ag_run_for_context_service_level_connection_queries(hfp_connection);
    } 

    if (!cmd_sent){
        cmd_sent = call_setup_state_machine(hfp_connection);
    }

    // trigger codec exchange (must be before hfp_ag_run_for_audio_connection)
    if (!cmd_sent && (hfp_connection->command == HFP_CMD_NONE) && hfp_connection->trigger_codec_exchange){
        switch (hfp_connection->codecs_state){
            case HFP_CODECS_IDLE:
            case HFP_CODECS_RECEIVED_LIST:
            case HFP_CODECS_AG_RESEND_COMMON_CODEC:
            case HFP_CODECS_ERROR:
                hfp_connection->trigger_codec_exchange = 0;
                hfp_connection->command = HFP_CMD_AG_SEND_COMMON_CODEC;
                break;
            default:
                break;
        }
    }

    if (!cmd_sent){
        cmd_sent = hfp_ag_run_for_audio_connection(hfp_connection);
    }


    // disconnect
    if (!cmd_sent && (hfp_connection->command == HFP_CMD_NONE) && (hfp_connection->state == HFP_W2_DISCONNECT_RFCOMM)){
        hfp_connection->state = HFP_W4_RFCOMM_DISCONNECTED;
        rfcomm_disconnect(hfp_connection->rfcomm_cid);
    }
    
    if (cmd_sent){
        hfp_connection->command = HFP_CMD_NONE;
        rfcomm_request_can_send_now_event(hfp_connection->rfcomm_cid); 
    }
}

static int hfp_parser_is_end_of_line(uint8_t byte){
    return (byte == '\n') || (byte == '\r');
}

static void hfp_ag_handle_rfcomm_data(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);    // ok: only called with RFCOMM_DATA_PACKET

    // assertion: size >= 1 as rfcomm.c does not deliver empty packets
    if (size < 1) return;

    hfp_connection_t * hfp_connection = get_hfp_connection_context_for_rfcomm_cid(channel);
    if (!hfp_connection) return;
    
    hfp_log_rfcomm_message("HFP_AG_RX", packet, size);
#ifdef ENABLE_HFP_AT_MESSAGES
    hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_AT_MESSAGE_RECEIVED, (char *) packet);
#endif

    // process messages byte-wise
    int pos;
    for (pos = 0; pos < size ; pos++){
        hfp_parse(hfp_connection, packet[pos], 0);

        // parse until end of line
        if (!hfp_parser_is_end_of_line(packet[pos])) continue;

        hfp_generic_status_indicator_t * indicator;
        switch(hfp_connection->command){
            case HFP_CMD_RESPONSE_AND_HOLD_QUERY:
                if (hfp_ag_response_and_hold_active){
                    hfp_connection->send_response_and_hold_status = HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD + 1;
                }
                hfp_connection->ok_pending = 1;
                break;
            case HFP_CMD_RESPONSE_AND_HOLD_COMMAND:
                switch(hfp_connection->ag_response_and_hold_action){
                    case HFP_RESPONSE_AND_HOLD_INCOMING_ON_HOLD:
                        hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF, hfp_connection);
                        break;
                    case HFP_RESPONSE_AND_HOLD_HELD_INCOMING_ACCEPTED:
                        hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF, hfp_connection);
                        break;
                    case HFP_RESPONSE_AND_HOLD_HELD_INCOMING_REJECTED:
                        hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF, hfp_connection);
                        break;
                    default:
                        break;
                }
                hfp_connection->ok_pending = 1;
                break;
            case HFP_CMD_HF_INDICATOR_STATUS:
                hfp_connection->command = HFP_CMD_NONE;
                if (hfp_connection->parser_indicator_index < hfp_generic_status_indicators_nr){
                    indicator = &hfp_generic_status_indicators[hfp_connection->parser_indicator_index];
                } else {
                    hfp_connection->send_error = 1;
                    break;
                }
                switch (indicator->uuid){
                    case 1: // enhanced security
                        if (hfp_connection->parser_indicator_value > 1) {
                            hfp_connection->send_error = 1;
                            return;
                        }
                        log_info("HF Indicator 'enhanced security' set to %u", (unsigned int) hfp_connection->parser_indicator_value);
                        break;
                    case 2: // battery level
                        if (hfp_connection->parser_indicator_value > 100){
                            hfp_connection->send_error = 1;
                            return;
                        }
                        log_info("HF Indicator 'battery' set to %u", (unsigned int) hfp_connection->parser_indicator_value);
                        break;
                    default:
                        log_info("HF Indicator unknown set to %u", (unsigned int) hfp_connection->parser_indicator_value);
                        break;
                }
                hfp_connection->ok_pending = 1;
                break;
            case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
                // expected by SLC state machine
                if (hfp_connection->state < HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED) break;
                hfp_connection->send_ag_indicators_segment = 0;
                hfp_connection->send_ag_status_indicators = 1;
                break;
            case HFP_CMD_LIST_CURRENT_CALLS:   
                hfp_connection->command = HFP_CMD_NONE;
                hfp_connection->next_call_index = 0;
                hfp_connection->send_status_of_current_calls = 1;
                break;
            case HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION:
                if (subscriber_numbers_count == 0){
                    hfp_ag_send_ok(hfp_connection->rfcomm_cid);
                    break;
                }
                hfp_connection->next_subscriber_number_to_send = 0;
                hfp_connection->send_subscriber_number = 1;
                break;
            case HFP_CMD_TRANSMIT_DTMF_CODES:
            {
                hfp_connection->command = HFP_CMD_NONE;
                char buffer[2];
                buffer[0] = (char) hfp_connection->ag_dtmf_code;
                buffer[1] = 0;
                hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_TRANSMIT_DTMF_CODES, buffer);
                break;
            }
            case HFP_CMD_HF_REQUEST_PHONE_NUMBER:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_emit_simple_event(hfp_connection, HFP_SUBEVENT_ATTACH_NUMBER_TO_VOICE_TAG);
                break;
            case HFP_CMD_TURN_OFF_EC_AND_NR:
                hfp_connection->command = HFP_CMD_NONE;
                if (get_bit(hfp_supported_features, HFP_AGSF_EC_NR_FUNCTION)){
                    hfp_connection->ok_pending = 1;
                    hfp_supported_features = store_bit(hfp_supported_features, HFP_AGSF_EC_NR_FUNCTION, hfp_connection->ag_echo_and_noise_reduction);
                    log_info("AG: EC/NR = %u", hfp_connection->ag_echo_and_noise_reduction);
                } else {
                    hfp_connection->send_error = 1;
                }
                break;
            case HFP_CMD_CALL_ANSWERED:
                hfp_connection->command = HFP_CMD_NONE;
                log_info("HFP: ATA");
                hfp_ag_call_sm(HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF, hfp_connection);
                break;
            case HFP_CMD_HANG_UP_CALL:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_connection->ok_pending = 1;
                hfp_ag_call_sm(HFP_AG_TERMINATE_CALL_BY_HF, hfp_connection);
                break;
            case HFP_CMD_CALL_HOLD: {
                hfp_connection->command = HFP_CMD_NONE;
                hfp_connection->ok_pending = 1;

                switch (hfp_connection->ag_call_hold_action){
                    case 0:
                        // Releases all held calls or sets User Determined User Busy (UDUB) for a waiting call.
                        hfp_ag_call_sm(HFP_AG_CALL_HOLD_USER_BUSY, hfp_connection);
                        break;
                    case 1:
                        // Releases all active calls (if any exist) and accepts the other (held or waiting) call.
                        // Where both a held and a waiting call exist, the above procedures shall apply to the
                        // waiting call (i.e., not to the held call) in conflicting situation.
                        hfp_ag_call_sm(HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, hfp_connection);
                        break;
                    case 2:
                        // Places all active calls (if any exist) on hold and accepts the other (held or waiting) call.
                        // Where both a held and a waiting call exist, the above procedures shall apply to the
                        // waiting call (i.e., not to the held call) in conflicting situation.
                        hfp_ag_call_sm(HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL, hfp_connection);
                        break;
                    case 3:
                        // Adds a held call to the conversation.
                        hfp_ag_call_sm(HFP_AG_CALL_HOLD_ADD_HELD_CALL, hfp_connection);
                        break;
                    case 4:
                        // Connects the two calls and disconnects the subscriber from both calls (Explicit Call Transfer).
                        hfp_ag_call_sm(HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS, hfp_connection);
                        break;
                    default:
                        break;
                }
				hfp_connection->call_index = 0;
                break;
            }
            case HFP_CMD_CALL_PHONE_NUMBER:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_INITIATED_BY_HF, hfp_connection);
                break;
            case HFP_CMD_REDIAL_LAST_NUMBER:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_ag_call_sm(HFP_AG_OUTGOING_REDIAL_INITIATED, hfp_connection);
                break;
            case HFP_CMD_ENABLE_CLIP:
                hfp_connection->command = HFP_CMD_NONE;
                log_info("hfp: clip set, now: %u", hfp_connection->clip_enabled);
                hfp_connection->ok_pending = 1;
                break;
            case HFP_CMD_ENABLE_CALL_WAITING_NOTIFICATION:
                hfp_connection->command = HFP_CMD_NONE;
                log_info("hfp: call waiting notification set, now: %u", hfp_connection->call_waiting_notification_enabled);
                hfp_connection->ok_pending = 1;
                break;
            case HFP_CMD_SET_SPEAKER_GAIN:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_connection->ok_pending = 1;
                log_info("HF speaker gain = %u", hfp_connection->speaker_gain);
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_SPEAKER_VOLUME, hfp_connection->speaker_gain);
                break;
            case HFP_CMD_SET_MICROPHONE_GAIN:
                hfp_connection->command = HFP_CMD_NONE;
                hfp_connection->ok_pending = 1;
                log_info("HF microphone gain = %u", hfp_connection->microphone_gain);
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_MICROPHONE_VOLUME, hfp_connection->microphone_gain);
                break;
            default:
                break;
        }
    }
}

static void hfp_ag_run(void){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_ag_run_for_context(hfp_connection);
    }
}

static void hfp_ag_rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type){
        case RFCOMM_DATA_PACKET:
            hfp_ag_handle_rfcomm_data(packet_type, channel, packet, size);
            break;
        case HCI_EVENT_PACKET:
            if (packet[0] == RFCOMM_EVENT_CAN_SEND_NOW){
                uint16_t rfcomm_cid = rfcomm_event_can_send_now_get_rfcomm_cid(packet);
                hfp_ag_run_for_context(get_hfp_connection_context_for_rfcomm_cid(rfcomm_cid));
                return;
            }
            hfp_handle_rfcomm_event(packet_type, channel, packet, size, HFP_ROLE_AG);
            break;
        default:
            break;
    }

    hfp_ag_run();
}

static void hfp_ag_hci_event_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    hfp_handle_hci_event(packet_type, channel, packet, size, HFP_ROLE_AG);
    hfp_ag_run();
}

void hfp_ag_init_codecs(int codecs_nr, const uint8_t * codecs){
    if (codecs_nr > HFP_MAX_NUM_CODECS){
        log_error("hfp_init: codecs_nr (%d) > HFP_MAX_NUM_CODECS (%d)", codecs_nr, HFP_MAX_NUM_CODECS);
        return;
    }
    int i;
    hfp_codecs_nr = codecs_nr;
    for (i=0; i < codecs_nr; i++){
        hfp_codecs[i] = codecs[i];
    }
}

void hfp_ag_init_supported_features(uint32_t supported_features){
    hfp_supported_features = supported_features;
}

void hfp_ag_init_ag_indicators(int ag_indicators_nr, const hfp_ag_indicator_t * ag_indicators){
    hfp_ag_indicators_nr = ag_indicators_nr;
    (void)memcpy(hfp_ag_indicators, ag_indicators,
                 ag_indicators_nr * sizeof(hfp_ag_indicator_t));
}

void hfp_ag_init_hf_indicators(int hf_indicators_nr, const hfp_generic_status_indicator_t * hf_indicators){
    if (hf_indicators_nr > HFP_MAX_NUM_INDICATORS) return;
    hfp_generic_status_indicators_nr = hf_indicators_nr;
    (void)memcpy(hfp_generic_status_indicators, hf_indicators,
                 hf_indicators_nr * sizeof(hfp_generic_status_indicator_t));
}

void hfp_ag_init_call_hold_services(int call_hold_services_nr, const char * call_hold_services[]){
    hfp_ag_call_hold_services_nr = call_hold_services_nr;
    (void)memcpy(hfp_ag_call_hold_services, call_hold_services,
                 call_hold_services_nr * sizeof(char *));
}


void hfp_ag_init(uint16_t rfcomm_channel_nr){

    hfp_init();
    hfp_ag_call_hold_services_nr = 0;
    hfp_ag_response_and_hold_active = 0;
    hfp_ag_indicators_nr = 0;
    hfp_codecs_nr = 0;
    hfp_supported_features = HFP_DEFAULT_AG_SUPPORTED_FEATURES;
    subscriber_numbers = NULL;
    subscriber_numbers_count = 0;

    hfp_ag_hci_event_callback_registration.callback = &hfp_ag_hci_event_packet_handler;
    hci_add_event_handler(&hfp_ag_hci_event_callback_registration);

    rfcomm_register_service(&hfp_ag_rfcomm_packet_handler, rfcomm_channel_nr, 0xffff);

    // used to set packet handler for outgoing rfcomm connections - could be handled by emitting an event to us
    hfp_set_ag_rfcomm_packet_handler(&hfp_ag_rfcomm_packet_handler);

    hfp_gsm_init();
}

void hfp_ag_deinit(void){
    hfp_deinit();
    hfp_gsm_deinit();
    (void) memset(&hfp_ag_hci_event_callback_registration, 0, sizeof(btstack_packet_callback_registration_t));
    (void) memset(&hfp_ag_callback, 0, sizeof(btstack_packet_handler_t));
    (void) memset(&hfp_ag_call_hold_services, 0, sizeof(hfp_ag_call_hold_services));
    (void) memset(&hfp_ag_response_and_hold_state, 0, sizeof(hfp_response_and_hold_state_t));
}

void hfp_ag_establish_service_level_connection(bd_addr_t bd_addr){
    hfp_establish_service_level_connection(bd_addr, BLUETOOTH_SERVICE_CLASS_HANDSFREE, HFP_ROLE_AG);
}

void hfp_ag_release_service_level_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_release_service_level_connection(hfp_connection);
    hfp_ag_run_for_context(hfp_connection);
}

void hfp_ag_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle, hfp_cme_error_t error){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->extended_audio_gateway_error = 0;
    if (!hfp_connection->enable_extended_audio_gateway_error_report){
        return;
    }
    hfp_connection->extended_audio_gateway_error = error;
    hfp_ag_run_for_context(hfp_connection);
}

static void hfp_ag_setup_audio_connection(hfp_connection_t * hfp_connection){
    log_info("hfp_ag_setup_audio_connection state %u", hfp_connection->state);
    if (hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED) return;
    if (hfp_connection->state >= HFP_W2_DISCONNECT_SCO) return;
        
    hfp_connection->establish_audio_connection = 1;
    if (!has_codec_negotiation_feature(hfp_connection)){
        log_info("hfp_ag_establish_audio_connection - no codec negotiation feature, using CVSD");
        hfp_connection->negotiated_codec = HFP_CODEC_CVSD;
        hfp_connection->codecs_state = HFP_CODECS_EXCHANGED;
        // now, pick link settings
        hfp_init_link_settings(hfp_connection, hfp_ag_esco_s4_supported(hfp_connection));
#ifdef ENABLE_CC256X_ASSISTED_HFP
        hfp_cc256x_prepare_for_sco(hfp_connection);
#endif
        return;
    } 

    uint8_t i;
    bool codec_was_in_use = false;
    bool better_codec_can_be_used = false;

    for (i = 0; i<hfp_connection->remote_codecs_nr; i++){
        if (hfp_connection->negotiated_codec == hfp_connection->remote_codecs[i]){
            codec_was_in_use = true;
        } else if (hfp_connection->negotiated_codec < hfp_connection->remote_codecs[i]){
            better_codec_can_be_used = true;
        }
    }
    
    if (!codec_was_in_use || better_codec_can_be_used){
        hfp_connection->trigger_codec_exchange = 1;
        hfp_connection->codecs_state = HFP_CODECS_IDLE;
    }
}

void hfp_ag_establish_audio_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }

    hfp_connection->establish_audio_connection = 0;
    hfp_ag_setup_audio_connection(hfp_connection);
    hfp_ag_run_for_context(hfp_connection);
}

void hfp_ag_release_audio_connection(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_release_audio_connection(hfp_connection);
    hfp_ag_run_for_context(hfp_connection);
}

/**
 * @brief Enable in-band ring tone
 */
void hfp_ag_set_use_in_band_ring_tone(int use_in_band_ring_tone){
    if (get_bit(hfp_supported_features, HFP_AGSF_IN_BAND_RING_TONE) == use_in_band_ring_tone){
        return;
    } 
    hfp_supported_features = store_bit(hfp_supported_features, HFP_AGSF_IN_BAND_RING_TONE, use_in_band_ring_tone);
        
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        hfp_connection->command = HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING;
        hfp_ag_run_for_context(hfp_connection);
    }
}

/**
 * @brief Called from GSM
 */
void hfp_ag_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_INCOMING_CALL, NULL);
}

void hfp_ag_outgoing_call_initiated(const char * number) {
    UNUSED(number);
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_INITIATED_BY_AG, NULL);
}

/**
 * @brief number is stored.
 */
void hfp_ag_set_clip(uint8_t type, const char * number){
    hfp_gsm_handler(HFP_AG_SET_CLIP, 0, type, number);
}

void hfp_ag_call_dropped(void){
    hfp_ag_call_sm(HFP_AG_CALL_DROPPED, NULL);
}

// call from AG UI
void hfp_ag_answer_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG, NULL);
}

void hfp_ag_join_held_call(void){
    hfp_ag_call_sm(HFP_AG_HELD_CALL_JOINED_BY_AG, NULL);
}

void hfp_ag_terminate_call(void){
    hfp_ag_call_sm(HFP_AG_TERMINATE_CALL_BY_AG, NULL);
}

void hfp_ag_outgoing_call_ringing(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_RINGING, NULL);
}

void hfp_ag_outgoing_call_established(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_ESTABLISHED, NULL);
}

void hfp_ag_outgoing_call_rejected(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_REJECTED, NULL);
}

void hfp_ag_outgoing_call_accepted(void){
    hfp_ag_call_sm(HFP_AG_OUTGOING_CALL_ACCEPTED, NULL);
}

void hfp_ag_hold_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG, NULL);
}

void hfp_ag_accept_held_incoming_call(void) {
    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG, NULL);
}

void hfp_ag_reject_held_incoming_call(void){
    hfp_ag_call_sm(HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG, NULL);
}

static void hfp_ag_set_ag_indicator(const char * name, int value){
    int indicator_index = get_ag_indicator_index_for_name(name);
    if (indicator_index < 0) return;
    hfp_ag_indicators[indicator_index].status = value;

    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->local_role != HFP_ROLE_AG) continue;
        if (!hfp_connection->ag_indicators[indicator_index].enabled) {
            log_info("Requested AG indicator '%s' update to %u, but it is not enabled", hfp_ag_indicators[indicator_index].name, value);
            continue;
        }
        log_info("AG indicator '%s' changed to %u, request transfer status", hfp_ag_indicators[indicator_index].name, value);
        hfp_connection->ag_indicators_status_update_bitmap = store_bit(hfp_connection->ag_indicators_status_update_bitmap, indicator_index, 1);
        hfp_ag_run_for_context(hfp_connection);
    }    
}

void hfp_ag_set_registration_status(int status){
    if ( (status == 0) && (hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT)){

        // if network goes away wihle a call is active:
        // - the  call gets dropped
        // - we send NO CARRIER
        // NOTE: the CALL=0 has to be sent before NO CARRIER

        hfp_ag_call_sm(HFP_AG_CALL_DROPPED, NULL);

        btstack_linked_list_iterator_t it;    
        btstack_linked_list_iterator_init(&it, hfp_get_connections());
        while (btstack_linked_list_iterator_has_next(&it)){
            hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
            hfp_connection->ag_send_no_carrier = true;
        } 
    }
    hfp_ag_set_ag_indicator("service", status);
}

void hfp_ag_set_signal_strength(int strength){
    hfp_ag_set_ag_indicator("signal", strength);
}

void hfp_ag_set_roaming_status(int status){
    hfp_ag_set_ag_indicator("roam", status);
}

void hfp_ag_set_battery_level(int level){
    hfp_ag_set_ag_indicator("battchg", level);
}

void hfp_ag_activate_voice_recognition(hci_con_handle_t acl_handle, int activate){
    if (!get_bit(hfp_supported_features, HFP_AGSF_VOICE_RECOGNITION_FUNCTION)) return;
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    
    if (!get_bit(hfp_connection->remote_supported_features, HFP_HFSF_VOICE_RECOGNITION_FUNCTION)) {
        log_info("AG cannot acivate voice recognition - not supported by HF");
        return;
    }

    hfp_connection->ag_activate_voice_recognition = activate;
    hfp_connection->command = HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION;
    hfp_ag_run_for_context(hfp_connection);
}

void hfp_ag_set_microphone_gain(hci_con_handle_t acl_handle, int gain){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    if (hfp_connection->microphone_gain != gain){
        hfp_connection->command = HFP_CMD_SET_MICROPHONE_GAIN;
        hfp_connection->microphone_gain = gain;
        hfp_connection->send_microphone_gain = 1;
    } 
    hfp_ag_run_for_context(hfp_connection);
}

void hfp_ag_set_speaker_gain(hci_con_handle_t acl_handle, int gain){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    if (hfp_connection->speaker_gain != gain){
        hfp_connection->speaker_gain = gain;
        hfp_connection->send_speaker_gain = 1;
    } 
    hfp_ag_run_for_context(hfp_connection);
}

void hfp_ag_send_phone_number_for_voice_tag(hci_con_handle_t acl_handle, const char * number){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_ag_set_clip(0, number);
    hfp_connection->send_phone_number_for_voice_tag = 1;
}

void hfp_ag_reject_phone_number_for_voice_tag(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->send_error = 1;
}

void hfp_ag_send_dtmf_code_done(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    hfp_connection->ok_pending = 1;
}

void hfp_ag_set_subcriber_number_information(hfp_phone_number_t * numbers, int numbers_count){
    subscriber_numbers = numbers;
    subscriber_numbers_count = numbers_count;
}

void hfp_ag_clear_last_dialed_number(void){
    hfp_gsm_clear_last_dialed_number();
}

void hfp_ag_set_last_dialed_number(const char * number){
    hfp_gsm_set_last_dialed_number(number);
}

void hfp_ag_notify_incoming_call_waiting(hci_con_handle_t acl_handle){
    hfp_connection_t * hfp_connection = get_hfp_ag_connection_context_for_acl_handle(acl_handle);
    if (!hfp_connection){
        log_error("HFP AG: ACL connection 0x%2x is not found.", acl_handle);
        return;
    }
    if (!hfp_connection->call_waiting_notification_enabled) return;
    
    hfp_connection->ag_notify_incoming_call_waiting = 1;
    hfp_ag_run_for_context(hfp_connection);
}

void hfp_ag_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint8_t ability_to_reject_call, uint16_t supported_features, int wide_band_speech){
	if (!name){
		name = default_hfp_ag_service_name;
	}
	hfp_create_sdp_record(service, service_record_handle, BLUETOOTH_SERVICE_CLASS_HANDSFREE_AUDIO_GATEWAY, rfcomm_channel_nr, name);

	/*
	 * 0x01 – Ability to reject a call
	 * 0x00 – No ability to reject a call
	 */
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x0301);    // Hands-Free Profile - Network
	de_add_number(service, DE_UINT, DE_SIZE_8, ability_to_reject_call);

	// Construct SupportedFeatures for SDP bitmap:
	//
	// "The values of the “SupportedFeatures” bitmap given in Table 5.4 shall be the same as the values
	//  of the Bits 0 to 4 of the unsolicited result code +BRSF"
	//
	// Wide band speech (bit 5) requires Codec negotiation
	//
	uint16_t sdp_features = supported_features & 0x1f;
	if ( (wide_band_speech == 1) && (supported_features & (1 << HFP_AGSF_CODEC_NEGOTIATION))){
		sdp_features |= 1 << 5;
	}

    if (supported_features & (1 << HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS)){
        sdp_features |= 1 << 6;
    }
    
    if (supported_features & (1 << HFP_AGSF_VOICE_RECOGNITION_TEXT)){
        sdp_features |= 1 << 7;
    }
    
	de_add_number(service, DE_UINT, DE_SIZE_16, 0x0311);    // Hands-Free Profile - SupportedFeatures
	de_add_number(service, DE_UINT, DE_SIZE_16, sdp_features);
}

void hfp_ag_register_packet_handler(btstack_packet_handler_t callback){
	if (callback == NULL){
		log_error("hfp_ag_register_packet_handler called with NULL callback");
		return;
	}
	hfp_ag_callback = callback;
	hfp_set_ag_callback(callback);
}
