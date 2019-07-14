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


#ifndef __BEACON_H
#define __BEACON_H

#include <stdint.h>

#include "btstack_defines.h"

#include "mesh/mesh_keys.h"
#include "mesh/mesh_network.h"

#if defined __cplusplus
extern "C" {
#endif


/**
 * Initialize Mesh Beacon
 */
void beacon_init(void);

/**
 * Start Unprovisioned Device Beacon
 */
void beacon_unprovisioned_device_start(const uint8_t * device_uuid, uint16_t oob_information);

/**
 * Stop Unprovisioned Device Beacon
 */
void beacon_unprovisioned_device_stop(void);

/**
 * Start sending Secure Network Beacons on given subnet
 * @param mesh_subnet
 */
void beacon_secure_network_start(mesh_subnet_t * mesh_subnet);

/**
 * Register Beacon handler for unprovisioned device beacons
 */
void beacon_register_for_unprovisioned_device_beacons(btstack_packet_handler_t packet_handler);

/**
 * Register Beacon handler for secure network beacons
 */
void beacon_register_for_secure_network_beacons(btstack_packet_handler_t packet_handler);

#if defined __cplusplus
}
#endif

#endif // __BEACON_H
