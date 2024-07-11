/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 *  @brief TODO
 */

#ifndef MAP_NOTIFICATION_CLIENT_H
#define MAP_NOTIFICATION_CLIENT_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_defines.h"
#include "goep_client.h"
#include "obex_parser.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    MNC_STATE_INIT = 0,
    MNC_STATE_W4_GOEP_CONNECTION,
    MNC_STATE_W2_SEND_CONNECT_REQUEST,
    MNC_STATE_W4_CONNECT_RESPONSE,
    MNC_STATE_CONNECT_RESPONSE_RECEIVED,
    MNC_STATE_CONNECTED,

    MNC_STATE_W2_SEND_DISCONNECT_REQUEST,
    MNC_STATE_W4_DISCONNECT_RESPONSE,

    MNC_STATE_W2_PUT_SEND_EVENT,
    MNC_STATE_W4_PUT_SEND_EVENT,
} map_notification_client_state_t;

typedef enum {
    SRM_DISABLED,
    SRM_W4_CONFIRM,
    SRM_ENABLED_BUT_WAITING,
    SRM_ENABLED
} map_access_client_srm_state_t;

typedef struct {
    uint8_t srm_value;
    uint8_t srmp_value;
} map_access_client_obex_srm_t;

typedef struct {
    // opaque storage for goep_client
    goep_client_t goep_client;

    // lookup via linked list and cid
    btstack_linked_item_t item;
    uint16_t cid;

    map_notification_client_state_t state;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint8_t   incoming;
    btstack_packet_handler_t client_handler;

    int request_number;

    /* obex parser */
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];
    /* srm */
    map_access_client_obex_srm_t obex_srm;
    map_access_client_srm_state_t srm_state;

    uint8_t* body_buf;
    size_t  body_buf_len;

    uint8_t app_params[20];
    size_t app_params_len;

} map_notification_client_t;

/**
* @brief returns map_notification_client_t* for mnc_cid
*/
map_notification_client_t* map_notification_client_for_mnc_cid(uint16_t mnc_cid);

uint8_t map_notification_client_send_event(uint16_t mnc_cid, uint8_t MASInstanceID, uint8_t* body_buf, size_t  body_buf_len);

/**
*
*/
void map_notification_client_init(void);

/**
 * @brief Create MAP Notification Client connection.
 * @param map_notification_client_t to store state
 * @param l2cap_ertm_config
 * @param l2cap_ertm_buffer_size
 * @param l2cap_ertm_buffer
 * @param handler
 * @param addr
 * @param instance_id of MAP Notification Server, use 0 for default instance
 * @param out_cid to use for further commands
 * @result status
*/
uint8_t map_notification_client_connect(map_notification_client_t *map_notification_client, l2cap_ertm_config_t *l2cap_ertm_config,
                                  uint16_t l2cap_ertm_buffer_size, uint8_t *l2cap_ertm_buffer,
                                  btstack_packet_handler_t handler, bd_addr_t addr, uint8_t instance_id,
                                  uint16_t *out_cid);

/**
 * @brief Disconnects MAP connection with given identifier.
 * @param mnc_cid
 * @return status
 */
uint8_t map_notification_client_disconnect(uint16_t mnc_cid);

#if defined __cplusplus
}
#endif
#endif // MAP_NOTIFICATION_CLIENT_H
