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

#ifndef BTP_MCS_H
#define BTP_MCS_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_bool.h"
#include "le-audio/gatt-service/object_transfer_service_util.h"

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

#define BT_OTS_OBJ_ID_SIZE OTS_OBJECT_ID_LEN

#pragma pack(1)
#define __packed

/* MCS commands */
#define BTP_MCS_READ_SUPPORTED_COMMANDS		0x01
struct btp_mcs_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_MCS_CMD_SEND			0x02
struct btp_mcs_send_cmd {
    uint8_t opcode;
    uint8_t use_param;
    int32_t param;
} __packed;

#define BTP_MCS_CURRENT_TRACK_OBJ_ID_GET	0x03
struct btp_mcs_current_track_obj_id_rp {
    uint8_t id[BT_OTS_OBJ_ID_SIZE];
} __packed;

#define BTP_MCS_NEXT_TRACK_OBJ_ID_GET		0x04
struct btp_mcs_next_track_obj_id_rp {
    uint8_t id[BT_OTS_OBJ_ID_SIZE];
} __packed;

#define BTP_MCS_INACTIVE_STATE_SET		0x05
struct btp_mcs_state_set_rp {
    uint8_t state;
} __packed;

#define BTP_MCS_PARENT_GROUP_SET		0x06


#pragma options align=reset

/**
* Init MCS Service
*/
void btp_mcs_init(void);

/**
 * Process MCS Operation
 */
void btp_mcs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_MCS_H
