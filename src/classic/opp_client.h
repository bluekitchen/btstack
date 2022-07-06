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

/**
 * @title OPP Client
 *
 */

#ifndef OPP_CLIENT_H
#define OPP_CLIENT_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include <stdint.h>

/* API_START */

/**
 * Setup Object Push Client
 */
void opp_client_init(void);

/**
 * @brief Create OPP connection to a Object Push Server on a remote device.
 * The status of OPP connection establishment is reported via OPP_SUBEVENT_CONNECTION_OPENED event,
 * i.e. on success status field is set to ERROR_CODE_SUCCESS.
 *
 * @param handler
 * @param addr
 * @param out_cid to use for further commands
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_MEMORY_ALLOC_FAILED if OPP or GOEP connection already exists.
 */
uint8_t opp_client_connect(btstack_packet_handler_t handler, bd_addr_t addr, uint16_t * out_cid);

/**
 * @brief Disconnects OPP connection with given identifier.
 * Event OPP_SUBEVENT_CONNECTION_CLOSED indicates that OPP connection is closed.
 *
 * @param opp_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t opp_client_disconnect(uint16_t opp_cid);

/**
 * @brief Pull default object from server. The result is reported via registered packet handler (see opp_connect function),
 * with packet type set to OPP_DATA_PACKET. Event OPP_SUBEVENT_OPERATION_COMPLETED marks the end of the default object.
 *_
 * @param opp_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */

uint8_t opp_client_pull_default_object(uint16_t opp_cid);

/**
 * @brief Push object to server.
 * Event OPP_SUBEVENT_OPERATION_COMPLETED marks the end of the transfer.
 *_
 * @param opp_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */

uint8_t opp_client_push_object(uint16_t        opp_cid,
                               const char     *name,
                               const char     *type,
                               const uint8_t  *data,
                               uint32_t        size);

/**
 * @brief Abort current operation. No event is emitted.
 * 
 * @param opp_cid
 * @return status ERROR_CODE_SUCCESS on success, otherwise BTSTACK_BUSY if in a wrong state.
 */
uint8_t opp_client_abort(uint16_t opp_cid);


/**
 * @brief De-Init OPP Client
 */
void opp_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif
#endif
