/*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */
 
#ifndef __CENTRAL_DEVICE_DB_H
#define __CENTRAL_DEVICE_DB_H

#include <btstack/utils.h>

#if defined __cplusplus
extern "C" {
#endif

/** 

	A Central Device DB is only required for signed writes
	
	Per bonded device, it stores the Identity Resolving Key (IRK), the Connection Signature Resolving Key (CSRK)
	   and the last used counter

	The IRK is necessary to identify a device that uses private addresses
	The CSRK is used to generate the signatur on the remote device and is needed to verify the signature itself
	The Counter is necessary to prevent reply attacks

*/


// Central Device db interface


/**
 * @brief init
 */
void central_device_db_init();

/**
 * @brief add device to db
 * @param addr_type, address of the device
 * @param irk and csrk of the device
 * @returns index if successful, -1 otherwise
 */
int central_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk, sm_key_t csrk);

/**
 * @brief get number of devices in db for enumeration
 * @returns number of device in db
 */
int central_device_db_count(void);

/**
 * @brief get device information: addr type and address needed to identify device
 * @param index
 * @param addr_type, address of the device as output
 * @param irk of the device
 */
void central_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t irk);

/**
 * @brief get signing key for this device
 * @param index
 * @param signing key as output
 */
void central_device_db_csrk(int index, sm_key_t csrk);

/**
 * @brief query last used/seen signing counter
 * @param index
 * @returns next expected counter, 0 after devices was added
 */
uint32_t central_device_db_counter_get(int index);

/**
 * @brief update signing counter
 * @param index
 * @param counter to store
 */
void central_device_db_counter_set(int index, uint32_t counter);

/**
 * @brief free device
 * @param index
 */
void central_device_db_remove(int index);

#endif // __CENTRAL_DEVICE_DB_H
