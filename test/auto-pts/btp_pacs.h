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

#ifndef BTP_PACS_H
#define BTP_PACS_H

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

/* PACSS commands */

#define BTP_PACS_READ_SUPPORTED_COMMANDS			0x01
struct btp_pacs_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_PACS_CHARACTERISTIC_SINK_PAC			0x01
#define BTP_PACS_CHARACTERISTIC_SOURCE_PAC			0x02
#define BTP_PACS_CHARACTERISTIC_SINK_AUDIO_LOCATIONS		0x03
#define BTP_PACS_CHARACTERISTIC_SOURCE_AUDIO_LOCATIONS		0x04
#define BTP_PACS_CHARACTERISTIC_AVAILABLE_AUDIO_CONTEXTS	0x05
#define BTP_PACS_CHARACTERISTIC_SUPPORTED_AUDIO_CONTEXTS	0x06

#define BTP_PACS_UPDATE_CHARACTERISTIC				0x02
struct btp_pacs_update_characteristic_cmd {
    uint8_t characteristic;
} __packed;

#define BTP_PACS_SET_LOCATION					0x03
struct btp_pacs_set_location_cmd {
    uint8_t dir;
    uint32_t location;
} __packed;

#define BTP_PACS_SET_AVAILABLE_CONTEXTS				0x04
struct btp_pacs_set_available_contexts_cmd {
    uint16_t sink_contexts;
    uint16_t source_contexts;
} __packed;

#define BTP_PACS_SET_SUPPORTED_CONTEXTS				0x05
struct btp_pacs_set_supported_contexts_cmd {
    uint16_t sink_contexts;
    uint16_t source_contexts;
} __packed;

#pragma options align=reset

/**
* Init PACS Service
*/
void btp_pacs_init(void);

/**
 * Process PACS Operation
 */
void btp_pacs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_PACS_H
