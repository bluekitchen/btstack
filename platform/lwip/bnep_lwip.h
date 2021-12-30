/*
 * Copyright (C) 2019 BlueKitchen GmbH
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
 *  bnep_lwip.h
 *  BNEP adapter for lwIP TCP/IP Network Stack
 * 
 *  Adapter handles all BNEP events and forwards Ethernet frames between an active BNEP connection and the lwIP network stack
 */

#ifndef BNEP_LWIP_H
#define BNEP_LWIP_H

#include <stdint.h>
#include "bluetooth.h"
#include "btstack_defines.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BNEP Adapter
 */
void bnep_lwip_init(void);

/**
 * @brief Register packet handler for BNEP events
 */
void bnep_lwip_register_packet_handler(btstack_packet_handler_t handler);

/**
 * @brief Register BNEP service
 * @brief Same as benp_register_service, but bnep lwip adapter handles all events
 * @param service_uuid 
 * @Param max_frame_size
 */
uint8_t bnep_lwip_register_service(uint16_t service_uuid, uint16_t max_frame_size);

/**
 * @brief Creates BNEP connection (channel) to a given server on a remote device with baseband address. A new baseband connection will be initiated if necessary.
 * @param addr
 * @param l2cap_psm
 * @param uuid_src
 * @param uuid_dest
 */
uint8_t bnep_lwip_connect(bd_addr_t addr, uint16_t l2cap_psm, uint16_t uuid_src, uint16_t uuid_dest);

#if defined __cplusplus
}
#endif

#endif
