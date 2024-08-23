/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#ifndef MAP_ACCESS_CLIENT_H
#define MAP_ACCESS_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "bluetooth.h"
#include "btstack_config.h"
#include "btstack_defines.h"
#include "classic/map.h"
#include "classic/map_util.h"
#include "classic/obex_parser.h"
#include "classic/obex_srm_client.h"

#include <stdint.h>

/* API_START */


typedef enum {
    MAP_INIT = 0,
    MAP_W4_GOEP_CONNECTION,
    MAP_W2_SEND_CONNECT_REQUEST,
    MAP_W4_CONNECT_RESPONSE,
    MAP_CONNECT_RESPONSE_RECEIVED,
    MAP_CONNECTED,

    MAP_W2_SET_PATH_ROOT,
    MAP_W4_SET_PATH_ROOT_COMPLETE,
    MAP_W2_SET_PATH_ELEMENT,
    MAP_W4_SET_PATH_ELEMENT_COMPLETE,

    MAP_W2_SEND_GET_FOLDERS,
    MAP_W4_FOLDERS,
    MAP_W2_SEND_GET_MESSAGES_FOR_FOLDER,
    MAP_W4_MESSAGES_IN_FOLDER,
    MAP_W2_SEND_GET_MESSAGE_WITH_HANDLE,
    MAP_W4_MESSAGE,
    MAP_W2_SEND_GET_CONVERSATION_LISTING,
    MAP_W4_CONVERSATION_LISTING,
    MAP_W2_SEND_SET_MESSAGE_STATUS,
    MAP_W4_SET_MESSAGE_STATUS,
    MAP_W2_SET_NOTIFICATION,
    MAP_W4_SET_NOTIFICATION,
    MAP_W2_SET_NOTIFICATION_FILTER,
    MAP_W4_SET_NOTIFICATION_FILTER,

    MAP_W2_SEND_UPDATE_INBOX,
    MAP_W4_UPDATE_INBOX,

    MAP_W2_SEND_GET_MAS_INSTANCE_INFO,
    MAP_W4_MAS_INSTANCE_INFO,

    MAP_W2_SEND_DISCONNECT_REQUEST,
    MAP_W4_DISCONNECT_RESPONSE,
} map_access_client_state_t;

typedef struct {
    // map access client linked list
    btstack_linked_item_t item;

    // goep client linked list
    goep_client_t goep_client;

    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint16_t cid;
    uint8_t   incoming;
    btstack_packet_handler_t client_handler;
    map_access_client_state_t state;

    int request_number;
    /* obex parser */
    bool obex_parser_waiting_for_response;
    obex_parser_t obex_parser;
    uint8_t obex_header_buffer[4];
    /* srm */
    obex_srm_client_t obex_srm;

    const char * folder_name;
    const char * current_folder;
    uint16_t set_path_offset;
    uint8_t  notifications_enabled;
    uint32_t notification_filter_mask;
    uint8_t  mas_instance_id;
    int max_list_count;
    int list_start_offset;

    map_message_handle_t message_handle;
    uint8_t get_message_attachment;
    int read_status;   /* for set_message_status */

    map_util_xml_parser mu_parser;
} map_access_client_t;

/**
 *
 */
void map_access_client_init(void);

/**
 * @brief Create MAP Client connection.
 * @param map_access_client to store state
 * @param l2cap_ertm_config
 * @param l2cap_ertm_buffer_size
 * @param l2cap_ertm_buffer
 * @param handler
 * @param addr
 * @param instance_id of MAP Access Server, use 0 for default instance
 * @param out_cid to use for further commands
 * @result status
*/
uint8_t map_access_client_connect(map_access_client_t *map_access_client, l2cap_ertm_config_t *l2cap_ertm_config,
                                  uint16_t l2cap_ertm_buffer_size, uint8_t *l2cap_ertm_buffer,
                                  btstack_packet_handler_t handler, bd_addr_t addr, uint8_t instance_id,
                                  uint16_t *out_cid);

/**
 * @brief Disconnects MAP connection with given identifier.
 * @param map_cid
 * @return status
 */
uint8_t map_access_client_disconnect(uint16_t map_cid);

/** 
 * @brief Request update on inbox
 * @param map_cid
 * @return status
 */
uint8_t map_access_client_update_inbox(uint16_t map_cid);

/** 
 * @brief Get list of folders.
 * @param map_cid
 * @return status
 */
uint8_t map_access_client_get_folder_listing(uint16_t map_cid);

/** 
 * @brief Set message status
 * @param map_cid
 * @param map_message_handle
 * @param read_status
 * @return status
 */
uint8_t map_access_client_set_message_status(uint16_t map_cid, const map_message_handle_t map_message_handle, int read_status);


/** 
 * @brief Set current folder
 * @param map_cid
 * @return status
 */
uint8_t map_access_client_set_path(uint16_t map_cid, const char * path);

/** 
 * @brief Get list of messages for particular folder.
 * @param map_cid
 * @param folder_name
 * @return status
 */
uint8_t map_access_client_get_message_listing_for_folder(uint16_t map_cid, const char * folder_name);

/** 
 * @brief Get list of conversations
 * @param map_cid
 * @param max_list_count
 * @param list_start_offset
 * @return status
 */
uint8_t map_access_client_get_conversation_listing(uint16_t map_cid, int max_list_count, int list_start_offset);

/** 
 * @brief Get message with particular handle.
 * @param map_cid
 * @param map_message_handle
 * @param with_attachment
 * @return status
 */
uint8_t map_access_client_get_message_with_handle(uint16_t map_cid, const map_message_handle_t map_message_handle, uint8_t with_attachment);

/** 
 * @brief Enable notifications.
 * @param map_cid
 * @return status
 */
uint8_t map_access_client_enable_notifications(uint16_t map_cid);

/** 
 * @brief Disable notifications.
 * @param map_cid
 * @return status
 */
uint8_t map_access_client_disable_notifications(uint16_t map_cid);

/**
 * @brief Configure Notification Filter
 * @param map_cid
 * @param filter_mask
 * @return status
 */
uint8_t map_access_client_set_notification_filter(uint16_t map_cid, uint32_t filter_mask);

/**
 * @brief Request information about a specific MAS instance
 * @param map_cid
 * @param mas_instance_id
 * @return status
 */
uint8_t map_access_client_get_mas_instance_info(uint16_t map_cid, uint8_t mas_instance_id);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
