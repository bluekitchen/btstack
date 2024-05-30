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

#ifndef BTP_MICS_H
#define BTP_MICS_H

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

/* MICS commands */
#define BTP_MICS_READ_SUPPORTED_COMMANDS	0x01
struct btp_mics_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_MICS_DEV_MUTE_DISABLE		0x02
#define BTP_MICS_DEV_MUTE_READ			0x03
#define BTP_MICS_DEV_MUTE			0x04
#define BTP_MICS_DEV_UNMUTE			0x05

#define BTP_MICS_MUTE_STATE_EV			0x80
struct btp_mics_mute_state_ev {
    uint8_t mute;
} __packed;

#pragma options align=reset

/**
* Init MICS Service
*/
void btp_mics_init(void);

/**
 * Process MICS Operation
 */
void btp_mics_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_MICS_H
