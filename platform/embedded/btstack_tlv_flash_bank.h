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
 *  btstack_tlv_flash_bank.h
 *
 *  Implementation for BTstack's Tag Value Length Persistent Storage implementations
 *  using hal_flash_bank storage
 */

#ifndef BTSTACK_TLV_FLASH_BANK_H
#define BTSTACK_TLV_FLASH_BANK_H

#include <stdint.h>
#include "btstack_tlv.h"
#include "hal_flash_bank.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
	const hal_flash_bank_t * hal_flash_bank_impl;
	void * hal_flash_bank_context;
	int current_bank;
	int write_offset;
	int delete_tag_len;
} btstack_tlv_flash_bank_t;

/**
 * Init Tag Length Value Store
 * @param context btstack_tlv_flash_bank_t 
 * @param hal_flash_bank_impl    of hal_flash_bank interface
 * @Param hal_flash_bank_context of hal_flash_bank_interface
 */
const btstack_tlv_t * btstack_tlv_flash_bank_init_instance(btstack_tlv_flash_bank_t * context, const hal_flash_bank_t * hal_flash_bank_impl, void * hal_flash_bank_context);

#if defined __cplusplus
}
#endif
#endif // BTSTACK_TLV_FLASH_BANK_H
