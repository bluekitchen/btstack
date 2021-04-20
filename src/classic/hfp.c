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

#define BTSTACK_FILE__ "hfp.c"
 

#include "btstack_config.h"

#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "classic/sdp_client.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"

#if defined(ENABLE_CC256X_ASSISTED_HFP) && !defined(ENABLE_SCO_OVER_PCM)
#error "Assisted HFP is only possible over PCM/I2S. Please add define: ENABLE_SCO_OVER_PCM"
#endif

#if defined(ENABLE_BCM_PCM_WBS) && !defined(ENABLE_SCO_OVER_PCM)
#error "WBS for PCM is only possible over PCM/I2S. Please add define: ENABLE_SCO_OVER_PCM"
#endif

#define HFP_HF_FEATURES_SIZE 10
#define HFP_AG_FEATURES_SIZE 12

typedef struct {
    hfp_role_t local_role;
    bd_addr_t  remote_address;
} hfp_sdp_query_context_t;

// globals

static hfp_sdp_query_context_t sdp_query_context;
static btstack_context_callback_registration_t hfp_handle_sdp_client_query_request;

static btstack_linked_list_t hfp_connections ;

static btstack_packet_handler_t hfp_hf_callback;
static btstack_packet_handler_t hfp_ag_callback;

static btstack_packet_handler_t hfp_hf_rfcomm_packet_handler;
static btstack_packet_handler_t hfp_ag_rfcomm_packet_handler;

static hfp_connection_t * sco_establishment_active;

static uint16_t hfp_allowed_sco_packet_types;

// prototypes
static hfp_link_settings_t hfp_next_link_setting_for_connection(hfp_link_settings_t current_setting, hfp_connection_t * hfp_connection, uint8_t eSCO_S4_supported);
static void parse_sequence(hfp_connection_t * context);


static const char * hfp_hf_features[] = {
        "EC and/or NR function",
        "Three-way calling",
        "CLI presentation capability",
        "Voice recognition activation",
        "Remote volume control",

        "Enhanced call status",
        "Enhanced call control",

        "Codec negotiation",

        "HF Indicators",
        "eSCO S4 (and T2) Settings Supported",
        "Reserved for future definition"
};

static const char * hfp_ag_features[] = {
        "Three-way calling",
        "EC and/or NR function",
        "Voice recognition function",
        "In-band ring tone capability",
        "Attach a number to a voice tag",
        "Ability to reject a call",
        "Enhanced call status",
        "Enhanced call control",
        "Extended Error Result Codes",
        "Codec negotiation",
        "HF Indicators",
        "eSCO S4 (and T2) Settings Supported",
        "Reserved for future definition"
};

static const char * hfp_enhanced_call_dir[] = {
        "outgoing",
        "incoming"
};

static const char * hfp_enhanced_call_status[] = {
        "active",
        "held",
        "outgoing dialing",
        "outgoing alerting",
        "incoming",
        "incoming waiting",
        "call held by response and hold"
};

static const char * hfp_enhanced_call_mode[] = {
        "voice",
        "data",
        "fax"
};

static const char * hfp_enhanced_call_mpty[] = {
        "not a conference call",
        "conference call"
};

const char * hfp_enhanced_call_dir2str(uint16_t index){
    if (index <= HFP_ENHANCED_CALL_DIR_INCOMING) return hfp_enhanced_call_dir[index];
    return "not defined";
}

const char * hfp_enhanced_call_status2str(uint16_t index){
    if (index <= HFP_ENHANCED_CALL_STATUS_CALL_HELD_BY_RESPONSE_AND_HOLD) return hfp_enhanced_call_status[index];
    return "not defined";
}

const char * hfp_enhanced_call_mode2str(uint16_t index){
    if (index <= HFP_ENHANCED_CALL_MODE_FAX) return hfp_enhanced_call_mode[index];
    return "not defined";
}

const char * hfp_enhanced_call_mpty2str(uint16_t index){
    if (index <= HFP_ENHANCED_CALL_MPTY_CONFERENCE_CALL) return hfp_enhanced_call_mpty[index];
    return "not defined";
}

static uint16_t hfp_parse_indicator_index(hfp_connection_t * hfp_connection){
    uint16_t index = btstack_atoi((char *)&hfp_connection->line_buffer[0]);

    if (index > HFP_MAX_NUM_INDICATORS){
        log_info("ignoring invalid indicator index bigger then HFP_MAX_NUM_INDICATORS");
        return HFP_MAX_NUM_INDICATORS - 1;
    } 

    // indicator index enumeration starts with 1, we substract 1 to store in array with starting index 0
    if (index > 0){
        index -= 1;
    } else {
        log_info("ignoring invalid indicator index 0");
        return 0;
    }
    return index;
}

static void hfp_next_indicators_index(hfp_connection_t * hfp_connection){
    if (hfp_connection->parser_item_index < HFP_MAX_NUM_INDICATORS - 1){
        hfp_connection->parser_item_index++;
    } else {
        log_info("Ignoring additional indicator");
    }
}

static void hfp_next_codec_index(hfp_connection_t * hfp_connection){
    if (hfp_connection->parser_item_index < HFP_MAX_NUM_CODECS - 1){
        hfp_connection->parser_item_index++;
    } else {
        log_info("Ignoring additional codec index");
    }
}

static void hfp_next_remote_call_services_index(hfp_connection_t * hfp_connection){
    if (hfp_connection->remote_call_services_index < HFP_MAX_NUM_CALL_SERVICES - 1){
        hfp_connection->remote_call_services_index++;
    } else {
        log_info("Ignoring additional remote_call_services");
    }
}

const char * hfp_hf_feature(int index){
    if (index > HFP_HF_FEATURES_SIZE){
        return hfp_hf_features[HFP_HF_FEATURES_SIZE];
    }
    return hfp_hf_features[index];
}

const char * hfp_ag_feature(int index){
    if (index > HFP_AG_FEATURES_SIZE){
        return hfp_ag_features[HFP_AG_FEATURES_SIZE];
    }
    return hfp_ag_features[index];
}

int send_str_over_rfcomm(uint16_t cid, char * command){
    if (!rfcomm_can_send_packet_now(cid)) return 1;
    log_info("HFP_TX %s", command);
    int err = rfcomm_send(cid, (uint8_t*) command, strlen(command));
    if (err){
        log_error("rfcomm_send -> error 0x%02x \n", err);
    } 
#ifdef ENABLE_HFP_AT_MESSAGES
    hfp_connection_t * hfp_connection = get_hfp_connection_context_for_rfcomm_cid(cid);
    hfp_emit_string_event(hfp_connection, HFP_SUBEVENT_AT_MESSAGE_SENT, command);
#endif
    return 1;
}

int hfp_supports_codec(uint8_t codec, int codecs_nr, uint8_t * codecs){

    // mSBC requires support for eSCO connections
    if ((codec == HFP_CODEC_MSBC) && !hci_extended_sco_link_supported()) return 0;

    int i;
    for (i = 0; i < codecs_nr; i++){
        if (codecs[i] != codec) continue;
        return 1;
    }
    return 0;
}

void hfp_hf_drop_mSBC_if_eSCO_not_supported(uint8_t * codecs, uint8_t * codecs_nr){
    if (hci_extended_sco_link_supported()) return;
    uint8_t tmp_codecs[HFP_MAX_NUM_CODECS];
    int i;
    int tmp_codec_nr = 0;
    for (i=0; i < *codecs_nr; i++){
        if (codecs[i] == HFP_CODEC_MSBC) continue;
        tmp_codecs[tmp_codec_nr++] = codecs[i];
    }
    *codecs_nr = tmp_codec_nr;
    (void)memcpy(codecs, tmp_codecs, tmp_codec_nr);
}

// UTILS
int get_bit(uint16_t bitmap, int position){
    return (bitmap >> position) & 1;
}

int store_bit(uint32_t bitmap, int position, uint8_t value){
    if (value){
        bitmap |= 1 << position;
    } else {
        bitmap &= ~ (1 << position);
    }
    return bitmap;
}

int join(char * buffer, int buffer_size, uint8_t * values, int values_nr){
    if (buffer_size < (values_nr * 3)) return 0;
    int i;
    int offset = 0;
    for (i = 0; i < (values_nr-1); i++) {
      offset += snprintf(buffer+offset, buffer_size-offset, "%d,", values[i]); // puts string into buffer
    }
    if (i<values_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", values[i]);
    }
    return offset;
}

int join_bitmap(char * buffer, int buffer_size, uint32_t values, int values_nr){
    if (buffer_size < (values_nr * 3)) return 0;

    int i;
    int offset = 0;
    for (i = 0; i < (values_nr-1); i++) {
      offset += snprintf(buffer+offset, buffer_size-offset, "%d,", get_bit(values,i)); // puts string into buffer
    }
    
    if (i<values_nr){
        offset += snprintf(buffer+offset, buffer_size-offset, "%d", get_bit(values,i));
    }
    return offset;
}

static void hfp_emit_event_for_context(hfp_connection_t * hfp_connection, uint8_t * packet, uint16_t size){
    if (!hfp_connection) return;
    btstack_packet_handler_t callback = NULL;
    switch (hfp_connection->local_role){
        case HFP_ROLE_HF:
            callback = hfp_hf_callback;
            break;
        case HFP_ROLE_AG:
            callback = hfp_ag_callback;
            break;
        default:
            return;
    }
    (*callback)(HCI_EVENT_PACKET, 0, packet, size);
}

void hfp_emit_simple_event(hfp_connection_t * hfp_connection, uint8_t event_subtype){
    hci_con_handle_t acl_handle = (hfp_connection != NULL) ? hfp_connection->acl_handle : HCI_CON_HANDLE_INVALID;
    uint8_t event[5];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    little_endian_store_16(event, 3, acl_handle);
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_event(hfp_connection_t * hfp_connection, uint8_t event_subtype, uint8_t value){
    hci_con_handle_t acl_handle = (hfp_connection != NULL) ? hfp_connection->acl_handle : HCI_CON_HANDLE_INVALID;
    uint8_t event[6];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    little_endian_store_16(event, 3, acl_handle);
    event[5] = value; // status 0 == OK
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_slc_connection_event(hfp_connection_t * hfp_connection, uint8_t status, hci_con_handle_t con_handle, bd_addr_t addr){
    btstack_assert(hfp_connection != NULL);
    uint8_t event[12];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
    event[pos++] = status; // status 0 == OK
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

static void hfp_emit_audio_connection_released(hfp_connection_t * hfp_connection, hci_con_handle_t sco_handle){
    btstack_assert(hfp_connection != NULL);
    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED;
    little_endian_store_16(event, pos, hfp_connection->acl_handle);
    pos += 2;
    little_endian_store_16(event, pos, sco_handle);
    pos += 2;
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_sco_event(hfp_connection_t * hfp_connection, uint8_t status, hci_con_handle_t sco_handle, bd_addr_t addr, uint8_t  negotiated_codec){
    btstack_assert(hfp_connection != NULL);
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED;
    little_endian_store_16(event, pos, hfp_connection->acl_handle);
    pos += 2;
    event[pos++] = status; // status 0 == OK
    little_endian_store_16(event, pos, sco_handle);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    event[pos++] = negotiated_codec;
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_string_event(hfp_connection_t * hfp_connection, uint8_t event_subtype, const char * value){
    btstack_assert(hfp_connection != NULL);
#ifdef ENABLE_HFP_AT_MESSAGES
    uint8_t event[256];
#else
    uint8_t event[40];
#endif
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    little_endian_store_16(event, 3, hfp_connection->acl_handle);
    uint16_t size = btstack_min(strlen(value), sizeof(event) - 6);
    strncpy((char*)&event[5], value, size);
    event[5 + size] = 0;
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

btstack_linked_list_t * hfp_get_connections(void){
    return (btstack_linked_list_t *) &hfp_connections;
} 

hfp_connection_t * get_hfp_connection_context_for_rfcomm_cid(uint16_t cid){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if (hfp_connection->rfcomm_cid == cid){
            return hfp_connection;
        }
    }
    return NULL;
}

hfp_connection_t * get_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr, hfp_role_t hfp_role){
    btstack_linked_list_iterator_t it;  
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if ((memcmp(hfp_connection->remote_addr, bd_addr, 6) == 0) && (hfp_connection->local_role == hfp_role)) {
            return hfp_connection;
        }
    }
    return NULL;
}

hfp_connection_t * get_hfp_connection_context_for_sco_handle(uint16_t handle, hfp_role_t hfp_role){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if ((hfp_connection->sco_handle == handle) && (hfp_connection->local_role == hfp_role)){
            return hfp_connection;
        }
    }
    return NULL;
}

hfp_connection_t * get_hfp_connection_context_for_acl_handle(uint16_t handle, hfp_role_t hfp_role){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, hfp_get_connections());
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * hfp_connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        if ((hfp_connection->acl_handle == handle) && (hfp_connection->local_role == hfp_role)){
            return hfp_connection;
        }
    }
    return NULL;
}

void hfp_reset_context_flags(hfp_connection_t * hfp_connection){
    if (!hfp_connection) return;
    hfp_connection->ok_pending = 0;
    hfp_connection->send_error = 0;

    hfp_connection->found_equal_sign = false;

    hfp_connection->change_status_update_for_individual_ag_indicators = 0; 
    hfp_connection->operator_name_changed = 0;      

    hfp_connection->enable_extended_audio_gateway_error_report = 0;
    hfp_connection->extended_audio_gateway_error = 0;

    // establish codecs hfp_connection
    hfp_connection->suggested_codec = 0;
    hfp_connection->negotiated_codec = 0;
    hfp_connection->codec_confirmed = 0;

    hfp_connection->establish_audio_connection = 0; 
    hfp_connection->call_waiting_notification_enabled = 0;
    hfp_connection->command = HFP_CMD_NONE;
    hfp_connection->enable_status_update_for_ag_indicators = 0xFF;
}

static hfp_connection_t * create_hfp_connection_context(void){
    hfp_connection_t * hfp_connection = btstack_memory_hfp_connection_get();
    if (!hfp_connection) return NULL;

    hfp_connection->state = HFP_IDLE;
    hfp_connection->call_state = HFP_CALL_IDLE;
    hfp_connection->codecs_state = HFP_CODECS_IDLE;

    hfp_connection->parser_state = HFP_PARSER_CMD_HEADER;
    hfp_connection->command = HFP_CMD_NONE;
    
    hfp_connection->acl_handle = HCI_CON_HANDLE_INVALID;
    hfp_connection->sco_handle = HCI_CON_HANDLE_INVALID;

    hfp_reset_context_flags(hfp_connection);

    btstack_linked_list_add(&hfp_connections, (btstack_linked_item_t*)hfp_connection);
    return hfp_connection;
}

void hfp_finalize_connection_context(hfp_connection_t * hfp_connection){
    btstack_run_loop_remove_timer(&hfp_connection->hfp_timeout);
    btstack_linked_list_remove(&hfp_connections, (btstack_linked_item_t*) hfp_connection);
    btstack_memory_hfp_connection_free(hfp_connection);
}

static hfp_connection_t * provide_hfp_connection_context_for_bd_addr(bd_addr_t bd_addr, hfp_role_t local_role){
    hfp_connection_t * hfp_connection = get_hfp_connection_context_for_bd_addr(bd_addr, local_role);
    if (hfp_connection) return  hfp_connection;
    hfp_connection = create_hfp_connection_context();
    (void)memcpy(hfp_connection->remote_addr, bd_addr, 6);
    hfp_connection->local_role = local_role;
    log_info("Create HFP context %p: role %u, addr %s", hfp_connection, local_role, bd_addr_to_str(bd_addr));

    return hfp_connection;
}

/* @param network.
 * 0 == no ability to reject a call. 
 * 1 == ability to reject a call.
 */

/* @param suported_features
 * HF bit 0: EC and/or NR function (yes/no, 1 = yes, 0 = no)
 * HF bit 1: Call waiting or three-way calling(yes/no, 1 = yes, 0 = no)
 * HF bit 2: CLI presentation capability (yes/no, 1 = yes, 0 = no)
 * HF bit 3: Voice recognition activation (yes/no, 1= yes, 0 = no)
 * HF bit 4: Remote volume control (yes/no, 1 = yes, 0 = no)
 * HF bit 5: Wide band speech (yes/no, 1 = yes, 0 = no)
 */
 /* Bit position:
 * AG bit 0: Three-way calling (yes/no, 1 = yes, 0 = no)
 * AG bit 1: EC and/or NR function (yes/no, 1 = yes, 0 = no)
 * AG bit 2: Voice recognition function (yes/no, 1 = yes, 0 = no)
 * AG bit 3: In-band ring tone capability (yes/no, 1 = yes, 0 = no)
 * AG bit 4: Attach a phone number to a voice tag (yes/no, 1 = yes, 0 = no)
 * AG bit 5: Wide band speech (yes/no, 1 = yes, 0 = no)
 */

void hfp_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint16_t service_uuid, int rfcomm_channel_nr, const char * name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        //  "UUID for Service"
        de_add_number(attribute, DE_UUID, DE_SIZE_16, service_uuid);
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_GENERIC_AUDIO);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);  // rfcomm_service
            de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel_nr);  // rfcomm channel
        }
        de_pop_sequence(attribute, rfcomm);
    }
    de_pop_sequence(service, attribute);

    
    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *sppProfile = de_push_sequence(attribute);
        {
            de_add_number(sppProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HANDSFREE); 
            de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0108); // Verision 1.8
        }
        de_pop_sequence(attribute, sppProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);    // ok: handling own sdp events
    UNUSED(channel);        // ok: no channel
    UNUSED(size);           // ok: handling own sdp events
    
    hfp_connection_t * hfp_connection = get_hfp_connection_context_for_bd_addr(sdp_query_context.remote_address, sdp_query_context.local_role);
    if (hfp_connection == NULL) {
        log_info("connection with %s and local role %d not found", sdp_query_context.remote_address, sdp_query_context.local_role);
        return;
    }
    
    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            hfp_connection->rfcomm_channel_nr = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (hfp_connection->rfcomm_channel_nr > 0){
                hfp_connection->state = HFP_W4_RFCOMM_CONNECTED;
                btstack_packet_handler_t packet_handler;
                switch (hfp_connection->local_role){
                    case HFP_ROLE_AG:
                        packet_handler = hfp_ag_rfcomm_packet_handler;
                        break;
                    case HFP_ROLE_HF:
                        packet_handler = hfp_hf_rfcomm_packet_handler;
                        break;
                    default:
                        btstack_assert(false);
                        return;
                }

                rfcomm_create_channel(packet_handler, hfp_connection->remote_addr, hfp_connection->rfcomm_channel_nr, NULL); 

            } else {
                hfp_connection->state = HFP_IDLE;
                uint8_t status = sdp_event_query_complete_get_status(packet);
                if (status == ERROR_CODE_SUCCESS){
                    // report service not found
                    status = SDP_SERVICE_NOT_FOUND;
                }
                hfp_emit_slc_connection_event(hfp_connection, status, HCI_CON_HANDLE_INVALID, hfp_connection->remote_addr);
                log_info("rfcomm service not found, status 0x%02x", status);
            }

            // register the SDP Query request to check if there is another connection waiting for the query
            // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
            (void) sdp_client_register_query_callback(&hfp_handle_sdp_client_query_request);
            break;
        default:
            break;
    }
}

// returns 0 if unexpected error or no other link options remained, otherwise 1
static int hfp_handle_failed_sco_connection(uint8_t status){
                   
    if (!sco_establishment_active){
        log_info("(e)SCO Connection failed but not started by us");
        return 0;
    }

    log_info("(e)SCO Connection failed 0x%02x", status);
    switch (status){
        case ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE:
        case ERROR_CODE_UNSPECIFIED_ERROR:
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES:
            break;
        default:
            return 0;
    }

    // note: eSCO_S4 supported flag not available, but it's only relevant for highest CVSD link setting (and the current failed)
    hfp_link_settings_t next_setting = hfp_next_link_setting_for_connection(sco_establishment_active->link_setting, sco_establishment_active, false);

    // handle no valid setting found
    if (next_setting == HFP_LINK_SETTINGS_NONE) {
        if (sco_establishment_active->negotiated_codec == HFP_CODEC_MSBC){
            log_info("T2/T1 failed, fallback to CVSD - D1");
            sco_establishment_active->negotiated_codec = HFP_CODEC_CVSD;
            sco_establishment_active->sco_for_msbc_failed = 1;
            sco_establishment_active->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_D1;
        } else {
            // no other options
            return 0;
        }
    }

    log_info("e)SCO Connection: try new link_setting %d", next_setting);
    sco_establishment_active->establish_audio_connection = 1;
    sco_establishment_active->link_setting = next_setting;
    sco_establishment_active = NULL;
    return 1;
}


void hfp_handle_hci_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, hfp_role_t local_role){
    UNUSED(packet_type);
    UNUSED(channel);    // ok: no channel
    UNUSED(size);    
    
    bd_addr_t event_addr;
    hci_con_handle_t handle;
    hfp_connection_t * hfp_connection = NULL;
    uint8_t status;

    log_debug("HFP HCI event handler type %u, event type %x, size %u", packet_type, hci_event_packet_get_type(packet), size);

    switch (hci_event_packet_get_type(packet)) {

        case HCI_EVENT_CONNECTION_REQUEST:
            switch(hci_event_connection_request_get_link_type(packet)){
                case 0: //  SCO
                case 2: // eSCO
                    hci_event_connection_request_get_bd_addr(packet, event_addr);
                    hfp_connection = get_hfp_connection_context_for_bd_addr(event_addr, local_role);
                    if (!hfp_connection) break;
                    if (hci_event_connection_request_get_link_type(packet) == 2){
                        hfp_connection->accept_sco = 2;
                    } else {
                        hfp_connection->accept_sco = 1;
                    }
#ifdef ENABLE_CC256X_ASSISTED_HFP
                    hfp_cc256x_prepare_for_sco(hfp_connection);
#endif
#ifdef ENABLE_BCM_PCM_WBS
                    hfp_bcm_prepare_for_sco(hfp_connection);
#endif
                    log_info("accept sco %u\n", hfp_connection->accept_sco);
                    sco_establishment_active = hfp_connection;
                    break;
                default:
                    break;                    
            }            
            break;
        
        case HCI_EVENT_COMMAND_STATUS:
            if (hci_event_command_status_get_command_opcode(packet) == hci_setup_synchronous_connection.opcode) {
                if (sco_establishment_active == NULL) break;
                status = hci_event_command_status_get_status(packet);
                if (status == ERROR_CODE_SUCCESS) break;
                
                hfp_connection = sco_establishment_active;
                if (hfp_handle_failed_sco_connection(status)) break;
                hfp_connection->establish_audio_connection = 0;
                hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                hfp_emit_sco_event(hfp_connection, status, 0, hfp_connection->remote_addr, hfp_connection->negotiated_codec);
            }
            break;

        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:{
            if (sco_establishment_active == NULL) break;
            hci_event_synchronous_connection_complete_get_bd_addr(packet, event_addr);
            hfp_connection = get_hfp_connection_context_for_bd_addr(event_addr, local_role);
            if (!hfp_connection) {
                log_error("HFP: connection does not exist for remote with addr %s.", bd_addr_to_str(event_addr));
                return;
            }
            
            status = hci_event_synchronous_connection_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                hfp_connection->accept_sco = 0;
                if (hfp_handle_failed_sco_connection(status)) break;

                hfp_connection->establish_audio_connection = 0;
                hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
                hfp_emit_sco_event(hfp_connection, status, 0, event_addr, hfp_connection->negotiated_codec);
                break;
            }
            
            uint16_t sco_handle = hci_event_synchronous_connection_complete_get_handle(packet);
            uint8_t  link_type = hci_event_synchronous_connection_complete_get_link_type(packet);
            uint8_t  transmission_interval = hci_event_synchronous_connection_complete_get_transmission_interval(packet);  // measured in slots
            uint8_t  retransmission_interval = hci_event_synchronous_connection_complete_get_retransmission_interval(packet);// measured in slots
            uint16_t rx_packet_length = hci_event_synchronous_connection_complete_get_rx_packet_length(packet); // measured in bytes
            uint16_t tx_packet_length = hci_event_synchronous_connection_complete_get_tx_packet_length(packet); // measured in bytes
            
            switch (link_type){
                case 0x00:
                    log_info("SCO Connection established.");
                    if (transmission_interval != 0) log_error("SCO Connection: transmission_interval not zero: %d.", transmission_interval);
                    if (retransmission_interval != 0) log_error("SCO Connection: retransmission_interval not zero: %d.", retransmission_interval);
                    if (rx_packet_length != 0) log_error("SCO Connection: rx_packet_length not zero: %d.", rx_packet_length);
                    if (tx_packet_length != 0) log_error("SCO Connection: tx_packet_length not zero: %d.", tx_packet_length);
                    break;
                case 0x02:
                    log_info("eSCO Connection established. \n");
                    break;
                default:
                    log_error("(e)SCO reserved link_type 0x%2x", link_type);
                    break;
            }
            
            log_info("sco_handle 0x%2x, address %s, transmission_interval %u slots, retransmission_interval %u slots, " 
                 " rx_packet_length %u bytes, tx_packet_length %u bytes, air_mode 0x%2x (0x02 == CVSD)\n", sco_handle,
                 bd_addr_to_str(event_addr), transmission_interval, retransmission_interval, rx_packet_length, tx_packet_length, 
                 hci_event_synchronous_connection_complete_get_air_mode(packet));

            if (hfp_connection->state == HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN){
                log_info("SCO about to disconnect: HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN");
                hfp_connection->state = HFP_W2_DISCONNECT_SCO;
                break;
            }
            hfp_connection->sco_handle = sco_handle;
            hfp_connection->establish_audio_connection = 0;
            hfp_connection->state = HFP_AUDIO_CONNECTION_ESTABLISHED;
            hfp_emit_sco_event(hfp_connection, status, sco_handle, event_addr, hfp_connection->negotiated_codec);
            break;                
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            handle = little_endian_read_16(packet,3);
            hfp_connection = get_hfp_connection_context_for_sco_handle(handle, local_role);
            
            if (!hfp_connection) break;

#ifdef ENABLE_CC256X_ASSISTED_HFP
            hfp_connection->cc256x_send_wbs_disassociate = true;
#endif
#ifdef ENABLE_BCM_PCM_WBS
            hfp_connection->bcm_send_disable_wbs = true;
#endif
            hfp_connection->sco_handle = HCI_CON_HANDLE_INVALID;
            hfp_connection->release_audio_connection = 0;

            if (hfp_connection->state == HFP_W4_SCO_DISCONNECTED_TO_SHUTDOWN){
                // RFCOMM already closed -> remote power off
#if defined(ENABLE_CC256X_ASSISTED_HFP) || defined (ENABLE_BCM_PCM_WBS)
                hfp_connection->state = HFP_W4_WBS_SHUTDOWN;
#else
                hfp_finalize_connection_context(hfp_connection);
#endif
                break;
            }

            hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
            hfp_emit_audio_connection_released(hfp_connection, handle);

            if (hfp_connection->release_slc_connection){
                hfp_connection->release_slc_connection = 0;
                log_info("SCO disconnected, w2 disconnect RFCOMM\n");
                hfp_connection->state = HFP_W2_DISCONNECT_RFCOMM;
            }   
            break;

        default:
            break;
    }
}


void hfp_handle_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size, hfp_role_t local_role){
    UNUSED(packet_type);
    UNUSED(channel);    // ok: no channel
    UNUSED(size);

    bd_addr_t event_addr;
    uint16_t rfcomm_cid;
    hfp_connection_t * hfp_connection = NULL;
    uint8_t status;

    log_debug("HFP packet_handler type %u, event type %x, size %u", packet_type, hci_event_packet_get_type(packet), size);

    switch (hci_event_packet_get_type(packet)) {

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr); 
            hfp_connection = provide_hfp_connection_context_for_bd_addr(event_addr, local_role);
            if (!hfp_connection){
                log_info("hfp: no memory to accept incoming connection - decline");
                rfcomm_decline_connection(rfcomm_event_incoming_connection_get_rfcomm_cid(packet));
                return;
            }
            if (hfp_connection->state != HFP_IDLE) {
                log_error("hfp: incoming connection but not idle, reject");
                rfcomm_decline_connection(rfcomm_event_incoming_connection_get_rfcomm_cid(packet));
                return;
            }

            hfp_connection->rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            hfp_connection->state = HFP_W4_RFCOMM_CONNECTED;
            rfcomm_accept_connection(hfp_connection->rfcomm_cid);
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)

            rfcomm_event_channel_opened_get_bd_addr(packet, event_addr); 
            status = rfcomm_event_channel_opened_get_status(packet);          
            
            hfp_connection = get_hfp_connection_context_for_bd_addr(event_addr, local_role);
            if (!hfp_connection || (hfp_connection->state != HFP_W4_RFCOMM_CONNECTED)) return;

            if (status) {
                hfp_emit_slc_connection_event(hfp_connection, status, rfcomm_event_channel_opened_get_con_handle(packet), event_addr);
                hfp_finalize_connection_context(hfp_connection);
            } else {
                hfp_connection->acl_handle = rfcomm_event_channel_opened_get_con_handle(packet);
                hfp_connection->rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                bd_addr_copy(hfp_connection->remote_addr, event_addr);

                switch (hfp_connection->state){
                    case HFP_W4_RFCOMM_CONNECTED:
                        hfp_connection->state = HFP_EXCHANGE_SUPPORTED_FEATURES;
                        break;
                    case HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN:
                        hfp_connection->state = HFP_W2_DISCONNECT_RFCOMM;
                        break;
                    default:
                        break;
                }
                rfcomm_request_can_send_now_event(hfp_connection->rfcomm_cid);
            }
            break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
            rfcomm_cid = little_endian_read_16(packet,2);
            hfp_connection = get_hfp_connection_context_for_rfcomm_cid(rfcomm_cid);
            if (!hfp_connection) break;
            if (hfp_connection->state == HFP_W4_RFCOMM_DISCONNECTED_AND_RESTART){
                hfp_connection->state = HFP_IDLE;
                hfp_establish_service_level_connection(hfp_connection->remote_addr, hfp_connection->service_uuid, local_role);
                break;
            }
            if ( hfp_connection->state == HFP_AUDIO_CONNECTION_ESTABLISHED){
                // service connection was released, this implicitly releases audio connection as well
                hfp_connection->release_audio_connection = 0;
                hci_con_handle_t sco_handle = hfp_connection->sco_handle;
                gap_disconnect(hfp_connection->sco_handle);
                hfp_connection->state = HFP_W4_SCO_DISCONNECTED_TO_SHUTDOWN;
                hfp_emit_audio_connection_released(hfp_connection, sco_handle);
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED, 0);
            } else {
                // regular case
                hfp_emit_event(hfp_connection, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED, 0);
                hfp_finalize_connection_context(hfp_connection);
            }
            break;

        default:
            break;
    }
}
// translates command string into hfp_command_t CMD

typedef struct {
    const char * command;
    hfp_command_t command_id;
} hfp_command_entry_t;

static hfp_command_entry_t hfp_ag_commmand_table[] = {
    { "AT+BAC=",   HFP_CMD_AVAILABLE_CODECS },
    { "AT+BCC",    HFP_CMD_TRIGGER_CODEC_CONNECTION_SETUP },
    { "AT+BCS=",   HFP_CMD_HF_CONFIRMED_CODEC },
    { "AT+BIA=",   HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE, }, // +BIA:<enabled>,,<enabled>,,,<enabled>
    { "AT+BIEV=",  HFP_CMD_HF_INDICATOR_STATUS },
    { "AT+BIND=",  HFP_CMD_LIST_GENERIC_STATUS_INDICATORS },
    { "AT+BIND=?", HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS },
    { "AT+BIND?",  HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE },
    { "AT+BINP=",  HFP_CMD_HF_REQUEST_PHONE_NUMBER },
    { "AT+BLDN",   HFP_CMD_REDIAL_LAST_NUMBER },
    { "AT+BRSF=",  HFP_CMD_SUPPORTED_FEATURES },
    { "AT+BTRH=",  HFP_CMD_RESPONSE_AND_HOLD_COMMAND },
    { "AT+BTRH?",  HFP_CMD_RESPONSE_AND_HOLD_QUERY },
    { "AT+BVRA=",  HFP_CMD_HF_ACTIVATE_VOICE_RECOGNITION },
    { "AT+CCWA=",  HFP_CMD_ENABLE_CALL_WAITING_NOTIFICATION},
    { "AT+CHLD=",  HFP_CMD_CALL_HOLD },
    { "AT+CHLD=?", HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES },
    { "AT+CHUP",   HFP_CMD_HANG_UP_CALL },
    { "AT+CIND=?", HFP_CMD_RETRIEVE_AG_INDICATORS },
    { "AT+CIND?",  HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS },
    { "AT+CLCC",   HFP_CMD_LIST_CURRENT_CALLS },
    { "AT+CLIP=",  HFP_CMD_ENABLE_CLIP},
    { "AT+CMEE=",  HFP_CMD_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR},
    { "AT+CMER=",  HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE },
    { "AT+CNUM",   HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION },
    { "AT+COPS=",  HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT },
    { "AT+COPS?",  HFP_CMD_QUERY_OPERATOR_SELECTION_NAME },
    { "AT+NREC=",  HFP_CMD_TURN_OFF_EC_AND_NR, },
    { "AT+VGM=",   HFP_CMD_SET_MICROPHONE_GAIN },
    { "AT+VGS=",   HFP_CMD_SET_SPEAKER_GAIN },
    { "AT+VTS=",   HFP_CMD_TRANSMIT_DTMF_CODES },
    { "ATA",       HFP_CMD_CALL_ANSWERED },
};

static hfp_command_entry_t hfp_hf_commmand_table[] = {
    { "+BCS:",  HFP_CMD_AG_SUGGESTED_CODEC },
    { "+BIND:", HFP_CMD_SET_GENERIC_STATUS_INDICATOR_STATUS },
    { "+BINP",  HFP_CMD_AG_SENT_PHONE_NUMBER },
    { "+BRSF:", HFP_CMD_SUPPORTED_FEATURES },
    { "+BSIR:", HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING },
    { "+BTRH:", HFP_CMD_RESPONSE_AND_HOLD_STATUS },
    { "+BVRA:", HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION },
    { "+CCWA:", HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE, },
    { "+CHLD:", HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES },
    { "+CIEV:", HFP_CMD_TRANSFER_AG_INDICATOR_STATUS},
    { "+CIND:", HFP_CMD_RETRIEVE_AG_INDICATORS_GENERIC },
    { "+CLCC:", HFP_CMD_LIST_CURRENT_CALLS },
    { "+CLIP:", HFP_CMD_AG_SENT_CLIP_INFORMATION },
    { "+CME ERROR:", HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR },
    { "+CNUM:", HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION},
    { "+COPS:", HFP_CMD_QUERY_OPERATOR_SELECTION_NAME },
    { "+VGM:",  HFP_CMD_SET_MICROPHONE_GAIN },
    { "+VGS:",  HFP_CMD_SET_SPEAKER_GAIN},
    { "ERROR",  HFP_CMD_ERROR},
    { "NOP",    HFP_CMD_NONE}, // dummy command used by unit tests
    { "OK",     HFP_CMD_OK },
    { "RING",   HFP_CMD_RING },
};

static hfp_command_t parse_command(const char * line_buffer, int isHandsFree){

    // table lookup based on role
    uint16_t num_entries;
    hfp_command_entry_t * table;
    if (isHandsFree == 0){
        table = hfp_ag_commmand_table;
        num_entries = sizeof(hfp_ag_commmand_table) / sizeof(hfp_command_entry_t);
    } else {
        table = hfp_hf_commmand_table;
        num_entries = sizeof(hfp_hf_commmand_table) / sizeof(hfp_command_entry_t);
    }
    // binary search
    uint16_t left = 0;
    uint16_t right = num_entries - 1;
    while (left <= right){
        uint16_t middle = left + (right - left) / 2;
        hfp_command_entry_t *entry = &table[middle];
        int match = strcmp(line_buffer, entry->command);
        if (match < 0){
            // search term is lower than middle element
            if (right == 0) break;
            right = middle - 1;
        } else if (match == 0){
            return entry->command_id;
        } else {
            // search term is higher than middle element
            left = middle + 1;
        }
    }

    // note: if parser in CMD_HEADER state would treats digits and maybe '+' as separator, match on "ATD" would work.
    // note: phone number is currently expected in line_buffer[3..]
    // prefix match on 'ATD', AG only
    if ((isHandsFree == 0) && (strncmp(line_buffer, HFP_CALL_PHONE_NUMBER, strlen(HFP_CALL_PHONE_NUMBER)) == 0)){
        return HFP_CMD_CALL_PHONE_NUMBER;
    }

    // Valid looking, but unknown commands/responses
    if ((isHandsFree == 0) && (strncmp(line_buffer, "AT+", 3) == 0)){
        return HFP_CMD_UNKNOWN;
    }

    if ((isHandsFree != 0) && (strncmp(line_buffer, "+", 1) == 0)){
        return HFP_CMD_UNKNOWN;
    }

    return HFP_CMD_NONE;
}

static void hfp_parser_store_byte(hfp_connection_t * hfp_connection, uint8_t byte){
    if ((hfp_connection->line_size + 1 ) >= HFP_MAX_INDICATOR_DESC_SIZE) return;
    hfp_connection->line_buffer[hfp_connection->line_size++] = byte;
    hfp_connection->line_buffer[hfp_connection->line_size] = 0;
}
static int hfp_parser_is_buffer_empty(hfp_connection_t * hfp_connection){
    return hfp_connection->line_size == 0;
}

static int hfp_parser_is_end_of_line(uint8_t byte){
    return (byte == '\n') || (byte == '\r');
}

static void hfp_parser_reset_line_buffer(hfp_connection_t *hfp_connection) {
    hfp_connection->line_size = 0;
}

static void hfp_parser_store_if_token(hfp_connection_t * hfp_connection, uint8_t byte){
    switch (byte){
        case ',':
		case '-':
        case ';':
        case '(':
        case ')':
        case '\n':
        case '\r':
            break;
        default:
            hfp_parser_store_byte(hfp_connection, byte);
            break;
    }
}

static bool hfp_parser_is_separator( uint8_t byte){
    switch (byte){
        case ',':
		case '-':
        case ';':
        case '\n':
        case '\r':
            return true;
        default:
            return false;
    }
}

static bool hfp_parse_byte(hfp_connection_t * hfp_connection, uint8_t byte, int isHandsFree){

    // handle doubles quotes
    if (byte == '"'){
        hfp_connection->parser_quoted = !hfp_connection->parser_quoted;
        return true;
    }
    if (hfp_connection->parser_quoted) {
        hfp_parser_store_byte(hfp_connection, byte);
        return true;
    }

    // ignore spaces outside command or double quotes (required e.g. for '+CME ERROR:..") command
    if ((byte == ' ') && (hfp_connection->parser_state != HFP_PARSER_CMD_HEADER)) return true;

    bool processed = true;

    switch (hfp_connection->parser_state) {
        case HFP_PARSER_CMD_HEADER:
            switch (byte) {
                case '\n':
                case '\r':
                case ';':
                    // ignore separator
                    break;
                case ':':
                case '?':
                    // store separator
                    hfp_parser_store_byte(hfp_connection, byte);
                    break;
                case '=':
                    // equal sign: remember and wait for next char to decided between '=?' and '=\?'
                    hfp_connection->found_equal_sign = true;
                    hfp_parser_store_byte(hfp_connection, byte);
                    return true;
                default:
                    // store if not lookahead
                    if (!hfp_connection->found_equal_sign) {
                        hfp_parser_store_byte(hfp_connection, byte);
                        return true;
                    }
                    // mark as lookahead
                    processed = false;
                    break;
            }

            // ignore empty tokens
            if (hfp_parser_is_buffer_empty(hfp_connection)) return true;

            // parse
            hfp_connection->command = parse_command((char *)hfp_connection->line_buffer, isHandsFree);

            // pick +CIND version based on connection state: descriptions during SLC vs. states later
            if (hfp_connection->command == HFP_CMD_RETRIEVE_AG_INDICATORS_GENERIC){
                switch(hfp_connection->state){
                    case HFP_W4_RETRIEVE_INDICATORS_STATUS:
                        hfp_connection->command = HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS;
                        break;
                    case HFP_W4_RETRIEVE_INDICATORS:
                        hfp_connection->command = HFP_CMD_RETRIEVE_AG_INDICATORS;
                        break;
                    default:
                        hfp_connection->command = HFP_CMD_UNKNOWN;
                        break;
                }
            }

            log_info("command string '%s', handsfree %u -> cmd id %u", (char *)hfp_connection->line_buffer, isHandsFree, hfp_connection->command);

            // next state
            hfp_connection->found_equal_sign = false;
            hfp_parser_reset_line_buffer(hfp_connection);
            hfp_connection->parser_state = HFP_PARSER_CMD_SEQUENCE;

            return processed;

        case HFP_PARSER_CMD_SEQUENCE:
            // handle empty fields
            if ((byte == ',' ) && (hfp_connection->line_size == 0)){
                hfp_connection->line_buffer[0] = 0;
                hfp_connection->ignore_value = 1;
                parse_sequence(hfp_connection);
                return true;
            }

            hfp_parser_store_if_token(hfp_connection, byte);
            if (!hfp_parser_is_separator(byte)) return true;

            // ignore empty tokens
            if (hfp_parser_is_buffer_empty(hfp_connection) && (hfp_connection->ignore_value == 0)) return true;

            parse_sequence(hfp_connection);

            hfp_parser_reset_line_buffer(hfp_connection);

            switch (hfp_connection->command){
                case HFP_CMD_AG_SENT_PHONE_NUMBER:
                case HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE:
                case HFP_CMD_AG_SENT_CLIP_INFORMATION:
                case HFP_CMD_TRANSFER_AG_INDICATOR_STATUS:
                case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME:
                case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT:
                case HFP_CMD_RETRIEVE_AG_INDICATORS:
                case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE:
                case HFP_CMD_HF_INDICATOR_STATUS:
                    hfp_connection->parser_state = HFP_PARSER_SECOND_ITEM;
                    break;
                default:
                    break;
            }
            return true;

        case HFP_PARSER_SECOND_ITEM:

            hfp_parser_store_if_token(hfp_connection, byte);
            if (!hfp_parser_is_separator(byte)) return true;

            switch (hfp_connection->command){
                case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME:
                    log_info("format %s, ", hfp_connection->line_buffer);
                    hfp_connection->network_operator.format =  btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    break;
                case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT:
                    log_info("format %s \n", hfp_connection->line_buffer);
                    hfp_connection->network_operator.format =  btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    break;
                case HFP_CMD_LIST_GENERIC_STATUS_INDICATORS:
                case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS:
                case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE:
                    hfp_connection->generic_status_indicators[hfp_connection->parser_item_index].state = (uint8_t)btstack_atoi((char*)hfp_connection->line_buffer);
                    break;
                case HFP_CMD_TRANSFER_AG_INDICATOR_STATUS:
                    hfp_connection->ag_indicators[hfp_connection->parser_item_index].status = (uint8_t)btstack_atoi((char*)hfp_connection->line_buffer);
                    log_info("%d \n", hfp_connection->ag_indicators[hfp_connection->parser_item_index].status);
                    hfp_connection->ag_indicators[hfp_connection->parser_item_index].status_changed = 1;
                    break;
                case HFP_CMD_RETRIEVE_AG_INDICATORS:
                    hfp_connection->ag_indicators[hfp_connection->parser_item_index].min_range = btstack_atoi((char *)hfp_connection->line_buffer);
                    log_info("%s, ", hfp_connection->line_buffer);
                    break;
                case HFP_CMD_AG_SENT_PHONE_NUMBER:
                case HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE:
                case HFP_CMD_AG_SENT_CLIP_INFORMATION:
                    hfp_connection->bnip_type = (uint8_t)btstack_atoi((char*)hfp_connection->line_buffer);
                    break;
                case HFP_CMD_HF_INDICATOR_STATUS:
                    hfp_connection->parser_indicator_value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    break;
                default:
                    break;
            }

            hfp_parser_reset_line_buffer(hfp_connection);

            hfp_connection->parser_state = HFP_PARSER_THIRD_ITEM;

            return true;

        case HFP_PARSER_THIRD_ITEM:

            hfp_parser_store_if_token(hfp_connection, byte);
            if (!hfp_parser_is_separator(byte)) return true;

            switch (hfp_connection->command){
                case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME:
                    strncpy(hfp_connection->network_operator.name, (char *)hfp_connection->line_buffer, HFP_MAX_NETWORK_OPERATOR_NAME_SIZE);
                    hfp_connection->network_operator.name[HFP_MAX_NETWORK_OPERATOR_NAME_SIZE - 1] = 0;
                    log_info("name %s\n", hfp_connection->line_buffer);
                    break;
                case HFP_CMD_RETRIEVE_AG_INDICATORS:
                    hfp_connection->ag_indicators[hfp_connection->parser_item_index].max_range = btstack_atoi((char *)hfp_connection->line_buffer);
                    hfp_next_indicators_index(hfp_connection);
                    hfp_connection->ag_indicators_nr = hfp_connection->parser_item_index;
                    log_info("%s)\n", hfp_connection->line_buffer);
                    break;
                default:
                    break;
            }

            hfp_parser_reset_line_buffer(hfp_connection);

            if (hfp_connection->command == HFP_CMD_RETRIEVE_AG_INDICATORS){
                hfp_connection->parser_state = HFP_PARSER_CMD_SEQUENCE;
            } else {
                hfp_connection->parser_state = HFP_PARSER_CMD_HEADER;
            }
            return true;

        default:
            btstack_assert(false);
            return true;
    }
}

void hfp_parse(hfp_connection_t * hfp_connection, uint8_t byte, int isHandsFree){
    bool processed = false;
    while (!processed){
        processed = hfp_parse_byte(hfp_connection, byte, isHandsFree);
    }
    // reset parser state on end-of-line
    if (hfp_parser_is_end_of_line(byte)){
        hfp_connection->parser_item_index = 0;
        hfp_connection->parser_state = HFP_PARSER_CMD_HEADER;
    }
}

static void parse_sequence(hfp_connection_t * hfp_connection){
    int value;
    switch (hfp_connection->command){
        case HFP_CMD_SET_GENERIC_STATUS_INDICATOR_STATUS:
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            int i;
            switch (hfp_connection->parser_item_index){
                case 0:
                    for (i=0;i<hfp_connection->generic_status_indicators_nr;i++){
                        if (hfp_connection->generic_status_indicators[i].uuid == value){
                            hfp_connection->parser_indicator_index = i;
                            break;
                        }
                    }
                    break;
                case 1:
                    if (hfp_connection->parser_indicator_index <0) break;
                    hfp_connection->generic_status_indicators[hfp_connection->parser_indicator_index].state = value;
                    log_info("HFP_CMD_SET_GENERIC_STATUS_INDICATOR_STATUS set indicator at index %u, to %u\n",
                     hfp_connection->parser_item_index, value);
                    break;
                default:
                    break;
            }
            hfp_next_indicators_index(hfp_connection);
            break;

        case HFP_CMD_GET_SUBSCRIBER_NUMBER_INFORMATION:
            switch(hfp_connection->parser_item_index){
                case 0:
                    // <alpha>: This optional field is not supported, and shall be left blank.
                    break;
                case 1:
                    // <number>: Quoted string containing the phone number in the format specified by <type>.
                    strncpy(hfp_connection->bnip_number, (char *)hfp_connection->line_buffer, sizeof(hfp_connection->bnip_number));
                    hfp_connection->bnip_number[sizeof(hfp_connection->bnip_number)-1] = 0;
                    break;
                case 2:
                    /*
                      <type> field specifies the format of the phone number provided, and can be one of the following values:
                      - values 128-143: The phone number format may be a national or international format, and may contain prefix and/or escape digits. No changes on the number presentation are required.
                     - values 144-159: The phone number format is an international number, including the country code prefix. If the plus sign ("+") is not included as part of the number and shall be added by the AG as needed.
                     - values 160-175: National number. No prefix nor escape digits included.
                     */
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->bnip_type = value;
                    break;
                case 3:
                    // <speed>: This optional field is not supported, and shall be left blank.
                    break;
                case 4:
                    // <service>: Indicates which service this phone number relates to. Shall be either 4 (voice) or 5 (fax).
                default:
                    break;
            }
            // index > 2 are ignored in switch above
            hfp_connection->parser_item_index++;
            break;            
        case HFP_CMD_LIST_CURRENT_CALLS:
            switch(hfp_connection->parser_item_index){
                case 0:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->clcc_idx = value;
                    break;
                case 1:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->clcc_dir = value;
                    break;
                case 2:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->clcc_status = value;
                    break;
                case 3:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->clcc_mode = value;
                    break;
                case 4:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->clcc_mpty = value;
                    break;
                case 5:
                    strncpy(hfp_connection->bnip_number, (char *)hfp_connection->line_buffer, sizeof(hfp_connection->bnip_number));
                    hfp_connection->bnip_number[sizeof(hfp_connection->bnip_number)-1] = 0;
                    break;
                case 6:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->bnip_type = value;
                    break;
                default:
                    break;
            }
            // index > 6 are ignored in switch above
            hfp_connection->parser_item_index++;
            break;
        case HFP_CMD_SET_MICROPHONE_GAIN:
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            hfp_connection->microphone_gain = value;
            log_info("hfp parse HFP_CMD_SET_MICROPHONE_GAIN %d\n", value);
            break;
        case HFP_CMD_SET_SPEAKER_GAIN:
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            hfp_connection->speaker_gain = value;
            log_info("hfp parse HFP_CMD_SET_SPEAKER_GAIN %d\n", value);
            break;
        case HFP_CMD_HF_ACTIVATE_VOICE_RECOGNITION:
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            hfp_connection->ag_activate_voice_recognition = value;
            log_info("hfp parse HFP_CMD_HF_ACTIVATE_VOICE_RECOGNITION %d\n", value);
            break;
        case HFP_CMD_TURN_OFF_EC_AND_NR:
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            hfp_connection->ag_echo_and_noise_reduction = value;
            log_info("hfp parse HFP_CMD_TURN_OFF_EC_AND_NR %d\n", value);
            break;
        case HFP_CMD_CHANGE_IN_BAND_RING_TONE_SETTING:
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            hfp_connection->remote_supported_features = store_bit(hfp_connection->remote_supported_features, HFP_AGSF_IN_BAND_RING_TONE, value);
            log_info("hfp parse HFP_CHANGE_IN_BAND_RING_TONE_SETTING %d\n", value);
            break;
        case HFP_CMD_HF_CONFIRMED_CODEC:
            hfp_connection->codec_confirmed = btstack_atoi((char*)hfp_connection->line_buffer);
            log_info("hfp parse HFP_CMD_HF_CONFIRMED_CODEC %d\n", hfp_connection->codec_confirmed);
            break;
        case HFP_CMD_AG_SUGGESTED_CODEC:
            hfp_connection->suggested_codec = btstack_atoi((char*)hfp_connection->line_buffer);
            log_info("hfp parse HFP_CMD_AG_SUGGESTED_CODEC %d\n", hfp_connection->suggested_codec);
            break;
        case HFP_CMD_SUPPORTED_FEATURES:
            hfp_connection->remote_supported_features = btstack_atoi((char*)hfp_connection->line_buffer);
            log_info("Parsed supported feature %d\n", (int) hfp_connection->remote_supported_features);
            break;
        case HFP_CMD_AVAILABLE_CODECS:
            log_info("Parsed codec %s\n", hfp_connection->line_buffer);
            hfp_connection->remote_codecs[hfp_connection->parser_item_index] = (uint16_t)btstack_atoi((char*)hfp_connection->line_buffer);
            hfp_next_codec_index(hfp_connection);
            hfp_connection->remote_codecs_nr = hfp_connection->parser_item_index;
            break;
        case HFP_CMD_RETRIEVE_AG_INDICATORS:
            strncpy((char *)hfp_connection->ag_indicators[hfp_connection->parser_item_index].name,  (char *)hfp_connection->line_buffer, HFP_MAX_INDICATOR_DESC_SIZE);
            hfp_connection->ag_indicators[hfp_connection->parser_item_index].name[HFP_MAX_INDICATOR_DESC_SIZE-1] = 0;
            hfp_connection->ag_indicators[hfp_connection->parser_item_index].index = hfp_connection->parser_item_index+1;
            log_info("Indicator %d: %s (", hfp_connection->ag_indicators_nr+1, hfp_connection->line_buffer);
            break;
        case HFP_CMD_RETRIEVE_AG_INDICATORS_STATUS:
            log_info("Parsed Indicator %d with status: %s\n", hfp_connection->parser_item_index+1, hfp_connection->line_buffer);
            hfp_connection->ag_indicators[hfp_connection->parser_item_index].status = btstack_atoi((char *) hfp_connection->line_buffer);
            hfp_next_indicators_index(hfp_connection);
            break;
        case HFP_CMD_ENABLE_INDICATOR_STATUS_UPDATE:
            hfp_next_indicators_index(hfp_connection);
            if (hfp_connection->parser_item_index != 4) break;
            log_info("Parsed Enable indicators: %s\n", hfp_connection->line_buffer);
            value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            hfp_connection->enable_status_update_for_ag_indicators = (uint8_t) value;
            break;
        case HFP_CMD_SUPPORT_CALL_HOLD_AND_MULTIPARTY_SERVICES:
            log_info("Parsed Support call hold: %s\n", hfp_connection->line_buffer);
            if (hfp_connection->line_size > 2 ) break;
            memcpy((char *)hfp_connection->remote_call_services[hfp_connection->remote_call_services_index].name, (char *)hfp_connection->line_buffer, HFP_CALL_SERVICE_SIZE-1);
            hfp_connection->remote_call_services[hfp_connection->remote_call_services_index].name[HFP_CALL_SERVICE_SIZE - 1] = 0;
            hfp_next_remote_call_services_index(hfp_connection);
            break;
        case HFP_CMD_LIST_GENERIC_STATUS_INDICATORS:
        case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS:
            log_info("Parsed Generic status indicator: %s\n", hfp_connection->line_buffer);
            hfp_connection->generic_status_indicators[hfp_connection->parser_item_index].uuid = (uint16_t)btstack_atoi((char*)hfp_connection->line_buffer);
            hfp_next_indicators_index(hfp_connection);
            hfp_connection->generic_status_indicators_nr = hfp_connection->parser_item_index;
            break;
        case HFP_CMD_RETRIEVE_GENERIC_STATUS_INDICATORS_STATE:
            // HF parses inital AG gen. ind. state
            log_info("Parsed List generic status indicator %s state: ", hfp_connection->line_buffer);
            hfp_connection->parser_item_index = hfp_parse_indicator_index(hfp_connection);
            break;
        case HFP_CMD_HF_INDICATOR_STATUS:
            hfp_connection->parser_indicator_index = hfp_parse_indicator_index(hfp_connection);
            log_info("Parsed HF indicator index %u", hfp_connection->parser_indicator_index);
            break;
        case HFP_CMD_ENABLE_INDIVIDUAL_AG_INDICATOR_STATUS_UPDATE:
            // AG parses new gen. ind. state
            if (hfp_connection->ignore_value){
                hfp_connection->ignore_value = 0;
                log_info("Parsed Enable AG indicator pos %u('%s') - unchanged (stays %u)\n", hfp_connection->parser_item_index,
                    hfp_connection->ag_indicators[hfp_connection->parser_item_index].name, hfp_connection->ag_indicators[hfp_connection->parser_item_index].enabled);
            }
            else if (hfp_connection->ag_indicators[hfp_connection->parser_item_index].mandatory){
                log_info("Parsed Enable AG indicator pos %u('%s') - ignore (mandatory)\n", 
                    hfp_connection->parser_item_index, hfp_connection->ag_indicators[hfp_connection->parser_item_index].name);
            } else {
                value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                hfp_connection->ag_indicators[hfp_connection->parser_item_index].enabled = value;
                log_info("Parsed Enable AG indicator pos %u('%s'): %u\n", hfp_connection->parser_item_index,
                    hfp_connection->ag_indicators[hfp_connection->parser_item_index].name, value);
            }
            hfp_next_indicators_index(hfp_connection);
            break;
        case HFP_CMD_TRANSFER_AG_INDICATOR_STATUS:
            // indicators are indexed starting with 1
            hfp_connection->parser_item_index = hfp_parse_indicator_index(hfp_connection);
            log_info("Parsed status of the AG indicator %d, status ", hfp_connection->parser_item_index);
            break;
        case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME:
            hfp_connection->network_operator.mode = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            log_info("Parsed network operator mode: %d, ", hfp_connection->network_operator.mode);
            break;
        case HFP_CMD_QUERY_OPERATOR_SELECTION_NAME_FORMAT:
            if (hfp_connection->line_buffer[0] == '3'){
                log_info("Parsed Set network operator format : %s, ", hfp_connection->line_buffer);
                break;
            }
            // TODO emit ERROR, wrong format
            log_info("ERROR Set network operator format: index %s not supported\n", hfp_connection->line_buffer);
            break;
        case HFP_CMD_ERROR:
            break;
        case HFP_CMD_EXTENDED_AUDIO_GATEWAY_ERROR:
            hfp_connection->extended_audio_gateway_error = 1;
            hfp_connection->extended_audio_gateway_error_value = (uint8_t)btstack_atoi((char*)hfp_connection->line_buffer);
            break;
        case HFP_CMD_ENABLE_EXTENDED_AUDIO_GATEWAY_ERROR:
            hfp_connection->enable_extended_audio_gateway_error_report = (uint8_t)btstack_atoi((char*)hfp_connection->line_buffer);
            hfp_connection->ok_pending = 1;
            hfp_connection->extended_audio_gateway_error = 0;
            break;
        case HFP_CMD_AG_SENT_PHONE_NUMBER:
        case HFP_CMD_AG_SENT_CALL_WAITING_NOTIFICATION_UPDATE:
        case HFP_CMD_AG_SENT_CLIP_INFORMATION:
            strncpy(hfp_connection->bnip_number, (char *)hfp_connection->line_buffer, sizeof(hfp_connection->bnip_number));
            hfp_connection->bnip_number[sizeof(hfp_connection->bnip_number)-1] = 0;
            break;
        case HFP_CMD_CALL_HOLD:
            hfp_connection->ag_call_hold_action = hfp_connection->line_buffer[0] - '0';
            if (hfp_connection->line_buffer[1] != '\0'){
                hfp_connection->call_index = btstack_atoi((char *)&hfp_connection->line_buffer[1]);
            }
            break;
        case HFP_CMD_RESPONSE_AND_HOLD_COMMAND:
            hfp_connection->ag_response_and_hold_action = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
            break;
        case HFP_CMD_TRANSMIT_DTMF_CODES:
            hfp_connection->ag_dtmf_code = hfp_connection->line_buffer[0];
            break;
        case HFP_CMD_ENABLE_CLIP:
            hfp_connection->clip_enabled = hfp_connection->line_buffer[0] != '0';
            break;
        case HFP_CMD_ENABLE_CALL_WAITING_NOTIFICATION:
            hfp_connection->call_waiting_notification_enabled = hfp_connection->line_buffer[0] != '0';
            break;
        default:
            break;
    }  
}

static void hfp_handle_start_sdp_client_query(void * context){
    UNUSED(context);
    
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &hfp_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hfp_connection_t * connection = (hfp_connection_t *)btstack_linked_list_iterator_next(&it);
        
        if (connection->state != HFP_W2_SEND_SDP_QUERY) continue;
        
        connection->state = HFP_W4_SDP_QUERY_COMPLETE;
        sdp_query_context.local_role = connection->local_role;
        (void)memcpy(sdp_query_context.remote_address, connection->remote_addr, 6);
        sdp_client_query_rfcomm_channel_and_name_for_service_class_uuid(&handle_query_rfcomm_event, connection->remote_addr, connection->service_uuid);
        return;
    }
}

void hfp_establish_service_level_connection(bd_addr_t bd_addr, uint16_t service_uuid, hfp_role_t local_role){
    hfp_connection_t * hfp_connection = provide_hfp_connection_context_for_bd_addr(bd_addr, local_role);
    log_info("hfp_connect %s, hfp_connection %p", bd_addr_to_str(bd_addr), hfp_connection);
    
    if (!hfp_connection) {
        log_error("hfp_establish_service_level_connection for addr %s failed", bd_addr_to_str(bd_addr));
        return;
    }
    switch (hfp_connection->state){
        case HFP_W2_DISCONNECT_RFCOMM:
            hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
            return;
        case HFP_W4_RFCOMM_DISCONNECTED:
            hfp_connection->state = HFP_W4_RFCOMM_DISCONNECTED_AND_RESTART;
            return;
        case HFP_IDLE:
            (void)memcpy(hfp_connection->remote_addr, bd_addr, 6);
            hfp_connection->state = HFP_W2_SEND_SDP_QUERY;
            hfp_connection->service_uuid = service_uuid;

            hfp_handle_sdp_client_query_request.callback = &hfp_handle_start_sdp_client_query;
            // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
            (void) sdp_client_register_query_callback(&hfp_handle_sdp_client_query_request);
            break;
        default:
            break;
    }
}

void hfp_release_service_level_connection(hfp_connection_t * hfp_connection){
    if (!hfp_connection) return;
    hfp_release_audio_connection(hfp_connection);

    if (hfp_connection->state < HFP_W4_RFCOMM_CONNECTED){
        hfp_connection->state = HFP_IDLE;
        return;
    }
    
    if (hfp_connection->state == HFP_W4_RFCOMM_CONNECTED){
        hfp_connection->state = HFP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
        return;
    }

    if (hfp_connection->state < HFP_W4_SCO_CONNECTED){
        hfp_connection->state = HFP_W2_DISCONNECT_RFCOMM;
        return;
    }

    if (hfp_connection->state < HFP_W4_SCO_DISCONNECTED){
        hfp_connection->state = HFP_W2_DISCONNECT_SCO;
        return;
    }

    // HFP_W4_SCO_DISCONNECTED or later 
    hfp_connection->release_slc_connection = 1;
}

void hfp_release_audio_connection(hfp_connection_t * hfp_connection){
    if (!hfp_connection) return;
    if (hfp_connection->state >= HFP_W2_DISCONNECT_SCO) return;
    hfp_connection->release_audio_connection = 1; 
}

static const struct link_settings {
    const uint16_t max_latency;
    const uint8_t  retransmission_effort;
    const uint16_t packet_types;
    const bool     eSCO;
    const uint8_t  codec;
} hfp_link_settings [] = {
    { 0xffff, 0xff, SCO_PACKET_TYPES_HV1,  false, HFP_CODEC_CVSD }, // HFP_LINK_SETTINGS_D0
    { 0xffff, 0xff, SCO_PACKET_TYPES_HV3,  false, HFP_CODEC_CVSD }, // HFP_LINK_SETTINGS_D1
    { 0x0007, 0x01, SCO_PACKET_TYPES_EV3,  true,  HFP_CODEC_CVSD }, // HFP_LINK_SETTINGS_S1
    { 0x0007, 0x01, SCO_PACKET_TYPES_2EV3, true,  HFP_CODEC_CVSD }, // HFP_LINK_SETTINGS_S2
    { 0x000a, 0x01, SCO_PACKET_TYPES_2EV3, true,  HFP_CODEC_CVSD }, // HFP_LINK_SETTINGS_S3
    { 0x000c, 0x02, SCO_PACKET_TYPES_2EV3, true,  HFP_CODEC_CVSD }, // HFP_LINK_SETTINGS_S4
    { 0x0008, 0x02, SCO_PACKET_TYPES_EV3,  true,  HFP_CODEC_MSBC }, // HFP_LINK_SETTINGS_T1
    { 0x000d, 0x02, SCO_PACKET_TYPES_2EV3, true,  HFP_CODEC_MSBC }  // HFP_LINK_SETTINGS_T2
};

void hfp_setup_synchronous_connection(hfp_connection_t * hfp_connection){
    // all packet types, fixed bandwidth
    int setting = hfp_connection->link_setting;
    log_info("hfp_setup_synchronous_connection using setting nr %u", setting);
    sco_establishment_active = hfp_connection;
    uint16_t sco_voice_setting = hci_get_sco_voice_setting();
    if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
#ifdef ENABLE_BCM_PCM_WBS
        sco_voice_setting = 0x0063; // Transparent data, 16-bit for BCM controllers
#else
        sco_voice_setting = 0x0043; // Transparent data, 8-bit otherwise
#endif
    }
    // get packet types - bits 6-9 are 'don't allow'
    uint16_t packet_types = hfp_link_settings[setting].packet_types ^ 0x03c0;
    hci_send_cmd(&hci_setup_synchronous_connection, hfp_connection->acl_handle, 8000, 8000, hfp_link_settings[setting].max_latency,
        sco_voice_setting, hfp_link_settings[setting].retransmission_effort, packet_types);
}

void hfp_accept_synchronous_connection(hfp_connection_t * hfp_connection, bool incoming_eSCO){

    // remote supported feature eSCO is set if link type is eSCO
    // eSCO: S4 - max latency == transmission interval = 0x000c == 12 ms,
    uint16_t max_latency;
    uint8_t  retransmission_effort;
    uint16_t packet_types;

    if (incoming_eSCO && hci_extended_sco_link_supported() && hci_remote_esco_supported(hfp_connection->acl_handle)){
        max_latency = 0x000c;
        retransmission_effort = 0x02;
        // eSCO: EV3 and 2-EV3
        packet_types = 0x0048;
    } else {
        max_latency = 0xffff;
        retransmission_effort = 0xff;
        // sco: HV1 and HV3
        packet_types = 0x005;
    }

    // mSBC only allows for transparent data
    uint16_t sco_voice_setting = hci_get_sco_voice_setting();
    if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
#ifdef ENABLE_BCM_PCM_WBS
        sco_voice_setting = 0x0063; // Transparent data, 16-bit for BCM controllers
#else
        sco_voice_setting = 0x0043; // Transparent data, 8-bit otherwise
#endif
    }

    // filter packet types
    packet_types &= hfp_get_sco_packet_types();

    // bits 6-9 are 'don't allow'
    packet_types ^= 0x3c0;

    log_info("HFP: sending hci_accept_connection_request, packet types 0x%04x, sco_voice_setting 0x%02x", packet_types, sco_voice_setting);
    hci_send_cmd(&hci_accept_synchronous_connection, hfp_connection->remote_addr, 8000, 8000, max_latency,
                 sco_voice_setting, retransmission_effort, packet_types);
}

#ifdef ENABLE_CC256X_ASSISTED_HFP
void hfp_cc256x_prepare_for_sco(hfp_connection_t * hfp_connection){
    hfp_connection->cc256x_send_write_codec_config = true;
    if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
        hfp_connection->cc256x_send_wbs_associate = true;
    }
}

void hfp_cc256x_write_codec_config(hfp_connection_t * hfp_connection){
    uint32_t sample_rate_hz;
    uint16_t clock_rate_khz;
    if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
        clock_rate_khz = 512;
        sample_rate_hz = 16000;
    } else {
        clock_rate_khz = 256;
        sample_rate_hz = 8000;
    }
    uint8_t clock_direction = 0;        // master
    uint16_t frame_sync_duty_cycle = 0; // i2s with 50%
    uint8_t  frame_sync_edge = 1;       // rising edge
    uint8_t  frame_sync_polarity = 0;   // active high
    uint8_t  reserved = 0;
    uint16_t size = 16;
    uint16_t chan_1_offset = 1;
    uint16_t chan_2_offset = chan_1_offset + size;
    uint8_t  out_edge = 1;              // rising
    uint8_t  in_edge = 0;               // falling
    hci_send_cmd(&hci_ti_write_codec_config, clock_rate_khz, clock_direction, sample_rate_hz, frame_sync_duty_cycle,
                 frame_sync_edge, frame_sync_polarity, reserved,
                 size, chan_1_offset, out_edge, size, chan_1_offset, in_edge, reserved,
                 size, chan_2_offset, out_edge, size, chan_2_offset, in_edge, reserved);
}
#endif

#ifdef ENABLE_BCM_PCM_WBS
void hfp_bcm_prepare_for_sco(hfp_connection_t * hfp_connection){
    hfp_connection->bcm_send_write_i2spcm_interface_param = true;
    if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
        hfp_connection->bcm_send_enable_wbs = true;
    }
}
void hfp_bcm_write_i2spcm_interface_param(hfp_connection_t * hfp_connection){
    uint8_t sample_rate = (hfp_connection->negotiated_codec == HFP_CODEC_MSBC) ? 1 : 0;
    // i2s enable, master, 8/16 kHz, 512 kHz
    hci_send_cmd(&hci_bcm_write_i2spcm_interface_paramhci_bcm_write_i2spcm_interface_param, 1, 1, sample_rate, 2);
}
#endif

void hfp_set_hf_callback(btstack_packet_handler_t callback){
    hfp_hf_callback = callback;
}

void hfp_set_ag_callback(btstack_packet_handler_t callback){
    hfp_ag_callback = callback;
}

void hfp_set_ag_rfcomm_packet_handler(btstack_packet_handler_t handler){
    hfp_ag_rfcomm_packet_handler = handler;
}

void hfp_set_hf_rfcomm_packet_handler(btstack_packet_handler_t handler){
    hfp_hf_rfcomm_packet_handler = handler;
}

void hfp_init(void){
    hfp_allowed_sco_packet_types = SCO_PACKET_TYPES_ALL;
}

void hfp_deinit(void){
    hfp_connections = NULL;
    hfp_hf_callback = NULL;
    hfp_ag_callback = NULL;
    hfp_hf_rfcomm_packet_handler = NULL;
    hfp_ag_rfcomm_packet_handler = NULL;
    sco_establishment_active = NULL;
    (void) memset(&sdp_query_context, 0, sizeof(hfp_sdp_query_context_t));
    (void) memset(&hfp_handle_sdp_client_query_request, 0, sizeof(btstack_context_callback_registration_t));
}

void hfp_set_sco_packet_types(uint16_t packet_types){
    hfp_allowed_sco_packet_types = packet_types;
}

uint16_t hfp_get_sco_packet_types(void){
    return hfp_allowed_sco_packet_types;
}

hfp_link_settings_t hfp_next_link_setting(hfp_link_settings_t current_setting, bool local_eSCO_supported, bool remote_eSCO_supported, bool eSCO_S4_supported, uint8_t negotiated_codec){
    int8_t setting = (int8_t) current_setting;
    bool can_use_eSCO = local_eSCO_supported && remote_eSCO_supported;
    while (setting > 0){
        setting--;
        // skip if eSCO required but not available
        if (hfp_link_settings[setting].eSCO && !can_use_eSCO) continue;
        // skip if S4 but not supported
        if ((setting == (int8_t) HFP_LINK_SETTINGS_S4) && !eSCO_S4_supported) continue;
        // skip wrong codec
        if ( hfp_link_settings[setting].codec != negotiated_codec) continue;
        // skip disabled packet types
        uint16_t required_packet_types = hfp_link_settings[setting].packet_types;
        uint16_t allowed_packet_types  = hfp_allowed_sco_packet_types;
        if ((required_packet_types & allowed_packet_types) == 0) continue;

        // found matching setting
        return (hfp_link_settings_t) setting;
    }
    return HFP_LINK_SETTINGS_NONE;
}

static hfp_link_settings_t hfp_next_link_setting_for_connection(hfp_link_settings_t current_setting, hfp_connection_t * hfp_connection, uint8_t eSCO_S4_supported){
    bool local_eSCO_supported  = hci_extended_sco_link_supported();
    bool remote_eSCO_supported = hci_remote_esco_supported(hfp_connection->acl_handle);
    uint8_t negotiated_codec   = hfp_connection->negotiated_codec;
    return  hfp_next_link_setting(current_setting, local_eSCO_supported, remote_eSCO_supported, eSCO_S4_supported, negotiated_codec);
}

void hfp_init_link_settings(hfp_connection_t * hfp_connection, uint8_t eSCO_S4_supported){
    // get highest possible link setting
    hfp_connection->link_setting = hfp_next_link_setting_for_connection(HFP_LINK_SETTINGS_NONE, hfp_connection, eSCO_S4_supported);
    log_info("hfp_init_link_settings: %u", hfp_connection->link_setting);
}

#define HFP_HF_RX_DEBUG_PRINT_LINE 80

void hfp_log_rfcomm_message(const char * tag, uint8_t * packet, uint16_t size){
#ifdef ENABLE_LOG_INFO
    // encode \n\r
    char printable[HFP_HF_RX_DEBUG_PRINT_LINE+2];
    int i = 0;
    int pos;
    for (pos=0 ; (pos < size) && (i < (HFP_HF_RX_DEBUG_PRINT_LINE - 3)) ; pos++){
        switch (packet[pos]){
            case '\n':
                printable[i++] = '\\';
                printable[i++] = 'n';
                break;
            case '\r':
                printable[i++] = '\\';
                printable[i++] = 'r';
                break;
            default:
                printable[i++] = packet[pos];
                break;
        }
    }
    printable[i] = 0;
    log_info("%s: '%s'", tag, printable);
#endif
}
