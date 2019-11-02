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
 *  btp_socket.h
 */

#ifndef BTP_SOCKET_H
#define BTP_SOCKET_H

#include "btstack_run_loop.h"
#include "btstack_defines.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// BTP Defines
#define BTP_INDEX_NON_CONTROLLER 0xff

#define BTP_ERROR_FAIL          0x01
#define BTP_ERROR_UNKNOWN_CMD   0x02
#define BTP_ERROR_NOT_READY     0x03
#define BTP_ERROR_INVALID_INDEX 0x04

#define BTP_CORE_SERVICE        0
#define BTP_GAP_SERVICE         1
#define BTP_GATT_SERVICE        2
#define BTP_L2CAP_SERVICE       3
#define BTP_MESH_NODE_SERVICE   4

/**
 * @format 1
 * @param status
 */ 
#define BTP_OP_ERROR                        0x00

#define BTP_OP_CORE_READ_SUPPORTED_COMMANDS 0x01
#define BTP_OP_CORE_READ_SUPPORTED_SERVICES 0x02
#define BTP_OP_CORE_REGISTER                0x03
#define BTP_OP_CORE_UNREGISTER              0x04

#define BTP_EV_CORE_READY                   0x80
	
/**
 * Init socket connection module
 */
void btp_socket_init(void);

/**
 * create unix socket connection to auto-pts client 
 * @returns true if successful
 */
bool btp_socket_open_unix(const char * socket_name);

/**
 * close unix connection to auto-pts client 
 * @returns true if successful
 */
bool btp_socket_close_unix(void);

/**
 * set packet handler
 */
void btp_socket_register_packet_handler( void (*packet_handler)(uint8_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) );

/**
 * send HCI packet to single connection
 */
void btp_socket_send_packet(uint16_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t * data);

#if defined __cplusplus
}
#endif

#endif // BTP_CONNECTION_H
