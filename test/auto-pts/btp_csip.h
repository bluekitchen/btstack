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
 *  btp_csip.h
 *  BTP CSI Implementation
 */

#ifndef BTP_CSIP_H
#define BTP_CSIP_H

#include "btstack_run_loop.h"
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btp_server.h"


#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// pack structs
#pragma pack(1)
#define __packed

/* CSIP commands */
#define BTP_CSIP_READ_SUPPORTED_COMMANDS	0x01
struct btp_csip_read_supported_commands_rp {
    uint8_t data[0];
} __packed;

#define BTP_CSIP_DISCOVER			0x02
struct btp_csip_discover_cmd {
    uint8_t addr_type;
    bd_addr_t address;
} __packed;

#define BTP_CSIP_START_ORDERED_ACCESS		0x03
struct btp_csip_start_ordered_access_cmd {
    uint8_t flags;
} __packed;

#define BTP_CSIP_SET_COORDINATOR_LOCK		0x04
struct btp_csip_set_coordinator_lock_cmd {
    uint8_t count;
} __packed;

#define BTP_CSIP_SET_COORDINATOR_RELEASE	0x05
struct btp_csip_set_coordinator_release_cmd {
    uint8_t count;
} __packed;

/* CSIP Events */
#define BTP_CSIP_DISCOVERED_EV			0x80
struct btp_csip_discovered_ev {
    uint8_t addr_type;
    bd_addr_t address;
    uint8_t status;
    uint16_t sirk_handle;
    uint16_t size_handle;
    uint16_t lock_handle;
    uint16_t rank_handle;
} __packed;

#define BTP_CSIP_SIRK_EV			0x81
struct btp_csip_sirk_ev {
    uint8_t addr_type;
    bd_addr_t address;
    uint8_t sirk[16];
} __packed;

#define BTP_CSIP_LOCK_EV			0x82
struct btp_csip_lock_ev {
    uint8_t status;
} __packed;

#pragma options align=reset

/**
 * Init CSIP Service
 */
void btp_csip_init(void);

/**
 * Process CSIP Operation
 */
void btp_csip_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data);

/**
 * Allow to process CSIP events by higher layer, e.g. BAP or CAP
 */
void btp_csip_register_higher_layer(btstack_packet_handler_t packet_handler);

/**
 * Connect to remote device
 */
void btp_csip_connect(hci_con_handle_t );

/**
 * Connect to remote device
 */
void btp_csip_connect_to_server(server_t * server);

#if defined __cplusplus
}
#endif

#endif // BTP_CSIP_H
