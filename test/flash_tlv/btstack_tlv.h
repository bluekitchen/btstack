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
 */

/*
 *  btstack_tlv_.h
 *
 *  Inteface for BTstack's Tag Value Length Persistent Storage implementations
 *  used to store pairing/bonding data
 */

#ifndef __BTSTACK_TLV_H
#define __BTSTACK_TLV_H

#include <stdint.h>
#include "hal_flash_sector.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * Init Tag Length Value Store
 */
void btstack_tlv_init(const hal_flash_sector_t * hal_flash_sector);

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @returns size of value
 */
int btstack_tlv_get_tag(uint32_t tag, uint8_t * buffer, uint32_t buffer_size);

/**
 * Store Tag 
 * @param tag
 * @param data
 * @param data_size
 */
void btstack_tlv_store_tag(uint32_t tag, const uint8_t * data, uint32_t data_size);

/**
 * Delete Tag
 * @param tag
 */
void btstack_tlv_delete_tag(uint32_t tag);

#if defined __cplusplus
}
#endif
#endif // __BTSTACK_TLV_H
