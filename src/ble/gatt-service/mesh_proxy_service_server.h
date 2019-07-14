/*
 * Copyright (C) 2018 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#ifndef __MESH_PROXY_SERVICE_SERVER_H
#define __MESH_PROXY_SERVICE_SERVER_H

#include <stdint.h>

#include "hci.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Implementation of the Mesh Proxy Service Server 
 */

/* API_START */

/**
 * @brief Init Mesh Proxy Service Server with ATT DB
 */
void mesh_proxy_service_server_init(void);

/**
 * @brief Send a Proxy PDU message containing proxy PDU from a proxy Server to a proxy Client.
 * @param con_handle 
 * @param proxy_pdu 
 * @param proxy_pdu_size max lenght MESH_PROV_MAX_PROXY_PDU
 */
void mesh_proxy_service_server_send_proxy_pdu(uint16_t con_handle, const uint8_t * proxy_pdu, uint16_t proxy_pdu_size);

/**
 * @brief Register callback for the PB-GATT.
 * @param callback
 */
void mesh_proxy_service_server_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Request can send now event to send PDU
 * Generates an MESH_SUBEVENT_CAN_SEND_NOW subevent
 * @param con_handle
 */
void mesh_proxy_service_server_request_can_send_now(hci_con_handle_t con_handle);
/* API_END */

#if defined __cplusplus
}
#endif

#endif

