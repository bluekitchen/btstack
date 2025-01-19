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
 */

/*
 *  hal_flash_sector_maxim.h
 * 
 *  HAL abstraction for Flash memory that can be written anywhere
 *  after being erased implemented with memory
 */

#ifndef __HAL_FLASH_BANK_MAXIM_H
#define __HAL_FLASH_BANK_MAXIM_H

#include <stdint.h>
#include "hal_flash_bank.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t   sector_size;
	uintptr_t  banks[2];

} hal_flash_bank_mxc_t;

/**
 * Configure MXC HAL Flash Implementation
 *
 * @param context of hal_flash_bank_mxc_t
 * @param sector_size
 * @param bank_0_addr
 * @param bank_1_addr
 * @return 
 */
const hal_flash_bank_t * hal_flash_bank_mxc_init_instance(hal_flash_bank_mxc_t * context, uint32_t sector_size, uintptr_t bank_0_addr, uintptr_t bank_1_addr);

#if defined __cplusplus
}
#endif
#endif
