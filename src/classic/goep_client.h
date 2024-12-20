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
 * @title GOEP Client 
 *
 * Communicate with remote OBEX server - General Object Exchange
 *
 */

#ifndef GOEP_CLIENT_H
#define GOEP_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif
 
#include <stdlib.h>
#include <string.h>

#include "btstack_defines.h"
#include "l2cap.h"

typedef enum {
    GOEP_CLIENT_INIT,
    GOEP_CLIENT_W4_SDP,
    GOEP_CLIENT_CONNECTED,
} goep_client_state_t;

/* API_START */

typedef struct {
    btstack_linked_item_t item;

    uint16_t         cid;

    goep_client_state_t     state;
    bd_addr_t        bd_addr;
    uint16_t         uuid;
    hci_con_handle_t con_handle;
    uint8_t          incoming;

    btstack_packet_handler_t client_handler;
    btstack_context_callback_registration_t sdp_query_request;

    uint8_t          rfcomm_port;
    uint16_t         l2cap_psm;
    uint16_t         bearer_cid;
    uint16_t         bearer_mtu;

    uint16_t         record_index;

    // cached higher layer information PBAP + MAP
    uint32_t         profile_supported_features;
    uint8_t          map_mas_instance_id;
    uint8_t          map_supported_message_types;
    uint16_t         map_version;

    // needed to select one of multiple MAS Instances
    struct {
        uint32_t supported_features;
        uint8_t  instance_id;
        uint8_t  supported_message_types;
        uint8_t  rfcomm_port;
        uint16_t version;
#ifdef ENABLE_GOEP_L2CAP
        uint16_t l2cap_psm;
#endif
    } mas_info;

    uint8_t          obex_opcode;
    uint32_t         obex_connection_id;
    int              obex_connection_id_set;

#ifdef ENABLE_GOEP_L2CAP
    l2cap_ertm_config_t ertm_config;
    uint16_t            ertm_buffer_size;
    uint8_t           * ertm_buffer;
#endif
} goep_client_t;


// remote does not expose PBAP features in SDP record
#define PBAP_FEATURES_NOT_PRESENT ((uint32_t) -1)
#define MAP_FEATURES_NOT_PRESENT ((uint32_t) -1)
#define PROFILE_FEATURES_NOT_PRESENT ((uint32_t) -1)

/**
 * Setup GOEP Client
 */
void goep_client_init(void);

/**
 * @brief Connect to a GEOP server with specified UUID on a remote device.
 * @param goep_client
 * @param l2cap_ertm_config
 * @param l2cap_ertm_buffer_size
 * @param l2cap_ertm_buffer
 * @param handler
 * @param addr
 * @param uuid
 * @param instance_id e.g. used to connect to different MAP Access Servers, default: 0
 * @param out_cid
 * @return
 */
uint8_t
goep_client_connect(goep_client_t *goep_client, l2cap_ertm_config_t *l2cap_ertm_config, uint8_t *l2cap_ertm_buffer,
                    uint16_t l2cap_ertm_buffer_size, btstack_packet_handler_t handler, bd_addr_t addr, uint16_t uuid,
                    uint8_t instance_id, uint16_t *out_cid);

/**
 * @brief Connect to a GEOP server over L2CAP with specified PSM on a remote device.
 * @note In contrast to goep_client_connect which searches for a OBEX service with a given UUID, this
 *       function only supports GOEP v2.0 (L2CAP) to a specified L2CAP PSM
 * @param goep_client
 * @param l2cap_ertm_config
 * @param l2cap_ertm_buffer_size
 * @param l2cap_ertm_buffer
 * @param handler
 * @param addr
 * @param l2cap_psm
 * @param out_cid
 * @return
 */
uint8_t
goep_client_connect_l2cap(goep_client_t *goep_client, l2cap_ertm_config_t *l2cap_ertm_config, uint8_t *l2cap_ertm_buffer,
                    uint16_t l2cap_ertm_buffer_size, btstack_packet_handler_t handler, bd_addr_t addr, uint16_t l2cap_psm,
                    uint16_t *out_cid);

/** 
 * @brief Disconnects GOEP connection with given identifier.
 * @param goep_cid
 * @return status
 */
uint8_t goep_client_disconnect(uint16_t goep_cid);

/** 
 * @brief Request emission of GOEP_SUBEVENT_CAN_SEND_NOW as soon as possible
 * @note GOEP_SUBEVENT_CAN_SEND_NOW might be emitted during call to this function
 *       so packet handler should be ready to handle it
 * @param goep_cid
 */
void goep_client_request_can_send_now(uint16_t goep_cid);

/**
 * @brief Get Opcode from last created request, needed for parsing of OBEX response packet
 * @param goep_cid
 * @return opcode
 */
uint8_t goep_client_get_request_opcode(uint16_t goep_cid);

/**
 * @brief Get PBAP Supported Features found in SDP record during connect
 */
uint32_t goep_client_get_pbap_supported_features(uint16_t goep_cid);

/**
 * @brief Get MAP Supported Features found in SDP record during connect
 */
uint32_t goep_client_get_map_supported_features(uint16_t goep_cid);

/**
 * @brief Get MAP MAS Instance ID found in SDP record during connect
 */
uint8_t goep_client_get_map_mas_instance_id(uint16_t goep_cid);

/**
 * @brief Get MAP MAS Supported Message Types found in SDP record during connect
 */
uint8_t goep_client_get_map_supported_message_types(uint16_t goep_cid);

/**
 * @brief Check if GOEP 2.0 or higher features can be used
 * @return true if GOEP Version 2.0 or higher
 */
bool goep_client_version_20_or_higher(uint16_t goep_cid);

/**
 * @brief Set Connection ID used for newly created requests
 * @param goep_cid
 */
void goep_client_set_connection_id(uint16_t goep_cid, uint32_t connection_id);

/**
 * @brief Start Connect request
 * @param goep_cid
 * @param obex_version_number
 * @param flags
 * @param maximum_obex_packet_length
 */
void goep_client_request_create_connect(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length);

/**
 * @brief Start Disconnect request
 * @param goep_cid
 */
void goep_client_request_create_disconnect(uint16_t goep_cid);

/**
 * @brief Create Get request
 * @param goep_cid
 */
void goep_client_request_create_get(uint16_t goep_cid);

/**
 * @brief Create Abort request
 * @param goep_cid
 */
void goep_client_request_create_abort(uint16_t goep_cid);

/**
 * @brief Start Set Path request
 * @param goep_cid
 */
void goep_client_request_create_set_path(uint16_t goep_cid, uint8_t flags);

/**
 * @brief Create Put request
 * @param goep_cid
 */
void goep_client_request_create_put(uint16_t goep_cid);

/**
 * @brief Get max size of body data that can be added to current response with goep_client_body_add_static
 * @param goep_cid
 * @param data
 * @param length
 * @return size in bytes or 0
 */
uint16_t goep_client_request_get_max_body_size(uint16_t goep_cid);

/**
 * @brief Add SRM Enable
 * @param goep_cid
 */
void goep_client_header_add_srm_enable(uint16_t goep_cid);

/**
 * @brief Add SRMP Waiting
 * @param goep_cid
 */
void goep_client_header_add_srmp_waiting(uint16_t goep_cid);

/**
 * @brief Add header with single byte value (8 bit)
 * @param goep_cid
 * @param header_type
 * @param value
 */
void goep_client_header_add_byte(uint16_t goep_cid, uint8_t header_type, uint8_t value);

/**
 * @brief Add header with word value (32 bit)
 * @param goep_cid
 * @param header_type
 * @param value
 */
void goep_client_header_add_word(uint16_t goep_cid, uint8_t header_type, uint32_t value);

/**
 * @brief Add header with variable size
 * @param goep_cid
 * @param header_type
 * @param header_data
 * @param header_data_length
 */
void goep_client_header_add_variable(uint16_t goep_cid, uint8_t header_type, const uint8_t * header_data, uint16_t header_data_length);

/**
 * @brief Add name header to current request
 * @param goep_cid
 * @param name
 */
void goep_client_header_add_name(uint16_t goep_cid, const char * name);

/**
 * @brief Add name header to current request
 * @param goep_cid
 * @param name
 * @param name_len
 */
void goep_client_header_add_name_prefix(uint16_t goep_cid, const char * name, uint16_t name_len);

/**
 * @brief Add string encoded as unicode to current request
 * @param goep_cid
 * @param name
 * @param name_len
 */
void goep_client_header_add_unicode_prefix(uint16_t goep_cid, uint8_t header_id, const char * name, uint16_t name_len);

/**
 * @brief Add target header to current request
 * @param goep_cid
 * @param target
 * @param length of target
 */
void goep_client_header_add_target(uint16_t goep_cid, const uint8_t * target, uint16_t length);

/**
 * @brief Add type header to current request
 * @param goep_cid
 * @param type
 */
void goep_client_header_add_type(uint16_t goep_cid, const char * type);

/**
 * @brief Add count header to current request
 * @param goep_cid
 * @param count
 */
void goep_client_header_add_count(uint16_t goep_cid, uint32_t count);

/**
 * @brief Add length header to current request
 * @param goep_cid
 * @param length
 */
void goep_client_header_add_length(uint16_t goep_cid, uint32_t length);

/**
 * @brief Add application parameters header to current request
 * @param goep_cid
 * @param data 
 * @param lenght of application parameters
 */
void goep_client_header_add_application_parameters(uint16_t goep_cid, const uint8_t * data, uint16_t length);

/**
 * @brief Add application parameters header to current request
 * @param goep_cid
 * @param data
 * @param lenght of challenge response
 */
void goep_client_header_add_challenge_response(uint16_t goep_cid, const uint8_t * data, uint16_t length);

/**
 * @brief Add body
 * @param goep_cid
 * @param data
 * @param lenght 
 */
void goep_client_body_add_static(uint16_t goep_cid, const uint8_t * data, uint32_t length);

/**
 * @brief Query remaining buffer size
 * @param goep_cid
 * @return size
 */
uint16_t goep_client_body_get_outgoing_buffer_len(uint16_t goep_cid);

/**
 * @brief Add body
 * @param goep_cid
 * @param data
 * @param length
 * @param ret_length
 */
void goep_client_body_fillup_static(uint16_t goep_cid, const uint8_t * data, uint32_t length, uint32_t * ret_length);

/**
 * @brief Execute prepared request
 * @param goep_cid
 * @param daa 
 */
int goep_client_execute(uint16_t goep_cid);

/**
 * @brief Execute prepared request with final bit
 * @param goep_cid
 * @param final
 */
int goep_client_execute_with_final_bit(uint16_t goep_cid, bool final);

/**
 * @brief De-Init GOEP Client
 */
void goep_client_deinit(void);

/* API_END */

// int goep_client_body_add_dynamic(uint16_t goep_cid, uint32_t length, void (*data_callback)(uint32_t offset, uint8_t * buffer, uint32_t len));

#if defined __cplusplus
}
#endif
#endif

