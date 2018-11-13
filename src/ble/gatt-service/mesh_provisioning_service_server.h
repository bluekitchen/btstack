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
#ifndef __MESH_PROVISIONING_SERVICE_SERVER_H
#define __MESH_PROVISIONING_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * Implementation of the Mesh Provisioning Service Server 
 */

/* API_START */

typedef enum {
	MESH_OOB_INFORMATION_INDEX_OTHER = 0,
	MESH_OOB_INFORMATION_INDEX_ELECTRONIC_OR_URI,
	MESH_OOB_INFORMATION_INDEX_2D_MACHINE_READABLE_CODE,
	MESH_OOB_INFORMATION_INDEX_BAR_CODE,
	MESH_OOB_INFORMATION_INDEX_NEAR_FIELD_COMMUNICATION,
	MESH_OOB_INFORMATION_INDEX_NUMBER,
	MESH_OOB_INFORMATION_INDEX_STRING,
	MESH_OOB_INFORMATION_INDEX_RESERVED_7,
	MESH_OOB_INFORMATION_INDEX_RESERVED_8,
	MESH_OOB_INFORMATION_INDEX_RESERVED_9,
	MESH_OOB_INFORMATION_INDEX_RESERVED_10,
	MESH_OOB_INFORMATION_INDEX_ON_BOX,
	MESH_OOB_INFORMATION_INDEX_INSIDE_BOX,
	MESH_OOB_INFORMATION_INDEX_ON_PIECE_OF_PAPER,
	MESH_OOB_INFORMATION_INDEX_INSIDE_MANUAL,
	MESH_OOB_INFORMATION_INDEX_ON_DEVICE
} mesh_oob_information_index_t;

/**
 * @brief Init Mesh Provisioning Service Server with ATT DB
 */
void mesh_provisioning_service_server_init(void);

/**
 * @brief Send a Proxy PDU message containing Provisioning PDU from a Provisioning Server to a Provisioning Client.
 * @param con_handle 
 * @param proxy_pdu 
 * @param proxy_pdu_size max lenght MESH_PROV_MAX_PROXY_PDU
 */
void mesh_provisioning_service_server_send_proxy_pdu(uint16_t con_handle, const uint8_t * proxy_pdu, uint16_t proxy_pdu_size);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

