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

#ifndef MAP_CLIENT_H
#define MAP_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include "map.h"
#include <stdint.h>

/* API_START */

/**
 *
 */
void map_client_init(void);

/**
 * @brief Create MAP Client connection.
 * @param handler 
 * @param addr
 * @param out_cid to use for further commands
 * @result status
*/
uint8_t map_client_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid);

/** 
 * @brief Disconnects MAP connection with given identifier.
 * @param map_cid
 * @return status
 */
uint8_t map_client_disconnect(uint16_t map_cid);

/** 
 * @brief Get list of folders.
 * @param map_cid
 * @return status
 */
uint8_t map_client_get_folder_listing(uint16_t map_cid);

/** 
 * @brief Set current folder
 * @param map_cid
 * @return status
 */
uint8_t map_client_set_path(uint16_t map_cid, const char * path);

/** 
 * @brief Get list of messages for particular folder.
 * @param map_cid
 * @param folder_name
 * @return status
 */
uint8_t map_client_get_message_listing_for_folder(uint16_t map_cid, const char * folder_name);

/** 
 * @brief Get message with particular handle.
 * @param map_cid
 * @param map_message_handle
 * @param with_attachment
 * @return status
 */
uint8_t map_client_get_message_with_handle(uint16_t map_cid, const map_message_handle_t map_message_handle, uint8_t with_attachment);

/** 
 * @brief Enable notifications.
 * @param map_cid
 * @return status
 */
uint8_t map_client_enable_notifications(uint16_t map_cid);

/** 
 * @brief Disable notifications.
 * @param map_cid
 * @return status
 */
uint8_t map_client_disable_notifications(uint16_t map_cid);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
