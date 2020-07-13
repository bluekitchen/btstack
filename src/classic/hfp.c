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
#include "classic/core.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"

#define HFP_HF_FEATURES_SIZE 10
#define HFP_AG_FEATURES_SIZE 12


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

static void parse_sequence(hfp_connection_t * context);

static btstack_linked_list_t hfp_connections = NULL;

static btstack_packet_handler_t hfp_hf_callback;
static btstack_packet_handler_t hfp_ag_callback;

static btstack_packet_handler_t hfp_hf_rfcomm_packet_handler;
static btstack_packet_handler_t hfp_ag_rfcomm_packet_handler;

static void (*hfp_hf_run_for_context)(hfp_connection_t * hfp_connection);

static hfp_connection_t * sco_establishment_active;

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
    uint8_t event[3];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_event(hfp_connection_t * hfp_connection, uint8_t event_subtype, uint8_t value){
    uint8_t event[4];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    event[3] = value; // status 0 == OK
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_slc_connection_event(hfp_connection_t * hfp_connection, uint8_t status, hci_con_handle_t con_handle, bd_addr_t addr){
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

static void hfp_emit_sco_event(hfp_connection_t * hfp_connection, uint8_t status, hci_con_handle_t con_handle, bd_addr_t addr, uint8_t  negotiated_codec){
    uint8_t event[13];
    int pos = 0;
    event[pos++] = HCI_EVENT_HFP_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED;
    event[pos++] = status; // status 0 == OK
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    reverse_bd_addr(addr,&event[pos]);
    pos += 6;
    event[pos++] = negotiated_codec;
    hfp_emit_event_for_context(hfp_connection, event, sizeof(event));
}

void hfp_emit_string_event(hfp_connection_t * hfp_connection, uint8_t event_subtype, const char * value){
    uint8_t event[40];
    event[0] = HCI_EVENT_HFP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    uint16_t size = btstack_min(strlen(value), sizeof(event) - 4);
    strncpy((char*)&event[3], value, size);
    event[3 + size] = 0;
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

static void remove_hfp_connection_context(hfp_connection_t * hfp_connection){
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
            de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0107); // Verision 1.7
        }
        de_pop_sequence(attribute, sppProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
}

static hfp_connection_t * connection_doing_sdp_query = NULL;

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);    // ok: handling own sdp events
    UNUSED(channel);        // ok: no channel
    UNUSED(size);           // ok: handling own sdp events
    hfp_connection_t * hfp_connection = connection_doing_sdp_query;
    if (!hfp_connection) {
        log_error("handle_query_rfcomm_event, no connection");
        return;
    }

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            hfp_connection->rfcomm_channel_nr = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            connection_doing_sdp_query = NULL;
            if (hfp_connection->rfcomm_channel_nr > 0){
                hfp_connection->state = HFP_W4_RFCOMM_CONNECTED;
                log_info("HFP: SDP_EVENT_QUERY_COMPLETE context %p, addr %s, state %d", hfp_connection, bd_addr_to_str( hfp_connection->remote_addr),  hfp_connection->state);
                btstack_packet_handler_t packet_handler;
                switch (hfp_connection->local_role){
                    case HFP_ROLE_AG:
                        packet_handler = hfp_ag_rfcomm_packet_handler;
                        break;
                    case HFP_ROLE_HF:
                        packet_handler = hfp_hf_rfcomm_packet_handler;
                        break;
                    default:
                        log_error("Role %x", hfp_connection->local_role);
                        return;
                }
                rfcomm_create_channel(packet_handler, hfp_connection->remote_addr, hfp_connection->rfcomm_channel_nr, NULL); 
                break;
            }
            hfp_connection->state = HFP_IDLE;
            hfp_emit_slc_connection_event(hfp_connection, sdp_event_query_complete_get_status(packet), HCI_CON_HANDLE_INVALID, hfp_connection->remote_addr);
            log_info("rfcomm service not found, status %u.", sdp_event_query_complete_get_status(packet));
            break;
        default:
            break;
    }
}

// returns 0 if unexpected error or no other link options remained, otherwise 1
static int hfp_handle_failed_sco_connection(uint8_t status){
                   
    if (!sco_establishment_active){
        log_error("(e)SCO Connection failed but not started by us");
        return 0;
    }
    log_info("(e)SCO Connection failed status 0x%02x", status);
    // invalid params / unspecified error
    if ((status != 0x11) && (status != 0x1f) && (status != 0x0D)) return 0;
                
    switch (sco_establishment_active->link_setting){
        case HFP_LINK_SETTINGS_D0:
            return 0; // no other option left
        case HFP_LINK_SETTINGS_D1:
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_D0;
            break;
        case HFP_LINK_SETTINGS_S1:
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_D1;
            break;                    
        case HFP_LINK_SETTINGS_S2:
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_S1;
            break;
        case HFP_LINK_SETTINGS_S3:
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_S2;
            break;
        case HFP_LINK_SETTINGS_S4:
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_S3;
            break;
        case HFP_LINK_SETTINGS_T1:
            log_info("T1 failed, fallback to CVSD - D1");
            sco_establishment_active->negotiated_codec = HFP_CODEC_CVSD;
            sco_establishment_active->sco_for_msbc_failed = 1;
            sco_establishment_active->command = HFP_CMD_AG_SEND_COMMON_CODEC;
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_D1;
            break;
        case HFP_LINK_SETTINGS_T2:
            sco_establishment_active->link_setting = HFP_LINK_SETTINGS_T1;
            break;
    }
    log_info("e)SCO Connection: try new link_setting %d", sco_establishment_active->link_setting);
    sco_establishment_active->establish_audio_connection = 1;
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
                        hfp_connection->hf_accept_sco = 2;
                    } else {
                        hfp_connection->hf_accept_sco = 1;
                    }
                    log_info("hf accept sco %u\n", hfp_connection->hf_accept_sco);
                    if (!hfp_hf_run_for_context) break;
                    (*hfp_hf_run_for_context)(hfp_connection);
                    break;
                default:
                    break;                    
            }            
            break;
        
        case HCI_EVENT_COMMAND_STATUS:
            if (hci_event_command_status_get_command_opcode(packet) == hci_setup_synchronous_connection.opcode) {
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
            hci_event_synchronous_connection_complete_get_bd_addr(packet, event_addr);
            hfp_connection = get_hfp_connection_context_for_bd_addr(event_addr, local_role);
            if (!hfp_connection) {
                log_error("HFP: connection does not exist for remote with addr %s.", bd_addr_to_str(event_addr));
                return;
            }
            
            status = hci_event_synchronous_connection_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                hfp_connection->hf_accept_sco = 0;
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
            uint8_t  air_mode = hci_event_synchronous_connection_complete_get_air_mode(packet);

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
                 bd_addr_to_str(event_addr), transmission_interval, retransmission_interval, rx_packet_length, tx_packet_length, air_mode);

            // hfp_connection = get_hfp_connection_context_for_bd_addr(event_addr, local_role);
            
            // if (!hfp_connection) {
            //     log_error("SCO link created, hfp_connection for address %s not found.", bd_addr_to_str(event_addr));
            //     break;
            // }

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

            hfp_connection->sco_handle = HCI_CON_HANDLE_INVALID;
            hfp_connection->release_audio_connection = 0;
            hfp_connection->state = HFP_SERVICE_LEVEL_CONNECTION_ESTABLISHED;
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED, 0);

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
                remove_hfp_connection_context(hfp_connection);
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
            
            hfp_emit_event(hfp_connection, HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED, 0);
            remove_hfp_connection_context(hfp_connection);
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
    { "AT+BINP",   HFP_CMD_HF_REQUEST_PHONE_NUMBER },
    { "AT+BLDN",   HFP_CMD_REDIAL_LAST_NUMBER },
    { "AT+BRSF=",  HFP_CMD_SUPPORTED_FEATURES },
    { "AT+BTRH=",  HFP_CMD_RESPONSE_AND_HOLD_COMMAND },
    { "AT+BTRH?",  HFP_CMD_RESPONSE_AND_HOLD_QUERY },
    { "AT+BVRA=",  HFP_CMD_AG_ACTIVATE_VOICE_RECOGNITION },
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
    { "AT+VTS:",   HFP_CMD_TRANSMIT_DTMF_CODES },
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
                    strncpy(hfp_connection->bnip_number, (char *)hfp_connection->line_buffer, sizeof(hfp_connection->bnip_number));
                    hfp_connection->bnip_number[sizeof(hfp_connection->bnip_number)-1] = 0;
                    break;
                case 1:
                    value = btstack_atoi((char *)&hfp_connection->line_buffer[0]);
                    hfp_connection->bnip_type = value;
                    break;
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
            strncpy((char *)hfp_connection->remote_call_services[hfp_connection->remote_call_services_index].name, (char *)hfp_connection->line_buffer, HFP_CALL_SERVICE_SIZE);
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
            hfp_connection->state = HFP_W4_SDP_QUERY_COMPLETE;
            connection_doing_sdp_query = hfp_connection;
            hfp_connection->service_uuid = service_uuid;
            sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, hfp_connection->remote_addr, service_uuid);
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
} hfp_link_settings [] = {
    { 0xffff, 0xff, 0x03c1 }, // HFP_LINK_SETTINGS_D0,   HV1
    { 0xffff, 0xff, 0x03c4 }, // HFP_LINK_SETTINGS_D1,   HV3
    { 0x0007, 0x01, 0x03c8 }, // HFP_LINK_SETTINGS_S1,   EV3
    { 0x0007, 0x01, 0x0380 }, // HFP_LINK_SETTINGS_S2, 2-EV3
    { 0x000a, 0x01, 0x0380 }, // HFP_LINK_SETTINGS_S3, 2-EV3
    { 0x000c, 0x02, 0x0380 }, // HFP_LINK_SETTINGS_S4, 2-EV3
    { 0x0008, 0x02, 0x03c8 }, // HFP_LINK_SETTINGS_T1,   EV3
    { 0x000d, 0x02, 0x0380 }  // HFP_LINK_SETTINGS_T2, 2-EV3
};

void hfp_setup_synchronous_connection(hfp_connection_t * hfp_connection){
    // all packet types, fixed bandwidth
    int setting = hfp_connection->link_setting;
    log_info("hfp_setup_synchronous_connection using setting nr %u", setting);
    sco_establishment_active = hfp_connection;
    uint16_t sco_voice_setting = hci_get_sco_voice_setting();
    if (hfp_connection->negotiated_codec == HFP_CODEC_MSBC){
        sco_voice_setting = 0x0043; // Transparent data
    }
    hci_send_cmd(&hci_setup_synchronous_connection, hfp_connection->acl_handle, 8000, 8000, hfp_link_settings[setting].max_latency,
        sco_voice_setting, hfp_link_settings[setting].retransmission_effort, hfp_link_settings[setting].packet_types); // all types 0x003f, only 2-ev3 0x380
}

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

void hfp_set_hf_run_for_context(void (*callback)(hfp_connection_t * hfp_connection)){
    hfp_hf_run_for_context = callback;
}

void hfp_init(void){
}

void hfp_init_link_settings(hfp_connection_t * hfp_connection, uint8_t esco_s4_supported){
    // determine highest possible link setting
    hfp_connection->link_setting = HFP_LINK_SETTINGS_D1;
    // anything else requires eSCO support on both sides
    if (hci_extended_sco_link_supported() && hci_remote_esco_supported(hfp_connection->acl_handle)){
        switch (hfp_connection->negotiated_codec){
            case HFP_CODEC_CVSD:
                hfp_connection->link_setting = HFP_LINK_SETTINGS_S3;
                if (esco_s4_supported){
                    hfp_connection->link_setting = HFP_LINK_SETTINGS_S4;
                }
                break;
            case HFP_CODEC_MSBC:
                hfp_connection->link_setting = HFP_LINK_SETTINGS_T2;
                break;
            default:
                break;
        }
    }
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
