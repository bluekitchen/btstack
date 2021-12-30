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


#ifndef __PB_GATT_H
#define __PB_GATT_H

#include <stdint.h>

#include "btstack_defines.h"
#include "btstack_config.h"
#include "hci.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Setup mesh provisioning service
 */
void pb_gatt_init(void);

/**
 * Register listener for Provisioning PDUs and events: MESH_PB_TRANSPORT_LINK_OPEN, MESH_PB_TRANSPORT_LINK_CLOSED, MESH_SUBEVENT_CAN_SEND_NOW
 * @param packet_handler
 */
void pb_gatt_register_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * Send PDU
 * @param con_handle
 * @param pdu
 * @param pdu_size
 */
void pb_gatt_send_pdu(uint16_t con_handle, const uint8_t * pdu, uint16_t pdu_size);

/**
 * Setup Link with unprovisioned device
 * @param   device_uuid
 * @return  con_handle or HCI_CON_HANDLE_INVALID
 */
hci_con_handle_t pb_gatt_create_link(const uint8_t * device_uuid);

/**
 * Close Link
 * @param con_handle
 * @param reason 0 = success, 1 = timeout, 2 = fail
 */
void pb_gatt_close_link(hci_con_handle_t con_handle, uint8_t reason);


#if defined __cplusplus
}
#endif

#endif // __PB_GATT_H
