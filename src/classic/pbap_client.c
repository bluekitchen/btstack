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

#define BTSTACK_FILE__ "pbap_client.c"
 
#include "btstack_config.h"

#include <stdint.h>
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
#include "classic/obex_parser.h"
#include "classic/goep_client.h"
#include "classic/pbap.h"
#include "classic/pbap_client.h"

// 796135f0-f0c5-11d8-0966- 0800200c9a66
static const uint8_t pbap_uuid[] = { 0x79, 0x61, 0x35, 0xf0, 0xf0, 0xc5, 0x11, 0xd8, 0x09, 0x66, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66};

const char * pbap_phonebook_type     = "x-bt/phonebook";
const char * pbap_vcard_listing_type = "x-bt/vcard-listing";
const char * pbap_vcard_entry_type   = "x-bt/vcard";

const char * pbap_vcard_listing_name = "pb";

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
    PBAP_W4_GET_CARD_ENTRY_COMPLETE,
    // abort operation
    PBAP_W4_ABORT_COMPLETE,

} pbap_state_t;

typedef enum {
    SRM_DISABLED,
    SRM_W4_CONFIRM,
    SRM_ENABLED_BUT_WAITING,
    SRM_ENABLED
} srm_state_t;

typedef enum {
    OBEX_AUTH_PARSER_STATE_W4_TYPE = 0,
    OBEX_AUTH_PARSER_STATE_W4_LEN,
    OBEX_AUTH_PARSER_STATE_W4_VALUE,
    OBEX_AUTH_PARSER_STATE_INVALID,
} obex_auth_parser_state_t;

typedef struct {
    // parsing
    obex_auth_parser_state_t state;
    uint8_t type;
    uint8_t len;
    uint8_t pos;
    // data
    uint8_t  authentication_options;
    uint16_t authentication_nonce[16];
} obex_auth_parser_t;

typedef struct {
    uint8_t srm_value;
    uint8_t srmp_value;
} obex_srm_t;

typedef enum {
    PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_TYPE = 0,
    PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_LEN,
    PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_VALUE,
    PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_INVALID,
} pbap_client_phonebook_size_parser_state_t;

typedef struct {
    // parsing
    pbap_client_phonebook_size_parser_state_t state;
    uint8_t type;
    uint8_t len;
    uint8_t pos;
    // data
    bool have_size;
    uint8_t  size_buffer[2];
} pbap_client_phonebook_size_parser_t;

typedef struct pbap_client {
    pbap_state_t state;
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    uint16_t  goep_cid;
    btstack_packet_handler_t client_handler;
    int request_number;
    const char * current_folder;
    const char * phone_number;
    const char * phonebook_path;
    const char * vcard_name;
    uint16_t set_path_offset;
    /* vcard selector / operator */
    uint32_t vcard_selector;
    uint8_t  vcard_selector_operator;
    uint8_t  vcard_selector_supported;
    /* property selector */
    uint32_t property_selector;
    /* abort */
    uint8_t  abort_operation;
    /* obex parser */
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];
    /* authentication */
    obex_auth_parser_t obex_auth_parser;
    const char * authentication_password;
    /* xml parser */
    yxml_t  xml_parser;
    uint8_t xml_buffer[50];
    /* vcard listing parser */
    bool parser_card_found;
    bool parser_name_found;
    bool parser_handle_found;
    char parser_name[PBAP_MAX_NAME_LEN];
    char parser_handle[PBAP_MAX_HANDLE_LEN];
    /* phonebook size */
    pbap_client_phonebook_size_parser_t phonebook_size_parser;
    /* flow control mode */
    uint8_t flow_control_enabled;
    uint8_t flow_next_triggered;
    bool flow_wait_for_user;
    /* srm */
    obex_srm_t obex_srm;
    srm_state_t srm_state;
} pbap_client_t;

static uint32_t pbap_client_supported_features;

static pbap_client_t pbap_client_singleton;
static pbap_client_t * pbap_client = &pbap_client_singleton;

static void pbap_client_emit_connected_event(pbap_client_t * context, uint8_t status){
    uint8_t event[15];
    int pos = 0;
    event[pos++] = HCI_EVENT_PBAP_META;
    pos++;  // skip len
    event[pos++] = PBAP_SUBEVENT_CONNECTION_OPENED;
    little_endian_store_16(event,pos,context->cid);
    pos+=2;
    event[pos++] = status;
    (void)memcpy(&event[pos], context->bd_addr, 6);
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
    uint8_t user_id_required = (options & 1) ? 1 : 0;
    uint8_t full_access      = (options & 2) ? 1 : 0;

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
    int name_len = btstack_min(PBAP_MAX_NAME_LEN, (uint16_t) strlen(name));
    event[pos++] = name_len;
    (void)memcpy(&event[pos], name, name_len);
    pos += name_len;
    int handle_len = btstack_min(PBAP_MAX_HANDLE_LEN, (uint16_t) strlen(handle));
    event[pos++] = handle_len;
    (void)memcpy(&event[pos], handle, handle_len);
    pos += handle_len;
    event[1] = pos - 2;
    context->client_handler(HCI_EVENT_PACKET, context->cid, &event[0], pos);
}

static const uint8_t collon = (uint8_t) ':';

static void pbap_client_vcard_listing_init_parser(pbap_client_t * client){
    yxml_init(&client->xml_parser, client->xml_buffer, sizeof(client->xml_buffer));
    client->parser_card_found = false;
    client->parser_name_found = false;
    client->parser_handle_found = false;
}

static void pbap_client_phonebook_size_parser_init(pbap_client_phonebook_size_parser_t * phonebook_size_parer){
    memset(phonebook_size_parer, 0, sizeof(pbap_client_phonebook_size_parser_t));
}

static void pbap_client_phoneboook_size_parser_process_data(pbap_client_phonebook_size_parser_t * phonebook_size_parser, const uint8_t * data_buffer, uint16_t data_len){
    while (data_len){
        uint16_t bytes_to_consume = 1;
        switch(phonebook_size_parser->state){
            case PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_INVALID:
                return;
            case PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_TYPE:
                phonebook_size_parser->type = *data_buffer;
                phonebook_size_parser->state = PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_LEN;
                break;
            case PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_LEN:
                phonebook_size_parser->len = *data_buffer;
                phonebook_size_parser->state = PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_VALUE;
                switch (phonebook_size_parser->type){
                    case PBAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE:
                        if (phonebook_size_parser->len != 2){
                            phonebook_size_parser->state = PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_INVALID;
                            return;
                        }
                        break;
                    default:
                        break;
                    }
                break;
            case PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_VALUE:
                bytes_to_consume = btstack_min(phonebook_size_parser->len - phonebook_size_parser->pos, data_len);
                switch (phonebook_size_parser->type){
                    case PBAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE:
                        memcpy(&phonebook_size_parser->size_buffer[phonebook_size_parser->pos], data_buffer, bytes_to_consume);
                        break;
                    default:
                        // ignore data
                        break;
                }
                phonebook_size_parser->pos += bytes_to_consume;
                if (phonebook_size_parser->pos == phonebook_size_parser->len){
                    phonebook_size_parser->state = PBAP_CLIENT_PHONEBOOK_SIZE_PARSER_STATE_W4_TYPE;
                    switch (phonebook_size_parser->type){
                        case PBAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE:
                            phonebook_size_parser->have_size = true;
                            break;
                        default:
                            break;
                    }
                }
                break;
            default:
                break;
        }
        data_buffer += bytes_to_consume;
        data_len    -= bytes_to_consume;
    }
}

static void obex_auth_parser_init(obex_auth_parser_t * auth_parser){
    memset(auth_parser, 0, sizeof(obex_auth_parser_t));
}

static void obex_auth_parser_process_data(obex_auth_parser_t * auth_parser, const uint8_t * data_buffer, uint16_t data_len){
    while (data_len){
        uint16_t bytes_to_consume = 1;
        switch(auth_parser->state){
            case OBEX_AUTH_PARSER_STATE_INVALID:
                return;
            case OBEX_AUTH_PARSER_STATE_W4_TYPE:
                auth_parser->type = *data_buffer;
                auth_parser->state = OBEX_AUTH_PARSER_STATE_W4_LEN;
                break;
            case OBEX_AUTH_PARSER_STATE_W4_LEN:
                auth_parser->len = *data_buffer;
                switch (auth_parser->type){
                    case 0:
                        if (auth_parser->len != 0x10){
                            auth_parser->state = OBEX_AUTH_PARSER_STATE_INVALID;
                            return;
                        }
                        break;
                    case 1:
                        if (auth_parser->len != 0x01){
                            auth_parser->state = OBEX_AUTH_PARSER_STATE_INVALID;
                            return;
                        }
                        break;
                    case 2:
                        // TODO: handle charset
                        // charset_code = challenge_data[i];
                        break;
                    default:
                        break;
                }
                auth_parser->state = OBEX_AUTH_PARSER_STATE_W4_VALUE;
                break;
            case OBEX_AUTH_PARSER_STATE_W4_VALUE:
                bytes_to_consume = btstack_min(auth_parser->len - auth_parser->pos, data_len);
                switch (auth_parser->type){
                    case 0:
                        memcpy(&auth_parser->authentication_nonce[auth_parser->pos], data_buffer, bytes_to_consume);
                        break;
                    case 1:
                        auth_parser->authentication_options = *data_buffer;
                        break;
                    default:
                        // ignore
                        break;
                }
                auth_parser->pos += bytes_to_consume;
                if (auth_parser->pos == auth_parser->len){
                    auth_parser->state = OBEX_AUTH_PARSER_STATE_W4_TYPE;
                }
                break;
            default:
                btstack_unreachable();
                break;
        }
        data_buffer += bytes_to_consume;
        data_len    -= bytes_to_consume;
    }
}

static void obex_srm_init(obex_srm_t * obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}
static void pbap_client_yml_append_character(yxml_t * xml_parser, char * buffer, uint16_t buffer_size){
    // "In UTF-8, characters from the U+0000..U+10FFFF range (the UTF-16 accessible range) are encoded using sequences of 1 to 4 octets."
    uint16_t char_len = (uint16_t) strlen(xml_parser->data);
    btstack_assert(char_len <= 4);
    uint16_t dest_len = (uint16_t) strlen(buffer);
    uint16_t zero_pos = dest_len + char_len;
    if (zero_pos >= buffer_size) return;
    memcpy(&buffer[dest_len], xml_parser->data, char_len);
    buffer[zero_pos] = '\0';
}

static void pbap_client_process_vcard_list_body(const uint8_t * data, uint16_t data_len){
    while (data_len--) {
        yxml_ret_t r = yxml_parse(&pbap_client->xml_parser, *data++);
        switch (r) {
            case YXML_ELEMSTART:
                pbap_client->parser_card_found = strcmp("card", pbap_client->xml_parser.elem) == 0;
                break;
            case YXML_ELEMEND:
                if (pbap_client->parser_card_found) {
                    pbap_client_emit_card_result_event(pbap_client, pbap_client->parser_name,
                                                       pbap_client->parser_handle);
                }
                pbap_client->parser_card_found = false;
                break;
            case YXML_ATTRSTART:
                if (!pbap_client->parser_card_found) break;
                if (strcmp("name", pbap_client->xml_parser.attr) == 0) {
                    pbap_client->parser_name_found = true;
                    pbap_client->parser_name[0] = 0;
                    break;
                }
                if (strcmp("handle", pbap_client->xml_parser.attr) == 0) {
                    pbap_client->parser_handle_found = true;
                    pbap_client->parser_handle[0] = 0;
                    break;
                }
                break;
            case YXML_ATTRVAL:
                if (pbap_client->parser_name_found) {
                    pbap_client_yml_append_character(&pbap_client->xml_parser,
                                                     pbap_client->parser_name,
                                                     sizeof(pbap_client->parser_name));
                    break;
                }
                if (pbap_client->parser_handle_found) {
                    pbap_client_yml_append_character(&pbap_client->xml_parser,
                                                     pbap_client->parser_handle,
                                                     sizeof(pbap_client->parser_handle));
                    break;
                }
                break;
            case YXML_ATTREND:
                pbap_client->parser_name_found = false;
                pbap_client->parser_handle_found = false;
                break;
            default:
                break;
        }
    }
}

static void pbap_client_parser_callback_connect(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    pbap_client_t * client = (pbap_client_t *) user_data;
    switch (header_id){
        case OBEX_HEADER_CONNECTION_ID:
            if (obex_parser_header_store(client->obex_header_buffer, sizeof(client->obex_header_buffer), total_len, data_offset, data_buffer, data_len) == OBEX_PARSER_HEADER_COMPLETE){
                goep_client_set_connection_id(client->goep_cid, big_endian_read_32(client->obex_header_buffer, 0));
            }
            break;
        case OBEX_HEADER_AUTHENTICATION_CHALLENGE:
            obex_auth_parser_process_data(&client->obex_auth_parser, data_buffer, data_len);
            break;
        default:
            break;
    }
}

static void pbap_client_parser_callback_get_phonebook_size(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    UNUSED(total_len);
    UNUSED(data_offset);
    pbap_client_t *client = (pbap_client_t *) user_data;
    switch (header_id) {
        case OBEX_HEADER_APPLICATION_PARAMETERS:
            pbap_client_phoneboook_size_parser_process_data(&client->phonebook_size_parser, data_buffer, data_len);
            break;
        default:
            break;
    }
}

static void pbap_client_parser_callback_get_operation(void * user_data, uint8_t header_id, uint16_t total_len, uint16_t data_offset, const uint8_t * data_buffer, uint16_t data_len){
    pbap_client_t *client = (pbap_client_t *) user_data;
    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&client->obex_srm.srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&client->obex_srm.srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_BODY:
        case OBEX_HEADER_END_OF_BODY:
            switch(pbap_client->state){
                case PBAP_W4_PHONEBOOK:
                case PBAP_W4_GET_CARD_ENTRY_COMPLETE:
                    client->client_handler(PBAP_DATA_PACKET, client->cid, (uint8_t *) data_buffer, data_len);
                    if (data_offset + data_len == total_len){
                        client->flow_wait_for_user = true;
                    }
                    break;
                case PBAP_W4_GET_CARD_LIST_COMPLETE:
                    pbap_client_process_vcard_list_body(data_buffer, data_len);
                    break;
                default:
                    btstack_unreachable();
                    break;
            }
            break;
        default:
            // ignore other headers
            break;
    }
}

static uint16_t pbap_client_application_params_add_vcard_selector(const pbap_client_t * client, uint8_t * application_parameters){
    uint16_t pos = 0;
    if (client->vcard_selector_supported){
        // vCard Selector
        if (pbap_client->vcard_selector){
            application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR;
            application_parameters[pos++] = 8;
            memset(&application_parameters[pos], 0, 4);
            pos += 4;
            big_endian_store_32(application_parameters, pos, client->vcard_selector);
            pos += 4;
        }
        // vCard Selector Operator
        if (client->vcard_selector_operator != PBAP_VCARD_SELECTOR_OPERATOR_OR){
            application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR_OPERATOR;
            application_parameters[pos++] = 1;
            application_parameters[pos++] = client->vcard_selector_operator;
        }
    }
    return pos;
}

static uint16_t pbap_client_application_params_add_max_list_count(const pbap_client_t * client, uint8_t * application_parameters, uint16_t max_count){
    UNUSED(client);
    uint16_t pos = 0;
    application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_MAX_LIST_COUNT;
    application_parameters[pos++] = 2;
    big_endian_store_16(application_parameters, 2, max_count);
    pos += 2;
    return pos;
}

// max size: PBAP_MAX_PHONE_NUMBER_LEN + 5
static uint16_t pbap_client_application_params_add_phone_number(const pbap_client_t * client, uint8_t * application_parameters){
    uint16_t pos = 0;
    if (client->phone_number){
        // Search by phone number
        uint16_t phone_number_len = btstack_min(PBAP_MAX_PHONE_NUMBER_LEN, (uint16_t) strlen(client->phone_number));
        application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_SEARCH_VALUE;
		btstack_assert(phone_number_len <= 255);
        application_parameters[pos++] = (uint8_t) phone_number_len;
        (void)memcpy(&application_parameters[pos],
                     pbap_client->phone_number, phone_number_len);
        pos += phone_number_len;
        application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_SEARCH_PROPERTY;
        application_parameters[pos++] = 1;
        application_parameters[pos++] = 0x01; // Number
    }
    return pos;
}

static uint16_t pbap_client_application_params_add_property_selector(const pbap_client_t * client, uint8_t * application_parameters){
    // TODO: support format
    uint16_t pos = 0;
    uint32_t property_selector_lower = client->property_selector;
    if (pbap_client->vcard_name != NULL){
        if (strncmp(pbap_client->vcard_name, "X-BT-UID:", 9) == 0) {
            property_selector_lower |= 1U << 31;
        }
        if (strncmp(pbap_client->vcard_name, "X-BT-UCI:", 9) == 0) {
            property_selector_lower |= 1U << 30;
        }
    }
    if (property_selector_lower != 0){
        application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_PROPERTY_SELECTOR;
        application_parameters[pos++] = 8;
        big_endian_store_32(application_parameters, pos, 0);    // upper 32-bits are reserved/unused so far
        pos += 4;
        big_endian_store_32(application_parameters, pos, property_selector_lower);
        pos += 4;
    }
    return pos;
}

// Mandatory if the PSE advertises a PbapSupportedFeatures attribute in its SDP record, else excluded.
static uint16_t pbap_client_application_parameters_add_supported_features(const pbap_client_t * client, uint8_t *application_parameters) {
    uint16_t pos = 0;
    if (goep_client_get_pbap_supported_features(client->goep_cid) != PBAP_FEATURES_NOT_PRESENT){
        application_parameters[pos++] = PBAP_APPLICATION_PARAMETER_PBAP_SUPPORTED_FEATURES;
        application_parameters[pos++] = 4;
        big_endian_store_32(application_parameters, 2, pbap_client_supported_features);
        pos += 4;
    }
    return pos;
}

static void pbap_client_add_application_parameters(const pbap_client_t * client, uint8_t * application_parameters, uint16_t len){
    if (len > 0){
        goep_client_header_add_application_parameters(client->goep_cid, &application_parameters[0], len);
    }
}

static void pbap_client_prepare_srm_header(const pbap_client_t * client){
    if (!client->flow_control_enabled && goep_client_version_20_or_higher(client->goep_cid)){
        goep_client_header_add_srm_enable(client->goep_cid);
        pbap_client->srm_state = SRM_W4_CONFIRM;
    }
}

static void pbap_client_prepare_get_operation(pbap_client_t * client){
    obex_parser_init_for_response(&client->obex_parser, OBEX_OPCODE_GET, pbap_client_parser_callback_get_operation, pbap_client);
    obex_srm_init(&client->obex_srm);
    client->obex_parser_waiting_for_response = true;
}

static void pbap_handle_can_send_now(void){
    uint16_t path_element_start;
    uint16_t path_element_len;
    const char * path_element;
    uint8_t  application_parameters[PBAP_MAX_PHONE_NUMBER_LEN + 10];
    uint8_t  challenge_response[36];
    uint16_t pos;

    MD5_CTX md5_ctx;

    if (pbap_client->abort_operation){
        pbap_client->abort_operation = 0;
        // prepare request
        goep_client_request_create_abort(pbap_client->goep_cid);
        // state
        pbap_client->state = PBAP_W4_ABORT_COMPLETE;
        // prepare response
        obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_ABORT, NULL, pbap_client);
        obex_srm_init(&pbap_client->obex_srm);
        pbap_client->obex_parser_waiting_for_response = true;
        // send packet
        goep_client_execute(pbap_client->goep_cid);
        return;
    }

    switch (pbap_client->state){
        case PBAP_W2_SEND_CONNECT_REQUEST:
            // prepare request
            goep_client_request_create_connect(pbap_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(pbap_client->goep_cid, pbap_uuid, 16);
            pos = 0;
            pos += pbap_client_application_parameters_add_supported_features(pbap_client, &application_parameters[pos]);
            pbap_client_add_application_parameters(pbap_client, application_parameters, pos);
            // state
            pbap_client->state = PBAP_W4_CONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_CONNECT, pbap_client_parser_callback_connect, pbap_client);
            obex_auth_parser_init(&pbap_client->obex_auth_parser);
            obex_srm_init(&pbap_client->obex_srm);
            pbap_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SEND_AUTHENTICATED_CONNECT:
            // prepare request
            goep_client_request_create_connect(pbap_client->goep_cid, OBEX_VERSION, 0, OBEX_MAX_PACKETLEN_DEFAULT);
            goep_client_header_add_target(pbap_client->goep_cid, pbap_uuid, 16);
            // setup authentication challenge response
            pos = 0;
            challenge_response[pos++] = 0;  // Tag Digest
            challenge_response[pos++] = 16; // Len
            // calculate md5
            MD5_Init(&md5_ctx);
            MD5_Update(&md5_ctx, pbap_client->obex_auth_parser.authentication_nonce, 16);
            MD5_Update(&md5_ctx, &collon, 1);
            MD5_Update(&md5_ctx, pbap_client->authentication_password, (uint16_t) strlen(pbap_client->authentication_password));
            MD5_Final(&challenge_response[pos], &md5_ctx);
            pos += 16;
            challenge_response[pos++] = 2;  // Tag Nonce
            challenge_response[pos++] = 16; // Len
            (void)memcpy(&challenge_response[pos], pbap_client->obex_auth_parser.authentication_nonce, 16);
            pos += 16;
            goep_client_header_add_challenge_response(pbap_client->goep_cid, challenge_response, pos);
            // state
            pbap_client->state = PBAP_W4_CONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_CONNECT, pbap_client_parser_callback_connect, pbap_client);
            obex_srm_init(&pbap_client->obex_srm);
            pbap_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SEND_DISCONNECT_REQUEST:
            // prepare request
            goep_client_request_create_disconnect(pbap_client->goep_cid);
            // state
            pbap_client->state = PBAP_W4_DISCONNECT_RESPONSE;
            // prepare response
            obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_DISCONNECT, NULL, pbap_client);
            obex_srm_init(&pbap_client->obex_srm);
            pbap_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            return;
        case PBAP_W2_GET_PHONEBOOK_SIZE:
            // prepare request
            goep_client_request_create_get(pbap_client->goep_cid);
            pbap_client_prepare_srm_header(pbap_client);
            goep_client_header_add_name(pbap_client->goep_cid, pbap_client->phonebook_path);
            goep_client_header_add_type(pbap_client->goep_cid, pbap_phonebook_type);

            pos = 0;
            pos += pbap_client_application_params_add_vcard_selector(pbap_client, &application_parameters[pos]);
            pos += pbap_client_application_params_add_max_list_count(pbap_client, &application_parameters[pos], 0); // just get size
            pbap_client_add_application_parameters(pbap_client, application_parameters, pos);

            // state
            pbap_client->state = PBAP_W4_GET_PHONEBOOK_SIZE_COMPLETE;
            // prepare response
            obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_GET, pbap_client_parser_callback_get_phonebook_size, pbap_client);
            obex_srm_init(&pbap_client->obex_srm);
            pbap_client_phonebook_size_parser_init(&pbap_client->phonebook_size_parser);
            pbap_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_PULL_PHONEBOOK:
            // prepare request
            goep_client_request_create_get(pbap_client->goep_cid);
            if (pbap_client->request_number == 0){
                pbap_client_prepare_srm_header(pbap_client);
                goep_client_header_add_name(pbap_client->goep_cid, pbap_client->phonebook_path);
                goep_client_header_add_type(pbap_client->goep_cid, pbap_phonebook_type);

                pos = 0;
                pos += pbap_client_application_params_add_property_selector(pbap_client, &application_parameters[pos]);
                pos += pbap_client_application_params_add_vcard_selector(pbap_client, &application_parameters[pos]);
                pbap_client_add_application_parameters(pbap_client, application_parameters, pos);
            }
            // state
            pbap_client->state = PBAP_W4_PHONEBOOK;
            pbap_client->flow_next_triggered = 0;
            pbap_client->flow_wait_for_user = 0;
            // prepare response
            pbap_client_prepare_get_operation(pbap_client);
            // send packet
            pbap_client->request_number++;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_GET_CARD_LIST:
            // prepare request
            goep_client_request_create_get(pbap_client->goep_cid);
            if (pbap_client->request_number == 0){
                pbap_client_prepare_srm_header(pbap_client);
                goep_client_header_add_name(pbap_client->goep_cid, pbap_client->phonebook_path);
                goep_client_header_add_type(pbap_client->goep_cid, pbap_vcard_listing_type);

                pos = 0;
                pos += pbap_client_application_params_add_vcard_selector(pbap_client, &application_parameters[pos]);
                pos += pbap_client_application_params_add_phone_number(pbap_client, &application_parameters[pos]);
                pbap_client_add_application_parameters(pbap_client, application_parameters, pos);
            }
            // state
            pbap_client->state = PBAP_W4_GET_CARD_LIST_COMPLETE;
            // prepare response
            pbap_client_prepare_get_operation(pbap_client);
            // send packet
            pbap_client->request_number++;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_GET_CARD_ENTRY:
            // prepare request
            goep_client_request_create_get(pbap_client->goep_cid);
            if (pbap_client->request_number == 0){
                pbap_client_prepare_srm_header(pbap_client);
                goep_client_header_add_name(pbap_client->goep_cid, pbap_client->vcard_name);
                goep_client_header_add_type(pbap_client->goep_cid, pbap_vcard_entry_type);

                pos = 0;
                pos += pbap_client_application_params_add_property_selector(pbap_client, &application_parameters[pos]);
                pbap_client_add_application_parameters(pbap_client, application_parameters, pos);
            }
            // state
            pbap_client->state = PBAP_W4_GET_CARD_ENTRY_COMPLETE;
            // prepare response
            pbap_client_prepare_get_operation(pbap_client);
            // send packet
            pbap_client->request_number++;
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SET_PATH_ROOT:
            // prepare request
            goep_client_request_create_set_path(pbap_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name(pbap_client->goep_cid, "");
            // state
            pbap_client->state = PBAP_W4_SET_PATH_ROOT_COMPLETE;
            // prepare response
            obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_SETPATH, NULL, pbap_client);
            obex_srm_init(&pbap_client->obex_srm);
            pbap_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        case PBAP_W2_SET_PATH_ELEMENT:
            // prepare request
            // find '/' or '\0'
            path_element_start = pbap_client->set_path_offset;
            while ((pbap_client->current_folder[pbap_client->set_path_offset] != '\0') &&
                (pbap_client->current_folder[pbap_client->set_path_offset] != '/')){
                pbap_client->set_path_offset++;              
            }
            path_element_len = pbap_client->set_path_offset-path_element_start;
            path_element = (const char *) &pbap_client->current_folder[path_element_start];

            // skip /
            if (pbap_client->current_folder[pbap_client->set_path_offset] == '/'){
                pbap_client->set_path_offset++;  
            }

            goep_client_request_create_set_path(pbap_client->goep_cid, 1 << 1); // Don’t create directory
            goep_client_header_add_name_prefix(pbap_client->goep_cid, path_element, path_element_len); // next element
            // state
            pbap_client->state = PBAP_W4_SET_PATH_ELEMENT_COMPLETE;
            // prepare response
            obex_parser_init_for_response(&pbap_client->obex_parser, OBEX_OPCODE_SETPATH, NULL, pbap_client);
            obex_srm_init(&pbap_client->obex_srm);
            pbap_client->obex_parser_waiting_for_response = true;
            // send packet
            goep_client_execute(pbap_client->goep_cid);
            break;
        default:
            break;
    }
}

static void pbap_client_handle_srm_headers(pbap_client_t *context) {
    const obex_srm_t * obex_srm = &pbap_client->obex_srm;
    // Update SRM state based on SRM headers
    switch (context->srm_state){
        case SRM_W4_CONFIRM:
            switch (obex_srm->srm_value){
                case OBEX_SRM_ENABLE:
                    switch (obex_srm->srmp_value){
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
            switch (obex_srm->srmp_value){
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

static void pbap_packet_handler_hci(uint8_t *packet, uint16_t size){
    UNUSED(size);
    uint8_t status;
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
                    if (pbap_client->state > PBAP_CONNECTED){
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_DISCONNECTED);
                    }
                    pbap_client->state = PBAP_INIT;
                    pbap_client_emit_connection_closed_event(pbap_client);
                    break;
                case GOEP_SUBEVENT_CAN_SEND_NOW:
                    pbap_handle_can_send_now();
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void pbap_packet_handler_goep(uint8_t *packet, uint16_t size){
    if (pbap_client->obex_parser_waiting_for_response == false) return;

    obex_parser_object_state_t parser_state;
    parser_state = obex_parser_process_data(&pbap_client->obex_parser, packet, size);
    if (parser_state == OBEX_PARSER_OBJECT_STATE_COMPLETE){
        pbap_client->obex_parser_waiting_for_response = false;
        obex_parser_operation_info_t op_info;
        obex_parser_get_operation_info(&pbap_client->obex_parser, &op_info);
        switch (pbap_client->state){
            case PBAP_W4_CONNECT_RESPONSE:
                switch (op_info.response_code) {
                    case OBEX_RESP_SUCCESS:
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client->vcard_selector_supported = pbap_client_supported_features & goep_client_get_pbap_supported_features( pbap_client->goep_cid) & PBAP_SUPPORTED_FEATURES_VCARD_SELECTING;
                        pbap_client_emit_connected_event(pbap_client, ERROR_CODE_SUCCESS);
                        break;
                    case OBEX_RESP_UNAUTHORIZED:
                        pbap_client->state = PBAP_W4_USER_AUTHENTICATION;
                        pbap_client_emit_authentication_event(pbap_client, pbap_client->obex_auth_parser.authentication_options);
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
                switch (op_info.response_code) {
                    case OBEX_RESP_SUCCESS:
                        // more path?
                        if (pbap_client->current_folder[pbap_client->set_path_offset]) {
                            pbap_client->state = PBAP_W2_SET_PATH_ELEMENT;
                            goep_client_request_can_send_now(pbap_client->goep_cid);
                        } else {
                            pbap_client->current_folder = NULL;
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_operation_complete_event(pbap_client, ERROR_CODE_SUCCESS);
                        }
                        break;
                    case OBEX_RESP_NOT_FOUND:
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_NOT_FOUND);
                        break;
                    default:
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                        break;
                }
                break;
            case PBAP_W4_PHONEBOOK:
                switch (op_info.response_code) {
                    case OBEX_RESP_CONTINUE:
                        pbap_client_handle_srm_headers(pbap_client);
                        if (pbap_client->srm_state == SRM_ENABLED) {
                            // prepare response
                            pbap_client_prepare_get_operation(pbap_client);
                            break;
                        }
                        pbap_client->state = PBAP_W2_PULL_PHONEBOOK;
                        if (!pbap_client->flow_control_enabled || !pbap_client->flow_wait_for_user ||
                            pbap_client->flow_next_triggered) {
                            goep_client_request_can_send_now(pbap_client->goep_cid);
                        }
                        break;
                    case OBEX_RESP_SUCCESS:
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, ERROR_CODE_SUCCESS);
                        break;
                    default:
                        log_info("unexpected response 0x%02x", packet[0]);
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, OBEX_UNKNOWN_ERROR);
                        break;
                }
                break;
            case PBAP_W4_GET_PHONEBOOK_SIZE_COMPLETE:
                switch (op_info.response_code) {
                    case OBEX_RESP_SUCCESS:
                        if (pbap_client->phonebook_size_parser.have_size) {
                            uint16_t phonebook_size = big_endian_read_16(pbap_client->phonebook_size_parser.size_buffer, 0);
                            pbap_client->state = PBAP_CONNECTED;
                            pbap_client_emit_phonebook_size_event(pbap_client, 0, phonebook_size);
                            break;
                        }
                        /* fall through */
                    default:
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_phonebook_size_event(pbap_client, OBEX_UNKNOWN_ERROR, 0);
                        break;
                }
                break;
            case PBAP_W4_GET_CARD_LIST_COMPLETE:
                switch (op_info.response_code) {
                    case OBEX_RESP_CONTINUE:
                        // handle continue
                        pbap_client_handle_srm_headers(pbap_client);
                        if (pbap_client->srm_state == SRM_ENABLED) {
                            // prepare response
                            pbap_client_prepare_get_operation(pbap_client);
                            break;
                        }
                        pbap_client->state = PBAP_W2_GET_CARD_LIST;
                        goep_client_request_can_send_now(pbap_client->goep_cid);
                        break;
                    case OBEX_RESP_SUCCESS:
                        // done
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, ERROR_CODE_SUCCESS);
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
                switch (op_info.response_code) {
                    case OBEX_RESP_CONTINUE:
                        pbap_client_handle_srm_headers(pbap_client);
                        if (pbap_client->srm_state == SRM_ENABLED) {
                            // prepare response
                            pbap_client_prepare_get_operation(pbap_client);
                            break;
                        }
                        pbap_client->state = PBAP_W2_GET_CARD_ENTRY;
                        goep_client_request_can_send_now(pbap_client->goep_cid);
                        break;
                    case OBEX_RESP_SUCCESS:
                        pbap_client->state = PBAP_CONNECTED;
                        pbap_client_emit_operation_complete_event(pbap_client, ERROR_CODE_SUCCESS);
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
            case PBAP_W4_ABORT_COMPLETE:
                pbap_client->state = PBAP_CONNECTED;
                pbap_client_emit_operation_complete_event(pbap_client, OBEX_ABORTED);
                break;
            default:
                btstack_unreachable();
                break;
        }
    }
}

static void pbap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel); // ok: there is no channel
    UNUSED(size);    // ok: handling own geop events

    switch (packet_type){
        case HCI_EVENT_PACKET:
            pbap_packet_handler_hci(packet, size);
            break;
        case GOEP_DATA_PACKET:
            pbap_packet_handler_goep(packet, size);
            break;
        default:
            break;
    }
}

static void pbap_client_reset_state(void) {
    pbap_client_supported_features =
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
    pbap_client->state = PBAP_INIT;
    pbap_client->cid = 1;
}

void pbap_client_init(void){
    pbap_client_reset_state();
}

void pbap_client_deinit(void){
    pbap_client_reset_state();
    memset(pbap_client, 0, sizeof(pbap_client_t));
}

uint8_t pbap_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid){
    if (pbap_client->state != PBAP_INIT){
        return BTSTACK_MEMORY_ALLOC_FAILED;
    } 

    pbap_client->state = PBAP_W4_GOEP_CONNECTION;
    pbap_client->client_handler = handler;
    pbap_client->vcard_selector = 0;
    pbap_client->vcard_selector_operator = PBAP_VCARD_SELECTOR_OPERATOR_OR;

    uint8_t err = goep_client_create_connection(&pbap_packet_handler, addr, BLUETOOTH_SERVICE_CLASS_PHONEBOOK_ACCESS_PSE, &pbap_client->goep_cid);
    *out_cid = pbap_client->cid;
    if (err) return err;
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_disconnect(uint16_t pbap_cid){
    UNUSED(pbap_cid);
    if (pbap_client->state < PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_SEND_DISCONNECT_REQUEST;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_get_phonebook_size(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_GET_PHONEBOOK_SIZE;
    pbap_client->phonebook_path = path;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_pull_phonebook(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_PULL_PHONEBOOK;
    pbap_client->phonebook_path = path;
    pbap_client->vcard_name = NULL;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_set_phonebook(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_SET_PATH_ROOT;
    pbap_client->current_folder = path;
    pbap_client->set_path_offset = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_authentication_password(uint16_t pbap_cid, const char * password){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_W4_USER_AUTHENTICATION){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_SEND_AUTHENTICATED_CONNECT;
    pbap_client->authentication_password = password;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_pull_vcard_listing(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_GET_CARD_LIST;
    pbap_client->phonebook_path = path;
    pbap_client->phone_number = NULL;
    pbap_client->request_number = 0;
    pbap_client_vcard_listing_init_parser(pbap_client);
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_pull_vcard_entry(uint16_t pbap_cid, const char * path){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_GET_CARD_ENTRY;
    // pbap_client->phonebook_path = NULL;
    // pbap_client->phone_number = NULL;
    pbap_client->vcard_name = path;
    pbap_client->request_number = 0;
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_lookup_by_number(uint16_t pbap_cid, const char * phone_number){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->state = PBAP_W2_GET_CARD_LIST;
    pbap_client->phonebook_path = pbap_vcard_listing_name;
    pbap_client->phone_number   = phone_number;
    pbap_client->request_number = 0;
    pbap_client_vcard_listing_init_parser(pbap_client);
    goep_client_request_can_send_now(pbap_client->goep_cid);
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_abort(uint16_t pbap_cid){
    UNUSED(pbap_cid);
    if ((pbap_client->state < PBAP_CONNECTED) || (pbap_client->abort_operation != 0)){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    log_info("abort current operation, state 0x%02x", pbap_client->state);
    pbap_client->abort_operation = 1;
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_next_packet(uint16_t pbap_cid){
    // log_info("pbap_next_packet, state %x", pbap_client->state);
    UNUSED(pbap_cid);
    if (!pbap_client->flow_control_enabled){
        return ERROR_CODE_SUCCESS;
    }
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
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_set_flow_control_mode(uint16_t pbap_cid, int enable){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->flow_control_enabled = enable;
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_set_vcard_selector(uint16_t pbap_cid, uint32_t vcard_selector){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->vcard_selector = vcard_selector;
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_set_vcard_selector_operator(uint16_t pbap_cid, int vcard_selector_operator){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->vcard_selector_operator = vcard_selector_operator;
    return ERROR_CODE_SUCCESS;
}

uint8_t pbap_set_property_selector(uint16_t pbap_cid, uint32_t property_selector){
    UNUSED(pbap_cid);
    if (pbap_client->state != PBAP_CONNECTED){
        return BTSTACK_BUSY;
    }
    pbap_client->property_selector  = property_selector;
    return ERROR_CODE_SUCCESS;
}
