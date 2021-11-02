/*
 * Copyright (C) 2017 BlueKitchen GmbH
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


#ifndef __PB_ADV_H
#define __PB_ADV_H

#include <stdint.h>

#include "btstack_defines.h"
#include "btstack_config.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Initialize Provisioning Bearer using Advertisement Bearer
 */
void pb_adv_init(void);

/**
 * Register provisioning device listener for Provisioning PDUs and MESH_PBV_ADV_SEND_COMPLETE
 */
void pb_adv_register_device_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * Register provisioning provisioner listener for Provisioning PDUs and MESH_PBV_ADV_SEND_COMPLETE
 */
void pb_adv_register_provisioner_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * Send Provisioning PDU
 * @param pb_adv_cid
 * @param pdu
 * @param pb_adv_cid
 */
void pb_adv_send_pdu(uint16_t pb_adv_cid, const uint8_t * pdu, uint16_t pdu_size);
 
/**
 * Close Link
 * @param pb_adv_cid
 * @param reason 0 = success, 1 = timeout, 2 = fail
 */
void pb_adv_close_link(uint16_t pb_adv_cid, uint8_t reason); 

#ifdef ENABLE_MESH_PROVISIONER
/**
 * Setup Link with unprovisioned device
 * @param DeviceUUID - data not copied
 * @return pb_adv_cid or 0
 */
uint16_t pb_adv_create_link(const uint8_t * device_uuid);
#endif

#if defined __cplusplus
}
#endif

#endif // __PB_ADV_H
