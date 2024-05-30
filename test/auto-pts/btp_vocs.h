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

#ifndef BTP_VOCS_H
#define BTP_VOCS_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_bool.h"

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

/* VOCS commands */

#define BTP_VOCS_READ_SUPPORTED_COMMANDS	0x01
struct btp_vocs_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_VOCS_UPDATE_LOC			0x02
struct btp_vocs_audio_loc_cmd {
    bt_addr_le_t address;
    uint32_t loc;
} __packed;

#define BTP_VOCS_UPDATE_DESC			0x03
struct btp_vocs_audio_desc_cmd {
    uint8_t desc_len;
    uint8_t desc[0];
} __packed;

#define BTP_VOCS_STATE_GET			0x04
struct btp_vocs_state_get_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VOCS_LOCATION_GET			0x05
struct btp_vocs_location_get_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_VOCS_OFFSET_STATE_SET		0x06
struct btp_vocs_offset_set_cmd {
    bt_addr_le_t address;
    int16_t offset;
} __packed;

#define BTP_VOCS_OFFSET_STATE_EV		0x80
struct btp_vocs_offset_state_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    int16_t offset;
} __packed;

#define BTP_VOCS_AUDIO_LOCATION_EV		0x81
struct btp_vocs_audio_location_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint32_t location;
} __packed;

#define BTP_VOCS_PROCEDURE_EV			0x82
struct btp_vocs_procedure_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t opcode;
} __packed;

#pragma options align=reset

/**
* Init VOCS Service
*/
void btp_vocs_init(void);

/**
 * Process VOCS Operation
 */
void btp_vocs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_VOCS_H
