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

#ifndef BTP_AICS_H
#define BTP_AICS_H

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

/* AICS commands */

#define BTP_AICS_READ_SUPPORTED_COMMANDS	0x01
struct btp_aics_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

/* AICS client/server commands */
#define BTP_AICS_SET_GAIN			0x02
struct btp_aics_set_gain_cmd {
    bt_addr_le_t address;
    int8_t gain;
} __packed;

#define BTP_AICS_MUTE				0x03
struct btp_aics_mute_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_UNMUTE				0x04
struct btp_aics_unmute_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_MAN_GAIN_SET			0x05
struct btp_aics_manual_gain_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_AUTO_GAIN_SET			0x06
struct btp_aics_auto_gain_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_SET_MAN_GAIN_ONLY		0x07
#define BTP_AICS_SET_AUTO_GAIN_ONLY		0x08
#define BTP_AICS_AUDIO_DESCRIPTION_SET		0x09
struct btp_aics_audio_desc_cmd {
    uint8_t desc_len;
    uint8_t desc[0];
} __packed;

#define BTP_AICS_MUTE_DISABLE			0x0a
#define BTP_AICS_GAIN_SETTING_PROP_GET		0x0b
struct btp_aics_gain_setting_prop_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_TYPE_GET			0x0c
struct btp_aics_type_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_STATUS_GET			0x0d
struct btp_aics_status_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_STATE_GET			0x0e
struct btp_aics_state_cmd {
    bt_addr_le_t address;
} __packed;

#define BTP_AICS_DESCRIPTION_GET		0x0f
struct btp_aics_desc_cmd {
    bt_addr_le_t address;
} __packed;

/* AICS events */
#define BTP_AICS_STATE_EV			0x80
struct btp_aics_state_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    int8_t gain;
    uint8_t mute;
    uint8_t mode;
} __packed;

#define BTP_GAIN_SETTING_PROPERTIES_EV		0x81
struct btp_gain_setting_properties_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t units;
    int8_t minimum;
    int8_t maximum;
} __packed;

#define BTP_AICS_INPUT_TYPE_EV			0x82
struct btp_aics_input_type_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t input_type;
} __packed;

#define BTP_AICS_STATUS_EV			0x83
struct btp_aics_status_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    bool active;
} __packed;

#define BTP_AICS_DESCRIPTION_EV			0x84
struct btp_aics_description_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t data_len;
    char data[0];
} __packed;

#define BTP_AICS_PROCEDURE_EV			0x85
struct btp_aics_procedure_ev {
    bt_addr_le_t address;
    uint8_t att_status;
    uint8_t opcode;
} __packed;

#pragma options align=reset

/**
* Init AICS Service
*/
void btp_aics_init(void);

/**
 * Process PACS Operation
 */
void btp_aics_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_AICS_H
