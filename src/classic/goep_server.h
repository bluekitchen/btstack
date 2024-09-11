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

#ifndef GOEP_SERVER_H
#define GOEP_SERVER_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include "gap.h"
#include <stdint.h>

#ifdef ENABLE_GOEP_L2CAP
#ifndef GOEP_SERVER_ERTM_BUFFER
#define GOEP_SERVER_ERTM_BUFFER 2000
#endif
#endif

/* API_START */

typedef enum {
    GOEP_SERVER_IDLE,
    GOEP_SERVER_W4_ACCEPT_REJECT,
    GOEP_SERVER_W4_CONNECTED,
    GOEP_SERVER_CONNECTED,
    GOEP_SERVER_OUTGOING_BUFFER_RESERVED,
} goep_server_state_t;

typedef enum {
    GOEP_CONNECTION_RFCOMM = 0,
    GOEP_CONNECTION_L2CAP,
} goep_connection_type_t;

typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;

    btstack_packet_handler_t callback;
    uint8_t  rfcomm_channel;
    uint16_t l2cap_psm;
} goep_server_service_t;

// type
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;

    uint16_t goep_cid;
    goep_server_state_t     state;
    goep_connection_type_t  type;
    uint16_t bearer_cid;
    uint16_t bearer_mtu;
    btstack_packet_handler_t callback;
    uint32_t obex_connection_id;

#ifdef ENABLE_GOEP_L2CAP
    uint8_t ertm_buffer[GOEP_SERVER_ERTM_BUFFER];
#endif

} goep_server_connection_t;

/**
 * Init GOEP Server
 */
void goep_server_init(void);


/**
 * Register OBEX Service
 * @param packet_handler
 * @param rfcomm_channel
 * @param rfcomm_max_frame_size
 * @param l2cap_psm
 * @param l2cap_mtu
 * @param security_level
 * @return status
 */
uint8_t goep_server_register_service(btstack_packet_handler_t packet_handler, uint8_t rfcomm_channel, uint16_t rfcomm_max_frame_size,
                uint16_t l2cap_psm, uint16_t l2cap_mtu, gap_security_level_t security_level);

/**
 * @brief Accepts incoming GOEP connection.
 * @param goep_cid
 * @return status
 */
uint8_t goep_server_accept_connection(uint16_t goep_cid);

/**
 * @brief Deny incoming GOEP connection.
 * @param goep_cid
 * @return status
 */
uint8_t goep_server_decline_connection(uint16_t goep_cid);

/**
 * @brief Get max size of GOEP message
 * @param goep_cid
 * @return size in bytes or 0
 */
uint16_t goep_server_response_get_max_message_size(uint16_t goep_cid);

/**
 * Request GOEP_SUBEVENT_CAN_SEND_NOW
 * @param goep_cid
 * @return status
 */
uint8_t goep_server_request_can_send_now(uint16_t goep_cid);

/**
 * @brief Start Connect response
 * @note Reserves outgoing packet buffer
 * @note Must only be called after a 'can send now' check or event
 * @note Asserts if packet buffer is already reserved
 * @param goep_cid
 * @param obex_version_number
 * @param flags
 * @param maximum_obex_packet_length
 * @return status
 */
uint8_t goep_server_response_create_connect(uint16_t goep_cid, uint8_t obex_version_number, uint8_t flags, uint16_t maximum_obex_packet_length);

/**
 * @brief Start General response
 * @note Reserves outgoing packet buffer
 * @note Must only be called after a 'can send now' check or event
 * @note Asserts if packet buffer is already reserved
 * @note response code is set by goep_server_execute
 * @param goep_cid
 * @param opcode
 * @return status
 */
uint8_t goep_server_response_create_general(uint16_t goep_cid);

/**
 * @brief Add who header to current response
 * @param goep_cid
 * @param who 16 bytes
 * @return status
 */
uint8_t goep_server_header_add_who(uint16_t goep_cid, const uint8_t * who);

/**
 * @brief Add end_of_body header to current response
 * @param goep_cid
 * @param end_of_body
 * @param length
 * @return status
 */
uint8_t goep_server_header_add_end_of_body(uint16_t goep_cid, const uint8_t * end_of_body, uint16_t length);

/**
 * @brief Add SRM ENABLE header to current response
 * @param goep_cid
 * @return
 */
uint8_t goep_server_header_add_srm_enable(uint16_t goep_cid);

/**
 * @brief Add SRM ENABLE_WAIT header to current response
 * @param goep_cid
 * @return
 */
uint8_t goep_server_header_add_srm_enable_wait(uint16_t goep_cid);

/**
 * @brief Add name header to current response
 * @param goep_cid
 * @param name \0 terminated string
 * @return
 */
uint8_t goep_server_header_add_name(uint16_t goep_cid, const char *name);

/**
 * @brief Add type header to current response
 * @param goep_cid
 * @param name \0 terminated string
 * @return
 */
uint8_t goep_server_header_add_type(uint16_t goep_cid, const char *type);

/**
 * @brief Add application parameters header to current request
 * @param goep_cid
 * @param data
 * @param length
 * @return
 */
uint8_t goep_server_header_add_application_parameters(uint16_t goep_cid, const uint8_t * data, uint16_t length);

/**
 * @brief Send prepared response
 * @param goep_cid
 * @param response_code
 * @return status
 */
uint8_t goep_server_execute(uint16_t goep_cid, uint8_t response_code);

/**
 * @brief Disconnect client
 * @param goep_cid
 * @return status
 */
uint8_t goep_server_disconnect(uint16_t goep_cid);

/**
 * De-Init
 */
void geop_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
