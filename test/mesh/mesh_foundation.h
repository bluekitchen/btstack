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

#ifndef __MESH_FOUNDATION_H
#define __MESH_FOUNDATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif


#include <stdint.h>
#include "ble/mesh/mesh_network.h"

#define MESH_TTL_MAX 0x7f
#define MESH_FOUNDATION_STATE_NOT_SUPPORTED 2

/**
 *
 * @param ttl
 */
void mesh_foundation_gatt_proxy_set(uint8_t ttl);

/**
 *
 * @return
 */
uint8_t mesh_foundation_gatt_proxy_get(void);

/**
 *
 * @param ttl
 */
void mesh_foundation_beacon_set(uint8_t ttl);

/**
 *
 * @return
 */
uint8_t mesh_foundation_becaon_get(void);

/**
 *
 * @param ttl
 */
void mesh_foundation_default_ttl_set(uint8_t ttl);

/**
 *
 * @return
 */
uint8_t mesh_foundation_default_ttl_get(void);
/**
 *
 * @param ttl
 */
void mesh_foundation_friend_set(uint8_t ttl);

/**
 *
 * @return
 */
uint8_t mesh_foundation_friend_get(void);

/**
 *
 * @param network_transmit
 */
void mesh_foundation_network_transmit_set(uint8_t network_transmit);

/**
 *
 * @return
 */
uint8_t mesh_foundation_network_transmit_get(void);
/**
 *
 * @param relay
 */
void mesh_foundation_relay_set(uint8_t relay);

/**
 *
 * @return
 */
uint8_t mesh_foundation_relay_get(void);

/**
 *
 * @param relay_retransmit
 */
void mesh_foundation_relay_retransmit_set(uint8_t relay_retransmit);

/**
 *
 * @return
 */
uint8_t mesh_foundation_relay_retransmit_get(void);

/**
 *
 */
void mesh_foundation_node_reset(void);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
