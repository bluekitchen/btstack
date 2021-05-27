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

/**
 * @title AVRCP Browsing
 *
 */

#ifndef AVRCP_BROWSING_H
#define AVRCP_BROWSING_H

#include <stdint.h>
#include "avrcp.h"
#include "btstack_run_loop.h"
#include "btstack_linked_list.h"
#include "l2cap.h"

#if defined __cplusplus
extern "C" {
#endif

void avrcp_browsing_register_controller_packet_handler(btstack_packet_handler_t callback);
void avrcp_browsing_register_target_packet_handler(btstack_packet_handler_t callback);
void avrcp_browsing_request_can_send_now(avrcp_browsing_connection_t * connection, uint16_t l2cap_cid);

/* API_START */
/**
 * @brief Set up AVRCP Browsing service
 */
void avrcp_browsing_init(void);

/**
 * @brief Register callback for the AVRCP Browsing Controller client. 
 * @param callback
 */
void avrcp_browsing_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief   Connect to AVRCP Browsing service on a remote device, emits AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED with status
 * @param   remote_addr
 * @param   ertm_buffer
 * @param   ertm_buffer_size
 * @param   ertm_config
 * @param   avrcp_browsing_cid  outgoing parameter, valid if status == ERROR_CODE_SUCCESS
 * @returns status     
 */
uint8_t avrcp_browsing_connect(bd_addr_t remote_addr, uint8_t * ertm_buffer, uint32_t ertm_buffer_size, l2cap_ertm_config_t * ertm_config, uint16_t * avrcp_browsing_cid);

/**
 * @brief Configure incoming connection for Browsing Service.
 * @param avrcp_browsing_cid
 * @param ertm_buffer
 * @param ertm_buffer_size
 * @param ertm_config
 * @returns status
 */
uint8_t avrcp_browsing_configure_incoming_connection(uint16_t avrcp_browsing_cid, uint8_t * ertm_buffer, uint32_t ertm_buffer_size, l2cap_ertm_config_t * ertm_config);

/**
 * @brief Decline incoming connection Browsing Service.
 * @param avrcp_browsing_cid
 * @returns status
 */
uint8_t avrcp_browsing_decline_incoming_connection(uint16_t avrcp_browsing_cid);

/**
 * @brief   Disconnect from AVRCP Browsing service
 * @param   avrcp_browsing_cid
 * @returns status
 */
uint8_t avrcp_browsing_disconnect(uint16_t avrcp_browsing_cid);

/**
 * @brief De-Init AVRCP Browsing
 */
void avrcp_browsing_deinit(void);

/* API_END */


#if defined __cplusplus
}
#endif

#endif // AVRCP_BROWSING_H
