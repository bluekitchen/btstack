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

#ifndef BTP_VCP_H
#define BTP_VCP_H

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

#define BTP_VCP_READ_SUPPORTED_COMMANDS		0x01
struct btp_vcp_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_VCP_VOL_CTLR_DISCOVER		0x02
struct btp_vcp_discover_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_STATE_READ		0x03
struct btp_vcp_state_read_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_FLAGS_READ		0x04
struct btp_vcp_flags_read_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_VOL_DOWN		0x05
struct btp_vcp_ctlr_vol_down_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_VOL_UP			0x06
struct btp_vcp_ctlr_vol_up_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_UNMUTE_VOL_DOWN	0x07
struct btp_vcp_ctlr_unmute_vol_down_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_UNMUTE_VOL_UP		0x08
struct btp_vcp_ctlr_unmute_vol_up_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_SET_VOL		0x09
struct btp_vcp_ctlr_set_vol_cmd {
    bt_addr_le_t address;
    uint8_t volume;
} __packed;

#define BTP_VCP_VOL_CTLR_UNMUTE			0x0a
struct btp_vcp_ctlr_unmute_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VCP_VOL_CTLR_MUTE			0x0b
struct btp_vcp_ctlr_mute_cmd {
    bt_addr_le_t address;
} __packed;

/* VCP events */
#define BTP_VCP_DISCOVERED_EV			0x80
struct btp_vcp_discovered_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    struct {
        uint16_t control_handle;
        uint16_t flag_handle;
        uint16_t state_handle;
    } vcs_handles;

    struct {
        uint16_t state_handle;
        uint16_t location_handle;
        uint16_t control_handle;
        uint16_t desc_handle;
    } vocs_handles;

    struct {
        uint16_t state_handle;
        uint16_t gain_handle;
        uint16_t type_handle;
        uint16_t status_handle;
        uint16_t control_handle;
        uint16_t desc_handle;
    } aics_handles;
} __packed;

#define BTP_VCP_STATE_EV			0x81
struct btp_vcp_state_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t volume;
    uint8_t mute;
} __packed;

#define BTP_VCP_FLAGS_EV			0x82
struct btp_vcp_volume_flags_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t flags;
} __packed;

#define BTP_VCP_PROCEDURE_EV			0x83
struct btp_vcp_procedure_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t opcode;
} __packed;

#pragma options align=reset

/**
* Init VCP Service
*/
void btp_vcp_init(void);

/**
 * Process VCP Operation
 */
void btp_vcp_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_VCP_H
