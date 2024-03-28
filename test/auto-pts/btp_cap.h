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
 *  btp_cap.h
 *  BTP CAP Implementation
 */

#ifndef BTP_CAP_H
#define BTP_CAP_H

#include "btstack_run_loop.h"
#include "btstack_defines.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

#pragma pack(1)

/* CAP commands */
#define BTP_CAP_READ_SUPPORTED_COMMANDS		0x01

#define BTP_CAP_DISCOVER			0x02
struct btp_cap_discover_cmd {
    uint8_t addr_type;
    bd_addr_t address;
};

#define BTP_CAP_UNICAST_SETUP_ASE		0x03
struct btp_cap_unicast_setup_ase_cmd {
    uint8_t addr_type;
    bd_addr_t address;
    uint8_t ase_id;
    uint8_t cis_id;
    uint8_t cig_id;
    uint8_t coding_format;
    uint16_t vid;
    uint16_t cid;
    uint8_t sdu_interval[3];
    uint8_t framing;
    uint16_t max_sdu;
    uint8_t retransmission_num;
    uint16_t max_transport_latency;
    uint8_t presentation_delay[3];
    uint8_t cc_ltvs_len;
    uint8_t metadata_ltvs_len;
    uint8_t ltvs[0];
};

#define BTP_CAP_UNICAST_AUDIO_START		0x04
struct btp_cap_unicast_audio_start_cmd {
    uint8_t cig_id;
    uint8_t set_type;
};

#define BTP_CAP_UNICAST_AUDIO_START_SET_TYPE_AD_HOC	0x00
#define BTP_CAP_UNICAST_AUDIO_START_SET_TYPE_CSIP	0x01

#define BTP_CAP_UNICAST_AUDIO_UPDATE		0x05
struct btp_cap_unicast_audio_update_cmd {
    uint8_t stream_count;
    uint8_t update_data[0];
};
struct btp_cap_unicast_audio_update_data {
    uint8_t addr_type;
    bd_addr_t address;
    uint8_t ase_id;
    uint8_t metadata_ltvs_len;
    uint8_t metadata_ltvs[0];
};

#define BTP_CAP_UNICAST_AUDIO_STOP		0x06
struct btp_cap_unicast_audio_stop_cmd {
    uint8_t cig_id;
};

#define BTP_CAP_BROADCAST_SOURCE_SETUP_STREAM	0x07
struct btp_cap_broadcast_source_setup_stream_cmd {
    uint8_t source_id;
    uint8_t subgroup_id;
    uint8_t coding_format;
    uint16_t vid;
    uint16_t cid;
    uint8_t cc_ltvs_len;
    uint8_t metadata_ltvs_len;
    uint8_t ltvs[0];
};

#define BTP_CAP_BROADCAST_SOURCE_SETUP_SUBGROUP	0x08
struct btp_cap_broadcast_source_setup_subgroup_cmd {
    uint8_t source_id;
    uint8_t subgroup_id;
    uint8_t coding_format;
    uint16_t vid;
    uint16_t cid;
    uint8_t cc_ltvs_len;
    uint8_t metadata_ltvs_len;
    uint8_t ltvs[0];
};

#define BTP_CAP_BROADCAST_SOURCE_SETUP		0x09
struct btp_cap_broadcast_source_setup_cmd {
    uint8_t source_id;
    uint8_t broadcast_id[3];
    uint8_t sdu_interval[3];
    uint8_t framing;
    uint16_t max_sdu;
    uint8_t retransmission_num;
    uint16_t max_transport_latency;
    uint8_t presentation_delay[3];
    uint8_t flags;
    uint8_t broadcast_code[16];
};
struct btp_cap_broadcast_source_setup_rp {
    uint8_t source_id;
    uint32_t gap_settings;
    uint8_t broadcast_id[16];
};
#define BTP_CAP_BROADCAST_SOURCE_SETUP_FLAG_ENCRYPTION		0x01
#define BTP_CAP_BROADCAST_SOURCE_SETUP_FLAG_SUBGROUP_CODEC	0x02

#define BTP_CAP_BROADCAST_SOURCE_RELEASE	0x0a
struct btp_cap_broadcast_source_release_cmd {
    uint8_t source_id;
};

#define BTP_CAP_BROADCAST_ADV_START		0x0b
struct btp_cap_broadcast_adv_start_cmd {
    uint8_t source_id;
};

#define BTP_CAP_BROADCAST_ADV_STOP		0x0c
struct btp_cap_broadcast_adv_stop_cmd {
    uint8_t source_id;
};

#define BTP_CAP_BROADCAST_SOURCE_START		0x0d
struct btp_cap_broadcast_source_start_cmd {
    uint8_t source_id;
};

#define BTP_CAP_BROADCAST_SOURCE_STOP		0x0e
struct btp_cap_broadcast_source_stop_cmd {
    uint8_t source_id;
};

#define BTP_CAP_BROADCAST_SOURCE_UPDATE		0x0f
struct btp_cap_broadcast_source_update_cmd {
    uint8_t source_id;
    uint8_t metadata_ltvs_len;
    uint8_t metadata_ltvs[0];
};

/* CAP events */
#define BTP_CAP_EV_DISCOVERY_COMPLETED		0x80
struct btp_cap_discovery_completed_ev {
    uint8_t addr_type;
    bd_addr_t address;
    uint8_t status;
};
#define BTP_CAP_DISCOVERY_STATUS_SUCCESS	0x00
#define BTP_CAP_DISCOVERY_STATUS_FAILED		0x01

#define BTP_CAP_EV_UNICAST_START_COMPLETED	0x81
struct btp_cap_unicast_start_completed_ev {
    uint8_t cig_id;
    uint8_t status;
};
#define BTP_CAP_UNICAST_START_STATUS_SUCCESS	0x00
#define BTP_CAP_UNICAST_START_STATUS_FAILED	0x01

#define BTP_CAP_EV_UNICAST_STOP_COMPLETED	0x82
struct btp_cap_unicast_stop_completed_ev {
    uint8_t cig_id;
    uint8_t status;
};
#define BTP_CAP_UNICAST_STOP_STATUS_SUCCESS	0x00
#define BTP_CAP_UNICAST_STOP_STATUS_FAILED	0x01

#pragma options align=reset

/**
 * Init CAP Service
 */
void btp_cap_init(void);

/**
 * Process CAP Operation
 */
void btp_cap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

#if defined __cplusplus
}
#endif

#endif // BTP_CAP_H
