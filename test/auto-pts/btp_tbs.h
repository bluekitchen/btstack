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

#ifndef BTP_TBS_H
#define BTP_TBS_H

#include <stdint.h>
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

#ifndef BT_ADDR_LE_T
#define BT_ADDR_LE_T
typedef struct {
    uint8_t address_type;
    bd_addr_t address;
} bt_addr_le_t;
#endif

#pragma pack(1)
#define __packed


/* TBS commands */
#define BTP_TBS_READ_SUPPORTED_COMMANDS		0x01
struct btp_tbs_read_supported_commands_rp {
	uint8_t data[0];
} __packed;

#define BTP_TBS_REMOTE_INCOMING			0x02
struct btp_tbs_remote_incoming_cmd {
	uint8_t index;
	uint8_t recv_len;
	uint8_t caller_len;
	uint8_t fn_len;
	uint8_t data_len;
	uint8_t data[0];
} __packed;

#define BTP_TBS_HOLD				0x03
struct btp_tbs_hold_cmd {
	uint8_t index;
} __packed;

#define BTP_TBS_SET_BEARER_NAME			0x04
struct btp_tbs_set_bearer_name_cmd {
	uint8_t index;
	uint8_t name_len;
	uint8_t name[0];
} __packed;

#define BTP_TBS_SET_TECHNOLOGY			0x05
struct btp_tbs_set_technology_cmd {
	uint8_t index;
	uint8_t tech;
} __packed;

#define BTP_TBS_SET_URI_SCHEME			0x06
struct btp_tbs_set_uri_schemes_list_cmd {
	uint8_t index;
	uint8_t uri_len;
	uint8_t uri_count;
	uint8_t uri_list[0];
} __packed;

#define BTP_TBS_SET_STATUS_FLAGS		0x07
struct btp_tbs_set_status_flags_cmd {
	uint8_t index;
	uint16_t flags;
} __packed;

#define BTP_TBS_REMOTE_HOLD			0x08
struct btp_tbs_remote_hold_cmd {
	uint8_t index;
} __packed;

#define BTP_TBS_ORIGINATE			0x09
struct btp_tbs_originate_cmd {
	uint8_t index;
	uint8_t uri_len;
	uint8_t uri[0];
} __packed;

#define BTP_TBS_SET_SIGNAL_STRENGTH		0x0a
struct btp_tbs_set_signal_strength_cmd {
	uint8_t index;
	uint8_t strength;
} __packed;

#define BTP_TBS_TERMINATE				0x0b
struct btp_tbs_terminate_cmd {
    uint8_t index;
} __packed;

#pragma options align=reset

/**
* Init TMAP Service
*/
void btp_tbs_init(void);

/**
 * Process TMAP Operation
 */
void btp_tbs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_TBS_H
