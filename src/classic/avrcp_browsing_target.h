/*
 * Copyright (C) 2016 BlueKitchen GmbH
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
 * @title AVRCP Browsing Target
 *
 */

#ifndef AVRCP_BROWSING_TARGET_H
#define AVRCP_BROWSING_TARGET_H

#include <stdint.h>
#include "classic/avrcp.h"

#if defined __cplusplus
extern "C" {
#endif


/* API_START */

/**
 * @brief Set up AVRCP Browsing Target client.
 */
void avrcp_browsing_target_init(void);

/**
 * @brief Register callback for the AVRCP Browsing Target client to receive events:
 * - AVRCP_SUBEVENT_BROWSING_SEARCH
 * - AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS
 * - AVRCP_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS
 * - AVRCP_SUBEVENT_BROWSING_SET_BROWSED_PLAYER
 * - AVRCP_SUBEVENT_BROWSING_CHANGE_PATH
 * - AVRCP_SUBEVENT_BROWSING_GET_ITEM_ATTRIBUTES
 *  
 * @param callback
 */
void avrcp_browsing_target_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Accept set browsed player
 * @param browsing_cid
 * @param uid_counter
 * @param browsed_player_id
 * @param response
 * @param response_size
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_accept_set_browsed_player(uint16_t browsing_cid, uint16_t uid_counter, uint16_t browsed_player_id, uint8_t * response, uint16_t response_len);

/**
 * @brief Reject set browsed player
 * @param browsing_cid
 * @param status
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_reject_set_browsed_player(uint16_t browsing_cid, avrcp_status_code_t status);

/**
 * @brief Send answer to get folder items query on event AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS. The first byte of this event defines the scope of the query, see avrcp_browsing_scope_t.
 * @param browsing_cid
 * @param uid_counter
 * @param attr_list
 * @param attr_list_size
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_get_folder_items_response(uint16_t browsing_cid, uint16_t uid_counter, uint8_t * attr_list, uint16_t attr_list_size, uint16_t num_items);

/**
 * @brief Send answer to get total number of items query on event AVRCP_SUBEVENT_BROWSING_GET_TOTAL_NUM_ITEMS. The first byte of this event defines the scope of the query, see avrcp_browsing_scope_t.
 * @param browsing_cid
 * @param uid_counter
 * @param total_num_items
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_get_total_num_items_response(uint16_t browsing_cid, uint16_t uid_counter, uint32_t total_num_items);

/**
 * @brief Send answer to change path command on event AVRCP_SUBEVENT_BROWSING_CHANGE_PATH.
 * @param browsing_cid
 * @param status
 * @param num_items
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_change_path_response(uint16_t browsing_cid, avrcp_status_code_t status, uint32_t num_items);

/**
 * @brief Send answer to get item attribute response on event AVRCP_SUBEVENT_BROWSING_GET_ITEM_ATTRIBUTES.
 * @param browsing_cid
 * @param status
 * @param attr_list
 * @param attr_list_size
 * @param num_items
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_get_item_attributes_response(uint16_t browsing_cid, avrcp_status_code_t status, uint8_t * attr_list, uint16_t attr_list_size, uint8_t num_items);

/**
 * @brief Send answer to search query on event AVRCP_SUBEVENT_BROWSING_SEARCH.
 * @param browsing_cid
 * @param uid_counter
 * @param total_num_items
 * @return ERROR_CODE_SUCCESS if successful, otherwise:
 *       - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if unknown connection cid,
 *       - ERROR_CODE_COMMAND_DISALLOWED if client is not done with previous query. 
 */
uint8_t avrcp_browsing_target_send_search_response(uint16_t browsing_cid, avrcp_status_code_t status, uint16_t uid_counter, uint32_t num_items);

/**
 * @brief De-Init AVRCP Browsing Target
 */
void avrcp_browsing_target_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // AVRCP_BROWSING_TARGET_H
