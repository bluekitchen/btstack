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

#ifndef BTP_ASCS_H
#define BTP_ASCS_H

#include <stdint.h>
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

#pragma pack(1)
#define __packed

typedef struct {
    uint8_t address_type;
    bd_addr_t address;
} bt_addr_le_t;

/* ASCS commands */
#define BTP_ASCS_READ_SUPPORTED_COMMANDS	0x01
struct btp_ascs_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_ASCS_CONFIGURE_CODEC	0x02
struct btp_ascs_configure_codec_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
    uint8_t coding_format;
    uint16_t vid;
    uint16_t cid;
    uint8_t ltvs_len;
    uint8_t ltvs[0];
} __packed;

#define BTP_ASCS_CONFIGURE_QOS	0x03
struct btp_ascs_configure_qos_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
    uint8_t cig_id;
    uint8_t cis_id;
    uint8_t sdu_interval[3];
    uint8_t framing;
    uint16_t max_sdu;
    uint8_t retransmission_num;
    uint16_t max_transport_latency;
    uint8_t presentation_delay[3];
} __packed;

#define BTP_ASCS_ENABLE	0x04
struct btp_ascs_enable_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
} __packed;

#define BTP_ASCS_RECEIVER_START_READY	0x05
struct btp_ascs_receiver_start_ready_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
} __packed;

#define BTP_ASCS_RECEIVER_STOP_READY	0x06
struct btp_ascs_receiver_stop_ready_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
} __packed;

#define BTP_ASCS_DISABLE	0x07
struct btp_ascs_disable_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
} __packed;

#define BTP_ASCS_RELEASE	0x08
struct btp_ascs_release_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
} __packed;

#define BTP_ASCS_UPDATE_METADATA	0x09
struct btp_ascs_update_metadata_cmd {
    bt_addr_le_t address;
    uint8_t ase_id;
} __packed;

/* ASCS events */
#define BTP_ASCS_EV_OPERATION_COMPLETED	0x80
struct btp_ascs_operation_completed_ev {
    bt_addr_le_t address;
    uint8_t ase_id;
    uint8_t opcode;
    uint8_t status;

    /* RFU */
    uint8_t flags;
} __packed;

#define BTP_ASCS_STATUS_SUCCESS	0x00
#define BTP_ASCS_STATUS_FAILED	0x01

#pragma options align=reset

/**
* Init ASCS Service
*/
void btp_ascs_init(void);

/**
 * Process ASCS Operation
 */
void btp_ascs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif
#endif // BTP_ASCS_H
