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

/**
 * @title PBAP Client
 *
 */

#ifndef PBAP_CLIENT_H
#define PBAP_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "btstack_config.h"
#include "yxml.h"
#include "classic/obex_srm_client.h"
#include "classic/obex_parser.h"

// max len of phone number used for lookup in pbap_lookup_by_number
#define PBAP_MAX_PHONE_NUMBER_LEN 32

// max len of name reported in PBAP_SUBEVENT_CARD_RESULT
#define PBAP_MAX_NAME_LEN   32
// max len of vcard handle reported in PBAP_SUBEVENT_CARD_RESULT
#define PBAP_MAX_HANDLE_LEN 16


typedef enum {
    PBAP_CLIENT_INIT = 0,
    PBAP_CLIENT_W4_GOEP_CONNECTION,
    PBAP_CLIENT_W2_SEND_CONNECT_REQUEST,
    PBAP_CLIENT_W4_CONNECT_RESPONSE,
    PBAP_CLIENT_W4_USER_AUTHENTICATION,
    PBAP_CLIENT_W2_SEND_AUTHENTICATED_CONNECT,
    PBAP_CLIENT_CONNECTED,
    //
    PBAP_CLIENT_W2_SEND_DISCONNECT_REQUEST,
    PBAP_CLIENT_W4_DISCONNECT_RESPONSE,
    //
    PBAP_CLIENT_W2_PULL_PHONEBOOK,
    PBAP_CLIENT_W4_PHONEBOOK,
    PBAP_CLIENT_W2_SET_PATH_ROOT,
    PBAP_CLIENT_W4_SET_PATH_ROOT_COMPLETE,
    PBAP_CLIENT_W2_SET_PATH_ELEMENT,
    PBAP_CLIENT_W4_SET_PATH_ELEMENT_COMPLETE,
    PBAP_CLIENT_W2_GET_PHONEBOOK_SIZE,
    PBAP_CLIENT_W4_GET_PHONEBOOK_SIZE_COMPLETE,
    // - pull vacard liast
    PBAP_CLIENT_W2_GET_CARD_LIST,
    PBAP_CLIENT_W4_GET_CARD_LIST_COMPLETE,
    // - pull vcard entry
    PBAP_CLIENT_W2_GET_CARD_ENTRY,
    PBAP_CLIENT_W4_GET_CARD_ENTRY_COMPLETE,
    // abort operation
    PBAP_CLIENT_W4_ABORT_COMPLETE,

} pbap_client_state_t;

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

typedef enum {
    OBEX_AUTH_PARSER_STATE_W4_TYPE = 0,
    OBEX_AUTH_PARSER_STATE_W4_LEN,
    OBEX_AUTH_PARSER_STATE_W4_VALUE,
    OBEX_AUTH_PARSER_STATE_INVALID,
} pbap_client_obex_auth_parser_state_t;

typedef struct {
    // parsing
    pbap_client_obex_auth_parser_state_t state;
    uint8_t type;
    uint8_t len;
    uint8_t pos;
    // data
    uint8_t  authentication_options;
    uint16_t authentication_nonce[16];
} pbap_client_obex_auth_parser_t;

typedef struct pbap_client {
    // pbap client linked list
    btstack_linked_item_t item;

    // goep client linked list
    goep_client_t goep_client;

    pbap_client_state_t state;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
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
    uint32_t     property_selector;
    uint16_t     list_start_offset;
    uint16_t     max_list_count;
    uint8_t      order;
    uint8_t      search_property;
    const char * search_value;
    /* abort */
    uint8_t  abort_operation;
    /* obex parser */
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];
    /* authentication */
    pbap_client_obex_auth_parser_t obex_auth_parser;
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
    obex_srm_client_t obex_srm;
} pbap_client_t;

/* API_START */

/**
 * Setup PhoneBook Access Client
 */
void pbap_client_init(void);

/**
 * @brief Create PBAP connection to a Phone Book Server (PSE) server on a remote device.
 * If the server requires authentication, a PBAP_SUBEVENT_AUTHENTICATION_REQUEST is emitted, which
 * can be answered with pbap_authentication_password(..).
 * The status of PBAP connection establishment is reported via PBAP_SUBEVENT_CONNECTION_OPENED event,
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 *
 * This function allows for multiple parallel connections.
 *
 * @param client storage for connection state. Must stay valid until connection closes
 * @param l2cap_ertm_config
 * @param l2cap_ertm_buffer_size
 * @param l2cap_ertm_buffer
 * @param handler
 * @param addr
 * @param out_cid to use for further commands
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_MEMORY_ALLOC_FAILED if PBAP or GOEP connection already exists.
 */

uint8_t pbap_client_connect(pbap_client_t * client, l2cap_ertm_config_t *l2cap_ertm_config, uint8_t *l2cap_ertm_buffer,
                            uint16_t l2cap_ertm_buffer_size, btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid);

/**
 * @brief Create PBAP connection to a Phone Book Server (PSE) server on a remote device. 
 * If the server requires authentication, a PBAP_SUBEVENT_AUTHENTICATION_REQUEST is emitted, which
 * can be answered with pbap_authentication_password(..).
 * The status of PBAP connection establishment is reported via PBAP_SUBEVENT_CONNECTION_OPENED event, 
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 *
 * This function uses a single pbap_client_t and l2cap ertm buffer instance and can only be used for a single connection.
 * Fur multiple parallel connections, use pbap_client_connect.
 *
 * @param handler 
 * @param addr
 * @param out_cid to use for further commands
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_MEMORY_ALLOC_FAILED if PBAP or GOEP connection already exists.  
 */
uint8_t pbap_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid);

/**
 * Create SDP Record for Phonebook Access Client
 * @param service
 * @param service_record_handle
 * @param service_name
 */
void pbap_client_create_sdp_record(uint8_t *service, uint32_t service_record_handle, const char *service_name);

/**
 * @brief Provide password for OBEX Authentication after receiving PBAP_SUBEVENT_AUTHENTICATION_REQUEST.
 * The status of PBAP connection establishment is reported via PBAP_SUBEVENT_CONNECTION_OPENED event, 
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 * 
 * @param pbap_cid
 * @param password (null terminated string) - not copied, needs to stay valid until connection completed
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_authentication_password(uint16_t pbap_cid, const char * password);

/** 
 * @brief Disconnects PBAP connection with given identifier.
 * Event PBAP_SUBEVENT_CONNECTION_CLOSED indicates that PBAP connection is closed.
 *
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_disconnect(uint16_t pbap_cid);

/** 
 * @brief Set current folder on PSE. The status is reported via PBAP_SUBEVENT_OPERATION_COMPLETED event. 
 * 
 * @param pbap_cid
 * @param path - note: path is not copied
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_phonebook(uint16_t pbap_cid, const char * path);

/**
 * @brief Set vCard Selector for get/pull phonebook. No event is emitted.
 * 
 * @param pbap_cid
 * @param vcard_selector - combination of PBAP_PROPERTY_MASK_* properties
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_vcard_selector(uint16_t pbap_cid, uint32_t vcard_selector);

/**
 * @brief Set vCard Selector for get/pull phonebook. No event is emitted.
 * 
 * @param pbap_cid
 * @param vcard_selector_operator - PBAP_VCARD_SELECTOR_OPERATOR_OR (default) or PBAP_VCARD_SELECTOR_OPERATOR_AND
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_vcard_selector_operator(uint16_t pbap_cid, int vcard_selector_operator);

/**
 * @brief Set Property Selector. No event is emitted.
 * @param pbap_cid
 * @param property_selector - combination of PBAP_PROPERTY_MASK_* properties
 * @return
 */
uint8_t pbap_set_property_selector(uint16_t pbap_cid, uint32_t property_selector);

/**
 * @brief Set number of items returned by pull phonebook.
 * @param pbap_cid
 * @param max_list_count
 * @return
 */
uint8_t pbap_set_max_list_count(uint16_t pbap_cid, uint16_t max_list_count);

/**
 * @bbrief Set start offset for pull phonebook
 * @param pbap_cid
 * @param list_start_offset
 * @return
 */
uint8_t pbap_set_list_start_offset(uint16_t pbap_cid, uint16_t list_start_offset);

/**
 * @bbrief Set order for pbap_pull_vcard_listing
 * @param pbap_cid
 * @param order
 * @return
 */
uint8_t pbap_set_order(uint16_t pbap_cid, uint8_t order);

/**
 * @bbrief Set search property for pbap_pull_vcard_listing
 * @param pbap_cid
 * @param search_property
 * @return
 */
uint8_t pbap_set_search_property(uint16_t pbap_cid, uint8_t search_property);

/**
 * @bbrief Set search property for pbap_pull_vcard_listing
 * @param pbap_cid
 * @param search_value
 * @return
 */
uint8_t pbap_set_search_value(uint16_t pbap_cid, const char * search_value);

/**
 * @brief Get size of phone book from PSE. The result is reported via PBAP_SUBEVENT_PHONEBOOK_SIZE event. 
 * 
 * @param pbap_cid
 * @param path - note: path is not copied, common path 'telecom/pb.vcf'
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_get_phonebook_size(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull phone book from PSE. The result is reported via registered packet handler (see pbap_connect function),
 * with packet type set to PBAP_DATA_PACKET. Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of the phone book. 
 * 
 * @param pbap_cid
 * @param path - note: path is not copied, common path 'telecom/pb.vcf'
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_pull_phonebook(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull vCard listing. vCard data is emitted via PBAP_SUBEVENT_CARD_RESULT event. 
 * Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of vCard listing.
 *
 * @param pbap_cid
 * @param path
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_pull_vcard_listing(uint16_t pbap_cid, const char * path);

/**
 * @brief Pull vCard entry. The result is reported via callback (see pbap_connect function),
 * with packet type set to PBAP_DATA_PACKET.
 * Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of the vCard entry.
 * 
 * @param pbap_cid
 * @param path
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_pull_vcard_entry(uint16_t pbap_cid, const char * path);

/**
 * @brief Lookup contact(s) by phone number. vCard data is emitted via PBAP_SUBEVENT_CARD_RESULT event. 
 * Event PBAP_SUBEVENT_OPERATION_COMPLETED marks the end of the lookup.
 *
 * @param pbap_cid
 * @param phone_number
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_lookup_by_number(uint16_t pbap_cid, const char * phone_number);

/**
 * @brief Abort current operation. No event is emitted.
 * 
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_abort(uint16_t pbap_cid);


/**
 * @brief Set flow control mode - default is off. No event is emitted.
 * @note When enabled, pbap_next_packet needs to be called after a packet was processed to receive the next one
 *
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_set_flow_control_mode(uint16_t pbap_cid, int enable);
    
/**
 * @brief Trigger next packet from PSE when Flow Control Mode is enabled.
 * @param pbap_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t pbap_next_packet(uint16_t pbap_cid);

/**
 * @brief De-Init PBAP Client
 */
void pbap_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
