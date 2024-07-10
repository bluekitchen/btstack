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
 *  btp_ccp.h
 *  BTP CCP Implementation
 */

#ifndef BTP_CCP_H
#define BTP_CCP_H

#include "btstack_run_loop.h"
#include "btstack_defines.h"
#include "bluetooth.h"

#include <stdint.h>

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
#define __packed

#pragma pack(1)

/* CCP commands */
#define BTP_CCP_READ_SUPPORTED_COMMANDS	0x01
struct btp_ccp_read_supported_commands_rp {
	uint8_t data[0];
} __packed;

#define BTP_CCP_DISCOVER_TBS		0x02
struct btp_ccp_discover_tbs_cmd {
	bt_addr_le_t address;
} __packed;

#define BTP_CCP_ACCEPT_CALL		0x03
struct btp_ccp_accept_call_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t call_id;
} __packed;

#define BTP_CCP_TERMINATE_CALL		0x04
struct btp_ccp_terminate_call_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t call_id;
} __packed;

#define BTP_CCP_ORIGINATE_CALL		0x05
struct btp_ccp_originate_call_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t uri_len;
	char    uri[0];
} __packed;

#define BTP_CCP_READ_CALL_STATE		0x06
struct btp_ccp_read_call_state_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_BEARER_NAME	0x07
struct btp_ccp_read_bearer_name_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_BEARER_UCI		0x08
struct btp_ccp_read_bearer_uci_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_BEARER_TECH	0x09
struct btp_ccp_read_bearer_technology_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_URI_LIST		0x0a
struct btp_ccp_read_uri_list_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_SIGNAL_STRENGTH	0x0b
struct btp_ccp_read_signal_strength_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_SIGNAL_INTERVAL	0x0c
struct btp_ccp_read_signal_interval_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_CURRENT_CALLS	0x0d
struct btp_ccp_read_current_calls_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_CCID		0x0e
struct btp_ccp_read_ccid_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_CALL_URI		0x0f
struct btp_ccp_read_call_uri_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_STATUS_FLAGS	0x10
struct btp_ccp_read_status_flags_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_OPTIONAL_OPCODES	0x11
struct btp_ccp_read_optional_opcodes_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_FRIENDLY_NAME	0x12
struct btp_ccp_read_friendly_name_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_READ_REMOTE_URI		0x13
struct btp_ccp_read_remote_uri_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
} __packed;

#define BTP_CCP_SET_SIGNAL_INTERVAL	0x14
struct btp_ccp_set_signal_interval_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t interval;
} __packed;

#define BTP_CCP_HOLD_CALL		0x15
struct btp_ccp_hold_call_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t call_id;
} __packed;

#define BTP_CCP_RETRIEVE_CALL		0x16
struct btp_ccp_retrieve_call_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t call_id;
} __packed;

#define BTP_CCP_JOIN_CALLS		0x17
struct btp_ccp_join_calls_cmd {
	bt_addr_le_t address;
	uint8_t inst_index;
	uint8_t count;
	uint8_t call_index[];
} __packed;

/* CCP events */
#define BTP_CCP_EV_DISCOVERED		0x80
struct btp_ccp_discovered_ev {
	int     status;
	uint8_t tbs_count;
	bool	gtbs_found;
} __packed;

#define BTP_CCP_EV_CALL_STATES		0x81
#if 0
struct btp_ccp_call_states_ev {
	int     status;
	uint8_t inst_index;
	uint8_t call_count;
	struct bt_tbs_client_call_state call_states[0];
} __packed;
#endif

#define BTP_CCP_EV_CHRC_HANDLES		0x82
struct btp_ccp_chrc_handles_ev {
	uint16_t provider_name;
	uint16_t bearer_uci;
	uint16_t bearer_technology;
	uint16_t uri_list;
	uint16_t signal_strength;
	uint16_t signal_interval;
	uint16_t current_calls;
	uint16_t ccid;
	uint16_t status_flags;
	uint16_t bearer_uri;
	uint16_t call_state;
	uint16_t control_point;
	uint16_t optional_opcodes;
	uint16_t termination_reason;
	uint16_t incoming_call;
	uint16_t friendly_name;
};

#define BTP_CCP_EV_CHRC_VAL		0x83
struct btp_ccp_chrc_val_ev {
	bt_addr_le_t address;
	uint8_t status;
	uint8_t inst_index;
	uint8_t value;
};

#define BTP_CCP_EV_CHRC_STR		0x84
struct btp_ccp_chrc_str_ev {
	bt_addr_le_t address;
	uint8_t status;
	uint8_t inst_index;
	uint8_t data_len;
	char data[0];
} __packed;

#define BTP_CCP_EV_CP			0x85
struct btp_ccp_cp_ev {
	bt_addr_le_t address;
	uint8_t status;
} __packed;

#define BTP_CCP_EV_CURRENT_CALLS	0x86
struct btp_ccp_current_calls_ev {
	bt_addr_le_t address;
	uint8_t status;
} __packed;

#pragma options align=reset

/**
 * Init CCP Service
 */
void btp_ccp_init(void);

/**
 * Process CCP Operation
 */
void btp_ccp_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_ASCS_H
