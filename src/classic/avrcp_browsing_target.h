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

/*
 * avrcp_browsing_controller.h
 * 
 * Audio/Video Remote Control Profile Browsing
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
 * @brief Set up AVRCP Browsing Controller device.
 */
void avrcp_browsing_target_init(void);

/**
 * @brief Register callback for the AVRCP Browsing Controller client. 
 * @param callback
 */
void avrcp_browsing_target_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a Bluetooth address.
 * @param bd_addr
 * @param ertm_buffer
 * @param ertm_buffer_size
 * @param ertm_config
 * @param avrcp_browsing_cid
 * @returns status
 */
uint8_t avrcp_browsing_target_connect(bd_addr_t bd_addr, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid);

/**
 * @brief Configure incoming connection.
 * @param avrcp_browsing_cid
 * @param ertm_buffer
 * @param ertm_buffer_size
 * @param ertm_config
 * @returns status
 */
uint8_t avrcp_browsing_target_configure_incoming_connection(uint16_t avrcp_browsing_cid, uint8_t * ertm_buffer, uint32_t size, l2cap_ertm_config_t * ertm_config);

/**
 * @brief Decline incoming connection.
 * @param avrcp_browsing_cid
 * @returns status
 */
uint8_t avrcp_browsing_target_decline_incoming_connection(uint16_t avrcp_browsing_cid);


/**
 * @brief Disconnect from AVRCP target
 * @param avrcp_browsing_cid
 * @returns status
 */
uint8_t avrcp_browsing_target_disconnect(uint16_t avrcp_browsing_cid);

uint8_t avrcp_subevent_browsing_get_folder_items_response(uint16_t browsing_cid, uint16_t uid_counter, uint8_t * attr_list, uint16_t attr_list_size);
uint8_t avrcp_subevent_browsing_get_total_num_items_response(uint16_t avrcp_browsing_cid, uint16_t uid_counter, uint32_t total_num_items);
/* API_END */

#if defined __cplusplus
}
#endif

#endif // AVRCP_BROWSING_TARGET_H
