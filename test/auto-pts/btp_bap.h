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


/*
 *  btp_bap.h
 *  BTP BAP Implementation
 */

#ifndef BTP_BAP_H
#define BTP_BAP_H

#include "btstack_run_loop.h"
#include "btstack_defines.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// BAP
#define BTP_BAP_READ_SUPPORTED_COMMANDS 			0x01
#define BTP_BAP_DISCOVER 				            0x02
#define BTP_BAP_SEND 				                0x03
#define BTP_BAP_BROADCAST_SOURCE_SETUP 				0x04
#define BTP_BAP_BROADCAST_SOURCE_RELEASE 			0x05
#define BTP_BAP_BROADCAST_ADV_START 				0x06
#define BTP_BAP_BROADCAST_ADV_STOP 				    0x07
#define BTP_BAP_BROADCAST_SOURCE_START 				0x08
#define BTP_BAP_BROADCAST_SOURCE_STOP 				0x09
#define BTP_BAP_BROADCAST_SINK_SETUP 				0x0a
#define BTP_BAP_BROADCAST_SINK_RELEASE 				0x0b
#define BTP_BAP_BROADCAST_SCAN_START 				0x0c
#define BTP_BAP_BROADCAST_SCAN_STOP 				0x0d
#define BTP_BAP_BROADCAST_SINK_SYNC 				0x0e
#define BTP_BAP_BROADCAST_SINK_STOP 				0x0f
#define BTP_BAP_BROADCAST_SINK_BIS_SYNC 			0x10
#define BTP_BAP_DISCOVER_SCAN_DELEGATOR 			0x11
#define BTP_BAP_BROADCAST_ASSISTANT_SCAN_START 		0x12
#define BTP_BAP_BROADCAST_ASSISTANT_SCAN_STOP 		0x13
#define BTP_BAP_ADD_BROADCAST_SRC 				    0x14
#define BTP_BAP_REMOVE_BROADCAST_SRC 				0x15
#define BTP_BAP_MODIFY_BROADCAST_SRC 				0x16
#define BTP_BAP_SET_BROADCAST_CODE 				    0x17
#define BTP_BAP_SEND_PAST 				            0x18
#define BTP_BAP_EV_DISCOVERY_COMPLETED 				0x80
#define BTP_BAP_EV_CODEC_CAP_FOUND 				    0x81
#define BTP_BAP_EV_ASE_FOUND 				        0x82
#define BTP_BAP_EV_STREAM_RECEIVED 				    0x83
#define BTP_BAP_EV_BAA_FOUND 				        0x84
#define BTP_BAP_EV_BIS_FOUND 				        0x85
#define BTP_BAP_EV_BIS_SYNCED 				        0x86
#define BTP_BAP_EV_BIS_STREAM_RECEIVED 				0x87
#define BTP_BAP_EV_SCAN_DELEGATOR_FOUND 			0x88
#define BTP_BAP_EV_BROADCAST_RECEIVE_STATE 			0x89
#define BTP_BAP_EV_PA_SYNC_REQ 				        0x8a

/**
 * Init BAP Service
 */
void btp_bap_init(void);

/**
 * Process BAP Operation
 */
void btp_bap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif

#endif // BTP_BAP_H
