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

#define BTSTACK_FILE__ "pbap_client.c"
 
#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hci_cmd.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "bluetooth_sdp.h"
#include "classic/sdp_client_rfcomm.h"
#include "btstack_event.h"
#include "md5.h"
#include "yxml.h"

#include "classic/obex.h"
#include "classic/obex_iterator.h"
#include "classic/goep_client.h"
#include "classic/pbap_client.h"

// 796135f0-f0c5-11d8-0966- 0800200c9a66
static const uint8_t pbap_uuid[] = { 0x79, 0x61, 0x35, 0xf0, 0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};

const char * pbap_phonebook_type     = "x-bt/phonebook";
const char * pbap_vcard_listing_type = "x-bt/vcard-listing";
const char * pbap_vcard_entry_type   = "x-bt/vcard";

const char * pbap_vcard_listing_name = "pb";

static uint32_t pbap_supported_features = \
    PBAP_SUPPORTED_FEATURES_DOWNLOAD |
    PBAP_SUPPORTED_FEATURES_BROWSING |
    PBAP_SUPPORTED_FEATURES_DATABASE_IDENTIFIER |
    PBAP_SUPPORTED_FEATURES_FOLDER_VERSION_COUNTERS |
    PBAP_SUPPORTED_FEATURES_VCARD_SELECTING |
    PBAP_SUPPORTED_FEATURES_ENHANCED_MISSED_CALLS | 
    PBAP_SUPPORTED_FEATURES_DEFAULT_CONTACT_IMAGE_FORMAT |
    PBAP_SUPPORTED_FEATURES_X_BT_UCI_VCARD_PROPERTY |
    PBAP_SUPPORTED_FEATURES_X_BT_UID_VCARD_PROPERTY |
    PBAP_SUPPORTED_FEATURES_CONTACT_REFERENCING;

typedef enum {
    PBAP_INIT = 0,
    PBAP_W4_GOEP_CONNECTION,
    PBAP_W2_SEND_CONNECT_REQUEST,
    PBAP_W4_CONNECT_RESPONSE,
    PBAP_W4_USER_AUTHENTICATION,
    PBAP_W2_SEND_AUTHENTICATED_CONNECT,
    PBAP_CONNECT_RESPONSE_RECEIVED,
    PBAP_CONNECTED,
    //
    PBAP_W2_SEND_DISCONNECT_REQUEST,
    PBAP_W4_DISCONNECT_RESPONSE,
    //
    PBAP_W2_PULL_PHONEBOOK,
    PBAP_W4_PHONEBOOK,
    PBAP_W2_SET_PATH_ROOT,
    PBAP_W4_SET_PATH_ROOT_COMPLETE,
    PBAP_W2_SET_PATH_ELEMENT,
    PBAP_W4_SET_PATH_ELEMENT_COMPLETE,
    PBAP_W2_GET_PHONEBOOK_SIZE,
    PBAP_W4_GET_PHONEBOOK_SIZE_COMPLETE,
    // - pull vacard liast
    PBAP_W2_GET_CARD_LIST,
    PBAP_W4_GET_CARD_LIST_COMPLETE,
    // - pull vcard entry
    PBAP_W2_GET_CARD_ENTRY,
    PBAP_W4_GET_CARD_ENTRY_COMPLETE

} pbap_state_t;

typedef enum {
    SRM_DISABLED,
    SRM_W4_CONFIRM,
    SRM_ENABLED_BUT_WAITING,
    SRM_ENABLED
} srm_state_t;

typedef struct pbap_client {
    pbap_state_t state;
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    uint16_t  goep_cid;
    btstack_packet_handler_t client_handler;
    int request_number;
    srm_state_t srm_state;
    const char * current_folder;
    const char * phone_number;
    const char * phonebook_path;
    const char * vcard_name;
    uint16_t set_path_offset;
    /* vcard selector / operator */
    uint32_t vcard_selector;
    uint8_t  vcard_selector_operator;
    uint8_t  vcard_selector_supported;
    /* abort */
    uint8_t  abort_operation;
    /* authentication */
    uint8_t  authentication_options;
    uint16_t authentication_nonce[16];
    const char * authentication_password;
    /* xml parser */
    yxml_t  xml_parser;
    uint8_t xml_buffer[50];
    /* flow control mode */
    uint8_t flow_control_enabled;
    uint8_t flow_next_triggered;
} pbap_client_t;

static pbap_client_t _pbap_client;
static pbap_client_t * pbap_client = &_pbap_client;

static void pbap_client_emit_connected_event(pbap_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    memcpy(&event[pos], context->bd_addr, 6);
    pos += 6;
    little_endian_store_16(event,pos,context->con_handle);
    pos += 2;
    event[pos++] = context->incoming;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("goep_client_emit_connected_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static void pbap_client_emit_connection_closed_event(pbap_client_t * context){
    uint8_t event[5];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_CONNECTION_CLOSED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("pbap_client_emit_connection_closed_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}   

static void pbap_client_emit_operation_complete_event(pbap_client_t * context, uint8_t status){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_OPERATION_COMPLETED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++]= status;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("pbap_client_emit_can_send_now_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void pbap_client_emit_phonebook_size_event(pbap_client_t * context, uint8_t status, uint16_t phonebook_size){
    uint8_t event[8];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_PHONEBOOK_SIZE;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    little_endian_store_16(event,pos, phonebook_size);
    pos+=2;
    event[1] = pos - 2;
    if (pos != sizeof(event)) log_error("pbap_client_emit_phonebook_size_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void pbap_client_emit_authentication_event(pbap_client_t * context, uint8_t options){
    // split options
    uint8_t user_id_required = options & 1 ? 1 : 0;
    uint8_t full_access      = options & 2 ? 1 : 0;

    uint8_t event[7];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_AUTHENTICATION_REQUEST;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = user_id_required;
    event[pos++] = full_access;
    if (pos != sizeof(event)) log_error("pbap_client_emit_authentication_event size %u", pos);
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static void pbap_client_emit_card_result_event(pbap_client_t * context, const char * name, const char * handle){
    uint8_t event[5 + PBAP_MAX_NAME_LEN + PBAP_MAX_HANDLE_LEN];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_CARD_RESULT;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    int name_len = btstack_min(PBAP_MAX_NAME_LEN, strlen(name));
    event[pos++] = name_len;
    memcpy(&event[pos], name, name_len);
    pos += name_len;
    int handle_len = btstack_min(PBAP_MAX_HANDLE_LEN, strlen(handle));
    event[pos++] = handle_len;
    memcpy(&event[pos], handle, handle_len);
    pos += handle_len;
    event[1] = pos - 2;
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static const uint8_t collon = (uint8_t) ':';

static void pbap_handle_can_send_now(void){
    uint8_t  path_element[20];
    uint16_t path_element_start;
    uint16_t path_element_len;
    uint8_t  application_parameters[PBAP_MAX_PHONE_NUMBER_LEN + 10];
    uint8_t  challenge_response[36];
    int i;
    uint16_t phone_number_len;
    int done;

    MD5_CTX md5_ctx;

    if (pbap_client->abort_operation){
        pbap_client->abort_operation = 0;
        pbap_client->state = PBAP_CONNECTED;
        goep_client_request_create_abort(pbap_client->goep_cid);
        goep_client_execute(pbap_client->goep_cid);
        return;
    }

    switch (pbap_client->state){
        case PBAP_W2_SEND_CONNECT_REQUEST:
            goep_client_request_create_connect(pbap_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(pbap_client->goep_cid, pbap_uuid, 18);
            // Mandatory if the PSE advertises a PbapSupportedFeatures attribute in its SDP record, else excluded.
            if (goep_client_get_pbap_supported_features(pbap_client->goep_cid) != PBAP_FEATURES_NOT_PRESENT){
                application_parameters[0] = PBAP_APPLICATION_PARAMETER_PBAP_SUPPORTED_FEATURES;
                application_parameters[1] = 4;
                big_endian_store_32(application_parameters, 2, pbap_supported_features);
                goep_client_header_add_application_parameters(pbap_client->goep_cid, &application_parameters[0], 6);
            }
            pbap_client->state = PBAP_W4_CONNECT_RESPONSE;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SEND_AUTHENTICATED_CONNECT:
            goep_client_request_create_connect(pbap_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(pbap_client->goep_cid, pbap_uuid, 16);
            // setup authentication challenge response
            i = 0;
            challenge_response[i++] = 0;  // Tag Digest
            challenge_response[i++] = 16; // Len
            // calculate md5
            MD5_Init(&md5_ctx);
            MD5_Update(&md5_ctx, pbap_client->authentication_nonce, 16);
            MD5_Update(&md5_ctx, &collon, 1);
            MD5_Update(&md5_ctx, pbap_client->authentication_password, strlen(pbap_client->authentication_password));
            MD5_Final(&challenge_response[i], &md5_ctx);
            i += 16;
            challenge_response[i++] = 2;  // Tag Nonce
            challenge_response[i++] = 16; // Len
            memcpy(&challenge_response[i], pbap_client->authentication_nonce, 16);
            i += 16;
            goep_client_header_add_challenge_response(pbap_client->goep_cid, challenge_response, i);
            pbap_client->state = PBAP_W4_CONNECT_RESPONSE;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SEND_DISCONNECT_REQUEST:
            goep_client_request_create_disconnect(pbap_client->goep_cid);
            pbap_client->state = PBAP_W4_DISCONNECT_RESPONSE;
            goep_client_execute(pbap_client->goep_cid);
            return;
        case PBAP_W2_PULL_PHONEBOOK:
        case PBAP_W2_GET_PHONEBOOK_SIZE:
            goep_client_request_create_get(pbap_client->goep_cid);
            if (pbap_client->request_number == 0){
                if (!pbap_client->flow_control_enabled){
                    goep_client_header_add_srm_enable(pbap_client->goep_cid);
                    pbap_client->srm_state = SRM_W4_CONFIRM;
                }
                goep_client_header_add_name(pbap_client->goep_cid, pbap_client->phonebook_path);
                goep_client_header_add_type(pbap_client->goep_cid, pbap_phonebook_type);
                i = 0;
                if (pbap_client->vcard_selector_supported){
                    // vCard Selector
                    if (pbap_client->vcard_selector){
                        application_parameters[i++] = PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR;
                        application_parameters[i++] = 8;
                        memset(&application_parameters[i], 0, 4);
                        i += 4;
                        big_endian_store_32(application_parameters, i, pbap_client->vcard_selector);
                        i += 4;
                    }
                    // vCard Selector Operator
                    if (pbap_client->vcard_selector_operator != PBAP_VCARD_SELECTOR_OPERATOR_OR){
                        application_parameters[i++] = PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR_OPERATOR;
                        application_parameters[i++] = 1;
                        application_parameters[i++] = pbap_client->vcard_selector_operator;
                    }
                }
                if (pbap_client->state == PBAP_W2_GET_PHONEBOOK_SIZE){
                    // Regular TLV wih 1-byte len
                    application_parameters[i++] = PBAP_APPLICATION_PARAMETER_MAX_LIST_COUNT;
                    application_parameters[i++] = 2;
                    big_endian_store_16(application_parameters, 2, 0);
                    i += 2;
                }
                if (i){
                    goep_client_header_add_application_parameters(pbap_client->goep_cid, application_parameters, i);
                }
            }
            if (pbap_client->state == PBAP_W2_GET_PHONEBOOK_SIZE){
                // state
                pbap_client->state = PBAP_W4_GET_PHONEBOOK_SIZE_COMPLETE;
            } else {
                // state
                pbap_client->state = PBAP_W4_PHONEBOOK;
            }
            // send packet
            pbap_client->request_number++;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_GET_CARD_LIST:
            goep_client_request_create_get(pbap_client->goep_cid);
            if (pbap_client->request_number == 0){
                if (!pbap_client->flow_control_enabled){
                    goep_client_header_add_srm_enable(pbap_client->goep_cid);
                    pbap_client->srm_state = SRM_W4_CONFIRM;
                }
                goep_client_header_add_name(pbap_client->goep_cid, pbap_client->phonebook_path);
                goep_client_header_add_type(pbap_client->goep_cid, pbap_vcard_listing_type);
                i = 0;
                if (pbap_client->vcard_selector_supported){
                    // vCard Selector
                    if (pbap_client->vcard_selector){
                        application_parameters[i++] = PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR;
                        application_parameters[i++] = 8;
                        memset(&application_parameters[i], 0, 4);
                        i += 4;
                        big_endian_store_32(application_parameters, i, pbap_client->vcard_selector);
                        i += 4;
                    }
                    // vCard Selector Operator
                    if (pbap_client->vcard_selector_operator != PBAP_VCARD_SELECTOR_OPERATOR_OR){
                        application_parameters[i++] = PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR_OPERATOR;
                        application_parameters[i++] = 1;
                        application_parameters[i++] = pbap_client->vcard_selector_operator;
                    }
                }
                if (pbap_client->phone_number){
                    // Search by phpone number
                    phone_number_len = btstack_min(PBAP_MAX_PHONE_NUMBER_LEN, strlen(pbap_client->phone_number));
                    application_parameters[i++] = PBAP_APPLICATION_PARAMETER_SEARCH_VALUE;
                    application_parameters[i++] = phone_number_len;
                    memcpy(&application_parameters[i], pbap_client->phone_number, phone_number_len);
                    i += phone_number_len;
                    application_parameters[i++] = PBAP_APPLICATION_PARAMETER_SEARCH_PROPERTY;
                    application_parameters[i++] = 1;
                    application_parameters[i++] = 0x01; // Number
                }
                if (i){
                    goep_client_header_add_application_parameters(pbap_client->goep_cid, &application_parameters[0], i);
                }
            }
            // send packet
            pbap_client->state = PBAP_W4_GET_CARD_LIST_COMPLETE;
            pbap_client->request_number++;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_GET_CARD_ENTRY:
            goep_client_request_create_get(pbap_client->goep_cid);
            if (pbap_client->request_number == 0){
                if (!pbap_client->flow_control_enabled){
                    goep_client_header_add_srm_enable(pbap_client->goep_cid);
                    pbap_client->srm_state = SRM_W4_CONFIRM;
                }
                goep_client_header_add_name(pbap_client->goep_cid, pbap_client->vcard_name);
                goep_client_header_add_type(pbap_client->goep_cid, pbap_vcard_entry_type);
                i = 0;
                if (i){
                    // TODO: support property selector
                    // TODO: support format
                    goep_client_header_add_application_parameters(pbap_client->goep_cid, &application_parameters[0], i);
                }
                pbap_client->state = PBAP_W4_GET_CARD_ENTRY_COMPLETE;
            }
            // send packet
            pbap_client->request_number++;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SET_PATH_ROOT:
            goep_client_request_create_set_path(pbap_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(pbap_client->goep_cid, "");
            // state
            pbap_client->state = PBAP_W4_SET_PATH_ROOT_COMPLETE;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SET_PATH_ELEMENT:
            // find '/' or '\0'
            path_element_start = pbap_client->set_path_offset;
            while (pbap_client->current_folder[pbap_client->set_path_offset] != '\0' &&
                pbap_client->current_folder[pbap_client->set_path_offset] != '/'){
                pbap_client->set_path_offset++;              
            }
            path_element_len = pbap_client->set_path_offset-path_element_start;
            memcpy(path_element, &pbap_client->current_folder[path_element_start], path_element_len);
            path_element[path_element_len] = 0;

            // skip /
            if (pbap_client->current_folder[pbap_client->set_path_offset] == '/'){
                pbap_client->set_path_offset++;  
            }

            // done?
            done = pbap_client->current_folder[pbap_client->set_path_offset] == '\0';

            // status
            log_info("Path element '%s', done %u", path_element, done);

            goep_client_request_create_set_path(pbap_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(pbap_client->goep_cid, (const char *) path_element); // next element
            // state
            pbap_client->state = PBAP_W4_SET_PATH_ELEMENT_COMPLETE;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        default:
            break;
    }
}

static void pbap_parse_authentication_challenge(pbap_client_t * context, const uint8_t * challenge_data, uint16_t challenge_len){
    // printf("Challenge:  ");
    // printf_hexdump(challenge_data, challenge_len);
    int i;
    // uint8_t charset_code = 0;
    for (i=0 ; i<challenge_len ; ){
        int tag = challenge_data[i];
        int len = challenge_data[i + 1];
        i += 2;
        switch (tag) {
            case 0:
                if (len != 0x10) {
                    log_error("Invalid OBEX digest len %u", len);
                    return;
                }
                memcpy(context->authentication_nonce, &challenge_data[i], 16);
                // printf("Nonce: ");
                // printf_hexdump(context->authentication_nonce, 16);
                break;
            case 1:
                context->authentication_options = challenge_data[i];
                // printf("Options %u\n", context->authentication_options);
                break;
            case 2:
                // TODO: handle charset
                // charset_code = challenge_data[i];
                break;
        }
        i += len;
    }
}

static void pbap_process_srm_headers(pbap_client_t * context, uint8_t *packet, uint16_t size){

    if (packet[0] != OBEX_RESP_CONTINUE) return;

    // get SRM and SRMP Headers
    int srm_value = OBEX_SRM_DISABLE;
    int srmp_value = OBEX_SRMP_NEXT;
    obex_iterator_t it;
    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(context->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
        uint8_t hi = obex_iterator_get_hi(&it);
        uint16_t     data_len = obex_iterator_get_data_len(&it);
        const uint8_t  * data = obex_iterator_get_data(&it);
        switch (hi){
            case OBEX_HEADER_SINGLE_RESPONSE_MODE:
                if (data_len != 1) break;
                srm_value = *data;
                break;
            case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
                if (data_len != 1) break;
                srmp_value = *data;
                break;
            default:
                break;
        }
    }

    // Update SRM state based on SRM haders
    switch (context->srm_state){
        case SRM_W4_CONFIRM:
            switch (srm_value){
                case OBEX_SRM_ENABLE:
                    switch (srmp_value){
                        case OBEX_SRMP_WAIT:
                            context->srm_state = SRM_ENABLED_BUT_WAITING;
                            break;
                        default:
                            context->srm_state = SRM_ENABLED;
                            break;
                    }
                    break;
                default:
                    context->srm_state = SRM_DISABLED;
                    break;
            }
            break;
        case SRM_ENABLED_BUT_WAITING:
            switch (srmp_value){
                case OBEX_SRMP_WAIT:
                    context->srm_state = SRM_ENABLED_BUT_WAITING;
                    break;
                default:
                    context->srm_state = SRM_ENABLED;
                    break;
            }
            break;
        default:
            break;
    }
    log_info("SRM state %u", context->srm_state);
}

static void pbap_client_process_vcard_listing(uint8_t *packet, uint16_t size){
    obex_iterator_t it;
    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
        uint8_t hi = obex_iterator_get_hi(&it);
        if (hi == OBEX_HEADER_END_OF_BODY ||
            hi == OBEX_HEADER_BODY){
            uint16_t     data_len = obex_iterator_get_data_len(&it);
            const uint8_t  * data =  obex_iterator_get_data(&it);
            // now try parsing it
            yxml_init(&pbap_client->xml_parser, pbap_client->xml_buffer, sizeof(pbap_client->xml_buffer));
            int card_found = 0;
            int name_found = 0;
            int handle_found = 0;
            char name[PBAP_MAX_NAME_LEN];
            char handle[PBAP_MAX_HANDLE_LEN];
            while (data_len--){
                yxml_ret_t r = yxml_parse(&pbap_client->xml_parser, *data++);
                switch (r){
                    case YXML_ELEMSTART:
                        card_found = strcmp("card", pbap_client->xml_parser.elem) == 0;
                        break;
                    case YXML_ELEMEND:
                        if (card_found){
                            pbap_client_emit_card_result_event(pbap_client, name, handle);
                        }
                        card_found = 0;
                        break;
                    case YXML_ATTRSTART:
                        if (!card_found) break;
                        if (strcmp("name", pbap_client->xml_parser.attr) == 0){
                            name_found = 1;
                            name[0]    = 0;
                            break;
                        }
                        if (strcmp("handle", pbap_client->xml_parser.attr) == 0){
                            handle_found = 1;
                            handle[0]    = 0;
                            break;
                        }
                        break;
                    case YXML_ATTRVAL:
                        if (name_found) {
                            // "In UTF-8, characters from the U+0000..U+10FFFF range (the UTF-16 accessible range) are encoded using sequences of 1 to 4 octets."
                            if (strlen(name) + 4 + 1 >= sizeof(name)) break;
                            strcat(name, pbap_client->xml_parser.data);
                            break;
                        }
                        if (handle_found) {
                            // "In UTF-8, characters from the U+0000..U+10FFFF range (the UTF-16 accessible range) are encoded using sequences of 1 to 4 octets."
                            if (strlen(handle) + 4 + 1 >= sizeof(handle)) break;
                            strcat(handle, pbap_client->xml_parser.data);
                            break;
                        }
                        break;
                    case YXML_ATTREND:
                        name_found = 0;
                        handle_found = 0;
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

static void pbap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events

    obex_iterator_t it;
    uint8_t status;
    int wait_for_user = 0;
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_GOEP_META:
                    switch (hci_event_goep_meta_get_subevent_code(packet)){
                        case GOEP_SUBEVENT_CONNECTION_OPENED:
                            status = goep_subevent_connection_opened_get_status(packet);
                            pbap_client->con_handle = goep_subevent_connection_opened_get_con_handle(packet);
                            pbap_client->incoming = goep_subevent_connection_opened_get_incoming(packet);
                            goep_subevent_connection_opened_get_bd_addr(packet, pbap_client->bd_addr); 
                            if (status){
                                log_info("pbap: connection failed %u", status);
                                pbap_client->state = PBAP_INIT;
                                pbap_client_emit_connected_event(pbap_client, status);
                            } else {
                                log_info("pbap: connection established");
                                pbap_client->goep_cid = goep_subevent_connection_opened_get_goep_cid(packet);
                                pbap_client->state = PBAP_W2_SEND_CONNECT_REQUEST;
                                goep_client_request_can_send_now(pbap_client->goep_cid);
                            }
                            break;
                        case GOEP_SUBEVENT_CONNECTION_CLOSED:
                            if (pbap_client->state != PBAP_CONNECTED){
                                pbap_client_emit_operation_complete_event(pbap_client, OBEX_DISCONNECTED);
                            }
                            pbap_client->state = PBAP_INIT;
                            pbap_client_emit_connection_closed_event(pbap_client);
                            break;
                        case GOEP_SUBEVENT_CAN_SEND_NOW:
                            pbap_handle_can_send_now();
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case GOEP_DATA_PACKET:
            // TODO: handle chunked data
            // obex_dump_packet(goep_client_get_request_opcode(pbap_client->goep_cid), packet, size);
            switch (pbap_client->state){
                case PBAP_W4_CONNECT_RESPONSE:
                    switch (packet[0]){
                        case OBEX_RESP_SUCCESS:
                            for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                                uint8_t hi = obex_iterator_get_hi(&it);
                                if (hi == OBEX_HEADER_CONNECTION_ID){
                                    goep_client_set_connection_id(pbap_client->goep_cid, obex_iterator_get_data_32(&it));
                                }
                            }
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client->vcard_selector_supported = pbap_supported_features & goep_client_get_pbap_supported_features(pbap_client->goep_cid) & PBAP_SUPPORTED_FEATURES_VCARD_SELECTING;
                            pbap_client_emit_connected_event(pbap_client, 0);
                            break;
                        case OBEX_RESP_UNAUTHORIZED:
                            for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                                uint8_t hi = obex_iterator_get_hi(&it);
                                if (hi == OBEX_HEADER_AUTHENTICATION_CHALLENGE){
                                    pbap_parse_authentication_challenge(pbap_client, obex_iterator_get_data(&it), obex_iterator_get_data_len(&it));
                                }
                            }
                            pbap_client->state = PBAP_W4_USER_AUTHENTICATION;
                            pbap_client_emit_authentication_event(pbap_client, pbap_client->authentication_options);
                            break;
                        default:
                            log_info("pbap: obex connect failed, result 0x%02x", packet[0]);
                            pbap_client->state = PBAP_INIT;
                            pbap_client_emit_connected_event(pbap_client, OBEX_CONNECT_FAILED);
                            break;                            
                    }
                    break;
                case PBAP_W4_DISCONNECT_RESPONSE:
                        goep_client_disconnect(pbap_client->goep_cid);
                        break;
                case PBAP_W4_SET_PATH_ROOT_COMPLETE:
                case PBAP_W4_SET_PATH_ELEMENT_COMPLETE:
                    if (packet[0] == OBEX_RESP_SUCCESS){
                        // more path?
                        if (pbap_client->current_folder[pbap_client->set_path_offset]){
                            pbap_client->state = PBAP_W2_SET_PATH_ELEMENT;
                            goep_client_request_can_send_now(pbap_client->goep_cid);
                        } else {
                            pbap_client->current_folder = NULL;   
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, 0);
                        }
                    } else if (packet[0] == OBEX_RESP_NOT_FOUND){
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_NOT_FOUND);
                    } else {
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                    }
                    break;
                case PBAP_W4_PHONEBOOK:
                    pbap_client->flow_next_triggered = 0;
                    wait_for_user = 0;
                    for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                        uint8_t hi = obex_iterator_get_hi(&it);
                        uint16_t     data_len = obex_iterator_get_data_len(&it);
                        const uint8_t  * data = obex_iterator_get_data(&it);
                        switch (hi){
                            case OBEX_HEADER_BODY:
                            case OBEX_HEADER_END_OF_BODY:
                                pbap_client->client_handler(PBAP_DATA_PACKET, pbap_client->cid, (uint8_t *) data, data_len);
                                wait_for_user++;
                                if (wait_for_user > 1){
                                    log_error("wait_for_user %u", wait_for_user);
                                }
                                break;
                            default:
                                break;
                        }
                    }
                    switch(packet[0]){
                        case OBEX_RESP_CONTINUE:
                            pbap_process_srm_headers(pbap_client, packet, size);
                            if (pbap_client->srm_state ==  SRM_ENABLED) break;
                            pbap_client->state = PBAP_W2_PULL_PHONEBOOK;
                            if (!pbap_client->flow_control_enabled || !wait_for_user || pbap_client->flow_next_triggered) {
                                goep_client_request_can_send_now(pbap_client->goep_cid);                
                            }
                            break;
                        case OBEX_RESP_SUCCESS:
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, 0);
                            break;
                        default:
                            log_info("unexpected response 0x%02x", packet[0]);
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                            break;                                                                                
                    }
                    break;
                case PBAP_W4_GET_PHONEBOOK_SIZE_COMPLETE:
                    pbap_client->state = PBAP_CONNECTED;
                    if (packet[0] == OBEX_RESP_SUCCESS){
                        int have_size = 0;
                        uint16_t phonebook_size;
                        for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                            uint8_t hi = obex_iterator_get_hi(&it);
                            if (hi == OBEX_HEADER_APPLICATION_PARAMETERS){
                                uint16_t     data_len = obex_iterator_get_data_len(&it);
                                const uint8_t  * data =  obex_iterator_get_data(&it);
                                // iterate over application headers (TLV with 1 bytes len)
                                unsigned int i = 0;
                                while (i<data_len){
                                    uint8_t tag = data[i++];
                                    uint8_t len = data[i++];
                                    if (tag == PBAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE && len == 2){
                                        have_size = 1;
                                        phonebook_size = big_endian_read_16(data, i);
                                    }
                                    i+=len;
                                }
                            }
                        }
                        if (have_size){
                            pbap_client_emit_phonebook_size_event(pbap_client, 0, phonebook_size);
                            break;
                        }
                    }
                    pbap_client_emit_phonebook_size_event(pbap_client, OBEX_UNKNOWN_ERROR, 0);
                    break;
                case PBAP_W4_GET_CARD_LIST_COMPLETE:
                    switch (packet[0]){
                        case OBEX_RESP_CONTINUE:
                            // process data
                            pbap_client_process_vcard_listing(packet, size);
                            // handle continue
                            pbap_process_srm_headers(pbap_client, packet, size);
                            if (pbap_client->srm_state ==  SRM_ENABLED) break;
                            pbap_client->state = PBAP_W2_GET_CARD_LIST;
                            if (!pbap_client->flow_control_enabled || !wait_for_user || pbap_client->flow_next_triggered) {
                                goep_client_request_can_send_now(pbap_client->goep_cid);                
                            }
                            break;
                        case OBEX_RESP_SUCCESS:
                            // process data
                            pbap_client_process_vcard_listing(packet, size);
                            // done
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, 0);
                            break;
                        case OBEX_RESP_NOT_ACCEPTABLE:
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, OBEX_NOT_ACCEPTABLE);
                            break;
                        default:
                            log_info("unexpected response 0x%02x", packet[0]);
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                            break;
                    }
                    break;
                case PBAP_W4_GET_CARD_ENTRY_COMPLETE:
                    switch (packet[0]){
                        case OBEX_RESP_CONTINUE:
                            pbap_process_srm_headers(pbap_client, packet, size);
                            if (pbap_client->srm_state ==  SRM_ENABLED) break;
                            pbap_client->state = PBAP_W2_GET_CARD_ENTRY;
                            if (!pbap_client->flow_control_enabled || !wait_for_user || pbap_client->flow_next_triggered) {
                                goep_client_request_can_send_now(pbap_client->goep_cid);                
                            }
                            break;
                        case OBEX_RESP_SUCCESS:
                            for (obex_iterator_init_with_response_packet(&it, goep_client_get_request_opcode(pbap_client->goep_cid), packet, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
                                uint8_t hi = obex_iterator_get_hi(&it);
                                if (hi == OBEX_HEADER_END_OF_BODY ||
                                    hi == OBEX_HEADER_BODY){
                                    // uint16_t     data_len = obex_iterator_get_data_len(&it);
                                    // const uint8_t  * data =  obex_iterator_get_data(&it);
                                    // now try parsing it
                                }
                            }
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, 0);
                            break;
                        case OBEX_RESP_NOT_ACCEPTABLE:
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, OBEX_NOT_ACCEPTABLE);
                            break;
                        default:
                            log_info("unexpected response 0x%02x", packet[0]);
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                            break;
                    }
                    break;                
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void pbap_client_init(void){
    memset(pbap_client, 0, sizeof(pbap_client_t));
    pbap_client->state = PBAP_INIT;
    pbap_client->cid = 1;
}

uint8_t pbap_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (pbap_client->state != PBAP_INIT) return BTSTACK_MEMORY_ALLOC_FAILED;

    pbap_client->state = PBAP_W4_GOEP_CONNECTION;
    pbap_client->client_handler = handler;
    pbap_client->vcard_selector = 0;
    pbap_client->vcard_selector_operator = PBAP_VCARD_SELECTOR_OPERATOR_OR;

    uint8_t err = goep_client_create_connection(&pbap_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE, &pbap_client->goep_cid);
    *out_cid = pbap_client->cid;
    if (err) return err;
    return 0;
}

uint8_t pbap_disconnect(uint16_t pbap_cid){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_get_phonebook_size(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_GET_PHONEBOOK_SIZE;
    pbap_client->phonebook_path = path;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_pull_phonebook(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_PULL_PHONEBOOK;
    pbap_client->phonebook_path = path;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_set_phonebook(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_SET_PATH_ROOT;
    pbap_client->current_folder = path;
    pbap_client->set_path_offset = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_authentication_password(uint16_t pbap_cid, const char * password){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_W4_USER_AUTHENTICATION) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_SEND_AUTHENTICATED_CONNECT;
    pbap_client->authentication_password = password;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_pull_vcard_listing(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_GET_CARD_LIST;
    pbap_client->phonebook_path = path;
    pbap_client->phone_number = NULL;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_pull_vcard_entry(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_GET_CARD_ENTRY;
    // pbap_client->phonebook_path = NULL;
    // pbap_client->phone_number = NULL;
    pbap_client->vcard_name = path;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_lookup_by_number(uint16_t pbap_cid, const char * phone_number){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->state = PBAP_W2_GET_CARD_LIST;
    pbap_client->phonebook_path = pbap_vcard_listing_name;
    pbap_client->phone_number   = phone_number;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_abort(uint16_t pbap_cid){
    UNUSED(pbap_cid);
    log_info("abort current operation, state 0x%02x", pbap_client->state);
    pbap_client->abort_operation = 1;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return 0;
}

uint8_t pbap_next_packet(uint16_t pbap_cid){
    // log_info("pbap_next_packet, state %x", pbap_client->state);
    UNUSED(pbap_cid);
    if (!pbap_client->flow_control_enabled) return 0;
    switch (pbap_client->state){
        case PBAP_W2_PULL_PHONEBOOK:
            goep_client_request_can_send_now(pbap_client->goep_cid);
            break;
        case PBAP_W4_PHONEBOOK:
            pbap_client->flow_next_triggered = 1;
            break;
        default:
            break;
    }
    return 0;
}

uint8_t pbap_set_flow_control_mode(uint16_t pbap_cid, int enable){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED) return BTSTACK_BUSY;
    pbap_client->flow_control_enabled = enable;
    return 0;
}

uint8_t pbap_set_vcard_selector(uint16_t pbap_cid, uint32_t vcard_selector){
    UNUSED(pbap_cid);
    pbap_client->vcard_selector = vcard_selector;
    return 0;
}

uint8_t pbap_set_vcard_selector_operator(uint16_t pbap_cid, int vcard_selector_operator){
    UNUSED(pbap_cid);
    pbap_client->vcard_selector_operator = vcard_selector_operator;
    return 0;
}
