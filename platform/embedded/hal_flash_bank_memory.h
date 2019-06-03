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
 *  hal_flash_bank.h
 * 
 *  HAL abstraction for Flash memory that can be written anywhere
 *  after being erased implemented with memory
 */

#ifndef HAL_FLASH_BANK_MEMORY_H
#define HAL_FLASH_BANK_MEMORY_H

#include <stdint.h>
#include "hal_flash_bank.h"

#if defined __cplusplus
extern "C" {
#endif

// private
typedef struct {
	uint32_t   bank_size;
	uint8_t  * banks[2];
} hal_flash_bank_memory_t;

// public

/** 
 * Init instance
 * @param context hal_flash_bank_memory_t
 * @param storage to use
 * @param size of storage
 */
const hal_flash_bank_t * hal_flash_bank_memory_init_instance(hal_flash_bank_memory_t * context, uint8_t * storage, uint32_t storage_size);

#if defined __cplusplus
}
#endif
#endif // HAL_FLASH_BANK_MEMORY_H
